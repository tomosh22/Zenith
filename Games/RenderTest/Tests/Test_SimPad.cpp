#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * RT_SimPad_Test -- the GAMEPAD column of RenderTest's C2 binding table
 * (RenderTest_Bindings.h), end to end on the real input path. No key, no mouse,
 * no C++ shortcut: a simulated pad drives MOVE (left stick), INTERACT (pad X),
 * FIRE (right trigger) and JUMP (pad A) through the action layer, and the game
 * reacts exactly as it does to a human's pad.
 *
 * WHY IT IS WORTH A WHOLE TEST. Three of the four rows below cannot be reached
 * by any other test in this game, and each one can regress silently:
 *
 *   - THE AUTO PROFILE SWITCH is what makes the pad column work at all. The
 *     game boots into P_DESKTOP, and the first stick deflection past the
 *     deadzone is what makes P_GAMEPAD active -- after which the keyboard and
 *     mouse rows are dead and only the pad answers. Every assertion below is
 *     meaningless if that never happens, so it is checked first and reported
 *     separately.
 *   - INTERACT and FIRE are consumed by RenderTest_PlayerActions.bgraph through
 *     OnActionPressed nodes that resolve an action NAME STRING. A rename on
 *     either side of that contract leaves the node inert with one logged error
 *     and no compile-time signal; this test is what turns that into a red.
 *   - FIRE's pad row is a GAMEPAD_AXIS_AS_BUTTON with 0.55/0.45 hysteresis, a
 *     latch that exists only in the binding table. Pulling the trigger to 1.0
 *     and releasing to 0.0 exercises both edges of it.
 *
 * JUMP is asserted BOTH ways deliberately: the action edge (scene-independent,
 * so it separates "the pad row broke" from "the player was not grounded") and
 * the physical rise, which is the only proof the press reached the consumer.
 *
 * requiresGraphics is FALSE: nothing here reads a pixel, so CI sees it.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

#include "RenderTest/RenderTest_Bindings.h"
#include "RenderTest/Components/RenderTest_PlayerComponent.h"

#include <cmath>

namespace
{
	enum class PadPhase
	{
		Boot, WaitReady, Steer,
		InteractDown, InteractUp, AssertEquipped,
		FirePull, FireEdge, AssertFired,
		JumpSettle, JumpDown, JumpEdge, JumpObserve,
		Done
	};

	PadPhase g_ePadPhase = PadPhase::Boot;
	int      g_iPadFrame = 0;
	int      g_iPadJumpRetries = 0;

	Zenith_Maths::Vector3 g_xPadStartPos(0.0f);
	float    g_fPadDisplacement = 0.0f;
	float    g_fPadJumpBaseY = 0.0f;
	float    g_fPadJumpRise = 0.0f;
	uint32_t g_uPadClipBeforeFire = 0;

	bool g_bPadProfileSwitched = false;
	bool g_bPadEquipped        = false;
	bool g_bPadSawFireEdge     = false;
	bool g_bPadFired           = false;
	bool g_bPadSawJumpEdge     = false;
	bool g_bPadJumped          = false;
	bool g_bPadDone            = false;

	Zenith_Entity Pad_FindEntity(const char* szName)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		if (!xScene.IsValid()) return Zenith_Entity();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) return Zenith_Entity();
		return pxSceneData->FindEntityByName(szName);
	}

	RenderTest_PlayerComponent* Pad_Player()
	{
		Zenith_Entity xPlayer = Pad_FindEntity("Player");
		if (!xPlayer.IsValid()) return nullptr;
		return xPlayer.TryGetComponent<RenderTest_PlayerComponent>();
	}

	bool Pad_GetEntityPos(const char* szName, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = Pad_FindEntity(szName);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	// The left stick, in the MOVE row's terms. That row scales (1, -1) because
	// GLFW reports a forward push as -1 and the engine convention is +y FORWARD,
	// so a caller who wants "+z in world" publishes -1 on the stick's y axis.
	void Pad_PushStick(float fWorldRight, float fWorldForward)
	{
		Zenith_InputSimulator::SimulateGamepadStick(
			ZENITH_GAMEPAD_AXIS_LEFT_X, fWorldRight, -fWorldForward);
	}

	// -1 / 0 / +1 with the same 0.15 m dead band RT_PlayerActions steers on.
	float Pad_SteerAxis(float fDelta)
	{
		if (fDelta >  0.15f) return  1.0f;
		if (fDelta < -0.15f) return -1.0f;
		return 0.0f;
	}
}

static void Setup_SimPad()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePadPhase = PadPhase::Boot;
	g_iPadFrame = 0;
	g_iPadJumpRetries = 0;
	g_xPadStartPos = Zenith_Maths::Vector3(0.0f);
	g_fPadDisplacement = 0.0f;
	g_fPadJumpBaseY = 0.0f;
	g_fPadJumpRise = 0.0f;
	g_uPadClipBeforeFire = 0;
	g_bPadProfileSwitched = false;
	g_bPadEquipped = false;
	g_bPadSawFireEdge = false;
	g_bPadFired = false;
	g_bPadSawJumpEdge = false;
	g_bPadJumped = false;
	g_bPadDone = false;

	// The harness wiped every simulated device before Setup ran, so the pad has
	// to announce itself again: activity detection skips a disconnected pad, and
	// without the profile switch not one row below resolves.
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
}

