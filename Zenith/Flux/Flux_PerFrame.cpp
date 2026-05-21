#include "Zenith.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"
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

// Phase 6a-1: per-frame state moved onto Flux_RendererImpl held by
// Zenith_Engine. Methods below dereference g_xEngine.FluxRenderer().m_xXxx.

void Flux_PerFrame::Initialise()
{
	g_xEngine.FluxRenderer().m_uFrameCounter = 0;
	g_xEngine.FluxRenderer().m_uNumBeginCallbacks = 0;
	g_xEngine.FluxRenderer().m_uNumEndCallbacks = 0;
}

void Flux_PerFrame::Shutdown()
{
	// Frame counter is intentionally NOT reset — by the time Shutdown runs the
	// per-resource deferred-deletion ring has already drained. Resetting the
	// counter here would do nothing useful. Just clear the callback arrays so
	// a subsequent Initialise() starts from a known empty state.
	g_xEngine.FluxRenderer().m_uNumBeginCallbacks = 0;
	g_xEngine.FluxRenderer().m_uNumEndCallbacks = 0;
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
	Zenith_Assert(g_xEngine.FluxRenderer().m_uNumBeginCallbacks < FLUX_MAX_PERFRAME_CALLBACKS,
		"Flux_PerFrame: begin-frame callback array overflow (max %u). Increase FLUX_MAX_PERFRAME_CALLBACKS in ZenithConfig.h.",
		static_cast<u_int>(FLUX_MAX_PERFRAME_CALLBACKS));
	g_xEngine.FluxRenderer().m_apfnBeginCallbacks[g_xEngine.FluxRenderer().m_uNumBeginCallbacks] = pfn;
	g_xEngine.FluxRenderer().m_apBeginUserData   [g_xEngine.FluxRenderer().m_uNumBeginCallbacks] = pUserData;
	g_xEngine.FluxRenderer().m_uNumBeginCallbacks++;
}

void Flux_PerFrame::RegisterEndFrameCallback(OnFrameEndFunc pfn, void* pUserData)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"Flux_PerFrame::RegisterEndFrameCallback must be called from the main thread — callback arrays have no mutex");
	Zenith_Assert(pfn != nullptr, "Flux_PerFrame::RegisterEndFrameCallback: null function pointer");
	Zenith_Assert(g_xEngine.FluxRenderer().m_uNumEndCallbacks < FLUX_MAX_PERFRAME_CALLBACKS,
		"Flux_PerFrame: end-frame callback array overflow (max %u). Increase FLUX_MAX_PERFRAME_CALLBACKS in ZenithConfig.h.",
		static_cast<u_int>(FLUX_MAX_PERFRAME_CALLBACKS));
	g_xEngine.FluxRenderer().m_apfnEndCallbacks[g_xEngine.FluxRenderer().m_uNumEndCallbacks] = pfn;
	g_xEngine.FluxRenderer().m_apEndUserData   [g_xEngine.FluxRenderer().m_uNumEndCallbacks] = pUserData;
	g_xEngine.FluxRenderer().m_uNumEndCallbacks++;
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
	AssertSubscriberTalliesMatch(g_xEngine.FluxRenderer().m_uNumBeginCallbacks, g_xEngine.FluxRenderer().m_uNumEndCallbacks);
#endif
	const u_int uRingIndex = GetRingIndex();
	for (u_int u = 0; u < g_xEngine.FluxRenderer().m_uNumBeginCallbacks; u++)
	{
		g_xEngine.FluxRenderer().m_apfnBeginCallbacks[u](uRingIndex, g_xEngine.FluxRenderer().m_apBeginUserData[u]);
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
	AssertSubscriberTalliesMatch(g_xEngine.FluxRenderer().m_uNumBeginCallbacks, g_xEngine.FluxRenderer().m_uNumEndCallbacks);
#endif
	const u_int uRingIndex = GetRingIndex();
	for (u_int u = 0; u < g_xEngine.FluxRenderer().m_uNumEndCallbacks; u++)
	{
		g_xEngine.FluxRenderer().m_apfnEndCallbacks[u](uRingIndex, g_xEngine.FluxRenderer().m_apEndUserData[u]);
	}
}

void Flux_PerFrame::AdvanceCounter()
{
	g_xEngine.FluxRenderer().m_uFrameCounter++;
}

u_int Flux_PerFrame::GetRingIndex()
{
	return g_xEngine.FluxRenderer().m_uFrameCounter % MAX_FRAMES_IN_FLIGHT;
}

u_int Flux_PerFrame::GetFrameCounter()
{
	return g_xEngine.FluxRenderer().m_uFrameCounter;
}
