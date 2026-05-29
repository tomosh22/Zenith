#pragma once

#include "Flux/Flux.h"

// ---------------------------------------------------------------------------
// Flux buffer wrappers.
//
// Seven concrete types live here (Vertex, Index, Constant, Indirect,
// ReadWrite + Dynamic* variants). They all follow one of two implementation
// patterns:
//   - single-buffer: one Flux_Buffer + optional view
//   - frame-indexed: Flux_Buffer[MAX_FRAMES_IN_FLIGHT] + optional parallel
//     view array
//
// The two template bases below capture those patterns. Concrete classes
// inherit and add the domain-specific accessor names (GetCBV / GetUAV) so
// compile-time binding type safety is preserved at call sites — e.g.
// Flux_CommandBindCBV still takes Flux_ConstantBufferView* and the compiler
// rejects mixing a vertex buffer for an index buffer. Flagged as a
// simplification candidate in Flux/CLAUDE.md:110-112.
// ---------------------------------------------------------------------------

// Sentinel used when a buffer wrapper has no associated view.
struct Flux_NoView {};

namespace Zenith_FluxBuffers_Detail
{
	inline u_int CurrentFrameIndex()
	{
		const u_int uIndex = g_xEngine.VulkanSwapchain().GetCurrentFrameIndex();
		Zenith_Assert(uIndex < MAX_FRAMES_IN_FLIGHT, "Frame index %u out of bounds (max %u)", uIndex, MAX_FRAMES_IN_FLIGHT);
		return uIndex;
	}
	inline void AssertFrameIndex(u_int uFrame)
	{
		Zenith_Assert(uFrame < MAX_FRAMES_IN_FLIGHT, "Frame index %u out of bounds (max %u)", uFrame, MAX_FRAMES_IN_FLIGHT);
	}
}

// Single-buffer pattern: one Flux_Buffer + (optional) one view.
template<typename TView>
class Flux_SingleBufferBase
{
public:
	void Reset()
	{
		m_xBuffer = Flux_Buffer();
	}

	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }

	// Generic view accessor. The concrete leaves forward their domain-specific
	// names (GetCBV / GetUAV / ...) to this so compile-time binder type safety
	// is preserved at call sites while the body lives in one place.
	TView& GetView() { return m_xView; }
	const TView& GetView() const { return m_xView; }

protected:
	Flux_Buffer m_xBuffer;
	TView m_xView;
};

template<>
class Flux_SingleBufferBase<Flux_NoView>
{
public:
	void Reset() { m_xBuffer = Flux_Buffer(); }
	const Flux_Buffer& GetBuffer() const { return m_xBuffer; }
	Flux_Buffer& GetBuffer() { return m_xBuffer; }

protected:
	Flux_Buffer m_xBuffer;
};

// Frame-indexed pattern: MAX_FRAMES_IN_FLIGHT Flux_Buffers + (optional)
// parallel view array.
template<typename TView>
class Flux_FrameIndexedBufferBase
{
public:
	void Reset()
	{
		for (u_int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_axBuffers[i] = Flux_Buffer();
		}
	}

	const Flux_Buffer& GetBuffer() const
	{
		return m_axBuffers[Zenith_FluxBuffers_Detail::CurrentFrameIndex()];
	}
	Flux_Buffer& GetBuffer()
	{
		return m_axBuffers[Zenith_FluxBuffers_Detail::CurrentFrameIndex()];
	}

	const Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame) const
	{
		Zenith_FluxBuffers_Detail::AssertFrameIndex(uFrame);
		return m_axBuffers[uFrame];
	}
	Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame)
	{
		Zenith_FluxBuffers_Detail::AssertFrameIndex(uFrame);
		return m_axBuffers[uFrame];
	}

	// Generic view accessors. The concrete leaves forward their domain-specific
	// names (GetCBV / GetUAV / ...) to these so compile-time binder type safety
	// is preserved at call sites while the bodies live in one place.
	TView& GetView() { return m_axViews[Zenith_FluxBuffers_Detail::CurrentFrameIndex()]; }
	const TView& GetView() const { return m_axViews[Zenith_FluxBuffers_Detail::CurrentFrameIndex()]; }

	TView& GetViewForFrameInFlight(const u_int uFrame)
	{
		Zenith_FluxBuffers_Detail::AssertFrameIndex(uFrame);
		return m_axViews[uFrame];
	}

protected:
	Flux_Buffer m_axBuffers[MAX_FRAMES_IN_FLIGHT];
	TView m_axViews[MAX_FRAMES_IN_FLIGHT];
};

template<>
class Flux_FrameIndexedBufferBase<Flux_NoView>
{
public:
	void Reset()
	{
		for (u_int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_axBuffers[i] = Flux_Buffer();
		}
	}

	const Flux_Buffer& GetBuffer() const
	{
		return m_axBuffers[Zenith_FluxBuffers_Detail::CurrentFrameIndex()];
	}
	Flux_Buffer& GetBuffer()
	{
		return m_axBuffers[Zenith_FluxBuffers_Detail::CurrentFrameIndex()];
	}

