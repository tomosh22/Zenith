#include "Zenith.h"
#include "EntityComponent/Zenith_SceneSystemBootstrap.h"
#include "TaskSystem/Zenith_TaskSystem.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneManager_Internal.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"

// Flux reset includes (for ResetAllRenderSystems)
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#endif
#include "Physics/Zenith_Physics.h"

// Static member definitions.
// A1: callback-list statics moved to Zenith_SceneCallbackBus.
// A2b: scene-storage statics moved to Zenith_SceneRegistry; Phase 5b moves
//      them further onto Zenith_SceneRegistry (g_xEngine.SceneRegistry()).
// Phase 5b: g_xEngine.SceneLifecycle().m_bRenderTasksActive / g_xEngine.SceneLifecycle().m_bAnimTasksActive moved to
// Zenith_SceneLifecycleScheduler (held by g_xEngine.SceneLifecycle()).
// Phase 5b: B3 UnloadUnusedAssets call-count counter moved onto
// Zenith_SceneOperationQueue (g_xEngine.SceneOperations().m_uUnloadUnusedAssetsCallCount).
// A3: async-operation statics moved to Zenith_SceneOperationQueue.
// A4: lifecycle/update statics (g_xEngine.SceneLifecycle().m_bIsLoadingScene, g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating,
// g_xEngine.SceneLifecycle().m_bIsUpdating, g_xEngine.SceneLifecycle().m_ulLastDeferredLoadOp, g_xEngine.SceneLifecycle().m_fFixedTimeAccumulator, Zenith_SceneLifecycleScheduler::s_fFixedTimestep,
// g_xEngine.SceneLifecycle().m_axCurrentlyLoadingPaths, g_xEngine.SceneLifecycle().m_axLifecycleLoadStack, g_xEngine.SceneLifecycle().m_pfnInitialSceneLoad)
// moved to Zenith_SceneLifecycleScheduler.

// PendingBuildIndexGuard: RAII helper that restores g_xEngine.SceneLifecycle().m_iPendingBuildIndex on scope
// exit. Protects against leaking the pending value if LoadScene aborts mid-call.
// Storage now lives on Zenith_SceneLifecycleScheduler (A4); the guard wraps the
// scheduler's static.
namespace
{
	struct PendingBuildIndexGuard
	{
		int m_iPrev;
		explicit PendingBuildIndexGuard(int iValue) : m_iPrev(g_xEngine.SceneLifecycle().m_iPendingBuildIndex)
		{
			g_xEngine.SceneLifecycle().m_iPendingBuildIndex = iValue;
		}
		~PendingBuildIndexGuard() { g_xEngine.SceneLifecycle().m_iPendingBuildIndex = m_iPrev; }

		PendingBuildIndexGuard(const PendingBuildIndexGuard&) = delete;
		PendingBuildIndexGuard& operator=(const PendingBuildIndexGuard&) = delete;
	};
}

// Pull in the detail symbols (ExtractSceneNameFromPath, progress milestones)
// so existing body code can use unqualified names.
using namespace Zenith_SceneManagerDetail;

Zenith_Scene Zenith_SceneManager::MakeInvalidScene() { return g_xEngine.SceneRegistry().MakeInvalidScene(); }

// Zenith_SceneLifecycleScheduler::IsCircularLoadDependency body migrated to Zenith_SceneLifecycleScheduler::IsCircularLoadDependency.

void Zenith_SceneManager::FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene) { g_xEngine.SceneCallbacks().FireUnloadCallbacksAndSelectNewActive(iHandle, xScene); }

void Zenith_SceneManager::AddToSceneNameCache(int iHandle, const std::string& strName) { g_xEngine.SceneRegistry().AddToSceneNameCache(iHandle, strName); }
void Zenith_SceneManager::RemoveFromSceneNameCache(int iHandle) { g_xEngine.SceneRegistry().RemoveFromSceneNameCache(iHandle); }

// Async load/unload progress milestones live in Zenith_SceneManager_Internal.h as
// `inline constexpr`s in Zenith_SceneManagerDetail; both this TU and
// Zenith_SceneOperationQueue.cpp share that single source of truth. The
// `using namespace Zenith_SceneManagerDetail;` above makes them unqualified here.

