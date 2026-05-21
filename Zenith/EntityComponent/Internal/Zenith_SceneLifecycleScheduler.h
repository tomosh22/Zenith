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
	// Storage type only (Phase 5e: actual state lives on
	// Zenith_SceneLifecycleSchedulerImpl owned by Zenith_Engine).
	// External readers reach it via g_xEngine.SceneLifecycle().m_xXxx.
	//==========================================================================

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
