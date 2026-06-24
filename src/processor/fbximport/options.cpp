// Copyright © 2026 CCP ehf.

#include "options.h"
#include "lodsimplygon.h"


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

namespace cmf::v1
{
void from_json( const nlohmann::json& j, ElementType& p )
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

void to_json( nlohmann::json& j, const ElementType& p )
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
}

void from_json( const nlohmann::json& j, SimplygonLodOptions& p )
{
	p = {};
	if( j.contains( "maxLods" ) )
	{
		j.at( "maxLods" ).get_to( p.maxLods );
	}
	if( j.contains( "geometryImportance" ) )
	{
		j.at( "geometryImportance" ).get_to( p.geometryImportance );
	}
	if( j.contains( "areaImportance" ) )
	{
		j.at( "areaImportance" ).get_to( p.areaImportance );
	}
	if( j.contains( "normalImportance" ) )
	{
		j.at( "normalImportance" ).get_to( p.normalImportance );
	}
	if( j.contains( "uvImportance" ) )
	{
		j.at( "uvImportance" ).get_to( p.uvImportance );
	}
	if( j.contains( "skinningImportance" ) )
	{
		j.at( "skinningImportance" ).get_to( p.skinningImportance );
	}
	if( j.contains( "vertexColorImportance" ) )
	{
		j.at( "vertexColorImportance" ).get_to( p.vertexColorImportance );
	}
	if( j.contains( "screenSizeFactor" ) )
	{
		j.at( "screenSizeFactor" ).get_to( p.screenSizeFactor );
	}
	if( j.contains( "lockVertexChannel" ) )
	{
		j.at( "lockVertexChannel" ).get_to( p.lockVertexChannel );
	}
}

void to_json( nlohmann::json& j, const SimplygonLodOptions& p )
{
	j = nlohmann::json{
		{ "maxLods", p.maxLods },
		{ "geometryImportance", p.geometryImportance },
		{ "areaImportance", p.areaImportance },
		{ "normalImportance", p.normalImportance },
		{ "uvImportance", p.uvImportance },
		{ "skinningImportance", p.skinningImportance },
		{ "vertexColorImportance", p.vertexColorImportance },
		{ "screenSizeFactor", p.screenSizeFactor },
		{ "lockVertexChannel", p.lockVertexChannel }
	};
}

void from_json( const nlohmann::json& j, SimplygonHoleFilling& p )
{
	p = SimplygonHoleFilling::Medium;
	if( j.is_string() )
	{
		auto str = j.get<std::string>();
		if( str == "Disabled" )
		{
			p = SimplygonHoleFilling::Disabled;
		}
		else if( str == "Low" )
		{
			p = SimplygonHoleFilling::Low;
		}
		else if( str == "Medium" )
		{
			p = SimplygonHoleFilling::Medium;
		}
		else if( str == "High" )
		{
			p = SimplygonHoleFilling::High;
		}
		else
		{
			throw std::runtime_error( "invalid SimplygonHoleFilling: " + str );
		}
	}
	else
	{
		throw std::runtime_error( "SimplygonHoleFilling must be a string" );
	}
}

void to_json( nlohmann::json& j, const SimplygonHoleFilling& p )
{
	std::string str;
	switch( p )
	{
	case SimplygonHoleFilling::Disabled:
		str = "Disabled";
		break;
	case SimplygonHoleFilling::Low:
		str = "Low";
		break;
	case SimplygonHoleFilling::Medium:
		str = "Medium";
		break;
	case SimplygonHoleFilling::High:
		str = "High";
		break;
	}
	j = str;
}

void from_json( const nlohmann::json& j, SimplygonAudioOcclusionMeshOptions& p )
{
	p = {};
	if( j.contains( "screenSize" ) )
	{
		j.at( "screenSize" ).get_to( p.screenSize );
	}
	if( j.contains( "holeFilling" ) )
	{
		j.at( "holeFilling" ).get_to( p.holeFilling );
	}
}

void to_json( nlohmann::json& j, const SimplygonAudioOcclusionMeshOptions& p )
{
	j = nlohmann::json{
		{ "screenSize", p.screenSize },
		{ "holeFilling", p.holeFilling }
	};
}

void from_json( const nlohmann::json& j, LodGenerationMethod& p )
{
	p = LodGenerationMethod::Simplygon;
	if( j.is_string() )
	{
		auto str = j.get<std::string>();
		if( str == "Simplygon" )
		{
			p = LodGenerationMethod::Simplygon;
		}
		else
		{
			throw std::runtime_error( "invalid LodGenerationMethod: " + str );
		}
	}
	else
	{
		throw std::runtime_error( "LodGenerationMethod must be a string" );
	}
}

void to_json( nlohmann::json& j, const LodGenerationMethod& p )
{
	std::string str;
	switch( p )
	{
	case LodGenerationMethod::Simplygon:
		str = "Simplygon";
		break;
	}
	j = str;
}

void from_json( const nlohmann::json& j, LodOptions& p )
{
	p = {};
	if( j.contains( "generate" ) )
	{
		j.at( "generate" ).get_to( p.generate );
	}
	if( j.contains( "method" ) )
	{
		j.at( "method" ).get_to( p.method );
	}
	if( j.contains( "simplygon" ) )
	{
		j.at( "simplygon" ).get_to( p.simplygon );
	}
}

