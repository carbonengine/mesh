#pragma once

#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

// Calculate axis-aligned bounding box for the entire mesh based on the first LOD
CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const BufferManager& buffers );

// Calculate axis-aligned bounding box for a specific mesh area based on the first LOD
CcpMath::AxisAlignedBox CalculateAreaBounds( const Mesh& mesh, uint32_t areaIndex, const BufferManager& buffers );

}
