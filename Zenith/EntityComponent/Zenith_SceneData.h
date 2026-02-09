#pragma once

// Save zone state and set up placement new protection
#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_SCENEDATA_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
#include "Memory/Zenith_MemoryManagement_Disabled.h"

#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <atomic>
#include <string>

class Zenith_CameraComponent;
class Zenith_TransformComponent;

// Forward declare for Zenith_Entity reference
class Zenith_SceneData;

// Include Zenith_Entity to get complete EntityID type definition
#include "EntityComponent/Zenith_Entity.h"

// Component pool base class (moved from Zenith_Scene.h)
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

// Forward declaration for query system
template<typename... Ts> class Zenith_Query;

/**
 * Zenith_SceneData - Internal scene storage class
 *
 * This class holds the actual scene data (entity pools, components, etc.)
 * It was previously the internals of Zenith_Scene before the multi-scene refactor.
 *
 * Zenith_Scene is now a lightweight handle; this class holds the real data.
 * Managed by Zenith_SceneManager.
 */
class Zenith_SceneData
{
public:
	using TypeID = u_int;
	class TypeIDGenerator
	{
	public:
		template<Zenith_Component T>
		static TypeID GetTypeID()
		{
			static TypeID ls_uRet = s_uCounter++;
			return ls_uRet;
		}
	private:
		inline static TypeID s_uCounter = 0;
	};

	//==========================================================================
	// Construction / Reset
	//==========================================================================

	Zenith_SceneData();
	~Zenith_SceneData();
	void Reset();

	//==========================================================================
	// Read-Only Scene Properties
	//==========================================================================

	const std::string& GetName() const { return m_strName; }
	const std::string& GetPath() const { return m_strPath; }
	int GetBuildIndex() const { return m_iBuildIndex; }
	int GetHandle() const { return m_iHandle; }
	bool IsLoaded() const { return m_bIsLoaded; }
	bool IsUnloading() const { return m_bIsUnloading; }
	bool WasLoadedAdditively() const { return m_bWasLoadedAdditively; }
	bool IsPaused() const { return m_bIsPaused; }

	//==========================================================================
	// Dirty Tracking (Editor)
	//==========================================================================

#ifdef ZENITH_TOOLS
	void MarkDirty() { m_bHasUnsavedChanges = true; }
	void ClearDirty() { m_bHasUnsavedChanges = false; }
#else
	void MarkDirty() {}
	void ClearDirty() {}
#endif

	//==========================================================================
	// Entity Management
	//==========================================================================

	void RemoveEntity(Zenith_EntityID xID);

	// Thread-safe: only reads from s_axEntitySlots which is stable during task execution
	// (main thread does not modify entity storage while worker threads are running)
	bool EntityExists(Zenith_EntityID xID) const
	{
		if (xID.m_uIndex == Zenith_EntityID::INVALID_INDEX) return false;
		if (xID.m_uIndex >= s_axEntitySlots.GetSize()) return false;
		const Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
		return xSlot.m_bOccupied && xSlot.m_uGeneration == xID.m_uGeneration;
	}

	Zenith_Entity GetEntity(Zenith_EntityID xID);
	Zenith_Entity TryGetEntity(Zenith_EntityID xID);
	Zenith_Entity FindEntityByName(const std::string& strName);

	//==========================================================================
	// Entity Count & Queries
	//==========================================================================

