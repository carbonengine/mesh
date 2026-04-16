
#include <iostream>
#include <mikktspace.h>
#include <meshoptimizer.h>
#include "data/cmfcontent.h"
#include <cmf/cmf.h>
#include <cmf/writer.h>
#include <cmf/memallocator.h>
#include <cmf/span.h>
#include <cmf/utils.h>
#include <optimizer.h>
#include <attributes.h>

using namespace cmf;
using namespace cmf::v1;


void decompress( const uint8_t* fileData, Section section, std::vector<uint8_t>& uncompressed )
{
	uncompressed.resize( section.uncompressedSize );

	switch( section.compression )
	{
	case SectionCompression::None: {
		memcpy( uncompressed.data(), fileData + section.offset, section.uncompressedSize );
		break;
	}
	case SectionCompression::MeshOptimizerVertexBuffer: {
		meshopt_decodeVertexBuffer( uncompressed.data(), section.uncompressedSize / section.gpuAlignment, section.gpuAlignment, fileData + section.offset, section.compressedSize );
		break;
	}
	case SectionCompression::MeshOptimizerIndexBuffer: {
		meshopt_decodeIndexBuffer( uncompressed.data(), section.uncompressedSize / section.gpuAlignment, section.gpuAlignment, fileData + section.offset, section.compressedSize );
		break;
	}
	}
}

void print( CmfContent* content )
{
	for( uint32_t meshIndex = 0; meshIndex < content->m_cmfData->meshes.size(); meshIndex++ )
	{

		const Mesh& mesh = content->m_cmfData->meshes[meshIndex];

		printf( "Mesh %d:\n", meshIndex );

		printf( "    Name: %s\n", mesh.name.data() );

		printf( "    Min bounds: (%f, %f, %f)\n", mesh.bounds.m_min.x, mesh.bounds.m_min.y, mesh.bounds.m_min.z );
		printf( "    Max bounds: (%f, %f, %f)\n", mesh.bounds.m_max.x, mesh.bounds.m_max.y, mesh.bounds.m_max.z );

		/* for( int attributeIndex = 0; attributeIndex < mesh.decl.size(); attributeIndex++ )
		{
			const VertexElement& attribute = mesh.decl[attributeIndex];
			printf( "    Vertex attribute %d: %s%d\n", attributeIndex, getUsageString( attribute.usage ), attribute.usageIndex );

			int size = getElementTypeByteSize( attribute.type ) * attribute.elementCount;
			printf( "        Offset: %d\n", attribute.offset );
			printf( "        Byte size: %d\n", size );
			printf( "        Type: %s\n", getElementTypeString( attribute.type ) );
		}*/

		for( uint32_t lodIndex = 0; lodIndex < mesh.lods.size(); lodIndex++ )
		{
			const MeshLod& lod = mesh.lods[lodIndex];
			printf( "    LOD %d:\n", lodIndex );
			printf( "        Num vertices: %d\n", lod.vb.size / lod.vb.stride );
			printf( "        Vertex stride: %d\n", lod.vb.stride );

			bool hasIndexBuffer = lod.ib.size > 0;
			if( hasIndexBuffer )
			{
				printf( "        Num indices: %d\n", lod.ib.size / lod.ib.stride );
				printf( "        Index stride: %d\n", lod.ib.stride );
			}


			for( int attributeIndex = 0; attributeIndex < mesh.decl.size(); attributeIndex++ )
			{
				const VertexElement& attribute = mesh.decl[attributeIndex];

				Vector4 minValue( +FLT_MAX, +FLT_MAX, +FLT_MAX, +FLT_MAX );
				Vector4 maxValue( -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX );

				/* uint32_t vertexBufferOffset = content->m_cmfHeader->sections[lod.vb.index].offset + lod.vb.offset;
				const uint8_t* vertexData = content->m_fileContent.data() + vertexBufferOffset;

				uint32_t indexBufferOffset = content->m_cmfHeader->sections[lod.ib.index].offset + lod.ib.offset;
				const uint8_t* indexData = content->m_fileContent.data() + indexBufferOffset;*/

				std::vector<uint8_t> vertexData;
				decompress( content->m_fileContent.data(), content->m_cmfHeader->sections[lod.vb.index], vertexData );

				std::vector<uint8_t> indexData;
				decompress( content->m_fileContent.data(), content->m_cmfHeader->sections[lod.ib.index], indexData );

				//uint32_t numIndices = area.elementCount * 3;
				uint32_t numVertices = lod.vb.size / lod.vb.stride;
				uint32_t numIndices = lod.ib.size / lod.ib.stride;

				/*
				for( LodMeshArea area : lod.areas )
				{
					uint32_t start = area.firstElement * 3;
					uint32_t numAreaIndices = area.elementCount * 3;
					if( lod.ib.stride == 2 )
					{
						printf( "        Area vertex cache ACMR: %f\n", meshopt_analyzeVertexCache( reinterpret_cast<const uint16_t*>( indexData ) + start, numAreaIndices, numVertices, 32, 32, 32 ).acmr );
						printf( "        Area vertex overfetch: %f\n", meshopt_analyzeVertexFetch( reinterpret_cast<const uint16_t*>( indexData ) + start, numAreaIndices, numVertices, lod.vb.stride ).overfetch );
					}
					else
					{
						printf( "        Area vertex cache ACMR: %f\n", meshopt_analyzeVertexCache( reinterpret_cast<const uint32_t*>( indexData ) + start, numAreaIndices, numVertices, 32, 32, 32 ).acmr );
						printf( "        Area vertex overfetch: %f\n", meshopt_analyzeVertexFetch( reinterpret_cast<const uint32_t*>( indexData ) + start, numAreaIndices, numVertices, lod.vb.stride ).overfetch );
					}
				}
                */

				for( uint32_t i = 0; i < numIndices; i++ )
				{

					uint32_t index;
					if( lod.ib.stride == 2 )
					{
						index = *reinterpret_cast<const uint16_t*>( indexData.data() + i * lod.ib.stride );
					}
					else
					{
						index = *reinterpret_cast<const uint32_t*>( indexData.data() + i * lod.ib.stride );
					}

					Vector4 value = readAttribute( attribute, vertexData.data() + index * lod.vb.stride );

					for( int j = 0; j < 4; j++ )
					{
						minValue[j] = std::min( minValue[j], value[j] );
						maxValue[j] = std::max( maxValue[j], value[j] );
					}
				}

				printf( "        Vertex attribute %d: %s%d\n", attributeIndex, getUsageString( attribute.usage ), attribute.usageIndex );
				printf( "            Min: (%f, %f, %f, %f)\n", minValue.x, minValue.y, minValue.z, minValue.w );
				printf( "            Max: (%f, %f, %f, %f)\n", maxValue.x, maxValue.y, maxValue.z, maxValue.w );
			}
		}
	}
}



