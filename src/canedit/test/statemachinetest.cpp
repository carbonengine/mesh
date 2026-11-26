// Copyright © 2026 CCP ehf.

#include "testutils.h"

#include "clipboard.h"

#include <nlohmann/json.hpp>

#include <cmf/animationnetworkinstance.h>

#include <fstream>
#include <iterator>
#include <set>

namespace canedittest
{
namespace
{

constexpr canedit::Vec2 IDLE_POSITION = { 10.f, 20.f };
constexpr canedit::Vec2 RUN_POSITION = { 300.f, 20.f };
constexpr canedit::Vec2 MOVE_OFFSET = { 5.f, -5.f };
constexpr canedit::Vec2 PASTE_ORIGIN = { 500.f, 500.f };
constexpr canedit::GraphView STATE_VIEW = { { 12.f, 34.f }, 0.75f };
constexpr float BLEND_DURATION = 0.25f;
constexpr canedit::Vec2 SAMPLE_POSITION = { -250.f, 0.f };
constexpr float RUN_SPEED = 2.f;
constexpr uint32_t INVALID_STATE_INDEX = 5;
/** More states than a row of the state grid holds. */
constexpr uint32_t GRID_STATE_COUNT = 5;

std::string ReadText( const std::filesystem::path& path )
{
	std::ifstream stream( path, std::ios::binary );
	return { std::istreambuf_iterator<char>( stream ), std::istreambuf_iterator<char>() };
}

/** A document whose root graph outputs a StateMachine node. */
struct StateMachineFixture : public EditorFixture
{
	cmf::AnimationNodeId machine;

	StateMachineFixture()
	{
		machine = Root().AddNode( cmf::AnimationNodeType::StateMachine );
		document.SetNodePosition( Root(), machine, {} );
		EXPECT_TRUE( Root().Connect( { machine, 0 }, { Output(), 0 } ) );
		document.MarkUnmodified();
	}

	[[nodiscard]] const cmf::AnimationStateMachineSettings& Settings()
	{
		return *Root().GetSettings<cmf::AnimationStateMachineSettings>( machine );
	}

	/** Connects an AnimationSample node playing an animation to the Output node of a state. */
	void PlayInState( uint32_t state, const char* animation )
	{
		auto& graph = *Root().GetStateGraph( machine, state );
		const auto sample = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
		document.SetNodePosition( graph, sample, SAMPLE_POSITION );
		ASSERT_TRUE( graph.SetAnimation( sample, CHARACTER_REFERENCE, animation ) );
		ASSERT_TRUE( graph.Connect( { sample, 0 }, { graph.GetOutputNode(), 0 } ) );
	}

	/** Opens the state machine view and adds two states with animations and a transition between them. */
	void BuildIdleAndRun()
	{
		ASSERT_TRUE( session.EnterSubgraph( machine ) );
		ASSERT_EQ( session.AddState( IDLE_POSITION ), 0u );
		ASSERT_EQ( session.AddState( RUN_POSITION ), 1u );
		PlayInState( 0, networktest::MOVE_ANIMATION );
		PlayInState( 1, networktest::LIFT_ANIMATION );
		const auto speed = document.GetNetwork().AddParameter( { "speed", 0.f, 0.f, 10.f } ); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers): parameter range
		ASSERT_NE( speed, cmf::AnimationNetworkBuilder::INVALID_PARAMETER );
		ASSERT_EQ( session.AddTransition( 0, 1 ), 0u );
		cmf::AnimationTransition transition = Settings().transitions[0];
		transition.expression = "speed > 1";
		transition.duration = BLEND_DURATION;
		ASSERT_TRUE( Root().SetTransition( machine, 0, transition ) );
	}
};

}

