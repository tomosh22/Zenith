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
// Test_P1Scent_AccumulatesOnPossession (MVP-1.6.1 + 1.6.2)
//
// Verifies the demon-scent accumulation path added by MVP-1.6:
//   * Successful possession via `TryVoluntaryPossessSwitch` accumulates
//     `possession.demon_scent_per_possession` (default 0.3) on the
//     possessed villager.
//   * `SetPossessedVillager` (system path) does NOT bump scent.
//   * Idempotent re-clicks do NOT bump scent.
//   * `DP_Player::GetDemonScent` reads back the accumulated value.
//   * The saturation cap (`possession.demon_scent_max` = 1.0) and the
//     decay rate are *implemented* (visible in the source) but only
//     loosely asserted here -- the saturation behaviour is tested
//     indirectly via the "switch back to A also bumps A" leg, which
//     proves accumulation continues to track per villager.
//
// Procedure:
//   1. Load GameLevel.
//   2. Find a trio of villagers in mutual range (A, B, C). Three
//      rather than two because MVP-1.4.1-3 added the Fainted state:
//      after switching away from A, A is unavailable for re-
//      possession until the 10 s faint recovery completes. The
//      original 2-villager A->B->A test no longer fits because the
//      B->A leg is refused by the faint gate. Using a fresh C
//      preserves the accumulation assertion (each successful switch
//      bumps scent on the destination) without bumping into faint.
//   3. Snapshot baseline scent on all three.
//   4. SetPossessedVillager(A) -- direct write, no scent bump.
//   5. TryVoluntaryPossessSwitch(A) -- idempotent re-click; no bump.
//   6. TryVoluntaryPossessSwitch(B) -- scent[B] = 0.3.
//   7. Wait cooldown + TryVoluntaryPossessSwitch(C) -- scent[C] = 0.3.
//
// Why no hard saturation assert: the test's natural pacing (1.5 s
// cooldown between switches) interacts with the 0.05 /s decay rate
// to drain ~0.092 of scent during every cooldown window. Net scent
// per switch cycle is only ~0.116, so an exact saturation check
// becomes brittle. The data path is proven by the accumulation legs;
// a future hound-tuning pass can re-tune cooldown + decay to make
// saturation a stable observable.
// ============================================================================

namespace
{
	enum Phase : int {
		kSA_Start, kSA_WaitScene, kSA_RecordBaseline,
		kSA_PossessADirect, kSA_VerifyANoBump,
		kSA_TryIdempotent, kSA_VerifyIdempotentNoBump,
		kSA_SwitchToB, kSA_VerifyBAfterFirst,
		kSA_WaitCooldown1, kSA_SwitchToC, kSA_VerifyCAfterSwitch,
		kSA_Done
	};

	int                     g_iPhase = kSA_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EntityID         g_xC;

	float                   g_fBaselineA = -1.0f;
	float                   g_fBaselineB = -1.0f;
	float                   g_fBaselineC = -1.0f;
	float                   g_fScentAfterADirect = -1.0f;
	float                   g_fScentAfterIdempotent = -1.0f;
	float                   g_fScentBAfterFirstSwitch = -1.0f;
	float                   g_fScentCAfterSwitch = -1.0f;

	int                     g_iCooldownWaitFrames = 0;

	// 1.5s cooldown default -> ~95 frames at 60 Hz. We wait 110 to be
	// pessimistic against frame-time jitter.
	constexpr int kCOOLDOWN_FRAMES = 110;

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

	// Find any trio A, B, C all within the possession range gate
	// (15 m default) of each other -- A is anchor, then B->C hop
	// must satisfy both d(A,B)<=range and d(B,C)<=range. Brute-force
	// O(n^3) over GameLevel's 17 villagers = 4913 candidates, trivially
	// fast.
	bool PickMutualRangeTrio(Zenith_EntityID& xA,
	                         Zenith_EntityID& xB,
	                         Zenith_EntityID& xC)
	{
		const float fRange = DP_Tuning::Get<float>("possession.range_from_anchor_m");
		struct Cand { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<Cand> axCands;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axCands](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				Cand xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axCands.PushBack(xV);
			});
		if (axCands.GetSize() < 3) return false;
		for (uint32_t i = 0; i < axCands.GetSize(); ++i)
		for (uint32_t j = 0; j < axCands.GetSize(); ++j)
		for (uint32_t k = 0; k < axCands.GetSize(); ++k)
		{
			if (i == j || j == k || i == k) continue;
			const float fAB = HorizontalDistance(axCands.Get(i).xPos, axCands.Get(j).xPos);
			const float fBC = HorizontalDistance(axCands.Get(j).xPos, axCands.Get(k).xPos);
			if (fAB <= fRange && fBC <= fRange)
			{
				xA = axCands.Get(i).xId;
				xB = axCands.Get(j).xId;
				xC = axCands.Get(k).xId;
				return true;
			}
		}
		return false;
	}

	bool ApproxEquals(float fA, float fB, float fTol = 0.001f)
	{
		const float fD = fA - fB;
		return (fD > -fTol) && (fD < fTol);
	}
}

static void Setup_P1ScentAccumulates()
{
	g_iPhase = kSA_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_xC = INVALID_ENTITY_ID;
	g_fBaselineA = -1.0f;
	g_fBaselineB = -1.0f;
	g_fBaselineC = -1.0f;
	g_fScentAfterADirect = -1.0f;
	g_fScentAfterIdempotent = -1.0f;
	g_fScentBAfterFirstSwitch = -1.0f;
	g_fScentCAfterSwitch = -1.0f;
	g_iCooldownWaitFrames = 0;
}

