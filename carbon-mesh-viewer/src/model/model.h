#pragma once
#include <cmf/cmf.h>
#include "device.h"
#include <buffer.h>
#include <vulkan/vulkan.h>

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
};

class Model
{
public:
	Model();
	Model( std::vector<uint8_t> fileContent, std::string filePath );
	~Model();
	void Release( Device* device, VkAllocationCallbacks* allocator );

	VkResult Render( VkCommandBuffer commandBuffer, size_t meshIndex, size_t lodIndex );

	VkResult Initialize( Device* device, VkCommandPool commandPool );

    uint32_t GetStride() const;
	const std::vector<VkVertexInputAttributeDescription>& GetVertexDescriptions() const;

	CcpMath::Sphere GetBoundingSphere() const;

private:
	VkResult SetupBuffers( Device* device, VkCommandPool commandPool );

	cmf::Header* m_cmfHeader;
	cmf::Data* m_cmfData;

	std::vector<uint8_t> m_fileContent;
	std::string m_filePath;

	// contains all the rendering information for each lod
	std::vector<Mesh> m_meshes;
	// contains the vertex input descriptions 
	std::vector<VkVertexInputAttributeDescription> m_vertexDescriptions;
	VkPipelineLayout m_pipelineLayout;
	bool m_isValid;
};

namespace ModelLoader
{
Model* LoadModelFromFile( const std::string& filePath );
};
