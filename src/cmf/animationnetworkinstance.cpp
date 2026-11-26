// Copyright © 2026 CCP ehf.

#include "cmf/animationnetworkinstance.h"

#include "animationinternal.h"
#include "animationnetworkcompiled.h"
#include "posekernels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace cmf
{
namespace
{

constexpr uint8_t OP_NEEDED = 1;
constexpr uint8_t OP_CYCLED = 2;
constexpr uint32_t DEFAULT_SEED = 0x9e3779b9;
constexpr float HALF = 0.5f;
/** Scale converting the upper 24 bits of a random number to [0, 1). */
constexpr float RANDOM_SCALE = 1.f / 16777216.f;
constexpr uint32_t RANDOM_SHIFT = 8;

float Clamp01( float value )
{
	// Also maps NaN to 0
	return value > 0.f ? std::min( value, 1.f ) : 0.f;
}

// Wraps or clamps a time to the animation duration
float ToLocalTime( float time, float duration, bool loop )
{
	if( duration <= 0.f )
	{
		return 0.f;
	}
	if( loop )
	{
		const float wrapped = std::fmod( time, duration );
		return wrapped < 0.f ? wrapped + duration : wrapped;
	}
	return std::clamp( time, 0.f, duration );
}

}

AnimationNetworkInstance::AnimationNetworkInstance( const AnimationNetwork& network, uint32_t seed ) :
	m_network( &network ),
	m_random( seed == 0 ? DEFAULT_SEED : seed )
{
	if( !network.IsReady() )
	{
		return;
	}
	m_compiled = network.m_compiled.get();
	m_loadId = network.m_loadId;
	const auto& compiled = *m_compiled;
	const auto& layout = compiled.instanceLayout;

	m_block.reset( new std::byte[layout.size] ); // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): raw memory block owned by the unique_ptr
	auto* block = m_block.get();
	m_floats = detail::ConstructArray<float>( block, layout.floatSlots, compiled.floatSlotCount );
	m_playheads = detail::ConstructArray<detail::Playhead>( block, layout.playheads, compiled.sampleCount );
	m_vectorCaches = detail::ConstructArray<CurveEvaluationCache<Vector3>>( block, layout.vectorCaches, compiled.vectorCurveCount );
	m_quaternionCaches = detail::ConstructArray<CurveEvaluationCache<Vector4>>( block, layout.quaternionCaches, compiled.quaternionCurveCount );
	m_randomStates = detail::ConstructArray<detail::RandomSwitchState>( block, layout.randomStates, compiled.randomSwitchCount );
	m_stateMachineStates = detail::ConstructArray<detail::StateMachineState>( block, layout.stateMachineStates, compiled.stateMachineCount );
	m_stateMachineLayers = detail::ConstructArray<detail::StateMachineLayer>( block, layout.stateMachineLayers, static_cast<size_t>( compiled.stateMachineCount ) * detail::MAX_STATE_MACHINE_LAYERS );
	m_opOutputs = detail::ConstructArray<const Transform*>( block, layout.opOutputs, compiled.poseOpCount );
	m_opFlags = detail::ConstructArray<uint8_t>( block, layout.opFlags, compiled.poseOpCount );
	m_poseSlots = detail::ConstructArray<Transform>( block, layout.poseSlots, static_cast<size_t>( compiled.poseSlotCount ) * compiled.boneCount );
	m_arena = compiled.arenaSize > 0 ? block + layout.arena : nullptr;

	for( uint32_t i = 0; i < compiled.parameterCount; ++i )
	{
		m_floats[i] = compiled.parameters[i].defaultValue;
	}
	std::copy( compiled.constants, compiled.constants + compiled.constantCount, m_floats + compiled.parameterCount );
	for( uint32_t i = 0; i < compiled.randomSwitchCount; ++i )
	{
		Reroll( i );
	}
}

AnimationNetworkInstance::~AnimationNetworkInstance() = default;

AnimationNetworkInstance::AnimationNetworkInstance( AnimationNetworkInstance&& ) noexcept = default;

AnimationNetworkInstance& AnimationNetworkInstance::operator=( AnimationNetworkInstance&& ) noexcept = default;

bool AnimationNetworkInstance::IsValid() const
{
	return m_compiled && m_network->m_loadId == m_loadId;
}

uint32_t AnimationNetworkInstance::FindParameter( std::string_view name ) const
{
	if( !m_compiled )
	{
		return INVALID_PARAMETER;
	}
	for( uint32_t i = 0; i < m_compiled->parameterCount; ++i )
	{
		if( ToStdStringView( m_compiled->parameters[i].name ) == name )
		{
			return i;
		}
	}
	return INVALID_PARAMETER;
}

void AnimationNetworkInstance::SetParameter( uint32_t index, float value )
{
	if( !m_compiled || index >= m_compiled->parameterCount )
	{
		return;
	}
	const auto& parameter = m_compiled->parameters[index];
	m_floats[index] = std::clamp( value, parameter.minValue, parameter.maxValue );
}

float AnimationNetworkInstance::GetParameter( uint32_t index ) const
{
	if( !m_compiled || index >= m_compiled->parameterCount )
	{
		return 0.f;
	}
	return m_floats[index];
}

void AnimationNetworkInstance::Update( float deltaTime )
{
	if( !IsValid() )
	{
		return;
	}
	m_time += deltaTime;
	++m_updateCount;
	EvaluateFloats();
	for( uint32_t i = 0; i < m_compiled->sampleCount; ++i )
	{
		const auto& sample = m_compiled->samples[i];
		auto& playhead = m_playheads[i];
		playhead.cycled = false;
		if( sample.timeConnected || sample.duration <= 0.f )
		{
			continue;
		}
		const bool loop = m_floats[sample.loopSlot] != 0.f;
		const float time = playhead.time + deltaTime * m_floats[sample.speedSlot];
		if( loop )
		{
			playhead.cycled = time >= sample.duration || time < 0.f;
			playhead.ended = false;
			playhead.time = ToLocalTime( time, sample.duration, true );
		}
		else
		{
			const bool ended = time >= sample.duration;
			playhead.cycled = ended && !playhead.ended;
			playhead.ended = ended;
			playhead.time = ToLocalTime( time, sample.duration, false );
		}
	}
	UpdateStateMachines( deltaTime );
}

void AnimationNetworkInstance::UpdateStateMachines( float deltaTime )
{
	for( uint32_t m = 0; m < m_compiled->stateMachineCount; ++m )
	{
		auto& state = m_stateMachineStates[m];
		if( !state.active )
		{
			continue;
		}
		state.stateTime += deltaTime;
		m_floats[m_compiled->stateMachines[m].stateTimeSlot] = state.stateTime;
		auto* layers = m_stateMachineLayers + static_cast<size_t>( m ) * detail::MAX_STATE_MACHINE_LAYERS;
		// The bottom layer is fully visible; the layers above it fade in
		for( uint32_t i = 1; i < state.layerCount; ++i )
		{
			auto& layer = layers[i];
			layer.progress = layer.duration > 0.f ? std::min( layer.progress + deltaTime / layer.duration, 1.f ) : 1.f;
		}
	}
}

uint32_t AnimationNetworkInstance::GetCurrentState( uint32_t graphIndex, uint32_t nodeIndex ) const
{
	if( !IsValid() )
	{
		return INVALID_STATE;
	}
	for( uint32_t m = 0; m < m_compiled->stateMachineCount; ++m )
	{
		const auto& machine = m_compiled->stateMachines[m];
		if( machine.graph == graphIndex && machine.node == nodeIndex )
		{
			const auto& state = m_stateMachineStates[m];
			if( state.layerCount == 0 )
			{
				return INVALID_STATE;
			}
			return m_stateMachineLayers[static_cast<size_t>( m ) * detail::MAX_STATE_MACHINE_LAYERS + state.layerCount - 1].state;
		}
	}
	return INVALID_STATE;
}

bool AnimationNetworkInstance::Evaluate()
{
	if( !IsValid() )
	{
		return false;
	}
	EvaluateFloats();
	MarkNeededOperations();
	ExecuteOperations();
	m_output = m_opOutputs[m_compiled->poseOpCount - 1];
	return true;
}

float AnimationNetworkInstance::GetTime() const
{
	return m_time;
}

uint32_t AnimationNetworkInstance::GetBoneCount() const
{
	return m_compiled ? m_compiled->boneCount : 0;
}

const Transform* AnimationNetworkInstance::GetLocalTransforms() const
{
	return m_output;
}

void AnimationNetworkInstance::GetPose( SkeletonPose& pose ) const
{
	if( !m_output )
	{
		return;
	}
	pose.skeleton = m_compiled->skeleton;
	pose.boneTransforms.assign( m_output, m_output + m_compiled->boneCount );
}

void AnimationNetworkInstance::EvaluateFloats()
{
	const auto& compiled = *m_compiled;
	std::array<void*, 1> externData = { m_floats };
	for( uint32_t i = 0; i < compiled.floatOpCount; ++i )
	{
		const auto& op = compiled.floatOps[i];
		m_floats[op.outputSlot] = compiled.programs[op.program].Eval( externData.data(), m_arena );
	}
}

AnimationNetworkInstance::OperationHandlers AnimationNetworkInstance::GetOperationHandlers( detail::PoseOpType type )
{
	using Instance = AnimationNetworkInstance;
	// Every operation type needs a case; there is deliberately no default case
	switch( type )
	{
	case detail::PoseOpType::Sample:
		return { &Instance::MarkNoInputs, &Instance::ExecuteSample };
	case detail::PoseOpType::RestPose:
		return { &Instance::MarkNoInputs, &Instance::ExecuteRestPose };
	case detail::PoseOpType::Blend:
		return { &Instance::MarkBlendInputs, &Instance::ExecuteBlend };
	case detail::PoseOpType::AdditiveBlend:
		return { &Instance::MarkAdditiveBlendInputs, &Instance::ExecuteAdditiveBlend };
	case detail::PoseOpType::Switch:
		return { &Instance::MarkSwitchInputs, &Instance::ExecuteSwitch };
	case detail::PoseOpType::RandomSwitch:
		return { &Instance::MarkRandomSwitchInputs, &Instance::ExecuteRandomSwitch };
	case detail::PoseOpType::StateMachine:
		return { &Instance::MarkStateMachineInputs, &Instance::ExecuteStateMachine };
	case detail::PoseOpType::Count:
		break;
	}
	return {};
}

const AnimationNetworkInstance::OperationHandlers& AnimationNetworkInstance::GetHandlers( detail::PoseOpType type )
{
	static const auto handlers = [] {
		std::array<OperationHandlers, static_cast<size_t>( detail::PoseOpType::Count )> result;
		for( size_t i = 0; i < result.size(); ++i )
		{
			result[i] = GetOperationHandlers( static_cast<detail::PoseOpType>( i ) ); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index): i is below the array size
		}
		return result;
	}();
	return handlers[static_cast<size_t>( type )]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index): operation types of compiled networks are valid
}

