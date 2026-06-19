// Copyright © 2026 CCP ehf.

#include "cmf/cmf.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include "cmf/tangents.h"

#include "lodsimplygon.h"
#include "options.h"

#if WITH_SIMPLYGON

// Include Simplygon loader implementation directly
#include "SimplygonLoader.cpp"

namespace
{

constexpr float TARGET_RATIO = 0.9f; // Target ratio for reduction, used to calculate the target triangle count
constexpr float VERTEX_WELDING_THRESHOLD = 0.0001f;
constexpr float T_JUNCTION_THRESHOLD = 0.00001f;
constexpr uint32_t MAX_SCREEN_SIZE = 2048; // After applying the factor
constexpr uint32_t MIN_SCREEN_SIZE = 20; // Simplygon min screen size limit

/** Helper function to check Simplygon error codes and throw exceptions. */
void AssertSimplygonError( Simplygon::EErrorCodes ec, const std::string& message )
{
	if( ec != Simplygon::EErrorCodes::NoError )
	{
		throw std::runtime_error( message + ": " + std::string( Simplygon::GetError( ec ) ) );
	}
}

/** Helper class to manage Simplygon initialization and deinitialization. */
class SimplygonLibrary : public Simplygon::ErrorHandler
{
public:
	static Simplygon::ISimplygon* Get()
	{
		static SimplygonLibrary instance;
		return instance.sg;
	}

private:
	Simplygon::ISimplygon* sg = nullptr;
	SimplygonLibrary()
	{
		auto ec = Simplygon::Initialize( &sg );
		AssertSimplygonError( ec, "Failed to initialize Simplygon" );
		sg->SetErrorHandler( this );
	}
	~SimplygonLibrary()
	{
		if( sg )
		{
			Simplygon::Deinitialize( sg );
			sg = nullptr;
		}
	}

