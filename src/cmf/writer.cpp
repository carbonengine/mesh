#include "cmf/writer.h"
#include "cmf/utils.h"
#include "cmf/compression.h"
#include "meshoptimizer.h"

namespace
{

template <typename T>
void RemapBufferIndices( T& obj, std::vector<uint32_t>& indices )
{
	if constexpr( std::is_same_v<T, cmf::BufferView>  )
    {
		auto found = find( begin( indices ), end( indices ), obj.index );
		if( found == end( indices ) )
		{
            indices.push_back( obj.index );
			obj.index = uint32_t( indices.size() ); // +1 because 0 is the "data" segment
        }
		else
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

    std::vector<uint32_t> bufferIndices;
	RemapBufferIndices( *reinterpret_cast<Data*>( flattenedData.data.get() ), bufferIndices );

    // The compressed data of each buffer
    std::vector<std::vector<uint8_t>> compressedBufferDatas( bufferIndices.size() );

	for( size_t i = 0; i < bufferIndices.size(); i++ )
	{
		uint32_t bufferIndex = bufferIndices[i];

		auto& section = Modify( header.sections, allocator ).emplace_back();
		section.type = SectionType::GpuBuffer;

        BufferManager::Buffer buffer = buffers.GetBuffer( bufferIndex );

        // Compress the buffer and store the result
        std::vector<uint8_t>& compressedData = compressedBufferDatas[i];
		compressedData = Compress( buffer );

		section.compressedSize = (uint32_t) compressedData.size();
		section.uncompressedSize = buffer.size;
		section.compression = buffer.compression;
		section.gpuAlignment = buffer.compressionStride; //TODO: This is a hack! Needs to be fixed!
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

    auto flattenedHeader = Flatten( header );
	uint32_t offset = uint32_t( flattenedHeader.size );
	for( auto& section : reinterpret_cast<Header*>( flattenedHeader.data.get() )->sections )
    {
		section.offset = offset;
		offset += section.compressedSize;
    }
	PointersToOffsets( *reinterpret_cast<Header*>( flattenedHeader.data.get() ) );
	reinterpret_cast<Header*>( flattenedHeader.data.get() )->headerSize = uint32_t( flattenedHeader.size );

	std::vector<uint8_t> result;
    result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedHeader.data.get() ), reinterpret_cast<uint8_t*>( flattenedHeader.data.get() ) + flattenedHeader.size );
	result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedData.data.get() ), reinterpret_cast<uint8_t*>( flattenedData.data.get() ) + flattenedData.size );

    
	for( size_t i = 0; i < bufferIndices.size(); i++ )
	{
        std::vector<uint8_t>& data = compressedBufferDatas[i];
		result.insert( end( result ), begin( data ), end( data ) );
	}

	result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedMetadata.data.get() ), reinterpret_cast<uint8_t*>( flattenedMetadata.data.get() ) + flattenedMetadata.size );

    {
		auto crcOffset = offsetof( Header, crc32 );
		auto crc = ComputeCrc32( result.data() + crcOffset + sizeof( Header::crc32 ), result.size() - ( crcOffset + sizeof( Header::crc32 ) ) );
		std::memcpy( result.data() + crcOffset, &crc, sizeof( Header::crc32 ) );
	}

	return result;
}

}