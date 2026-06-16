#pragma once
#include "Maths/Zenith_Maths.h"
#include <cstdint>

// Jetpack backpack testbed for RenderTest.
//
// A procedurally-built jetpack is worn on the player's back, attached to the
// "Spine" bone by the engine's Zenith_AttachmentComponent — the same component
// the tennis racket and the FPS guns use, here applied to a permanently-worn
// backpack instead of a hand-held tool. The player controller drives it:
// holding the space bar fires the jetpack, accelerating the player upward and
// (since the existing horizontal-movement code is not grounded-gated) letting
// the arrow keys steer mid-air. A Zenith_ParticleEmitterComponent on the
// jetpack entity renders the jet trail, positioned each frame at the jetpack's
// two exhaust nozzles and aimed downward.
//
// Like the tennis court and the guns, everything here is procedural (meshes
// built at runtime, do not survive SaveScene), so the spawn runs post
// scene-load and is windowed-only (Zenith_CommandLine::IsHeadless() early-out).
// Unlike the guns, the jetpack is NOT a pickup — it is auto-attached to the
// player at spawn and stays on.

// Procedural spawn entry point (queued via AddStep_Custom after the guns).
// Builds the backpack mesh, creates the jetpack entity, attaches it to the
// player's Spine bone, and gives it the jet-trail emitter. Windowed-only.
void RenderTest_SpawnJetpack();

// Release the file-scope material handles BEFORE Zenith_AssetRegistry shutdown
// (mirrors RenderTest_GunsShutdown). Call from Project_Shutdown.
void RenderTest_JetpackShutdown();

// Optional showcase / tuning support. --rendertest-jetpack-showcase enters Play
// and forces the jetpack to thrust while a side photo-camera frames the rising
// player + trail so it can be screenshotted. The --jetpack-mount-* float knobs
// live-override the backpack's mount transform (position + Euler degrees) so the
// placement on the back can be calibrated from screenshots without recompiling.
namespace RenderTest_JetpackTuning
{
	// True when --rendertest-jetpack-showcase was passed.
	inline bool s_bShowcaseActive = false;

	// Live mount overrides (1e30 = "unset, use the default"). Applied when the
	// jetpack is attached to the Spine bone.
	inline float s_fMountX     = 1e30f;
	inline float s_fMountY     = 1e30f;
	inline float s_fMountZ     = 1e30f;
	inline float s_fMountPitch = 1e30f;   // degrees about X
	inline float s_fMountYaw   = 1e30f;   // degrees about Y
	inline float s_fMountRoll  = 1e30f;   // degrees about Z

	inline bool IsSet(float f) { return f < 1e29f; }
}
