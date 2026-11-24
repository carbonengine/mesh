// Copyright © 2026 CCP ehf.

#pragma once

#include "can.h"
#include "cmf.h"
#include "utils.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cmf
{

namespace detail
{
struct CompiledNetwork; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase struct
}

class AnimationNetworkInstance; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase class

/** @brief Type of an animation network node. See can::v1::NodeType for the sockets of each type. */
using AnimationNodeType = can::NodeType;

/** @brief Type of data that flows through an animation network edge. */
using AnimationSocketType = can::SocketType;

/** @brief Resolves asset paths referenced by an animation network to loaded CMF data.
*
* Threading contract: networks call RequestLoad and Release from a single thread, and the loader must invoke the
* onLoaded callback on that same thread. The callback may be invoked before RequestLoad returns (for example, when
* the asset is already cached) or at any later time. Because of this contract networks don't use any synchronization.
*
* The loader owns the loaded data. Data passed to a callback must stay valid until Release is called for the path.
* The loader must outlive all networks that use it.
*/
class AnimationAssetLoader
{
public:
	/** Callback receiving the loaded data, or nullptr if loading failed. */
	using Callback = std::function<void( const cmf::Data* )>;

	AnimationAssetLoader() = default;
	AnimationAssetLoader( const AnimationAssetLoader& ) = default;
	AnimationAssetLoader( AnimationAssetLoader&& ) = default;
	AnimationAssetLoader& operator=( const AnimationAssetLoader& ) = default;
	AnimationAssetLoader& operator=( AnimationAssetLoader&& ) = default;
	virtual ~AnimationAssetLoader() = default;

	/** @brief Starts loading a CMF file.
	* @param path Asset path; it does not have to be a file system path.
	* @param onLoaded Callback invoked exactly once on the calling thread, possibly before this function returns.
	*/
	virtual void RequestLoad( std::string_view path, Callback onLoaded ) = 0;

	/** @brief Tells the loader that the network no longer needs the asset requested with RequestLoad.
	* Release is called once for every RequestLoad, also when loading is still in progress or failed.
	* @param path Asset path previously passed to RequestLoad.
	*/
	virtual void Release( std::string_view path ) = 0;
};

/** @brief A read-only animation network loaded from a CAN file.
*
* The network keeps the file buffer and reads the network description (can::v1::NetworkData) in place, so loading
* does not allocate memory per node. Networks are created and edited with AnimationNetworkBuilder, which saves them
* in the CAN format.
*
* External assets are loaded through the AnimationAssetLoader passed to the constructor; embedded assets are read
* from the file. Loading may complete asynchronously; IsReady returns true once all assets are loaded and the
* network has no errors. The network does not evaluate itself: create an AnimationNetworkInstance per animated
* object for that.
*/
class AnimationNetwork
{
public:
	/** @brief Creates an empty network. The loader must outlive the network. */
	CARBON_MESH_EXPORT explicit AnimationNetwork( AnimationAssetLoader& loader );
	CARBON_MESH_EXPORT ~AnimationNetwork();

	AnimationNetwork( const AnimationNetwork& ) = delete;
	AnimationNetwork( AnimationNetwork&& ) = delete;
	AnimationNetwork& operator=( const AnimationNetwork& ) = delete;
	AnimationNetwork& operator=( AnimationNetwork&& ) = delete;

	/** @brief Loads the network from the contents of a CAN file and starts loading its external assets.
	* The network takes ownership of the buffer. A previously loaded network is unloaded first.
	* @return An error if the file is malformed; the network is empty in that case.
	*/
	CARBON_MESH_EXPORT ValidationResult Load( std::vector<uint8_t> file );

	/** @brief Returns the network description, or nullptr if no network is loaded. */
	[[nodiscard]] CARBON_MESH_EXPORT const can::NetworkData* GetData() const;

	/** @brief Returns the loaded data of an asset in can::NetworkData::assets, or nullptr if it isn't loaded. */
	[[nodiscard]] CARBON_MESH_EXPORT const cmf::Data* GetAssetData( uint32_t assetIndex ) const;

	/** @brief Returns the skeleton if it is loaded and found, otherwise nullptr. */
	[[nodiscard]] CARBON_MESH_EXPORT const Skeleton* GetSkeleton() const;

	/** @brief Returns true if some assets are still loading. */
	[[nodiscard]] CARBON_MESH_EXPORT bool HasPendingAssets() const;

	/** @brief Returns descriptions of all problems that prevent the network from being evaluated. Assets that are
	* still loading are not reported as errors. */
	[[nodiscard]] CARBON_MESH_EXPORT std::vector<std::string> GetErrors() const;

	/** @brief Returns true if a network is loaded, all its assets are loaded and it has no errors, so that
	* AnimationNetworkInstance objects can be created. The network is prepared for evaluation when it becomes ready. */
	[[nodiscard]] CARBON_MESH_EXPORT bool IsReady() const;

private:
	enum class AssetState : uint8_t
	{
		Pending,
		Loaded,
		Failed,
	};

	struct AssetEntry
	{
		const cmf::Data* data = nullptr;
		AssetState state = AssetState::Pending;
	};

	friend class AnimationNetworkInstance;

	void Unload();
	void CompileIfReady();
	void OnAssetLoaded( uint32_t loadId, uint32_t assetIndex, const cmf::Data* data );
	void ValidateNode( const can::Graph& graph, const can::Node& node, const Skeleton* skeleton, const std::string& prefix, std::vector<std::string>& errors ) const;
	void ValidateAnimationSample( const can::AnimationSampleNode& node, const std::string& prefix, std::vector<std::string>& errors ) const;

	AnimationAssetLoader* m_loader = nullptr;
	/** Expires when the network is destroyed, so that late loader callbacks are ignored. */
	std::shared_ptr<bool> m_aliveToken;
	/** Identifies the current Load call, so that callbacks for a previously loaded file are ignored. */
	uint32_t m_loadId = 0;
	std::vector<uint8_t> m_file;
	const can::NetworkData* m_data = nullptr;
	std::vector<AssetEntry> m_assets;
	/** Evaluation data shared by all instances; created when the network becomes ready. */
	std::unique_ptr<detail::CompiledNetwork> m_compiled;
	/** Problems found while preparing the network for evaluation. */
	std::vector<std::string> m_compileErrors;
};

}