// Walks the operations from the output back to the inputs and marks the ones that contribute to the result
void AnimationNetworkInstance::MarkNeededOperations()
{
	const auto& compiled = *m_compiled;
	std::fill( m_opFlags, m_opFlags + compiled.poseOpCount, static_cast<uint8_t>( 0 ) );
	m_opFlags[compiled.poseOpCount - 1] = OP_NEEDED;
	for( uint32_t i = compiled.poseOpCount; i-- > 0; )
	{
		const bool needed = ( m_opFlags[i] & OP_NEEDED ) != 0;
		( this->*GetHandlers( compiled.poseOps[i].type ).markInputs )( i, needed );
	}
}

void AnimationNetworkInstance::ExecuteOperations()
{
	const auto& compiled = *m_compiled;
	for( uint32_t i = 0; i < compiled.poseOpCount; ++i )
	{
		if( ( m_opFlags[i] & OP_NEEDED ) != 0 )
		{
			const bool cycled = ( this->*GetHandlers( compiled.poseOps[i].type ).execute )( i );
			m_opFlags[i] = static_cast<uint8_t>( OP_NEEDED | ( cycled ? OP_CYCLED : 0 ) );
		}
	}
}

void AnimationNetworkInstance::MarkInput( uint32_t inputOpIndex )
{
	m_opFlags[inputOpIndex] |= OP_NEEDED;
}

