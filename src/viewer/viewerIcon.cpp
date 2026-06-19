// Copyright © 2026 CCP ehf.

#include "viewerIcon.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "CarbonMeshViewerResource.h"
#endif

namespace
{
#ifdef _WIN32
int GetIconResourceForSize( int width, int height )
{
	const int requestedSize = width > height ? width : height;
	if( requestedSize <= 16 )
	{
		return IDI_CARBON_MESH_VIEWER_16;
	}
	if( requestedSize <= 24 )
	{
		return IDI_CARBON_MESH_VIEWER_24;
	}
	if( requestedSize <= 32 )
	{
		return IDI_CARBON_MESH_VIEWER_32;
	}
	if( requestedSize <= 48 )
	{
		return IDI_CARBON_MESH_VIEWER_48;
	}
	if( requestedSize <= 64 )
	{
		return IDI_CARBON_MESH_VIEWER_64;
	}
	if( requestedSize <= 96 )
	{
		return IDI_CARBON_MESH_VIEWER_96;
	}
	if( requestedSize <= 128 )
	{
		return IDI_CARBON_MESH_VIEWER_128;
	}
	return IDI_CARBON_MESH_VIEWER_256;
}

HICON LoadViewerIcon( int width, int height )
{
	return static_cast<HICON>( LoadImageW(
		GetModuleHandleW( nullptr ),
		MAKEINTRESOURCEW( GetIconResourceForSize( width, height ) ),
		IMAGE_ICON,
		width,
		height,
		LR_DEFAULTCOLOR | LR_SHARED ) );
}

bool SetNativeWindowIcon( GLFWwindow* window )
{
	HWND hwnd = glfwGetWin32Window( window );
	if( !hwnd )
	{
		return false;
	}

	HICON smallIcon = LoadViewerIcon( GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ) );
	HICON largeIcon = LoadViewerIcon( GetSystemMetrics( SM_CXICON ), GetSystemMetrics( SM_CYICON ) );

	if( !smallIcon && !largeIcon )
	{
		return false;
	}

	if( smallIcon )
	{
		SendMessageW( hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( smallIcon ) );
		SendMessageW( hwnd, WM_SETICON, ICON_SMALL2, reinterpret_cast<LPARAM>( smallIcon ) );
		SetClassLongPtrW( hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>( smallIcon ) );
	}

	if( largeIcon )
	{
		SendMessageW( hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( largeIcon ) );
		SetClassLongPtrW( hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>( largeIcon ) );
	}

	return true;
}
#endif

}

void SetCarbonMeshViewerWindowIcon( GLFWwindow* window )
{
	if( !window )
	{
		return;
	}

#ifdef _WIN32
	SetNativeWindowIcon( window );
#else
	(void)window;
#endif
}
