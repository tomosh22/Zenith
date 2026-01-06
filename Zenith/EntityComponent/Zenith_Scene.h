#pragma once

#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <atomic>
#include <mutex>
#include <unordered_set>

class Zenith_CameraComponent;

// Forward declare Zenith_Scene so Zenith_Entity can reference it
class Zenith_Scene;

// Include Zenith_Entity to get complete EntityID type definition (must come before ComponentPool)
#include "EntityComponent/Zenith_Entity.h"

class Zenith_ComponentPoolBase
{
public:
	virtual ~Zenith_ComponentPoolBase() = default;
};

template<typename T>
class Zenith_ComponentPool : public Zenith_ComponentPoolBase
{
public:
	Zenith_Vector<T> m_xData;
	Zenith_Vector<Zenith_EntityID> m_xOwningEntities;  // Parallel array tracking which entity owns each component
};

// Define the component concept after Zenith_Entity is fully defined
template<typename T>
concept Zenith_Component =
// Component must be constructible from an entity reference
// This matches the existing pattern where components store their parent entity
std::is_constructible_v<T, Zenith_Entity&>&&
// Component must be destructible
std::is_destructible_v<T>
	#ifdef ZENITH_TOOLS
	&&
// Component must have a RenderPropertiesPanel method for editor UI
// This method is responsible for rendering the component's properties in ImGui
	requires(T& t) { { t.RenderPropertiesPanel() } -> std::same_as<void>; }
	#endif
	;

// Forward declaration for query system
template<typename... Ts> class Zenith_Query;