void AnimationNetworkInstance::MarkNoInputs( uint32_t /*opIndex*/, bool /*needed*/ )
{
}

void AnimationNetworkInstance::MarkBlendInputs( uint32_t opIndex, bool needed )
{
	const auto& op = m_compiled->poseOps[opIndex];
	if( !needed )
	{
		return;
	}
	const float alpha = Clamp01( m_floats[op.floatSlot] );
	if( alpha < 1.f || op.boneWeights != can::NO_INDEX )
	{
		MarkInput( op.inputs[0] );
	}
	if( alpha > 0.f )
	{
		MarkInput( op.inputs[1] );
	}
}

void AnimationNetworkInstance::MarkAdditiveBlendInputs( uint32_t opIndex, bool needed )
{
	const auto& op = m_compiled->poseOps[opIndex];
	if( !needed )
	{
		return;
	}
	MarkInput( op.inputs[0] );
	if( Clamp01( m_floats[op.floatSlot] ) > 0.f )
	{
		MarkInput( op.inputs[1] );
		MarkInput( op.inputs[2] );
	}
}

void AnimationNetworkInstance::MarkSwitchInputs( uint32_t opIndex, bool needed )
{
	const auto& op = m_compiled->poseOps[opIndex];
	if( needed )
	{
		MarkInput( m_compiled->switchInputs[op.firstInput + GetSwitchSelection( opIndex )] );
	}
}

