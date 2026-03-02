#include "cmf/utils.h"
#include "cmf/declutils.h"
#include <numeric>


namespace
{
template <typename T>
bool AreSpanPointersValid( const T& value, const void* base, size_t totalSize )
{
	if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		if( value.size() == 0 )
		{
			return true;
		}
		// Total size must be multiple of element size
		if( value.byteSize % sizeof( typename T::value_type ) != 0 )
		{
			return false;
		}
		// Pointer must be within the base + totalSize range
		if( value.data() < base || reinterpret_cast<const uint8_t*>( value.end() ) > reinterpret_cast<const uint8_t*>( base ) + totalSize )
		{
			return false;
		}
		return std::all_of( value.begin(), value.end(), [base, totalSize]( const auto& element ) {
			return AreSpanPointersValid( element, base, totalSize );
		} );
	}
	else
	{
		bool valid = true;
		cmf::EnumerateMembers( const_cast<T&>( value ), [&valid, base, totalSize]( auto&&, auto& value, const char* ) {
			if( valid )
			{
				valid = AreSpanPointersValid( value, base, totalSize );
			}
		} );
		return valid;
	}
}

template <typename T>
bool AreBufferViewsValid( const T& value, const cmf::Span<cmf::Section>& sections )
{
	if constexpr( std::is_base_of_v<cmf::BufferView, T> )
	{
		// Empty buffer views are valid
		if( value.size == 0 )
		{
			return true;
		}
		// Buffer index must be > 0 (0 is reserved for "data" segment)
		if( value.index == 0 || value.index >= sections.size() )
		{
			return false;
		}
		if( sections[value.index - 1].type == cmf::SectionType::Metadata )
		{
			return false;
		}
		// Offset + size must be within totalSize
		if( value.offset + value.size > sections[value.index].uncompressedSize )
		{
			return false;
		}
		// If stride is non-zero, size must be multiple of stride
		if( value.stride != 0 && sections[value.index].gpuAlignment % value.stride != 0 )
		{
			return false;
		}
		return true;
	}
	else
	{
		bool valid = true;
		cmf::EnumerateChildren( const_cast<T&>( value ), [&valid, &sections]( auto&&, auto& value, const char* ) {
			if( valid )
			{
				valid = AreBufferViewsValid( value, sections );
			}
		} );
		return valid;
	}
}

bool IsHeaderSectionValid( const cmf::Section& section, const cmf::Header& header, uint32_t lastEnd, size_t fileSize )
{
	// Section must be within file bounds
	if( section.offset + section.compressedSize > fileSize )
	{
		return false;
	}
	// Sections must not overlap
	if( section.offset < lastEnd )
	{
		return false;
	}
	// The first section must be a data section
	if( &section == header.sections.begin() && section.type != cmf::SectionType::Data )
	{
		return false;
	}
	// If not compressed, uncompressedSize must match size
	if( section.compression == cmf::SectionCompression::None )
	{
		if( section.uncompressedSize != section.compressedSize )
		{
			return false;
		}
	}
	switch( section.type )
	{
	case cmf::SectionType::Data:
		// Only one data section allowed
		if( &section != header.sections.begin() )
		{
			return false;
		}
		// Data section must not be compressed
		if( section.compression != cmf::SectionCompression::None )
		{
			return false;
		}
		break;
	case cmf::SectionType::Metadata:
		// Metadata section must be last
		if( &section != header.sections.end() - 1 )
		{
			return false;
		}
		// Metadata section must not be compressed
		if( section.compression != cmf::SectionCompression::None )
		{
			return false;
		}
		break;
	default:
		break;
	}
	return true;
}

bool AreHeaderSectionsValid( const cmf::Header& header, size_t fileSize )
{
	if( header.sections.empty() )
	{
		return false;
	}

	uint32_t lastEnd = header.headerSize;
	for( auto& section : header.sections )
	{
		if( !IsHeaderSectionValid( section, header, lastEnd, fileSize ) )
		{
			return false;
		}
		lastEnd = section.offset + section.compressedSize;
	}
	return true;
}

