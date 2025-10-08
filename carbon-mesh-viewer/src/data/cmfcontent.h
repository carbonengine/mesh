#pragma once
#include <cmf/cmf.h>

class CmfContent
{
public:
	CmfContent();
	CmfContent( std::vector<uint8_t> fileContent, std::string filePath );
	~CmfContent();

	CcpMath::Sphere GetBoundingSphere() const;

	cmf::Header* m_cmfHeader;
	cmf::Data* m_cmfData;

	std::vector<uint8_t> m_fileContent;

	std::string m_filePath;
};

namespace CmfContentLoader
{
CmfContent* LoadContentFromFile( const std::string& filePath );
};