	void HandleError( Simplygon::spObject /*object*/, const char* /*interfaceName*/, const char* /*methodName*/, Simplygon::rid /*errorType*/, const char* errorText ) override
	{
		throw std::runtime_error( std::string( "Simplygon error: " ) + errorText );
	}
};

/** Enumerates elements of a container: calls the provided operation with the index and element.
* This is useful for filling Simplygon data structures, which require indices. */
template <typename T, typename O>
void Enumerate( T&& container, O&& operation )
{
	size_t index = 0;
	for( auto&& element : container )
	{
		operation( index++, element );
	}
}

bool IsFloatElementType( cmf::ElementType type )
{
	switch( type )
	{
	case cmf::ElementType::Float32:
	case cmf::ElementType::Float16:
	case cmf::ElementType::UInt16Norm:
	case cmf::ElementType::Int16Norm:
	case cmf::ElementType::UInt8Norm:
	case cmf::ElementType::Int8Norm:
		return true;
	default:
		return false;
	}
}

/** Exports a per polygon corner vertex element to a Simplygon array. */
template <typename T, typename S>
void ExportCornerField( S simplygonArray, const cmf::VertexElement& element, const cmf::BufferView& vb, const cmf::BufferView& ib, cmf::BufferManager& bufferAllocator )
{
	const auto indexStream = cmf::ConstIndexBufferStream( ib, bufferAllocator );
	auto source = cmf::ConstBufferElementStream<T>( element, vb, bufferAllocator );
	Enumerate( indexStream, [&]( size_t idx, uint32_t vtx ) {
		auto src = source[vtx];
		simplygonArray->SetTuple( Simplygon::rid( idx ), &src.x );
	} );
}

/** Exports a per vertex element to a Simplygon array. */
template <typename T, typename S>
void ExportVertexField( S simplygonArray, const cmf::VertexElement& element, const cmf::BufferView& vb, cmf::BufferManager& bufferAllocator )
{
	auto source = cmf::ConstBufferElementStream<T>( element, vb, bufferAllocator );
	Enumerate( source, [&]( size_t idx, const auto& src ) {
		simplygonArray->SetTuple( Simplygon::rid( idx ), &src.x );
	} );
}

std::string GetCustomElementName( const std::string& prefix, const cmf::VertexElement& element )
{
	return prefix + "_" + std::to_string( static_cast<int>( element.usage ) ) + "_" + std::to_string( element.usageIndex );
}

/** Exports a vertex element as a user field into Simplygon geometry data. User fields are used for vertex elements that don't have a corresponding Simplygon field. */
void ExportUserField( Simplygon::spGeometryData& geomData, const cmf::VertexElement& element, const std::string& namePrefix, const cmf::BufferView& vb, const cmf::BufferView& ib, cmf::BufferManager& bufferAllocator )
{
	const std::string name = GetCustomElementName( namePrefix, element );
	const auto indexStream = cmf::ConstIndexBufferStream( ib, bufferAllocator );
	if( IsFloatElementType( element.type ) )
	{
		Simplygon::spRealArray sgArray = SimplygonLibrary::Get()->CreateRealArray();
		sgArray->SetTupleSize( element.elementCount );
		sgArray->SetTupleCount( indexStream.size() );
		ExportCornerField<Vector4>( sgArray, element, vb, ib, bufferAllocator );
		sgArray->SetName( name.c_str() );
		geomData->AddUserCornerField( sgArray );
	}
	else
	{
		Simplygon::spIntArray sgArray = SimplygonLibrary::Get()->CreateIntArray();
		sgArray->SetTupleSize( element.elementCount );
		sgArray->SetTupleCount( indexStream.size() );
		Enumerate( indexStream, [&, elStream = cmf::ConstBufferElementStream<std::array<int, 4>>( element, vb, bufferAllocator )]( size_t idx, uint32_t vtx ) {
			auto x = elStream[vtx];
			sgArray->SetTuple( Simplygon::rid( idx ), &x[0] );
		} );
		sgArray->SetName( name.c_str() );
		geomData->AddUserCornerField( sgArray );
	}
}

/** Exports morph targets into a Simplygon geometry data */
void ExportMorphTargets( Simplygon::spGeometryData& geomData, const cmf::Mesh& mesh, const cmf::MeshLod& srcLod, cmf::BufferManager& bufferAllocator )
{
	if( !mesh.morphTargets.targets.empty() )
	{
		AssertSimplygonError( geomData->AddBlendshape( "TheBlendShape" ), "Error adding blendshape for simplygon" );
		for( size_t targetIndex = 0; targetIndex < mesh.morphTargets.targets.size(); ++targetIndex )
		{
			const Simplygon::rid level = Simplygon::rid( targetIndex );
			const auto& target = mesh.morphTargets.targets[targetIndex];
			geomData->AddBlendshapeChannel( level );
			geomData->AddBlendshapeShape( level );

			for( auto& element : mesh.morphTargets.decl )
			{
				switch( element.usage )
				{
				case cmf::Usage::Position:
					if( element.usageIndex == 0 )
					{
						ExportVertexField<Vector3>( geomData->GetBlendshapeShapeCoords( level ), element, srcLod.morphTargets[targetIndex].vb, bufferAllocator );
						continue;
					}
					break;
				case cmf::Usage::Normal:
					if( element.usageIndex == 0 )
					{
						ExportCornerField<Vector3>( geomData->GetBlendshapeShapeNormals( level ), element, srcLod.morphTargets[targetIndex].vb, srcLod.ib, bufferAllocator );
						continue;
					}
					break;
				default:
					break;
				}

				ExportUserField( geomData, element, "CmfMorphAttr_" + std::to_string( targetIndex ), srcLod.morphTargets[targetIndex].vb, srcLod.ib, bufferAllocator );
			}
		}
	}
}


/** Exports a mesh into Simplygon geometry data */
void ExportMeshToSimplygon( Simplygon::spGeometryData& geomData, const cmf::Mesh& mesh, const SimplygonLodOptions& options, cmf::BufferManager& bufferAllocator )
{
	const auto& srcLod = mesh.lods[mesh.lods.size() - 1];

	geomData->SetVertexCount( srcLod.vb.size / srcLod.vb.stride );

	geomData->SetTriangleCount( srcLod.ib.size / srcLod.ib.stride / 3 );
	Enumerate( cmf::ConstIndexBufferStream( srcLod.ib, bufferAllocator ), [dataIndices = geomData->GetVertexIds()]( size_t idx, uint32_t vtx ) {
		dataIndices->SetItem( Simplygon::rid( idx ), vtx );
	} );

	std::vector<uint8_t> addedTangents;

	for( auto& element : mesh.decl )
	{
		switch( element.usage )
		{
		case cmf::Usage::Position:
			if( element.usageIndex == 0 )
			{
				Enumerate(
					cmf::ConstBufferElementStream<Vector3>( element, srcLod.vb, bufferAllocator ),
					[dataPositions = geomData->GetCoords()]( size_t idx, Vector3 pos ) {
						dataPositions->SetTuple( Simplygon::rid( idx ), &pos.x );
					} );
				continue;
			}
			break;
		case cmf::Usage::Normal:
			if( element.usageIndex == 0 )
			{
				geomData->AddNormals();
				ExportCornerField<Vector3>( geomData->GetNormals(), element, srcLod.vb, srcLod.ib, bufferAllocator );
				continue;
			}
			break;
		case cmf::Usage::Color:
			if( element.usageIndex == LOCKED_VERTEX_USAGE_INDEX )
			{
				geomData->AddVertexLocks();
				auto source = cmf::ConstBufferElementStream<Vector4>( element, srcLod.vb, bufferAllocator );
				Enumerate( source, [&]( size_t idx, const Vector4& color ) {
					geomData->GetVertexLocks()->SetItem( Simplygon::rid( idx ), color.x > 0 );
				} );
			}
			geomData->AddColors( element.usageIndex );
			ExportCornerField<Vector4>( geomData->GetColors( element.usageIndex ), element, srcLod.vb, srcLod.ib, bufferAllocator );
			continue;
		case cmf::Usage::Tangent:
			if( find( addedTangents.begin(), addedTangents.end(), element.usageIndex ) == addedTangents.end() )
			{
				geomData->AddTangents( element.usageIndex );
				addedTangents.push_back( element.usageIndex );
			}
			ExportCornerField<Vector3>( geomData->GetTangents( element.usageIndex ), element, srcLod.vb, srcLod.ib, bufferAllocator );
			continue;
		case cmf::Usage::Binormal:
			if( find( addedTangents.begin(), addedTangents.end(), element.usageIndex ) == addedTangents.end() )
			{
				geomData->AddTangents( element.usageIndex );
				addedTangents.push_back( element.usageIndex );
			}
			ExportCornerField<Vector3>( geomData->GetBitangents( element.usageIndex ), element, srcLod.vb, srcLod.ib, bufferAllocator );
			continue;
		case cmf::Usage::TexCoord:
			geomData->AddTexCoords( element.usageIndex );
			ExportCornerField<Vector3>( geomData->GetTexCoords( element.usageIndex ), element, srcLod.vb, srcLod.ib, bufferAllocator );
			continue;
		case cmf::Usage::BoneIndices: {
			const auto boneWeights = FindElement( mesh.decl, cmf::Usage::BoneWeights );
			const auto bonesPerVertex = boneWeights ? 4u : 1u;
			geomData->AddBoneWeights( bonesPerVertex );

			auto indices = geomData->GetBoneIds();
			auto weights = geomData->GetBoneWeights();

			Enumerate( cmf::ConstBufferElementStream<std::array<uint32_t, 4>>( element, srcLod.vb, bufferAllocator ), [&]( size_t idx, std::array<uint32_t, 4> uv ) {
				for( uint32_t b = 0; b < bonesPerVertex; ++b )
				{
					indices->SetItem( Simplygon::rid( idx * bonesPerVertex + b ), Simplygon::rid( uv[b] ) );
				}
			} );

			if( bonesPerVertex == 4 )
			{
				Enumerate( cmf::ConstBufferElementStream<Vector4>( *boneWeights, srcLod.vb, bufferAllocator ), [&]( size_t idx, Vector4 uv ) {
					for( uint32_t b = 0; b < 4; ++b )
					{
						weights->SetItem( Simplygon::rid( idx * 4 + b ), uv[b] );
					}
				} );
			}
			else
			{
				for( uint32_t i = 0; i < srcLod.vb.size / srcLod.vb.stride; ++i )
				{
					weights->SetItem( Simplygon::rid( i ), 1.f );
				}
			}
			continue;
		}
		case cmf::Usage::BoneWeights:
			continue;
		default:
			break;
		}

		ExportUserField( geomData, element, "CmfAttr", srcLod.vb, srcLod.ib, bufferAllocator );
	}

	geomData->AddGroupIds();
	auto dataGroupID = geomData->GetGroupIds();
	for( uint32_t g = 0; g < mesh.areas.size(); ++g )
	{
		auto first = srcLod.areas[g].firstElement;
		for( uint32_t t = 0; t < srcLod.areas[g].elementCount; ++t )
		{
			dataGroupID->SetItem( Simplygon::rid( t + first ), Simplygon::rid( g ) );
		}
	}

	ExportMorphTargets( geomData, mesh, srcLod, bufferAllocator );
}

/** Imports Simplygon field into a vertex buffer */
template <typename T, typename S>
void PopulateVertexBuffer( const S& sgArray, const cmf::VertexElement& element, const cmf::BufferView& vb, cmf::BufferManager& bufferAllocator )
{
	auto tupleSize = sgArray->GetTupleSize();
	auto destStream = cmf::BufferElementStream<T>( element, vb, bufferAllocator );
	for( uint32_t v = 0; v < destStream.size(); ++v )
	{
		auto src = sgArray->GetTuple( v );
		T dst = {};
		for( uint32_t c = 0; c < tupleSize; ++c )
		{
			dst[c] = src[c];
		}
		destStream.set( v, dst );
	}
}

/** Imports index buffer and area information from Simplygon geometry data. Simplygon may reorder triangles, so we need to keep track of the group ids to reconstruct the areas correctly. */
void ImportIndexBuffer( cmf::MeshLod& newLod, const Simplygon::spPackedGeometryData& packed, uint32_t areaCount, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	const unsigned int newIndexCount = packed->GetVertexIds()->GetTupleCount();
	std::vector<std::vector<uint32_t>> groupIndices;
	groupIndices.resize( areaCount );

	auto vertexIds = packed->GetVertexIds();
	auto groupIds = packed->GetGroupIds();

	for( unsigned int t = 0; t < newIndexCount / 3; ++t )
	{
		Simplygon::spRidData data = groupIds->GetTuple( t );
		uint32_t groupID = data.GetItem( 0 );
		groupIndices[groupID].push_back( uint32_t( vertexIds->GetItem( t * 3 + 0 ) ) );
		groupIndices[groupID].push_back( uint32_t( vertexIds->GetItem( t * 3 + 1 ) ) );
		groupIndices[groupID].push_back( uint32_t( vertexIds->GetItem( t * 3 + 2 ) ) );
	}

	// create areas
	uint32_t cntr = 0;
	for( auto& indices : groupIndices )
	{
		cmf::LodMeshArea area;
		area.firstElement = cntr;
		area.elementCount = uint32_t( indices.size() / 3 );
		cmf::Modify( newLod.areas, allocator ).push_back( area );
		cntr += area.elementCount;
	}

	// build index buffer to keep group order intact
	newLod.ib = bufferAllocator.AllocateBuffer( nullptr, newIndexCount * sizeof( uint32_t ), sizeof( uint32_t ) );
	auto newIndices = static_cast<uint32_t*>( bufferAllocator.GetData( newLod.ib ) );
	cntr = 0;
	for( auto& indices : groupIndices )
	{
		std::copy( indices.begin(), indices.end(), newIndices + cntr );
		cntr += uint32_t( indices.size() );
	}
}

cmf::MeshLod ImportMeshLodFromSimplygon( const Simplygon::spPackedGeometryData& packed, const cmf::Mesh& mesh, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	cmf::MeshLod newLod;

	uint32_t newVertexCount = packed->GetCoords()->GetTupleCount();
	newLod.vb = bufferAllocator.AllocateBuffer( nullptr, uint32_t( newVertexCount * mesh.lods[0].vb.stride ), mesh.lods[0].vb.stride );
	uint8_t* vbData = static_cast<uint8_t*>( bufferAllocator.GetData( newLod.vb ) );

	for( auto& element : mesh.decl )
	{
		switch( element.usage )
		{
		case cmf::Usage::Position:
			if( element.usageIndex == 0 )
			{
				PopulateVertexBuffer<Vector3>( packed->GetCoords(), element, newLod.vb, bufferAllocator );
			}
			continue;
		case cmf::Usage::Normal:
			if( element.usageIndex == 0 )
			{
				PopulateVertexBuffer<Vector3>( packed->GetNormals(), element, newLod.vb, bufferAllocator );
			}
			continue;
		case cmf::Usage::Color:
			PopulateVertexBuffer<Vector4>( packed->GetColors( element.usageIndex ), element, newLod.vb, bufferAllocator );
			continue;
		case cmf::Usage::Tangent:
			PopulateVertexBuffer<Vector3>( packed->GetTangents( element.usageIndex ), element, newLod.vb, bufferAllocator );
			continue;
		case cmf::Usage::Binormal:
			PopulateVertexBuffer<Vector3>( packed->GetBitangents( element.usageIndex ), element, newLod.vb, bufferAllocator );
			continue;
		case cmf::Usage::TexCoord:
			PopulateVertexBuffer<Vector2>( packed->GetTexCoords( element.usageIndex ), element, newLod.vb, bufferAllocator );
			continue;
		case cmf::Usage::BoneIndices:
			PopulateVertexBuffer<std::array<uint32_t, 4>>( packed->GetBoneIds(), element, newLod.vb, bufferAllocator );
			continue;
		case cmf::Usage::BoneWeights:
			PopulateVertexBuffer<Vector4>( packed->GetBoneWeights(), element, newLod.vb, bufferAllocator );
			continue;
		default:
			break;
		}
		std::string name = GetCustomElementName( "CmfAttr", element );
		auto field = packed->GetUserVertexField( name.c_str() );
		if( IsFloatElementType( element.type ) )
		{
			PopulateVertexBuffer<Vector4>( Simplygon::spRealArray::SafeCast( field ), element, newLod.vb, bufferAllocator );
		}
		else
		{
			PopulateVertexBuffer<std::array<int, 4>>( Simplygon::spIntArray::SafeCast( field ), element, newLod.vb, bufferAllocator );
		}
	}
	for( size_t targetIndex = 0; targetIndex < mesh.morphTargets.targets.size(); ++targetIndex )
	{
		cmf::LodMorphTarget newMorphTarget;
		newMorphTarget.vb = bufferAllocator.AllocateBuffer( nullptr, uint32_t( newVertexCount * mesh.lods[0].morphTargets[0].vb.stride ), mesh.lods[0].morphTargets[0].vb.stride );

		for( auto& element : mesh.morphTargets.decl )
		{
			switch( element.usage )
			{
			case cmf::Usage::Position:
				if( element.usageIndex == 0 )
				{
					auto sgPositions = packed->GetUserVertexField( ( "BlendshapeShapeCoords" + std::to_string( targetIndex ) ).c_str() );
					PopulateVertexBuffer<Vector3>( Simplygon::spRealArray::SafeCast( sgPositions ), element, newMorphTarget.vb, bufferAllocator );
					continue;
				}
			case cmf::Usage::Normal:
				if( element.usageIndex == 0 )
				{
					auto sgNormals = packed->GetUserVertexField( ( "BlendshapeShapeNormals" + std::to_string( targetIndex ) ).c_str() );
					PopulateVertexBuffer<Vector3>( Simplygon::spRealArray::SafeCast( sgNormals ), element, newMorphTarget.vb, bufferAllocator );
					continue;
				}
			default:
				break;
			}
			std::string name = GetCustomElementName( "CmfMorphAttr_" + std::to_string( targetIndex ), element );
			auto field = packed->GetUserVertexField( name.c_str() );
			if( IsFloatElementType( element.type ) )
			{
				PopulateVertexBuffer<Vector4>( Simplygon::spRealArray::SafeCast( field ), element, newMorphTarget.vb, bufferAllocator );
			}
			else
			{
				PopulateVertexBuffer<std::array<int, 4>>( Simplygon::spIntArray::SafeCast( field ), element, newMorphTarget.vb, bufferAllocator );
			}
		}
		cmf::Modify( newLod.morphTargets, allocator ).push_back( newMorphTarget );
	}

	ImportIndexBuffer( newLod, packed, (uint32_t)mesh.areas.size(), allocator, bufferAllocator );

	return newLod;
}


std::pair<cmf::MeshLod, uint32_t> GenerateLod( const cmf::Mesh& mesh, uint32_t screenSize, const SimplygonLodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	auto sg = SimplygonLibrary::Get();
	Simplygon::spGeometryData geomData = sg->CreateGeometryData();
	ExportMeshToSimplygon( geomData, mesh, options, bufferAllocator );

	Simplygon::spScene scene = sg->CreateScene();
	Simplygon::spSceneMesh sceneMesh = sg->CreateSceneMesh();
	sceneMesh->SetGeometry( geomData );
	scene->GetRootNode()->AddChild( sceneMesh );

	Simplygon::spReductionProcessor processor = sg->CreateReductionProcessor();
	processor->SetScene( scene );

	Simplygon::spRepairSettings repairSettings = processor->GetRepairSettings();
	repairSettings->SetUseWelding( true );
	repairSettings->SetUseTJunctionRemover( true );
	repairSettings->SetTJuncDist( T_JUNCTION_THRESHOLD );
	repairSettings->SetWeldDist( VERTEX_WELDING_THRESHOLD );
	repairSettings->SetProgressivePasses( 2 );

	Simplygon::spReductionSettings reductionSettings = processor->GetReductionSettings();
	reductionSettings->SetMaterialImportance( 0.f );
	reductionSettings->SetGroupImportance( mesh.areas.size() > 1 ? options.areaImportance : 0.f );
	reductionSettings->SetShadingImportance( options.normalImportance );
	reductionSettings->SetTextureImportance( options.uvImportance );
	reductionSettings->SetEdgeSetImportance( 0.f );
	reductionSettings->SetGeometryImportance( options.geometryImportance );
	reductionSettings->SetSkinningImportance( FindElement( mesh.decl, cmf::Usage::BoneWeights ) ? options.skinningImportance : 0.f );
	reductionSettings->SetVertexColorImportance( options.vertexColorImportance );
	reductionSettings->SetDataCreationPreferences( Simplygon::EDataCreationPreferences::PreferOriginalData );
	reductionSettings->SetReductionHeuristics( Simplygon::EReductionHeuristics::Fast );
	reductionSettings->SetReductionTargets( Simplygon::EStopCondition::All, true, false, false, true );
	reductionSettings->SetReductionTargetTriangleRatio( TARGET_RATIO );
	reductionSettings->SetReductionTargetOnScreenSize( screenSize );

	reductionSettings->SetAllowDegenerateTexCoords( false );
	reductionSettings->SetUseHighQualityNormalCalculation( true );

	Simplygon::spNormalCalculationSettings normalSettings = processor->GetNormalCalculationSettings();
	normalSettings->SetReplaceNormals( false );

	Simplygon::spMappingImageSettings mappingSettings = processor->GetMappingImageSettings();
	mappingSettings->SetGenerateMappingImage( false );

	Simplygon::spVisibilitySettings visibilitySettings = processor->GetVisibilitySettings();
	visibilitySettings->SetUseVisibilityWeightsInReducer( false );
	visibilitySettings->SetUseVisibilityWeightsInTexcoordGenerator( false );

	Simplygon::spBoneSettings boneSettings = processor->GetBoneSettings();
	boneSettings->SetUseBoneReducer( false );

	AssertSimplygonError( processor->RunProcessing(), "Failed to run Simplygon processing" );

	if( geomData->GetVertexCount() == 0 || geomData->GetTriangleCount() == 0 )
	{
		// Simplygon reduced everything away
		return { cmf::MeshLod(), 0 };
	}

	cmf::MeshLod newLod = ImportMeshLodFromSimplygon( geomData->NewPackedCopy(), mesh, allocator, bufferAllocator );

	auto maxDeviation = scene->GetCustomFieldMaxDeviation()->GetItem( 0 );
	auto diameter = scene->GetCustomFieldProcessedMeshesExtents()->GetBoundingSphereRadius() * 2.0f;

	auto newScreenSize = std::min( uint32_t( diameter / maxDeviation ), screenSize );
	newLod.threshold = uint32_t( newScreenSize * options.screenSizeFactor );


	return { newLod, newScreenSize };
}

void ValidateVertexDeclaration( const cmf::Mesh& mesh )
{
	if( !FindElement( mesh.decl, cmf::Usage::Position ) )
	{
		throw std::runtime_error( "Mesh " + ToStdString( mesh.name ) + " doesn't have a position element, which is required for LOD generation." );
	}
	for( auto& element : mesh.decl )
	{
		switch( element.usage )
		{
		case cmf::Usage::Position:
			if( element.usageIndex == 0 && element.elementCount != 3 )
			{
				throw std::runtime_error( "Unsupported vertex position format in mesh " + ToStdString( mesh.name ) );
			}
			break;
		case cmf::Usage::Normal:
			if( element.usageIndex == 0 && element.elementCount != 3 )
			{
				throw std::runtime_error( "Unsupported vertex normal format in mesh " + ToStdString( mesh.name ) );
			}
			break;
		case cmf::Usage::BoneIndices:
			if( element.usageIndex != 0 )
			{
				throw std::runtime_error( "Unsupported blend indices usage in mesh " + ToStdString( mesh.name ) );
			}
			break;
		case cmf::Usage::BoneWeights:
			if( element.usageIndex != 0 )
			{
				throw std::runtime_error( "Unsupported blend weights usage in mesh " + ToStdString( mesh.name ) );
			}
			break;
		case cmf::Usage::PackedTangent:
		case cmf::Usage::PackedTangentLegacy:
			throw std::runtime_error( "Packed tangents are not supported in LOD generation; issue in mesh " + ToStdString( mesh.name ) );
		default:
			break;
		}
	}
	for( auto& element : mesh.morphTargets.decl )
	{
		switch( element.usage )
		{
		case cmf::Usage::Position:
			if( element.usageIndex == 0 && element.elementCount != 3 )
			{
				throw std::runtime_error( "Unsupported vertex position format in morph target of mesh " + ToStdString( mesh.name ) );
			}
			break;
		case cmf::Usage::Normal:
			if( element.usageIndex == 0 && element.elementCount != 3 )
			{
				throw std::runtime_error( "Unsupported vertex normal format in morph target of mesh " + ToStdString( mesh.name ) );
			}
			break;
		default:
			break;
		}
	}
}

const cmf::MeshLod& FindSourceLodForAudioOcclusion( const cmf::Mesh& mesh, uint32_t screenSize )
{
	for( size_t i = 0; i < mesh.lods.size(); ++i )
	{
		if( i + 1 >= mesh.lods.size() || mesh.lods[i + 1].threshold < screenSize )
		{
			return mesh.lods[i];
		}
	}
	throw std::runtime_error( "Mesh " + ToStdString( mesh.name ) + " does not have any LODs." );
}
}

