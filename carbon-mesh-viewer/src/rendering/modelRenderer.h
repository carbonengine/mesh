#pragma once

#include "appState.h"
#include "camera.h"
#include "data/cmfcontent.h"
#include "rendering/renderer.h"
#include "vulkan/buffer.h"
#include "vulkan/shadercache.h"

// Handles rendering a cmf model
class ModelRenderer
{
public:
	ModelRenderer( std::shared_ptr<const Renderer> renderer );

	void Initialize( AppState& state );

	VkResult SetPerFrameData();
	VkResult RenderMesh( const AppState& state, const Camera& camera );

	void Release();
	void SetData( const CmfContent* data );
	void SetShader( std::string shaderName );
	void SetPolygonMode( VkPolygonMode mode );

private:
	void ReleaseMeshes();

	struct ModelArea
	{
		uint32_t firstElement = 0;
		uint32_t elementCount = 0;
	};

	struct ModelLod
	{
		Buffer* vertexBuffer{ VK_NULL_HANDLE };
		uint32_t vertexStride{ 0 };
		Buffer* indexBuffer{ VK_NULL_HANDLE };
		uint8_t indexStride{ 0 };

		std::vector<ModelArea> areas;
	};

	struct Mesh
	{
		std::vector<ModelLod> lods;
		std::vector<VkVertexInputAttributeDescription> vertexDescriptions;
		uint32_t stride{ 0 };
		VkPipeline pipeline{ VK_NULL_HANDLE };
	};

	struct UniformBuffer
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer buffer{ VK_NULL_HANDLE };
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
	};

	struct PerFrameData
	{
		Matrix proj;
		Matrix view;
	};

	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	std::vector<Mesh> m_meshes;

	std::string m_shaderName{ "" };
	VkPolygonMode m_polygonMode{ VK_POLYGON_MODE_FILL };

	ShaderCache m_shaderCache;
	std::array<UniformBuffer, RenderUtils::MAX_FRAMES_IN_FLIGHT> m_perFrameBuffers;
	uint32_t m_currentFrame{ 0 };

	std::shared_ptr<const Renderer> m_renderer{ nullptr };
};