// Animation update task system moved to Zenith_SceneLifecycleScheduler.cpp (A4).

// Helper to extract scene name from file path (e.g., "Levels/MyScene.zscen" -> "MyScene").
// In namespace Zenith_SceneManagerDetail so Zenith_SceneOperationQueue.cpp can call it;
// the using-directive above keeps existing call sites in this file unqualified.
namespace Zenith_SceneManagerDetail
{
	std::string ExtractSceneNameFromPath(const std::string& strPath)
	{
		size_t uLastSlash = strPath.find_last_of("/\\");
		size_t uStart = (uLastSlash == std::string::npos) ? 0 : uLastSlash + 1;
		size_t uLastDot = strPath.find_last_of('.');
		size_t uEnd = (uLastDot == std::string::npos || uLastDot < uStart) ?
			strPath.size() : uLastDot;
		return strPath.substr(uStart, uEnd - uStart);
	}
}

// CanonicalizeScenePath helper moved to Zenith_SceneRegistry.cpp (Phase 5e) —
// the registry's GetSceneByPath now canonicalises internally.

//==========================================================================
// Initialization / Shutdown
//==========================================================================

// Orchestrator bodies live in Zenith_SceneSystemBootstrap.cpp (Phase 5e).
// SceneManager keeps 1-line forwarders for backwards compatibility until the
// SceneManager class is deleted.
void Zenith_SceneManager::Initialise() { Zenith_InitialiseSceneSystem(); }
void Zenith_SceneManager::Shutdown() { Zenith_ShutdownSceneSystem(); }

#ifdef ZENITH_TESTING
void Zenith_SceneManager::ResetForNextTest() { Zenith_ResetSceneSystemForNextTest(); }
#endif // ZENITH_TESTING

//==========================================================================
// Scene Count Queries
//==========================================================================

bool Zenith_SceneManager::IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData) { return g_xEngine.SceneRegistry().IsSceneVisibleToUser(uSlotIndex, pxData); }
bool Zenith_SceneManager::IsSceneUpdatable(const Zenith_SceneData* pxData) { return g_xEngine.SceneRegistry().IsSceneUpdatable(pxData); }

// Scene-count queries (GetLoadedSceneCount / GetTotalSceneCount /
// GetBuildSceneCount) are forwarders defined near the bottom of this file.

//==========================================================================
// Scene Creation
//==========================================================================

Zenith_Scene Zenith_SceneManager::CreateScene(const std::string& strName) { return g_xEngine.SceneRegistry().CreateScene(strName); }
Zenith_Entity Zenith_SceneManager::CreateEntity(const std::string& strName) { return Zenith_SceneEntityOwnership::CreateEntity(strName); }
Zenith_Scene Zenith_SceneManager::CreateEmptyScene(const std::string& strName, bool bAllowSetActive) { return g_xEngine.SceneRegistry().CreateEmptyScene(strName, bAllowSetActive); }

//==========================================================================
// Scene Queries
//==========================================================================
// GetActiveScene, GetSceneAt, GetSceneByBuildIndex, GetSceneByName all live
// on Zenith_SceneRegistry (post-A2). GetSceneByPath stays here because it
// depends on the file-local CanonicalizeScenePath helper above.

Zenith_Scene Zenith_SceneManager::GetSceneByPath(const std::string& strPath) { return g_xEngine.SceneRegistry().GetSceneByPath(strPath); }

// Build index registry (RegisterSceneBuildIndex / ClearBuildIndexRegistry /
// GetRegisteredScenePath / GetBuildIndexRegistrySize) lives on
// Zenith_SceneRegistry.

//==========================================================================
// Scene Loading (Synchronous)
//==========================================================================

bool Zenith_SceneManager::ValidateLoadRequest(const std::string& strPath)
{
	// Body moved to Zenith_SceneOperationQueue.cpp file-static
	// ValidateLoadRequestInternal during Phase 5e. Kept here so the public
	// declaration still resolves until the SceneManager class is deleted.
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "LoadScene must be called from main thread");
	if (strPath.empty()) { Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: Path is empty"); return false; }
	return true;
}

