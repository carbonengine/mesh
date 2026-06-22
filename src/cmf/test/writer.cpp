// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"
#include "cmf/writer.h"
#include "cmf/utils.h"
#include "cmf/memallocator.h"
#include "cmf/compression.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <array>

namespace
{

// Unit cube vertex positions (8 vertices, 3 floats each)
// A cube from (-0.5, -0.5, -0.5) to (0.5, 0.5, 0.5)
struct CubeVertex
{
	float x, y, z;
};

// 8 unique vertices of a unit cube
static const std::array<CubeVertex, 8> g_cubeVertices = { {
	{ -0.5f, -0.5f, -0.5f },
	{ 0.5f, -0.5f, -0.5f },
	{ 0.5f, 0.5f, -0.5f },
	{ -0.5f, 0.5f, -0.5f },
	{ -0.5f, -0.5f, 0.5f },
	{ 0.5f, -0.5f, 0.5f },
	{ 0.5f, 0.5f, 0.5f },
	{ -0.5f, 0.5f, 0.5f },
} };

// 12 triangles (36 indices) for a cube
static const std::array<uint32_t, 36> g_cubeIndices = { {
	// Front face
	0,
	1,
	2,
	0,
	2,
	3,
	// Back face
	4,
	6,
	5,
	4,
	7,
	6,
	// Left face
	0,
	3,
	7,
	0,
	7,
	4,
	// Right face
	1,
	5,
	6,
	1,
	6,
	2,
	// Top face
	3,
	2,
	6,
	3,
	6,
	7,
	// Bottom face
	0,
	4,
	5,
	0,
	5,
	1,
} };

// Helper: write bytes to a temporary file and return the path
struct TempFile
{
	explicit TempFile( const std::vector<uint8_t>& data )
	{
		std::string path;
#if _WIN32
		char tempPath[MAX_PATH];
		char tempFile[MAX_PATH];
		GetTempPathA( MAX_PATH, tempPath );
		GetTempFileNameA( tempPath, "cmf", 0, tempFile );
		path = tempFile;
#else
		char tempFile[] = "/tmp/cmf_test_XXXXXX";
		int fd = mkstemp( tempFile );
		if( fd >= 0 )
			close( fd );
		path = tempFile;
#endif
		FILE* f = nullptr;
#if _WIN32
		fopen_s( &f, path.c_str(), "wb" );
#else
		f = fopen( path.c_str(), "wb" );
#endif
		fwrite( data.data(), 1, data.size(), f );
		fclose( f );
		m_path = path;
	}
	~TempFile()
	{
		std::remove( m_path.c_str() );
	}

	std::string m_path;
};

// Helper: read a file into a byte vector
std::vector<uint8_t> ReadTempFile( const std::string& path )
{
	FILE* f = nullptr;
#if _WIN32
	fopen_s( &f, path.c_str(), "rb" );
#else
	f = fopen( path.c_str(), "rb" );
#endif
	if( !f )
		return {};
	fseek( f, 0, SEEK_END );
	size_t size = ftell( f );
	fseek( f, 0, SEEK_SET );
	std::vector<uint8_t> result( size );
	if( fread( result.data(), 1, size, f ) != size )
	{
		// Failed to read the whole file
		result.clear();
	}
	fclose( f );
	return result;
}

// Helper: build a cmf::Data representing a unit cube with one mesh and one LOD
struct CubeData
{
	cmf::MemoryAllocator allocator;
	cmf::BufferManager buffers;
	cmf::Data data;

