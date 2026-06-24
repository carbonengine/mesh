// Copyright © 2026 CCP ehf.

#include "cmffile.h"
#include "commands.h"

#include <map>

namespace
{
struct SetMetadataOptions
{
	std::map<std::string, std::string> set;
	std::vector<std::string> unset;
	std::string file;
};

cmf::MetadataEntry* FindMetadataEntry( cmf::Metadata& metadata, const std::string& key )
{
	return std::find_if( metadata.entries.begin(), metadata.entries.end(), [&]( const cmf::MetadataEntry& entry ) { return cmf::ToStdString( entry.key ) == key; } );
}

void SetMetadata( CLI::App& app, SetMetadataOptions& options )
{
	app.add_option( "--set", options.set, "Set metadata key value pairs, (e.g., '--set myKey myValue'); may specify multiple pairs to set multiple keys" )->expected( 0, -1 );
	app.add_option( "--unset", options.unset, "Remove metadata keys; may specify multiple to remove multiple keys" )->expected( 0, -1 );
	app.add_option( "file", options.file, "Path to CMF file" )->required()->check( CLI::ExistingFile );

	app.final_callback( [&options]() {
		CmfFile file( options.file );
		for( auto& [key, value] : options.set )
		{
			auto* found = FindMetadataEntry( file.GetMetadata(), key );
			if( found != file.GetMetadata().entries.end() )
			{
				found->value = file.GetAllocator().AllocateString( value );
			}
			else
			{
				cmf::Modify( file.GetMetadata().entries, file.GetAllocator() ).emplace_back( cmf::MetadataEntry{ file.GetAllocator().AllocateString( key ), file.GetAllocator().AllocateString( value ) } );
			}
		}
		for( auto& key : options.unset )
		{
			auto* found = FindMetadataEntry( file.GetMetadata(), key );
			if( found != file.GetMetadata().entries.end() )
			{
				cmf::Modify( file.GetMetadata().entries, file.GetAllocator() ).erase( found );
			}
		}
		file.Save( options.file );
	} );
}
}

REGISTER_COMMAND(
	"metadata",
	"Modify metadata in a CMF file. Add/remove/change metadata entries in the file in-place.",
	&SetMetadata );