class Zenith_Scene
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
			// Editor registration is now handled by ZENITH_REGISTER_COMPONENT macro
			// which registers with both ComponentMeta and ComponentRegistry
			return ls_uRet;
		}
	private:
		inline static TypeID s_uCounter = 0;
	};

	//--------------------------------------------------------------------------
	// Slot-based Entity Storage (Generation Counter System)
	// Entity state is stored HERE (single source of truth).
	// Zenith_Entity is now a lightweight handle that delegates to this slot.
	//--------------------------------------------------------------------------
	struct Zenith_EntitySlot
	{
		// Entity state (single source of truth)
		std::string m_strName;
		bool m_bEnabled = true;
		bool m_bTransient = true;  // Default: transient (not saved)

		// Slot metadata
		uint32_t m_uGeneration = 0;
		bool m_bOccupied = false;
		bool m_bMarkedForDestruction = false;
	};

	Zenith_Scene();
	~Zenith_Scene();
	void Reset();

	void AcquireMutex()
	{
		m_xMutex.Lock();
	}
	void ReleaseMutex()
	{
		m_xMutex.Unlock();
	}

	Zenith_EntityID CreateEntity()
	{
		uint32_t uIndex;
		uint32_t uGeneration;

		if (m_xFreeIndices.GetSize() > 0)
		{
			// Reuse a slot from free list
			uIndex = m_xFreeIndices.GetBack();
			m_xFreeIndices.PopBack();

			Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(uIndex);
			xSlot.m_uGeneration++;  // Increment to invalidate old references
			uGeneration = xSlot.m_uGeneration;
			xSlot.m_bOccupied = true;
			xSlot.m_bMarkedForDestruction = false;
		}
		else
		{
			// Allocate new slot
			uIndex = m_xEntitySlots.GetSize();
			uGeneration = 1;  // Start at 1 (generation 0 is always invalid)

			Zenith_EntitySlot xNewSlot;
			xNewSlot.m_uGeneration = uGeneration;
			xNewSlot.m_bOccupied = true;
			xNewSlot.m_bMarkedForDestruction = false;
			m_xEntitySlots.PushBack(std::move(xNewSlot));
		}

		// Ensure component mapping has space for this index
		while (m_xEntityComponents.GetSize() <= uIndex)
		{
			m_xEntityComponents.PushBack({});
		}

		Zenith_EntityID xNewID = { uIndex, uGeneration };
		m_xActiveEntities.PushBack(xNewID);
		return xNewID;
	}

	template<typename T, typename... Args>
	T& CreateComponent(Zenith_EntityID xID, Args&&... args)
	{
		Zenith_Assert(EntityExists(xID), "CreateComponent: Entity (idx=%u, gen=%u) is stale or invalid", xID.m_uIndex, xID.m_uGeneration);

		Zenith_ComponentPool<T>* const pxPool = GetComponentPool<T>();
		u_int uIndex = pxPool->m_xData.GetSize();
		pxPool->m_xData.EmplaceBack(std::forward<Args>(args)...);
		pxPool->m_xOwningEntities.PushBack(xID);  // Track which entity owns this component

		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(xID.m_uIndex);
		Zenith_Assert(xComponentsForThisEntity.find(uTypeID) == xComponentsForThisEntity.end(), "This component already has this entity");

		xComponentsForThisEntity[uTypeID] = uIndex;

		T& xRet = GetComponentFromPool<T>(uIndex);
		return xRet;
	}

	template<typename T>
	bool EntityHasComponent(Zenith_EntityID xID) const
	{
		if (!EntityExists(xID)) return false;
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		const std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(xID.m_uIndex);
		return xComponentsForThisEntity.contains(uTypeID);
	}

	template<typename T>
	T& GetComponentFromEntity(Zenith_EntityID xID)
	{
		Zenith_Assert(EntityExists(xID), "GetComponentFromEntity: Entity (idx=%u, gen=%u) is stale or invalid", xID.m_uIndex, xID.m_uGeneration);
		Zenith_Assert(EntityHasComponent<T>(xID), "GetComponentFromEntity: Entity %u does not have requested component type", xID.m_uIndex);
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(xID.m_uIndex);
		const u_int uIndex = xComponentsForThisEntity.at(uTypeID);
		return GetComponentPool<T>()->m_xData.Get(uIndex);
	}

	template<typename T>
	bool RemoveComponentFromEntity(Zenith_EntityID xID)
	{
		Zenith_Assert(EntityExists(xID), "RemoveComponentFromEntity: Entity (idx=%u, gen=%u) is stale or invalid", xID.m_uIndex, xID.m_uGeneration);
		Zenith_Assert(EntityHasComponent<T>(xID), "RemoveComponentFromEntity: Entity %u does not have component to remove", xID.m_uIndex);
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(xID.m_uIndex);
		const u_int uRemovedIndex = xComponentsForThisEntity.at(uTypeID);
		xComponentsForThisEntity.erase(uTypeID);

		Zenith_ComponentPool<T>* pxPool = GetComponentPool<T>();
		const u_int uLastIndex = pxPool->m_xData.GetSize() - 1;

		if (uRemovedIndex != uLastIndex)
		{
			// Swap with last element using move semantics
			pxPool->m_xData.Get(uRemovedIndex) = std::move(pxPool->m_xData.Get(uLastIndex));

			// Update the entity that owned the moved component
			Zenith_EntityID xMovedEntityID = pxPool->m_xOwningEntities.Get(uLastIndex);
			Zenith_Assert(EntityExists(xMovedEntityID), "RemoveComponentFromEntity: Moved entity is stale");
			m_xEntityComponents.Get(xMovedEntityID.m_uIndex)[uTypeID] = uRemovedIndex;
			pxPool->m_xOwningEntities.Get(uRemovedIndex) = xMovedEntityID;
		}

		pxPool->m_xData.PopBack();
		pxPool->m_xOwningEntities.PopBack();
		return true;
	}

	template<typename T>
	void GetAllOfComponentType(Zenith_Vector<T*>& xOut)
	{
		for (typename Zenith_Vector<T>::Iterator xIt(GetComponentPool<T>()->m_xData); !xIt.Done(); xIt.Next())
		{
			xOut.PushBack(&xIt.GetData());
		}
	}

	// Multi-component query - returns a query object for fluent iteration
	// Usage: scene.Query<ComponentA, ComponentB>().ForEach([](Zenith_EntityID, ComponentA&, ComponentB&) { ... });
	// Implementation in Zenith_Query.h (must include after this header)
	template<typename... Ts>
	Zenith_Query<Ts...> Query();

	// Serialization methods
	// bIncludeTransient: when true, saves ALL entities (including transient ones).
	// Use true for editor backup (Play/Stop), false for normal scene saving.
	void SaveToFile(const std::string& strFilename, bool bIncludeTransient = false);
	void LoadFromFile(const std::string& strFilename);

	// Entity management
	void RemoveEntity(Zenith_EntityID uID);

	//--------------------------------------------------------------------------
	// Instantiate (runtime entity creation)
	//--------------------------------------------------------------------------

	/**
	 * Instantiate a prefab. The resulting entity is transient (not saved with scene).
	 */
	static Zenith_Entity Instantiate(const class Zenith_Prefab& xPrefab, const std::string& strName = "");

	/**
	 * Instantiate a prefab with position. The resulting entity is transient.
	 */
	static Zenith_Entity Instantiate(const class Zenith_Prefab& xPrefab, const Zenith_Maths::Vector3& xPosition, const std::string& strName = "");

	/**
	 * Instantiate a prefab with position and rotation. The resulting entity is transient.
	 */
	static Zenith_Entity Instantiate(const class Zenith_Prefab& xPrefab, const Zenith_Maths::Vector3& xPosition, const Zenith_Maths::Quat& xRotation, const std::string& strName = "");

	/**
	 * Destroy an entity (Unity-style deferred destruction at end of frame).
	 * Children are also marked for destruction.
	 */
	static void Destroy(Zenith_Entity& xEntity);
	static void Destroy(Zenith_EntityID uEntityID);

	/**
	 * Immediately destroy an entity (current-frame destruction for editor/tests).
	 */
	static void DestroyImmediate(Zenith_Entity& xEntity);
	static void DestroyImmediate(Zenith_EntityID uEntityID);

	/**
	 * Check if an entity is marked for destruction.
	 */
	bool IsMarkedForDestruction(Zenith_EntityID uEntityID) const;

	/**
	 * Process pending destructions (called at end of Update).
	 */
	void ProcessPendingDestructions();

	/**
	 * Check if currently in Update loop (for deferred creation tracking).
	 */
	bool IsUpdating() const { return m_bIsUpdating; }

	/**
	 * Register an entity as created during Update (won't receive callbacks this frame).
	 */
	void RegisterCreatedDuringUpdate(Zenith_EntityID uID) { if (m_bIsUpdating) m_xCreatedDuringUpdate.insert(uID); }

	/**
	 * Check if entity was created during current Update frame.
	 */
	bool WasCreatedDuringUpdate(Zenith_EntityID uID) const { return m_xCreatedDuringUpdate.count(uID) > 0; }

	// Query methods
	u_int GetEntityCount() const { return m_xActiveEntities.GetSize(); }
	const Zenith_Vector<Zenith_EntityID>& GetActiveEntities() const { return m_xActiveEntities; }

	// Check if entity exists AND is valid (generation matches)
	bool EntityExists(Zenith_EntityID xID) const
	{
		if (!xID.IsValid()) return false;
		if (xID.m_uIndex >= m_xEntitySlots.GetSize()) return false;
		const Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(xID.m_uIndex);
		return xSlot.m_bOccupied && xSlot.m_uGeneration == xID.m_uGeneration;
	}

	// Create a lightweight entity handle from an ID
	// Note: The returned Zenith_Entity is a handle, not a reference to stored data
	Zenith_Entity GetEntity(Zenith_EntityID xID);

	// Try to get an entity - returns nullptr if entity doesn't exist
	// Note: Returns pointer to static entity handle, valid until next TryGetEntity call
	Zenith_Entity* TryGetEntity(Zenith_EntityID xID)
	{
		if (!EntityExists(xID))
		{
			return nullptr;
		}
		static Zenith_Entity s_xTempEntity;
		s_xTempEntity = Zenith_Entity(this, xID);
		return &s_xTempEntity;
	}

	// Legacy compatibility - same as GetEntity but with old name
	Zenith_Entity GetEntityFromID(Zenith_EntityID xID) { return GetEntity(xID); }

	// Find entity by name - returns invalid entity if not found
	Zenith_Entity FindEntityByName(const std::string& strName);

	// Direct slot access for internal use and serialization
	Zenith_EntitySlot& GetSlot(Zenith_EntityID xID)
	{
		Zenith_Assert(EntityExists(xID), "GetSlot: Entity (idx=%u, gen=%u) is invalid", xID.m_uIndex, xID.m_uGeneration);
		return m_xEntitySlots.Get(xID.m_uIndex);
	}

	const Zenith_EntitySlot& GetSlot(Zenith_EntityID xID) const
	{
		Zenith_Assert(EntityExists(xID), "GetSlot: Entity (idx=%u, gen=%u) is invalid", xID.m_uIndex, xID.m_uGeneration);
		return m_xEntitySlots.Get(xID.m_uIndex);
	}

	static void Update(const float fDt);
	static void WaitForUpdateComplete();

	// Legacy alias - use GetEntity instead
	Zenith_Entity GetEntityByID(Zenith_EntityID xID) { return GetEntity(xID); }

	static Zenith_Scene& GetCurrentScene() { return s_xCurrentScene; }

	void SetMainCameraEntity(Zenith_EntityID uEntity);
	Zenith_EntityID GetMainCameraEntity();
	Zenith_CameraComponent& GetMainCamera();
	Zenith_CameraComponent* TryGetMainCamera();  // Safe version - returns nullptr if no valid camera

	// Scene loading state (prevents asset deletion during Reset())
	static bool IsLoadingScene() { return s_bIsLoadingScene; }

	// Prefab instantiation state (allows entity creation during prefab instantiate)
	static void SetPrefabInstantiating(bool b) { s_bIsPrefabInstantiating = b; }

	/**
	 * DispatchFullLifecycleInit - Safely dispatch OnAwake, OnEnable, OnStart to all entities
	 *
	 * CRITICAL: This method handles entity creation during callbacks safely by:
	 * 1. Copying entity IDs before iteration (prevents vector reference invalidation)
	 * 2. Using separate loops for each lifecycle stage (prevents dangling entity references)
	 * 3. Re-fetching entity references before each callback
	 *
	 * Use this instead of manually iterating and dispatching lifecycle callbacks.
	 * Called by: Editor (after backup restore), and any code needing full entity init.
	 */
	static void DispatchFullLifecycleInit();

