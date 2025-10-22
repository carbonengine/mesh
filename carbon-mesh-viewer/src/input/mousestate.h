#pragma once
#include <vector>
#include <Vector2.h>    
#include <map>

enum MouseButton
{
    LEFT = 0,
    RIGHT = 1,
    MIDDLE = 2
};

class MouseState
{
public:
	MouseState() = default;
	
	Vector2 GetPos() const;
	Vector2 GetLastPos() const;

    Vector2 GetPosChangePercentage() const;

	Vector2 GetScroll() const;
	bool IsButtonPressed( MouseButton button ) const;

    void UpdatePosition( float x, float y);
	void UpdateScroll( float x, float y );
	void UpdateScreenSize( float x, float y );
	void PressButton( MouseButton button );
	void ReleaseButton( MouseButton button );
	Vector2 GetButtonPressPos( MouseButton button ) const;

    void Clean();

private:
	Vector2 m_pos;
    Vector2 m_lastPos;
	Vector2 m_scroll;
	std::map<MouseButton, Vector2> m_buttonsPressed;
    Vector2 m_screenSize;
};
