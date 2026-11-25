// Copyright © 2026 CCP ehf.

#include "panels.h"

#include "fileDialogs.h"
#include "graphCanvas.h"
#include "nodeInfo.h"
#include "preview.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

// ImGui uses C-style variadic functions for formatted text
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace canedit
{
namespace
{

constexpr float DRAG_SPEED = 0.01f;
constexpr float DEFAULT_RANGE_MAX = 1.f;
constexpr ImVec4 ERROR_TEXT = { 1.f, 0.4f, 0.35f, 1.f };
constexpr ImVec4 WARNING_TEXT = { 1.f, 0.75f, 0.3f, 1.f };
constexpr ImVec4 HINT_TEXT = { 0.65f, 0.65f, 0.65f, 1.f };
constexpr ImVec4 OK_TEXT = { 0.5f, 0.9f, 0.5f, 1.f };
/** Height of multiline text fields, in text lines. */
constexpr float MULTILINE_LINES = 4.f;
/** Width of the category color swatch in the palette, relative to the font size. */
constexpr float SWATCH_WIDTH = 0.6f;
/** Space left next to a reference field for its buttons, in frame heights. */
constexpr float BROWSE_BUTTON_SPACE = 3.f;
constexpr float BROWSE_CLEAR_BUTTONS_SPACE = 5.f;
/** Space left next to a bone weight for its remove button, in frame heights. */
constexpr float REMOVE_BUTTON_SPACE = 1.5f;
/** Width of the value column of the parameter table relative to the name column. */
constexpr float VALUE_COLUMN_WEIGHT = 1.5f;
constexpr float HALF = 0.5f;
/** Frame padding is applied on both sides of a widget. */
constexpr float TWO_SIDES = 2.f;

/** Drag speed of integer fields, in values per pixel. */
constexpr float INTEGER_DRAG_SPEED = 0.1f;
/** Largest magnitude of integer fields: 2^24, up to which floats hold every whole number exactly. */
constexpr int MAX_INTEGER = 1 << 24;

/** Shows the control for the constant of a Float input that fits its kind of value. Returns the new value when it
* changed. */
std::optional<float> InputConstantField( const char* label, cmf::AnimationValueKind kind, float value )
{
	switch( kind )
	{
	case cmf::AnimationValueKind::Boolean: {
		// Networks treat any value other than 0 as true
		bool checked = value != 0.f;
		if( ImGui::Checkbox( label, &checked ) )
		{
			return checked ? 1.f : 0.f;
		}
		return std::nullopt;
	}
	case cmf::AnimationValueKind::Integer: {
		// Networks truncate fractional values; out of range values are shown clamped
		const auto limit = static_cast<float>( MAX_INTEGER );
		int whole = std::isfinite( value ) ? static_cast<int>( std::clamp( std::trunc( value ), -limit, limit ) ) : 0;
		if( ImGui::DragInt( label, &whole, INTEGER_DRAG_SPEED, -MAX_INTEGER, MAX_INTEGER ) )
		{
			return static_cast<float>( whole );
		}
		return std::nullopt;
	}
	case cmf::AnimationValueKind::Number:
		break;
	}
	if( ImGui::DragFloat( label, &value, DRAG_SPEED ) )
	{
		return value;
	}
	return std::nullopt;
}

/** Shows a text field that applies its value when editing ends. Returns true and sets result when it does. */
bool CommitText( std::map<std::string, std::string>& texts, const std::string& key, const char* label, const std::string& current, std::string& result, bool multiline = false )
{
	auto& buffer = texts[key];
	if( ImGui::GetActiveID() != ImGui::GetID( label ) )
	{
		buffer = current;
	}
	if( multiline )
	{
		ImGui::InputTextMultiline( label, &buffer, { -1.f, ImGui::GetTextLineHeight() * MULTILINE_LINES } );
	}
	else
	{
		ImGui::InputText( label, &buffer );
	}
	if( ImGui::IsItemDeactivatedAfterEdit() && buffer != current )
	{
		result = buffer;
		return true;
	}
	return false;
}

std::string Join( const std::vector<std::string>& names )
{
	std::string result;
	for( const auto& name : names )
	{
		result += result.empty() ? name : ", " + name;
	}
	return result;
}

std::vector<std::string> SplitNames( const std::string& text )
{
	std::vector<std::string> names;
	std::stringstream stream( text );
	std::string name;
	while( std::getline( stream, name, ',' ) )
	{
		const auto first = name.find_first_not_of( " \t" );
		const auto last = name.find_last_not_of( " \t" );
		if( first != std::string::npos )
		{
			names.push_back( name.substr( first, last - first + 1 ) );
		}
	}
	return names;
}

/** Combo listing names, with an entry for an empty name. Returns true when the selection changed. */
bool NameCombo( const char* label, const std::string& current, const std::vector<std::string>& names, const char* emptyName, std::string& result )
{
	bool changed = false;
	const auto preview = current.empty() ? std::string( emptyName ) : current;
	if( ImGui::BeginCombo( label, preview.c_str() ) )
	{
		if( ImGui::Selectable( emptyName, current.empty() ) )
		{
			result.clear();
			changed = true;
		}
		for( const auto& name : names )
		{
			if( ImGui::Selectable( name.c_str(), name == current ) )
			{
				result = name;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed && result != current;
}

std::vector<std::string> ToNames( const cmf::Span<cmf::String>& strings )
{
	std::vector<std::string> names;
	for( const auto& string : strings )
	{
		names.push_back( cmf::ToStdString( string ) );
	}
	return names;
}

void ReportResult( AppContext& context, const cmf::ValidationResult& result )
{
	if( !result )
	{
		context.SetStatus( result.error, true );
	}
}

/** Asks for a CMF file and returns its reference, using a prefix if the file is in a prefix directory. */
std::optional<std::string> BrowseReference( AppContext& context, const char* title )
{
	const auto path = dialogs::OpenCmf( title );
	if( !path )
	{
		return std::nullopt;
	}
	return context.resolver->MakeReference( std::filesystem::u8path( *path ) );
}

void SectionHeader( const char* text )
{
	ImGui::Spacing();
	ImGui::SeparatorText( text );
}

}

// Palette

void PalettePanel::Draw( AppContext& context, GraphCanvas& canvas )
{
	auto& session = *context.session;
	const auto& graph = session.GetGraph();
	ImGui::SetNextItemWidth( -1.f );
	ImGui::InputTextWithHint( "##paletteFilter", "Search node types", &m_filter );
	std::string filter = m_filter;
	std::transform( filter.begin(), filter.end(), filter.begin(), []( char c ) { return static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) ); } );

	ImGui::BeginDisabled( !session.CanEdit() );
	ImGui::BeginChild( "##paletteList" );
	for( const auto& category : GetNodeCategories() )
	{
		bool header = false;
		for( const auto type : category.types )
		{
			const char* name = cmf::GetAnimationNodeTypeInfo( type ).name;
			std::string lower = name;
			std::transform( lower.begin(), lower.end(), lower.begin(), []( char c ) { return static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) ); } );
			if( !filter.empty() && lower.find( filter ) == std::string::npos )
			{
				continue;
			}
			if( !header )
			{
				ImGui::TextDisabled( "%s", category.name );
				header = true;
			}
			const bool allowed = CanAddNode( graph, type );
			ImGui::BeginDisabled( !allowed );
			ImGui::PushID( name );
			const auto color = ImGui::ColorConvertU32ToFloat4( category.color );
			ImGui::ColorButton( "##color", color, ImGuiColorEditFlags_NoTooltip, { ImGui::GetFontSize() * SWATCH_WIDTH, ImGui::GetFontSize() } );
			ImGui::SameLine();
			if( ImGui::Selectable( name ) )
			{
				session.AddNode( type, canvas.GetViewCenter() );
			}
			if( ImGui::BeginDragDropSource() )
			{
				const auto value = static_cast<uint32_t>( type );
				ImGui::SetDragDropPayload( NODE_TYPE_PAYLOAD, &value, sizeof( value ) );
				ImGui::Text( "%s", name );
				ImGui::EndDragDropSource();
			}
			if( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled ) )
			{
				ImGui::SetTooltip( "%s%s", GetNodeDescription( type ), allowed ? "" : "\nNot available in this graph." );
			}
			ImGui::PopID();
			ImGui::EndDisabled();
		}
	}
	ImGui::EndChild();
	ImGui::EndDisabled();
}

