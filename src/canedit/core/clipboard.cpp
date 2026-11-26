// Copyright © 2026 CCP ehf.

#include "clipboard.h"

#include "networkJson.h"

#include <algorithm>
#include <limits>

namespace canedit
{
namespace
{

using Json = nlohmann::json;

constexpr int CLIPBOARD_VERSION = 1;

}

std::string CopyNodes( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes )
{
	std::vector<cmf::AnimationNodeId> valid;
	std::copy_if( nodes.begin(), nodes.end(), std::back_inserter( valid ), [&graph]( cmf::AnimationNodeId id ) { return graph.IsValid( id ); } );
	Json json = WriteGraphJson( document, graph, valid, GraphJsonMode::Clipboard );
	json[CLIPBOARD_MARKER] = CLIPBOARD_VERSION;
	return json.dump( 1, '\t' );
}

bool IsClipboardText( std::string_view text )
{
	const auto json = Json::parse( text.begin(), text.end(), nullptr, false );
	return !json.is_discarded() && json.is_object() && json.contains( CLIPBOARD_MARKER ) && json.contains( "nodes" ) && json.contains( "edges" );
}

PasteResult PasteNodes( Document& document, cmf::AnimationGraphBuilder& graph, std::string_view text, Vec2 origin )
{
	PasteResult result;
	if( !IsClipboardText( text ) )
	{
		result.error = "The clipboard doesn't contain animation network nodes";
		return result;
	}
	try
	{
		const auto json = Json::parse( text.begin(), text.end() );
		// Move the top left copied node to the origin
		Vec2 topLeft{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		for( const auto& node : json.at( "nodes" ) )
		{
			const auto& position = node.at( "position" );
			topLeft.x = std::min( topLeft.x, position.at( 0 ).get<float>() );
			topLeft.y = std::min( topLeft.y, position.at( 1 ).get<float>() );
		}
		const Vec2 offset = json.at( "nodes" ).empty() ? Vec2{} : Vec2{ origin.x - topLeft.x, origin.y - topLeft.y };
		GraphJsonReader reader( document, result.warnings, GraphJsonMode::Clipboard );
		for( const auto id : reader.ReadGraph( graph, json, offset ) )
		{
			if( id.IsSet() )
			{
				result.nodes.push_back( id );
			}
		}
	}
	catch( const nlohmann::json::exception& exception )
	{
		result.error = std::string( "Invalid clipboard content: " ) + exception.what();
	}
	return result;
}

}
