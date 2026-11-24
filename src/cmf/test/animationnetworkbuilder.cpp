// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"

#include "networktestutils.h"

namespace networktest
{
namespace
{

constexpr const char* OTHER_PATH = "assets/other.cmf";

struct NetworkFixture : public ::testing::Test
{
	TestAsset asset;
	FakeLoader loader;

	NetworkFixture()
	{
		loader.AddAsset( ASSET_PATH, &asset.data );
		loader.AddAsset( OTHER_PATH, &asset.data );
	}
};

/** Checks a fixed input of a new node against its description. */
void ExpectInputMatches( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id, uint32_t socket, const cmf::AnimationSocketInfo& info )
{
	EXPECT_EQ( graph.GetInputName( { id, socket } ), info.name );
	EXPECT_EQ( graph.GetInputType( { id, socket } ), info.type );
	EXPECT_EQ( graph.GetNode( id )->inputConstants[socket], info.defaultValue );
}

/** Checks the fixed sockets of a new node against the description of its type. */
void ExpectSocketsMatch( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id, const cmf::AnimationNodeTypeInfo& info )
{
	constexpr uint32_t defaultPoseInputs = 2;
	const auto extra = info.layout == cmf::AnimationSocketLayout::Switch ? defaultPoseInputs : 0;
	ASSERT_EQ( graph.GetInputCount( id ), info.inputCount + extra );
	for( uint32_t i = 0; i < info.inputCount; ++i )
	{
		ExpectInputMatches( graph, id, i, info.inputs[i] );
	}
	ASSERT_EQ( graph.GetOutputCount( id ), info.hasOutput ? 1u : 0u );
	if( info.hasOutput )
	{
		EXPECT_EQ( graph.GetOutputType( { id, 0 } ), info.outputType );
		EXPECT_EQ( graph.GetOutputName( { id, 0 } ), info.outputName );
	}
}

bool ContainsText( const std::vector<std::string>& messages, const std::string& text )
{
	return std::any_of( messages.begin(), messages.end(), [&text]( const std::string& message ) { return message.find( text ) != std::string::npos; } );
}

bool HasErrorContaining( const cmf::AnimationNetworkBuilder& network, const std::string& text )
{
	return ContainsText( network.GetErrors(), text );
}

bool HasWarningContaining( const cmf::AnimationNetworkBuilder& network, const std::string& text )
{
	return ContainsText( network.GetWarnings(), text );
}
}

// Editing

TEST_F( NetworkFixture, NodesHaveSocketsOfTheirType )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();

	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	ASSERT_TRUE( graph.IsValid( blend ) );
	EXPECT_EQ( graph.GetInputCount( blend ), 4u );
	EXPECT_EQ( graph.GetOutputCount( blend ), 1u );
	EXPECT_EQ( graph.GetInputType( { blend, 0 } ), cmf::AnimationSocketType::Pose );
	EXPECT_EQ( graph.GetInputType( { blend, 2 } ), cmf::AnimationSocketType::Float );
	EXPECT_EQ( graph.GetInputType( { blend, 3 } ), cmf::AnimationSocketType::BoneWeights );
	EXPECT_EQ( graph.GetInputName( { blend, 2 } ), "alpha" );

	const auto sample = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
	EXPECT_EQ( graph.GetInputName( { sample, 2 } ), "speed" );
	EXPECT_EQ( graph.GetNode( sample )->inputConstants, ( std::vector<float>{ 0.f, 1.f, 1.f } ) );

	const auto weights = graph.AddNode( cmf::AnimationNodeType::NamedBoneWeights );
	EXPECT_EQ( graph.GetOutputType( { weights, 0 } ), cmf::AnimationSocketType::BoneWeights );
}

