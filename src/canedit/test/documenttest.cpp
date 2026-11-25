// Copyright © 2026 CCP ehf.

#include "testutils.h"

#include "autoLayout.h"

#include <nlohmann/json.hpp>

#include <iterator>

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
constexpr const char* EMBEDDED_FILE = "embedded/character.cmf";
constexpr const char* EMBEDDED_REFERENCE = "res:/embedded/character.cmf";
constexpr float SPEED_DEFAULT = 2.f;
constexpr float SPEED_MAX = 10.f;
constexpr float BLEND_WEIGHT = 0.25f;

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

std::vector<uint8_t> ReadFile( const std::filesystem::path& path )
{
	std::ifstream stream( path, std::ios::binary );
	return { std::istreambuf_iterator<char>( stream ), std::istreambuf_iterator<char>() };
}

nlohmann::json ReadJson( const std::filesystem::path& path )
{
	const auto bytes = ReadFile( path );
	return nlohmann::json::parse( bytes.begin(), bytes.end() );
}

void WriteText( const std::filesystem::path& path, const std::string& text )
{
	WriteFile( path, { text.begin(), text.end() } );
}

/** Saves a document and removes the node positions from the file. */
void WriteWithoutPositions( canedit::Document& document, const std::filesystem::path& path )
{
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	auto json = ReadJson( path );
	for( auto& node : json["graph"]["nodes"] )
	{
		node.erase( "position" );
	}
	WriteText( path, json.dump() );
}

TEST_F( EditorFixture, NewDocumentHasAnOutputNode )
{
	canedit::Document fresh( loader, resolver );
	fresh.New();
	EXPECT_FALSE( fresh.IsModified() );
	const auto& root = fresh.GetNetwork().GetRootGraph();
	ASSERT_TRUE( root.GetOutputNode().IsSet() );
	EXPECT_TRUE( fresh.GetNodePosition( root, root.GetOutputNode() ).has_value() );
	EXPECT_TRUE( fresh.IsLocked() ) << "a new network has no skeleton";
	EXPECT_FALSE( document.IsLocked() );
}

