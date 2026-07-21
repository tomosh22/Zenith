#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_SaveResume -- the two WINDOWED proofs for S7 item 2 SC3.
//
//   * ZM_ResumePlacement_Test -- in the real baked Dawnmere, under real
//     simulated input, the player WALKS somewhere that is provably NOT the spawn
//     marker, that pose is CAPTURED into a ZM_GameState, and a resume through the
//     ordinary validated warp puts the player back at THAT pose -- position AND
//     facing -- rather than at the spawn marker the warp would otherwise use.
//     It also proves, from INSIDE the in-flight warp, that a milestone autosave is
//     refused while a save BLOCKER is live even though the capture would have
//     succeeded there -- the one place either test pins the blocker term at the
//     integration boundary. It then proves the capture REFUSES in a playerless
//     scene.
//
//   * ZM_QuitToFrontEnd_Test -- quit-to-title goes through the SAME fade + single
//     load machine and actually FINISHES, on a scene that authors neither a
//     Player nor a ZM_SpawnPoint nor a ZM_FollowCamera.
//
// ---- WHY EACH ASSERTION IS NOT VACUOUS -------------------------------------
// The dominant trap in this whole item is that ZM_GameStateManager is
// DontDestroyOnLoad and the FrontEnd re-author path DESTROYS its duplicate
// rather than reseeding, so anything held in the live ZM_GameState survives a
// quit-to-title entirely in RAM. That is why the thing this file proves restored
// is the PLAYER'S BODY POSE, not a game-state field: the player entity and its
// physics body are destroyed and rebuilt by the scene reload, so the pose
// genuinely goes away and has to be put back.
//
// Three assertions carry the weight, and each one kills a different way of
// faking it:
//   1. distance(final, capturedCentre) is tiny        -> it IS the captured pose
//   2. distance(final, spawnCentre) is LARGE          -> it is NOT the marker
//      placement the warp performs anyway (the ONE assertion separating
//      "restored exactly" from "warped to TownCenter")
//   3. the issued-load count rose by exactly 1        -> a real SINGLE scene load
//      happened, so (1) cannot be satisfied by a player that never went away
// plus the yaw assertion, which reds the moment the rotation write is dropped
// (TeleportBody forces IDENTITY rotation, so a missing SetBodyRotation returns
// yaw 0) or is issued BEFORE the marker teleport that overwrites it.
//
// ---- WHY THIS FILE CARRIES ITS OWN WALK MACHINERY --------------------------
// Games/Zenithmon/Tests is .cpp-only with NO shared header, and every helper in
// ZM_AutoTests_NpcServices.cpp lives in a per-file anonymous namespace. The
// CAMERA-RELATIVE drive (DriveTowardXZ) is copied from
// ZM_AutoTests_NpcServices.cpp:546-604 with its reasoning intact: player movement
// is camera-relative and ZM_FollowCamera's yaw swings as the player turns, so
// choosing W/A/S/D from world-space dx/dz is correct ONLY for a single leg walked
// from rest. The rest of that file's WalkContext machine is an NPC-APPROACH
// machine (it resolves a target entity by name, polls the interaction seam and
// emits an interact edge); this test walks to a COORDINATE and never interacts,
// so copying the interaction stages verbatim would ship dead code. The parts that
// are load-bearing here -- the camera-relative key choice, the stall watchdog and
// AccumulatePhysicsMotionEvidence -- are carried over unchanged.
//
// ---- DESIGN RULES ----------------------------------------------------------
//   * EVERY phase flag DEFAULTS TO FAILING, so a phase that never runs fails.
//   * SetPosition appears NOWHERE. Motion is physics-driven and is ASSERTED to be.
//   * SetFocusedElement appears NOWHERE; focus is never parked programmatically.
//   * Every waiting phase owns its OWN deadline with its OWN diagnostic -- being
//     ended by the harness frame cap says nothing about WHERE the test stalled.
//   * Teardown is UNCONDITIONAL and runs on every exit path, including a
//     mid-phase failure: input, fixed dt, the manager's session state, the slot
//     files the autosave latch may have written, and the boot scene.
//
// FRAME ORDERING both tests depend on: Zenith_AutomatedTestRunner::Tick() (which
// calls Step) runs BEFORE g_xEngine.Scenes().Update() in Zenith_MainLoop, so a
// key edge injected in Step is consumed later in the SAME frame and observable
// from the NEXT Step call.
//
// HEADLESS NOTE: both tests are m_bRequiresGraphics = true, so the headless CI
// batch SKIPS them and a skip counts as a PASS. A green zm-tests proves NOTHING
// about either test here -- they must be run windowed, locally, with
// `--filter ZM_ResumePlacement_Test` / `--filter ZM_QuitToFrontEnd_Test`.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
// ZENITH_SCENE_EXT / ZENITH_TEXTURE_EXT / ZENITH_MESH_EXT, used by the asset guard
// below. Included EXPLICITLY (as the sibling ZM_AutoTests_GameState.cpp does) --
// they were previously reaching this TU only through SaveData/Zenith_SaveData.h's
// transitive chain, which is nobody's contract to keep.
#include "FileAccess/Zenith_FileAccess.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "SaveData/Zenith_SaveData.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Save/ZM_Autosave.h"     // ZM_GetAutosaveCount
#include "Zenithmon/Source/Save/ZM_ResumePoint.h"
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
	// ------------------------------------------------------------------------
	// Shared constants
	// ------------------------------------------------------------------------

	// 1/60, matching the PROVEN walk in ZM_AutoTests_NpcTalk.cpp /
	// ZM_AutoTests_NpcServices.cpp rather than the 1/30 of the UI screen gates --
	// the walk is the physics-coupled risky part and must match it exactly.
	constexpr float fRESUME_FIXED_DT = 1.0f / 60.0f;

	constexpr int iBUILD_INDEX_FRONTEND = 0;
	constexpr int iBUILD_INDEX_DAWNMERE = 2;
	constexpr const char* szDAWNMERE_SPAWN_TAG = "TownCenter";

	// The walk destination, expressed as an OFFSET from the live spawn centre so
	// re-authoring Dawnmere moves the target with it.
	//   +6 X / -9 Z  =>  ~10.8 m from the spawn, which is
	//     * far more than the 3 m the "did you actually walk" gate needs,
	//     * far more than the 2 m the "this is NOT marker placement" gate needs,
	//     * a heading of atan2(6, -9) ~= 2.56 rad, so the yaw round trip is not
	//       vacuous, and
	//     * ~130 m from Dawnmere's only warp sensor, HomeDoorTrigger at
	//       (384, 27.161484, 476) scale (3,2,2). ZM_WarpTrigger resets its overlap
	//       latch in OnStart, so a freshly loaded scene starts UNLATCHED and a pose
	//       restored inside that volume would instantly re-warp.
	constexpr float fWALK_OFFSET_X = 6.0f;
	constexpr float fWALK_OFFSET_Z = -9.0f;
	constexpr float fWALK_ARRIVAL_RADIUS = 0.6f;

	// ---- gates ----
	constexpr float fMIN_WALK_DISTANCE_FROM_SPAWN = 3.0f;   // "you really walked"
	constexpr float fMIN_YAW_MAGNITUDE            = 0.5f;   // "the facing is not ~0"
	constexpr float fMAX_RESTORE_PLANAR_ERROR     = 0.05f;  // "restored EXACTLY" (XZ)
	// The vertical axis gets its own, looser bound: the restored capsule settles
	// against the terrain contact it was resting on, and a couple of centimetres of
	// contact resolution there is correct behaviour, not a placement error. The XZ
	// bound above is the load-bearing one -- every mutation this test is written to
	// catch moves the player METRES in XZ, never millimetres in Y.
	constexpr float fMAX_RESTORE_VERTICAL_ERROR   = 0.10f;
	constexpr float fMIN_RESTORE_DISTANCE_FROM_SPAWN = 2.0f;
	constexpr float fMAX_YAW_ERROR                = 0.05f;
	constexpr float fMAX_CAPTURE_MATCH_ERROR      = 0.01f;

	// ---- physics-motion evidence (copied from ZM_AutoTests_NpcServices.cpp) ----
	constexpr float fPHYSICS_MIN_TRAVEL  = 1.0f;
	constexpr float fPHYSICS_MIN_SPEED   = 1.0f;
	constexpr float fRUN_SPEED_TOLERANCE = 0.001f;
	constexpr float fBASIS_MIN_TRAVEL    = 0.5f;

	// ---- rest detection before the capture ----
	constexpr float fREST_SPEED           = 0.05f;
	constexpr int   iREST_CONSECUTIVE     = 10;

	// ---- walk watchdog ----
	constexpr int   iSTALL_LIMIT_FRAMES = 90;
	constexpr float fSTALL_IMPROVEMENT  = 0.01f;

	// ------------------------------------------------------------------------
	// Asset guards (mirror ZM_AutoTests_GameState.cpp:59-90)
	// ------------------------------------------------------------------------

	bool DiskFilePresent(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	bool RequiredDawnmereAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		// FrontEnd is listed too: both tests END on it (one quits to it, the other
		// loads it to prove the playerless capture refusal), and it is the scene that
		// authors the persistent ZM_GameStateRoot the whole warp machine rides on.
		const std::array<std::string, 8> astrRequired = {
			strRoot + "Scenes/FrontEnd" ZENITH_SCENE_EXT,
			strRoot + "Scenes/Dawnmere" ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			if (!DiskFilePresent(strPath))
			{
				return false;
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------
	// Live resolvers. NOTHING is cached across frames: the component pools
	// relocate on swap-and-pop, and the scene reload rebuilds every entity.
	// ------------------------------------------------------------------------

	int ActiveBuildIndex()
	{
		return g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
	}

	struct PlayerView
	{
		Zenith_EntityID           m_xEntityID    = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3     m_xPosition    = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController*      m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider   = nullptr;
	};

	// The UNIQUE active-scene player. Uniqueness is part of the contract the warp
	// machine itself enforces (FindUniquePlayerInScene), so a second player must
	// fail here rather than silently pick one.
	bool FindActivePlayer(PlayerView& xOut)
	{
		xOut = PlayerView{};
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

	u_int ActiveScenePlayerCount()
	{
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().ForEach(
			[&](Zenith_EntityID, ZM_PlayerController&) { ++uCount; });
		return uCount;
	}

	// The live persistent manager. Null is EXPECTED for a short window around a
	// FrontEnd load: that load re-authors ZM_GameStateRoot, so a DUPLICATE
	// ZM_GameStateManager exists until its OnStart destroys it, and while both are
	// alive TryGetUniqueSingletonEntityID counts 2 and refuses. Every caller below
	// therefore polls rather than failing on the first null.
	ZM_GameStateManager* ResolveManager()
	{
		Zenith_EntityID xManagerID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerID))
		{
			return nullptr;
		}
		Zenith_Entity xManager = g_xEngine.Scenes().ResolveEntity(xManagerID);
		return xManager.IsValid()
			? xManager.TryGetComponent<ZM_GameStateManager>()
			: nullptr;
	}

	// The body CENTRE, read straight from physics. Spawn MARKERS store FEET and
	// ZM_GameStateManager::CalculateSpawnCenter adds the capsule half-extent, so
	// capture, restore and every assertion in this file speak CENTRE throughout --
	// mixing the two conventions is a silent 0.9 m error.
	bool ReadPlayerPose(const PlayerView& xPlayer, Zenith_Maths::Vector3& xCentreOut,
		float& fYawOut)
	{
		if (xPlayer.m_pxCollider == nullptr || !xPlayer.m_pxCollider->HasValidBody())
		{
			return false;
		}
		const Zenith_PhysicsBodyID xBodyID = xPlayer.m_pxCollider->GetBodyID();
		xCentreOut = g_xEngine.Physics().GetBodyPosition(xBodyID);
		// ZM_YawFromRotation, never glm::eulerAngles -- the latter collapses past
		// 90 degrees off +Z (a documented engine trap in this repo).
		fYawOut = ZM_YawFromRotation(g_xEngine.Physics().GetBodyRotation(xBodyID));
		return true;
	}

	float PlanarDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	float WrapAngleDifference(float fA, float fB)
	{
		float fDelta = fA - fB;
		while (fDelta > 3.14159265358979323846f)  { fDelta -= 6.28318530717958647692f; }
		while (fDelta < -3.14159265358979323846f) { fDelta += 6.28318530717958647692f; }
		return fDelta;
	}

	// ------------------------------------------------------------------------
	// The camera-relative drive, copied from ZM_AutoTests_NpcServices.cpp:546-604.
	//
	// Player movement is camera-relative: ZM_PlayerController::OnUpdate reads the
	// main camera's facing dir and hands it to BuildCameraRelativeDirection, which
	// builds xForward = flatten(cameraForward), xRight = (xForward.z, 0, -xForward.x)
	// and moves along xForward * input.y + xRight * input.x. So "W" does NOT mean
	// +Z -- it means "the way the camera is facing", and ZM_FollowCamera's yaw
	// swings as the player turns. Choosing keys from world-space dx/dz is correct
	// only for a SINGLE leg walked from rest; this is the exact INVERSE of the
	// controller's mapping and degenerates to the world-space version at the
	// authored yaw of 0.
	// ------------------------------------------------------------------------

	struct PointWalk
	{
		Zenith_Maths::Vector3 m_xTarget        = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xApproachStart = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xLivePosition  = Zenith_Maths::Vector3(0.0f);
		float m_fBestDistance    = 0.0f;
		float m_fCurrentDistance = 0.0f;
		int   m_iStallFrames     = 0;
		bool  m_abHeldKeys[4]    = { false, false, false, false };   // W A S D
		bool  m_bPhysicsMotionSeen = false;
	};

	void ClearWalkInput(PointWalk& xWalk)
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT_SHIFT, false);
		xWalk.m_abHeldKeys[0] = false;
		xWalk.m_abHeldKeys[1] = false;
		xWalk.m_abHeldKeys[2] = false;
		xWalk.m_abHeldKeys[3] = false;
	}

	void DriveTowardXZ(PointWalk& xWalk, const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		ClearWalkInput(xWalk);
		constexpr float fDEAD_ZONE = 0.08f;

		Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
		{
			pxCamera->GetFacingDir(xCameraForward);
		}
		Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
		const float fForwardLengthSq = xForward.x * xForward.x + xForward.z * xForward.z;
		if (fForwardLengthSq <= 0.000001f)
		{
			xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		}
		else
		{
			xForward /= std::sqrt(fForwardLengthSq);
		}
		const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);

		const Zenith_Maths::Vector3 xToTarget(
			xTarget.x - xPosition.x, 0.0f, xTarget.z - xPosition.z);
		const float fForwardAmount = xToTarget.x * xForward.x + xToTarget.z * xForward.z;
		const float fRightAmount   = xToTarget.x * xRight.x   + xToTarget.z * xRight.z;

		const float fDeltaX = fRightAmount;
		const float fDeltaZ = fForwardAmount;
		if (fDeltaX < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
			xWalk.m_abHeldKeys[1] = true;
		}
		else if (fDeltaX > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
			xWalk.m_abHeldKeys[3] = true;
		}
		if (fDeltaZ < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, true);
			xWalk.m_abHeldKeys[2] = true;
		}
		else if (fDeltaZ > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			xWalk.m_abHeldKeys[0] = true;
		}
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
	}

	// This frame's evidence that the motion is PHYSICS-DRIVEN rather than a
	// teleport: the controller is asking for RUN speed, the BODY carries a real
	// planar velocity, and real ground has been covered since the walk began.
	void AccumulatePhysicsMotionEvidence(PointWalk& xWalk, const PlayerView& xPlayer)
	{
		if (xPlayer.m_pxCollider == nullptr || !xPlayer.m_pxCollider->HasValidBody()
			|| xPlayer.m_pxController == nullptr)
		{
			return;
		}
		const Zenith_Maths::Vector3 xVelocity =
			g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
		const float fPlanarSpeed = std::sqrt(
			xVelocity.x * xVelocity.x + xVelocity.z * xVelocity.z);
		xWalk.m_bPhysicsMotionSeen |=
			PlanarDistance(xPlayer.m_xPosition, xWalk.m_xApproachStart) > fPHYSICS_MIN_TRAVEL
			&& std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
				- ZM_PlayerController::fRUN_SPEED) <= fRUN_SPEED_TOLERANCE
			&& fPlanarSpeed > fPHYSICS_MIN_SPEED;
	}

	// Shared teardown for both tests. Runs on EVERY exit path, including a
	// mid-phase failure. The autosave latch fires on a completed warp into an
	// overworld scene, so both tests can leave a real Auto slot file behind --
	// Zenith_SaveData::ClearForTest does NOT delete files, so the slot sweep is
	// what actually cleans disk.
	void TearDownSaveResumeTest(bool bLoadedAScene)
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT_SHIFT, false);
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		if (bLoadedAScene)
		{
			g_xEngine.Scenes().LoadSceneByIndex(iBUILD_INDEX_FRONTEND, SCENE_LOAD_SINGLE);
		}
	}

	// ========================================================================
	// ZM_ResumePlacement_Test
	// ========================================================================

	// ---- phase budgets. 600 + 480 + 30 + 720 + 300 + 8 + 300 + 8 + 420 + 8
	// = 2874 < the 3000-frame cap, so every phase owns a REAL deadline and the
	// harness cap is only a backstop. ----
	constexpr int iRP_DAWNMERE_DEADLINE   = 600;
	constexpr int iRP_ARRIVE_DEADLINE     = 480;
	constexpr int iRP_BASIS_PROBE_FRAMES  = 30;
	constexpr int iRP_WALK_DEADLINE       = 720;
	constexpr int iRP_REST_DEADLINE       = 300;
	constexpr int iRP_CAPTURE_FRAMES      = 8;
	constexpr int iRP_RESUME_DEADLINE     = 300;
	constexpr int iRP_SETTLE_FRAMES       = 8;
	constexpr int iRP_FRONTEND_DEADLINE   = 420;
	constexpr int iRP_FRONTEND_SETTLE     = 8;

	enum class RPPhase
	{
		AwaitDawnmere,
		ArriveViaWarp,
		BasisProbe,
		Walk,
		RestBeforeCapture,
		CaptureAndResume,
		AwaitResume,
		SettleAfterResume,
		AwaitFrontEnd,
		PlayerlessCapture,
		Done,
	};

	RPPhase     g_eRPPhase        = RPPhase::Done;
	int         g_iRPPhaseFrames  = 0;
	int         g_iRPRestFrames   = 0;
	bool        g_bRPActive       = false;
	bool        g_bRPPrereqs      = false;
	bool        g_bRPFailed       = false;
	const char* g_szRPFailure     = "the test did not reach verification";

	PointWalk   g_xRPWalk;
	ZM_GameState g_xRPCaptureState;
	ZM_GameState g_xRPPlayerlessState;

	// The byte image of the playerless-capture destination, taken the moment that
	// destination is seeded. It is ONE fixed-size struct, so a fixed array plus an
	// explicit "was it taken" flag says exactly what a sized buffer used to say --
	// the flag carries the property the old size check carried, namely that an
	// UNTAKEN snapshot can never satisfy the byte comparison below and therefore can
	// never make the intactness verdict pass by accident.
	u_int8 g_auRPPlayerlessSnapshot[sizeof(ZM_WorldPosition)] = {};
	bool   g_bRPPlayerlessSnapshotTaken = false;

	Zenith_Maths::Vector3 g_xRPBasisStart   = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xRPSpawnCentre  = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xRPCapturedCentre = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xRPFinalCentre  = Zenith_Maths::Vector3(0.0f);
	float g_fRPBasisDeltaX  = 0.0f;
	float g_fRPBasisDeltaZ  = 0.0f;
	float g_fRPCapturedYaw  = 0.0f;
	float g_fRPFinalYaw     = 0.0f;
	float g_fRPFinalAlpha   = 1.0f;   // defaults to OPAQUE, i.e. failing
	u_int g_uRPLoadCountBeforeResume = 0u;
	u_int g_uRPLoadCountAfterResume  = 0u;
	u_int g_uRPFinalTransitionState  = (u_int)ZM_WARP_TRANSITION_QUEUED;   // != IDLE
	char  g_szRPCapturedTag[32]      = {};

	// ---- the MILESTONE AUTOSAVE observability. Both counters default to 0, so the
	// "rose by exactly 1" assertion FAILS unless both samples were really taken. ----
	u_int g_uRPAutosaveBeforeArrival = 0u;
	u_int g_uRPAutosaveAfterArrival  = 0u;
	// Defaults to EMPTY, i.e. failing: the assertion below demands READY.
	u_int g_uRPAutoSlotStatusAfterArrival = (u_int)ZM_SAVE_SLOT_EMPTY;

	// ---- the LIVE-BLOCKER integration negative. Every default FAILS: the blocker
	// defaults to NONE (the assertion demands WARP) and "after" starts BELOW
	// "before", so an unsampled probe can never pass on 0 == 0. ----
	bool  g_bRPBlockedProbeTaken       = false;
	bool  g_bRPBlockedCaptureWouldWork = false;
	bool  g_bRPBlockedAutosaveRefused  = false;
	u_int g_uRPBlockedProbeBlocker     = (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
	u_int g_uRPAutosaveBeforeBlocked   = 1u;
	u_int g_uRPAutosaveAfterBlocked    = 0u;
	ZM_GameState g_xRPBlockedProbeState;

	// ---- EVERY phase flag defaults to FAILING ----
	bool g_bRPArrivedViaWarp        = false;
	bool g_bRPSawArrivalWarpRun     = false;
	bool g_bRPArrivalTagRecorded    = false;
	bool g_bRPBasisPassed           = false;
	bool g_bRPReachedWalkTarget     = false;
	bool g_bRPWalkedFarEnough       = false;
	bool g_bRPYawIsNonTrivial       = false;
	bool g_bRPPhysicsMotionSeen     = false;
	bool g_bRPCaptureSucceeded      = false;
	bool g_bRPCaptureSceneCorrect   = false;
	bool g_bRPCaptureMatchesBody    = false;
	bool g_bRPCaptureTagNonEmpty    = false;
	bool g_bRPResumeRequested       = false;
	bool g_bRPResumePendingObserved = false;
	bool g_bRPSawResumeWarpRun      = false;
	bool g_bRPResumeCompleted       = false;
	bool g_bRPFinalMovementEnabled  = false;
	bool g_bRPPlayerlessRefused     = false;
	bool g_bRPPlayerlessStateIntact = false;
	bool g_bRPAutoSlotEmptyAtStart  = false;
	bool g_bRPAutosaveSampled       = false;

	void FailRP(const char* szReason)
	{
		g_szRPFailure = szReason;
		g_bRPFailed = true;
		ClearWalkInput(g_xRPWalk);
		g_eRPPhase = RPPhase::Done;
	}

	void Setup_ZMResumePlacement()
	{
		g_eRPPhase       = RPPhase::Done;
		g_iRPPhaseFrames = 0;
		g_iRPRestFrames  = 0;
		g_bRPActive      = false;
		g_bRPPrereqs     = false;
		g_bRPFailed      = false;
		g_szRPFailure    = "the test did not reach verification";

		g_xRPWalk = PointWalk{};
		g_xRPCaptureState = ZM_GameState();
		g_xRPPlayerlessState = ZM_GameState();
		memset(g_auRPPlayerlessSnapshot, 0, sizeof(g_auRPPlayerlessSnapshot));
		g_bRPPlayerlessSnapshotTaken = false;

		g_xRPBasisStart     = Zenith_Maths::Vector3(0.0f);
		g_xRPSpawnCentre    = Zenith_Maths::Vector3(0.0f);
		g_xRPCapturedCentre = Zenith_Maths::Vector3(0.0f);
		g_xRPFinalCentre    = Zenith_Maths::Vector3(0.0f);
		g_fRPBasisDeltaX = 0.0f;
		g_fRPBasisDeltaZ = 0.0f;
		g_fRPCapturedYaw = 0.0f;
		g_fRPFinalYaw    = 0.0f;
		g_fRPFinalAlpha  = 1.0f;
		g_uRPLoadCountBeforeResume = 0u;
		g_uRPLoadCountAfterResume  = 0u;
		g_uRPFinalTransitionState  = (u_int)ZM_WARP_TRANSITION_QUEUED;
		memset(g_szRPCapturedTag, 0, sizeof(g_szRPCapturedTag));
		g_uRPAutosaveBeforeArrival = 0u;
		g_uRPAutosaveAfterArrival  = 0u;
		g_uRPAutoSlotStatusAfterArrival = (u_int)ZM_SAVE_SLOT_EMPTY;
		g_bRPBlockedProbeTaken       = false;
		g_bRPBlockedCaptureWouldWork = false;
		g_bRPBlockedAutosaveRefused  = false;
		g_uRPBlockedProbeBlocker     = (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE;
		g_uRPAutosaveBeforeBlocked   = 1u;
		g_uRPAutosaveAfterBlocked    = 0u;
		g_xRPBlockedProbeState       = ZM_GameState();

		g_bRPArrivedViaWarp        = false;
		g_bRPSawArrivalWarpRun     = false;
		g_bRPArrivalTagRecorded    = false;
		g_bRPBasisPassed           = false;
		g_bRPReachedWalkTarget     = false;
		g_bRPWalkedFarEnough       = false;
		g_bRPYawIsNonTrivial       = false;
		g_bRPPhysicsMotionSeen     = false;
		g_bRPCaptureSucceeded      = false;
		g_bRPCaptureSceneCorrect   = false;
		g_bRPCaptureMatchesBody    = false;
		g_bRPCaptureTagNonEmpty    = false;
		g_bRPResumeRequested       = false;
		g_bRPResumePendingObserved = false;
		g_bRPSawResumeWarpRun      = false;
		g_bRPResumeCompleted       = false;
		g_bRPFinalMovementEnabled  = false;
		g_bRPPlayerlessRefused     = false;
		g_bRPPlayerlessStateIntact = false;
		g_bRPAutoSlotEmptyAtStart  = false;
		g_bRPAutosaveSampled       = false;

		// Asset guard FIRST. RequestSkip bypasses Verify entirely, so NO fixed dt
		// and NO scene load may be installed until every git-ignored input is
		// confirmed present.
		g_bRPPrereqs = RequiredDawnmereAssetsPresent();
		if (!g_bRPPrereqs)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_ResumePlacement] the Dawnmere scene / terrain bake is absent or "
				"incomplete -- there is no world to walk through (run a *_True config "
				"once to bake it)");
			return;
		}
		// The second and LAST skip: no persistent ZM_GameStateManager means
		// FrontEnd.zscen was never baked, so there is no ZM_GameStateRoot, no
		// "WarpFade" overlay, and TryQueueWarp refuses every warp on ApplyFadeVisual.
		// Deliberately narrow -- a missing arrival tag, a missing resume placement, a
		// dropped rotation write and a wedged machine all FAIL below.
		{
			Zenith_EntityID xManagerID = INVALID_ENTITY_ID;
			if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerID))
			{
				Zenith_AutomatedTestRunner::RequestSkip(
					"[ZM_ResumePlacement] no persistent ZM_GameStateManager singleton -- "
					"FrontEnd.zscen has not been baked, so there is no warp machine to "
					"resume through");
				return;
			}
		}

		// The Auto slot must start ABSENT, or the ProbeSlot(...) == READY assertion in
		// Verify could be satisfied by a file an EARLIER run left on disk instead of by
		// the autosave this test exists to observe. TearDownSaveResumeTest already
		// deletes every slot file on every exit path, so the only way a stale file
		// survives is a crashed or killed run -- which is exactly the case this covers,
		// and it is why the emptiness is RECORDED and asserted rather than assumed.
		// Nothing here can touch a real player save: under the automated harness
		// ZM_SaveSlots::SlotName resolves the "_Test" aliases
		// (Zenith_CommandLine::IsAutomatedTestRun()).
		ZM_SaveSlots::DeleteAllSlotsForTests();
		g_bRPAutoSlotEmptyAtStart =
			ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_EMPTY;

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fRESUME_FIXED_DT);
		g_xEngine.Scenes().LoadSceneByIndex(iBUILD_INDEX_DAWNMERE, SCENE_LOAD_SINGLE);

		g_eRPPhase  = RPPhase::AwaitDawnmere;
		g_bRPActive = true;
	}

	bool Step_ZMResumePlacement(int)
	{
		if (!g_bRPActive || g_bRPFailed || g_eRPPhase == RPPhase::Done)
		{
			return false;
		}
		++g_iRPPhaseFrames;

		switch (g_eRPPhase)
		{
		// ------------------------------------------------------------------
		// 1. Wait for the Setup SINGLE load to settle into a playable Dawnmere.
		// ------------------------------------------------------------------
		case RPPhase::AwaitDawnmere:
		{
			PlayerView xPlayer;
			ZM_GameStateManager* pxManager = ResolveManager();
			const bool bReady = ActiveBuildIndex() == iBUILD_INDEX_DAWNMERE
				&& FindActivePlayer(xPlayer)
				&& xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded()
				&& pxManager != nullptr
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE;
			if (!bReady)
			{
				if (g_iRPPhaseFrames > iRP_DAWNMERE_DEADLINE)
				{
					FailRP("Dawnmere never became playable (scene / unique player / valid "
						"body / grounded / an IDLE warp machine)");
					return false;
				}
				return true;
			}

			// Arrive through a REAL warp rather than the raw scene load. Two reasons,
			// both load-bearing:
			//   * m_szLastArrivedSpawnTag is written by the warp SUCCESS TAIL and by
			//     nothing else, and CaptureWorldPosition needs an arrived tag. A raw
			//     LoadSceneByIndex is not an arrival.
			//   * it exercises the ORDINARY (playerful) warp in the same session, so a
			//     playerless-destination patch that broke the normal path shows up here.
			// Dawnmere -> Dawnmere is a same-build-index warp, which the shipped
			// whiteout consume in ZM_GameStateManager::OnUpdate (it TryQueueWarps
			// uWHITEOUT_BUILD_INDEX / szWHITEOUT_SPAWN_TAG, i.e. Dawnmere's TownCenter)
			// already does in production. Named rather than line-cited on purpose --
			// the line numbers in that file move with every sub-commit.
			//
			// Snapshot the milestone-autosave counter IMMEDIATELY before the warp is
			// requested. The arrival tail latches the autosave and
			// ZM_GameStateManager::OnUpdate drains it on a LATER, fully IDLE frame, so
			// the delta sampled at capture time below belongs to THIS warp and to
			// nothing that happened before it (a stale latch inherited from a previous
			// batched test would have drained during the many IDLE frames this phase
			// already spent waiting for Dawnmere).
			g_uRPAutosaveBeforeArrival = ZM_GetAutosaveCount();
			if (!ZM_GameStateManager::RequestWarp(
				(u_int)iBUILD_INDEX_DAWNMERE, szDAWNMERE_SPAWN_TAG))
			{
				FailRP("RequestWarp(Dawnmere, TownCenter) was refused from a settled, "
					"IDLE Dawnmere -- the ordinary warp entry point is broken");
				return false;
			}
			g_eRPPhase = RPPhase::ArriveViaWarp;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 2. Let that warp complete, then take the SPAWN baseline from the pose it
		//    placed. Everything downstream is measured against this.
		// ------------------------------------------------------------------
		case RPPhase::ArriveViaWarp:
		{
			ZM_GameStateManager* pxManager = ResolveManager();
			if (pxManager != nullptr)
			{
				g_bRPSawArrivalWarpRun |=
					pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE;
			}

			// ------------------------------------------------------------------
			// The LIVE-BLOCKER integration negative, taken ONCE, on the first frame
			// of this phase where the warp is genuinely in flight AND the Dawnmere
			// player is still alive. This is the only place in either windowed test
			// where the autosave is refused by the POLICY and by nothing else:
			// ZM_TryAutosave's other two gates are both open here (SCENE_ENTERED is
			// live, no menu is on the stack), and the capture that would follow a
			// passing policy is PROVEN to succeed on this same frame -- so the
			// refusal cannot be attributed to the playerless-capture guard the way
			// it can on the quit-to-title path.
			//
			// Deterministic, not a race with the state machine: TryQueueWarp set the
			// state to QUEUED synchronously inside RequestWarp last frame and the
			// fade-out alone runs fFADE_DURATION_SECONDS (0.20 s, i.e. ~12 frames at
			// the fixed 1/60 dt this test installs), so the machine is non-IDLE for
			// many frames and ZM_SaveSlots::ResolveLiveSaveBlocker's WARP term --
			// IsWarpInProgress(), true for EVERY non-IDLE state -- is live for all of
			// them. Overworld is true (Dawnmere is a TOWN), no battle transition is
			// active and no whiteout is latched, so WARP is the FIRST matching term
			// in the fixed precedence and is the blocker that must do the refusing.
			// The probe is guarded on state != IDLE anyway, and g_bRPBlockedProbeTaken
			// defaults to false, so a run where the window somehow never appeared
			// FAILS in Verify rather than passing silently.
			//
			// No disk assertion is taken here on purpose: the counter only moves on a
			// LANDED write (ZM_Autosave.cpp increments after WriteState succeeds), so
			// "the count did not move" already means "no file was written", and a slot
			// probe here would additionally depend on nothing having autosaved earlier
			// in the batch -- a coupling this negative does not need.
			if (!g_bRPBlockedProbeTaken
				&& pxManager != nullptr
				&& pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
				&& ActiveBuildIndex() == iBUILD_INDEX_DAWNMERE)
			{
				PlayerView xBlockedPlayer;
				if (FindActivePlayer(xBlockedPlayer))
				{
					g_uRPBlockedProbeBlocker =
						(u_int)ZM_SaveSlots::ResolveLiveSaveBlocker();
					// Into a THROWAWAY state, never the live one: this call exists only
					// to prove the capture half would have succeeded here.
					g_xRPBlockedProbeState = ZM_GameState();
					g_bRPBlockedCaptureWouldWork =
						ZM_GameStateManager::CaptureWorldPosition(g_xRPBlockedProbeState);
					g_uRPAutosaveBeforeBlocked = ZM_GetAutosaveCount();
					g_bRPBlockedAutosaveRefused =
						!ZM_TryAutosave(ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED);
					g_uRPAutosaveAfterBlocked = ZM_GetAutosaveCount();
					g_bRPBlockedProbeTaken = true;
				}
			}

			PlayerView xPlayer;
			const bool bDone = pxManager != nullptr
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
				&& g_bRPSawArrivalWarpRun
				&& ActiveBuildIndex() == iBUILD_INDEX_DAWNMERE
				&& FindActivePlayer(xPlayer)
				&& xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded()
				&& xPlayer.m_pxController->IsMovementEnabled();
			if (!bDone)
			{
				if (g_iRPPhaseFrames > iRP_ARRIVE_DEADLINE)
				{
					FailRP("the ordinary Dawnmere->Dawnmere warp never completed back to "
						"an IDLE machine with a grounded, movable player");
					return false;
				}
				return true;
			}

			float fYaw = 0.0f;
			if (!ReadPlayerPose(xPlayer, g_xRPSpawnCentre, fYaw))
			{
				FailRP("the player's physics body could not be read after the arrival warp");
				return false;
			}
			g_bRPArrivedViaWarp = true;

			// The arrival tag the warp just recorded. Nothing tracked this before SC3:
			// m_szTargetSpawnTag is memset by ResetTransitionState on completion.
			const char* szArrived = ZM_GameStateManager::GetActiveSceneArrivedSpawnTag();
			g_bRPArrivalTagRecorded = szArrived != nullptr
				&& strcmp(szArrived, szDAWNMERE_SPAWN_TAG) == 0;

			g_xRPWalk.m_xTarget = Zenith_Maths::Vector3(
				g_xRPSpawnCentre.x + fWALK_OFFSET_X,
				g_xRPSpawnCentre.y,
				g_xRPSpawnCentre.z + fWALK_OFFSET_Z);

			// Plain held W (NO run modifier): the basis probe characterises the
			// movement basis and must match the shipped walk evidence exactly.
			g_xRPBasisStart = xPlayer.m_xPosition;
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			g_xRPWalk.m_abHeldKeys[0] = true;
			g_eRPPhase = RPPhase::BasisProbe;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 3. BASIS PROBE. Held W must move the player +Z and +Z must dominate.
		//    Without this, a walk that never moves at all reads as a mystery
		//    stall instead of "simulated input is not reaching the controller".
		// ------------------------------------------------------------------
		case RPPhase::BasisProbe:
		{
			PlayerView xPlayer;
			if (!FindActivePlayer(xPlayer))
			{
				FailRP("the player disappeared during the +Z basis probe");
				return false;
			}
			if (g_iRPPhaseFrames < iRP_BASIS_PROBE_FRAMES)
			{
				return true;
			}
			g_fRPBasisDeltaX = xPlayer.m_xPosition.x - g_xRPBasisStart.x;
			g_fRPBasisDeltaZ = xPlayer.m_xPosition.z - g_xRPBasisStart.z;
			ClearWalkInput(g_xRPWalk);
			if (g_fRPBasisDeltaZ < fBASIS_MIN_TRAVEL
				|| std::fabs(g_fRPBasisDeltaZ) <= std::fabs(g_fRPBasisDeltaX))
			{
				FailRP("held W did not move the player forward along +Z -- the movement "
					"basis is wrong (both measured deltas are logged below)");
				return false;
			}
			g_bRPBasisPassed = true;

			// Baseline the walk HERE, after the probe, so the physics-motion
			// displacement clause measures the WALK and not the probe.
			g_xRPWalk.m_xApproachStart = xPlayer.m_xPosition;
			g_xRPWalk.m_fCurrentDistance =
				PlanarDistance(xPlayer.m_xPosition, g_xRPWalk.m_xTarget);
			g_xRPWalk.m_fBestDistance = g_xRPWalk.m_fCurrentDistance;
			g_xRPWalk.m_iStallFrames = 0;
			g_eRPPhase = RPPhase::Walk;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 4. WALK to a coordinate that is provably NOT the spawn marker. Closed
		//    loop on the live position, with a progress watchdog.
		// ------------------------------------------------------------------
		case RPPhase::Walk:
		{
			PlayerView xPlayer;
			if (!FindActivePlayer(xPlayer))
			{
				FailRP("the player disappeared during the walk");
				return false;
			}

			AccumulatePhysicsMotionEvidence(g_xRPWalk, xPlayer);
			g_xRPWalk.m_xLivePosition = xPlayer.m_xPosition;
			g_xRPWalk.m_fCurrentDistance =
				PlanarDistance(xPlayer.m_xPosition, g_xRPWalk.m_xTarget);

			if (g_xRPWalk.m_fCurrentDistance <= fWALK_ARRIVAL_RADIUS)
			{
				ClearWalkInput(g_xRPWalk);
				g_bRPReachedWalkTarget = true;
				g_iRPRestFrames = 0;
				g_eRPPhase = RPPhase::RestBeforeCapture;
				g_iRPPhaseFrames = 0;
				return true;
			}

			if (g_xRPWalk.m_fCurrentDistance < g_xRPWalk.m_fBestDistance - fSTALL_IMPROVEMENT)
			{
				g_xRPWalk.m_fBestDistance = g_xRPWalk.m_fCurrentDistance;
				g_xRPWalk.m_iStallFrames = 0;
			}
			else if (++g_xRPWalk.m_iStallFrames > iSTALL_LIMIT_FRAMES)
			{
				FailRP("the walk STALLED -- the player stopped closing on the capture "
					"point (distances and held keys are logged below)");
				return false;
			}

			if (g_iRPPhaseFrames > iRP_WALK_DEADLINE)
			{
				FailRP("the walk never reached the capture point");
				return false;
			}

			DriveTowardXZ(g_xRPWalk, xPlayer.m_xPosition, g_xRPWalk.m_xTarget);
			return true;
		}

		// ------------------------------------------------------------------
		// 5. COME TO REST before capturing. A pose captured mid-motion or mid-air
		//    would be restored faithfully and then immediately settle somewhere
		//    else, and the restore assertion would red for a reason that has
		//    nothing to do with the code under test.
		// ------------------------------------------------------------------
		case RPPhase::RestBeforeCapture:
		{
			PlayerView xPlayer;
			if (!FindActivePlayer(xPlayer) || !xPlayer.m_pxCollider->HasValidBody())
			{
				FailRP("the player disappeared while settling before the capture");
				return false;
			}
			const Zenith_Maths::Vector3 xVelocity =
				g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
			const float fSpeed = std::sqrt(xVelocity.x * xVelocity.x
				+ xVelocity.y * xVelocity.y + xVelocity.z * xVelocity.z);
			if (xPlayer.m_pxController->IsGrounded() && fSpeed < fREST_SPEED)
			{
				++g_iRPRestFrames;
			}
			else
			{
				g_iRPRestFrames = 0;
			}

			if (g_iRPRestFrames < iREST_CONSECUTIVE)
			{
				if (g_iRPPhaseFrames > iRP_REST_DEADLINE)
				{
					FailRP("the player never came to rest on the ground at the capture "
						"point -- the captured pose would not be reproducible");
					return false;
				}
				return true;
			}

			if (!ReadPlayerPose(xPlayer, g_xRPCapturedCentre, g_fRPCapturedYaw))
			{
				FailRP("the player's physics body could not be read at the capture point");
				return false;
			}

			// The two anti-vacuity gates for everything that follows.
			g_bRPWalkedFarEnough = PlanarDistance(g_xRPCapturedCentre, g_xRPSpawnCentre)
				> fMIN_WALK_DISTANCE_FROM_SPAWN;
			g_bRPYawIsNonTrivial = std::fabs(g_fRPCapturedYaw) > fMIN_YAW_MAGNITUDE;
			g_bRPPhysicsMotionSeen = g_xRPWalk.m_bPhysicsMotionSeen;

			g_eRPPhase = RPPhase::CaptureAndResume;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 6. CAPTURE the pose into a GameState, then RESUME from it.
		// ------------------------------------------------------------------
		case RPPhase::CaptureAndResume:
		{
			if (g_iRPPhaseFrames < iRP_CAPTURE_FRAMES)
			{
				return true;
			}

			PlayerView xPlayer;
			if (!FindActivePlayer(xPlayer))
			{
				FailRP("the player disappeared before the capture");
				return false;
			}
			ZM_GameStateManager* pxManager = ResolveManager();
			if (pxManager == nullptr)
			{
				FailRP("the persistent ZM_GameStateManager was unresolvable at capture time");
				return false;
			}

			// Capture into a SEPARATE, default-constructed GameState -- never the live
			// one. The live state is DontDestroyOnLoad and would survive the reload in
			// RAM regardless, which is exactly the vacuity this test must not have.
			g_xRPCaptureState = ZM_GameState();
			g_bRPCaptureSucceeded =
				ZM_GameStateManager::CaptureWorldPosition(g_xRPCaptureState);
			if (!g_bRPCaptureSucceeded)
			{
				FailRP("CaptureWorldPosition refused with a live unique player in an "
					"overworld scene reached through a completed warp -- if the arrived "
					"spawn tag is empty, the warp success tail is not recording it");
				return false;
			}

			g_bRPCaptureSceneCorrect =
				g_xRPCaptureState.m_xWorldPosition.m_uSceneBuildIndex
					== (u_int)iBUILD_INDEX_DAWNMERE;
			g_bRPCaptureTagNonEmpty =
				g_xRPCaptureState.m_xWorldPosition.m_szSpawnTag[0] != '\0';
			memcpy(g_szRPCapturedTag, g_xRPCaptureState.m_xWorldPosition.m_szSpawnTag,
				sizeof(g_szRPCapturedTag) - 1u);

			// The captured CENTRE must be the live body centre, not the feet and not
			// the spawn marker -- a 0.9 m half-extent error lands squarely here.
			const Zenith_Maths::Vector3 xCaptured(
				g_xRPCaptureState.m_xWorldPosition.m_afPosition[0],
				g_xRPCaptureState.m_xWorldPosition.m_afPosition[1],
				g_xRPCaptureState.m_xWorldPosition.m_afPosition[2]);
			const Zenith_Maths::Vector3 xDelta = xCaptured - g_xRPCapturedCentre;
			g_bRPCaptureMatchesBody = glm::length(xDelta) < fMAX_CAPTURE_MATCH_ERROR;

			g_uRPLoadCountBeforeResume = pxManager->GetIssuedLoadRequestCount();

			// The MILESTONE AUTOSAVE, sampled here and NOT later, for two reasons:
			//   * the resume below is itself an arrival and latches a SECOND autosave,
			//     so a sample taken after it would fold two milestones into one delta
			//     and could no longer say "exactly one"; and
			//   * hundreds of frames of walking have passed since the arrival warp
			//     completed, so ZM_GameStateManager::OnUpdate's `m_bArrivalAutosavePending
			//     && IDLE` drain has long since run -- this is not a race with it.
			// The counter is a process global reset by
			// ZM_GameStateManager::ResetRuntimeStateForTests, which is why only the
			// DELTA is asserted.
			g_uRPAutosaveAfterArrival = ZM_GetAutosaveCount();
			g_uRPAutoSlotStatusAfterArrival =
				(u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO);
			g_bRPAutosaveSampled = true;

			g_bRPResumeRequested =
				ZM_GameStateManager::RequestResume(g_xRPCaptureState.m_xWorldPosition);
			if (!g_bRPResumeRequested)
			{
				FailRP("RequestResume refused a pose this test just captured through the "
					"production capture path");
				return false;
			}

			g_eRPPhase = RPPhase::AwaitResume;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 7. Let the resume warp run to completion.
		// ------------------------------------------------------------------
		case RPPhase::AwaitResume:
		{
			ZM_GameStateManager* pxManager = ResolveManager();
			if (pxManager != nullptr)
			{
				g_bRPSawResumeWarpRun |=
					pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE;
				g_bRPResumePendingObserved |= pxManager->IsResumePending();
			}

			PlayerView xPlayer;
			const bool bDone = pxManager != nullptr
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
				&& !pxManager->IsResumePending()
				&& g_bRPSawResumeWarpRun
				&& ActiveBuildIndex() == iBUILD_INDEX_DAWNMERE
				&& FindActivePlayer(xPlayer)
				&& xPlayer.m_pxCollider->HasValidBody();
			if (!bDone)
			{
				if (g_iRPPhaseFrames > iRP_RESUME_DEADLINE)
				{
					FailRP("the resume warp never completed back to an IDLE machine with "
						"the resume latch cleared");
					return false;
				}
				return true;
			}

			g_eRPPhase = RPPhase::SettleAfterResume;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 8. Settle, then re-resolve the player FRESH (the component pools
		//    relocate, and the reload rebuilt every entity) and read the final
		//    pose.
		// ------------------------------------------------------------------
		case RPPhase::SettleAfterResume:
		{
			if (g_iRPPhaseFrames < iRP_SETTLE_FRAMES)
			{
				return true;
			}

			PlayerView xPlayer;
			ZM_GameStateManager* pxManager = ResolveManager();
			if (!FindActivePlayer(xPlayer) || pxManager == nullptr)
			{
				FailRP("the player or the manager was unresolvable after the resume settled");
				return false;
			}
			if (!ReadPlayerPose(xPlayer, g_xRPFinalCentre, g_fRPFinalYaw))
			{
				FailRP("the restored player's physics body could not be read");
				return false;
			}
			g_bRPFinalMovementEnabled = xPlayer.m_pxController->IsMovementEnabled();
			g_fRPFinalAlpha = pxManager->GetFadeAlpha();
			g_uRPFinalTransitionState = (u_int)pxManager->GetTransitionState();
			g_uRPLoadCountAfterResume = pxManager->GetIssuedLoadRequestCount();
			g_bRPResumeCompleted = true;

			// Seed the playerless-capture destination with a RECOGNISABLE world
			// position and snapshot its bytes, so "the refusal left it untouched" is a
			// real byte comparison rather than "it was already default".
			g_xRPPlayerlessState = ZM_GameState();
			g_xRPPlayerlessState.m_xWorldPosition.m_uSceneBuildIndex = 4242u;
			memset(g_xRPPlayerlessState.m_xWorldPosition.m_szSpawnTag, 0,
				uZM_WORLD_SPAWN_TAG_CAPACITY);
			memcpy(g_xRPPlayerlessState.m_xWorldPosition.m_szSpawnTag, "SENTINEL", 8u);
			g_xRPPlayerlessState.m_xWorldPosition.m_afPosition[0] = -777.5f;
			g_xRPPlayerlessState.m_xWorldPosition.m_afPosition[1] = -778.5f;
			g_xRPPlayerlessState.m_xWorldPosition.m_afPosition[2] = -779.5f;
			g_xRPPlayerlessState.m_xWorldPosition.m_fYaw = -2.5f;
			memcpy(g_auRPPlayerlessSnapshot,
				&g_xRPPlayerlessState.m_xWorldPosition, sizeof(ZM_WorldPosition));
			g_bRPPlayerlessSnapshotTaken = true;

			g_xEngine.Scenes().LoadSceneByIndex(iBUILD_INDEX_FRONTEND, SCENE_LOAD_SINGLE);
			g_eRPPhase = RPPhase::AwaitFrontEnd;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 9. FrontEnd -- the one scene that authors NO Player at all.
		// ------------------------------------------------------------------
		case RPPhase::AwaitFrontEnd:
		{
			const bool bReady = ActiveBuildIndex() == iBUILD_INDEX_FRONTEND
				&& ActiveScenePlayerCount() == 0u;
			if (!bReady)
			{
				if (g_iRPPhaseFrames > iRP_FRONTEND_DEADLINE)
				{
					FailRP("FrontEnd did not become the active, playerless scene in time");
					return false;
				}
				return true;
			}
			if (g_iRPPhaseFrames < iRP_FRONTEND_SETTLE)
			{
				return true;
			}
			g_eRPPhase = RPPhase::PlayerlessCapture;
			g_iRPPhaseFrames = 0;
			return true;
		}

		// ------------------------------------------------------------------
		// 10. The capture must REFUSE with no player, and must not have touched
		//     its destination on the way out.
		// ------------------------------------------------------------------
		case RPPhase::PlayerlessCapture:
		{
			const bool bCaptured =
				ZM_GameStateManager::CaptureWorldPosition(g_xRPPlayerlessState);
			g_bRPPlayerlessRefused = !bCaptured;
			g_bRPPlayerlessStateIntact =
				g_bRPPlayerlessSnapshotTaken
				&& memcmp(g_auRPPlayerlessSnapshot,
					&g_xRPPlayerlessState.m_xWorldPosition, sizeof(ZM_WorldPosition)) == 0;
			g_eRPPhase = RPPhase::Done;
			return false;
		}

		case RPPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMResumePlacement()
	{
		bool bPassed = true;

		if (g_bRPActive)
		{
			const float fRestorePlanarError =
				PlanarDistance(g_xRPFinalCentre, g_xRPCapturedCentre);
			const float fRestoreVerticalError =
				std::fabs(g_xRPFinalCentre.y - g_xRPCapturedCentre.y);
			const float fRestoreFromSpawn =
				PlanarDistance(g_xRPFinalCentre, g_xRPSpawnCentre);
			const float fYawError =
				std::fabs(WrapAngleDifference(g_fRPFinalYaw, g_fRPCapturedYaw));

			// Everything captured, on one line, so a failure is fully localisable from
			// the log alone without a rebuild.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ResumePlacement] failed=%s (%s) | arrivedViaWarp=%s sawArrivalWarp=%s "
				"arrivalTagRecorded=%s | basis=%s (dx=%.3f dz=%.3f) | reachedTarget=%s "
				"walkedFarEnough=%s yawNonTrivial=%s physicsMotion=%s | "
				"spawn=(%.3f,%.3f,%.3f) captured=(%.3f,%.3f,%.3f) yawCaptured=%.4f | "
				"capture ok=%s scene=%s tag='%s' matchesBody=%s | "
				"resumeRequested=%s pendingObserved=%s sawResumeWarp=%s completed=%s | "
				"final=(%.3f,%.3f,%.3f) yawFinal=%.4f planarErr=%.4f (want < %.3f) "
				"vertErr=%.4f (want < %.3f) fromSpawn=%.3f (want > %.3f) yawErr=%.4f "
				"(want < %.3f) | alpha=%.3f state=%u loads %u->%u (want +1) movable=%s | "
				"autoSlotEmptyAtStart=%s autosaves %u->%u (want +1) autoSlotStatus=%u "
				"(want %u=READY) | blockedProbeTaken=%s blocker=%u (want %u=WARP) "
				"captureWouldWork=%s refused=%s autosaves %u->%u (want +0) | "
				"playerlessRefused=%s playerlessIntact=%s",
				g_bRPFailed ? "true" : "false", g_szRPFailure,
				g_bRPArrivedViaWarp ? "true" : "false",
				g_bRPSawArrivalWarpRun ? "true" : "false",
				g_bRPArrivalTagRecorded ? "true" : "false",
				g_bRPBasisPassed ? "true" : "false",
				(double)g_fRPBasisDeltaX, (double)g_fRPBasisDeltaZ,
				g_bRPReachedWalkTarget ? "true" : "false",
				g_bRPWalkedFarEnough ? "true" : "false",
				g_bRPYawIsNonTrivial ? "true" : "false",
				g_bRPPhysicsMotionSeen ? "true" : "false",
				(double)g_xRPSpawnCentre.x, (double)g_xRPSpawnCentre.y,
				(double)g_xRPSpawnCentre.z,
				(double)g_xRPCapturedCentre.x, (double)g_xRPCapturedCentre.y,
				(double)g_xRPCapturedCentre.z, (double)g_fRPCapturedYaw,
				g_bRPCaptureSucceeded ? "true" : "false",
				g_bRPCaptureSceneCorrect ? "true" : "false",
				g_szRPCapturedTag,
				g_bRPCaptureMatchesBody ? "true" : "false",
				g_bRPResumeRequested ? "true" : "false",
				g_bRPResumePendingObserved ? "true" : "false",
				g_bRPSawResumeWarpRun ? "true" : "false",
				g_bRPResumeCompleted ? "true" : "false",
				(double)g_xRPFinalCentre.x, (double)g_xRPFinalCentre.y,
				(double)g_xRPFinalCentre.z, (double)g_fRPFinalYaw,
				(double)fRestorePlanarError, (double)fMAX_RESTORE_PLANAR_ERROR,
				(double)fRestoreVerticalError, (double)fMAX_RESTORE_VERTICAL_ERROR,
				(double)fRestoreFromSpawn, (double)fMIN_RESTORE_DISTANCE_FROM_SPAWN,
				(double)fYawError, (double)fMAX_YAW_ERROR,
				(double)g_fRPFinalAlpha, g_uRPFinalTransitionState,
				g_uRPLoadCountBeforeResume, g_uRPLoadCountAfterResume,
				g_bRPFinalMovementEnabled ? "true" : "false",
				g_bRPAutoSlotEmptyAtStart ? "true" : "false",
				g_uRPAutosaveBeforeArrival, g_uRPAutosaveAfterArrival,
				g_uRPAutoSlotStatusAfterArrival, (u_int)ZM_SAVE_SLOT_READY,
				g_bRPBlockedProbeTaken ? "true" : "false",
				g_uRPBlockedProbeBlocker,
				(u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP,
				g_bRPBlockedCaptureWouldWork ? "true" : "false",
				g_bRPBlockedAutosaveRefused ? "true" : "false",
				g_uRPAutosaveBeforeBlocked, g_uRPAutosaveAfterBlocked,
				g_bRPPlayerlessRefused ? "true" : "false",
				g_bRPPlayerlessStateIntact ? "true" : "false");

			if (g_bRPFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_ResumePlacement] %s", g_szRPFailure);
				bPassed = false;
			}

			// ---- the arrival, which is what makes an arrived spawn tag exist ----
			if (!g_bRPArrivedViaWarp || !g_bRPSawArrivalWarpRun)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the ordinary Dawnmere warp did not run to "
					"completion, so nothing downstream was measured from a real arrival");
				bPassed = false;
			}
			if (!g_bRPArrivalTagRecorded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the completed warp did not record 'TownCenter' as "
					"the arrived spawn tag -- the warp success tail is not copying "
					"m_szTargetSpawnTag into m_szLastArrivedSpawnTag BEFORE "
					"ResetTransitionState memsets it");
				bPassed = false;
			}

			// ---- the walk: without these the position assertions are satisfiable by
			// the spawn-marker teleport that happens anyway ----
			if (!g_bRPBasisPassed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the movement basis probe failed (dx=%.3f dz=%.3f)",
					(double)g_fRPBasisDeltaX, (double)g_fRPBasisDeltaZ);
				bPassed = false;
			}
			if (!g_bRPReachedWalkTarget)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the player never reached the capture point");
				bPassed = false;
			}
			if (!g_bRPPhysicsMotionSeen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] no PHYSICS-DRIVEN motion was ever observed -- the "
					"player did not run under simulated input (a teleport would look like "
					"this)");
				bPassed = false;
			}
			if (!g_bRPWalkedFarEnough)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the capture point is only %.3f m from the spawn "
					"(need > %.1f) -- the restore assertion would be satisfied by plain "
					"marker placement",
					(double)PlanarDistance(g_xRPCapturedCentre, g_xRPSpawnCentre),
					(double)fMIN_WALK_DISTANCE_FROM_SPAWN);
				bPassed = false;
			}
			if (!g_bRPYawIsNonTrivial)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the captured yaw is only %.4f rad -- a yaw round "
					"trip through ~0 is vacuous because TeleportBody's identity rotation "
					"also yields ~0", (double)g_fRPCapturedYaw);
				bPassed = false;
			}

			// ---- the capture ----
			if (!g_bRPCaptureSucceeded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] CaptureWorldPosition returned false");
				bPassed = false;
			}
			if (!g_bRPCaptureSceneCorrect)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the captured scene build index was %u, expected %d",
					g_xRPCaptureState.m_xWorldPosition.m_uSceneBuildIndex,
					iBUILD_INDEX_DAWNMERE);
				bPassed = false;
			}
			if (!g_bRPCaptureTagNonEmpty)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the captured spawn tag is EMPTY -- the codec "
					"rejects that pairing and the save would be unwritable");
				bPassed = false;
			}
			if (!g_bRPCaptureMatchesBody)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the captured position is not the live body CENTRE "
					"(a 0.9 m gap means feet-vs-centre confusion)");
				bPassed = false;
			}

			// ---- the resume ----
			if (!g_bRPResumeRequested || !g_bRPSawResumeWarpRun || !g_bRPResumeCompleted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the resume did not run to completion");
				bPassed = false;
			}
			if (!g_bRPResumePendingObserved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] IsResumePending() was never true after "
					"RequestResume -- the one-shot pose override was never latched, so the "
					"placement below could only be marker placement");
				bPassed = false;
			}
			if (g_uRPLoadCountAfterResume != g_uRPLoadCountBeforeResume + 1u)
			{
				// This is what stops the restore assertions being satisfiable by a player
				// that never went away: exactly one SINGLE load must have happened.
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the resume issued %u loads, expected exactly 1 "
					"(%u -> %u) -- without a real scene load the 'restored' pose is just "
					"the pose that was never destroyed",
					g_uRPLoadCountAfterResume - g_uRPLoadCountBeforeResume,
					g_uRPLoadCountBeforeResume, g_uRPLoadCountAfterResume);
				bPassed = false;
			}
			if (fRestorePlanarError >= fMAX_RESTORE_PLANAR_ERROR)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the restored XZ position is %.4f m from the "
					"captured one (limit %.3f) -- ApplyPendingResumePlacement is missing, or "
					"runs BEFORE the marker TeleportBody that overwrites it",
					(double)fRestorePlanarError, (double)fMAX_RESTORE_PLANAR_ERROR);
				bPassed = false;
			}
			if (fRestoreVerticalError >= fMAX_RESTORE_VERTICAL_ERROR)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the restored height is %.4f m from the captured "
					"one (limit %.3f)",
					(double)fRestoreVerticalError, (double)fMAX_RESTORE_VERTICAL_ERROR);
				bPassed = false;
			}
			if (fRestoreFromSpawn <= fMIN_RESTORE_DISTANCE_FROM_SPAWN)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the player ended %.3f m from the SPAWN MARKER "
					"(need > %.1f) -- the resume degraded to ordinary spawn-tag placement",
					(double)fRestoreFromSpawn, (double)fMIN_RESTORE_DISTANCE_FROM_SPAWN);
				bPassed = false;
			}
			if (fYawError >= fMAX_YAW_ERROR)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the restored yaw is %.4f rad off the captured "
					"%.4f (limit %.3f) -- TeleportBody forces IDENTITY rotation, so a "
					"missing or mis-ordered SetBodyRotation lands exactly here",
					(double)fYawError, (double)g_fRPCapturedYaw, (double)fMAX_YAW_ERROR);
				bPassed = false;
			}
			if (g_uRPFinalTransitionState != (u_int)ZM_WARP_TRANSITION_IDLE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the warp machine ended in state %u, expected IDLE",
					g_uRPFinalTransitionState);
				bPassed = false;
			}
			if (g_fRPFinalAlpha != 0.0f)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the fade ended at alpha %.3f, expected 0 -- the "
					"resume landed geometrically but left the screen dark",
					(double)g_fRPFinalAlpha);
				bPassed = false;
			}
			if (!g_bRPFinalMovementEnabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the player is still frozen after the resume");
				bPassed = false;
			}

			// ---- the MILESTONE AUTOSAVE ------------------------------------------
			// The only assertions anywhere on the autosave PRODUCER. Source mutations
			// each of the three reds, and none of them is caught by any other assertion
			// in this file:
			//   * delete the `m_bArrivalAutosavePending && IDLE` drain block in
			//     ZM_GameStateManager::OnUpdate  -> the count never rises AND the Auto
			//     slot stays EMPTY (reds the second and third checks);
			//   * drop RecordArrivalAndLatchAutosave from the playerful fade-in tail in
			//     AdvanceFadeIn                  -> same two reds;
			//   * make ZM_TryAutosave write a MANUAL slot instead of ZM_SAVE_SLOT_AUTO
			//                                    -> the count rises but the Auto slot
			//                                       probes EMPTY (reds the third alone).
			// The test can observe all of them because the counter is incremented ONLY
			// on a landed write (ZM_Autosave.cpp) and the slot probe is a real re-read
			// of the file from disk.
			if (!g_bRPAutoSlotEmptyAtStart)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the Auto slot was NOT empty when the test started "
					"-- the READY assertion below would be satisfiable by a file an earlier "
					"run left behind, so this run proves nothing about the autosave");
				bPassed = false;
			}
			if (!g_bRPAutosaveSampled
				|| g_uRPAutosaveAfterArrival != g_uRPAutosaveBeforeArrival + 1u)
			{
				// The two counter values are printed RAW rather than as a delta: an
				// unsampled run leaves "after" below "before", and an unsigned subtraction
				// would report that as ~4 billion.
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the autosave counter went %u -> %u across the "
					"arrival warp (sampled=%s), expected exactly +1 -- the milestone latch "
					"set by the arrival tail is never being drained into ZM_TryAutosave",
					g_uRPAutosaveBeforeArrival, g_uRPAutosaveAfterArrival,
					g_bRPAutosaveSampled ? "true" : "false");
				bPassed = false;
			}
			if (g_uRPAutoSlotStatusAfterArrival != (u_int)ZM_SAVE_SLOT_READY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the Auto slot probed %u after the arrival autosave, "
					"expected %u (READY) -- the counter can rise without a loadable file on "
					"disk, so this is the assertion that proves a real save landed",
					g_uRPAutoSlotStatusAfterArrival, (u_int)ZM_SAVE_SLOT_READY);
				bPassed = false;
			}

			// ---- the LIVE-BLOCKER integration negative --------------------------
			// The save-BLOCKER term of the autosave policy, proven at the INTEGRATION
			// boundary and not only by the pure boot units. The quit-to-title test
			// cannot do this job (see the comment on its own autosave assertions): on
			// that path the refusal is over-determined, because FrontEnd has no player
			// and CaptureWorldPosition would refuse whatever the policy answered. Here
			// the capture is proven to succeed on the very same frame, so the policy is
			// the ONLY thing that can be refusing.
			//
			// Source mutations these red. None of them was observable anywhere in
			// either windowed test before this probe existed -- the quit-to-title
			// assertions cannot see any of them (see that test's own comment), and the
			// arrival delta above only co-reds now because a mutated autosave would
			// fire from INSIDE the window this probe sits in:
			//   * delete the `if (!ZM_ShouldAutosave(...)) { return false; }` block at
			//     the top of ZM_TryAutosave (ZM_Autosave.cpp) -> the capture succeeds,
			//     the Auto slot is written mid-warp and the counter moves by 1;
			//   * make ZM_ShouldAutosave stop comparing the blocker against NONE, or
			//     whitelist ZM_SAVE_BLOCKER_WARP -> same;
			//   * drop the IsWarpInProgress() term from
			//     ZM_SaveSlots::ResolveLiveSaveBlocker -> the blocker resolves NONE and
			//     both the blocker assertion and the counter assertion red.
			// (The ORDER of the policy check and the capture inside ZM_TryAutosave is
			// NOT pinned here -- ZM_TryAutosave captures into the LIVE game state, which
			// nothing in this test reads. That ordering has no coverage; it is called
			// out so nobody mistakes this probe for it.)
			//
			// The POSITIVE half -- that the same trigger DOES save once the blocker
			// clears -- is the "autosaves ... (want +1)" assertion above: the arrival
			// drain calls ZM_TryAutosave with the identical trigger from an IDLE,
			// overworld, menu-closed frame and it lands. Without that pairing this
			// negative would be satisfiable by an autosave that never fires at all.
			if (!g_bRPBlockedProbeTaken)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the live-blocker probe was never taken -- the "
					"warp was never observed in flight with a live Dawnmere player, so "
					"the blocker half of the autosave policy was not exercised at the "
					"integration boundary at all");
				bPassed = false;
			}
			if (g_uRPBlockedProbeBlocker != (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the live blocker during the in-flight warp "
					"resolved to %u, expected %u (WARP) -- the refusal below would then "
					"be attributable to some other term of the policy",
					g_uRPBlockedProbeBlocker,
					(u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP);
				bPassed = false;
			}
			if (!g_bRPBlockedCaptureWouldWork)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] CaptureWorldPosition refused on the same frame "
					"as the blocked autosave -- without a capture that WOULD have "
					"succeeded, the refusal below proves nothing about the blocker");
				bPassed = false;
			}
			if (!g_bRPBlockedAutosaveRefused)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] ZM_TryAutosave RETURNED TRUE with a live WARP "
					"blocker -- a milestone autosave fired from inside a transition");
				bPassed = false;
			}
			if (g_uRPAutosaveAfterBlocked != g_uRPAutosaveBeforeBlocked)
			{
				// Printed RAW rather than as a delta: the unsampled defaults leave
				// "after" below "before", and an unsigned subtraction would report that
				// as ~4 billion.
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the autosave counter went %u -> %u across a "
					"blocked attempt (probeTaken=%s), expected NO change -- an autosave "
					"landed on disk while a warp owned the screen",
					g_uRPAutosaveBeforeBlocked, g_uRPAutosaveAfterBlocked,
					g_bRPBlockedProbeTaken ? "true" : "false");
				bPassed = false;
			}

			// ---- the playerless refusal ----
			if (!g_bRPPlayerlessRefused)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] CaptureWorldPosition SUCCEEDED in FrontEnd, which "
					"authors no Player at all");
				bPassed = false;
			}
			if (!g_bRPPlayerlessStateIntact)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ResumePlacement] the refused capture still mutated its destination "
					"world position");
				bPassed = false;
			}
		}

		TearDownSaveResumeTest(g_bRPActive);
		g_bRPActive = false;

		return bPassed || !g_bRPPrereqs;
	}

	// ========================================================================
	// ZM_QuitToFrontEnd_Test
	// ========================================================================

	// 600 + 420 + 160 + 8 = 1188 < the 1200-frame cap.
	constexpr int iQF_DAWNMERE_DEADLINE = 600;
	constexpr int iQF_FRONTEND_DEADLINE = 420;
	constexpr int iQF_IDLE_DEADLINE     = 160;
	constexpr int iQF_SETTLE_FRAMES     = 8;

	enum class QFPhase
	{
		AwaitDawnmere,
		AwaitFrontEnd,
		AwaitIdle,
		Settle,
		Done,
	};

	QFPhase     g_eQFPhase       = QFPhase::Done;
	int         g_iQFPhaseFrames = 0;
	bool        g_bQFActive      = false;
	bool        g_bQFPrereqs     = false;
	bool        g_bQFFailed      = false;
	const char* g_szQFFailure    = "the test did not reach verification";

	u_int g_uQFLoadCountBefore = 0u;
	u_int g_uQFLoadCountAfter  = 0u;
	u_int g_uQFPlayerCountBefore = 0u;
	u_int g_uQFPlayerCountAfter  = 99u;   // defaults to FAILING
	u_int g_uQFFinalTransitionState = (u_int)ZM_WARP_TRANSITION_QUEUED;   // != IDLE
	float g_fQFFinalAlpha = 1.0f;         // defaults to OPAQUE, i.e. failing
	float g_fQFPeakAlpha  = 0.0f;
	int   g_iQFFramesToFrontEnd = -1;

	// ---- the MILESTONE AUTOSAVE must NOT fire on a quit to the title. "After"
	// defaults to a value the "before" can never legitimately reach, so an unsampled
	// run FAILS rather than passing on 0 == 0. ----
	u_int g_uQFAutosaveBefore = 0u;
	u_int g_uQFAutosaveAfter  = 0xFFFFFFFFu;
	u_int g_uQFAutoSlotStatusAfter = (u_int)ZM_SAVE_SLOT_READY;   // defaults to FAILING
	bool  g_bQFAutoSlotEmptyAtStart = false;

	bool g_bQFRequested          = false;
	bool g_bQFReachedFrontEnd    = false;
	bool g_bQFReachedIdle        = false;
	bool g_bQFSawPlayerlessFlag  = false;
	bool g_bQFSawTransitionRun   = false;

	void FailQF(const char* szReason)
	{
		g_szQFFailure = szReason;
		g_bQFFailed = true;
		g_eQFPhase = QFPhase::Done;
	}

	void Setup_ZMQuitToFrontEnd()
	{
		g_eQFPhase       = QFPhase::Done;
		g_iQFPhaseFrames = 0;
		g_bQFActive      = false;
		g_bQFPrereqs     = false;
		g_bQFFailed      = false;
		g_szQFFailure    = "the test did not reach verification";

		g_uQFLoadCountBefore = 0u;
		g_uQFLoadCountAfter  = 0u;
		g_uQFPlayerCountBefore = 0u;
		g_uQFPlayerCountAfter  = 99u;
		g_uQFFinalTransitionState = (u_int)ZM_WARP_TRANSITION_QUEUED;
		g_fQFFinalAlpha = 1.0f;
		g_fQFPeakAlpha  = 0.0f;
		g_iQFFramesToFrontEnd = -1;
		g_uQFAutosaveBefore = 0u;
		g_uQFAutosaveAfter  = 0xFFFFFFFFu;
		g_uQFAutoSlotStatusAfter = (u_int)ZM_SAVE_SLOT_READY;
		g_bQFAutoSlotEmptyAtStart = false;

		g_bQFRequested         = false;
		g_bQFReachedFrontEnd   = false;
		g_bQFReachedIdle       = false;
		g_bQFSawPlayerlessFlag = false;
		g_bQFSawTransitionRun  = false;

		g_bQFPrereqs = RequiredDawnmereAssetsPresent();
		if (!g_bQFPrereqs)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_QuitToFrontEnd] the Dawnmere scene / terrain bake is absent or "
				"incomplete -- there is no overworld to quit FROM (run a *_True config "
				"once to bake it)");
			return;
		}
		{
			Zenith_EntityID xManagerID = INVALID_ENTITY_ID;
			if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerID))
			{
				Zenith_AutomatedTestRunner::RequestSkip(
					"[ZM_QuitToFrontEnd] no persistent ZM_GameStateManager singleton -- "
					"FrontEnd.zscen has not been baked, so there is no warp machine to quit "
					"through");
				return;
			}
		}

		// Start from a known-empty Auto slot, for the same reason the placement test
		// does: without it the "the Auto slot is still EMPTY after the quit" assertion
		// below could red on a file an earlier run left behind rather than on anything
		// the quit did. Only "_Test" alias files are reachable here (see the placement
		// test's Setup).
		ZM_SaveSlots::DeleteAllSlotsForTests();
		g_bQFAutoSlotEmptyAtStart =
			ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO) == ZM_SAVE_SLOT_EMPTY;

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fRESUME_FIXED_DT);
		g_xEngine.Scenes().LoadSceneByIndex(iBUILD_INDEX_DAWNMERE, SCENE_LOAD_SINGLE);

		g_eQFPhase  = QFPhase::AwaitDawnmere;
		g_bQFActive = true;
	}

	bool Step_ZMQuitToFrontEnd(int)
	{
		if (!g_bQFActive || g_bQFFailed || g_eQFPhase == QFPhase::Done)
		{
			return false;
		}
		++g_iQFPhaseFrames;

		switch (g_eQFPhase)
		{
		case QFPhase::AwaitDawnmere:
		{
			PlayerView xPlayer;
			ZM_GameStateManager* pxManager = ResolveManager();
			const bool bReady = ActiveBuildIndex() == iBUILD_INDEX_DAWNMERE
				&& FindActivePlayer(xPlayer)
				&& xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded()
				&& pxManager != nullptr
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE;
			if (!bReady)
			{
				if (g_iQFPhaseFrames > iQF_DAWNMERE_DEADLINE)
				{
					FailQF("Dawnmere never became playable (scene / unique player / valid "
						"body / grounded / an IDLE warp machine)");
					return false;
				}
				return true;
			}

			g_uQFLoadCountBefore = pxManager->GetIssuedLoadRequestCount();
			g_uQFPlayerCountBefore = ActiveScenePlayerCount();
			// Sampled from a settled, IDLE Dawnmere immediately before the quit, so the
			// delta measured in Settle belongs to the quit alone. Nothing has warped in
			// this test yet -- Setup entered Dawnmere with a raw LoadSceneByIndex, which
			// is not an arrival and latches nothing.
			g_uQFAutosaveBefore = ZM_GetAutosaveCount();

			if (!ZM_GameStateManager::RequestQuitToFrontEnd())
			{
				FailQF("RequestQuitToFrontEnd was refused from a settled, IDLE Dawnmere");
				return false;
			}
			g_bQFRequested = true;
			g_eQFPhase = QFPhase::AwaitFrontEnd;
			g_iQFPhaseFrames = 0;
			return true;
		}

		case QFPhase::AwaitFrontEnd:
		{
			ZM_GameStateManager* pxManager = ResolveManager();
			if (pxManager != nullptr)
			{
				g_bQFSawTransitionRun |=
					pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE;
				// The FLAG that switches on the whole playerless-destination path. It is
				// cleared by ResetTransitionState on completion, so it can only ever be
				// observed DURING the transition.
				g_bQFSawPlayerlessFlag |= pxManager->IsPlayerlessDestination();
				const float fAlpha = pxManager->GetFadeAlpha();
				if (fAlpha > g_fQFPeakAlpha) { g_fQFPeakAlpha = fAlpha; }
			}

			if (ActiveBuildIndex() != iBUILD_INDEX_FRONTEND)
			{
				if (g_iQFPhaseFrames > iQF_FRONTEND_DEADLINE)
				{
					FailQF("FrontEnd never became the active scene -- the SINGLE load was "
						"never issued");
					return false;
				}
				return true;
			}
			g_bQFReachedFrontEnd = true;
			g_iQFFramesToFrontEnd = g_iQFPhaseFrames;
			g_eQFPhase = QFPhase::AwaitIdle;
			g_iQFPhaseFrames = 0;
			return true;
		}

		case QFPhase::AwaitIdle:
		{
			// The manager is legitimately UNRESOLVABLE for a few frames here: the
			// FrontEnd load re-authors ZM_GameStateRoot, so a DUPLICATE manager exists
			// until its OnStart destroys it, and TryGetUniqueSingletonEntityID refuses
			// while both are alive. Poll rather than fail.
			ZM_GameStateManager* pxManager = ResolveManager();
			if (pxManager != nullptr)
			{
				g_bQFSawPlayerlessFlag |= pxManager->IsPlayerlessDestination();
				const float fAlpha = pxManager->GetFadeAlpha();
				if (fAlpha > g_fQFPeakAlpha) { g_fQFPeakAlpha = fAlpha; }
			}

			if (pxManager == nullptr
				|| pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE)
			{
				if (g_iQFPhaseFrames > iQF_IDLE_DEADLINE)
				{
					FailQF("the warp machine never returned to IDLE on FrontEnd -- it is "
						"parked waiting for a Player or a follow camera that FrontEnd does "
						"not author (BOTH AdvanceFadeIn barriers must be bypassed, not just "
						"the spawn poll)");
					return false;
				}
				return true;
			}
			g_bQFReachedIdle = true;
			g_eQFPhase = QFPhase::Settle;
			g_iQFPhaseFrames = 0;
			return true;
		}

		case QFPhase::Settle:
		{
			if (g_iQFPhaseFrames < iQF_SETTLE_FRAMES)
			{
				return true;
			}
			ZM_GameStateManager* pxManager = ResolveManager();
			if (pxManager == nullptr)
			{
				FailQF("the persistent manager was unresolvable after the quit settled");
				return false;
			}
			g_fQFFinalAlpha = pxManager->GetFadeAlpha();
			g_uQFFinalTransitionState = (u_int)pxManager->GetTransitionState();
			g_uQFLoadCountAfter = pxManager->GetIssuedLoadRequestCount();
			g_uQFPlayerCountAfter = ActiveScenePlayerCount();
			// Sampled AFTER the machine reached IDLE plus iQF_SETTLE_FRAMES more frames,
			// which is what makes the "did not rise" assertion reachable rather than
			// vacuously true: AdvanceFadeIn's playerless tail DOES latch the milestone,
			// and ZM_GameStateManager::OnUpdate's drain has therefore already run and
			// already called ZM_TryAutosave, which refused it (FrontEnd is not an
			// overworld, so ResolveLiveSaveBlocker reports NOT_OVERWORLD).
			g_uQFAutosaveAfter = ZM_GetAutosaveCount();
			g_uQFAutoSlotStatusAfter = (u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO);
			g_eQFPhase = QFPhase::Done;
			return false;
		}

		case QFPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMQuitToFrontEnd()
	{
		bool bPassed = true;

		if (g_bQFActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_QuitToFrontEnd] failed=%s (%s) | requested=%s reachedFrontEnd=%s "
				"(after %d frames, deadline %d) reachedIdle=%s | sawTransitionRun=%s "
				"sawPlayerlessFlag=%s peakAlpha=%.3f (want >= 1) | finalAlpha=%.3f "
				"(want 0) finalState=%u (want %u=IDLE) | players %u->%u (want 1->0) | "
				"loads %u->%u (want +1) | autoSlotEmptyAtStart=%s autosaves %u->%u "
				"(want +0) autoSlotStatus=%u (want %u=EMPTY)",
				g_bQFFailed ? "true" : "false", g_szQFFailure,
				g_bQFRequested ? "true" : "false",
				g_bQFReachedFrontEnd ? "true" : "false",
				g_iQFFramesToFrontEnd, iQF_FRONTEND_DEADLINE,
				g_bQFReachedIdle ? "true" : "false",
				g_bQFSawTransitionRun ? "true" : "false",
				g_bQFSawPlayerlessFlag ? "true" : "false",
				(double)g_fQFPeakAlpha,
				(double)g_fQFFinalAlpha,
				g_uQFFinalTransitionState, (u_int)ZM_WARP_TRANSITION_IDLE,
				g_uQFPlayerCountBefore, g_uQFPlayerCountAfter,
				g_uQFLoadCountBefore, g_uQFLoadCountAfter,
				g_bQFAutoSlotEmptyAtStart ? "true" : "false",
				g_uQFAutosaveBefore, g_uQFAutosaveAfter,
				g_uQFAutoSlotStatusAfter, (u_int)ZM_SAVE_SLOT_EMPTY);

			if (g_bQFFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_QuitToFrontEnd] %s", g_szQFFailure);
				bPassed = false;
			}

			if (!g_bQFRequested)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] RequestQuitToFrontEnd was never accepted");
				bPassed = false;
			}
			// The SOURCE must genuinely have had a player, or "FrontEnd has none"
			// afterwards proves nothing.
			if (g_uQFPlayerCountBefore != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] Dawnmere carried %u players before the quit, "
					"expected exactly 1", g_uQFPlayerCountBefore);
				bPassed = false;
			}
			if (!g_bQFSawTransitionRun)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the warp machine was never observed out of IDLE -- "
					"the quit did not go through the fade + load machine at all");
				bPassed = false;
			}
			if (g_fQFPeakAlpha < 1.0f)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the fade only reached alpha %.3f -- the load must "
					"be issued from a FULLY opaque screen, never from a bare "
					"LoadSceneByIndex call bolted on beside the machine",
					(double)g_fQFPeakAlpha);
				bPassed = false;
			}
			if (!g_bQFSawPlayerlessFlag)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] IsPlayerlessDestination() was never true during the "
					"transition -- the destination was not classified as playerless, so the "
					"spawn poll and the fade-in are still running their ordinary barriers");
				bPassed = false;
			}
			if (!g_bQFReachedFrontEnd)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] FrontEnd never became the active scene");
				bPassed = false;
			}
			if (!g_bQFReachedIdle)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the warp machine never returned to IDLE");
				bPassed = false;
			}
			if (g_uQFFinalTransitionState != (u_int)ZM_WARP_TRANSITION_IDLE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the machine settled in state %u, expected IDLE",
					g_uQFFinalTransitionState);
				bPassed = false;
			}
			if (g_fQFFinalAlpha != 0.0f)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the fade ended at alpha %.3f, expected 0 -- the "
					"title screen is sitting behind a permanently opaque overlay",
					(double)g_fQFFinalAlpha);
				bPassed = false;
			}
			if (g_uQFPlayerCountAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] FrontEnd carries %u ZM_PlayerControllers -- it is "
					"supposed to author NONE, and one would silently disable the whole "
					"playerless-destination path this test exists to prove",
					g_uQFPlayerCountAfter);
				bPassed = false;
			}
			if (g_uQFLoadCountAfter != g_uQFLoadCountBefore + 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the quit issued %u loads, expected exactly 1 "
					"(%u -> %u) -- a second load path was added beside the machine",
					g_uQFLoadCountAfter - g_uQFLoadCountBefore,
					g_uQFLoadCountBefore, g_uQFLoadCountAfter);
				bPassed = false;
			}

			// ---- quitting to the title must NOT autosave --------------------------
			// These two assertions ARE reached: the playerless fade-in tail in
			// AdvanceFadeIn latches the milestone exactly like the playerful one, and
			// the drain in ZM_GameStateManager::OnUpdate really does call ZM_TryAutosave
			// with it. What they are NOT is a proof of the save-BLOCKER policy, and an
			// earlier version of this comment wrongly claimed they caught two specific
			// policy mutations. They cannot, because on this path the refusal is
			// OVER-DETERMINED by two INDEPENDENT gates:
			//   1. the POLICY: FrontEnd is not an overworld, so ResolveLiveSaveBlocker
			//      reports NOT_OVERWORLD and ZM_ShouldAutosave refuses first; and
			//   2. the CAPTURE: even with the whole policy check deleted (or
			//      NOT_OVERWORLD whitelisted), the very next gate --
			//      CaptureWorldPosition's FindUniquePlayerInScene -- refuses, because
			//      FrontEnd authors no Player at all. This test pins that itself: the
			//      players 1 -> 0 assertion above.
			// Either gate alone keeps the counter still and the slot EMPTY, so no single
			// mutation to ZM_ShouldAutosave or to ZM_TryAutosave's policy consultation
			// is observable from here, and the harness cannot red on the Zenith_Error
			// ZM_TryAutosave would log either -- pass/fail keys solely off Verify's
			// return.
			//
			// What these two DO pin is the property nothing else covers: an autosave
			// must never land on a PLAYERLESS destination BY ANY ROUTE -- a second write
			// path bolted on beside ZM_TryAutosave, a capture that invents a pose when
			// it finds no player, or a ZM_TryAutosave that writes the slot despite a
			// failed capture, all land exactly here (the count rises and the Auto slot
			// probes READY on the title screen).
			//
			// The blocker POLICY itself is pinned by the pure boot units
			// Autosave_BlockedByNotOverworld / Autosave_BlockedByBattle /
			// Autosave_BlockedByWarp / Autosave_BlockedByPendingWhiteout /
			// Autosave_BlockedByMenuOpen in Tests/ZM_Tests_ResumePoint.cpp (one blocker
			// per unit, each with its own all-clear control), and at the INTEGRATION
			// boundary by ZM_ResumePlacement_Test's live-blocker probe, which calls
			// ZM_TryAutosave mid-warp with a live Dawnmere player whose capture is
			// proven to succeed on the same frame.
			if (g_uQFAutosaveAfter != g_uQFAutosaveBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the autosave counter moved %u -> %u across the quit "
					"-- quitting to the title must never autosave, and a FrontEnd capture "
					"would write a playerless, tagless world position over a good one",
					g_uQFAutosaveBefore, g_uQFAutosaveAfter);
				bPassed = false;
			}
			if (!g_bQFAutoSlotEmptyAtStart)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the Auto slot was not empty when the test started, "
					"so the EMPTY assertion below cannot attribute a file to the quit");
				bPassed = false;
			}
			if (g_uQFAutoSlotStatusAfter != (u_int)ZM_SAVE_SLOT_EMPTY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_QuitToFrontEnd] the Auto slot probed %u after the quit, expected %u "
					"(EMPTY) -- a save file was written on the way to the title screen",
					g_uQFAutoSlotStatusAfter, (u_int)ZM_SAVE_SLOT_EMPTY);
				bPassed = false;
			}
		}

		TearDownSaveResumeTest(g_bQFActive);
		g_bQFActive = false;

		return bPassed || !g_bQFPrereqs;
	}
}

static const Zenith_AutomatedTest g_xZMResumePlacementTest = {
	"ZM_ResumePlacement_Test",
	&Setup_ZMResumePlacement,
	&Step_ZMResumePlacement,
	&Verify_ZMResumePlacement,
	/* maxFrames */ 3000,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMResumePlacementTest);

static const Zenith_AutomatedTest g_xZMQuitToFrontEndTest = {
	"ZM_QuitToFrontEnd_Test",
	&Setup_ZMQuitToFrontEnd,
	&Step_ZMQuitToFrontEnd,
	&Verify_ZMQuitToFrontEnd,
	/* maxFrames */ 1200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMQuitToFrontEndTest);

#endif // ZENITH_INPUT_SIMULATOR
