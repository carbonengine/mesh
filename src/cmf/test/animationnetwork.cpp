// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"

#include "networktestutils.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <set>

namespace networktest
{
namespace
{

constexpr const char* EMBEDDED_PATH = "embedded/character.cmf";
constexpr std::array<float, 2> RANDOM_WEIGHTS = { 1.f, 3.f };

void ExpectOk( const cmf::ValidationResult& result )
{
	EXPECT_TRUE( result ) << result.error;
}

void ExpectOk( bool result )
{
	EXPECT_TRUE( result );
}

struct RuntimeFixture : public ::testing::Test
{
	TestAsset asset;
	FakeLoader loader;

	RuntimeFixture()
	{
		loader.AddAsset( ASSET_PATH, &asset.data );
	}

	/** Builds a network that uses every node type: two animations blended by a parameter driven expression,
	* masked by named bone weights, inside a subgraph. */
	std::vector<uint8_t> BuildCompleteNetwork( bool embedSkeleton = false )
	{
		cmf::AnimationNetworkBuilder builder( loader );
		if( embedSkeleton )
		{
			EXPECT_TRUE( builder.EmbedAsset( EMBEDDED_PATH, asset.BuildCmfFile() ) );
			builder.SetSkeleton( EMBEDDED_PATH, SKELETON_NAME );
		}
		else
		{
			builder.SetSkeleton( ASSET_PATH, SKELETON_NAME );
		}
		cmf::AnimationParameter speed;
		speed.name = "speed";
		speed.defaultValue = 1.f;
		speed.minValue = 0.f;
		speed.maxValue = 4.f; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers): arbitrary range
		const auto speedIndex = builder.AddParameter( speed );

		auto& root = builder.GetRootGraph();
		const auto output = root.AddNode( cmf::AnimationNodeType::Output );
		const auto subgraphNode = root.AddNode( cmf::AnimationNodeType::Subgraph );
		root.SetNodeName( subgraphNode, "Locomotion" );
		const auto parameter = root.AddNode( cmf::AnimationNodeType::Parameter );
		ExpectOk( root.SetParameter( parameter, speedIndex ) );

		auto& sub = *root.GetSubgraph( subgraphNode );
		const auto speedIn = sub.AddNode( cmf::AnimationNodeType::SubgraphInput );
		ExpectOk( sub.SetInterfaceSocket( speedIn, "speed", cmf::AnimationSocketType::Float ) );
		const auto subOutput = sub.AddNode( cmf::AnimationNodeType::Output );
		ExpectOk( sub.SetInterfaceSocket( subOutput, "pose", cmf::AnimationSocketType::Pose ) );
		const auto move = sub.AddNode( cmf::AnimationNodeType::AnimationSample );
		ExpectOk( sub.SetAnimation( move, embedSkeleton ? EMBEDDED_PATH : ASSET_PATH, MOVE_ANIMATION ) );
		const auto rest = sub.AddNode( cmf::AnimationNodeType::RestPose );
		const auto blend = sub.AddNode( cmf::AnimationNodeType::Blend );
		const auto mask = sub.AddNode( cmf::AnimationNodeType::NamedBoneWeights );
		ExpectOk( sub.SetBoneMask( mask, MASK_NAME ) );
		const auto expression = sub.AddNode( cmf::AnimationNodeType::Expression );
		ExpectOk( sub.SetExpression( expression, "min( speed / 2, 1 )", { "speed" } ) );
		const auto random = sub.AddNode( cmf::AnimationNodeType::RandomSwitch );
		ExpectOk( sub.SetRandomWeights( random, { RANDOM_WEIGHTS.begin(), RANDOM_WEIGHTS.end() } ) );
		ExpectOk( sub.SetRerollOnActivate( random, true ) );
		const auto weights = sub.AddNode( cmf::AnimationNodeType::BoneWeights );
		ExpectOk( sub.SetBoneWeights( weights, { { "Leaf", 1.f } }, 0.f ) );

		ExpectOk( sub.Connect( { speedIn, 0 }, { expression, 0 } ) );
		ExpectOk( sub.Connect( { expression, 0 }, { blend, 2 } ) );
		ExpectOk( sub.Connect( { mask, 0 }, { blend, 3 } ) );
		ExpectOk( sub.Connect( { rest, 0 }, { random, 0 } ) );
		ExpectOk( sub.Connect( { move, 0 }, { random, 1 } ) );
		ExpectOk( sub.Connect( { rest, 0 }, { blend, 0 } ) );
		ExpectOk( sub.Connect( { random, 0 }, { blend, 1 } ) );
		ExpectOk( sub.Connect( { blend, 0 }, { subOutput, 0 } ) );

