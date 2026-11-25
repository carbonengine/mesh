// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/can.h"
#include "cmf/cmf.h"
#include "cmf/utils.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Internal helpers describing the animation network node socket layouts and reading/validating CAN files. Shared by
// AnimationNetworkBuilder, AnimationNetwork and the network compiler.

namespace cmf::detail
{

/** Socket layout parameters of a node that don't depend on the graph. */
struct NodeShape
{
	can::NodeType type = can::NodeType::RestPose;
	/** Declared type of SubgraphInput and Output sockets. */
	can::SocketType interfaceType = can::SocketType::Pose;
	/** Number of pose inputs of Switch and RandomSwitch nodes. */
	uint32_t poseInputCount = 0;
	/** Number of inputs of Expression nodes. */
	uint32_t expressionInputCount = 0;
};

constexpr uint32_t SAMPLE_TIME_INPUT = 0;
constexpr uint32_t SAMPLE_LOOP_INPUT = 1;
constexpr uint32_t SAMPLE_SPEED_INPUT = 2;
constexpr uint32_t BLEND_POSE_A_INPUT = 0;
constexpr uint32_t BLEND_POSE_B_INPUT = 1;
constexpr uint32_t BLEND_ALPHA_INPUT = 2;
constexpr uint32_t BLEND_WEIGHTS_INPUT = 3;
constexpr uint32_t SWITCH_SELECTOR_INPUT = 0;
constexpr uint32_t ADDITIVE_POSE_INPUT = 0;
constexpr uint32_t ADDITIVE_BASE_INPUT = 1;
constexpr uint32_t ADDITIVE_ADDITIVE_INPUT = 2;
constexpr uint32_t ADDITIVE_ALPHA_INPUT = 3;
constexpr uint32_t ADDITIVE_WEIGHTS_INPUT = 4;

/** How the sockets of a node type are defined. */
enum class SocketLayout : uint8_t
{
	/** The inputs and the output are listed in NodeTypeInfo. */
	Fixed,
	/** SubgraphInput and Output: like Fixed, but the socket type is declared by the node. */
	Interface,
	/** Switch and RandomSwitch: the listed inputs followed by a configurable number of pose inputs. */
	Switch,
	/** Expression: one Float input per expression input name. */
	Expression,
	/** Subgraph: the sockets are the interface of the contained graph. */
	Subgraph,
};

/** How the value of a Float socket is interpreted (mirrors AnimationValueKind). Not stored in files. */
enum class ValueKind : uint8_t
{
	Number,
	Boolean,
	Integer,
};

/** Description of an input socket with a fixed name and type. */
struct SocketInfo
{
	const char* name = "";
	can::SocketType type = can::SocketType::Float;
	/** Constant of the input in a newly created node. */
	float defaultValue = 0.f;
	/** Kind of value of a Float input. */
	ValueKind valueKind = ValueKind::Number;
};

/** Description of a node type: its name, sockets and data. All socket queries are derived from it. */
struct NodeTypeInfo
{
	const char* name = "";
	SocketLayout layout = SocketLayout::Fixed;
	/** Fixed inputs; for the Switch layout, the inputs preceding the pose inputs. */
	const SocketInfo* inputs = nullptr;
	uint32_t inputCount = 0;
	/** The node has an output socket (all except Output; Subgraph sockets come from its graph). */
	bool hasOutput = true;
	can::SocketType outputType = can::SocketType::Pose;
	const char* outputName = "pose";
	/** The node has type specific data in a table of the graph. */
	bool hasData = false;
};

/** Returns the description of a node type. Not valid for NodeType::Count. */
NodeTypeInfo GetNodeTypeInfo( can::NodeType type );

/** Returns the name of a node type. */
const char* GetNodeTypeName( can::NodeType type );

/** Returns true if the value is a valid NodeType. */
bool IsValidNodeType( can::NodeType type );

/** Returns true if the value is a valid SocketType. */
bool IsValidSocketType( can::SocketType type );

/** Returns the index of the first pose input of Switch and RandomSwitch nodes. */
uint32_t GetFirstSwitchPoseInput( can::NodeType type );

/** Returns the number of inputs of a node. Not valid for Subgraph nodes, whose sockets come from their graph. */
uint32_t GetInputCount( const NodeShape& shape );

/** Returns the number of outputs of a node. Not valid for Subgraph nodes. */
uint32_t GetOutputCount( can::NodeType type );

/** Returns the type of an input socket. Not valid for Subgraph nodes. */
can::SocketType GetInputType( const NodeShape& shape, uint32_t socket );

/** Returns the type of an output socket. Not valid for Subgraph nodes. */
can::SocketType GetOutputType( const NodeShape& shape );

/** Returns the fixed name of an input socket, or an empty string for sockets named by the node (Expression inputs,
* Output nodes in subgraphs). Not valid for Subgraph nodes. */
std::string GetInputName( const NodeShape& shape, uint32_t socket );

/** Returns the fixed name of the output socket, or an empty string for SubgraphInput nodes. */
std::string GetOutputName( can::NodeType type );

/** Returns the default constant of an input of a newly created node. */
float GetDefaultInputConstant( can::NodeType type, uint32_t socket );

/** Returns the kind of value of an input socket: listed in the socket table for fixed inputs, Number for the rest. */
ValueKind GetInputValueKind( const NodeShape& shape, uint32_t socket );

/** Returns true if nodes of the type have data in a per-type table of the graph. */
bool HasNodeData( can::NodeType type );

/** Returns the size of the graph table holding the data of nodes of the given type. */
size_t GetNodeTableSize( const can::Graph& graph, can::NodeType type );

/** Returns the graph table holding node data of type T. */
template <typename T>
const Span<T>& GetNodeTable( const can::Graph& graph ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	if constexpr( std::is_same_v<T, can::InterfaceNode> )
	{
		return graph.interfaceNodes;
	}
	else if constexpr( std::is_same_v<T, can::ParameterNode> )
	{
		return graph.parameterNodes;
	}
	else if constexpr( std::is_same_v<T, can::AnimationSampleNode> )
	{
		return graph.animationSampleNodes;
	}
	else if constexpr( std::is_same_v<T, can::SwitchNode> )
	{
		return graph.switchNodes;
	}
	else if constexpr( std::is_same_v<T, can::RandomSwitchNode> )
	{
		return graph.randomSwitchNodes;
	}
	else if constexpr( std::is_same_v<T, can::NamedBoneWeightsNode> )
	{
		return graph.namedBoneWeightsNodes;
	}
	else if constexpr( std::is_same_v<T, can::BoneWeightsNode> )
	{
		return graph.boneWeightsNodes;
	}
	else if constexpr( std::is_same_v<T, can::SubgraphNode> )
	{
		return graph.subgraphNodes;
	}
	else
	{
		static_assert( std::is_same_v<T, can::ExpressionNode>, "Unknown node data type" );
		return graph.expressionNodes;
	}
}

/** Returns the type specific data of a node in a validated graph. */
template <typename T>
const T& GetNodeData( const can::Graph& graph, const can::Node& node ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	return GetNodeTable<T>( graph )[node.dataIndex];
}

/** Returns the shape of a node stored in a CAN file. */
NodeShape GetShape( const can::Graph& graph, const can::Node& node );

/** Returns the graph contained in a Subgraph node of a validated graph. */
const can::Graph& GetSubgraph( const can::NetworkData& data, const can::Graph& graph, const can::Node& node );

/** Socket queries on nodes stored in a CAN file, including Subgraph nodes. The data must be valid. */
uint32_t GetInputCount( const can::NetworkData& data, const can::Graph& graph, const can::Node& node );
uint32_t GetOutputCount( const can::NetworkData& data, const can::Graph& graph, const can::Node& node );
can::SocketType GetInputType( const can::NetworkData& data, const can::Graph& graph, const can::Node& node, uint32_t socket );
can::SocketType GetOutputType( const can::NetworkData& data, const can::Graph& graph, const can::Node& node, uint32_t socket );
std::string GetInputName( const can::NetworkData& data, const can::Graph& graph, const can::Node& node, uint32_t socket );

/** Checks that a CAN file is well formed: header, sections, CRC, span bounds, embedded CMF files, all indices,
* socket types, one edge per input and no cycles. Networks that are well formed may still be incomplete (for example,
* have unconnected inputs); those problems are reported by AnimationNetwork::GetErrors. */
ValidationResult ValidateCanFile( const void* data, size_t size );

/** Returns the network data of a validated CAN file. */
const can::NetworkData& GetNetworkData( const void* file );

/** Returns the CMF data embedded in a section of a validated CAN file. */
const cmf::Data& GetEmbeddedCmfData( const void* file, uint32_t section );

/** Returns the skeleton with the given name, or the first skeleton if the name is empty. */
const cmf::Skeleton* FindSkeleton( const cmf::Data& data, std::string_view name );

/** Returns the animation with the given name, or the first animation if the name is empty. */
const cmf::Animation* FindAnimation( const cmf::Data& data, std::string_view name );

/** Returns the index of the bone with the given name, or can::NO_INDEX. */
uint32_t FindBone( const cmf::Skeleton& skeleton, std::string_view name );

/** Returns the bone mask with the given name, or nullptr. */
const cmf::BoneMask* FindBoneMask( const cmf::Skeleton& skeleton, std::string_view name );

/** Returns the bytes of a section of a validated CAN file. */
std::pair<const uint8_t*, size_t> GetSectionBytes( const void* file, uint32_t section );
}
