// Copyright © 2026 CCP ehf.

#pragma once

#include "document.h"

#include <string>
#include <string_view>
#include <vector>

namespace canedit
{

/** Key that identifies clipboard text written by the editor. */
constexpr const char* CLIPBOARD_MARKER = "canedit-clipboard";

/** Serializes nodes of a graph as JSON text for the clipboard: their settings, input constants, positions, the
* contents of Subgraph nodes and the edges between the copied nodes. Parameters used by Parameter nodes are stored
* by definition, so that the nodes can be pasted into another network. */
[[nodiscard]] std::string CopyNodes( const Document& document, const cmf::AnimationGraphBuilder& graph, const std::vector<cmf::AnimationNodeId>& nodes );

/** Returns true if the text was written by CopyNodes. */
[[nodiscard]] bool IsClipboardText( std::string_view text );

/** Result of PasteNodes. */
struct PasteResult
{
	/** The pasted nodes of the target graph. */
	std::vector<cmf::AnimationNodeId> nodes;
	/** Problems with parts that couldn't be pasted, for example a second Output node in the root graph. */
	std::vector<std::string> warnings;
	/** Set if the text is not clipboard content of the editor. */
	std::string error;
};

/** Creates the nodes stored by CopyNodes in a graph. The pasted nodes keep their relative positions; the top left
* node is placed at origin. Parameters that don't exist in the network are added. */
PasteResult PasteNodes( Document& document, cmf::AnimationGraphBuilder& graph, std::string_view text, Vec2 origin );

}