TEST_F( StateMachineFixture, StateMachineViewIsOpenedFromTheNode )
{
	EXPECT_TRUE( session.CanEnter( machine ) );
	EXPECT_FALSE( session.CanEnter( Output() ) );
	ASSERT_TRUE( session.EnterSubgraph( machine ) );
	EXPECT_EQ( session.GetStateMachine(), machine );
	EXPECT_EQ( &session.GetGraph(), &Root() ) << "the graph of a state machine view is the one containing the node";
	EXPECT_FALSE( session.CanEnter( machine ) );

	const auto state = session.AddState( IDLE_POSITION );
	ASSERT_EQ( state, 0u );
	EXPECT_EQ( Settings().stateNames[0], "State 1" );
	EXPECT_TRUE( session.IsStateSelected( state ) );
	const auto* stateGraph = Root().GetStateGraph( machine, state );
	ASSERT_NE( stateGraph, nullptr );
	EXPECT_TRUE( stateGraph->GetOutputNode().IsSet() ) << "new states have an Output node";
	EXPECT_EQ( document.GetStatePosition( *stateGraph ), IDLE_POSITION );

	ASSERT_TRUE( session.EnterState( state ) );
	EXPECT_FALSE( session.GetStateMachine().IsSet() );
	EXPECT_EQ( &session.GetGraph(), stateGraph );
	ASSERT_EQ( session.GetGraphPath().size(), 1u );
	EXPECT_EQ( session.GetGraphPath()[0].state, state );

	// Leaving a state goes back to the state machine view, leaving that goes back to the graph
	ASSERT_TRUE( session.LeaveSubgraph() );
	EXPECT_EQ( session.GetStateMachine(), machine );
	EXPECT_TRUE( session.IsStateSelected( state ) );
	ASSERT_TRUE( session.LeaveSubgraph() );
	EXPECT_TRUE( session.GetGraphPath().empty() );
	EXPECT_TRUE( session.IsSelected( machine ) );
}

TEST_F( StateMachineFixture, StateOperationsAreUndoable )
{
	BuildIdleAndRun();
	EXPECT_EQ( session.GetSelectedTransition(), std::optional<uint32_t>( 0 ) );
	ASSERT_EQ( Settings().stateNames.size(), 2u );

	session.SelectState( 1 );
	session.MoveStates( session.GetSelectedStates(), MOVE_OFFSET );
	const auto* run = Root().GetStateGraph( machine, 1 );
	EXPECT_EQ( document.GetStatePosition( *run ), ( canedit::Vec2{ RUN_POSITION.x + MOVE_OFFSET.x, RUN_POSITION.y + MOVE_OFFSET.y } ) );
	ASSERT_TRUE( session.SetStartState( 1 ) );
	EXPECT_EQ( Settings().startState, 1u );

	session.SelectState( 0 );
	session.DeleteSelection();
	ASSERT_EQ( Settings().stateNames.size(), 1u );
	EXPECT_TRUE( Settings().transitions.empty() );
	EXPECT_EQ( Settings().startState, 0u );

	ASSERT_TRUE( session.Undo() );
	ASSERT_EQ( Settings().stateNames.size(), 2u );
	EXPECT_EQ( Settings().transitions.size(), 1u );
	EXPECT_EQ( Settings().startState, 1u );
	EXPECT_EQ( session.GetStateMachine(), machine ) << "undo keeps the state machine view open";
	EXPECT_EQ( session.GetSelectedStates(), std::vector<uint32_t>{ 0 } );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( Settings().startState, 0u );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( document.GetStatePosition( *Root().GetStateGraph( machine, 1 ) ), RUN_POSITION );

	ASSERT_TRUE( session.Redo() );
	ASSERT_TRUE( session.Redo() );
	ASSERT_TRUE( session.Redo() );
	EXPECT_EQ( Settings().stateNames.size(), 1u );

	// Deleting a transition keeps the states
	ASSERT_TRUE( session.Undo() );
	session.SelectTransition( 0 );
	session.DeleteSelection();
	EXPECT_EQ( Settings().stateNames.size(), 2u );
	EXPECT_TRUE( Settings().transitions.empty() );
}

TEST_F( StateMachineFixture, UndoInsideAStateKeepsTheOpenGraph )
{
	BuildIdleAndRun();
	ASSERT_TRUE( session.EnterState( 1 ) );
	const auto rest = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	ASSERT_TRUE( rest.IsSet() );
	const auto other = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	session.SetSelection( { rest } );
	ASSERT_TRUE( session.Undo() );
	ASSERT_EQ( session.GetGraphPath().size(), 1u );
	EXPECT_EQ( session.GetGraphPath()[0].state, 1u );
	EXPECT_EQ( &session.GetGraph(), Root().GetStateGraph( machine, 1 ) );
	EXPECT_EQ( session.GetGraph().GetNodes().size(), 3u );
	EXPECT_EQ( session.GetSelection().size(), 1u );
	static_cast<void>( other );
}

