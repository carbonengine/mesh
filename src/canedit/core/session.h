// Copyright © 2026 CCP ehf.

#pragma once

#include "clipboard.h"
#include "document.h"
#include "history.h"

#include <optional>
#include <string>
#include <vector>

namespace canedit
{

/** Editing state on top of a Document: the open graph, the selection and undo history, and the editing operations
* of the editor. Every modification goes through an edit (BeginEdit / EndEdit), which records an undo step if the
* document changed. Edits nest, so that a mouse drag or an edit of a text field becomes a single undo step. */
class EditorSession
{
public:
	explicit EditorSession( Document& document );

	[[nodiscard]] Document& GetDocument();
	[[nodiscard]] const Document& GetDocument() const;

	/** Clears the history and the selection, for example after a document was loaded. */
	void Reset();

	// Navigation

	[[nodiscard]] const GraphPath& GetGraphPath() const;
	/** Returns the open graph (the root graph if the path became invalid). */
	[[nodiscard]] cmf::AnimationGraphBuilder& GetGraph();
	[[nodiscard]] const cmf::AnimationGraphBuilder& GetGraph() const;
	void OpenGraph( GraphPath path );
	/** Opens the graph of a Subgraph node of the open graph. */
	bool EnterSubgraph( cmf::AnimationNodeId node );
	/** Opens the parent of the open graph. */
	bool LeaveSubgraph();

	// Selection (nodes of the open graph, or one edge)

	[[nodiscard]] const std::vector<cmf::AnimationNodeId>& GetSelection() const;
	[[nodiscard]] bool IsSelected( cmf::AnimationNodeId node ) const;
	void Select( cmf::AnimationNodeId node, bool add = false );
	void Deselect( cmf::AnimationNodeId node );
	void SetSelection( std::vector<cmf::AnimationNodeId> nodes );
	void SelectAll();
	void ClearSelection();
	/** The selected edge, identified by its input socket. Selecting an edge clears the node selection. */
	[[nodiscard]] const std::optional<cmf::AnimationSocketRef>& GetSelectedEdge() const;
	void SelectEdge( cmf::AnimationSocketRef input );

	// Edits and history

	/** Starts an edit; the outermost BeginEdit captures the state to restore on undo. */
	void BeginEdit();
	/** Ends an edit; the outermost EndEdit records an undo step if the document changed. */
	void EndEdit();
	/** Ends the outermost edit and restores the state captured by BeginEdit, for example when an edit failed. */
	void CancelEdit();
	[[nodiscard]] bool IsEditing() const;

	[[nodiscard]] bool CanUndo() const;
	[[nodiscard]] bool CanRedo() const;
	bool Undo();
	bool Redo();

	/** Returns false if the document can't be edited because its skeleton is missing. */
	[[nodiscard]] bool CanEdit() const;

	// Operations; each is a single undo step

	/** Adds a node at a position and selects it. Returns an unset handle if the node can't be added here. */
	cmf::AnimationNodeId AddNode( cmf::AnimationNodeType type, Vec2 position );
	/** Deletes the selected nodes or the selected edge. */
	void DeleteSelection();
	/** Moves nodes by an offset. Call within an edit to combine a drag into one undo step. */
	void MoveNodes( const std::vector<cmf::AnimationNodeId>& nodes, Vec2 offset );
	/** Connects an output to an input, replacing an existing connection of the input. */
	cmf::ValidationResult Connect( cmf::AnimationSocketRef from, cmf::AnimationSocketRef to );
	bool Disconnect( cmf::AnimationSocketRef to );

	/** Returns clipboard text for the selected nodes, empty if nothing is selected. */
	[[nodiscard]] std::string CopySelection() const;
	/** Copies and deletes the selected nodes. */
	std::string CutSelection();
	/** Pastes clipboard text into the open graph at a position and selects the pasted nodes. */
	PasteResult Paste( std::string_view text, Vec2 origin );

	/** Arranges the nodes of the open graph automatically. */
	void AutoLayout();

private:
	[[nodiscard]] Snapshot Capture();
	void Restore( const Snapshot& snapshot );

	Document* m_document;
	History m_history;
	GraphPath m_graphPath;
	std::vector<cmf::AnimationNodeId> m_selection;
	std::optional<cmf::AnimationSocketRef> m_selectedEdge;
	int m_editDepth = 0;
	Snapshot m_editStart;
	uint64_t m_editStartRevision = 0;
};

/** Begins an edit for the lifetime of the object. */
class ScopedEdit
{
public:
	explicit ScopedEdit( EditorSession& session ) :
		m_session( &session )
	{
		m_session->BeginEdit();
	}
	~ScopedEdit()
	{
		m_session->EndEdit();
	}
	ScopedEdit( const ScopedEdit& ) = delete;
	ScopedEdit& operator=( const ScopedEdit& ) = delete;
	ScopedEdit( ScopedEdit&& ) = delete;
	ScopedEdit& operator=( ScopedEdit&& ) = delete;

private:
	EditorSession* m_session;
};

}