	CubeData() :
		buffers( allocator )
	{
		// Allocate vertex buffer
		const uint32_t vertexStride = sizeof( CubeVertex );
		cmf::BufferView vbView = buffers.AllocateBuffer(
			g_cubeVertices.data(),
			uint32_t( g_cubeVertices.size() * vertexStride ),
			vertexStride );

		// Allocate index buffer
		const uint32_t indexStride = sizeof( uint32_t );
		cmf::BufferView ibView = buffers.AllocateBuffer(
			g_cubeIndices.data(),
			uint32_t( g_cubeIndices.size() * indexStride ),
			indexStride );

		// Vertex declaration: position (float32 x 3)
		cmf::VertexElement posElement = {};
		posElement.usage = cmf::Usage::Position;
		posElement.usageIndex = 0;
		posElement.type = cmf::ElementType::Float32;
		posElement.elementCount = 3;
		posElement.offset = 0;

		// LodMeshArea: one area covering all 36 indices
		cmf::LodMeshArea lodArea = {};
		lodArea.firstElement = 0;
		lodArea.elementCount = uint32_t( g_cubeIndices.size() ) / 3;

		// MeshLod
		cmf::MeshLod lod = {};
		lod.vb = vbView;
		lod.ib = ibView;
		cmf::Modify( lod.areas, allocator ).emplace_back( lodArea );

		// MeshArea
		cmf::MeshArea area = {};
		area.name = allocator.AllocateString( "CubeMaterial" );
		area.bounds = CcpMath::AxisAlignedBox(
			Vector3( -0.5f, -0.5f, -0.5f ),
			Vector3( 0.5f, 0.5f, 0.5f ) );

		// Mesh
		cmf::Mesh mesh = {};
		mesh.name = allocator.AllocateString( "UnitCube" );
		cmf::Modify( mesh.decl, allocator ).emplace_back( posElement );
		cmf::Modify( mesh.lods, allocator ).emplace_back( lod );
		cmf::Modify( mesh.areas, allocator ).emplace_back( area );
		mesh.topology = cmf::MeshTopology::TriangleList;
		mesh.bounds = CcpMath::AxisAlignedBox(
			Vector3( -0.5f, -0.5f, -0.5f ),
			Vector3( 0.5f, 0.5f, 0.5f ) );

		data = {};
		cmf::Modify( data.meshes, allocator ).emplace_back( mesh );
	}
};


// Helper: build a cmf::Data representing a point list with 8 points (the cube vertices) and no indices
struct PointListData
{
	cmf::MemoryAllocator allocator;
	cmf::BufferManager buffers;
	cmf::Data data;
	PointListData() :
		buffers( allocator )
	{
		// Allocate vertex buffer
		const uint32_t vertexStride = sizeof( CubeVertex );
		cmf::BufferView vbView = buffers.AllocateBuffer(
			g_cubeVertices.data(),
			uint32_t( g_cubeVertices.size() * vertexStride ),
			vertexStride );

		// Vertex declaration: position (float32 x 3)
		cmf::VertexElement posElement = {};
		posElement.usage = cmf::Usage::Position;
		posElement.usageIndex = 0;
		posElement.type = cmf::ElementType::Float32;
		posElement.elementCount = 3;
		posElement.offset = 0;

		// MeshLod
		cmf::MeshLod lod = {};
		lod.vb = vbView;

		// Mesh
		cmf::Mesh mesh = {};
		mesh.name = allocator.AllocateString( "PointList" );
		cmf::Modify( mesh.decl, allocator ).emplace_back( posElement );
		cmf::Modify( mesh.lods, allocator ).emplace_back( lod );
		mesh.topology = cmf::MeshTopology::PointList;
		mesh.bounds = CcpMath::AxisAlignedBox(
			Vector3( -0.5f, -0.5f, -0.5f ),
			Vector3( 0.5f, 0.5f, 0.5f ) );

		data = {};
		cmf::Modify( data.meshes, allocator ).emplace_back( mesh );
	}
};

} // anonymous namespace