// Properties

void PropertiesPanel::Draw( AppContext& context )
{
	auto& session = *context.session;
	auto& graph = session.GetGraph();
	const auto& selection = session.GetSelection();
	if( selection.size() == 1 && graph.IsValid( selection.front() ) )
	{
		DrawNode( context, graph, selection.front() );
	}
	else if( selection.size() > 1 )
	{
		ImGui::Text( "%zu nodes selected", selection.size() );
	}
	else
	{
		DrawNetwork( context );
	}
}

void PropertiesPanel::DrawNetwork( AppContext& context )
{
	auto& session = *context.session;
	auto& document = *context.document;
	auto& network = document.GetNetwork();

	SectionHeader( "Skeleton" );
	if( document.IsLocked() )
	{
		ImGui::TextColored( ERROR_TEXT, "%s", network.GetSkeletonPath().empty() ? "Set the skeleton the network animates." : "The skeleton is missing. Fix the reference to edit the network." );
	}
	std::string reference;
	ImGui::SetNextItemWidth( -ImGui::GetFrameHeight() * BROWSE_BUTTON_SPACE );
	if( CommitText( m_text, "skeleton", "##skeleton", network.GetSkeletonPath(), reference ) )
	{
		const ScopedEdit edit( session );
		network.SetSkeleton( reference, network.GetSkeletonName() );
	}
	ImGui::SameLine();
	if( ImGui::Button( "Browse##skeleton" ) )
	{
		if( const auto browsed = BrowseReference( context, "Select Skeleton File" ) )
		{
			const ScopedEdit edit( session );
			network.SetSkeleton( *browsed, "" );
		}
	}
	std::vector<std::string> skeletons;
	if( const auto* data = network.GetAssetData( network.GetSkeletonPath() ) )
	{
		for( const auto& skeleton : data->skeletons )
		{
			skeletons.push_back( cmf::ToStdString( skeleton.name ) );
		}
	}
	std::string name;
	ImGui::SetNextItemWidth( -1.f );
	if( NameCombo( "##skeletonName", network.GetSkeletonName(), skeletons, "(first skeleton)", name ) )
	{
		const ScopedEdit edit( session );
		network.SetSkeleton( network.GetSkeletonPath(), name );
	}

	SectionHeader( "Preview Meshes" );
	ImGui::TextColored( HINT_TEXT, "Meshes shown together in the preview, stored with the editor data." );
	// Edits apply to a copy, so that the list isn't changed while it is drawn
	auto meshes = document.GetPreviewMeshes();
	bool meshesChanged = false;
	for( size_t i = 0; i < meshes.size(); ++i )
	{
		ImGui::PushID( static_cast<int>( i ) );
		ImGui::SetNextItemWidth( -ImGui::GetFrameHeight() * BROWSE_CLEAR_BUTTONS_SPACE );
		if( CommitText( m_text, "previewMesh" + std::to_string( i ), "##previewMesh", meshes[i], reference ) )
		{
			meshes[i] = reference;
			meshesChanged = true;
		}
		ImGui::SameLine();
		if( ImGui::Button( "Browse" ) )
		{
			if( const auto browsed = BrowseReference( context, "Select Preview Mesh" ) )
			{
				meshes[i] = *browsed;
				meshesChanged = true;
			}
		}
		ImGui::SameLine();
		if( ImGui::Button( "Remove" ) )
		{
			meshes[i].clear(); // Empty references are dropped from the list
			meshesChanged = true;
		}
		ImGui::PopID();
	}
	if( meshes.empty() )
	{
		ImGui::TextColored( HINT_TEXT, "None: the preview shows the skeleton file." );
	}
	if( ImGui::Button( "Add Mesh..." ) )
	{
		if( const auto browsed = BrowseReference( context, "Add Preview Mesh" ) )
		{
			meshes.push_back( *browsed );
			meshesChanged = true;
		}
	}
	if( meshesChanged )
	{
		const ScopedEdit edit( session );
		document.SetPreviewMeshes( std::move( meshes ) );
	}

	SectionHeader( "Network" );
	ImGui::Text( "%zu parameters", network.GetParameters().size() );
	ImGui::Text( "%zu nodes in this graph", session.GetGraph().GetNodes().size() );
	ImGui::TextColored( HINT_TEXT, "Select a node to edit it." );
}

