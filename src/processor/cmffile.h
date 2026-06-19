// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/utils.h"
#include "cmf/memallocator.h"


struct CmfFile
{
	explicit CmfFile( std::string_view path );
	void Save( std::string_view path );

	cmf::Data& GetData();
	cmf::Metadata& GetMetadata();
	cmf::Header& GetHeader();

	cmf::MemoryAllocator& GetAllocator();
	cmf::BufferManager& GetBufferManager();

	static void SetModificationRecord( std::string_view record );

private:
	void InjectModificationRecord();

	cmf::MemoryAllocator m_allocator;
	cmf::BufferManager m_bufferManager{ m_allocator };
	cmf::Data* m_data = nullptr;
	cmf::Metadata* m_metadata = nullptr;
	std::vector<uint8_t> m_fileContents;
	static std::string m_modificationRecord;
};


std::vector<uint8_t> ReadFile( const char* path );
void WriteFile( const char* path, const std::vector<uint8_t>& data );