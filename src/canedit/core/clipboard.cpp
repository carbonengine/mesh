// Copyright © 2026 CCP ehf.

#include "clipboard.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace canedit
{
namespace
{

using Json = nlohmann::json;

constexpr int CLIPBOARD_VERSION = 1;

const char* SocketTypeName( cmf::AnimationSocketType type )
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
	return "Pose";
}

cmf::AnimationSocketType SocketTypeFromName( const std::string& name )
{
	if( name == "Float" )
	{
		return cmf::AnimationSocketType::Float;
	}
	if( name == "BoneWeights" )
	{
		return cmf::AnimationSocketType::BoneWeights;
	}
	return cmf::AnimationSocketType::Pose;
}

std::optional<cmf::AnimationNodeType> NodeTypeFromName( const std::string& name )
{
	for( uint32_t t = 0; t < static_cast<uint32_t>( cmf::AnimationNodeType::Count ); ++t )
	{
		const auto type = static_cast<cmf::AnimationNodeType>( t );
		if( name == cmf::GetAnimationNodeTypeInfo( type ).name )
		{
			return type;
		}
	}
	return std::nullopt;
}

// JSON has no infinity: non-finite values are stored as null
Json WriteFloat( float value )
{
	return std::isfinite( value ) ? Json( value ) : Json( nullptr );
}

float ReadFloat( const Json& json, float fallback )
{
	return json.is_number() ? json.get<float>() : fallback;
}

Json WriteSettings( const cmf::AnimationNetworkBuilder& network, const cmf::AnimationNodeSettings& settings )
{
	Json json = Json::object();
	if( const auto* interfaceSettings = std::get_if<cmf::AnimationInterfaceSettings>( &settings ) )
	{
		json["socketName"] = interfaceSettings->name;
		json["socketType"] = SocketTypeName( interfaceSettings->type );
	}
	else if( const auto* parameter = std::get_if<cmf::AnimationParameterSettings>( &settings ) )
	{
		const auto& parameters = network.GetParameters();
		if( parameter->parameterIndex < parameters.size() )
		{
			const auto& definition = parameters[parameter->parameterIndex];
			json["parameter"] = { { "name", definition.name }, { "default", WriteFloat( definition.defaultValue ) }, { "min", WriteFloat( definition.minValue ) }, { "max", WriteFloat( definition.maxValue ) } };
		}
	}
	else if( const auto* sample = std::get_if<cmf::AnimationSampleSettings>( &settings ) )
	{
		json["asset"] = sample->assetPath;
		json["animation"] = sample->animationName;
	}
	else if( const auto* switchSettings = std::get_if<cmf::AnimationSwitchSettings>( &settings ) )
	{
		json["poseInputs"] = switchSettings->poseInputCount;
	}
	else if( const auto* random = std::get_if<cmf::AnimationRandomSwitchSettings>( &settings ) )
	{
		json["poseInputs"] = random->poseInputCount;
		json["weights"] = random->weights;
		json["reroll"] = random->rerollOnActivate;
	}
	else if( const auto* mask = std::get_if<cmf::AnimationNamedBoneWeightsSettings>( &settings ) )
	{
		json["mask"] = mask->maskName;
	}
	else if( const auto* weights = std::get_if<cmf::AnimationBoneWeightsSettings>( &settings ) )
	{
		json["default"] = weights->defaultWeight;
		auto& list = json["weights"];
		list = Json::array();
		for( const auto& weight : weights->weights )
		{
			list.push_back( { weight.bone, weight.weight } );
		}
	}
	else if( const auto* expression = std::get_if<cmf::AnimationExpressionSettings>( &settings ) )
	{
		json["expression"] = expression->expression;
		json["inputs"] = expression->inputNames;
	}
	return json;
}

// Subgraphs are written recursively; the depth is the nesting depth of subgraphs in the network

Json WriteGraphContents( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes );

Json WriteNode( const Document& document, const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id ) // NOLINT(misc-no-recursion): see above
{
	const auto& node = *graph.GetNode( id );
	Json json;
	json["type"] = cmf::GetAnimationNodeTypeInfo( node.type ).name;
	json["name"] = node.name;
	json["inputs"] = node.inputConstants;
	const auto position = document.GetNodePosition( graph, id ).value_or( Vec2{} );
	json["position"] = { position.x, position.y };
	json["settings"] = WriteSettings( graph.GetNetwork(), node.settings );
	if( const auto* subgraph = graph.GetSubgraph( id ) )
	{
		json["graph"] = WriteGraphContents( document, *subgraph, subgraph->GetNodes() );
	}
	return json;
}

Json WriteGraphContents( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes ) // NOLINT(misc-no-recursion): see above
{
	Json json;
	auto& nodeList = json["nodes"];
	nodeList = Json::array();
	for( const auto id : nodes )
	{
		nodeList.push_back( WriteNode( document, graph, id ) );
	}
	auto& edgeList = json["edges"];
	edgeList = Json::array();
	const auto indexOf = [&nodes]( cmf::AnimationNodeId id ) { return static_cast<size_t>( std::find( nodes.begin(), nodes.end(), id ) - nodes.begin() ); };
	for( const auto& edge : graph.GetEdges() )
	{
		const auto from = indexOf( edge.from.node );
		const auto to = indexOf( edge.to.node );
		if( from < nodes.size() && to < nodes.size() )
		{
			edgeList.push_back( { from, edge.from.socket, to, edge.to.socket } );
		}
	}
	return json;
}

class Paster
{
public:
	Paster( Document& document, PasteResult& result ) :
		m_document( &document ),
		m_result( &result )
	{
	}

	// Pastes nodes and edges into a graph; positions are moved by offset. Subgraphs are pasted recursively; the depth is
	// the nesting depth of subgraphs in the copied nodes.
	std::vector<cmf::AnimationNodeId> PasteGraph( cmf::AnimationGraphBuilder& graph, const Json& contents, Vec2 offset ) // NOLINT(misc-no-recursion): see above
	{
		std::vector<cmf::AnimationNodeId> ids;
		const auto& nodes = contents.at( "nodes" );
		for( const auto& node : nodes )
		{
			ids.push_back( PasteNode( graph, node, offset ) );
		}
		for( const auto& edge : contents.at( "edges" ) )
		{
			const auto from = edge.at( 0 ).get<size_t>();
			const auto to = edge.at( 2 ).get<size_t>();
			if( from >= ids.size() || to >= ids.size() || !ids[from].IsSet() || !ids[to].IsSet() )
			{
				continue;
			}
			const auto result = graph.Connect( { ids[from], edge.at( 1 ).get<uint32_t>() }, { ids[to], edge.at( 3 ).get<uint32_t>() } );
			if( !result )
			{
				m_result->warnings.push_back( "Edge not pasted: " + result.error );
			}
		}
		return ids;
	}

private:
	cmf::AnimationNodeId PasteNode( cmf::AnimationGraphBuilder& graph, const Json& json, Vec2 offset ) // NOLINT(misc-no-recursion): see PasteGraph
	{
		const auto typeName = json.at( "type" ).get<std::string>();
		const auto type = NodeTypeFromName( typeName );
		if( !type )
		{
			m_result->warnings.push_back( "Unknown node type '" + typeName + "'" );
			return {};
		}
		const auto id = graph.AddNode( *type );
		if( !id.IsSet() )
		{
			m_result->warnings.push_back( typeName + " node can't be pasted into this graph" );
			return {};
		}
		static_cast<void>( graph.SetNodeName( id, json.value( "name", "" ) ) );
		const auto& position = json.at( "position" );
		m_document->SetNodePosition( graph, id, { position.at( 0 ).get<float>() + offset.x, position.at( 1 ).get<float>() + offset.y } );
		ApplySettings( graph, id, *type, json.at( "settings" ) );
		const auto subgraphContents = json.find( "graph" );
		if( subgraphContents != json.end() )
		{
			if( auto* subgraph = graph.GetSubgraph( id ) )
			{
				// The contents define the interface of the subgraph, so they are pasted before the input constants
				PasteGraph( *subgraph, *subgraphContents, {} );
			}
		}
		const auto& inputs = json.at( "inputs" );
		for( uint32_t socket = 0; socket < inputs.size() && socket < graph.GetInputCount( id ); ++socket )
		{
			if( graph.GetInputType( { id, socket } ) == cmf::AnimationSocketType::Float && inputs[socket].is_number() )
			{
				static_cast<void>( graph.SetInputConstant( { id, socket }, inputs[socket].get<float>() ) );
			}
		}
		return id;
	}

	void Check( const cmf::ValidationResult& result )
	{
		if( !result )
		{
			m_result->warnings.push_back( result.error );
		}
	}

	uint32_t FindOrAddParameter( const Json& json )
	{
		auto& network = m_document->GetNetwork();
		cmf::AnimationParameter parameter;
		parameter.name = json.value( "name", "" );
		const auto existing = network.FindParameter( parameter.name );
		if( existing != cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
		{
			return existing;
		}
		parameter.defaultValue = ReadFloat( json.value( "default", Json() ), 0.f );
		parameter.minValue = ReadFloat( json.value( "min", Json() ), -std::numeric_limits<float>::infinity() );
		parameter.maxValue = ReadFloat( json.value( "max", Json() ), std::numeric_limits<float>::infinity() );
		const auto index = network.AddParameter( parameter );
		if( index == cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
		{
			m_result->warnings.push_back( "Parameter '" + parameter.name + "' couldn't be added" );
		}
		return index;
	}

	void ApplySettings( cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id, cmf::AnimationNodeType type, const Json& json )
	{
		switch( type )
		{
		case cmf::AnimationNodeType::Output:
		case cmf::AnimationNodeType::SubgraphInput:
			Check( graph.SetInterfaceSocket( id, json.value( "socketName", "" ), SocketTypeFromName( json.value( "socketType", "Pose" ) ) ) );
			break;
		case cmf::AnimationNodeType::Parameter:
			if( json.contains( "parameter" ) )
			{
				const auto index = FindOrAddParameter( json.at( "parameter" ) );
				if( index != cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
				{
					Check( graph.SetParameter( id, index ) );
				}
			}
			break;
		case cmf::AnimationNodeType::AnimationSample:
			Check( graph.SetAnimation( id, json.value( "asset", "" ), json.value( "animation", "" ) ) );
			break;
		case cmf::AnimationNodeType::Switch:
			Check( graph.SetPoseInputCount( id, json.value( "poseInputs", 2u ) ) );
			break;
		case cmf::AnimationNodeType::RandomSwitch:
			Check( graph.SetPoseInputCount( id, json.value( "poseInputs", 2u ) ) );
			Check( graph.SetRandomWeights( id, json.value( "weights", std::vector<float>() ) ) );
			Check( graph.SetRerollOnActivate( id, json.value( "reroll", false ) ) );
			break;
		case cmf::AnimationNodeType::NamedBoneWeights:
			Check( graph.SetBoneMask( id, json.value( "mask", "" ) ) );
			break;
		case cmf::AnimationNodeType::BoneWeights: {
			std::vector<cmf::AnimationNamedBoneWeight> weights;
			for( const auto& weight : json.value( "weights", Json::array() ) )
			{
				weights.push_back( { weight.at( 0 ).get<std::string>(), weight.at( 1 ).get<float>() } );
			}
			Check( graph.SetBoneWeights( id, std::move( weights ), json.value( "default", 0.f ) ) );
			break;
		}
		case cmf::AnimationNodeType::Expression: {
			const auto expression = json.value( "expression", "" );
			if( !expression.empty() )
			{
				Check( graph.SetExpression( id, expression, json.value( "inputs", std::vector<std::string>() ) ) );
			}
			break;
		}
		case cmf::AnimationNodeType::RestPose:
		case cmf::AnimationNodeType::Blend:
		case cmf::AnimationNodeType::AdditiveBlend:
		case cmf::AnimationNodeType::Subgraph:
		case cmf::AnimationNodeType::Count:
			break;
		}
	}

	Document* m_document;
	PasteResult* m_result;
};

}

std::string CopyNodes( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes )
{
	std::vector<cmf::AnimationNodeId> valid;
	std::copy_if( nodes.begin(), nodes.end(), std::back_inserter( valid ), [&graph]( cmf::AnimationNodeId id ) { return graph.IsValid( id ); } );
	Json json = WriteGraphContents( document, graph, valid );
	json[CLIPBOARD_MARKER] = CLIPBOARD_VERSION;
	return json.dump( 1, '\t' );
}

bool IsClipboardText( std::string_view text )
{
	const auto json = Json::parse( text.begin(), text.end(), nullptr, false );
	return !json.is_discarded() && json.is_object() && json.contains( CLIPBOARD_MARKER ) && json.contains( "nodes" ) && json.contains( "edges" );
}

PasteResult PasteNodes( Document& document, cmf::AnimationGraphBuilder& graph, std::string_view text, Vec2 origin )
{
	PasteResult result;
	if( !IsClipboardText( text ) )
	{
		result.error = "The clipboard doesn't contain animation network nodes";
		return result;
	}
	try
	{
		const auto json = Json::parse( text.begin(), text.end() );
		// Move the top left copied node to the origin
		Vec2 topLeft{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		for( const auto& node : json.at( "nodes" ) )
		{
			const auto& position = node.at( "position" );
			topLeft.x = std::min( topLeft.x, position.at( 0 ).get<float>() );
			topLeft.y = std::min( topLeft.y, position.at( 1 ).get<float>() );
		}
		const Vec2 offset = json.at( "nodes" ).empty() ? Vec2{} : Vec2{ origin.x - topLeft.x, origin.y - topLeft.y };
		Paster paster( document, result );
		for( const auto id : paster.PasteGraph( graph, json, offset ) )
		{
			if( id.IsSet() )
			{
				result.nodes.push_back( id );
			}
		}
	}
	catch( const nlohmann::json::exception& exception )
	{
		result.error = std::string( "Invalid clipboard content: " ) + exception.what();
	}
	return result;
}

}
