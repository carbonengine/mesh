#pragma once

#include "../renderer.h"
#include "../vulkan/buffer.h"
#include "../vulkan/commandbuffer.h"
#include <cmf/bufferstreams.h>

class MeshLodRenderable
{
public:
	struct Area
	{
		uint32_t firstElement = 0;
		uint32_t elementCount = 0;
	};
	MeshLodRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, const cmf::MeshLod& cmfLod, std::shared_ptr<const Renderer> renderer );
	~MeshLodRenderable();

	void Initialize( VkCommandBuffer initializeCmd, size_t morphTargetStateIndex );
	void Finalize();
	void UpdateGeo( const AppState& appState );
	void Render( CommandBuffer& commandBuffer );

private:
	struct BufferData
	{
		const uint8_t* data{ nullptr };
		uint32_t size{ 0 };
		uint32_t stride{ 0 };
	};

	void RenderBuffers( CommandBuffer& commandBuffer, Buffer* vertex, Buffer* index );

	Buffer* Morph( const AppState& appState );
	bool HasMorphs( const AppState& appState );
	template <typename T>
	void ApplyMorph( const AppState& appState, const cmf::VertexElement& element, BufferData& outputBuffer );

	std::shared_ptr<const Renderer> m_renderer;

	std::vector<Buffer*> m_modifiedVertexBuffer{};
	Buffer* m_currentVertexBuffer{ nullptr };

	Buffer* m_vertexBuffer{ nullptr };
	Buffer* m_indexBuffer{ nullptr };

	cmf::MeshLod m_cmfLod{};
	cmf::Mesh m_cmfMesh{};
	CmfContent* m_data{ nullptr };
	size_t m_morphTargetStateIndex;

	BufferData m_vertexData = {};
	BufferData m_indexData = {};

	std::vector<VkVertexInputAttributeDescription> m_morphAttributeDescriptions{};
	std::vector<Area> m_areas{};
};