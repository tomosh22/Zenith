#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * CB_SimPad_Test -- the GAMEPAD column of CityBuilder's C2 binding table
 * (CB_Bindings.h), end to end on the real input path. No key, no mouse, no C++
 * shortcut for the input itself: a simulated pad drives PAN (left stick),
 * ORBIT_RATE (right stick), TOOL_NEXT (dpad Right) and PAUSE (Start) through the
 * action layer, and the game reacts exactly as it does to a human's pad.
 *
 * WHY IT IS WORTH A WHOLE TEST. Nothing else in this suite publishes a single
 * pad event -- CB_HumanSession, the only other input-driven test here, is a pure
 * keyboard-and-mouse playthrough -- so every pad row in the table is otherwise
 * unexercised and can regress in total silence.
 *
 *   - THE AUTO PROFILE SWITCH is what makes the pad column work at all. The game
 *     boots into P_DESKTOP, and the first stick deflection past the deadzone is
 *     what makes P_GAMEPAD active; after that the keyboard and mouse rows are
 *     dead and only the pad answers. Every assertion below is meaningless if
 *     that never happens, so it is checked first and reported separately.
 *   - PAN and ORBIT_RATE are the two halves of the RATE story: both are stick
 *     deflections the camera multiplies by dt, and both are asserted through the
 *     REAL consumer (the live CB_CameraController's target and yaw), not as an
 *     axis value read back off the action layer. An axis read would still pass
 *     with CB_CityCameraComponent unplugged.
 *   - TOOL_NEXT is the one row with no keyboard equivalent at all: the number row
 *     selects tools directly, and the pad instead WALKS the toolbar order through
 *     SelectUITool. It is asserted as a real tool change on the live tool system.
 *   - PAUSE proves a row that is bound on BOTH columns (P and Start) answers on
 *     the pad half.
 *
 * ★ IT ALSO PINS THE DOCUMENTED D-PAD COLLISION. dpad Right is bound to this
 * game's TOOL_NEXT *and* to the engine-reserved UI_NAV_RIGHT (ids 0-15). The
 * binding-table header calls that overlap benign because both fire and
 * CityBuilder's HUD is never focus-navigated during play. "Both fire" is a
 * claim, so it is checked here rather than left as folklore: the reserved edge
 * and the game edge are asserted on the same press.
 *
 * requiresGraphics is FALSE: nothing here reads a pixel, so CI sees it.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "CityBuilder/CB_Bindings.h"
#include "CityBuilder/Components/CB_CityCameraComponent.h"
#include "CityBuilder/Components/CB_CityManagerComponent.h"
#include "CityBuilder/Source/CB_CameraController.h"
#include "CityBuilder/Source/CB_ToolSystem.h"

#include <cmath>

namespace
{
	enum class CBPadPhase
	{
		WaitScene, Baseline,
		PanSteer, PanStop,
		OrbitSteer, OrbitStop,
		ToolNextDown, ToolNextEdge, ToolNextSettle,
		PauseDown, PauseEdge,
		Done
	};

	CBPadPhase g_ePadPhase = CBPadPhase::WaitScene;
	int        g_iPadWait  = 0;

	// Baselines captured once the manager + camera are live.
	Zenith_Maths::Vector3 g_xPadPanStart(0.0f);
	float        g_fPadYawStart   = 0.0f;
	CB_ETool     g_ePadToolBefore = CB_TOOL_NONE;
	CB_ESimSpeed g_ePadSpeedBefore = CB_SIM_NORMAL;

	// Observations.
	bool     g_bPadProfileSwitched = false;
	float    g_fPadPanDistance     = 0.0f;
	float    g_fPadYawTravel       = 0.0f;
	bool     g_bPadSawToolNextEdge = false;
	bool     g_bPadSawUINavEdge    = false;
	CB_ETool g_ePadToolAfter       = CB_TOOL_NONE;
	bool     g_bPadSawPauseEdge    = false;
	CB_ESimSpeed g_ePadSpeedAfter  = CB_SIM_NORMAL;
	bool     g_bPadDone            = false;

	// How long each stick phase runs. 60 fixed-dt frames of full deflection is
	// ~240 world units of pan (distance 400 x 0.6/s) and ~1.8 rad of yaw
	// (1.8 rad/s) -- both an order of magnitude over the bars in Verify, so a
	// slow frame or a clamp cannot make this a false red.
	constexpr int iPAD_STICK_FRAMES = 60;

	// The left stick in the PAN row's terms. That row scales (1, -1) because GLFW
	// reports a forward push as -1 and the engine convention is +y FORWARD, so a
	// caller who wants "forward" publishes -1 on the stick's y axis.
	void Pad_PushLeftStick(float fRight, float fForward)
	{
		Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, fRight, -fForward);
	}
	// The right stick in the ORBIT_RATE row's terms -- straight passthrough
	// scales, so +x is stick-right (turn right) and +y is stick-down.
	void Pad_PushRightStick(float fX, float fY)
	{
		Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_RIGHT_X, fX, fY);
	}

	CB_CameraController* Pad_Controller()
	{
		CB_CityCameraComponent* pxCam = CB_CityCameraComponent::GetActive();
		return (pxCam != nullptr) ? &pxCam->GetController() : nullptr;
	}
}

