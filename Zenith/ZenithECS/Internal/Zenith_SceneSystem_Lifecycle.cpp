#include "Zenith.h"

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Entity.h"

#include <algorithm>

//=============================================================================
// Zenith_SceneSystem — Update pipeline + lifecycle state + RAII guards +
// bootstrap.
//
// Instance methods reach their own members directly (this == &Zenith_SceneSystem::Get()).
// The static bootstrap methods and the non-member RAII guards have no `this`,
// so they reach the singleton through Zenith_SceneSystem::Get().
//=============================================================================

//=============================================================================
// Bootstrap orchestrators
//=============================================================================

void Zenith_SceneSystem::InitialiseSubsystems()
{
	// Create the persistent ("DontDestroyOnLoad") scene WITHOUT auto-activating.
	// It's a container for DontDestroyOnLoad entities, not a user-visible scene.
	Zenith_Scene xPersistent = Zenith_SceneSystem::Get().AllocateEmptyScene("DontDestroyOnLoad", /*bAllowSetActive=*/false);
	Zenith_SceneSystem::Get().m_iPersistentSceneHandle = xPersistent.m_iHandle;

	Zenith_Log(LOG_CATEGORY_SCENE, "Scene system initialised with persistent scene (handle=%d)",
		Zenith_SceneSystem::Get().m_iPersistentSceneHandle);
}

void Zenith_SceneSystem::ShutdownSubsystems()
{
	Zenith_SceneSystem::Get().Shutdown();
	Zenith_ECS_EntityStore().Reset();
}

#ifdef ZENITH_TESTING
void Zenith_SceneSystem::ResetForNextTest()
{
	auto& xScn = Zenith_SceneSystem::Get();
	xScn.m_bIsLoadingScene = false;
	xScn.m_bIsUpdating     = false;

	ShutdownSubsystems();
	InitialiseSubsystems();

	// Editor / AI / physics tests assume an active scene exists. Provide one
	// up-front so individual tests don't repeat the boilerplate.
	Zenith_Scene xActive = Zenith_SceneSystem::Get().AllocateEmptyScene("TestHarnessDefaultScene");
	Zenith_SceneSystem::Get().m_iActiveSceneHandle = xActive.m_iHandle;
}
#endif

namespace
{
	// Sorts a Zenith_Vector<uint32_t>/<int> free list DESCENDING and drops any
	// entry the caller marks retired. Both allocators pop from the BACK, so a
	// descending list hands indices out in ascending order -- the property that
	// makes slot allocation replay identically for every test without ever
	// replaying a generation.
	template<typename T, typename FnIsRetired>
	void NormaliseFreeListDescending(Zenith_Vector<T>& axFreeList, FnIsRetired fnIsRetired)
	{
		Zenith_Vector<T> axKept;
		for (u_int i = 0; i < axFreeList.GetSize(); ++i)
		{
			if (!fnIsRetired(axFreeList.Get(i))) axKept.PushBack(axFreeList.Get(i));
		}
		if (axKept.GetSize() > 1)
		{
			std::sort(axKept.GetDataPointer(), axKept.GetDataPointer() + axKept.GetSize(),
				[](const T& xA, const T& xB) { return xA > xB; });
		}
		axFreeList = axKept;
	}

	// RAII so the flag cannot survive an early return added later.
	struct ResettingWorldGuard
	{
		explicit ResettingWorldGuard(bool& bFlag) : m_bFlag(bFlag) { m_bFlag = true; }
		~ResettingWorldGuard() { m_bFlag = false; }
		bool& m_bFlag;
	};
}

