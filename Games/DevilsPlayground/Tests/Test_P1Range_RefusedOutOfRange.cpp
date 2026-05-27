#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Range_RefusedOutOfRange (MVP-1.8.1 + 1.8.2)
//
// Verifies the possession-range gate added by MVP-1.8.2. After the
// anchor is seeded by a successful possession, voluntary switches to a
// villager OUTSIDE `possession.range_from_anchor_m` (default 15 m) MUST
// be refused.
//
// Procedure:
//   1. Load GameLevel.
//   2. Pick two villagers separated by >> 15 m (closest <-> farthest
//      pair from an anchor reference -- in stock GameLevel this is
//      ~65-80 m horizontally).
//   3. SetPossessedVillager(near) -- direct write seeds the anchor at
//      the near villager's world position.
//   4. TryVoluntaryPossessSwitch(far) -- the range gate must REFUSE
//      because horizontal distance(near.pos, far.pos) > 15 m.
//   5. Assert the call returned false AND DP_Player still has the
//      anchor villager possessed (not the far one, not INVALID).
// ============================================================================

namespace
{
	enum Phase : int { kRR_Start, kRR_WaitScene, kRR_PossessNear,
	                   kRR_TrySwitchFar, kRR_Done };

	int                     g_iPhase = kRR_Start;
	Zenith_EntityID         g_xNear;
	Zenith_EntityID         g_xFar;
	float                   g_fSeparation = 0.0f;
	bool                    g_bSwitchOk = true;  // tracks return value; pre-set to "fail" sentinel
	Zenith_EntityID         g_xPossessionAfterTry;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(xId);
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

static void Setup_P1RangeRefused()
{
	g_iPhase = kRR_Start;
	g_xNear = INVALID_ENTITY_ID;
	g_xFar = INVALID_ENTITY_ID;
	g_fSeparation = 0.0f;
	g_bSwitchOk = true;
	g_xPossessionAfterTry = INVALID_ENTITY_ID;
}

static bool Step_P1RangeRefused(int iFrame)
{
	switch (g_iPhase)
	{
	case kRR_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kRR_WaitScene;
		return true;

	case kRR_WaitScene:
	{
		// Pick the most-separated pair among all DPVillager entities.
		// 17 villagers in GameLevel; O(n^2) at 17 is 289 distance
		// computations -- trivial.
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
			float fMax = -1.0f;
			for (uint32_t i = 0; i < axCands.GetSize(); ++i)
			{
				for (uint32_t j = i + 1; j < axCands.GetSize(); ++j)
				{
					const float fD = HorizontalDistance(
						axCands.Get(i).xPos, axCands.Get(j).xPos);
					if (fD > fMax)
					{
						fMax = fD;
						g_xNear = axCands.Get(i).xId;
						g_xFar  = axCands.Get(j).xId;
					}
				}
			}
			g_fSeparation = fMax;
			g_iPhase = kRR_PossessNear;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kRR_Done;
		}
		return true;
	}

	case kRR_PossessNear:
		// Seed the anchor by directly possessing the "near" villager
		// (which is one half of the most-separated pair; the names
		// "near" and "far" are just labels for "anchor" and "the one
		// we're trying to switch to"). SetPossessedVillager is the
		// system path -- no cooldown, no gate -- so the next switch
		// attempt is gated only by range, not by a hangover cooldown.
		DP_Player::SetPossessedVillager(g_xNear);
		g_iPhase = kRR_TrySwitchFar;
		return true;

	case kRR_TrySwitchFar:
		g_bSwitchOk = DP_Player::TryVoluntaryPossessSwitch(g_xFar);
		g_xPossessionAfterTry = DP_Player::GetPossessedVillager();
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeRefused: separation=%.2fm switchOk=%d possessionAfter=(%u/%u) anchor=(%u/%u)",
			g_fSeparation, (int)g_bSwitchOk,
			g_xPossessionAfterTry.m_uIndex, g_xPossessionAfterTry.m_uGeneration,
			g_xNear.m_uIndex, g_xNear.m_uGeneration);
		g_iPhase = kRR_Done;
		return false;

	case kRR_Done:
	default:
		return false;
	}
}

static bool Verify_P1RangeRefused()
{
	const float fMaxRange = DP_Tuning::Get<float>("possession.range_from_anchor_m");
	if (!g_xNear.IsValid() || !g_xFar.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1RangeRefused: failed to pick a villager pair");
		return false;
	}
	if (g_fSeparation <= fMaxRange)
	{
		// Sanity guard: GameLevel must contain at least one pair separated
		// by more than the range threshold, otherwise this test has no
		// teeth. With the stock 17-villager spread the max separation is
		// ~80 m vs a 15 m threshold; if a future GameLevel rework drops
		// below that, surface the test-design break.
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeRefused: max villager separation %.2fm is within range threshold %.2fm -- test setup is meaningless",
			g_fSeparation, fMaxRange);
		return false;
	}
	if (g_bSwitchOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeRefused: switch to far villager %.2fm away was ALLOWED (range gate didn't fire)",
			g_fSeparation);
		return false;
	}
	if (g_xPossessionAfterTry.m_uIndex != g_xNear.m_uIndex
		|| g_xPossessionAfterTry.m_uGeneration != g_xNear.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1RangeRefused: rejected switch left possession on (%u/%u) instead of anchor (%u/%u)",
			g_xPossessionAfterTry.m_uIndex, g_xPossessionAfterTry.m_uGeneration,
			g_xNear.m_uIndex, g_xNear.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1RangeRefusedTest = {
	"Test_P1Range_RefusedOutOfRange",
	&Setup_P1RangeRefused,
	&Step_P1RangeRefused,
	&Verify_P1RangeRefused,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1RangeRefusedTest);

#endif // ZENITH_INPUT_SIMULATOR
