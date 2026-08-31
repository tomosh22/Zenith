#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_LabRoundTrip (S8 SC-E) -- the whole lab door, walked.
//
// Dawnmere -> (LabDoorTrigger) -> ProfLab -> (ProfLabExitTrigger) -> Dawnmere,
// with REAL INPUT through both sensors and every build index and spawn tag read
// from the compiled world table rather than spelled.
//
// ★★ WHAT ONLY THIS TEST CAN SAY. Every other check on this seam looks at ONE
// side of it. The pure units compare compiled constants against compiled
// constants. Clause I4 of ZM_ProfLabWarp_Test reads the loaded ProfLab's exit
// sensor -- what it is POINTED at, not what happens when you walk into it. The
// committed-bytes needle proves a string is in Dawnmere.zscen. None of them
// executes the round trip, and the failure this slice exists to prevent is a
// RUNTIME state: ZM_GameStateManager::IsWarpDestinationValid consults ONLY the
// compiled ZM_WorldSpec tag list and NEVER the destination scene, so a warp to a
// tag no marker carries is ACCEPTED and the machine stalls in
// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN behind a fully opaque fade with the player
// frozen, until that barrier's frame budget expires and a Zenith_Error names the
// tag (ZM-D-200). A black screen that ends in an error, not a crash.
//
// ★ PHASE 1 IS THE TEST. It issues RequestWarp(<Dawnmere>, <the ProfLab exit's
// own tag>) from the playerless FrontEnd -- byte for byte the call the shipped
// exit sensor makes -- so if Dawnmere.zscen carries no marker with that tag, this
// test dies at PHASE 1's deadline with a message that NAMES WAITING_FOR_SPAWN.
// Everything after it is the walk that proves the two sensors actually fire.
//
// ★★ AND IT SKIPS ON CI, WHICH IS WHY IT IS NOT THE PROOF. Dawnmere needs its
// GITIGNORED terrain bake, so Setup RequestSkips on a tree without one -- and a
// skip counts as a PASS. This file therefore CANNOT be the gate for the seam. The
// two things that run on a bare CI checkout are named above and both were added
// alongside it: ZM_CommittedSceneBytes/DawnmereCarriesTheLabSeamMarkerAndTag (a
// boot unit over the tracked Dawnmere bytes) and clause I4 of ZM_ProfLabWarp_Test
// (which requires no terrain, no GPU and calls RequestSkip nowhere).
//
// C6 / ZM-D-147 SPLIT GUARD, stated because getting it backwards is the classic
// error: the THREE .zscen files this test loads are TRACKED, so their absence is
// a DEFECT and this test does NOT probe them and does NOT skip on them -- a skip
// counts as a PASS and would silence the gate exactly when it broke. The terrain
// family under Assets/Terrain/Dawnmere IS git-ignored, and that is the only thing
// guarded.
//
// C2 (fixed dt): 1/30, matching ZM_PlayerHomeRoundTrip_Test. This test drives real
// input through two sensors, and the deterministic 30 Hz presentation cadence is
// the one the shipped traversal tests walk at; physics still consumes its normal
// fixed substeps.
//
// STACK-FRAME RULE (ZM-D-141): per-phase driver functions behind a thin dispatch
// Step, and NOT ONE ZM_GameState local anywhere -- this test holds none, and if a
// later clause needs one it goes at FILE SCOPE. A monolithic /Od Step frame is the
// SUM of every local in every phase; ~six ZM_GameState locals measured 1,312,136
// bytes against a 1,048,576-byte reserve and died in __chkstk on the first call.
//
// EVERY placement value below is READ from Source/World/ZM_DawnmerePlacement.h or
// Source/World/ZM_ProfLabPlacement.h -- the same headers the ZENITH_TOOLS
// authoring reads -- and every build index and tag from ZM_GetWorldSpec. The only
// numeric literals here are tolerances and frame deadlines.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"   // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "FileAccess/Zenith_FileAccess.h"
#include "Input/Zenith_InputSimulator.h"
#include "Zenithmon/Tests/ZM_TestWalkDrive.h"   // the ONE walk driver -- read its header before trusting a straight line
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
	// See the C2 note in the header.
	constexpr float fLAB_FIXED_DT = 1.0f / 30.0f;

	// ---- Tolerances ---------------------------------------------------------
	//
	// ★ THE ARRIVAL TOLERANCES ARE DELIBERATELY LOOSE, AND THAT COSTS NOTHING.
	// The claim these serve is "the FromLab warp put the player AT THE LAB", and
	// the nearest other spawn in this scene is the TownCenter marker 134 m away --
	// so a metre and a half of slack discriminates by two orders of magnitude. They
	// are NOT trying to pin the placement: that is the boot units' job, against the
	// compiled constants, with no settling body in the way.
	//
	// They have to be loose, because what is measured here is a DYNAMIC CAPSULE a
	// few frames after a teleport onto graded terrain. The marker's Y is a FROZEN
	// raycast (the SC-D table), the body settles under gravity onto whatever the
	// live baked heightfield says, and the two are allowed to differ by the
	// centimetres a re-bake moves plus the millimetres a physics tick does.
	constexpr float fARRIVAL_XZ_TOLERANCE = 1.5f;
	constexpr float fARRIVAL_Y_TOLERANCE = 2.0f;

	// "This authored transform is the one the header derives." Tight, because both
	// sides are compiled values with no physics between them.
	constexpr float fPLACEMENT_EPSILON = 0.001f;

	// ---- Phase budgets ------------------------------------------------------
	//
	// ★ EVERY DEADLINE MUST FIRE BEFORE THE HARNESS CAP, or the diagnosis is
	// replaced by a bare "timed out" with no captured state -- which is precisely
	// the symptom a missing spawn marker produces if nothing names it. Sum:
	// 180 + 600 + 300 + 420 + 300 + 600 = 2400, inside the registered 3,000.
	//
	// ★★ THE DRIVE BUDGETS, AND WHY THIS TEST NEVER WALKS THE 147 m CORRIDOR.
	// The obvious shape for this test is "spawn at TownCenter (512, 480) and walk
	// to the lab pad (640, 552)" -- about 147 m, roughly 630 frames of clean
	// running at fRUN_SPEED on this cadence, and a route that passes within ~5 m of
	// the wanderer's patrol (540, 476..484) and near the clerk (526, 498).
	// DriveTowardXZ has NO obstacle avoidance and rival Vesper is a LIVE ARMED
	// TRAINER in this scene, so a hijack on that corridor surfaces as a timeout
	// naming a DISTANCE, not naming him.
	//
	// This test does not walk it, and does not need to: the FromLab warp lands the
	// player IN THE LAB FORECOURT at (640, 520), which is the shipped entry point
	// for exactly this journey. The two legs that remain are:
	//
	//   LEG A (Dawnmere, phase 3): the marker (640, 520) -> the staging waypoint
	//     ZM_GetDawnmereLabDoorStagingXZ() (640, 522) -> the sensor centre
	//     ZM_GetDawnmereLabDoorTargetXZ() (640, 525). 2.0 m + 3.0 m = 5.0 m, a pure
	//     +Z line with the camera at its authored yaw, i.e. the "single leg from
	//     rest" regime DriveTowardXZ is safe in. At fRUN_SPEED / 30 Hz that is
	//     5.0 / 7 * 30 = 21.4 frames of travel.
	//   LEG B (ProfLab, phase 5): the arrival marker (0, 5.0) -> the exit sensor's
	//     centre (0, 7.0). 2.0 m, again pure +Z. 8.6 frames of travel.
	//
	// Both are budgeted at 300 frames -- 14x and 35x the travel. The headroom is
	// not padding: each leg begins with a body that was TELEPORTED onto its marker
	// and has to settle under gravity, the fade-in tail runs before movement is
	// re-enabled, and the capsule accelerates rather than starting at run speed.
	// If either ever gets close to 300, something is holding the player, not
	// something is slow.
	constexpr int iLAB_BOOTSTRAP_DEADLINE = 180;
	constexpr int iLAB_DAWNMERE_ARRIVAL_DEADLINE = 600;   // + terrain streaming
	constexpr int iLAB_DOOR_DRIVE_DEADLINE = 300;         // LEG A
	constexpr int iLAB_PROFLAB_ARRIVAL_DEADLINE = 420;    // an interior, no terrain
	constexpr int iLAB_EXIT_DRIVE_DEADLINE = 300;         // LEG B
	constexpr int iLAB_RETURN_DEADLINE = 600;             // + terrain streaming

	// ---- The composed failure text ------------------------------------------
	//
	// ★ FILE SCOPE, AND SIZED. The results-JSON `failures` array is always empty,
	// so a composed string logged from Verify is the ONLY way a reader ever sees
	// why this failed. It is a file-scope buffer rather than a local because the
	// ZM-D-141 stack-frame rule applies to it as much as to anything else.
	char g_aszLabFailureDetail[1536] = {};
	const char* g_szLabFailure = "test did not reach verification";

	// ---- Phases + the state every clause reads ------------------------------

	enum class LabPhase
	{
		FrontEndBootstrap,
		WaitForDawnmere,
		DriveToLabDoor,
		WaitForProfLab,
		DriveToExit,
		WaitForReturn,
		Done,
	};

	LabPhase g_eLabPhase = LabPhase::Done;
	int g_iLabPhaseFrames = 0;
	bool g_bLabPrerequisitesPresent = false;
	bool g_bLabPassed = false;
	bool g_bLabDoorStagingReached = false;
	bool g_bLabDoorLatchSeen = false;
	bool g_bLabExitLatchSeen = false;
	Zenith_EntityID g_xLabManagerEntityID = INVALID_ENTITY_ID;

	void ClearLabInput()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
	}

	void FailLab(const char* szReason)
	{
		g_szLabFailure = szReason;
		g_bLabPassed = false;
		g_eLabPhase = LabPhase::Done;
		ClearLabInput();
	}

	void FailLabDetailed(const char* szFormat, ...)
	{
		va_list xArgs;
		va_start(xArgs, szFormat);
		std::vsnprintf(g_aszLabFailureDetail, sizeof(g_aszLabFailureDetail),
			szFormat, xArgs);
		va_end(xArgs);
		FailLab(g_aszLabFailureDetail);
	}

	// ---- The destinations, RESOLVED rather than spelled ---------------------
	//
	// Both sides of the seam read the compiled table, exactly as the authoring
	// does. A literal 2 / 41 / "FromLab" / "Door" here would be a second inventory
	// -- the mutation "the configure function wrote the wrong build index" would
	// then need two edits to stay green, which is the whole reason this test could
	// be trusted to catch it.
	u_int LabDawnmereBuildIndex()
	{
		return ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex;
	}

	u_int LabProfLabBuildIndex()
	{
		return ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex;
	}

	// ---- Scene-shaped views (the shipped WorldTraversal shape) --------------

	struct LabManagerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_GameStateManager* m_pxManager = nullptr;
		u_int m_uCount = 0u;
	};

	struct LabPlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	bool FindUniqueLabManager(LabManagerView& xOut)
	{
		xOut = LabManagerView{};
		g_xEngine.Scenes().QueryAllScenes<ZM_GameStateManager>().ForEach(
			[&xOut](Zenith_EntityID xEntityID, ZM_GameStateManager& xManager)
			{
				++xOut.m_uCount;
				if (xOut.m_uCount == 1u)
				{
					xOut.m_xEntityID = xEntityID;
					xOut.m_pxManager = &xManager;
				}
			});
		return xOut.m_uCount == 1u && xOut.m_pxManager != nullptr;
	}

	bool FindUniqueLabPlayer(LabPlayerView& xOut)
	{
		xOut = LabPlayerView{};
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
		return uCount == 1u && xOut.m_pxController != nullptr;
	}

	// ★ CAMERA-RELATIVE, exactly as ZM_AutoTests_WorldTraversal's DriveTowardXZ is
	// and for the identical reason: ZM_PlayerController moves along the MAIN
	// CAMERA'S flattened basis, so "W" means "the way the camera faces" and not
	// "+Z". Both legs here are single legs from rest at the authored yaw, where
	// this degenerates to the world-space chooser exactly -- but the general form
	// is written because the world-space one is correct only in that regime, and a
	// later leg added to this file would inherit a latent 45-degree heading bug.
	// The shared, unit-tested walk driver. See
	// Tests/ZM_TestWalkDrive.h -- it is CAMERA-RELATIVE and QUANTISED TO EIGHT
	// DIRECTIONS, so the player walks a PURSUIT CURVE rather than the straight
	// line, and anything this suite must approach ON A BEARING wants one of the
	// driver's fixed points. This wrapper exists only to keep the local name and
	// to mirror the held keys where this file reports them.
	void DriveLabTowardXZ(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		// ★ THE CLEAR STAYS HERE, AND DROPPING IT COST A BATCH. Consolidating the
		// driver moved this call out on the assumption that callers cleared before
		// driving; they do not -- every one of the eight originals cleared INSIDE,
		// and without it last frame's keys stay held and the walk curves away.
		// Six automated tests went red. The shared driver deliberately does not
		// clear, because each caller releases a DIFFERENT key set.
		ClearLabInput();
		const ZM_WalkDriveKeys xKeys =
			ZM_DriveWalkTowardXZ(xPosition, xTarget, /*bRun*/ true);
		(void)xKeys;
	}

	// ---- The ONE git-ignored prerequisite -----------------------------------
	//
	// ★ THE TRACKED .zscen FILES ARE DELIBERATELY NOT IN THIS LIST (ZM-D-147/148).
	// FrontEnd, Dawnmere and ProfLab are committed assets; their absence is a
	// DEFECT, and probing for them would convert that defect into a SKIP, which the
	// harness counts as a PASS. Only the git-ignored terrain family is guarded, and
	// a tree without it genuinely cannot run this test.
	//
	// FileExists plus a one-byte ReadPrefix, rather than a size query: a truncated
	// zero-byte bake exists but cannot be loaded, and ReadPrefix is the cheap
	// header-peek the file layer already offers (no std::filesystem, no allocation,
	// no reading a heightmap into memory to ask whether it is there).
	bool LabTerrainBakePresent()
	{
		static const char* const aszRequired[] = {
			GAME_ASSETS_DIR "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Physics_0_0" ZENITH_GEOMETRY_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_GEOMETRY_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Render_0_0" ZENITH_GEOMETRY_EXT,
		};
		const u_int uCount =
			(u_int)(sizeof(aszRequired) / sizeof(aszRequired[0]));
		for (u_int u = 0u; u < uCount; ++u)
		{
			unsigned char uProbe = 0u;
			if (!Zenith_FileAccess::FileExists(aszRequired[u])
				|| !Zenith_FileAccess::ReadPrefix(
					aszRequired[u], &uProbe, sizeof(uProbe)))
			{
				return false;
			}
		}
		return true;
	}

	// "The arriving body landed on this marker." XZ and Y are separated because
	// they carry different evidence: XZ is a teleport (exact to the millimetre, and
	// the thing that says WHICH spawn resolved), Y is a settled capsule on graded
	// terrain (see the tolerance block).
	bool LabBodyIsOnMarker(
		const Zenith_Maths::Vector3& xBody,
		const Zenith_Maths::Vector3& xMarkerFeet)
	{
		const float fDeltaX = xBody.x - xMarkerFeet.x;
		const float fDeltaZ = xBody.z - xMarkerFeet.z;
		const float fXZ = std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
		const float fDeltaY =
			xBody.y - (xMarkerFeet.y + fZM_HUMAN_BODY_HALF_HEIGHT);
		return fXZ <= fARRIVAL_XZ_TOLERANCE
			&& std::fabs(fDeltaY) <= fARRIVAL_Y_TOLERANCE;
	}

	// ---- PHASE 1: the playerless FrontEnd issues the FromLab warp -----------
	//
	// ★★ THIS PHASE IS THE POINT OF THE FILE. RequestWarp(<Dawnmere>, <the ProfLab
	// exit's own tag>) is byte for byte the call the shipped exit sensor makes, and
	// IsWarpDestinationValid will return TRUE for it whether or not any entity in
	// Dawnmere.zscen carries that tag -- it reads the compiled table and never the
	// scene. So an authoring change that dropped the FromLabSpawn marker leaves
	// this warp ACCEPTED and the machine stalled in WAITING_FOR_SPAWN for that
	// barrier's whole frame budget (ZM-D-200). The deadline below fires long before
	// that, and is the ONLY thing that turns it into a red test rather than a log
	// line nobody reads.
	bool LabPhaseFrontEndBootstrap()
	{
		const Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		LabManagerView xManager;
		if (!xScene.IsValid()
			|| g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex != 0
			|| !FindUniqueLabManager(xManager))
		{
			if (g_iLabPhaseFrames > iLAB_BOOTSTRAP_DEADLINE)
			{
				FailLabDetailed(
					"the FrontEnd never presented a unique ZM_GameStateManager in "
					"%d frames [activeBuildIndex=%d managerCount=%u]",
					iLAB_BOOTSTRAP_DEADLINE,
					g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex,
					xManager.m_uCount);
				return false;
			}
			return true;
		}

		if (!xManager.m_pxManager->IsAuthoritativeSingleton()
			|| xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
			|| g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().Count()
				!= 0u)
		{
			FailLab("the FrontEnd traversal authority was not idle and playerless");
			return false;
		}
		g_xLabManagerEntityID = xManager.m_xEntityID;

		const char* const szFromLabTag = ZM_GetProfLabExitSpawnTag();
		if (szFromLabTag == nullptr || szFromLabTag[0] == '\0')
		{
			FailLab(
				"the compiled ZM_SCENE_PROFLAB row carries no connection targeting "
				"ZM_SCENE_DAWNMERE, so there is no lab-return tag to warp to. Fix "
				"Source/Data/ZM_WorldSpec.cpp -- the whole seam is unbuildable");
			return false;
		}

		if (!ZM_GameStateManager::RequestWarp(
				LabDawnmereBuildIndex(), szFromLabTag)
			|| xManager.m_pxManager->GetTransitionState()
				!= ZM_WARP_TRANSITION_QUEUED
			|| xManager.m_pxManager->GetTargetBuildIndex()
				!= LabDawnmereBuildIndex()
			|| std::strcmp(
				xManager.m_pxManager->GetTargetSpawnTag(), szFromLabTag) != 0)
		{
			FailLabDetailed(
				"RequestWarp(%u, \"%s\") did not queue transactionally from the "
				"FrontEnd [state=%d target=%u tag='%s']",
				LabDawnmereBuildIndex(), szFromLabTag,
				(int)xManager.m_pxManager->GetTransitionState(),
				xManager.m_pxManager->GetTargetBuildIndex(),
				xManager.m_pxManager->GetTargetSpawnTag());
			return false;
		}

		g_eLabPhase = LabPhase::WaitForDawnmere;
		g_iLabPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 2: arrive at the lab forecourt ------------------------------
	bool LabPhaseWaitForDawnmere()
	{
		LabManagerView xManager;
		if (!FindUniqueLabManager(xManager)
			|| xManager.m_xEntityID != g_xLabManagerEntityID)
		{
			FailLab("the persistent traversal manager changed during the FromLab warp");
			return false;
		}

		const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		const int iBuild = g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
		if (iBuild == (int)LabDawnmereBuildIndex()
			&& xManager.m_pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
		{
			LabPlayerView xPlayer;
			if (!FindUniqueLabPlayer(xPlayer)
				|| !xPlayer.m_pxController->IsMovementEnabled())
			{
				if (g_iLabPhaseFrames > iLAB_DAWNMERE_ARRIVAL_DEADLINE)
				{
					FailLab("Dawnmere reached IDLE but never handed back a movable player");
					return false;
				}
				return true;
			}

			// ★ THE CLAUSE THAT SAYS WHICH MARKER RESOLVED. TownCenter is 134 m
			// from here, so landing on the wrong spawn is unmissable at this
			// tolerance -- and landing on the RIGHT one is the live proof that
			// Dawnmere.zscen really does carry a marker tagged for the lab return.
			const Zenith_Maths::Vector3 xMarkerFeet =
				ZM_GetDawnmereFromLabSpawnFeet();
			if (!LabBodyIsOnMarker(xPlayer.m_xPosition, xMarkerFeet))
			{
				FailLabDetailed(
					"the FromLab warp put the player at (%.3f, %.3f, %.3f), not on "
					"the lab arrival marker ZM_GetDawnmereFromLabSpawnFeet() = "
					"(%.3f, %.3f, %.3f) [XZ tolerance %.2f, Y tolerance %.2f]. If "
					"this reads like the town centre, some OTHER marker answered "
					"the '%s' tag",
					(double)xPlayer.m_xPosition.x, (double)xPlayer.m_xPosition.y,
					(double)xPlayer.m_xPosition.z,
					(double)xMarkerFeet.x, (double)xMarkerFeet.y,
					(double)xMarkerFeet.z,
					(double)fARRIVAL_XZ_TOLERANCE, (double)fARRIVAL_Y_TOLERANCE,
					ZM_GetProfLabExitSpawnTag());
				return false;
			}

			g_eLabPhase = LabPhase::DriveToLabDoor;
			g_iLabPhaseFrames = 0;
			return true;
		}

		if (g_iLabPhaseFrames > iLAB_DAWNMERE_ARRIVAL_DEADLINE)
		{
			// ★ THE MESSAGE THIS WHOLE FILE EXISTS TO PRINT.
			FailLabDetailed(
				"the FromLab warp never completed in %d frames [state=%d "
				"activeBuildIndex=%d fadeAlpha=%.3f]. IF state READS %d "
				"(ZM_WARP_TRANSITION_WAITING_FOR_SPAWN) THE DIAGNOSIS IS EXACT: "
				"Dawnmere.zscen carries no ZM_SpawnPoint tagged '%s'. "
				"IsWarpDestinationValid consults ONLY the compiled ZM_WorldSpec tag "
				"list and never the scene, so the warp was ACCEPTED and the machine "
				"is now waiting for a marker that does not exist. In the shipped game "
				"that is an opaque fade and a frozen player until that barrier's frame "
				"budget expires (ZM-D-200). Check that the FromLabSpawn authoring "
				"is still in the Dawnmere block of Zenithmon.cpp AND that the scene "
				"was re-authored by a WINDOWED *_True tools boot afterwards",
				iLAB_DAWNMERE_ARRIVAL_DEADLINE,
				(int)xManager.m_pxManager->GetTransitionState(),
				g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex,
				(double)xManager.m_pxManager->GetFadeAlpha(),
				(int)ZM_WARP_TRANSITION_WAITING_FOR_SPAWN,
				ZM_GetProfLabExitSpawnTag());
			return false;
		}
		return true;
	}

	// ---- PHASE 3 (LEG A): walk into the Dawnmere lab doorway ---------------
	bool LabPhaseDriveToLabDoor()
	{
		LabManagerView xManager;
		LabPlayerView xPlayer;
		if (!FindUniqueLabManager(xManager) || !FindUniqueLabPlayer(xPlayer))
		{
			FailLab("the Dawnmere manager or player vanished during the lab-door approach");
			return false;
		}

		// The sensor's live configuration, checked ONCE on entry to the phase. Its
		// destination is resolved from the compiled table, never spelled -- the
		// mutation "the Dawnmere trigger points at PlayerHome" is caught here.
		if (g_iLabPhaseFrames == 1)
		{
			Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
			Zenith_Entity xTrigger = pxData != nullptr
				? pxData->FindEntityByName(
					szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME)
				: Zenith_Entity();
			ZM_WarpTrigger* pxWarp = xTrigger.IsValid()
				? xTrigger.TryGetComponent<ZM_WarpTrigger>() : nullptr;
			Zenith_ColliderComponent* pxCollider = xTrigger.IsValid()
				? xTrigger.TryGetComponent<Zenith_ColliderComponent>() : nullptr;
			if (pxWarp == nullptr || pxCollider == nullptr
				|| !pxCollider->HasValidBody()
				|| pxWarp->GetTargetBuildIndex() != LabProfLabBuildIndex()
				|| std::strcmp(pxWarp->GetSpawnTag(), szZM_PROFLAB_SPAWN_TAG) != 0)
			{
				FailLabDetailed(
					"Dawnmere's '%s' is missing or misconfigured [found=%d warp=%d "
					"liveBody=%d target=%u expected=%u tag='%s' expected='%s']. "
					"Without it the lab is a building the player can never enter",
					szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME,
					xTrigger.IsValid() ? 1 : 0, pxWarp != nullptr ? 1 : 0,
					pxCollider != nullptr && pxCollider->HasValidBody() ? 1 : 0,
					pxWarp != nullptr ? pxWarp->GetTargetBuildIndex() : 0u,
					LabProfLabBuildIndex(),
					pxWarp != nullptr ? pxWarp->GetSpawnTag() : "<none>",
					szZM_PROFLAB_SPAWN_TAG);
				return false;
			}

			// ...and the shell really is standing there. A trigger with no building
			// around it warps the player into a lab whose exterior does not exist.
			Zenith_Entity xShell = pxData != nullptr
				? pxData->FindEntityByName(szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME)
				: Zenith_Entity();
			if (!xShell.IsValid())
			{
				FailLabDetailed(
					"Dawnmere carries no '%s' -- the lab's warp sensor stands in an "
					"empty field. (This name is also the key SC-D's ground-truth "
					"oracle uses to ignore the shell during its post-SC-E "
					"re-measure.)", szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME);
				return false;
			}
			Zenith_Maths::Vector3 xShellPosition(0.0f);
			xShell.GetComponent<Zenith_TransformComponent>()
				.GetPosition(xShellPosition);
			const ZM_DawnmereBlockout xExpectedShell = ZM_GetDawnmereLabShell();
			if (glm::length(xShellPosition - xExpectedShell.m_xCenter)
				> fPLACEMENT_EPSILON)
			{
				FailLabDetailed(
					"Dawnmere's '%s' stands at (%.4f, %.4f, %.4f) but "
					"ZM_GetDawnmereLabShell() derives (%.4f, %.4f, %.4f) -- the "
					"scene was not re-authored after the SC-D block changed",
					szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME,
					(double)xShellPosition.x, (double)xShellPosition.y,
					(double)xShellPosition.z,
					(double)xExpectedShell.m_xCenter.x,
					(double)xExpectedShell.m_xCenter.y,
					(double)xExpectedShell.m_xCenter.z);
				return false;
			}
		}

		if (xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE)
		{
			// The sensor fired. Everything below is the state it should have left.
			ClearLabInput();
			if (!g_bLabDoorStagingReached
				|| xManager.m_pxManager->GetTargetBuildIndex()
					!= LabProfLabBuildIndex()
				|| std::strcmp(xManager.m_pxManager->GetTargetSpawnTag(),
					szZM_PROFLAB_SPAWN_TAG) != 0
				|| xManager.m_pxManager->GetFrozenPlayerEntityID()
					!= xPlayer.m_xEntityID
				|| xPlayer.m_pxController->IsMovementEnabled())
			{
				FailLabDetailed(
					"the Dawnmere lab door fired but did not queue ProfLab cleanly "
					"[stagingReached=%d target=%u expected=%u tag='%s' frozen=%d "
					"movementEnabled=%d]",
					g_bLabDoorStagingReached ? 1 : 0,
					xManager.m_pxManager->GetTargetBuildIndex(),
					LabProfLabBuildIndex(),
					xManager.m_pxManager->GetTargetSpawnTag(),
					xManager.m_pxManager->GetFrozenPlayerEntityID()
						== xPlayer.m_xEntityID ? 1 : 0,
					xPlayer.m_pxController->IsMovementEnabled() ? 1 : 0);
				return false;
			}
			g_bLabDoorLatchSeen = true;
			g_eLabPhase = LabPhase::WaitForProfLab;
			g_iLabPhaseFrames = 0;
			return true;
		}

		// LEG A. Align on the doorway first, then step +Z into the sensor -- the
		// same two-waypoint shape the shipped Home approach uses, and for the same
		// reason: DriveTowardXZ has no obstacle avoidance and the entrance face is
		// solid 2 m past the sensor.
		const Zenith_Maths::Vector3 xStaging = ZM_GetDawnmereLabDoorStagingXZ();
		if (!g_bLabDoorStagingReached)
		{
			constexpr float fSTAGING_TOLERANCE = 0.5f;
			g_bLabDoorStagingReached =
				std::fabs(xPlayer.m_xPosition.x - xStaging.x) <= fSTAGING_TOLERANCE
				&& std::fabs(xPlayer.m_xPosition.z - xStaging.z)
					<= fSTAGING_TOLERANCE;
			DriveLabTowardXZ(xPlayer.m_xPosition, xStaging);
		}
		else
		{
			DriveLabTowardXZ(
				xPlayer.m_xPosition, ZM_GetDawnmereLabDoorTargetXZ());
		}

		if (g_iLabPhaseFrames > iLAB_DOOR_DRIVE_DEADLINE)
		{
			FailLabDetailed(
				"LEG A timed out after %d frames: the player stands at "
				"(%.3f, %.3f, %.3f), %.3f m from the staging waypoint "
				"(%.3f, _, %.3f) [stagingReached=%d]. The whole leg is %.1f m of "
				"pure +Z from the arrival marker, roughly %.0f frames of travel at "
				"run speed on this cadence, so a timeout here is something HOLDING "
				"the player rather than something slow -- a mis-authored sensor "
				"height, or geometry between the marker and the door",
				iLAB_DOOR_DRIVE_DEADLINE,
				(double)xPlayer.m_xPosition.x, (double)xPlayer.m_xPosition.y,
				(double)xPlayer.m_xPosition.z,
				(double)std::sqrt(
					(xPlayer.m_xPosition.x - xStaging.x)
						* (xPlayer.m_xPosition.x - xStaging.x)
					+ (xPlayer.m_xPosition.z - xStaging.z)
						* (xPlayer.m_xPosition.z - xStaging.z)),
				(double)xStaging.x, (double)xStaging.z,
				g_bLabDoorStagingReached ? 1 : 0,
				(double)(ZM_GetDawnmereLabDoorTargetXZ().z
					- ZM_GetDawnmereFromLabSpawnFeet().z),
				(double)((ZM_GetDawnmereLabDoorTargetXZ().z
					- ZM_GetDawnmereFromLabSpawnFeet().z)
					/ ZM_PlayerController::fRUN_SPEED / fLAB_FIXED_DT));
			return false;
		}
		return true;
	}

	// ---- PHASE 4: arrive inside the lab ------------------------------------
	bool LabPhaseWaitForProfLab()
	{
		LabManagerView xManager;
		if (!FindUniqueLabManager(xManager)
			|| xManager.m_xEntityID != g_xLabManagerEntityID)
		{
			FailLab("the persistent traversal manager changed during the ProfLab load");
			return false;
		}

		const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		const int iBuild = g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
		if (iBuild == (int)LabProfLabBuildIndex()
			&& xManager.m_pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
		{
			LabPlayerView xPlayer;
			if (!FindUniqueLabPlayer(xPlayer)
				|| !xPlayer.m_pxController->IsMovementEnabled())
			{
				if (g_iLabPhaseFrames > iLAB_PROFLAB_ARRIVAL_DEADLINE)
				{
					FailLab("ProfLab reached IDLE but never handed back a movable player");
					return false;
				}
				return true;
			}

			const Zenith_Maths::Vector3 xMarkerFeet = ZM_GetProfLabSpawnFeet();
			if (!LabBodyIsOnMarker(xPlayer.m_xPosition, xMarkerFeet))
			{
				FailLabDetailed(
					"the lab door put the player at (%.3f, %.3f, %.3f), not on "
					"ProfLab's arrival marker (%.3f, %.3f, %.3f)",
					(double)xPlayer.m_xPosition.x, (double)xPlayer.m_xPosition.y,
					(double)xPlayer.m_xPosition.z,
					(double)xMarkerFeet.x, (double)xMarkerFeet.y,
					(double)xMarkerFeet.z);
				return false;
			}

			g_eLabPhase = LabPhase::DriveToExit;
			g_iLabPhaseFrames = 0;
			return true;
		}

		if (g_iLabPhaseFrames > iLAB_PROFLAB_ARRIVAL_DEADLINE)
		{
			FailLabDetailed(
				"the Dawnmere lab door never delivered the player into ProfLab in "
				"%d frames [state=%d activeBuildIndex=%d]. FIRST SUSPECT: a missing "
				"RegisterSceneBuildIndex(%u, ...) line in Project_LoadInitialScene",
				iLAB_PROFLAB_ARRIVAL_DEADLINE,
				(int)xManager.m_pxManager->GetTransitionState(),
				g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex,
				LabProfLabBuildIndex());
			return false;
		}
		return true;
	}

	// ---- PHASE 5 (LEG B): walk back out through the exit sensor -------------
	bool LabPhaseDriveToExit()
	{
		LabManagerView xManager;
		LabPlayerView xPlayer;
		if (!FindUniqueLabManager(xManager) || !FindUniqueLabPlayer(xPlayer))
		{
			FailLab("the ProfLab manager or player vanished during the exit approach");
			return false;
		}

		if (xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE)
		{
			ClearLabInput();
			// ★ THE RECIPROCAL OF PHASE 1. The sensor fired and it is asking for
			// the SAME (build index, tag) pair the compiled connection declares --
			// which is what makes the return leg below a real test of the marker
			// rather than a repeat of the warp phase 1 already issued by hand.
			if (xManager.m_pxManager->GetTargetBuildIndex()
					!= LabDawnmereBuildIndex()
				|| std::strcmp(xManager.m_pxManager->GetTargetSpawnTag(),
					ZM_GetProfLabExitSpawnTag()) != 0
				|| xPlayer.m_pxController->IsMovementEnabled())
			{
				FailLabDetailed(
					"the ProfLab exit fired but queued (%u, '%s') instead of the "
					"connection's (%u, '%s') [movementEnabled=%d]",
					xManager.m_pxManager->GetTargetBuildIndex(),
					xManager.m_pxManager->GetTargetSpawnTag(),
					LabDawnmereBuildIndex(), ZM_GetProfLabExitSpawnTag(),
					xPlayer.m_pxController->IsMovementEnabled() ? 1 : 0);
				return false;
			}
			g_bLabExitLatchSeen = true;
			g_eLabPhase = LabPhase::WaitForReturn;
			g_iLabPhaseFrames = 0;
			return true;
		}

		// LEG B: one straight +Z step onto the sensor's centre. No staging waypoint
		// -- the aperture is 6 m wide, the arrival marker is on its centreline, and
		// there is nothing between the two.
		const ZM_ProfLabBlockout xSensor = ZM_GetProfLabExitTrigger();
		DriveLabTowardXZ(xPlayer.m_xPosition, xSensor.m_xCenter);

		if (g_iLabPhaseFrames > iLAB_EXIT_DRIVE_DEADLINE)
		{
			FailLabDetailed(
				"LEG B timed out after %d frames: the player stands at "
				"(%.3f, %.3f, %.3f) and never reached the exit sensor centred at "
				"(%.3f, %.3f, %.3f). The leg is %.2f m of pure +Z, roughly %.0f "
				"frames of travel -- so this is a sensor that is NOT FIRING rather "
				"than a walk that is slow. Check that ProfLabExitTrigger still "
				"carries a live static AABB body and a ZM_WarpTrigger (clause I4 of "
				"ZM_ProfLabWarp_Test checks the same thing without walking)",
				iLAB_EXIT_DRIVE_DEADLINE,
				(double)xPlayer.m_xPosition.x, (double)xPlayer.m_xPosition.y,
				(double)xPlayer.m_xPosition.z,
				(double)xSensor.m_xCenter.x, (double)xSensor.m_xCenter.y,
				(double)xSensor.m_xCenter.z,
				(double)(xSensor.m_xCenter.z - fZM_PROFLAB_SPAWN_Z),
				(double)((xSensor.m_xCenter.z - fZM_PROFLAB_SPAWN_Z)
					/ ZM_PlayerController::fRUN_SPEED / fLAB_FIXED_DT));
			return false;
		}
		return true;
	}

	// ---- PHASE 6: back out onto the marker, closing the loop ---------------
	bool LabPhaseWaitForReturn()
	{
		LabManagerView xManager;
		if (!FindUniqueLabManager(xManager)
			|| xManager.m_xEntityID != g_xLabManagerEntityID)
		{
			FailLab("the persistent traversal manager changed during the return warp");
			return false;
		}

		const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		const int iBuild = g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
		if (iBuild == (int)LabDawnmereBuildIndex()
			&& xManager.m_pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
		{
			LabPlayerView xPlayer;
			if (!FindUniqueLabPlayer(xPlayer)
				|| !xPlayer.m_pxController->IsMovementEnabled())
			{
				if (g_iLabPhaseFrames > iLAB_RETURN_DEADLINE)
				{
					FailLab("the return to Dawnmere reached IDLE but handed back no movable player");
					return false;
				}
				return true;
			}

			const Zenith_Maths::Vector3 xMarkerFeet =
				ZM_GetDawnmereFromLabSpawnFeet();
			if (!LabBodyIsOnMarker(xPlayer.m_xPosition, xMarkerFeet))
			{
				FailLabDetailed(
					"the ProfLab exit returned the player to (%.3f, %.3f, %.3f), "
					"not to the lab arrival marker (%.3f, %.3f, %.3f)",
					(double)xPlayer.m_xPosition.x, (double)xPlayer.m_xPosition.y,
					(double)xPlayer.m_xPosition.z,
					(double)xMarkerFeet.x, (double)xMarkerFeet.y,
					(double)xMarkerFeet.z);
				return false;
			}

			if (!g_bLabDoorLatchSeen || !g_bLabExitLatchSeen)
			{
				FailLabDetailed(
					"the round trip completed without both sensors being observed "
					"firing [labDoorLatch=%d exitLatch=%d] -- one of the two legs "
					"was warped rather than walked",
					g_bLabDoorLatchSeen ? 1 : 0, g_bLabExitLatchSeen ? 1 : 0);
				return false;
			}

			g_bLabPassed = true;
			g_eLabPhase = LabPhase::Done;
			return false;
		}

		if (g_iLabPhaseFrames > iLAB_RETURN_DEADLINE)
		{
			FailLabDetailed(
				"the ProfLab exit never delivered the player back to Dawnmere in "
				"%d frames [state=%d activeBuildIndex=%d]. IF state READS %d "
				"(WAITING_FOR_SPAWN) the exit's '%s' tag resolved to nothing in the "
				"loaded Dawnmere -- the marker is missing, mis-tagged, or the scene "
				"predates the SC-E authoring. The shipped game sits here behind an "
				"opaque fade until that barrier's frame budget expires (ZM-D-200), "
					"far later than this deadline",
				iLAB_RETURN_DEADLINE,
				(int)xManager.m_pxManager->GetTransitionState(),
				g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex,
				(int)ZM_WARP_TRANSITION_WAITING_FOR_SPAWN,
				ZM_GetProfLabExitSpawnTag());
			return false;
		}
		return true;
	}

	// ---- Setup / dispatch / Verify -----------------------------------------

	void Setup_LabRoundTrip()
	{
		g_eLabPhase = LabPhase::Done;
		g_iLabPhaseFrames = 0;
		g_bLabPassed = false;
		g_bLabDoorStagingReached = false;
		g_bLabDoorLatchSeen = false;
		g_bLabExitLatchSeen = false;
		g_szLabFailure = "test did not reach verification";
		g_aszLabFailureDetail[0] = '\0';
		g_xLabManagerEntityID = INVALID_ENTITY_ID;
		g_bLabPrerequisitesPresent = LabTerrainBakePresent();
		Zenith_InputSimulator::ResetAllInputState();

		// RequestSkip bypasses Verify, so no fixed dt or other process state may be
		// installed until the git-ignored prerequisite is known present.
		if (!g_bLabPrerequisitesPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"the git-ignored Dawnmere terrain bake is absent");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fLAB_FIXED_DT);
		g_eLabPhase = LabPhase::FrontEndBootstrap;
	}

	bool Step_LabRoundTrip(int)
	{
		if (!g_bLabPrerequisitesPresent || g_eLabPhase == LabPhase::Done)
		{
			return false;
		}
		++g_iLabPhaseFrames;
		switch (g_eLabPhase)
		{
		case LabPhase::FrontEndBootstrap: return LabPhaseFrontEndBootstrap();
		case LabPhase::WaitForDawnmere:   return LabPhaseWaitForDawnmere();
		case LabPhase::DriveToLabDoor:    return LabPhaseDriveToLabDoor();
		case LabPhase::WaitForProfLab:    return LabPhaseWaitForProfLab();
		case LabPhase::DriveToExit:       return LabPhaseDriveToExit();
		case LabPhase::WaitForReturn:     return LabPhaseWaitForReturn();
		case LabPhase::Done:              return false;
		}
		return false;
	}

	bool Verify_LabRoundTrip()
	{
		ClearLabInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		if (!g_bLabPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_LabRoundTrip] %s", g_szLabFailure);
		}
		return g_bLabPassed;
	}
}   // close the anonymous namespace BEFORE the registration

static const Zenith_AutomatedTest g_xZMLabRoundTripTest = {
	"ZM_LabRoundTrip_Test",
	&Setup_LabRoundTrip,
	&Step_LabRoundTrip,
	&Verify_LabRoundTrip,
	/* maxFrames */ 3000,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMLabRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
