
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
void State<T>::CallCallbacks( const AppState& appstate)
{
	if( m_fireCallbacks )
	{
		for( auto& callback : m_callbacks )
		{
			callback( m_value, appstate );
		}
		m_fireCallbacks = false;
	}
}

template <typename T>
void State<T>::RegisterCallback( std::function<void( T, const AppState& )> callback )
{
	m_callbacks.push_back( callback );
}