// Copyright © 2026 CCP ehf.

#include "../commands.h"
#include "../cmffile.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include "cmf/writer.h"
#include "options.h"
#include <md5.h>
#include "ufbx.h"
#include "mesh.h"
#include "skeleton.h"
#include "animation.h"
#include "transform.h"

extern const char* CMF_PROCESSOR_VERSION;

namespace
{

using json = nlohmann::json;


struct ImportFBXArguments
{
	std::string fbxPath;
	std::string cmfPath;
	std::string configPath;
	std::string metadataPath;
	NamedFilter meshFilter;
	NamedFilter skeletonFilter;
	NamedFilter animationFilter;
};

ImportOptions LoadOptions( const ImportFBXArguments& cliArgs )
{
	ImportOptions options;
	if( !cliArgs.configPath.empty() )
	{
		auto configFile = ReadFile( cliArgs.configPath.c_str() );
		try
		{
			options = nlohmann::json::parse( configFile.begin(), configFile.end() );
		}
		catch( const std::exception& e )
		{
			throw std::runtime_error( "Failed to parse config file: " + std::string( e.what() ) );
		}
	}
	if( !cliArgs.meshFilter.m_names.empty() )
	{
		options.meshOptions.namedFilter = cliArgs.meshFilter;
	}
	if( !cliArgs.skeletonFilter.m_names.empty() )
	{
		options.skeletonOptions.namedFilter = cliArgs.skeletonFilter;
	}
	if( !cliArgs.animationFilter.m_names.empty() )
	{
		options.animationOptions.namedFilter = cliArgs.animationFilter;
	}
	try
	{
		ValidateOptions( options );
	}
	catch( const std::exception& e )
	{
		throw std::runtime_error( "Invalid options: " + std::string( e.what() ) );
	}
	return options;
}

void WriteCmf( const char* path, const cmf::Data& data, const cmf::Metadata& metadata, cmf::BufferManager& bufferAllocator )
{
	auto fileData = cmf::BuildFile( data, bufferAllocator, &metadata );
	auto validated = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true, true } );
	if( !validated )
	{
		throw std::runtime_error( "Generated CMF file is invalid: " + validated.error );
	}
	WriteFile( path, fileData );
}

void ImportFBX( CLI::App& app, ImportFBXArguments& cliArgs )
{
	app.add_option( "--config", cliArgs.configPath, "Path to a JSON config file that specifies import options" )->check( CLI::ExistingFile );
	app.add_option( "--mesh", cliArgs.meshFilter.m_names, "Name of a mesh to import; may specify multiple; if not specified, import all meshes; overrides filter in JSON file" );
	app.add_option( "--skeleton", cliArgs.skeletonFilter.m_names, "Name of a skeleton to import; may specify multiple; if not specified, import all skeletons; overrides filter in JSON file" );
	app.add_option( "--animation", cliArgs.animationFilter.m_names, "Name of an animation to import; may specify multiple; if not specified, import all animations; overrides filter in JSON file" );
	app.add_option( "--path", cliArgs.metadataPath, "Overrides source path stored in metadata. Uses relative path to source from destination if not present." );
	app.add_option( "source", cliArgs.fbxPath, "Path to source FBX file" )->required()->check( CLI::ExistingFile );
	app.add_option( "destination", cliArgs.cmfPath, "Path to destination CMF file" )->required()->option_text( "TEXT:FILE" );

	app.final_callback( [&cliArgs]() {
		const ImportOptions options = LoadOptions( cliArgs );

		auto file = ReadFile( cliArgs.fbxPath.c_str() );

		ufbx_error error;
		auto* scene = ufbx_load_memory( file.data(), file.size(), nullptr, &error );
		if( !scene )
		{
			constexpr size_t largeEnoughForError = 4096;
			std::array<char, largeEnoughForError> buf;
			ufbx_format_error( buf.data(), buf.size(), &error );

			throw std::runtime_error( "Failed to parse FBX file " + cliArgs.fbxPath + ": " + std::string( buf.data() ) );
		}

		auto data = cmf::Data{};
		cmf::MemoryAllocator allocator;
		cmf::BufferManager bufferAllocator( allocator );

		const CoordinateSystem system =
			scene->metadata.file_format == UFBX_FILE_FORMAT_OBJ ?
			CoordinateSystem{ options.unitsPerMeter > 0.f ? ( 1.0f / options.unitsPerMeter ) : 1.0f } :
			CoordinateSystem( scene->settings.axes, options.unitsPerMeter > 0.f ? ( 1.0f / options.unitsPerMeter ) : float( scene->settings.unit_meters ) );

		auto [skeletons, boneMap] = ImportSkeletons( *scene, options.skeletonOptions, allocator, system );
		data.skeletons = skeletons;

		data.meshes = ImportMeshes( *scene, options.meshOptions, boneMap, allocator, bufferAllocator, system );

		if( options.animationOptions.importAnimations )
		{
			data.animations = ImportAnimations( *scene, boneMap, options.animationOptions, allocator, system );
		}

		cmf::Metadata metadata;
		if( cliArgs.metadataPath.empty() )
		{
			cliArgs.metadataPath = std::filesystem::proximate( cliArgs.fbxPath, std::filesystem::path( cliArgs.cmfPath ).remove_filename() ).string();
		}
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "source" ), allocator.AllocateString( cliArgs.metadataPath ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "sourceHash" ), allocator.AllocateString( MD5()( file.data(), file.size() ) ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generator" ), allocator.AllocateString( "cmfprocessor fbximport" ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generatorVersion" ), allocator.AllocateString( CMF_PROCESSOR_VERSION ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "options" ), allocator.AllocateString( json( options ).dump() ) } );

		ufbx_free_scene( scene );
		scene = nullptr;

		WriteCmf( cliArgs.cmfPath.c_str(), data, metadata, bufferAllocator );

		if( !options.lowdetailSuffix.empty() )
		{
			const bool hasLods = std::any_of( data.meshes.begin(), data.meshes.end(), []( const auto& mesh ) { return mesh.lods.size() > 1; } );
			if( hasLods )
			{
				for( auto& mesh : data.meshes )
				{
					if( mesh.lods.size() > 1 )
					{
						auto lastLod = mesh.lods[mesh.lods.size() - 1];
						lastLod.threshold = cmf::MeshLod::MAX_THRESHOLD; // make sure the last LOD is visible at all distances
						mesh.lods = {};
						cmf::Modify( mesh.lods, allocator ).emplace_back( lastLod );
					}
					mesh.audioOcclusionMesh = {};
				}
				auto path = std::filesystem::path( cliArgs.cmfPath );
				const std::string fulldetailFilename = path.filename().string();
				const std::string lowdetailPath = path.replace_filename( path.stem().string() + options.lowdetailSuffix + path.extension().string() ).string();
				cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "fulldetail" ), allocator.AllocateString( fulldetailFilename ) } );
				WriteCmf( lowdetailPath.c_str(), data, metadata, bufferAllocator );
			}
		}
	} );
}

}

REGISTER_COMMAND( "fbximport", "Converts an FBX file into CMF", &ImportFBX );
