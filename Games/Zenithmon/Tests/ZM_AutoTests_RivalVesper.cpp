#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_RivalVesper -- the windowed/headless gate for S7 item 3 SC8 and
// S7 item 4: THE AUTHORED RIVAL, off the committed scene bytes.
//
// Two independent gameplay tests are m_bRequiresGraphics = FALSE (they assert
// nothing about pixels, so they run FOR REAL on the Null backend):
//   * ZM_RivalVesperAuthored_Test proves the authored rival, visual SPOTTED beat,
//     and win/payout path.
//   * ZM_RivalVesperWhiteout_Test proves the exact-starter loss, heal, warp, and
//     no-immediate-retrigger path.
// ★ A THIRD, GRAPHICS-REQUIRED TEST USED TO LIVE AT THE TAIL OF THIS TU AND WAS
// DELETED AT ZM-D-181. ZM_NpcRenderedPalette_Test read actual swapchain pixels off
// the six NPC bodies and required all 15 RGB separations to clear a floor derived
// (at ZM-D-171) from PALETTE-COLOURED BLOCKS. Those bodies are generated human
// MODELS now, wearing baked textures, so the quantity it measured no longer exists
// and its constant described a picture the game does not draw. It was removed
// rather than re-baselined: a floor guessed against new content would be a number
// that LOOKS like a check. If a pixel-level "the six read as six different people"
// gate is wanted back, it must be DERIVED the way ZM-D-171 derived the old one --
// run it, read the separations, run the severed-wiring mutation, read that band,
// and set the floor strictly between the two.
//
// ★ THE APPEARANCE BLOCK IN THE FIRST TEST asserts on COMPONENT STATE, not on
// pixels, so it runs for real headless -- and it is what survives. It is split by
// the SAME ZM_AreHumanAssetsReady() call the runtime made: WARM, every NPC must
// wear the .zmodel its own ZM_NpcData row names; COLD, every NPC must wear its
// row's palette colour on the fallback block. Either way the complement holds
// unconditionally -- walls, floors, doors and lintels must still be EXACTLY the
// shipped blockout grey. The two halves are deliberately complementary: deleting
// the wiring reds the first, painting everything reds the second, and neither can
// pass on its own.
//
// WHAT IS NEW HERE, and why it is not a copy of ZM_TrainerSightWalkUp_Test. That
// test PLACES a transient trainer at runtime and calls ConfigureTrainerSight. THIS
// FILE CONTAINS NO ConfigureTrainerSight CALL AT ALL -- that absence is grep-
// provable and it is the claim: the trainer standing in Dawnmere came out of
// Dawnmere.zscen plus the compiled ZM_NpcData row, with nobody arming him.
//
// The shipped walk-up test is NOT re-pointed at this rival and must not be: its
// phases 7a2/8/9 rotate the trainer and reconfigure it onto the rambler row, which
// an authored scene entity must never be made to do. That file keeps its runtime
// fixture and gains only a silence clause about this one.
//
// ★ S7 ITEM 1 SC3 ADDED THE WALK, AND IT IS THE ONLY PLACE THE RE-AUTHORED BODY
// CAN BE SHOWN TO MATTER. Phase (6b) sits between the visual beat and the bark: the
// player is FROZEN and holding no keys, and the gap to the rival's LIVE transform
// must still close, end inside the standoff band, and hand movement back afterwards.
// Every clause there is measured off his live pose rather than off the authored
// coordinates, because the whole claim is that he left them. Revert
// ZM_QueueDawnmereTrainerNpc to OBB/STATIC and re-author, and (6b) is what reds --
// no boot unit can, because a static body is a property of the SAVED BYTES.
//
// Phases (12)-(14) then buy two things a successful walk cannot: (13) CANCELS a walk
// mid-flight and requires the freeze to come off (arrival is the exit every happy
// run takes; cancel is the exit that strands a player forever when it is forgotten),
// and (14) deliberately ENDS THIS TEST while a walk is still running, leaving
// ZM_TrainerCinematicLatch ARMED so the between-tests hook in Zenithmon.cpp finally
// has something to clear. Do not "tidy" (14) by resetting the latch in this test's
// teardown -- that moves the proof back inside the test and re-hides the hook.
//
// WHAT THIS FILE DOES NOT PROVE, stated so nobody reads more into it:
//   * The authored rival carries NO graph slot. That is pinned at AUTHORING TIME
//     by ZM_ConfigureRivalVesperNpc's TryGetComponent<Zenith_GraphComponent>()
//     == nullptr assert, and it CANNOT be pinned here: EnsureTrainerChallengeGraph
//     is idempotent by path, so an authored slot and the runtime attach both yield
//     GetGraphCount() == 1 and are indistinguishable at runtime.
// The first test deliberately over-levels its lead so the payout proof is stable;
// the second test separately keeps the exact full-health L5 starter and drives a
// deterministic losing choice. Keeping the outcomes separate makes neither one
// conditional on a balance coin flip.
//
// Per-phase driver functions, never one monolithic Step (the ZM-D-141 stack rule),
// and EVERY component pointer is re-resolved each frame (pools relocate on
// swap-and-pop). NO Zenith_EventDispatcher::ScopedTestIsolation -- it would steal
// the live subscription tables and delete the subscriber under test.
//
// ★ THE GRAPH ASSET IS GITIGNORED, AND IT IS A PER-CLAUSE FLAG, NOT A TEST
// PREREQUISITE. game:Graphs/ZM_TrainerChallenge.bgraph is written by a tools boot,
// so on a fresh CI checkout it is simply absent. Folding it into the whole-test
// prerequisite (as an earlier revision did) would SKIP this entire test there --
// and a skip is counted as a PASS -- hiding SC8's actual centrepiece (authored
// placement, zero-byte persistence, the prize payout) behind an asset only the two
// bark phases need. So Setup records g_bRVBarkAssetPresent SEPARATELY and only the
// bark block in Verify consults it; everything else RUNS AND ASSERTS with no
// .bgraph on disk.
//
// That is safe because the engine itself fails OPEN here, by design and by
// contract: EnsureTrainerChallengeGraph appends an UNRESOLVED slot for a missing
// asset, RunTrainerChallenge then speaks to nobody, and the FSM's CHALLENGING
// window raises the encounter anyway (ZM_TrainerSightFsmTuning::
// m_fChallengeConfirmSeconds, whose polarity is deliberately the opposite of the
// raise window's for exactly this reason). No bark, same battle -- so the approach
// simply routes straight through the transition check.
// ============================================================================

#include "AssetHandling/Zenith_AssetRegistry.h"          // ResolvePath -- the bark asset's disk location
#include "AssetHandling/Zenith_MaterialAsset.h"          // the W4 appearance sample (GetName / GetBaseColor)
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"   // the W4 appearance sample
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "EntityComponent/Zenith_CameraResolve.h"       // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "Input/Zenith_InputSimulator.h"
#include "Zenithmon/Tests/ZM_TestWalkDrive.h"   // the ONE walk driver -- read its header before trusting a straight line
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Physics/Zenith_Physics.h"
#include "Zenithmon/Components/ZM_BattleDirector.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"               // the bark's screen model (GetTopScreen / IsMenuOpen)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"      // ZM_SetInstantBattlesForTests
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"                   // ZM_GetNpcData -- the rival's appearance row (W4)
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"                // ZM_IsStoryFlagSet
#include "Zenithmon/Source/Data/ZM_TrainerData.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"            // the W4 greybox appearance palette
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"           // szZM_GRAPH_TRAINER_CHALLENGE_ASSET
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"    // the latch + the observed state
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"  // the SC3 cone, for the spawn-camp poll
#include "Zenithmon/Source/Interaction/ZM_TrainerSightProbe.h"  // the occlusion filter, for the stall breakdown
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"                  // ZM_BuildMonsterRecord
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"            // ZM_ApplyStarterChoice / ZM_STARTER_CHOICE_FERNFAWN
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"        // the AUTHORED coordinates, shared with the boot units
#include "Zenithmon/Source/World/ZM_HumanAssetPolicy.h"          // the SAME warm/cold question the runtime asked

#ifdef ZENITH_TOOLS
#include "Core/Zenith_EditorQuery.h"
#endif

