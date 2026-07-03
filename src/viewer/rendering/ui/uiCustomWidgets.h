// Copyright © 2026 CCP ehf.

#pragma once

#include <faLookup.h>
#include <vector>

#include "uiConsts.h"

namespace ImGui
{
enum class CheckBoxTriStateValue
{
	UNCHECKED = 0,
	CHECKED = 1,
	MIXED = -1
};

CheckBoxTriStateValue GetCheckedStatus( int64_t checked, int64_t count );

bool CheckBoxTristate( const char* label, CheckBoxTriStateValue* v_tristate );
bool FontAwesomeButton( const FaIcon& icon, int id = 0, float width = UiConsts::BUTTON_WIDTH, float height = UiConsts::BUTTON_HEIGHT );
bool FontAwesomeSlashedButton( const FaIcon& icon, int id = 0, float width = UiConsts::BUTTON_WIDTH, float height = UiConsts::BUTTON_HEIGHT );
void FontAwesomeText( const FaIcon& icon, float width );
const char* OpenCmfFileDialog();

template <typename Callable>
void ComboBox( const char* name, const std::vector<std::string>& items, int32_t selectedIndex, Callable onChange )
{
	if( items.empty() )
	{
		if( ImGui::BeginCombo( name, "N/A" ) )
		{
			ImGui::EndCombo();
		}
		return;
	}

	if( selectedIndex < 0 || selectedIndex >= static_cast<int32_t>( items.size() ) )
	{
		selectedIndex = 0;
	}

	if( ImGui::BeginCombo( name, items[selectedIndex].c_str() ) )
	{
		for( int32_t itemIndex = 0; itemIndex < static_cast<int32_t>( items.size() ); ++itemIndex )
		{
			const bool isSelected = ( itemIndex == selectedIndex );
			if( ImGui::Selectable( items[itemIndex].c_str(), isSelected ) )
			{
				onChange( itemIndex );
			}

			if( isSelected )
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}
}