#include "Zenith.h"
#include "TaskSystem/Zenith_TaskSystem.h"

#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"

#include <algorithm>

//=============================================================================
// Zenith_SceneSystem — Update pipeline + lifecycle state + RAII guards +
// bootstrap. State accessed via g_xEngine.Scenes().m_xXxx.
//=============================================================================

//=============================================================================
// Bootstrap orchestrators (was Zenith_SceneSystem.cpp)
//=============================================================================

void Zenith_SceneSystem::InitialiseSubsystems()
{
	g_xEngine.Scenes().Initialise();

	// Create the persistent ("DontDestroyOnLoad") scene WITHOUT auto-activating.
	// It's a container for DontDestroyOnLoad entities, not a user-visible scene.
	Zenith_Scene xPersistent = g_xEngine.Scenes().CreateEmptyScene("DontDestroyOnLoad", /*bAllowSetActive=*/false);
	g_xEngine.Scenes().m_iPersistentSceneHandle = xPersistent.m_iHandle;

	Zenith_Log(LOG_CATEGORY_SCENE, "Scene system initialised with persistent scene (handle=%d)",
		g_xEngine.Scenes().m_iPersistentSceneHandle);
}

void Zenith_SceneSystem::ShutdownSubsystems()
{
	g_xEngine.Scenes().Shutdown();
	g_xEngine.EntityStore().Reset();
}

#ifdef ZENITH_TESTING
void Zenith_SceneSystem::ResetForNextTest()
{
	auto& xScn = g_xEngine.Scenes();
	xScn.m_bIsLoadingScene = false;
	xScn.m_bIsUpdating     = false;
	xScn.m_uUnloadUnusedAssetsCallCount = 0;

	ShutdownSubsystems();
	InitialiseSubsystems();

	// Editor / AI / physics tests assume an active scene exists. Provide one
	// up-front so individual tests don't repeat the boilerplate.
	Zenith_Scene xActive = g_xEngine.Scenes().CreateEmptyScene("TestHarnessDefaultScene");
	g_xEngine.Scenes().m_iActiveSceneHandle = xActive.m_iHandle;
}
#endif

bool& Zenith_SceneSystem::MutableLifecycleLoadingFlagForGuard()
{
	return g_xEngine.Scenes().m_bIsLoadingScene;
}

//=============================================================================
// RAII guard bodies (was Zenith_SceneSystemGuards.cpp)
//=============================================================================

Zenith_PrefabInstantiationGuard::Zenith_PrefabInstantiationGuard()
	: m_bPrevValue(g_xEngine.Scenes().m_bIsPrefabInstantiating)
{
	g_xEngine.Scenes().m_bIsPrefabInstantiating = true;
}

Zenith_PrefabInstantiationGuard::~Zenith_PrefabInstantiationGuard()
{
	g_xEngine.Scenes().m_bIsPrefabInstantiating = m_bPrevValue;
}

Zenith_SceneUpdateDeferralGuard::Zenith_SceneUpdateDeferralGuard()
	: m_bPrevValue(g_xEngine.Scenes().m_bIsUpdating)
{
	g_xEngine.Scenes().m_bIsUpdating = true;
}

Zenith_SceneUpdateDeferralGuard::~Zenith_SceneUpdateDeferralGuard()
{
	auto& xScn = g_xEngine.Scenes();
	xScn.m_bIsUpdating = m_bPrevValue;

	// Only the top-level guard drains — nested guards leave the request for
	// the outermost scope to flush, otherwise an inner UI iteration would
	// tear down its own canvas.
	if (!xScn.m_bIsUpdating)
		xScn.DrainPendingLoadIfAny();
}

Zenith_SceneCreationTargetScope::Zenith_SceneCreationTargetScope(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"Zenith_SceneCreationTargetScope must be constructed on the main thread");
	g_xEngine.Scenes().m_axCreationTargetStack.PushBack(xScene);
}

