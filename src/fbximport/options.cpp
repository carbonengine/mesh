#include "options.h"


bool NamedFilter::operator()( const std::string& name ) const
{
	if( m_names.empty() )
	{
		return true;
	}
	return std::find( m_names.begin(), m_names.end(), name ) != m_names.end();
}

void from_json( const nlohmann::json& j, MorphTargetOptions& p )
{
	p = {};
	if( j.contains( "import" ) )
	{
		j.at( "import" ).get_to( p.importMorphTargets );
	}
	if( j.contains( "useCustomNormals" ) )
	{
		j.at( "useCustomNormals" ).get_to( p.useCustomNormals );
	}
}

void to_json( nlohmann::json& j, const MorphTargetOptions& p )
{
	j = nlohmann::json{ { "import", p.importMorphTargets }, { "useCustomNormals", p.useCustomNormals } };
}

void from_json( const nlohmann::json& j, cmf::ElementType& p )
{
	p = cmf::ElementType::Float32;
	if( j.is_string() )
	{
		auto str = j.get<std::string>();
		if( str == "Float32" )
		{
			p = cmf::ElementType::Float32;
		}
		else if( str == "Float16" )
		{
			p = cmf::ElementType::Float16;
		}
		else if( str == "Uint16Norm" )
		{
			p = cmf::ElementType::UInt16Norm;
		}
		else if( str == "Uint16" )
		{
			p = cmf::ElementType::UInt16;
		}
		else if( str == "Int16Norm" )
		{
			p = cmf::ElementType::Int16Norm;
		}
		else if( str == "Int16" )
		{
			p = cmf::ElementType::Int16;
		}
		else if( str == "Uint8Norm" )
		{
			p = cmf::ElementType::UInt8Norm;
		}
		else if( str == "Uint8" )
		{
			p = cmf::ElementType::UInt8;
		}
		else if( str == "Int8Norm" )
		{
			p = cmf::ElementType::Int8Norm;
		}
		else if( str == "Int8" )
		{
			p = cmf::ElementType::Int8;
		}
		else
		{
			throw std::runtime_error( "invalid ElementType: " + str );
		}
	}
	else
	{
		throw std::runtime_error( "ElementType must be a string" );
	}
}

void to_json( nlohmann::json& j, const cmf::ElementType& p )
{
	std::string str;
	switch( p )
	{
	case cmf::ElementType::Float32:
		str = "Float32";
		break;
	case cmf::ElementType::Float16:
		str = "Float16";
		break;
	case cmf::ElementType::UInt16Norm:
		str = "Uint16Norm";
		break;
	case cmf::ElementType::UInt16:
		str = "Uint16";
		break;
	case cmf::ElementType::Int16Norm:
		str = "Int16Norm";
		break;
	case cmf::ElementType::Int16:
		str = "Int16";
		break;
	case cmf::ElementType::UInt8Norm:
		str = "Uint8Norm";
		break;
	case cmf::ElementType::UInt8:
		str = "Uint8";
		break;
	case cmf::ElementType::Int8Norm:
		str = "Int8Norm";
		break;
	case cmf::ElementType::Int8:
		str = "Int8";
		break;
	}
	j = str;
}

void from_json( const nlohmann::json& j, MeshImportOptions& p )
{
	p = {};
	if( j.contains( "import" ) )
	{
		j.at( "import" ).get_to( p.importMeshes );
	}
	if( j.contains( "filter" ) )
	{
		j.at( "filter" ).get_to( p.namedFilter.m_names );
	}
	if( j.contains( "normals" ) )
	{
		j.at( "normals" ).get_to( p.normals );
	}
	if( j.contains( "tangents" ) )
	{
		j.at( "tangents" ).get_to( p.tangents );
	}
	if( j.contains( "alwaysComputeTangents" ) )
	{
		j.at( "alwaysComputeTangents" ).get_to( p.alwaysComputeTangents );
	}
	if( j.contains( "compressTangents" ) )
	{
		j.at( "compressTangents" ).get_to( p.compressTangents );
	}
	if( j.contains( "legacyCompressedTangents" ) )
	{
		j.at( "legacyCompressedTangents" ).get_to( p.legacyCompressedTangents );
	}
	if( j.contains( "colors" ) )
	{
		j.at( "colors" ).get_to( p.colors );
	}
	if( j.contains( "skinning" ) )
	{
		j.at( "skinning" ).get_to( p.skinning );
	}
	if( j.contains( "bonesPerVertex" ) )
	{
		j.at( "bonesPerVertex" ).get_to( p.bonesPerVertex );
	}
	if( j.contains( "boneIndexType" ) )
	{
		j.at( "boneIndexType" ).get_to( p.boneIndexType );
	}
	if( j.contains( "regenerateNormals" ) )
	{
		j.at( "regenerateNormals" ).get_to( p.regenerateNormals );
	}
	if( j.contains( "uvSets" ) )
	{
		j.at( "uvSets" ).get_to( p.uvSets );
	}
	if( j.contains( "uvType" ) )
	{
		j.at( "uvType" ).get_to( p.uvType );
	}
	if( j.contains( "morphTargets" ) )
	{
		j.at( "morphTargets" ).get_to( p.morphTargets );
	}
}