void Zenith_SceneSystem::ResetWorldForNextTest()
{
	Zenith_SceneSystem& xScn = Zenith_SceneSystem::Get();

	// Same preconditions LoadScene enforces: this tears down every scene, so it
	// must not race a render task reading component storage.
	Zenith_Assert(Zenith_ECS_IsMainThread(), "ResetWorldForNextTest must be called from the main thread");
	Zenith_Assert(!xScn.m_bRenderTasksActive,
		"ResetWorldForNextTest: scene mutation while render tasks are reading — render-task invariant violated");

	if (xScn.m_bIsResettingWorld)
	{
		Zenith_Assert(false,
			"ResetWorldForNextTest: nested reset rejected — a reset cannot be driven from an "
			"OnDestroy body running inside another reset");
		return;
	}
	ResettingWorldGuard xGuard(xScn.m_bIsResettingWorld);

	// -- 1. Render systems FIRST -------------------------------------------
	// Preserves today's teardown invariant: SCENE_LOAD_SINGLE resets the render
	// systems BEFORE unloading scenes, so component destructors always run
	// against reset render state. The boot-scene load that follows runs the same
	// reset a second time; the reset bodies are wholesale clears, and
	// FluxDecals::ResetIsIdempotent pins that a double fire is a no-op.
	xScn.ResetAllRenderSystems();

	// -- 2. Gameplay scenes ------------------------------------------------
	// Normal SINGLE semantics: gameplay scenes tear down while the persistent
	// managers they may reference are still alive.
	xScn.UnloadAllNonPersistent();

	// -- 3. The persistent scene -------------------------------------------
	// This is the step no production path performs. Destruction goes through
	// UnloadOneScene -> ~Zenith_SceneData -> Reset(), whose OnDisable->OnDestroy
	// two-pass the SceneData destructor explicitly blesses as a legitimate route
	// for the persistent scene. (ScrubAndReset must NOT be used -- it asserts
	// against the persistent scene.)
	const int iPersistentHandle = xScn.m_iPersistentSceneHandle;
	if (iPersistentHandle >= 0
		&& iPersistentHandle < static_cast<int>(xScn.m_axScenes.GetSize())
		&& xScn.m_axScenes.Get(iPersistentHandle) != nullptr)
	{
		const Zenith_Scene xPersistent = xScn.GetSceneFromHandle(iPersistentHandle);
		bool bActiveSceneUnloaded = false;
		xScn.UnloadOneScene(xPersistent, bActiveSceneUnloaded);
		if (bActiveSceneUnloaded)
		{
			xScn.UpdateActiveSceneAfterUnload();
		}
	}
	// Neither UnloadOneScene nor FreeSceneHandle clears this — only Shutdown
	// does. Left dangling, the eight "is this the persistent scene?" handle
	// comparisons scattered through the system would treat whichever scene next
	// recycles that slot as the persistent one (un-unloadable, skipped by
	// UnloadAllNonPersistent).
	xScn.m_iPersistentSceneHandle = -1;

	Zenith_Assert(xScn.m_iActiveSceneHandle == -1,
		"ResetWorldForNextTest: an active scene survived the teardown (handle=%d)",
		xScn.m_iActiveSceneHandle);

	// -- 4. Transient lifecycle state --------------------------------------
	// A PRESERVE list, not "Shutdown minus exceptions": m_axBuildIndexToPath
	// (the harness reloads by build index straight after), m_xRuntimeHooks,
	// m_fFixedTimestep, m_uRenderMutationEpoch (monotonic — resetting it to 1
	// would make a stale render snapshot compare CURRENT and dereference freed
	// instances), m_bIsMainLoopRunning (the main loop IS still running),
	// m_ulNextLoadTimestamp (monotonic; SelectNewActiveScene's tie-break),
	// m_bUseSparseQueryReads (a pin-and-restore test contract), m_axScenes /
	// m_axSceneGenerations (see step 6) and the entity store all stay.
	xScn.m_fFixedTimeAccumulator = 0.0f;
	xScn.m_axCurrentlyLoadingPaths.Clear();
	xScn.m_axLifecycleLoadStack.Clear();
	xScn.m_iPendingBuildIndex = -1;
	xScn.m_bIsPrefabInstantiating = false;
	if (xScn.m_xPendingLoad.m_bSet)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"ResetWorldForNextTest: dropping a scene-load request stashed before the reset "
			"(byIndex=%d index=%d path='%s') — it targets a world that no longer exists",
			xScn.m_xPendingLoad.m_bByIndex ? 1 : 0,
			xScn.m_xPendingLoad.m_iBuildIndex,
			xScn.m_xPendingLoad.m_strPath.c_str());
	}
	xScn.m_xPendingLoad.m_bSet        = false;
	xScn.m_xPendingLoad.m_bByIndex    = false;
	xScn.m_xPendingLoad.m_iBuildIndex = -1;
	xScn.m_xPendingLoad.m_strPath.clear();
	xScn.m_xPendingLoad.m_uMode       = 0;

	// -- 5. Entity slots ---------------------------------------------------
	// Scene teardown ALREADY released every live slot (each ~Zenith_SceneData ->
	// Reset() -> FreeGlobalSlotsForActiveEntities, which performs the monotonic
	// generation + m_uHierRevision bumps). So there is nothing to free here --
	// a survivor would be an engine bug, and asserting says so instead of
	// silently papering over it with a bulk wipe.
	{
		Zenith_EntityStore& xStore = Zenith_ECS_EntityStore();
		u_int uOccupied = 0;
		for (u_int i = 0; i < xStore.m_axEntitySlots.GetSize(); ++i)
		{
			if (xStore.m_axEntitySlots.Get(i).IsOccupied()) ++uOccupied;
		}
		Zenith_Assert(uOccupied == 0,
			"ResetWorldForNextTest: %u entity slot(s) still occupied after every scene was destroyed",
			uOccupied);

		// Retired slots (generation saturated at UINT32_MAX) are dropped from the
		// free list entirely -- CreateEntity would pop and discard them anyway, and
		// leaving them in would make the allocation sequence depend on how many
		// slots had saturated.
		NormaliseFreeListDescending(xStore.m_axFreeEntityIndices,
			[&xStore](uint32_t uIndex)
			{
				return uIndex < xStore.m_axEntitySlots.GetSize()
					&& xStore.m_axEntitySlots.Get(uIndex).m_uGeneration == UINT32_MAX;
			});
	}

	// Scene handles: same normalisation, same reason. Scene slots retire at FREE
	// time rather than allocate time (FreeSceneHandle never re-pushes a saturated
	// handle), so a retired scene handle is simply absent from the list already.
	// m_axScenes / m_axSceneGenerations are NEVER Clear()'d: AllocateSceneHandle
	// assumes they stay size-synced and pushes a hardcoded generation 1 on the
	// fresh-slot path, so clearing either desyncs the allocator and breaks
	// monotonicity.
	NormaliseFreeListDescending(xScn.m_axFreeHandles, [](int) { return false; });

	// -- 6. Re-create the empty persistent scene ---------------------------
	// Pops the just-normalised free list, so the slot is deterministic and its
	// generation is the preserved monotonic counter -- never a replayed 1.
	InitialiseSubsystems();
}

