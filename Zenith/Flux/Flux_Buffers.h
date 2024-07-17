#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};

class Flux_IndexBuffer
{
public:
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};

class Flux_ConstantBuffer
{
public:
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};