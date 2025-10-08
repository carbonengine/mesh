#pragma once

#include "rendering/renderer.h"
#include "data/cmfContent.h"
#include "vulkan/buffer.h"
#include "vulkan/shadercache.h"

class ModelRenderer
{
public:
	ModelRenderer();
	~ModelRenderer();

    void Initialize( const Renderer* renderer );

    VkResult SetPerFrameData( const Renderer* renderer );
	VkResult RenderMesh( const Renderer* renderer, size_t meshIndex, size_t lodIndex );
	void Release( const Renderer* m_renderer );
	void SetData( const CmfContent* data, const Renderer* renderer );
    void SetShader( std::string shaderName, const Renderer* renderer );
	void SetPolygonMode( VkPolygonMode mode, const Renderer* renderer );

private: 
    void ReleaseMeshes( const Renderer* renderer );

    struct ModelLod
	{
		Buffer* vertexBuffer{ VK_NULL_HANDLE };
		uint32_t vertexStride{ 0 };
		Buffer* indexBuffer{ VK_NULL_HANDLE };
		uint8_t indexStride{ 0 };
	};
	
	struct Mesh
	{
		std::vector<ModelLod> lods;
		std::vector<VkVertexInputAttributeDescription> vertexDescriptions;
        uint32_t stride { 0 };
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

    VkPipelineLayout m_pipelineLayout {VK_NULL_HANDLE };
    std::vector<Mesh> m_meshes;

    std::string m_shaderName { "" };
	VkPolygonMode m_polygonMode{ VK_POLYGON_MODE_LINE };

	ShaderCache m_shaderCache;
	std::array<UniformBuffer, RenderUtils::MAX_FRAMES_IN_FLIGHT> m_perFrameBuffers;
	uint32_t m_currentFrame{ 0 };

    CcpMath::Sphere m_boundingSphere{};
};