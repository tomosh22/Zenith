#pragma once

#include "Collections/Zenith_Vector.h"
#include <atomic>
#include <string>
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Forward declarations
struct Zenith_Scene;
class Zenith_SceneData;
class Zenith_SceneOperation;
class Zenith_Entity;
struct Zenith_EntityID;
class Zenith_DataStream;
class Zenith_CameraComponent;

/**
 * Scene loading modes (similar to Unity's LoadSceneMode)
 *
 * Unity equivalent modes:
 * - SCENE_LOAD_SINGLE = LoadSceneMode.Single
 * - SCENE_LOAD_ADDITIVE = LoadSceneMode.Additive
 *
 * ZENITH EXTENSION:
 * - SCENE_LOAD_ADDITIVE_WITHOUT_LOADING has no Unity equivalent.
 *   Use this for procedurally generated scenes or runtime-created content.
 */
enum Zenith_SceneLoadMode : uint8_t
{
	SCENE_LOAD_SINGLE = 0,                   // Unity: LoadSceneMode.Single - Unload existing non-persistent scenes, load new
	SCENE_LOAD_ADDITIVE = 1,                 // Unity: LoadSceneMode.Additive - Keep existing scenes, add new scene
	SCENE_LOAD_ADDITIVE_WITHOUT_LOADING = 2  // ZENITH EXTENSION: Create empty scene (for procedural content)
};

/**
 * Operation ID for tracking async scene operations
 * Use GetOperation() to retrieve the operation from an ID.
 * Operations are automatically cleaned up after completion.
 */
using Zenith_SceneOperationID = uint64_t;
static constexpr Zenith_SceneOperationID ZENITH_INVALID_OPERATION_ID = 0;

/**
 * Zenith_SceneManager - Multi-scene management system
 *
 * Static class that manages scene lifecycle, similar to Unity's SceneManager.
 * Supports:
 * - Multiple simultaneous scenes (additive loading)
 * - Async scene loading with progress tracking
 * - Persistent entities across scene loads
 * - Scene event callbacks
 *
 * THREAD SAFETY:
 * - All public methods must be called from the main thread only
 * - Internal async operations use worker threads for file I/O but process
 *   scene creation and entity deserialization on the main thread
 * - Callback invocations always occur on the main thread
 * - The only thread-safe operations are reading progress from Zenith_SceneOperation
 *
 * Thread Safety Summary:
 * +----------------------------------+------------------+--------------------------------+
 * | Operation                        | Thread           | Notes                          |
 * +----------------------------------+------------------+--------------------------------+
 * | LoadScene/LoadSceneAsync         | Main thread only | Asserted at function entry     |
 * | UnloadScene/UnloadSceneAsync     | Main thread only | Asserted at function entry     |
 * | GetOperation()                   | Main thread only | Asserted at function entry     |
 * | Operation::GetProgress()         | Any thread       | Uses memory_order_acquire      |
 * | Operation::IsComplete()          | Any thread       | Uses memory_order_acquire      |
 * | Operation::SetActivationAllowed  | Main thread only | Asserted at function entry     |
 * | Operation::RequestCancel         | Main thread only | Asserted at function entry     |
 * | All callbacks                    | Main thread      | Invoked during Update()        |
 * | AsyncSceneLoadTask (internal)    | Worker thread    | File I/O only, no ECS access   |
 * +----------------------------------+------------------+--------------------------------+
 *
 * Example usage:
 *   // Synchronous load
 *   Zenith_Scene xScene = Zenith_SceneManager::LoadScene("Level.zscen", SCENE_LOAD_SINGLE);
 *
 *   // Asynchronous load with progress
 *   Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync("Level.zscen");
 *   Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
 *   if (pxOp) pxOp->SetActivationAllowed(false);  // Pause at 90%
 *
 * UNITY PARITY NOTES:
 * - MoveEntityToScene(): Matches Unity's MoveGameObjectToScene behavior.
 *   EntityID is globally unique and stable across scene moves (no recreate).
 *   Components are move-constructed to the target scene's pools (zero-copy).
 *   No lifecycle events fire during the move (Unity parity).
 *   Children are moved recursively.
 *
 *   Example:
 *       Zenith_SceneManager::MoveEntityToScene(xMyEntity, xTargetScene);
 *       xMyEntity.GetName(); // Safe - same EntityID, same handle
 *
 * - UnloadSceneAsync(): Spreads entity destruction over multiple frames
 *   (50 entities/frame by default) to avoid frame hitches on large scenes.
 *
 * - Lifecycle timing: OnAwake and OnEnable are called during scene load.
 *   OnStart is deferred until the first Update() frame, matching Unity behavior.
 *
 * - ActiveSceneChanged: Fires on SetActiveScene(), LoadScene(SINGLE), and scene unloads.
 *   This matches Unity's activeSceneChanged behavior.
 *
 * IMPORTANT - ASSET MANAGEMENT:
 * Unlike Unity, Zenith does NOT automatically unload assets when scenes change.
 * Unity calls Resources.UnloadUnusedAssets() during SINGLE mode loads; Zenith does not.
 * To prevent memory growth when cycling through scenes, you must manually unload
 * unused assets after scene changes.
 *
 * Example:
 *   Zenith_SceneManager::UnloadScene(oldScene);
 *   // Call your asset manager's cleanup function here
 *   // e.g., Flux_TextureManager::UnloadUnused();
 */