TEST_F( NetworkFixture, NodeTypeInfoDescribesTheSocketsOfNewNodes )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& root = network.GetRootGraph();
	// A subgraph accepts every node type, including interface nodes
	auto& graph = *root.GetSubgraph( root.AddNode( cmf::AnimationNodeType::Subgraph ) );
	for( uint32_t t = 0; t < static_cast<uint32_t>( cmf::AnimationNodeType::Count ); ++t )
	{
		const auto type = static_cast<cmf::AnimationNodeType>( t );
		const auto info = cmf::GetAnimationNodeTypeInfo( type );
		SCOPED_TRACE( info.name );
		const auto id = graph.AddNode( type );
		ASSERT_TRUE( id.IsSet() );
		if( info.layout == cmf::AnimationSocketLayout::Fixed || info.layout == cmf::AnimationSocketLayout::Switch )
		{
			ExpectSocketsMatch( graph, id, info );
		}
		EXPECT_EQ( std::holds_alternative<std::monostate>( graph.GetNode( id )->settings ), !info.hasSettings );
	}
	EXPECT_EQ( cmf::GetAnimationNodeTypeInfo( cmf::AnimationNodeType::Output ).hasOutput, false );
	EXPECT_EQ( cmf::GetAnimationNodeTypeInfo( cmf::AnimationNodeType::Subgraph ).layout, cmf::AnimationSocketLayout::Subgraph );
}

TEST_F( NetworkFixture, RootGraphAllowsOnlyOneOutputAndNoSubgraphInputs )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	EXPECT_TRUE( graph.AddNode( cmf::AnimationNodeType::Output ).IsSet() );
	EXPECT_FALSE( graph.AddNode( cmf::AnimationNodeType::Output ).IsSet() );
	EXPECT_FALSE( graph.AddNode( cmf::AnimationNodeType::SubgraphInput ).IsSet() );
}

TEST_F( NetworkFixture, ConnectChecksSocketsAndTypes )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );

	EXPECT_TRUE( graph.Connect( { rest, 0 }, { blend, 0 } ) );
	EXPECT_FALSE( graph.Connect( { rest, 0 }, { blend, 0 } ) ) << "input is already connected";
	EXPECT_FALSE( graph.Connect( { rest, 0 }, { blend, 2 } ) ) << "pose into float";
	EXPECT_FALSE( graph.Connect( { rest, 1 }, { blend, 1 } ) ) << "output socket doesn't exist";
	EXPECT_FALSE( graph.Connect( { rest, 0 }, { blend, 4 } ) ) << "input socket doesn't exist";
	EXPECT_FALSE( graph.Connect( { rest, 0 }, { cmf::AnimationNodeId{}, 0 } ) ) << "invalid node";

	ASSERT_TRUE( graph.GetConnection( { blend, 0 } ).has_value() );
	EXPECT_EQ( graph.GetConnection( { blend, 0 } ).value_or( cmf::AnimationSocketRef{} ).node, rest );
	EXPECT_TRUE( graph.Disconnect( { blend, 0 } ) );
	EXPECT_FALSE( graph.Disconnect( { blend, 0 } ) );
	EXPECT_TRUE( graph.GetEdges().empty() );
}

TEST_F( NetworkFixture, ConnectRejectsCycles )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto blendA = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto blendB = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto blendC = graph.AddNode( cmf::AnimationNodeType::Blend );

	ASSERT_TRUE( graph.Connect( { blendA, 0 }, { blendB, 0 } ) );
	ASSERT_TRUE( graph.Connect( { blendB, 0 }, { blendC, 0 } ) );
	EXPECT_FALSE( graph.Connect( { blendC, 0 }, { blendA, 0 } ) );
	EXPECT_FALSE( graph.Connect( { blendA, 0 }, { blendA, 1 } ) );
	EXPECT_TRUE( graph.Connect( { blendA, 0 }, { blendC, 1 } ) ) << "diamonds are not cycles";
}

TEST_F( NetworkFixture, RemoveNodeRemovesEdgesAndInvalidatesHandle )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	ASSERT_TRUE( graph.Connect( { rest, 0 }, { blend, 0 } ) );

	EXPECT_TRUE( graph.RemoveNode( rest ) );
	EXPECT_FALSE( graph.IsValid( rest ) );
	EXPECT_FALSE( graph.RemoveNode( rest ) );
	EXPECT_TRUE( graph.GetEdges().empty() );
	EXPECT_EQ( graph.GetNode( rest ), nullptr );

	const auto newNode = graph.AddNode( cmf::AnimationNodeType::RestPose );
	EXPECT_NE( newNode, rest );
	EXPECT_FALSE( graph.IsValid( rest ) );
	EXPECT_EQ( graph.GetNodes().size(), 2u );
}

