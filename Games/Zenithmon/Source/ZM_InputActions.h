#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// Game-local action names over Zenith's raw keyboard input. Keeping this layer
// stateless makes the mappings easy to replace when rebinding lands, while the
// pure resolver gives tests the same opposite-key and diagonal semantics as the
// live readers without requiring an engine instance.
//
// The KEY SETS below are the single source of every binding in this game: each
// reader walks the same array a test walks, so a rebind that collided with the
// interact key would fail the collision units instead of silently double-firing
// (S6 item 3 SC1). Never spell a ZENITH_KEY_* literal in PRODUCTION code outside
// this file. Windowed tests deliberately DO spell raw key codes, so that they
// characterise the binding rather than merely restate it.
namespace ZM_InputActions
{
	// ---- Bindings (the ONE place a raw key code appears) --------------------

	// Talk to / examine the thing in front of the player (S6 item 3). E was
	// verified free across the whole Zenithmon tree: the only codes this game
	// claims are the four movement pairs, Enter/Space, Escape/Backspace, M/Tab
	// and both Shifts, all of which are enumerated below.
	inline constexpr Zenith_KeyCode ZM_KEY_INTERACT = ZENITH_KEY_E;

	inline constexpr Zenith_KeyCode ZM_CONFIRM_KEYS[] = { ZENITH_KEY_ENTER, ZENITH_KEY_SPACE };
	inline constexpr Zenith_KeyCode ZM_CANCEL_KEYS[]  = { ZENITH_KEY_ESCAPE, ZENITH_KEY_BACKSPACE };
	inline constexpr Zenith_KeyCode ZM_MENU_KEYS[]    = { ZENITH_KEY_M, ZENITH_KEY_TAB };
	inline constexpr Zenith_KeyCode ZM_RUN_KEYS[]     = { ZENITH_KEY_LEFT_SHIFT, ZENITH_KEY_RIGHT_SHIFT };

	// Per-direction sets: the movement readers AND the menu-cursor reader walk
	// these, so up/down mean the same keys in both places by construction.
	inline constexpr Zenith_KeyCode ZM_MOVE_FORWARD_KEYS[]  = { ZENITH_KEY_W, ZENITH_KEY_UP };
	inline constexpr Zenith_KeyCode ZM_MOVE_BACKWARD_KEYS[] = { ZENITH_KEY_S, ZENITH_KEY_DOWN };
	inline constexpr Zenith_KeyCode ZM_MOVE_LEFT_KEYS[]     = { ZENITH_KEY_A, ZENITH_KEY_LEFT };
	inline constexpr Zenith_KeyCode ZM_MOVE_RIGHT_KEYS[]    = { ZENITH_KEY_D, ZENITH_KEY_RIGHT };

	inline constexpr u_int uZM_CONFIRM_KEY_COUNT       = (u_int)(sizeof(ZM_CONFIRM_KEYS)       / sizeof(ZM_CONFIRM_KEYS[0]));
	inline constexpr u_int uZM_CANCEL_KEY_COUNT        = (u_int)(sizeof(ZM_CANCEL_KEYS)        / sizeof(ZM_CANCEL_KEYS[0]));
	inline constexpr u_int uZM_MENU_KEY_COUNT          = (u_int)(sizeof(ZM_MENU_KEYS)          / sizeof(ZM_MENU_KEYS[0]));
	inline constexpr u_int uZM_RUN_KEY_COUNT           = (u_int)(sizeof(ZM_RUN_KEYS)           / sizeof(ZM_RUN_KEYS[0]));
	inline constexpr u_int uZM_MOVE_FORWARD_KEY_COUNT  = (u_int)(sizeof(ZM_MOVE_FORWARD_KEYS)  / sizeof(ZM_MOVE_FORWARD_KEYS[0]));
	inline constexpr u_int uZM_MOVE_BACKWARD_KEY_COUNT = (u_int)(sizeof(ZM_MOVE_BACKWARD_KEYS) / sizeof(ZM_MOVE_BACKWARD_KEYS[0]));
	inline constexpr u_int uZM_MOVE_LEFT_KEY_COUNT     = (u_int)(sizeof(ZM_MOVE_LEFT_KEYS)     / sizeof(ZM_MOVE_LEFT_KEYS[0]));
	inline constexpr u_int uZM_MOVE_RIGHT_KEY_COUNT    = (u_int)(sizeof(ZM_MOVE_RIGHT_KEYS)    / sizeof(ZM_MOVE_RIGHT_KEYS[0]));

	// Every movement key in one walkable set, BUILT FROM the per-direction sets
	// above rather than re-listed. It hand-indexes them, so a direction set that
	// GAINS a key would silently drop out of the collision unit's walk -- the
	// static_assert below turns that drift into a build break.
	inline constexpr Zenith_KeyCode ZM_MOVE_KEYS[] =
	{
		ZM_MOVE_FORWARD_KEYS[0],  ZM_MOVE_FORWARD_KEYS[1],
		ZM_MOVE_BACKWARD_KEYS[0], ZM_MOVE_BACKWARD_KEYS[1],
		ZM_MOVE_LEFT_KEYS[0],     ZM_MOVE_LEFT_KEYS[1],
		ZM_MOVE_RIGHT_KEYS[0],    ZM_MOVE_RIGHT_KEYS[1]
	};
	inline constexpr u_int uZM_MOVE_KEY_COUNT = (u_int)(sizeof(ZM_MOVE_KEYS) / sizeof(ZM_MOVE_KEYS[0]));

