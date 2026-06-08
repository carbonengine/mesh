#pragma once
#include "../renderer.h"
#include "effect.h"

class GraphicsEffect : public Effect
{
public:
	GraphicsEffect( std::shared_ptr<const Renderer> renderer );

	struct VertexUboData
	{
		Matrix proj;
		Matrix view;
	};

	struct Config
	{
		VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
		VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
		float lineWidth{ 1.0f };
		VkCompareOp depthCompareOp{ VK_COMPARE_OP_LESS };
		VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
		bool blend{ false };
		size_t stride{ 0 };
		VkVertexInputRate vertexInputRate{ VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector<cmf::VertexElement> availableVertexElements{};
	};

	void SetConfig( GraphicsEffect::Config config );
	void Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex ) override;
	size_t GetStride() const;

protected:
	VkResult CreatePipeline() override;

private:
	Config m_config{};
};
