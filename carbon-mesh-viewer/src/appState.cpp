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
	windowSize.CallCallbacks();
	mouseState.CallCallbacks();
	cameraTrigger.CallCallbacks();

	cmfContent.CallCallbacks();
	cmfPath.CallCallbacks();

	selectedLod.CallCallbacks();
	selectedMesh.CallCallbacks();
	visualizationShader.CallCallbacks();
	polygonMode.CallCallbacks();
}
