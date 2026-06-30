#pragma once

// =============================================================================
// Zenith_TempScene — RAII scope for the isolated-empty-scene unit-test bracket.
//
// Replaces the hand-repeated
//   Zenith_Scene s = g_xEngine.Scenes().LoadScene(name, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
//   g_xEngine.Scenes().SetActiveScene(s);
//   Zenith_SceneData* pd = g_xEngine.Scenes().GetSceneData(s);
//   ... CreateEntity(pd, ...) ...
//   g_xEngine.Scenes().UnloadSceneForced(s);   // easy to forget -> scene leaks
// with one local whose destructor force-unloads even on an early return, so a
// test can never leak its scene into the next test.
//
// PLACEMENT: this is an ENGINE/TEST-layer helper (it names g_xEngine.Scenes()),
// so it lives under Zenith/UnitTests/, NOT in any ZenithECS public header — the
// ECS leaf surface (and the SentinelECS link-proof) stays untouched. It is NOT
// gated on ZENITH_TOOLS so the _False Physics / Graph / Light suites can use it.
//
// The scene name is made process-unique (a monotonic suffix) so two temp scenes
// staged at the same time never collide on the name cache.
// =============================================================================

#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include <string>

struct Zenith_TempScene
{
	// Mint a fresh empty additive scene and make it the active scene. The base
	// name is suffixed with a monotonic counter so the name is process-unique.
	explicit Zenith_TempScene(const char* szBaseName)
	{
		static u_int ls_uCounter = 0;
		const std::string strName = std::string(szBaseName) + "_" + std::to_string(ls_uCounter++);
		m_xScene = g_xEngine.Scenes().LoadScene(strName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xScene);
		m_pxData = g_xEngine.Scenes().GetSceneData(m_xScene);
	}

	~Zenith_TempScene()
	{
		g_xEngine.Scenes().UnloadSceneForced(m_xScene);
	}

	Zenith_TempScene(const Zenith_TempScene&) = delete;
	Zenith_TempScene& operator=(const Zenith_TempScene&) = delete;

	// The scene's storage pointer (raw + recyclable — valid for this scope only).
	Zenith_SceneData* Data() const { return m_pxData; }
	Zenith_Scene Scene() const { return m_xScene; }

	// Convenience: create a (non-bare) entity in this scene.
	Zenith_Entity CreateEntity(const char* szName)
	{
		return g_xEngine.Scenes().CreateEntity(m_pxData, szName);
	}

	Zenith_Scene m_xScene;
	Zenith_SceneData* m_pxData = nullptr;
};
