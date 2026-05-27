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

#include "Collections/Zenith_Vector.h"
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
	// Zenith_SceneLifecycleScheduler owned by Zenith_Engine).
	// External readers reach it via g_xEngine.SceneLifecycle().m_xXxx.
	//==========================================================================

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Create the animation update task. Called from Zenith_SceneManager::Initialise().
	void Initialise();

	// Reset all scheduler state. Called from Zenith_SceneManager::Shutdown().
	void Shutdown();

	//==========================================================================
	// Update pipeline
	//==========================================================================

	void Update(float fDt);
	void WaitForUpdateComplete();

	//==========================================================================
	// Circular-load detection
	//==========================================================================

	void PushLifecycleContext(const std::string& strCanonicalPath);
	void PopLifecycleContext(const std::string& strCanonicalPath);
	bool IsCircularLoadDependency(const std::string& strCanonicalPath);

	//==========================================================================
	// Initial-scene-load callback (editor / Play hook)
	//==========================================================================

	void SetInitialSceneLoadCallback(InitialSceneLoadFn pfn);
	InitialSceneLoadFn GetInitialSceneLoadCallback();

	//==========================================================================
	// Fixed timestep
	//==========================================================================

	void SetFixedTimestep(float fTimestep);
	float GetFixedTimestep();

	//==========================================================================
	// Default creation-target / main-loop / last-deferred-op accessors
	// (Phase 5c: migrated from Zenith_SceneManager).
	//==========================================================================

	// Top of the creation-target stack (set by SceneCreationTargetScope) or
	// the active scene when no scope is in flight.
	Zenith_Scene GetDefaultCreationScene();

	// Sets the main-loop-running flag (main thread only).
	void SetMainLoopRunning(bool bRunning);

	// Returns the operation id of the last LoadScene() call that was deferred
	// (auto-promoted to async) because lifecycle deferral was active.
	Zenith_SceneOperationID GetLastDeferredLoadOp() const { return m_ulLastDeferredLoadOp; }

	// Inline accessors for the lifecycle-deferral flags. Read-only — the only
	// writers are the RAII guards in Zenith_SceneManagerGuards.h.
	bool IsLoadingScene() const        { return m_bIsLoadingScene; }
	bool IsPrefabInstantiating() const { return m_bIsPrefabInstantiating; }
	bool IsUpdating() const            { return m_bIsUpdating; }

	// Pending build-index plumb consumed by scene creation.
	int  GetPendingBuildIndex() const  { return m_iPendingBuildIndex; }

	//==========================================================================
	// Test harness hook
	//==========================================================================

#ifdef ZENITH_TESTING
	// Test-only: dispatch lifecycle init across all loaded scenes. Only called
	// by tests that build a scene via deferred entity creation. Production
	// lifecycle dispatch flows through Update() / DispatchLifecycleForNewScene()
	// on per-scene boundaries.
	void DispatchFullLifecycleInit();
#endif

	//==========================================================================
	// Data members (was Zenith_SceneLifecycleScheduler)
	//==========================================================================

	// Lifecycle-deferral flags written via Zenith_SceneManager's RAII guards
	// (LifecycleDeferralGuard / PrefabInstantiationGuard / SceneUpdateDeferralGuard).
	bool                          m_bIsLoadingScene          = false;
	bool                          m_bIsPrefabInstantiating   = false;
	bool                          m_bIsUpdating              = false;

	Zenith_SceneOperationID       m_ulLastDeferredLoadOp     = 0;

	float                         m_fFixedTimeAccumulator    = 0.0f;
	float                         m_fFixedTimestep           = 0.02f;  // 50Hz default

	// Circular-load-detection stacks.
	Zenith_Vector<std::string>    m_axCurrentlyLoadingPaths;
	Zenith_Vector<std::string>    m_axLifecycleLoadStack;

	int                           m_iPendingBuildIndex       = -1;

	// Creation-target stack (entities created during scene materialisation land
	// here instead of the active scene).
	Zenith_Vector<Zenith_Scene>   m_axCreationTargetStack;

	bool                          m_bIsMainLoopRunning       = false;

	InitialSceneLoadFn            m_pfnInitialSceneLoad      = nullptr;

	// ===== Phase 5b: render-task-active flags (was on Zenith_SceneManager) =====
	// Debug-only flags marking the "render tasks in flight" window. The
	// engine asserts that scene mutation calls cannot happen while these
	// are true. Production builds compile them out via ZENITH_ASSERT.
#ifdef ZENITH_ASSERT
	bool                          m_bRenderTasksActive = false;
	bool                          m_bAnimTasksActive   = false;
#endif

	// Always-defined accessor. In ZENITH_ASSERT builds, returns the live
	// flag; in non-assert builds, returns false (no render-task window is
	// ever tracked). Callers treat "false" as "not in a render-task
	// window" either way, so IsValid()-style logic stays correct across
	// configs.
	bool AreRenderTasksActive() const
	{
#ifdef ZENITH_ASSERT
		return m_bRenderTasksActive;
#else
		return false;
#endif
	}

#ifdef ZENITH_ASSERT
	void SetRenderTasksActive(bool b) { m_bRenderTasksActive = b; }
	void SetAnimTasksActive(bool b)   { m_bAnimTasksActive = b; }
#endif
};
