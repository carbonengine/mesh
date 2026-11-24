// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/animationnetworkbuilder.h"
#include "cmf/memallocator.h"
#include "cmf/writer.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace networktest
{

constexpr uint32_t ROOT_BONE = 0;
constexpr uint32_t CHILD_BONE = 1;
constexpr uint32_t LEAF_BONE = 2;
constexpr uint32_t BONE_COUNT = 3;
constexpr uint32_t NO_PARENT = 0xffffffff;

constexpr const char* SKELETON_NAME = "Skeleton";
constexpr const char* MASK_NAME = "Upper";
constexpr const char* MOVE_ANIMATION = "Move";
constexpr const char* LIFT_ANIMATION = "Lift";

/** Weight of the Child bone in the bone mask; the Leaf bone has weight 1. */
constexpr float CHILD_MASK_WEIGHT = 0.5f;

/** Animation length of both test animations. */
constexpr float ANIMATION_DURATION = 2.f;

/** Position of the Child bone in the Move animation at the given local time: (time, 0, 0). */
inline Vector3 MovePosition( float time )
{
	return { time, 0.f, 0.f };
}

/** Position of the Leaf bone in the Lift animation at the given local time: (0, time * 10, 0). */
inline Vector3 LiftPosition( float time )
{
	return { 0.f, time * 10.f, 0.f }; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers): documented in the comment above
}

/** In-memory CMF data with a three bone skeleton (Root -> Child -> Leaf), a bone mask and two animations. */
struct TestAsset
{
	cmf::MemoryAllocator allocator;
	cmf::Data data;

	TestAsset()
	{
		cmf::Skeleton skeleton;
		skeleton.name = allocator.AllocateString( SKELETON_NAME );
		const std::vector<std::pair<const char*, uint32_t>> bones = { { "Root", NO_PARENT }, { "Child", ROOT_BONE }, { "Leaf", CHILD_BONE } };
		for( const auto& [name, parent] : bones )
		{
			cmf::Modify( skeleton.bones, allocator ).push_back( allocator.AllocateString( name ) );
			cmf::Modify( skeleton.parents, allocator ).push_back( parent );
			cmf::Modify( skeleton.restTransforms, allocator ).push_back( cmf::Transform{} );
			cmf::Modify( skeleton.invBindTransforms, allocator ).push_back( Matrix{} );
		}
		cmf::BoneMask mask;
		mask.name = allocator.AllocateString( MASK_NAME );
		cmf::Modify( mask.weights, allocator ) = std::vector<cmf::BoneWeight>{ { CHILD_BONE, CHILD_MASK_WEIGHT }, { LEAF_BONE, 1.f } };
		cmf::Modify( skeleton.boneMasks, allocator ).push_back( mask );
		cmf::Modify( data.skeletons, allocator ).push_back( skeleton );

		cmf::Modify( data.animations, allocator ).push_back( MakeAnimation( MOVE_ANIMATION, "Child", MovePosition( 0.f ), MovePosition( ANIMATION_DURATION ) ) );
		cmf::Modify( data.animations, allocator ).push_back( MakeAnimation( LIFT_ANIMATION, "Leaf", LiftPosition( 0.f ), LiftPosition( ANIMATION_DURATION ) ) );
	}

	TestAsset( const TestAsset& ) = delete;
	TestAsset( TestAsset&& ) = delete;
	TestAsset& operator=( const TestAsset& ) = delete;
	TestAsset& operator=( TestAsset&& ) = delete;
	~TestAsset() = default;

