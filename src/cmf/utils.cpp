#include "cmf/utils.h"
#include "cmf/declutils.h"
#include "cmf/bufferstreams.h"
#include <numeric>


namespace
{
template <typename T>
bool AreSpanPointersValid( const T& value, const void* base, size_t totalSize )
{
	if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		// Total size must be multiple of element size
		if( value.byteSize % sizeof( typename T::value_type ) != 0 )
		{
			return false;
		}
		if( value.size() == 0 )
		{
			return true;
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

std::string IsHeaderSectionValid( const cmf::Section& section, const cmf::Header& header, uint32_t lastEnd, size_t fileSize )
{
	// Section must be within file bounds
	if( section.offset + section.compressedSize > fileSize )
	{
		return "Section exceeds file bounds (offset + compressedSize > fileSize)";
	}
	// Sections must not overlap
	if( section.offset < lastEnd )
	{
		return "Section overlaps with a previous section";
	}
	// The first section must be a data section
	if( &section == header.sections.begin() && section.type != cmf::SectionType::Data )
	{
		return "First section must be a data section";
	}
	// If not compressed, uncompressedSize must match size
	if( section.compression == cmf::SectionCompression::None )
	{
		if( section.uncompressedSize != section.compressedSize )
		{
			return "Uncompressed section has mismatched uncompressedSize and compressedSize";
		}
	}
	switch( section.type )
	{
	case cmf::SectionType::Data:
		// Only one data section allowed
		if( &section != header.sections.begin() )
		{
			return "Multiple data sections found; only one is allowed";
		}
		// Data section must not be compressed
		if( section.compression != cmf::SectionCompression::None )
		{
			return "Data section must not be compressed";
		}
		break;
	case cmf::SectionType::Metadata:
		// Metadata section must be last
		if( &section != header.sections.end() - 1 )
		{
			return "Metadata section must be the last section";
		}
		// Metadata section must not be compressed
		if( section.compression != cmf::SectionCompression::None )
		{
			return "Metadata section must not be compressed";
		}
		break;
	default:
		break;
	}
	return {};
}

std::string AreHeaderSectionsValid( const cmf::Header& header, size_t fileSize )
{
	if( header.sections.empty() )
	{
		return "Header contains no sections";
	}

	uint32_t lastEnd = header.headerSize;
	for( const auto& section : header.sections )
	{
		auto error = IsHeaderSectionValid( section, header, lastEnd, fileSize );
		if( !error.empty() )
		{
			return error;
		}
		lastEnd = section.offset + section.compressedSize;
	}
	return {};
}

bool IsVertexElementValid( const cmf::VertexElement& element, const cmf::Span<cmf::VertexElement>& decl )
{
	// Element count must be between 1 and 4
	if( element.elementCount == 0 || element.elementCount > 4 )
	{
		return false;
	}
	// Each (usage, usageIndex) pair must be unique within the decl
	for( const auto* other = &element + 1; other != decl.end(); ++other )
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

std::string IsMeshLodValid( const cmf::Mesh& mesh, const cmf::MeshLod& lod, size_t lodIndex )
{
	// LOD must have a vertex buffer
	if( lod.vb.size == 0 )
	{
		return "LOD " + std::to_string( lodIndex ) + " has no vertex buffer";
	}
	// If not a point list, LOD must have an index buffer
	if( mesh.topology != cmf::MeshTopology::PointList )
	{
		if( lod.ib.size == 0 )
		{
			return "LOD " + std::to_string( lodIndex ) + " has no index buffer";
		}
	}
	else
	{
		if( lod.ib.size != 0 )
		{
			return "LOD " + std::to_string( lodIndex ) + " has an index buffer but topology is PointList";
		}
	}
	if( lod.ib.size > 0 )
	{
		// Mesh index buffer is either 2 or 4 bytes per index
		if( lod.ib.stride != 2 && lod.ib.stride != 4 )
		{
			return "LOD " + std::to_string( lodIndex ) + " index buffer stride must be 2 or 4";
		}
	}

	// Mesh areas lists must match
	if( lod.areas.size() != mesh.areas.size() )
	{
		return "LOD " + std::to_string( lodIndex ) + " area count does not match mesh area count";
	}
	for( size_t i = 0; i < lod.areas.size(); ++i )
	{
		const auto& area = lod.areas[i];
		// Area must be within the mesh index or vertex range
		const uint32_t verticesPerElement = mesh.topology == cmf::MeshTopology::PointList ? 1 : 3;
		const uint32_t vertexCount = mesh.topology == cmf::MeshTopology::PointList ? lod.vb.size / lod.vb.stride : lod.ib.size / lod.ib.stride;
		if( area.firstElement * verticesPerElement + area.elementCount * verticesPerElement > vertexCount )
		{
			return "LOD " + std::to_string( lodIndex ) + " area " + std::to_string( i ) + " exceeds vertex/index range";
		}
	}

	// Mesh morph target lists must match
	if( lod.morphTargets.size() != mesh.morphTargets.targets.size() )
	{
		return "LOD " + std::to_string( lodIndex ) + " morph target count does not match mesh morph target count";
	}

	for( size_t i = 0; i < lod.morphTargets.size(); ++i )
	{
		const auto& morph = lod.morphTargets[i];
		if( morph.vb.size == 0 )
		{
			continue;
		}
		// Morph target vertex buffer must have the same number of vertices as the LOD
		if( morph.vb.size / morph.vb.stride != lod.vb.size / lod.vb.stride )
		{
			return "LOD " + std::to_string( lodIndex ) + " morph target " + std::to_string( i ) + " vertex count does not match LOD vertex count";
		}
	}
	return {};
}

std::string MeshHasValidLodThresholds( const cmf::Mesh& mesh )
{
	// First LOD must have threshold of 0xffffffff
	if( mesh.lods[0].threshold != 0xffffffff )
	{
		return "First LOD threshold must be 0xffffffff";
	}
	// LOD thresholds must be in descending order
	for( size_t i = 1; i < mesh.lods.size(); ++i )
	{
		if( mesh.lods[i].threshold >= mesh.lods[i - 1].threshold )
		{
			return "LOD thresholds must be in strictly descending order (LOD " + std::to_string( i ) + ")";
		}
	}
	return {};
}

std::string AreMorphTargetsValid( const cmf::Mesh& mesh )
{
	if( mesh.morphTargets.targets.empty() )
	{
		return {};
	}
	if( !IsVertexDeclarationValid( mesh.morphTargets.decl ) )
	{
		return "Morph target vertex declaration is invalid";
	}
	// Morph target decl must be a subset of the mesh decl
	for( const auto& element : mesh.morphTargets.decl )
	{
		if( !FindElement( mesh.decl, element.usage, element.usageIndex ) )
		{
			return "Morph target declaration element not found in mesh declaration";
		}
	}
	for( size_t i = 0; i < mesh.morphTargets.targets.size(); ++i )
	{
		if( mesh.morphTargets.targets[i].maxDisplacement < 0 )
		{
			return "Morph target " + std::to_string( i ) + " has negative maxDisplacement";
		}
	}
	return {};
}


std::string IsMeshValid( const cmf::Mesh& mesh, size_t skeletonCount )
{
	// Mesh must have at least one LOD
	if( mesh.lods.empty() )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has no LODs";
	}
	{
		auto error = MeshHasValidLodThresholds( mesh );
		if( !error.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\": " + error;
		}
	}
	for( size_t i = 0; i < mesh.lods.size(); ++i )
	{
		auto error = IsMeshLodValid( mesh, mesh.lods[i], i );
		if( !error.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\": " + error;
		}
	}

	for( size_t areaIdx = 0; areaIdx < mesh.areas.size(); ++areaIdx )
	{
		for( const auto& bone : mesh.areas[areaIdx].bones )
		{
			if( bone >= mesh.boneBindings.size() )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" area " + std::to_string( areaIdx ) + " references out-of-range bone binding";
			}
		}
	}

	if( !IsVertexDeclarationValid( mesh.decl ) )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has an invalid vertex declaration";
	}

	{
		auto error = AreMorphTargetsValid( mesh );
		if( !error.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\": " + error;
		}
	}

	if( const auto* boneIndicesElement = FindElement( mesh.decl, cmf::Usage::BoneIndices ) )
	{
		if( boneIndicesElement->type == cmf::ElementType::UInt8 )
		{
			// Mesh can have up to 255 bone bindings
			if( mesh.boneBindings.size() > std::numeric_limits<uint8_t>::max() )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has more than 255 bone bindings with UInt8 bone indices";
			}
		}
	}

