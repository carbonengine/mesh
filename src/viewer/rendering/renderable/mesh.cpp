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
	for( const auto& vertexElement : m_cmfMesh.decl )
	{
		m_availableVertexElements.push_back( vertexElement );
		if( vertexElement.usage == cmf::Usage::Normal )
		{
			m_normalAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreateNormal( renderer, m_cmfMesh ) );
		}
		else if( vertexElement.usage == cmf::Usage::Tangent )
		{
			m_tangentAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreateTangent( renderer, m_cmfMesh ) );
		}
		else if( vertexElement.usage == cmf::Usage::Binormal )
		{
			m_binormalAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreateBinormal( renderer, m_cmfMesh ) );
		}
		else if( vertexElement.usage == cmf::Usage::PackedTangent )
		{
			m_normalAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreatePackedNormal( renderer, m_cmfMesh ) );
			m_tangentAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreatePackedTangent( renderer, m_cmfMesh ) );
			m_binormalAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreatePackedBinormal( renderer, m_cmfMesh ) );
		}
		else if( vertexElement.usage == cmf::Usage::PackedTangentLegacy )
		{
			m_normalAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreatePackedLegacyNormal( renderer, m_cmfMesh ) );
			m_tangentAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreatePackedLegacyTangent( renderer, m_cmfMesh ) );
			m_binormalAxisRenderable = std::make_unique<PrimitiveRenderable>( Axis::CreatePackedLegacyBinormal( renderer, m_cmfMesh ) );
		}
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
	m_prepass.Initialize( appState );

	appState.modelState.polygonMode.RegisterCallback( [this]( VkPolygonMode mode, AppState& appState ) {
		SetRenderingMode( appState.modelState.visualizationShader.GetValue(), mode );
	} );

	appState.modelState.visualizationShader.RegisterCallback( [this]( std::string shaderName, AppState& appState ) {
		SetRenderingMode( shaderName, appState.modelState.polygonMode.GetValue() );
	} );

	m_meshIndex = appState.modelState.meshes.AddState( [this]( MeshState& meshState ) {
		meshState.display.RegisterCallback( [this]( bool visible, AppState& ) {
			m_display = visible;
		} );
		meshState.wireframeOverlay.RegisterCallback( [this]( bool enabled, AppState& ) {
			m_wireframe = enabled;
		} );
		meshState.audioOcclusionMesh.RegisterCallback( [this]( bool enabled, AppState& ) {
			m_audioOcclusion = enabled;
		} );
		meshState.renderBoundingBox.RegisterCallback( [this]( bool enabled, AppState& ) {
			m_showBoundingBox = enabled;
		} );
		meshState.activeLod.RegisterCallback( [this]( uint32_t lodIndex, AppState& appState ) {
			SetLod( lodIndex );
		} );
		SetLod( meshState.activeLod.GetValue() );
		for( size_t i = 0; i < m_cmfMesh.morphTargets.targets.size(); ++i )
		{
			meshState.morphs.AddState();
			meshState.morphs[i].RegisterCallback( [this, i]( std::pair<float, bool> morph, AppState& ) {
				m_prepass.SetMorphWeight( static_cast<uint32_t>( i ), morph.second ? morph.first : 0.0f );
			} );
		}
	} );

	appState.modelState.selectedLod.RegisterCallback( [this]( int32_t lodIndex, AppState& appState ) {
		if( lodIndex != -1 )
		{
			appState.modelState.meshes[m_meshIndex].GetValue().activeLod.SetValue( lodIndex );
		}
	} );

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

	if( m_normalAxisRenderable )
	{
		m_normalAxisRenderable->Initialize();
	}
	if( m_tangentAxisRenderable )
	{
		m_tangentAxisRenderable->Initialize();
	}
	if( m_binormalAxisRenderable )
	{
		m_binormalAxisRenderable->Initialize();
	}
	m_initialized = true;
}

void MeshRenderable::SetAnimation( const cmf::Animation* animation )
{
	m_morphCurveToTargetMapping.clear();

	for( const auto& channel: animation->channels )
	{
		if( channel.targetType == cmf::AnimationChannelTargetType::MorphTarget )
		{
			uint32_t morphIndex = 0;
			for( const auto& morphTarget : m_cmfMesh.morphTargets.targets )
			{
				if( cmf::ToStdString( morphTarget.name ) == cmf::ToStdString( channel.target ) )
				{
					m_morphCurveToTargetMapping.emplace_back( channel.curveIndex, morphIndex );
					break;
				}
				++morphIndex;
			}
		}
	}
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
			auto& morphState = appState.modelState.meshes[m_meshIndex].GetValue().morphs[morphIndex];
			morphState.SetValueNoCallback( { weight, morphState.GetValue().second } );
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
		auto& meshState = appState.modelState.meshes[m_meshIndex].GetValue();
		meshState.meshScreenSize.SetValueNoCallback( sizeOnScreen );
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
			meshState.activeLod.SetValueNoCallback( lodLevel );
		}
	}
}

void MeshRenderable::Render( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
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

void MeshRenderable::RenderDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
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
	const auto& meshState = appState.modelState.meshes[m_meshIndex].GetValue();

	if( m_tangentAxisRenderable && meshState.showVertexTangents.GetValue() )
	{
		m_tangentAxisRenderable->SetUniformData( 0, viewProj );
		m_tangentAxisRenderable->Render( commandBuffer, &( m_prepass.GetVertexBuffer() ), nullptr, 2, cmf::GetStreamElementCount( m_cmfMesh.lods[m_currentLod].vb ) );
	}
	if( m_normalAxisRenderable && meshState.showVertexNormals.GetValue() )
	{
		m_normalAxisRenderable->SetUniformData( 0, viewProj );
		m_normalAxisRenderable->Render( commandBuffer, &( m_prepass.GetVertexBuffer() ), nullptr, 2, cmf::GetStreamElementCount( m_cmfMesh.lods[m_currentLod].vb ) );
	}
	if( m_binormalAxisRenderable && meshState.showVertexBinormals.GetValue() )
	{
		m_binormalAxisRenderable->SetUniformData( 0, viewProj );
		m_binormalAxisRenderable->Render( commandBuffer, &( m_prepass.GetVertexBuffer() ), nullptr, 2, cmf::GetStreamElementCount( m_cmfMesh.lods[m_currentLod].vb ) );
	}
}

void MeshRenderable::PrepareMesh( ComputeCommandBuffer& commandBuffer )
{
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
		m_modelEffect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
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
		m_wireframeEffect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
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
	effect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	return effect;
}

uint8_t MeshRenderable::GetSkeletonIndex() const
{
	return m_cmfMesh.skeleton;
}