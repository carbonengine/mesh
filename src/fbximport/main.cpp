#include <memory>
#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include "cmf/cmf.h"
#include "cmf/utils.h"
#include "cmf/memallocator.h"
#include "cmf/writer.h"
#include "options.h"
#include <md5.h>
#include "ufbx.h"
#include "mesh.h"
#include "skeleton.h"
#include "animation.h"
#include "transform.h"

const char* FBX_IMPORT_VERSION = "1.0.0";

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
	size_t s = ftell( f );
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

int main( int argc, char** argv )
{
	CLI::App app{ "FBX to CMF Converter" };
	argv = app.ensure_utf8( argv );
	app.set_version_flag( "--version,-v", std::string( "fbximport version " ) + FBX_IMPORT_VERSION );

	std::string fbxPath;
	std::string cmfPath;
	std::string configPath;
	std::string metadataPath;
	NamedFilter meshFilter;
	NamedFilter skeletonFilter;
	NamedFilter animationFilter;
	app.add_option( "--config", configPath, "Path to a JSON config file that specifies import options" )->check( CLI::ExistingFile );
	app.add_option( "--mesh", meshFilter.m_names, "Name of a mesh to import; may specify multiple; if not specified, import all meshes; overrides filter in JSON file" );
	app.add_option( "--skeleton", skeletonFilter.m_names, "Name of a skeleton to import; may specify multiple; if not specified, import all skeletons; overrides filter in JSON file" );
	app.add_option( "--animation", animationFilter.m_names, "Name of an animation to import; may specify multiple; if not specified, import all animations; overrides filter in JSON file" );
	app.add_option( "--path", metadataPath, "Overrides source path stored in metadata. Uses relative path to source from destination if not present." );
	app.add_option( "source", fbxPath, "Path to source FBX file" )->required()->check( CLI::ExistingFile );
	app.add_option( "destination", cmfPath, "Path to destination CMF file" )->required()->option_text( "TEXT:FILE" );

	CLI11_PARSE( app, argc, argv );

	ImportOptions options;
	if( !configPath.empty() )
	{
		auto configFile = LoadFile( configPath.c_str() );
		if( !configFile.first )
		{
			fprintf( stderr, "Failed to load config file: %s\n", configPath.c_str() );
			return 1;
		}
		try
		{
			options = nlohmann::json::parse( configFile.first.get(), configFile.first.get() + configFile.second );
		}
		catch( const std::exception& e )
		{
			fprintf( stderr, "Failed to parse config file: %s\n", e.what() );
			return 1;
		}
	}
	if( !meshFilter.m_names.empty() )
	{
		options.meshOptions.namedFilter = meshFilter;
	}
	if( !skeletonFilter.m_names.empty() )
	{
		options.skeletonOptions.namedFilter = skeletonFilter;
	}
	if( !animationFilter.m_names.empty() )
	{
		options.animationOptions.namedFilter = animationFilter;
	}
	try
	{
		ValidateOptions( options );
	}
	catch( const std::exception& e )
	{
		fprintf( stderr, "Invalid options: %s\n", e.what() );
		return 1;
	}

	auto file = LoadFile( fbxPath.c_str() );
	if( !file.first )
	{
		fprintf( stderr, "Failed to load file: %s\n", fbxPath.c_str() );
		return 1;
	}

	ufbx_error error;
	auto scene = ufbx_load_memory( file.first.get(), file.second, nullptr, NULL );
	if( !scene )
	{
		char buf[4096];
		ufbx_format_error( buf, sizeof( buf ), &error );

		fprintf( stderr, "Failed to parse FBX file %s: %s\n", fbxPath.c_str(), buf );
		return 1;
	}

	auto data = cmf::Data{};
	cmf::MemoryAllocator allocator;
	cmf::BufferManager bufferAllocator( allocator );

	CoordinateSystem system( scene->settings.axes, float( scene->settings.unit_meters ) );

	auto [skeletons, boneMap] = ImportSkeletons( *scene, options.skeletonOptions, allocator, system );
	data.skeletons = skeletons;

	data.meshes = ImportMeshes( *scene, options.meshOptions, boneMap, allocator, bufferAllocator, system );

	if( options.animationOptions.importAnimations )
	{
		auto [anims, curves] = ImportAnimations( *scene, boneMap, options.animationOptions, allocator, system );
		data.animations = anims;
		data.curves = curves;
	}

	cmf::Metadata metadata;
	if( metadataPath.empty() )
	{
		metadataPath = std::filesystem::proximate( fbxPath, std::filesystem::path( cmfPath ).remove_filename() ).string();
	}
	cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "source" ), allocator.AllocateString( metadataPath ) } );
	cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "sourceHash" ), allocator.AllocateString( MD5()( file.first.get(), file.second ) ) } );
	cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generator" ), allocator.AllocateString( "fbximporter" ) } );
	cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "generatorVersion" ), allocator.AllocateString( FBX_IMPORT_VERSION ) } );
	cmf::Modify( metadata.entries, allocator ).emplace_back( cmf::MetadataEntry{ allocator.AllocateString( "options" ), allocator.AllocateString( json( options ).dump() ) } );

	auto fileData = cmf::BuildFile( data, bufferAllocator, &metadata );

	auto validated = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
	if( !validated.first )
	{
		fprintf( stderr, "Internal error: generated CMF file is invalid" );
		return 1;
	}

	ufbx_free_scene( scene );
	scene = nullptr;

#if _WIN32
	FILE* outFile = nullptr;
	fopen_s( &outFile, cmfPath.c_str(), "wb" );
#else
	FILE* outFile = fopen( cmfPath.c_str(), "wb" );
#endif
	if( !outFile )
	{
		fprintf( stderr, "Failed to open output file: %s\n", cmfPath.c_str() );
		return 1;
	}
	if( fwrite( fileData.data(), 1, fileData.size(), outFile ) != fileData.size() )
	{
		fprintf( stderr, "Failed to write output file: %s\n", cmfPath.c_str() );
		fclose( outFile );
		return 1;
	}
	fclose( outFile );

	return 0;
}