TEST_F( NetworkFixture, InputConstantsOnlyApplyToFloatInputs )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	const float alpha = 0.25f;
	EXPECT_TRUE( graph.SetInputConstant( { blend, 2 }, alpha ) );
	EXPECT_EQ( graph.GetNode( blend )->inputConstants[2], alpha );
	EXPECT_FALSE( graph.SetInputConstant( { blend, 0 }, alpha ) );
	EXPECT_FALSE( graph.SetInputConstant( { blend, 4 }, alpha ) );
}

TEST_F( NetworkFixture, SettersCheckNodeType )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	EXPECT_FALSE( graph.SetAnimation( blend, ASSET_PATH, "" ) );
	EXPECT_FALSE( graph.SetBoneMask( blend, "mask" ) );
	EXPECT_FALSE( graph.SetPoseInputCount( blend, 3 ) );
	EXPECT_FALSE( graph.SetExpression( blend, "1", {} ) );
	EXPECT_EQ( loader.GetRequests().size(), 0u );
}

TEST_F( NetworkFixture, SwitchPoseInputCountRemovesExtraEdges )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	const auto switchNode = graph.AddNode( cmf::AnimationNodeType::Switch );
	EXPECT_EQ( graph.GetInputCount( switchNode ), 3u );

	ASSERT_TRUE( graph.SetPoseInputCount( switchNode, 4 ) );
	EXPECT_EQ( graph.GetInputCount( switchNode ), 5u );
	EXPECT_EQ( graph.GetInputName( { switchNode, 4 } ), "pose3" );
	ASSERT_TRUE( graph.Connect( { rest, 0 }, { switchNode, 4 } ) );
	ASSERT_TRUE( graph.Connect( { rest, 0 }, { switchNode, 1 } ) );

	ASSERT_TRUE( graph.SetPoseInputCount( switchNode, 2 ) );
	EXPECT_EQ( graph.GetEdges().size(), 1u );
	EXPECT_FALSE( graph.SetPoseInputCount( switchNode, 0 ) );
}

TEST_F( NetworkFixture, ExpressionIsParsedWhenSet )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto expression = graph.AddNode( cmf::AnimationNodeType::Expression );

	ASSERT_TRUE( graph.SetExpression( expression, "min( speed / 4, 1 ) * weight", { "speed", "weight" } ) );
	EXPECT_EQ( graph.GetInputCount( expression ), 2u );
	EXPECT_EQ( graph.GetInputName( { expression, 1 } ), "weight" );

	const auto syntaxError = graph.SetExpression( expression, "speed * (", { "speed" } );
	EXPECT_FALSE( syntaxError );
	EXPECT_FALSE( syntaxError.error.empty() );
	EXPECT_FALSE( graph.SetExpression( expression, "unknown + 1", {} ) );
	EXPECT_FALSE( graph.SetExpression( expression, "a", { "a", "a" } ) );
	EXPECT_EQ( graph.GetSettings<cmf::AnimationExpressionSettings>( expression )->expression, "min( speed / 4, 1 ) * weight" ) << "failed calls leave the node unchanged";
}

TEST_F( NetworkFixture, ParametersHaveUniqueNames )
{
	cmf::AnimationNetworkBuilder network( loader );
	cmf::AnimationParameter speed;
	speed.name = "speed";
	EXPECT_EQ( network.AddParameter( speed ), 0u );
	EXPECT_EQ( network.AddParameter( speed ), cmf::AnimationNetworkBuilder::INVALID_PARAMETER );

	cmf::AnimationParameter direction;
	direction.name = "direction";
	EXPECT_EQ( network.AddParameter( direction ), 1u );
	EXPECT_EQ( network.FindParameter( "direction" ), 1u );
	EXPECT_EQ( network.FindParameter( "missing" ), cmf::AnimationNetworkBuilder::INVALID_PARAMETER );

	EXPECT_FALSE( network.SetParameter( 1, speed ) ) << "name is taken";
	direction.minValue = 1.f;
	direction.maxValue = 0.f;
	EXPECT_FALSE( network.SetParameter( 1, direction ) ) << "min > max";
}

