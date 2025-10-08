#pragma once
#include <cmf/cmf.h>

class CmfData
{
public:
	CmfData();
	CmfData( std::vector<uint8_t> fileContent, std::string filePath );
	~CmfData();

    CcpMath::Sphere GetBoundingSphere() const;  

	cmf::Header* m_cmfHeader;
	cmf::Data* m_cmfData;

	std::vector<uint8_t> m_fileContent;

	std::string m_filePath;
};

namespace CmfDataLoader
{
CmfData* LoadDataFromFile( const std::string& filePath );
};
