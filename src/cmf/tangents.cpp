// Copyright © 2026 CCP ehf.

#include "cmf/tangents.h"
#include "cmf/declutils.h"
#include "cmf/bufferstreams.h"
#include "cmf/bufferutils.h"
#include <array>
#include <optional>
#include <meshoptimizer.h>
#include <mikktspace.h>

namespace
{

struct TangentElements
{
	cmf::VertexElement normal;
	cmf::VertexElement tangent;
	cmf::VertexElement bitangent;
};

std::optional<TangentElements> FindTangentElements( const cmf::Span<cmf::VertexElement>& decl, uint32_t usageIndex )
{
	const auto* normal = FindElement( decl, cmf::Usage::Normal );
	const auto* tangent = FindElement( decl, cmf::Usage::Tangent, usageIndex );
	const auto* bitangent = FindElement( decl, cmf::Usage::Binormal, usageIndex );
	if( normal && tangent && bitangent )
	{
		return TangentElements{ *normal, *tangent, *bitangent };
	}
	return {};
}

const cmf::VertexElement* FindPackedTangentElement( const cmf::Span<cmf::VertexElement>& decl, uint32_t usageIndex )
{
	if( const auto* packedTangentElement = FindElement( decl, cmf::Usage::PackedTangent, usageIndex ) )
	{
		return packedTangentElement;
	}
	return FindElement( decl, cmf::Usage::PackedTangentLegacy, usageIndex );
}


void GenerateMikkTSpaceTangents(
	cmf::ConstBufferElementStream<Vector3> positions,
	cmf::ConstBufferElementStream<Vector3> normals,
	cmf::ConstBufferElementStream<Vector2> texCoords,
	const cmf::FlipTangentOptions& flip,
	cmf::BufferElementStream<Vector3> tangents,
	cmf::BufferElementStream<Vector3> bitangents )
{
	const int vertexCount = (int)positions.size();
	const int faceCount = vertexCount / 3;


	struct MikkTSpaceData
	{
		int faceCount;
		cmf::FlipTangentOptions flip;

		cmf::ConstBufferElementStream<Vector3> positions;
		cmf::ConstBufferElementStream<Vector3> normals;
		cmf::ConstBufferElementStream<Vector2> texCoords;

		cmf::BufferElementStream<Vector3> tangents;
		cmf::BufferElementStream<Vector3> bitangents;
	};

	MikkTSpaceData data = { faceCount, flip, positions, normals, texCoords, tangents, bitangents };

	SMikkTSpaceInterface interface = {};

	interface.m_getNumFaces = []( const SMikkTSpaceContext* ctx ) -> int {
		auto* data = static_cast<MikkTSpaceData*>( ctx->m_pUserData );
		return data->faceCount;
	};

	interface.m_getNumVerticesOfFace = []( const SMikkTSpaceContext*, int ) -> int {
		return 3;
	};

	interface.m_getPosition = []( const SMikkTSpaceContext* ctx, float pos[3], int face, int vert ) {
		auto* data = static_cast<MikkTSpaceData*>( ctx->m_pUserData );
		auto position = data->positions[face * 3 + vert];
		pos[0] = position[0];
		pos[1] = position[1];
		pos[2] = position[2];
	};

	interface.m_getNormal = []( const SMikkTSpaceContext* ctx, float norm[3], int face, int vert ) {
		auto* data = static_cast<MikkTSpaceData*>( ctx->m_pUserData );
		auto normal = data->normals[face * 3 + vert];
		norm[0] = normal[0];
		norm[1] = normal[1];
		norm[2] = normal[2];
	};

	interface.m_getTexCoord = []( const SMikkTSpaceContext* ctx, float uv[2], int face, int vert ) {
		auto* data = static_cast<MikkTSpaceData*>( ctx->m_pUserData );
		auto texCoord = data->texCoords[face * 3 + vert];
		uv[0] = texCoord[0];
		uv[1] = texCoord[1];
	};

	interface.m_setTSpaceBasic = []( const SMikkTSpaceContext* ctx, const float tangentFloats[3], float sign, int face, int vert ) {
		auto* data = static_cast<MikkTSpaceData*>( ctx->m_pUserData );
		const int index = face * 3 + vert;
		auto normal = data->normals[index];
		Vector3 tangent = Vector3( tangentFloats[0], tangentFloats[1], tangentFloats[2] );
		auto bitangent = Cross( tangent, normal ) * sign;
		if( data->flip.flipTangent )
		{
			tangent = -tangent;
		}
		if( data->flip.flipBinormal )
		{
			bitangent = -bitangent;
		}
		data->tangents.set( index, tangent );
		data->bitangents.set( index, bitangent );
	};


	const SMikkTSpaceContext context = { &interface, &data };

	// TODO: expose tolerance as function argument
	genTangSpaceDefault( &context );
}
}


