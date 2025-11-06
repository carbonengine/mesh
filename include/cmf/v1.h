#pragma once

#include "span.h"
#include <cstdint>
#include <string_view>
#include <CcpMath.h>

namespace cmf
{
namespace v1
{

constexpr uint32_t FILE_VERSION = 1;


struct BufferView
{
	uint32_t index = 0;
	uint32_t offset = 0;
	uint32_t size = 0;
	uint32_t stride = 0;

	static constexpr std::string_view TypeName = "BufferView";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, index, "index" );
		visitor( *this, offset, "offset" );
		visitor( *this, size, "size" );
		visitor( *this, stride, "stride" );
	}
};


enum class Usage : uint8_t
{
	Position,
	Normal,
	Tangent,
	Binormal,
	TexCoord,
	Color,
	BoneIndices,
	BoneWeights,
    PackedTangent,
};

enum class ElementType : uint8_t
{
	Float32,
	Float16,
    UInt16Norm,
	UInt16,
	Int16Norm,
	Int16,
	UInt8Norm,
	UInt8,
	Int8Norm,
	Int8,
};

struct VertexElement
{
	Usage usage = Usage::Position;
	uint8_t usageIndex = 0;
	ElementType type = ElementType::Float32;
	uint8_t elementCount = 0;
	uint32_t offset = 0;

	static constexpr std::string_view TypeName = "VertexElement";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, usage, "usage" );
		visitor( *this, usageIndex, "usageIndex" );
		visitor( *this, type, "type" );
		visitor( *this, elementCount, "elementCount" );
		visitor( *this, offset, "offset" );
	}
};

struct MeshArea
{
	String name;
	CcpMath::AxisAlignedBox bounds = {};
	Span<uint8_t> bones;

	static constexpr std::string_view TypeName = "MeshArea";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, bounds, "bounds" );
		visitor( *this, bones, "bones" );
	}
};

struct LodMeshArea
{
	uint32_t firstElement = 0;
	uint32_t elementCount = 0;

	static constexpr std::string_view TypeName = "LodMeshArea";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, firstElement, "firstElement" );
		visitor( *this, elementCount, "elementCount" );
	}
};

struct BoneBinding
{
	String name;
	CcpMath::AxisAlignedBox bounds = {};

	static constexpr std::string_view TypeName = "BoneBinding";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, bounds, "bounds" );
	}
};

struct MorphTarget
{
	String name;
	Span<VertexElement> decl;
	CcpMath::AxisAlignedBox bounds = {}; // bounds of non-zero morphed vertices
	Span<Vector4> maxDisplacements; // max displacements for each element in decl

	static constexpr std::string_view TypeName = "MorphTarget";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, decl, "decl" );
		visitor( *this, bounds, "bounds" );
		visitor( *this, maxDisplacements, "maxDisplacements" );
	}
};

struct LodMorphTarget
{
	BufferView vb;

	static constexpr std::string_view TypeName = "LodMorphTarget";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, vb, "vb" );
	}
};

struct MeshLod
{
	BufferView vb;
	BufferView ib;
	Span<LodMeshArea> areas;
	Span<LodMorphTarget> morphTargets;
	uint32_t threshold = 0xffffffff; // max visible diameter in pixels for this LOD

	static constexpr std::string_view TypeName = "MeshLod";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, vb, "vb" );
		visitor( *this, ib, "ib" );
		visitor( *this, areas, "areas" );
		visitor( *this, morphTargets, "morphTargets" );
		visitor( *this, threshold, "threshold" );
	}
};

enum class IndexType : uint8_t
{
	UInt32,
	UInt16,
};

enum class MeshTopology : uint8_t
{
	TriangleList,
	PointList,
};

struct Mesh
{
	String name;
	Span<VertexElement> decl;

	Span<MeshLod> lods;
	Span<MeshArea> areas;
	Span<BoneBinding> boneBindings;
	Span<MorphTarget> morphTargets;
	Span<float> uvDensities;
	CcpMath::AxisAlignedBox bounds;
	MeshTopology topology = MeshTopology::TriangleList;
	uint8_t skeleton = 0xff;

	static constexpr std::string_view TypeName = "Mesh";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, decl, "decl" );
		visitor( *this, lods, "lods" );
		visitor( *this, areas, "areas" );
		visitor( *this, boneBindings, "boneBindings" );
		visitor( *this, morphTargets, "morphTargets" );
		visitor( *this, uvDensities, "uvDensities" );
		visitor( *this, bounds, "bounds" );
		visitor( *this, topology, "topology" );
		visitor( *this, skeleton, "skeleton" );
	}
};

