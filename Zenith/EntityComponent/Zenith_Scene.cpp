#include "Zenith.h"

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

//------------------------------------------------------------------------------
// Zenith_Scene Property Implementations
// These delegate to SceneManager/SceneData to get actual values
//------------------------------------------------------------------------------

bool Zenith_Scene::IsValid() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(),
		"IsValid must be called from main thread or during render task execution");
	// Check if handle is in valid range and generation matches
	// GetSceneData performs the generation check internally
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
	return pxData != nullptr;
}

int Zenith_Scene::GetBuildIndex() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetBuildIndex must be called from main thread");
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
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
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
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
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	// Unity parity (audit §3.3): IsLoaded is true once Awake/OnEnable have completed,
	// and remains true during SceneUnloading callbacks so subscribers can still
	// enumerate entities. Unity's Scene.isLoaded stays true until the scene is
	// actually removed from the manager — not while it's being torn down.
	// Ref: https://docs.unity3d.com/ScriptReference/SceneManagement.Scene-isLoaded.html
	//
	// LoadScene is synchronous: a scene is created in SCENE_STATE_LOADING
	// (IsActivated()==false), then flipped to SCENE_STATE_LOADED after Awake/OnEnable
	// complete and BEFORE the SceneLoaded callback fires (this holds for the SINGLE,
	// ADDITIVE, and ADDITIVE_WITHOUT_LOADING paths alike). So handlers registered with
	// SceneLoaded and SceneUnloading observe IsLoaded==true; handlers registered with
	// SceneUnloaded observe IsLoaded==false (scene data is already null).
	return pxData->IsLoaded() && pxData->IsActivated();
}

bool Zenith_Scene::WasLoadedAdditively() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "WasLoadedAdditively must be called from main thread");
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
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
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
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
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
	if (pxData == nullptr)
	{
		return s_strEmpty;
	}
	return pxData->GetPath();
}

uint32_t Zenith_Scene::GetRootEntityCount() const
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetRootEntityCount must be called from main thread");
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
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
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(*this);
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
