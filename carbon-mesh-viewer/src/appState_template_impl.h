
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
void State<T>::CallCallbacks( AppState& appState )
{
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
	return m_states.size() - 1;
}

template <typename T>
const T StateCollection<T>::GetValue( size_t index ) const
{
	if( index >= m_states.size() )
	{
		return m_initialValue;
	}
	return m_states[index].GetValue();
}

template <typename T>
void StateCollection<T>::SetValue( size_t index, T newValue )
{
	m_states[index].SetValue( newValue );
}

template <typename T>
void StateCollection<T>::ForceSetValue( size_t index, T newValue )
{
	m_states[index].ForceSetValue( newValue );
}

template <typename T>
void StateCollection<T>::SetValueNoCallback( size_t index, T newValue )
{
	m_states[index].SetValueNoCallback( newValue );
}

template <typename T>
void StateCollection<T>::RegisterCallback( size_t index, std::function<void( T, AppState& )> callback )
{
	m_states[index].RegisterCallback( callback );
}

template <typename T>
void StateCollection<T>::CallCallbacks( AppState& appState )
{
	for( auto& state : m_states )
	{
		state.CallCallbacks( appState );
	}
}

template <typename T>
void StateCollection<T>::Clear()
{
    m_states.clear();
}