struct Transform
{
	Vector3 position = { 0, 0, 0 };
	Quaternion rotation = { 0, 0, 0, 1 };
	Vector3 scale = { 1, 1, 1 };

	static constexpr std::string_view TypeName = "Transform";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, position, "position" );
		visitor( *this, rotation, "rotation" );
		visitor( *this, scale, "scale" );
	}
};

struct Skeleton
{
	String name;
	Span<String> bones;
	Span<uint32_t> parents;
	Span<Transform> restTransforms;
	Span<Matrix> invBindTransforms;

	static constexpr std::string_view TypeName = "Skeleton";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, bones, "bones" );
		visitor( *this, parents, "parents" );
		visitor( *this, restTransforms, "restTransforms" );
		visitor( *this, invBindTransforms, "invBindTransforms" );
	}
};

enum class AnimationChannelTargetType : uint8_t
{
	BonePosition,
	BoneRotation,
	BoneScale,
	MorphTarget,
    Other,
};

enum class Interpolation : uint8_t
{
	Step,
	Linear,
	CubicSpline,
};

struct AnimationChannel
{
	String target;
	AnimationChannelTargetType targetType = AnimationChannelTargetType::BonePosition;
	uint32_t curveIndex = 0;

	static constexpr std::string_view TypeName = "BoneAnimationChannel";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, target, "target" );
		visitor( *this, targetType, "targetType" );
		visitor( *this, curveIndex, "curveIndex" );
	}
};

struct Animation
{
	String name;
	Span<AnimationChannel> channels;
	float duration = 0;

	static constexpr std::string_view TypeName = "Animation";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, channels, "channels" );
		visitor( *this, duration, "duration" );
	}
};

struct AnimationCurve
{
	uint8_t valueDimension = 0;
	Interpolation interpolation = Interpolation::Step;
	ElementType knotType = ElementType::Float32;
	ElementType valueType = ElementType::Float32;
	uint32_t knotCount = 0;
	Span<uint8_t> knots;
	Span<uint8_t> values;

	static constexpr std::string_view TypeName = "Skeleton";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, valueDimension, "valueDimension" );
		visitor( *this, interpolation, "interpolation" );
		visitor( *this, knotType, "knotType" );
		visitor( *this, valueType, "valueType" );
		visitor( *this, knotCount, "knotCount" );
		visitor( *this, knots, "knots" );
		visitor( *this, values, "values" );
	}
};

struct MetadataKeyValue
{
	String key;
	String value;

	static constexpr std::string_view TypeName = "MetadataKeyValue";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, key, "key" );
		visitor( *this, value, "value" );
	}
};

struct Metadata
{
	Span<MetadataKeyValue> values;

	static constexpr std::string_view TypeName = "Metadata";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, values, "values" );
	}
};

enum class SectionCompression : uint8_t
{
    None,
	MeshOptimizerVertexBuffer,
	MeshOptimizerIndexBuffer,
};

enum class SectionType : uint8_t
{
    Data,
    GpuBuffer,
	Metadata,
};

struct Section
{
	uint32_t offset = 0;
	uint32_t size = 0;
	uint32_t uncompressedSize = 0;
	uint16_t gpuAlignment = 0;
	SectionType type = SectionType::Data;
	SectionCompression compression = SectionCompression::None;

	static constexpr std::string_view TypeName = "Section";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, offset, "offset" );
		visitor( *this, size, "size" );
		visitor( *this, uncompressedSize, "uncompressedSize" );
		visitor( *this, gpuAlignment, "gpuAlignment" );
		visitor( *this, type, "type" );
		visitor( *this, compression, "compression" );
	}
};

struct Header
{
	uint32_t signature = FILE_SIGNATURE;
	uint32_t version = FILE_VERSION;
	uint32_t headerSize = 0; // size of the header including sections
	uint32_t crc32 = 0; // CRC32 of the file excluding signature, version, headerSize and crc32
	Span<Section> sections;

	static constexpr std::string_view TypeName = "Header";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, signature, "signature" );
		visitor( *this, version, "version" );
		visitor( *this, headerSize, "headerSize" );
		visitor( *this, crc32, "crc32" );
		visitor( *this, sections, "sections" );
	}
};

struct Data
{
	Span<Mesh> meshes;
	Span<Skeleton> skeletons;
	Span<Animation> animations;
	Span<AnimationCurve> curves;

	static constexpr std::string_view TypeName = "Data";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, meshes, "meshes" );
		visitor( *this, skeletons, "skeletons" );
		visitor( *this, animations, "animations" );
		visitor( *this, curves, "curves" );
	}
};

}
}