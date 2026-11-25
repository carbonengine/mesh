// Copyright © 2026 CCP ehf.

#pragma once

#include <cmf/animationnetworkbuilder.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace canedit
{

/** A point or offset in graph coordinates. */
struct Vec2
{
	float x = 0.f;
	float y = 0.f;

	bool operator==( const Vec2& other ) const
	{
		return x == other.x && y == other.y;
	}
	bool operator!=( const Vec2& other ) const
	{
		return !( *this == other );
	}
};

/** Pan and zoom of a graph view. */
struct GraphView
{
	/** Graph position shown at the top left corner of the view. */
	Vec2 pan;
	float zoom = 1.f;
};

/** Location of a node in a saved network: graph index and node index (see AnimationNetworkBuilder::GetSaveLayout). */
struct NodeLocation
{
	uint32_t graph = 0;
	uint32_t node = 0;

	bool operator==( const NodeLocation& other ) const
	{
		return graph == other.graph && node == other.node;
	}
	bool operator<( const NodeLocation& other ) const
	{
		return graph != other.graph ? graph < other.graph : node < other.node;
	}
};

/** Path from the root graph to a subgraph: the Subgraph nodes entered on the way, outermost first. */
using GraphPath = std::vector<cmf::AnimationNodeId>;

/** An animation network being edited, with the editor data stored alongside it (node positions, graph views and
* the preview meshes). */
class Document
{
public:
	/** Version of the editor data format. Version 2 replaced the single "previewMesh" with a "previewMeshes" list. */
	static constexpr int EDITOR_DATA_VERSION = 2;

	explicit Document( cmf::AnimationAssetLoader& loader );

	[[nodiscard]] cmf::AnimationNetworkBuilder& GetNetwork();
	[[nodiscard]] const cmf::AnimationNetworkBuilder& GetNetwork() const;

	/** Returns the graph at the end of a path, or nullptr if the path is no longer valid. */
	[[nodiscard]] cmf::AnimationGraphBuilder* FindGraph( const GraphPath& path );
	[[nodiscard]] const cmf::AnimationGraphBuilder* FindGraph( const GraphPath& path ) const;

	/** Returns the position of a node, if it has one. */
	[[nodiscard]] std::optional<Vec2> GetNodePosition( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	void SetNodePosition( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, Vec2 position );

	/** View of a graph. View changes are stored in the file but don't count as modifications. */
	[[nodiscard]] GraphView GetView( const cmf::AnimationGraphBuilder& graph ) const;
	void SetView( const cmf::AnimationGraphBuilder& graph, const GraphView& view );

	/** References of the meshes shown together in the preview, for example a body and a head saved in separate files. */
	[[nodiscard]] const std::vector<std::string>& GetPreviewMeshes() const;
	/** Replaces the preview meshes. Empty and repeated references are dropped. */
	void SetPreviewMeshes( std::vector<std::string> references );
	/** Adds a preview mesh, unless the reference is empty or already in the list. */
	void AddPreviewMesh( std::string reference );

	/** Places nodes without a position, using an automatic layout of the graph. */
	void PlaceUnpositionedNodes( const cmf::AnimationGraphBuilder& graph );
	/** Lays out all nodes of a graph automatically. */
	void AutoLayout( const cmf::AnimationGraphBuilder& graph );

	/** Returns the network in the CAN format with the editor data. */
	[[nodiscard]] std::vector<uint8_t> Serialize();
	/** Replaces the document with a CAN file. Nodes without editor data are laid out automatically.
	* Returns false and leaves the document unchanged if the file is malformed. */
	bool Deserialize( const std::vector<uint8_t>& file, std::string& error );

	/** Replaces the document with an empty network: a root graph with an Output node. */
	void New();
	bool LoadFile( const std::filesystem::path& path, std::string& error );
	bool SaveFile( const std::filesystem::path& path, std::string& error );
	[[nodiscard]] const std::filesystem::path& GetFilePath() const;

	/** Returns true if the network or the editor data changed since the document was loaded or saved. */
	[[nodiscard]] bool IsModified() const;
	/** Treats the current state as saved. */
	void MarkUnmodified();
	/** A value that changes whenever the network or the editor data (except views) changes. */
	[[nodiscard]] uint64_t GetRevision() const;

	/** Returns true if the skeleton is missing, so that the network can only be fixed by setting a skeleton. */
	[[nodiscard]] bool IsLocked() const;

	/** Returns the location of a node in the saved file, or nullopt if the node or graph doesn't exist. */
	[[nodiscard]] std::optional<NodeLocation> Locate( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	/** Returns the graph and node at a location, or {nullptr, unset} if there is none. */
	[[nodiscard]] std::pair<cmf::AnimationGraphBuilder*, cmf::AnimationNodeId> Find( const NodeLocation& location );
	/** Converts a graph path to locations of its Subgraph nodes, and back. Invalid parts are dropped. */
	[[nodiscard]] std::vector<NodeLocation> LocatePath( const GraphPath& path ) const;
	[[nodiscard]] GraphPath FindPath( const std::vector<NodeLocation>& locations );

private:
	struct GraphState
	{
		std::map<uint32_t, Vec2> positions;
		GraphView view;
	};

	[[nodiscard]] std::vector<uint8_t> WriteEditorData() const;
	void ReadEditorData( const std::vector<uint8_t>& data );
	void PruneGraphs();

	cmf::AnimationNetworkBuilder m_network;
	std::map<const cmf::AnimationGraphBuilder*, GraphState> m_graphs;
	std::vector<std::string> m_previewMeshes;
	std::filesystem::path m_filePath;
	uint64_t m_editorRevision = 0;
	uint64_t m_savedRevision = 0;
};

}
