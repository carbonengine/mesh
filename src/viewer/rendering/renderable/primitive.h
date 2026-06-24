// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "../vulkan/graphicseffect.h"


class PrimitiveRenderable
{
public:
	PrimitiveRenderable( std::shared_ptr<const Renderer> renderer, GraphicsEffect&& effect );
	~PrimitiveRenderable();

	void SetBufferData( const uint8_t* data, uint32_t size, uint32_t stride );
	void SetIndexData( const uint8_t* data, uint32_t size, uint32_t stride );
	GraphicsEffect& GetEffect();

	template <typename T>
	void SetUniformData( uint32_t layoutBindingIndex, const T& data )
	{
		m_effect.SetUniformData( layoutBindingIndex, data );
	}

	VkResult Initialize();
	void Render( GraphicsCommandBuffer& commandBuffer, uint32_t instanceCount = 1 );
	void Render( GraphicsCommandBuffer& commandBuffer, const Buffer* vertexBuffer, const Buffer* indexBuffer, uint32_t elements, uint32_t instanceCount );

private:
	std::shared_ptr<const Renderer> m_renderer{ nullptr };

	Buffer* m_vertexBuffer{ nullptr };
	Buffer* m_indexBuffer{ nullptr };
	std::vector<std::pair<uint32_t, Buffer*>> m_storageBuffers{};

	uint32_t m_vertexStride{ 0 };
	uint32_t m_vertexBufferSize{ 0 };
	const uint8_t* m_data{ nullptr };

	uint32_t m_indexStride{ 0 };
	uint32_t m_indexBufferSize{ 0 };
	const uint8_t* m_indexData{ nullptr };

	uint32_t m_elements{ 0 };

	GraphicsEffect m_effect;
};