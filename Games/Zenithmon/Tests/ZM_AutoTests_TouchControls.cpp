#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_Pointers.h"
#include "UI/Zenith_UIVirtualButton.h"
#include "UI/Zenith_UIVirtualStick.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TouchLayoutController.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/ZM_Bindings.h"

#include <cmath>

// =============================================================================
// Zenithmon input program WP3b -- the ON-SCREEN CONTROLS, end to end.
//
// Six windowed/headless automated tests that drive the real B9 widgets on the
// persistent ZM_TouchRoot HUD with real pointer events, through the real frame
// contract, and assert on what the GAME did -- the player's world position, the
// interaction runtime's latch, the dialogue box's line -- rather than on the
// widget's own bookkeeping.
//
// ★ THE ONE-FRAME LAG IS STRUCTURAL, NOT A HACK. A Step runs at PumpAutomatedTest
// (before step 7), so anything it injects is applied, closed and consumed LATER
// IN THE SAME FRAME -- and the earliest a Step can OBSERVE the result is the
// following frame. Every phase below therefore injects on one frame and asserts
// on the next. A test that asserts in the same Step it injected in reads the
// PREVIOUS frame's state and passes or fails for the wrong reason.
//
// ★ NO m_bRequiresGraphics. Nothing here reads a pixel: pointer hit-testing,
// claims, virtual publishes and the action close all run identically on the Null
// backend, so these are CI-visible. Setting the flag would have made the whole
// touch path invisible to the gate that actually runs.
// =============================================================================

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;

	// ---- The asset-free fixture (floor + player + camera at yaw 0) -----------
	//
	// Camera yaw 0 means camera-forward is world +Z and camera-right is world +X,
	// so a stick axis of (+x, +y) must move the player (+X, +Z). That is the whole
	// reason the fixture pins the yaw instead of taking whatever it inherits.

	Zenith_Scene g_xPreviousScene;
	Zenith_Scene g_xFixtureScene;
	int          g_iPhase = 0;
	int          g_iPhaseFrames = 0;
	bool         g_bPassed = false;
	const char*  g_szFailure = "test did not reach verification";

	Zenith_Maths::Vector3 g_xStartPosition(0.0f);
	Zenith_Maths::Vector2 g_xObservedAxis(0.0f);
	bool  g_bObservedInteract = false;
	bool  g_bObservedConfirm  = false;
	bool  g_bObservedCancel   = false;
	u_int g_uDialogueLineBefore = 0u;
	u_int g_uDialogueLineAfter  = 0u;
	u_int8 g_uProfileAfterBack  = Zenith_InputActions::uPROFILE_AUTO;

	void Fail(const char* szReason)
	{
		g_szFailure = szReason;
		g_bPassed = false;
		g_iPhase = -1;
	}

	Zenith_UIComponent* ResolveTouchUI()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_TouchLayoutController::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid() ? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
	}

	Zenith_UI::Zenith_UIVirtualStick* ResolveStick()
	{
		Zenith_UIComponent* pxUI = ResolveTouchUI();
		return pxUI != nullptr
			? pxUI->FindElement<Zenith_UI::Zenith_UIVirtualStick>(
				ZM_TouchLayoutController::szSTICK_NAME)
			: nullptr;
	}

	Zenith_UI::Zenith_UIVirtualButton* ResolveButton(const char* szName)
	{
		Zenith_UIComponent* pxUI = ResolveTouchUI();
		return pxUI != nullptr
			? pxUI->FindElement<Zenith_UI::Zenith_UIVirtualButton>(szName)
			: nullptr;
	}

	Zenith_Maths::Vector2 RectCentre(const Zenith_Maths::Vector4& xRect)
	{
		return { (xRect.x + xRect.z) * 0.5f, (xRect.y + xRect.w) * 0.5f };
	}

	// A point inside the A button's hit rect, computed FROM the live rect so the
	// test never restates the authored geometry (which would pass whatever the
	// authoring said, including nothing).
	bool ButtonTouchPoint(const char* szName, Zenith_Maths::Vector2& xOut)
	{
		Zenith_UI::Zenith_UIVirtualButton* pxButton = ResolveButton(szName);
		if (pxButton == nullptr)
		{
			return false;
		}
		xOut = RectCentre(pxButton->GetHitRect(g_xEngine.Pointers().GetDisplayScale()));
		return true;
	}

	// The stick's base centre, plus an optional offset in RADII. Canvas +y is DOWN
	// and the published axis is +y FORWARD, so a forward-right tilt is (+x, -y).
	bool StickTouchPoint(float fRadiiX, float fRadiiY, Zenith_Maths::Vector2& xOut)
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
		if (pxStick == nullptr)
		{
			return false;
		}
		const float fScale = g_xEngine.Pointers().GetDisplayScale();
		const Zenith_Maths::Vector2 xCentre =
			RectCentre(pxStick->GetActivationRect(fScale));
		const float fRadiusPx = pxStick->GetRadius() * (fScale > 0.0f ? fScale : 1.0f);
		xOut = { xCentre.x + fRadiiX * fRadiusPx, xCentre.y + fRadiiY * fRadiusPx };
		return true;
	}

	bool FindPlayer(ZM_PlayerController*& pxOut, Zenith_Maths::Vector3& xPositionOut)
	{
		pxOut = nullptr;
		ZM_PlayerController* pxFound = nullptr;
		Zenith_Maths::Vector3 xPosition(0.0f);
		g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController, Zenith_TransformComponent>()
			.ForEach([&pxFound, &xPosition](Zenith_EntityID,
				ZM_PlayerController& xController, Zenith_TransformComponent& xTransform)
			{
				if (pxFound != nullptr)
				{
					return;
				}
				pxFound = &xController;
				xTransform.GetPosition(xPosition);
			});
		pxOut = pxFound;
		xPositionOut = xPosition;
		return pxFound != nullptr;
	}

	void SetupFixture()
	{
		g_iPhase = 0;
		g_iPhaseFrames = 0;
		g_bPassed = false;
		g_szFailure = "test did not reach verification";
		g_xStartPosition = Zenith_Maths::Vector3(0.0f);
		g_xObservedAxis = Zenith_Maths::Vector2(0.0f);
		g_bObservedInteract = false;
		g_bObservedConfirm  = false;
		g_bObservedCancel   = false;
		g_uDialogueLineBefore = 0u;
		g_uDialogueLineAfter  = 0u;
		g_uProfileAfterBack = Zenith_InputActions::uPROFILE_AUTO;

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);

		g_xPreviousScene = g_xEngine.Scenes().GetActiveScene();
		g_xFixtureScene = g_xEngine.Scenes().LoadScene(
			"ZM_TouchControlsHarness", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(g_xFixtureScene);
		if (pxSceneData == nullptr || !g_xEngine.Scenes().SetActiveScene(g_xFixtureScene))
		{
			Fail("could not create the isolated touch-controls fixture scene");
			return;
		}

		Zenith_Entity xFloor = g_xEngine.Scenes().CreateEntity(pxSceneData, "Floor");
		Zenith_TransformComponent& xFloorTransform =
			xFloor.GetComponent<Zenith_TransformComponent>();
		xFloorTransform.SetPosition({ 0.0f, -0.25f, 0.0f });
		xFloorTransform.SetScale({ 40.0f, 0.5f, 40.0f });
		xFloor.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "Player");
		Zenith_TransformComponent& xPlayerTransform =
			xPlayer.GetComponent<Zenith_TransformComponent>();
		xPlayerTransform.SetPosition({ 0.0f, 0.95f, 0.0f });
		xPlayerTransform.SetScale({ 0.8f, 1.8f, 0.8f });
		xPlayer.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xPlayer.AddComponent<ZM_PlayerController>();

		Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(pxSceneData, "TouchCamera");
		Zenith_CameraComponent& xCameraComponent = xCamera.AddComponent<Zenith_CameraComponent>();
		xCameraComponent.SetPosition({ 0.0f, 3.0f, -5.5f });
		xCameraComponent.SetYaw(0.0);
		xCameraComponent.SetPitch(-0.4);
		Zenith_UnitTests::SetMainCameraForTest(pxSceneData, xCamera.GetEntityID());

		// ★ THE OVERRIDE IS WHAT MAKES THE CONTROLS EXIST AT ALL. A B9 widget is
		// inert and invisible unless the ACTIVE profile carries TOUCH, and a
		// desktop boot resolves P_KEYBOARD. Forcing it also SUSPENDS the auto
		// switch, so a stray key from a neighbouring test cannot silently take the
		// scheme away mid-gesture. The harness restores AUTO between tests.
		g_xEngine.Actions().SetProfileOverride(ZM_Bindings::uPROFILE_TOUCH);
	}

	void TeardownFixture()
	{
		g_xEngine.Actions().ClearOverride();
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_xPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xPreviousScene);
		}
		if (g_xFixtureScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xFixtureScene);
			g_xFixtureScene = Zenith_Scene();
		}
	}

	// True once the fixture has settled AND the HUD has resolved the OVERWORLD
	// context (which is what makes the stick visible and pointed at Move).
	bool OverworldHudReady()
	{
		ZM_PlayerController* pxPlayer = nullptr;
		Zenith_Maths::Vector3 xPosition(0.0f);
		if (!FindPlayer(pxPlayer, xPosition) || !pxPlayer->IsGrounded())
		{
			return false;
		}
		if (ZM_TouchLayoutController::GetLiveContext() != ZM_TOUCH_CONTEXT_OVERWORLD)
		{
			return false;
		}
		Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
		return pxStick != nullptr && pxStick->IsVisible();
	}

	bool VerifyCommon(const char* szTag)
	{
		if (!g_bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[%s] %s", szTag, g_szFailure);
		}
		TeardownFixture();
		return g_bPassed;
	}
}

