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
	exitRequested.CallCallbacks( *this );

	modelState.selectedLod.CallCallbacks( *this );
	modelState.activeLod.CallCallbacks( *this );
	modelState.meshScreenSize.CallCallbacks( *this );
	modelState.visualizationShader.CallCallbacks( *this );
	modelState.availableShaders.CallCallbacks( *this );
	modelState.polygonMode.CallCallbacks( *this );
	modelState.meshVisibilityStates.CallCallbacks( *this );
	modelState.morphTargetWeight.CallCallbacks( *this );
	modelState.morphTargetEnabled.CallCallbacks( *this );
	modelState.meshWireframeOverlay.CallCallbacks( *this );
	modelState.audioOcclusionMesh.CallCallbacks( *this );
	modelState.meshBoundingBox.CallCallbacks( *this );
	modelState.modelBoundingBox.CallCallbacks( *this );
	modelState.currentAnimation.CallCallbacks( *this );
	modelState.currentAnimationTime.CallCallbacks( *this );
	modelState.availableShaders.CallCallbacks( *this );
	modelState.boneDebug.CallCallbacks( *this );
	modelState.jointDebug.CallCallbacks( *this );
	modelState.jointAxisDebug.CallCallbacks( *this );
	modelState.activeAnimationOwner.CallCallbacks( *this );
	modelState.selectedBones.CallCallbacks( *this );
}

void AppState::ResetModelState()
{
	modelState = {};
}