namespace cmf
{


bool GenerateTangents( Mesh& mesh, uint32_t usageIndex, const FlipTangentOptions& flip, bool forceRebuild, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	bool success = true;

	// TODO: check for already existing packed tangents

	// Check if the mesh already has tangents and bitangents.
	auto meshTangentElement = FindElement( mesh.decl, Usage::Tangent, usageIndex );
	auto meshBitangentElement = FindElement( mesh.decl, Usage::Binormal, usageIndex );
	bool rebuildMeshTangents = forceRebuild || !meshTangentElement || !meshBitangentElement;

	auto morphTargetTangentElement = FindElement( mesh.morphTargets.decl, Usage::Tangent, usageIndex );
	auto morphTargetBitangentElement = FindElement( mesh.morphTargets.decl, Usage::Binormal, usageIndex );
	bool rebuildMorphTargetTangents = !mesh.morphTargets.targets.empty() && ( forceRebuild || !morphTargetTangentElement || !morphTargetBitangentElement );

	if( !rebuildMeshTangents && !rebuildMorphTargetTangents )
	{
		return success;
	}

	auto createVertexDeclarationWithTangents = []( const Span<VertexElement>& decl, uint32_t usageIndex, MemoryAllocator& allocator ) {
		Span<VertexElement> newVertexDeclaration;
		{
			uint32_t offset = 0;
			for( VertexElement element : decl )
			{
				if( ( element.usage == Usage::Tangent || element.usage == Usage::Binormal ) && element.usageIndex == usageIndex )
				{
					// Omit any existing tangents.
					continue;
				}

				VertexElement newElement = element;
				newElement.offset = offset;
				Modify( newVertexDeclaration, allocator ).push_back( newElement );
				offset += GetVertexElementSize( element );

				if( element.usage == Usage::Normal && element.usageIndex == 0 )
				{
					// Insert the tangent and bitangent after the normal.

					VertexElement newTangentElement = { Usage::Tangent, (uint8_t)usageIndex, ElementType::Float32, 3, offset };
					Modify( newVertexDeclaration, allocator ).push_back( newTangentElement );
					offset += GetVertexElementSize( newTangentElement );

					VertexElement newBitangentElement = { Usage::Binormal, (uint8_t)usageIndex, ElementType::Float32, 3, offset };
					Modify( newVertexDeclaration, allocator ).push_back( newBitangentElement );
					offset += GetVertexElementSize( newBitangentElement );
				}
			}
		}

		return newVertexDeclaration;
	};

	//Completely un-index the mesh.
	for( auto& lod : mesh.lods )
	{
		lod.vb = UnapplyIndexBuffer( lod.vb, lod.ib, allocator, bufferManager );
		for( auto& morphTarget : lod.morphTargets )
		{
			morphTarget.vb = UnapplyIndexBuffer( morphTarget.vb, lod.ib, allocator, bufferManager );
		}
		// Replace the index buffer with a simple identity index buffer.
		lod.ib = MakeIdentityIndexBuffer( lod.ib.size / lod.ib.stride, allocator, bufferManager );
	}


	if( rebuildMeshTangents )
	{

		// Make sure that we have the required vertex attributes to generate tangents.
		auto positionElement = FindElement( mesh.decl, Usage::Position );
		auto normalElement = FindElement( mesh.decl, Usage::Normal );
		auto texCoordElement = FindElement( mesh.decl, Usage::TexCoord, usageIndex );

		if( !positionElement || !normalElement || !texCoordElement )
		{
			success = false;
		}
		else
		{
			// Create a new vertex declaration with the new tangents
			Span<VertexElement> newVertexDeclaration = createVertexDeclarationWithTangents( mesh.decl, usageIndex, allocator );

			VertexElement newTangentElement = *FindElement( newVertexDeclaration, Usage::Tangent, usageIndex );
			VertexElement newBitangentElement = *FindElement( newVertexDeclaration, Usage::Binormal, usageIndex );

			for( auto& lod : mesh.lods )
			{
				auto newVb = ChangeBufferVertexDeclaration( lod.vb, mesh.decl, newVertexDeclaration, allocator, bufferManager );

				GenerateMikkTSpaceTangents(
					ConstBufferElementStream<Vector3>( *positionElement, lod.vb, bufferManager ),
					ConstBufferElementStream<Vector3>( *normalElement, lod.vb, bufferManager ),
					ConstBufferElementStream<Vector2>( *texCoordElement, lod.vb, bufferManager ),
					flip,
					BufferElementStream<Vector3>( newTangentElement, newVb, bufferManager ),
					BufferElementStream<Vector3>( newBitangentElement, newVb, bufferManager ) );

				lod.vb = newVb;
			}

			mesh.decl = newVertexDeclaration;
		}
	}

	if( rebuildMorphTargetTangents )
	{

		// Make sure that we have the required vertex attributes to generate morph target tangents.
		auto positionElement = FindElement( mesh.morphTargets.decl, Usage::Position );
		auto normalElement = FindElement( mesh.morphTargets.decl, Usage::Normal );
		auto morphTexCoordElement = FindElement( mesh.morphTargets.decl, Usage::TexCoord, usageIndex );
		auto meshTexCoordElement = FindElement( mesh.decl, Usage::TexCoord, usageIndex );

		if( !positionElement || !normalElement || ( !morphTexCoordElement && !meshTexCoordElement ) )
		{
			success = false;
		}
		else
		{
			// Create a new vertex declaration with the new tangents
			Span<VertexElement> newVertexDeclaration = createVertexDeclarationWithTangents( mesh.morphTargets.decl, usageIndex, allocator );

			VertexElement newTangentElement = *FindElement( newVertexDeclaration, Usage::Tangent, usageIndex );
			VertexElement newBitangentElement = *FindElement( newVertexDeclaration, Usage::Binormal, usageIndex );

			for( auto& lod : mesh.lods )
			{
				for( auto& morphTarget : lod.morphTargets )
				{
					auto newVb = ChangeBufferVertexDeclaration( morphTarget.vb, mesh.morphTargets.decl, newVertexDeclaration, allocator, bufferManager );
					auto newTangents = BufferElementStream<Vector3>( newTangentElement, newVb, bufferManager );
					auto newBitangents = BufferElementStream<Vector3>( newBitangentElement, newVb, bufferManager );

					GenerateMikkTSpaceTangents(
						ConstBufferElementStream<Vector3>( *positionElement, morphTarget.vb, bufferManager ),
						ConstBufferElementStream<Vector3>( *normalElement, morphTarget.vb, bufferManager ),
						morphTexCoordElement ?
							ConstBufferElementStream<Vector2>( *morphTexCoordElement, morphTarget.vb, bufferManager ) :
							ConstBufferElementStream<Vector2>( *meshTexCoordElement, lod.vb, bufferManager ),
						flip,
						BufferElementStream<Vector3>( newTangentElement, newVb, bufferManager ),
						BufferElementStream<Vector3>( newBitangentElement, newVb, bufferManager ) );

					morphTarget.vb = newVb;
				}
			}

			mesh.morphTargets.decl = newVertexDeclaration;
		}
	}


	for( auto& lod : mesh.lods )
	{
		//Re-index the data again.
		RemoveDuplicateVertices( lod, bufferManager );
	}

	return success;
}

namespace
{
Vector4 PackTangentsQuaternion( Vector3 normal, Vector3 tangent, Vector3 bitangent )
{
	// Figure out if we need to flip the normal before compressing.
	const bool flipNormal = Dot( normal, Cross( tangent, bitangent ) ) < 0.0f;
	if( flipNormal )
	{
		normal = -normal;
	}

	// Construct a TBN matrix.
	const Matrix matrix(
		tangent.x, tangent.y, tangent.z, 0.0f, bitangent.x, bitangent.y, bitangent.z, 0.0f, normal.x, normal.y, normal.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f );

	// Convert matrix to quaternion.
	Quaternion q = Normalize( RotationQuaternion( matrix ) );

	// We need to be able to reconstruct the W-component during unpacking, so make sure that it's always positive.
	if( q.w < 0.0f )
	{
		q = -q; // Represents the same rotation.
	}

	return { q.x, q.y, q.z, flipNormal ? -1.0f : 1.0f }; // W stores the normal sign
}


std::tuple<Vector3, Vector3, Vector3> UnpackTangentsQuaternion( Vector4 t )
{

	/*
    
    Unoptimized code that clearly shows the steps of unpacking the quaternion and reconstructing the TBN matrix.
     
    // Reconstruct the w-value of the quaternion. This is guaranteed to be positive, so we don't need to store a sign for it.
	float w = sqrtf( std::max( 0.0f, 1.0f - t.x * t.x - t.y * t.y - t.z * t.z ) );

    // Construct the quaternion.
	Quaternion q = Quaternion( t.x, t.y, t.z, w );

    // Turn it into a TBN matrix.
	Matrix m = RotationMatrix( q );

    // Extract the vectors from the matrix. Normalization isn't strictly needed, but slightly improves precision
	*tangent = Normalize( Vector3( m._11, m._12, m._13 ) );
	*bitangent = Normalize( Vector3( m._21, m._22, m._23 ) );
	*normal = Normalize( Vector3( m._31, m._32, m._33 ) ) * t.w; // t.w is the normal sign
    */

	// Heavily optimized shader-ready code that constructs the TBN matrix.

	// Extract the xyz components and square them
	float x = t.x;
	float y = t.y;
	float z = t.z;
	float x2 = x * x;
	float y2 = y * y;
	float z2 = z * z;

	// Optimized fma() chain to reconstruct W = sqrt(1 - x2 - y2 - z2)
	// Don't use the above x2, y2 and z2 values, to reduce pipeline dependencies.
	float w2 = std::clamp( fma( z, -z, fma( y, -y, fma( x, -x, 1.0f ) ) ), 0.0f, 1.0f );
	float w = sqrt( w2 );

	// Calculate shared values.
	// These multiplications by 2.0f are free on some GPUs.
	float xy = x * y * 2.0f;
	float xz = x * z * 2.0f;
	float yz = y * z * 2.0f;
	float xw = x * w * 2.0f;
	float yw = y * w * 2.0f;
	float zw = z * w * 2.0f;

	// Compute the three vectors.
	// This takes advantage of the fact that we know that w2 = 1 - x2 - y2 - z2 to simplify the math along the diagonal.
	//Equivalent to:
	//  *tangent =   Vector3( 1.0f - 2.0f * y2 - 2.0f * z2, + xy + zw, + xz - yw );
	//  *bitangent = Vector3( - zw + xy, 1.0f - 2.0f * x2 - 2.0f * z2, + yz + xw );
	//  *normal =    Vector3( + yw + xz, + yz - xw, 1.0f - 2.0f * x2 - 2.0f * y2 ) * t.w; // packed normal sign multiplication

	auto tangent = Vector3( fma( -2.0f, y2, fma( -2.0f, z2, 1.0f ) ), +xy + zw, +xz - yw );
	auto bitangent = Vector3( -zw + xy, fma( -2.0f, x2, fma( -2.0f, z2, 1.0f ) ), +yz + xw );
	auto normal = Vector3( +yw + xz, +yz - xw, fma( -2.0f, x2, fma( -2.0f, y2, 1.0f ) ) ) * t.w; // packed normal sign multiplication

	return { normal, tangent, bitangent };
}

//Remaps a value that lies between oldMin and oldMax to [0, 1] range
inline float Remap( float x, float oldMin, float oldMax )
{
	return std::clamp( ( x - oldMin ) / ( oldMax - oldMin ), 0.0f, 1.0f );
}

Vector4 PackTangentsLegacy( Vector3 normal, Vector3 tangent, Vector3 bitangent )
{
	float angles[4];
	angles[0] = atan2( tangent.y, tangent.x );
	angles[1] = acos( std::min( 1.f, std::max( -1.f, tangent.z ) ) );
	angles[2] = atan2( bitangent.y, bitangent.x );
	angles[3] = acos( std::min( 1.f, std::max( -1.f, bitangent.z ) ) );
	if( Dot( normal, Cross( tangent, bitangent ) ) < 0 )
	{
		angles[1] = -angles[1];
		angles[3] = -angles[3];
	}
	Vector4 packed;
	packed.x = Remap( angles[0], -XM_PI, +XM_PI );
	packed.y = Remap( angles[1], -XM_PI, +XM_PI );
	packed.z = Remap( angles[2], -XM_PI, +XM_PI );
	packed.w = Remap( angles[3], -XM_PI, +XM_PI );
	return packed;
}


std::tuple<Vector3, Vector3, Vector3> UnpackTangentsLegacy( Vector4 t )
{
	auto angles = t * XM_2PI - Vector4( XM_PI, XM_PI, XM_PI, XM_PI );
	float sc0x = sin( angles[0] );
	float sc0y = cos( angles[0] );
	float sc0z = sin( angles[1] );
	float sc0w = cos( angles[1] );
	float sc1x = sin( angles[2] );
	float sc1y = cos( angles[2] );
	float sc1z = sin( angles[3] );
	float sc1w = cos( angles[3] );

	auto tangent = Vector3( sc0y * abs( sc0z ), sc0x * abs( sc0z ), sc0w );
	auto bitangent = Vector3( sc1y * abs( sc1z ), sc1x * abs( sc1z ), sc1w );
	auto normal = Cross( bitangent, tangent );

	if( angles[3] > 0 && angles[1] > 0 )
	{
		normal = -normal;
	}
	return { normal, tangent, bitangent };
}
}


Vector4 PackTangents( TangentCompression compression, const Vector3& normal, const Vector3& tangent, const Vector3& bitangent )
{
	if( compression == TangentCompression::PackedTangent )
	{
		return PackTangentsQuaternion( normal, tangent, bitangent );
	}
	return PackTangentsLegacy( normal, tangent, bitangent );
}

std::tuple<Vector3, Vector3, Vector3> UnpackTangents( TangentCompression compression, const Vector4& t )
{
	if( compression == TangentCompression::PackedTangent )
	{
		return UnpackTangentsQuaternion( t );
	}
	return UnpackTangentsLegacy( t );
}

bool CompressTangents( Mesh& mesh, uint32_t usageIndex, bool retainNormal, TangentCompression compression, CompressionErrorMetrics* metrics, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	// Check if tangents are already packed.
	if( FindPackedTangentElement( mesh.decl, usageIndex ) )
	{
		return false;
	}
	// Check if the mesh has the required data to pack tangents.
	if( !FindTangentElements( mesh.decl, usageIndex ) )
	{
		return false;
	}

	// Create a new vertex declaration with the new tangents, and a mapping for copying to it.
	auto ChangeDeclaration = []( const Span<VertexElement>& decl, uint32_t usageIndex, bool retainNormal, TangentCompression compression, MemoryAllocator& allocator ) {
		Span<VertexElement> newVertexDeclaration;
		uint32_t offset = 0;
		for( const auto& element : decl )
		{
			if( ( element.usage == Usage::Tangent || element.usage == Usage::Binormal ) && element.usageIndex == usageIndex )
			{
				// Omit the unpacked tangents.
				continue;
			}

			if( element.usage == Usage::Normal && element.usageIndex == 0 )
			{
				// Insert the packed tangent
				VertexElement newPackedTangentElement;
				if( compression == TangentCompression::PackedTangent )
				{
					newPackedTangentElement = { Usage::PackedTangent, (uint8_t)usageIndex, ElementType::Int16Norm, 4, offset };
				}
				else
				{
					newPackedTangentElement = { Usage::PackedTangentLegacy, (uint8_t)usageIndex, ElementType::UInt16Norm, 4, offset };
				}
				Modify( newVertexDeclaration, allocator ).push_back( newPackedTangentElement );
				offset += GetVertexElementSize( newPackedTangentElement );
			}

			if( !retainNormal && element.usage == Usage::Normal && element.usageIndex == 0 )
			{
				// Omit the normal.
				continue;
			}

			VertexElement newElement = element;
			newElement.offset = offset;
			Modify( newVertexDeclaration, allocator ).push_back( newElement );
			offset += GetVertexElementSize( element );
		}
		return newVertexDeclaration;
	};

	auto newVertexDeclaration = ChangeDeclaration( mesh.decl, usageIndex, retainNormal, compression, allocator );

	struct TotalMetrics
	{
		uint32_t vertexCount = 0;

		double normalError = 0.0;
		double tangentError = 0.0;
		double bitangentError = 0.0;
	};
	TotalMetrics totals;

	auto CompressBuffer = [&allocator, &bufferManager, compression, usageIndex]( const BufferView& vb, const Span<VertexElement>& oldDecl, const Span<VertexElement>& newDecl, TotalMetrics* totals ) {
		auto tangentElements = FindTangentElements( oldDecl, usageIndex );
		const auto* newPackedTangentElement = FindPackedTangentElement( newDecl, usageIndex );

		auto newVertexBuffer = ChangeBufferVertexDeclaration( vb, oldDecl, newDecl, allocator, bufferManager );
		const uint32_t vertexCount = newVertexBuffer.size / newVertexBuffer.stride;

		const ConstBufferElementStream<Vector3> normals( tangentElements->normal, vb, bufferManager );
		const ConstBufferElementStream<Vector3> tangents( tangentElements->tangent, vb, bufferManager );
		const ConstBufferElementStream<Vector3> bitangents( tangentElements->bitangent, vb, bufferManager );

		const BufferElementStream<Vector4> packedTangents( *newPackedTangentElement, newVertexBuffer, bufferManager );

		for( uint32_t i = 0; i < vertexCount; i++ )
		{
			const Vector3 normal = Normalize( normals[i] );
			const Vector3 tangent = Normalize( tangents[i] );
			const Vector3 bitangent = Normalize( bitangents[i] );
			const Vector4 packedTangent = PackTangents( compression, normal, tangent, bitangent );
			packedTangents.set( i, packedTangent );

			if( totals )
			{
				const auto packedTangent2 = packedTangents[i];
				auto [normal2, tangent2, bitangent2] = UnpackTangents( compression, packedTangent2 );

				totals->normalError += acosf( std::clamp( Dot( normal, normal2 ), -1.0f, +1.0f ) );
				totals->tangentError += acosf( std::clamp( Dot( tangent, tangent2 ), -1.0f, +1.0f ) );
				totals->bitangentError += acosf( std::clamp( Dot( bitangent, bitangent2 ), -1.0f, +1.0f ) );
			}
		}
		if( totals )
		{
			totals->vertexCount += vertexCount;
		}

		return newVertexBuffer;
	};

	for( auto& lod : mesh.lods )
	{
		lod.vb = CompressBuffer( lod.vb, mesh.decl, newVertexDeclaration, ( metrics && mesh.lods.begin() == &lod ) ? &totals : nullptr );
	}

	mesh.decl = newVertexDeclaration;

	if( auto morphTargetTangents = FindTangentElements( mesh.morphTargets.decl, usageIndex ) )
	{
		auto newMorphDecl = ChangeDeclaration( mesh.morphTargets.decl, usageIndex, retainNormal, compression, allocator );
		for( auto& lod : mesh.lods )
		{
			for( auto& morphTarget : lod.morphTargets )
			{
				morphTarget.vb = CompressBuffer( morphTarget.vb, mesh.morphTargets.decl, newMorphDecl, nullptr );
			}
		}
		mesh.morphTargets.decl = newMorphDecl;
	}

	if( metrics )
	{
		if( totals.vertexCount > 0 )
		{
			const float radiansToDegrees = 180.0f / 3.14159265359f;
			metrics->averageNormalErrorDegrees = float( totals.normalError / totals.vertexCount * radiansToDegrees );
			metrics->averageTangentErrorDegrees = float( totals.tangentError / totals.vertexCount * radiansToDegrees );
			metrics->averageBitangentErrorDegrees = float( totals.bitangentError / totals.vertexCount * radiansToDegrees );
		}
		else
		{
			metrics->averageNormalErrorDegrees = 0.0f;
			metrics->averageTangentErrorDegrees = 0.0f;
			metrics->averageBitangentErrorDegrees = 0.0f;
		}
	}
	return true;
}

bool DecompressTangents( Mesh& mesh, uint32_t usageIndex, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	// Check if tangents are already packed.
	const auto* packedTangentElement = FindPackedTangentElement( mesh.decl, usageIndex );
	if( !packedTangentElement )
	{
		return false;
	}

	// Create a new vertex declaration with the new tangents, and a mapping for copying to it.
	auto ChangeDeclaration = []( const Span<VertexElement>& decl, uint32_t usageIndex, MemoryAllocator& allocator ) {
		Span<VertexElement> newDecl;
		uint32_t offset = 0;
		for( const auto& element : decl )
		{
			if( ( element.usage == Usage::PackedTangent || element.usage == Usage::PackedTangentLegacy ) && element.usageIndex == usageIndex )
			{
				// Insert the unpacked tangents
				auto newNormalElement = VertexElement{ Usage::Normal, (uint8_t)0, ElementType::Float32, 3, offset };
				Modify( newDecl, allocator ).push_back( newNormalElement );
				offset += GetVertexElementSize( newNormalElement );

				auto newTangentElement = VertexElement{ Usage::Tangent, (uint8_t)usageIndex, ElementType::Float32, 3, offset };
				Modify( newDecl, allocator ).push_back( newTangentElement );
				offset += GetVertexElementSize( newTangentElement );
				auto newBitangentElement = VertexElement{ Usage::Binormal, (uint8_t)usageIndex, ElementType::Float32, 3, offset };
				Modify( newDecl, allocator ).push_back( newBitangentElement );
				offset += GetVertexElementSize( newBitangentElement );
			}
			if( ( element.usage == Usage::PackedTangent || element.usage == Usage::PackedTangentLegacy || element.usage == Usage::Tangent || element.usage == Usage::Binormal ) && element.usageIndex == usageIndex )
			{
				// Omit any existing tangents.
				continue;
			}
			if( element.usage == Usage::Normal && element.usageIndex == 0 )
			{
				// Omit any existing normal
				continue;
			}
			VertexElement newElement = element;
			newElement.offset = offset;
			Modify( newDecl, allocator ).push_back( newElement );
			offset += GetVertexElementSize( element );
		}
		return newDecl;
	};

	auto DecompressBuffer = [&allocator, &bufferManager]( const BufferView& vb, const Span<VertexElement>& oldDecl, const cmf::VertexElement& packedTangentElement, const Span<VertexElement>& newDecl ) {
		auto compression = ( packedTangentElement.usage == Usage::PackedTangent ) ? TangentCompression::PackedTangent : TangentCompression::PackedTangentLegacy;
		auto newVertexBuffer = ChangeBufferVertexDeclaration( vb, oldDecl, newDecl, allocator, bufferManager );
		const uint32_t vertexCount = newVertexBuffer.size / newVertexBuffer.stride;

		const auto tangentElements = FindTangentElements( newDecl, packedTangentElement.usageIndex );

		const ConstBufferElementStream<Vector4> packedTangents( packedTangentElement, vb, bufferManager );
		const BufferElementStream<Vector3> normals( tangentElements->normal, newVertexBuffer, bufferManager );
		const BufferElementStream<Vector3> tangents( tangentElements->tangent, newVertexBuffer, bufferManager );
		const BufferElementStream<Vector3> bitangents( tangentElements->bitangent, newVertexBuffer, bufferManager );

		for( uint32_t i = 0; i < vertexCount; i++ )
		{
			// Write out the new tangent and bitangent.
			const Vector4 packedTangent = packedTangents[i];

			auto [normal, tangent, bitangent] = UnpackTangents( compression, packedTangent );
			normals.set( i, normal );
			tangents.set( i, tangent );
			bitangents.set( i, bitangent );
		}
		return newVertexBuffer;
	};

	auto newDecl = ChangeDeclaration( mesh.decl, usageIndex, allocator );
	for( auto& lod : mesh.lods )
	{
		lod.vb = DecompressBuffer( lod.vb, mesh.decl, *packedTangentElement, newDecl );
	}
	mesh.decl = newDecl;

	if( const auto* morphTargetTangents = FindPackedTangentElement( mesh.morphTargets.decl, usageIndex ) )
	{
		auto morphTargetDecl = ChangeDeclaration( mesh.morphTargets.decl, usageIndex, allocator );
		for( auto& lod : mesh.lods )
		{
			for( auto& morphTarget : lod.morphTargets )
			{
				morphTarget.vb = DecompressBuffer( morphTarget.vb, mesh.morphTargets.decl, *morphTargetTangents, morphTargetDecl );
			}
		}
		mesh.morphTargets.decl = morphTargetDecl;
	}
	return true;
}
}
