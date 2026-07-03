// Copyright © 2026 CCP ehf.

#include "uiMenubar.h"
#include <imgui.h>

#include "uiCustomWidgets.h"

void UiMenubar::Render( AppState& appState, MenuState& menuState )
{
	if( ImGui::BeginMainMenuBar() )
	{
		if( ImGui::BeginMenu( "File" ) )
		{
			if( ImGui::MenuItem( "Open", "Ctrl+O" ) )
			{
				auto filePath = ImGui::OpenCmfFileDialog();
				if( filePath != nullptr )
				{
					appState.cmfLoadRequest.SetValue( filePath );
				}
			}
			if( appState.cmfContent.GetValue() == nullptr )
			{
				ImGui::BeginDisabled();
			}
			if( ImGui::MenuItem( "Load Animation Override" ) )
			{
				const auto* filePath = ImGui::OpenCmfFileDialog();
				if( filePath != nullptr )
				{
					auto data = CmfContentLoader::LoadContentFromFile( filePath );
					if( data )
					{
						appState.modelState.animationOverrides.AddState( data );
					}
				}
			}
			if( appState.cmfContent.GetValue() == nullptr )
			{
				ImGui::EndDisabled();
			}

			ImGui::Separator();
			if( ImGui::MenuItem( "Exit" ) )
			{
				appState.exitRequested.SetValue( true );
			}
			ImGui::EndMenu();
		}
		auto content = appState.cmfContent.GetValue();

		if( ImGui::BeginMenu( "View" ) )
		{
			if( ImGui::BeginMenu( "Camera", content != nullptr ) )
			{
				std::vector<std::tuple<std::string, CcpMath::Sphere>> focusObjects;
				if( content != nullptr )
				{
					const auto& meshes = content->m_cmfData->meshes;

					if( meshes.size() > 1 )
					{
						focusObjects.push_back( { "Whole Model", content->GetBoundingSphere() } );
					}
					std::for_each( meshes.begin(), meshes.end(), [&focusObjects]( const cmf::Mesh& mesh ) {
						focusObjects.push_back( { cmf::ToStdString( mesh.name ), CcpMath::Sphere( mesh.bounds ) } );
					} );
				}

				uint32_t index = 0;
				for( const auto& [menuName, sphere] : focusObjects )
				{
					std::string menuLabel = "Focus on " + menuName;
					const char* shortcut = nullptr;
					if( index == 0 )
					{
						shortcut = "Ctrl+F";
					}
					if( ImGui::MenuItem( menuLabel.c_str(), shortcut ) )
					{
						appState.cameraFocus.ForceSetValue( sphere );
						appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_FOCUS );
					}
					++index;
				}
				ImGui::Separator();
				if( ImGui::MenuItem( "Look Right (+X)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_RIGHT );
				}
				if( ImGui::MenuItem( "Look Left (-X)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_LEFT );
				}
				if( ImGui::MenuItem( "Look Up (+Y)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_UP );
				}
				if( ImGui::MenuItem( "Look Down (-Y)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_DOWN );
				}
				if( ImGui::MenuItem( "Look Front (-Z)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_FRONT );
				}
				if( ImGui::MenuItem( "Look Back (+Z)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_BACK );
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			const char* toggleUiLabel = menuState.showUi ? "Hide UI" : "Show UI";
			if( ImGui::MenuItem( toggleUiLabel, "Ctrl+F12" ) )
			{
				menuState.showUi = !menuState.showUi;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}
