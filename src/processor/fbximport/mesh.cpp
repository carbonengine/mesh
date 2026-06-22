// Copyright © 2026 CCP ehf.

#include "mesh.h"
#include "lodsimplygon.h"
#include "cmf/declutils.h"
#include "cmf/bufferstreams.h"
#include "cmf/bounds.h"
#include "cmf/bufferutils.h"
#include "cmf/tangents.h"
#include "cmf/uvdensity.h"



/** @brief Represents skin weights and indices for a vertex. 
* This struct holds up to 4 bone indices and their corresponding weights for a single vertex. 
*/
struct SkinWeights
{
	uint32_t indices[4] = { 0, 0, 0, 0 };
	float weights[4] = { 0, 0, 0, 0 };
	uint32_t count = 0;

	/** @brief Appends the bone indices to a vertex buffer. 
     *
     * @param buffer The vertex buffer to which the packed indices will be appended.
     * @param indexType The type of the bone indices, which determines how they are packed and stored in the buffer. Only cmf::ElementType::UInt8 and cmf::ElementType::UInt16 are supported.
     */
	void AppendPackedIndices( std::vector<uint8_t>& buffer, cmf::ElementType indexType ) const
	{
		if( indexType == cmf::ElementType::UInt8 )
		{
			for( uint32_t j = 0; j < 4; ++j )
			{
				uint8_t index = static_cast<uint8_t>( indices[j] );
				auto ptr = reinterpret_cast<const uint8_t*>( &index );
				buffer.insert( buffer.end(), ptr, ptr + sizeof( index ) );
			}
		}
		else if( indexType == cmf::ElementType::UInt16 )
		{
			for( uint32_t j = 0; j < 4; ++j )
			{
				uint16_t index = static_cast<uint16_t>( indices[j] );
				auto ptr = reinterpret_cast<const uint8_t*>( &index );
				buffer.insert( buffer.end(), ptr, ptr + sizeof( index ) );
			}
		}
		else
		{
			// This should never happen because ValidateOptions should have been called before, but we check it just in case.
			throw std::runtime_error( "Unsupported bone index type" );
		}
	}

	/** @brief Adds a bone to the vertex.
     *
     * This method adds a bone weight to the vertex. If the number of weights exceeds the allowed bones per vertex,
     * the smallest weight is replaced if the new weight is larger.
     *
     * @param boneIndex The index of the bone.
     * @param weight The weight of the bone.
     * @param bonesPerVertex The maximum number of bones per vertex.
     */
	void AddWeight( uint32_t boneIndex, float weight, uint32_t bonesPerVertex )
	{
		if( count < bonesPerVertex )
		{
			indices[count] = boneIndex;
			weights[count] = std::max( 0.f, weight );
			++count;
		}
		else
		{
			// Find smallest weight and replace it if the new weight is larger
			uint32_t minIndex = 0;
			for( uint32_t k = 1; k < bonesPerVertex; ++k )
			{
				if( weights[k] < weights[minIndex] )
				{
					minIndex = k;
				}
			}
			if( weight > weights[minIndex] )
			{
				indices[minIndex] = boneIndex;
				weights[minIndex] = std::max( 0.f, weight );
			}
		}
	}

	/** @brief Normalizes the bone weights so that they sum up to 1.
     */
	void Normalize()
	{
		float total = weights[0] + weights[1] + weights[2] + weights[3];
		if( total > 0 )
		{
			weights[0] /= total;
			weights[1] /= total;
			weights[2] /= total;
			weights[3] /= total;
		}
	}

	/** @brief Packs the bone weights into a 32-bit unsigned integer.
     *
     * This method converts the bone weights, into a packed format suitable for storage in a vertex buffer. 
     * Each weight is scaled to fit into an 8-bit unsigned integer (0-255) and then combined into a single 32-bit unsigned integer.
     *
     * @return A 32-bit unsigned integer containing the packed bone weights.
     */
	[[nodiscard]] uint32_t GetPackedWeights() const
	{
		const uint32_t maxUint8 = std::numeric_limits<uint8_t>::max();

		auto weightAsUint8 = [maxUint8]( float weight ) -> uint32_t {
			return std::min( static_cast<uint32_t>( std::max( 0.0f, std::round( weight * float( maxUint8 ) ) ) ), maxUint8 );
		};
		std::array<uint32_t, 4> intWeights = {
			weightAsUint8( weights[0] ),
			weightAsUint8( weights[1] ),
			weightAsUint8( weights[2] ),
			weightAsUint8( weights[3] )
		};
		// Renormalize the integer weights to ensure they sum up to 255.
		auto sum = std::accumulate( intWeights.begin(), intWeights.end(), 0u );
		if( sum != 0 )
		{
			while( sum != maxUint8 )
			{
				for( auto& w : intWeights )
				{
					if( sum < maxUint8 )
					{
						if( w < maxUint8 )
						{
							++w;
							++sum;
						}
					}
					else
					{
						if( w > 0 )
						{
							--w;
							--sum;
						}
					}
					if( sum == maxUint8 )
					{
						break;
					}
				}
			}
		}
		return intWeights[0] | ( intWeights[1] << 8 ) | ( intWeights[2] << 16 ) | ( intWeights[3] << 24 );
	}
};

/** @brief Represents the skinning information for a mesh, including the skin weights for each vertex and the bone nodes.
 */
struct MeshSkin
{
	std::vector<SkinWeights> weights;
	std::vector<const ufbx_node*> bones;
};

/** @brief Fills a vertex buffer with vertex data from an FBX mesh, based on the specified vertex declaration and skinning information.
 *
 * This function iterates over the vertices of the given FBX mesh and constructs a vertex buffer according to the provided vertex declaration. 
 * It handles various vertex attributes such as positions, normals, tangents, colors, UVs, and skinning weights/indices. 
 *
 * @param geom The FBX mesh containing the vertex data to be imported.
 * @param decl The vertex declaration specifying the layout of the vertex buffer.
 * @param skin A vector of SkinWeights containing skinning information for each vertex.
 * @param options The options for importing the mesh, which may affect how certain attributes are processed.
 * @param bufferAllocator The buffer manager used to allocate the resulting vertex buffer.
 * @param meshTransform The transformation matrix applied to the mesh vertices.
 * @param systemTransform The coordinate system transformation applied to the vertex data.
 * @return A BufferView representing the filled vertex buffer.
 */