TEST_F( StateMachineFixture, StateMachinesSurviveSavingAndLoading )
{
	BuildIdleAndRun();
	ASSERT_TRUE( Root().AddTransition( machine, { 1, 0, cmf::AnimationTransitionCondition::AnimationFinished, {}, 0.f, 0.f } ) != cmf::AnimationGraphBuilder::INVALID_INDEX );
	ASSERT_TRUE( Root().AddTransition( machine, { 1, 1, cmf::AnimationTransitionCondition::Timeout, {}, 2.f, 0.f } ) != cmf::AnimationGraphBuilder::INVALID_INDEX );
	ASSERT_TRUE( Root().SetStartState( machine, 1 ) );
	ASSERT_TRUE( Root().SetResumeOnActivate( machine, true ) );
	ASSERT_TRUE( Root().SetStateName( machine, 0, "Idle" ) );
	document.SetStateMachineView( Root(), machine, STATE_VIEW );
	const auto savedNetwork = networktest::SaveNetwork( document.GetNetwork() );

	const auto path = directory.GetPath() / "machine.cane";
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	const auto json = nlohmann::json::parse( ReadText( path ) );
	const auto& node = json["graph"]["nodes"][1];
	EXPECT_EQ( node["type"], "StateMachine" );
	EXPECT_EQ( node["states"].size(), 2u );
	EXPECT_EQ( node["states"][0]["name"], "Idle" );
	EXPECT_EQ( node["transitions"][0]["expression"], "speed > 1" );
	EXPECT_EQ( node["transitions"][1]["condition"], "AnimationFinished" );
	EXPECT_EQ( node["settings"]["startState"], 1 );

	canedit::Document loaded( loader, resolver );
	std::vector<std::string> warnings;
	ASSERT_TRUE( loaded.LoadFile( path, error, &warnings ) ) << error;
	EXPECT_TRUE( warnings.empty() ) << warnings.front();
	EXPECT_EQ( networktest::SaveNetwork( loaded.GetNetwork() ), savedNetwork ) << "the network is stored completely";
	const auto& loadedRoot = loaded.GetNetwork().GetRootGraph();
	const auto loadedMachine = loadedRoot.GetNodes()[1];
	EXPECT_EQ( loaded.GetStatePosition( *loadedRoot.GetStateGraph( loadedMachine, 1 ) ), RUN_POSITION );
	EXPECT_EQ( loaded.GetStateMachineView( loadedRoot, loadedMachine ).pan, STATE_VIEW.pan );
	EXPECT_EQ( loaded.GetStateMachineView( loadedRoot, loadedMachine ).zoom, STATE_VIEW.zoom );

	const auto again = directory.GetPath() / "again.cane";
	ASSERT_TRUE( loaded.SaveFile( again, error ) ) << error;
	EXPECT_EQ( ReadText( again ), ReadText( path ) );
}

TEST_F( StateMachineFixture, ExpressionsThatDontParseAreKept )
{
	BuildIdleAndRun();
	// Renaming the parameter breaks the expression; undo and saving must keep its text
	auto parameter = document.GetNetwork().GetParameters()[0];
	parameter.name = "velocity";
	{
		const canedit::ScopedEdit edit( session );
		ASSERT_TRUE( document.GetNetwork().SetParameter( 0, parameter ) );
	}
	EXPECT_FALSE( document.GetNetwork().GetErrors().empty() );
	session.SelectState( 0 );
	session.MoveStates( { 0 }, MOVE_OFFSET );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( Settings().transitions[0].expression, "speed > 1" );
}

TEST_F( StateMachineFixture, InvalidTransitionsAreSkippedWithWarnings )
{
	BuildIdleAndRun();
	auto json = nlohmann::json::parse( document.Serialize() );
	auto& transitions = json["graph"]["nodes"][1]["transitions"];
	transitions[0]["to"] = INVALID_STATE_INDEX;
	transitions.push_back( { { "from", 0 }, { "to", 1 }, { "condition", "Sometimes" } } );
	std::string error;
	std::vector<std::string> warnings;
	ASSERT_TRUE( document.Deserialize( json.dump(), error, &warnings ) ) << error;
	ASSERT_EQ( warnings.size(), 2u );
	EXPECT_EQ( warnings[0], "Transition skipped: Transition references an invalid state" );
	EXPECT_EQ( warnings[1], "Transition skipped: unknown condition 'Sometimes'" );
	EXPECT_TRUE( Settings().transitions.empty() );
}

