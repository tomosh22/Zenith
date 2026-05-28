#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Faint_RecoversToIdle (MVP-1.4.2 follow-up coverage gap)
//
// Test_P1Switch_FaintNotDie proves the Possessed -> Fainted transition
// fires on voluntary switch. It does NOT prove the Fainted -> Idle
// recovery (after `possession.voluntary_switch_faint_recovery_s` has
// elapsed). Without this test, a regression that leaves villagers
// permanently fainted (timer never decrements, or threshold check
// inverted) would break the GDD's "demon revisits exhausted vessels"
// mechanic and slip past CI.
//
// Procedure:
//   1. Load GameLevel; pick two villagers in mutual range.
//   2. SetPossessedVillager(A); switch to B via TryVoluntary. A goes
//      Fainted with ~10 s recovery timer.
//   3. Snapshot A's state (assert Fainted).
//   4. Shortcut the recovery timer with SetFaintRecoveryForTest(0.005)
//      -- one frame's tick of fDt (~0.01666) drains it past 0.
//   5. Wait one frame so the OnUpdate switch's `case Fainted` decrement
//      runs and transitions A to Idle.
//   6. Snapshot A's state (assert Idle).
//   7. Also assert: TryVoluntaryPossessSwitch(A) is now ALLOWED -- the
//      cooldown has expired (we waited 110+ frames implicitly via the
//      cooldown wait between SetPossessed and TryVoluntary in step 2).
//
// What this catches:
//   * The Fainted state machine never decrements the timer (timer
//     stays at 10s forever).
//   * The Fainted->Idle threshold check uses `>` instead of `<=` (the
//     transition fires only when fully past 0).
//   * IsPossessable() still returns false for Idle villagers (the
//     state-machine transition fired but the gate didn't update).
// ============================================================================

namespace
{
	enum Phase : int { kFR_Start, kFR_WaitScene, kFR_PossessA, kFR_WaitForA,
	                   kFR_SwitchToB, kFR_WaitForFaint, kFR_SnapshotFainted,
	                   kFR_ShortcutTimer, kFR_WaitForRecovery,
	                   kFR_SnapshotIdle, kFR_TryRepossess, kFR_Verify, kFR_Done };

	int                     g_iPhase = kFR_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	DPVillagerState         g_eAFaintedSnapshot = DPVillagerState::Idle;
	DPVillagerState         g_eAIdleSnapshot = DPVillagerState::Fainted; // sentinel: must become Idle
	bool                    g_bRepossessOk = false;
	int                     g_iCooldownWait = 0;

	// 110 frames * 0.01666s = ~1.83s, just past the 1.5s
	// voluntary-switch cooldown. We need cooldown clear for the
	// final repossess-A attempt to pass on a state-only-refusal
	// rather than a cooldown-refusal.
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
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVs](Zenith_EntityID xId, DPVillager_Behaviour&)
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

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P1FaintRecovers()
{
	g_iPhase = kFR_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_eAFaintedSnapshot = DPVillagerState::Idle;
	g_eAIdleSnapshot = DPVillagerState::Fainted;
	g_bRepossessOk = false;
	g_iCooldownWait = 0;
}

