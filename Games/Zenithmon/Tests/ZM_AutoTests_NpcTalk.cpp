#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_NpcTalk -- ZM_NpcTalk_Test (S6 item 3 SC5). THE PROOF THE WHOLE
// ITEM EXISTS TO PRODUCE: a player, in the real baked Dawnmere, under real
// simulated input, WALKS UP to an authored NPC and TALKS to it.
//
// Everything before this sub-commit was decomposition -- a pure gate, a pure
// picker, a const table, and a headless dispatch fixture. None of them can tell
// you whether an NPC actually exists in the world, whether a player can reach it,
// or whether the words on the row reach the screen. This test can.
//
// It requires GRAPHICS (m_bRequiresGraphics = true) because it needs the real
// Dawnmere scene, its terrain streaming and its render path -- NOT because
// physics is unavailable headless. (Physics IS live headless:
// Zenith_Physics::HasActiveSimulation() is `m_pxPhysicsSystem != nullptr` and
// Physics().Initialise() is unconditional. Do not justify anything here by the
// old, wrong "physics is off headless" claim.)
//
// DESIGN RULES this file obeys, each of which is a failure mode this project has
// actually shipped before:
//   * EVERY phase flag DEFAULTS TO FAILING, so a phase that never runs fails.
//   * The negatives are load-bearing. An interactor stubbed permanently-true, or
//     a re-raise loop, passes a positives-only version of this test outright.
//   * The press is EVENT-DRIVEN: the test polls EvaluateForTests until the live
//     decision says OK *at the villager*, then presses. It never walks N frames
//     and hopes.
//   * The walk has its own PROGRESS WATCHDOG, so a broken approach fails in ~1
//     second with the measured distance and the held-key set, not by silently
//     exhausting the overall frame budget.
//   * A BASIS PROBE runs first and fails in ~30 frames with the measured dx/dz if
//     the movement basis is wrong -- the difference between a legible failure and
//     a mysterious walk timeout.
//   * SetPosition appears NOWHERE. Motion is physics-driven and is ASSERTED to be
//     (run speed + a real linear velocity + >1 m of displacement).
//   * No reentrant Zenith_MainLoop and no reentrant InputSimulator helpers inside
//     a Step -- that deadlocks on vkWaitForFences.
//
// FRAME ORDERING this test depends on: Zenith_AutomatedTestRunner::Tick() (which
// calls this Step) runs BEFORE g_xEngine.Scenes().Update() in Zenith_MainLoop.
// A key edge injected in Step is therefore consumed by ZM_PlayerController::
// OnUpdate LATER IN THE SAME FRAME, and is observable from the NEXT Step call.
// Every "press here, assert next frame" pair below is that, and nothing more.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"       // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "EntityComponent/Zenith_PhysicsQuery.h"        // RaycastIgnoring -- the W5 ground-truth probe
#include "Input/Zenith_InputSimulator.h"
#include "Zenithmon/Tests/ZM_TestWalkDrive.h"   // the ONE walk driver -- read its header before trusting a straight line
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"   // the W5 anchor table under test

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
	constexpr float fTALK_FIXED_DT = 1.0f / 60.0f;

	// Dawnmere's build index (ZM_WorldSpec / ZM-D-012).
	constexpr int iTALK_OVERWORLD_BUILD_INDEX = 2;

	// The SC5 authoring contract, restated here on purpose: a windowed test names
	// the authored entity so that DELETING the NPC from Zenithmon.cpp fails this
	// test loudly rather than quietly removing what it was proving.
	constexpr const char* szVILLAGER_ENTITY_NAME = "Npc_Villager";

	// ---- Phase budgets. Each phase owns a deadline that FAILS; none of them may
	// be reached by the harness's overall m_iMaxFrames instead, because that path
	// produces no diagnostic about WHERE the test stalled. ----
	// (The two assert-only phases resolve on their first frame and therefore have no
	// deadline -- a timeout there would be unreachable code pretending to guard.)
	constexpr int iBOOT_DEADLINE_FRAMES     = 600;
	constexpr int iBASIS_PROBE_FRAMES       = 30;   // ~0.5 s at 60 Hz -- fails FAST
	constexpr int iAPPROACH_DEADLINE_FRAMES = 900;
	constexpr int iARM_DEADLINE_FRAMES      = 180;

	// The approach must keep CLOSING on the target. 60 consecutive frames (1 s)
	// without a real improvement means stuck-on-geometry / wrong-basis / oscillation,
	// and the test says so immediately instead of burning the whole budget.
	constexpr int   iSTALL_LIMIT_FRAMES    = 60;
	constexpr float fSTALL_IMPROVEMENT     = 0.01f;
	// The basis probe must show meaningful, +Z-DOMINANT travel.
	constexpr float fBASIS_MIN_FORWARD     = 0.5f;
	// "Really moved" for the physics-motion evidence, in metres.
	constexpr float fPHYSICS_MIN_TRAVEL    = 1.0f;
	// ...and a real XZ speed rather than a nudge, in m/s.
	constexpr float fPHYSICS_MIN_SPEED     = 1.0f;
	constexpr float fRUN_SPEED_TOLERANCE   = 0.001f;

	enum class TalkPhase
	{
		Boot,
		BasisProbe,
		NegativePress,      // out of range: press E
		NegativeAssert,     // ...and prove nothing happened
		Approach,
		ArmAndPress,
		AssertRaise,
		ReRaisePress,       // menu open: press E again
		ReRaiseAssert,      // ...and prove the count did NOT move
		Done,
	};

	struct TalkPlayerView
	{
		Zenith_EntityID           m_xEntityID   = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3     m_xPosition   = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController*      m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider  = nullptr;
	};

	TalkPhase g_eTalkPhase = TalkPhase::Done;
	int       g_iTalkPhaseFrames = 0;

	Zenith_EntityID       g_xTalkVillagerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xTalkVillagerPosition(0.0f);
	Zenith_Maths::Vector3 g_xTalkStartPosition(0.0f);
	Zenith_Maths::Vector3 g_xTalkBasisStart(0.0f);
	u_int                 g_uTalkRaiseCountBefore = 0u;

	// Approach bookkeeping (also the failure diagnostics).
	float g_fTalkBestDistance    = 0.0f;
	float g_fTalkCurrentDistance = 0.0f;
	int   g_iTalkStallFrames     = 0;
	bool  g_abTalkHeldKeys[4]    = { false, false, false, false };   // W A S D

	// Basis-probe diagnostics, reported verbatim in the failure text.
	float g_fTalkBasisDeltaX = 0.0f;
	float g_fTalkBasisDeltaZ = 0.0f;

	// The re-raise negative's ONE press-sensitive pair: the conversation's read cursor
	// either side of the press (see ReRaiseAssert). The defaults DIFFER on both halves,
	// so a phase that never ran cannot read as "unchanged".
	u_int       g_uTalkReRaiseLineIdxBefore = 99u;
	u_int       g_uTalkReRaiseLineIdxAfter  = 98u;
	std::string g_strTalkReRaiseLineBefore;
	std::string g_strTalkReRaiseLineAfter   = "<not sampled>";

	// Boot diagnostics: exactly WHAT had not resolved when the deadline expired.
	bool g_bTalkSawScene       = false;
	bool g_bTalkSawPlayer      = false;
	bool g_bTalkSawPlayerBody  = false;
	bool g_bTalkSawVillager    = false;

	// ---- Phase flags. ALL DEFAULT TO FAILING. ----
	bool g_bTalkPrerequisitesPresent = false;
	bool g_bTalkSkipped              = false;
	bool g_bTalkBootPassed           = false;
	bool g_bTalkBasisPassed          = false;
	bool g_bTalkNegativePassed       = false;
	bool g_bTalkApproachPassed       = false;
	bool g_bTalkRaisePassed          = false;
	bool g_bTalkContentPassed        = false;
	bool g_bTalkReRaisePassed        = false;
	bool g_bTalkReRaiseLinePassed    = false;   // ...its ONE press-sensitive clause
	bool g_bTalkPhysicsMotionSeen    = false;
	const char* g_szTalkFailure = "test did not reach verification";

	void ClearTalkInput()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_UP);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_DOWN);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_RIGHT);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_RIGHT_SHIFT);
		g_abTalkHeldKeys[0] = false;
		g_abTalkHeldKeys[1] = false;
		g_abTalkHeldKeys[2] = false;
		g_abTalkHeldKeys[3] = false;
	}

	// The observed reject name when the out-of-range negative refuses for an
	// unexpected reason -- dumped by Verify, matching this file's convention of
	// keeping measured values in globals rather than formatting into FailTalk.
	const char* g_szTalkNegativeReject = "<not sampled>";

	void FailTalk(const char* szReason)
	{
		g_szTalkFailure = szReason;
		g_eTalkPhase = TalkPhase::Done;
		ClearTalkInput();
	}

	// The shared, unit-tested walk driver. See
	// Tests/ZM_TestWalkDrive.h -- it is CAMERA-RELATIVE and QUANTISED TO EIGHT
	// DIRECTIONS, so the player walks a PURSUIT CURVE rather than the straight
	// line, and anything this suite must approach ON A BEARING wants one of the
	// driver's fixed points. This wrapper exists only to keep the local name and
	// to mirror the held keys where this file reports them.
	void DriveTowardXZ(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		// ★ THE CLEAR STAYS HERE, AND DROPPING IT COST A BATCH. Consolidating the
		// driver moved this call out on the assumption that callers cleared before
		// driving; they do not -- every one of the eight originals cleared INSIDE,
		// and without it last frame's keys stay held and the walk curves away.
		// Six automated tests went red. The shared driver deliberately does not
		// clear, because each caller releases a DIFFERENT key set.
		ClearTalkInput();
		const ZM_WalkDriveKeys xKeys =
			ZM_DriveWalkTowardXZ(xPosition, xTarget, /*bRun*/ true);
		xKeys.CopyTo(g_abTalkHeldKeys);
	}

	float PlanarDistance(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	bool RequiredTalkAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_GEOMETRY_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_GEOMETRY_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" ZENITH_GEOMETRY_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			std::error_code xError;
			if (!std::filesystem::is_regular_file(strPath, xError) || xError)
			{
				return false;
			}
			const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
			if (xError || ulSize == 0u)
			{
				return false;
			}
		}
		return true;
	}

	bool FindActiveTalkPlayer(TalkPlayerView& xOut)
	{
		xOut = TalkPlayerView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xEntityID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xEntityID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
			});
		return uCount == 1u;
	}

	// Resolve the AUTHORED villager by name, and refresh its world position. The
	// entity is re-resolved every frame rather than cached as a pointer: the ECS
	// pool relocates components, so a cached ZM_Interactable* is a dangling read
	// waiting to happen.
	bool ResolveVillager(
		Zenith_EntityID& xEntityIDOut,
		Zenith_Maths::Vector3& xPositionOut,
		bool& bInteractableOut)
	{
		xEntityIDOut = INVALID_ENTITY_ID;
		bInteractableOut = false;
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxData == nullptr)
		{
			return false;
		}
		Zenith_Entity xEntity = pxData->FindEntityByName(szVILLAGER_ENTITY_NAME);
		if (!xEntity.IsValid())
		{
			return false;
		}
		ZM_Interactable* pxInteractable = xEntity.TryGetComponent<ZM_Interactable>();
		Zenith_TransformComponent* pxTransform =
			xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxInteractable == nullptr || pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xPositionOut);
		xEntityIDOut = xEntity.GetEntityID();
		bInteractableOut = pxInteractable->IsInteractable();
		return true;
	}

	// The LIVE menu-stack singleton, for the content assertion. Null whenever the
	// persistent ZM_MenuRoot is absent -- which Setup has already turned into a skip,
	// so reaching a null here mid-run is a genuine failure.
	ZM_UI_MenuStack* ResolveMenuStack()
	{
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			return nullptr;
		}
		Zenith_Entity xMenuRoot = g_xEngine.Scenes().ResolveEntity(xMenuRootID);
		return xMenuRoot.IsValid()
			? xMenuRoot.TryGetComponent<ZM_UI_MenuStack>()
			: nullptr;
	}

	// Fold this frame's evidence that the motion is PHYSICS-DRIVEN rather than a
	// teleport: the controller is asking for RUN speed, the BODY has a real planar
	// velocity, and the player has covered real ground since the walk began.
	void AccumulatePhysicsMotionEvidence(const TalkPlayerView& xPlayer)
	{
		if (xPlayer.m_pxCollider == nullptr || !xPlayer.m_pxCollider->HasValidBody())
		{
			return;
		}
		const Zenith_Maths::Vector3 xVelocity =
			g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
		const float fPlanarSpeed = std::sqrt(
			xVelocity.x * xVelocity.x + xVelocity.z * xVelocity.z);
		g_bTalkPhysicsMotionSeen |=
			PlanarDistance(xPlayer.m_xPosition, g_xTalkStartPosition) > fPHYSICS_MIN_TRAVEL
			&& std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
				- ZM_PlayerController::fRUN_SPEED) <= fRUN_SPEED_TOLERANCE
			&& fPlanarSpeed > fPHYSICS_MIN_SPEED;
	}

	void Setup_NpcTalk()
	{
		g_eTalkPhase = TalkPhase::Done;
		g_iTalkPhaseFrames = 0;
		g_xTalkVillagerEntityID = INVALID_ENTITY_ID;
		g_xTalkVillagerPosition = Zenith_Maths::Vector3(0.0f);
		g_xTalkStartPosition = Zenith_Maths::Vector3(0.0f);
		g_xTalkBasisStart = Zenith_Maths::Vector3(0.0f);
		g_uTalkRaiseCountBefore = 0u;
		g_fTalkBestDistance = 0.0f;
		g_fTalkCurrentDistance = 0.0f;
		g_iTalkStallFrames = 0;
		g_fTalkBasisDeltaX = 0.0f;
		g_fTalkBasisDeltaZ = 0.0f;
		g_bTalkSawScene = false;
		g_bTalkSawPlayer = false;
		g_bTalkSawPlayerBody = false;
		g_bTalkSawVillager = false;
		g_bTalkSkipped = false;
		g_bTalkPrerequisitesPresent = false;
		g_szTalkNegativeReject = "<not sampled>";
		g_bTalkBootPassed = false;
		g_bTalkBasisPassed = false;
		g_bTalkNegativePassed = false;
		g_bTalkApproachPassed = false;
		g_bTalkRaisePassed = false;
		g_bTalkContentPassed = false;
		g_bTalkReRaisePassed = false;
		g_bTalkReRaiseLinePassed = false;
		g_uTalkReRaiseLineIdxBefore = 99u;
		g_uTalkReRaiseLineIdxAfter = 98u;
		g_strTalkReRaiseLineBefore.clear();
		g_strTalkReRaiseLineAfter = "<not sampled>";
		g_bTalkPhysicsMotionSeen = false;
		g_szTalkFailure = "test did not reach verification";

		Zenith_InputSimulator::ResetAllInputState();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// ---- The ONLY two skips, both "a baked dependency does not exist" ----
		// Deliberately narrow: a test that always skips reports PASS forever and is
		// worse than no test at all. A MISSING NPC, an unconfigured NPC, an
		// unreachable NPC and a mute NPC all FAIL below -- none of them skip.
		//
		// RequestSkip bypasses Verify, so no fixed-dt or scene-load state may be
		// installed until both prerequisites are known present.
		g_bTalkPrerequisitesPresent = RequiredTalkAssetsPresent();
		if (!g_bTalkPrerequisitesPresent)
		{
			g_bTalkSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcTalk] the Dawnmere scene / terrain bake is absent or incomplete -- "
				"there is no world to walk through (run a *_True config once to bake it)");
			return;
		}
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bTalkSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcTalk] no persistent ZM_MenuRoot / ZM_UI_MenuStack singleton -- "
				"FrontEnd.zscen has not been baked, so there is no screen for an NPC to raise");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fTALK_FIXED_DT);
		g_eTalkPhase = TalkPhase::Boot;
		g_xEngine.Scenes().LoadSceneByIndex(
			iTALK_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_NpcTalk(int)
	{
		if (g_eTalkPhase == TalkPhase::Done)
		{
			return false;
		}
		++g_iTalkPhaseFrames;

		switch (g_eTalkPhase)
		{
		// ------------------------------------------------------------------
		// 1. BOOT / SETTLE. Wait for the scene, the player, the player's physics
		//    body, and the AUTHORED villager's armed ZM_Interactable. Expiry names
		//    exactly which of the four never resolved -- "Dawnmere did not become
		//    ready" on its own has cost this project entire debugging sessions.
		// ------------------------------------------------------------------
		case TalkPhase::Boot:
		{
			TalkPlayerView xPlayer;
			bool bVillagerInteractable = false;
			const bool bScene = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
					== iTALK_OVERWORLD_BUILD_INDEX;
			const bool bPlayer = bScene && FindActiveTalkPlayer(xPlayer);
			const bool bBody = bPlayer
				&& xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded();
			const bool bVillager = bScene && ResolveVillager(
				g_xTalkVillagerEntityID, g_xTalkVillagerPosition, bVillagerInteractable);
			g_bTalkSawScene |= bScene;
			g_bTalkSawPlayer |= bPlayer;
			g_bTalkSawPlayerBody |= bBody;
			g_bTalkSawVillager |= bVillager && bVillagerInteractable;

			if (!bScene || !bPlayer || !bBody || !bVillager || !bVillagerInteractable)
			{
				if (g_iTalkPhaseFrames > iBOOT_DEADLINE_FRAMES)
				{
					FailTalk("Dawnmere never reached a talkable state (see the "
						"scene/player/body/villager flags logged below -- a false "
						"villager flag means SC5's authored Npc_Villager is missing "
						"or was authored non-interactable)");
					return false;
				}
				return true;
			}

			// The walk-up target must genuinely be a walk AWAY, not a spawn-camp. TWICE
			// the global reach, so the short basis probe below cannot accidentally
			// consume the whole separation and leave the approach phase vacuous. The
			// SC5 authoring places the villager 10 m out against a 2.5 m reach, so this
			// has ample headroom -- it exists to catch a LATER re-placement that quietly
			// parks the villager on the spawn.
			const float fSpawnDistance =
				PlanarDistance(xPlayer.m_xPosition, g_xTalkVillagerPosition);
			g_fTalkCurrentDistance = fSpawnDistance;
			if (fSpawnDistance <= fZM_INTERACT_MAX_DISTANCE * 2.0f)
			{
				FailTalk("the authored villager spawns too close to the player -- the "
					"walk-up phase would prove nothing");
				return false;
			}

			g_xTalkStartPosition = xPlayer.m_xPosition;
			g_xTalkBasisStart = xPlayer.m_xPosition;
			g_bTalkBootPassed = true;
			// Plain held W (no run modifier): this probe characterises the BASIS, and
			// it must match the shipped ZM_DawnmerePlayerCamera_Test evidence exactly.
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
			g_eTalkPhase = TalkPhase::BasisProbe;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 2. BASIS PROBE. Held W must move the player +Z, and +Z must dominate.
		//    This exists so a broken movement basis fails in 30 frames WITH THE
		//    MEASURED DELTAS, instead of the approach phase silently grinding out
		//    900 frames against a wall.
		// ------------------------------------------------------------------
		case TalkPhase::BasisProbe:
		{
			TalkPlayerView xPlayer;
			if (!FindActiveTalkPlayer(xPlayer))
			{
				FailTalk("the player disappeared during the basis probe");
				return false;
			}
			if (g_iTalkPhaseFrames < iBASIS_PROBE_FRAMES)
			{
				return true;
			}
			g_fTalkBasisDeltaX = xPlayer.m_xPosition.x - g_xTalkBasisStart.x;
			g_fTalkBasisDeltaZ = xPlayer.m_xPosition.z - g_xTalkBasisStart.z;
			ClearTalkInput();
			if (g_fTalkBasisDeltaZ < fBASIS_MIN_FORWARD
				|| std::fabs(g_fTalkBasisDeltaZ) <= std::fabs(g_fTalkBasisDeltaX))
			{
				FailTalk("held W did not move the player forward along +Z -- the "
					"movement basis is wrong (measured deltas are logged below)");
				return false;
			}
			g_bTalkBasisPassed = true;
			g_eTalkPhase = TalkPhase::NegativePress;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 3. OUT-OF-RANGE NEGATIVE (before the walk). WITHOUT THIS, an interactor
		//    stubbed permanently-true passes this entire test. The seam must refuse
		//    from here, and an actual E press must raise NOTHING.
		// ------------------------------------------------------------------
		case TalkPhase::NegativePress:
		{
			TalkPlayerView xPlayer;
			bool bVillagerInteractable = false;
			if (!FindActiveTalkPlayer(xPlayer)
				|| !ResolveVillager(g_xTalkVillagerEntityID,
					g_xTalkVillagerPosition, bVillagerInteractable)
				|| !bVillagerInteractable)
			{
				FailTalk("player or armed villager was lost before the out-of-range negative");
				return false;
			}
			g_fTalkCurrentDistance =
				PlanarDistance(xPlayer.m_xPosition, g_xTalkVillagerPosition);
			// 2 x fZM_INTERACT_MAX_DISTANCE (5.0 m) is beyond the 2.9 m effective reach
			// this villager is actually authored with (2.5 global + 0.4 authored), so the
			// refusal below is unambiguously "too far", never a boundary case. It does
			// NOT cover the 10.5 m an author could reach by pushing the per-NPC radius up
			// to ZM_Interactable::fMAX_RADIUS (8.0 m).
			if (g_fTalkCurrentDistance <= fZM_INTERACT_MAX_DISTANCE * 2.0f)
			{
				FailTalk("the basis probe walked too close to the villager -- the "
					"out-of-range negative would be a boundary case, not a negative");
				return false;
			}

			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
			if (eSeam == ZM_INTERACT_OK || xSeamTarget != INVALID_ENTITY_ID)
			{
				FailTalk("the interaction seam accepted a target from OUT OF RANGE -- "
					"the picker or the interactor is stubbed / always-true");
				return false;
			}
			if (ZM_UI_MenuStack::IsMenuOpen())
			{
				FailTalk("a menu was already open before the first interact press");
				return false;
			}

			// The RAW key code, deliberately: a windowed test characterises the binding
			// rather than restating ZM_Bindings' constant back to itself.
			g_uTalkRaiseCountBefore = xRuntime.GetRaiseCount();
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
			g_eTalkPhase = TalkPhase::NegativeAssert;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		case TalkPhase::NegativeAssert:
		{
			TalkPlayerView xPlayer;
			if (!FindActiveTalkPlayer(xPlayer))
			{
				FailTalk("the player disappeared during the out-of-range negative");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			// The edge has been consumed by the controller's OnUpdate one frame ago
			// (Step runs before Scenes().Update -- see the file header).
			if (xRuntime.GetRaiseCount() != g_uTalkRaiseCountBefore)
			{
				FailTalk("pressing E from out of range RAISED a screen");
				return false;
			}
			// ★ TWO DIFFERENT PROOFS, and they are NOT interchangeable.
			// HasLatchedResult() proves only THE RUNTIME TICKED AT ALL:
			// ZM_InteractionRuntime::Tick sets s_bHasLatchedResult = true
			// UNCONDITIONALLY, outside the `if (bInteractPressed)` branch that writes
			// s_eLastResult / s_xLastTarget (the runtime source says so itself --
			// "s_bHasLatchedResult still moves every tick, so it remains an honest 'the
			// runtime ran' signal"). It would therefore go false only if
			// ZM_PlayerController::OnUpdate stopped calling Tick at all.
			// The EDGE proof is the GetLastResult() == OUT_OF_RANGE check below:
			// s_eLastResult is written ONLY under bInteractPressed and its reset value
			// is NO_INPUT_EDGE, so that clause -- and only that clause -- fails if the E
			// press never landed. It is also the half that survives a stubbed-true
			// interactor: a real refusal for a real reason, not merely "nothing
			// happened". Three enabled probes exist and all are far beyond reach, so
			// OUT_OF_RANGE is fully deterministic here.
			if (!xRuntime.HasLatchedResult())
			{
				FailTalk("the interaction runtime never ticked at all "
					"(ZM_PlayerController::OnUpdate is no longer driving it)");
				return false;
			}
			if (xRuntime.GetLastResult() != ZM_INTERACT_REJECT_OUT_OF_RANGE)
			{
				g_szTalkNegativeReject = ZM_InteractRejectName(xRuntime.GetLastResult());
				FailTalk("the out-of-range press was refused for the WRONG reason "
					"(the observed reject name is logged below)");
				return false;
			}
			if (xRuntime.GetLastTarget() != INVALID_ENTITY_ID)
			{
				FailTalk("an out-of-range refusal still named a target entity");
				return false;
			}
			if (ZM_UI_MenuStack::IsMenuOpen())
			{
				FailTalk("pressing E from out of range opened the menu");
				return false;
			}
			// No deadline here on purpose: this phase resolves on its FIRST frame
			// either way, so a timeout would be unreachable code pretending to guard.
			g_bTalkNegativePassed = true;
			g_fTalkBestDistance = g_fTalkCurrentDistance;
			g_iTalkStallFrames = 0;
			// RE-BASELINE the displacement here, at the APPROACH's start. Captured at
			// spawn instead, the basis probe (30 frames of held W, ~2 m) would already
			// have satisfied fPHYSICS_MIN_TRAVEL before the walk began, so the
			// displacement clause of the physics-motion proof would measure the PROBE
			// rather than the approach it is written to measure.
			g_xTalkStartPosition = xPlayer.m_xPosition;
			g_eTalkPhase = TalkPhase::Approach;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 4. APPROACH -- a CLOSED LOOP on the live position, with a progress
		//    watchdog. It ends the instant the LIVE decision says the villager is
		//    reachable; it never counts frames and hopes.
		// ------------------------------------------------------------------
		case TalkPhase::Approach:
		{
			TalkPlayerView xPlayer;
			bool bVillagerInteractable = false;
			if (!FindActiveTalkPlayer(xPlayer)
				|| !ResolveVillager(g_xTalkVillagerEntityID,
					g_xTalkVillagerPosition, bVillagerInteractable)
				|| !bVillagerInteractable)
			{
				FailTalk("player or armed villager was lost during the approach");
				return false;
			}

			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			// Nothing may raise while the player is merely WALKING: no interact edge is
			// injected in this phase, so any movement here is a spurious raise.
			if (xRuntime.GetRaiseCount() != g_uTalkRaiseCountBefore)
			{
				FailTalk("a screen was raised during the walk, with no interact press");
				return false;
			}

			AccumulatePhysicsMotionEvidence(xPlayer);

			g_fTalkCurrentDistance =
				PlanarDistance(xPlayer.m_xPosition, g_xTalkVillagerPosition);

			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			if (xRuntime.EvaluateForTests(xSeamTarget) == ZM_INTERACT_OK
				&& xSeamTarget == g_xTalkVillagerEntityID)
			{
				ClearTalkInput();
				g_bTalkApproachPassed = true;
				g_eTalkPhase = TalkPhase::ArmAndPress;
				g_iTalkPhaseFrames = 0;
				return true;
			}

			// The watchdog: the walk must keep CLOSING. Stalling for a second means
			// stuck geometry, a wrong basis or an oscillation, and the failure names
			// the current distance, the best distance and the held keys.
			if (g_fTalkCurrentDistance < g_fTalkBestDistance - fSTALL_IMPROVEMENT)
			{
				g_fTalkBestDistance = g_fTalkCurrentDistance;
				g_iTalkStallFrames = 0;
			}
			else if (++g_iTalkStallFrames > iSTALL_LIMIT_FRAMES)
			{
				FailTalk("the walk-up STALLED -- the player stopped closing on the "
					"villager (distances and held keys logged below)");
				return false;
			}

			if (g_iTalkPhaseFrames > iAPPROACH_DEADLINE_FRAMES)
			{
				FailTalk("the walk-up never brought the villager into interaction range");
				return false;
			}

			DriveTowardXZ(xPlayer.m_xPosition, g_xTalkVillagerPosition);
			return true;
		}

		// ------------------------------------------------------------------
		// 5. ARM + PRESS -- EVENT-DRIVEN. Re-poll after the keys were released (the
		//    player is still decelerating) and press only once the live decision
		//    still names the VILLAGER.
		// ------------------------------------------------------------------
		case TalkPhase::ArmAndPress:
		{
			TalkPlayerView xPlayer;
			if (!FindActiveTalkPlayer(xPlayer))
			{
				FailTalk("the player disappeared while arming the interact press");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
			if (eSeam != ZM_INTERACT_OK || xSeamTarget != g_xTalkVillagerEntityID)
			{
				if (g_iTalkPhaseFrames > iARM_DEADLINE_FRAMES)
				{
					FailTalk("the villager never armed as the interaction target after "
						"the approach reported it reachable");
					return false;
				}
				return true;
			}
			if (xRuntime.HasLatchedResult() && xRuntime.GetRaiseCount()
				!= g_uTalkRaiseCountBefore)
			{
				FailTalk("a screen was raised before the arming press");
				return false;
			}

			g_uTalkRaiseCountBefore = xRuntime.GetRaiseCount();
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
			g_eTalkPhase = TalkPhase::AssertRaise;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 6. THE RAISE. Exactly one screen, at the RIGHT entity, actually open --
		//    and the villager's OWN FIRST LINE on it, which is what proves the
		//    CONTENT reached the screen rather than merely that something opened.
		// ------------------------------------------------------------------
		case TalkPhase::AssertRaise:
		{
			TalkPlayerView xPlayer;
			if (!FindActiveTalkPlayer(xPlayer))
			{
				FailTalk("the player disappeared before the raise could be asserted");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			if (xRuntime.GetRaiseCount() != g_uTalkRaiseCountBefore + 1u)
			{
				FailTalk("the walk-up press did not raise EXACTLY one screen");
				return false;
			}
			if (xRuntime.GetLastResult() != ZM_INTERACT_OK)
			{
				FailTalk("the walk-up press did not latch ZM_INTERACT_OK");
				return false;
			}
			if (xRuntime.GetLastTarget() != g_xTalkVillagerEntityID)
			{
				FailTalk("the raised interaction named an entity that is not the villager");
				return false;
			}
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				FailTalk("the raised dialogue did not open the menu stack");
				return false;
			}
			g_bTalkRaisePassed = true;

			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailTalk("the ZM_MenuRoot singleton vanished between Setup and the raise");
				return false;
			}
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailTalk("a TALKER NPC raised something other than the DIALOGUE screen");
				return false;
			}
			// The CONTENT assertion: the villager's own row reached the screen. An
			// NPC wired to the wrong row, or a dialogue box raised with someone else's
			// lines, satisfies every check above and fails only here.
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_VILLAGER);
			if (xRow.m_uLineCount == 0u || xRow.m_paszLines == nullptr
				|| std::strcmp(pxMenu->GetDialogue().GetCurrentLine().c_str(),
					xRow.m_paszLines[0]) != 0)
			{
				FailTalk("the open dialogue is not showing the villager row's FIRST line");
				return false;
			}
			g_bTalkContentPassed = true;

			g_uTalkRaiseCountBefore = xRuntime.GetRaiseCount();
			g_eTalkPhase = TalkPhase::ReRaisePress;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 7. RE-RAISE NEGATIVE. With the dialogue up, E must do NOTHING: this is
		//    the ZM_ShouldInteract menu-open blocker, and it is what stops a
		//    re-raise loop that would re-open the box every frame the key is held.
		// ------------------------------------------------------------------
		case TalkPhase::ReRaisePress:
		{
			TalkPlayerView xPlayer;
			if (!FindActiveTalkPlayer(xPlayer))
			{
				FailTalk("the player disappeared before the re-raise negative");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			// The seam is a FREE evaluation, so it answers even though the frozen
			// player's OnUpdate no longer ticks the runtime at all. MENU_OPEN is rule 2
			// of the fixed blocker precedence and therefore outranks PLAYER_FROZEN.
			if (xRuntime.EvaluateForTests(xSeamTarget) != ZM_INTERACT_REJECT_MENU_OPEN)
			{
				FailTalk("with the dialogue open the seam did not report MENU_OPEN");
				return false;
			}
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailTalk("the ZM_MenuRoot singleton vanished before the re-raise negative");
				return false;
			}
			// The PRESS-SENSITIVE baseline (see ReRaiseAssert): where the conversation's
			// read cursor is, and what it is showing, on the frame BEFORE E goes down.
			g_uTalkReRaiseLineIdxBefore = pxMenu->GetDialogue().GetCurrentLineIndex();
			g_strTalkReRaiseLineBefore = pxMenu->GetDialogue().GetCurrentLine();
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
			g_eTalkPhase = TalkPhase::ReRaiseAssert;
			g_iTalkPhaseFrames = 0;
			return true;
		}

		case TalkPhase::ReRaiseAssert:
		{
			TalkPlayerView xPlayer;
			if (!FindActiveTalkPlayer(xPlayer))
			{
				FailTalk("the player disappeared during the re-raise negative");
				return false;
			}
			const ZM_InteractionRuntime& xRuntime =
				xPlayer.m_pxController->GetInteractionRuntime();
			// ★ HONEST ACCOUNTING OF WHAT THIS PHASE PROVES. The blocker-PRECEDENCE check
			// is the EvaluateForTests(...) == MENU_OPEN assertion on the PRESS frame
			// above: that is the one that fails if ZM_ShouldInteract's bMenuOpen rule
			// is deleted (the seam would then answer PLAYER_FROZEN instead). It is NOT a
			// proof that the press landed: the seam hard-codes bInteractPressed = true, so
			// it answers MENU_OPEN whether or not E was ever pressed.
			// The raise-count check below is a re-raise-loop / self-closing-dialogue
			// guard, NOT a proof of the menu-open blocker: raising the dialogue
			// freezes the player, and OnUpdate early-outs on !m_bMovementEnabled
			// strictly BEFORE the interaction tick, so with a box open the runtime
			// cannot tick at all and this count is structurally incapable of moving.
			// THE PRESS PROOF is the line-index / line-text pair: it is the only thing
			// here the live E edge can move, and it reds the moment E enters
			// ZM_CONFIRM_KEYS or the dialogue box starts consuming the interact key.
			// Keep all three, but do not mistake any one of them for the others.
			if (xRuntime.GetRaiseCount() != g_uTalkRaiseCountBefore)
			{
				FailTalk("pressing E with the dialogue already open raised ANOTHER screen");
				return false;
			}
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				FailTalk("the dialogue closed itself during the re-raise negative");
				return false;
			}
			ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
			if (pxMenu == nullptr)
			{
				FailTalk("the ZM_MenuRoot singleton vanished during the re-raise negative");
				return false;
			}
			g_uTalkReRaiseLineIdxAfter = pxMenu->GetDialogue().GetCurrentLineIndex();
			g_strTalkReRaiseLineAfter = pxMenu->GetDialogue().GetCurrentLine();
			if (g_uTalkReRaiseLineIdxAfter != g_uTalkReRaiseLineIdxBefore
				|| g_strTalkReRaiseLineAfter != g_strTalkReRaiseLineBefore)
			{
				FailTalk("pressing E with the dialogue already open ADVANCED it -- the "
					"interact key is being consumed as a CONFIRM (the line index and text "
					"either side of the press are logged below)");
				return false;
			}
			g_bTalkReRaiseLinePassed = true;
			// Single-frame phase, same as the out-of-range assert: no reachable timeout.
			g_bTalkReRaisePassed = true;
			g_eTalkPhase = TalkPhase::Done;
			return false;
		}

		case TalkPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_NpcTalk()
	{
		ClearTalkInput();
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		if (g_bTalkSkipped)
		{
			// The harness already recorded the skip + its reason in Setup.
			return true;
		}

		const bool bPassed = g_bTalkBootPassed
			&& g_bTalkBasisPassed
			&& g_bTalkNegativePassed
			&& g_bTalkApproachPassed
			&& g_bTalkRaisePassed
			&& g_bTalkContentPassed
			&& g_bTalkReRaisePassed
			&& g_bTalkReRaiseLinePassed
			// The motion must have been PHYSICS-DRIVEN. This flag defaults false and is
			// only ever set from a live body velocity + run speed + real displacement,
			// so a walk faked by any means at all leaves it false. (SetPosition appears
			// NOWHERE in this file -- project rule: no teleportation for movement, even
			// in tests.)
			&& g_bTalkPhysicsMotionSeen;

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_NpcTalk] %s", g_szTalkFailure);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcTalk] boot flags: scene=%d player=%d body=%d villagerArmed=%d",
				(int)g_bTalkSawScene, (int)g_bTalkSawPlayer,
				(int)g_bTalkSawPlayerBody, (int)g_bTalkSawVillager);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcTalk] basis probe: dx=%.3f dz=%.3f (dz must exceed %.3f and dominate)",
				g_fTalkBasisDeltaX, g_fTalkBasisDeltaZ, fBASIS_MIN_FORWARD);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcTalk] approach: distance=%.3f best=%.3f stallFrames=%d "
				"held W=%d A=%d S=%d D=%d",
				g_fTalkCurrentDistance, g_fTalkBestDistance, g_iTalkStallFrames,
				(int)g_abTalkHeldKeys[0], (int)g_abTalkHeldKeys[1],
				(int)g_abTalkHeldKeys[2], (int)g_abTalkHeldKeys[3]);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcTalk] out-of-range negative: observed reject = %s (expected OUT_OF_RANGE)",
				g_szTalkNegativeReject);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_NpcTalk] phase flags: boot=%d basis=%d negative=%d approach=%d "
				"raise=%d content=%d reRaise=%d reRaiseLine=%d physicsMotion=%d "
				"| reRaise lineIdx %u->%u line '%s'->'%s' (both must be UNCHANGED)",
				(int)g_bTalkBootPassed, (int)g_bTalkBasisPassed,
				(int)g_bTalkNegativePassed, (int)g_bTalkApproachPassed,
				(int)g_bTalkRaisePassed, (int)g_bTalkContentPassed,
				(int)g_bTalkReRaisePassed, (int)g_bTalkReRaiseLinePassed,
				(int)g_bTalkPhysicsMotionSeen,
				g_uTalkReRaiseLineIdxBefore, g_uTalkReRaiseLineIdxAfter,
				g_strTalkReRaiseLineBefore.c_str(), g_strTalkReRaiseLineAfter.c_str());
		}
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMNpcTalkTest = {
	"ZM_NpcTalk_Test",
	&Setup_NpcTalk,
	&Step_NpcTalk,
	&Verify_NpcTalk,
	// Every waiting phase owns a deadline that FAILS with a diagnostic; this cap is
	// only a backstop well above their sum (600 + 30 + 900 + 180 + a handful of
	// single-frame asserts), so hitting it means a phase-machine bug rather than a
	// gameplay timeout.
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMNpcTalkTest);

