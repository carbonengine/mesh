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
}

void MeshRenderable::Initialize( AppState& appState )
{
	m_prepass.Initialize( appState );

	appState.modelState.polygonMode.RegisterCallback( [this]( VkPolygonMode mode, AppState& appState ) {
		auto [name, declaration] = appState.modelState.activeShader.GetValue();
		SetRenderingMode( name, declaration, mode );
	} );

	appState.modelState.activeShader.RegisterCallback( [this]( std::pair<std::string, GraphicsEffectTypes::ShaderInputDeclaration> shaderInputDeclaration, AppState& appState ) {
		auto [name, declaration] = shaderInputDeclaration;

		SetRenderingMode( name, declaration, appState.modelState.polygonMode.GetValue() );
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
		for( const auto& vertexElement : m_cmfMesh.decl )
		{
			m_availableVertexElements.push_back( vertexElement );
			if( vertexElement.usage == cmf::Usage::Normal )
			{
				meshState.showVertexNormals.AddState( { vertexElement.usageIndex, false } );
				m_normalAxisRenderables.push_back( Axis::CreateNormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			}
			else if( vertexElement.usage == cmf::Usage::Tangent )
			{
				meshState.showVertexTangents.AddState( { vertexElement.usageIndex, false } );
				m_tangentAxisRenderables.push_back( Axis::CreateTangent( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			}
			else if( vertexElement.usage == cmf::Usage::Binormal )
			{
				meshState.showVertexBinormals.AddState( { vertexElement.usageIndex, false } );
				m_binormalAxisRenderables.push_back( Axis::CreateBinormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			}
			else if( vertexElement.usage == cmf::Usage::PackedTangent )
			{
				meshState.showVertexNormals.AddState( { vertexElement.usageIndex, false } );
				meshState.showVertexTangents.AddState( { vertexElement.usageIndex, false } );
				meshState.showVertexBinormals.AddState( { vertexElement.usageIndex, false } );

				m_normalAxisRenderables.push_back( Axis::CreatePackedNormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
				m_tangentAxisRenderables.push_back( Axis::CreatePackedTangent( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
				m_binormalAxisRenderables.push_back( Axis::CreatePackedBinormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			}
			else if( vertexElement.usage == cmf::Usage::PackedTangentLegacy )
			{
				meshState.showVertexNormals.AddState( { vertexElement.usageIndex, false } );
				meshState.showVertexTangents.AddState( { vertexElement.usageIndex, false } );
				meshState.showVertexBinormals.AddState( { vertexElement.usageIndex, false } );

				m_normalAxisRenderables.push_back( Axis::CreatePackedLegacyNormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
				m_tangentAxisRenderables.push_back( Axis::CreatePackedLegacyTangent( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
				m_binormalAxisRenderables.push_back( Axis::CreatePackedLegacyBinormal( m_renderer, m_cmfMesh, vertexElement.usageIndex ) );
			}
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

	auto streamElementCount = cmf::GetStreamElementCount( m_cmfMesh.lods[m_currentLod].vb );
	const auto* buffer = &( m_prepass.GetVertexBuffer() );
	uint32_t index = 0;
	for( const auto& normalState : meshState.showVertexNormals )
	{
		if( normalState.GetValue().second )
		{
			// since the state and renderables are created in the same order based on the vertex elements,
			// we can use the index to get the corresponding renderable for the normal/tangent/binormal we want to render
			m_normalAxisRenderables[index].SetUniformData( 0, viewProj );
			m_normalAxisRenderables[index].Render( commandBuffer, buffer, nullptr, 2, streamElementCount );
		}
		++index;
	}
	index = 0;
	for( const auto& tangentState : meshState.showVertexTangents )
	{
		if( tangentState.GetValue().second )
		{
			m_tangentAxisRenderables[index].SetUniformData( 0, viewProj );
			m_tangentAxisRenderables[index].Render( commandBuffer, buffer, nullptr, 2, streamElementCount );
		}
		++index;
	}

	index = 0;
	for( const auto& binormalState : meshState.showVertexBinormals )
	{
		if( binormalState.GetValue().second )
		{
			m_binormalAxisRenderables[index].SetUniformData( 0, viewProj );
			m_binormalAxisRenderables[index].Render( commandBuffer, buffer, nullptr, 2, streamElementCount );
		}
		++index;
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

VkResult MeshRenderable::SetRenderingMode( std::string shaderName, GraphicsEffectTypes::ShaderInputDeclaration shaderInputDeclaration, VkPolygonMode polygonMode )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();

	m_polygonMode = polygonMode;
	m_shaderName = shaderName;

	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );

	auto config = GraphicsEffectTypes::Config();
	config.topology = m_topology;
	config.polygonMode = polygonMode;
	config.cullMode = ( polygonMode == VK_POLYGON_MODE_FILL ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	config.inputDeclaration.vertexDeclarations = shaderInputDeclaration.vertexDeclarations;
	config.inputDeclaration.stride = m_stride;

	m_modelEffect.SetShaderName( m_shaderName );
	m_modelEffect.SetConfig( config );
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

		m_wireframeEffect.SetShaderName( "wireframeoverlay" );
		m_wireframeEffect.SetConfig( wireframeConfig );
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
	effect.SetShaderName( "Face Normal" );
	effect.SetConfig( config );
	effect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	return effect;
}

uint8_t MeshRenderable::GetSkeletonIndex() const
{
	return m_cmfMesh.skeleton;
}