class Zenith_SceneManager
{
	friend class Zenith_SceneData;  // For lifecycle context access
	friend class Zenith_SceneTests; // For unit test access to s_bIsUpdating

public:
	//==========================================================================
	// Scene Count Queries
	//==========================================================================

	/**
	 * Get number of currently loaded scenes.
	 * Matches Unity's sceneCount: includes DontDestroyOnLoad once it has entities.
	 */
	static uint32_t GetLoadedSceneCount();

	/**
	 * Get total scene count (includes scenes being loaded/unloaded)
	 */
	static uint32_t GetTotalSceneCount();

	/**
	 * Get number of scenes registered in build settings
	 */
	static uint32_t GetBuildSceneCount();

	//==========================================================================
	// Scene Creation
	//==========================================================================

	/**
	 * Create an empty scene at runtime (no file)
	 * Useful for procedural content generation.
	 */
	static Zenith_Scene CreateEmptyScene(const std::string& strName);

	//==========================================================================
	// Scene Queries
	//==========================================================================

	/**
	 * Get the currently active scene.
	 * Note: Safe to call from worker threads during render task execution.
	 * The active scene handle is stable during this window because all scene changes
	 * complete before render tasks are submitted, and the task system's queue mutex
	 * provides the happens-before relationship for memory visibility.
	 */
	static Zenith_Scene GetActiveScene();

	/**
	 * Get scene by index in loaded scenes list
	 */
	static Zenith_Scene GetSceneAt(uint32_t uIndex);

	/**
	 * Get scene by build index
	 */
	static Zenith_Scene GetSceneByBuildIndex(int iBuildIndex);

	/**
	 * Get scene by name
	 */
	static Zenith_Scene GetSceneByName(const std::string& strName);

	/**
	 * Get scene by file path
	 */
	static Zenith_Scene GetSceneByPath(const std::string& strPath);

	//==========================================================================
	// Scene Loading (Synchronous)
	//==========================================================================

