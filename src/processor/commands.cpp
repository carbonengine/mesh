// Copyright © 2026 CCP ehf.

#include "commands.h"

std::vector<std::unique_ptr<CliCommand>>& GetCommands()
{
	static std::vector<std::unique_ptr<CliCommand>> commands;
	return commands;
}
