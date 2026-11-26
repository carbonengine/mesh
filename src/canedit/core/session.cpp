// Copyright © 2026 CCP ehf.

#include "session.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <tuple>

namespace canedit
{

EditorSession::EditorSession( Document& document ) :
	m_document( &document )
{
}

Document& EditorSession::GetDocument()
{
	return *m_document;
}

const Document& EditorSession::GetDocument() const
{
	return *m_document;
}

void EditorSession::Reset()
{
	m_history.Clear();
	m_graphPath.clear();
	ClearAllSelections();
	m_editDepth = 0;
}

void EditorSession::Reload()
{
	const bool modified = m_document->IsModified();
	const auto snapshot = Capture();
	m_document->ForgetEmbeddedFiles();
	Restore( snapshot );
	if( !modified )
	{
		m_document->MarkUnmodified();
	}
}

const GraphPath& EditorSession::GetGraphPath() const
{
	return m_graphPath;
}

cmf::AnimationGraphBuilder& EditorSession::GetGraph()
{
	auto* graph = m_document->FindGraph( m_graphPath );
	if( graph == nullptr )
	{
		m_graphPath.clear();
		ClearAllSelections();
		graph = &m_document->GetNetwork().GetRootGraph();
	}
	return *graph;
}

const cmf::AnimationGraphBuilder& EditorSession::GetGraph() const
{
	const auto* graph = m_document->FindGraph( m_graphPath );
	return graph != nullptr ? *graph : m_document->GetNetwork().GetRootGraph();
}

cmf::AnimationNodeId EditorSession::GetStateMachine() const
{
	return m_document->FindStateMachine( m_graphPath );
}

void EditorSession::OpenGraph( GraphPath path )
{
	if( m_document->FindGraph( path ) == nullptr )
	{
		path.clear();
	}
	if( path != m_graphPath )
	{
		m_graphPath = std::move( path );
		ClearAllSelections();
	}
}

bool EditorSession::CanEnter( cmf::AnimationNodeId node ) const
{
	if( GetStateMachine().IsSet() )
	{
		return false;
	}
	const auto& graph = GetGraph();
	return graph.GetSubgraph( node ) != nullptr || graph.GetSettings<cmf::AnimationStateMachineSettings>( node ) != nullptr;
}

bool EditorSession::EnterSubgraph( cmf::AnimationNodeId node )
{
	if( !CanEnter( node ) )
	{
		return false;
	}
	auto path = m_graphPath;
	path.push_back( { node, NO_STATE } );
	OpenGraph( std::move( path ) );
	return true;
}

bool EditorSession::EnterState( uint32_t state )
{
	if( !GetStateMachine().IsSet() || state >= GetStateCount() )
	{
		return false;
	}
	auto path = m_graphPath;
	path.back().state = state;
	OpenGraph( std::move( path ) );
	return true;
}

bool EditorSession::LeaveSubgraph()
{
	if( m_graphPath.empty() )
	{
		return false;
	}
	auto path = m_graphPath;
	const auto last = path.back();
	if( last.state != NO_STATE )
	{
		// Back to the state machine view, with the state that was open selected
		path.back().state = NO_STATE;
		OpenGraph( std::move( path ) );
		SelectState( last.state );
		return true;
	}
	path.pop_back();
	OpenGraph( std::move( path ) );
	Select( last.node );
	return true;
}

const std::vector<cmf::AnimationNodeId>& EditorSession::GetSelection() const
{
	return m_selection;
}

bool EditorSession::IsSelected( cmf::AnimationNodeId node ) const
{
	return std::find( m_selection.begin(), m_selection.end(), node ) != m_selection.end();
}

void EditorSession::Select( cmf::AnimationNodeId node, bool add )
{
	m_selectedEdge.reset();
	if( !add )
	{
		m_selection.clear();
	}
	if( GetGraph().IsValid( node ) && !IsSelected( node ) )
	{
		m_selection.push_back( node );
	}
}

void EditorSession::Deselect( cmf::AnimationNodeId node )
{
	m_selection.erase( std::remove( m_selection.begin(), m_selection.end(), node ), m_selection.end() );
}

void EditorSession::SetSelection( std::vector<cmf::AnimationNodeId> nodes )
{
	m_selectedEdge.reset();
	const auto& graph = GetGraph();
	nodes.erase( std::remove_if( nodes.begin(), nodes.end(), [&graph]( cmf::AnimationNodeId id ) { return !graph.IsValid( id ); } ), nodes.end() );
	m_selection = std::move( nodes );
}

void EditorSession::SelectAll()
{
	if( GetStateMachine().IsSet() )
	{
		std::vector<uint32_t> states( GetStateCount() );
		std::iota( states.begin(), states.end(), 0u );
		SetSelectedStates( std::move( states ) );
		return;
	}
	SetSelection( GetGraph().GetNodes() );
}

void EditorSession::ClearSelection()
{
	ClearAllSelections();
}

const std::optional<cmf::AnimationSocketRef>& EditorSession::GetSelectedEdge() const
{
	return m_selectedEdge;
}

void EditorSession::SelectEdge( cmf::AnimationSocketRef input )
{
	m_selection.clear();
	m_selectedEdge = input;
}

uint32_t EditorSession::GetStateCount() const
{
	const auto stateMachine = GetStateMachine();
	if( !stateMachine.IsSet() )
	{
		return 0;
	}
	return static_cast<uint32_t>( GetGraph().GetSettings<cmf::AnimationStateMachineSettings>( stateMachine )->stateNames.size() );
}

const std::vector<uint32_t>& EditorSession::GetSelectedStates() const
{
	return m_selectedStates;
}

bool EditorSession::IsStateSelected( uint32_t state ) const
{
	return std::find( m_selectedStates.begin(), m_selectedStates.end(), state ) != m_selectedStates.end();
}

void EditorSession::SelectState( uint32_t state, bool add )
{
	m_selectedTransition.reset();
	if( !add )
	{
		m_selectedStates.clear();
	}
	if( state < GetStateCount() && !IsStateSelected( state ) )
	{
		m_selectedStates.push_back( state );
	}
}

void EditorSession::DeselectState( uint32_t state )
{
	m_selectedStates.erase( std::remove( m_selectedStates.begin(), m_selectedStates.end(), state ), m_selectedStates.end() );
}

void EditorSession::SetSelectedStates( std::vector<uint32_t> states )
{
	m_selectedTransition.reset();
	const auto count = GetStateCount();
	states.erase( std::remove_if( states.begin(), states.end(), [count]( uint32_t state ) { return state >= count; } ), states.end() );
	m_selectedStates = std::move( states );
}

const std::optional<uint32_t>& EditorSession::GetSelectedTransition() const
{
	return m_selectedTransition;
}

void EditorSession::SelectTransition( uint32_t transition )
{
	m_selectedStates.clear();
	m_selectedTransition = transition;
}

void EditorSession::ClearAllSelections()
{
	m_selection.clear();
	m_selectedEdge.reset();
	m_selectedStates.clear();
	m_selectedTransition.reset();
}

Snapshot EditorSession::Capture()
{
	Snapshot snapshot;
	snapshot.document = m_document->Serialize();
	snapshot.graphPath = m_document->LocatePath( m_graphPath );
	const auto& graph = GetGraph();
	for( const auto id : m_selection )
	{
		if( const auto location = m_document->Locate( graph, id ) )
		{
			snapshot.selection.push_back( *location );
		}
	}
	snapshot.selectedStates = m_selectedStates;
	snapshot.selectedTransition = m_selectedTransition;
	return snapshot;
}

void EditorSession::Restore( const Snapshot& snapshot )
{
	// Views and the export path are not part of the edit history: keep the current ones
	const auto exportPath = m_document->GetExportPath();
	std::vector<GraphView> views;
	std::vector<std::tuple<size_t, size_t, GraphView>> stateMachineViews;
	const auto& network = m_document->GetNetwork();
	const auto previousLayout = network.GetLayout();
	for( size_t g = 0; g < previousLayout.size(); ++g )
	{
		const auto& graph = *previousLayout[g].graph;
		views.push_back( m_document->GetView( graph ) );
		for( size_t n = 0; n < previousLayout[g].nodes.size(); ++n )
		{
			if( graph.GetSettings<cmf::AnimationStateMachineSettings>( previousLayout[g].nodes[n] ) != nullptr )
			{
				stateMachineViews.emplace_back( g, n, m_document->GetStateMachineView( graph, previousLayout[g].nodes[n] ) );
			}
		}
	}
	std::string error;
	if( !m_document->Deserialize( snapshot.document, error ) )
	{
		return;
	}
	m_document->SetExportPath( exportPath );
	const auto layout = network.GetLayout();
	for( size_t i = 0; i < layout.size() && i < views.size(); ++i )
	{
		m_document->SetView( *layout[i].graph, views[i] );
	}
	for( const auto& [g, n, view] : stateMachineViews )
	{
		if( g < layout.size() && n < layout[g].nodes.size() )
		{
			m_document->SetStateMachineView( *layout[g].graph, layout[g].nodes[n], view );
		}
	}
	m_graphPath = m_document->FindPath( snapshot.graphPath );
	ClearAllSelections();
	for( const auto& location : snapshot.selection )
	{
		const auto [graph, id] = m_document->Find( location );
		if( graph == &GetGraph() )
		{
			m_selection.push_back( id );
		}
	}
	SetSelectedStates( snapshot.selectedStates );
	const auto stateMachine = GetStateMachine();
	if( snapshot.selectedTransition && stateMachine.IsSet() && *snapshot.selectedTransition < GetGraph().GetSettings<cmf::AnimationStateMachineSettings>( stateMachine )->transitions.size() )
	{
		m_selectedTransition = snapshot.selectedTransition;
	}
}

void EditorSession::BeginEdit()
{
	if( m_editDepth++ == 0 )
	{
		m_editStart = Capture();
		m_editStartRevision = m_document->GetRevision();
	}
}

void EditorSession::EndEdit()
{
	if( m_editDepth == 0 )
	{
		return;
	}
	if( --m_editDepth == 0 && m_document->GetRevision() != m_editStartRevision )
	{
		m_history.Push( std::move( m_editStart ) );
		m_editStart = {};
	}
}

void EditorSession::CancelEdit()
{
	if( m_editDepth == 0 )
	{
		return;
	}
	m_editDepth = 0;
	if( m_document->GetRevision() != m_editStartRevision )
	{
		Restore( m_editStart );
	}
	m_editStart = {};
}

bool EditorSession::IsEditing() const
{
	return m_editDepth > 0;
}

bool EditorSession::CanUndo() const
{
	return m_history.CanUndo() && m_editDepth == 0;
}

bool EditorSession::CanRedo() const
{
	return m_history.CanRedo() && m_editDepth == 0;
}

bool EditorSession::Undo()
{
	if( !CanUndo() )
	{
		return false;
	}
	const auto snapshot = m_history.Undo( Capture() );
	Restore( *snapshot );
	return true;
}

bool EditorSession::Redo()
{
	if( !CanRedo() )
	{
		return false;
	}
	const auto snapshot = m_history.Redo( Capture() );
	Restore( *snapshot );
	return true;
}

bool EditorSession::CanEdit() const
{
	return !m_document->IsLocked();
}

cmf::AnimationNodeId EditorSession::AddNode( cmf::AnimationNodeType type, Vec2 position )
{
	if( !CanEdit() )
	{
		return {};
	}
	const ScopedEdit edit( *this );
	auto& graph = GetGraph();
	const auto id = graph.AddNode( type );
	if( id.IsSet() )
	{
		m_document->SetNodePosition( graph, id, position );
		Select( id );
	}
	return id;
}

void EditorSession::DeleteSelection()
{
	if( !CanEdit() )
	{
		return;
	}
	const ScopedEdit edit( *this );
	if( GetStateMachine().IsSet() )
	{
		DeleteStateSelection();
		return;
	}
	auto& graph = GetGraph();
	if( m_selectedEdge )
	{
		graph.Disconnect( *m_selectedEdge );
		m_selectedEdge.reset();
	}
	for( const auto id : m_selection )
	{
		graph.RemoveNode( id );
	}
	m_selection.clear();
}

void EditorSession::MoveNodes( const std::vector<cmf::AnimationNodeId>& nodes, Vec2 offset )
{
	if( !CanEdit() )
	{
		return;
	}
	const ScopedEdit edit( *this );
	const auto& graph = GetGraph();
	for( const auto id : nodes )
	{
		const auto position = m_document->GetNodePosition( graph, id ).value_or( Vec2{} );
		m_document->SetNodePosition( graph, id, { position.x + offset.x, position.y + offset.y } );
	}
}

cmf::ValidationResult EditorSession::Connect( cmf::AnimationSocketRef from, cmf::AnimationSocketRef to )
{
	if( !CanEdit() )
	{
		return { false, "The network can't be edited without a skeleton" };
	}
	BeginEdit();
	auto& graph = GetGraph();
	graph.Disconnect( to );
	auto result = graph.Connect( from, to );
	if( !result )
	{
		// Keep the previous connection of the input
		CancelEdit();
		return result;
	}
	EndEdit();
	return result;
}

bool EditorSession::Disconnect( cmf::AnimationSocketRef to )
{
	if( !CanEdit() )
	{
		return false;
	}
	const ScopedEdit edit( *this );
	if( m_selectedEdge && *m_selectedEdge == to )
	{
		m_selectedEdge.reset();
	}
	return GetGraph().Disconnect( to );
}

std::string EditorSession::CopySelection() const
{
	if( m_selection.empty() || GetStateMachine().IsSet() )
	{
		return {};
	}
	return CopyNodes( *m_document, GetGraph(), m_selection );
}

std::string EditorSession::CutSelection()
{
	auto text = CopySelection();
	if( !text.empty() )
	{
		DeleteSelection();
	}
	return text;
}

PasteResult EditorSession::Paste( std::string_view text, Vec2 origin )
{
	if( !CanEdit() )
	{
		PasteResult result;
		result.error = "The network can't be edited without a skeleton";
		return result;
	}
	if( GetStateMachine().IsSet() )
	{
		PasteResult result;
		result.error = "Nodes can't be pasted into a state machine; open a state to paste them into its graph";
		return result;
	}
	BeginEdit();
	auto result = PasteNodes( *m_document, GetGraph(), text, origin );
	if( !result.error.empty() )
	{
		CancelEdit();
		return result;
	}
	EndEdit();
	SetSelection( result.nodes );
	return result;
}

void EditorSession::AutoLayout()
{
	if( !CanEdit() )
	{
		return;
	}
	const ScopedEdit edit( *this );
	const auto stateMachine = GetStateMachine();
	if( stateMachine.IsSet() )
	{
		m_document->AutoLayoutStates( GetGraph(), stateMachine );
		return;
	}
	m_document->AutoLayout( GetGraph() );
}

void EditorSession::DeleteStateSelection()
{
	auto& graph = GetGraph();
	const auto stateMachine = GetStateMachine();
	if( m_selectedTransition )
	{
		static_cast<void>( graph.RemoveTransition( stateMachine, *m_selectedTransition ) );
		m_selectedTransition.reset();
	}
	// Remove from the last state, so that the indices of the remaining selected states don't shift
	auto states = m_selectedStates;
	std::sort( states.begin(), states.end(), std::greater<>() );
	for( const auto state : states )
	{
		static_cast<void>( graph.RemoveState( stateMachine, state ) );
	}
	m_selectedStates.clear();
}

uint32_t EditorSession::AddState( Vec2 position )
{
	const auto stateMachine = GetStateMachine();
	if( !CanEdit() || !stateMachine.IsSet() )
	{
		return NO_STATE;
	}
	const ScopedEdit edit( *this );
	auto& graph = GetGraph();
	const auto& names = graph.GetSettings<cmf::AnimationStateMachineSettings>( stateMachine )->stateNames;
	std::string name;
	for( size_t number = names.size() + 1;; ++number )
	{
		name = "State " + std::to_string( number );
		if( std::find( names.begin(), names.end(), name ) == names.end() )
		{
			break;
		}
	}
	const auto state = graph.AddState( stateMachine, name );
	if( state == cmf::AnimationGraphBuilder::INVALID_INDEX )
	{
		return NO_STATE;
	}
	// A new state starts with an Output node, like a new document
	auto& stateGraph = *graph.GetStateGraph( stateMachine, state );
	m_document->SetNodePosition( stateGraph, stateGraph.AddNode( cmf::AnimationNodeType::Output ), {} );
	m_document->SetStatePosition( stateGraph, position );
	SelectState( state );
	return state;
}

void EditorSession::MoveStates( const std::vector<uint32_t>& states, Vec2 offset )
{
	const auto stateMachine = GetStateMachine();
	if( !CanEdit() || !stateMachine.IsSet() )
	{
		return;
	}
	const ScopedEdit edit( *this );
	const auto& graph = GetGraph();
	for( const auto state : states )
	{
		if( const auto* stateGraph = graph.GetStateGraph( stateMachine, state ) )
		{
			const auto position = m_document->GetStatePosition( *stateGraph ).value_or( Vec2{} );
			m_document->SetStatePosition( *stateGraph, { position.x + offset.x, position.y + offset.y } );
		}
	}
}

uint32_t EditorSession::AddTransition( uint32_t from, uint32_t to )
{
	const auto stateMachine = GetStateMachine();
	if( !CanEdit() || !stateMachine.IsSet() )
	{
		return NO_STATE;
	}
	const ScopedEdit edit( *this );
	cmf::AnimationTransition transition;
	transition.fromState = from;
	transition.toState = to;
	const auto index = GetGraph().AddTransition( stateMachine, transition );
	if( index == cmf::AnimationGraphBuilder::INVALID_INDEX )
	{
		return NO_STATE;
	}
	SelectTransition( index );
	return index;
}

bool EditorSession::SetStartState( uint32_t state )
{
	const auto stateMachine = GetStateMachine();
	if( !CanEdit() || !stateMachine.IsSet() )
	{
		return false;
	}
	const ScopedEdit edit( *this );
	return static_cast<bool>( GetGraph().SetStartState( stateMachine, state ) );
}
}
