#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

#include "DP_Knots.h"
#include "DP_MetaSave.h"
#include "DP_Tuning.h"
#include "DPCommonTypes.h"

#include "ZenithECS/Zenith_EventSystem.h"

namespace DP_Knots
{
	namespace
	{
		// Per-run state. Reset by DPProcLevelBootstrap::OnAwake (run start)
		// and by the between-tests hook (batched tests).
		uint32_t g_uRunKnotTally      = 0;
		uint32_t g_uHandoffChain     = 0;
		bool     g_bBankedThisRun    = false;

		// Subscription handles so Initialise can RE-subscribe. A simple
		// "initialised once" latch is NOT enough: the boot-time engine unit
		// tests (which run in every launch without --skip-unit-tests) call
		// Zenith_EventDispatcher::ClearAllSubscriptions, silently killing
		// anything subscribed during Project_RegisterGameComponents. So
		// Initialise is called again from DPProcLevelBootstrap::OnAwake —
		// after the unit-test phase by construction, at every run start —
		// and tears down its own (possibly already-wiped) handles first.
		Zenith_EventHandle g_xDiedSub    = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xVictorySub = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xRunLostSub = INVALID_EVENT_HANDLE;
	}

	void Initialise()
	{
		// Captureless lambdas — no component-relocation hazard. Re-entrant:
		// drop our previous handles (no-op if the dispatcher already wiped
		// them) and stand up fresh subscriptions.
		auto& xDisp = Zenith_EventDispatcher::Get();
		if (g_xDiedSub    != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(g_xDiedSub);
		if (g_xVictorySub != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(g_xVictorySub);
		if (g_xRunLostSub != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(g_xRunLostSub);

		// Any villager death breaks the hand-off chain (the chain is only
		// alive while every prior vessel survived).
		g_xDiedSub = xDisp.Subscribe<DP_OnVillagerDied>(
			[](const DP_OnVillagerDied&)
			{
				g_uHandoffChain = 0;
			});

		// Run end — win OR loss — banks the tally exactly once. Reagents
		// already inscribed count even when the run is subsequently lost.
		g_xVictorySub = xDisp.Subscribe<DP_OnVictory>(
			[](const DP_OnVictory&)
			{
				BankRunTallyIntoMetaSave();
			});
		g_xRunLostSub = xDisp.Subscribe<DP_OnRunLost>(
			[](const DP_OnRunLost&)
			{
				BankRunTallyIntoMetaSave();
			});
	}

	void NotifyReagentInscribed()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Knots::NotifyReagentInscribed must be called from main thread");
		uint32_t uEarned = static_cast<uint32_t>(
			DP_Tuning::Get<int>("metagame.knots_per_reagent"));
		const uint32_t uChainMin = static_cast<uint32_t>(
			DP_Tuning::Get<int>("metagame.handoff_chain_min"));
		if (g_uHandoffChain >= uChainMin)
		{
			uEarned += static_cast<uint32_t>(
				DP_Tuning::Get<int>("metagame.handoff_chain_bonus"));
		}
		g_uRunKnotTally += uEarned;
	}

	void NotifyVoluntaryHandoff()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Knots::NotifyVoluntaryHandoff must be called from main thread");
		++g_uHandoffChain;
	}

	void NotifyHoundNeutralised()
	{
		// Deliberate stub: hound gameplay does not exist (GDD escalation
		// modifiers are post-MVP; Tuning.json's modifier_hound_release_s is
		// -1 = never). The hook keeps the earning schema stable for when
		// hounds land — it contributes 0 Knots today.
	}

	uint32_t GetRunKnotTally()       { return g_uRunKnotTally; }
	uint32_t GetHandoffChainLength() { return g_uHandoffChain; }
	bool HasBankedThisRun()          { return g_bBankedThisRun; }

	uint32_t BankRunTallyIntoMetaSave()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Knots::BankRunTallyIntoMetaSave must be called from main thread");
		if (g_bBankedThisRun) return 0;
		g_bBankedThisRun = true;

		const uint32_t uBanked = g_uRunKnotTally;
		DP_MetaState xState = DP_MetaSave::Cached();
		xState.m_uKnotBalance += uBanked;
		xState.m_uEarnedUnspentKnotsLastRun = uBanked;
		DP_MetaSave::SaveToDisk(xState);
		return uBanked;
	}

	void ResetForNewRun()
	{
		g_uRunKnotTally   = 0;
		g_uHandoffChain   = 0;
		g_bBankedThisRun  = false;
	}
}