		ExpectOk( root.Connect( { parameter, 0 }, { subgraphNode, 0 } ) );
		ExpectOk( root.Connect( { subgraphNode, 0 }, { output, 0 } ) );

		// A removed node leaves a gap in the slots
		ExpectOk( sub.RemoveNode( weights ) );
		EXPECT_TRUE( builder.GetErrors().empty() );
		return SaveNetwork( builder );
	}
};

/** Index of the AnimationSample node in the subgraph of the complete network. */
constexpr uint32_t COMPLETE_NETWORK_SAMPLE_NODE = 2;

cmf::can::Header& GetHeader( std::vector<uint8_t>& file )
{
	return *reinterpret_cast<cmf::can::Header*>( file.data() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): test access to the file header
}

cmf::can::NetworkData& GetNetworkData( std::vector<uint8_t>& file )
{
	return *reinterpret_cast<cmf::can::NetworkData*>( file.data() + GetHeader( file ).sections[0].offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): test access to the network section
}

const cmf::can::AnimationSampleNode& GetSampleNode( const cmf::can::Graph& graph, uint32_t node )
{
	return graph.animationSampleNodes[graph.nodes[node].dataIndex];
}

cmf::can::AnimationSampleNode& GetSampleNode( cmf::can::Graph& graph, uint32_t node )
{
	return graph.animationSampleNodes[graph.nodes[node].dataIndex];
}

void UpdateCrc( std::vector<uint8_t>& file )
{
	constexpr size_t crcEnd = offsetof( cmf::can::Header, crc32 ) + sizeof( uint32_t );
	GetHeader( file ).crc32 = cmf::ComputeCrc32( file.data() + crcEnd, file.size() - crcEnd );
}

/** Checks that the nodes of a graph layout are stored in the graph in layout order. */
void ExpectGraphMatchesFile( const cmf::AnimationGraphLayout& layout, const cmf::can::Graph& graph )
{
	ASSERT_EQ( graph.nodes.size(), layout.nodes.size() );
	for( size_t n = 0; n < layout.nodes.size(); ++n )
	{
		const auto& node = *layout.graph->GetNode( layout.nodes[n] );
		EXPECT_EQ( graph.nodes[n].type, node.type ) << "node " << n;
		EXPECT_EQ( cmf::ToStdString( graph.nodes[n].name ), node.name ) << "node " << n;
	}
}

/** Checks that the nodes of a save layout are stored in the file in layout order. */
void ExpectLayoutMatchesFile( const std::vector<cmf::AnimationGraphLayout>& layout, const cmf::can::NetworkData& data )
{
	ASSERT_EQ( data.graphs.size(), layout.size() );
	for( size_t g = 0; g < layout.size(); ++g )
	{
		SCOPED_TRACE( "graph " + std::to_string( g ) );
		ExpectGraphMatchesFile( layout[g], data.graphs[g] );
	}
}

bool HasErrorContaining( const std::vector<std::string>& errors, const std::string& text )
{
	return std::any_of( errors.begin(), errors.end(), [&text]( const std::string& error ) { return error.find( text ) != std::string::npos; } );
}

/** Connects pose nodes to an Output node through a RandomSwitch node, so that they are used and saved. */
void ConnectToOutput( cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId output, const std::vector<cmf::AnimationNodeId>& nodes )
{
	const auto random = graph.AddNode( cmf::AnimationNodeType::RandomSwitch );
	ExpectOk( graph.SetPoseInputCount( random, static_cast<uint32_t>( nodes.size() ) ) );
	for( uint32_t i = 0; i < nodes.size(); ++i )
	{
		ExpectOk( graph.Connect( { nodes[i], 0 }, { random, i } ) );
	}
	ExpectOk( graph.Connect( { random, 0 }, { output, 0 } ) );
}

/** A network with a subgraph connected to the output, a removed node, and nodes and subgraphs that are not connected
* to the output. */
struct LayoutNetwork
{
	cmf::AnimationNetworkBuilder builder;
	cmf::AnimationNodeId output;
	/** The Subgraph node connected to the output, and the used nodes of its subgraph. */
	cmf::AnimationNodeId used;
	cmf::AnimationNodeId usedRest;
	cmf::AnimationNodeId usedOutput;
	/** All graphs of the network. */
	std::set<const cmf::AnimationGraphBuilder*> graphs;

