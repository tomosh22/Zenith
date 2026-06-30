#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Component.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P2Fog_AelfricNotRevealed (MVP-2.4.3)
//
// Pins the GDD §4.6 contract: "Aelfric does not carve a fog hole."
// The fog system reveals the area around possessed/un-possessed
// villagers and around lights -- but the priest (Aelfric) must
// remain invisible in fog, so the player can't read the priest's
// position through fog when he's outside their direct sight cone.
// This is a core piece of the horror loop: the priest hunts you in
// the dark.
//
// Procedure:
//   1. Load GameLevel.
//   2. Find the priest entity.
//   3. Tick a few frames so DPFogPass_Component::OnUpdate runs at
//      least once (which clears + rebuilds the fog hole table).
//   4. Snapshot the priest's world position.
//   5. Gather every fog hole's position via DP_Fog::GatherFogHolePositions.
//   6. Assert: no fog hole is centered within 0.5 m of the priest's
//      position (i.e., no fog hole was registered for the priest
//      entity).
//
// What this catches:
//   * A regression where DPFogPass starts iterating Priest_Component
//     scripts in the same loop as DPVillager_Component and registers
//     a hole for the priest.
//   * A regression where the priest is given a light component (e.g.,
//     a lantern in a future visual polish pass) and the light-fog-hole
//     code path accidentally also reveals the priest's silhouette.
//     If/when the priest legitimately gets a lantern, the fog system
//     needs explicit logic to size the lantern hole conservatively or
//     skip it entirely -- this test surfaces that decision rather
//     than letting it slip in silently.
// ============================================================================

namespace
{
	enum Phase : int { kFR_Start, kFR_WaitScene, kFR_Tick, kFR_Snapshot,
	                   kFR_Verify, kFR_Done };

	int                     g_iPhase = kFR_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_Maths::Vector3   g_xPriestPos(0.0f);
	uint32_t                g_uHoleCount = 0;
	float                   g_fClosestHoleDist = 1e30f;
	int                     g_iTickFrames = 0;

	// 10 frames is plenty for DPFogPass::OnUpdate to run at least
	// once. The fog hole table is cleared-and-rebuilt each frame so
	// we only need one OnUpdate firing.
	constexpr int kTICK_FRAMES = 10;
	// 64 covers GameLevel's 17 villagers + ~30 lights with margin.
	constexpr uint32_t kMAX_HOLES = 64;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}
}

static void Setup_P2FogAelfricNotRevealed()
{
	g_iPhase = kFR_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xPriestPos = Zenith_Maths::Vector3(0.0f);
	g_uHoleCount = 0;
	g_fClosestHoleDist = 1e30f;
	g_iTickFrames = 0;
}

static bool Step_P2FogAelfricNotRevealed(int iFrame)
{
	switch (g_iPhase)
	{
	case kFR_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFR_WaitScene;
		return true;

	case kFR_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFound](Zenith_EntityID xId, Priest_Component&)
			{ xFound = xId; });
		if (xFound.IsValid())
		{
			g_xPriest = xFound;
			g_iPhase = kFR_Tick;
			g_iTickFrames = 0;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kFR_Done;
		}
		return true;
	}

	case kFR_Tick:
		++g_iTickFrames;
		if (g_iTickFrames >= kTICK_FRAMES) g_iPhase = kFR_Snapshot;
		return true;

	case kFR_Snapshot:
	{
		TryGetEntityPos(g_xPriest, g_xPriestPos);
		Zenith_Maths::Vector4 axHoles[kMAX_HOLES];
		g_uHoleCount = DP_Fog::GatherFogHolePositions(axHoles, kMAX_HOLES);
		// For every hole, compute horizontal distance to priest.
		// Track the closest one; the assertion checks it against a
		// "is this centered ON the priest?" threshold.
		for (uint32_t u = 0; u < g_uHoleCount; ++u)
		{
			const float fDx = axHoles[u].x - g_xPriestPos.x;
			const float fDz = axHoles[u].z - g_xPriestPos.z;
			const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
			if (fDist < g_fClosestHoleDist) g_fClosestHoleDist = fDist;
		}
		std::printf("[P2FogAelfricNotRevealed] priestPos=(%.2f,%.2f,%.2f) holes=%u closestHoleDist=%.3f\n",
			g_xPriestPos.x, g_xPriestPos.y, g_xPriestPos.z,
			g_uHoleCount, g_fClosestHoleDist);
		std::fflush(stdout);
		g_iPhase = kFR_Verify;
		return true;
	}

	case kFR_Verify:
		g_iPhase = kFR_Done;
		return false;

	case kFR_Done:
	default:
		return false;
	}
}

static bool Verify_P2FogAelfricNotRevealed()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2FogAelfricNotRevealed: priest not found in GameLevel");
		return false;
	}
	if (g_uHoleCount == 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2FogAelfricNotRevealed: no fog holes registered at all -- DPFogPass_Component didn't OnUpdate, or there are no villagers/lights in GameLevel. Test pre-condition failed");
		return false;
	}
	// 0.5 m threshold: if a fog hole's CENTER is within half a metre
	// of the priest, that hole is "on" the priest. The closest other
	// villager / light is much farther (smallest authored spacing
	// is several metres). A regression that registered a hole for
	// the priest entity would put distance == 0 -- well below 0.5.
	const float fAelfricRevealedThreshold = 0.5f;
	if (g_fClosestHoleDist < fAelfricRevealedThreshold)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2FogAelfricNotRevealed: a fog hole was registered within %.3f m of the priest (threshold %.3f). DPFogPass_Component is revealing the priest, breaking the GDD's 'Aelfric does not carve a hole' contract",
			g_fClosestHoleDist, fAelfricRevealedThreshold);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2FogAelfricNotRevealedTest = {
	"Test_P2Fog_AelfricNotRevealed",
	&Setup_P2FogAelfricNotRevealed,
	&Step_P2FogAelfricNotRevealed,
	&Verify_P2FogAelfricNotRevealed,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2FogAelfricNotRevealedTest);

#endif // ZENITH_INPUT_SIMULATOR
