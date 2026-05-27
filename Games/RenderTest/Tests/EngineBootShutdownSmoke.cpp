#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Zenith_SceneManager.h"

// Phase 1 of the Zenith_Engine refactor: headless boot/shutdown smoke
// test. Hosted in RenderTest because RenderTest is the primary
// per-phase verification gate (Execution Constraints in the refactor
// plan).
//
// Runs Initialise -> 1 frame -> Shutdown via the existing harness.
// The host runner invokes the test 10x in separate processes to verify
// cross-cycle invariants. We do NOT verify in-process re-init safety
// -- Zenith_Window::GetInstance and other process-level singletons
// aren't shaped for clean reset within one process (see
// "What Stays Static" in the refactor plan).
//
// The test deliberately does the bare minimum because the whole point
// is to verify the Zenith_Engine class can stand up + tear down
// cleanly through every subsystem Phase 0+ migrates. Setup runs AFTER
// engine init + scene load + Awake + Playing-mode + first OnStart
// flush -- the fact that we get here means boot succeeded. Step
// returns false immediately and Verify is a no-op pass; the meaningful
// gate is the green process exit code, which signals Initialise +
// MainLoop tick + Shutdown all completed without asserts.

namespace
{
	void Setup_EngineBootShutdownSmoke()
	{
		// Sanity: an active scene exists after the harness's bootstrap
		// path. If this fires, the boot sequence Phase 0 migrated
		// returned control with the scene system in a bad state.
		const Zenith_Scene xActive = g_xEngine.SceneRegistry().GetActiveScene();
		Zenith_Assert(xActive.IsValid(),
			"Phase 1 smoke: no active scene after engine boot.");
	}

	bool Step_EngineBootShutdownSmoke(int /*iFrame*/)
	{
		// One frame is enough: don't request more ticks. The harness
		// then drives Verify and shutdown.
		return false;
	}

	bool Verify_EngineBootShutdownSmoke()
	{
		// If we got here at all, Initialise + scene-load + at least one
		// MainLoop tick succeeded. The harness's shutdown path then
		// exercises Zenith_Engine::Shutdown (via Zenith_FullShutdown ->
		// Zenith_Core::Zenith_Shutdown -> g_xEngine.Shutdown). The
		// process exit code is the actual gate.
		return true;
	}
}

static const Zenith_AutomatedTest g_xEngineBootShutdownSmoke = {
	"EngineBootShutdownSmoke",
	&Setup_EngineBootShutdownSmoke,
	&Step_EngineBootShutdownSmoke,
	&Verify_EngineBootShutdownSmoke,
	1   // m_iMaxFrames -- one frame is enough
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xEngineBootShutdownSmoke);

#endif // ZENITH_INPUT_SIMULATOR
