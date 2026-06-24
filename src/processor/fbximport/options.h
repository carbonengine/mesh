// Copyright © 2026 CCP ehf.

#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "cmf/cmf.h"

struct NamedFilter
{
	bool operator()( const std::string& name ) const;

	std::vector<std::string> m_names;
};

struct MorphTargetOptions
{
	// Import morph targets (blend shapes) from the source file if available
	bool importMorphTargets = true;
	// Use normals stored as custom geometry properties in the FBX file instead of generating normals from morphed vertex positions and mesh topology.
	bool useCustomNormals = false;
};

void from_json( const nlohmann::json& j, MorphTargetOptions& p );
void to_json( nlohmann::json& j, const MorphTargetOptions& p );

namespace cmf::v1
{
void from_json( const nlohmann::json& j, ElementType& p );
void to_json( nlohmann::json& j, const ElementType& p );
}

struct SimplygonLodOptions
{
	// Maximum number of LODs for the mesh, including LOD0.
	uint32_t maxLods = 6;
	// Relative importance of vertex positions (silouette preservation)
	float geometryImportance = 1.f;
	// Relative importance of mesh area borders
	float areaImportance = 1.f;
	// Relative importance of shading (normals and tangents)
	float normalImportance = 1.f;
	// Relative importance of texture coordinates
	float uvImportance = 1.f;
	// Relative importance of bone weights (if more that one bone per vertex)
	float skinningImportance = 1.f;
	// Relative importance of vertex colors
	float vertexColorImportance = 1.f;
	// Multiplier for screen size thresholds for reported LOD threshold. The larger the number the more aggressive the LOD reduction will be.
	float screenSizeFactor = 2.f;
	// Name of the vertex color channel containing "locked" vertex flags. If specified, vertices with a non-zero red value in this channel will be
	// locked and not moved during LOD generation.
	std::string lockVertexChannel;
};

void from_json( const nlohmann::json& j, SimplygonLodOptions& p );
void to_json( nlohmann::json& j, const SimplygonLodOptions& p );

enum class SimplygonHoleFilling
{
	Disabled,
	Low,
	Medium,
	High,
};

void from_json( const nlohmann::json& j, SimplygonHoleFilling& p );
void to_json( nlohmann::json& j, const SimplygonHoleFilling& p );

struct SimplygonAudioOcclusionMeshOptions
{
	// Target screen size in pixels for the generated audio occlusion mesh. The larger the screen size, the more detailed the generated mesh will be,
	// but it will also have more triangles and be more expensive to use for audio occlusion calculations.
	uint32_t screenSize = 80;
	// Hole filling mode to use for the generated audio occlusion mesh: hint for Simplygon on how aggressive it should be with filling holes in the source mesh.
	SimplygonHoleFilling holeFilling = SimplygonHoleFilling::Medium;
};

void from_json( const nlohmann::json& j, SimplygonAudioOcclusionMeshOptions& p );
void to_json( nlohmann::json& j, const SimplygonAudioOcclusionMeshOptions& p );

enum class LodGenerationMethod
{
	Simplygon,
};

void from_json( const nlohmann::json& j, LodGenerationMethod& p );
void to_json( nlohmann::json& j, const LodGenerationMethod& p );

struct LodOptions
{
	bool generate = true;
	LodGenerationMethod method = LodGenerationMethod::Simplygon;
	SimplygonLodOptions simplygon;
};

void from_json( const nlohmann::json& j, LodOptions& p );
void to_json( nlohmann::json& j, const LodOptions& p );

enum class AudioOcclusionMeshGenerationMethod
{
	Simplygon,
};

void from_json( const nlohmann::json& j, AudioOcclusionMeshGenerationMethod& p );
void to_json( nlohmann::json& j, const AudioOcclusionMeshGenerationMethod& p );

struct AudioOcclusionMeshOptions
{
	bool generate = false;
	AudioOcclusionMeshGenerationMethod method = AudioOcclusionMeshGenerationMethod::Simplygon;
	SimplygonAudioOcclusionMeshOptions simplygon;
};

void from_json( const nlohmann::json& j, AudioOcclusionMeshOptions& p );
void to_json( nlohmann::json& j, const AudioOcclusionMeshOptions& p );

