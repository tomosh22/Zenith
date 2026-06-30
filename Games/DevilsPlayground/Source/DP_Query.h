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
	// Thin forward to the engine's QueryActiveScene<T> (which owns the active-scene
	// resolution + the no-active-scene null guard).
	template<typename T, typename Fn>
	void ForEachComponentInActiveScene(Fn&& fn)
	{
		g_xEngine.Scenes().QueryActiveScene<T>().ForEach(
			[&fn](Zenith_EntityID xId, T& xComponent)
			{
				fn(xId, xComponent);
			});
	}

	// Iterate every entity in ALL currently-loaded scenes that carries a component of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	// Thin forward to the engine's QueryAllScenes<T> (the canonical all-loaded-scenes form).
	template<typename T, typename Fn>
	void ForEachComponentInLoadedScenes(Fn&& fn)
	{
		g_xEngine.Scenes().QueryAllScenes<T>().ForEach(
			[&fn](Zenith_EntityID xId, T& xComponent)
			{
				fn(xId, xComponent);
			});
	}
}
