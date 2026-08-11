#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_TennisCharacterization - characterization tests for the W3 behaviour-
 * graph conversion of the RenderTest tennis BT (TennisBrain) + the slim
 * PlayerActions graph.
 *
 * Written against the C++/BT versions FIRST; the graph versions must keep
 * every one of these green UNCHANGED. All probes go through surfaces that
 * survive the conversion: the referee's public getters (phase/points/epoch/
 * RNG-state seams), the brains' TennisRng state, entity transforms, and the
 * real input path (Zenith_InputSimulator state-setters only - never the
 * reentrant StepFrame / SimulateMouseClick helpers, which deadlock windowed).
 *
 * The RenderTest scene is the BOOT scene and is already simulating on
 * VARIABLE dt before a test's Setup runs, so every test reloads the scene
 * (LoadSceneByIndex(0, SINGLE)) in its Boot step AFTER fixed-dt engages -
 * from that point the whole sim is deterministic.
 *
 * The R2 RNG-determinism contract (brain-tick cadence / Selector gate order /
 * RNG draw counts) is NOT gated here. It used to be, by
 * RT_TennisDeterminismDigest: an FNV-1a digest over 2400 frames of the live
 * match, compared to a hard-pinned constant. That cost ~52 s and pinned an
 * entire physics simulation, so any legitimate gameplay or physics tweak broke
 * it with no signal about which clause of the contract had actually moved. It
 * is replaced by the hermetic, per-clause RT_TennisBrain* tests in
 * Tests/Test_TennisBrainContract.cpp, which drive the production brain-graph
 * definition directly with no match, terrain, navmesh or ball.
 *
 *   RT_TennisMatchFlow        - the autonomous match works: WARMUP->SERVING->
 *                               LIVE, a serve is struck, the receiver stands
 *                               up at the service line awaiting the serve
 *                               (ComputeReadyZ pin), at least one point
 *                               resolves, the ball epoch advances.
 *   RT_PlayerActions          - the discrete player actions that convert to
 *                               the PlayerActions graph: walk to a gun with
 *                               real held input, E equips, LMB fires (ammo
 *                               decrements), R reloads (clip refills), E
 *                               drops, T cycles the tennis camera modes.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include "RenderTest/Components/RenderTest_TennisMatchComponent.h"
#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_PlayerComponent.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"
#include "RenderTest/RenderTest_Tennis.h"

#include <cmath>
#include <cstdint>

namespace
{
	Zenith_Entity Tennis_FindEntity(const char* szName)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		if (!xScene.IsValid()) return Zenith_Entity();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) return Zenith_Entity();
		return pxSceneData->FindEntityByName(szName);
	}

	RenderTest_TennisMatchComponent* Tennis_FindReferee()
	{
		Zenith_Entity xMatch = Tennis_FindEntity("Tennis_Match");
		if (!xMatch.IsValid()) return nullptr;
		return xMatch.TryGetComponent<RenderTest_TennisMatchComponent>();
	}

	RenderTest_TennisAgentComponent* Tennis_FindBrain(int iSide)
	{
		Zenith_Entity xNpc = Tennis_FindEntity(iSide == 0 ? "Tennis_NPC_Near" : "Tennis_NPC_Far");
		if (!xNpc.IsValid()) return nullptr;
		return xNpc.TryGetComponent<RenderTest_TennisAgentComponent>();
	}

	bool Tennis_GetEntityPos(const char* szName, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = Tennis_FindEntity(szName);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	// Both brains started + referee resolved + the navmesh built (referee
	// OnStart completed). The gate every test waits on after its scene reload.
	bool Tennis_Ready(RenderTest_TennisMatchComponent*& pxRefereeOut)
	{
		pxRefereeOut = Tennis_FindReferee();
		RenderTest_TennisAgentComponent* pxNear = Tennis_FindBrain(0);
		RenderTest_TennisAgentComponent* pxFar = Tennis_FindBrain(1);
		return pxRefereeOut != nullptr && pxRefereeOut->IsNavMeshValid()
			&& pxNear != nullptr && pxNear->IsStarted()
			&& pxFar != nullptr && pxFar->IsStarted();
	}
}

