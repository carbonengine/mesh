// Copyright © 2026 CCP ehf.

#include "document.h"

#include "autoLayout.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <set>

namespace canedit
{
namespace
{

constexpr const char* FORMAT_NAME = "canedit";

// Reads the view and node positions of a graph from the editor data
void ReadGraphState( const nlohmann::json& source, const cmf::AnimationGraphLayout& layout, GraphView& view, std::map<uint32_t, Vec2>& positions )
{
	const auto isNumber = []( const nlohmann::json& value ) { return value.is_number(); };
	const auto storedView = source.find( "view" );
	if( storedView != source.end() && storedView->is_array() && storedView->size() == 3 && std::all_of( storedView->begin(), storedView->end(), isNumber ) )
	{
		view.pan = { ( *storedView )[0].get<float>(), ( *storedView )[1].get<float>() };
		view.zoom = ( *storedView )[2].get<float>();
	}
	const auto nodes = source.find( "nodes" );
	if( nodes == source.end() || !nodes->is_array() )
	{
		return;
	}
	for( size_t n = 0; n < nodes->size() && n < layout.nodes.size(); ++n )
	{
		const auto& position = ( *nodes )[n];
		if( position.is_array() && position.size() == 2 && std::all_of( position.begin(), position.end(), isNumber ) )
		{
			positions[layout.nodes[n].index] = { position[0].get<float>(), position[1].get<float>() };
		}
	}
}

// Adds the positions of nodes that don't have one yet
void FillPositions( std::map<uint32_t, Vec2>& positions, const std::map<uint32_t, Vec2>& layout, const std::vector<cmf::AnimationNodeId>& nodes )
{
	if( nodes.empty() )
	{
		return;
	}
	// Place the computed layout below the existing nodes, so that the new nodes don't overlap them
	float bottom = 0.f;
	for( const auto& [index, position] : positions )
	{
		bottom = std::max( bottom, position.y + LAYOUT_ROW_SPACING );
	}
	const bool hasPositions = !positions.empty();
	for( const auto id : nodes )
	{
		const auto it = layout.find( id.index );
		if( positions.count( id.index ) == 0 && it != layout.end() )
		{
			positions[id.index] = { it->second.x, it->second.y + ( hasPositions ? bottom : 0.f ) };
		}
	}
}

}

Document::Document( cmf::AnimationAssetLoader& loader ) :
	m_network( loader )
{
}

cmf::AnimationNetworkBuilder& Document::GetNetwork()
{
	return m_network;
}

const cmf::AnimationNetworkBuilder& Document::GetNetwork() const
{
	return m_network;
}

cmf::AnimationGraphBuilder* Document::FindGraph( const GraphPath& path )
{
	auto* graph = &m_network.GetRootGraph();
	for( const auto id : path )
	{
		graph = graph->GetSubgraph( id );
		if( graph == nullptr )
		{
			return nullptr;
		}
	}
	return graph;
}

const cmf::AnimationGraphBuilder* Document::FindGraph( const GraphPath& path ) const
{
	return const_cast<Document*>( this )->FindGraph( path ); // NOLINT(cppcoreguidelines-pro-type-const-cast): shares the non-const lookup, which doesn't modify anything
}

std::optional<Vec2> Document::GetNodePosition( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const
{
	const auto it = m_graphs.find( &graph );
	if( it == m_graphs.end() )
	{
		return std::nullopt;
	}
	const auto position = it->second.positions.find( node.index );
	if( position == it->second.positions.end() )
	{
		return std::nullopt;
	}
	return position->second;
}

void Document::SetNodePosition( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node, Vec2 position )
{
	auto& positions = m_graphs[&graph].positions;
	const auto it = positions.find( node.index );
	if( it != positions.end() && it->second == position )
	{
		return;
	}
	positions[node.index] = position;
	++m_editorRevision;
}

GraphView Document::GetView( const cmf::AnimationGraphBuilder& graph ) const
{
	const auto it = m_graphs.find( &graph );
	return it == m_graphs.end() ? GraphView{} : it->second.view;
}

void Document::SetView( const cmf::AnimationGraphBuilder& graph, const GraphView& view )
{
	m_graphs[&graph].view = view;
}

const std::string& Document::GetPreviewMesh() const
{
	return m_previewMesh;
}

void Document::SetPreviewMesh( std::string reference )
{
	if( reference != m_previewMesh )
	{
		m_previewMesh = std::move( reference );
		++m_editorRevision;
	}
}

void Document::PlaceUnpositionedNodes( const cmf::AnimationGraphBuilder& graph )
{
	auto& positions = m_graphs[&graph].positions;
	const auto nodes = graph.GetNodes();
	std::vector<cmf::AnimationNodeId> missing;
	for( const auto id : nodes )
	{
		if( positions.count( id.index ) == 0 )
		{
			missing.push_back( id );
		}
	}
	if( missing.empty() )
	{
		return;
	}
	FillPositions( positions, ComputeAutoLayout( graph ), missing );
	++m_editorRevision;
}

void Document::AutoLayout( const cmf::AnimationGraphBuilder& graph )
{
	m_graphs[&graph].positions = ComputeAutoLayout( graph );
	++m_editorRevision;
}

std::vector<uint8_t> Document::WriteEditorData() const
{
	nlohmann::json json;
	json["format"] = FORMAT_NAME;
	json["version"] = EDITOR_DATA_VERSION;
	if( !m_previewMesh.empty() )
	{
		json["previewMesh"] = m_previewMesh;
	}
	auto& graphs = json["graphs"];
	graphs = nlohmann::json::array();
	for( const auto& layout : m_network.GetSaveLayout() )
	{
		nlohmann::json graph;
		const auto it = m_graphs.find( layout.graph );
		const GraphState state = it == m_graphs.end() ? GraphState{} : it->second;
		graph["view"] = { state.view.pan.x, state.view.pan.y, state.view.zoom };
		auto& nodes = graph["nodes"];
		nodes = nlohmann::json::array();
		for( const auto id : layout.nodes )
		{
			const auto position = state.positions.find( id.index );
			if( position == state.positions.end() )
			{
				nodes.push_back( nullptr );
			}
			else
			{
				nodes.push_back( { position->second.x, position->second.y } );
			}
		}
		graphs.push_back( std::move( graph ) );
	}
	const auto text = json.dump();
	return { text.begin(), text.end() };
}

void Document::ReadEditorData( const std::vector<uint8_t>& data )
{
	m_graphs.clear();
	m_previewMesh.clear();
	const auto layout = m_network.GetSaveLayout();
	const auto json = nlohmann::json::parse( data.begin(), data.end(), nullptr, false );
	if( !json.is_discarded() && json.is_object() && json.value( "format", "" ) == FORMAT_NAME )
	{
		m_previewMesh = json.value( "previewMesh", "" );
		const auto graphs = json.find( "graphs" );
		if( graphs != json.end() && graphs->is_array() )
		{
			for( size_t g = 0; g < graphs->size() && g < layout.size(); ++g )
			{
				auto& state = m_graphs[layout[g].graph];
				ReadGraphState( ( *graphs )[g], layout[g], state.view, state.positions );
			}
		}
	}
	for( const auto& graph : layout )
	{
		PlaceUnpositionedNodes( *graph.graph );
	}
}

void Document::PruneGraphs()
{
	std::set<const cmf::AnimationGraphBuilder*> live;
	for( const auto& layout : m_network.GetSaveLayout() )
	{
		live.insert( layout.graph );
	}
	for( auto it = m_graphs.begin(); it != m_graphs.end(); )
	{
		it = live.count( it->first ) == 0 ? m_graphs.erase( it ) : std::next( it );
	}
}

std::vector<uint8_t> Document::Serialize()
{
	m_network.SetEditorData( WriteEditorData() );
	return m_network.Save();
}

bool Document::Deserialize( const std::vector<uint8_t>& file, std::string& error )
{
	const auto result = m_network.Load( file.data(), file.size() );
	if( !result )
	{
		error = result.error;
		return false;
	}
	ReadEditorData( m_network.GetEditorData() );
	PruneGraphs();
	++m_editorRevision;
	return true;
}

void Document::New()
{
	m_network.Clear();
	m_graphs.clear();
	m_previewMesh.clear();
	m_filePath.clear();
	auto& root = m_network.GetRootGraph();
	const auto output = root.AddNode( cmf::AnimationNodeType::Output );
	SetNodePosition( root, output, { 0.f, 0.f } );
	m_savedRevision = GetRevision();
}

bool Document::LoadFile( const std::filesystem::path& path, std::string& error )
{
	std::ifstream stream( path, std::ios::binary );
	if( !stream )
	{
		error = "Cannot open '" + path.u8string() + "'";
		return false;
	}
	const std::vector<uint8_t> bytes( ( std::istreambuf_iterator<char>( stream ) ), std::istreambuf_iterator<char>() );
	if( !Deserialize( bytes, error ) )
	{
		error = "'" + path.u8string() + "' is not a valid animation network: " + error;
		return false;
	}
	m_filePath = path;
	m_savedRevision = GetRevision();
	return true;
}

bool Document::SaveFile( const std::filesystem::path& path, std::string& error )
{
	const auto bytes = Serialize();
	// Write to a temporary file first, so that a failed save doesn't destroy the previous file
	auto temporary = path;
	temporary += ".tmp";
	{
		std::ofstream stream( temporary, std::ios::binary | std::ios::trunc );
		if( !stream )
		{
			error = "Cannot write '" + path.u8string() + "'";
			return false;
		}
		stream.write( reinterpret_cast<const char*>( bytes.data() ), static_cast<std::streamsize>( bytes.size() ) ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): streams write chars
		if( !stream )
		{
			error = "Failed to write '" + path.u8string() + "'";
			return false;
		}
	}
	std::error_code ec;
	std::filesystem::rename( temporary, path, ec );
	if( ec )
	{
		std::filesystem::remove( temporary, ec );
		error = "Cannot replace '" + path.u8string() + "'";
		return false;
	}
	m_filePath = path;
	m_savedRevision = GetRevision();
	return true;
}

const std::filesystem::path& Document::GetFilePath() const
{
	return m_filePath;
}

bool Document::IsModified() const
{
	return GetRevision() != m_savedRevision;
}

uint64_t Document::GetRevision() const
{
	return m_network.GetGeneration() + m_editorRevision;
}

bool Document::IsLocked() const
{
	return m_network.GetSkeleton() == nullptr && !m_network.HasPendingAssets();
}

std::optional<NodeLocation> Document::Locate( const cmf::AnimationGraphBuilder& graph, cmf::AnimationNodeId node ) const
{
	const auto layout = m_network.GetSaveLayout();
	for( size_t g = 0; g < layout.size(); ++g )
	{
		if( layout[g].graph != &graph )
		{
			continue;
		}
		const auto& nodes = layout[g].nodes;
		const auto it = std::find( nodes.begin(), nodes.end(), node );
		if( it == nodes.end() )
		{
			return std::nullopt;
		}
		return NodeLocation{ static_cast<uint32_t>( g ), static_cast<uint32_t>( it - nodes.begin() ) };
	}
	return std::nullopt;
}

std::pair<cmf::AnimationGraphBuilder*, cmf::AnimationNodeId> Document::Find( const NodeLocation& location )
{
	const auto layout = m_network.GetSaveLayout();
	if( location.graph >= layout.size() || location.node >= layout[location.graph].nodes.size() )
	{
		return { nullptr, {} };
	}
	// Layouts only hold const graphs; the document owns the network, so the graph can be modified
	auto* graph = const_cast<cmf::AnimationGraphBuilder*>( layout[location.graph].graph ); // NOLINT(cppcoreguidelines-pro-type-const-cast): see above
	return { graph, layout[location.graph].nodes[location.node] };
}

std::vector<NodeLocation> Document::LocatePath( const GraphPath& path ) const
{
	std::vector<NodeLocation> locations;
	const auto* graph = &m_network.GetRootGraph();
	for( const auto id : path )
	{
		const auto location = Locate( *graph, id );
		graph = graph->GetSubgraph( id );
		if( !location || graph == nullptr )
		{
			break;
		}
		locations.push_back( *location );
	}
	return locations;
}

GraphPath Document::FindPath( const std::vector<NodeLocation>& locations )
{
	GraphPath path;
	for( const auto& location : locations )
	{
		const auto [graph, id] = Find( location );
		if( graph == nullptr || graph != FindGraph( path ) || graph->GetSubgraph( id ) == nullptr )
		{
			break;
		}
		path.push_back( id );
	}
	return path;
}

}