private:
	static bool s_bIsLoadingScene;
	static bool s_bIsPrefabInstantiating;
	friend class Zenith_Entity;
	template<typename... Ts> friend class Zenith_Query;
#ifdef ZENITH_TOOLS
	friend class Zenith_Editor;
	friend class Zenith_SelectionSystem;
#endif

	template<typename T>
	T& GetComponentFromPool(u_int uIndex)
	{
		const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
		Zenith_ComponentPool<T>* pxPool = static_cast<Zenith_ComponentPool<T>*>(m_xComponents.Get(uTypeID));
		return pxPool->m_xData.Get(uIndex);
	}

	template<typename T>
	Zenith_ComponentPool<T>* GetComponentPool()
	{
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
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

	Zenith_Vector<Zenith_EntitySlot> m_xEntitySlots;      // Dense array of entity slots
	Zenith_Vector<uint32_t> m_xFreeIndices;               // Free list for slot recycling
	Zenith_Vector<Zenith_EntityID> m_xActiveEntities;     // Active entity IDs for iteration
	std::unordered_set<Zenith_EntityID> m_xEntitiesStarted;  // Tracks which entities have had OnStart called
	static Zenith_Scene s_xCurrentScene;
	static float s_fFixedTimeAccumulator;  // Accumulator for fixed timestep updates
	Zenith_EntityID m_xMainCameraEntity = INVALID_ENTITY_ID;
	Zenith_Mutex m_xMutex;

	// Deferred destruction tracking (Unity-style)
	Zenith_Vector<Zenith_EntityID> m_xPendingDestruction;
	std::unordered_set<Zenith_EntityID> m_xPendingDestructionSet;

	// Deferred creation tracking - entities created during Update() are tracked
	// so they don't receive callbacks (OnStart, OnUpdate, etc.) until next frame
	bool m_bIsUpdating = false;
	std::unordered_set<Zenith_EntityID> m_xCreatedDuringUpdate;

	public:
	//#TO type id is index into vector
	Zenith_Vector<Zenith_ComponentPoolBase*> m_xComponents;

	//#TO EntityID is index into vector
	Zenith_Vector<std::unordered_map<Zenith_Scene::TypeID, u_int>> m_xEntityComponents;
};

