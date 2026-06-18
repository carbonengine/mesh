// Copyright © 2026 CCP ehf.

template <typename Decl>
std::vector<UIRenderer::AttributeInfo> UIRenderer::BuildAttributes( const Decl& decl )
{
	std::vector<AttributeInfo> attributes;
	for( const auto& elem : decl )
	{
		auto conv = cmf::GetScalarConversionFunction<float>( elem.type );
		if( !conv.first.to )
			continue;
		attributes.push_back( { GetUsageFlagLabel( elem.usage, elem.usageIndex ),
								elem.offset,
								std::min( elem.elementCount, uint8_t( 4 ) ),
								conv } );
	}
	return attributes;
}

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