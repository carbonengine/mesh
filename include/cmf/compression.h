// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

/**
 * @brief Compresses a block of data using the specified compression method and stride. The function supports different compression algorithms based on the 
 * cmf::SectionCompression enum, and it returns the compressed data as a vector of bytes. 
 * @param data A pointer to the data to be compressed.
 * @param size The size of the data in bytes.
 * @param compressionStride The stride to use for compression.
 * @param compression The compression method to use.
 * @return A vector of bytes containing the compressed data.
 */
CARBON_MESH_EXPORT std::vector<uint8_t> Compress( const void* data, uint32_t size, uint32_t compressionStride, SectionCompression compression );

/**
 * @brief Decompresses data from a section into a destination buffer. If the section compression method is `None`, the function simply copies the data from 
 * the source to the destination. For other compression methods, it uses the appropriate decompression algorithm based on the section's compression type and 
 * metadata. The function does not perform any validation on the parameters, so it assumes that the destination buffer is large enough to hold the uncompressed 
 * data and that the section metadata correctly describes the compressed data. The size of the destination buffer should be at least `section.uncompressedSize` bytes.
 * @param dest Pointer to the destination buffer where decompressed data will be written.
 * @param section Reference to the section containing metadata about the data to decompress.
 * @param sectionData Pointer to the compressed source data to decompress.
 */
CARBON_MESH_EXPORT void Decompress( void* dest, const Section& section, const void* sectionData );

}