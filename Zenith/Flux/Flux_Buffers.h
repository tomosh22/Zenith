#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};

class Flux_IndexBuffer
{
public:
	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};

class Flux_ConstantBuffer
{
public:
	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};