TEST_F( EditorFixture, DocumentSurvivesSavingAndLoading )
{
	auto& network = document.GetNetwork();
	auto& root = Root();
	const auto speedIndex = network.AddParameter( { "speed", SPEED_DEFAULT, 0.f, SPEED_MAX } );
	network.AddParameter( { "unused", 0.f, -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() } );
	const auto blend = root.AddNode( cmf::AnimationNodeType::Blend );
	const auto sample = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( root.SetAnimation( sample, CHARACTER_REFERENCE, networktest::LIFT_ANIMATION ) );
	const auto speed = root.AddNode( cmf::AnimationNodeType::Parameter );
	ASSERT_TRUE( root.SetParameter( speed, speedIndex ) );
	ASSERT_TRUE( root.SetNodeName( blend, "Mix" ) );
	ASSERT_TRUE( root.Connect( { blend, 0 }, { Output(), 0 } ) );
	ASSERT_TRUE( root.Connect( { sample, 0 }, { blend, 1 } ) );
	ASSERT_TRUE( root.SetInputConstant( { blend, 2 }, BLEND_WEIGHT ) );
	document.SetNodePosition( root, sample, SAMPLE_POSITION );
	document.SetNodePosition( root, speed, LOOSE_POSITION );
	const auto subgraphNode = root.AddNode( cmf::AnimationNodeType::Subgraph );
	document.SetNodePosition( root, subgraphNode, {} );
	auto& subgraph = *root.GetSubgraph( subgraphNode );
	const auto inner = subgraph.AddNode( cmf::AnimationNodeType::RestPose );
	const auto innerOutput = subgraph.AddNode( cmf::AnimationNodeType::Output );
	ASSERT_TRUE( subgraph.Connect( { inner, 0 }, { innerOutput, 0 } ) );
	document.SetNodePosition( subgraph, inner, INNER_POSITION );
	document.SetView( subgraph, SUBGRAPH_VIEW );
	document.SetPreviewMeshes( { CHARACTER_REFERENCE, HEAD_REFERENCE } );
	ASSERT_TRUE( root.Connect( { subgraphNode, 0 }, { blend, 0 } ) );
	const auto savedNetwork = networktest::SaveNetwork( network );

	const auto path = directory.GetPath() / "network.cane";
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	EXPECT_FALSE( document.IsModified() );

	canedit::Document loaded( loader, resolver );
	std::vector<std::string> warnings;
	ASSERT_TRUE( loaded.LoadFile( path, error, &warnings ) ) << error;
	EXPECT_TRUE( warnings.empty() );
	EXPECT_FALSE( loaded.IsModified() );
	EXPECT_EQ( loaded.GetFilePath(), path );
	EXPECT_EQ( networktest::SaveNetwork( loaded.GetNetwork() ), savedNetwork ) << "the network is stored completely";
	EXPECT_EQ( loaded.GetPreviewMeshes(), ( std::vector<std::string>{ CHARACTER_REFERENCE, HEAD_REFERENCE } ) );
	const auto& loadedRoot = loaded.GetNetwork().GetRootGraph();
	const auto nodes = loadedRoot.GetNodes();
	ASSERT_EQ( nodes.size(), 5u );
	EXPECT_EQ( loaded.GetNodePosition( loadedRoot, nodes[2] ), SAMPLE_POSITION );
	EXPECT_EQ( loaded.GetNodePosition( loadedRoot, nodes[3] ), LOOSE_POSITION );
	const auto& loadedSubgraph = *loadedRoot.GetSubgraph( nodes[4] );
	EXPECT_EQ( loaded.GetNodePosition( loadedSubgraph, loadedSubgraph.GetNodes()[0] ), INNER_POSITION );
	EXPECT_EQ( loaded.GetView( loadedSubgraph ).zoom, SUBGRAPH_VIEW.zoom );
	EXPECT_EQ( loaded.GetView( loadedSubgraph ).pan, SUBGRAPH_VIEW.pan );

	// Saving again gives the same file
	const auto again = directory.GetPath() / "again.cane";
	ASSERT_TRUE( loaded.SaveFile( again, error ) ) << error;
	EXPECT_EQ( ReadFile( again ), ReadFile( path ) );
}

