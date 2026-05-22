#include "Zenith.h"
#include "TaskSystem/Zenith_TaskSystemImpl.h"

#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include "EntityComponent/Internal/Zenith_SceneRegistryImpl.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleSchedulerImpl.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"

#include <algorithm>

//=============================================================================
// Zenith_SceneLifecycleScheduler — implementations.
//
// Owns the per-frame Update pipeline + the lifecycle-deferral state that
// gates it. Public Zenith_SceneManager methods forward into this class.
//=============================================================================

// Phase 5e: scheduler state lives on Zenith_SceneLifecycleSchedulerImpl
// (held by Zenith_Engine as m_pxSceneLifecycle). Method bodies and
// external readers reach it via g_xEngine.SceneLifecycle().m_xXxx.

//=============================================================================
// Animation update task — file-statics, only this TU touches them.
//=============================================================================

namespace
{
	Zenith_TaskArray*               g_pxAnimUpdateTask = nullptr;
	Zenith_Vector<Flux_MeshAnimation*> g_xAnimationsToUpdate;

	void AnimUpdateTask(void*, u_int uInvocationIndex, u_int uNumInvocations)
	{
		const float fDt = g_xEngine.Frame().GetDt();
		const u_int uTotalAnimations = g_xAnimationsToUpdate.GetSize();

		if (uInvocationIndex >= uTotalAnimations)
		{
			return;
		}

		const u_int uAnimsPerInvocation = (uTotalAnimations + uNumInvocations - 1) / uNumInvocations;
		const u_int uStartIndex = uInvocationIndex * uAnimsPerInvocation;
		const u_int uEndIndex = (uStartIndex + uAnimsPerInvocation < uTotalAnimations) ?
			uStartIndex + uAnimsPerInvocation : uTotalAnimations;

		for (u_int u = uStartIndex; u < uEndIndex; u++)
		{
			Flux_MeshAnimation* pxAnim = g_xAnimationsToUpdate.Get(u);
			Zenith_Assert(pxAnim != nullptr, "Null animation");
			pxAnim->Update(fDt);
		}
	}

	void CollectUpdatableScenes(Zenith_Vector<Zenith_SceneData*>& axOut)
	{
		for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
		{
			Zenith_SceneData* pxData = g_xEngine.SceneRegistry().m_axScenes.Get(i);
			if (Zenith_SceneRegistry::IsSceneUpdatable(pxData))
			{
				axOut.PushBack(pxData);
			}
		}
	}

	void CollectAnimationsFromScene(Zenith_SceneData* pxData)
	{
		// Skeletal animation is handled by Zenith_AnimatorComponent::OnUpdate;
		// kept as a no-op so the update-loop call site stays structurally familiar.
		(void)pxData;
	}
}

//=============================================================================
// Lifecycle
//=============================================================================

void Zenith_SceneLifecycleScheduler::Initialise()
{
	Zenith_Assert(g_pxAnimUpdateTask == nullptr, "SceneLifecycleScheduler::Initialise called twice without Shutdown");
	g_pxAnimUpdateTask = new Zenith_TaskArray(ZENITH_PROFILE_INDEX__ANIMATION, AnimUpdateTask, nullptr, 4, true);
}

void Zenith_SceneLifecycleScheduler::Shutdown()
{
	if (g_pxAnimUpdateTask)
	{
		g_pxAnimUpdateTask->WaitUntilComplete();
		delete g_pxAnimUpdateTask;
		g_pxAnimUpdateTask = nullptr;
	}
	g_xAnimationsToUpdate.Clear();

	g_xEngine.SceneLifecycle().m_bIsLoadingScene = false;
	g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating = false;
	g_xEngine.SceneLifecycle().m_bIsUpdating = false;
	g_xEngine.SceneLifecycle().m_ulLastDeferredLoadOp = 0;
	g_xEngine.SceneLifecycle().m_fFixedTimeAccumulator = 0.0f;
	// g_xEngine.SceneLifecycle().m_fFixedTimestep intentionally NOT reset — it's a config knob, not transient state.
	g_xEngine.SceneLifecycle().m_axCurrentlyLoadingPaths.Clear();
	g_xEngine.SceneLifecycle().m_axLifecycleLoadStack.Clear();
	g_xEngine.SceneLifecycle().m_iPendingBuildIndex = -1;
	g_xEngine.SceneLifecycle().m_axCreationTargetStack.Clear();
	g_xEngine.SceneLifecycle().m_bIsMainLoopRunning = false;
	g_xEngine.SceneLifecycle().m_pfnInitialSceneLoad = nullptr;
}

//=============================================================================
// Update pipeline
//=============================================================================

