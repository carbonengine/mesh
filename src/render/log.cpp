// Copyright © 2026 CCP ehf.

#include "log.h"

#include <cstdarg>
#include <cstdio>

namespace Log // NOLINT(readability-identifier-naming) established name used throughout the tools
{

void Write( Level level, const char* format, ... ) // NOLINT(modernize-avoid-variadic-functions) the single printf style sink behind the typed functions in log.h
{
#ifndef DEBUG_MODE
	if( level == Level::Debug )
	{
		return;
	}
#endif
	const char* prefix = "[Debug] ";
	switch( level )
	{
	case Level::Error:
		prefix = "[Error] ";
		break;
	case Level::Warning:
		prefix = "[Warning] ";
		break;
	case Level::Info:
		prefix = "[Info] ";
		break;
	case Level::Debug:
		break;
	}
	printf( "%s", prefix ); // NOLINT(cppcoreguidelines-pro-type-vararg)
	va_list args; // NOLINT(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-init-variables) initialized by va_start
	va_start( args, format ); // NOLINT(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	vprintf( format, args ); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	va_end( args ); // NOLINT(cppcoreguidelines-pro-type-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	printf( "\n" ); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

}
