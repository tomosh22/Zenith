#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Zenith_RenderBus -- MVP-0.4.2
//
// Engine-wide per-frame draw-call recording instrumentation. Mirrors the
// design of Zenith_AudioBus (MVP-0.4.1): game / engine render code calls
// RecordDrawCall during submit; tests inspect the recorded sequence to
// assert what was rendered without needing a real GPU readback.
//
// Caller pattern (engine render layer):
//   Zenith_RenderBus::RecordDrawCall("StaticMesh.Floor", xWorldMatrix,
//                                    uVertexCount, uMaterialId);
//
// Test pattern (ZENITH_INPUT_SIMULATOR builds only):
//   Zenith_RenderBus::ClearDrawCallsForTest();
//   ... simulate one frame ...
//   const auto& xCalls = Zenith_RenderBus::GetSubmittedDrawCallsForTest();
//   Zenith_Assert(xCalls.GetSize() > 0, "Expected at least one draw call");
//
// In shipping builds (ZENITH_INPUT_SIMULATOR undefined), RecordDrawCall is
// an arg-silenced stub -- same pattern as Zenith_AudioBus. The recording
// buffer is excluded from shipping binaries.
//
// Threading: RecordDrawCall is expected to be called from the main thread
// (or the engine's single render-task thread before pre-MVP multi-thread
// render submission lands). If multi-thread submission is added later,
// the buffer needs a mutex.
// ============================================================================
namespace Zenith_RenderBus
{
	struct DrawCall
	{
		const char*           m_szName        = nullptr; // Caller-owned literal / interned string.
		Zenith_Maths::Matrix4 m_xWorldMatrix  = Zenith_Maths::Matrix4(1.0f);
		u_int                 m_uVertexCount  = 0;
		u_int                 m_uMaterialId   = 0; // Caller-defined id; 0 means "unassigned".
		u_int                 m_uFrame        = 0;
	};

	// Record a draw-call submission. Cheap; safe to call from main-thread
	// render code. Args are passed by value/const-ref so the caller can use
	// stack temporaries.
	//
	// In shipping builds this is a no-op stub. Callers do not need to guard
	// with #ifdef.
	void RecordDrawCall(const char* szName,
	                    const Zenith_Maths::Matrix4& xWorldMatrix,
	                    u_int uVertexCount,
	                    u_int uMaterialId);

#ifdef ZENITH_INPUT_SIMULATOR
	// Returns the recorded draw calls since the last Clear. The reference
	// is valid until the next Clear / RecordDrawCall call (vector may
	// reallocate).
	const Zenith_Vector<DrawCall>& GetSubmittedDrawCallsForTest();

	// Reset the recording buffer. Typically called at the start of a test's
	// Step phase to scope recording to the phase under test.
	void ClearDrawCallsForTest();

	// Tick the frame counter recorded in DrawCall::m_uFrame. Called by the
	// engine's render task at end-of-frame in test builds; game code does
	// not call this directly.
	void AdvanceFrameForTest();
#endif
}
