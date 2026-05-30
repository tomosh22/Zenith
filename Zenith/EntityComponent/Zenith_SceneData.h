#pragma once

// Placement-new scope guard: Zenith_Vector and other containers use placement new internally,
// which conflicts with the engine's memory management overrides of global operator new/delete.
// This pattern saves whether the zone was already active, defines it if not, disables memory
// management overrides for this header, and restores the zone state at the bottom of the file.
// See the matching #undef block at the end of this header.
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

// Phase 5a: Zenith_EntitySlot + the three "global entity storage" arrays
// live here. SceneData's inline methods reach the arrays through
// g_xEngine.EntityStore().m_axXxx -- requires Zenith_Engine.h to be included
// (transitively via Zenith.h PCH, which every .cpp pulls in first).
#include "EntityComponent/Zenith_EntityStore.h"
#include "Core/Zenith_Engine.h"

// Pool types + Zenith_Component concept moved out to break the historical
// SceneData.h ↔ SceneManager.h cycle (T2.4). The new header carries its own
// placement-new guard and is fully self-contained.
#include "EntityComponent/Zenith_ComponentPool.h"

// Free-function form of g_xEngine.Scenes().AreRenderTasksActive(). Used in
// SceneData.h's template assertion bodies so we don't have to drag the full
// SceneManager.h include in (closing the cycle).
#include "EntityComponent/Zenith_RenderTaskState.h"

// Forward declaration for query system
template<typename... Ts> class Zenith_Query;

