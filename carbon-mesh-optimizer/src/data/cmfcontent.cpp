#include "cmfcontent.h"
#include <cmf/utils.h>

#ifndef _WIN32
// Special case for non windows builders
inline errno_t fopen_s( FILE** stream, char const* fileName, char const* mode )
{
	*stream = fopen( fileName, mode );
	if( !*stream )
	{
		auto error = errno;
		return error ? error : -1;
	}
	return 0;
}

#endif
namespace CmfContentLoader
{
CmfContent* LoadContentFromFile( const std::string& filePath )
{
	printf( "Loading cmf file: %s\n", filePath.c_str() );

	// read the file and create a CmfContent object
	const char* filename = filePath.c_str();
	const char* mode = "rb";

	FILE* file = nullptr;
	fopen_s( &file, filename, mode );

	if( !file )
	{
		printf( "Failed to open file: %s\n", filename );
		return nullptr;
	}

	fseek( file, 0, SEEK_END );
	size_t fileSize = ftell( file );
	fseek( file, 0, SEEK_SET );
	std::vector<uint8_t> fileData( fileSize );
	size_t bytesRead = fread( fileData.data(), 1, fileSize, file );
	if( bytesRead != fileSize )
	{
		printf( "Failed to read file: %s\n", filename );
		fclose( file );
		return nullptr;
	}
	fclose( file );

	auto validationResult = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
	if( !validationResult )
	{
		printf( "File %s validation failed: %s\n", filename, validationResult.error.c_str() );
		return nullptr;
	}

	// cast the data to m_cmfModel
	return new CmfContent( fileData, filePath );
}
};

CmfContent::CmfContent() :
	m_cmfData( nullptr ),
	m_cmfHeader( nullptr )
{
}

CmfContent::CmfContent( std::vector<uint8_t> fileContent, std::string filePath ) :
	m_fileContent( fileContent ),
	m_filePath( filePath )
{
	if( m_fileContent.size() != 0 )
	{
		auto data = m_fileContent.data();
		m_cmfHeader = reinterpret_cast<cmf::Header*>( data );
		cmf::OffsetsToPointers( *m_cmfHeader );

		m_cmfData = reinterpret_cast<cmf::Data*>( data + m_cmfHeader->sections[0].offset );
		cmf::OffsetsToPointers( *m_cmfData );
	}
}

CmfContent::~CmfContent()
{
	if( m_cmfHeader )
	{
		delete m_cmfHeader;
	}
	if( m_cmfData )
	{
		delete m_cmfData;
	}
}

CcpMath::Sphere CmfContent::GetBoundingSphere() const
{
	// add up all the bounding boxes and create a sphere
	CcpMath::AxisAlignedBox accumulated;
	for( const auto& mesh : m_cmfData->meshes )
	{
		accumulated.IncludeBox( mesh.bounds );
	}

	return CcpMath::Sphere( accumulated );
}
