// Copyright © 2026 CCP ehf.

#include "graphCanvas.h"

#include "nodeInfo.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

// ImGui uses C-style variadic functions for formatted text
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace canedit
{
namespace
{

// Node geometry in graph units
constexpr float NODE_WIDTH = 200.f;
constexpr float HEADER_HEIGHT = 26.f;
constexpr float ROW_HEIGHT = 22.f;
constexpr float NODE_PADDING = 8.f;
constexpr float SOCKET_RADIUS = 6.f;
constexpr float NODE_ROUNDING = 6.f;
constexpr float TEXT_INDENT = 14.f;
constexpr float GRID_SPACING = 40.f;

constexpr float MIN_ZOOM = 0.15f;
constexpr float MAX_ZOOM = 3.f;
constexpr float WHEEL_ZOOM_STEP = 1.15f;
constexpr float FRAME_MARGIN = 60.f;
constexpr float MAX_FRAME_ZOOM = 1.25f;
/** Zoom below which node text is not drawn. */
constexpr float MIN_TEXT_ZOOM = 0.35f;
/** Distance in pixels within which an edge is picked. */
constexpr float EDGE_PICK_DISTANCE = 6.f;
constexpr int EDGE_PICK_SEGMENTS = 24;
constexpr float EDGE_THICKNESS = 2.5f;
constexpr float SELECTED_EDGE_THICKNESS = 4.5f;
constexpr float MIN_EDGE_CURVE = 40.f;
constexpr ImVec2 PASTE_OFFSET = { 40.f, 40.f };
/** Mouse travel in pixels, squared, after which dropping a connection on empty space opens the connect menu. */
constexpr float CONNECT_DRAG_THRESHOLD_SQUARED = 4.f;
/** Grid spacing in pixels below which the grid is not drawn. */
constexpr float MIN_GRID_SPACING = 8.f;
constexpr float NODE_BORDER_THICKNESS = 1.f;
constexpr float SELECTED_NODE_BORDER_THICKNESS = 2.5f;
constexpr float SOCKET_OUTLINE_THICKNESS = 2.f;
/** Radius of the problem marker in the node header, relative to the header height. */
constexpr float ISSUE_MARKER_RADIUS = 0.32f;
/** Width of the node type search field, in font sizes. */
constexpr float FILTER_WIDTH = 12.f;
/** Distance of the read-only message from the bottom of the canvas, in text lines. */
constexpr float MESSAGE_LINE_OFFSET = 1.5f;
constexpr float HALF = 0.5f;
constexpr size_t LABEL_BUFFER_SIZE = 32;

constexpr ImU32 BACKGROUND_COLOR = IM_COL32( 30, 30, 34, 255 );
constexpr ImU32 GRID_COLOR = IM_COL32( 50, 50, 56, 255 );
constexpr ImU32 NODE_BODY_COLOR = IM_COL32( 48, 48, 54, 245 );
constexpr ImU32 NODE_BORDER_COLOR = IM_COL32( 15, 15, 15, 255 );
constexpr ImU32 SELECTION_COLOR = IM_COL32( 255, 200, 60, 255 );
constexpr ImU32 TEXT_COLOR = IM_COL32( 230, 230, 230, 255 );
constexpr ImU32 DIM_TEXT_COLOR = IM_COL32( 170, 170, 170, 255 );
constexpr ImU32 ERROR_COLOR = IM_COL32( 230, 70, 60, 255 );
constexpr ImU32 WARNING_COLOR = IM_COL32( 240, 170, 40, 255 );
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

float DistanceSquared( ImVec2 a, ImVec2 b )
{
	const auto d = Sub( a, b );
	return d.x * d.x + d.y * d.y;
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
	return std::sqrt( DistanceSquared( p, Add( a, Scale( ab, t ) ) ) );
}

// Control points of the curve of an edge
std::pair<ImVec2, ImVec2> GetEdgeControls( ImVec2 from, ImVec2 to, float zoom )
{
	const auto curve = std::max( MIN_EDGE_CURVE * zoom, std::abs( to.x - from.x ) * 0.5f );
	return { { from.x + curve, from.y }, { to.x - curve, to.y } };
}

uint32_t GetRowCount( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	return std::max( { graph.GetInputCount( node ), graph.GetOutputCount( node ), 1u } );
}

bool HasSummaryRow( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	return !GetNodeSummary( graph, node ).empty();
}

// Returns true if new nodes of the type can connect to a socket of the given type and direction
bool IsCompatible( cmf::AnimationNodeType type, cmf::AnimationSocketType socketType, bool needInput, bool rootGraph )
{
	const auto info = cmf::GetAnimationNodeTypeInfo( type );
	if( info.layout == cmf::AnimationSocketLayout::Interface )
	{
		// Interface sockets take any type, except the Pose output of the root graph
		return needInput ? ( type == cmf::AnimationNodeType::Output && ( !rootGraph || socketType == cmf::AnimationSocketType::Pose ) ) : type == cmf::AnimationNodeType::SubgraphInput;
	}
	if( !needInput )
	{
		return info.hasOutput && info.layout != cmf::AnimationSocketLayout::Subgraph && info.outputType == socketType;
	}
	if( info.layout == cmf::AnimationSocketLayout::Switch && socketType == cmf::AnimationSocketType::Pose )
	{
		return true;
	}
	if( info.layout == cmf::AnimationSocketLayout::Expression )
	{
		return false;
	}
	for( uint32_t i = 0; i < info.inputCount; ++i )
	{
		if( info.inputs[i].type == socketType )
		{
			return true;
		}
	}
	return false;
}

std::string FormatInputLabel( const cmf::AnimationGraphBuilder& graph, cmf::AnimationSocketRef input )
{
	auto label = graph.GetInputName( input );
	if( graph.GetInputType( input ) == cmf::AnimationSocketType::Float && !graph.GetConnection( input ) )
	{
		std::array<char, LABEL_BUFFER_SIZE> value{};
		std::snprintf( value.data(), value.size(), " = %.3g", graph.GetNode( input.node )->inputConstants[input.socket] );
		label += value.data();
	}
	return label;
}

}

GraphCanvas::GraphCanvas( AppContext& context ) :
	m_context( &context )
{
}

ImVec2 GraphCanvas::ToScreen( Vec2 position ) const
{
	return { m_origin.x + ( position.x - m_view.pan.x ) * m_view.zoom, m_origin.y + ( position.y - m_view.pan.y ) * m_view.zoom };
}

Vec2 GraphCanvas::ToGraph( ImVec2 position ) const
{
	return { ( position.x - m_origin.x ) / m_view.zoom + m_view.pan.x, ( position.y - m_origin.y ) / m_view.zoom + m_view.pan.y };
}

float GraphCanvas::GetNodeHeight( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	const auto rows = static_cast<float>( GetRowCount( graph, node ) ) + ( HasSummaryRow( graph, node ) ? 1.f : 0.f );
	return HEADER_HEIGHT + rows * ROW_HEIGHT + NODE_PADDING;
}

Vec2 GraphCanvas::GetViewCenter() const
{
	return ToGraph( { m_origin.x + m_size.x * HALF, m_origin.y + m_size.y * HALF } );
}

bool GraphCanvas::IsInteracting() const
{
	return m_mode != Mode::None;
}

void GraphCanvas::UpdateGeometry( cmf::AnimationGraphBuilder& graph )
{
	m_nodes.clear();
	m_nodeIndices.clear();
	const auto& document = *m_context->document;
	for( const auto id : graph.GetNodes() )
	{
		const auto position = document.GetNodePosition( graph, id ).value_or( Vec2{} );
		NodeGeometry node;
		node.id = id;
		node.min = ToScreen( position );
		node.max = ToScreen( { position.x + NODE_WIDTH, position.y + GetNodeHeight( graph, id ) } );
		const float firstRow = HEADER_HEIGHT + ( HasSummaryRow( graph, id ) ? ROW_HEIGHT : 0.f );
		for( uint32_t i = 0; i < graph.GetInputCount( id ); ++i )
		{
			node.inputs.push_back( ToScreen( { position.x, position.y + firstRow + ( static_cast<float>( i ) + HALF ) * ROW_HEIGHT } ) );
		}
		for( uint32_t i = 0; i < graph.GetOutputCount( id ); ++i )
		{
			node.outputs.push_back( ToScreen( { position.x + NODE_WIDTH, position.y + firstRow + ( static_cast<float>( i ) + HALF ) * ROW_HEIGHT } ) );
		}
		m_nodeIndices[id.index] = m_nodes.size();
		m_nodes.push_back( std::move( node ) );
	}
}

std::optional<GraphCanvas::Socket> GraphCanvas::FindSocket( ImVec2 position ) const
{
	const auto radius = std::max( SOCKET_RADIUS * m_view.zoom, SOCKET_RADIUS * 0.75f ) + 2.f;
	const auto radiusSquared = radius * radius;
	// Nodes drawn last are on top
	for( auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it )
	{
		for( uint32_t i = 0; i < it->outputs.size(); ++i )
		{
			if( DistanceSquared( position, it->outputs[i] ) <= radiusSquared )
			{
				return Socket{ { it->id, i }, true };
			}
		}
		for( uint32_t i = 0; i < it->inputs.size(); ++i )
		{
			if( DistanceSquared( position, it->inputs[i] ) <= radiusSquared )
			{
				return Socket{ { it->id, i }, false };
			}
		}
	}
	return std::nullopt;
}

const GraphCanvas::NodeGeometry* GraphCanvas::FindNode( ImVec2 position ) const
{
	for( auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it )
	{
		if( position.x >= it->min.x && position.x <= it->max.x && position.y >= it->min.y && position.y <= it->max.y )
		{
			return &*it;
		}
	}
	return nullptr;
}

ImVec2 GraphCanvas::GetSocketPosition( const Socket& socket ) const
{
	const auto it = m_nodeIndices.find( socket.ref.node.index );
	if( it == m_nodeIndices.end() )
	{
		return {};
	}
	const auto& node = m_nodes[it->second];
	const auto& sockets = socket.output ? node.outputs : node.inputs;
	return socket.ref.socket < sockets.size() ? sockets[socket.ref.socket] : node.min;
}

std::optional<cmf::AnimationSocketRef> GraphCanvas::FindEdge( ImVec2 position ) const
{
	const auto& graph = m_context->session->GetGraph();
	for( const auto& edge : graph.GetEdges() )
	{
		const auto from = GetSocketPosition( { edge.from, true } );
		const auto to = GetSocketPosition( { edge.to, false } );
		const auto [c1, c2] = GetEdgeControls( from, to, m_view.zoom );
		ImVec2 previous = from;
		for( int i = 1; i <= EDGE_PICK_SEGMENTS; ++i )
		{
			const auto point = ImBezierCubicCalc( from, c1, c2, to, static_cast<float>( i ) / static_cast<float>( EDGE_PICK_SEGMENTS ) );
			if( DistanceToSegment( position, previous, point ) <= EDGE_PICK_DISTANCE )
			{
				return edge.to;
			}
			previous = point;
		}
	}
	return std::nullopt;
}

void GraphCanvas::ZoomAt( ImVec2 screenPosition, float factor )
{
	const auto anchor = ToGraph( screenPosition );
	m_view.zoom = std::clamp( m_view.zoom * factor, MIN_ZOOM, MAX_ZOOM );
	// Keep the graph position under the mouse in place
	m_view.pan = { anchor.x - ( screenPosition.x - m_origin.x ) / m_view.zoom, anchor.y - ( screenPosition.y - m_origin.y ) / m_view.zoom };
}

void GraphCanvas::FrameNodes( bool selectionOnly )
{
	m_frameRequest = true;
	m_frameSelectionOnly = selectionOnly;
}

void GraphCanvas::Draw()
{
	auto& session = *m_context->session;
	auto& document = *m_context->document;
	auto& graph = session.GetGraph();
	if( &graph != m_viewGraph )
	{
		m_viewGraph = &graph;
		m_view = document.GetView( graph );
		m_mode = Mode::None;
		if( m_framedGraphs.insert( &graph ).second && m_view.pan == Vec2{} && m_view.zoom == 1.f )
		{
			FrameNodes( false );
		}
	}
	const auto previousView = m_view;

	m_origin = ImGui::GetCursorScreenPos();
	m_size = ImGui::GetContentRegionAvail();
	m_size.x = std::max( m_size.x, 1.f );
	m_size.y = std::max( m_size.y, 1.f );

	if( m_frameRequest )
	{
		m_frameRequest = false;
		Vec2 low{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		Vec2 high{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
		for( const auto id : graph.GetNodes() )
		{
			if( m_frameSelectionOnly && !session.GetSelection().empty() && !session.IsSelected( id ) )
			{
				continue;
			}
			const auto position = document.GetNodePosition( graph, id ).value_or( Vec2{} );
			low = { std::min( low.x, position.x ), std::min( low.y, position.y ) };
			high = { std::max( high.x, position.x + NODE_WIDTH ), std::max( high.y, position.y + GetNodeHeight( graph, id ) ) };
		}
		if( low.x <= high.x )
		{
			const auto width = high.x - low.x + 2.f * FRAME_MARGIN;
			const auto height = high.y - low.y + 2.f * FRAME_MARGIN;
			m_view.zoom = std::clamp( std::min( m_size.x / width, m_size.y / height ), MIN_ZOOM, MAX_FRAME_ZOOM );
			const Vec2 center = { ( low.x + high.x ) * HALF, ( low.y + high.y ) * HALF };
			m_view.pan = { center.x - m_size.x * HALF / m_view.zoom, center.y - m_size.y * HALF / m_view.zoom };
		}
	}

	ImGui::InvisibleButton( "canvas", m_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle );
	const bool hovered = ImGui::IsItemHovered();
	HandleDrop( graph );

	UpdateGeometry( graph );
	HandleInput( graph, hovered );
	// Edits may have changed the graph
	auto& currentGraph = session.GetGraph();
	UpdateGeometry( currentGraph );

	auto& drawList = *ImGui::GetWindowDrawList();
	drawList.PushClipRect( m_origin, Add( m_origin, m_size ), true );
	DrawGrid( drawList );
	DrawEdges( drawList, currentGraph );
	DrawNodes( drawList, currentGraph );
	DrawInteraction( drawList, currentGraph );
	if( !session.CanEdit() )
	{
		const char* message = "The skeleton is missing: set a skeleton in the network properties to edit the network.";
		drawList.AddText( Add( m_origin, { ImGui::GetStyle().WindowPadding.x, m_size.y - ImGui::GetTextLineHeightWithSpacing() * MESSAGE_LINE_OFFSET } ), ERROR_COLOR, message );
	}
	drawList.PopClipRect();

	if( hovered && m_mode == Mode::None )
	{
		DrawNodeTooltip( currentGraph );
	}
	DrawContextMenus( currentGraph );
	HandleShortcuts();

	if( m_view.pan != previousView.pan || m_view.zoom != previousView.zoom )
	{
		document.SetView( session.GetGraph(), m_view );
	}
}

void GraphCanvas::HandleDrop( cmf::AnimationGraphBuilder& graph )
{
	if( !ImGui::BeginDragDropTarget() )
	{
		return;
	}
	auto& session = *m_context->session;
	const auto position = ToGraph( ImGui::GetMousePos() );
	if( const auto* payload = ImGui::AcceptDragDropPayload( NODE_TYPE_PAYLOAD ) )
	{
		uint32_t type = 0;
		std::memcpy( &type, payload->Data, sizeof( type ) );
		session.AddNode( static_cast<cmf::AnimationNodeType>( type ), position );
	}
	if( const auto* payload = ImGui::AcceptDragDropPayload( PARAMETER_PAYLOAD ) )
	{
		uint32_t parameter = 0;
		std::memcpy( &parameter, payload->Data, sizeof( parameter ) );
		const ScopedEdit edit( session );
		const auto node = session.AddNode( cmf::AnimationNodeType::Parameter, position );
		if( node.IsSet() )
		{
			static_cast<void>( graph.SetParameter( node, parameter ) );
		}
	}
	ImGui::EndDragDropTarget();
}

void GraphCanvas::HandleInput( cmf::AnimationGraphBuilder& graph, bool hovered )
{
	const auto& io = ImGui::GetIO();
	const auto mouse = io.MousePos;
	UpdateHover( mouse, hovered );

	if( hovered && io.MouseWheel != 0.f )
	{
		ZoomAt( mouse, std::pow( WHEEL_ZOOM_STEP, io.MouseWheel ) );
	}

	if( m_mode == Mode::None && hovered && !HandleIdleInput( graph, mouse ) )
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
	case Mode::MovingNodes:
		if( io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f )
		{
			m_context->session->MoveNodes( m_context->session->GetSelection(), { io.MouseDelta.x / m_view.zoom, io.MouseDelta.y / m_view.zoom } );
			m_moved = true;
		}
		break;
	case Mode::BoxSelecting:
	case Mode::Connecting:
	case Mode::None:
		break;
	}

	if( m_mode != Mode::None && m_mode != Mode::Panning && !ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
	{
		HandleRelease( graph );
	}
}

void GraphCanvas::UpdateHover( ImVec2 mouse, bool hovered )
{
	m_hoveredSocket.reset();
	m_hoveredNode.reset();
	m_hoveredEdge.reset();
	if( !hovered && m_mode != Mode::Connecting )
	{
		return;
	}
	m_hoveredSocket = FindSocket( mouse );
	if( const auto* node = FindNode( mouse ) )
	{
		m_hoveredNode = node->id;
	}
	if( !m_hoveredSocket && !m_hoveredNode && m_mode == Mode::None )
	{
		m_hoveredEdge = FindEdge( mouse );
	}
}

bool GraphCanvas::HandleIdleInput( cmf::AnimationGraphBuilder& graph, ImVec2 mouse )
{
	if( ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) || ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && ImGui::IsKeyDown( ImGuiKey_Space ) ) )
	{
		m_mode = Mode::Panning;
	}
	else if( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && m_hoveredNode && graph.GetSubgraph( *m_hoveredNode ) != nullptr )
	{
		m_context->session->EnterSubgraph( *m_hoveredNode );
		return false;
	}
	else if( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
	{
		HandleLeftPress( graph );
	}
	else
	{
		HandleRightButton( mouse );
	}
	return true;
}

void GraphCanvas::HandleRightButton( ImVec2 mouse )
{
	if( ImGui::IsMouseReleased( ImGuiMouseButton_Right ) && ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ).x == 0.f && ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ).y == 0.f )
	{
		m_contextPosition = ToGraph( mouse );
		if( m_hoveredNode )
		{
			if( !m_context->session->IsSelected( *m_hoveredNode ) )
			{
				m_context->session->Select( *m_hoveredNode );
			}
			m_contextNode = m_hoveredNode;
			ImGui::OpenPopup( "NodeMenu" );
		}
		else
		{
			m_menuFilter.clear();
			ImGui::OpenPopup( "CanvasMenu" );
		}
	}
	else if( ImGui::IsMouseDragging( ImGuiMouseButton_Right ) )
	{
		m_mode = Mode::Panning;
	}
}