TEST( WriterTest, UnitCube_RoundTrip )
{
	// --- Build the cube data ---
	CubeData cube;

	// --- Write to file ---
	std::vector<uint8_t> fileBytes = cmf::BuildFile( cube.data, cube.buffers, nullptr );
	ASSERT_FALSE( fileBytes.empty() );

	TempFile tempPath( fileBytes );
	ASSERT_FALSE( tempPath.m_path.empty() );

	// --- Read back from file ---
	std::vector<uint8_t> loadedBytes = ReadTempFile( tempPath.m_path );
	ASSERT_EQ( loadedBytes.size(), fileBytes.size() );
	EXPECT_EQ( loadedBytes, fileBytes );

	// --- Validate the file ---
	auto validation = cmf::ValidateFile( loadedBytes.data(), loadedBytes.size(), { true, true, true } );
	ASSERT_TRUE( validation.valid ) << "File validation failed: " << validation.error;

	// --- Parse the loaded data ---
	cmf::Header* loadedHeader = reinterpret_cast<cmf::Header*>( loadedBytes.data() );
	cmf::OffsetsToPointers( *loadedHeader );

	// Expect at least 3 sections: Data + 2 GpuBuffers (VB, IB)
	ASSERT_GE( loadedHeader->sections.size(), 3u );
	EXPECT_EQ( loadedHeader->sections[0].type, cmf::SectionType::Data );
	EXPECT_EQ( loadedHeader->sections[1].type, cmf::SectionType::GpuBuffer );
	EXPECT_EQ( loadedHeader->sections[2].type, cmf::SectionType::GpuBuffer );

	// --- Parse the main Data section ---
	cmf::Data* loadedData = reinterpret_cast<cmf::Data*>( loadedBytes.data() + loadedHeader->sections[0].offset );
	cmf::OffsetsToPointers( *loadedData );

	// Verify mesh count
	ASSERT_EQ( loadedData->meshes.size(), 1u );
	EXPECT_EQ( loadedData->skeletons.size(), 0u );
	EXPECT_EQ( loadedData->animations.size(), 0u );

	const cmf::Mesh& loadedMesh = loadedData->meshes[0];

	// Verify mesh name
	std::string meshName( loadedMesh.name.begin(), loadedMesh.name.end() );
	EXPECT_EQ( meshName, "UnitCube" );

	// Verify vertex declaration
	ASSERT_EQ( loadedMesh.decl.size(), 1u );
	EXPECT_EQ( loadedMesh.decl[0].usage, cmf::Usage::Position );
	EXPECT_EQ( loadedMesh.decl[0].type, cmf::ElementType::Float32 );
	EXPECT_EQ( loadedMesh.decl[0].elementCount, 3 );

	// Verify topology
	EXPECT_EQ( loadedMesh.topology, cmf::MeshTopology::TriangleList );

	// Verify LODs
	ASSERT_EQ( loadedMesh.lods.size(), 1u );
	const cmf::MeshLod& loadedLod = loadedMesh.lods[0];

	// Verify LOD areas
	ASSERT_EQ( loadedLod.areas.size(), 1u );
	EXPECT_EQ( loadedLod.areas[0].firstElement, 0u );
	EXPECT_EQ( loadedLod.areas[0].elementCount, 36u / 3 );

	// Verify mesh areas
	ASSERT_EQ( loadedMesh.areas.size(), 1u );
	std::string areaName( loadedMesh.areas[0].name.begin(), loadedMesh.areas[0].name.end() );
	EXPECT_EQ( areaName, "CubeMaterial" );

	// Verify bounding box
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_min.x, -0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_min.y, -0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_min.z, -0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_max.x, 0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_max.y, 0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_max.z, 0.5f );

	// Verify no skeleton binding
	EXPECT_EQ( loadedMesh.skeleton, 0xff );

	// --- Decompress and verify GPU buffer contents ---
	cmf::MemoryAllocator loadAllocator;
	cmf::BufferManager loadBuffers( loadAllocator );

	// Register the GPU buffer sections into a BufferManager for decompression
	for( size_t i = 0; i < loadedHeader->sections.size(); ++i )
	{
		const cmf::Section& section = loadedHeader->sections[i];

		void* sectionData = loadedBytes.data() + section.offset;
		if( section.compression != cmf::SectionCompression::None )
		{
			loadBuffers.AddCompressedBuffer(
				sectionData, section.uncompressedSize, section.gpuAlignment, section.compression, section.compressedSize );
		}
		else
		{
			loadBuffers.AddBuffer( sectionData, section.uncompressedSize, section.gpuAlignment );
		}
	}

	// Decompress and verify vertex data
	{
		const float* loadedVerts = static_cast<const float*>( loadBuffers.GetData( loadedLod.vb ) );
		ASSERT_NE( loadedVerts, nullptr );

		for( size_t i = 0; i < g_cubeVertices.size(); ++i )
		{
			EXPECT_FLOAT_EQ( loadedVerts[i * 3 + 0], g_cubeVertices[i].x ) << "Vertex " << i << " X mismatch";
			EXPECT_FLOAT_EQ( loadedVerts[i * 3 + 1], g_cubeVertices[i].y ) << "Vertex " << i << " Y mismatch";
			EXPECT_FLOAT_EQ( loadedVerts[i * 3 + 2], g_cubeVertices[i].z ) << "Vertex " << i << " Z mismatch";
		}
	}

	struct Triangle
	{
		uint32_t i0, i1, i2;

		bool operator==( const Triangle& other ) const
		{
			return ( i0 == other.i0 && i1 == other.i1 && i2 == other.i2 ) ||
				( i0 == other.i1 && i1 == other.i2 && i2 == other.i0 ) ||
				( i0 == other.i2 && i1 == other.i0 && i2 == other.i1 );
		}
	};

	// Decompress and verify index data
	{
		const Triangle* loadedIndices = static_cast<const Triangle*>( loadBuffers.GetData( loadedLod.ib ) );
		ASSERT_NE( loadedIndices, nullptr );

		const Triangle* expectedIndices = reinterpret_cast<const Triangle*>( g_cubeIndices.data() );

		std::vector<Triangle> loadedTris( loadedIndices, loadedIndices + g_cubeIndices.size() / 3 );
		std::vector<Triangle> expectedTris( expectedIndices, expectedIndices + g_cubeIndices.size() / 3 );

		for( const Triangle& tri : loadedTris )
		{
			EXPECT_TRUE( std::find( expectedTris.begin(), expectedTris.end(), tri ) != expectedTris.end() ) << "Unexpected triangle: " << tri.i0 << ", " << tri.i1 << ", " << tri.i2;
		}
	}
}

