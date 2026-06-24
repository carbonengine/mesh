// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"
#include "fbximport/options.h"
#include "fbximport/mesh.h"

namespace
{

struct GenerateAudioOcclusionArguments
{
	std::string path;
	std::string configPath;
	bool force = false;
};

void GenerateAudioOcclusion( CLI::App& app, GenerateAudioOcclusionArguments& arguments )
{
	app.add_option( "--config", arguments.configPath, "Path to a JSON config file that specifies audio occlusion options" )->check( CLI::ExistingFile );
	app.add_flag( "--force", arguments.force, "Force regeneration of audio occlusion data" );
	app.add_option( "file", arguments.path, "Path to CMF file" )->required()->check( CLI::ExistingFile );
	app.final_callback( [&arguments]() {
		CmfFile file( arguments.path );

		AudioOcclusionMeshOptions options;
		options.generate = true;
		if( !arguments.configPath.empty() )
		{
			const auto configFile = ReadFile( arguments.configPath.c_str() );
			try
			{
				options = nlohmann::json::parse( configFile.begin(), configFile.end() );
			}
			catch( const std::exception& e )
			{
				throw std::runtime_error( "Failed to parse config file " + arguments.configPath + ": " + std::string( e.what() ) );
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
			if( !mesh.audioOcclusionMesh.indices.empty() )
			{
				if( !arguments.force )
				{
					continue;
				}
				modified = true;
			}
			mesh.audioOcclusionMesh = {};

			GenerateAudioOcclusionMesh( mesh, options, allocator, bufferManager );
			modified |= !mesh.audioOcclusionMesh.indices.empty();
		}
		if( modified )
		{
			cmf::Modify( file.GetMetadata().entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generateaudioocclusion_options" ), allocator.AllocateString( nlohmann::json( options ).dump() ) } );
			file.Save( arguments.path );
		}
	} );
}
}

REGISTER_COMMAND( "generateaudioocclusion", "Generates audio occlusion data for a CMF file", &GenerateAudioOcclusion );