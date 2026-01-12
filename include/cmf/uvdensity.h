#pragma once

#include "cmf.h"
#include "memallocator.h"
#include <vector>

namespace cmf
{

// Calculates UV density (estimated rate of UV change over model space) for a specific UV set of a mesh. Returns density in UV units per meter. 
CARBON_MESH_EXPORT float CalculateUvDensity( const cmf::Mesh& mesh, const cmf::VertexElement& posDecl, const cmf::VertexElement& uvDecl, const cmf::BufferManager& buffers );

// Calculates UV densities for all UV sets of a mesh and stores them in the mesh. Returns a span pointing to the densities (indexed by UV usageIndex).
CARBON_MESH_EXPORT std::vector<float> CalculateUvDensities( const cmf::Mesh& mesh, const cmf::BufferManager& buffers );

}