	u_int GetEntityCount() const
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetEntityCount must be called from main thread");
		return m_xActiveEntities.GetSize();
	}
	const Zenith_Vector<Zenith_EntityID>& GetActiveEntities() const
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetActiveEntities must be called from main thread");
		return m_xActiveEntities;
	}

	// Root entity cache for O(1) count access (Unity scene.rootCount parity)
	uint32_t GetCachedRootEntityCount();
	void GetCachedRootEntities(Zenith_Vector<Zenith_EntityID>& axOut);

	//==========================================================================
	// Component Management
	//==========================================================================

	template<typename T, typename... Args>
	T& CreateComponent(Zenith_EntityID xID, Args&&... args);

	template<typename T>
	bool EntityHasComponent(Zenith_EntityID xID) const;

	template<typename T>
	T& GetComponentFromEntity(Zenith_EntityID xID) const;

	template<typename T>
	bool RemoveComponentFromEntity(Zenith_EntityID xID);

	template<typename T>
	void GetAllOfComponentType(Zenith_Vector<T*>& xOut) const;

	template<typename T>
	bool IsComponentHandleValid(const Zenith_ComponentHandle<T>& xHandle) const;

	template<typename T>
	T* TryGetComponentFromHandle(const Zenith_ComponentHandle<T>& xHandle) const;

	template<typename T>
	Zenith_ComponentHandle<T> GetComponentHandle(Zenith_EntityID xID) const;

	template<typename... Ts>
	Zenith_Query<Ts...> Query();

	//==========================================================================
	// Camera
	//==========================================================================

	void SetMainCameraEntity(Zenith_EntityID xEntity);
	Zenith_EntityID GetMainCameraEntity() const;
	Zenith_CameraComponent& GetMainCamera() const;
	Zenith_CameraComponent* TryGetMainCamera() const;

	//==========================================================================
	// Serialization
	//==========================================================================

	void SaveToFile(const std::string& strFilename, bool bIncludeTransient = false);
	bool LoadFromFile(const std::string& strFilename);
	bool LoadFromDataStream(Zenith_DataStream& xStream);

private:
	//==========================================================================
	// Friend Declarations
	//==========================================================================

	friend class Zenith_Entity;
	friend class Zenith_TransformComponent;
	friend class Zenith_SceneManager;
	friend struct Zenith_Scene;
	template<typename... Ts> friend class Zenith_Query;
	friend class Zenith_SceneTests;
	friend class Zenith_Prefab;
	friend class Zenith_ScriptComponent;
	friend class Zenith_ComponentMetaRegistry;
#ifdef ZENITH_TOOLS
	friend class Zenith_Editor;
	friend class Zenith_SelectionSystem;
#endif

	//==========================================================================
	// Entity Slot Storage (Generation Counter System)
	//==========================================================================

	struct Zenith_EntitySlot
	{
		std::string m_strName;
		bool m_bEnabled = true;
		bool m_bTransient = true;

		uint32_t m_uGeneration = 0;
		bool m_bOccupied = false;
		bool m_bMarkedForDestruction = false;
		int m_iSceneHandle = -1;        // Which scene owns this entity

		// Lifecycle flags (previously per-scene bool arrays, now per-entity for global ID support)
		bool m_bAwoken = false;
		bool m_bStarted = false;
		bool m_bPendingStart = false;
		bool m_bCreatedDuringUpdate = false;
		bool m_bOnEnableDispatched = false;  // Tracks whether OnEnable has been dispatched (Unity parity: prevents double-dispatch)

		// Cached activeInHierarchy state (Unity parity: avoids O(depth) parent chain walk per call)
		// Invalidated when SetEnabled or SetParent changes. Rebuilt lazily on first access.
		mutable bool m_bActiveInHierarchy = true;
		mutable bool m_bActiveInHierarchyDirty = true;
	};

	const Zenith_EntitySlot& GetSlot(Zenith_EntityID xID) const;

	// Global entity storage (shared across all scenes - indexed by EntityID.m_uIndex)
	static Zenith_Vector<Zenith_EntitySlot> s_axEntitySlots;
	static Zenith_Vector<uint32_t> s_axFreeEntityIndices;
	static Zenith_Vector<std::unordered_map<TypeID, u_int>> s_axEntityComponents; // #TODO: Replace with engine hash map
	static void ResetGlobalEntityStorage();

	// Internal entity creation - use Zenith_Entity constructor for external code
	Zenith_EntityID CreateEntity();

	//==========================================================================
	// Scene Metadata (private - use read-only accessors above)
	//==========================================================================

	std::string m_strName;
	std::string m_strPath;
	int m_iBuildIndex = -1;
	int m_iHandle = -1;
	uint32_t m_uGeneration = 0;  // Generation counter for stale handle detection
#ifdef ZENITH_TOOLS
	bool m_bHasUnsavedChanges = false;