void to_json( nlohmann::json& j, const LodOptions& p )
{
	j = nlohmann::json{
		{ "generate", p.generate },
		{ "method", p.method },
		{ "simplygon", p.simplygon }
	};
}

void from_json( const nlohmann::json& j, AudioOcclusionMeshGenerationMethod& p )
{
	p = AudioOcclusionMeshGenerationMethod::Simplygon;
	if( j.is_string() )
	{
		auto str = j.get<std::string>();
		if( str == "Simplygon" )
		{
			p = AudioOcclusionMeshGenerationMethod::Simplygon;
		}
		else
		{
			throw std::runtime_error( "invalid AudioOcclusionMeshGenerationMethod: " + str );
		}
	}
	else
	{
		throw std::runtime_error( "AudioOcclusionMeshGenerationMethod must be a string" );
	}
}

void to_json( nlohmann::json& j, const AudioOcclusionMeshGenerationMethod& p )
{
	std::string str;
	switch( p )
	{
	case AudioOcclusionMeshGenerationMethod::Simplygon:
		str = "Simplygon";
		break;
	}
	j = str;
}

void from_json( const nlohmann::json& j, AudioOcclusionMeshOptions& p )
{
	p = {};
	if( j.contains( "generate" ) )
	{
		j.at( "generate" ).get_to( p.generate );
	}
	if( j.contains( "method" ) )
	{
		j.at( "method" ).get_to( p.method );
	}
	if( j.contains( "simplygon" ) )
	{
		j.at( "simplygon" ).get_to( p.simplygon );
	}
}

void to_json( nlohmann::json& j, const AudioOcclusionMeshOptions& p )
{
	j = nlohmann::json{
		{ "generate", p.generate },
		{ "method", p.method },
		{ "simplygon", p.simplygon }
	};
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
	if( j.contains( "colorType" ) )
	{
		j.at( "colorType" ).get_to( p.colorType );
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
	if( j.contains( "flipV" ) )
	{
		j.at( "flipV" ).get_to( p.flipV );
	}
	if( j.contains( "uvType" ) )
	{
		j.at( "uvType" ).get_to( p.uvType );
	}
	if( j.contains( "morphTargets" ) )
	{
		j.at( "morphTargets" ).get_to( p.morphTargets );
	}
	if( j.contains( "lods" ) )
	{
		j.at( "lods" ).get_to( p.lods );
	}
	if( j.contains( "audioOcclusionMesh" ) )
	{
		j.at( "audioOcclusionMesh" ).get_to( p.audioOcclusionMesh );
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
		{ "colorType", p.colorType },
		{ "skinning", p.skinning },
		{ "bonesPerVertex", p.bonesPerVertex },
		{ "boneIndexType", p.boneIndexType },
		{ "regenerateNormals", p.regenerateNormals },
		{ "uvSets", p.uvSets },
		{ "flipV", p.flipV },
		{ "uvType", p.uvType },
		{ "morphTargets", p.morphTargets },
		{ "lods", p.lods }
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
	if( j.contains( "reduceKeyframes" ) )
	{
		j.at( "reduceKeyframes" ).get_to( p.reduceKeyframes );
	}
	if( j.contains( "keyReductionTolerance" ) )
	{
		j.at( "keyReductionTolerance" ).get_to( p.keyReductionTolerance );
	}
	if( j.contains( "optimizeFormat" ) )
	{
		j.at( "optimizeFormat" ).get_to( p.optimizeFormat );
	}
	if( j.contains( "keyTolerance" ) )
	{
		j.at( "keyTolerance" ).get_to( p.keyTolerance );
	}
	if( j.contains( "valueTolerance" ) )
	{
		j.at( "valueTolerance" ).get_to( p.valueTolerance );
	}
}

void to_json( nlohmann::json& j, const AnimationImportOptions& p )
{
	j = nlohmann::json{
		{ "import", p.importAnimations },
		{ "filter", p.namedFilter.m_names },
		{ "moveToOrigin", p.moveToOrigin },
		{ "reduceKeyframes", p.reduceKeyframes },
		{ "keyReductionTolerance", p.keyReductionTolerance },
		{ "optimizeFormat", p.optimizeFormat },
		{ "keyTolerance", p.keyTolerance },
		{ "valueTolerance", p.valueTolerance }
	};
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
	if( j.contains( "lowdetailSuffix" ) )
	{
		j.at( "lowdetailSuffix" ).get_to( p.lowdetailSuffix );
	}
	if( j.contains( "unitsPerMeter" ) )
	{
		j.at( "unitsPerMeter" ).get_to( p.unitsPerMeter );
	}
}

void to_json( nlohmann::json& j, const ImportOptions& p )
{
	j = nlohmann::json{
		{ "mesh", p.meshOptions },
		{ "skeleton", p.skeletonOptions },
		{ "animation", p.animationOptions },
		{ "lowdetailSuffix", p.lowdetailSuffix },
		{ "unitsPerMeter", p.unitsPerMeter }
	};
}

void ValidateOptions( const ImportOptions& options )
{
	if( options.meshOptions.colors >= LOCKED_VERTEX_USAGE_INDEX )
	{
		throw std::runtime_error( "the number of vertex color sets cannot be greater than " + std::to_string( LOCKED_VERTEX_USAGE_INDEX - 1 ) );
	}
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