// Copyright © 2026 CCP ehf.

#pragma once
#include "../renderer.h"
#include "effect.h"
#include "graphicseffecttypes.h"

class GraphicsEffect : public Effect
{
public:
	GraphicsEffect( std::shared_ptr<const Renderer> renderer );

	struct VertexUboData
	{
		Matrix proj;
		Matrix view;
	};

	void SetConfig( GraphicsEffectTypes::Config config );
	void Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex ) override;
	size_t GetStride() const;

protected:
	VkResult CreatePipeline() override;

private:
	GraphicsEffectTypes::Config m_config{};
};
