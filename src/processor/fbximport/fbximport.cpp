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


std::pair<std::unique_ptr<uint8_t[]>, size_t> LoadFile( const char* path )
{
#if _WIN32
	FILE* f = nullptr;
	fopen_s( &f, path, "rb" );
#else
	FILE* f = fopen( path, "rb" );
#endif
	if( !f )
	{
		return { nullptr, 0 };
	}
	fseek( f, 0, SEEK_END );
	const size_t s = ftell( f );
	fseek( f, 0, SEEK_SET );
	std::unique_ptr<uint8_t[]> data( new uint8_t[s] );
	if( fread( data.get(), 1, s, f ) != s )
	{
		fclose( f );
		return { nullptr, 0 };
	}
	fclose( f );
	return { std::move( data ), s };
}

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
		auto configFile = LoadFile( cliArgs.configPath.c_str() );
		if( !configFile.first )
		{
			throw std::runtime_error( "Failed to load config file: " + cliArgs.configPath );
		}
		try
		{
			options = nlohmann::json::parse( configFile.first.get(), configFile.first.get() + configFile.second );
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

		auto file = LoadFile( cliArgs.fbxPath.c_str() );
		if( !file.first )
		{
			throw std::runtime_error( "Failed to load file: " + cliArgs.fbxPath );
		}

		ufbx_error error;
		auto* scene = ufbx_load_memory( file.first.get(), file.second, nullptr, &error );
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

		const CoordinateSystem system( scene->settings.axes, float( scene->settings.unit_meters ) );

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
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "sourceHash" ), allocator.AllocateString( MD5()( file.first.get(), file.second ) ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generator" ), allocator.AllocateString( "cmfprocessor" ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generatorVersion" ), allocator.AllocateString( CMF_PROCESSOR_VERSION ) } );
		cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "options" ), allocator.AllocateString( json( options ).dump() ) } );

		auto fileData = cmf::BuildFile( data, bufferAllocator, &metadata );

		auto validated = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
		if( !validated )
		{
			throw std::runtime_error( "Generated CMF file is invalid: " + validated.error );
		}

		ufbx_free_scene( scene );
		scene = nullptr;

#if _WIN32
		FILE* outFile = nullptr;
		fopen_s( &outFile, cliArgs.cmfPath.c_str(), "wb" );
#else
		FILE* outFile = fopen( cliArgs.cmfPath.c_str(), "wb" );
#endif
		if( !outFile )
		{
			throw std::runtime_error( "Failed to open output file: " + cliArgs.cmfPath );
		}
		if( fwrite( fileData.data(), 1, fileData.size(), outFile ) != fileData.size() )
		{
			fclose( outFile );
			throw std::runtime_error( "Failed to write output file: " + cliArgs.cmfPath );
		}
		fclose( outFile );
	} );
}

}

REGISTER_COMMAND( "fbximport", "Converts an FBX file into CMF", &ImportFBX );
