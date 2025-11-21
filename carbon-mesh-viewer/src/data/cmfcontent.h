#pragma once
#include <cmf/cmf.h>


// Simple owner class for the cmf data
// only holds on to the content and provides access to the header and data and some utility functions
class CmfContent
{
public:
	CmfContent();
	CmfContent( std::vector<uint8_t> fileContent, std::string filePath );
	~CmfContent();

	CcpMath::Sphere GetBoundingSphere() const;

	cmf::Header* m_cmfHeader{ nullptr };
	cmf::Data* m_cmfData{ nullptr };

	std::vector<uint8_t> m_fileContent;

	std::string m_filePath{ "" };
};

namespace CmfContentLoader
{
CmfContent* LoadContentFromFile( const std::string& filePath );
};
