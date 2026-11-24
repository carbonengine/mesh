// Copyright © 2026 CCP ehf.

#include "nodeInfo.h"

#include <algorithm>

namespace canedit
{
namespace
{

using Type = cmf::AnimationNodeType;

const cmf::Animation* FindAnimation( const cmf::Data& data, const std::string& name )
{
	const auto* it = std::find_if( data.animations.begin(), data.animations.end(), [&name]( const cmf::Animation& animation ) { return name.empty() || cmf::ToStdString( animation.name ) == name; } );
	return it == data.animations.end() ? nullptr : &*it;
}

bool HasBoneMask( const cmf::Skeleton& skeleton, const std::string& name )
{
	return std::any_of( skeleton.boneMasks.begin(), skeleton.boneMasks.end(), [&name]( const cmf::BoneMask& mask ) { return cmf::ToStdString( mask.name ) == name; } );
}

bool HasBone( const cmf::Skeleton& skeleton, const std::string& name )
{
	return std::any_of( skeleton.bones.begin(), skeleton.bones.end(), [&name]( const cmf::String& bone ) { return cmf::ToStdString( bone ) == name; } );
}

void DiagnoseSample( const cmf::AnimationNetworkBuilder& network, const cmf::AnimationSampleSettings& sample, NodeIssues& issues )
{
	if( sample.assetPath.empty() )
	{
		issues.warnings.emplace_back( "No animation is set; the node plays the rest pose" );
		return;
	}
	const auto* data = network.GetAssetData( sample.assetPath );
	if( data == nullptr )
	{
		if( !network.HasPendingAssets() )
		{
			issues.warnings.push_back( "'" + sample.assetPath + "' failed to load; the node plays the rest pose" );
		}
	}
	else if( FindAnimation( *data, sample.animationName ) == nullptr )
	{
		issues.warnings.push_back( "Animation '" + sample.animationName + "' not found; the node plays the rest pose" );
	}
}

}

const std::vector<NodeCategory>& GetNodeCategories()
{
	static const std::vector<NodeCategory> categories = {
		{ "Interface", IM_COL32( 90, 90, 110, 255 ), { Type::Output, Type::SubgraphInput } },
		{ "Poses", IM_COL32( 150, 105, 40, 255 ), { Type::AnimationSample, Type::RestPose } },
		{ "Blending", IM_COL32( 160, 70, 60, 255 ), { Type::Blend, Type::AdditiveBlend, Type::Switch, Type::RandomSwitch } },
		{ "Values", IM_COL32( 60, 120, 70, 255 ), { Type::Parameter, Type::Expression } },
		{ "Bone Weights", IM_COL32( 60, 90, 150, 255 ), { Type::NamedBoneWeights, Type::BoneWeights } },
		{ "Structure", IM_COL32( 110, 70, 140, 255 ), { Type::Subgraph } },
	};
	return categories;
}

ImU32 GetNodeColor( cmf::AnimationNodeType type )
{
	for( const auto& category : GetNodeCategories() )
	{
		if( std::find( category.types.begin(), category.types.end(), type ) != category.types.end() )
		{
			return category.color;
		}
	}
	return IM_COL32( 90, 90, 90, 255 );
}

ImU32 GetSocketColor( cmf::AnimationSocketType type )
{
	switch( type )
	{
	case cmf::AnimationSocketType::Pose:
		return IM_COL32( 235, 175, 70, 255 );
	case cmf::AnimationSocketType::BoneWeights:
		return IM_COL32( 110, 160, 245, 255 );
	case cmf::AnimationSocketType::Float:
		return IM_COL32( 120, 210, 120, 255 );
	}
	return IM_COL32( 200, 200, 200, 255 );
}

const char* GetSocketTypeName( cmf::AnimationSocketType type )
{
	switch( type )
	{
	case cmf::AnimationSocketType::Pose:
		return "Pose";
	case cmf::AnimationSocketType::BoneWeights:
		return "BoneWeights";
	case cmf::AnimationSocketType::Float:
		return "Float";
	}
	return "Unknown";
}

const char* GetNodeDescription( cmf::AnimationNodeType type )
{
	switch( type )
	{
	case Type::Output:
		return "Output of the graph. The root graph has one Output node; in subgraphs each Output node is an output socket of the Subgraph node.";
	case Type::SubgraphInput:
		return "Input socket of a subgraph, provided by the Subgraph node in the parent graph.";
	case Type::Parameter:
		return "Value of a control parameter of the network.";
	case Type::AnimationSample:
		return "Plays an animation. The time input overrides the playhead when connected.";
	case Type::RestPose:
		return "The rest pose of the skeleton.";
	case Type::Blend:
		return "Blends two poses by alpha, optionally per bone.";
	case Type::Switch:
		return "Selects one of its pose inputs by the selector value.";
	case Type::RandomSwitch:
		return "Picks a random pose input when the selected one finishes an animation cycle.";
	case Type::AdditiveBlend:
		return "Adds the difference between the additive and base poses to a pose.";
	case Type::NamedBoneWeights:
		return "Bone weights from a bone mask of the skeleton.";
	case Type::BoneWeights:
		return "Bone weights listed per bone.";
	case Type::Subgraph:
		return "A nested graph. Double-click to open it.";
	case Type::Expression:
		return "Computes a value from its inputs, for example 'min( speed / 4, 1 )'.";
	case Type::Count:
		break;
	}
	return "";
}

std::string GetNodeTitle( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	const auto* data = graph.GetNode( node );
	if( data == nullptr )
	{
		return {};
	}
	if( !data->name.empty() )
	{
		return data->name;
	}
	return cmf::GetAnimationNodeTypeInfo( data->type ).name;
}

std::string GetNodeSummary( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	const auto* data = graph.GetNode( node );
	if( data == nullptr )
	{
		return {};
	}
	if( const auto* sample = std::get_if<cmf::AnimationSampleSettings>( &data->settings ) )
	{
		if( sample->assetPath.empty() )
		{
			return "<no animation>";
		}
		const auto slash = sample->assetPath.find_last_of( "/\\:" );
		const auto file = slash == std::string::npos ? sample->assetPath : sample->assetPath.substr( slash + 1 );
		return sample->animationName.empty() ? file : sample->animationName + " (" + file + ")";
	}
	if( const auto* parameter = std::get_if<cmf::AnimationParameterSettings>( &data->settings ) )
	{
		const auto& parameters = graph.GetNetwork().GetParameters();
		return parameter->parameterIndex < parameters.size() ? parameters[parameter->parameterIndex].name : "<no parameter>";
	}
	if( const auto* mask = std::get_if<cmf::AnimationNamedBoneWeightsSettings>( &data->settings ) )
	{
		return mask->maskName.empty() ? "<no mask>" : mask->maskName;
	}
	if( const auto* expression = std::get_if<cmf::AnimationExpressionSettings>( &data->settings ) )
	{
		return expression->expression.empty() ? "<no expression>" : expression->expression;
	}
	if( const auto* interfaceSettings = std::get_if<cmf::AnimationInterfaceSettings>( &data->settings ) )
	{
		return interfaceSettings->name;
	}
	return {};
}

NodeIssues DiagnoseNode( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	NodeIssues issues;
	const auto* data = graph.GetNode( node );
	if( data == nullptr )
	{
		return issues;
	}
	for( uint32_t socket = 0; socket < graph.GetInputCount( node ); ++socket )
	{
		if( graph.GetInputType( { node, socket } ) == cmf::AnimationSocketType::Pose && !graph.GetConnection( { node, socket } ) )
		{
			issues.errors.push_back( "Input '" + graph.GetInputName( { node, socket } ) + "' is not connected" );
		}
	}
	const auto& network = graph.GetNetwork();
	const auto* skeleton = network.GetSkeleton();
	if( const auto* sample = std::get_if<cmf::AnimationSampleSettings>( &data->settings ) )
	{
		DiagnoseSample( network, *sample, issues );
	}
	else if( const auto* mask = std::get_if<cmf::AnimationNamedBoneWeightsSettings>( &data->settings ) )
	{
		if( mask->maskName.empty() || ( skeleton != nullptr && !HasBoneMask( *skeleton, mask->maskName ) ) )
		{
			issues.warnings.emplace_back( "Bone mask not found; all bones are weighted fully" );
		}
	}
	else if( const auto* weights = std::get_if<cmf::AnimationBoneWeightsSettings>( &data->settings ) )
	{
		for( const auto& weight : weights->weights )
		{
			if( skeleton != nullptr && !HasBone( *skeleton, weight.bone ) )
			{
				issues.warnings.push_back( "Bone '" + weight.bone + "' not found" );
			}
		}
	}
	else if( const auto* expression = std::get_if<cmf::AnimationExpressionSettings>( &data->settings ) )
	{
		if( expression->expression.empty() )
		{
			issues.errors.emplace_back( "No expression is set" );
		}
	}
	else if( const auto* parameter = std::get_if<cmf::AnimationParameterSettings>( &data->settings ) )
	{
		if( parameter->parameterIndex >= network.GetParameters().size() )
		{
			issues.errors.emplace_back( "No parameter is selected" );
		}
	}
	return issues;
}

bool CanAddNode( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeType type )
{
	if( graph.GetParent() != nullptr )
	{
		return true;
	}
	if( type == Type::SubgraphInput )
	{
		return false;
	}
	return type != Type::Output || !graph.GetOutputNode().IsSet();
}

}
