#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_TrainerSight -- the windowed/headless gate for S7 item 3 SC6:
// A TRAINER WHO SEES YOU STARTS A BATTLE, end to end, in the real baked Dawnmere.
//
// ONE test, ZM_TrainerSightWalkUp_Test, m_bRequiresGraphics = FALSE. It asserts
// nothing about pixels, so it runs FOR REAL on the Null (GPU-less) backend
// instead of skipping as a pass.
//
// WHAT IT DRIVES, and what it deliberately does not. The player WALKS on Jolt
// velocity (no SetPosition anywhere) into a runtime-placed trainer's forward
// cone; ZM_Interactable::TickTrainerSight runs the SC3 pure cone, then the SC6
// occlusion ray, then the pure FSM, and dispatches ZM_OnTrainerEncounter into the
// SHIPPED SC5 subscriber. Everything past that dispatch -- the freeze, the fade,
// the additive Battle load, the prize and the defeat flag -- was already proven by
// ZM_TrainerBattle_Test; what is NEW here is the RAISE, and the two negatives that
// stop it firing twice.
//
// THE FLAGGED/FLAGLESS ASYMMETRY IS DELIBERATE (Q-2026-07-28-001) and will look
// like a bug: a row WITH a defeat flag is gated on the flag and ignores the
// session latch (losing to Vesper leaves him re-battleable -- heal up and come
// back); a row WITHOUT one is gated on the latch, which is set on the RAISE, so
// its prize is not farmable. Phase (7) proves the first arm and phase (8) the
// second. Do not "fix" either.
//
// S7 item 3 SC7 EXTENDS THE SAME WALK with the challenge bark: phases (4b) and
// (4c) sit between the approach and the battle, and phase (9) is appended after the
// flagless arm. (4b) proves the bark PRECEDES the encounter, (4c) proves the
// order-112-closes-before-order-113-dispatches handoff lands the battle within two
// frames of the box closing, and (9) proves a row with ZERO challenge lines skips
// CHALLENGING and battles anyway after the shared W3 marker. NO new
// ZENITH_AUTOMATED_TEST_REGISTER: the automated registry count deliberately does
// not move.
//
// S7 item 1 SC2 APPENDS ONE MORE PHASE to the same walk: (7a1), between the settle
// and the re-arm, proves the FOURTH freeze owner. With ZM_TrainerCinematicLatch armed
// a dialogue closing must NOT hand movement back, and -- in the SAME run, on the SAME
// player, through the SAME close path -- with the latch ended it MUST. The pairing is
// the whole design: "still frozen" alone is satisfied by a build in which nothing ever
// unfreezes anybody, which is precisely the regression a fourth writer of a
// non-refcounted bool invites. Again NO new ZENITH_AUTOMATED_TEST_REGISTER: the
// automated registry count deliberately does not move.
//
// S7 item 1 SC3 ADDS NO PHASE AND NO FRAME, DELIBERATELY. Its claim about this
// suite is a NEGATIVE -- the trainer placed here carries NO COLLIDER, so
// ZM_Interactable::IsDrivableBodyContractMet answers false for it and the walk-up
// must not happen at all. A negative that has to hold on every frame is sampled
// per frame (TSSampleApproachFailOpen, called ahead of the phase switch) rather
// than in a phase of its own, and the SHIPPED maxFrames below is unchanged --
// which is itself part of the claim: teaching the FSM to walk cost a trainer who
// cannot walk exactly nothing. This is the only place in the repo where that
// fail-open is observed on a LIVE component instead of in a truth table, which is
// why the fixture must stay collider-less. Again NO new
// ZENITH_AUTOMATED_TEST_REGISTER: the automated registry count does not move.
//
// TWO SMALL THINGS IN THE MIDDLE ARE LOAD-BEARING; neither is decoration:
//   * phase (7a) samples ZM_TrainerEngagementLatch::HasEngaged AFTER the walk-up
//     encounter. That is the ONLY observation in this repo of the production
//     MarkEngaged inside ZM_Interactable::TickTrainerSight -- phase (8) marks the
//     flagless row BY HAND, so it cannot see that write.
//   * phase (7a2) breaks the trainer's line of sight and re-enters, then clears the
//     session latch. Without it the hold's "raise count stayed 1" is pinned by the
//     FSM's own confirmed-raise latch and by the latch arm of ZM_MayTrainerEngage,
//     and says nothing whatever about the defeat-flag gate it claims to prove.
//
// NO Zenith_EventDispatcher::ScopedTestIsolation -- that guard STEALS the live
// subscription tables, which would delete the very subscriber under test. This
// file adds no subscription of its own. (Same ruling as ZM_AutoTests_TrainerBattle.)
//
// Per-phase driver functions, never one monolithic Step (the ZM-D-141 stack rule),
// and EVERY singleton / component pointer is re-resolved each frame because
// component pools relocate on swap-and-pop.
//
// ★ THE KNOWN FLAKE SURFACE IS WILD GRASS. A wild encounter latching first makes
// ZM_BattleTransition::OnTrainerEncounterEvent silently DROP the trainer raise
// (Dispatch returns void). The walk line is therefore the -X corridor at z = 480
// that ZM_PlayerHomeRoundTrip_Test already traverses for 128 m without wild
// interference, and phase (5) asserts GetBattleTrainer() == ZM_TRAINER_RIVAL_VESPER
// and NAMES grass in its failure text. If it ever flakes, MOVE THE WALK LINE --
// never weaken that assertion.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"       // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"               // the bark's screen model (GetTopScreen / IsMenuOpen)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"      // ZM_SetInstantBattlesForTests
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"                // ZM_IsStoryFlagSet
#include "Zenithmon/Source/Data/ZM_TrainerData.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"    // the latch + the observed state
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"  // the SC3 cone, for the anti-vacuity poll
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"                  // ZM_BuildMonsterRecord
#include "Zenithmon/Source/Party/ZM_Party.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

namespace
{
	// -------------------------------------------------------------------------
	// Constants
	// -------------------------------------------------------------------------

	// THE SINGLE DIFFICULTY KNOB, exactly as ZM_TrainerBattle_Test spells it:
	// Vesper's authored lead is a L5 KINDLET whose type favours the ENEMY, so the
	// 55-level gap is what makes the win reliable. If the player ever fails to win,
	// RAISE this -- never weaken an assertion.
	constexpr ZM_SPECIES_ID eTS_PLAYER_LEAD_SPECIES = ZM_SPECIES_FERNFAWN;
	constexpr u_int         uTS_PLAYER_LEAD_LEVEL   = 60u;

	// The FLAGGED row the walk-up forces, and its authored prize.
	constexpr ZM_TRAINER_ID eTS_TRAINER          = ZM_TRAINER_RIVAL_VESPER;
	constexpr u_int         uTS_EXPECTED_PRIZE   = 500u;
	// The FLAGLESS row phase (8) reconfigures onto the SAME entity: its gate is the
	// session latch, not a story flag.
	constexpr ZM_TRAINER_ID eTS_FLAGLESS_TRAINER = ZM_TRAINER_ROUTE1_RAMBLER;

	// Dawnmere's build index (ZM_WorldSpec / ZM-D-012).
	constexpr int iTS_OVERWORLD_BUILD_INDEX = 2;

	// Coarser than 1/60 for the same reason the trainer-battle gate is: the trip
	// has to fit an additive load + arena build + the battle + the resume poll
	// chains inside the frame budget.
	constexpr float fTS_FIXED_DT = 1.0f / 30.0f;

	// ---- Where the trainer stands ------------------------------------------
	//
	// -X of the live player position, on the z = 480 corridor. That corridor is the
	// ONE long walk in this game with measured evidence of no wild interference
	// (ZM_PlayerHomeRoundTrip_Test drives 128 m of it), and it is clear of every
	// authored occluder: the Home shell is x 376..392, the flank NPCs sit at
	// z = 498, and the wanderer patrols x = 540 -- i.e. BEHIND the trainer, never
	// between it and the player.
	constexpr float fTS_TRAINER_OFFSET_X = -12.0f;
	// The SPAWN-CAMP GUARD: the placement must clear the shipped sight range by a
	// real margin, or the approach phase would prove nothing (the trainer would
	// already have seen the player standing still).
	constexpr float fTS_MIN_SPAWN_SEPARATION = fZM_SIGHT_MAX_DISTANCE * 1.25f;

	// ---- Phase budgets. Each phase owns a deadline that FAILS with a diagnostic;
	// the harness's maxFrames is only a backstop above their sum. ----
	constexpr int iTS_READY_DEADLINE    = 420;
	constexpr int iTS_BASIS_FRAMES      = 30;
	constexpr int iTS_APPROACH_DEADLINE = 900;
	constexpr int iTS_INBATTLE_DEADLINE = 600;
	constexpr int iTS_DRIVE_DEADLINE    = 900;
	constexpr int iTS_SETTLE_FRAMES     = 8;
	constexpr int iTS_REARM_DEADLINE    = 120;
	constexpr int iTS_HOLD_FRAMES       = 200;
	constexpr int iTS_HOLD2_FRAMES      = 200;

	// ---- W3: the live SPOTTED visual beat -----------------------------------
	// Sample a few real movement frames, break the cone during SPOTTED, then
	// re-enter and let the full presentation window complete before SC7's bark.
	constexpr int   iTS_SPOTTED_CANCEL_SAMPLES      = 3;
	constexpr int   iTS_SPOTTED_CANCEL_DEADLINE     = 30;
	constexpr int   iTS_SPOTTED_REENTRY_DEADLINE    = 60;
	constexpr int   iTS_SPOTTED_COMPLETE_DEADLINE   = 60;
	constexpr int   iTS_SPOTTED_MIN_COMPLETE_SAMPLES = 10;
	constexpr float fTS_SPOTTED_MIN_MOVEMENT        = 0.10f;

	// ---- S7 item 3 SC7: the challenge bark's own budgets --------------------
	//
	// How long the bark is held up, un-pressed, proving it does not ride under the
	// battle fade.
	constexpr int iTS_CHALLENGE_HOLD_FRAMES = 30;
	// ★ THE OVERWORLD TYPEWRITER IS NOT COLLAPSIBLE. ZM_UI_DialogueBox reveals at a
	// hard `constexpr float fCHARS_PER_SEC = 45.0f` (ZM_UI_BattleHUD.cpp:37) and
	// passes only its OWN m_bRevealInstant -- it never consults
	// ZM_InstantBattlesEnabled() the way the battle log does, so the
	// ZM_SetInstantBattlesForTests(true) in Setup does NOTHING for a bark. Vesper's
	// two authored lines are 32 and 35 characters, i.e. ceil(32/45) + ceil(35/45) =
	// 2 s of reveal == 60 frames at the 1/30 dt this test pins. This is that budget
	// times three, so a slow frame cannot turn a working handoff into a red.
	constexpr int iTS_CHALLENGE_DISMISS_DEADLINE = 180;
	// THE HANDOFF. ECS order 112 (ZM_UI_MenuStack) pops, closes and unfreezes before
	// order 113 (ZM_Interactable) dispatches, in the SAME frame; the transition
	// machine accepts its pending latch on its next OnUpdate. Two frames is
	// deliberately generous for a one-frame handoff -- the exact frame relationship
	// between that synchronous Dispatch and the transition's IDLE-arm accept is
	// inherited from SC6's header comment, not measured.
	constexpr int iTS_BARK_TO_BATTLE_DEADLINE = 2;
	// THE SILENT ARM. A row with no challenge lines must reach the battle without a
	// challenge bark. W3's shared marker still runs first. This is BOTH the hold
	// WINDOW and the failure deadline: the phase runs the whole of it, asserting on
	// every frame that no DIALOGUE is up, and the raise + transition have to have
	// landed by the time it elapses. It is NOT an early exit -- see
	// TSPhaseRamblerNoBark.
	constexpr int iTS_RAMBLER_DEADLINE = 200;
	// The window's FIRST frame is spent clearing the two gates phase (8) closed, so the
	// no-DIALOGUE property is sampled on the remaining frames. Verify checks the sampled
	// count against this: a phase that went back to exiting the instant it saw the raise
	// would leave it at ~2 and red, rather than quietly shrinking a 200-frame claim to a
	// 3-frame one.
	constexpr int iTS_RAMBLER_SAMPLED_FRAMES = iTS_RAMBLER_DEADLINE - 1;

	// ---- S7 item 3 SC7: the ordering pin's UNRESOLVED sentinels -------------
	//
	// DISTINCT PER SAMPLE POINT, deliberately. The pin is an EQUALITY test between the
	// pre-walk encounter count and the count on the frame the bark was observed; if both
	// samples fell back to the SAME sentinel, a run in which the ZM_BattleTransition
	// singleton never resolved at either point would compare a sentinel against itself
	// and PASS having measured nothing. Distinct values make that run red the equality
	// clause too, and the resolved-bools beside them name WHY in the log.
	constexpr u_int uTS_ENCOUNTERS_UNRESOLVED_BEFORE  = 0xfffffffeu;
	constexpr u_int uTS_ENCOUNTERS_UNRESOLVED_AT_BARK = 0xffffffffu;

	// ---- S7 item 1 SC2: the CINEMATIC FREEZE LATCH's own budgets ------------
	//
	// How long the phase waits, after the battle round trip has settled, for the player
	// to be UNFROZEN again. This is the phase's ANTI-VACUITY PRECONDITION, not a
	// convenience: its negative half is "the player is STILL frozen", which a player
	// who was frozen the whole time satisfies for free.
	constexpr int iTS_CINE_UNFROZEN_DEADLINE = 90;
	// One confirm press per frame closes a ONE-LINE box in two frames: the first press
	// snaps the typewriter to the end and the second advances past the last line
	// (ZM_UI_DialogueBox::Confirm). This is that budget times thirty, because the
	// overworld typewriter is a hard constexpr 45 chars/sec that
	// ZM_SetInstantBattlesForTests does NOT collapse.
	constexpr int iTS_CINE_DISMISS_DEADLINE = 60;
	// How many frames "still frozen" is sampled on AFTER the armed box closed. One
	// frame would be satisfied by a release that merely arrived late -- and a
	// cinematic freeze released one frame late is still one the player walks out of.
	constexpr int iTS_CINE_HOLD_FRAMES = 20;
	// After End(), how long the release may take. ZM_UI_MenuStack::UnfreezePlayer
	// re-enables movement in the SAME call that closes the box, so this is slack
	// against a slow frame rather than a race.
	constexpr int iTS_CINE_RELEASE_DEADLINE = 30;

	// The approach must keep CLOSING; a second of no improvement means stuck
	// geometry / wrong basis / oscillation, and the test says so immediately.
	constexpr int   iTS_STALL_LIMIT_FRAMES = 60;
	constexpr float fTS_STALL_IMPROVEMENT  = 0.01f;
	// The basis probe must show meaningful, +Z-DOMINANT travel.
	constexpr float fTS_BASIS_MIN_FORWARD  = 0.5f;

	enum class TSPhase
	{
		AwaitReady,
		PlaceTrainer,
		BasisProbe,
		Approach,
		ObserveFirstSpotted,
		AwaitSpottedCancellation,
		AwaitSpottedReentry,
		ObserveSecondSpotted,
		// S7 item 3 SC7. INSERTED between Approach and AwaitInBattle so the bark is
		// proven inside the SHIPPED walk rather than paying a second ~500-frame
		// approach for it.
		AwaitChallenge,
		DismissChallenge,
		AwaitInBattle,
		DriveMenu,
		Settle,
		// S7 item 1 SC2. INSERTED between Settle and BreakSight: the one point in the
		// walk where Dawnmere is active, the transition is IDLE and the player is a
		// live, UNFROZEN controller -- i.e. the only place the fourth freeze owner can
		// be proven to both hold and release.
		CinematicFreeze,
		BreakSight,
		HoldInCone,
		FlaglessArm,
		// S7 item 3 SC7. APPENDED after the flagless arm: the same entity, now on a
		// SILENT row: must battle with no challenge bark after the shared marker.
		RamblerNoBark,
		Done,
	};

	// -------------------------------------------------------------------------
	// Entity views + asset guards (file-local copies -- the originals live in other
	// tests' anonymous namespaces and are not linkable across TUs).
	// -------------------------------------------------------------------------

	struct TSPlayerView
	{
		Zenith_EntityID           m_xEntityID    = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3     m_xPosition    = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController*      m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider   = nullptr;
	};

	struct TSCameraView
	{
		Zenith_EntityID         m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera*        m_pxFollow  = nullptr;
		Zenith_CameraComponent* m_pxCamera  = nullptr;
	};