Zenith_SceneCreationTargetScope::~Zenith_SceneCreationTargetScope()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"Zenith_SceneCreationTargetScope must be destroyed on the main thread");
	Zenith_Assert(g_xEngine.Scenes().m_axCreationTargetStack.GetSize() > 0,
		"Zenith_SceneCreationTargetScope: creation-target stack underflow on destruction");
	g_xEngine.Scenes().m_axCreationTargetStack.PopBack();
}

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
		for (u_int i = 0; i < g_xEngine.Scenes().m_axScenes.GetSize(); ++i)
		{
			Zenith_SceneData* pxData = g_xEngine.Scenes().m_axScenes.Get(i);
			if (g_xEngine.Scenes().IsSceneUpdatable(pxData))
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

void Zenith_SceneSystem::Initialise()
{
	Zenith_Assert(g_pxAnimUpdateTask == nullptr, "SceneLifecycleScheduler::Initialise called twice without Shutdown");
	g_pxAnimUpdateTask = new Zenith_TaskArray(ZENITH_PROFILE_INDEX__ANIMATION, AnimUpdateTask, nullptr, 4, true);
}

void Zenith_SceneSystem::Shutdown()
{
	if (g_pxAnimUpdateTask)
	{
		g_pxAnimUpdateTask->WaitUntilComplete();
		delete g_pxAnimUpdateTask;
		g_pxAnimUpdateTask = nullptr;
	}
	g_xAnimationsToUpdate.Clear();

	auto& xLC = g_xEngine.Scenes();
	xLC.m_bIsLoadingScene = false;
	xLC.m_bIsPrefabInstantiating = false;
	xLC.m_bIsUpdating = false;
	xLC.m_fFixedTimeAccumulator = 0.0f;
	// m_fFixedTimestep intentionally NOT reset — it's a config knob, not transient state.
	xLC.m_axCurrentlyLoadingPaths.Clear();
	xLC.m_axLifecycleLoadStack.Clear();
	xLC.m_iPendingBuildIndex = -1;
	xLC.m_axCreationTargetStack.Clear();
	xLC.m_bIsMainLoopRunning = false;
	xLC.m_pfnInitialSceneLoad = nullptr;
	// Drop any deferred LoadScene/LoadSceneByIndex request — if Shutdown or
	// ResetForNextTest runs while a load is stashed, executing it later would
	// fire the stale request inside the next session/test against scrubbed
	// scene state. The drain-on-guard-destructor path can't reach it once the
	// scene system has been torn down.
	xLC.m_xPendingLoad.m_bSet = false;
	xLC.m_xPendingLoad.m_bByIndex = false;
	xLC.m_xPendingLoad.m_iBuildIndex = -1;
	xLC.m_xPendingLoad.m_strPath.clear();
	xLC.m_xPendingLoad.m_uMode = 0;

	// Callback bus state (was Zenith_SceneCallbackBus::Shutdown).
	xLC.m_xActiveSceneChangedCallbacks.m_axEntries.Clear();
	xLC.m_xSceneLoadedCallbacks.m_axEntries.Clear();
	xLC.m_xSceneUnloadingCallbacks.m_axEntries.Clear();
	xLC.m_xSceneUnloadedCallbacks.m_axEntries.Clear();
	xLC.m_axCallbacksPendingRemoval.Clear();
	xLC.m_uFiringCallbacksDepth = 0;
	xLC.m_bSuppressActiveSceneChanged = false;
	xLC.m_bHaveDeferredOldActive = false;

	// Scene-slot table (was Zenith_SceneRegistry::Shutdown). Must come LAST —
	// every other Shutdown step touches scene-data pointers that this loop
	// deletes.
	for (u_int i = 0; i < xLC.m_axScenes.GetSize(); ++i)
	{
		delete xLC.m_axScenes.Get(i);
	}
	xLC.m_axScenes.Clear();
	xLC.m_axSceneGenerations.Clear();
	xLC.m_axFreeHandles.Clear();
	xLC.m_axBuildIndexToPath.Clear();
	xLC.m_axLoadedSceneNames.Clear();
	xLC.m_iActiveSceneHandle = -1;
	xLC.m_iPersistentSceneHandle = -1;
	xLC.m_ulNextLoadTimestamp = 1;
}

void Zenith_SceneSystem::DrainPendingLoadIfAny()
{
	auto& xLC = g_xEngine.Scenes();
	if (!xLC.m_xPendingLoad.m_bSet)
		return;
	// Snapshot + clear BEFORE the call so a nested deferred load from a
	// SceneLoaded subscriber can re-populate the slot.
	const bool bByIndex   = xLC.m_xPendingLoad.m_bByIndex;
	const int  iIndex     = xLC.m_xPendingLoad.m_iBuildIndex;
	const std::string strPath = xLC.m_xPendingLoad.m_strPath;
	const Zenith_SceneLoadMode eMode = static_cast<Zenith_SceneLoadMode>(xLC.m_xPendingLoad.m_uMode);
	xLC.m_xPendingLoad.m_bSet        = false;
	xLC.m_xPendingLoad.m_bByIndex    = false;
	xLC.m_xPendingLoad.m_iBuildIndex = -1;
	xLC.m_xPendingLoad.m_strPath.clear();

	if (bByIndex)
		g_xEngine.Scenes().LoadSceneByIndex(iIndex, eMode);
	else
		g_xEngine.Scenes().LoadScene(strPath, eMode);
}

//=============================================================================
// Update pipeline
//=============================================================================

void Zenith_SceneSystem::Update(float fDt)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Update must be called from main thread");

	// Scene async pipeline removed; LoadScene runs inline. Nothing to drain here.

	g_xEngine.Scenes().m_bIsUpdating = true;

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
	g_xEngine.Scenes().m_fFixedTimeAccumulator += std::min(fDt, fMAX_FIXED_DT);
	while (g_xEngine.Scenes().m_fFixedTimeAccumulator >= g_xEngine.Scenes().m_fFixedTimestep)
	{
		for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
		{
			axUpdatable.Get(i)->FixedUpdate(g_xEngine.Scenes().m_fFixedTimestep);
		}
		g_xEngine.Scenes().m_fFixedTimeAccumulator -= g_xEngine.Scenes().m_fFixedTimestep;
	}

	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		axUpdatable.Get(i)->Update(fDt);
	}

	// Animation Update (parallel task system).
	g_xAnimationsToUpdate.Clear();
	for (u_int i = 0; i < g_xEngine.Scenes().m_axScenes.GetSize(); ++i)
	{
		CollectAnimationsFromScene(g_xEngine.Scenes().m_axScenes.Get(i));
	}

	g_xEngine.Scenes().m_bIsUpdating = false;

	// Drain any LoadScene/LoadSceneByIndex stashed by script OnUpdate calls.
	// Bypassing the SceneUpdateDeferralGuard (which fires its own drain on
	// destructor) means we have to do it ourselves — otherwise headless or
	// no-render frames silently strand the pending load.
	DrainPendingLoadIfAny();

	if (g_pxAnimUpdateTask)
	{
#ifdef ZENITH_ASSERT
		g_xEngine.Scenes().SetAnimTasksActive(true);
#endif
		g_xEngine.Tasks().SubmitTaskArray(g_pxAnimUpdateTask);
	}
}

