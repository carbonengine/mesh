// Copyright © 2025 CCP ehf.

#include "camera.h"

const float DEFAULT_ZOOM_MULTIPLIER = 2.5f;

void Camera::Initialize( AppState& state )
{
	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> newSize, AppState& ) {
		auto [width, height] = newSize;
		SetScreenSize( width, height );
	} );

	state.cameraTrigger.RegisterCallback( [this]( CameraTrigger trigger, AppState& ) {
		HandleCameraTriggerChange( trigger );
	} );

	state.mouseState.RegisterCallback( [this]( MouseState newMouseState, AppState& ) {
		HandleMouseStateChanged( newMouseState );
	} );

	state.cmfContent.RegisterCallback( [this]( std::shared_ptr<CmfContent> cmfContent, AppState& ) {
		if( cmfContent != nullptr )
		{
			auto boundingSphere = cmfContent->GetBoundingSphere();
			this->Reset( boundingSphere );
		}
	} );
	auto [width, height] = state.windowSize.GetValue();
	SetScreenSize( width, height );
}

void Camera::HandleCameraTriggerChange( CameraTrigger& trigger )
{
	if( trigger == CameraTrigger::CAMERA_TRIGGER_NONE )
	{
		return;
	}
	switch( trigger )
	{
	case CameraTrigger::CAMERA_TRIGGER_FOCUS:
		this->LookAt( this->m_boundingSphere.center );
		m_targetZoom = this->m_boundingSphere.radius * DEFAULT_ZOOM_MULTIPLIER;
		break;
	case CameraTrigger::CAMERA_TRIGGER_LOOK_UP:
		this->m_targetRotation = RotationQuaternion( 0.0f, -PI / 2.0f, 0.0f );
		break;
	case CameraTrigger::CAMERA_TRIGGER_LOOK_DOWN:
		this->m_targetRotation = RotationQuaternion( 0.0f, PI / 2.0f, 0.0f );
		break;
	case CameraTrigger::CAMERA_TRIGGER_LOOK_RIGHT:
		this->m_targetRotation = RotationQuaternion( -PI / 2.0f, 0.0f, 0.0f );
		break;
	case CameraTrigger::CAMERA_TRIGGER_LOOK_LEFT:
		this->m_targetRotation = RotationQuaternion( PI / 2.0f, 0.0f, 0.0f );
		break;
	case CameraTrigger::CAMERA_TRIGGER_LOOK_FRONT:
		this->m_targetRotation = RotationQuaternion( PI, 0.0f, 0.0f );
		break;
	case CameraTrigger::CAMERA_TRIGGER_LOOK_BACK:
		this->m_targetRotation = IdentityQuaternion();
		break;
	default:
		break;
	}
}

void Camera::HandleMouseStateChanged( MouseState& mouseState )
{
	if( mouseState.leftButton )
	{
		auto mousePos = mouseState.position;
		auto lastMousePos = mouseState.previousPosition;
		this->Orbit( mousePos, lastMousePos );
	}
	else if( mouseState.middleButton )
	{
		auto mousePercentageChange = ( mouseState.position - mouseState.previousPosition ) / m_screenSize;
		auto absChange = Vector2( abs( mousePercentageChange.x ), abs( mousePercentageChange.y ) );

		float zoom = mousePercentageChange.x;

		if( absChange.y > absChange.x )
		{
			zoom = mousePercentageChange.y;
		}

		this->Zoom( -zoom * 10.0f );
	}
	else
	{
		this->Zoom( mouseState.wheelDelta );
	}

	if( mouseState.rightButton )
	{
		auto mousePercentageChange = ( mouseState.position - mouseState.previousPosition ) / m_screenSize;
		this->Pan( mousePercentageChange );
	}
}

Matrix Camera::GetProjection() const
{
	const float ABSOLUTE_MIN_NEAR_PLANE = 0.01f;
	const float boundingDepth = m_boundingSphere.radius * 2.0f;
	const float centerDepth = -TransformCoord( m_boundingSphere.center, GetView() ).z;
	const float nearPlane = std::max( ABSOLUTE_MIN_NEAR_PLANE, centerDepth - boundingDepth );
	const float farPlane = std::max( nearPlane + ABSOLUTE_MIN_NEAR_PLANE, centerDepth + boundingDepth );

	return PerspectiveFovMatrix(
		m_fov,
		m_screenSize.x / m_screenSize.y,
		nearPlane,
		farPlane );
}

Matrix Camera::GetRotation() const
{
	return RotationMatrix( m_currentRotation );
}

Matrix Camera::GetView() const
{
	return TranslationMatrix( -m_at ) * RotationMatrix( m_currentRotation ) * TranslationMatrix( Vector3( 0.0, 0.0, -m_zoom ) );
}

void Camera::SetScreenSize( uint32_t width, uint32_t height )
{
	m_screenSize = Vector2( (float)width, (float)height );
}

void Camera::Reset( CcpMath::Sphere boundingSphere )
{
	m_currentRotation = RotationQuaternion( 0.0f, 0.0f, 0.0f );
	m_boundingSphere = boundingSphere;
	m_zoom = boundingSphere.radius * DEFAULT_ZOOM_MULTIPLIER;
	m_targetZoom = m_zoom;
	m_closestZoom = boundingSphere.radius / 100.0f;
	m_targetRotation = m_currentRotation;
	LookAt( boundingSphere.center, true );
}

void Camera::LookAt( Vector3 target, bool immediate )
{
	m_targetAt = target;
	if( immediate )
	{
		m_at = target;
	}
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
	m_targetAt += ( -right * percentageChange.x + up * percentageChange.y ) * panSpeed;
}

void Camera::Update( float deltaTime )
{
	// Smoothly interpolate to the target zoom level
	m_zoom = m_zoom + ( m_targetZoom - m_zoom ) * std::min( deltaTime * 6.5f, 1.0f );
	m_at = m_at + ( m_targetAt - m_at ) * std::min( deltaTime * 6.5f, 1.0f );
	m_currentRotation = Slerp( m_currentRotation, m_targetRotation, std::min( deltaTime * 6.5f, 1.0f ) );
	m_eye = Inverse( GetView() ).GetTranslation();
}

float Camera::GetSizeOnScreen( const CcpMath::Sphere& sphere ) const
{
	const auto projection = GetProjection();

	Vector3 d( sphere.center - m_eye );

	float lengthSqrd = LengthSq( d );
	float radiusSqrd = sphere.radius * sphere.radius;
	if( lengthSqrd < radiusSqrd )
	{
		//The camera is inside the object, it essentially has infinite screen size.
		return std::numeric_limits<float>::max();
	}

	//adjusted distance based on the visible part of the sphere, that properly goes to infinity as you enter the sphere.
	float distance = sqrt( lengthSqrd - radiusSqrd );

	return sphere.radius / distance * projection._11 * m_screenSize.x;
}