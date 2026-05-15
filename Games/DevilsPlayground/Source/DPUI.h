#pragma once
/**
 * DPUI - DevilsPlayground UI sizing constants.
 *
 * Mirrors the TilePuzzleUI pattern (Games/TilePuzzle/TilePuzzle.cpp ~line 1808+).
 * Centralised so the front-end + HUD authoring + future settings menus all
 * pull from the same set of numbers, and so adjusting a font size in one
 * place updates every consumer.
 *
 * **Reference resolution: 1920x1080.** The UI canvas scales relative to
 * that. Font sizes below are tuned to be legible at 720p (the lowest
 * resolution the project targets) without overflowing edges.
 *
 * Off-edge bug prevention: any HUD element anchored to a screen edge
 * (TopRight, BottomLeft, etc.) needs its `TextAlignment` set so the
 * rendered text grows in the direction that keeps it inside the screen.
 * The engine logs a Zenith_Warning when an element renders past an edge
 * (Zenith_UIText::Render), so a regression here surfaces as a runtime
 * warning rather than silent clipping.
 */

namespace DPUI
{
	// ---------------------------------------------------------------
	// Front-end main menu sizes.
	// ---------------------------------------------------------------
	// Big block title -- "DEVIL'S PLAYGROUND". Matches TilePuzzle's
	// MENU_TITLE_FONT (96.0f).
	static constexpr float fMENU_TITLE_FONT    = 96.0f;
	static constexpr float fMENU_SUBTITLE_FONT = 36.0f;
	// Buttons.
	static constexpr float fMENU_BTN_FONT      = 44.0f;
	static constexpr float fMENU_BTN_W         = 360.0f;
	static constexpr float fMENU_BTN_H         = 88.0f;
	static constexpr float fMENU_BTN_SPACING   = 24.0f;

	// ---------------------------------------------------------------
	// In-game HUD font sizes.
	// ---------------------------------------------------------------
	// Status banner (VICTORY / CAUGHT BY AELFRIC / DAWN BREAKS / ...).
	// Largest in-game element; visible from any reading distance.
	static constexpr float fHUD_STATUS_FONT     = 96.0f;
	// Restart prompt under the status banner.
	static constexpr float fHUD_RESTART_FONT    = 36.0f;
	// Per-frame readouts at screen corners.
	static constexpr float fHUD_LIFEBAR_FONT    = 38.0f;
	static constexpr float fHUD_HELDITEM_FONT   = 32.0f;
	static constexpr float fHUD_OBJECTIVES_FONT = 36.0f;
	static constexpr float fHUD_DAWN_FONT       = 38.0f;
	static constexpr float fHUD_SCENT_FONT      = 32.0f;
	// Aelfric awareness icon + whisper line.
	static constexpr float fHUD_AWARENESS_FONT  = 32.0f;
	static constexpr float fHUD_WHISPER_FONT    = 30.0f;
	// Pause overlay.
	static constexpr float fHUD_PAUSE_FONT      = 56.0f;
	// Gym scene title (the per-gym label in the top centre).
	static constexpr float fHUD_GYM_TITLE_FONT  = 48.0f;

	// ---------------------------------------------------------------
	// Edge insets in pixels. Used as the pixel offset from the
	// corresponding anchor when authoring HUD elements (e.g.,
	// TopRight anchor + (-fEDGE_INSET, +fEDGE_INSET) position).
	// ---------------------------------------------------------------
	static constexpr float fEDGE_INSET = 40.0f;
}
