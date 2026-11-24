// Copyright © 2026 CCP ehf.

#include "history.h"

namespace canedit
{

History::History( size_t limit ) :
	m_limit( limit )
{
}

void History::Clear()
{
	m_undo.clear();
	m_redo.clear();
}

void History::Push( Snapshot before )
{
	m_redo.clear();
	m_undo.push_back( std::move( before ) );
	while( m_undo.size() > m_limit )
	{
		m_undo.pop_front();
	}
}

bool History::CanUndo() const
{
	return !m_undo.empty();
}

bool History::CanRedo() const
{
	return !m_redo.empty();
}

std::optional<Snapshot> History::Undo( Snapshot current )
{
	if( m_undo.empty() )
	{
		return std::nullopt;
	}
	auto snapshot = std::move( m_undo.back() );
	m_undo.pop_back();
	m_redo.push_back( std::move( current ) );
	return snapshot;
}

std::optional<Snapshot> History::Redo( Snapshot current )
{
	if( m_redo.empty() )
	{
		return std::nullopt;
	}
	auto snapshot = std::move( m_redo.back() );
	m_redo.pop_back();
	m_undo.push_back( std::move( current ) );
	return snapshot;
}

size_t History::GetUndoCount() const
{
	return m_undo.size();
}

}
