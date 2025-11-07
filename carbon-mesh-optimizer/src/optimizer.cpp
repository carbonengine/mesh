
#include "optimizer.h"
#include <string>
#include <functional>
#include <cmf/writer.h>
#include <meshoptimizer.h>
#include <mikktspace.h>

#include <attributes.h>

using namespace cmf;
using namespace cmf::optimizer;

OptBufferData readBuffer( const CmfContent* content, const cmf::BufferView view )
{
	if( view.size == 0 )
	{
		return { std::vector<uint8_t>(), 1 };
	}
	uint32_t vertexBufferOffset = content->m_cmfHeader->sections[view.index].offset + view.offset;
	const uint8_t* vertexData = content->m_fileContent.data() + vertexBufferOffset;
	return { std::vector<uint8_t>( vertexData, vertexData + view.size ), view.stride };
}

/*
OptBufferData convertVertexBuffer( const OptBufferData vertexBuffer, std::vector<VertexElement> newVertexDeclaration, std::function<void()> converter )
{

}
*/

OptBufferData convertIndexBuffer( const OptBufferData indexBuffer, const uint32_t newStride )
{
	if( indexBuffer.stride == newStride )
	{
		return indexBuffer;
	}

    uint32_t length = indexBuffer.length();
    
	OptBufferData newIndexBuffer;
	newIndexBuffer.data.resize( length * newStride );
	newIndexBuffer.stride = newStride;


    if (indexBuffer.stride == 2 && newStride == 4)
    {
		const uint16_t* oldData = reinterpret_cast<const uint16_t*>(indexBuffer.data.data());
		uint32_t* newData = reinterpret_cast<uint32_t*>( newIndexBuffer.data.data() );

        for( uint32_t i = 0; i < length; i++ )
		{
			newData[i] = oldData[i];
		}

    }
    else if (indexBuffer.stride == 4 && newStride == 2)
	{
		const uint32_t* oldData = reinterpret_cast<const uint32_t*>( indexBuffer.data.data() );
		uint16_t* newData = reinterpret_cast<uint16_t*>( newIndexBuffer.data.data() );

		for( uint32_t i = 0; i < length; i++ )
		{
			newData[i] = (uint16_t) oldData[i];
		}
    }
	else
	{
		printf( "Unsupported index buffer stride conversion: %d --> %d", indexBuffer.stride, newStride );
		return indexBuffer;
	}

    return newIndexBuffer;
}

Optimizer::Optimizer( CmfContent* content ) :
	content( content )
{

    for( Mesh& mesh : content->m_cmfData->meshes )
    {
		OptMesh optMesh;

        optMesh.name = std::string(mesh.name.begin(), mesh.name.end() );

        optMesh.vertexDeclaration = std::vector( mesh.decl.begin(), mesh.decl.end() );

        for( MeshLod& lod : mesh.lods )
        {
            OptMeshLod optLod;

			optLod.vertexBuffer = readBuffer( content, lod.vb );

            //Convert to 32-bit indices for simplicity during optimizations.
            OptBufferData indexBuffer = readBuffer( content, lod.ib );
			optLod.indexBuffer = convertIndexBuffer( indexBuffer, 4 );

			for( LodMeshArea& area : lod.areas )
			{
				optLod.areas.push_back( { area.firstElement * 3, area.elementCount * 3 } );
			}

			for( LodMorphTarget& morphTarget : lod.morphTargets )
			{
				optLod.morphTargetLods.push_back( { readBuffer( content, morphTarget.vb ) } );
			}

            optLod.threshold = lod.threshold;

			optMesh.lods.push_back( std::move( optLod ) );
        }

        for( MorphTarget& morphTarget : mesh.morphTargets )
        {
			optMesh.morphTargets.push_back( { std::vector( morphTarget.decl.begin(), morphTarget.decl.end() ) } );
        }

        meshes.push_back( std::move( optMesh ) );
    }

}

Optimizer::~Optimizer()
{
}