// Unity parity: LoadScene called during script execution (Update/FixedUpdate/callbacks)
// is deferred to next frame's ProcessPendingAsyncLoads, matching Unity's
// EarlyUpdate.UpdatePreloading behavior. This prevents use-after-free when the
// calling entity's scene is destroyed by SCENE_LOAD_SINGLE.

Zenith_SceneOperationID Zenith_SceneManager::GetLastDeferredLoadOp() { return g_xEngine.SceneLifecycle().GetLastDeferredLoadOp(); }

// HandleDeferredLoad / ValidateFileAndDetectCircular /
// PerformSingleModeTeardownAndSwap / DispatchLifecycleAndFire were the
// pre-B4 multi-phase helpers backing the synchronous LoadScene body. Post-B4
// LoadScene is queue-and-defer; the multi-phase work moved to
// Zenith_SceneOperationQueue::RunAsyncJobPhase1/2 (file I/O, header
// validation, teardown, lifecycle dispatch). The helpers here have been
// retired with the rest of the sync body.

Zenith_Scene Zenith_SceneManager::LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadScene(strPath, eMode); }
Zenith_Scene Zenith_SceneManager::LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneByIndex(iBuildIndex, eMode); }

//==========================================================================
// Scene loading (blocking — bootstrap / tools only) — bodies moved to
// Zenith_SceneOperationQueue.cpp. The static helpers IsBootstrapLoadContext /
// PumpDeferredLoadUntilComplete moved with them as file-scope helpers there.
//==========================================================================

Zenith_Scene Zenith_SceneManager::LoadSceneBlockingForBootstrap(const std::string& strPath, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneBlockingForBootstrap(strPath, eMode); }
Zenith_Scene Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(int iBuildIndex, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneByIndexBlockingForBootstrap(iBuildIndex, eMode); }

#ifdef ZENITH_TOOLS
Zenith_Scene Zenith_SceneManager::LoadSceneBlocking_ToolsOnly(const std::string& strPath, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneBlocking_ToolsOnly(strPath, eMode); }
Zenith_Scene Zenith_SceneManager::LoadSceneByIndexBlocking_ToolsOnly(int iBuildIndex, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneByIndexBlocking_ToolsOnly(iBuildIndex, eMode); }
#endif

//==========================================================================
// Scene Loading (Asynchronous) — bodies live on Zenith_SceneOperationQueue
// post-Phase 5e. Kept here as 1-line forwarders until the SceneManager class
// is deleted.
//==========================================================================

Zenith_SceneOperationID Zenith_SceneManager::LoadSceneAsync(const std::string& strPath, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneAsync(strPath, eMode); }
Zenith_SceneOperationID Zenith_SceneManager::LoadSceneAsyncByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode) { return g_xEngine.SceneOperations().LoadSceneAsyncByIndex(iBuildIndex, eMode); }

//==========================================================================
// Scene Unloading — bodies migrated to Zenith_SceneOperationQueue.cpp
//==========================================================================

// CanUnloadScene / UnloadScene / UnloadSceneAsync / UnloadSceneForced bodies
// migrated to Zenith_SceneOperationQueue.cpp during Phase 5e. SceneManager keeps
// 1-line forwarders for the public ones; UnloadSceneInternal (private) collapses
// to a direct FireUnload call.
bool Zenith_SceneManager::CanUnloadScene(Zenith_Scene xScene) { return g_xEngine.SceneOperations().CanUnloadScene(xScene); }
void Zenith_SceneManager::UnloadSceneInternal(Zenith_Scene xScene) { FireUnloadCallbacksAndSelectNewActive(xScene.m_iHandle, xScene); }
void Zenith_SceneManager::UnloadSceneForced(Zenith_Scene xScene) { g_xEngine.SceneOperations().UnloadSceneForced(xScene); }
void Zenith_SceneManager::UnloadScene(Zenith_Scene xScene) { g_xEngine.SceneOperations().UnloadScene(xScene); }
Zenith_SceneOperationID Zenith_SceneManager::UnloadSceneAsync(Zenith_Scene xScene) { return g_xEngine.SceneOperations().UnloadSceneAsync(xScene); }

//==========================================================================
// Entity Destruction
//==========================================================================