void GraphCanvas::HandleLeftPress( cmf::AnimationGraphBuilder& graph )
{
	auto& session = *m_context->session;
	const bool canEdit = session.CanEdit();
	const bool additive = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
	m_pressPosition = ImGui::GetMousePos();
	m_moved = false;
	m_editStarted = false;
	if( m_hoveredSocket && canEdit )
	{
		m_connectStart = *m_hoveredSocket;
		if( !m_hoveredSocket->output )
		{
			// Dragging a connected input picks up its edge
			if( const auto connection = graph.GetConnection( m_hoveredSocket->ref ) )
			{
				session.BeginEdit();
				m_editStarted = true;
				session.Disconnect( m_hoveredSocket->ref );
				m_connectStart = { *connection, true };
			}
		}
		m_mode = Mode::Connecting;
	}
	else if( m_hoveredNode )
	{
		if( additive )
		{
			if( session.IsSelected( *m_hoveredNode ) )
			{
				session.Deselect( *m_hoveredNode );
			}
			else
			{
				session.Select( *m_hoveredNode, true );
			}
			return;
		}
		if( !session.IsSelected( *m_hoveredNode ) )
		{
			session.Select( *m_hoveredNode );
		}
		if( canEdit )
		{
			session.BeginEdit();
			m_editStarted = true;
			m_mode = Mode::MovingNodes;
		}
	}
	else if( m_hoveredEdge )
	{
		session.SelectEdge( *m_hoveredEdge );
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

void GraphCanvas::HandleRelease( cmf::AnimationGraphBuilder& graph )
{
	auto& session = *m_context->session;
	const auto mouse = ImGui::GetMousePos();
	if( m_mode == Mode::BoxSelecting )
	{
		FinishBoxSelection( mouse );
	}
	else if( m_mode == Mode::Connecting )
	{
		FinishConnection( mouse );
	}
	if( m_editStarted )
	{
		// Ends the drag, or the pick up of an edge; a failed connection already restored the edge
		if( session.IsEditing() )
		{
			session.EndEdit();
		}
		m_editStarted = false;
	}
	static_cast<void>( graph );
	m_mode = Mode::None;
}

void GraphCanvas::FinishBoxSelection( ImVec2 mouse )
{
	auto& session = *m_context->session;
	const ImVec2 low = { std::min( m_pressPosition.x, mouse.x ), std::min( m_pressPosition.y, mouse.y ) };
	const ImVec2 high = { std::max( m_pressPosition.x, mouse.x ), std::max( m_pressPosition.y, mouse.y ) };
	auto selection = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift ? session.GetSelection() : std::vector<cmf::AnimationNodeId>();
	for( const auto& node : m_nodes )
	{
		if( node.max.x >= low.x && node.min.x <= high.x && node.max.y >= low.y && node.min.y <= high.y && std::find( selection.begin(), selection.end(), node.id ) == selection.end() )
		{
			selection.push_back( node.id );
		}
	}
	session.SetSelection( std::move( selection ) );
}

void GraphCanvas::FinishConnection( ImVec2 mouse )
{
	auto& session = *m_context->session;
	const auto target = FindSocket( mouse );
	if( target && target->output != m_connectStart.output )
	{
		const auto from = m_connectStart.output ? m_connectStart.ref : target->ref;
		const auto to = m_connectStart.output ? target->ref : m_connectStart.ref;
		const auto result = session.Connect( from, to );
		if( !result )
		{
			m_context->SetStatus( "Can't connect: " + result.error, true );
		}
	}
	else if( !target && DistanceSquared( mouse, m_pressPosition ) > CONNECT_DRAG_THRESHOLD_SQUARED )
	{
		// Dropped on empty space: offer the nodes that can connect to the socket
		m_pendingConnection = m_connectStart;
		m_contextPosition = ToGraph( mouse );
		m_menuFilter.clear();
		ImGui::OpenPopup( "ConnectMenu" );
	}
}

void GraphCanvas::HandleShortcuts()
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
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_C ) )
	{
		CopyToClipboard( false );
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_X ) )
	{
		CopyToClipboard( true );
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_V ) )
	{
		PasteFromClipboard();
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_D ) )
	{
		const auto text = session.CopySelection();
		if( !text.empty() )
		{
			const auto position = Add( ToImVec2( session.GetDocument().GetNodePosition( session.GetGraph(), session.GetSelection().front() ).value_or( Vec2{} ) ), PASTE_OFFSET );
			session.Paste( text, ToVec2( position ) );
		}
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_A ) )
	{
		session.SelectAll();
	}
	if( ImGui::IsKeyPressed( ImGuiKey_F, false ) && !ImGui::GetIO().KeyCtrl )
	{
		FrameNodes( true );
	}
	if( ImGui::IsKeyPressed( ImGuiKey_Home, false ) )
	{
		FrameNodes( false );
	}
	if( ImGui::IsKeyPressed( ImGuiKey_Backspace, false ) )
	{
		session.LeaveSubgraph();
	}
	if( ImGui::IsKeyPressed( ImGuiKey_Enter, false ) && session.GetSelection().size() == 1 )
	{
		session.EnterSubgraph( session.GetSelection().front() );
	}
}

