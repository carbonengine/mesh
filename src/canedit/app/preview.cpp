// Copyright © 2026 CCP ehf.

#include "preview.h"

#include "rendering/imguiLayer.h"

#include <cmf/utils.h>

#include <algorithm>
#include <cmath>

// ImGui uses C-style variadic functions for formatted text
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace canedit
{
namespace
{

/** Seconds without changes before the preview network is rebuilt. */
constexpr double REBUILD_DELAY = 0.1;
/** Step of the frame buttons in seconds. */
constexpr float FRAME_STEP = 1.f / 30.f;
constexpr float MAX_SPEED = 4.f;
constexpr float MIN_TIMELINE_LENGTH = 1.f;
constexpr float MAX_TIMELINE_LENGTH = 600.f;
/** Widths of the speed and length fields, in font sizes. */
constexpr float SPEED_FIELD_WIDTH = 6.f;
constexpr float LENGTH_FIELD_WIDTH = 5.f;
/** Largest time step used when the playhead is moved, so that sampling stays accurate. */
constexpr float SEEK_STEP = 1.f / 60.f;
constexpr float CLEAR_COLOR = 0.16f;
/** Smallest radius of the model bounds used to place the camera. */
constexpr float MIN_RADIUS = 1e-3f;

std::shared_ptr<CmfContent> MakeContent( const std::vector<uint8_t>& bytes, const std::string& reference )
{
	cmf::ValidationOptions options;
	options.validateCrc = true;
	options.validateHeader = true;
	options.validateMainData = true;
	if( !cmf::ValidateFile( bytes.data(), bytes.size(), options ) )
	{
		return nullptr;
	}
	return std::make_shared<CmfContent>( bytes, reference );
}

}

PreviewController::PreviewController( std::shared_ptr<Renderer> renderer, FileAssetLoader& loader ) :
	m_renderer( std::move( renderer ) ),
	m_loader( &loader ),
	m_scene( m_renderer ),
	m_target( m_renderer ),
	m_network( loader )
{
	m_scene.Initialize( 1, 1 );
	m_scene.SetClearColor( CLEAR_COLOR, CLEAR_COLOR, CLEAR_COLOR );
	m_scene.SetRenderTarget( &m_target );
}

PreviewController::~PreviewController()
{
	ImGuiLayer::RemoveTexture( m_texture );
}

void PreviewController::BeginFrame()
{
	if( m_requestedWidth == 0 || m_requestedHeight == 0 )
	{
		return;
	}
	if( m_target.Resize( m_requestedWidth, m_requestedHeight ) )
	{
		ImGuiLayer::RemoveTexture( m_texture );
		m_texture = ImGuiLayer::AddTexture( m_target );
		m_rendered = false;
	}
	m_scene.SetRenderSize( m_target.GetWidth(), m_target.GetHeight() );
	m_camera.SetScreenSize( m_target.GetWidth(), m_target.GetHeight() );
}

void PreviewController::Reload()
{
	m_scene.SetData( nullptr );
	m_modelReference.clear();
	m_modelError.clear();
	m_instance.reset();
	m_network.Load( {} );
	m_builtGeneration = 0;
	m_seenGeneration = 0;
}

void PreviewController::UpdateModel( Document& document )
{
	const auto& builder = document.GetNetwork();
	const bool isMesh = !document.GetPreviewMesh().empty();
	const auto& reference = isMesh ? document.GetPreviewMesh() : builder.GetSkeletonPath();
	if( reference == m_modelReference && isMesh == m_modelIsMesh && ( m_scene.GetModel() != nullptr || !m_modelError.empty() || reference.empty() ) )
	{
		return;
	}
	if( !isMesh && builder.HasPendingAssets() )
	{
		return;
	}
	m_modelReference = reference;
	m_modelIsMesh = isMesh;
	m_modelError.clear();
	m_scene.SetData( nullptr );
	if( reference.empty() )
	{
		return;
	}

	std::shared_ptr<CmfContent> content;
	if( const auto* embedded = builder.GetEmbeddedAsset( reference ) )
	{
		content = MakeContent( *embedded, reference );
	}
	else
	{
		std::string error;
		const auto file = m_loader->LoadUnmanaged( reference, error );
		if( file )
		{
			content = MakeContent( file->GetBytes(), reference );
		}
		else
		{
			m_modelError = ( isMesh ? "Preview mesh: " : "Skeleton: " ) + error;
		}
	}
	if( !content )
	{
		if( m_modelError.empty() )
		{
			m_modelError = "Preview model '" + reference + "' could not be loaded";
		}
		return;
	}
	auto* model = m_scene.SetData( content );
	if( model == nullptr )
	{
		return;
	}
	const auto shaders = SceneRenderer::GetAvailableShaders( *content );
	if( const auto shader = SceneRenderer::ChooseShader( shaders, "" ) )
	{
		model->SetShader( shader->first, shader->second );
	}
	m_bounds = content->GetBoundingSphere();
	if( m_bounds.radius < MIN_RADIUS )
	{
		// For example a skeleton whose bones are all at the origin
		m_bounds.radius = 1.f;
	}
	m_camera.Reset( m_bounds );
}

void PreviewController::StartInstance()
{
	m_instance.reset();
	m_time = 0.f;
	if( !m_network.IsReady() )
	{
		return;
	}
	m_instance = std::make_unique<cmf::AnimationNetworkInstance>( m_network );
	for( const auto& [name, value] : m_parameterValues )
	{
		const auto index = m_instance->FindParameter( name );
		if( index != cmf::AnimationNetworkInstance::INVALID_PARAMETER )
		{
			m_instance->SetParameter( index, value );
		}
	}
}

void PreviewController::SeekTo( float time )
{
	if( time < m_time || !m_instance )
	{
		// The network only plays forward, so going back starts over
		StartInstance();
	}
	if( !m_instance )
	{
		m_time = time;
		return;
	}
	while( m_time < time )
	{
		const auto step = std::min( SEEK_STEP, time - m_time );
		m_instance->Update( step );
		m_time += step;
	}
}

void PreviewController::UpdateNetwork( Document& document )
{
	const auto& builder = document.GetNetwork();
	const auto generation = builder.GetGeneration();
	if( generation != m_seenGeneration )
	{
		m_seenGeneration = generation;
		m_changeTime = ImGui::GetTime();
	}
	if( generation == m_builtGeneration || ImGui::GetTime() - m_changeTime < REBUILD_DELAY || builder.HasPendingAssets() )
	{
		return;
	}
	m_builtGeneration = generation;
	m_instance.reset();
	if( !m_network.Load( builder.Save() ) )
	{
		return;
	}
	// Continue at the same time with the same parameter values
	const auto time = m_time;
	StartInstance();
	SeekTo( time );
}

void PreviewController::ApplyPose( const cmf::AnimationNetworkBuilder& builder )
{
	auto* model = m_scene.GetModel();
	if( model == nullptr )
	{
		return;
	}
	if( m_instance && m_instance->IsValid() && m_instance->Evaluate() )
	{
		m_instance->GetPose( m_pose );
	}
	else if( const auto* skeleton = builder.GetSkeleton() )
	{
		// Networks that can't be evaluated show the bind pose
		cmf::RestPose( m_pose, *skeleton );
	}
	else
	{
		return;
	}
	const auto& content = model->GetContent();
	for( size_t i = 0; i < content->m_cmfData->skeletons.size(); ++i )
	{
		model->SetPose( static_cast<uint8_t>( i ), m_pose );
	}
}

void PreviewController::Update( float deltaTime, Document& document )
{
	UpdateModel( document );
	UpdateNetwork( document );
	if( m_instance && !m_instance->IsValid() )
	{
		m_instance.reset();
	}
	if( m_playing && m_instance )
	{
		const auto step = deltaTime * m_speed;
		m_instance->Update( step );
		m_time += step;
		m_timelineLength = std::max( m_timelineLength, m_time );
	}
	ApplyPose( document.GetNetwork() );
	m_camera.Update( deltaTime );
	m_scene.Update( m_camera );
}

void PreviewController::PrePass()
{
	m_scene.PrePass();
}

void PreviewController::Render()
{
	if( !m_target.IsValid() )
	{
		return;
	}
	DebugDrawSettings settings;
	settings.bones = m_showSkeleton || !m_modelIsMesh;
	settings.joints = settings.bones;
	settings.radius = m_bounds.radius > 0.f ? m_bounds.radius : 1.f;
	m_scene.Render( m_camera, settings );
	m_rendered = true;
}

void PreviewController::DrawViewport()
{
	const auto available = ImGui::GetContentRegionAvail();
	const auto width = static_cast<uint32_t>( std::max( available.x, 1.f ) );
	const auto height = static_cast<uint32_t>( std::max( available.y, 1.f ) );
	m_requestedWidth = width;
	m_requestedHeight = height;

	const auto origin = ImGui::GetCursorScreenPos();
	if( m_texture != ImTextureID{} && m_rendered && m_target.GetWidth() == width && m_target.GetHeight() == height )
	{
		ImGui::Image( m_texture, available );
	}
	else
	{
		ImGui::Dummy( available );
	}

	// Camera: left drag orbits, right drag pans, wheel or middle drag zooms
	const bool hovered = ImGui::IsItemHovered();
	const auto& io = ImGui::GetIO();
	if( hovered && ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) || ImGui::IsMouseClicked( ImGuiMouseButton_Right ) || ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) ) )
	{
		m_cameraDrag = true;
	}
	if( !ImGui::IsMouseDown( ImGuiMouseButton_Left ) && !ImGui::IsMouseDown( ImGuiMouseButton_Right ) && !ImGui::IsMouseDown( ImGuiMouseButton_Middle ) )
	{
		m_cameraDrag = false;
	}
	if( m_cameraDrag || hovered )
	{
		MouseState mouse;
		mouse.position = { io.MousePos.x - origin.x, io.MousePos.y - origin.y };
		mouse.previousPosition = { mouse.position.x - io.MouseDelta.x, mouse.position.y - io.MouseDelta.y };
		mouse.wheelDelta = hovered ? io.MouseWheel : 0.f;
		mouse.leftButton = m_cameraDrag && ImGui::IsMouseDown( ImGuiMouseButton_Left );
		mouse.rightButton = m_cameraDrag && ImGui::IsMouseDown( ImGuiMouseButton_Right );
		mouse.middleButton = m_cameraDrag && ImGui::IsMouseDown( ImGuiMouseButton_Middle );
		m_camera.HandleMouseStateChanged( mouse );
	}
	if( hovered && ImGui::IsKeyPressed( ImGuiKey_F ) )
	{
		m_camera.HandleCameraTriggerChange( CameraTrigger::Focus, m_bounds );
	}

	// Overlay with the state of the preview
	auto* drawList = ImGui::GetWindowDrawList();
	const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
	auto textPosition = ImVec2( origin.x + ImGui::GetStyle().ItemSpacing.x, origin.y + ImGui::GetStyle().ItemSpacing.y );
	const auto overlay = [&]( ImU32 color, const std::string& text ) {
		drawList->AddText( textPosition, color, text.c_str() );
		textPosition.y += lineHeight;
	};
	if( !m_modelError.empty() )
	{
		overlay( IM_COL32( 255, 120, 100, 255 ), m_modelError );
	}
	if( !m_network.IsReady() )
	{
		overlay( IM_COL32( 255, 190, 90, 255 ), "Network can't be evaluated, showing the bind pose" );
	}
	if( m_modelReference.empty() )
	{
		overlay( IM_COL32( 200, 200, 200, 255 ), "Set a skeleton or preview mesh to see the network" );
	}

	// The option goes in the bottom left corner, away from the messages
	ImGui::SetCursorScreenPos( { origin.x + ImGui::GetStyle().ItemSpacing.x, origin.y + available.y - ImGui::GetFrameHeightWithSpacing() } );
	ImGui::Checkbox( "Show skeleton", &m_showSkeleton );
}

