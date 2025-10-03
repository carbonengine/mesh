#pragma once
#include <cmf/cmf.h>
#include "device.h"
#include <buffer.h>
#include <vulkan/vulkan.h>
#include "effect/effect.h"

struct ModelLod
{
	Buffer* vertexBuffer;
	uint32_t vertexStride{ 0 };
	Buffer* indexBuffer;
	uint8_t indexStride{ 0 };
};

struct Mesh
{
	std::vector<ModelLod> lods;
	std::vector<VkVertexInputAttributeDescription> vertexDescriptions;
	std::vector<VkPipeline> pipelines;
};

class Model
{
public:
	Model();
	Model( std::vector<uint8_t> fileContent, std::string filePath );
	~Model();
	void Release( Device* device );

	VkResult Render( VkCommandBuffer commandBuffer, VkDescriptorSet perFrameDataDescriptor, size_t meshIndex, size_t lodIndex );

	VkResult Initialize( Device* device, VkCommandPool commandPool, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout );
	ModelLod GetLod( size_t meshIndex, size_t lodIndex ) const;
	CcpMath::Sphere GetBoundingSphere() const;

private:
	VkResult SetupBuffers( Device* device, VkCommandPool commandPool );
	VkResult SetupPipeline( Device* device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout );

	VkResult CreatePipelineLayout( VkDevice device, VkDescriptorSetLayout descriptorSetLayout );
	VkResult FinalizePipeline( Mesh& mesh, VkDevice device, VkRenderPass renderPass, VkPipelineVertexInputStateCreateInfo inputState, VkPolygonMode topology );

	cmf::Header* m_cmfHeader;
	cmf::Data* m_cmfData;

	std::vector<uint8_t> m_fileContent;
	std::string m_filePath;

	// contains all the rendering information for each lod
	std::vector<Mesh> m_meshes;
	// contains the vertex input descriptions for each mesh. There is one vertex input description per meshlod, probably a bad idea
	std::vector<std::vector<VkVertexInputAttributeDescription>> m_vertexDescriptions;
	Effect* m_effect;
	VkPipelineLayout m_pipelineLayout;
	bool m_isValid;
};

namespace ModelLoader
{
Model* LoadModelFromFile( const std::string& filePath );
};