bool& Zenith_SceneSystem::MutableLifecycleLoadingFlagForGuard()
{
	return m_bIsLoadingScene;
}

//=============================================================================
// RAII guard bodies. These are free-standing types (not members of
// Zenith_SceneSystem), so they reach the singleton's friended private flags
// through Zenith_SceneSystem::Get().
//=============================================================================

Zenith_PrefabInstantiationGuard::Zenith_PrefabInstantiationGuard()
	: m_bPrevValue(Zenith_SceneSystem::Get().m_bIsPrefabInstantiating)
{
	Zenith_SceneSystem::Get().m_bIsPrefabInstantiating = true;
}

Zenith_PrefabInstantiationGuard::~Zenith_PrefabInstantiationGuard()
{
	Zenith_SceneSystem::Get().m_bIsPrefabInstantiating = m_bPrevValue;
}

Zenith_SceneUpdateDeferralGuard::Zenith_SceneUpdateDeferralGuard()
	: m_bPrevValue(Zenith_SceneSystem::Get().m_bIsUpdating)
{
	Zenith_SceneSystem::Get().m_bIsUpdating = true;
}

Zenith_SceneUpdateDeferralGuard::~Zenith_SceneUpdateDeferralGuard()
{
	auto& xScn = Zenith_SceneSystem::Get();
	xScn.m_bIsUpdating = m_bPrevValue;

	// Only the top-level guard drains — nested guards leave the request for
	// the outermost scope to flush, otherwise an inner UI iteration would
	// tear down its own canvas.
	if (!xScn.m_bIsUpdating)
		xScn.DrainPendingLoadIfAny();
}

