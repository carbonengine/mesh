// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"

#include "cmf/animation.h"
#include "cmf/animationnetworkinstance.h"
#include "networktestutils.h"

#include <cmath>
#include <set>

namespace networktest
{
namespace
{

constexpr float EPSILON = 0.0001f;
constexpr float QUARTER = 0.25f;
constexpr float HALF = 0.5f;
constexpr uint32_t MOVE_INDEX = 0;
constexpr uint32_t LIFT_INDEX = 1;
constexpr float DOUBLE_SPEED = 2.f;
constexpr float THREE_QUARTERS = 1.5f;
constexpr float OUT_OF_RANGE = 2.f;
constexpr uint32_t SEED_COUNT = 10;
constexpr int REPETITIONS = 20;

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
		EXPECT_TRUE( NearlyEqual( actual[i], expected.boneTransforms[i] ) ) << "bone " << i;
	}
}

void ExpectOk( const cmf::ValidationResult& result )
{
	EXPECT_TRUE( result ) << result.error;
}

/** Builds a network with the builder, saves it and loads it as a runtime network. */
struct InstanceFixture : public ::testing::Test
{
	TestAsset asset;
	FakeLoader loader;
	cmf::AnimationNetworkBuilder builder{ loader };
	cmf::AnimationGraphBuilder& graph = builder.GetRootGraph();
	cmf::AnimationNodeId output;
	cmf::AnimationNetwork network{ loader };

	InstanceFixture()
	{
		loader.AddAsset( ASSET_PATH, &asset.data );
		output = SetUpNetwork( builder );
	}

	cmf::AnimationNodeId AddSample( const char* animation )
	{
		const auto node = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
		ExpectOk( graph.SetAnimation( node, ASSET_PATH, animation ) );
		return node;
	}

	uint32_t AddParameter( const char* name, float defaultValue, float minValue = -1000.f, float maxValue = 1000.f )
	{
		cmf::AnimationParameter parameter;
		parameter.name = name;
		parameter.defaultValue = defaultValue;
		parameter.minValue = minValue;
		parameter.maxValue = maxValue;
		return builder.AddParameter( parameter );
	}

	cmf::AnimationNodeId AddParameterNode( uint32_t parameter )
	{
		const auto node = graph.AddNode( cmf::AnimationNodeType::Parameter );
		ExpectOk( graph.SetParameter( node, parameter ) );
		return node;
	}

	void Connect( cmf::AnimationNodeId from, cmf::AnimationNodeId to, uint32_t socket )
	{
		ExpectOk( graph.Connect( { from, 0 }, { to, socket } ) );
	}

	void LoadNetwork( cmf::AnimationNodeId result, bool expectReady = true )
	{
		Connect( result, output, 0 );
		const auto errors = builder.GetErrors();
		EXPECT_TRUE( errors.empty() ) << ( errors.empty() ? "" : errors[0] );
		ExpectOk( network.Load( builder.Save() ) );
		EXPECT_EQ( network.IsReady(), expectReady );
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

	// Returns which of the two test animations an output pose comes from: Move animates Child, Lift animates Leaf
	static uint32_t GetAnimation( const cmf::AnimationNetworkInstance& instance )
	{
		return instance.GetLocalTransforms()[CHILD_BONE].position.x != 0.f ? MOVE_INDEX : LIFT_INDEX;
	}
};

}

TEST_F( InstanceFixture, InstanceOfNetworkThatIsNotReadyIsInvalid )
{
	cmf::AnimationNetworkInstance instance( network );
	EXPECT_FALSE( instance.IsValid() );
	EXPECT_FALSE( instance.Evaluate() );
	EXPECT_EQ( instance.GetLocalTransforms(), nullptr );
	EXPECT_EQ( instance.GetBoneCount(), 0u );
	instance.Update( 1.f );
}