bool IsVertexElementValid( const cmf::VertexElement& element, const cmf::Span<cmf::VertexElement>& decl )
{
	// Element count must be between 1 and 4
	if( element.elementCount == 0 || element.elementCount > 4 )
	{
		return false;
	}
	// Each (usage, usageIndex) pair must be unique within the decl
	for( auto other = &element + 1; other != decl.end(); ++other )
	{
		if( element.usage == other->usage && element.usageIndex == other->usageIndex )
		{
			return false;
		}
	}
	if( element.usage == cmf::Usage::PackedTangent )
	{
		// If the declaration contains a packed tangent, it must be the only tangent space element with the same usageIndex
		if( FindElement( decl, cmf::Usage::Normal, element.usageIndex ) || FindElement( decl, cmf::Usage::Tangent, element.usageIndex ) || FindElement( decl, cmf::Usage::Binormal, element.usageIndex ) )
		{
			return false;
		}
		// Packed tangent must be 4-component signed normalized integer
		if( ( element.type != cmf::ElementType::Int16Norm && element.type != cmf::ElementType::Int8Norm ) || element.elementCount != 4 )
		{
			return false;
		}
	}
	if( element.usage == cmf::Usage::PackedTangentLegacy )
	{
		// If the declaration contains a packed tangent, it must be the only tangent space element with the same usageIndex
		if( FindElement( decl, cmf::Usage::Normal, element.usageIndex ) || FindElement( decl, cmf::Usage::Tangent, element.usageIndex ) || FindElement( decl, cmf::Usage::Binormal, element.usageIndex ) )
		{
			return false;
		}
		// Packed tangent must be 4-component unsigned normalized integer
		if( ( element.type != cmf::ElementType::UInt16Norm && element.type != cmf::ElementType::UInt8Norm ) || element.elementCount != 4 )
		{
			return false;
		}
	}
	return true;
}

bool IsVertexDeclarationValid( const cmf::Span<cmf::VertexElement>& decl )
{
	if( decl.empty() )
	{
		return false;
	}
	// Declaration must contain a position element (usage == Position and usageIndex == 0)
	if( !FindElement( decl, cmf::Usage::Position ) )
	{
		return false;
	}
	return std::all_of( decl.begin(), decl.end(), [&decl]( const auto& element ) {
		return IsVertexElementValid( element, decl );
	} );
}

bool IsMeshLodValid( const cmf::Mesh& mesh, const cmf::MeshLod& lod )
{
	// LOD must have a vertex buffer
	if( lod.vb.size == 0 )
	{
		return false;
	}
	// If not a point list, LOD must have an index buffer
	if( mesh.topology != cmf::MeshTopology::PointList )
	{
		if( lod.ib.size == 0 )
		{
			return false;
		}
	}
	else
	{
		if( lod.ib.size != 0 )
		{
			return false;
		}
	}
	if( lod.ib.size > 0 )
	{
		// Mesh index buffer is either 2 or 4 bytes per index
		if( lod.ib.stride != 2 && lod.ib.stride != 4 )
		{
			return false;
		}
	}

	// Mesh areas lists must match
	if( lod.areas.size() != mesh.areas.size() )
	{
		return false;
	}
	for( auto& area : lod.areas )
	{
		// Area must be within the mesh index or vertex range
		uint32_t verticesPerElement = mesh.topology == cmf::MeshTopology::PointList ? 1 : 3;
		uint32_t vertexCount = mesh.topology == cmf::MeshTopology::PointList ? lod.vb.size / lod.vb.stride : lod.ib.size / lod.ib.stride;
		if( area.firstElement * verticesPerElement + area.elementCount * verticesPerElement > vertexCount )
		{
			return false;
		}
	}

	// Mesh morph target lists must match
	if( lod.morphTargets.size() != mesh.morphTargets.targets.size() )
	{
		return false;
	}

	return std::all_of( lod.morphTargets.begin(), lod.morphTargets.end(), [&lod]( const auto& morph ) {
		if( morph.vb.size == 0 )
		{
			return true;
		}
		// Morph target vertex buffer must have the same number of vertices as the LOD
		if( morph.vb.size / morph.vb.stride != lod.vb.size / lod.vb.stride )
		{
			return false;
		}
		return true;
	} );
}

