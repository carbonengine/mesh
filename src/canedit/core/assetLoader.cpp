// Copyright © 2026 CCP ehf.

#include "assetLoader.h"

#include <cmf/utils.h>

#include <fstream>
#include <iterator>

namespace canedit
{

std::shared_ptr<const CmfFile> CmfFile::Load( const std::filesystem::path& path, std::string& error )
{
	std::ifstream stream( path, std::ios::binary );
	if( !stream )
	{
		error = "Cannot open '" + path.u8string() + "'";
		return nullptr;
	}
	std::vector<uint8_t> bytes( ( std::istreambuf_iterator<char>( stream ) ), std::istreambuf_iterator<char>() );
	auto file = FromBytes( std::move( bytes ), error );
	if( !file )
	{
		error = "'" + path.u8string() + "': " + error;
	}
	return file;
}

std::shared_ptr<const CmfFile> CmfFile::FromBytes( std::vector<uint8_t> bytes, std::string& error )
{
	cmf::ValidationOptions options;
	options.validateCrc = true;
	options.validateHeader = true;
	options.validateMainData = true;
	const auto result = cmf::ValidateFile( bytes.data(), bytes.size(), options );
	if( !result )
	{
		error = "invalid CMF file: " + result.error;
		return nullptr;
	}
	auto file = std::make_shared<CmfFile>();
	file->m_loaded = bytes;
	file->m_bytes = std::move( bytes );
	const auto& header = *reinterpret_cast<const cmf::Header*>( file->m_loaded.data() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the file was validated above
	auto& data = *reinterpret_cast<cmf::Data*>( file->m_loaded.data() + header.sections[0].offset ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): section 0 holds the validated main data
	cmf::OffsetsToPointers( data );
	file->m_data = &data;
	return file;
}

const cmf::Data& CmfFile::GetData() const
{
	return *m_data;
}

const std::vector<uint8_t>& CmfFile::GetBytes() const
{
	return m_bytes;
}

FileAssetLoader::FileAssetLoader( const PathResolver& resolver ) :
	m_resolver( &resolver )
{
}

void FileAssetLoader::RequestLoad( std::string_view path, Callback onLoaded )
{
	auto it = m_entries.find( path );
	if( it == m_entries.end() )
	{
		it = m_entries.emplace( std::string( path ), Entry{} ).first;
	}
	auto& entry = it->second;
	if( !entry.file && entry.references == 0 )
	{
		std::string error;
		const auto physical = m_resolver->Resolve( path, &error );
		if( physical )
		{
			entry.file = CmfFile::Load( *physical, error );
		}
		entry.error = entry.file ? std::string() : error;
	}
	++entry.references;
	onLoaded( entry.file ? &entry.file->GetData() : nullptr );
}

void FileAssetLoader::Release( std::string_view path )
{
	auto it = m_entries.find( path );
	if( it == m_entries.end() || it->second.references == 0 )
	{
		return;
	}
	if( --it->second.references == 0 && !it->second.file )
	{
		// Failed loads are retried when the asset is requested again
		m_entries.erase( it );
	}
}

const std::string* FileAssetLoader::GetError( std::string_view path ) const
{
	const auto it = m_entries.find( path );
	if( it == m_entries.end() || it->second.references == 0 || it->second.file )
	{
		return nullptr;
	}
	return &it->second.error;
}

std::vector<std::pair<std::string, std::string>> FileAssetLoader::GetErrors() const
{
	std::vector<std::pair<std::string, std::string>> errors;
	for( const auto& [path, entry] : m_entries )
	{
		if( entry.references > 0 && !entry.file )
		{
			errors.emplace_back( path, entry.error );
		}
	}
	return errors;
}

bool FileAssetLoader::IsLoaded( std::string_view path ) const
{
	const auto it = m_entries.find( path );
	return it != m_entries.end() && it->second.file != nullptr;
}

std::shared_ptr<const CmfFile> FileAssetLoader::LoadUnmanaged( std::string_view path, std::string& error ) const
{
	const auto physical = m_resolver->Resolve( path, &error );
	if( !physical )
	{
		return nullptr;
	}
	return CmfFile::Load( *physical, error );
}

void FileAssetLoader::Flush()
{
	for( auto it = m_entries.begin(); it != m_entries.end(); )
	{
		it = it->second.references == 0 ? m_entries.erase( it ) : std::next( it );
	}
}

uint32_t FileAssetLoader::GetReferenceCount( std::string_view path ) const
{
	const auto it = m_entries.find( path );
	return it == m_entries.end() ? 0 : it->second.references;
}

}
