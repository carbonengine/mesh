#include <attributes.h>
#include <meshoptimizer.h>


using namespace cmf;
using namespace cmf::v1;

const char* getUsageString( Usage usage )
{
	switch( usage )
	{
	case Usage::Position:
		return "Position";

	case Usage::Normal:
		return "Normal";

	case Usage::Tangent:
		return "Tangent";

	case Usage::Binormal:
		return "Binormal";

	case Usage::TexCoord:
		return "TexCoord";

	case Usage::Color:
		return "Color";

	case Usage::BoneIndices:
		return "BoneIndices";

	case Usage::BoneWeights:
		return "BoneWeights";

	case Usage::PackedTangent:
		return "PackedTangent";

	default:
		printf( "Unknown usage: %hhu", usage );
		return "Unknown usage!";
	}
}

const char* getElementTypeString( ElementType type )
{
	switch( type )
	{
	case ElementType::Float32:
		return "Float32";

	case ElementType::Float16:
		return "Float16";

	case ElementType::UInt16Norm:
		return "UInt16Norm";

	case ElementType::UInt16:
		return "UInt16";

	case ElementType::Int16Norm:
		return "Int16Norm";

	case ElementType::Int16:
		return "Int16";

	case ElementType::UInt8Norm:
		return "UInt8Norm";

	case ElementType::UInt8:
		return "UInt8";

	case ElementType::Int8Norm:
		return "Int8Norm";

	case ElementType::Int8:
		return "Int8";

	default:
		printf( "Unknown element type: %hhu", type );
		return "Unknown element type!";
	}
}

int getElementTypeByteSize( ElementType type )
{
	switch( type )
	{
	case ElementType::Float32:
		return 4;

	case ElementType::Float16:
	case ElementType::UInt16Norm:
	case ElementType::UInt16:
	case ElementType::Int16Norm:
	case ElementType::Int16:
		return 2;

	case ElementType::UInt8Norm:
	case ElementType::UInt8:
	case ElementType::Int8Norm:
	case ElementType::Int8:
		return 1;

	default:
		printf( "Unknown element type: %d", type );
		return -1;
	}
}


float readValue( ElementType type, const uint8_t* data )
{
	switch( type )
	{
	case ElementType::Float32:
		return *reinterpret_cast<const float*>( data );

	case ElementType::Float16:
		return meshopt_dequantizeHalf( *reinterpret_cast<const uint16_t*>( data ) );


	case ElementType::UInt16Norm:
		return *reinterpret_cast<const uint16_t*>( data ) * ( 1.0f / 65535.0f );

	case ElementType::UInt16:
		return *reinterpret_cast<const uint16_t*>( data );

	case ElementType::Int16Norm:
		return std::max( *reinterpret_cast<const int16_t*>( data ) * ( 1.0f / 32767.0f ), -1.0f );

	case ElementType::Int16:
		return *reinterpret_cast<const int16_t*>( data );



	case ElementType::UInt8Norm:
		return *reinterpret_cast<const uint8_t*>( data ) * ( 1.0f / 255.0f );

	case ElementType::UInt8:
		return *reinterpret_cast<const uint8_t*>( data );

	case ElementType::Int8Norm:
		return std::max( *reinterpret_cast<const int8_t*>( data ) * ( 1.0f / 127.0f ), -1.0f );

	case ElementType::Int8:
		return *reinterpret_cast<const int8_t*>( data );

	default:
		printf( "Unknown element type: %d", type );
		return 0.0f;
	}
}

Vector4 readAttribute( const VertexElement& attribute, const uint8_t* vertexData )
{
	Vector4 result;

	int byteStride = getElementTypeByteSize( attribute.type );

	for( int i = 0; i < attribute.elementCount; i++ )
	{
		result[i] = readValue( attribute.type, vertexData + attribute.offset );
		vertexData += byteStride;
	}
	return result;
}


void writeValue( ElementType type, uint8_t* data, float value )
{
	switch( type )
	{
	case ElementType::Float32:
		*reinterpret_cast<float*>( data ) = value;
		break;

	case ElementType::Float16:
		*reinterpret_cast<uint16_t*>( data ) = meshopt_quantizeHalf( value );
		break;


	case ElementType::UInt16Norm:
		*reinterpret_cast<uint16_t*>( data ) = (uint16_t)std::roundf( std::clamp( value, 0.0f, 1.0f ) * 65535.0f );
		break;

	case ElementType::UInt16:
		*reinterpret_cast<uint16_t*>( data ) = (uint16_t)std::clamp( std::roundf( value ), 0.0f, 65535.0f );
		break;

	case ElementType::Int16Norm:
		*reinterpret_cast<int16_t*>( data ) = (int16_t)std::roundf( std::clamp( value, -1.0f, 1.0f ) * 32767.0f );
		break;

	case ElementType::Int16:
		*reinterpret_cast<int16_t*>( data ) = (int16_t)std::clamp( std::roundf( value ), -32767.0f, +32767.0f );
		break;


	case ElementType::UInt8Norm:
		*reinterpret_cast<uint8_t*>( data ) = (uint8_t)std::roundf( std::clamp( value, 0.0f, 1.0f ) * 255.0f );
		break;

	case ElementType::UInt8:
		*reinterpret_cast<uint8_t*>( data ) = (uint8_t)std::clamp( std::roundf( value ), 0.0f, 255.0f );
		break;

	case ElementType::Int8Norm:
		*reinterpret_cast<int8_t*>( data ) = (int8_t)std::roundf( std::clamp( value, -1.0f, 1.0f ) * 127.0f );
		break;

	case ElementType::Int8:
		*reinterpret_cast<int8_t*>( data ) = (int8_t)std::clamp( std::roundf( value ), -127.0f, +127.0f );
		break;



	default:
		printf( "Unknown element type: %d", type );
	}
}

void writeAttribute( const VertexElement& attribute, uint8_t* vertexData, Vector4 data )
{
	int byteStride = getElementTypeByteSize( attribute.type );

	for( int i = 0; i < attribute.elementCount; i++ )
	{
		writeValue( attribute.type, vertexData + attribute.offset, data[i] );
		vertexData += byteStride;
	}
}