	explicit LayoutNetwork( cmf::AnimationAssetLoader& loader ) :
		builder( loader ),
		output( SetUpNetwork( builder ) )
	{
		auto& root = builder.GetRootGraph();
		graphs.insert( &root );
		const auto removed = root.AddNode( cmf::AnimationNodeType::RestPose );
		const auto unconnected = root.AddNode( cmf::AnimationNodeType::Blend );
		root.SetNodeName( unconnected, "unconnected" );
		const auto outer = root.AddNode( cmf::AnimationNodeType::Subgraph );
		root.SetNodeName( outer, "outer" );
		auto& outerGraph = *root.GetSubgraph( outer );
		outerGraph.SetNodeName( outerGraph.AddNode( cmf::AnimationNodeType::RestPose ), "outer rest" );
		const auto inner = outerGraph.AddNode( cmf::AnimationNodeType::Subgraph );
		outerGraph.GetSubgraph( inner )->AddNode( cmf::AnimationNodeType::Blend );
		const auto second = root.AddNode( cmf::AnimationNodeType::Subgraph );
		root.SetNodeName( second, "second" );
		ExpectOk( root.RemoveNode( removed ) );
		graphs.insert( { &outerGraph, outerGraph.GetSubgraph( inner ), root.GetSubgraph( second ) } );

		// The used subgraph also holds a disconnected node and feeds a disconnected node
		used = root.AddNode( cmf::AnimationNodeType::Subgraph );
		root.SetNodeName( used, "used" );
		auto& usedGraph = *root.GetSubgraph( used );
		graphs.insert( &usedGraph );
		usedRest = usedGraph.AddNode( cmf::AnimationNodeType::RestPose );
		usedGraph.SetNodeName( usedGraph.AddNode( cmf::AnimationNodeType::RestPose ), "used unconnected" );
		usedOutput = usedGraph.AddNode( cmf::AnimationNodeType::Output );
		ExpectOk( usedGraph.SetInterfaceSocket( usedOutput, "pose", cmf::AnimationSocketType::Pose ) );
		ExpectOk( usedGraph.Connect( { usedRest, 0 }, { usedOutput, 0 } ) );
		ExpectOk( root.Connect( { used, 0 }, { output, 0 } ) );
		ExpectOk( root.Connect( { used, 0 }, { unconnected, 0 } ) );
	}
};
}

// Builder serialization

TEST_F( RuntimeFixture, SavedFileContainsTheNetwork )
{
	auto file = BuildCompleteNetwork();
	const auto& data = GetNetworkData( file );
	EXPECT_EQ( data.skeletonAsset, 0u );
	EXPECT_EQ( cmf::ToStdStringView( data.skeletonName ), SKELETON_NAME );
	ASSERT_EQ( data.assets.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data.assets[0].path ), ASSET_PATH );
	EXPECT_TRUE( data.skeleton.empty() );
	EXPECT_TRUE( data.animations.empty() );
	ASSERT_EQ( data.parameters.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data.parameters[0].name ), "speed" );

	ASSERT_EQ( data.graphs.size(), 2u );
	const auto& root = data.graphs[0];
	EXPECT_EQ( root.nodes.size(), 3u );
	EXPECT_EQ( root.edges.size(), 2u );
	EXPECT_EQ( cmf::ToStdStringView( root.nodes[1].name ), "Locomotion" );
	const auto& sub = data.graphs[1];
	EXPECT_EQ( sub.nodes.size(), 8u );
	EXPECT_EQ( sub.edges.size(), 8u );

	const auto& sample = GetSampleNode( sub, COMPLETE_NETWORK_SAMPLE_NODE );
	EXPECT_EQ( sample.assetIndex, 0u );
	EXPECT_EQ( sample.embeddedAnimation, cmf::can::NO_INDEX );
	EXPECT_EQ( cmf::ToStdStringView( sample.animationName ), MOVE_ANIMATION );

	ASSERT_EQ( sub.randomSwitchNodes.size(), 1u );
	const auto& random = sub.randomSwitchNodes[0];
	EXPECT_EQ( std::vector<float>( random.weights.begin(), random.weights.end() ), ( std::vector<float>{ RANDOM_WEIGHTS.begin(), RANDOM_WEIGHTS.end() } ) );
	EXPECT_EQ( random.flags, cmf::can::RANDOM_SWITCH_REROLL_ON_ACTIVATE );
}

