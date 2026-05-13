#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Zenith_AudioBus -- MVP-0.4.1
//
// Engine-wide one-shot audio emission hook. The MVP audio "system" is just a
// recording bus: EmitSound() records (name, position, loudness, radius, frame)
// into a per-frame ring buffer in test builds so DP tests can assert audio
// effects without a real audio backend. Shipping builds (when an actual audio
// system lands post-MVP) will delegate EmitSound() to the playback layer; the
// recording hook stays available behind ZENITH_INPUT_SIMULATOR.
//
// Caller pattern (game code):
//   Zenith_AudioBus::EmitSound("Forge.Hammer", xPos, 1.0f, 30.0f);
//
// Test pattern (ZENITH_INPUT_SIMULATOR builds only):
//   Zenith_AudioBus::ClearEmittedSoundsForTest();
//   ... simulate gameplay ...
//   const auto& xSounds = Zenith_AudioBus::GetEmittedSoundsForTest();
//   for (u_int u = 0; u < xSounds.GetSize(); ++u) {
//       const auto& xS = xSounds.Get(u);
//       if (strcmp(xS.m_szName, "Forge.Hammer") == 0) {
//           Zenith_Assert(xS.m_fRadius >= 30.0f, "Forge audible-at-30m contract");
//       }
//   }
//
// The recording ring is per-frame logically -- callers Clear at the start of
// each test phase and inspect at the end. The implementation uses a simple
// growable vector (not a true ring), reset by Clear; test scopes are short
// enough that this is fine.
// ============================================================================
namespace Zenith_AudioBus
{
	// One recorded emission entry. POD layout for trivial copy.
	struct EmittedSound
	{
		const char*           m_szName    = nullptr; // Caller-owned literal or interned string.
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		float                 m_fLoudness = 0.0f;    // Normalised 0..1 (caller convention).
		float                 m_fRadius   = 0.0f;    // Audible-falloff metres.
		u_int                 m_uFrame    = 0;       // Frame counter at emission time.
	};

	// Record a sound emission. Cheap; safe to call from main-thread game code
	// (Editor Update / OnUpdate / OnInteract). The radius parameter is required
	// by tests asserting audible-at-X-metres contracts (e.g.
	// Test_P2Forge_AudibleAt30m per TestPlan section 3.5) -- don't drop it.
	//
	// In shipping builds (ZENITH_INPUT_SIMULATOR undefined) this is a no-op
	// stub until the post-MVP audio playback layer lands. Calls are cheap
	// enough that callers don't need to guard with #ifdef.
	void EmitSound(const char* szName,
	               const Zenith_Maths::Vector3& xPosition,
	               float fLoudness,
	               float fRadius);

#ifdef ZENITH_INPUT_SIMULATOR
	// Test-only accessors. Available behind ZENITH_INPUT_SIMULATOR so shipping
	// builds don't carry the recording buffer.

	// Returns the recorded sounds since the last Clear. The reference is valid
	// until the next Clear / EmitSound call (vector may reallocate).
	const Zenith_Vector<EmittedSound>& GetEmittedSoundsForTest();

	// Reset the recording buffer. Typically called at the start of a test's
	// Step phase (after Setup) to scope the recording to the phase under test.
	void ClearEmittedSoundsForTest();

	// Tick the frame counter recorded in EmittedSound::m_uFrame. Called by
	// the engine's frame loop in test builds; game code shouldn't call this.
	void AdvanceFrameForTest();
#endif
}
