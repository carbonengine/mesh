// Copyright © 2026 CCP ehf.

#include "stateMachineCanvas.h"

#include "nodeInfo.h"
#include "preview.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>

// ImGui uses C-style variadic functions for formatted text
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace canedit
{
namespace
{

// State geometry in graph units
constexpr float STATE_WIDTH = 180.f;
constexpr float STATE_HEIGHT = 56.f;
constexpr float STATE_ROUNDING = 10.f;
/** Width of the band along the border of a state where dragging starts a transition. */
constexpr float CONNECT_BORDER = 9.f;
constexpr float GRID_SPACING = 40.f;
/** Distance between transitions that connect the same two states. */
constexpr float PAIR_SPACING = 16.f;
/** Height of a loop of a transition to the same state above the state, and the step between loops. */
constexpr float LOOP_HEIGHT = 34.f;
constexpr float LOOP_STEP = 16.f;
/** Horizontal spread of a loop, relative to the state width. */
constexpr float LOOP_SPREAD = 0.22f;
constexpr float LOOP_BULGE = 14.f;
constexpr float ARROW_SIZE = 10.f;
/** Length of the arrow pointing at the start state. */
constexpr float START_ARROW_LENGTH = 34.f;
constexpr float START_DOT_RADIUS = 4.f;

constexpr float MIN_ZOOM = 0.15f;
constexpr float MAX_ZOOM = 3.f;
constexpr float WHEEL_ZOOM_STEP = 1.15f;
constexpr float FRAME_MARGIN = 80.f;
constexpr float MAX_FRAME_ZOOM = 1.25f;
/** Zoom below which text is not drawn. */
constexpr float MIN_TEXT_ZOOM = 0.35f;
/** Distance in pixels within which a transition is picked. */
constexpr float PICK_DISTANCE = 6.f;
constexpr int LOOP_SEGMENTS = 16;
constexpr float TRANSITION_THICKNESS = 2.5f;
constexpr float SELECTED_TRANSITION_THICKNESS = 4.5f;
constexpr float BORDER_THICKNESS = 1.5f;
constexpr float SELECTED_BORDER_THICKNESS = 3.f;
constexpr float START_BORDER_THICKNESS = 2.5f;
/** Grid spacing in pixels below which the grid is not drawn. */
constexpr float MIN_GRID_SPACING = 8.f;
/** Radius of the problem marker, relative to the state height. */
constexpr float ISSUE_MARKER_RADIUS = 0.14f;
/** Scale of the transition label font relative to the state font. */
constexpr float LABEL_SCALE = 0.85f;
constexpr float LABEL_PADDING = 3.f;
/** Distance of the read-only message from the bottom of the canvas, in text lines. */
constexpr float MESSAGE_LINE_OFFSET = 1.5f;
constexpr float HALF = 0.5f;
constexpr float THIRD = 1.f / 3.f;

constexpr ImU32 BACKGROUND_COLOR = IM_COL32( 30, 30, 34, 255 );
constexpr ImU32 GRID_COLOR = IM_COL32( 50, 50, 56, 255 );
constexpr ImU32 STATE_BODY_COLOR = IM_COL32( 48, 48, 54, 245 );
constexpr ImU32 CURRENT_STATE_COLOR = IM_COL32( 50, 105, 65, 250 );
constexpr ImU32 BORDER_COLOR = IM_COL32( 15, 15, 15, 255 );
constexpr ImU32 START_COLOR = IM_COL32( 120, 200, 120, 255 );
constexpr ImU32 SELECTION_COLOR = IM_COL32( 255, 200, 60, 255 );
constexpr ImU32 TRANSITION_COLOR = IM_COL32( 190, 190, 200, 255 );
constexpr ImU32 HOVER_COLOR = IM_COL32( 230, 230, 240, 255 );
constexpr ImU32 LABEL_BACKGROUND_COLOR = IM_COL32( 30, 30, 34, 200 );
constexpr ImU32 TEXT_COLOR = IM_COL32( 230, 230, 230, 255 );
constexpr ImU32 DIM_TEXT_COLOR = IM_COL32( 170, 170, 170, 255 );
constexpr ImU32 ERROR_COLOR = IM_COL32( 230, 70, 60, 255 );
constexpr ImU32 BOX_FILL_COLOR = IM_COL32( 90, 140, 255, 40 );
constexpr ImU32 BOX_BORDER_COLOR = IM_COL32( 90, 140, 255, 200 );

ImVec2 Add( ImVec2 a, ImVec2 b )
{
	return { a.x + b.x, a.y + b.y };
}

ImVec2 Sub( ImVec2 a, ImVec2 b )
{
	return { a.x - b.x, a.y - b.y };
}

ImVec2 Scale( ImVec2 a, float s )
{
	return { a.x * s, a.y * s };
}

ImVec2 Center( ImVec2 min, ImVec2 max )
{
	return Scale( Add( min, max ), HALF );
}

float Length( ImVec2 a )
{
	return std::sqrt( a.x * a.x + a.y * a.y );
}

ImVec2 Normalize( ImVec2 a )
{
	const auto length = Length( a );
	return length > 0.f ? Scale( a, 1.f / length ) : ImVec2{ 1.f, 0.f };
}

float DistanceToSegment( ImVec2 p, ImVec2 a, ImVec2 b )
{
	const auto ab = Sub( b, a );
	const auto lengthSquared = ab.x * ab.x + ab.y * ab.y;
	float t = 0.f;
	if( lengthSquared > 0.f )
	{
		const auto ap = Sub( p, a );
		t = std::clamp( ( ap.x * ab.x + ap.y * ab.y ) / lengthSquared, 0.f, 1.f );
	}
	return Length( Sub( p, Add( a, Scale( ab, t ) ) ) );
}

bool Contains( ImVec2 min, ImVec2 max, ImVec2 p )
{
	return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

// Returns where the segment from a point inside a box towards another point leaves the box
ImVec2 ClipToBox( ImVec2 inside, ImVec2 toward, ImVec2 min, ImVec2 max )
{
	const auto direction = Sub( toward, inside );
	float t = 1.f;
	if( direction.x > 0.f )
	{
		t = std::min( t, ( max.x - inside.x ) / direction.x );
	}
	else if( direction.x < 0.f )
	{
		t = std::min( t, ( min.x - inside.x ) / direction.x );
	}
	if( direction.y > 0.f )
	{
		t = std::min( t, ( max.y - inside.y ) / direction.y );
	}
	else if( direction.y < 0.f )
	{
		t = std::min( t, ( min.y - inside.y ) / direction.y );
	}
	return Add( inside, Scale( direction, std::max( t, 0.f ) ) );
}

void DrawArrowHead( ImDrawList& drawList, ImVec2 tip, ImVec2 direction, float size, ImU32 color )
{
	const auto forward = Normalize( direction );
	const ImVec2 side = { -forward.y, forward.x };
	const auto base = Sub( tip, Scale( forward, size ) );
	drawList.AddTriangleFilled( tip, Add( base, Scale( side, size * HALF ) ), Sub( base, Scale( side, size * HALF ) ), color );
}

// Draws the arrow pointing at the start state from the left
void DrawStartArrow( ImDrawList& drawList, ImVec2 min, ImVec2 max, float zoom )
{
	const auto centerY = ( min.y + max.y ) * HALF;
	const ImVec2 arrowStart = { min.x - START_ARROW_LENGTH * zoom, centerY };
	drawList.AddCircleFilled( arrowStart, START_DOT_RADIUS * zoom, START_COLOR );
	drawList.AddLine( arrowStart, { min.x - ARROW_SIZE * zoom * HALF, centerY }, START_COLOR, TRANSITION_THICKNESS * std::max( zoom, HALF ) );
	DrawArrowHead( drawList, { min.x, centerY }, { 1.f, 0.f }, ARROW_SIZE * zoom, START_COLOR );
}

// Draws the name of a state centered in its box, with an optional note below it
void DrawStateText( ImDrawList& drawList, ImVec2 min, ImVec2 max, float zoom, const std::string& title, const char* note )
{
	auto* font = ImGui::GetFont();
	const auto fontSize = ImGui::GetFontSize() * zoom;
	const auto center = Center( min, max );
	const ImVec4 clip = { min.x, min.y, max.x, max.y };
	const auto size = font->CalcTextSizeA( fontSize, FLT_MAX, 0.f, title.c_str() );
	const auto titleY = note != nullptr ? center.y - size.y : center.y - size.y * HALF;
	drawList.AddText( font, fontSize, { center.x - size.x * HALF, titleY }, TEXT_COLOR, title.c_str(), nullptr, 0.f, &clip );
	if( note != nullptr )
	{
		const auto noteSize = font->CalcTextSizeA( fontSize, FLT_MAX, 0.f, note );
		drawList.AddText( font, fontSize, { center.x - noteSize.x * HALF, center.y }, DIM_TEXT_COLOR, note, nullptr, 0.f, &clip );
	}
}

// Draws the problem marker in the top right corner of a state
void DrawIssueMarker( ImDrawList& drawList, ImVec2 min, ImVec2 max, float zoom, bool drawText )
{
	const auto radius = STATE_HEIGHT * ISSUE_MARKER_RADIUS * zoom;
	const ImVec2 markerCenter = { max.x - radius * 2.f, min.y + radius * 2.f };
	drawList.AddCircleFilled( markerCenter, radius, ERROR_COLOR );
	if( drawText )
	{
		auto* font = ImGui::GetFont();
		const auto fontSize = ImGui::GetFontSize() * zoom;
		const auto width = font->CalcTextSizeA( fontSize, FLT_MAX, 0.f, "!" ).x;
		drawList.AddText( font, fontSize, { markerCenter.x - width * HALF, markerCenter.y - fontSize * HALF }, IM_COL32_WHITE, "!" );
	}
}

}

StateMachineCanvas::StateMachineCanvas( AppContext& context ) :
	m_context( &context )
{
}

ImVec2 StateMachineCanvas::ToScreen( Vec2 position ) const
{
	return { m_origin.x + ( position.x - m_view.pan.x ) * m_view.zoom, m_origin.y + ( position.y - m_view.pan.y ) * m_view.zoom };
}

Vec2 StateMachineCanvas::ToGraph( ImVec2 position ) const
{
	return { ( position.x - m_origin.x ) / m_view.zoom + m_view.pan.x, ( position.y - m_origin.y ) / m_view.zoom + m_view.pan.y };
}

bool StateMachineCanvas::IsInteracting() const
{
	return m_mode != Mode::None;
}

void StateMachineCanvas::FrameStates( bool selectionOnly )
{
	m_frameRequest = true;
	m_frameSelectionOnly = selectionOnly;
}

void StateMachineCanvas::ZoomAt( ImVec2 screenPosition, float factor )
{
	const auto anchor = ToGraph( screenPosition );
	m_view.zoom = std::clamp( m_view.zoom * factor, MIN_ZOOM, MAX_ZOOM );
	// Keep the position under the mouse in place
	m_view.pan = { anchor.x - ( screenPosition.x - m_origin.x ) / m_view.zoom, anchor.y - ( screenPosition.y - m_origin.y ) / m_view.zoom };
}

void StateMachineCanvas::UpdateGeometry( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	m_states.clear();
	m_transitions.clear();
	const auto* settings = graph.GetSettings<cmf::AnimationStateMachineSettings>( node );
	if( settings == nullptr )
	{
		return;
	}
	const auto& document = *m_context->document;
	const auto zoom = m_view.zoom;
	for( uint32_t state = 0; state < settings->stateNames.size(); ++state )
	{
		const auto position = document.GetStatePosition( *graph.GetStateGraph( node, state ) ).value_or( Vec2{} );
		m_states.push_back( { ToScreen( position ), ToScreen( { position.x + STATE_WIDTH, position.y + STATE_HEIGHT } ) } );
	}

	// Transitions between the same two states are spread apart; loops of a state are stacked
	std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> pairs;
	for( uint32_t i = 0; i < settings->transitions.size(); ++i )
	{
		const auto& transition = settings->transitions[i];
		pairs[{ std::min( transition.fromState, transition.toState ), std::max( transition.fromState, transition.toState ) }].push_back( i );
	}
	for( uint32_t i = 0; i < settings->transitions.size(); ++i )
	{
		const auto& transition = settings->transitions[i];
		if( transition.fromState >= m_states.size() || transition.toState >= m_states.size() )
		{
			continue;
		}
		const auto low = std::min( transition.fromState, transition.toState );
		const auto high = std::max( transition.fromState, transition.toState );
		const auto& group = pairs[{ low, high }];
		const auto slot = static_cast<float>( std::find( group.begin(), group.end(), i ) - group.begin() );
		TransitionGeometry geometry;
		geometry.index = i;
		const auto& from = m_states[transition.fromState];
		if( low == high )
		{
			const auto center = Center( from.min, from.max );
			const auto spread = ( from.max.x - from.min.x ) * LOOP_SPREAD;
			const auto height = ( LOOP_HEIGHT + slot * LOOP_STEP ) * zoom;
			geometry.loop = true;
			geometry.from = { center.x - spread, from.min.y };
			geometry.to = { center.x + spread, from.min.y };
			geometry.control1 = { geometry.from.x - LOOP_BULGE * zoom, from.min.y - height };
			geometry.control2 = { geometry.to.x + LOOP_BULGE * zoom, from.min.y - height };
		}
		else
		{
			const auto& to = m_states[transition.toState];
			const auto lowCenter = Center( m_states[low].min, m_states[low].max );
			const auto highCenter = Center( m_states[high].min, m_states[high].max );
			const auto direction = Normalize( Sub( highCenter, lowCenter ) );
			const ImVec2 side = { -direction.y, direction.x };
			const auto offset = Scale( side, ( slot - static_cast<float>( group.size() - 1 ) * HALF ) * PAIR_SPACING * zoom );
			const auto start = Add( Center( from.min, from.max ), offset );
			const auto end = Add( Center( to.min, to.max ), offset );
			geometry.from = ClipToBox( start, end, from.min, from.max );
			geometry.to = ClipToBox( end, start, to.min, to.max );
		}
		m_transitions.push_back( geometry );
	}
}

std::optional<uint32_t> StateMachineCanvas::FindState( ImVec2 position ) const
{
	// States drawn last are on top
	for( auto i = static_cast<uint32_t>( m_states.size() ); i-- > 0; )
	{
		if( Contains( m_states[i].min, m_states[i].max, position ) )
		{
			return i;
		}
	}
	return std::nullopt;
}

bool StateMachineCanvas::IsOnBorder( uint32_t state, ImVec2 position ) const
{
	const auto border = CONNECT_BORDER * m_view.zoom;
	const auto& geometry = m_states[state];
	return Contains( geometry.min, geometry.max, position ) && !Contains( { geometry.min.x + border, geometry.min.y + border }, { geometry.max.x - border, geometry.max.y - border }, position );
}

std::optional<uint32_t> StateMachineCanvas::FindTransition( ImVec2 position ) const
{
	for( const auto& transition : m_transitions )
	{
		if( !transition.loop )
		{
			if( DistanceToSegment( position, transition.from, transition.to ) <= PICK_DISTANCE )
			{
				return transition.index;
			}
			continue;
		}
		ImVec2 previous = transition.from;
		for( int i = 1; i <= LOOP_SEGMENTS; ++i )
		{
			const auto point = ImBezierCubicCalc( transition.from, transition.control1, transition.control2, transition.to, static_cast<float>( i ) / static_cast<float>( LOOP_SEGMENTS ) );
			if( DistanceToSegment( position, previous, point ) <= PICK_DISTANCE )
			{
				return transition.index;
			}
			previous = point;
		}
	}
	return std::nullopt;
}

void StateMachineCanvas::ApplyFrameRequest( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	if( !m_frameRequest )
	{
		return;
	}
	m_frameRequest = false;
	const auto& session = *m_context->session;
	const auto& document = *m_context->document;
	const auto& settings = *graph.GetSettings<cmf::AnimationStateMachineSettings>( node );
	Vec2 low{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	Vec2 high{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
	for( uint32_t state = 0; state < settings.stateNames.size(); ++state )
	{
		if( m_frameSelectionOnly && !session.GetSelectedStates().empty() && !session.IsStateSelected( state ) )
		{
			continue;
		}
		const auto position = document.GetStatePosition( *graph.GetStateGraph( node, state ) ).value_or( Vec2{} );
		low = { std::min( low.x, position.x ), std::min( low.y, position.y ) };
		high = { std::max( high.x, position.x + STATE_WIDTH ), std::max( high.y, position.y + STATE_HEIGHT ) };
	}
	if( low.x > high.x )
	{
		return;
	}
	const auto width = high.x - low.x + 2.f * FRAME_MARGIN;
	const auto height = high.y - low.y + 2.f * FRAME_MARGIN;
	m_view.zoom = std::clamp( std::min( m_size.x / width, m_size.y / height ), MIN_ZOOM, MAX_FRAME_ZOOM );
	const Vec2 center = { ( low.x + high.x ) * HALF, ( low.y + high.y ) * HALF };
	m_view.pan = { center.x - m_size.x * HALF / m_view.zoom, center.y - m_size.y * HALF / m_view.zoom };
}

void StateMachineCanvas::Draw()
{
	auto& session = *m_context->session;
	auto& document = *m_context->document;
	const auto node = session.GetStateMachine();
	if( !node.IsSet() )
	{
		return;
	}
	const auto& graph = session.GetGraph();
	const std::pair<const cmf::AnimationGraphBuilder*, uint32_t> key = { &graph, node.index };
	if( key != m_viewKey )
	{
		m_viewKey = key;
		m_view = document.GetStateMachineView( graph, node );
		m_mode = Mode::None;
		if( m_framedViews.insert( key ).second && m_view.pan == Vec2{} && m_view.zoom == 1.f )
		{
			FrameStates( false );
		}
	}
	const auto previousView = m_view;

	m_origin = ImGui::GetCursorScreenPos();
	m_size = ImGui::GetContentRegionAvail();
	m_size.x = std::max( m_size.x, 1.f );
	m_size.y = std::max( m_size.y, 1.f );
	ApplyFrameRequest( graph, node );

	ImGui::InvisibleButton( "stateCanvas", m_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle );
	const bool hovered = ImGui::IsItemHovered();

	UpdateGeometry( graph, node );
	HandleInput( hovered );
	// Edits may have changed the state machine, or left it
	const auto currentNode = session.GetStateMachine();
	if( !currentNode.IsSet() )
	{
		return;
	}
	const auto& currentGraph = session.GetGraph();
	UpdateGeometry( currentGraph, currentNode );

	auto& drawList = *ImGui::GetWindowDrawList();
	drawList.PushClipRect( m_origin, Add( m_origin, m_size ), true );
	DrawGrid( drawList );
	DrawTransitions( drawList, currentGraph, currentNode );
	DrawStates( drawList, currentGraph, currentNode );
	DrawInteraction( drawList );
	const char* message = nullptr;
	if( !session.CanEdit() )
	{
		message = "The skeleton is missing: set a skeleton in the network properties to edit the network.";
	}
	else if( m_states.empty() )
	{
		message = "Right-click to add a state.";
	}
	if( message != nullptr )
	{
		drawList.AddText( Add( m_origin, { ImGui::GetStyle().WindowPadding.x, m_size.y - ImGui::GetTextLineHeightWithSpacing() * MESSAGE_LINE_OFFSET } ), session.CanEdit() ? DIM_TEXT_COLOR : ERROR_COLOR, message );
	}
	drawList.PopClipRect();

	if( hovered && m_mode == Mode::None )
	{
		DrawTooltip( currentGraph, currentNode );
	}
	DrawContextMenus( currentGraph, currentNode );
	HandleShortcuts();

	if( session.GetStateMachine().IsSet() && ( m_view.pan != previousView.pan || m_view.zoom != previousView.zoom ) )
	{
		document.SetStateMachineView( session.GetGraph(), session.GetStateMachine(), m_view );
	}
}

void StateMachineCanvas::HandleInput( bool hovered )
{
	const auto& io = ImGui::GetIO();
	const auto mouse = io.MousePos;
	UpdateHover( mouse, hovered );

	if( hovered && io.MouseWheel != 0.f )
	{
		ZoomAt( mouse, std::pow( WHEEL_ZOOM_STEP, io.MouseWheel ) );
	}
	if( m_mode == Mode::None && hovered && !HandleIdleInput( mouse ) )
	{
		return;
	}

	switch( m_mode )
	{
	case Mode::Panning:
		m_view.pan = { m_view.pan.x - io.MouseDelta.x / m_view.zoom, m_view.pan.y - io.MouseDelta.y / m_view.zoom };
		if( !ImGui::IsMouseDown( ImGuiMouseButton_Middle ) && !ImGui::IsMouseDown( ImGuiMouseButton_Left ) && !ImGui::IsMouseDown( ImGuiMouseButton_Right ) )
		{
			m_mode = Mode::None;
		}
		break;
	case Mode::MovingStates:
		if( io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f )
		{
			m_context->session->MoveStates( m_context->session->GetSelectedStates(), { io.MouseDelta.x / m_view.zoom, io.MouseDelta.y / m_view.zoom } );
		}
		break;
	case Mode::BoxSelecting:
	case Mode::Connecting:
	case Mode::None:
		break;
	}

	if( m_mode != Mode::None && m_mode != Mode::Panning && !ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
	{
		HandleRelease();
	}
}

void StateMachineCanvas::UpdateHover( ImVec2 mouse, bool hovered )
{
	m_hoveredState.reset();
	m_hoveredTransition.reset();
	if( !hovered && m_mode != Mode::Connecting )
	{
		return;
	}
	m_hoveredState = FindState( mouse );
	if( !m_hoveredState && m_mode == Mode::None )
	{
		m_hoveredTransition = FindTransition( mouse );
	}
	if( m_mode == Mode::None && m_hoveredState && m_context->session->CanEdit() && IsOnBorder( *m_hoveredState, mouse ) )
	{
		ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
	}
}

bool StateMachineCanvas::HandleIdleInput( ImVec2 mouse )
{
	if( ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) || ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && ImGui::IsKeyDown( ImGuiKey_Space ) ) )
	{
		m_mode = Mode::Panning;
	}
	else if( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && m_hoveredState )
	{
		m_context->session->EnterState( *m_hoveredState );
		return false;
	}
	else if( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
	{
		HandleLeftPress();
	}
	else
	{
		HandleRightButton( mouse );
	}
	return true;
}

void StateMachineCanvas::HandleRightButton( ImVec2 mouse )
{
	auto& session = *m_context->session;
	if( ImGui::IsMouseReleased( ImGuiMouseButton_Right ) && ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ).x == 0.f && ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ).y == 0.f )
	{
		m_contextPosition = ToGraph( mouse );
		if( m_hoveredState )
		{
			if( !session.IsStateSelected( *m_hoveredState ) )
			{
				session.SelectState( *m_hoveredState );
			}
			m_contextState = m_hoveredState;
			ImGui::OpenPopup( "StateMenu" );
		}
		else if( m_hoveredTransition )
		{
			session.SelectTransition( *m_hoveredTransition );
			ImGui::OpenPopup( "TransitionMenu" );
		}
		else
		{
			ImGui::OpenPopup( "StateCanvasMenu" );
		}
	}
	else if( ImGui::IsMouseDragging( ImGuiMouseButton_Right ) )
	{
		m_mode = Mode::Panning;
	}
}

void StateMachineCanvas::HandleLeftPress()
{
	auto& session = *m_context->session;
	const bool canEdit = session.CanEdit();
	const bool additive = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
	m_pressPosition = ImGui::GetMousePos();
	m_editStarted = false;
	if( m_hoveredState && canEdit && !additive && IsOnBorder( *m_hoveredState, m_pressPosition ) )
	{
		m_connectStart = *m_hoveredState;
		m_mode = Mode::Connecting;
	}
	else if( m_hoveredState )
	{
		if( additive )
		{
			if( session.IsStateSelected( *m_hoveredState ) )
			{
				session.DeselectState( *m_hoveredState );
			}
			else
			{
				session.SelectState( *m_hoveredState, true );
			}
			return;
		}
		if( !session.IsStateSelected( *m_hoveredState ) )
		{
			session.SelectState( *m_hoveredState );
		}
		if( canEdit )
		{
			session.BeginEdit();
			m_editStarted = true;
			m_mode = Mode::MovingStates;
		}
	}
	else if( m_hoveredTransition )
	{
		session.SelectTransition( *m_hoveredTransition );
	}
	else
	{
		if( !additive )
		{
			session.ClearSelection();
		}
		m_mode = Mode::BoxSelecting;
	}
}

void StateMachineCanvas::HandleRelease()
{
	auto& session = *m_context->session;
	const auto mouse = ImGui::GetMousePos();
	if( m_mode == Mode::BoxSelecting )
	{
		FinishBoxSelection( mouse );
	}
	else if( m_mode == Mode::Connecting )
	{
		// Transitions to the same state are added from the context menu, so that a click on the border doesn't add one
		const auto target = FindState( mouse );
		if( target && *target != m_connectStart && session.AddTransition( m_connectStart, *target ) == NO_STATE )
		{
			m_context->SetStatus( "Can't add the transition", true );
		}
	}
	if( m_editStarted )
	{
		if( session.IsEditing() )
		{
			session.EndEdit();
		}
		m_editStarted = false;
	}
	m_mode = Mode::None;
}

void StateMachineCanvas::FinishBoxSelection( ImVec2 mouse )
{
	auto& session = *m_context->session;
	const ImVec2 low = { std::min( m_pressPosition.x, mouse.x ), std::min( m_pressPosition.y, mouse.y ) };
	const ImVec2 high = { std::max( m_pressPosition.x, mouse.x ), std::max( m_pressPosition.y, mouse.y ) };
	auto selection = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift ? session.GetSelectedStates() : std::vector<uint32_t>();
	for( uint32_t state = 0; state < m_states.size(); ++state )
	{
		const auto& geometry = m_states[state];
		if( geometry.max.x >= low.x && geometry.min.x <= high.x && geometry.max.y >= low.y && geometry.min.y <= high.y && std::find( selection.begin(), selection.end(), state ) == selection.end() )
		{
			selection.push_back( state );
		}
	}
	session.SetSelectedStates( std::move( selection ) );
}

void StateMachineCanvas::HandleShortcuts()
{
	if( !ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) || ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() )
	{
		return;
	}
	auto& session = *m_context->session;
	if( ImGui::IsKeyPressed( ImGuiKey_Delete ) )
	{
		session.DeleteSelection();
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_A ) )
	{
		session.SelectAll();
	}
	if( ImGui::IsKeyPressed( ImGuiKey_F, false ) && !ImGui::GetIO().KeyCtrl )
	{
		FrameStates( true );
	}
	if( ImGui::IsKeyPressed( ImGuiKey_Home, false ) )
	{
		FrameStates( false );
	}
	if( ImGui::IsKeyPressed( ImGuiKey_Backspace, false ) )
	{
		session.LeaveSubgraph();
	}
	else if( ImGui::IsKeyPressed( ImGuiKey_Enter, false ) && session.GetSelectedStates().size() == 1 )
	{
		session.EnterState( session.GetSelectedStates().front() );
	}
}

