// Copyright © 2026 CCP ehf.

#include "cmf/animation.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include "cmf/transforms.h"

#include <mutex>
#include <array>
#include <algorithm>
#include <cmath>

namespace
{

bool IsBoneTarget( cmf::AnimationChannelTargetType targetType )
{
	return targetType == cmf::AnimationChannelTargetType::BonePosition || targetType == cmf::AnimationChannelTargetType::BoneRotation || targetType == cmf::AnimationChannelTargetType::BoneScale;
}

struct Interval
{
	uint32_t knotIndex0 = 0;
	uint32_t knotIndex1 = 0;
	float time = 0;
};

using Knots = cmf::ConstBufferElementStream<float>;

Interval FindKnotInterval( const Knots& knots, float time )
{
	Interval interval = {};
	if( knots.size() == 0 || time < knots[0] )
	{
		return interval;
	}
	auto knot0 = knots[interval.knotIndex0];
	if( time < knot0 )
	{
		return interval;
	}
	interval.knotIndex1 = 1;
	interval.time = 1;
	while( interval.knotIndex1 < knots.size() )
	{
		const auto knot1 = knots[interval.knotIndex1];
		if( time < knot1 )
		{
			interval.time = ( time - knot0 ) / ( knot1 - knot0 );
			return interval;
		}
		++interval.knotIndex0;
		++interval.knotIndex1;
		knot0 = knot1;
	}
	interval.knotIndex1 = interval.knotIndex0;
	return interval;
}

template <typename T>
Interval FindKnotInterval( const Knots& knots, float time, cmf::CurveEvaluationCache<T>& cache )
{
	Interval interval = {};
	if( knots.size() == 0 )
	{
		return interval;
	}
	if( cache.knotIndex < knots.size() )
	{
		if( time >= cache.knot0 )
		{
			interval.knotIndex0 = cache.knotIndex;
			if( cache.knotIndex + 1 >= knots.size() )
			{
				interval.knotIndex1 = cache.knotIndex;
				interval.time = 1;
				return interval;
			}
			if( time < cache.knot1 )
			{
				interval.knotIndex1 = cache.knotIndex + 1;
				interval.time = ( time - cache.knot0 ) / ( cache.knot1 - cache.knot0 );
				return interval;
			}
		}
	}
	auto knot0 = cache.knotIndex == interval.knotIndex0 ? cache.knot0 : knots[interval.knotIndex0];
	if( time < knot0 )
	{
		cache.knotIndex = interval.knotIndex0;
		cache.knot0 = knot0;
		cache.knot1 = cache.knot0;
		return interval;
	}
	cache.knotIndex = interval.knotIndex0;
	cache.knot0 = knot0;
	interval.knotIndex1 = interval.knotIndex0 + 1;
	interval.time = 1;
	while( interval.knotIndex1 < knots.size() )
	{
		const auto knot1 = knots[interval.knotIndex1];
		cache.knot1 = knot1;
		if( time < knot1 )
		{
			interval.time = ( time - knot0 ) / ( knot1 - knot0 );
			cache.knotIndex = interval.knotIndex0;
			return interval;
		}
		++interval.knotIndex0;
		++interval.knotIndex1;
		knot0 = knot1;
		cache.knot0 = knot0;
	}
	interval.knotIndex1 = interval.knotIndex0;
	cache.knot1 = cache.knot0;
	cache.knotIndex = interval.knotIndex0;
	return interval;
}

Knots GetKnotStream( const cmf::AnimationCurve& curve )
{
	cmf::VertexElement element = {};
	element.type = curve.knotType;
	element.elementCount = 1;
	const auto stride = cmf::GetVertexElementSize( element );
	return { element, curve.knots.data(), uint32_t( curve.knots.size() / stride ), stride };
}

template <typename T>
cmf::ConstBufferElementStream<T> GetValueStream( const cmf::AnimationCurve& curve )
{
	cmf::VertexElement element = {};
	element.type = curve.valueType;
	element.elementCount = curve.valueDimension;
	const auto stride = cmf::GetVertexElementSize( element );
	return { element, curve.values.data(), uint32_t( curve.values.size() / stride ), stride };
}

template <typename T>
T SampleCurve( const cmf::AnimationCurve& curve, float time )
{
	const auto knotStream = GetKnotStream( curve );
	const auto interval = FindKnotInterval( knotStream, time );
	const auto valueStream = GetValueStream<T>( curve );

	switch( curve.interpolation )
	{
	case cmf::Interpolation::Step:
		return valueStream[interval.knotIndex0];
	case cmf::Interpolation::Linear: {
		const auto v0 = valueStream[interval.knotIndex0];
		const auto v1 = valueStream[interval.knotIndex1];
		return v0 + ( v1 - v0 ) * interval.time;
	}
	default:
		return {};
	}
}

template <typename T>
T SampleCurve( const Knots& knots, const cmf::ConstBufferElementStream<T>& values, cmf::Interpolation interpolation, float time )
{
	const auto interval = FindKnotInterval( knots, time );
	switch( interpolation )
	{
	case cmf::Interpolation::Step:
		return values[interval.knotIndex0];
	case cmf::Interpolation::Linear: {
		const auto v0 = values[interval.knotIndex0];
		const auto v1 = values[interval.knotIndex1];
		return v0 + ( v1 - v0 ) * interval.time;
	}
	default:
		return {};
	}
}

template <typename T>
T SampleCurve( const Knots& knots, const cmf::ConstBufferElementStream<T>& values, cmf::Interpolation interpolation, float time, cmf::CurveEvaluationCache<T>& cache )
{
	auto prevKnot = cache.knotIndex;
	const auto interval = FindKnotInterval( knots, time, cache );
	switch( interpolation )
	{
	case cmf::Interpolation::Step:
		if( prevKnot != cache.knotIndex )
		{
			cache.value0 = values[interval.knotIndex0];
		}
		return cache.value0;
	case cmf::Interpolation::Linear:
		if( prevKnot != cache.knotIndex )
		{
			cache.value0 = values[interval.knotIndex0];
			cache.value1 = values[interval.knotIndex1];
		}
		return cache.value0 + ( cache.value1 - cache.value0 ) * interval.time;
	default:
		return {};
	}
}

Quaternion SampleQuaternionCurve( const Knots& knots, const cmf::ConstBufferElementStream<Vector4>& values, cmf::Interpolation interpolation, float time, cmf::CurveEvaluationCache<Vector4>& cache )
{
	auto prevKnot = cache.knotIndex;
	const auto interval = FindKnotInterval( knots, time, cache );
	switch( interpolation )
	{
	case cmf::Interpolation::Step:
		if( prevKnot != cache.knotIndex )
		{
			cache.value0 = values[interval.knotIndex0];
		}
		return cache.value0;
	case cmf::Interpolation::Linear:
		if( prevKnot != cache.knotIndex )
		{
			cache.value0 = values[interval.knotIndex0];
			cache.value1 = values[interval.knotIndex1];
		}
		return Slerp( cache.value0, cache.value1, interval.time );
	default:
		return {};
	}
}

std::mutex& GetActiveAnimationPlayersMutex()
{
	static std::mutex mutex;
	return mutex;
}

std::vector<cmf::AnimationPlayer*>& GetActiveAnimationPlayers()
{
	static std::vector<cmf::AnimationPlayer*> activePlayers;
	return activePlayers;
}

cmf::BufferElementStream<float> GetScalarStream( cmf::ElementType type, cmf::Span<uint8_t>& data )
{
	cmf::VertexElement element = {};
	element.type = type;
	element.elementCount = 1;
	const auto stride = cmf::GetVertexElementSize( element );
	return { element, data.data(), uint32_t( data.size() / stride ), stride };
}

cmf::Span<uint8_t> ConvertScalarValues( const float* values, uint32_t valueCount, cmf::ElementType type, cmf::MemoryAllocator& allocator )
{
	auto result = allocator.AllocateSpan<uint8_t>( valueCount * cmf::GetElementTypeSize( type ) );
	auto knotStream = GetScalarStream( type, result );
	for( uint32_t i = 0; i < valueCount; ++i )
	{
		knotStream.set( i, values[i] );
	}
	return result;
}

bool ValuesWithinTolerance( const float* values, uint32_t valueCount, cmf::ElementType type, float minValue, float maxValue, float tolerance )
{
	cmf::DeclTypeConverter<float> converter( type, 1 );
	return std::all_of( values, values + valueCount, [&]( float v ) {
		if( v < minValue || v > maxValue )
		{
			return false;
		}
		std::array<uint8_t, 4> buffer;
		converter.set( buffer.data(), v );
		return std::abs( converter( buffer.data() ) - v ) <= tolerance;
	} );
}

std::pair<cmf::Span<uint8_t>, cmf::ElementType> OptimizeValues( const float* values, uint32_t valueCount, float tolerance, cmf::MemoryAllocator& allocator )
{
	if( ValuesWithinTolerance( values, valueCount, cmf::ElementType::UInt8Norm, 0.f, 1.f, tolerance ) )
	{
		const auto type = cmf::ElementType::UInt8Norm;
		return { ConvertScalarValues( values, valueCount, type, allocator ), type };
	}
	if( ValuesWithinTolerance( values, valueCount, cmf::ElementType::UInt16Norm, 0.f, 1.f, tolerance ) )
	{
		const auto type = cmf::ElementType::UInt16Norm;
		return { ConvertScalarValues( values, valueCount, type, allocator ), type };
	}
	if( ValuesWithinTolerance( values, valueCount, cmf::ElementType::Int8Norm, -1.f, 1.f, tolerance ) )
	{
		const auto type = cmf::ElementType::Int8Norm;
		return { ConvertScalarValues( values, valueCount, type, allocator ), type };
	}
	if( ValuesWithinTolerance( values, valueCount, cmf::ElementType::Int16Norm, -1.f, 1.f, tolerance ) )
	{
		const auto type = cmf::ElementType::Int16Norm;
		return { ConvertScalarValues( values, valueCount, type, allocator ), type };
	}
	if( ValuesWithinTolerance( values, valueCount, cmf::ElementType::Float16, -65504.f, 65504.f, tolerance ) )
	{
		const auto type = cmf::ElementType::Float16;
		return { ConvertScalarValues( values, valueCount, type, allocator ), type };
	}
	return { {}, cmf::ElementType::Float32 };
}

}