// A5: Destroy / DestroyImmediate / MoveEntityToScene / MergeScenes / MarkEntityPersistent
// bodies live in Zenith_SceneEntityOwnership.
bool Zenith_SceneManager::RenameScene(Zenith_Scene xScene, const std::string& strNewName) { return g_xEngine.SceneRegistry().RenameScene(xScene, strNewName); }
Zenith_Scene Zenith_SceneManager::GetPersistentScene() { return g_xEngine.SceneRegistry().GetPersistentScene(); }
void Zenith_SceneManager::Destroy(Zenith_Entity& xEntity) { Zenith_SceneEntityOwnership::Destroy(xEntity); }
void Zenith_SceneManager::Destroy(Zenith_Entity& xEntity, float fDelay) { Zenith_SceneEntityOwnership::Destroy(xEntity, fDelay); }
void Zenith_SceneManager::DestroyImmediate(Zenith_Entity& xEntity) { Zenith_SceneEntityOwnership::DestroyImmediate(xEntity); }
bool Zenith_SceneManager::MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget) { return Zenith_SceneEntityOwnership::MoveEntityToScene(xEntity, xTarget); }
bool Zenith_SceneManager::MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget) { return Zenith_SceneEntityOwnership::MergeScenes(xSource, xTarget); }
void Zenith_SceneManager::MarkEntityPersistent(Zenith_Entity& xEntity) { Zenith_SceneEntityOwnership::MarkEntityPersistent(xEntity); }

//==========================================================================
// Scene Management
//==========================================================================

bool Zenith_SceneManager::SetActiveScene(Zenith_Scene xScene) { return g_xEngine.SceneRegistry().SetActiveScene(xScene); }
void Zenith_SceneManager::SetScenePaused(Zenith_Scene xScene, bool bPaused) { g_xEngine.SceneRegistry().SetScenePaused(xScene, bPaused); }
bool Zenith_SceneManager::IsScenePaused(Zenith_Scene xScene) { return g_xEngine.SceneRegistry().IsScenePaused(xScene); }

void Zenith_SceneManager::UnloadUnusedAssets() { g_xEngine.SceneOperations().UnloadUnusedAssets(); }

// ============================================================================
// Multi-Scene Rendering Helpers
// ============================================================================

Zenith_CameraComponent* Zenith_SceneManager::FindMainCameraAcrossScenes() { return g_xEngine.SceneRegistry().FindMainCameraAcrossScenes(); }

uint32_t Zenith_SceneManager::GetSceneSlotCount() { return g_xEngine.SceneRegistry().GetSceneSlotCount(); }
Zenith_SceneData* Zenith_SceneManager::GetSceneDataAtSlot(uint32_t uIndex) { return g_xEngine.SceneRegistry().GetSceneDataAtSlot(uIndex); }
Zenith_SceneData* Zenith_SceneManager::GetLoadedSceneDataAtSlot(uint32_t uIndex) { return g_xEngine.SceneRegistry().GetLoadedSceneDataAtSlot(uIndex); }
int Zenith_SceneManager::SelectNewActiveScene(int iExcludeHandle) { return g_xEngine.SceneRegistry().SelectNewActiveScene(iExcludeHandle); }

// Internal helper that moves entity between scenes using zero-copy transfer.
// EntityID is preserved (globally unique) - no serialize/deserialize.
// No lifecycle events fire (Unity parity: MoveGameObjectToScene is seamless).
// Returns true on success, false on failure.
// Cross-scene entity operations (MoveEntityInternal, MoveEntityToScene,
// MergeScenes, MarkEntityPersistent, Destroy*) live on
// Zenith_SceneEntityOwnership (post-A5). RenameScene + GetPersistentScene
// live on Zenith_SceneRegistry (post-A2).

//==========================================================================
// Event Callbacks
//==========================================================================

// AllocateOperationID body migrated to Zenith_SceneOperationQueue::AllocateOperationID.

// Callback subsystem (Zenith_CallbackList<T>, AllocateCallbackHandle,
// IsCallbackHandleInUse, IsCallbackPendingRemoval, ProcessPendingCallbackRemovals)
// is defined in Zenith_SceneCallbackBus.cpp post-A1. The public Register/Unregister/
// Fire methods on Zenith_SceneManager are forwarders defined near the end of this file.