bool MeshHasValidLodThresholds( const cmf::Mesh& mesh )
{
	// First LOD must have threshold of 0xffffffff
	if( mesh.lods[0].threshold != 0xffffffff )
	{
		return false;
	}
	// LOD thresholds must be in descending order
	for( size_t i = 1; i < mesh.lods.size(); ++i )
	{
		if( mesh.lods[i].threshold >= mesh.lods[i - 1].threshold )
		{
			return false;
		}
	}
	return true;
}

bool AreMorphTargetsValid( const cmf::Mesh& mesh )
{
	if( mesh.morphTargets.targets.empty() )
	{
		return true;
	}
	if( !IsVertexDeclarationValid( mesh.morphTargets.decl ) )
	{
		return false;
	}
	// Morph target decl must be a subset of the mesh decl
	for( auto& element : mesh.morphTargets.decl )
	{
		if( !FindElement( mesh.decl, element.usage, element.usageIndex ) )
		{
			return false;
		}
	}
	return std::all_of( mesh.morphTargets.targets.begin(), mesh.morphTargets.targets.end(), []( const auto& morph ) {
		return morph.maxDisplacement >= 0;
	} );
}


bool IsMeshValid( const cmf::Mesh& mesh, size_t skeletonCount )
{
	// Mesh must have at least one LOD
	if( mesh.lods.empty() )
	{
		return false;
	}
	if( !MeshHasValidLodThresholds( mesh ) )
	{
		return false;
	}
	for( auto& lod : mesh.lods )
	{
		if( !IsMeshLodValid( mesh, lod ) )
		{
			return false;
		}
	}

	for( auto& area : mesh.areas )
	{
		for( auto& bone : area.bones )
		{
			if( bone >= mesh.boneBindings.size() )
			{
				return false;
			}
		}
	}

	if( !IsVertexDeclarationValid( mesh.decl ) )
	{
		return false;
	}

	if( !AreMorphTargetsValid( mesh ) )
	{
		return false;
	}

	if( auto boneIndicesElement = FindElement( mesh.decl, cmf::Usage::BoneIndices ) )
	{
		if( boneIndicesElement->type == cmf::ElementType::UInt8 )
		{
			// Mesh can have up to 255 bone bindings
			if( mesh.boneBindings.size() > 255 )
			{
				return false;
			}
		}
	}

	size_t uvCount = std::accumulate( mesh.decl.begin(), mesh.decl.end(), size_t( 0 ), []( size_t count, const cmf::VertexElement& element ) {
		if( element.usage == cmf::Usage::TexCoord )
		{
			return std::max( count, size_t( element.usageIndex + 1 ) );
		}
		return count;
	} );
	if( mesh.uvDensities.size() != uvCount )
	{
		return false;
	}

	if( mesh.skeleton != 0xff )
	{
		if( mesh.skeleton >= skeletonCount )
		{
			return false;
		}
	}
	return true;
}

