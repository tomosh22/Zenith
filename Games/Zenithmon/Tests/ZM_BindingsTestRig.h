#pragma once

// ============================================================================
// ZM_BindingsTestRig -- the boot-unit harness for Zenithmon's action table.
//
// TEST-ONLY, and it exists because the action layer is a FRAME CONTRACT, not a
// poll. The engine's own Zenith_InputActions is opened at step 8 and closed at
// steps 10b/10e inside the main loop; a ZENITH_TEST runs long before the first
// frame, so reading g_xEngine.Actions() from one would answer with nothing.
//
// So every unit here drives a LOCAL Zenith_InputActions bound to a LOCAL
// Zenith_Input + Zenith_Pointers (B13's singleton ratchet), with ZM's own
// binding table installed into it by ZM_Bindings::Register. No window, no
// engine state, and the game's live registrations are never touched.
//
// One test "frame" is the real contract minus the engine:
//   BeginFrame()   -> the device-layer frame boundary (edges + transition log)
//   ...events...   -> raw KEY / PAD transitions, or the simulator drain
//   CloseFrame()   -> UpdateProfile (8), FinalizeReservedUI (10b),
//                     FinalizeGameplay (10e)
//
// ★ RAW DEVICE CODES BELONG HERE. Production code spells a key exactly once,
// in ZM_Bindings.h; the units deliberately spell them again so they
// CHARACTERISE the binding instead of restating it. That is the whole point of
// a device-layer proof and is why Tests/ is exempt from the no-literals rule.
// ============================================================================

#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Input/Zenith_Pointers.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/ZM_Bindings.h"

namespace ZM_BindingsTest
{
	struct Rig
	{
		Zenith_Input        m_xInput;
		Zenith_Pointers     m_xPointers;
		Zenith_InputActions m_xActions;

		Rig()
		{
			m_xActions.Initialise(m_xInput, m_xPointers);
			ZM_Bindings::Register(m_xActions);
		}

		void BeginFrame()
		{
			m_xInput.DrainPendingPlatformEvents();
			m_xPointers.BeginFrame(1.0f);
		}

		void CloseFrame()
		{
			OpenActionFrame();
			CloseStages();
		}

		// ★ SPLIT FOR VIRTUAL PUBLISHES, AND THE ORDER IS LOAD-BEARING.
		// UpdateProfile (step 8) OPENS the action frame, and opening it CLEARS the
		// virtual transition queue -- because the real loop publishes at 10d,
		// AFTER step 8. A test that publishes before UpdateProfile is publishing
		// into the previous frame: its LEVEL survives (an axis still reads) but its
		// ordered rise/fall is discarded, so a same-frame tap fires no edges at
		// all. Publish between these two calls.
		void OpenActionFrame() { m_xActions.UpdateProfile(); }
		void CloseStages()
		{
			m_xActions.FinalizeReservedUI();
			m_xActions.FinalizeGameplay();
		}

		// A whole frame with nothing in it -- used to prove an EDGE does not
		// repeat while the source stays held.
		void EmptyFrame()
		{
			BeginFrame();
			CloseFrame();
		}

		void RaiseEvent(Zenith_InputEventType eType, int32_t iCode, int32_t iDevice = 0,
			bool bSystemNav = false)
		{
			Zenith_InputEvent xEvent;
			xEvent.m_eType      = eType;
			xEvent.m_iCode      = iCode;
			xEvent.m_iDevice    = iDevice;
			xEvent.m_bSystemNav = bSystemNav;
			m_xInput.AppendInjectedEvent(xEvent);
		}

		void KeyDown(int32_t iKey) { RaiseEvent(INPUT_EVENT_KEY_PRESS, iKey); }
		void KeyUp(int32_t iKey)   { RaiseEvent(INPUT_EVENT_KEY_RELEASE, iKey); }
		void PadDown(int32_t iButton, int32_t iPad = 0) { RaiseEvent(INPUT_EVENT_PAD_PRESS, iButton, iPad); }
		void PadUp(int32_t iButton, int32_t iPad = 0)   { RaiseEvent(INPUT_EVENT_PAD_RELEASE, iButton, iPad); }
		void SystemBack() { RaiseEvent(INPUT_EVENT_SYSTEM_BACK, 0, 0, /*bSystemNav*/ true); }

		// One frame in which exactly these keys are pressed and then held.
		void PressKeyForOneFrame(int32_t iKey)
		{
			BeginFrame();
			KeyDown(iKey);
			CloseFrame();
		}

		Zenith_Maths::Vector2 Move() const
		{
			Zenith_Maths::Vector2 xMove(0.0f);
			m_xActions.GetAxis2D(ZM_Bindings::ZM_ACTION_MOVE, xMove);
			return xMove;
		}
	};

