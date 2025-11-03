#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	void Reset()
	{
		m_xBufferVRAM = Flux_BufferVRAM();
	}

	const Flux_BufferVRAM& GetBufferVRAM() const { return m_xBufferVRAM; }
	Flux_BufferVRAM& GetBufferVRAM() { return m_xBufferVRAM; }
private:
	Flux_BufferVRAM m_xBufferVRAM;
};

class Flux_DynamicVertexBuffer
{
public:
	void Reset()
	{
		for (Flux_BufferVRAM& xBuffer : m_axBufferVRAMs)
		{
			xBuffer = Flux_BufferVRAM();
		}
	}
	const Flux_BufferVRAM& GetBufferVRAM() const { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_BufferVRAM& GetBufferVRAM() { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_BufferVRAM& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) const { return m_axBufferVRAMs[uFrame]; }
	Flux_BufferVRAM& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) { return m_axBufferVRAMs[uFrame]; }
private:
	Flux_BufferVRAM m_axBufferVRAMs[MAX_FRAMES_IN_FLIGHT];
};

class Flux_IndexBuffer
{
public:
	void Reset()
	{
		m_xBufferVRAM = Flux_BufferVRAM();
	}

	const Flux_BufferVRAM& GetBufferVRAM() const { return m_xBufferVRAM; }
	Flux_BufferVRAM& GetBufferVRAM() { return m_xBufferVRAM; }
private:
	Flux_BufferVRAM m_xBufferVRAM;
};

class Flux_DynamicConstantBuffer
{
public:
	void Reset()
	{
		for (Flux_BufferVRAM& xBuffer : m_axBufferVRAMs)
		{
			xBuffer = Flux_BufferVRAM();
		}
	}

	const Flux_BufferVRAM& GetBufferVRAM() const { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_BufferVRAM& GetBufferVRAM() { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_BufferVRAM& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) const { return m_axBufferVRAMs[uFrame]; }
	Flux_BufferVRAM& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) { return m_axBufferVRAMs[uFrame]; }
private:
	Flux_BufferVRAM m_axBufferVRAMs[MAX_FRAMES_IN_FLIGHT];
};