#include "Core/Zenith_TestTGA.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
	// -------------------------------------------------------------------------
	// Constants
	// -------------------------------------------------------------------------

	// THE SINGLE DIFFICULTY KNOB, exactly as ZM_TrainerSightWalkUp_Test spells it.
	// Vesper's authored lead is a L5 KINDLET whose type favours the ENEMY, so the
	// 55-level gap is what makes the win reliable. If the player ever fails to win,
	// RAISE this -- never weaken an assertion. The LOSS path is deliberately out of
	// scope (see the header): an honest loss here would be an untested branch
	// dressed up as a flake.
	constexpr ZM_SPECIES_ID eRV_PLAYER_LEAD_SPECIES = ZM_SPECIES_FERNFAWN;
	constexpr u_int         uRV_PLAYER_LEAD_LEVEL   = 60u;
	constexpr ZM_TRAINER_ID eRV_TRAINER             = ZM_TRAINER_RIVAL_VESPER;
	constexpr u_int         uRV_EXPECTED_PRIZE      = 500u;

	// Dawnmere's build index (ZM_WorldSpec / ZM-D-012).
	constexpr int   iRV_OVERWORLD_BUILD_INDEX = 2;
	// Coarser than 1/60 for the same reason the shipped sight gate is: the trip has
	// to fit two full scene loads + an additive battle load + the battle inside the
	// frame budget.
	constexpr float fRV_FIXED_DT              = 1.0f / 30.0f;

	// The spawn-camp margin, spelled against the SHIPPED tuning rather than against
	// the placement header, so only ONE side of the claim can move at a time.
	constexpr float fRV_MIN_SPAWN_SEPARATION = fZM_SIGHT_MAX_DISTANCE * 1.25f;

	// ---- Phase budgets. Each phase owns a deadline that FAILS with a diagnostic;
	// the harness's maxFrames is only a backstop above their sum. ----
	constexpr int iRV_READY_DEADLINE             = 420;
	constexpr int iRV_RELOAD_DEADLINE            = 420;
	// One quiet frame after a load before anything is re-resolved (mirrors the
	// GameState suite's post-load settle).
	constexpr int iRV_POST_LOAD_SETTLE_FRAMES    = 4;
	constexpr int iRV_BASIS_FRAMES               = 30;
	constexpr int iRV_APPROACH_DEADLINE          = 900;
	constexpr int iRV_SPOTTED_DEADLINE           = 60;
	constexpr int iRV_CHALLENGE_HOLD_FRAMES      = 30;
	// The overworld typewriter is a hard constexpr 45 chars/sec and
	// ZM_SetInstantBattlesForTests does NOTHING for a bark, so a bark costs real
	// frames. Same budget the shipped sight gate proved sufficient.
	constexpr int iRV_CHALLENGE_DISMISS_DEADLINE = 180;
	// ECS order 112 (ZM_UI_MenuStack) pops, closes and unfreezes before order 113
	// (ZM_Interactable) dispatches, in the SAME frame.
	constexpr int iRV_BARK_TO_BATTLE_DEADLINE    = 2;
	constexpr int iRV_INBATTLE_DEADLINE          = 600;
	constexpr int iRV_DRIVE_DEADLINE             = 900;
	constexpr int iRV_SETTLE_FRAMES              = 8;

	// The approach must keep CLOSING; a second of no improvement means stuck
	// geometry / wrong basis / oscillation, and the test says so immediately.
	constexpr int   iRV_STALL_LIMIT_FRAMES = 60;
	constexpr float fRV_STALL_IMPROVEMENT  = 0.01f;
	// The basis probe must show meaningful, +Z-DOMINANT travel.
	constexpr float fRV_BASIS_MIN_FORWARD  = 0.5f;

	// ---- S7 item 1 SC3: THE TRAINER'S OWN WALK -------------------------------
	//
	// ★ NAMED, AND DELIBERATELY NOT iRV_APPROACH_DEADLINE. That name is already
	// taken by the PLAYER's walk-up above; this is the window the TRAINER gets to
	// close the gap once he has spotted anyone, and confusing the two would produce a
	// diagnostic naming the wrong actor. The FSM caps the walk at
	// m_fApproachTimeoutSeconds (2.0 s == 60 frames at this test's 1/30 dt), so this
	// is that budget times three: a slow frame must never be diagnosed by the
	// harness's maxFrames backstop as "an ordinary stall".
	constexpr int iRV_TRAINER_APPROACH_DEADLINE = 180;
	// How long the release may take once the walk ends. The hold is dropped inside
	// the same TickTrainerSight that leaves APPROACHING, so this is slack against a
	// slow frame rather than a race.
	constexpr int iRV_CINE_RELEASE_DEADLINE = 30;
	// The re-arm walk-back: out past the sight range so the ENGAGED machine re-arms
	// to WATCHING, then back in to the band below. Generous -- it is two short walks
	// on a player who has just come out of a battle.
	constexpr int iRV_REARM_DEADLINE = 420;
	// The band the re-armed walk must start from: comfortably inside the cone (so he
	// spots at once) and comfortably outside the standoff ring (so the walk he then
	// takes is long enough to be caught MID-flight rather than arriving instantly).
	constexpr float fRV_REARM_MIN_GAP = 5.0f;
	constexpr float fRV_REARM_MAX_GAP = 6.5f;
	// How far past the sight range the walk-back must reach before the machine is
	// judged to have re-armed. 15% of margin against a spring-lagged camera basis.
	constexpr float fRV_REARM_CLEAR_GAP = fZM_SIGHT_MAX_DISTANCE * 1.15f;

	// The approach must CLOSE, and this is what "closed" means. One frame of the
	// trainer's own walk at the shipped speed covers ~0.13 m at this dt, so a
	// requirement of 0.5 m over the whole window is a floor on "he actually moved",
	// not a per-frame rate.
	constexpr float fRV_TRAINER_MIN_CLOSURE = 0.5f;
	// Per-frame monotonicity slack. The trainer walks on Jolt velocity across real
	// terrain, so a single frame may show float-scale non-improvement; anything
	// beyond this is him going BACKWARDS, which is exactly the M2 signature
	// (transposed ZM_StepTrainerApproach arguments).
	constexpr float fRV_TRAINER_BACKSTEP_SLACK = 0.02f;

	// ---- WHERE THE FINISHED WALK MUST STOP -----------------------------------
	//
	// ★ THIS IS DERIVED FROM THE SHIPPED STANDOFF, NEVER TYPED BESIDE IT. The first
	// revision of this file hard-coded 1.500 m next to an
	// m_fApproachStandoffMetres of 2.0 m, which made a PERFECT arrival fail: the
	// trainer stopped exactly where he was told to and the assertion said he had
	// not. Two numbers describing one ring is the defect; there is now one number
	// and a tolerance around it.
	//
	// ★ THE TOLERANCE IS A MEASURED FRAME-ORDERING ARTEFACT, NOT A FUDGE.
	// Zenith_Core.cpp's frame order is
	//     PumpAutomatedTest (this test's Step)  ->  Physics().Update
	//     ->  Zenith_SyncPhysicsTransforms  ->  Scenes().Update (the component tick)
	// so this test ALWAYS reads a pose one integrated frame behind the last
	// measurement ZM_Interactable made. The trainer covers
	// fWALK_SPEED * fRV_FIXED_DT = 0.133 m in that frame, which is exactly the
	// 2.105 m this clause observed for a walk whose own final measurement was
	// 1.972 m -- i.e. INSIDE the 2.0 m ring. Two frames of it, plus 0.25 m for
	// contact response against a second capsule on real terrain, is the honest
	// window; anything wider would stop distinguishing an arrival from a timeout,
	// which is what the elapsed clause below is for.
	//
	// fWALK_SPEED is named rather than re-typed because it IS the constant
	// ZM_Interactable drives the walk with (fZM_TRAINER_APPROACH_SPEED), so the
	// test's arithmetic and the game's speed cannot drift apart.
	constexpr float fRV_TRAINER_FRAME_TRAVEL =
		ZM_PlayerController::fWALK_SPEED * fRV_FIXED_DT;
	constexpr float fRV_STANDOFF_TOLERANCE =
		fRV_TRAINER_FRAME_TRAVEL * 2.0f + 0.25f;

	// ---- S7 item 1 SC3 / risk R1: THE ANTI-SHOVE BOUNDS ----------------------
	//
	// RIGIDBODY_TYPE has no KINEMATIC, so the authored rival now stands on a body the
	// player can physically lean on. These two bounds are what turn "he might drift"
	// from a worry into a measurement.
	//
	// (a) BEFORE he has ever walked, he must be ON his authored anchor. A dynamic
	//     capsule that slides, settles sideways or gets shoved shows up here and
	//     nowhere else -- the placement clause in phase (1) samples ONE frame at load.
	constexpr float fRV_IDLE_DRIFT_TOLERANCE = 0.35f;
	// (b) ONCE he has walked, his displacement is bounded by the geometry of the
	//     walk itself: he only ever moves toward a target inside his own cone. Twice
	//     the sight range covers the three approaches this test drives with room to
	//     spare, and still reds a rival who has been pushed across the plaza.
	constexpr float fRV_WALKED_DRIFT_TOLERANCE = fZM_SIGHT_MAX_DISTANCE * 2.0f;

	// THE COUPLING TOLERANCES between the committed scene bytes and the compiled
	// placement header. Both are deliberately tight: the authoring writes these
	// exact values, so anything but float round-trip noise means the file and the
	// header have drifted.
	constexpr float fRV_PLACEMENT_TOLERANCE = 0.01f;    // metres
	constexpr float fRV_FACING_MIN_ABS_DOT  = 0.999f;   // |dot| of two unit quats

	// ---- Known-limit W4: the AUTHORED APPEARANCE sample ----------------------
	//
	// Keyed on the MATERIAL NAME, because ZM_GreyboxVisual is a file-local class in
	// Zenithmon.cpp and cannot be named from a test TU. Both populations wear a
	// material called this; what differs is the base colour they were given.
	const char* const szRV_GREYBOX_MATERIAL = "ZM_Greybox";
	// Dawnmere authors 4 blockout blocks and 6 NPC bodies; the cap is slack, and the
	// sample counts anything it had to DROP so a truncated scan reds rather than
	// quietly reporting a clean subset.
	constexpr u_int uRV_MAX_SAMPLED_GREYBOX_BODIES = 64u;
	// The palette is a compiled pure function, so the sampled material colour must
	// match it to float noise, not "approximately".
	constexpr float fRV_PALETTE_TOLERANCE = 1.0e-4f;

	// ---- The bark ordering pin's UNRESOLVED sentinels ------------------------
	//
	// DISTINCT PER SAMPLE POINT, deliberately. The pin is an EQUALITY test between
	// the pre-walk encounter count and the count on the frame the bark was observed;
	// if both samples fell back to the SAME sentinel, a run in which the
	// ZM_BattleTransition singleton never resolved at either point would compare a
	// sentinel against itself and PASS having measured nothing.
	constexpr u_int uRV_ENCOUNTERS_UNRESOLVED_BEFORE  = 0xfffffffeu;
	constexpr u_int uRV_ENCOUNTERS_UNRESOLVED_AT_BARK = 0xffffffffu;

	enum class RVPhase
	{
		AwaitReady,        // (0)
		ResolveAuthored,   // (1)  THE CENTREPIECE
		ReloadDawnmere,    // (2)  the persistence proof
		InstallLead,       // (3)
		BasisProbe,        // (4)
		Approach,          // (5)
		AwaitSpotted,      // (6)  the W3 visual beat
		// S7 item 1 SC3. INSERTED between the visual beat and the bark: the TRAINER's
		// own walk, which is the sub-commit's centrepiece and the only place a
		// re-authored DYNAMIC CAPSULE can be shown to be load-bearing.
		TrainerApproach,   // (6b)
		AwaitChallenge,    // (7)
		DismissChallenge,  // (8)
		AwaitInBattle,     // (9)
		DriveMenu,         // (10)
		Settle,            // (11)
		// S7 item 1 SC3. APPENDED after the shipped round trip, so none of the
		// clauses above can be affected by them. They exist for the two things a
		// single successful walk cannot show: that the freeze is RELEASED on the
		// CANCEL path, and that a test which dies mid-cinematic leaks a freeze the
		// between-tests hook has to clear.
		ReArmWalkBack,     // (12)
		CancelMidApproach, // (13)
		LeakMidApproach,   // (14)
		Done,
	};

	// -------------------------------------------------------------------------
	// Entity views + asset guards (file-local copies -- the originals live in other
	// tests' anonymous namespaces and are not linkable across TUs).
	// -------------------------------------------------------------------------

	struct RVPlayerView
	{
		Zenith_EntityID           m_xEntityID    = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3     m_xPosition    = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController*      m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider   = nullptr;
	};

	struct RVCameraView
	{
		Zenith_EntityID         m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera*        m_pxFollow  = nullptr;
		Zenith_CameraComponent* m_pxCamera  = nullptr;
	};

	bool FindActivePlayer(RVPlayerView& xOut)
	{
		xOut = RVPlayerView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
			});
		return uCount == 1u;
	}

	bool FindActiveCamera(RVCameraView& xOut)
	{
		xOut = RVCameraView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_FollowCamera,
			Zenith_CameraComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_FollowCamera& xFollow,
				Zenith_CameraComponent& xCamera)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxFollow = &xFollow;
				xOut.m_pxCamera = &xCamera;
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool ActiveGrassIsReady()
	{
		bool bReady = false;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&bReady](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				// A Null (headless) build never APPLIES grass -- the blades are
				// GPU-only content the backend deliberately does not author. The
				// component still reaches its terminal inert state, which is what
				// this gate is really asking: has the overworld finished coming up?
				bReady = bReady || xGrass.IsGrassApplied() || Zenith_IsNullRenderer();
			});
		return bReady;
	}

	bool DawnmereRuntimeReady(RVPlayerView& xPlayer, RVCameraView& xCamera)
	{
		return FindActivePlayer(xPlayer)
			&& FindActiveCamera(xCamera)
			&& xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController->IsGrounded()
			&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
			&& xCamera.m_pxFollow->GetCurrentArmDistance() > 0.0f
			&& ActiveGrassIsReady();
	}

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
		const std::array<std::string, 7> astrRequired = {
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

	// Re-resolved FRESH every frame: component pools relocate entries on
	// swap-and-pop, so these pointers must never be cached.
	ZM_BattleTransition* ResolveSingletonBattleTransition()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_BattleTransition::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_BattleTransition>()
			: nullptr;
	}

	// The persistent ZM_MenuRoot's stack. Same re-resolve discipline, same reason.
	ZM_UI_MenuStack* ResolveMenuStack()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_UI_MenuStack>()
			: nullptr;
	}

	// The top screen, or ZM_MENU_SCREEN_NONE when nothing is raised. Every bark
	// clause is keyed on this MODEL, never on a rendered glyph count: this test runs
	// for real on the Null backend.
	ZM_MENU_SCREEN RVTopScreen()
	{
		const ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
		return (pxMenu != nullptr) ? pxMenu->GetTopScreen() : ZM_MENU_SCREEN_NONE;
	}

	// -------------------------------------------------------------------------
	// Control state (ALL reset in Setup; batch mode reuses the process)
	// -------------------------------------------------------------------------

	RVPhase     g_eRVPhase          = RVPhase::Done;
	int         g_iRVPhaseFrames    = 0;
	bool        g_bRVActive         = false;
	bool        g_bRVFailed         = false;
	bool        g_bRVPrereqsPresent = false;
	bool        g_bRVPrimitivesOptionSaved = false;
	bool        g_bRVPrimitivesEnabledBefore = true;
	const char* g_szRVFailure       = "test did not reach verification";
	// Backing store for the failure messages that have to carry a MEASUREMENT (the
	// feet-height delta, and -- since S7 item 1 SC3 -- the four walk-up / cancel /
	// leak diagnostics). FailRV takes a plain const char* and NOTHING in this file
	// takes a format, so a measured message is snprintf'd in here first and the
	// pointer handed over; the text then has to live for the rest of the run.
	//
	// ★ ONE BUFFER IS ENOUGH, AND IT IS NOT LUCK: FailRV drives the machine to
	// RVPhase::Done and latches g_bRVFailed, and every caller returns false
	// immediately, so Step never runs another phase. Exactly one measured failure
	// can ever be written. If a future clause ever needs to record a measurement
	// WITHOUT ending the run, it needs its own buffer, not this one.
	char        g_szRVMeasuredFailure[512] = { '\0' };

	// ---- (1) THE AUTHORED RESOLUTION ----
	bool                  g_bRVFirstResolved        = false;
	u_int                 g_uRVFirstCount           = 0u;
	Zenith_EntityID       g_xRVFirstEntityID        = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xRVVesperPosition       = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Quat    g_xRVVesperRotation       = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	ZM_TRAINER_ID         g_eRVFirstTrainer         = ZM_TRAINER_NONE;
	bool                  g_bRVFirstSightEnabled    = false;
	bool                  g_bRVFirstInteractable    = false;
	float                 g_fRVPlacementErrorX      = 0.0f;
	float                 g_fRVPlacementErrorZ      = 0.0f;
	float                 g_fRVFacingAbsDot         = 0.0f;
	float                 g_fRVSpawnSeparation      = 0.0f;
	bool                  g_bRVSpawnOutsideCone     = false;
	// ★ THE MEASUREMENT that turns Zenithmon.cpp's standing "height is an
	// assumption" caveat into an observation for this ONE npc.
	float                 g_fRVHeightDelta          = 0.0f;

	// ---- (1b) KNOWN-LIMIT W4: THE AUTHORED APPEARANCE ----
	// Every sentinel FAILS CLOSED, and the POLARITY of each is chosen for that: the
	// two "how many did I see" counters start at 0 (Verify wants > 0), the two "how
	// many were wrong" counters start at 0xffffffff (Verify wants exactly 0), the two
	// separations start NEGATIVE (Verify wants >= a positive margin), and the palette
	// ERROR starts HUGE (Verify wants <= a tiny tolerance -- a negative default there
	// would have passed a run that measured nothing). A run in which the scan never
	// happened therefore cannot satisfy a single clause.
	bool                  g_bRVAppearanceSampled     = false;
	u_int                 g_uRVAppearanceOverflow    = 0xffffffffu;   // want 0
	u_int                 g_uRVGreyboxBlockCount     = 0u;            // want > 0
	u_int                 g_uRVGreyboxBlockOffGrey   = 0xffffffffu;   // want 0
	u_int                 g_uRVNpcVisualCount        = 0u;            // want > 1
	u_int                 g_uRVNpcVisualStillGrey    = 0xffffffffu;   // want 0
	u_int                 g_uRVNpcColoursSampled     = 0u;            // want > 0 (excludes the rival)
	// WARM/COLD split: which population the runtime actually built, and how many
	// NPCs carry the model their own row names. Recorded from the SAME policy call
	// the runtime made, never inferred from what the scan happened to find.
	bool                  g_bRVHumansWarm            = false;
	u_int                 g_uRVNpcModelsCorrect      = 0u;            // want == g_uRVNpcVisualCount when warm
	bool                  g_bRVVesperVisualFound     = false;
	Zenith_Maths::Vector4 g_xRVVesperSampledColour   = Zenith_Maths::Vector4(-1.0f);
	Zenith_Maths::Vector4 g_xRVVesperExpectedColour  = Zenith_Maths::Vector4(-1.0f);
	float                 g_fRVVesperPaletteError    = 1.0e9f;   // want <= tolerance
	float                 g_fRVVesperMinNpcSeparation = -1.0f;  // want >= the margin
	float                 g_fRVVesperGreySeparation  = -1.0f;   // want >= the margin

	// ---- (2) THE RELOAD ----
	bool            g_bRVReloadIssued    = false;
	bool            g_bRVSecondResolved  = false;
	u_int           g_uRVSecondCount     = 0u;
	Zenith_EntityID g_xRVSecondEntityID  = INVALID_ENTITY_ID;
	ZM_TRAINER_ID   g_eRVSecondTrainer   = ZM_TRAINER_NONE;
	u_int           g_uRVSecondState     = 0xffffffffu;   // want WATCHING
	u_int           g_uRVSecondRaise     = 0xffffffffu;   // want 0
	u_int           g_uRVSecondSpotted   = 0xffffffffu;   // want 0
	u_int           g_uRVSecondIndicator = 0xffffffffu;   // want 0
	bool            g_bRVEntityIDChanged = false;

	// ---- (3) baselines. "after" money defaults to 0xffffffff so an unsampled run
	//      FAILS rather than passing on 0 == 0. ----
	bool  g_bRVBaselineCaptured = false;
	u_int g_uRVMoneyBefore      = 0u;
	bool  g_bRVFlagBefore       = true;    // want false; a true here IS the vacuity failure
	bool  g_bRVLatchBefore      = true;    // want false; same reason
	bool  g_bRVLeadInstalled    = false;
	u_int g_uRVEncountersBeforeWalk = uRV_ENCOUNTERS_UNRESOLVED_BEFORE;
	bool  g_bRVEncountersBeforeWalkResolved = false;

	// ---- (4)/(5) walk diagnostics ----
	Zenith_Maths::Vector3 g_xRVBasisStart(0.0f);
	float g_fRVBasisDeltaX     = 0.0f;
	float g_fRVBasisDeltaZ     = 0.0f;
	bool  g_bRVBasisPassed     = false;
	float g_fRVBestDistance    = 0.0f;
	float g_fRVCurrentDistance = 0.0f;
	int   g_iRVStallFrames     = 0;
	bool  g_abRVHeldKeys[4]    = { false, false, false, false };   // W A S D
	// ANTI-SPAWN-CAMP: a raise observed while the player was still further away than
	// the shipped sight range by a real margin means the pass is not a walk-up.
	bool  g_bRVEarlyRaiseSeen  = false;
	float g_fRVEarlyRaiseDistance = 0.0f;

	// ---- (6) THE VISUAL SPOTTED BEAT ----
	// The indicator submit sample is captured while the FSM is still SPOTTED.
	// Therefore zero challenge/raise counts and idle UI/transition on that SAME
	// sample prove the marker preceded both the bark and the encounter.
	bool  g_bRVSpottedObserved          = false;
	bool  g_bRVSpottedCompleted         = false;
	// ★ THE PIXEL HALF OF SHORTFALLS 1.8-3c. Everything else about the marker is a
	// statement about Flux's CPU queues; this is the only thing that can speak for
	// the framebuffer. The dump is requested from inside a REAL SPOTTED frame (see
	// RVObserveSpottedFrame) with Graphics/Primitives/Enabled held FALSE for the
	// whole run, so a marker that reaches the swapchain here reaches it off the
	// gameplay channel and not off the tools/debug one. Vulkan only:
	// Flux_Screenshot::RequestDump has exactly one consumer
	// (Zenith_Vulkan_Swapchain::EndFrame), so on the Null backend it writes nothing
	// and the assertion is correctly skipped rather than failed.
	bool        g_bRVMarkerShotRequested = false;
	std::string g_strRVMarkerShotPath;
	bool  g_bRVIndicatorObserved        = false;
	bool  g_bRVSpottedMenuIdleAtSubmit  = false;
	bool  g_bRVSpottedTransitionIdleAtSubmit = false;
	u_int g_uRVSpottedCount             = 0xffffffffu;   // want 1
	u_int g_uRVSpottedIndicatorSubmits  = 0xffffffffu;   // want > 0
	u_int g_uRVSpottedRaiseAtSubmit     = 0xffffffffu;   // want 0
	u_int g_uRVSpottedChallengeAtSubmit = 0xffffffffu;   // want 0
	u_int g_uRVSpottedFramesObserved    = 0u;            // want > 0
	float g_fRVSpottedElapsedAtSubmit   = -1.0f;

	// ---- (6b) S7 item 1 SC3: THE TRAINER'S OWN WALK -------------------------
	// EVERY sentinel FAILS CLOSED. The two "did it happen" bools start false, the
	// closure starts NEGATIVE (Verify wants a real positive distance), the final gap
	// starts HUGE (Verify wants it inside the standoff band), the counts start at
	// 0xffffffff, and the two "was the player frozen" clauses are counted rather than
	// latched -- a single sampled frame would be satisfied by a freeze that arrived
	// late and left early.
	bool  g_bRVTrainerApproachSeen     = false;
	bool  g_bRVTrainerApproachEnded    = false;
	u_int g_uRVTrainerApproachFrames   = 0u;         // want > 0
	u_int g_uRVTrainerFrozenFrames     = 0u;         // want == frames observed
	u_int g_uRVTrainerHeldFrames       = 0u;         // want == frames observed
	float g_fRVTrainerStartGap         = -1.0f;      // want > the standoff band
	float g_fRVTrainerEndGap           = 1.0e9f;     // want <= the standoff band
	float g_fRVTrainerWorstBackstep    = 0.0f;       // want <= the slack
	u_int g_uRVTrainerApproachCount    = 0xffffffffu;// want 1 at the end of the walk
	// ★ HOW THE WALK ENDED, MEASURED RATHER THAN INFERRED. ZM_TrainerSightFsm
	// deliberately does NOT clear m_fApproachElapsed on completion (only the cancel
	// arm clears it), and its header says in as many words that this is "what lets a
	// unit prove the arrival beat the clock". The first revision of this file did not
	// sample it and instead printed "the walk ran out on the FAIL-OPEN timeout" from
	// a distance clause that could not tell the two apart -- naming a cause it had
	// never observed. Sentinel is NEGATIVE so an unsampled run cannot look like a
	// fast arrival.
	float g_fRVTrainerElapsedAtEnd     = -1.0f;      // want 0 < elapsed < the timeout
	// The RELEASE half of the paired positive: movement back on, hold down, latch
	// down -- observed AFTER the walk rather than assumed from its end.
	bool  g_bRVReleaseObserved         = false;
	int   g_iRVReleaseFrames           = -1;

	// ---- R1: the anti-shove drift bounds, sampled EVERY frame ---------------
	bool  g_bRVDriftSampled     = false;
	float g_fRVIdleDriftMax     = 0.0f;   // want <= fRV_IDLE_DRIFT_TOLERANCE
	float g_fRVWalkedDriftMax   = 0.0f;   // want <= fRV_WALKED_DRIFT_TOLERANCE
	float g_fRVWatchFacingMinDot = 2.0f;  // want >= fRV_FACING_MIN_ABS_DOT

	// ---- (12)/(13)/(14) the re-arm, the CANCEL, and the deliberate leak ------
	bool  g_bRVReArmCleared        = false;
	bool  g_bRVReArmed             = false;
	float g_fRVReArmGap            = -1.0f;
	bool  g_bRVCancelApproachSeen  = false;
	bool  g_bRVCancelFrozenAtEntry = false;
	bool  g_bRVCancelHeldAtEntry   = false;
	bool  g_bRVCancelIssued        = false;
	bool  g_bRVCancelReleased      = false;
	int   g_iRVCancelReleaseFrames = -1;
	u_int g_uRVCancelApproachCount = 0xffffffffu;   // want 2
	bool  g_bRVLeakApproachSeen    = false;
	bool  g_bRVLeakFrozen          = false;
	bool  g_bRVLeakHeld            = false;
	bool  g_bRVLeakLatchArmed      = false;
	u_int g_uRVLeakApproachCount   = 0xffffffffu;   // want 3

	// ---- (7)/(8) THE BARK ----
	// ★ NOT part of g_bRVPrereqsPresent. This one flag gates the BARK CLAUSES in
	// Verify and nothing else; see the file header for why a whole-test skip on a
	// gitignored .bgraph would have been a pass that proved nothing.
	bool  g_bRVBarkAssetPresent     = false;
	bool  g_bRVBarkObserved         = false;
	bool  g_bRVBarkMissed           = false;
	bool  g_bRVBarkTopWasDialogue   = false;
	bool  g_bRVBarkStateChallenging = false;
	bool  g_bRVBarkTransitionIdle   = false;
	u_int g_uRVBarkRaiseCount       = 0xffffffffu;   // want 0: the battle has NOT started
	u_int g_uRVBarkChallengeCount   = 0xffffffffu;   // want 1: the beat ran exactly once
	u_int g_uRVEncountersAtBark     = uRV_ENCOUNTERS_UNRESOLVED_AT_BARK;
	bool  g_bRVEncountersAtBarkResolved = false;
	bool  g_bRVBarkHoldCompleted    = false;
	bool  g_bRVDismissClosed        = false;
	int   g_iRVDismissFrames        = -1;
	bool  g_bRVBarkToBattleOK       = false;
	int   g_iRVBarkToBattleFrames   = -1;

	// ---- (9)/(10)/(11) battle + payout ----
	bool          g_bRVChannelCaptured = false;
	ZM_TRAINER_ID g_eRVChannelTrainer  = ZM_TRAINER_NONE;
	bool          g_bRVReachedInBattle = false;
	bool  g_bRVAfterCaptured  = false;
	u_int g_uRVMoneyAfter     = 0xffffffffu;
	bool  g_bRVFlagAfter      = false;
	bool  g_bRVWhiteoutAfter  = true;      // want false (a WIN never latches a whiteout)
	u_int g_uRVCompletedAfter = 0xffffffffu;
	u_int g_uRVAbortedAfter   = 0xffffffffu;
	u_int g_uRVStateAfter     = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;   // want IDLE
	bool  g_bRVBattleUnloaded = false;
	bool  g_bRVSettleResolved = false;
	u_int g_uRVSettleSpotted   = 0xffffffffu;   // want 1
	u_int g_uRVSettleIndicator = 0xffffffffu;   // want the observed submit count
	u_int g_uRVSettleRaise     = 0xffffffffu;   // want 1
	u_int g_uRVSettleChallenge = 0xffffffffu;   // want 1

	void ClearRVInput()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
		g_abRVHeldKeys[0] = false;
		g_abRVHeldKeys[1] = false;
		g_abRVHeldKeys[2] = false;
		g_abRVHeldKeys[3] = false;
	}

	void FailRV(const char* szReason)
	{
		g_szRVFailure = szReason;
		g_bRVFailed = true;
		g_eRVPhase = RVPhase::Done;
		ClearRVInput();
	}

	float PlanarDistance(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	// |dot| of two rotations. ABSOLUTE because q and -q are the SAME rotation, so a
	// sign-flipped-but-identical quaternion must not read as a placement drift.
	// Spelled out component-wise rather than through a library call so the
	// convention is visible at the one site that depends on it.
	float RVFacingAbsDot(
		const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
	{
		const float fDot = xA.w * xB.w + xA.x * xB.x + xA.y * xB.y + xA.z * xB.z;
		return std::fabs(fDot);
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
		ClearRVInput();
		const ZM_WalkDriveKeys xKeys =
			ZM_DriveWalkTowardXZ(xPosition, xTarget, /*bRun*/ true);
		xKeys.CopyTo(g_abRVHeldKeys);
	}

	// Resolves the ONE ZM_Interactable in the active scene whose npc row is the
	// rival's. Re-resolved every frame; returns the COUNT so a phase can assert
	// EXACTLY one rather than "at least one".
	//
	// It keys on GetNpcId(), never on the entity NAME (name-string refs evade greps)
	// and never on GetTrainerId() (that is the thing under test -- keying on it
	// would make every derivation clause self-satisfying).
	u_int FindAuthoredVesper(Zenith_EntityID& xIDOut,
		Zenith_Maths::Vector3& xPositionOut, Zenith_Maths::Quat& xRotationOut,
		ZM_TRAINER_ID& eTrainerOut, u_int& uRaiseOut, u_int& uChallengeOut)
	{
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_Interactable, Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xID, ZM_Interactable& xInteractable,
				Zenith_TransformComponent& xTransform)
			{
				if (xInteractable.GetNpcId() != ZM_NPC_RIVAL_VESPER)
				{
					return;
				}
				++uCount;
				if (uCount != 1u) { return; }
				xIDOut = xID;
				xTransform.GetPosition(xPositionOut);
				xTransform.GetRotation(xRotationOut);
				eTrainerOut     = xInteractable.GetTrainerId();
				uRaiseOut       = xInteractable.GetTrainerSightRaiseCount();
				uChallengeOut   = xInteractable.GetTrainerChallengeCount();
			});
		return uCount;
	}

	// The rival's live component, by the id phase (2) resolved. Same re-resolve
	// discipline as everything else here.
	const ZM_Interactable* ResolveVesperComponent()
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_xRVSecondEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_Interactable>()
			: nullptr;
	}

	// ★ HIS LIVE POSE, RE-READ EVERY FRAME. Before SC3 the rival never moved, so
	// g_xRVVesperPosition (captured once at load) was the whole truth. He now WALKS,
	// and measuring the walk against the position he started from would report a
	// gap that closes only because the PLAYER moved -- which is precisely the
	// mistake the trainer-approach phase exists to rule out.
	bool RVResolveVesperPose(
		Zenith_Maths::Vector3& xPositionOut, Zenith_Maths::Quat& xRotationOut)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_xRVSecondEntityID);
		Zenith_TransformComponent* pxTransform = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_TransformComponent>()
			: nullptr;
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xPositionOut);
		pxTransform->GetRotation(xRotationOut);
		return true;
	}

	// The player's movement bool, through the same unique-player resolve every other
	// clause here uses. Answers false when there is no resolvable player, which is
	// the FAIL-CLOSED direction for the "he was frozen" half and is caught separately
	// by the phase's own player-lost clause.
	bool RVPlayerMovementEnabled()
	{
		RVPlayerView xPlayer;
		return FindActivePlayer(xPlayer)
			&& xPlayer.m_pxController != nullptr
			&& xPlayer.m_pxController->IsMovementEnabled();
	}

	// ★ THE R1 SAMPLER, CALLED ON EVERY FRAME OF THE WHOLE RUN rather than at phase
	// boundaries. The defect it exists to catch -- a DYNAMIC capsule shoved off its
	// anchor, or yawed by a collision into permanent blindness -- is live state that
	// heals itself the moment the pushing body moves away, so a boundary sample can
	// easily miss it entirely. Costs one transform read per frame.
	void RVSampleVesperDrift()
	{
		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
		if (!RVResolveVesperPose(xPosition, xRotation))
		{
			return;
		}
		g_bRVDriftSampled = true;

		const float fDeltaX = xPosition.x - fZM_DAWNMERE_VESPER_X;
		const float fDeltaZ = xPosition.z - fZM_DAWNMERE_VESPER_Z;
		const float fDrift = std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
		if (fDrift > g_fRVWalkedDriftMax)
		{
			g_fRVWalkedDriftMax = fDrift;
		}

		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		const bool bHasWalked = pxVesper != nullptr
			&& pxVesper->GetTrainerApproachCount() > 0u;
		if (!bHasWalked && fDrift > g_fRVIdleDriftMax)
		{
			// The IDLE bound only applies before he has ever been asked to move: after
			// that, displacement is content rather than damage.
			g_fRVIdleDriftMax = fDrift;
		}

		// The WATCHING facing repair, measured. A rival who is watching must be
		// pointing where the committed bytes said, whatever has bumped into him since
		// -- and it is sampled ONLY in WATCHING because every other state legitimately
		// owns his facing (the walk authors it outright).
		if (pxVesper != nullptr
			&& pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_WATCHING)
		{
			const float fDot = RVFacingAbsDot(xRotation, ZM_DawnmereVesperFacing());
			if (fDot < g_fRVWatchFacingMinDot)
			{
				g_fRVWatchFacingMinDot = fDot;
			}
		}
	}

	// The planar gap between the player and the rival's LIVE position.
	bool RVLiveTrainerGap(float& fGapOut)
	{
		RVPlayerView xPlayer;
		Zenith_Maths::Vector3 xVesperPosition(0.0f);
		Zenith_Maths::Quat xVesperRotation(1.0f, 0.0f, 0.0f, 0.0f);
		if (!FindActivePlayer(xPlayer)
			|| !RVResolveVesperPose(xVesperPosition, xVesperRotation))
		{
			return false;
		}
		fGapOut = PlanarDistance(xPlayer.m_xPosition, xVesperPosition);
		return true;
	}

	// Set or clear the rival's defeat flag on the live game state. Returns whether it
	// took: the re-arm phases are worthless if the write silently missed.
	bool RVSetRivalDefeatFlag(bool bSet)
	{
		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState)
			|| pxGameState == nullptr)
		{
			return false;
		}
		ZM_SetStoryFlag(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED, bSet);
		return ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED) == bSet;
	}

	// ★ THE ORDERING PROOF, latched on the FIRST frame the challenge dialogue is
	// observed. All pins are captured AT ONCE and on that one frame, because the
	// mutation they exist to catch -- transposing the two arms of the action switch
	// in ZM_Interactable::TickTrainerSight -- still delivers the battle. ONLY the
	// order is evidence.
	void RVLatchBarkObservation()
	{
		g_bRVBarkObserved = true;
		g_bRVBarkTopWasDialogue = RVTopScreen() == ZM_MENU_SCREEN_DIALOGUE;

		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper != nullptr)
		{
			g_bRVBarkStateChallenging =
				pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_CHALLENGING;
			g_uRVBarkRaiseCount = pxVesper->GetTrainerSightRaiseCount();
			g_uRVBarkChallengeCount = pxVesper->GetTrainerChallengeCount();
		}

		g_bRVBarkTransitionIdle = !ZM_BattleTransition::IsTransitionActive();
		const ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		// The RESOLVED-BOOL is the load-bearing half: without it an unresolved sample
		// here would leave the sentinel in place, and comparing that against an
		// equally unresolved pre-walk sample would pass the ordering pin having
		// measured nothing.
		g_bRVEncountersAtBarkResolved = pxTransition != nullptr;
		g_uRVEncountersAtBark = (pxTransition != nullptr)
			? pxTransition->GetObservedEncounterCount()
			: uRV_ENCOUNTERS_UNRESOLVED_AT_BARK;
	}

	// Sample one ACTUAL SPOTTED frame off the authored component. The submit count
	// advances only after both marker primitives have been queued, so observing it
	// here while challenge/raise/UI/transition are all still idle is the ordering
	// proof: the visual beat happened first, rather than being inferred later from a
	// battle that could have bypassed it.
	bool RVObserveSpottedFrame(const ZM_Interactable& xVesper)
	{
		g_bRVSpottedObserved = true;
		++g_uRVSpottedFramesObserved;
		// Frame-exact, because a wall-clock screen-grab loop cannot be. Measured
		// 2026-07-29: Tools\capture_viewport.ps1 asked for 40 ms and delivered
		// 206 ms at 2560x1440 (PNG encode dominates the loop), so a 0.35 s beat
		// gets 1-2 samples and "I did not capture it" carries no information. This
		// asks the swapchain for THIS frame instead.
		if (!g_bRVMarkerShotRequested && !g_strRVMarkerShotPath.empty())
		{
			Flux_Screenshot::RequestDump(g_strRVMarkerShotPath.c_str());
			g_bRVMarkerShotRequested = true;
		}
		g_uRVSpottedCount = xVesper.GetTrainerSpottedCount();
		g_uRVSpottedIndicatorSubmits =
			xVesper.GetTrainerSpottedIndicatorSubmitCount();

		const u_int uRaise = xVesper.GetTrainerSightRaiseCount();
		const u_int uChallenge = xVesper.GetTrainerChallengeCount();
		const bool bMenuIdle = RVTopScreen() == ZM_MENU_SCREEN_NONE;
		const bool bTransitionIdle = !ZM_BattleTransition::IsTransitionActive();
		if (g_uRVSpottedCount != 1u || g_uRVSpottedIndicatorSubmits == 0u
			|| uRaise != 0u || uChallenge != 0u || !bMenuIdle
			|| !bTransitionIdle)
		{
			FailRV("the authored SPOTTED frame did not submit its indicator before "
				"dialogue/challenge/encounter activity");
			return false;
		}

		if (!g_bRVIndicatorObserved)
		{
			g_bRVIndicatorObserved = true;
			g_bRVSpottedMenuIdleAtSubmit = bMenuIdle;
			g_bRVSpottedTransitionIdleAtSubmit = bTransitionIdle;
			g_uRVSpottedRaiseAtSubmit = uRaise;
			g_uRVSpottedChallengeAtSubmit = uChallenge;
			g_fRVSpottedElapsedAtSubmit =
				xVesper.GetTrainerSpottedElapsedSeconds();
		}
		return true;
	}

	// ★ KNOWN-LIMIT W4. Sample the LIVE material every blockout body in the
	// committed Dawnmere is wearing, and split it into the two populations the
	// wiring exists to keep apart:
	//   * entities WITH a resolvable ZM_NpcData row -- which must wear that row's
	//     appearance and must NOT still be grey;
	//   * entities WITHOUT one (the DawnmereHome shell, its two door leaves and its
	//     lintel) -- which must be EXACTLY the shipped blockout grey, byte for byte.
	//
	// The two halves are what make each other non-vacuous. Delete the wiring and the
	// NPC half reds while the block half stays green; paint EVERYTHING from the
	// palette and the block half reds while the NPC half stays green. Neither
	// mistake can pass, and neither clause can pass alone.
	//
	// It reads the material off Zenith_ModelComponent rather than off
	// ZM_GreyboxVisual, which is file-local to Zenithmon.cpp and unnameable here, and
	// it keys the NPC/non-NPC split on GetNpcId() -- never on the colour it is about
	// to judge.
	//
	// ★ THE SCAN IS DAWNMERE-SCOPED, AND SINCE ZM-D-176 THAT IS LOAD-BEARING. It
	// walks the ACTIVE scene, and PlayerHome is never active in this test -- so
	// the "must be EXACTLY the shipped grey" half above is a claim about
	// DAWNMERE'S blockout bodies only. It cannot see, and must not be read as
	// covering, PlayerHome's seven shell blocks, which now deliberately wear the
	// warm ZM-D-176 interior tint. The two greybox interiors are covered by
	// ZM_InteriorTint_Test (Tests/ZM_AutoTests_InteriorTint.cpp), which loads
	// PlayerHome and ProfLab in turn and judges each against its own expected
	// colour. Nothing about this file changed for ZM-D-176 except this note.
	void RVSampleAuthoredAppearance()
	{
		const Zenith_Maths::Vector4 xFallback = ZM_GetHumanPaletteFallbackColour();
		// The COMPILED expectation, resolved through the same row the component reads.
		const Zenith_Maths::Vector4 xVesperExpected =
			ZM_GetHumanPaletteColour(ZM_GetNpcData(ZM_NPC_RIVAL_VESPER).m_eHuman);

		// ★ THE SAME QUESTION THE RUNTIME ASKED. Whether an NPC wears a model or the
		// cold-start block is decided by ZM_AreHumanAssetsReady, so this test asks it
		// too rather than inferring the answer from what it happens to find -- an
		// inference would turn "the wiring is severed" into "ah, must be cold".
		const bool bWarm = ZM_AreHumanAssetsReady();
		g_bRVHumansWarm = bWarm;

		// ---- HALF ONE: the blockout bodies ----------------------------------
		// COLLECT INSIDE THE QUERY, RESOLVE OUTSIDE IT -- the
		// Zenith_GraphComponent::BroadcastCustomEvent idiom
		// (Zenith_GraphComponent.cpp gathers receiver ids first, then resolves each).
		// Nothing but the material read happens under the iterator.
		Zenith_EntityID axIDs[uRV_MAX_SAMPLED_GREYBOX_BODIES] = {};
		Zenith_Maths::Vector4 axColours[uRV_MAX_SAMPLED_GREYBOX_BODIES] = {};
		u_int uCollected = 0u;
		u_int uOverflow = 0u;

		g_xEngine.Scenes().QueryActiveScene<Zenith_ModelComponent>().ForEach(
			[&](Zenith_EntityID xID, Zenith_ModelComponent& xModel)
			{
				if (xModel.GetNumMeshes() == 0u)
				{
					return;
				}
				const Zenith_MaterialAsset* pxMaterial = xModel.GetMaterial(0u);
				if (pxMaterial == nullptr
					|| pxMaterial->GetName() != szRV_GREYBOX_MATERIAL)
				{
					return;   // terrain, buildings, a human MODEL -- not a block body
				}
				if (uCollected >= uRV_MAX_SAMPLED_GREYBOX_BODIES)
				{
					++uOverflow;   // Verify reds on this rather than judging a subset
					return;
				}
				axIDs[uCollected] = xID;
				axColours[uCollected] = pxMaterial->GetBaseColor();
				++uCollected;
			});

		u_int uBlockCount = 0u;
		u_int uBlockOffGrey = 0u;
		for (u_int u = 0u; u < uCollected; ++u)
		{
			const Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(axIDs[u]);
			const ZM_Interactable* pxInteractable = xEntity.IsValid()
				? xEntity.TryGetComponent<ZM_Interactable>()
				: nullptr;
			// ZM_NPC_NONE aliases ZM_NPC_COUNT, so one comparison covers "no
			// component", "the sentinel" and "garbage" together.
			if (pxInteractable != nullptr && pxInteractable->GetNpcId() < ZM_NPC_COUNT)
			{
				continue;   // an NPC wearing the cold block; half two judges it
			}
			++uBlockCount;
			// EXACT equality, deliberately: the behaviour-preservation claim is that
			// these bytes did not move at all, not that they moved a little.
			const Zenith_Maths::Vector4& xColour = axColours[u];
			if (xColour.x != xFallback.x || xColour.y != xFallback.y
				|| xColour.z != xFallback.z || xColour.w != xFallback.w)
			{
				++uBlockOffGrey;
			}
		}

		// ---- HALF TWO: the NPC bodies ---------------------------------------
		// Walked from the ROWS rather than from the materials, because a warm NPC
		// carries its own baked material and would simply not appear in a
		// material-name scan -- which would make every clause about NPCs silently
		// vacuous instead of red.
		Zenith_EntityID axNpcIDs[uRV_MAX_SAMPLED_GREYBOX_BODIES] = {};
		u_int uNpcIDs = 0u;
		g_xEngine.Scenes().QueryActiveScene<ZM_Interactable>().ForEach(
			[&](Zenith_EntityID xID, ZM_Interactable& xInteractable)
			{
				if (xInteractable.GetNpcId() >= ZM_NPC_COUNT
					|| uNpcIDs >= uRV_MAX_SAMPLED_GREYBOX_BODIES)
				{
					return;
				}
				axNpcIDs[uNpcIDs++] = xID;
			});

		u_int uNpcCount = 0u;
		u_int uNpcStillGrey = 0u;
		u_int uNpcModelsCorrect = 0u;
		u_int uOtherColours = 0u;
		bool  bVesperFound = false;
		Zenith_Maths::Vector4 xVesperSampled(0.0f);
		Zenith_Maths::Vector4 axOtherColours[uRV_MAX_SAMPLED_GREYBOX_BODIES] = {};

		for (u_int u = 0u; u < uNpcIDs; ++u)
		{
			const Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(axNpcIDs[u]);
			const ZM_Interactable* pxInteractable = xEntity.IsValid()
				? xEntity.TryGetComponent<ZM_Interactable>()
				: nullptr;
			const Zenith_ModelComponent* pxModel = xEntity.IsValid()
				? xEntity.TryGetComponent<Zenith_ModelComponent>()
				: nullptr;
			if (pxInteractable == nullptr || pxModel == nullptr)
			{
				continue;
			}
			const ZM_NPC_ID eNpcId = pxInteractable->GetNpcId();
			if (eNpcId >= ZM_NPC_COUNT)
			{
				continue;
			}
			++uNpcCount;

			if (bWarm)
			{
				// WARM: the body wears its OWN generated model, resolved through the
				// same row -> ZM_HUMAN_ID -> asset-path chain the component walks.
				char acExpectedRef[256];
				const ZM_HUMAN_ID eHumanId = ZM_GetNpcData(eNpcId).m_eHuman;
				if (eHumanId < ZM_HUMAN_COUNT
					&& ZM_HumanAssetPath(eHumanId, ZM_HUMAN_ASSET_MODEL, acExpectedRef,
						(u_int)sizeof(acExpectedRef))
					&& pxModel->GetModelPath() == acExpectedRef)
				{
					++uNpcModelsCorrect;
				}
				if (eNpcId == ZM_NPC_RIVAL_VESPER)
				{
					bVesperFound = true;
				}
				continue;
			}

			// COLD: the proportioned palette block, judged exactly as it always was.
			const Zenith_MaterialAsset* pxMaterial = pxModel->GetNumMeshes() > 0u
				? pxModel->GetMaterial(0u)
				: nullptr;
			if (pxMaterial == nullptr
				|| pxMaterial->GetName() != szRV_GREYBOX_MATERIAL)
			{
				continue;   // neither a model nor a block: Verify's count guard reds
			}
			const Zenith_Maths::Vector4 xColour = pxMaterial->GetBaseColor();
			if (ZM_HumanPaletteSeparation(xColour, xFallback)
				< fZM_HUMAN_PALETTE_MIN_SEPARATION)
			{
				++uNpcStillGrey;
			}
			if (eNpcId == ZM_NPC_RIVAL_VESPER)
			{
				bVesperFound = true;
				xVesperSampled = xColour;
			}
			else
			{
				axOtherColours[uOtherColours] = xColour;
				++uOtherColours;
			}
		}

		g_uRVAppearanceOverflow   = uOverflow;
		g_uRVGreyboxBlockCount    = uBlockCount;
		g_uRVGreyboxBlockOffGrey  = uBlockOffGrey;
		g_uRVNpcVisualCount       = uNpcCount;
		g_uRVNpcVisualStillGrey   = uNpcStillGrey;
		g_uRVNpcModelsCorrect     = uNpcModelsCorrect;
		g_uRVNpcColoursSampled    = uOtherColours;
		g_bRVVesperVisualFound    = bVesperFound;
		g_xRVVesperExpectedColour = xVesperExpected;
		if (bVesperFound && !bWarm)
		{
			g_xRVVesperSampledColour = xVesperSampled;
			g_fRVVesperPaletteError =
				ZM_HumanPaletteSeparation(xVesperSampled, xVesperExpected);
			g_fRVVesperGreySeparation =
				ZM_HumanPaletteSeparation(xVesperSampled, xFallback);
			float fMinSeparation = -1.0f;
			for (u_int u = 0u; u < uOtherColours; ++u)
			{
				const float fSeparation =
					ZM_HumanPaletteSeparation(xVesperSampled, axOtherColours[u]);
				if (fMinSeparation < 0.0f || fSeparation < fMinSeparation)
				{
					fMinSeparation = fSeparation;
				}
			}
			g_fRVVesperMinNpcSeparation = fMinSeparation;
		}
		g_bRVAppearanceSampled = true;
	}

	// -------------------------------------------------------------------------
	// Per-phase drivers. Each returns true to keep stepping, false to stop.
	// -------------------------------------------------------------------------

	// (0) The overworld has finished coming up and the battle machine exists.
	bool RVPhaseAwaitReady()
	{
		RVPlayerView xPlayer;
		RVCameraView xCamera;
		if (!DawnmereRuntimeReady(xPlayer, xCamera))
		{
			if (g_iRVPhaseFrames > iRV_READY_DEADLINE)
			{
				FailRV("Dawnmere did not become runtime-ready in time");
				return false;
			}
			return true;
		}

		if (ResolveSingletonBattleTransition() == nullptr)
		{
			FailRV("no unique ZM_BattleTransition singleton -- the subscriber the "
				"sight FSM raises into cannot exist");
			return false;
		}

		g_eRVPhase = RVPhase::ResolveAuthored;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (1) THE CENTREPIECE. The rival standing in Dawnmere came out of the COMMITTED
	//     scene bytes plus his compiled ZM_NpcData row, and NOTHING in this file
	//     called ConfigureTrainerSight. Everything asserted here is therefore a
	//     property of the shipped content, not of a fixture.
	bool RVPhaseResolveAuthored()
	{
		RVPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailRV("the player disappeared before the authored rival could be resolved");
			return false;
		}

		Zenith_EntityID xID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
		ZM_TRAINER_ID eTrainer = ZM_TRAINER_NONE;
		u_int uRaise = 0xffffffffu;
		u_int uChallenge = 0xffffffffu;
		g_uRVFirstCount = FindAuthoredVesper(
			xID, xPosition, xRotation, eTrainer, uRaise, uChallenge);
		if (g_uRVFirstCount != 1u)
		{
			FailRV("Dawnmere did not contain EXACTLY ONE ZM_Interactable standing on "
				"the ZM_NPC_RIVAL_VESPER row -- either the scene was never re-authored "
				"with the rival, or it carries more than one");
			return false;
		}
		g_bRVFirstResolved   = true;
		g_xRVFirstEntityID   = xID;
		g_xRVSecondEntityID  = xID;   // the live id until the reload replaces it
		g_xRVVesperPosition  = xPosition;
		g_xRVVesperRotation  = xRotation;
		g_eRVFirstTrainer    = eTrainer;

		// ★ THE DERIVATION, off committed bytes with nobody arming him.
		if (eTrainer != eRV_TRAINER)
		{
			FailRV("the authored rival came up with the wrong trainer id -- "
				"ZM_Interactable::OnStart did not derive ZM_TRAINER_RIVAL_VESPER from "
				"his ZM_NpcData row (nothing in this file calls ConfigureTrainerSight, "
				"which is the whole claim)");
			return false;
		}

		const Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		const ZM_Interactable* pxVesper = xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_Interactable>() : nullptr;
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival's ZM_Interactable stopped resolving one frame "
				"after it was found");
			return false;
		}
		g_bRVFirstSightEnabled = pxVesper->IsTrainerSightEnabled();
		g_bRVFirstInteractable = pxVesper->IsInteractable();
		if (!g_bRVFirstSightEnabled || !g_bRVFirstInteractable)
		{
			FailRV("the authored rival is not a live armed trainer at load "
				"(IsTrainerSightEnabled / IsInteractable disagree)");
			return false;
		}
		if (uRaise != 0u || uChallenge != 0u
			|| pxVesper->GetTrainerSpottedCount() != 0u
			|| pxVesper->GetTrainerSpottedIndicatorSubmitCount() != 0u)
		{
			FailRV("the authored rival had already spotted, drawn, raised, or barked "
				"before this test moved -- the walk-up below would be vacuous");
			return false;
		}

		// ★ THE COUPLING PROOF between the COMMITTED FILE and the compiled header
		// the boot units reason about. Change one without the other and this reds:
		// move the constant without re-authoring and the transform disagrees; move
		// both and the boot unit Vesper_PlacementCannotSpawnCampOnTheWhiteoutTarget
		// reds instead. Either edit is caught, which is the point.
		g_fRVPlacementErrorX = std::fabs(xPosition.x - fZM_DAWNMERE_VESPER_X);
		g_fRVPlacementErrorZ = std::fabs(xPosition.z - fZM_DAWNMERE_VESPER_Z);
		if (g_fRVPlacementErrorX > fRV_PLACEMENT_TOLERANCE
			|| g_fRVPlacementErrorZ > fRV_PLACEMENT_TOLERANCE)
		{
			FailRV("the authored rival's XZ in the committed Dawnmere.zscen does not "
				"match Source/World/ZM_DawnmerePlacement.h -- the scene was not "
				"re-authored after the constants moved");
			return false;
		}
		g_fRVFacingAbsDot = RVFacingAbsDot(xRotation, ZM_DawnmereVesperFacing());
		if (g_fRVFacingAbsDot < fRV_FACING_MIN_ABS_DOT)
		{
			FailRV("the authored rival's rotation does not match "
				"ZM_DawnmereVesperFacing() -- the committed scene bytes and the "
				"compiled yaw derivation have drifted apart");
			return false;
		}

		// THE SPAWN-CAMP GUARD. Standing at the whiteout respawn must be outside his
		// cone by a real margin, or the approach below would prove nothing AND the
		// whiteout softlock would be live.
		g_fRVSpawnSeparation = PlanarDistance(xPlayer.m_xPosition, xPosition);
		if (g_fRVSpawnSeparation <= fRV_MIN_SPAWN_SEPARATION)
		{
			FailRV("the authored rival stands inside (or barely outside) the shipped "
				"sight range of the spawn -- a whited-out player would be re-engaged "
				"the instant he respawns");
			return false;
		}
		g_bRVSpawnOutsideCone = !ZM_IsTargetInTrainerSightFromRotation(
			xPosition, xRotation, xPlayer.m_xPosition, ZM_TrainerSightTuning{});
		if (!g_bRVSpawnOutsideCone)
		{
			FailRV("the SC3 cone predicate says the authored rival can already see the "
				"player standing on the spawn -- the walk-up proves nothing");
			return false;
		}

		// ★ HEIGHT, MEASURED RATHER THAN ASSUMED. Every Dawnmere NPC reuses the ONE
		// feet height sampled at the town centre, and the picker/cone band is
		// |npc.y - player.y|; Zenithmon.cpp says in as many words that this is an
		// assumption. For this one NPC it stops being one.
		g_fRVHeightDelta = std::fabs(xPlayer.m_xPosition.y - xPosition.y);
		if (g_fRVHeightDelta >= fZM_SIGHT_MAX_VERTICAL)
		{
			snprintf(g_szRVMeasuredFailure, sizeof(g_szRVMeasuredFailure),
				"the authored rival's centre is %.3f m off the settled player's in Y, "
				"which is outside the shipped sight band of %.3f m -- he is "
				"permanently blind on this heightmap (author a sampled per-NPC feet "
				"height, see Zenithmon.cpp's HEIGHT IS AN ASSUMPTION block)",
				(double)g_fRVHeightDelta, (double)fZM_SIGHT_MAX_VERTICAL);
			FailRV(g_szRVMeasuredFailure);
			return false;
		}

		// ★ KNOWN-LIMIT W4, sampled HERE because this is the first point at which the
		// committed scene is fully started: every ZM_GreyboxVisual::OnStart in
		// Dawnmere has run, so every blockout material already carries its final
		// colour. The clauses live in Verify, so a scan that observes something wrong
		// still lets the rest of the walk-up run and report.
		RVSampleAuthoredAppearance();

		g_eRVPhase = RVPhase::ReloadDawnmere;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (2) THE PERSISTENCE PROOF. SINGLE-load Dawnmere again -- the same call Setup
	//     makes -- and the rival must come back armed off the same committed bytes.
	//     The ENTITY ID must DIFFER, which is what makes this a genuine
	//     teardown/rebuild rather than a no-op load that re-observed phase (1)'s
	//     component.
	bool RVPhaseReloadDawnmere()
	{
		if (!g_bRVReloadIssued)
		{
			g_xEngine.Scenes().LoadSceneByIndex(
				iRV_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);
			g_bRVReloadIssued = true;
			return true;
		}

		RVPlayerView xPlayer;
		RVCameraView xCamera;
		if (!DawnmereRuntimeReady(xPlayer, xCamera)
			|| g_iRVPhaseFrames < iRV_POST_LOAD_SETTLE_FRAMES)
		{
			if (g_iRVPhaseFrames > iRV_RELOAD_DEADLINE)
			{
				FailRV("the reloaded Dawnmere never became runtime-ready");
				return false;
			}
			return true;
		}

		Zenith_EntityID xID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
		ZM_TRAINER_ID eTrainer = ZM_TRAINER_NONE;
		u_int uRaise = 0xffffffffu;
		u_int uChallenge = 0xffffffffu;
		g_uRVSecondCount = FindAuthoredVesper(
			xID, xPosition, xRotation, eTrainer, uRaise, uChallenge);
		if (g_uRVSecondCount != 1u)
		{
			FailRV("the RELOADED Dawnmere did not contain exactly one rival row");
			return false;
		}
		g_bRVSecondResolved = true;
		g_xRVSecondEntityID = xID;
		g_eRVSecondTrainer  = eTrainer;
		g_uRVSecondRaise    = uRaise;
		g_bRVEntityIDChanged = xID != g_xRVFirstEntityID;

		const Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		const ZM_Interactable* pxVesper = xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_Interactable>() : nullptr;
		if (pxVesper == nullptr)
		{
			FailRV("the reloaded rival's ZM_Interactable stopped resolving");
			return false;
		}
		g_uRVSecondState = (u_int)pxVesper->GetTrainerSightState();
		g_uRVSecondSpotted = pxVesper->GetTrainerSpottedCount();
		g_uRVSecondIndicator = pxVesper->GetTrainerSpottedIndicatorSubmitCount();

		if (eTrainer != eRV_TRAINER)
		{
			FailRV("the rival came back from the SECOND load with no trainer -- the "
				"derivation does not survive a scene reload, which is the whole "
				"zero-byte persistence claim");
			return false;
		}
		if (g_uRVSecondState != (u_int)ZM_TRAINER_SIGHT_WATCHING || uRaise != 0u
			|| uChallenge != 0u || g_uRVSecondSpotted != 0u
			|| g_uRVSecondIndicator != 0u)
		{
			FailRV("the reloaded rival did not come up as a COLD watcher (state / "
				"spotted / indicator / raise / challenge disagree)");
			return false;
		}
		if (!g_bRVEntityIDChanged)
		{
			FailRV("the rival's Zenith_EntityID is unchanged across a SINGLE reload -- "
				"the scene was never actually torn down, so 'the identity survived a "
				"reload' would be a claim about the SAME live component");
			return false;
		}

		g_eRVPhase = RVPhase::InstallLead;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (3) Install the deterministic-win lead and capture every payout baseline.
	bool RVPhaseInstallLead()
	{
		RVPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailRV("the player disappeared before the lead could be installed");
			return false;
		}

		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr)
		{
			FailRV("no persistent ZM_GameState resolved -- the prize/flag payout has "
				"no target");
			return false;
		}
		g_uRVMoneyBefore = pxGameState->m_uMoney;
		g_bRVFlagBefore = ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
		// Setup cleared the whole engagement mask, so this must read FALSE.
		g_bRVLatchBefore = ZM_TrainerEngagementLatch::HasEngaged(eRV_TRAINER);
		g_bRVBaselineCaptured = true;

		// The bark's ordering baseline. GetObservedEncounterCount is a PER-COMPONENT
		// counter on the DontDestroyOnLoad singleton, so a batched run needs a
		// captured baseline rather than an assumed zero.
		const ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		g_bRVEncountersBeforeWalkResolved = pxTransition != nullptr;
		g_uRVEncountersBeforeWalk = (pxTransition != nullptr)
			? pxTransition->GetObservedEncounterCount()
			: uRV_ENCOUNTERS_UNRESOLVED_BEFORE;

		pxGameState->m_xParty = ZM_Party{};
		g_bRVLeadInstalled = pxGameState->m_xParty.Add(
			ZM_BuildMonsterRecord(eRV_PLAYER_LEAD_SPECIES, uRV_PLAYER_LEAD_LEVEL));
		if (!g_bRVLeadInstalled)
		{
			FailRV("could not install the deterministic-win party lead");
			return false;
		}

		// Collapse every presentation op to zero duration so a turn drains in a
		// single Tick. It does NOTHING for the overworld bark, which reveals at a
		// hard constexpr 45 chars/sec -- that budget is iRV_CHALLENGE_DISMISS_DEADLINE.
		ZM_SetInstantBattlesForTests(true);

		g_xRVBasisStart = xPlayer.m_xPosition;
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		g_abRVHeldKeys[0] = true;
		g_eRVPhase = RVPhase::BasisProbe;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (4) BASIS PROBE. Held W must move the player, and +Z must dominate, so a
	//     broken movement basis fails in a second WITH the measured deltas instead
	//     of grinding out the approach deadline.
	bool RVPhaseBasisProbe()
	{
		RVPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailRV("the player disappeared during the basis probe");
			return false;
		}
		if (g_iRVPhaseFrames < iRV_BASIS_FRAMES)
		{
			return true;
		}
		g_fRVBasisDeltaX = xPlayer.m_xPosition.x - g_xRVBasisStart.x;
		g_fRVBasisDeltaZ = xPlayer.m_xPosition.z - g_xRVBasisStart.z;
		ClearRVInput();
		if (g_fRVBasisDeltaZ < fRV_BASIS_MIN_FORWARD
			|| std::fabs(g_fRVBasisDeltaZ) <= std::fabs(g_fRVBasisDeltaX))
		{
			FailRV("held W did not move the player forward along +Z -- the movement "
				"basis is wrong (measured deltas are logged below)");
			return false;
		}
		g_bRVBasisPassed = true;

		// Nothing may have raised yet: the probe walked ALONG +Z, not at the rival,
		// and he is 49 m away.
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival vanished during the basis probe");
			return false;
		}
		if (pxVesper->GetTrainerSightRaiseCount() != 0u
			|| pxVesper->GetTrainerChallengeCount() != 0u
			|| pxVesper->GetTrainerSpottedCount() != 0u
			|| pxVesper->GetTrainerSpottedIndicatorSubmitCount() != 0u)
		{
			FailRV("the authored rival spotted, drew, barked, or raised before the "
				"walk-up even began");
			return false;
		}

		g_fRVCurrentDistance =
			PlanarDistance(xPlayer.m_xPosition, g_xRVVesperPosition);
		g_fRVBestDistance = g_fRVCurrentDistance;
		g_iRVStallFrames = 0;
		g_eRVPhase = RVPhase::Approach;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (5) APPROACH -- a CLOSED LOOP on the live position with a progress watchdog.
	//     No SetPosition anywhere, so the player walks on Jolt velocity.
	// ★★ THE FOUR FSM INPUTS, PRINTED ON EVERY FAILING EXIT OF THE APPROACH.
	//
	// A stalled walk-up says only "the player stopped closing"; it never says WHY
	// the rival did not react, and the four things that decide that live inside
	// ZM_Interactable::TickTrainerSight where no accessor reaches them. Every one
	// of them, though, is a PURE function of state this test already holds -- so
	// they are recomputed here rather than exposed, which keeps the component's
	// surface unchanged and cannot drift from the shipped answer because it calls
	// the same functions.
	//
	// The failure that forced this: correct placement, exact facing
	// (facingAbsDot 1.00000), WATCHING, sight enabled, the player driven to 0.077 m,
	// and no spot -- a state in which every clause the test already printed was
	// green. Three separate hypotheses (props on the sight line, approach distance,
	// unflattened ground under a dynamic capsule) were each expensive to falsify
	// and none of them was it.
	void RVLogSightGateBreakdown(const ZM_Interactable& xVesper,
		const RVPlayerView& xPlayer)
	{
		Zenith_Maths::Vector3 xTrainerPos(0.0f);
		Zenith_Maths::Quat xTrainerRot(1.0f, 0.0f, 0.0f, 0.0f);
		const bool bPose = RVResolveVesperPose(xTrainerPos, xTrainerRot);

		const bool bInSight = bPose && ZM_IsTargetInTrainerSightFromRotation(
			xTrainerPos, xTrainerRot, xPlayer.m_xPosition, ZM_TrainerSightTuning{});
		ZM_TrainerSightProbeResult xProbe;
		if (bInSight)
		{
			xProbe = ZM_ProbeTrainerSightLine(xTrainerPos, g_xRVSecondEntityID,
				xPlayer.m_xPosition, xPlayer.m_xEntityID);
		}

		const ZM_TrainerData& xRow = ZM_GetTrainerData(eRV_TRAINER);
		ZM_GameState* pxGameState = nullptr;
		const bool bHasGameState =
			ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr;
		const bool bDefeatFlagSet =
			bHasGameState && ZM_IsStoryFlagSet(*pxGameState, xRow.m_eDefeatFlag);
		const bool bLatchSet = ZM_TrainerEngagementLatch::HasEngaged(eRV_TRAINER);
		const bool bCanBattle = !bHasGameState || ZM_CanEnterBattle(*pxGameState);
		const bool bMayEngage =
			ZM_MayTrainerEngage(xRow, bDefeatFlagSet, bLatchSet, bCanBattle);
		const bool bChannelBusy = ZM_BattleTransition::IsTransitionActive()
			|| ZM_GameStateManager::IsWarpInProgress()
			|| ZM_UI_MenuStack::IsMenuOpen();

		// The cone's own arithmetic, spelled out. targetInSight is one bool over
		// three thresholds, and "false" alone never says WHICH -- range, band or
		// bearing. The dot is the one that has actually bitten.
		Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xTrainerRot);
		const Zenith_Maths::Vector3 xToPlayer(
			xPlayer.m_xPosition.x - xTrainerPos.x, 0.0f,
			xPlayer.m_xPosition.z - xTrainerPos.z);
		const float fToLen = std::sqrt(
			xToPlayer.x * xToPlayer.x + xToPlayer.z * xToPlayer.z);
		const float fConeDot = fToLen > 0.0001f
			? (xToPlayer.x * xForward.x + xToPlayer.z * xForward.z) / fToLen
			: 1.0f;
		Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
		{
			pxCamera->GetFacingDir(xCameraForward);
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_RivalVesper] SIGHT GATE at the failing frame: poseResolved=%s "
			"trainer=(%.3f, %.3f, %.3f) player=(%.3f, %.3f, %.3f) liveGap=%.3f "
			"dY=%.3f | targetInSight=%s (the pure cone) sightLineClear=%s "
			"physicsAvailable=%s blockerHit=%s blockerEntity=%u blockerDist=%.3f "
			"| mayEngage=%s (hasGameState=%s defeatFlag=%s latch=%s canBattle=%s) "
			"channelBusy=%s (transition=%s warp=%s menu=%s) "
			"| approachPossible=%s state=%u "
			// ★ THE CONE ARITHMETIC AND THE DRIVER'S BASIS, TOGETHER. DriveTowardXZ
			// is CAMERA-RELATIVE and quantised to eight directions, so the player's
			// path is a curve, not the straight line from the spawn -- and the rival's
			// facing is derived from the STRAIGHT line. When those two disagree the
			// player walks in outside the cone and nothing else in this log says so.
			"| coneDot=%.5f (want >= %.5f) forward=(%.4f, %.4f) "
			"cameraForward=(%.4f, %.4f) heldKeys W=%d A=%d S=%d D=%d",
			bPose ? "true" : "false",
			xTrainerPos.x, xTrainerPos.y, xTrainerPos.z,
			xPlayer.m_xPosition.x, xPlayer.m_xPosition.y, xPlayer.m_xPosition.z,
			PlanarDistance(xPlayer.m_xPosition, xTrainerPos),
			xPlayer.m_xPosition.y - xTrainerPos.y,
			bInSight ? "true" : "false",
			xProbe.m_bClear ? "true" : "false",
			xProbe.m_bPhysicsAvailable ? "true" : "false",
			xProbe.m_bBlockerHit ? "true" : "false",
			xProbe.m_xBlockerEntityID != INVALID_ENTITY_ID ? "true" : "false",
			xProbe.m_fBlockerDistance,
			bMayEngage ? "true" : "false",
			bHasGameState ? "true" : "false",
			bDefeatFlagSet ? "true" : "false",
			bLatchSet ? "true" : "false",
			bCanBattle ? "true" : "false",
			bChannelBusy ? "true" : "false",
			ZM_BattleTransition::IsTransitionActive() ? "true" : "false",
			ZM_GameStateManager::IsWarpInProgress() ? "true" : "false",
			ZM_UI_MenuStack::IsMenuOpen() ? "true" : "false",
			xVesper.IsTrainerApproachPossible() ? "true" : "false",
			(u_int)xVesper.GetTrainerSightState(),
			fConeDot, fZM_SIGHT_MIN_FACING_DOT, xForward.x, xForward.z,
			xCameraForward.x, xCameraForward.z,
			(int)g_abRVHeldKeys[0], (int)g_abRVHeldKeys[1],
			(int)g_abRVHeldKeys[2], (int)g_abRVHeldKeys[3]);
	}

	bool RVPhaseApproach()
	{
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival was lost during the approach");
			return false;
		}

		// The !completed guard mirrors the walk-up test's. Re-entry is unreachable
		// today (after the beat the machine is CHALLENGING, and ClearRVInput has left
		// the player standing inside the cone so it never re-arms to WATCHING), but
		// without the guard a re-entry would red on RVObserveSpottedFrame's count
		// clause and name the WRONG cause.
		if (!g_bRVSpottedCompleted
			&& pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_SPOTTED)
		{
			ClearRVInput();
			if (!RVObserveSpottedFrame(*pxVesper))
			{
				return false;
			}
			g_eRVPhase = RVPhase::AwaitSpotted;
			g_iRVPhaseFrames = 0;
			return true;
		}

		// S7 item 1 SC3, DEFENSIVE ROUTE. The handoff into the trainer's walk normally
		// happens out of the completed SPOTTED beat one phase down; this catches a
		// machine that reached APPROACHING by any other road (a degenerate spotted
		// duration fails OPEN straight through the beat) so the walk is never measured
		// by a phase whose progress watchdog would read a frozen player as a stall.
		if (pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_APPROACHING)
		{
			ClearRVInput();
			g_eRVPhase = RVPhase::TrainerApproach;
			g_iRVPhaseFrames = 0;
			return true;
		}

		// Fixed 1/30 stepping leaves the shipped 0.35 s SPOTTED state observable
		// for many frames. Reaching any later activity without that observation is
		// therefore a real bypass, not a polling race.
		if (!g_bRVSpottedObserved
			&& (RVTopScreen() == ZM_MENU_SCREEN_DIALOGUE
				|| ZM_BattleTransition::IsTransitionActive()
				|| pxVesper->GetTrainerChallengeCount() != 0u
				|| pxVesper->GetTrainerSightRaiseCount() != 0u))
		{
			FailRV("the authored rival reached dialogue/challenge/encounter activity "
				"without an observable SPOTTED indicator frame");
			return false;
		}

		// THE BARK IS CHECKED FIRST, ahead of the transition, so a frame carrying
		// both is still read as the bark: the claim is that the dialogue PRECEDES
		// the encounter, and reading it the other way round would let the
		// transposed-switch mutation slip through as "the bark was just late".
		if (RVTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
		{
			ClearRVInput();
			RVLatchBarkObservation();
			g_eRVPhase = RVPhase::AwaitChallenge;
			g_iRVPhaseFrames = 0;
			return true;
		}

		if (ZM_BattleTransition::IsTransitionActive())
		{
			// ANTI-VACUITY. The battle started and NO bark was ever seen. The run
			// continues (every payout clause below still means what it meant), but
			// Verify fails naming it.
			if (!g_bRVBarkObserved)
			{
				g_bRVBarkMissed = true;
			}
			ClearRVInput();
			g_eRVPhase = RVPhase::AwaitInBattle;
			g_iRVPhaseFrames = 0;
			return true;
		}

		RVPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailRV("the player was lost during the approach");
			return false;
		}
		g_fRVCurrentDistance =
			PlanarDistance(xPlayer.m_xPosition, g_xRVVesperPosition);

		// ★ ANTI-SPAWN-CAMP, sampled EVERY frame rather than once: while the player
		// is still further away than the shipped sight range by a real margin the
		// raise count must be 0, so a pass cannot be "he could see the spawn all
		// along".
		if (g_fRVCurrentDistance > fRV_MIN_SPAWN_SEPARATION
			&& (pxVesper->GetTrainerSightRaiseCount() != 0u
				|| pxVesper->GetTrainerChallengeCount() != 0u
				|| pxVesper->GetTrainerSpottedCount() != 0u
				|| pxVesper->GetTrainerSpottedIndicatorSubmitCount() != 0u))
		{
			g_bRVEarlyRaiseSeen = true;
			g_fRVEarlyRaiseDistance = g_fRVCurrentDistance;
			FailRV("the authored rival spotted, drew, barked, or raised while the "
				"player was still well outside the shipped sight range -- the walk-up "
				"is not what triggered him");
			return false;
		}

		// ★ THE PLAYER-DRIVEN HALF IS OVER ONCE THE TRAINER HAS WALKED. Beyond that
		// point the rival is standing on the standoff ring and the bark/encounter is
		// one or two frames away, so continuing to drive toward his AUTHORED position
		// would walk the player straight through him, and the progress watchdog would
		// be measuring a player who is deliberately standing still. Wait it out on the
		// phase deadline alone.
		if (g_bRVTrainerApproachEnded)
		{
			if (g_iRVPhaseFrames > iRV_APPROACH_DEADLINE)
			{
				FailRV("the rival finished his walk-up and then never barked and never "
					"raised -- the handoff out of APPROACHING reached neither the "
					"challenge graph nor the encounter dispatch");
				return false;
			}
			return true;
		}

		if (g_fRVCurrentDistance < g_fRVBestDistance - fRV_STALL_IMPROVEMENT)
		{
			g_fRVBestDistance = g_fRVCurrentDistance;
			g_iRVStallFrames = 0;
		}
		else if (++g_iRVStallFrames > iRV_STALL_LIMIT_FRAMES)
		{
			RVLogSightGateBreakdown(*pxVesper, xPlayer);
			FailRV("the walk-up STALLED -- the player stopped closing on the authored "
				"rival (distances and held keys logged below). DriveTowardXZ has NO "
				"obstacle avoidance, so a flank NPC drifting into the approach lane "
				"looks exactly like this");
			return false;
		}

		if (g_iRVPhaseFrames > iRV_APPROACH_DEADLINE)
		{
			RVLogSightGateBreakdown(*pxVesper, xPlayer);
			FailRV("walking into the authored rival's cone never started a battle -- "
				"either he is facing the wrong way (transpose atan2's arguments in "
				"ZM_DawnmereVesperYaw and this is exactly what you get) or the raise "
				"never reached the subscriber");
			return false;
		}

		DriveTowardXZ(xPlayer.m_xPosition, g_xRVVesperPosition);
		return true;
	}

	// (6) THE SPOTTED INDICATOR IS UP. No input is held and no bark/encounter may
	//     begin until the fixed-duration visual beat leaves SPOTTED naturally.
	bool RVPhaseAwaitSpotted()
	{
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival was lost while his SPOTTED indicator was up");
			return false;
		}

		if (pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_SPOTTED)
		{
			if (g_iRVPhaseFrames > iRV_SPOTTED_DEADLINE)
			{
				FailRV("the authored rival remained SPOTTED past the visual-beat deadline");
				return false;
			}
			return RVObserveSpottedFrame(*pxVesper);
		}

		if (!g_bRVSpottedObserved || !g_bRVIndicatorObserved
			|| g_uRVSpottedFramesObserved == 0u
			|| pxVesper->GetTrainerSpottedCount() != 1u
			|| pxVesper->GetTrainerSpottedIndicatorSubmitCount()
				!= g_uRVSpottedIndicatorSubmits)
		{
			FailRV("the authored rival left SPOTTED without one completed, submitted "
				"visual beat");
			return false;
		}
		if (pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_WATCHING)
		{
			FailRV("the authored rival cancelled back to WATCHING before the visual "
				"beat handed off to his challenge");
			return false;
		}

		g_bRVSpottedCompleted = true;
		// ★ S7 item 1 SC3. The completed visual beat now hands off to the TRAINER's
		// walk rather than straight to the bark, and the walk is where the player is
		// frozen -- so control must leave the player-driven Approach phase here or its
		// progress watchdog would diagnose a deliberately frozen player as a stall.
		if (pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_APPROACHING)
		{
			ClearRVInput();
			g_eRVPhase = RVPhase::TrainerApproach;
			g_iRVPhaseFrames = 0;
			return true;
		}
		g_eRVPhase = RVPhase::Approach;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (6b) ★ THE CENTREPIECE OF S7 ITEM 1 SC3: THE RIVAL PHYSICALLY WALKS.
	//
	// Everything here is measured off his LIVE transform, never off the position the
	// scene authored, because the whole claim is that he LEFT it. The player is
	// frozen for the duration and holds no keys, so every metre the gap loses is a
	// metre HE covered -- which is what makes this phase, and not the player's
	// walk-up above, the thing the re-authored DYNAMIC CAPSULE is load-bearing for.
	//
	// Revert the collider to OBB/STATIC and re-author, and this phase is where it
	// reds: a static body cannot be given a velocity, so the gap never closes and the
	// timeout fires with the trainer standing exactly where he started.
	bool RVPhaseTrainerApproach()
	{
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival was lost during his own walk-up");
			return false;
		}

		float fGap = 0.0f;
		if (!RVLiveTrainerGap(fGap))
		{
			FailRV("the player or the rival stopped resolving during the walk-up, so "
				"the closing gap could not be measured");
			return false;
		}

		if (pxVesper->GetTrainerSightState() == ZM_TRAINER_SIGHT_APPROACHING)
		{
			if (!g_bRVTrainerApproachSeen)
			{
				g_bRVTrainerApproachSeen = true;
				g_fRVTrainerStartGap = fGap;
				g_fRVTrainerEndGap = fGap;
			}
			++g_uRVTrainerApproachFrames;

			// ★ THE FREEZE, COUNTED RATHER THAN LATCHED. "IsMovementEnabled() was false
			// once" is satisfied by a freeze that arrived late and left early; the
			// clause in Verify compares these counts against the frames observed, so
			// the freeze has to hold for the WHOLE walk.
			if (!RVPlayerMovementEnabled())
			{
				++g_uRVTrainerFrozenFrames;
			}
			if (pxVesper->IsTrainerCinematicHoldActive()
				&& ZM_TrainerCinematicLatch::IsActive())
			{
				++g_uRVTrainerHeldFrames;
			}

			// MONOTONIC CLOSURE, with a slack that is float noise rather than a metre.
			// Transposing ZM_StepTrainerApproach's two positions at the live call site
			// makes the gap OPEN instead, and this is the clause that says so.
			const float fBackstep = fGap - g_fRVTrainerEndGap;
			if (fBackstep > g_fRVTrainerWorstBackstep)
			{
				g_fRVTrainerWorstBackstep = fBackstep;
			}
			g_fRVTrainerEndGap = fGap;

			if (g_iRVPhaseFrames > iRV_TRAINER_APPROACH_DEADLINE)
			{
				FailRV("the rival never finished his walk-up. The FSM caps APPROACHING "
					"at m_fApproachTimeoutSeconds and then FAILS OPEN, so staying in it "
					"past this deadline means the state is being re-entered every frame "
					"rather than progressing");
				return false;
			}
			return true;
		}

		// ---- THE WALK IS OVER. Everything below is the RELEASE half. ----------
		if (!g_bRVTrainerApproachEnded)
		{
			if (!g_bRVTrainerApproachSeen)
			{
				// FailRV takes ONE pre-formatted string, so a measured diagnostic is
				// composed into the shared backing store first -- the same idiom the
				// feet-height clause in phase (1) uses.
				snprintf(g_szRVMeasuredFailure, sizeof(g_szRVMeasuredFailure),
					"the completed SPOTTED beat handed off with no observable "
					"APPROACHING frame (approachCount=%u). A count of 0 means his "
					"authored body is not a DYNAMIC CAPSULE -- re-author Dawnmere -- or "
					"m_bApproachPossible is answering false for a body that satisfies "
					"the contract; a NON-zero count means the walk arrived on the tick "
					"it started, i.e. the standoff already covered the gap",
					pxVesper->GetTrainerApproachCount());
				FailRV(g_szRVMeasuredFailure);
				return false;
			}
			g_bRVTrainerApproachEnded = true;
			g_uRVTrainerApproachCount = pxVesper->GetTrainerApproachCount();
			// Sampled on the FIRST frame after the walk, while the accumulator still
			// holds the duration the finished walk actually ran.
			g_fRVTrainerElapsedAtEnd = pxVesper->GetTrainerApproachElapsedSeconds();
			// ★ THE RELEASE DEADLINE IS MEASURED FROM THE WALK'S END, not from the
			// phase's start -- the walk itself is allowed to run far longer than the
			// release window, so sharing one counter would fail every successful run.
			g_iRVPhaseFrames = 0;
			return true;
		}

		// The paired POSITIVE. "Still frozen" on its own is satisfied by a build in
		// which nothing ever unfreezes anybody, which is the standing hazard of a
		// fourth writer of a non-refcounted bool.
		if (!g_bRVReleaseObserved)
		{
			const bool bHoldDown = !pxVesper->IsTrainerCinematicHoldActive();
			// The BARK freezes through ZM_UI_MenuStack the same frame the walk ends, so
			// the honest release clause is "this component let go", plus movement being
			// back unless another owner has legitimately taken it.
			const bool bMovementBack = RVPlayerMovementEnabled()
				|| ZM_UI_MenuStack::IsMenuOpen()
				|| ZM_BattleTransition::IsTransitionActive();
			if (bHoldDown && bMovementBack)
			{
				g_bRVReleaseObserved = true;
				g_iRVReleaseFrames = g_iRVPhaseFrames;
			}
			else if (g_iRVPhaseFrames > iRV_CINE_RELEASE_DEADLINE)
			{
				FailRV("the rival left APPROACHING but the cinematic freeze was never "
					"released -- the player is stranded frozen with no owner, which is "
					"the exact failure ZM_TrainerCinematicLatch is shaped to prevent");
				return false;
			}
			return true;
		}

		// Hand back to the shipped walk-up phase, which already knows how to read the
		// bark and the transition apart.
		g_eRVPhase = RVPhase::Approach;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (7) THE BARK IS UP AND THE BATTLE IS NOT. Hold it, un-pressed, for
	//     iRV_CHALLENGE_HOLD_FRAMES: the challenge dialogue must PRECEDE the
	//     encounter, not ride under the fade.
	bool RVPhaseAwaitChallenge()
	{
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival was lost while his challenge bark was up");
			return false;
		}

		if (ZM_BattleTransition::IsTransitionActive()
			|| pxVesper->GetTrainerSightRaiseCount() != 0u)
		{
			FailRV("the encounter was raised WHILE the challenge bark was still up -- "
				"the bark must PRECEDE the battle (order 112 closes the menu before "
				"order 113 dispatches), never ride under the fade");
			return false;
		}
		if (RVTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
		{
			FailRV("the challenge dialogue stopped being the top screen with no "
				"confirm press -- something else claimed the menu stack mid-bark");
			return false;
		}

		if (g_iRVPhaseFrames < iRV_CHALLENGE_HOLD_FRAMES)
		{
			return true;
		}
		g_bRVBarkHoldCompleted = true;
		g_eRVPhase = RVPhase::DismissChallenge;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (7) THE HANDOFF PROOF. Read the bark out with one confirm press per frame,
	//     then require the battle to start within iRV_BARK_TO_BATTLE_DEADLINE
	//     frames of the box closing.
	bool RVPhaseDismissChallenge()
	{
		if (!g_bRVDismissClosed)
		{
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				g_bRVDismissClosed = true;
				g_iRVDismissFrames = g_iRVPhaseFrames;
				// Restart the counter: the deadline below is measured from the CLOSE,
				// not from the first press.
				g_iRVPhaseFrames = 0;
				return true;
			}
			if (g_iRVPhaseFrames > iRV_CHALLENGE_DISMISS_DEADLINE)
			{
				FailRV("the challenge bark never closed under one confirm press per "
					"frame -- the dialogue is stuck (the typewriter is a hard "
					"constexpr 45 chars/sec and ZM_SetInstantBattlesForTests does "
					"NOTHING for a bark, so a longer authored line costs real frames)");
				return false;
			}
			// State-setter only (convention C1), and the same press the shipped
			// DriveMenu phase uses: ENTER is a ZM_CONFIRM_KEYS member.
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			return true;
		}

		if (ZM_BattleTransition::IsTransitionActive())
		{
			g_bRVBarkToBattleOK = true;
			g_iRVBarkToBattleFrames = g_iRVPhaseFrames;
			g_eRVPhase = RVPhase::AwaitInBattle;
			g_iRVPhaseFrames = 0;
			return true;
		}
		if (g_iRVPhaseFrames > iRV_BARK_TO_BATTLE_DEADLINE)
		{
			FailRV("the battle did NOT start within the deadline after the challenge "
				"bark closed -- the withheld encounter was never dispatched, so the "
				"rival talked at the player and then let him walk away");
			return false;
		}
		return true;
	}

	// (8) THE CHANNEL DISCRIMINATOR. Latch which trainer the accepted round trip
	//     carries the first frame the transition leaves IDLE.
	bool RVPhaseAwaitInBattle()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailRV("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE");
			return false;
		}

		if (!g_bRVChannelCaptured
			&& pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
		{
			g_eRVChannelTrainer = pxTransition->GetBattleTrainer();
			g_bRVChannelCaptured = true;
		}

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
		{
			g_bRVReachedInBattle = true;
			g_eRVPhase = RVPhase::DriveMenu;
			g_iRVPhaseFrames = 0;
			return true;
		}

		if (g_iRVPhaseFrames > iRV_INBATTLE_DEADLINE)
		{
			FailRV("the trainer encounter never reached IN_BATTLE before the deadline");
			return false;
		}
		return true;
	}

	// (9) Drive the battle out. The default menu is ACTION_ROOT with cursor 0 =
	//     Fight, so an ENTER every frame picks Fight->move0 each turn.
	bool RVPhaseDriveMenu()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailRV("the ZM_BattleTransition singleton stopped resolving during the battle");
			return false;
		}

		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
			&& pxTransition->GetCompletedBattleCount() == 1u)
		{
			g_eRVPhase = RVPhase::Settle;
			g_iRVPhaseFrames = 0;
			return true;
		}

		if (g_iRVPhaseFrames > iRV_DRIVE_DEADLINE)
		{
			FailRV("the player-driven rival battle never ended (never returned to "
				"IDLE with completed == 1). The lead is deliberately over-levelled, "
				"so an honest LOSS is out of scope here -- RAISE the level rather "
				"than weakening a clause");
			return false;
		}
		return true;
	}

	// (10) Settle, then sample the payout off a FRESHLY re-resolved GameState.
	bool RVPhaseSettle()
	{
		if (g_iRVPhaseFrames < iRV_SETTLE_FRAMES)
		{
			return true;
		}

		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailRV("the ZM_BattleTransition singleton stopped resolving after the resume");
			return false;
		}
		g_uRVCompletedAfter = pxTransition->GetCompletedBattleCount();
		g_uRVAbortedAfter = pxTransition->GetAbortedTransitionCount();
		g_uRVStateAfter = (u_int)pxTransition->GetTransitionState();
		g_bRVBattleUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();

		ZM_GameState* pxGameState = nullptr;
		if (ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr)
		{
			g_uRVMoneyAfter = pxGameState->m_uMoney;
			g_bRVFlagAfter = ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
			g_bRVWhiteoutAfter = pxGameState->m_bPendingWhiteout;
			g_bRVAfterCaptured = true;
		}

		// The rival's own counters, off the AUTHORED component -- the resume has put
		// Dawnmere back as the active scene, so he resolves again.
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper != nullptr)
		{
			g_uRVSettleSpotted = pxVesper->GetTrainerSpottedCount();
			g_uRVSettleIndicator =
				pxVesper->GetTrainerSpottedIndicatorSubmitCount();
			g_uRVSettleRaise = pxVesper->GetTrainerSightRaiseCount();
			g_uRVSettleChallenge = pxVesper->GetTrainerChallengeCount();
			g_bRVSettleResolved = true;
		}

		g_eRVPhase = RVPhase::ReArmWalkBack;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (12) S7 item 1 SC3. RE-ARM THE RIVAL FOR TWO THINGS THE SUCCESSFUL WALK ABOVE
	//      CANNOT SHOW. Walk the player OUT past the sight range (an ENGAGED machine
	//      only re-arms on losing sight), then back IN to a band that is inside the
	//      cone and outside the standoff ring, then clear the gates by hand.
	//
	//      The walk-back is driven with the SAME camera-relative key driver as the
	//      approach -- no SetPosition anywhere, so the player is still moving on Jolt
	//      velocity even in a fixture phase.
	bool RVPhaseReArmWalkBack()
	{
		RVPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailRV("the player disappeared before the cancel-path re-arm");
			return false;
		}
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival disappeared before the cancel-path re-arm");
			return false;
		}
		Zenith_Maths::Vector3 xVesperPosition(0.0f);
		Zenith_Maths::Quat xVesperRotation(1.0f, 0.0f, 0.0f, 0.0f);
		if (!RVResolveVesperPose(xVesperPosition, xVesperRotation))
		{
			FailRV("the authored rival's transform stopped resolving after the battle");
			return false;
		}
		const float fGap = PlanarDistance(xPlayer.m_xPosition, xVesperPosition);

		if (g_iRVPhaseFrames > iRV_REARM_DEADLINE)
		{
			FailRV("the player could not be walked back out of and into the rival's "
				"cone after the battle -- the cancel-path clauses below never ran. The "
				"battle leaves the player parked, so this usually means movement was "
				"never handed back at all");
			return false;
		}
		// The battle's own park is released by ZM_BattleTransition; nothing here may
		// drive input until it has been.
		if (!xPlayer.m_pxController->IsMovementEnabled())
		{
			ClearRVInput();
			return true;
		}

		if (!g_bRVReArmed)
		{
			// (a) OUT past the sight range, so ENGAGED re-arms to WATCHING on lost
			//     sight. Walking a POINT away from him rather than a direction keeps
			//     the driver camera-relative and identical to the approach.
			if (fGap < fRV_REARM_CLEAR_GAP)
			{
				const Zenith_Maths::Vector3 xAway(
					xPlayer.m_xPosition.x + (xPlayer.m_xPosition.x - xVesperPosition.x),
					xPlayer.m_xPosition.y,
					xPlayer.m_xPosition.z + (xPlayer.m_xPosition.z - xVesperPosition.z));
				DriveTowardXZ(xPlayer.m_xPosition, xAway);
				return true;
			}
			if (pxVesper->GetTrainerSightState() != ZM_TRAINER_SIGHT_WATCHING)
			{
				// Out of range and still not re-armed: give the machine its frames,
				// the phase deadline above is the backstop.
				ClearRVInput();
				return true;
			}
			g_bRVReArmed = true;
			return true;
		}

		// (b) BACK IN to the band.
		if (fGap > fRV_REARM_MAX_GAP)
		{
			DriveTowardXZ(xPlayer.m_xPosition, xVesperPosition);
			return true;
		}
		ClearRVInput();
		if (fGap < fRV_REARM_MIN_GAP)
		{
			// Overshot on the frame the keys were released; the capsule decelerates on
			// friction alone, so simply wait rather than driving backwards into an
			// oscillation.
			return true;
		}
		g_fRVReArmGap = fGap;

		// (c) OPEN THE GATES. Both, and BY HAND: the win set the defeat flag, and the
		//     raise burnt the session latch. Clearing them is what makes the second
		//     and third walks possible at all.
		if (!RVSetRivalDefeatFlag(false))
		{
			FailRV("could not clear ZM_STORY_FLAG_RIVAL1_DEFEATED for the cancel-path "
				"re-arm");
			return false;
		}
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
		g_bRVReArmCleared = true;
		g_eRVPhase = RVPhase::CancelMidApproach;
		g_iRVPhaseFrames = 0;
		return true;
	}

	// (13) ★ THE CANCEL PATH, WHICH IS THE ONE THAT CAN STRAND A PLAYER FOREVER.
	//      Arrival releases the freeze on a path a successful run exercises every
	//      time; CANCEL does not, and a release dropped there leaves a frozen player
	//      standing in an overworld with no owner left to unfreeze him. This phase
	//      cancels the walk MID-FLIGHT (by re-setting the defeat flag, which closes
	//      ZM_MayTrainerEngage under him) and then requires the release.
	bool RVPhaseCancelMidApproach()
	{
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival disappeared during the cancel-path walk");
			return false;
		}
		if (g_iRVPhaseFrames > iRV_REARM_DEADLINE)
		{
			snprintf(g_szRVMeasuredFailure, sizeof(g_szRVMeasuredFailure),
				"the re-armed rival never started a second walk-up, so the CANCEL "
				"path was never exercised (state=%u approachCount=%u)",
				(u_int)pxVesper->GetTrainerSightState(),
				pxVesper->GetTrainerApproachCount());
			FailRV(g_szRVMeasuredFailure);
			return false;
		}

		if (!g_bRVCancelIssued)
		{
			if (pxVesper->GetTrainerSightState() != ZM_TRAINER_SIGHT_APPROACHING)
			{
				return true;
			}
			// Sample the hold WHILE it is held, then close the gate under him.
			g_bRVCancelApproachSeen = true;
			g_uRVCancelApproachCount = pxVesper->GetTrainerApproachCount();
			g_bRVCancelFrozenAtEntry = !RVPlayerMovementEnabled();
			g_bRVCancelHeldAtEntry = pxVesper->IsTrainerCinematicHoldActive()
				&& ZM_TrainerCinematicLatch::IsActive();
			if (!RVSetRivalDefeatFlag(true))
			{
				FailRV("could not re-set ZM_STORY_FLAG_RIVAL1_DEFEATED to cancel the "
					"walk mid-flight");
				return false;
			}
			g_bRVCancelIssued = true;
			g_iRVPhaseFrames = 0;
			return true;
		}

		// The machine must abandon the walk (the gate closed), and the freeze must
		// come off WITH it.
		const bool bLeftApproach =
			pxVesper->GetTrainerSightState() != ZM_TRAINER_SIGHT_APPROACHING;
		const bool bHoldDown = !pxVesper->IsTrainerCinematicHoldActive()
			&& !ZM_TrainerCinematicLatch::IsActive();
		// Sampled ONCE and shared by the decision and the diagnostic below, so the
		// failure text can never report a different movement state from the one that
		// actually failed the check.
		const bool bMovementRestored = RVPlayerMovementEnabled();
		if (bLeftApproach && bHoldDown && bMovementRestored)
		{
			g_bRVCancelReleased = true;
			g_iRVCancelReleaseFrames = g_iRVPhaseFrames;
			// Re-open the gate for the leak phase. He is already in WATCHING with the
			// player standing in his cone, so the third walk starts on its own.
			if (!RVSetRivalDefeatFlag(false))
			{
				FailRV("could not re-clear the defeat flag for the leak phase");
				return false;
			}
			ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
			g_eRVPhase = RVPhase::LeakMidApproach;
			g_iRVPhaseFrames = 0;
			return true;
		}
		if (g_iRVPhaseFrames > iRV_CINE_RELEASE_DEADLINE)
		{
			snprintf(g_szRVMeasuredFailure, sizeof(g_szRVMeasuredFailure),
				"the walk was CANCELLED mid-flight and the cinematic freeze was "
				"never released: leftApproach=%d holdDown=%d movement=%d. A release "
				"wired to the ARRIVAL path only looks exactly like this, and it strands "
				"the player frozen with no owner",
				(int)bLeftApproach, (int)bHoldDown, (int)bMovementRestored);
			FailRV(g_szRVMeasuredFailure);
			return false;
		}
		return true;
	}

	// (14) ★ THE DELIBERATE LEAK, AND IT IS THE POINT OF THE PHASE.
	//
	//      This test ENDS while the rival is still mid-APPROACHING, so it leaves
	//      ZM_TrainerCinematicLatch ARMED on purpose. That is the only way the
	//      between-tests hook's ZM_TrainerCinematicLatch::ResetRuntimeStateForTests()
	//      line has any teeth at all: until SC3 nothing ever called Begin() at
	//      runtime, so deleting that line redded nothing. A batched run now inherits a
	//      freeze owner nobody can name unless the hook clears it.
	//
	//      DO NOT "tidy" this by resetting the latch in this test's own teardown --
	//      that would move the proof back inside the test and re-hide the hook.
	bool RVPhaseLeakMidApproach()
	{
		const ZM_Interactable* pxVesper = ResolveVesperComponent();
		if (pxVesper == nullptr)
		{
			FailRV("the authored rival disappeared before the leak phase");
			return false;
		}
		if (pxVesper->GetTrainerSightState() != ZM_TRAINER_SIGHT_APPROACHING)
		{
			if (g_iRVPhaseFrames > iRV_REARM_DEADLINE)
			{
				snprintf(g_szRVMeasuredFailure, sizeof(g_szRVMeasuredFailure),
					"the rival never took a THIRD walk after the cancel, so the "
					"deliberate mid-cinematic exit never happened (state=%u count=%u)",
					(u_int)pxVesper->GetTrainerSightState(),
					pxVesper->GetTrainerApproachCount());
				FailRV(g_szRVMeasuredFailure);
				return false;
			}
			return true;
		}

		g_bRVLeakApproachSeen = true;
		g_uRVLeakApproachCount = pxVesper->GetTrainerApproachCount();
		g_bRVLeakFrozen = !RVPlayerMovementEnabled();
		g_bRVLeakHeld = pxVesper->IsTrainerCinematicHoldActive();
		g_bRVLeakLatchArmed = ZM_TrainerCinematicLatch::IsActive();

		// STOP HERE, mid-walk, with the freeze still on.
		ClearRVInput();
		g_eRVPhase = RVPhase::Done;
		return false;
	}

	// -------------------------------------------------------------------------
	// Harness entry points
	// -------------------------------------------------------------------------

	void Setup_ZMRivalVesper()
	{
		g_eRVPhase          = RVPhase::Done;
		g_iRVPhaseFrames    = 0;
		g_bRVActive         = false;
		g_bRVFailed         = false;
		g_bRVPrereqsPresent = false;
		g_bRVPrimitivesOptionSaved = false;
		g_bRVPrimitivesEnabledBefore = true;
		g_szRVFailure       = "test did not reach verification";
		g_szRVMeasuredFailure[0] = '\0';

		g_bRVFirstResolved     = false;
		g_uRVFirstCount        = 0u;
		g_xRVFirstEntityID     = INVALID_ENTITY_ID;
		g_xRVVesperPosition    = Zenith_Maths::Vector3(0.0f);
		g_xRVVesperRotation    = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		g_eRVFirstTrainer      = ZM_TRAINER_NONE;
		g_bRVFirstSightEnabled = false;
		g_bRVFirstInteractable = false;
		g_fRVPlacementErrorX   = 0.0f;
		g_fRVPlacementErrorZ   = 0.0f;
		g_fRVFacingAbsDot      = 0.0f;
		g_fRVSpawnSeparation   = 0.0f;
		g_bRVSpawnOutsideCone  = false;
		g_fRVHeightDelta       = 0.0f;

		g_bRVAppearanceSampled      = false;
		g_uRVAppearanceOverflow     = 0xffffffffu;
		g_uRVGreyboxBlockCount      = 0u;
		g_uRVGreyboxBlockOffGrey    = 0xffffffffu;
		g_uRVNpcVisualCount         = 0u;
		g_uRVNpcVisualStillGrey     = 0xffffffffu;
		g_uRVNpcColoursSampled      = 0u;
		g_bRVHumansWarm             = false;
		g_uRVNpcModelsCorrect       = 0u;
		g_bRVVesperVisualFound      = false;
		g_xRVVesperSampledColour    = Zenith_Maths::Vector4(-1.0f);
		g_xRVVesperExpectedColour   = Zenith_Maths::Vector4(-1.0f);
		g_fRVVesperPaletteError     = 1.0e9f;
		g_fRVVesperMinNpcSeparation = -1.0f;
		g_fRVVesperGreySeparation   = -1.0f;

		g_bRVReloadIssued    = false;
		g_bRVSecondResolved  = false;
		g_uRVSecondCount     = 0u;
		g_xRVSecondEntityID  = INVALID_ENTITY_ID;
		g_eRVSecondTrainer   = ZM_TRAINER_NONE;
		g_uRVSecondState     = 0xffffffffu;
		g_uRVSecondRaise     = 0xffffffffu;
		g_uRVSecondSpotted   = 0xffffffffu;
		g_uRVSecondIndicator = 0xffffffffu;
		g_bRVEntityIDChanged = false;

		g_bRVBaselineCaptured = false;
		g_uRVMoneyBefore      = 0u;
		g_bRVFlagBefore       = true;
		g_bRVLatchBefore      = true;
		g_bRVLeadInstalled    = false;
		g_uRVEncountersBeforeWalk = uRV_ENCOUNTERS_UNRESOLVED_BEFORE;
		g_bRVEncountersBeforeWalkResolved = false;

		g_xRVBasisStart      = Zenith_Maths::Vector3(0.0f);
		g_fRVBasisDeltaX     = 0.0f;
		g_fRVBasisDeltaZ     = 0.0f;
		g_bRVBasisPassed     = false;
		g_fRVBestDistance    = 0.0f;
		g_fRVCurrentDistance = 0.0f;
		g_iRVStallFrames     = 0;
		g_abRVHeldKeys[0] = false;
		g_abRVHeldKeys[1] = false;
		g_abRVHeldKeys[2] = false;
		g_abRVHeldKeys[3] = false;
		g_bRVEarlyRaiseSeen = false;
		g_fRVEarlyRaiseDistance = 0.0f;

		g_bRVSpottedObserved = false;
		g_bRVSpottedCompleted = false;
		g_bRVMarkerShotRequested = false;
		g_strRVMarkerShotPath.clear();
		if constexpr (!Zenith_IsNullRenderer())
		{
			std::error_code xMarkerDirError;
			const std::filesystem::path xRepoRoot =
				std::filesystem::weakly_canonical(
					std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..",
					xMarkerDirError);
			const std::filesystem::path xMarkerDir =
				xRepoRoot / "Build" / "artifacts" / "zenithmon" / "visual_audit";
			std::filesystem::create_directories(xMarkerDir, xMarkerDirError);
			if (!xMarkerDirError)
			{
				g_strRVMarkerShotPath =
					(xMarkerDir / "spotted_marker.tga").string();
				std::error_code xRemoveError;
				std::filesystem::remove(g_strRVMarkerShotPath, xRemoveError);
			}
		}
		g_bRVIndicatorObserved = false;
		g_bRVSpottedMenuIdleAtSubmit = false;
		g_bRVSpottedTransitionIdleAtSubmit = false;
		g_uRVSpottedCount = 0xffffffffu;
		g_uRVSpottedIndicatorSubmits = 0xffffffffu;
		g_uRVSpottedRaiseAtSubmit = 0xffffffffu;
		g_uRVSpottedChallengeAtSubmit = 0xffffffffu;
		g_uRVSpottedFramesObserved = 0u;
		g_fRVSpottedElapsedAtSubmit = -1.0f;

		g_bRVTrainerApproachSeen   = false;
		g_bRVTrainerApproachEnded  = false;
		g_uRVTrainerApproachFrames = 0u;
		g_uRVTrainerFrozenFrames   = 0u;
		g_uRVTrainerHeldFrames     = 0u;
		g_fRVTrainerStartGap       = -1.0f;
		g_fRVTrainerEndGap         = 1.0e9f;
		g_fRVTrainerWorstBackstep  = 0.0f;
		g_uRVTrainerApproachCount  = 0xffffffffu;
		g_fRVTrainerElapsedAtEnd   = -1.0f;
		g_bRVReleaseObserved       = false;
		g_iRVReleaseFrames         = -1;

		g_bRVDriftSampled      = false;
		g_fRVIdleDriftMax      = 0.0f;
		g_fRVWalkedDriftMax    = 0.0f;
		g_fRVWatchFacingMinDot = 2.0f;

		g_bRVReArmCleared        = false;
		g_bRVReArmed             = false;
		g_fRVReArmGap            = -1.0f;
		g_bRVCancelApproachSeen  = false;
		g_bRVCancelFrozenAtEntry = false;
		g_bRVCancelHeldAtEntry   = false;
		g_bRVCancelIssued        = false;
		g_bRVCancelReleased      = false;
		g_iRVCancelReleaseFrames = -1;
		g_uRVCancelApproachCount = 0xffffffffu;
		g_bRVLeakApproachSeen    = false;
		g_bRVLeakFrozen          = false;
		g_bRVLeakHeld            = false;
		g_bRVLeakLatchArmed      = false;
		g_uRVLeakApproachCount   = 0xffffffffu;

		g_bRVBarkAssetPresent     = false;
		g_bRVBarkObserved         = false;
		g_bRVBarkMissed           = false;
		g_bRVBarkTopWasDialogue   = false;
		g_bRVBarkStateChallenging = false;
		g_bRVBarkTransitionIdle   = false;
		g_uRVBarkRaiseCount       = 0xffffffffu;
		g_uRVBarkChallengeCount   = 0xffffffffu;
		g_uRVEncountersAtBark     = uRV_ENCOUNTERS_UNRESOLVED_AT_BARK;
		g_bRVEncountersAtBarkResolved = false;
		g_bRVBarkHoldCompleted    = false;
		g_bRVDismissClosed        = false;
		g_iRVDismissFrames        = -1;
		g_bRVBarkToBattleOK       = false;
		g_iRVBarkToBattleFrames   = -1;

		g_bRVChannelCaptured = false;
		g_eRVChannelTrainer  = ZM_TRAINER_NONE;
		g_bRVReachedInBattle = false;
		g_bRVAfterCaptured   = false;
		g_uRVMoneyAfter      = 0xffffffffu;
		g_bRVFlagAfter       = false;
		g_bRVWhiteoutAfter   = true;
		g_uRVCompletedAfter  = 0xffffffffu;
		g_uRVAbortedAfter    = 0xffffffffu;
		g_uRVStateAfter      = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;
		g_bRVBattleUnloaded  = false;
		g_bRVSettleResolved  = false;
		g_uRVSettleSpotted   = 0xffffffffu;
		g_uRVSettleIndicator = 0xffffffffu;
		g_uRVSettleRaise     = 0xffffffffu;
		g_uRVSettleChallenge = 0xffffffffu;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO
		// process state (fixed dt, instant-battles flag, scene load) until every
		// input this test CANNOT RUN WITHOUT is confirmed present. The bark .bgraph
		// is deliberately NOT one of them -- see below.
		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bRVPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath)
			&& bWarm;
		if (!g_bRVPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		// ★ THE BARK ASSET IS SAMPLED AFTER THE SKIP GATE AND KEPT OUT OF IT.
		// Written by a tools boot and gitignored, so on a fresh CI checkout it is
		// absent -- and its absence must cost this test only its two bark clauses,
		// never the whole run (a skip is a PASS, and everything else here is SC8's
		// actual subject). Resolved through the SHARED constant so this flag can
		// never name a different file from the one EnsureTrainerChallengeGraph loads.
		const std::string strChallengeGraphPath =
			Zenith_AssetRegistry::ResolvePath(szZM_GRAPH_TRAINER_CHALLENGE_ASSET);
		g_bRVBarkAssetPresent = DiskFilePresent(strChallengeGraphPath);

		// This authored run is also the visual proof that SPOTTED no longer rides
		// the tools-only debug-primitives toggle. Keep it disabled for the whole run;
		// the promoted gameplay cylinder/sphere queues must still reach the G-buffer.
		Zenith_GraphicsOptions& xGraphicsOptions = Zenith_GraphicsOptions::Get();
		g_bRVPrimitivesEnabledBefore = xGraphicsOptions.m_bPrimitivesEnabled;
		g_bRVPrimitivesOptionSaved = true;
		xGraphicsOptions.m_bPrimitivesEnabled = false;

		// Clear the transition's ownerless channel latches AND the ownerless session
		// latch, so an earlier batched test cannot bleed either in. Neither touches
		// any subscription. NO ScopedTestIsolation: it would steal the live
		// subscription tables and delete the subscriber under test.
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fRV_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(
			iRV_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);

		g_eRVPhase = RVPhase::AwaitReady;
		g_bRVActive = true;
	}

	bool Step_ZMRivalVesper(int)
	{
		if (!g_bRVActive || g_bRVFailed || g_eRVPhase == RVPhase::Done)
		{
			return false;
		}

		++g_iRVPhaseFrames;
		// S7 item 1 SC3 / risk R1. Sampled on EVERY frame of EVERY phase, ahead of the
		// switch, because a shoved or yawed DYNAMIC capsule is transient live state
		// that a phase-boundary sample would routinely miss. One transform read.
		RVSampleVesperDrift();
		switch (g_eRVPhase)
		{
		case RVPhase::AwaitReady:        return RVPhaseAwaitReady();
		case RVPhase::ResolveAuthored:   return RVPhaseResolveAuthored();
		case RVPhase::ReloadDawnmere:    return RVPhaseReloadDawnmere();
		case RVPhase::InstallLead:       return RVPhaseInstallLead();
		case RVPhase::BasisProbe:        return RVPhaseBasisProbe();
		case RVPhase::Approach:          return RVPhaseApproach();
		case RVPhase::AwaitSpotted:      return RVPhaseAwaitSpotted();
		case RVPhase::TrainerApproach:   return RVPhaseTrainerApproach();
		case RVPhase::AwaitChallenge:    return RVPhaseAwaitChallenge();
		case RVPhase::DismissChallenge:  return RVPhaseDismissChallenge();
		case RVPhase::AwaitInBattle:     return RVPhaseAwaitInBattle();
		case RVPhase::DriveMenu:         return RVPhaseDriveMenu();
		case RVPhase::Settle:            return RVPhaseSettle();
		case RVPhase::ReArmWalkBack:     return RVPhaseReArmWalkBack();
		case RVPhase::CancelMidApproach: return RVPhaseCancelMidApproach();
		case RVPhase::LeakMidApproach:   return RVPhaseLeakMidApproach();
		case RVPhase::Done:              return false;
		}
		return false;
	}

	bool Verify_ZMRivalVesper()
	{
		bool bPassed = true;

		if (g_bRVActive)
		{
			// The compiled side of the facing coupling, so the log can print BOTH
			// quaternions rather than only their |dot|.
			const Zenith_Maths::Quat xRVExpectedFacing = ZM_DawnmereVesperFacing();

			// Everything captured, so a failure is fully localisable from the log
			// alone without a rebuild.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] authored: failed=%s (%s) firstResolved=%s count=%u "
				"(want 1) trainer=%u (want %u) sightEnabled=%s interactable=%s "
				"placementErrX=%.4f placementErrZ=%.4f (both want <= %.4f) "
				"facingAbsDot=%.5f (want > %.5f) "
				// ★ THE QUATERNIONS THEMSELVES, both sides. |dot| alone says HOW FAR
				// off the facing is but never WHICH WAY, and the failure this pins --
				// transposing ZM_DawnmereVesperYaw's atan2 arguments -- is a clean 90
				// degrees that looks identical to several other mistakes. With both
				// spellings in the log it is diagnosable without a rebuild, which is
				// the rule the rest of this block already follows.
				"authoredRot=(w %.5f, x %.5f, y %.5f, z %.5f) "
				"expectedRot=(w %.5f, x %.5f, y %.5f, z %.5f) "
				"spawnSeparation=%.3f (want > %.3f) "
				"spawnOutsideCone=%s heightDelta=%.4f (want < %.3f)",
				g_bRVFailed ? "true" : "false", g_szRVFailure,
				g_bRVFirstResolved ? "true" : "false", g_uRVFirstCount,
				(u_int)g_eRVFirstTrainer, (u_int)eRV_TRAINER,
				g_bRVFirstSightEnabled ? "true" : "false",
				g_bRVFirstInteractable ? "true" : "false",
				g_fRVPlacementErrorX, g_fRVPlacementErrorZ, fRV_PLACEMENT_TOLERANCE,
				g_fRVFacingAbsDot, fRV_FACING_MIN_ABS_DOT,
				g_xRVVesperRotation.w, g_xRVVesperRotation.x,
				g_xRVVesperRotation.y, g_xRVVesperRotation.z,
				xRVExpectedFacing.w, xRVExpectedFacing.x,
				xRVExpectedFacing.y, xRVExpectedFacing.z,
				g_fRVSpawnSeparation, fRV_MIN_SPAWN_SEPARATION,
				g_bRVSpawnOutsideCone ? "true" : "false",
				g_fRVHeightDelta, fZM_SIGHT_MAX_VERTICAL);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] reload: issued=%s resolved=%s count=%u (want 1) "
				"trainer=%u (want %u) state=%u (want %u=WATCHING) spotted=%u "
				"indicator=%u raise=%u (all want 0) "
				"entityIdChanged=%s (want true) | walk: basisPassed=%s dx=%.3f dz=%.3f "
				"distance=%.3f best=%.3f stallFrames=%d held W=%d A=%d S=%d D=%d "
				"earlyRaiseSeen=%s at %.3f m",
				g_bRVReloadIssued ? "true" : "false",
				g_bRVSecondResolved ? "true" : "false", g_uRVSecondCount,
				(u_int)g_eRVSecondTrainer, (u_int)eRV_TRAINER,
				g_uRVSecondState, (u_int)ZM_TRAINER_SIGHT_WATCHING,
				g_uRVSecondSpotted, g_uRVSecondIndicator, g_uRVSecondRaise,
				g_bRVEntityIDChanged ? "true" : "false",
				g_bRVBasisPassed ? "true" : "false",
				g_fRVBasisDeltaX, g_fRVBasisDeltaZ,
				g_fRVCurrentDistance, g_fRVBestDistance, g_iRVStallFrames,
				(int)g_abRVHeldKeys[0], (int)g_abRVHeldKeys[1],
				(int)g_abRVHeldKeys[2], (int)g_abRVHeldKeys[3],
				g_bRVEarlyRaiseSeen ? "true" : "false", g_fRVEarlyRaiseDistance);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] appearance (W4): sampled=%s overflow=%u (want 0) "
				"blocks=%u (want >0) "
				"blocksOffGrey=%u (want 0) npcBodies=%u (want >1) npcStillGrey=%u "
				"(want 0) otherNpcColours=%u (want >0) vesperFound=%s "
				"vesperSampled=(%.4f, %.4f, %.4f, %.4f) "
				"vesperExpected=(%.4f, %.4f, %.4f, %.4f) paletteError=%.6f "
				"(want <= %.6f) vsGrey=%.4f vsNearestNpc=%.4f (both want >= %.4f)",
				g_bRVAppearanceSampled ? "true" : "false", g_uRVAppearanceOverflow,
				g_uRVGreyboxBlockCount, g_uRVGreyboxBlockOffGrey,
				g_uRVNpcVisualCount, g_uRVNpcVisualStillGrey,
				g_uRVNpcColoursSampled,
				g_bRVVesperVisualFound ? "true" : "false",
				g_xRVVesperSampledColour.x, g_xRVVesperSampledColour.y,
				g_xRVVesperSampledColour.z, g_xRVVesperSampledColour.w,
				g_xRVVesperExpectedColour.x, g_xRVVesperExpectedColour.y,
				g_xRVVesperExpectedColour.z, g_xRVVesperExpectedColour.w,
				g_fRVVesperPaletteError, fRV_PALETTE_TOLERANCE,
				g_fRVVesperGreySeparation, g_fRVVesperMinNpcSeparation,
				fZM_HUMAN_PALETTE_MIN_SEPARATION);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] spotted: observed=%s completed=%s frames=%u count=%u "
				"(want 1) indicatorObserved=%s submits=%u (want >0) elapsedAtSubmit=%.3f "
				"menuIdleAtSubmit=%s transitionIdleAtSubmit=%s raiseAtSubmit=%u "
				"challengeAtSubmit=%u (both want 0)",
				g_bRVSpottedObserved ? "true" : "false",
				g_bRVSpottedCompleted ? "true" : "false",
				g_uRVSpottedFramesObserved, g_uRVSpottedCount,
				g_bRVIndicatorObserved ? "true" : "false",
				g_uRVSpottedIndicatorSubmits, g_fRVSpottedElapsedAtSubmit,
				g_bRVSpottedMenuIdleAtSubmit ? "true" : "false",
				g_bRVSpottedTransitionIdleAtSubmit ? "true" : "false",
				g_uRVSpottedRaiseAtSubmit, g_uRVSpottedChallengeAtSubmit);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] trainerWalk (SC3): seen=%s ended=%s frames=%u "
				"frozenFrames=%u heldFrames=%u (both want == frames) startGap=%.3f "
				"endGap=%.3f (want %.3f +/- %.3f) elapsed=%.3f s (want < %.3f s, i.e. "
				"ARRIVED not timed out) achievedSpeed=%.3f m/s (nominal %.3f) "
				"closure=%.3f (want >= %.3f) "
				"worstBackstep=%.4f (want <= %.4f) approachCount=%u (want 1) "
				"releaseObserved=%s in %d frames (want <= %d) | drift: sampled=%s "
				"idleMax=%.4f (want <= %.3f) walkedMax=%.4f (want <= %.3f) "
				"watchFacingMinDot=%.5f (want >= %.5f)",
				g_bRVTrainerApproachSeen ? "true" : "false",
				g_bRVTrainerApproachEnded ? "true" : "false",
				g_uRVTrainerApproachFrames, g_uRVTrainerFrozenFrames,
				g_uRVTrainerHeldFrames, g_fRVTrainerStartGap, g_fRVTrainerEndGap,
				ZM_TrainerSightFsmTuning{}.m_fApproachStandoffMetres,
				fRV_STANDOFF_TOLERANCE,
				g_fRVTrainerElapsedAtEnd,
				ZM_TrainerSightFsmTuning{}.m_fApproachTimeoutSeconds,
				// The ACHIEVED speed, logged on every run so the compile-time
				// efficiency assumption in ZM_Interactable.cpp is checkable against a
				// real number instead of being taken on faith.
				(g_fRVTrainerElapsedAtEnd > 0.0f)
					? (g_fRVTrainerStartGap - g_fRVTrainerEndGap) / g_fRVTrainerElapsedAtEnd
					: -1.0f,
				ZM_PlayerController::fWALK_SPEED,
				g_fRVTrainerStartGap - g_fRVTrainerEndGap,
				fRV_TRAINER_MIN_CLOSURE, g_fRVTrainerWorstBackstep,
				fRV_TRAINER_BACKSTEP_SLACK, g_uRVTrainerApproachCount,
				g_bRVReleaseObserved ? "true" : "false", g_iRVReleaseFrames,
				iRV_CINE_RELEASE_DEADLINE,
				g_bRVDriftSampled ? "true" : "false",
				g_fRVIdleDriftMax, fRV_IDLE_DRIFT_TOLERANCE,
				g_fRVWalkedDriftMax, fRV_WALKED_DRIFT_TOLERANCE,
				g_fRVWatchFacingMinDot, fRV_FACING_MIN_ABS_DOT);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] cancel+leak (SC3): reArmed=%s cleared=%s gap=%.3f "
				"cancelSeen=%s frozenAtEntry=%s heldAtEntry=%s issued=%s released=%s in "
				"%d frames count=%u (want 2) | leakSeen=%s frozen=%s held=%s "
				"latchArmed=%s count=%u (want 3)",
				g_bRVReArmed ? "true" : "false",
				g_bRVReArmCleared ? "true" : "false", g_fRVReArmGap,
				g_bRVCancelApproachSeen ? "true" : "false",
				g_bRVCancelFrozenAtEntry ? "true" : "false",
				g_bRVCancelHeldAtEntry ? "true" : "false",
				g_bRVCancelIssued ? "true" : "false",
				g_bRVCancelReleased ? "true" : "false", g_iRVCancelReleaseFrames,
				g_uRVCancelApproachCount,
				g_bRVLeakApproachSeen ? "true" : "false",
				g_bRVLeakFrozen ? "true" : "false",
				g_bRVLeakHeld ? "true" : "false",
				g_bRVLeakLatchArmed ? "true" : "false", g_uRVLeakApproachCount);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] bark: assetPresent=%s (false => the bark clauses are "
				"NOT APPLICABLE this run; every other clause still asserted) observed=%s "
				"missed=%s topWasDialogue=%s "
				"stateChallenging=%s transitionIdleAtBark=%s raiseAtBark=%u (want 0) "
				"challengeAtBark=%u (want 1) encountersBeforeWalk=%u (resolved=%s) "
				"encountersAtBark=%u (resolved=%s) (want equal AND both resolved) "
				"holdCompleted=%s dismissClosed=%s dismissFrames=%d barkToBattleOK=%s "
				"barkToBattleFrames=%d (want <= %d)",
				g_bRVBarkAssetPresent ? "true" : "false",
				g_bRVBarkObserved ? "true" : "false",
				g_bRVBarkMissed ? "true" : "false",
				g_bRVBarkTopWasDialogue ? "true" : "false",
				g_bRVBarkStateChallenging ? "true" : "false",
				g_bRVBarkTransitionIdle ? "true" : "false",
				g_uRVBarkRaiseCount, g_uRVBarkChallengeCount,
				g_uRVEncountersBeforeWalk,
				g_bRVEncountersBeforeWalkResolved ? "true" : "false",
				g_uRVEncountersAtBark,
				g_bRVEncountersAtBarkResolved ? "true" : "false",
				g_bRVBarkHoldCompleted ? "true" : "false",
				g_bRVDismissClosed ? "true" : "false", g_iRVDismissFrames,
				g_bRVBarkToBattleOK ? "true" : "false", g_iRVBarkToBattleFrames,
				iRV_BARK_TO_BATTLE_DEADLINE);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesper] payout: channelCaptured=%s channelTrainer=%u (want "
				"%u) reachedInBattle=%s baselineCaptured=%s leadInstalled=%s "
				"moneyBefore=%u flagBefore=%s (want false) latchBefore=%s (want false) "
				"afterCaptured=%s moneyAfter=%u (want before+%u) flagAfter=%s (want "
				"true) whiteoutAfter=%s (want false) completed=%u (want 1) aborted=%u "
				"(want 0) stateAfter=%u (want %u=IDLE) battleUnloaded=%s "
				"settleResolved=%s settleSpotted=%u (want 1) settleIndicator=%u "
				"settleRaise=%u (want 1) settleChallenge=%u (want 1)",
				g_bRVChannelCaptured ? "true" : "false",
				(u_int)g_eRVChannelTrainer, (u_int)eRV_TRAINER,
				g_bRVReachedInBattle ? "true" : "false",
				g_bRVBaselineCaptured ? "true" : "false",
				g_bRVLeadInstalled ? "true" : "false",
				g_uRVMoneyBefore, g_bRVFlagBefore ? "true" : "false",
				g_bRVLatchBefore ? "true" : "false",
				g_bRVAfterCaptured ? "true" : "false",
				g_uRVMoneyAfter, uRV_EXPECTED_PRIZE,
				g_bRVFlagAfter ? "true" : "false",
				g_bRVWhiteoutAfter ? "true" : "false",
				g_uRVCompletedAfter, g_uRVAbortedAfter,
				g_uRVStateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE,
				g_bRVBattleUnloaded ? "true" : "false",
				g_bRVSettleResolved ? "true" : "false",
				g_uRVSettleSpotted, g_uRVSettleIndicator,
				g_uRVSettleRaise, g_uRVSettleChallenge);

			if (g_bRVFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_RivalVesper] %s", g_szRVFailure);
				bPassed = false;
			}

			// ===== THE AUTHORED RESOLUTION HAPPENED AT ALL =====================
			if (!g_bRVFirstResolved || g_uRVFirstCount != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the committed Dawnmere did not yield EXACTLY ONE "
					"ZM_NPC_RIVAL_VESPER interactable (found %u) -- everything below is "
					"vacuous. Re-author the scene from a WINDOWED tools boot",
					g_uRVFirstCount);
				bPassed = false;
			}

			// ===== THE RELOAD: the derivation survives a SECOND load ============
			if (!g_bRVSecondResolved || g_uRVSecondCount != 1u
				|| g_eRVSecondTrainer != eRV_TRAINER || !g_bRVEntityIDChanged
				|| g_uRVSecondState != (u_int)ZM_TRAINER_SIGHT_WATCHING
				|| g_uRVSecondRaise != 0u || g_uRVSecondSpotted != 0u
				|| g_uRVSecondIndicator != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the SINGLE reload did not re-derive the rival off "
					"the committed bytes (resolved=%s count=%u trainer=%u entityIdChanged"
					"=%s). A false entityIdChanged means the scene was never torn down, "
					"so the persistence claim is about the SAME live component",
					g_bRVSecondResolved ? "true" : "false", g_uRVSecondCount,
					(u_int)g_eRVSecondTrainer,
					g_bRVEntityIDChanged ? "true" : "false");
				bPassed = false;
			}

			// ===== THE CHANNEL DISCRIMINATOR ====================================
			// A ZM_TRAINER_NONE here means a WILD encounter stole the round trip,
			// which silently DROPS the trainer raise (Dispatch returns void).
			if (!g_bRVChannelCaptured || g_eRVChannelTrainer != eRV_TRAINER)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the accepted round trip carried trainer %u, expected "
					"%u. If it is %u (ZM_TRAINER_NONE) a WILD GRASS encounter latched first "
					"and the trainer raise was dropped -- MOVE THE WALK LINE, never weaken "
					"this assertion",
					(u_int)g_eRVChannelTrainer, (u_int)eRV_TRAINER, (u_int)ZM_TRAINER_NONE);
				bPassed = false;
			}
			if (!g_bRVReachedInBattle)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the encounter never reached IN_BATTLE");
				bPassed = false;
			}

			// ===== ANTI-VACUITY on the walk and the payout ======================
			if (!g_bRVBasisPassed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the movement basis probe never passed -- the walk-up "
					"below proves nothing about the sight cone");
				bPassed = false;
			}
			if (g_bRVEarlyRaiseSeen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the rival raised at %.3f m, further than the "
					"spawn-camp margin of %.3f m -- the pass would be proximity at spawn, "
					"not a walk-up", g_fRVEarlyRaiseDistance, fRV_MIN_SPAWN_SEPARATION);
				bPassed = false;
			}
			if (!g_bRVBaselineCaptured || !g_bRVLeadInstalled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] no pre-encounter baseline was captured -- the payout "
					"deltas below are vacuous");
				bPassed = false;
			}
			else
			{
				if (g_bRVFlagBefore)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] ZM_STORY_FLAG_RIVAL1_DEFEATED was ALREADY set before "
						"the encounter -- the flag transition is vacuous, and an already-"
						"defeated rival would never have engaged at all");
					bPassed = false;
				}
				if (g_bRVLatchBefore)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the session latch ALREADY held trainer %u before the "
						"encounter -- Setup's ZM_TrainerEngagementLatch reset did not take",
						(u_int)eRV_TRAINER);
					bPassed = false;
				}
				if (g_uRVMoneyBefore + uRV_EXPECTED_PRIZE >= uZM_MONEY_CAP)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the purse (%u) plus the prize is at/over the cap -- "
						"the credit delta would be a saturation artefact", g_uRVMoneyBefore);
					bPassed = false;
				}
			}

			// ===== KNOWN-LIMIT W4: THE AUTHORED APPEARANCE ======================
			// The GUARD comes first and is separate on purpose: it names a MISSING
			// OBSERVATION, while the clauses under it name a wiring violation.
			//
			// ★★ IT USED TO REQUIRE g_uRVGreyboxBlockCount > 0, AND THAT PREMISE IS
			// NOW DELIBERATELY FALSE. Dawnmere's only greybox blockout bodies were the
			// Home and Lab shells with their door leaves and lintels; the building
			// facade overhaul (ZM-D-219) turned all eight into collider-only blockouts
			// wearing generated models, so this scene contains ZERO greybox blocks by
			// design and the scan correctly reports blocks=0. This is the recurring
			// stale-clause shape the map playbook names in section 3.15: a cheap proxy
			// that was conservative under an assumption nobody wrote down, red on
			// something that is exactly right.
			//
			// ★ THE FLOOR IS NOT BEING LOWERED, THE WITNESS MOVED -- and it moved to a
			// BIGGER one. The property is "W4's repaint touched NPCs only, and every
			// non-NPC blockout still wears the shipped grey". This test could only ever
			// witness it over Dawnmere's FOUR Home blocks (its own comment below says
			// so). ZM_InteriorTint_Test walks all FOURTEEN interior shell blocks --
			// PlayerHome's seven and ProfLab's seven -- asserting the material name
			// per block, and it is a hard CI gate that needs no terrain and never
			// skips. So the anti-vacuity requirement is dropped HERE, where the
			// population is legitimately empty, and the clause below is kept, where it
			// still fires the moment a greybox block reappears in Dawnmere wearing
			// anything but grey.
			//
			// The NPC half of the guard is UNTOUCHED: two bodies minimum, the rival
			// found, and a sampled palette when the humans are cold. That is what stops
			// the clauses under it passing on an empty scan.
			if (!g_bRVAppearanceSampled || g_uRVAppearanceOverflow != 0u
				|| g_uRVNpcVisualCount < 2u
					|| (!g_bRVHumansWarm && g_uRVNpcColoursSampled == 0u)
				|| !g_bRVVesperVisualFound)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the appearance scan did not observe the NPC "
					"population off the committed scene (sampled=%s overflow=%u blocks=%u "
					"npcBodies=%u warm=%s otherNpcColours=%u vesperFound=%s) -- every "
					"appearance clause below would be vacuous, or would be judging a "
					"truncated subset. NOTE blocks=0 is EXPECTED since ZM-D-219 and is "
					"no longer part of this guard; the blockout-colour witness is "
					"ZM_InteriorTint_Test over the 14 interior shell blocks",
					g_bRVAppearanceSampled ? "true" : "false", g_uRVAppearanceOverflow,
					g_uRVGreyboxBlockCount, g_uRVNpcVisualCount,
					g_bRVHumansWarm ? "true" : "false", g_uRVNpcColoursSampled,
					g_bRVVesperVisualFound ? "true" : "false");
				bPassed = false;
			}
			else
			{
				// THE BEHAVIOUR-PRESERVATION NET, SCOPED HONESTLY. The scan walks the
				// ACTIVE scene, so this clause covers whatever greybox blockout bodies
				// DAWNMERE holds -- and since ZM-D-219 that is NONE: the Home and Lab
				// shells, their door leaves and their lintels became collider-only
				// blockouts wearing generated building models. The clause is therefore
				// vacuously true today and is kept as a TRIPWIRE: it fires the moment a
				// greybox block reappears in this scene wearing anything but the
				// shipped grey.
				//
				// It used to claim "four committed scenes", which was never true of a
				// scene-scoped walk and became actively misleading at ZM-D-176:
				// PlayerHome's seven shell blocks wear a warm interior tint on purpose,
				// and this test cannot see them (nor should it red on them). The
				// interiors -- all fourteen blocks, which is now the ONLY live witness
				// of this property -- are ZM_InteriorTint_Test's job.
				if (g_uRVGreyboxBlockOffGrey != 0u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] %u of %u NON-NPC blockout bodies are no longer "
						"EXACTLY the shipped greybox grey -- W4 was supposed to repaint NPCs "
						"only, and every wall/floor/door/lintel in the game wears this",
						g_uRVGreyboxBlockOffGrey, g_uRVGreyboxBlockCount);
					bPassed = false;
				}
				// ...and its complement, which is what stops the clause above passing
				// on a build where the wiring was simply deleted.
				// ★ THE COMPLEMENT HAS TWO SHAPES, AND WHICH ONE APPLIES IS DECIDED
				// BY THE SAME POLICY THE RUNTIME CONSULTED -- never by what the scan
				// happened to find. WARM: every NPC wears the model its own row names.
				// COLD: the proportioned fallback block in its own palette colour, the
				// clause this test has always run. Both say "the row reached the body",
				// about two different pictures.
				if (g_bRVHumansWarm && g_uRVNpcModelsCorrect != g_uRVNpcVisualCount)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] only %u of %u authored NPC bodies wear the human "
						"model their own ZM_NpcData row names -- the row never reached the "
						"model, so the rival is still indistinguishable from the townsfolk",
						g_uRVNpcModelsCorrect, g_uRVNpcVisualCount);
					bPassed = false;
				}
				// The palette clauses measure the COLD fallback. On a warm tree they are
				// not skipped for convenience: there is no palette on screen to measure,
				// and the model clause above is the claim in its place.
				if (!g_bRVHumansWarm && g_uRVNpcVisualStillGrey != 0u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] %u of %u authored NPC bodies came up wearing the "
						"blockout grey -- their ZM_NpcData row never reached the material, so "
						"the rival is still indistinguishable from the townsfolk",
						g_uRVNpcVisualStillGrey, g_uRVNpcVisualCount);
					bPassed = false;
				}
				if (!g_bRVHumansWarm && g_fRVVesperPaletteError > fRV_PALETTE_TOLERANCE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the authored rival is wearing (%.4f, %.4f, %.4f) but "
						"his compiled ZM_NpcData row resolves to (%.4f, %.4f, %.4f) -- %.6f "
						"apart. The greybox resolved the WRONG row (or no row at all)",
						g_xRVVesperSampledColour.x, g_xRVVesperSampledColour.y,
						g_xRVVesperSampledColour.z, g_xRVVesperExpectedColour.x,
						g_xRVVesperExpectedColour.y, g_xRVVesperExpectedColour.z,
						g_fRVVesperPaletteError);
					bPassed = false;
				}
				if (!g_bRVHumansWarm
					&& g_fRVVesperGreySeparation < fZM_HUMAN_PALETTE_MIN_SEPARATION)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival's live colour is only %.4f from the "
						"blockout grey (want >= %.4f) -- 'painted' and 'never wired' would "
						"look the same on screen",
						g_fRVVesperGreySeparation, fZM_HUMAN_PALETTE_MIN_SEPARATION);
					bPassed = false;
				}
				// ★ THE ACTUAL W4 CLAIM, measured off LIVE materials rather than off the
				// compiled table: a NUMERIC separation, never a `!=`.
				if (!g_bRVHumansWarm
					&& g_fRVVesperMinNpcSeparation < fZM_HUMAN_PALETTE_MIN_SEPARATION)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival's live colour is only %.4f from the "
						"NEAREST of the %u other authored NPC bodies (want >= %.4f) -- known "
						"limit W4 is that he must not read as one of the townsfolk",
						g_fRVVesperMinNpcSeparation, g_uRVNpcColoursSampled,
						fZM_HUMAN_PALETTE_MIN_SEPARATION);
					bPassed = false;
				}
			}

			// ===== THE VISUAL SPOTTED BEAT CAME FIRST ===========================
			if (!g_bRVSpottedObserved || !g_bRVSpottedCompleted
				|| !g_bRVIndicatorObserved || g_uRVSpottedFramesObserved == 0u
				|| g_uRVSpottedCount != 1u || g_uRVSpottedIndicatorSubmits == 0u
				|| !g_bRVSpottedMenuIdleAtSubmit
				|| !g_bRVSpottedTransitionIdleAtSubmit
				|| g_uRVSpottedRaiseAtSubmit != 0u
				|| g_uRVSpottedChallengeAtSubmit != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the authored walk did not observe one SPOTTED state "
					"with a real indicator submission while dialogue, challenge, raise, and "
					"transition were all still idle");
				bPassed = false;
			}

			// ===== THE BARK CAME FIRST =========================================
			// ★ THE ONLY BLOCK GATED ON THE GITIGNORED .bgraph, and it is gated
			// per-clause rather than by skipping the test (see the file header). With
			// the asset absent the engine fails OPEN -- no dialogue, same battle -- so
			// the approach routes straight through the transition check and every
			// clause outside this block above and below still ran for real. Said out
			// loud in the log so a green run can never be mistaken for a proven bark.
			if (!g_bRVBarkAssetPresent)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the bark clauses are NOT APPLICABLE: %s is absent "
					"(it is written by a tools boot and gitignored). The challenge beat "
					"still STARTED and still raised the encounter -- which is why the "
					"authored rival's own challenge counter is asserted below either way "
					"-- but nothing about the DIALOGUE ordering was observed",
					szZM_GRAPH_TRAINER_CHALLENGE_ASSET);
			}
			else if (!g_bRVBarkObserved || g_bRVBarkMissed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the authored rival reached the battle with no challenge "
					"bark -- the DIALOGUE screen was never observed at any point of the "
					"walk-up. Either the beat never ran (an unresolved node type or a refused "
					"TryPushDialogue -- both FAIL OPEN to the battle by design) or the two "
					"action arms of TickTrainerSight are transposed");
				bPassed = false;
			}
			else
			{
				if (!g_bRVBarkTopWasDialogue)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the raised screen was not the DIALOGUE screen");
					bPassed = false;
				}
				if (!g_bRVBarkStateChallenging)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the sight machine was not CHALLENGING on the frame its "
						"bark was observed -- the dialogue on screen belongs to something else");
					bPassed = false;
				}
				// ★ THE ORDERING PIN IS AN EQUALITY TEST, so an UNRESOLVED sample must
				// never be able to satisfy it. This clause names a MISSING OBSERVATION
				// and is deliberately separate from the equality clause, which names an
				// ordering violation.
				if (!g_bRVEncountersBeforeWalkResolved || !g_bRVEncountersAtBarkResolved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the ZM_BattleTransition singleton did not resolve at "
						"the pre-walk sample (resolved=%s) and/or on the frame the bark was "
						"observed (resolved=%s), so at least one encounter count is an "
						"unresolved sentinel -- the ordering pin measured NOTHING",
						g_bRVEncountersBeforeWalkResolved ? "true" : "false",
						g_bRVEncountersAtBarkResolved ? "true" : "false");
					bPassed = false;
				}
				if (!g_bRVBarkTransitionIdle
					|| g_uRVEncountersAtBark != g_uRVEncountersBeforeWalk)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the battle transition was ALREADY running (or had "
						"already observed an encounter: %u vs the pre-walk %u) on the frame the "
						"bark was observed -- the bark must PRECEDE the encounter, never ride "
						"under the fade", g_uRVEncountersAtBark, g_uRVEncountersBeforeWalk);
					bPassed = false;
				}
				if (g_uRVBarkRaiseCount != 0u || g_uRVBarkChallengeCount != 1u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] at the bark the rival had raised %u encounter(s) and "
						"started %u challenge beat(s); expected EXACTLY 0 and 1",
						g_uRVBarkRaiseCount, g_uRVBarkChallengeCount);
					bPassed = false;
				}
				if (!g_bRVBarkHoldCompleted)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the %d-frame un-pressed hold on the challenge bark "
						"never completed -- 'the battle does not start under the bark' is "
						"unproven", iRV_CHALLENGE_HOLD_FRAMES);
					bPassed = false;
				}
				if (!g_bRVDismissClosed)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the challenge bark never closed under confirm presses");
					bPassed = false;
				}
				else if (!g_bRVBarkToBattleOK)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the bark closed but no battle transition started within "
						"%d frames -- the withheld encounter was never dispatched",
						iRV_BARK_TO_BATTLE_DEADLINE);
					bPassed = false;
				}
			}

			// ===== THE PAYOUT ===================================================
			if (!g_bRVAfterCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] no ZM_GameState resolved after the resume -- the payout "
					"was never sampled");
				bPassed = false;
			}
			else
			{
				if (g_uRVMoneyAfter != g_uRVMoneyBefore + uRV_EXPECTED_PRIZE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the purse went %u -> %u, expected exactly +%u (the "
						"rival's authored prize)",
						g_uRVMoneyBefore, g_uRVMoneyAfter, uRV_EXPECTED_PRIZE);
					bPassed = false;
				}
				if (!g_bRVFlagAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] ZM_STORY_FLAG_RIVAL1_DEFEATED was NOT set after beating "
						"the AUTHORED rival");
					bPassed = false;
				}
				if (g_bRVWhiteoutAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] a WIN latched m_bPendingWhiteout -- the loss path fired "
						"on a won battle");
					bPassed = false;
				}
			}

			// ===== THE ROUND TRIP CLOSED CLEANLY ================================
			if (g_uRVCompletedAfter != 1u || g_uRVAbortedAfter != 0u
				|| g_uRVStateAfter != (u_int)ZM_BATTLE_TRANSITION_IDLE
				|| !g_bRVBattleUnloaded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the battle round trip did not close cleanly: "
					"completed=%u (want 1) aborted=%u (want 0) state=%u (want %u=IDLE) "
					"battleUnloaded=%s", g_uRVCompletedAfter, g_uRVAbortedAfter,
					g_uRVStateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE,
					g_bRVBattleUnloaded ? "true" : "false");
				bPassed = false;
			}

			// ===== S7 item 1 SC3: THE RIVAL PHYSICALLY WALKED ===================
			// The GUARD is separate from the measurements on purpose: it names a
			// MISSING OBSERVATION (he never walked at all -- almost always a scene that
			// was not re-authored with the DYNAMIC CAPSULE), while the clauses under it
			// name a walk that happened and was wrong.
			if (!g_bRVTrainerApproachSeen || !g_bRVTrainerApproachEnded
				|| g_uRVTrainerApproachFrames == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the authored rival never took an observable walk-up "
					"(seen=%s ended=%s frames=%u). Roadmap item 104 is the claim that he "
					"WALKS TO THE PLAYER; with this clause failing, every measurement "
					"below is vacuous. Check that Dawnmere.zscen was re-authored after "
					"ZM_QueueDawnmereTrainerNpc moved to CAPSULE/DYNAMIC",
					g_bRVTrainerApproachSeen ? "true" : "false",
					g_bRVTrainerApproachEnded ? "true" : "false",
					g_uRVTrainerApproachFrames);
				bPassed = false;
			}
			else
			{
				const float fClosure = g_fRVTrainerStartGap - g_fRVTrainerEndGap;
				if (fClosure < fRV_TRAINER_MIN_CLOSURE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival's walk closed only %.3f m (%.3f -> "
						"%.3f) against a required %.3f m, WITH THE PLAYER FROZEN AND "
						"HOLDING NO KEYS -- so he did not move. A STATIC body cannot be "
						"given a velocity: this is what reverting the authored collider "
						"to OBB/STATIC looks like",
						fClosure, g_fRVTrainerStartGap, g_fRVTrainerEndGap,
						fRV_TRAINER_MIN_CLOSURE);
					bPassed = false;
				}
				if (g_fRVTrainerWorstBackstep > fRV_TRAINER_BACKSTEP_SLACK)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival's walk OPENED the gap by %.4f m on at "
						"least one frame (slack %.4f). Transposing the two positions at "
						"the live ZM_StepTrainerApproach call site makes him walk AWAY, "
						"and it is invisible to every boot unit",
						g_fRVTrainerWorstBackstep, fRV_TRAINER_BACKSTEP_SLACK);
					bPassed = false;
				}
				// ★ HE STOPPED **ON** THE RING. Judged against the SHIPPED standoff
				// with a tolerance derived from it, so the two can never disagree the
				// way a hard-coded band did. The window is two-sided on purpose: too
				// far out means the walk ended without arriving, too far IN means he
				// walked through the ring he was told to stop on and is standing on
				// the player.
				const ZM_TrainerSightFsmTuning xRVShippedTuning;
				const float fStandoffError = std::fabs(
					g_fRVTrainerEndGap - xRVShippedTuning.m_fApproachStandoffMetres);
				if (fStandoffError > fRV_STANDOFF_TOLERANCE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival stopped %.3f m from the player, "
						"%.3f m off the shipped standoff ring of %.3f m (tolerance "
						"%.3f = two frames of his own travel plus contact response). "
						"Read this WITH the elapsed clause below: elapsed short of the "
						"timeout means he arrived and this is a geometry problem; "
						"elapsed AT the timeout means he never got here at all",
						g_fRVTrainerEndGap, fStandoffError,
						xRVShippedTuning.m_fApproachStandoffMetres,
						fRV_STANDOFF_TOLERANCE);
					bPassed = false;
				}
				// ★ ARRIVAL, NOT A TIMEOUT -- AND MEASURED. The FSM's timeout FAILS
				// OPEN, so a walk that never reaches the ring still hands off to the
				// bark and every downstream clause in this test still passes. Without
				// this clause the whole phase would certify a trainer who stopped
				// short as long as he stopped short CONSISTENTLY.
				if (g_fRVTrainerElapsedAtEnd <= 0.0f
					|| g_fRVTrainerElapsedAtEnd
						>= xRVShippedTuning.m_fApproachTimeoutSeconds)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the walk ran %.3f s against a fail-open "
						"timeout of %.3f s -- it ENDED ON THE CLOCK rather than on "
						"arrival (or was never sampled). The rival stops wherever he "
						"happened to be and the beat reads as a stutter, with every "
						"other clause in this test still green. Either the walk speed "
						"no longer clears fZM_SIGHT_MAX_DISTANCE minus the standoff "
						"inside the timeout, or something is holding him up",
						g_fRVTrainerElapsedAtEnd,
						xRVShippedTuning.m_fApproachTimeoutSeconds);
					bPassed = false;
				}
				// ★ THE FREEZE, THROUGHOUT. Counted, not latched: a freeze that arrived
				// late or left early satisfies any single-sample clause.
				if (g_uRVTrainerFrozenFrames != g_uRVTrainerApproachFrames
					|| g_uRVTrainerHeldFrames != g_uRVTrainerApproachFrames)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the player was frozen on %u of %u walk frames "
						"and the cinematic hold was armed on %u -- the freeze must cover "
						"the WHOLE walk, or the player can move during a cinematic he is "
						"not supposed to be able to leave",
						g_uRVTrainerFrozenFrames, g_uRVTrainerApproachFrames,
						g_uRVTrainerHeldFrames);
					bPassed = false;
				}
				// ...AND THE PAIRED POSITIVE. Without this the clause above is
				// satisfied by a build in which nothing ever unfreezes anybody.
				if (!g_bRVReleaseObserved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the walk ended and the cinematic hold was never "
						"observed coming off");
					bPassed = false;
				}
				if (g_uRVTrainerApproachCount != 1u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the finished walk booked %u approaches, expected "
						"exactly 1 -- a machine re-entering APPROACHING inside one "
						"spotting would freeze the player again after releasing him",
						g_uRVTrainerApproachCount);
					bPassed = false;
				}
			}

			// ===== S7 item 1 SC3 / risk R1: HE STAYED WHERE HE WAS AUTHORED ======
			// RIGIDBODY_TYPE has no KINEMATIC, so this body can be shoved and (without
			// the all-axis rotation lock) yawed. Both would degrade every geometric
			// claim ZM_DawnmerePlacement.h makes while redding NOTHING -- the boot
			// units reason about compiled constants, and the damage is live state.
			if (!g_bRVDriftSampled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the rival's pose was never sampled, so the R1 drift "
					"bounds below measured nothing");
				bPassed = false;
			}
			else
			{
				if (g_fRVIdleDriftMax > fRV_IDLE_DRIFT_TOLERANCE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival drifted %.4f m off his authored XZ "
						"BEFORE he had ever been asked to walk (tolerance %.3f). A "
						"DYNAMIC capsule that slides or is shoved invalidates every "
						"clearance derived in ZM_DawnmerePlacement.h",
						g_fRVIdleDriftMax, fRV_IDLE_DRIFT_TOLERANCE);
					bPassed = false;
				}
				if (g_fRVWalkedDriftMax > fRV_WALKED_DRIFT_TOLERANCE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the rival ended up %.4f m from his authored XZ "
						"(bound %.3f) -- further than any walk toward a target inside his "
						"own cone can explain, so he was pushed",
						g_fRVWalkedDriftMax, fRV_WALKED_DRIFT_TOLERANCE);
					bPassed = false;
				}
				// The WATCHING facing, live. Phase (1) proves the committed BYTES carry
				// the authored yaw; this proves a DYNAMIC body still carries it after
				// being walked into, battled and walked into again.
				if (g_fRVWatchFacingMinDot < fRV_FACING_MIN_ABS_DOT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the WATCHING rival's live facing fell to |dot| "
						"%.5f against the authored one (required %.5f). A yawed trainer "
						"is a PERMANENTLY BLIND one -- this is ZM-D-156 recurring in "
						"runtime state instead of in the saved bytes",
						g_fRVWatchFacingMinDot, fRV_FACING_MIN_ABS_DOT);
					bPassed = false;
				}
			}

			// ===== S7 item 1 SC3: THE CANCEL PATH AND THE DELIBERATE LEAK ========
			if (!g_bRVReArmed || !g_bRVReArmCleared || !g_bRVCancelApproachSeen
				|| !g_bRVCancelIssued)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the cancel path was never reached (reArmed=%s "
					"cleared=%s approachSeen=%s issued=%s) -- the release clause below "
					"proves nothing, and the ARRIVAL path is then the only exit with any "
					"coverage at all",
					g_bRVReArmed ? "true" : "false",
					g_bRVReArmCleared ? "true" : "false",
					g_bRVCancelApproachSeen ? "true" : "false",
					g_bRVCancelIssued ? "true" : "false");
				bPassed = false;
			}
			else
			{
				if (!g_bRVCancelFrozenAtEntry || !g_bRVCancelHeldAtEntry)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the SECOND walk did not freeze the player "
						"(frozen=%s held=%s), so cancelling it cannot prove a release",
						g_bRVCancelFrozenAtEntry ? "true" : "false",
						g_bRVCancelHeldAtEntry ? "true" : "false");
					bPassed = false;
				}
				if (!g_bRVCancelReleased)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] a walk CANCELLED mid-flight never released the "
						"cinematic freeze -- the player is stranded frozen. This is the "
						"exit an arrival-only release forgets");
					bPassed = false;
				}
				if (g_uRVCancelApproachCount != 2u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] the cancelled walk was approach #%u, expected #2",
						g_uRVCancelApproachCount);
					bPassed = false;
				}
			}
			if (!g_bRVLeakApproachSeen || !g_bRVLeakFrozen || !g_bRVLeakHeld
				|| !g_bRVLeakLatchArmed || g_uRVLeakApproachCount != 3u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the deliberate mid-cinematic exit did not happen "
					"(seen=%s frozen=%s held=%s latchArmed=%s count=%u, want 3). This "
					"test is SUPPOSED to end with ZM_TrainerCinematicLatch armed: it is "
					"the only thing that gives the between-tests hook's reset any teeth",
					g_bRVLeakApproachSeen ? "true" : "false",
					g_bRVLeakFrozen ? "true" : "false",
					g_bRVLeakHeld ? "true" : "false",
					g_bRVLeakLatchArmed ? "true" : "false", g_uRVLeakApproachCount);
				bPassed = false;
			}

			// ===== THE AUTHORED COMPONENT'S OWN COUNTERS ========================
			// Sampled off the entity the reload resolved, so this is the AUTHORED
			// rival's own bookkeeping and not the transition's.
			if (!g_bRVSettleResolved || g_uRVSettleSpotted != 1u
				|| g_uRVSettleIndicator != g_uRVSpottedIndicatorSubmits
				|| g_uRVSettleIndicator == 0u || g_uRVSettleRaise != 1u
				|| g_uRVSettleChallenge != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_RivalVesper] the authored rival's own counters after the battle were "
					"resolved=%s spotted=%u indicator=%u raise=%u challenge=%u; expected "
					"one visual beat, one bark, and one battle",
					g_bRVSettleResolved ? "true" : "false",
					g_uRVSettleSpotted, g_uRVSettleIndicator,
					g_uRVSettleRaise, g_uRVSettleChallenge);
				bPassed = false;
			}

			// ===== THE PIXEL ASSERTION (Shortfalls 1.8-3c) ======================
			// The marker's measured hue window, stated ONCE — both scan passes below
			// must classify a pixel identically or the centroid and the span would be
			// computed over different sets. See the derivation note that follows.
			const auto ZM_RVIsMarkerHue = [](const uint8_t* puBGRA) -> bool
			{
				const float fBlue  = static_cast<float>(puBGRA[0]);
				const float fGreen = static_cast<float>(puBGRA[1]);
				const float fRed   = static_cast<float>(puBGRA[2]);
				if (fRed < 125.0f || fRed > 195.0f) { return false; }
				const float fGreenRatio = fGreen / fRed;
				const float fBlueRatio  = fBlue / fRed;
				return fGreenRatio >= 0.82f && fGreenRatio <= 0.96f
					&& fBlueRatio  >= 0.32f && fBlueRatio  <= 0.54f;
			};

			// Everything above is about Flux's CPU queues. This reads the ACTUAL
			// swapchain bytes for a real SPOTTED frame captured with
			// Graphics/Primitives/Enabled held FALSE all run, so it is the only
			// clause here that can distinguish "queued" from "drawn" -- exactly the
			// distinction ZM-D-168's standing rule turns on.
			//
			// ★ THE HUE IS MEASURED, NOT PREDICTED, and predicting it is how two
			// hand-rolled scans produced false negatives on 2026-07-29. The marker
			// submits linear (1.0, 0.82, 0.08) and is drawn UNLIT at 1.5x, so a
			// saturated yellow looks obvious -- but after exposure + tonemap the
			// bytes depend on the whole display chain, so the window below is
			// RE-DERIVED off the captured TGA whenever the chain legitimately
			// changes. ZM-D-171 (the ISO-derived exposure key, 0.74x the old
			// hand-tuned key) moved the marker from RGB(208,182,97) to red
			// 145-176, g/r 0.858-0.931, b/r 0.364-0.490 (122 px, same 7x28
			// upright box). The window (r 125-195, g/r 0.82-0.96, b/r 0.32-0.54)
			// was verified UNIQUE across the whole 1280x720 frame on that capture:
			// 125 matching pixels, ALL inside the marker's own box, ZERO elsewhere
			// (a looser r>=120 floor admits ~12k terrain pixels -- measured, which
			// is why the floor sits at 125).
			if constexpr (!Zenith_IsNullRenderer())
			{
				Zenith_TestTGAImage xMarkerShot;
				if (!g_bRVMarkerShotRequested || g_strRVMarkerShotPath.empty()
					|| !Zenith_TestLoadTGA(g_strRVMarkerShotPath.c_str(), xMarkerShot))
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] no SPOTTED-frame swapchain capture to read "
						"(requested=%s path='%s')",
						g_bRVMarkerShotRequested ? "true" : "false",
						g_strRVMarkerShotPath.c_str());
					bPassed = false;
				}
				else
				{
					// ★ THE SPAN MUST BE MEASURED ON THE MARKER, NOT ON THE FRAME.
					// This used to take the bounding box of EVERY hue-matching pixel in the
					// whole 1280x720 capture, which made the shape clause hostage to a single
					// stray: observed 2026-08-10 with the marker drawing perfectly (114 px in
					// a 6x29 upright box) and ONE pixel 313 px away stretching the box to
					// 319x77, failing "height >= 2x width". The hue window deliberately sits
					// close to the terrain's colours -- the note above says dropping the red
					// floor to 120 admits ~12k terrain pixels -- so whether a stray exists is
					// a coin flip on an LSB of exposure noise, not a signal about the marker.
					//
					// So: pass 1 finds the hue pixels and their centroid, pass 2 keeps only
					// those within fMARKER_CLUSTER_RADIUS of it, and the span is measured on
					// those. The marker is a compact blob about 7x29, so the radius is huge
					// headroom for it and far too tight for a frame-away stray. The centroid
					// is safe against the handful of outliers this can have (114 vs 1 moves
					// it under 3 px); if strays ever DOMINATED, the kept count collapses and
					// the min-pixel clause below fires with its own message instead of this
					// one lying about the shape.
					constexpr float fMARKER_CLUSTER_RADIUS = 64.0f;

					u_int  uHuePixels = 0u;
					double fHueSumX   = 0.0;
					double fHueSumY   = 0.0;
					for (uint32_t uY = 0u; uY < xMarkerShot.m_uHeight; ++uY)
					{
						for (uint32_t uX = 0u; uX < xMarkerShot.m_uWidth; ++uX)
						{
							if (!ZM_RVIsMarkerHue(xMarkerShot.GetPixelBGRA(uX, uY))) { continue; }
							++uHuePixels;
							fHueSumX += static_cast<double>(uX);
							fHueSumY += static_cast<double>(uY);
						}
					}

					u_int uMarkerPixels = 0u;
					u_int uMinX = xMarkerShot.m_uWidth;
					u_int uMaxX = 0u;
					u_int uMinY = xMarkerShot.m_uHeight;
					u_int uMaxY = 0u;
					if (uHuePixels > 0u)
					{
						const float fCentreX = static_cast<float>(fHueSumX / static_cast<double>(uHuePixels));
						const float fCentreY = static_cast<float>(fHueSumY / static_cast<double>(uHuePixels));
						for (uint32_t uY = 0u; uY < xMarkerShot.m_uHeight; ++uY)
						{
							for (uint32_t uX = 0u; uX < xMarkerShot.m_uWidth; ++uX)
							{
								if (!ZM_RVIsMarkerHue(xMarkerShot.GetPixelBGRA(uX, uY))) { continue; }
								const float fDX = static_cast<float>(uX) - fCentreX;
								const float fDY = static_cast<float>(uY) - fCentreY;
								if ((fDX * fDX + fDY * fDY) > (fMARKER_CLUSTER_RADIUS * fMARKER_CLUSTER_RADIUS)) { continue; }
								++uMarkerPixels;
								uMinX = uX < uMinX ? uX : uMinX;
								uMaxX = uX > uMaxX ? uX : uMaxX;
								uMinY = uY < uMinY ? uY : uMinY;
								uMaxY = uY > uMaxY ? uY : uMaxY;
							}
						}
					}

					Zenith_Log(LOG_CATEGORY_UNITTEST,
						"[ZM_RivalVesper] SPOTTED marker scan: %u hue px in the frame, %u within "
						"%.0f px of their centroid (%u discarded as strays)",
						uHuePixels, uMarkerPixels, static_cast<double>(fMARKER_CLUSTER_RADIUS),
						uHuePixels - uMarkerPixels);

					// 119 observed at 1280x720; 20 leaves room for a smaller window
					// without accepting a stray pixel or two as a marker.
					constexpr u_int uMARKER_MIN_PIXELS = 20u;
					const u_int uSpanX = uMarkerPixels > 0u ? (uMaxX - uMinX + 1u) : 0u;
					const u_int uSpanY = uMarkerPixels > 0u ? (uMaxY - uMinY + 1u) : 0u;
					if (uMarkerPixels < uMARKER_MIN_PIXELS)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_RivalVesper] the SPOTTED marker reached Flux's queues but NOT "
							"the framebuffer: %u marker-hue pixels in %ux%u (want >= %u). The "
							"gameplay primitive queues must draw with "
							"Graphics/Primitives/Enabled unchecked",
							uMarkerPixels, xMarkerShot.m_uWidth, xMarkerShot.m_uHeight,
							uMARKER_MIN_PIXELS);
						bPassed = false;
					}
					// ★ SHAPE, not just presence -- this is audit finding 3. The OLD
					// stem was Flux's flat debug LINE quad, which renders centred on
					// its start point and at an unconstrained roll, so it read as a
					// DIAGONAL stroke through the dot. A diagonal has a roughly square
					// bounding box; an upright bar-above-dot is tall and narrow.
					else if (uSpanY < uSpanX * 2u)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_RivalVesper] the SPOTTED marker drew but is not an upright "
							"exclamation mark: %u px spanning %ux%u (want height >= 2x width). "
							"A near-square span is the old diagonal-stroke shape",
							uMarkerPixels, uSpanX, uSpanY);
						bPassed = false;
					}
					else
					{
						Zenith_Log(LOG_CATEGORY_UNITTEST,
							"[ZM_RivalVesper] SPOTTED marker OBSERVED IN PIXELS: %u marker-hue "
							"px spanning %ux%u in a %ux%u swapchain capture, with "
							"Graphics/Primitives/Enabled FALSE for the whole run. TGA=%s",
							uMarkerPixels, uSpanX, uSpanY,
							xMarkerShot.m_uWidth, xMarkerShot.m_uHeight,
							g_strRVMarkerShotPath.c_str());
					}
				}
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure: drop
		// the fixed timestep, clear the instant-battles flag, clear BOTH ownerless
		// latch sets, close any dialogue left up mid-bark, re-seed the persistent
		// GameState (this test replaced the party), force-unload any lingering
		// Battle scene, restore FrontEnd, then wipe input.
		ClearRVInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
		// CloseMenu unfreezes, so this runs AFTER the transition reset has restored
		// any parked player: exactly one owner releases the player, and it releases
		// it last.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT);
		if (xBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xBattle);
		}
		if (g_bRVActive)
		{
			// SINGLE-loading FrontEnd destroys Dawnmere, so the armed authored rival
			// this test walked into does not outlive it inside a batched run.
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		}
		Zenith_InputSimulator::ResetAllInputState();
		if (g_bRVPrimitivesOptionSaved)
		{
			Zenith_GraphicsOptions::Get().m_bPrimitivesEnabled =
				g_bRVPrimitivesEnabledBefore;
			g_bRVPrimitivesOptionSaved = false;
		}
		g_bRVActive = false;

		return bPassed || !g_bRVPrereqsPresent;
	}
}