	// RAII so a failing assertion cannot leave the simulator enabled for the rest
	// of the boot batch.
	struct SimScope
	{
		SimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::Enable();
			Zenith_InputSimulator::ResetAllInputState();
#endif
		}
		~SimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::DiscardPendingInjections();
			Zenith_InputSimulator::ResetAllInputState();
			Zenith_InputSimulator::Disable();
#endif
		}
	};

	// Frame contract step 7, minus the engine: everything the simulator queued
	// becomes ordered transitions on the LOCAL device layer.
	inline void DrainSimulatorInjections(Rig& xRig)
	{
#ifdef ZENITH_INPUT_SIMULATOR
		const u_int32 uCount = Zenith_InputSimulator::GetPendingInjectionCount();
		for (u_int32 u = 0; u < uCount; u++)
		{
			xRig.m_xInput.AppendInjectedEvent(Zenith_InputSimulator::GetPendingInjection(u));
		}
		Zenith_InputSimulator::DiscardPendingInjections();
#else
		(void)xRig;
#endif
	}

	// ------------------------------------------------------------------------
	// Driving the ENGINE's action layer from a boot unit.
	//
	// The two helpers below exist for the ONE case a local rig cannot serve: a
	// unit that drives a real ECS component (ZM_PlayerController) by hand. That
	// component reads g_xEngine.Actions(), and the engine's instance is opened at
	// frame step 8 and closed at 10e inside Zenith_Core::Zenith_MainLoop -- which
	// a unit test never runs. Without these, every action reads "not held" and the
	// controller no-ops in silence.
	//
	// ★ THE ORDER IS THE WHOLE CONTRACT. Zenith_Input::BeginFrame DISCARDS the
	// simulator's pending injections (the real loop queues them from a test Step
	// AFTER BeginFrame), so a Begin placed after SimulateKeyDown would swallow the
	// press. Begin OPENS the next frame; Close drains and finalises the current
	// one. Keys are queued between them, exactly as a Step queues between the real
	// loop's BeginFrame_Platform and ApplySimulatorInjection.
	// ------------------------------------------------------------------------

	// Frame contract step 3: end the frame's edges and its transition log.
	inline void BeginEngineInputFrame()
	{
		g_xEngine.Input().BeginFrame();
	}

	// Frame contract steps 7, 8, 10b and 10e on the engine's own instances.
	inline void CloseEngineActionFrame()
	{
		g_xEngine.Input().ApplySimulatorInjection();
		g_xEngine.Actions().UpdateProfile();
		g_xEngine.Actions().FinalizeReservedUI();
		g_xEngine.Actions().FinalizeGameplay();
	}

	// Restore the engine's device / action / pointer layers to their boot state.
	// The engine's automated-test harness does exactly this between tests; a boot
	// unit that reached the engine layers owes the rest of the batch the same.
	inline void ResetEngineInputForTest()
	{
		g_xEngine.Input().ResetTransientForTest();
		g_xEngine.Actions().ResetTransientForTest();
		g_xEngine.Pointers().ResetTransientForTest();
	}

	// ★ A MID-TEST WIPE MUST CLEAR BOTH SIDES. Zenith_InputSimulator::
	// ResetAllInputState drops the simulator's LEVEL table without emitting the
	// releases, so the action layer's source shadow -- which is fed by
	// TRANSITIONS alone -- would keep the old key held forever. A second phase
	// then presses the opposite key and the two cancel, and the test fails with
	// "the character did not move" and no clue why.
	inline void ResetSimulatedAndEngineInput()
	{
#ifdef ZENITH_INPUT_SIMULATOR
		Zenith_InputSimulator::ResetAllInputState();
#endif
		ResetEngineInputForTest();
		BeginEngineInputFrame();
	}

	// Every KEYBOARD code an action's rows name, flattened across every key set of
	// every KEY_* row. This is how the collision units walk the LIVE table rather
	// than a constant re-spelled beside it: a rebind that put the interact key onto
	// a movement row shows up here.
	inline u_int CollectKeys(const Zenith_InputActions& xActions, Zenith_InputActionID uAction,
		int32_t* piOut, u_int uCapacity)
	{
		u_int uCount = 0u;
		const u_int32 uBindings = xActions.GetBindingCount(uAction);
		for (u_int32 uBinding = 0; uBinding < uBindings; uBinding++)
		{
			const Zenith_InputBinding& xBinding = xActions.GetBinding(uAction, uBinding);
			if (xBinding.m_eType != INPUT_BINDING_KEY_SET
				&& xBinding.m_eType != INPUT_BINDING_KEY_AXIS1D
				&& xBinding.m_eType != INPUT_BINDING_KEY_AXIS2D)
			{
				continue;
			}
			for (u_int32 uSet = 0; uSet < Zenith_InputBinding::uMAX_KEY_SETS; uSet++)
			{
				for (u_int32 uKey = 0; uKey < xBinding.m_auKeyCount[uSet]; uKey++)
				{
					if (uCount >= uCapacity)
					{
						return uCount;
					}
					piOut[uCount++] = xBinding.m_aaiKeys[uSet][uKey];
				}
			}
		}
		return uCount;
	}
}