// ============================================================================
// RT_TennisMatchFlow
// ============================================================================

namespace
{
	enum class FlowPhase { Boot, WaitReady, Observe, Done };

	FlowPhase g_eFlowPhase = FlowPhase::Boot;
	bool      g_bFlowSawServing = false;
	bool      g_bFlowSawLive = false;
	bool      g_bFlowServeStruck = false;
	int       g_iFlowPointsResolved = 0;
	uint32_t  g_uFlowMaxEpoch = 0;
	float     g_fFlowReceiverStanceMin = 1e30f;
	bool      g_bFlowWasPointOver = false;
}

static void Setup_TennisMatchFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eFlowPhase = FlowPhase::Boot;
	g_bFlowSawServing = false;
	g_bFlowSawLive = false;
	g_bFlowServeStruck = false;
	g_iFlowPointsResolved = 0;
	g_uFlowMaxEpoch = 0;
	g_fFlowReceiverStanceMin = 1e30f;
	g_bFlowWasPointOver = false;
}

static bool Step_TennisMatchFlow(int iFrame)
{
	switch (g_eFlowPhase)
	{
	case FlowPhase::Boot:
		// Reload the boot scene under fixed dt so the whole match sim runs
		// deterministically (the pre-Setup boot frames ran on variable dt).
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eFlowPhase = FlowPhase::WaitReady;
		return true;

	case FlowPhase::WaitReady:
	{
		RenderTest_TennisMatchComponent* pxReferee = nullptr;
		if (Tennis_Ready(pxReferee))
		{
			g_eFlowPhase = FlowPhase::Observe;
		}
		return iFrame < 600;
	}

	case FlowPhase::Observe:
	{
		RenderTest_TennisMatchComponent* pxReferee = Tennis_FindReferee();
		if (pxReferee == nullptr) return false;

		const RenderTest_Tennis::PointPhase ePhase = pxReferee->GetPhase();
		if (ePhase == RenderTest_Tennis::POINT_PHASE_SERVING)
		{
			g_bFlowSawServing = true;
			if (pxReferee->GetLastHitter() >= 0)
			{
				g_bFlowServeStruck = true;   // struck serve in flight, pre-bounce
			}
			else
			{
				// Receiver awaiting the serve: ComputeReadyZ stands them up at
				// the service line (the "receiver stands in" rally-length fix).
				// Track the closest they get; they start at the baseline ~5.5 m
				// away, so a sub-1.5 m minimum proves the stand-in behaviour.
				const int iReceiver = pxReferee->GetExpectedReceiver();
				Zenith_Maths::Vector3 xRecvPos;
				if (Tennis_GetEntityPos(iReceiver == 0 ? "Tennis_NPC_Near" : "Tennis_NPC_Far", xRecvPos))
				{
					const float fSign = (iReceiver == 0) ? -1.0f : 1.0f;
					const float fServiceLineZ = RenderTest_Tennis::fCOURT_CZ
						+ fSign * RenderTest_Tennis::fSERVICE_LINE_OFFSET;
					const float fDist = std::fabs(xRecvPos.z - fServiceLineZ);
					if (fDist < g_fFlowReceiverStanceMin) g_fFlowReceiverStanceMin = fDist;
				}
			}
		}
		else if (ePhase == RenderTest_Tennis::POINT_PHASE_LIVE)
		{
			g_bFlowSawLive = true;
		}

		// Count POINT_OVER entries (edge-detect) - immune to per-game point
		// resets, unlike a raw score sum.
		const bool bPointOver = ePhase == RenderTest_Tennis::POINT_PHASE_POINT_OVER;
		if (bPointOver && !g_bFlowWasPointOver)
		{
			++g_iFlowPointsResolved;
		}
		g_bFlowWasPointOver = bPointOver;

		if (pxReferee->GetBallEpoch() > g_uFlowMaxEpoch)
		{
			g_uFlowMaxEpoch = pxReferee->GetBallEpoch();
		}

		// Stop early once every pin is satisfied.
		if (g_bFlowSawServing && g_bFlowSawLive && g_bFlowServeStruck
			&& g_iFlowPointsResolved >= 1 && g_uFlowMaxEpoch >= 2
			&& g_fFlowReceiverStanceMin < 1.5f)
		{
			g_eFlowPhase = FlowPhase::Done;
			return false;
		}
		return true;
	}

	case FlowPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_TennisMatchFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	bool bPass = true;
	if (!g_bFlowSawServing || !g_bFlowSawLive)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisFlow] phases missed (serving %d live %d)",
			g_bFlowSawServing ? 1 : 0, g_bFlowSawLive ? 1 : 0);
		bPass = false;
	}
	if (!g_bFlowServeStruck)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisFlow] no serve was ever struck");
		bPass = false;
	}
	if (g_iFlowPointsResolved < 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisFlow] no point ever resolved");
		bPass = false;
	}
	if (g_uFlowMaxEpoch < 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisFlow] ball epoch never advanced past %u", g_uFlowMaxEpoch);
		bPass = false;
	}
	if (g_fFlowReceiverStanceMin >= 1.5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisFlow] receiver never stood in at the service line (min dist %.2f)",
			g_fFlowReceiverStanceMin);
		bPass = false;
	}
	return bPass;
}

