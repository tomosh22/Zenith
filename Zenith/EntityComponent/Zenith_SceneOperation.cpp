#include "Zenith.h"

#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"

Zenith_Scene Zenith_SceneOperation::GetResultScene() const
{
	// Build from the generation captured at result-set time, not the current
	// generation. If the scene was unloaded and the slot recycled between
	// op-completion and this call, returning the current generation would
	// point the caller at a *different* scene (or wrap to a freshly-occupied
	// one). With the captured generation, Zenith_Scene::IsValid() returns
	// false in that case and callers detect the stale state.
	Zenith_Scene xScene;
	xScene.m_iHandle = m_iResultSceneHandle;
	xScene.m_uGeneration = m_uResultSceneGeneration;
	return xScene;
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
	// D3: completion callback firing before the op is actually complete means
	// the SceneManager state machine is out of order (e.g. SetProgress(1.0) without
	// SetComplete(true)). Callers downstream of this see IsComplete()==false and
	// get confused; catch the ordering mistake at the fire site.
	Zenith_Assert(m_bIsComplete.load(std::memory_order_acquire),
		"FireCompletionCallback: op is not marked complete (SetComplete(true) must precede the fire)");
	if (m_pfnOnComplete)
	{
		// Same rationale as GetResultScene above: use the captured generation
		// so a recycled slot doesn't masquerade as a successful load.
		Zenith_Scene xScene;
		xScene.m_iHandle = m_iResultSceneHandle;
		xScene.m_uGeneration = m_uResultSceneGeneration;
		m_pfnOnComplete(xScene);
	}
}
