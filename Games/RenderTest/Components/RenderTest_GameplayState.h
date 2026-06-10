#pragma once

// Tiny shared header to break a header-only mutual-include cycle between
// RenderTest_FollowCamera and RenderTest_PlayerBehaviour: the camera reads the
// player's aiming flag, and the player reads the camera's yaw/pitch.
//
// IMPORTANT: keep this strictly minimal. Anything beyond camera angles + the
// aiming flag belongs in a proper component, not in this header.

namespace RenderTest_GameplayState
{
	// Inline globals (C++17+). The two behaviour headers each #include this and
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
	}
}