//=============================================================================
// Lifecycle
//=============================================================================

void Zenith_SceneSystem::Shutdown()
{
	m_bIsLoadingScene = false;
	m_bIsPrefabInstantiating = false;
	m_bIsUpdating = false;
	m_fFixedTimeAccumulator = 0.0f;
	// m_fFixedTimestep intentionally NOT reset — it's a config knob, not transient state.
	m_axCurrentlyLoadingPaths.Clear();
	m_axLifecycleLoadStack.Clear();
	m_iPendingBuildIndex = -1;
	m_bIsMainLoopRunning = false;
	// Drop any deferred LoadScene/LoadSceneByIndex request — if Shutdown or
	// ResetForNextTest runs while a load is stashed, executing it later would
	// fire the stale request inside the next session/test against scrubbed
	// scene state. The drain-on-guard-destructor path can't reach it once the
	// scene system has been torn down.
	m_xPendingLoad.m_bSet = false;
	m_xPendingLoad.m_bByIndex = false;
	m_xPendingLoad.m_iBuildIndex = -1;
	m_xPendingLoad.m_strPath.clear();
	m_xPendingLoad.m_uMode = 0;

	// (Phase 7b-2: the scene-callback lists / handle allocator / pending-removal
	// queue / firing-depth counter are gone — scene-lifecycle dispatch is now
	// Zenith_EventDispatcher, which is a process-level singleton not owned by the
	// scene system, so Shutdown does NOT touch its subscriptions. There are no
	// production scene-event subscribers to clear regardless. The active-scene
	// suppression flags this block once reset were removed with their scope.)

	// Scene-slot table. Must come LAST — every other Shutdown step touches
	// scene-data pointers that this loop deletes.
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		if (m_axScenes.Get(i) == nullptr) continue;
		// Same destruction notification UnloadOneScene fires, so scene-owned
		// state outside the ECS is torn down on the shutdown path too rather
		// than only on the unload path.
		if (m_xRuntimeHooks.m_pfnSceneDestroyed)
		{
			m_xRuntimeHooks.m_pfnSceneDestroyed(GetSceneFromHandle(static_cast<int>(i)));
		}
		delete m_axScenes.Get(i);
		m_axScenes.Get(i) = nullptr;
	}
	m_axScenes.Clear();
	m_axSceneGenerations.Clear();
	m_axFreeHandles.Clear();
	m_axBuildIndexToPath.Clear();
	m_axLoadedSceneNames.Clear();
	m_iActiveSceneHandle = -1;
	m_iPersistentSceneHandle = -1;
	m_ulNextLoadTimestamp = 1;
}