TEST( WriterTest, UnitCube_RoundTrip_WithMetadata )
{
	// --- Build the cube data with metadata ---
	CubeData cube;

	cmf::MemoryAllocator metaAllocator;
	cmf::Metadata metadata = {};

	cmf::MetadataEntry entry = {};
	entry.key = metaAllocator.AllocateString( "generator" );
	entry.value = metaAllocator.AllocateString( "unit_test" );

	auto entriesSpan = metaAllocator.AllocateSpan<cmf::MetadataEntry>( 1 );
	entriesSpan[0] = entry;
	metadata.entries = entriesSpan;

	// --- Write to file ---
	std::vector<uint8_t> fileBytes = cmf::BuildFile( cube.data, cube.buffers, &metadata );
	ASSERT_FALSE( fileBytes.empty() );

	TempFile tempPath( fileBytes );
	ASSERT_FALSE( tempPath.m_path.empty() );

	// --- Read back from file ---
	std::vector<uint8_t> loadedBytes = ReadTempFile( tempPath.m_path );
	ASSERT_EQ( loadedBytes.size(), fileBytes.size() );

	// --- Validate ---
	auto validation = cmf::ValidateFile( loadedBytes.data(), loadedBytes.size(), { true, true, true } );
	EXPECT_TRUE( validation.valid ) << "File validation failed: " << validation.error;

	// --- Parse header and verify sections ---
	cmf::Header* loadedHeader = reinterpret_cast<cmf::Header*>( loadedBytes.data() );
	cmf::OffsetsToPointers( *loadedHeader );

	// Should have Data + 2 GpuBuffers + Metadata = 4 sections
	ASSERT_GE( loadedHeader->sections.size(), 4u );

	// Find the metadata section
	bool foundMetadata = false;
	for( size_t i = 0; i < loadedHeader->sections.size(); ++i )
	{
		if( loadedHeader->sections[i].type == cmf::SectionType::Metadata )
		{
			foundMetadata = true;

			// Parse the metadata
			cmf::Metadata* loadedMetadata = reinterpret_cast<cmf::Metadata*>(
				loadedBytes.data() + loadedHeader->sections[i].offset );
			cmf::OffsetsToPointers( *loadedMetadata );

			ASSERT_EQ( loadedMetadata->entries.size(), 1u );

			std::string key( loadedMetadata->entries[0].key.begin(), loadedMetadata->entries[0].key.end() );
			std::string value( loadedMetadata->entries[0].value.begin(), loadedMetadata->entries[0].value.end() );

			EXPECT_EQ( key, "generator" );
			EXPECT_EQ( value, "unit_test" );
			break;
		}
	}
	EXPECT_TRUE( foundMetadata ) << "Metadata section not found in output file";

	// --- Verify the mesh data is still intact ---
	cmf::Data* loadedData = reinterpret_cast<cmf::Data*>( loadedBytes.data() + loadedHeader->sections[0].offset );
	cmf::OffsetsToPointers( *loadedData );

	ASSERT_EQ( loadedData->meshes.size(), 1u );
	std::string meshName( loadedData->meshes[0].name.begin(), loadedData->meshes[0].name.end() );
	EXPECT_EQ( meshName, "UnitCube" );
}