	bool FindActivePlayer(TSPlayerView& xOut)
	{
		xOut = TSPlayerView{};
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

	bool FindActiveCamera(TSCameraView& xOut)
	{
		xOut = TSCameraView{};
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

	bool DawnmereRuntimeReady(TSPlayerView& xPlayer, TSCameraView& xCamera)
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

	// The top screen, or ZM_MENU_SCREEN_NONE when nothing is raised (which is exactly
	// what ZM_MenuScreenStack::Top() reports for an empty stack).
	//
	// ★ EVERY SC7 CLAUSE IS KEYED ON THIS MODEL, never on a rendered glyph count:
	// whether ZM_UI_DialogueBox::Present's post-wrap glyph total is non-zero on the
	// Null backend is UNVERIFIED, and this test runs there for real.
	ZM_MENU_SCREEN TSTopScreen()
	{
		const ZM_UI_MenuStack* pxMenu = ResolveMenuStack();
		return (pxMenu != nullptr) ? pxMenu->GetTopScreen() : ZM_MENU_SCREEN_NONE;
	}

	// -------------------------------------------------------------------------
	// Control state (ALL reset in Setup; batch mode reuses the process)
	// -------------------------------------------------------------------------

	TSPhase     g_eTSPhase          = TSPhase::Done;
	int         g_iTSPhaseFrames    = 0;
	bool        g_bTSActive         = false;
	bool        g_bTSFailed         = false;
	bool        g_bTSPrereqsPresent = false;
	const char* g_szTSFailure       = "test did not reach verification";

	// ---- The runtime-placed trainer ----
	Zenith_EntityID       g_xTSTrainerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xTSTrainerPosition(0.0f);
	bool                  g_bTSTrainerPlaced   = false;
	bool                  g_bTSTrainerArmed    = false;   // IsTrainerSightEnabled at placement
	float                 g_fTSSpawnSeparation = 0.0f;
	// The placement facing, and its exact BACK. Phase (7a2) turns the trainer through
	// the second to break sight and restores the first verbatim, so the hold's
	// geometry is bit-for-bit the geometry the walk-up already proved.
	Zenith_Maths::Quat    g_xTSTrainerFacing(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Quat    g_xTSTrainerAway(1.0f, 0.0f, 0.0f, 0.0f);

	// ---- Baselines captured BEFORE the encounter. "after" money defaults to
	//      0xffffffff so an unsampled run FAILS rather than passing on 0 == 0. ----
	bool  g_bTSBaselineCaptured = false;
	u_int g_uTSMoneyBefore      = 0u;
	bool  g_bTSFlagBefore       = true;    // want false; a true here IS the vacuity failure
	bool  g_bTSLatchBefore      = true;    // want false; same reason
	bool  g_bTSLeadInstalled    = false;

	// ---- Walk diagnostics ----
	Zenith_Maths::Vector3 g_xTSBasisStart(0.0f);
	float g_fTSBasisDeltaX      = 0.0f;
	float g_fTSBasisDeltaZ      = 0.0f;
	bool  g_bTSBasisPassed      = false;
	float g_fTSBestDistance     = 0.0f;
	float g_fTSCurrentDistance  = 0.0f;
	int   g_iTSStallFrames      = 0;
	bool  g_abTSHeldKeys[4]     = { false, false, false, false };   // W A S D

	// ---- W3: live SPOTTED presentation, cancellation, and re-entry ----
	bool  g_bTSFirstSpottedObserved       = false;
	bool  g_bTSSpottedCancellationIssued  = false;
	bool  g_bTSSpottedCancellationObserved = false;
	bool  g_bTSSpottedFacingRestored      = false;
	bool  g_bTSSecondSpottedObserved      = false;
	bool  g_bTSSpottedCompleted           = false;
	bool  g_bTSSpottedNeverFrozen         = true;
	bool  g_bTSSpottedPerFrameSubmissions = true;
	bool  g_bTSFirstSpottedSawCone        = false;
	bool  g_bTSSecondSpottedSawCone       = false;
	int   g_iTSFirstSpottedSamples        = 0;
	int   g_iTSSecondSpottedSamples       = 0;
	u_int g_uTSSpottedIndicatorBefore     = 0xffffffffu;
	u_int g_uTSSpottedIndicatorLast       = 0xffffffffu;
	u_int g_uTSSpottedIndicatorAfterCancel = 0xffffffffu;
	u_int g_uTSSpottedIndicatorAfterComplete = 0xffffffffu;
	u_int g_uTSSpottedCountAfterComplete  = 0xffffffffu;
	float g_fTSSpottedElapsedAfterComplete = 0.0f;
	float g_fTSFirstSpottedMovement       = 0.0f;
	float g_fTSSecondSpottedMovement      = 0.0f;
	Zenith_Maths::Vector3 g_xTSFirstSpottedStart(0.0f);
	Zenith_Maths::Vector3 g_xTSFirstSpottedEnd(0.0f);
	Zenith_Maths::Vector3 g_xTSSecondSpottedStart(0.0f);
	Zenith_Maths::Vector3 g_xTSSecondSpottedEnd(0.0f);

	// ---- Channel captures ----
	bool          g_bTSChannelCaptured = false;
	ZM_TRAINER_ID g_eTSChannelTrainer  = ZM_TRAINER_NONE;
	bool          g_bTSReachedInBattle = false;

	// ---- Post-resume samples ----
	bool  g_bTSAfterCaptured  = false;
	u_int g_uTSMoneyAfter     = 0xffffffffu;
	bool  g_bTSFlagAfter      = false;
	bool  g_bTSWhiteoutAfter  = true;      // want false (a WIN never latches a whiteout)
	u_int g_uTSCompletedAfter = 0xffffffffu;
	u_int g_uTSAbortedAfter   = 0xffffffffu;
	u_int g_uTSStateAfter     = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;   // want IDLE
	bool  g_bTSBattleUnloaded = false;
	u_int g_uTSEncountersAtSettle = 0xffffffffu;
	// ★ THE PRODUCTION SESSION LATCH. Sampled once, AFTER the encounter the walk-up
	// really drove, and it is the ONLY observation anywhere of
	// ZM_Interactable::TickTrainerSight's MarkEngaged call -- phase (8) sets the latch
	// BY HAND for the flagless row, so nothing there can see this write.
	bool  g_bTSLatchAfter     = false;     // want true

	// ---- Phase (7a2): BREAK SIGHT, then RE-ENTER (what gives the hold its teeth) ----
	bool  g_bTSRearmStarted     = false;
	bool  g_bTSRearmConeBroken  = false;   // the geometry really stopped seeing
	bool  g_bTSRearmBroke       = false;   // ...and the FSM really went back to WATCHING
	bool  g_bTSRearmRestored    = false;   // the placement facing is back
	u_int g_uTSRaiseAtRearm     = 0xffffffffu;
	u_int g_uTSStateAtBreakEntry = 0xffffffffu;   // diagnostic only; see the phase
	float g_fTSRearmSeparation  = 0.0f;

	// ---- Phase (7a1): THE CINEMATIC FREEZE LATCH, both halves (S7 item 1 SC2) ----
	// The negative ("still frozen") and the positive ("movement came back") are
	// separate observations on purpose: a build in which nothing ever unfreezes
	// anybody satisfies the negative for free, so Verify requires BOTH.
	bool g_bTSCineStarted         = false;
	bool g_bTSCineLatchWasClear   = false;   // want true: nothing leaked a freeze in
	bool g_bTSCineUnfrozenAtEntry = false;   // want true: THE precondition
	bool g_bTSCineArmed           = false;   // Begin() took, and named OUR trainer
	bool g_bTSCineBoxRaised       = false;   // the armed-half box really went up
	bool g_bTSCineFrozenByBox     = false;   // ...and raising it really froze the player
	bool g_bTSCineArmedBoxClosed  = false;
	int  g_iTSCineHeldFrames      = 0;
	bool g_bTSCineHeldWhileArmed  = false;   // want true: frozen on EVERY held frame
	bool g_bTSCineEnded           = false;   // End() took
	bool g_bTSCineBoxRaised2      = false;
	bool g_bTSCineFreeBoxClosed   = false;
	bool g_bTSCineReleased        = false;   // want true: movement CAME BACK
	int  g_iTSCineReleaseFrames   = -1;
	bool g_bTSCineCompleted       = false;

	// ---- S7 item 1 SC3: THE FAIL-OPEN, SAMPLED ON EVERY FRAME OF THE WHOLE RUN ----
	//
	// ★ WHAT THIS FIXTURE IS FOR, RESTATED. The trainer this test places carries NO
	// COLLIDER at all (see TSPhasePlaceTrainer: it is transient so it can never reach
	// the committed scene, and bodyless so it adds no static body to a scene several
	// other windowed tests walk through). That makes it the ONE live trainer in the
	// repo for which ZM_Interactable::IsDrivableBodyContractMet answers FALSE -- and
	// therefore the ONE place the walk-up's fail-open can be observed AT RUNTIME
	// rather than argued from a boot unit's truth table.
	//
	// The claim: with no body, the machine must behave EXACTLY as it did before the
	// walk-up existed -- SPOTTED hands straight to CHALLENGING, no approach is ever
	// booked, no cinematic freeze is ever taken, and the shipped frame budget
	// (maxFrames) DOES NOT MOVE. Sampled per frame rather than at phase boundaries so
	// a single-frame approach could not slip between two checks; costs one component
	// read on a component every phase already resolves.
	bool  g_bTSApproachEverPossible = false;      // want false: no body, no walk
	u_int g_uTSApproachCountMax     = 0u;         // want 0
	bool  g_bTSApproachHoldEverSeen = false;      // want false: no cinematic freeze
	bool  g_bTSApproachLatchEverSeen = false;     // want false, before phase (7a1) arms it
	bool  g_bTSApproachStateEverSeen = false;     // want false: never in APPROACHING
	// Sampled at the SPOTTED -> CHALLENGING handoff, which is the exact tick the walk
	// would have been inserted on.
	u_int g_uTSHandoffApproachCount = 0xffffffffu;   // want 0
	bool  g_bTSHandoffApproachPossible = true;       // want false
	bool  g_bTSHandoffSampled       = false;

	// ---- Phase (7): HOLD IN CONE (the flagged arm, end to end) ----
	bool  g_bTSHoldCompleted        = false;
	bool  g_bTSHoldSawCone          = false;      // anti-vacuity: the geometry really watched
	u_int g_uTSHoldMaxRaiseCount    = 0xffffffffu;
	u_int g_uTSHoldMinRaiseCount    = 0xffffffffu;
	bool  g_bTSHoldEverEngaged      = false;      // want false: the WATCHING arm ran all hold
	bool  g_bTSHoldTransitionMoved  = false;
	bool  g_bTSHoldEncountersMoved  = false;
	float g_fTSHoldSeparation       = 0.0f;

	// ---- Phases (4b/4c): THE SC7 CHALLENGE BARK ----
	// The encounter-count baseline the bark's ordering pin is measured against. It is
	// a PER-COMPONENT counter on the DontDestroyOnLoad singleton, so a batched run
	// needs a captured baseline rather than an assumed zero.
	//
	// ★ EACH SAMPLE CARRIES ITS OWN RESOLVED-BOOL. The pin is an EQUALITY test, so
	// "the singleton did not resolve" must never be able to look like "the counts
	// matched": Verify fails outright when either bool is false, and the two distinct
	// unresolved sentinels make the equality clause red as well.
	u_int g_uTSEncountersBeforeWalk = uTS_ENCOUNTERS_UNRESOLVED_BEFORE;
	bool  g_bTSEncountersBeforeWalkResolved = false;
	// ★ THE ANTI-VACUITY LATCH: the approach ended at a battle without a bark ever
	// being seen. Verify FAILS on it -- a silently barkless SC7 is precisely the
	// regression these phases exist to catch.
	bool  g_bTSBarkMissed          = false;
	bool  g_bTSBarkObserved        = false;
	// The four ordering pins, all latched on the FIRST frame the dialogue is seen.
	bool  g_bTSBarkTopWasDialogue  = false;
	bool  g_bTSBarkStateChallenging = false;
	bool  g_bTSBarkTransitionIdle  = false;
	u_int g_uTSBarkRaiseCount      = 0xffffffffu;   // want 0: the battle has NOT started
	u_int g_uTSBarkChallengeCount  = 0xffffffffu;   // want 1: the beat ran exactly once
	u_int g_uTSEncountersAtBark    = uTS_ENCOUNTERS_UNRESOLVED_AT_BARK;   // want == g_uTSEncountersBeforeWalk
	bool  g_bTSEncountersAtBarkResolved = false;
	bool  g_bTSBarkHoldCompleted   = false;
	// The dismissal + the one-frame handoff into the battle.
	bool  g_bTSDismissClosed       = false;
	int   g_iTSDismissFrames       = -1;
	bool  g_bTSBarkToBattleOK      = false;
	int   g_iTSBarkToBattleFrames  = -1;

	// ---- Phase (8): the FLAGLESS latch arm ----
	bool  g_bTSFlaglessConfigured   = false;
	bool  g_bTSFlaglessCompleted    = false;
	bool  g_bTSFlaglessSawCone      = false;
	u_int g_uTSFlaglessMaxRaise     = 0xffffffffu;
	bool  g_bTSFlaglessTransitionMoved = false;
	float g_fTSFlaglessSeparation   = 0.0f;

	// ---- Phase (9): THE SILENT ARM, END TO END ----
	bool  g_bTSRamblerStarted       = false;
	bool  g_bTSRamblerCompleted     = false;
	bool  g_bTSRamblerSawCone       = false;
	bool  g_bTSRamblerSawDialogue   = false;   // want FALSE on every frame of the window
	// How many frames the no-DIALOGUE property was ACTUALLY sampled on. Verify checks
	// it against iTS_RAMBLER_SAMPLED_FRAMES, so the size of the window is itself a
	// tested claim rather than a comment.
	int   g_iTSRamblerSampledFrames = 0;
	u_int g_uTSRamblerRaiseAtStart  = 0xffffffffu;
	u_int g_uTSRamblerMaxRaise      = 0u;
	u_int g_uTSRamblerMaxChallenge  = 0u;      // want 0: a silent row never enters the beat
	bool  g_bTSRamblerTransitionMoved = false;
	float g_fTSRamblerSeparation    = 0.0f;

	void ClearTSInput()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
		g_abTSHeldKeys[0] = false;
		g_abTSHeldKeys[1] = false;
		g_abTSHeldKeys[2] = false;
		g_abTSHeldKeys[3] = false;
	}

	void FailTS(const char* szReason)
	{
		g_szTSFailure = szReason;
		g_bTSFailed = true;
		g_eTSPhase = TSPhase::Done;
		ClearTSInput();
	}

	float PlanarDistance(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	// A LOCAL COPY of ZM_AutoTests_NpcTalk.cpp's CAMERA-RELATIVE DriveTowardXZ. It
	// cannot be shared (the original lives in that file's anonymous namespace), and
	// it MUST be the camera-relative version: player movement is camera-relative
	// and ZM_FollowCamera is a LAGGING spring, so a world-space key chooser was
	// MEASURED settling onto a stable 45-degree wrong heading. Copy THIS one.
	void DriveTowardXZ(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		ClearTSInput();
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

		if (fRightAmount < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_A);
			g_abTSHeldKeys[1] = true;
		}
		else if (fRightAmount > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_D);
			g_abTSHeldKeys[3] = true;
		}
		if (fForwardAmount < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_S);
			g_abTSHeldKeys[2] = true;
		}
		else if (fForwardAmount > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
			g_abTSHeldKeys[0] = true;
		}
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_LEFT_SHIFT);
	}

	// The runtime-placed trainer's live component. Re-resolved every frame.
	ZM_Interactable* ResolveTrainerComponent()
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_xTSTrainerEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_Interactable>()
			: nullptr;
	}

	// S7 item 1 SC3. ONE frame's worth of the fail-open, ORed into the run-wide
	// observations. Every one of these is a NEGATIVE that must hold on EVERY frame, so
	// they accumulate the WORST answer seen rather than the latest.
	void TSSampleApproachFailOpen()
	{
		const ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			return;
		}
		g_bTSApproachEverPossible =
			g_bTSApproachEverPossible || pxTrainer->IsTrainerApproachPossible();
		const u_int uApproachCount = pxTrainer->GetTrainerApproachCount();
		if (uApproachCount > g_uTSApproachCountMax)
		{
			g_uTSApproachCountMax = uApproachCount;
		}
		g_bTSApproachStateEverSeen = g_bTSApproachStateEverSeen
			|| pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_APPROACHING;
		g_bTSApproachHoldEverSeen = g_bTSApproachHoldEverSeen
			|| pxTrainer->IsTrainerCinematicHoldActive();
		// ★ SAMPLED ONLY UNTIL PHASE (7a1) ARMS THE LATCH BY HAND. That phase's whole
		// job is to Begin() the latch on this same trainer, so continuing past it would
		// make this observation say "a freeze happened" about the one freeze the test
		// itself asked for.
		if (!g_bTSCineArmed)
		{
			g_bTSApproachLatchEverSeen =
				g_bTSApproachLatchEverSeen || ZM_TrainerCinematicLatch::IsActive();
		}
	}

	// ...and its transform. Same re-resolve discipline, for the same reason.
	Zenith_TransformComponent* ResolveTrainerTransform()
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_xTSTrainerEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_TransformComponent>()
			: nullptr;
	}

	bool ResolveTrainerPose(
		Zenith_Maths::Vector3& xPositionOut, Zenith_Maths::Quat& xRotationOut)
	{
		Zenith_TransformComponent* pxTransform = ResolveTrainerTransform();
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xPositionOut);
		pxTransform->GetRotation(xRotationOut);
		return true;
	}

	// THE ANTI-VACUITY POLL for both hold phases: is the player GEOMETRICALLY still
	// in the trainer's cone? Without it, "no raise happened" is indistinguishable
	// from "nothing was watching", which is this project's most-repeated failure
	// mode. It calls the SC3 predicate directly -- the same one the component runs.
	bool PlayerIsInTrainerCone(float& fSeparationOut)
	{
		fSeparationOut = 0.0f;
		TSPlayerView xPlayer;
		Zenith_Maths::Vector3 xTrainerPosition(0.0f);
		Zenith_Maths::Quat xTrainerRotation(1.0f, 0.0f, 0.0f, 0.0f);
		if (!FindActivePlayer(xPlayer)
			|| !ResolveTrainerPose(xTrainerPosition, xTrainerRotation))
		{
			return false;
		}
		fSeparationOut = PlanarDistance(xPlayer.m_xPosition, xTrainerPosition);
		return ZM_IsTargetInTrainerSightFromRotation(
			xTrainerPosition, xTrainerRotation, xPlayer.m_xPosition,
			ZM_TrainerSightTuning{});
	}

	// One live-frame observation of W3's SPOTTED state. This deliberately couples
	// the pure state to the shipped component seam: movement remains enabled, the
	// cone is genuinely occupied, no bark/battle side effect has fired, and the
	// indicator submission counter advances exactly once per SPOTTED update.
	bool TSSampleSpotted(bool bFirstPass)
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		TSPlayerView xPlayer;
		if (pxTrainer == nullptr || !FindActivePlayer(xPlayer))
		{
			FailTS("the trainer or player vanished while sampling SPOTTED");
			return false;
		}
		if (pxTrainer->GetTrainerSightState() != ZM_TRAINER_SIGHT_SPOTTED)
		{
			FailTS("the trainer left SPOTTED before the requested live samples completed");
			return false;
		}
		if (xPlayer.m_pxController == nullptr || !xPlayer.m_pxController->IsMovementEnabled())
		{
			g_bTSSpottedNeverFrozen = false;
			FailTS("SPOTTED froze player movement before the challenge bark");
			return false;
		}

		float fSeparation = 0.0f;
		if (!PlayerIsInTrainerCone(fSeparation))
		{
			FailTS("a SPOTTED sample was not geometrically inside the trainer cone");
			return false;
		}
		if (bFirstPass)
		{
			g_bTSFirstSpottedSawCone = true;
		}
		else
		{
			g_bTSSecondSpottedSawCone = true;
		}

		const u_int uExpectedSpottedCount = bFirstPass ? 1u : 2u;
		if (ZM_UI_MenuStack::IsMenuOpen()
			|| ZM_BattleTransition::IsTransitionActive()
			|| pxTrainer->GetTrainerSightRaiseCount() != 0u
			|| pxTrainer->GetTrainerChallengeCount() != 0u
			|| ZM_TrainerEngagementLatch::HasEngaged(eTS_TRAINER)
			|| pxTrainer->GetTrainerSpottedCount() != uExpectedSpottedCount)
		{
			FailTS("SPOTTED produced an early bark/battle side effect or wrong entry count");
			return false;
		}

		const u_int uSubmissions = pxTrainer->GetTrainerSpottedIndicatorSubmitCount();
		if (g_uTSSpottedIndicatorLast == 0xffffffffu
			|| uSubmissions != g_uTSSpottedIndicatorLast + 1u)
		{
			g_bTSSpottedPerFrameSubmissions = false;
			FailTS("the spotted indicator was not submitted exactly once on a SPOTTED frame");
			return false;
		}
		g_uTSSpottedIndicatorLast = uSubmissions;
		if (!std::isfinite(pxTrainer->GetTrainerSpottedElapsedSeconds()))
		{
			FailTS("the live SPOTTED elapsed time was non-finite");
			return false;
		}

		if (bFirstPass)
		{
			++g_iTSFirstSpottedSamples;
			g_xTSFirstSpottedEnd = xPlayer.m_xPosition;
		}
		else
		{
			++g_iTSSecondSpottedSamples;
			g_xTSSecondSpottedEnd = xPlayer.m_xPosition;
		}
		DriveTowardXZ(xPlayer.m_xPosition, g_xTSTrainerPosition);
		return true;
	}

	// ★ THE ORDERING PROOF, latched on the FIRST frame the challenge dialogue is
	// observed. All four pins are captured AT ONCE and on that one frame, because the
	// mutation they exist to catch -- transposing the two arms of the action switch in
	// ZM_Interactable::TickTrainerSight, so the encounter is dispatched on
	// RUN_CHALLENGE and the bark fired on RAISE_ENCOUNTER -- still delivers the
	// battle. ONLY the order is evidence, so only these four have teeth.
	void TSLatchBarkObservation()
	{
		g_bTSBarkObserved = true;
		g_bTSBarkTopWasDialogue = TSTopScreen() == ZM_MENU_SCREEN_DIALOGUE;

		const ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer != nullptr)
		{
			g_bTSBarkStateChallenging =
				pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_CHALLENGING;
			g_uTSBarkRaiseCount = pxTrainer->GetTrainerSightRaiseCount();
			g_uTSBarkChallengeCount = pxTrainer->GetTrainerChallengeCount();
		}

		g_bTSBarkTransitionIdle = !ZM_BattleTransition::IsTransitionActive();
		const ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		// The RESOLVED-BOOL is the load-bearing half. Without it an unresolved sample
		// here would simply leave the sentinel in place, and comparing that against an
		// equally unresolved pre-walk sample would pass the ordering pin on 0 == 0
		// arithmetic -- the exact "passes for the wrong reason" shape this file is full
		// of guards against.
		g_bTSEncountersAtBarkResolved = pxTransition != nullptr;
		g_uTSEncountersAtBark = (pxTransition != nullptr)
			? pxTransition->GetObservedEncounterCount()
			: uTS_ENCOUNTERS_UNRESOLVED_AT_BARK;
	}

	// -------------------------------------------------------------------------
	// Per-phase drivers. Each returns true to keep stepping, false to stop.
	// -------------------------------------------------------------------------

	// (1) The overworld has finished coming up and the battle machine exists.
	bool TSPhaseAwaitReady()
	{
		TSPlayerView xPlayer;
		TSCameraView xCamera;
		if (!DawnmereRuntimeReady(xPlayer, xCamera))
		{
			if (g_iTSPhaseFrames > iTS_READY_DEADLINE)
			{
				FailTS("Dawnmere did not become runtime-ready in time");
				return false;
			}
			return true;
		}

		if (ResolveSingletonBattleTransition() == nullptr)
		{
			FailTS("no unique ZM_BattleTransition singleton -- the subscriber the "
				"sight FSM raises into cannot exist");
			return false;
		}

		g_eTSPhase = TSPhase::PlaceTrainer;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// (2) Install the deterministic-win lead, capture the payout baselines, then
	//     CREATE the trainer at runtime and arm its sight.
	bool TSPhasePlaceTrainer()
	{
		TSPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailTS("the player disappeared before the trainer could be placed");
			return false;
		}

		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr)
		{
			FailTS("no persistent ZM_GameState resolved -- the prize/flag payout has "
				"no target");
			return false;
		}
		g_uTSMoneyBefore = pxGameState->m_uMoney;
		g_bTSFlagBefore = ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
		// Setup cleared the whole engagement mask, so this must read FALSE -- and it is
		// what makes the post-encounter latch sample a genuine observation of the
		// PRODUCTION MarkEngaged rather than of something an earlier test left behind.
		g_bTSLatchBefore = ZM_TrainerEngagementLatch::HasEngaged(eTS_TRAINER);
		g_bTSBaselineCaptured = true;

		// S7 item 3 SC7's ordering baseline. GetObservedEncounterCount is a
		// PER-COMPONENT counter on the DontDestroyOnLoad singleton, so the bark's
		// "the encounter has NOT happened yet" pin is measured against this captured
		// value rather than against an assumed zero.
		const ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		g_bTSEncountersBeforeWalkResolved = pxTransition != nullptr;
		g_uTSEncountersBeforeWalk = (pxTransition != nullptr)
			? pxTransition->GetObservedEncounterCount()
			: uTS_ENCOUNTERS_UNRESOLVED_BEFORE;

		pxGameState->m_xParty = ZM_Party{};
		g_bTSLeadInstalled = pxGameState->m_xParty.Add(
			ZM_BuildMonsterRecord(eTS_PLAYER_LEAD_SPECIES, uTS_PLAYER_LEAD_LEVEL));
		if (!g_bTSLeadInstalled)
		{
			FailTS("could not install the deterministic-win party lead");
			return false;
		}

		// The trainer is a RUNTIME entity: TRANSIENT, so it can never be written
		// into the committed Dawnmere.zscen, and carrying no collider, so it adds no
		// static body to a scene several other windowed tests walk through.
		const Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_Entity xTrainer = g_xEngine.Scenes().CreateEntity(xScene, "ZM_SightTrainer");
		if (!xTrainer.IsValid())
		{
			FailTS("could not create the runtime trainer entity in Dawnmere");
			return false;
		}
		xTrainer.SetTransient(true);

		Zenith_TransformComponent* pxTransform =
			xTrainer.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			FailTS("the runtime trainer entity came up with no transform");
			return false;
		}
		g_xTSTrainerPosition = Zenith_Maths::Vector3(
			xPlayer.m_xPosition.x + fTS_TRAINER_OFFSET_X,
			xPlayer.m_xPosition.y,
			xPlayer.m_xPosition.z);
		pxTransform->SetPosition(g_xTSTrainerPosition);

		// FACE THE APPROACH. Derived from the placement offset rather than
		// hard-coded, and built with AngleAxis about +Y -- never
		// glm::eulerAngles(quat).y, which collapses past 90 degrees off +Z.
		const Zenith_Maths::Vector3 xToPlayer(
			xPlayer.m_xPosition.x - g_xTSTrainerPosition.x,
			0.0f,
			xPlayer.m_xPosition.z - g_xTSTrainerPosition.z);
		const float fYaw = std::atan2(xToPlayer.x, xToPlayer.z);
		g_xTSTrainerFacing =
			Zenith_Maths::AngleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		// The exact BACK of that facing, DERIVED the same way from the same vector
		// (never a hard-coded quaternion, never eulerAngles). Phase (7a2) turns the
		// trainer through it to break sight, then restores g_xTSTrainerFacing verbatim.
		g_xTSTrainerAway = Zenith_Maths::AngleAxis(
			std::atan2(-xToPlayer.x, -xToPlayer.z),
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		pxTransform->SetRotation(g_xTSTrainerFacing);

		ZM_Interactable& xInteractable = xTrainer.AddComponent<ZM_Interactable>();
		if (!xInteractable.ConfigureTrainerSight(eTS_TRAINER))
		{
			FailTS("ConfigureTrainerSight refused the authored rival row");
			return false;
		}
		g_xTSTrainerEntityID = xTrainer.GetEntityID();
		g_bTSTrainerPlaced = true;
		g_uTSSpottedIndicatorBefore =
			xInteractable.GetTrainerSpottedIndicatorSubmitCount();
		g_uTSSpottedIndicatorLast = g_uTSSpottedIndicatorBefore;
		g_bTSTrainerArmed = xInteractable.IsTrainerSightEnabled()
			&& xInteractable.GetTrainerId() == eTS_TRAINER
			&& xInteractable.GetTrainerSightRaiseCount() == 0u
			&& xInteractable.GetTrainerSpottedCount() == 0u
			&& xInteractable.GetTrainerSpottedElapsedSeconds() == 0.0f
			&& g_uTSSpottedIndicatorBefore == 0u;
		if (!g_bTSTrainerArmed)
		{
			FailTS("the placed trainer did not come up armed with zero sight counters");
			return false;
		}

		// THE SPAWN-CAMP GUARD. Placed inside the shipped sight range, the approach
		// phase below would prove nothing -- the trainer would have spotted a player
		// who never moved.
		g_fTSSpawnSeparation = PlanarDistance(xPlayer.m_xPosition, g_xTSTrainerPosition);
		if (g_fTSSpawnSeparation <= fTS_MIN_SPAWN_SEPARATION)
		{
			FailTS("the trainer was placed inside (or barely outside) the shipped "
				"sight range -- the walk-up would be vacuous");
			return false;
		}

		g_xTSBasisStart = xPlayer.m_xPosition;
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		g_abTSHeldKeys[0] = true;
		g_eTSPhase = TSPhase::BasisProbe;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// (3) BASIS PROBE. Held W must move the player, and +Z must dominate, so a
	//     broken movement basis fails in a second WITH the measured deltas instead
	//     of grinding out the approach deadline.
	bool TSPhaseBasisProbe()
	{
		TSPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailTS("the player disappeared during the basis probe");
			return false;
		}
		if (g_iTSPhaseFrames < iTS_BASIS_FRAMES)
		{
			return true;
		}
		g_fTSBasisDeltaX = xPlayer.m_xPosition.x - g_xTSBasisStart.x;
		g_fTSBasisDeltaZ = xPlayer.m_xPosition.z - g_xTSBasisStart.z;
		ClearTSInput();
		if (g_fTSBasisDeltaZ < fTS_BASIS_MIN_FORWARD
			|| std::fabs(g_fTSBasisDeltaZ) <= std::fabs(g_fTSBasisDeltaX))
		{
			FailTS("held W did not move the player forward along +Z -- the movement "
				"basis is wrong (measured deltas are logged below)");
			return false;
		}
		g_bTSBasisPassed = true;

		// Nothing may have raised yet: the probe walked ALONG +Z, not toward the
		// trainer, so it is still well outside sight range.
		const ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the runtime trainer vanished during the basis probe");
			return false;
		}
		if (pxTrainer->GetTrainerSightRaiseCount() != 0u)
		{
			FailTS("the trainer raised an encounter before the walk-up even began");
			return false;
		}

		g_fTSCurrentDistance =
			PlanarDistance(xPlayer.m_xPosition, g_xTSTrainerPosition);
		g_fTSBestDistance = g_fTSCurrentDistance;
		g_iTSStallFrames = 0;
		g_eTSPhase = TSPhase::Approach;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// (4) APPROACH -- a CLOSED LOOP on the live position with a progress watchdog.
	//     It ends the instant the shipped transition goes active; no SetPosition
	//     anywhere, so the player walks on Jolt velocity.
	bool TSPhaseApproach()
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the runtime trainer was lost during the approach");
			return false;
		}

		// W3 is observed before SC7: the visual beat must exist as its own live
		// state, with no dialogue, encounter, freeze, or transition yet.
		if (!g_bTSSpottedCompleted
			&& pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_SPOTTED)
		{
			TSPlayerView xPlayer;
			if (!FindActivePlayer(xPlayer))
			{
				FailTS("the player was lost on first entry to SPOTTED");
				return false;
			}
			g_bTSFirstSpottedObserved = true;
			g_xTSFirstSpottedStart = xPlayer.m_xPosition;
			g_xTSFirstSpottedEnd = xPlayer.m_xPosition;
			if (!TSSampleSpotted(true))
			{
				return false;
			}
			g_eTSPhase = TSPhase::ObserveFirstSpotted;
			g_iTSPhaseFrames = 0;
			return true;
		}

		// S7 item 3 SC7: THE BARK IS CHECKED FIRST, ahead of the transition, so a
		// frame carrying both would still be read as the bark -- SC7's whole claim is
		// that the dialogue PRECEDES the encounter, and reading it the other way round
		// would let the transposed-switch mutation slip through as "the bark was just
		// late".
		if (TSTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
		{
			if (!g_bTSSpottedCompleted)
			{
				FailTS("the challenge bark appeared before W3's SPOTTED window completed");
				return false;
			}
			ClearTSInput();
			TSLatchBarkObservation();
			g_eTSPhase = TSPhase::AwaitChallenge;
			g_iTSPhaseFrames = 0;
			return true;
		}

		if (ZM_BattleTransition::IsTransitionActive())
		{
			if (!g_bTSSpottedCompleted)
			{
				FailTS("the battle transition started before W3's SPOTTED window completed");
				return false;
			}
			// ANTI-VACUITY. The battle started and NO bark was ever seen. The run
			// continues (every SC6 clause below still means what it meant), but Verify
			// fails naming it: a silently barkless SC7 is exactly the regression the
			// challenge phases exist to catch.
			if (!g_bTSBarkObserved)
			{
				g_bTSBarkMissed = true;
			}
			ClearTSInput();
			g_eTSPhase = TSPhase::AwaitInBattle;
			g_iTSPhaseFrames = 0;
			return true;
		}

		TSPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			FailTS("the player was lost during the approach");
			return false;
		}
		g_fTSCurrentDistance =
			PlanarDistance(xPlayer.m_xPosition, g_xTSTrainerPosition);

		if (g_fTSCurrentDistance < g_fTSBestDistance - fTS_STALL_IMPROVEMENT)
		{
			g_fTSBestDistance = g_fTSCurrentDistance;
			g_iTSStallFrames = 0;
		}
		else if (++g_iTSStallFrames > iTS_STALL_LIMIT_FRAMES)
		{
			FailTS("the walk-up STALLED -- the player stopped closing on the trainer "
				"(distances and held keys logged below)");
			return false;
		}

		if (g_iTSPhaseFrames > iTS_APPROACH_DEADLINE)
		{
			FailTS("walking into the trainer's cone never started a battle -- either "
				"the sight tick never ran or the raise never reached the subscriber");
			return false;
		}

		DriveTowardXZ(xPlayer.m_xPosition, g_xTSTrainerPosition);
		return true;
	}

	// W3 first entry: keep driving for several real frames, then rotate the trainer
	// through its exact back while still SPOTTED. Lost sight must cancel the beat.
	bool TSPhaseObserveFirstSpotted()
	{
		if (!TSSampleSpotted(true))
		{
			return false;
		}
		if (g_iTSFirstSpottedSamples < iTS_SPOTTED_CANCEL_SAMPLES)
		{
			return true;
		}

		g_fTSFirstSpottedMovement =
			PlanarDistance(g_xTSFirstSpottedStart, g_xTSFirstSpottedEnd);
		if (g_fTSFirstSpottedMovement < fTS_SPOTTED_MIN_MOVEMENT)
		{
			FailTS("the player did not keep moving during the first SPOTTED window");
			return false;
		}
		Zenith_TransformComponent* pxTransform = ResolveTrainerTransform();
		if (pxTransform == nullptr)
		{
			FailTS("the trainer transform vanished before SPOTTED cancellation");
			return false;
		}
		pxTransform->SetRotation(g_xTSTrainerAway);
		ClearTSInput();
		g_bTSSpottedCancellationIssued = true;
		g_eTSPhase = TSPhase::AwaitSpottedCancellation;
		g_iTSPhaseFrames = 0;
		return true;
	}

	bool TSPhaseAwaitSpottedCancellation()
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		TSPlayerView xPlayer;
		if (pxTrainer == nullptr || !FindActivePlayer(xPlayer))
		{
			FailTS("the trainer or player vanished during SPOTTED cancellation");
			return false;
		}
		// The rotation was issued at the END of the previous phase, so frame 0 of this
		// one is the first that could possibly observe it. Checking the submit
		// invariant on that frame would blame the INDICATOR for one tick of rotation
		// latency; the cone/state check below is what actually enforces the deadline.
		if (g_iTSPhaseFrames > 0
			&& pxTrainer->GetTrainerSpottedIndicatorSubmitCount()
				!= g_uTSSpottedIndicatorLast)
		{
			g_bTSSpottedPerFrameSubmissions = false;
			FailTS("the spotted indicator was submitted after sight cancellation");
			return false;
		}

		float fSeparation = 0.0f;
		const bool bInCone = PlayerIsInTrainerCone(fSeparation);
		if (!bInCone
			&& pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_WATCHING)
		{
			if (pxTrainer->GetTrainerSpottedCount() != 1u
				|| pxTrainer->GetTrainerSpottedElapsedSeconds() != 0.0f
				|| pxTrainer->GetTrainerSightRaiseCount() != 0u
				|| pxTrainer->GetTrainerChallengeCount() != 0u
				|| ZM_UI_MenuStack::IsMenuOpen()
				|| ZM_BattleTransition::IsTransitionActive()
				|| !xPlayer.m_pxController->IsMovementEnabled())
			{
				FailTS("SPOTTED cancellation left elapsed time or an early side effect behind");
				return false;
			}
			g_bTSSpottedCancellationObserved = true;
			g_uTSSpottedIndicatorAfterCancel =
				pxTrainer->GetTrainerSpottedIndicatorSubmitCount();

			Zenith_TransformComponent* pxTransform = ResolveTrainerTransform();
			if (pxTransform == nullptr)
			{
				FailTS("the trainer transform vanished before SPOTTED re-entry");
				return false;
			}
			pxTransform->SetRotation(g_xTSTrainerFacing);
			g_bTSSpottedFacingRestored = true;
			DriveTowardXZ(xPlayer.m_xPosition, g_xTSTrainerPosition);
			g_eTSPhase = TSPhase::AwaitSpottedReentry;
			g_iTSPhaseFrames = 0;
			return true;
		}
		if (g_iTSPhaseFrames > iTS_SPOTTED_CANCEL_DEADLINE)
		{
			FailTS("breaking the cone did not cancel SPOTTED back to WATCHING");
			return false;
		}
		return true;
	}

	bool TSPhaseAwaitSpottedReentry()
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		TSPlayerView xPlayer;
		if (pxTrainer == nullptr || !FindActivePlayer(xPlayer))
		{
			FailTS("the trainer or player vanished while re-entering SPOTTED");
			return false;
		}
		if (pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_SPOTTED)
		{
			g_bTSSecondSpottedObserved = true;
			g_xTSSecondSpottedStart = xPlayer.m_xPosition;
			g_xTSSecondSpottedEnd = xPlayer.m_xPosition;
			if (!TSSampleSpotted(false))
			{
				return false;
			}
			g_eTSPhase = TSPhase::ObserveSecondSpotted;
			g_iTSPhaseFrames = 0;
			return true;
		}
		if (pxTrainer->GetTrainerSightState() != ZM_TRAINER_SIGHT_WATCHING
			|| pxTrainer->GetTrainerSpottedIndicatorSubmitCount()
				!= g_uTSSpottedIndicatorAfterCancel
			|| ZM_UI_MenuStack::IsMenuOpen()
			|| ZM_BattleTransition::IsTransitionActive())
		{
			FailTS("SPOTTED re-entry produced an unexpected state or early side effect");
			return false;
		}
		if (g_iTSPhaseFrames > iTS_SPOTTED_REENTRY_DEADLINE)
		{
			FailTS("restoring the cone never re-entered SPOTTED");
			return false;
		}
		DriveTowardXZ(xPlayer.m_xPosition, g_xTSTrainerPosition);
		return true;
	}

	bool TSPhaseObserveSecondSpotted()
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the trainer vanished during the full SPOTTED window");
			return false;
		}
		if (pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_SPOTTED)
		{
			if (g_iTSPhaseFrames > iTS_SPOTTED_COMPLETE_DEADLINE)
			{
				FailTS("the full SPOTTED presentation window never completed");
				return false;
			}
			return TSSampleSpotted(false);
		}

		g_fTSSecondSpottedMovement =
			PlanarDistance(g_xTSSecondSpottedStart, g_xTSSecondSpottedEnd);
		// SPLIT DELIBERATELY. Folded into one condition, a future tuning or dt change
		// that merely shortened the beat below the sample bound would red with a
		// message accusing the CHALLENGING handoff -- naming a cause that is not the
		// one that fired. Each clause now names what it actually measured.
		if (g_iTSSecondSpottedSamples < iTS_SPOTTED_MIN_COMPLETE_SAMPLES)
		{
			FailTS("the completed SPOTTED window was observable for too few frames -- "
				"the marker duration and the fixed test dt no longer agree");
			return false;
		}
		if (g_fTSSecondSpottedMovement < fTS_SPOTTED_MIN_MOVEMENT)
		{
			FailTS("the player did not keep physically moving through the full SPOTTED "
				"window");
			return false;
		}
		// ★ S7 item 1 SC3, THE FAIL-OPEN AT ITS EXACT TICK. This is the handoff the
		// walk-up is inserted into: with a DYNAMIC CAPSULE the machine would be in
		// APPROACHING right now. This trainer has NO COLLIDER, so it must be in
		// CHALLENGING with no approach booked -- i.e. the pre-SC1 beat, unchanged.
		g_bTSHandoffSampled = true;
		g_uTSHandoffApproachCount = pxTrainer->GetTrainerApproachCount();
		g_bTSHandoffApproachPossible = pxTrainer->IsTrainerApproachPossible();
		if (pxTrainer->GetTrainerSightState() != ZM_TRAINER_SIGHT_CHALLENGING)
		{
			FailTS("the completed SPOTTED window did not hand off to CHALLENGING. A "
				"collider-less trainer must take the SHIPPED exit: with "
				"m_bApproachPossible false the FSM skips APPROACHING entirely, so "
				"landing anywhere else means the body contract is answering true for a "
				"component that owns no body at all");
			return false;
		}
		if (pxTrainer->GetTrainerSpottedCount() != 2u
			|| pxTrainer->GetTrainerSightRaiseCount() != 0u
			|| pxTrainer->GetTrainerChallengeCount() != 1u)
		{
			FailTS("the completed SPOTTED window left the wrong beat/bark/raise counts");
			return false;
		}
		if (pxTrainer->GetTrainerSpottedIndicatorSubmitCount()
			!= g_uTSSpottedIndicatorLast)
		{
			FailTS("the indicator was submitted on the tick that LEFT SPOTTED");
			return false;
		}

		g_fTSSpottedElapsedAfterComplete =
			pxTrainer->GetTrainerSpottedElapsedSeconds();
		if (!std::isfinite(g_fTSSpottedElapsedAfterComplete)
			|| g_fTSSpottedElapsedAfterComplete
				< ZM_TrainerSightFsmTuning{}.m_fSpottedSeconds)
		{
			FailTS("SPOTTED completed before its configured presentation duration");
			return false;
		}
		g_uTSSpottedIndicatorAfterComplete =
			pxTrainer->GetTrainerSpottedIndicatorSubmitCount();
		g_uTSSpottedCountAfterComplete = pxTrainer->GetTrainerSpottedCount();
		g_bTSSpottedCompleted = true;
		ClearTSInput();
		g_eTSPhase = TSPhase::Approach;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// (4b) THE BARK IS UP AND THE BATTLE IS NOT. Hold it, un-pressed, for
	//      iTS_CHALLENGE_HOLD_FRAMES: the challenge dialogue must PRECEDE the
	//      encounter, not ride under the fade. Nothing here presses anything -- the
	//      dismissal is phase (4c)'s job.
	bool TSPhaseAwaitChallenge()
	{
		const ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the runtime trainer was lost while its challenge bark was up");
			return false;
		}

		if (ZM_BattleTransition::IsTransitionActive()
			|| pxTrainer->GetTrainerSightRaiseCount() != 0u)
		{
			FailTS("the encounter was raised WHILE the challenge bark was still up -- "
				"the bark must PRECEDE the battle (order 112 closes the menu before "
				"order 113 dispatches), never ride under the fade");
			return false;
		}
		if (TSTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
		{
			FailTS("the challenge dialogue stopped being the top screen with no confirm "
				"press -- something else claimed the menu stack mid-bark");
			return false;
		}

		if (g_iTSPhaseFrames < iTS_CHALLENGE_HOLD_FRAMES)
		{
			return true;
		}
		g_bTSBarkHoldCompleted = true;
		g_eTSPhase = TSPhase::DismissChallenge;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// (4c) THE HANDOFF PROOF. Read the bark out with one confirm press per frame,
	//      then require the battle to start within iTS_BARK_TO_BATTLE_DEADLINE
	//      frames of the box closing: ZM_UI_MenuStack (order 112) pops, closes and
	//      UNFREEZES, and ZM_Interactable (order 113) dispatches the withheld
	//      encounter, in the SAME frame. Exactly one freeze owner at every instant,
	//      and SC7 introduced neither of them.
	bool TSPhaseDismissChallenge()
	{
		if (!g_bTSDismissClosed)
		{
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				g_bTSDismissClosed = true;
				g_iTSDismissFrames = g_iTSPhaseFrames;
				// Restart the counter: the deadline below is measured from the CLOSE,
				// not from the first press.
				g_iTSPhaseFrames = 0;
				return true;
			}
			if (g_iTSPhaseFrames > iTS_CHALLENGE_DISMISS_DEADLINE)
			{
				FailTS("the challenge bark never closed under one confirm press per "
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
			g_bTSBarkToBattleOK = true;
			g_iTSBarkToBattleFrames = g_iTSPhaseFrames;
			g_eTSPhase = TSPhase::AwaitInBattle;
			g_iTSPhaseFrames = 0;
			return true;
		}
		if (g_iTSPhaseFrames > iTS_BARK_TO_BATTLE_DEADLINE)
		{
			FailTS("the battle did NOT start within the deadline after the challenge "
				"bark closed -- the withheld encounter was never dispatched, so the "
				"trainer talked at the player and then let him walk away");
			return false;
		}
		return true;
	}

	// (5) THE CHANNEL DISCRIMINATOR. Latch which trainer the accepted round trip
	//     carries the first frame the transition leaves IDLE.
	bool TSPhaseAwaitInBattle()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTS("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE");
			return false;
		}

		if (!g_bTSChannelCaptured
			&& pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
		{
			g_eTSChannelTrainer = pxTransition->GetBattleTrainer();
			g_bTSChannelCaptured = true;
		}

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
		{
			g_bTSReachedInBattle = true;
			g_eTSPhase = TSPhase::DriveMenu;
			g_iTSPhaseFrames = 0;
			return true;
		}

		if (g_iTSPhaseFrames > iTS_INBATTLE_DEADLINE)
		{
			FailTS("the trainer encounter never reached IN_BATTLE before the deadline");
			return false;
		}
		return true;
	}

	// (6) Drive the battle out. The default menu is ACTION_ROOT with cursor 0 =
	//     Fight, so an ENTER every frame picks Fight->move0 each turn.
	bool TSPhaseDriveMenu()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTS("the ZM_BattleTransition singleton stopped resolving during the battle");
			return false;
		}

		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
			&& pxTransition->GetCompletedBattleCount() == 1u)
		{
			g_eTSPhase = TSPhase::Settle;
			g_iTSPhaseFrames = 0;
			return true;
		}

		if (g_iTSPhaseFrames > iTS_DRIVE_DEADLINE)
		{
			FailTS("the player-driven trainer battle never ended (never returned to "
				"IDLE with completed == 1)");
			return false;
		}
		return true;
	}

	// (7a) Settle, then sample the payout off a FRESHLY re-resolved GameState.
	bool TSPhaseSettle()
	{
		if (g_iTSPhaseFrames < iTS_SETTLE_FRAMES)
		{
			return true;
		}

		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTS("the ZM_BattleTransition singleton stopped resolving after the resume");
			return false;
		}
		g_uTSCompletedAfter = pxTransition->GetCompletedBattleCount();
		g_uTSAbortedAfter = pxTransition->GetAbortedTransitionCount();
		g_uTSStateAfter = (u_int)pxTransition->GetTransitionState();
		g_uTSEncountersAtSettle = pxTransition->GetObservedEncounterCount();
		g_bTSBattleUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();

		ZM_GameState* pxGameState = nullptr;
		if (ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr)
		{
			g_uTSMoneyAfter = pxGameState->m_uMoney;
			g_bTSFlagAfter = ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
			g_bTSWhiteoutAfter = pxGameState->m_bPendingWhiteout;
			g_bTSAfterCaptured = true;
		}

		// ★ SAMPLE THE PRODUCTION SESSION LATCH, here and nowhere else. The encounter
		// above went through ZM_Interactable::TickTrainerSight, whose
		// `ZM_TrainerEngagementLatch::MarkEngaged(m_eTrainerId);` runs BEFORE the
		// dispatch. Setup cleared the mask and phase (2) proved it clear, so a true here
		// can only have come from that line -- delete it and this sample goes false.
		// It MUST be taken before phase (7a2) clears the latch again.
		g_bTSLatchAfter = ZM_TrainerEngagementLatch::HasEngaged(eTS_TRAINER);

		g_uTSHoldMaxRaiseCount = 0u;
		g_uTSHoldMinRaiseCount = 0xffffffffu;
		g_eTSPhase = TSPhase::CinematicFreeze;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// Raise the cinematic phase's own one-line box. The line is SHORT on purpose: the
	// overworld typewriter is a hard constexpr 45 chars/sec (ZM_UI_BattleHUD.cpp:37)
	// that ZM_SetInstantBattlesForTests does NOTHING for, so every character it does
	// not have is real frames this phase does not spend.
	bool TSPushCinematicLine()
	{
		const char* const aszLines[] = { "..." };
		return ZM_UI_MenuStack::TryPushDialogue(aszLines, 1u);
	}

	// (7a1) ★ THE CINEMATIC FREEZE LATCH, LIVE -- A NEGATIVE PAIRED WITH A POSITIVE,
	//       IN THE SAME RUN (S7 item 1 SC2).
	//
	// THE CLAIM: ZM_UI_MenuStack::UnfreezePlayer now coordinates with a FOURTH freeze
	// owner. With ZM_TrainerCinematicLatch armed, a dialogue closing underneath a
	// cinematic must NOT hand movement back; with it released, the very same close must.
	//
	// ★ THE NEGATIVE ON ITS OWN WOULD BE WORTHLESS, and that is not a hypothetical here.
	// "The player is still frozen after the box closed" is satisfied for free by a build
	// in which nothing ever unfreezes anybody -- which is EXACTLY the failure this latch
	// is most likely to introduce, because m_bMovementEnabled is a bare bool with no
	// refcount and four owners now write it. So the phase proves both halves against the
	// SAME player in the SAME run:
	//   (a) PRECONDITION -- the player is UNFROZEN when the phase begins;
	//   (b) ARMED -- open a box, close it, and movement stays OFF for a WINDOW of
	//       frames, not merely for the frame after the close;
	//   (c) RELEASED -- End(), then open the SAME box and close it the SAME way, and
	//       movement must come BACK.
	// (c) is what gives (b) meaning, and it is the direct test of the release path:
	// make End() a no-op and (c) is the clause that reds.
	//
	// WHY HERE. This is the one point in the walk where Dawnmere is the active scene,
	// the transition is IDLE, the Battle scene is unloaded and the player is a live
	// unfrozen controller. The phase leaves the sight machine exactly as it found it:
	// Vesper's defeat flag is set by now, so ZM_MayTrainerEngage answers NO for its whole
	// duration and nothing here can raise, bark, or move a counter that phases (7a2) and
	// (7) go on to read.
	bool TSPhaseCinematicFreeze()
	{
		TSPlayerView xPlayer;
		if (!FindActivePlayer(xPlayer) || xPlayer.m_pxController == nullptr)
		{
			FailTS("the player vanished before the cinematic freeze phase");
			return false;
		}

		// ---- (a) THE PRECONDITION ------------------------------------------------
		if (!g_bTSCineStarted)
		{
			// ★ THE LATCH IS READ BEFORE IT IS TOUCHED, and Setup deliberately does NOT
			// clear it, so this sample doubles as the live tripwire on the between-tests
			// hook in Zenithmon.cpp. Nothing in the repo arms this latch outside this
			// phase today, so it cannot red yet; the moment a runtime caller exists, a
			// test that dies mid-cinematic leaks a FREEZE and this is what sees it.
			g_bTSCineLatchWasClear = !ZM_TrainerCinematicLatch::IsActive();
			if (!g_bTSCineLatchWasClear)
			{
				FailTS("a cinematic freeze was ALREADY armed when this phase began -- "
					"something leaked one across the batch and the between-tests hook in "
					"Zenithmon.cpp did not clear it, so every later test inherits a player "
					"who can never be unfrozen");
				return false;
			}
			if (!xPlayer.m_pxController->IsMovementEnabled())
			{
				// Not a failure yet: the battle resume restores the player and this phase
				// starts only a handful of frames behind it.
				if (g_iTSPhaseFrames > iTS_CINE_UNFROZEN_DEADLINE)
				{
					FailTS("the player was STILL frozen when the cinematic phase began, so "
						"the 'still frozen' half below would be satisfied by a build in "
						"which nothing ever unfreezes anybody -- precisely the vacuous "
						"negative this phase exists to refuse");
					return false;
				}
				return true;
			}
			g_bTSCineUnfrozenAtEntry = true;

			ZM_TrainerCinematicLatch::Begin(eTS_TRAINER);
			if (!ZM_TrainerCinematicLatch::IsActive()
				|| ZM_TrainerCinematicLatch::GetActiveTrainerForTests() != eTS_TRAINER)
			{
				FailTS("ZM_TrainerCinematicLatch::Begin did not arm the freeze for the "
					"trainer it was handed -- every clause below would be vacuous");
				return false;
			}
			g_bTSCineArmed = true;

			if (!TSPushCinematicLine())
			{
				FailTS("the cinematic phase could not raise a dialogue to close");
				return false;
			}
			g_bTSCineStarted = true;
			g_iTSPhaseFrames = 0;
			return true;
		}

		// ---- (b) THE NEGATIVE: armed, the close must NOT release -----------------
		if (!g_bTSCineEnded)
		{
			if (!g_bTSCineBoxRaised)
			{
				// PushDialogueLines is synchronous, so the box is top on this very frame.
				if (TSTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
				{
					FailTS("the cinematic phase's dialogue never became the top screen");
					return false;
				}
				// ★ ANTI-VACUITY ON THE FREEZE ITSELF. PushDialogueLines calls
				// FreezePlayer only when the stack was EMPTY; a player who was not frozen
				// here would make "still frozen after the close" a claim about a freeze
				// that never happened.
				if (xPlayer.m_pxController->IsMovementEnabled())
				{
					FailTS("raising the cinematic phase's dialogue did NOT freeze the "
						"player, so 'still frozen once it closes' would prove nothing");
					return false;
				}
				g_bTSCineBoxRaised   = true;
				g_bTSCineFrozenByBox = true;
				g_iTSPhaseFrames = 0;
				return true;
			}

			if (!g_bTSCineArmedBoxClosed)
			{
				if (!ZM_UI_MenuStack::IsMenuOpen())
				{
					g_bTSCineArmedBoxClosed = true;
					g_iTSPhaseFrames = 0;
					return true;
				}
				if (g_iTSPhaseFrames > iTS_CINE_DISMISS_DEADLINE)
				{
					FailTS("the cinematic phase's armed-half dialogue never closed under "
						"one confirm press per frame");
					return false;
				}
				// State-setter only (convention C1), the same press DismissChallenge uses.
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}

			// THE PROPERTY, on every frame of a real window rather than on one frame.
			++g_iTSCineHeldFrames;
			if (xPlayer.m_pxController->IsMovementEnabled())
			{
				FailTS("the dialogue closed and handed movement BACK while a cinematic "
					"freeze was armed -- ZM_UI_MenuStack::UnfreezePlayer is not "
					"coordinating with the fourth owner, so the player walks out from "
					"under the walk-up mid-cinematic");
				return false;
			}
			if (g_iTSCineHeldFrames < iTS_CINE_HOLD_FRAMES)
			{
				return true;
			}
			g_bTSCineHeldWhileArmed = true;

			// ---- (c) THE PAIRED POSITIVE: release, and movement must come back ----
			ZM_TrainerCinematicLatch::End();
			if (ZM_TrainerCinematicLatch::IsActive())
			{
				FailTS("ZM_TrainerCinematicLatch::End did not release the freeze");
				return false;
			}
			g_bTSCineEnded = true;
			if (!TSPushCinematicLine())
			{
				FailTS("the cinematic phase could not raise its second dialogue");
				return false;
			}
			g_iTSPhaseFrames = 0;
			return true;
		}

		if (!g_bTSCineBoxRaised2)
		{
			if (TSTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailTS("the cinematic phase's second dialogue never became the top screen "
					"-- the released half must exercise the SAME close path as the armed "
					"half or it compares nothing");
				return false;
			}
			g_bTSCineBoxRaised2 = true;
			g_iTSPhaseFrames = 0;
			return true;
		}

		if (!g_bTSCineFreeBoxClosed)
		{
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				g_bTSCineFreeBoxClosed = true;
				g_iTSPhaseFrames = 0;
				return true;
			}
			if (g_iTSPhaseFrames > iTS_CINE_DISMISS_DEADLINE)
			{
				FailTS("the cinematic phase's released-half dialogue never closed under "
					"one confirm press per frame");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			return true;
		}

		if (xPlayer.m_pxController->IsMovementEnabled())
		{
			g_bTSCineReleased      = true;
			g_iTSCineReleaseFrames = g_iTSPhaseFrames;
			g_bTSCineCompleted     = true;
			ClearTSInput();
			g_eTSPhase = TSPhase::BreakSight;
			g_iTSPhaseFrames = 0;
			return true;
		}
		if (g_iTSPhaseFrames > iTS_CINE_RELEASE_DEADLINE)
		{
			FailTS("with the cinematic freeze ENDED, closing the dialogue did NOT give "
				"movement back -- the release path is broken and the player is stranded "
				"permanently frozen. This is the PAIRED POSITIVE: without it the 'still "
				"frozen' half above is satisfied by a game that never unfreezes anybody");
			return false;
		}
		return true;
	}

	// (7a2) BREAK SIGHT, THEN RE-ENTER THE CONE. Without this the hold below proves
	// LESS than it claims. Coming out of the battle the FSM sits ENGAGED with
	// m_bRaiseConfirmed LATCHED (ZM_TrainerSightFsm.cpp, the ENGAGED arm), and that
	// latch ALONE pins the raise count at 1 for the whole hold -- ZM_MayTrainerEngage
	// is never even reached, so "the defeated rival never re-spots you" would be an
	// unsupported claim about a gate nothing exercised.
	//
	// Turning the trainer AWAY drives the FSM's `if (!bSees)` re-arm back to WATCHING
	// (the same edge a player stepping behind cover produces); restoring the exact
	// placement facing then puts the player back inside the cone with a COLD machine,
	// so every frame of the hold runs the WATCHING arm and ZM_MayTrainerEngage is the
	// only thing standing between it and a second raise.
	//
	// THE SESSION LATCH IS CLEARED HERE ON PURPOSE. ZM_MayTrainerEngage splits into
	// two independent arms once its two closed-for-everyone clauses (unregistered
	// row, and -- since S8 item 1 SC3 -- a player with nothing to send out) have
	// passed; Vesper's row is FLAGGED, but the production MarkEngaged has
	// also set his session latch, so with both set, deleting the defeat-flag arm would
	// leave the latch arm answering "no" and the hold would stay green. Clearing the
	// SESSION-scoped latch while the PERSISTENT story flag stays set is exactly the
	// state a save + reload produces, and it makes the defeat flag the SOLE remaining
	// guard. The latch's own value was captured in phase (7a), above.
	bool TSPhaseBreakSight()
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		Zenith_TransformComponent* pxTransform = ResolveTrainerTransform();
		if (pxTrainer == nullptr || pxTransform == nullptr)
		{
			FailTS("the runtime trainer was lost while breaking its line of sight");
			return false;
		}

		if (!g_bTSRearmStarted)
		{
			g_uTSRaiseAtRearm = pxTrainer->GetTrainerSightRaiseCount();
			// LOGGED, NOT ASSERTED. Coming out of the battle this is normally ENGAGED
			// (confirmed), but a frame in which the player was briefly outside the cone
			// during the round trip would legitimately have re-armed it already. Either
			// way what the hold needs is the state on EXIT from this phase, which is
			// what the completion test below pins; demanding ENGAGED here would be a
			// flake with no extra proof behind it.
			g_uTSStateAtBreakEntry = (u_int)pxTrainer->GetTrainerSightState();
			pxTransform->SetRotation(g_xTSTrainerAway);
			ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
			g_bTSRearmStarted = true;
			return true;
		}

		float fSeparation = 0.0f;
		if (!PlayerIsInTrainerCone(fSeparation))
		{
			g_bTSRearmConeBroken = true;
		}
		g_fTSRearmSeparation = fSeparation;

		// BOTH observations are required: the geometry really stopped seeing the player
		// (so the re-arm had a real cause), AND the machine really came back to
		// WATCHING (so the hold really starts cold).
		//
		// Whenever the entry state WAS ENGAGED (the normal case -- stateAtBreakEntry is
		// logged) this additionally rules out a deferred raise: that arm tests
		// m_bChannelBusy BEFORE `!bSees`, so reaching WATCHING at all means the
		// encounter channel was observed IDLE on the frame it re-armed.
		if (g_bTSRearmConeBroken
			&& pxTrainer->GetTrainerSightState() == ZM_TRAINER_SIGHT_WATCHING)
		{
			g_bTSRearmBroke = true;
			pxTransform->SetRotation(g_xTSTrainerFacing);
			g_bTSRearmRestored = true;
			g_eTSPhase = TSPhase::HoldInCone;
			g_iTSPhaseFrames = 0;
			return true;
		}

		if (g_iTSPhaseFrames > iTS_REARM_DEADLINE)
		{
			FailTS("turning the trainer away never re-armed its sight machine (state is "
				"still ENGAGED, or the player is somehow still inside the reversed cone) "
				"-- the hold below could not then tell the defeat-flag gate apart from "
				"the FSM's own confirmed-raise latch");
			return false;
		}
		return true;
	}

	// (7b) HOLD IN THE CONE, with the machine freshly RE-ARMED by phase (7a2) and the
	//      session latch cleared. The player is inside the cone and the FSM is
	//      WATCHING, so the raise arm is re-entered every single frame and only
	//      ZM_MayTrainerEngage's defeat-flag test keeps it silent. Standing there must
	//      NOT re-raise: that is 'spotted once, not once per frame' AND the
	//      defeat-flag gate, each now with a mechanism of its own to red it.
	bool TSPhaseHoldInCone()
	{
		const ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the runtime trainer was lost while holding in its cone");
			return false;
		}
		g_bTSHoldEverEngaged = g_bTSHoldEverEngaged
			|| pxTrainer->GetTrainerSightState() != ZM_TRAINER_SIGHT_WATCHING;
		const u_int uRaiseCount = pxTrainer->GetTrainerSightRaiseCount();
		g_uTSHoldMaxRaiseCount = (uRaiseCount > g_uTSHoldMaxRaiseCount)
			? uRaiseCount : g_uTSHoldMaxRaiseCount;
		g_uTSHoldMinRaiseCount = (uRaiseCount < g_uTSHoldMinRaiseCount)
			? uRaiseCount : g_uTSHoldMinRaiseCount;

		float fSeparation = 0.0f;
		if (PlayerIsInTrainerCone(fSeparation))
		{
			g_bTSHoldSawCone = true;
		}
		g_fTSHoldSeparation = fSeparation;

		g_bTSHoldTransitionMoved =
			g_bTSHoldTransitionMoved || ZM_BattleTransition::IsTransitionActive();
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition != nullptr)
		{
			g_bTSHoldEncountersMoved = g_bTSHoldEncountersMoved
				|| pxTransition->GetObservedEncounterCount() != g_uTSEncountersAtSettle;
		}

		if (g_iTSPhaseFrames < iTS_HOLD_FRAMES)
		{
			return true;
		}
		g_bTSHoldCompleted = true;
		g_eTSPhase = TSPhase::FlaglessArm;
		g_iTSPhaseFrames = 0;
		g_uTSFlaglessMaxRaise = 0u;
		return true;
	}

	// (8) THE FLAGLESS ARM. Reconfigure the SAME entity onto a row that writes NO
	//     defeat flag and mark its session latch by hand: ZM_MayTrainerEngage's
	//     second arm must keep it silent even though the geometry still sees the
	//     player. Without this, a flagless trainer's prize would be farmable.
	bool TSPhaseFlaglessArm()
	{
		ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the runtime trainer was lost before the flagless arm");
			return false;
		}

		if (!g_bTSFlaglessConfigured)
		{
			if (!pxTrainer->ConfigureTrainerSight(eTS_FLAGLESS_TRAINER))
			{
				FailTS("ConfigureTrainerSight refused the authored flagless row");
				return false;
			}
			// ANTI-VACUITY: the reconfigure really took, and it reset the machine to
			// a COLD watcher -- so a later zero raise count is a genuine silence and
			// not a leftover.
			if (!pxTrainer->IsTrainerSightEnabled()
				|| pxTrainer->GetTrainerId() != eTS_FLAGLESS_TRAINER
				|| pxTrainer->GetTrainerSightRaiseCount() != 0u)
			{
				FailTS("the flagless reconfigure did not take (id / enable / raise "
					"count disagree)");
				return false;
			}
			ZM_TrainerEngagementLatch::MarkEngaged(eTS_FLAGLESS_TRAINER);
			if (!ZM_TrainerEngagementLatch::HasEngaged(eTS_FLAGLESS_TRAINER))
			{
				FailTS("the session latch did not record the flagless trainer -- the "
					"hold below would prove nothing");
				return false;
			}
			g_bTSFlaglessConfigured = true;
		}

		const u_int uRaiseCount = pxTrainer->GetTrainerSightRaiseCount();
		g_uTSFlaglessMaxRaise = (uRaiseCount > g_uTSFlaglessMaxRaise)
			? uRaiseCount : g_uTSFlaglessMaxRaise;

		float fSeparation = 0.0f;
		if (PlayerIsInTrainerCone(fSeparation))
		{
			g_bTSFlaglessSawCone = true;
		}
		g_fTSFlaglessSeparation = fSeparation;

		g_bTSFlaglessTransitionMoved =
			g_bTSFlaglessTransitionMoved || ZM_BattleTransition::IsTransitionActive();

		if (g_iTSPhaseFrames < iTS_HOLD2_FRAMES)
		{
			return true;
		}
		g_bTSFlaglessCompleted = true;
		g_eTSPhase = TSPhase::RamblerNoBark;
		g_iTSPhaseFrames = 0;
		return true;
	}

	// (9) THE SILENT ARM, END TO END. The same entity is still on the flagless row,
	//     which ships ZERO challenge lines. Clear the two gates phase (8) deliberately
	//     closed and the trainer must battle again -- with NO bark: no dialogue is
	//     ever raised and GetTrainerChallengeCount stays exactly 0. That is what makes
	//     the availability test two-sided: invert it and Vesper's bark disappears
	//     while the rambler grows one.
	//
	// ★ THE RAISE AND THE TRANSITION ARE LATCHES, NOT AN EXIT CONDITION. This phase
	// runs the FULL iTS_RAMBLER_DEADLINE window and asserts the no-DIALOGUE property on
	// every frame of it (iTS_RAMBLER_SAMPLED_FRAMES frames -- the first is spent
	// clearing the two gates phase (8) closed). Exiting the moment both positives were
	// observed, which is what the raise -> dispatch -> accept chain does in about three
	// frames, would have sampled the ONE property this phase exists for on three frames
	// and called it a 200-frame hold. Verify checks g_iTSRamblerSampledFrames against
	// the window, so that shrinkage cannot come back silently.
	//
	// It ends DEEP IN THE ROUND TRIP on purpose, and that is a widening of SC7's
	// accepted risk rather than a new one: at 1/30 dt the 0.20 s fade-out is ~6 frames
	// and the additive Battle load + arena build a handful more, so the window outlives
	// them and the phase now typically ends in IN_BATTLE rather than mid-fade. Three
	// things make that safe. (1) IN_BATTLE has NO timer -- ZM_BattleTransition::OnUpdate
	// only leaves it on RequestBattleEnd, which nothing here calls, so the state is
	// stationary rather than racing us. (2) The teardown in Verify already force-resets
	// the transition (restoring the parked player), force-unloads any lingering Battle
	// scene and SINGLE-loads FrontEnd on EVERY exit path -- it is the same teardown that
	// runs today whenever DriveMenu deadlines out mid-battle, so ending there is an
	// already-designed-for state, not an unhandled one. (3) Nothing in the battle can
	// push a DIALOGUE screen: every TryPushDialogue / PushDialogueLines call site is in
	// ZM_Interactable (the overworld, which EnterBattleOnce PAUSES) or the graph node,
	// so the property stays cleanly observable for the whole window.
	//
	// HONEST ABOUT WHAT THE TAIL BUYS: once EnterBattleOnce pauses the overworld,
	// ZM_Interactable stops ticking and the sight machine cannot raise anything at all,
	// so those frames prove no NEW bark is possible rather than exercising the beat.
	// They are kept because they cost nothing and they cover the fade -- where a bark
	// that "rode under" the encounter instead of preceding it would surface.
	bool TSPhaseRamblerNoBark()
	{
		// Resolves for the whole window even once Battle takes focus: the trainer lives
		// in Dawnmere, which the battle loads ADDITIVELY (paused, never unloaded), and
		// ResolveEntity is keyed on the entity's own owning scene.
		const ZM_Interactable* pxTrainer = ResolveTrainerComponent();
		if (pxTrainer == nullptr)
		{
			FailTS("the runtime trainer was lost during the silent-arm hold");
			return false;
		}

		if (!g_bTSRamblerStarted)
		{
			// BOTH gates, because phase (8) closed both: the session latch it marked by
			// hand, and any transition state the earlier round trip left behind.
			ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
			ZM_BattleTransition::ResetRuntimeStateForTests();
			if (ZM_TrainerEngagementLatch::HasEngaged(eTS_FLAGLESS_TRAINER))
			{
				FailTS("the flagless trainer's session latch survived its reset -- the "
					"silent arm below could never raise and would prove nothing");
				return false;
			}
			g_uTSRamblerRaiseAtStart = pxTrainer->GetTrainerSightRaiseCount();
			g_bTSRamblerStarted = true;
			return true;
		}

		++g_iTSRamblerSampledFrames;

		// ANTI-VACUITY, the SC3 predicate directly: the geometry really is watching.
		float fSeparation = 0.0f;
		if (PlayerIsInTrainerCone(fSeparation))
		{
			g_bTSRamblerSawCone = true;
		}
		// KEEP THE LAST RESOLVED separation for the diagnostic. The poll reports 0 once
		// Battle takes focus (FindActivePlayer is active-scene bound), and overwriting a
		// real measurement with that would make the failure text lie about geometry the
		// early frames did observe.
		if (fSeparation > 0.0f)
		{
			g_fTSRamblerSeparation = fSeparation;
		}

		// ★ THE PROPERTY, ON EVERY FRAME OF THE WINDOW -- this is what the phase is for.
		// A bark that appeared and was gone again before the end must still red it.
		if (TSTopScreen() == ZM_MENU_SCREEN_DIALOGUE)
		{
			g_bTSRamblerSawDialogue = true;
		}

		const u_int uRaiseCount = pxTrainer->GetTrainerSightRaiseCount();
		g_uTSRamblerMaxRaise = (uRaiseCount > g_uTSRamblerMaxRaise)
			? uRaiseCount : g_uTSRamblerMaxRaise;
		const u_int uChallengeCount = pxTrainer->GetTrainerChallengeCount();
		g_uTSRamblerMaxChallenge = (uChallengeCount > g_uTSRamblerMaxChallenge)
			? uChallengeCount : g_uTSRamblerMaxChallenge;
		g_bTSRamblerTransitionMoved =
			g_bTSRamblerTransitionMoved || ZM_BattleTransition::IsTransitionActive();

		// The two positives above are LATCHED, never exited on. Keep holding.
		if (g_iTSPhaseFrames < iTS_RAMBLER_DEADLINE)
		{
			return true;
		}

		// The window elapsed. Both positives had to land somewhere inside it -- this is
		// the same deadline failure as before, moved to the end of the hold.
		if (g_uTSRamblerMaxRaise <= g_uTSRamblerRaiseAtStart
			|| !g_bTSRamblerTransitionMoved)
		{
			FailTS("the un-latched SILENT trainer never started a battle while the "
				"player stood in his cone -- after the shared SPOTTED marker, a row "
				"with no challenge lines must skip CHALLENGING and raise");
			return false;
		}

		g_bTSRamblerCompleted = true;
		g_eTSPhase = TSPhase::Done;
		return false;
	}

	// -------------------------------------------------------------------------
	// Harness entry points
	// -------------------------------------------------------------------------

	void Setup_ZMTrainerSight()
	{
		g_eTSPhase          = TSPhase::Done;
		g_iTSPhaseFrames    = 0;
		g_bTSActive         = false;
		g_bTSFailed         = false;
		g_bTSPrereqsPresent = false;
		g_szTSFailure       = "test did not reach verification";

		g_xTSTrainerEntityID = INVALID_ENTITY_ID;
		g_xTSTrainerPosition = Zenith_Maths::Vector3(0.0f);
		g_bTSTrainerPlaced   = false;
		g_bTSTrainerArmed    = false;
		g_fTSSpawnSeparation = 0.0f;
		g_xTSTrainerFacing   = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		g_xTSTrainerAway     = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

		g_bTSBaselineCaptured = false;
		g_uTSMoneyBefore      = 0u;
		g_bTSFlagBefore       = true;
		g_bTSLatchBefore      = true;
		g_bTSLeadInstalled    = false;

		g_xTSBasisStart      = Zenith_Maths::Vector3(0.0f);
		g_fTSBasisDeltaX     = 0.0f;
		g_fTSBasisDeltaZ     = 0.0f;
		g_bTSBasisPassed     = false;
		g_fTSBestDistance    = 0.0f;
		g_fTSCurrentDistance = 0.0f;
		g_iTSStallFrames     = 0;
		g_abTSHeldKeys[0] = false;
		g_abTSHeldKeys[1] = false;
		g_abTSHeldKeys[2] = false;
		g_abTSHeldKeys[3] = false;

		g_bTSFirstSpottedObserved        = false;
		g_bTSSpottedCancellationIssued   = false;
		g_bTSSpottedCancellationObserved = false;
		g_bTSSpottedFacingRestored       = false;
		g_bTSSecondSpottedObserved       = false;
		g_bTSSpottedCompleted            = false;
		g_bTSSpottedNeverFrozen          = true;
		g_bTSSpottedPerFrameSubmissions  = true;
		g_bTSFirstSpottedSawCone         = false;
		g_bTSSecondSpottedSawCone        = false;
		g_iTSFirstSpottedSamples         = 0;
		g_iTSSecondSpottedSamples        = 0;
		g_uTSSpottedIndicatorBefore      = 0xffffffffu;
		g_uTSSpottedIndicatorLast        = 0xffffffffu;
		g_uTSSpottedIndicatorAfterCancel = 0xffffffffu;
		g_uTSSpottedIndicatorAfterComplete = 0xffffffffu;
		g_uTSSpottedCountAfterComplete   = 0xffffffffu;
		g_fTSSpottedElapsedAfterComplete = 0.0f;
		g_fTSFirstSpottedMovement        = 0.0f;
		g_fTSSecondSpottedMovement       = 0.0f;
		g_xTSFirstSpottedStart           = Zenith_Maths::Vector3(0.0f);
		g_xTSFirstSpottedEnd             = Zenith_Maths::Vector3(0.0f);
		g_xTSSecondSpottedStart          = Zenith_Maths::Vector3(0.0f);
		g_xTSSecondSpottedEnd            = Zenith_Maths::Vector3(0.0f);

		g_bTSChannelCaptured = false;
		g_eTSChannelTrainer  = ZM_TRAINER_NONE;
		g_bTSReachedInBattle = false;

		g_bTSAfterCaptured  = false;
		g_uTSMoneyAfter     = 0xffffffffu;
		g_bTSFlagAfter      = false;
		g_bTSWhiteoutAfter  = true;
		g_uTSCompletedAfter = 0xffffffffu;
		g_uTSAbortedAfter   = 0xffffffffu;
		g_uTSStateAfter     = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;
		g_bTSBattleUnloaded = false;
		g_uTSEncountersAtSettle = 0xffffffffu;
		g_bTSLatchAfter     = false;

		g_bTSRearmStarted    = false;
		g_bTSRearmConeBroken = false;
		g_bTSRearmBroke      = false;
		g_bTSRearmRestored   = false;
		g_uTSRaiseAtRearm    = 0xffffffffu;
		g_uTSStateAtBreakEntry = 0xffffffffu;
		g_fTSRearmSeparation = 0.0f;

		// S7 item 1 SC2. ★ NOTE WHAT IS *NOT* HERE: the cinematic latch itself is
		// deliberately NOT reset by Setup, unlike ZM_TrainerEngagementLatch below.
		// Clearing it here would make the phase's own "was a freeze leaked into this
		// test" check dead by construction, and that check is the only live tripwire on
		// the between-tests hook in Zenithmon.cpp. Verify's teardown resets it on EVERY
		// exit path, so nothing this test arms can outlive it.
		g_bTSCineStarted         = false;
		g_bTSCineLatchWasClear   = false;
		g_bTSCineUnfrozenAtEntry = false;
		g_bTSCineArmed           = false;
		g_bTSCineBoxRaised       = false;
		g_bTSCineFrozenByBox     = false;
		g_bTSCineArmedBoxClosed  = false;
		g_iTSCineHeldFrames      = 0;
		g_bTSCineHeldWhileArmed  = false;
		g_bTSCineEnded           = false;
		g_bTSCineBoxRaised2      = false;
		g_bTSCineFreeBoxClosed   = false;
		g_bTSCineReleased        = false;
		g_iTSCineReleaseFrames   = -1;
		g_bTSCineCompleted       = false;

		g_bTSHoldCompleted       = false;
		g_bTSHoldSawCone         = false;
		g_uTSHoldMaxRaiseCount   = 0xffffffffu;
		g_uTSHoldMinRaiseCount   = 0xffffffffu;
		g_bTSHoldEverEngaged     = false;
		g_bTSHoldTransitionMoved = false;
		g_bTSHoldEncountersMoved = false;
		g_fTSHoldSeparation      = 0.0f;

		g_bTSFlaglessConfigured      = false;
		g_bTSFlaglessCompleted       = false;
		g_bTSFlaglessSawCone         = false;
		g_uTSFlaglessMaxRaise        = 0xffffffffu;
		g_bTSFlaglessTransitionMoved = false;
		g_fTSFlaglessSeparation      = 0.0f;

		g_uTSEncountersBeforeWalk  = uTS_ENCOUNTERS_UNRESOLVED_BEFORE;
		g_bTSEncountersBeforeWalkResolved = false;
		g_bTSBarkMissed            = false;
		g_bTSBarkObserved          = false;
		g_bTSBarkTopWasDialogue    = false;
		g_bTSBarkStateChallenging  = false;
		g_bTSBarkTransitionIdle    = false;
		g_uTSBarkRaiseCount        = 0xffffffffu;
		g_uTSBarkChallengeCount    = 0xffffffffu;
		g_uTSEncountersAtBark      = uTS_ENCOUNTERS_UNRESOLVED_AT_BARK;
		g_bTSEncountersAtBarkResolved = false;
		g_bTSBarkHoldCompleted     = false;
		g_bTSDismissClosed         = false;
		g_iTSDismissFrames         = -1;
		g_bTSBarkToBattleOK        = false;
		g_iTSBarkToBattleFrames    = -1;

		g_bTSRamblerStarted        = false;
		g_bTSRamblerCompleted      = false;
		g_bTSRamblerSawCone        = false;
		g_bTSRamblerSawDialogue    = false;
		g_iTSRamblerSampledFrames  = 0;
		g_uTSRamblerRaiseAtStart   = 0xffffffffu;
		g_uTSRamblerMaxRaise       = 0u;
		g_uTSRamblerMaxChallenge   = 0u;
		g_bTSRamblerTransitionMoved = false;
		g_fTSRamblerSeparation     = 0.0f;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO
		// process state (fixed dt, instant-battles flag, scene load) until EVERY
		// git-ignored input is confirmed present.
		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bTSPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath)
			&& bWarm;
		if (!g_bTSPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		// Clear the transition's ownerless channel latches AND SC6's ownerless
		// session latch, so an earlier batched test cannot bleed either in. Neither
		// touches any subscription.
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();

		// S7 item 1 SC3's fail-open observations. Every one is a NEGATIVE that must
		// hold on every frame, so they reset to the PASSING value and only the run
		// itself can spoil them.
		g_bTSApproachEverPossible    = false;
		g_uTSApproachCountMax        = 0u;
		g_bTSApproachHoldEverSeen    = false;
		g_bTSApproachLatchEverSeen   = false;
		g_bTSApproachStateEverSeen   = false;
		g_uTSHandoffApproachCount    = 0xffffffffu;
		g_bTSHandoffApproachPossible = true;
		g_bTSHandoffSampled          = false;

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fTS_FIXED_DT);

		// Collapse every presentation op to zero duration so a turn drains in a
		// single Tick; the AWAIT_INPUT gate still needs one real frame per press.
		ZM_SetInstantBattlesForTests(true);

		g_xEngine.Scenes().LoadSceneByIndex(
			iTS_OVERWORLD_BUILD_INDEX, SCENE_LOAD_SINGLE);

		g_eTSPhase = TSPhase::AwaitReady;
		g_bTSActive = true;
	}

	bool Step_ZMTrainerSight(int)
	{
		if (!g_bTSActive || g_bTSFailed || g_eTSPhase == TSPhase::Done)
		{
			return false;
		}

		++g_iTSPhaseFrames;
		// S7 item 1 SC3. The fail-open is a per-FRAME negative, so it is sampled ahead
		// of the switch across every phase rather than inside any one of them. It adds
		// NO frames -- the shipped budget below is unchanged, which is itself part of
		// what this sub-commit claims about a trainer who cannot walk.
		TSSampleApproachFailOpen();
		switch (g_eTSPhase)
		{
		case TSPhase::AwaitReady:    return TSPhaseAwaitReady();
		case TSPhase::PlaceTrainer:  return TSPhasePlaceTrainer();
		case TSPhase::BasisProbe:    return TSPhaseBasisProbe();
		case TSPhase::Approach:      return TSPhaseApproach();
		case TSPhase::ObserveFirstSpotted: return TSPhaseObserveFirstSpotted();
		case TSPhase::AwaitSpottedCancellation: return TSPhaseAwaitSpottedCancellation();
		case TSPhase::AwaitSpottedReentry: return TSPhaseAwaitSpottedReentry();
		case TSPhase::ObserveSecondSpotted: return TSPhaseObserveSecondSpotted();
		case TSPhase::AwaitChallenge:   return TSPhaseAwaitChallenge();
		case TSPhase::DismissChallenge: return TSPhaseDismissChallenge();
		case TSPhase::AwaitInBattle: return TSPhaseAwaitInBattle();
		case TSPhase::DriveMenu:     return TSPhaseDriveMenu();
		case TSPhase::Settle:        return TSPhaseSettle();
		case TSPhase::CinematicFreeze: return TSPhaseCinematicFreeze();
		case TSPhase::BreakSight:    return TSPhaseBreakSight();
		case TSPhase::HoldInCone:    return TSPhaseHoldInCone();
		case TSPhase::FlaglessArm:   return TSPhaseFlaglessArm();
		case TSPhase::RamblerNoBark: return TSPhaseRamblerNoBark();
		case TSPhase::Done:          return false;
		}
		return false;
	}

	bool Verify_ZMTrainerSight()
	{
		bool bPassed = true;

		if (g_bTSActive)
		{
			// Everything captured, so a failure is fully localisable from the log
			// alone without a rebuild.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] captured: failed=%s (%s) trainerPlaced=%s armed=%s "
				"spawnSeparation=%.3f (must exceed %.3f) basisPassed=%s dx=%.3f dz=%.3f "
				"approachDistance=%.3f best=%.3f stallFrames=%d held W=%d A=%d S=%d D=%d "
				"channelCaptured=%s channelTrainer=%u (want %u) reachedInBattle=%s "
				"baselineCaptured=%s leadInstalled=%s moneyBefore=%u flagBefore=%s "
				"(want false) afterCaptured=%s moneyAfter=%u (want before+%u) flagAfter=%s "
				"(want true) whiteoutAfter=%s (want false) completed=%u (want 1) aborted=%u "
				"(want 0) stateAfter=%u (want %u=IDLE) battleUnloaded=%s",
				g_bTSFailed ? "true" : "false", g_szTSFailure,
				g_bTSTrainerPlaced ? "true" : "false",
				g_bTSTrainerArmed ? "true" : "false",
				g_fTSSpawnSeparation, fTS_MIN_SPAWN_SEPARATION,
				g_bTSBasisPassed ? "true" : "false",
				g_fTSBasisDeltaX, g_fTSBasisDeltaZ,
				g_fTSCurrentDistance, g_fTSBestDistance, g_iTSStallFrames,
				(int)g_abTSHeldKeys[0], (int)g_abTSHeldKeys[1],
				(int)g_abTSHeldKeys[2], (int)g_abTSHeldKeys[3],
				g_bTSChannelCaptured ? "true" : "false",
				(u_int)g_eTSChannelTrainer, (u_int)eTS_TRAINER,
				g_bTSReachedInBattle ? "true" : "false",
				g_bTSBaselineCaptured ? "true" : "false",
				g_bTSLeadInstalled ? "true" : "false",
				g_uTSMoneyBefore, g_bTSFlagBefore ? "true" : "false",
				g_bTSAfterCaptured ? "true" : "false",
				g_uTSMoneyAfter, uTS_EXPECTED_PRIZE,
				g_bTSFlagAfter ? "true" : "false",
				g_bTSWhiteoutAfter ? "true" : "false",
				g_uTSCompletedAfter, g_uTSAbortedAfter,
				g_uTSStateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE,
				g_bTSBattleUnloaded ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] W3 spotted: first=%s samples=%d cone=%s movement=%.3f "
				"cancelIssued=%s cancelObserved=%s facingRestored=%s second=%s samples=%d "
				"cone=%s movement=%.3f completed=%s neverFrozen=%s perFrameSubmits=%s "
				"indicator before=%u afterCancel=%u afterComplete=%u spottedCount=%u "
				"elapsed=%.3f (want >= %.3f)",
				g_bTSFirstSpottedObserved ? "true" : "false",
				g_iTSFirstSpottedSamples,
				g_bTSFirstSpottedSawCone ? "true" : "false",
				g_fTSFirstSpottedMovement,
				g_bTSSpottedCancellationIssued ? "true" : "false",
				g_bTSSpottedCancellationObserved ? "true" : "false",
				g_bTSSpottedFacingRestored ? "true" : "false",
				g_bTSSecondSpottedObserved ? "true" : "false",
				g_iTSSecondSpottedSamples,
				g_bTSSecondSpottedSawCone ? "true" : "false",
				g_fTSSecondSpottedMovement,
				g_bTSSpottedCompleted ? "true" : "false",
				g_bTSSpottedNeverFrozen ? "true" : "false",
				g_bTSSpottedPerFrameSubmissions ? "true" : "false",
				g_uTSSpottedIndicatorBefore,
				g_uTSSpottedIndicatorAfterCancel,
				g_uTSSpottedIndicatorAfterComplete,
				g_uTSSpottedCountAfterComplete,
				g_fTSSpottedElapsedAfterComplete,
				ZM_TrainerSightFsmTuning{}.m_fSpottedSeconds);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] SC3 fail-open (collider-less trainer): "
				"approachEverPossible=%s (want false) approachCountMax=%u (want 0) "
				"stateEverApproaching=%s holdEverActive=%s latchEverActive=%s (all want "
				"false) | at the SPOTTED->CHALLENGING handoff: sampled=%s "
				"approachCount=%u (want 0) approachPossible=%s (want false)",
				g_bTSApproachEverPossible ? "true" : "false",
				g_uTSApproachCountMax,
				g_bTSApproachStateEverSeen ? "true" : "false",
				g_bTSApproachHoldEverSeen ? "true" : "false",
				g_bTSApproachLatchEverSeen ? "true" : "false",
				g_bTSHandoffSampled ? "true" : "false",
				g_uTSHandoffApproachCount,
				g_bTSHandoffApproachPossible ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] negatives: holdCompleted=%s holdSawCone=%s "
				"holdRaiseCount min=%u max=%u (both want 1) holdTransitionMoved=%s "
				"holdEncountersMoved=%s holdSeparation=%.3f | flaglessConfigured=%s "
				"flaglessCompleted=%s flaglessSawCone=%s flaglessMaxRaise=%u (want 0) "
				"flaglessTransitionMoved=%s flaglessSeparation=%.3f",
				g_bTSHoldCompleted ? "true" : "false",
				g_bTSHoldSawCone ? "true" : "false",
				g_uTSHoldMinRaiseCount, g_uTSHoldMaxRaiseCount,
				g_bTSHoldTransitionMoved ? "true" : "false",
				g_bTSHoldEncountersMoved ? "true" : "false",
				g_fTSHoldSeparation,
				g_bTSFlaglessConfigured ? "true" : "false",
				g_bTSFlaglessCompleted ? "true" : "false",
				g_bTSFlaglessSawCone ? "true" : "false",
				g_uTSFlaglessMaxRaise,
				g_bTSFlaglessTransitionMoved ? "true" : "false",
				g_fTSFlaglessSeparation);

			// The two SC6 gates the negatives above depend on: the production session
			// latch, and the re-arm that makes the hold's silence attributable.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] gates: latchBefore=%s (want false) latchAfterRaise=%s "
				"(want true) rearmStarted=%s stateAtBreakEntry=%u rearmConeBroken=%s "
				"rearmBroke=%s rearmRestored=%s rearmSeparation=%.3f raiseAtRearm=%u "
				"(want 1) holdEverEngaged=%s (want false)",
				g_bTSLatchBefore ? "true" : "false",
				g_bTSLatchAfter ? "true" : "false",
				g_bTSRearmStarted ? "true" : "false",
				g_uTSStateAtBreakEntry,
				g_bTSRearmConeBroken ? "true" : "false",
				g_bTSRearmBroke ? "true" : "false",
				g_bTSRearmRestored ? "true" : "false",
				g_fTSRearmSeparation, g_uTSRaiseAtRearm,
				g_bTSHoldEverEngaged ? "true" : "false");

			// S7 item 3 SC7: the bark, its ordering pins, its handoff, and the silent arm.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] SC7 bark: observed=%s missed=%s topWasDialogue=%s "
				"stateChallenging=%s transitionIdleAtBark=%s raiseAtBark=%u (want 0) "
				"challengeAtBark=%u (want 1) encountersBeforeWalk=%u (resolved=%s) "
				"encountersAtBark=%u (resolved=%s) (want equal AND both resolved) "
				"holdCompleted=%s dismissClosed=%s dismissFrames=%d "
				"barkToBattleOK=%s barkToBattleFrames=%d (want <= %d) | rambler: "
				"started=%s completed=%s sawCone=%s sawDialogue=%s (want false) "
				"sampledFrames=%d (want %d) "
				"raiseAtStart=%u maxRaise=%u maxChallenge=%u (want 0) transitionMoved=%s "
				"separation=%.3f",
				g_bTSBarkObserved ? "true" : "false",
				g_bTSBarkMissed ? "true" : "false",
				g_bTSBarkTopWasDialogue ? "true" : "false",
				g_bTSBarkStateChallenging ? "true" : "false",
				g_bTSBarkTransitionIdle ? "true" : "false",
				g_uTSBarkRaiseCount, g_uTSBarkChallengeCount,
				g_uTSEncountersBeforeWalk,
				g_bTSEncountersBeforeWalkResolved ? "true" : "false",
				g_uTSEncountersAtBark,
				g_bTSEncountersAtBarkResolved ? "true" : "false",
				g_bTSBarkHoldCompleted ? "true" : "false",
				g_bTSDismissClosed ? "true" : "false", g_iTSDismissFrames,
				g_bTSBarkToBattleOK ? "true" : "false", g_iTSBarkToBattleFrames,
				iTS_BARK_TO_BATTLE_DEADLINE,
				g_bTSRamblerStarted ? "true" : "false",
				g_bTSRamblerCompleted ? "true" : "false",
				g_bTSRamblerSawCone ? "true" : "false",
				g_bTSRamblerSawDialogue ? "true" : "false",
				g_iTSRamblerSampledFrames, iTS_RAMBLER_SAMPLED_FRAMES,
				g_uTSRamblerRaiseAtStart, g_uTSRamblerMaxRaise,
				g_uTSRamblerMaxChallenge,
				g_bTSRamblerTransitionMoved ? "true" : "false",
				g_fTSRamblerSeparation);

			// S7 item 1 SC2: the fourth freeze owner, both halves.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerSight] SC2 cinematic freeze: latchWasClear=%s (want true) "
				"unfrozenAtEntry=%s (want true) armed=%s boxRaised=%s frozenByBox=%s "
				"armedBoxClosed=%s heldFrames=%d (want %d) heldWhileArmed=%s (want true) "
				"ended=%s boxRaised2=%s freeBoxClosed=%s released=%s (want true) "
				"releaseFrames=%d (want <= %d) completed=%s",
				g_bTSCineLatchWasClear ? "true" : "false",
				g_bTSCineUnfrozenAtEntry ? "true" : "false",
				g_bTSCineArmed ? "true" : "false",
				g_bTSCineBoxRaised ? "true" : "false",
				g_bTSCineFrozenByBox ? "true" : "false",
				g_bTSCineArmedBoxClosed ? "true" : "false",
				g_iTSCineHeldFrames, iTS_CINE_HOLD_FRAMES,
				g_bTSCineHeldWhileArmed ? "true" : "false",
				g_bTSCineEnded ? "true" : "false",
				g_bTSCineBoxRaised2 ? "true" : "false",
				g_bTSCineFreeBoxClosed ? "true" : "false",
				g_bTSCineReleased ? "true" : "false",
				g_iTSCineReleaseFrames, iTS_CINE_RELEASE_DEADLINE,
				g_bTSCineCompleted ? "true" : "false");

			if (g_bTSFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_TrainerSight] %s", g_szTSFailure);
				bPassed = false;
			}

			// --- the walk-up actually happened ---
			if (!g_bTSTrainerPlaced || !g_bTSTrainerArmed || !g_bTSBasisPassed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the armed trainer was never placed, or the movement "
					"basis probe never passed -- everything below is vacuous");
				bPassed = false;
			}

			// --- W3's live presentation, including cancellation and no-freeze ---
			const bool bSpottedElapsedValid =
				std::isfinite(g_fTSSpottedElapsedAfterComplete)
				&& g_fTSSpottedElapsedAfterComplete
					>= ZM_TrainerSightFsmTuning{}.m_fSpottedSeconds;
			const bool bFirstSubmissionsExact =
				g_uTSSpottedIndicatorBefore == 0u
				&& g_uTSSpottedIndicatorAfterCancel
					== static_cast<u_int>(g_iTSFirstSpottedSamples);
			const bool bSecondSubmissionsExact =
				g_uTSSpottedIndicatorAfterComplete
					== g_uTSSpottedIndicatorAfterCancel
						+ static_cast<u_int>(g_iTSSecondSpottedSamples);
			if (!g_bTSFirstSpottedObserved
				|| !g_bTSSpottedCancellationIssued
				|| !g_bTSSpottedCancellationObserved
				|| !g_bTSSpottedFacingRestored
				|| !g_bTSSecondSpottedObserved
				|| !g_bTSSpottedCompleted
				|| !g_bTSSpottedNeverFrozen
				|| !g_bTSSpottedPerFrameSubmissions
				|| !g_bTSFirstSpottedSawCone
				|| !g_bTSSecondSpottedSawCone
				|| g_iTSFirstSpottedSamples < iTS_SPOTTED_CANCEL_SAMPLES
				|| g_iTSSecondSpottedSamples < iTS_SPOTTED_MIN_COMPLETE_SAMPLES
				|| g_fTSFirstSpottedMovement < fTS_SPOTTED_MIN_MOVEMENT
				|| g_fTSSecondSpottedMovement < fTS_SPOTTED_MIN_MOVEMENT
				|| !bFirstSubmissionsExact
				|| !bSecondSubmissionsExact
				|| g_uTSSpottedCountAfterComplete != 2u
				|| !bSpottedElapsedValid)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the live W3 marker contract was incomplete: it must "
					"submit exactly once per SPOTTED frame, leave movement enabled and "
					"physically moving, cancel on lost cone with no extra submit, re-enter, "
					"and complete its full duration before the bark");
				bPassed = false;
			}

			// --- THE CHANNEL DISCRIMINATOR. A ZM_TRAINER_NONE here means a WILD grass
			//     encounter stole the round trip, which silently DROPS the trainer
			//     raise (Dispatch returns void). ---
			if (!g_bTSChannelCaptured || g_eTSChannelTrainer != eTS_TRAINER)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the accepted round trip carried trainer %u, expected "
					"%u. If it is %u (ZM_TRAINER_NONE) a WILD GRASS encounter latched first "
					"and the trainer raise was dropped -- MOVE THE WALK LINE (see the file "
					"header), never weaken this assertion",
					(u_int)g_eTSChannelTrainer, (u_int)eTS_TRAINER, (u_int)ZM_TRAINER_NONE);
				bPassed = false;
			}
			if (!g_bTSReachedInBattle)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the encounter never reached IN_BATTLE");
				bPassed = false;
			}

			// --- ANTI-VACUITY on the payout ---
			if (!g_bTSBaselineCaptured || !g_bTSLeadInstalled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] no pre-encounter baseline was captured -- the payout "
					"deltas below are vacuous");
				bPassed = false;
			}
			else
			{
				if (g_bTSFlagBefore)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] ZM_STORY_FLAG_RIVAL1_DEFEATED was ALREADY set before "
						"the encounter -- the flag transition, and the defeat-flag gate the hold "
						"phase proves, are both vacuous");
					bPassed = false;
				}
				if (g_bTSLatchBefore)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the session latch ALREADY held trainer %u before the "
						"encounter -- Setup's ZM_TrainerEngagementLatch reset did not take, so "
						"the post-encounter latch clause proves nothing about the PRODUCTION "
						"MarkEngaged", (u_int)eTS_TRAINER);
					bPassed = false;
				}
				if (g_uTSMoneyBefore + uTS_EXPECTED_PRIZE >= uZM_MONEY_CAP)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the purse (%u) plus the prize is at/over the cap -- "
						"the credit delta would be a saturation artefact", g_uTSMoneyBefore);
					bPassed = false;
				}
			}

			// --- THE PAYOUT ---
			if (!g_bTSAfterCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] no ZM_GameState resolved after the resume -- the payout "
					"was never sampled");
				bPassed = false;
			}
			else
			{
				if (g_uTSMoneyAfter != g_uTSMoneyBefore + uTS_EXPECTED_PRIZE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the purse went %u -> %u, expected exactly +%u (the "
						"rival's authored prize)",
						g_uTSMoneyBefore, g_uTSMoneyAfter, uTS_EXPECTED_PRIZE);
					bPassed = false;
				}
				if (!g_bTSFlagAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] ZM_STORY_FLAG_RIVAL1_DEFEATED was NOT set after beating "
						"the rival the sight FSM raised");
					bPassed = false;
				}
				if (g_bTSWhiteoutAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] a WIN latched m_bPendingWhiteout -- the loss path fired "
						"on a won battle");
					bPassed = false;
				}
			}

			// --- the round trip closed cleanly ---
			if (g_uTSCompletedAfter != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] completed battle count was %u, expected exactly 1",
					g_uTSCompletedAfter);
				bPassed = false;
			}
			if (g_uTSAbortedAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] aborted transition count was %u, expected 0",
					g_uTSAbortedAfter);
				bPassed = false;
			}
			if (g_uTSStateAfter != (u_int)ZM_BATTLE_TRANSITION_IDLE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the transition settled in state %u, expected IDLE (%u)",
					g_uTSStateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE);
				bPassed = false;
			}
			if (!g_bTSBattleUnloaded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the Battle scene was still loaded after the resume");
				bPassed = false;
			}

			// ========== THE PRODUCTION SESSION LATCH (nothing else sees it) =========
			// ZM_Interactable::TickTrainerSight latches BEFORE it dispatches. Phase (8)
			// marks the flagless row BY HAND, so this sample -- taken at settle, off the
			// encounter the walk-up really drove, from a mask Setup cleared and phase (2)
			// proved clear -- is the ONLY place in the suite where deleting that
			// production line changes an observation.
			if (!g_bTSLatchAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the session latch did NOT hold trainer %u after the "
					"raise -- ZM_Interactable::TickTrainerSight's MarkEngaged never ran. A "
					"row with NO defeat flag is gated on that latch alone, so without this "
					"write its prize is farmable one raise per re-entry",
					(u_int)eTS_TRAINER);
				bPassed = false;
			}

			// ====== (7a1) THE CINEMATIC FREEZE LATCH: HOLD *AND* RELEASE (SC2) =====
			// ★ BOTH HALVES ARE REQUIRED, SEPARATELY. The hold clause alone is satisfied
			// by any build that never unfreezes anybody -- which is the specific
			// regression a fourth writer of a non-refcounted bool invites -- so the
			// release clause is what makes the hold a claim about ARBITRATION rather than
			// about paralysis.
			if (!g_bTSCineCompleted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the cinematic freeze phase never completed -- the "
					"fourth freeze owner is unproven in BOTH directions");
				bPassed = false;
			}
			else
			{
				if (!g_bTSCineLatchWasClear
					|| !g_bTSCineUnfrozenAtEntry
					|| !g_bTSCineArmed
					|| !g_bTSCineBoxRaised
					|| !g_bTSCineFrozenByBox)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the cinematic freeze phase's preconditions were "
						"not met (latchClear=%s unfrozenAtEntry=%s armed=%s boxRaised=%s "
						"frozenByBox=%s) -- 'still frozen after the box closed' would be a "
						"statement about a player who was already frozen, or about a box "
						"that never froze him",
						g_bTSCineLatchWasClear ? "true" : "false",
						g_bTSCineUnfrozenAtEntry ? "true" : "false",
						g_bTSCineArmed ? "true" : "false",
						g_bTSCineBoxRaised ? "true" : "false",
						g_bTSCineFrozenByBox ? "true" : "false");
					bPassed = false;
				}
				// THE NEGATIVE, over a real window rather than one frame.
				if (!g_bTSCineHeldWhileArmed || g_iTSCineHeldFrames < iTS_CINE_HOLD_FRAMES)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] with a cinematic freeze ARMED, closing a dialogue "
						"handed movement back (or the hold was sampled on only %d of its %d "
						"frames) -- ZM_UI_MenuStack::UnfreezePlayer must leave the player "
						"frozen for the fourth owner exactly as it already does for the warp "
						"and battle owners",
						g_iTSCineHeldFrames, iTS_CINE_HOLD_FRAMES);
					bPassed = false;
				}
				// THE PAIRED POSITIVE. This is the release path -- the one whose failure
				// strands a player frozen for the rest of the session.
				if (!g_bTSCineEnded || !g_bTSCineFreeBoxClosed || !g_bTSCineReleased)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] with the cinematic freeze ENDED, the SAME dialogue "
						"close did NOT restore movement (ended=%s closed=%s released=%s, "
						"releaseFrames=%d) -- the freeze can be taken but not given back, "
						"which is a permanently frozen player, and it also empties the "
						"'still frozen' clause above of all meaning",
						g_bTSCineEnded ? "true" : "false",
						g_bTSCineFreeBoxClosed ? "true" : "false",
						g_bTSCineReleased ? "true" : "false",
						g_iTSCineReleaseFrames);
					bPassed = false;
				}
			}

			// ================= (7) SPOTTED ONCE, NOT ONCE PER FRAME =================
			if (!g_bTSHoldCompleted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the hold-in-cone phase never completed -- the "
					"'spotted once' and defeat-flag-gate clauses below never ran");
				bPassed = false;
			}
			else
			{
				if (!g_bTSHoldSawCone)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the restored player was NEVER geometrically inside the "
						"trainer's cone during the %d-frame hold (separation %.3f) -- 'no re-raise' "
						"would be 'nothing was watching', not a proof",
						iTS_HOLD_FRAMES, g_fTSHoldSeparation);
					bPassed = false;
				}
				// WHAT MAKES THE COUNT BELOW ATTRIBUTABLE. Phase (7a2) broke sight and
				// re-entered, so the FSM is WATCHING rather than sitting on its latched
				// m_bRaiseConfirmed, and it cleared the session latch, so Vesper's
				// FLAGGED arm of ZM_MayTrainerEngage is the only gate left. Without both,
				// the count would be pinned at 1 by mechanisms this phase is not
				// measuring and the defeat-flag claim would be unsupported.
				//
				// ONE CO-DETERMINANT REMAINS UNOBSERVED, and it is named rather than
				// papered over: g_bTSHoldSawCone polls the pure SC3 cone only, so a
				// hold in which the occlusion ray reported BLOCKED every frame would
				// also be silent. Nothing new is asserted about it here because the
				// walk-up raise already required a CLEAR line in this same corridor at
				// this same range, and the conjunction itself
				// (m_bTargetInSight && m_bSightLineClear) is unit-covered in
				// ZM_Tests_TrainerSightFsm.cpp / ZM_Tests_TrainerSightProbe.cpp.
				if (!g_bTSRearmBroke || !g_bTSRearmRestored)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the hold did not begin from a COLD watcher -- the "
						"trainer was never observed WATCHING with its cone broken, or its "
						"placement facing was never restored (coneBroken=%s "
						"watchingWithConeBroken=%s facingRestored=%s). The raise count below "
						"would then be pinned at 1 by the FSM's own confirmed-raise latch and "
						"would say nothing about the defeat-flag gate",
						g_bTSRearmConeBroken ? "true" : "false",
						g_bTSRearmBroke ? "true" : "false",
						g_bTSRearmRestored ? "true" : "false");
					bPassed = false;
				}
				if (g_uTSRaiseAtRearm != 1u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the raise count entering the re-arm was %u, expected "
						"exactly 1 (one raise for the walk-up encounter)", g_uTSRaiseAtRearm);
					bPassed = false;
				}
				if (g_uTSHoldMaxRaiseCount != 1u || g_uTSHoldMinRaiseCount != 1u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the trainer's raise count over the hold ranged %u..%u, "
						"expected EXACTLY 1 the whole time. The machine was RE-ARMED to WATCHING "
						"and the session latch cleared first, so the WATCHING raise arm is "
						"re-entered every frame and -- the sight line being the one the walk-up "
						"already proved clear -- ZM_MayTrainerEngage's DEFEAT-FLAG arm is what "
						"holds it at 1: a trainer that fires once per frame, and one whose "
						"defeat flag no longer gates him, each fail here",
						g_uTSHoldMinRaiseCount, g_uTSHoldMaxRaiseCount);
					bPassed = false;
				}
				if (g_bTSHoldEverEngaged)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the re-armed sight machine left WATCHING during the "
						"hold -- the defeated rival engaged the player a second time");
					bPassed = false;
				}
				if (g_bTSHoldTransitionMoved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] a SECOND battle transition started while the player "
						"stood in the defeated rival's cone");
					bPassed = false;
				}
				if (g_bTSHoldEncountersMoved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the transition observed another encounter during the "
						"hold (baseline %u)", g_uTSEncountersAtSettle);
					bPassed = false;
				}
			}

			// ================= (8) THE FLAGLESS SESSION-LATCH ARM ==================
			if (!g_bTSFlaglessConfigured || !g_bTSFlaglessCompleted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the flagless-latch phase never completed -- the second "
					"arm of ZM_MayTrainerEngage is unproven end to end");
				bPassed = false;
			}
			else
			{
				if (!g_bTSFlaglessSawCone)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the player was NEVER geometrically inside the flagless "
						"trainer's cone during its %d-frame hold (separation %.3f) -- its silence "
						"proves nothing", iTS_HOLD2_FRAMES, g_fTSFlaglessSeparation);
					bPassed = false;
				}
				if (g_uTSFlaglessMaxRaise != 0u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the LATCHED flagless trainer raised %u encounter(s) -- "
						"a row with ZM_STORY_FLAG_NONE is gated on the session latch, and without "
						"that gate its prize is farmable (ZM_IsStoryFlagSet on the NONE sentinel "
						"returns false forever)", g_uTSFlaglessMaxRaise);
					bPassed = false;
				}
				if (g_bTSFlaglessTransitionMoved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] a battle transition started for the LATCHED flagless "
						"trainer");
					bPassed = false;
				}
			}

			// ============ (4b) THE CHALLENGE BARK CAME FIRST (SC7) =================
			// ★ THE MUTATION THESE CLAUSES EXIST FOR still delivers the battle:
			// transpose the two arms of the action switch in
			// ZM_Interactable::TickTrainerSight (dispatch the encounter on
			// RUN_CHALLENGE, bark on RAISE_ENCOUNTER) and everything above stays green.
			// ONLY the ordering pins below have teeth.
			if (!g_bTSBarkObserved || g_bTSBarkMissed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] Vesper reached the battle with no challenge bark -- the "
					"DIALOGUE screen was never observed at any point of the walk-up. Either the "
					"beat never ran (a missing game:Graphs/ZM_TrainerChallenge.bgraph, an "
					"unresolved node type, or a refused TryPushDialogue -- all of which FAIL OPEN "
					"to the battle by design) or the two action arms are transposed");
				bPassed = false;
			}
			else
			{
				if (!g_bTSBarkTopWasDialogue)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the raised screen was not the DIALOGUE screen");
					bPassed = false;
				}
				if (!g_bTSBarkStateChallenging)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the sight machine was not CHALLENGING on the frame its "
						"bark was observed -- the dialogue on screen belongs to something else");
					bPassed = false;
				}
				// ★ THE ORDERING PIN IS AN EQUALITY TEST, so an UNRESOLVED sample must
				// never be able to satisfy it. Both counts come off the
				// ZM_BattleTransition singleton; if it failed to resolve at either
				// sample point the pin below is comparing sentinels, not encounter
				// counts, and it would have "passed" having measured nothing. This
				// clause is what makes that a red, and it is separate from the equality
				// clause on purpose: the equality clause names an ordering violation,
				// this one names a missing observation.
				if (!g_bTSEncountersBeforeWalkResolved || !g_bTSEncountersAtBarkResolved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the ZM_BattleTransition singleton did not resolve at "
						"the pre-walk sample (resolved=%s) and/or on the frame the bark was "
						"observed (resolved=%s), so at least one encounter count is an "
						"unresolved sentinel -- the 'the encounter has not happened yet' "
						"ordering pin measured NOTHING",
						g_bTSEncountersBeforeWalkResolved ? "true" : "false",
						g_bTSEncountersAtBarkResolved ? "true" : "false");
					bPassed = false;
				}
				if (!g_bTSBarkTransitionIdle
					|| g_uTSEncountersAtBark != g_uTSEncountersBeforeWalk)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the battle transition was ALREADY running (or had "
						"already observed an encounter: %u vs the pre-walk %u) on the frame the "
						"bark was observed -- the bark must PRECEDE the encounter, never ride "
						"under the fade", g_uTSEncountersAtBark, g_uTSEncountersBeforeWalk);
					bPassed = false;
				}
				if (g_uTSBarkRaiseCount != 0u || g_uTSBarkChallengeCount != 1u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] at the bark the trainer had raised %u encounter(s) and "
						"started %u challenge beat(s); expected EXACTLY 0 and 1. The beat must run "
						"once and the encounter must not have been raised yet",
						g_uTSBarkRaiseCount, g_uTSBarkChallengeCount);
					bPassed = false;
				}
				if (!g_bTSBarkHoldCompleted)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the %d-frame un-pressed hold on the challenge bark never "
						"completed -- 'the battle does not start under the bark' is unproven",
						iTS_CHALLENGE_HOLD_FRAMES);
					bPassed = false;
				}

				// ======= (4c) THE HANDOFF: order 112 closes, then 113 dispatches =======
				if (!g_bTSDismissClosed)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the challenge bark never closed under confirm presses");
					bPassed = false;
				}
				else if (!g_bTSBarkToBattleOK)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the bark closed but no battle transition started within "
						"%d frames. ZM_UI_MenuStack (ECS order 112) pops, closes and UNFREEZES "
						"before ZM_Interactable (order 113) dispatches the withheld encounter, in "
						"the SAME frame -- so the trainer must not be able to talk at the player "
						"and then let him walk away", iTS_BARK_TO_BATTLE_DEADLINE);
					bPassed = false;
				}
			}

			// ================= (9) THE SILENT ARM, END TO END (SC7) =================
			if (!g_bTSRamblerStarted || !g_bTSRamblerCompleted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the silent-arm phase never completed -- a trainer row with "
					"ZERO challenge lines is unproven end to end");
				bPassed = false;
			}
			else
			{
				if (!g_bTSRamblerSawCone)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the player was NEVER geometrically inside the SILENT "
						"trainer's cone (separation %.3f) -- his battle proves nothing",
						g_fTSRamblerSeparation);
					bPassed = false;
				}
				// ★ THE WINDOW ITSELF IS A TESTED CLAIM. The no-DIALOGUE clause below is
				// only worth as many frames as it was actually sampled on, and the
				// regression it guards against is a phase that goes back to exiting the
				// instant it sees the raise and the transition -- about three frames --
				// while the comments still say 200. This reds that shrinkage directly.
				if (g_iTSRamblerSampledFrames < iTS_RAMBLER_SAMPLED_FRAMES)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the SILENT trainer's no-DIALOGUE property was sampled "
						"on only %d frame(s) of its %d-frame window -- the phase stopped holding "
						"early, so 'a row with zero challenge lines never barks' is a claim about "
						"a handful of frames rather than about the window",
						g_iTSRamblerSampledFrames, iTS_RAMBLER_SAMPLED_FRAMES);
					bPassed = false;
				}
				if (g_uTSRamblerMaxChallenge != 0u || g_bTSRamblerSawDialogue)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the SILENT trainer started %u challenge beat(s) and "
						"%s raised a dialogue. A row whose ZM_SelectTrainerChallengeLines yields "
						"zero lines must skip CHALLENGING -- the shared SPOTTED marker is "
						"intentional, but no bark window may run, and the availability test is "
						"inverted", g_uTSRamblerMaxChallenge,
						g_bTSRamblerSawDialogue ? "DID" : "did not");
					bPassed = false;
				}
				if (g_uTSRamblerMaxRaise <= g_uTSRamblerRaiseAtStart
					|| !g_bTSRamblerTransitionMoved)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the un-latched SILENT trainer's raise count went %u -> "
						"%u and transitionStarted=%s -- with the session latch cleared he must "
						"raise, and the battle must still happen after only the shared marker",
						g_uTSRamblerRaiseAtStart, g_uTSRamblerMaxRaise,
						g_bTSRamblerTransitionMoved ? "true" : "false");
					bPassed = false;
				}
			}

			// ====== (S7 item 1 SC3) THE FAIL-OPEN, ON A TRAINER WITH NO BODY =======
			//
			// ★ THIS IS THE LOAD-BEARING NEGATIVE OF THE WHOLE WALK-UP FEATURE, not a
			// convenience. m_bApproachPossible false must mean the machine behaves
			// EXACTLY as it did before APPROACHING existed -- otherwise every caller
			// that was never taught about the walk (and the entire pre-SC1 test estate)
			// silently changes behaviour. The trainer this suite places carries no
			// collider at all, which makes it the only live component in the repo that
			// can hold this claim at RUNTIME rather than in a truth table.
			//
			// Inverting the body check inside IsDrivableBodyContractMet reds HERE and in
			// the matching boot unit together: the collider-less fixture would start
			// trying to walk, freeze the player, and take a cinematic latch nobody in
			// this suite asked for.
			if (!g_bTSHandoffSampled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the SPOTTED -> CHALLENGING handoff was never "
					"sampled, so the SC3 fail-open clauses below measured nothing");
				bPassed = false;
			}
			else if (g_uTSHandoffApproachCount != 0u || g_bTSHandoffApproachPossible)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the collider-less trainer reported "
					"approachPossible=%s and had booked %u approaches at the "
					"SPOTTED -> CHALLENGING handoff (want false / 0). A component that "
					"owns NO COLLIDER cannot satisfy the DYNAMIC CAPSULE contract, so "
					"this is the body check answering backwards",
					g_bTSHandoffApproachPossible ? "true" : "false",
					g_uTSHandoffApproachCount);
				bPassed = false;
			}
			if (g_bTSApproachEverPossible || g_uTSApproachCountMax != 0u
				|| g_bTSApproachStateEverSeen || g_bTSApproachHoldEverSeen
				|| g_bTSApproachLatchEverSeen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerSight] the collider-less trainer walked, or froze the "
					"player, at some point in the run (everPossible=%s countMax=%u "
					"everApproaching=%s everHeld=%s everLatched=%s). The fail-open is not "
					"a convenience: it is what keeps this whole suite -- and every other "
					"caller that predates the walk-up -- behaving as it did",
					g_bTSApproachEverPossible ? "true" : "false", g_uTSApproachCountMax,
					g_bTSApproachStateEverSeen ? "true" : "false",
					g_bTSApproachHoldEverSeen ? "true" : "false",
					g_bTSApproachLatchEverSeen ? "true" : "false");
				bPassed = false;
			}

			// ====== (SC8) THE AUTHORED RIVAL NEVER TOUCHED THIS SUITE ==============
			// It converts "an authored live trainer does not hijack a suite that never
			// mentions him" from an assumption into a measured invariant for at least
			// this one suite.
			{
				// ★ THE SAMPLE IS MANDATORY WHEN -- AND ONLY WHEN -- DAWNMERE IS LIVE.
				// The counters below are summed over the ACTIVE scene, so with Dawnmere
				// gone the walk finds nothing, both sums stay 0, and "he raised nothing"
				// would be green having measured nothing whatever. That is exactly the
				// repo's state until the scene is re-authored WITH the rival, which is
				// the one condition this clause must not silently tolerate. Verify does
				// legitimately run with Dawnmere gone on some paths (an early FailTS
				// leaves the Battle scene active; the teardown's own FrontEnd SINGLE load
				// runs later, below), so that case is reported NOT APPLICABLE out loud
				// rather than counted as a pass.
				//
				// "Is Dawnmere the active scene" is spelled with the same
				// FindLoadedSceneByPath idiom the settle phase uses for the Battle scene,
				// against GetActiveScene() -- the scene QueryActiveScene actually walks.
				const Zenith_Scene xDawnmereScene = g_xEngine.Scenes().FindLoadedSceneByPath(
					std::string(GAME_ASSETS_DIR) + "Scenes/Dawnmere" ZENITH_SCENE_EXT);
				const bool bDawnmereIsActive = xDawnmereScene.IsValid()
					&& xDawnmereScene == g_xEngine.Scenes().GetActiveScene();

				u_int uAuthoredRaises = 0u;
				u_int uAuthoredChallenges = 0u;
				u_int uAuthoredCount = 0u;
				g_xEngine.Scenes().QueryActiveScene<ZM_Interactable>().ForEach(
					[&](Zenith_EntityID xID, ZM_Interactable& xInteractable)
					{
						// The AUTHORED rival: not this test's runtime fixture, and standing
						// on the rival's npc row.
						if (xID == g_xTSTrainerEntityID
							|| xInteractable.GetNpcId() != ZM_NPC_RIVAL_VESPER)
						{
							return;
						}
						++uAuthoredCount;
						uAuthoredRaises += xInteractable.GetTrainerSightRaiseCount();
						uAuthoredChallenges += xInteractable.GetTrainerChallengeCount();
					});

				if (!bDawnmereIsActive)
				{
					// VISIBLE, never silent: the clause did not run, and the log says so.
					Zenith_Log(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] the authored-rival silence clause is NOT "
						"APPLICABLE on this run -- Dawnmere is not the active scene at "
						"Verify (dawnmereLoaded=%s), so nothing could be sampled",
						xDawnmereScene.IsValid() ? "true" : "false");
				}
				else if (uAuthoredCount == 0u)
				{
					g_szTSFailure = "the authored rival was never sampled, so this clause "
						"proved nothing -- Dawnmere IS the active scene yet carries no "
						"ZM_NPC_RIVAL_VESPER interactable. Re-author the scene from a "
						"WINDOWED tools boot (see Source/World/ZM_DawnmerePlacement.h)";
					// Logged HERE, not through the g_bTSFailed block above: that block has
					// already run by this point, so an assignment alone would never reach
					// the log and the red would name no cause.
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerSight] %s", g_szTSFailure);
					bPassed = false;
				}
				else
				{
					// THE CLAIM ITSELF, now standing on a real sample.
					bPassed = bPassed
						&& uAuthoredRaises == 0u
						&& uAuthoredChallenges == 0u;
					if (uAuthoredRaises != 0u || uAuthoredChallenges != 0u)
					{
						g_szTSFailure = "the AUTHORED Dawnmere rival raised or barked during a "
							"suite that never mentions him -- his placement now overlaps this "
							"walk line (see Source/World/ZM_DawnmerePlacement.h)";
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_TrainerSight] %s (authoredComponentsFound=%u raises=%u "
							"challenges=%u)", g_szTSFailure, uAuthoredCount, uAuthoredRaises,
							uAuthoredChallenges);
					}
				}
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure: drop
		// the fixed timestep, clear the instant-battles flag, clear BOTH ownerless
		// latch sets, re-seed the persistent GameState (this test replaced the
		// party), force-unload any lingering Battle scene, restore FrontEnd, then
		// wipe input.
		ClearTSInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
		// S7 item 1 SC2: UNCONDITIONAL, and BEFORE the MenuStack reset below. A run that
		// died inside the cinematic phase leaves a FREEZE armed, and an armed freeze is
		// exactly what makes the MenuStack's CloseMenu-unfreeze a no-op -- releasing it
		// here is what lets the next line actually hand the player back.
		ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
		// S7 item 3 SC7: a run that died mid-bark would otherwise leave a DIALOGUE
		// screen up and the player frozen by MenuStack's own freeze. This closes it
		// (CloseMenu unfreezes) AFTER the transition reset has restored any parked
		// player, so exactly one owner releases the player and it releases it last.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT);
		if (xBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xBattle);
		}
		if (g_bTSActive)
		{
			// SINGLE-loading FrontEnd also destroys the runtime trainer entity along
			// with Dawnmere, so nothing this test created outlives it.
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bTSActive = false;

		return bPassed || !g_bTSPrereqsPresent;
	}
}

