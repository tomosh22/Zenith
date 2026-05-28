#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Range_AcceptedInRange (MVP-1.8.3)
//
// Positive-case counterpart to Test_P1Range_RefusedOutOfRange: when the
// target villager is WITHIN `possession.range_from_anchor_m` (default
// 15 m) of the anchor, `TryVoluntaryPossessSwitch` must SUCCEED (modulo
// the cooldown gate, which the test setup steers clear of).
//
// Procedure:
//   1. Load GameLevel.
//   2. Find the closest pair of villagers anywhere in the level.
//      (Blacksmith <-> Blacksmith2 ~3.7 m in stock authoring; well
//      under the 15 m gate.)
//   3. SetPossessedVillager(A) -- direct write seeds the anchor at A's
//      world position, no cooldown.
//   4. TryVoluntaryPossessSwitch(B) -- range gate sees fDist < 15 m,
//      cooldown gate sees 0, so the call must SUCCEED.
//   5. Assert the call returned true AND DP_Player now reports B as
//      the possessed villager.
//
// Combined with the OutOfRange test, this proves the range gate fires
// on the right side of the threshold (refuses past, accepts within)
// rather than being a constant-FAIL bug.
// ============================================================================

namespace
{
	enum Phase : int { kRA_Start, kRA_WaitScene, kRA_PossessA,
	                   kRA_TrySwitchB, kRA_Done };

	int                     g_iPhase = kRA_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	float                   g_fSeparation = 0.0f;
	bool                    g_bSwitchOk = false;
	Zenith_EntityID         g_xPossessionAfterTry;

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
}

static void Setup_P1RangeAccepted()
{
	g_iPhase = kRA_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_fSeparation = 0.0f;
	g_bSwitchOk = false;
	g_xPossessionAfterTry = INVALID_ENTITY_ID;
}

static bool Step_P1RangeAccepted(int iFrame)
{
	switch (g_iPhase)
	{
	case kRA_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kRA_WaitScene;
		return true;

	case kRA_WaitScene:
	{
		// Find the closest pair of villagers. Same O(n^2) shape as the
		// OutOfRange test, opposite extremum.
		struct VillagerPos { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<VillagerPos> axCands;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axCands]
			(Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				VillagerPos xV;
				xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos))
				{
					axCands.PushBack(xV);
				}
			});
		if (axCands.GetSize() >= 2)
		{
			float fMin = 1e30f;
			for (uint32_t i = 0; i < axCands.GetSize(); ++i)
			{
				for (uint32_t j = i + 1; j < axCands.GetSize(); ++j)
				{
					const float fD = HorizontalDistance(
						axCands.Get(i).xPos, axCands.Get(j).xPos);
					if (fD < fMin)
					{
						fMin = fD;
						g_xA = axCands.Get(i).xId;
						g_xB = axCands.Get(j).xId;
					}
				}
			}
			g_fSeparation = fMin;
			g_iPhase = kRA_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kRA_Done;
		}
		return true;
	}

	case kRA_PossessA:
		// Direct write seeds the anchor at A's world position, with no
		// cooldown. The subsequent TryVoluntaryPossessSwitch is gated
		// only by range (which should pass) -- if it failed, we'd know
		// the impl is broken.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kRA_TrySwitchB;
		return true;

	case kRA_TrySwitchB:
		g_bSwitchOk = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_xPossessionAfterTry = DP_Player::GetPossessedVillager();
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeAccepted: separation=%.2fm switchOk=%d possessionAfter=(%u/%u) target=(%u/%u)",
			g_fSeparation, (int)g_bSwitchOk,
			g_xPossessionAfterTry.m_uIndex, g_xPossessionAfterTry.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		g_iPhase = kRA_Done;
		return false;

	case kRA_Done:
	default:
		return false;
	}
}

static bool Verify_P1RangeAccepted()
{
	const float fMaxRange = DP_Tuning::Get<float>("possession.range_from_anchor_m");
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1RangeAccepted: failed to pick a closest pair");
		return false;
	}
	if (g_fSeparation > fMaxRange)
	{
		// Sanity guard: GameLevel must contain at least one pair within
		// the range threshold, otherwise this test has no teeth. With
		// stock 17-villager authoring the closest pair is ~3.7 m vs a
		// 15 m threshold; if a future GameLevel rework spreads villagers
		// out so the closest pair exceeds 15 m, surface that here.
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeAccepted: closest villager pair %.2fm exceeds range threshold %.2fm -- test setup is meaningless",
			g_fSeparation, fMaxRange);
		return false;
	}
	if (!g_bSwitchOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeAccepted: switch to villager %.2fm away was REFUSED (range gate is too strict)",
			g_fSeparation);
		return false;
	}
	if (g_xPossessionAfterTry.m_uIndex != g_xB.m_uIndex
		|| g_xPossessionAfterTry.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeAccepted: accepted switch did not move possession to B (%u/%u)",
			g_xB.m_uIndex, g_xB.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1RangeAcceptedTest = {
	"Test_P1Range_AcceptedInRange",
	&Setup_P1RangeAccepted,
	&Step_P1RangeAccepted,
	&Verify_P1RangeAccepted,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1RangeAcceptedTest);

#endif // ZENITH_INPUT_SIMULATOR