void GraphCanvas::CopyToClipboard( bool cut )
{
	auto& session = *m_context->session;
	const auto text = cut ? session.CutSelection() : session.CopySelection();
	if( !text.empty() )
	{
		ImGui::SetClipboardText( text.c_str() );
	}
}

void GraphCanvas::PasteFromClipboard()
{
	const char* text = ImGui::GetClipboardText();
	if( text == nullptr )
	{
		return;
	}
	const auto mouse = ImGui::GetMousePos();
	const bool inside = mouse.x >= m_origin.x && mouse.y >= m_origin.y && mouse.x < m_origin.x + m_size.x && mouse.y < m_origin.y + m_size.y;
	const auto result = m_context->session->Paste( text, inside ? ToGraph( mouse ) : GetViewCenter() );
	if( !result.error.empty() )
	{
		m_context->SetStatus( result.error, true );
	}
	else if( !result.warnings.empty() )
	{
		m_context->SetStatus( "Pasted with problems: " + result.warnings.front(), true );
	}
}

void GraphCanvas::DrawGrid( ImDrawList& drawList ) const
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

void GraphCanvas::DrawEdges( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph ) const
{
	const auto& selectedEdge = m_context->session->GetSelectedEdge();
	for( const auto& edge : graph.GetEdges() )
	{
		const auto from = GetSocketPosition( { edge.from, true } );
		const auto to = GetSocketPosition( { edge.to, false } );
		const auto [c1, c2] = GetEdgeControls( from, to, m_view.zoom );
		const bool selected = selectedEdge && *selectedEdge == edge.to;
		const bool hovered = m_hoveredEdge && *m_hoveredEdge == edge.to;
		const auto color = selected ? SELECTION_COLOR : GetSocketColor( graph.GetOutputType( edge.from ) );
		const auto thickness = ( selected || hovered ? SELECTED_EDGE_THICKNESS : EDGE_THICKNESS ) * std::max( m_view.zoom, 0.5f );
		drawList.AddBezierCubic( from, c1, c2, to, color, thickness );
	}
}

