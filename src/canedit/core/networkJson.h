// Copyright © 2026 CCP ehf.

#pragma once

#include "document.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace canedit
{

/** What graph JSON is written for. */
enum class GraphJsonMode : uint8_t
{
	/** Copied nodes: Parameter nodes store the parameter definition, so that they can be pasted into another
	* network. */
	Clipboard,
	/** A whole document: Parameter nodes reference network parameters by name, and graphs store their view. */
	Document,
};

/** Writes nodes of a graph as JSON: their settings, input constants, positions, the contents of Subgraph nodes and
* the edges between the given nodes. Nodes are stored in the given order; edges reference them by index. */
[[nodiscard]] nlohmann::json WriteGraphJson( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes, GraphJsonMode mode );

/** Creates the nodes and edges written by WriteGraphJson in a graph.
*
* Parts that can't be applied (an unknown node type, an edge between incompatible sockets, ...) are skipped and
* reported as warnings. JSON of the wrong structure throws nlohmann::json::exception.
*/
class GraphJsonReader
{
public:
	GraphJsonReader( Document& document, std::vector<std::string>& warnings, GraphJsonMode mode );

	/** Adds the nodes and edges to a graph; node positions are moved by offset. Returns the handles of the created
	* nodes in stored order, unset for nodes that were skipped. */
	std::vector<cmf::AnimationNodeId> ReadGraph( cmf::AnimationGraphBuilder& graph, const nlohmann::json& contents, Vec2 offset );

private:
	cmf::AnimationNodeId ReadNode( cmf::AnimationGraphBuilder& graph, const nlohmann::json& json, Vec2 offset );
	void ApplySettings( cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId id, cmf::AnimationNodeType type, const nlohmann::json& json );
	uint32_t FindParameter( const nlohmann::json& json );
	uint32_t FindOrAddParameter( const nlohmann::json& json );
	void Check( const cmf::ValidationResult& result );

	Document* m_document;
	std::vector<std::string>* m_warnings;
	GraphJsonMode m_mode;
};

/** Converts parameters to and from JSON. Non-finite limits are stored as null, because JSON has no infinity. */
[[nodiscard]] nlohmann::json WriteParameterJson( const cmf::AnimationParameter& parameter );
[[nodiscard]] cmf::AnimationParameter ReadParameterJson( const nlohmann::json& json );

}