void Optimizer::generateTangents( uint32_t usageIndex, bool force )
{
	for( OptMesh& mesh : meshes )
	{

		std::vector<VertexElement>& vertexDeclaration = mesh.vertexDeclaration;


        // Check if the mesh already has tangents and bitangents.
		auto tangentElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::Tangent && v.usageIndex == usageIndex; } );
		auto bitangentElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::Binormal && v.usageIndex == usageIndex; } );

        bool shouldGenerate = force || tangentElement == vertexDeclaration.end() || bitangentElement == vertexDeclaration.end();
		if( !shouldGenerate )
        {
			printf( "Mesh %s already has tangents, using existing ones.\n", mesh.name.c_str() );
			continue;
        }

        // Make sure that we have the required vertex attributes to generate tangents.
		auto positionElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), []( const auto& v ) { return v.usage == Usage::Position && v.usageIndex == 0; } );
		auto normalElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), []( const auto& v ) { return v.usage == Usage::Normal && v.usageIndex == 0; } );
		auto texCoordElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::TexCoord && v.usageIndex == usageIndex; } );

        if( positionElement == vertexDeclaration.end() || normalElement == vertexDeclaration.end() || texCoordElement == vertexDeclaration.end() )
		{
			printf( "Failed to generate tangents for mesh %s\n", mesh.name.c_str() );

			if( positionElement == vertexDeclaration.end() )
				printf( "    No Position%d attribute found.\n", 0 );

			if( normalElement == vertexDeclaration.end() )
				printf( "    No Normal%d attribute found.\n", 0 );

			if( texCoordElement == vertexDeclaration.end() )
				printf( "    No TexCoord%d attribute found.\n", usageIndex );

            continue;
        }


        // Create a new vertex declaration with the new tangents, and a mapping for copying to it.
		std::vector<VertexElement> newVertexDeclaration;
		std::vector<std::pair<VertexElement, VertexElement>> vertexElementMapping;

        VertexElement newTangentElement;
		VertexElement newBitangentElement;
		uint32_t newVertexStride;

        {
			uint32_t offset = 0;
			for( VertexElement element : mesh.vertexDeclaration )
			{
				if( ( element.usage == Usage::Tangent || element.usage == Usage::Binormal ) && element.usageIndex == usageIndex )
				{
                    // Omit the old tangents.
					continue;
				}

				VertexElement newElement = element;
				newElement.offset = offset;
				newVertexDeclaration.push_back( newElement );
                offset += getElementTypeByteSize( element.type ) * element.elementCount;

                vertexElementMapping.emplace_back( element, newElement );
                
                if( element.usage == Usage::Normal && element.usageIndex == 0 )
				{
					// Insert the tangent and bitangent after the normal.

					newTangentElement = { Usage::Tangent, (uint8_t) usageIndex, ElementType::Float32, 3, offset };
					newVertexDeclaration.push_back( newTangentElement );
					offset += getElementTypeByteSize( ElementType::Float32 ) * 3;

					newBitangentElement = { Usage::Binormal, (uint8_t) usageIndex, ElementType::Float32, 3, offset };
					newVertexDeclaration.push_back( newBitangentElement );
					offset += getElementTypeByteSize( ElementType::Float32 ) * 3;
                }
			}

            newVertexStride = offset;
		}

		for( OptMeshLod& lod : mesh.lods )
		{
			struct MikkTSpaceData
			{
				const uint32_t* indexData;
				const uint32_t indexCount;

				const uint8_t* vertexData;
				const uint32_t vertexDataStride;

				const VertexElement position;
				const VertexElement normal;
				const VertexElement texCoord;


				std::vector<Vector4>& tangentData;
			};

            std::vector<Vector4> tangentData( lod.indexBuffer.length() );

			MikkTSpaceData data = {
				reinterpret_cast<uint32_t*>( lod.indexBuffer.data.data() ),
				lod.indexBuffer.length(),

				lod.vertexBuffer.data.data(),
                lod.vertexBuffer.stride,

                *positionElement,
                *normalElement,
                *texCoordElement,

                tangentData
			};

			SMikkTSpaceInterface interface = {};

			interface.m_getNumFaces = []( const SMikkTSpaceContext* ctx ) -> int {
				MikkTSpaceData* data = reinterpret_cast<MikkTSpaceData*>( ctx->m_pUserData );
				return data->indexCount / 3;
			};

			interface.m_getNumVerticesOfFace = []( const SMikkTSpaceContext* ctx, int ) -> int {
				return 3;
			};

			interface.m_getPosition = []( const SMikkTSpaceContext* ctx, float pos[3], int face, int vert ) {
				MikkTSpaceData* data = reinterpret_cast<MikkTSpaceData*>( ctx->m_pUserData );
				uint32_t index = data->indexData[face * 3 + vert];
				Vector4 position = readAttribute( data->position, data->vertexData + index * data->vertexDataStride );
				pos[0] = position[0];
				pos[1] = position[1];
				pos[2] = position[2];
			};

			interface.m_getNormal = []( const SMikkTSpaceContext* ctx, float norm[3], int face, int vert ) {
				MikkTSpaceData* data = reinterpret_cast<MikkTSpaceData*>( ctx->m_pUserData );
				uint32_t index = data->indexData[face * 3 + vert];
				Vector4 normal = readAttribute( data->normal, data->vertexData + index * data->vertexDataStride );
				norm[0] = normal[0];
				norm[1] = normal[1];
				norm[2] = normal[2];
			};

			interface.m_getTexCoord = []( const SMikkTSpaceContext* ctx, float uv[2], int face, int vert ) {
				MikkTSpaceData* data = reinterpret_cast<MikkTSpaceData*>( ctx->m_pUserData );
				uint32_t index = data->indexData[face * 3 + vert];
				Vector4 texCoord = readAttribute( data->texCoord, data->vertexData + index * data->vertexDataStride );
				uv[0] = texCoord[0];
				uv[1] = texCoord[1];
			};

			interface.m_setTSpaceBasic = []( const SMikkTSpaceContext* ctx, const float tangent[3], float sign, int face, int vert ) {
				MikkTSpaceData* data = reinterpret_cast<MikkTSpaceData*>( ctx->m_pUserData );

                data->tangentData[face * 3 + vert] = Vector4( tangent[0], tangent[1], tangent[2], sign );
			};
			

			SMikkTSpaceContext context = { &interface, &data };

            genTangSpaceDefault( &context );

            
            /* if( tangentElement != vertexDeclaration.end() && bitangentElement != vertexDeclaration.end() )
			{
				double totalTangentDifference = 0.0;
				float maxTangentDifference = 0.0;

				double totalBitangentDifference = 0.0;
				float maxBitangentDifference = 0.0;


				for( uint32_t i = 0; i < data.indexCount; i++ )
                {
					uint32_t index = data.indexData[i];

                    Vector4 previousTangent = readAttribute( *tangentElement, data.vertexData + index * data.vertexDataStride );
					Vector4 generatedTangent = tangentData[i];

                    Vector4 previousNormal = readAttribute( *normalElement, data.vertexData + index * data.vertexDataStride );
                    Vector4 previousBitangent = readAttribute( *bitangentElement, data.vertexData + index * data.vertexDataStride );

					Vector3 generatedBitangent = -Cross( Vector3( previousNormal.x, previousNormal.y, previousNormal.z ), Vector3( generatedTangent.x, generatedTangent.y, generatedTangent.z ) ) * tangentData[i].w;

					float tangentAngle = acosf( std::clamp( previousTangent.x * generatedTangent.x + previousTangent.y * generatedTangent.y + previousTangent.z * generatedTangent.z, -1.0f, +1.0f ) ) * ( 180.0f / 3.14159265359f );
					float bitangentAngle = acosf( std::clamp( previousBitangent.x * generatedBitangent.x + previousBitangent.y * generatedBitangent.y + previousBitangent.z * generatedBitangent.z, -1.0f, +1.0f ) ) * ( 180.0f / 3.14159265359f );

					totalTangentDifference += tangentAngle;
					maxTangentDifference = std::max( maxTangentDifference, tangentAngle );

					totalBitangentDifference += bitangentAngle;
					maxBitangentDifference = std::max( maxBitangentDifference, bitangentAngle );

					if( tangentAngle > 20 )
					{
						printf( "Large tangent change for %d: (%f, %f, %f) --> (%f, %f, %f), %f degrees\n",
								i,
								previousTangent.x,
								previousTangent.y,
								previousTangent.z,
								generatedTangent.x,
								generatedTangent.y,
								generatedTangent.z,
								tangentAngle );
					}

					if( bitangentAngle > 20 )
					{
						printf( "Large bitangent change for %d: (%f, %f, %f) --> (%f, %f, %f), %f degrees\n",
								i,
								previousBitangent.x,
								previousBitangent.y,
								previousBitangent.z,
								generatedBitangent.x,
								generatedBitangent.y,
								generatedBitangent.z,
								bitangentAngle );
					}
				}

				printf( "Average tangent change: %f degrees\n", totalTangentDifference / data.indexCount );
				printf( "Max tangent change: %f degrees\n", maxTangentDifference );

				printf( "Average bitangent change: %f degrees\n", totalBitangentDifference / data.indexCount );
				printf( "Max bitangent change: %f degrees\n", maxBitangentDifference );
            }*/
            

            

            
            {

				// Create a new vertex and index buffer that includes the new tangent and bitangent data.
				OptBufferData newVertexBuffer;
				newVertexBuffer.stride = newVertexStride;
				newVertexBuffer.data.resize( data.indexCount * newVertexStride );

				OptBufferData newIndexBuffer;
				newIndexBuffer.stride = 4;
				newIndexBuffer.data.resize( data.indexCount * 4 );

				for( uint32_t i = 0; i < data.indexCount; i++ )
				{
					uint32_t index = data.indexData[i];

					const uint8_t* oldData = data.vertexData + index * data.vertexDataStride;
					uint8_t* newData = newVertexBuffer.data.data() + i * newVertexBuffer.stride;

                    // Move all unmodified attributes over.
					for( std::pair<VertexElement, VertexElement> elements : vertexElementMapping )
					{
						Vector4 attribute = readAttribute( elements.first, oldData );
                        writeAttribute( elements.second, newData, attribute );
					}

                    
                    // Write out the new tangent and bitangent.
					Vector4 normal = readAttribute( *normalElement, oldData );
					Vector4 tangent = tangentData[i];
					Vector3 bitangent = -Cross( Vector3( normal.x, normal.y, normal.z ), Vector3( tangent.x, tangent.y, tangent.z ) ) * tangentData[i].w;

                    writeAttribute( newTangentElement, newData, tangent );
					writeAttribute( newBitangentElement, newData, Vector4( bitangent, 0.0f ) );

                    // Write out an identity index buffer
                    reinterpret_cast<uint32_t*>( newIndexBuffer.data.data() )[i] = i;


				}

				// TODO: Need to handle morph target here too.

                lod.vertexBuffer = newVertexBuffer;
				lod.indexBuffer = newIndexBuffer;
			}


            //Re-index the data to avoid unnecessary work in the coming steps.

            {
				uint32_t* indexData = reinterpret_cast<uint32_t*>( lod.indexBuffer.data.data() );
				uint32_t indexCount = lod.indexBuffer.length();

				uint8_t* vertexData = lod.vertexBuffer.data.data();
				uint32_t vertexCount = lod.vertexBuffer.length();
				uint32_t vertexStride = lod.vertexBuffer.stride;

				std::vector<uint32_t> remap( vertexCount );
				uint32_t newVertexCount = (uint32_t)meshopt_generateVertexRemap( remap.data(), indexData, indexCount, vertexData, vertexCount, vertexStride );

				meshopt_remapVertexBuffer( vertexData, vertexData, vertexCount, vertexStride, remap.data() );
				meshopt_remapIndexBuffer( indexData, indexData, indexCount, remap.data() );

				lod.vertexBuffer.data.resize( newVertexCount * vertexStride );

				for( OptMorphTargetLod& morph : lod.morphTargetLods )
				{
					uint8_t* morphData = morph.vertexBuffer.data.data();
					uint32_t morphStride = morph.vertexBuffer.stride;
					meshopt_remapVertexBuffer( morphData, morphData, vertexCount, morphStride, remap.data() );

					morph.vertexBuffer.data.resize( newVertexCount * vertexStride );
				}

				vertexCount = newVertexCount;
			}


		}

        mesh.vertexDeclaration = newVertexDeclaration;
	}
}


