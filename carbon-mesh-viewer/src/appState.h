#pragma once

#include <functional>

#include "data/cmfcontent.h"

//forwards declaration
struct AppState;

template <typename T>
class StateCollection;

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
	void SetValue( T newValue );
	void ForceSetValue( T newValue );
	void SetValueNoCallback( T newValue );
	void Reset();

	void RegisterCallback( std::function<void( T, AppState& )> callback );
	void CallCallbacks( AppState& );

private:
	T m_value;
	T m_initialValue;
	std::vector<std::function<void( T, AppState& )>> m_callbacks;
	bool m_fireCallbacks = false;
	friend class StateCollection<T>;
};

template <typename T>
class StateCollection
{
public:
	using iterator = typename std::vector<State<T>>::iterator;
	using const_iterator = typename std::vector<State<T>>::const_iterator;

	StateCollection( T initialValue );
	size_t AddState();

	void Clear();
	void CallCallbacks( AppState& appState );

	size_t size() const;

	// Non-const iterators
	iterator begin();
	iterator end();

	// Const iterators
	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin() const;
	const_iterator cend() const;

	// Indexing operators
	State<T>& operator[]( size_t index );
	const State<T>& operator[]( size_t index ) const;

private:
	std::vector<State<T>> m_states;
	T m_initialValue;
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

struct ModelState
{
	State<uint32_t> selectedLod{ 0 };
	State<std::string> visualizationShader{ "" };
	State<std::vector<std::string>> availableShaders{ {} };

	State<VkPolygonMode> polygonMode{ VK_POLYGON_MODE_FILL };
	State<std::string> currentAnimation{ "" };
	State<float> currentAnimationTime{ 0.0f };

	StateCollection<bool> meshVisibilityStates{ true };
	StateCollection<float> morphTargetWeight{ 0.0 };
	StateCollection<bool> morphTargetEnabled{ true };
	StateCollection<bool> meshWireframeOverlay{ false };
	StateCollection<bool> audioOcclusionMesh{ false };
	StateCollection<bool> meshBoundingBox{ false };
	State<bool> modelBoundingBox{ false };

	State<std::string> animationOverridePath{ "" };
	State<std::shared_ptr<CmfContent>> animationOverride{ nullptr };
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
	State<std::shared_ptr<CmfContent>> cmfContent{ nullptr };
	State<std::string> cmfPath{ "" };

	State<bool> exitRequested{ false };

	// model
	ModelState modelState{};

	void CallStateCallbacks();
	void ResetModelState();
};



#include "appState_template_impl.h"