// Copyright © 2026 CCP ehf.

#include "testutils.h"

#include <cmath>

namespace canedittest
{
namespace
{

using Vec2 = canedit::Vec2;

constexpr Vec2 PASTE_ORIGIN = { 1000.f, 500.f };
constexpr Vec2 SAMPLE_POSITION = { 0.f, 0.f };
constexpr Vec2 EXPRESSION_POSITION = { 40.f, 100.f };
constexpr float ALPHA = 0.25f;

TEST_F( EditorFixture, CopiedNodesArePastedWithSettingsEdgesAndPositions )
{
	auto& root = Root();
	const auto sample = session.AddNode( cmf::AnimationNodeType::AnimationSample, SAMPLE_POSITION );
	ASSERT_TRUE( root.SetAnimation( sample, CHARACTER_REFERENCE, networktest::MOVE_ANIMATION ) );
	const auto expression = session.AddNode( cmf::AnimationNodeType::Expression, EXPRESSION_POSITION );
	ASSERT_TRUE( root.SetExpression( expression, "x * 2", { "x" } ) );
	ASSERT_TRUE( root.SetInputConstant( { expression, 0 }, ALPHA ) );
	ASSERT_TRUE( root.Connect( { expression, 0 }, { sample, 2 } ) );
	ASSERT_TRUE( root.Connect( { sample, 0 }, { Output(), 0 } ) );
	root.SetNodeName( sample, "walk" );

	session.SetSelection( { sample, expression } );
	const auto text = session.CopySelection();
	EXPECT_TRUE( canedit::IsClipboardText( text ) );

	const auto result = session.Paste( text, PASTE_ORIGIN );
	ASSERT_TRUE( result.error.empty() ) << result.error;
	EXPECT_TRUE( result.warnings.empty() );
	ASSERT_EQ( result.nodes.size(), 2u );
	EXPECT_EQ( session.GetSelection(), result.nodes );

	const auto pastedSample = result.nodes[0];
	const auto pastedExpression = result.nodes[1];
	EXPECT_EQ( root.GetNode( pastedSample )->name, "walk" );
	const auto* sampleSettings = root.GetSettings<cmf::AnimationSampleSettings>( pastedSample );
	ASSERT_NE( sampleSettings, nullptr );
	EXPECT_EQ( sampleSettings->animationName, networktest::MOVE_ANIMATION );
	EXPECT_EQ( root.GetSettings<cmf::AnimationExpressionSettings>( pastedExpression )->expression, "x * 2" );
	EXPECT_EQ( root.GetNode( pastedExpression )->inputConstants[0], ALPHA );
	EXPECT_EQ( root.GetConnection( { pastedSample, 2 } ), ( cmf::AnimationSocketRef{ pastedExpression, 0 } ) ) << "edges between copied nodes are pasted";
	EXPECT_EQ( root.GetConnection( { Output(), 0 } ), ( cmf::AnimationSocketRef{ sample, 0 } ) ) << "edges to other nodes are not";
	EXPECT_EQ( document.GetNodePosition( root, pastedSample ), PASTE_ORIGIN );
	EXPECT_EQ( document.GetNodePosition( root, pastedExpression ), ( Vec2{ PASTE_ORIGIN.x + EXPRESSION_POSITION.x, PASTE_ORIGIN.y + EXPRESSION_POSITION.y } ) );

	ASSERT_TRUE( session.Undo() );
	// Undo reloads the network, so graph references must be fetched again
	EXPECT_EQ( Root().GetNodes().size(), 3u ) << "pasting is one undo step";
}

TEST_F( EditorFixture, PastingIntoAnotherNetworkAddsParameters )
{
	cmf::AnimationParameter speed;
	speed.name = "speed";
	speed.minValue = 0.f;
	speed.defaultValue = 1.f;
	const auto index = document.GetNetwork().AddParameter( speed );
	const auto parameter = session.AddNode( cmf::AnimationNodeType::Parameter, {} );
	ASSERT_TRUE( Root().SetParameter( parameter, index ) );
	const auto text = session.CopySelection();

	canedit::Document other( loader, resolver );
	other.New();
	other.GetNetwork().SetSkeleton( CHARACTER_REFERENCE, "" );
	canedit::EditorSession otherSession( other );
	const auto result = otherSession.Paste( text, {} );
	ASSERT_EQ( result.nodes.size(), 1u );
	const auto& parameters = other.GetNetwork().GetParameters();
	ASSERT_EQ( parameters.size(), 1u );
	EXPECT_EQ( parameters[0].name, "speed" );
	EXPECT_EQ( parameters[0].minValue, 0.f );
	EXPECT_TRUE( std::isinf( parameters[0].maxValue ) ) << "unbounded ranges survive the clipboard";
	EXPECT_EQ( other.GetNetwork().GetRootGraph().GetSettings<cmf::AnimationParameterSettings>( result.nodes[0] )->parameterIndex, 0u );
}

TEST_F( EditorFixture, SubgraphsArePastedWithTheirContents )
{
	const auto subgraphNode = session.AddNode( cmf::AnimationNodeType::Subgraph, {} );
	ASSERT_TRUE( session.EnterSubgraph( subgraphNode ) );
	const auto input = session.AddNode( cmf::AnimationNodeType::SubgraphInput, {} );
	ASSERT_TRUE( session.GetGraph().SetInterfaceSocket( input, "alpha", cmf::AnimationSocketType::Float ) );
	const auto output = session.AddNode( cmf::AnimationNodeType::Output, {} );
	const auto rest = session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	ASSERT_TRUE( session.Connect( { rest, 0 }, { output, 0 } ) );
	ASSERT_TRUE( session.LeaveSubgraph() );
	ASSERT_TRUE( Root().SetInputConstant( { subgraphNode, 0 }, ALPHA ) );

	session.SetSelection( { subgraphNode } );
	const auto result = session.Paste( session.CopySelection(), PASTE_ORIGIN );
	ASSERT_EQ( result.nodes.size(), 1u );
	const auto* pasted = Root().GetSubgraph( result.nodes[0] );
	ASSERT_NE( pasted, nullptr );
	EXPECT_EQ( pasted->GetNodes().size(), 3u );
	EXPECT_EQ( pasted->GetEdges().size(), 1u );
	EXPECT_EQ( Root().GetInputName( { result.nodes[0], 0 } ), "alpha" );
	EXPECT_EQ( Root().GetNode( result.nodes[0] )->inputConstants[0], ALPHA );
	EXPECT_TRUE( document.GetNodePosition( *pasted, pasted->GetNodes()[2] ).has_value() );
}

TEST_F( EditorFixture, NodesThatDontFitTheGraphAreSkipped )
{
	session.SetSelection( { Output() } );
	const auto result = session.Paste( session.CopySelection(), {} );
	EXPECT_TRUE( result.nodes.empty() );
	ASSERT_EQ( result.warnings.size(), 1u );
	EXPECT_NE( result.warnings[0].find( "Output" ), std::string::npos );
	EXPECT_FALSE( session.CanUndo() ) << "nothing changed";
}

TEST_F( EditorFixture, OtherTextIsNotPasted )
{
	EXPECT_FALSE( canedit::IsClipboardText( "hello" ) );
	EXPECT_FALSE( canedit::IsClipboardText( R"({ "nodes": [] })" ) );
	const auto result = session.Paste( "hello", {} );
	EXPECT_FALSE( result.error.empty() );
	EXPECT_EQ( Root().GetNodes().size(), 1u );
}

}
}