	/** Returns the asset serialized as a CMF file. */
	[[nodiscard]] std::vector<uint8_t> BuildCmfFile() const
	{
		cmf::MemoryAllocator bufferAllocator;
		const cmf::BufferManager buffers( bufferAllocator );
		return cmf::BuildFile( data, buffers );
	}

private:
	cmf::Animation MakeAnimation( const char* name, const char* bone, const Vector3& start, const Vector3& end )
	{
		const std::vector<float> knots = { 0.f, ANIMATION_DURATION };
		const std::vector<Vector3> values = { start, end };

		cmf::AnimationCurve curve;
		curve.valueDimension = 3;
		curve.interpolation = cmf::Interpolation::Linear;
		curve.knotType = cmf::ElementType::Float32;
		curve.valueType = cmf::ElementType::Float32;
		curve.knotCount = static_cast<uint32_t>( knots.size() );
		curve.knots = allocator.AllocateSpan<uint8_t>( knots.size() * sizeof( float ) );
		std::memcpy( curve.knots.data(), knots.data(), knots.size() * sizeof( float ) );
		curve.values = allocator.AllocateSpan<uint8_t>( values.size() * sizeof( Vector3 ) );
		std::memcpy( curve.values.data(), values.data(), values.size() * sizeof( Vector3 ) );

		cmf::AnimationChannel channel;
		channel.target = allocator.AllocateString( bone );
		channel.targetType = cmf::AnimationChannelTargetType::BonePosition;
		channel.curveIndex = 0;

		cmf::Animation animation;
		animation.name = allocator.AllocateString( name );
		animation.duration = ANIMATION_DURATION;
		cmf::Modify( animation.channels, allocator ).push_back( channel );
		cmf::Modify( animation.curves, allocator ).push_back( curve );
		return animation;
	}
};

/** Asset loader that serves registered assets, either immediately or when Complete is called. */
class FakeLoader : public cmf::AnimationAssetLoader
{
public:
	/** When true, callbacks are deferred until Complete is called. */
	void SetDeferred( bool deferred )
	{
		m_deferred = deferred;
	}

	/** Paths passed to RequestLoad, in call order. */
	[[nodiscard]] const std::vector<std::string>& GetRequests() const
	{
		return m_requests;
	}

	/** Paths passed to Release, in call order. */
	[[nodiscard]] const std::vector<std::string>& GetReleases() const
	{
		return m_releases;
	}

	void AddAsset( const std::string& path, const cmf::Data* data )
	{
		m_assets[path] = data;
	}

	void RequestLoad( std::string_view path, Callback onLoaded ) override
	{
		m_requests.emplace_back( path );
		if( m_deferred )
		{
			m_pending.emplace_back( std::string( path ), std::move( onLoaded ) );
		}
		else
		{
			onLoaded( Find( path ) );
		}
	}

	void Release( std::string_view path ) override
	{
		m_releases.emplace_back( path );
	}

	/** Completes all deferred requests. */
	void Complete()
	{
		auto pending = std::move( m_pending );
		m_pending.clear();
		for( auto& [path, callback] : pending )
		{
			callback( Find( path ) );
		}
	}

	/** Returns the number of times a path was requested minus the number of times it was released. */
	[[nodiscard]] int64_t GetReferenceBalance( const std::string& path ) const
	{
		return std::count( m_requests.begin(), m_requests.end(), path ) - std::count( m_releases.begin(), m_releases.end(), path );
	}

private:
	[[nodiscard]] const cmf::Data* Find( std::string_view path ) const
	{
		auto it = m_assets.find( std::string( path ) );
		return it == m_assets.end() ? nullptr : it->second;
	}

	bool m_deferred = false;
	std::vector<std::string> m_requests;
	std::vector<std::string> m_releases;
	std::map<std::string, const cmf::Data*> m_assets;
	std::vector<std::pair<std::string, Callback>> m_pending;
};

constexpr const char* ASSET_PATH = "assets/character.cmf";

/** Sets up a network whose skeleton comes from ASSET_PATH and whose root graph has an Output node. */
inline cmf::AnimationNodeId SetUpNetwork( cmf::AnimationNetworkBuilder& network )
{
	network.SetSkeleton( ASSET_PATH, SKELETON_NAME );
	return network.GetRootGraph().AddNode( cmf::AnimationNodeType::Output );
}

}
