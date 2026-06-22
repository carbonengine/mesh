// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"


namespace
{
void ValidateFile( CLI::App& app, std::string& path )
{
	app.add_option( "file", path, "Path to CMF file to validate" )->required()->check( CLI::ExistingFile );

	app.final_callback( [&path]() {
		// Simply try loading a file: it will throw an exception if the file is not valid, and succeed silently if it is valid
		CmfFile{ path };
	} );
}
}

REGISTER_COMMAND( "validate", "Validates a CMF file", &ValidateFile );
