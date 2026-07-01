#pragma once

// Placement-new scope guard: Zenith_ComponentPool uses placement new directly
// for explicit lifetime management of pooled components. Save whether the
// zone was already active for this TU, define it if not, disable memory
// management overrides for this header, and restore the zone state at the
// bottom. See the matching #undef block at the end of this file.
//
// Lifted out of Zenith_SceneData.h so the pool types are reachable without
// pulling in SceneData/SceneManager — this is the T2.4 cycle-break that

#include "Collections/Zenith_Vector.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Core/Memory/Zenith_MemoryManagement.h"

// Component pool base class
class Zenith_ComponentPoolBase
{
public:
	virtual ~Zenith_ComponentPoolBase() = default;
};

// Templated component pool with explicit lifetime management
// Uses raw memory to give full control over construction/destruction timing.
//
// The pool is DENSE: live components occupy slots [0, m_uSize) with no holes.
// Removal is a real swap-and-pop (see RemoveAtSwapAndPop) — the last live
// element is move-constructed into the vacated slot, so the pool stays
// contiguous. This contiguity is the precondition for the future sparse-set
// index. m_xOwningEntities is a parallel array whose size always tracks
// m_uSize (one owner EntityID per live slot).
template<typename T>
class Zenith_ComponentPool : public Zenith_ComponentPoolBase
{
public:
	T* m_pxData = nullptr;
	u_int m_uSize = 0;      // Number of live, contiguous slots
	u_int m_uCapacity = 0;  // Allocated capacity
	Zenith_Vector<Zenith_EntityID> m_xOwningEntities;

	// Sparse-set index (WS10 keystone). Maps an entity SLOT index
	// (Zenith_EntityID::m_uIndex) -> the DENSE pool index that holds that
	// entity's component, or uINVALID_DENSE when the entity has no component of
	// this type. This is the O(1) "does entity E have component T, and where?"
	// lookup the sparse-set query fast path walks instead of the per-entity
	// component map. It is maintained ADDITIVELY by the pool mutators below
	// (EmplaceBack / MoveEmplaceBack / MoveConstructAt / RemoveAtSwapAndPop) so
	// every existing call site dual-writes it automatically — no SceneData /
	// Entity call-site edits are needed. The legacy m_axEntityComponents map
	// stays authoritative for the public accessors; this is a parallel index.
	Zenith_Vector<u_int> m_xSparse;
	static constexpr u_int uINVALID_DENSE = 0xFFFFFFFFu;

	static constexpr u_int uINITIAL_CAPACITY = 16;

	Zenith_ComponentPool() = default;

	~Zenith_ComponentPool()
	{
		// Dense pool: every slot in [0, m_uSize) is live, so destruct them all.
		for (u_int i = 0; i < m_uSize; i++)
		{
			Zenith_Assert(m_xOwningEntities.Get(i).IsValid(), "ComponentPool dtor: slot %u in dense range is not occupied", i);
			m_pxData[i].~T();
		}
		// Free raw memory (no automatic destructor calls)
		if (m_pxData)
		{
			Zenith_MemoryManagement::Deallocate(m_pxData);
		}
	}

	// Non-copyable
	Zenith_ComponentPool(const Zenith_ComponentPool&) = delete;
	Zenith_ComponentPool& operator=(const Zenith_ComponentPool&) = delete;

