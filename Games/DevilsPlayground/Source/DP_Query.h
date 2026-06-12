#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"

#include <cstdint>

// ============================================================================
// DP_Query — component-iteration helpers. Game components live in their own
// pools, so these are thin wrappers over the scene's Query<T>() that keep the
// pre-migration callback shape (Zenith_EntityID, T&) and the null-scene
// guard in one place. Header-only because the templates instantiate per-T at
// call sites.
// ============================================================================
namespace DP_Query
{
	// Iterate every entity in the active scene that carries a component of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	template<typename T, typename Fn>
	void ForEachComponentInActiveScene(Fn&& fn)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) return;
		pxScene->Query<T>().ForEach(
			[&fn](Zenith_EntityID xId, T& xComponent)
			{
				fn(xId, xComponent);
			});
	}

	// Iterate every entity in ALL currently-loaded scenes that carries a component of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	template<typename T, typename Fn>
	void ForEachComponentInLoadedScenes(Fn&& fn)
	{
		const uint32_t uSlotCount = g_xEngine.Scenes().GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<T>().ForEach(
				[&fn](Zenith_EntityID xId, T& xComponent)
				{
					fn(xId, xComponent);
				});
		}
	}
}
