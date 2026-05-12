#include "geometryprepass.h"

#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include "rendering/vulkan/vulkanerrors.h"
#include <numeric>

GeometryPrePass::GeometryPrePass( std::shared_ptr<const Renderer> renderer, CmfContent* cmfContent, const cmf::Mesh& cmfMesh ) :
	m_cmfMesh( cmfMesh ),
	m_cmfContent( cmfContent ),
	m_renderer( renderer ),
	m_effect( renderer )
{
	bool hasMorphs = !m_cmfMesh.morphTargets.targets.empty();
	bool hasAnimations = !m_cmfMesh.boneBindings.empty();
	m_mode = hasMorphs || hasAnimations ? Mode::DynamicMesh : Mode::StaticMesh;
}

GeometryPrePass::~GeometryPrePass()
{
}

void GeometryPrePass::Initialize( AppState& appState )
{
	auto firstMorphState = appState.morphTargetEnabled.size();

	// go through the morph targets so we can register to the morph weight and display flags
	for( size_t i = 0; i < m_cmfMesh.morphTargets.targets.size(); i++ )
	{
		auto index = appState.morphTargetEnabled.AddState();
		appState.morphTargetWeight.AddState();

		appState.morphTargetWeight[index].RegisterCallback( [this, index, firstMorphState]( float weight, AppState& ) {
			UpdateMorphWeights( index - firstMorphState, weight );
		} );
		appState.morphTargetEnabled[index].RegisterCallback( [this, index, firstMorphState]( bool enabled, AppState& appState ) {
			float weight = enabled ? appState.morphTargetWeight[index].GetValue() : 0.0f;
			UpdateMorphWeights( index - firstMorphState, weight );
		} );
	}

	if( m_mode == Mode::DynamicMesh )
	{
		m_weights.resize( m_cmfMesh.morphTargets.targets.size(), 0.0f );
	}

	UpdateLod( appState.selectedLod.GetValue() );
}

