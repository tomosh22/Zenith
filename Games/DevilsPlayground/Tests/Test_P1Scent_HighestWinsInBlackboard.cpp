#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/Priest_Component.h"
#include "Components/DPVillager_Component.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Scent_HighestWinsInBlackboard (MVP-1.6 follow-up coverage gap)
//
// `Test_P1Scent_NotificationToBlackboard` proves the BB receives a
// non-null scent target after a single switch -- but with only ONE
// villager bumped, "highest" trivially equals "the only one". A
// regression where `WriteHighestScentToBlackboard` got rewritten as
// "most recently bumped" or "first found" would silently pass that
// test.
//
// This test bumps TWO villagers different amounts (B twice, A once)
// AND ensures A is the most recently bumped villager, then asserts
// the priest's BB reads B (the higher scent) -- not A (the most
// recent).
//
// Procedure:
//   1. Load GameLevel + find priest + two villagers in mutual range.
//   2. SetPossessedVillager(A) -- system path, no scent bump.
//   3. TryVoluntaryPossessSwitch(B) -- scent[B] = 0.3. Wait cooldown.
//   4. SetPossessedVillager(A) -- system path BACK TO A, no scent
//      bump. (Using SetPossessedVillager here is the trick: it re-
//      anchors at A without crediting A any scent. A regular
//      TryVoluntaryPossessSwitch(A) would bump scent[A] and ruin the
//      asymmetry we need.)
//   5. TryVoluntaryPossessSwitch(B) -- scent[B] += 0.3. (With ~2s of
//      decay from step 3 the previous deposit is ~0.21, so total
//      scent[B] ~= 0.51.) Wait cooldown.
//   6. TryVoluntaryPossessSwitch(A) -- scent[A] = 0.3, A is now the
//      most recently bumped villager. scent[B] decayed ~0.09 in this
//      cooldown wait so it's ~0.42 -- still higher than A's 0.3.
//   7. Wait one frame for the controller's per-frame
//      WriteHighestScentToBlackboard to run.
//   8. Read priest's BB_KEY_HIGH_SCENT_TARGET.
//   9. Assert BB target == B (higher scent), NOT A (most recent).
//
// What this catches:
//   * A refactor that swapped `>` for `<` in the scan (lowest wins).
//   * A refactor to "most recently bumped" semantics (would write A).
//   * A regression where the scan returned the first map entry rather
//     than the max-by-value entry (would write whichever villager
//     happened to hash first in the unordered_map -- non-deterministic
//     and rarely matches the highest).
// ============================================================================

namespace
{
	enum Phase : int { kHW_Start, kHW_WaitScene, kHW_PossessA, kHW_HopBFirst,
	                   kHW_Wait1, kHW_ReanchorA, kHW_HopBSecond,
	                   kHW_Wait2, kHW_HopAFinal, kHW_WaitForBBWrite,
	                   kHW_Verify, kHW_Done };

	int                     g_iPhase = kHW_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EntityID         g_xBBTarget;
	float                   g_fScentAFinal = 0.0f;
	float                   g_fScentBFinal = 0.0f;
	int                     g_iCooldownWait = 0;

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

	void PickClosestPair(Zenith_EntityID& xA, Zenith_EntityID& xB)
	{
		struct VPos { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<VPos> axVs;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&axVs](Zenith_EntityID xId, DPVillager_Component&)
			{
				VPos xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axVs.PushBack(xV);
			});
		if (axVs.GetSize() < 2) return;
		float fMin = 1e30f;
		for (uint32_t i = 0; i < axVs.GetSize(); ++i)
		for (uint32_t j = i + 1; j < axVs.GetSize(); ++j)
		{
			const float fD = HorizontalDistance(axVs.Get(i).xPos, axVs.Get(j).xPos);
			if (fD < fMin) { fMin = fD; xA = axVs.Get(i).xId; xB = axVs.Get(j).xId; }
		}
	}

	Zenith_EntityID ReadPriestBBHighScent(Zenith_EntityID xPriestId)
	{
		Zenith_SceneData* pxScene =
			g_xEngine.Scenes().GetSceneDataForEntity(xPriestId);
		if (pxScene == nullptr) return INVALID_ENTITY_ID;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
		if (!xEnt.IsValid()) return INVALID_ENTITY_ID;
		if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return INVALID_ENTITY_ID;
		Zenith_AIAgentComponent& xAg = xEnt.GetComponent<Zenith_AIAgentComponent>();
		return xAg.GetBlackboard().GetEntityID(DP_AI::BB_KEY_HIGH_SCENT_TARGET);
	}
}

