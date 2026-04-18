#include "Zenith.h"
#include "Flux/Flux_PerFrame.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

// Subscriber-tally static assert. Every subsystem that calls
// RegisterBeginFrameCallback / RegisterEndFrameCallback bumps the tally here
// (plus inline comment at the register site). The build fails if
// FLUX_MAX_PERFRAME_CALLBACKS was left too small when a new subscriber was
// added — turning "runtime assert 50 frames in" into "compile error at the
// commit that introduced the overflow".
//
// Current subscribers:
//   Begin: 1 — Zenith_Vulkan::OnFluxPerFrameBegin (fence wait, descriptor
//              pool reset, typed deletion queue drain, scratch-offset reset)
//   End:   1 — Zenith_Vulkan_MemoryManager::OnFluxPerFrameEnd (deferred-VRAM
//              deletion clock advance)
// Total: 2 per side. Cap is 4 either side (shared constant) — two slots of
// headroom for hot-reload / profiler hooks before this static_assert needs
// to bump the cap in ZenithConfig.h.
constexpr u_int FLUX_PERFRAME_BEGIN_SUBSCRIBER_TALLY = 1;
constexpr u_int FLUX_PERFRAME_END_SUBSCRIBER_TALLY   = 1;
static_assert(FLUX_MAX_PERFRAME_CALLBACKS >= FLUX_PERFRAME_BEGIN_SUBSCRIBER_TALLY,
	"FLUX_MAX_PERFRAME_CALLBACKS smaller than the counted begin-subscriber tally. Bump the config constant.");
static_assert(FLUX_MAX_PERFRAME_CALLBACKS >= FLUX_PERFRAME_END_SUBSCRIBER_TALLY,
	"FLUX_MAX_PERFRAME_CALLBACKS smaller than the counted end-subscriber tally. Bump the config constant.");

u_int Flux_PerFrame::s_uFrameCounter = 0;

Flux_PerFrame::OnFrameBeginFunc Flux_PerFrame::s_apfnBeginCallbacks[FLUX_MAX_PERFRAME_CALLBACKS] = {};
void*                           Flux_PerFrame::s_apBeginUserData   [FLUX_MAX_PERFRAME_CALLBACKS] = {};
u_int                           Flux_PerFrame::s_uNumBeginCallbacks = 0;

Flux_PerFrame::OnFrameEndFunc   Flux_PerFrame::s_apfnEndCallbacks[FLUX_MAX_PERFRAME_CALLBACKS] = {};
void*                           Flux_PerFrame::s_apEndUserData   [FLUX_MAX_PERFRAME_CALLBACKS] = {};
u_int                           Flux_PerFrame::s_uNumEndCallbacks = 0;

void Flux_PerFrame::Initialise()
{
	s_uFrameCounter = 0;
	s_uNumBeginCallbacks = 0;
	s_uNumEndCallbacks = 0;
}

void Flux_PerFrame::Shutdown()
{
	// Frame counter is intentionally NOT reset — by the time Shutdown runs the
	// per-resource deferred-deletion ring has already drained. Resetting the
	// counter here would do nothing useful. Just clear the callback arrays so
	// a subsequent Initialise() starts from a known empty state.
	s_uNumBeginCallbacks = 0;
	s_uNumEndCallbacks = 0;
}

void Flux_PerFrame::RegisterBeginFrameCallback(OnFrameBeginFunc pfn, void* pUserData)
{
	// The callback arrays are raw statics with no mutex — registration must
	// happen on the main thread, matching the codebase-wide convention that
	// mutating global state is main-thread-only (see MEMORY.md). Unit tests
	// that exercise the registration path do so from the main thread too
	// (RunAllTests is called from Zenith_Main.cpp), so this assertion doesn't
	// conflict with them.
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"Flux_PerFrame::RegisterBeginFrameCallback must be called from the main thread — callback arrays have no mutex");
	Zenith_Assert(pfn != nullptr, "Flux_PerFrame::RegisterBeginFrameCallback: null function pointer");
	Zenith_Assert(s_uNumBeginCallbacks < FLUX_MAX_PERFRAME_CALLBACKS,
		"Flux_PerFrame: begin-frame callback array overflow (max %u). Increase FLUX_MAX_PERFRAME_CALLBACKS in ZenithConfig.h.",
		static_cast<u_int>(FLUX_MAX_PERFRAME_CALLBACKS));
	s_apfnBeginCallbacks[s_uNumBeginCallbacks] = pfn;
	s_apBeginUserData   [s_uNumBeginCallbacks] = pUserData;
	s_uNumBeginCallbacks++;
}

