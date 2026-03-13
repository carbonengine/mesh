#include "cmf/animation.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"

namespace
{

bool IsBoneTarget( cmf::AnimationChannelTargetType targetType )
{
	return targetType == cmf::AnimationChannelTargetType::BonePosition || targetType == cmf::AnimationChannelTargetType::BoneRotation || targetType == cmf::AnimationChannelTargetType::BoneScale;
}

struct Interval
{
	uint32_t knot0 = 0;
	uint32_t knot1 = 0;
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
	auto knot0 = knots[interval.knot0];
	if( time < knot0 )
	{
		return interval;
	}
	interval.knot1 = 1;
	interval.time = 1;
	while( interval.knot1 < knots.size() )
	{
		const auto knot1 = knots[interval.knot1];
		if( time < knot1 )
		{
			interval.time = ( time - knot0 ) / ( knot1 - knot0 );
			return interval;
		}
		++interval.knot0;
		++interval.knot1;
		knot0 = knot1;
	}
	interval.knot1 = interval.knot0;
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
		return valueStream[interval.knot0];
	case cmf::Interpolation::Linear: {
		const auto v0 = valueStream[interval.knot0];
		const auto v1 = valueStream[interval.knot1];
		return v0 + ( v1 - v0 ) * interval.time;
	}
	default:
		return {};
	}
}

cmf::Transform BlendTransforms( const cmf::Transform& a, const cmf::Transform& b, float alpha )
{
	return {
		a.position + ( b.position - a.position ) * alpha,
		Slerp( a.rotation, b.rotation, alpha ),
		a.scale + ( b.scale - a.scale ) * alpha
	};
}

float GetMaskedWeight( uint32_t boneIndex, const cmf::Span<cmf::BoneWeight>& boneWeights )
{
	for( const auto& boneWeight : boneWeights )
	{
		if( boneWeight.index == boneIndex )
		{
			return boneWeight.weight;
		}
	}
	return 0.0f;
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
		return valueStream[interval.knot0];
	case cmf::Interpolation::Linear:
		return Slerp( valueStream[interval.knot0], valueStream[interval.knot1], interval.time );
	default:
		return {};
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
		outPose.boneTransforms[i] = BlendTransforms( poseA.boneTransforms[i], poseB.boneTransforms[i], alpha );
	}
}

void BlendPoses( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& poseB, const Span<BoneWeight>& boneWeights )
{
	outPose.boneTransforms.resize( poseA.boneTransforms.size() );
	for( size_t i = 0; i < poseA.boneTransforms.size(); ++i )
	{
		const float weight = GetMaskedWeight( uint32_t( i ), boneWeights );
		outPose.boneTransforms[i] = BlendTransforms( poseA.boneTransforms[i], poseB.boneTransforms[i], weight );
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


}