#endif
	bool m_bIsLoaded = false;
	bool m_bIsActivated = true;  // False during async load until Awake/OnEnable complete (Unity: scene.isLoaded is false until activated)
	bool m_bWasLoadedAdditively = false;
	bool m_bIsPaused = false;  // When true, Update is skipped for this scene
	bool m_bIsUnloading = false;  // True during async unload - scene partially destroyed
	uint64_t m_ulLoadTimestamp = 0;  // For selecting most recently loaded scene when active is unloaded

	//==========================================================================
	// Lifecycle Tracking (uses global slot flags)
	//==========================================================================

	void MarkEntityAwoken(Zenith_EntityID xID)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkEntityAwoken must be called from main thread");
		s_axEntitySlots.Get(xID.m_uIndex).m_bAwoken = true;
	}
	void MarkEntityStarted(Zenith_EntityID xID)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkEntityStarted must be called from main thread");
		s_axEntitySlots.Get(xID.m_uIndex).m_bStarted = true;
	}
	void MarkEntityPendingStart(Zenith_EntityID xID)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkEntityPendingStart must be called from main thread");
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
		if (!xSlot.m_bPendingStart) { xSlot.m_bPendingStart = true; m_uPendingStartCount++; m_axPendingStartEntities.PushBack(xID); }
	}
	bool HasPendingStarts() const { return m_uPendingStartCount > 0; }
	bool IsEntityAwoken(Zenith_EntityID xID) const { return xID.m_uIndex < s_axEntitySlots.GetSize() && s_axEntitySlots.Get(xID.m_uIndex).m_bAwoken; }
	bool IsEntityStarted(Zenith_EntityID xID) const { return xID.m_uIndex < s_axEntitySlots.GetSize() && s_axEntitySlots.Get(xID.m_uIndex).m_bStarted; }
	bool IsOnEnableDispatched(Zenith_EntityID xID) const { return xID.m_uIndex < s_axEntitySlots.GetSize() && s_axEntitySlots.Get(xID.m_uIndex).m_bOnEnableDispatched; }
	void SetOnEnableDispatched(Zenith_EntityID xID, bool bDispatched) { Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetOnEnableDispatched must be called from main thread"); s_axEntitySlots.Get(xID.m_uIndex).m_bOnEnableDispatched = bDispatched; }

	// Invalidate cached activeInHierarchy for an entity and all descendants
	static void InvalidateActiveInHierarchyCache(Zenith_EntityID xID);

	bool IsUpdating() const { return m_bIsUpdating; }
	void RegisterCreatedDuringUpdate(Zenith_EntityID xID)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RegisterCreatedDuringUpdate must be called from main thread");
		if (m_bIsUpdating)
		{
			s_axEntitySlots.Get(xID.m_uIndex).m_bCreatedDuringUpdate = true;
		}
	}
	bool WasCreatedDuringUpdate(Zenith_EntityID xID) const { return xID.m_uIndex < s_axEntitySlots.GetSize() && s_axEntitySlots.Get(xID.m_uIndex).m_bCreatedDuringUpdate; }

	//==========================================================================
	// Immediate Lifecycle Dispatch (Unity parity: Awake/OnEnable fire immediately on Instantiate)
	//==========================================================================

	void DispatchImmediateLifecycleForRuntime(Zenith_EntityID xID);

	//==========================================================================
	// Deferred Destruction
	//==========================================================================

	void MarkForDestruction(Zenith_EntityID xID);
	void MarkChildrenForDestructionRecursive(Zenith_EntityID xID);
	void MarkForTimedDestruction(Zenith_EntityID xID, float fDelay);
	bool IsMarkedForDestruction(Zenith_EntityID xID) const;
	void ProcessPendingDestructions();

	//==========================================================================
	// Internal State Queries
	//==========================================================================

	bool IsBeingDestroyed() const { return m_bIsBeingDestroyed; }
	void SetPaused(bool bPaused) { Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetPaused must be called from main thread"); m_bIsPaused = bPaused; }
	void InvalidateRootEntityCache() { Zenith_Assert(Zenith_Multithreading::IsMainThread(), "InvalidateRootEntityCache must be called from main thread"); m_bRootEntitiesDirty = true; }

	//==========================================================================
	// Update (called by SceneManager)
	//==========================================================================

	void Update(float fDt);
	void FixedUpdate(float fFixedDt);

	/**
	 * Dispatch OnAwake and OnEnable for newly loaded scene.
	 * OnStart is deferred to first Update() (Unity behavior).
	 */
	void DispatchLifecycleForNewScene();

	/**
	 * Split lifecycle phases for correct sceneLoaded callback timing.
	 * Unity fires sceneLoaded after Awake() and OnEnable() but before Start().
	 * Use DispatchAwakeForNewScene(), DispatchEnableAndPendingStartsForNewScene(), then fire sceneLoaded.
	 */
	void DispatchAwakeForNewScene();
	void DispatchEnableAndPendingStartsForNewScene();

	/**
	 * Dispatch pending OnStart calls (called at start of Update).
	 * Unity behavior: Start() runs on first frame after scene load.
	 */
	void DispatchPendingStarts();

	//==========================================================================
	// Internal Component Transfer
	//==========================================================================

	// Transfer a single component type from source scene's pool to target scene's pool
	// Used by MoveEntityInternal for zero-copy cross-scene entity transfer
	template<typename T>
	static void TransferComponent(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget);

	//==========================================================================
	// Internal Helpers
	//==========================================================================

	// Collect entity and all descendants depth-first (children before parent)
	void CollectHierarchyDepthFirst(Zenith_EntityID xID, Zenith_Vector<Zenith_EntityID>& axOut);

	// Shared helper: dispatch OnAwake for a single entity if not already awoken
	void DispatchAwakeForEntity(Zenith_EntityID xEntityID);

	template<typename T>
	T& GetComponentFromPool(u_int uIndex) const
	{
		const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
		Zenith_ComponentPool<T>* pxPool = static_cast<Zenith_ComponentPool<T>*>(m_xComponents.Get(uTypeID));
		return pxPool->Get(uIndex);
	}

	template<typename T>
	Zenith_ComponentPool<T>* GetComponentPool() const
	{
		const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
		Zenith_Assert(uTypeID < m_xComponents.GetSize(), "GetComponentPool: Component type not registered");
		Zenith_ComponentPoolBase* pxPoolBase = m_xComponents.Get(uTypeID);
		Zenith_Assert(pxPoolBase != nullptr, "GetComponentPool: Component pool does not exist");
		return static_cast<Zenith_ComponentPool<T>*>(pxPoolBase);
	}

	template<typename T>
	Zenith_ComponentPool<T>* GetOrCreateComponentPool()
	{
		const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
		while (m_xComponents.GetSize() <= uTypeID)
		{
			m_xComponents.PushBack(nullptr);
		}
		Zenith_ComponentPoolBase*& pxPoolBase = static_cast<Zenith_ComponentPoolBase*&>(m_xComponents.Get(uTypeID));
		if (pxPoolBase == nullptr)
		{
			pxPoolBase = new Zenith_ComponentPool<T>;
		}
		return static_cast<Zenith_ComponentPool<T>*>(pxPoolBase);
	}

	//==========================================================================
	// Per-Scene Data
	//==========================================================================

	// Per-scene entity tracking
	Zenith_Vector<Zenith_EntityID> m_xActiveEntities;
	Zenith_Vector<Zenith_EntityID> m_axNewlyCreatedEntities;  // Entities awaiting Awake (cleared each Update)
	Zenith_Vector<Zenith_EntityID> m_axPendingStartEntities; // Entities awaiting Start (avoids O(N) scan in DispatchPendingStarts)
	u_int m_uPendingStartCount = 0;
	Zenith_EntityID m_xMainCameraEntity = INVALID_ENTITY_ID;

	// Deferred destruction (list of IDs, slot flag used for duplicate check)
	Zenith_Vector<Zenith_EntityID> m_xPendingDestruction;

	// Timed destruction (Unity Destroy(obj, delay) parity)
	struct TimedDestruction
	{
		Zenith_EntityID m_xEntityID;
		float m_fTimeRemaining;
	};
	Zenith_Vector<TimedDestruction> m_axTimedDestructions;

	// Update tracking
	bool m_bIsUpdating = false;
	bool m_bIsBeingDestroyed = false;

	// Root entity cache for O(1) count access
	Zenith_Vector<Zenith_EntityID> m_axCachedRootEntities;
	bool m_bRootEntitiesDirty = true;
	void RebuildRootEntityCache();

	// Component pools (per-scene)
	Zenith_Vector<Zenith_ComponentPoolBase*> m_xComponents;
};

