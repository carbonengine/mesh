// Copyright © 2026 CCP ehf.

template <typename T>
void UIRenderer::SetupCombo( const char* name, UIRenderer::CmfUiComboBox<T>& combo, State<T>& applicableState )
{
	if( ImGui::BeginCombo( name, combo.selectedItemName.c_str() ) )
	{
		for( const auto& nameValue : combo.items )
		{
			bool is_selected = ( nameValue.second == combo.selectedItemValue );

			if( ImGui::Selectable( nameValue.first.c_str(), is_selected ) )
			{
				combo.SetSelectedItemByValue( nameValue.second );
				applicableState.SetValue( nameValue.second );
			}

			if( is_selected )
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

template <typename T>
void UIRenderer::CmfUiComboBox<T>::SetSelectedItemByValue( T value )
{
	for( const auto& item : items )
	{
		if( item.second == value )
		{
			selectedItemName = item.first;
			selectedItemValue = item.second;
			return;
		}
	}
	if( !items.empty() )
	{
		selectedItemName = items.front().first;
		selectedItemValue = items.front().second;
	}
}

template <typename Callable>
void UIRenderer::SetupModelAxisRow( std::vector<std::pair<uint32_t, ImGui::CheckBoxTriStateValue>>& checkboxStates, const std::string& name, const Callable& changeCallback )
{
	int32_t maxIndex = checkboxStates.empty() ? -1 : std::max_element( checkboxStates.begin(), checkboxStates.end(), []( const std::pair<uint32_t, ImGui::CheckBoxTriStateValue>& a, const std::pair<uint32_t, ImGui::CheckBoxTriStateValue>& b ) {
														 return a.first < b.first;
													 } )->first;

	ImGui::BeginDisabled( maxIndex < 0 );
	ImGui::TableNextColumn();
	ImGui::Text( "%s", name.c_str() );
	ImGui::SetItemTooltip( "Toggles the %s visualization for all meshes", name.c_str() );
	ImGui::TableNextColumn();

	if( maxIndex >= 0 )
	{
		for( int32_t i = 0; i <= maxIndex; ++i )
		{
			auto foundState = std::find_if( checkboxStates.begin(), checkboxStates.end(), [i]( const std::pair<uint32_t, ImGui::CheckBoxTriStateValue>& pair ) {
				return pair.first == (uint32_t)i;
			} );

			if( foundState == checkboxStates.end() )
			{
				// add a disabled checkbox
				ImGui::BeginDisabled( true );
				auto disabledValue = ImGui::CheckBoxTriStateValue::UNCHECKED;
				ImGui::CheckBoxTristate( ( std::string( "##tricheckbox" ) + name + std::to_string( i ) ).c_str(), &disabledValue );

				ImGui::SetItemTooltip( "No mesh has %s information for usage %d", name.c_str(), i );
				ImGui::SameLine();
				ImGui::EndDisabled();
			}
			else
			{
				auto value = foundState->second;
				OnChange( ImGui::CheckBoxTristate( ( std::string( "##tricheckbox" ) + name + std::to_string( i ) ).c_str(), &value ), [changeCallback, i, value]() {
					changeCallback( value != ImGui::CheckBoxTriStateValue::CHECKED, i );
				} );
				ImGui::SetItemTooltip( "Toggles debug visualization for %s with usage index %d for all meshes (if available)", name.c_str(), i );
				ImGui::SameLine();
			}
		}
	}
	ImGui::EndDisabled();
}