// Copyright © 2026 CCP ehf.

#include "cmf/animationnetworkbuilder.h"
#include "cmf/writer.h"

#include "animationnetworkformat.h"

#include <ccpparser.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <set>
#include <tuple>
#include <variant>

namespace cmf
{
namespace
{

ValidationResult Ok()
{
	return { true, {} };
}

ValidationResult Error( std::string message )
{
	return { false, std::move( message ) };
}

detail::NodeShape GetShape( const AnimationNode& node )
{
	detail::NodeShape shape;
	shape.type = node.type;
	if( const auto* settings = std::get_if<AnimationInterfaceSettings>( &node.settings ) )
	{
		shape.interfaceType = settings->type;
	}
	else if( const auto* settings = std::get_if<AnimationSwitchSettings>( &node.settings ) )
	{
		shape.poseInputCount = settings->poseInputCount;
	}
	else if( const auto* settings = std::get_if<AnimationRandomSwitchSettings>( &node.settings ) )
	{
		shape.poseInputCount = settings->poseInputCount;
	}
	else if( const auto* settings = std::get_if<AnimationExpressionSettings>( &node.settings ) )
	{
		shape.expressionInputCount = static_cast<uint32_t>( settings->inputNames.size() );
	}
	return shape;
}

void ValidateRandomWeights( const AnimationRandomSwitchSettings& settings, const std::string& prefix, std::vector<std::string>& errors )
{
	if( settings.weights.empty() )
	{
		return;
	}
	if( settings.weights.size() != settings.poseInputCount )
	{
		errors.push_back( prefix + "number of random weights doesn't match the number of pose inputs" );
	}
	else if( std::accumulate( settings.weights.begin(), settings.weights.end(), 0.f ) <= 0.f )
	{
		errors.push_back( prefix + "random weights must not all be zero" );
	}
}

AnimationNodeSettings CreateSettings( AnimationNodeType type )
{
	// Every node type needs a case; there is deliberately no default case
	switch( type )
	{
	case AnimationNodeType::Output:
	case AnimationNodeType::SubgraphInput:
		return AnimationInterfaceSettings{};
	case AnimationNodeType::Parameter:
		return AnimationParameterSettings{};
	case AnimationNodeType::AnimationSample:
		return AnimationSampleSettings{};
	case AnimationNodeType::Switch:
		return AnimationSwitchSettings{};
	case AnimationNodeType::RandomSwitch:
		return AnimationRandomSwitchSettings{};
	case AnimationNodeType::NamedBoneWeights:
		return AnimationNamedBoneWeightsSettings{};
	case AnimationNodeType::BoneWeights:
		return AnimationBoneWeightsSettings{};
	case AnimationNodeType::Expression:
		return AnimationExpressionSettings{};
	case AnimationNodeType::RestPose:
	case AnimationNodeType::Blend:
	case AnimationNodeType::AdditiveBlend:
	case AnimationNodeType::Subgraph:
	case AnimationNodeType::Count:
		break;
	}
	return std::monostate{};
}

// Returns the asset path of an AnimationSample node, or nullptr for other nodes
const std::string* GetSampleAssetPath( const AnimationNode& node )
{
	const auto* settings = std::get_if<AnimationSampleSettings>( &node.settings );
	return settings && !settings->assetPath.empty() ? &settings->assetPath : nullptr;
}

const AnimationInterfaceSettings& GetInterface( const AnimationNode& node )
{
	return std::get<AnimationInterfaceSettings>( node.settings );
}

std::string GetNodeLabel( const AnimationNode& node, AnimationNodeId id )
{
	std::string label = std::string( detail::GetNodeTypeName( node.type ) ) + " #" + std::to_string( id.index );
	if( !node.name.empty() )
	{
		label += " '" + node.name + "'";
	}
	return label;
}

ValidationResult ParseExpression( const std::string& expression, const std::vector<std::string>& inputNames )
{
	std::vector<CcpParser::Variable> variables;
	variables.reserve( inputNames.size() );
	for( size_t i = 0; i < inputNames.size(); ++i )
	{
		variables.push_back( { inputNames[i].c_str(), 0, static_cast<CcpParser::OffsetType>( i * sizeof( float ) ) } );
	}
	std::array<CcpParser::VariableView, 1> variableViews;
	variableViews[0] = variables;
	CcpParser::Externals externals;
	externals.variables.data = variableViews.data();
	externals.variables.count = variableViews.size();

	CcpParser::Program program;
	const auto result = CcpParser::Parse( expression.c_str(), externals, program );
	if( !result )
	{
		return Error( "Invalid expression: " + CcpParser::ToString( result, expression.c_str() ) );
	}
	return Ok();
}

void ResizeInputConstants( AnimationNode& node, uint32_t inputCount )
{
	node.inputConstants.resize( inputCount, 0.f );
}

// Calls visitor for the graph and all its subgraphs, depth first
template <typename Graph, typename Visitor>
void ForEachGraph( Graph& root, const Visitor& visitor ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	std::vector<Graph*> stack = { &root };
	while( !stack.empty() )
	{
		Graph* graph = stack.back();
		stack.pop_back();
		visitor( *graph );
		for( const auto id : graph->GetNodes() )
		{
			if( auto* subgraph = graph->GetSubgraph( id ) )
			{
				stack.push_back( subgraph );
			}
		}
	}
}

}

// AnimationGraphBuilder

AnimationGraphBuilder::AnimationGraphBuilder( AnimationNetworkBuilder& network, AnimationGraphBuilder* parent, AnimationNodeId ownerNode ) :
	m_network( &network ),
	m_parent( parent ),
	m_ownerNode( ownerNode )
{
}

AnimationGraphBuilder::~AnimationGraphBuilder() = default;

AnimationGraphBuilder::Slot* AnimationGraphBuilder::FindSlot( AnimationNodeId node )
{
	if( node.index >= m_slots.size() )
	{
		return nullptr;
	}
	auto& slot = m_slots[node.index];
	return slot.alive && slot.generation == node.generation ? &slot : nullptr;
}

const AnimationGraphBuilder::Slot* AnimationGraphBuilder::FindSlot( AnimationNodeId node ) const
{
	return const_cast<AnimationGraphBuilder*>( this )->FindSlot( node ); // NOLINT(cppcoreguidelines-pro-type-const-cast): shares the non-const lookup; the result is returned as const
}

AnimationGraphBuilder::Slot* AnimationGraphBuilder::FindSlot( AnimationNodeId node, AnimationNodeType type, ValidationResult& result )
{
	auto* slot = FindSlot( node );
	if( !slot )
	{
		result = Error( "Invalid node handle" );
		return nullptr;
	}
	if( slot->node.type != type )
	{
		result = Error( "Operation requires a " + std::string( detail::GetNodeTypeName( type ) ) + " node, got " + GetNodeLabel( slot->node, node ) );
		return nullptr;
	}
	result = Ok();
	return slot;
}

AnimationNodeId AnimationGraphBuilder::AddNode( AnimationNodeType type )
{
	if( type == AnimationNodeType::SubgraphInput && !m_parent )
	{
		return {};
	}
	if( type == AnimationNodeType::Output && !m_parent && GetOutputNode().IsSet() )
	{
		return {};
	}

	const AnimationNodeId id = { static_cast<uint32_t>( m_slots.size() ), 0 };
	auto& slot = m_slots.emplace_back();
	slot.alive = true;
	slot.node.type = type;
	slot.node.settings = CreateSettings( type );

	if( type == AnimationNodeType::Subgraph )
	{
		slot.subgraph = std::unique_ptr<AnimationGraphBuilder>( new AnimationGraphBuilder( *m_network, this, id ) ); // NOLINT(modernize-make-unique): the constructor is private
	}
	else if( type == AnimationNodeType::SubgraphInput )
	{
		m_interfaceInputs.push_back( id );
	}
	else if( type == AnimationNodeType::Output )
	{
		m_interfaceOutputs.push_back( id );
	}
	ResizeInputConstants( slot.node, GetInputCount( id ) );
	for( uint32_t socket = 0; socket < slot.node.inputConstants.size(); ++socket )
	{
		slot.node.inputConstants[socket] = detail::GetDefaultInputConstant( type, socket );
	}

	if( m_parent && ( type == AnimationNodeType::SubgraphInput || type == AnimationNodeType::Output ) )
	{
		// The owner Subgraph node got a new socket
		auto* owner = m_parent->FindSlot( m_ownerNode );
		ResizeInputConstants( owner->node, m_parent->GetInputCount( m_ownerNode ) );
	}
	MarkModified();
	return id;
}

bool AnimationGraphBuilder::RemoveNode( AnimationNodeId node )
{
	auto* slot = FindSlot( node );
	if( !slot )
	{
		return false;
	}

	RemoveEdgesIf( [node]( const AnimationEdge& edge ) { return edge.from.node == node || edge.to.node == node; } );

	if( const auto* assetPath = GetSampleAssetPath( slot->node ) )
	{
		m_network->ReleaseAsset( *assetPath );
	}
	if( slot->subgraph )
	{
		slot->subgraph->ReleaseAssets();
	}

	for( const bool input : { true, false } )
	{
		auto& interfaceSockets = input ? m_interfaceInputs : m_interfaceOutputs;
		auto it = std::find( interfaceSockets.begin(), interfaceSockets.end(), node );
		if( it != interfaceSockets.end() )
		{
			const auto socket = static_cast<uint32_t>( it - interfaceSockets.begin() );
			interfaceSockets.erase( it );
			if( m_parent )
			{
				OnInterfaceSocketRemoved( input, socket );
			}
		}
	}

	slot->alive = false;
	++slot->generation;
	slot->node = {};
	slot->subgraph.reset();
	MarkModified();
	return true;
}

bool AnimationGraphBuilder::IsValid( AnimationNodeId node ) const
{
	return FindSlot( node ) != nullptr;
}

std::vector<AnimationNodeId> AnimationGraphBuilder::GetNodes() const
{
	std::vector<AnimationNodeId> result;
	for( uint32_t i = 0; i < m_slots.size(); ++i )
	{
		if( m_slots[i].alive )
		{
			result.push_back( { i, m_slots[i].generation } );
		}
	}
	return result;
}

const AnimationNode* AnimationGraphBuilder::GetNode( AnimationNodeId node ) const
{
	const auto* slot = FindSlot( node );
	return slot ? &slot->node : nullptr;
}

AnimationNodeId AnimationGraphBuilder::GetOutputNode() const
{
	return m_interfaceOutputs.empty() ? AnimationNodeId{} : m_interfaceOutputs.front();
}

const std::vector<AnimationNodeId>& AnimationGraphBuilder::GetInterfaceInputs() const
{
	return m_interfaceInputs;
}

const std::vector<AnimationNodeId>& AnimationGraphBuilder::GetInterfaceOutputs() const
{
	return m_interfaceOutputs;
}

const AnimationGraphBuilder* AnimationGraphBuilder::GetParent() const
{
	return m_parent;
}

const AnimationNetworkBuilder& AnimationGraphBuilder::GetNetwork() const
{
	return *m_network;
}

ValidationResult AnimationGraphBuilder::SetNodeName( AnimationNodeId node, std::string_view name )
{
	auto* slot = FindSlot( node );
	if( !slot )
	{
		return Error( "Invalid node handle" );
	}
	slot->node.name = name;
	MarkModified();
	return Ok();
}

uint32_t AnimationGraphBuilder::GetInputCount( AnimationNodeId node ) const
{
	const auto* slot = FindSlot( node );
	if( !slot )
	{
		return 0;
	}
	if( slot->node.type == AnimationNodeType::Subgraph )
	{
		return static_cast<uint32_t>( slot->subgraph->m_interfaceInputs.size() );
	}
	return detail::GetInputCount( GetShape( slot->node ) );
}

uint32_t AnimationGraphBuilder::GetOutputCount( AnimationNodeId node ) const
{
	const auto* slot = FindSlot( node );
	if( !slot )
	{
		return 0;
	}
	if( slot->node.type == AnimationNodeType::Subgraph )
	{
		return static_cast<uint32_t>( slot->subgraph->m_interfaceOutputs.size() );
	}
	return detail::GetOutputCount( slot->node.type );
}

AnimationSocketType AnimationGraphBuilder::GetSocketType( const Slot& slot, uint32_t socket, bool input )
{
	if( slot.node.type == AnimationNodeType::Subgraph )
	{
		const auto& graph = *slot.subgraph;
		const auto& interfaceNode = input ? graph.m_interfaceInputs[socket] : graph.m_interfaceOutputs[socket];
		return GetInterface( graph.FindSlot( interfaceNode )->node ).type;
	}
	return input ? detail::GetInputType( GetShape( slot.node ), socket ) : detail::GetOutputType( GetShape( slot.node ) );
}

AnimationSocketType AnimationGraphBuilder::GetInputType( AnimationSocketRef input ) const
{
	return GetSocketType( *FindSlot( input.node ), input.socket, true );
}

AnimationSocketType AnimationGraphBuilder::GetOutputType( AnimationSocketRef output ) const
{
	return GetSocketType( *FindSlot( output.node ), output.socket, false );
}

std::string AnimationGraphBuilder::GetInputName( AnimationSocketRef input ) const
{
	const auto* slot = FindSlot( input.node );
	if( !slot || input.socket >= GetInputCount( input.node ) )
	{
		return {};
	}
	const auto& node = slot->node;
	switch( detail::GetNodeTypeInfo( node.type ).layout )
	{
	case detail::SocketLayout::Interface:
		// Only Output nodes have an input; the one of the root graph keeps its fixed name
		return m_parent ? GetInterface( node ).name : detail::GetInputName( GetShape( node ), input.socket );
	case detail::SocketLayout::Subgraph: {
		const auto& graph = *slot->subgraph;
		return GetInterface( graph.FindSlot( graph.m_interfaceInputs[input.socket] )->node ).name;
	}
	case detail::SocketLayout::Expression:
		return std::get<AnimationExpressionSettings>( node.settings ).inputNames[input.socket];
	case detail::SocketLayout::Fixed:
	case detail::SocketLayout::Switch:
		return detail::GetInputName( GetShape( node ), input.socket );
	}
	return {};
}

std::string AnimationGraphBuilder::GetOutputName( AnimationSocketRef output ) const
{
	const auto* slot = FindSlot( output.node );
	if( !slot || output.socket >= GetOutputCount( output.node ) )
	{
		return {};
	}
	switch( detail::GetNodeTypeInfo( slot->node.type ).layout )
	{
	case detail::SocketLayout::Interface:
		// Only SubgraphInput nodes have an output
		return GetInterface( slot->node ).name;
	case detail::SocketLayout::Subgraph: {
		const auto& graph = *slot->subgraph;
		return GetInterface( graph.FindSlot( graph.m_interfaceOutputs[output.socket] )->node ).name;
	}
	case detail::SocketLayout::Fixed:
	case detail::SocketLayout::Switch:
	case detail::SocketLayout::Expression:
		return detail::GetOutputName( slot->node.type );
	}
	return {};
}

bool AnimationGraphBuilder::IsReachable( AnimationNodeId from, AnimationNodeId to ) const
{
	std::vector<AnimationNodeId> stack = { from };
	std::set<uint32_t> visited;
	while( !stack.empty() )
	{
		const auto node = stack.back();
		stack.pop_back();
		if( node == to )
		{
			return true;
		}
		if( !visited.insert( node.index ).second )
		{
			continue;
		}
		for( const auto& edge : m_edges )
		{
			if( edge.from.node == node )
			{
				stack.push_back( edge.to.node );
			}
		}
	}
	return false;
}

ValidationResult AnimationGraphBuilder::Connect( AnimationSocketRef from, AnimationSocketRef to )
{
	if( !IsValid( from.node ) || !IsValid( to.node ) )
	{
		return Error( "Invalid node handle" );
	}
	if( from.socket >= GetOutputCount( from.node ) )
	{
		return Error( "Output socket " + std::to_string( from.socket ) + " doesn't exist" );
	}
	if( to.socket >= GetInputCount( to.node ) )
	{
		return Error( "Input socket " + std::to_string( to.socket ) + " doesn't exist" );
	}
	if( GetOutputType( from ) != GetInputType( to ) )
	{
		return Error( "Socket types don't match" );
	}
	if( GetConnection( to ) )
	{
		return Error( "Input socket is already connected" );
	}
	if( IsReachable( to.node, from.node ) )
	{
		return Error( "Connection would create a cycle" );
	}
	m_edges.push_back( { from, to } );
	MarkModified();
	return Ok();
}

bool AnimationGraphBuilder::Disconnect( AnimationSocketRef to )
{
	auto it = std::find_if( m_edges.begin(), m_edges.end(), [to]( const AnimationEdge& edge ) { return edge.to == to; } );
	if( it == m_edges.end() )
	{
		return false;
	}
	m_edges.erase( it );
	MarkModified();
	return true;
}

std::optional<AnimationSocketRef> AnimationGraphBuilder::GetConnection( AnimationSocketRef to ) const
{
	auto it = std::find_if( m_edges.begin(), m_edges.end(), [to]( const AnimationEdge& edge ) { return edge.to == to; } );
	if( it == m_edges.end() )
	{
		return std::nullopt;
	}
	return it->from;
}

const std::vector<AnimationEdge>& AnimationGraphBuilder::GetEdges() const
{
	return m_edges;
}

ValidationResult AnimationGraphBuilder::SetInputConstant( AnimationSocketRef input, float value )
{
	auto* slot = FindSlot( input.node );
	if( !slot )
	{
		return Error( "Invalid node handle" );
	}
	if( input.socket >= GetInputCount( input.node ) )
	{
		return Error( "Input socket " + std::to_string( input.socket ) + " doesn't exist" );
	}
	if( GetInputType( input ) != AnimationSocketType::Float )
	{
		return Error( "Constants can only be set on Float inputs" );
	}
	slot->node.inputConstants[input.socket] = value;
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetAnimation( AnimationNodeId node, std::string_view assetPath, std::string_view animationName )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::AnimationSample, result );
	if( !slot )
	{
		return result;
	}
	auto& settings = std::get<AnimationSampleSettings>( slot->node.settings );
	if( settings.assetPath != assetPath )
	{
		// Acquire first so that an asset shared with the old path isn't released and requested again
		std::string newPath( assetPath );
		if( !newPath.empty() )
		{
			m_network->AcquireAsset( newPath );
		}
		if( !settings.assetPath.empty() )
		{
			m_network->ReleaseAsset( settings.assetPath );
		}
		settings.assetPath = std::move( newPath );
	}
	settings.animationName = animationName;
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetPoseInputCount( AnimationNodeId node, uint32_t count )
{
	auto* slot = FindSlot( node );
	if( !slot || ( slot->node.type != AnimationNodeType::Switch && slot->node.type != AnimationNodeType::RandomSwitch ) )
	{
		return Error( "Operation requires a Switch or RandomSwitch node" );
	}
	if( count == 0 )
	{
		return Error( "Pose input count must be at least 1" );
	}
	const auto inputCount = detail::GetFirstSwitchPoseInput( slot->node.type ) + count;
	RemoveEdgesIf( [node, inputCount]( const AnimationEdge& edge ) { return edge.to.node == node && edge.to.socket >= inputCount; } );
	if( auto* settings = std::get_if<AnimationSwitchSettings>( &slot->node.settings ) )
	{
		settings->poseInputCount = count;
	}
	else
	{
		std::get<AnimationRandomSwitchSettings>( slot->node.settings ).poseInputCount = count;
	}
	ResizeInputConstants( slot->node, inputCount );
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetRandomWeights( AnimationNodeId node, std::vector<float> weights )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::RandomSwitch, result );
	if( !slot )
	{
		return result;
	}
	if( std::any_of( weights.begin(), weights.end(), []( float weight ) { return !( weight >= 0.f ); } ) )
	{
		return Error( "Random weights must not be negative" );
	}
	std::get<AnimationRandomSwitchSettings>( slot->node.settings ).weights = std::move( weights );
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetRerollOnActivate( AnimationNodeId node, bool reroll )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::RandomSwitch, result );
	if( !slot )
	{
		return result;
	}
	std::get<AnimationRandomSwitchSettings>( slot->node.settings ).rerollOnActivate = reroll;
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetBoneMask( AnimationNodeId node, std::string_view maskName )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::NamedBoneWeights, result );
	if( !slot )
	{
		return result;
	}
	std::get<AnimationNamedBoneWeightsSettings>( slot->node.settings ).maskName = maskName;
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetBoneWeights( AnimationNodeId node, std::vector<AnimationNamedBoneWeight> weights, float defaultWeight )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::BoneWeights, result );
	if( !slot )
	{
		return result;
	}
	auto& settings = std::get<AnimationBoneWeightsSettings>( slot->node.settings );
	settings.weights = std::move( weights );
	settings.defaultWeight = defaultWeight;
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetExpression( AnimationNodeId node, std::string_view expression, std::vector<std::string> inputNames )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::Expression, result );
	if( !slot )
	{
		return result;
	}
	std::set<std::string_view> uniqueNames;
	for( const auto& name : inputNames )
	{
		if( name.empty() || !uniqueNames.insert( name ).second )
		{
			return Error( "Expression input names must be unique and not empty" );
		}
	}
	std::string text( expression );
	result = ParseExpression( text, inputNames );
	if( !result )
	{
		return result;
	}

	const auto inputCount = static_cast<uint32_t>( inputNames.size() );
	RemoveEdgesIf( [node, inputCount]( const AnimationEdge& edge ) { return edge.to.node == node && edge.to.socket >= inputCount; } );
	auto& settings = std::get<AnimationExpressionSettings>( slot->node.settings );
	settings.expression = std::move( text );
	settings.inputNames = std::move( inputNames );
	ResizeInputConstants( slot->node, inputCount );
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetParameter( AnimationNodeId node, uint32_t parameterIndex )
{
	ValidationResult result;
	auto* slot = FindSlot( node, AnimationNodeType::Parameter, result );
	if( !slot )
	{
		return result;
	}
	if( parameterIndex >= m_network->GetParameters().size() )
	{
		return Error( "Parameter index " + std::to_string( parameterIndex ) + " is out of range" );
	}
	std::get<AnimationParameterSettings>( slot->node.settings ).parameterIndex = parameterIndex;
	MarkModified();
	return Ok();
}

