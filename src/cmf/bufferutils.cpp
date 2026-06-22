// Copyright © 2026 CCP ehf.

#include "cmf/bufferutils.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include <meshoptimizer.h>
#include <numeric>

namespace
{

void CopyElement( const cmf::VertexElement& srcElement, const cmf::BufferView& srcView, const cmf::VertexElement& dstElement, const cmf::BufferView& dstView, cmf::BufferManager& bufferManager )
{
	const auto vertexCount = dstView.size / dstView.stride;
	if( srcElement.type == dstElement.type && srcElement.elementCount == dstElement.elementCount )
	{
		const auto size = cmf::GetVertexElementSize( dstElement );
		const auto* oldBuffer = static_cast<const uint8_t*>( bufferManager.GetData( srcView ) );
		auto* newBuffer = static_cast<uint8_t*>( bufferManager.GetData( dstView ) );
		for( uint32_t i = 0; i < vertexCount; ++i )
		{
			memcpy(
				newBuffer + i * dstView.stride + dstElement.offset,
				oldBuffer + i * srcView.stride + srcElement.offset,
				size );
		}
	}
	else
	{
		const cmf::ConstBufferElementStream<Vector4> oldStream( srcElement, srcView, bufferManager );
		const cmf::BufferElementStream<Vector4> newStream( dstElement, dstView, bufferManager );
		for( uint32_t i = 0; i < vertexCount; ++i )
		{
			newStream.set( i, oldStream[i] );
		}
	}
}

void ZeroFillElement( const cmf::VertexElement& element, const cmf::BufferView& view, cmf::BufferManager& bufferManager )
{
	const auto vertexCount = view.size / view.stride;
	const auto size = cmf::GetVertexElementSize( element );
	auto* buffer = static_cast<uint8_t*>( bufferManager.GetData( view ) );
	for( uint32_t i = 0; i < vertexCount; ++i )
	{
		memset(
			buffer + i * view.stride + element.offset,
			0,
			size );
	}
}

}

