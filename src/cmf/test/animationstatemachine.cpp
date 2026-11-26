// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"

#include "cmf/animation.h"
#include "cmf/animationnetworkinstance.h"
#include "networktestutils.h"

#include <cmath>
#include <cstddef>

namespace networktest
{
namespace
{

constexpr float EPSILON = 0.0001f;
constexpr float QUARTER = 0.25f;
constexpr float HALF = 0.5f;
constexpr float THREE_QUARTERS = 0.75f;
constexpr float ONE_AND_A_HALF = 1.5f;
constexpr float TWO = 2.f;
constexpr float THREE = 3.f;
constexpr uint32_t MOVE_INDEX = 0;
constexpr uint32_t LIFT_INDEX = 1;

void ExpectOk( const cmf::ValidationResult& result )
{
	EXPECT_TRUE( result ) << result.error;
}

bool NearlyEqual( const cmf::Transform& a, const cmf::Transform& b )
{
	const auto isNear = []( float x, float y ) { return std::abs( x - y ) < EPSILON; };
	return isNear( a.position.x, b.position.x ) && isNear( a.position.y, b.position.y ) && isNear( a.position.z, b.position.z ) && isNear( a.rotation.x, b.rotation.x ) && isNear( a.rotation.y, b.rotation.y ) && isNear( a.rotation.z, b.rotation.z ) && isNear( a.rotation.w, b.rotation.w ) && isNear( a.scale.x, b.scale.x ) && isNear( a.scale.y, b.scale.y ) && isNear( a.scale.z, b.scale.z );
}

void ExpectPose( const cmf::AnimationNetworkInstance& instance, const cmf::SkeletonPose& expected )
{
	const auto* actual = instance.GetLocalTransforms();
	ASSERT_NE( actual, nullptr );
	ASSERT_EQ( instance.GetBoneCount(), expected.boneTransforms.size() );
	for( uint32_t i = 0; i < instance.GetBoneCount(); ++i )
	{
		EXPECT_TRUE( NearlyEqual( actual[i], expected.boneTransforms[i] ) ) << "bone " << i << ": got (" << actual[i].position.x << ", " << actual[i].position.y << ")";
	}
}

cmf::can::Header& GetHeader( std::vector<uint8_t>& file )
{
	return *reinterpret_cast<cmf::can::Header*>( file.data() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): test access to the file header
}

cmf::can::NetworkData& GetNetworkData( std::vector<uint8_t>& file )
{
	return *reinterpret_cast<cmf::can::NetworkData*>( file.data() + GetHeader( file ).sections[0].offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): test access to the network section
}

void UpdateCrc( std::vector<uint8_t>& file )
{
	constexpr size_t crcEnd = offsetof( cmf::can::Header, crc32 ) + sizeof( uint32_t );
	GetHeader( file ).crc32 = cmf::ComputeCrc32( file.data() + crcEnd, file.size() - crcEnd );
}

cmf::AnimationTransition MakeTransition( uint32_t from, uint32_t to, cmf::AnimationTransitionCondition condition, std::string expression = {}, float timeout = 0.f, float duration = 0.f )
{
	cmf::AnimationTransition transition;
	transition.fromState = from;
	transition.toState = to;
	transition.condition = condition;
	transition.expression = std::move( expression );
	transition.timeout = timeout;
	transition.duration = duration;
	return transition;
}

cmf::AnimationTransition ExpressionTransition( uint32_t from, uint32_t to, std::string expression, float duration = 0.f )
{
	return MakeTransition( from, to, cmf::AnimationTransitionCondition::Expression, std::move( expression ), 0.f, duration );
}

/** A network whose root graph outputs a StateMachine node; states are added by the tests. */
struct StateMachineFixture : public ::testing::Test
{
	TestAsset asset;
	FakeLoader loader;
	cmf::AnimationNetworkBuilder builder{ loader };
	cmf::AnimationGraphBuilder& graph = builder.GetRootGraph();
	cmf::AnimationNodeId output;
	cmf::AnimationNodeId machine;
	cmf::AnimationNetwork network{ loader };

