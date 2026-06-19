// Copyright © 2025 CCP ehf.

#include "cmf/writer.h"
#include "cmf/utils.h"
#include "cmf/compression.h"
#include "meshoptimizer.h"

namespace
{

struct BufferRecord
{
	uint32_t index = 0;
	uint32_t stride = 0;
	cmf::SectionCompression compression = cmf::SectionCompression::None;
};

template <typename T>
void BuildBufferIndexRemap( T& obj, std::vector<BufferRecord>& indices )
{
	auto remapBuffer = [&indices]( cmf::BufferView& view, cmf::SectionCompression compression ) {
		if( view.size == 0 )
		{
			return; // Empty buffer, skip it
		}
		auto found = find_if( begin( indices ), end( indices ), [&view]( const BufferRecord& record ) {
			return record.index == view.index;
		} );
		if( found == end( indices ) )
		{
			indices.push_back( { view.index, view.stride, compression } );
		}
		else if( view.stride != found->stride || compression != found->compression )
		{
			// Two buffer views with different strides or usages pointing to the same buffer.
			// We can't use mesh optimizer compression in this case, so we need to fall back to uncompressed buffer.
			found->stride = 0;
			found->compression = cmf::SectionCompression::None;
		}
	};

	if constexpr( std::is_same_v<T, cmf::MeshLod> )
	{
		// We need to treat index buffers separately becuase of the different compression method used for index buffers (MeshOptimizerIndexBuffer vs MeshOptimizerVertexBuffer)
		remapBuffer( obj.vb, cmf::SectionCompression::MeshOptimizerVertexBuffer );
		remapBuffer( obj.ib, cmf::SectionCompression::MeshOptimizerIndexBuffer );
		BuildBufferIndexRemap( obj.areas, indices );
		BuildBufferIndexRemap( obj.morphTargets, indices );
	}
	else if constexpr( std::is_same_v<T, cmf::BufferView> )
	{
		remapBuffer( obj, cmf::SectionCompression::MeshOptimizerVertexBuffer );
	}
	else if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		for( auto& element : obj )
		{
			BuildBufferIndexRemap( element, indices );
		}
	}
	else
	{
		cmf::EnumerateMembers( obj, [&indices]( auto&&, auto& value, const char* ) {
			BuildBufferIndexRemap( value, indices );
		} );
	}
}


template <typename T>
void RemapBufferIndices( T& obj, const std::vector<BufferRecord>& indices )
{
	if constexpr( std::is_same_v<T, cmf::BufferView> )
	{
		auto found = find_if( begin( indices ), end( indices ), [&obj]( const BufferRecord& record ) {
			return record.index == obj.index;
		} );
		if( found != end( indices ) )
		{
			obj.index = uint32_t( distance( begin( indices ), found ) + 1 ); // +1 because 0 is the "data" segment
		}
	}
	else if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		for( auto& element : obj )
		{
			RemapBufferIndices( element, indices );
		}
	}
	else
	{
		cmf::EnumerateMembers( obj, [&indices]( auto&&, auto& value, const char* ) {
			RemapBufferIndices( value, indices );
		} );
	}
}

uint32_t Align( uint32_t offset, uint32_t alignment )
{
	return ( offset + alignment - 1 ) & ~( alignment - 1 );
}

void PadToAlignment( std::vector<uint8_t>& data, uint32_t alignment )
{
	data.resize( Align( uint32_t( data.size() ), alignment ), 0 );
}

}

