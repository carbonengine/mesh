// Copyright © 2026 CCP ehf.

#include "animationnetworkformat.h"

#include "validationutils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <set>

namespace cmf::detail
{
namespace
{

constexpr uint32_t SECTION_ALIGNMENT = 8;

// Fixed input sockets of the node types, in socket index order (see the *_INPUT constants in the header)
constexpr std::array<SocketInfo, 1> OUTPUT_INPUTS = { { { "pose", can::SocketType::Pose, 0.f } } };
constexpr std::array<SocketInfo, 3> SAMPLE_INPUTS = { {
	{ "time", can::SocketType::Float, 0.f },
	{ "loop", can::SocketType::Float, 1.f },
	{ "speed", can::SocketType::Float, 1.f },
} };
constexpr std::array<SocketInfo, 4> BLEND_INPUTS = { {
	{ "poseA", can::SocketType::Pose, 0.f },
	{ "poseB", can::SocketType::Pose, 0.f },
	{ "alpha", can::SocketType::Float, 0.f },
	{ "weights", can::SocketType::BoneWeights, 0.f },
} };
constexpr std::array<SocketInfo, 5> ADDITIVE_INPUTS = { {
	{ "pose", can::SocketType::Pose, 0.f },
	{ "base", can::SocketType::Pose, 0.f },
	{ "additive", can::SocketType::Pose, 0.f },
	{ "alpha", can::SocketType::Float, 1.f },
	{ "weights", can::SocketType::BoneWeights, 0.f },
} };
constexpr std::array<SocketInfo, 1> SWITCH_INPUTS = { { { "selector", can::SocketType::Float, 0.f } } };

static_assert( SAMPLE_INPUTS.size() == SAMPLE_SPEED_INPUT + 1 && BLEND_INPUTS.size() == BLEND_WEIGHTS_INPUT + 1 && ADDITIVE_INPUTS.size() == ADDITIVE_WEIGHTS_INPUT + 1, "Socket tables must match the socket index constants" );

template <size_t Count>
NodeTypeInfo MakeInfo( const char* name, SocketLayout layout, const std::array<SocketInfo, Count>& inputs, can::SocketType outputType, const char* outputName, bool hasData ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	return { name, layout, inputs.data(), static_cast<uint32_t>( Count ), true, outputType, outputName, hasData };
}

NodeTypeInfo MakeInfo( const char* name, SocketLayout layout, can::SocketType outputType, const char* outputName, bool hasData )
{
	return { name, layout, nullptr, 0, true, outputType, outputName, hasData };
}

const can::Header& GetHeader( const void* file )
{
	return *static_cast<const can::Header*>( file );
}

const can::InterfaceNode& GetInterfaceNode( const can::Graph& graph, uint32_t nodeIndex )
{
	return GetNodeData<can::InterfaceNode>( graph, graph.nodes[nodeIndex] );
}

// Checks that every node has a valid type and that nodes with type specific data reference their own table entry
std::string ValidateNodeData( const can::Graph& graph )
{
	std::vector<std::vector<bool>> used( static_cast<size_t>( can::NodeType::Count ) );
	for( uint32_t n = 0; n < graph.nodes.size(); ++n )
	{
		const auto& node = graph.nodes[n];
		const auto prefix = "Node " + std::to_string( n ) + ": ";
		if( !IsValidNodeType( node.type ) )
		{
			return prefix + "invalid type";
		}
		if( !HasNodeData( node.type ) )
		{
			if( node.dataIndex != can::NO_INDEX )
			{
				return prefix + "unexpected node data";
			}
			continue;
		}
		auto& typeUsed = used[static_cast<size_t>( node.type )];
		typeUsed.resize( GetNodeTableSize( graph, node.type ), false );
		if( node.dataIndex >= typeUsed.size() || typeUsed[node.dataIndex] )
		{
			return prefix + "invalid node data index";
		}
		typeUsed[node.dataIndex] = true;
	}
	for( const auto& interfaceNode : graph.interfaceNodes )
	{
		if( !IsValidSocketType( interfaceNode.type ) )
		{
			return "Invalid interface socket type";
		}
	}
	return {};
}

std::string ValidateHeader( const uint8_t* bytes, size_t size )
{
	if( reinterpret_cast<uintptr_t>( bytes ) % alignof( can::Header ) != 0 ) // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): alignment check needs the address as an integer
	{
		return "File data is not aligned";
	}
	if( size < sizeof( can::Header ) )
	{
		return "File is too small";
	}
	const auto& header = GetHeader( bytes );
	if( header.signature != can::FILE_SIGNATURE )
	{
		return "Invalid file signature";
	}
	if( header.version != can::FILE_VERSION )
	{
		return "Unsupported file version " + std::to_string( header.version );
	}
	if( header.headerSize < sizeof( can::Header ) || header.headerSize > size )
	{
		return "Invalid header size";
	}
	constexpr size_t crcEnd = offsetof( can::Header, crc32 ) + sizeof( can::Header::crc32 );
	if( ComputeCrc32( bytes + crcEnd, size - crcEnd ) != header.crc32 )
	{
		return "CRC mismatch";
	}
	auto error = AreSpanPointersValid( header, &header, header.headerSize );
	if( !error.empty() )
	{
		return "Invalid header: " + error;
	}
	if( header.sections.empty() || header.sections[0].type != can::SectionType::Network )
	{
		return "The first section must be the Network section";
	}
	size_t previousEnd = header.headerSize;
	bool hasEditorData = false;
	for( const auto& section : header.sections )
	{
		if( section.offset % SECTION_ALIGNMENT != 0 || section.offset < previousEnd || static_cast<size_t>( section.offset ) + section.size > size )
		{
			return "Invalid section bounds";
		}
		if( section.type == can::SectionType::EditorData )
		{
			if( hasEditorData )
			{
				return "Only one EditorData section is allowed";
			}
			hasEditorData = true;
		}
		else if( section.type != can::SectionType::Network && section.type != can::SectionType::EmbeddedCmf )
		{
			return "Invalid section type";
		}
		previousEnd = static_cast<size_t>( section.offset ) + section.size;
	}
	return {};
}

std::string ValidateEmbeddedFiles( const uint8_t* bytes )
{
	const auto& sections = GetHeader( bytes ).sections;
	for( uint32_t i = 1; i < sections.size(); ++i )
	{
		if( sections[i].type == can::SectionType::EditorData )
		{
			// Editor data is opaque to the runtime
			continue;
		}
		if( sections[i].type != can::SectionType::EmbeddedCmf )
		{
			return "Only the first section can be a Network section";
		}
		ValidationOptions options;
		options.validateCrc = true;
		options.validateHeader = true;
		options.validateMainData = true;
		auto result = ValidateFile( bytes + sections[i].offset, sections[i].size, options );
		if( !result )
		{
			return "Embedded CMF file in section " + std::to_string( i ) + " is invalid: " + result.error;
		}
	}
	return {};
}

std::string ValidateAssets( const can::NetworkData& data, const can::Header& header )
{
	for( const auto& asset : data.assets )
	{
		if( asset.section != can::NO_INDEX && ( asset.section >= header.sections.size() || header.sections[asset.section].type != can::SectionType::EmbeddedCmf ) )
		{
			return "Asset '" + ToStdString( asset.path ) + "' references an invalid section";
		}
	}
	if( data.skeletonAsset != can::NO_INDEX && data.skeletonAsset >= data.assets.size() )
	{
		return "Invalid skeleton asset index";
	}
	return {};
}

// Checks that every subgraph is referenced by exactly one Subgraph node and is reachable from the root graph
std::string ValidateGraphTree( const can::NetworkData& data )
{
	if( data.graphs.empty() )
	{
		return "The network has no root graph";
	}
	std::vector<uint32_t> referenceCounts( data.graphs.size(), 0 );
	for( const auto& graph : data.graphs )
	{
		for( const auto& subgraph : graph.subgraphNodes )
		{
			if( subgraph.graphIndex == 0 || subgraph.graphIndex >= data.graphs.size() )
			{
				return "Subgraph node references an invalid graph";
			}
			++referenceCounts[subgraph.graphIndex];
		}
	}
	if( std::any_of( referenceCounts.begin() + 1, referenceCounts.end(), []( uint32_t count ) { return count != 1; } ) )
	{
		return "Every subgraph must be referenced by exactly one Subgraph node";
	}
	std::vector<bool> visited( data.graphs.size(), false );
	std::vector<uint32_t> stack = { 0 };
	while( !stack.empty() )
	{
		const auto index = stack.back();
		stack.pop_back();
		visited[index] = true;
		for( const auto& subgraph : data.graphs[index].subgraphNodes )
		{
			if( !visited[subgraph.graphIndex] )
			{
				stack.push_back( subgraph.graphIndex );
			}
		}
	}
	if( std::find( visited.begin(), visited.end(), false ) != visited.end() )
	{
		return "Subgraphs form a cycle";
	}
	return {};
}

std::string ValidateInterface( const can::Graph& graph, bool root )
{
	if( root && ( !graph.interfaceInputs.empty() || graph.interfaceOutputs.size() > 1 ) )
	{
		return "The root graph must have no SubgraphInput nodes and at most one Output node";
	}
	for( const bool input : { true, false } )
	{
		const auto expectedType = input ? can::NodeType::SubgraphInput : can::NodeType::Output;
		const auto& sockets = input ? graph.interfaceInputs : graph.interfaceOutputs;
		std::set<uint32_t> listed;
		for( const auto index : sockets )
		{
			if( index >= graph.nodes.size() || graph.nodes[index].type != expectedType || !listed.insert( index ).second )
			{
				return "Invalid interface node list";
			}
		}
		const auto count = std::count_if( graph.nodes.begin(), graph.nodes.end(), [expectedType]( const can::Node& node ) { return node.type == expectedType; } );
		if( static_cast<size_t>( count ) != listed.size() )
		{
			return "Interface node list is incomplete";
		}
	}
	if( root && !graph.interfaceOutputs.empty() && GetInterfaceNode( graph, graph.interfaceOutputs[0] ).type != can::SocketType::Pose )
	{
		return "The Output node of the root graph must be a Pose";
	}
	return {};
}

std::string ValidateNode( const can::NetworkData& data, const can::Graph& graph, const can::Node& node )
{
	if( node.type == can::NodeType::Parameter && GetNodeData<can::ParameterNode>( graph, node ).parameterIndex >= data.parameters.size() )
	{
		return "invalid parameter index";
	}
	if( node.type == can::NodeType::AnimationSample )
	{
		const auto assetIndex = GetNodeData<can::AnimationSampleNode>( graph, node ).assetIndex;
		if( assetIndex != can::NO_INDEX && assetIndex >= data.assets.size() )
		{
			return "invalid asset index";
		}
	}
	if( GetNodeTypeInfo( node.type ).layout == SocketLayout::Switch && GetShape( graph, node ).poseInputCount == 0 )
	{
		return "no pose inputs";
	}
	if( node.inputConstants.size() != GetInputCount( data, graph, node ) )
	{
		return "wrong number of input constants";
	}
	return {};
}

std::string ValidateEdges( const can::NetworkData& data, const can::Graph& graph )
{
	const auto nodeCount = static_cast<uint32_t>( graph.nodes.size() );
	std::set<std::pair<uint32_t, uint32_t>> connectedInputs;
	std::vector<uint32_t> incomingEdges( nodeCount, 0 );
	for( const auto& edge : graph.edges )
	{
		if( edge.from.node >= nodeCount || edge.to.node >= nodeCount )
		{
			return "Edge references an invalid node";
		}
		const auto& from = graph.nodes[edge.from.node];
		const auto& to = graph.nodes[edge.to.node];
		if( edge.from.socket >= GetOutputCount( data, graph, from ) || edge.to.socket >= GetInputCount( data, graph, to ) )
		{
			return "Edge references an invalid socket";
		}
		if( GetOutputType( data, graph, from, edge.from.socket ) != GetInputType( data, graph, to, edge.to.socket ) )
		{
			return "Edge connects sockets of different types";
		}
		if( !connectedInputs.insert( { edge.to.node, edge.to.socket } ).second )
		{
			return "Input has more than one incoming edge";
		}
		++incomingEdges[edge.to.node];
	}

	// Kahn's algorithm: all nodes can be sorted topologically if there are no cycles
	std::vector<uint32_t> ready;
	for( uint32_t i = 0; i < nodeCount; ++i )
	{
		if( incomingEdges[i] == 0 )
		{
			ready.push_back( i );
		}
	}
	uint32_t sortedCount = 0;
	while( !ready.empty() )
	{
		const auto node = ready.back();
		ready.pop_back();
		++sortedCount;
		for( const auto& edge : graph.edges )
		{
			if( edge.from.node == node && --incomingEdges[edge.to.node] == 0 )
			{
				ready.push_back( edge.to.node );
			}
		}
	}
	if( sortedCount != nodeCount )
	{
		return "Edges form a cycle";
	}
	return {};
}

std::string LocateError( uint32_t graph, const std::string& error )
{
	return "Graph " + std::to_string( graph ) + ": " + error;
}

std::string LocateError( uint32_t graph, uint32_t node, const std::string& error )
{
	return "Graph " + std::to_string( graph ) + " node " + std::to_string( node ) + ": " + error;
}

std::string ValidateNetworkData( const can::NetworkData& data, const can::Header& header )
{
	auto error = ValidateAssets( data, header );
	for( uint32_t i = 0; i < data.graphs.size() && error.empty(); ++i )
	{
		error = ValidateNodeData( data.graphs[i] );
		if( !error.empty() )
		{
			error = LocateError( i, error );
		}
	}
	if( error.empty() )
	{
		error = ValidateGraphTree( data );
	}
	for( uint32_t i = 0; i < data.graphs.size() && error.empty(); ++i )
	{
		error = ValidateInterface( data.graphs[i], i == 0 );
	}
	for( uint32_t i = 0; i < data.graphs.size() && error.empty(); ++i )
	{
		const auto& graph = data.graphs[i];
		for( uint32_t n = 0; n < graph.nodes.size() && error.empty(); ++n )
		{
			error = ValidateNode( data, graph, graph.nodes[n] );
			if( !error.empty() )
			{
				error = LocateError( i, n, error );
			}
		}
		if( error.empty() )
		{
			error = ValidateEdges( data, graph );
		}
	}
	return error;
}

}

NodeTypeInfo GetNodeTypeInfo( can::NodeType type )
{
	// Every node type needs a case; there is deliberately no default case
	switch( type )
	{
	case can::NodeType::Output: {
		auto info = MakeInfo( "Output", SocketLayout::Interface, OUTPUT_INPUTS, can::SocketType::Pose, "", true );
		info.hasOutput = false;
		return info;
	}
	case can::NodeType::SubgraphInput:
		return MakeInfo( "SubgraphInput", SocketLayout::Interface, can::SocketType::Pose, "", true );
	case can::NodeType::Parameter:
		return MakeInfo( "Parameter", SocketLayout::Fixed, can::SocketType::Float, "value", true );
	case can::NodeType::AnimationSample:
		return MakeInfo( "AnimationSample", SocketLayout::Fixed, SAMPLE_INPUTS, can::SocketType::Pose, "pose", true );
	case can::NodeType::RestPose:
		return MakeInfo( "RestPose", SocketLayout::Fixed, can::SocketType::Pose, "pose", false );
	case can::NodeType::Blend:
		return MakeInfo( "Blend", SocketLayout::Fixed, BLEND_INPUTS, can::SocketType::Pose, "pose", false );
	case can::NodeType::Switch:
		return MakeInfo( "Switch", SocketLayout::Switch, SWITCH_INPUTS, can::SocketType::Pose, "pose", true );
	case can::NodeType::RandomSwitch:
		return MakeInfo( "RandomSwitch", SocketLayout::Switch, can::SocketType::Pose, "pose", true );
	case can::NodeType::AdditiveBlend:
		return MakeInfo( "AdditiveBlend", SocketLayout::Fixed, ADDITIVE_INPUTS, can::SocketType::Pose, "pose", false );
	case can::NodeType::NamedBoneWeights:
		return MakeInfo( "NamedBoneWeights", SocketLayout::Fixed, can::SocketType::BoneWeights, "weights", true );
	case can::NodeType::BoneWeights:
		return MakeInfo( "BoneWeights", SocketLayout::Fixed, can::SocketType::BoneWeights, "weights", true );
	case can::NodeType::Subgraph:
		return MakeInfo( "Subgraph", SocketLayout::Subgraph, can::SocketType::Pose, "", true );
	case can::NodeType::Expression:
		return MakeInfo( "Expression", SocketLayout::Expression, can::SocketType::Float, "value", true );
	case can::NodeType::Count:
		break;
	}
	return MakeInfo( "Unknown", SocketLayout::Fixed, can::SocketType::Pose, "", false );
}

const char* GetNodeTypeName( can::NodeType type )
{
	return GetNodeTypeInfo( type ).name;
}

bool IsValidNodeType( can::NodeType type )
{
	return static_cast<uint8_t>( type ) < static_cast<uint8_t>( can::NodeType::Count );
}

bool IsValidSocketType( can::SocketType type )
{
	return static_cast<uint8_t>( type ) <= static_cast<uint8_t>( can::SocketType::Float );
}

uint32_t GetFirstSwitchPoseInput( can::NodeType type )
{
	return GetNodeTypeInfo( type ).inputCount;
}

uint32_t GetInputCount( const NodeShape& shape )
{
	const auto info = GetNodeTypeInfo( shape.type );
	switch( info.layout )
	{
	case SocketLayout::Fixed:
	case SocketLayout::Interface:
		return info.inputCount;
	case SocketLayout::Switch:
		return info.inputCount + shape.poseInputCount;
	case SocketLayout::Expression:
		return shape.expressionInputCount;
	case SocketLayout::Subgraph:
		return 0;
	}
	return 0;
}

uint32_t GetOutputCount( can::NodeType type )
{
	return GetNodeTypeInfo( type ).hasOutput ? 1 : 0;
}

can::SocketType GetInputType( const NodeShape& shape, uint32_t socket )
{
	const auto info = GetNodeTypeInfo( shape.type );
	switch( info.layout )
	{
	case SocketLayout::Fixed:
		return info.inputs[socket].type;
	case SocketLayout::Interface:
		return shape.interfaceType;
	case SocketLayout::Switch:
		return socket < info.inputCount ? info.inputs[socket].type : can::SocketType::Pose;
	case SocketLayout::Expression:
	case SocketLayout::Subgraph:
		return can::SocketType::Float;
	}
	return can::SocketType::Float;
}

can::SocketType GetOutputType( const NodeShape& shape )
{
	const auto info = GetNodeTypeInfo( shape.type );
	return info.layout == SocketLayout::Interface ? shape.interfaceType : info.outputType;
}

std::string GetInputName( const NodeShape& shape, uint32_t socket )
{
	const auto info = GetNodeTypeInfo( shape.type );
	switch( info.layout )
	{
	case SocketLayout::Fixed:
	case SocketLayout::Interface:
		return info.inputs[socket].name;
	case SocketLayout::Switch:
		return socket < info.inputCount ? info.inputs[socket].name : "pose" + std::to_string( socket - info.inputCount );
	case SocketLayout::Expression:
	case SocketLayout::Subgraph:
		return {};
	}
	return {};
}

std::string GetOutputName( can::NodeType type )
{
	return GetNodeTypeInfo( type ).outputName;
}

float GetDefaultInputConstant( can::NodeType type, uint32_t socket )
{
	const auto info = GetNodeTypeInfo( type );
	const bool listed = info.layout != SocketLayout::Interface && socket < info.inputCount;
	return listed ? info.inputs[socket].defaultValue : 0.f;
}

bool HasNodeData( can::NodeType type )
{
	return GetNodeTypeInfo( type ).hasData;
}

size_t GetNodeTableSize( const can::Graph& graph, can::NodeType type )
{
	// Every node type needs a case; there is deliberately no default case
	switch( type )
	{
	case can::NodeType::Output:
	case can::NodeType::SubgraphInput:
		return graph.interfaceNodes.size();
	case can::NodeType::Parameter:
		return graph.parameterNodes.size();
	case can::NodeType::AnimationSample:
		return graph.animationSampleNodes.size();
	case can::NodeType::Switch:
		return graph.switchNodes.size();
	case can::NodeType::RandomSwitch:
		return graph.randomSwitchNodes.size();
	case can::NodeType::NamedBoneWeights:
		return graph.namedBoneWeightsNodes.size();
	case can::NodeType::BoneWeights:
		return graph.boneWeightsNodes.size();
	case can::NodeType::Subgraph:
		return graph.subgraphNodes.size();
	case can::NodeType::Expression:
		return graph.expressionNodes.size();
	case can::NodeType::RestPose:
	case can::NodeType::Blend:
	case can::NodeType::AdditiveBlend:
	case can::NodeType::Count:
		return 0;
	}
	return 0;
}

NodeShape GetShape( const can::Graph& graph, const can::Node& node )
{
	NodeShape shape;
	shape.type = node.type;
	switch( GetNodeTypeInfo( node.type ).layout )
	{
	case SocketLayout::Interface:
		shape.interfaceType = GetNodeData<can::InterfaceNode>( graph, node ).type;
		break;
	case SocketLayout::Switch:
		shape.poseInputCount = node.type == can::NodeType::Switch ? GetNodeData<can::SwitchNode>( graph, node ).poseInputCount : GetNodeData<can::RandomSwitchNode>( graph, node ).poseInputCount;
		break;
	case SocketLayout::Expression:
		shape.expressionInputCount = static_cast<uint32_t>( GetNodeData<can::ExpressionNode>( graph, node ).inputNames.size() );
		break;
	case SocketLayout::Fixed:
	case SocketLayout::Subgraph:
		break;
	}
	return shape;
}

const can::Graph& GetSubgraph( const can::NetworkData& data, const can::Graph& graph, const can::Node& node )
{
	return data.graphs[GetNodeData<can::SubgraphNode>( graph, node ).graphIndex];
}

uint32_t GetInputCount( const can::NetworkData& data, const can::Graph& graph, const can::Node& node )
{
	if( node.type == can::NodeType::Subgraph )
	{
		return static_cast<uint32_t>( GetSubgraph( data, graph, node ).interfaceInputs.size() );
	}
	return GetInputCount( GetShape( graph, node ) );
}

uint32_t GetOutputCount( const can::NetworkData& data, const can::Graph& graph, const can::Node& node )
{
	if( node.type == can::NodeType::Subgraph )
	{
		return static_cast<uint32_t>( GetSubgraph( data, graph, node ).interfaceOutputs.size() );
	}
	return GetOutputCount( node.type );
}

can::SocketType GetInputType( const can::NetworkData& data, const can::Graph& graph, const can::Node& node, uint32_t socket )
{
	if( node.type == can::NodeType::Subgraph )
	{
		const auto& subgraph = GetSubgraph( data, graph, node );
		return GetInterfaceNode( subgraph, subgraph.interfaceInputs[socket] ).type;
	}
	return GetInputType( GetShape( graph, node ), socket );
}

can::SocketType GetOutputType( const can::NetworkData& data, const can::Graph& graph, const can::Node& node, uint32_t socket )
{
	if( node.type == can::NodeType::Subgraph )
	{
		const auto& subgraph = GetSubgraph( data, graph, node );
		return GetInterfaceNode( subgraph, subgraph.interfaceOutputs[socket] ).type;
	}
	return GetOutputType( GetShape( graph, node ) );
}

std::string GetInputName( const can::NetworkData& data, const can::Graph& graph, const can::Node& node, uint32_t socket )
{
	switch( GetNodeTypeInfo( node.type ).layout )
	{
	case SocketLayout::Subgraph: {
		const auto& subgraph = GetSubgraph( data, graph, node );
		return ToStdString( GetInterfaceNode( subgraph, subgraph.interfaceInputs[socket] ).name );
	}
	case SocketLayout::Expression:
		return ToStdString( GetNodeData<can::ExpressionNode>( graph, node ).inputNames[socket] );
	case SocketLayout::Interface: {
		const auto& name = GetNodeData<can::InterfaceNode>( graph, node ).name;
		return name.empty() ? GetInputName( GetShape( graph, node ), socket ) : ToStdString( name );
	}
	case SocketLayout::Fixed:
	case SocketLayout::Switch:
		return GetInputName( GetShape( graph, node ), socket );
	}
	return {};
}

ValidationResult ValidateCanFile( const void* data, size_t size )
{
	const auto* bytes = static_cast<const uint8_t*>( data );
	auto error = ValidateHeader( bytes, size );
	if( error.empty() )
	{
		error = ValidateEmbeddedFiles( bytes );
	}
	if( error.empty() )
	{
		const auto& section = GetHeader( bytes ).sections[0];
		const auto& networkData = GetNetworkData( bytes );
		error = AreSpanPointersValid( networkData, &networkData, section.size );
		if( error.empty() )
		{
			error = ValidateNetworkData( networkData, GetHeader( bytes ) );
		}
		else
		{
			error = "Invalid network data: " + error;
		}
	}
	return { error.empty(), error };
}

const can::NetworkData& GetNetworkData( const void* file )
{
	const auto* bytes = static_cast<const uint8_t*>( file );
	return *reinterpret_cast<const can::NetworkData*>( bytes + GetHeader( file ).sections[0].offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the section holds a NetworkData structure
}

const cmf::Data& GetEmbeddedCmfData( const void* file, uint32_t section )
{
	const auto* cmfFile = GetSectionBytes( file, section ).first;
	const auto& cmfHeader = *reinterpret_cast<const cmf::Header*>( cmfFile ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the section holds a validated CMF file
	return *reinterpret_cast<const cmf::Data*>( cmfFile + cmfHeader.sections[0].offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): section 0 of a CMF file holds its Data
}

const cmf::Skeleton* FindSkeleton( const cmf::Data& data, std::string_view name )
{
	const auto* it = std::find_if( data.skeletons.begin(), data.skeletons.end(), [name]( const cmf::Skeleton& skeleton ) { return name.empty() || ToStdStringView( skeleton.name ) == name; } );
	return it == data.skeletons.end() ? nullptr : &*it;
}

const cmf::Animation* FindAnimation( const cmf::Data& data, std::string_view name )
{
	const auto* it = std::find_if( data.animations.begin(), data.animations.end(), [name]( const cmf::Animation& animation ) { return name.empty() || ToStdStringView( animation.name ) == name; } );
	return it == data.animations.end() ? nullptr : &*it;
}

uint32_t FindBone( const cmf::Skeleton& skeleton, std::string_view name )
{
	const auto* it = std::find_if( skeleton.bones.begin(), skeleton.bones.end(), [name]( const cmf::String& bone ) { return ToStdStringView( bone ) == name; } );
	return it == skeleton.bones.end() ? can::NO_INDEX : static_cast<uint32_t>( it - skeleton.bones.begin() );
}

const cmf::BoneMask* FindBoneMask( const cmf::Skeleton& skeleton, std::string_view name )
{
	const auto* it = std::find_if( skeleton.boneMasks.begin(), skeleton.boneMasks.end(), [name]( const cmf::BoneMask& mask ) { return ToStdStringView( mask.name ) == name; } );
	return it == skeleton.boneMasks.end() ? nullptr : &*it;
}

std::pair<const uint8_t*, size_t> GetSectionBytes( const void* file, uint32_t section )
{
	const auto& entry = GetHeader( file ).sections[section];
	return { static_cast<const uint8_t*>( file ) + entry.offset, entry.size };
}

uint32_t FindSection( const void* file, can::SectionType type )
{
	const auto& sections = GetHeader( file ).sections;
	for( uint32_t i = 0; i < sections.size(); ++i )
	{
		if( sections[i].type == type )
		{
			return i;
		}
	}
	return can::NO_INDEX;
}
}
