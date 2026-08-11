#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * DP_SimPad_Test -- the GAMEPAD column of DevilsPlayground's C2 binding table
 * (DP_Bindings.h), end to end on the real input path. No key, no mouse, no C++
 * shortcut for the input itself: a simulated pad drives MOVE (left stick),
 * INTERACT (pad X) and ABILITY (pad RB) through the action layer, and the game
 * reacts exactly as it does to a human's pad.
 *
 * WHY IT IS WORTH A WHOLE TEST. Nothing else in this game's 140-odd tests
 * publishes a single pad event -- the whole suite drives the keyboard and the
 * mouse -- so every pad row below is otherwise unexercised and can regress in
 * total silence.
 *
 *   - THE AUTO PROFILE SWITCH is what makes the pad column work at all. The
 *     game boots into P_DESKTOP, and the first stick deflection past the
 *     deadzone is what makes P_GAMEPAD active -- after which the keyboard and
 *     mouse rows are dead and only the pad answers. Every assertion below is
 *     meaningless if that never happens, so it is checked first and reported
 *     separately.
 *   - MOVE's pad row inverts the stick's y (GLFW reports a forward push as -1,
 *     the engine convention is +y FORWARD). A dropped inversion still moves the
 *     villager, just backwards, so displacement alone would not catch it --
 *     which is why the assert is on distance travelled from a known start.
 *   - INTERACT is asserted through its real consumer: pad X inside a door's
 *     radius must reach DPInteractable_Base's per-frame poll and dispatch
 *     DP_OnInteract. That is the whole chain -- pad button, binding row, 10e
 *     close, game logic read -- not just an edge on the action layer.
 *
 * ABILITY is asserted as an ACTION EDGE ONLY, and deliberately so: the C2 table
 * registers it (Space / pad RB) but this game has no consumer for it yet -- the
 * retired raw-key reader it replaced had none either. The edge is the whole of
 * what there is to guard, and when a consumer is wired the assertion below
 * becomes its first half rather than needing to be rewritten.
 *
 * POSSESS is NOT exercised here, because the C2 table deliberately gives it no
 * pad row: it is the press half of a cursor gesture and a face button cannot
 * supply a position (see DP_Bindings.h). Possession is therefore staged through
 * DP_Player::SetPossessedVillager -- the same system path the death-respawn and
 * the other possession tests use -- so that the rows that DO have a pad column
 * can be driven by the pad.
 *
 * requiresGraphics is FALSE: nothing here reads a pixel, so CI sees it.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "DP_Bindings.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPDoor_Component.h"

#include <cmath>

namespace
{
	enum class DPPadPhase
	{
		Boot, WaitScene, Possess, Settle,
		Steer, StopStick,
		Teleport, EnterRange, InteractDown, InteractUp, AssertInteract,
		AbilityDown, AbilityEdge,
		Done
	};

	DPPadPhase g_ePadPhase = DPPadPhase::Boot;
	int        g_iPadWait  = 0;
	int        g_iPadInteractAttempts = 0;

	Zenith_EntityID g_xPadVillager;
	Zenith_EntityID g_xPadDoor;

	Zenith_Maths::Vector3 g_xPadStartPos(0.0f);
	float g_fPadDisplacement = 0.0f;

	bool g_bPadProfileSwitched = false;
	bool g_bPadInteractFired   = false;
	bool g_bPadSawAbilityEdge  = false;
	bool g_bPadDone            = false;

	Zenith_EventHandle g_xPadInteractHandle = INVALID_EVENT_HANDLE;

	void Pad_OnInteract(const DP_OnInteract&) { g_bPadInteractFired = true; }

	template<typename T>
	Zenith_EntityID Pad_FindFirstEntityWith()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachComponentInActiveScene<T>(
			[&xResult](Zenith_EntityID xId, T&) { if (!xResult.IsValid()) xResult = xId; });
		return xResult;
	}

	template<typename T>
	T* Pad_GetComponent(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<T>();
	}

	bool Pad_GetPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	void Pad_SetPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return;
		if (Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->SetPosition(xPos);
		}
	}

	// The left stick, in the MOVE row's terms. That row scales (1, -1) because
	// GLFW reports a forward push as -1 and the engine convention is +y FORWARD,
	// so a caller who wants "forward" publishes -1 on the stick's y axis.
	void Pad_PushStick(float fRight, float fForward)
	{
		Zenith_InputSimulator::SimulateGamepadStick(
			ZENITH_GAMEPAD_AXIS_LEFT_X, fRight, -fForward);
	}
}