void PreviewController::DrawTimeline()
{
	const bool ready = m_instance != nullptr;
	ImGui::BeginDisabled( !ready );
	if( ImGui::Button( m_playing ? "Pause" : "Play" ) || ( ImGui::IsKeyPressed( ImGuiKey_Space ) && !ImGui::GetIO().WantTextInput && ImGui::IsWindowHovered() ) )
	{
		m_playing = !m_playing;
	}
	ImGui::SameLine();
	if( ImGui::Button( "<" ) )
	{
		m_playing = false;
		SeekTo( std::max( 0.f, m_time - FRAME_STEP ) );
	}
	ImGui::SameLine();
	if( ImGui::Button( ">" ) )
	{
		m_playing = false;
		SeekTo( m_time + FRAME_STEP );
	}
	ImGui::SameLine();
	if( ImGui::Button( "Restart" ) )
	{
		SeekTo( 0.f );
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth( ImGui::GetFontSize() * SPEED_FIELD_WIDTH );
	ImGui::SliderFloat( "Speed", &m_speed, 0.f, MAX_SPEED, "%.2fx" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( ImGui::GetFontSize() * LENGTH_FIELD_WIDTH );
	ImGui::DragFloat( "Length", &m_timelineLength, 1.f, MIN_TIMELINE_LENGTH, MAX_TIMELINE_LENGTH, "%.0f s" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( -1.f );
	float time = m_time;
	if( ImGui::SliderFloat( "##time", &time, 0.f, std::max( m_timelineLength, m_time ), "%.2f s" ) )
	{
		m_playing = false;
		SeekTo( std::max( 0.f, time ) );
	}
	ImGui::EndDisabled();
}

bool PreviewController::IsNetworkReady() const
{
	return m_network.IsReady();
}

const std::string& PreviewController::GetModelError() const
{
	return m_modelError;
}

float PreviewController::GetParameterValue( const cmf::AnimationParameter& parameter ) const
{
	const auto it = m_parameterValues.find( parameter.name );
	return it == m_parameterValues.end() ? parameter.defaultValue : it->second;
}

void PreviewController::SetParameterValue( const std::string& name, float value )
{
	m_parameterValues[name] = value;
	if( m_instance )
	{
		const auto index = m_instance->FindParameter( name );
		if( index != cmf::AnimationNetworkInstance::INVALID_PARAMETER )
		{
			m_instance->SetParameter( index, value );
		}
	}
}

void PreviewController::ResetParameterValue( const std::string& name )
{
	m_parameterValues.erase( name );
	if( m_instance )
	{
		const auto index = m_instance->FindParameter( name );
		const auto* data = m_network.GetData();
		if( index != cmf::AnimationNetworkInstance::INVALID_PARAMETER && data != nullptr && index < data->parameters.size() )
		{
			m_instance->SetParameter( index, data->parameters[index].defaultValue );
		}
	}
}

}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