//==========================================================================
// Internal
//==========================================================================

//==========================================================================
// RAII guard implementations moved to Zenith_SceneSystemGuards.cpp (Phase 5e).
// The guards are now top-level types `Zenith_LifecycleDeferralGuard`,
// `Zenith_PrefabInstantiationGuard`, `Zenith_SceneUpdateDeferralGuard`,
// `Zenith_SceneCreationTargetScope`. The aliases in
// Zenith_SceneManagerGuards.h preserve old call sites until 5e codemod.
//==========================================================================

// (deleted: PrefabInstantiationGuard / SceneUpdateDeferralGuard /
//           SceneCreationTargetScope constructor + destructor definitions)
#if 0
Zenith_SceneManager::SceneCreationTargetScope::SceneCreationTargetScope(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SceneCreationTargetScope must be constructed on the main thread");
	g_xEngine.SceneLifecycle().m_axCreationTargetStack.PushBack(xScene);
}

Zenith_SceneManager::SceneCreationTargetScope::~SceneCreationTargetScope()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SceneCreationTargetScope must be destroyed on the main thread");
	Zenith_Assert(g_xEngine.SceneLifecycle().m_axCreationTargetStack.GetSize() > 0,
		"SceneCreationTargetScope: creation-target stack underflow on destruction");
	g_xEngine.SceneLifecycle().m_axCreationTargetStack.PopBack();
}
#endif // 0 — guard definitions moved to Zenith_SceneSystemGuards.cpp

Zenith_Scene Zenith_SceneManager::GetDefaultCreationScene() { return g_xEngine.SceneLifecycle().GetDefaultCreationScene(); }
void Zenith_SceneManager::SetMainLoopRunning(bool bRunning) { g_xEngine.SceneLifecycle().SetMainLoopRunning(bRunning); }

void Zenith_SceneManager::SetFixedTimestep(float fTimestep) { g_xEngine.SceneLifecycle().SetFixedTimestep(fTimestep); }
float Zenith_SceneManager::GetFixedTimestep() { return g_xEngine.SceneLifecycle().GetFixedTimestep(); }

// A4: Update / WaitForUpdateComplete bodies live in Zenith_SceneLifecycleScheduler.cpp.
void Zenith_SceneManager::Update(float fDt) { g_xEngine.SceneLifecycle().Update(fDt); }
void Zenith_SceneManager::WaitForUpdateComplete() { g_xEngine.SceneLifecycle().WaitForUpdateComplete(); }

Zenith_SceneData* Zenith_SceneManager::GetSceneData(Zenith_Scene xScene) { return g_xEngine.SceneRegistry().GetSceneData(xScene); }
Zenith_SceneData* Zenith_SceneManager::GetSceneDataByHandle(int iHandle) { return g_xEngine.SceneRegistry().GetSceneDataByHandle(iHandle); }
Zenith_SceneData* Zenith_SceneManager::GetSceneDataForEntity(Zenith_EntityID xID) { return g_xEngine.SceneRegistry().GetSceneDataForEntity(xID); }
Zenith_Scene Zenith_SceneManager::GetSceneFromHandle(int iHandle) { return g_xEngine.SceneRegistry().GetSceneFromHandle(iHandle); }

// A4: lifecycle accessors / setters / dispatch hook all forward to scheduler.
bool Zenith_SceneManager::IsLoadingScene() { return g_xEngine.SceneLifecycle().m_bIsLoadingScene; }
bool Zenith_SceneManager::IsPrefabInstantiating() { return g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating; }
bool Zenith_SceneManager::IsUpdating() { return g_xEngine.SceneLifecycle().m_bIsUpdating; }
bool Zenith_SceneManager::IsActiveSceneSuppressed() { return g_xEngine.SceneCallbacks().IsActiveSceneSuppressed(); }
int Zenith_SceneManager::GetPendingBuildIndex() { return g_xEngine.SceneLifecycle().m_iPendingBuildIndex; }
bool Zenith_SceneManager::IsCircularLoadDependency(const std::string& strCanonicalPath) { return g_xEngine.SceneLifecycle().IsCircularLoadDependency(strCanonicalPath); }
void Zenith_SceneManager::SetInitialSceneLoadCallback(InitialSceneLoadFn pfnCallback) { g_xEngine.SceneLifecycle().SetInitialSceneLoadCallback(pfnCallback); }
Zenith_SceneManager::InitialSceneLoadFn Zenith_SceneManager::GetInitialSceneLoadCallback() { return g_xEngine.SceneLifecycle().GetInitialSceneLoadCallback(); }
#ifdef ZENITH_TESTING
void Zenith_SceneManager::DispatchFullLifecycleInit() { g_xEngine.SceneLifecycle().DispatchFullLifecycleInit(); }
#endif

