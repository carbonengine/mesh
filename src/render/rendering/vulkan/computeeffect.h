// Copyright © 2026 CCP ehf.

#pragma once
#include "../renderer.h"
#include "effect.h"


// Effect class that dictates how shaders are set up
class ComputeEffect : public Effect
{
public:
	ComputeEffect();
	ComputeEffect( std::shared_ptr<const Renderer> renderer );

	void Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex ) override;

protected:
	VkResult CreatePipeline() override;
};