	/**
	 * Load scene synchronously (blocks until complete)
	 */
	static Zenith_Scene LoadScene(const std::string& strPath,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	/**
	 * Load scene by build index synchronously
	 */
	static Zenith_Scene LoadSceneByIndex(int iBuildIndex,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	//==========================================================================
	// Scene Loading (Asynchronous)
	//==========================================================================

	/**
	 * Load scene asynchronously (non-blocking)
	 * Returns operation ID for progress tracking via GetOperation().
	 * Operation is automatically cleaned up after completion.
	 */
	static Zenith_SceneOperationID LoadSceneAsync(const std::string& strPath,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	/**
	 * Load scene by build index asynchronously
	 */
	static Zenith_SceneOperationID LoadSceneAsyncByIndex(int iBuildIndex,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	/**
	 * Get operation by ID
	 * Returns nullptr if operation has been cleaned up or ID is invalid.
	 *
	 * WARNING: The returned pointer is only valid for ~60 frames after the
	 * operation completes. Do NOT cache this pointer across frames.
	 * Use IsOperationValid(ulID) to check validity before each access.
	 */
	static Zenith_SceneOperation* GetOperation(Zenith_SceneOperationID ulID);

	/**
	 * Check if an operation ID is still valid (not yet cleaned up)
	 * Safe to call even with invalid IDs - returns false for ZENITH_INVALID_OPERATION_ID.
	 * Use this to check validity before calling GetOperation() after delays.
	 */
	static bool IsOperationValid(Zenith_SceneOperationID ulID);

	//==========================================================================
	// Scene Unloading
	//==========================================================================

	/**
	 * Unload a scene
	 *
	 * @note Executes synchronously. The scene is fully unloaded when this method returns.
	 *       Cannot unload the persistent scene.
	 */
	static void UnloadScene(Zenith_Scene xScene);

	/**
	 * Unload a scene asynchronously (Unity-style)
	 * Spreads entity destruction across multiple frames to avoid hitches.
	 * Returns operation ID for progress tracking via GetOperation().
	 *
	 * @note Cannot unload the persistent scene.
	 * @note Unlike Unity, assets loaded by the scene are NOT automatically unloaded.
	 *       Consider calling asset cleanup methods after scene unload if memory is a concern.
	 *       Unity equivalent: Resources.UnloadUnusedAssets() - not yet implemented in Zenith.
	 */
	static Zenith_SceneOperationID UnloadSceneAsync(Zenith_Scene xScene);

	//==========================================================================
	// Scene Management
	//==========================================================================

	/**
	 * Set the active scene (for new entity creation)
	 *
	 * When the active scene is unloaded (via UnloadScene or UnloadSceneAsync),
	 * Zenith automatically selects a new active scene:
	 * 1. Most recently loaded non-persistent scene (by load timestamp)
	 * 2. Fallback to persistent scene if no other scenes loaded
	 *
	 * Note: Unity behavior may differ in edge cases.
	 */
	static bool SetActiveScene(Zenith_Scene xScene);

	/**
	 * Pause a scene - paused scenes skip Update calls
	 * Useful for freezing background scenes while another is active.
	 */
	static void SetScenePaused(Zenith_Scene xScene, bool bPaused);

	/**
	 * Check if a scene is paused
	 */
	static bool IsScenePaused(Zenith_Scene xScene);

	/**
	 * Move an entity to a different scene (Unity-style MoveGameObjectToScene)
	 *
	 * The passed-in entity reference is updated in-place to point to the entity's
	 * new location in the target scene. Children are moved recursively.
	 *
	 * Example:
	 *   Zenith_SceneManager::MoveEntityToScene(xMyEntity, xTargetScene);
	 *   xMyEntity.GetName(); // Safe - xMyEntity now points to entity in target scene
	 *
	 * EntityID is globally unique and remains stable across scene moves (Unity parity).
	 * Components are move-constructed to the target scene's pools (zero-copy, no serialize).
	 * No lifecycle events (OnDisable/OnDestroy/OnAwake/OnEnable) fire during the move.
	 * Cached EntityIDs and Entity handles remain valid after this call.
	 *
	 * @param xEntity The entity to move (reference updated to new location)
	 * @param xTarget The target scene
	 * @return true if move succeeded, false if validation failed (invalid entity, non-root, etc.)
	 */
	static bool MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget);

	/**
	 * Merge all entities from source scene into target scene
	 * Source scene is unloaded after transfer completes.
	 *
	 * @param xSource The scene to merge from (will be unloaded)
	 * @param xTarget The scene to merge into
	 * @return true if merge succeeded, false if validation failed
	 */
	static bool MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget);

	//==========================================================================
	// Entity Destruction
	//==========================================================================

	/**
	 * Destroy an entity (Unity-style deferred destruction at end of frame)
	 * Children are also marked for destruction.
	 *
	 * @note Must be called from the main thread only (not thread-safe).
	 *       Destruction is deferred until the end of the current frame's Update.
	 */
	static void Destroy(Zenith_Entity& xEntity);

	/**
	 * Destroy an entity after a delay (Unity-style Destroy(obj, delay))
	 * Commonly used for particle effects, projectiles, etc.
	 *
	 * @param xEntity The entity to destroy
	 * @param fDelay Delay in seconds before destruction
	 * @note Must be called from the main thread only (not thread-safe).
	 */
	static void Destroy(Zenith_Entity& xEntity, float fDelay);

	/**
	 * Immediately destroy an entity (current-frame destruction for editor/tests)
	 *
	 * @note Must be called from the main thread only (not thread-safe).
	 */
	static void DestroyImmediate(Zenith_Entity& xEntity);

	//==========================================================================
	// Entity Persistence
	//==========================================================================

	/**
	 * Mark entity to persist across scene loads (Unity DontDestroyOnLoad)
	 * Entity is moved to the persistent scene.
	 * The passed-in reference is updated to point to the new location.
	 *
	 * @note Only root entities (no parent) can be marked persistent.
	 *       This matches Unity's DontDestroyOnLoad behavior.
	 *       Non-root entities will log an error and return without action.
	 *
	 * EntityID remains stable after move to persistent scene (globally unique IDs).
	 * Cached EntityIDs and Entity handles remain valid.
	 */
	static void MarkEntityPersistent(Zenith_Entity& xEntity);

	/**
	 * Get the persistent scene (always loaded, never unloaded)
	 */
	static Zenith_Scene GetPersistentScene();

	//==========================================================================
	// Event Callbacks
	//==========================================================================

	// Callback handle for unregistration
	using CallbackHandle = uint64_t;
	static constexpr CallbackHandle INVALID_CALLBACK_HANDLE = 0;

	using SceneChangedCallback = void(*)(Zenith_Scene, Zenith_Scene);
	using SceneLoadedCallback = void(*)(Zenith_Scene, Zenith_SceneLoadMode);
	using SceneUnloadingCallback = void(*)(Zenith_Scene);  // BEFORE destruction
	using SceneUnloadedCallback = void(*)(Zenith_Scene);   // AFTER destruction
	using SceneLoadStartedCallback = void(*)(const std::string&);
	using EntityPersistentCallback = void(*)(const Zenith_Entity&);

	/**
	 * Register callback for active scene changes.
	 * Fires when:
	 * - Explicit SetActiveScene() is called
	 * - Active scene is unloaded and a new active scene is auto-selected
	 * - LoadScene(SINGLE) replaces the active scene
	 * This matches Unity's activeSceneChanged behavior.
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterActiveSceneChangedCallback(SceneChangedCallback pfn);
	static void UnregisterActiveSceneChangedCallback(CallbackHandle ulHandle);

	/**
	 * Register callback for scene loaded events
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterSceneLoadedCallback(SceneLoadedCallback pfn);
	static void UnregisterSceneLoadedCallback(CallbackHandle ulHandle);

	/**
	 * Register callback for scene unloading events (BEFORE destruction)
	 *
	 * ZENITH ENHANCEMENT: Unlike Unity, Zenith provides two unload callbacks:
	 * - sceneUnloading (this one) - fires BEFORE scene destruction
	 * - sceneUnloaded - fires AFTER scene destruction
	 *
	 * Unity only provides sceneUnloaded (after destruction).
	 * Use sceneUnloading for cleanup that needs access to scene data.
	 *
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterSceneUnloadingCallback(SceneUnloadingCallback pfn);
	static void UnregisterSceneUnloadingCallback(CallbackHandle ulHandle);

	/**
	 * Register callback for scene unloaded events (AFTER destruction)
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterSceneUnloadedCallback(SceneUnloadedCallback pfn);
	static void UnregisterSceneUnloadedCallback(CallbackHandle ulHandle);

	/**
	 * Register callback for scene load started events
	 * Called before scene loading begins.
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterSceneLoadStartedCallback(SceneLoadStartedCallback pfn);
	static void UnregisterSceneLoadStartedCallback(CallbackHandle ulHandle);

	/**
	 * Register callback for when entities are moved to persistent scene (DontDestroyOnLoad)
	 * Called AFTER entity is transferred. The entity reference passed to the callback
	 * is valid and points to the entity's new location in the persistent scene.
	 * Note: This is a Zenith extension; Unity's DontDestroyOnLoad has no callback.
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterEntityPersistentCallback(EntityPersistentCallback pfn);
	static void UnregisterEntityPersistentCallback(CallbackHandle ulHandle);

	//==========================================================================
	// Asset Management
	//==========================================================================

	/**
	 * Unload assets that are no longer referenced by any loaded scene.
	 *
	 * IMPORTANT - UNITY DIFFERENCE:
	 * Unity automatically calls Resources.UnloadUnusedAssets() during LoadScene(SINGLE).
	 * Zenith does NOT automatically unload assets. Call this method manually after
	 * scene changes to prevent memory growth.
	 *
	 * Currently a stub - will integrate with Flux asset managers once they support
	 * reference counting (Flux_TextureManager, Flux_ModelManager, etc.).
	 *
	 * Example usage:
	 *   Zenith_SceneManager::LoadScene("NewLevel.zscen", SCENE_LOAD_SINGLE);
	 *   Zenith_SceneManager::UnloadUnusedAssets();  // Clean up textures/models from old level
	 */
	static void UnloadUnusedAssets();

	//==========================================================================
	// Build Settings Registry
	//==========================================================================

	/**
	 * Register a scene path with a build index
	 * Called by editor when loading project build settings.
	 */
	static void RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath);

	/**
	 * Clear all registered build indices
	 * Called when reloading project settings.
	 */
	static void ClearBuildIndexRegistry();

	/**
	 * Get the registered path for a build index.
	 * Returns empty string if build index is not registered.
	 */
	static const std::string& GetRegisteredScenePath(int iBuildIndex);

	/**
	 * Get the total size of the build index registry (includes empty/sparse slots).
	 * Use with GetRegisteredScenePath() to iterate all registered scenes.
	 */
	static uint32_t GetBuildIndexRegistrySize();

	//==========================================================================
	// Fixed Timestep Configuration
	//==========================================================================

	/**
	 * Set the fixed timestep for FixedUpdate calls (default: 0.02s = 50Hz, matching Unity)
	 * @param fTimestep Time in seconds between fixed updates
	 */
	static void SetFixedTimestep(float fTimestep);

	/**
	 * Get the current fixed timestep
	 * @return Time in seconds between fixed updates
	 */
	static float GetFixedTimestep();

	//==========================================================================
	// Async Unload Configuration
	//==========================================================================

	/**
	 * Set the number of entities destroyed per frame during async unloading (default: 50)
	 * Higher values = faster unload but may cause frame hitches
	 * Lower values = smoother unload but takes longer
	 */
	static void SetAsyncUnloadBatchSize(uint32_t uEntitiesPerFrame);

	/**
	 * Get the current async unload batch size
	 */
	static uint32_t GetAsyncUnloadBatchSize();

	/**
	 * Set the maximum number of concurrent async load operations (default: 8)
	 * Loads beyond this limit will still proceed, but a warning will be logged.
	 */
	static void SetMaxConcurrentAsyncLoads(uint32_t uMax);

	/**
	 * Get the maximum number of concurrent async loads
	 */
	static uint32_t GetMaxConcurrentAsyncLoads();

	//==========================================================================
	// Multi-Scene Rendering
	//==========================================================================

	/**
	 * Collect all components of type T from ALL loaded, non-unloading scenes.
	 * Unlike GetSceneData()->GetAllOfComponentType() which only queries one scene,
	 * this iterates all loaded scenes to support multi-scene rendering.
	 *
	 * Safe to call from render tasks (scene list is stable during render window).
	 */
	template<typename T>
	static void GetAllOfComponentTypeFromAllScenes(Zenith_Vector<T*>& xOut);

	/**
	 * Find the main camera across all loaded scenes.
	 * Tries the active scene first, then searches all loaded scenes.
	 * Returns nullptr if no valid main camera exists in any scene.
	 */
	static Zenith_CameraComponent* FindMainCameraAcrossScenes();

	/**
	 * Get number of internal scene slots (for iterating scenes in render systems).
	 * Slots may be empty (nullptr) - always check GetSceneDataAtSlot() return value.
	 * Safe to call from render tasks.
	 */
	static uint32_t GetSceneSlotCount();

	/**
	 * Get scene data at internal slot index.
	 * Returns nullptr for empty/freed slots. Check IsLoaded() && !IsUnloading()
	 * before accessing scene data for rendering.
	 * Safe to call from render tasks.
	 */
	static Zenith_SceneData* GetSceneDataAtSlot(uint32_t uIndex);

	//==========================================================================
	// Internal (Engine Use Only)
	//==========================================================================

	static void Initialise();
	static void Shutdown();
	static void NotifyAsyncJobPriorityChanged() { s_bAsyncJobsNeedSort = true; }
	static void Update(float fDt);
	static void WaitForUpdateComplete();

	/**
	 * Access internal scene data (for engine systems)
	 */
	static Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	static Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	static Zenith_SceneData* GetSceneDataForEntity(Zenith_EntityID xID);

	/**
	 * Build a full scene handle (with generation) from an internal handle index
	 */
	static Zenith_Scene GetSceneFromHandle(int iHandle);

	/**
	 * Check if scene is loading (for asset management)
	 */
	static bool IsLoadingScene();

	/**
	 * Set prefab instantiating flag
	 */
	static void SetPrefabInstantiating(bool b);

	/**
	 * Set scene loading flag (suppresses immediate lifecycle dispatch in entity constructors)
	 */
	static void SetLoadingScene(bool b);

	/**
	 * Dispatch lifecycle init for all loaded scenes
	 */
	static void DispatchFullLifecycleInit();

	/**
	 * Register the initial scene load callback (called by platform main to set Project_LoadInitialScene)
	 * Used by the editor to re-run initial scene setup after Play/Stop cycle
	 */
	typedef void(*InitialSceneLoadFn)();
	static void SetInitialSceneLoadCallback(InitialSceneLoadFn pfnCallback);
	static InitialSceneLoadFn GetInitialSceneLoadCallback();

	/**
	 * Reset all Flux render systems (called before scene teardown)
	 * Must be called before UnloadScene/UnloadAllNonPersistent when doing
	 * a full scene swap, to clear Flux system state before entity destruction.
	 */
	static void ResetAllRenderSystems();

	/**
	 * Mark that we're inside a frame update phase (script execution, UI callbacks, etc.)
	 * While true, LoadScene/LoadSceneByIndex route through async to defer to next frame.
	 * Set by SceneManager::Update internally, and by the main loop around UI update.
	 */
	static void SetIsUpdating(bool b) { s_bIsUpdating = b; }

	/**
	 * Unload a scene bypassing the "last scene" guard.
	 * Used by editor backup restore where the guard would prevent cleanup.
	 */
	static void UnloadSceneForced(Zenith_Scene xScene);

private:
	// Internal helper for moving entities between scenes (zero-copy transfer)
	static bool MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData);

