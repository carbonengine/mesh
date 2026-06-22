// Copyright © 2026 CCP ehf.

#include "mesh.h"

#include "../models/boundingBox.h"
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
	for( const auto& vertexElement : m_cmfMesh.decl )
	{
		m_availableVertexElements.push_back( vertexElement );
	}
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
}

void MeshRenderable::Initialize( AppState& appState )
{
	appState.modelState.polygonMode.RegisterCallback( [this]( VkPolygonMode mode, AppState& appState ) {
		SetRenderingMode( appState.modelState.visualizationShader.GetValue(), mode );
	} );

	appState.modelState.visualizationShader.RegisterCallback( [this]( std::string shaderName, AppState& appState ) {
		SetRenderingMode( shaderName, appState.modelState.polygonMode.GetValue() );
	} );

	// Register mesh visibility state
	size_t stateIndex = appState.modelState.meshVisibilityStates.AddState();
	appState.modelState.meshVisibilityStates[stateIndex].RegisterCallback( [this]( bool visible, AppState& appState ) {
		m_display = visible;
	} );

	stateIndex = appState.modelState.meshWireframeOverlay.AddState();
	appState.modelState.meshWireframeOverlay[stateIndex].RegisterCallback( [this]( bool enabled, AppState& appState ) {
		m_wireframe = enabled;
	} );

	stateIndex = appState.modelState.audioOcclusionMesh.AddState();
	appState.modelState.audioOcclusionMesh[stateIndex].RegisterCallback( [this]( bool enabled, AppState& appState ) {
		m_audioOcclusion = enabled;
	} );

	stateIndex = appState.modelState.meshBoundingBox.AddState();
	appState.modelState.meshBoundingBox[stateIndex].RegisterCallback( [this]( bool enabled, AppState& appState ) {
		m_showBoundingBox = enabled;
	} );

	m_meshScreenSizeStateIndex = appState.modelState.meshScreenSize.AddState();
	m_activeLodStateIndex = appState.modelState.activeLod.AddState();

	appState.modelState.activeLod[m_activeLodStateIndex].RegisterCallback( [this]( uint32_t lodIndex, AppState& appState ) {
		SetLod( lodIndex );
	} );

	appState.modelState.selectedLod.RegisterCallback( [this]( int32_t lodIndex, AppState& appState ) {
		if( lodIndex != -1 )
		{
			appState.modelState.activeLod[m_activeLodStateIndex].SetValue( lodIndex );
		}
	} );

	m_prepass.Initialize( appState );

	SetLod( appState.modelState.activeLod[m_activeLodStateIndex].GetValue() );

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
	m_initialized = true;
}

void MeshRenderable::UpdateMeshCurves( float animationTime, const cmf::Animation* animation, AppState& appState )
{
	if( animation )
	{
		for( const auto& [curveIndex, morphIndex] : m_morphCurveToTargetMapping )
		{
			float weight = cmf::SampleScalarCurve( animation->curves[curveIndex], animationTime );

			m_prepass.SetMorphWeight( morphIndex, weight );
			// the line above is the one that updates the morph target weight in the prepass,
			// but we also need to update the app state so that the UI reflects the current weight
			// if we would do it the other way, then prepass would get the update with one frame delay
			appState.modelState.morphTargetWeight[morphIndex].SetValueNoCallback( weight );
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

void MeshRenderable::Update( AppState& appState, const Camera& camera )
{
	if( !m_initialized )
	{
		return;
	}

	if( appState.modelState.selectedLod.GetValue() < 0 )
	{
		// update the lod based on the camera and bounding sphere of the mesh
		auto sizeOnScreen = camera.GetSizeOnScreen( m_boundingSphere );
		appState.modelState.meshScreenSize[m_meshScreenSizeStateIndex].SetValueNoCallback( sizeOnScreen );
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
			if( m_activeLodStateIndex < appState.modelState.activeLod.size() )
			{
				appState.modelState.activeLod[m_activeLodStateIndex].SetValueNoCallback( lodLevel );
			}
		}
	}
}

void MeshRenderable::Render( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
{
	auto viewProj = VertexUboData{ camera.GetProjection(), camera.GetView() };

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

void MeshRenderable::RenderDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
{
	if( m_showBoundingBox )
	{
		auto vertexData = PrimitiveEffects::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform, Vector4() };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}

	if( m_audioOcclusion && !m_cmfMesh.audioOcclusionMesh.vertices.empty() && !m_cmfMesh.audioOcclusionMesh.indices.empty() )
	{
		auto viewProj = VertexUboData{ camera.GetProjection(), camera.GetView() };
		m_audioOcclusionRenderable.SetUniformData( 0, viewProj );
		m_audioOcclusionRenderable.Render( commandBuffer );
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
		auto area = m_areas[i];
		commandBuffer.Draw( area.firstElement, area.elementCount );
	}
}

void MeshRenderable::DrawIndexed( GraphicsCommandBuffer& commandBuffer )
{
	for( uint32_t i = 0; i < m_areas.size(); i++ )
	{
		auto area = m_areas[i];
		commandBuffer.DrawIndexed( area.firstElement, area.elementCount );
	}
}

VkResult MeshRenderable::SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();

	m_polygonMode = polygonMode;
	m_shaderName = shaderName;

	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );

	auto config = GraphicsEffect::Config();
	config.topology = m_topology;
	config.polygonMode = polygonMode;
	config.cullMode = ( polygonMode == VK_POLYGON_MODE_FILL ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	config.availableVertexElements = m_availableVertexElements;
	config.stride = m_stride;

	m_modelEffect.SetShaderName( m_shaderName );
	m_modelEffect.SetConfig( config );
	if( !m_modelEffect.IsInitialized() )
	{
		m_modelEffect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_modelEffect.Initialize();
	}

	if( !m_wireframeEffect.IsInitialized() )
	{
		auto wireframeConfig = GraphicsEffect::Config();
		wireframeConfig.topology = m_topology;
		// use fill mode even though we are rendering wireframe
		// The reason is when we rasterize the lines we will get issues with the depth buffer where some lines
		// will fail the depth test and not get rendered.
		// We use barycentric coordinates in the shader to discard pixels that are not on the wireframe edges.
		wireframeConfig.polygonMode = VK_POLYGON_MODE_FILL;
		wireframeConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		wireframeConfig.cullMode = VK_CULL_MODE_NONE;
		wireframeConfig.blend = true;
		wireframeConfig.availableVertexElements = m_availableVertexElements;
		wireframeConfig.stride = m_stride;

		m_wireframeEffect.SetShaderName( "wireframeoverlay" );
		m_wireframeEffect.SetConfig( wireframeConfig );
		m_wireframeEffect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_wireframeEffect.Initialize();
	}
	return VK_SUCCESS;
}

GraphicsEffect MeshRenderable::GetAudioOcclusionEffect( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& cmfMesh )
{
	auto config = GraphicsEffect::Config();
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.polygonMode = VK_POLYGON_MODE_FILL;
	config.cullMode = VK_CULL_MODE_NONE;
	config.blend = false;
	config.stride = sizeof( Vector3 );
	config.availableVertexElements = {
		cmf::VertexElement{
			cmf::Usage::Position,
			0,
			cmf::ElementType::Float32,
			3,
			0 }
	};

	GraphicsEffect effect( renderer );
	effect.SetShaderName( "Face Normal" );
	effect.SetConfig( config );
	effect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	return effect;
}

uint8_t MeshRenderable::GetSkeletonIndex() const
{
	return m_cmfMesh.skeleton;
}