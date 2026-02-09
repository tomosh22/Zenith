#include "Zenith.h"

#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"

Zenith_Scene Zenith_SceneOperation::GetResultScene() const
{
	return Zenith_SceneManager::GetSceneFromHandle(m_iResultSceneHandle);
}

void Zenith_SceneOperation::SetPriority(int iPriority)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetPriority must be called from main thread");
	m_iPriority = iPriority;
	Zenith_SceneManager::NotifyAsyncJobPriorityChanged();
}

void Zenith_SceneOperation::FireCompletionCallback()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "FireCompletionCallback must be called from main thread");
	if (m_pfnOnComplete)
	{
		m_pfnOnComplete(Zenith_SceneManager::GetSceneFromHandle(m_iResultSceneHandle));
	}
}