// -----------------------------------------------------------------------------
// (1) The stick moves the player, diagonals included
// -----------------------------------------------------------------------------

namespace
{
	bool Step_TouchStick(int)
	{
		if (g_iPhase < 0)
		{
			return false;
		}
		++g_iPhaseFrames;

		switch (g_iPhase)
		{
		case 0:   // settle
			if (!OverworldHudReady())
			{
				if (g_iPhaseFrames > 240)
				{
					Fail("the fixture never settled into the OVERWORLD touch context");
					return false;
				}
				return true;
			}
			{
				ZM_PlayerController* pxPlayer = nullptr;
				FindPlayer(pxPlayer, g_xStartPosition);
			}
			{
				// Half a radius out along BOTH axes: a genuine diagonal, and far
				// enough past the deadzone that the axis cannot be a rounding
				// artefact. Canvas -y is forward.
				Zenith_Maths::Vector2 xPoint(0.0f);
				if (!StickTouchPoint(0.5f, -0.5f, xPoint))
				{
					Fail("the on-screen stick is not authored on the persistent HUD");
					return false;
				}
				Zenith_InputSimulator::SimulateTouchDown(0, xPoint.x, xPoint.y);
			}
			g_iPhase = 1;
			g_iPhaseFrames = 0;
			return true;

		case 1:   // hold and observe
			if (g_iPhaseFrames == 1)
			{
				g_xEngine.Actions().GetAxis2D(ZM_Bindings::ZM_ACTION_MOVE, g_xObservedAxis);
				Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
				if (pxStick == nullptr || !pxStick->IsEngaged())
				{
					Fail("the stick did not engage on a touch inside its activation rect");
					return false;
				}
				if (g_xObservedAxis.x <= 0.1f || g_xObservedAxis.y <= 0.1f)
				{
					Fail("a diagonal tilt did not reach MOVE on BOTH axes");
					return false;
				}
			}
			if (g_iPhaseFrames < 60)
			{
				return true;
			}
			{
				ZM_PlayerController* pxPlayer = nullptr;
				Zenith_Maths::Vector3 xPosition(0.0f);
				if (!FindPlayer(pxPlayer, xPosition))
				{
					Fail("the player disappeared mid-gesture");
					return false;
				}
				// Camera yaw 0: axis +x is world +X and axis +y is world +Z.
				if (xPosition.x <= g_xStartPosition.x + 0.5f
					|| xPosition.z <= g_xStartPosition.z + 0.5f)
				{
					Fail("a diagonal stick tilt did not move the player on both world axes");
					return false;
				}
				if (pxPlayer->GetRequestedSpeed() <= 0.0f)
				{
					Fail("the stick drove no requested speed");
					return false;
				}
			}
			Zenith_InputSimulator::SimulateTouchUp(0, 0.0f, 0.0f);
			g_iPhase = 2;
			g_iPhaseFrames = 0;
			return true;

		case 2:   // the release must stop the player
			if (g_iPhaseFrames < 20)
			{
				return true;
			}
			{
				ZM_PlayerController* pxPlayer = nullptr;
				Zenith_Maths::Vector3 xPosition(0.0f);
				FindPlayer(pxPlayer, xPosition);
				Zenith_Maths::Vector2 xAxis(0.0f);
				g_xEngine.Actions().GetAxis2D(ZM_Bindings::ZM_ACTION_MOVE, xAxis);
				if (std::fabs(xAxis.x) > 0.001f || std::fabs(xAxis.y) > 0.001f)
				{
					Fail("lifting the finger left MOVE non-zero");
					return false;
				}
				if (pxPlayer == nullptr || pxPlayer->GetRequestedSpeed() != 0.0f)
				{
					Fail("lifting the finger left the player still requesting speed");
					return false;
				}
			}
			g_bPassed = true;
			g_iPhase = -1;
			return false;

		default:
			return false;
		}
	}

