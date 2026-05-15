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
	// Front-end main menu sizes. Generously scaled (large screens at
	// 1080p+ make tighter ranges look anemic).
	// ---------------------------------------------------------------
	static constexpr float fMENU_TITLE_FONT    = 128.0f;
	static constexpr float fMENU_SUBTITLE_FONT = 44.0f;
	// Buttons.
	static constexpr float fMENU_BTN_FONT      = 48.0f;
	static constexpr float fMENU_BTN_W         = 400.0f;
	static constexpr float fMENU_BTN_H         = 96.0f;
	static constexpr float fMENU_BTN_SPACING   = 32.0f;

	// ---------------------------------------------------------------
	// In-game HUD font sizes. Bumped 2026-05-15 (user feedback: the
	// 28-36px range looked far too small at gameplay resolutions).
	// New targets:
	//   - Status banner: 120px (huge, dominates the screen on win/loss).
	//   - HUD readouts: 44-48px (clearly readable from arms-length).
	//   - Whisper / awareness line: 36-40px.
	//   - Pause overlay: 64px multi-line.
	// ---------------------------------------------------------------
	// Status banner (VICTORY / CAUGHT BY AELFRIC / DAWN BREAKS / ...).
	static constexpr float fHUD_STATUS_FONT     = 120.0f;
	// Restart prompt under the status banner.
	static constexpr float fHUD_RESTART_FONT    = 44.0f;
	// Per-frame readouts at screen corners.
	static constexpr float fHUD_LIFEBAR_FONT    = 48.0f;
	static constexpr float fHUD_HELDITEM_FONT   = 40.0f;
	static constexpr float fHUD_OBJECTIVES_FONT = 44.0f;
	static constexpr float fHUD_DAWN_FONT       = 48.0f;
	static constexpr float fHUD_SCENT_FONT      = 40.0f;
	// Aelfric awareness icon + whisper line.
	static constexpr float fHUD_AWARENESS_FONT  = 40.0f;
	static constexpr float fHUD_WHISPER_FONT    = 36.0f;
	// Secondary readouts -- archetype name, life-seconds, movement
	// mode, villagers-alive count, priest distance, run timer,
	// interact hint, reagent description. Slightly smaller than the
	// primary corner readouts because there are more of them and the
	// HUD shouldn't drown the gameplay view.
	static constexpr float fHUD_VILLAGER_INFO_FONT  = 32.0f;
	static constexpr float fHUD_LIFE_NUMERIC_FONT   = 32.0f;
	static constexpr float fHUD_MOVEMENT_FONT       = 32.0f;
	static constexpr float fHUD_VILLAGER_COUNT_FONT = 32.0f;
	static constexpr float fHUD_PRIEST_DIST_FONT    = 32.0f;
	static constexpr float fHUD_RUN_TIMER_FONT      = 32.0f;
	static constexpr float fHUD_INTERACT_HINT_FONT  = 36.0f;
	static constexpr float fHUD_REAGENT_HELP_FONT   = 28.0f;
	// Pause overlay (multi-line; the line spacing makes 64px feel less
	// dominant than the Status banner).
	static constexpr float fHUD_PAUSE_FONT      = 64.0f;
	// Gym scene title.
	static constexpr float fHUD_GYM_TITLE_FONT  = 56.0f;

	// ---------------------------------------------------------------
	// Edge insets in pixels. Used as the pixel offset from the
	// corresponding anchor when authoring HUD elements (e.g.,
	// TopRight anchor + (-fEDGE_INSET, +fEDGE_INSET) position).
	// ---------------------------------------------------------------
	static constexpr float fEDGE_INSET = 40.0f;
}
