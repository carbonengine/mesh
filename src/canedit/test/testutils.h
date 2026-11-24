// Copyright © 2026 CCP ehf.

#pragma once

#include "assetLoader.h"
#include "document.h"
#include "pathResolver.h"
#include "session.h"

#include "networktestutils.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

namespace canedittest
{

constexpr const char* PREFIX = "res";
constexpr const char* CHARACTER_FILE = "characters/character.cmf";
constexpr const char* CHARACTER_REFERENCE = "res:/characters/character.cmf";

/** A temporary directory that is removed with the object. */
class TemporaryDirectory
{
public:
	TemporaryDirectory()
	{
		static std::atomic<int> counter{ 0 };
		m_path = std::filesystem::temp_directory_path() / ( "canedittest-" + std::to_string( std::time( nullptr ) ) + "-" + std::to_string( counter++ ) );
		std::filesystem::create_directories( m_path );
	}
	~TemporaryDirectory()
	{
		try
		{
			std::error_code ec;
			std::filesystem::remove_all( m_path, ec );
		}
		catch( ... ) // NOLINT(bugprone-empty-catch): a leftover temporary directory must not fail the tests
		{
		}
	}
	TemporaryDirectory( const TemporaryDirectory& ) = delete;
	TemporaryDirectory& operator=( const TemporaryDirectory& ) = delete;
	TemporaryDirectory( TemporaryDirectory&& ) = delete;
	TemporaryDirectory& operator=( TemporaryDirectory&& ) = delete;

	[[nodiscard]] const std::filesystem::path& GetPath() const
	{
		return m_path;
	}

private:
	std::filesystem::path m_path;
};

inline void WriteFile( const std::filesystem::path& path, const std::vector<uint8_t>& bytes )
{
	std::filesystem::create_directories( path.parent_path() );
	std::ofstream stream( path, std::ios::binary );
	stream.write( reinterpret_cast<const char*>( bytes.data() ), static_cast<std::streamsize>( bytes.size() ) ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): streams write chars
}

/** A document whose assets are files in a temporary directory, mapped to the "res" prefix. The directory contains
* the test character with a skeleton, a bone mask and two animations. */
struct EditorFixture : public ::testing::Test
{
	networktest::TestAsset asset;
	TemporaryDirectory directory;
	canedit::PathResolver resolver;
	canedit::FileAssetLoader loader{ resolver };
	canedit::Document document{ loader };
	canedit::EditorSession session{ document };

	EditorFixture()
	{
		WriteFile( directory.GetPath() / CHARACTER_FILE, asset.BuildCmfFile() );
		resolver.SetPrefixes( { { PREFIX, directory.GetPath().u8string() } } );
		document.New();
		document.GetNetwork().SetSkeleton( CHARACTER_REFERENCE, networktest::SKELETON_NAME );
	}

	cmf::AnimationGraphBuilder& Root()
	{
		return document.GetNetwork().GetRootGraph();
	}

	cmf::AnimationNodeId Output()
	{
		return Root().GetOutputNode();
	}
};

}
