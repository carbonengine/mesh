// Copyright © 2026 CCP ehf.

#include "testutils.h"

#include "autoLayout.h"

#include <nlohmann/json.hpp>

namespace canedittest
{
namespace
{

using Vec2 = canedit::Vec2;

constexpr Vec2 SAMPLE_POSITION = { -300.f, 20.f };
constexpr Vec2 LOOSE_POSITION = { 50.f, 400.f };
constexpr Vec2 INNER_POSITION = { 10.f, -10.f };
constexpr canedit::GraphView SUBGRAPH_VIEW = { { 1.f, 2.f }, 0.5f };
constexpr canedit::GraphView OTHER_VIEW = { { 5.f, 5.f }, 2.f };
constexpr const char* HEAD_REFERENCE = "res:/characters/head.cmf";

std::vector<Vec2> GetPositions( const canedit::Document& document, const cmf::AnimationGraphBuilder& graph )
{
	std::vector<Vec2> positions;
	for( const auto id : graph.GetNodes() )
	{
		const auto position = document.GetNodePosition( graph, id );
		EXPECT_TRUE( position.has_value() );
		positions.push_back( position.value_or( Vec2{} ) );
	}
	return positions;
}

TEST_F( EditorFixture, NewDocumentHasAnOutputNode )
{
	canedit::Document fresh( loader );
	fresh.New();
	EXPECT_FALSE( fresh.IsModified() );
	const auto& root = fresh.GetNetwork().GetRootGraph();
	ASSERT_TRUE( root.GetOutputNode().IsSet() );
	EXPECT_TRUE( fresh.GetNodePosition( root, root.GetOutputNode() ).has_value() );
	EXPECT_TRUE( fresh.IsLocked() ) << "a new network has no skeleton";
	EXPECT_FALSE( document.IsLocked() );
}

TEST_F( EditorFixture, EditorDataSurvivesSavingAndLoading )
{
	auto& root = Root();
	const auto sample = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( root.Connect( { sample, 0 }, { Output(), 0 } ) );
	document.SetNodePosition( root, sample, SAMPLE_POSITION );
	// Nodes that are not connected keep their position too
	const auto loose = root.AddNode( cmf::AnimationNodeType::Blend );
	document.SetNodePosition( root, loose, LOOSE_POSITION );
	const auto subgraphNode = root.AddNode( cmf::AnimationNodeType::Subgraph );
	document.SetNodePosition( root, subgraphNode, {} );
	auto& subgraph = *root.GetSubgraph( subgraphNode );
	const auto inner = subgraph.AddNode( cmf::AnimationNodeType::RestPose );
	document.SetNodePosition( subgraph, inner, INNER_POSITION );
	document.SetView( subgraph, SUBGRAPH_VIEW );
	document.SetPreviewMeshes( { CHARACTER_REFERENCE, HEAD_REFERENCE } );

	const auto path = directory.GetPath() / "network.can";
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	EXPECT_FALSE( document.IsModified() );

	canedit::Document loaded( loader );
	ASSERT_TRUE( loaded.LoadFile( path, error ) ) << error;
	EXPECT_FALSE( loaded.IsModified() );
	EXPECT_EQ( loaded.GetFilePath(), path );
	EXPECT_EQ( loaded.GetPreviewMeshes(), ( std::vector<std::string>{ CHARACTER_REFERENCE, HEAD_REFERENCE } ) );
	const auto& loadedRoot = loaded.GetNetwork().GetRootGraph();
	const auto nodes = loadedRoot.GetNodes();
	ASSERT_EQ( nodes.size(), 4u );
	EXPECT_EQ( loaded.GetNodePosition( loadedRoot, nodes[1] ), SAMPLE_POSITION );
	EXPECT_EQ( loaded.GetNodePosition( loadedRoot, nodes[2] ), LOOSE_POSITION );
	const auto& loadedSubgraph = *loadedRoot.GetSubgraph( nodes[3] );
	EXPECT_EQ( loaded.GetNodePosition( loadedSubgraph, loadedSubgraph.GetNodes()[0] ), INNER_POSITION );
	EXPECT_EQ( loaded.GetView( loadedSubgraph ).zoom, SUBGRAPH_VIEW.zoom );
	EXPECT_EQ( loaded.GetView( loadedSubgraph ).pan, SUBGRAPH_VIEW.pan );
}

TEST_F( EditorFixture, EditorDataIsJson )
{
	document.SetPreviewMeshes( { "body.cmf", "head.cmf" } );
	const auto file = document.Serialize();
	const auto& editorData = document.GetNetwork().GetEditorData();
	const auto json = nlohmann::json::parse( editorData.begin(), editorData.end() );
	EXPECT_EQ( json["format"], "canedit" );
	EXPECT_EQ( json["version"], canedit::Document::EDITOR_DATA_VERSION );
	EXPECT_EQ( json["previewMeshes"], nlohmann::json::array( { "body.cmf", "head.cmf" } ) );
	EXPECT_FALSE( json.contains( "previewMesh" ) );
	EXPECT_EQ( json["graphs"].size(), 1u );

	// Other clients load the network and skip the editor data
	cmf::AnimationNetwork network( loader );
	EXPECT_TRUE( network.Load( file ) );
}

TEST_F( EditorFixture, PreviewMeshesSkipEmptyAndRepeatedReferences )
{
	document.MarkUnmodified();
	document.AddPreviewMesh( "body.cmf" );
	EXPECT_TRUE( document.IsModified() );
	document.AddPreviewMesh( "head.cmf" );
	document.AddPreviewMesh( "body.cmf" );
	document.AddPreviewMesh( "" );
	EXPECT_EQ( document.GetPreviewMeshes(), ( std::vector<std::string>{ "body.cmf", "head.cmf" } ) );

	document.SetPreviewMeshes( { "", "head.cmf", "head.cmf", "hair.cmf" } );
	EXPECT_EQ( document.GetPreviewMeshes(), ( std::vector<std::string>{ "head.cmf", "hair.cmf" } ) );

	// Setting the same list is not a modification
	document.MarkUnmodified();
	document.SetPreviewMeshes( { "head.cmf", "hair.cmf" } );
	EXPECT_FALSE( document.IsModified() );
	document.SetPreviewMeshes( {} );
	EXPECT_TRUE( document.IsModified() );
	EXPECT_TRUE( document.GetPreviewMeshes().empty() );
}

TEST_F( EditorFixture, PreviewMeshEditsCanBeUndone )
{
	{
		const canedit::ScopedEdit edit( session );
		document.AddPreviewMesh( "body.cmf" );
	}
	{
		const canedit::ScopedEdit edit( session );
		document.AddPreviewMesh( "head.cmf" );
	}
	ASSERT_TRUE( session.CanUndo() );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( document.GetPreviewMeshes(), ( std::vector<std::string>{ "body.cmf" } ) );
	ASSERT_TRUE( session.Redo() );
	EXPECT_EQ( document.GetPreviewMeshes(), ( std::vector<std::string>{ "body.cmf", "head.cmf" } ) );
}

TEST_F( EditorFixture, SinglePreviewMeshOfEarlierFilesIsRead )
{
	const nlohmann::json json = { { "format", "canedit" }, { "version", 1 }, { "previewMesh", "mesh.cmf" } };
	const auto text = json.dump();
	document.GetNetwork().SetEditorData( { text.begin(), text.end() } );
	const auto file = document.GetNetwork().Save();

	canedit::Document loaded( loader );
	std::string error;
	ASSERT_TRUE( loaded.Deserialize( file, error ) ) << error;
	EXPECT_EQ( loaded.GetPreviewMeshes(), ( std::vector<std::string>{ "mesh.cmf" } ) );
}

TEST_F( EditorFixture, NetworksWithoutEditorDataAreLaidOut )
{
	cmf::AnimationNetworkBuilder builder( loader );
	builder.SetSkeleton( CHARACTER_REFERENCE, "" );
	auto& root = builder.GetRootGraph();
	const auto output = root.AddNode( cmf::AnimationNodeType::Output );
	const auto blend = root.AddNode( cmf::AnimationNodeType::Blend );
	const auto restA = root.AddNode( cmf::AnimationNodeType::RestPose );
	const auto restB = root.AddNode( cmf::AnimationNodeType::RestPose );
	root.AddNode( cmf::AnimationNodeType::Expression );
	ASSERT_TRUE( root.Connect( { blend, 0 }, { output, 0 } ) );
	ASSERT_TRUE( root.Connect( { restA, 0 }, { blend, 0 } ) );
	ASSERT_TRUE( root.Connect( { restB, 0 }, { blend, 1 } ) );

	std::string error;
	ASSERT_TRUE( document.Deserialize( builder.Save(), error ) ) << error;
	const auto& loadedRoot = document.GetNetwork().GetRootGraph();
	const auto positions = GetPositions( document, loadedRoot );
	ASSERT_EQ( positions.size(), 5u );
	// Inputs are to the left of the nodes they feed; unconnected nodes are below the graph
	EXPECT_LT( positions[1].x, positions[0].x );
	EXPECT_LT( positions[2].x, positions[1].x );
	EXPECT_EQ( positions[2].x, positions[3].x );
	EXPECT_NE( positions[2].y, positions[3].y );
	EXPECT_GT( positions[4].y, std::max( positions[2].y, positions[3].y ) );

	// The layout only depends on the graph
	EXPECT_EQ( canedit::ComputeAutoLayout( loadedRoot ), canedit::ComputeAutoLayout( loadedRoot ) );
}

TEST_F( EditorFixture, MissingSkeletonLocksTheDocument )
{
	document.GetNetwork().SetSkeleton( "res:/missing.cmf", "" );
	EXPECT_TRUE( document.IsLocked() );
	EXPECT_FALSE( session.CanEdit() );
	EXPECT_FALSE( session.AddNode( cmf::AnimationNodeType::RestPose, {} ).IsSet() );

	document.GetNetwork().SetSkeleton( CHARACTER_REFERENCE, "" );
	EXPECT_FALSE( document.IsLocked() );
	EXPECT_TRUE( session.AddNode( cmf::AnimationNodeType::RestPose, {} ).IsSet() );
}

TEST_F( EditorFixture, ChangesMarkTheDocumentModified )
{
	std::string error;
	ASSERT_TRUE( document.SaveFile( directory.GetPath() / "a.can", error ) ) << error;
	EXPECT_FALSE( document.IsModified() );
	document.SetView( Root(), OTHER_VIEW );
	EXPECT_FALSE( document.IsModified() ) << "views are not modifications";
	document.SetNodePosition( Root(), Output(), { 1.f, 1.f } );
	EXPECT_TRUE( document.IsModified() );
	ASSERT_TRUE( document.SaveFile( directory.GetPath() / "a.can", error ) ) << error;
	Root().AddNode( cmf::AnimationNodeType::RestPose );
	EXPECT_TRUE( document.IsModified() );
}

TEST_F( EditorFixture, MalformedFilesAreNotLoaded )
{
	WriteFile( directory.GetPath() / "bad.can", { 'n', 'o', 'p', 'e' } );
	std::string error;
	EXPECT_FALSE( document.LoadFile( directory.GetPath() / "bad.can", error ) );
	EXPECT_FALSE( error.empty() );
	EXPECT_TRUE( Output().IsSet() ) << "the document is unchanged";
}

}
}
