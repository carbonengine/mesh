#pragma once
#include <Matrix.h>
#include <Sphere.h>


class Camera
{
public:
	Camera() = default;

	Matrix GetProjection();
	Matrix GetView();

	void SetFOV( float fov );

	void SetScreenSize( uint32_t width, uint32_t height );

	void LookAt( CcpMath::Sphere boundingSphere );

	void Orbit( Vector2 start, Vector2 end );
	void Zoom( float deltaZoom );

    void Update( float deltaTime );

private:
	float m_fov{ PI / 4.0f };
	CcpMath::Sphere m_boundingSphere{ { 0.0f, 0.0f, 0.0f }, 0.0f };
	Vector2 m_screenSize{ 0.0f, 0.0f };

    float m_zoom{ 0.0f };
	float m_zoomTarget{ 0.0f };
	float m_closestZoom{ 0.0f };

    float m_orbitRadius{ 0.0f };

    Vector3 m_eye{ 0.0f, 0.0f, 0.0f };
	Vector3 m_at{ 0.0f, 0.0f, 0.0f };
    Vector3 m_up{ 0.0f, 1.0f ,0.0f };

    Quaternion m_currentRotation{ IdentityQuaternion() };

    float m_yaw{ 0.0f };
	float m_pitch{ 0.0f };
    bool m_flipped{ false };
};