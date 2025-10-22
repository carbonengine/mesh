#include "mousestate.h"

Vector2 MouseState::GetPos() const
{
	return m_pos;
}

Vector2 MouseState::GetLastPos() const
{
	return m_lastPos;
}

Vector2 MouseState::GetScroll() const
{
	return m_scroll;
}

bool MouseState::IsButtonPressed( MouseButton button ) const
{
	auto it = m_buttonsPressed.find( button );
	return it != m_buttonsPressed.end();
}

Vector2 MouseState::GetButtonPressPos( MouseButton button ) const
{
	auto it = m_buttonsPressed.find( button );
	if( it != m_buttonsPressed.end() )
	{
		return it->second;
	}
	return Vector2( FLT_MAX, FLT_MAX );
}

void MouseState::UpdatePosition( float x, float y )
{
	m_lastPos = m_pos;
	m_pos.x = x;
	m_pos.y = y;
}

void MouseState::UpdateScroll( float x, float y )
{
	m_scroll.x = x;
	m_scroll.y = y;
}

void MouseState::PressButton( MouseButton button )
{
	m_buttonsPressed[button] = m_pos;
}

void MouseState::ReleaseButton( MouseButton button )
{
	m_buttonsPressed.erase( button );
}

void MouseState::Clean()
{
	m_scroll *= 0.0f;
	m_lastPos = m_pos;
}

void MouseState::UpdateScreenSize( uint32_t x, uint32_t y )
{
	m_screenSize.x = (float)x;
	m_screenSize.y = (float)y;
}

Vector2 MouseState::GetPosChangePercentage() const
{
	return ( m_pos - m_lastPos ) / m_screenSize;
}