int Zenith_SceneManager::AllocateSceneHandle() { return g_xEngine.SceneRegistry().AllocateSceneHandle(); }
void Zenith_SceneManager::FreeSceneHandle(int iHandle) { g_xEngine.SceneRegistry().FreeSceneHandle(iHandle); }

// Free-function forwarder for use from Zenith_SceneData.h template bodies
// (declared in Zenith_RenderTaskState.h). The static class accessor and
// underlying flag storage stay in Zenith_SceneManagerInternal.h — this file
// is just the definition site, intentionally minimal.
bool Zenith_AreRenderTasksActive() { return g_xEngine.SceneLifecycle().AreRenderTasksActive(); }

// Bulk-unload helpers migrated to Zenith_SceneOperationQueue.cpp (Phase 5e).
void Zenith_SceneManager::UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut) { g_xEngine.SceneOperations().UnloadOneScene(xScene, bActiveSceneUnloadedInOut); }
void Zenith_SceneManager::PushLifecycleContext(const std::string& strCanonicalPath) { g_xEngine.SceneLifecycle().PushLifecycleContext(strCanonicalPath); }
void Zenith_SceneManager::PopLifecycleContext(const std::string& strCanonicalPath) { g_xEngine.SceneLifecycle().PopLifecycleContext(strCanonicalPath); }
void Zenith_SceneManager::ProcessPendingUnloads() { g_xEngine.SceneOperations().ProcessPendingUnloads(); }
void Zenith_SceneManager::CompleteAsyncUnloadJobs(Zenith_HashSet<int>& xAlreadyFiredOut) { g_xEngine.SceneOperations().CompleteAsyncUnloadJobs(xAlreadyFiredOut); }
bool Zenith_SceneManager::DestroyScenesAndFireUnloaded(const Zenith_Vector<Zenith_Scene>& axScenes, const Zenith_HashSet<int>& xAlreadyFired) { return g_xEngine.SceneOperations().DestroyScenesAndFireUnloaded(axScenes, xAlreadyFired); }
void Zenith_SceneManager::UpdateActiveSceneAfterUnload(Zenith_Scene xOldActive) { g_xEngine.SceneOperations().UpdateActiveSceneAfterUnload(xOldActive); }
void Zenith_SceneManager::UnloadAllNonPersistent(int iExcludeHandle) { g_xEngine.SceneOperations().UnloadAllNonPersistent(iExcludeHandle); }
#if 0
// (orphan block from Phase 5e edit — bodies migrated to Zenith_SceneOperationQueue.cpp)
#endif

uint32_t Zenith_SceneManager::CountScenesBeingAsyncUnloaded() { return g_xEngine.SceneOperations().CountScenesBeingAsyncUnloaded(); }

bool Zenith_SceneManager::HasPendingDestructions() { return g_xEngine.SceneOperations().HasPendingDestructions(); }
void Zenith_SceneManager::ResetAllRenderSystems() { g_xEngine.SceneOperations().ResetAllRenderSystems(); }

//==========================================================================
// Scene-query API forwarders (A2). Bodies live in Zenith_SceneRegistry.cpp.
//==========================================================================

uint32_t Zenith_SceneManager::GetLoadedSceneCount() { return g_xEngine.SceneRegistry().GetLoadedSceneCount(); }
uint32_t Zenith_SceneManager::GetTotalSceneCount() { return g_xEngine.SceneRegistry().GetTotalSceneCount(); }
uint32_t Zenith_SceneManager::GetBuildSceneCount() { return g_xEngine.SceneRegistry().GetBuildSceneCount(); }

