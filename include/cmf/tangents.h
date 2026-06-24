// Copyright © 2026 CCP ehf.

#pragma once


#include "cmf.h"
#include "memallocator.h"
#include <tuple>

namespace cmf
{

struct FlipTangentOptions
{
	bool flipTangent = false;
	bool flipBinormal = false;
};

/**
 * @brief Generates tangents and binormals for the specified mesh. If the mesh already has tangents and binormals for the specified usage index, they will be reused unless forceRebuild is true.
 * The vertex declaration of the mesh must contain position, normal and texcoord attributes with the corresponding usage index. The function will regenerate
 * index buffers for the mesh as some vertices may need to be duplicated to accommodate the new tangents. This function will also regenerate tangents for morph targets if they exist, 
 * using the same usage index and applying the same flipping options as for the base mesh.
 * 
 * @param mesh The mesh to process.
 * @param usageIndex The index of the tangent usage set to generate.
 * @param flip Flags that control flipping of the resulting tangents and/or binormals. This may need to be used if the source UVs were flipped.
 * @param forceRebuild Whether to force rebuilding tangents even if they already exist.
 * @param allocator The memory allocator to use for memory operations.
 * @param bufferManager The buffer manager to use for buffer operations.
 * @return True if tangent generation succeeded, false otherwise.
 */
CARBON_MESH_EXPORT bool GenerateTangents( Mesh& mesh, uint32_t usageIndex, const FlipTangentOptions& flip, bool forceRebuild, MemoryAllocator& allocator, BufferManager& bufferManager );

enum class TangentCompression
{
	// Quaternion-based compression using 16-bit signed normalized integers. Corresponds to cmf::Usage::PackedTangent.
	PackedTangent,
	// Angle-based compression using 16-bit signed normalized integers. Corresponds to cmf::Usage::PackedTangentLegacy.
	PackedTangentLegacy,
};

struct CompressionErrorMetrics
{
	float averageNormalErrorDegrees = 0.0f;
	float averageTangentErrorDegrees = 0.0f;
	float averageBitangentErrorDegrees = 0.0f;
};

/** @brief Compresses tangent vectors in a mesh using the specified compression method.
* 
* The function processes all LODs of the mesh and all morph targets, compressing the tangent data for the specified usage index. The function will fail
* if the mesh already contains packed tangents or it does not have the required tangent data to perform the compression (normals, tangents, bitangents).
* Note, that regardless of the usageIndex parameter, the function always uses normals with usage index 0.
* 
 * @param mesh The mesh containing tangent data to compress.
 * @param usageIndex The index of the tangent usage set to compress.
 * @param retainNormal Whether to retain normal vectors in the mesh after the compression.
 * @param compression The compression method to apply to the tangents.
 * @param metrics Optional pointer to receive compression error metrics. Can be null.
 * @param allocator The memory allocator to use for memory operations.
 * @param bufferManager The buffer manager to use for buffer operations.
 * @return True if compression succeeded, false otherwise.
 */
CARBON_MESH_EXPORT bool CompressTangents( Mesh& mesh, uint32_t usageIndex, bool retainNormal, TangentCompression compression, CompressionErrorMetrics* metrics, MemoryAllocator& allocator, BufferManager& bufferManager );

/** @brief Decompresses tangent vectors in a mesh that were compressed using the specified compression method.
* 
* The function processes all LODs of the mesh and all morph targets, decompressing the tangent data for the specified usage index. The function will fail
* if the mesh does not contain packed tangents for the specified usage index. Note, that regardless of the usageIndex parameter, the function always writes normals with usage index 0.
* 
* @param mesh The mesh containing tangent data to decompress.
* @param usageIndex The index of the tangent usage set to decompress.
* @param allocator The memory allocator to use for memory operations.
* @param bufferManager The buffer manager to use for buffer operations.
* @return True if decompression succeeded, false otherwise.
*/
CARBON_MESH_EXPORT bool DecompressTangents( Mesh& mesh, uint32_t usageIndex, MemoryAllocator& allocator, BufferManager& bufferManager );

/** @brief Packs tangent space vectors into a Vector4 using the specified compression method.
* 
* The tangent basis is assumed to be orthonormal.
* 
 * @param compression The compression method to use when packing the tangent data.
 * @param normal The surface normal vector.
 * @param tangent The surface tangent vector.
 * @param bitangent The surface bitangent vector.
 * @return A Vector4 containing the packed tangent space data.
 */
CARBON_MESH_EXPORT Vector4 PackTangents( TangentCompression compression, const Vector3& normal, const Vector3& tangent, const Vector3& bitangent );

/** @brief Unpacks tangent space vectors from a Vector4 using the specified compression method.
* @param compression The compression method that was used to pack the tangent data.
* @param packed A Vector4 containing the packed tangent space data.
* @return A tuple containing the unpacked normal, tangent and bitangent vectors, in that order.
*/
CARBON_MESH_EXPORT std::tuple<Vector3, Vector3, Vector3> UnpackTangents( TangentCompression compression, const Vector4& packed );

} // namespace cmf