// Include SceneManager after class definition for template implementations
// that need AreRenderTasksActive(). This is safe because SceneManager.h only
// forward-declares SceneData (no circular dependency).
#include "EntityComponent/Zenith_SceneManager.h"

//==============================================================================
// Template Implementations
// These must be in the header so they're visible to all translation units
//==============================================================================

template<typename T, typename... Args>
T& Zenith_SceneData::CreateComponent(Zenith_EntityID xID, Args&&... args)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateComponent must be called from main thread");
	Zenith_Assert(EntityExists(xID), "CreateComponent: Entity (idx=%u, gen=%u) does not exist", xID.m_uIndex, xID.m_uGeneration);

	Zenith_ComponentPool<T>* pxPool = GetOrCreateComponentPool<T>();
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();

	u_int uComponentIndex;
	if (pxPool->m_xFreeIndices.GetSize() > 0)
	{
		uComponentIndex = pxPool->m_xFreeIndices.GetBack();

		// Check for generation overflow (matching entity slot overflow handling)
		if (pxPool->m_xGenerations.Get(uComponentIndex) == UINT32_MAX)
		{
			static uint32_t ls_uRetiredSlotCount = 0;
			ls_uRetiredSlotCount++;
			Zenith_Warning(LOG_CATEGORY_ECS,
				"Component slot %u generation overflow - retiring slot (total retired: %u). "
				"Consider restarting if memory is a concern.", uComponentIndex, ls_uRetiredSlotCount);
			pxPool->m_xFreeIndices.PopBack();
			// Allocate a fresh slot instead
			uComponentIndex = pxPool->EmplaceBack(xID, std::forward<Args>(args)...);
		}
		else
		{
			pxPool->m_xFreeIndices.PopBack();
			pxPool->ConstructAt(uComponentIndex, xID, std::forward<Args>(args)...);
		}
	}
	else
	{
		uComponentIndex = pxPool->EmplaceBack(xID, std::forward<Args>(args)...);
	}

	s_axEntityComponents.Get(xID.m_uIndex)[uTypeID] = uComponentIndex;
	MarkDirty();
	return pxPool->Get(uComponentIndex);
}

