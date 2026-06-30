#pragma once

#include "Collections/Zenith_Vector.h"
#include "Core/ZenithConfig.h"

// ============================================================================
// Flux_BindlessAllocator — dense index allocator for the bindless texture table
// (set 2, g_axTextures[]).
//
// WHY: historically a texture's bindless slot WAS its image-view registry handle
// (handle.AsUInt()). That registry is dense over ALL image views (render targets,
// depth, UAVs, transients, cubemaps), so the bindless table had to span the whole
// view space even though only a small subset of views are sampled bindlessly.
// This allocator hands out a SEPARATE dense index space containing only the
// bindless textures, so the table can stay small.
//
// - Index 0 is RESERVED (never handed out) for the engine default-white texture,
//   so an unset material slot can reference slot 0 safely.
// - Free is DEFERRED by MAX_FRAMES_IN_FLIGHT+1 frames (AdvanceFrame drives reclaim)
//   so an index is never recycled while its descriptor may still be read by an
//   in-flight frame.
// - A monotonic generation counter bumps on every Free; consumers that cache a
//   GPU record keyed on it (e.g. the material table) can detect a table change.
//
// Pure index bookkeeping — no Vulkan/D3D dependency — so it is unit-testable
// without booting the renderer. Header-only (trivial bodies) to avoid a Sharpmake
// regen for a single TU.
// ============================================================================
class Flux_BindlessAllocator
{
public:
	// Index 0 is reserved for the engine default-white texture (never allocated).
	static constexpr u_int uRESERVED_DEFAULT_WHITE = 0u;

	Flux_BindlessAllocator() = default;

	// uCapacity = the backend's clamped bindless table size. Allocations beyond it
	// assert. Resets all state (idempotent — safe to re-Initialise).
	void Initialise(u_int uCapacity)
	{
		Zenith_Assert(uCapacity > 1, "Bindless table capacity %u too small (slot 0 is reserved)", uCapacity);
		m_uCapacity   = uCapacity;
		m_uHighWater  = 1;   // slot 0 reserved for the engine default-white texture
		m_uFrame      = 0;
		m_uGeneration = 0;
		m_xFreeList.Clear();
		m_xPendingFree.Clear();
		m_xLive.Clear();
		m_xLive.Resize(uCapacity, 0);
	}

	// Returns a dense index in [1, uCapacity). Reuses a reclaimed index when
	// available, otherwise grows the high-water mark. Asserts on exhaustion.
	u_int Allocate()
	{
		u_int uIndex;
		if (m_xFreeList.GetSize() > 0)
		{
			uIndex = m_xFreeList.GetBack();
			m_xFreeList.PopBack();
		}
		else
		{
			Zenith_Assert(m_uHighWater < m_uCapacity,
				"Bindless table exhausted (%u/%u in use) — raise FLUX_BINDLESS_TABLE_SIZE_TARGET or free textures",
				m_uHighWater, m_uCapacity);
			uIndex = m_uHighWater++;
		}
		m_xLive.Get(uIndex) = 1;
		m_uLiveCount++;
		return uIndex;
	}

	// Defers the index for reclaim; it becomes allocatable again only after
	// MAX_FRAMES_IN_FLIGHT+1 AdvanceFrame() calls. Ignores the invalid sentinel,
	// the reserved slot 0, and a double-free / free-of-unallocated (idempotent).
	// Bumps the generation counter on a real free.
	void Free(u_int uIndex)
	{
		if (uIndex == uRESERVED_DEFAULT_WHITE || uIndex >= m_uCapacity || m_xLive.Get(uIndex) == 0)
		{
			return;
		}
		m_xLive.Get(uIndex) = 0;
		m_uLiveCount--;
		m_xPendingFree.PushBack({ uIndex, m_uFrame });
		m_uGeneration++;
	}

	// Advances the internal frame clock by one and reclaims any deferred index
	// whose grace period has elapsed. Call exactly once per rendered frame.
	void AdvanceFrame()
	{
		m_uFrame++;

		// Stable order is irrelevant (indices are interchangeable) — swap-erase.
		for (u_int i = 0; i < m_xPendingFree.GetSize(); )
		{
			if (m_xPendingFree.Get(i).m_uFrameFreed + uFREE_GRACE_FRAMES <= m_uFrame)
			{
				m_xFreeList.PushBack(m_xPendingFree.Get(i).m_uIndex);
				m_xPendingFree.Get(i) = m_xPendingFree.GetBack();
				m_xPendingFree.PopBack();
			}
			else
			{
				++i;
			}
		}
	}

	u_int    GetCapacity()   const { return m_uCapacity; }
	u_int64  GetGeneration() const { return m_uGeneration; }
	// Indices currently allocated (Allocate'd and not yet Free'd). Pending-free
	// indices (within the grace window) are NOT counted — they have been freed.
	u_int    GetLiveCount()  const { return m_uLiveCount; }

private:
	struct PendingFree
	{
		u_int   m_uIndex;
		u_int64 m_uFrameFreed;
	};

	// Indices are recycled only once the GPU can no longer be reading the old
	// descriptor: free + this many frames.
	static constexpr u_int64 uFREE_GRACE_FRAMES = MAX_FRAMES_IN_FLIGHT + 1;

	u_int                    m_uCapacity   = 0;
	u_int                    m_uHighWater  = 1;   // next fresh index (0 reserved)
	u_int                    m_uLiveCount  = 0;
	u_int64                  m_uFrame      = 0;
	u_int64                  m_uGeneration = 0;
	Zenith_Vector<u_int>       m_xFreeList;          // reclaimed, ready to reissue
	Zenith_Vector<PendingFree> m_xPendingFree;       // inside the grace window
	Zenith_Vector<u_int8>      m_xLive;               // per-index allocated flag (double-free guard)
};