void PropertiesPanel::DrawNode( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	auto& session = *context.session;
	if( !( node == m_shownNode ) )
	{
		m_shownNode = node;
		m_expressionError.clear();
	}
	const auto* data = graph.GetNode( node );
	const auto info = cmf::GetAnimationNodeTypeInfo( data->type );
	ImGui::TextColored( ImGui::ColorConvertU32ToFloat4( GetNodeColor( data->type ) ), "%s", info.name );
	ImGui::TextWrapped( "%s", GetNodeDescription( data->type ) );

	ImGui::BeginDisabled( !session.CanEdit() );
	SectionHeader( "Node" );
	std::string name;
	ImGui::SetNextItemWidth( -ImGui::CalcTextSize( "Name" ).x - ImGui::GetStyle().ItemInnerSpacing.x );
	if( CommitText( m_text, "name", "Name", data->name, name ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetNodeName( node, name ) );
	}
	if( graph.GetSubgraph( node ) != nullptr && ImGui::Button( "Open Subgraph" ) )
	{
		session.EnterSubgraph( node );
		ImGui::EndDisabled();
		return;
	}

	DrawSettings( context, graph, node );
	DrawInputConstants( context, graph, node );

	const auto issues = DiagnoseNode( graph, node );
	if( !issues.errors.empty() || !issues.warnings.empty() )
	{
		SectionHeader( "Problems" );
		for( const auto& error : issues.errors )
		{
			ImGui::TextColored( ERROR_TEXT, "%s", error.c_str() );
		}
		for( const auto& warning : issues.warnings )
		{
			ImGui::TextColored( WARNING_TEXT, "%s", warning.c_str() );
		}
	}
	ImGui::EndDisabled();
}