void to_json( nlohmann::json& j, const MeshImportOptions& p )
{
	j = nlohmann::json{
		{ "import", p.importMeshes },
		{ "filter", p.namedFilter.m_names },
		{ "normals", p.normals },
		{ "tangents", p.tangents },
		{ "alwaysComputeTangents", p.alwaysComputeTangents },
		{ "compressTangents", p.compressTangents },
		{ "legacyCompressedTangents", p.legacyCompressedTangents },
		{ "colors", p.colors },
		{ "skinning", p.skinning },
		{ "bonesPerVertex", p.bonesPerVertex },
		{ "boneIndexType", p.boneIndexType },
		{ "regenerateNormals", p.regenerateNormals },
		{ "uvSets", p.uvSets },
		{ "uvType", p.uvType },
		{ "morphTargets", p.morphTargets }
	};
}

void from_json( const nlohmann::json& j, SkeletonImportOptions& p )
{
	p = {};
	if( j.contains( "import" ) )
	{
		j.at( "import" ).get_to( p.importSkeletons );
	}
	if( j.contains( "filter" ) )
	{
		j.at( "filter" ).get_to( p.namedFilter.m_names );
	}
	if( j.contains( "moveToOrigin" ) )
	{
		j.at( "moveToOrigin" ).get_to( p.moveToOrigin );
	}
}

void to_json( nlohmann::json& j, const SkeletonImportOptions& p )
{
	j = nlohmann::json{ { "import", p.importSkeletons }, { "filter", p.namedFilter.m_names }, { "moveToOrigin", p.moveToOrigin } };
}

void from_json( const nlohmann::json& j, AnimationImportOptions& p )
{
	p = {};
	if( j.contains( "import" ) )
	{
		j.at( "import" ).get_to( p.importAnimations );
	}
	if( j.contains( "filter" ) )
	{
		j.at( "filter" ).get_to( p.namedFilter.m_names );
	}
	if( j.contains( "moveToOrigin" ) )
	{
		j.at( "moveToOrigin" ).get_to( p.moveToOrigin );
	}
}

void to_json( nlohmann::json& j, const AnimationImportOptions& p )
{
	j = nlohmann::json{ { "import", p.importAnimations }, { "filter", p.namedFilter.m_names }, { "moveToOrigin", p.moveToOrigin } };
}

void from_json( const nlohmann::json& j, ImportOptions& p )
{
	p = {};
	if( j.contains( "mesh" ) )
	{
		j.at( "mesh" ).get_to( p.meshOptions );
	}
	if( j.contains( "skeleton" ) )
	{
		j.at( "skeleton" ).get_to( p.skeletonOptions );
	}
	if( j.contains( "animation" ) )
	{
		j.at( "animation" ).get_to( p.animationOptions );
	}
}

void to_json( nlohmann::json& j, const ImportOptions& p )
{
	j = nlohmann::json{ { "mesh", p.meshOptions }, { "skeleton", p.skeletonOptions }, { "animation", p.animationOptions } };
}

void ValidateOptions( const ImportOptions& options )
{
	if( options.meshOptions.tangents > 0 && !options.meshOptions.normals )
	{
		throw std::runtime_error( "tangents cannot be imported/computed if normals are not imported" );
	}
	if( options.meshOptions.bonesPerVertex != 1 && options.meshOptions.bonesPerVertex != 4 )
	{
		throw std::runtime_error( "bonesPerVertex must be either 1 or 4" );
	}
	if( options.meshOptions.tangents > options.meshOptions.uvSets )
	{
		throw std::runtime_error( "tangents cannot be imported/computed if the number of UV sets is less than the number of tangent spaces" );
	}
	if( options.meshOptions.boneIndexType != cmf::ElementType::UInt8 && options.meshOptions.boneIndexType != cmf::ElementType::UInt16 )
	{
		throw std::runtime_error( "boneIndexType must be either UInt8 or UInt16" );
	}
}