	bool Verify_TouchStick() { return VerifyCommon("ZM_TouchStick"); }
}

static const Zenith_AutomatedTest g_xZMTouchStickTest = {
	"ZM_TouchStick_Test",
	&SetupFixture,
	&Step_TouchStick,
	&Verify_TouchStick,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTouchStickTest);

// -----------------------------------------------------------------------------
// (2) Overworld A = INTERACT, all the way into the interaction runtime
// -----------------------------------------------------------------------------

namespace
{
	bool Step_TouchInteract(int)
	{
		if (g_iPhase < 0)
		{
			return false;
		}
		++g_iPhaseFrames;

		switch (g_iPhase)
		{
		case 0:
			if (!OverworldHudReady())
			{
				if (g_iPhaseFrames > 240)
				{
					Fail("the fixture never settled into the OVERWORLD touch context");
					return false;
				}
				return true;
			}
			{
				// The A button must be pointed at Interact BEFORE the tap -- that
				// is the context machine's whole job in the overworld.
				Zenith_UI::Zenith_UIVirtualButton* pxButtonA =
					ResolveButton(ZM_TouchLayoutController::szBUTTON_A_NAME);
				if (pxButtonA == nullptr || !pxButtonA->IsVisible())
				{
					Fail("the A button is not on screen in the overworld context");
					return false;
				}
				if (pxButtonA->GetActionName() != ZM_Bindings::szACTION_INTERACT)
				{
					Fail("the overworld A button is not targeting Interact");
					return false;
				}
				Zenith_Maths::Vector2 xPoint(0.0f);
				ButtonTouchPoint(ZM_TouchLayoutController::szBUTTON_A_NAME, xPoint);
				Zenith_InputSimulator::SimulateTouchDown(0, xPoint.x, xPoint.y);
			}
			g_iPhase = 1;
			g_iPhaseFrames = 0;
			return true;

		case 1:
			g_bObservedInteract = g_bObservedInteract
				|| g_xEngine.Actions().WasPressedThisFrame(ZM_Bindings::ZM_ACTION_INTERACT);
			g_bObservedConfirm = g_bObservedConfirm
				|| g_xEngine.Actions().WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CONFIRM);
			if (g_iPhaseFrames < 3)
			{
				return true;
			}
			Zenith_InputSimulator::SimulateTouchUp(0, 0.0f, 0.0f);
			g_iPhase = 2;
			g_iPhaseFrames = 0;
			return true;

		case 2:
			if (g_iPhaseFrames < 3)
			{
				return true;
			}
			if (!g_bObservedInteract)
			{
				Fail("a tap on the overworld A button never pressed INTERACT");
				return false;
			}
			// ★ THE OTHER HALF OF THE CONTRACT. One semantics per button per
			// context: the SAME physical button is CONFIRM in a menu, and if the
			// two shared a virtual source a single tap would fire both.
			if (g_bObservedConfirm)
			{
				Fail("the overworld A button also fired CONFIRM -- the two actions share a source");
				return false;
			}
			// ...and the edge reached the GAME, not merely the action layer. The
			// fixture has no interactable, so the runtime rejects -- but it can
			// only reach a reject OTHER than NO_INPUT_EDGE by having seen one.
			// (The latches are process-global; the accessor is instance-shaped, so
			// this reads them through the live player's own runtime.)
			{
				ZM_PlayerController* pxPlayer = nullptr;
				Zenith_Maths::Vector3 xPosition(0.0f);
				if (!FindPlayer(pxPlayer, xPosition) || pxPlayer == nullptr)
				{
					Fail("the player disappeared before the interaction latch was read");
					return false;
				}
				const ZM_InteractionRuntime& xRuntime = pxPlayer->GetInteractionRuntime();
				if (!xRuntime.HasLatchedResult()
					|| xRuntime.GetLastResult() == ZM_INTERACT_REJECT_NO_INPUT_EDGE)
				{
					Fail("the interaction runtime never saw the touch INTERACT edge");
					return false;
				}
			}
			g_bPassed = true;
			g_iPhase = -1;
			return false;

		default:
			return false;
		}
	}

	bool Verify_TouchInteract() { return VerifyCommon("ZM_TouchInteract"); }
}

