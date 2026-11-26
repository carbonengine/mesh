// Copyright © 2026 CCP ehf.

#pragma once

#include "pathResolver.h"

#include <cmf/animationnetworkbuilder.h>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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

/** Location of a node in a network: graph index and node index (see AnimationNetworkBuilder::GetSaveLayout). */
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

/** Value of GraphPathStep::state for steps that don't enter a state. */
constexpr uint32_t NO_STATE = 0xffffffff;

/** A step of a GraphPath: a Subgraph node, a StateMachine node or a state of a StateMachine node. */
struct GraphPathStep
{
	/** Subgraph or StateMachine node in the graph of the previous step. */
	cmf::AnimationNodeId node;
	/** For a StateMachine node, the entered state; NO_STATE opens the state machine view of the node. */
	uint32_t state = NO_STATE;

	bool operator==( const GraphPathStep& other ) const
	{
		return node == other.node && state == other.state;
	}
	bool operator!=( const GraphPathStep& other ) const
	{
		return !( *this == other );
	}
};

/** Path from the root graph to what the editor shows, outermost first. Each step enters the graph of a Subgraph node
* or a state of a StateMachine node. A final step with NO_STATE on a StateMachine node shows its states and
* transitions instead of a graph. */
using GraphPath = std::vector<GraphPathStep>;

/** Location of a GraphPathStep that stays valid when the document is read again (see NodeLocation). */
struct PathLocation
{
	NodeLocation node;
	uint32_t state = NO_STATE;

	bool operator==( const PathLocation& other ) const
	{
		return node == other.node && state == other.state;
	}
};


/** An animation network being edited, with the editor state stored alongside it (node positions, graph views,
* preview meshes and the export path).
*
* Documents are stored as JSON (.cane files) and exported to the CAN format, which is what the runtime loads.
* Embedded assets are stored as references; their files are read when the document is loaded and again when it is
* exported, so that the exported network contains their current contents.
*/
class Document
{
public:
	/** Extension of document files. */
	static constexpr const char* FILE_EXTENSION = ".cane";
	/** Value of the "format" field of document files. */
	static constexpr const char* FORMAT_NAME = "cane";
	/** Version of the document format. Files of newer versions are rejected. */
	static constexpr int FILE_VERSION = 1;

	/** The loader and the resolver must outlive the document. Embedded assets are read through the resolver. */
	Document( cmf::AnimationAssetLoader& loader, const PathResolver& resolver );

	[[nodiscard]] cmf::AnimationNetworkBuilder& GetNetwork();
	[[nodiscard]] const cmf::AnimationNetworkBuilder& GetNetwork() const;

	/** Returns the graph at the end of a path, or nullptr if the path is no longer valid. For a path that ends in a
	* state machine view, the graph containing the StateMachine node. */
	[[nodiscard]] cmf::AnimationGraphBuilder* FindGraph( const GraphPath& path );
	[[nodiscard]] const cmf::AnimationGraphBuilder* FindGraph( const GraphPath& path ) const;
	/** Returns the StateMachine node whose view a valid path ends in, or an unset handle. */
	[[nodiscard]] cmf::AnimationNodeId FindStateMachine( const GraphPath& path ) const;