TEST_F( RuntimeFixture, EmbeddedAssetsStoreOnlyTheReferencedSkeletonAndAnimations )
{
	auto file = BuildCompleteNetwork( true );
	EXPECT_EQ( GetHeader( file ).sections.size(), 1u );
	const auto& data = GetNetworkData( file );
	EXPECT_TRUE( data.assets.empty() ) << "embedded assets are not listed as external assets";
	EXPECT_EQ( data.skeletonAsset, cmf::can::NO_INDEX );
	EXPECT_EQ( cmf::ToStdStringView( data.skeletonName ), SKELETON_NAME );
	ASSERT_EQ( data.skeleton.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data.skeleton[0].name ), SKELETON_NAME );
	EXPECT_EQ( data.skeleton[0].bones.size(), BONE_COUNT );
	ASSERT_EQ( data.skeleton[0].boneMasks.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data.skeleton[0].boneMasks[0].name ), MASK_NAME );

	// Only Move is sampled; Lift is left out
	ASSERT_EQ( data.animations.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data.animations[0].name ), MOVE_ANIMATION );
	const auto& sample = GetSampleNode( data.graphs[1], COMPLETE_NETWORK_SAMPLE_NODE );
	EXPECT_EQ( sample.assetIndex, cmf::can::NO_INDEX );
	EXPECT_EQ( sample.embeddedAnimation, 0u );
	EXPECT_EQ( cmf::ToStdStringView( sample.animationName ), MOVE_ANIMATION );
}

TEST_F( RuntimeFixture, EmbeddedAnimationsAreStoredOnce )
{
	cmf::AnimationNetworkBuilder builder( loader );
	const auto output = SetUpNetwork( builder );
	ASSERT_TRUE( builder.EmbedAsset( EMBEDDED_PATH, asset.BuildCmfFile() ) );
	auto& root = builder.GetRootGraph();
	const std::vector<std::string> names = { MOVE_ANIMATION, LIFT_ANIMATION, "", MOVE_ANIMATION };
	std::vector<cmf::AnimationNodeId> samples;
	for( const auto& name : names )
	{
		samples.push_back( root.AddNode( cmf::AnimationNodeType::AnimationSample ) );
		ExpectOk( root.SetAnimation( samples.back(), EMBEDDED_PATH, name ) );
	}
	ConnectToOutput( root, output, samples );
	auto file = SaveNetwork( builder );
	const auto& data = GetNetworkData( file );
	std::vector<std::string> animationNames;
	for( const auto& animation : data.animations )
	{
		animationNames.push_back( cmf::ToStdString( animation.name ) );
	}
	EXPECT_EQ( animationNames, ( std::vector<std::string>{ MOVE_ANIMATION, LIFT_ANIMATION } ) );
	// The empty name selects the first animation of the file, Move. The names are kept as they were set.
	std::vector<uint32_t> indices;
	std::vector<std::string> storedNames;
	for( const auto& sample : data.graphs[0].animationSampleNodes )
	{
		indices.push_back( sample.embeddedAnimation );
		storedNames.push_back( cmf::ToStdString( sample.animationName ) );
	}
	EXPECT_EQ( indices, ( std::vector<uint32_t>{ 0, 1, 0, 0 } ) );
	EXPECT_EQ( storedNames, names );
	EXPECT_EQ( data.assets.size(), 1u ) << "only the external skeleton asset";
}

TEST_F( RuntimeFixture, UnreferencedEmbeddedAssetsAreNotStored )
{
	cmf::AnimationNetworkBuilder builder( loader );
	SetUpNetwork( builder );
	ASSERT_TRUE( builder.EmbedAsset( EMBEDDED_PATH, asset.BuildCmfFile() ) );
	auto file = SaveNetwork( builder );
	const auto& data = GetNetworkData( file );
	EXPECT_TRUE( data.skeleton.empty() );
	EXPECT_TRUE( data.animations.empty() );
	EXPECT_EQ( data.assets.size(), 1u );
}