/**
 * Zenith_SceneData - Internal scene storage class
 *
 * This class holds the actual scene data (entity pools, components, etc.)
 * It was previously the internals of Zenith_Scene before the multi-scene refactor.
 *
 * Zenith_Scene is now a lightweight handle; this class holds the real data.
 * Managed by Zenith_SceneSystem (g_xEngine.Scenes()).
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

	// Destroys every entity + component pool in the scene but leaves scene metadata
	// (name, path, build index, isLoaded, isActivated, wasLoadedAdditively, unsaved-changes
	// flag) untouched. The safe default — use this when you want a clean scene
	// that stays in the registry as a loaded scene.
	void Reset();

	// Destroys entities AND scrubs all scene metadata (name/path/buildIndex,
	// m_bIsLoaded, etc.). Refuses to run on the persistent scene. Intended for
	// tests that explicitly inspect the scrubbed state + the SceneData destructor.
	void ScrubAndReset();

	//==========================================================================
	// Read-Only Scene Properties
	//==========================================================================

	const std::string& GetName() const { return m_strName; }
	const std::string& GetPath() const { return m_strPath; }
	int GetBuildIndex() const { return m_iBuildIndex; }
	int GetHandle() const { return m_iHandle; }

#ifdef ZENITH_TOOLS
	// Editor-only hooks used by the play-mode backup/restore path
	// (Zenith_Editor::EnterStopMode). The editor restores the original scene
	// path / build-index after running the scene in play mode, and re-marks
	// entities as started when re-entering play mode from a backup.
	// Exposed publicly so Zenith_Editor (now a namespace) doesn't need the
	// stale `friend class Zenith_Editor` declaration.
	void Editor_SetPath(const std::string& strPath) { m_strPath = strPath; }
	void Editor_SetBuildIndex(int iBuildIndex) { m_iBuildIndex = iBuildIndex; }
	void Editor_MarkEntityStarted(Zenith_EntityID xID);
#endif
	// Scene load state. The historical bool trio (m_bIsLoaded / m_bIsActivated /
	// m_bIsUnloading) was collapsed into one byte so the half-loaded scene that
	// caused the Play→Menu crash is now unrepresentable. Use the bool getters or
	// GetLoadState() to query, TransitionTo() to mutate.
	enum LoadState : uint8_t
	{
		SCENE_STATE_DESTROYED = 0,  // ScrubAndReset has run
		SCENE_STATE_LOADING,        // exists, lifecycle dispatch pending
		SCENE_STATE_LOADED,         // steady state
	};
	LoadState GetLoadState() const { return m_eLoadState; }
	bool IsLoaded()    const { return m_eLoadState != SCENE_STATE_DESTROYED; }
	bool IsActivated() const { return m_eLoadState == SCENE_STATE_LOADED; }
	// IsUnloading() preserved for API parity but always false — async unload was
	// retired with the rest of the async pipeline. Readers' branches were left
	// in place rather than churned because they cost nothing at runtime.
	bool IsUnloading() const { return false; }
	bool WasLoadedAdditively() const { return m_bWasLoadedAdditively; }
	bool IsPaused() const { return m_bIsPaused; }

	// Transition state with a legal-edge assert. Catches the persistent-scene
	// regression at the cause site rather than several frames downstream.
	void TransitionTo(LoadState eNew);

	//==========================================================================
	// Dirty Tracking (Editor)
	//==========================================================================

#ifdef ZENITH_TOOLS
	bool HasUnsavedChanges() const { return m_bHasUnsavedChanges; }
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

	// Thread-safe: only reads from g_xEngine.EntityStore().m_axEntitySlots which is stable during task execution
	// (main thread does not modify entity storage while worker threads are running)
	bool EntityExists(Zenith_EntityID xID) const
	{
		if (xID.m_uIndex == Zenith_EntityID::INVALID_INDEX) return false;
		if (xID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) return false;
		const Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		return xSlot.IsOccupied() && xSlot.m_uGeneration == xID.m_uGeneration;
	}

	Zenith_Entity GetEntity(Zenith_EntityID xID);
	Zenith_Entity TryGetEntity(Zenith_EntityID xID);
	Zenith_Entity FindEntityByName(const std::string& strName);

	//==========================================================================
	// Entity Count & Queries
	//==========================================================================

	u_int GetEntityCount() const
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetEntityCount must be called from main thread");
		return m_xActiveEntities.GetSize();
	}
	const Zenith_Vector<Zenith_EntityID>& GetActiveEntities() const
	{
		return m_xActiveEntities;
	}

	// Root entity cache for O(1) count access (Unity scene.rootCount parity)
	uint32_t GetCachedRootEntityCount();
	void GetCachedRootEntities(Zenith_Vector<Zenith_EntityID>& axOut);

	//==========================================================================
	// Lifecycle & State (engine-internal, not for game code)
	//==========================================================================

	bool IsBeingDestroyed() const { return m_bIsBeingDestroyed; }
	bool IsMarkedForDestruction(Zenith_EntityID xID) const;
	void InvalidateRootEntityCache() { Zenith_Assert(g_xEngine.Threading().IsMainThread(), "InvalidateRootEntityCache must be called from main thread"); m_bRootEntitiesDirty = true; }

	// Returns the scene handle that owns this entity (-1 if invalid)
	static int GetEntitySceneHandle(Zenith_EntityID xID)
	{
		if (xID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) return -1;
		return g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_iSceneHandle;
	}

	void MarkEntityAwoken(Zenith_EntityID xID)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MarkEntityAwoken must be called from main thread");
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (xSlot.IsAwoken()) return;  // Already awoken or further along - no-op
		xSlot.TransitionToAwoken();
	}

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

	// Append-variant of GetAllOfComponentType: does NOT clear xOut first. Used by
	// multi-scene iteration (GetAllOfComponentTypeFromAllScenes) to avoid a
	// per-scene temp vector + copy. Appends this scene's pool entries to xOut.
	template<typename T>
	void AppendAllOfComponentType(Zenith_Vector<T*>& xOut) const;

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

	// Scene file header constants — single source of truth for all call sites that
	// write, read, or validate a .zscen binary. Bumping the format version? Update
	// uSCENE_VERSION_CURRENT here and the call sites that reference it:
	//   SaveToFile, LoadFromDataStream.
	// All of them reference these constants — no magic numbers elsewhere.
	//
	// Version history (read/write asymmetry is intentional — see DeserializeEntityComponents):
	//   v3      legacy entity layout (explicit per-entity child-index list)
	//   v4/v5   compact entity layout [fileIndex][name]; per-component [typeName][size][payload]
	//   v6      adds an OPTIONAL per-component schemaVersion written OUTSIDE the
	//           size-prefixed payload: [typeName][schemaVersion u_int][size u_int][payload].
	//           INERT this wave — no component opts in yet (every component reports
	//           the default schema 1), but the field round-trips so a later component
	//           migration can branch on it. v3/4/5 files carry no schemaVersion bytes;
	//           the read path is gated on (uSceneVersion >= 6) so they stay byte-aligned.
	static constexpr u_int uSCENE_MAGIC                 = 0x5A53434E;
	static constexpr u_int uSCENE_VERSION_CURRENT       = 6;
	static constexpr u_int uSCENE_VERSION_MIN_SUPPORTED = 3;

	// Non-destructive header validator: saves/restores the stream cursor so the
	// caller's subsequent reads start from the same offset. Single source of truth
	// for the header checks (IsValid → size → magic → version) shared by the
	// LoadScene pre-pass and LoadFromDataStream. Static — operates only on the
	// passed stream + the static header constants above, touches no instance state.
	static bool ValidateSceneStream(Zenith_DataStream& xStream);

	void SaveToFile(const std::string& strFilename, bool bIncludeTransient = false);
	bool LoadFromFile(const std::string& strFilename);
	bool LoadFromDataStream(Zenith_DataStream& xStream);

private:
	//==========================================================================
	// Friend Declarations
	//==========================================================================

	friend class Zenith_Entity;
	friend class Zenith_SceneSystem;              // merged registry / operations / lifecycle / callbacks / entity-ownership all reach scene-data privates
	friend class Zenith_SceneTests;
	friend class Zenith_UnitTests;
	friend class Zenith_ComponentMetaRegistry;
	// WS10: the sparse-set query fast path reaches component pools directly
	// (TryGetComponentPool + the pool's m_xOwningEntities / GetSparseDense / Get).
	// Query already held a Zenith_SceneData* and used its public accessors; the
	// fast path additionally needs the (private) non-asserting pool fetch.
	template<typename... Ts> friend class Zenith_Query;
	// (Earlier revisions friended `class Zenith_Editor` here so editor code
	// could reach into scene-data privates. Audited stale -- editor accesses
	// scene data only through the public API -- and removed when
	// Zenith_Editor became a namespace.)

	//==========================================================================
	// Entity Slot Storage (Generation Counter System)
	//==========================================================================
	//
	// Entity Lifecycle State Machine (Unity parity):
	//
	//   FREE                                      -- slot unoccupied, available for reuse
	//     |  CreateEntity()
	//     v
	//   OCCUPIED                                   -- slot taken, entity exists but not yet awoken
	//     |  DispatchAwakeForEntity()              -- same frame as creation
	//     v
	//   AWOKEN  (+m_bOnEnableDispatched if active) -- Awake() called; OnEnable() if active in hierarchy
	//     |  QueuePendingStartsForNewEntities()
	//     v
	//   PENDING_START                              -- queued for Start() next frame
	//     |  DispatchPendingStarts() (next frame)
	//     v
	//   STARTED                                    -- Start() called; now receives Update/LateUpdate
	//     |  Destroy() or DestroyImmediate()
	//     v
	//   (m_bMarkedForDestruction set)              -- OnDisable+OnDestroy dispatched, then slot released
	//     |  ProcessPendingDestructions()
	//     v
	//   FREE                                      -- generation incremented, slot available
	//
	// m_bMarkedForDestruction is orthogonal to lifecycle state (can overlay any active state).
	//

	// Phase 5a: Zenith_EntitySlot, EntityLifecycleState, and the three global
	// entity-storage arrays moved off this class. Lifecycle state and slot type
	// now live at file scope in Zenith_EntityStore.h; the arrays moved onto
	// Zenith_EntityStore which Zenith_Engine owns. Back-compat using-aliases
	// below keep qualified names like Zenith_SceneData::Zenith_EntitySlot and
	// Zenith_SceneData::EntityLifecycleState::OCCUPIED resolving for the
	// existing 20-odd call sites; new code should use the unqualified file-
	// scope names. ResetGlobalEntityStorage is now Zenith_EntityStore::Reset().
	using Zenith_EntitySlot = ::Zenith_EntitySlot;
	using EntityLifecycleState = Zenith_EntityLifecycleState;

	const Zenith_EntitySlot& GetSlot(Zenith_EntityID xID) const;

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
	// Initial state matches the historical default (m_bIsLoaded=false,
	// m_bIsActivated=true) — the constructor body promotes to LOADED.
	LoadState m_eLoadState = SCENE_STATE_DESTROYED;
	bool m_bWasLoadedAdditively = false;
	bool m_bIsPaused = false;  // When true, Update is skipped for this scene
	uint64_t m_ulLoadTimestamp = 0;  // For selecting most recently loaded scene when active is unloaded

	//==========================================================================
	// Lifecycle Tracking (uses global slot flags)
	//==========================================================================

	void MarkEntityStarted(Zenith_EntityID xID)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MarkEntityStarted must be called from main thread");
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (xSlot.IsStarted()) return;  // Already started - no-op
		xSlot.TransitionToStarted();
	}
	void MarkEntityPendingStart(Zenith_EntityID xID)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MarkEntityPendingStart must be called from main thread");
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (!xSlot.IsPendingStart()) { xSlot.TransitionToPendingStart(); m_uPendingStartCount++; m_axPendingStartEntities.PushBack(xID); }
	}
	// Cancel a pending start: decrement count, remove from pending list, revert lifecycle to AWOKEN
	void CancelPendingStart(Zenith_EntityID xID)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CancelPendingStart must be called from main thread");
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (!xSlot.IsPendingStart()) return;
		xSlot.RevertFromPendingStart();
		Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in CancelPendingStart");
		m_uPendingStartCount--;
		m_axPendingStartEntities.EraseValue(xID);
	}

	bool HasPendingStarts() const { return m_uPendingStartCount > 0; }
	bool IsEntityAwoken(Zenith_EntityID xID) const { return xID.m_uIndex < g_xEngine.EntityStore().m_axEntitySlots.GetSize() && g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).IsAwoken(); }
	bool IsEntityStarted(Zenith_EntityID xID) const { return xID.m_uIndex < g_xEngine.EntityStore().m_axEntitySlots.GetSize() && g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).IsStarted(); }
	bool IsOnEnableDispatched(Zenith_EntityID xID) const { return xID.m_uIndex < g_xEngine.EntityStore().m_axEntitySlots.GetSize() && g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bOnEnableDispatched; }
	void SetOnEnableDispatched(Zenith_EntityID xID, bool bDispatched) { Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetOnEnableDispatched must be called from main thread"); g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bOnEnableDispatched = bDispatched; }

	// Invalidate cached activeInHierarchy for an entity and all descendants
	static void InvalidateActiveInHierarchyCache(Zenith_EntityID xID);

	bool IsUpdating() const { return m_bIsUpdating; }
	void RegisterCreatedDuringUpdate(Zenith_EntityID xID)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RegisterCreatedDuringUpdate must be called from main thread");
		if (m_bIsUpdating)
		{
			g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bCreatedDuringUpdate = true;
		}
	}
	bool WasCreatedDuringUpdate(Zenith_EntityID xID) const { return xID.m_uIndex < g_xEngine.EntityStore().m_axEntitySlots.GetSize() && g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bCreatedDuringUpdate; }

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
	void ProcessPendingDestructions();

	//==========================================================================
	// Internal State Queries
	//==========================================================================

	void SetPaused(bool bPaused) { Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetPaused must be called from main thread"); m_bIsPaused = bPaused; }

	//==========================================================================
	// Update (called by SceneManager)
	//==========================================================================

	void Update(float fDt);
	void FixedUpdate(float fFixedDt);

	// Snapshot active entity IDs for safe iteration (entities may be created/destroyed during dispatch)
	Zenith_Vector<Zenith_EntityID> SnapshotActiveEntities() const
	{
		Zenith_Vector<Zenith_EntityID> xSnapshot;
		xSnapshot.Reserve(m_xActiveEntities.GetSize());
		for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
		{
			xSnapshot.PushBack(m_xActiveEntities.Get(u));
		}
		return xSnapshot;
	}

	// Update sub-phases (extracted from Update for readability)
	void DispatchAwakeAndEnableForNewEntities(Zenith_Vector<Zenith_EntityID>& axAllNewEntities);
	void QueuePendingStartsForNewEntities(const Zenith_Vector<Zenith_EntityID>& axAllNewEntities);

	// Per-frame lifecycle hooks dispatched into every still-valid, active
	// entity in the snapshot. Update and LateUpdate share the iteration
	// loop and validity checks; only the dispatched hook differs.
	enum class LifecycleHook { UPDATE, LATE_UPDATE };
	void DispatchLifecycleHookForEntities(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt, LifecycleHook eHook);
	void DispatchOnUpdateForEntities(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt)
		{ DispatchLifecycleHookForEntities(xSnapshotIDs, fDt, LifecycleHook::UPDATE); }
	void DispatchOnLateUpdateForEntities(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt)
		{ DispatchLifecycleHookForEntities(xSnapshotIDs, fDt, LifecycleHook::LATE_UPDATE); }
	void TickTimedDestructions(float fDt);
	void ClearCreatedDuringUpdateFlags();

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

	// Cleanup helper invoked from DispatchAwakeForNewScene when the awake
	// wave limit is hit (pathological OnAwake-creates-entities chain).
	void HandleAwakeOverflow(u_int uWaveStart, u_int uWaveEnd, u_int uMaxIterations);

	/**
	 * Dispatch pending OnStart calls (called at start of Update).
	 * Unity behavior: Start() runs on first frame after scene load.
	 */
	void DispatchPendingStarts();

	enum class PendingStartResult
	{
		SKIP,     // Invalid/stale/not pending - no action needed
		CLEARED,  // Flag cleared and count decremented
		REQUEUE,  // Inactive - re-add to pending list
	};
	PendingStartResult ProcessSinglePendingStart(Zenith_EntityID xEntityID, class Zenith_ComponentMetaRegistry& xRegistry);

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

	// Shared destruction helpers (used by Reset and RemoveEntity)
	void DisableEntity(Zenith_EntityID xID);
	void DestroyEntityComponents(Zenith_EntityID xID);

	// Reset() helpers — each is called exactly once from that function.
	// Builds the destruction-order hierarchy: roots first (via depth-first expansion),
	// then any active entities the walk missed (no-transform or detached).
	void CollectResetHierarchy(Zenith_Vector<Zenith_EntityID>& axHierarchyOut);
	// Deletes pool objects and clears the pool registry.
	void DestroyComponentPools();
	// Releases global entity slots allocated to this scene back to the free list.
	void FreeGlobalSlotsForActiveEntities();
	// Clears per-scene state vectors and flags after destruction completes.
	void ClearSceneStateAfterReset();

	// Shared deserialization helper
	Zenith_EntityID ReadEntityFromDataStream(Zenith_DataStream& xStream, u_int uVersion,
		std::unordered_map<uint32_t, Zenith_EntityID>& xFileIndexToNewID); // #TODO: Replace with engine hash map

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

	// NON-asserting pool fetch (WS10). GetComponentPool asserts when the type is
	// unregistered or the slot is null; the sparse-set query fast path needs to
	// treat "no pool of this type in this scene" as simply an EMPTY result
	// (Query<NeverAddedType> must not assert). Returns nullptr in that case.
	// Friended to Zenith_Query via the Zenith_UnitTests-style access; Query holds
	// a Zenith_SceneData* and calls this directly.
	template<typename T>
	Zenith_ComponentPool<T>* TryGetComponentPool() const
	{
		const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
		if (uTypeID >= m_xComponents.GetSize()) return nullptr;
		Zenith_ComponentPoolBase* pxPoolBase = m_xComponents.Get(uTypeID);
		if (pxPoolBase == nullptr) return nullptr;
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

// Structural note: the SceneData.h ↔ scene-system-header textual cycle was
// broken in the T2.4 refactor by:
//   * Lifting Zenith_ComponentPool* / Zenith_Component concept into
//     Zenith_ComponentPool.h (this header includes that one above)
//   * Replacing the template-body call to g_xEngine.Scenes().AreRenderTasksActive()
//     with the free-function forwarder Zenith_AreRenderTasksActive() declared
//     in Zenith_RenderTaskState.h (also included above)
// Zenith_SceneSystem.h still includes SceneData.h at the bottom for its
// GetAllOfComponentTypeFromAllScenes template body's calls into
// Zenith_SceneData::AppendAllOfComponentType<T>; that dependency is now one-way.

//==============================================================================
// Template Implementations
// These must be in the header so they're visible to all translation units
//==============================================================================

template<typename T, typename... Args>
T& Zenith_SceneData::CreateComponent(Zenith_EntityID xID, Args&&... args)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CreateComponent must be called from main thread");
	Zenith_Assert(EntityExists(xID), "CreateComponent: Entity (idx=%u, gen=%u) does not exist", xID.m_uIndex, xID.m_uGeneration);

	Zenith_ComponentPool<T>* pxPool = GetOrCreateComponentPool<T>();
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();

	// Dense pool: always append at the end. Removal (RemoveComponentFromEntity)
	// keeps the pool contiguous via swap-and-pop, so there are no holes to reuse.
	const u_int uComponentIndex = pxPool->EmplaceBack(xID, std::forward<Args>(args)...);

	g_xEngine.EntityStore().m_axEntityComponents.Get(xID.m_uIndex)[uTypeID] = uComponentIndex;
	MarkDirty();
	return pxPool->Get(uComponentIndex);
}

