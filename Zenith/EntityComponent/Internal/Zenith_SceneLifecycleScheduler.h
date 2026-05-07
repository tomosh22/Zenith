#pragma once

// Zenith_SceneLifecycleScheduler — owns the per-frame Update pipeline and the
// lifecycle-deferral state that gates it:
//   * Update / WaitForUpdateComplete / animation task plumbing.
//   * IsLoadingScene / IsPrefabInstantiating / IsUpdating flags.
//   * Fixed-timestep accumulator and timestep config.
//   * Circular-load-detection stacks (s_axCurrentlyLoadingPaths, s_axLifecycleLoadStack).
//   * Last-deferred-load operation id (sync LoadScene auto-promote stash).
//   * Pending build-index plumb consumed by scene creation.
//   * Initial-scene-load callback (set by Zenith_Core for editor restarts).
//   * DispatchFullLifecycleInit (test-harness hook).
//
// Public Zenith_SceneManager methods (Update, WaitForUpdateComplete, IsLoadingScene,
// IsPrefabInstantiating, IsUpdating, GetLastDeferredLoadOp, SetFixedTimestep,
// GetFixedTimestep, SetInitialSceneLoadCallback, GetInitialSceneLoadCallback,
// DispatchFullLifecycleInit) forward into this class. The lifecycle-flag setters
// were removed in A6.3; LifecycleDeferralGuard / PrefabInstantiationGuard /
// SceneUpdateDeferralGuard (defined on Zenith_SceneManager) are now the only
// writers of s_bIsLoadingScene / s_bIsPrefabInstantiating / s_bIsUpdating.
//
// Independent of Zenith_SceneManager.h to avoid the friend-class injection issue
// (same pattern as Zenith_SceneRegistry / Zenith_SceneOperationQueue).

#include "EntityComponent/Zenith_Scene.h"
#include <cstdint>
#include <string>

class Zenith_SceneData;
using Zenith_SceneOperationID = uint64_t;

class Zenith_SceneLifecycleScheduler
{
public:
	using InitialSceneLoadFn = void(*)();

	//==========================================================================
	// Storage (A4: state migrated from Zenith_SceneManager)
	//==========================================================================

	// Lifecycle-deferral flags. Read by Zenith_SceneLifecycleContext accessors;
	// written via the public RAII guards declared on Zenith_SceneManager.
	static bool                          s_bIsLoadingScene;
	static bool                          s_bIsPrefabInstantiating;
	static bool                          s_bIsUpdating;

	static Zenith_SceneOperationID       s_ulLastDeferredLoadOp;

	// Fixed-timestep accumulator (drives FixedUpdate dispatch in Update()).
	static float                         s_fFixedTimeAccumulator;
	static float                         s_fFixedTimestep;            // default 0.02s = 50Hz

	// Two stacks of in-progress load paths used for circular-dependency detection:
	//   * s_axCurrentlyLoadingPaths is pushed during file-I/O / Phase1 deserialization.
	//   * s_axLifecycleLoadStack is pushed during Awake/OnEnable dispatch.
	static Zenith_Vector<std::string>    s_axCurrentlyLoadingPaths;
	static Zenith_Vector<std::string>    s_axLifecycleLoadStack;

	// Pending build-index plumb: LoadSceneByIndex / LoadSceneAsyncByIndex park
	// the caller-supplied build index here; downstream scene-creation reads and
	// clears it. Main-thread-only.
	static int                           s_iPendingBuildIndex;

	// B1: explicit creation-target stack. Pushed by SceneCreationTargetScope
	// around the load/deserialization/activation paths so entities created by
	// scene-load callbacks (deserialization side-effects, OnEnable spawning,
	// SceneLoaded subscribers) land in the scene currently being materialized
	// instead of the active scene. Top of stack drives
	// Zenith_SceneManager::GetDefaultCreationScene(); empty stack falls back
	// to the active scene.
	static Zenith_Vector<Zenith_Scene>   s_axCreationTargetStack;

	// B4: true while Zenith_Core::Zenith_MainLoop is running. Set once by
	// Zenith_SceneManager::SetMainLoopRunning(true) just before the main
	// loop's while(...) and cleared after the loop exits. Read by
	// LoadSceneBlockingForBootstrap to assert bootstrap-only invocation.
	static bool                          s_bIsMainLoopRunning;

	// Set by Zenith_Core (or editor) so the Play/Stop cycle can re-run the
	// project's initial-scene-load hook.
	static InitialSceneLoadFn            s_pfnInitialSceneLoad;

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Create the animation update task. Called from Zenith_SceneManager::Initialise().
	static void Initialise();

	// Reset all scheduler state. Called from Zenith_SceneManager::Shutdown().
	static void Shutdown();

	//==========================================================================
	// Update pipeline
	//==========================================================================

	static void Update(float fDt);
	static void WaitForUpdateComplete();

	//==========================================================================
	// Circular-load detection
	//==========================================================================

	static void PushLifecycleContext(const std::string& strCanonicalPath);
	static void PopLifecycleContext(const std::string& strCanonicalPath);
	static bool IsCircularLoadDependency(const std::string& strCanonicalPath);

	//==========================================================================
	// Initial-scene-load callback (editor / Play hook)
	//==========================================================================

	static void SetInitialSceneLoadCallback(InitialSceneLoadFn pfn);
	static InitialSceneLoadFn GetInitialSceneLoadCallback();

	//==========================================================================
	// Fixed timestep
	//==========================================================================

	static void SetFixedTimestep(float fTimestep);
	static float GetFixedTimestep();

	//==========================================================================
	// Test harness hook
	//==========================================================================

#ifdef ZENITH_TESTING
	// Test-only: dispatch lifecycle init across all loaded scenes. Only called
	// by tests that build a scene via deferred entity creation. Production
	// lifecycle dispatch flows through Update() / DispatchLifecycleForNewScene()
	// on per-scene boundaries.
	static void DispatchFullLifecycleInit();
#endif
};