void GraphCanvas::DrawNodes( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph ) const
{
	for( const auto& node : m_nodes )
	{
		DrawNode( drawList, graph, node );
	}
}

void GraphCanvas::DrawNode( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, const NodeGeometry& node ) const
{
	const auto zoom = m_view.zoom;
	const auto* data = graph.GetNode( node.id );
	const bool selected = m_context->session->IsSelected( node.id );
	const bool hovered = m_hoveredNode && *m_hoveredNode == node.id;
	const auto rounding = NODE_ROUNDING * zoom;
	const ImVec2 headerMax = { node.max.x, node.min.y + HEADER_HEIGHT * zoom };

	auto borderColor = hovered ? DIM_TEXT_COLOR : NODE_BORDER_COLOR;
	if( selected )
	{
		borderColor = SELECTION_COLOR;
	}
	drawList.AddRectFilled( node.min, node.max, NODE_BODY_COLOR, rounding );
	drawList.AddRectFilled( node.min, headerMax, GetNodeColor( data->type ), rounding, ImDrawFlags_RoundCornersTop );
	drawList.AddRect( node.min, node.max, borderColor, rounding, 0, selected ? SELECTED_NODE_BORDER_THICKNESS : NODE_BORDER_THICKNESS );

	const bool drawText = zoom >= MIN_TEXT_ZOOM;
	if( drawText )
	{
		auto* font = ImGui::GetFont();
		const auto fontSize = ImGui::GetFontSize() * zoom;
		const ImVec4 clip = { node.min.x, node.min.y, node.max.x, node.max.y };
		const auto textOffsetY = ( ROW_HEIGHT * zoom - fontSize ) * HALF;
		auto title = GetNodeTitle( graph, node.id );
		if( graph.GetSubgraph( node.id ) != nullptr )
		{
			title += "  [+]";
		}
		drawList.AddText( font, fontSize, { node.min.x + NODE_PADDING * zoom, node.min.y + ( HEADER_HEIGHT * zoom - fontSize ) * HALF }, TEXT_COLOR, title.c_str(), nullptr, 0.f, &clip );
		const auto summary = GetNodeSummary( graph, node.id );
		if( !summary.empty() )
		{
			drawList.AddText( font, fontSize, { node.min.x + NODE_PADDING * zoom, headerMax.y + textOffsetY }, DIM_TEXT_COLOR, summary.c_str(), nullptr, 0.f, &clip );
		}
	}

	DrawNodeSockets( drawList, graph, node, drawText );
	DrawNodeIssues( drawList, graph, node, zoom, drawText );
}