	// Shared predicate: should this scene be visible to GetLoadedSceneCount/GetSceneAt?
	// Prevents the two functions from diverging if the visibility logic changes.
	static bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);

	// Shared predicate: should this scene receive FixedUpdate/Start/Update/LateUpdate calls?
	// Loaded, activated, not unloading, and not paused.
	static bool IsSceneUpdatable(const Zenith_SceneData* pxData);

	// Select new active scene when current one is unloaded.
	// Prefers lowest build index, falls back to most recently loaded.
	// iExcludeHandle: scene handle to exclude (e.g. scene being unloaded, -1 for none)
	static int SelectNewActiveScene(int iExcludeHandle = -1);

	// Scene storage
	static Zenith_Vector<Zenith_SceneData*> s_axScenes;
	static Zenith_Vector<uint32_t> s_axSceneGenerations;  // Generation counters for stale handle detection
	static Zenith_Vector<int> s_axFreeHandles;
	static int s_iActiveSceneHandle;
#ifdef ZENITH_ASSERT
	static bool s_bRenderTasksActive;  // Debug-only: true between SubmitRenderTasks and WaitForAllRenderTasks
	static bool s_bAnimTasksActive;    // Debug-only: true between SubmitTaskArray(animTask) and WaitUntilComplete
public:
	static void SetRenderTasksActive(bool b) { s_bRenderTasksActive = b; }
	static bool AreRenderTasksActive() { return s_bRenderTasksActive; }
	static void SetAnimTasksActive(bool b) { s_bAnimTasksActive = b; }
