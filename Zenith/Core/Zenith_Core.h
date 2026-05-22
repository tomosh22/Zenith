#pragma once

// Zenith_Core namespace - main loop entry points and per-frame timer
// tick. Phase 2 of the engine refactor moved per-frame timing state
// (g_fDt / g_fTimePassed / g_xLastFrameTime) out of this namespace
// and onto Zenith_Engine::Frame() (FrameContext). All call sites that
// previously read Zenith_Core::GetDt() now read
// g_xEngine.Frame().GetDt() directly.
namespace Zenith_Core
{
#ifdef ZENITH_WINDOWS
	void Zenith_Main();
#endif
	void Zenith_Init();
	void Zenith_Shutdown();
	// Convenience wrapper: Shutdown + delete window singleton + any other
	// "init-only" teardown the steady-state main-loop exit performs. Use
	// from early-exit paths so they don't have to mirror the main-loop's
	// post-loop sequence by hand.
	void Zenith_FullShutdown();

	// Main loop and per-frame timer tick. UpdateTimers writes the
	// new frame's dt / accumulated time into g_xEngine.Frame() and
	// is called once per main-loop iteration.
	void Zenith_MainLoop();
	void UpdateTimers();
}