TEST_F( NetworkFixture, RemovingReferencedParameterFails )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	cmf::AnimationParameter parameter;
	parameter.name = "a";
	network.AddParameter( parameter );
	parameter.name = "b";
	network.AddParameter( parameter );

	const auto node = graph.AddNode( cmf::AnimationNodeType::Parameter );
	ASSERT_TRUE( graph.SetParameter( node, 1 ) );
	EXPECT_FALSE( graph.SetParameter( node, 2 ) );
	EXPECT_FALSE( network.RemoveParameter( 1 ) );

	ASSERT_TRUE( network.RemoveParameter( 0 ) );
	EXPECT_EQ( graph.GetSettings<cmf::AnimationParameterSettings>( node )->parameterIndex, 0u ) << "references shift down";
	EXPECT_EQ( network.GetParameters()[0].name, "b" );
}

TEST_F( NetworkFixture, SubgraphSocketsFollowItsInterface )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto subgraphNode = graph.AddNode( cmf::AnimationNodeType::Subgraph );
	auto& subgraph = *graph.GetSubgraph( subgraphNode );
	EXPECT_EQ( subgraph.GetParent(), &graph );

	const auto poseIn = subgraph.AddNode( cmf::AnimationNodeType::SubgraphInput );
	const auto alphaIn = subgraph.AddNode( cmf::AnimationNodeType::SubgraphInput );
	const auto output = subgraph.AddNode( cmf::AnimationNodeType::Output );
	ASSERT_TRUE( subgraph.SetInterfaceSocket( poseIn, "pose", cmf::AnimationSocketType::Pose ) );
	ASSERT_TRUE( subgraph.SetInterfaceSocket( alphaIn, "alpha", cmf::AnimationSocketType::Float ) );
	ASSERT_TRUE( subgraph.SetInterfaceSocket( output, "result", cmf::AnimationSocketType::Pose ) );

	EXPECT_EQ( graph.GetInputCount( subgraphNode ), 2u );
	EXPECT_EQ( graph.GetOutputCount( subgraphNode ), 1u );
	EXPECT_EQ( graph.GetInputType( { subgraphNode, 1 } ), cmf::AnimationSocketType::Float );
	EXPECT_EQ( graph.GetInputName( { subgraphNode, 1 } ), "alpha" );
	EXPECT_EQ( graph.GetOutputName( { subgraphNode, 0 } ), "result" );

	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	const auto expression = graph.AddNode( cmf::AnimationNodeType::Expression );
	ASSERT_TRUE( graph.SetExpression( expression, "0.5", {} ) );
	ASSERT_TRUE( graph.Connect( { rest, 0 }, { subgraphNode, 0 } ) );
	ASSERT_TRUE( graph.Connect( { expression, 0 }, { subgraphNode, 1 } ) );
	const float alpha = 0.75f;
	ASSERT_TRUE( graph.SetInputConstant( { subgraphNode, 1 }, alpha ) );

	// Removing the first interface input shifts the second one down
	ASSERT_TRUE( subgraph.RemoveNode( poseIn ) );
	EXPECT_EQ( graph.GetInputCount( subgraphNode ), 1u );
	ASSERT_EQ( graph.GetEdges().size(), 1u );
	EXPECT_EQ( graph.GetEdges()[0].from.node, expression );
	EXPECT_EQ( graph.GetEdges()[0].to.socket, 0u );
	EXPECT_EQ( graph.GetNode( subgraphNode )->inputConstants, std::vector<float>{ alpha } );

	// Changing the type of an interface socket disconnects it
	ASSERT_TRUE( subgraph.SetInterfaceSocket( alphaIn, "alpha", cmf::AnimationSocketType::BoneWeights ) );
	EXPECT_TRUE( graph.GetEdges().empty() );
}

