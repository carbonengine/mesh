// Copyright © 2025 CCP ehf.

#include "cmfcontent.h"

#include <cmf/utils.h>
#include <cmf/animation.h>
#include <cmf/compression.h>

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
std::shared_ptr<CmfContent> LoadContentFromFile( const std::string& filePath )
{
	Log::Info( "Loading cmf file: %s", filePath.c_str() );

	// read the file and create a CmfContent object
	const char* filename = filePath.c_str();
	const char* mode = "rb";

	FILE* file = nullptr;
	fopen_s( &file, filename, mode );

	if( !file )
	{
		Log::Error( "Failed to open file: %s", filename );
		return nullptr;
	}

	fseek( file, 0, SEEK_END );
	size_t fileSize = ftell( file );
	fseek( file, 0, SEEK_SET );
	std::vector<uint8_t> fileData( fileSize );
	size_t bytesRead = fread( fileData.data(), 1, fileSize, file );
	if( bytesRead != fileSize )
	{
		Log::Error( "Failed to read file: %s", filename );
		fclose( file );
		return nullptr;
	}
	fclose( file );

	auto validationResult = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
	if( !validationResult )
	{
		Log::Error( "File %s validation failed: %s", filename, validationResult.error.c_str() );
		return nullptr;
	}

	// cast the data to m_cmfModel
	return std::make_shared<CmfContent>( fileData, filePath );
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

CcpMath::Sphere CmfContent::GetBoundingSphere() const
{
	if( !m_cmfData->meshes.empty() )
	{
		// add up all the bounding boxes and create a sphere
		CcpMath::AxisAlignedBox accumulated{};
		for( const auto& mesh : m_cmfData->meshes )
		{
			accumulated.IncludeBox( mesh.bounds );
		}

		return CcpMath::Sphere( accumulated );
	}

	if( !m_cmfData->skeletons.empty() )
	{
		auto sphere = CcpMath::Sphere( Vector4( 0.0, 0.0, 0.0, 0.0 ) );
		for( const auto& skeleton : m_cmfData->skeletons )
		{
			cmf::SkeletonPose pose;
			cmf::RestPose( pose, skeleton );
			std::vector<Matrix> boneWorldTransforms;
			cmf::ComputeWorldTransforms( boneWorldTransforms, pose );
			for( const auto& transform : boneWorldTransforms )
			{
				sphere.IncludePoint( transform.GetTranslation() );
			}
		}
		return sphere;
	}
	return CcpMath::Sphere( Vector4( 0.0f, 0.0f, 0.0f, 1.0f ) );
}

const uint8_t* CmfContent::Index( size_t sectionIndex, size_t offset ) const
{
	if( m_decompressedSections.size() <= sectionIndex )
	{
		m_decompressedSections.resize( sectionIndex + 1 );
	}
	if( !m_decompressedSections[sectionIndex] )
	{
		auto& section = m_cmfHeader->sections[sectionIndex];
		m_decompressedSections[sectionIndex] = std::make_unique<uint8_t[]>( section.uncompressedSize );
		cmf::Decompress( m_decompressedSections[sectionIndex].get(), section, m_fileContent.data() + section.offset + offset );
	}
	return m_decompressedSections[sectionIndex].get();
}
