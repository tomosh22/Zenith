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
//   * a screen was ACTUALLY raised (the monotonic raise count moved), and
//   * a candidate that is out of range does NOT raise, and reports OUT_OF_RANGE.
// The negative half matters as much as the positive one: without it an interactor
// stubbed permanently true would pass.
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
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"

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
		Positive,
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
	bool        g_bNpcPositivePassed = false;
	bool        g_bNpcSkipped = false;
	const char* g_szNpcFailure = "test did not reach verification";

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
		g_bNpcSkipped = false;
		g_szNpcFailure = "test did not reach verification";
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
			g_eNpcPhase = NpcPhase::Positive;
			g_iNpcPhaseFrames = 0;
			return true;
		}

		case NpcPhase::Positive:
		{
			ZM_InteractionRuntime::ResetRuntimeStateForTests();
			pxNear->SetInteractable(true);   // arm the reachable candidate

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
			pxController->OnUpdate(fNPC_FIXED_DT);

			const ZM_INTERACT_REJECT eResult = xRuntime.GetLastResult();
			const Zenith_EntityID xTarget = xRuntime.GetLastTarget();
			const u_int uRaiseCount = xRuntime.GetRaiseCount();

			if (eResult != ZM_INTERACT_OK)
			{
				FailNpc("the reachable, faced, enabled NPC was not accepted");
				return false;
			}
			// Asserted by IDENTITY, not merely by "something was picked": the far NPC is
			// also enabled, so picking it would be a real defect an OK-only check misses.
			if (xTarget != g_xNpcNearEntityID)
			{
				FailNpc("the winning target was not the NEAR NPC");
				return false;
			}
			// The load-bearing assertion: a screen ACTUALLY went up. A stubbed-true
			// interactor satisfies every check above but never moves this counter.
			if (uRaiseCount != 1u)
			{
				FailNpc("Interact() did not raise exactly one screen");
				return false;
			}
			if (!ZM_UI_MenuStack::IsMenuOpen())
			{
				FailNpc("the raised dialogue did not open the menu stack");
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

			g_bNpcPositivePassed = true;
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
			&& g_bNpcPositivePassed;
		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_NpcDispatch] %s", g_szNpcFailure);
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
