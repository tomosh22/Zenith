#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_Npc -- ZM_NpcDispatch_Test (S6 item 3 SC4).
//
// THE ONE automated test in this item that genuinely runs in CI: every windowed
// walk-up test that follows (SC5-SC7) needs graphics and auto-skips headless, so
// this is the only place the live interaction path is exercised by the batch.
// m_bRequiresGraphics is therefore FALSE, and everything below is chosen to keep
// it honestly headless.
//
// It proves the DISPATCH, not the walking:
//   * the ZM_InteractionRuntime tick reaches ZM_Interactable::Interact(),
//   * the winner is the RIGHT entity (asserted by EntityID, not just "OK"),
//   * a screen was ACTUALLY raised (the monotonic raise count moved),
//   * ...and, for EACH of the three NPC roles, WHICH screen came up,
//   * that a GATED row's spoken lines follow the LIVE story flags, both ways, and
//   * a candidate that is out of range does NOT raise, and reports OUT_OF_RANGE.
// The negative half matters as much as the positive one: without it an interactor
// stubbed permanently true would pass.
//
// ★ WHY THE PER-ROLE SCREEN ASSERTIONS EXIST. VERIFIED BY MUTATION (2026-07-20):
// rewiring the SHOPKEEP and CARETAKER arms of ZM_Interactable::Interact() to call
// TryPushDialogue reddened two WINDOWED tests but left this one GREEN, because it
// only asked whether *a* screen went up. A dispatch arm pointed at the wrong seam
// was therefore invisible to CI entirely -- the one place the feature is tested
// there. Each role case below now pins the screen the role's seam actually raises:
//     ZM_NPC_ROLE_TALKER    -> TryPushDialogue         -> DIALOGUE, action NONE
//     ZM_NPC_ROLE_SHOPKEEP  -> TryOpenShop             -> SHOP, stocked from the row
//     ZM_NPC_ROLE_CARETAKER -> TryOpenCareCenterPrompt -> DIALOGUE, action HEAL_PARTY
// TALKER and CARETAKER both raise DIALOGUE, so the top-screen check ALONE cannot
// tell them apart; the pending dialogue action is what separates them, and it is
// exactly the clause the mutation above trips.
//
// Every role case reads its expectations OUT OF THE LIVE TABLE (ZM_GetNpcData) at
// runtime rather than re-spelling ids as literals: a test that restates the content
// it is checking proves only that someone typed the same thing twice.
//
// ★ WHY THE TWO STORY-GATE CASES EXIST (S7 item 2 SC1). The three role cases above
// all drive UNGATED rows, and ZM_SelectNpcLines hands back m_paszLines verbatim for
// any row whose m_paszGatedLines is null -- so with only those cases, reverting
// ZM_Interactable.cpp's ZM_NPC_RAISE_DIALOGUE arm to its pre-gating one-liner
//     bRaised = ZM_UI_MenuStack::TryPushDialogue(xRow.m_paszLines, xRow.m_uLineCount);
// left EVERY test in this game green: nothing drove a GATED row through Interact(),
// so the whole gating feature was unpinned at the integration boundary (the pure
// selector's own units cannot see the call site at all). The two cases below drive
// the ROUTE WARDEN -- the one authored row that carries both sets -- through the
// real Interact(), once with ZM_STORY_FLAG_WARDEN_CLEARED clear and once with it
// set, and assert the FIRST LINE TEXT each time. Text, not count: the warden's two
// sets happen to be the same length, so a count-only clause would not notice a
// selector (or a reverted arm) that always returns the ordinary lines.
// They live HERE, in the one headless test, rather than in a windowed walk-up:
// m_bRequiresGraphics tests are SKIPPED in CI and a skip counts as a pass, so
// gating coverage parked in one would be invisible exactly where it must not be.
//
// NO baked asset beyond what the harness already boots is touched. The fixture
// scene is created through LoadSceneByIndex(<Dawnmere>, ADDITIVE_WITHOUT_LOADING),
// which stamps the real overworld build index onto an EMPTY scene without reading
// a single byte from disk -- so the ZM_ShouldInteract overworld gate is satisfied
// with zero terrain, zero streaming and zero Dawnmere content.
//
// The one genuine external prerequisite is the persistent ZM_MenuRoot singleton
// (authored into FrontEnd, which the harness boots): without it there is no screen
// for an NPC to raise. Its absence means FrontEnd.zscen was never baked, which is
// an environment fault rather than a regression -- so that, and only that, skips.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"   // TryGetGameState -- the LIVE story flags the gate reads
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"      // ZM_SetStoryFlag / ZM_IsStoryFlagSet / ZM_STORY_FLAG_WARDEN_CLEARED
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"   // GetDialogue(): IsChoiceArmed / GetQueuedLineCount / GetCurrentLine
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"          // GetShopScreen(): GetInventoryCount / GetInventoryItem

#include <cstring>   // strcmp -- the two warden line sets must genuinely differ

namespace
{
	constexpr float fNPC_FIXED_DT = 1.0f / 60.0f;

	// Dawnmere's build index (ZM_WorldSpec / ZM-D-012). Registered at boot by
	// Project_LoadInitialScene, so it resolves whether or not the scene file exists.
	constexpr int iNPC_OVERWORLD_BUILD_INDEX = 2;