	const Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame) const
	{
		Zenith_FluxBuffers_Detail::AssertFrameIndex(uFrame);
		return m_axBuffers[uFrame];
	}
	Flux_Buffer& GetBufferForFrameInFlight(const u_int uFrame)
	{
		Zenith_FluxBuffers_Detail::AssertFrameIndex(uFrame);
		return m_axBuffers[uFrame];
	}

protected:
	Flux_Buffer m_axBuffers[MAX_FRAMES_IN_FLIGHT];
};

// ---------------------------------------------------------------------------
// Concrete types. Each is a distinct class so overloaded command-list binders
// (Flux_CommandBindCBV, Flux_CommandBindUAV, ...) remain type-safe and will
// reject a vertex buffer passed as an index buffer.
// ---------------------------------------------------------------------------

class Flux_VertexBuffer : public Flux_SingleBufferBase<Flux_NoView> {};
class Flux_IndexBuffer  : public Flux_SingleBufferBase<Flux_NoView> {};

class Flux_ConstantBuffer : public Flux_SingleBufferBase<Flux_ConstantBufferView>
{
public:
	Flux_ConstantBufferView& GetCBV() { return GetView(); }
};

class Flux_IndirectBuffer : public Flux_SingleBufferBase<Flux_UnorderedAccessView_Buffer>
{
public:
	Flux_UnorderedAccessView_Buffer& GetUAV() { return GetView(); }
};

class Flux_ReadWriteBuffer : public Flux_SingleBufferBase<Flux_UnorderedAccessView_Buffer>
{
public:
	Flux_UnorderedAccessView_Buffer& GetUAV() { return GetView(); }
	const Flux_UnorderedAccessView_Buffer& GetUAV() const { return GetView(); }

	// Read-only structured-buffer view, populated alongside m_xView at buffer
	// init time (see Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer).
	// The Vulkan descriptor is identical for read-only and read-write SSBOs;
	// the distinct view type lets render-graph access declarations and the
	// shader binder route through the read-only path without claiming a write.
	Flux_ShaderResourceView_Buffer& GetSRV() { return m_xSRVView; }
	const Flux_ShaderResourceView_Buffer& GetSRV() const { return m_xSRVView; }

	// Override the base Reset() so both views are cleared alongside the buffer.
	// The base only clears m_xBuffer (and m_xView via the TView template); the
	// SRV mirror lives outside the base's TView slot, so it must be explicitly
	// reset here to avoid handle leaks across buffer destroy/recreate cycles.
	void Reset()
	{
		Flux_SingleBufferBase<Flux_UnorderedAccessView_Buffer>::Reset();
		m_xView    = Flux_UnorderedAccessView_Buffer{};
		m_xSRVView = Flux_ShaderResourceView_Buffer{};
	}

private:
	Flux_ShaderResourceView_Buffer m_xSRVView;
};

class Flux_DynamicVertexBuffer : public Flux_FrameIndexedBufferBase<Flux_NoView> {};

class Flux_DynamicConstantBuffer : public Flux_FrameIndexedBufferBase<Flux_ConstantBufferView>
{
public:
	const Flux_ConstantBufferView& GetCBV() const { return GetView(); }

	Flux_ConstantBufferView& GetCBVForFrameInFlight(const u_int uFrame) { return GetViewForFrameInFlight(uFrame); }
};

class Flux_DynamicReadWriteBuffer : public Flux_FrameIndexedBufferBase<Flux_UnorderedAccessView_Buffer>
{
public:
	const Flux_UnorderedAccessView_Buffer& GetUAV() const { return GetView(); }
	Flux_UnorderedAccessView_Buffer& GetUAV() { return GetView(); }

	Flux_UnorderedAccessView_Buffer& GetUAVForFrameInFlight(const u_int uFrame) { return GetViewForFrameInFlight(uFrame); }

	// Read-only structured-buffer view, populated alongside the UAV at buffer
	// init time (see Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer).
	// The Vulkan descriptor is identical for read-only and read-write SSBOs;
	// the distinct view type lets render-graph access declarations and the
	// shader binder route through the read-only path without claiming a write.
	// Mirrors the same SRV shape that Flux_ReadWriteBuffer exposes.
	const Flux_ShaderResourceView_Buffer& GetSRV() const
	{
		return m_axSRVViews[Zenith_FluxBuffers_Detail::CurrentFrameIndex()];
	}
	Flux_ShaderResourceView_Buffer& GetSRV()
	{
		return m_axSRVViews[Zenith_FluxBuffers_Detail::CurrentFrameIndex()];
	}

	Flux_ShaderResourceView_Buffer& GetSRVForFrameInFlight(const u_int uFrame)
	{
		Zenith_FluxBuffers_Detail::AssertFrameIndex(uFrame);
		return m_axSRVViews[uFrame];
	}

	void Reset()
	{
		Flux_FrameIndexedBufferBase<Flux_UnorderedAccessView_Buffer>::Reset();
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			m_axViews[u]    = Flux_UnorderedAccessView_Buffer{};
			m_axSRVViews[u] = Flux_ShaderResourceView_Buffer{};
		}
	}

private:
	Flux_ShaderResourceView_Buffer m_axSRVViews[MAX_FRAMES_IN_FLIGHT];
};