static const Zenith_AutomatedTest g_xTennisMatchFlowTest = {
	"RT_TennisMatchFlow",
	&Setup_TennisMatchFlow,
	&Step_TennisMatchFlow,
	&Verify_TennisMatchFlow,
	/*maxFrames*/ 4200,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTennisMatchFlowTest);

// ============================================================================
// RT_PlayerActions
// ============================================================================

namespace
{
	enum class ActPhase
	{
		Boot, WaitReady, Steer, PressE, ReleaseE, AssertEquipped,
		FireDown, FireUp, AssertFired, PressR, ReleaseR, AwaitReload,
		PressDropE, ReleaseDropE, AssertDropped,
		CamPressT, CamReleaseT, Done
	};

	ActPhase g_eActPhase = ActPhase::Boot;
	int      g_iActFrame = 0;
	int      g_iCamPresses = 0;
	uint32_t g_uClipBeforeFire = 0;
	uint32_t g_uClipAfterFire = 0;
	bool     g_bEquipped = false;
	bool     g_bFired = false;
	bool     g_bReloaded = false;
	bool     g_bDropped = false;
	bool     g_bCamCycleOk = false;
	bool     g_bActDone = false;

	RenderTest_PlayerComponent* Act_Player()
	{
		Zenith_Entity xPlayer = Tennis_FindEntity("Player");
		if (!xPlayer.IsValid()) return nullptr;
		return xPlayer.TryGetComponent<RenderTest_PlayerComponent>();
	}

	// ★ C1a -- SetKeyHeld IS NO LONGER A USABLE HOLD HERE, AND ITS ABSENCE WOULD
	// BE SILENT. It writes the simulator's LEVEL array and nothing else, while
	// the player's movement now goes through the MOVE ACTION, whose key rows are
	// fed by ORDERED TRANSITIONS. A key "held" that way would reach IsKeyDown and
	// nothing else: the player would not move, no assertion would fire at the
	// call site, and Steer would just time out 2400 frames later.
	//
	// So the steer publishes real down/up EDGES -- and only on CHANGE, so a key
	// held across many frames raises exactly one press and one release, which is
	// what a human hand does and what the action layer's replay expects.
	const Zenith_KeyCode g_aeActMoveKeys[4] = {
		ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D
	};
	bool g_abActMoveKeyHeld[4] = { false, false, false, false };

	void Act_SetMoveKeyHeld(int iKey, bool bHeld)
	{
		if (g_abActMoveKeyHeld[iKey] == bHeld)
		{
			return;
		}
		g_abActMoveKeyHeld[iKey] = bHeld;
		if (bHeld)
		{
			Zenith_InputSimulator::SimulateKeyDown(g_aeActMoveKeys[iKey]);
		}
		else
		{
			Zenith_InputSimulator::SimulateKeyUp(g_aeActMoveKeys[iKey]);
		}
	}

