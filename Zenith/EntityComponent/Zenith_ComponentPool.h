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

// Unity-style component handle with generation counter
template<typename T>
struct Zenith_ComponentHandle
{
	uint32_t m_uIndex = UINT32_MAX;
	uint32_t m_uGeneration = 0;

	bool IsValid() const { return m_uIndex != UINT32_MAX; }
	static Zenith_ComponentHandle Invalid() { return { UINT32_MAX, 0 }; }

	bool operator==(const Zenith_ComponentHandle& xOther) const
	{
		return m_uIndex == xOther.m_uIndex && m_uGeneration == xOther.m_uGeneration;
	}
	bool operator!=(const Zenith_ComponentHandle& xOther) const { return !(*this == xOther); }
};

// Templated component pool with explicit lifetime management
// Uses raw memory to give full control over construction/destruction timing
template<typename T>
class Zenith_ComponentPool : public Zenith_ComponentPoolBase
{
public:
	T* m_pxData = nullptr;
	u_int m_uSize = 0;      // Number of slots (high water mark)
	u_int m_uCapacity = 0;  // Allocated capacity
	Zenith_Vector<Zenith_EntityID> m_xOwningEntities;
	Zenith_Vector<uint32_t> m_xGenerations;
	Zenith_Vector<uint32_t> m_xFreeIndices;

	static constexpr u_int uINITIAL_CAPACITY = 16;

	Zenith_ComponentPool() = default;

	~Zenith_ComponentPool()
	{
		// Only destruct occupied slots - freed slots were already destructed in RemoveComponentFromEntity
		for (u_int i = 0; i < m_uSize; i++)
		{
			if (m_xOwningEntities.Get(i).IsValid())
			{
				m_pxData[i].~T();
			}
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

	// Allocate a new slot and construct component in-place
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
		m_xGenerations.PushBack(1);
		return uIndex;
	}

	// Construct component at existing slot (for reuse)
	template<typename... Args>
	void ConstructAt(u_int uIndex, Zenith_EntityID xOwner, Args&&... args)
	{
		Zenith_Assert(uIndex < m_uSize, "ConstructAt: Index out of range");
		new (&m_pxData[uIndex]) T(std::forward<Args>(args)...);
		m_xOwningEntities.Get(uIndex) = xOwner;
		m_xGenerations.Get(uIndex)++;
	}

	// Destruct component at slot (marks as free)
	void DestructAt(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "DestructAt: Index out of range");
		m_pxData[uIndex].~T();
		m_xOwningEntities.Get(uIndex) = INVALID_ENTITY_ID;
	}

	// Move-construct component at existing slot (for cross-scene transfer)
	void MoveConstructAt(u_int uIndex, Zenith_EntityID xOwner, T&& xSource)
	{
		Zenith_Assert(uIndex < m_uSize, "MoveConstructAt: Index out of range");
		new (&m_pxData[uIndex]) T(std::move(xSource));
		m_xOwningEntities.Get(uIndex) = xOwner;
		m_xGenerations.Get(uIndex)++;
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
		m_xGenerations.PushBack(1);
		return uIndex;
	}

	bool IsSlotOccupied(uint32_t uIndex) const
	{
		if (uIndex >= m_xOwningEntities.GetSize()) return false;
		return m_xOwningEntities.Get(uIndex).IsValid();
	}

	uint32_t GetGeneration(uint32_t uIndex) const
	{
		Zenith_Assert(uIndex < m_xGenerations.GetSize(), "GetGeneration: Invalid component index %u", uIndex);
		return m_xGenerations.Get(uIndex);
	}

private:
	void Grow()
	{
		u_int uNewCapacity = m_uCapacity == 0 ? uINITIAL_CAPACITY : m_uCapacity * 2;

		// Allocate new buffer
		T* pxNewData = static_cast<T*>(Zenith_MemoryManagement::Allocate(uNewCapacity * sizeof(T)));
		Zenith_Assert(pxNewData != nullptr, "ComponentPool::Grow: Allocation failed");

		// Move existing components to new buffer
		for (u_int i = 0; i < m_uSize; i++)
		{
			if (m_xOwningEntities.Get(i).IsValid())
			{
				// Move-construct at new location
				new (&pxNewData[i]) T(std::move(m_pxData[i]));
				// Destruct old location
				m_pxData[i].~T();
			}
			// Note: freed slots are left uninitialized in new buffer (will be constructed when reused)
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