void AnimationNetworkInstance::MarkRandomSwitchInputs( uint32_t opIndex, bool needed )
{
	const auto& op = m_compiled->poseOps[opIndex];
	auto& state = m_randomStates[op.data];
	if( needed && !state.active && m_compiled->randomSwitches[op.data].rerollOnActivate )
	{
		Reroll( op.data );
	}
	state.active = needed;
	if( needed )
	{
		MarkInput( m_compiled->switchInputs[op.firstInput + state.selected] );
	}
}

// Takes the state machine transitions and marks the states that contribute to its pose. Operations are marked in
// reverse order, so a state machine decides before the state machines nested in its states.
void AnimationNetworkInstance::MarkStateMachineInputs( uint32_t opIndex, bool needed )
{
	const auto& compiled = *m_compiled;
	const auto& op = compiled.poseOps[opIndex];
	const auto machineIndex = op.data;
	const auto& machine = compiled.stateMachines[machineIndex];
	auto& state = m_stateMachineStates[machineIndex];
	if( !needed )
	{
		state.active = false;
		return;
	}
	auto* layers = m_stateMachineLayers + static_cast<size_t>( machineIndex ) * detail::MAX_STATE_MACHINE_LAYERS;
	if( !state.active && ( !state.started || !machine.resumeOnActivate ) )
	{
		state.started = true;
		state.layerCount = 0;
		EnterState( machineIndex, machine.startState, 0.f );
	}
	state.active = true;

	// Layers below a fully faded in layer are no longer visible
	for( uint32_t i = state.layerCount; i-- > 1; )
	{
		if( layers[i].progress >= 1.f )
		{
			std::copy( layers + i, layers + state.layerCount, layers );
			state.layerCount -= i;
			break;
		}
	}

	const auto& current = compiled.states[machine.firstState + layers[state.layerCount - 1].state];
	for( uint32_t t = 0; t < current.transitionCount; ++t )
	{
		const auto transitionIndex = current.firstTransition + t;
		if( IsTransitionSatisfied( machineIndex, transitionIndex ) )
		{
			const auto& transition = compiled.transitions[transitionIndex];
			EnterState( machineIndex, transition.target, transition.duration );
			break;
		}
	}

	for( uint32_t i = 0; i < state.layerCount; ++i )
	{
		MarkInput( compiled.switchInputs[op.firstInput + layers[i].state] );
	}
}