Vector4 packTangents( Vector3 normal, Vector3 tangent, Vector3 bitangent )
{
    // Figure out if we need to flip the normal before compressing.
	bool flipNormal = Dot( normal, Cross( tangent, bitangent ) ) < 0.0f;
	if( flipNormal )
	{
		normal = -normal;
	}

    // Construct a TBN matrix.
	Matrix matrix(
		tangent.x, tangent.y, tangent.z, 0.0f, 
        bitangent.x, bitangent.y, bitangent.z, 0.0f, 
        normal.x, normal.y, normal.z, 0.0f, 
        0.0f, 0.0f, 0.0f, 1.0f 
    );

    // Convert matrix to quaternion.
	Quaternion q = Normalize( RotationQuaternion( matrix ) );

    // We need to be able to reconstruct the W-component during unpacking, so make sure that it's always positive.
	if( q.w < 0.0f )
	{
		q = -q; // Represents the same rotation.
	}

	return Vector4( q.x, q.y, q.z, flipNormal ? -1.0f : 1.0f ); // W stores the normal sign
}


void unpackTangents( Vector4 t, Vector3* normal, Vector3* tangent, Vector3* bitangent )
{

#if 0
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

#else

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

    *tangent =   Vector3( fma( -2.0f, y2, fma( -2.0f, z2, 1.0f ) ), + xy + zw, + xz - yw );
    *bitangent = Vector3( - zw + xy, fma( -2.0f, x2, fma( -2.0f, z2, 1.0f ) ), + yz + xw );
	*normal =    Vector3( + yw + xz, + yz - xw, fma( -2.0f, x2, fma( -2.0f, y2, 1.0f ) ) ) * t.w; // packed normal sign multiplication

#endif




}

