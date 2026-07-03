// Copyright © 2026 CCP ehf.

#include "appState.h"
#include "rendering/renderer.h"
#include "uiDetailWindow.h"
#include "uiMenubar.h"
#include "uiGeneralWindow.h"
#include "uiAnimationPlayback.h"
#include "../vulkan/commandbuffer.h"

// Handles rendering the UI
class UIRenderer
{
public:
	UIRenderer( std::shared_ptr<const Renderer> );
	~UIRenderer();

	void Initialize( GLFWwindow* window, AppState& appState );
	void BeginFrame();

	void Render( AppState& appState );

	void SetupUi( AppState& appState );
	void CMFInfoWindow( AppState& appState );
	void Update( AppState& state );

private:
	void SetupPopupWindows( AppState& appState );
	void UpdateInputs( AppState& state );

	enum LoadStatus
	{
		FAILED,
		SUCCESSFUL,
		NOTHING_LOADED
	};

	void UpdateUiState( AppState& appState );

	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

	std::shared_ptr<const Renderer> m_renderer;
	GraphicsCommandBuffer m_graphicsCommandBuffer;

	LoadStatus m_loadStatus{ NOTHING_LOADED };
	UIGeneralWindow m_generalWindow{};
	UIDetailWindow m_detailWindow{};
	UIAnimationPlayback m_animationPlayback{};
	UiMenubar m_menubar{};
};
