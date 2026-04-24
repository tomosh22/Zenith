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
 *   PREFER `CreateEmptyScene(name)` for new code — it expresses the same
 *   intent without routing through the file-load state machine, and avoids
 *   the intersection rules with async deferral / LoadSceneAsyncByIndex.
 */
enum Zenith_SceneLoadMode : uint8_t
{
	SCENE_LOAD_SINGLE = 0,                   // Unity: LoadSceneMode.Single - Unload existing non-persistent scenes, load new
	SCENE_LOAD_ADDITIVE = 1,                 // Unity: LoadSceneMode.Additive - Keep existing scenes, add new scene
	SCENE_LOAD_ADDITIVE_WITHOUT_LOADING = 2  // ZENITH EXTENSION: Create empty scene (prefer CreateEmptyScene for new code)
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
 *   (50 entities/frame by default — see audit §3.5). This is a SOFT cap:
 *   RemoveEntity recursively destroys a subtree in one call, so deep
 *   hierarchies can temporarily exceed the per-frame budget. The guarantee
 *   is "at least one subtree root per frame, not exact-N". A warning fires
 *   when a single cascade destroys more than 2x the batch size so QA can
 *   retune SetAsyncUnloadBatchSize() for their content.
 *
 * - Lifecycle timing: OnAwake and OnEnable are called during scene load.
 *   OnStart is deferred until the first Update() frame, matching Unity behavior.
 *
 * - ActiveSceneChanged: Fires on SetActiveScene(), LoadScene(SINGLE), and scene unloads.
 *   This matches Unity's activeSceneChanged behavior.
 *
 * - SCENE_LOAD_SINGLE transition ordering differs from Unity. Zenith uses a
 *   staging-scene atomic-swap: the new scene is fully deserialized while the
 *   old scenes still exist, then the old scenes are torn down as a BATCH.
 *   Subscribers therefore observe all `SceneUnloading`/`SceneUnloaded` callbacks
 *   AFTER the new scene is prepared, not interleaved with it. Unity fires
 *   `sceneUnloaded` per-scene as teardown progresses. If you need Unity's exact
 *   cadence (e.g. for progress bars that count `sceneCount` descending), use
 *   `UnloadSceneAsync` per old scene followed by `LoadSceneAsync` for the new —
 *   the staging pattern only applies to sync/async `SINGLE` mode.
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
	// Test Harness Reset
	//==========================================================================
#ifdef ZENITH_TESTING
	/**
	 * Reset SceneManager to a clean post-Initialise state for the next test.
	 * Called by the test runner between tests so one test can't poison the
	 * next via leaked flags, unloaded persistent scene, etc.
	 *  - Clears s_bIsUpdating / s_bIsLoadingScene / s_bAsyncJobsNeedSort flags.
	 *  - Unloads every non-persistent scene currently loaded.
	 *  - If the persistent scene handle is invalid, re-initialises it.
	 *  - Clears the active scene handle.
	 * Never throws; never asserts on bad input (tests are already failing).
	 */
	static void ResetForNextTest();
#endif

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
	// Creates an empty scene. By default auto-activates if there is no active scene
	// (matches Unity's CreateScene behaviour for the first loaded scene). Pass
	// bAllowSetActive=false to prevent auto-activation — used internally when creating
	// the persistent DontDestroyOnLoad scene (A6).
	static Zenith_Scene CreateEmptyScene(const std::string& strName, bool bAllowSetActive = true);

	//==========================================================================
	// Scene Queries
	//==========================================================================

