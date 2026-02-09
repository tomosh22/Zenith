#include "Zenith.h"

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

//------------------------------------------------------------------------------
// Zenith_Scene Property Implementations
// These delegate to SceneManager/SceneData to get actual values
//------------------------------------------------------------------------------

bool Zenith_Scene::IsValid() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"IsValid must be called from main thread or during render task execution");
	// Check if handle is in valid range and generation matches
	// GetSceneData performs the generation check internally
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	return pxData != nullptr;
}

int Zenith_Scene::GetBuildIndex() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetBuildIndex must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return -1;
	}
	return pxData->m_iBuildIndex;
}

#ifdef ZENITH_TOOLS
bool Zenith_Scene::HasUnsavedChanges() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "HasUnsavedChanges must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	return pxData->m_bHasUnsavedChanges;
}
#endif

bool Zenith_Scene::IsLoaded() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsLoaded must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	// Return false if scene is being unloaded or not yet activated (Unity parity:
	// scene.isLoaded is false until Awake/OnEnable have completed for async loads)
	return pxData->m_bIsLoaded && pxData->m_bIsActivated && !pxData->m_bIsUnloading;
}

bool Zenith_Scene::WasLoadedAdditively() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "WasLoadedAdditively must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return false;
	}
	return pxData->m_bWasLoadedAdditively;
}


const std::string& Zenith_Scene::GetName() const
{
	static const std::string s_strEmpty;
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetName must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return s_strEmpty;
	}
	return pxData->m_strName;
}

const std::string& Zenith_Scene::GetPath() const
{
	static const std::string s_strEmpty;
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetPath must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr)
	{
		return s_strEmpty;
	}
	return pxData->m_strPath;
}

uint32_t Zenith_Scene::GetRootEntityCount() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetRootEntityCount must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr || !pxData->m_bIsLoaded)
	{
		return 0;
	}
	// Use cached root entity count for O(1) performance (Unity parity)
	return pxData->GetCachedRootEntityCount();
}

void Zenith_Scene::GetRootEntities(Zenith_Vector<Zenith_Entity>& axOut) const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetRootEntities must be called from main thread");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(*this);
	if (pxData == nullptr || !pxData->m_bIsLoaded)
	{
		return;
	}

	// Use cached root entities for consistent performance
	Zenith_Vector<Zenith_EntityID> axRootIDs;
	pxData->GetCachedRootEntities(axRootIDs);
	for (u_int u = 0; u < axRootIDs.GetSize(); ++u)
	{
		Zenith_EntityID xID = axRootIDs.Get(u);
		if (pxData->EntityExists(xID))
		{
			axOut.PushBack(pxData->GetEntity(xID));
		}
	}
}