// ============================================================================
// KNOWN-LIMIT W5 -- ZM_DawnmereNpcGroundTruth_Test.
//
// THE ORACLE for the per-NPC feet heights frozen in
// Source/World/ZM_DawnmerePlacement.h. It loads the COMMITTED Dawnmere, and for
// each of the six authored NPCs casts a REAL downward ray at that NPC's anchor XZ
// against the baked terrain body. Two independent things then get checked, and
// keeping them separate is what makes the failure legible:
//   * the COMPILED CONSTANT vs the TERRAIN  (|measured - table| <= tolerance);
//   * the COMMITTED BYTES vs the TERRAIN
//     (|entity.y - (measured + halfExtent + the authored clearance)|).
// Move the constants without re-authoring and the second reds; regenerate the
// heightmap without re-measuring and the first reds. Neither can hide the other.
//
// ★ THE CLEARANCE TERM IS NOT A FUDGE FACTOR (ZM-D-184). Dawnmere's two DYNAMIC
// humans -- the wanderer and rival Vesper -- are deliberately authored one capsule
// half-extent ABOVE their resting centre, so gravity settles them onto the surface
// from the FRONT side rather than leaving them depending on the solver catching them
// on the very first tick. An assertion that demanded exact contact from them would
// be asserting the defect ZM-D-184 fixed, and it did: Vesper reded at 0.913 m for a
// full day after that commit re-authored him +0.9 m. GTAuthoredClearance reads the
// gap back out of the placement accessors the scene was authored with, so it can
// never be restated wrongly here.
//
// ★ IT LOGS EVERY MEASURED HEIGHT AT INFO ON EVERY RUN, PASS OR FAIL. That log is
// the WORKFLOW: run this test, read the `name=... measured=...` pairs, paste them
// into the W5 block, rebuild, re-author Dawnmere from a windowed tools boot. A
// version that logged only on failure would make re-measuring impossible the
// moment it went green.
//
// ★ THE SPREAD CLAUSE IS WHAT MAKES THE WHOLE ITEM NON-VACUOUS, and it is measured
// off the LIVE heightfield rather than off the table: max(measured) - min(measured)
// must be a real distance. That is the proof, from the terrain itself, that
// Dawnmere is NOT flat under the roster -- i.e. that one shared town-centre height
// really was an approximation. If this clause cannot pass, W5's premise is wrong
// and we want to know that rather than ship six copies of one number.
//
// WHY THIS FILE. It already guards on the baked Dawnmere terrain assets and loads
// the real committed scene, so the skip path and the scene plumbing are shared and
// proven. It is deliberately NOT in ZM_AutoTests_Npc.cpp, which loads
// ADDITIVE_WITHOUT_LOADING onto an empty scene to stay CI-honest and therefore has
// no terrain to measure, and deliberately NOT in ZM_AutoTests_RivalVesper.cpp,
// which is about ONE npc.
//
// m_bRequiresGraphics = false: nothing here reads a pixel. Physics is live on the
// Null backend (Zenith_Physics::HasActiveSimulation() is `m_pxPhysicsSystem !=
// nullptr` and Physics().Initialise() is unconditional), so the rays are real
// headless. The ONLY skip is "the terrain bake does not exist".
// ============================================================================