TEST_F( RuntimeFixture, SavingFailsForMissingEmbeddedData )
{
	cmf::AnimationNetworkBuilder builder( loader );
	ASSERT_TRUE( builder.EmbedAsset( EMBEDDED_PATH, asset.BuildCmfFile() ) );
	builder.SetSkeleton( EMBEDDED_PATH, "Missing" );
	std::vector<uint8_t> file = { 1, 2, 3 };
	auto result = builder.Save( file );
	EXPECT_FALSE( result );
	EXPECT_EQ( result.error, "The embedded asset 'embedded/character.cmf' has no skeleton named 'Missing'" );
	EXPECT_EQ( file, ( std::vector<uint8_t>{ 1, 2, 3 } ) ) << "the file is unchanged";

	builder.SetSkeleton( EMBEDDED_PATH, SKELETON_NAME );
	auto& root = builder.GetRootGraph();
	const auto sample = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ExpectOk( root.SetAnimation( sample, EMBEDDED_PATH, "Walk" ) );
	// Unused nodes are not saved, so their missing data doesn't matter
	ExpectOk( builder.Save( file ) );

	ExpectOk( root.Connect( { sample, 0 }, { root.AddNode( cmf::AnimationNodeType::Output ), 0 } ) );
	result = builder.Save( file );
	EXPECT_FALSE( result );
	EXPECT_EQ( result.error, "The embedded asset 'embedded/character.cmf' has no animation named 'Walk'" );

	// Missing data in external assets is only reported when the network is loaded
	ExpectOk( root.SetAnimation( sample, ASSET_PATH, "Walk" ) );
	ExpectOk( builder.Save( file ) );
}

TEST_F( RuntimeFixture, InvalidSectionsAreRejected )
{
	auto valid = BuildCompleteNetwork( true );
	cmf::AnimationNetwork network( loader );
	ASSERT_EQ( GetHeader( valid ).sections.size(), 1u );

	// Unknown section types, for example the removed EmbeddedCmf section
	auto unknownType = valid;
	const uint8_t unknownSectionType = 1;
	std::memcpy( &GetHeader( unknownType ).sections[0].type, &unknownSectionType, sizeof( unknownSectionType ) );
	UpdateCrc( unknownType );
	EXPECT_EQ( network.Load( unknownType ).error, "Invalid section type" );

	auto noSections = valid;
	GetHeader( noSections ).sections.byteSize = 0;
	UpdateCrc( noSections );
	EXPECT_EQ( network.Load( noSections ).error, "The file must have exactly one section" );
}

TEST_F( RuntimeFixture, InvalidEmbeddedDataIsRejected )
{
	cmf::AnimationNetwork network( loader );
	const auto valid = BuildCompleteNetwork( true );
	constexpr uint32_t badIndex = 42;

	auto file = valid;
	GetNetworkData( file ).skeletonAsset = 0;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Invalid skeleton asset index" ) << "the file has no external assets";

	file = valid;
	GetSampleNode( GetNetworkData( file ).graphs[1], COMPLETE_NETWORK_SAMPLE_NODE ).embeddedAnimation = badIndex;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 1 node 2: invalid embedded animation index" );

	file = valid;
	GetNetworkData( file ).animations[0].duration = 0.f;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Invalid embedded animation 0: Animation \"Move\" has non-positive duration" );

	file = valid;
	GetNetworkData( file ).skeleton[0].parents[CHILD_BONE] = badIndex;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Invalid embedded skeleton: Skeleton \"Skeleton\" bone 1 has out-of-range parent index" );
}

TEST_F( RuntimeFixture, ReferencesMustBeEitherExternalOrEmbedded )
{
	// Embedded skeleton, one embedded and one external animation
	cmf::AnimationNetworkBuilder builder( loader );
	ASSERT_TRUE( builder.EmbedAsset( EMBEDDED_PATH, asset.BuildCmfFile() ) );
	builder.SetSkeleton( EMBEDDED_PATH, SKELETON_NAME );
	auto& root = builder.GetRootGraph();
	const auto embeddedSample = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ExpectOk( root.SetAnimation( embeddedSample, EMBEDDED_PATH, MOVE_ANIMATION ) );
	const auto externalSample = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ExpectOk( root.SetAnimation( externalSample, ASSET_PATH, LIFT_ANIMATION ) );
	ConnectToOutput( root, root.AddNode( cmf::AnimationNodeType::Output ), { embeddedSample, externalSample } );
	const auto valid = SaveNetwork( builder );
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( valid ) );

	auto file = valid;
	GetNetworkData( file ).skeletonAsset = 0;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "The skeleton is both external and embedded" );

	file = valid;
	auto& external = GetSampleNode( GetNetworkData( file ).graphs[0], 1 );
	ASSERT_EQ( external.assetIndex, 0u );
	external.embeddedAnimation = 0;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0 node 1: the animation is both external and embedded" );
}

