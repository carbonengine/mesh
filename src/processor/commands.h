// Copyright © 2026 CCP ehf.

#pragma once

#include <CLI/CLI.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <memory>


/** Base class for command handlers */
class CliCommand
{
public:
	virtual ~CliCommand() = default;
	virtual void DescribeCLI( CLI::App& app ) = 0;

	std::string m_name;
	std::string m_description;
};

/** Template class for command handlers with specific options */
template <typename T>
class CliCommandImpl : public CliCommand
{
public:
	using DescribeCLIFunc = void ( * )( CLI::App& app, T& options );

	void DescribeCLI( CLI::App& app ) override
	{
		m_describeCLI( app, m_options );
	}

	DescribeCLIFunc m_describeCLI;
	T m_options;
};

/** Global list of registered commands */
std::vector<std::unique_ptr<CliCommand>>& GetCommands();

/** Helper struct to register commands at static initialization time */
template <typename T>
struct RegisterCommandImpl
{
	RegisterCommandImpl( std::string_view name, std::string_view description, void ( *describeCLI )( CLI::App& app, T& options ) )
	{
		auto cmd = std::make_unique<CliCommandImpl<T>>();
		cmd->m_name = std::string( name );
		cmd->m_description = std::string( description );
		cmd->m_describeCLI = describeCLI;

		GetCommands().emplace_back( std::unique_ptr<CliCommand>( std::move( cmd ) ) );
	}
};

/** Helper function to create a RegisterCommandImpl instance (to help with deriving template type) */
template <typename T>
inline RegisterCommandImpl<T> AutoRegisterCommand( std::string_view name, std::string_view description, void ( *describeCLI )( CLI::App& app, T& options ) )
{
	return RegisterCommandImpl<T>( name, description, describeCLI );
}


#define CONCAT_IMPL( x, y ) x##y
#define MACRO_CONCAT( x, y ) CONCAT_IMPL( x, y )

/** @brief Registers a command handler.
* @param name The name of the command (e.g. "extract") - used as a subcommand in the CLI
* @param description A short description of the command (e.g. "Extracts files from a CMF")
* @param describeCLI A function that takes a CLI::App and a reference to an options struct, and sets up the CLI::App with the appropriate options for the command
*/
#define REGISTER_COMMAND( name, description, describeCLI )                                                                  \
	namespace                                                                                                               \
	{                                                                                                                       \
	const auto MACRO_CONCAT( g_register, __COUNTER__ ) = AutoRegisterCommand( ( name ), ( description ), ( describeCLI ) ); \
	}