namespace
{
	// The ray starts this far ABOVE the compiled feet height and travels down. Two
	// metres of headroom clears any plausible drift between a stale constant and the
	// live surface while still starting well below the NPC's own head, and six
	// metres of travel reaches ground that has dropped a full storey.
	constexpr float fGT_RAY_START_HEIGHT = 2.0f;
	constexpr float fGT_RAY_MAX_DISTANCE = 6.0f;

	// How far a compiled row may sit from the surface the capsule actually rests on.
	// 0.15 m is well under the 2.0 m band the interact picker and the trainer cone
	// both police, so a row can drift measurably without an NPC going mute -- this
	// reds first, while the failure is still cheap.
	constexpr float fGT_HEIGHT_TOLERANCE = 0.15f;

	// The live-terrain spread that proves Dawnmere is not flat under the roster.
	// Same figure the pure boot unit uses against the compiled table, on purpose:
	// one of them is a claim about constants, this one is a claim about the world.
	constexpr float fGT_MIN_TERRAIN_SPREAD = 0.05f;

	constexpr int iGT_RESOLVE_DEADLINE_FRAMES = 900;
	constexpr int iGT_MEASURE_DEADLINE_FRAMES = 900;

	constexpr u_int uGT_NPC_COUNT = (u_int)ZM_DAWNMERE_NPC_COUNT;
	// Slots for the patrol endpoints. The table is authored with two; the loops
	// clamp to ZM_GetDawnmereWanderWaypointCount() so a third would be measured
	// rather than silently ignored, up to this cap.
	constexpr u_int uGT_WAYPOINT_SLOTS = 4u;