TEST_F( NetworkFixture, RootOutputMustBePose )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto output = graph.AddNode( cmf::AnimationNodeType::Output );
	EXPECT_FALSE( graph.SetInterfaceSocket( output, "out", cmf::AnimationSocketType::Float ) );
	EXPECT_TRUE( graph.SetInterfaceSocket( output, "out", cmf::AnimationSocketType::Pose ) );
}

TEST_F( NetworkFixture, EditsChangeGeneration )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	auto generation = network.GetGeneration();
	const auto node = graph.AddNode( cmf::AnimationNodeType::RestPose );
	EXPECT_NE( network.GetGeneration(), generation );
	generation = network.GetGeneration();
	graph.SetNodeName( node, "rest" );
	EXPECT_NE( network.GetGeneration(), generation );
	const auto subgraphNode = graph.AddNode( cmf::AnimationNodeType::Subgraph );
	generation = network.GetGeneration();
	graph.GetSubgraph( subgraphNode )->AddNode( cmf::AnimationNodeType::RestPose );
	EXPECT_NE( network.GetGeneration(), generation ) << "subgraph edits change the network generation";
}

// Validation

TEST_F( NetworkFixture, MinimalNetworkIsReady )
{
	cmf::AnimationNetworkBuilder network( loader );
	const auto output = SetUpNetwork( network );
	auto& graph = network.GetRootGraph();
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	ASSERT_TRUE( graph.Connect( { rest, 0 }, { output, 0 } ) );

	EXPECT_TRUE( network.GetErrors().empty() );
	EXPECT_TRUE( network.IsReady() );
	ASSERT_NE( network.GetSkeleton(), nullptr );
	EXPECT_EQ( network.GetSkeleton()->bones.size(), BONE_COUNT );
}

TEST_F( NetworkFixture, ErrorsDescribeProblems )
{
	cmf::AnimationNetworkBuilder network( loader );
	EXPECT_TRUE( HasErrorContaining( network, "No skeleton" ) );
	EXPECT_TRUE( HasErrorContaining( network, "no Output node" ) );

	const auto output = SetUpNetwork( network );
	auto& graph = network.GetRootGraph();
	EXPECT_TRUE( HasErrorContaining( network, "input 'pose' is not connected" ) );

	const auto sample = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( graph.Connect( { sample, 0 }, { output, 0 } ) );
	EXPECT_TRUE( HasWarningContaining( network, "no animation asset" ) );
	ASSERT_TRUE( graph.SetAnimation( sample, ASSET_PATH, "Missing" ) );
	EXPECT_TRUE( HasWarningContaining( network, "animation 'Missing' not found" ) );
	ASSERT_TRUE( graph.SetAnimation( sample, ASSET_PATH, MOVE_ANIMATION ) );

	const auto mask = graph.AddNode( cmf::AnimationNodeType::NamedBoneWeights );
	ASSERT_TRUE( graph.SetBoneMask( mask, "Lower" ) );
	EXPECT_TRUE( HasWarningContaining( network, "bone mask 'Lower' not found" ) );
	ASSERT_TRUE( graph.SetBoneMask( mask, MASK_NAME ) );

	const auto weights = graph.AddNode( cmf::AnimationNodeType::BoneWeights );
	ASSERT_TRUE( graph.SetBoneWeights( weights, { { "Tail", 1.f } }, 0.f ) );
	EXPECT_TRUE( HasWarningContaining( network, "bone 'Tail' not found" ) );
	ASSERT_TRUE( graph.SetBoneWeights( weights, { { "Leaf", 1.f } }, 0.f ) );
	// Warnings don't prevent evaluation
	EXPECT_FALSE( HasErrorContaining( network, "not found" ) );

	const auto random = graph.AddNode( cmf::AnimationNodeType::RandomSwitch );
	ASSERT_TRUE( graph.SetRandomWeights( random, { 1.f } ) );
	EXPECT_TRUE( HasErrorContaining( network, "number of random weights" ) );
	EXPECT_FALSE( graph.SetRandomWeights( random, { 1.f, -1.f } ) );
	ASSERT_TRUE( graph.RemoveNode( random ) );

	EXPECT_TRUE( network.GetErrors().empty() );
	EXPECT_TRUE( network.GetWarnings().empty() );
	EXPECT_TRUE( network.IsReady() );
}