static const Zenith_AutomatedTest g_xZMTouchInteractTest = {
	"ZM_TouchInteract_Test",
	&SetupFixture,
	&Step_TouchInteract,
	&Verify_TouchInteract,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTouchInteractTest);

// -----------------------------------------------------------------------------
// (3) DIALOGUE context: the SAME button now confirms, and advances the box
// -----------------------------------------------------------------------------

namespace
{
	const char* const aszDIALOGUE_LINES[] =
	{
		"The touch controls retarget on context.",
		"A confirms while a dialogue is up.",
	};

	bool Step_TouchDialogue(int)
	{
		if (g_iPhase < 0)
		{
			return false;
		}
		++g_iPhaseFrames;

		switch (g_iPhase)
		{
		case 0:
			if (!OverworldHudReady())
			{
				if (g_iPhaseFrames > 240)
				{
					Fail("the fixture never settled into the OVERWORLD touch context");
					return false;
				}
				return true;
			}
			if (!ZM_UI_MenuStack::TryPushDialogue(aszDIALOGUE_LINES, 2u))
			{
				Fail("the persistent menu root refused to raise the fixture dialogue");
				return false;
			}
			g_iPhase = 1;
			g_iPhaseFrames = 0;
			return true;

		case 1:   // wait for the context machine to notice and RETARGET
			if (ZM_TouchLayoutController::GetLiveContext() != ZM_TOUCH_CONTEXT_DIALOGUE)
			{
				if (g_iPhaseFrames > 120)
				{
					Fail("raising a dialogue never moved the HUD into the DIALOGUE context");
					return false;
				}
				return true;
			}
			{
				Zenith_UI::Zenith_UIVirtualButton* pxButtonA =
					ResolveButton(ZM_TouchLayoutController::szBUTTON_A_NAME);
				Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
				if (pxButtonA == nullptr || pxStick == nullptr)
				{
					Fail("the HUD widgets vanished when the dialogue opened");
					return false;
				}
				if (pxButtonA->GetActionName() != ZM_Bindings::szACTION_CONFIRM)
				{
					Fail("the dialogue A button was not retargeted to Confirm");
					return false;
				}
				if (pxStick->IsVisible())
				{
					Fail("the stick stayed on screen over a dialogue");
					return false;
				}
				g_uDialogueLineBefore = 0u;
				{
					Zenith_EntityID xMenuID = INVALID_ENTITY_ID;
					if (ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuID))
					{
						Zenith_Entity xMenu = g_xEngine.Scenes().ResolveEntity(xMenuID);
						const ZM_UI_MenuStack* pxMenu = xMenu.IsValid()
							? xMenu.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
						if (pxMenu != nullptr)
						{
							g_uDialogueLineBefore = pxMenu->GetDialogue().GetRemainingLineCount();
						}
					}
				}
				Zenith_Maths::Vector2 xPoint(0.0f);
				ButtonTouchPoint(ZM_TouchLayoutController::szBUTTON_A_NAME, xPoint);
				Zenith_InputSimulator::SimulateTouchDown(0, xPoint.x, xPoint.y);
			}
			g_iPhase = 2;
			g_iPhaseFrames = 0;
			return true;

		case 2:
			g_bObservedConfirm = g_bObservedConfirm
				|| g_xEngine.Actions().WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CONFIRM);
			g_bObservedInteract = g_bObservedInteract
				|| g_xEngine.Actions().WasPressedThisFrame(ZM_Bindings::ZM_ACTION_INTERACT);
			if (g_iPhaseFrames < 3)
			{
				return true;
			}
			Zenith_InputSimulator::SimulateTouchUp(0, 0.0f, 0.0f);
			g_iPhase = 3;
			g_iPhaseFrames = 0;
			return true;

