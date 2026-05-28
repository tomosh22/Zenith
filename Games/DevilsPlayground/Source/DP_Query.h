#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_Query.h"

#include <cstdint>

// ============================================================================
// DP_Query — script-iteration helpers. Scripts live INSIDE
// Zenith_ScriptComponent, so direct Query<T> doesn't work — these
// template helpers iterate ScriptComponents and filter by script type.
// Header-only because the templates instantiate per-T at call sites.
// ============================================================================
namespace DP_Query
{
	// Iterate every entity in the active scene that carries a script of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	template<typename T, typename Fn>
	void ForEachScriptInActiveScene(Fn&& fn)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) return;
		pxScene->Query<Zenith_ScriptComponent>().ForEach(
			[&fn](Zenith_EntityID xId, Zenith_ScriptComponent& xScript)
			{
				T* pxT = xScript.GetScript<T>();
				if (pxT != nullptr)
				{
					fn(xId, *pxT);
				}
			});
	}

	// Iterate every entity in ALL currently-loaded scenes that carries a script of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	template<typename T, typename Fn>
	void ForEachScriptInLoadedScenes(Fn&& fn)
	{
		const uint32_t uSlotCount = g_xEngine.Scenes().GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_ScriptComponent>().ForEach(
				[&fn](Zenith_EntityID xId, Zenith_ScriptComponent& xScript)
				{
					T* pxT = xScript.GetScript<T>();
					if (pxT != nullptr)
					{
						fn(xId, *pxT);
					}
				});
		}
	}
}
