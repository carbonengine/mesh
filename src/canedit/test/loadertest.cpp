// Copyright © 2026 CCP ehf.

#include "testutils.h"

namespace canedittest
{
namespace
{

using PathResolver = canedit::PathResolver;

TEST( PathResolver, PlainPathsAreFilePaths )
{
	PathResolver resolver;
	resolver.SetPrefixes( { { "res", "/content" } } );
	EXPECT_EQ( resolver.Resolve( "characters/a.cmf" ), std::filesystem::path( "characters/a.cmf" ) );
	// Single letters are drive letters, not prefixes
	EXPECT_EQ( resolver.Resolve( "c:/content/a.cmf" ), std::filesystem::path( "c:/content/a.cmf" ) );
}

TEST( PathResolver, PrefixesMapToDirectories )
{
	PathResolver resolver;
	resolver.SetPrefixes( { { "res", "/content" } } );
	const auto expected = std::filesystem::path( "/content/characters/a.cmf" ).lexically_normal();
	EXPECT_EQ( resolver.Resolve( "res:/characters/a.cmf" ), expected );
	EXPECT_EQ( resolver.Resolve( "res:characters/a.cmf" ), expected );
	EXPECT_EQ( resolver.Resolve( "res://characters/a.cmf" ), expected );
}

TEST( PathResolver, UnknownPrefixesAreErrors )
{
	const PathResolver resolver;
	std::string error;
	EXPECT_FALSE( resolver.Resolve( "art:/a.cmf", &error ).has_value() );
	EXPECT_EQ( error, "Unknown path prefix 'art'" );
}

TEST( PathResolver, ReferencesUseTheMostSpecificPrefix )
{
	PathResolver resolver;
	resolver.SetPrefixes( { { "res", "/content" }, { "chars", "/content/characters/" } } );
	EXPECT_EQ( resolver.MakeReference( "/content/characters/soldier/a.cmf" ), "chars:/soldier/a.cmf" );
	EXPECT_EQ( resolver.MakeReference( "/content/props/b.cmf" ), "res:/props/b.cmf" );
	EXPECT_EQ( resolver.MakeReference( "/contents/b.cmf" ), "/contents/b.cmf" ) << "only whole directory names match";
	EXPECT_EQ( resolver.MakeReference( "/other/c.cmf" ), "/other/c.cmf" );
}

TEST( PathResolver, PrefixNamesAreValidated )
{
	EXPECT_TRUE( PathResolver::ValidatePrefixName( "res" ).empty() );
	EXPECT_FALSE( PathResolver::ValidatePrefixName( "c" ).empty() );
	EXPECT_FALSE( PathResolver::ValidatePrefixName( "my prefix" ).empty() );
}

TEST_F( EditorFixture, LoaderSharesFilesBetweenReferences )
{
	const cmf::Data* first = nullptr;
	const cmf::Data* second = nullptr;
	loader.RequestLoad( CHARACTER_REFERENCE, [&first]( const cmf::Data* data ) { first = data; } );
	loader.RequestLoad( CHARACTER_REFERENCE, [&second]( const cmf::Data* data ) { second = data; } );
	ASSERT_NE( first, nullptr );
	EXPECT_EQ( first, second );
	EXPECT_EQ( first->skeletons.size(), 1u );
	// The document's skeleton reference is the third one
	EXPECT_EQ( loader.GetReferenceCount( CHARACTER_REFERENCE ), 3u );
	loader.Release( CHARACTER_REFERENCE );
	loader.Release( CHARACTER_REFERENCE );
	EXPECT_EQ( loader.GetReferenceCount( CHARACTER_REFERENCE ), 1u );
	EXPECT_TRUE( loader.IsLoaded( CHARACTER_REFERENCE ) );
}

TEST_F( EditorFixture, LoaderReportsMissingFilesWhileTheyAreReferenced )
{
	const cmf::Data* data = &asset.data;
	loader.RequestLoad( "res:/missing.cmf", [&data]( const cmf::Data* loaded ) { data = loaded; } );
	EXPECT_EQ( data, nullptr );
	ASSERT_NE( loader.GetError( "res:/missing.cmf" ), nullptr );
	EXPECT_NE( loader.GetError( "res:/missing.cmf" )->find( "Cannot open" ), std::string::npos );
	EXPECT_EQ( loader.GetErrors().size(), 1u );

	loader.Release( "res:/missing.cmf" );
	EXPECT_EQ( loader.GetError( "res:/missing.cmf" ), nullptr );
	EXPECT_TRUE( loader.GetErrors().empty() );
}

TEST_F( EditorFixture, LoaderReportsInvalidFiles )
{
	WriteFile( directory.GetPath() / "broken.cmf", { 1, 2, 3 } );
	loader.RequestLoad( "res:/broken.cmf", []( const cmf::Data* ) {} );
	ASSERT_NE( loader.GetError( "res:/broken.cmf" ), nullptr );
	EXPECT_NE( loader.GetError( "res:/broken.cmf" )->find( "invalid CMF file" ), std::string::npos );
	loader.Release( "res:/broken.cmf" );
}

}
}
