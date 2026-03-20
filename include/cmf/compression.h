#pragma once

#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

CARBON_MESH_EXPORT std::vector<uint8_t> Compress( const void* data, uint32_t size, uint32_t compressionStride, SectionCompression compression );

CARBON_MESH_EXPORT void Decompress( void* dest, const Section& section, const void* sectionData );

}