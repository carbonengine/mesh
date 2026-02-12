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


void from_json( const nlohmann::json& j, cmf::ElementType& p );
void to_json( nlohmann::json& j, const cmf::ElementType& p );


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
	// Import vertex colors from the source mesh
	bool colors = false;
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
	// Element type to use for UV sets.
	cmf::ElementType uvType = cmf::ElementType::Float16;

	MorphTargetOptions morphTargets;
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
};

void from_json( const nlohmann::json& j, AnimationImportOptions& p );
void to_json( nlohmann::json& j, const AnimationImportOptions& p );


struct ImportOptions
{
	MeshImportOptions meshOptions;
	SkeletonImportOptions skeletonOptions;
	AnimationImportOptions animationOptions;
};

void from_json( const nlohmann::json& j, ImportOptions& p );
void to_json( nlohmann::json& j, const ImportOptions& p );

void ValidateOptions( const ImportOptions& options );