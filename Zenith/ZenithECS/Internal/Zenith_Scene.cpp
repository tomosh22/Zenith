#include "Zenith.h"

#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

//------------------------------------------------------------------------------
// Zenith_Scene is an OPAQUE handle (Phase 7b-1). It exposes only identity +
// validity. All scene metadata that used to live as Zenith_Scene getters
// (GetName / GetPath / GetBuildIndex / IsLoaded / WasLoadedAdditively /
// HasUnsavedChanges / GetRootEntityCount) is now read through
// Zenith_SceneSystem::GetSceneInfo(xScene). The only behaviour left on
// the handle is the validity check below (which still delegates to the scene
// system's generation-checked GetSceneData).
//------------------------------------------------------------------------------

bool Zenith_Scene::IsValid() const
{
	Zenith_Assert(Zenith_ECS_IsMainThread() || Zenith_AreRenderTasksActive(),
		"IsValid must be called from main thread or during render task execution");
	// Check if handle is in valid range and generation matches.
	// GetSceneData performs the generation check internally.
	Zenith_SceneData* pxData = Zenith_SceneSystem::Get().GetSceneData(*this);
	return pxData != nullptr;
}
