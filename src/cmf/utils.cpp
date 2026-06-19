// Copyright © 2025 CCP ehf.

#include "cmf/utils.h"
#include "cmf/declutils.h"
#include "cmf/bufferstreams.h"
#include "cmf/compression.h"
#include <map>
#include <numeric>
#include <vector>
#include <cmath>

namespace
{
template <typename T>
std::string AreSpanPointersValid( const T& value, const void* base, size_t totalSize )
{
	if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		// Total size must be multiple of element size
		if( value.byteSize % sizeof( typename T::value_type ) != 0 )
		{
			return "Total size must be multiple of element size";
		}
		// Empty spans are valid
		if( value.size() == 0 )
		{
			return "";
		}
		// Pointer must be aligned to element size
		if( reinterpret_cast<uintptr_t>( value.data() ) % alignof( typename T::value_type ) != 0 )
		{
			return "Pointer must be aligned to element size";
		}

		// Pointer must be within the base + totalSize range
		if( value.data() < base || reinterpret_cast<const uint8_t*>( value.end() ) > reinterpret_cast<const uint8_t*>( base ) + totalSize )
		{
			return "Pointer must be within the base + totalSize range";
		}

		for( size_t i = 0; i < value.size(); i++ )
		{
			auto result = AreSpanPointersValid( value[i], base, totalSize );
			if( !result.empty() )
			{
				return "[" + std::to_string( i ) + "] " + result;
			}
		}
		return "";
	}
	else
	{
		std::string errorMessage;
		cmf::EnumerateMembers( const_cast<T&>( value ), [&errorMessage, base, totalSize]( auto&&, auto& value, const char* memberName ) {
			if( errorMessage.empty() )
			{
				auto result = AreSpanPointersValid( value, base, totalSize );
				if( !result.empty() )
				{
					errorMessage = std::string( memberName ) + " " + result;
				}
			}
		} );
		return errorMessage;
	}
}

template <typename T>
std::string AreBufferViewsValid( const T& value, const cmf::Span<cmf::Section>& sections )
{
	if constexpr( std::is_base_of_v<cmf::BufferView, T> )
	{
		// Empty buffer views are valid
		if( value.size == 0 )
		{
			return "";
		}
		// Buffer index must be > 0 (0 is reserved for "data" segment)
		if( value.index == 0 || value.index >= sections.size() )
		{
			return "Buffer index must be > 0 (0 is reserved for 'data' segment)";
		}
		if( sections[value.index].type == cmf::SectionType::Metadata )
		{
			return "SectionType must not be Metadata";
		}
		// Offset + size must be within totalSize
		if( uint64_t( value.offset ) + value.size > sections[value.index].uncompressedSize )
		{
			return "Offset + size must be within totalSize";
		}
		// If stride is non-zero, size must be multiple of stride
		if( value.stride != 0 && value.size % value.stride != 0 )
		{
			return "If stride is non-zero, size must be multiple of stride";
		}
		// If stride is non-zero, offset must be multiple of stride
		if( value.stride != 0 && value.offset % value.stride != 0 )
		{
			return "If stride is non-zero, offset must be multiple of stride";
		}
		// Stride must match gpuAlignment
		if( sections[value.index].gpuAlignment != 0 && value.stride != sections[value.index].gpuAlignment )
		{
			return "Stride must match gpuAlignment";
		}
		return "";
	}
	else
	{
		std::string errorMessage;
		cmf::EnumerateChildren( const_cast<T&>( value ), [&errorMessage, &sections]( auto&&, auto& value, const char* memberName ) {
			if( errorMessage.empty() )
			{
				auto result = AreBufferViewsValid( value, sections );
				if( !result.empty() )
				{
					errorMessage = std::string( memberName ) + " " + result;
				}
			}
		} );
		return errorMessage;
	}
}