		case 3:
			if (g_iPhaseFrames < 5)
			{
				return true;
			}
			if (!g_bObservedConfirm)
			{
				Fail("a tap on the dialogue A button never pressed CONFIRM");
				return false;
			}
			if (g_bObservedInteract)
			{
				Fail("the dialogue A button still fired INTERACT -- the retarget did not take");
				return false;
			}
			{
				Zenith_EntityID xMenuID = INVALID_ENTITY_ID;
				if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuID))
				{
					Fail("the persistent menu root disappeared mid-dialogue");
					return false;
				}
				Zenith_Entity xMenu = g_xEngine.Scenes().ResolveEntity(xMenuID);
				const ZM_UI_MenuStack* pxMenu = xMenu.IsValid()
					? xMenu.TryGetComponent<ZM_UI_MenuStack>() : nullptr;
				if (pxMenu == nullptr)
				{
					Fail("the menu component disappeared mid-dialogue");
					return false;
				}
				g_uDialogueLineAfter = pxMenu->GetDialogue().GetRemainingLineCount();
				// The box either finished revealing the current line or moved on;
				// either way the CONFIRM reached it, which is what a touch player
				// needs and what a dead retarget would not produce.
				if (g_uDialogueLineAfter >= g_uDialogueLineBefore
					&& pxMenu->GetDialogue().IsActive()
					&& !pxMenu->GetDialogue().IsRevealComplete())
				{
					Fail("the CONFIRM never reached the dialogue box");
					return false;
				}
			}
			g_bPassed = true;
			g_iPhase = -1;
			return false;

		default:
			return false;
		}
	}

	bool Verify_TouchDialogue()
	{
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		return VerifyCommon("ZM_TouchDialogue");
	}
}

