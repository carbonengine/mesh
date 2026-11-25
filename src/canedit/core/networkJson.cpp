// Copyright © 2026 CCP ehf.

#include "networkJson.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canedit
{
namespace
{

using Json = nlohmann::json;

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

Json WriteSettings( const cmf::AnimationNetworkBuilder& network, const cmf::AnimationNodeSettings& settings, GraphJsonMode mode )
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
			json["parameter"] = mode == GraphJsonMode::Document ? Json( definition.name ) : WriteParameterJson( definition );
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

Json WriteNode( const Document& document, const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id, GraphJsonMode mode ) // NOLINT(misc-no-recursion): see above
{
	const auto& node = *graph.GetNode( id );
	Json json;
	json["type"] = cmf::GetAnimationNodeTypeInfo( node.type ).name;
	json["name"] = node.name;
	auto& inputs = json["inputs"];
	inputs = Json::array();
	for( const auto value : node.inputConstants )
	{
		inputs.push_back( WriteFloat( value ) );
	}
	const auto position = document.GetNodePosition( graph, id ).value_or( Vec2{} );
	json["position"] = { position.x, position.y };
	json["settings"] = WriteSettings( graph.GetNetwork(), node.settings, mode );
	if( const auto* subgraph = graph.GetSubgraph( id ) )
	{
		json["graph"] = WriteGraphJson( document, *subgraph, subgraph->GetNodes(), mode );
	}
	return json;
}

}

Json WriteGraphJson( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes, GraphJsonMode mode ) // NOLINT(misc-no-recursion): see WriteNode
{
	Json json;
	if( mode == GraphJsonMode::Document )
	{
		const auto view = document.GetView( graph );
		json["view"] = { view.pan.x, view.pan.y, view.zoom };
	}
	auto& nodeList = json["nodes"];
	nodeList = Json::array();
	for( const auto id : nodes )
	{
		nodeList.push_back( WriteNode( document, graph, id, mode ) );
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

Json WriteParameterJson( const cmf::AnimationParameter& parameter )
{
	return { { "name", parameter.name }, { "default", WriteFloat( parameter.defaultValue ) }, { "min", WriteFloat( parameter.minValue ) }, { "max", WriteFloat( parameter.maxValue ) } };
}

cmf::AnimationParameter ReadParameterJson( const Json& json )
{
	cmf::AnimationParameter parameter;
	parameter.name = json.value( "name", "" );
	parameter.defaultValue = ReadFloat( json.value( "default", Json() ), 0.f );
	parameter.minValue = ReadFloat( json.value( "min", Json() ), -std::numeric_limits<float>::infinity() );
	parameter.maxValue = ReadFloat( json.value( "max", Json() ), std::numeric_limits<float>::infinity() );
	return parameter;
}

GraphJsonReader::GraphJsonReader( Document& document, std::vector<std::string>& warnings, GraphJsonMode mode ) :
	m_document( &document ),
	m_warnings( &warnings ),
	m_mode( mode )
{
}

// Subgraphs are read recursively; the depth is the nesting depth of subgraphs in the JSON
std::vector<cmf::AnimationNodeId> GraphJsonReader::ReadGraph( cmf::AnimationGraphBuilder& graph, const Json& contents, Vec2 offset ) // NOLINT(misc-no-recursion): see above
{
	if( m_mode == GraphJsonMode::Document )
	{
		const auto view = contents.find( "view" );
		if( view != contents.end() )
		{
			m_document->SetView( graph, { { view->at( 0 ).get<float>(), view->at( 1 ).get<float>() }, view->at( 2 ).get<float>() } );
		}
	}
	std::vector<cmf::AnimationNodeId> ids;
	for( const auto& node : contents.at( "nodes" ) )
	{
		ids.push_back( ReadNode( graph, node, offset ) );
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
			m_warnings->push_back( "Edge skipped: " + result.error );
		}
	}
	return ids;
}

cmf::AnimationNodeId GraphJsonReader::ReadNode( cmf::AnimationGraphBuilder& graph, const Json& json, Vec2 offset ) // NOLINT(misc-no-recursion): see ReadGraph
{
	const auto typeName = json.at( "type" ).get<std::string>();
	const auto type = NodeTypeFromName( typeName );
	if( !type )
	{
		m_warnings->push_back( "Unknown node type '" + typeName + "'" );
		return {};
	}
	const auto id = graph.AddNode( *type );
	if( !id.IsSet() )
	{
		m_warnings->push_back( typeName + " node can't be added to this graph" );
		return {};
	}
	static_cast<void>( graph.SetNodeName( id, json.value( "name", "" ) ) );
	const auto position = json.find( "position" );
	if( position != json.end() )
	{
		m_document->SetNodePosition( graph, id, { position->at( 0 ).get<float>() + offset.x, position->at( 1 ).get<float>() + offset.y } );
	}
	ApplySettings( graph, id, *type, json.value( "settings", Json::object() ) );
	const auto subgraphContents = json.find( "graph" );
	if( subgraphContents != json.end() )
	{
		if( auto* subgraph = graph.GetSubgraph( id ) )
		{
			// The contents define the interface of the subgraph, so they are read before the input constants
			ReadGraph( *subgraph, *subgraphContents, {} );
		}
	}
	const auto inputs = json.value( "inputs", Json::array() );
	for( uint32_t socket = 0; socket < inputs.size() && socket < graph.GetInputCount( id ); ++socket )
	{
		if( graph.GetInputType( { id, socket } ) == cmf::AnimationSocketType::Float )
		{
			const auto value = ReadFloat( inputs[socket], std::numeric_limits<float>::quiet_NaN() );
			if( !std::isnan( value ) )
			{
				static_cast<void>( graph.SetInputConstant( { id, socket }, value ) );
			}
		}
	}
	return id;
}

void GraphJsonReader::Check( const cmf::ValidationResult& result )
{
	if( !result )
	{
		m_warnings->push_back( result.error );
	}
}

uint32_t GraphJsonReader::FindParameter( const Json& json )
{
	const auto name = json.get<std::string>();
	const auto index = m_document->GetNetwork().FindParameter( name );
	if( index == cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
	{
		m_warnings->push_back( "Parameter node references unknown parameter '" + name + "'" );
	}
	return index;
}

uint32_t GraphJsonReader::FindOrAddParameter( const Json& json )
{
	auto& network = m_document->GetNetwork();
	const auto parameter = ReadParameterJson( json );
	const auto existing = network.FindParameter( parameter.name );
	if( existing != cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
	{
		return existing;
	}
	const auto index = network.AddParameter( parameter );
	if( index == cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
	{
		m_warnings->push_back( "Parameter '" + parameter.name + "' couldn't be added" );
	}
	return index;
}

void GraphJsonReader::ApplySettings( cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id, cmf::AnimationNodeType type, const Json& json )
{
	switch( type )
	{
	case cmf::AnimationNodeType::Output:
	case cmf::AnimationNodeType::SubgraphInput:
		Check( graph.SetInterfaceSocket( id, json.value( "socketName", "" ), SocketTypeFromName( json.value( "socketType", "Pose" ) ) ) );
		break;
	case cmf::AnimationNodeType::Parameter: {
		const auto parameter = json.find( "parameter" );
		if( parameter != json.end() )
		{
			const auto index = parameter->is_string() ? FindParameter( *parameter ) : FindOrAddParameter( *parameter );
			if( index != cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
			{
				Check( graph.SetParameter( id, index ) );
			}
		}
		break;
	}
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

}