TEST_F( NetworkFixture, SubgraphErrorsIncludeTheirPath )
{
	cmf::AnimationNetworkBuilder network( loader );
	SetUpNetwork( network );
	auto& graph = network.GetRootGraph();
	const auto subgraphNode = graph.AddNode( cmf::AnimationNodeType::Subgraph );
	graph.SetNodeName( subgraphNode, "Locomotion" );
	graph.GetSubgraph( subgraphNode )->AddNode( cmf::AnimationNodeType::Blend );
	EXPECT_TRUE( HasErrorContaining( network, "root/Subgraph #1 'Locomotion': Blend #0: input 'poseA' is not connected" ) );
}

// Asset loading

TEST_F( NetworkFixture, DeferredLoadingIsPendingUntilComplete )
{
	loader.SetDeferred( true );
	cmf::AnimationNetworkBuilder network( loader );
	const auto output = SetUpNetwork( network );
	auto& graph = network.GetRootGraph();
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	ASSERT_TRUE( graph.Connect( { rest, 0 }, { output, 0 } ) );

	EXPECT_TRUE( network.HasPendingAssets() );
	EXPECT_TRUE( network.GetErrors().empty() );
	EXPECT_FALSE( network.IsReady() );
	EXPECT_EQ( network.GetSkeleton(), nullptr );

	const auto generation = network.GetGeneration();
	loader.Complete();
	EXPECT_FALSE( network.HasPendingAssets() );
	EXPECT_TRUE( network.IsReady() );
	EXPECT_NE( network.GetGeneration(), generation );
}

TEST_F( NetworkFixture, FailedLoadIsAnError )
{
	cmf::AnimationNetworkBuilder network( loader );
	network.SetSkeleton( "missing.cmf", "" );
	EXPECT_FALSE( network.HasPendingAssets() );
	EXPECT_TRUE( HasErrorContaining( network, "Failed to load skeleton asset 'missing.cmf'" ) );
}

TEST_F( NetworkFixture, SharedAssetsAreRequestedOnceAndReleased )
{
	{
		cmf::AnimationNetworkBuilder network( loader );
		SetUpNetwork( network );
		auto& graph = network.GetRootGraph();
		const auto sampleA = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
		const auto sampleB = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
		ASSERT_TRUE( graph.SetAnimation( sampleA, ASSET_PATH, MOVE_ANIMATION ) );
		ASSERT_TRUE( graph.SetAnimation( sampleB, OTHER_PATH, LIFT_ANIMATION ) );
		EXPECT_EQ( loader.GetRequests(), ( std::vector<std::string>{ ASSET_PATH, OTHER_PATH } ) );

		// Switching to an already referenced path doesn't request it again
		ASSERT_TRUE( graph.SetAnimation( sampleB, ASSET_PATH, LIFT_ANIMATION ) );
		EXPECT_EQ( loader.GetRequests().size(), 2u );
		EXPECT_EQ( loader.GetReleases(), std::vector<std::string>{ OTHER_PATH } );

		ASSERT_TRUE( graph.RemoveNode( sampleA ) );
		EXPECT_EQ( loader.GetReleases().size(), 1u ) << "still referenced by the skeleton and sampleB";
	}
	EXPECT_EQ( loader.GetReferenceBalance( ASSET_PATH ), 0 );
	EXPECT_EQ( loader.GetReferenceBalance( OTHER_PATH ), 0 );
}

TEST_F( NetworkFixture, RemovingSubgraphReleasesItsAssets )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto& graph = network.GetRootGraph();
	const auto subgraphNode = graph.AddNode( cmf::AnimationNodeType::Subgraph );
	auto& subgraph = *graph.GetSubgraph( subgraphNode );
	const auto sample = subgraph.AddNode( cmf::AnimationNodeType::AnimationSample );
	ASSERT_TRUE( subgraph.SetAnimation( sample, OTHER_PATH, "" ) );
	EXPECT_EQ( loader.GetReferenceBalance( OTHER_PATH ), 1 );
	ASSERT_TRUE( graph.RemoveNode( subgraphNode ) );
	EXPECT_EQ( loader.GetReferenceBalance( OTHER_PATH ), 0 );
}

