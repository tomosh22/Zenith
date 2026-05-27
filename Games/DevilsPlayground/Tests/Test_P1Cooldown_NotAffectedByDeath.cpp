#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

// ============================================================================
// Test_P1Cooldown_NotAffectedByDeath (MVP-1.5.3)
//
// Regression guard: when a possessed villager dies (life timer expires
// -> DPVillager_Behaviour::TickLife dispatches DP_OnVillagerDied and
// calls DP_Player::SetPossessedVillager(INVALID_ENTITY_ID)), NO
// possession cooldown should accrue. The player must be able to
// immediately TryVoluntaryPossessSwitch onto another villager.
//
// Tuning canon: "possession.cooldown_after_burnout_s = 0.0" --
// burnout (death-by-life-timer) imposes no cooldown by design.
//
// Procedure:
//   1. Load GameLevel.
//   2. Find two villagers (A and B; B != A).
//   3. SetPossessedVillager(A). Wait one frame so DPVillager A's
//      OnUpdate flips its m_bIsPossessed flag + bumps remaining life.
//   4. SetRemainingLifeForTest(0.05f) -- shrink A's life so the next
//      TickLife frame burns it down to zero.
//   5. Tick ~10 frames. Within that window:
//      - DPVillager_A::TickLife sees remainingLife <= 0
//      - Dispatches DP_OnVillagerDied
//      - Calls DP_Player::SetPossessedVillager(INVALID_ENTITY_ID)
//      - This is the SYSTEM path -- NO cooldown set.
//   6. Snapshot DP_Player::GetPossessionCooldownRemaining() (expect 0).
//   7. Snapshot DP_Player::GetPossessedVillager() (expect INVALID).
//   8. TryVoluntaryPossessSwitch(B). Expect success.
//
// What this proves:
//   * Death path does not call any cooldown-setting code.
//   * The cooldown timer state is clean after a death.
//   * Player can immediately repossess after losing a vessel to
//     burnout, without waiting out a switch-cooldown.
// ============================================================================

namespace
{
	enum Phase : int { kND_Start, kND_WaitScene, kND_Possess, kND_AfterBump,
	                   kND_ShrinkLife, kND_TickForDeath,
	                   kND_VerifyDeath, kND_TrySwitchB, kND_Done };

	int                     g_iPhase = kND_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EventHandle      g_xDeathHandle = INVALID_EVENT_HANDLE;
	bool                    g_bDeathFired = false;
	float                   g_fCooldownAfterDeath = -1.0f;
	Zenith_EntityID         g_xPossessionAfterDeath;
	bool                    g_bSwitchToBOk = false;
	Zenith_EntityID         g_xPossessionAfterSwitch;
	int                     g_iTickCount = 0;

	constexpr int kFRAMES_FOR_DEATH = 30; // ~0.5s -- well past the 0.05s life budget
}

static void Setup_P1CooldownDeath()
{
	g_iPhase = kND_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_bDeathFired = false;
	g_fCooldownAfterDeath = -1.0f;
	g_xPossessionAfterDeath = INVALID_ENTITY_ID;
	g_bSwitchToBOk = false;
	g_xPossessionAfterSwitch = INVALID_ENTITY_ID;
	g_iTickCount = 0;
	g_xDeathHandle = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnVillagerDied>(
		[](const DP_OnVillagerDied&) { g_bDeathFired = true; });
}