void GraphCanvas::DrawNodeSockets( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, const NodeGeometry& node, bool drawText ) const
{
	const auto zoom = m_view.zoom;
	auto* font = ImGui::GetFont();
	const auto fontSize = ImGui::GetFontSize() * zoom;
	const ImVec4 clip = { node.min.x, node.min.y, node.max.x, node.max.y };
	const auto radius = SOCKET_RADIUS * zoom;
	for( uint32_t i = 0; i < node.inputs.size(); ++i )
	{
		const cmf::AnimationSocketRef ref = { node.id, i };
		const auto type = graph.GetInputType( ref );
		const bool connected = graph.GetConnection( ref ).has_value();
		const bool socketHovered = m_hoveredSocket && !m_hoveredSocket->output && m_hoveredSocket->ref == ref;
		if( connected || socketHovered )
		{
			drawList.AddCircleFilled( node.inputs[i], radius, GetSocketColor( type ) );
		}
		else
		{
			drawList.AddCircle( node.inputs[i], radius, GetSocketColor( type ), 0, SOCKET_OUTLINE_THICKNESS );
		}
		if( drawText )
		{
			const auto label = FormatInputLabel( graph, ref );
			drawList.AddText( font, fontSize, { node.inputs[i].x + TEXT_INDENT * zoom, node.inputs[i].y - fontSize * HALF }, TEXT_COLOR, label.c_str(), nullptr, 0.f, &clip );
		}
	}
	for( uint32_t i = 0; i < node.outputs.size(); ++i )
	{
		const cmf::AnimationSocketRef ref = { node.id, i };
		drawList.AddCircleFilled( node.outputs[i], radius, GetSocketColor( graph.GetOutputType( ref ) ) );
		if( drawText )
		{
			const auto label = graph.GetOutputName( ref );
			const auto width = font->CalcTextSizeA( fontSize, FLT_MAX, 0.f, label.c_str() ).x;
			drawList.AddText( font, fontSize, { node.outputs[i].x - TEXT_INDENT * zoom - width, node.outputs[i].y - fontSize * HALF }, TEXT_COLOR, label.c_str(), nullptr, 0.f, &clip );
		}
	}
}

