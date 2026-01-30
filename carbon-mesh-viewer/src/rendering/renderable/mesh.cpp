#include "mesh.h"

#include "../renderer.h"
#include "../vulkan/vulkanenums.h"
#include "../vulkan/vulkanerrors.h"

MeshRenderable::MeshRenderable( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
}

MeshRenderable::MeshRenderable( const CmfContent* data, const cmf::Mesh cmfMesh, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
	for( const auto& decl : cmfMesh.decl )
	{
		// Generate a predictable location so that shaders can find the attribute.
		uint32_t location = (uint32_t)decl.usage * 4u + decl.usageIndex;

		VkVertexInputAttributeDescription attrDesc{};
		attrDesc.binding = 0;
		attrDesc.location = location;
		attrDesc.offset = decl.offset;
		attrDesc.format = VulkanEnums::ElementTypeToVkFormat( decl.type, decl.elementCount );
		m_vertexDescriptions.push_back( attrDesc );
	}

	// Implementation for constructing Mesh from cmf::Mesh goes here
	for( const auto& cmfLod : cmfMesh.lods )
	{
		m_lods.push_back( { data, cmfLod, renderer } );
		m_stride = std::max( m_stride, cmfLod.vb.stride );
	}
}

MeshRenderable::~MeshRenderable()
{
	vkDestroyPipeline( m_renderer->GetDevice()->GetLogicalDevice(), m_pipeline, m_renderer->GetAllocator() );
}

void MeshRenderable::SetStride( uint32_t stride )
{
	m_stride = stride;
}

void MeshRenderable::Initialize( VkCommandBuffer initializeCmd )
{
	// initialize the pipeline
	for( auto& lod : m_lods )
	{
		lod.Initialize( initializeCmd );
	}
}

void MeshRenderable::Finalize()
{
	for( auto& lod : m_lods )
	{
		lod.Finalize();
	}
}

void MeshRenderable::Render( CommandBuffer& commandBuffer, uint32_t lodIndex, int32_t areaIndex ) const
{
	if( lodIndex >= m_lods.size() )
	{
		// maybe this is not an error, we should just skip rendering. But for now, log an error.
		Log::Error( "Lod index out of bounds in Mesh::Render (%d requested, %d available)", lodIndex, (uint32_t)m_lods.size() );
		return;
	}

	commandBuffer.BindPipeline( m_pipeline );

	m_lods[lodIndex].Render( commandBuffer, areaIndex );
}

VkResult MeshRenderable::SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );
	if( m_pipeline != VK_NULL_HANDLE )
	{
		vkDestroyPipeline( logicalDevice, m_pipeline, m_renderer->GetAllocator() );
		m_pipeline = VK_NULL_HANDLE;
	}
	CR_RETURN( shaderCache->CreatePipeline( shaderName, m_topology, polygonMode, m_lineWidth, m_stride, m_vertexDescriptions, &m_pipeline ) );

	return VK_SUCCESS;
}

void MeshRenderable::SetVertexDescriptions( const std::vector<VkVertexInputAttributeDescription>& vertexDescriptions )
{
	m_vertexDescriptions = vertexDescriptions;
}

void MeshRenderable::AddLodRenderable( MeshLodRenderable&& lodRenderable )
{
	m_lods.push_back( lodRenderable );
}

void MeshRenderable::SetTopology( VkPrimitiveTopology topology )
{
	m_topology = topology;
}

void MeshRenderable::SetLineWidth( float lineWidth )
{
	m_lineWidth = lineWidth;
}