	/**
	 * Get the currently active scene.
	 *
	 * UNITY-PARITY CONTRACT — read this before calling.
	 * The active scene has exactly two legitimate uses, mirroring Unity's
	 * SceneManager.GetActiveScene (https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.GetActiveScene.html):
	 *   1. Default destination for newly created entities / instantiated prefabs.
	 *   2. Source of lighting / skybox / environment settings for the frame.
	 *
	 * NEVER use GetActiveScene() to resolve the owning scene of an existing entity.
	 * EntityIDs are globally unique across all loaded scenes — an entity may live in
	 * the persistent (DontDestroyOnLoad) scene, an additively-loaded scene, or any
	 * non-active scene. The pattern
	 *     Zenith_Scene xScene = GetActiveScene();
	 *     Zenith_SceneData* pxData = GetSceneData(xScene);
	 *     pxData->EntityHasComponent<T>(id);   // BUG: wrong scene for non-active entities
	 * is the "active-scene-as-filter" anti-pattern. It silently returns false / nullptr
	 * for any entity not in the active scene — breaking multi-scene editing, persistent
	 * entities, and cross-scene gameplay. Unity's docs are explicit: "the active Scene
	 * has no impact on what Scenes are rendered" — and by extension, no impact on which
	 * entities can be queried or edited.
	 *
	 * Use these instead for EntityID → scene resolution:
	 *   - Zenith_SceneManager::GetSceneDataForEntity(Zenith_EntityID) — free-function sites
	 *   - Zenith_Entity::GetSceneData()                               — when entity is in scope
	 *   - Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<T>() — cross-scene iteration
	 *
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
	 * Get scene by name.
	 *
	 * Audit §3.4 note (Unity parity): when two or more loaded scenes share a
	 * name (which CreateEmptyScene / RenameScene allow without explicit
	 * deduplication), this function returns the first-registered match and
	 * logs an ambiguity warning. Unity's SceneManager.GetSceneByName exhibits
	 * the same "first match" behaviour. For deterministic lookup against a
	 * potentially-ambiguous corpus, prefer GetSceneByPath(canonical-path),
	 * which uses the scene's file path as a unique key.
	 */
	static Zenith_Scene GetSceneByName(const std::string& strName);

	/**
	 * Get scene by file path. Use this when names may collide (see
	 * GetSceneByName note) — file paths are unique per loaded scene.
	 */
	static Zenith_Scene GetSceneByPath(const std::string& strPath);

	//==========================================================================
	// Scene Loading (Synchronous)
	//==========================================================================

