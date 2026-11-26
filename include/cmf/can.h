// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf.h"
#include "span.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

/** @file
* Data structures of the CAN (Carbon Animation Network) binary file format.
*
* A CAN file consists of a Header and a Network section holding a NetworkData structure. Skeletons and animations
* embedded in the network are stored in NetworkData as CMF structures. Like CMF, all structures are stored as-is, with Span members encoded as self-relative offsets
* (see PointersToOffsets), so the data can be used directly from the loaded file buffer.
*/

namespace cmf::can
{

/** File signature: the bytes "canf". */
constexpr uint32_t FILE_SIGNATURE = 0x666e6163;

namespace v1
{

constexpr uint32_t FILE_VERSION = 1;

/** Value of index fields that don't reference anything. */
constexpr uint32_t NO_INDEX = 0xffffffff;

/** @brief Type of data that flows through an animation network edge. */
enum class SocketType : uint8_t
{
	/** Skeleton pose: one local transform per skeleton bone. */
	Pose,
	/** Per-bone blend weights: one weight per skeleton bone. */
	BoneWeights,
	/** Single floating point value. */
	Float,
};

/** @brief Type of an animation network node.
*
* Input and output sockets of each node type (in socket index order):
*
* | Type             | Inputs                                                            | Outputs                              |
* |------------------|-------------------------------------------------------------------|--------------------------------------|
* | Output           | pose (Pose; declared type in subgraphs)                           | -                                    |
* | SubgraphInput    | -                                                                 | value (declared type)                |
* | Parameter        | -                                                                 | value (Float)                        |
* | AnimationSample  | time (Float), loop (Float), speed (Float)                         | pose (Pose)                          |
* | RestPose         | -                                                                 | pose (Pose)                          |
* | Blend            | poseA (Pose), poseB (Pose), alpha (Float), weights (BoneWeights)  | pose (Pose)                          |
* | Switch           | selector (Float), pose0..poseN-1 (Pose)                           | pose (Pose)                          |
* | RandomSwitch     | pose0..poseN-1 (Pose)                                             | pose (Pose)                          |
* | AdditiveBlend    | pose, base, additive (Pose), alpha (Float), weights (BoneWeights) | pose (Pose)                          |
* | NamedBoneWeights | -                                                                 | weights (BoneWeights)                |
* | BoneWeights      | -                                                                 | weights (BoneWeights)                |
* | Subgraph         | one per SubgraphInput node of the subgraph                        | one per Output node of the subgraph  |
* | Expression       | one Float input per expression input name                         | value (Float)                        |
* | StateMachine     | -                                                                 | pose (Pose)                          |
*
* When adding a node type, the compiler reports every switch statement that needs a new case: switch statements
* over node types have no default case, and the library is built with missing enum cases reported as errors
* (MSVC C4062, clang/GCC -Wswitch).
*
* Pose inputs must be connected. Float inputs that are not connected use a per-input constant value. BoneWeights
* inputs are optional: when not connected, the blend applies to all bones uniformly. The time input of
* AnimationSample uses the node's playhead (advanced by speed) unless an edge is connected to it.
*/
enum class NodeType : uint8_t
{
	Output,
	SubgraphInput,
	Parameter,
	AnimationSample,
	RestPose,
	Blend,
	Switch,
	RandomSwitch,
	AdditiveBlend,
	NamedBoneWeights,
	BoneWeights,
	Subgraph,
	Expression,
	StateMachine,
	/** Number of node types; not a valid node type. */
	Count,
};

// NOLINTBEGIN(readability-identifier-naming,cppcoreguidelines-missing-std-forward): TypeName and EnumerateMembers follow the CMF reflection pattern used by cmf::EnumerateMembers

/** @brief Reference to an input or output socket of a node in the same graph. */
struct SocketRef
{
	/** Index of the node in Graph::nodes. */
	uint32_t node = NO_INDEX;
	/** Socket index within the node's inputs or outputs. */
	uint32_t socket = 0;

