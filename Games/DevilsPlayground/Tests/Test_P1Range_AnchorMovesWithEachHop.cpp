#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Component.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Range_AnchorMovesWithEachHop (MVP-1.8 follow-up coverage gap)
//
// MVP-1.8's two existing tests (`RefusedOutOfRange`, `AcceptedInRange`)
// only check ONE hop from the anchor. A regression where the anchor
// was set on the first possession and never updated (i.e., stuck at
// the run-start position) would still pass both tests because each
// only validates a single A->B switch.
//
// This test pins the GDD's "demon-hop chain" semantic: each successful
// voluntary switch redefines the anchor circle for the next hop.
//
// Procedure:
//   1. Load GameLevel.
//   2. Find three villagers A, B, C such that:
//        d(A, B) <= range_from_anchor_m  (A->B is a legal hop)
//        d(B, C) <= range_from_anchor_m  (B->C is legal IF anchor moved)
//        d(A, C) >  range_from_anchor_m  (B->C would be REFUSED if anchor stuck on A)
//      The gap between d(B, C) and d(A, C) is what makes this test
//      meaningfully distinguish "anchor moves" from "anchor stuck".
//   3. SetPossessedVillager(A) -- direct write, seeds anchor at A.
//   4. TryVoluntaryPossessSwitch(B) -- legal, anchor moves to B per
//      the impl's intent. Cooldown armed for ~1.5 s.
//   5. Wait the cooldown out.
//   6. TryVoluntaryPossessSwitch(C). With anchor at B this succeeds
//      (d(B, C) <= 15 m). With anchor stuck at A it would refuse
//      (d(A, C) > 15 m).
//   7. Assert the switch succeeded AND possession is now C.
//
// What this catches:
//   * A regression where `TryVoluntaryPossessSwitch` updates
//     `g_xPossessedVillager` but FORGETS to update
//     `g_xPossessionAnchor` (anchor stays on whatever the first
//     possession was).
//   * A semantic refactor to "anchor = run-start position" -- the
//     test would fail noisily because the run started at A and
//     C would be out of range.
// ============================================================================

namespace
{
	enum Phase : int { kAH_Start, kAH_WaitScene, kAH_PossessA, kAH_HopToB,
	                   kAH_WaitCooldown, kAH_HopToC, kAH_Verify, kAH_Done };

	int                     g_iPhase = kAH_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EntityID         g_xC;
	float                   g_fDistAB = 0.0f;
	float                   g_fDistBC = 0.0f;
	float                   g_fDistAC = 0.0f;
	bool                    g_bHopAToBOk = false;
	bool                    g_bHopBToCOk = false;
	Zenith_EntityID         g_xFinalPossession;
	int                     g_iCooldownWait = 0;

	// 1.5 s cooldown default -> ~95 frames at fixed 60 Hz dt.
	constexpr int kCOOLDOWN_FRAMES = 110;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	// Search GameLevel for a {A, B, C} trio satisfying the distance
	// inequality above. Brute-force O(n^3) on 17 villagers = 4913
	// candidates -- trivially fast.
	bool FindHopChainTrio(Zenith_EntityID& xA,
	                     Zenith_EntityID& xB,
	                     Zenith_EntityID& xC,
	                     float& fDAB, float& fDBC, float& fDAC)
	{
		const float fRange =
			DP_Tuning::Get<float>("possession.range_from_anchor_m");

		struct VPos { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<VPos> axVs;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&axVs](Zenith_EntityID xId, DPVillager_Component&)
			{
				VPos xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axVs.PushBack(xV);
			});
		if (axVs.GetSize() < 3) return false;

		for (uint32_t i = 0; i < axVs.GetSize(); ++i)
		for (uint32_t j = 0; j < axVs.GetSize(); ++j)
		for (uint32_t k = 0; k < axVs.GetSize(); ++k)
		{
			if (i == j || j == k || i == k) continue;
			const float fAB = HorizontalDistance(axVs.Get(i).xPos, axVs.Get(j).xPos);
			const float fBC = HorizontalDistance(axVs.Get(j).xPos, axVs.Get(k).xPos);
			const float fAC = HorizontalDistance(axVs.Get(i).xPos, axVs.Get(k).xPos);
			if (fAB <= fRange && fBC <= fRange && fAC > fRange)
			{
				xA = axVs.Get(i).xId;
				xB = axVs.Get(j).xId;
				xC = axVs.Get(k).xId;
				fDAB = fAB; fDBC = fBC; fDAC = fAC;
				return true;
			}
		}
		return false;
	}
}

