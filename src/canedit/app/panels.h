// Copyright © 2026 CCP ehf.

#pragma once

#include "appContext.h"

#include <map>
#include <optional>
#include <string>

namespace canedit
{

class GraphCanvas;

/** Node types that can be added to the open graph by clicking or dragging them onto the graph. */
class PalettePanel
{
public:
	void Draw( AppContext& context, GraphCanvas& canvas );

private:
	std::string m_filter;
};

/** Settings of the selected node, or of the network when no node is selected. */
class PropertiesPanel
{
public:
	void Draw( AppContext& context );

private:
	void DrawNetwork( AppContext& context );
	void DrawNode( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );
	void DrawSettings( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );
	void DrawInterface( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationInterfaceSettings& settings );
	static void DrawParameter( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationParameterSettings& settings );
	static void DrawSwitch( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationSwitchSettings& settings );
	static void DrawBoneMask( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationNamedBoneWeightsSettings& settings );
	void DrawSample( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationSampleSettings& settings );
	void DrawExpression( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationExpressionSettings& settings );
	static void DrawBoneWeights( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationBoneWeightsSettings& settings );
	static void DrawRandomSwitch( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, const cmf::AnimationRandomSwitchSettings& settings );
	static void DrawInputConstants( AppContext& context, cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node );

	/** Text being edited, so that it is applied when editing ends. Keyed by field. */
	std::map<std::string, std::string> m_text;
	std::string m_expressionError;
	cmf::AnimationNodeId m_shownNode;
};

/** Control parameters: their definitions and preview values. */
class ParametersPanel
{
public:
	void Draw( AppContext& context );

private:
	void DrawParameterRow( AppContext& context, uint32_t index, std::optional<uint32_t>& removed );
	static void DrawParameterDefinition( AppContext& context, uint32_t index, const cmf::AnimationParameter& parameter, std::optional<uint32_t>& removed );
	void DrawAddParameter( AppContext& context );

	std::string m_newName;
	/** Names being edited, keyed by field. */
	std::map<std::string, std::string> m_text;
};

/** Problems of the network and its assets, and embedded assets. */
class AssetsPanel
{
public:
	static void Draw( AppContext& context );
};

}