TEST_F( EditorFixture, DocumentsAreReadableJson )
{
	auto& network = document.GetNetwork();
	const auto speed = Root().AddNode( cmf::AnimationNodeType::Parameter );
	ASSERT_TRUE( Root().SetParameter( speed, network.AddParameter( { "speed", 0.f, 0.f, SPEED_MAX } ) ) );
	document.SetPreviewMeshes( { "body.cmf", "head.cmf" } );
	const auto path = directory.GetPath() / "network.cane";
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;

	const auto json = ReadJson( path );
	EXPECT_EQ( json["format"], "cane" );
	EXPECT_EQ( json["version"], canedit::Document::FILE_VERSION );
	EXPECT_EQ( json["skeleton"]["asset"], CHARACTER_REFERENCE );
	EXPECT_EQ( json["skeleton"]["name"], networktest::SKELETON_NAME );
	EXPECT_EQ( json["previewMeshes"], nlohmann::json::array( { "body.cmf", "head.cmf" } ) );
	ASSERT_EQ( json["parameters"].size(), 1u );
	EXPECT_EQ( json["parameters"][0]["name"], "speed" );
	EXPECT_EQ( json["parameters"][0]["max"], SPEED_MAX );
	const auto& nodes = json["graph"]["nodes"];
	ASSERT_EQ( nodes.size(), 2u );
	EXPECT_EQ( nodes[0]["type"], "Output" );
	EXPECT_EQ( nodes[1]["type"], "Parameter" );
	EXPECT_EQ( nodes[1]["settings"]["parameter"], "speed" ) << "Parameter nodes reference parameters by name";
	EXPECT_TRUE( json["graph"].contains( "view" ) );
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

TEST_F( EditorFixture, NodesWithoutPositionsAreLaidOut )
{
	auto& root = Root();
	const auto blend = root.AddNode( cmf::AnimationNodeType::Blend );
	const auto restA = root.AddNode( cmf::AnimationNodeType::RestPose );
	const auto restB = root.AddNode( cmf::AnimationNodeType::RestPose );
	root.AddNode( cmf::AnimationNodeType::Expression );
	ASSERT_TRUE( root.Connect( { blend, 0 }, { Output(), 0 } ) );
	ASSERT_TRUE( root.Connect( { restA, 0 }, { blend, 0 } ) );
	ASSERT_TRUE( root.Connect( { restB, 0 }, { blend, 1 } ) );

	// A document written by another tool, without node positions
	const auto path = directory.GetPath() / "network.cane";
	WriteWithoutPositions( document, path );
	canedit::Document loaded( loader, resolver );
	std::string error;
	ASSERT_TRUE( loaded.LoadFile( path, error ) ) << error;
	const auto& loadedRoot = loaded.GetNetwork().GetRootGraph();
	const auto positions = GetPositions( loaded, loadedRoot );
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
	ASSERT_TRUE( document.SaveFile( directory.GetPath() / "a.cane", error ) ) << error;
	EXPECT_FALSE( document.IsModified() );
	document.SetView( Root(), OTHER_VIEW );
	EXPECT_FALSE( document.IsModified() ) << "views are not modifications";
	document.SetNodePosition( Root(), Output(), { 1.f, 1.f } );
	EXPECT_TRUE( document.IsModified() );
	ASSERT_TRUE( document.SaveFile( directory.GetPath() / "a.cane", error ) ) << error;
	Root().AddNode( cmf::AnimationNodeType::RestPose );
	EXPECT_TRUE( document.IsModified() );
	ASSERT_TRUE( document.SaveFile( directory.GetPath() / "a.cane", error ) ) << error;
	document.SetExportPath( directory.GetPath() / "a.can" );
	EXPECT_TRUE( document.IsModified() ) << "the export path is stored in the document";
}

TEST_F( EditorFixture, MalformedFilesAreNotLoaded )
{
	const auto& directoryPath = directory.GetPath();
	WriteText( directoryPath / "text.cane", "nope" );
	WriteText( directoryPath / "other.cane", R"({ "format": "other", "version": 1 })" );
	WriteText( directoryPath / "newer.cane", R"({ "format": "cane", "version": 99, "graph": { "nodes": [], "edges": [] } })" );
	WriteText( directoryPath / "structure.cane", R"({ "format": "cane", "version": 1, "graph": { "nodes": 5, "edges": [] } })" );
	WriteFile( directoryPath / "network.cane", asset.BuildCmfFile() );
	std::string error;
	ASSERT_TRUE( document.SaveFile( directoryPath / "a.cane", error ) ) << error;
	for( const auto* name : { "text.cane", "other.cane", "newer.cane", "structure.cane", "network.cane", "missing.cane" } )
	{
		error.clear();
		const bool loaded = document.LoadFile( directoryPath / name, error );
		const bool unchanged = Output().IsSet() && document.GetNetwork().GetSkeletonPath() == CHARACTER_REFERENCE && !document.IsModified() && document.GetFilePath() == directoryPath / "a.cane";
		EXPECT_TRUE( !loaded && !error.empty() ) << name << " was loaded";
		EXPECT_TRUE( unchanged ) << name << " changed the document";
	}
	EXPECT_NE( error.find( "missing.cane" ), std::string::npos );
}

TEST_F( EditorFixture, PartsThatCantBeAppliedAreSkipped )
{
	const auto path = directory.GetPath() / "network.cane";
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	auto json = ReadJson( path );
	auto& graph = json["graph"];
	graph["nodes"].push_back( { { "type", "NoSuchNode" } } );
	graph["nodes"].push_back( { { "type", "Parameter" }, { "settings", { { "parameter", "missing" } } } } );
	graph["nodes"].push_back( { { "type", "Expression" } } );
	// A Float output into the Pose input of the Output node
	graph["edges"].push_back( { 3, 0, 0, 0 } );
	WriteText( path, json.dump() );

	canedit::Document loaded( loader, resolver );
	std::vector<std::string> warnings;
	ASSERT_TRUE( loaded.LoadFile( path, error, &warnings ) ) << error;
	EXPECT_EQ( warnings.size(), 3u );
	EXPECT_TRUE( loaded.IsModified() ) << "saving the document drops the skipped parts";
	EXPECT_EQ( loaded.GetNetwork().GetRootGraph().GetNodes().size(), 3u );
	EXPECT_TRUE( loaded.GetNetwork().GetRootGraph().GetEdges().empty() );
}

TEST_F( EditorFixture, ExportPathIsStoredRelativeToTheDocument )
{
	const auto exportPath = directory.GetPath() / "out" / "network.can";
	document.SetExportPath( exportPath );
	std::string error;
	const auto path = directory.GetPath() / "documents" / "network.cane";
	std::filesystem::create_directories( path.parent_path() );
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	EXPECT_EQ( ReadJson( path )["exportPath"], "../out/network.can" );

	canedit::Document loaded( loader, resolver );
	ASSERT_TRUE( loaded.LoadFile( path, error ) ) << error;
	EXPECT_EQ( loaded.GetExportPath(), exportPath.lexically_normal() );
}

TEST_F( EditorFixture, ExportWritesTheNetwork )
{
	const auto rest = Root().AddNode( cmf::AnimationNodeType::RestPose );
	ASSERT_TRUE( Root().Connect( { rest, 0 }, { Output(), 0 } ) );
	document.MarkUnmodified();
	const auto path = directory.GetPath() / "network.can";
	std::string error;
	ASSERT_TRUE( document.Export( path, error ) ) << error;
	EXPECT_FALSE( document.IsModified() ) << "exporting doesn't change the document";
	const auto file = ReadFile( path );
	EXPECT_EQ( file, networktest::SaveNetwork( document.GetNetwork() ) );
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( file ) );
	EXPECT_TRUE( network.IsReady() );
}