	const size_t uvCount = std::accumulate( mesh.decl.begin(), mesh.decl.end(), size_t( 0 ), []( size_t count, const cmf::VertexElement& element ) {
		if( element.usage == cmf::Usage::TexCoord )
		{
			return std::max( count, size_t( element.usageIndex + 1 ) );
		}
		return count;
	} );
	if( mesh.uvDensities.size() != uvCount )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" uvDensities count does not match UV channel count";
	}

	if( mesh.skeleton != 0xff )
	{
		if( mesh.skeleton >= skeletonCount )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" references out-of-range skeleton index";
		}
	}
	return {};
}

std::string IsSkeletonValid( const cmf::Skeleton& skeleton )
{
	// Skeleton must have at least one bone
	if( skeleton.bones.empty() )
	{
		return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has no bones";
	}
	// All skeleton arrays must have the same size
	if( skeleton.bones.size() != skeleton.parents.size() || skeleton.bones.size() != skeleton.restTransforms.size() || skeleton.bones.size() != skeleton.invBindTransforms.size() )
	{
		return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has mismatched array sizes";
	}
	for( const auto& parent : skeleton.parents )
	{
		auto idx = &parent - skeleton.parents.data();
		// Parent index must be either -1 (0xffffffff) or a valid bone index
		if( parent != 0xffffffff && parent >= skeleton.bones.size() )
		{
			return "Skeleton \"" + ToStdString( skeleton.name ) + "\" bone " + std::to_string( idx ) + " has out-of-range parent index";
		}
		// Parent index must be less than the bone index (no cycles)
		if( parent != 0xffffffff && parent >= static_cast<uint32_t>( idx ) )
		{
			return "Skeleton \"" + ToStdString( skeleton.name ) + "\" bone " + std::to_string( idx ) + " has parent index >= own index (would form a cycle)";
		}
	}
	return {};
}

