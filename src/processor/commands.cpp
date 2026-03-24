#include "commands.h"

std::vector<std::unique_ptr<CliCommand>>& GetCommands()
{
	static std::vector<std::unique_ptr<CliCommand>> commands;
	return commands;
}
