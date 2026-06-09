#pragma once
#include "Core/Zenith_Engine.h"

#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

namespace DP_Input
{
	inline Zenith_Maths::Vector2 ReadMoveVillager()
	{
		Zenith_Maths::Vector2 xInput(0.0f);
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_W) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_UP))
			xInput.y += 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_S) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_DOWN))
			xInput.y -= 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_A) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT))
			xInput.x -= 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_D) || g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT))
			xInput.x += 1.0f;
		return xInput;
	}

	// MVP-1.7: sprint hold. Either Shift key works (some Windows /
	// laptop keyboards don't fire both as the same logical key).
	// DPVillager_Behaviour reads this each frame to decide whether
	// to apply movement.sprint_speed_mps and the
	// movement.sprint_life_cost_extra_per_s drain.
	inline bool ReadSprintHeld()
	{
		return g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_SHIFT)
			|| g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_SHIFT);
	}

	// MVP-1.7: walk-quiet hold. Either Ctrl key works. While held AND
	// the villager is moving, footsteps emit at
	// `movement.walk_footstep_loudness_multiplier` (0.5x default) and
	// the villager moves at `movement.walk_speed_mps` instead of the
	// jog default. Sprint wins ties (holding Shift+Ctrl resolves to
	// sprint -- the louder, faster mode).
	inline bool ReadWalkQuietHeld()
	{
		return g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_CONTROL)
			|| g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);
	}

	inline bool ReadInteractPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_F);
	}

	// MVP-1.4.5: drop verb. Releases the possessed villager's held
	// item at the villager's foot position. Single-frame edge so
	// holding G doesn't keep dropping (no-op once held = None).
	inline bool ReadDropPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_G);
	}

	inline bool ReadAbilityPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_SPACE);
	}

	inline float ReadCameraRotate()
	{
		float f = 0.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_Q)) f -= 1.0f;
		if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_E)) f += 1.0f;
		return f;
	}

	inline bool ReadEscapePressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);
	}

	inline bool ReadPossessClickPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);
	}

	// 2026-05-16: instructional-HUD toggle. H opens / closes the
	// full-screen help overlay authored on the GameLevel scene
	// (HelpBg + HelpOverlay). Single-frame edge so holding H doesn't
	// strobe the overlay.
	inline bool ReadHelpTogglePressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_H);
	}
}