static void Setup_P1ScentHighest()
{
	g_iPhase = kHW_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_xBBTarget = INVALID_ENTITY_ID;
	g_fScentAFinal = 0.0f;
	g_fScentBFinal = 0.0f;
	g_iCooldownWait = 0;
}

static bool Step_P1ScentHighest(int iFrame)
{
	switch (g_iPhase)
	{
	case kHW_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kHW_WaitScene;
		return true;

	case kHW_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Component&)
			{ xFoundPriest = xId; });
		PickClosestPair(g_xA, g_xB);
		if (xFoundPriest.IsValid() && g_xA.IsValid() && g_xB.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_iPhase = kHW_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kHW_Done;
		}
		return true;
	}

	case kHW_PossessA:
		// System path -- anchor at A, no scent.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kHW_HopBFirst;
		return true;

	case kHW_HopBFirst:
		// scent[B] = 0.3, anchor moves to B, cooldown armed.
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iCooldownWait = 0;
		g_iPhase = kHW_Wait1;
		return true;

	case kHW_Wait1:
		if (++g_iCooldownWait >= kCOOLDOWN_FRAMES) g_iPhase = kHW_ReanchorA;
		return true;

	case kHW_ReanchorA:
		// SYSTEM path back to A. No scent bump for A. Anchor moves
		// to A. The next TryVoluntaryPossessSwitch(B) will pay
		// scent into B again without ever crediting A.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kHW_HopBSecond;
		return true;

	case kHW_HopBSecond:
		// scent[B] += 0.3 (B now has TWO deposits minus decay). Anchor
		// moves to B again, cooldown re-armed.
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iCooldownWait = 0;
		g_iPhase = kHW_Wait2;
		return true;

	case kHW_Wait2:
		if (++g_iCooldownWait >= kCOOLDOWN_FRAMES) g_iPhase = kHW_HopAFinal;
		return true;

	case kHW_HopAFinal:
		// FINAL hop to A. scent[A] = 0.3 (single bump, fresh). A is now
		// the most recently bumped villager. scent[B] has decayed across
		// 2 cooldown windows since its 2nd bump (~1.8s), so scent[B] is
		// ~0.51 - 0.09 ~= 0.42. Higher than A's 0.3.
		DP_Player::TryVoluntaryPossessSwitch(g_xA);
		g_iPhase = kHW_WaitForBBWrite;
		return true;

	case kHW_WaitForBBWrite:
		// One frame so the controller's per-frame
		// `WriteHighestScentToBlackboard` runs.
		g_xBBTarget = ReadPriestBBHighScent(g_xPriest);
		g_fScentAFinal = DP_Player::GetDemonScent(g_xA);
		g_fScentBFinal = DP_Player::GetDemonScent(g_xB);
		g_iPhase = kHW_Verify;
		return true;

	case kHW_Verify:
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentHighest: scent[A]=%.3f scent[B]=%.3f bbTarget=(%u/%u) A=(%u/%u) B=(%u/%u)",
			g_fScentAFinal, g_fScentBFinal,
			g_xBBTarget.m_uIndex, g_xBBTarget.m_uGeneration,
			g_xA.m_uIndex, g_xA.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		g_iPhase = kHW_Done;
		return false;

	case kHW_Done:
	default:
		return false;
	}
}

static bool Verify_P1ScentHighest()
{
	if (!g_xPriest.IsValid() || !g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentHighest: priest/villager pick failed");
		return false;
	}
	// Pre-flight: confirm our test setup actually produced scent[B] > scent[A].
	// If decay was so aggressive (or accumulation so weak) that B isn't
	// higher, the assertion below would be meaningless.
	if (g_fScentBFinal <= g_fScentAFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentHighest: scent[B]=%.3f not > scent[A]=%.3f -- test setup didn't produce the expected ordering (decay too aggressive or accumulation cap hit)",
			g_fScentBFinal, g_fScentAFinal);
		return false;
	}
	if (g_xBBTarget.m_uIndex != g_xB.m_uIndex
		|| g_xBBTarget.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentHighest: BB target = (%u/%u), expected B=(%u/%u) (scent[B]=%.3f > scent[A]=%.3f). Possible impl bug: WriteHighestScentToBlackboard is using 'most recently bumped' instead of 'highest scent'",
			g_xBBTarget.m_uIndex, g_xBBTarget.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration,
			g_fScentBFinal, g_fScentAFinal);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ScentHighestTest = {
	"Test_P1Scent_HighestWinsInBlackboard",
	&Setup_P1ScentHighest,
	&Step_P1ScentHighest,
	&Verify_P1ScentHighest,
	600
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ScentHighestTest);

#endif // ZENITH_INPUT_SIMULATOR
