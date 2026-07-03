// Copyright © 2026 CCP ehf.
#include "uiCustomWidgets.h"

#include <faLookup.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "uiConsts.h"

// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
namespace ImGui
{

// threeway checkbox taken from https://github.com/ocornut/imgui/issues/2644
bool CheckBoxTristate( const char* label, CheckBoxTriStateValue* v_tristate )
{
	bool ret;
	if( *v_tristate == CheckBoxTriStateValue::MIXED )
	{
		ImGui::PushItemFlag( ImGuiItemFlags_MixedValue, true );
		bool b = false;
		ret = ImGui::Checkbox( label, &b );
		if( ret )
		{
			*v_tristate = CheckBoxTriStateValue::CHECKED;
		}
		ImGui::PopItemFlag();
	}
	else
	{
		bool b = ( *v_tristate != CheckBoxTriStateValue::UNCHECKED );
		ret = ImGui::Checkbox( label, &b );
		if( ret )
		{
			*v_tristate = (CheckBoxTriStateValue)(int)b;
		}
	}
	return ret;
}

CheckBoxTriStateValue GetCheckedStatus( int64_t checked, int64_t count )
{
	if( checked == 0 )
	{
		return CheckBoxTriStateValue::UNCHECKED;
	}
	else if( checked == count )
	{
		return CheckBoxTriStateValue::CHECKED;
	}
	else
	{
		return CheckBoxTriStateValue::MIXED;
	}
}


bool FontAwesomeButton( const FaIcon& icon, int id, float width, float height )
{
	const float faButtonPadding = 4.0f;
	const float glyphWidth = UiConsts::FONT_AWESOME_SIZE * icon.xyRatio;
	const float paddingX = std::max( 0.0f, ( width - glyphWidth ) * 0.5f );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( paddingX, faButtonPadding ) );
	ImGui::PushID( id );
	bool ret = ImGui::Button( icon.text, ImVec2( width, height ) );
	ImGui::PopID();
	ImGui::PopStyleVar();

	return ret;
}

void FontAwesomeText( const FaIcon& icon, float width )
{
	const float faButtonPadding = -4.0f;
	const float glyphWidth = UiConsts::FONT_AWESOME_SIZE * icon.xyRatio;
	const float paddingX = std::max( 0.0f, ( width - glyphWidth ) * 0.5f );
	ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( paddingX, faButtonPadding ) );
	ImGui::Text( icon.text );
	ImGui::PopStyleVar();
}

bool FontAwesomeSlashedButton( const FaIcon& icon, int id, float width, float height )
{
	bool ret = ImGui::FontAwesomeButton( icon, id, width, height );
	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	float slashPadding = 1.5f;
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2( min.x + slashPadding, max.y - slashPadding ),
		ImVec2( max.x - slashPadding, min.y + slashPadding ),
		ImGui::GetColorU32( ImGuiCol_Text ),
		2.0f );

	return ret;
}

const char* OpenCmfFileDialog()
{
	char const* filter[1] = { "*.cmf" };
	return tinyfd_openFileDialog(
		"Open CMF File",
		NULL,
		1,
		filter,
		"CMF Files",
		0 );
}
}
// NOLINTEND(cppcoreguidelines-pro-type-vararg)