// m_bRequiresGraphics = FALSE: this test asserts nothing about pixels, so it runs
// FOR REAL on the Null (GPU-less) backend instead of skipping as a pass.
static const Zenith_AutomatedTest g_xZMRivalVesperAuthoredTest = {
	"ZM_RivalVesperAuthored_Test",
	&Setup_ZMRivalVesper,
	&Step_ZMRivalVesper,
	&Verify_ZMRivalVesper,
	// Above the SUM of the named phase deadlines (420 ready + 1 resolve + 424
	// reload + 1 lead + 30 basis + 900 approach + 60 spotted + 180 TRAINER approach
	// + 30 bark hold + 180 bark dismiss + 2 bark->battle + 600 in-battle + 900 drive
	// + 8 settle + 420 re-arm + 420 cancel + 420 leak = 4996 worst case, of which the
	// three S7 item 1 SC3 tails are short walks in practice).
	// Two
	// runtime-ready windows -- the initial load and the mid-test reload -- dominate.
	// The harness jumps straight to Verify when maxFrames is hit, so this must
	// exceed the REALISTIC sum or a slow-but-valid run would be cut off mid-battle
	// and read as a failure rather than a timeout. Raised 4000 -> 4400 by S7 item 1
	// SC3 for the trainer's own walk and the two cancel/leak tails; every one of
	// those phases owns a NAMED deadline that fails with a diagnostic first, so this
	// cap stays a backstop rather than the thing that reports.
	/* maxFrames */ 4400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMRivalVesperAuthoredTest);

