#include "camera.h"

Matrix Camera::GetProjection()
{
	return PerspectiveFovMatrix(
		m_fov,
		m_screenSize.x / m_screenSize.y,
		std::max( 0.1f, m_zoom - m_boundingSphere.radius ),
		m_zoom + m_boundingSphere.radius );
}

Matrix Camera::GetView()
{
	return LookAtMatrix( m_eye, m_at, m_up );
}

void Camera::SetFOV( float fov )
{
	m_fov = fov;
}

void Camera::SetScreenSize( uint32_t width, uint32_t height )
{
	m_screenSize = Vector2( (float)width, (float)height );
}

void Camera::LookAt( CcpMath::Sphere boundingSphere )
{
	m_currentRotation = IdentityQuaternion();
	m_pitch = 0.0f;
	m_yaw = PI / 2.0f;
	m_boundingSphere = boundingSphere;
	m_zoom = boundingSphere.radius * 2.0f;
	m_orbitRadius = boundingSphere.radius * 2.0f;
	m_zoomTarget = m_zoom;
	m_at = boundingSphere.center;
	m_closestZoom = boundingSphere.radius / 100.0f;
}

// Helper: Map screen coordinates to arcball sphere
static Vector3 ArcballMap( const Vector2& point, const Vector2& screenSize )
{
	float x = ( 2.0f * point.x - screenSize.x ) / screenSize.x;
	float y = ( screenSize.y - 2.0f * point.y ) / screenSize.y;
	float lengthSq = x * x + y * y;

	if( lengthSq > 1.0f )
	{
		// Outside the sphere, project onto the sphere's surface
		float length = sqrtf( lengthSq );
		x /= length;
		y /= length;
		return Vector3( x, y, 0.0f );
	}
	else
	{
		// Inside the sphere
		float z = sqrtf( 1.0f - lengthSq );
		return Vector3( x, y, z );
	}
}

void Camera::Orbit( Vector2 start, Vector2 end )
{
	if( m_screenSize.x <= 0.0f || m_screenSize.y <= 0.0f )
		return;

    float yawDelta = -( end.x - start.x ) / m_screenSize.x * 2.0f * PI;
	float pitchDelta = ( end.y - start.y ) / m_screenSize.y * PI;

	m_yaw += yawDelta;
	m_pitch += pitchDelta;

	// Clamp pitch to avoid flipping
	const float limit = PI / 2.0f - 0.01f;
	if( m_pitch > limit )
		m_pitch = limit;
	if( m_pitch < -limit )
		m_pitch = -limit;
}

void Camera::Zoom( float zoomAmount )
{
	float zoomStepSize = m_zoom / 10.0f;
	m_zoomTarget = std::max( m_zoomTarget + zoomAmount * zoomStepSize, m_closestZoom );
}

void Camera::Update( float deltaTime )
{
	// Smoothly interpolate to the target zoom level
	m_zoom = m_zoom + ( m_zoomTarget - m_zoom ) * std::min( deltaTime * 5.0f, 1.0f );

	// Spherical coordinates
	float x = m_zoom * cosf( m_pitch ) * sinf( m_yaw );
	float y = m_zoom * sinf( m_pitch );
	float z = m_zoom * cosf( m_pitch ) * cosf( m_yaw );

	m_eye = m_at + Vector3( x, y, z );

	// Always use world up for the up vector
	m_up = Vector3( 0, 1, 0 );
}