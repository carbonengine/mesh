// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"

#include "networktestutils.h"

#include <array>
#include <cstddef>

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
		return builder.Save();
	}
};

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

bool HasErrorContaining( const std::vector<std::string>& errors, const std::string& text )
{
	return std::any_of( errors.begin(), errors.end(), [&text]( const std::string& error ) { return error.find( text ) != std::string::npos; } );
}

}

// Builder serialization

TEST_F( RuntimeFixture, SavedNetworkLoadsIntoBuilder )
{
	const auto file = BuildCompleteNetwork();

	cmf::AnimationNetworkBuilder builder( loader );
	ASSERT_TRUE( builder.Load( file.data(), file.size() ) );
	EXPECT_TRUE( builder.GetErrors().empty() );
	EXPECT_EQ( builder.GetSkeletonPath(), ASSET_PATH );
	ASSERT_EQ( builder.GetParameters().size(), 1u );
	EXPECT_EQ( builder.GetParameters()[0].name, "speed" );

	const auto& root = builder.GetRootGraph();
	EXPECT_EQ( root.GetNodes().size(), 3u );
	EXPECT_EQ( root.GetEdges().size(), 2u );
	const auto subgraphNode = root.GetNodes()[1];
	EXPECT_EQ( root.GetNode( subgraphNode )->name, "Locomotion" );
	const auto& sub = *root.GetSubgraph( subgraphNode );
	EXPECT_EQ( sub.GetNodes().size(), 8u );
	EXPECT_EQ( sub.GetEdges().size(), 8u );
	EXPECT_EQ( root.GetInputName( { subgraphNode, 0 } ), "speed" );

	const auto random = sub.GetNodes()[7];
	EXPECT_EQ( sub.GetNode( random )->type, cmf::AnimationNodeType::RandomSwitch );
	const auto* randomSettings = sub.GetSettings<cmf::AnimationRandomSwitchSettings>( random );
	ASSERT_NE( randomSettings, nullptr );
	EXPECT_EQ( randomSettings->weights, ( std::vector<float>{ RANDOM_WEIGHTS.begin(), RANDOM_WEIGHTS.end() } ) );
	EXPECT_TRUE( randomSettings->rerollOnActivate );
	EXPECT_EQ( sub.GetSettings<cmf::AnimationSampleSettings>( random ), nullptr ) << "settings of another node type";

	EXPECT_EQ( builder.Save(), file ) << "saving a loaded network reproduces the file";
}

TEST_F( RuntimeFixture, EmbeddedAssetsSurviveSaving )
{
	const auto file = BuildCompleteNetwork( true );
	const auto requests = loader.GetRequests().size();

	cmf::AnimationNetworkBuilder builder( loader );
	ASSERT_TRUE( builder.Load( file.data(), file.size() ) );
	EXPECT_EQ( builder.GetEmbeddedAssetPaths(), std::vector<std::string>{ EMBEDDED_PATH } );
	EXPECT_EQ( *builder.GetEmbeddedAsset( EMBEDDED_PATH ), asset.BuildCmfFile() );
	EXPECT_NE( builder.GetSkeleton(), nullptr );
	EXPECT_EQ( loader.GetRequests().size(), requests );
	EXPECT_EQ( builder.Save(), file );
}

TEST_F( RuntimeFixture, BuilderRejectsMalformedFilesAndKeepsItsContents )
{
	cmf::AnimationNetworkBuilder builder( loader );
	networktest::SetUpNetwork( builder );
	auto file = BuildCompleteNetwork();
	file.back() ^= 1;
	EXPECT_FALSE( builder.Load( file.data(), file.size() ) );
	EXPECT_EQ( builder.GetSkeletonPath(), ASSET_PATH );
	EXPECT_EQ( builder.GetRootGraph().GetNodes().size(), 1u );
}

TEST_F( RuntimeFixture, ClearReleasesEverything )
{
	cmf::AnimationNetworkBuilder builder( loader );
	const auto file = BuildCompleteNetwork();
	ASSERT_TRUE( builder.Load( file.data(), file.size() ) );
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
	ASSERT_NE( network.GetSkeleton(), nullptr );
	EXPECT_EQ( cmf::ToStdStringView( network.GetSkeleton()->bones[LEAF_BONE] ), "Leaf" );
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
	SetUpNetwork( builder );
	builder.GetRootGraph().AddNode( cmf::AnimationNodeType::Blend );
	const auto sample = builder.GetRootGraph().AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( builder.GetRootGraph().SetAnimation( sample, ASSET_PATH, "Missing" ) );

	cmf::AnimationNetwork network( loader );
	ASSERT_TRUE( network.Load( builder.Save() ) );
	EXPECT_FALSE( network.IsReady() );
	const auto errors = network.GetErrors();
	EXPECT_TRUE( HasErrorContaining( errors, "graph 0: Output #0: input 'pose' is not connected" ) );
	EXPECT_TRUE( HasErrorContaining( errors, "Blend #1: input 'poseA' is not connected" ) );
	EXPECT_TRUE( HasErrorContaining( errors, "animation 'Missing' not found" ) );
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
	EXPECT_EQ( network.Load( file ).error, "Subgraph node references an invalid graph" );

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
