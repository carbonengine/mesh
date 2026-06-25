// Copyright © 2026 CCP ehf.

#include "graphicseffect.h"

#include "shadercache.h"
#include "vulkanerrors.h"


GraphicsEffect::GraphicsEffect( std::shared_ptr<const Renderer> renderer ) :
	Effect( renderer )
{
}

void GraphicsEffect::SetConfig( GraphicsEffectTypes::Config config )
{
	m_config = config;

	if( m_initialized )
	{
		m_initialized = RecreatePipeline() == VK_SUCCESS;
	}
}

VkResult GraphicsEffect::CreatePipeline()
{
	return ShaderCache::CreateGraphicsPipeline( m_renderer.get(), m_shaderName, m_config, m_pipelineLayout, &m_pipeline );
}

void GraphicsEffect::Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex )
{
	assert( m_initialized );

	vkCmdSetLineWidth( commandBuffer, m_config.lineWidth );

	vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[currentFrameIndex], 0, nullptr );

	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );
}

size_t GraphicsEffect::GetStride() const
{
	return m_config.inputDeclaration.stride;
}