template<typename T>
bool Zenith_SceneData::EntityHasComponent(Zenith_EntityID xID) const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || Zenith_AreRenderTasksActive()
		,
		"EntityHasComponent must be called from main thread");
	if (!EntityExists(xID)) return false;
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	const auto& xMap = g_xEngine.EntityStore().m_axEntityComponents.Get(xID.m_uIndex);
	return xMap.contains(uTypeID);
}

template<typename T>
T& Zenith_SceneData::GetComponentFromEntity(Zenith_EntityID xID) const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || Zenith_AreRenderTasksActive(),
		"GetComponentFromEntity must be called from main thread");
	Zenith_Assert(EntityExists(xID), "GetComponentFromEntity: Entity (idx=%u, gen=%u) does not exist", xID.m_uIndex, xID.m_uGeneration);

	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	const auto& xMap = g_xEngine.EntityStore().m_axEntityComponents.Get(xID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	Zenith_Assert(xIt != xMap.end(), "GetComponentFromEntity: Entity does not have component");
	const u_int uIndex = xIt->second;
	return GetComponentPool<T>()->Get(uIndex);
}

template<typename T>
bool Zenith_SceneData::RemoveComponentFromEntity(Zenith_EntityID xID)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RemoveComponentFromEntity must be called from main thread");
	if (!EntityExists(xID)) return false;

	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	auto& xMap = g_xEngine.EntityStore().m_axEntityComponents.Get(xID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	if (xIt == xMap.end()) return false;

	u_int uComponentIndex = xIt->second;
	Zenith_ComponentPool<T>* pxPool = GetOrCreateComponentPool<T>();

	// Call OnRemove lifecycle if the component type has it (C++20 requires clause)
	if constexpr (requires(T& t) { t.OnRemove(); })
	{
		pxPool->Get(uComponentIndex).OnRemove();
	}

	// Real swap-and-pop: destruct this slot and, unless it was the last one,
	// move the last live element into it. RemoveAtSwapAndPop returns the owner
	// of the moved element so we can repoint its stored component index.
	const Zenith_EntityID xMovedOwner = pxPool->RemoveAtSwapAndPop(uComponentIndex);

	// Erase the removed entity's entry BEFORE repointing the moved owner — the
	// moved owner may be a different entity, and (in the last-element case) is
	// INVALID_ENTITY_ID, so its branch is skipped.
	xMap.erase(xIt);

	if (xMovedOwner.IsValid() && xMovedOwner != xID)
	{
		g_xEngine.EntityStore().m_axEntityComponents.Get(xMovedOwner.m_uIndex)[uTypeID] = uComponentIndex;
	}

	MarkDirty();
	return true;
}