static const Zenith_AutomatedTest g_xZMTouchDialogueTest = {
	"ZM_TouchDialogueConfirm_Test",
	&SetupFixture,
	&Step_TouchDialogue,
	&Verify_TouchDialogue,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTouchDialogueTest);

// -----------------------------------------------------------------------------
// (4) Multi-touch independence: stick AND button in the SAME frame
// -----------------------------------------------------------------------------

namespace
{
	bool Step_TouchMultiTouch(int)
	{
		if (g_iPhase < 0)
		{
			return false;
		}
		++g_iPhaseFrames;

		switch (g_iPhase)
		{
		case 0:
			if (!OverworldHudReady())
			{
				if (g_iPhaseFrames > 240)
				{
					Fail("the fixture never settled into the OVERWORLD touch context");
					return false;
				}
				return true;
			}
			{
				// ONE Step, TWO fingers. Both land in the same step-7 injection, so
				// the pointer table hands out two live slots and the two widgets
				// each claim their own -- the property a single-touch table (the
				// deleted Zenith_TouchInput) could not have.
				Zenith_Maths::Vector2 xStickPoint(0.0f);
				Zenith_Maths::Vector2 xButtonPoint(0.0f);
				if (!StickTouchPoint(0.0f, -0.6f, xStickPoint)
					|| !ButtonTouchPoint(ZM_TouchLayoutController::szBUTTON_A_NAME, xButtonPoint))
				{
					Fail("the HUD widgets are not authored on the persistent root");
					return false;
				}
				Zenith_InputSimulator::SimulateTouchDown(0, xStickPoint.x, xStickPoint.y);
				Zenith_InputSimulator::SimulateTouchDown(1, xButtonPoint.x, xButtonPoint.y);
			}
			g_iPhase = 1;
			g_iPhaseFrames = 0;
			return true;

		case 1:
			if (g_iPhaseFrames == 1)
			{
				g_xEngine.Actions().GetAxis2D(ZM_Bindings::ZM_ACTION_MOVE, g_xObservedAxis);
				g_bObservedInteract =
					g_xEngine.Actions().WasPressedThisFrame(ZM_Bindings::ZM_ACTION_INTERACT);

				Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
				Zenith_UI::Zenith_UIVirtualButton* pxButtonA =
					ResolveButton(ZM_TouchLayoutController::szBUTTON_A_NAME);
				if (pxStick == nullptr || !pxStick->IsEngaged())
				{
					Fail("the stick did not engage while a second finger was on a button");
					return false;
				}
				if (pxButtonA == nullptr || !pxButtonA->IsHeld())
				{
					Fail("the A button did not hold while a first finger was on the stick");
					return false;
				}
				if (g_xObservedAxis.y <= 0.1f)
				{
					Fail("the stick's forward tilt did not reach MOVE in the multi-touch frame");
					return false;
				}
				if (!g_bObservedInteract)
				{
					Fail("the second finger's press did not reach INTERACT in the same frame");
					return false;
				}
			}
			if (g_iPhaseFrames < 5)
			{
				return true;
			}
			// Lifting ONE finger must not disturb the other: the claims are
			// per-pointer, not a single "is something down" flag.
			Zenith_InputSimulator::SimulateTouchUp(1, 0.0f, 0.0f);
			g_iPhase = 2;
			g_iPhaseFrames = 0;
			return true;

		case 2:
			if (g_iPhaseFrames < 3)
			{
				return true;
			}
			{
				Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
				Zenith_UI::Zenith_UIVirtualButton* pxButtonA =
					ResolveButton(ZM_TouchLayoutController::szBUTTON_A_NAME);
				if (pxStick == nullptr || !pxStick->IsEngaged())
				{
					Fail("lifting the BUTTON finger also dropped the stick");
					return false;
				}
				if (pxButtonA == nullptr || pxButtonA->IsHeld())
				{
					Fail("the button stayed held after its own finger lifted");
					return false;
				}
				if (!g_xEngine.Actions().IsHeld(ZM_Bindings::ZM_ACTION_MOVE))
				{
					Fail("MOVE was released when the unrelated button finger lifted");
					return false;
				}
			}
			Zenith_InputSimulator::SimulateTouchUp(0, 0.0f, 0.0f);
			g_bPassed = true;
			g_iPhase = -1;
			return false;

		default:
			return false;
		}
	}

	bool Verify_TouchMultiTouch() { return VerifyCommon("ZM_TouchMultiTouch"); }
}

static const Zenith_AutomatedTest g_xZMTouchMultiTouchTest = {
	"ZM_TouchMultiTouch_Test",
	&SetupFixture,
	&Step_TouchMultiTouch,
	&Verify_TouchMultiTouch,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTouchMultiTouchTest);