static bool Step_P1CooldownDeath(int iFrame)
{
	switch (g_iPhase)
	{
	case kND_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kND_WaitScene;
		return true;

	case kND_WaitScene:
	{
		// Pick first two distinct villagers iterated. Order matches
		// DP_LevelData::kVillager insertion order, so this is reproducible.
		Zenith_EntityID xFirst, xSecond;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFirst, &xSecond]
			(Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFirst.IsValid()) { xFirst = xId; return; }
				if (!xSecond.IsValid() && xId.m_uIndex != xFirst.m_uIndex)
				{
					xSecond = xId;
				}
			});
		if (xFirst.IsValid() && xSecond.IsValid())
		{
			g_xA = xFirst;
			g_xB = xSecond;
			g_iPhase = kND_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kND_Done;
		}
		return true;
	}

	case kND_Possess:
		// Direct write -- system path, no cooldown plumbing. This is
		// also what DPPlayerController would have done historically
		// before MVP-1.5; the test models the baseline state of "the
		// player is currently possessing A" without going through
		// TryVoluntaryPossessSwitch (which would itself arm a
		// cooldown and confound the test).
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kND_AfterBump;
		return true;

	case kND_AfterBump:
	{
		// One frame later: DPVillager_A's OnUpdate has flipped its
		// m_bIsPossessed flag and bumped m_fRemainingLife back to
		// m_fMaxLife (the "freshly-possessed transition" handler). Now
		// the test can shrink life without it being clobbered.
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(g_xA);
		if (pxScene == nullptr) { g_iPhase = kND_Done; return false; }
		Zenith_Entity xEnt = pxScene->TryGetEntity(g_xA);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ScriptComponent>()) { g_iPhase = kND_Done; return false; }
		DPVillager_Behaviour* pxV = xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
		if (pxV == nullptr) { g_iPhase = kND_Done; return false; }
		pxV->SetRemainingLifeForTest(0.05f);
		g_iPhase = kND_ShrinkLife;
		return true;
	}

	case kND_ShrinkLife:
		// One more frame to ensure TickLife sees the shrunk value next
		// tick. Splitting kND_AfterBump from kND_ShrinkLife from
		// kND_TickForDeath makes the test's timing pessimistic on
		// purpose: each phase consumes exactly one Step() invocation,
		// and the assertions don't depend on whether death fires on
		// frame N or N+1.
		g_iPhase = kND_TickForDeath;
		return true;

	case kND_TickForDeath:
		++g_iTickCount;
		if (g_bDeathFired || g_iTickCount >= kFRAMES_FOR_DEATH)
		{
			g_iPhase = kND_VerifyDeath;
		}
		return true;

	case kND_VerifyDeath:
		g_fCooldownAfterDeath = DP_Player::GetPossessionCooldownRemaining();
		g_xPossessionAfterDeath = DP_Player::GetPossessedVillager();
		g_iPhase = kND_TrySwitchB;
		return true;

	case kND_TrySwitchB:
		// The crucial assertion: TryVoluntaryPossessSwitch onto B must
		// succeed IMMEDIATELY after the death, with no cooldown wait.
		g_bSwitchToBOk = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_xPossessionAfterSwitch = DP_Player::GetPossessedVillager();
		Zenith_Log(LOG_CATEGORY_AI,
			"P1CooldownNoDeath: death=%d cooldownAfter=%.3f possAfter=(%u/%u) switchOk=%d possessionAfterSwitch=(%u/%u)",
			(int)g_bDeathFired, g_fCooldownAfterDeath,
			g_xPossessionAfterDeath.m_uIndex, g_xPossessionAfterDeath.m_uGeneration,
			(int)g_bSwitchToBOk,
			g_xPossessionAfterSwitch.m_uIndex, g_xPossessionAfterSwitch.m_uGeneration);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xDeathHandle);
		g_iPhase = kND_Done;
		return false;

	case kND_Done:
	default:
		return false;
	}
}

static bool Verify_P1CooldownDeath()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1CooldownNoDeath: failed to pick two villagers");
		return false;
	}
	if (!g_bDeathFired)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1CooldownNoDeath: DP_OnVillagerDied never fired -- TickLife didn't run");
		return false;
	}
	if (g_xPossessionAfterDeath.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1CooldownNoDeath: possession not cleared by death (got %u/%u)",
			g_xPossessionAfterDeath.m_uIndex, g_xPossessionAfterDeath.m_uGeneration);
		return false;
	}
	if (g_fCooldownAfterDeath > 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1CooldownNoDeath: cooldown=%.3f after death -- death path is leaking into cooldown timer",
			g_fCooldownAfterDeath);
		return false;
	}
	if (!g_bSwitchToBOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1CooldownNoDeath: TryVoluntaryPossessSwitch(B) refused immediately after death");
		return false;
	}
	if (g_xPossessionAfterSwitch.m_uIndex != g_xB.m_uIndex
		|| g_xPossessionAfterSwitch.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1CooldownNoDeath: possession not switched to B");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1CooldownDeathTest = {
	"Test_P1Cooldown_NotAffectedByDeath",
	&Setup_P1CooldownDeath,
	&Step_P1CooldownDeath,
	&Verify_P1CooldownDeath,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1CooldownDeathTest);

#endif // ZENITH_INPUT_SIMULATOR