	StateMachineFixture()
	{
		loader.AddAsset( ASSET_PATH, &asset.data );
		output = SetUpNetwork( builder );
		machine = graph.AddNode( cmf::AnimationNodeType::StateMachine );
	}

	uint32_t AddParameter( const char* name, float defaultValue = 0.f )
	{
		cmf::AnimationParameter parameter;
		parameter.name = name;
		parameter.defaultValue = defaultValue;
		parameter.minValue = -1000.f; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers): wide test range
		parameter.maxValue = 1000.f; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers): wide test range
		return builder.AddParameter( parameter );
	}

	/** Adds a state whose graph outputs the node created by makeNode. */
	template <typename MakeNode>
	static uint32_t AddState( cmf::AnimationGraphBuilder& owner, cmf::AnimationNodeId node, const char* name, const MakeNode& makeNode ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
	{
		const auto state = owner.AddState( node, name );
		EXPECT_NE( state, cmf::AnimationGraphBuilder::INVALID_INDEX );
		auto& stateGraph = *owner.GetStateGraph( node, state );
		const auto stateOutput = stateGraph.AddNode( cmf::AnimationNodeType::Output );
		ExpectOk( stateGraph.Connect( { makeNode( stateGraph ), 0 }, { stateOutput, 0 } ) );
		return state;
	}

	/** Adds a state playing an animation; loop is the constant of the loop input. */
	static uint32_t AddSampleState( cmf::AnimationGraphBuilder& owner, cmf::AnimationNodeId node, const char* name, const char* animation, bool loop = true )
	{
		return AddState( owner, node, name, [animation, loop]( cmf::AnimationGraphBuilder& stateGraph ) {
			const auto sample = stateGraph.AddNode( cmf::AnimationNodeType::AnimationSample );
			ExpectOk( stateGraph.SetAnimation( sample, ASSET_PATH, animation ) );
			ExpectOk( stateGraph.SetInputConstant( { sample, 1 }, loop ? 1.f : 0.f ) );
			return sample;
		} );
	}

	uint32_t AddSampleState( const char* name, const char* animation, bool loop = true )
	{
		return AddSampleState( graph, machine, name, animation, loop );
	}

	uint32_t AddRestPoseState( const char* name )
	{
		return AddState( graph, machine, name, []( cmf::AnimationGraphBuilder& stateGraph ) { return stateGraph.AddNode( cmf::AnimationNodeType::RestPose ); } );
	}

	void LoadNetwork( cmf::AnimationNodeId result )
	{
		ExpectOk( graph.Connect( { result, 0 }, { output, 0 } ) );
		const auto errors = builder.GetErrors();
		EXPECT_TRUE( errors.empty() ) << ( errors.empty() ? "" : errors[0] );
		ExpectOk( network.Load( SaveNetwork( builder ) ) );
		const auto runtimeErrors = network.GetErrors();
		EXPECT_TRUE( network.IsReady() ) << ( runtimeErrors.empty() ? "" : runtimeErrors[0] );
	}

	void LoadNetwork()
	{
		LoadNetwork( machine );
	}

	/** Returns the graph and node index of a node in the saved network. */
	[[nodiscard]] std::pair<uint32_t, uint32_t> Locate( const cmf::AnimationGraphBuilder& owner, cmf::AnimationNodeId node ) const
	{
		const auto layout = builder.GetSaveLayout();
		for( uint32_t g = 0; g < layout.size(); ++g )
		{
			if( layout[g].graph != &owner )
			{
				continue;
			}
			for( uint32_t n = 0; n < layout[g].nodes.size(); ++n )
			{
				if( layout[g].nodes[n] == node )
				{
					return { g, n };
				}
			}
		}
		return { cmf::can::NO_INDEX, cmf::can::NO_INDEX };
	}

	[[nodiscard]] uint32_t GetCurrentState( const cmf::AnimationNetworkInstance& instance, const cmf::AnimationGraphBuilder& owner, cmf::AnimationNodeId node ) const
	{
		const auto [g, n] = Locate( owner, node );
		return instance.GetCurrentState( g, n );
	}

	[[nodiscard]] uint32_t GetCurrentState( const cmf::AnimationNetworkInstance& instance ) const
	{
		return GetCurrentState( instance, graph, machine );
	}

	[[nodiscard]] cmf::SkeletonPose RestPose() const
	{
		cmf::SkeletonPose pose;
		cmf::RestPose( pose, asset.data.skeletons[0] );
		return pose;
	}

	[[nodiscard]] cmf::SkeletonPose Sample( uint32_t animation, float time ) const
	{
		auto pose = RestPose();
		const auto& data = asset.data.animations[animation];
		cmf::SampleAnimation( pose, data, data.curves, time );
		return pose;
	}

	static cmf::SkeletonPose Blend( const cmf::SkeletonPose& a, const cmf::SkeletonPose& b, float alpha )
	{
		cmf::SkeletonPose result;
		cmf::BlendPoses( result, a, b, alpha );
		return result;
	}

	[[nodiscard]] const cmf::AnimationStateMachineSettings& Settings() const
	{
		return *graph.GetSettings<cmf::AnimationStateMachineSettings>( machine );
	}
};

}