	/** Returns the position of a node, if it has one. */
	[[nodiscard]] std::optional<Vec2> GetNodePosition( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	void SetNodePosition( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, Vec2 position );

	/** View of a graph. View changes are stored in the file but don't count as modifications. */
	[[nodiscard]] GraphView GetView( const cmf::AnimationGraphBuilder& graph ) const;
	void SetView( const cmf::AnimationGraphBuilder& graph, const GraphView& view );

	/** Position of a state in the view of its state machine, identified by the graph of the state. */
	[[nodiscard]] std::optional<Vec2> GetStatePosition( const cmf::AnimationGraphBuilder& stateGraph ) const;
	void SetStatePosition( const cmf::AnimationGraphBuilder& stateGraph, Vec2 position );
	/** View of the states of a StateMachine node. Like graph views, changes don't count as modifications. */
	[[nodiscard]] GraphView GetStateMachineView( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	void SetStateMachineView( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const GraphView& view );
	/** Places the states of a StateMachine node that have no position in a grid next to the existing ones. */
	void PlaceUnpositionedStates( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );
	/** Places all states of a StateMachine node in a grid. */
	void AutoLayoutStates( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

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

	// Embedded assets

	/** References of the embedded assets, including the ones whose file couldn't be read. */
	[[nodiscard]] const std::set<std::string>& GetEmbeddedAssets() const;
	/** Embeds the file a reference resolves to. Fails, without adding the reference, if the file can't be read or is
	* not a valid CMF file. */
	cmf::ValidationResult EmbedAsset( const std::string& reference );
	/** Stops embedding an asset; references to it go through the asset loader afterwards. */
	void RemoveEmbeddedAsset( const std::string& reference );
	/** Returns why the file of an embedded asset couldn't be read, or nullptr if it was read. */
	[[nodiscard]] const std::string* GetEmbeddedAssetError( const std::string& reference ) const;
	/** Forgets the contents of embedded files read so far, so that they are read again when the document is
	* deserialized next time. Until then, deserializing (for example undo) reuses the contents read before. */
	void ForgetEmbeddedFiles();

	// Files

	/** Returns the document as JSON text. The export path is stored relative to baseDirectory if it is set. */
	[[nodiscard]] std::string Serialize( const std::filesystem::path& baseDirectory = {} ) const;
	/** Replaces the document with JSON text written by Serialize. Relative export paths are resolved against
	* baseDirectory. Fails, leaving the document unchanged, if the text is not a document of a supported version or
	* has the wrong structure. Parts that can't be applied, for example an edge between incompatible sockets or an
	* embedded asset whose file is missing, are skipped and reported as warnings. */
	bool Deserialize( std::string_view text, std::string& error, std::vector<std::string>* warnings = nullptr, const std::filesystem::path& baseDirectory = {} );

	/** Replaces the document with an empty network: a root graph with an Output node. */
	void New();
	/** Loads a document file. A document that loaded with warnings is marked modified. */
	bool LoadFile( const std::filesystem::path& path, std::string& error, std::vector<std::string>* warnings = nullptr );
	bool SaveFile( const std::filesystem::path& path, std::string& error );
	[[nodiscard]] const std::filesystem::path& GetFilePath() const;


	/** Path the document was last exported to, or empty. Stored in the document; setting it is a modification. */
	[[nodiscard]] const std::filesystem::path& GetExportPath() const;
	void SetExportPath( const std::filesystem::path& path );
	/** Writes the network in the CAN format. Embedded files are read again. Fails if an embedded file can't be read,
	* the network has errors (assets that are still loading are not errors) or a skeleton or animation referenced from
	* an embedded file is missing. Doesn't change the document. */
	bool Export( const std::filesystem::path& path, std::string& error ) const;

	/** Returns true if the network or the editor state changed since the document was loaded or saved. */
	[[nodiscard]] bool IsModified() const;
	/** Treats the current state as saved. */
	void MarkUnmodified();
	/** A value that changes whenever the network or the editor state (except views) changes. */
	[[nodiscard]] uint64_t GetRevision() const;

	/** Returns true if the skeleton is missing, so that the network can only be fixed by setting a skeleton. */
	[[nodiscard]] bool IsLocked() const;

	/** Returns the location of a node, or nullopt if the node or graph doesn't exist. */
	[[nodiscard]] std::optional<NodeLocation> Locate( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	/** Returns the graph and node at a location, or {nullptr, unset} if there is none. */
	[[nodiscard]] std::pair<cmf::AnimationGraphBuilder*, cmf::AnimationNodeId> Find( const NodeLocation& location );
	/** Converts a graph path to locations, and back. Invalid parts are dropped. */
	[[nodiscard]] std::vector<PathLocation> LocatePath( const GraphPath& path ) const;
	[[nodiscard]] GraphPath FindPath( const std::vector<PathLocation>& locations );

private:
	struct GraphState
	{
		std::map<uint32_t, Vec2> positions;
		GraphView view;
		/** Views of the StateMachine nodes of the graph, by node slot index. */
		std::map<uint32_t, GraphView> stateMachineViews;
	};

	/** Clears the network and the editor state, except the file path. */
	void Reset();
	/** Reads the document; throws nlohmann::json::exception for JSON of the wrong structure. */
	void Read( std::string_view text, std::vector<std::string>& warnings, const std::filesystem::path& baseDirectory );
	/** Embeds an asset from the contents read before or from its file; records an error on failure. */
	bool LoadEmbeddedAsset( const std::string& reference );
	void PlaceAllUnpositionedNodes();

	cmf::AnimationAssetLoader* m_loader;
	const PathResolver* m_resolver;
	cmf::AnimationNetworkBuilder m_network;
	std::map<const cmf::AnimationGraphBuilder*, GraphState> m_graphs;
	/** Positions of the states in their state machine view, by state graph. */
	std::map<const cmf::AnimationGraphBuilder*, Vec2> m_statePositions;
	std::vector<std::string> m_previewMeshes;
	std::set<std::string> m_embeddedAssets;
	std::map<std::string, std::string> m_embeddedErrors;
	/** Contents of embedded files read so far. */
	std::map<std::string, std::vector<uint8_t>> m_embeddedFiles;
	std::filesystem::path m_exportPath;
	std::filesystem::path m_filePath;
	uint64_t m_editorRevision = 0;
	uint64_t m_savedRevision = 0;
};

}
