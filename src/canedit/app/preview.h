// Copyright © 2026 CCP ehf.

#pragma once

#include "appContext.h"

#include "rendering/camera.h"
#include "rendering/sceneRenderer.h"
#include "rendering/vulkan/rendertarget.h"

#include <cmf/animationnetworkinstance.h>

#include <imgui.h>

#include <map>
#include <memory>
#include <string>

namespace canedit
{

/** Length in seconds of the preview time line until it is changed. */
constexpr float DEFAULT_TIMELINE_LENGTH = 10.f;

/** Evaluates the edited network and shows the result on a skinned model.
*
* The network is saved and loaded into a runtime network shortly after it changes. Playback time and parameter
* values are kept across rebuilds. The model is the preview mesh of the document, or the skeleton file when no mesh
* is set; the skeleton is always drawn when there is no mesh. */
class PreviewController
{
public:
	PreviewController( std::shared_ptr<Renderer> renderer, FileAssetLoader& loader );
	~PreviewController();

	PreviewController( const PreviewController& ) = delete;
	PreviewController& operator=( const PreviewController& ) = delete;
	PreviewController( PreviewController&& ) = delete;
	PreviewController& operator=( PreviewController&& ) = delete;

	/** Applies a resize of the viewport requested in the previous frame. Call before the ImGui frame starts. */
	void BeginFrame();
	/** Rebuilds the network after changes, advances playback and poses the model. */
	void Update( float deltaTime, Document& document );
	/** Records the skinning compute pass. Call between Renderer::BeginCompute and EndCompute. */
	void PrePass();
	/** Renders the scene into the viewport texture. Call after Renderer::BeginRender, before ImGui is rendered. */
	void Render();

	/** Draws the viewport image filling the available ImGui space and handles camera input. */
	void DrawViewport();
	/** Draws play controls and the time line. */
	void DrawTimeline();

	/** Forgets the network and the model, so that they are loaded again (for example after the asset paths changed). */
	void Reload();

	[[nodiscard]] bool IsNetworkReady() const;
	/** Problems of the preview itself, such as a preview mesh that failed to load. */
	[[nodiscard]] const std::string& GetModelError() const;

	/** Preview values of the control parameters, by name. */
	[[nodiscard]] float GetParameterValue( const cmf::AnimationParameter& parameter ) const;
	void SetParameterValue( const std::string& name, float value );
	void ResetParameterValue( const std::string& name );

private:
	void UpdateModel( Document& document );
	void UpdateNetwork( Document& document );
	/** Creates a new instance at time 0 with the current parameter values. */
	void StartInstance();
	void ApplyPose( const cmf::AnimationNetworkBuilder& builder );
	void SeekTo( float time );

	std::shared_ptr<Renderer> m_renderer;
	FileAssetLoader* m_loader;
	SceneRenderer m_scene;
	RenderTarget m_target;
	Camera m_camera;
	ImTextureID m_texture{};
	uint32_t m_requestedWidth = 0;
	uint32_t m_requestedHeight = 0;
	bool m_rendered = false;
	bool m_cameraDrag = false;

	// Model
	std::string m_modelReference;
	bool m_modelIsMesh = false;
	std::string m_modelError;
	CcpMath::Sphere m_bounds{};

	// Network
	cmf::AnimationNetwork m_network;
	std::unique_ptr<cmf::AnimationNetworkInstance> m_instance;
	uint64_t m_builtGeneration = 0;
	uint64_t m_seenGeneration = 0;
	double m_changeTime = 0.0;
	std::map<std::string, float> m_parameterValues;
	cmf::SkeletonPose m_pose;

	// Playback
	bool m_playing = true;
	float m_time = 0.f;
	float m_speed = 1.f;
	float m_timelineLength = DEFAULT_TIMELINE_LENGTH;
	bool m_showSkeleton = false;
};

}