	enum class GroundTruthPhase
	{
		Resolve,   // find all six authored bodies in the committed bytes
		Measure,   // cast the ground rays until every one of them lands
		Done,
	};

	GroundTruthPhase g_eGTPhase       = GroundTruthPhase::Done;
	int              g_iGTPhaseFrames = 0;

	// ---- Phase flags. ALL DEFAULT TO FAILING. ----
	bool        g_bGTPrereqsPresent = false;
	bool        g_bGTSkipped        = false;
	bool        g_bGTAllResolved    = false;
	bool        g_bGTAllMeasured    = false;
	const char* g_szGTFailure       = "test did not reach verification";

	// ---- Per-NPC samples. Every numeric sentinel FAILS CLOSED: a negative
	// half-extent can never satisfy the centre clause, and a measured height of
	// -1e9 can never sit inside the tolerance of any authored row. ----
	bool                  g_abGTResolved[uGT_NPC_COUNT]         = {};
	Zenith_EntityID       g_axGTEntityID[uGT_NPC_COUNT];
	Zenith_Maths::Vector3 g_axGTAuthoredPosition[uGT_NPC_COUNT];
	float                 g_afGTHalfExtent[uGT_NPC_COUNT]       = {};
	int                   g_aiGTResolveFrame[uGT_NPC_COUNT]     = {};
	bool                  g_abGTRayHit[uGT_NPC_COUNT]           = {};
	float                 g_afGTMeasuredFeetY[uGT_NPC_COUNT]    = {};
	Zenith_EntityID       g_axGTHitEntity[uGT_NPC_COUNT];