// Builder

TEST_F( StateMachineFixture, StateMachineNodeHasOnePoseOutput )
{
	const auto info = cmf::GetAnimationNodeTypeInfo( cmf::AnimationNodeType::StateMachine );
	EXPECT_STREQ( info.name, "StateMachine" );
	EXPECT_EQ( info.inputCount, 0u );
	EXPECT_TRUE( info.hasOutput );
	EXPECT_EQ( info.outputType, cmf::AnimationSocketType::Pose );
	EXPECT_TRUE( info.hasSettings );
	EXPECT_EQ( graph.GetInputCount( machine ), 0u );
	EXPECT_EQ( graph.GetOutputCount( machine ), 1u );
	EXPECT_EQ( graph.GetOutputName( { machine, 0 } ), "pose" );
}

TEST_F( StateMachineFixture, StateGraphsAreLikeTheRootGraph )
{
	const auto state = graph.AddState( machine, "Idle" );
	auto* stateGraph = graph.GetStateGraph( machine, state );
	ASSERT_NE( stateGraph, nullptr );
	EXPECT_EQ( stateGraph->GetParent(), &graph );
	EXPECT_FALSE( stateGraph->AddNode( cmf::AnimationNodeType::SubgraphInput ).IsSet() );
	const auto stateOutput = stateGraph->AddNode( cmf::AnimationNodeType::Output );
	EXPECT_TRUE( stateOutput.IsSet() );
	EXPECT_FALSE( stateGraph->AddNode( cmf::AnimationNodeType::Output ).IsSet() );
	EXPECT_FALSE( stateGraph->SetInterfaceSocket( stateOutput, "value", cmf::AnimationSocketType::Float ) );
	// Adding nodes to the state graph doesn't change the sockets of the StateMachine node
	EXPECT_EQ( graph.GetInputCount( machine ), 0u );
	EXPECT_EQ( graph.GetStateGraph( machine, 1 ), nullptr );
	EXPECT_EQ( graph.GetSubgraph( machine ), nullptr );
}

