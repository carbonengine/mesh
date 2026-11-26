// Copyright © 2026 CCP ehf.

#include "application.h"

#include "log.h"

#include <cstdlib>
#include <exception>

namespace
{

void SetVulkanEnvironmentVariables()
{
#ifdef APPLE
	setenv( "VK_DRIVER_FILES", "MoltenVK_icd.json", 0 );
	setenv( "VULKAN_LAYER_PATH", "explicit_layer.d", 0 );
#endif
}

}

int main( int argc, char** argv )
{
	try
	{
		SetVulkanEnvironmentVariables();
		canedit::Application application;
		if( !application.Initialize() )
		{
			return EXIT_FAILURE;
		}
		if( argc > 1 )
		{
			application.OpenFile( argv[1] ); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): command line arguments
		}
		application.Run();
		return EXIT_SUCCESS;
	}
	catch( const std::exception& e )
	{
		Log::Error( "Unhandled exception: %s", e.what() );
	}
	catch( ... )
	{
		Log::Error( "Unhandled unknown exception" );
	}
	return EXIT_FAILURE;
}
