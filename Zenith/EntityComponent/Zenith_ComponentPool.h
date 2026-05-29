#pragma once

// Placement-new scope guard: Zenith_ComponentPool uses placement new directly
// for explicit lifetime management of pooled components. Save whether the
// zone was already active for this TU, define it if not, disable memory
// management overrides for this header, and restore the zone state at the
// bottom. See the matching #undef block at the end of this file.
//
// Lifted out of Zenith_SceneData.h so the pool types are reachable without
// pulling in SceneData/SceneManager — this is the T2.4 cycle-break that
// breaks the Scene.h ↔ SceneRegistry.h ↔ SceneData.h ↔ SceneManager.h chain.
#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_COMPONENTPOOL_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
#include "Memory/Zenith_MemoryManagement_Disabled.h"

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Zenith_Entity.h"
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

		// Capture the owner of the last live element FIRST — before any move /
		// destruct mutates the parallel owner array.
		const u_int uLastIndex = m_uSize - 1;
		const Zenith_EntityID xMovedOwner = m_xOwningEntities.Get(uLastIndex);

		// Destruct the component being removed.
		m_pxData[uIndex].~T();

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
			return xMovedOwner;
		}

		// uIndex was the last/only slot: nothing moved.
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
		return uIndex;
	}

private:
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

// Restore zone state
#ifndef ZENITH_COMPONENTPOOL_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_COMPONENTPOOL_ZONE_WAS_SET