// -----------------------------------------------------------------------------
// (5) A lifecycle CANCEL neutralises held touch state
// -----------------------------------------------------------------------------

namespace
{
	bool Step_TouchCancel(int)
	{
		if (g_iPhase < 0)
		{
			return false;
		}
		++g_iPhaseFrames;

		switch (g_iPhase)
		{
		case 0:
			if (!OverworldHudReady())
			{
				if (g_iPhaseFrames > 240)
				{
					Fail("the fixture never settled into the OVERWORLD touch context");
					return false;
				}
				return true;
			}
			{
				Zenith_Maths::Vector2 xStickPoint(0.0f);
				Zenith_Maths::Vector2 xButtonPoint(0.0f);
				if (!StickTouchPoint(0.0f, -0.6f, xStickPoint)
					|| !ButtonTouchPoint(ZM_TouchLayoutController::szBUTTON_B_NAME, xButtonPoint))
				{
					Fail("the HUD widgets are not authored on the persistent root");
					return false;
				}
				Zenith_InputSimulator::SimulateTouchDown(0, xStickPoint.x, xStickPoint.y);
				Zenith_InputSimulator::SimulateTouchDown(1, xButtonPoint.x, xButtonPoint.y);
			}
			g_iPhase = 1;
			g_iPhaseFrames = 0;
			return true;

		case 1:
			if (g_iPhaseFrames < 3)
			{
				return true;
			}
			// Both must actually be HELD before the cancel, or the "it let go"
			// assertion below would pass against a gesture that never started.
			{
				Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
				Zenith_UI::Zenith_UIVirtualButton* pxButtonB =
					ResolveButton(ZM_TouchLayoutController::szBUTTON_B_NAME);
				if (pxStick == nullptr || !pxStick->IsEngaged()
					|| pxButtonB == nullptr || !pxButtonB->IsHeld()
					|| !g_xEngine.Actions().IsHeld(ZM_Bindings::ZM_ACTION_MOVE)
					|| !g_xEngine.Actions().IsHeld(ZM_Bindings::ZM_ACTION_RUN))
				{
					Fail("the fixture gesture was not fully held before the cancel");
					return false;
				}
			}
			// The app losing focus / the OS taking the gesture: every live pointer
			// is CANCELLED. Nothing sends an UP, so a widget that only listened for
			// UP would hold its action forever -- a character walking on their own
			// after the phone was unlocked.
			Zenith_InputSimulator::SimulateTouchCancel(0, 0.0f, 0.0f);
			Zenith_InputSimulator::SimulateTouchCancel(1, 0.0f, 0.0f);
			g_iPhase = 2;
			g_iPhaseFrames = 0;
			return true;

		case 2:
			if (g_iPhaseFrames < 5)
			{
				return true;
			}
			{
				Zenith_UI::Zenith_UIVirtualStick* pxStick = ResolveStick();
				Zenith_UI::Zenith_UIVirtualButton* pxButtonB =
					ResolveButton(ZM_TouchLayoutController::szBUTTON_B_NAME);
				Zenith_Maths::Vector2 xAxis(0.0f);
				g_xEngine.Actions().GetAxis2D(ZM_Bindings::ZM_ACTION_MOVE, xAxis);
				if (pxStick == nullptr || pxStick->IsEngaged())
				{
					Fail("a cancelled pointer left the stick engaged");
					return false;
				}
				if (pxButtonB == nullptr || pxButtonB->IsHeld())
				{
					Fail("a cancelled pointer left the B button held");
					return false;
				}
				if (g_xEngine.Actions().IsHeld(ZM_Bindings::ZM_ACTION_MOVE)
					|| g_xEngine.Actions().IsHeld(ZM_Bindings::ZM_ACTION_RUN))
				{
					Fail("a cancelled gesture left its actions held");
					return false;
				}
				if (std::fabs(xAxis.x) > 0.001f || std::fabs(xAxis.y) > 0.001f)
				{
					Fail("a cancelled gesture left a non-zero MOVE axis");
					return false;
				}
				ZM_PlayerController* pxPlayer = nullptr;
				Zenith_Maths::Vector3 xPosition(0.0f);
				FindPlayer(pxPlayer, xPosition);
				if (pxPlayer == nullptr || pxPlayer->GetRequestedSpeed() != 0.0f)
				{
					Fail("a cancelled gesture left the player walking");
					return false;
				}
			}
			g_bPassed = true;
			g_iPhase = -1;
			return false;

		default:
			return false;
		}
	}

	bool Verify_TouchCancel() { return VerifyCommon("ZM_TouchCancel"); }
}