void GraphCanvas::DrawNodeIssues( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph, const NodeGeometry& node, float zoom, bool drawText )
{
	const auto issues = DiagnoseNode( graph, node.id );
	if( issues.errors.empty() && issues.warnings.empty() )
	{
		return;
	}
	const ImVec2 center = { node.max.x - HEADER_HEIGHT * HALF * zoom, node.min.y + HEADER_HEIGHT * HALF * zoom };
	drawList.AddCircleFilled( center, HEADER_HEIGHT * ISSUE_MARKER_RADIUS * zoom, issues.errors.empty() ? WARNING_COLOR : ERROR_COLOR );
	if( drawText )
	{
		auto* font = ImGui::GetFont();
		const auto fontSize = ImGui::GetFontSize() * zoom;
		const auto width = font->CalcTextSizeA( fontSize, FLT_MAX, 0.f, "!" ).x;
		drawList.AddText( font, fontSize, { center.x - width * HALF, center.y - fontSize * HALF }, IM_COL32_WHITE, "!" );
	}
}

void GraphCanvas::DrawInteraction( ImDrawList& drawList, const cmf::AnimationGraphBuilder& graph ) const
{
	const auto mouse = ImGui::GetMousePos();
	if( m_mode == Mode::Connecting )
	{
		const auto start = GetSocketPosition( m_connectStart );
		const auto from = m_connectStart.output ? start : mouse;
		const auto to = m_connectStart.output ? mouse : start;
		const auto type = m_connectStart.output ? graph.GetOutputType( m_connectStart.ref ) : graph.GetInputType( m_connectStart.ref );
		const auto [c1, c2] = GetEdgeControls( from, to, m_view.zoom );
		drawList.AddBezierCubic( from, c1, c2, to, GetSocketColor( type ), EDGE_THICKNESS * std::max( m_view.zoom, HALF ) );
	}
	else if( m_mode == Mode::BoxSelecting )
	{
		drawList.AddRectFilled( m_pressPosition, mouse, BOX_FILL_COLOR );
		drawList.AddRect( m_pressPosition, mouse, BOX_BORDER_COLOR );
	}
}

