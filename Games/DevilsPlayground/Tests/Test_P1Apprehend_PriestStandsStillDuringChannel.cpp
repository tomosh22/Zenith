#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Apprehend_PriestStandsStillDuringChannel (MVP-1.3.2 coverage gap)
//
// Test_P1Apprehend_PriestCatchesPlayer pins the END condition (event
// fires after channel completes). It says nothing about what happens
// DURING the channel. The GDD framing of Apprehend is "the priest
// plants and channels in place" -- a regression where the priest
// drifts toward the villager via leftover Pursue velocity, or gets
// nudged by physics, or rubber-bands due to a path request that
// wasn't fully cancelled, would all pass the existing test as long
// as the event eventually fires.
//
// This test pins the stillness invariant. Sequence:
//   1. Load GameLevel, possess closest villager, teleport priest to
//      ~1.5 m from the villager. Initially Pursue may run for a
//      handful of frames (the priest's collider gets nudged out of
//      apprehend range by the villager's collider on the first
//      physics step) before Apprehend takes over.
//   2. Wait kSETTLE_FRAMES (60 frames = ~1 s) for the Pursue->
//      Apprehend transition to settle and Apprehend.OnEnter's
//      NavMeshAgent::Stop() to take effect.
//   3. Sample priest position (xPosChannelMid).
//   4. Continue ticking until DP_OnRunLost{Apprehended} dispatches.
//   5. Sample priest position again (xPosChannelEnd).
//   6. Assert:
//      a) Channel did dispatch (otherwise the stillness check is
//         meaningless -- a no-op priest doesn't drift).
//      b) Drift between xPosChannelMid and xPosChannelEnd is below
//         kMAX_HORIZ_DRIFT_M. Apprehend.OnEnter has run by mid-
//         sample so any further drift is a real regression in
//         (i) Stop() not taking effect, (ii) ApplyVelocityToBody
//         not zeroing velocity, or (iii) gravity coming back on.
//
// The kSETTLE_FRAMES wait deliberately skips the Pursue-before-
// Apprehend window -- that drift is real but is governed by the
// physics-nudge + Pursue path-following, not by the Apprehend node.
// The test we want to pin is "once Apprehend takes over, the priest
// stays put."
//
// What this catches:
//   * Apprehend.OnEnter forgot to call nav.Stop() -- the leftover
//     Pursue path would keep the navmesh agent moving toward the
//     villager throughout the 3s channel.
//   * ApplyVelocityToBodyIfPresent regressed (e.g., started pushing
//     the BT-requested velocity into Jolt instead of zeroing).
//   * Physics gravity was re-enabled on the priest's body (the
//     priest would integrate downward; we check XZ drift but also
//     log the Y delta for diagnostics).
//   * A new Selector child between Apprehend and Patrol is competing
//     with Apprehend (e.g., a "step toward target" action that
//     wasn't supposed to run during channel).
// ============================================================================

namespace
{
	enum Phase : int {
		kSS_Start, kSS_WaitScene, kSS_Possess, kSS_TeleportPriest,
		kSS_Settle, kSS_SnapshotMid, kSS_TickToEnd, kSS_SnapshotEnd,
		kSS_Verify, kSS_Done
	};

	int                     g_iPhase = kSS_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	bool                    g_bRunLostFired = false;
	int                     g_iTickFramesSinceTeleport = 0;
	int                     g_iTickFramesSinceMid = 0;
	Zenith_Maths::Vector3   g_xPriestPosAtMid(0.0f);
	Zenith_Maths::Vector3   g_xPriestPosAtEnd(0.0f);
	float                   g_fHorizDriftMidToEnd = -1.0f;
	float                   g_fYDriftMidToEnd     = 0.0f;