static const Zenith_AutomatedTest g_xZMTouchCancelTest = {
	"ZM_TouchCancelNeutralisesHeld_Test",
	&SetupFixture,
	&Step_TouchCancel,
	&Verify_TouchCancel,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTouchCancelTest);

// -----------------------------------------------------------------------------
// (6) System BACK cancels, and does NOT take the touch profile away
// -----------------------------------------------------------------------------

namespace
{
	// ★ WHY THIS DOES NOT CALL Zenith_Input::SystemBackCallback.
	// That entry point pushes onto the PENDING PLATFORM FIFO, and
	// Zenith_Input::BeginFrame DISCARDS that FIFO outright while the simulator is
	// active ("a sim run replaces the device sources wholesale"). A Step that used
	// it would see the gesture silently swallowed. Appending the event the Step's
	// own way puts it in this frame's ordered transition log -- which is exactly
	// where the platform callback would have delivered it on a real device.
	void InjectSystemBack()
	{
		Zenith_InputEvent xEvent;
		xEvent.m_eType      = INPUT_EVENT_SYSTEM_BACK;
		xEvent.m_bSystemNav = true;
		g_xEngine.Input().AppendInjectedEvent(xEvent);
	}

	bool Step_TouchSystemBack(int)
	{
		if (g_iPhase < 0)
		{
			return false;
		}
		++g_iPhaseFrames;

		switch (g_iPhase)
		{
		case 0:
			if (!OverworldHudReady())
			{
				if (g_iPhaseFrames > 240)
				{
					Fail("the fixture never settled into the OVERWORLD touch context");
					return false;
				}
				return true;
			}
			// ★ THE OVERRIDE COMES OFF FIRST, DELIBERATELY. With a forced profile
			// "the profile stayed TOUCH" is vacuous. Clearing it and letting a real
			// finger WIN the auto-switch is what makes the assertion mean
			// something: the profile is genuinely free to move, and the Back
			// gesture must still not move it.
			g_xEngine.Actions().ClearOverride();
			{
				Zenith_Maths::Vector2 xPoint(0.0f);
				StickTouchPoint(0.0f, -0.6f, xPoint);
				Zenith_InputSimulator::SimulateTouchDown(0, xPoint.x, xPoint.y);
			}
			g_iPhase = 1;
			g_iPhaseFrames = 0;
			return true;

		case 1:
			if (g_iPhaseFrames < 2)
			{
				return true;
			}
			if (g_xEngine.Actions().GetActiveProfile() != ZM_Bindings::uPROFILE_TOUCH)
			{
				Fail("a finger on the glass did not auto-switch the game into P_TOUCH");
				return false;
			}
			Zenith_InputSimulator::SimulateTouchUp(0, 0.0f, 0.0f);
			g_iPhase = 2;
			g_iPhaseFrames = 0;
			return true;

		case 2:
			if (g_iPhaseFrames < 3)
			{
				return true;
			}
			InjectSystemBack();
			g_iPhase = 3;
			g_iPhaseFrames = 0;
			return true;

		case 3:
			g_bObservedCancel = g_bObservedCancel
				|| g_xEngine.Actions().WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CANCEL);
			if (g_uProfileAfterBack == Zenith_InputActions::uPROFILE_AUTO)
			{
				g_uProfileAfterBack = g_xEngine.Actions().GetActiveProfile();
			}
			if (g_iPhaseFrames < 4)
			{
				return true;
			}
			if (!g_bObservedCancel)
			{
				Fail("the system Back gesture never fired CANCEL");
				return false;
			}
			// bSystemNav is excluded from activity detection: the SYSTEM talking is
			// not the player picking up a device, so a Back press on a phone must
			// not hand the game back to the keyboard column.
			if (g_uProfileAfterBack != ZM_Bindings::uPROFILE_TOUCH)
			{
				Fail("the system Back gesture moved the active profile off P_TOUCH");
				return false;
			}
			// It is a PULSE, not a hold: nothing is left down afterwards.
			if (g_xEngine.Actions().IsHeld(ZM_Bindings::ZM_ACTION_CANCEL))
			{
				Fail("the Back gesture left CANCEL held");
				return false;
			}
			g_bPassed = true;
			g_iPhase = -1;
			return false;

		default:
			return false;
		}
	}

	bool Verify_TouchSystemBack() { return VerifyCommon("ZM_TouchSystemBack"); }
}

static const Zenith_AutomatedTest g_xZMTouchSystemBackTest = {
	"ZM_TouchSystemBack_Test",
	&SetupFixture,
	&Step_TouchSystemBack,
	&Verify_TouchSystemBack,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTouchSystemBackTest);

#endif // ZENITH_INPUT_SIMULATOR
