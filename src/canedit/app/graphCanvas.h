// Copyright © 2026 CCP ehf.

#pragma once

#include "appContext.h"

#include <imgui.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace canedit
{

/** Interactive view of the open graph: draws nodes and edges and handles selecting, moving, connecting, zooming
* and panning. */
class GraphCanvas
{
public:
	explicit GraphCanvas( AppContext& context );

	/** Draws the canvas into the remaining space of the current window. */
	void Draw();

	/** Fits the view to all nodes, or to the selected nodes. */
	void FrameNodes( bool selectionOnly );
	/** Graph position at the center of the view, for example to place new nodes. */
	[[nodiscard]] Vec2 GetViewCenter() const;
	/** Returns true while a mouse interaction (drag, connection, box selection) is in progress. */
	[[nodiscard]] bool IsInteracting() const;

	/** Pastes the clipboard at the mouse position, or at the view center if the mouse is outside the canvas. */
	void PasteFromClipboard();
	void CopyToClipboard( bool cut );

private:
	struct Socket
	{
		cmf::AnimationSocketRef ref;
		bool output = false;
	};

	/** Screen space geometry of a node. */
	struct NodeGeometry
	{
		cmf::AnimationNodeId id;
		ImVec2 min;
		ImVec2 max;
		std::vector<ImVec2> inputs;
		std::vector<ImVec2> outputs;
	};

	enum class Mode : uint8_t
	{
		None,
		Panning,
		MovingNodes,
		BoxSelecting,
		Connecting,
	};

	[[nodiscard]] ImVec2 ToScreen( Vec2 position ) const;
	[[nodiscard]] Vec2 ToGraph( ImVec2 position ) const;
	[[nodiscard]] static float GetNodeHeight( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

	void UpdateGeometry( cmf::AnimationGraphBuilder& graph );
	[[nodiscard]] std::optional<Socket> FindSocket( ImVec2 position ) const;
	[[nodiscard]] const NodeGeometry* FindNode( ImVec2 position ) const;
	[[nodiscard]] std::optional<cmf::AnimationSocketRef> FindEdge( ImVec2 position ) const;
	[[nodiscard]] ImVec2 GetSocketPosition( const Socket& socket ) const;

	void HandleInput( cmf::AnimationGraphBuilder& graph, bool hovered );
	void UpdateHover( ImVec2 mouse, bool hovered );
	/** Starts an interaction from mouse presses while idle. Returns false if the graph changed and input handling
	* must stop for this frame. */
	bool HandleIdleInput( cmf::AnimationGraphBuilder& graph, ImVec2 mouse );
	void HandleRightButton( ImVec2 mouse );
	void HandleLeftPress( cmf::AnimationGraphBuilder& graph );
	void HandleRelease( cmf::AnimationGraphBuilder& graph );
	void FinishBoxSelection( ImVec2 mouse );
	void FinishConnection( ImVec2 mouse );
	void HandleShortcuts();
	void HandleDrop( cmf::AnimationGraphBuilder& graph );
	void ZoomAt( ImVec2 screenPosition, float factor );

	void DrawGrid( ImDrawList& drawList ) const;
	void DrawEdges( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph ) const;
	void DrawNodes( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph ) const;
	void DrawNode( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, const NodeGeometry& node ) const;
	void DrawNodeSockets( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, const NodeGeometry& node, bool drawText ) const;
	static void DrawNodeIssues( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, const NodeGeometry& node, float zoom, bool drawText );
	void DrawInteraction( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph ) const;
	void DrawContextMenus( cmf::AnimationGraphBuilder& graph );
	void DrawCanvasMenu( cmf::AnimationGraphBuilder& graph );
	void DrawNodeMenu( const cmf::AnimationGraphBuilder& graph );
	void DrawConnectMenu( const cmf::AnimationGraphBuilder& graph );
	void DrawNodeTooltip( const cmf::AnimationGraphBuilder& graph ) const;
	/** Lists node types that can be added; returns the chosen one. */
	std::optional<cmf::AnimationNodeType> DrawNodeTypeMenu( const cmf::AnimationGraphBuilder& graph, std::optional<cmf::AnimationSocketType> compatibleWith, bool outputSide );
	void AddConnectedNode( cmf::AnimationNodeType type );

	AppContext* m_context;

	// View of the open graph
	const cmf::AnimationGraphBuilder* m_viewGraph = nullptr;
	GraphView m_view;
	ImVec2 m_origin;
	ImVec2 m_size;
	std::set<const cmf::AnimationGraphBuilder*> m_framedGraphs;
	bool m_frameRequest = false;
	bool m_frameSelectionOnly = false;

	std::vector<NodeGeometry> m_nodes;
	std::map<uint32_t, size_t> m_nodeIndices;

	// Interaction
	Mode m_mode = Mode::None;
	ImVec2 m_pressPosition;
	Socket m_connectStart{};
	std::optional<Socket> m_hoveredSocket;
	std::optional<cmf::AnimationNodeId> m_hoveredNode;
	std::optional<cmf::AnimationSocketRef> m_hoveredEdge;
	bool m_editStarted = false;
	bool m_moved = false;

	// Context menus
	std::optional<cmf::AnimationNodeId> m_contextNode;
	Vec2 m_contextPosition{};
	std::optional<Socket> m_pendingConnection;
	std::string m_menuFilter;
};

}