static void Setup_P1AnchorMoves()
{
	g_iPhase = kAH_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_xC = INVALID_ENTITY_ID;
	g_fDistAB = g_fDistBC = g_fDistAC = 0.0f;
	g_bHopAToBOk = false;
	g_bHopBToCOk = false;
	g_xFinalPossession = INVALID_ENTITY_ID;
	g_iCooldownWait = 0;
}

static bool Step_P1AnchorMoves(int iFrame)
{
	switch (g_iPhase)
	{
	case kAH_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kAH_WaitScene;
		return true;

	case kAH_WaitScene:
		if (FindHopChainTrio(g_xA, g_xB, g_xC, g_fDistAB, g_fDistBC, g_fDistAC))
		{
			g_iPhase = kAH_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kAH_Done;
		}
		return true;

	case kAH_PossessA:
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kAH_HopToB;
		return true;

	case kAH_HopToB:
		g_bHopAToBOk = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iCooldownWait = 0;
		g_iPhase = kAH_WaitCooldown;
		return true;

	case kAH_WaitCooldown:
		++g_iCooldownWait;
		if (g_iCooldownWait >= kCOOLDOWN_FRAMES)
		{
			g_iPhase = kAH_HopToC;
		}
		return true;

	case kAH_HopToC:
		g_bHopBToCOk = DP_Player::TryVoluntaryPossessSwitch(g_xC);
		g_xFinalPossession = DP_Player::GetPossessedVillager();
		g_iPhase = kAH_Verify;
		return true;

	case kAH_Verify:
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AnchorMoves: AB=%.1fm BC=%.1fm AC=%.1fm hopAB=%d hopBC=%d possessionFinal=(%u/%u) expected=(%u/%u)",
			g_fDistAB, g_fDistBC, g_fDistAC,
			(int)g_bHopAToBOk, (int)g_bHopBToCOk,
			g_xFinalPossession.m_uIndex, g_xFinalPossession.m_uGeneration,
			g_xC.m_uIndex, g_xC.m_uGeneration);
		g_iPhase = kAH_Done;
		return false;

	case kAH_Done:
	default:
		return false;
	}
}

static bool Verify_P1AnchorMoves()
{
	const float fRange = DP_Tuning::Get<float>("possession.range_from_anchor_m");
	if (!g_xA.IsValid() || !g_xB.IsValid() || !g_xC.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AnchorMoves: failed to find a 3-villager hop chain in GameLevel (need d(A,B) <= %.1f, d(B,C) <= %.1f, d(A,C) > %.1f)",
			fRange, fRange, fRange);
		return false;
	}
	// Pre-flight: make sure the trio actually has the inequality we
	// claimed. Catches an O(n^3) bug in the picker, not the SUT.
	if (g_fDistAB > fRange || g_fDistBC > fRange || g_fDistAC <= fRange)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AnchorMoves: trio inequality broken: AB=%.1f BC=%.1f AC=%.1f range=%.1f",
			g_fDistAB, g_fDistBC, g_fDistAC, fRange);
		return false;
	}
	if (!g_bHopAToBOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1AnchorMoves: A->B hop refused -- AB distance was supposed to be in range");
		return false;
	}
	if (!g_bHopBToCOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AnchorMoves: B->C hop refused (AB=%.1f BC=%.1f AC=%.1f). If d(B,C) is in range and d(A,C) is out, the anchor is STUCK on A -- range gate uses the wrong reference",
			g_fDistAB, g_fDistBC, g_fDistAC);
		return false;
	}
	if (g_xFinalPossession.m_uIndex != g_xC.m_uIndex
		|| g_xFinalPossession.m_uGeneration != g_xC.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1AnchorMoves: B->C reported success but possession isn't C");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1AnchorMovesTest = {
	"Test_P1Range_AnchorMovesWithEachHop",
	&Setup_P1AnchorMoves,
	&Step_P1AnchorMoves,
	&Verify_P1AnchorMoves,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1AnchorMovesTest);

#endif // ZENITH_INPUT_SIMULATOR
