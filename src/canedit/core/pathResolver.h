// Copyright © 2026 CCP ehf.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canedit
{

/** A protocol prefix: references of the form "name:/rest" resolve to "directory/rest". */
struct PathPrefix
{
	std::string name;
	std::string directory;

	bool operator==( const PathPrefix& other ) const
	{
		return name == other.name && directory == other.directory;
	}
};

/** Resolves asset references stored in networks to physical file paths.
*
* References are file paths, unless they start with a registered prefix ("res:/characters/a.cmf"). Prefix names are
* at least two characters long, so that Windows drive letters ("c:/") are never taken for prefixes.
*/
class PathResolver
{
public:
	/** Minimum length of a prefix name. */
	static constexpr size_t MIN_PREFIX_LENGTH = 2;

	void SetPrefixes( std::vector<PathPrefix> prefixes );
	[[nodiscard]] const std::vector<PathPrefix>& GetPrefixes() const;

	/** Returns the physical path of a reference, or nullopt with an error if it uses an unknown prefix. */
	[[nodiscard]] std::optional<std::filesystem::path> Resolve( std::string_view reference, std::string* error = nullptr ) const;

	/** Returns a reference for a physical path: "prefix:/relative/path" if the path is inside the directory of a
	* prefix (the most specific one wins), otherwise the path itself with forward slashes. */
	[[nodiscard]] std::string MakeReference( const std::filesystem::path& path ) const;

	/** Splits "name:rest" into its prefix name and the rest, if the reference has a prefix of a valid length. */
	static bool SplitPrefix( std::string_view reference, std::string_view& name, std::string_view& rest );

	/** Returns an error message if a prefix name is not valid, an empty string otherwise. */
	static std::string ValidatePrefixName( std::string_view name );

private:
	std::vector<PathPrefix> m_prefixes;
};

}