bool AnimationNetworkInstance::IsTransitionSatisfied( uint32_t machineIndex, uint32_t transitionIndex )
{
	const auto& compiled = *m_compiled;
	const auto& transition = compiled.transitions[transitionIndex];
	const auto& state = m_stateMachineStates[machineIndex];
	// Every condition type needs a case; there is deliberately no default case
	switch( transition.condition )
	{
	case can::TransitionCondition::Expression: {
		std::array<void*, 1> externData = { m_floats };
		return compiled.programs[transition.program].Eval( externData.data(), m_arena ) != 0.f;
	}
	case can::TransitionCondition::AnimationFinished: {
		const auto& machine = compiled.stateMachines[machineIndex];
		const auto* layers = m_stateMachineLayers + static_cast<size_t>( machineIndex ) * detail::MAX_STATE_MACHINE_LAYERS;
		const auto& current = compiled.states[machine.firstState + layers[state.layerCount - 1].state];
		return current.finishSample != can::NO_INDEX && m_playheads[current.finishSample].ended;
	}
	case can::TransitionCondition::Timeout:
		return state.stateTime >= transition.timeout;
	}
	return false;
}

// Makes a state the current state: restarts its animations and adds a layer fading it in over the duration
void AnimationNetworkInstance::EnterState( uint32_t machineIndex, uint32_t stateIndex, float duration )
{
	const auto& compiled = *m_compiled;
	const auto& machine = compiled.stateMachines[machineIndex];
	const auto& info = compiled.states[machine.firstState + stateIndex];
	std::fill( m_playheads + info.firstSample, m_playheads + info.firstSample + info.sampleCount, detail::Playhead{} );

	auto& state = m_stateMachineStates[machineIndex];
	state.stateTime = 0.f;
	m_floats[machine.stateTimeSlot] = 0.f;
	auto* layers = m_stateMachineLayers + static_cast<size_t>( machineIndex ) * detail::MAX_STATE_MACHINE_LAYERS;
	if( duration <= 0.f || state.layerCount == 0 )
	{
		layers[0] = { stateIndex, 1.f, 0.f };
		state.layerCount = 1;
		return;
	}
	if( state.layerCount == detail::MAX_STATE_MACHINE_LAYERS )
	{
		// Drop the oldest fading state
		std::copy( layers + 1, layers + state.layerCount, layers );
		--state.layerCount;
	}
	layers[state.layerCount++] = { stateIndex, 0.f, duration };
}

bool AnimationNetworkInstance::ExecuteStateMachine( uint32_t opIndex )
{
	const auto& compiled = *m_compiled;
	const auto& op = compiled.poseOps[opIndex];
	const auto& state = m_stateMachineStates[op.data];
	const auto* layers = m_stateMachineLayers + static_cast<size_t>( op.data ) * detail::MAX_STATE_MACHINE_LAYERS;
	const auto topInput = compiled.switchInputs[op.firstInput + layers[state.layerCount - 1].state];
	if( state.layerCount == 1 )
	{
		return PassThrough( opIndex, topInput );
	}
	// Blend the layers bottom to top: each layer covers the result of the layers below it by its fade-in progress
	auto* output = GetPoseSlot( op.outputSlot );
	const Transform* blended = m_opOutputs[compiled.switchInputs[op.firstInput + layers[0].state]];
	for( uint32_t i = 1; i < state.layerCount; ++i )
	{
		const auto* pose = m_opOutputs[compiled.switchInputs[op.firstInput + layers[i].state]];
		detail::BlendTransforms( output, blended, pose, compiled.boneCount, Clamp01( layers[i].progress ) );
		blended = output;
	}
	m_opOutputs[opIndex] = output;
	return ( m_opFlags[topInput] & OP_CYCLED ) != 0;
}