	// ---- Patrol-endpoint samples, so the wander waypoints' feet heights can be
	// re-measured from the same run rather than guessed. ----
	bool  g_abGTWaypointHit[uGT_WAYPOINT_SLOTS]      = {};
	float g_afGTWaypointMeasured[uGT_WAYPOINT_SLOTS] = {};

	u_int GTWaypointSampleCount()
	{
		const u_int uCount = ZM_GetDawnmereWanderWaypointCount();
		return (uCount < uGT_WAYPOINT_SLOTS) ? uCount : uGT_WAYPOINT_SLOTS;
	}

	void FailGT(const char* szReason)
	{
		g_szGTFailure = szReason;
		g_eGTPhase = GroundTruthPhase::Done;
	}

	// ONE downward cast at an authored XZ. The ignore filter is MANDATORY: every
	// stationary NPC owns a solid STATIC AABB centred on this exact column, so an
	// unfiltered ray measures the top of that box and reports the NPC as standing
	// one body-height above itself -- a self-confirming measurement.
	Zenith_Physics::RaycastResult GTCastGroundRay(
		float fX, float fZ, float fStartFeetY, Zenith_EntityID xIgnoreEntity)
	{
		const Zenith_Maths::Vector3 xOrigin(
			fX, fStartFeetY + fGT_RAY_START_HEIGHT, fZ);
		return Zenith_PhysicsQuery::RaycastIgnoring(
			xOrigin, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
			fGT_RAY_MAX_DISTANCE, xIgnoreEntity);
	}