void Optimizer::compressTangents( uint32_t usageIndex, bool retainNormal )
{

	for( OptMesh& mesh : meshes )
	{
		std::vector<VertexElement>& vertexDeclaration = mesh.vertexDeclaration;

        // Check if tangents are already packed.
		auto packedTangentElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::PackedTangent && v.usageIndex == usageIndex; } );
		if( packedTangentElement != vertexDeclaration.end() )
        {
			continue;
        }


		// Check if the mesh has the required data to pack tangents.
		auto normalElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), []( const auto& v ) { return v.usage == Usage::Normal && v.usageIndex == 0; } );
		auto tangentElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::Tangent && v.usageIndex == usageIndex; } );
		auto bitangentElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::Binormal && v.usageIndex == usageIndex; } );

        if( normalElement == vertexDeclaration.end() || tangentElement == vertexDeclaration.end() || bitangentElement == vertexDeclaration.end() )
		{
			printf( "Failed to pack tangents for mesh %s\n", mesh.name.c_str() );

			if( normalElement == vertexDeclaration.end() )
				printf( "    No Normal%d attribute found.\n", 0 );

			if( tangentElement == vertexDeclaration.end() )
				printf( "    No Tangent%d attribute found.\n", usageIndex );

			if( bitangentElement == vertexDeclaration.end() )
				printf( "    No Binormal%d attribute found.\n", usageIndex );

			continue;
		}


        // Create a new vertex declaration with the new tangents, and a mapping for copying to it.
		std::vector<VertexElement> newVertexDeclaration;
		std::vector<std::pair<VertexElement, VertexElement>> vertexElementMapping;

		VertexElement newPackedTangentElement;
		uint32_t newVertexStride;

		{
			uint32_t offset = 0;
			for( VertexElement element : mesh.vertexDeclaration )
			{
				if( ( element.usage == Usage::Tangent || element.usage == Usage::Binormal ) && element.usageIndex == usageIndex )
				{
					// Omit the unpacked tangents.
					continue;
				}

				if( element.usage == Usage::Normal && element.usageIndex == usageIndex )
				{
					// Insert the packed tangent
					newPackedTangentElement = { Usage::PackedTangent, (uint8_t)usageIndex, ElementType::Int16Norm, 4, offset };
					newVertexDeclaration.push_back( newPackedTangentElement );
					offset += getElementTypeByteSize( newPackedTangentElement.type ) * newPackedTangentElement.elementCount;
				}

                if( !retainNormal && element.usage == Usage::Normal && element.usageIndex == 0 )
				{
					// Omit the normal. //TODO: This should be a separate cleanup pass.
					continue;
				}

				VertexElement newElement = element;
				newElement.offset = offset;
				newVertexDeclaration.push_back( newElement );
				offset += getElementTypeByteSize( element.type ) * element.elementCount;

				vertexElementMapping.emplace_back( element, newElement );
			}

			newVertexStride = offset;
		}

		for( OptMeshLod& lod : mesh.lods )
		{

			uint8_t* vertexData = lod.vertexBuffer.data.data();
			uint32_t vertexCount = lod.vertexBuffer.length();
			uint32_t vertexStride = lod.vertexBuffer.stride;

			// Create a new vertex and index buffer that includes the new tangent and bitangent data.
			OptBufferData newVertexBuffer;
			newVertexBuffer.stride = newVertexStride;
			newVertexBuffer.data.resize( vertexCount * newVertexStride );

            
            double totalNormalError = 0.0;
			double totalTangentError = 0.0;
			double totalBitangentError = 0.0;

			for( uint32_t i = 0; i < vertexCount; i++ )
			{
				const uint8_t* oldData = vertexData + i * vertexStride;
				uint8_t* newData = newVertexBuffer.data.data() + i * newVertexBuffer.stride;

				// Move all unmodified attributes over.
				for( std::pair<VertexElement, VertexElement> elements : vertexElementMapping )
				{
					Vector4 attribute = readAttribute( elements.first, oldData );
					writeAttribute( elements.second, newData, attribute );
				}


				// Write out the new tangent and bitangent.
				Vector3 normal = readAttribute( *normalElement, oldData ).GetXYZ();
				Vector3 tangent = readAttribute( *tangentElement, oldData ).GetXYZ();
				Vector3 bitangent = readAttribute( *bitangentElement, oldData ).GetXYZ();

                Vector4 packedTangents = packTangents( normal, tangent, bitangent );
				
				writeAttribute( newPackedTangentElement, newData, packedTangents );


                if( true )
				{
                    packedTangents = readAttribute( newPackedTangentElement, newData );
					Vector3 normal2;
					Vector3 tangent2;
					Vector3 bitangent2;
					unpackTangents( packedTangents, &normal2, &tangent2, &bitangent2 );

					totalNormalError += acosf( std::clamp( Dot( normal, normal2 ), -1.0f, +1.0f ) );
					totalTangentError += acosf( std::clamp( Dot( tangent, tangent2 ), -1.0f, +1.0f ) );
					totalBitangentError += acosf( std::clamp( Dot( bitangent, bitangent2 ), -1.0f, +1.0f ) );

                    //printf( "Normal error: %f\n", acosf( std::clamp( Dot( normal, normal2 ), -1.0f, +1.0f) ) * 180 / 3.14159265359 );
					//printf( "Tangent error: %f\n", acosf( std::clamp( Dot( tangent, tangent2 ), -1.0f, +1.0f ) ) * 180 / 3.14159265359 );
					//printf( "Bitangent error: %f\n", acosf( std::clamp( Dot( bitangent, bitangent2 ), -1.0f, +1.0f ) ) * 180 / 3.14159265359 );
                }
			}

			printf( "Average normal error: %f degrees\n", totalNormalError / vertexCount * 180 / 3.14159265359 );
			printf( "Average tangent error: %f degrees\n", totalTangentError / vertexCount * 180 / 3.14159265359 );
			printf( "Average bitangent error: %f degrees\n", totalBitangentError / vertexCount * 180 / 3.14159265359 );



			// TODO: May need to compress morph target tangents here too.

			lod.vertexBuffer = newVertexBuffer;
		}

		mesh.vertexDeclaration = newVertexDeclaration;
	}
}