	// The flat set must enumerate EVERY per-direction binding. Without this, adding a
	// third key to a direction would leave ZM_MOVE_KEYS listing only elements [0] and
	// [1] of it: the live reader would consume the new key while the interact-collision
	// unit walked a stale set and passed. Shrinking already fails (constexpr OOB index);
	// this makes GROWING -- the realistic edit -- fail too.
	static_assert(uZM_MOVE_KEY_COUNT ==
		uZM_MOVE_FORWARD_KEY_COUNT + uZM_MOVE_BACKWARD_KEY_COUNT +
		uZM_MOVE_LEFT_KEY_COUNT + uZM_MOVE_RIGHT_KEY_COUNT,
		"ZM_MOVE_KEYS must list EVERY per-direction key -- the collision unit walks it");

	// ---- Set readers -------------------------------------------------------

	// True iff ANY key in the set is held. Level query.
	inline bool AnyKeyDown(const Zenith_KeyCode* piKeys, u_int uCount)
	{
		Zenith_Input& xInput = g_xEngine.Input();
		for (u_int u = 0u; u < uCount; ++u)
		{
			if (xInput.IsKeyDown(piKeys[u]))
			{
				return true;
			}
		}
		return false;
	}

	// True iff ANY key in the set had its press EDGE this frame. NON-consuming:
	// two callers reading the same edge in one frame both see it, so any mutual
	// exclusion between consumers must be written out explicitly by the caller.
	inline bool AnyKeyPressedThisFrame(const Zenith_KeyCode* piKeys, u_int uCount)
	{
		Zenith_Input& xInput = g_xEngine.Input();
		for (u_int u = 0u; u < uCount; ++u)
		{
			if (xInput.WasKeyPressedThisFrame(piKeys[u]))
			{
				return true;
			}
		}
		return false;
	}

	inline Zenith_Maths::Vector2 ResolveMove(
		bool bForward,
		bool bBackward,
		bool bLeft,
		bool bRight)
	{
		Zenith_Maths::Vector2 xMove(0.0f);
		if (bForward)
		{
			xMove.y += 1.0f;
		}
		if (bBackward)
		{
			xMove.y -= 1.0f;
		}
		if (bLeft)
		{
			xMove.x -= 1.0f;
		}
		if (bRight)
		{
			xMove.x += 1.0f;
		}
		return xMove;
	}

	inline Zenith_Maths::Vector2 ReadMove()
	{
		return ResolveMove(
			AnyKeyDown(ZM_MOVE_FORWARD_KEYS,  uZM_MOVE_FORWARD_KEY_COUNT),
			AnyKeyDown(ZM_MOVE_BACKWARD_KEYS, uZM_MOVE_BACKWARD_KEY_COUNT),
			AnyKeyDown(ZM_MOVE_LEFT_KEYS,     uZM_MOVE_LEFT_KEY_COUNT),
			AnyKeyDown(ZM_MOVE_RIGHT_KEYS,    uZM_MOVE_RIGHT_KEY_COUNT));
	}

	inline bool ReadConfirmPressed()
	{
		return AnyKeyPressedThisFrame(ZM_CONFIRM_KEYS, uZM_CONFIRM_KEY_COUNT);
	}

	inline bool ReadCancelPressed()
	{
		return AnyKeyPressedThisFrame(ZM_CANCEL_KEYS, uZM_CANCEL_KEY_COUNT);
	}

	// The interact / talk EDGE (S6 item 3). Non-consuming, exactly like its confirm
	// and cancel siblings -- the ZM_ShouldInteract gate, not consumption, is what
	// stops a menu-open frame from also firing an interaction.
	inline bool ReadInteractPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZM_KEY_INTERACT);
	}

	// -1 up / +1 down / 0 none -- EDGE (WasKeyPressedThisFrame) so one press = one step.
	// Drives the battle Fight/Run menu cursor (SC5). Kept in this namespace to match
	// the sibling readers rather than the brief's free-function ZM_ReadMenuVertical.
	inline int ReadMenuVertical()
	{
		const bool bUp   = AnyKeyPressedThisFrame(ZM_MOVE_FORWARD_KEYS,  uZM_MOVE_FORWARD_KEY_COUNT);
		const bool bDown = AnyKeyPressedThisFrame(ZM_MOVE_BACKWARD_KEYS, uZM_MOVE_BACKWARD_KEY_COUNT);
		return (bDown ? 1 : 0) - (bUp ? 1 : 0);
	}

	inline bool ReadMenuPressed()
	{
		return AnyKeyPressedThisFrame(ZM_MENU_KEYS, uZM_MENU_KEY_COUNT);
	}

	inline bool ReadRunHeld()
	{
		return AnyKeyDown(ZM_RUN_KEYS, uZM_RUN_KEY_COUNT);
	}
}
