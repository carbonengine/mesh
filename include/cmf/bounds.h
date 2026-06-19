// Copyright © 2025 CCP ehf.

#pragma once

#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

/**
 * @brief Calculates the axis-aligned bounding box for a mesh.
 * @param mesh The mesh to calculate bounds for.
 * @param buffers The buffer manager providing access to mesh data.
 * @return An axis-aligned bounding box that encompasses the mesh.
 */
CARBON_MESH_EXPORT CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const BufferManager& buffers );

/**
 * @brief Calculates the axis-aligned bounding box for a specific area of a mesh.
 * @param mesh The mesh containing the area.
 * @param areaIndex The index of the area to calculate bounds for.
 * @param buffers The buffer manager providing access to mesh data.
 * @return The axis-aligned bounding box enclosing the specified area.
 */
CARBON_MESH_EXPORT CcpMath::AxisAlignedBox CalculateAreaBounds( const Mesh& mesh, uint32_t areaIndex, const BufferManager& buffers );

}