cmf::BufferView FillVertexBuffer( const ufbx_mesh& geom, const cmf::Span<cmf::VertexElement>& decl, const std::vector<SkinWeights>& skin, const MeshImportOptions& options, cmf::BufferManager& bufferAllocator, const Matrix& meshTransform, const CoordinateSystem& systemTransform )
{
	const auto& positions = geom.vertex_position;
	auto normals = geom.vertex_normal;
	const auto& tangents = geom.vertex_tangent;
	std::vector<ufbx_vertex_vec4> colors;
	for( uint32_t i = 0; i < options.colors; ++i )
	{
		const auto* found = std::find_if( geom.color_sets.begin(), geom.color_sets.end(), [i]( const ufbx_color_set& cs ) {
			return cs.index == i;
		} );
		// No need for any checks here because CreateVertexDeclaration should have already verified that the color set exists and contains vertex colors.
		colors.push_back( found->vertex_color );
	}
	auto lockedVertices = std::find_if( geom.color_sets.begin(), geom.color_sets.end(), [&options]( const ufbx_color_set& cs ) {
		return ToString( cs.name ) == options.lods.simplygon.lockVertexChannel;
	} );

	uint32_t stride = std::accumulate( decl.begin(), decl.end(), 0u, []( uint32_t acc, const cmf::VertexElement& element ) {
		return acc + cmf::GetVertexElementSize( element );
	} );

	std::vector<uint8_t> vb;
	vb.reserve( positions.indices.count * stride );

	std::vector<ufbx_vec3> genNormals;
	std::vector<uint32_t> normalIndices( geom.num_indices );
	if( options.normals && options.regenerateNormals )
	{
		std::vector<ufbx_topo_edge> topology;
		topology.resize( geom.num_indices );
		ufbx_compute_topology( &geom, topology.data(), geom.num_indices );
		auto normalIndexCount = ufbx_generate_normal_mapping( &geom, topology.data(), geom.num_indices, normalIndices.data(), normalIndices.size(), true );
		genNormals.resize( normalIndexCount );
		ufbx_compute_normals( &geom, &geom.vertex_position, normalIndices.data(), normalIndices.size(), genNormals.data(), genNormals.size() );
		normals.values.data = genNormals.data();
		normals.values.count = genNormals.size();
		normals.indices.data = normalIndices.data();
		normals.indices.count = normalIndices.size();
	}

	std::vector<uint8_t> vertex;
	for( int i = 0; i < positions.indices.count; ++i )
	{
		auto AddToVertex = [&vertex]( const auto& value ) {
			auto ptr = reinterpret_cast<const uint8_t*>( &value );
			vertex.insert( vertex.end(), ptr, ptr + sizeof( value ) );
		};
		vertex.clear();

		Vector3 norm = {};
		Vector3 tan = {};
		Vector3 bitan = {};
		if( options.normals )
		{
			norm = Normalize( systemTransform.TransformVector( TransformNormal( ToVector3( normals[i] ), meshTransform ) ) );
		}
		if( options.tangents > 0 )
		{
			if( tangents.exists && !options.alwaysComputeTangents )
			{
				tan = Normalize( systemTransform.TransformVector( TransformNormal( ToVector3( tangents[i] ), meshTransform ) ) );
				bitan = Normalize( Cross( norm, tan ) );
				if( options.flipV )
				{
					bitan = -bitan;
				}
			}
			else
			{
				// Fill in default tangent space if tangents are missing: we will recompute them later
				tan = { 1, 0, 0 };
				bitan = Normalize( Cross( norm, tan ) );
			}
		}

		for( auto& element : decl )
		{
			switch( element.usage )
			{
			case cmf::Usage::Position:
				AddToVertex( systemTransform.TransformPoint( TransformCoord( ToVector3( positions[i] ), meshTransform ) ) );
				break;
			case cmf::Usage::Normal:
				AddToVertex( norm );
				break;
			case cmf::Usage::Tangent:
				AddToVertex( tan );
				break;
			case cmf::Usage::Binormal:
				AddToVertex( bitan );
				break;
			case cmf::Usage::Color:
				if( element.usageIndex == LOCKED_VERTEX_USAGE_INDEX )
				{
					// locked vertex flags
					AddToVertex( ToVector4( lockedVertices->vertex_color[i] ) );
				}
				else
				{
					AddToVertex( ToVector4( colors[element.usageIndex][i] ) );
				}
				break;
			case cmf::Usage::TexCoord: {
				auto& uvSet = geom.uv_sets[element.usageIndex].vertex_uv;
				auto tex = ToVector2( uvSet[i] );
				if( options.flipV )
				{
					tex.y = 1.f - tex.y;
				}
				AddToVertex( tex );
				break;
			}
			case cmf::Usage::BoneIndices: {
				auto& w = skin[positions.indices[i]];
				w.AppendPackedIndices( vertex, element.type );
				break;
			}
			case cmf::Usage::BoneWeights: {
				auto& w = skin[positions.indices[i]];
				AddToVertex( w.GetPackedWeights() );
				break;
			}
			default:
				throw std::runtime_error( "Internal error: unsupported vertex element usage" );
			}
		}

		if( vertex.size() != stride )
		{
			throw std::runtime_error( "Internal error: vertex size mismatch" );
		}

		vb.insert( vb.end(), vertex.begin(), vertex.end() );
	}

	return bufferAllocator.AllocateBuffer( vb.data(), uint32_t( vb.size() ), stride );
}

/**
 * @brief Creates a vertex declaration for an FBX mesh based on the specified import options and skinning information.
 *
 * This function generates a vertex declaration that describes the layout of vertex attributes in the vertex buffer.
 *
 * @param geom The FBX mesh containing the vertex data.
 * @param options The options for importing the mesh, which may affect the vertex declaration.
 * @param hasSkinning Indicates whether the mesh has skinning information.
 * @param allocator The memory allocator used to allocate the vertex declaration.
 * @return A span of VertexElement representing the vertex declaration.
 */