	static constexpr std::string_view TypeName = "SocketRef";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, node, "node" );
		visitor( *this, socket, "socket" );
	}
};

/** @brief Directed edge from an output socket to an input socket. */
struct Edge
{
	SocketRef from;
	SocketRef to;

	static constexpr std::string_view TypeName = "Edge";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, from, "from" );
		visitor( *this, to, "to" );
	}
};

/** @brief Weight of a single bone, referenced by name. */
struct NamedBoneWeight
{
	String bone;
	float weight = 1.f;
	/** Reserved, always 0. Keeps the structure free of implicit padding. */
	uint32_t reserved = 0;

	static constexpr std::string_view TypeName = "NamedBoneWeight";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, bone, "bone" );
		visitor( *this, weight, "weight" );
	}
};

/** @brief Animation network node: the data common to all node types.
*
* Node type specific data is stored in the per-type tables of the Graph (see Graph::animationSamples and the other
* tables) and referenced by dataIndex. Node types without specific data (RestPose, Blend, AdditiveBlend) have
* dataIndex set to NO_INDEX.
*/
struct Node
{
	NodeType type = NodeType::RestPose;
	/** Reserved, always 0. Keeps the structure free of implicit padding. */
	uint8_t reserved0 = 0;
	uint8_t reserved1 = 0;
	uint8_t reserved2 = 0;
	/** Index of the node data in the Graph table for the node type, or NO_INDEX. */
	uint32_t dataIndex = NO_INDEX;
	/** Optional display name. */
	String name;
	/** Constant values of the node inputs, used for Float inputs without an incoming edge. One value per input. */
	Span<float> inputConstants;

	static constexpr std::string_view TypeName = "Node";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, type, "type" );
		visitor( *this, dataIndex, "dataIndex" );
		visitor( *this, name, "name" );
		visitor( *this, inputConstants, "inputConstants" );
	}
};

/** @brief Data of SubgraphInput and Output nodes: a socket of the graph interface. */
struct InterfaceNode
{
	/** Socket type. The Output node of the root graph is always a Pose. */
	SocketType type = SocketType::Pose;
	/** Reserved, always 0. */
	uint8_t reserved0 = 0;
	uint8_t reserved1 = 0;
	uint8_t reserved2 = 0;
	uint32_t reserved3 = 0;
	/** Socket name. */
	String name;

	static constexpr std::string_view TypeName = "InterfaceNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, type, "type" );
		visitor( *this, name, "name" );
	}
};

/** @brief Data of Parameter nodes. */
struct ParameterNode
{
	/** Index in NetworkData::parameters. */
	uint32_t parameterIndex = NO_INDEX;

	static constexpr std::string_view TypeName = "ParameterNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, parameterIndex, "parameterIndex" );
	}
};

/** @brief Data of AnimationSample nodes. */
struct AnimationSampleNode
{
	/** Index in NetworkData::assets of the external CMF file containing the animation, or NO_INDEX. */
	uint32_t assetIndex = NO_INDEX;
	/** Index in NetworkData::animations of the embedded animation, or NO_INDEX. At most one of assetIndex and
	* embeddedAnimation is set; if neither is, the node has no animation. */
	uint32_t embeddedAnimation = NO_INDEX;
	/** Animation name in the external file; empty selects the first animation. For embedded animations, the name the
	* animation was referenced by, kept for diagnostics. */
	String animationName;

	static constexpr std::string_view TypeName = "AnimationSampleNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, assetIndex, "assetIndex" );
		visitor( *this, embeddedAnimation, "embeddedAnimation" );
		visitor( *this, animationName, "animationName" );
	}
};

/** @brief Data of Switch nodes. */
struct SwitchNode
{
	/** Number of pose inputs. */
	uint32_t poseInputCount = 0;