TEST_F( InstanceFixture, RestPoseNetworkOutputsRestPose )
{
	LoadNetwork( graph.AddNode( cmf::AnimationNodeType::RestPose ) );
	cmf::AnimationNetworkInstance instance( network );
	ASSERT_TRUE( instance.IsValid() );
	ASSERT_TRUE( instance.Evaluate() );
	ExpectPose( instance, RestPose() );

	cmf::SkeletonPose pose;
	instance.GetPose( pose );
	EXPECT_EQ( pose.skeleton, network.GetSkeleton() );
	EXPECT_EQ( pose.boneTransforms.size(), BONE_COUNT );
}

TEST_F( InstanceFixture, MissingAnimationsPlayTheRestPose )
{
	// A missing animation, a missing file and a sample without an animation are blended together
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto inner = graph.AddNode( cmf::AnimationNodeType::Blend );
	Connect( AddSample( "Missing" ), inner, 0 );
	const auto missingFile = graph.AddNode( cmf::AnimationNodeType::AnimationSample );
	ExpectOk( graph.SetAnimation( missingFile, "missing.cmf", MOVE_ANIMATION ) );
	Connect( missingFile, inner, 1 );
	Connect( inner, blend, 0 );
	Connect( graph.AddNode( cmf::AnimationNodeType::AnimationSample ), blend, 1 );
	ExpectOk( graph.SetInputConstant( { blend, 2 }, HALF ) );
	LoadNetwork( blend );
	EXPECT_EQ( network.GetWarnings().size(), 3u );

	cmf::AnimationNetworkInstance instance( network );
	ASSERT_TRUE( instance.IsValid() );
	instance.Update( 1.f );
	ASSERT_TRUE( instance.Evaluate() );
	ExpectPose( instance, RestPose() );
}

TEST_F( InstanceFixture, MissingBoneMaskWeightsAllBones )
{
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto mask = graph.AddNode( cmf::AnimationNodeType::NamedBoneWeights );
	ExpectOk( graph.SetBoneMask( mask, "Lower" ) );
	const auto weights = graph.AddNode( cmf::AnimationNodeType::BoneWeights );
	ExpectOk( graph.SetBoneWeights( weights, { { "Tail", 0.f } }, 1.f ) );
	const auto additive = graph.AddNode( cmf::AnimationNodeType::Blend );
	Connect( graph.AddNode( cmf::AnimationNodeType::RestPose ), blend, 0 );
	Connect( AddSample( MOVE_ANIMATION ), blend, 1 );
	Connect( mask, blend, 3 );
	ExpectOk( graph.SetInputConstant( { blend, 2 }, 1.f ) );
	Connect( blend, additive, 0 );
	Connect( AddSample( MOVE_ANIMATION ), additive, 1 );
	Connect( weights, additive, 3 );
	ExpectOk( graph.SetInputConstant( { additive, 2 }, 1.f ) );
	LoadNetwork( additive );
	EXPECT_EQ( network.GetWarnings().size(), 2u );

	cmf::AnimationNetworkInstance instance( network );
	instance.Update( 1.f );
	instance.Evaluate();
	ExpectPose( instance, Sample( MOVE_INDEX, 1.f ) );
}

TEST_F( InstanceFixture, SamplePlayheadAdvancesWithSpeed )
{
	const auto speed = AddParameter( "speed", 1.f );
	const auto sample = AddSample( MOVE_ANIMATION );
	Connect( AddParameterNode( speed ), sample, 2 );
	LoadNetwork( sample );

	cmf::AnimationNetworkInstance instance( network );
	instance.Update( HALF );
	ASSERT_TRUE( instance.Evaluate() );
	ExpectPose( instance, Sample( MOVE_INDEX, HALF ) );

	// Changing the speed continues from the current playhead position
	instance.SetParameter( instance.FindParameter( "speed" ), DOUBLE_SPEED );
	instance.Update( QUARTER );
	instance.Evaluate();
	ExpectPose( instance, Sample( MOVE_INDEX, 1.f ) );
	EXPECT_FLOAT_EQ( instance.GetTime(), HALF + QUARTER );
}

TEST_F( InstanceFixture, LoopingSampleWrapsAround )
{
	LoadNetwork( AddSample( MOVE_ANIMATION ) );
	cmf::AnimationNetworkInstance instance( network );
	instance.Update( ANIMATION_DURATION + HALF );
	instance.Evaluate();
	ExpectPose( instance, Sample( MOVE_INDEX, HALF ) );
}