void GeometryPrePass::Process( ComputeCommandBuffer& commandBuffer )
{
	if( m_mode == Mode::StaticMesh )
	{
		return;
	}

	auto vkCommandBuffer = commandBuffer.GetActiveCommandBuffer();

	m_effect.SetUniformData( 0, m_ubo );
	commandBuffer.BindEffect( m_effect );

	IssueBarrier( vkCommandBuffer, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	commandBuffer.Dispatch( m_ubo.vertexCount, 1, 1 );
	IssueBarrier( vkCommandBuffer,
				  VK_ACCESS_SHADER_WRITE_BIT, // src: flush compute writes
				  VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, // dst: make visible to vertex reads
				  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // src stage: compute
				  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT ); // dst stage: vertex input
}

void GeometryPrePass::IssueBarrier( VkCommandBuffer& commandBuffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask )
{
	VkBufferMemoryBarrier bufferBarrier{};
	bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

	bufferBarrier.srcAccessMask = srcAccessMask;
	bufferBarrier.dstAccessMask = dstAccessMask;

	uint32_t srcFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	uint32_t destFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	if( m_renderer->HasDedicatedComputeQueue() )
	{
		const bool isSrcGraphics = ( srcStageMask == VK_PIPELINE_STAGE_VERTEX_INPUT_BIT );
		const bool isDstGraphics = ( dstStageMask == VK_PIPELINE_STAGE_VERTEX_INPUT_BIT );

		auto graphicsFamilyIndex = m_renderer->GetDevice()->GetQueueFamilyIndices().graphicsFamily.value();
		auto computeFamilyIndex = m_renderer->GetDevice()->GetQueueFamilyIndices().computeFamily.value();
		srcFamilyIndex = isSrcGraphics ? graphicsFamilyIndex : computeFamilyIndex;
		destFamilyIndex = isDstGraphics ? graphicsFamilyIndex : computeFamilyIndex;
	}

	bufferBarrier.srcQueueFamilyIndex = srcFamilyIndex;
	bufferBarrier.dstQueueFamilyIndex = destFamilyIndex;
	bufferBarrier.size = VK_WHOLE_SIZE;

	bufferBarrier.buffer = m_computeOutBuffer.GetGpuBuffer();
	vkCmdPipelineBarrier( commandBuffer,
						  srcStageMask,
						  dstStageMask,
						  0,
						  0,
						  nullptr,
						  1,
						  &bufferBarrier,
						  0,
						  nullptr );
}

void GeometryPrePass::UpdateMorphWeights( size_t morphIndex, float weight )
{
	uint32_t morphCount = (uint32_t)m_cmfMesh.morphTargets.targets.size();
	if( morphIndex >= morphCount )
	{
		Log::Error( "UpdateMorphWeights: Morph index %zu is out of range, morph count is %u", morphIndex, morphCount );
		return;
	}
	m_weights[morphIndex] = weight;
	m_weightBuffer.SetData( m_renderer.get(), (const uint8_t*)m_weights.data(), (uint32_t)( m_weights.size() * sizeof( float ) ) );
}

void GeometryPrePass::UpdateLod( uint32_t lodIndex )
{
	m_currentLod = lodIndex;

	auto lod = m_cmfMesh.lods[m_currentLod];
	if( lod.morphTargets.empty() )
	{
		m_mode = Mode::StaticMesh;
	}
	else
	{
		m_mode = Mode::DynamicMesh;
	}

	if( m_mode == Mode::StaticMesh )
	{
		SetupForStaticMesh();
	}
	else
	{
		SetupForDynamicMesh();
	}
}

void GeometryPrePass::SetupForStaticMesh()
{
	auto lod = m_cmfMesh.lods[m_currentLod];
	const uint8_t* vertexData = m_cmfContent->Index( lod.vb.index, lod.vb.offset );

	const uint8_t* indexData = nullptr;
	if( lod.ib.size > 0 )
	{
		indexData = m_cmfContent->Index( lod.ib.index, lod.ib.offset );
	}

	VkCommandBuffer copyCmd = VK_NULL_HANDLE;
	CR( m_renderer->CreateCopyCommandBuffer( &copyCmd ) );
	m_vertexBuffer.Initialize( m_renderer.get(), BufferType::Vertex, vertexData, lod.vb.size, lod.vb.stride );
	m_vertexBuffer.CopyFromStaging( copyCmd );

	if( indexData != nullptr )
	{
		m_indexBuffer.Initialize( m_renderer.get(), BufferType::Index, indexData, lod.ib.size, lod.ib.stride );
		m_indexBuffer.CopyFromStaging( copyCmd );
	}
	m_renderer->EndCopyCommandBuffer( copyCmd );

	// release the staging buffers for constant buffers
	m_vertexBuffer.ReleaseStaging( m_renderer.get() );
	m_morphTargetBuffer.ReleaseStaging( m_renderer.get() );

	if( indexData != nullptr )
	{
		m_indexBuffer.ReleaseStaging( m_renderer.get() );
	}
}

void GeometryPrePass::SetupForDynamicMesh()
{
	auto lod = m_cmfMesh.lods[m_currentLod];

	m_morphJobs.clear();

	const uint8_t* vertexData = m_cmfContent->Index( lod.vb.index, lod.vb.offset );

	const uint8_t* indexData = nullptr;
	if( lod.ib.size > 0 )
	{
		indexData = m_cmfContent->Index( lod.ib.index, lod.ib.offset );
	}

	// Gather all the morph target data
	std::vector<uint8_t> morphTargetData{};
	const size_t totalMorphTargetDataSize = (size_t)std::accumulate( lod.morphTargets.begin(), lod.morphTargets.end(), (size_t)0, []( size_t sum, const cmf::LodMorphTarget& morphTarget ) {
		return sum + (size_t)morphTarget.vb.size;
	} );
	morphTargetData.resize( totalMorphTargetDataSize );

	uint32_t morphTargetDataOffset = 0;
	std::vector<Element> elements{};

	const uint32_t morphTargetStride = (uint32_t)std::accumulate( m_cmfMesh.morphTargets.decl.begin(), m_cmfMesh.morphTargets.decl.end(), (uint32_t)0, []( uint32_t currentStride, const cmf::VertexElement& decl ) {
		return currentStride + cmf::GetVertexElementSize( decl );
	} );

	elements.reserve( m_cmfMesh.decl.size() + m_cmfMesh.morphTargets.decl.size() );

	for( const auto vertexDecl : m_cmfMesh.decl )
	{
		bool normalized = vertexDecl.usage == cmf::Usage::Normal ||
			vertexDecl.usage == cmf::Usage::Tangent ||
			vertexDecl.usage == cmf::Usage::Binormal ||
			vertexDecl.usage == cmf::Usage::PackedTangent ||
			vertexDecl.usage == cmf::Usage::PackedTangentLegacy;
		elements.push_back( { (uint32_t)vertexDecl.type, (uint32_t)vertexDecl.elementCount, vertexDecl.offset / 4, normalized } );
	}

	for( const auto morphTargetDecl : m_cmfMesh.morphTargets.decl )
	{
		// no need to normalize the morph target data
		elements.push_back( { (uint32_t)morphTargetDecl.type, (uint32_t)morphTargetDecl.elementCount, morphTargetDecl.offset / 4, false } );
	}

	// gather all the morph jobs for this morph target, one job per vertex element
	for( const auto morphTargetDecl : m_cmfMesh.morphTargets.decl )
	{
		const auto vertexDecl = std::find_if( m_cmfMesh.decl.begin(), m_cmfMesh.decl.end(), [&morphTargetDecl]( const cmf::VertexElement& vertexDecl ) {
			return vertexDecl.usage == morphTargetDecl.usage && vertexDecl.usageIndex == morphTargetDecl.usageIndex && vertexDecl.elementCount == morphTargetDecl.elementCount;
		} );

		if( vertexDecl == m_cmfMesh.decl.end() )
		{
			throw std::runtime_error( "Morph target declaration for usage " + std::to_string( static_cast<uint32_t>( morphTargetDecl.usage ) ) + " " + std::to_string( morphTargetDecl.usageIndex ) + " does not have a matching vertex declaration" );
		}

		auto sourceVertexDeclIndex = (uint32_t)std::distance( m_cmfMesh.decl.begin(), vertexDecl );

		m_morphJobs.push_back(
			{
				sourceVertexDeclIndex,
				(uint32_t)( m_cmfMesh.decl.size() + m_morphJobs.size() ),
			} );
	}

	for( uint32_t morphIndex = 0; morphIndex < lod.morphTargets.size(); ++morphIndex )
	{
		const auto& morphTarget = lod.morphTargets[morphIndex];

		const uint8_t* morphData = m_cmfContent->Index( morphTarget.vb.index, morphTarget.vb.offset );
		memcpy( morphTargetData.data() + morphTargetDataOffset, morphData, morphTarget.vb.size );
		morphTargetDataOffset += morphTarget.vb.size;
	}

	VkCommandBuffer copyCmd = VK_NULL_HANDLE;
	CR( m_renderer->CreateCopyCommandBuffer( &copyCmd ) );
	m_vertexBuffer.Initialize( m_renderer.get(), BufferType::Storage, vertexData, lod.vb.size, sizeof( float ) );

	if( indexData != nullptr )
	{
		m_indexBuffer.Initialize( m_renderer.get(), BufferType::Index, indexData, lod.ib.size, lod.ib.stride );
		m_indexBuffer.CopyFromStaging( copyCmd );
	}

	// just fill the compute output buffer with the original vertex data for now, the compute shader will read from the vertex buffer and write the morphed vertices back to this buffer
	m_computeOutBuffer.Initialize( m_renderer.get(), BufferType::Storage, vertexData, lod.vb.size, sizeof( float ) );

	m_morphTargetBuffer.Initialize( m_renderer.get(), BufferType::Storage, morphTargetData.data(), (uint32_t)( morphTargetData.size() ), sizeof( float ) );

	m_morphJobBuffer.Initialize( m_renderer.get(), BufferType::Storage, (const uint8_t*)m_morphJobs.data(), (uint32_t)( m_morphJobs.size() * sizeof( MorphJob ) ), sizeof( MorphJob ) );
	m_elementBuffer.Initialize( m_renderer.get(), BufferType::Storage, (const uint8_t*)elements.data(), (uint32_t)( elements.size() * sizeof( Element ) ), sizeof( Element ) );
	m_weightBuffer.Initialize( m_renderer.get(), BufferType::Storage, (const uint8_t*)m_weights.data(), (uint32_t)( m_weights.size() * sizeof( float ) ), sizeof( float ) );
	m_renderer->EndCopyCommandBuffer( copyCmd );

	// release the staging buffers for constant buffers
	if( indexData != nullptr )
	{
		m_indexBuffer.ReleaseStaging( m_renderer.get() );
	}

	m_ubo = {
		(uint32_t)m_morphJobs.size(),
		(uint32_t)( lod.vb.size / lod.vb.stride ),
		lod.vb.stride / 4,
		(uint32_t)lod.morphTargets.size(),
		lod.morphTargets[0].vb.size / 4,
		morphTargetStride / 4
	};

	// recreate the effect, the buffers may have changed
	m_effect = ComputeEffect( m_renderer );
	m_effect.SetShaderName( "geometryprepass" );
	m_effect.RegisterUniformData<GeoPrepassUBO>( VK_SHADER_STAGE_COMPUTE_BIT, 0 ); // uniform buffer
	m_effect.RegisterStorageBuffer( VK_SHADER_STAGE_COMPUTE_BIT, 1, &m_vertexBuffer ); // input vertex buffer
	m_effect.RegisterStorageBuffer( VK_SHADER_STAGE_COMPUTE_BIT, 2, &m_computeOutBuffer ); // output vertex buffer
	m_effect.RegisterStorageBuffer( VK_SHADER_STAGE_COMPUTE_BIT, 3, &m_morphTargetBuffer ); // morphTarget buffer
	m_effect.RegisterStorageBuffer( VK_SHADER_STAGE_COMPUTE_BIT, 4, &m_morphJobBuffer ); // job buffer
	m_effect.RegisterStorageBuffer( VK_SHADER_STAGE_COMPUTE_BIT, 5, &m_elementBuffer ); // element buffer
	m_effect.RegisterStorageBuffer( VK_SHADER_STAGE_COMPUTE_BIT, 6, &m_weightBuffer ); // weight buffer

	m_effect.Initialize();
}

const Buffer& GeometryPrePass::GetVertexBuffer() const
{
	if( m_mode == Mode::StaticMesh )
	{
		return m_vertexBuffer;
	}
	return m_computeOutBuffer;
}

const Buffer& GeometryPrePass::GetIndexBuffer() const
{
	return m_indexBuffer;
}