int main()
{

	//std::string path = "C:\\Users\\isheden\\Desktop\\Release\\ab1_t1.cmf";
	//std::string path = "C:\\Users\\isheden\\Desktop\\Release\\ab1_t1_compressed.cmf";
	std::string path = "C:\\Users\\isheden\\Desktop\\Release\\mf1_t1.cmf";
	//std::string path = "C:\\Users\\isheden\\Desktop\\Release\\uwi01_t1.cmf";

	CmfContent* content = CmfContentLoader::LoadContentFromFile( path );

	cmf::optimizer::Optimizer optimizer( content );

	print( content );


	optimizer.generateTangents( 0, true );
	optimizer.compressTangents( 0, false );
	//optimizer.decompressTangents( 0 );

	optimizer.optimizeVertexPerformance();

	optimizer.generateLODs();

	optimizer.optimizeVertexPerformance();

	{


		std::string outputPath = "C:\\Users\\isheden\\Desktop\\Release\\result.cmf";
		//std::string outputPath = "C:\\Users\\isheden\\Desktop\\Release\\shadow.cmf";

		std::vector<uint8_t> fileData = optimizer.toCmf( false );

		ValidationResult validationResult = ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
		if( !validationResult )
		{
			printf( "File %s validation failed: %s\n", outputPath.c_str(), validationResult.error.c_str() );
		}

		FILE* outFile = fopen( outputPath.c_str(), "wb" );
		if( !outFile )
		{
			printf( "Failed to open output file: %s\n", outputPath.c_str() );
			return 1;
		}
		if( fwrite( fileData.data(), 1, fileData.size(), outFile ) != fileData.size() )
		{
			printf( "Failed to write output file: %s\n", outputPath.c_str() );
			fclose( outFile );
			return 1;
		}
		fclose( outFile );
		printf( "Successfully wrote CMF file: %s\n", outputPath.c_str() );

		print( new CmfContent( fileData, outputPath.c_str() ) );
	}




	return EXIT_SUCCESS;
}