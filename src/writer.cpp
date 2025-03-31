#include "cmf/writer.h"
#include <map>
#include <numeric>

namespace
{

void PadToAlignment( std::vector<uint8_t>& buffer, uint32_t alignment )
{
	auto padding = ( buffer.size() + alignment - 1 ) / alignment * alignment - buffer.size();
	buffer.resize( buffer.size() + padding, 0 );
}

struct AssembleBuffers
{
	template <typename P, typename T>
	void operator()( P&&, T& value, const char* )
	{
		cmf::EnumerateMembers( value, *this );
	}

	template <typename P, typename T>
	void operator()( P&&, cmf::Span<T>& value, const char* )
	{
		for( auto& element : value )
		{
			if constexpr( std::is_base_of_v<cmf::SpanRepr, typename std::remove_reference<T>::type> )
			{
				operator()( value, element, "" );
			}
			else
			{
				cmf::EnumerateMembers( element, *this );
			}
		}
	};
	template <typename P>
	void operator()( P&&, cmf::BufferView& value, const char* )
	{
		if( auto found = m_assembledBuffers.find( value.offset ); found != end( m_assembledBuffers ) )
		{
			value.offset = found->second;
			return;
		}

		PadToAlignment( result, value.stride );
		uint32_t offset = uint32_t( result.size() );
		result.insert( end( result ), static_cast<const uint8_t*>( writer.GetBufferData( value ) ), static_cast<const uint8_t*>( writer.GetBufferData( value ) ) + value.size );
		m_bufferAlignment = std::lcm( m_bufferAlignment, value.stride );
		m_assembledBuffers[value.offset] = offset;
		value.offset = offset;
	}
	const cmf::BufferAllocator& writer;
	uint8_t* basePtr;
	std::vector<uint8_t>& result;
	uint32_t& m_bufferAlignment;
	std::map<uint32_t, uint32_t> m_assembledBuffers;
};

}

namespace cmf
{

std::vector<uint8_t> BuildFile( const Data& data, const BufferAllocator& allocator )
{
	auto flattened = Flatten( data );
	auto root = reinterpret_cast<Data*>( flattened.data.get() );
	root->header.bufferOffset = uint32_t( flattened.size );
	root->header.version = 1;
	std::vector<uint8_t> buffers;
	uint32_t bufferAlignment = 4;
	EnumerateMembers( *root, AssembleBuffers{ allocator, flattened.data.get(), buffers, bufferAlignment } );
	root->bufferDesc.gpuAlignment = bufferAlignment;

	PointersToOffsets( *reinterpret_cast<Data*>( flattened.data.get() ), flattened.data.get() );
	std::vector<uint8_t> result;
	result.insert( end( result ), reinterpret_cast<uint8_t*>( flattened.data.get() ), reinterpret_cast<uint8_t*>( flattened.data.get() ) + flattened.size );
	result.insert( result.end(), begin( buffers ), end( buffers ) );
	return result;
}

}