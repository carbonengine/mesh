// Copyright © 2026 CCP ehf.

#include "cmf/compression.h"
#include "meshoptimizer.h"

namespace cmf
{

std::vector<uint8_t> Compress( const void* data, uint32_t size, uint32_t compressionStride, SectionCompression compression )
{
	std::vector<uint8_t> compressedData;
	switch( compression )
	{

	case SectionCompression::MeshOptimizerVertexBuffer: {

		size_t count = size / compressionStride;
		size_t maximumSize = meshopt_encodeVertexBufferBound( count, compressionStride );
		compressedData.resize( maximumSize );
		size_t compressedSize = meshopt_encodeVertexBuffer( compressedData.data(), compressedData.size(), data, count, compressionStride );
		compressedData.resize( compressedSize );
		break;
	}

	case SectionCompression::MeshOptimizerIndexBuffer: {

		size_t count = size / compressionStride;
		size_t maximumSize = meshopt_encodeIndexBufferBound( count, (uint32_t)-1 );
		compressedData.resize( maximumSize );

		size_t compressedSize;

		if( compressionStride == 4 )
		{
			compressedSize = meshopt_encodeIndexBuffer( compressedData.data(), compressedData.size(), reinterpret_cast<const uint32_t*>( data ), count );
		}
		else if( compressionStride == 2 )
		{
			compressedSize = meshopt_encodeIndexBuffer( compressedData.data(), compressedData.size(), reinterpret_cast<const uint16_t*>( data ), count );
		}
		else
		{
			compressedSize = meshopt_encodeIndexBuffer( compressedData.data(), compressedData.size(), reinterpret_cast<const uint8_t*>( data ), count );
		}

		compressedData.resize( compressedSize );

		break;
	}
	default:
		//No compression
		const uint8_t* pointer = reinterpret_cast<const uint8_t*>( data );
		compressedData.assign( pointer, pointer + size );
		break;
	}
	return compressedData;
}

void Decompress( void* dest, const Section& section, const void* sectionData )
{
	switch( section.compression )
	{
	case SectionCompression::None:
		memcpy( dest, sectionData, section.uncompressedSize );
		break;

	case SectionCompression::MeshOptimizerVertexBuffer:
		meshopt_decodeVertexBuffer( dest, section.uncompressedSize / section.gpuAlignment, section.gpuAlignment, static_cast<const uint8_t*>( sectionData ), section.compressedSize );
		break;
	case SectionCompression::MeshOptimizerIndexBuffer:
		meshopt_decodeIndexBuffer( dest, section.uncompressedSize / section.gpuAlignment, section.gpuAlignment, static_cast<const uint8_t*>( sectionData ), section.compressedSize );
		break;
	}
}

}