std::string IsHeaderSectionValid( const cmf::Section& section, const cmf::Header& header, uint32_t lastEnd, size_t fileSize )
{
	// Section must be within file bounds
	if( uint64_t( section.offset ) + section.compressedSize > fileSize )
	{
		return "Section exceeds file bounds (offset + compressedSize > fileSize)";
	}
	// Sections must not overlap
	if( section.offset < lastEnd )
	{
		return "Section overlaps with a previous section";
	}

	switch( section.type )
	{
	case cmf::SectionType::Data:
	case cmf::SectionType::GpuBuffer:
	case cmf::SectionType::Metadata:
		break;
	default:
		return "Invalid section type";
	}

	switch( section.compression )
	{
	case cmf::SectionCompression::MeshOptimizerIndexBuffer:
	case cmf::SectionCompression::MeshOptimizerVertexBuffer:
	case cmf::SectionCompression::None:
		break;
	default:
		return "Invalid compression type";
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
	// uncompressedSize must be a multiple of gpuAlignment
	if( section.gpuAlignment != 0 && section.uncompressedSize % section.gpuAlignment != 0 )
	{
		return "Section uncompressedSize must be a multiple of gpuAlignment";
	}

	if( section.compression != cmf::SectionCompression::None && section.gpuAlignment == 0 )
	{
		return "Compressed section must have a non-zero gpuAlignment";
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

bool IsValidElementType( cmf::ElementType type )
{
	switch( type )
	{
	case cmf::ElementType::Float32:
	case cmf::ElementType::Float16:
	case cmf::ElementType::UInt16Norm:
	case cmf::ElementType::UInt16:
	case cmf::ElementType::Int16Norm:
	case cmf::ElementType::Int16:
	case cmf::ElementType::UInt8Norm:
	case cmf::ElementType::UInt8:
	case cmf::ElementType::Int8Norm:
	case cmf::ElementType::Int8:
		return true;
	default:
		return false;
	}
}

bool IsVertexElementValid( const cmf::VertexElement& element, const cmf::Span<cmf::VertexElement>& decl )
{
	// Element count must be between 1 and 4
	if( element.elementCount == 0 || element.elementCount > 4 )
	{
		return false;
	}

	switch( element.usage )
	{
	case cmf::Usage::Position:
	case cmf::Usage::Normal:
	case cmf::Usage::Tangent:
	case cmf::Usage::Binormal:
	case cmf::Usage::TexCoord:
	case cmf::Usage::Color:
	case cmf::Usage::BoneIndices:
	case cmf::Usage::BoneWeights:
	case cmf::Usage::PackedTangent:
	case cmf::Usage::PackedTangentLegacy:
		break;
	default:
		return false;
	}

	if( !IsValidElementType( element.type ) )
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
		// Packed tangent must be 4-component signed normalized 16 bit integer
		if( ( element.type != cmf::ElementType::Int16Norm ) || element.elementCount != 4 )
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

	if( element.offset % cmf::GetElementTypeSize( element.type ) != 0 )
	{
		return false;
	}

	if( element.usage == cmf::Usage::BoneIndices )
	{
		if( element.type != cmf::ElementType::UInt8 && element.type != cmf::ElementType::UInt16 )
		{
			return false;
		}

		if( element.usageIndex != 0 )
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

	for( size_t i = 0; i < decl.size(); i++ )
	{
		uint64_t a_lo = decl[i].offset;
		uint64_t a_hi = a_lo + cmf::GetVertexElementSize( decl[i] );
		for( size_t j = i + 1; j < decl.size(); j++ )
		{
			uint64_t b_lo = decl[j].offset;
			uint64_t b_hi = b_lo + cmf::GetVertexElementSize( decl[j] );
			if( a_lo < b_hi && b_lo < a_hi )
			{
				return false;
			}
		}
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
	if( lod.vb.stride == 0 )
	{
		return "LOD " + std::to_string( lodIndex ) + " has stride 0";
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

	if( mesh.topology == cmf::MeshTopology::TriangleList && lod.ib.size > 0 )
	{
		if( ( lod.ib.size / lod.ib.stride ) % 3 != 0 )
		{
			return "LOD " + std::to_string( lodIndex ) + " index buffer must contain multiples of 3 for TriangleList topology";
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
		if( uint64_t( area.firstElement ) * verticesPerElement + uint64_t( area.elementCount ) * verticesPerElement > vertexCount )
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
		if( morph.vb.stride == 0 )
		{
			return "LOD " + std::to_string( lodIndex ) + " morph target " + std::to_string( i ) + " contains data but has stride 0";
		}
		// Morph target vertex buffer must have the same number of vertices as the LOD
		if( morph.vb.size / morph.vb.stride != lod.vb.size / lod.vb.stride )
		{
			return "LOD " + std::to_string( lodIndex ) + " morph target " + std::to_string( i ) + " vertex count does not match LOD vertex count";
		}
	}

	for( const auto& element : mesh.decl )
	{
		uint32_t elementSize = cmf::GetVertexElementSize( element );
		if( uint64_t( element.offset ) + elementSize > lod.vb.stride )
		{
			return "LOD " + std::to_string( lodIndex ) + " has a vertex element that extends past vb stride";
		}
	}

	uint32_t morphStride = std::numeric_limits<uint32_t>::max();
	for( const auto& morphTarget : lod.morphTargets )
	{
		if( morphTarget.vb.size == 0 )
		{
			continue;
		}

		if( morphStride == std::numeric_limits<uint32_t>::max() )
		{
			morphStride = morphTarget.vb.stride;
		}
		else if( morphStride != morphTarget.vb.stride )
		{
			return "LOD " + std::to_string( lodIndex ) + " has morph targets with varying strides";
		}
	}

	if( morphStride != std::numeric_limits<uint32_t>::max() )
	{
		for( const auto& element : mesh.morphTargets.decl )
		{
			uint32_t elementSize = cmf::GetVertexElementSize( element );
			if( uint64_t( element.offset ) + elementSize > morphStride )
			{
				return "LOD " + std::to_string( lodIndex ) + " has a morph target with vertex element that extends past vb stride";
			}
		}
	}

	return {};
}

std::string MeshHasValidLodThresholds( const cmf::Mesh& mesh )
{
	// First LOD must have threshold of MeshLod::MAX_THRESHOLD
	if( mesh.lods[0].threshold != cmf::MeshLod::MAX_THRESHOLD )
	{
		return "First LOD threshold must be MeshLod::MAX_THRESHOLD";
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


std::string IsMeshValid( const cmf::Mesh& mesh, const cmf::Span<cmf::Skeleton>& skeletons )
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

	switch( mesh.topology )
	{
	case cmf::MeshTopology::PointList:
	case cmf::MeshTopology::TriangleList:
		break;
	default:
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has invalid topology";
	}

	for( size_t i = 0; i < mesh.lods.size(); ++i )
	{
		auto error = IsMeshLodValid( mesh, mesh.lods[i], i );
		if( !error.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\": " + error;
		}
	}

	for( const auto& lod : mesh.lods )
	{
		if( lod.vb.stride != mesh.lods[0].vb.stride )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" has lods with varying strides";
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

		if( mesh.areas[areaIdx].bounds.IsInitialized() )
		{
			for( int i = 0; i < 3; i++ )
			{
				if( mesh.areas[areaIdx].bounds.m_max[i] < mesh.areas[areaIdx].bounds.m_min[i] )
				{
					return "Mesh \"" + ToStdString( mesh.name ) + "\" area " + std::to_string( areaIdx ) + " has invalid bounds";
				}
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

	{
		const auto* boneIndicesElement = FindElement( mesh.decl, cmf::Usage::BoneIndices );

		if( boneIndicesElement && mesh.boneBindings.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" has boneIndices but no boneBindings";
		}

		if( !mesh.boneBindings.empty() && !boneIndicesElement )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" has boneBindings but no boneIndices";
		}

		if( boneIndicesElement )
		{
			if( boneIndicesElement->type == cmf::ElementType::UInt8 && mesh.boneBindings.size() > std::numeric_limits<uint8_t>::max() )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has more than 255 bone bindings with UInt8 bone indices";
			}
		}
	}

	if( mesh.boneBindings.size() > std::numeric_limits<uint16_t>::max() )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has more than 65535 bone bindings";
	}

	for( const auto& boneBinding : mesh.boneBindings )
	{
		if( boneBinding.name.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" has boneBinding with empty name";
		}
	}

	for( size_t i = 0; i < mesh.boneBindings.size(); i++ )
	{
		for( size_t j = i + 1; j < mesh.boneBindings.size(); j++ )
		{
			if( mesh.boneBindings[i].name == mesh.boneBindings[j].name )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has duplicate boneBinding name " + ToStdString( mesh.boneBindings[i].name );
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
		if( mesh.skeleton >= skeletons.size() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" references out-of-range skeleton index";
		}

		if( mesh.boneBindings.size() > skeletons[mesh.skeleton].bones.size() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" binds more bones than present in the referenced skeleton";
		}

		for( const auto& boneBinding : mesh.boneBindings )
		{
			if( std::find( skeletons[mesh.skeleton].bones.begin(), skeletons[mesh.skeleton].bones.end(), boneBinding.name ) == skeletons[mesh.skeleton].bones.end() )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has boneBinding which cannot be found in referenced skeleton";
			}
		}
	}

	if( !mesh.audioOcclusionMesh.vertices.empty() && mesh.audioOcclusionMesh.indices.empty() )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has audioOcclusionMesh without indices";
	}

	if( !mesh.audioOcclusionMesh.indices.empty() && mesh.audioOcclusionMesh.vertices.empty() )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has audioOcclusionMesh without vertices";
	}

	if( mesh.audioOcclusionMesh.indices.size() % 3 != 0 )
	{
		return "Mesh \"" + ToStdString( mesh.name ) + "\" has audioOcclusionMesh that does not consist of triangles";
	}

	if( mesh.audioOcclusionMesh.bounds.IsInitialized() )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( mesh.audioOcclusionMesh.bounds.m_max[i] < mesh.audioOcclusionMesh.bounds.m_min[i] )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has audioOcclusionMesh with invalid bounds";
			}
		}
	}

	for( uint16_t idx : mesh.audioOcclusionMesh.indices )
	{
		if( idx >= mesh.audioOcclusionMesh.vertices.size() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" has audioOcclusionMesh with out-of-range index";
		}
	}

	if( mesh.bounds.IsInitialized() )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( mesh.bounds.m_max[i] < mesh.bounds.m_min[i] )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has invalid bounds";
			}
		}
	}

	for( const auto& morphTarget : mesh.morphTargets.targets )
	{
		if( morphTarget.name.empty() )
		{
			return "Mesh \"" + ToStdString( mesh.name ) + "\" has empty morphTarget name";
		}
	}
	for( size_t i = 0; i < mesh.morphTargets.targets.size(); i++ )
	{
		for( size_t j = i + 1; j < mesh.morphTargets.targets.size(); j++ )
		{
			if( mesh.morphTargets.targets[i].name == mesh.morphTargets.targets[j].name )
			{
				return "Mesh \"" + ToStdString( mesh.name ) + "\" has duplicate morphTarget name " + ToStdString( mesh.morphTargets.targets[i].name );
			}
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
	for( const auto& boneName : skeleton.bones )
	{
		if( boneName.empty() )
		{
			return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has empty bone name";
		}
	}
	for( size_t i = 0; i < skeleton.bones.size(); i++ )
	{
		for( size_t j = i + 1; j < skeleton.bones.size(); j++ )
		{
			if( skeleton.bones[i] == skeleton.bones[j] )
			{
				return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has duplicate bone name " + ToStdString( skeleton.bones[i] );
			}
		}
	}
	for( const auto& mask : skeleton.boneMasks )
	{
		if( mask.name.empty() )
		{
			return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has empty boneMask name";
		}
	}
	for( size_t i = 0; i < skeleton.boneMasks.size(); i++ )
	{
		for( size_t j = i + 1; j < skeleton.boneMasks.size(); j++ )
		{
			if( skeleton.boneMasks[i].name == skeleton.boneMasks[j].name )
			{
				return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has duplicate boneMask name " + ToStdString( skeleton.boneMasks[i].name );
			}
		}
	}
	for( const auto& mask : skeleton.boneMasks )
	{
		for( const auto& weight : mask.weights )
		{
			if( weight.index >= skeleton.bones.size() )
			{
				return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has boneMask " + ToStdString( mask.name ) + " with invalid index";
			}
			if( std::clamp( weight.weight, 0.f, 1.f ) != weight.weight )
			{
				return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has boneMask " + ToStdString( mask.name ) + " with weight outside range [0, 1]";
			}
		}
	}
	for( const auto& transform : skeleton.restTransforms )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( std::isinf( transform.position[i] ) || std::isnan( transform.position[i] ) )
			{
				return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has restTransform position with invalid float";
			}
		}
		if( std::isinf( transform.rotation.x ) || std::isnan( transform.rotation.x ) ||
			std::isinf( transform.rotation.y ) || std::isnan( transform.rotation.y ) ||
			std::isinf( transform.rotation.z ) || std::isnan( transform.rotation.z ) ||
			std::isinf( transform.rotation.w ) || std::isnan( transform.rotation.w ) )
		{
			return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has restTransform rotation with invalid float";
		}
		for( int i = 0; i < 3; i++ )
		{
			if( std::isinf( transform.scale[i] ) || std::isnan( transform.scale[i] ) )
			{
				return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has restTransform scale with invalid float";
			}
		}
	}
	for( const auto& transform : skeleton.invBindTransforms )
	{
		for( int i = 0; i < 4; i++ )
		{
			for( int j = 0; j < 4; j++ )
			{
				if( std::isinf( transform.m[i][j] ) || std::isnan( transform.m[i][j] ) )
				{
					return "Skeleton \"" + ToStdString( skeleton.name ) + "\" has invBindTransform with invalid float";
				}
			}
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

	if( !IsValidElementType( curve.knotType ) )
	{
		return "Curve has invalid knotType";
	}

	switch( curve.interpolation )
	{
	case cmf::Interpolation::Step:
	case cmf::Interpolation::Linear:
		break;
	default:
		return "Curve has invalid interpolation";
	}

	if( !IsValidElementType( curve.valueType ) )
	{
		return "Curve has invalid valueType";
	}

	// Knots must be in ascending order
	cmf::VertexElement element = {};
	element.type = curve.knotType;
	element.elementCount = 1;
	uint32_t stride = cmf::GetVertexElementSize( element );
	if( curve.knots.size() != uint64_t( curve.knotCount ) * stride )
	{
		return "Curve keyframe buffer size does not match keyframes count and time type";
	}
	const cmf::ConstBufferElementStream<float> knots{ element, curve.knots.data(), uint32_t( curve.knots.size() / stride ), stride };
	for( uint32_t i = 0; i < knots.size(); ++i )
	{
		if( std::isinf( knots[i] ) || std::isnan( knots[i] ) )
		{
			return "Curve has knots with invalid float";
		}
	}
	for( uint32_t i = 1; i < knots.size(); ++i )
	{
		if( knots[i] < knots[i - 1] )
		{
			return "Curve keyframes are not in ascending order";
		}
	}

	if( curve.valueDimension == 0 )
	{
		return "Curve has zero valueDimension";
	}

	if( curve.values.size() != uint64_t( curve.knotCount ) * curve.valueDimension * cmf::GetElementTypeSize( curve.valueType ) )
	{
		return "Curve value buffer size does not match keyframes count, value dimension and value type";
	}

	cmf::VertexElement valueElement = {};
	valueElement.type = curve.valueType;
	valueElement.elementCount = 1;
	uint32_t valueStride = cmf::GetVertexElementSize( valueElement );
	const cmf::ConstBufferElementStream<float> values{ valueElement, curve.values.data(), uint32_t( curve.values.size() / valueStride ), valueStride };
	for( uint32_t i = 0; i < values.size(); ++i )
	{
		if( std::isinf( values[i] ) || std::isnan( values[i] ) )
		{
			return "Curve has values with invalid float";
		}
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
		case cmf::AnimationChannelTargetType::BoneRotation:
		case cmf::AnimationChannelTargetType::BoneScale:
		case cmf::AnimationChannelTargetType::MorphTarget:
		case cmf::AnimationChannelTargetType::Other:
			break;
		default:
			return "Animation \"" + ToStdString( animation.name ) + "\" channel " + std::to_string( i ) + " has invalid targetType";
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

		if( animation.channels[i].target.empty() )
		{
			return "Animation \"" + ToStdString( animation.name ) + "\" channel " + std::to_string( i ) + " has empty target name";
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
	auto spanPointerErrorMessage = AreSpanPointersValid( mainData, &mainData, header.sections[0].uncompressedSize );
	if( !spanPointerErrorMessage.empty() )
	{
		return "Main data contains invalid span pointers: " + spanPointerErrorMessage;
	}
	auto bufferViewPointerErrorMessage = AreBufferViewsValid( mainData, header.sections );
	if( !bufferViewPointerErrorMessage.empty() )
	{
		return "Main data contains invalid buffer views: " + bufferViewPointerErrorMessage;
	}

	for( size_t i = 0; i < mainData.meshes.size(); ++i )
	{
		auto error = IsMeshValid( mainData.meshes[i], mainData.skeletons );
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

std::string IsMetadataValid( const cmf::Metadata& metaData, size_t sectionSize )
{
	auto spanPointerErrorMessage = AreSpanPointersValid( metaData, &metaData, sectionSize );
	if( !spanPointerErrorMessage.empty() )
	{
		return "Meta data contains invalid span pointers: " + spanPointerErrorMessage;
	}

	for( size_t i = 0; i < metaData.entries.size(); i++ )
	{
		if( metaData.entries[i].key.empty() )
		{
			return "Meta data contains empty key";
		}

		for( size_t j = i + 1; j < metaData.entries.size(); j++ )
		{
			if( metaData.entries[i].key == metaData.entries[j].key )
			{
				return "Meta data contains duplicate keys";
			}
		}
	}

	return {};
}

const uint8_t* GetDecompressedSection( uint32_t index, const void* fileData, const cmf::Span<cmf::Section>& sections, std::map<uint32_t, std::vector<uint8_t>>& cache )
{
	const auto& section = sections[index];
	const auto* sectionData = static_cast<const uint8_t*>( fileData ) + section.offset;
	if( section.compression == cmf::SectionCompression::None )
	{
		return sectionData;
	}
	auto [it, inserted] = cache.try_emplace( index );
	if( inserted )
	{
		it->second.resize( section.uncompressedSize );
		cmf::Decompress( it->second.data(), section, sectionData );
	}
	return it->second.data();
}

std::string AreFloatElementsFinite( const cmf::Span<cmf::VertexElement>& decl, const cmf::BufferView& vb, const uint8_t* sectionData )
{
	if( vb.stride == 0 )
	{
		return {};
	}
	uint32_t vertexCount = vb.size / vb.stride;
	const auto* bufferData = sectionData + vb.offset;
	for( const auto& element : decl )
	{
		if( element.type != cmf::ElementType::Float32 && element.type != cmf::ElementType::Float16 )
		{
			continue;
		}
		const cmf::ConstBufferElementStream<Vector4> stream{ element, bufferData, vertexCount, vb.stride };
		for( const auto& value : stream )
		{
			for( int i = 0; i < 4 && i < element.elementCount; i++ )
			{
				if( std::isinf( value[i] ) || std::isnan( value[i] ) )
				{
					return "contains an invalid float";
				}
			}
		}
	}
	return {};
}

std::string AreBuffersValid( const cmf::Data& mainData, const cmf::Header& header, const void* fileData )
{
	std::map<uint32_t, std::vector<uint8_t>> sectionCache;
	for( const auto& mesh : mainData.meshes )
	{
		for( size_t lodIndex = 0; lodIndex < mesh.lods.size(); lodIndex++ )
		{
			const auto& lod = mesh.lods[lodIndex];
			if( lod.vb.size != 0 )
			{
				auto error = AreFloatElementsFinite( mesh.decl, lod.vb, GetDecompressedSection( lod.vb.index, fileData, header.sections, sectionCache ) );
				if( !error.empty() )
				{
					return "Mesh \"" + ToStdString( mesh.name ) + "\" LOD " + std::to_string( lodIndex ) + " vertex buffer " + error;
				}
			}
			for( size_t i = 0; i < lod.morphTargets.size(); i++ )
			{
				const auto& morph = lod.morphTargets[i];
				if( morph.vb.size == 0 )
				{
					continue;
				}
				auto error = AreFloatElementsFinite( mesh.morphTargets.decl, morph.vb, GetDecompressedSection( morph.vb.index, fileData, header.sections, sectionCache ) );
				if( !error.empty() )
				{
					return "Mesh \"" + ToStdString( mesh.name ) + "\" LOD " + std::to_string( lodIndex ) + " morph target " + std::to_string( i ) + " " + error;
				}
			}
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

	if( !options.validateHeader && !options.validateMainData && !options.validateBuffers )
	{
		return { true, {} };
	}

	auto spanPointerErrorMessage = AreSpanPointersValid( header, &header, header.headerSize );
	if( !spanPointerErrorMessage.empty() )
	{
		return { false, "Header contains invalid span pointers: " + spanPointerErrorMessage };
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

	const auto mainDataAddress = static_cast<const uint8_t*>( data ) + header.sections[0].offset;
	if( reinterpret_cast<uintptr_t>( mainDataAddress ) % alignof( cmf::Data ) != 0 )
	{
		return { false, "Data section not aligned" };
	}

	const auto& mainData = *reinterpret_cast<const Data*>( mainDataAddress );
	{
		auto error = IsMainDataValid( mainData, header );
		if( !error.empty() )
		{
			return { false, error };
		}
	}

	const auto& lastSection = header.sections[header.sections.size() - 1];
	if( lastSection.type == cmf::SectionType::Metadata )
	{
		const auto metadataAddress = static_cast<const uint8_t*>( data ) + lastSection.offset;
		if( reinterpret_cast<uintptr_t>( metadataAddress ) % alignof( cmf::Metadata ) != 0 )
		{
			return { false, "Metadata section not aligned" };
		}

		const auto& metadata = *reinterpret_cast<const Metadata*>( metadataAddress );
		auto error = IsMetadataValid( metadata, lastSection.uncompressedSize );
		if( !error.empty() )
		{
			return { false, error };
		}
	}
	if( !options.validateBuffers )
	{
		return { true, {} };
	}

	auto error = AreBuffersValid( mainData, header, data );
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