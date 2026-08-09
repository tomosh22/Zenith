#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "ZenithECS/Zenith_Query.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Component.h"

#include <cmath>

#include <cstdio>

// ============================================================================
// DimLightsCutFog_Test
//
// Three simultaneous invariants on the GameLevel scene:
//
//   1. Every authored light carries the scene's authored intensity, proving
//      the author-time setter ran and stuck.
//
//      RETARGETED 2026-08-09. This clause used to assert <= 200 lumens,
//      "dimmed compared to the engine default (800)", because AuthorLightBatch
//      scaled a UE-imported ~1000 down by 0.10 with a 60-lumen floor. That
//      whole pipeline -- Tools/dp_export/, the UE bridge and the hand-authored
//      GameLevel scene -- was DELETED on 2026-05-19; procgen is the only
//      gameplay surface now and DevilsPlayground.cpp authors its lights at
//      2000 lumens outright (AddStep_SetLightIntensity(2000.0f)). The clause
//      had been asserting a property of deleted content ever since, and nobody
//      saw it because the test is m_bRequiresGraphics: the headless gate SKIPS
//      it and counts it as passed. Same rot, same cause, as Materials_Test and
//      Test_GraphEditorLiveAuthoring (see Tests/CLAUDE.md).
//
//   2. *All* villagers (possessed or not) register fog holes — not just
//      the currently-possessed one. Player needs to see every villager
//      to pick a successor when the host dies.
//
//   3. DPFogPass_Component::OnUpdate has rebuilt the fog-hole table this
//      frame, registering a hole per light + a hole per villager.
//      Verify hole count == LightCount + VillagerCount. (Clauses 2 and 3 are
//      the test's real subject and have been green throughout: the procgen
//      level reads 4 lights + 17 villagers = 21 holes.)
// ============================================================================

namespace
{
	// Mirrors AddStep_SetLightIntensity(2000.0f) in DevilsPlayground.cpp's
	// procgen light authoring -- deliberately spelled out here so that editing
	// one without the other trips this test.
	constexpr float fDP_AUTHORED_LIGHT_LUMENS = 2000.0f;

	enum Phase : int { kStart, kWait, kPossess, kSettle, kVerify, kDone };

	int   g_iPhase             = kStart;
	int   g_iWait              = 0;
	int   g_iLightCount        = 0;
	int   g_iVillagerCount     = 0;
	float g_fMaxLightIntensity = 0.0f;
	float g_fMinLightIntensity = 1e9f;
	uint32_t g_uFogHoleCount   = 0;
	bool  g_bAllLightsDim      = true;
}

static void Setup_DimLightsCutFog()
{
	g_iPhase = kStart;
	g_iWait = 0;
	g_iLightCount        = 0;
	g_iVillagerCount     = 0;
	g_fMaxLightIntensity = 0.0f;
	g_fMinLightIntensity = 1e9f;
	g_uFogHoleCount      = 0;
	g_bAllLightsDim      = true;
}

static bool Step_DimLightsCutFog(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		++g_iWait;
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxScene)
		{
			int iLights = 0;
			pxScene->Query<Zenith_LightComponent>().ForEach(
				[&iLights](Zenith_EntityID, Zenith_LightComponent&) { ++iLights; });
			if (iLights > 0)
			{
				g_iPhase = kPossess;
				return true;
			}
		}
		if (g_iWait > 60) { g_iPhase = kDone; return false; }
		return true;
	}

	case kPossess:
	{
		// No possession needed: the fog system now registers a hole for
		// EVERY villager regardless of possession state. Skip directly
		// to the settle phase — possessing an entity here would only
		// muddle the count assertion below.
		g_iPhase = kSettle;
		g_iWait = 0;
		return true;
	}

	case kSettle:
	{
		// One frame for DPFogPass_Component::OnUpdate to clear-and-rebuild
		// the fog-hole table after the scene-load completes.
		++g_iWait;
		if (g_iWait < 3) return true;

		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxScene)
		{
			pxScene->Query<Zenith_LightComponent>().ForEach(
				[](Zenith_EntityID, Zenith_LightComponent& xLight)
				{
					++g_iLightCount;
					const float fI = xLight.GetIntensity();
					if (fI > g_fMaxLightIntensity) g_fMaxLightIntensity = fI;
					if (fI < g_fMinLightIntensity) g_fMinLightIntensity = fI;
					// The authored value, not a "dim" band: DevilsPlayground.cpp
					// authors every procgen light at fDP_AUTHORED_LIGHT_LUMENS.
					if (std::fabs(fI - fDP_AUTHORED_LIGHT_LUMENS) > 1.0f) g_bAllLightsDim = false;
				});
		}
		// Count villagers via DP_Query (component-pool query).
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[](Zenith_EntityID, DPVillager_Component&) { ++g_iVillagerCount; });
		g_uFogHoleCount = DP_Fog::GetFogHoleCount();

		std::printf("[DimLightsCutFog] lights=%d villagers=%d intensity=[%.0f..%.0f] foghole=%u allDim=%d\n",
			g_iLightCount, g_iVillagerCount,
			g_fMinLightIntensity, g_fMaxLightIntensity,
			g_uFogHoleCount, (int)g_bAllLightsDim);
		std::fflush(stdout);

		g_iPhase = kVerify;
		return true;
	}

	case kVerify:
		g_iPhase = kDone;
		return false;

	case kDone:
	default:
		return false;
	}
}

static bool Verify_DimLightsCutFog()
{
	// At least one light must exist (the procgen level authors 4).
	if (g_iLightCount < 1)        return false;
	// Every light carries the authored intensity.
	if (!g_bAllLightsDim)         return false;
	// At least one villager (the procgen level authors 17).
	if (g_iVillagerCount < 1)     return false;
	// Hole count = lights + villagers. No possession is set in this
	// test, so the only way villagers contribute is the always-on path.
	const uint32_t uExpected = static_cast<uint32_t>(g_iLightCount + g_iVillagerCount);
	if (g_uFogHoleCount != uExpected) return false;
	return true;
}

static const Zenith_AutomatedTest g_xDimLightsCutFogTest = {
	"DimLightsCutFog_Test",
	&Setup_DimLightsCutFog,
	&Step_DimLightsCutFog,
	&Verify_DimLightsCutFog,
	240,
	true // m_bRequiresGraphics: light intensity readbacks need scene load with GPU-uploaded materials
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDimLightsCutFogTest);

#endif // ZENITH_INPUT_SIMULATOR