void PropertiesPanel::DrawSettings( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	const auto& settings = graph.GetNode( node )->settings;
	if( std::holds_alternative<std::monostate>( settings ) )
	{
		return;
	}
	SectionHeader( "Settings" );
	if( const auto* interfaceSettings = std::get_if<cmf::AnimationInterfaceSettings>( &settings ) )
	{
		DrawInterface( context, graph, node, *interfaceSettings );
	}
	else if( const auto* parameter = std::get_if<cmf::AnimationParameterSettings>( &settings ) )
	{
		DrawParameter( context, graph, node, *parameter );
	}
	else if( const auto* sample = std::get_if<cmf::AnimationSampleSettings>( &settings ) )
	{
		DrawSample( context, graph, node, *sample );
	}
	else if( const auto* switchSettings = std::get_if<cmf::AnimationSwitchSettings>( &settings ) )
	{
		DrawSwitch( context, graph, node, *switchSettings );
	}
	else if( const auto* random = std::get_if<cmf::AnimationRandomSwitchSettings>( &settings ) )
	{
		DrawRandomSwitch( context, graph, node, *random );
	}
	else if( const auto* mask = std::get_if<cmf::AnimationNamedBoneWeightsSettings>( &settings ) )
	{
		DrawBoneMask( context, graph, node, *mask );
	}
	else if( const auto* weights = std::get_if<cmf::AnimationBoneWeightsSettings>( &settings ) )
	{
		DrawBoneWeights( context, graph, node, *weights );
	}
	else if( const auto* expression = std::get_if<cmf::AnimationExpressionSettings>( &settings ) )
	{
		DrawExpression( context, graph, node, *expression );
	}
}