private:
#endif
	static int s_iPersistentSceneHandle;

	// True while inside Update() - LoadScene/LoadSceneByIndex route through async when set
	// (Unity parity: LoadScene is deferred during script execution)
	static bool s_bIsUpdating;

	static Zenith_Vector<Zenith_SceneOperation*> s_axActiveOperations;
	static float s_fFixedTimeAccumulator;
	static float s_fFixedTimestep;  // Configurable fixed timestep (default 0.02 = 50Hz, matching Unity)
	static uint32_t s_uAsyncUnloadBatchSize;  // Entities destroyed per frame during async unload (default 50)
	static uint32_t s_uMaxConcurrentAsyncLoads;  // Maximum concurrent async loads (default 8)

	// Loaded scene name cache - smaller than scanning all scene slots
	// #TODO: Replace with engine hash map when available
	struct SceneNameEntry
	{
		std::string m_strName;
		int m_iHandle;
	};
	static Zenith_Vector<SceneNameEntry> s_axLoadedSceneNames;
	static void AddToSceneNameCache(int iHandle, const std::string& strName);
	static void RemoveFromSceneNameCache(int iHandle);

	// Flags
	static bool s_bIsLoadingScene;
	static bool s_bIsPrefabInstantiating;

	// Initial scene load callback (set by platform main, used by editor for Play/Stop reset)
	static InitialSceneLoadFn s_pfnInitialSceneLoad;

	// Load order counter (for selecting most recently loaded scene when active is unloaded)
	static uint64_t s_ulNextLoadTimestamp;

	// Event callback wrappers with handles
	template<typename T>
	struct CallbackEntry
	{
		CallbackHandle m_ulHandle;
		T m_pfnCallback;
	};

	// Templatized callback list to reduce boilerplate across 6 callback types
	template<typename TCallback>
	struct Zenith_CallbackList
	{
		Zenith_Vector<CallbackEntry<TCallback>> m_axEntries;

		CallbackHandle Register(TCallback pfn);
		bool Unregister(CallbackHandle ulHandle);

		template<typename... Args>
		void Fire(Args&&... args);
	};

	static Zenith_CallbackList<SceneChangedCallback> s_xActiveSceneChangedCallbacks;
	static Zenith_CallbackList<SceneLoadedCallback> s_xSceneLoadedCallbacks;
	static Zenith_CallbackList<SceneUnloadingCallback> s_xSceneUnloadingCallbacks;
	static Zenith_CallbackList<SceneUnloadedCallback> s_xSceneUnloadedCallbacks;
	static Zenith_CallbackList<SceneLoadStartedCallback> s_xSceneLoadStartedCallbacks;
	static Zenith_CallbackList<EntityPersistentCallback> s_xEntityPersistentCallbacks;
	static CallbackHandle s_ulNextCallbackHandle;

	// Deferred callback removal to prevent issues when unregistering during callback dispatch
	static Zenith_Vector<CallbackHandle> s_axCallbacksPendingRemoval;
	static uint32_t s_uFiringCallbacksDepth;
	static void ProcessPendingCallbackRemovals();
	static bool IsCallbackPendingRemoval(CallbackHandle ulHandle);

	// Build index registry (indexed by build index - dense, non-negative)
	static Zenith_Vector<std::string> s_axBuildIndexToPath;

	// Circular load detection (small sets - linear scan is fine)
	static Zenith_Vector<std::string> s_axCurrentlyLoadingPaths;
	static Zenith_Vector<std::string> s_axLifecycleLoadStack;  // Scenes in lifecycle dispatch (OnAwake/OnEnable)

	// Operation ID tracking for safe async operation access (few active at any time - linear scan)
	// #TODO: Replace with engine hash map
	struct OperationMapEntry
	{
		uint64_t m_ulOperationID = 0;
		Zenith_SceneOperation* m_pxOperation = nullptr;
	};
	static Zenith_Vector<OperationMapEntry> s_axOperationMap;
	static uint64_t s_ulNextOperationID;

	// Internal helpers
	static int AllocateSceneHandle();
	static void FreeSceneHandle(int iHandle);

	// Lifecycle context tracking (for circular load detection during OnAwake/OnEnable)
	static void PushLifecycleContext(const std::string& strCanonicalPath);
	static void PopLifecycleContext(const std::string& strCanonicalPath);
	static void ProcessPendingUnloads();
	static void UnloadAllNonPersistent();
	static uint32_t CountScenesBeingAsyncUnloaded();
	static void CleanupCompletedOperations();
	static void FireSceneLoadedCallbacks(Zenith_Scene xScene, Zenith_SceneLoadMode eMode);
	static void FireSceneUnloadingCallbacks(Zenith_Scene xScene);
	static void FireSceneUnloadedCallbacks(Zenith_Scene xScene);
	static void FireActiveSceneChangedCallbacks(Zenith_Scene xOld, Zenith_Scene xNew);
	static void FireSceneLoadStartedCallbacks(const std::string& strPath);
	static void FireEntityPersistentCallbacks(const Zenith_Entity& xEntity);
	static CallbackHandle AllocateCallbackHandle();
	static Zenith_SceneOperationID AllocateOperationID();
	static void AsyncSceneLoadTask(void* pData);

	// File load milestones (used instead of atomic float for better ordering guarantees)
	enum class FileLoadMilestone : uint8_t
	{
		IDLE = 0,
		FILE_READ_STARTED = 10,     // Maps to 0.1 progress
		FILE_READ_COMPLETE = 70     // Maps to 0.7 progress
	};

	// Async loading job - file I/O happens on worker thread
	struct AsyncLoadJob
	{
		// Phase tracking for two-phase async load:
		// Phase 1: File I/O → SINGLE cleanup → create scene → deserialize (progress 0→0.9)
		// Phase 2: Activation (Awake/OnEnable) when allowed (progress 0.9→1.0)
		enum class LoadPhase : uint8_t
		{
			WAITING_FOR_FILE,     // File I/O on worker thread
			DESERIALIZED,         // Scene created and deserialized, waiting for activation
		};

		std::string m_strPath;           // Original path for file I/O
		std::string m_strCanonicalPath;  // Canonical path for tracking/cleanup
		Zenith_SceneLoadMode m_eMode;
		int m_iBuildIndex;                   // Build index if loaded by index, -1 otherwise
		Zenith_SceneOperation* m_pxOperation;  // Owned by s_axActiveOperations
		std::atomic<bool> m_bFileLoadComplete;
		std::atomic<FileLoadMilestone> m_eMilestone;  // File read progress milestones
		Zenith_DataStream* m_pxLoadedData;  // Owned, delete when done
		Zenith_Task* m_pxTask;              // Task for worker thread execution
		LoadPhase m_ePhase;                 // Current load phase (main thread only)
		int m_iCreatedSceneHandle;          // Scene handle after deserialization (-1 until created)

		AsyncLoadJob() : m_iBuildIndex(-1), m_pxOperation(nullptr), m_bFileLoadComplete(false), m_eMilestone(FileLoadMilestone::IDLE), m_pxLoadedData(nullptr), m_pxTask(nullptr), m_ePhase(LoadPhase::WAITING_FOR_FILE), m_iCreatedSceneHandle(-1) {}
		~AsyncLoadJob()
		{
			Zenith_Assert(Zenith_Multithreading::IsMainThread(), "AsyncLoadJob must be deleted from main thread");
			delete m_pxLoadedData;
			delete m_pxTask;
		}
	};
	static Zenith_Vector<AsyncLoadJob*> s_axAsyncJobs;
	static bool s_bAsyncJobsNeedSort;

	// Async unloading job - destruction spread across frames
	struct AsyncUnloadJob
	{
		int m_iSceneHandle;
		uint32_t m_uSceneGeneration;
		Zenith_SceneOperation* m_pxOperation;
		uint32_t m_uTotalEntities;
		uint32_t m_uDestroyedEntities;
		bool m_bUnloadingCallbackFired;

		AsyncUnloadJob() : m_iSceneHandle(-1), m_uSceneGeneration(0), m_pxOperation(nullptr),
			m_uTotalEntities(0), m_uDestroyedEntities(0), m_bUnloadingCallbackFired(false) {}
		~AsyncUnloadJob()
		{
			Zenith_Assert(Zenith_Multithreading::IsMainThread(), "AsyncUnloadJob must be deleted from main thread");
		}
	};
	static Zenith_Vector<AsyncUnloadJob*> s_axAsyncUnloadJobs;

	// Returns true if the scene can be unloaded (not persistent, not last scene, not already unloading)
	static bool CanUnloadScene(Zenith_Scene xScene);
	// Shared unload logic (callbacks, destruction, active scene reselection)
	static void UnloadSceneInternal(Zenith_Scene xScene);

	static void CancelAllPendingAsyncLoads(AsyncLoadJob* pxExclude = nullptr);
	static void ProcessPendingAsyncLoads();
	static void ProcessPendingAsyncUnloads();

	// Mark an async load operation as failed and fire its completion callback
	static void FailAsyncLoadOperation(Zenith_SceneOperation* pxOp);
	// Clean up an async load job and remove it from the job list (does not increment index)
	static void CleanupAndRemoveAsyncJob(u_int uIndex);
};

// Include SceneData after class definition for template implementations
// that need the full Zenith_SceneData type. This is safe because SceneData.h
// includes SceneManager.h after its own class definition (no circular issue).
#include "EntityComponent/Zenith_SceneData.h"

// ============================================================================
// Template implementations
// ============================================================================

template<typename T>
void Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes(Zenith_Vector<T*>& xOut)
{
	xOut.Clear();
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->IsLoaded() && !pxData->IsUnloading())
		{
			Zenith_Vector<T*> xTemp;
			pxData->GetAllOfComponentType<T>(xTemp);
			for (u_int j = 0; j < xTemp.GetSize(); ++j)
			{
				xOut.PushBack(xTemp.Get(j));
			}
		}
	}
}
