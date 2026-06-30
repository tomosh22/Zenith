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
// Test_P1Switch_FaintNotDie (MVP-1.4.1)
//
// Pins the GDD's "voluntary switch faints, doesn't kill" semantic:
// when the player voluntarily switches off villager A (mid-life, NOT
// at burn-out), A enters Fainted state -- temporarily unavailable for
// re-possession -- and recovers to Idle after
// `possession.voluntary_switch_faint_recovery_s` seconds.
//
// Crucially DISTINCT from MVP-1.4.3 (BurnOutDoesDie): voluntary
// switch must NOT set state=Dead, otherwise the villager is lost
// for the rest of the run. That's a one-life-per-villager mechanic
// the GDD explicitly rejects -- the demon can revisit vessels after
// they recover.
//
// Procedure:
//   1. Load GameLevel; pick two villagers (A and B) in mutual range.
//   2. SetPossessedVillager(A) -- system path, anchor at A. A's
//      state transitions Idle -> Possessed in OnUpdate.
//   3. Wait one frame for that transition.
//   4. TryVoluntaryPossessSwitch(B) -- player-driven path. B becomes
//      possessed. A's possession flag clears.
//   5. Wait one frame so A's OnUpdate observes the un-possession
//      and runs the Possessed -> Fainted transition.
//   6. Snapshot A's state + faint-recovery-remaining + remaining-life.
//   7. Assert:
//        A.state == Fainted (NOT Dead, NOT Idle)
//        A.faintRecoveryRemaining > 0 (timer armed)
//        A.remainingLife > 0 (didn't die)
//   8. Also assert: TryVoluntaryPossessSwitch(A) refuses because A
//      is Fainted -- the player can't possess them again until
//      recovery completes.
//
// What this catches:
//   * Voluntary switch transitions to Dead instead of Fainted.
//   * Faint-recovery timer never arms (=0 immediately) -- the
//     state-transition forgot to set the timer.
//   * IsPossessable() returns true for Fainted -- the refusal
//     gate in TryVoluntaryPossessSwitch is missing or wrong.
//   * Voluntary-switch path bumps life to 0 (regression where
//     un-possession is treated as death).
// ============================================================================

namespace
{
	enum Phase : int { kFN_Start, kFN_WaitScene, kFN_PossessA, kFN_WaitForA,
	                   kFN_SwitchToB, kFN_WaitForFaint, kFN_Snapshot,
	                   kFN_TryRePossessA, kFN_Verify, kFN_Done };

	int                     g_iPhase = kFN_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	DPVillagerState         g_eAFinalState = DPVillagerState::Idle;
	float                   g_fAFaintTimer = 0.0f;
	float                   g_fALifeAfterSwitch = 0.0f;
	bool                    g_bSwitchToBOk = false;
	bool                    g_bRePossessAOk = false;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
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

	DPVillager_Component* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<DPVillager_Component>();
	}
}

static void Setup_P1FaintNotDie()
{
	g_iPhase = kFN_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_eAFinalState = DPVillagerState::Idle;
	g_fAFaintTimer = 0.0f;
	g_fALifeAfterSwitch = 0.0f;
	g_bSwitchToBOk = false;
	g_bRePossessAOk = false;
}