TEST_F( InstanceFixture, NonLoopingSampleStopsAtTheEnd )
{
	const auto sample = AddSample( MOVE_ANIMATION );
	ExpectOk( graph.SetInputConstant( { sample, 1 }, 0.f ) );
	LoadNetwork( sample );
	cmf::AnimationNetworkInstance instance( network );
	instance.Update( ANIMATION_DURATION + HALF );
	instance.Evaluate();
	const float end = ANIMATION_DURATION;
	ExpectPose( instance, Sample( MOVE_INDEX, end ) );
}

TEST_F( InstanceFixture, ConnectedTimeInputOverridesThePlayhead )
{
	const auto time = AddParameter( "time", 0.f );
	const auto sample = AddSample( MOVE_ANIMATION );
	Connect( AddParameterNode( time ), sample, 0 );
	LoadNetwork( sample );

	cmf::AnimationNetworkInstance instance( network );
	instance.SetParameter( instance.FindParameter( "time" ), THREE_QUARTERS );
	instance.Update( HALF );
	instance.Evaluate();
	ExpectPose( instance, Sample( MOVE_INDEX, THREE_QUARTERS ) );
}

TEST_F( InstanceFixture, BlendMixesPosesAndKeepsUnusedPlayheadsRunning )
{
	const auto alpha = AddParameter( "alpha", 0.f, 0.f, 1.f );
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	Connect( AddSample( MOVE_ANIMATION ), blend, 0 );
	Connect( AddSample( LIFT_ANIMATION ), blend, 1 );
	Connect( AddParameterNode( alpha ), blend, 2 );
	LoadNetwork( blend );

	cmf::AnimationNetworkInstance instance( network );
	const auto alphaIndex = instance.FindParameter( "alpha" );
	instance.Update( 1.f );
	instance.Evaluate();
	ExpectPose( instance, Sample( MOVE_INDEX, 1.f ) );

	instance.SetParameter( alphaIndex, QUARTER );
	instance.Evaluate();
	ExpectPose( instance, Blend( Sample( MOVE_INDEX, 1.f ), Sample( LIFT_INDEX, 1.f ), QUARTER ) );

	// The Lift playhead advanced while its input wasn't used
	instance.SetParameter( alphaIndex, 1.f );
	instance.Update( HALF );
	instance.Evaluate();
	ExpectPose( instance, Sample( LIFT_INDEX, 1.f + HALF ) );

	instance.SetParameter( alphaIndex, OUT_OF_RANGE );
	EXPECT_EQ( instance.GetParameter( alphaIndex ), 1.f ) << "parameters are clamped to their range";
}

TEST_F( InstanceFixture, BlendUsesBoneWeights )
{
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto mask = graph.AddNode( cmf::AnimationNodeType::NamedBoneWeights );
	ExpectOk( graph.SetBoneMask( mask, MASK_NAME ) );
	Connect( graph.AddNode( cmf::AnimationNodeType::RestPose ), blend, 0 );
	Connect( AddSample( MOVE_ANIMATION ), blend, 1 );
	Connect( mask, blend, 3 );
	ExpectOk( graph.SetInputConstant( { blend, 2 }, 1.f ) );
	LoadNetwork( blend );

	cmf::AnimationNetworkInstance instance( network );
	instance.Update( 1.f );
	instance.Evaluate();
	cmf::SkeletonPose expected;
	cmf::BlendPoses( expected, RestPose(), Sample( MOVE_INDEX, 1.f ), cmf::ExtractBoneWeights( asset.data.skeletons[0], MASK_NAME ), 1.f );
	ExpectPose( instance, expected );
}