static bool Step_SimPad(int iFrame)
{
	switch (g_ePadPhase)
	{
	case PadPhase::Boot:
		// The boot scene is already simulating on variable dt when Setup runs;
		// reload it now that the fixed dt is engaged, exactly as the sibling
		// RenderTest characterizations do.
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_ePadPhase = PadPhase::WaitReady;
		return true;

	case PadPhase::WaitReady:
		if (Pad_Player() != nullptr && Pad_FindEntity("Gun_Pistol").IsValid()
			&& Pad_GetEntityPos("Player", g_xPadStartPos))
		{
			g_ePadPhase = PadPhase::Steer;
		}
		return iFrame < 900;

	case PadPhase::Steer:
	{
		// Steering to the pistol proves MOVE and leaves the player in range for
		// INTERACT, so one phase buys two rows. No mouse input ever arrives, so
		// the camera yaw is still 0 and camera-relative forward is world +Z.
		if (g_xEngine.Actions().GetActiveProfile() == RenderTest_Bindings::uPROFILE_GAMEPAD)
		{
			g_bPadProfileSwitched = true;
		}

		Zenith_Maths::Vector3 xPlayerPos, xGunPos;
		if (!Pad_GetEntityPos("Player", xPlayerPos)) return false;
		if (!Pad_GetEntityPos("Gun_Pistol", xGunPos)) return false;

		const float fDxTravelled = xPlayerPos.x - g_xPadStartPos.x;
		const float fDzTravelled = xPlayerPos.z - g_xPadStartPos.z;
		const float fMoved = std::sqrt(fDxTravelled * fDxTravelled + fDzTravelled * fDzTravelled);
		if (fMoved > g_fPadDisplacement)
		{
			g_fPadDisplacement = fMoved;
		}

		const float fDx = xGunPos.x - xPlayerPos.x;
		const float fDz = xGunPos.z - xPlayerPos.z;
		if (std::sqrt(fDx * fDx + fDz * fDz) <= 1.7f)   // inside the 2.2 pickup radius
		{
			Pad_PushStick(0.0f, 0.0f);
			g_ePadPhase = PadPhase::InteractDown;
			return true;
		}
		Pad_PushStick(Pad_SteerAxis(fDx), Pad_SteerAxis(fDz));
		return iFrame < 2400;
	}

	case PadPhase::InteractDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_X);
		g_ePadPhase = PadPhase::InteractUp;
		return true;

	case PadPhase::InteractUp:
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_X);
		g_iPadFrame = 0;
		g_ePadPhase = PadPhase::AssertEquipped;
		return true;

	case PadPhase::AssertEquipped:
	{
		RenderTest_PlayerComponent* pxPlayer = Pad_Player();
		if (pxPlayer != nullptr && pxPlayer->IsHoldingGun())
		{
			g_bPadEquipped = true;
			g_uPadClipBeforeFire = pxPlayer->GetAmmoInClip();
			g_ePadPhase = PadPhase::FirePull;
			return true;
		}
		return ++g_iPadFrame < 40;
	}

	case PadPhase::FirePull:
		// Past 0.55 -> the hysteresis latch closes at this frame's 10e and the
		// graph's OnActionPressed(Fire) fires later in the SAME frame.
		Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 1.0f);
		g_ePadPhase = PadPhase::FireEdge;
		return true;

	case PadPhase::FireEdge:
		// ★ C1b -- this reads the edge raised by the PREVIOUS Step's injection.
		// A Step runs at PumpAutomatedTest, before this frame's step 7/8, so
		// what it sees on the action layer is the state closed at 10e last
		// frame. Reading it in the pulling Step would always be false.
		if (g_xEngine.Actions().WasPressedThisFrame(RenderTest_Bindings::RENDERTEST_ACTION_FIRE))
		{
			g_bPadSawFireEdge = true;
		}
		// Below 0.45 -> the other side of the hysteresis band.
		Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.0f);
		g_iPadFrame = 0;
		g_ePadPhase = PadPhase::AssertFired;
		return true;

	case PadPhase::AssertFired:
	{
		RenderTest_PlayerComponent* pxPlayer = Pad_Player();
		if (pxPlayer != nullptr && pxPlayer->GetAmmoInClip() == g_uPadClipBeforeFire - 1u)
		{
			g_bPadFired = true;
			g_iPadFrame = 0;
			g_ePadPhase = PadPhase::JumpSettle;
			return true;
		}
		return ++g_iPadFrame < 60;
	}

	case PadPhase::JumpSettle:
	{
		// Let the walk bleed off so the grounded raycast is settled before the
		// press, and re-baseline the height each retry.
		if (++g_iPadFrame < 20)
		{
			return true;
		}
		Zenith_Maths::Vector3 xSettled;
		if (!Pad_GetEntityPos("Player", xSettled))
		{
			return false;
		}
		g_fPadJumpBaseY = xSettled.y;
		g_fPadJumpRise = 0.0f;
		g_ePadPhase = PadPhase::JumpDown;
		return true;
	}

	case PadPhase::JumpDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A);
		g_ePadPhase = PadPhase::JumpEdge;
		return true;

	case PadPhase::JumpEdge:
		// C1b again: last Step's press, closed at last frame's 10e.
		if (g_xEngine.Actions().WasPressedThisFrame(RenderTest_Bindings::RENDERTEST_ACTION_JUMP))
		{
			g_bPadSawJumpEdge = true;
		}
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_A);
		g_iPadFrame = 0;
		g_ePadPhase = PadPhase::JumpObserve;
		return true;

	case PadPhase::JumpObserve:
	{
		Zenith_Maths::Vector3 xNow;
		if (Pad_GetEntityPos("Player", xNow))
		{
			const float fRise = xNow.y - g_fPadJumpBaseY;
			if (fRise > g_fPadJumpRise)
			{
				g_fPadJumpRise = fRise;
			}
		}
		if (g_fPadJumpRise > 0.25f)
		{
			g_bPadJumped = true;
			g_bPadDone = true;
			g_ePadPhase = PadPhase::Done;
			return false;
		}
		if (++g_iPadFrame > 90)
		{
			// A press that lands on a non-grounded frame is consumed with no
			// pop; retry from a settled state a couple of times before calling
			// it broken. The ACTION edge is already recorded either way, so the
			// two failure modes stay distinguishable in Verify.
			if (++g_iPadJumpRetries > 2)
			{
				g_bPadDone = true;
				g_ePadPhase = PadPhase::Done;
				return false;
			}
			g_iPadFrame = 0;
			g_ePadPhase = PadPhase::JumpSettle;
		}
		return iFrame < 3400;
	}

	case PadPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_SimPad()
{
	Zenith_InputSimulator::ClearFixedDt();
	Pad_PushStick(0.0f, 0.0f);

	if (!g_bPadDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_SimPad] never completed (phase %d)",
			static_cast<int>(g_ePadPhase));
		return false;
	}
	if (!g_bPadProfileSwitched)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] the stick never switched the active profile to P_GAMEPAD (now %u) -- pad rows "
			"resolve only while P_GAMEPAD is active, so nothing below would mean anything",
			static_cast<u_int32>(g_xEngine.Actions().GetActiveProfile()));
		return false;
	}
	if (g_fPadDisplacement < 0.5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] the player moved %.2f m on the left stick -- the MOVE row's stick binding or "
			"its y inversion regressed", g_fPadDisplacement);
		return false;
	}
	if (!g_bPadEquipped)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] pad X never equipped the pistol -- either the INTERACT row lost its pad button "
			"or the PlayerActions graph's OnActionPressed(\"%s\") node no longer resolves",
			RenderTest_Bindings::szACTION_INTERACT);
		return false;
	}
	if (!g_bPadSawFireEdge)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] pulling the right trigger to 1.0 raised no FIRE press edge -- the "
			"GAMEPAD_AXIS_AS_BUTTON row's %.2f/%.2f hysteresis latch regressed",
			RenderTest_Bindings::fTRIGGER_PRESS_AT, RenderTest_Bindings::fTRIGGER_RELEASE_AT);
		return false;
	}
	if (!g_bPadFired)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] the FIRE edge never decremented the clip -- the edge reached the action layer, "
			"so the break is between it and RTPlayerTryFire (graph node name or OnActionPressed(\"%s\"))",
			RenderTest_Bindings::szACTION_FIRE);
		return false;
	}
	if (!g_bPadSawJumpEdge)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] pad A raised no JUMP press edge -- the pad row or the 10e close regressed");
		return false;
	}
	if (!g_bPadJumped)
	{
		// Distinct from the check above on purpose: the edge existing but the
		// player not rising means the press never reached the consumer (or the
		// player was never grounded), not that the binding broke.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[RT_SimPad] pad A raised a JUMP edge but the player rose only %.2f m over %d attempts -- "
			"the edge reached the action layer, so look at the OnUpdate jump block or the grounded check",
			g_fPadJumpRise, g_iPadJumpRetries + 1);
		return false;
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[RT_SimPad] pad drove MOVE (%.2f m), INTERACT (pistol equipped), FIRE (trigger hysteresis -> "
		"clip %u->%u) and JUMP (rose %.2f m)",
		g_fPadDisplacement, g_uPadClipBeforeFire, g_uPadClipBeforeFire - 1u, g_fPadJumpRise);
	return true;
}

static const Zenith_AutomatedTest g_xRenderTestSimPadTest = {
	"RT_SimPad_Test",
	&Setup_SimPad,
	&Step_SimPad,
	&Verify_SimPad,
	/*maxFrames*/ 3600,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRenderTestSimPadTest);

#endif // ZENITH_INPUT_SIMULATOR