cmf::Span<cmf::VertexElement> CreateVertexDeclaration( const ufbx_mesh& geom, const MeshImportOptions& options, bool hasSkinning, cmf::MemoryAllocator& allocator )
{
	if( !geom.vertex_position.exists )
	{
		throw std::runtime_error( "mesh is missing vertex positions" );
	}

	cmf::Span<cmf::VertexElement> decl;
	uint32_t offset = 0;
	cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Position, 0, cmf::ElementType::Float32, 3 } );
	offset += sizeof( Vector3 );
	if( hasSkinning )
	{
		cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::BoneIndices, 0, options.boneIndexType, 4, offset } );
		offset += cmf::GetElementTypeSize( options.boneIndexType ) * 4;
		if( options.bonesPerVertex != 1 )
		{
			cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::BoneWeights, 0, cmf::ElementType::UInt8Norm, 4, offset } );
			offset += sizeof( uint32_t );
		}
	}
	if( options.normals )
	{
		if( !geom.vertex_normal.exists && !options.regenerateNormals )
		{
			throw std::runtime_error( "mesh is missing vertex normals" );
		}
		cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Normal, 0, cmf::ElementType::Float32, 3, offset } );
		offset += sizeof( Vector3 );
		for( uint32_t i = 0; i < options.tangents; ++i )
		{
			cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Tangent, uint8_t( i ), cmf::ElementType::Float32, 3, offset } );
			offset += sizeof( Vector3 );
			cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Binormal, uint8_t( i ), cmf::ElementType::Float32, 3, offset } );
			offset += sizeof( Vector3 );
		}
	}
	if( options.colors > 0 )
	{
		for( uint32_t i = 0; i < options.colors; ++i )
		{
			const auto* found = std::find_if( geom.color_sets.begin(), geom.color_sets.end(), [i]( const ufbx_color_set& cs ) {
				return cs.index == i;
			} );
			if( found == geom.color_sets.end() || !found->vertex_color.exists )
			{
				throw std::runtime_error( "mesh is missing vertex color set " + std::to_string( i ) );
			}
			cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Color, uint8_t( i ), cmf::ElementType::Float32, 4, offset } );
			offset += sizeof( Vector4 );
		}
	}
	if( !options.lods.simplygon.lockVertexChannel.empty() )
	{
		auto found = std::find_if( geom.color_sets.begin(), geom.color_sets.end(), [&options]( const ufbx_color_set& cs ) {
			return ToString( cs.name ) == options.lods.simplygon.lockVertexChannel;
		} );
		if( found == geom.color_sets.end() || !found->vertex_color.exists )
		{
			throw std::runtime_error( "mesh is missing simplygon lock vertex color set " + options.lods.simplygon.lockVertexChannel );
		}
		cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Color, LOCKED_VERTEX_USAGE_INDEX, cmf::ElementType::Float32, 4, offset } );
		offset += sizeof( Vector4 );
	}
	for( uint32_t uvSet = 0; uvSet < options.uvSets; ++uvSet )
	{
		if( uvSet >= geom.uv_sets.count || !geom.uv_sets[uvSet].vertex_uv.exists )
		{
			throw std::runtime_error( "mesh is missing UV set " + std::to_string( uvSet ) );
		}
		cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::TexCoord, uint8_t( uvSet ), cmf::ElementType::Float32, 2, offset } );
		offset += sizeof( Vector2 );
	}
	return decl;
}

/** 
* @brief Adds a value to a container if it is not already present, and returns a reference to the value in the container.
*/
template <typename T>
T& AddUnique( cmf::Span<T>& container, const T& value, cmf::MemoryAllocator& allocator )
{
	auto found = std::find( container.begin(), container.end(), value );
	if( found == container.end() )
	{
		cmf::Modify( container, allocator ).push_back( value );
		return *( container.end() - 1 );
	}
	return *found;
}

/** 
* @brief Imports skinning information from an FBX mesh deformers and constructs a MeshSkin structure containing bone indices and weights for each vertex.
* 
* @param mesh The FBX mesh containing the skin deformers to be imported.
* @param options The options for importing the mesh, which may affect how skinning information is processed.
* @return A MeshSkin structure containing the imported skinning information.
*/
MeshSkin ImportSkin( const ufbx_mesh& mesh, const MeshImportOptions& options )
{
	MeshSkin result;
	if( !options.skinning || !mesh.skin_deformers.count )
	{
		return result;
	}
	auto skin = mesh.skin_deformers[0];
	result.weights.resize( mesh.vertex_position.values.count );
	for( auto cluster : skin->clusters )
	{
		if( !cluster->vertices.count )
		{
			continue;
		}
		auto link = cluster->bone_node;
		if( !link )
		{
			continue;
		}

		auto found = std::find( begin( result.bones ), end( result.bones ), link );
		uint32_t boneIndex = 0;
		if( found == end( result.bones ) )
		{
			result.bones.push_back( link );
			boneIndex = uint32_t( result.bones.size() - 1 );
		}
		else
		{
			boneIndex = uint32_t( std::distance( begin( result.bones ), found ) );
		}

		const auto indices = cluster->vertices;
		const auto weights = cluster->weights;
		for( int j = 0; j < cluster->vertices.count; ++j )
		{
			if( indices[j] >= result.weights.size() )
			{
				continue;
			}
			auto& sw = result.weights[indices[j]];
			sw.AddWeight( boneIndex, static_cast<float>( weights[j] ), options.bonesPerVertex );
		}
	}
	for( auto& sw : result.weights )
	{
		sw.Normalize();
	}
	return result;
}

/** @brief Decodes a Base64-encoded string into a vector of bytes.
 *
 * @param str The Base64-encoded string to decode.
 * @return A vector of bytes containing the decoded data.
 */