// Zenith_Entity template implementations (placed here after Zenith_Scene is fully defined)
template<typename T, typename... Args>
T& Zenith_Entity::AddComponent(Args&&... args)
{
	Zenith_Assert(m_pxParentScene != nullptr, "AddComponent: Entity has no scene");
	Zenith_Assert(m_pxParentScene->EntityExists(m_xEntityID), "AddComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_Assert(!HasComponent<T>(), "AddComponent: Entity already has this component type");
	return m_pxParentScene->CreateComponent<T>(m_xEntityID, std::forward<Args>(args)..., *this);
}

template<typename T, typename... Args>
T& Zenith_Entity::AddOrReplaceComponent(Args&&... args)
{
	if (HasComponent<T>())
	{
		RemoveComponent<T>();
	}
	return AddComponent<T>(args);
}

template<typename T>
bool Zenith_Entity::HasComponent() const
{
	Zenith_Assert(m_pxParentScene != nullptr, "HasComponent: Entity has no scene");
	return m_pxParentScene->EntityHasComponent<T>(m_xEntityID);
}

template<typename T>
T& Zenith_Entity::GetComponent() const
{
	Zenith_Assert(m_pxParentScene != nullptr, "GetComponent: Entity has no scene");
	Zenith_Assert(m_pxParentScene->EntityExists(m_xEntityID), "GetComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_Assert(HasComponent<T>(), "GetComponent: Entity does not have this component type");
	return m_pxParentScene->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
void Zenith_Entity::RemoveComponent()
{
	Zenith_Assert(m_pxParentScene != nullptr, "RemoveComponent: Entity has no scene");
	Zenith_Assert(m_pxParentScene->EntityExists(m_xEntityID), "RemoveComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_Assert(HasComponent<T>(), "RemoveComponent: Entity does not have this component type");
	m_pxParentScene->RemoveComponentFromEntity<T>(m_xEntityID);
}