static void Setup_CB_SimPad()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePadPhase = CBPadPhase::WaitScene;
	g_iPadWait  = 0;
	g_xPadPanStart        = Zenith_Maths::Vector3(0.0f);
	g_fPadYawStart        = 0.0f;
	g_ePadToolBefore      = CB_TOOL_NONE;
	g_ePadSpeedBefore     = CB_SIM_NORMAL;
	g_bPadProfileSwitched = false;
	g_fPadPanDistance     = 0.0f;
	g_fPadYawTravel       = 0.0f;
	g_bPadSawToolNextEdge = false;
	g_bPadSawUINavEdge    = false;
	g_ePadToolAfter       = CB_TOOL_NONE;
	g_bPadSawPauseEdge    = false;
	g_ePadSpeedAfter      = CB_SIM_NORMAL;
	g_bPadDone            = false;

	// The harness wiped every simulated device before Setup ran, so the pad has
	// to announce itself again: activity detection skips a disconnected pad, and
	// without the profile switch not one row below resolves.
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
	Pad_PushLeftStick(0.0f, 0.0f);
	Pad_PushRightStick(0.0f, 0.0f);
}

static bool Step_CB_SimPad(int iFrame)
{
	CB_CityManagerComponent* pxMgr = CB_CityManagerComponent::GetActive();
	CB_CameraController*     pxCtl = Pad_Controller();

	switch (g_ePadPhase)
	{
	case CBPadPhase::WaitScene:
		// The camera component publishes itself from its OnUpdate, so a non-null
		// controller also proves the consumer under test is actually ticking.
		if (pxMgr != nullptr && pxCtl != nullptr) { g_ePadPhase = CBPadPhase::Baseline; }
		return iFrame < 200;

	case CBPadPhase::Baseline:
		if (pxMgr == nullptr || pxCtl == nullptr) { return false; }
		g_xPadPanStart    = pxCtl->m_xTarget;
		g_fPadYawStart    = pxCtl->m_fYaw;
		g_ePadToolBefore  = pxMgr->GetTools().GetTool();
		g_ePadSpeedBefore = pxMgr->GetSpeed();
		g_iPadWait  = 0;
		g_ePadPhase = CBPadPhase::PanSteer;
		return true;

	case CBPadPhase::PanSteer:
	{
		if (g_xEngine.Actions().GetActiveProfile() == CB_Bindings::uPROFILE_GAMEPAD)
		{
			g_bPadProfileSwitched = true;
		}
		Pad_PushLeftStick(0.0f, 1.0f);
		if (pxCtl != nullptr)
		{
			const float fDx = pxCtl->m_xTarget.x - g_xPadPanStart.x;
			const float fDz = pxCtl->m_xTarget.z - g_xPadPanStart.z;
			const float fMoved = std::sqrt(fDx * fDx + fDz * fDz);
			if (fMoved > g_fPadPanDistance) { g_fPadPanDistance = fMoved; }
		}
		if (++g_iPadWait >= iPAD_STICK_FRAMES) { g_ePadPhase = CBPadPhase::PanStop; }
		return true;
	}

	case CBPadPhase::PanStop:
		// Zero the left stick BEFORE the orbit phase so the two measurements
		// cannot borrow from each other (a moving target does not change yaw, but
		// a leaking stick would make the failure message lie about which row broke).
		Pad_PushLeftStick(0.0f, 0.0f);
		if (pxCtl != nullptr) { g_fPadYawStart = pxCtl->m_fYaw; }
		g_iPadWait  = 0;
		g_ePadPhase = CBPadPhase::OrbitSteer;
		return true;

	case CBPadPhase::OrbitSteer:
	{
		Pad_PushRightStick(1.0f, 0.0f);
		if (pxCtl != nullptr)
		{
			const float fTravel = std::fabs(pxCtl->m_fYaw - g_fPadYawStart);
			if (fTravel > g_fPadYawTravel) { g_fPadYawTravel = fTravel; }
		}
		if (++g_iPadWait >= iPAD_STICK_FRAMES) { g_ePadPhase = CBPadPhase::OrbitStop; }
		return true;
	}

	case CBPadPhase::OrbitStop:
		Pad_PushRightStick(0.0f, 0.0f);
		g_ePadPhase = CBPadPhase::ToolNextDown;
		return true;

	case CBPadPhase::ToolNextDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT);
		g_ePadPhase = CBPadPhase::ToolNextEdge;
		return true;

	case CBPadPhase::ToolNextEdge:
		// ★ C1b -- this reads the edge raised by the PREVIOUS Step's injection. A
		// Step runs at PumpAutomatedTest, before this frame's step 7/8, so what it
		// sees on the action layer is the state closed at 10e LAST frame (which is
		// also the frame the manager consumed it in, at step 11).
		if (g_xEngine.Actions().WasPressedThisFrame(CB_Bindings::CB_ACTION_TOOL_NEXT))
		{
			g_bPadSawToolNextEdge = true;
		}
		// The same press on the engine-reserved row -- the documented collision.
		if (g_xEngine.Actions().WasPressedThisFrame(INPUT_ACTION_UI_NAV_RIGHT))
		{
			g_bPadSawUINavEdge = true;
		}
		if (pxMgr != nullptr) { g_ePadToolAfter = pxMgr->GetTools().GetTool(); }
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT);
		g_iPadWait  = 0;
		g_ePadPhase = CBPadPhase::ToolNextSettle;
		return true;

	case CBPadPhase::ToolNextSettle:
		// Let the release close before the next button, so the two presses cannot
		// share a frame on the action layer.
		if (++g_iPadWait < 3) { return true; }
		g_ePadPhase = CBPadPhase::PauseDown;
		return true;

	case CBPadPhase::PauseDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_START);
		g_ePadPhase = CBPadPhase::PauseEdge;
		return true;

	case CBPadPhase::PauseEdge:
		// C1b again.
		if (g_xEngine.Actions().WasPressedThisFrame(CB_Bindings::CB_ACTION_PAUSE))
		{
			g_bPadSawPauseEdge = true;
		}
		if (pxMgr != nullptr) { g_ePadSpeedAfter = pxMgr->GetSpeed(); }
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_START);
		g_bPadDone  = true;
		g_ePadPhase = CBPadPhase::Done;
		return false;

	case CBPadPhase::Done:
	default:
		return false;
	}
}

