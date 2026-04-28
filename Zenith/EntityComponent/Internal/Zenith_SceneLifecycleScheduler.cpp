#include "Zenith.h"

#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "TaskSystem/Zenith_TaskSystem.h"

#include <algorithm>

//=============================================================================
// Zenith_SceneLifecycleScheduler — implementations.
//
// Owns the per-frame Update pipeline + the lifecycle-deferral state that
// gates it. Public Zenith_SceneManager methods forward into this class.
//=============================================================================

//=============================================================================
// Static storage definitions
//=============================================================================

bool                            Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = false;
bool                            Zenith_SceneLifecycleScheduler::s_bIsPrefabInstantiating = false;
bool                            Zenith_SceneLifecycleScheduler::s_bIsUpdating = false;
Zenith_SceneOperationID         Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp = 0;
float                           Zenith_SceneLifecycleScheduler::s_fFixedTimeAccumulator = 0.0f;
float                           Zenith_SceneLifecycleScheduler::s_fFixedTimestep = 0.02f;  // 50Hz default (Unity parity)
Zenith_Vector<std::string>      Zenith_SceneLifecycleScheduler::s_axCurrentlyLoadingPaths;
Zenith_Vector<std::string>      Zenith_SceneLifecycleScheduler::s_axLifecycleLoadStack;
int                             Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex = -1;
Zenith_Vector<Zenith_Scene>     Zenith_SceneLifecycleScheduler::s_axCreationTargetStack;
bool                            Zenith_SceneLifecycleScheduler::s_bIsMainLoopRunning = false;
Zenith_SceneLifecycleScheduler::InitialSceneLoadFn Zenith_SceneLifecycleScheduler::s_pfnInitialSceneLoad = nullptr;

//=============================================================================
// Animation update task — file-statics, only this TU touches them.
//=============================================================================

namespace
{
	Zenith_TaskArray*               g_pxAnimUpdateTask = nullptr;
	Zenith_Vector<Flux_MeshAnimation*> g_xAnimationsToUpdate;

	void AnimUpdateTask(void*, u_int uInvocationIndex, u_int uNumInvocations)
	{
		const float fDt = Zenith_Core::GetDt();
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
		for (u_int i = 0; i < Zenith_SceneRegistry::s_axScenes.GetSize(); ++i)
		{
			Zenith_SceneData* pxData = Zenith_SceneRegistry::s_axScenes.Get(i);
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

	s_bIsLoadingScene = false;
	s_bIsPrefabInstantiating = false;
	s_bIsUpdating = false;
	s_ulLastDeferredLoadOp = 0;
	s_fFixedTimeAccumulator = 0.0f;
	// s_fFixedTimestep intentionally NOT reset — it's a config knob, not transient state.
	s_axCurrentlyLoadingPaths.Clear();
	s_axLifecycleLoadStack.Clear();
	s_iPendingBuildIndex = -1;
	s_axCreationTargetStack.Clear();
	s_bIsMainLoopRunning = false;
	s_pfnInitialSceneLoad = nullptr;
}

//=============================================================================
// Update pipeline
//=============================================================================

void Zenith_SceneLifecycleScheduler::Update(float fDt)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Update must be called from main thread");

	Zenith_SceneOperationQueue::ProcessPendingAsyncLoads();
	Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads();
	Zenith_SceneOperationQueue::CleanupCompletedOperations();

	// Mark as updating - any LoadScene/LoadSceneByIndex calls during script execution
	// will route through LoadSceneAsync to defer to next frame (Unity parity).
	s_bIsUpdating = true;

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
	s_fFixedTimeAccumulator += std::min(fDt, fMAX_FIXED_DT);
	while (s_fFixedTimeAccumulator >= s_fFixedTimestep)
	{
		for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
		{
			axUpdatable.Get(i)->FixedUpdate(s_fFixedTimestep);
		}
		s_fFixedTimeAccumulator -= s_fFixedTimestep;
	}

	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		axUpdatable.Get(i)->Update(fDt);
	}

	// Animation Update (parallel task system).
	g_xAnimationsToUpdate.Clear();
	for (u_int i = 0; i < Zenith_SceneRegistry::s_axScenes.GetSize(); ++i)
	{
		CollectAnimationsFromScene(Zenith_SceneRegistry::s_axScenes.Get(i));
	}

	s_bIsUpdating = false;

	if (g_pxAnimUpdateTask)
	{
#ifdef ZENITH_ASSERT
		Zenith_SceneManager::SetAnimTasksActive(true);
#endif
		Zenith_TaskSystem::SubmitTaskArray(g_pxAnimUpdateTask);
	}
}

void Zenith_SceneLifecycleScheduler::WaitForUpdateComplete()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "WaitForUpdateComplete must be called from main thread");

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
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "PushLifecycleContext must be called from main thread");
	s_axLifecycleLoadStack.PushBack(strCanonicalPath);
}

void Zenith_SceneLifecycleScheduler::PopLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "PopLifecycleContext must be called from main thread");
	s_axLifecycleLoadStack.EraseValue(strCanonicalPath);
}

bool Zenith_SceneLifecycleScheduler::IsCircularLoadDependency(const std::string& strCanonicalPath)
{
	for (u_int i = 0; i < s_axCurrentlyLoadingPaths.GetSize(); ++i)
	{
		if (s_axCurrentlyLoadingPaths.Get(i) == strCanonicalPath) return true;
	}
	for (u_int i = 0; i < s_axLifecycleLoadStack.GetSize(); ++i)
	{
		if (s_axLifecycleLoadStack.Get(i) == strCanonicalPath) return true;
	}
	return false;
}

//=============================================================================
// Initial-scene-load callback
//=============================================================================

void Zenith_SceneLifecycleScheduler::SetInitialSceneLoadCallback(InitialSceneLoadFn pfn)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetInitialSceneLoadCallback must be called from main thread");
	s_pfnInitialSceneLoad = pfn;
}

Zenith_SceneLifecycleScheduler::InitialSceneLoadFn Zenith_SceneLifecycleScheduler::GetInitialSceneLoadCallback()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetInitialSceneLoadCallback must be called from main thread");
	return s_pfnInitialSceneLoad;
}

//=============================================================================
// Fixed timestep
//=============================================================================

void Zenith_SceneLifecycleScheduler::SetFixedTimestep(float fTimestep)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetFixedTimestep must be called from main thread");
	Zenith_Assert(fTimestep > 0.0f, "Fixed timestep must be positive");
	s_fFixedTimestep = fTimestep;
}

float Zenith_SceneLifecycleScheduler::GetFixedTimestep()
{
	return s_fFixedTimestep;
}

//=============================================================================
// Test-harness hook
//=============================================================================

#ifdef ZENITH_TESTING
void Zenith_SceneLifecycleScheduler::DispatchFullLifecycleInit()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "DispatchFullLifecycleInit must be called from main thread");
	for (u_int i = 0; i < Zenith_SceneRegistry::s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = Zenith_SceneRegistry::s_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded)
		{
			pxData->DispatchLifecycleForNewScene();
		}
	}
}
#endif