TEST( WriterTest, PointList )
{
	// --- Build the pointList data ---
	PointListData pointList;

	// --- Write to file ---
	std::vector<uint8_t> fileBytes = cmf::BuildFile( pointList.data, pointList.buffers, nullptr );
	ASSERT_FALSE( fileBytes.empty() );

	TempFile tempPath( fileBytes );
	ASSERT_FALSE( tempPath.m_path.empty() );

	// --- Read back from file ---
	std::vector<uint8_t> loadedBytes = ReadTempFile( tempPath.m_path );
	ASSERT_EQ( loadedBytes.size(), fileBytes.size() );
	EXPECT_EQ( loadedBytes, fileBytes );

	// --- Validate the file ---
	auto validation = cmf::ValidateFile( loadedBytes.data(), loadedBytes.size(), { true, true, true } );
	ASSERT_TRUE( validation.valid ) << "File validation failed: " << validation.error;

	// --- Parse the loaded data ---
	cmf::Header* loadedHeader = reinterpret_cast<cmf::Header*>( loadedBytes.data() );
	cmf::OffsetsToPointers( *loadedHeader );

	// Expect at least 3 sections: Data + 1 GpuBuffer (VB)
	ASSERT_GE( loadedHeader->sections.size(), 2u );
	EXPECT_EQ( loadedHeader->sections[0].type, cmf::SectionType::Data );
	EXPECT_EQ( loadedHeader->sections[1].type, cmf::SectionType::GpuBuffer );

	// --- Parse the main Data section ---
	cmf::Data* loadedData = reinterpret_cast<cmf::Data*>( loadedBytes.data() + loadedHeader->sections[0].offset );
	cmf::OffsetsToPointers( *loadedData );

	// Verify mesh count
	ASSERT_EQ( loadedData->meshes.size(), 1u );
	EXPECT_EQ( loadedData->skeletons.size(), 0u );
	EXPECT_EQ( loadedData->animations.size(), 0u );

	const cmf::Mesh& loadedMesh = loadedData->meshes[0];

	// Verify mesh name
	EXPECT_EQ( ToStdString( loadedMesh.name ), "PointList" );

	// Verify vertex declaration
	ASSERT_EQ( loadedMesh.decl.size(), 1u );
	EXPECT_EQ( loadedMesh.decl[0].usage, cmf::Usage::Position );
	EXPECT_EQ( loadedMesh.decl[0].type, cmf::ElementType::Float32 );
	EXPECT_EQ( loadedMesh.decl[0].elementCount, 3 );

	// Verify topology
	EXPECT_EQ( loadedMesh.topology, cmf::MeshTopology::PointList );

	// Verify LODs
	ASSERT_EQ( loadedMesh.lods.size(), 1u );
	const cmf::MeshLod& loadedLod = loadedMesh.lods[0];

	// Verify LOD areas
	ASSERT_EQ( loadedLod.areas.size(), 0u );

	// Verify mesh areas
	ASSERT_EQ( loadedMesh.areas.size(), 0u );

	// Verify bounding box
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_min.x, -0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_min.y, -0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_min.z, -0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_max.x, 0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_max.y, 0.5f );
	EXPECT_FLOAT_EQ( loadedMesh.bounds.m_max.z, 0.5f );

	// Verify no skeleton binding
	EXPECT_EQ( loadedMesh.skeleton, 0xff );

	// --- Decompress and verify GPU buffer contents ---
	cmf::MemoryAllocator loadAllocator;
	cmf::BufferManager loadBuffers( loadAllocator );

	// Register the GPU buffer sections into a BufferManager for decompression
	for( size_t i = 0; i < loadedHeader->sections.size(); ++i )
	{
		const cmf::Section& section = loadedHeader->sections[i];

		void* sectionData = loadedBytes.data() + section.offset;
		if( section.compression != cmf::SectionCompression::None )
		{
			loadBuffers.AddCompressedBuffer(
				sectionData, section.uncompressedSize, section.gpuAlignment, section.compression, section.compressedSize );
		}
		else
		{
			loadBuffers.AddBuffer( sectionData, section.uncompressedSize, section.gpuAlignment );
		}
	}

	// Decompress and verify vertex data
	{
		const float* loadedVerts = static_cast<const float*>( loadBuffers.GetData( loadedLod.vb ) );
		ASSERT_NE( loadedVerts, nullptr );

		for( size_t i = 0; i < g_cubeVertices.size(); ++i )
		{
			EXPECT_FLOAT_EQ( loadedVerts[i * 3 + 0], g_cubeVertices[i].x ) << "Vertex " << i << " X mismatch";
			EXPECT_FLOAT_EQ( loadedVerts[i * 3 + 1], g_cubeVertices[i].y ) << "Vertex " << i << " Y mismatch";
			EXPECT_FLOAT_EQ( loadedVerts[i * 3 + 2], g_cubeVertices[i].z ) << "Vertex " << i << " Z mismatch";
		}
	}
}