std::vector<uint8_t> Base64Decode( ufbx_string str )
{
	std::vector<uint8_t> result;
	static const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::map<char, uint8_t> charMap;
	for( uint8_t i = 0; i < 64; ++i )
	{
		charMap[base64Chars[i]] = i;
	}
	uint32_t buffer = 0;
	int bitsInBuffer = 0;
	for( size_t i = 0; i < str.length; ++i )
	{
		char c = str.data[i];
		if( c == '=' )
		{
			break;
		}
		if( charMap.find( c ) == charMap.end() )
		{
			continue;
		}
		buffer = ( buffer << 6 ) | charMap[c];
		bitsInBuffer += 6;
		if( bitsInBuffer >= 8 )
		{
			bitsInBuffer -= 8;
			result.push_back( ( buffer >> bitsInBuffer ) & 0xFF );
		}
	}
	return result;
}

/** @brief Creates a vertex declaration for morph target vertices based on the specified import options.
 *
 * This function generates a vertex declaration that describes the layout of vertex attributes for morph targets, which typically include positions and optionally normals and tangents.
 *
 * @param options The options for importing the mesh, which may affect the vertex declaration for morph targets.
 * @param allocator The memory allocator used to allocate the vertex declaration.
 * @return A vertex declaration for morph targets.
 */
cmf::Span<cmf::VertexElement> CreateMorphVertexDeclaration( const MeshImportOptions& options, cmf::MemoryAllocator& allocator )
{
	cmf::Span<cmf::VertexElement> decl;
	cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Position, 0, cmf::ElementType::Float32, 3 } );
	uint32_t offset = sizeof( Vector3 );
	if( options.normals )
	{
		cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Normal, 0, cmf::ElementType::Float32, 3, offset } );
		offset += sizeof( Vector3 );
		for( uint32_t t = 0; t < options.tangents; ++t )
		{
			cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Tangent, 0, cmf::ElementType::Float32, 3, offset } );
			offset += sizeof( Vector3 );
			cmf::Modify( decl, allocator ).emplace_back( cmf::VertexElement{ cmf::Usage::Binormal, 0, cmf::ElementType::Float32, 3, offset } );
			offset += sizeof( Vector3 );
		}
	}
	return decl;
}

/** @brief Extracts custom normals for a morph target from a mesh node.
 *
 * This function searches for a property in the mesh node that contains custom normal data for the specified morph target.
 * The custom normals are expected to be Base64-encoded and are decoded into a vector of Vector3 values, one per face vertex.
 *
 * @param customNormals A vector to store the extracted custom normals.
 * @param meshNode The mesh node containing the custom normal data.
 * @param shapeName The name of the morph target for which to extract custom normals.
 * @throws std::runtime_error If the custom normal data is invalid or missing.
 */
void ExtractCustomNormals( std::vector<Vector3>& customNormals, const ufbx_node& meshNode, const std::string& shapeName )
{
	for( auto prop : meshNode.props.props )
	{
		auto name = ToString( prop.name );
		auto prefix = std::string( "bsNormals_" );
		if( name != prefix + shapeName )
		{
			continue;
		}
		auto decoded = Base64Decode( prop.value_str );
		if( decoded.size() != meshNode.mesh->vertex_position.indices.count * sizeof( Vector3 ) )
		{
			throw std::runtime_error( "Invalid custom normal data for morph target " + shapeName );
		}
		auto decodedNormals = reinterpret_cast<const Vector3*>( decoded.data() );
		customNormals.assign( decodedNormals, decodedNormals + meshNode.mesh->vertex_position.indices.count );
		return;
	}
	throw std::runtime_error( "Morph target " + shapeName + " is missing custom normal data" );
}

/** @brief Manages normals for morph targets, including custom normals if specified.
 *
 * This struct handles the extraction and storage of normals for morph targets. It handles three cases:
 * 1. Normals are not required.
 * 2. Normals are required and generated from morphed mesh vertices using the original mesh topology.
 * 3. Normals are required and normals stored in custom FBX properties are used.
 */
struct MorphTargetNormals
{
	MorphTargetNormals( bool requireNormals, bool useCustomNormals, const ufbx_node& meshNode ) :
		m_meshNode( meshNode ),
		m_requireNormals( requireNormals ),
		m_useCustomNormals( useCustomNormals )
	{
		if( !m_requireNormals || m_useCustomNormals )
		{
			return;
		}

		const auto& mesh = *meshNode.mesh;
		m_normalIndices.resize( mesh.num_indices );
		if( mesh.blend_deformers.count > 0 )
		{
			std::vector<ufbx_topo_edge> topology;
			topology.resize( mesh.num_indices );
			ufbx_compute_topology( &mesh, topology.data(), mesh.num_indices );
			auto normalIndexCount = ufbx_generate_normal_mapping( &mesh, topology.data(), mesh.num_indices, m_normalIndices.data(), m_normalIndices.size(), true );
			m_normals.resize( normalIndexCount );
		}
	}
	void ExtractNormals( const std::string& shapeName, ufbx_vec3* positions )
	{
		if( !m_requireNormals )
		{
			return;
		}
		if( m_useCustomNormals )
		{
			ExtractCustomNormals( m_customNormals, m_meshNode, shapeName );
		}
		else
		{
			ufbx_vertex_vec3 appliedPositionStream = m_meshNode.mesh->vertex_position;
			appliedPositionStream.values.data = positions;
			ufbx_compute_normals( m_meshNode.mesh, &appliedPositionStream, m_normalIndices.data(), m_normalIndices.size(), m_normals.data(), m_normals.size() );
		}
	}

	Vector3 GetNormal( size_t vertexIndex ) const
	{
		if( !m_requireNormals )
		{
			return Vector3( 0, 0, 0 );
		}
		if( m_useCustomNormals )
		{
			return m_customNormals[vertexIndex];
		}
		return ToVector3( m_normals[m_normalIndices[vertexIndex]] );
	}

	const ufbx_node& m_meshNode;
	bool m_requireNormals = false;
	bool m_useCustomNormals = false;
	std::vector<Vector3> m_customNormals;
	std::vector<uint32_t> m_normalIndices;
	std::vector<ufbx_vec3> m_normals;
};

/** @brief Enumerates blend shapes (morph targets) in an FBX mesh.
 *
 * @tparam T The type of the callback function, which should be callable with a const ufbx_blend_shape& argument.
 * @param mesh The FBX mesh containing the blend deformers to be enumerated.
 * @param callback The callback function to be invoked for each applicable blend shape.
 */