void Flux_PerFrame::RegisterEndFrameCallback(OnFrameEndFunc pfn, void* pUserData)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"Flux_PerFrame::RegisterEndFrameCallback must be called from the main thread — callback arrays have no mutex");
	Zenith_Assert(pfn != nullptr, "Flux_PerFrame::RegisterEndFrameCallback: null function pointer");
	Zenith_Assert(s_uNumEndCallbacks < FLUX_MAX_PERFRAME_CALLBACKS,
		"Flux_PerFrame: end-frame callback array overflow (max %u). Increase FLUX_MAX_PERFRAME_CALLBACKS in ZenithConfig.h.",
		static_cast<u_int>(FLUX_MAX_PERFRAME_CALLBACKS));
	s_apfnEndCallbacks[s_uNumEndCallbacks] = pfn;
	s_apEndUserData   [s_uNumEndCallbacks] = pUserData;
	s_uNumEndCallbacks++;
}

// Cross-check the registered callback counts against the subscriber-tally
// constants on the first BeginFrame / FireEndCallbacks of the process. The
// FLUX_PERFRAME_*_SUBSCRIBER_TALLY constants and their static_asserts above
// catch the cap being too small at compile time — but those static_asserts
// can't notice when the tally constants themselves drift below the actual
// registered count (e.g. a new subscriber is added without bumping the
// tally). This runtime check turns silent drift into a loud assertion at
// startup. Guarded by ZENITH_DEBUG so it compiles out of shipping builds.
//
// Skipped entirely when EITHER callback array is empty. Both sides have at
// least one subscriber in a normally-initialised engine (Vulkan registers a
// begin callback, MemoryManager registers an end callback). A zero on either
// side means we're in a test scenario — `PerFrameScopedReset` clears both,
// and per-side tests like TestFluxPerFrameBeginCallbackFires register only
// begin OR only end. The check is meant to catch tally drift in production
// init, not policed asymmetric test setup.
#ifdef ZENITH_DEBUG
static void AssertSubscriberTalliesMatch(u_int uActualBegin, u_int uActualEnd)
{
	static bool s_bChecked = false;
	if (s_bChecked) return;
	if (uActualBegin == 0 || uActualEnd == 0) return; // test / pre-init state
	s_bChecked = true;
	Zenith_Assert(uActualBegin == FLUX_PERFRAME_BEGIN_SUBSCRIBER_TALLY,
		"Flux_PerFrame: registered %u begin-frame callbacks but FLUX_PERFRAME_BEGIN_SUBSCRIBER_TALLY says %u. "
		"A new subscriber was added without bumping the tally constant in Flux_PerFrame.cpp.",
		uActualBegin, FLUX_PERFRAME_BEGIN_SUBSCRIBER_TALLY);
	Zenith_Assert(uActualEnd == FLUX_PERFRAME_END_SUBSCRIBER_TALLY,
		"Flux_PerFrame: registered %u end-frame callbacks but FLUX_PERFRAME_END_SUBSCRIBER_TALLY says %u. "
		"A new subscriber was added without bumping the tally constant in Flux_PerFrame.cpp.",
		uActualEnd, FLUX_PERFRAME_END_SUBSCRIBER_TALLY);
}
#endif

void Flux_PerFrame::BeginFrame()
{
#ifdef ZENITH_DEBUG
	AssertSubscriberTalliesMatch(s_uNumBeginCallbacks, s_uNumEndCallbacks);
#endif
	const u_int uRingIndex = GetRingIndex();
	for (u_int u = 0; u < s_uNumBeginCallbacks; u++)
	{
		s_apfnBeginCallbacks[u](uRingIndex, s_apBeginUserData[u]);
	}
}

void Flux_PerFrame::EndFrame()
{
	FireEndCallbacks();
	AdvanceCounter();
}

void Flux_PerFrame::FireEndCallbacks()
{
#ifdef ZENITH_DEBUG
	AssertSubscriberTalliesMatch(s_uNumBeginCallbacks, s_uNumEndCallbacks);
#endif
	const u_int uRingIndex = GetRingIndex();
	for (u_int u = 0; u < s_uNumEndCallbacks; u++)
	{
		s_apfnEndCallbacks[u](uRingIndex, s_apEndUserData[u]);
	}
}

void Flux_PerFrame::AdvanceCounter()
{
	s_uFrameCounter++;
}

u_int Flux_PerFrame::GetRingIndex()
{
	return s_uFrameCounter % MAX_FRAMES_IN_FLIGHT;
}

u_int Flux_PerFrame::GetFrameCounter()
{
	return s_uFrameCounter;
}