TEST_F( RuntimeFixture, SaveLayoutMatchesTheSavedFile )
{
	const LayoutNetwork network( loader );
	const auto& root = network.builder.GetRootGraph();
	const auto layout = network.builder.GetSaveLayout();
	ASSERT_EQ( layout.size(), 2u );
	EXPECT_EQ( layout[0].graph, &root );
	EXPECT_EQ( layout[0].nodes, ( std::vector<cmf::AnimationNodeId>{ network.output, network.used } ) );
	EXPECT_EQ( layout[1].graph, root.GetSubgraph( network.used ) );
	EXPECT_EQ( layout[1].nodes, ( std::vector<cmf::AnimationNodeId>{ network.usedRest, network.usedOutput } ) );

	auto file = SaveNetwork( network.builder );
	ExpectLayoutMatchesFile( layout, GetNetworkData( file ) );
	const auto& savedRoot = GetNetworkData( file ).graphs[0];
	ASSERT_EQ( savedRoot.edges.size(), 1u ) << "the edge to the disconnected node is left out";
	EXPECT_EQ( savedRoot.edges[0].to.node, 0u );
	EXPECT_EQ( savedRoot.edges[0].from.node, 1u );
	EXPECT_EQ( GetNetworkData( file ).graphs[1].interfaceOutputs[0], 1u );
}

TEST_F( RuntimeFixture, LayoutHasAllGraphsAndNodes )
{
	const LayoutNetwork network( loader );
	const auto& root = network.builder.GetRootGraph();
	const auto layout = network.builder.GetLayout();
	ASSERT_EQ( layout.size(), network.graphs.size() );
	EXPECT_EQ( layout[0].graph, &root );
	std::set<const cmf::AnimationGraphBuilder*> graphs;
	for( const auto& graphLayout : layout )
	{
		graphs.insert( graphLayout.graph );
		EXPECT_EQ( graphLayout.nodes, graphLayout.graph->GetNodes() );
	}
	EXPECT_EQ( graphs, network.graphs );
}

TEST_F( RuntimeFixture, UnusedNodesAreNotSaved )
{
	cmf::AnimationNetworkBuilder builder( loader );
	const auto output = SetUpNetwork( builder );
	ASSERT_TRUE( builder.EmbedAsset( EMBEDDED_PATH, asset.BuildCmfFile() ) );
	auto& root = builder.GetRootGraph();
	const auto used = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ExpectOk( root.SetAnimation( used, EMBEDDED_PATH, MOVE_ANIMATION ) );
	ExpectOk( root.Connect( { used, 0 }, { output, 0 } ) );
	// Unused nodes with problems that would prevent evaluation, and animations that would be stored
	root.AddNode( cmf::AnimationNodeType::Blend );
	root.AddNode( cmf::AnimationNodeType::StateMachine );
	ExpectOk( root.SetAnimation( root.AddNode( cmf::AnimationNodeType::AnimationSample ), EMBEDDED_PATH, LIFT_ANIMATION ) );
	ExpectOk( root.SetAnimation( root.AddNode( cmf::AnimationNodeType::AnimationSample ), "other/asset.cmf", MOVE_ANIMATION ) );
	ASSERT_TRUE( builder.IsReady() );

	auto file = SaveNetwork( builder );
	const auto& data = GetNetworkData( file );
	ASSERT_EQ( data.graphs.size(), 1u );
	EXPECT_EQ( data.graphs[0].nodes.size(), 2u );
	EXPECT_TRUE( data.graphs[0].stateMachineNodes.empty() );
	ASSERT_EQ( data.animations.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data.animations[0].name ), MOVE_ANIMATION );
	ASSERT_EQ( data.assets.size(), 1u ) << "only the skeleton asset";
	EXPECT_EQ( cmf::ToStdStringView( data.assets[0].path ), ASSET_PATH );

	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( file ) );
	EXPECT_TRUE( network.IsReady() );
	EXPECT_TRUE( network.GetErrors().empty() );
	EXPECT_TRUE( network.GetWarnings().empty() );
}

