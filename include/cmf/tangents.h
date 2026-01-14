#pragma once


#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

// Generates tangents and bitangents for the specified mesh. If the mesh already has tangents and bitangents, they will be reused unless forceRebuild is true.
// The vertex declaration must contain position, normal and texcoord (with the corresponding usageIndex) attributes. The function will regenerate
// index buffers for the mesh as some vertices may need to be duplicated to accommodate the new tangents.
CARBON_MESH_EXPORT void GenerateTangents( Mesh& mesh, uint32_t usageIndex, bool forceRebuild, MemoryAllocator& allocator, BufferManager& bufferManager );

enum class TangentCompression
{
	// Quaternion-based compression using 16-bit signed normalized integers. Corresponts to cmf::Usage::PackedTangent.
	PackedTangent,
	// Angle-based compression using 16-bit signed normalized integers. Corresponts to cmf::Usage::PackedTangentAngles.
	PackedTangentLegacy,
};

struct CompressionErrorMetrics
{
    float averageNormalErrorDegrees = 0.0f;
    float averageTangentErrorDegrees = 0.0f;
    float averageBitangentErrorDegrees = 0.0f;
};

CARBON_MESH_EXPORT void CompressTangents( Mesh& mesh, uint32_t usageIndex, bool retainNormal, TangentCompression compression, CompressionErrorMetrics* metrics, MemoryAllocator& allocator, BufferManager& bufferManager );
CARBON_MESH_EXPORT void DecompressTangents( Mesh& mesh, uint32_t usageIndex, MemoryAllocator& allocator, BufferManager& bufferManager );

} // namespace cmf