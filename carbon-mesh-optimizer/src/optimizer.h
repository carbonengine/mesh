#pragma once

#include <vector>
#include <cmf/cmf.h>
#include <data/cmfcontent.h>

namespace cmf
{
namespace optimizer
{

struct OptBufferData
{
	std::vector<uint8_t> data;
	uint32_t stride = 1;

	uint32_t length() const
	{
		return (uint32_t)(data.size() / stride);
	}

    OptBufferData& operator=( OptBufferData other )
	{
        data.assign( other.data.begin(), other.data.end() );
		stride = other.stride;

		return *this;
	}
};

struct OptMeshAreaLod
{
	uint32_t firstElement = 0;
	uint32_t elementCount = 0;
};

struct OptMorphTarget
{
	//std::string name;
	std::vector<cmf::v1::VertexElement> vertexDeclaration;
	//CcpMath::AxisAlignedBox bounds = {};
	//std::vector<Vector4> maxDisplacements;
};

struct OptMorphTargetLod
{
	OptBufferData vertexBuffer;
};

struct OptMeshLod
{
	OptBufferData vertexBuffer;
	OptBufferData indexBuffer;
	std::vector<OptMeshAreaLod> areas;
	std::vector<OptMorphTargetLod> morphTargetLods;
	uint32_t threshold; //TODO: should be changed to float
};

struct OptMesh
{
	std::string name;
	std::vector<VertexElement> vertexDeclaration;

	std::vector<OptMeshLod> lods;
	std::vector<OptMorphTarget> morphTargets;
};

class Optimizer
{
public:
	Optimizer( CmfContent* content );
	~Optimizer();

	void generateTangents( uint32_t usageIndex, bool force );
	void compressTangents( uint32_t usageIndex, bool retainNormal );
	void decompressTangents( uint32_t usageIndex );
	void optimizeVertexPerformance();

	std::vector<uint8_t> toCmf( bool compress );

private:
	
    CmfContent* content;

    std::vector<OptMesh> meshes;
};


}
}