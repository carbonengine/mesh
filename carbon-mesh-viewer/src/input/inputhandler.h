#pragma once

class InputHandler
{
public:
    InputHandler();
    ~InputHandler();

    void KeyCallback( int key, int scancode, int action, int mods );
	void MouseCallback( double x, double y );
	void MouseButtonCallback( int button, int action, int mods );
};
