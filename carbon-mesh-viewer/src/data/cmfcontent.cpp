#include "cmfcontent.h"
#include <cmf/utils.h>

namespace CmfContentLoader
{
CmfContent* LoadContentFromFile( const std::string& filePath )
{
	CCP_LOGNOTICE( "Loading cmf file: %s", filePath.c_str() );

	// read the file and cast it to m_cmfModel
	const char* filename = filePath.c_str();
	const char* mode = "rb";

	FILE* file = nullptr;
	fopen_s( &file, filename, mode );

	if( !file )
	{
		CCP_LOGERR( "Failed to open file: %s", filename );
		return nullptr;
	}

	fseek( file, 0, SEEK_END );
	size_t fileSize = ftell( file );
	fseek( file, 0, SEEK_SET );
	std::vector<uint8_t> fileData( fileSize );
	size_t bytesRead = fread( fileData.data(), 1, fileSize, file );
	if( bytesRead != fileSize )
	{
		CCP_LOGERR( "Failed to read file: %s", filename );
		fclose( file );
		return nullptr;
	}
	fclose( file );

	auto validationResult = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
	if( !validationResult.first )
	{
		CCP_LOGERR( "File validation failed: %s", filename );
		return nullptr;
	}
	if( !validationResult.second.validateHeader )
	{
		CCP_LOGERR( "File header validation failed: %s", filename );
		return nullptr;
	}
	if( !validationResult.second.validateMainData )
	{
		CCP_LOGERR( "File main data validation failed: %s", filename );
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
		cmf::OffsetsToPointers( m_cmfHeader );

		m_cmfData = reinterpret_cast<cmf::Data*>( data + m_cmfHeader->sections[0].offset );
		cmf::OffsetsToPointers( m_cmfData );
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