TEST_F( RuntimeFixture, ClearReleasesEverything )
{
	cmf::AnimationNetworkBuilder builder( loader );
	SetUpNetwork( builder );
	ASSERT_TRUE( builder.AddParameter( { "speed", 0.f, 0.f, 1.f } ) != cmf::AnimationNetworkBuilder::INVALID_PARAMETER );
	auto& root = builder.GetRootGraph();
	ExpectOk( root.SetAnimation( root.AddNode( cmf::AnimationNodeType::AnimationSample ), ASSET_PATH, MOVE_ANIMATION ) );
	builder.Clear();
	EXPECT_TRUE( builder.GetRootGraph().GetNodes().empty() );
	EXPECT_TRUE( builder.GetParameters().empty() );
	EXPECT_EQ( loader.GetReferenceBalance( ASSET_PATH ), 0 );
}

// Runtime network

TEST_F( RuntimeFixture, NetworkLoadsSavedFile )
{
	const auto file = BuildCompleteNetwork();
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( file ) );
	EXPECT_TRUE( network.IsReady() );
	EXPECT_TRUE( network.GetErrors().empty() );

	const auto* data = network.GetData();
	ASSERT_NE( data, nullptr );
	EXPECT_EQ( data->graphs.size(), 2u );
	EXPECT_EQ( data->assets.size(), 1u );
	EXPECT_EQ( cmf::ToStdStringView( data->parameters[0].name ), "speed" );
	EXPECT_EQ( network.GetAssetData( 0 ), &asset.data );
	ASSERT_NE( network.GetSkeleton(), nullptr );
	EXPECT_EQ( network.GetSkeleton()->bones.size(), BONE_COUNT );
}

TEST_F( RuntimeFixture, NetworkReadsEmbeddedAssetsFromTheFile )
{
	const auto file = BuildCompleteNetwork( true );
	const auto requests = loader.GetRequests().size();
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( file ) );
	EXPECT_EQ( loader.GetRequests().size(), requests );
	EXPECT_TRUE( network.IsReady() );
	EXPECT_TRUE( network.GetErrors().empty() );
	EXPECT_TRUE( network.GetWarnings().empty() );
	ASSERT_NE( network.GetSkeleton(), nullptr );
	EXPECT_EQ( cmf::ToStdStringView( network.GetSkeleton()->bones[LEAF_BONE] ), "Leaf" );

	const auto& sample = GetSampleNode( network.GetData()->graphs[1], COMPLETE_NETWORK_SAMPLE_NODE );
	const auto* animation = network.GetAnimation( sample );
	ASSERT_NE( animation, nullptr );
	EXPECT_EQ( cmf::ToStdStringView( animation->name ), MOVE_ANIMATION );
	EXPECT_EQ( animation->duration, ANIMATION_DURATION );
}

TEST_F( RuntimeFixture, NetworkResolvesExternalAnimations )
{
	const auto file = BuildCompleteNetwork();
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( file ) );
	const auto& sample = GetSampleNode( network.GetData()->graphs[1], COMPLETE_NETWORK_SAMPLE_NODE );
	EXPECT_EQ( network.GetAnimation( sample ), asset.data.animations.data() );
}

TEST_F( RuntimeFixture, NetworkWaitsForDeferredAssets )
{
	const auto file = BuildCompleteNetwork();
	loader.SetDeferred( true );
	{
		cmf::AnimationNetwork network( loader );
		ASSERT_TRUE( network.Load( file ) );
		EXPECT_TRUE( network.HasPendingAssets() );
		EXPECT_FALSE( network.IsReady() );
		EXPECT_TRUE( network.GetErrors().empty() );
		loader.Complete();
		EXPECT_TRUE( network.IsReady() );

		// Reloading releases the assets of the previous file; its late callbacks are ignored
		ASSERT_TRUE( network.Load( file ) );
		ASSERT_TRUE( network.Load( file ) );
		loader.Complete();
		EXPECT_TRUE( network.IsReady() );
	}
	loader.Complete();
	EXPECT_EQ( loader.GetReferenceBalance( ASSET_PATH ), 0 );
}