namespace cmf
{

BufferView UnapplyIndexBuffer( const BufferView& vb, const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	auto indexCount = ib.size / ib.stride;

	auto newVB = bufferManager.AllocateBuffer( nullptr, vb.stride * indexCount, vb.stride );

	ConstIndexBufferStream oldIndices( ib, bufferManager );
	for( uint32_t i = 0; i < oldIndices.size(); ++i )
	{
		uint32_t index = oldIndices[i];
		memcpy(
			static_cast<uint8_t*>( bufferManager.GetData( newVB ) ) + i * vb.stride,
			static_cast<uint8_t*>( bufferManager.GetData( vb ) ) + index * vb.stride,
			vb.stride );
	}
	return newVB;
}

BufferView MakeIdentityIndexBuffer( uint32_t indexCount, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	auto newIB = bufferManager.AllocateBuffer( nullptr, indexCount * sizeof( uint32_t ), sizeof( uint32_t ) );
	for( uint32_t i = 0; i < indexCount; ++i )
	{
		static_cast<uint32_t*>( bufferManager.GetData( newIB ) )[i] = i;
	}
	return newIB;
}

BufferView ChangeBufferVertexDeclaration( const BufferView& bufferView, const Span<VertexElement>& oldDecl, const Span<VertexElement>& newDecl, MemoryAllocator& allocator, BufferManager& bufferManager, uint32_t alignment )
{
	const auto vertexCount = bufferView.size / bufferView.stride;
	uint32_t newVertexStride = 0;
	for( const auto& element : newDecl )
	{
		newVertexStride = std::max( newVertexStride, element.offset + GetVertexElementSize( element ) );
	}
	newVertexStride = ( newVertexStride + alignment - 1 ) & ~( alignment - 1 );
	const auto newView = bufferManager.AllocateBuffer( nullptr, vertexCount * newVertexStride, newVertexStride );

	for( const auto& newElement : newDecl )
	{
		const auto* oldElement = FindElement( oldDecl, newElement.usage, newElement.usageIndex );
		if( oldElement )
		{
			CopyElement( *oldElement, bufferView, newElement, newView, bufferManager );
		}
		else
		{
			ZeroFillElement( newElement, newView, bufferManager );
		}
	}
	return newView;
}

void RemoveDuplicateVertices( MeshLod& lod, BufferManager& bufferManager )
{
	auto indexData = bufferManager.GetData( lod.ib );
	uint32_t indexCount = lod.ib.size / lod.ib.stride;

	auto vertexData = bufferManager.GetData( lod.vb );
	uint32_t vertexCount = lod.vb.size / lod.vb.stride;
	uint32_t vertexStride = lod.vb.stride;

	std::vector<unsigned> remap( vertexCount );
	uint32_t newVertexCount;
	// TODO: we need to take morph targets into account when deduplicating vertices.
	if( lod.ib.stride == 4 )
	{
		newVertexCount = (uint32_t)meshopt_generateVertexRemap( remap.data(), reinterpret_cast<uint32_t*>( indexData ), indexCount, vertexData, vertexCount, vertexStride );
	}
	else
	{
		newVertexCount = (uint32_t)meshopt_generateVertexRemap( remap.data(), reinterpret_cast<uint16_t*>( indexData ), indexCount, vertexData, vertexCount, vertexStride );
	}
	if( newVertexCount == vertexCount )
	{
		// No duplicates found.
		return;
	}

	lod.vb = bufferManager.AllocateBuffer( nullptr, newVertexCount * vertexStride, vertexStride );

	meshopt_remapVertexBuffer( bufferManager.GetData( lod.vb ), vertexData, vertexCount, vertexStride, remap.data() );
	if( lod.ib.stride == 4 )
	{
		meshopt_remapIndexBuffer( reinterpret_cast<uint32_t*>( indexData ), reinterpret_cast<uint32_t*>( indexData ), indexCount, remap.data() );
	}
	else
	{
		meshopt_remapIndexBuffer( reinterpret_cast<uint16_t*>( indexData ), reinterpret_cast<uint16_t*>( indexData ), indexCount, remap.data() );
	}
	for( auto& morph : lod.morphTargets )
	{
		auto morphData = bufferManager.GetData( morph.vb );
		morph.vb = bufferManager.AllocateBuffer( nullptr, newVertexCount * morph.vb.stride, morph.vb.stride );
		meshopt_remapVertexBuffer( bufferManager.GetData( morph.vb ), morphData, vertexCount, morph.vb.stride, remap.data() );
	}
}

BufferView ConvertTo16BitIndexBuffer( const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	if( ib.stride == 2 )
	{
		// Already 16-bit.
		return ib;
	}
	auto indexData = static_cast<const uint32_t*>( bufferManager.GetData( ib ) );
	uint32_t indexCount = ib.size / ib.stride;
	uint32_t maxIndex = 0;
	for( uint32_t i = 0; i < indexCount; ++i )
	{
		maxIndex = std::max( maxIndex, indexData[i] );
	}
	if( maxIndex > 0xffffu )
	{
		// Cannot convert to 16-bit because of large indices.
		return ib;
	}
	auto newIB = bufferManager.AllocateBuffer( nullptr, indexCount * sizeof( uint16_t ), sizeof( uint16_t ) );
	for( uint32_t i = 0; i < indexCount; ++i )
	{
		static_cast<uint16_t*>( bufferManager.GetData( newIB ) )[i] = (uint16_t)indexData[i];
	}
	return newIB;
}

void OptimizeBuffers( const Mesh& mesh, MeshLod& lod, BufferManager& bufferManager )
{
	const float optimizeOverdrawThreshold = 1.01f;

	if( mesh.topology != MeshTopology::TriangleList )
	{
		// Optimization is only supported for triangle lists.
		return;
	}

	const auto vertexCount = lod.vb.size / lod.vb.stride;
	std::vector<float> positions;
	const auto* positionElement = FindElement( mesh.decl, Usage::Position );
	if( positionElement )
	{
		// Extract positions
		const cmf::ConstBufferElementStream<Vector3> posStream( *positionElement, lod.vb, bufferManager );
		positions.reserve( vertexCount * 3 );
		for( const auto pos : posStream )
		{
			positions.push_back( pos.x );
			positions.push_back( pos.y );
			positions.push_back( pos.z );
		}
	}

	for( const auto& area : lod.areas )
	{
		void* indexData = static_cast<uint8_t*>( bufferManager.GetData( lod.ib ) ) + area.firstElement * 3 * lod.ib.stride;
		uint16_t* indexData16 = lod.ib.stride == 2 ? static_cast<uint16_t*>( indexData ) : nullptr;
		uint32_t* indexData32 = lod.ib.stride != 2 ? static_cast<uint32_t*>( indexData ) : nullptr;
		if( indexData16 )
		{
			meshopt_optimizeVertexCache( indexData16, indexData16, area.elementCount * 3, vertexCount );
		}
		else if( indexData32 )
		{
			meshopt_optimizeVertexCache( indexData32, indexData32, area.elementCount * 3, vertexCount );
		}

		if( positionElement )
		{
			if( indexData16 )
			{
				meshopt_optimizeOverdraw( indexData16, indexData16, area.elementCount * 3, positions.data(), vertexCount, sizeof( Vector3 ), optimizeOverdrawThreshold );
			}
			else if( indexData32 )
			{
				meshopt_optimizeOverdraw( indexData32, indexData32, area.elementCount * 3, positions.data(), vertexCount, sizeof( Vector3 ), optimizeOverdrawThreshold );
			}
		}
	}

	// Reorder all the vertices so that they are in the order that they appear in the optimized index buffer.
	// This improves cache coherency when reading vertex attributes, which is a minor but easy win.
	{
		std::vector<uint32_t> remap( vertexCount );

		void* indexData = static_cast<uint8_t*>( bufferManager.GetData( lod.ib ) );
		uint16_t* indexData16 = lod.ib.stride == 2 ? static_cast<uint16_t*>( indexData ) : nullptr;
		uint32_t* indexData32 = lod.ib.stride != 2 ? static_cast<uint32_t*>( indexData ) : nullptr;

		uint32_t newVertexCount = 0;
		if( indexData16 )
		{
			newVertexCount = static_cast<uint32_t>( meshopt_optimizeVertexFetchRemap( remap.data(), indexData16, lod.ib.size / lod.ib.stride, vertexCount ) );
			meshopt_remapIndexBuffer( indexData16, indexData16, lod.ib.size / lod.ib.stride, remap.data() );
		}
		else if( indexData32 )
		{
			newVertexCount = static_cast<uint32_t>( meshopt_optimizeVertexFetchRemap( remap.data(), indexData32, lod.ib.size / lod.ib.stride, vertexCount ) );
			meshopt_remapIndexBuffer( indexData32, indexData32, lod.ib.size / lod.ib.stride, remap.data() );
		}
		else
		{
			return;
		}

		auto* vertexData = bufferManager.GetData( lod.vb );
		if( newVertexCount != vertexCount )
		{
			lod.vb = bufferManager.AllocateBuffer( nullptr, newVertexCount * lod.vb.stride, lod.vb.stride );
			meshopt_remapVertexBuffer( bufferManager.GetData( lod.vb ), vertexData, vertexCount, lod.vb.stride, remap.data() );
		}
		else
		{
			meshopt_remapVertexBuffer( vertexData, vertexData, vertexCount, lod.vb.stride, remap.data() );
		}
		for( auto& morph : lod.morphTargets )
		{
			auto* morphData = bufferManager.GetData( morph.vb );
			if( newVertexCount != vertexCount )
			{
				morph.vb = bufferManager.AllocateBuffer( nullptr, newVertexCount * morph.vb.stride, morph.vb.stride );
				meshopt_remapVertexBuffer( bufferManager.GetData( morph.vb ), morphData, vertexCount, morph.vb.stride, remap.data() );
			}
			else
			{
				meshopt_remapVertexBuffer( morphData, morphData, vertexCount, morph.vb.stride, remap.data() );
			}
		}
	}
}

void OptimizeBuffers( Mesh& mesh, BufferManager& bufferManager )
{
	for( auto& lod : mesh.lods )
	{
		OptimizeBuffers( mesh, lod, bufferManager );
	}
}
}
