// Copyright © 2025 CCP ehf.

#pragma once

#include "cmf.h"
#include "memallocator.h"
#include <vector>

namespace cmf
{

/**
 * @brief Calculates the UV density of a mesh. The UV density is defined roughly as a minimal ratio of change in UVs to the change in positions. The function calculates the 
 * density for each triangle in the mesh and returns a value that represents the density of the mesh. The function filters out degenerate and outlier triangles in the process.
 * UV density can be used to drive texture LOD calculations in the client application.
 * @param mesh The mesh to analyze.
 * @param posDecl The vertex element declaration for position data.
 * @param uvDecl The vertex element declaration for UV coordinate data.
 * @param buffers The buffer manager containing the mesh data.
 * @return The calculated UV density value.
 */
CARBON_MESH_EXPORT float CalculateUvDensity( const cmf::Mesh& mesh, const cmf::VertexElement& posDecl, const cmf::VertexElement& uvDecl, const cmf::BufferManager& buffers );

/**
 * @brief Calculates the UV density values for a mesh. The function identifies all UV sets in the mesh and calculates the density for each set using the CalculateUvDensity function. 
 * The resulting vector contains the density values for each UV set, indexed by their usage index. If the mesh does not contain any UV sets or position data, an empty vector is returned.
 * @param mesh The mesh for which to calculate UV densities.
 * @param buffers The buffer manager containing the mesh data.
 * @return A vector of UV density values for the mesh.
 */
CARBON_MESH_EXPORT std::vector<float> CalculateUvDensities( const cmf::Mesh& mesh, const cmf::BufferManager& buffers );

}