bool AnimationNetworkInstance::ExecuteRestPose( uint32_t opIndex )
{
	m_opOutputs[opIndex] = m_compiled->skeleton->restTransforms.data();
	return false;
}

bool AnimationNetworkInstance::ExecuteSwitch( uint32_t opIndex )
{
	const auto& op = m_compiled->poseOps[opIndex];
	return PassThrough( opIndex, m_compiled->switchInputs[op.firstInput + GetSwitchSelection( opIndex )] );
}

Transform* AnimationNetworkInstance::GetPoseSlot( uint32_t slot ) const
{
	return m_poseSlots + static_cast<size_t>( slot ) * m_compiled->boneCount;
}

// Uses the output of an input operation as the output of an operation; returns the cycle signal of the input
bool AnimationNetworkInstance::PassThrough( uint32_t opIndex, uint32_t inputOpIndex )
{
	m_opOutputs[opIndex] = m_opOutputs[inputOpIndex];
	return ( m_opFlags[inputOpIndex] & OP_CYCLED ) != 0;
}

bool AnimationNetworkInstance::ExecuteSample( uint32_t opIndex )
{
	const auto& op = m_compiled->poseOps[opIndex];
	auto* output = GetPoseSlot( op.outputSlot );
	SampleAnimation( op.data, output );
	m_opOutputs[opIndex] = output;
	return m_playheads[op.data].cycled;
}

bool AnimationNetworkInstance::ExecuteBlend( uint32_t opIndex )
{
	const auto& compiled = *m_compiled;
	const auto& op = compiled.poseOps[opIndex];
	const float alpha = Clamp01( m_floats[op.floatSlot] );
	const auto* weights = op.boneWeights == can::NO_INDEX ? nullptr : compiled.boneWeights + static_cast<size_t>( op.boneWeights ) * compiled.boneCount;
	if( alpha <= 0.f )
	{
		return PassThrough( opIndex, op.inputs[0] );
	}
	if( alpha >= 1.f && !weights )
	{
		return PassThrough( opIndex, op.inputs[1] );
	}
	auto* output = GetPoseSlot( op.outputSlot );
	const auto* poseA = m_opOutputs[op.inputs[0]];
	const auto* poseB = m_opOutputs[op.inputs[1]];
	if( weights )
	{
		detail::BlendTransformsWeighted( output, poseA, poseB, weights, compiled.boneCount, alpha );
	}
	else
	{
		detail::BlendTransforms( output, poseA, poseB, compiled.boneCount, alpha );
	}
	m_opOutputs[opIndex] = output;
	// The cycle signal comes from the input with the larger contribution
	return ( m_opFlags[alpha < HALF ? op.inputs[0] : op.inputs[1]] & OP_CYCLED ) != 0;
}

bool AnimationNetworkInstance::ExecuteAdditiveBlend( uint32_t opIndex )
{
	const auto& compiled = *m_compiled;
	const auto& op = compiled.poseOps[opIndex];
	const float alpha = Clamp01( m_floats[op.floatSlot] );
	if( alpha <= 0.f )
	{
		return PassThrough( opIndex, op.inputs[0] );
	}
	auto* output = GetPoseSlot( op.outputSlot );
	const auto* pose = m_opOutputs[op.inputs[0]];
	const auto* base = m_opOutputs[op.inputs[1]];
	const auto* additive = m_opOutputs[op.inputs[2]];
	if( op.boneWeights != can::NO_INDEX )
	{
		const auto* weights = compiled.boneWeights + static_cast<size_t>( op.boneWeights ) * compiled.boneCount;
		detail::BlendAdditiveTransformsWeighted( output, pose, base, additive, weights, compiled.boneCount, alpha );
	}
	else
	{
		detail::BlendAdditiveTransforms( output, pose, base, additive, compiled.boneCount, alpha );
	}
	m_opOutputs[opIndex] = output;
	return ( m_opFlags[op.inputs[0]] & OP_CYCLED ) != 0;
}