	static constexpr std::string_view TypeName = "SwitchNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, poseInputCount, "poseInputCount" );
	}
};

/** RandomSwitchNode flag: pick a new input when the node becomes active after being inactive. */
constexpr uint32_t RANDOM_SWITCH_REROLL_ON_ACTIVATE = 1;

/** @brief Data of RandomSwitch nodes. */
struct RandomSwitchNode
{
	/** Number of pose inputs. */
	uint32_t poseInputCount = 0;
	/** Combination of RANDOM_SWITCH_* flags. */
	uint32_t flags = 0;
	/** Relative probability of each pose input. Empty means uniform. */
	Span<float> weights;

	static constexpr std::string_view TypeName = "RandomSwitchNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, poseInputCount, "poseInputCount" );
		visitor( *this, flags, "flags" );
		visitor( *this, weights, "weights" );
	}
};

/** @brief Data of NamedBoneWeights nodes. */
struct NamedBoneWeightsNode
{
	/** Name of the skeleton bone mask. */
	String maskName;

	static constexpr std::string_view TypeName = "NamedBoneWeightsNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, maskName, "maskName" );
	}
};

/** @brief Data of BoneWeights nodes. */
struct BoneWeightsNode
{
	/** Weight of bones not listed in weights. */
	float defaultWeight = 0.f;
	/** Reserved, always 0. */
	uint32_t reserved = 0;
	/** Weights of individual bones. */
	Span<NamedBoneWeight> weights;

	static constexpr std::string_view TypeName = "BoneWeightsNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, defaultWeight, "defaultWeight" );
		visitor( *this, weights, "weights" );
	}
};

/** @brief Data of Subgraph nodes. */
struct SubgraphNode
{
	/** Index in NetworkData::graphs of the contained graph. */
	uint32_t graphIndex = NO_INDEX;

	static constexpr std::string_view TypeName = "SubgraphNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, graphIndex, "graphIndex" );
	}
};

/** @brief Data of Expression nodes. */
struct ExpressionNode
{
	/** Expression source text. */
	String expression;
	/** Input variable names; each one is a Float input socket of the node. */
	Span<String> inputNames;

	static constexpr std::string_view TypeName = "ExpressionNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, expression, "expression" );
		visitor( *this, inputNames, "inputNames" );
	}
};

/** @brief Kind of condition that activates a state machine transition. */
enum class TransitionCondition : uint8_t
{
	/** A numerical expression over the network parameters and stateTime; satisfied when it is not zero. */
	Expression,
	/** Satisfied when the animation of the source state finishes. States whose Output is not fed directly by a
	* non-looping AnimationSample never finish. */
	AnimationFinished,
	/** Satisfied when the state machine has been in the source state for at least the timeout. */
	Timeout,
};

/** @brief A state of a StateMachine node. */
struct StateMachineState
{
	/** Index in NetworkData::graphs of the state graph. State graphs have one Pose Output and no SubgraphInput nodes. */
	uint32_t graphIndex = NO_INDEX;
	/** Reserved, always 0. Keeps the structure free of implicit padding. */
	uint32_t reserved = 0;
	/** State name. */
	String name;

	static constexpr std::string_view TypeName = "StateMachineState";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, graphIndex, "graphIndex" );
		visitor( *this, name, "name" );
	}
};

/** @brief A transition between two states of a StateMachine node. */
struct StateMachineTransition
{
	/** Index of the source state in StateMachineNode::states. */
	uint32_t fromState = NO_INDEX;
	/** Index of the destination state in StateMachineNode::states. */
	uint32_t toState = NO_INDEX;
	TransitionCondition condition = TransitionCondition::Expression;
	/** Reserved, always 0. */
	uint8_t reserved0 = 0;
	uint8_t reserved1 = 0;
	uint8_t reserved2 = 0;
	/** Time in seconds over which the source state fades out; 0 switches immediately. */
	float duration = 0.f;
	/** Timeout condition: time in seconds after which the transition is taken. */
	float timeout = 0.f;
	/** Reserved, always 0. */
	uint32_t reserved3 = 0;
	/** Expression condition: expression source text. */
	String expression;

