#include "appState.h"

bool MouseState::operator==( const MouseState& other ) const
{
	return position == other.position &&
		previousPosition == other.previousPosition &&
		wheelDelta == other.wheelDelta &&
		leftButton == other.leftButton &&
		middleButton == other.middleButton &&
		rightButton == other.rightButton;
}

void AppState::CallStateCallbacks()
{
	windowSize.CallCallbacks( *this );
	mouseState.CallCallbacks( *this );
	cameraTrigger.CallCallbacks( *this );

	cmfContent.CallCallbacks( *this );
	cmfPath.CallCallbacks( *this );

	selectedLod.CallCallbacks( *this );
	selectedMesh.CallCallbacks( *this );
	visualizationShader.CallCallbacks( *this );
	polygonMode.CallCallbacks( *this );
}