void GraphCanvas::DrawNodeTooltip( const cmf::AnimationGraphBuilder& graph ) const
{
	if( m_hoveredSocket )
	{
		const auto& ref = m_hoveredSocket->ref;
		const auto type = m_hoveredSocket->output ? graph.GetOutputType( ref ) : graph.GetInputType( ref );
		const auto name = m_hoveredSocket->output ? graph.GetOutputName( ref ) : graph.GetInputName( ref );
		ImGui::SetTooltip( "%s (%s)", name.c_str(), GetSocketTypeName( type ) );
		return;
	}
	if( !m_hoveredNode )
	{
		return;
	}
	const auto issues = DiagnoseNode( graph, *m_hoveredNode );
	if( issues.errors.empty() && issues.warnings.empty() )
	{
		return;
	}
	ImGui::BeginTooltip();
	for( const auto& error : issues.errors )
	{
		ImGui::TextColored( ImGui::ColorConvertU32ToFloat4( ERROR_COLOR ), "%s", error.c_str() );
	}
	for( const auto& warning : issues.warnings )
	{
		ImGui::TextColored( ImGui::ColorConvertU32ToFloat4( WARNING_COLOR ), "%s", warning.c_str() );
	}
	ImGui::EndTooltip();
}

std::optional<cmf::AnimationNodeType> GraphCanvas::DrawNodeTypeMenu( const cmf::AnimationGraphBuilder& graph, std::optional<cmf::AnimationSocketType> compatibleWith, bool outputSide )
{
	if( ImGui::IsWindowAppearing() )
	{
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::SetNextItemWidth( ImGui::GetFontSize() * FILTER_WIDTH );
	ImGui::InputTextWithHint( "##filter", "Search", &m_menuFilter );
	std::string filter = m_menuFilter;
	std::transform( filter.begin(), filter.end(), filter.begin(), []( char c ) { return static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) ); } );
	std::optional<cmf::AnimationNodeType> chosen;
	std::optional<cmf::AnimationNodeType> first;
	for( const auto& category : GetNodeCategories() )
	{
		bool header = false;
		for( const auto type : category.types )
		{
			const std::string name = cmf::GetAnimationNodeTypeInfo( type ).name;
			std::string lower = name;
			std::transform( lower.begin(), lower.end(), lower.begin(), []( char c ) { return static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) ); } );
			if( !CanAddNode( graph, type ) || ( !filter.empty() && lower.find( filter ) == std::string::npos ) )
			{
				continue;
			}
			// An output socket needs a node with a matching input, and the other way around
			if( compatibleWith && !IsCompatible( type, *compatibleWith, outputSide, graph.GetParent() == nullptr ) )
			{
				continue;
			}
			if( !header )
			{
				ImGui::TextDisabled( "%s", category.name );
				header = true;
			}
			first = first ? first : type;
			if( ImGui::MenuItem( name.c_str() ) )
			{
				chosen = type;
			}
		}
	}
	if( !chosen && first && ImGui::IsKeyPressed( ImGuiKey_Enter ) )
	{
		chosen = first;
		ImGui::CloseCurrentPopup();
	}
	return chosen;
}

