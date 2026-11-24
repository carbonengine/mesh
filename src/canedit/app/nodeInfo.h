// Copyright © 2026 CCP ehf.

#pragma once

#include <cmf/animationnetworkbuilder.h>

#include <imgui.h>

#include <string>
#include <vector>

namespace canedit
{

/** A group of node types in the palette. */
struct NodeCategory
{
	const char* name;
	ImU32 color;
	std::vector<cmf::AnimationNodeType> types;
};

/** Returns the node categories shown in the palette. */
const std::vector<NodeCategory>& GetNodeCategories();

/** Returns the header color of a node type. */
ImU32 GetNodeColor( cmf::AnimationNodeType type );

/** Returns the color of edges and sockets of a socket type. */
ImU32 GetSocketColor( cmf::AnimationSocketType type );

/** Returns a short description of a node type for tooltips. */
const char* GetNodeDescription( cmf::AnimationNodeType type );

/** Returns the node name, or its type name if it has none. */
std::string GetNodeTitle( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

/** Returns a one line summary of the node settings, for example the animation of an AnimationSample node. */
std::string GetNodeSummary( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

/** Problems of a single node, shown as badges in the graph. */
struct NodeIssues
{
	/** Prevent evaluation, for example an unconnected pose input. */
	std::vector<std::string> errors;
	/** Worked around, for example a missing animation. */
	std::vector<std::string> warnings;
};

NodeIssues DiagnoseNode( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

/** Returns true if a node type can be added to a graph (interface nodes are limited in the root graph). */
bool CanAddNode( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeType type );

/** Returns the name of a socket type. */
const char* GetSocketTypeName( cmf::AnimationSocketType type );

}