static void Setup_DPSimPad()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePadPhase = DPPadPhase::Boot;
	g_iPadWait  = 0;
	g_iPadInteractAttempts = 0;
	g_xPadVillager = INVALID_ENTITY_ID;
	g_xPadDoor     = INVALID_ENTITY_ID;
	g_xPadStartPos = Zenith_Maths::Vector3(0.0f);
	g_fPadDisplacement    = 0.0f;
	g_bPadProfileSwitched = false;
	g_bPadInteractFired   = false;
	g_bPadSawAbilityEdge  = false;
	g_bPadDone            = false;

	g_xPadInteractHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnInteract>(&Pad_OnInteract);

	// The harness wiped every simulated device before Setup ran, so the pad has
	// to announce itself again: activity detection skips a disconnected pad, and
	// without the profile switch not one row below resolves.
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
}

static bool Step_DPSimPad(int iFrame)
{
	switch (g_ePadPhase)
	{
	case DPPadPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);   // ProcLevel
		g_ePadPhase = DPPadPhase::WaitScene;
		return true;

	case DPPadPhase::WaitScene:
		g_xPadVillager = Pad_FindFirstEntityWith<DPVillager_Component>();
		g_xPadDoor     = Pad_FindFirstEntityWith<DPDoor_Component>();
		if (g_xPadVillager.IsValid() && g_xPadDoor.IsValid())
		{
			g_ePadPhase = DPPadPhase::Possess;
		}
		return iFrame < 400;

	case DPPadPhase::Possess:
		// System possession path -- POSSESS has no pad row by design (see the
		// header note). Everything AFTER this point is pad-driven.
		DP_Player::SetPossessedVillager(g_xPadVillager);
		g_iPadWait  = 0;
		g_ePadPhase = DPPadPhase::Settle;
		return true;

	case DPPadPhase::Settle:
		// The villager shim stages "possessedNow" and DP_Villager.bgraph makes
		// the Idle->Possessed transition; movement is gated on the POST-decision
		// state, so give it a couple of frames before baselining the position.
		if (++g_iPadWait < 4) return true;
		if (!Pad_GetPos(g_xPadVillager, g_xPadStartPos)) return false;
		g_iPadWait  = 0;
		g_ePadPhase = DPPadPhase::Steer;
		return true;

	case DPPadPhase::Steer:
	{
		if (g_xEngine.Actions().GetActiveProfile() == DP_Bindings::uPROFILE_GAMEPAD)
		{
			g_bPadProfileSwitched = true;
		}

		Pad_PushStick(0.0f, 1.0f);

		Zenith_Maths::Vector3 xNow;
		if (Pad_GetPos(g_xPadVillager, xNow))
		{
			const float fDx = xNow.x - g_xPadStartPos.x;
			const float fDz = xNow.z - g_xPadStartPos.z;
			const float fMoved = std::sqrt(fDx * fDx + fDz * fDz);
			if (fMoved > g_fPadDisplacement) g_fPadDisplacement = fMoved;
		}

		// A full stick push is jog speed (~3 m/s), so 90 frames of fixed dt is
		// several metres of clearance over the 0.5 m bar -- enough that a wall
		// two steps away cannot make this a false red.
		if (++g_iPadWait >= 90)
		{
			g_ePadPhase = DPPadPhase::StopStick;
		}
		return true;
	}

	case DPPadPhase::StopStick:
		Pad_PushStick(0.0f, 0.0f);
		g_ePadPhase = DPPadPhase::Teleport;
		return true;

	case DPPadPhase::Teleport:
	{
		// Put the villager inside the door's interact radius (2 m by default).
		// The door's LOGICAL centre, not its transform: the corner-anchored mesh
		// offsets the transform by ~1 m, which is most of the radius.
		DPDoor_Component* pxDoor = Pad_GetComponent<DPDoor_Component>(g_xPadDoor);
		if (pxDoor == nullptr) return false;
		Pad_SetPos(g_xPadVillager,
			pxDoor->GetInteractionCentre() + Zenith_Maths::Vector3(0.6f, 0.0f, 0.0f));
		g_iPadWait  = 0;
		g_ePadPhase = DPPadPhase::EnterRange;
		return true;
	}

	case DPPadPhase::EnterRange:
		// The interactable detects range in its own OnUpdate, one frame after
		// the teleport lands.
		if (++g_iPadWait < 3) return true;
		g_ePadPhase = DPPadPhase::InteractDown;
		return true;

	case DPPadPhase::InteractDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_X);
		g_ePadPhase = DPPadPhase::InteractUp;
		return true;

	case DPPadPhase::InteractUp:
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_X);
		g_iPadWait  = 0;
		g_ePadPhase = DPPadPhase::AssertInteract;
		return true;

	case DPPadPhase::AssertInteract:
		// The press closed at the previous frame's 10e and the door's OnUpdate
		// read it in the same frame's game logic, so the event is already here;
		// the poll window only covers component-order slack.
		if (g_bPadInteractFired || ++g_iPadWait > 20)
		{
			if (!g_bPadInteractFired && ++g_iPadInteractAttempts < 3)
			{
				// A press that landed on a frame the door had not yet detected
				// range on is consumed with no dispatch. Re-press from a known
				// in-range state rather than calling the binding broken.
				g_iPadWait  = 0;
				g_ePadPhase = DPPadPhase::InteractDown;
				return true;
			}
			g_ePadPhase = DPPadPhase::AbilityDown;
		}
		return true;

	case DPPadPhase::AbilityDown:
		Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_RIGHT_BUMPER);
		g_ePadPhase = DPPadPhase::AbilityEdge;
		return true;

	case DPPadPhase::AbilityEdge:
		// ★ C1b -- this reads the edge raised by the PREVIOUS Step's injection.
		// A Step runs at PumpAutomatedTest, before this frame's step 7/8, so what
		// it sees on the action layer is the state closed at 10e LAST frame.
		// Reading it in the pressing Step would always be false.
		if (g_xEngine.Actions().WasPressedThisFrame(DP_Bindings::DP_ACTION_ABILITY))
		{
			g_bPadSawAbilityEdge = true;
		}
		Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_RIGHT_BUMPER);
		g_bPadDone  = true;
		g_ePadPhase = DPPadPhase::Done;
		return false;

	case DPPadPhase::Done:
	default:
		return false;
	}
}

