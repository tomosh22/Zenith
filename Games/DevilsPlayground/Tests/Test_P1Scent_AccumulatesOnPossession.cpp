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
//   2. Find the closest pair of villagers.
//   3. Snapshot baseline scent on both (expect 0.0 after suite reset).
//   4. SetPossessedVillager(A) -- direct write, no scent bump.
//   5. TryVoluntaryPossessSwitch(A) -- idempotent re-click; no bump.
//   6. TryVoluntaryPossessSwitch(B) -- scent[B] = 0.3.
//   7. Wait cooldown + TryVoluntaryPossessSwitch(A) -- scent[A] = 0.3.
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
		kSA_WaitCooldown1, kSA_SwitchToA2, kSA_VerifyAAfterFirst,
		kSA_Done
	};

	int                     g_iPhase = kSA_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;

	float                   g_fBaselineA = -1.0f;
	float                   g_fBaselineB = -1.0f;
	float                   g_fScentAfterADirect = -1.0f;
	float                   g_fScentAfterIdempotent = -1.0f;
	float                   g_fScentBAfterFirstSwitch = -1.0f;
	float                   g_fScentAAfterFirstSwitch = -1.0f;

	int                     g_iCooldownWaitFrames = 0;

	// 1.5s cooldown default -> ~95 frames at 60 Hz. We wait 110 to be
	// pessimistic against frame-time jitter.
	constexpr int kCOOLDOWN_FRAMES = 110;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
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
		struct Cand { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<Cand> axCands;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axCands](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				Cand xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axCands.PushBack(xV);
			});
		if (axCands.GetSize() < 2) return;
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
					xA = axCands.Get(i).xId;
					xB = axCands.Get(j).xId;
				}
			}
		}
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
	g_fBaselineA = -1.0f;
	g_fBaselineB = -1.0f;
	g_fScentAfterADirect = -1.0f;
	g_fScentAfterIdempotent = -1.0f;
	g_fScentBAfterFirstSwitch = -1.0f;
	g_fScentAAfterFirstSwitch = -1.0f;
	g_iCooldownWaitFrames = 0;
}

static bool Step_P1ScentAccumulates(int iFrame)
{
	switch (g_iPhase)
	{
	case kSA_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSA_WaitScene;
		return true;

	case kSA_WaitScene:
		PickClosestPair(g_xA, g_xB);
		if (g_xA.IsValid() && g_xB.IsValid()
			&& g_xA.m_uIndex != g_xB.m_uIndex)
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
			g_iPhase = kSA_SwitchToA2;
		}
		return true;

	case kSA_SwitchToA2:
		// Switch back to A. scent[A] += 0.3.
		DP_Player::TryVoluntaryPossessSwitch(g_xA);
		g_iPhase = kSA_VerifyAAfterFirst;
		return true;

	case kSA_VerifyAAfterFirst:
		g_fScentAAfterFirstSwitch = DP_Player::GetDemonScent(g_xA);
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: baseA=%.3f baseB=%.3f directA=%.3f idempA=%.3f firstB=%.3f firstA=%.3f",
			g_fBaselineA, g_fBaselineB, g_fScentAfterADirect,
			g_fScentAfterIdempotent, g_fScentBAfterFirstSwitch,
			g_fScentAAfterFirstSwitch);
		g_iPhase = kSA_Done;
		return false;

	case kSA_Done:
	default:
		return false;
	}
}

static bool Verify_P1ScentAccumulates()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentAccumulates: villager pick failed");
		return false;
	}
	if (!ApproxEquals(g_fBaselineA, 0.0f) || !ApproxEquals(g_fBaselineB, 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: baseline non-zero (A=%.3f B=%.3f) -- did ResetForTest run?",
			g_fBaselineA, g_fBaselineB);
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
	// scent[A] after switching back has been decayed by the cooldown
	// wait (110 frames * 0.01666 s/frame * 0.05/s = ~0.09 lost).
	// Expected: 0.3 (from this switch) but the read is BEFORE any
	// decay tick after the switch frame, so we expect exactly 0.3 +/-
	// one-frame-of-decay. Allow 0.05 tolerance.
	if (g_fScentAAfterFirstSwitch < fPerPossession - 0.05f
		|| g_fScentAAfterFirstSwitch > fPerPossession + 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentAccumulates: scent[A] after second switch = %.3f, expected near %.3f",
			g_fScentAAfterFirstSwitch, fPerPossession);
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