template<typename T>
bool Zenith_SceneData::EntityHasComponent(Zenith_EntityID xID) const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive()
		,
		"EntityHasComponent must be called from main thread");
	if (!EntityExists(xID)) return false;
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	const auto& xMap = s_axEntityComponents.Get(xID.m_uIndex);
	return xMap.contains(uTypeID);
}

template<typename T>
T& Zenith_SceneData::GetComponentFromEntity(Zenith_EntityID xID) const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"GetComponentFromEntity must be called from main thread");
	Zenith_Assert(EntityExists(xID), "GetComponentFromEntity: Entity (idx=%u, gen=%u) does not exist", xID.m_uIndex, xID.m_uGeneration);

	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	const auto& xMap = s_axEntityComponents.Get(xID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	Zenith_Assert(xIt != xMap.end(), "GetComponentFromEntity: Entity does not have component");
	const u_int uIndex = xIt->second;
	return GetComponentPool<T>()->Get(uIndex);
}

template<typename T>
bool Zenith_SceneData::RemoveComponentFromEntity(Zenith_EntityID xID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RemoveComponentFromEntity must be called from main thread");
	if (!EntityExists(xID)) return false;

	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	auto& xMap = s_axEntityComponents.Get(xID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	if (xIt == xMap.end()) return false;

	u_int uComponentIndex = xIt->second;
	Zenith_ComponentPool<T>* pxPool = GetOrCreateComponentPool<T>();

	// Call OnRemove lifecycle if the component type has it (C++20 requires clause)
	if constexpr (requires(T& t) { t.OnRemove(); })
	{
		pxPool->Get(uComponentIndex).OnRemove();
	}

	// Destruct and mark slot as free
	pxPool->DestructAt(uComponentIndex);
	pxPool->m_xFreeIndices.PushBack(uComponentIndex);

	xMap.erase(xIt);
	MarkDirty();
	return true;
}

template<typename T>
void Zenith_SceneData::GetAllOfComponentType(Zenith_Vector<T*>& xOut) const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "GetAllOfComponentType must be called from main thread");
	xOut.Clear();
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	if (uTypeID >= m_xComponents.GetSize()) return;

	Zenith_ComponentPoolBase* pxPoolBase = m_xComponents.Get(uTypeID);
	if (pxPoolBase == nullptr) return;

	Zenith_ComponentPool<T>* pxPool = static_cast<Zenith_ComponentPool<T>*>(pxPoolBase);
	for (u_int u = 0; u < pxPool->GetSize(); ++u)
	{
		if (pxPool->m_xOwningEntities.Get(u).IsValid())
		{
			xOut.PushBack(&pxPool->Get(u));
		}
	}
}