template <typename T>
void EnumerateApplicableBlendShapes( const ufbx_mesh& mesh, T&& callback )
{
	for( int i = 0; i < mesh.blend_deformers.count; ++i )
	{
		auto deformer = mesh.blend_deformers[i];
		for( int j = 0; j < deformer->channels.count; ++j )
		{
			auto channel = deformer->channels[j];
			auto shape = channel->target_shape;
			if( !shape || shape->num_offsets == 0 )
			{
				continue;
			}
			callback( *shape );
		}
	}
}

/** @brief Imports blend shapes (morph targets) from an FBX mesh.
 *
 * @param meshNode The FBX node containing the mesh to import blend shapes from.
 * @param options The options for importing the mesh, which may include filters for morph target names and other import settings.
 * @param allocator The memory allocator used for allocating morph target data.
 * @param bufferAllocator The buffer manager used for allocating vertex buffers for LOD morph targets.
 * @param systemTransform The coordinate system transformation to apply to the morph target vertices and normals.
 * @return A pair containing the imported morph targets and their corresponding LOD morph targets.
 */
std::pair<cmf::MorphTargets, cmf::Span<cmf::LodMorphTarget>> ImportBlendShapes( const ufbx_node& meshNode, const MeshImportOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator, const CoordinateSystem& systemTransform )
{
	cmf::MorphTargets morphTargets;
	cmf::Span<cmf::LodMorphTarget> lodMorphTargets;

	if( !options.morphTargets.importMorphTargets )
	{
		return { morphTargets, lodMorphTargets };
	}

	auto& mesh = *meshNode.mesh;
	auto meshTransform = ToMatrix( meshNode.geometry_to_world );

	std::vector<ufbx_vec3> positions( mesh.vertex_position.values.count );
	MorphTargetNormals normals( options.normals, options.morphTargets.useCustomNormals, meshNode );

	uint32_t vertexSize = 0;

	EnumerateApplicableBlendShapes( mesh, [&]( const ufbx_blend_shape& shape ) {
		auto name = ToString( shape.name );

		cmf::MorphTarget morphTarget;
		morphTarget.name = allocator.AllocateString( name );
		if( morphTargets.decl.empty() )
		{
			morphTargets.decl = CreateMorphVertexDeclaration( options, allocator );
			vertexSize = std::accumulate( morphTargets.decl.begin(), morphTargets.decl.end(), 0u, []( uint32_t acc, const cmf::VertexElement& element ) {
				return acc + cmf::GetVertexElementSize( element );
			} );
		}

		std::copy( mesh.vertex_position.values.begin(), mesh.vertex_position.values.end(), positions.begin() );

		for( size_t k = 0; k < shape.num_offsets; ++k )
		{
			const auto& delta = shape.position_offsets[k];
			const auto& orig = mesh.vertex_position.values[shape.offset_vertices[k]];
			ufbx_vec3 offs;
			offs.x = orig.x + delta.x;
			offs.y = orig.y + delta.y;
			offs.z = orig.z + delta.z;
			positions[shape.offset_vertices[k]] = offs;

			auto deltaLength = Length( ToVector3( delta ) ) * systemTransform.m_scale;
			morphTarget.maxDisplacement = std::max( morphTarget.maxDisplacement, deltaLength );
		}

		normals.ExtractNormals( name, positions.data() );

		cmf::LodMorphTarget lodMorphTarget;
		lodMorphTarget.vb = bufferAllocator.AllocateBuffer( nullptr, uint32_t( mesh.vertex_position.indices.count * vertexSize ), vertexSize );
		auto vb = static_cast<uint8_t*>( bufferAllocator.GetData( lodMorphTarget.vb ) );

		auto AddData = [&vb]( const auto& x ) {
			auto ptr = reinterpret_cast<const uint8_t*>( &x );
			std::copy( ptr, ptr + sizeof( x ), vb );
			vb += sizeof( x );
		};

		for( size_t v = 0; v < mesh.vertex_position.indices.count; ++v )
		{
			AddData( systemTransform.TransformPoint( TransformCoord( ToVector3( positions[mesh.vertex_position.indices[v]] ), meshTransform ) ) );
			if( options.normals )
			{
				AddData( Normalize( systemTransform.TransformVector( TransformNormal( normals.GetNormal( v ), meshTransform ) ) ) );
			}
			for( uint32_t t = 0; t < options.tangents; ++t )
			{
				AddData( Vector3( 1, 0, 0 ) );
				AddData( Vector3( 0, 1, 0 ) );
			}
		}

		cmf::Modify( morphTargets.targets, allocator ).push_back( morphTarget );
		cmf::Modify( lodMorphTargets, allocator ).push_back( lodMorphTarget );
	} );
	return { morphTargets, lodMorphTargets };
}

/** @brief Updates the bone-related data for each area of the mesh.
 *
 * This function updates list of bones affecting each mesh area.
 *
 * @param mesh The mesh whose area bone data is to be updated.
 * @param allocator The memory allocator used for modifying the mesh data.
 * @param bufferAllocator The buffer manager used for accessing the mesh buffers.
 */
void UpdateMeshAreaBoneData( cmf::Mesh& mesh, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	if( !FindElement( mesh.decl, cmf::Usage::BoneIndices ) )
	{
		return;
	}
	for( uint32_t i = 0; i < mesh.areas.size(); ++i )
	{
		for( auto& lod : mesh.lods )
		{
			auto indices = cmf::ConstIndexBufferStream( lod.ib, bufferAllocator );
			auto boneIndices = cmf::ConstBufferElementStream<std::array<uint32_t, 4>>( *FindElement( mesh.decl, cmf::Usage::BoneIndices ), lod.vb, bufferAllocator );

			std::optional<cmf::ConstBufferElementStream<Vector4>> boneWeights;
			if( FindElement( mesh.decl, cmf::Usage::BoneWeights ) )
			{
				boneWeights = cmf::ConstBufferElementStream<Vector4>( *FindElement( mesh.decl, cmf::Usage::BoneWeights ), lod.vb, bufferAllocator );
			}
			auto first = lod.areas[i].firstElement * 3;
			for( uint32_t j = 0; j < lod.areas[i].elementCount * 3; ++j )
			{
				auto idx = boneIndices[indices[first + j]];
				auto w = boneWeights.has_value() ? boneWeights.value()[indices[first + j]] : Vector4( 1, 0, 0, 0 );
				for( uint32_t k = 0; k < 4; ++k )
				{
					if( w[int32_t( k )] > 0 )
					{
						AddUnique( mesh.areas[i].bones, uint16_t( idx[k] ), allocator );
						mesh.areas[i].affectedByBones = idx[k] > 0;
					}
				}
			}
		}
	}
}