// ============================================================================
// ZM_RivalVesperWhiteout_Test -- the independent LOSS half of the authored
// Vesper contract. This uses the exact L5 Fernfawn starter, physically walks into
// the committed rival's cone, and drives Fight -> its second learned move through
// the live HUD.
// No combat stat, HP, or trainer level is weakened before the core decides the
// outcome. The input bot selects the starter's second legitimately learned move
// through the live HUD because the old move-0 assumption was measured producing
// a PLAYER win. Only after the observed ENEMY win + pending-whiteout latch do we make
// HP/PP/status dirty, so the manager's HealAllFull is independently observable.
// ============================================================================
namespace
{
	constexpr ZM_TRAINER_ID eRVW_TRAINER = ZM_TRAINER_RIVAL_VESPER;
	constexpr const char* szRVW_EXPECTED_WHITEOUT_TAG = "TownCenter";
	constexpr int iRVW_DAWNMERE_BUILD = 2;
	constexpr float fRVW_FIXED_DT = 1.0f / 30.0f;
	constexpr float fRVW_POSITION_EPSILON = 0.05f;
	constexpr int iRVW_READY_DEADLINE = 420;
	constexpr int iRVW_APPROACH_DEADLINE = 900;
	constexpr int iRVW_BARK_DEADLINE = 180;
	constexpr int iRVW_IN_BATTLE_DEADLINE = 600;
	constexpr int iRVW_BATTLE_DEADLINE = 900;
	constexpr int iRVW_WHITEOUT_DEADLINE = 900;
	constexpr int iRVW_NO_INPUT_HOLD_FRAMES = 200;