void PropertiesPanel::DrawInterface( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationInterfaceSettings& settings )
{
	auto& session = *context.session;
	std::string socketName;
	if( CommitText( m_text, "socketName", "Socket name", settings.name, socketName ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetInterfaceSocket( node, socketName, settings.type ) );
	}
	const bool rootOutput = graph.GetParent() == nullptr;
	ImGui::BeginDisabled( rootOutput );
	if( ImGui::BeginCombo( "Socket type", GetSocketTypeName( settings.type ) ) )
	{
		for( const auto type : { cmf::AnimationSocketType::Pose, cmf::AnimationSocketType::Float, cmf::AnimationSocketType::BoneWeights } )
		{
			if( ImGui::Selectable( GetSocketTypeName( type ), type == settings.type ) && type != settings.type )
			{
				const ScopedEdit edit( session );
				ReportResult( context, graph.SetInterfaceSocket( node, settings.name, type ) );
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();
}

void PropertiesPanel::DrawParameter( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationParameterSettings& settings )
{
	auto& session = *context.session;
	const auto& parameters = context.document->GetNetwork().GetParameters();
	const auto* current = settings.parameterIndex < parameters.size() ? parameters[settings.parameterIndex].name.c_str() : "<none>";
	if( ImGui::BeginCombo( "Parameter", current ) )
	{
		for( uint32_t i = 0; i < parameters.size(); ++i )
		{
			if( ImGui::Selectable( parameters[i].name.c_str(), i == settings.parameterIndex ) )
			{
				const ScopedEdit edit( session );
				ReportResult( context, graph.SetParameter( node, i ) );
			}
		}
		ImGui::EndCombo();
	}
	if( parameters.empty() )
	{
		ImGui::TextColored( HINT_TEXT, "Add parameters in the Parameters panel." );
	}
}

void PropertiesPanel::DrawSwitch( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationSwitchSettings& settings )
{
	auto& session = *context.session;
	ImGui::Text( "Pose inputs: %u", settings.poseInputCount );
	ImGui::SameLine();
	if( ImGui::Button( " - " ) && settings.poseInputCount > 1 )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetPoseInputCount( node, settings.poseInputCount - 1 ) );
	}
	ImGui::SameLine();
	if( ImGui::Button( " + " ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetPoseInputCount( node, settings.poseInputCount + 1 ) );
	}
}

void PropertiesPanel::DrawBoneMask( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationNamedBoneWeightsSettings& settings )
{
	auto& session = *context.session;
	std::vector<std::string> masks;
	if( const auto* skeleton = context.document->GetNetwork().GetSkeleton() )
	{
		for( const auto& boneMask : skeleton->boneMasks )
		{
			masks.push_back( cmf::ToStdString( boneMask.name ) );
		}
	}
	std::string maskName;
	if( NameCombo( "Bone mask", settings.maskName, masks, "<none>", maskName ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetBoneMask( node, maskName ) );
	}
}

void PropertiesPanel::DrawSample( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationSampleSettings& settings )
{
	auto& session = *context.session;
	ImGui::TextUnformatted( "Animation file" );
	std::string reference;
	ImGui::SetNextItemWidth( -ImGui::GetFrameHeight() * BROWSE_BUTTON_SPACE );
	if( CommitText( m_text, "asset", "##asset", settings.assetPath, reference ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetAnimation( node, reference, settings.animationName ) );
	}
	ImGui::SameLine();
	if( ImGui::Button( "Browse" ) )
	{
		if( const auto browsed = BrowseReference( context, "Select Animation File" ) )
		{
			const ScopedEdit edit( session );
			ReportResult( context, graph.SetAnimation( node, *browsed, "" ) );
		}
	}
	if( const auto* error = context.loader->GetError( settings.assetPath ) )
	{
		ImGui::TextColored( ERROR_TEXT, "%s", error->c_str() );
	}
	std::vector<std::string> animations;
	if( const auto* data = context.document->GetNetwork().GetAssetData( settings.assetPath ) )
	{
		for( const auto& animation : data->animations )
		{
			animations.push_back( cmf::ToStdString( animation.name ) );
		}
	}
	std::string animation;
	if( NameCombo( "Animation", settings.animationName, animations, "(first animation)", animation ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetAnimation( node, settings.assetPath, animation ) );
	}
}

void PropertiesPanel::DrawRandomSwitch( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationRandomSwitchSettings& settings )
{
	auto& session = *context.session;
	ImGui::Text( "Pose inputs: %u", settings.poseInputCount );
	ImGui::SameLine();
	if( ImGui::Button( " - " ) && settings.poseInputCount > 1 )
	{
		const ScopedEdit edit( session );
		auto weights = settings.weights;
		if( !weights.empty() )
		{
			weights.resize( settings.poseInputCount - 1 );
		}
		ReportResult( context, graph.SetPoseInputCount( node, settings.poseInputCount - 1 ) );
		ReportResult( context, graph.SetRandomWeights( node, weights ) );
	}
	ImGui::SameLine();
	if( ImGui::Button( " + " ) )
	{
		const ScopedEdit edit( session );
		auto weights = settings.weights;
		if( !weights.empty() )
		{
			weights.push_back( 1.f );
		}
		ReportResult( context, graph.SetPoseInputCount( node, settings.poseInputCount + 1 ) );
		ReportResult( context, graph.SetRandomWeights( node, weights ) );
	}
	bool custom = !settings.weights.empty();
	if( ImGui::Checkbox( "Custom weights", &custom ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetRandomWeights( node, custom ? std::vector<float>( settings.poseInputCount, 1.f ) : std::vector<float>() ) );
	}
	if( custom )
	{
		auto weights = settings.weights;
		for( size_t i = 0; i < weights.size(); ++i )
		{
			ImGui::PushID( static_cast<int>( i ) );
			const auto label = "pose" + std::to_string( i );
			if( ImGui::DragFloat( label.c_str(), &weights[i], DRAG_SPEED, 0.f, std::numeric_limits<float>::max() ) )
			{
				ReportResult( context, graph.SetRandomWeights( node, weights ) );
			}
			TrackEdit( session );
			ImGui::PopID();
		}
	}
	bool reroll = settings.rerollOnActivate;
	if( ImGui::Checkbox( "Reroll on activate", &reroll ) )
	{
		const ScopedEdit edit( session );
		ReportResult( context, graph.SetRerollOnActivate( node, reroll ) );
	}
}

void PropertiesPanel::DrawBoneWeights( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationBoneWeightsSettings& settings )
{
	auto& session = *context.session;
	auto weights = settings.weights;
	float defaultWeight = settings.defaultWeight;
	if( ImGui::DragFloat( "Default weight", &defaultWeight, DRAG_SPEED, 0.f, 1.f ) )
	{
		ReportResult( context, graph.SetBoneWeights( node, weights, defaultWeight ) );
	}
	TrackEdit( session );

	std::vector<std::string> bones;
	if( const auto* skeleton = context.document->GetNetwork().GetSkeleton() )
	{
		bones = ToNames( skeleton->bones );
	}
	std::optional<size_t> removed;
	for( size_t i = 0; i < weights.size(); ++i )
	{
		ImGui::PushID( static_cast<int>( i ) );
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * HALF );
		std::string bone;
		if( NameCombo( "##bone", weights[i].bone, bones, "<bone>", bone ) )
		{
			const ScopedEdit edit( session );
			weights[i].bone = bone;
			ReportResult( context, graph.SetBoneWeights( node, weights, defaultWeight ) );
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * REMOVE_BUTTON_SPACE );
		if( ImGui::DragFloat( "##weight", &weights[i].weight, DRAG_SPEED, 0.f, 1.f ) )
		{
			ReportResult( context, graph.SetBoneWeights( node, weights, defaultWeight ) );
		}
		TrackEdit( session );
		ImGui::SameLine();
		if( ImGui::Button( "x" ) )
		{
			removed = i;
		}
		ImGui::PopID();
	}
	if( removed )
	{
		const ScopedEdit edit( session );
		weights.erase( weights.begin() + static_cast<std::ptrdiff_t>( *removed ) );
		ReportResult( context, graph.SetBoneWeights( node, weights, defaultWeight ) );
	}
	if( ImGui::Button( "Add Bone" ) )
	{
		const ScopedEdit edit( session );
		weights.push_back( { bones.empty() ? std::string() : bones.front(), 1.f } );
		ReportResult( context, graph.SetBoneWeights( node, weights, defaultWeight ) );
	}
}