	// The player looks down +Z (identity rotation), so a candidate at +Z is IN FRONT.
	// 1.5 is inside fZM_INTERACT_MAX_DISTANCE (2.5); 20.0 is far outside it.
	constexpr float fNPC_NEAR_Z = 1.5f;
	constexpr float fNPC_FAR_Z  = 20.0f;

	enum class NpcPhase
	{
		Settle,
		Negative,
		// One phase per NPC ROLE, in role order. Each raises through the SAME near NPC
		// (re-pointed at that role's row) and then asserts the screen the role's seam
		// is contracted to raise.
		RoleTalker,
		RoleShopkeep,
		RoleCaretaker,
		// The two STORY-GATE cases, both through the SAME dialogue seam the TALKER case
		// uses, differing only in the live flag: refusal lines while the gate FAILS, the
		// ordinary lines once it PASSES.
		GatedRefusal,
		GatedCleared,
		Done,
	};

	NpcPhase        g_eNpcPhase = NpcPhase::Done;
	int             g_iNpcPhaseFrames = 0;
	Zenith_Scene    g_xNpcPreviousScene;
	Zenith_Scene    g_xNpcFixtureScene;
	Zenith_EntityID g_xNpcPlayerEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xNpcNearEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xNpcFarEntityID = INVALID_ENTITY_ID;

	// Phase flags DEFAULT TO FAILING: a phase that never runs must fail, not pass.
	bool        g_bNpcSetupComplete = false;
	bool        g_bNpcNegativePassed = false;
	bool        g_bNpcPositivePassed = false;     // the TALKER role case
	bool        g_bNpcShopkeepPassed = false;     // the SHOPKEEP role case
	bool        g_bNpcCaretakerPassed = false;    // the CARETAKER role case
	bool        g_bNpcGatedRefusalPassed = false; // the warden with the gate FAILING
	bool        g_bNpcGatedClearedPassed = false; // ...and with it PASSING
	bool        g_bNpcSkipped = false;
	// ★ STATE THIS TEST MUTATES AND MUST PUT BACK. The story flags live on the
	// DontDestroyOnLoad ZM_GameStateManager's ZM_GameState, so a flag set here would
	// otherwise be visible to every test that runs after this one in the same process.
	// Zenithmon.cpp's between-tests hook does re-seed the whole state
	// (ZM_GameStateManager::ResetGameStateForTests -> *pxGameState = ZM_MakeStarterGameState(),
	// whose ZM_StoryFlagSet is all-zero), so the leak is already covered -- but this test
	// restores the flag itself in Verify anyway rather than depending on a hook it does
	// not own, and because Verify is the only place that runs on EVERY exit path
	// (including a mid-phase failure that leaves the flag set).
	bool        g_bNpcWardenFlagCaptured = false;
	bool        g_bNpcWardenFlagOriginal = false;
	const char* g_szNpcFailure = "test did not reach verification";
	// Which role case was running when it broke. The role cases share their failure
	// text (they share their code), so this is what makes a failure legible -- the
	// same "measured values live in globals, Verify prints them" convention the rest
	// of this game's automated tests use.
	const char* g_szNpcRoleUnderTest = "<none>";

	void FailNpc(const char* szReason)
	{
		g_szNpcFailure = szReason;
		g_eNpcPhase = NpcPhase::Done;
	}

