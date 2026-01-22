#pragma once

#include <functional>

#include "data/cmfcontent.h"

struct MouseState
{
	Vector2 position = { 0.0f, 0.0f };
	Vector2 previousPosition = { 0.0f, 0.0f };
	// vertical wheel delta
	float wheelDelta{ 0.0f };
	bool leftButton{ false };
	bool middleButton{ false };
	bool rightButton{ false };

	bool operator==( const MouseState& other ) const;
};

template <typename T>
class State
{
public:
	State( T initialValue );
	const T GetValue() const;
	void SetValue( T newValue, bool force = false );
	void Reset();

	void RegisterCallback( std::function<void( T )> callback );
	void CallCallbacks();

private:
	T m_value;
	T m_initialValue;
	std::vector<std::function<void( T )>> m_callbacks;
	bool m_fireCallbacks = false;
};

enum class CameraTrigger
{
	CAMERA_TRIGGER_NONE,
	CAMERA_TRIGGER_FOCUS,
	CAMERA_TRIGGER_LOOK_UP,
	CAMERA_TRIGGER_LOOK_DOWN,
	CAMERA_TRIGGER_LOOK_RIGHT,
	CAMERA_TRIGGER_LOOK_LEFT,
	CAMERA_TRIGGER_LOOK_FRONT,
	CAMERA_TRIGGER_LOOK_BACK,
};


struct AppState
{
	// window
	State<std::pair<uint32_t, uint32_t>> windowSize{ { 0, 0 } };

	// mouse
	State<MouseState> mouseState{ {} };

	// camera
	State<CameraTrigger> cameraTrigger{ CameraTrigger::CAMERA_TRIGGER_NONE };

	// cmf
	State<CmfContent*> cmfContent{ nullptr };
	State<std::string> cmfPath{ "" };
	// ui
	State<uint32_t> selectedLod{ 0 };
	State<int32_t> selectedMesh{ -1 };
	State<std::string> visualizationShader{ "facenormal" };
	State<VkPolygonMode> polygonMode{ VK_POLYGON_MODE_FILL };

	void CallStateCallbacks();
};

#include "appState_template_impl.h"