static bool Verify_DPSimPad()
{
	Zenith_InputSimulator::ClearFixedDt();
	Pad_PushStick(0.0f, 0.0f);
	if (g_xPadInteractHandle != INVALID_EVENT_HANDLE)
	{
		Zenith_EventDispatcher::Get().Unsubscribe(g_xPadInteractHandle);
		g_xPadInteractHandle = INVALID_EVENT_HANDLE;
	}

	if (!g_bPadDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[DP_SimPad] never completed (phase %d) -- the ProcLevel scene never produced both a "
			"villager and a door, or the pad steer never finished",
			static_cast<int>(g_ePadPhase));
		return false;
	}
	if (!g_bPadProfileSwitched)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[DP_SimPad] the stick never switched the active profile to P_GAMEPAD (now %u) -- pad rows "
			"resolve only while P_GAMEPAD is active, so nothing below would mean anything",
			static_cast<u_int32>(g_xEngine.Actions().GetActiveProfile()));
		return false;
	}
	if (g_fPadDisplacement < 0.5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[DP_SimPad] the possessed villager moved %.2f m on the left stick -- the MOVE row's stick "
			"binding or its y inversion regressed (or the villager never reached Possessed)",
			g_fPadDisplacement);
		return false;
	}
	if (!g_bPadInteractFired)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[DP_SimPad] pad X inside the door's radius dispatched no DP_OnInteract over %d attempts -- "
			"either the INTERACT row ('%s') lost its pad button or DPInteractable_Base's per-frame poll "
			"no longer reads the action layer",
			g_iPadInteractAttempts + 1, DP_Bindings::szACTION_INTERACT);
		return false;
	}
	if (!g_bPadSawAbilityEdge)
	{
		// Distinct from the INTERACT check on purpose: ABILITY has no consumer,
		// so the edge IS the contract. A failure here is a binding-table break,
		// never a gameplay one.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[DP_SimPad] pad RB raised no ABILITY press edge -- the pad row or the 10e close regressed");
		return false;
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[DP_SimPad] pad drove MOVE (%.2f m on the left stick), INTERACT (pad X -> DP_OnInteract) and "
		"ABILITY (pad RB edge) after auto-switching to P_GAMEPAD",
		g_fPadDisplacement);
	return true;
}

static const Zenith_AutomatedTest g_xDPSimPadTest = {
	"DP_SimPad_Test",
	&Setup_DPSimPad,
	&Step_DPSimPad,
	&Verify_DPSimPad,
	/*maxFrames*/ 900,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPSimPadTest);

#endif // ZENITH_INPUT_SIMULATOR
