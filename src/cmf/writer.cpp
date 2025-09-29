#include "cmf/writer.h"
#include "cmf/utils.h"

namespace cmf
{

std::vector<uint8_t> BuildFile( const Data& data, const BufferAllocator& buffers, const Metadata* metadata )
{
	MemoryAllocator allocator;

   	Header header;
	
	auto flattenedData = Flatten( data );
	PointersToOffsets( *reinterpret_cast<Data*>( flattenedData.data.get() ) );

	auto& dataSection = Modify( header.sections, allocator ).emplace_back();
	dataSection.type = SectionType::Data;
	dataSection.offset = sizeof( Header );
	dataSection.size = uint32_t( flattenedData.size );
	dataSection.uncompressedSize = uint32_t( flattenedData.size );

    for ( auto& buf : buffers.m_buffers )
    {
		auto& section = Modify( header.sections, allocator ).emplace_back();
		section.type = SectionType::GpuBuffer;
		section.size = uint32_t( buf.size() );
		section.uncompressedSize = uint32_t( buf.size() );
    }

    FlattenedBuffer flattenedMetadata;
    if( metadata )
    {
        flattenedMetadata = Flatten( *metadata );
		PointersToOffsets( *reinterpret_cast<Metadata*>( flattenedMetadata.data.get() ) );

		auto& metadataSection = Modify( header.sections, allocator ).emplace_back();
        metadataSection.type = SectionType::Metadata;
        metadataSection.size = uint32_t( flattenedMetadata.size );
        metadataSection.uncompressedSize = uint32_t( flattenedMetadata.size );
	}

    auto flattenedHeader = Flatten( header );
	uint32_t offset = uint32_t( flattenedHeader.size );
	for( auto& section : reinterpret_cast<Header*>( flattenedHeader.data.get() )->sections )
    {
		section.offset = offset;
		offset += uint32_t( section.size );
    }
	PointersToOffsets( *reinterpret_cast<Header*>( flattenedHeader.data.get() ) );
	reinterpret_cast<Header*>( flattenedHeader.data.get() )->headerSize = uint32_t( flattenedHeader.size );

	std::vector<uint8_t> result;
    result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedHeader.data.get() ), reinterpret_cast<uint8_t*>( flattenedHeader.data.get() ) + flattenedHeader.size );
	result.insert( end( result ), reinterpret_cast<uint8_t*>( flattenedData.data.get() ), reinterpret_cast<uint8_t*>( flattenedData.data.get() ) + flattenedData.size );
	for( auto& buf : buffers.m_buffers )
	{
		result.insert( end( result ), buf.begin(), buf.end() );
	}

    {
		auto crcOffset = offsetof( Header, crc32 );
		auto crc = ComputeCrc32( result.data() + crcOffset + sizeof( Header::crc32 ), result.size() - ( crcOffset + sizeof( Header::crc32 ) ) );
		std::memcpy( result.data() + crcOffset, &crc, sizeof( Header::crc32 ) );
	}

	return result;
}

}