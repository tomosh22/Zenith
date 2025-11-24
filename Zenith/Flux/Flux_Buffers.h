#pragma once

#include "Flux/Flux.h"

class Flux_VertexBuffer
{
public:
	void Reset()
	{
		m_xBuffer = Flux_Buffer();
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
			xBuffer = Flux_Buffer();
		}
	}
	const Flux_Buffer& GetBuffer() const { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_Buffer& GetBuffer() { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame) const { return m_axBuffers[uFrame]; }
	Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame) { return m_axBuffers[uFrame]; }
private:
	Flux_Buffer m_axBuffers[MAX_FRAMES_IN_FLIGHT];
	
};

class Flux_IndexBuffer
{
public:
	void Reset()
	{
		m_xBuffer = Flux_Buffer();
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
		m_xBuffer = Flux_Buffer();
	}

	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }

	Flux_ConstantBufferView& GetCBV() { return m_xCBV; }
private:
	Flux_Buffer m_xBuffer;
	Flux_ConstantBufferView m_xCBV;
};

class Flux_DynamicConstantBuffer
{
public:
	void Reset()
	{
		for (Flux_Buffer& xBuffer : m_axBuffers)
		{
			xBuffer = Flux_Buffer();
		}
	}

	const Flux_Buffer& GetBuffer() const { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }
	Flux_Buffer& GetBuffer() { return m_axBuffers[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_ConstantBufferView& GetCBV() const { return m_axCBVs[Flux_Swapchain::GetCurrentFrameIndex()]; }

	const Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame) const { return m_axBuffers[uFrame]; }
	Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame) { return m_axBuffers[uFrame]; }

	Flux_ConstantBufferView& GetCBVForFrameInFlight(const u_int uFrame) { return m_axCBVs[uFrame]; }
private:
	Flux_Buffer m_axBuffers[MAX_FRAMES_IN_FLIGHT];
	Flux_ConstantBufferView m_axCBVs[MAX_FRAMES_IN_FLIGHT];
};

class Flux_IndirectBuffer
{
public:
	void Reset()
	{
		m_xBuffer = Flux_Buffer();
	}

	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }

	Flux_UnorderedAccessView_Buffer& GetUAV() { return m_xUAV; }
private:
	Flux_Buffer m_xBuffer;
	Flux_UnorderedAccessView_Buffer m_xUAV;
};

class Flux_ReadWriteBuffer
{
public:
	void Reset()
	{
		m_xBuffer = Flux_Buffer();
	}

	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }

	Flux_UnorderedAccessView_Buffer& GetUAV() { return m_xUAV; }
private:
	Flux_Buffer m_xBuffer;
	Flux_UnorderedAccessView_Buffer m_xUAV;
};