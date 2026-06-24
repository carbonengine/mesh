// Copyright © 2025 CCP ehf.

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

/** @brief Struct defining a reference to a part of binary data contained in a different file section.
* 
* Mostly used for defining vertex and index buffers for meshes, but can be used for any type of data. 
* 
*/
struct BufferView
{
	/// Index of the section containing the data
	uint32_t index = 0;
	/// Byte offset from the start of the section to the start of the data
	uint32_t offset = 0;
	/// Size of the data in bytes
	uint32_t size = 0;
	/// Stride in bytes between elements
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

/** @brief Enum defining the usage of a vertex element. */
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
	PackedTangentLegacy,
};

/** @brief Enum defining the storage type for the element. */
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
	Int8
};

/** @brief Number of elements in the ElementType enum. */
static constexpr uint8_t ElementTypeCount = 10;

struct VertexElement
{
	Usage usage = Usage::Position;
	uint8_t usageIndex = 0;
	ElementType type = ElementType::Float32;
	/// Number of components for this element (e.g., 3 for a Vector3, 2 for a UV coordinate, etc.), must be between 1 and 4
	uint8_t elementCount = 0;
	/// Byte offset from the start of the vertex to this element
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
	Span<uint16_t> bones;
	// Is the area affected by any non-root bones
	bool affectedByBones = false;
	// Is the area affected by any morph targets
	bool affectedByMorphTargets = false;

	static constexpr std::string_view TypeName = "MeshArea";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, bounds, "bounds" );
		visitor( *this, bones, "bones" );
		visitor( *this, affectedByBones, "affectedByBones" );
		visitor( *this, affectedByMorphTargets, "affectedByMorphTargets" );
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
	/// Maximum displacement of any vertex in this morph target, used for culling and LOD purposes. This is the maximum distance that any vertex moves when this morph target is applied at full weight.
	float maxDisplacement = 0.0f;

	static constexpr std::string_view TypeName = "MorphTarget";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, maxDisplacement, "maxDisplacement" );
	}
};

struct MorphTargets
{
	Span<VertexElement> decl;
	Span<MorphTarget> targets;

	static constexpr std::string_view TypeName = "MorphTargets";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, decl, "decl" );
		visitor( *this, targets, "targets" );
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
	static constexpr uint32_t MAX_THRESHOLD = 0xffffffff;

	BufferView vb;
	BufferView ib;
	Span<LodMeshArea> areas;
	Span<LodMorphTarget> morphTargets;
	uint32_t threshold = MAX_THRESHOLD; // max visible diameter in pixels for this LOD

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

struct AudioOcclusionMesh
{
	Span<Vector3> vertices;
	Span<uint16_t> indices;
	CcpMath::AxisAlignedBox bounds;

	static constexpr std::string_view TypeName = "AudioOcclusionMesh";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, vertices, "vertices" );
		visitor( *this, indices, "indices" );
		visitor( *this, bounds, "bounds" );
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
	MorphTargets morphTargets;
	Span<float> uvDensities;
	CcpMath::AxisAlignedBox bounds;
	AudioOcclusionMesh audioOcclusionMesh;
	MeshTopology topology = MeshTopology::TriangleList;
	/// Index of the skeleton in the parent data `skeletons` list used for this mesh, or 0xff if the mesh is not skinned
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
		visitor( *this, audioOcclusionMesh, "audioOcclusionMesh" );
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

struct BoneWeight
{
	uint32_t index = 0;
	float weight = 1;
	static constexpr std::string_view TypeName = "BoneWeight";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, index, "index" );
		visitor( *this, weight, "weight" );
	}
};

struct BoneMask
{
	String name;
	Span<BoneWeight> weights;

	static constexpr std::string_view TypeName = "BoneMask";
	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, weights, "weights" );
	}
};

struct Skeleton
{
	String name;
	Span<String> bones;
	Span<uint32_t> parents;
	Span<Transform> restTransforms;
	Span<Matrix> invBindTransforms;
	Span<BoneMask> boneMasks;

	static constexpr std::string_view TypeName = "Skeleton";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, bones, "bones" );
		visitor( *this, parents, "parents" );
		visitor( *this, restTransforms, "restTransforms" );
		visitor( *this, invBindTransforms, "invBindTransforms" );
		visitor( *this, boneMasks, "boneMasks" );
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
};

struct AnimationChannel
{
	String target;
	AnimationChannelTargetType targetType = AnimationChannelTargetType::BonePosition;
	/// Index of the curve in the parent data `curves` list used for this channel
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

struct Animation
{
	String name;
	Span<AnimationChannel> channels;
	Span<AnimationCurve> curves;
	float duration = 0;

	static constexpr std::string_view TypeName = "Animation";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, name, "name" );
		visitor( *this, channels, "channels" );
		visitor( *this, curves, "curves" );
		visitor( *this, duration, "duration" );
	}
};

struct MetadataEntry
{
	String key;
	String value;

	static constexpr std::string_view TypeName = "MetadataEntry";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, key, "key" );
		visitor( *this, value, "value" );
	}
};

struct Metadata
{
	Span<MetadataEntry> entries;

	static constexpr std::string_view TypeName = "Metadata";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, entries, "entries" );
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
	/// Size of the section data in bytes after compression. If compression is None, this is the same as uncompressedSize.
	uint32_t compressedSize = 0;
	uint32_t uncompressedSize = 0;
	/// Alignment in bytes required for the GPU when using this section as a sub-buffer. This should match the stride of the vertex/index buffers contained in this section.
	uint16_t gpuAlignment = 0;
	SectionType type = SectionType::Data;
	SectionCompression compression = SectionCompression::None;

	static constexpr std::string_view TypeName = "Section";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, offset, "offset" );
		visitor( *this, compressedSize, "compressedSize" );
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

	static constexpr std::string_view TypeName = "Data";

	template <typename T>
	constexpr void EnumerateMembers( T&& visitor )
	{
		visitor( *this, meshes, "meshes" );
		visitor( *this, skeletons, "skeletons" );
		visitor( *this, animations, "animations" );
	}
};

}
}