TEST_F( EditorFixture, NetworksWithErrorsAreNotExported )
{
	document.GetNetwork().SetSkeleton( "res:/missing.cmf", "" );
	const auto path = directory.GetPath() / "network.can";
	std::string error;
	EXPECT_FALSE( document.Export( path, error ) );
	EXPECT_NE( error.find( "can't be exported" ), std::string::npos ) << error;
	EXPECT_FALSE( std::filesystem::exists( path ) );
}

TEST_F( EditorFixture, EmbeddedAssetsAreStoredAsReferences )
{
	WriteFile( directory.GetPath() / EMBEDDED_FILE, asset.BuildCmfFile() );
	EXPECT_FALSE( document.EmbedAsset( "res:/missing.cmf" ) );
	EXPECT_TRUE( document.GetEmbeddedAssets().empty() );
	ASSERT_TRUE( document.EmbedAsset( EMBEDDED_REFERENCE ) );
	EXPECT_EQ( document.GetEmbeddedAssets(), std::set<std::string>{ EMBEDDED_REFERENCE } );
	EXPECT_NE( document.GetNetwork().GetEmbeddedAsset( EMBEDDED_REFERENCE ), nullptr );
	document.GetNetwork().SetSkeleton( EMBEDDED_REFERENCE, "" );
	const auto rest = Root().AddNode( cmf::AnimationNodeType::RestPose );
	ASSERT_TRUE( Root().Connect( { rest, 0 }, { Output(), 0 } ) );

	const auto path = directory.GetPath() / "network.cane";
	std::string error;
	ASSERT_TRUE( document.SaveFile( path, error ) ) << error;
	EXPECT_EQ( ReadJson( path )["embeddedAssets"], nlohmann::json::array( { EMBEDDED_REFERENCE } ) );

	// The exported network contains the skeleton of the file
	const auto exported = directory.GetPath() / "network.can";
	ASSERT_TRUE( document.Export( exported, error ) ) << error;
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( ReadFile( exported ) ) );
	EXPECT_TRUE( network.GetData()->assets.empty() );
	ASSERT_EQ( network.GetData()->skeleton.size(), 1u );
	EXPECT_TRUE( network.IsReady() );

	// Without the file, the document loads with a warning and keeps the reference, but can't be exported
	std::filesystem::remove( directory.GetPath() / EMBEDDED_FILE );
	EXPECT_FALSE( document.Export( exported, error ) ) << "embedded files are read again when exporting";
	EXPECT_NE( error.find( EMBEDDED_REFERENCE ), std::string::npos ) << error;
	canedit::Document loaded( loader, resolver );
	std::vector<std::string> warnings;
	ASSERT_TRUE( loaded.LoadFile( path, error, &warnings ) ) << error;
	EXPECT_EQ( warnings.size(), 1u );
	EXPECT_EQ( loaded.GetEmbeddedAssets(), std::set<std::string>{ EMBEDDED_REFERENCE } );
	EXPECT_NE( loaded.GetEmbeddedAssetError( EMBEDDED_REFERENCE ), nullptr );
	EXPECT_EQ( loaded.GetNetwork().GetEmbeddedAsset( EMBEDDED_REFERENCE ), nullptr );
}

