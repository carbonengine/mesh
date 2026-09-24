// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"

#include <nlohmann/json.hpp>

extern const char* CMF_PROCESSOR_VERSION;

namespace
{
struct NoOptions
{
};

void VersionInfo( CLI::App& app, NoOptions& noOptions )
{
	app.final_callback( []() {
		auto j = nlohmann::json{ { "format", cmf::FILE_VERSION }, { "cmfprocessor", CMF_PROCESSOR_VERSION } };
		printf( "%s\n", j.dump( 4 ).c_str() ); // NOLINT(cppcoreguidelines-pro-type-vararg)
	} );
}
}

REGISTER_COMMAND( "versioninfo", "Print out CMF version information (file format version and cmfprocessor version) as a JSON object", &VersionInfo );