TEST_F( NetworkFixture, CallbacksAfterReleaseOrDestructionAreIgnored )
{
	loader.SetDeferred( true );
	{
		cmf::AnimationNetworkBuilder network( loader );
		network.SetSkeleton( ASSET_PATH, "" );
		network.SetSkeleton( OTHER_PATH, "" );
		loader.Complete();
		EXPECT_EQ( network.GetAssetData( ASSET_PATH ), nullptr ) << "released before the load completed";
		EXPECT_NE( network.GetAssetData( OTHER_PATH ), nullptr );

		network.SetSkeleton( ASSET_PATH, "" );
	}
	loader.Complete();
	EXPECT_EQ( loader.GetReferenceBalance( ASSET_PATH ), 0 );
	EXPECT_EQ( loader.GetReferenceBalance( OTHER_PATH ), 0 );
}

TEST_F( NetworkFixture, EmbeddedAssetsBypassTheLoader )
{
	cmf::AnimationNetworkBuilder network( loader );
	const std::string embeddedPath = "embedded/character.cmf";
	ASSERT_TRUE( network.EmbedAsset( embeddedPath, asset.BuildCmfFile() ) );
	EXPECT_EQ( network.GetEmbeddedAssetPaths(), std::vector<std::string>{ embeddedPath } );
	ASSERT_NE( network.GetEmbeddedAsset( embeddedPath ), nullptr );

	network.SetSkeleton( embeddedPath, SKELETON_NAME );
	EXPECT_TRUE( loader.GetRequests().empty() );
	ASSERT_NE( network.GetSkeleton(), nullptr );
	EXPECT_EQ( cmf::ToStdStringView( network.GetSkeleton()->bones[LEAF_BONE] ), "Leaf" );

	// Removing the embedded asset makes the reference go through the loader
	loader.AddAsset( embeddedPath, &asset.data );
	ASSERT_TRUE( network.RemoveEmbeddedAsset( embeddedPath ) );
	EXPECT_EQ( loader.GetRequests(), std::vector<std::string>{ embeddedPath } );
	EXPECT_EQ( network.GetAssetData( embeddedPath ), &asset.data );
	EXPECT_FALSE( network.RemoveEmbeddedAsset( embeddedPath ) );
}

TEST_F( NetworkFixture, EmbeddingAReferencedPathReleasesTheLoaderAsset )
{
	loader.SetDeferred( true );
	cmf::AnimationNetworkBuilder network( loader );
	network.SetSkeleton( ASSET_PATH, "" );
	ASSERT_TRUE( network.EmbedAsset( ASSET_PATH, asset.BuildCmfFile() ) );
	EXPECT_EQ( loader.GetReferenceBalance( ASSET_PATH ), 0 );
	EXPECT_FALSE( network.HasPendingAssets() );

	const auto* embeddedData = network.GetAssetData( ASSET_PATH );
	ASSERT_NE( embeddedData, nullptr );
	EXPECT_NE( embeddedData, &asset.data );
	loader.Complete();
	EXPECT_EQ( network.GetAssetData( ASSET_PATH ), embeddedData ) << "the stale loader callback is ignored";
}

TEST_F( NetworkFixture, InvalidEmbeddedAssetIsRejected )
{
	cmf::AnimationNetworkBuilder network( loader );
	auto file = asset.BuildCmfFile();
	constexpr uint8_t corruption = 0xff;
	file.back() ^= corruption;
	EXPECT_FALSE( network.EmbedAsset( "broken.cmf", file ) );
	EXPECT_FALSE( network.EmbedAsset( "", asset.BuildCmfFile() ) );
	EXPECT_TRUE( network.GetEmbeddedAssetPaths().empty() );
}

}