void Optimizer::decompressTangents( uint32_t usageIndex )
{

	for( OptMesh& mesh : meshes )
	{
		std::vector<VertexElement>& vertexDeclaration = mesh.vertexDeclaration;

		// Check if tangents are already packed.
		auto packedTangentElement = std::find_if( vertexDeclaration.begin(), vertexDeclaration.end(), [usageIndex]( const auto& v ) { return v.usage == Usage::PackedTangent && v.usageIndex == usageIndex; } );
		if( packedTangentElement == vertexDeclaration.end() )
		{
			printf( "Failed to unpack tangents for mesh %s\n", mesh.name.c_str() );
			printf( "    No PackedTangent%d attribute found.\n", 0 );
			continue;
		}


		// Create a new vertex declaration with the new tangents, and a mapping for copying to it.
		std::vector<VertexElement> newVertexDeclaration;
		std::vector<std::pair<VertexElement, VertexElement>> vertexElementMapping;

		VertexElement newNormalElement;
		VertexElement newTangentElement;
		VertexElement newBitangentElement;
		uint32_t newVertexStride;

		{
			uint32_t offset = 0;
			for( VertexElement element : mesh.vertexDeclaration )
			{

				if( element.usage == Usage::PackedTangent && element.usageIndex == usageIndex )
				{
					// Insert the unpacked tangents
					newNormalElement = { Usage::Normal, (uint8_t)0, ElementType::Float32, 3, offset };
					newVertexDeclaration.push_back( newNormalElement );
					offset += getElementTypeByteSize( newNormalElement.type ) * newNormalElement.elementCount;

					newTangentElement = { Usage::Tangent, (uint8_t)usageIndex, ElementType::Float32, 3, offset };
					newVertexDeclaration.push_back( newTangentElement );
					offset += getElementTypeByteSize( newTangentElement.type ) * newTangentElement.elementCount;

					newBitangentElement = { Usage::Binormal, (uint8_t)usageIndex, ElementType::Float32, 3, offset };
					newVertexDeclaration.push_back( newBitangentElement );
					offset += getElementTypeByteSize( newBitangentElement.type ) * newBitangentElement.elementCount;
				}


				if( ( element.usage == Usage::PackedTangent || element.usage == Usage::Tangent || element.usage == Usage::Binormal ) && element.usageIndex == usageIndex )
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
				newVertexDeclaration.push_back( newElement );
				offset += getElementTypeByteSize( element.type ) * element.elementCount;

				vertexElementMapping.emplace_back( element, newElement );
			}

			newVertexStride = offset;
		}

		for( OptMeshLod& lod : mesh.lods )
		{

			uint8_t* vertexData = lod.vertexBuffer.data.data();
			uint32_t vertexCount = lod.vertexBuffer.length();
			uint32_t vertexStride = lod.vertexBuffer.stride;

			// Create a new vertex and index buffer that includes the new tangent and bitangent data.
			OptBufferData newVertexBuffer;
			newVertexBuffer.stride = newVertexStride;
			newVertexBuffer.data.resize( vertexCount * newVertexStride );

			for( uint32_t i = 0; i < vertexCount; i++ )
			{
				const uint8_t* oldData = vertexData + i * vertexStride;
				uint8_t* newData = newVertexBuffer.data.data() + i * newVertexBuffer.stride;

				// Move all unmodified attributes over.
				for( std::pair<VertexElement, VertexElement> elements : vertexElementMapping )
				{
					Vector4 attribute = readAttribute( elements.first, oldData );
					writeAttribute( elements.second, newData, attribute );
				}


				// Write out the new tangent and bitangent.
				Vector4 packedTangents = readAttribute( *packedTangentElement, oldData );

				Vector3 normal;
                Vector3 tangent;
				Vector3 bitangent;
                unpackTangents( packedTangents, &normal, &tangent, &bitangent );
                
				writeAttribute( newTangentElement, newData, Vector4( tangent, 0.0f ) );
				writeAttribute( newBitangentElement, newData, Vector4( bitangent, 0.0f ) );
				writeAttribute( newNormalElement, newData, Vector4( normal, 0.0f ) );
			}

			// TODO: May need to compress morph target tangents here too.

			lod.vertexBuffer = newVertexBuffer;
		}

		mesh.vertexDeclaration = newVertexDeclaration;
	}
}

