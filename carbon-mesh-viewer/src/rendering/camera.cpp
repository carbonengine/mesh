#include "camera.h"

Matrix Camera::GetProjection()
{
	float distToModel = Length( m_at - m_boundingSphere.center );
	return PerspectiveFovMatrix(
		m_fov,
		m_screenSize.x / m_screenSize.y,
		std::max( 0.1f, m_zoom - distToModel - m_boundingSphere.radius ),
		distToModel + m_zoom + m_boundingSphere.radius );
}

Matrix Camera::GetView()
{
	return TranslationMatrix( m_at ) * RotationMatrix( m_currentRotation ) * TranslationMatrix( Vector3( 0.0, 0.0, -m_zoom ) );
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
	m_currentRotation = RotationQuaternion( 0.0f, 0.0f, 0.0f );
	m_boundingSphere = boundingSphere;
	m_zoom = boundingSphere.radius * 2.0f;
	m_targetZoom = m_zoom;
	m_at = boundingSphere.center;
	m_closestZoom = boundingSphere.radius / 100.0f;
	m_targetRotation = m_currentRotation;
	m_targetAt = m_at;
}

Vector3 Camera::CalcEye()
{
	return m_at + TransformCoord( Vector3( 0.0, 0.0, m_zoom ), RotationMatrix( m_currentRotation ) );
}

static Quaternion NdcToArc( const Vector2& ndc )
{
	Vector3 ndcAsVec3( ndc.x, ndc.y, 0.0f );
	const float length = Dot( ndcAsVec3, ndcAsVec3 );

	if( length <= 1.0f )
	{
		// point is on the sphere
		return Quaternion( ndc.x, ndc.y, sqrt( 1.0f - length ), 0.0f );
	}
	else
	{
		// point is outside the sphere, project onto sphere
		return Normalize( Quaternion( ndc.x, ndc.y, 0.0f, 0.0f ) );
	}
}

static Vector2 ScreenToNdc( const Vector2& point, const Vector2& screenSize )
{
	return Vector2( 1.0, -1.0 ) * ( 2.0f * point / screenSize - Vector2( 1.0, 1.0 ) );
}

void Camera::Orbit( Vector2 currentPos, Vector2 previousPos )
{
	if( currentPos == previousPos )
	{
		return;
	}

	const Vector2 currentNdcPos = ScreenToNdc( currentPos, m_screenSize );
	const Vector2 previousNdcPos = ScreenToNdc( previousPos, m_screenSize );

	const Quaternion currentRotation = NdcToArc( currentNdcPos );
	const Quaternion previousRotation = NdcToArc( previousNdcPos );
	m_targetRotation *= Normalize( previousRotation * currentRotation );
}

void Camera::Zoom( float zoomAmount )
{
	float zoomStepSize = m_zoom / 10.0f;
	m_targetZoom = std::max( m_targetZoom + zoomAmount * zoomStepSize, m_closestZoom );
}

void Camera::Pan( Vector2 percentageChange )
{
	Vector3 right = TransformCoord( Vector3( 1.0f, 0.0f, 0.0f ), RotationMatrix( Inverse( m_currentRotation ) ) );
	Vector3 up = TransformCoord( Vector3( 0.0f, 1.0f, 0.0f ), RotationMatrix( Inverse( m_currentRotation ) ) );
	float panSpeed = m_zoom * 0.5f;
	m_targetAt += ( right * percentageChange.x - up * percentageChange.y ) * panSpeed;
}

void Camera::Update( float deltaTime )
{
	// Smoothly interpolate to the target zoom level
	m_zoom = m_zoom + ( m_targetZoom - m_zoom ) * std::min( deltaTime * 6.5f, 1.0f );
	m_at = m_at + ( m_targetAt - m_at ) * std::min( deltaTime * 6.5f, 1.0f );
	m_currentRotation = Slerp( m_currentRotation, m_targetRotation, std::min( deltaTime * 6.5f, 1.0f ) );
}