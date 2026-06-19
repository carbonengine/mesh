// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"

namespace
{
struct ExtractLodOptions
{
	std::string srcPath;
	std::string dstPath;
};

void ExtractLod( CLI::App& app, ExtractLodOptions& options )
{
	app.add_option( "src", options.srcPath, "Path to the source CMF file" )->required()->check( CLI::ExistingFile );
	app.add_option( "dst", options.dstPath, "Path to the output CMF file" )->required();

	app.final_callback( [&options]() {
		CmfFile file( options.srcPath );

		const bool hasLods = std::any_of( file.GetData().meshes.begin(), file.GetData().meshes.end(), []( const auto& mesh ) { return mesh.lods.size() > 1; } );
		if( !hasLods )
		{
			fprintf( stdout, "No meshes with multiple LODs in file %s; output not written\n", options.srcPath.c_str() ); // NOLINT(cppcoreguidelines-pro-type-vararg)
			return;
		}

		for( auto& mesh : file.GetData().meshes )
		{
			if( mesh.lods.size() > 1 )
			{
				auto lastLod = mesh.lods[mesh.lods.size() - 1];
				lastLod.threshold = cmf::MeshLod::MAX_THRESHOLD; // make sure the last LOD is visible at all distances
				mesh.lods = {};
				cmf::Modify( mesh.lods, file.GetAllocator() ).emplace_back( lastLod );
			}
			mesh.audioOcclusionMesh = {};
		}
		file.Save( options.dstPath );
	} );
}
}

REGISTER_COMMAND(
	"extractlod",
	"Creates a new file with only the last LOD of each mesh.\n"
	"If no mesh in the source file has more than one LOD, the output\n"
	"file is not created and the command finishes successfully.\n"
	"The command also removes audio occlusion mesh from the output file.",
	&ExtractLod );
