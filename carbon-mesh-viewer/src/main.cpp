
#include <iostream>
#include "application.h"

extern const char* g_moduleName = "CarbonMeshViewer";

void LogToStdOut( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
{
	std::cout << logObject.facility << "." << logObject.object << " logged a message with severity " << type << ": " << message << std::endl;
}

int main()
{
	// let the logging framework know about the main thread ID
	CCP::SetLogMainThreadId();

	// log all messages of type info or higher to stdout
#ifdef DEBUG_MODE
	CCP::RegisterLogEcho( LogToStdOut, CCP::LOGTYPE_LOWEST, true );
#else
	CCP::RegisterLogEcho( LogToStdOut, CCP::LOGTYPE_WARN, true );
#endif
	Application app;

	app.Initialize();
	app.Run();

	return EXIT_SUCCESS;
}