static bool Verify_CB_SimPad()
{
	Zenith_InputSimulator::ClearFixedDt();
	Pad_PushLeftStick(0.0f, 0.0f);
	Pad_PushRightStick(0.0f, 0.0f);

	// Leave the live city as this test found it: the batch reloads scene 0 between
	// tests, but a paused clock or a stray tool is exactly the kind of residue that
	// turns into a mystery failure somewhere else if that ever changes.
	if (CB_CityManagerComponent* pxMgr = CB_CityManagerComponent::GetActive())
	{
		pxMgr->SetUISpeed(g_ePadSpeedBefore);
		pxMgr->SelectUITool(g_ePadToolBefore, CB_BUILDING_NONE);
	}

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_SimPad: profileSwitched=%d panMetres=%.1f yawRadians=%.3f toolBefore=%d toolAfter=%d "
		"toolNextEdge=%d uiNavEdge=%d pauseEdge=%d speedBefore=%d speedAfter=%d",
		g_bPadProfileSwitched, g_fPadPanDistance, g_fPadYawTravel,
		static_cast<int>(g_ePadToolBefore), static_cast<int>(g_ePadToolAfter),
		g_bPadSawToolNextEdge, g_bPadSawUINavEdge, g_bPadSawPauseEdge,
		static_cast<int>(g_ePadSpeedBefore), static_cast<int>(g_ePadSpeedAfter));

	if (!g_bPadDone)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: never completed (phase %d) -- the City scene never produced a live "
			"CityManager AND a ticking CB_CityCameraComponent",
			static_cast<int>(g_ePadPhase));
		return false;
	}
	if (!g_bPadProfileSwitched)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: the stick never switched the active profile to P_GAMEPAD (now %u) -- pad "
			"rows resolve only while P_GAMEPAD is active, so nothing below would mean anything",
			static_cast<u_int32>(g_xEngine.Actions().GetActiveProfile()));
		return false;
	}
	if (g_fPadPanDistance < 5.0f)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: the camera target moved %.2f units on the left stick -- PAN's stick row, "
			"its y inversion, or CB_CityCameraComponent's read of it regressed", g_fPadPanDistance);
		return false;
	}
	if (g_fPadYawTravel < 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: the orbit yaw moved %.4f rad on the right stick -- ORBIT_RATE's stick row "
			"or the camera's rate-times-dt application regressed", g_fPadYawTravel);
		return false;
	}
	if (!g_bPadSawToolNextEdge)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: dpad Right raised no TOOL_NEXT press edge -- the pad row or the 10e close "
			"regressed");
		return false;
	}
	if (!g_bPadSawUINavEdge)
	{
		// A DELIBERATE pin on the documented overlap, not a gameplay check: if the
		// reserved row stopped answering, the binding-table header's claim that
		// both fire (and therefore that the collision is understood) is stale.
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: dpad Right raised TOOL_NEXT but NOT the engine-reserved UI_NAV_RIGHT -- "
			"CB_Bindings.h documents that both fire; re-check that note before changing this test");
		return false;
	}
	if (g_ePadToolAfter == g_ePadToolBefore)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: TOOL_NEXT edged but the active tool stayed %d -- CycleUITool no longer "
			"walks the toolbar order through SelectUITool", static_cast<int>(g_ePadToolBefore));
		return false;
	}
	if (!g_bPadSawPauseEdge)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: pad Start raised no PAUSE press edge -- the pad half of a row that is "
			"bound on BOTH columns regressed");
		return false;
	}
	if (g_ePadSpeedAfter != CB_SIM_PAUSED)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"CB_SimPad FAIL: PAUSE edged but the sim clock is %d, not PAUSED(%d) -- the manager no "
			"longer consumes the action", static_cast<int>(g_ePadSpeedAfter), static_cast<int>(CB_SIM_PAUSED));
		return false;
	}

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_SimPad: pad drove PAN (%.1f units on the left stick), ORBIT_RATE (%.3f rad on the right "
		"stick), TOOL_NEXT (dpad Right -> tool %d) and PAUSE (Start) after auto-switching to P_GAMEPAD",
		g_fPadPanDistance, g_fPadYawTravel, static_cast<int>(g_ePadToolAfter));
	return true;
}

static const Zenith_AutomatedTest g_xCBSimPadTest = {
	"CB_SimPad_Test",
	&Setup_CB_SimPad,
	&Step_CB_SimPad,
	&Verify_CB_SimPad,
	/*maxFrames*/ 600,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCBSimPadTest);

#endif // ZENITH_INPUT_SIMULATOR
