#include "cmf/utils.h"

namespace cmf
{

bool IsHeaderValid( const void* data, size_t size )
{
	auto header = static_cast<const Header*>( data );
	if( size < sizeof( Header ) )
    {
        return false;
	}
    if( header->signature != FILE_SIGNATURE )
    {
        return false;
	}
	if( header->version != FILE_VERSION )
    {
        return false;
    }
	// The file must contain at least one section
    if ( header->sections.byteSize < sizeof( Section ) )
    {
		return false;
    }
	// The first section must be a data section
	auto section0 = reinterpret_cast<const Section*>( static_cast<const uint8_t*>( data ) + header->sections.offset );
	if( section0->type != SectionType::Data )
    {
        return false;
	}

	auto crcOffset = offsetof( Header, crc32 );
	auto crc = ComputeCrc32( static_cast<const uint8_t*>( data ) + crcOffset + sizeof( Header::crc32 ), size - ( crcOffset + sizeof( Header::crc32 ) ) );
	if( crc != header->crc32 )
    {
        return false;
	}
	return true;
}

uint32_t ComputeCrc32( const void* data, size_t size )
{
	const uint8_t* bytes = reinterpret_cast<const uint8_t*>( data );
	uint32_t crc = 0xFFFFFFFF;
	for( size_t i = 0; i < size; ++i )
	{
		crc ^= bytes[i];
		for( int j = 0; j < 8; ++j )
		{
			if( crc & 1 )
			{
				crc = ( crc >> 1 ) ^ 0xEDB88320;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return ~crc;
}

CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb )
{
	CcpMath::AxisAlignedBox bounds;
	auto element = std::find_if( mesh.decl.begin(), mesh.decl.end(), []( const auto& x ) { return x.usage == Usage::Position && x.usageIndex == 0; } );
	if( element == mesh.decl.end() )
	{
		return bounds;
	}
	for( auto& lod : mesh.lods )
	{
		for( auto pos : BufferElementStream<Vector3>( *element, vb, lod.vb.size / lod.vb.stride, lod.vb.stride ) )
		{
			bounds.Include( pos );
		}
		break;
	}
	return bounds;
}

CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb, const void* ib, uint32_t firstElement, uint32_t elementCount )
{
	return CalculateBounds( mesh, vb );
}

}