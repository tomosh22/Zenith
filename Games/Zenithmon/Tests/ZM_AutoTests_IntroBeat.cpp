#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_IntroBeat (S8 item 1, ZM-D-188) -- THE INTRO, PLAYED.
//
//   FrontEnd title -> New Game (a real Enter edge) -> PlayerHome -> Dawnmere ->
//   Aster's Lab -> walk up to the professor -> read his lines -> CHOOSE a starter.
//
// ★★ WHAT ONLY THIS TEST CAN SAY, AND WHY IT IS WRITTEN THE WAY IT IS.
//
// Every other check on this beat looks at one piece of it: pure units compare
// compiled constants, the routing predicate is exercised with hand-built flag sets,
// and the grant is driven directly on a by-value state. None of them can tell you
// that the state a REAL New Game publishes is the state Aster actually grants into.
// That is the whole failure class this project keeps hitting, and it is why the
// headline assertion is not "the party is non-empty" but:
//
//   THE PARTY CONTAINS THE **CHOSEN** SPECIES AND **NOT** FERNFAWN.
//
// Fernfawn is what the deleted new-game seed handed out. An assertion satisfiable
// by that seed would pass with the entire slice reverted and prove nothing at all,
// so this test picks a deliberately NON-default starter and asserts both halves --
// what is there, and what is not.
//
// ★ IT IS ALSO THE ONLY LIVE PROOF OF THE PARTYLESS STRETCH. Between PHASE 2 and
// PHASE 4 the player owns nothing, which had never happened in a playthrough
// before this slice. The Dawnmere leg asserts rival Vesper -- an armed trainer with
// an 8 m sight cone standing in that scene -- raises nothing while the player walks
// past him with an empty party.
//
// ★★ AND IT IS A ONE-SHOT PROOF TOO. PHASE 8 walks back up to Aster with the
// starter already granted and asserts the picker does NOT raise again, that he
// speaks his POST-starter lines, and that the party did not grow. The one-shot
// property lives in ZM_NpcRaisesStarterChoice, in the RAISE path -- NOT in
// ZM_ApplyStarterChoice, which would happily grant a second monster (see
// Intro_ApplyStarterChoiceItselfWouldHappilyGrantTwice) -- so it needs its own
// live clause and this is it.
//
// ---- CONVENTIONS THIS FILE OBEYS, EACH A FAILURE THIS PROJECT HAS SHIPPED ----
//
// * STACK-FRAME RULE (ZM-D-141): per-phase driver functions behind a thin dispatch
//   Step, and NOT ONE ZM_GameState local anywhere. A monolithic /Od Step frame is
//   the SUM of every local in every phase; ~six ZM_GameState locals measured
//   1,312,136 bytes against a 1,048,576-byte reserve and died in __chkstk on the
//   first call. Every observation of the live state here is taken through a
//   POINTER and reduced to a scalar immediately.
// * STATE-SETTERS ONLY inside Step. No reentrant Zenith_MainLoop, no StepFrame, no
//   SimulateMouseClick -- those deadlock in vkWaitForFences.
// * A STEP CANNOT READ AN EDGE IT INJECTED IN THE SAME STEP. Step runs before the
//   frame's injection and before the action layer closes, so every press below is
//   made on frame N and asserted on frame N+1, in a separate phase.
// * SimulateKeyDown / SimulateKeyUp / SimulateKeyPress, NEVER SetKeyHeld -- that
//   writes only the level array, the action layer never sees an edge, and the
//   failure surfaces hundreds of frames later with an unrelated message.
// * THE WALK-UP DRIVES AT THE PROFESSOR HIMSELF, WHICH IS TO SAY IT WALKS THE
//   45-DEGREE DIAGONAL A HUMAN HOLDING W+A PRODUCES. IBDriveTowardXZ is an EIGHT-WAY
//   chooser: a target that is both sideways and deeper presses both keys. That walk
//   used to cross the lab's doorway sensor and WARP THE PLAYER OUT mid-approach
//   (Q-2026-08-15-001), and this file carried a depth-clamped "lane" target to route
//   around it. The sensor has since been RETREATED to the last quarter of the
//   walk-in span, so the workaround is gone and the natural input is walked again --
//   see the block above IBTouchesExitSensor.
// * EVERY phase flag DEFAULTS TO FAILING, so a phase that never runs fails.
// * EVERY phase owns a deadline that fires before the harness cap, or a stall is
//   reported as a bare "timed out" with no captured state.
// * Build indices and spawn tags are READ from the compiled world table, never
//   spelled: a second inventory is how a mutation stays green.
//
// C6 / ZM-D-147 SPLIT GUARD: the four .zscen files this test loads are TRACKED, so
// their absence is a DEFECT and is NOT probed for -- a skip counts as a PASS and
// would silence the gate exactly when it broke. Only the git-ignored Dawnmere
// TERRAIN family is guarded, exactly as ZM_LabRoundTrip_Test guards it, which is
// also why this test cannot be the CI gate for the beat: the boot units in
// Tests/ZM_Tests_IntroBeat.cpp are.
//
// C2 (fixed dt): 1/30, matching ZM_LabRoundTrip_Test -- this test warps between the
// same scenes and walks one short leg inside the lab.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"   // the persistent canvas the focus is read from
#include "EntityComponent/Zenith_CameraResolve.h"   // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "FileAccess/Zenith_FileAccess.h"
#include "Input/Zenith_InputSimulator.h"
#include "Zenithmon/Tests/ZM_TestWalkDrive.h"   // the ONE walk driver -- read its header before trusting a straight line
#include "Input/Zenith_KeyCodes.h"
#include "UI/Zenith_UICanvas.h"    // GetFocusedElement -- the title's + the picker's focus
#include "UI/Zenith_UIElement.h"   // GetName -- dispatch-by-focused-name, read side
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"
#include "Zenithmon/Source/UI/ZM_UI_StarterChoice.h"
#include "Zenithmon/Source/UI/ZM_UI_TitleMenu.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
	constexpr float fIB_FIXED_DT = 1.0f / 30.0f;

	// ★ A DELIBERATELY NON-DEFAULT PICK. Fernfawn is what the deleted new-game seed
	// granted, so choosing it here would make the headline assertion satisfiable by
	// the very state this slice removed.
	constexpr ZM_STARTER_CHOICE eIB_CHOICE = ZM_STARTER_CHOICE_KINDLET;

	// ---- Phase budgets ------------------------------------------------------
	// Sum: 240 + 600 + 700 + 480 + 420 + 60 + 300 + 240 + 60 + 300 + 60 = 3460,
	// inside the registered 4,200.
	constexpr int iIB_TITLE_DEADLINE      = 240;
	constexpr int iIB_PLAYERHOME_DEADLINE = 600;
	constexpr int iIB_DAWNMERE_DEADLINE   = 700;   // + terrain streaming
	constexpr int iIB_PROFLAB_DEADLINE    = 480;
	constexpr int iIB_APPROACH_DEADLINE   = 420;
	constexpr int iIB_PRESS_DEADLINE      = 60;
	constexpr int iIB_READ_DEADLINE       = 300;
	constexpr int iIB_FOCUS_DEADLINE      = 240;
	constexpr int iIB_PICK_DEADLINE       = 60;
	constexpr int iIB_SETTLE_DEADLINE     = 300;
	constexpr int iIB_SECOND_DEADLINE     = 60;

	// The approach must keep CLOSING on the professor. 60 frames (2 s at this
	// cadence) without real improvement is stuck geometry or a wrong basis, and the
	// failure says so immediately instead of burning the whole budget.
	constexpr int   iIB_STALL_LIMIT_FRAMES = 60;
	constexpr float fIB_STALL_IMPROVEMENT  = 0.01f;

	// ---- The composed failure text ------------------------------------------
	// FILE SCOPE and SIZED: the results-JSON `failures` array is always empty, so a
	// composed string logged from Verify is the only way a reader sees why this
	// failed -- and the ZM-D-141 stack rule applies to buffers as much as to states.
	char g_aszIBFailureDetail[1536] = {};
	const char* g_szIBFailure = "test did not reach verification";

	enum class IBPhase
	{
		AwaitTitle,
		PressNewGame,
		AwaitPlayerHome,
		WarpToDawnmere,
		AwaitDawnmere,
		WarpToProfLab,
		AwaitProfLab,
		Approach,
		PressInteract,
		AssertRaise,
		ReadLines,
		FocusChoice,
		PressChoice,
		AssertGrant,
		SecondInteract,
		AssertSecondInteract,
		Done,
	};

	IBPhase g_eIBPhase = IBPhase::Done;
	int  g_iIBPhaseFrames = 0;
	bool g_bIBPrerequisitesPresent = false;
	bool g_bIBSkipped = false;

	// ---- Observations. ALL DEFAULT TO FAILING. ------------------------------
	bool g_bIBTitlePassed          = false;
	bool g_bIBNewGameIsPartyless   = false;   // ★ THE GRANT-REMOVAL TRIPWIRE
	bool g_bIBLeftHomeFlagSet      = false;
	bool g_bIBVesperStayedSilent   = false;
	bool g_bIBMetProfessorFlagSet  = false;
	bool g_bIBApproachPassed       = false;
	bool g_bIBRaisePassed          = false;
	bool g_bIBPreStarterLinePassed = false;
	bool g_bIBPickerReachedTop     = false;
	bool g_bIBFocusOnChoiceCell    = false;
	bool g_bIBGrantPassed          = false;
	bool g_bIBChosenSpeciesPassed  = false;   // ★ THE HEADLINE ASSERTION
	bool g_bIBNoFernfawnPassed     = false;   // ★ ...and its other half
	bool g_bIBStarterFlagSet       = false;
	bool g_bIBMenuClosedAndFreed   = false;
	bool g_bIBSecondRaisePassed    = false;
	bool g_bIBPostStarterLinePassed = false;
	bool g_bIBNoSecondGrant        = false;

	Zenith_EntityID g_xIBAsterEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xIBAsterPosition(0.0f);
	u_int g_uIBRaiseCountBefore = 0u;
	u_int g_uIBTargetCell = 0u;
	float g_fIBBestDistance = 0.0f;
	int   g_iIBStallFrames = 0;

	void ClearIBInput()
	{
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
	}

	void FailIB(const char* szReason)
	{
		g_szIBFailure = szReason;
		g_eIBPhase = IBPhase::Done;
		ClearIBInput();
	}

	void FailIBDetailed(const char* szFormat, ...)
	{
		va_list xArgs;
		va_start(xArgs, szFormat);
		std::vsnprintf(g_aszIBFailureDetail, sizeof(g_aszIBFailureDetail),
			szFormat, xArgs);
		va_end(xArgs);
		FailIB(g_aszIBFailureDetail);
	}

	// ---- Destinations, RESOLVED rather than spelled --------------------------
	u_int IBBuild(ZM_SCENE_ID eScene) { return ZM_GetWorldSpec(eScene).m_uBuildIndex; }

	// The tag the PlayerHome exit sensor asks Dawnmere for. Read from the compiled
	// connection, exactly as the shipped sensor and ZM_IntroStoryFlagForArrival do --
	// a literal "FromHome" here would be a second inventory, and the mutation "the
	// arrival flag reads the wrong tag" would need two edits to stay green.
	const char* IBHomeExitTag()
	{
		const ZM_WorldSpec& xHome = ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME);
		if (xHome.m_pxConnections == nullptr)
		{
			return "";
		}
		for (u_int u = 0u; u < xHome.m_uConnectionCount; ++u)
		{
			if (xHome.m_pxConnections[u].m_eTarget == ZM_SCENE_DAWNMERE
				&& xHome.m_pxConnections[u].m_szSpawnTag != nullptr)
			{
				return xHome.m_pxConnections[u].m_szSpawnTag;
			}
		}
		return "";
	}

	// ---- Live-world views ----------------------------------------------------

	int IBActiveBuildIndex()
	{
		return g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
	}

	ZM_GameStateManager* IBResolveManager()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_GameStateManager>() : nullptr;
	}

	ZM_UI_MenuStack* IBResolveMenu()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
	}

	struct IBPlayerView
	{
		Zenith_EntityID       m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController*  m_pxController = nullptr;
	};

	bool IBFindPlayer(IBPlayerView& xOut)
	{
		xOut = IBPlayerView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xEntityID,
				ZM_PlayerController& xController,
				Zenith_TransformComponent& xTransform)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xEntityID;
				xOut.m_pxController = &xController;
				xTransform.GetPosition(xOut.m_xPosition);
			});
		return uCount == 1u && xOut.m_pxController != nullptr;
	}

	// ★ SCALARS OUT, NEVER A ZM_GameState COPY. Every live-state observation in this
	// file goes through this shape, so no phase frame ever holds one (ZM-D-141).
	bool IBReadStoryFlag(ZM_STORY_FLAG_ID eFlag)
	{
		ZM_GameState* pxState = nullptr;
		return ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr
			&& ZM_IsStoryFlagSet(*pxState, eFlag);
	}

	u_int IBReadPartyCount()
	{
		ZM_GameState* pxState = nullptr;
		return (ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr)
			? pxState->m_xParty.Count() : 0xFFFFFFFFu;
	}

	// The species in a given party slot, or ZM_SPECIES_NONE when the slot does not
	// exist. Guarded rather than trusting: ZM_Party::Get Zenith_Asserts out of range,
	// which BREAKS THE PROCESS in every configuration.
	ZM_SPECIES_ID IBReadPartySpecies(u_int uSlot)
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr
			|| uSlot >= pxState->m_xParty.Count())
		{
			return ZM_SPECIES_NONE;
		}
		return pxState->m_xParty.Get(uSlot).m_eSpecies;
	}

	bool IBPartyHoldsSpecies(ZM_SPECIES_ID eSpecies)
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return false;
		}
		for (u_int u = 0u; u < pxState->m_xParty.Count(); ++u)
		{
			if (pxState->m_xParty.Get(u).m_eSpecies == eSpecies)
			{
				return true;
			}
		}
		return false;
	}

	bool IBDexHasSpecies(ZM_SPECIES_ID eSpecies)
	{
		ZM_GameState* pxState = nullptr;
		return ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr
			&& pxState->m_xCaught.IsSet(eSpecies);
	}

	// The professor, re-resolved BY NAME every use. Never cached as a pointer: the
	// ECS pool relocates components on swap-and-pop.
	bool IBResolveAster(Zenith_EntityID& xEntityIDOut, Zenith_Maths::Vector3& xPositionOut)
	{
		xEntityIDOut = INVALID_ENTITY_ID;
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxData == nullptr)
		{
			return false;
		}
		Zenith_Entity xEntity =
			pxData->FindEntityByName(szZM_PROFLAB_ASTER_ENTITY_NAME);
		ZM_Interactable* pxInteractable = xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_Interactable>() : nullptr;
		Zenith_TransformComponent* pxTransform = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxInteractable == nullptr || pxTransform == nullptr
			|| !pxInteractable->IsInteractable())
		{
			return false;
		}
		pxTransform->GetPosition(xPositionOut);
		xEntityIDOut = xEntity.GetEntityID();
		return true;
	}

	// The rival's live sight machine in Dawnmere, or nulls. Absent is NOT a failure
	// on its own -- the clause that uses it says so.
	bool IBReadVesperRaiseCounts(u_int& uRaisesOut, u_int& uChallengesOut)
	{
		uRaisesOut = 0u;
		uChallengesOut = 0u;
		bool bFound = false;
		g_xEngine.Scenes().QueryActiveScene<ZM_Interactable>().ForEach(
			[&](Zenith_EntityID, ZM_Interactable& xInteractable)
			{
				if (xInteractable.GetNpcId() != ZM_NPC_RIVAL_VESPER)
				{
					return;
				}
				bFound = true;
				uRaisesOut += xInteractable.GetTrainerSightRaiseCount();
				uChallengesOut += xInteractable.GetTrainerChallengeCount();
			});
		return bFound;
	}

	const char* IBFocusedElementName()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		Zenith_UIComponent* pxUI = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
		if (pxUI == nullptr)
		{
			return nullptr;
		}
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		return pxFocused != nullptr ? pxFocused->GetName().c_str() : nullptr;
	}

	float IBPlanarDistance(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	// CAMERA-RELATIVE, the shipped ZM_AutoTests_NpcTalk / ZM_AutoTests_LabRoundTrip
	// chooser verbatim: ZM_PlayerController moves along the MAIN CAMERA'S flattened
	// basis, so "W" means "the way the camera faces" and not "+Z". A world-space
	// chooser is correct only on frames where the spring has settled at the authored
	// yaw, and has been MEASURED settling onto a stable 45-degree wrong heading.
	// The shared, unit-tested walk driver. See
	// Tests/ZM_TestWalkDrive.h -- it is CAMERA-RELATIVE and QUANTISED TO EIGHT
	// DIRECTIONS, so the player walks a PURSUIT CURVE rather than the straight
	// line, and anything this suite must approach ON A BEARING wants one of the
	// driver's fixed points. This wrapper exists only to keep the local name and
	// to mirror the held keys where this file reports them.
	void IBDriveTowardXZ(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		// NO RUN KEY, deliberately: the intro beat is timed against a WALKING
		// pace, and this was the one of the eight originals that omitted SHIFT.
		// ★ THE CLEAR STAYS HERE, AND DROPPING IT COST A BATCH. Consolidating the
		// driver moved this call out on the assumption that callers cleared before
		// driving; they do not -- every one of the eight originals cleared INSIDE,
		// and without it last frame's keys stay held and the walk curves away.
		// Six automated tests went red. The shared driver deliberately does not
		// clear, because each caller releases a DIFFERENT key set.
		ClearIBInput();
		const ZM_WalkDriveKeys xKeys =
			ZM_DriveWalkTowardXZ(xPosition, xTarget, /*bRun*/ false);
		(void)xKeys;
	}

	// ============================================================================
	// ★★ THE APPROACH TARGET IS ASTER HIMSELF, AND THAT IS THE POINT. THE LANE
	// WORKAROUND THAT USED TO LIVE HERE IS GONE (Q-2026-08-15-001, fixed 2026-08-15).
	//
	// IBDriveTowardXZ above is the shipped EIGHT-WAY chooser: every target component
	// past the dead zone presses that key, so a target that is BOTH to the side AND
	// deeper walks a 45-degree diagonal rather than the bearing. That is not an
	// artefact of driving a test -- it is EXACTLY what a human holding W+A produces,
	// which is the natural input for "up and to the left, toward the professor". So
	// it is the input this test should be giving.
	//
	// WHAT IT USED TO DO, and why the workaround existed: a diagonal spends DEPTH as
	// fast as it spends width, so the walk gained 0.934 m of depth before the picker's
	// 2.9 m reach was met, putting the capsule's leading edge (centre +
	// fZM_PROFLAB_PLAYER_RADIUS) at 6.334 -- past an exit-sensor near face that then
	// sat at 6.25. ZM_WarpTrigger::OnCollisionEnter fired, RequestWarp took the player
	// back to Dawnmere, and the interact press one frame later landed on a FROZEN
	// player in a scene that no longer contained the professor. NOTHING warned: the
	// seam had already answered ZM_INTERACT_OK, so the approach clause passed and only
	// the raise count -- 0 -> 0 -- ever said a word. This file worked around it by
	// driving at Aster's XZ with the depth CLAMPED to the arrival depth.
	//
	// THE SENSOR WAS RETREATED INSTEAD (see the two stars on the exit-sensor block in
	// Source/World/ZM_ProfLabPlacement.h): its near face is now the last quarter of
	// the walk-in span, at 7.0625, which the same 45-degree leading edge clears by
	// 0.729 m. The clamp is therefore deleted rather than kept "just in case" -- a
	// workaround left in place is a test that can no longer see the defect come back.
	//
	// ★ AND THE GUARD BELOW IS WHAT MAKES THAT A REGRESSION TEST RATHER THAN A HOPE.
	// With the lane gone, IBTouchesExitSensor is checked FIRST in PHASE 7, on the real
	// walk, every frame: retreat the sensor back toward the room and this test reds
	// with a measured leading edge against a measured near face, instead of a mystery
	// warp three phases later. The boot unit ProfLab_ExitSensorClearsTheDiagonalWalkUp
	// ToTheProfessor pins the same property in arithmetic, at boot, where it cannot be
	// skipped; the two are deliberately different KINDS of proof about one property.
	// ============================================================================

	// "Would the player's CAPSULE be touching that sensor here?" Both axes are tested
	// against the box half-extent PLUS the capsule radius, because ZM_WarpTrigger is
	// driven by a physics contact between bodies and not by a centre-in-box test -- a
	// depth-only check would false-fire for a player standing well off to the side.
	// The near face is READ from fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z rather than
	// re-subtracted from the centre: a plane spelled twice is a plane that can drift.
	bool IBTouchesExitSensor(const Zenith_Maths::Vector3& xPosition)
	{
		const float fHalfSpanX =
			fZM_PROFLAB_EXIT_TRIGGER_SCALE_X * 0.5f + fZM_PROFLAB_PLAYER_RADIUS;
		return xPosition.z + fZM_PROFLAB_PLAYER_RADIUS
				>= fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z
			&& std::fabs(xPosition.x - fZM_PROFLAB_SPAWN_X) <= fHalfSpanX;
	}

	// ---- The ONE git-ignored prerequisite ------------------------------------
	// The TRACKED .zscen files are deliberately absent from this list (ZM-D-147/148):
	// their absence is a DEFECT, and probing for them would convert that defect into
	// a SKIP, which the harness counts as a PASS.
	bool IBTerrainBakePresent()
	{
		static const char* const aszRequired[] = {
			GAME_ASSETS_DIR "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Physics_0_0" ZENITH_GEOMETRY_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_GEOMETRY_EXT,
			GAME_ASSETS_DIR "Terrain/Dawnmere/Render_0_0" ZENITH_GEOMETRY_EXT,
		};
		const u_int uCount = (u_int)(sizeof(aszRequired) / sizeof(aszRequired[0]));
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

	// ---- PHASE 1: the settled title, with New Game focused ------------------
	bool IBPhaseAwaitTitle()
	{
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		if (IBActiveBuildIndex() != (int)IBBuild(ZM_SCENE_FRONTEND)
			|| pxMenu == nullptr
			|| pxMenu->GetTopScreen() != ZM_MENU_SCREEN_TITLE
			|| IBResolveManager() == nullptr)
		{
			if (g_iIBPhaseFrames > iIB_TITLE_DEADLINE)
			{
				FailIBDetailed(
					"the FrontEnd never presented a settled TITLE in %d frames "
					"[activeBuildIndex=%d menu=%d top=%u]",
					iIB_TITLE_DEADLINE, IBActiveBuildIndex(),
					pxMenu != nullptr ? 1 : 0,
					pxMenu != nullptr ? (u_int)pxMenu->GetTopScreen() : 0u);
				return false;
			}
			return true;
		}

		const char* const szFocused = IBFocusedElementName();
		if (szFocused == nullptr
			|| std::strcmp(szFocused, ZM_UI_TitleMenu::szNEW_GAME_NAME) != 0)
		{
			if (g_iIBPhaseFrames > iIB_TITLE_DEADLINE)
			{
				FailIBDetailed(
					"the title never focused '%s' in %d frames [focused='%s']",
					ZM_UI_TitleMenu::szNEW_GAME_NAME, iIB_TITLE_DEADLINE,
					szFocused != nullptr ? szFocused : "<none>");
				return false;
			}
			return true;
		}

		g_bIBTitlePassed = true;
		g_eIBPhase = IBPhase::PressNewGame;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 2: the REAL Enter edge on New Game ---------------------------
	// The press is made here and read NEXT frame: Step runs before the frame's
	// injection and before the action layer closes, so a same-Step assertion would
	// read the world before the edge existed.
	bool IBPhasePressNewGame()
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		g_eIBPhase = IBPhase::AwaitPlayerHome;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 3: the bedroom, with an EMPTY PARTY ---------------------------
	bool IBPhaseAwaitPlayerHome()
	{
		ZM_GameStateManager* pxManager = IBResolveManager();
		if (pxManager == nullptr)
		{
			FailIB("the persistent traversal manager vanished during New Game");
			return false;
		}

		IBPlayerView xPlayer;
		if (IBActiveBuildIndex() != (int)IBBuild(ZM_SCENE_PLAYERHOME)
			|| pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
			|| !IBFindPlayer(xPlayer)
			|| !xPlayer.m_pxController->IsMovementEnabled())
		{
			if (g_iIBPhaseFrames > iIB_PLAYERHOME_DEADLINE)
			{
				FailIBDetailed(
					"New Game never delivered a movable player into PlayerHome in %d "
					"frames [activeBuildIndex=%d state=%d]. If the title never "
					"accepted the Enter edge at all, suspect the focused element "
					"rather than the warp",
					iIB_PLAYERHOME_DEADLINE, IBActiveBuildIndex(),
					(int)pxManager->GetTransitionState());
				return false;
			}
			return true;
		}

		// ★★ THE GRANT-REMOVAL TRIPWIRE, ON THE REAL PATH. RequestNewGame publishes
		// the state a human being plays; it must be PARTYLESS. Re-adding the Fernfawn
		// grant to production reds exactly here, before anything downstream can
		// paper over it.
		const u_int uPartyCount = IBReadPartyCount();
		if (uPartyCount != 0u)
		{
			FailIBDetailed(
				"a REAL New Game published a party of %u. The intro beat requires an "
				"EMPTY party: the player is supposed to choose their first partner "
				"from Professor Aster, and a pre-granted starter makes that choice "
				"cosmetic (the picker refuses only a FULL party, so the player would "
				"end up holding TWO monsters)",
				uPartyCount);
			return false;
		}
		if (IBReadStoryFlag(ZM_STORY_FLAG_STARTER_RECEIVED)
			|| IBReadStoryFlag(ZM_STORY_FLAG_INTRO_LEFT_HOME)
			|| IBReadStoryFlag(ZM_STORY_FLAG_MET_PROFESSOR))
		{
			FailIB("a fresh New Game already carries an intro story flag");
			return false;
		}
		g_bIBNewGameIsPartyless = true;

		g_eIBPhase = IBPhase::WarpToDawnmere;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 4: out of the front door ------------------------------------
	// RequestWarp(<Dawnmere>, <the PlayerHome exit's own connection tag>) is byte for
	// byte the call the shipped PlayerHomeExitTrigger makes. The DOOR ITSELF is walked
	// by ZM_PlayerHomeRoundTrip_Test; repeating that walk here would add ~600 frames
	// and re-prove a shipped seam instead of the beat this test owns.
	bool IBPhaseWarpToDawnmere()
	{
		const char* const szTag = IBHomeExitTag();
		if (szTag[0] == '\0')
		{
			FailIB("the compiled ZM_SCENE_PLAYERHOME row carries no connection "
				"targeting Dawnmere -- the front door is unbuildable");
			return false;
		}
		if (!ZM_GameStateManager::RequestWarp(IBBuild(ZM_SCENE_DAWNMERE), szTag))
		{
			FailIBDetailed(
				"RequestWarp(%u, \"%s\") was refused from a settled PlayerHome",
				IBBuild(ZM_SCENE_DAWNMERE), szTag);
			return false;
		}
		g_eIBPhase = IBPhase::AwaitDawnmere;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 5: Dawnmere, partyless, past an armed rival ------------------
	bool IBPhaseAwaitDawnmere()
	{
		ZM_GameStateManager* pxManager = IBResolveManager();
		IBPlayerView xPlayer;
		if (pxManager == nullptr
			|| IBActiveBuildIndex() != (int)IBBuild(ZM_SCENE_DAWNMERE)
			|| pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
			|| !IBFindPlayer(xPlayer)
			|| !xPlayer.m_pxController->IsMovementEnabled())
		{
			if (g_iIBPhaseFrames > iIB_DAWNMERE_DEADLINE)
			{
				FailIBDetailed(
					"the front-door warp never completed in %d frames [state=%d "
					"activeBuildIndex=%d]. IF state READS %d (WAITING_FOR_SPAWN) the "
					"'%s' tag resolved to no marker in the loaded Dawnmere -- this "
					"deadline fires well inside that barrier's own frame budget "
					"(ZM-D-200), so this message is what you get, not the runtime error",
					iIB_DAWNMERE_DEADLINE,
					pxManager != nullptr ? (int)pxManager->GetTransitionState() : -1,
					IBActiveBuildIndex(),
					(int)ZM_WARP_TRANSITION_WAITING_FOR_SPAWN, IBHomeExitTag());
				return false;
			}
			return true;
		}

		// ★ THE FIRST PRODUCTION STORY FLAG THIS GAME HAS EVER WRITTEN.
		if (!IBReadStoryFlag(ZM_STORY_FLAG_INTRO_LEFT_HOME))
		{
			FailIB("arriving in Dawnmere through the front door did not set "
				"ZM_STORY_FLAG_INTRO_LEFT_HOME -- the arrival tail's producer is not "
				"firing, and nothing else in the game writes that flag");
			return false;
		}
		g_bIBLeftHomeFlagSet = true;

		// ★ THE PARTYLESS TRAINER CLAUSE. Rival Vesper stands in this scene with an
		// 8 m sight cone; the player crossing it owns NOTHING. ZM_MayTrainerEngage's
		// bPlayerCanBattle arm is what keeps him quiet, and until this slice made
		// production partyless that arm had never run in a playthrough.
		//
		// It is honest about its own strength: whether the arrival marker even falls
		// inside his cone is a placement fact this test does not control, so the
		// clause can be vacuously satisfied by distance. The DISCRIMINATING proof is
		// the pure Intro_PartylessNewGamePlayerIsNeverEngagedByATrainer; this is the
		// live one, and it costs nothing.
		// The non-vacuous half, and it costs one read: the LIVE state crossing this
		// scene really is the one ZM_MayTrainerEngage answers FALSE for.
		if (IBReadPartyCount() != 0u)
		{
			FailIBDetailed(
				"the player reached Dawnmere holding %u monster(s) -- the partyless "
				"stretch between the front door and the lab does not exist, so the "
				"clause below is about a player who could fight after all",
				IBReadPartyCount());
			return false;
		}

		u_int uVesperRaises = 0u;
		u_int uVesperChallenges = 0u;
		if (!IBReadVesperRaiseCounts(uVesperRaises, uVesperChallenges))
		{
			FailIB("Dawnmere carries no ZM_Interactable on the rival's NPC row -- the "
				"partyless-trainer clause has nothing to observe");
			return false;
		}
		if (uVesperRaises != 0u || uVesperChallenges != 0u)
		{
			FailIBDetailed(
				"rival Vesper engaged a player with an EMPTY party [raises=%u "
				"challenges=%u]. The battle would open with nothing to send out",
				uVesperRaises, uVesperChallenges);
			return false;
		}
		g_bIBVesperStayedSilent = true;

		g_eIBPhase = IBPhase::WarpToProfLab;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 6: in through the lab door -----------------------------------
	bool IBPhaseWarpToProfLab()
	{
		if (!ZM_GameStateManager::RequestWarp(
				IBBuild(ZM_SCENE_PROFLAB), szZM_PROFLAB_SPAWN_TAG))
		{
			FailIBDetailed("RequestWarp(%u, \"%s\") was refused from Dawnmere",
				IBBuild(ZM_SCENE_PROFLAB), szZM_PROFLAB_SPAWN_TAG);
			return false;
		}
		g_eIBPhase = IBPhase::AwaitProfLab;
		g_iIBPhaseFrames = 0;
		return true;
	}

	bool IBPhaseAwaitProfLab()
	{
		ZM_GameStateManager* pxManager = IBResolveManager();
		IBPlayerView xPlayer;
		if (pxManager == nullptr
			|| IBActiveBuildIndex() != (int)IBBuild(ZM_SCENE_PROFLAB)
			|| pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
			|| !IBFindPlayer(xPlayer)
			|| !xPlayer.m_pxController->IsMovementEnabled()
			|| !IBResolveAster(g_xIBAsterEntityID, g_xIBAsterPosition))
		{
			if (g_iIBPhaseFrames > iIB_PROFLAB_DEADLINE)
			{
				FailIBDetailed(
					"the lab door never delivered a movable player beside an armed "
					"Professor Aster in %d frames [activeBuildIndex=%d state=%d "
					"aster=%d]",
					iIB_PROFLAB_DEADLINE, IBActiveBuildIndex(),
					pxManager != nullptr ? (int)pxManager->GetTransitionState() : -1,
					g_xIBAsterEntityID != INVALID_ENTITY_ID ? 1 : 0);
				return false;
			}
			return true;
		}

		if (!IBReadStoryFlag(ZM_STORY_FLAG_MET_PROFESSOR))
		{
			FailIB("arriving in Aster's lab did not set ZM_STORY_FLAG_MET_PROFESSOR");
			return false;
		}
		g_bIBMetProfessorFlagSet = true;

		// The arrival must be OUTSIDE interact reach, or the walk-up below proves
		// nothing. The placement header derives this deliberately.
		const float fStandoff =
			IBPlanarDistance(xPlayer.m_xPosition, g_xIBAsterPosition);
		if (fStandoff <= fZM_PROFLAB_ASTER_EFFECTIVE_REACH)
		{
			FailIBDetailed(
				"the player arrives %.3f m from Aster, inside his %.3f m effective "
				"reach -- the walk-up phase would be vacuous and a stray interact "
				"press could open the beat from the doormat",
				(double)fStandoff, (double)fZM_PROFLAB_ASTER_EFFECTIVE_REACH);
			return false;
		}

		g_fIBBestDistance = fStandoff;
		g_iIBStallFrames = 0;
		g_eIBPhase = IBPhase::Approach;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 7: walk up, EVENT-DRIVEN ------------------------------------
	// It ends the instant the LIVE decision says Aster is reachable; it never walks
	// N frames and hopes.
	bool IBPhaseApproach()
	{
		IBPlayerView xPlayer;
		if (!IBFindPlayer(xPlayer)
			|| !IBResolveAster(g_xIBAsterEntityID, g_xIBAsterPosition))
		{
			FailIB("the player or the professor was lost during the approach");
			return false;
		}

		// ★★ THE REGRESSION TEST FOR Q-2026-08-15-001, AND IT IS CHECKED FIRST --
		// BEFORE the seam clause below. The frame this defect actually cost was one on
		// which the seam said OK *and* the capsule was already in the sensor, so a
		// guard placed after that early-out would have watched it happen and said
		// nothing. Reported here, the scene cannot change out from under a later phase
		// with only "raises 0 -> 0" to show for it. Now that the approach drives at the
		// professor directly -- the real 45-degree W+A line -- this clause is the live
		// proof that the retreated sensor clears that walk.
		if (IBTouchesExitSensor(xPlayer.m_xPosition))
		{
			FailIBDetailed(
				"the walk-up carried the player into the lab's EXIT SENSOR [capsule "
				"leading edge %.3f m vs near face %.3f m at x=%.3f]. ZM_WarpTrigger is "
				"about to send the player back to Dawnmere, so the interact press would "
				"land on a frozen player in another scene. This is Q-2026-08-15-001: "
				"the eight-way chooser walks a 45-degree diagonal at a target that is "
				"both sideways and deeper, which gains depth as fast as width. The fix "
				"is to RETREAT THE SENSOR (ZM_ProfLabPlacement.h), never to clamp this "
				"walk back into a lane -- a clamped walk cannot see the defect return",
				(double)(xPlayer.m_xPosition.z + fZM_PROFLAB_PLAYER_RADIUS),
				(double)fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z,
				(double)xPlayer.m_xPosition.x);
			return false;
		}

		const ZM_InteractionRuntime& xRuntime =
			xPlayer.m_pxController->GetInteractionRuntime();
		Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
		if (xRuntime.EvaluateForTests(xSeamTarget) == ZM_INTERACT_OK
			&& xSeamTarget == g_xIBAsterEntityID)
		{
			ClearIBInput();
			g_bIBApproachPassed = true;
			g_uIBRaiseCountBefore = xRuntime.GetRaiseCount();
			g_eIBPhase = IBPhase::PressInteract;
			g_iIBPhaseFrames = 0;
			return true;
		}

		const float fDistance =
			IBPlanarDistance(xPlayer.m_xPosition, g_xIBAsterPosition);
		if (fDistance < g_fIBBestDistance - fIB_STALL_IMPROVEMENT)
		{
			g_fIBBestDistance = fDistance;
			g_iIBStallFrames = 0;
		}
		else if (++g_iIBStallFrames > iIB_STALL_LIMIT_FRAMES)
		{
			FailIBDetailed(
				"the walk-up STALLED %.3f m from the professor (best %.3f m). The leg "
				"is under two metres of clear floor, so this is something HOLDING the "
				"player rather than something slow",
				(double)fDistance, (double)g_fIBBestDistance);
			return false;
		}

		if (g_iIBPhaseFrames > iIB_APPROACH_DEADLINE)
		{
			FailIBDetailed(
				"the walk-up never brought Aster into interaction range in %d frames "
				"[distance %.3f m, effective reach %.3f m]",
				iIB_APPROACH_DEADLINE, (double)fDistance,
				(double)fZM_PROFLAB_ASTER_EFFECTIVE_REACH);
			return false;
		}

		// ★ AT THE PROFESSOR HIMSELF -- the eight-way chooser turns that into the
		// 45-degree W+A line a player actually holds, which is the whole point. See
		// the block above IBTouchesExitSensor: this used to be a depth-clamped lane
		// target that routed around the exit sensor, and that workaround is gone.
		IBDriveTowardXZ(xPlayer.m_xPosition, g_xIBAsterPosition);
		return true;
	}

	// ---- PHASE 8: press E (assert NEXT frame) -------------------------------
	bool IBPhasePressInteract()
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
		g_eIBPhase = IBPhase::AssertRaise;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 9: the DIALOGUE, with the PICKER underneath it ---------------
	bool IBPhaseAssertRaise()
	{
		IBPlayerView xPlayer;
		if (!IBFindPlayer(xPlayer))
		{
			FailIB("the player disappeared before the raise could be asserted");
			return false;
		}
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		if (pxMenu == nullptr)
		{
			FailIB("the ZM_MenuRoot singleton vanished before the raise");
			return false;
		}

		const ZM_InteractionRuntime& xRuntime =
			xPlayer.m_pxController->GetInteractionRuntime();
		if (xRuntime.GetRaiseCount() != g_uIBRaiseCountBefore + 1u)
		{
			if (g_iIBPhaseFrames > iIB_PRESS_DEADLINE)
			{
				FailIBDetailed(
					"pressing E at the professor did not raise EXACTLY one screen "
					"[raises %u -> %u]",
					g_uIBRaiseCountBefore, xRuntime.GetRaiseCount());
				return false;
			}
			return true;
		}

		// ★ THE STACK SHAPE IS THE BEAT. The picker is pushed FIRST and the lines
		// SECOND, so the player reads the invitation and THEN chooses -- a LIFO
		// played back in the order that reads correctly. Depth 2 with DIALOGUE on
		// top is what that looks like from outside; depth 1 means the picker never
		// raised and the beat is dialogue-only.
		if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE
			|| pxMenu->GetDepth() != 2u)
		{
			FailIBDetailed(
				"the professor's press left the menu at top=%u depth=%u; the intro "
				"beat wants DIALOGUE (%u) over STARTER (%u), i.e. depth 2. Depth 1 "
				"means ZM_NpcRaisesStarterChoice refused and the player is being "
				"talked at instead of offered a partner",
				(u_int)pxMenu->GetTopScreen(), pxMenu->GetDepth(),
				(u_int)ZM_MENU_SCREEN_DIALOGUE, (u_int)ZM_MENU_SCREEN_STARTER);
			return false;
		}
		if (xPlayer.m_pxController->IsMovementEnabled())
		{
			FailIB("the intro beat raised its screens without freezing the player");
			return false;
		}
		g_bIBRaisePassed = true;

		// The CONTENT clause: his PRE-starter set, which is the GATED array (the gate
		// is require-SET on a flag that is still clear). A row whose two sets were
		// authored the wrong way round satisfies every check above and fails here.
		const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_PROF_ASTER);
		if (xRow.m_paszGatedLines == nullptr || xRow.m_uGatedLineCount == 0u
			|| std::strcmp(pxMenu->GetDialogue().GetCurrentLine().c_str(),
				xRow.m_paszGatedLines[0]) != 0)
		{
			FailIBDetailed(
				"the open dialogue shows '%s'; the professor's PRE-starter set opens "
				"with '%s'",
				pxMenu->GetDialogue().GetCurrentLine().c_str(),
				(xRow.m_paszGatedLines != nullptr && xRow.m_uGatedLineCount > 0u)
					? xRow.m_paszGatedLines[0] : "<none authored>");
			return false;
		}
		g_bIBPreStarterLinePassed = true;

		g_eIBPhase = IBPhase::ReadLines;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 10: read to the end; the picker is revealed underneath -------
	bool IBPhaseReadLines()
	{
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		if (pxMenu == nullptr)
		{
			FailIB("the ZM_MenuRoot singleton vanished while reading the lines");
			return false;
		}

		if (pxMenu->GetTopScreen() == ZM_MENU_SCREEN_STARTER)
		{
			g_bIBPickerReachedTop = true;
			g_eIBPhase = IBPhase::FocusChoice;
			g_iIBPhaseFrames = 0;
			return true;
		}
		if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
		{
			FailIBDetailed(
				"reading the professor's lines left the menu at top=%u instead of "
				"revealing the STARTER picker (%u) beneath them",
				(u_int)pxMenu->GetTopScreen(), (u_int)ZM_MENU_SCREEN_STARTER);
			return false;
		}
		if (g_iIBPhaseFrames > iIB_READ_DEADLINE)
		{
			FailIBDetailed(
				"the dialogue never read to the end in %d frames [line %u of %u]",
				iIB_READ_DEADLINE, pxMenu->GetDialogue().GetCurrentLineIndex(),
				pxMenu->GetDialogue().GetQueuedLineCount());
			return false;
		}
		// One confirm edge per frame: the first completes the typewriter reveal, the
		// rest advance a line each. Injected here, observed on the next Step.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		return true;
	}

	// ---- PHASE 11: move the focus onto the CHOSEN cell ----------------------
	bool IBPhaseFocusChoice()
	{
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		if (pxMenu == nullptr || pxMenu->GetTopScreen() != ZM_MENU_SCREEN_STARTER)
		{
			FailIB("the starter picker was lost before a cell could be focused");
			return false;
		}

		const int iSelected = pxMenu->GetStarterScreen().GetSelectedCell();
		if (iSelected == (int)g_uIBTargetCell)
		{
			g_bIBFocusOnChoiceCell = true;
			g_eIBPhase = IBPhase::PressChoice;
			g_iIBPhaseFrames = 0;
			return true;
		}
		if (g_iIBPhaseFrames > iIB_FOCUS_DEADLINE)
		{
			FailIBDetailed(
				"the starter picker's focus never reached cell %u in %d frames "
				"[selected=%d]. The cells are ONE vertical column, so d-pad/arrow "
				"DOWN is the only navigation involved",
				g_uIBTargetCell, iIB_FOCUS_DEADLINE, iSelected);
			return false;
		}
		// The ENGINE-reserved UI focus navigation, not a game action: arrow DOWN moves
		// the canvas focus one cell. One edge per frame, read on the next Step.
		if (iSelected >= 0 && iSelected < (int)g_uIBTargetCell)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
		}
		else if (iSelected > (int)g_uIBTargetCell)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_UP);
		}
		return true;
	}

	// ---- PHASE 12: confirm the pick (assert NEXT frame) ---------------------
	bool IBPhasePressChoice()
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		g_eIBPhase = IBPhase::AssertGrant;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 13: THE HEADLINE ASSERTIONS ---------------------------------
	bool IBPhaseAssertGrant()
	{
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		if (pxMenu == nullptr)
		{
			FailIB("the ZM_MenuRoot singleton vanished during the grant");
			return false;
		}
		if (pxMenu->GetStarterGrantCount() == 0u)
		{
			if (g_iIBPhaseFrames > iIB_PICK_DEADLINE)
			{
				FailIBDetailed(
					"confirming cell %u granted nothing in %d frames [top=%u "
					"selected=%d]",
					g_uIBTargetCell, iIB_PICK_DEADLINE,
					(u_int)pxMenu->GetTopScreen(),
					pxMenu->GetStarterScreen().GetSelectedCell());
				return false;
			}
			return true;
		}
		if (pxMenu->GetStarterGrantCount() != 1u)
		{
			FailIBDetailed("the picker granted %u starters on one confirm",
				pxMenu->GetStarterGrantCount());
			return false;
		}
		g_bIBGrantPassed = true;

		// ★★ THE ASSERTION THIS WHOLE FILE EXISTS FOR, IN BOTH DIRECTIONS.
		const ZM_SPECIES_ID eExpected = ZM_ResolvePlayerStarterSpecies(eIB_CHOICE);
		const u_int uPartyCount = IBReadPartyCount();
		const ZM_SPECIES_ID eLead = IBReadPartySpecies(0u);
		if (uPartyCount != 1u || eLead != eExpected)
		{
			FailIBDetailed(
				"after choosing starter cell %u the party holds %u member(s) with "
				"lead species %u; expected exactly one of species %u (%s)",
				g_uIBTargetCell, uPartyCount, (u_int)eLead, (u_int)eExpected,
				ZM_GetSpeciesName(eExpected));
			return false;
		}
		g_bIBChosenSpeciesPassed = true;

		// ...and the half that a reverted grant-removal would fail. Fernfawn is what
		// the deleted new-game seed handed out, so its absence is the discriminating
		// evidence that the party came from THIS choice and not from that seed.
		if (IBPartyHoldsSpecies(ZM_SPECIES_FERNFAWN)
			|| IBDexHasSpecies(ZM_SPECIES_FERNFAWN))
		{
			FailIBDetailed(
				"the player owns / has recorded a Fernfawn they never chose "
				"[inParty=%d inDex=%d]. That is the DELETED new-game seed reappearing: "
				"the picker refuses only a FULL party, so a pre-granted starter leaves "
				"the player holding two monsters",
				IBPartyHoldsSpecies(ZM_SPECIES_FERNFAWN) ? 1 : 0,
				IBDexHasSpecies(ZM_SPECIES_FERNFAWN) ? 1 : 0);
			return false;
		}
		g_bIBNoFernfawnPassed = true;

		if (!IBReadStoryFlag(ZM_STORY_FLAG_STARTER_RECEIVED))
		{
			FailIB("the grant did not set ZM_STORY_FLAG_STARTER_RECEIVED -- Aster "
				"would keep offering and the intro would replay on every visit");
			return false;
		}
		g_bIBStarterFlagSet = true;

		g_eIBPhase = IBPhase::SecondInteract;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 14: the menu closes and the player is handed back -----------
	bool IBPhaseSecondInteract()
	{
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		IBPlayerView xPlayer;
		if (pxMenu == nullptr || !IBFindPlayer(xPlayer))
		{
			FailIB("the menu or the player was lost after the grant");
			return false;
		}
		if (pxMenu->IsOpen() || !xPlayer.m_pxController->IsMovementEnabled())
		{
			if (g_iIBPhaseFrames > iIB_SETTLE_DEADLINE)
			{
				FailIBDetailed(
					"the intro beat never handed the player back in %d frames "
					"[menuOpen=%d top=%u movementEnabled=%d]. A screen that took the "
					"freeze and did not release it leaves a PERMANENTLY frozen player "
					"-- m_bMovementEnabled is a bare bool with no refcount",
					iIB_SETTLE_DEADLINE, pxMenu->IsOpen() ? 1 : 0,
					(u_int)pxMenu->GetTopScreen(),
					xPlayer.m_pxController->IsMovementEnabled() ? 1 : 0);
				return false;
			}
			return true;
		}
		g_bIBMenuClosedAndFreed = true;

		// Still standing where the walk-up left us, so the seam should still name him.
		const ZM_InteractionRuntime& xRuntime =
			xPlayer.m_pxController->GetInteractionRuntime();
		Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
		if (xRuntime.EvaluateForTests(xSeamTarget) != ZM_INTERACT_OK
			|| xSeamTarget != g_xIBAsterEntityID)
		{
			if (g_iIBPhaseFrames > iIB_SETTLE_DEADLINE)
			{
				FailIB("the professor never re-armed as the interaction target after "
					"the grant, so the one-shot clause cannot be driven");
				return false;
			}
			return true;
		}

		g_uIBRaiseCountBefore = xRuntime.GetRaiseCount();
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
		g_eIBPhase = IBPhase::AssertSecondInteract;
		g_iIBPhaseFrames = 0;
		return true;
	}

	// ---- PHASE 15: THE ONE-SHOT PROOF, ON THE REAL PATH --------------------
	bool IBPhaseAssertSecondInteract()
	{
		IBPlayerView xPlayer;
		ZM_UI_MenuStack* pxMenu = IBResolveMenu();
		if (pxMenu == nullptr || !IBFindPlayer(xPlayer))
		{
			FailIB("the menu or the player was lost during the second interaction");
			return false;
		}
		const ZM_InteractionRuntime& xRuntime =
			xPlayer.m_pxController->GetInteractionRuntime();
		if (xRuntime.GetRaiseCount() != g_uIBRaiseCountBefore + 1u)
		{
			if (g_iIBPhaseFrames > iIB_SECOND_DEADLINE)
			{
				FailIBDetailed(
					"the second press at the professor raised %u screens, not one "
					"[raises %u -> %u]",
					xRuntime.GetRaiseCount() - g_uIBRaiseCountBefore,
					g_uIBRaiseCountBefore, xRuntime.GetRaiseCount());
				return false;
			}
			return true;
		}

		// ★★ NO PICKER THIS TIME. Depth 1, DIALOGUE only. A depth of 2 here means the
		// intro replays and the player farms a monster per visit -- and NOTHING below
		// this routing layer would stop it: ZM_ApplyStarterChoice refuses only an
		// unregistered choice and a FULL party.
		if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE
			|| pxMenu->GetDepth() != 1u)
		{
			FailIBDetailed(
				"the SECOND press left the menu at top=%u depth=%u. Depth 2 means the "
				"starter picker raised again with ZM_STORY_FLAG_STARTER_RECEIVED "
				"already set -- the one-shot guard in ZM_NpcRaisesStarterChoice is not "
				"holding",
				(u_int)pxMenu->GetTopScreen(), pxMenu->GetDepth());
			return false;
		}
		g_bIBSecondRaisePassed = true;

		// ...and he says his POST-starter lines now, which is the gate flipping.
		const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_PROF_ASTER);
		if (xRow.m_paszLines == nullptr || xRow.m_uLineCount == 0u
			|| std::strcmp(pxMenu->GetDialogue().GetCurrentLine().c_str(),
				xRow.m_paszLines[0]) != 0)
		{
			FailIBDetailed(
				"after the grant the professor shows '%s'; his POST-starter set opens "
				"with '%s'",
				pxMenu->GetDialogue().GetCurrentLine().c_str(),
				(xRow.m_paszLines != nullptr && xRow.m_uLineCount > 0u)
					? xRow.m_paszLines[0] : "<none authored>");
			return false;
		}
		g_bIBPostStarterLinePassed = true;

		if (pxMenu->GetStarterGrantCount() != 1u || IBReadPartyCount() != 1u)
		{
			FailIBDetailed(
				"the second interaction moved the grant count to %u and the party to "
				"%u; both must still be 1",
				pxMenu->GetStarterGrantCount(), IBReadPartyCount());
			return false;
		}
		g_bIBNoSecondGrant = true;

		g_eIBPhase = IBPhase::Done;
		return false;
	}

	// ---- Setup / dispatch / Verify -----------------------------------------

	void Setup_IntroBeat()
	{
		g_eIBPhase = IBPhase::Done;
		g_iIBPhaseFrames = 0;
		g_bIBSkipped = false;
		g_szIBFailure = "test did not reach verification";
		g_aszIBFailureDetail[0] = '\0';
		g_xIBAsterEntityID = INVALID_ENTITY_ID;
		g_xIBAsterPosition = Zenith_Maths::Vector3(0.0f);
		g_uIBRaiseCountBefore = 0u;
		g_fIBBestDistance = 0.0f;
		g_iIBStallFrames = 0;

		g_bIBTitlePassed = false;
		g_bIBNewGameIsPartyless = false;
		g_bIBLeftHomeFlagSet = false;
		g_bIBVesperStayedSilent = false;
		g_bIBMetProfessorFlagSet = false;
		g_bIBApproachPassed = false;
		g_bIBRaisePassed = false;
		g_bIBPreStarterLinePassed = false;
		g_bIBPickerReachedTop = false;
		g_bIBFocusOnChoiceCell = false;
		g_bIBGrantPassed = false;
		g_bIBChosenSpeciesPassed = false;
		g_bIBNoFernfawnPassed = false;
		g_bIBStarterFlagSet = false;
		g_bIBMenuClosedAndFreed = false;
		g_bIBSecondRaisePassed = false;
		g_bIBPostStarterLinePassed = false;
		g_bIBNoSecondGrant = false;

		// Which CELL shows the chosen starter. Cells are authored in TABLE ORDER, so
		// this is resolved rather than spelled -- a reordered table moves the walk.
		g_uIBTargetCell = 0u;
		for (u_int u = 0u; u < ZM_UI_StarterChoice::uCELL_COUNT; ++u)
		{
			if (ZM_UI_StarterChoice::CellChoice(u) == eIB_CHOICE)
			{
				g_uIBTargetCell = u;
			}
		}

		Zenith_InputSimulator::ResetAllInputState();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		// RequestSkip bypasses Verify, so no fixed dt or other process state may be
		// installed until the git-ignored prerequisite is known present.
		g_bIBPrerequisitesPresent = IBTerrainBakePresent();
		if (!g_bIBPrerequisitesPresent)
		{
			g_bIBSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"the git-ignored Dawnmere terrain bake is absent, so the intro's "
				"middle leg has no world to cross");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fIB_FIXED_DT);
		g_eIBPhase = IBPhase::AwaitTitle;
	}

	bool Step_IntroBeat(int)
	{
		if (!g_bIBPrerequisitesPresent || g_eIBPhase == IBPhase::Done)
		{
			return false;
		}
		++g_iIBPhaseFrames;
		switch (g_eIBPhase)
		{
		case IBPhase::AwaitTitle:           return IBPhaseAwaitTitle();
		case IBPhase::PressNewGame:         return IBPhasePressNewGame();
		case IBPhase::AwaitPlayerHome:      return IBPhaseAwaitPlayerHome();
		case IBPhase::WarpToDawnmere:       return IBPhaseWarpToDawnmere();
		case IBPhase::AwaitDawnmere:        return IBPhaseAwaitDawnmere();
		case IBPhase::WarpToProfLab:        return IBPhaseWarpToProfLab();
		case IBPhase::AwaitProfLab:         return IBPhaseAwaitProfLab();
		case IBPhase::Approach:             return IBPhaseApproach();
		case IBPhase::PressInteract:        return IBPhasePressInteract();
		case IBPhase::AssertRaise:          return IBPhaseAssertRaise();
		case IBPhase::ReadLines:            return IBPhaseReadLines();
		case IBPhase::FocusChoice:          return IBPhaseFocusChoice();
		case IBPhase::PressChoice:          return IBPhasePressChoice();
		case IBPhase::AssertGrant:          return IBPhaseAssertGrant();
		case IBPhase::SecondInteract:       return IBPhaseSecondInteract();
		case IBPhase::AssertSecondInteract: return IBPhaseAssertSecondInteract();
		case IBPhase::Done:                 return false;
		}
		return false;
	}

	bool Verify_IntroBeat()
	{
		ClearIBInput();
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);

		if (g_bIBSkipped)
		{
			return true;   // the harness already recorded the skip + its reason
		}

		const bool bPassed = g_bIBTitlePassed
			&& g_bIBNewGameIsPartyless
			&& g_bIBLeftHomeFlagSet
			&& g_bIBVesperStayedSilent
			&& g_bIBMetProfessorFlagSet
			&& g_bIBApproachPassed
			&& g_bIBRaisePassed
			&& g_bIBPreStarterLinePassed
			&& g_bIBPickerReachedTop
			&& g_bIBFocusOnChoiceCell
			&& g_bIBGrantPassed
			&& g_bIBChosenSpeciesPassed
			&& g_bIBNoFernfawnPassed
			&& g_bIBStarterFlagSet
			&& g_bIBMenuClosedAndFreed
			&& g_bIBSecondRaisePassed
			&& g_bIBPostStarterLinePassed
			&& g_bIBNoSecondGrant;

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_IntroBeat] %s", g_szIBFailure);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_IntroBeat] title=%d partyless=%d leftHome=%d vesperSilent=%d "
				"metProf=%d approach=%d raise=%d preLine=%d picker=%d focus=%d "
				"grant=%d chosenSpecies=%d noFernfawn=%d starterFlag=%d "
				"handedBack=%d secondRaise=%d postLine=%d noSecondGrant=%d",
				g_bIBTitlePassed ? 1 : 0, g_bIBNewGameIsPartyless ? 1 : 0,
				g_bIBLeftHomeFlagSet ? 1 : 0, g_bIBVesperStayedSilent ? 1 : 0,
				g_bIBMetProfessorFlagSet ? 1 : 0, g_bIBApproachPassed ? 1 : 0,
				g_bIBRaisePassed ? 1 : 0, g_bIBPreStarterLinePassed ? 1 : 0,
				g_bIBPickerReachedTop ? 1 : 0, g_bIBFocusOnChoiceCell ? 1 : 0,
				g_bIBGrantPassed ? 1 : 0, g_bIBChosenSpeciesPassed ? 1 : 0,
				g_bIBNoFernfawnPassed ? 1 : 0, g_bIBStarterFlagSet ? 1 : 0,
				g_bIBMenuClosedAndFreed ? 1 : 0, g_bIBSecondRaisePassed ? 1 : 0,
				g_bIBPostStarterLinePassed ? 1 : 0, g_bIBNoSecondGrant ? 1 : 0);
		}
		return bPassed;
	}
}   // close the anonymous namespace BEFORE the registration

static const Zenith_AutomatedTest g_xZMIntroBeatTest = {
	"ZM_IntroBeat_Test",
	&Setup_IntroBeat,
	&Step_IntroBeat,
	&Verify_IntroBeat,
	/* maxFrames */ 4200,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMIntroBeatTest);

#endif // ZENITH_INPUT_SIMULATOR
