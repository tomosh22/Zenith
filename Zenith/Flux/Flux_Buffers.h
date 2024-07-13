#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	void Initialise(const void* pData, uint64_t ulSize);
private:
	Flux_Buffer m_xBuffer;
};

class Flux_IndexBuffer
{
public:
	void Initialise(const void* pData, uint64_t ulSize);
private:
	Flux_Buffer m_xBuffer;
};