/** @brief Updates the derived area data for the mesh, including bone data and bounding volumes.
 *
 * @param mesh The mesh whose area data is to be updated.
 * @param allocator The memory allocator used for modifying the mesh data.
 * @param bufferAllocator The buffer manager used for accessing the mesh buffers.
 */
void UpdateMeshAreaData( cmf::Mesh& mesh, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	UpdateMeshAreaBoneData( mesh, allocator, bufferAllocator );
	for( uint32_t i = 0; i < mesh.areas.size(); ++i )
	{
		mesh.areas[i].bounds = cmf::CalculateAreaBounds( mesh, i, bufferAllocator );
		if( !mesh.morphTargets.targets.empty() )
		{
			for( auto& lod : mesh.lods )
			{
				auto indices = cmf::ConstIndexBufferStream( lod.ib, bufferAllocator );
				auto positions = cmf::ConstBufferElementStream<Vector3>( *FindElement( mesh.decl, cmf::Usage::Position ), lod.vb, bufferAllocator );
				auto first = lod.areas[i].firstElement * 3;
				for( uint32_t m = 0; !mesh.areas[i].affectedByMorphTargets && m < mesh.morphTargets.targets.size(); ++m )
				{
					auto morphPositions = cmf::ConstBufferElementStream<Vector3>( *FindElement( mesh.morphTargets.decl, cmf::Usage::Position ), lod.morphTargets[m].vb, bufferAllocator );

					for( uint32_t j = 0; j < lod.areas[i].elementCount * 3; ++j )
					{
						auto delta = morphPositions[indices[first + j]] - positions[indices[first + j]];
						if( delta.x != 0.f || delta.y != 0.f || delta.z != 0.f )
						{
							mesh.areas[i].affectedByMorphTargets = true;
							break;
						}
					}
				}
			}
		}
	}
}

/** @brief Creates a new vertex declaration with compressed elements for the specified usage based on the original declaration and the desired element type.
 *
 * @param decl The original vertex declaration to be modified.
 * @param usage The usage type of the elements to be compressed (e.g., TexCoord for UVs).
 * @param elementType The element type to use for the specified usage in the new declaration (e.g., Float16 for half-precision).
 * @param allocator The memory allocator used for allocating the new vertex declaration.
 * @return A new vertex declaration with compressed elements for the specified usage.
 */
cmf::Span<cmf::VertexElement> GetCompressedElementDeclaration( const cmf::Span<cmf::VertexElement>& decl, cmf::Usage usage, cmf::ElementType elementType, cmf::MemoryAllocator& allocator )
{
	cmf::Span<cmf::VertexElement> newDecl;
	uint32_t offset = 0;
	for( const auto& element : decl )
	{
		auto newElement = element;
		newElement.offset = offset;
		if( element.usage == usage )
		{
			newElement.type = elementType;
		}
		cmf::Modify( newDecl, allocator ).push_back( newElement );
		offset += cmf::GetVertexElementSize( newElement );
	}
	return newDecl;
}

/** @brief Compresses the vertex attributes with the given usage of a mesh's vertex buffers to a more compact number format if specified.
 *
 * This changes element types for vertex attributes to the desired type and adjusts vertex declarations and vertex buffer contents.
 * The function assumes the original element type is Float32.
 *
 * @param mesh The mesh whose vertex attributes are to be compressed.
 * @param usage The usage type of the vertex attributes to be compressed (e.g., TexCoord for UVs).
 * @param elementType The element type to use for the specified usage (e.g., Float16 for half-precision).
 * @param allocator The memory allocator used for modifying the mesh data.
 * @param bufferAllocator The buffer manager used for accessing and modifying the mesh buffers.
 */
void CompressElements( cmf::Mesh& mesh, cmf::Usage usage, cmf::ElementType elementType, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	if( elementType == cmf::ElementType::Float32 )
	{
		return;
	}

	const bool hasAttributes = std::any_of( mesh.decl.begin(), mesh.decl.end(), [usage]( const cmf::VertexElement& element ) { return element.usage == usage; } );
	if( !hasAttributes )
	{
		return;
	}

	const auto newDecl = GetCompressedElementDeclaration( mesh.decl, usage, elementType, allocator );

	for( auto& lod : mesh.lods )
	{
		lod.vb = cmf::ChangeBufferVertexDeclaration( lod.vb, mesh.decl, newDecl, allocator, bufferAllocator );
	}
	mesh.decl = newDecl;
}

/** @brief Creates the index buffer and mesh area information for a mesh LOD based on the geometry of the FBX mesh and its material partitions.
 *
 * @param outLod The MeshLod structure to be filled with the created topology information.
 * @param geom The FBX mesh containing the geometry and material partitions to be used for creating the topology.
 * @param allocator The memory allocator used for modifying the MeshLod data.
 * @param bufferAllocator The buffer manager used for allocating the index buffer.
 */
void CreateTopology( cmf::MeshLod& outLod, const ufbx_mesh& geom, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	std::unique_ptr<uint32_t[]> triIndices( new uint32_t[geom.max_face_triangles * 3] );

	const auto partitionCount = geom.material_parts.count;
	std::vector<uint32_t> indices;
	for( int j = 0; j < partitionCount; ++j )
	{
		cmf::LodMeshArea area;
		area.firstElement = uint32_t( indices.size() / 3 );

		const auto& partition = geom.material_parts[geom.material_part_usage_order[j]];

		for( int p = 0; p < partition.num_faces; ++p )
		{
			const auto& polygon = partition.face_indices[p];
			const auto triCount = ufbx_triangulate_face( triIndices.get(), geom.max_face_triangles * 3, &geom, geom.faces[polygon] );
			for( uint32_t k = 0; k < triCount * 3; ++k )
			{
				indices.push_back( triIndices[k] );
			}
		}
		area.elementCount = uint32_t( indices.size() / 3 ) - area.firstElement;
		cmf::Modify( outLod.areas, allocator ).push_back( area );
	}
	outLod.ib = bufferAllocator.AllocateBuffer( indices.data(), uint32_t( indices.size() * sizeof( uint32_t ) ), sizeof( uint32_t ) );
}

