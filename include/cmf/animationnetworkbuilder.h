// Copyright © 2026 CCP ehf.

#pragma once

#include "animationnetwork.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cmf
{

class AnimationNetworkBuilder;

/** @brief Stable handle of a node within an AnimationGraphBuilder. Handles of removed nodes are never reused. */
struct AnimationNodeId
{
	static constexpr uint32_t INVALID_INDEX = 0xffffffff;

	/** Slot index of the node in its graph. */
	uint32_t index = INVALID_INDEX;
	/** Generation of the slot; distinguishes nodes that reuse the same slot. */
	uint32_t generation = 0;

	/** @brief Returns true if the handle refers to a node (it may still be stale). */
	[[nodiscard]] bool IsSet() const
	{
		return index != INVALID_INDEX;
	}

	bool operator==( const AnimationNodeId& other ) const
	{
		return index == other.index && generation == other.generation;
	}

	bool operator!=( const AnimationNodeId& other ) const
	{
		return !( *this == other );
	}
};

/** @brief Reference to an input or output socket of a node. */
struct AnimationSocketRef
{
	/** Node that owns the socket. */
	AnimationNodeId node;
	/** Socket index within the node's inputs or outputs. */
	uint32_t socket = 0;

	bool operator==( const AnimationSocketRef& other ) const
	{
		return node == other.node && socket == other.socket;
	}

	bool operator!=( const AnimationSocketRef& other ) const
	{
		return !( *this == other );
	}
};

/** @brief Directed edge from an output socket to an input socket. */
struct AnimationEdge
{
	/** Output socket the data comes from. */
	AnimationSocketRef from;
	/** Input socket the data goes to. */
	AnimationSocketRef to;
};

/** @brief Weight of a single bone, referenced by name. */
struct AnimationNamedBoneWeight
{
	/** Bone name in the network skeleton. */
	std::string bone;
	/** Bone weight. */
	float weight = 1.f;
};

/** @brief Control parameter of an animation network. */
struct AnimationParameter
{
	/** Unique parameter name. */
	std::string name;
	/** Value the parameter has when an instance is created. */
	float defaultValue = 0.f;
	/** Smallest allowed value; values set on instances are clamped to [minValue, maxValue]. */
	float minValue = -std::numeric_limits<float>::infinity();
	/** Largest allowed value. */
	float maxValue = std::numeric_limits<float>::infinity();
};

/** @brief Settings of SubgraphInput and Output nodes: a socket of the graph interface. */
struct AnimationInterfaceSettings
{
	/** Socket name. */
	std::string name;
	/** Socket type. The Output node of the root graph is always a Pose. */
	AnimationSocketType type = AnimationSocketType::Pose;
};

/** @brief Settings of Parameter nodes. */
struct AnimationParameterSettings
{
	/** Index of the network parameter. */
	uint32_t parameterIndex = 0;
};

/** @brief Settings of AnimationSample nodes. */
struct AnimationSampleSettings
{
	/** Asset path of the CMF file containing the animation. */
	std::string assetPath;
	/** Animation name; empty selects the first animation. */
	std::string animationName;
};

/** @brief Settings of Switch nodes. */
struct AnimationSwitchSettings
{
	/** Number of pose inputs. */
	uint32_t poseInputCount = 2;
};

/** @brief Settings of RandomSwitch nodes. */
struct AnimationRandomSwitchSettings
{
	/** Number of pose inputs. */
	uint32_t poseInputCount = 2;
	/** Relative probability of each pose input. Empty means uniform. */
	std::vector<float> weights;
	/** Pick a new input when the node becomes active after being inactive. */
	bool rerollOnActivate = false;
};

/** @brief Settings of NamedBoneWeights nodes. */
struct AnimationNamedBoneWeightsSettings
{
	/** Name of the skeleton bone mask. */
	std::string maskName;
};

/** @brief Settings of BoneWeights nodes. */
struct AnimationBoneWeightsSettings
{
	/** Weights of individual bones. */
	std::vector<AnimationNamedBoneWeight> weights;
	/** Weight of bones not listed in weights. */
	float defaultWeight = 0.f;
};

/** @brief Settings of Expression nodes. */
struct AnimationExpressionSettings
{
	/** Expression source text. */
	std::string expression;
	/** Input variable names; each one is a Float input socket of the node. */
	std::vector<std::string> inputNames;
};

/** @brief Type specific node settings. Node types without settings (RestPose, Blend, AdditiveBlend, Subgraph) hold
* std::monostate; the graph of a Subgraph node is accessed with AnimationGraphBuilder::GetSubgraph. */
using AnimationNodeSettings = std::variant<std::monostate, AnimationInterfaceSettings, AnimationParameterSettings, AnimationSampleSettings, AnimationSwitchSettings, AnimationRandomSwitchSettings, AnimationNamedBoneWeightsSettings, AnimationBoneWeightsSettings, AnimationExpressionSettings>;

/** @brief An animation network node. */
struct AnimationNode
{
	/** Node type. */
	AnimationNodeType type = AnimationNodeType::RestPose;
	/** Optional display name. */
	std::string name;
	/** Constant values of the node inputs, used for Float inputs that have no incoming edge. One value per input. */
	std::vector<float> inputConstants;
	/** Type specific settings; use AnimationGraphBuilder::GetSettings to access them. */
	AnimationNodeSettings settings;
};

/** @brief A graph of animation nodes: the root graph of an AnimationNetworkBuilder, or the contents of a Subgraph node.
*
* Graphs are owned by their network builder and can't be created directly. All modifications change the builder's
* generation (see AnimationNetworkBuilder::GetGeneration).
*/
class AnimationGraphBuilder
{
public:
	AnimationGraphBuilder( const AnimationGraphBuilder& ) = delete;
	AnimationGraphBuilder( AnimationGraphBuilder&& ) = delete;
	AnimationGraphBuilder& operator=( const AnimationGraphBuilder& ) = delete;
	AnimationGraphBuilder& operator=( AnimationGraphBuilder&& ) = delete;
	CARBON_MESH_EXPORT ~AnimationGraphBuilder();

	/** @brief Adds a node with default settings.
	* @return Handle of the new node, or an unset handle if the node can't be added (a second Output node or a
	* SubgraphInput node in the root graph).
	*/
	CARBON_MESH_EXPORT AnimationNodeId AddNode( AnimationNodeType type );

	/** @brief Removes a node together with all edges connected to it.
	* @return false if the handle is invalid.
	*/
	CARBON_MESH_EXPORT bool RemoveNode( AnimationNodeId node );

	/** @brief Returns true if the handle refers to a node in this graph. */
	[[nodiscard]] CARBON_MESH_EXPORT bool IsValid( AnimationNodeId node ) const;

	/** @brief Returns handles of all nodes in the graph, in slot order. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<AnimationNodeId> GetNodes() const;

	/** @brief Returns the node, or nullptr if the handle is invalid. */
	[[nodiscard]] CARBON_MESH_EXPORT const AnimationNode* GetNode( AnimationNodeId node ) const;

	/** @brief Returns the type specific settings of a node, or nullptr if the handle is invalid or the node has
	* settings of a different type. For example: `graph.GetSettings<AnimationSampleSettings>( node )`. */
	template <typename T>
	[[nodiscard]] const T* GetSettings( AnimationNodeId node ) const // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
	{
		const auto* data = GetNode( node );
		return data ? std::get_if<T>( &data->settings ) : nullptr;
	}

	/** @brief Returns the Output node of the graph (the first one for subgraphs), or an unset handle. */
	[[nodiscard]] CARBON_MESH_EXPORT AnimationNodeId GetOutputNode() const;

	/** @brief Returns the SubgraphInput nodes in interface order. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::vector<AnimationNodeId>& GetInterfaceInputs() const;

	/** @brief Returns the Output nodes in interface order. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::vector<AnimationNodeId>& GetInterfaceOutputs() const;

	/** @brief Returns the parent graph for a subgraph, nullptr for the root graph. */
	[[nodiscard]] CARBON_MESH_EXPORT const AnimationGraphBuilder* GetParent() const;

	/** @brief Returns the network the graph belongs to. */
	[[nodiscard]] CARBON_MESH_EXPORT const AnimationNetworkBuilder& GetNetwork() const;

	/** @brief Sets the display name of a node. */
	CARBON_MESH_EXPORT ValidationResult SetNodeName( AnimationNodeId node, std::string_view name );

	/** @brief Returns the number of input sockets of a node, 0 for invalid handles. */
	[[nodiscard]] CARBON_MESH_EXPORT uint32_t GetInputCount( AnimationNodeId node ) const;
	/** @brief Returns the number of output sockets of a node, 0 for invalid handles. */
	[[nodiscard]] CARBON_MESH_EXPORT uint32_t GetOutputCount( AnimationNodeId node ) const;
	/** @brief Returns the type of an input socket. The socket must exist. */
	[[nodiscard]] CARBON_MESH_EXPORT AnimationSocketType GetInputType( AnimationSocketRef input ) const;
	/** @brief Returns the type of an output socket. The socket must exist. */
	[[nodiscard]] CARBON_MESH_EXPORT AnimationSocketType GetOutputType( AnimationSocketRef output ) const;
	/** @brief Returns the name of an input socket, or an empty string if it doesn't exist. */
	[[nodiscard]] CARBON_MESH_EXPORT std::string GetInputName( AnimationSocketRef input ) const;
	/** @brief Returns the name of an output socket, or an empty string if it doesn't exist. */
	[[nodiscard]] CARBON_MESH_EXPORT std::string GetOutputName( AnimationSocketRef output ) const;

	/** @brief Connects an output socket to an input socket.
	* Fails if the sockets don't exist, the types differ, the input already has an incoming edge, or the edge would
	* create a cycle.
	*/
	CARBON_MESH_EXPORT ValidationResult Connect( AnimationSocketRef from, AnimationSocketRef to );

	/** @brief Removes the edge connected to an input socket.
	* @return false if the input has no incoming edge.
	*/
	CARBON_MESH_EXPORT bool Disconnect( AnimationSocketRef to );

	/** @brief Returns the output socket connected to an input socket, if any. */
	[[nodiscard]] CARBON_MESH_EXPORT std::optional<AnimationSocketRef> GetConnection( AnimationSocketRef to ) const;

	/** @brief Returns all edges in the graph. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::vector<AnimationEdge>& GetEdges() const;

	/** @brief Sets the constant used by a Float input without an incoming edge. */
	CARBON_MESH_EXPORT ValidationResult SetInputConstant( AnimationSocketRef input, float value );

	/** @brief Sets the animation sampled by an AnimationSample node.
	* @param assetPath Path of the CMF file, resolved by the network's asset loader or the embedded assets.
	* @param animationName Name of the animation in the file; empty selects the first animation.
	*/
	CARBON_MESH_EXPORT ValidationResult SetAnimation( AnimationNodeId node, std::string_view assetPath, std::string_view animationName );

	/** @brief Sets the number of pose inputs of a Switch or RandomSwitch node. Edges to removed inputs are removed. */
	CARBON_MESH_EXPORT ValidationResult SetPoseInputCount( AnimationNodeId node, uint32_t count );

	/** @brief Sets the relative probabilities of the pose inputs of a RandomSwitch node; empty means uniform. */
	CARBON_MESH_EXPORT ValidationResult SetRandomWeights( AnimationNodeId node, std::vector<float> weights );

	/** @brief Sets whether a RandomSwitch node picks a new input when it becomes active after being inactive. */
	CARBON_MESH_EXPORT ValidationResult SetRerollOnActivate( AnimationNodeId node, bool reroll );

	/** @brief Sets the skeleton bone mask name used by a NamedBoneWeights node. */
	CARBON_MESH_EXPORT ValidationResult SetBoneMask( AnimationNodeId node, std::string_view maskName );

	/** @brief Sets the per-bone weights of a BoneWeights node. */
	CARBON_MESH_EXPORT ValidationResult SetBoneWeights( AnimationNodeId node, std::vector<AnimationNamedBoneWeight> weights, float defaultWeight );

	/** @brief Sets the expression of an Expression node.
	* The expression is parsed immediately; parse errors are returned and leave the node unchanged.
	* @param expression Expression text, for example `clamp01( speed / 4 )`.
	* @param inputNames Names of the variables used in the expression; each becomes a Float input socket.
	*/
	CARBON_MESH_EXPORT ValidationResult SetExpression( AnimationNodeId node, std::string_view expression, std::vector<std::string> inputNames );

	/** @brief Sets the network parameter output by a Parameter node. */
	CARBON_MESH_EXPORT ValidationResult SetParameter( AnimationNodeId node, uint32_t parameterIndex );

	/** @brief Sets the name and type of a subgraph interface socket (a SubgraphInput or subgraph Output node).
	* The Output node of the root graph is always a Pose. Edges to the changed socket in the parent graph are removed
	* if the type changes.
	*/
	CARBON_MESH_EXPORT ValidationResult SetInterfaceSocket( AnimationNodeId node, std::string_view name, AnimationSocketType type );

	/** @brief Returns the graph contained in a Subgraph node, or nullptr. */
	[[nodiscard]] CARBON_MESH_EXPORT AnimationGraphBuilder* GetSubgraph( AnimationNodeId node );
	/** @brief Returns the graph contained in a Subgraph node, or nullptr. */
	[[nodiscard]] CARBON_MESH_EXPORT const AnimationGraphBuilder* GetSubgraph( AnimationNodeId node ) const;

private:
	friend class AnimationNetworkBuilder;

	struct Slot
	{
		uint32_t generation = 0;
		bool alive = false;
		AnimationNode node;
		std::unique_ptr<AnimationGraphBuilder> subgraph;
	};

	AnimationGraphBuilder( AnimationNetworkBuilder& network, AnimationGraphBuilder* parent, AnimationNodeId ownerNode );

	Slot* FindSlot( AnimationNodeId node );
	[[nodiscard]] const Slot* FindSlot( AnimationNodeId node ) const;
	Slot* FindSlot( AnimationNodeId node, AnimationNodeType type, ValidationResult& result );
	[[nodiscard]] bool IsReachable( AnimationNodeId from, AnimationNodeId to ) const;
	[[nodiscard]] static AnimationSocketType GetSocketType( const Slot& slot, uint32_t socket, bool input );
	void RemoveEdgesIf( const std::function<bool( const AnimationEdge& )>& predicate );
	void OnInterfaceSocketRemoved( bool input, uint32_t socket );
	void OnInterfaceSocketChanged( bool input, uint32_t socket );
	void ReleaseAssets();
	void MarkModified();

	AnimationNetworkBuilder* m_network = nullptr;
	AnimationGraphBuilder* m_parent = nullptr;
	AnimationNodeId m_ownerNode;
	std::vector<Slot> m_slots;
	std::vector<AnimationEdge> m_edges;
	std::vector<AnimationNodeId> m_interfaceInputs;
	std::vector<AnimationNodeId> m_interfaceOutputs;
};

/** @brief A graph of a network and its nodes in the order AnimationNetworkBuilder::Save stores them. */
struct AnimationGraphLayout
{
	/** The graph. */
	const AnimationGraphBuilder* graph = nullptr;
	/** Live nodes of the graph; the position of a node is its index in the saved graph. */
	std::vector<AnimationNodeId> nodes;
};

/** @brief Editable animation network: a graph of nodes that combines skeleton poses, a set of control parameters
* and references to the skeleton and animations it uses.
*
* The builder is meant for tools. It favors simple data structures over memory efficiency and saves networks in the
* CAN format (Save), which AnimationNetwork loads for playback. Load reads a saved network back for editing.
*
* Assets referenced by path are loaded through the AnimationAssetLoader passed to the constructor unless an
* embedded asset with the same path exists (see EmbedAsset); they are needed to validate the network against the
* skeleton and animations. Loading may complete asynchronously; IsReady returns true once all referenced assets are
* loaded and the network has no errors.
*/
class AnimationNetworkBuilder
{
public:
	static constexpr uint32_t INVALID_PARAMETER = 0xffffffff;

	/** @brief Creates an empty network. The loader must outlive the network. */
	CARBON_MESH_EXPORT explicit AnimationNetworkBuilder( AnimationAssetLoader& loader );
	CARBON_MESH_EXPORT ~AnimationNetworkBuilder();

	AnimationNetworkBuilder( const AnimationNetworkBuilder& ) = delete;
	AnimationNetworkBuilder( AnimationNetworkBuilder&& ) = delete;
	AnimationNetworkBuilder& operator=( const AnimationNetworkBuilder& ) = delete;
	AnimationNetworkBuilder& operator=( AnimationNetworkBuilder&& ) = delete;

	/** @brief Sets the skeleton the network animates.
	* @param assetPath Path of the CMF file containing the skeleton.
	* @param skeletonName Name of the skeleton in the file; empty selects the first skeleton.
	*/
	CARBON_MESH_EXPORT void SetSkeleton( std::string_view assetPath, std::string_view skeletonName );
	/** @brief Returns the asset path of the skeleton. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::string& GetSkeletonPath() const;
	/** @brief Returns the skeleton name. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::string& GetSkeletonName() const;
	/** @brief Returns the skeleton if it is loaded and found, otherwise nullptr. */
	[[nodiscard]] CARBON_MESH_EXPORT const Skeleton* GetSkeleton() const;

	/** @brief Embeds a CMF file into the network. References to the path resolve to the embedded data instead of
	* going through the asset loader, and the file is stored when the network is saved.
	* @param assetPath Path under which the asset is referenced.
	* @param cmfFile Contents of a CMF file. The file is validated.
	*/
	CARBON_MESH_EXPORT ValidationResult EmbedAsset( std::string_view assetPath, std::vector<uint8_t> cmfFile );
	/** @brief Removes an embedded asset; references to its path go through the asset loader afterwards.
	* @return false if no asset is embedded under the path.
	*/
	CARBON_MESH_EXPORT bool RemoveEmbeddedAsset( std::string_view assetPath );
	/** @brief Returns the paths of all embedded assets. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<std::string> GetEmbeddedAssetPaths() const;
	/** @brief Returns the contents of an embedded CMF file, or nullptr. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::vector<uint8_t>* GetEmbeddedAsset( std::string_view assetPath ) const;

	/** @brief Adds a control parameter.
	* @return Index of the new parameter, or INVALID_PARAMETER if the name is empty or already used.
	*/
	CARBON_MESH_EXPORT uint32_t AddParameter( const AnimationParameter& parameter );
	/** @brief Changes a control parameter. Fails if the index is invalid or the new name is used by another parameter. */
	CARBON_MESH_EXPORT ValidationResult SetParameter( uint32_t index, const AnimationParameter& parameter );
	/** @brief Removes a control parameter. Fails if a Parameter node references it. Parameters after it shift down by one. */
	CARBON_MESH_EXPORT ValidationResult RemoveParameter( uint32_t index );
	/** @brief Returns the index of the parameter with the given name, or INVALID_PARAMETER. */
	[[nodiscard]] CARBON_MESH_EXPORT uint32_t FindParameter( std::string_view name ) const;
	/** @brief Returns all control parameters. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::vector<AnimationParameter>& GetParameters() const;

	/** @brief Returns the root graph. */
	[[nodiscard]] CARBON_MESH_EXPORT AnimationGraphBuilder& GetRootGraph();
	/** @brief Returns the root graph. */
	[[nodiscard]] CARBON_MESH_EXPORT const AnimationGraphBuilder& GetRootGraph() const;

	/** @brief Returns true if some referenced assets are still loading. */
	[[nodiscard]] CARBON_MESH_EXPORT bool HasPendingAssets() const;
	/** @brief Returns descriptions of all problems that prevent the network from being evaluated. Assets that are
	* still loading are not reported as errors. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<std::string> GetErrors() const;
	/** @brief Returns descriptions of problems the network works around when it is evaluated: animations that are
	* not set, failed to load or are missing play the rest pose, missing bone masks weight all bones fully and weights
	* of unknown bones are ignored. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<std::string> GetWarnings() const;
	/** @brief Returns true if all assets are loaded and the network has no errors. It may have warnings. */
	[[nodiscard]] CARBON_MESH_EXPORT bool IsReady() const;

	/** @brief Returns the loaded data of a referenced asset, or nullptr if it isn't referenced, loaded or failed. */
	[[nodiscard]] CARBON_MESH_EXPORT const cmf::Data* GetAssetData( std::string_view assetPath ) const;

	/** @brief Saves the network in the CAN format. Node handles are not stored: nodes are numbered in slot order
	* without removed nodes, so handles change when a network with removed nodes is saved and loaded again. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<uint8_t> Save() const;

	/** @brief Replaces the network with the contents of a CAN file.
	* @return An error if the file is malformed; the network is unchanged in that case.
	*/
	CARBON_MESH_EXPORT ValidationResult Load( const void* file, size_t size );

	/** @brief Returns all graphs in the order Save stores them, starting with the root graph, with their live nodes
	* in stored order. Use it to associate data with nodes of a saved file. Right after Load of a file written by Save,
	* the layout matches the file. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<AnimationGraphLayout> GetSaveLayout() const;

	/** @brief Sets data of an editing tool, stored in the EditorData section of saved files. The data is opaque to
	* the network; changing it does not change the generation. Empty data omits the section. */
	CARBON_MESH_EXPORT void SetEditorData( std::vector<uint8_t> data );
	/** @brief Returns the editor data set with SetEditorData or read by Load. */
	[[nodiscard]] CARBON_MESH_EXPORT const std::vector<uint8_t>& GetEditorData() const;

	/** @brief Removes all nodes, parameters, embedded assets, editor data and the skeleton reference. */
	CARBON_MESH_EXPORT void Clear();

	/** @brief Returns a counter that changes whenever the network or one of its assets changes, for example to
	* track unsaved changes in an editor. */
	[[nodiscard]] CARBON_MESH_EXPORT uint64_t GetGeneration() const;

private:
	friend class AnimationGraphBuilder;
	class Serializer; // NOLINT(readability-identifier-naming): nested class name follows ClassCase

	enum class AssetState : uint8_t
	{
		Pending,
		Loaded,
		Failed,
	};

	struct AssetEntry
	{
		AssetState state = AssetState::Pending;
		const cmf::Data* data = nullptr;
		uint32_t referenceCount = 0;
		uint32_t requestId = 0;
		bool embedded = false;
	};

	struct EmbeddedAsset
	{
		/** Original file contents, as stored when saving the network. */
		std::vector<uint8_t> file;
		/** Copy of the file with offsets converted to pointers; data points into it. */
		std::vector<uint8_t> loaded;
		const cmf::Data* data = nullptr;
	};

	void AcquireAsset( const std::string& assetPath );
	void ReleaseAsset( const std::string& assetPath );
	void RequestFromLoader( const std::string& assetPath, AssetEntry& entry );
	void OnAssetLoaded( uint32_t requestId, const cmf::Data* data );
	void CollectProblems( std::vector<std::string>* errors, std::vector<std::string>* warnings ) const;
	void ValidateNode( const AnimationGraphBuilder& graph, AnimationNodeId id, const std::string& prefix, std::vector<std::string>& errors, std::vector<std::string>& warnings ) const;
	void ValidateAnimationSample( const AnimationSampleSettings& settings, const std::string& prefix, std::vector<std::string>& warnings ) const;
	[[nodiscard]] bool IsParameterReferenced( uint32_t index ) const;
	void ShiftParameterReferences( uint32_t removedIndex );
	void MarkModified();

	AnimationAssetLoader* m_loader = nullptr;
	std::shared_ptr<bool> m_aliveToken;
	std::map<std::string, AssetEntry, std::less<>> m_assets;
	std::map<std::string, EmbeddedAsset, std::less<>> m_embeddedAssets;
	uint32_t m_nextRequestId = 1;
	std::string m_skeletonPath;
	std::string m_skeletonName;
	std::vector<AnimationParameter> m_parameters;
	uint64_t m_generation = 1;
	std::vector<uint8_t> m_editorData;
	std::unique_ptr<AnimationGraphBuilder> m_rootGraph;
};

}