void GraphCanvas::AddConnectedNode( cmf::AnimationNodeType type )
{
	auto& session = *m_context->session;
	const ScopedEdit edit( session );
	const auto node = session.AddNode( type, m_contextPosition );
	if( !node.IsSet() || !m_pendingConnection )
	{
		return;
	}
	auto& graph = session.GetGraph();
	const auto start = *m_pendingConnection;
	const auto startType = start.output ? graph.GetOutputType( start.ref ) : graph.GetInputType( start.ref );
	if( type == cmf::AnimationNodeType::Output || type == cmf::AnimationNodeType::SubgraphInput )
	{
		// Interface nodes take the type of the socket they are created for
		const auto name = start.output ? graph.GetOutputName( start.ref ) : graph.GetInputName( start.ref );
		static_cast<void>( graph.SetInterfaceSocket( node, name, startType ) );
	}
	if( start.output )
	{
		for( uint32_t socket = 0; socket < graph.GetInputCount( node ); ++socket )
		{
			if( graph.GetInputType( { node, socket } ) == startType && graph.Connect( start.ref, { node, socket } ) )
			{
				break;
			}
		}
	}
	else if( graph.GetOutputCount( node ) > 0 && graph.GetOutputType( { node, 0 } ) == startType )
	{
		session.Connect( { node, 0 }, start.ref );
	}
}

void GraphCanvas::DrawContextMenus( cmf::AnimationGraphBuilder& graph )
{
	DrawCanvasMenu( graph );
	DrawNodeMenu( graph );
	DrawConnectMenu( graph );
}

void GraphCanvas::DrawCanvasMenu( cmf::AnimationGraphBuilder& graph )
{
	if( !ImGui::BeginPopup( "CanvasMenu" ) )
	{
		return;
	}
	auto& session = *m_context->session;
	ImGui::BeginDisabled( !session.CanEdit() );
	if( ImGui::BeginMenu( "Add Node" ) )
	{
		m_pendingConnection.reset();
		if( const auto type = DrawNodeTypeMenu( graph, std::nullopt, false ) )
		{
			session.AddNode( *type, m_contextPosition );
		}
		ImGui::EndMenu();
	}
	if( ImGui::MenuItem( "Paste", "Ctrl+V" ) )
	{
		const char* text = ImGui::GetClipboardText();
		if( text != nullptr )
		{
			const auto result = session.Paste( text, m_contextPosition );
			if( !result.error.empty() )
			{
				m_context->SetStatus( result.error, true );
			}
		}
	}
	if( ImGui::MenuItem( "Auto Arrange" ) )
	{
		session.AutoLayout();
		FrameNodes( false );
	}
	ImGui::EndDisabled();
	ImGui::Separator();
	if( ImGui::MenuItem( "Select All", "Ctrl+A" ) )
	{
		session.SelectAll();
	}
	if( ImGui::MenuItem( "Frame All", "Home" ) )
	{
		FrameNodes( false );
	}
	if( !session.GetGraphPath().empty() && ImGui::MenuItem( "Leave Subgraph", "Backspace" ) )
	{
		session.LeaveSubgraph();
	}
	ImGui::EndPopup();
}

void GraphCanvas::DrawNodeMenu( const cmf::AnimationGraphBuilder& graph )
{
	if( !ImGui::BeginPopup( "NodeMenu" ) )
	{
		return;
	}
	auto& session = *m_context->session;
	if( m_contextNode && graph.GetSubgraph( *m_contextNode ) != nullptr && ImGui::MenuItem( "Open Subgraph", "Enter" ) )
	{
		session.EnterSubgraph( *m_contextNode );
	}
	ImGui::BeginDisabled( !session.CanEdit() );
	if( ImGui::MenuItem( "Copy", "Ctrl+C" ) )
	{
		CopyToClipboard( false );
	}
	if( ImGui::MenuItem( "Cut", "Ctrl+X" ) )
	{
		CopyToClipboard( true );
	}
	if( ImGui::MenuItem( "Duplicate", "Ctrl+D" ) )
	{
		const auto text = session.CopySelection();
		session.Paste( text, { m_contextPosition.x + PASTE_OFFSET.x, m_contextPosition.y + PASTE_OFFSET.y } );
	}
	if( ImGui::MenuItem( "Delete", "Del" ) )
	{
		session.DeleteSelection();
	}
	ImGui::EndDisabled();
	ImGui::EndPopup();
}

void GraphCanvas::DrawConnectMenu( const cmf::AnimationGraphBuilder& graph )
{
	if( !ImGui::BeginPopup( "ConnectMenu" ) )
	{
		return;
	}
	if( m_pendingConnection && graph.IsValid( m_pendingConnection->ref.node ) )
	{
		const auto& start = *m_pendingConnection;
		const auto type = start.output ? graph.GetOutputType( start.ref ) : graph.GetInputType( start.ref );
		if( const auto chosen = DrawNodeTypeMenu( graph, type, start.output ) )
		{
			AddConnectedNode( *chosen );
			m_pendingConnection.reset();
		}
	}
	else
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