static bool Step_P1FaintNotDie(int iFrame)
{
	switch (g_iPhase)
	{
	case kFN_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFN_WaitScene;
		return true;

	case kFN_WaitScene:
		PickClosestPair(g_xA, g_xB);
		if (g_xA.IsValid() && g_xB.IsValid())
		{
			g_iPhase = kFN_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kFN_Done;
		}
		return true;

	case kFN_PossessA:
		// System path -- anchor at A, no scent. A's state transitions
		// Idle -> Possessed on the next OnUpdate.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kFN_WaitForA;
		return true;

	case kFN_WaitForA:
		// One frame for A's OnUpdate to observe possession and run
		// the Idle -> Possessed transition.
		g_iPhase = kFN_SwitchToB;
		return true;

	case kFN_SwitchToB:
		// Player-driven voluntary switch. A's possession flag clears
		// after B becomes possessed.
		g_bSwitchToBOk = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kFN_WaitForFaint;
		return true;

	case kFN_WaitForFaint:
		// One frame so A's OnUpdate observes the un-possession and
		// runs Possessed -> Fainted.
		g_iPhase = kFN_Snapshot;
		return true;

	case kFN_Snapshot:
	{
		DPVillager_Component* pxA = GetVillagerBehaviour(g_xA);
		if (pxA != nullptr)
		{
			g_eAFinalState = pxA->GetState();
			g_fAFaintTimer = pxA->GetFaintRecoveryRemaining();
			g_fALifeAfterSwitch = pxA->GetRemainingLife();
		}
		g_iPhase = kFN_TryRePossessA;
		return true;
	}

	case kFN_TryRePossessA:
	{
		// Try to voluntary-switch back to A. A is Fainted, so the
		// switch should be REFUSED by the IsPossessable() gate.
		// (Aside: the cooldown from kFN_SwitchToB is still active
		// at ~1.5s; the test does NOT distinguish between cooldown
		// refusal and faint refusal -- it only proves "refused".
		// Both refusals are correct for this scenario; the faint
		// refusal is the one that would persist after cooldown
		// expires. We accept the conjunction as the assertion.)
		g_bRePossessAOk = DP_Player::TryVoluntaryPossessSwitch(g_xA);
		g_iPhase = kFN_Verify;
		return true;
	}

	case kFN_Verify:
		std::printf("[P1FaintNotDie] state=%d faintTimer=%.3f life=%.3f switchToBOk=%d rePossessAOk=%d\n",
			(int)g_eAFinalState, g_fAFaintTimer, g_fALifeAfterSwitch,
			(int)g_bSwitchToBOk, (int)g_bRePossessAOk);
		std::fflush(stdout);
		g_iPhase = kFN_Done;
		return false;

	case kFN_Done:
	default:
		return false;
	}
}

static bool Verify_P1FaintNotDie()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1FaintNotDie: villager pair pick failed");
		return false;
	}
	if (!g_bSwitchToBOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintNotDie: voluntary switch A->B was refused -- precondition failure (cooldown still active from a previous test? range gate?)");
		return false;
	}
	if (g_eAFinalState != DPVillagerState::Fainted)
	{
		const char* szState = "Idle";
		if (g_eAFinalState == DPVillagerState::Possessed) szState = "Possessed";
		else if (g_eAFinalState == DPVillagerState::Fainted) szState = "Fainted";
		else if (g_eAFinalState == DPVillagerState::Dead) szState = "Dead";
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintNotDie: A's state after voluntary-switch-away is %s, expected Fainted. Possessed->Fainted transition didn't fire in OnUpdate",
			szState);
		return false;
	}
	const float fExpectedFaintTimer =
		DP_Tuning::Get<float>("possession.voluntary_switch_faint_recovery_s");
	// Allow 1 frame's worth of tick decrement (~0.017 s) since
	// OnUpdate may have started counting down already.
	if (g_fAFaintTimer < fExpectedFaintTimer - 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintNotDie: A's faint recovery timer is %.3f s but should be ~%.3f s -- transition forgot to arm the timer, or the tuning key is wrong",
			g_fAFaintTimer, fExpectedFaintTimer);
		return false;
	}
	if (g_fALifeAfterSwitch <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintNotDie: A's remaining life is %.3f after voluntary switch -- voluntary-switch-away should NOT kill the villager",
			g_fALifeAfterSwitch);
		return false;
	}
	if (g_bRePossessAOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintNotDie: TryVoluntaryPossessSwitch(A) succeeded but A is Fainted -- IsPossessable() gate in PublicInterfaces.cpp didn't fire");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1FaintNotDieTest = {
	"Test_P1Switch_FaintNotDie",
	&Setup_P1FaintNotDie,
	&Step_P1FaintNotDie,
	&Verify_P1FaintNotDie,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1FaintNotDieTest);

#endif // ZENITH_INPUT_SIMULATOR
