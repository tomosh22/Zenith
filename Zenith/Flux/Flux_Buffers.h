#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	void Reset()
	{
		m_xBufferVRAM = Flux_Buffer();
	}

	const Flux_Buffer& GetBufferVRAM() const { return m_xBufferVRAM; }
	Flux_Buffer& GetBufferVRAM() { return m_xBufferVRAM; }
private:
	Flux_Buffer m_xBufferVRAM;
};

class Flux_DynamicVertexBuffer
{
public:
	void Reset()
	{
		for (Flux_Buffer& xBuffer : m_axBufferVRAMs)
		{
			xBuffer = Flux_Buffer();
		}
	}
	const Flux_Buffer& GetBufferVRAM() const { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_Buffer& GetBufferVRAM() { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_Buffer& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) const { return m_axBufferVRAMs[uFrame]; }
	Flux_Buffer& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) { return m_axBufferVRAMs[uFrame]; }
private:
	Flux_Buffer m_axBufferVRAMs[MAX_FRAMES_IN_FLIGHT];
};

class Flux_IndexBuffer
{
public:
	void Reset()
	{
		m_xBufferVRAM = Flux_Buffer();
	}

	const Flux_Buffer& GetBufferVRAM() const { return m_xBufferVRAM; }
	Flux_Buffer& GetBufferVRAM() { return m_xBufferVRAM; }
private:
	Flux_Buffer m_xBufferVRAM;
};

class Flux_DynamicConstantBuffer
{
public:
	void Reset()
	{
		for (Flux_Buffer& xBuffer : m_axBufferVRAMs)
		{
			xBuffer = Flux_Buffer();
		}
	}

	const Flux_Buffer& GetBufferVRAM() const { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_Buffer& GetBufferVRAM() { return m_axBufferVRAMs[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_Buffer& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) const { return m_axBufferVRAMs[uFrame]; }
	Flux_Buffer& GetBufferVRAMForFrameInFlight(const uint32_t uFrame) { return m_axBufferVRAMs[uFrame]; }
private:
	Flux_Buffer m_axBufferVRAMs[MAX_FRAMES_IN_FLIGHT];
};