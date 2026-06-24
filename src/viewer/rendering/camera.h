// Copyright © 2025 CCP ehf.

#pragma once

#include <Matrix.h>
#include <Sphere.h>

#include "appState.h"

class Camera
{
public:
	Camera() = default;

	void Initialize( AppState& state );

	void HandleCameraTriggerChange( CameraTrigger& trigger );

	void HandleMouseStateChanged( MouseState& newMouseState );

	Matrix GetProjection() const;
	Matrix GetView() const;
	Matrix GetRotation() const;

	void SetScreenSize( uint32_t width, uint32_t height );

	void Reset( CcpMath::Sphere boundingSphere );
	void LookAt( Vector3 target, bool immediate = false );

	void Orbit( Vector2 currentPos, Vector2 previousPos );
	void Zoom( float deltaZoom );
	void Pan( Vector2 percentageChange );

	void Update( float deltaTime );
	float GetSizeOnScreen( const CcpMath::Sphere& sphere ) const;

private:
	float m_fov{ PI / 4.0f };
	CcpMath::Sphere m_boundingSphere{ { 0.0f, 0.0f, 0.0f }, 0.0f };
	Vector2 m_screenSize{ 0.0f, 0.0f };

	Vector3 m_at{ 0.0, 0.0, 0.0 };
	Vector3 m_eye{ 0.0, 0.0, 0.0 };
	Vector3 m_targetAt{ 0.0, 0.0, 0.0 };

	float m_zoom{ 0.0f };
	float m_targetZoom{ 0.0f };
	float m_closestZoom{ 0.0f };

	Quaternion m_targetRotation{ IdentityQuaternion() };
	Quaternion m_currentRotation{ IdentityQuaternion() };
};