void Zenith_SceneSystem::DrainPendingLoadIfAny()
{
	if (!m_xPendingLoad.m_bSet)
		return;
	// Covers all four drain sites at once: a guard destructor firing inside the
	// world reset must not resurrect a request aimed at the world being torn
	// down. The reset clears the slot itself, so this is the window between a
	// mid-reset stash and that clear.
	if (m_bIsResettingWorld)
		return;
	// Snapshot + clear BEFORE the call so a nested deferred load triggered during
	// the drained load's lifecycle dispatch can re-populate the slot.
	const bool bByIndex   = m_xPendingLoad.m_bByIndex;
	const int  iIndex     = m_xPendingLoad.m_iBuildIndex;
	const std::string strPath = m_xPendingLoad.m_strPath;
	const Zenith_SceneLoadMode eMode = static_cast<Zenith_SceneLoadMode>(m_xPendingLoad.m_uMode);
	m_xPendingLoad.m_bSet        = false;
	m_xPendingLoad.m_bByIndex    = false;
	m_xPendingLoad.m_iBuildIndex = -1;
	m_xPendingLoad.m_strPath.clear();

	if (bByIndex)
		LoadSceneByIndex(iIndex, eMode);
	else
		LoadScene(strPath, eMode);
}

//=============================================================================
// Update pipeline
//=============================================================================

void Zenith_SceneSystem::Update(float fDt)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "Update must be called from main thread");

	// Save/restore (rather than unconditional false) so a future nested Update
	// can't prematurely clear an outer pass's updating flag.
	const bool bPrevUpdating = m_bIsUpdating;
	m_bIsUpdating = true;

	// E.20 (finding 3.21): collect updatable scenes once per frame - as
	// handle+generation pairs, NOT SceneData pointers. Dispatched user logic
	// below (component updates, behaviour-graph chains) can synchronously
	// UnloadScene a scene that sits later in this snapshot (e.g. a game-flow
	// update unloading its additive level scene); UnloadOneScene deletes the
	// SceneData immediately, so a pointer snapshot would hand freed memory to
	// the loops below. Each loop re-resolves the handle and skips freed slots
	// (nulled) and same-frame slot reuse (generation bump). NOTE: a scene
	// synchronously unloading ITSELF from inside its own dispatch remains
	// unsupported (the SceneData whose Update frame is on the stack would be
	// freed) - self-teardown goes through the deferred SINGLE-load path.
	struct UpdatableScene
	{
		int m_iHandle = -1;
		uint32_t m_uGeneration = 0;
	};
	Zenith_Vector<UpdatableScene> axUpdatable;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (IsSceneUpdatable(pxData))
		{
			UpdatableScene xEntry;
			xEntry.m_iHandle = static_cast<int>(i);
			xEntry.m_uGeneration = m_axSceneGenerations.Get(i);
			axUpdatable.PushBack(xEntry);
		}
	}

	// nullptr = unloaded (or its slot reused) by an earlier dispatch this frame.
	auto ResolveUpdatable = [this](const UpdatableScene& xEntry) -> Zenith_SceneData*
	{
		Zenith_SceneData* pxData = m_axScenes.Get(xEntry.m_iHandle);
		if (pxData == nullptr || m_axSceneGenerations.Get(xEntry.m_iHandle) != xEntry.m_uGeneration)
		{
			return nullptr;
		}
		return pxData;
	};

	// HIGH-1: Unity execution order is Awake -> OnEnable -> Start -> FixedUpdate
	// -> Update -> LateUpdate. Flush pending Starts BEFORE the FixedUpdate
	// accumulator.
	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		if (Zenith_SceneData* pxData = ResolveUpdatable(axUpdatable.Get(i)))
		{
			pxData->DispatchPendingStarts();
		}
	}

	// Fixed-timestep accumulation.
	static constexpr float fMAX_FIXED_DT = 0.333f;
	m_fFixedTimeAccumulator += std::min(fDt, fMAX_FIXED_DT);
	while (m_fFixedTimeAccumulator >= m_fFixedTimestep)
	{
		for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
		{
			if (Zenith_SceneData* pxData = ResolveUpdatable(axUpdatable.Get(i)))
			{
				pxData->FixedUpdate(m_fFixedTimestep);
			}
		}
		m_fFixedTimeAccumulator -= m_fFixedTimestep;
	}

	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		if (Zenith_SceneData* pxData = ResolveUpdatable(axUpdatable.Get(i)))
		{
			pxData->Update(fDt);
		}
	}

	// Skeletal animation is dispatched by Zenith_AnimatorComponent::OnUpdate
	// above — there is no separate scene-driven animation task.

	m_bIsUpdating = bPrevUpdating;

	// Drain any LoadScene/LoadSceneByIndex stashed by script OnUpdate calls.
	// Only the outermost pass drains (mirrors Zenith_SceneUpdateDeferralGuard),
	// otherwise headless or no-render frames would silently strand the load.
	if (!m_bIsUpdating)
	{
		DrainPendingLoadIfAny();
	}
}