	// The authored AIR GAP above uNpc's resting centre: zero for a body authored in
	// contact with the ground, one capsule half-extent for a DYNAMIC one.
	//
	// ★ DERIVED FROM THE PLACEMENT CODE, NEVER RESTATED HERE (ZM-D-184). Each dynamic
	// row subtracts that NPC's resting centre from the very accessor Zenithmon.cpp
	// authors it with, so the clearance this test expects cannot drift from the
	// clearance the scene was written with -- re-derive either accessor and this moves
	// with it, with no number to update in this file.
	//
	// ★ AND THE SUBTRACTION CANCELS THE COMPILED TABLE. Both terms read the same W5
	// feet height, so what comes back is the pure air gap and nothing else. That is
	// what keeps clause (b) below a claim about the BYTES vs THE TERRAIN rather than a
	// claim about the bytes vs the table -- the separation the two clauses exist for.
	//
	// A row not named here is treated as authored in contact. Give a future NPC a
	// dynamic body without adding it and clause (b) reds by the whole clearance and
	// names the entity, which is the failure we want rather than a silent pass.
	float GTAuthoredClearance(u_int uNpc, float fHalfExtent)
	{
		switch (uNpc)
		{
		case (u_int)ZM_DAWNMERE_NPC_WANDERER:
			return ZM_DawnmereWandererSpawnY(fHalfExtent)
				- ZM_DawnmereNpcFeetY(ZM_DAWNMERE_NPC_WANDERER);
		case (u_int)ZM_DAWNMERE_NPC_RIVAL_VESPER:
			return ZM_DawnmereTrainerSpawnY(fHalfExtent)
				- ZM_DawnmereNpcFeetY(ZM_DAWNMERE_NPC_RIVAL_VESPER);
		default:
			return 0.0f;
		}
	}

	// (1) Resolve every authored NPC out of the COMMITTED bytes, capturing each one
	//     the FIRST frame it appears and never again. The re-capture guard is not
	//     cosmetic: the wanderer is a DYNAMIC capsule and gravity starts moving it
	//     immediately, so a sample taken later would measure the fall rather than
	//     the authoring.
	bool GTStepResolve()
	{
		const bool bScene = g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
				== iTALK_OVERWORLD_BUILD_INDEX;
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		if (!bScene || pxData == nullptr)
		{
			if (g_iGTPhaseFrames > iGT_RESOLVE_DEADLINE_FRAMES)
			{
				FailGT("Dawnmere never became the active scene");
				return false;
			}
			return true;
		}

		u_int uResolved = 0u;
		for (u_int u = 0u; u < uGT_NPC_COUNT; ++u)
		{
			if (g_abGTResolved[u])
			{
				++uResolved;
				continue;
			}
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
			Zenith_Entity xEntity = pxData->FindEntityByName(xAnchor.m_szEntityName);
			if (!xEntity.IsValid())
			{
				continue;
			}
			Zenith_TransformComponent* pxTransform =
				xEntity.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				continue;
			}
			Zenith_Maths::Vector3 xPosition(0.0f);
			Zenith_Maths::Vector3 xScale(1.0f);
			pxTransform->GetPosition(xPosition);
			pxTransform->GetScale(xScale);
			g_axGTEntityID[u]         = xEntity.GetEntityID();
			g_axGTAuthoredPosition[u] = xPosition;
			// THE BODY CONTRACT, never the transform scale: every human wears the
			// same compiled body regardless of how large its model is drawn.
			g_afGTHalfExtent[u] = fZM_HUMAN_BODY_HALF_HEIGHT;
			g_aiGTResolveFrame[u]     = g_iGTPhaseFrames;
			g_abGTResolved[u]         = true;
			++uResolved;
		}

		if (uResolved != uGT_NPC_COUNT)
		{
			if (g_iGTPhaseFrames > iGT_RESOLVE_DEADLINE_FRAMES)
			{
				FailGT("an authored Dawnmere NPC never resolved out of the committed "
					"scene bytes -- the per-NPC resolved flags are logged below (a "
					"false one means that entity name is not in Dawnmere.zscen)");
				return false;
			}
			return true;
		}