void PropertiesPanel::DrawExpression( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationExpressionSettings& settings )
{
	auto& session = *context.session;
	ImGui::TextUnformatted( "Expression" );
	std::string expression;
	const bool expressionChanged = CommitText( m_text, "expression", "##expression", settings.expression, expression, true );
	ImGui::TextUnformatted( "Inputs (comma separated)" );
	std::string inputs;
	ImGui::SetNextItemWidth( -1.f );
	const bool inputsChanged = CommitText( m_text, "inputs", "##inputs", Join( settings.inputNames ), inputs );
	if( expressionChanged || inputsChanged )
	{
		const ScopedEdit edit( session );
		const auto result = graph.SetExpression( node, expressionChanged ? expression : settings.expression, inputsChanged ? SplitNames( inputs ) : settings.inputNames );
		m_expressionError = result ? std::string() : result.error;
	}
	if( !m_expressionError.empty() )
	{
		ImGui::TextColored( ERROR_TEXT, "%s", m_expressionError.c_str() );
	}
	ImGui::TextColored( HINT_TEXT, "Operators: + - * / < > == && || ?:  Functions: min max abs sqrt sin cos" );
}

void PropertiesPanel::DrawInputConstants( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node )
{
	auto& session = *context.session;
	bool header = false;
	for( uint32_t socket = 0; socket < graph.GetInputCount( node ); ++socket )
	{
		const cmf::AnimationSocketRef input = { node, socket };
		if( graph.GetInputType( input ) != cmf::AnimationSocketType::Float )
		{
			continue;
		}
		if( !header )
		{
			SectionHeader( "Inputs" );
			header = true;
		}
		ImGui::PushID( static_cast<int>( socket ) );
		const auto name = graph.GetInputName( input );
		if( graph.GetConnection( input ) )
		{
			ImGui::TextDisabled( "%s: connected", name.c_str() );
		}
		else
		{
			if( const auto value = InputConstantField( name.c_str(), graph.GetInputValueKind( input ), graph.GetNode( node )->inputConstants[socket] ) )
			{
				ReportResult( context, graph.SetInputConstant( input, *value ) );
			}
			TrackEdit( session );
		}
		ImGui::PopID();
	}
}

