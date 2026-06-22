// Copyright © 2026 CCP ehf.

#include "cmffile.h"
#include "cmf/compression.h"
#include "cmf/writer.h"

// NOLINTBEGIN(cppcoreguidelines-owning-memory)
std::vector<uint8_t> ReadFile( const char* path )
{
	FILE* f = nullptr;
#if _WIN32
	fopen_s( &f, path, "rb" );
#else
	f = fopen( path, "rb" );
#endif
	if( !f )
	{
		throw std::runtime_error( std::string( "Failed to open file " ) + path );
	}

	fseek( f, 0, SEEK_END );
	const size_t s = ftell( f );
	fseek( f, 0, SEEK_SET );
	std::vector<uint8_t> fileContents( s );
	if( fread( fileContents.data(), 1, s, f ) != s )
	{
		fclose( f );
		throw std::runtime_error( std::string( "Failed to read file " ) + path );
	}
	fclose( f );
	return fileContents;
}

void WriteFile( const char* path, const std::vector<uint8_t>& data )
{
	FILE* f = nullptr;
#if _WIN32
	fopen_s( &f, path, "wb" );
#else
	f = fopen( path, "wb" );
#endif
	if( !f )
	{
		throw std::runtime_error( std::string( "Failed to open file for writing: " ) + path );
	}
	if( fwrite( data.data(), 1, data.size(), f ) != data.size() )
	{
		fclose( f );
		throw std::runtime_error( std::string( "Failed to write file: " ) + path );
	}
	fclose( f );
}
// NOLINTEND(cppcoreguidelines-owning-memory)

std::string CmfFile::m_modificationRecord;

CmfFile::CmfFile( std::string_view path )
{
	m_fileContents = ReadFile( std::string( path ).c_str() );

	auto* header = reinterpret_cast<cmf::Header*>( m_fileContents.data() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	auto result = cmf::ValidateFile( m_fileContents.data(), m_fileContents.size(), { true, true, true, true } );
	if( !result )
	{
		throw std::runtime_error( std::string( "File is not a valid CMF file: " ) + std::string( path ) + " (" + result.error + ")" );
	}

	for( auto& section : header->sections )
	{
		m_bufferManager.AddCompressedBuffer( m_fileContents.data() + section.offset, section.uncompressedSize, section.gpuAlignment, section.compression, section.compressedSize );

		if( section.type == cmf::SectionType::Metadata )
		{
			m_metadata = reinterpret_cast<cmf::Metadata*>( m_fileContents.data() + section.offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
		}
		else if( section.type == cmf::SectionType::Data )
		{
			m_data = reinterpret_cast<cmf::Data*>( m_fileContents.data() + section.offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
		}
	}
	cmf::OffsetsToPointers( *m_data );

	if( m_metadata )
	{
		cmf::OffsetsToPointers( *m_metadata );
	}
}

void CmfFile::Save( std::string_view path )
{
	InjectModificationRecord();

	if( m_metadata && m_metadata->entries.empty() )
	{
		m_metadata = nullptr;
	}

	auto fileData = cmf::BuildFile( *m_data, m_bufferManager, m_metadata );

	auto status = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true, true } );
	if( !status )
	{
		throw std::runtime_error( std::string( "Generated file is not valid: " ) + status.error );
	}

	WriteFile( std::string( path ).c_str(), fileData );
}

cmf::Data& CmfFile::GetData()
{
	return *m_data;
}

cmf::Metadata& CmfFile::GetMetadata()
{
	if( !m_metadata )
	{
		m_metadata = new( m_allocator.Allocate( sizeof( cmf::Metadata ) ) ) cmf::Metadata(); // NOLINT(cppcoreguidelines-owning-memory)
	}
	return *m_metadata;
}

cmf::Header& CmfFile::GetHeader()
{
	return *reinterpret_cast<cmf::Header*>( m_fileContents.data() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

cmf::MemoryAllocator& CmfFile::GetAllocator()
{
	return m_allocator;
}

cmf::BufferManager& CmfFile::GetBufferManager()
{
	return m_bufferManager;
}

void CmfFile::SetModificationRecord( std::string_view record )
{
	m_modificationRecord = record;
}

void CmfFile::InjectModificationRecord()
{
	if( m_modificationRecord.empty() )
	{
		return;
	}
	if( !m_metadata )
	{
		m_metadata = new( m_allocator.Allocate( sizeof( cmf::Metadata ) ) ) cmf::Metadata(); // NOLINT(cppcoreguidelines-owning-memory)
	}
	std::string modificationKey;
	for( uint32_t i = 0;; ++i )
	{
		modificationKey = "modification" + std::to_string( i );
		const auto* found = std::find_if( m_metadata->entries.begin(), m_metadata->entries.end(), [&modificationKey]( const cmf::MetadataEntry& entry ) {
			return ToStdString( entry.key ) == modificationKey;
		} );
		if( found == m_metadata->entries.end() )
		{
			break;
		}
	}
	cmf::Modify( m_metadata->entries, m_allocator ).emplace_back( cmf::MetadataEntry{ m_allocator.AllocateString( modificationKey ), m_allocator.AllocateString( m_modificationRecord ) } );
}