//=============================================================================
// Circular-load detection
//=============================================================================

void Zenith_SceneSystem::PushLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "PushLifecycleContext must be called from main thread");
	m_axLifecycleLoadStack.PushBack(strCanonicalPath);
}

void Zenith_SceneSystem::PopLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "PopLifecycleContext must be called from main thread");
	m_axLifecycleLoadStack.EraseValue(strCanonicalPath);
}

bool Zenith_SceneSystem::IsCircularLoadDependency(const std::string& strCanonicalPath)
{
	for (u_int i = 0; i < m_axCurrentlyLoadingPaths.GetSize(); ++i)
	{
		if (m_axCurrentlyLoadingPaths.Get(i) == strCanonicalPath) return true;
	}
	for (u_int i = 0; i < m_axLifecycleLoadStack.GetSize(); ++i)
	{
		if (m_axLifecycleLoadStack.Get(i) == strCanonicalPath) return true;
	}
	return false;
}

//=============================================================================
// Main-loop flag.
//=============================================================================

void Zenith_SceneSystem::SetMainLoopRunning(bool bRunning)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetMainLoopRunning must be called from main thread");
	m_bIsMainLoopRunning = bRunning;
}

//=============================================================================
// Test-harness hook
//=============================================================================

#ifdef ZENITH_TESTING
void Zenith_SceneSystem::DispatchFullLifecycleInit()
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "DispatchFullLifecycleInit must be called from main thread");
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded())
		{
			pxData->DispatchLifecycleForNewScene();
		}
	}
}
#endif

//=============================================================================
// Free function declared in Zenith_RenderTaskState.h — forwards to the
// scene system's flag. Used by SceneData.h template asserts that need to know
// whether render tasks are reading scene storage.
//=============================================================================

#include "ZenithECS/Internal/Zenith_RenderTaskState.h"
bool Zenith_AreRenderTasksActive() { return Zenith_SceneSystem::Get().AreRenderTasksActive(); }

// Leaf forwarder to the SceneSystem-owned entity store (declared in
// Zenith_RenderTaskState.h). Uses Get() directly -- NOT g_xEngine -- so the ECS
// core can reach the store without naming the engine singleton. Returns exactly
// the instance Zenith_SceneSystem owns (same one the old g_xEngine.EntityStore()
// forwarder returns).
Zenith_EntityStore& Zenith_ECS_EntityStore() { return Zenith_SceneSystem::Get().GetEntityStore(); }

// WS10: forwards the sparse-set query read toggle to Zenith_Query.h without the
// header cycle (see Zenith_RenderTaskState.h for the rationale).
bool Zenith_AreSparseQueryReadsEnabled() { return Zenith_SceneSystem::Get().AreSparseQueryReadsEnabled(); }

// Main-thread predicate for the ECS-leaf thread-affinity asserts (Zenith_Query.h,
// Zenith_EventSystem.h). Forwards to the installed ECS runtime hook; returns true
// when no hook is installed. Phase 2.2 repointed this off g_xEngine onto the
// ECS-owned Zenith_SceneSystem::Get(), so the ECS core no longer names the engine
// singleton here.
bool Zenith_ECS_IsMainThread()
{
	const Zenith_ECSRuntimeHooks& xHooks = Zenith_SceneSystem::Get().GetRuntimeHooks();
	return xHooks.m_pfnIsMainThread ? xHooks.m_pfnIsMainThread() : true;
}
