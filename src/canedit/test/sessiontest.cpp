// Copyright © 2026 CCP ehf.

#include "testutils.h"

namespace canedittest
{
namespace
{

using Vec2 = canedit::Vec2;

constexpr Vec2 START = { 100.f, 50.f };
constexpr Vec2 STEP = { 10.f, 0.f };
constexpr canedit::GraphView ZOOMED_VIEW = { { 7.f, 8.f }, 2.f };

TEST_F( EditorFixture, UndoAndRedoRestoreTheNetworkAndPositions )
{
	const auto sample = session.AddNode( cmf::AnimationNodeType::AnimationSample, START );
	ASSERT_TRUE( sample.IsSet() );
	EXPECT_TRUE( session.IsSelected( sample ) );
	session.MoveNodes( { sample }, STEP );
	ASSERT_TRUE( session.Connect( { sample, 0 }, { Output(), 0 } ) );
	EXPECT_EQ( Root().GetEdges().size(), 1u );

	ASSERT_TRUE( session.Undo() );
	EXPECT_TRUE( Root().GetEdges().empty() );
	ASSERT_TRUE( session.Undo() );
	const auto restored = Root().GetNodes()[1];
	EXPECT_EQ( document.GetNodePosition( Root(), restored ), START );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( Root().GetNodes().size(), 1u );
	EXPECT_FALSE( session.CanUndo() );

	ASSERT_TRUE( session.Redo() );
	ASSERT_TRUE( session.Redo() );
	ASSERT_TRUE( session.Redo() );
	EXPECT_FALSE( session.CanRedo() );
	ASSERT_EQ( Root().GetNodes().size(), 2u );
	EXPECT_EQ( document.GetNodePosition( Root(), Root().GetNodes()[1] ), ( Vec2{ START.x + STEP.x, START.y } ) );
	EXPECT_EQ( Root().GetEdges().size(), 1u );
}

TEST_F( EditorFixture, ADragIsOneUndoStep )
{
	const auto node = session.AddNode( cmf::AnimationNodeType::RestPose, START );
	session.BeginEdit();
	for( int i = 0; i < 3; ++i )
	{
		session.MoveNodes( { node }, STEP );
	}
	session.EndEdit();
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( document.GetNodePosition( Root(), Root().GetNodes()[1] ), START );
}

TEST_F( EditorFixture, EditsWithoutChangesAreNotRecorded )
{
	session.BeginEdit();
	session.EndEdit();
	EXPECT_FALSE( session.CanUndo() );
}

TEST_F( EditorFixture, ConnectingAnInputReplacesItsEdge )
{
	const auto first = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	const auto second = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	const auto value = session.AddNode( cmf::AnimationNodeType::Expression, {} );
	ASSERT_TRUE( session.Connect( { first, 0 }, { Output(), 0 } ) );
	ASSERT_TRUE( session.Connect( { second, 0 }, { Output(), 0 } ) );
	ASSERT_EQ( Root().GetEdges().size(), 1u );
	EXPECT_EQ( Root().GetConnection( { Output(), 0 } ), ( cmf::AnimationSocketRef{ second, 0 } ) );

	// A failed connection keeps the existing edge and isn't an undo step
	const auto undoCount = session.CanUndo();
	EXPECT_FALSE( session.Connect( { value, 0 }, { Output(), 0 } ) );
	EXPECT_EQ( Root().GetConnection( { Output(), 0 } ), ( cmf::AnimationSocketRef{ second, 0 } ) );
	EXPECT_EQ( session.CanUndo(), undoCount );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( Root().GetConnection( { Output(), 0 } ), ( cmf::AnimationSocketRef{ Root().GetNodes()[1], 0 } ) );
}

TEST_F( EditorFixture, DeleteRemovesSelectedNodesOrEdge )
{
	const auto rest = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	ASSERT_TRUE( session.Connect( { rest, 0 }, { Output(), 0 } ) );
	session.SelectEdge( { Output(), 0 } );
	session.DeleteSelection();
	EXPECT_TRUE( Root().GetEdges().empty() );
	EXPECT_EQ( Root().GetNodes().size(), 2u );

	session.SetSelection( { rest, Output() } );
	session.DeleteSelection();
	EXPECT_TRUE( Root().GetNodes().empty() );
	EXPECT_TRUE( session.GetSelection().empty() );
}

TEST_F( EditorFixture, UndoKeepsTheOpenGraphAndSelection )
{
	const auto subgraphNode = session.AddNode( cmf::AnimationNodeType::Subgraph, {} );
	ASSERT_TRUE( session.EnterSubgraph( subgraphNode ) );
	const auto input = session.AddNode( cmf::AnimationNodeType::SubgraphInput, {} );
	ASSERT_TRUE( input.IsSet() );
	const auto rest = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	session.SetSelection( { input, rest } );
	session.MoveNodes( session.GetSelection(), STEP );
	session.AddNode( cmf::AnimationNodeType::Blend, {} );

	ASSERT_TRUE( session.Undo() );
	ASSERT_EQ( session.GetGraphPath().size(), 1u );
	EXPECT_EQ( &session.GetGraph(), document.FindGraph( session.GetGraphPath() ) );
	EXPECT_NE( &session.GetGraph(), &Root() );
	EXPECT_EQ( session.GetSelection().size(), 2u );

	ASSERT_TRUE( session.LeaveSubgraph() );
	EXPECT_TRUE( session.GetGraphPath().empty() );
	EXPECT_EQ( session.GetSelection().size(), 1u ) << "the subgraph node is selected";
}

TEST_F( EditorFixture, UndoKeepsViews )
{
	session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	document.SetView( Root(), ZOOMED_VIEW );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( document.GetView( Root() ).zoom, ZOOMED_VIEW.zoom );
}

TEST_F( EditorFixture, ReloadRequestsAssetsAgain )
{
	document.GetNetwork().SetSkeleton( "res:/moved/character.cmf", "" );
	EXPECT_TRUE( document.IsLocked() );
	std::string error;
	ASSERT_TRUE( document.SaveFile( directory.GetPath() / "a.cane", error ) ) << error;

	// The skeleton is found after the prefix points to its new location
	WriteFile( directory.GetPath() / "other" / "moved" / "character.cmf", asset.BuildCmfFile() );
	resolver.SetPrefixes( { { PREFIX, ( directory.GetPath() / "other" ).u8string() } } );
	session.Reload();
	EXPECT_FALSE( document.IsLocked() );
	EXPECT_FALSE( document.IsModified() );
}

TEST_F( EditorFixture, AutoLayoutIsUndoable )
{
	const auto rest = session.AddNode( cmf::AnimationNodeType::RestPose, START );
	ASSERT_TRUE( session.Connect( { rest, 0 }, { Output(), 0 } ) );
	session.AutoLayout();
	EXPECT_NE( document.GetNodePosition( Root(), rest ), START );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( document.GetNodePosition( Root(), Root().GetNodes()[1] ), START );
}

}
}