namespace cmf
{

float SampleScalarCurve( const cmf::AnimationCurve& curve, float time )
{
	return SampleCurve<float>( curve, time );
}

Vector2 SampleVector2Curve( const cmf::AnimationCurve& curve, float time )
{
	return SampleCurve<Vector2>( curve, time );
}

Vector3 SampleVector3Curve( const cmf::AnimationCurve& curve, float time )
{
	return SampleCurve<Vector3>( curve, time );
}

Vector4 SampleVector4Curve( const cmf::AnimationCurve& curve, float time )
{
	return SampleCurve<Vector4>( curve, time );
}

Quaternion SampleQuaternionCurve( const cmf::AnimationCurve& curve, float time )
{
	const auto knotStream = GetKnotStream( curve );
	const auto interval = FindKnotInterval( knotStream, time );
	const auto valueStream = GetValueStream<Vector4>( curve );
	switch( curve.interpolation )
	{
	case cmf::Interpolation::Step:
		return valueStream[interval.knotIndex0];
	case cmf::Interpolation::Linear:
		return Slerp( valueStream[interval.knotIndex0], valueStream[interval.knotIndex1], interval.time );
	default:
		return {};
	}
}

void RemoveDuplicateCurves( cmf::Animation& animation, cmf::MemoryAllocator& allocator )
{
	auto sameCurve = []( const cmf::AnimationCurve& a, const cmf::AnimationCurve& b ) {
		if( a.valueDimension != b.valueDimension || a.interpolation != b.interpolation || a.knotType != b.knotType || a.valueType != b.valueType || a.knotCount != b.knotCount )
		{
			return false;
		}
		return std::equal( a.knots.begin(), a.knots.end(), b.knots.begin() ) && std::equal( a.values.begin(), a.values.end(), b.values.begin() );
	};
	auto remapCurveIndex = [&]( uint32_t index, uint32_t newIndex ) {
		for( auto& channel : animation.channels )
		{
			if( channel.curveIndex == index )
			{
				channel.curveIndex = newIndex;
			}
		}
	};
	auto* end = animation.curves.end();
	for( auto* it = animation.curves.begin(); it != end; ++it )
	{
		auto& curve = *it;
		auto* found = std::find_if( animation.curves.begin(), &curve, [&]( const cmf::AnimationCurve& c ) { return sameCurve( c, curve ); } );
		if( found != &curve )
		{
			const auto curveIndex = uint32_t( it - animation.curves.begin() );
			auto foundIndex = uint32_t( found - animation.curves.begin() );
			remapCurveIndex( curveIndex, foundIndex );
			if( it + 1 != end )
			{
				// Swap the current curve with the last curve and update channel indices to point to the swapped curve if needed, then remove the last curve (which is a duplicate of the current curve)
				auto& lastCurve = *( end - 1 );
				const auto lastIndex = uint32_t( &lastCurve - animation.curves.begin() );
				std::swap( curve, lastCurve );
				remapCurveIndex( lastIndex, curveIndex );
			}
			--end;
			--it;
		}
	}
	if( end != animation.curves.end() )
	{
		auto newCurves = allocator.AllocateSpan<cmf::AnimationCurve>( size_t( end - animation.curves.begin() ) );
		std::copy( animation.curves.begin(), end, newCurves.begin() );
		animation.curves = newCurves;
	}
}

void OptimizeCurveFormat( cmf::AnimationCurve& curve, float keyTolerance, float tolerance, cmf::MemoryAllocator& allocator )
{
	if( curve.knotType == cmf::ElementType::Float32 )
	{
		auto [newKnots, newKnotType] = OptimizeValues( reinterpret_cast<const float*>( curve.knots.data() ), curve.knotCount, keyTolerance, allocator );
		if( newKnotType != cmf::ElementType::Float32 )
		{
			curve.knotType = newKnotType;
			curve.knots = newKnots;
		}
	}
	if( curve.valueType == cmf::ElementType::Float32 )
	{
		auto [newValues, newValueType] = OptimizeValues( reinterpret_cast<const float*>( curve.values.data() ), curve.knotCount * curve.valueDimension, tolerance, allocator );
		if( newValueType != cmf::ElementType::Float32 )
		{
			curve.valueType = newValueType;
			curve.values = newValues;
		}
	}
}


void SampleAnimation( SkeletonPose& pose, const cmf::Animation& animation, const Span<AnimationCurve>& curves, float time )
{
	for( const auto& channel : animation.channels )
	{
		if( !IsBoneTarget( channel.targetType ) )
		{
			continue;
		}
		const auto* const found = std::find_if( pose.skeleton->bones.begin(), pose.skeleton->bones.end(), [&channel]( const String& boneName ) {
			return boneName == channel.target;
		} );
		if( found == pose.skeleton->bones.end() )
		{
			continue;
		}
		const auto boneIndex = static_cast<uint32_t>( std::distance( pose.skeleton->bones.begin(), found ) );

		const auto& curve = curves[channel.curveIndex];
		auto& boneTransform = pose.boneTransforms[boneIndex];
		switch( channel.targetType )
		{
		case AnimationChannelTargetType::BonePosition:
			boneTransform.position = SampleVector3Curve( curve, time );
			break;
		case AnimationChannelTargetType::BoneRotation:
			boneTransform.rotation = SampleQuaternionCurve( curve, time );
			break;
		case AnimationChannelTargetType::BoneScale:
			boneTransform.scale = SampleVector3Curve( curve, time );
			break;
		default:
			break;
		}
	}
}

void RestPose( SkeletonPose& outPose, const Skeleton& skeleton )
{
	outPose.boneTransforms.resize( skeleton.restTransforms.size() );
	outPose.skeleton = &skeleton;
	std::copy( skeleton.restTransforms.begin(), skeleton.restTransforms.end(), outPose.boneTransforms.begin() );
}

void BlendPoses( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& poseB, float alpha )
{
	outPose.boneTransforms.resize( poseA.boneTransforms.size() );
	for( size_t i = 0; i < poseA.boneTransforms.size(); ++i )
	{
		outPose.boneTransforms[i] = Lerp( poseA.boneTransforms[i], poseB.boneTransforms[i], alpha );
	}
}

BoneWeights ExtractBoneWeights( const Skeleton& skeleton, const std::string_view& boneMask )
{
	BoneWeights result = { &skeleton };
	result.boneWeights.resize( skeleton.bones.size(), 0.f );

	for( const auto& mask : skeleton.boneMasks )
	{
		if( ToStdStringView( mask.name ) == boneMask )
		{
			for( const auto& boneWeight : mask.weights )
			{
				result.boneWeights[boneWeight.index] = boneWeight.weight;
			}
			break;
		}
	}
	return result;
}

void BlendPoses( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& poseB, const BoneWeights& boneWeights, float alpha )
{
	outPose.boneTransforms.resize( poseA.boneTransforms.size() );
	for( size_t i = 0; i < poseA.boneTransforms.size(); ++i )
	{
		const float weight = boneWeights.boneWeights[i] * alpha;
		outPose.boneTransforms[i] = Lerp( poseA.boneTransforms[i], poseB.boneTransforms[i], weight );
	}
}

void BlendAdditivePose( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& basePose, const SkeletonPose& additivePose, float alpha )
{
	outPose.boneTransforms.resize( poseA.boneTransforms.size() );
	for( size_t i = 0; i < poseA.boneTransforms.size(); ++i )
	{
		const auto& additive = additivePose.boneTransforms[i];
		const auto& base = basePose.boneTransforms[i];
		const auto inverseBase = Inverse( base );
		const auto multiplied = Multiply( inverseBase, additive );

		const auto blended = Lerp( {}, multiplied, alpha );
		outPose.boneTransforms[i] = Multiply( poseA.boneTransforms[i], blended );
	}
}

void BlendAdditivePose( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& basePose, const SkeletonPose& additivePose, const BoneWeights& boneWeights, float alpha )
{
	outPose.boneTransforms.resize( poseA.boneTransforms.size() );
	for( size_t i = 0; i < poseA.boneTransforms.size(); ++i )
	{
		const auto& additive = additivePose.boneTransforms[i];
		const auto& base = basePose.boneTransforms[i];
		const auto inverseBase = Inverse( base );
		const auto multiplied = Multiply( inverseBase, additive );

		const float weight = boneWeights.boneWeights[i] * alpha;
		const auto blended = Lerp( {}, multiplied, weight );
		outPose.boneTransforms[i] = Multiply( poseA.boneTransforms[i], blended );
	}
}

void ComputeWorldTransforms( std::vector<Matrix>& outWorldTransforms, const SkeletonPose& pose )
{
	outWorldTransforms.resize( pose.boneTransforms.size() );
	for( size_t i = 0; i < pose.boneTransforms.size(); ++i )
	{
		const auto& localTransform = pose.boneTransforms[i];
		const auto localMatrix = TransformationMatrix( localTransform.scale, localTransform.rotation, localTransform.position );
		if( pose.skeleton->parents[i] != -1 )
		{
			outWorldTransforms[i] = localMatrix * outWorldTransforms[pose.skeleton->parents[i]];
		}
		else
		{
			outWorldTransforms[i] = localMatrix;
		}
	}
}



AnimationPlayer::AnimationPlayer( const cmf::Skeleton& skeleton, const cmf::Animation& animation ) :
	m_stopTime( animation.duration ),
	m_loopDuration( animation.duration ),
	m_skeleton( &skeleton )
{
	for( const auto& channel : animation.channels )
	{
		if( !IsBoneTarget( channel.targetType ) )
		{
			continue;
		}
		const auto* const bone = std::find_if( skeleton.bones.begin(), skeleton.bones.end(), [&channel]( const String& boneName ) {
			return boneName == channel.target;
		} );
		if( bone == skeleton.bones.end() )
		{
			continue;
		}
		const auto boneIndex = static_cast<uint32_t>( std::distance( skeleton.bones.begin(), bone ) );
		const auto& curve = animation.curves[channel.curveIndex];
		switch( channel.targetType )
		{
		case AnimationChannelTargetType::BonePosition:
			m_positionCurves.push_back( { GetKnotStream( curve ), GetValueStream<Vector3>( curve ), curve.interpolation, boneIndex } );
			break;
		case AnimationChannelTargetType::BoneRotation:
			m_rotationCurves.push_back( { GetKnotStream( curve ), GetValueStream<Vector4>( curve ), curve.interpolation, boneIndex } );
			break;
		case AnimationChannelTargetType::BoneScale:
			m_scaleCurves.push_back( { GetKnotStream( curve ), GetValueStream<Vector3>( curve ), curve.interpolation, boneIndex } );
			break;
		default:
			break;
		}
	}

	const std::scoped_lock lock( GetActiveAnimationPlayersMutex() );

	m_playerIndex = GetActiveAnimationPlayers().size(); // NOLINT(cppcoreguidelines-prefer-member-initializer): requires mutex lock; cannot use member initializer
	GetActiveAnimationPlayers().push_back( this );
}

AnimationPlayer::~AnimationPlayer()
{
	const std::scoped_lock lock( GetActiveAnimationPlayersMutex() );

	auto& activePlayers = GetActiveAnimationPlayers();
	if( m_playerIndex != activePlayers.size() - 1 )
	{
		activePlayers[m_playerIndex] = activePlayers.back();
		activePlayers[m_playerIndex]->m_playerIndex = m_playerIndex;
	}
	activePlayers.pop_back();
}

void AnimationPlayer::AdjustStopTime()
{
	if( m_explicitStopTime )
	{
		return;
	}
	if( m_loopCount == INFINITE_LOOP || m_speed <= 0 )
	{
		m_stopTime = std::numeric_limits<float>::infinity();
	}
	else
	{
		m_stopTime = m_startTime + ( m_loopDuration * float( m_loopCount ) ) / m_speed;
	}
}

uint32_t AnimationPlayer::GetLoopCount() const
{
	return m_loopCount;
}

void AnimationPlayer::SetLoopCount( uint32_t loopCount )
{
	m_loopCount = loopCount;
	AdjustStopTime();
}

float AnimationPlayer::GetSpeed() const
{
	return m_speed;
}

void AnimationPlayer::SetSpeed( float speed )
{
	m_speed = std::max( 0.0f, speed );
	AdjustStopTime();
}

float AnimationPlayer::GetStartTime() const
{
	return m_startTime;
}

void AnimationPlayer::SetStartTime( float startTime )
{
	m_startTime = startTime;
	AdjustStopTime();
}

float AnimationPlayer::GetStopTime() const
{
	return m_stopTime;
}

void AnimationPlayer::SetStopTime( float stopTime )
{
	m_stopTime = stopTime;
	m_explicitStopTime = true;
}

bool AnimationPlayer::GetExtrapolateBefore() const
{
	return m_extrapolateBefore;
}

void AnimationPlayer::SetExtrapolateBefore( bool extrapolateBefore )
{
	m_extrapolateBefore = extrapolateBefore;
}

bool AnimationPlayer::GetExtrapolateAfter() const
{
	return m_extrapolateAfter;
}

void AnimationPlayer::SetExtrapolateAfter( bool extrapolateAfter )
{
	m_extrapolateAfter = extrapolateAfter;
}

float AnimationPlayer::GetLoopDuration() const
{
	return m_loopDuration;
}


float AnimationPlayer::GetDurationLeft( float time ) const
{
	if( m_loopCount == 0 || m_speed <= 0 )
	{
		return std::numeric_limits<float>::infinity();
	}
	return std::max( 0.f, m_stopTime - time );
}

bool AnimationPlayer::IsActive( float time ) const
{
	return ( time >= m_startTime || m_extrapolateBefore ) && ( time < m_stopTime || m_extrapolateAfter );
}

int32_t AnimationPlayer::GetLoopIndex( float time ) const
{
	if( m_loopCount == 0 )
	{
		return int32_t( ( time - m_startTime ) * m_speed / m_loopDuration );
	}
	const float totalDuration = m_loopDuration * float( m_loopCount );
	const float elapsed = ( time - m_startTime ) * m_speed;
	if( elapsed >= totalDuration )
	{
		return int32_t( m_loopCount - 1 );
	}
	return int32_t( elapsed / m_loopDuration );
}

float AnimationPlayer::GetLocalTime( float time ) const
{
	time -= m_startTime;
	time *= m_speed;
	if( m_loopCount == 0 )
	{
		return std::fmod( time, m_loopDuration );
	}
	const float totalDuration = m_loopDuration * float( m_loopCount );
	if( time >= totalDuration )
	{
		return m_loopDuration;
	}
	return std::fmod( time, m_loopDuration );
}


bool AnimationPlayer::Sample( SkeletonPose& outPose, float time ) const
{
	if( time < m_startTime && !m_extrapolateBefore )
	{
		return false;
	}
	if( time >= m_stopTime && !m_extrapolateAfter )
	{
		return false;
	}
	auto localTime = GetLocalTime( time );

	SampleAtLocalTime( outPose, localTime );
	return true;
}

void AnimationPlayer::SampleAtLocalTime( SkeletonPose& outPose, float localTime ) const
{
	for( const auto& position : m_positionCurves )
	{
		auto& boneTransform = outPose.boneTransforms[position.targetIndex];
		boneTransform.position = SampleCurve( position.knots, position.values, position.interpolation, localTime, position.cache );
	}
	for( const auto& rotation : m_rotationCurves )
	{
		auto& boneTransform = outPose.boneTransforms[rotation.targetIndex];
		boneTransform.rotation = ::SampleQuaternionCurve( rotation.knots, rotation.values, rotation.interpolation, localTime, rotation.cache );
	}
	for( const auto& scale : m_scaleCurves )
	{
		auto& boneTransform = outPose.boneTransforms[scale.targetIndex];
		boneTransform.scale = SampleCurve( scale.knots, scale.values, scale.interpolation, localTime, scale.cache );
	}
}

void AnimationPlayer::RebaseClocks( float deltaTime )
{
	m_startTime += deltaTime;
	m_stopTime += deltaTime;
}

const cmf::Skeleton& AnimationPlayer::GetSkeleton() const
{
	return *m_skeleton;
}


AnimationSequencer::AnimationSequencer( const cmf::Skeleton& skeleton ) :
	m_skeleton( &skeleton )
{
}

std::shared_ptr<AnimationPlayer> AnimationSequencer::PlayAnimation( const cmf::Animation& animation )
{
	auto player = std::make_shared<AnimationPlayer>( *m_skeleton, animation );
	m_animations.push_back( player );
	return player;
}

void AnimationSequencer::StopAnimation( const std::shared_ptr<AnimationPlayer>& player )
{
	auto found = std::find( m_animations.begin(), m_animations.end(), player );
	if( found != m_animations.end() )
	{
		m_animations.erase( found );
	}
}

void AnimationSequencer::EnumerateAnimations( const std::function<void( const std::shared_ptr<AnimationPlayer>& )>& callback )
{
	for( const auto& player : m_animations )
	{
		callback( player );
	}
}

void AnimationSequencer::Sample( SkeletonPose& outPose, float time )
{
	for( const auto& player : m_animations )
	{
		if( player->Sample( outPose, time ) )
		{
			break;
		}
	}
}

void AnimationSequencer::RemoveFinishedAnimations( float time )
{
	m_animations.erase( std::remove_if( m_animations.begin(), m_animations.end(), [time]( const auto& player ) {
							return player->GetStopTime() <= time && !player->GetExtrapolateAfter();
						} ),
						m_animations.end() );
}

const cmf::Skeleton& AnimationSequencer::GetSkeleton() const
{
	return *m_skeleton;
}

void RebaseAllAnimationPlayerClocks( float deltaTime )
{
	const std::scoped_lock lock( GetActiveAnimationPlayersMutex() );
	for( auto* player : GetActiveAnimationPlayers() )
	{
		player->RebaseClocks( deltaTime );
	}
}

}