	/**
	 * Load scene synchronously (blocks until complete).
	 *
	 * IMPORTANT — BEHAVIOUR WHEN CALLED DURING Update():
	 * If invoked from a component OnUpdate/OnLateUpdate/etc., the call is
	 * auto-deferred via LoadSceneAsync and returns INVALID_SCENE (the scene
	 * slot is not allocated until Phase 1 runs next frame, so no handle can
	 * be returned synchronously). A warning is logged. Callers that need to
	 * track the load should invoke LoadSceneAsync directly and retain the
	 * returned operation ID.
	 *
	 * Unity divergence: Unity returns a valid loading handle immediately even
	 * during Update; Zenith returns INVALID in that case. See F9 in the scene
	 * audit for why this is a deliberate simplification.
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

	/**
	 * Retrieve the operation ID created by the most recent deferred sync-load
	 * auto-promotion. When sync `LoadScene(path, SINGLE)` is called inside an
	 * `Update()` tick (where `s_bIsUpdating == true`), Zenith transparently
	 * routes the call through `LoadSceneAsync` and the original caller receives
	 * `INVALID_SCENE`. Call this immediately after that sync `LoadScene` returns
	 * to recover the op-id and poll / subscribe for completion.
	 *
	 * Returns `ZENITH_INVALID_OPERATION_ID` if:
	 *  - The previous `LoadScene` call did not trigger a deferred auto-promotion
	 *    (it completed synchronously outside `Update`, or never happened).
	 *  - The op-id has since been cleaned up (see `GetOperation` docstring for
	 *    the ~60-frame cleanup window).
	 *
	 * Audit §3.8 — Unity-parity note: Unity's `LoadScene` returns a valid
	 * `Scene` handle immediately even from script Update. Zenith's sync entry
	 * point returns `INVALID_SCENE` on deferral, so this accessor surfaces the
	 * op-id that was otherwise dropped inside `HandleDeferredLoad`.
	 * Ref: https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.LoadScene.html
	 */
	static Zenith_SceneOperationID GetLastDeferredLoadOp();

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
	 * Zenith automatically selects a new active scene via SelectNewActiveScene():
	 * 1. Non-persistent scene with the LOWEST m_iBuildIndex (Unity parity).
	 * 2. Fallback: most recently loaded non-persistent scene (by load timestamp)
	 *    among scenes without a build index.
	 * 3. Fallback: INVALID_SCENE. The persistent (DontDestroyOnLoad) scene is NEVER
	 *    selected — it's a container, not a real scene (enforced in A6).
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
	 * Active-scene handling: if xSource is the active scene, xTarget is promoted
	 * to active BEFORE teardown so the active handle never goes invalid during
	 * the merge (differs from Unity, which throws ArgumentException instead).
	 *
	 * Fails if: either handle is invalid, xSource==xTarget, or xSource is the
	 * persistent scene.
	 *
	 * @param xSource The scene to merge from (will be unloaded)
	 * @param xTarget The scene to merge into
	 * @return true if merge succeeded, false if validation failed
	 */
	static bool MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget);

	/**
	 * Rename a loaded scene (E.18 / finding 3.15).
	 *
	 * Updates both the SceneData's m_strName and the SceneManager's name-lookup
	 * cache atomically so GetSceneByName continues to return correct results after
	 * the rename. Previously m_strName was mutable via friend access but the cache
	 * never caught up — leaving stale name lookups.
	 *
	 * @param xScene Target scene. Must be valid and not the persistent scene.
	 * @param strNewName New scene name (empty allowed, though lookups against ""
	 *        then match every nameless scene — caller's responsibility).
	 * @return true if the rename succeeded; false for invalid handle / persistent scene.
	 */
	static bool RenameScene(Zenith_Scene xScene, const std::string& strNewName);

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
	 * Mark entity to persist across scene loads (Unity DontDestroyOnLoad).
	 *
	 * E.19 / finding 3.20: Unity walks UP the hierarchy and marks the ROOT persistent;
	 * a non-root child becomes persistent as part of its ancestor's subtree. Zenith
	 * matches this behaviour — the passed-in entity may be a descendant, and the call
	 * will silently locate its root and move that whole subtree to the persistent
	 * scene. On return, the passed-in Zenith_Entity reference is still valid (entity
	 * IDs are globally unique and stable across scene moves) and points to the same
	 * entity which now lives under the persistent scene.
	 *
	 * Previous documentation here claimed "only root entities can be marked persistent
	 * — non-root entities will log an error and return without action." That was
	 * never what the implementation did. The new docstring reflects reality.
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
	 * Register callback fired BEFORE a scene is destroyed.
	 *
	 * ZENITH ENHANCEMENT over Unity: Zenith splits Unity's single sceneUnloaded
	 * into two callbacks — sceneUnloading (before destruction) and sceneUnloaded
	 * (after destruction). Pick the one whose contract matches your needs:
	 *
	 *   sceneUnloading (THIS callback)            sceneUnloaded (below)
	 *   ──────────────────────────────           ──────────────────────
	 *   Scene data is still intact.              Scene data has been deleted.
	 *   GetSceneData(xScene) != nullptr.         GetSceneData(xScene) == nullptr.
	 *   Entity queries work.                     Handle still unique (not yet
	 *   Name/path/buildIndex readable.           recycled — generation bump happens
	 *                                            after this callback returns).
	 *   Use for: final data capture,             Use for: notification / bookkeeping
	 *     analytics, decoupled save state,         tied to handle identity
	 *     anything that reads entity data          (subscription cleanup, UI refresh
	 *     before it's gone.                        that does NOT touch scene data).
	 *
	 * Subscribers MUST NOT mutate the scene from sceneUnloading — entities will be
	 * destroyed immediately after this callback returns.
	 *
	 * @return Handle for unregistration (0 = invalid)
	 */
	static CallbackHandle RegisterSceneUnloadingCallback(SceneUnloadingCallback pfn);
	static void UnregisterSceneUnloadingCallback(CallbackHandle ulHandle);

	/**
	 * Register callback fired AFTER a scene has been destroyed.
	 *
	 * At dispatch time:
	 *   - The scene's Zenith_SceneData has already been deleted and the slot set to
	 *     nullptr. GetSceneData(xScene) will return nullptr.
	 *   - The scene handle is still unique and safe to identify the scene by —
	 *     the slot generation is NOT bumped until after all subscribers return,
	 *     matching Unity's sceneUnloaded semantics.
	 *   - Do not call GetSceneData, query entities, read the name/path, etc.
	 *     Use RegisterSceneUnloadingCallback for anything that needs scene data.
	 *
	 * Typical use: removing bookkeeping keyed by handle, notifying other systems
	 * that this particular scene is gone, logging/analytics.
	 *
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
	 * Set the async-load warning threshold (default: 8).
	 *
	 * This is a WARNING THRESHOLD, not an enforced cap — loads beyond this count
	 * still proceed. When the in-flight count crosses the threshold, a one-shot
	 * Zenith_Warning is logged so callers notice unbounded queuing. Unity
	 * behaves similarly (unbounded LoadSceneAsync), so "max concurrent" here
	 * means "concurrent count at which to start warning", not "cap".
	 *
	 * To actually cap concurrency, gate submissions in the caller.
	 */
	static void SetMaxConcurrentAsyncLoads(uint32_t uMax);

	/**
	 * Get the current async-load warning threshold.
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

	/**
	 * Get scene data at slot index ONLY if the scene is loaded and not unloading.
	 *
	 * Equivalent to:
	 *   Zenith_SceneData* pxData = GetSceneDataAtSlot(uIndex);
	 *   if (!pxData || !pxData->IsLoaded() || pxData->IsUnloading()) return nullptr;
	 *   return pxData;
	 *
	 * Returns nullptr if the slot is empty, the scene is not fully loaded yet, or
	 * the scene is in teardown. Prefer this over GetSceneDataAtSlot for iteration
	 * from render systems / editor panels — it encapsulates the three safety
	 * checks that every caller otherwise has to repeat.
	 *
	 * Safe to call from render tasks.
	 */
	static Zenith_SceneData* GetLoadedSceneDataAtSlot(uint32_t uIndex);

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
	 *
	 * GetSceneData(Zenith_Scene) resolves a scene HANDLE to its backing SceneData. Do not
	 * pair it with an EntityID to route a per-entity query — that is the active-scene-as-filter
	 * anti-pattern described on GetActiveScene(). Use GetSceneDataForEntity(EntityID) when you
	 * have an EntityID and want the owning scene; it walks the global entity slot table and
	 * is immune to cross-scene moves.
	 */
	static Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	static Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	/**
	 * Resolve an EntityID to its owning scene's data. Prefer this over
	 * GetSceneData(GetActiveScene()) for any EntityID-indexed lookup. Returns nullptr for
	 * invalid / stale IDs (generation check) or when the owning scene has been unloaded.
	 */
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
	 * RAII guard for lifecycle deferral flags.
	 * Ensures the flag is reset even if an exception or early return occurs.
	 * Usage: Zenith_SceneManager::LifecycleDeferralGuard xGuard(s_bIsLoadingScene);
	 */
	struct LifecycleDeferralGuard
	{
		bool& m_bFlag;
		LifecycleDeferralGuard(bool& bFlag) : m_bFlag(bFlag) { m_bFlag = true; }
		~LifecycleDeferralGuard() { m_bFlag = false; }
		LifecycleDeferralGuard(const LifecycleDeferralGuard&) = delete;
		LifecycleDeferralGuard& operator=(const LifecycleDeferralGuard&) = delete;
	};

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
	//==========================================================================
	// Internal Helpers
	//==========================================================================

	static bool MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData);
	static bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);
	static bool IsSceneUpdatable(const Zenith_SceneData* pxData);
	static int SelectNewActiveScene(int iExcludeHandle = -1);
	static int AllocateSceneHandle();
	static void FreeSceneHandle(int iHandle);

	// Per-frame helpers called from Update(). Split out to flatten Update()'s
	// nesting — the animation collection walk was five levels deep inline.
	static void CollectUpdatableScenes(Zenith_Vector<Zenith_SceneData*>& axOut);
	static void CollectAnimationsFromScene(Zenith_SceneData* pxData);

	/**
	 * Create and return an invalid scene handle (handle=-1, generation=0).
	 * Used by query and load functions to return a sentinel when a scene cannot be found or loaded.
	 */
	static Zenith_Scene MakeInvalidScene();

	/**
	 * Check if the given canonical path is already in the pending load list or lifecycle stack.
	 * Used by LoadScene and LoadSceneAsync to prevent circular scene loads.
	 * @return true if the path is already being loaded (circular dependency detected)
	 */
	static bool CheckCircularLoadDependency(const std::string& strCanonicalPath);

	/**
	 * Fire unloading/unloaded callbacks and select a new active scene if needed.
	 * Used by UnloadSceneInternal to consolidate the callback+active-scene-selection logic.
	 * @param iHandle The scene handle being unloaded
	 * @param xScene The scene being unloaded (with valid generation for callbacks)
	 */
	static void FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene);

	//==========================================================================
	// LoadScene helpers — carved out so LoadScene reads as an orchestration
	// shell instead of a 250-line multi-phase body. See LoadScene for the
	// ordering contract that ties these together.
	//==========================================================================

	// Phase 1: pre-condition checks. Returns false if the caller must bail
	// with MakeInvalidScene().
	static bool ValidateLoadRequest(const std::string& strPath);

	// Phase 2: if called from inside an Update (s_bIsUpdating), defer to the
	// async path. Returns true when the load was handled (caller must return
	// xOutScene); false means continue synchronously.
	static bool HandleDeferredLoad(const std::string& strPath, Zenith_SceneLoadMode eMode, Zenith_Scene& xOutScene);

	// Phase 3: file-existence / circular-load / recursion-depth / SINGLE-mode
	// header validation. Returns false if the caller must bail.
	static bool ValidateFileAndDetectCircular(const std::string& strPath, const std::string& strCanonicalPath, Zenith_SceneLoadMode eMode);

	// Phase 4b: SINGLE-mode teardown + swap. Called only after a successful
	// staging-scene deserialization. Fires the consolidated ActiveSceneChanged.
	static void PerformSingleModeTeardownAndSwap(Zenith_Scene xScene, Zenith_Scene xOldActiveBeforeTeardown);

	// Phase 5: Awake / OnEnable / IsActivated flip / SceneLoaded callbacks /
	// clear loading path. Runs after the scene is fully deserialized.
	static void DispatchLifecycleAndFire(Zenith_Scene xScene, Zenith_SceneData* pxSceneData, const std::string& strCanonicalPath, Zenith_SceneLoadMode eMode);

	//==========================================================================
	// Scene Storage
	//==========================================================================

	static Zenith_Vector<Zenith_SceneData*> s_axScenes;
	static Zenith_Vector<uint32_t> s_axSceneGenerations;  // Generation counters for stale handle detection
	static Zenith_Vector<int> s_axFreeHandles;
	static int s_iActiveSceneHandle;
	static int s_iPersistentSceneHandle;
	static uint64_t s_ulNextLoadTimestamp;  // For selecting most recently loaded scene when active is unloaded

	// Scene name cache for O(1) lookup by name
	// #TODO: Replace with engine hash map when available
	struct SceneNameEntry
	{
		std::string m_strName;
		int m_iHandle;
	};
	static Zenith_Vector<SceneNameEntry> s_axLoadedSceneNames;
	static void AddToSceneNameCache(int iHandle, const std::string& strName);
	static void RemoveFromSceneNameCache(int iHandle);

	// Build index registry (indexed by build index - dense, non-negative)
	static Zenith_Vector<std::string> s_axBuildIndexToPath;

	// Audit §3.12: AreRenderTasksActive() is a const read used by Zenith_Scene::IsValid()
	// (see Zenith_Scene.cpp). Previously the getter lived entirely inside
	// `#ifdef ZENITH_ASSERT`, meaning IsValid() only compiled because MEMORY.md says
	// ZENITH_ASSERT is always defined — a fragile coupling. The getter now always
	// exists; the underlying flag and the setter stay debug-only because only the
	// assert paths flip it.