void SimplygonGenerateLods( cmf::Mesh& mesh, const SimplygonLodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	ValidateVertexDeclaration( mesh );

	uint32_t screenSize = uint32_t( MAX_SCREEN_SIZE / options.screenSizeFactor );
	while( screenSize >= MIN_SCREEN_SIZE )
	{
		auto [lod, newScreenSize] = GenerateLod( mesh, screenSize, options, allocator, bufferAllocator );
		if( lod.ib.size == 0 || lod.vb.size == 0 )
		{
			// Simplygon reduced everything away, no point in generating more LODs.
			break;
		}
		cmf::Modify( mesh.lods, allocator ).push_back( lod );
		if( newScreenSize <= MIN_SCREEN_SIZE )
		{
			break;
		}
		screenSize = std::max( MIN_SCREEN_SIZE, newScreenSize / 2 );
	}

	// Remove any extra LODs if we exceeded the max LOD count. We remove the one with the smallest delta to preserve more aggressive LODs.
	while( mesh.lods.size() > options.maxLods )
	{
		float minDelta = std::numeric_limits<float>::max();
		uint32_t candidateIndex = 0;
		for( uint32_t i = 1; i + 1 < mesh.lods.size(); ++i )
		{
			auto prevIndexCount = mesh.lods[i - 1].ib.size / mesh.lods[i - 1].ib.stride;
			auto thisIndexCount = mesh.lods[i].ib.size / mesh.lods[i].ib.stride;
			auto delta = float( prevIndexCount - thisIndexCount ) / float( prevIndexCount );
			if( delta < minDelta )
			{
				minDelta = delta;
				candidateIndex = i;
			}
		}
		cmf::Modify( mesh.lods, allocator ).erase( mesh.lods.begin() + candidateIndex );
	}
}

