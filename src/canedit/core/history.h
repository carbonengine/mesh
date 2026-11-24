// Copyright © 2026 CCP ehf.

#pragma once

#include "document.h"

#include <deque>
#include <optional>
#include <vector>

namespace canedit
{

/** State of an editing session that undo and redo restore. */
struct Snapshot
{
	/** The network with its editor data, as written by Document::Serialize. */
	std::vector<uint8_t> file;
	/** The graph that is open in the editor. */
	std::vector<NodeLocation> graphPath;
	/** The selected nodes of the open graph. */
	std::vector<NodeLocation> selection;
};

/** Undo and redo stacks of snapshots. */
class History
{
public:
	static constexpr size_t DEFAULT_LIMIT = 200;

	explicit History( size_t limit = DEFAULT_LIMIT );

	void Clear();
	/** Records the state before an edit. Clears the redo stack. The oldest state is dropped beyond the limit. */
	void Push( Snapshot before );

	[[nodiscard]] bool CanUndo() const;
	[[nodiscard]] bool CanRedo() const;

	/** Returns the state to restore for undo and keeps the current state for redo. */
	std::optional<Snapshot> Undo( Snapshot current );
	/** Returns the state to restore for redo and keeps the current state for undo. */
	std::optional<Snapshot> Redo( Snapshot current );

	[[nodiscard]] size_t GetUndoCount() const;

private:
	std::deque<Snapshot> m_undo;
	std::deque<Snapshot> m_redo;
	size_t m_limit;
};

}