TEST_F( EditorFixture, UndoReusesEmbeddedFiles )
{
	WriteFile( directory.GetPath() / EMBEDDED_FILE, asset.BuildCmfFile() );
	{
		const canedit::ScopedEdit edit( session );
		ASSERT_TRUE( document.EmbedAsset( EMBEDDED_REFERENCE ) );
	}
	std::filesystem::remove( directory.GetPath() / EMBEDDED_FILE );
	ASSERT_TRUE( session.Undo() );
	EXPECT_TRUE( document.GetEmbeddedAssets().empty() );
	EXPECT_EQ( document.GetNetwork().GetEmbeddedAsset( EMBEDDED_REFERENCE ), nullptr );
	ASSERT_TRUE( session.Redo() );
	EXPECT_NE( document.GetNetwork().GetEmbeddedAsset( EMBEDDED_REFERENCE ), nullptr ) << "the file was read before";

	// Reloading reads the files again
	session.Reload();
	EXPECT_EQ( document.GetNetwork().GetEmbeddedAsset( EMBEDDED_REFERENCE ), nullptr );
	EXPECT_NE( document.GetEmbeddedAssetError( EMBEDDED_REFERENCE ), nullptr );
}

TEST_F( EditorFixture, UndoKeepsTheExportPath )
{
	session.AddNode( cmf::AnimationNodeType::RestPose, {} );
	document.SetExportPath( directory.GetPath() / "network.can" );
	ASSERT_TRUE( session.Undo() );
	EXPECT_EQ( document.GetExportPath(), directory.GetPath() / "network.can" );
}

TEST_F( EditorFixture, MissingEmbeddedAnimationsAreNotExported )
{
	WriteFile( directory.GetPath() / EMBEDDED_FILE, asset.BuildCmfFile() );
	ASSERT_TRUE( document.EmbedAsset( EMBEDDED_REFERENCE ) );
	const auto sample = Root().AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( Root().SetAnimation( sample, EMBEDDED_REFERENCE, "Missing" ) );
	ASSERT_TRUE( Root().Connect( { sample, 0 }, { Output(), 0 } ) );

	const auto path = directory.GetPath() / "network.can";
	std::string error;
	EXPECT_FALSE( document.Export( path, error ) );
	EXPECT_NE( error.find( "has no animation named 'Missing'" ), std::string::npos ) << error;
	EXPECT_FALSE( std::filesystem::exists( path ) );
}

}
}