void Optimizer::optimizeVertexPerformance()
{
    for( OptMesh& mesh : meshes )
    {
		for( OptMeshLod& lod : mesh.lods )
		{
			uint32_t* indexData = reinterpret_cast<uint32_t*>( lod.indexBuffer.data.data() );
			uint32_t indexCount = lod.indexBuffer.length();

            uint8_t* vertexData = lod.vertexBuffer.data.data();
            uint32_t vertexCount = lod.vertexBuffer.length();
			uint32_t vertexStride = lod.vertexBuffer.stride;


            // Regenerate the index buffer.
            // - Some exporters may not be perfect.
            // - Some vertices may get merged after attribute compression.
            // - Important to do after generating tangents, as it outputs an unindexed mesh.
            {
				std::vector<uint32_t> remap( vertexCount );
				uint32_t newVertexCount = (uint32_t) meshopt_generateVertexRemap( remap.data(), indexData, indexCount, vertexData, vertexCount, vertexStride );

				meshopt_remapVertexBuffer( vertexData, vertexData, vertexCount, vertexStride, remap.data() );
				meshopt_remapIndexBuffer( indexData, indexData, indexCount, remap.data() );

                lod.vertexBuffer.data.resize( newVertexCount * vertexStride );

				for( OptMorphTargetLod& morph : lod.morphTargetLods )
				{
					uint8_t* morphData = morph.vertexBuffer.data.data();
					uint32_t morphStride = morph.vertexBuffer.stride;
					meshopt_remapVertexBuffer( morphData, morphData, vertexCount, morphStride, remap.data() );

                    morph.vertexBuffer.data.resize( newVertexCount * vertexStride );
				}

                vertexCount = newVertexCount;
            }
            

            // Optimize the index buffer for vertex cache coherency and overdraw. This can improve performance a lot.
            for( OptMeshAreaLod& area : lod.areas )
            {
				uint32_t* data = indexData + area.firstElement;
				meshopt_optimizeVertexCache( data, data, area.elementCount, vertexCount );
				meshopt_optimizeOverdraw( data, data, area.elementCount, reinterpret_cast<const float*>( vertexData ), vertexCount, vertexStride, 1.01f );
            }

            // Reorder all the vertices so that they are in the order that they appear in the optimized index buffer.
            // This improves cache coherency when reading vertex attributes, which is a minor but easy win.
            {
				std::vector<uint32_t> remap( vertexCount );
				meshopt_optimizeVertexFetchRemap( remap.data(), indexData, indexCount, vertexCount );

				meshopt_remapVertexBuffer( vertexData, vertexData, vertexCount, vertexStride, remap.data() );
				meshopt_remapIndexBuffer( indexData, indexData, indexCount, remap.data() );

				for( OptMorphTargetLod& morph : lod.morphTargetLods )
				{
					uint8_t* morphData = morph.vertexBuffer.data.data();
					uint32_t morphStride = morph.vertexBuffer.stride;
					meshopt_remapVertexBuffer( morphData, morphData, vertexCount, morphStride, remap.data() );
				}
			}
        }
    }
}

