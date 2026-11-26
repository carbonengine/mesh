// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/animation.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"

// Internal curve sampling helpers shared by AnimationPlayer and the animation network evaluation.
// Evaluation caches are passed in explicitly so that curve accessors can be shared between users.

namespace cmf::detail
{

inline bool IsBoneTarget( cmf::AnimationChannelTargetType targetType )
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

inline Interval FindKnotInterval( const Knots& knots, float time )
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
Interval FindKnotInterval( const Knots& knots, float time, cmf::CurveEvaluationCache<T>& cache ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
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

inline Knots GetKnotStream( const cmf::AnimationCurve& curve )
{
	cmf::VertexElement element = {};
	element.type = curve.knotType;
	element.elementCount = 1;
	const auto stride = cmf::GetVertexElementSize( element );
	return { element, curve.knots.data(), static_cast<uint32_t>( curve.knots.size() / stride ), stride };
}

template <typename T>
cmf::ConstBufferElementStream<T> GetValueStream( const cmf::AnimationCurve& curve ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	cmf::VertexElement element = {};
	element.type = curve.valueType;
	element.elementCount = curve.valueDimension;
	const auto stride = cmf::GetVertexElementSize( element );
	return { element, curve.values.data(), static_cast<uint32_t>( curve.values.size() / stride ), stride };
}

template <typename T>
T SampleCurve( const cmf::AnimationCurve& curve, float time ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
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
T SampleCurve( const Knots& knots, const cmf::ConstBufferElementStream<T>& values, cmf::Interpolation interpolation, float time ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
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
T SampleCurve( const Knots& knots, const cmf::ConstBufferElementStream<T>& values, cmf::Interpolation interpolation, float time, cmf::CurveEvaluationCache<T>& cache ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
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

inline Quaternion SampleQuaternionCurve( const Knots& knots, const cmf::ConstBufferElementStream<Vector4>& values, cmf::Interpolation interpolation, float time, cmf::CurveEvaluationCache<Vector4>& cache )
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

}
