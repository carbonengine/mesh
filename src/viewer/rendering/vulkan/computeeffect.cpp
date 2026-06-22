// Copyright © 2026 CCP ehf.

#include "computeeffect.h"
#include "vulkanerrors.h"
#include "shadercache.h"

ComputeEffect::ComputeEffect()
{
}

ComputeEffect::ComputeEffect( std::shared_ptr<const Renderer> renderer ) :
	Effect( renderer )
{
}

VkResult ComputeEffect::CreatePipeline()
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	CR_RETURN( ShaderCache::CreateComputePipeline( m_renderer.get(), m_shaderName, m_pipelineLayout, &m_pipeline ) );

	return VkResult::VK_SUCCESS;
}

void ComputeEffect::Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex )
{
	assert( m_initialized );

	vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSets[currentFrameIndex], 0, nullptr );

	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline );
}