ValidationResult AnimationGraphBuilder::SetInterfaceSocket( AnimationNodeId node, std::string_view name, AnimationSocketType type )
{
	auto* slot = FindSlot( node );
	if( !slot || ( slot->node.type != AnimationNodeType::SubgraphInput && slot->node.type != AnimationNodeType::Output ) )
	{
		return Error( "Operation requires a SubgraphInput or Output node" );
	}
	const bool input = slot->node.type == AnimationNodeType::SubgraphInput;
	if( !m_parent && type != AnimationSocketType::Pose )
	{
		return Error( "The Output node of the root graph must be a Pose" );
	}
	auto& settings = std::get<AnimationInterfaceSettings>( slot->node.settings );
	settings.name = name;
	if( settings.type != type )
	{
		settings.type = type;
		// Edges inside this graph that use the node's socket have the old type
		RemoveEdgesIf( [node]( const AnimationEdge& edge ) { return edge.from.node == node || edge.to.node == node; } );
		if( m_parent )
		{
			const auto& sockets = input ? m_interfaceInputs : m_interfaceOutputs;
			const auto socket = static_cast<uint32_t>( std::find( sockets.begin(), sockets.end(), node ) - sockets.begin() );
			OnInterfaceSocketChanged( input, socket );
		}
	}
	MarkModified();
	return Ok();
}

