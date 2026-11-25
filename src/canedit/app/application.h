// Copyright © 2026 CCP ehf.

#pragma once

#include "appContext.h"
#include "graphCanvas.h"
#include "panels.h"
#include "preview.h"

#include "rendering/imguiLayer.h"
#include "rendering/renderer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace canedit
{

/** The animation network editor application: window, main loop and UI layout. */
class Application
{
public:
	Application();
	~Application();

	Application( const Application& ) = delete;
	Application& operator=( const Application& ) = delete;
	Application( Application&& ) = delete;
	Application& operator=( Application&& ) = delete;

	/** Creates the window and the renderer. Returns false if that fails. */
	bool Initialize();
	/** Runs the main loop until the window is closed. */
	void Run();
	/** Opens a network document (.cane) or imports a CAN file, asking to save unsaved changes first. */
	void OpenFile( const std::string& path );

private:
	void Shutdown();
	void OnFramebufferResize();
	void OnDrop( const std::vector<std::string>& paths );
	void ApplyUiScale( float scale );

	// Commands
	bool ConfirmDiscardChanges();
	void NewDocument();
	void OpenDocument();
	void LoadDocument( const std::string& path );
	void ImportNetwork( const std::string& path );
	/** Offers to save embedded assets of an imported network that are not available as files. */
	void SaveOrphanedAssets( const std::vector<OrphanedAsset>& orphans );
	bool SaveDocument( bool askForPath );
	void ExportDocument( bool askForPath );
	void Undo();
	void Redo();
	void UpdateTitle();

	// UI
	void DrawUi();
	void DrawMenuBar();
	void DrawFileMenu();
	void DrawEditMenu();
	void DrawViewMenu();
	void DrawLayout();
	void DrawBreadcrumb();
	void DrawStatusBar();
	void DrawSettingsWindow();
	/** Draws the table row of an edited prefix, recording a removal request and the first validation error. */
	void DrawPrefixRow( size_t index, std::optional<size_t>& removed, std::string& error );
	void ApplyEditedPrefixes();
	void HandleGlobalShortcuts();
	/** A draggable bar that changes a panel size. */
	void Splitter( const char* id, bool vertical, float length, float& size, float minSize, float maxSize ) const;

	GLFWwindow* m_window = nullptr;
	std::shared_ptr<Renderer> m_renderer;
	std::unique_ptr<ImGuiLayer> m_imgui;
	bool m_shadersInitialized = false;

	EditorSettings m_settings;
	PathResolver m_resolver;
	std::unique_ptr<FileAssetLoader> m_loader;
	std::unique_ptr<Document> m_document;
	std::unique_ptr<EditorSession> m_session;
	std::unique_ptr<PreviewController> m_preview;
	AppContext m_context;
	std::unique_ptr<GraphCanvas> m_canvas;
	PalettePanel m_palette;
	PropertiesPanel m_properties;
	ParametersPanel m_parameters;

	bool m_framebufferResized = false;
	bool m_minimized = false;
	bool m_closeRequested = false;
	bool m_quit = false;
	bool m_showSettings = false;
	std::vector<PathPrefix> m_editedPrefixes;
	float m_uiScale = 1.f;
	float m_pendingUiScale = 0.f;
	std::string m_title;
};

}