template<typename T>
void Zenith_SceneData::GetAllOfComponentType(Zenith_Vector<T*>& xOut) const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || Zenith_AreRenderTasksActive(), "GetAllOfComponentType must be called from main thread");
	xOut.Clear();
	AppendAllOfComponentType<T>(xOut);
}

template<typename T>
void Zenith_SceneData::AppendAllOfComponentType(Zenith_Vector<T*>& xOut) const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || Zenith_AreRenderTasksActive(), "AppendAllOfComponentType must be called from main thread");
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	if (uTypeID >= m_xComponents.GetSize()) return;

	Zenith_ComponentPoolBase* pxPoolBase = m_xComponents.Get(uTypeID);
	if (pxPoolBase == nullptr) return;

	Zenith_ComponentPool<T>* pxPool = static_cast<Zenith_ComponentPool<T>*>(pxPoolBase);
	// Dense pool: every slot in [0, GetSize()) is live — no holes to skip.
	for (u_int u = 0; u < pxPool->GetSize(); ++u)
	{
		Zenith_Assert(pxPool->m_xOwningEntities.Get(u).IsValid(), "AppendAllOfComponentType: dense pool slot %u is not occupied", u);
		xOut.PushBack(&pxPool->Get(u));
	}
}

template<typename T>
void Zenith_SceneData::TransferComponent(Zenith_EntityID xEntityID, Zenith_SceneData* pxSource, Zenith_SceneData* pxTarget)
{
	const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
	auto& xMap = g_xEngine.EntityStore().m_axEntityComponents.Get(xEntityID.m_uIndex);
	auto xIt = xMap.find(uTypeID);
	if (xIt == xMap.end()) return;

	u_int uSourcePoolIndex = xIt->second;

	Zenith_ComponentPool<T>* pxSourcePool = pxSource->GetComponentPool<T>();
	Zenith_ComponentPool<T>* pxTargetPool = pxTarget->GetOrCreateComponentPool<T>();

	// Move the component into the target pool (appended at its end).
	T& xSourceComponent = pxSourcePool->Get(uSourcePoolIndex);
	u_int uNewPoolIndex = pxTargetPool->MoveEmplaceBack(xEntityID, std::move(xSourceComponent));

	// Remove the (now moved-from) source slot with a real swap-and-pop so the
	// SOURCE pool stays dense. If a different entity's component was moved into
	// uSourcePoolIndex to fill the gap, repoint THAT entity's stored index.
	const Zenith_EntityID xMovedOwner = pxSourcePool->RemoveAtSwapAndPop(uSourcePoolIndex);
	if (xMovedOwner.IsValid() && xMovedOwner != xEntityID)
	{
		g_xEngine.EntityStore().m_axEntityComponents.Get(xMovedOwner.m_uIndex)[uTypeID] = uSourcePoolIndex;
	}

	// Update the transferred entity's mapping to point at the target pool index.
	xMap[uTypeID] = uNewPoolIndex;
}

// Restore zone state
#ifndef ZENITH_SCENEDATA_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_SCENEDATA_ZONE_WAS_SET
