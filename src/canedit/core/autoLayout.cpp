// Copyright © 2026 CCP ehf.

#include "autoLayout.h"

#include <algorithm>
#include <set>

namespace canedit
{
namespace
{

/** Small offset that orders nodes with the same sort key by slot index, so that the layout is stable. */
constexpr float TIE_BREAKER = 1e-3f;

struct LayoutGraph // NOLINT(bugprone-exception-escape): the implicit move constructor of std::map is not noexcept in some standard libraries
{
	std::vector<cmf::AnimationNodeId> nodes;
	/** Consumers of each node, by slot index. */
	std::map<uint32_t, std::set<uint32_t>> consumers;
	/** Nodes with at least one edge. */
	std::set<uint32_t> connected;
	/** Column of each node: its longest path to a node whose output isn't connected. */
	std::map<uint32_t, uint32_t> columns;
	uint32_t columnCount = 0;
};

LayoutGraph Analyze( const cmf::AnimationGraphBuilder& graph )
{
	LayoutGraph result;
	result.nodes = graph.GetNodes();
	const auto& edges = graph.GetEdges();
	for( const auto& edge : edges )
	{
		result.consumers[edge.from.node.index].insert( edge.to.node.index );
		result.connected.insert( edge.from.node.index );
		result.connected.insert( edge.to.node.index );
	}
	for( const auto id : result.nodes )
	{
		result.columns[id.index] = 0;
	}
	// Edges form no cycles, so relaxing them repeatedly finds the longest paths
	bool changed = true;
	for( size_t pass = 0; pass < result.nodes.size() && changed; ++pass )
	{
		changed = false;
		for( const auto& edge : edges )
		{
			const auto required = result.columns[edge.to.node.index] + 1;
			auto& column = result.columns[edge.from.node.index];
			if( column < required )
			{
				column = required;
				changed = true;
			}
		}
	}
	for( const auto index : result.connected )
	{
		result.columnCount = std::max( result.columnCount, result.columns[index] + 1 );
	}
	return result;
}

// Returns the average row of the consumers of a node, or its slot index if it has none
float GetSortKey( const LayoutGraph& graph, uint32_t index, const std::map<uint32_t, float>& rows )
{
	const auto it = graph.consumers.find( index );
	if( it == graph.consumers.end() || it->second.empty() )
	{
		return static_cast<float>( index );
	}
	float sum = 0.f;
	for( const auto consumer : it->second )
	{
		const auto row = rows.find( consumer );
		sum += row == rows.end() ? 0.f : row->second;
	}
	return sum / static_cast<float>( it->second.size() ) + static_cast<float>( index ) * TIE_BREAKER;
}

// Assigns rows to the nodes of a column, ordered by the rows of their consumers; returns the end of the last row
float PlaceColumn( const LayoutGraph& graph, uint32_t column, std::map<uint32_t, float>& rows )
{
	std::vector<std::pair<float, uint32_t>> members;
	for( const auto index : graph.connected )
	{
		if( graph.columns.at( index ) == column )
		{
			members.emplace_back( GetSortKey( graph, index, rows ), index );
		}
	}
	std::sort( members.begin(), members.end() );
	// Center the column around the average row of its consumers
	const auto halfSize = static_cast<float>( members.size() - 1 ) * 0.5f;
	const float first = column == 0 || members.empty() ? 0.f : std::max( 0.f, members.front().first - halfSize );
	float end = 0.f;
	for( size_t i = 0; i < members.size(); ++i )
	{
		const auto row = first + static_cast<float>( i );
		rows[members[i].second] = row;
		end = row + 1.f;
	}
	return end;
}

}

std::map<uint32_t, Vec2> ComputeAutoLayout( const cmf::AnimationGraphBuilder& graph )
{
	const auto layout = Analyze( graph );

	// Columns are placed from the right, so that inputs follow the rows of the nodes they feed
	std::map<uint32_t, float> rows;
	float tallest = 0.f;
	for( uint32_t column = 0; column < layout.columnCount; ++column )
	{
		tallest = std::max( tallest, PlaceColumn( layout, column, rows ) );
	}

	std::map<uint32_t, Vec2> positions;
	for( const auto& [index, row] : rows )
	{
		const auto column = static_cast<float>( layout.columnCount - 1 - layout.columns.at( index ) );
		positions[index] = { column * LAYOUT_COLUMN_SPACING, row * LAYOUT_ROW_SPACING };
	}

	// Unconnected nodes go in a row below the graph
	float x = 0.f;
	for( const auto id : layout.nodes )
	{
		if( layout.connected.count( id.index ) == 0 )
		{
			positions[id.index] = { x, tallest * LAYOUT_ROW_SPACING };
			x += LAYOUT_COLUMN_SPACING;
		}
	}
	return positions;
}

}