struct MeshImportOptions
{
	// Import meshes from the source file
	bool importMeshes = true;
	// A list of mesh names to import. If empty, all meshes will be imported (if importMeshes is true).
	NamedFilter namedFilter;
	// Import normal vectors from the source mesh
	bool normals = true;
	// Number of tangent spaces to import/compute. If the value is greater than zero, "normals" must be true.
	// Tangent space 0 is imported if the source FBX contains explicit tangents and alwaysComputeTangents is false; otherwise, it is computed.
	// Additional tangent spaces (1..N) are always computed. Tangents are computed using the corresponding UV set.
	// The "uvSets" option must be at least equal to this value.
	uint32_t tangents = 1;
	// Always compute tangents even if they are present in the source mesh
	bool alwaysComputeTangents = true;
	// Compress tangents into packed format
	bool compressTangents = true;
	// Use legacy tangent compression (angle-based) instead of quaternion-based compression
	bool legacyCompressedTangents = false;
	// Number of vertex color sets to import from the source mesh
	uint32_t colors = 0;
	// Element type to use for vertex color sets.
	cmf::ElementType colorType = cmf::ElementType::Float16;
	// Import skinning information from the source mesh if available
	bool skinning = true;
	// Bones per vertex. Can be either 4 or 1. If 1, the file will not contain blend weight attributes.
	uint32_t bonesPerVertex = 4;
	// Vertex element type to use for bone indices can be either UInt8 or UInt16
	cmf::ElementType boneIndexType = cmf::ElementType::UInt8;
	// Force-regenerate normals for meshes instead of importing them from the source file. This may be useful if the mesh contains
	// morph targets and the importer generates normals for morph targets.
	bool regenerateNormals = false;
	// Number of UV sets to import
	uint32_t uvSets = 1;
	// Flip V coordinate of UVs during import.
	bool flipV = true;
	// Element type to use for UV sets.
	cmf::ElementType uvType = cmf::ElementType::Float16;

	MorphTargetOptions morphTargets;
	LodOptions lods;
	AudioOcclusionMeshOptions audioOcclusionMesh;
};

void from_json( const nlohmann::json& j, MeshImportOptions& p );
void to_json( nlohmann::json& j, const MeshImportOptions& p );

struct SkeletonImportOptions
{
	// Import skeletons from the source file
	bool importSkeletons = true;
	// A list of skeleton names to import (names of root bones in FBX). If empty, all skeletons will be imported (if importSkeletons is true).
	NamedFilter namedFilter;
	// Move the root bone to the origin
	bool moveToOrigin = true;
};

void from_json( const nlohmann::json& j, SkeletonImportOptions& p );
void to_json( nlohmann::json& j, const SkeletonImportOptions& p );

struct AnimationImportOptions
{
	// Import animations from the source file
	bool importAnimations = true;
	// A list of animation names to import (names of root bones in FBX). If empty, all animations will be imported (if importAnimations is true).
	NamedFilter namedFilter;
	// Animation is exported relative to the first frame pose
	bool moveToOrigin = true;
	// Reduce keyframes to optimize animation data
	bool reduceKeyframes = true;
	// Tolerance used for keyframe reduction. The smaller the value, the more keyframes will be retained.
	float keyReductionTolerance = 0.001f;
	// Try to find a smaller numeric format for animation data (e.g. half float instead of full float) to reduce memory usage.
	bool optimizeFormat = true;
	// Tolerance for keyframe times used for format optimization. Smaller the values may prevent format optimization
	float keyTolerance = 0.001f;
	// Tolerance for keyframe values used for format optimization. Smaller the values may prevent format optimization
	float valueTolerance = 0.0001f;
};

void from_json( const nlohmann::json& j, AnimationImportOptions& p );
void to_json( nlohmann::json& j, const AnimationImportOptions& p );


struct ImportOptions
{
	MeshImportOptions meshOptions;
	SkeletonImportOptions skeletonOptions;
	AnimationImportOptions animationOptions;

	// Filename suffix for low-detail file version
	std::string lowdetailSuffix;

	// Units per meter override. If greater than zero, it will override the units in the source file
	float unitsPerMeter = 0.f;
};

void from_json( const nlohmann::json& j, ImportOptions& p );
void to_json( nlohmann::json& j, const ImportOptions& p );

void ValidateOptions( const ImportOptions& options );