Zenith_Scene Zenith_SceneManager::GetActiveScene() { return g_xEngine.SceneRegistry().GetActiveScene(); }
Zenith_Scene Zenith_SceneManager::GetSceneAt(uint32_t uIndex) { return g_xEngine.SceneRegistry().GetSceneAt(uIndex); }
Zenith_Scene Zenith_SceneManager::GetSceneByBuildIndex(int iBuildIndex) { return g_xEngine.SceneRegistry().GetSceneByBuildIndex(iBuildIndex); }
Zenith_Scene Zenith_SceneManager::GetSceneByName(const std::string& strName) { return g_xEngine.SceneRegistry().GetSceneByName(strName); }

void Zenith_SceneManager::RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath) { g_xEngine.SceneRegistry().RegisterSceneBuildIndex(iBuildIndex, strPath); }
void Zenith_SceneManager::ClearBuildIndexRegistry() { g_xEngine.SceneRegistry().ClearBuildIndexRegistry(); }
const std::string& Zenith_SceneManager::GetRegisteredScenePath(int iBuildIndex) { return g_xEngine.SceneRegistry().GetRegisteredScenePath(iBuildIndex); }
uint32_t Zenith_SceneManager::GetBuildIndexRegistrySize() { return g_xEngine.SceneRegistry().GetBuildIndexRegistrySize(); }