/** @brief Computes the transformation matrices from mesh space to each bone's local space.
 *
 * @param skin The skin containing the bones.
 * @param meshTransform The transformation matrix of the mesh (source FBX space).
 * @param systemTransform The coordinate system transformation.
 * @return A vector of matrices representing the mesh-to-bone transformations.
 */
std::vector<Matrix> GetMeshToBoneTransforms( const MeshSkin& skin, const Matrix& meshTransform, const CoordinateSystem& systemTransform )
{
	auto mt = systemTransform.TransformMatrix( meshTransform );
	std::vector<Matrix> meshToBoneTransforms;
	meshToBoneTransforms.reserve( skin.bones.size() );
	for( auto& b : skin.bones )
	{
		meshToBoneTransforms.push_back( mt * Inverse( systemTransform.TransformMatrix( ToMatrix( b->geometry_to_world ) ) ) );
	}
	return meshToBoneTransforms;
}

/** @brief Populates bone binding list for the mesh.
* 
 * This function calculates the bounding volumes for each bone binding based on the vertices influenced by each bone and the corresponding mesh-to-bone transformations.
 *
 * @param outMesh The mesh for which to create bone bindings.
 * @param skin The skin containing the bones and vertex weights.
 * @param meshToBoneTransforms The transformation matrices from mesh space to each bone's local space.
 * @param options The options for importing the mesh, which may affect how bone bindings are created.
 * @param allocator The memory allocator used for modifying the mesh data.
 * @param bufferAllocator The buffer manager used for accessing the mesh buffers.
 */
void CreateBoneBindings( cmf::Mesh& outMesh, const MeshSkin& skin, const std::vector<Matrix>& meshToBoneTransforms, const MeshImportOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	if( !skin.bones.empty() )
	{
		for( auto& b : skin.bones )
		{
			cmf::BoneBinding binding;
			binding.name = allocator.AllocateString( ToString( b->name ) );

			auto boneIndex = uint32_t( std::distance( begin( skin.bones ), std::find( begin( skin.bones ), end( skin.bones ), b ) ) );
			auto positions = cmf::ConstBufferElementStream<Vector3>( *FindElement( outMesh.decl, cmf::Usage::Position ), outMesh.lods[0].vb, bufferAllocator );
			auto boneIndices = cmf::ConstBufferElementStream<std::array<uint32_t, 4>>( *FindElement( outMesh.decl, cmf::Usage::BoneIndices ), outMesh.lods[0].vb, bufferAllocator );
			std::optional<cmf::ConstBufferElementStream<Vector4>> boneWeights;
			if( FindElement( outMesh.decl, cmf::Usage::BoneWeights ) )
			{
				boneWeights = cmf::ConstBufferElementStream<Vector4>( *FindElement( outMesh.decl, cmf::Usage::BoneWeights ), outMesh.lods[0].vb, bufferAllocator );
			}

			for( uint32_t i = 0; i < positions.size(); ++i )
			{
				auto bonePos = TransformCoord( positions[i], meshToBoneTransforms[boneIndex] );
				auto indices = boneIndices[i];
				for( uint32_t j = 0; j < options.bonesPerVertex; ++j )
				{
					if( indices[j] == boneIndex && boneWeights && ( *boneWeights )[i][int32_t( j )] > 0 )
					{
						binding.bounds.Include( bonePos );
					}
				}
			}
			cmf::Modify( outMesh.boneBindings, allocator ).push_back( binding );
		}
	}
}

void SimplygonGenerateLods( cmf::Mesh& mesh, const SimplygonLodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator );

void GenerateLods( cmf::Mesh& mesh, const LodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	if( !options.generate )
	{
		return;
	}
	switch( options.method )
	{
	case LodGenerationMethod::Simplygon:
		SimplygonGenerateLods( mesh, options.simplygon, allocator, bufferAllocator );
		break;
	}
}

void SimplygonGenerateAudioOcclusionMesh( cmf::Mesh& mesh, const SimplygonAudioOcclusionMeshOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator );

void GenerateAudioOcclusionMesh( cmf::Mesh& mesh, const AudioOcclusionMeshOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	if( !options.generate )
	{
		return;
	}
	switch( options.method )
	{
	case AudioOcclusionMeshGenerationMethod::Simplygon:
		SimplygonGenerateAudioOcclusionMesh( mesh, options.simplygon, allocator, bufferAllocator );
		break;
	}
}

/** @brief Imports a mesh from an FBX node and constructs a corresponding cmf::Mesh structure.
 *
 * This function handles the entire process of importing a mesh, including vertex data, skinning information, blend shapes, topology creation, tangent generation, UV compression, and bone binding creation.
 *
 * @param meshNode The FBX node containing the mesh.
 * @param options The options for importing the mesh, which may affect various aspects of the import process.
 * @param allocator The memory allocator used for allocating mesh data.
 * @param bufferAllocator The buffer manager used for allocating vertex and index buffers.
 * @param systemTransform The coordinate system transformation to apply to the mesh data.
 * @return A cmf::Mesh structure representing the imported mesh.
 */