// m_bRequiresGraphics = FALSE: this test asserts nothing about pixels, so it runs
// FOR REAL on the Null (GPU-less) backend instead of skipping as a pass.
static const Zenith_AutomatedTest g_xZMTrainerSightWalkUpTest = {
	"ZM_TrainerSightWalkUp_Test",
	&Setup_ZMTrainerSight,
	&Step_ZMTrainerSight,
	&Verify_ZMTrainerSight,
	// Above the SUM of the named phase deadlines (420 ready + 1 place + 30 basis +
	// 900 approach + 150 W3 cancellation/re-entry/complete (30 + 60 + 60) + 30 bark
	// hold + 180 bark dismiss + 2 bark->battle + 600 in-battle
	// + 900 drive + 8 settle + 260 cinematic freeze (90 unfrozen + 60 dismiss + 20
	// hold + 60 dismiss + 30 release) + 120 re-arm + 200 hold + 200 hold2 + 200
	// rambler = 4201). ObserveSecondSpotted RE-ENTERS Approach with its frame counter reset,
	// re-granting that 900 on paper; measured, the second pass costs 1-2 frames
	// because the machine is already CHALLENGING. The harness jumps straight to
	// Verify when maxFrames is hit, so this
	// must exceed that sum or a slow-but-valid run would be cut off mid-battle and
	// read as a failure rather than a timeout. NOTE: the rambler phase SPENDS its
	// whole 200 -- it holds the window rather than exiting on the raise -- so the
	// slack above the sum is what absorbs a slow frame, not an early-exiting phase.
	/* maxFrames */ 4700,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTrainerSightWalkUpTest);

#endif // ZENITH_INPUT_SIMULATOR