TEST_F( InstanceFixture, AdditiveBlendAddsTheDifferenceToTheBase )
{
	const auto additive = graph.AddNode( cmf::AnimationNodeType::AdditiveBlend );
	Connect( AddSample( MOVE_ANIMATION ), additive, 0 );
	Connect( graph.AddNode( cmf::AnimationNodeType::RestPose ), additive, 1 );
	Connect( AddSample( LIFT_ANIMATION ), additive, 2 );
	ExpectOk( graph.SetInputConstant( { additive, 3 }, HALF ) );
	LoadNetwork( additive );

	cmf::AnimationNetworkInstance instance( network );
	instance.Update( 1.f );
	instance.Evaluate();
	cmf::SkeletonPose expected;
	cmf::BlendAdditivePose( expected, Sample( MOVE_INDEX, 1.f ), RestPose(), Sample( LIFT_INDEX, 1.f ), HALF );
	ExpectPose( instance, expected );
}

TEST_F( InstanceFixture, SwitchSelectsAnInputBySelectorValue )
{
	const auto selector = AddParameter( "selector", 0.f );
	const auto switchNode = graph.AddNode( cmf::AnimationNodeType::Switch );
	ExpectOk( graph.SetPoseInputCount( switchNode, 3 ) );
	Connect( AddParameterNode( selector ), switchNode, 0 );
	Connect( AddSample( MOVE_ANIMATION ), switchNode, 1 );
	Connect( AddSample( LIFT_ANIMATION ), switchNode, 2 );
	Connect( graph.AddNode( cmf::AnimationNodeType::RestPose ), switchNode, 3 );
	LoadNetwork( switchNode );

	cmf::AnimationNetworkInstance instance( network );
	const auto selectorIndex = instance.FindParameter( "selector" );
	instance.Update( 1.f );
	const std::vector<std::pair<float, cmf::SkeletonPose>> cases = {
		{ 0.f, Sample( MOVE_INDEX, 1.f ) },
		{ 1.7f, Sample( LIFT_INDEX, 1.f ) },
		{ 2.f, RestPose() },
		{ 10.f, RestPose() },
		{ -3.f, Sample( MOVE_INDEX, 1.f ) },
	};
	for( const auto& [value, expected] : cases )
	{
		instance.SetParameter( selectorIndex, value );
		instance.Evaluate();
		ExpectPose( instance, expected );
	}
}

TEST_F( InstanceFixture, RandomSwitchFollowsWeights )
{
	const auto random = graph.AddNode( cmf::AnimationNodeType::RandomSwitch );
	Connect( AddSample( MOVE_ANIMATION ), random, 0 );
	Connect( AddSample( LIFT_ANIMATION ), random, 1 );
	ExpectOk( graph.SetRandomWeights( random, { 0.f, 1.f } ) );
	LoadNetwork( random );

	for( uint32_t seed = 1; seed < SEED_COUNT; ++seed )
	{
		cmf::AnimationNetworkInstance instance( network, seed );
		instance.Update( HALF );
		instance.Evaluate();
		EXPECT_EQ( GetAnimation( instance ), LIFT_INDEX );
	}
}

TEST_F( InstanceFixture, RandomSwitchRerollsWhenTheSelectedAnimationLoops )
{
	const auto random = graph.AddNode( cmf::AnimationNodeType::RandomSwitch );
	Connect( AddSample( MOVE_ANIMATION ), random, 0 );
	Connect( AddSample( LIFT_ANIMATION ), random, 1 );
	LoadNetwork( random );

	cmf::AnimationNetworkInstance instance( network, 1 );
	std::set<uint32_t> selected;
	bool changedWithinCycle = false;
	for( int cycle = 0; cycle < REPETITIONS; ++cycle )
	{
		// Within a cycle the selection stays the same
		instance.Update( HALF );
		instance.Evaluate();
		const auto first = GetAnimation( instance );
		instance.Update( HALF );
		instance.Evaluate();
		changedWithinCycle = changedWithinCycle || GetAnimation( instance ) != first;
		selected.insert( first );
		instance.Update( ANIMATION_DURATION - 1.f );
		instance.Evaluate();
	}
	EXPECT_FALSE( changedWithinCycle );
	EXPECT_EQ( selected.size(), 2u );
}

