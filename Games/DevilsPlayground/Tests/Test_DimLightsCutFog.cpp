#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Zenith_Query.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// DimLightsCutFog_Test
//
// Three simultaneous invariants on the GameLevel scene:
//
//   1. Lights are *dimmed* compared to the engine default (800 lumens).
//      AuthorLightBatch scales the UE-imported intensity (~1000) down by
//      0.10 with a 60-lumen floor. Verify every authored light reads
//      <= 200 lumens (i.e. way below the engine default — proves the
//      author-time setter ran and stuck).
//
//   2. *All* villagers (possessed or not) register fog holes — not just
//      the currently-possessed one. Player needs to see every villager
//      to pick a successor when the host dies.
//
//   3. DPFogPass_Behaviour::OnUpdate has rebuilt the fog-hole table this
//      frame, registering a hole per light + a hole per villager.
//      Verify hole count == LightCount + VillagerCount.
// ============================================================================

namespace
{
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
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		++g_iWait;
		Zenith_Scene xActive = g_xEngine.SceneRegistry().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneData(xActive);
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
		// One frame for DPFogPass_Behaviour::OnUpdate to clear-and-rebuild
		// the fog-hole table after the scene-load completes.
		++g_iWait;
		if (g_iWait < 3) return true;

		Zenith_Scene xActive = g_xEngine.SceneRegistry().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneData(xActive);
		if (pxScene)
		{
			pxScene->Query<Zenith_LightComponent>().ForEach(
				[](Zenith_EntityID, Zenith_LightComponent& xLight)
				{
					++g_iLightCount;
					const float fI = xLight.GetIntensity();
					if (fI > g_fMaxLightIntensity) g_fMaxLightIntensity = fI;
					if (fI < g_fMinLightIntensity) g_fMinLightIntensity = fI;
					// "Dim" threshold: the engine default is 800; we're
					// looking for everything well under that.
					if (fI > 200.0f) g_bAllLightsDim = false;
				});
		}
		// Count villagers via DP_Query (lives on Zenith_ScriptComponent).
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[](Zenith_EntityID, DPVillager_Behaviour&) { ++g_iVillagerCount; });
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
	// At least one light must exist (GameLevel authors 26).
	if (g_iLightCount < 1)        return false;
	// Every light dimmed below 200 lumens.
	if (!g_bAllLightsDim)         return false;
	// At least one villager (GameLevel authors 17).
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