AnimationGraphBuilder* AnimationGraphBuilder::GetSubgraph( AnimationNodeId node )
{
	auto* slot = FindSlot( node );
	return slot ? slot->subgraph.get() : nullptr;
}

const AnimationGraphBuilder* AnimationGraphBuilder::GetSubgraph( AnimationNodeId node ) const
{
	const auto* slot = FindSlot( node );
	return slot ? slot->subgraph.get() : nullptr;
}

void AnimationGraphBuilder::RemoveEdgesIf( const std::function<bool( const AnimationEdge& )>& predicate )
{
	m_edges.erase( std::remove_if( m_edges.begin(), m_edges.end(), predicate ), m_edges.end() );
}

void AnimationGraphBuilder::OnInterfaceSocketRemoved( bool input, uint32_t socket )
{
	auto& parent = *m_parent;
	const auto owner = m_ownerNode;
	parent.RemoveEdgesIf( [owner, input, socket]( const AnimationEdge& edge ) {
		const auto& ref = input ? edge.to : edge.from;
		return ref.node == owner && ref.socket == socket;
	} );
	for( auto& edge : parent.m_edges )
	{
		auto& ref = input ? edge.to : edge.from;
		if( ref.node == owner && ref.socket > socket )
		{
			--ref.socket;
		}
	}
	if( input )
	{
		auto& constants = parent.FindSlot( owner )->node.inputConstants;
		constants.erase( constants.begin() + socket );
	}
}

