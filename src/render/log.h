// Copyright © 2026 CCP ehf.

#pragma once

#include <cstdint>

namespace Log // NOLINT(readability-identifier-naming) established name used throughout the tools
{

enum class Level : uint8_t
{
	Error,
	Warning,
	Info,
	Debug
};

/// Writes a printf style formatted message with a level prefix. Debug messages are only written in debug builds.
void Write( Level level, const char* format, ... );

// The functions below are templates rather than C-style vararg functions so that calls to them are type checked
// by the compiler like any other call; Write is the single vararg sink.
// Function templates are always reported by readability-identifier-naming, hence the NOLINTs.

template <typename... Args>
void Error( const char* format, Args... args ) // NOLINT(readability-identifier-naming)
{
	Write( Level::Error, format, args... ); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

template <typename... Args>
void Warning( const char* format, Args... args ) // NOLINT(readability-identifier-naming)
{
	Write( Level::Warning, format, args... ); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

template <typename... Args>
void Info( const char* format, Args... args ) // NOLINT(readability-identifier-naming)
{
	Write( Level::Info, format, args... ); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

template <typename... Args>
void Debug( const char* format, Args... args ) // NOLINT(readability-identifier-naming)
{
	Write( Level::Debug, format, args... ); // NOLINT(cppcoreguidelines-pro-type-vararg)
}

}