TEST_F( RuntimeFixture, IncompleteNetworkLoadsWithErrors )
{
	cmf::AnimationNetworkBuilder builder( loader );
	const auto output = SetUpNetwork( builder );
	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( SaveNetwork( builder ) ) );
	EXPECT_FALSE( network.IsReady() );
	EXPECT_TRUE( HasErrorContaining( network.GetErrors(), "graph 0: Output #0: input 'pose' is not connected" ) );

	auto& root = builder.GetRootGraph();
	const auto blend = root.AddNode( cmf::AnimationNodeType::Blend );
	const auto sample = root.AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( root.SetAnimation( sample, ASSET_PATH, "Missing" ) );
	ASSERT_TRUE( root.Connect( { sample, 0 }, { blend, 1 } ) );
	ASSERT_TRUE( root.Connect( { blend, 0 }, { output, 0 } ) );
	ASSERT_TRUE( network.Load( SaveNetwork( builder ) ) );
	EXPECT_FALSE( network.IsReady() );
	const auto errors = network.GetErrors();
	EXPECT_TRUE( HasErrorContaining( errors, "Blend #1: input 'poseA' is not connected" ) );
	EXPECT_FALSE( HasErrorContaining( errors, "animation 'Missing' not found" ) );
	EXPECT_TRUE( HasErrorContaining( network.GetWarnings(), "animation 'Missing' not found" ) );
}

TEST_F( RuntimeFixture, NetworkWithoutFileIsNotReady )
{
	const cmf::AnimationNetwork network( loader );
	EXPECT_EQ( network.GetData(), nullptr );
	EXPECT_FALSE( network.IsReady() );
	EXPECT_TRUE( HasErrorContaining( network.GetErrors(), "No network is loaded" ) );
}

// Malformed files

TEST_F( RuntimeFixture, CorruptedFilesAreRejected )
{
	cmf::AnimationNetwork network( loader );
	const auto valid = BuildCompleteNetwork();

	auto file = valid;
	file.back() ^= 1;
	EXPECT_EQ( network.Load( file ).error, "CRC mismatch" );

	file = valid;
	file.resize( file.size() / 2 );
	EXPECT_FALSE( network.Load( file ) );

	file = valid;
	file[0] = 'x';
	EXPECT_EQ( network.Load( file ).error, "Invalid file signature" );

	EXPECT_FALSE( network.Load( {} ) );
	EXPECT_EQ( network.GetData(), nullptr );
}

TEST_F( RuntimeFixture, InvalidEdgesAreRejected )
{
	cmf::AnimationNetwork network( loader );
	const auto valid = BuildCompleteNetwork();
	constexpr uint32_t badSocket = 99;

	auto file = valid;
	GetNetworkData( file ).graphs[1].edges[0].to.socket = badSocket;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Edge references an invalid socket" );

	// Feed the blend output back into the random switch: blend -> random -> blend
	file = valid;
	auto& sub = GetNetworkData( file ).graphs[1];
	constexpr size_t moveToRandomEdge = 4;
	constexpr size_t blendToOutputEdge = 7;
	auto& edge = sub.edges[moveToRandomEdge];
	ASSERT_EQ( edge.to.socket, 1u );
	edge.from = sub.edges[blendToOutputEdge].from;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Edges form a cycle" );

	file = valid;
	GetNetworkData( file ).graphs[1].edges[1].to = GetNetworkData( file ).graphs[1].edges[0].to;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Input has more than one incoming edge" );
}

TEST_F( RuntimeFixture, InvalidIndicesAreRejected )
{
	cmf::AnimationNetwork network( loader );
	const auto valid = BuildCompleteNetwork();
	constexpr uint32_t badIndex = 42;

	auto file = valid;
	GetNetworkData( file ).graphs[0].subgraphNodes[0].graphIndex = 0;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Subgraph node or state references an invalid graph" );

	file = valid;
	// Identical arrays are stored once in the file (parameterNodes shares its bytes with interfaceOutputs), so the
	// parameter table is emptied instead of changing the index
	GetNetworkData( file ).parameters.byteSize = 0;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0 node 2: invalid parameter index" );

	file = valid;
	GetNetworkData( file ).skeletonAsset = badIndex;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Invalid skeleton asset index" );

	file = valid;
	GetNetworkData( file ).graphs[0].nodes[0].type = static_cast<cmf::AnimationNodeType>( badIndex );
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0: Node 0: invalid type" );

	file = valid;
	GetNetworkData( file ).graphs[0].nodes[0].dataIndex = badIndex;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0: Node 0: invalid node data index" );

	file = valid;
	GetNetworkData( file ).graphs[0].nodes[1].type = cmf::AnimationNodeType::Blend;
	UpdateCrc( file );
	EXPECT_EQ( network.Load( file ).error, "Graph 0: Node 1: unexpected node data" );
}

}