	static constexpr std::string_view TypeName = "StateMachineTransition";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, fromState, "fromState" );
		visitor( *this, toState, "toState" );
		visitor( *this, condition, "condition" );
		visitor( *this, duration, "duration" );
		visitor( *this, timeout, "timeout" );
		visitor( *this, expression, "expression" );
	}
};

/** StateMachineNode flag: resume the previous state instead of restarting from the start state when the node becomes
* active after being inactive. */
constexpr uint32_t STATE_MACHINE_RESUME_ON_ACTIVATE = 1;

/** @brief Data of StateMachine nodes. */
struct StateMachineNode
{
	/** Index in states of the state the machine starts in. */
	uint32_t startState = 0;
	/** Combination of STATE_MACHINE_* flags. */
	uint32_t flags = 0;
	Span<StateMachineState> states;
	Span<StateMachineTransition> transitions;

	static constexpr std::string_view TypeName = "StateMachineNode";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, startState, "startState" );
		visitor( *this, flags, "flags" );
		visitor( *this, states, "states" );
		visitor( *this, transitions, "transitions" );
	}
};

/** @brief A graph of nodes: the root graph, the contents of a Subgraph node or a state of a StateMachine node. */
struct Graph
{
	Span<Node> nodes;
	Span<Edge> edges;
	/** Indices of the SubgraphInput nodes, in interface socket order. */
	Span<uint32_t> interfaceInputs;
	/** Indices of the Output nodes, in interface socket order. The root graph has at most one. */
	Span<uint32_t> interfaceOutputs;

	/** Data of SubgraphInput and Output nodes. */
	Span<InterfaceNode> interfaceNodes;
	Span<ParameterNode> parameterNodes;
	Span<AnimationSampleNode> animationSampleNodes;
	Span<SwitchNode> switchNodes;
	Span<RandomSwitchNode> randomSwitchNodes;
	Span<NamedBoneWeightsNode> namedBoneWeightsNodes;
	Span<BoneWeightsNode> boneWeightsNodes;
	Span<SubgraphNode> subgraphNodes;
	Span<ExpressionNode> expressionNodes;
	Span<StateMachineNode> stateMachineNodes;

	static constexpr std::string_view TypeName = "Graph";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, nodes, "nodes" );
		visitor( *this, edges, "edges" );
		visitor( *this, interfaceInputs, "interfaceInputs" );
		visitor( *this, interfaceOutputs, "interfaceOutputs" );
		visitor( *this, interfaceNodes, "interfaceNodes" );
		visitor( *this, parameterNodes, "parameterNodes" );
		visitor( *this, animationSampleNodes, "animationSampleNodes" );
		visitor( *this, switchNodes, "switchNodes" );
		visitor( *this, randomSwitchNodes, "randomSwitchNodes" );
		visitor( *this, namedBoneWeightsNodes, "namedBoneWeightsNodes" );
		visitor( *this, boneWeightsNodes, "boneWeightsNodes" );
		visitor( *this, subgraphNodes, "subgraphNodes" );
		visitor( *this, expressionNodes, "expressionNodes" );
		visitor( *this, stateMachineNodes, "stateMachineNodes" );
	}
};

/** @brief Control parameter of an animation network. */
struct Parameter
{
	String name;
	float defaultValue = 0.f;
	float minValue = 0.f;
	float maxValue = 0.f;
	/** Reserved, always 0. Keeps the structure free of implicit padding. */
	uint32_t reserved = 0;

	static constexpr std::string_view TypeName = "Parameter";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, defaultValue, "defaultValue" );
		visitor( *this, minValue, "minValue" );
		visitor( *this, maxValue, "maxValue" );
	}
};

