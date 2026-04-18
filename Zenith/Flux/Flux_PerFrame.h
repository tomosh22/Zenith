#pragma once

#include "Core/ZenithConfig.h"

// Platform-neutral per-frame ring scheduling. Owns the monotonic frame counter
// and the ring index (frame counter mod MAX_FRAMES_IN_FLIGHT). Backends
// register begin/end-frame callbacks during their Initialise(); Flux_PerFrame
// fires them in registration order each frame. Today's two subscribers are
// the Vulkan backend (begin: wait fence, reset descriptor pools, drain typed
// deletion queues) and the memory manager (end: ProcessDeferredDeletions
// decrements per-resource counters). The ring index passed to each callback
// matches GetRingIndex() at call time.
//
// Counter advance happens inside EndFrame, AFTER end callbacks fire. This
// preserves the existing ordering: begin callbacks see the same ring index
// that end callbacks saw at the previous EndFrame, so the per-frame state
// they touch is the same slot.
//
// Skipped-frame semantics: Flux_PerFrame::BeginFrame and ::EndFrame are
// called unconditionally each main-loop iteration. On a skipped frame the
// counter still advances; the per-slot fence is signalled by the previous
// non-skipped use of that slot, so wait-for-fence inside the Vulkan begin
// callback succeeds instantly. The deferred-deletion counter inside
// MemoryManager::ProcessDeferredDeletions also advances, matching the
// pre-extraction behaviour where MemoryManager::EndFrame ran on every
// iteration including skipped frames.
class Flux_PerFrame
{
public:
	// Callback signature. uRingIndex == GetRingIndex() at call time.
	// pUserData is the pointer supplied to Register*FrameCallback — current
	// engine subscribers pass nullptr, but the parameter is retained for
	// future subsystems that want per-callback state (e.g. profiling hooks
	// that need a pointer to a stats struct, hot-reload hooks that carry a
	// context handle). Kept typed as void* rather than a template to keep
	// the registration API non-templated and backend-agnostic.
	// Names are class-scoped (Flux_PerFrame::OnFrameBeginFunc) so they use
	// the short `On…Func` form instead of the file-scope `Flux_…_OnFunc`
	// prefix convention — disambiguation is supplied by the enclosing class.
	using OnFrameBeginFunc = void(*)(u_int uRingIndex, void* pUserData);
	using OnFrameEndFunc   = void(*)(u_int uRingIndex, void* pUserData);

	// Resets state. Counter starts at 0; callback arrays start empty.
	// Call this BEFORE backend Initialise() so backends can register their
	// callbacks during their own setup.
	static void Initialise();

	// Clears callbacks. Frame counter is left as-is (deferred-deletion etc.
	// have already settled by this point in shutdown ordering).
	static void Shutdown();

	// Backends register here at Initialise() time. Registration order is
	// invocation order: register the most "load-bearing" callback first
	// (e.g. Vulkan PerFrame begin must run before any other begin callback
	// touches the ring slot). Asserts on overflow of FLUX_MAX_PERFRAME_CALLBACKS.
	static void RegisterBeginFrameCallback(OnFrameBeginFunc pfn, void* pUserData);
	static void RegisterEndFrameCallback  (OnFrameEndFunc   pfn, void* pUserData);

	// Fires registered callbacks in registration order with the current ring
	// index. BeginFrame fires the begin set. EndFrame fires the end set AND
	// advances the counter — the normal per-frame path. Use the two-part
	// FireEndCallbacks / AdvanceCounter decomposition below from the
	// skipped-frame path in Zenith_Core::Zenith_MainLoop where the deferred-
	// deletion clock should still tick but the ring counter must not move
	// (a sequence of skipped frames should not advance past valid fences).
	static void BeginFrame();
	static void EndFrame();

	// Skipped-frame helpers — FireEndCallbacks drives the deferred-VRAM-deletion
	// clock (and any other end-frame subscribers) without advancing the ring
	// counter. Zenith_Core::Zenith_MainLoop's swapchain-acquire-failed branch
	// calls this (instead of EndFrame) so a rapid-resize sequence of skipped
	// frames doesn't wrap the ring counter past valid fences.
	static void FireEndCallbacks();
	static void AdvanceCounter();

	// Current ring slot in [0, MAX_FRAMES_IN_FLIGHT). Backends use this
	// rather than maintaining their own ring counter.
	static u_int GetRingIndex();

	// Monotonic frame number since Initialise(). 32-bit; wraps after ~828
	// days at 60 FPS. The wrap is harmless because GetRingIndex() takes the
	// modulo and per-resource deferred-deletion counters are decremented
	// (not compared by absolute frame number).
	static u_int GetFrameCounter();

private:
	static u_int s_uFrameCounter;

	static OnFrameBeginFunc s_apfnBeginCallbacks[FLUX_MAX_PERFRAME_CALLBACKS];
	static void*            s_apBeginUserData   [FLUX_MAX_PERFRAME_CALLBACKS];
	static u_int            s_uNumBeginCallbacks;

	static OnFrameEndFunc   s_apfnEndCallbacks[FLUX_MAX_PERFRAME_CALLBACKS];
	static void*            s_apEndUserData   [FLUX_MAX_PERFRAME_CALLBACKS];
	static u_int            s_uNumEndCallbacks;

	// Unit tests reset state and inspect counters / callback arrays directly.
	friend class Zenith_UnitTests;
};