	// Wait 60 frames (~1 s) after the teleport for Pursue to push the
	// priest back into apprehend range and for Apprehend.OnEnter to
	// fire its NavMeshAgent::Stop(). After this point the priest
	// should be planted; subsequent drift is real.
	constexpr int kSETTLE_FRAMES = 60;
	// Allow up to ~5 s for the channel to complete after the mid sample
	// (channel default is 3 s; mid sample lands somewhere inside it).
	constexpr int kMAX_CHANNEL_WAIT_FRAMES = 300;
	// 0.1 m drift tolerance for the post-settle window. The priest's
	// body is DYNAMIC but constrained (gravity off, rotations locked,
	// linearVelocity zeroed each frame by ApplyVelocityToBodyIfPresent);
	// 0.1 m allows for sub-physics-step jitter while flagging any
	// real movement.
	constexpr float kMAX_HORIZ_DRIFT_M = 0.1f;

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		if (xEvt.m_eCause == DP_RunLostCause::Apprehended)
		{
			g_bRunLostFired = true;
		}
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

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

	bool TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		return true;
	}
}

static void Setup_P1ApprehendStill()
{
	g_iPhase = kSS_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_bRunLostFired = false;
	g_iTickFramesSinceTeleport = 0;
	g_iTickFramesSinceMid = 0;
	g_xPriestPosAtMid = Zenith_Maths::Vector3(0.0f);
	g_xPriestPosAtEnd = Zenith_Maths::Vector3(0.0f);
	g_fHorizDriftMidToEnd = -1.0f;
	g_fYDriftMidToEnd = 0.0f;
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P1ApprehendStill(int iFrame)
{
	switch (g_iPhase)
	{
	case kSS_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSS_WaitScene;
		return true;

	case kSS_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xFoundVillager;
		float fClosestDist = 1e30f;

		Zenith_Maths::Vector3 xPriestPos(0.0f);
		bool bGotPriestPos = false;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest, &xPriestPos, &bGotPriestPos]
			(Zenith_EntityID xId, Priest_Behaviour&)
			{
				xFoundPriest = xId;
				bGotPriestPos = TryGetEntityPos(xId, xPriestPos);
			});

		if (xFoundPriest.IsValid() && bGotPriestPos)
		{
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&xFoundVillager, &fClosestDist, &xPriestPos]
				(Zenith_EntityID xId, DPVillager_Behaviour&)
				{
					Zenith_Maths::Vector3 xVPos;
					if (!TryGetEntityPos(xId, xVPos)) return;
					const float fD = HorizontalDistance(xPriestPos, xVPos);
					if (fD < fClosestDist)
					{
						fClosestDist = fD;
						xFoundVillager = xId;
					}
				});
		}

		if (xFoundPriest.IsValid() && xFoundVillager.IsValid())
		{
			g_xPriest   = xFoundPriest;
			g_xVillager = xFoundVillager;
			g_iPhase    = kSS_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSS_Done;
		}
		return true;
	}

	case kSS_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kSS_TeleportPriest;
		return true;

	case kSS_TeleportPriest:
	{
		Zenith_Maths::Vector3 xVillagerPos;
		if (!TryGetEntityPos(g_xVillager, xVillagerPos)) { g_iPhase = kSS_Done; return false; }
		// 1.5 m east of the villager: inside the 2 m apprehend range
		// default. Same setup as Test_P1Apprehend_PriestCatchesPlayer.
		const Zenith_Maths::Vector3 xPriestPos(
			xVillagerPos.x + 1.5f, xVillagerPos.y, xVillagerPos.z);
		TrySetEntityPos(g_xPriest, xPriestPos);
		g_iTickFramesSinceTeleport = 0;
		g_iPhase = kSS_Settle;
		return true;
	}

	case kSS_Settle:
		// Let the BT settle into Apprehend. On the first physics step
		// after teleport, the priest's body may get nudged outside
		// the apprehend range by the villager's collider; Pursue then
		// runs briefly, requests a path back into range, and the
		// next Selector tick lands on Apprehend.
		++g_iTickFramesSinceTeleport;
		if (g_iTickFramesSinceTeleport >= kSETTLE_FRAMES)
		{
			g_iPhase = kSS_SnapshotMid;
		}
		// Defensive: if the channel completes during settle (it shouldn't
		// at 1 s -- channel default is 3 s), still bail to verify so
		// we don't miss the dispatch entirely.
		if (g_bRunLostFired)
		{
			g_iPhase = kSS_SnapshotMid;
		}
		return true;

	case kSS_SnapshotMid:
		TryGetEntityPos(g_xPriest, g_xPriestPosAtMid);
		g_iTickFramesSinceMid = 0;
		g_iPhase = kSS_TickToEnd;
		return true;

	case kSS_TickToEnd:
		++g_iTickFramesSinceMid;
		// Tick until run-lost fires or we exceed the max wait budget.
		if (g_bRunLostFired || g_iTickFramesSinceMid >= kMAX_CHANNEL_WAIT_FRAMES)
		{
			g_iPhase = kSS_SnapshotEnd;
		}
		return true;

	case kSS_SnapshotEnd:
		TryGetEntityPos(g_xPriest, g_xPriestPosAtEnd);
		g_fHorizDriftMidToEnd = HorizontalDistance(g_xPriestPosAtMid, g_xPriestPosAtEnd);
		g_fYDriftMidToEnd     = g_xPriestPosAtEnd.y - g_xPriestPosAtMid.y;
		g_iPhase = kSS_Verify;
		return true;

	case kSS_Verify:
		std::printf("[P1ApprehendStill] midPos=(%.3f,%.3f,%.3f) endPos=(%.3f,%.3f,%.3f) horizDriftMidToEnd=%.3fm yDrift=%.3f runLostFired=%d (mid sampled after %d-frame settle, end sampled after %d ticks)\n",
			g_xPriestPosAtMid.x, g_xPriestPosAtMid.y, g_xPriestPosAtMid.z,
			g_xPriestPosAtEnd.x, g_xPriestPosAtEnd.y, g_xPriestPosAtEnd.z,
			g_fHorizDriftMidToEnd, g_fYDriftMidToEnd,
			(int)g_bRunLostFired,
			kSETTLE_FRAMES, g_iTickFramesSinceMid);
		std::fflush(stdout);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_xRunLostHandle = INVALID_EVENT_HANDLE;
		g_iPhase = kSS_Done;
		return false;

	case kSS_Done:
	default:
		if (g_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
			g_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
		return false;
	}
}