#ifdef ZENITH_ASSERT
	static bool s_bRenderTasksActive;  // Debug-only: true between SubmitRenderTasks and WaitForAllRenderTasks
	static bool s_bAnimTasksActive;    // Debug-only: true between SubmitTaskArray(animTask) and WaitUntilComplete
public:
	static void SetRenderTasksActive(bool b) { s_bRenderTasksActive = b; }
	static void SetAnimTasksActive(bool b) { s_bAnimTasksActive = b; }
private:
#endif

public:
	// In ZENITH_ASSERT builds: returns the live flag. In non-assert builds:
	// returns false because no render-task window is ever tracked. Callers treat
	// "false" as "not in a render-task window" either way, so IsValid() logic
	// stays correct across configs.
	static bool AreRenderTasksActive()
	{
#ifdef ZENITH_ASSERT
		return s_bRenderTasksActive;
#else
		return false;
#endif
	}
private:

	//==========================================================================
	// Lifecycle / Update State
	//==========================================================================

	static bool s_bIsUpdating;            // True inside Update(); LoadScene defers to async when set
	static bool s_bIsLoadingScene;        // Suppresses immediate lifecycle dispatch during load
	// Audit §3.8 — op-id stashed by HandleDeferredLoad when sync LoadScene is
	// auto-promoted to async. Exposed via GetLastDeferredLoadOp().
	static Zenith_SceneOperationID s_ulLastDeferredLoadOp;
	static bool s_bIsPrefabInstantiating;
	static float s_fFixedTimeAccumulator;
	static float s_fFixedTimestep;                // Default 0.02 = 50Hz (Unity parity)

	static InitialSceneLoadFn s_pfnInitialSceneLoad;

	// Circular load detection (small sets - linear scan is fine)
	static Zenith_Vector<std::string> s_axCurrentlyLoadingPaths;
	static Zenith_Vector<std::string> s_axLifecycleLoadStack;  // Scenes in lifecycle dispatch (OnAwake/OnEnable)
	static void PushLifecycleContext(const std::string& strCanonicalPath);
	static void PopLifecycleContext(const std::string& strCanonicalPath);

	//==========================================================================
	// Callback System
	//==========================================================================

	template<typename T>
	struct CallbackEntry
	{
		CallbackHandle m_ulHandle;
		T m_pfnCallback;
	};

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
	static bool IsCallbackHandleInUse(CallbackHandle ulHandle);
	static CallbackHandle AllocateCallbackHandle();

	static void FireSceneLoadedCallbacks(Zenith_Scene xScene, Zenith_SceneLoadMode eMode);
	static void FireSceneUnloadingCallbacks(Zenith_Scene xScene);
	static void FireSceneUnloadedCallbacks(Zenith_Scene xScene);
	static void FireActiveSceneChangedCallbacks(Zenith_Scene xOld, Zenith_Scene xNew);
	static void FireSceneLoadStartedCallbacks(const std::string& strPath);
	static void FireEntityPersistentCallbacks(const Zenith_Entity& xEntity);

	//==========================================================================
	// Async Operations
	//==========================================================================

	static Zenith_Vector<Zenith_SceneOperation*> s_axActiveOperations;
	static uint32_t s_uAsyncUnloadBatchSize;      // Entities destroyed per frame during async unload (default 50)
	static uint32_t s_uMaxConcurrentAsyncLoads;    // Maximum concurrent async loads (default 8)

	// Operation ID tracking (few active at any time - linear scan)
	// #TODO: Replace with engine hash map
	struct OperationMapEntry
	{
		uint64_t m_ulOperationID = 0;
		Zenith_SceneOperation* m_pxOperation = nullptr;
	};
	static Zenith_Vector<OperationMapEntry> s_axOperationMap;
	static uint64_t s_ulNextOperationID;
	static Zenith_SceneOperationID AllocateOperationID();
	static void CleanupCompletedOperations();

	// File load milestones (used instead of atomic float for better ordering guarantees)
	enum class FileLoadMilestone : uint8_t
	{
		IDLE = 0,
		FILE_READ_STARTED = 10,     // Maps to 0.1 progress
		FILE_READ_COMPLETE = 70     // Maps to 0.7 progress
	};

	// Async loading job - file I/O on worker thread, scene creation on main thread
	struct AsyncLoadJob
	{
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
		// A5: generation captured at scene-creation time. Cancellation/finalization paths
		// compare against the current s_axSceneGenerations entry before dereferencing —
		// if they differ, the slot was recycled (scene unloaded + replacement loaded into
		// the same handle) and touching m_iCreatedSceneHandle would corrupt the wrong scene.
		uint32_t m_uCreatedSceneGeneration;

		// A5: Pre-teardown active scene captured so Phase 2 can fire a single consolidated
		// ActiveSceneChanged (old→new) instead of two (old→fallback, fallback→new). Stored
		// as handle+generation rather than Zenith_Scene to avoid making this header depend
		// on Zenith_Scene.h (keeps forward-decl usable). -1 handle means "no snapshot yet".
		int m_iSingleModeOldActiveHandle;
		uint32_t m_uSingleModeOldActiveGeneration;

		AsyncLoadJob() : m_iBuildIndex(-1), m_pxOperation(nullptr), m_bFileLoadComplete(false), m_eMilestone(FileLoadMilestone::IDLE), m_pxLoadedData(nullptr), m_pxTask(nullptr), m_ePhase(LoadPhase::WAITING_FOR_FILE), m_iCreatedSceneHandle(-1), m_uCreatedSceneGeneration(0), m_iSingleModeOldActiveHandle(-1), m_uSingleModeOldActiveGeneration(0) {}
		~AsyncLoadJob()
		{
			Zenith_Assert(Zenith_Multithreading::IsMainThread(), "AsyncLoadJob must be deleted from main thread");
			delete m_pxLoadedData;
			delete m_pxTask;
		}
	};
	static Zenith_Vector<AsyncLoadJob*> s_axAsyncJobs;
	static bool s_bAsyncJobsNeedSort;
	static void AsyncSceneLoadTask(void* pData);
	// Returns the post-cancellation index of pxExclude in s_axAsyncJobs. If pxExclude
	// is nullptr or not found, returns UINT32_MAX. Callers tracking a loop index over
	// s_axAsyncJobs must update it from this return value instead of hard-coding 0
	// (the surviving job is not always at index 0 — insertion-sort by priority can
	// reorder in future refactors).
	static u_int CancelAllPendingAsyncLoads(AsyncLoadJob* pxExclude = nullptr);
	static void ProcessPendingAsyncLoads();
	static void FailAsyncLoadOperation(Zenith_SceneOperation* pxOp);
	static void CleanupAndRemoveAsyncJob(u_int uIndex);

	// ProcessPendingAsyncLoads helpers. Each step returns one of these to tell the
	// outer per-job loop how to advance — job-removal paths all funnel through
	// CleanupAndRemoveAsyncJob which is why the outer loop needs the hint rather
	// than deriving it from the returned bool.
	enum class AsyncJobStepResult
	{
		Removed,     // job removed this iteration; do NOT advance index
		Waiting,     // job still pending; advance to next job
		FallThrough, // step complete; try the next phase on the SAME job this iteration
	};
	static void SortAsyncJobsByPriority();
	// uIndex is mutable: RunAsyncJobPhase1 may cancel peer jobs in SCENE_LOAD_SINGLE
	// mode via CancelAllPendingAsyncLoads. Phase1 updates uIndex from the return
	// value so the outer loop stays aligned with the surviving job's actual slot.
	static bool HandleAsyncJobCancellation(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex);
	static AsyncJobStepResult RunAsyncJobPhase1(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int& uIndex);
	static AsyncJobStepResult RunAsyncJobPhase2(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex);

	// Async unloading job - destruction spread across frames
	struct AsyncUnloadJob
	{
		int m_iSceneHandle;
		uint32_t m_uSceneGeneration;
		Zenith_SceneOperation* m_pxOperation;
		uint32_t m_uTotalEntities;
		uint32_t m_uDestroyedEntities;
		bool m_bUnloadingCallbackFired;

		// MEDIUM-1: when we unload the active scene, the active-pointer swap
		// happens early (so observers never see the dying scene as active)
		// but the ActiveSceneChanged callback fire is DEFERRED until after
		// SceneUnloaded, matching the sync-unload ordering. These fields hold
		// the pending fire until then. Stored as raw handle+generation pairs
		// so this header doesn't need to pull in Zenith_Scene.h.
		bool m_bActiveSceneChangePending;
		int m_iOldActiveHandle;
		uint32_t m_uOldActiveGeneration;
		int m_iNewActiveHandle;
		uint32_t m_uNewActiveGeneration;

		AsyncUnloadJob() : m_iSceneHandle(-1), m_uSceneGeneration(0), m_pxOperation(nullptr),
			m_uTotalEntities(0), m_uDestroyedEntities(0), m_bUnloadingCallbackFired(false),
			m_bActiveSceneChangePending(false),
			m_iOldActiveHandle(-1), m_uOldActiveGeneration(0),
			m_iNewActiveHandle(-1), m_uNewActiveGeneration(0) {}
		~AsyncUnloadJob()
		{
			Zenith_Assert(Zenith_Multithreading::IsMainThread(), "AsyncUnloadJob must be deleted from main thread");
		}
	};
	static Zenith_Vector<AsyncUnloadJob*> s_axAsyncUnloadJobs;
	static void ProcessPendingAsyncUnloads();
	static uint32_t CountScenesBeingAsyncUnloaded();

	//==========================================================================
	// Unload Helpers
	//==========================================================================

	static bool CanUnloadScene(Zenith_Scene xScene);
	static void UnloadSceneInternal(Zenith_Scene xScene);
	static void ProcessPendingUnloads();
	// iExcludeHandle: optional scene-slot index to skip (in addition to the persistent
	// scene). D.12 uses this to keep the staging scene alive while the old world is
	// torn down during an atomic-swap sync LoadScene(SINGLE). Pass -1 (the default)
	// for the original "unload every non-persistent scene" behaviour.
	static void UnloadAllNonPersistent(int iExcludeHandle = -1);
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
	// Clear once, append from each scene directly into xOut — avoids the
	// per-scene temporary vector + copy that previously allocated 1 Zenith_Vector
	// per loaded scene per call, per render-system-frame.
	xOut.Clear();
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->IsLoaded() && !pxData->IsUnloading())
		{
			pxData->AppendAllOfComponentType<T>(xOut);
		}
	}
}
