// Copyright © 2026 CCP ehf.

#include "cmf/animationnetwork.h"

#include "animationnetworkcompiled.h"
#include "animationnetworkformat.h"

#include <algorithm>
#include <numeric>

namespace cmf
{
namespace
{

std::string GetNodeLabel( uint32_t graphIndex, uint32_t nodeIndex, const can::Node& node )
{
	std::string label = "graph " + std::to_string( graphIndex ) + ": " + detail::GetNodeTypeName( node.type ) + " #" + std::to_string( nodeIndex );
	if( !node.name.empty() )
	{
		label += " '" + ToStdString( node.name ) + "'";
	}
	return label + ": ";
}

bool IsInputConnected( const can::Graph& graph, uint32_t node, uint32_t socket )
{
	return std::any_of( graph.edges.begin(), graph.edges.end(), [node, socket]( const can::Edge& edge ) { return edge.to.node == node && edge.to.socket == socket; } );
}

void ValidateRandomSwitch( const can::RandomSwitchNode& node, const std::string& prefix, std::vector<std::string>& errors )
{
	if( node.weights.empty() )
	{
		return;
	}
	if( node.weights.size() != node.poseInputCount )
	{
		errors.push_back( prefix + "number of random weights doesn't match the number of pose inputs" );
	}
	else if( std::accumulate( node.weights.begin(), node.weights.end(), 0.f ) <= 0.f )
	{
		errors.push_back( prefix + "random weights must not all be zero" );
	}
}

void ValidateBoneMask( const can::NamedBoneWeightsNode& node, const Skeleton* skeleton, const std::string& prefix, std::vector<std::string>& warnings )
{
	if( node.maskName.empty() )
	{
		warnings.push_back( prefix + "no bone mask is set" );
	}
	else if( skeleton && !detail::FindBoneMask( *skeleton, ToStdStringView( node.maskName ) ) )
	{
		warnings.push_back( prefix + "bone mask '" + ToStdString( node.maskName ) + "' not found in the skeleton" );
	}
}

void ValidateBoneWeights( const can::BoneWeightsNode& node, const Skeleton* skeleton, const std::string& prefix, std::vector<std::string>& warnings )
{
	for( const auto& weight : node.weights )
	{
		if( skeleton && detail::FindBone( *skeleton, ToStdStringView( weight.bone ) ) == can::NO_INDEX )
		{
			warnings.push_back( prefix + "bone '" + ToStdString( weight.bone ) + "' not found in the skeleton" );
		}
	}
}

}

AnimationNetwork::AnimationNetwork( AnimationAssetLoader& loader ) :
	m_loader( &loader ),
	m_aliveToken( std::make_shared<bool>( true ) )
{
}

AnimationNetwork::~AnimationNetwork()
{
	Unload();
}

ValidationResult AnimationNetwork::Load( std::vector<uint8_t> file )
{
	Unload();
	auto result = detail::ValidateCanFile( file.data(), file.size() );
	if( !result )
	{
		return result;
	}
	m_file = std::move( file );
	m_data = &detail::GetNetworkData( m_file.data() );
	m_assets.assign( m_data->assets.size(), {} );

	const auto loadId = ++m_loadId;
	for( uint32_t i = 0; i < m_data->assets.size(); ++i )
	{
		const auto& asset = m_data->assets[i];
		if( asset.section != can::NO_INDEX )
		{
			m_assets[i] = { &detail::GetEmbeddedCmfData( m_file.data(), asset.section ), AssetState::Loaded };
			continue;
		}
		std::weak_ptr<bool> alive = m_aliveToken;
		// The callback may run before RequestLoad returns
		m_loader->RequestLoad( ToStdStringView( asset.path ), [this, alive = std::move( alive ), loadId, i]( const cmf::Data* data ) {
			if( !alive.expired() )
			{
				OnAssetLoaded( loadId, i, data );
			}
		} );
	}
	CompileIfReady();
	return result;
}

void AnimationNetwork::Unload()
{
	if( !m_data )
	{
		return;
	}
	// Callbacks of requests made for the current file are ignored from now on
	++m_loadId;
	for( const auto& asset : m_data->assets )
	{
		if( asset.section == can::NO_INDEX )
		{
			m_loader->Release( ToStdStringView( asset.path ) );
		}
	}
	m_data = nullptr;
	m_compiled.reset();
	m_compileErrors.clear();
	m_assets.clear();
	m_file.clear();
}

void AnimationNetwork::OnAssetLoaded( uint32_t loadId, uint32_t assetIndex, const cmf::Data* data )
{
	if( loadId != m_loadId )
	{
		return;
	}
	m_assets[assetIndex] = { data, data ? AssetState::Loaded : AssetState::Failed };
	CompileIfReady();
}

void AnimationNetwork::CompileIfReady()
{
	// Callbacks may arrive while Load is still requesting assets, before all entries are set up
	if( !m_data || m_compiled || HasPendingAssets() || !m_compileErrors.empty() || !GetErrors().empty() )
	{
		return;
	}
	m_compiled = detail::CompileNetwork( *this, m_compileErrors );
}

const can::NetworkData* AnimationNetwork::GetData() const
{
	return m_data;
}

const cmf::Data* AnimationNetwork::GetAssetData( uint32_t assetIndex ) const
{
	if( assetIndex >= m_assets.size() || m_assets[assetIndex].state != AssetState::Loaded )
	{
		return nullptr;
	}
	return m_assets[assetIndex].data;
}

const Skeleton* AnimationNetwork::GetSkeleton() const
{
	if( !m_data )
	{
		return nullptr;
	}
	const auto* data = GetAssetData( m_data->skeletonAsset );
	return data ? detail::FindSkeleton( *data, ToStdStringView( m_data->skeletonName ) ) : nullptr;
}

bool AnimationNetwork::HasPendingAssets() const
{
	return std::any_of( m_assets.begin(), m_assets.end(), []( const AssetEntry& asset ) { return asset.state == AssetState::Pending; } );
}

std::vector<std::string> AnimationNetwork::GetErrors() const
{
	std::vector<std::string> errors;
	CollectProblems( &errors, nullptr );
	return errors;
}

std::vector<std::string> AnimationNetwork::GetWarnings() const
{
	std::vector<std::string> warnings;
	CollectProblems( nullptr, &warnings );
	return warnings;
}

void AnimationNetwork::CollectProblems( std::vector<std::string>* errorsOut, std::vector<std::string>* warningsOut ) const
{
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
	if( !m_data )
	{
		if( errorsOut != nullptr )
		{
			*errorsOut = { "No network is loaded" };
		}
		return;
	}
	const auto& data = *m_data;
	if( data.skeletonAsset == can::NO_INDEX )
	{
		errors.emplace_back( "No skeleton is set" );
	}
	else if( m_assets[data.skeletonAsset].state == AssetState::Failed )
	{
		errors.push_back( "Failed to load skeleton asset '" + ToStdString( data.assets[data.skeletonAsset].path ) + "'" );
	}
	else if( m_assets[data.skeletonAsset].state == AssetState::Loaded && !GetSkeleton() )
	{
		errors.push_back( "Skeleton '" + ToStdString( data.skeletonName ) + "' not found in '" + ToStdString( data.assets[data.skeletonAsset].path ) + "'" );
	}
	if( data.graphs[0].interfaceOutputs.empty() )
	{
		errors.emplace_back( "The root graph has no Output node" );
	}

	const auto* skeleton = GetSkeleton();
	for( uint32_t g = 0; g < data.graphs.size(); ++g )
	{
		const auto& graph = data.graphs[g];
		for( uint32_t n = 0; n < graph.nodes.size(); ++n )
		{
			const auto& node = graph.nodes[n];
			const auto prefix = GetNodeLabel( g, n, node );
			for( uint32_t socket = 0; socket < detail::GetInputCount( data, graph, node ); ++socket )
			{
				if( detail::GetInputType( data, graph, node, socket ) == can::SocketType::Pose && !IsInputConnected( graph, n, socket ) )
				{
					errors.push_back( prefix + "input '" + detail::GetInputName( data, graph, node, socket ) + "' is not connected" );
				}
			}
			ValidateNode( graph, node, skeleton, prefix, errors, warnings );
		}
	}
	errors.insert( errors.end(), m_compileErrors.begin(), m_compileErrors.end() );
	if( errorsOut != nullptr )
	{
		*errorsOut = std::move( errors );
	}
	if( warningsOut != nullptr )
	{
		*warningsOut = std::move( warnings );
	}
}

void AnimationNetwork::ValidateNode( const can::Graph& graph, const can::Node& node, const Skeleton* skeleton, const std::string& prefix, std::vector<std::string>& errors, std::vector<std::string>& warnings ) const
{
	// Every node type needs a case; there is deliberately no default case
	switch( node.type )
	{
	case can::NodeType::AnimationSample:
		ValidateAnimationSample( detail::GetNodeData<can::AnimationSampleNode>( graph, node ), prefix, warnings );
		break;
	case can::NodeType::NamedBoneWeights:
		ValidateBoneMask( detail::GetNodeData<can::NamedBoneWeightsNode>( graph, node ), skeleton, prefix, warnings );
		break;
	case can::NodeType::BoneWeights:
		ValidateBoneWeights( detail::GetNodeData<can::BoneWeightsNode>( graph, node ), skeleton, prefix, warnings );
		break;
	case can::NodeType::Expression:
		if( detail::GetNodeData<can::ExpressionNode>( graph, node ).expression.empty() )
		{
			errors.push_back( prefix + "no expression is set" );
		}
		break;
	case can::NodeType::RandomSwitch:
		ValidateRandomSwitch( detail::GetNodeData<can::RandomSwitchNode>( graph, node ), prefix, errors );
		break;
	case can::NodeType::Output:
	case can::NodeType::SubgraphInput:
	case can::NodeType::Parameter:
	case can::NodeType::RestPose:
	case can::NodeType::Blend:
	case can::NodeType::Switch:
	case can::NodeType::AdditiveBlend:
	case can::NodeType::Subgraph:
	case can::NodeType::Count:
		// Unconnected pose inputs are reported for all node types by GetErrors
		break;
	}
}

void AnimationNetwork::ValidateAnimationSample( const can::AnimationSampleNode& node, const std::string& prefix, std::vector<std::string>& warnings ) const
{
	const auto& data = *m_data;
	if( node.assetIndex == can::NO_INDEX )
	{
		warnings.push_back( prefix + "no animation asset is set" );
		return;
	}
	const auto& asset = m_assets[node.assetIndex];
	const auto path = ToStdString( data.assets[node.assetIndex].path );
	if( asset.state == AssetState::Failed )
	{
		warnings.push_back( prefix + "failed to load animation asset '" + path + "'" );
	}
	else if( asset.state == AssetState::Loaded && !detail::FindAnimation( *asset.data, ToStdStringView( node.animationName ) ) )
	{
		warnings.push_back( prefix + "animation '" + ToStdString( node.animationName ) + "' not found in '" + path + "'" );
	}
}

bool AnimationNetwork::IsReady() const
{
	return m_compiled != nullptr;
}

AnimationNodeTypeInfo GetAnimationNodeTypeInfo( AnimationNodeType type )
{
	// The public socket description mirrors the internal one, so that the internal socket tables can be exposed
	static_assert( sizeof( AnimationSocketInfo ) == sizeof( detail::SocketInfo ) && alignof( AnimationSocketInfo ) == alignof( detail::SocketInfo ) );
	static_assert( offsetof( AnimationSocketInfo, name ) == offsetof( detail::SocketInfo, name ) && offsetof( AnimationSocketInfo, type ) == offsetof( detail::SocketInfo, type ) && offsetof( AnimationSocketInfo, defaultValue ) == offsetof( detail::SocketInfo, defaultValue ) );
	const auto info = detail::GetNodeTypeInfo( type );
	AnimationNodeTypeInfo result;
	result.name = info.name;
	result.layout = static_cast<AnimationSocketLayout>( info.layout );
	result.inputs = reinterpret_cast<const AnimationSocketInfo*>( info.inputs ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the layouts are checked by the static_asserts above
	result.inputCount = info.inputCount;
	result.hasOutput = info.hasOutput;
	result.outputType = info.outputType;
	result.outputName = info.outputName;
	// Subgraph nodes reference their graph instead of having settings
	result.hasSettings = info.hasData && type != AnimationNodeType::Subgraph;
	return result;
}
}
