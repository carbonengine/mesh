// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/cmf.h"
#include "cmf/memallocator.h"
#include "options.h"

/** Usage index for Color vertex declaration element storing locked vertex information */
constexpr uint8_t LOCKED_VERTEX_USAGE_INDEX = 254;

/** @brief Generates LODs for the given mesh using Simplygon.
 *
 * This function generates LODs for the given mesh using Simplygon. The input mesh is expected to have a single LOD containing the full detail geometry. 
 * The generated LODs will be added to the mesh's LOD array. The function will use the provided options to control the LOD generation process.
 * This function is stubbed out if the project is built without Simplygon support (USE_SIMPLYGON=OFF).
 *
 * @param mesh The mesh for which to generate LODs. Must contain at least one LOD with the full detail geometry.
 * @param options The options controlling the LOD generation process.
 * @param allocator The memory allocator used to allocate memory during LOD generation.
 * @param bufferAllocator The buffer manager used to manage vertex and index buffers during LOD generation.
 */
void SimplygonGenerateLods( cmf::Mesh& mesh, const SimplygonLodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator );
