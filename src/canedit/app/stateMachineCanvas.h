// Copyright © 2026 CCP ehf.

#pragma once

#include "appContext.h"

#include <imgui.h>

#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace canedit
{

/** Interactive view of the states and transitions of the open state machine: draws states as boxes and transitions as
* arrows, and handles selecting, moving and connecting states, zooming and panning. */
class StateMachineCanvas
{
public:
	explicit StateMachineCanvas( AppContext& context );

	/** Draws the canvas into the remaining space of the current window. The session must show a state machine. */
	void Draw();

	/** Fits the view to all states, or to the selected states. */
	void FrameStates( bool selectionOnly );
	/** Returns true while a mouse interaction (drag, connection, box selection) is in progress. */
	[[nodiscard]] bool IsInteracting() const;

private:
	/** Screen space geometry of a state. */
	struct StateGeometry
	{
		ImVec2 min;
		ImVec2 max;
	};

	/** Screen space path of a transition arrow: a straight segment, or a loop for transitions to the same state. */
	struct TransitionGeometry
	{
		uint32_t index = 0;
		bool loop = false;
		ImVec2 from;
		ImVec2 to;
		/** Control points of the loop curve. */
		ImVec2 control1;
		ImVec2 control2;
	};

	enum class Mode : uint8_t
	{
		None,
		Panning,
		MovingStates,
		BoxSelecting,
		Connecting,
	};

	[[nodiscard]] ImVec2 ToScreen( Vec2 position ) const;
	[[nodiscard]] Vec2 ToGraph( ImVec2 position ) const;

	void UpdateGeometry( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );
	[[nodiscard]] std::optional<uint32_t> FindState( ImVec2 position ) const;
	/** Returns true if a position is on the border of a state, where dragging starts a transition. */
	[[nodiscard]] bool IsOnBorder( uint32_t state, ImVec2 position ) const;
	[[nodiscard]] std::optional<uint32_t> FindTransition( ImVec2 position ) const;
	void ZoomAt( ImVec2 screenPosition, float factor );
	void ApplyFrameRequest( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

	void HandleInput( bool hovered );
	void UpdateHover( ImVec2 mouse, bool hovered );
	/** Starts an interaction from mouse presses while idle. Returns false if the view changed and input handling must
	* stop for this frame. */
	bool HandleIdleInput( ImVec2 mouse );
	void HandleRightButton( ImVec2 mouse );
	void HandleLeftPress();
	void HandleRelease();
	void FinishBoxSelection( ImVec2 mouse );
	void HandleShortcuts();

	void DrawGrid( ImDrawList& drawList ) const;
	void DrawTransitions( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	void DrawStates( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	void DrawInteraction( ImDrawList& drawList ) const;
	void DrawTooltip( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const;
	void DrawContextMenus( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );
	void DrawCanvasMenu();
	void DrawStateMenu( const cmf::AnimationStateMachineSettings& settings );
	void DrawTransitionMenu();

	AppContext* m_context;

	// View of the open state machine, identified by its graph and node
	std::pair<const cmf::AnimationGraphBuilder*, uint32_t> m_viewKey{ nullptr, 0 };
	GraphView m_view;
	ImVec2 m_origin;
	ImVec2 m_size;
	std::set<std::pair<const cmf::AnimationGraphBuilder*, uint32_t>> m_framedViews;
	bool m_frameRequest = false;
	bool m_frameSelectionOnly = false;

	std::vector<StateGeometry> m_states;
	std::vector<TransitionGeometry> m_transitions;

	// Interaction
	Mode m_mode = Mode::None;
	ImVec2 m_pressPosition;
	uint32_t m_connectStart = 0;
	std::optional<uint32_t> m_hoveredState;
	std::optional<uint32_t> m_hoveredTransition;
	bool m_editStarted = false;

	// Context menus
	std::optional<uint32_t> m_contextState;
	Vec2 m_contextPosition{};
};

}