void AnimationGraphBuilder::OnInterfaceSocketChanged( bool input, uint32_t socket )
{
	const auto owner = m_ownerNode;
	m_parent->RemoveEdgesIf( [owner, input, socket]( const AnimationEdge& edge ) {
		const auto& ref = input ? edge.to : edge.from;
		return ref.node == owner && ref.socket == socket;
	} );
}

void AnimationGraphBuilder::ReleaseAssets()
{
	ForEachGraph( *this, [this]( const AnimationGraphBuilder& graph ) {
		for( const auto id : graph.GetNodes() )
		{
			const auto& node = *graph.GetNode( id );
			if( const auto* assetPath = GetSampleAssetPath( node ) )
			{
				m_network->ReleaseAsset( *assetPath );
			}
		}
	} );
}

void AnimationGraphBuilder::MarkModified()
{
	m_network->MarkModified();
}

// AnimationNetworkBuilder

AnimationNetworkBuilder::AnimationNetworkBuilder( AnimationAssetLoader& loader ) :
	m_loader( &loader ),
	m_aliveToken( std::make_shared<bool>( true ) ),
	m_rootGraph( new AnimationGraphBuilder( *this, nullptr, {} ) ) // NOLINT(modernize-make-unique): the constructor is private
{
}

AnimationNetworkBuilder::~AnimationNetworkBuilder()
{
	// Ignore callbacks of requests that are still in flight
	m_aliveToken.reset();
	m_rootGraph.reset();
	for( const auto& [path, entry] : m_assets )
	{
		if( !entry.embedded )
		{
			m_loader->Release( path );
		}
	}
}

void AnimationNetworkBuilder::SetSkeleton( std::string_view assetPath, std::string_view skeletonName )
{
	if( m_skeletonPath != assetPath )
	{
		std::string newPath( assetPath );
		if( !newPath.empty() )
		{
			AcquireAsset( newPath );
		}
		if( !m_skeletonPath.empty() )
		{
			ReleaseAsset( m_skeletonPath );
		}
		m_skeletonPath = std::move( newPath );
	}
	m_skeletonName = skeletonName;
	MarkModified();
}

const std::string& AnimationNetworkBuilder::GetSkeletonPath() const
{
	return m_skeletonPath;
}

const std::string& AnimationNetworkBuilder::GetSkeletonName() const
{
	return m_skeletonName;
}

const Skeleton* AnimationNetworkBuilder::GetSkeleton() const
{
	const auto* data = GetAssetData( m_skeletonPath );
	return data ? detail::FindSkeleton( *data, m_skeletonName ) : nullptr;
}