	T& Get(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "ComponentPool::Get: Index %u out of range (size=%u)", uIndex, m_uSize);
		return m_pxData[uIndex];
	}

	const T& Get(u_int uIndex) const
	{
		Zenith_Assert(uIndex < m_uSize, "ComponentPool::Get: Index %u out of range (size=%u)", uIndex, m_uSize);
		return m_pxData[uIndex];
	}

	u_int GetSize() const { return m_uSize; }

	//--------------------------------------------------------------------------
	// Sparse-set index maintenance (WS10)
	//--------------------------------------------------------------------------

	// Point entity slot uSlot at dense index uDense. Zenith_Vector has no
	// fill-resize (the doubling Resize is private; Reserve grows capacity but
	// not size), so grow the logical size with a PushBack(uINVALID_DENSE) loop
	// until uSlot is addressable, then write the entry. Newly grown slots in
	// between default to uINVALID_DENSE (no component), which is exactly the
	// "absent" sentinel GetSparseDense returns.
	void SetSparse(u_int uSlot, u_int uDense)
	{
		while (m_xSparse.GetSize() <= uSlot)
		{
			m_xSparse.PushBack(uINVALID_DENSE);
		}
		m_xSparse.Get(uSlot) = uDense;
	}

	// Dense index for entity slot uSlot, or uINVALID_DENSE if the slot has never
	// been pointed at this pool (sparse array shorter than uSlot) or was cleared.
	// NON-asserting: an out-of-range slot is simply "absent".
	u_int GetSparseDense(u_int uSlot) const
	{
		return (uSlot < m_xSparse.GetSize()) ? m_xSparse.Get(uSlot) : uINVALID_DENSE;
	}

	// Allocate a new slot at the end and construct component in-place
	template<typename... Args>
	u_int EmplaceBack(Zenith_EntityID xOwner, Args&&... args)
	{
		if (m_uSize >= m_uCapacity)
		{
			Grow();
		}
		u_int uIndex = m_uSize++;
		new (&m_pxData[uIndex]) T(std::forward<Args>(args)...);
		m_xOwningEntities.PushBack(xOwner);
		// Sparse dual-write: entity slot -> this new dense index.
		SetSparse(xOwner.m_uIndex, uIndex);
		return uIndex;
	}

	// Real swap-and-pop removal: keeps the pool dense.
	//
	// Destructs the component at uIndex. If uIndex is NOT the last live slot,
	// the last element is MOVE-constructed into uIndex (move — not copy — so
	// components that own VRAM / Jolt handles transfer ownership), the old last
	// slot is destructed, and uIndex's owner is repointed to the moved element's
	// owner. m_uSize is decremented either way.
	//
	// Returns the EntityID that owned the MOVED element (so the caller can
	// repoint that entity's stored component index to uIndex), or
	// INVALID_ENTITY_ID when uIndex was the last/only slot (no move happened).
	Zenith_EntityID RemoveAtSwapAndPop(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "RemoveAtSwapAndPop: Index %u out of range (size=%u)", uIndex, m_uSize);

		// Capture the owner of the last live element AND the owner being removed
		// FIRST — before any move / destruct mutates the parallel owner array.
		const u_int uLastIndex = m_uSize - 1;
		const Zenith_EntityID xMovedOwner = m_xOwningEntities.Get(uLastIndex);
		const Zenith_EntityID xRemovedOwner = m_xOwningEntities.Get(uIndex);

		// Destruct the component being removed.
		m_pxData[uIndex].~T();

		// Sparse dual-write, step 1: the removed entity no longer has this
		// component. Clear it FIRST so the clear-then-repoint order is correct
		// even when uIndex == uLastIndex (removed == tail): the single tail entry
		// is cleared and nothing repoints it back. When uIndex != uLastIndex and
		// removed != tail, step 2 below repoints the tail owner; if removed ==
		// tail it cannot happen here (that is the last/only branch). SetSparse
		// (not a bare Get-assign) so the write is bounds-safe even in the
		// impossible-by-construction case of a short sparse array.
		SetSparse(xRemovedOwner.m_uIndex, uINVALID_DENSE);

		if (uIndex != uLastIndex)
		{
			// Move-construct the last element into the vacated slot, then
			// destruct the old last slot. Move (not memcpy/copy) so handle-owning
			// components transfer ownership rather than aliasing it.
			new (&m_pxData[uIndex]) T(std::move(m_pxData[uLastIndex]));
			m_pxData[uLastIndex].~T();
			m_xOwningEntities.Get(uIndex) = xMovedOwner;
			m_xOwningEntities.PopBack();  // POD EntityID — drops the now-duplicated tail entry, keeps size == m_uSize
			m_uSize--;
			// Sparse dual-write, step 2: the tail element now lives at uIndex.
			// Repoint its owner's sparse entry. (Safe even if xMovedOwner ==
			// xRemovedOwner is impossible here — they are different live slots.)
			SetSparse(xMovedOwner.m_uIndex, uIndex);
			return xMovedOwner;
		}

		// uIndex was the last/only slot: nothing moved. Sparse was already
		// cleared above; do ONLY the clear (no repoint).
		m_xOwningEntities.PopBack();
		m_uSize--;
		return INVALID_ENTITY_ID;
	}

	// Move-construct component at existing slot (for cross-scene transfer into
	// an already-grown slot). Caller guarantees uIndex < m_uSize.
	void MoveConstructAt(u_int uIndex, Zenith_EntityID xOwner, T&& xSource)
	{
		Zenith_Assert(uIndex < m_uSize, "MoveConstructAt: Index out of range");
		new (&m_pxData[uIndex]) T(std::move(xSource));
		m_xOwningEntities.Get(uIndex) = xOwner;
		// Sparse dual-write: entity slot -> this (existing) dense index.
		SetSparse(xOwner.m_uIndex, uIndex);
	}

	// Move a component into a new slot at the end (for cross-scene transfer)
	u_int MoveEmplaceBack(Zenith_EntityID xOwner, T&& xSource)
	{
		if (m_uSize >= m_uCapacity)
		{
			Grow();
		}
		u_int uIndex = m_uSize++;
		new (&m_pxData[uIndex]) T(std::move(xSource));
		m_xOwningEntities.PushBack(xOwner);
		// Sparse dual-write: entity slot -> this new dense index.
		SetSparse(xOwner.m_uIndex, uIndex);
		return uIndex;
	}

private:
	// NOTE (WS10 sparse index): Grow() reallocates the dense component buffer but
	// every live element keeps its SAME dense index [0, m_uSize) — only the
	// backing storage address moves. The sparse array maps entity-slot -> dense
	// index, and those dense indices are unchanged, so Grow() MUST NOT touch
	// m_xSparse. (Capacity, not logical contents, changed.)
	void Grow()
	{
		u_int uNewCapacity = m_uCapacity == 0 ? uINITIAL_CAPACITY : m_uCapacity * 2;

		// Allocate new buffer
		T* pxNewData = static_cast<T*>(Zenith_MemoryManagement::Allocate(uNewCapacity * sizeof(T)));
		Zenith_Assert(pxNewData != nullptr, "ComponentPool::Grow: Allocation failed");

		// Dense pool: move every live element in [0, m_uSize) to the new buffer.
		for (u_int i = 0; i < m_uSize; i++)
		{
			Zenith_Assert(m_xOwningEntities.Get(i).IsValid(), "ComponentPool::Grow: slot %u in dense range is not occupied", i);
			// Move-construct at new location, then destruct old location.
			new (&pxNewData[i]) T(std::move(m_pxData[i]));
			m_pxData[i].~T();
		}

		// Free old buffer
		if (m_pxData)
		{
			Zenith_MemoryManagement::Deallocate(m_pxData);
		}

		m_pxData = pxNewData;
		m_uCapacity = uNewCapacity;
	}
};

// Component concept (C++20)
template<typename T>
concept Zenith_Component =
	std::is_constructible_v<T, Zenith_Entity&>&&
	std::is_destructible_v<T>
#ifdef ZENITH_TOOLS
	&&
	requires(T& t) { { t.RenderPropertiesPanel() } -> std::same_as<void>; }
#endif
;

