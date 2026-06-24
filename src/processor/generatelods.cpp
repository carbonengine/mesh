// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"
#include "fbximport/options.h"
#include "fbximport/mesh.h"
#include "cmf/tangents.h"
#include "cmf/bufferutils.h"

namespace
{

std::vector<std::pair<cmf::Usage, uint8_t>> UnpackTangents( cmf::Mesh& mesh, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	std::vector<std::pair<cmf::Usage, uint8_t>> packedTangents;
	for( auto& element : mesh.decl )
	{
		if( element.usage == cmf::Usage::PackedTangent || element.usage == cmf::Usage::PackedTangentLegacy )
		{
			packedTangents.emplace_back( element.usage, element.usageIndex );
		}
	}
	for( auto& tangent : packedTangents )
	{
		cmf::DecompressTangents( mesh, tangent.second, allocator, bufferAllocator );
	}
	return packedTangents;
}

void PackTangents( cmf::Mesh& mesh, const std::vector<std::pair<cmf::Usage, uint8_t>>& packedTangents, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	for( const auto& tangent : packedTangents )
	{
		const bool retainNormal = tangent != packedTangents.back();
		const auto compression = tangent.first == cmf::Usage::PackedTangent ? cmf::TangentCompression::PackedTangent : cmf::TangentCompression::PackedTangentLegacy;
		cmf::CompressTangents( mesh, tangent.second, retainNormal, compression, nullptr, allocator, bufferAllocator );
	}
}


struct GenerateLodsArguments
{
	std::string path;
	std::string configPath;
	bool force = false;
};

void GenerateLods( CLI::App& app, GenerateLodsArguments& arguments )
{
	app.add_option( "--config", arguments.configPath, "Path to a JSON config file that specifies LOD generation options" )->check( CLI::ExistingFile );
	app.add_flag( "--force", arguments.force, "Force regeneration of LODs" );
	app.add_option( "file", arguments.path, "Path to CMF file" )->required()->check( CLI::ExistingFile );
	app.final_callback( [&arguments]() {
		CmfFile file( arguments.path );

		LodOptions options;
		if( !arguments.configPath.empty() )
		{
			const auto configFile = ReadFile( arguments.configPath.c_str() );
			try
			{
				options = nlohmann::json::parse( configFile.begin(), configFile.end() );
			}
			catch( const std::exception& e )
			{
				throw std::runtime_error( "Failed to parse config file: " + std::string( e.what() ) );
			}
			if( !options.generate )
			{
				return;
			}
		}

		bool modified = false;

		auto& allocator = file.GetAllocator();
		auto& bufferManager = file.GetBufferManager();

		for( auto& mesh : file.GetData().meshes )
		{
			if( mesh.topology != cmf::MeshTopology::TriangleList )
			{
				continue;
			}
			if( mesh.lods.size() > 1 )
			{
				if( !arguments.force )
				{
					continue;
				}
				auto firstLod = mesh.lods[0];
				mesh.lods = allocator.AllocateSpan<cmf::MeshLod>( 1 );
				mesh.lods[0] = firstLod;
			}

			const auto packedTangents = UnpackTangents( mesh, allocator, bufferManager );
			::GenerateLods( mesh, options, allocator, bufferManager );
			PackTangents( mesh, packedTangents, allocator, bufferManager );
			for( auto& lod : mesh.lods )
			{
				lod.ib = cmf::ConvertTo16BitIndexBuffer( lod.ib, allocator, bufferManager );
			}
			cmf::OptimizeBuffers( mesh, bufferManager );
			modified |= mesh.lods.size() > 1;
		}
		if( modified )
		{
			cmf::Modify( file.GetMetadata().entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "options" ), allocator.AllocateString( nlohmann::json( options ).dump() ) } );
			file.Save( arguments.path );
		}
	} );
}
}

REGISTER_COMMAND( "generatelods", "Generates LODs for a CMF file", &GenerateLods );