static bool Step_P1FaintRecovers(int iFrame)
{
	switch (g_iPhase)
	{
	case kFR_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFR_WaitScene;
		return true;

	case kFR_WaitScene:
		PickClosestPair(g_xA, g_xB);
		if (g_xA.IsValid() && g_xB.IsValid())
		{
			g_iPhase = kFR_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kFR_Done;
		}
		return true;

	case kFR_PossessA:
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kFR_WaitForA;
		return true;

	case kFR_WaitForA:
		// One frame for A's OnUpdate to run Idle -> Possessed.
		g_iPhase = kFR_SwitchToB;
		return true;

	case kFR_SwitchToB:
		// Player-driven voluntary switch. A becomes Fainted.
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kFR_WaitForFaint;
		return true;

	case kFR_WaitForFaint:
		// One frame for A's OnUpdate to run Possessed -> Fainted.
		g_iPhase = kFR_SnapshotFainted;
		return true;

	case kFR_SnapshotFainted:
	{
		DPVillager_Behaviour* pxA = GetVillagerBehaviour(g_xA);
		if (pxA != nullptr) g_eAFaintedSnapshot = pxA->GetState();
		g_iPhase = kFR_ShortcutTimer;
		return true;
	}

	case kFR_ShortcutTimer:
	{
		// Shortcut: set the recovery timer near 0 so the next frame's
		// OnUpdate (which decrements fDt of game time) crosses zero.
		// This avoids ticking 600 frames of real time -- the test
		// pins the STATE TRANSITION, not the timer duration (which
		// the tuning test pins separately).
		DPVillager_Behaviour* pxA = GetVillagerBehaviour(g_xA);
		if (pxA != nullptr) pxA->SetFaintRecoveryForTest(0.005f);
		// Also wait out the cooldown from the original switch (we
		// want the final TryVoluntary to be refused by state, not
		// cooldown, to prove the assertion has teeth).
		g_iCooldownWait = 0;
		g_iPhase = kFR_WaitForRecovery;
		return true;
	}

	case kFR_WaitForRecovery:
		// Tick frames until cooldown is cleared AND recovery fired.
		// Cooldown clears at ~110 frames; recovery fires on the very
		// next OnUpdate after kFR_ShortcutTimer, well before then.
		++g_iCooldownWait;
		if (g_iCooldownWait >= kCOOLDOWN_FRAMES)
		{
			g_iPhase = kFR_SnapshotIdle;
		}
		return true;

	case kFR_SnapshotIdle:
	{
		DPVillager_Behaviour* pxA = GetVillagerBehaviour(g_xA);
		if (pxA != nullptr) g_eAIdleSnapshot = pxA->GetState();
		g_iPhase = kFR_TryRepossess;
		return true;
	}

	case kFR_TryRepossess:
		// Cooldown is clear (waited > 1.5s). A's state is hopefully
		// Idle. Voluntary switch should succeed if both gates pass.
		g_bRepossessOk = DP_Player::TryVoluntaryPossessSwitch(g_xA);
		g_iPhase = kFR_Verify;
		return true;

	case kFR_Verify:
		std::printf("[P1FaintRecovers] eAFainted=%d eAIdle=%d repossessOk=%d\n",
			(int)g_eAFaintedSnapshot, (int)g_eAIdleSnapshot,
			(int)g_bRepossessOk);
		std::fflush(stdout);
		g_iPhase = kFR_Done;
		return false;

	case kFR_Done:
	default:
		return false;
	}
}

static bool Verify_P1FaintRecovers()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1FaintRecovers: villager pair not picked");
		return false;
	}
	if (g_eAFaintedSnapshot != DPVillagerState::Fainted)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintRecovers: A's state after voluntary switch is %d, expected Fainted (precondition for the recovery test)",
			(int)g_eAFaintedSnapshot);
		return false;
	}
	if (g_eAIdleSnapshot != DPVillagerState::Idle)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintRecovers: A's state after timer shortcut + tick is %d, expected Idle. The Fainted->Idle transition didn't fire -- check the OnUpdate state machine's timer decrement and threshold (must be <= 0, not < 0)",
			(int)g_eAIdleSnapshot);
		return false;
	}
	if (!g_bRepossessOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintRecovers: TryVoluntaryPossessSwitch(A) refused after recovery -- IsPossessable() still returns false for an Idle villager, or the state didn't actually flip");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1FaintRecoversTest = {
	"Test_P1Faint_RecoversToIdle",
	&Setup_P1FaintRecovers,
	&Step_P1FaintRecovers,
	&Verify_P1FaintRecovers,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1FaintRecoversTest);

#endif // ZENITH_INPUT_SIMULATOR