bool IsSkeletonValid( const cmf::Skeleton& skeleton )
{
	// Skeleton must have at least one bone
	if( skeleton.bones.empty() )
	{
		return false;
	}
	// All skeleton arrays must have the same size
	if( skeleton.bones.size() != skeleton.parents.size() || skeleton.bones.size() != skeleton.restTransforms.size() || skeleton.bones.size() != skeleton.invBindTransforms.size() )
	{
		return false;
	}
	for( auto& parent : skeleton.parents )
	{
		// Parent index must be either -1 (0xffffffff) or a valid bone index
		if( parent != 0xffffffff && parent >= skeleton.bones.size() )
		{
			return false;
		}
		// Parent index must be less than the bone index (no cycles)
		auto idx = &parent - skeleton.parents.data();
		if( parent != 0xffffffff && parent >= idx )
		{
			return false;
		}
	}
	return true;
}

bool IsAnimationValid( const cmf::Animation& animation, size_t curveCount )
{
	// Animation duration must be > 0
	if( animation.duration <= 0 )
	{
		return false;
	}
	if( animation.channels.empty() )
	{
		return false;
	}
	return std::all_of( animation.channels.begin(), animation.channels.end(), [curveCount]( const auto& channel ) {
		return channel.curveIndex < curveCount;
	} );
}

bool IsMainDataValid( const cmf::Data& mainData, const cmf::Header& header )
{
	if( !AreSpanPointersValid( mainData, &mainData, header.sections[0].uncompressedSize ) )
	{
		return false;
	}
	if( !AreBufferViewsValid( mainData, header.sections ) )
	{
		return false;
	}

	for( auto& mesh : mainData.meshes )
	{
		if( !IsMeshValid( mesh, mainData.skeletons.size() ) )
		{
			return false;
		}
	}

	for( auto& skeleton : mainData.skeletons )
	{
		if( !IsSkeletonValid( skeleton ) )
		{
			return false;
		}
	}

	return std::all_of( mainData.animations.begin(), mainData.animations.end(), [&mainData]( const auto& animation ) {
		return IsAnimationValid( animation, mainData.curves.size() );
	} );
}

}

namespace cmf
{


ValidationResult ValidateFile( const void* data, size_t size, const ValidationOptions& options )
{
	ValidationOptions result = {};

	auto& header = *static_cast<const Header*>( data );
	if( size < sizeof( Header ) )
	{
		return { false, {} };
	}
	if( header.signature != FILE_SIGNATURE )
	{
		return { false, {} };
	}
	if( header.version != FILE_VERSION )
	{
		return { false, {} };
	}
	if( header.headerSize < sizeof( Header ) || header.headerSize > size )
	{
		return { false, {} };
	}
	if( options.validateCrc )
	{
		auto crcOffset = offsetof( Header, crc32 );
		auto crc = ComputeCrc32( static_cast<const uint8_t*>( data ) + crcOffset + sizeof( Header::crc32 ), size - ( crcOffset + sizeof( Header::crc32 ) ) );
		if( crc != header.crc32 )
		{
			return { false, result };
		}
		result.validateCrc = true;
	}

	if( !options.validateHeader && !options.validateMainData )
	{
		return { true, result };
	}

	if( !AreSpanPointersValid( header, &header, header.headerSize ) )
	{
		return { false, result };
	}
	if( !AreHeaderSectionsValid( header, size ) )
	{
		return { false, result };
	}
	result.validateHeader = true;

	if( !options.validateMainData )
	{
		return { true, result };
	}

	if( options.validateMainData )
	{
		auto& mainData = *reinterpret_cast<const Data*>( static_cast<const uint8_t*>( data ) + header.sections[0].offset );
		if( !IsMainDataValid( mainData, header ) )
		{
			return { false, result };
		}
		result.validateMainData = true;
	}
	return { true, result };
}

uint32_t ComputeCrc32( const void* data, size_t size )
{
	const uint8_t* bytes = reinterpret_cast<const uint8_t*>( data );
	uint32_t crc = 0xFFFFFFFF;
	for( size_t i = 0; i < size; ++i )
	{
		crc ^= bytes[i];
		for( int j = 0; j < 8; ++j )
		{
			if( crc & 1 )
			{
				crc = ( crc >> 1 ) ^ 0xEDB88320;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return ~crc;
}
}