static bool Verify_P1ApprehendStill()
{
	if (!g_xPriest.IsValid() || !g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ApprehendStill: setup entities missing");
		return false;
	}
	// Channel must actually complete for the stillness assertion to be
	// meaningful; if it never fires the priest could be still for the
	// wrong reason (e.g. BT crash).
	if (!g_bRunLostFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendStill: DP_OnRunLost{Apprehended} never fired -- the channel did not complete, so the stillness check is meaningless. Most likely the apprehend BT branch regressed (see Test_P1Apprehend_PriestCatchesPlayer).");
		return false;
	}
	// Drift assertion: between mid and end the priest should be
	// stationary. mid is sampled after kSETTLE_FRAMES so Pursue ->
	// Apprehend has happened; end is sampled at dispatch.
	if (g_fHorizDriftMidToEnd < 0.0f
		|| g_fHorizDriftMidToEnd > kMAX_HORIZ_DRIFT_M)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendStill: priest drifted %.3fm horizontally between mid (post-settle) and end (channel complete), max allowed %.3fm. Apprehend.OnEnter is supposed to call NavMeshAgent::Stop(); the priest moving after settle means either (i) Stop() didn't take, (ii) ApplyVelocityToBodyIfPresent regressed and is pushing BT-requested velocity into Jolt instead of zeroing it, or (iii) physics gravity came back on the priest body.",
			g_fHorizDriftMidToEnd, kMAX_HORIZ_DRIFT_M);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ApprehendStillTest = {
	"Test_P1Apprehend_PriestStandsStillDuringChannel",
	&Setup_P1ApprehendStill,
	&Step_P1ApprehendStill,
	&Verify_P1ApprehendStill,
	500
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ApprehendStillTest);

#endif // ZENITH_INPUT_SIMULATOR
