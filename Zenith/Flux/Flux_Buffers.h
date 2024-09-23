#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	void Reset()
	{
		m_xBuffer.Reset();
	}

	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};
class Flux_DynamicVertexBuffer
{
public:
	void Reset()
	{
		for (Flux_Buffer& xBuffer : m_axBuffers)
		{
			xBuffer.Reset();
		}
	}
	const Flux_Buffer& GetBuffer() const { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_Buffer& GetBuffer() { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }

	Flux_Buffer& GetBufferForFrameInFlight(const uint32_t uFrame) { return m_axBuffers[uFrame]; }
private:
	Flux_Buffer m_axBuffers[MAX_FRAMES_IN_FLIGHT];
};

class Flux_IndexBuffer
{
public:
	void Reset()
	{
		m_xBuffer.Reset();
	}

	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }
private:
	Flux_Buffer m_xBuffer;
};

class Flux_ConstantBuffer
{
public:
	void Reset()
	{
		for (Flux_Buffer& xBuffer : m_axBuffers)
		{
			xBuffer.Reset();
		}
	}

	const Flux_Buffer& GetBuffer() const { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_Buffer& GetBuffer() { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }

	Flux_Buffer& GetBufferForFrameInFlight(const uint32_t uFrame) { return m_axBuffers[uFrame]; }
private:
	Flux_Buffer m_axBuffers[MAX_FRAMES_IN_FLIGHT];
};