		g_bGTAllResolved = true;
		g_eGTPhase = GroundTruthPhase::Measure;
		g_iGTPhaseFrames = 0;
		return true;
	}

	// (2) Measure. The rays are RETRIED every frame until they all land, because the
	//     terrain physics body arrives with streaming rather than with the scene --
	//     "no hit yet" is a wait, and only the deadline turns it into a failure.
	bool GTStepMeasure()
	{
		u_int uHits = 0u;
		for (u_int u = 0u; u < uGT_NPC_COUNT; ++u)
		{
			if (g_abGTRayHit[u])
			{
				++uHits;
				continue;
			}
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
			const Zenith_Physics::RaycastResult xHit = GTCastGroundRay(
				xAnchor.m_fX, xAnchor.m_fZ, xAnchor.m_fFeetY, g_axGTEntityID[u]);
			if (!xHit.m_bHit)
			{
				continue;
			}
			g_abGTRayHit[u]        = true;
			g_afGTMeasuredFeetY[u] = xHit.m_xHitPoint.y;
			g_axGTHitEntity[u]     = xHit.m_xHitEntity;
			++uHits;
		}

		const u_int uWaypointCount = GTWaypointSampleCount();
		u_int uWaypointHits = 0u;
		for (u_int u = 0u; u < uWaypointCount; ++u)
		{
			if (g_abGTWaypointHit[u])
			{
				++uWaypointHits;
				continue;
			}
			const ZM_DawnmereNpcAnchor& xWaypoint = ZM_GetDawnmereWanderWaypoint(u);
			// The wanderer is ignored at BOTH endpoints: it is a live patrolling
			// capsule and may be standing on either one when the ray goes out.
			const Zenith_Physics::RaycastResult xHit = GTCastGroundRay(
				xWaypoint.m_fX, xWaypoint.m_fZ, xWaypoint.m_fFeetY,
				g_axGTEntityID[ZM_DAWNMERE_NPC_WANDERER]);
			if (!xHit.m_bHit)
			{
				continue;
			}
			g_abGTWaypointHit[u]      = true;
			g_afGTWaypointMeasured[u] = xHit.m_xHitPoint.y;
			++uWaypointHits;
		}

		if (uHits != uGT_NPC_COUNT || uWaypointHits != uWaypointCount)
		{
			if (g_iGTPhaseFrames > iGT_MEASURE_DEADLINE_FRAMES)
			{
				FailGT("a downward probe ray never found ground under an authored "
					"anchor -- either the terrain physics body never streamed in, or a "
					"compiled feet height is now more than the probe's reach away from "
					"the real surface (the per-NPC hit flags are logged below)");
				return false;
			}
			return true;
		}

		g_bGTAllMeasured = true;
		g_eGTPhase = GroundTruthPhase::Done;
		return false;
	}

	void Setup_DawnmereNpcGroundTruth()
	{
		g_eGTPhase          = GroundTruthPhase::Done;
		g_iGTPhaseFrames    = 0;
		g_bGTPrereqsPresent = false;
		g_bGTSkipped        = false;
		g_bGTAllResolved    = false;
		g_bGTAllMeasured    = false;
		g_szGTFailure       = "test did not reach verification";

		for (u_int u = 0u; u < uGT_NPC_COUNT; ++u)
		{
			g_abGTResolved[u]         = false;
			g_axGTEntityID[u]         = INVALID_ENTITY_ID;
			g_axGTAuthoredPosition[u] = Zenith_Maths::Vector3(0.0f);
			g_afGTHalfExtent[u]       = -1.0f;        // fails closed
			g_aiGTResolveFrame[u]     = -1;
			g_abGTRayHit[u]           = false;
			g_afGTMeasuredFeetY[u]    = -1.0e9f;      // fails closed
			g_axGTHitEntity[u]        = INVALID_ENTITY_ID;
		}
		for (u_int u = 0u; u < uGT_WAYPOINT_SLOTS; ++u)
		{
			g_abGTWaypointHit[u]      = false;
			g_afGTWaypointMeasured[u] = -1.0e9f;      // fails closed
		}

		Zenith_InputSimulator::ResetAllInputState();

		// THE ONLY SKIP, and it is deliberately narrow: "the heightfield this test
		// measures against does not exist". A MISSING NPC, a MOVED NPC and a STALE
		// constant all FAIL below -- none of them skip. RequestSkip bypasses Verify,
		// so no fixed-dt or scene-load state may be installed before this point.
		g_bGTPrereqsPresent = RequiredTalkAssetsPresent();
		if (!g_bGTPrereqsPresent)
		{
			g_bGTSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_DawnmereNpcGroundTruth] the Dawnmere scene / terrain bake is absent "
				"or incomplete -- there is no heightfield to measure the authored NPC "
				"feet heights against (run a *_True config once to bake it)");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fTALK_FIXED_DT);
		g_eGTPhase = GroundTruthPhase::Resolve;
		g_xEngine.Scenes().LoadSceneByIndex(
			iTALK_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_DawnmereNpcGroundTruth(int)
	{
		if (g_eGTPhase == GroundTruthPhase::Done)
		{
			return false;
		}
		++g_iGTPhaseFrames;

		switch (g_eGTPhase)
		{
		case GroundTruthPhase::Resolve: return GTStepResolve();
		case GroundTruthPhase::Measure: return GTStepMeasure();
		case GroundTruthPhase::Done:    return false;
		}
		return false;
	}

	bool Verify_DawnmereNpcGroundTruth()
	{
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();

		if (g_bGTSkipped)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereNpcGroundTruth] SKIPPED -- no baked Dawnmere terrain, so "
				"nothing was measured. The W5 feet heights in "
				"Source/World/ZM_DawnmerePlacement.h are UNVERIFIED on this run.");
			// The harness already recorded the skip + its reason in Setup.
			return true;
		}

		// ★ THE PASTE-READY LOG, EMITTED ON EVERY RUN, PASS OR FAIL. This is how the
		// W5 constants are (re-)obtained; see the block comment at the top.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereNpcGroundTruth] MEASURED FEET Y (paste into the W5 block in "
			"Source/World/ZM_DawnmerePlacement.h): name=%s measured=%.5f | "
			"name=%s measured=%.5f | name=%s measured=%.5f",
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_VILLAGER).m_szEntityName,
			g_afGTMeasuredFeetY[ZM_DAWNMERE_NPC_VILLAGER],
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_TRADE_POST_CLERK).m_szEntityName,
			g_afGTMeasuredFeetY[ZM_DAWNMERE_NPC_TRADE_POST_CLERK],
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_CARETAKER).m_szEntityName,
			g_afGTMeasuredFeetY[ZM_DAWNMERE_NPC_CARETAKER]);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereNpcGroundTruth] MEASURED FEET Y (paste into the W5 block in "
			"Source/World/ZM_DawnmerePlacement.h): name=%s measured=%.5f | "
			"name=%s measured=%.5f | name=%s measured=%.5f | name=%s measured=%.5f",
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WARDEN).m_szEntityName,
			g_afGTMeasuredFeetY[ZM_DAWNMERE_NPC_WARDEN],
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WANDERER).m_szEntityName,
			g_afGTMeasuredFeetY[ZM_DAWNMERE_NPC_WANDERER],
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_RIVAL_VESPER).m_szEntityName,
			g_afGTMeasuredFeetY[ZM_DAWNMERE_NPC_RIVAL_VESPER],
			ZM_GetDawnmereWanderWaypoint(1u).m_szEntityName,
			g_afGTWaypointMeasured[1]);

		// Per-anchor detail, also on every run: what the table says, what the ground
		// says, and what the committed transform says, in one line each.
		//
		// expectedCentre CARRIES THE CLEARANCE, so this line reports exactly what
		// clause (b) asserts. It did not before ZM-D-184, and the two dynamic rows
		// consequently logged a ~0.9 m centreError that looked like a failure on a run
		// where only one of them actually failed -- a diagnostic that costs a reader a
		// debugging session is worse than no diagnostic.
		for (u_int u = 0u; u < uGT_NPC_COUNT; ++u)
		{
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
			const float fLoggedClearance =
				GTAuthoredClearance(u, g_afGTHalfExtent[u]);
			// ★ NO HALF-EXTENT TERM. An authored human position IS its feet, so the
			// expected value is the measured ground plus the authored air gap. The
			// half-extent used to be here because the authored value was a body
			// CENTRE; leaving it in would demand every static NPC hover 0.9 m up.
			const float fLoggedExpected =
				g_afGTMeasuredFeetY[u] + fLoggedClearance;
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereNpcGroundTruth] name=%s xz=(%.1f, %.1f) measured=%.5f "
				"table=%.5f tableError=%.5f | authoredY=%.5f expectedFeet=%.5f "
				"centreError=%.5f halfExtent=%.4f clearance=%.4f | resolved=%d "
				"rayHit=%d resolveFrame=%d",
				xAnchor.m_szEntityName, xAnchor.m_fX, xAnchor.m_fZ,
				g_afGTMeasuredFeetY[u], xAnchor.m_fFeetY,
				g_afGTMeasuredFeetY[u] - xAnchor.m_fFeetY,
				g_axGTAuthoredPosition[u].y, fLoggedExpected,
				g_axGTAuthoredPosition[u].y - fLoggedExpected,
				g_afGTHalfExtent[u], fLoggedClearance, (int)g_abGTResolved[u],
				(int)g_abGTRayHit[u], g_aiGTResolveFrame[u]);
		}

		bool bPassed = g_bGTPrereqsPresent && g_bGTAllResolved && g_bGTAllMeasured;
		if (!bPassed)
		{
			// The phases did not complete, so every measurement below would be a
			// fail-closed sentinel. Report WHERE it stopped and stop -- a wall of
			// "-1000000000 is 1e9 m off the surface" errors would bury the one line
			// that says the terrain body never streamed in.
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereNpcGroundTruth] %s (prereqs=%d allResolved=%d "
				"allMeasured=%d)",
				g_szGTFailure, (int)g_bGTPrereqsPresent, (int)g_bGTAllResolved,
				(int)g_bGTAllMeasured);
			return false;
		}

		// ---- (a) THE COMPILED CONSTANT vs THE TERRAIN, per NPC ----
		float fMinMeasured = 0.0f;
		float fMaxMeasured = 0.0f;
		for (u_int u = 0u; u < uGT_NPC_COUNT; ++u)
		{
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
			if (u == 0u || g_afGTMeasuredFeetY[u] < fMinMeasured)
			{
				fMinMeasured = g_afGTMeasuredFeetY[u];
			}
			if (u == 0u || g_afGTMeasuredFeetY[u] > fMaxMeasured)
			{
				fMaxMeasured = g_afGTMeasuredFeetY[u];
			}

			// The ray must have measured GROUND. Any of the six authored bodies
			// answering would be a self-confirming measurement, so name it as a
			// failure rather than trusting the ignore filter to have been enough.
			for (u_int uOther = 0u; uOther < uGT_NPC_COUNT; ++uOther)
			{
				if (g_axGTHitEntity[u] != INVALID_ENTITY_ID
					&& g_axGTHitEntity[u] == g_axGTEntityID[uOther])
				{
					bPassed = false;
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DawnmereNpcGroundTruth] the probe under '%s' terminated on "
						"authored NPC '%s' rather than on the terrain -- the measurement "
						"is a body top, not a ground height",
						xAnchor.m_szEntityName,
						ZM_GetDawnmereNpcAnchor(uOther).m_szEntityName);
				}
			}

			const float fTableError =
				std::fabs(g_afGTMeasuredFeetY[u] - xAnchor.m_fFeetY);
			if (fTableError > fGT_HEIGHT_TOLERANCE)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereNpcGroundTruth] '%s': the compiled feet height %.5f is "
					"%.5f m off the terrain surface %.5f (tolerance %.3f) -- re-measure "
					"the W5 block in Source/World/ZM_DawnmerePlacement.h",
					xAnchor.m_szEntityName, xAnchor.m_fFeetY, fTableError,
					g_afGTMeasuredFeetY[u], fGT_HEIGHT_TOLERANCE);
			}

			// ---- (b) THE COMMITTED BYTES vs THE TERRAIN ----
			// ALL SIX ROWS, static and dynamic alike, against ONE rule: the committed
			// transform is the measured surface, plus a capsule half-extent, plus that
			// NPC's authored clearance (zero for the four in contact with the ground).
			//
			// ★ WHY THE TWO DYNAMIC ROWS CAN BE HELD TO AN EXACT VALUE AT ALL, given
			// that gravity starts closing their gap the moment the scene loads. The
			// transform is captured on the FIRST frame the entity resolves, and the
			// collider rebuild that gives it a body does not happen until frame 2 (the
			// ZM-D-184 capture measured exactly that: `COLLIDER REBUILT ...
			// framesSinceLoad=2`). So what this compares is the DESERIALIZED authored
			// value, before a physics body exists to move it -- which is why the
			// re-capture guard in GTStepResolve is load-bearing rather than cosmetic,
			// and observed drift on both dynamic rows is 0.00000 m.
			//
			// The wanderer used to be excluded here and hold a separate ~1.2 m-wide
			// band instead. That band could not have caught a missing re-author, and
			// it is strictly contained in this window, so it is gone rather than kept
			// beside a stronger statement of the same fact.
			const float fClearance = GTAuthoredClearance(u, g_afGTHalfExtent[u]);
			const float fExpectedCentre = g_afGTMeasuredFeetY[u] + fClearance;
			const float fCentreError =
				std::fabs(g_axGTAuthoredPosition[u].y - fExpectedCentre);
			if (fCentreError > fGT_HEIGHT_TOLERANCE)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereNpcGroundTruth] '%s': the COMMITTED transform Y %.5f is "
					"%.5f m off terrain + clearance %.5f (halfExtent %.4f, "
					"clearance %.4f, tolerance %.3f) -- either Dawnmere has not been "
					"re-authored since the W5 heights moved, or this body's authored "
					"clearance changed and GTAuthoredClearance was not told. A committed "
					"Y BELOW the expected value on a dynamic row is the ZM-D-184 hazard "
					"itself: a capsule authored into the terrain mesh, which gravity "
					"cannot settle from the front side.",
					xAnchor.m_szEntityName, g_axGTAuthoredPosition[u].y, fCentreError,
					fExpectedCentre, g_afGTHalfExtent[u], fClearance,
					fGT_HEIGHT_TOLERANCE);
			}
		}

		// ---- (c) THE PATROL ENDPOINTS vs THE TERRAIN ----
		const u_int uWaypointCount = GTWaypointSampleCount();
		for (u_int u = 0u; u < uWaypointCount; ++u)
		{
			const ZM_DawnmereNpcAnchor& xWaypoint = ZM_GetDawnmereWanderWaypoint(u);
			const float fError =
				std::fabs(g_afGTWaypointMeasured[u] - xWaypoint.m_fFeetY);
			if (!g_abGTWaypointHit[u] || fError > fGT_HEIGHT_TOLERANCE)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereNpcGroundTruth] '%s': compiled feet %.5f vs terrain "
					"%.5f (error %.5f, hit=%d, tolerance %.3f)",
					xWaypoint.m_szEntityName, xWaypoint.m_fFeetY,
					g_afGTWaypointMeasured[u], fError, (int)g_abGTWaypointHit[u],
					fGT_HEIGHT_TOLERANCE);
			}
		}

		// ---- (d) THE TWO-SIDED CLAUSE THAT MAKES W5 NON-VACUOUS ----
		// Measured off the LIVE heightfield, never off the table: this is the terrain
		// itself saying that six independent heights were warranted.
		const float fMeasuredSpread = fMaxMeasured - fMinMeasured;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereNpcGroundTruth] live terrain spread under the roster = %.5f m "
			"(min %.5f, max %.5f; must be >= %.3f)",
			fMeasuredSpread, fMinMeasured, fMaxMeasured, fGT_MIN_TERRAIN_SPREAD);
		if (fMeasuredSpread < fGT_MIN_TERRAIN_SPREAD)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereNpcGroundTruth] the live terrain under the six authored "
				"NPCs is FLAT to within %.5f m -- W5's whole premise (that one shared "
				"town-centre height was an approximation) does not hold on this "
				"heightmap, and six measured constants buy nothing",
				fMeasuredSpread);
		}

		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMDawnmereNpcGroundTruthTest = {
	"ZM_DawnmereNpcGroundTruth_Test",
	&Setup_DawnmereNpcGroundTruth,
	&Step_DawnmereNpcGroundTruth,
	&Verify_DawnmereNpcGroundTruth,
	// Both waiting phases own a deadline that FAILS with a diagnostic; this cap is
	// only a backstop above their sum (900 + 900).
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDawnmereNpcGroundTruthTest);

#endif // ZENITH_INPUT_SIMULATOR
