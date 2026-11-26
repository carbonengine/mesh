// Copyright © 2026 CCP ehf.

#pragma once

#include "pathResolver.h"

#include <cmf/animationnetwork.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace canedit
{

/** A CMF file loaded into memory, with its main data ready to use. */
class CmfFile
{
public:
	/** Reads and validates a CMF file. Returns nullptr and sets error on failure. */
	static std::shared_ptr<const CmfFile> Load( const std::filesystem::path& path, std::string& error );
	/** Validates CMF file contents. Returns nullptr and sets error on failure. */
	static std::shared_ptr<const CmfFile> FromBytes( std::vector<uint8_t> bytes, std::string& error );

	[[nodiscard]] const cmf::Data& GetData() const;
	/** Returns the file contents as read, with offsets (not pointers). */
	[[nodiscard]] const std::vector<uint8_t>& GetBytes() const;

private:
	std::vector<uint8_t> m_bytes;
	/** Copy of the file with the main data offsets converted to pointers. */
	std::vector<uint8_t> m_loaded;
	const cmf::Data* m_data = nullptr;
};

/** Loads network assets from files, resolving references with a PathResolver.
*
* Loading is synchronous. Files are cached while they are referenced, so that the network being edited and its
* preview share them. Failures are remembered per reference until the reference is no longer used or Reload is
* called, so that the editor can show them.
*/
class FileAssetLoader : public cmf::AnimationAssetLoader
{
public:
	explicit FileAssetLoader( const PathResolver& resolver );

	void RequestLoad( std::string_view path, Callback onLoaded ) override;
	void Release( std::string_view path ) override;

	/** Returns the load error of a referenced asset, or nullptr if it loaded (or isn't referenced). */
	[[nodiscard]] const std::string* GetError( std::string_view path ) const;
	/** Returns all referenced assets that failed to load, with their errors. */
	[[nodiscard]] std::vector<std::pair<std::string, std::string>> GetErrors() const;
	/** Returns true if the asset is referenced and loaded. */
	[[nodiscard]] bool IsLoaded( std::string_view path ) const;

	/** Loads a file that is not managed by networks (for example the preview mesh). Not cached. */
	[[nodiscard]] std::shared_ptr<const CmfFile> LoadUnmanaged( std::string_view path, std::string& error ) const;

	/** Forgets cached files that are not referenced. Files are read again when they are requested next time. */
	void Flush();

	/** Number of references to a path, for tests. */
	[[nodiscard]] uint32_t GetReferenceCount( std::string_view path ) const;

private:
	struct Entry
	{
		std::shared_ptr<const CmfFile> file;
		std::string error;
		uint32_t references = 0;
	};

	const PathResolver* m_resolver;
	std::map<std::string, Entry, std::less<>> m_entries;
};

}
