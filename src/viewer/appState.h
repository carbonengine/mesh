// Copyright © 2026 CCP ehf.

#pragma once

#include <functional>

#include "data/cmfcontent.h"
#include "rendering/vulkan/graphicseffecttypes.h"

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
	T& GetValue();
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
	size_t AddState( T initialValue );

	template <typename Callable>
	size_t AddState( Callable configurator );

	template <typename Callable>
	size_t AddState( T initialValue, Callable configurator );

	void Clear();
	void CallCallbacks( AppState& appState );

	void RegisterCallback( std::function<void( std::vector<T>, AppState& )> callback );

	void RemoveAt( size_t index );

	size_t size() const;
	bool empty() const;

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
	std::vector<std::function<void( std::vector<T>, AppState& )>> m_callbacks;
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

struct MeshState
{
	State<bool> display{ true };
	/// the pair is <weight, enabled>
	StateCollection<std::pair<float, bool>> morphs{ { 0.0f, true } };
	State<bool> wireframeOverlay{ false };
	State<bool> audioOcclusionMesh{ false };
	State<bool> renderBoundingBox{ false };
	StateCollection<std::pair<uint32_t, bool>> showVertexNormals{ { 0, false } };
	StateCollection<std::pair<uint32_t, bool>> showVertexTangents{ { 0, false } };
	StateCollection<std::pair<uint32_t, bool>> showVertexBinormals{ { 0, false } };
	State<uint32_t> activeLod{ 0 };
	State<float> meshScreenSize{ 0.0f };

	void CallCallbacks( AppState& appState )
	{
		display.CallCallbacks( appState );
		morphs.CallCallbacks( appState );
		wireframeOverlay.CallCallbacks( appState );
		audioOcclusionMesh.CallCallbacks( appState );
		renderBoundingBox.CallCallbacks( appState );
		activeLod.CallCallbacks( appState );
		meshScreenSize.CallCallbacks( appState );
		showVertexNormals.CallCallbacks( appState );
		showVertexTangents.CallCallbacks( appState );
		showVertexBinormals.CallCallbacks( appState );
	}
};

struct ModelState
{
	/// The lod selected by the user (-1 for auto)
	State<int32_t> selectedLod{ -1 };

	StateCollection<MeshState> meshes{ {} };

	State<std::pair<std::string, GraphicsEffectTypes::ShaderInputDeclaration>> activeShader{ {} };
	State<std::vector<std::pair<std::string, GraphicsEffectTypes::ShaderInputDeclaration>>> availableShaders{ {} };

	State<VkPolygonMode> polygonMode{ VK_POLYGON_MODE_FILL };
	State<std::string> currentAnimation{ "" };
	State<float> currentAnimationTime{ 0.0f };

	State<bool> boneDebug{ false };
	State<bool> jointDebug{ false };
	State<bool> jointAxisDebug{ false };
	State<bool> modelBoundingBox{ false };
	State<std::shared_ptr<CmfContent>> activeAnimationOwner{ nullptr };
	StateCollection<std::shared_ptr<CmfContent>> animationOverrides{ nullptr };
	StateCollection<uint32_t> selectedBones{ 0xFF };

	void CallCallbacks( AppState& appState )
	{
		selectedLod.CallCallbacks( appState );
		meshes.CallCallbacks( appState );
		activeShader.CallCallbacks( appState );
		availableShaders.CallCallbacks( appState );
		polygonMode.CallCallbacks( appState );
		currentAnimation.CallCallbacks( appState );
		currentAnimationTime.CallCallbacks( appState );
		boneDebug.CallCallbacks( appState );
		jointDebug.CallCallbacks( appState );
		jointAxisDebug.CallCallbacks( appState );
		modelBoundingBox.CallCallbacks( appState );
		activeAnimationOwner.CallCallbacks( appState );
		animationOverrides.CallCallbacks( appState );
		selectedBones.CallCallbacks( appState );
	}
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