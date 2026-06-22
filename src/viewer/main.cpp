// Copyright © 2025 CCP ehf.

#include <iostream>

#include "application.h"

void SetVulkanEnvironmentVariables()
{
#ifdef APPLE
	setenv( "VK_DRIVER_FILES", "MoltenVK_icd.json", 0 );
	setenv( "VULKAN_LAYER_PATH", "explicit_layer.d", 0 );

	Log::Info( "VK_DRIVER_FILES points is %s", getenv( "VK_DRIVER_FILES" ) );
	Log::Info( "VULKAN_LAYER_PATH points is %s", getenv( "VULKAN_LAYER_PATH" ) );
#endif
}

int main( int argc, char** argv )
{
	SetVulkanEnvironmentVariables();
	Application app;

	app.Initialize();
	if( argc > 1 )
	{
		app.LoadCmfFile( argv[1] );
	}
	app.Run();

	return EXIT_SUCCESS;
}