TEST_F( StateMachineFixture, StateMachinesArePastedWithTheirContents )
{
	BuildIdleAndRun();
	ASSERT_TRUE( session.LeaveSubgraph() );
	session.SetSelection( { machine } );
	const auto text = session.CopySelection();
	ASSERT_FALSE( text.empty() );

	// Pasting into an empty network adds the parameter the transition uses
	canedit::Document other( loader, resolver );
	other.New();
	other.GetNetwork().SetSkeleton( CHARACTER_REFERENCE, networktest::SKELETON_NAME );
	const auto result = canedit::PasteNodes( other, other.GetNetwork().GetRootGraph(), text, PASTE_ORIGIN );
	ASSERT_TRUE( result.error.empty() ) << result.error;
	EXPECT_TRUE( result.warnings.empty() );
	ASSERT_EQ( result.nodes.size(), 1u );
	const auto& otherRoot = other.GetNetwork().GetRootGraph();
	const auto* settings = otherRoot.GetSettings<cmf::AnimationStateMachineSettings>( result.nodes[0] );
	ASSERT_NE( settings, nullptr );
	EXPECT_EQ( settings->stateNames.size(), 2u );
	ASSERT_EQ( settings->transitions.size(), 1u );
	EXPECT_EQ( settings->transitions[0].duration, BLEND_DURATION );
	EXPECT_EQ( other.GetNetwork().FindParameter( "speed" ), 0u );
	EXPECT_EQ( otherRoot.GetStateGraph( result.nodes[0], 1 )->GetNodes().size(), 2u );
	EXPECT_EQ( other.GetStatePosition( *otherRoot.GetStateGraph( result.nodes[0], 1 ) ), RUN_POSITION );

	// Nodes can't be pasted into the state machine view
	ASSERT_TRUE( session.EnterSubgraph( machine ) );
	EXPECT_FALSE( session.Paste( text, PASTE_ORIGIN ).error.empty() );
	EXPECT_TRUE( session.CopySelection().empty() );
}

TEST_F( StateMachineFixture, ExportedStateMachinesRun )
{
	BuildIdleAndRun();
	const auto path = directory.GetPath() / "machine.can";
	std::string error;
	ASSERT_TRUE( document.Export( path, error ) ) << error;
	std::ifstream stream( path, std::ios::binary );
	const std::vector<uint8_t> file( ( std::istreambuf_iterator<char>( stream ) ), std::istreambuf_iterator<char>() );
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( file ) );
	ASSERT_TRUE( network.IsReady() );

	cmf::AnimationNetworkInstance instance( network );
	const auto location = document.Locate( Root(), machine ).value_or( canedit::NodeLocation{ cmf::can::NO_INDEX, cmf::can::NO_INDEX } );
	ASSERT_TRUE( instance.Evaluate() );
	EXPECT_EQ( instance.GetCurrentState( location.graph, location.node ), 0u );
	instance.SetParameter( instance.FindParameter( "speed" ), RUN_SPEED );
	ASSERT_TRUE( instance.Evaluate() );
	EXPECT_EQ( instance.GetCurrentState( location.graph, location.node ), 1u );
}

TEST_F( StateMachineFixture, StatesAreArrangedInAGrid )
{
	ASSERT_TRUE( session.EnterSubgraph( machine ) );
	for( uint32_t i = 0; i < GRID_STATE_COUNT; ++i )
	{
		session.AddState( IDLE_POSITION );
	}
	session.AutoLayout();
	std::set<std::pair<float, float>> positions;
	for( uint32_t state = 0; state < GRID_STATE_COUNT; ++state )
	{
		const auto position = document.GetStatePosition( *Root().GetStateGraph( machine, state ) );
		ASSERT_TRUE( position.has_value() );
		const auto value = position.value_or( canedit::Vec2{} );
		positions.insert( { value.x, value.y } );
	}
	EXPECT_EQ( positions.size(), GRID_STATE_COUNT ) << "no two states share a position";
}

}
