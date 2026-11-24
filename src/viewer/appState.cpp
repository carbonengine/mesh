// Copyright © 2026 CCP ehf.

#include "appState.h"

void AppState::CallStateCallbacks()
{
	windowSize.CallCallbacks( *this );
	mouseState.CallCallbacks( *this );
	cameraTrigger.CallCallbacks( *this );
	cameraFocus.CallCallbacks( *this );

	cmfContent.CallCallbacks( *this );
	cmfPath.CallCallbacks( *this );
	cmfLoadRequest.CallCallbacks( *this );
	exitRequested.CallCallbacks( *this );
	showUi.CallCallbacks( *this );
	uiScale.CallCallbacks( *this );

	modelState.CallCallbacks( *this );
}

void AppState::ResetModelState()
{
	modelState = {};
}
