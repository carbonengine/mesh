// Copyright © 2026 CCP ehf.

#include "document.h"

#include "autoLayout.h"
#include "networkJson.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>

namespace canedit
{
namespace
{

using Json = nlohmann::json;

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

// Drops empty and repeated references, keeping the order of the rest
std::vector<std::string> CleanReferences( std::vector<std::string> references )
{
	std::set<std::string> seen;
	const auto removed = std::remove_if( references.begin(), references.end(), [&seen]( const std::string& reference ) {
		return reference.empty() || !seen.insert( reference ).second;
	} );
	references.erase( removed, references.end() );
	return references;
}

bool ReadBytes( const std::filesystem::path& path, std::vector<uint8_t>& bytes, std::string& error )
{
	std::ifstream stream( path, std::ios::binary );
	if( !stream )
	{
		error = "Cannot open '" + path.u8string() + "'";
		return false;
	}
	bytes.assign( std::istreambuf_iterator<char>( stream ), std::istreambuf_iterator<char>() );
	return true;
}

// Writes to a temporary file first, so that a failed write doesn't destroy the previous file
bool WriteBytes( const std::filesystem::path& path, const void* data, size_t size, std::string& error )
{
	auto temporary = path;
	temporary += ".tmp";
	{
		std::ofstream stream( temporary, std::ios::binary | std::ios::trunc );
		if( !stream )
		{
			error = "Cannot write '" + path.u8string() + "'";
			return false;
		}
		stream.write( static_cast<const char*>( data ), static_cast<std::streamsize>( size ) );
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
	return true;
}

// Returns the directory of a file as an absolute path, so that paths relative to it stay valid
std::filesystem::path GetBaseDirectory( const std::filesystem::path& file )
{
	std::error_code ec;
	auto absolute = std::filesystem::absolute( file, ec );
	return ec ? file.parent_path() : absolute.parent_path();
}

}

Document::Document( cmf::AnimationAssetLoader& loader, const PathResolver& resolver ) :
	m_loader( &loader ),
	m_resolver( &resolver ),
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

const std::vector<std::string>& Document::GetPreviewMeshes() const
{
	return m_previewMeshes;
}

void Document::SetPreviewMeshes( std::vector<std::string> references )
{
	references = CleanReferences( std::move( references ) );
	if( references != m_previewMeshes )
	{
		m_previewMeshes = std::move( references );
		++m_editorRevision;
	}
}

void Document::AddPreviewMesh( std::string reference )
{
	auto references = m_previewMeshes;
	references.push_back( std::move( reference ) );
	SetPreviewMeshes( std::move( references ) );
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

void Document::PlaceAllUnpositionedNodes()
{
	for( const auto& layout : m_network.GetSaveLayout() )
	{
		PlaceUnpositionedNodes( *layout.graph );
	}
}

const std::set<std::string>& Document::GetEmbeddedAssets() const
{
	return m_embeddedAssets;
}

bool Document::LoadEmbeddedAsset( const std::string& reference )
{
	m_embeddedErrors.erase( reference );
	std::vector<uint8_t> bytes;
	std::string error;
	const auto cached = m_embeddedFiles.find( reference );
	if( cached != m_embeddedFiles.end() )
	{
		bytes = cached->second;
	}
	else
	{
		const auto path = m_resolver->Resolve( reference, &error );
		if( !path || !ReadBytes( *path, bytes, error ) )
		{
			m_embeddedErrors[reference] = error;
			return false;
		}
	}
	const auto result = m_network.EmbedAsset( reference, bytes );
	if( !result )
	{
		m_embeddedErrors[reference] = result.error;
		return false;
	}
	m_embeddedFiles[reference] = std::move( bytes );
	return true;
}

cmf::ValidationResult Document::EmbedAsset( const std::string& reference )
{
	if( reference.empty() || m_embeddedAssets.count( reference ) != 0 )
	{
		return { false, reference.empty() ? std::string( "The reference is empty" ) : "'" + reference + "' is already embedded" };
	}
	// Read the file again, it may have changed since it was embedded before
	m_embeddedFiles.erase( reference );
	if( !LoadEmbeddedAsset( reference ) )
	{
		auto error = "Cannot embed '" + reference + "': " + m_embeddedErrors[reference];
		m_embeddedErrors.erase( reference );
		return { false, std::move( error ) };
	}
	m_embeddedAssets.insert( reference );
	++m_editorRevision;
	return { true, {} };
}

void Document::RemoveEmbeddedAsset( const std::string& reference )
{
	if( m_embeddedAssets.erase( reference ) == 0 )
	{
		return;
	}
	m_network.RemoveEmbeddedAsset( reference );
	m_embeddedErrors.erase( reference );
	++m_editorRevision;
}

const std::string* Document::GetEmbeddedAssetError( const std::string& reference ) const
{
	const auto it = m_embeddedErrors.find( reference );
	return it == m_embeddedErrors.end() ? nullptr : &it->second;
}

void Document::ForgetEmbeddedFiles()
{
	m_embeddedFiles.clear();
}


std::string Document::Serialize( const std::filesystem::path& baseDirectory ) const
{
	Json json;
	json["format"] = FORMAT_NAME;
	json["version"] = FILE_VERSION;
	json["skeleton"] = { { "asset", m_network.GetSkeletonPath() }, { "name", m_network.GetSkeletonName() } };
	auto& parameters = json["parameters"];
	parameters = Json::array();
	for( const auto& parameter : m_network.GetParameters() )
	{
		parameters.push_back( WriteParameterJson( parameter ) );
	}
	json["embeddedAssets"] = m_embeddedAssets;
	json["previewMeshes"] = m_previewMeshes;
	auto exportPath = m_exportPath;
	if( !baseDirectory.empty() && !exportPath.empty() )
	{
		// Paths on another drive have no relative form
		const auto relative = exportPath.lexically_relative( baseDirectory );
		if( !relative.empty() )
		{
			exportPath = relative;
		}
	}
	json["exportPath"] = exportPath.generic_u8string();
	const auto& root = m_network.GetRootGraph();
	json["graph"] = WriteGraphJson( *this, root, root.GetNodes(), GraphJsonMode::Document );
	return json.dump( 1, '\t' ) + "\n";
}

void Document::Reset()
{
	m_network.Clear();
	m_graphs.clear();
	m_previewMeshes.clear();
	m_embeddedAssets.clear();
	m_embeddedErrors.clear();
	m_exportPath.clear();
	++m_editorRevision;
}

void Document::Read( std::string_view text, std::vector<std::string>& warnings, const std::filesystem::path& baseDirectory )
{
	const auto json = Json::parse( text.begin(), text.end() );
	Reset();
	// Embedded assets first, so that the references below resolve to them
	for( const auto& reference : json.value( "embeddedAssets", std::vector<std::string>() ) )
	{
		m_embeddedAssets.insert( reference );
		if( !LoadEmbeddedAsset( reference ) )
		{
			warnings.push_back( "Embedded asset '" + reference + "' is not available: " + m_embeddedErrors[reference] );
		}
	}
	const auto skeleton = json.value( "skeleton", Json::object() );
	m_network.SetSkeleton( skeleton.value( "asset", "" ), skeleton.value( "name", "" ) );
	for( const auto& parameter : json.value( "parameters", Json::array() ) )
	{
		const auto definition = ReadParameterJson( parameter );
		if( m_network.AddParameter( definition ) == cmf::AnimationNetworkBuilder::INVALID_PARAMETER )
		{
			warnings.push_back( "Parameter '" + definition.name + "' skipped: its name is empty or used twice" );
		}
	}
	GraphJsonReader reader( *this, warnings, GraphJsonMode::Document );
	reader.ReadGraph( m_network.GetRootGraph(), json.at( "graph" ), {} );
	m_previewMeshes = CleanReferences( json.value( "previewMeshes", std::vector<std::string>() ) );
	const auto exportPath = std::filesystem::u8path( json.value( "exportPath", "" ) );
	m_exportPath = exportPath.is_relative() && !exportPath.empty() && !baseDirectory.empty() ? ( baseDirectory / exportPath ).lexically_normal() : exportPath;
	PlaceAllUnpositionedNodes();
}

bool Document::Deserialize( std::string_view text, std::string& error, std::vector<std::string>* warnings, const std::filesystem::path& baseDirectory )
{
	const auto json = Json::parse( text.begin(), text.end(), nullptr, false );
	if( json.is_discarded() || !json.is_object() )
	{
		error = "not a JSON document";
		return false;
	}
	if( json.value( "format", "" ) != FORMAT_NAME || !json.contains( "version" ) || !json["version"].is_number_integer() )
	{
		error = "not a CanEdit document";
		return false;
	}
	const auto version = json["version"].get<int>();
	if( version > FILE_VERSION || version < 1 )
	{
		error = "unsupported document version " + std::to_string( version ) + ", this CanEdit supports version " + std::to_string( FILE_VERSION );
		return false;
	}
	const auto previous = Serialize();
	const bool wasModified = IsModified();
	std::vector<std::string> readWarnings;
	try
	{
		Read( text, readWarnings, baseDirectory );
	}
	catch( const nlohmann::json::exception& exception )
	{
		error = std::string( "invalid document: " ) + exception.what();
		// Restore the previous state, which can be read
		std::vector<std::string> ignored;
		Read( previous, ignored, {} );
		if( !wasModified )
		{
			MarkUnmodified();
		}
		return false;
	}
	if( warnings != nullptr )
	{
		*warnings = std::move( readWarnings );
	}
	return true;
}

void Document::New()
{
	Reset();
	m_embeddedFiles.clear();
	m_filePath.clear();
	auto& root = m_network.GetRootGraph();
	const auto output = root.AddNode( cmf::AnimationNodeType::Output );
	SetNodePosition( root, output, { 0.f, 0.f } );
	m_savedRevision = GetRevision();
}

bool Document::LoadFile( const std::filesystem::path& path, std::string& error, std::vector<std::string>* warnings )
{
	std::vector<uint8_t> bytes;
	if( !ReadBytes( path, bytes, error ) )
	{
		return false;
	}
	// Read embedded files again: they may have changed since they were read
	auto embeddedFiles = std::move( m_embeddedFiles );
	m_embeddedFiles.clear();
	std::vector<std::string> readWarnings;
	const std::string_view text( reinterpret_cast<const char*>( bytes.data() ), bytes.size() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): the file is text
	if( !Deserialize( text, error, &readWarnings, GetBaseDirectory( path ) ) )
	{
		m_embeddedFiles = std::move( embeddedFiles );
		error = "'" + path.u8string() + "' is not a valid animation network document: " + error;
		return false;
	}
	m_filePath = path;
	if( readWarnings.empty() )
	{
		m_savedRevision = GetRevision();
	}
	if( warnings != nullptr )
	{
		*warnings = std::move( readWarnings );
	}
	return true;
}

bool Document::SaveFile( const std::filesystem::path& path, std::string& error )
{
	const auto text = Serialize( GetBaseDirectory( path ) );
	if( !WriteBytes( path, text.data(), text.size(), error ) )
	{
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


const std::filesystem::path& Document::GetExportPath() const
{
	return m_exportPath;
}

void Document::SetExportPath( const std::filesystem::path& path )
{
	if( path != m_exportPath )
	{
		m_exportPath = path;
		++m_editorRevision;
	}
}

bool Document::Export( const std::filesystem::path& path, std::string& error ) const
{
	// Build the exported network from the document in a separate document, which reads the embedded files again
	Document exported( *m_loader, *m_resolver );
	if( !exported.Deserialize( Serialize(), error ) )
	{
		return false;
	}
	std::string problems;
	for( const auto& [reference, embedError] : exported.m_embeddedErrors )
	{
		problems.append( "\nEmbedded asset '" ).append( reference ).append( "': " ).append( embedError );
	}
	for( const auto& networkError : exported.GetNetwork().GetErrors() )
	{
		problems.append( "\n" ).append( networkError );
	}
	if( !problems.empty() )
	{
		error = "The network can't be exported:" + problems;
		return false;
	}
	std::vector<uint8_t> bytes;
	const auto result = exported.GetNetwork().Save( bytes );
	if( !result )
	{
		error = "The network can't be exported:\n" + result.error;
		return false;
	}
	return WriteBytes( path, bytes.data(), bytes.size(), error );
}

bool Document::IsModified() const
{
	return GetRevision() != m_savedRevision;
}

void Document::MarkUnmodified()
{
	m_savedRevision = GetRevision();
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
