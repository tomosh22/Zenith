#pragma once

// =============================================================================
// Navigation guide
// -----------------------------------------------------------------------------
// Zenith_SceneManager is a static facade over six subsystems in
// Zenith/EntityComponent/Internal/. The class declaration is split across
// three headers so this top file stays focused on the public game-facing API:
//
//   Zenith_SceneManager.h          (this file) — public API only
//   Zenith_SceneManagerInternal.h  — engine-internal section: Initialise /
//                                    Shutdown / Update / GetSceneData /
//                                    lifecycle-state read accessors /
//                                    Fire*Callbacks / unload helpers / the
//                                    private implementation block.
//   Zenith_SceneManagerGuards.h    — nested RAII scope-guard types
//                                    (LifecycleDeferralGuard,
//                                     PrefabInstantiationGuard,
//                                     SceneUpdateDeferralGuard,
//                                     SceneCreationTargetScope).
//
// The two sibling headers are included from inside `class Zenith_SceneManager`
// so all members remain class-static. Existing call sites that resolve names
// via `Zenith_SceneManager::Foo` continue to compile unchanged — there is no
// behavioural difference, only a file-organisation one.
//
// Public API sections (this file):
//   - Test harness reset (#ifdef ZENITH_TESTING)
//   - Scene loading (Load/Unload/Async/Bootstrap)
//   - Scene queries (GetActiveScene / GetSceneData / etc.)
//   - Active-scene management
//   - Entity helpers (MoveEntityToScene / etc.)
//   - Callbacks (Loaded / Unloading / ActiveSceneChanged)
//
// Internal subsystems delegated to (full mapping in EntityComponent/CLAUDE.md
// and Internal/ARCHITECTURE.md):
//   - Zenith_SceneRegistry           (slot table + generations + name cache)
//   - Zenith_SceneCallbackBus        (event dispatch)
//   - Zenith_SceneOperationQueue     (async scene ops)
//   - Zenith_SceneLifecycleScheduler (OnAwake/OnEnable/Update/etc.)
//   - Zenith_SceneEntityOwnership    (entity slot ownership across moves)
//   - Zenith_SceneLifecycleContext   (per-frame lifecycle state read surface)
//
// Why is Zenith_SceneData.h included AT THE BOTTOM, after the class definition?
// This header's template bodies (e.g. GetAllOfComponentTypeFromAllScenes) call
// into Zenith_SceneData::AppendAllOfComponentType<T>, so SceneData must be
// fully defined by the time those bodies are compiled. The class body comes
// first and the sibling include is deferred to the bottom.
//
// As of the T2.4 cycle-break, SceneData.h NO LONGER includes this header back —
// component-pool types live in Zenith_ComponentPool.h and the AreRenderTasksActive
// dependency was replaced with the free-function forwarder in
// Zenith_RenderTaskState.h. The dependency is now strictly one-way:
// SceneManager.h → SceneData.h.
// =============================================================================

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
 *   PREFER `CreateEmptyScene(name)` for new code — it expresses the same
 *   intent without routing through the file-load state machine.
 *
 *   Status (A6.4): no production callers exist; the mode is exercised only
 *   by its own regression-guard tests in Zenith_SceneManager.Tests.inl. A
 *   future cleanup will either retire the mode entirely (delete the enum
 *   value + implementation + regression tests as a unit) or formalize it as
 *   a documented test-fixture-only API. A `[[deprecated]]` attribute is NOT
 *   added here because warnings-as-errors would fail the build on the
 *   regression-guard tests.
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
 *   // Gameplay transition (queue-and-defer; completes on the next Update tick).
 *   // Return value is INVALID_SCENE; retrieve the operation via
 *   // Zenith_SceneManager::GetLastDeferredLoadOp() if you need to track it.
 *   Zenith_SceneManager::LoadScene("Level.zscen", SCENE_LOAD_SINGLE);
 *
 *   // Bootstrap (pre-main-loop). Pumps Update internally; returns a real Scene.
 *   Zenith_Scene xScene = Zenith_SceneManager::LoadSceneBlockingForBootstrap(
 *       "Level.zscen", SCENE_LOAD_SINGLE);
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
	friend class Zenith_SceneOperationQueue; // A3+: queue calls private UnloadAllNonPersistent during SINGLE-mode teardown

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

	/**
	 * Unity-parity CreateScene. Validates the name (rejects empty + duplicates)
	 * before delegating to CreateEmptyScene(name, true). Returns INVALID_SCENE
	 * on rejection and logs an error; CreateEmptyScene retains its permissive
	 * behaviour for legacy callers.
	 *
	 * Reference: https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.CreateScene.html
	 */
	static Zenith_Scene CreateScene(const std::string& strName);

	/**
	 * Create an entity in GetDefaultCreationScene() — the top of the
	 * SceneCreationTargetScope stack if a load is in progress, otherwise the
	 * active scene. Logs an error and returns a default-constructed (invalid)
	 * Zenith_Entity when no creation target exists (no active scene + no
	 * scope). Mirrors Unity's contract that GameObjects created during scene
	 * deserialization / SceneLoaded land in the loading scene.
	 */
	static Zenith_Entity CreateEntity(const std::string& strName);

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
	// Scene Loading (Queue-and-defer — gameplay transitions)
	//==========================================================================

	/**
	 * Queue-and-defer scene load (B4). Forces all in-flight async operations
	 * to complete first (Unity flush-prior-async semantic), then queues the
	 * load via LoadSceneAsync. The new scene's lifecycle (Awake / OnEnable /
	 * SceneLoaded) fires during a subsequent Zenith_SceneManager::Update tick.
	 *
	 * RETURN VALUE: always Zenith_Scene::INVALID_SCENE. The scene handle is
	 * not knowable until Phase 1 runs next tick. Callers that need to track
	 * the load:
	 *   * Use Zenith_SceneManager::GetLastDeferredLoadOp() and resolve via
	 *     GetOperation(opId)->GetResultScene() once IsComplete() is true.
	 *   * Or use LoadSceneAsync directly and retain the operation ID.
	 *   * Bootstrap and editor-tool code: prefer LoadSceneBlockingForBootstrap
	 *     / LoadSceneBlocking_ToolsOnly which pump Update and return a real
	 *     scene handle.
	 *
	 * RE-ENTRANCY: when invoked from inside a SceneLoaded handler (or any
	 * context where ProcessPendingAsyncLoads is mid-iteration, or
	 * IsUpdating() is true), the prior-flush is skipped — the queue is
	 * mid-process and re-pumping it would re-fire the same callback.
	 * The op is still queued and recoverable via GetLastDeferredLoadOp().
	 */
	static Zenith_Scene LoadScene(const std::string& strPath,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	/**
	 * Queue-and-defer scene load by build index. Same contract as LoadScene
	 * (returns INVALID_SCENE; op is recoverable via GetLastDeferredLoadOp).
	 */
	static Zenith_Scene LoadSceneByIndex(int iBuildIndex,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	//==========================================================================
	// Scene Loading (Blocking — bootstrap / tools only)
	//==========================================================================

	/**
	 * Bootstrap-only blocking scene load. Use from per-game
	 * Project_LoadInitialScene wrappers. Two legitimate call sites:
	 *   * Non-tools bootstrap: Zenith_Main.cpp calls Project_LoadInitialScene
	 *     before Zenith_MainLoop starts.
	 *   * Tools editor automation: the LOAD_INITIAL_SCENE step replays
	 *     Project_LoadInitialScene as the final automation action. This runs
	 *     inside the main loop but under a LifecycleDeferralGuard.
	 *
	 * Asserts that either the main loop has not yet started OR a
	 * LifecycleDeferralGuard is currently on the stack (i.e.,
	 * IsLoadingScene()). Gameplay calls from script Update have neither and
	 * fail loudly — those callers must use LoadSceneAsync.
	 */
	static Zenith_Scene LoadSceneBlockingForBootstrap(const std::string& strPath,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	/**
	 * Bootstrap-only blocking load by build index. Same contract as
	 * LoadSceneBlockingForBootstrap; provided because per-game bootstrap
	 * registers build indices before loading the initial scene.
	 */
	static Zenith_Scene LoadSceneByIndexBlockingForBootstrap(int iBuildIndex,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

#ifdef ZENITH_TOOLS
	/**
	 * Editor-only blocking scene load. Used by editor commands (Open Scene,
	 * Play/Stop transitions, hierarchy drag-drop) where the editor is allowed
	 * to block on a user-initiated action. Compiled out of non-tools builds.
	 *
	 * No main-loop guard — the editor runs inside the main loop and is
	 * permitted to block on its own commands.
	 */
	static Zenith_Scene LoadSceneBlocking_ToolsOnly(const std::string& strPath,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);

	/**
	 * Editor-only blocking load by build index. Same contract as
	 * LoadSceneBlocking_ToolsOnly.
	 */
	static Zenith_Scene LoadSceneByIndexBlocking_ToolsOnly(int iBuildIndex,
		Zenith_SceneLoadMode eMode = SCENE_LOAD_SINGLE);
#endif

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
	 * Retrieve the operation ID for the most recent `LoadScene` /
	 * `LoadSceneByIndex` call. Post-B4 these are queue-and-defer always: they
	 * return `INVALID_SCENE` and stash the op-id here, where callers can pick
	 * it up to poll / subscribe for completion (or to feed into
	 * `LoadSceneBlockingForBootstrap` / `LoadSceneBlocking_ToolsOnly` which
	 * pump Update internally).
	 *
	 * Returns `ZENITH_INVALID_OPERATION_ID` if:
	 *  - No `LoadScene*` call has been made yet, or the runtime was just reset.
	 *  - The op-id has since been cleaned up (see `GetOperation` docstring for
	 *    the ~60-frame cleanup window).
	 *
	 * Unity-parity reference: Unity's `LoadScene` returns a `Scene` handle
	 * directly even from script Update. Zenith decoupled the return value
	 * from the underlying op so callers explicitly opt into the right
	 * synchronization model (queue-and-defer for gameplay, blocking helpers
	 * for bootstrap / tools).
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

#ifdef ZENITH_TESTING
	/**
	 * Test-only call counter for UnloadUnusedAssets. Incremented on every
	 * call (manual or auto-fired by SCENE_LOAD_SINGLE teardown). Reset to 0
	 * by ResetForNextTest. Used by B3 regression tests to assert that
	 * SCENE_LOAD_SINGLE auto-fires UnloadUnusedAssets and SCENE_LOAD_ADDITIVE
	 * does not.
	 */
	static uint32_t GetUnloadUnusedAssetsCallCount();
#endif

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

	// Engine-internal section: Initialise / Shutdown / Update / GetSceneData /
	// lifecycle-state read accessors / Fire*Callbacks / Unload helpers / etc.,
	// plus the full private implementation block. Moved to a sibling header so
	// this file stays focused on the public game-facing API. Existing call
	// sites resolve unchanged because the declarations remain class members.
	#include "EntityComponent/Zenith_SceneManagerInternal.h"
};

// Include the registry header BEFORE SceneData.h so the manager's template
// body below sees Zenith_SceneRegistry's full class declaration (with public
// query API). SceneData.h's `friend class Zenith_SceneRegistry` would otherwise
// inject a forward declaration that the template can't resolve members against.
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"

// A3: same rationale as the registry include above. Manager.cpp's public async
// API methods and lifecycle code reference Zenith_SceneOperationQueue::s_*
// directly; they need the full class declaration here so name lookup succeeds.
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"

// A4: same rationale. Manager-internal lifecycle code references
// Zenith_SceneLifecycleScheduler::s_* directly.
#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"

// A5: cross-scene entity moves, merges, persistent promotion, destruction.
#include "EntityComponent/Internal/Zenith_SceneEntityOwnership.h"

// Include SceneData after class definition for template implementations
// that need the full Zenith_SceneData type. This is safe because SceneData.h
// includes SceneManager.h after its own class definition (no circular issue).
#include "EntityComponent/Zenith_SceneData.h"

// ============================================================================
// Template implementations
// ============================================================================

// Iterate all loaded scenes via the registry's public accessors instead of
// touching slot storage directly. Keeps the template body free of any assumption
// about where the storage lives — works whether s_axScenes is on the manager
// (A2a) or on the registry (post-A2b).
template<typename T>
void Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes(Zenith_Vector<T*>& xOut)
{
	xOut.Clear();

	const uint32_t uSlotCount = Zenith_SceneRegistry::GetSceneSlotCount();
	for (uint32_t uIndex = 0; uIndex < uSlotCount; ++uIndex)
	{
		Zenith_SceneData* pxData = Zenith_SceneRegistry::GetLoadedSceneDataAtSlot(uIndex);
		if (pxData)
		{
			pxData->AppendAllOfComponentType<T>(xOut);
		}
	}
}