//==========================================================================
// Callback API forwarders (A1).
//
// Public Register/Unregister/Fire methods on Zenith_SceneManager forward
// directly into Zenith_SceneCallbackBus. The bus owns the lists, handle
// allocator, deferred-removal queue, dispatch-depth counter and
// active-scene-changed suppression scope state.
//==========================================================================

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterActiveSceneChangedCallback(SceneChangedCallback pfn) { return g_xEngine.SceneCallbacks().RegisterActiveSceneChanged(pfn); }
void Zenith_SceneManager::UnregisterActiveSceneChangedCallback(CallbackHandle ulHandle) { g_xEngine.SceneCallbacks().UnregisterActiveSceneChanged(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneLoadedCallback(SceneLoadedCallback pfn) { return g_xEngine.SceneCallbacks().RegisterSceneLoaded(pfn); }
void Zenith_SceneManager::UnregisterSceneLoadedCallback(CallbackHandle ulHandle) { g_xEngine.SceneCallbacks().UnregisterSceneLoaded(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneUnloadingCallback(SceneUnloadingCallback pfn) { return g_xEngine.SceneCallbacks().RegisterSceneUnloading(pfn); }
void Zenith_SceneManager::UnregisterSceneUnloadingCallback(CallbackHandle ulHandle) { g_xEngine.SceneCallbacks().UnregisterSceneUnloading(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneUnloadedCallback(SceneUnloadedCallback pfn) { return g_xEngine.SceneCallbacks().RegisterSceneUnloaded(pfn); }
void Zenith_SceneManager::UnregisterSceneUnloadedCallback(CallbackHandle ulHandle) { g_xEngine.SceneCallbacks().UnregisterSceneUnloaded(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneLoadStartedCallback(SceneLoadStartedCallback pfn) { return g_xEngine.SceneCallbacks().RegisterSceneLoadStarted(pfn); }
void Zenith_SceneManager::UnregisterSceneLoadStartedCallback(CallbackHandle ulHandle) { g_xEngine.SceneCallbacks().UnregisterSceneLoadStarted(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterEntityPersistentCallback(EntityPersistentCallback pfn) { return g_xEngine.SceneCallbacks().RegisterEntityPersistent(pfn); }
void Zenith_SceneManager::UnregisterEntityPersistentCallback(CallbackHandle ulHandle) { g_xEngine.SceneCallbacks().UnregisterEntityPersistent(ulHandle); }

//==========================================================================
// Async-operation API forwarders (A3). Bodies live in Zenith_SceneOperationQueue.cpp.
//==========================================================================

void Zenith_SceneManager::NotifyAsyncJobPriorityChanged() { g_xEngine.SceneOperations().NotifyAsyncJobPriorityChanged(); }
Zenith_SceneOperation* Zenith_SceneManager::GetOperation(Zenith_SceneOperationID ulID) { return g_xEngine.SceneOperations().GetOperation(ulID); }
bool Zenith_SceneManager::IsOperationValid(Zenith_SceneOperationID ulID) { return g_xEngine.SceneOperations().IsOperationValid(ulID); }
void Zenith_SceneManager::SetAsyncUnloadBatchSize(uint32_t uEntitiesPerFrame) { g_xEngine.SceneOperations().SetAsyncUnloadBatchSize(uEntitiesPerFrame); }
uint32_t Zenith_SceneManager::GetAsyncUnloadBatchSize() { return g_xEngine.SceneOperations().GetAsyncUnloadBatchSize(); }
void Zenith_SceneManager::SetMaxConcurrentAsyncLoads(uint32_t uMax) { g_xEngine.SceneOperations().SetMaxConcurrentAsyncLoads(uMax); }
uint32_t Zenith_SceneManager::GetMaxConcurrentAsyncLoads() { return g_xEngine.SceneOperations().GetMaxConcurrentAsyncLoads(); }

void Zenith_SceneManager::FireSceneLoadedCallbacks(Zenith_Scene xScene, Zenith_SceneLoadMode eMode) { g_xEngine.SceneCallbacks().FireSceneLoaded(xScene, eMode); }
void Zenith_SceneManager::FireSceneUnloadingCallbacks(Zenith_Scene xScene) { g_xEngine.SceneCallbacks().FireSceneUnloading(xScene); }
void Zenith_SceneManager::FireSceneUnloadedCallbacks(Zenith_Scene xScene) { g_xEngine.SceneCallbacks().FireSceneUnloaded(xScene); }
void Zenith_SceneManager::FireActiveSceneChangedCallbacks(Zenith_Scene xOld, Zenith_Scene xNew) { g_xEngine.SceneCallbacks().FireActiveSceneChanged(xOld, xNew); }
void Zenith_SceneManager::FireSceneLoadStartedCallbacks(const std::string& strPath) { g_xEngine.SceneCallbacks().FireSceneLoadStarted(strPath); }
void Zenith_SceneManager::FireEntityPersistentCallbacks(const Zenith_Entity& xEntity) { g_xEngine.SceneCallbacks().FireEntityPersistent(xEntity); }

//==========================================================================
// Zenith_SceneLifecycleContext (A0): read-only cross-subsystem state surface.
// Implementations forward to the corresponding Zenith_SceneManager accessors
// during Phase A. As subsystems are extracted, each forwarder is repointed
// at the new owner without changing the call surface.
//==========================================================================

#include "EntityComponent/Internal/Zenith_SceneLifecycleContext.h"

namespace Zenith_SceneLifecycleContext
{
	bool IsLoadingScene()
	{
		return Zenith_SceneManager::IsLoadingScene();
	}

	bool IsPrefabInstantiating()
	{
		return Zenith_SceneManager::IsPrefabInstantiating();
	}

	bool IsUpdating()
	{
		return Zenith_SceneManager::IsUpdating();
	}

	bool IsMainLoopRunning()
	{
		return g_xEngine.SceneLifecycle().m_bIsMainLoopRunning;
	}

	int GetPendingBuildIndex()
	{
		return Zenith_SceneManager::GetPendingBuildIndex();
	}

	bool IsCircularLoadDependency(const std::string& strCanonicalPath)
	{
		return Zenith_SceneManager::IsCircularLoadDependency(strCanonicalPath);
	}

	bool IsActiveSceneSuppressed()
	{
		return Zenith_SceneManager::IsActiveSceneSuppressed();
	}

	Zenith_Scene GetCurrentCreationTarget()
	{
		const u_int uDepth = g_xEngine.SceneLifecycle().m_axCreationTargetStack.GetSize();
		if (uDepth == 0)
		{
			return Zenith_Scene::INVALID_SCENE;
		}
		return g_xEngine.SceneLifecycle().m_axCreationTargetStack.Get(uDepth - 1);
	}
}

#ifdef ZENITH_TESTING
#include "EntityComponent/Zenith_SceneManager.Tests.inl"
#endif

