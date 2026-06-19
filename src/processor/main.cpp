// Copyright © 2026 CCP ehf.

#include <CLI/CLI.hpp>
#include "commands.h"
#include <nlohmann/json.hpp>
#include "cmffile.h"

const char* CMF_PROCESSOR_VERSION = "1.0.0";


int main( int argc, char** argv )
{
	CLI::App app{ "CMF Processor" };
	argv = app.ensure_utf8( argv );
	app.set_version_flag( "--version,-v", std::string( "cmfprocessor version " ) + CMF_PROCESSOR_VERSION );
	app.require_subcommand( 1, -1 );

	try
	{
		auto modification = nlohmann::json{
			{ "generator", "cmfprocessor" },
			{ "generatorVersion", CMF_PROCESSOR_VERSION },
			{ "commandLine", std::vector<std::string>( argv + 1, argv + argc ) }
		};
		CmfFile::SetModificationRecord( modification.dump() );

		for( auto& command : GetCommands() )
		{
			auto* cmd = app.add_subcommand( command->m_name, command->m_description );
			command->DescribeCLI( *cmd );
		}

		CLI11_PARSE( app, argc, argv );
	}
	catch( const std::exception& e )
	{
		fprintf( stderr, "%s\n", e.what() ); // NOLINT(cppcoreguidelines-pro-type-vararg)
		return 1;
	}
	return 0;
}