void Zenith_SceneSystem::WaitForUpdateComplete()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "WaitForUpdateComplete must be called from main thread");

	if (g_pxAnimUpdateTask)
	{
		g_pxAnimUpdateTask->WaitUntilComplete();
#ifdef ZENITH_ASSERT
		g_xEngine.Scenes().SetAnimTasksActive(false);
#endif
	}
}

//=============================================================================
// Circular-load detection
//=============================================================================

void Zenith_SceneSystem::PushLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "PushLifecycleContext must be called from main thread");
	g_xEngine.Scenes().m_axLifecycleLoadStack.PushBack(strCanonicalPath);
}

void Zenith_SceneSystem::PopLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "PopLifecycleContext must be called from main thread");
	g_xEngine.Scenes().m_axLifecycleLoadStack.EraseValue(strCanonicalPath);
}

bool Zenith_SceneSystem::IsCircularLoadDependency(const std::string& strCanonicalPath)
{
	for (u_int i = 0; i < g_xEngine.Scenes().m_axCurrentlyLoadingPaths.GetSize(); ++i)
	{
		if (g_xEngine.Scenes().m_axCurrentlyLoadingPaths.Get(i) == strCanonicalPath) return true;
	}
	for (u_int i = 0; i < g_xEngine.Scenes().m_axLifecycleLoadStack.GetSize(); ++i)
	{
		if (g_xEngine.Scenes().m_axLifecycleLoadStack.Get(i) == strCanonicalPath) return true;
	}
	return false;
}