TEST_F( InstanceFixture, RandomSwitchRerollsWhenItBecomesActive )
{
	const auto active = AddParameter( "active", 1.f );
	const auto switchNode = graph.AddNode( cmf::AnimationNodeType::Switch );
	const auto random = graph.AddNode( cmf::AnimationNodeType::RandomSwitch );
	ExpectOk( graph.SetRerollOnActivate( random, true ) );
	Connect( AddSample( MOVE_ANIMATION ), random, 0 );
	Connect( AddSample( LIFT_ANIMATION ), random, 1 );
	Connect( AddParameterNode( active ), switchNode, 0 );
	Connect( graph.AddNode( cmf::AnimationNodeType::RestPose ), switchNode, 1 );
	Connect( random, switchNode, 2 );
	LoadNetwork( switchNode );

	cmf::AnimationNetworkInstance instance( network, 1 );
	const auto activeIndex = instance.FindParameter( "active" );
	instance.Update( HALF );
	std::set<uint32_t> selected;
	for( int i = 0; i < REPETITIONS; ++i )
	{
		instance.SetParameter( activeIndex, 1.f );
		instance.Evaluate();
		selected.insert( GetAnimation( instance ) );
		instance.SetParameter( activeIndex, 0.f );
		instance.Evaluate();
		ExpectPose( instance, RestPose() );
	}
	EXPECT_EQ( selected.size(), 2u );
}

TEST_F( InstanceFixture, ExpressionsComputeFloatInputs )
{
	const auto weight = AddParameter( "weight", 0.f );
	const auto expression = graph.AddNode( cmf::AnimationNodeType::Expression );
	ExpectOk( graph.SetExpression( expression, "weight / 4", { "weight" } ) );
	Connect( AddParameterNode( weight ), expression, 0 );
	const auto blend = graph.AddNode( cmf::AnimationNodeType::Blend );
	Connect( graph.AddNode( cmf::AnimationNodeType::RestPose ), blend, 0 );
	Connect( AddSample( MOVE_ANIMATION ), blend, 1 );
	Connect( expression, blend, 2 );
	LoadNetwork( blend );

	cmf::AnimationNetworkInstance instance( network );
	instance.SetParameter( instance.FindParameter( "weight" ), 1.f );
	instance.Update( 1.f );
	instance.Evaluate();
	ExpectPose( instance, Blend( RestPose(), Sample( MOVE_INDEX, 1.f ), QUARTER ) );
}

TEST_F( InstanceFixture, SubgraphBehavesLikeTheEquivalentFlatGraph )
{
	// Subgraph blending its pose input with Lift by its alpha input
	const auto subgraphNode = graph.AddNode( cmf::AnimationNodeType::Subgraph );
	auto& sub = *graph.GetSubgraph( subgraphNode );
	const auto poseIn = sub.AddNode( cmf::AnimationNodeType::SubgraphInput );
	const auto alphaIn = sub.AddNode( cmf::AnimationNodeType::SubgraphInput );
	const auto subOutput = sub.AddNode( cmf::AnimationNodeType::Output );
	ExpectOk( sub.SetInterfaceSocket( poseIn, "pose", cmf::AnimationSocketType::Pose ) );
	ExpectOk( sub.SetInterfaceSocket( alphaIn, "alpha", cmf::AnimationSocketType::Float ) );
	ExpectOk( sub.SetInterfaceSocket( subOutput, "result", cmf::AnimationSocketType::Pose ) );
	const auto blend = sub.AddNode( cmf::AnimationNodeType::Blend );
	const auto lift = sub.AddNode( cmf::AnimationNodeType::AnimationSample );
	ExpectOk( sub.SetAnimation( lift, ASSET_PATH, LIFT_ANIMATION ) );
	ExpectOk( sub.Connect( { poseIn, 0 }, { blend, 0 } ) );
	ExpectOk( sub.Connect( { lift, 0 }, { blend, 1 } ) );
	ExpectOk( sub.Connect( { alphaIn, 0 }, { blend, 2 } ) );
	ExpectOk( sub.Connect( { blend, 0 }, { subOutput, 0 } ) );

	Connect( AddSample( MOVE_ANIMATION ), subgraphNode, 0 );
	ExpectOk( graph.SetInputConstant( { subgraphNode, 1 }, QUARTER ) );
	LoadNetwork( subgraphNode );

	cmf::AnimationNetworkInstance instance( network );
	instance.Update( 1.f );
	instance.Evaluate();
	ExpectPose( instance, Blend( Sample( MOVE_INDEX, 1.f ), Sample( LIFT_INDEX, 1.f ), QUARTER ) );
}