ValidationResult AnimationNetworkBuilder::EmbedAsset( std::string_view assetPath, std::vector<uint8_t> cmfFile )
{
	if( assetPath.empty() )
	{
		return Error( "Asset path must not be empty" );
	}
	ValidationOptions options;
	options.validateCrc = true;
	options.validateHeader = true;
	options.validateMainData = true;
	auto result = ValidateFile( cmfFile.data(), cmfFile.size(), options );
	if( !result )
	{
		return Error( "Invalid CMF file: " + result.error );
	}

	EmbeddedAsset asset;
	asset.loaded = cmfFile;
	asset.file = std::move( cmfFile );
	const auto& header = *reinterpret_cast<const Header*>( asset.loaded.data() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the file was validated above
	auto& data = *reinterpret_cast<Data*>( asset.loaded.data() + header.sections[0].offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): section 0 is the validated Data section
	OffsetsToPointers( data );
	asset.data = &data;

	const std::string path( assetPath );
	m_embeddedAssets[path] = std::move( asset );

	auto it = m_assets.find( path );
	if( it != m_assets.end() )
	{
		auto& entry = it->second;
		if( !entry.embedded )
		{
			m_loader->Release( path );
			entry.embedded = true;
		}
		// Invalidates callbacks of a loader request that may still be in flight
		entry.requestId = m_nextRequestId++;
		entry.state = AssetState::Loaded;
		entry.data = m_embeddedAssets[path].data;
	}
	MarkModified();
	return Ok();
}

bool AnimationNetworkBuilder::RemoveEmbeddedAsset( std::string_view assetPath )
{
	auto embedded = m_embeddedAssets.find( assetPath );
	if( embedded == m_embeddedAssets.end() )
	{
		return false;
	}
	const std::string path( assetPath );
	auto it = m_assets.find( path );
	if( it != m_assets.end() )
	{
		it->second.embedded = false;
		it->second.data = nullptr;
		RequestFromLoader( path, it->second );
	}
	m_embeddedAssets.erase( path );
	MarkModified();
	return true;
}

std::vector<std::string> AnimationNetworkBuilder::GetEmbeddedAssetPaths() const
{
	std::vector<std::string> result;
	result.reserve( m_embeddedAssets.size() );
	for( const auto& [path, asset] : m_embeddedAssets )
	{
		result.push_back( path );
	}
	return result;
}

const std::vector<uint8_t>* AnimationNetworkBuilder::GetEmbeddedAsset( std::string_view assetPath ) const
{
	auto it = m_embeddedAssets.find( assetPath );
	return it == m_embeddedAssets.end() ? nullptr : &it->second.file;
}

uint32_t AnimationNetworkBuilder::AddParameter( const AnimationParameter& parameter )
{
	if( parameter.name.empty() || FindParameter( parameter.name ) != INVALID_PARAMETER || parameter.minValue > parameter.maxValue )
	{
		return INVALID_PARAMETER;
	}
	m_parameters.push_back( parameter );
	MarkModified();
	return static_cast<uint32_t>( m_parameters.size() - 1 );
}

ValidationResult AnimationNetworkBuilder::SetParameter( uint32_t index, const AnimationParameter& parameter )
{
	if( index >= m_parameters.size() )
	{
		return Error( "Parameter index " + std::to_string( index ) + " is out of range" );
	}
	if( parameter.name.empty() )
	{
		return Error( "Parameter name must not be empty" );
	}
	const auto existing = FindParameter( parameter.name );
	if( existing != INVALID_PARAMETER && existing != index )
	{
		return Error( "Parameter '" + parameter.name + "' already exists" );
	}
	if( parameter.minValue > parameter.maxValue )
	{
		return Error( "Parameter minimum is larger than maximum" );
	}
	m_parameters[index] = parameter;
	MarkModified();
	return Ok();
}

ValidationResult AnimationNetworkBuilder::RemoveParameter( uint32_t index )
{
	if( index >= m_parameters.size() )
	{
		return Error( "Parameter index " + std::to_string( index ) + " is out of range" );
	}
	if( IsParameterReferenced( index ) )
	{
		return Error( "Parameter '" + m_parameters[index].name + "' is used by a Parameter node" );
	}
	m_parameters.erase( m_parameters.begin() + index );
	ShiftParameterReferences( index );
	MarkModified();
	return Ok();
}

uint32_t AnimationNetworkBuilder::FindParameter( std::string_view name ) const
{
	auto it = std::find_if( m_parameters.begin(), m_parameters.end(), [name]( const AnimationParameter& parameter ) { return parameter.name == name; } );
	return it == m_parameters.end() ? INVALID_PARAMETER : static_cast<uint32_t>( it - m_parameters.begin() );
}

const std::vector<AnimationParameter>& AnimationNetworkBuilder::GetParameters() const
{
	return m_parameters;
}

AnimationGraphBuilder& AnimationNetworkBuilder::GetRootGraph()
{
	return *m_rootGraph;
}

const AnimationGraphBuilder& AnimationNetworkBuilder::GetRootGraph() const
{
	return *m_rootGraph;
}

bool AnimationNetworkBuilder::HasPendingAssets() const
{
	return std::any_of( m_assets.begin(), m_assets.end(), []( const auto& asset ) { return asset.second.state == AssetState::Pending; } );
}

std::vector<std::string> AnimationNetworkBuilder::GetErrors() const
{
	std::vector<std::string> errors;
	if( m_skeletonPath.empty() )
	{
		errors.emplace_back( "No skeleton is set" );
	}
	else
	{
		const auto& entry = m_assets.find( m_skeletonPath )->second;
		if( entry.state == AssetState::Failed )
		{
			errors.push_back( "Failed to load skeleton asset '" + m_skeletonPath + "'" );
		}
		else if( entry.state == AssetState::Loaded && !GetSkeleton() )
		{
			errors.push_back( "Skeleton '" + m_skeletonName + "' not found in '" + m_skeletonPath + "'" );
		}
	}
	if( !m_rootGraph->GetOutputNode().IsSet() )
	{
		errors.emplace_back( "The root graph has no Output node" );
	}
	std::vector<std::pair<const AnimationGraphBuilder*, std::string>> graphs = { { m_rootGraph.get(), "root" } };
	while( !graphs.empty() )
	{
		const auto [graph, graphName] = graphs.back();
		graphs.pop_back();
		for( const auto id : graph->GetNodes() )
		{
			const auto label = graphName + ": " + GetNodeLabel( *graph->GetNode( id ), id );
			ValidateNode( *graph, id, label + ": ", errors );
			if( const auto* subgraph = graph->GetSubgraph( id ) )
			{
				graphs.emplace_back( subgraph, graphName + "/" + GetNodeLabel( *graph->GetNode( id ), id ) );
			}
		}
	}
	return errors;
}

bool AnimationNetworkBuilder::IsReady() const
{
	return !HasPendingAssets() && GetErrors().empty();
}

const cmf::Data* AnimationNetworkBuilder::GetAssetData( std::string_view assetPath ) const
{
	auto it = m_assets.find( assetPath );
	if( it == m_assets.end() || it->second.state != AssetState::Loaded )
	{
		return nullptr;
	}
	return it->second.data;
}

uint64_t AnimationNetworkBuilder::GetGeneration() const
{
	return m_generation;
}

void AnimationNetworkBuilder::AcquireAsset( const std::string& assetPath )
{
	auto it = m_assets.find( assetPath );
	if( it != m_assets.end() )
	{
		++it->second.referenceCount;
		return;
	}
	auto& entry = m_assets[assetPath];
	entry.referenceCount = 1;
	auto embedded = m_embeddedAssets.find( assetPath );
	if( embedded != m_embeddedAssets.end() )
	{
		entry.embedded = true;
		entry.state = AssetState::Loaded;
		entry.data = embedded->second.data;
		return;
	}
	RequestFromLoader( assetPath, entry );
}

void AnimationNetworkBuilder::ReleaseAsset( const std::string& assetPath )
{
	auto it = m_assets.find( assetPath );
	if( it == m_assets.end() || --it->second.referenceCount > 0 )
	{
		return;
	}
	const bool embedded = it->second.embedded;
	m_assets.erase( it );
	if( !embedded )
	{
		m_loader->Release( assetPath );
	}
}

void AnimationNetworkBuilder::RequestFromLoader( const std::string& assetPath, AssetEntry& entry )
{
	entry.state = AssetState::Pending;
	entry.data = nullptr;
	entry.requestId = m_nextRequestId++;
	std::weak_ptr<bool> alive = m_aliveToken;
	const auto requestId = entry.requestId;
	// The callback may run before RequestLoad returns, so the entry must be fully set up at this point
	m_loader->RequestLoad( assetPath, [this, alive = std::move( alive ), requestId]( const cmf::Data* data ) {
		if( !alive.expired() )
		{
			OnAssetLoaded( requestId, data );
		}
	} );
}

void AnimationNetworkBuilder::OnAssetLoaded( uint32_t requestId, const cmf::Data* data )
{
	auto it = std::find_if( m_assets.begin(), m_assets.end(), [requestId]( const auto& asset ) { return asset.second.requestId == requestId; } );
	if( it == m_assets.end() || it->second.embedded )
	{
		// The request was released or superseded
		return;
	}
	it->second.state = data ? AssetState::Loaded : AssetState::Failed;
	it->second.data = data;
	MarkModified();
}

void AnimationNetworkBuilder::ValidateNode( const AnimationGraphBuilder& graph, AnimationNodeId id, const std::string& prefix, std::vector<std::string>& errors ) const
{
	const auto& node = *graph.GetNode( id );
	for( uint32_t socket = 0; socket < graph.GetInputCount( id ); ++socket )
	{
		const AnimationSocketRef input = { id, socket };
		if( graph.GetInputType( input ) == AnimationSocketType::Pose && !graph.GetConnection( input ) )
		{
			errors.push_back( prefix + "input '" + graph.GetInputName( input ) + "' is not connected" );
		}
	}

	const auto* skeleton = GetSkeleton();
	if( const auto* sample = std::get_if<AnimationSampleSettings>( &node.settings ) )
	{
		ValidateAnimationSample( *sample, prefix, errors );
	}
	else if( const auto* mask = std::get_if<AnimationNamedBoneWeightsSettings>( &node.settings ) )
	{
		if( mask->maskName.empty() )
		{
			errors.push_back( prefix + "no bone mask is set" );
		}
		else if( skeleton && !detail::FindBoneMask( *skeleton, mask->maskName ) )
		{
			errors.push_back( prefix + "bone mask '" + mask->maskName + "' not found in the skeleton" );
		}
	}
	else if( const auto* weights = std::get_if<AnimationBoneWeightsSettings>( &node.settings ) )
	{
		for( const auto& weight : weights->weights )
		{
			if( skeleton && detail::FindBone( *skeleton, weight.bone ) == can::NO_INDEX )
			{
				errors.push_back( prefix + "bone '" + weight.bone + "' not found in the skeleton" );
			}
		}
	}
	else if( const auto* parameter = std::get_if<AnimationParameterSettings>( &node.settings ) )
	{
		if( parameter->parameterIndex >= m_parameters.size() )
		{
			errors.push_back( prefix + "parameter index is out of range" );
		}
	}
	else if( const auto* expression = std::get_if<AnimationExpressionSettings>( &node.settings ) )
	{
		if( expression->expression.empty() )
		{
			errors.push_back( prefix + "no expression is set" );
		}
	}
	else if( const auto* random = std::get_if<AnimationRandomSwitchSettings>( &node.settings ) )
	{
		ValidateRandomWeights( *random, prefix, errors );
	}
}

void AnimationNetworkBuilder::ValidateAnimationSample( const AnimationSampleSettings& settings, const std::string& prefix, std::vector<std::string>& errors ) const
{
	if( settings.assetPath.empty() )
	{
		errors.push_back( prefix + "no animation asset is set" );
		return;
	}
	const auto& entry = m_assets.find( settings.assetPath )->second;
	if( entry.state == AssetState::Failed )
	{
		errors.push_back( prefix + "failed to load animation asset '" + settings.assetPath + "'" );
	}
	else if( entry.state == AssetState::Loaded && !detail::FindAnimation( *entry.data, settings.animationName ) )
	{
		errors.push_back( prefix + "animation '" + settings.animationName + "' not found in '" + settings.assetPath + "'" );
	}
}

bool AnimationNetworkBuilder::IsParameterReferenced( uint32_t index ) const
{
	bool referenced = false;
	ForEachGraph( *m_rootGraph, [index, &referenced]( const AnimationGraphBuilder& graph ) {
		for( const auto id : graph.GetNodes() )
		{
			const auto* parameter = graph.GetSettings<AnimationParameterSettings>( id );
			referenced = referenced || ( parameter && parameter->parameterIndex == index );
		}
	} );
	return referenced;
}

void AnimationNetworkBuilder::ShiftParameterReferences( uint32_t removedIndex )
{
	ForEachGraph( *m_rootGraph, [removedIndex]( AnimationGraphBuilder& graph ) {
		for( auto& slot : graph.m_slots )
		{
			auto* parameter = std::get_if<AnimationParameterSettings>( &slot.node.settings );
			if( slot.alive && parameter && parameter->parameterIndex > removedIndex )
			{
				--parameter->parameterIndex;
			}
		}
	} );
}

void AnimationNetworkBuilder::MarkModified()
{
	++m_generation;
}

// Serialization

class AnimationNetworkBuilder::Serializer
{
public:
	static std::vector<uint8_t> Save( const AnimationNetworkBuilder& builder )
	{
		Serializer serializer;
		return serializer.Write( builder );
	}

	static void Load( AnimationNetworkBuilder& builder, const uint8_t* file )
	{
		const auto& data = detail::GetNetworkData( file );
		for( const auto& asset : data.assets )
		{
			if( asset.section != can::NO_INDEX )
			{
				const auto [bytes, size] = detail::GetSectionBytes( file, asset.section );
				builder.EmbedAsset( ToStdStringView( asset.path ), std::vector<uint8_t>( bytes, bytes + size ) );
			}
		}
		if( data.skeletonAsset != can::NO_INDEX )
		{
			builder.SetSkeleton( ToStdStringView( data.assets[data.skeletonAsset].path ), ToStdStringView( data.skeletonName ) );
		}
		for( const auto& parameter : data.parameters )
		{
			builder.m_parameters.push_back( { ToStdString( parameter.name ), parameter.defaultValue, parameter.minValue, parameter.maxValue } );
		}
		ReadGraphs( *builder.m_rootGraph, data );
	}

private:
	MemoryAllocator m_allocator;
	can::NetworkData m_data;
	std::map<std::string, uint32_t, std::less<>> m_assetIndices;
	std::vector<can::Graph> m_graphs;

	std::vector<uint8_t> Write( const AnimationNetworkBuilder& builder )
	{
		std::vector<const std::vector<uint8_t>*> embeddedFiles;
		for( const auto& [path, asset] : builder.m_embeddedAssets )
		{
			embeddedFiles.push_back( &asset.file );
			m_data.assets[GetAssetIndex( path )].section = static_cast<uint32_t>( embeddedFiles.size() );
		}
		m_data.skeletonAsset = GetAssetIndex( builder.m_skeletonPath );
		m_data.skeletonName = m_allocator.AllocateString( builder.m_skeletonName );
		for( const auto& parameter : builder.m_parameters )
		{
			Modify( m_data.parameters, m_allocator ).push_back( { m_allocator.AllocateString( parameter.name ), parameter.defaultValue, parameter.minValue, parameter.maxValue } );
		}
		WriteGraphs( *builder.m_rootGraph );
		Modify( m_data.graphs, m_allocator ) = m_graphs;
		return WriteFile( embeddedFiles );
	}

	uint32_t GetAssetIndex( const std::string& path )
	{
		if( path.empty() )
		{
			return can::NO_INDEX;
		}
		auto it = m_assetIndices.find( path );
		if( it != m_assetIndices.end() )
		{
			return it->second;
		}
		const auto index = static_cast<uint32_t>( m_data.assets.size() );
		Modify( m_data.assets, m_allocator ).push_back( { m_allocator.AllocateString( path ), can::NO_INDEX } );
		m_assetIndices.emplace( path, index );
		return index;
	}

	using PendingGraphs = std::vector<std::pair<const AnimationGraphBuilder*, uint32_t>>;

	// Writes the root graph and all subgraphs to m_graphs
	void WriteGraphs( const AnimationGraphBuilder& root )
	{
		m_graphs.emplace_back();
		PendingGraphs pending = { { &root, 0 } };
		while( !pending.empty() )
		{
			const auto [graph, graphIndex] = pending.back();
			pending.pop_back();
			WriteGraph( *graph, graphIndex, pending );
		}
	}

	// Writes a graph to m_graphs[graphIndex]; its subgraphs get new indices and are added to pending
	void WriteGraph( const AnimationGraphBuilder& graph, uint32_t graphIndex, PendingGraphs& pending )
	{
		// Nodes are stored without the slots of removed nodes
		// m_graphs grows while subgraphs are added, so the graph is assembled here and stored at the end
		can::Graph result;
		std::vector<uint32_t> nodeIndices( graph.m_slots.size(), can::NO_INDEX );
		std::vector<can::Node> nodes;
		for( uint32_t i = 0; i < graph.m_slots.size(); ++i )
		{
			const auto& slot = graph.m_slots[i];
			if( !slot.alive )
			{
				continue;
			}
			nodeIndices[i] = static_cast<uint32_t>( nodes.size() );
			nodes.push_back( WriteNode( slot.node, result ) );
			if( slot.subgraph )
			{
				const auto subgraphIndex = static_cast<uint32_t>( m_graphs.size() );
				m_graphs.emplace_back();
				pending.emplace_back( slot.subgraph.get(), subgraphIndex );
				nodes.back().dataIndex = AddNodeData( result.subgraphNodes, can::SubgraphNode{ subgraphIndex } );
			}
		}

		std::vector<can::Edge> edges;
		edges.reserve( graph.m_edges.size() );
		for( const auto& edge : graph.m_edges )
		{
			edges.push_back( { { nodeIndices[edge.from.node.index], edge.from.socket }, { nodeIndices[edge.to.node.index], edge.to.socket } } );
		}

		Modify( result.nodes, m_allocator ) = nodes;
		Modify( result.edges, m_allocator ) = edges;
		for( const auto& id : graph.m_interfaceInputs )
		{
			Modify( result.interfaceInputs, m_allocator ).push_back( nodeIndices[id.index] );
		}
		for( const auto& id : graph.m_interfaceOutputs )
		{
			Modify( result.interfaceOutputs, m_allocator ).push_back( nodeIndices[id.index] );
		}
		m_graphs[graphIndex] = result;
	}

	// Appends node data to a graph table and returns its index
	template <typename T>
	uint32_t AddNodeData( Span<T>& table, const T& data ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
	{
		Modify( table, m_allocator ).push_back( data );
		return static_cast<uint32_t>( table.size() - 1 );
	}

	Span<String> WriteStrings( const std::vector<std::string>& strings )
	{
		auto result = m_allocator.AllocateSpan<String>( strings.size() );
		for( size_t i = 0; i < strings.size(); ++i )
		{
			result[i] = m_allocator.AllocateString( strings[i] );
		}
		return result;
	}

	// Writes the common node data and adds the type specific data to the graph tables
	can::Node WriteNode( const AnimationNode& node, can::Graph& graph )
	{
		can::Node result;
		result.type = node.type;
		result.name = m_allocator.AllocateString( node.name );
		Modify( result.inputConstants, m_allocator ) = node.inputConstants;
		std::visit( [this, &result, &graph]( const auto& settings ) { result.dataIndex = this->WriteSettings( settings, graph ); }, node.settings );
		return result;
	}

	static uint32_t WriteSettings( const std::monostate& /*settings*/, can::Graph& /*graph*/ )
	{
		return can::NO_INDEX;
	}

	uint32_t WriteSettings( const AnimationInterfaceSettings& settings, can::Graph& graph )
	{
		can::InterfaceNode data;
		data.type = settings.type;
		data.name = m_allocator.AllocateString( settings.name );
		return AddNodeData( graph.interfaceNodes, data );
	}

	uint32_t WriteSettings( const AnimationParameterSettings& settings, can::Graph& graph )
	{
		return AddNodeData( graph.parameterNodes, can::ParameterNode{ settings.parameterIndex } );
	}

	uint32_t WriteSettings( const AnimationSampleSettings& settings, can::Graph& graph )
	{
		can::AnimationSampleNode data;
		data.assetIndex = GetAssetIndex( settings.assetPath );
		data.animationName = m_allocator.AllocateString( settings.animationName );
		return AddNodeData( graph.animationSampleNodes, data );
	}

	uint32_t WriteSettings( const AnimationSwitchSettings& settings, can::Graph& graph )
	{
		return AddNodeData( graph.switchNodes, can::SwitchNode{ settings.poseInputCount } );
	}

	uint32_t WriteSettings( const AnimationRandomSwitchSettings& settings, can::Graph& graph )
	{
		can::RandomSwitchNode data;
		data.poseInputCount = settings.poseInputCount;
		data.flags = settings.rerollOnActivate ? can::RANDOM_SWITCH_REROLL_ON_ACTIVATE : 0;
		Modify( data.weights, m_allocator ) = settings.weights;
		return AddNodeData( graph.randomSwitchNodes, data );
	}

	uint32_t WriteSettings( const AnimationNamedBoneWeightsSettings& settings, can::Graph& graph )
	{
		return AddNodeData( graph.namedBoneWeightsNodes, can::NamedBoneWeightsNode{ m_allocator.AllocateString( settings.maskName ) } );
	}

	uint32_t WriteSettings( const AnimationBoneWeightsSettings& settings, can::Graph& graph )
	{
		can::BoneWeightsNode data;
		data.defaultWeight = settings.defaultWeight;
		data.weights = m_allocator.AllocateSpan<can::NamedBoneWeight>( settings.weights.size() );
		for( size_t i = 0; i < settings.weights.size(); ++i )
		{
			data.weights[i].bone = m_allocator.AllocateString( settings.weights[i].bone );
			data.weights[i].weight = settings.weights[i].weight;
		}
		return AddNodeData( graph.boneWeightsNodes, data );
	}

	uint32_t WriteSettings( const AnimationExpressionSettings& settings, can::Graph& graph )
	{
		can::ExpressionNode data;
		data.expression = m_allocator.AllocateString( settings.expression );
		data.inputNames = WriteStrings( settings.inputNames );
		return AddNodeData( graph.expressionNodes, data );
	}

	static uint32_t Align( uint32_t offset )
	{
		constexpr uint32_t alignment = 8;
		return ( offset + alignment - 1 ) / alignment * alignment;
	}

	std::vector<uint8_t> WriteFile( const std::vector<const std::vector<uint8_t>*>& embeddedFiles )
	{
		auto network = Flatten( m_data );
		PointersToOffsets( *reinterpret_cast<can::NetworkData*>( network.data.get() ) ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): Flatten returns the flattened root structure

		can::Header header;
		Modify( header.sections, m_allocator ).push_back( { 0, static_cast<uint32_t>( network.size ), can::SectionType::Network } );
		for( const auto* file : embeddedFiles )
		{
			Modify( header.sections, m_allocator ).push_back( { 0, static_cast<uint32_t>( file->size() ), can::SectionType::EmbeddedCmf } );
		}
		auto flattenedHeader = Flatten( header );
		auto& fileHeader = *reinterpret_cast<can::Header*>( flattenedHeader.data.get() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): Flatten returns the flattened root structure
		fileHeader.headerSize = static_cast<uint32_t>( flattenedHeader.size );
		uint32_t offset = fileHeader.headerSize;
		for( auto& section : fileHeader.sections )
		{
			section.offset = Align( offset );
			offset = section.offset + section.size;
		}
		std::vector<uint8_t> result( offset, 0 );
		const auto& sections = fileHeader.sections;
		std::memcpy( result.data() + sections[0].offset, network.data.get(), network.size );
		for( uint32_t i = 0; i < embeddedFiles.size(); ++i )
		{
			std::memcpy( result.data() + sections[i + 1].offset, embeddedFiles[i]->data(), embeddedFiles[i]->size() );
		}
		PointersToOffsets( fileHeader );
		std::memcpy( result.data(), flattenedHeader.data.get(), flattenedHeader.size );

		constexpr size_t crcEnd = offsetof( can::Header, crc32 ) + sizeof( can::Header::crc32 );
		const auto crc = ComputeCrc32( result.data() + crcEnd, result.size() - crcEnd );
		std::memcpy( result.data() + offsetof( can::Header, crc32 ), &crc, sizeof( crc ) );
		return result;
	}

	static void ReadGraphs( AnimationGraphBuilder& root, const can::NetworkData& data )
	{
		std::vector<std::pair<AnimationGraphBuilder*, uint32_t>> pending = { { &root, 0 } };
		// Sockets of Subgraph nodes depend on their subgraph, so their input constants are set when all graphs are read
		std::vector<std::tuple<AnimationGraphBuilder*, AnimationNodeId, const can::Node*>> subgraphNodes;
		while( !pending.empty() )
		{
			const auto [graph, graphIndex] = pending.back();
			pending.pop_back();
			const auto& source = data.graphs[graphIndex];
			std::vector<AnimationNodeId> ids;
			ids.reserve( source.nodes.size() );
			for( const auto& node : source.nodes )
			{
				const auto id = graph->AddNode( node.type );
				ids.push_back( id );
				auto& slot = *graph->FindSlot( id );
				ReadNode( *graph, slot, data, source, node );
				if( slot.subgraph )
				{
					pending.emplace_back( slot.subgraph.get(), detail::GetNodeData<can::SubgraphNode>( source, node ).graphIndex );
					subgraphNodes.emplace_back( graph, id, &node );
				}
			}
			for( const auto& edge : source.edges )
			{
				graph->m_edges.push_back( { { ids[edge.from.node], edge.from.socket }, { ids[edge.to.node], edge.to.socket } } );
			}
			// Interface order may differ from the node order
			graph->m_interfaceInputs.clear();
			for( const auto index : source.interfaceInputs )
			{
				graph->m_interfaceInputs.push_back( ids[index] );
			}
			graph->m_interfaceOutputs.clear();
			for( const auto index : source.interfaceOutputs )
			{
				graph->m_interfaceOutputs.push_back( ids[index] );
			}
		}
		for( const auto& [graph, id, node] : subgraphNodes )
		{
			graph->FindSlot( id )->node.inputConstants.assign( node->inputConstants.begin(), node->inputConstants.end() );
		}
	}

	static std::vector<std::string> ReadStrings( const Span<String>& strings )
	{
		std::vector<std::string> result;
		result.reserve( strings.size() );
		for( const auto& string : strings )
		{
			result.push_back( ToStdString( string ) );
		}
		return result;
	}

	static void ReadNode( AnimationGraphBuilder& graph, AnimationGraphBuilder::Slot& slot, const can::NetworkData& data, const can::Graph& sourceGraph, const can::Node& source )
	{
		auto& node = slot.node;
		node.name = ToStdString( source.name );
		node.inputConstants.assign( source.inputConstants.begin(), source.inputConstants.end() );
		node.settings = ReadSettings( data, sourceGraph, source );
		if( const auto* assetPath = GetSampleAssetPath( node ) )
		{
			graph.m_network->AcquireAsset( *assetPath );
		}
	}

	static AnimationNodeSettings ReadSettings( const can::NetworkData& data, const can::Graph& graph, const can::Node& node )
	{
		switch( node.type )
		{
		case AnimationNodeType::Output:
		case AnimationNodeType::SubgraphInput: {
			const auto& source = detail::GetNodeData<can::InterfaceNode>( graph, node );
			return AnimationInterfaceSettings{ ToStdString( source.name ), source.type };
		}
		case AnimationNodeType::Parameter:
			return AnimationParameterSettings{ detail::GetNodeData<can::ParameterNode>( graph, node ).parameterIndex };
		case AnimationNodeType::AnimationSample: {
			const auto& source = detail::GetNodeData<can::AnimationSampleNode>( graph, node );
			const auto assetPath = source.assetIndex == can::NO_INDEX ? std::string() : ToStdString( data.assets[source.assetIndex].path );
			return AnimationSampleSettings{ assetPath, ToStdString( source.animationName ) };
		}
		case AnimationNodeType::Switch:
			return AnimationSwitchSettings{ detail::GetNodeData<can::SwitchNode>( graph, node ).poseInputCount };
		case AnimationNodeType::RandomSwitch: {
			const auto& source = detail::GetNodeData<can::RandomSwitchNode>( graph, node );
			return AnimationRandomSwitchSettings{ source.poseInputCount, { source.weights.begin(), source.weights.end() }, ( source.flags & can::RANDOM_SWITCH_REROLL_ON_ACTIVATE ) != 0 };
		}
		case AnimationNodeType::NamedBoneWeights:
			return AnimationNamedBoneWeightsSettings{ ToStdString( detail::GetNodeData<can::NamedBoneWeightsNode>( graph, node ).maskName ) };
		case AnimationNodeType::BoneWeights: {
			const auto& source = detail::GetNodeData<can::BoneWeightsNode>( graph, node );
			AnimationBoneWeightsSettings settings;
			settings.defaultWeight = source.defaultWeight;
			for( const auto& weight : source.weights )
			{
				settings.weights.push_back( { ToStdString( weight.bone ), weight.weight } );
			}
			return settings;
		}
		case AnimationNodeType::Expression: {
			const auto& source = detail::GetNodeData<can::ExpressionNode>( graph, node );
			return AnimationExpressionSettings{ ToStdString( source.expression ), ReadStrings( source.inputNames ) };
		}
		case AnimationNodeType::RestPose:
		case AnimationNodeType::Blend:
		case AnimationNodeType::AdditiveBlend:
		case AnimationNodeType::Subgraph:
		case AnimationNodeType::Count:
			break;
		}
		return std::monostate{};
	}
};

std::vector<uint8_t> AnimationNetworkBuilder::Save() const
{
	return Serializer::Save( *this );
}

ValidationResult AnimationNetworkBuilder::Load( const void* file, size_t size )
{
	// Copy the file so that it is aligned and can be read in place
	const auto* bytes = static_cast<const uint8_t*>( file );
	const std::vector<uint8_t> buffer( bytes, bytes + size );
	auto result = detail::ValidateCanFile( buffer.data(), buffer.size() );
	if( !result )
	{
		return result;
	}
	Clear();
	Serializer::Load( *this, buffer.data() );
	MarkModified();
	return result;
}

void AnimationNetworkBuilder::Clear()
{
	m_rootGraph->ReleaseAssets();
	if( !m_skeletonPath.empty() )
	{
		ReleaseAsset( m_skeletonPath );
	}
	m_skeletonPath.clear();
	m_skeletonName.clear();
	m_rootGraph = std::unique_ptr<AnimationGraphBuilder>( new AnimationGraphBuilder( *this, nullptr, {} ) ); // NOLINT(modernize-make-unique): the constructor is private
	m_parameters.clear();
	m_embeddedAssets.clear();
	MarkModified();
}

}
