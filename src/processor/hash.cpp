// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"

#include <md5.h>


namespace
{
void PrintHash( CLI::App& app, std::string& path )
{
	app.add_option( "file", path, "Path to a file to hash" )->required()->check( CLI::ExistingFile );

	app.final_callback( [&path]() {
		const auto contents = ReadFile( path.c_str() );
		const auto hash = MD5()( contents.data(), contents.size() );
		printf( "%s\n", hash.c_str() ); // NOLINT(cppcoreguidelines-pro-type-vararg)
	} );
}
}

REGISTER_COMMAND( "hash", "Print out file hash (arbitrary file)", &PrintHash );
