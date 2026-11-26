// Copyright © 2026 CCP ehf.

#pragma once

#include "animation.h"
#include "animationnetwork.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cmf
{

namespace detail
{
struct CompiledNetwork; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase struct
struct Playhead; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase struct
struct RandomSwitchState; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase struct
struct StateMachineState; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase struct
struct StateMachineLayer; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase struct
enum class PoseOpType : uint8_t;
}

/** @brief Evaluates an AnimationNetwork for one animated object.
*
* Create one instance per animated object. Each frame, set the control parameters, call Update with the elapsed
* time and Evaluate to compute the pose. The instance keeps the playback state of the network: the playhead of every
* AnimationSample node, the selection of every RandomSwitch node and the states of every StateMachine node.
*
* StateMachine nodes take transitions in Evaluate. Update advances the time spent in the current state and the blends
* of state machines that contributed to the last evaluated pose.
*
* All instance data is stored in a single memory allocation made by the constructor; Update and Evaluate don't
* allocate memory. Instances of the same network can be used on different threads at the same time; a single
* instance must not be used by several threads at once.
*
* The network must be ready (AnimationNetwork::IsReady) when the instance is created, must outlive the instance and
* must not be loaded again while the instance exists.
*/
class AnimationNetworkInstance
{
public:
	/** Value returned by FindParameter for unknown names. */
	static constexpr uint32_t INVALID_PARAMETER = can::NO_INDEX;

	/** @brief Creates an instance of a ready network. If the network isn't ready, the instance is invalid.
	* @param network The network to evaluate.
	* @param seed Seed of the random number generator used by RandomSwitch nodes.
	*/
	CARBON_MESH_EXPORT explicit AnimationNetworkInstance( const AnimationNetwork& network, uint32_t seed = 0 );
	CARBON_MESH_EXPORT ~AnimationNetworkInstance();

	AnimationNetworkInstance( const AnimationNetworkInstance& ) = delete;
	AnimationNetworkInstance& operator=( const AnimationNetworkInstance& ) = delete;
	CARBON_MESH_EXPORT AnimationNetworkInstance( AnimationNetworkInstance&& ) noexcept;
	CARBON_MESH_EXPORT AnimationNetworkInstance& operator=( AnimationNetworkInstance&& ) noexcept;

	/** @brief Returns true if the instance was created for a ready network that hasn't been reloaded since. */
	[[nodiscard]] CARBON_MESH_EXPORT bool IsValid() const;

	/** @brief Returns the index of a control parameter, or INVALID_PARAMETER. Look up indices once and use them
	* with SetParameter every frame. */
	[[nodiscard]] CARBON_MESH_EXPORT uint32_t FindParameter( std::string_view name ) const;

	/** @brief Sets a control parameter. The value is clamped to the parameter range. */
	CARBON_MESH_EXPORT void SetParameter( uint32_t index, float value );

	/** @brief Returns the current value of a control parameter. */
	[[nodiscard]] CARBON_MESH_EXPORT float GetParameter( uint32_t index ) const;

	/** @brief Advances the playheads of all AnimationSample nodes by deltaTime multiplied by their speed input.
	* Playheads advance whether or not their node contributes to the current pose, so that a node that becomes
	* active again continues in sync. */
	CARBON_MESH_EXPORT void Update( float deltaTime );

	/** @brief Computes the pose for the current parameters and playheads. Only nodes that contribute to the result
	* are evaluated.
	* @return false if the instance is invalid.
	*/
	CARBON_MESH_EXPORT bool Evaluate();

	/** @brief Returns the total time passed to Update. */
	[[nodiscard]] CARBON_MESH_EXPORT float GetTime() const;

	/** @brief Returns the number of skeleton bones, 0 for invalid instances. */
	[[nodiscard]] CARBON_MESH_EXPORT uint32_t GetBoneCount() const;

	/** @brief Returns the local bone transforms computed by the last Evaluate call, one per skeleton bone, or nullptr
	* if Evaluate hasn't succeeded yet. The data is valid until the next Evaluate call. */
	[[nodiscard]] CARBON_MESH_EXPORT const Transform* GetLocalTransforms() const;

	/** @brief Copies the pose computed by the last Evaluate call. */
	CARBON_MESH_EXPORT void GetPose( SkeletonPose& pose ) const;

	/** @brief Returns the current state of a StateMachine node, for example to show it in an editor.
	* @param graphIndex Index of the graph containing the node in can::NetworkData::graphs (the same as the index in
	* AnimationNetworkBuilder::GetSaveLayout).
	* @param nodeIndex Index of the node in the graph.
	* @return Index of the current state, or INVALID_STATE if the node is not a StateMachine node or hasn't been
	* evaluated yet.
	*/
	[[nodiscard]] CARBON_MESH_EXPORT uint32_t GetCurrentState( uint32_t graphIndex, uint32_t nodeIndex ) const;

	/** Value returned by GetCurrentState when there is no current state. */
	static constexpr uint32_t INVALID_STATE = can::NO_INDEX;

private:
	void EvaluateFloats();
	/** Functions implementing one pose operation type. */
	struct OperationHandlers
	{
		/** Marks the inputs the operation needs; called for all operations, needed or not, in reverse order. */
		void ( AnimationNetworkInstance::*markInputs )( uint32_t opIndex, bool needed ) = nullptr;
		/** Computes the output of a needed operation; returns true if the output finished an animation cycle. */
		bool ( AnimationNetworkInstance::*execute )( uint32_t opIndex ) = nullptr;
	};

	static OperationHandlers GetOperationHandlers( detail::PoseOpType type );
	static const OperationHandlers& GetHandlers( detail::PoseOpType type );

	void MarkNeededOperations();
	void ExecuteOperations();
	[[nodiscard]] Transform* GetPoseSlot( uint32_t slot ) const;
	void MarkInput( uint32_t inputOpIndex );
	bool PassThrough( uint32_t opIndex, uint32_t inputOpIndex );

	void MarkNoInputs( uint32_t opIndex, bool needed );
	void MarkBlendInputs( uint32_t opIndex, bool needed );
	void MarkAdditiveBlendInputs( uint32_t opIndex, bool needed );
	void MarkSwitchInputs( uint32_t opIndex, bool needed );
	void MarkRandomSwitchInputs( uint32_t opIndex, bool needed );
	void MarkStateMachineInputs( uint32_t opIndex, bool needed );

	bool ExecuteSample( uint32_t opIndex );
	bool ExecuteRestPose( uint32_t opIndex );
	bool ExecuteBlend( uint32_t opIndex );
	bool ExecuteAdditiveBlend( uint32_t opIndex );
	bool ExecuteSwitch( uint32_t opIndex );
	bool ExecuteRandomSwitch( uint32_t opIndex );
	bool ExecuteStateMachine( uint32_t opIndex );
	void UpdateStateMachines( float deltaTime );
	[[nodiscard]] bool IsTransitionSatisfied( uint32_t machineIndex, uint32_t transitionIndex );
	void EnterState( uint32_t machineIndex, uint32_t state, float duration );
	void SampleAnimation( uint32_t sampleIndex, Transform* output );
	[[nodiscard]] float GetSampleTime( uint32_t sampleIndex ) const;
	[[nodiscard]] uint32_t GetSwitchSelection( uint32_t opIndex ) const;
	void Reroll( uint32_t randomSwitchIndex );
	uint32_t NextRandom();

	const AnimationNetwork* m_network = nullptr;
	const detail::CompiledNetwork* m_compiled = nullptr;
	uint32_t m_loadId = 0;
	std::unique_ptr<std::byte[]> m_block; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): owner of the raw memory block
	float* m_floats = nullptr;
	detail::Playhead* m_playheads = nullptr;
	CurveEvaluationCache<Vector3>* m_vectorCaches = nullptr;
	CurveEvaluationCache<Vector4>* m_quaternionCaches = nullptr;
	detail::RandomSwitchState* m_randomStates = nullptr;
	detail::StateMachineState* m_stateMachineStates = nullptr;
	detail::StateMachineLayer* m_stateMachineLayers = nullptr;
	const Transform** m_opOutputs = nullptr;
	uint8_t* m_opFlags = nullptr;
	Transform* m_poseSlots = nullptr;
	void* m_arena = nullptr;
	const Transform* m_output = nullptr;
	float m_time = 0.f;
	uint32_t m_random = 0;
	uint32_t m_updateCount = 0;
};

}