TEST_F( StateMachineFixture, StateOperationsRequireAStateMachineNode )
{
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	EXPECT_EQ( graph.AddState( rest, "A" ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_FALSE( graph.SetStartState( rest, 0 ) );
	EXPECT_FALSE( graph.SetResumeOnActivate( rest, true ) );
	EXPECT_EQ( graph.AddTransition( rest, {} ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_FALSE( graph.RemoveState( machine, 0 ) );
	EXPECT_FALSE( graph.SetStartState( machine, 0 ) );
}

TEST_F( StateMachineFixture, RemovingAStateUpdatesTransitionsAndTheStartState )
{
	const auto a = graph.AddState( machine, "A" );
	const auto b = graph.AddState( machine, "B" );
	const auto c = graph.AddState( machine, "C" );
	const auto* graphC = graph.GetStateGraph( machine, c );
	EXPECT_NE( graph.AddTransition( machine, ExpressionTransition( a, b, "1" ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_NE( graph.AddTransition( machine, ExpressionTransition( b, c, "1" ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_NE( graph.AddTransition( machine, ExpressionTransition( c, a, "1" ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	ExpectOk( graph.SetStartState( machine, c ) );
	ExpectOk( graph.SetStateName( machine, c, "Last" ) );

	ExpectOk( graph.RemoveState( machine, b ) );
	const auto& settings = Settings();
	ASSERT_EQ( settings.stateNames.size(), 2u );
	EXPECT_EQ( settings.stateNames[0], "A" );
	EXPECT_EQ( settings.stateNames[1], "Last" );
	EXPECT_EQ( settings.startState, 1u );
	EXPECT_EQ( graph.GetStateGraph( machine, 1 ), graphC );
	ASSERT_EQ( settings.transitions.size(), 1u );
	EXPECT_EQ( settings.transitions[0].fromState, 1u );
	EXPECT_EQ( settings.transitions[0].toState, 0u );

	// Removing the start state makes the first state the start state
	ExpectOk( graph.RemoveState( machine, 1 ) );
	EXPECT_EQ( Settings().startState, 0u );
	EXPECT_TRUE( Settings().transitions.empty() );
	EXPECT_FALSE( graph.RemoveState( machine, 1 ) );
}

TEST_F( StateMachineFixture, InvalidTransitionsAreRejected )
{
	AddParameter( "speed" );
	graph.AddState( machine, "A" );
	graph.AddState( machine, "B" );
	EXPECT_EQ( graph.AddTransition( machine, ExpressionTransition( 0, 2, "1" ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_EQ( graph.AddTransition( machine, ExpressionTransition( 0, 1, "1", -1.f ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_EQ( graph.AddTransition( machine, MakeTransition( 0, 1, cmf::AnimationTransitionCondition::Timeout, {}, -1.f ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
	EXPECT_FALSE( graph.SetTransition( machine, 0, ExpressionTransition( 0, 1, "1" ) ) );

	// Expressions can use the parameters and the state time
	const auto index = graph.AddTransition( machine, ExpressionTransition( 0, 1, "speed > 1 && stateTime > 0.5" ) );
	ASSERT_EQ( index, 0u );
	ExpectOk( graph.ValidateTransition( machine, Settings().transitions[index] ) );
	EXPECT_FALSE( graph.ValidateTransition( machine, ExpressionTransition( 0, 1, "speed >" ) ) );
	EXPECT_FALSE( graph.ValidateTransition( machine, ExpressionTransition( 0, 1, "unknown > 1" ) ) );
	EXPECT_FALSE( graph.ValidateTransition( machine, ExpressionTransition( 0, 2, "1" ) ) );
	// Expressions are only parsed for Expression conditions
	ExpectOk( graph.ValidateTransition( machine, MakeTransition( 1, 0, cmf::AnimationTransitionCondition::AnimationFinished, "speed >" ) ) );

	// Expressions that don't parse are kept, so that documents can store them, and reported by GetErrors
	EXPECT_EQ( graph.AddTransition( machine, ExpressionTransition( 1, 1, "speed >" ) ), 1u );
	EXPECT_EQ( Settings().transitions[1].expression, "speed >" );
	ExpectOk( graph.SetTransition( machine, index, ExpressionTransition( 0, 1, "unknown" ) ) );
	EXPECT_EQ( Settings().transitions[index].expression, "unknown" );
	EXPECT_FALSE( graph.SetTransition( machine, index, ExpressionTransition( 0, 1, "1", -1.f ) ) );
	EXPECT_EQ( Settings().transitions[index].expression, "unknown" );
	EXPECT_FALSE( graph.SetTransition( machine, 2, ExpressionTransition( 0, 1, "1" ) ) );

	ExpectOk( graph.RemoveTransition( machine, 0 ) );
	ASSERT_EQ( Settings().transitions.size(), 1u );
	EXPECT_EQ( Settings().transitions[0].fromState, 1u );
	EXPECT_FALSE( graph.RemoveTransition( machine, 1 ) );
}

TEST_F( StateMachineFixture, StateProblemsAreReported )
{
	ExpectOk( graph.Connect( { machine, 0 }, { output, 0 } ) );
	auto hasError = [this]( const std::string& text ) {
		const auto errors = builder.GetErrors();
		return std::any_of( errors.begin(), errors.end(), [&text]( const std::string& error ) { return error.find( text ) != std::string::npos; } );
	};
	EXPECT_TRUE( hasError( "has no states" ) );

	const auto state = graph.AddState( machine, "Idle" );
	EXPECT_TRUE( hasError( "state #0 'Idle' has no Output node" ) );
	auto& stateGraph = *graph.GetStateGraph( machine, state );
	const auto stateOutput = stateGraph.AddNode( cmf::AnimationNodeType::Output );
	EXPECT_TRUE( hasError( "root/StateMachine #1/state #0 'Idle': Output #0: input 'pose' is not connected" ) ) << ::testing::PrintToString( builder.GetErrors() );
	ExpectOk( stateGraph.Connect( { stateGraph.AddNode( cmf::AnimationNodeType::RestPose ), 0 }, { stateOutput, 0 } ) );
	EXPECT_TRUE( builder.GetErrors().empty() );
}

TEST_F( StateMachineFixture, TransitionExpressionProblemsAreReported )
{
	ExpectOk( graph.Connect( { machine, 0 }, { output, 0 } ) );
	const auto state = AddRestPoseState( "Idle" );
	auto hasError = [this]( const std::string& text ) {
		const auto errors = builder.GetErrors();
		return std::any_of( errors.begin(), errors.end(), [&text]( const std::string& error ) { return error.find( text ) != std::string::npos; } );
	};
	graph.AddTransition( machine, ExpressionTransition( state, state, "" ) );
	EXPECT_TRUE( hasError( "no expression is set" ) );

	// Renaming a parameter breaks expressions that use it
	const auto speed = AddParameter( "speed" );
	ExpectOk( graph.SetTransition( machine, 0, ExpressionTransition( state, state, "speed > 1" ) ) );
	EXPECT_TRUE( builder.GetErrors().empty() );
	cmf::AnimationParameter renamed = builder.GetParameters()[speed];
	renamed.name = "velocity";
	ExpectOk( builder.SetParameter( speed, renamed ) );
	EXPECT_TRUE( hasError( "transition 0 (state #0 'Idle' to state #0 'Idle'): Invalid expression" ) ) << ::testing::PrintToString( builder.GetErrors() );
}

TEST_F( StateMachineFixture, StateGraphsAreSaved )
{
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Jump", LIFT_ANIMATION, false );
	ExpectOk( graph.SetStartState( machine, 1 ) );
	ExpectOk( graph.SetResumeOnActivate( machine, true ) );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "stateTime > 1", HALF ) );
	graph.AddTransition( machine, MakeTransition( 1, 0, cmf::AnimationTransitionCondition::AnimationFinished ) );
	graph.AddTransition( machine, MakeTransition( 1, 1, cmf::AnimationTransitionCondition::Timeout, {}, THREE ) );
	ExpectOk( graph.Connect( { machine, 0 }, { output, 0 } ) );

	// The save layout lists the state graphs after the root graph
	const auto layout = builder.GetSaveLayout();
	ASSERT_EQ( layout.size(), 3u );
	EXPECT_EQ( layout[1].graph, graph.GetStateGraph( machine, 0 ) );
	EXPECT_EQ( layout[2].graph, graph.GetStateGraph( machine, 1 ) );

	auto file = SaveNetwork( builder );
	const auto& data = GetNetworkData( file );
	ASSERT_EQ( data.graphs.size(), 3u );
	const auto& root = data.graphs[0];
	ASSERT_EQ( root.stateMachineNodes.size(), 1u );
	const auto& node = root.stateMachineNodes[0];
	EXPECT_EQ( node.startState, 1u );
	EXPECT_EQ( node.flags, cmf::can::STATE_MACHINE_RESUME_ON_ACTIVATE );
	ASSERT_EQ( node.states.size(), 2u );
	EXPECT_EQ( cmf::ToStdString( node.states[0].name ), "Walk" );
	EXPECT_EQ( node.states[0].graphIndex, 1u );
	EXPECT_EQ( node.states[1].graphIndex, 2u );
	ASSERT_EQ( node.transitions.size(), 3u );
	EXPECT_EQ( node.transitions[0].condition, cmf::can::TransitionCondition::Expression );
	EXPECT_EQ( cmf::ToStdString( node.transitions[0].expression ), "stateTime > 1" );
	EXPECT_EQ( node.transitions[0].duration, HALF );
	EXPECT_EQ( node.transitions[2].timeout, THREE );

	ExpectOk( network.Load( file ) );
	EXPECT_TRUE( network.IsReady() );
}

TEST_F( StateMachineFixture, MalformedStateMachinesAreRejected )
{
	AddSampleState( "A", MOVE_ANIMATION );
	AddSampleState( "B", LIFT_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "1" ) );
	ExpectOk( graph.Connect( { machine, 0 }, { output, 0 } ) );
	const auto valid = SaveNetwork( builder );
	auto stateMachine = []( std::vector<uint8_t>& file ) -> cmf::can::StateMachineNode& { return GetNetworkData( file ).graphs[0].stateMachineNodes[0]; };

	auto file = valid;
	stateMachine( file ).startState = 2;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0 node 1: invalid start state" );

	file = valid;
	stateMachine( file ).transitions[0].toState = 2;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0 node 1: transition references an invalid state" );

	file = valid;
	stateMachine( file ).transitions[0].duration = -1.f;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0 node 1: invalid transition duration" );

	file = valid;
	stateMachine( file ).states[1].graphIndex = 1;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Every subgraph must be referenced by exactly one Subgraph node or state" );

	file = valid;
	stateMachine( file ).states[1].graphIndex = 0;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Subgraph node or state references an invalid graph" );

	ExpectOk( network.Load( valid ) );
}

// Evaluation

TEST_F( StateMachineFixture, MachineStartsInTheStartState )
{
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	ExpectOk( graph.SetStartState( machine, 1 ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	ASSERT_TRUE( instance.IsValid() );
	EXPECT_EQ( GetCurrentState( instance ), cmf::AnimationNetworkInstance::INVALID_STATE );
	ASSERT_TRUE( instance.Evaluate() );
	EXPECT_EQ( GetCurrentState( instance ), 1u );
	ExpectPose( instance, Sample( LIFT_INDEX, 0.f ) );
	instance.Update( HALF );
	ASSERT_TRUE( instance.Evaluate() );
	ExpectPose( instance, Sample( LIFT_INDEX, HALF ) );
	EXPECT_EQ( instance.GetCurrentState( 0, 0 ), cmf::AnimationNetworkInstance::INVALID_STATE );
}

TEST_F( StateMachineFixture, ExpressionTransitionsFollowParameters )
{
	const auto go = AddParameter( "go" );
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "go > 0.5" ) );
	graph.AddTransition( machine, ExpressionTransition( 1, 0, "go < 0.5" ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	instance.Update( HALF );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	ExpectPose( instance, Sample( MOVE_INDEX, HALF ) );

	instance.SetParameter( go, 1.f );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
	// Entering a state restarts its animations
	ExpectPose( instance, Sample( LIFT_INDEX, 0.f ) );

	instance.Update( HALF );
	instance.SetParameter( go, 0.f );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	ExpectPose( instance, Sample( MOVE_INDEX, 0.f ) );
}

TEST_F( StateMachineFixture, StateTimeIsAvailableInExpressions )
{
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "stateTime >= 1" ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.Update( THREE_QUARTERS );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	instance.Update( HALF );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
}

TEST_F( StateMachineFixture, ParameterNamedStateTimeHidesTheBuiltInVariable )
{
	const auto stateTime = AddParameter( "stateTime" );
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "stateTime >= 1" ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.Update( TWO );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	instance.SetParameter( stateTime, 1.f );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
}

TEST_F( StateMachineFixture, TimeoutTransitionsAreTakenAfterTheTimeout )
{
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	graph.AddTransition( machine, MakeTransition( 0, 1, cmf::AnimationTransitionCondition::Timeout, {}, 1.f ) );
	graph.AddTransition( machine, MakeTransition( 1, 0, cmf::AnimationTransitionCondition::Timeout, {}, HALF ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.Update( THREE_QUARTERS );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	instance.Update( THREE_QUARTERS );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
	// The state time restarts when a state is entered
	instance.Update( QUARTER );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
	instance.Update( QUARTER );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
}

TEST_F( StateMachineFixture, NonLoopingAnimationsFinishTheirState )
{
	AddSampleState( "Once", MOVE_ANIMATION, false );
	AddSampleState( "Loop", LIFT_ANIMATION, true );
	AddRestPoseState( "Rest" );
	graph.AddTransition( machine, MakeTransition( 0, 1, cmf::AnimationTransitionCondition::AnimationFinished ) );
	graph.AddTransition( machine, MakeTransition( 1, 2, cmf::AnimationTransitionCondition::AnimationFinished ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.Update( ONE_AND_A_HALF );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	instance.Update( 1.f );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
	ExpectPose( instance, Sample( LIFT_INDEX, 0.f ) );

	// Looping animations never finish
	for( int i = 0; i < 10; ++i ) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers): several animation cycles
	{
		instance.Update( HALF );
		instance.Evaluate();
	}
	EXPECT_EQ( GetCurrentState( instance ), 1u );
}

TEST_F( StateMachineFixture, StatesWithoutASingleAnimationNeverFinish )
{
	const auto blendState = AddState( graph, machine, "Blend", []( cmf::AnimationGraphBuilder& stateGraph ) {
		const auto blend = stateGraph.AddNode( cmf::AnimationNodeType::Blend );
		for( uint32_t input = 0; input < 2; ++input )
		{
			const auto sample = stateGraph.AddNode( cmf::AnimationNodeType::AnimationSample );
			ExpectOk( stateGraph.SetAnimation( sample, ASSET_PATH, MOVE_ANIMATION ) );
			ExpectOk( stateGraph.SetInputConstant( { sample, 1 }, 0.f ) );
			ExpectOk( stateGraph.Connect( { sample, 0 }, { blend, input } ) );
		}
		return blend;
	} );
	AddRestPoseState( "Rest" );
	graph.AddTransition( machine, MakeTransition( blendState, 1, cmf::AnimationTransitionCondition::AnimationFinished ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.Update( THREE );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
}

TEST_F( StateMachineFixture, TransitionsWithADurationBlendTheStates )
{
	const auto go = AddParameter( "go" );
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "go", 1.f ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.Update( QUARTER );
	instance.SetParameter( go, 1.f );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 1u );
	ExpectPose( instance, Sample( MOVE_INDEX, QUARTER ) );

	instance.Update( HALF );
	instance.Evaluate();
	ExpectPose( instance, Blend( Sample( MOVE_INDEX, THREE_QUARTERS ), Sample( LIFT_INDEX, HALF ), HALF ) );

	instance.Update( HALF );
	instance.Evaluate();
	ExpectPose( instance, Sample( LIFT_INDEX, 1.f ) );
}

TEST_F( StateMachineFixture, SeveralStatesCanFadeOutAtTheSameTime )
{
	const auto go = AddParameter( "go" );
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	AddRestPoseState( "Rest" );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "go == 1", 1.f ) );
	graph.AddTransition( machine, ExpressionTransition( 1, 2, "go == 2", 1.f ) );
	LoadNetwork();

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	instance.SetParameter( go, 1.f );
	instance.Evaluate();
	instance.Update( HALF );
	instance.SetParameter( go, TWO );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 2u );
	ExpectPose( instance, Blend( Sample( MOVE_INDEX, HALF ), Sample( LIFT_INDEX, HALF ), HALF ) );

	instance.Update( QUARTER );
	instance.Evaluate();
	ExpectPose( instance, Blend( Blend( Sample( MOVE_INDEX, THREE_QUARTERS ), Sample( LIFT_INDEX, THREE_QUARTERS ), THREE_QUARTERS ), RestPose(), QUARTER ) );

	// Walk is hidden once Lift has fully faded in
	instance.Update( QUARTER );
	instance.Evaluate();
	ExpectPose( instance, Blend( Sample( LIFT_INDEX, 1.f ), RestPose(), HALF ) );
}

TEST_F( StateMachineFixture, ReactivatedMachineRestartsOrResumes )
{
	const auto go = AddParameter( "go" );
	const auto weight = AddParameter( "weight", 1.f );
	AddSampleState( "Walk", MOVE_ANIMATION );
	AddSampleState( "Lift", LIFT_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 1, "go" ) );
	// The machine contributes to the pose only while weight is not 0
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto weightNode = graph.AddNode( cmf::AnimationNodeType::Parameter );
	ExpectOk( graph.SetParameter( weightNode, weight ) );
	ExpectOk( graph.Connect( { graph.AddNode( cmf::AnimationNodeType::RestPose ), 0 }, { blend, 0 } ) );
	ExpectOk( graph.Connect( { machine, 0 }, { blend, 1 } ) );
	ExpectOk( graph.Connect( { weightNode, 0 }, { blend, 2 } ) );
	LoadNetwork( blend );

	auto run = [&]( bool expectResume ) {
		cmf::AnimationNetworkInstance instance( network );
		instance.Evaluate();
		instance.SetParameter( go, 1.f );
		instance.Evaluate();
		instance.SetParameter( go, 0.f );
		EXPECT_EQ( GetCurrentState( instance ), 1u );
		instance.SetParameter( weight, 0.f );
		instance.Evaluate();
		ExpectPose( instance, RestPose() );
		instance.SetParameter( weight, 1.f );
		instance.Evaluate();
		EXPECT_EQ( GetCurrentState( instance ), expectResume ? 1u : 0u );
	};
	run( false );

	ExpectOk( graph.SetResumeOnActivate( machine, true ) );
	ExpectOk( network.Load( SaveNetwork( builder ) ) );
	run( true );
}

TEST_F( StateMachineFixture, StateMachinesCanBeNested )
{
	const auto inner = AddParameter( "inner" );
	cmf::AnimationNodeId innerMachine;
	cmf::AnimationGraphBuilder* outerState = nullptr;
	AddState( graph, machine, "Outer", [&]( cmf::AnimationGraphBuilder& stateGraph ) {
		outerState = &stateGraph;
		innerMachine = stateGraph.AddNode( cmf::AnimationNodeType::StateMachine );
		AddSampleState( stateGraph, innerMachine, "Walk", MOVE_ANIMATION );
		AddSampleState( stateGraph, innerMachine, "Lift", LIFT_ANIMATION );
		EXPECT_NE( stateGraph.AddTransition( innerMachine, ExpressionTransition( 0, 1, "inner" ) ), cmf::AnimationGraphBuilder::INVALID_INDEX );
		return innerMachine;
	} );
	LoadNetwork();
	EXPECT_EQ( builder.GetSaveLayout().size(), 4u );

	cmf::AnimationNetworkInstance instance( network );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance ), 0u );
	EXPECT_EQ( GetCurrentState( instance, *outerState, innerMachine ), 0u );
	ExpectPose( instance, Sample( MOVE_INDEX, 0.f ) );
	instance.SetParameter( inner, 1.f );
	instance.Evaluate();
	EXPECT_EQ( GetCurrentState( instance, *outerState, innerMachine ), 1u );
	ExpectPose( instance, Sample( LIFT_INDEX, 0.f ) );
}

TEST_F( StateMachineFixture, InvalidTransitionExpressionsPreventCompiling )
{
	const auto speed = AddParameter( "speed" );
	AddSampleState( "Walk", MOVE_ANIMATION );
	graph.AddTransition( machine, ExpressionTransition( 0, 0, "speed > 1" ) );
	ExpectOk( graph.Connect( { machine, 0 }, { output, 0 } ) );
	auto file = SaveNetwork( builder );
	// Rename the parameter in the file so that the expression no longer parses
	GetNetworkData( file ).parameters[speed].name.data()[0] = 'x';
	UpdateCrc( file );
	ExpectOk( network.Load( file ) );
	EXPECT_FALSE( network.IsReady() );
	const auto errors = network.GetErrors();
	ASSERT_EQ( errors.size(), 1u );
	EXPECT_NE( errors[0].find( "Invalid transition expression 'speed > 1'" ), std::string::npos );
}

}
