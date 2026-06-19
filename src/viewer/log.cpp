// Copyright © 2026 CCP ehf.

#include "log.h"

#include <ctime>
#include <stdarg.h>
#include <stdio.h>

namespace Log
{

void Error( const char* message, ... )
{
	printf( "[Error] " );
	va_list args;
	va_start( args, message );
	vprintf( message, args );
	va_end( args );
	printf( "\n" );
}


void Warning( const char* message, ... )
{
	printf( "[Warning] " );
	va_list args;
	va_start( args, message );
	vprintf( message, args );
	va_end( args );
	printf( "\n" );
}

void Info( const char* message, ... )
{
	printf( "[Info] " );
	va_list args;
	va_start( args, message );
	vprintf( message, args );
	va_end( args );
	printf( "\n" );
}

void Debug( const char* message, ... )
{
#ifdef DEBUG_MODE
	printf( "[Debug] " );
	va_list args;
	va_start( args, message );
	vprintf( message, args );
	va_end( args );
	printf( "\n" );
#endif
}

}
