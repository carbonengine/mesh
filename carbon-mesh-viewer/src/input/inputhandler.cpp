#include "inputhandler.h"

InputHandler::InputHandler( )
{
}

InputHandler::~InputHandler()
{
}   

void InputHandler::KeyCallback( int key, int scancode, int action, int mods )
{
	//CCP_LOGNOTICE( "Key: %d, Scancode: %d, Action: %d, Mods: %d", key, scancode, action, mods );
}

void InputHandler::MouseCallback( double x, double y )
{
	//CCP_LOGNOTICE( "Mouse Position: (%f, %f)", x, y );
}

void InputHandler::MouseButtonCallback( int button, int action, int mods )
{
	//CCP_LOGNOTICE( "Mouse Button: %d, Action: %d, Mods: %d", button, action, mods );
}