template <typename T>
Span<T> convertToSpan( MemoryAllocator& allocator, const std::vector<T>& elements )
{
	Span<T> span = allocator.AllocateSpan<T>( elements.size() );
	memcpy( span.data(), elements.data(), elements.size() * sizeof( T ) );
    return span;
}

std::vector<uint8_t> Optimizer::toCmf()
{
	MemoryAllocator allocator;
	BufferManager bufferAllocator(allocator);

	Data* data = content->m_cmfData;

	Data newData;

	newData.meshes = allocator.AllocateSpan<Mesh>( data->meshes.size() );

	for( uint32_t meshIndex = 0; meshIndex < content->m_cmfData->meshes.size(); meshIndex++ )
	{
		const Mesh& oldMesh = content->m_cmfData->meshes[meshIndex];

		const OptMesh& optMesh = meshes[meshIndex];

		Mesh& newMesh = newData.meshes[meshIndex];

		newMesh.name = oldMesh.name;
		newMesh.decl = convertToSpan( allocator, optMesh.vertexDeclaration );
		newMesh.areas = oldMesh.areas;
		newMesh.boneBindings = oldMesh.boneBindings;
		newMesh.uvDensities = oldMesh.uvDensities;
		newMesh.bounds = oldMesh.bounds;
		newMesh.topology = oldMesh.topology;
		newMesh.skeleton = oldMesh.skeleton;

		newMesh.morphTargets = allocator.AllocateSpan<MorphTarget>( optMesh.morphTargets.size() );
		for( uint32_t morphIndex = 0; morphIndex < optMesh.morphTargets.size(); morphIndex++ ) 
        {
			MorphTarget oldMorph = oldMesh.morphTargets[morphIndex];
			OptMorphTarget optMorph = optMesh.morphTargets[morphIndex];
			newMesh.morphTargets[morphIndex] = {
				oldMorph.name,
				convertToSpan( allocator, optMorph.vertexDeclaration ),
				oldMorph.bounds,
				oldMorph.maxDisplacements
            };
	    }


		newMesh.lods = allocator.AllocateSpan<MeshLod>( optMesh.lods.size() );
		for( uint32_t lodIndex = 0; lodIndex < optMesh.lods.size(); lodIndex++ )
		{
			const OptMeshLod& optLod = optMesh.lods[lodIndex];

			MeshLod& newLod = newMesh.lods[lodIndex];

            OptBufferData vertexBuffer = optLod.vertexBuffer;
			OptBufferData indexBuffer = optLod.indexBuffer;

            
			//meshopt_encodeVertexBufferBound()

			newLod.vb = bufferAllocator.AddBuffer( vertexBuffer.data.data(), (uint32_t) vertexBuffer.data.size(), vertexBuffer.stride );

            
            if( vertexBuffer.length() < (1u << 16) )
            {
				indexBuffer = convertIndexBuffer( indexBuffer, 2 );
            }
			newLod.ib = bufferAllocator.AddBuffer( indexBuffer.data.data(), (uint32_t) indexBuffer.data.size(), indexBuffer.stride );

            newLod.areas = allocator.AllocateSpan<LodMeshArea>( optLod.areas.size() );
			for( uint32_t areaIndex = 0; areaIndex < optLod.areas.size(); areaIndex++ )
            {
				OptMeshAreaLod optArea = optLod.areas[areaIndex];
				newLod.areas[areaIndex] = { optArea.firstElement / 3, optArea.elementCount / 3 };
            }

            newLod.morphTargets = allocator.AllocateSpan<LodMorphTarget>( optLod.morphTargetLods.size() );
			for( uint32_t morphIndex = 0; morphIndex < optLod.morphTargetLods.size(); morphIndex++ )
			{
				OptMorphTargetLod optMorphTarget = optLod.morphTargetLods[morphIndex];
				newLod.morphTargets[morphIndex] = { bufferAllocator.AddBuffer( optMorphTarget.vertexBuffer.data.data(), (uint32_t) optMorphTarget.vertexBuffer.data.size(), optMorphTarget.vertexBuffer.stride ) };
			}

			newLod.threshold = optLod.threshold;
		}
	}

	newData.animations = data->animations;
	newData.curves = data->curves;
	newData.skeletons = data->skeletons;

	return BuildFile( newData, bufferAllocator );
}