	enum class RVWPhase
	{
		AwaitReady,
		Approach,
		DismissChallenge,
		AwaitInBattle,
		DriveBattle,
		AwaitBattleResume,
		AwaitWhiteout,
		HoldRearmed,
		Done,
	};

	struct RVWVesperView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_TRAINER_ID m_eTrainer = ZM_TRAINER_NONE;
		bool m_bSightEnabled = false;
		ZM_TRAINER_SIGHT_STATE m_eState = ZM_TRAINER_SIGHT_STATE_COUNT;
		u_int m_uRaiseCount = 0xffffffffu;
		u_int m_uChallengeCount = 0xffffffffu;
	};

	RVWPhase g_eRVWPhase = RVWPhase::Done;
	int g_iRVWPhaseFrames = 0;
	bool g_bRVWActive = false;
	bool g_bRVWPrereqsPresent = false;
	bool g_bRVWFailed = false;
	const char* g_szRVWFailure = "test did not reach verification";

	Zenith_EntityID g_xRVWOriginalVesperID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xRVWReloadedVesperID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xRVWVesperPosition = Zenith_Maths::Vector3(0.0f);
	float g_fRVWInitialSeparation = 0.0f;
	float g_fRVWBestSeparation = 0.0f;
	int g_iRVWStallFrames = 0;
	bool g_bRVWWalkedIntoCone = false;
	bool g_bRVWReachedInBattle = false;
	bool g_bRVWChannelCaptured = false;
	ZM_TRAINER_ID g_eRVWChannelTrainer = ZM_TRAINER_NONE;

	bool g_bRVWStarterCaptured = false;
	u_int g_uRVWMoneyBefore = 0xffffffffu;
	u_int g_uRVWExpBefore = 0xffffffffu;
	u_int g_uRVWLevelBefore = 0xffffffffu;
	u_int g_uRVWMaxHpBefore = 0u;
	u_int g_uRVWMaxPpBefore = 0u;
	bool g_bRVWFlagBefore = true;
	u_int g_uRVWManagerLoadsBefore = 0xffffffffu;

	bool g_bRVWBattleConfigCaptured = false;
	bool g_bRVWLiveHealthyMatchup = false;
	bool g_bRVWCatchAllowed = true;
	bool g_bRVWFleeAllowed = true;
	ZM_MOVE_ID g_eRVWChosenMove = ZM_MOVE_NONE;
	bool g_bRVWChosenMoveUsed = false;
	ZM_SPECIES_ID g_eRVWPlayerSpecies = ZM_SPECIES_NONE;
	u_int g_uRVWPlayerLevel = 0u;
	ZM_SPECIES_ID g_eRVWEnemySpecies = ZM_SPECIES_NONE;
	u_int g_uRVWEnemyLevel = 0u;
	bool g_bRVWOutcomeCaptured = false;
	ZM_SIDE g_eRVWWinner = ZM_SIDE_COUNT;
	u_int g_uRVWFinalBattleHp = 0xffffffffu;
	bool g_bRVWPendingObserved = false;
	bool g_bRVWDirtyHealProbeInstalled = false;

	bool g_bRVWBattleResumeObserved = false;
	bool g_bRVWWhiteoutWarpObserved = false;
	bool g_bRVWWhiteoutSettled = false;
	bool g_bRVWPendingAfter = true;
	u_int g_uRVWHpAfter = 0u;
	u_int g_uRVWMaxHpAfter = 0u;
	u_int g_uRVWPpAfter = 0u;
	u_int g_uRVWMaxPpAfter = 0u;
	ZM_MAJOR_STATUS g_eRVWStatusAfter = ZM_MAJOR_STATUS_COUNT;
	u_int g_uRVWMoneyAfter = 0xffffffffu;
	u_int g_uRVWExpAfter = 0xffffffffu;
	bool g_bRVWFlagAfter = true;
	u_int g_uRVWManagerLoadsAfter = 0xffffffffu;
	u_int g_uRVWTransitionLoadsAfter = 0xffffffffu;
	u_int g_uRVWEncounterCountAfter = 0xffffffffu;
	u_int g_uRVWCompletedAfter = 0xffffffffu;
	u_int g_uRVWAbortedAfter = 0xffffffffu;
	bool g_bRVWTownCenterTag = false;
	bool g_bRVWTownCenterResolved = false;
	float g_fRVWTransformSpawnError = 9999.0f;
	float g_fRVWBodySpawnError = 9999.0f;
	bool g_bRVWVesperRearmed = false;
	u_int g_uRVWReloadedRaise = 0xffffffffu;
	u_int g_uRVWReloadedChallenge = 0xffffffffu;
	bool g_bRVWNoInputHoldCompleted = false;

	float RVWDistance(const Zenith_Maths::Vector3& xA,
		const Zenith_Maths::Vector3& xB)
	{
		const float fX = xA.x - xB.x;
		const float fY = xA.y - xB.y;
		const float fZ = xA.z - xB.z;
		return std::sqrt(fX * fX + fY * fY + fZ * fZ);
	}

	float RVWPlanarDistance(const Zenith_Maths::Vector3& xA,
		const Zenith_Maths::Vector3& xB)
	{
		const float fX = xA.x - xB.x;
		const float fZ = xA.z - xB.z;
		return std::sqrt(fX * fX + fZ * fZ);
	}

	void RVWClearInput()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
	}

	void FailRVW(const char* szReason)
	{
		g_szRVWFailure = szReason;
		g_bRVWFailed = true;
		g_eRVWPhase = RVWPhase::Done;
		RVWClearInput();
	}

	ZM_GameStateManager* RVWResolveManager()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_GameStateManager>() : nullptr;
	}

	ZM_BattleDirector* RVWResolveDirector()
	{
		Zenith_EntityID xID = INVALID_ENTITY_ID;
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryAllScenes<ZM_BattleDirector>().ForEach(
			[&](Zenith_EntityID xCandidate, ZM_BattleDirector&)
			{
				++uCount;
				if (uCount == 1u) { xID = xCandidate; }
			});
		if (uCount != 1u) { return nullptr; }
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_BattleDirector>() : nullptr;
	}

	u_int RVWFindVesper(RVWVesperView& xOut)
	{
		xOut = RVWVesperView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_Interactable, Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xID, ZM_Interactable& xInteractable,
				Zenith_TransformComponent& xTransform)
			{
				if (xInteractable.GetNpcId() != ZM_NPC_RIVAL_VESPER) { return; }
				++uCount;
				if (uCount != 1u) { return; }
				xOut.m_xEntityID = xID;
				xTransform.GetPosition(xOut.m_xPosition);
				xOut.m_eTrainer = xInteractable.GetTrainerId();
				xOut.m_bSightEnabled = xInteractable.IsTrainerSightEnabled();
				xOut.m_eState = xInteractable.GetTrainerSightState();
				xOut.m_uRaiseCount = xInteractable.GetTrainerSightRaiseCount();
				xOut.m_uChallengeCount = xInteractable.GetTrainerChallengeCount();
			});
		return uCount;
	}

	void RVWDriveToward(const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		RVWClearInput();
		Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
		{
			pxCamera->GetFacingDir(xCameraForward);
		}
		Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
		const float fLengthSq = xForward.x * xForward.x + xForward.z * xForward.z;
		if (fLengthSq <= 0.000001f) { xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f); }
		else { xForward /= std::sqrt(fLengthSq); }
		const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);
		const Zenith_Maths::Vector3 xToTarget(
			xTarget.x - xPosition.x, 0.0f, xTarget.z - xPosition.z);
		const float fForward = xToTarget.x * xForward.x + xToTarget.z * xForward.z;
		const float fRight = xToTarget.x * xRight.x + xToTarget.z * xRight.z;
		if (fRight < -0.08f) { Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_A); }
		else if (fRight > 0.08f) { Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_D); }
		if (fForward < -0.08f) { Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_S); }
		else if (fForward > 0.08f) { Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W); }
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_LEFT_SHIFT);
	}

	void RVWResetCaptures()
	{
		g_eRVWPhase = RVWPhase::Done;
		g_iRVWPhaseFrames = 0;
		g_bRVWActive = false;
		g_bRVWPrereqsPresent = false;
		g_bRVWFailed = false;
		g_szRVWFailure = "test did not reach verification";
		g_xRVWOriginalVesperID = INVALID_ENTITY_ID;
		g_xRVWReloadedVesperID = INVALID_ENTITY_ID;
		g_xRVWVesperPosition = Zenith_Maths::Vector3(0.0f);
		g_fRVWInitialSeparation = 0.0f;
		g_fRVWBestSeparation = 0.0f;
		g_iRVWStallFrames = 0;
		g_bRVWWalkedIntoCone = false;
		g_bRVWReachedInBattle = false;
		g_bRVWChannelCaptured = false;
		g_eRVWChannelTrainer = ZM_TRAINER_NONE;
		g_bRVWStarterCaptured = false;
		g_uRVWMoneyBefore = g_uRVWExpBefore = g_uRVWLevelBefore = 0xffffffffu;
		g_uRVWMaxHpBefore = g_uRVWMaxPpBefore = 0u;
		g_bRVWFlagBefore = true;
		g_uRVWManagerLoadsBefore = 0xffffffffu;
		g_bRVWBattleConfigCaptured = false;
		g_bRVWLiveHealthyMatchup = false;
		g_bRVWCatchAllowed = true;
		g_bRVWFleeAllowed = true;
		g_eRVWChosenMove = ZM_MOVE_NONE;
		g_bRVWChosenMoveUsed = false;
		g_eRVWPlayerSpecies = g_eRVWEnemySpecies = ZM_SPECIES_NONE;
		g_uRVWPlayerLevel = g_uRVWEnemyLevel = 0u;
		g_bRVWOutcomeCaptured = false;
		g_eRVWWinner = ZM_SIDE_COUNT;
		g_uRVWFinalBattleHp = 0xffffffffu;
		g_bRVWPendingObserved = false;
		g_bRVWDirtyHealProbeInstalled = false;
		g_bRVWBattleResumeObserved = false;
		g_bRVWWhiteoutWarpObserved = false;
		g_bRVWWhiteoutSettled = false;
		g_bRVWPendingAfter = true;
		g_uRVWHpAfter = g_uRVWMaxHpAfter = 0u;
		g_uRVWPpAfter = g_uRVWMaxPpAfter = 0u;
		g_eRVWStatusAfter = ZM_MAJOR_STATUS_COUNT;
		g_uRVWMoneyAfter = g_uRVWExpAfter = 0xffffffffu;
		g_bRVWFlagAfter = true;
		g_uRVWManagerLoadsAfter = 0xffffffffu;
		g_uRVWTransitionLoadsAfter = 0xffffffffu;
		g_uRVWEncounterCountAfter = g_uRVWCompletedAfter = g_uRVWAbortedAfter = 0xffffffffu;
		g_bRVWTownCenterTag = false;
		g_bRVWTownCenterResolved = false;
		g_fRVWTransformSpawnError = g_fRVWBodySpawnError = 9999.0f;
		g_bRVWVesperRearmed = false;
		g_uRVWReloadedRaise = g_uRVWReloadedChallenge = 0xffffffffu;
		g_bRVWNoInputHoldCompleted = false;
	}

	bool RVWSampleTownCenter(const RVPlayerView& xPlayer)
	{
		const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
		if (!xActive.IsValid()
			|| ZM_SpawnPoint::FindUniqueInScene(xActive,
				szRVW_EXPECTED_WHITEOUT_TAG, xSpawnID)
				!= ZM_SPAWN_POINT_LOOKUP_FOUND)
		{
			return false;
		}
		Zenith_Entity xSpawn = g_xEngine.Scenes().ResolveEntity(xSpawnID);
		Zenith_Entity xPlayerEntity = g_xEngine.Scenes().ResolveEntity(xPlayer.m_xEntityID);
		Zenith_TransformComponent* pxSpawnTransform = xSpawn.IsValid()
			? xSpawn.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		Zenith_TransformComponent* pxPlayerTransform = xPlayerEntity.IsValid()
			? xPlayerEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxSpawnTransform == nullptr || pxPlayerTransform == nullptr
			|| xPlayer.m_pxCollider == nullptr || !xPlayer.m_pxCollider->HasValidBody())
		{
			return false;
		}
		Zenith_Maths::Vector3 xFeet(0.0f);
		Zenith_Maths::Vector3 xScale(1.0f);
		pxSpawnTransform->GetPosition(xFeet);
		pxPlayerTransform->GetScale(xScale);
		// Independent oracle: do not call GameStateManager::CalculateSpawnCenter,
		// because that is the production function whose result the warp consumes.
		// Following a bad production offset with the same helper would compare a
		// wrong placement to itself and stay green.
		const Zenith_Maths::Vector3 xExpected = xFeet
			+ Zenith_Maths::Vector3(0.0f, fZM_HUMAN_BODY_HALF_HEIGHT, 0.0f);
		g_fRVWTransformSpawnError = RVWDistance(xPlayer.m_xPosition, xExpected);
		g_fRVWBodySpawnError = RVWDistance(
			g_xEngine.Physics().GetBodyPosition(xPlayer.m_pxCollider->GetBodyID()),
			xExpected);
		g_bRVWTownCenterResolved = true;
		return true;
	}

	bool RVWCaptureBattleAndDrive()
	{
		ZM_BattleDirector* pxDirector = RVWResolveDirector();
		if (pxDirector == nullptr) { return true; }
		const ZM_BATTLE_DIRECTOR_PHASE ePhase = pxDirector->GetPhase();
		if (ePhase != ZM_BD_RUNNING && ePhase != ZM_BD_RESOLVED
			&& ePhase != ZM_BD_DONE)
		{
			return true;
		}

		const ZM_BattleDirectorCore& xCore = pxDirector->GetCore();
		const ZM_BattleState& xState = xCore.GetEngine().GetState();
		if (!g_bRVWBattleConfigCaptured)
		{
			const ZM_BattleSide& xPlayerSide = xState.Side(ZM_SIDE_PLAYER);
			const ZM_BattleSide& xEnemySide = xState.Side(ZM_SIDE_ENEMY);
			if (xPlayerSide.m_xParty.GetSize() != 1u
				|| xEnemySide.m_xParty.GetSize() != 1u)
			{
				FailRVW("the live core did not begin with exactly two one-member parties");
				return false;
			}
			const ZM_BattleMonster& xPlayer = xPlayerSide.Active();
			const ZM_BattleMonster& xEnemy = xEnemySide.Active();
			g_eRVWPlayerSpecies = xPlayer.m_eSpecies;
			g_uRVWPlayerLevel = xPlayer.m_uLevel;
			g_eRVWEnemySpecies = xEnemy.m_eSpecies;
			g_uRVWEnemyLevel = xEnemy.m_uLevel;
			g_bRVWCatchAllowed = xCore.IsCatchAllowed();
			g_bRVWFleeAllowed = xCore.IsFleeAllowed();
			g_eRVWChosenMove = xPlayer.m_axMoves[1].m_eMove;
			if (xPlayer.m_uCurHP != xPlayer.m_auMaxStat[ZM_STAT_HP]
				|| xPlayer.m_uCurHP != g_uRVWMaxHpBefore
				|| xPlayer.m_eStatus != ZM_MAJOR_STATUS_NONE
				|| xPlayer.m_axMoves[0].m_eMove == ZM_MOVE_NONE
				|| xPlayer.m_axMoves[0].m_uCurPP != xPlayer.m_axMoves[0].m_uMaxPP
				|| g_eRVWChosenMove == ZM_MOVE_NONE
				|| xPlayer.m_axMoves[1].m_uCurPP != xPlayer.m_axMoves[1].m_uMaxPP
				|| xEnemy.m_uCurHP != xEnemy.m_auMaxStat[ZM_STAT_HP]
				|| xEnemy.m_eStatus != ZM_MAJOR_STATUS_NONE)
			{
				FailRVW("the live core did not begin with two healthy one-member L5 parties and full moves");
				return false;
			}
			g_bRVWLiveHealthyMatchup = true;
			g_bRVWBattleConfigCaptured = true;
		}

		for (u_int i = 0u; i < xCore.GetEngine().GetEventCount(); ++i)
		{
			const ZM_BattleEvent& xEvent = xCore.GetEngine().GetEvent(i);
			if (xEvent.m_eKind == ZM_BATTLE_EVENT_MOVE_USED
				&& xEvent.m_uSide == ZM_SIDE_PLAYER
				&& xEvent.m_uMoveId == (u_int)g_eRVWChosenMove)
			{
				g_bRVWChosenMoveUsed = true;
			}
		}

		if (!xCore.IsOver())
		{
			// The real HUD still owns submission. The exact L5 starter has two
			// legitimately learned moves. On MOVE_SELECT choose its second entry via
			// the same DOWN+ENTER edge a player uses (navigation is processed before
			// confirm); every other input frame confirms Fight / opens the menu.
			// Nothing about the battle state, party record, or authored trainer is
			// weakened to force an outcome.
			if (pxDirector->GetHudMenuScreen() == ZM_BATTLE_MENU_MOVE_SELECT
				&& pxDirector->GetHudMenuCursor() == 0)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			return true;
		}

		if (g_bRVWOutcomeCaptured) { return true; }
		g_eRVWWinner = xCore.GetWinner();
		g_uRVWFinalBattleHp = xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP;
		g_bRVWOutcomeCaptured = true;

		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr
			|| pxGameState->m_xParty.IsEmpty())
		{
			FailRVW("the persistent starter did not resolve at the natural loss");
			return false;
		}
		g_bRVWPendingObserved = pxGameState->m_bPendingWhiteout;
		if (g_eRVWWinner != ZM_SIDE_ENEMY || g_uRVWFinalBattleHp != 0u
			|| !g_bRVWPendingObserved)
		{
			FailRVW("the honest L5-vs-L5 battle did not produce ENEMY winner, fainted lead, and a pending whiteout");
			return false;
		}

		// The manager ran earlier this frame (order 104); the director set the latch
		// later (111). Dirty the durable lead only NOW, after the core's natural loss,
		// so every dimension of HealAllFull has an observable before/after value.
		ZM_Monster& xLead = pxGameState->m_xParty.Lead();
		if (xLead.m_axMoves[0].m_eMove == ZM_MOVE_NONE
			|| xLead.m_axMoves[0].m_uMaxPP == 0u)
		{
			FailRVW("the exact starter has no move-0 PP for the whiteout heal probe");
			return false;
		}
		xLead.m_uCurrentHp = 1u;
		xLead.m_axMoves[0].m_uCurPP = 0u;
		xLead.m_eStatus = ZM_MAJOR_STATUS_POISON;
		g_bRVWDirtyHealProbeInstalled = true;
		return true;
	}

	void Setup_ZMRivalVesperWhiteout()
	{
		RVWResetCaptures();

		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bRVWPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath) && bWarm;
		if (!g_bRVWPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fRVW_FIXED_DT);
		ZM_SetInstantBattlesForTests(true);
		g_xEngine.Scenes().LoadSceneByIndex(iRVW_DAWNMERE_BUILD, SCENE_LOAD_SINGLE);
		g_eRVWPhase = RVWPhase::AwaitReady;
		g_bRVWActive = true;
	}

	bool Step_ZMRivalVesperWhiteout(int)
	{
		if (!g_bRVWActive || g_bRVWFailed || g_eRVWPhase == RVWPhase::Done)
		{
			return false;
		}
		++g_iRVWPhaseFrames;

		switch (g_eRVWPhase)
		{
		case RVWPhase::AwaitReady:
		{
			RVPlayerView xPlayer;
			RVCameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iRVWPhaseFrames > iRVW_READY_DEADLINE)
				{
					FailRVW("Dawnmere did not become runtime-ready for the loss walk");
					return false;
				}
				return true;
			}
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			ZM_GameStateManager* pxManager = RVWResolveManager();
			ZM_GameState* pxGameState = nullptr;
			RVWVesperView xVesper;
			if (pxTransition == nullptr || pxManager == nullptr
				|| !ZM_GameStateManager::TryGetGameState(pxGameState)
				|| pxGameState == nullptr || RVWFindVesper(xVesper) != 1u
				|| xVesper.m_eTrainer != eRVW_TRAINER)
			{
				FailRVW("the authored Vesper, transition, manager, or game state did not resolve uniquely");
				return false;
			}

			// The exact production starter: one full-health L5 Fernfawn, no edits --
			// composed exactly as ZM_GameStateManager's seed sites compose it.
			*pxGameState = ZM_MakeNewGameState();
			ZM_ApplyStarterChoice(*pxGameState, ZM_STARTER_CHOICE_FERNFAWN);
			if (pxGameState->m_xParty.Count() != 1u)
			{
				FailRVW("the new-game + Fernfawn composition did not produce exactly one member");
				return false;
			}
			const ZM_Monster& xLead = pxGameState->m_xParty.Get(0u);
			const ZM_TrainerData& xVesperRow = ZM_GetTrainerData(eRVW_TRAINER);
			if (xLead.m_eSpecies != ZM_SPECIES_FERNFAWN || xLead.m_uLevel != 5u
				|| xLead.m_uCurrentHp != xLead.GetMaxHP()
				|| xLead.m_eStatus != ZM_MAJOR_STATUS_NONE
				|| xVesperRow.m_uPartyCount == 0u
				|| xVesperRow.m_paxParty[0].m_eSpecies != ZM_SPECIES_KINDLET
				|| xVesperRow.m_paxParty[0].m_uLevel != 5u)
			{
				FailRVW("the honest matchup is not full L5 Fernfawn versus authored L5 Kindlet");
				return false;
			}

			g_bRVWStarterCaptured = true;
			g_uRVWMoneyBefore = pxGameState->m_uMoney;
			g_uRVWExpBefore = xLead.m_uCurrentExp;
			g_uRVWLevelBefore = xLead.m_uLevel;
			g_uRVWMaxHpBefore = xLead.GetMaxHP();
			g_uRVWMaxPpBefore = xLead.m_axMoves[0].m_uMaxPP;
			g_bRVWFlagBefore = ZM_IsStoryFlagSet(
				*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
			g_uRVWManagerLoadsBefore = pxManager->GetIssuedLoadRequestCount();
			g_xRVWOriginalVesperID = xVesper.m_xEntityID;
			g_xRVWVesperPosition = xVesper.m_xPosition;
			g_fRVWInitialSeparation = RVWPlanarDistance(
				xPlayer.m_xPosition, xVesper.m_xPosition);
			g_fRVWBestSeparation = g_fRVWInitialSeparation;
			if (g_bRVWFlagBefore || pxGameState->m_bPendingWhiteout
				|| g_fRVWInitialSeparation <= fZM_SIGHT_MAX_DISTANCE * 1.25f)
			{
				FailRVW("the loss walk began with a defeated/pending rival or inside the spawn-camp margin");
				return false;
			}
			g_eRVWPhase = RVWPhase::Approach;
			g_iRVWPhaseFrames = 0;
			return true;
		}

		case RVWPhase::Approach:
		{
			if (RVTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
			{
				RVWClearInput();
				g_bRVWWalkedIntoCone = true;
				g_eRVWPhase = RVWPhase::DismissChallenge;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			if (ZM_BattleTransition::IsTransitionActive())
			{
				RVWClearInput();
				g_bRVWWalkedIntoCone = true;
				g_eRVWPhase = RVWPhase::AwaitInBattle;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			RVPlayerView xPlayer;
			RVWVesperView xVesper;
			if (!FindActivePlayer(xPlayer) || RVWFindVesper(xVesper) != 1u)
			{
				FailRVW("the player or authored Vesper disappeared during the loss walk");
				return false;
			}
			const float fDistance = RVWPlanarDistance(
				xPlayer.m_xPosition, xVesper.m_xPosition);
			if (fDistance < g_fRVWBestSeparation - 0.01f)
			{
				g_fRVWBestSeparation = fDistance;
				g_iRVWStallFrames = 0;
			}
			else if (++g_iRVWStallFrames > 90)
			{
				FailRVW("the physical loss walk stopped closing on authored Vesper");
				return false;
			}
			if (g_iRVWPhaseFrames > iRVW_APPROACH_DEADLINE)
			{
				FailRVW("the physical walk never entered authored Vesper's sight encounter");
				return false;
			}
			RVWDriveToward(xPlayer.m_xPosition, xVesper.m_xPosition);
			return true;
		}

		case RVWPhase::DismissChallenge:
			RVWClearInput();
			if (ZM_BattleTransition::IsTransitionActive())
			{
				g_eRVWPhase = RVWPhase::AwaitInBattle;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			if (g_iRVWPhaseFrames > iRVW_BARK_DEADLINE)
			{
				FailRVW("Vesper's challenge bark did not hand off to the battle");
				return false;
			}
			if (ZM_UI_MenuStack::IsMenuOpen())
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;

		case RVWPhase::AwaitInBattle:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailRVW("the battle transition disappeared before the honest loss");
				return false;
			}
			if (!g_bRVWChannelCaptured
				&& pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
			{
				g_eRVWChannelTrainer = pxTransition->GetBattleTrainer();
				g_bRVWChannelCaptured = true;
			}
			if (!RVWCaptureBattleAndDrive()) { return false; }
			if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
			{
				g_bRVWReachedInBattle = true;
				g_eRVWPhase = RVWPhase::DriveBattle;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			if (g_iRVWPhaseFrames > iRVW_IN_BATTLE_DEADLINE)
			{
				FailRVW("Vesper's loss encounter never reached IN_BATTLE");
				return false;
			}
			return true;
		}

		case RVWPhase::DriveBattle:
			if (!RVWCaptureBattleAndDrive()) { return false; }
			if (g_bRVWOutcomeCaptured)
			{
				g_eRVWPhase = RVWPhase::AwaitBattleResume;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			if (g_iRVWPhaseFrames > iRVW_BATTLE_DEADLINE)
			{
				FailRVW("the honest L5 rival battle did not resolve through the real menu");
				return false;
			}
			return true;

		case RVWPhase::AwaitBattleResume:
		{
			if (!RVWCaptureBattleAndDrive()) { return false; }
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			ZM_GameState* pxGameState = nullptr;
			if (pxTransition != nullptr
				&& pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
				&& pxTransition->GetCompletedBattleCount() == 1u
				&& ZM_GameStateManager::TryGetGameState(pxGameState)
				&& pxGameState != nullptr && pxGameState->m_bPendingWhiteout)
			{
				g_bRVWBattleResumeObserved = true;
				g_eRVWPhase = RVWPhase::AwaitWhiteout;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			if (g_iRVWPhaseFrames > iRVW_BATTLE_DEADLINE)
			{
				FailRVW("the lost rival battle did not resume to IDLE with the whiteout still pending");
				return false;
			}
			return true;
		}

		case RVWPhase::AwaitWhiteout:
		{
			RVWClearInput();
			ZM_GameStateManager* pxManager = RVWResolveManager();
			if (pxManager != nullptr
				&& pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE)
			{
				g_bRVWWhiteoutWarpObserved = true;
			}
			ZM_GameState* pxGameState = nullptr;
			const bool bHaveGameState = ZM_GameStateManager::TryGetGameState(pxGameState)
				&& pxGameState != nullptr && !pxGameState->m_xParty.IsEmpty();
			RVPlayerView xPlayer;
			RVCameraView xCamera;
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			const int iBuild = xActive.IsValid()
				? g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex : -1;
			const bool bSettled = pxManager != nullptr && bHaveGameState
				&& g_bRVWWhiteoutWarpObserved
				&& pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE
				&& pxManager->GetIssuedLoadRequestCount()
					== g_uRVWManagerLoadsBefore + 1u
				&& !pxGameState->m_bPendingWhiteout
				&& iBuild == iRVW_DAWNMERE_BUILD
				&& DawnmereRuntimeReady(xPlayer, xCamera)
				&& xPlayer.m_pxController->IsMovementEnabled()
				&& pxTransition != nullptr
				&& pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE;
			if (bSettled)
			{
				RVWVesperView xVesper;
				if (RVWFindVesper(xVesper) != 1u
					|| !RVWSampleTownCenter(xPlayer))
				{
					FailRVW("the whiteout destination lacked unique Vesper or TownCenter placement");
					return false;
				}
				const ZM_Monster& xLead = pxGameState->m_xParty.Lead();
				g_bRVWWhiteoutSettled = true;
				g_bRVWPendingAfter = pxGameState->m_bPendingWhiteout;
				g_uRVWHpAfter = xLead.m_uCurrentHp;
				g_uRVWMaxHpAfter = xLead.GetMaxHP();
				g_uRVWPpAfter = xLead.m_axMoves[0].m_uCurPP;
				g_uRVWMaxPpAfter = xLead.m_axMoves[0].m_uMaxPP;
				g_eRVWStatusAfter = xLead.m_eStatus;
				g_uRVWMoneyAfter = pxGameState->m_uMoney;
				g_uRVWExpAfter = xLead.m_uCurrentExp;
				g_bRVWFlagAfter = ZM_IsStoryFlagSet(
					*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
				g_uRVWManagerLoadsAfter = pxManager->GetIssuedLoadRequestCount();
				g_uRVWTransitionLoadsAfter = pxTransition->GetIssuedLoadRequestCount();
				g_uRVWEncounterCountAfter = pxTransition->GetObservedEncounterCount();
				g_uRVWCompletedAfter = pxTransition->GetCompletedBattleCount();
				g_uRVWAbortedAfter = pxTransition->GetAbortedTransitionCount();
				g_bRVWTownCenterTag = std::strcmp(
					ZM_GameStateManager::GetActiveSceneArrivedSpawnTag(),
					szRVW_EXPECTED_WHITEOUT_TAG) == 0;
				g_xRVWReloadedVesperID = xVesper.m_xEntityID;
				g_uRVWReloadedRaise = xVesper.m_uRaiseCount;
				g_uRVWReloadedChallenge = xVesper.m_uChallengeCount;
				g_bRVWVesperRearmed = xVesper.m_eState == ZM_TRAINER_SIGHT_WATCHING;
				g_eRVWPhase = RVWPhase::HoldRearmed;
				g_iRVWPhaseFrames = 0;
				return true;
			}
			if (g_iRVWPhaseFrames > iRVW_WHITEOUT_DEADLINE)
			{
				FailRVW("the pending loss did not heal and whiteout-warp to settled TownCenter");
				return false;
			}
			return true;
		}

		case RVWPhase::HoldRearmed:
		{
			RVWClearInput();
			ZM_GameStateManager* pxManager = RVWResolveManager();
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			ZM_GameState* pxGameState = nullptr;
			RVWVesperView xVesper;
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			const int iBuild = xActive.IsValid()
				? g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex : -1;
			if (pxManager == nullptr || pxTransition == nullptr
				|| !ZM_GameStateManager::TryGetGameState(pxGameState)
				|| pxGameState == nullptr || pxGameState->m_xParty.IsEmpty()
				|| RVWFindVesper(xVesper) != 1u)
			{
				FailRVW("a required singleton, state, or reloaded Vesper disappeared during the no-input hold");
				return false;
			}
			const ZM_Monster& xLead = pxGameState->m_xParty.Lead();
			if (iBuild != iRVW_DAWNMERE_BUILD
				|| pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
				|| pxManager->GetIssuedLoadRequestCount() != g_uRVWManagerLoadsAfter
				|| ZM_BattleTransition::IsTransitionActive()
				|| pxTransition->GetObservedEncounterCount() != 1u
				|| pxTransition->GetIssuedLoadRequestCount() != 1u
				|| pxTransition->GetCompletedBattleCount() != 1u
				|| pxTransition->GetAbortedTransitionCount() != 0u
				|| ZM_UI_MenuStack::IsMenuOpen()
				|| xVesper.m_eTrainer != eRVW_TRAINER || !xVesper.m_bSightEnabled
				|| xVesper.m_eState != ZM_TRAINER_SIGHT_WATCHING
				|| xVesper.m_uRaiseCount != 0u || xVesper.m_uChallengeCount != 0u
				|| pxGameState->m_bPendingWhiteout
				|| pxGameState->m_uMoney != g_uRVWMoneyBefore
				|| xLead.m_uCurrentExp != g_uRVWExpBefore
				|| ZM_IsStoryFlagSet(*pxGameState,
					ZM_STORY_FLAG_RIVAL1_DEFEATED))
			{
				FailRVW("the healed TownCenter hold re-triggered Vesper or mutated the loss outcome");
				return false;
			}
			if (g_iRVWPhaseFrames >= iRVW_NO_INPUT_HOLD_FRAMES)
			{
				g_bRVWNoInputHoldCompleted = true;
				g_eRVWPhase = RVWPhase::Done;
				return false;
			}
			return true;
		}

		case RVWPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMRivalVesperWhiteout()
	{
		bool bPassed = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_RivalVesperWhiteout] failed=%s (%s) starter=%s walk=%s "
			"separation=%.3f->%.3f channel=%u inBattle=%s matchup=(%u L%u vs %u L%u) "
			"outcome=%s winner=%u finalHP=%u pendingSeen=%s dirtyProbe=%s",
			g_bRVWFailed ? "true" : "false", g_szRVWFailure,
			g_bRVWStarterCaptured ? "true" : "false",
			g_bRVWWalkedIntoCone ? "true" : "false",
			g_fRVWInitialSeparation, g_fRVWBestSeparation,
			(u_int)g_eRVWChannelTrainer,
			g_bRVWReachedInBattle ? "true" : "false",
			(u_int)g_eRVWPlayerSpecies, g_uRVWPlayerLevel,
			(u_int)g_eRVWEnemySpecies, g_uRVWEnemyLevel,
			g_bRVWOutcomeCaptured ? "true" : "false", (u_int)g_eRVWWinner,
			g_uRVWFinalBattleHp, g_bRVWPendingObserved ? "true" : "false",
			g_bRVWDirtyHealProbeInstalled ? "true" : "false");
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_RivalVesperWhiteout] whiteout: battleResume=%s warpSeen=%s settled=%s "
			"loads=%u->%u pendingAfter=%s HP=%u/%u PP=%u/%u status=%u "
			"money=%u->%u exp=%u->%u flag=%s->%s encounters=%u completed=%u "
			"battleLoads=%u aborted=%u townTag=%s townResolved=%s transformErr=%.4f bodyErr=%.4f",
			g_bRVWBattleResumeObserved ? "true" : "false",
			g_bRVWWhiteoutWarpObserved ? "true" : "false",
			g_bRVWWhiteoutSettled ? "true" : "false",
			g_uRVWManagerLoadsBefore, g_uRVWManagerLoadsAfter,
			g_bRVWPendingAfter ? "true" : "false",
			g_uRVWHpAfter, g_uRVWMaxHpAfter, g_uRVWPpAfter, g_uRVWMaxPpAfter,
			(u_int)g_eRVWStatusAfter, g_uRVWMoneyBefore, g_uRVWMoneyAfter,
			g_uRVWExpBefore, g_uRVWExpAfter,
			g_bRVWFlagBefore ? "true" : "false",
			g_bRVWFlagAfter ? "true" : "false",
			g_uRVWEncounterCountAfter, g_uRVWCompletedAfter,
			g_uRVWTransitionLoadsAfter, g_uRVWAbortedAfter,
			g_bRVWTownCenterTag ? "true" : "false",
			g_bRVWTownCenterResolved ? "true" : "false",
			g_fRVWTransformSpawnError, g_fRVWBodySpawnError);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_RivalVesperWhiteout] rearm: original=%llu reloaded=%llu stateWatching=%s "
			"raise=%u challenge=%u noInputHold=%s (%d frames)",
			(unsigned long long)g_xRVWOriginalVesperID.GetPacked(),
			(unsigned long long)g_xRVWReloadedVesperID.GetPacked(),
			g_bRVWVesperRearmed ? "true" : "false", g_uRVWReloadedRaise,
			g_uRVWReloadedChallenge,
			g_bRVWNoInputHoldCompleted ? "true" : "false",
			iRVW_NO_INPUT_HOLD_FRAMES);

		if (g_bRVWFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] %s", g_szRVWFailure);
			bPassed = false;
		}
		if (!g_bRVWStarterCaptured || g_uRVWLevelBefore != 5u
			|| g_uRVWMaxHpBefore == 0u || g_uRVWMaxPpBefore == 0u
			|| g_bRVWFlagBefore)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] the exact healthy L5 starter baseline was not captured");
			bPassed = false;
		}
		if (!g_bRVWWalkedIntoCone
			|| g_fRVWBestSeparation >= g_fRVWInitialSeparation
			|| !g_bRVWChannelCaptured || g_eRVWChannelTrainer != eRVW_TRAINER
			|| !g_bRVWReachedInBattle)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] the authored physical walk/channel proof is incomplete");
			bPassed = false;
		}
		if (!g_bRVWBattleConfigCaptured || !g_bRVWLiveHealthyMatchup
			|| g_eRVWPlayerSpecies != ZM_SPECIES_FERNFAWN || g_uRVWPlayerLevel != 5u
			|| g_eRVWEnemySpecies != ZM_SPECIES_KINDLET || g_uRVWEnemyLevel != 5u
			|| g_bRVWCatchAllowed || g_bRVWFleeAllowed
			|| g_eRVWChosenMove == ZM_MOVE_NONE || !g_bRVWChosenMoveUsed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] the live core was not L5 Fernfawn versus L5 Kindlet");
			bPassed = false;
		}
		if (!g_bRVWOutcomeCaptured || g_eRVWWinner != ZM_SIDE_ENEMY
			|| g_uRVWFinalBattleHp != 0u || !g_bRVWPendingObserved
			|| !g_bRVWDirtyHealProbeInstalled || !g_bRVWBattleResumeObserved)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] no natural ENEMY win -> faint -> pending-whiteout chain was observed");
			bPassed = false;
		}
		if (!g_bRVWWhiteoutWarpObserved || !g_bRVWWhiteoutSettled
			|| g_uRVWManagerLoadsAfter != g_uRVWManagerLoadsBefore + 1u
			|| g_bRVWPendingAfter || g_uRVWHpAfter != g_uRVWMaxHpAfter
			|| g_uRVWPpAfter != g_uRVWMaxPpAfter
			|| g_eRVWStatusAfter != ZM_MAJOR_STATUS_NONE)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] exactly-one warp did not fully heal HP, PP, and status");
			bPassed = false;
		}
		if (g_uRVWMoneyAfter != g_uRVWMoneyBefore
			|| g_uRVWExpAfter != g_uRVWExpBefore || g_bRVWFlagAfter)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] the trainer loss paid money/exp or set the defeat flag");
			bPassed = false;
		}
		if (g_uRVWEncounterCountAfter != 1u || g_uRVWCompletedAfter != 1u
			|| g_uRVWTransitionLoadsAfter != 1u
			|| g_uRVWAbortedAfter != 0u || !g_bRVWTownCenterTag
			|| !g_bRVWTownCenterResolved
			|| g_fRVWTransformSpawnError > fRVW_POSITION_EPSILON
			|| g_fRVWBodySpawnError > fRVW_POSITION_EPSILON)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] round-trip counts or exact TownCenter placement failed");
			bPassed = false;
		}
		if (g_xRVWReloadedVesperID == g_xRVWOriginalVesperID
			|| !g_bRVWVesperRearmed || g_uRVWReloadedRaise != 0u
			|| g_uRVWReloadedChallenge != 0u || !g_bRVWNoInputHoldCompleted)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_RivalVesperWhiteout] reloaded Vesper was not a fresh watcher through the no-input hold");
			bPassed = false;
		}
		return bPassed || !g_bRVWPrereqsPresent;
	}

	void Teardown_ZMRivalVesperWhiteout()
	{
		RVWClearInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		Zenith_InputSimulator::ResetAllInputState();
		g_bRVWActive = false;
	}
}

static const Zenith_AutomatedTest g_xZMRivalVesperWhiteoutTest = {
	"ZM_RivalVesperWhiteout_Test",
	&Setup_ZMRivalVesperWhiteout,
	&Step_ZMRivalVesperWhiteout,
	&Verify_ZMRivalVesperWhiteout,
	// Above the sum of all independently reset phase budgets (5000 frames),
	// leaving the harness as a backstop rather than pre-empting a phase's own
	// diagnostic deadline.
	/* maxFrames */ 5400,
	false /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMRivalVesperWhiteout,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMRivalVesperWhiteoutTest);

#endif // ZENITH_INPUT_SIMULATOR