	void Act_ReleaseMovementKeys()
	{
		for (int i = 0; i < 4; ++i)
		{
			Act_SetMoveKeyHeld(i, false);
		}
	}

	// After this many camera-cycle presses the expected GameplayState triple.
	bool Act_CamStateMatches(int iPresses)
	{
		const bool bSpectator = RenderTest_GameplayState::s_bTennisSpectatorActive;
		const bool bFollow = RenderTest_GameplayState::s_bTennisFollowActive;
		const int iSide = RenderTest_GameplayState::s_iTennisFollowSide;
		switch (iPresses)
		{
		case 1: return bSpectator && !bFollow;               // court overlook
		case 2: return bSpectator && bFollow && iSide == 0;  // follow near
		case 3: return bSpectator && bFollow && iSide == 1;  // follow far
		case 4: return !bSpectator && !bFollow;              // back off
		}
		return false;
	}
}

static void Setup_PlayerActions()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	// The harness normalised BOTH sides at the test boundary (the simulator's
	// level table AND the action layer's transition-fed source shadow), so the
	// edge tracker starts from the same all-released state they did. Getting
	// this wrong is silent: a stale "held" entry suppresses the down edge that
	// would have started the walk.
	for (int i = 0; i < 4; ++i)
	{
		g_abActMoveKeyHeld[i] = false;
	}
	g_eActPhase = ActPhase::Boot;
	g_iActFrame = 0;
	g_iCamPresses = 0;
	g_uClipBeforeFire = 0;
	g_uClipAfterFire = 0;
	g_bEquipped = false;
	g_bFired = false;
	g_bReloaded = false;
	g_bDropped = false;
	g_bCamCycleOk = true;   // ANDed per press below
	g_bActDone = false;
}