std::string IsCurveValid( const cmf::AnimationCurve& curve )
{
	if( curve.knotCount == 0 )
	{
		return "Curve has no keyframes";
	}
	// Knots must be in ascending order
	cmf::VertexElement element = {};
	element.type = curve.knotType;
	element.elementCount = 1;
	const auto stride = cmf::GetVertexElementSize( element );
	if( curve.knots.size() != curve.knotCount * stride )
	{
		return "Curve keyframe buffer size does not match keyframes count and time type";
	}
	const cmf::ConstBufferElementStream<float> knots{ element, curve.knots.data(), uint32_t( curve.knots.size() / stride ), stride };
	for( uint32_t i = 1; i < knots.size(); ++i )
	{
		if( knots[i] < knots[i - 1] )
		{
			return "Curve keyframes are not in ascending order";
		}
	}

	if( curve.values.size() != curve.knotCount * curve.valueDimension * cmf::GetElementTypeSize( curve.valueType ) )
	{
		return "Curve value buffer size does not match keyframes count, value dimension and value type";
	}
	return {};
}

std::string IsAnimationValid( const cmf::Animation& animation )
{
	// Animation duration must be > 0
	if( animation.duration <= 0 )
	{
		return "Animation \"" + ToStdString( animation.name ) + "\" has non-positive duration";
	}
	if( animation.channels.empty() )
	{
		return "Animation \"" + ToStdString( animation.name ) + "\" has no channels";
	}
	for( size_t i = 0; i < animation.channels.size(); ++i )
	{
		if( animation.channels[i].curveIndex >= animation.curves.size() )
		{
			return "Animation \"" + ToStdString( animation.name ) + "\" channel " + std::to_string( i ) + " references out-of-range curve index";
		}

		switch( animation.channels[i].targetType )
		{
		case cmf::AnimationChannelTargetType::BonePosition:
		case cmf::AnimationChannelTargetType::BoneScale:
			if( animation.curves[animation.channels[i].curveIndex].valueDimension != 3 )
			{
				return "Animation \"" + ToStdString( animation.name ) + "\" channel " + std::to_string( i ) + " targets BonePosition/BoneScale but curve value dimension is not 3";
			}
			break;
		case cmf::AnimationChannelTargetType::BoneRotation:
			if( animation.curves[animation.channels[i].curveIndex].valueDimension != 4 )
			{
				return "Animation \"" + ToStdString( animation.name ) + "\" channel " + std::to_string( i ) + " targets BoneRotation but curve value dimension is not 4";
			}
			break;
		case cmf::AnimationChannelTargetType::MorphTarget:
			if( animation.curves[animation.channels[i].curveIndex].valueDimension != 1 )
			{
				return "Animation \"" + ToStdString( animation.name ) + "\" channel " + std::to_string( i ) + " targets MorphTarget but curve value dimension is not 1";
			}
			break;
		default:
			break;
		}
	}
	for( size_t i = 0; i < animation.curves.size(); ++i )
	{
		auto error = IsCurveValid( animation.curves[i] );
		if( !error.empty() )
		{
			return "Animation \"" + ToStdString( animation.name ) + "\" curve " + std::to_string( i ) + ": " + error;
		}
	}
	return {};
}