//=============================================================================
// Initial-scene-load callback
//=============================================================================

void Zenith_SceneSystem::SetInitialSceneLoadCallback(InitialSceneLoadFn pfn)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetInitialSceneLoadCallback must be called from main thread");
	g_xEngine.Scenes().m_pfnInitialSceneLoad = pfn;
}

Zenith_SceneSystem::InitialSceneLoadFn Zenith_SceneSystem::GetInitialSceneLoadCallback()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetInitialSceneLoadCallback must be called from main thread");
	return g_xEngine.Scenes().m_pfnInitialSceneLoad;
}

//=============================================================================
// Fixed timestep
//=============================================================================

void Zenith_SceneSystem::SetFixedTimestep(float fTimestep)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetFixedTimestep must be called from main thread");
	Zenith_Assert(fTimestep > 0.0f, "Fixed timestep must be positive");
	g_xEngine.Scenes().m_fFixedTimestep = fTimestep;
}

float Zenith_SceneSystem::GetFixedTimestep()
{
	return g_xEngine.Scenes().m_fFixedTimestep;
}

//=============================================================================
// Default creation target + main-loop flag.
//=============================================================================

Zenith_Scene Zenith_SceneSystem::GetDefaultCreationScene()
{
	const u_int uDepth = m_axCreationTargetStack.GetSize();
	if (uDepth > 0)
	{
		return m_axCreationTargetStack.Get(uDepth - 1);
	}
	return g_xEngine.Scenes().GetActiveScene();
}

void Zenith_SceneSystem::SetMainLoopRunning(bool bRunning)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetMainLoopRunning must be called from main thread");
	m_bIsMainLoopRunning = bRunning;
}

//=============================================================================
// Test-harness hook
//=============================================================================

#ifdef ZENITH_TESTING
void Zenith_SceneSystem::DispatchFullLifecycleInit()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "DispatchFullLifecycleInit must be called from main thread");
	for (u_int i = 0; i < g_xEngine.Scenes().m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = g_xEngine.Scenes().m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded())
		{
			pxData->DispatchLifecycleForNewScene();
		}
	}
}
#endif

//=============================================================================
// Free function declared in Zenith_RenderTaskState.h — forwards to the
// scheduler's flag. Used by SceneData.h template asserts that need to know
// whether render tasks are reading scene storage.
//=============================================================================

#include "EntityComponent/Zenith_RenderTaskState.h"
bool Zenith_AreRenderTasksActive() { return g_xEngine.Scenes().AreRenderTasksActive(); }

//=============================================================================
// Zenith_SceneLifecycleContext — context-query namespace consumed by
// Zenith_SceneSystem's re-entrancy guards. Declared in
// Internal/Zenith_SceneLifecycleContext.h.
//=============================================================================

namespace Zenith_SceneLifecycleContext
{
	bool IsLoadingScene()        { return g_xEngine.Scenes().m_bIsLoadingScene; }
	bool IsPrefabInstantiating() { return g_xEngine.Scenes().m_bIsPrefabInstantiating; }
	bool IsUpdating()            { return g_xEngine.Scenes().m_bIsUpdating; }
	bool IsMainLoopRunning()     { return g_xEngine.Scenes().m_bIsMainLoopRunning; }
	int  GetPendingBuildIndex()  { return g_xEngine.Scenes().m_iPendingBuildIndex; }
	bool IsCircularLoadDependency(const std::string& strCanonicalPath)
	{
		return g_xEngine.Scenes().IsCircularLoadDependency(strCanonicalPath);
	}
	bool IsActiveSceneSuppressed() { return g_xEngine.Scenes().IsActiveSceneSuppressed(); }
	Zenith_Scene GetCurrentCreationTarget()
	{
		const u_int uDepth = g_xEngine.Scenes().m_axCreationTargetStack.GetSize();
		if (uDepth == 0) return Zenith_Scene::INVALID_SCENE;
		return g_xEngine.Scenes().m_axCreationTargetStack.Get(uDepth - 1);
	}
}
