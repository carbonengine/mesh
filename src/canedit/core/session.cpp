// Copyright © 2026 CCP ehf.

#include "session.h"

#include <algorithm>

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
	m_selection.clear();
	m_selectedEdge.reset();
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
		m_selection.clear();
		m_selectedEdge.reset();
		graph = &m_document->GetNetwork().GetRootGraph();
	}
	return *graph;
}

const cmf::AnimationGraphBuilder& EditorSession::GetGraph() const
{
	const auto* graph = m_document->FindGraph( m_graphPath );
	return graph != nullptr ? *graph : m_document->GetNetwork().GetRootGraph();
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
		m_selection.clear();
		m_selectedEdge.reset();
	}
}

bool EditorSession::EnterSubgraph( cmf::AnimationNodeId node )
{
	if( GetGraph().GetSubgraph( node ) == nullptr )
	{
		return false;
	}
	auto path = m_graphPath;
	path.push_back( node );
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
	const auto subgraphNode = path.back();
	path.pop_back();
	OpenGraph( std::move( path ) );
	Select( subgraphNode );
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
	SetSelection( GetGraph().GetNodes() );
}

void EditorSession::ClearSelection()
{
	m_selection.clear();
	m_selectedEdge.reset();
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
	return snapshot;
}

void EditorSession::Restore( const Snapshot& snapshot )
{
	// Views and the export path are not part of the edit history: keep the current ones
	const auto exportPath = m_document->GetExportPath();
	std::vector<GraphView> views;
	const auto& network = m_document->GetNetwork();
	for( const auto& layout : network.GetSaveLayout() )
	{
		views.push_back( m_document->GetView( *layout.graph ) );
	}
	std::string error;
	if( !m_document->Deserialize( snapshot.document, error ) )
	{
		return;
	}
	m_document->SetExportPath( exportPath );
	const auto layout = network.GetSaveLayout();
	for( size_t i = 0; i < layout.size() && i < views.size(); ++i )
	{
		m_document->SetView( *layout[i].graph, views[i] );
	}
	m_graphPath = m_document->FindPath( snapshot.graphPath );
	m_selection.clear();
	m_selectedEdge.reset();
	for( const auto& location : snapshot.selection )
	{
		const auto [graph, id] = m_document->Find( location );
		if( graph == &GetGraph() )
		{
			m_selection.push_back( id );
		}
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
	if( m_selection.empty() )
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
	m_document->AutoLayout( GetGraph() );
}

}