void SimplygonGenerateAudioOcclusionMesh( cmf::Mesh& mesh, const SimplygonAudioOcclusionMeshOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	auto* positionElement = FindElement( mesh.decl, cmf::Usage::Position );
	if( !positionElement )
	{
		throw std::runtime_error( "Mesh " + ToStdString( mesh.name ) + " doesn't have a position element, which is required for audio occlusion mesh generation." );
	}

	const auto* sg = SimplygonLibrary::Get();
	const Simplygon::spGeometryData geomData = sg->CreateGeometryData();
	const auto& srcLod = FindSourceLodForAudioOcclusion( mesh, options.screenSize );

	geomData->SetVertexCount( srcLod.vb.size / srcLod.vb.stride );

	geomData->SetTriangleCount( srcLod.ib.size / srcLod.ib.stride / 3 );
	Enumerate( cmf::ConstIndexBufferStream( srcLod.ib, bufferAllocator ), [dataIndices = geomData->GetVertexIds()]( size_t idx, uint32_t vtx ) {
		dataIndices->SetItem( Simplygon::rid( idx ), Simplygon::rid( vtx ) );
	} );
	Enumerate(
		cmf::ConstBufferElementStream<Vector3>( *positionElement, srcLod.vb, bufferAllocator ),
		[dataPositions = geomData->GetCoords()]( size_t idx, Vector3 pos ) {
			dataPositions->SetTuple( Simplygon::rid( idx ), &pos.x );
		} );

	const Simplygon::spScene scene = sg->CreateScene();
	const Simplygon::spSceneMesh sceneMesh = sg->CreateSceneMesh();
	sceneMesh->SetGeometry( geomData );
	scene->GetRootNode()->AddChild( sceneMesh );

	const Simplygon::spRemeshingProcessor processor = sg->CreateRemeshingProcessor();
	processor->SetScene( scene );

	const Simplygon::spRemeshingSettings rs = processor->GetRemeshingSettings();

	rs->SetRemeshingTarget( Simplygon::ERemeshingTarget::OnScreenSize );
	rs->SetOnScreenSize( options.screenSize );
	rs->SetRemeshingMode( Simplygon::ERemeshingMode::Outside );

	Simplygon::EHoleFilling sgHf = Simplygon::EHoleFilling::High;
	switch( options.holeFilling )
	{
	case SimplygonHoleFilling::Disabled:
		sgHf = Simplygon::EHoleFilling::Disabled;
		break;
	case SimplygonHoleFilling::Low:
		sgHf = Simplygon::EHoleFilling::Low;
		break;
	case SimplygonHoleFilling::Medium:
		sgHf = Simplygon::EHoleFilling::Medium;
		break;
	default:
		sgHf = Simplygon::EHoleFilling::High;
		break;
	}
	rs->SetHoleFilling( sgHf );

	AssertSimplygonError( processor->RunProcessing(), "Failed to run Simplygon processing" );

	// Simplygon does not do re-meshing in-place. We need to find the mesh in the scene again after processing to retrieve the re-meshed geometry.
	Simplygon::spSceneMesh resultMesh = Simplygon::spSceneMesh::SafeCast( scene->GetRootNode()->GetChild( 0 ) );
	if( resultMesh.IsNull() )
	{
		const Simplygon::spSceneNode child = scene->GetRootNode()->GetChild( 0 );
		if( child.NonNull() )
		{
			resultMesh = Simplygon::spSceneMesh::SafeCast( child->GetChild( 0 ) );
		}
	}
	if( resultMesh.IsNull() )
	{
		throw std::runtime_error( "Failed to retrieve re-meshed geometry from Simplygon scene." );
	}

	auto packed = resultMesh->GetGeometry()->NewPackedCopy();
	const uint32_t newVertexCount = packed->GetCoords()->GetTupleCount();
	mesh.audioOcclusionMesh.vertices = allocator.AllocateSpan<Vector3>( newVertexCount );
	for( uint32_t v = 0; v < newVertexCount; ++v )
	{
		auto src = packed->GetCoords()->GetTuple( Simplygon::rid( v ) );
		mesh.audioOcclusionMesh.vertices[v] = Vector3( src.GetItem( 0 ), src.GetItem( 1 ), src.GetItem( 2 ) );
	}
	const uint32_t newIndexCount = packed->GetVertexIds()->GetTupleCount();
	if( newIndexCount > std::numeric_limits<uint16_t>::max() )
	{
		throw std::runtime_error( "Simplygon generated an audio occlusion mesh with more vertices than supported by 16-bit indices." );
	}
	mesh.audioOcclusionMesh.indices = allocator.AllocateSpan<uint16_t>( newIndexCount );
	for( uint32_t i = 0; i < newIndexCount; ++i )
	{
		mesh.audioOcclusionMesh.indices[i] = static_cast<uint16_t>( packed->GetVertexIds()->GetItem( Simplygon::rid( i ) ) );
	}
	mesh.audioOcclusionMesh.bounds = {};
	for( const auto& vertex : mesh.audioOcclusionMesh.vertices )
	{
		mesh.audioOcclusionMesh.bounds.Include( vertex );
	}
}

#else
void SimplygonGenerateLods( cmf::Mesh& mesh, const SimplygonLodOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	throw std::runtime_error( "Simplygon support is disabled in this build." );
}

void SimplygonGenerateAudioOcclusionMesh( cmf::Mesh& mesh, const SimplygonAudioOcclusionMeshOptions& options, cmf::MemoryAllocator& allocator, cmf::BufferManager& bufferAllocator )
{
	throw std::runtime_error( "Simplygon support is disabled in this build." );
}
#endif