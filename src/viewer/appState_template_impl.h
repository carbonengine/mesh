// Copyright © 2026 CCP ehf.

template <typename T>
State<T>::State( T initialValue ) :
	m_value( initialValue ), m_initialValue( initialValue )
{
}


template <typename T>
const T State<T>::GetValue() const
{
	return m_value;
}

template <typename T>
void State<T>::Reset()
{
	m_value = m_initialValue;
	m_fireCallbacks = true;
}

template <typename T>
void State<T>::SetValue( T newValue )
{
	if( m_value == newValue )
	{
		return;
	}

	m_value = newValue;
	m_fireCallbacks = true;
}

template <typename T>
void State<T>::ForceSetValue( T newValue )
{
	m_value = newValue;
	m_fireCallbacks = true;
}

template <typename T>
void State<T>::SetValueNoCallback( T newValue )
{
	m_value = newValue;
}

template <typename T>
T& State<T>::GetValue()
{
	return m_value;
}

namespace detail
{
template <typename T, typename = void>
struct has_call_callbacks : std::false_type
{
};

template <typename T>
struct has_call_callbacks<T, std::void_t<decltype( std::declval<T&>().CallCallbacks( std::declval<AppState&>() ) )>> : std::true_type
{
};
}

template <typename T>
void State<T>::CallCallbacks( AppState& appState )
{
	if constexpr( detail::has_call_callbacks<T>::value )
	{
		m_value.CallCallbacks( appState );
	}

	if( m_fireCallbacks )
	{
		for( auto& callback : m_callbacks )
		{
			callback( m_value, appState );
		}
		m_fireCallbacks = false;
	}
}

template <typename T>
void State<T>::RegisterCallback( std::function<void( T, AppState& )> callback )
{
	m_callbacks.push_back( callback );
}

template <typename T>
StateCollection<T>::StateCollection( T initialValue ) :
	m_initialValue( initialValue )
{
}

template <typename T>
size_t StateCollection<T>::AddState()
{
	State<T> state( m_initialValue );
	m_states.push_back( state );
	// the vector changed, fire the callback
	m_fireCallbacks = true;
	return m_states.size() - 1;
}

template <typename T>
size_t StateCollection<T>::AddState( T initialValue )
{
	State<T> state( initialValue );
	m_states.push_back( state );
	// the vector changed, fire the callback
	m_fireCallbacks = true;

	return m_states.size() - 1;
}

template <typename T>
template <typename Callable>
size_t StateCollection<T>::AddState( Callable configurator )
{
	State<T> state( m_initialValue );
	m_states.push_back( state );
	configurator( m_states.back().GetValue() );
	m_fireCallbacks = true;
	return m_states.size() - 1;
}

template <typename T>
template <typename Callable>
size_t StateCollection<T>::AddState( T initialValue, Callable configurator )
{
	State<T> state( initialValue );
	m_states.push_back( state );
	configurator( m_states.back().GetValue() );
	m_fireCallbacks = true;
	return m_states.size() - 1;
}

template <typename T>
void StateCollection<T>::RemoveAt( size_t index )
{
	if( index < m_states.size() )
	{
		m_states.erase( m_states.begin() + index );
	}
	// fire local callbacks to notify about the change in the collection
	m_fireCallbacks = true;
}

template <typename T>
void StateCollection<T>::RegisterCallback( std::function<void( std::vector<T>, AppState& )> callback )
{
	m_callbacks.push_back( callback );
}

template <typename T>
void StateCollection<T>::CallCallbacks( AppState& appState )
{
	for( auto& state : m_states )
	{
		state.CallCallbacks( appState );
	}

	std::vector<T> values;
	values.reserve( m_states.size() );
	for( auto& state : m_states )
	{
		values.push_back( state.GetValue() );
	}

	if( m_fireCallbacks )
	{
		for( auto& callback : m_callbacks )
		{
			callback( values, appState );
		}
		m_fireCallbacks = false;
	}
}

template <typename T>
void StateCollection<T>::Clear()
{
	if( !m_states.empty() )
	{
		m_states.clear();
		// fire local callbacks to notify about the change in the collection
		m_fireCallbacks = true;
	}
}

template <typename T>
size_t StateCollection<T>::size() const
{
	return m_states.size();
}

template <typename T>
bool StateCollection<T>::empty() const
{
	return m_states.empty();
}

// Non-const iterators
template <typename T>
typename StateCollection<T>::iterator StateCollection<T>::begin()
{
	return m_states.begin();
}

template <typename T>
typename StateCollection<T>::iterator StateCollection<T>::end()
{
	return m_states.end();
}

// Const iterators
template <typename T>
typename StateCollection<T>::const_iterator StateCollection<T>::begin() const
{
	return m_states.begin();
}

template <typename T>
typename StateCollection<T>::const_iterator StateCollection<T>::end() const
{
	return m_states.end();
}

template <typename T>
typename StateCollection<T>::const_iterator StateCollection<T>::cbegin() const
{
	return m_states.cbegin();
}

template <typename T>
typename StateCollection<T>::const_iterator StateCollection<T>::cend() const
{
	return m_states.cend();
}

// Indexing operators
template <typename T>
State<T>& StateCollection<T>::operator[]( size_t index )
{
	return m_states[index];
}

template <typename T>
const State<T>& StateCollection<T>::operator[]( size_t index ) const
{
	return m_states[index];
}