void Zenith_SceneLifecycleScheduler::Update(float fDt)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Update must be called from main thread");

	Zenith_SceneOperationQueue::ProcessPendingAsyncLoads();
	Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads();
	Zenith_SceneOperationQueue::CleanupCompletedOperations();

	// Mark as updating - any LoadScene/LoadSceneByIndex calls during script execution
	// will route through LoadSceneAsync to defer to next frame (Unity parity).
	g_xEngine.SceneLifecycle().m_bIsUpdating = true;

	// E.20 (finding 3.21): collect updatable scenes once per frame.
	Zenith_Vector<Zenith_SceneData*> axUpdatable;
	CollectUpdatableScenes(axUpdatable);

	// HIGH-1: Unity execution order is Awake -> OnEnable -> Start -> FixedUpdate
	// -> Update -> LateUpdate. Flush pending Starts BEFORE the FixedUpdate
	// accumulator.
	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		axUpdatable.Get(i)->DispatchPendingStarts();
	}

	// Fixed-timestep accumulation.
	static constexpr float fMAX_FIXED_DT = 0.333f;
	g_xEngine.SceneLifecycle().m_fFixedTimeAccumulator += std::min(fDt, fMAX_FIXED_DT);
	while (g_xEngine.SceneLifecycle().m_fFixedTimeAccumulator >= g_xEngine.SceneLifecycle().m_fFixedTimestep)
	{
		for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
		{
			axUpdatable.Get(i)->FixedUpdate(g_xEngine.SceneLifecycle().m_fFixedTimestep);
		}
		g_xEngine.SceneLifecycle().m_fFixedTimeAccumulator -= g_xEngine.SceneLifecycle().m_fFixedTimestep;
	}

	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		axUpdatable.Get(i)->Update(fDt);
	}

	// Animation Update (parallel task system).
	g_xAnimationsToUpdate.Clear();
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		CollectAnimationsFromScene(g_xEngine.SceneRegistry().m_axScenes.Get(i));
	}

	g_xEngine.SceneLifecycle().m_bIsUpdating = false;

	if (g_pxAnimUpdateTask)
	{
#ifdef ZENITH_ASSERT
		Zenith_SceneManager::SetAnimTasksActive(true);
#endif
		g_xEngine.Tasks().SubmitTaskArray(g_pxAnimUpdateTask);
	}
}

void Zenith_SceneLifecycleScheduler::WaitForUpdateComplete()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "WaitForUpdateComplete must be called from main thread");

	if (g_pxAnimUpdateTask)
	{
		g_pxAnimUpdateTask->WaitUntilComplete();
#ifdef ZENITH_ASSERT
		Zenith_SceneManager::SetAnimTasksActive(false);
#endif
	}
}

//=============================================================================
// Circular-load detection
//=============================================================================

void Zenith_SceneLifecycleScheduler::PushLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "PushLifecycleContext must be called from main thread");
	g_xEngine.SceneLifecycle().m_axLifecycleLoadStack.PushBack(strCanonicalPath);
}

void Zenith_SceneLifecycleScheduler::PopLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "PopLifecycleContext must be called from main thread");
	g_xEngine.SceneLifecycle().m_axLifecycleLoadStack.EraseValue(strCanonicalPath);
}

bool Zenith_SceneLifecycleScheduler::IsCircularLoadDependency(const std::string& strCanonicalPath)
{
	for (u_int i = 0; i < g_xEngine.SceneLifecycle().m_axCurrentlyLoadingPaths.GetSize(); ++i)
	{
		if (g_xEngine.SceneLifecycle().m_axCurrentlyLoadingPaths.Get(i) == strCanonicalPath) return true;
	}
	for (u_int i = 0; i < g_xEngine.SceneLifecycle().m_axLifecycleLoadStack.GetSize(); ++i)
	{
		if (g_xEngine.SceneLifecycle().m_axLifecycleLoadStack.Get(i) == strCanonicalPath) return true;
	}
	return false;
}

//=============================================================================
// Initial-scene-load callback
//=============================================================================

void Zenith_SceneLifecycleScheduler::SetInitialSceneLoadCallback(InitialSceneLoadFn pfn)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetInitialSceneLoadCallback must be called from main thread");
	g_xEngine.SceneLifecycle().m_pfnInitialSceneLoad = pfn;
}

Zenith_SceneLifecycleScheduler::InitialSceneLoadFn Zenith_SceneLifecycleScheduler::GetInitialSceneLoadCallback()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetInitialSceneLoadCallback must be called from main thread");
	return g_xEngine.SceneLifecycle().m_pfnInitialSceneLoad;
}

//=============================================================================
// Fixed timestep
//=============================================================================

void Zenith_SceneLifecycleScheduler::SetFixedTimestep(float fTimestep)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetFixedTimestep must be called from main thread");
	Zenith_Assert(fTimestep > 0.0f, "Fixed timestep must be positive");
	g_xEngine.SceneLifecycle().m_fFixedTimestep = fTimestep;
}

float Zenith_SceneLifecycleScheduler::GetFixedTimestep()
{
	return g_xEngine.SceneLifecycle().m_fFixedTimestep;
}

//=============================================================================
// Test-harness hook
//=============================================================================

#ifdef ZENITH_TESTING
void Zenith_SceneLifecycleScheduler::DispatchFullLifecycleInit()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "DispatchFullLifecycleInit must be called from main thread");
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = g_xEngine.SceneRegistry().m_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded)
		{
			pxData->DispatchLifecycleForNewScene();
		}
	}
}
#endif