	ZM_Interactable* ResolveInteractable(Zenith_EntityID xEntityID)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_Interactable>() : nullptr;
	}

	bool TryGetPlayerPose(Zenith_Maths::Vector3& xPositionOut, Zenith_Maths::Quat& xRotationOut)
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(g_xNpcPlayerEntityID);
		Zenith_TransformComponent* pxTransform = xPlayer.IsValid()
			? xPlayer.TryGetComponent<Zenith_TransformComponent>()
			: nullptr;
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xPositionOut);
		pxTransform->GetRotation(xRotationOut);
		return true;
	}

	// ★ THE POINT OF THIS HELPER: every tick in this test is driven through the REAL
	// ZM_PlayerController::OnUpdate, never through a locally-constructed
	// ZM_InteractionRuntime. The runtime is stateless with process-global latches, so
	// a local instance behaves IDENTICALLY -- which would make this test completely
	// blind to whether the tick is wired into OnUpdate at all, or where.
	//
	// VERIFIED BY MUTATION (2026-07-20), so the strength of this claim is known
	// rather than assumed:
	//   * DELETE the m_xInteraction.Tick call from ZM_PlayerController::OnUpdate ->
	//     this test goes RED. That is the property this helper buys, and it is the
	//     defect the SC4 review caught: with a locally-constructed runtime the whole
	//     wiring half of the sub-commit had ZERO coverage.
	//   * MOVE the call below OnUpdate's collider / HasActiveSimulation early-out ->
	//     this test stays GREEN. Physics IS live headless
	//     (HasActiveSimulation() == m_pxPhysicsSystem != nullptr, and
	//     Physics().Initialise() is unconditional), and the fixture player gets a
	//     valid body from EnsureAndConfigureBody in OnStart, so that early-out never
	//     fires here. The placement is still correct on principle -- interaction is
	//     transform-only geometry -- but it is NOT pinned by this test. Do not claim
	//     otherwise.
	ZM_PlayerController* ResolveFixtureController()
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(g_xNpcPlayerEntityID);
		return xPlayer.IsValid() ? xPlayer.TryGetComponent<ZM_PlayerController>() : nullptr;
	}

	// The LIVE menu singleton. Every screen assertion below reads through THIS rather
	// than through a locally built ZM_UI_MenuStack: the three raise seams resolve the
	// singleton themselves, so anything else would be interrogating a different object
	// than the one ZM_Interactable::Interact() actually raised -- the same class of
	// blindness the local-runtime defect had.
	ZM_UI_MenuStack* ResolveLiveMenuStack()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
	}

	// The shared front half of a ROLE CASE: point the NEAR NPC at eNpcId's row, prove
	// the live decision still predicts it, press E through the REAL call site, and
	// require an OK dispatch AT that entity which raised EXACTLY ONE screen. The caller
	// then asserts WHICH screen -- the half this test used to be blind to.
	//
	// The two resets at the top are load-bearing, not hygiene. The role cases raise real
	// screens on the ONE live ZM_UI_MenuStack singleton, and:
	//   * the world gate rejects with MENU_OPEN while a previous case's screen is up
	//     (ZM_ShouldInteract blocker 2), and
	//   * the dialogue box is SINGLE-TENANT while a choice is armed -- OpenCareCenterPrompt
	//     refuses outright if the box is still active, and PushDialogueLines refuses while
	//     a choice is armed.
	// ResetRuntimeStateForTests force-closes the menu (which drops every screen's session
	// state and unfreezes the player) and clears the latched dialogue answer.
	bool RaiseThroughNearNpc(
		ZM_PlayerController& xController,
		ZM_Interactable& xNear,
		const ZM_InteractionRuntime& xRuntime,
		ZM_NPC_ID eNpcId,
		ZM_NPC_ROLE eExpectedRole,
		const char* szRoleLabel)
	{
		g_szNpcRoleUnderTest = szRoleLabel;
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		// NOT "a previous role case left the menu open" -- that is the NORMAL state here.
		// Every role case ends having raised a screen and never closes it, so cases 2 and 3
		// arrive with the menu genuinely open; the reset above is precisely what clears it.
		// What must hold is that the reset ACTUALLY closed it, because the world gate
		// rejects with MENU_OPEN while any screen is up (ZM_ShouldInteract blocker 2) and
		// every case after the first would then prove nothing. Reachable exactly when
		// ZM_UI_MenuStack::ResetRuntimeStateForTests / CloseMenu stops emptying the stack.
		if (ZM_UI_MenuStack::IsMenuOpen())
		{
			FailNpc("ResetRuntimeStateForTests left the menu open -- the world gate would "
				"reject the next raise with MENU_OPEN and prove nothing");
			return false;
		}

		// A CONTENT precondition, not a behaviour assertion: if the row this case picked
		// stopped carrying the role it is meant to exercise, the case would silently test
		// the wrong seam and still pass. Read from the live table, never restated.
		if (ZM_GetNpcData(eNpcId).m_eRole != eExpectedRole)
		{
			FailNpc("the NPC row this role case exercises no longer carries that role");
			return false;
		}
		if (!xNear.SetNpcId(eNpcId))
		{
			FailNpc("SetNpcId refused the row this role case exercises");
			return false;
		}
		xNear.SetInteractable(true);   // arm the reachable candidate

		Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
		const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
		if (eSeam != ZM_INTERACT_OK || xSeamTarget != g_xNpcNearEntityID)
		{
			FailNpc("EvaluateForTests did not predict the reachable NPC as the winner");
			return false;
		}
		if (xRuntime.HasLatchedResult())
		{
			FailNpc("EvaluateForTests must not touch a latch");
			return false;
		}

		// The RAW key code, deliberately: a windowed/automated test characterises the
		// binding rather than restating ZM_InputActions' constant back to itself.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
		// Through the REAL call site, never a local runtime -- see
		// ResolveFixtureController. This is what makes the tick's PLACEMENT
		// (above OnUpdate's collider / HasActiveSimulation early-out) load-bearing
		// for this test rather than merely asserted in a comment.
		xController.OnUpdate(fNPC_FIXED_DT);

		// Captured IMMEDIATELY, and every screen assertion the caller makes runs in THIS
		// same Step call: the player's own controller ticks the same runtime later this
		// frame, and the menu component's own OnUpdate runs later still.
		if (xRuntime.GetLastResult() != ZM_INTERACT_OK)
		{
			FailNpc("the reachable, faced, enabled NPC was not accepted");
			return false;
		}
		// Asserted by IDENTITY, not merely by "something was picked": the far NPC is
		// also enabled, so picking it would be a real defect an OK-only check misses.
		if (xRuntime.GetLastTarget() != g_xNpcNearEntityID)
		{
			FailNpc("the winning target was not the NEAR NPC");
			return false;
		}
		// The load-bearing count: a screen ACTUALLY went up, exactly once. A stubbed-true
		// interactor satisfies every check above but never moves this counter.
		if (xRuntime.GetRaiseCount() != 1u)
		{
			FailNpc("Interact() did not raise exactly one screen");
			return false;
		}
		if (!ZM_UI_MenuStack::IsMenuOpen())
		{
			FailNpc("the raised screen did not open the menu stack");
			return false;
		}
		return true;
	}

	// The LIVE story flags, read through the SAME seam ZM_Interactable::Interact()
	// reads them through. Anything else (a locally built ZM_GameState, a flag set
	// handed straight to ZM_SelectNpcLines) would prove only that the pure selector
	// works -- which its own units already do -- and would leave the dispatch arm's
	// TryGetGameState reach completely uncovered.
	//
	// ZM_GameStateRoot is authored into FrontEnd by the SAME automation block that
	// authors ZM_MenuRoot, so once Setup has cleared its menu-singleton skip this
	// manager is present too; its absence is a regression, not an environment fault,
	// and therefore FAILS rather than skips.
	ZM_GameState* ResolveLiveGameState()
	{
		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState))
		{
			return nullptr;
		}
		return pxGameState;
	}

	// CONTENT preconditions for the two gate cases. Without these the cases would
	// still "pass" against a warden row that had lost its gate, its gated set, or
	// (worst) had two IDENTICAL line sets -- in which case the first-line-text clause
	// below could never tell the two selections apart and would be pure decoration.
	bool WardenRowIsAGenuineGateDemonstration(const ZM_NpcData& xRow)
	{
		if (xRow.m_xLineGate.m_eFlag != ZM_STORY_FLAG_WARDEN_CLEARED
			|| !xRow.m_xLineGate.m_bRequireSet)
		{
			FailNpc("the warden row no longer gates on WARDEN_CLEARED being SET, so these "
				"two cases would drive the same selection twice");
			return false;
		}
		if (xRow.m_paszLines == nullptr || xRow.m_uLineCount == 0u
			|| xRow.m_paszLines[0] == nullptr
			|| xRow.m_paszGatedLines == nullptr || xRow.m_uGatedLineCount == 0u
			|| xRow.m_paszGatedLines[0] == nullptr)
		{
			FailNpc("the warden row no longer carries BOTH an ordinary and a gated line "
				"set, so the selection under test has nothing to choose between");
			return false;
		}
		// The whole reason these cases assert TEXT rather than count: the two authored
		// sets are the SAME LENGTH, so a count clause alone cannot see a selector -- or a
		// dispatch arm -- that always hands back the ordinary lines. If the first lines
		// ever became equal, the text clause would go blind the same way, silently.
		if (std::strcmp(xRow.m_paszLines[0], xRow.m_paszGatedLines[0]) == 0)
		{
			FailNpc("the warden's ordinary and gated first lines are IDENTICAL, so no "
				"assertion here can tell the two selections apart");
			return false;
		}
		return true;
	}

	// The shared back half of a GATE CASE: the raise landed on the dialogue seam and
	// queued EXACTLY the expected line array.
	//
	// FAILS IF: ZM_Interactable.cpp's ZM_NPC_RAISE_DIALOGUE arm stops consulting the
	// live flags -- most bluntly, if it is reverted to the pre-gating one-liner
	// `TryPushDialogue(xRow.m_paszLines, xRow.m_uLineCount)`, which makes the
	// gate-FAILING case queue the ordinary lines and trips the first-line clause.
	// Also fails if the arm passes a flag set that is not the live one (a
	// default-constructed ZM_StoryFlagSet reads as all-clear, which would make the
	// gate-PASSING case keep speaking the refusal), or if ZM_SelectNpcLines' two
	// outputs drift apart from each other.
	bool CheckRaisedDialogueLines(
		const ZM_UI_MenuStack& xMenu,
		const char* const* paszExpectedLines,
		u_int uExpectedLineCount)
	{
		if (xMenu.GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
		{
			FailNpc("the gated TALKER did not raise the DIALOGUE screen");
			return false;
		}
		const ZM_UI_DialogueBox& xBox = xMenu.GetDialogue();
		if (xBox.GetQueuedLineCount() != uExpectedLineCount)
		{
			FailNpc("the raised dialogue queued the wrong number of lines for the live "
				"story-flag state");
			return false;
		}
		// GetCurrentLine() answers with a shared EMPTY string when the box is inactive,
		// so an inactive box would compare unequal and read as a content mismatch. Ask
		// the honest question first -- and never let the check below run on a box whose
		// answer means "nothing is showing".
		if (!xBox.IsActive())
		{
			FailNpc("the raised dialogue is not active, so its current line means nothing");
			return false;
		}
		// THE clause the whole gate case exists for. Both sides come from the live table
		// at runtime; the expected text is never re-spelled here.
		if (xBox.GetCurrentLine() != paszExpectedLines[0])
		{
			FailNpc("the raised dialogue's FIRST LINE is not the set the live story flags "
				"select -- the dispatch arm is not consulting them");
			return false;
		}
		return true;
	}

	Zenith_EntityID CreateFixtureNpc(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		float fZ,
		bool bInteractable)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ 0.0f, 1.0f, fZ });

		ZM_Interactable& xInteractable = xEntity.AddComponent<ZM_Interactable>();
		// ZM_NPC_VILLAGER is a TALKER, so the seam under test is TryPushDialogue --
		// the one raise path that needs no game state beyond the menu singleton.
		xInteractable.SetNpcId(ZM_NPC_VILLAGER);
		xInteractable.SetInteractable(bInteractable);
		return xEntity.GetEntityID();
	}

	void Setup_NpcDispatch()
	{
		g_eNpcPhase = NpcPhase::Done;
		g_iNpcPhaseFrames = 0;
		g_bNpcSetupComplete = false;
		g_bNpcNegativePassed = false;
		g_bNpcPositivePassed = false;
		g_bNpcShopkeepPassed = false;
		g_bNpcCaretakerPassed = false;
		g_bNpcGatedRefusalPassed = false;
		g_bNpcGatedClearedPassed = false;
		g_bNpcSkipped = false;
		g_bNpcWardenFlagCaptured = false;
		g_bNpcWardenFlagOriginal = false;
		g_szNpcFailure = "test did not reach verification";
		g_szNpcRoleUnderTest = "<none>";
		g_xNpcPlayerEntityID = INVALID_ENTITY_ID;
		g_xNpcNearEntityID = INVALID_ENTITY_ID;
		g_xNpcFarEntityID = INVALID_ENTITY_ID;
		g_xNpcPreviousScene = Zenith_Scene();
		g_xNpcFixtureScene = Zenith_Scene();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fNPC_FIXED_DT);
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();

		// The ONLY legitimate skip: no persistent menu host exists, so no NPC could
		// raise anything no matter how correct the dispatch is.
		Zenith_EntityID xMenuRootID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuRootID))
		{
			g_bNpcSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_NpcDispatch] no persistent ZM_MenuRoot / ZM_UI_MenuStack singleton -- "
				"FrontEnd.zscen has not been baked (run a *_True config once), so there is "
				"no screen for an NPC to raise");
			return;
		}

		g_xNpcPreviousScene = g_xEngine.Scenes().GetActiveScene();
		// Empty scene stamped with the real overworld build index: NO file is read
		// (ADDITIVE_WITHOUT_LOADING short-circuits before any disk access), so the
		// world gate's IsActiveSceneOverworld() is satisfied with zero baked content.
		g_xNpcFixtureScene = g_xEngine.Scenes().LoadSceneByIndex(
			iNPC_OVERWORLD_BUILD_INDEX, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(g_xNpcFixtureScene);
		if (pxSceneData == nullptr
			|| !g_xEngine.Scenes().SetActiveScene(g_xNpcFixtureScene))
		{
			FailNpc("could not create the overworld-indexed fixture scene");
			return;
		}
		if (!ZM_UI_MenuStack::IsActiveSceneOverworld())
		{
			FailNpc("the fixture scene did not register as an overworld -- the world gate "
				"would refuse every interaction and the test would prove nothing");
			return;
		}

		// The player: transform + controller. The controller is what makes
		// EvaluateForTests resolvable (it finds the unique active-scene player) AND
		// what ticks the runtime for real every frame from its OnUpdate.
		Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "ZM_NpcTestPlayer");
		Zenith_TransformComponent& xPlayerTransform =
			xPlayer.GetComponent<Zenith_TransformComponent>();
		xPlayerTransform.SetPosition({ 0.0f, 1.0f, 0.0f });
		xPlayerTransform.SetRotation(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
		xPlayer.AddComponent<ZM_PlayerController>();
		g_xNpcPlayerEntityID = xPlayer.GetEntityID();

		// Near NPC starts DISABLED so the negative phase has a clean out-of-range
		// answer; the far NPC is the enabled-but-unreachable candidate that makes the
		// negative reject specifically OUT_OF_RANGE rather than NO_CANDIDATE.
		g_xNpcNearEntityID = CreateFixtureNpc(
			pxSceneData, "ZM_NpcTestNear", fNPC_NEAR_Z, /* bInteractable */ false);
		g_xNpcFarEntityID = CreateFixtureNpc(
			pxSceneData, "ZM_NpcTestFar", fNPC_FAR_Z, /* bInteractable */ true);

		g_bNpcSetupComplete = true;
		g_eNpcPhase = NpcPhase::Settle;
	}

	bool Step_NpcDispatch(int)
	{
		if (g_eNpcPhase == NpcPhase::Done)
		{
			return false;
		}
		++g_iNpcPhaseFrames;

		ZM_Interactable* pxNear = ResolveInteractable(g_xNpcNearEntityID);
		ZM_Interactable* pxFar = ResolveInteractable(g_xNpcFarEntityID);
		Zenith_Maths::Vector3 xPlayerPosition(0.0f);
		Zenith_Maths::Quat xPlayerRotation(1.0f, 0.0f, 0.0f, 0.0f);
		if (pxNear == nullptr || pxFar == nullptr
			|| !TryGetPlayerPose(xPlayerPosition, xPlayerRotation))
		{
			FailNpc("fixture entities disappeared");
			return false;
		}

		ZM_PlayerController* pxController = ResolveFixtureController();
		if (pxController == nullptr)
		{
			FailNpc("the fixture player lost its ZM_PlayerController");
			return false;
		}
		const ZM_InteractionRuntime& xRuntime = pxController->GetInteractionRuntime();

		switch (g_eNpcPhase)
		{
		case NpcPhase::Settle:
			// Let OnStart run for every fixture component before asserting on them.
			if (g_iNpcPhaseFrames < 3)
			{
				return true;
			}
			if (pxNear->IsInteractable() || !pxFar->IsInteractable())
			{
				FailNpc("fixture candidacy flags did not survive OnStart");
				return false;
			}
			g_eNpcPhase = NpcPhase::Negative;
			g_iNpcPhaseFrames = 0;
			return true;

		case NpcPhase::Negative:
		{
			ZM_InteractionRuntime::ResetRuntimeStateForTests();

			// The seam must agree with the live decision BEFORE the edge is injected:
			// they share one code path, so a divergence here is a real defect.
			Zenith_EntityID xSeamTarget = INVALID_ENTITY_ID;
			const ZM_INTERACT_REJECT eSeam = xRuntime.EvaluateForTests(xSeamTarget);
			if (eSeam != ZM_INTERACT_REJECT_OUT_OF_RANGE
				|| xSeamTarget != INVALID_ENTITY_ID)
			{
				FailNpc("EvaluateForTests did not report the unreachable candidate as OUT_OF_RANGE");
				return false;
			}

			// The RAW key code, deliberately: a windowed/automated test characterises the
			// binding rather than restating ZM_InputActions' constant back to itself.
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
			// Through the REAL call site, never a local runtime -- see
			// ResolveFixtureController. This is what makes the tick's PLACEMENT
			// (above OnUpdate's collider / HasActiveSimulation early-out) load-bearing
			// for this test rather than merely asserted in a comment.
			pxController->OnUpdate(fNPC_FIXED_DT);

			// Captured IMMEDIATELY: the player's own controller ticks the same runtime
			// later this frame and would overwrite the latches.
			const ZM_INTERACT_REJECT eResult = xRuntime.GetLastResult();
			const Zenith_EntityID xTarget = xRuntime.GetLastTarget();
			const u_int uRaiseCount = xRuntime.GetRaiseCount();

			if (eResult != ZM_INTERACT_REJECT_OUT_OF_RANGE)
			{
				FailNpc("an out-of-range NPC must reject with OUT_OF_RANGE");
				return false;
			}
			if (xTarget != INVALID_ENTITY_ID)
			{
				FailNpc("a rejected interaction must name no target");
				return false;
			}
			if (uRaiseCount != 0u)
			{
				FailNpc("an out-of-range NPC must not raise a screen");
				return false;
			}
			if (ZM_UI_MenuStack::IsMenuOpen())
			{
				FailNpc("an out-of-range NPC must leave the menu closed");
				return false;
			}

			g_bNpcNegativePassed = true;
			// The reachable candidate is armed at the START of the next phase, never
			// here: the player's own ZM_PlayerController::OnUpdate ticks the runtime
			// LATER IN THIS SAME FRAME with the E edge still asserted, so arming now
			// would let the real call site raise the dialogue before the positive phase
			// ever got to observe it.
			g_eNpcPhase = NpcPhase::RoleTalker;
			g_iNpcPhaseFrames = 0;
			return true;
		}

		case NpcPhase::RoleTalker:
		{
			if (!RaiseThroughNearNpc(*pxController, *pxNear, xRuntime,
				ZM_NPC_VILLAGER, ZM_NPC_ROLE_TALKER, "TALKER"))
			{
				return false;
			}
			const ZM_UI_MenuStack* pxMenu = ResolveLiveMenuStack();
			if (pxMenu == nullptr)
			{
				FailNpc("the live ZM_UI_MenuStack singleton could not be resolved");
				return false;
			}
			// FAILS IF: ZM_Interactable.cpp's ZM_NPC_RAISE_DIALOGUE arm stops calling
			// TryPushDialogue (e.g. is rewired to TryOpenShop), or ZM_RaiseKindForRole
			// stops mapping TALKER -> ZM_NPC_RAISE_DIALOGUE.
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailNpc("a TALKER must raise the DIALOGUE screen");
				return false;
			}
			// FAILS IF: the TALKER arm is rewired to TryOpenCareCenterPrompt. That also
			// raises DIALOGUE, so the top-screen clause above CANNOT see it -- but the
			// prompt arms HEAL_PARTY and an ordinary conversation must not.
			if (pxMenu->GetPendingDialogueAction() != ZM_DIALOGUE_ACTION_NONE)
			{
				FailNpc("an ordinary TALKER must not arm a pending dialogue action");
				return false;
			}
			// ...and the same mutation seen from the box: a care prompt ARMS a yes/no
			// choice, a conversation never does.
			if (pxMenu->GetDialogue().IsChoiceArmed())
			{
				FailNpc("an ordinary TALKER must not arm a yes/no choice");
				return false;
			}
			// FAILS IF: the arm stops handing THIS ROW's lines to the seam (e.g. passes a
			// hard-coded placeholder, or the row's count and pointer drift apart). Read
			// out of the live table at runtime -- never re-spelled here.
			if (pxMenu->GetDialogue().GetQueuedLineCount()
				!= ZM_GetNpcData(ZM_NPC_VILLAGER).m_uLineCount)
			{
				FailNpc("the raised dialogue did not carry this NPC row's own lines");
				return false;
			}

			g_bNpcPositivePassed = true;
			g_eNpcPhase = NpcPhase::RoleShopkeep;
			g_iNpcPhaseFrames = 0;
			return true;
		}

		case NpcPhase::RoleShopkeep:
		{
			if (!RaiseThroughNearNpc(*pxController, *pxNear, xRuntime,
				ZM_NPC_TRADE_POST_CLERK, ZM_NPC_ROLE_SHOPKEEP, "SHOPKEEP"))
			{
				return false;
			}
			const ZM_UI_MenuStack* pxMenu = ResolveLiveMenuStack();
			if (pxMenu == nullptr)
			{
				FailNpc("the live ZM_UI_MenuStack singleton could not be resolved");
				return false;
			}
			// FAILS IF: ZM_Interactable.cpp's ZM_NPC_RAISE_SHOP arm stops calling
			// TryOpenShop -- the EXACT mutation (SHOPKEEP -> TryPushDialogue) that used
			// to leave this test green while reddening only the windowed ones.
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_SHOP)
			{
				FailNpc("a SHOPKEEP must raise the SHOP screen");
				return false;
			}
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_TRADE_POST_CLERK);
			const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
			// A CONTENT precondition: a clerk with no stock would make every clause
			// below vacuously true.
			if (xRow.m_paeStock == nullptr || xRow.m_uStockCount == 0u)
			{
				FailNpc("the SHOPKEEP row carries no stock, so its shop assertions "
					"would be vacuous");
				return false;
			}
			// FAILS IF: the SHOP arm stops passing THIS ROW's stock (a hard-coded list, a
			// wrong row, or the count and pointer drifting apart). Both sides are read at
			// RUNTIME from ZM_GetNpcData -- re-spelling the ids here would only prove that
			// the same literals were typed twice.
			if (xShop.GetInventoryCount() != xRow.m_uStockCount)
			{
				FailNpc("the raised shop's stock count is not this NPC row's own");
				return false;
			}
			for (u_int uItem = 0u; uItem < xRow.m_uStockCount; ++uItem)
			{
				if (xShop.GetInventoryItem(uItem) != xRow.m_paeStock[uItem])
				{
					FailNpc("the raised shop's stock is not this NPC row's own");
					return false;
				}
			}

			g_bNpcShopkeepPassed = true;
			g_eNpcPhase = NpcPhase::RoleCaretaker;
			g_iNpcPhaseFrames = 0;
			return true;
		}

		case NpcPhase::RoleCaretaker:
		{
			if (!RaiseThroughNearNpc(*pxController, *pxNear, xRuntime,
				ZM_NPC_CARETAKER, ZM_NPC_ROLE_CARETAKER, "CARETAKER"))
			{
				return false;
			}
			const ZM_UI_MenuStack* pxMenu = ResolveLiveMenuStack();
			if (pxMenu == nullptr)
			{
				FailNpc("the live ZM_UI_MenuStack singleton could not be resolved");
				return false;
			}
			// The care prompt IS a dialogue, so this clause alone cannot separate it from
			// a TALKER -- it only FAILS IF the CARE_CENTER arm is rewired to TryOpenShop.
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailNpc("a CARETAKER must raise the DIALOGUE screen (its yes/no prompt)");
				return false;
			}
			// THE clause that separates the two DIALOGUE roles, and the one the verified
			// mutation trips. FAILS IF: ZM_Interactable.cpp's ZM_NPC_RAISE_CARE_CENTER arm
			// calls TryPushDialogue instead of TryOpenCareCenterPrompt -- only the prompt
			// path sets m_eDialogueAction, so plain lines leave it NONE.
			if (pxMenu->GetPendingDialogueAction() != ZM_DIALOGUE_ACTION_HEAL_PARTY)
			{
				FailNpc("a CARETAKER must arm the HEAL_PARTY prompt action, not plain lines");
				return false;
			}
			// ...and the same distinction seen from the box: only the prompt arms a
			// yes/no choice. FAILS IF the arm stops going through OpenCareCenterPrompt,
			// or that function stops arming the choice (which would strand an
			// unanswerable question on screen).
			if (!pxMenu->GetDialogue().IsChoiceArmed())
			{
				FailNpc("the CARETAKER's prompt did not arm its yes/no choice");
				return false;
			}

			// The ONLY place the reset's target/raise-count clears are genuinely
			// covered. The pure unit populates by ticking with no edge, which leaves
			// those two fields already AT their cleared values -- so dropping either
			// line from ResetRuntimeStateForTests breaks nothing there. Here the state
			// is really dirty (count 1, a live target), and the between-tests hook
			// depends on this: a leaked raise count would let a LATER batched test's
			// "GetRaiseCount() == 1" pass on this test's stale raise.
			ZM_InteractionRuntime::ResetRuntimeStateForTests();
			if (xRuntime.GetRaiseCount() != 0u
				|| xRuntime.GetLastTarget() != INVALID_ENTITY_ID
				|| xRuntime.HasLatchedResult())
			{
				FailNpc("ResetRuntimeStateForTests left a dirty latch behind");
				return false;
			}

			g_bNpcCaretakerPassed = true;
			g_eNpcPhase = NpcPhase::GatedRefusal;
			g_iNpcPhaseFrames = 0;
			return true;
		}

		case NpcPhase::GatedRefusal:
		{
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);
			if (!WardenRowIsAGenuineGateDemonstration(xRow))
			{
				return false;
			}
			ZM_GameState* pxGameState = ResolveLiveGameState();
			if (pxGameState == nullptr)
			{
				FailNpc("the persistent ZM_GameStateManager / ZM_GameState could not be "
					"resolved, so the dispatch arm has no live story flags to consult");
				return false;
			}

			// The flag is FORCED CLEAR here rather than assumed clear: this case must not
			// depend on what an earlier test (or an earlier phase) left behind, and the
			// gate-PASSING case that follows deliberately sets it. Captured first so
			// Verify can put back exactly what was there.
			if (!g_bNpcWardenFlagCaptured)
			{
				g_bNpcWardenFlagOriginal =
					ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_WARDEN_CLEARED);
				g_bNpcWardenFlagCaptured = true;
			}
			ZM_SetStoryFlag(*pxGameState, ZM_STORY_FLAG_WARDEN_CLEARED, false);
			// The write is checked, not trusted: a silently-refused clear would leave this
			// case asserting the ORDINARY lines against the gated expectation and read as
			// a dispatch defect instead of a fixture one.
			if (ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_WARDEN_CLEARED))
			{
				FailNpc("WARDEN_CLEARED could not be forced clear, so the gate under test "
					"is not in the state this case needs");
				return false;
			}

			if (!RaiseThroughNearNpc(*pxController, *pxNear, xRuntime,
				ZM_NPC_ROUTE_WARDEN, ZM_NPC_ROLE_TALKER, "WARDEN (flag CLEAR -> refusal)"))
			{
				return false;
			}
			const ZM_UI_MenuStack* pxMenu = ResolveLiveMenuStack();
			if (pxMenu == nullptr)
			{
				FailNpc("the live ZM_UI_MenuStack singleton could not be resolved");
				return false;
			}
			// FAILS IF: the dialogue arm stops selecting by the live flags -- exactly what
			// the pre-gating one-liner `TryPushDialogue(xRow.m_paszLines, xRow.m_uLineCount)`
			// does. With the gate FAILING the warden owes the player his refusal; the
			// one-liner speaks the open-road lines instead, and the first-line clause inside
			// CheckRaisedDialogueLines is what sees it (the two sets are the same LENGTH,
			// so the count clause alone would not).
			if (!CheckRaisedDialogueLines(*pxMenu,
				xRow.m_paszGatedLines, xRow.m_uGatedLineCount))
			{
				return false;
			}

			g_bNpcGatedRefusalPassed = true;
			g_eNpcPhase = NpcPhase::GatedCleared;
			g_iNpcPhaseFrames = 0;
			return true;
		}

		case NpcPhase::GatedCleared:
		{
			const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);
			ZM_GameState* pxGameState = ResolveLiveGameState();
			if (pxGameState == nullptr)
			{
				FailNpc("the persistent ZM_GameStateManager / ZM_GameState could not be "
					"resolved, so the dispatch arm has no live story flags to consult");
				return false;
			}

			// The ONE mutation this case is about: flip the story beat and interact again.
			// Nothing else changes -- same NPC row, same seam, same fixture.
			ZM_SetStoryFlag(*pxGameState, ZM_STORY_FLAG_WARDEN_CLEARED, true);
			if (!ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_WARDEN_CLEARED))
			{
				FailNpc("WARDEN_CLEARED could not be set, so this case would merely repeat "
					"the gate-failing one");
				return false;
			}

			if (!RaiseThroughNearNpc(*pxController, *pxNear, xRuntime,
				ZM_NPC_ROUTE_WARDEN, ZM_NPC_ROLE_TALKER, "WARDEN (flag SET -> ordinary)"))
			{
				return false;
			}
			const ZM_UI_MenuStack* pxMenu = ResolveLiveMenuStack();
			if (pxMenu == nullptr)
			{
				FailNpc("the live ZM_UI_MenuStack singleton could not be resolved");
				return false;
			}
			// FAILS IF: the arm hands the selector something other than the LIVE flags --
			// a default-constructed ZM_StoryFlagSet, or the manager-less all-clear
			// fallback taken unconditionally -- because either keeps the warden refusing
			// after the road has been cleared. Together with the case above, this pins the
			// gate in BOTH directions: a selector wired to always-gated fails here, one
			// wired to never-gated (the reverted one-liner) fails there.
			if (!CheckRaisedDialogueLines(*pxMenu, xRow.m_paszLines, xRow.m_uLineCount))
			{
				return false;
			}

			g_bNpcGatedClearedPassed = true;
			g_eNpcPhase = NpcPhase::Done;
			return false;
		}

		case NpcPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_NpcDispatch()
	{
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		// PUT THE STORY FLAG BACK. It lives on the DontDestroyOnLoad ZM_GameState, which
		// outlives this test, so leaving WARDEN_CLEARED set would hand every later test in
		// the batch a world where the warden's gate has already been passed. Restored to
		// the value that was actually there, and done HERE because Verify is the one place
		// that runs on every exit path -- including a phase that failed with the flag set.
		if (g_bNpcWardenFlagCaptured)
		{
			if (ZM_GameState* pxGameState = ResolveLiveGameState())
			{
				ZM_SetStoryFlag(*pxGameState, ZM_STORY_FLAG_WARDEN_CLEARED,
					g_bNpcWardenFlagOriginal);
			}
			g_bNpcWardenFlagCaptured = false;
		}

		if (g_xNpcPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xNpcPreviousScene);
		}
		if (g_xNpcFixtureScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xNpcFixtureScene);
			g_xNpcFixtureScene = Zenith_Scene();
		}

		if (g_bNpcSkipped)
		{
			// The harness already recorded the skip + its reason in Setup.
			return true;
		}

		const bool bPassed = g_bNpcSetupComplete
			&& g_bNpcNegativePassed
			&& g_bNpcPositivePassed
			&& g_bNpcShopkeepPassed
			&& g_bNpcCaretakerPassed
			&& g_bNpcGatedRefusalPassed
			&& g_bNpcGatedClearedPassed;
		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_NpcDispatch] %s (role case: %s)",
				g_szNpcFailure, g_szNpcRoleUnderTest);
		}
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMNpcDispatchTest = {
	"ZM_NpcDispatch_Test",
	&Setup_NpcDispatch,
	&Step_NpcDispatch,
	&Verify_NpcDispatch,
	/* maxFrames */ 240,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMNpcDispatchTest);

#endif // ZENITH_INPUT_SIMULATOR