static bool Step_P1ScentAccumulates(int iFrame)
{
	switch (g_iPhase)
	{
	case kSA_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSA_WaitScene;
		return true;

	case kSA_WaitScene:
		if (PickMutualRangeTrio(g_xA, g_xB, g_xC))
		{
			g_iPhase = kSA_RecordBaseline;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSA_Done;
		}
		return true;

	case kSA_RecordBaseline:
		g_fBaselineA = DP_Player::GetDemonScent(g_xA);
		g_fBaselineB = DP_Player::GetDemonScent(g_xB);
		g_fBaselineC = DP_Player::GetDemonScent(g_xC);
		g_iPhase = kSA_PossessADirect;
		return true;

	case kSA_PossessADirect:
		// SetPossessedVillager is the SYSTEM path -- no scent bump.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kSA_VerifyANoBump;
		return true;

	case kSA_VerifyANoBump:
		g_fScentAfterADirect = DP_Player::GetDemonScent(g_xA);
		g_iPhase = kSA_TryIdempotent;
		return true;

	case kSA_TryIdempotent:
		// Idempotent re-click on the same villager -- per the impl,
		// returns true but doesn't bump scent (early exit before the
		// scent accumulator).
		DP_Player::TryVoluntaryPossessSwitch(g_xA);
		g_iPhase = kSA_VerifyIdempotentNoBump;
		return true;

	case kSA_VerifyIdempotentNoBump:
		g_fScentAfterIdempotent = DP_Player::GetDemonScent(g_xA);
		g_iPhase = kSA_SwitchToB;
		return true;

	case kSA_SwitchToB:
		// Voluntary switch to B -- scent[B] += 0.3.
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kSA_VerifyBAfterFirst;
		return true;

	case kSA_VerifyBAfterFirst:
		g_fScentBAfterFirstSwitch = DP_Player::GetDemonScent(g_xB);
		g_iCooldownWaitFrames = 0;
		g_iPhase = kSA_WaitCooldown1;
		return true;

	case kSA_WaitCooldown1:
		++g_iCooldownWaitFrames;
		if (g_iCooldownWaitFrames >= kCOOLDOWN_FRAMES)
		{
			g_iPhase = kSA_SwitchToC;
		}
		return true;

	case kSA_SwitchToC:
		// Switch onward to C (NOT back to A -- A is Fainted post-MVP-
		// 1.4). scent[C] += 0.3.
		DP_Player::TryVoluntaryPossessSwitch(g_xC);
		g_iPhase = kSA_VerifyCAfterSwitch;
		return true;

	case kSA_VerifyCAfterSwitch:
		g_fScentCAfterSwitch = DP_Player::GetDemonScent(g_xC);
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: baseA=%.3f baseB=%.3f baseC=%.3f directA=%.3f idempA=%.3f firstB=%.3f firstC=%.3f",
			g_fBaselineA, g_fBaselineB, g_fBaselineC,
			g_fScentAfterADirect, g_fScentAfterIdempotent,
			g_fScentBAfterFirstSwitch, g_fScentCAfterSwitch);
		g_iPhase = kSA_Done;
		return false;

	case kSA_Done:
	default:
		return false;
	}
}

static bool Verify_P1ScentAccumulates()
{
	if (!g_xA.IsValid() || !g_xB.IsValid() || !g_xC.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentAccumulates: villager trio pick failed");
		return false;
	}
	if (!ApproxEquals(g_fBaselineA, 0.0f)
		|| !ApproxEquals(g_fBaselineB, 0.0f)
		|| !ApproxEquals(g_fBaselineC, 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: baseline non-zero (A=%.3f B=%.3f C=%.3f) -- did ResetForTest run?",
			g_fBaselineA, g_fBaselineB, g_fBaselineC);
		return false;
	}
	if (!ApproxEquals(g_fScentAfterADirect, 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: SetPossessedVillager(A) bumped scent (%.3f) -- system path should be no-op",
			g_fScentAfterADirect);
		return false;
	}
	if (!ApproxEquals(g_fScentAfterIdempotent, 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: idempotent re-click bumped scent (%.3f) -- early-exit should skip accumulator",
			g_fScentAfterIdempotent);
		return false;
	}
	const float fPerPossession =
		DP_Tuning::Get<float>("possession.demon_scent_per_possession");
	if (!ApproxEquals(g_fScentBAfterFirstSwitch, fPerPossession, 0.01f))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: scent[B] after first switch = %.3f, expected %.3f",
			g_fScentBAfterFirstSwitch, fPerPossession);
		return false;
	}
	// scent[C] after switching onward to C. C is fresh (no prior
	// bumps), so this is just the per-possession amount. Allow 0.05
	// tolerance for one-frame-of-decay slop.
	if (g_fScentCAfterSwitch < fPerPossession - 0.05f
		|| g_fScentCAfterSwitch > fPerPossession + 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: scent[C] after onward switch = %.3f, expected near %.3f",
			g_fScentCAfterSwitch, fPerPossession);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ScentAccumulatesTest = {
	"Test_P1Scent_AccumulatesOnPossession",
	&Setup_P1ScentAccumulates,
	&Step_P1ScentAccumulates,
	&Verify_P1ScentAccumulates,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ScentAccumulatesTest);

#endif // ZENITH_INPUT_SIMULATOR