static bool Step_PlayerActions(int iFrame)
{
	switch (g_eActPhase)
	{
	case ActPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eActPhase = ActPhase::WaitReady;
		return true;

	case ActPhase::WaitReady:
		if (Act_Player() != nullptr && Tennis_FindEntity("Gun_Pistol").IsValid())
		{
			g_eActPhase = ActPhase::Steer;
		}
		return iFrame < 600;

	case ActPhase::Steer:
	{
		// Walk to the pistol on the real input path. Camera yaw starts 0
		// (GameplayState::Reset in the player's OnAwake) and no mouse input
		// arrives, so camera-relative movement maps W->+Z, D->+X.
		Zenith_Maths::Vector3 xPlayerPos, xGunPos;
		if (!Tennis_GetEntityPos("Player", xPlayerPos)) return false;
		if (!Tennis_GetEntityPos("Gun_Pistol", xGunPos)) return false;
		const float fDx = xGunPos.x - xPlayerPos.x;
		const float fDz = xGunPos.z - xPlayerPos.z;
		const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
		if (fDist <= 1.8f)   // comfortably inside the 2.2 pickup radius
		{
			Act_ReleaseMovementKeys();
			g_eActPhase = ActPhase::PressE;
			return true;
		}
		Act_SetMoveKeyHeld(0, fDz >  0.15f);   // W
		Act_SetMoveKeyHeld(1, fDz < -0.15f);   // S
		Act_SetMoveKeyHeld(3, fDx >  0.15f);   // D
		Act_SetMoveKeyHeld(2, fDx < -0.15f);   // A
		return iFrame < 2400;
	}

	case ActPhase::PressE:
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_E);
		g_eActPhase = ActPhase::ReleaseE;
		return true;

	case ActPhase::ReleaseE:
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_E);
		g_iActFrame = 0;
		g_eActPhase = ActPhase::AssertEquipped;
		return true;

	case ActPhase::AssertEquipped:
	{
		RenderTest_PlayerComponent* pxPlayer = Act_Player();
		if (pxPlayer != nullptr && pxPlayer->IsHoldingGun())
		{
			g_bEquipped = true;
			g_uClipBeforeFire = pxPlayer->GetAmmoInClip();
			g_eActPhase = ActPhase::FireDown;
			return true;
		}
		return ++g_iActFrame < 30;   // give the press a few frames to land
	}

	case ActPhase::FireDown:
		Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
		g_eActPhase = ActPhase::FireUp;
		return true;

	case ActPhase::FireUp:
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
		g_iActFrame = 0;
		g_eActPhase = ActPhase::AssertFired;
		return true;

	case ActPhase::AssertFired:
	{
		RenderTest_PlayerComponent* pxPlayer = Act_Player();
		if (pxPlayer != nullptr && pxPlayer->GetAmmoInClip() == g_uClipBeforeFire - 1u)
		{
			g_bFired = true;
			g_uClipAfterFire = pxPlayer->GetAmmoInClip();
			g_eActPhase = ActPhase::PressR;
			return true;
		}
		return ++g_iActFrame < 30;
	}

	case ActPhase::PressR:
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_R);
		g_eActPhase = ActPhase::ReleaseR;
		return true;

	case ActPhase::ReleaseR:
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_R);
		g_iActFrame = 0;
		g_eActPhase = ActPhase::AwaitReload;
		return true;

	case ActPhase::AwaitReload:
	{
		// Reload takes 1.5 s (90 fixed frames); the clip refills at completion.
		RenderTest_PlayerComponent* pxPlayer = Act_Player();
		if (pxPlayer != nullptr && !pxPlayer->IsReloading()
			&& g_iActFrame > 10 && pxPlayer->GetAmmoInClip() > g_uClipAfterFire)
		{
			g_bReloaded = true;
			g_eActPhase = ActPhase::PressDropE;
			return true;
		}
		return ++g_iActFrame < 240;
	}

	case ActPhase::PressDropE:
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_E);
		g_eActPhase = ActPhase::ReleaseDropE;
		return true;

	case ActPhase::ReleaseDropE:
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_E);
		g_iActFrame = 0;
		g_eActPhase = ActPhase::AssertDropped;
		return true;

	case ActPhase::AssertDropped:
	{
		RenderTest_PlayerComponent* pxPlayer = Act_Player();
		if (pxPlayer != nullptr && !pxPlayer->IsHoldingGun())
		{
			g_bDropped = true;
			g_iCamPresses = 0;
			g_eActPhase = ActPhase::CamPressT;
			return true;
		}
		return ++g_iActFrame < 30;
	}

	case ActPhase::CamPressT:
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_T);
		g_eActPhase = ActPhase::CamReleaseT;
		return true;

	case ActPhase::CamReleaseT:
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_T);
		++g_iCamPresses;
		// The press landed last frame (T is handled in the camera's
		// OnLateUpdate of the frame the key-down was visible); by the key-up
		// frame the cycled state is observable.
		g_bCamCycleOk = g_bCamCycleOk && Act_CamStateMatches(g_iCamPresses);
		if (g_iCamPresses >= 4)
		{
			g_bActDone = true;
			g_eActPhase = ActPhase::Done;
			return false;
		}
		g_eActPhase = ActPhase::CamPressT;
		return true;
	}

	case ActPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PlayerActions()
{
	Zenith_InputSimulator::ClearFixedDt();
	Act_ReleaseMovementKeys();
	bool bPass = true;
	if (!g_bActDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerActions] never completed (phase %d)",
			static_cast<int>(g_eActPhase));
		bPass = false;
	}
	if (!g_bEquipped) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerActions] E never equipped the pistol"); bPass = false; }
	if (!g_bFired)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerActions] LMB never decremented the clip"); bPass = false; }
	if (!g_bReloaded) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerActions] R never refilled the clip"); bPass = false; }
	if (!g_bDropped)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerActions] second E never dropped the gun"); bPass = false; }
	if (!g_bCamCycleOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerActions] T camera cycle state mismatch"); bPass = false; }
	return bPass;
}

static const Zenith_AutomatedTest g_xPlayerActionsTest = {
	"RT_PlayerActions",
	&Setup_PlayerActions,
	&Step_PlayerActions,
	&Verify_PlayerActions,
	/*maxFrames*/ 3600,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerActionsTest);

#endif // ZENITH_INPUT_SIMULATOR