TEST_F( InstanceFixture, ChainedOperationsReusePoseSlotsCorrectly )
{
	// ( ( Move x Lift ) x Rest ) + additive Lift, with the Move sample also used directly by the additive blend
	const auto move = AddSample( MOVE_ANIMATION );
	const auto lift = AddSample( LIFT_ANIMATION );
	const auto rest = graph.AddNode( cmf::AnimationNodeType::RestPose );
	const auto first = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto second = graph.AddNode( cmf::AnimationNodeType::Blend );
	const auto additive = graph.AddNode( cmf::AnimationNodeType::AdditiveBlend );
	Connect( move, first, 0 );
	Connect( lift, first, 1 );
	ExpectOk( graph.SetInputConstant( { first, 2 }, QUARTER ) );
	Connect( first, second, 0 );
	Connect( rest, second, 1 );
	ExpectOk( graph.SetInputConstant( { second, 2 }, HALF ) );
	Connect( second, additive, 0 );
	Connect( move, additive, 1 );
	Connect( lift, additive, 2 );
	ExpectOk( graph.SetInputConstant( { additive, 3 }, HALF ) );
	LoadNetwork( additive );

	cmf::AnimationNetworkInstance instance( network );
	instance.Update( 1.f );
	instance.Evaluate();
	const auto moved = Sample( MOVE_INDEX, 1.f );
	const auto lifted = Sample( LIFT_INDEX, 1.f );
	cmf::SkeletonPose expected;
	cmf::BlendAdditivePose( expected, Blend( Blend( moved, lifted, QUARTER ), RestPose(), HALF ), moved, lifted, HALF );
	ExpectPose( instance, expected );
}

TEST_F( InstanceFixture, InstancesAreIndependent )
{
	const auto speed = AddParameter( "speed", 1.f );
	const auto sample = AddSample( MOVE_ANIMATION );
	Connect( AddParameterNode( speed ), sample, 2 );
	LoadNetwork( sample );

	cmf::AnimationNetworkInstance slow( network );
	cmf::AnimationNetworkInstance fast( network );
	fast.SetParameter( fast.FindParameter( "speed" ), DOUBLE_SPEED );
	slow.Update( HALF );
	fast.Update( HALF );
	slow.Evaluate();
	fast.Evaluate();
	ExpectPose( slow, Sample( MOVE_INDEX, HALF ) );
	ExpectPose( fast, Sample( MOVE_INDEX, 1.f ) );

	// Moving keeps the state
	auto moved = std::move( slow );
	moved.Update( HALF );
	moved.Evaluate();
	ExpectPose( moved, Sample( MOVE_INDEX, 1.f ) );
}

TEST_F( InstanceFixture, ReloadingTheNetworkInvalidatesInstances )
{
	LoadNetwork( graph.AddNode( cmf::AnimationNodeType::RestPose ) );
	cmf::AnimationNetworkInstance instance( network );
	ASSERT_TRUE( instance.IsValid() );
	ExpectOk( network.Load( builder.Save() ) );
	EXPECT_FALSE( instance.IsValid() );
	EXPECT_FALSE( instance.Evaluate() );
}

TEST_F( InstanceFixture, NetworkBecomesReadyWhenDeferredAssetsArrive )
{
	const auto sample = AddSample( MOVE_ANIMATION );
	loader.SetDeferred( true );
	LoadNetwork( sample, false );
	EXPECT_FALSE( cmf::AnimationNetworkInstance( network ).IsValid() );
	loader.Complete();
	EXPECT_TRUE( network.IsReady() );
	cmf::AnimationNetworkInstance instance( network );
	instance.Update( 1.f );
	ASSERT_TRUE( instance.Evaluate() );
	ExpectPose( instance, Sample( MOVE_INDEX, 1.f ) );
}

}
