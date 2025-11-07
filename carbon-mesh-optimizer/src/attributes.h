#pragma once

#include <cmf/cmf.h>


using namespace cmf;
using namespace cmf::v1;

const char* getUsageString( Usage usage );

const char* getElementTypeString( ElementType type );

int getElementTypeByteSize( ElementType type );


float readValue( ElementType type, const uint8_t* data );
Vector4 readAttribute( const VertexElement& attribute, const uint8_t* vertexData );


void writeValue( ElementType type, uint8_t* data, float value );

void writeAttribute( const VertexElement& attribute, uint8_t* vertexData, Vector4 data );