std::string IsMainDataValid( const cmf::Data& mainData, const cmf::Header& header )
{
	if( !AreSpanPointersValid( mainData, &mainData, header.sections[0].uncompressedSize ) )
	{
		return "Main data contains invalid span pointers";
	}
	if( !AreBufferViewsValid( mainData, header.sections ) )
	{
		return "Main data contains invalid buffer views";
	}

	for( size_t i = 0; i < mainData.meshes.size(); ++i )
	{
		auto error = IsMeshValid( mainData.meshes[i], mainData.skeletons.size() );
		if( !error.empty() )
		{
			return error;
		}
	}

	for( const auto& skeleton : mainData.skeletons )
	{
		auto error = IsSkeletonValid( skeleton );
		if( !error.empty() )
		{
			return error;
		}
	}

	for( const auto& animation : mainData.animations )
	{
		auto error = IsAnimationValid( animation );
		if( !error.empty() )
		{
			return error;
		}
	}
	return {};
}

}

namespace cmf
{


ValidationResult ValidateFile( const void* data, size_t size, const ValidationOptions& options )
{
	const auto& header = *static_cast<const Header*>( data );
	if( size < sizeof( Header ) )
	{
		return { false, "File is too small to contain a valid header" };
	}
	if( header.signature != FILE_SIGNATURE )
	{
		return { false, "Invalid file signature" };
	}
	if( header.version != FILE_VERSION )
	{
		return { false, "Unsupported file version" };
	}
	if( header.headerSize < sizeof( Header ) || header.headerSize > size )
	{
		return { false, "Invalid header size" };
	}
	if( options.validateCrc )
	{
		auto crcOffset = offsetof( Header, crc32 );
		auto crc = ComputeCrc32( static_cast<const uint8_t*>( data ) + crcOffset + sizeof( Header::crc32 ), size - ( crcOffset + sizeof( Header::crc32 ) ) );
		if( crc != header.crc32 )
		{
			return { false, "CRC32 checksum mismatch" };
		}
	}

	if( !options.validateHeader && !options.validateMainData )
	{
		return { true, {} };
	}

	if( !AreSpanPointersValid( header, &header, header.headerSize ) )
	{
		return { false, "Header contains invalid span pointers" };
	}
	{
		auto error = AreHeaderSectionsValid( header, size );
		if( !error.empty() )
		{
			return { false, error };
		}
	}

	if( !options.validateMainData )
	{
		return { true, {} };
	}

	const auto& mainData = *reinterpret_cast<const Data*>( static_cast<const uint8_t*>( data ) + header.sections[0].offset );
	auto error = IsMainDataValid( mainData, header );
	if( !error.empty() )
	{
		return { false, error };
	}
	return { true, {} };
}

uint32_t ComputeCrc32( const void* data, size_t size )
{
	const auto* bytes = static_cast<const uint8_t*>( data );
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