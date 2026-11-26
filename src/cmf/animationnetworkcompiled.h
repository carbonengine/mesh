// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/animation.h"
#include "cmf/animationnetwork.h"
#include "cmf/bufferstreams.h"

#include <ccpparser.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

// Internal representation of an animation network prepared for evaluation. A CompiledNetwork is built once per
// AnimationNetwork and shared by all AnimationNetworkInstance objects. Subgraphs are inlined, nodes are sorted in
// evaluation order and all data used during evaluation is stored in a single memory block.

namespace cmf::detail
{

/** Computes the layout of several arrays stored in one memory block. */
class BlockLayout
{
public:
	/** Reserves space for count elements of type T and returns their byte offset in the block. */
	template <typename T>
	size_t Add( size_t count ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
	{
		static_assert( std::is_trivially_destructible_v<T>, "Block arrays are never destroyed" );
		static_assert( alignof( T ) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "Block memory is aligned to the default new alignment" );
		const size_t offset = ( m_size + alignof( T ) - 1 ) / alignof( T ) * alignof( T );
		m_size = offset + count * sizeof( T );
		return offset;
	}

	[[nodiscard]] size_t GetSize() const
	{
		return m_size;
	}

private:
	size_t m_size = 0;
};

/** Value-initializes count elements of type T at a byte offset of a block and returns a pointer to the first one. */
template <typename T>
T* ConstructArray( std::byte* block, size_t offset, size_t count ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	auto* first = reinterpret_cast<T*>( block + offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the layout reserved properly aligned space for the array
	for( size_t i = 0; i < count; ++i )
	{
		new( static_cast<void*>( first + i ) ) T(); // NOLINT(cppcoreguidelines-owning-memory): objects live in the block and are trivially destructible
	}
	return first;
}

/** Index of the pose slot that refers to the skeleton rest pose instead of instance memory. */
constexpr uint32_t REST_POSE_SLOT = 0xfffffffe;

enum class PoseOpType : uint8_t
{
	Sample,
	RestPose,
	Blend,
	AdditiveBlend,
	Switch,
	RandomSwitch,
	StateMachine,
	/** Number of operation types; not a valid operation type. */
	Count,
};

/** Properties of a pose operation type used when assigning pose slots. */
struct PoseOpTraits
{
	/** The operation writes its result to its own pose slot. */
	bool ownsSlot = false;
	/** The operation may output one of its inputs without copying it, which extends the lifetime of the input. */
	bool mayPassThrough = false;
};

/** Returns the properties of a pose operation type. */
inline PoseOpTraits GetPoseOpTraits( PoseOpType type )
{
	// Every operation type needs a case; there is deliberately no default case
	switch( type )
	{
	case PoseOpType::Sample:
		return { true, false };
	case PoseOpType::RestPose:
		return { false, false };
	case PoseOpType::Blend:
	case PoseOpType::AdditiveBlend:
	case PoseOpType::StateMachine:
		return { true, true };
	case PoseOpType::Switch:
	case PoseOpType::RandomSwitch:
		return { false, true };
	case PoseOpType::Count:
		break;
	}
	return {};
}

/** One pose producing operation. Pose inputs reference other operations by index. */
struct PoseOp
{
	PoseOpType type = PoseOpType::RestPose;
	/** Pose slot receiving the result; REST_POSE_SLOT or can::NO_INDEX for operations that pass an input through. */
	uint32_t outputSlot = can::NO_INDEX;
	/** Blend: poseA, poseB; AdditiveBlend: pose, base, additive. */
	std::array<uint32_t, 3> inputs = { can::NO_INDEX, can::NO_INDEX, can::NO_INDEX };
	/** Blend, AdditiveBlend: alpha; Switch: selector. Float slot index. */
	uint32_t floatSlot = can::NO_INDEX;
	/** Blend, AdditiveBlend: index of the bone weight array, or NO_INDEX. */
	uint32_t boneWeights = can::NO_INDEX;
	/** Sample: sample index; RandomSwitch: random switch index; StateMachine: state machine index. */
	uint32_t data = can::NO_INDEX;
	/** Switch, RandomSwitch, StateMachine: number of pose inputs (one per state for StateMachine); their operation
	* indices are in CompiledNetwork::switchInputs. */
	uint32_t inputCount = 0;
	/** Switch, RandomSwitch, StateMachine: first entry in CompiledNetwork::switchInputs. */
	uint32_t firstInput = 0;
};

/** Expression evaluation writing to a float slot. */
struct FloatOp
{
	/** Index in CompiledNetwork::programs. */
	uint32_t program = 0;
	/** Float slot receiving the result. */
	uint32_t outputSlot = 0;
};

/** Curve of an animation bound to a bone. */
template <typename T>
struct CurveBinding // NOLINT(readability-identifier-naming): clang-tidy classifies class templates as Default (camelBack); name follows StructCase
{
	ConstBufferElementStream<float> knots;
	ConstBufferElementStream<T> values;
	Interpolation interpolation = Interpolation::Linear;
	/** Vector3 curves: true for scale, false for position. */
	bool scale = false;
	uint32_t bone = 0;
};

/** Animation sampled by a Sample operation. */
struct SampleInfo
{
	float duration = 0.f;
	/** Float slots of the time, loop and speed inputs. */
	uint32_t timeSlot = 0;
	uint32_t loopSlot = 0;
	uint32_t speedSlot = 0;
	/** The time input is connected: the sample uses it instead of its playhead. */
	bool timeConnected = false;
	/** Ranges in CompiledNetwork::vectorCurves and quaternionCurves. */
	uint32_t firstVectorCurve = 0;
	uint32_t vectorCurveCount = 0;
	uint32_t firstQuaternionCurve = 0;
	uint32_t quaternionCurveCount = 0;
};

struct RandomSwitchInfo
{
	/** First entry in CompiledNetwork::randomThresholds: cumulative, normalized input probabilities. */
	uint32_t firstThreshold = 0;
	/** Number of pose inputs. */
	uint32_t inputCount = 0;
	bool rerollOnActivate = false;
};

/** Playback state of a Sample operation. */
struct Playhead
{
	/** Local time in the animation. */
	float time = 0.f;
	/** The animation looped or reached its end during the last update. */
	bool cycled = false;
	/** A non-looping animation reached its end. */
	bool ended = false;
};

/** State of a RandomSwitch operation. */
struct RandomSwitchState
{
	/** Index of the selected input. */
	uint32_t selected = 0;
	/** The operation was used in the last evaluation. */
	bool active = false;
	/** Update count when the selected input last finished a cycle; limits rerolls to one per update. */
	uint32_t lastCycleUpdate = 0;
};

/** Maximum number of states a state machine blends at the same time; when a transition with a duration is taken while
* the stack is full, the oldest fading state is dropped. */
constexpr uint32_t MAX_STATE_MACHINE_LAYERS = 8;

struct StateMachineInfo
{
	/** Graph and node index of the StateMachine node in the CAN file. */
	uint32_t graph = can::NO_INDEX;
	uint32_t node = can::NO_INDEX;
	/** First entry in CompiledNetwork::states. */
	uint32_t firstState = 0;
	uint32_t stateCount = 0;
	uint32_t startState = 0;
	/** Float slot holding the time spent in the current state, read by transition expressions. */
	uint32_t stateTimeSlot = 0;
	bool resumeOnActivate = false;
};

struct StateInfo
{
	/** Pose operation producing the pose of the state. */
	uint32_t poseOp = can::NO_INDEX;
	/** Range in CompiledNetwork::samples of the samples inside the state; their playheads restart when the state is
	* entered. */
	uint32_t firstSample = 0;
	uint32_t sampleCount = 0;
	/** Sample whose end finishes the state, or NO_INDEX if the state never finishes. */
	uint32_t finishSample = can::NO_INDEX;
	/** Range in CompiledNetwork::transitions of the transitions leaving the state. */
	uint32_t firstTransition = 0;
	uint32_t transitionCount = 0;
};

struct TransitionInfo
{
	/** Index of the destination state within the state machine. */
	uint32_t target = 0;
	can::TransitionCondition condition = can::TransitionCondition::Expression;
	float duration = 0.f;
	float timeout = 0.f;
	/** Expression condition: index in CompiledNetwork::programs. */
	uint32_t program = can::NO_INDEX;
};

/** A state of a state machine being blended. */
struct StateMachineLayer
{
	/** Index of the state within the state machine. */
	uint32_t state = 0;
	/** Fade-in progress in [0, 1]; the layer fully covers the layers below it at 1. */
	float progress = 1.f;
	/** Fade-in duration in seconds. */
	float duration = 0.f;
};

/** State of a StateMachine operation. Its layers are in a separate array, MAX_STATE_MACHINE_LAYERS per machine. */
struct StateMachineState
{
	/** Time spent in the current state. */
	float stateTime = 0.f;
	/** Number of layers; the last one is the current state. */
	uint32_t layerCount = 0;
	/** The operation was used in the last evaluation. */
	bool active = false;
	/** The machine has entered its start state at least once. */
	bool started = false;
};

/** Byte offsets of the arrays in the memory block of an instance. */
struct InstanceLayout
{
	size_t size = 0;
	size_t floatSlots = 0;
	size_t playheads = 0;
	size_t vectorCaches = 0;
	size_t quaternionCaches = 0;
	size_t randomStates = 0;
	size_t stateMachineStates = 0;
	size_t stateMachineLayers = 0;
	size_t opOutputs = 0;
	size_t opFlags = 0;
	size_t poseSlots = 0;
	size_t arena = 0;
};

/** Evaluation data of an animation network. All arrays are stored in one memory block. */
struct CompiledNetwork // NOLINT(clang-analyzer-optin.performance.Padding): one object per network; fields are grouped by meaning
{
	std::unique_ptr<std::byte[]> block; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): owner of the raw memory block

	const Skeleton* skeleton = nullptr;
	uint32_t boneCount = 0;
	const can::Parameter* parameters = nullptr;

	/** Float slots: parameters, then constants, then expression results, then the state time of each state machine. */
	uint32_t parameterCount = 0;
	uint32_t constantCount = 0;
	uint32_t floatSlotCount = 0;
	const float* constants = nullptr;

	const FloatOp* floatOps = nullptr;
	uint32_t floatOpCount = 0;
	/** Expressions of Expression nodes (evaluated by floatOps) followed by transition conditions; the bytecode of each
	* program is a separate allocation of the parser. */
	std::vector<CcpParser::Program> programs;
	size_t arenaSize = 0;

	/** Pose operations in evaluation order; the last one produces the output pose. */
	const PoseOp* poseOps = nullptr;
	uint32_t poseOpCount = 0;
	const uint32_t* switchInputs = nullptr;
	uint32_t poseSlotCount = 0;

	const SampleInfo* samples = nullptr;
	uint32_t sampleCount = 0;
	const CurveBinding<Vector3>* vectorCurves = nullptr;
	uint32_t vectorCurveCount = 0;
	const CurveBinding<Vector4>* quaternionCurves = nullptr;
	uint32_t quaternionCurveCount = 0;

	const RandomSwitchInfo* randomSwitches = nullptr;
	uint32_t randomSwitchCount = 0;
	const float* randomThresholds = nullptr;

	const StateMachineInfo* stateMachines = nullptr;
	uint32_t stateMachineCount = 0;
	const StateInfo* states = nullptr;
	const TransitionInfo* transitions = nullptr;

	/** Bone weight arrays, boneCount values each. */
	const float* boneWeights = nullptr;

	InstanceLayout instanceLayout;
};

/** Builds the evaluation data of a network whose assets are loaded and that has no errors.
* @param errors Receives descriptions of problems found while compiling, such as invalid expressions.
* @return The compiled network, or nullptr if errors were found. */
std::unique_ptr<CompiledNetwork> CompileNetwork( const AnimationNetwork& network, std::vector<std::string>& errors );

/** Returns the memory layout of instances of a compiled network. */
InstanceLayout ComputeInstanceLayout( const CompiledNetwork& compiled );

}
