// Copyright © 2026 CCP ehf.

#pragma once

#include "document.h"

#include <map>

namespace canedit
{

/** Horizontal distance between layout columns, in graph units. */
constexpr float LAYOUT_COLUMN_SPACING = 300.f;
/** Vertical distance between nodes of a layout column, in graph units. */
constexpr float LAYOUT_ROW_SPACING = 170.f;

/** Computes positions for all nodes of a graph, keyed by node slot index.
*
* Nodes are arranged in columns by their distance from the nodes they feed: nodes whose output isn't connected
* (such as the Output node) form the rightmost column, their inputs the column to the left of it, and so on. Within
* a column, nodes are ordered by the average row of the nodes they feed to reduce edge crossings. Nodes without any
* connection are placed in a row below the graph. The result only depends on the graph. */
[[nodiscard]] std::map<uint32_t, Vec2> ComputeAutoLayout( const cmf::AnimationGraphBuilder& graph );

}