bool AnimationNetworkInstance::ExecuteRandomSwitch( uint32_t opIndex )
{
	const auto& op = m_compiled->poseOps[opIndex];
	auto& state = m_randomStates[op.data];
	const bool cycled = PassThrough( opIndex, m_compiled->switchInputs[op.firstInput + state.selected] );
	if( cycled && state.lastCycleUpdate != m_updateCount )
	{
		// The selected input finished a cycle: pick the input for the next frames
		state.lastCycleUpdate = m_updateCount;
		Reroll( op.data );
	}
	return cycled;
}

void AnimationNetworkInstance::SampleAnimation( uint32_t sampleIndex, Transform* output )
{
	const auto& compiled = *m_compiled;
	const auto& sample = compiled.samples[sampleIndex];
	std::copy( compiled.skeleton->restTransforms.begin(), compiled.skeleton->restTransforms.end(), output );
	const float time = GetSampleTime( sampleIndex );
	for( uint32_t i = 0; i < sample.vectorCurveCount; ++i )
	{
		const auto index = sample.firstVectorCurve + i;
		const auto& curve = compiled.vectorCurves[index];
		const auto value = detail::SampleCurve( curve.knots, curve.values, curve.interpolation, time, m_vectorCaches[index] );
		auto& transform = output[curve.bone];
		( curve.scale ? transform.scale : transform.position ) = value;
	}
	for( uint32_t i = 0; i < sample.quaternionCurveCount; ++i )
	{
		const auto index = sample.firstQuaternionCurve + i;
		const auto& curve = compiled.quaternionCurves[index];
		output[curve.bone].rotation = detail::SampleQuaternionCurve( curve.knots, curve.values, curve.interpolation, time, m_quaternionCaches[index] );
	}
}

float AnimationNetworkInstance::GetSampleTime( uint32_t sampleIndex ) const
{
	const auto& sample = m_compiled->samples[sampleIndex];
	if( !sample.timeConnected )
	{
		return m_playheads[sampleIndex].time;
	}
	return ToLocalTime( m_floats[sample.timeSlot], sample.duration, m_floats[sample.loopSlot] != 0.f );
}

uint32_t AnimationNetworkInstance::GetSwitchSelection( uint32_t opIndex ) const
{
	const auto& op = m_compiled->poseOps[opIndex];
	const float value = m_floats[op.floatSlot];
	// Selector values are truncated: [0, 1) selects the first input, [1, 2) the second and so on
	if( !( value > 0.f ) )
	{
		return 0;
	}
	return static_cast<uint32_t>( std::min( value, static_cast<float>( op.inputCount - 1 ) ) );
}

void AnimationNetworkInstance::Reroll( uint32_t randomSwitchIndex )
{
	const auto& info = m_compiled->randomSwitches[randomSwitchIndex];
	const float value = static_cast<float>( NextRandom() >> RANDOM_SHIFT ) * RANDOM_SCALE;
	const auto* thresholds = m_compiled->randomThresholds + info.firstThreshold;
	const auto* selected = std::upper_bound( thresholds, thresholds + info.inputCount, value );
	m_randomStates[randomSwitchIndex].selected = std::min( static_cast<uint32_t>( selected - thresholds ), info.inputCount - 1 );
}

uint32_t AnimationNetworkInstance::NextRandom()
{
	// xorshift32
	constexpr uint32_t shiftA = 13;
	constexpr uint32_t shiftB = 17;
	constexpr uint32_t shiftC = 5;
	m_random ^= m_random << shiftA;
	m_random ^= m_random >> shiftB;
	m_random ^= m_random << shiftC;
	return m_random;
}

}
