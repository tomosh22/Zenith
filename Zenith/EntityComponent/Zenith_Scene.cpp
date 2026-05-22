#include "Zenith.h"

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

//------------------------------------------------------------------------------
// Zenith_Scene Property Implementations
// These delegate to SceneManager/SceneData to get actual values
//------------------------------------------------------------------------------

bool Zenith_Scene::IsValid() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"IsValid must be called from main thread or during render task execution");
	// Check if handle is in valid range and generation matches
	// GetSceneData performs the generation check internally
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	return pxData != nullptr;
}

int Zenith_Scene::GetBuildIndex() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetBuildIndex must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return -1;
	}
	return pxData->GetBuildIndex();
}

#ifdef ZENITH_TOOLS
bool Zenith_Scene::HasUnsavedChanges() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "HasUnsavedChanges must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	return pxData->HasUnsavedChanges();
}
#endif

bool Zenith_Scene::IsLoaded() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "IsLoaded must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	// Unity parity (audit §3.3): IsLoaded is true once Awake/OnEnable have completed,
	// and remains true during SceneUnloading callbacks so subscribers can still
	// enumerate entities. Unity's Scene.isLoaded stays true until the scene is
	// actually removed from the SceneManager — not while it's being unloaded.
	// Ref: https://docs.unity3d.com/ScriptReference/SceneManagement.Scene-isLoaded.html
	//
	// State transitions:
	//   - SceneManager::LoadScene(SINGLE/ADDITIVE) sync path: creates with IsActivated=false,
	//     flips to true AFTER Awake/OnEnable complete, BEFORE SceneLoaded callback.
	//   - SceneManager::LoadSceneAsync sync path: same progression inside Phase 2.
	//   - CreateEmptyScene / LoadScene(ADDITIVE_WITHOUT_LOADING): same progression (false
	//     during construction, flipped to true before any callbacks fire).
	//   - UnloadSceneAsync: sets m_bIsUnloading=true as a re-entry / SetActiveScene
	//     rejection guard, but does NOT gate IsLoaded here — callbacks registered for
	//     SceneUnloading still observe IsLoaded==true.
	// Handlers registered with SceneLoadStarted observe IsLoaded==false; handlers
	// registered with SceneLoaded and SceneUnloading observe IsLoaded==true; handlers
	// registered with SceneUnloaded observe IsLoaded==false (scene data is null).
	return pxData->IsLoaded() && pxData->IsActivated();
}

bool Zenith_Scene::WasLoadedAdditively() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "WasLoadedAdditively must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	return pxData->WasLoadedAdditively();
}


const std::string& Zenith_Scene::GetName() const
{
	static const std::string s_strEmpty;
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetName must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return s_strEmpty;
	}
	return pxData->GetName();
}

const std::string& Zenith_Scene::GetPath() const
{
	static const std::string s_strEmpty;
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetPath must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return s_strEmpty;
	}
	return pxData->GetPath();
}

uint32_t Zenith_Scene::GetRootEntityCount() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetRootEntityCount must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr || !pxData->IsLoaded())
	{
		return 0;
	}
	// Use cached root entity count for O(1) performance (Unity parity)
	return pxData->GetCachedRootEntityCount();
}

void Zenith_Scene::GetRootEntities(Zenith_Vector<Zenith_Entity>& axOut) const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetRootEntities must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr || !pxData->IsLoaded())
	{
		return;
	}

	// GetCachedRootEntities rebuilds via RebuildRootEntityCache, which already
	// filters with EntityExists. Skipping the redundant filter here keeps
	// axOut.GetSize() equal to GetRootEntityCount() — otherwise a stale entry
	// would quietly drop from the output while the count still advertised it.
	Zenith_Vector<Zenith_EntityID> axRootIDs;
	pxData->GetCachedRootEntities(axRootIDs);
	for (u_int u = 0; u < axRootIDs.GetSize(); ++u)
	{
		axOut.PushBack(pxData->GetEntity(axRootIDs.Get(u)));
	}
}