void StateMachineCanvas::DrawGrid( ImDrawList& drawList ) const
{
	drawList.AddRectFilled( m_origin, Add( m_origin, m_size ), BACKGROUND_COLOR );
	const auto spacing = GRID_SPACING * m_view.zoom;
	if( spacing < MIN_GRID_SPACING )
	{
		return;
	}
	const auto startX = std::fmod( -m_view.pan.x * m_view.zoom, spacing );
	const auto startY = std::fmod( -m_view.pan.y * m_view.zoom, spacing );
	for( int i = 0; startX + static_cast<float>( i ) * spacing < m_size.x; ++i )
	{
		const auto x = startX + static_cast<float>( i ) * spacing;
		drawList.AddLine( { m_origin.x + x, m_origin.y }, { m_origin.x + x, m_origin.y + m_size.y }, GRID_COLOR );
	}
	for( int i = 0; startY + static_cast<float>( i ) * spacing < m_size.y; ++i )
	{
		const auto y = startY + static_cast<float>( i ) * spacing;
		drawList.AddLine( { m_origin.x, m_origin.y + y }, { m_origin.x + m_size.x, m_origin.y + y }, GRID_COLOR );
	}
}

void StateMachineCanvas::DrawTransitions( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const
{
	const auto& settings = *graph.GetSettings<cmf::AnimationStateMachineSettings>( node );
	const auto& selected = m_context->session->GetSelectedTransition();
	const auto zoom = m_view.zoom;
	const bool drawText = zoom >= MIN_TEXT_ZOOM;
	auto* font = ImGui::GetFont();
	const auto fontSize = ImGui::GetFontSize() * zoom * LABEL_SCALE;
	// Problems of unused StateMachine nodes are not reported
	const bool used = m_context->document->IsNodeUsed( graph, node );
	for( const auto& geometry : m_transitions )
	{
		const auto& transition = settings.transitions[geometry.index];
		const bool isSelected = selected && *selected == geometry.index;
		const bool isHovered = m_hoveredTransition && *m_hoveredTransition == geometry.index;
		const bool hasError = used && DiagnoseTransition( graph, node, transition ).has_value();
		auto color = hasError ? ERROR_COLOR : TRANSITION_COLOR;
		color = isHovered ? HOVER_COLOR : color;
		color = isSelected ? SELECTION_COLOR : color;
		const auto thickness = ( isSelected || isHovered ? SELECTED_TRANSITION_THICKNESS : TRANSITION_THICKNESS ) * std::max( zoom, HALF );
		const auto arrowSize = ARROW_SIZE * std::max( zoom, HALF );
		ImVec2 labelPosition;
		if( geometry.loop )
		{
			drawList.AddBezierCubic( geometry.from, geometry.control1, geometry.control2, geometry.to, color, thickness );
			DrawArrowHead( drawList, geometry.to, Sub( geometry.to, geometry.control2 ), arrowSize, color );
			labelPosition = ImBezierCubicCalc( geometry.from, geometry.control1, geometry.control2, geometry.to, HALF );
		}
		else
		{
			// The line ends at the base of the arrow head, so that the head stays sharp
			const auto direction = Normalize( Sub( geometry.to, geometry.from ) );
			drawList.AddLine( geometry.from, Sub( geometry.to, Scale( direction, arrowSize * HALF ) ), color, thickness );
			DrawArrowHead( drawList, geometry.to, direction, arrowSize, color );
			// Labels sit at a third of the way, so that the labels of opposite transitions don't overlap
			labelPosition = Add( geometry.from, Scale( Sub( geometry.to, geometry.from ), THIRD ) );
		}
		if( drawText )
		{
			const auto label = GetTransitionSummary( transition );
			const auto size = font->CalcTextSizeA( fontSize, FLT_MAX, 0.f, label.c_str() );
			const ImVec2 textMin = { labelPosition.x - size.x * HALF, labelPosition.y - size.y * HALF };
			const auto padding = LABEL_PADDING * zoom;
			drawList.AddRectFilled( Sub( textMin, { padding, padding } ), Add( Add( textMin, size ), { padding, padding } ), LABEL_BACKGROUND_COLOR, padding );
			drawList.AddText( font, fontSize, textMin, isSelected ? SELECTION_COLOR : DIM_TEXT_COLOR, label.c_str() );
		}
	}
}

void StateMachineCanvas::DrawStates( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const
{
	const auto& session = *m_context->session;
	const auto& document = *m_context->document;
	const auto& settings = *graph.GetSettings<cmf::AnimationStateMachineSettings>( node );
	const auto zoom = m_view.zoom;
	const bool drawText = zoom >= MIN_TEXT_ZOOM;
	const auto rounding = STATE_ROUNDING * zoom;

	// Problems of unused StateMachine nodes are not reported
	const bool used = document.IsNodeUsed( graph, node );
	auto currentState = cmf::AnimationNetworkInstance::INVALID_STATE;
	if( const auto location = document.LocateExported( graph, node ) )
	{
		currentState = m_context->preview->GetCurrentState( document.GetNetwork(), *location );
	}

	for( uint32_t state = 0; state < m_states.size(); ++state )
	{
		const auto& geometry = m_states[state];
		const bool isSelected = session.IsStateSelected( state );
		const bool isHovered = m_hoveredState && *m_hoveredState == state;
		const bool isStart = state == settings.startState;
		const bool isCurrent = state == currentState;
		if( isStart )
		{
			DrawStartArrow( drawList, geometry.min, geometry.max, zoom );
		}

		drawList.AddRectFilled( geometry.min, geometry.max, isCurrent ? CURRENT_STATE_COLOR : STATE_BODY_COLOR, rounding );
		auto borderColor = isStart ? START_COLOR : BORDER_COLOR;
		auto borderThickness = isStart ? START_BORDER_THICKNESS : BORDER_THICKNESS;
		borderColor = isHovered && !isStart ? DIM_TEXT_COLOR : borderColor;
		if( isSelected )
		{
			borderColor = SELECTION_COLOR;
			borderThickness = SELECTED_BORDER_THICKNESS;
		}
		drawList.AddRect( geometry.min, geometry.max, borderColor, rounding, 0, borderThickness );

		if( drawText )
		{
			const char* note = nullptr;
			if( isCurrent )
			{
				note = "current";
			}
			else if( isStart )
			{
				note = "start";
			}
			DrawStateText( drawList, geometry.min, geometry.max, zoom, GetStateTitle( settings, state ), note );
		}
		if( used && !DiagnoseState( graph, node, state ).empty() )
		{
			DrawIssueMarker( drawList, geometry.min, geometry.max, zoom, drawText );
		}
	}
}

void StateMachineCanvas::DrawInteraction( ImDrawList& drawList ) const
{
	const auto mouse = ImGui::GetMousePos();
	if( m_mode == Mode::Connecting && m_connectStart < m_states.size() )
	{
		const auto& from = m_states[m_connectStart];
		const auto start = ClipToBox( Center( from.min, from.max ), mouse, from.min, from.max );
		const auto arrowSize = ARROW_SIZE * std::max( m_view.zoom, HALF );
		drawList.AddLine( start, mouse, SELECTION_COLOR, TRANSITION_THICKNESS * std::max( m_view.zoom, HALF ) );
		DrawArrowHead( drawList, mouse, Sub( mouse, start ), arrowSize, SELECTION_COLOR );
	}
	else if( m_mode == Mode::BoxSelecting )
	{
		drawList.AddRectFilled( m_pressPosition, mouse, BOX_FILL_COLOR );
		drawList.AddRect( m_pressPosition, mouse, BOX_BORDER_COLOR );
	}
}

void StateMachineCanvas::DrawTooltip( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const
{
	const auto errorColor = ImGui::ColorConvertU32ToFloat4( ERROR_COLOR );
	// Problems of unused StateMachine nodes are not reported
	const bool used = m_context->document->IsNodeUsed( graph, node );
	if( m_hoveredState )
	{
		const auto errors = used ? DiagnoseState( graph, node, *m_hoveredState ) : std::vector<std::string>{};
		const bool border = m_context->session->CanEdit() && IsOnBorder( *m_hoveredState, ImGui::GetMousePos() );
		if( errors.empty() && !border )
		{
			return;
		}
		ImGui::BeginTooltip();
		for( const auto& error : errors )
		{
			ImGui::TextColored( errorColor, "%s", error.c_str() );
		}
		if( border )
		{
			ImGui::TextUnformatted( "Drag to another state to add a transition" );
		}
		ImGui::EndTooltip();
		return;
	}
	if( m_hoveredTransition )
	{
		const auto& settings = *graph.GetSettings<cmf::AnimationStateMachineSettings>( node );
		const auto& transition = settings.transitions[*m_hoveredTransition];
		ImGui::BeginTooltip();
		ImGui::Text( "%s to %s", GetStateTitle( settings, transition.fromState ).c_str(), GetStateTitle( settings, transition.toState ).c_str() );
		ImGui::TextDisabled( "%s", GetTransitionSummary( transition ).c_str() );
		if( const auto error = used ? DiagnoseTransition( graph, node, transition ) : std::nullopt )
		{
			ImGui::TextColored( errorColor, "%s", error->c_str() );
		}
		ImGui::EndTooltip();
	}
}

void StateMachineCanvas::DrawContextMenus( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	DrawCanvasMenu();
	DrawStateMenu( *graph.GetSettings<cmf::AnimationStateMachineSettings>( node ) );
	DrawTransitionMenu();
}

void StateMachineCanvas::DrawCanvasMenu()
{
	auto& session = *m_context->session;
	if( ImGui::BeginPopup( "StateCanvasMenu" ) )
	{
		ImGui::BeginDisabled( !session.CanEdit() );
		if( ImGui::MenuItem( "Add State" ) )
		{
			session.AddState( { m_contextPosition.x - STATE_WIDTH * HALF, m_contextPosition.y - STATE_HEIGHT * HALF } );
		}
		if( ImGui::MenuItem( "Auto Arrange" ) )
		{
			session.AutoLayout();
			FrameStates( false );
		}
		ImGui::EndDisabled();
		ImGui::Separator();
		if( ImGui::MenuItem( "Select All", "Ctrl+A" ) )
		{
			session.SelectAll();
		}
		if( ImGui::MenuItem( "Frame All", "Home" ) )
		{
			FrameStates( false );
		}
		if( ImGui::MenuItem( "Leave State Machine", "Backspace" ) )
		{
			session.LeaveSubgraph();
		}
		ImGui::EndPopup();
	}
}

void StateMachineCanvas::DrawStateMenu( const cmf::AnimationStateMachineSettings& settings )
{
	auto& session = *m_context->session;
	if( !ImGui::BeginPopup( "StateMenu" ) )
	{
		return;
	}
	const auto state = m_contextState.value_or( 0 );
	if( state < settings.stateNames.size() )
	{
		if( ImGui::MenuItem( "Open State", "Enter" ) )
		{
			session.EnterState( state );
		}
		ImGui::BeginDisabled( !session.CanEdit() );
		if( ImGui::MenuItem( "Set as Start State", nullptr, false, state != settings.startState ) )
		{
			session.SetStartState( state );
		}
		if( ImGui::BeginMenu( "Add Transition To" ) )
		{
			for( uint32_t target = 0; target < settings.stateNames.size(); ++target )
			{
				ImGui::PushID( static_cast<int>( target ) );
				const auto label = GetStateTitle( settings, target ) + ( target == state ? " (itself)" : "" );
				if( ImGui::MenuItem( label.c_str() ) )
				{
					session.AddTransition( state, target );
				}
				ImGui::PopID();
			}
			ImGui::EndMenu();
		}
		if( ImGui::MenuItem( "Delete", "Del" ) )
		{
			session.DeleteSelection();
		}
		ImGui::EndDisabled();
	}
	ImGui::EndPopup();
}

void StateMachineCanvas::DrawTransitionMenu()
{
	auto& session = *m_context->session;
	if( ImGui::BeginPopup( "TransitionMenu" ) )
	{
		ImGui::BeginDisabled( !session.CanEdit() );
		if( ImGui::MenuItem( "Delete", "Del" ) )
		{
			session.DeleteSelection();
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}
}

}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
