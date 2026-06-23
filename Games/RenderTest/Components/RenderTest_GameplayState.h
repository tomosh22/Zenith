#pragma once

#include <string>

// Tiny shared header to break a header-only mutual-include cycle between
// RenderTest_FollowCameraComponent and RenderTest_PlayerComponent: the camera
// reads the player's aiming flag, and the player reads the camera's yaw/pitch.
//
// IMPORTANT: keep this strictly minimal. Anything beyond camera angles + the
// aiming flag belongs in a proper component, not in this header.

namespace RenderTest_GameplayState
{
	// Inline globals (C++17+). The two component headers each #include this and
	// read/write directly; no static accessors on either class are needed.
	inline float s_fCameraYaw   = 0.0f;
	inline float s_fCameraPitch = 0.0f;
	inline bool  s_bLocalPlayerAiming = false;

	// Photo-mode camera override (visual tests / capture harnesses). When
	// active, the follow camera ignores mouse-look and parks at
	// player + s_xPhotoOffset (world-space) with the given pitch — same view
	// path as normal play, just scripted numbers.
	inline bool  s_bPhotoModeActive = false;
	inline float s_fPhotoOffsetX = 0.0f;
	inline float s_fPhotoOffsetY = 0.0f;
	inline float s_fPhotoOffsetZ = 0.0f;
	inline float s_fPhotoPitch   = 0.0f;
	inline float s_fPhotoYaw     = 0.0f;   // pi = parked in front, looking back at the player

	// Tennis spectator camera (capture aid for the tennis-court testbed). When
	// active the follow camera ignores the player and parks at a fixed
	// world-space vantage overlooking the floating court — so the autonomous NPC
	// match (which runs in Play mode) can be framed/screenshotted. Set by the
	// --rendertest-tennis-spectator flag in the tennis spawn. Intentionally NOT
	// cleared by Reset(): it's a process-level capture mode, not per-scene state.
	inline bool  s_bTennisSpectatorActive = false;
	inline float s_fTennisCamX = 0.0f;
	inline float s_fTennisCamY = 0.0f;
	inline float s_fTennisCamZ = 0.0f;
	inline float s_fTennisCamYaw = 0.0f;
	inline float s_fTennisCamPitch = 0.0f;

	// Follow-cam sub-mode: when active, the spectator camera tracks one NPC up
	// close (a 3/4 view) so the strokes, foot/arm IK and racket read clearly,
	// instead of the fixed court overlook. s_iTennisFollowSide: 0 = near, 1 = far.
	inline bool s_bTennisFollowActive = false;
	inline int  s_iTennisFollowSide = 0;

	// IK-showcase mode: instead of the normal match, the near player repeatedly
	// performs ONE stroke against a ball frozen at the contact point, so the
	// arm-IK visibly places the racket on the ball. s_iTennisShowcaseStroke:
	// 0 = serve, 1 = forehand, 2 = backhand.
	inline bool s_bTennisIkShowcase = false;
	inline int  s_iTennisShowcaseStroke = 0;

	// Telemetry: when active the tennis referee records per-frame samples +
	// gameplay events through Zenith_Telemetry and flushes a .ztlm (+ .json +
	// CSV sidecars) at match end / shutdown. Enabled by --rendertest-tennis-telemetry
	// [=<base-path>]. Gated to the real "RenderTest" game scene so the unit-test
	// fixtures (which construct referees in additive test scenes) never record.
	// Process-level, NOT cleared by Reset().
	inline bool        s_bTennisTelemetry = false;
	inline std::string s_strTennisTelemetryPath = "tennis_telemetry";  // base; .ztlm/.json/_*.csv appended

	inline float GetCameraYaw()        { return s_fCameraYaw; }
	inline float GetCameraPitch()      { return s_fCameraPitch; }
	inline bool  IsLocalPlayerAiming() { return s_bLocalPlayerAiming; }

	// Reset everything to defaults — call at scene-start time so static state
	// from a previous Play→Stop→Play cycle doesn't leak into the new run.
	inline void Reset()
	{
		s_fCameraYaw = 0.0f;
		s_fCameraPitch = 0.0f;
		s_bLocalPlayerAiming = false;
		s_bPhotoModeActive = false;
		s_fPhotoOffsetX = 0.0f;
		s_fPhotoOffsetY = 0.0f;
		s_fPhotoOffsetZ = 0.0f;
		s_fPhotoPitch = 0.0f;
		s_fPhotoYaw = 0.0f;
	}
}
