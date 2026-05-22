#pragma once

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"
#include "EntityComponent/Zenith_Scene.h"
#include <string>

// Phase 5e: per-Engine state for the scene lifecycle scheduler. The 12
// statics on the Zenith_SceneLifecycleScheduler facade
// (s_bIsLoadingScene / s_bIsPrefabInstantiating / s_bIsUpdating /
// s_ulLastDeferredLoadOp / s_fFixedTimeAccumulator / s_fFixedTimestep /
// s_axCurrentlyLoadingPaths / s_axLifecycleLoadStack /
// s_iPendingBuildIndex / s_axCreationTargetStack / s_bIsMainLoopRunning /
// s_pfnInitialSceneLoad) move onto this Impl, held by Zenith_Engine.
// External readers reach them via g_xEngine.SceneLifecycle().m_xXxx.
//
// Animation-task state (g_pxAnimUpdateTask + g_xAnimationsToUpdate) stays
// file-local to the .cpp -- only the scheduler implementation touches it.
class Zenith_SceneLifecycleSchedulerImpl
{
public:
	Zenith_SceneLifecycleSchedulerImpl() = default;
	~Zenith_SceneLifecycleSchedulerImpl() = default;

	Zenith_SceneLifecycleSchedulerImpl(const Zenith_SceneLifecycleSchedulerImpl&) = delete;
	Zenith_SceneLifecycleSchedulerImpl& operator=(const Zenith_SceneLifecycleSchedulerImpl&) = delete;

	// Lifecycle-deferral flags written via Zenith_SceneManager's RAII
	// guards (LifecycleDeferralGuard / PrefabInstantiationGuard /
	// SceneUpdateDeferralGuard).
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

	// Creation-target stack (entities created during scene materialisation
	// land here instead of the active scene).
	Zenith_Vector<Zenith_Scene>   m_axCreationTargetStack;

	bool                          m_bIsMainLoopRunning       = false;

	Zenith_SceneLifecycleScheduler::InitialSceneLoadFn m_pfnInitialSceneLoad = nullptr;
};