namespace cmf
{

std::vector<uint8_t> BuildFile( const Data& data, const BufferManager& buffers, const Metadata* metadata )
{
	MemoryAllocator allocator;

	Header header;

	auto flattenedData = Flatten( data );
	PointersToOffsets( *reinterpret_cast<Data*>( flattenedData.data.get() ) );

	auto& dataSection = Modify( header.sections, allocator ).emplace_back();
	dataSection.type = SectionType::Data;
	dataSection.offset = sizeof( Header );
	dataSection.compressedSize = uint32_t( flattenedData.size );
	dataSection.uncompressedSize = uint32_t( flattenedData.size );

	std::vector<BufferRecord> bufferIndices;
	BuildBufferIndexRemap( *reinterpret_cast<Data*>( flattenedData.data.get() ), bufferIndices );
	RemapBufferIndices( *reinterpret_cast<Data*>( flattenedData.data.get() ), bufferIndices );

	// The compressed data of each buffer
	std::vector<std::vector<uint8_t>> compressedBufferDatas( bufferIndices.size() );

	for( size_t i = 0; i < bufferIndices.size(); i++ )
	{
		const uint32_t bufferIndex = bufferIndices[i].index;

		auto& section = Modify( header.sections, allocator ).emplace_back();
		section.type = SectionType::GpuBuffer;

		const BufferManager::Buffer buffer = buffers.GetBuffer( bufferIndex );
		// Compress the buffer and store the result
		std::vector<uint8_t>& compressedData = compressedBufferDatas[i];

		if( buffer.compression == bufferIndices[i].compression )
		{
			// The buffer is already compressed with the desired compression, we can just use it directly
			compressedData.resize( buffer.compressedSize );
			memcpy( compressedData.data(), buffer.data, buffer.compressedSize );
		}
		else
		{
			std::vector<uint8_t> decompressedData( buffer.size );
			Decompress( decompressedData.data(), Section{ 0, buffer.compressedSize, buffer.size, uint16_t( buffer.compressionStride ), SectionType::GpuBuffer, buffer.compression }, buffer.data );
			compressedData = Compress( decompressedData.data(), buffer.size, bufferIndices[i].stride, bufferIndices[i].compression );
		}

		section.compressedSize = (uint32_t)compressedData.size();
		section.uncompressedSize = buffer.size;
		section.compression = bufferIndices[i].compression;
		section.gpuAlignment = bufferIndices[i].stride; //TODO: This is a hack! Needs to be fixed!
	}

	FlattenedBuffer flattenedMetadata;
	if( metadata )
	{
		flattenedMetadata = Flatten( *metadata );
		PointersToOffsets( *reinterpret_cast<Metadata*>( flattenedMetadata.data.get() ) );

		auto& metadataSection = Modify( header.sections, allocator ).emplace_back();
		metadataSection.type = SectionType::Metadata;
		metadataSection.compressedSize = uint32_t( flattenedMetadata.size );
		metadataSection.uncompressedSize = uint32_t( flattenedMetadata.size );
	}

	const uint32_t sectionAlignment = 8;

	auto flattenedHeader = Flatten( header );
	uint32_t offset = uint32_t( flattenedHeader.size );
	for( auto& section : reinterpret_cast<Header*>( flattenedHeader.data.get() )->sections )
	{
		offset = Align( offset, sectionAlignment ); // Align the offset to the required alignment
		section.offset = offset;
		offset += section.compressedSize;
	}
	PointersToOffsets( *reinterpret_cast<Header*>( flattenedHeader.data.get() ) );
	reinterpret_cast<Header*>( flattenedHeader.data.get() )->headerSize = uint32_t( flattenedHeader.size );

	std::vector<uint8_t> result;
	result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedHeader.data.get() ), reinterpret_cast<uint8_t*>( flattenedHeader.data.get() ) + flattenedHeader.size );
	PadToAlignment( result, sectionAlignment );
	result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedData.data.get() ), reinterpret_cast<uint8_t*>( flattenedData.data.get() ) + flattenedData.size );


	for( size_t i = 0; i < bufferIndices.size(); i++ )
	{
		PadToAlignment( result, sectionAlignment );

		std::vector<uint8_t>& data = compressedBufferDatas[i];
		result.insert( end( result ), begin( data ), end( data ) );
	}

	if( metadata )
	{
		PadToAlignment( result, sectionAlignment );
		result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedMetadata.data.get() ), reinterpret_cast<uint8_t*>( flattenedMetadata.data.get() ) + flattenedMetadata.size );
	}

	{
		auto crcOffset = offsetof( Header, crc32 );
		auto crc = ComputeCrc32( result.data() + crcOffset + sizeof( Header::crc32 ), result.size() - ( crcOffset + sizeof( Header::crc32 ) ) );
		std::memcpy( result.data() + crcOffset, &crc, sizeof( Header::crc32 ) );
	}

	return result;
}

}