cmf::Mesh ImportMesh( const ufbx_node& meshNode, const MeshImportOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator, const CoordinateSystem& systemTransform )
{
	auto& mesh = *meshNode.mesh;
	auto meshTransform = ToMatrix( meshNode.geometry_to_world );

	auto skin = ImportSkin( mesh, options );

	auto outMesh = cmf::Mesh{};
	auto outLod = cmf::MeshLod{};

	outMesh.name = allocator.AllocateString( ToString( meshNode.name ) );

	const auto& geom = mesh;
	outMesh.decl = CreateVertexDeclaration( geom, options, !skin.weights.empty(), allocator );
	outLod.vb = FillVertexBuffer( geom, outMesh.decl, skin.weights, options, bufferAllocator, meshTransform, systemTransform );

	auto [morphTargets, lodMorphTargets] = ImportBlendShapes( meshNode, options, allocator, bufferAllocator, systemTransform );
	outMesh.morphTargets = morphTargets;
	outLod.morphTargets = lodMorphTargets;

	for( int j = 0; j < geom.material_parts.count; ++j )
	{
		auto& meshArea = cmf::Modify( outMesh.areas, allocator ).emplace_back();
		if( geom.material_part_usage_order[j] < mesh.materials.count )
		{
			const auto& material = mesh.materials[geom.material_part_usage_order[j]];
			meshArea.name = allocator.AllocateString( ToString( material->name ) );
		}
		else
		{
			meshArea.name = allocator.AllocateString( "default" );
		}
	}
	CreateTopology( outLod, geom, allocator, bufferAllocator );

	cmf::Modify( outMesh.lods, allocator ).emplace_back( outLod );

	if( options.tangents > 0 && ( options.alwaysComputeTangents || !geom.vertex_tangent.exists ) )
	{
		cmf::FlipTangentOptions flipOptions{};
		flipOptions.flipBinormal = options.flipV;
		GenerateTangents( outMesh, 0, flipOptions, true, allocator, bufferAllocator );
	}
	for( uint32_t tangentIndex = 1; tangentIndex < options.tangents; ++tangentIndex )
	{
		cmf::FlipTangentOptions flipOptions{};
		flipOptions.flipBinormal = options.flipV;
		GenerateTangents( outMesh, tangentIndex, flipOptions, true, allocator, bufferAllocator );
	}
	RemoveDuplicateVertices( outMesh.lods[0], bufferAllocator );


	auto uvDensities = CalculateUvDensities( outMesh, bufferAllocator );
	cmf::Modify( outMesh.uvDensities, allocator ).insert( outMesh.uvDensities.end(), begin( uvDensities ), end( uvDensities ) );

	outMesh.bounds = cmf::CalculateBounds( outMesh, bufferAllocator );
	UpdateMeshAreaData( outMesh, allocator, bufferAllocator );
	CreateBoneBindings( outMesh, skin, GetMeshToBoneTransforms( skin, meshTransform, systemTransform ), options, allocator, bufferAllocator );

	GenerateLods( outMesh, options.lods, allocator, bufferAllocator );

	// Remove vertex colors if they were added for LOD generation but are not desired in the final mesh
	if( FindElement( outMesh.decl, cmf::Usage::Color, LOCKED_VERTEX_USAGE_INDEX ) )
	{
		cmf::Span<cmf::VertexElement> newDecl;
		uint32_t offset = 0;
		for( auto element : outMesh.decl )
		{
			if( element.usage != cmf::Usage::Color || element.usageIndex != LOCKED_VERTEX_USAGE_INDEX )
			{
				element.offset = offset;
				offset += cmf::GetVertexElementSize( element );
				cmf::Modify( newDecl, allocator ).push_back( element );
			}
		}
		for( auto& lod : outMesh.lods )
		{
			lod.vb = cmf::ChangeBufferVertexDeclaration( lod.vb, outMesh.decl, newDecl, allocator, bufferAllocator );
		}
		outMesh.decl = newDecl;
	}

	if( options.compressTangents )
	{
		auto compression = options.legacyCompressedTangents ? cmf::TangentCompression::PackedTangentLegacy : cmf::TangentCompression::PackedTangent;
		for( uint32_t tangentIndex = 0; tangentIndex < options.tangents; ++tangentIndex )
		{
			CompressTangents( outMesh, tangentIndex, tangentIndex + 1 != options.tangents, compression, nullptr, allocator, bufferAllocator );
		}
	}

	for( auto& lod : outMesh.lods )
	{
		lod.ib = ConvertTo16BitIndexBuffer( lod.ib, allocator, bufferAllocator );
	}

	CompressElements( outMesh, cmf::Usage::TexCoord, options.uvType, allocator, bufferAllocator );
	CompressElements( outMesh, cmf::Usage::Color, options.colorType, allocator, bufferAllocator );

	GenerateAudioOcclusionMesh( outMesh, options.audioOcclusionMesh, allocator, bufferAllocator );

	OptimizeBuffers( outMesh, bufferAllocator );

	return outMesh;
}

/** @brief Finds the skeleton index for a given mesh based on the bone map.
 *
 * @param mesh The FBX mesh to search for skeleton information.
 * @param boneMap The mapping of bones to skeleton indices.
 * @return The skeleton index if found, or 0xFF if not found.
 */
uint8_t FindSkeleton( const ufbx_mesh& mesh, const BoneMap& boneMap )
{
	if( mesh.skin_deformers.count > 0 )
	{
		auto skin = mesh.skin_deformers[0];
		for( int j = 0; j < skin->clusters.count; ++j )
		{
			const auto cluster = skin->clusters[j];
			if( cluster->vertices.count == 0 )
			{
				continue;
			}
			auto link = cluster->bone_node;
			auto found = boneMap.find( link );
			if( found != boneMap.end() )
			{
				return found->second.skeletonIndex;
			}
		}
	}
	return 0xFF;
}

cmf::Span<cmf::Mesh> ImportMeshes( const ufbx_scene& scene, const MeshImportOptions& options, const BoneMap& boneMap, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator, const CoordinateSystem& systemTransform )
{
	cmf::Span<cmf::Mesh> meshes;
	if( options.importMeshes )
	{
		for( auto mesh : scene.nodes )
		{
			if( mesh->mesh == nullptr )
			{
				continue;
			}
			if( !options.namedFilter( ToString( mesh->name ) ) )
			{
				continue;
			}
			auto cmfMesh = ImportMesh( *mesh, options, allocator, bufferAllocator, systemTransform );
			cmfMesh.skeleton = FindSkeleton( *mesh->mesh, boneMap );
			cmf::Modify( meshes, allocator ).push_back( cmfMesh );
		}
	}
	return meshes;
}