template<typename T>
bool Zenith_SceneData::IsComponentHandleValid(const Zenith_ComponentHandle<T>& xHandle) const
{
	if (!xHandle.IsValid()) return false;
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	if (uTypeID >= m_xComponents.GetSize()) return false;

	Zenith_ComponentPoolBase* pxPoolBase = m_xComponents.Get(uTypeID);
	if (pxPoolBase == nullptr) return false;

	Zenith_ComponentPool<T>* pxPool = static_cast<Zenith_ComponentPool<T>*>(pxPoolBase);
	if (xHandle.m_uIndex >= pxPool->m_xGenerations.GetSize()) return false;
	return pxPool->m_xGenerations.Get(xHandle.m_uIndex) == xHandle.m_uGeneration &&
	       pxPool->m_xOwningEntities.Get(xHandle.m_uIndex).IsValid();
}

template<typename T>
T* Zenith_SceneData::TryGetComponentFromHandle(const Zenith_ComponentHandle<T>& xHandle) const
{
	if (!IsComponentHandleValid(xHandle)) return nullptr;
	return &GetComponentPool<T>()->Get(xHandle.m_uIndex);
}

template<typename T>
Zenith_ComponentHandle<T> Zenith_SceneData::GetComponentHandle(Zenith_EntityID xID) const
{
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	const auto& xMap = s_axEntityComponents.Get(xID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	if (xIt == xMap.end()) return Zenith_ComponentHandle<T>::Invalid();
	const u_int uIndex = xIt->second;
	const uint32_t uGeneration = GetComponentPool<T>()->m_xGenerations.Get(uIndex);

	return { uIndex, uGeneration };
}

template<typename T>
void Zenith_SceneData::TransferComponent(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget)
{
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	auto& xMap = s_axEntityComponents.Get(xEntityID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	if (xIt == xMap.end()) return;

	u_int uSourcePoolIndex = xIt->second;

	Zenith_ComponentPool<T>* pxSourcePool = pxSource->GetComponentPool<T>();
	Zenith_ComponentPool<T>* pxTargetPool = pxTarget->GetOrCreateComponentPool<T>();

	T& xSourceComponent = pxSourcePool->Get(uSourcePoolIndex);
	u_int uNewPoolIndex = pxTargetPool->MoveEmplaceBack(xEntityID, std::move(xSourceComponent));

	// Destruct in source and free slot
	pxSourcePool->DestructAt(uSourcePoolIndex);
	pxSourcePool->m_xFreeIndices.PushBack(uSourcePoolIndex);

	// Update global mapping to point to target pool index
	xMap[uTypeID] = uNewPoolIndex;
}

// Restore zone state
#ifndef ZENITH_SCENEDATA_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_SCENEDATA_ZONE_WAS_SET
#include "Memory/Zenith_MemoryManagement_Enabled.h"
