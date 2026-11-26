// Copyright © 2026 CCP ehf.

#include "mesh.h"

#include "../models/boundingBox.h"
#include "../models/axis.h"
#include "../renderer.h"
#include "../vulkan/vulkanerrors.h"
#include "../models/primitiveEffects.h"


MeshRenderable::MeshRenderable( std::shared_ptr<CmfContent> data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_cmfMesh( cmfMesh ),
	m_modelEffect( renderer ),
	m_wireframeEffect( renderer ),
	m_prepass( renderer, data, cmfMesh ),
	m_audioOcclusionRenderable( renderer, GetAudioOcclusionEffect( renderer, cmfMesh ) ),
	m_boundingBox( BoundingBox::Create( renderer, Vector3( 0.5, 0.5, 0.0 ) ) )
{
	m_boundingSphere = CcpMath::Sphere( m_cmfMesh.bounds );

	m_boundingBoxTransform = ScalingMatrix( m_cmfMesh.bounds.Size() ) * TranslationMatrix( m_cmfMesh.bounds.Center() );
	m_stride = m_cmfMesh.lods[0].vb.stride;

	if( m_cmfMesh.skeleton != 0xFF && m_cmfMesh.skeleton < data->m_cmfData->skeletons.size() )
	{
		const auto& skeleton = data->m_cmfData->skeletons[m_cmfMesh.skeleton];
		for( const auto& boneBinding : m_cmfMesh.boneBindings )
		{
			m_boneBindingToBoneIndexMapping.push_back( 0xFF );
			auto it = std::find_if( skeleton.bones.begin(), skeleton.bones.end(), [boneBinding]( const cmf::String& name ) {
				return name == boneBinding.name;
			} );
			if( it != skeleton.bones.end() )
			{
				m_boneBindingToBoneIndexMapping.back() = static_cast<uint32_t>( std::distance( skeleton.bones.begin(), it ) );
			}
		}
	}

	m_polygonMode = m_cmfMesh.topology == cmf::MeshTopology::TriangleList ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_POINT;
	m_topology = m_cmfMesh.topology == cmf::MeshTopology::TriangleList ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

void MeshRenderable::Initialize()
{
	m_prepass.Initialize();

	m_areaDisplay.assign( m_cmfMesh.areas.size(), true );
	SetLod( 0 );
	for( const auto& vertexElement : m_cmfMesh.decl )
	{
		m_availableVertexElements.push_back( vertexElement );
		if( vertexElement.usage == cmf::Usage::Normal )
		{
			m_normalAxisUsages.push_back( vertexElement.usageIndex );
			m_normalAxisRenderables.push_back( Axis::CreateNormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
		}
		else if( vertexElement.usage == cmf::Usage::Tangent )
		{
			m_tangentAxisUsages.push_back( vertexElement.usageIndex );
			m_tangentAxisRenderables.push_back( Axis::CreateTangent( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
		}
		else if( vertexElement.usage == cmf::Usage::Binormal )
		{
			m_binormalAxisUsages.push_back( vertexElement.usageIndex );
			m_binormalAxisRenderables.push_back( Axis::CreateBinormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
		}
		else if( vertexElement.usage == cmf::Usage::PackedTangent )
		{
			m_normalAxisUsages.push_back( vertexElement.usageIndex );
			m_tangentAxisUsages.push_back( vertexElement.usageIndex );
			m_binormalAxisUsages.push_back( vertexElement.usageIndex );

			m_normalAxisRenderables.push_back( Axis::CreatePackedNormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			m_tangentAxisRenderables.push_back( Axis::CreatePackedTangent( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			m_binormalAxisRenderables.push_back( Axis::CreatePackedBinormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
		}
		else if( vertexElement.usage == cmf::Usage::PackedTangentLegacy )
		{
			m_normalAxisUsages.push_back( vertexElement.usageIndex );
			m_tangentAxisUsages.push_back( vertexElement.usageIndex );
			m_binormalAxisUsages.push_back( vertexElement.usageIndex );

			m_normalAxisRenderables.push_back( Axis::CreatePackedLegacyNormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			m_tangentAxisRenderables.push_back( Axis::CreatePackedLegacyTangent( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			m_binormalAxisRenderables.push_back( Axis::CreatePackedLegacyBinormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
		}
	}
	m_showNormals.assign( m_normalAxisUsages.size(), false );
	m_showTangents.assign( m_tangentAxisUsages.size(), false );
	m_showBinormals.assign( m_binormalAxisUsages.size(), false );

	m_boundingBox.Initialize();

	if( !m_cmfMesh.audioOcclusionMesh.vertices.empty() && !m_cmfMesh.audioOcclusionMesh.indices.empty() )
	{
		m_audioOcclusionRenderable.SetBufferData(
			reinterpret_cast<const uint8_t*>( m_cmfMesh.audioOcclusionMesh.vertices.data() ),
			uint32_t( m_cmfMesh.audioOcclusionMesh.vertices.size() * sizeof( Vector3 ) ),
			sizeof( Vector3 ) );
		m_audioOcclusionRenderable.SetIndexData(
			reinterpret_cast<const uint8_t*>( m_cmfMesh.audioOcclusionMesh.indices.data() ),
			uint32_t( m_cmfMesh.audioOcclusionMesh.indices.size() * sizeof( uint16_t ) ),
			sizeof( uint16_t ) );
		m_audioOcclusionRenderable.Initialize();
	}

	for( auto& normalAxisRenderable : m_normalAxisRenderables )
	{
		normalAxisRenderable.Initialize();
	}
	for( auto& tangentAxisRenderable : m_tangentAxisRenderables )
	{
		tangentAxisRenderable.Initialize();
	}
	for( auto& binormalAxisRenderable : m_binormalAxisRenderables )
	{
		binormalAxisRenderable.Initialize();
	}
	m_initialized = true;
}

void MeshRenderable::SetAnimation( const cmf::Animation* animation )
{
	m_morphCurveToTargetMapping.clear();
	if( !animation )
	{
		return;
	}

	for( const auto& channel : animation->channels )
	{
		if( channel.targetType == cmf::AnimationChannelTargetType::MorphTarget )
		{
			auto* morphTarget = std::find_if( m_cmfMesh.morphTargets.targets.begin(), m_cmfMesh.morphTargets.targets.end(), [&channel]( const cmf::MorphTarget& morphTarget ) {
				return morphTarget.name == channel.target;
			} );
			if( morphTarget != m_cmfMesh.morphTargets.targets.end() )
			{
				const auto morphIndex = static_cast<uint32_t>( std::distance( m_cmfMesh.morphTargets.targets.begin(), morphTarget ) );
				m_morphCurveToTargetMapping.emplace_back( channel.curveIndex, morphIndex );
			}
		}
	}
}

void MeshRenderable::UpdateMeshCurves( float animationTime, const cmf::Animation* animation, const MorphWeightCallback& onMorphWeight )
{
	if( animation )
	{
		for( const auto& [curveIndex, morphIndex] : m_morphCurveToTargetMapping )
		{
			float weight = cmf::SampleScalarCurve( animation->curves[curveIndex], animationTime );

			m_prepass.SetMorphWeight( morphIndex, weight );
			// let the owner know so that it can reflect the current weight (e.g. in the UI)
			if( onMorphWeight )
			{
				onMorphWeight( morphIndex, weight );
			}
		}
	}
}

void MeshRenderable::SetSkeletonPose( const std::array<Matrix, 0xFF>& boneTransforms )
{
	std::array<Matrix, 0xFF> mappedBoneTransforms;
	mappedBoneTransforms.fill( IdentityMatrix() );

	uint32_t index = 0;
	for( const auto& boneIndex : m_boneBindingToBoneIndexMapping )
	{
		if( boneIndex != 0xFF )
		{
			mappedBoneTransforms[index] = boneTransforms[boneIndex];
		}
		else
		{
			mappedBoneTransforms[index] = IdentityMatrix();
		}
		++index;
	}

	m_prepass.SetSkeletonPose( mappedBoneTransforms );
}

void MeshRenderable::SetLod( uint32_t lodLevel )
{
	if( lodLevel == m_currentLod )
	{
		return;
	}
	// find the lod that is closest to the asked lodLevel, in the unlikely scenario that a model has multiple meshes with different lod levels
	if( lodLevel >= m_cmfMesh.lods.size() && !m_cmfMesh.lods.empty() )
	{
		lodLevel = (uint32_t)m_cmfMesh.lods.size() - 1;
	}

	const auto& cmfLod = m_cmfMesh.lods[lodLevel];
	m_areas.clear();
	for( const auto& area : cmfLod.areas )
	{
		m_areas.push_back( { area.firstElement * 3, area.elementCount * 3 } );
	}
	m_prepass.SetLod( lodLevel );
	m_currentLod = lodLevel;
}

uint32_t MeshRenderable::GetCurrentLod() const
{
	return m_currentLod;
}

float MeshRenderable::GetScreenSize() const
{
	return m_screenSize;
}

void MeshRenderable::SetVisible( bool visible )
{
	m_display = visible;
}

void MeshRenderable::SetAreaVisible( size_t areaIndex, bool visible )
{
	if( areaIndex < m_areaDisplay.size() )
	{
		m_areaDisplay[areaIndex] = visible;
	}
}

void MeshRenderable::SetWireframe( bool enabled )
{
	m_wireframe = enabled;
}

void MeshRenderable::SetAudioOcclusion( bool enabled )
{
	m_audioOcclusion = enabled;
}

void MeshRenderable::SetShowBoundingBox( bool enabled )
{
	m_showBoundingBox = enabled;
}

void MeshRenderable::SetMorphWeight( uint32_t morphIndex, float weight )
{
	m_prepass.SetMorphWeight( morphIndex, weight );
}

const std::vector<uint32_t>& MeshRenderable::GetNormalAxisUsages() const
{
	return m_normalAxisUsages;
}

const std::vector<uint32_t>& MeshRenderable::GetTangentAxisUsages() const
{
	return m_tangentAxisUsages;
}

const std::vector<uint32_t>& MeshRenderable::GetBinormalAxisUsages() const
{
	return m_binormalAxisUsages;
}

void MeshRenderable::SetShowNormals( size_t index, bool enabled )
{
	if( index < m_showNormals.size() )
	{
		m_showNormals[index] = enabled;
	}
}

void MeshRenderable::SetShowTangents( size_t index, bool enabled )
{
	if( index < m_showTangents.size() )
	{
		m_showTangents[index] = enabled;
	}
}

void MeshRenderable::SetShowBinormals( size_t index, bool enabled )
{
	if( index < m_showBinormals.size() )
	{
		m_showBinormals[index] = enabled;
	}
}

const cmf::Mesh& MeshRenderable::GetCmfMesh() const
{
	return m_cmfMesh;
}

void MeshRenderable::Update( const Camera& camera, bool autoLod )
{
	if( !m_initialized )
	{
		return;
	}

	if( autoLod )
	{
		// update the lod based on the camera and bounding sphere of the mesh
		auto sizeOnScreen = camera.GetSizeOnScreen( m_boundingSphere );
		m_screenSize = sizeOnScreen;
		// find the closest lod that has the size on screen greater than the threshold
		uint32_t lodLevel = 0;

		for( lodLevel = (uint32_t)m_cmfMesh.lods.size() - 1; lodLevel > 0; --lodLevel )
		{
			if( sizeOnScreen <= m_cmfMesh.lods[lodLevel].threshold )
			{
				break;
			}
		}
		if( m_currentLod != lodLevel )
		{
			SetLod( lodLevel );
		}
	}
}

void MeshRenderable::Render( GraphicsCommandBuffer& commandBuffer, const Camera& camera )
{
	auto viewProj = GraphicsEffect::VertexUboData{ camera.GetProjection(), camera.GetView() };

	auto vertexBuffer = m_prepass.GetVertexBuffer();
	const Buffer indexBuffer = m_prepass.GetIndexBuffer();

	if( m_display && m_modelEffect.IsInitialized() )
	{
		commandBuffer.BindVertexBuffer( vertexBuffer.GetGpuBuffer() );
		if( indexBuffer.IsValid() )
		{
			commandBuffer.BindIndexBuffer( m_prepass.GetIndexBuffer() );
		}

		m_modelEffect.SetUniformData( 0, viewProj );

		commandBuffer.BindEffect( m_modelEffect );
		if( indexBuffer.IsValid() )
		{
			DrawIndexed( commandBuffer );
		}
		else
		{
			Draw( commandBuffer );
		}
	}
	if( m_polygonMode != VK_POLYGON_MODE_LINE && m_wireframeEffect.IsInitialized() && m_wireframe )
	{
		m_wireframeEffect.SetUniformData( 0, viewProj );
		commandBuffer.BindVertexBuffer( vertexBuffer.GetGpuBuffer() );
		commandBuffer.BindEffect( m_wireframeEffect );
		if( indexBuffer.IsValid() )
		{
			commandBuffer.BindIndexBuffer( m_prepass.GetIndexBuffer() );
			DrawIndexed( commandBuffer );
		}
		else
		{
			Draw( commandBuffer );
		}
	}
}

void MeshRenderable::RenderDebug( GraphicsCommandBuffer& commandBuffer, const Camera& camera )
{
	auto viewProj = GraphicsEffect::VertexUboData{ camera.GetProjection(), camera.GetView() };
	if( m_showBoundingBox )
	{
		auto vertexData = PrimitiveEffects::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform, Vector4() };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}

	if( m_audioOcclusion && !m_cmfMesh.audioOcclusionMesh.vertices.empty() && !m_cmfMesh.audioOcclusionMesh.indices.empty() )
	{
		m_audioOcclusionRenderable.SetUniformData( 0, viewProj );
		m_audioOcclusionRenderable.Render( commandBuffer );
	}
	auto streamElementCount = cmf::GetStreamElementCount( m_cmfMesh.lods[m_currentLod].vb );
	const auto* buffer = &( m_prepass.GetVertexBuffer() );
	// the show flags and renderables are created in the same order based on the vertex elements
	for( size_t index = 0; index < m_showNormals.size(); ++index )
	{
		if( m_showNormals[index] )
		{
			m_normalAxisRenderables[index].SetUniformData( 0, viewProj );
			m_normalAxisRenderables[index].Render( commandBuffer, buffer, nullptr, 2, streamElementCount );
		}
	}
	for( size_t index = 0; index < m_showTangents.size(); ++index )
	{
		if( m_showTangents[index] )
		{
			m_tangentAxisRenderables[index].SetUniformData( 0, viewProj );
			m_tangentAxisRenderables[index].Render( commandBuffer, buffer, nullptr, 2, streamElementCount );
		}
	}
	for( size_t index = 0; index < m_showBinormals.size(); ++index )
	{
		if( m_showBinormals[index] )
		{
			m_binormalAxisRenderables[index].SetUniformData( 0, viewProj );
			m_binormalAxisRenderables[index].Render( commandBuffer, buffer, nullptr, 2, streamElementCount );
		}
	}
}

void MeshRenderable::PrepareMesh( ComputeCommandBuffer& commandBuffer )
{
	if( !m_display )
	{
		return;
	}

	m_prepass.Process( commandBuffer );
}

void MeshRenderable::Draw( GraphicsCommandBuffer& commandBuffer )
{
	for( uint32_t i = 0; i < m_areas.size(); i++ )
	{
		if( !IsAreaVisible( i ) )
		{
			continue;
		}
		auto area = m_areas[i];
		commandBuffer.Draw( area.firstElement, area.elementCount );
	}
}

bool MeshRenderable::IsAreaVisible( size_t areaIndex ) const
{
	return areaIndex >= m_areaDisplay.size() || m_areaDisplay[areaIndex];
}

void MeshRenderable::DrawIndexed( GraphicsCommandBuffer& commandBuffer )
{
	for( uint32_t i = 0; i < m_areas.size(); i++ )
	{
		if( !IsAreaVisible( i ) )
		{
			continue;
		}
		auto area = m_areas[i];
		commandBuffer.DrawIndexed( area.firstElement, area.elementCount );
	}
}

VkResult MeshRenderable::SetRenderingMode( std::string shaderName, GraphicsEffectTypes::ShaderInputDeclaration shaderInputDeclaration )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();

	m_shaderName = shaderName;

	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );

	auto config = GraphicsEffectTypes::Config();
	config.topology = m_topology;
	config.polygonMode = m_polygonMode;
	config.cullMode = ( m_polygonMode == VK_POLYGON_MODE_FILL ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	config.inputDeclaration.vertexDeclarations = shaderInputDeclaration.vertexDeclarations;
	config.inputDeclaration.stride = m_stride;

	m_modelEffect.Reset( m_shaderName, config );

	if( !m_modelEffect.IsInitialized() )
	{
		m_modelEffect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_modelEffect.Initialize();
	}

	if( !m_wireframeEffect.IsInitialized() )
	{
		auto wireframeConfig = GraphicsEffectTypes::Config();
		wireframeConfig.topology = m_topology;
		// use fill mode even though we are rendering wireframe
		// The reason is when we rasterize the lines we will get issues with the depth buffer where some lines
		// will fail the depth test and not get rendered.
		// We use barycentric coordinates in the shader to discard pixels that are not on the wireframe edges.
		wireframeConfig.polygonMode = VK_POLYGON_MODE_FILL;
		wireframeConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		wireframeConfig.cullMode = VK_CULL_MODE_NONE;
		wireframeConfig.blend = true;
		wireframeConfig.inputDeclaration.stride = m_stride;
		wireframeConfig.inputDeclaration.vertexDeclarations = { { cmf::Usage::Position,
																  0,
																  cmf::ElementType::Float32,
																  3,
																  0 } };

		m_wireframeEffect.Reset( "wireframeoverlay", wireframeConfig );
		m_wireframeEffect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_wireframeEffect.Initialize();
	}
	return VK_SUCCESS;
}

GraphicsEffect MeshRenderable::GetAudioOcclusionEffect( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& cmfMesh )
{
	auto config = GraphicsEffectTypes::Config();
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.polygonMode = VK_POLYGON_MODE_FILL;
	config.cullMode = VK_CULL_MODE_NONE;
	config.blend = false;
	config.inputDeclaration.stride = sizeof( Vector3 );
	config.inputDeclaration.vertexDeclarations = {
		cmf::VertexElement{
			cmf::Usage::Position,
			0,
			cmf::ElementType::Float32,
			3,
			0 }
	};

	GraphicsEffect effect( renderer );
	effect.Reset( "Face Normal", config );
	effect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	return effect;
}

uint8_t MeshRenderable::GetSkeletonIndex() const
{
	return m_cmfMesh.skeleton;
}