/** @brief External CMF file referenced by the network, loaded through the asset loader. */
struct Asset
{
	/** Asset path, resolved by the asset loader. */
	String path;

	static constexpr std::string_view TypeName = "Asset";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, path, "path" );
	}
};

/** @brief Root structure of the Network section. */
struct NetworkData
{
	/** Index in assets of the external CMF file containing the skeleton, or NO_INDEX. At most one of skeletonAsset and
	* skeleton is set. */
	uint32_t skeletonAsset = NO_INDEX;
	/** Reserved, always 0. Keeps the structure free of implicit padding. */
	uint32_t reserved = 0;
	/** Skeleton name in the external file; empty selects the first skeleton. For an embedded skeleton, the name it was
	* referenced by, kept for diagnostics. */
	String skeletonName;
	/** External CMF files referenced by the network; each path appears once. */
	Span<Asset> assets;
	/** Embedded skeleton: empty, or a single skeleton. */
	Span<cmf::v1::Skeleton> skeleton;
	/** Embedded animations, referenced by AnimationSampleNode::embeddedAnimation. */
	Span<cmf::v1::Animation> animations;
	Span<Parameter> parameters;
	/** Graphs; graphs[0] is the root graph, the rest are subgraphs referenced by Subgraph nodes and state graphs
	* referenced by StateMachine nodes. */
	Span<Graph> graphs;

	static constexpr std::string_view TypeName = "NetworkData";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, skeletonAsset, "skeletonAsset" );
		visitor( *this, skeletonName, "skeletonName" );
		visitor( *this, assets, "assets" );
		visitor( *this, skeleton, "skeleton" );
		visitor( *this, animations, "animations" );
		visitor( *this, parameters, "parameters" );
		visitor( *this, graphs, "graphs" );
	}
};

static_assert( sizeof( Node ) == offsetof( Node, inputConstants ) + sizeof( Span<float> ), "Node must not have implicit padding" );
static_assert( sizeof( NamedBoneWeight ) == offsetof( NamedBoneWeight, reserved ) + sizeof( uint32_t ), "NamedBoneWeight must not have implicit padding" );
static_assert( sizeof( StateMachineState ) == offsetof( StateMachineState, name ) + sizeof( String ), "StateMachineState must not have implicit padding" );
static_assert( sizeof( StateMachineTransition ) == offsetof( StateMachineTransition, expression ) + sizeof( String ), "StateMachineTransition must not have implicit padding" );

enum class SectionType : uint8_t
{
	/** Contains NetworkData. The only section of a CAN file. */
	Network,
};

struct Section
{
	/** Offset of the section from the start of the file, aligned to 8 bytes. */
	uint32_t offset = 0;
	/** Size of the section in bytes. */
	uint32_t size = 0;
	SectionType type = SectionType::Network;
	/** Reserved, always 0. Keeps the structure free of implicit padding. */
	uint8_t reserved0 = 0;
	uint8_t reserved1 = 0;
	uint8_t reserved2 = 0;

	static constexpr std::string_view TypeName = "Section";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, offset, "offset" );
		visitor( *this, size, "size" );
		visitor( *this, type, "type" );
	}
};

struct Header
{
	uint32_t signature = FILE_SIGNATURE;
	uint32_t version = FILE_VERSION;
	/** Size of the header including the section table. */
	uint32_t headerSize = 0;
	/** CRC32 of the file following this field. */
	uint32_t crc32 = 0;
	Span<Section> sections;

	static constexpr std::string_view TypeName = "Header";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, signature, "signature" );
		visitor( *this, version, "version" );
		visitor( *this, headerSize, "headerSize" );
		visitor( *this, crc32, "crc32" );
		visitor( *this, sections, "sections" );
	}
};

// NOLINTEND(readability-identifier-naming,cppcoreguidelines-missing-std-forward)

}

using namespace v1;

}