// Parameters

void ParametersPanel::Draw( AppContext& context )
{
	auto& session = *context.session;
	auto& network = context.document->GetNetwork();
	const auto& parameters = network.GetParameters();

	std::optional<uint32_t> removed;
	if( ImGui::BeginTable( "parameters", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp ) )
	{
		ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch, 1.f );
		ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, VALUE_COLUMN_WEIGHT );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		for( uint32_t i = 0; i < parameters.size(); ++i )
		{
			DrawParameterRow( context, i, removed );
		}
		ImGui::EndTable();
	}
	if( removed )
	{
		const ScopedEdit edit( session );
		ReportResult( context, network.RemoveParameter( *removed ) );
	}
	DrawAddParameter( context );
}

void ParametersPanel::DrawParameterRow( AppContext& context, uint32_t index, std::optional<uint32_t>& removed )
{
	auto& session = *context.session;
	auto& network = context.document->GetNetwork();
	auto& preview = *context.preview;
	const bool canEdit = session.CanEdit();
	// Copy: editing the parameter may reallocate the list
	const auto parameter = network.GetParameters()[index];
	ImGui::PushID( static_cast<int>( index ) );
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::BeginDisabled( !canEdit );
	ImGui::SetNextItemWidth( -1.f );
	std::string name;
	if( CommitText( m_text, "name" + std::to_string( index ), "##name", parameter.name, name ) )
	{
		const ScopedEdit edit( session );
		auto changed = parameter;
		changed.name = name;
		ReportResult( context, network.SetParameter( index, changed ) );
	}
	ImGui::EndDisabled();
	if( ImGui::BeginDragDropSource() )
	{
		ImGui::SetDragDropPayload( PARAMETER_PAYLOAD, &index, sizeof( index ) );
		ImGui::Text( "Parameter '%s'", parameter.name.c_str() );
		ImGui::EndDragDropSource();
	}

	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth( -1.f );
	float value = preview.GetParameterValue( parameter );
	const bool bounded = std::isfinite( parameter.minValue ) && std::isfinite( parameter.maxValue );
	const bool changed = bounded ? ImGui::SliderFloat( "##value", &value, parameter.minValue, parameter.maxValue ) : ImGui::DragFloat( "##value", &value, DRAG_SPEED, parameter.minValue, parameter.maxValue );
	if( changed )
	{
		preview.SetParameterValue( parameter.name, value );
	}
	if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Right ) )
	{
		preview.ResetParameterValue( parameter.name );
	}

	ImGui::TableNextColumn();
	if( ImGui::Button( "..." ) )
	{
		ImGui::OpenPopup( "definition" );
	}
	if( ImGui::BeginPopup( "definition" ) )
	{
		ImGui::BeginDisabled( !canEdit );
		DrawParameterDefinition( context, index, parameter, removed );
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void ParametersPanel::DrawParameterDefinition( AppContext& context, uint32_t index, const cmf::AnimationParameter& parameter, std::optional<uint32_t>& removed )
{
	auto& session = *context.session;
	auto& network = context.document->GetNetwork();
	const bool bounded = std::isfinite( parameter.minValue ) && std::isfinite( parameter.maxValue );
	auto edited = parameter;
	bool range = bounded;
	if( ImGui::DragFloat( "Default", &edited.defaultValue, DRAG_SPEED ) )
	{
		ReportResult( context, network.SetParameter( index, edited ) );
	}
	TrackEdit( session );
	if( ImGui::Checkbox( "Limited range", &range ) )
	{
		const ScopedEdit edit( session );
		edited.minValue = range ? std::min( 0.f, edited.defaultValue ) : -std::numeric_limits<float>::infinity();
		edited.maxValue = range ? std::max( DEFAULT_RANGE_MAX, edited.defaultValue ) : std::numeric_limits<float>::infinity();
		ReportResult( context, network.SetParameter( index, edited ) );
	}
	if( range && bounded )
	{
		if( ImGui::DragFloat( "Min", &edited.minValue, DRAG_SPEED, -std::numeric_limits<float>::max(), edited.maxValue ) )
		{
			ReportResult( context, network.SetParameter( index, edited ) );
		}
		TrackEdit( session );
		if( ImGui::DragFloat( "Max", &edited.maxValue, DRAG_SPEED, edited.minValue, std::numeric_limits<float>::max() ) )
		{
			ReportResult( context, network.SetParameter( index, edited ) );
		}
		TrackEdit( session );
	}
	if( ImGui::Button( "Reset preview value" ) )
	{
		context.preview->ResetParameterValue( parameter.name );
	}
	ImGui::SameLine();
	if( ImGui::Button( "Remove" ) )
	{
		removed = index;
		ImGui::CloseCurrentPopup();
	}
}

void ParametersPanel::DrawAddParameter( AppContext& context )
{
	auto& session = *context.session;
	auto& network = context.document->GetNetwork();
	ImGui::BeginDisabled( !session.CanEdit() );
	ImGui::SetNextItemWidth( -ImGui::CalcTextSize( "Add" ).x - ImGui::GetStyle().FramePadding.x * TWO_SIDES - ImGui::GetStyle().ItemSpacing.x );
	const bool enter = ImGui::InputTextWithHint( "##newParameter", "New parameter name", &m_newName, ImGuiInputTextFlags_EnterReturnsTrue );
	ImGui::SameLine();
	if( ( ImGui::Button( "Add" ) || enter ) && !m_newName.empty() )
	{
		const ScopedEdit edit( session );
		cmf::AnimationParameter parameter;
		parameter.name = m_newName;
		if( network.AddParameter( parameter ) == cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
		{
			context.SetStatus( "A parameter named '" + m_newName + "' already exists", true );
		}
		else
		{
			m_newName.clear();
		}
	}
	ImGui::EndDisabled();
	ImGui::TextColored( HINT_TEXT, "Drag a parameter onto the graph to use it." );
}

// Assets and problems

void AssetsPanel::Draw( AppContext& context )
{
	auto& session = *context.session;
	auto& network = context.document->GetNetwork();

	const auto errors = network.GetErrors();
	const auto warnings = network.GetWarnings();
	const auto loadErrors = context.loader->GetErrors();
	const auto& modelErrors = context.preview->GetModelErrors();
	if( errors.empty() && warnings.empty() && loadErrors.empty() && modelErrors.empty() )
	{
		ImGui::TextColored( OK_TEXT, "No problems" );
	}
	for( const auto& [path, error] : loadErrors )
	{
		ImGui::TextColored( ERROR_TEXT, "%s", error.c_str() );
	}
	for( const auto& error : modelErrors )
	{
		ImGui::TextColored( ERROR_TEXT, "%s", error.c_str() );
	}
	for( const auto& error : errors )
	{
		ImGui::TextColored( ERROR_TEXT, "%s", error.c_str() );
	}
	for( const auto& warning : warnings )
	{
		ImGui::TextColored( WARNING_TEXT, "%s", warning.c_str() );
	}

	SectionHeader( "Embedded Assets" );
	ImGui::TextColored( HINT_TEXT, "Embedded files are stored in the network and replace the referenced files." );
	// Embedding stays available without a skeleton: embedding the skeleton file is a way to fix a missing one
	std::optional<std::string> removed;
	for( const auto& path : network.GetEmbeddedAssetPaths() )
	{
		ImGui::PushID( path.c_str() );
		if( ImGui::SmallButton( "Remove" ) )
		{
			removed = path;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted( path.c_str() );
		ImGui::PopID();
	}
	if( removed )
	{
		const ScopedEdit edit( session );
		network.RemoveEmbeddedAsset( *removed );
	}
	if( ImGui::Button( "Embed File..." ) )
	{
		if( const auto path = dialogs::OpenCmf( "Embed CMF File" ) )
		{
			std::ifstream stream( std::filesystem::u8path( *path ), std::ios::binary );
			std::vector<uint8_t> bytes( ( std::istreambuf_iterator<char>( stream ) ), std::istreambuf_iterator<char>() );
			const ScopedEdit edit( session );
			ReportResult( context, network.EmbedAsset( context.resolver->MakeReference( std::filesystem::u8path( *path ) ), std::move( bytes ) ) );
		}
	}
}

}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
