// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/cmf.h"
#include "cmf/memallocator.h"
#include "transform.h"
#include "options.h"
#include "skeleton.h"

/** @brief Imports meshes from an FBX scene based on the specified import options.
 *
 * @param scene The FBX scene containing the nodes and meshes to be imported.
 * @param options The options for importing meshes, which may include filters for mesh names and other settings.
 * @param boneMap The mapping of bones to skeleton indices used for associating meshes with skeletons.
 * @param allocator The memory allocator used for allocating mesh data.
 * @param bufferAllocator The buffer manager used for allocating vertex and index buffers for the meshes.
 * @param systemTransform The coordinate system transformation to apply to the mesh data.
 * @return A span of imported meshes from the FBX scene.
 */
cmf::Span<cmf::Mesh> ImportMeshes( const ufbx_scene& scene, const MeshImportOptions& options, const BoneMap& boneMap, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator, const CoordinateSystem& systemTransform );

/** @brief Generates LODs for a mesh based on the specified options.
 *
 * This function delegates LOD generation to the method specified in options (currently only Simplygon is supported).
 *
 * @param mesh The mesh for which to generate LODs.
 * @param options The options for LOD generation, including the method and specific settings for each method.
 * @param allocator The memory allocator used for allocating mesh data.
 * @param bufferAllocator The buffer manager used for allocating vertex and index buffers.
 */
void GenerateLods( cmf::Mesh& mesh, const LodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator );

/** @brief Generates an audio occlusion mesh for a mesh based on the specified options.
 *
 * This function delegates audio occlusion mesh generation to the method specified in options (currently only Simplygon is supported).
 *
 * @param mesh The mesh for which to generate the audio occlusion mesh.
 * @param options The options for audio occlusion mesh generation, including the method and specific settings for each method.
 * @param allocator The memory allocator used for allocating mesh data.
 * @param bufferAllocator The buffer manager used for allocating vertex and index buffers.
 */
void GenerateAudioOcclusionMesh( cmf::Mesh& mesh, const AudioOcclusionMeshOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator );
