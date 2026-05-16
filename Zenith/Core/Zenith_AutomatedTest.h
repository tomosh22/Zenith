#pragma once

#ifdef ZENITH_INPUT_SIMULATOR

#include <cstdint>

/*
 * Zenith_AutomatedTest (EXT-3a)
 *
 * Headless test harness driven by command-line flags:
 *   --automated-test <name>    : pick one named test
 *   --all-automated-tests      : run every registered test sequentially in
 *                                ONE process (skips per-test boot ~20s)
 *   --list-automated-tests     : print all registered test names + exit code 0
 *   --exit-after-frames <N>    : tick at most N frames then exit
 *   --test-results <path>      : write JSON {name, passed, frames, failures} to <path>
 *                                (single-test mode only)
 *   --test-results-dir <dir>   : in --all-automated-tests mode, write per-test
 *                                JSON to <dir>/<name>.json
 *   --fixed-dt <secs>          : wraps Zenith_InputSimulator::SetFixedDt
 *
 * Tests register via static-init linked-list, mirroring the ZENITH_TEST macro
 * pattern in Zenith_TestFramework.h. Each test provides:
 *   - Setup()  : runs after scene load + Awake + Playing-mode + 1 OnStart-flushing
 *                tick. Configures simulator state (held keys, fixed mouse pos…)
 *                and captures the entities it will assert against.
 *   - Step(i)  : returns true to continue ticking, false to stop. Called every
 *                main-loop iteration after the simulator's BeginTestFrame has
 *                injected one-shot key/click presses.
 *   - Verify() : returns pass/fail. Called once after Step terminates (or
 *                m_iMaxFrames is hit). The "failures" JSON field is currently
 *                always an empty array — there is no per-test mechanism for
 *                structured failure messages yet, only the pass/fail bool.
 *                Tests that need detail today print to stdout instead.
 *
 * Process exit code:
 *   0 = pass (or all batch tests passed)
 *   1 = test failed (or any batch test failed; Setup/Verify reported failures)
 *   2 = test name not found / no tests registered for batch mode
 *   3 = harness setup error (could not enter Playing mode etc.)
 *
 * Wave-3 integration tests (Games/DevilsPlayground/Tests/) consume this same
 * registry. The runner is engine-side because the boot ordering it owns
 * (automation drain → scene-load+Awake → Playing → first-frame OnStart flush)
 * spans subsystems no single game module can cleanly observe.
 */

struct Zenith_AutomatedTest
{
	const char* m_szName        = nullptr;
	void (*m_pfnSetup)()        = nullptr;          // optional
	bool (*m_pfnStep)(int iFrame) = nullptr;        // returns true=keep ticking
	bool (*m_pfnVerify)()       = nullptr;          // pass/fail
	int  m_iMaxFrames           = 600;              // 10 seconds at fixed 60Hz

	// Tests that depend on Flux being initialised (material assets, fog hole
	// tables, render hooks, visual wiring) set this to true. The harness skips
	// them when Zenith_CommandLine::IsHeadless() is true so they don't crash
	// on null Flux state. Defaults to false: most tests are gameplay logic
	// (entity state, possession, events) and run fine headless.
	bool m_bRequiresGraphics    = false;
};

// Linked-list node populated by the registrar macro. Kept writable (no
// const) so the registrar can chain it without const_cast — important
// because the compiler may place the user's `static const Zenith_AutomatedTest`
// in read-only memory. Also used as a side-channel for the harness to
// record per-test runtime back onto the node (the test struct itself is
// in read-only memory and can't be mutated).
struct Zenith_AutomatedTestNode
{
	const Zenith_AutomatedTest* m_pxTest = nullptr;
	Zenith_AutomatedTestNode*   m_pxNext = nullptr;
	// Wall-clock duration of the most-recent run of this test, measured
	// from start of Setup() to end of Verify(). Updated by the harness
	// in VerifyAndExit. -1.0f means "never run / not yet measured".
	// Sub-millisecond resolution (we use high_resolution_clock).
	//
	// `mutable` because the harness reaches the node via a
	// `const Zenith_AutomatedTestNode*` (registry walk type) -- the
	// timing write is conceptually metadata, not part of the
	// registration payload, so const-correctness vs the test struct
	// itself stays intact.
	mutable float               m_fLastDurationMs = -1.0f;
};

namespace Zenith_AutomatedTestRunner
{
	// Static-init registration. The macro below builds a globally-linked
	// instance that calls this from a constructor — order doesn't matter
	// because we walk the list at runtime.
	void RegisterNode(Zenith_AutomatedTestNode* pxNode);

	// CLI hooks called from Zenith_Main during boot.
	void ParseCommandLine(int argc, char** argv);

	// True iff a test was selected via --automated-test (drives main-loop hook).
	bool IsActive();

	// Per-frame tick. Called from Zenith_Core::Zenith_MainLoop AFTER the editor
	// automation queue / scene-load callbacks fire, before scene-update logic.
	// Returns false when the test has finished (and Zenith_Window::RequestClose
	// has been called); the caller may continue ticking until the window
	// shutdown path runs.
	bool Tick();

	// Set/read the pending exit code. main() reads this after Zenith_Shutdown.
	void SetPendingExitCode(int iCode);
	int  GetPendingExitCode();

	// --list-automated-tests path
	void PrintRegisteredTests();

	// Reset registrations (used by unit tests of the harness itself).
	void ResetRegistry_TEST_ONLY();

	// Per-game cleanup hook fired between tests in --all-automated-tests mode.
	// Called AFTER the harness reloads the boot scene (so scene-managed
	// side-tables are already cleared), giving the game a chance to reset
	// any persistent globals that aren't tied to entity lifetimes (e.g.
	// possessed-player state, win-condition masks, cached lookup tables).
	// Multiple hooks may be registered; they fire in registration order.
	using BetweenTestsHook = void (*)();
	void RegisterBetweenTestsHook(BetweenTestsHook pfn);
}

// Static-init registration helper. Place in any .cpp that defines a
// Zenith_AutomatedTest g_xMyTest; instance:
//
//   static const Zenith_AutomatedTest g_xMyTest = { "MyTest", &Setup, &Step, &Verify, 120 };
//   ZENITH_AUTOMATED_TEST_REGISTER(g_xMyTest);
//
// Same idea as TilePuzzle's auto-test except with public registry surface so
// game-side .cpp files can register without including game-private headers.
#define ZENITH_AUTOMATED_TEST_REGISTER(testVar)                                        \
	namespace                                                                          \
	{                                                                                  \
		static Zenith_AutomatedTestNode testVar##_regnode = { &(testVar), nullptr };   \
		struct testVar##_AutoRegistrar                                                 \
		{                                                                              \
			testVar##_AutoRegistrar()                                                  \
			{                                                                          \
				Zenith_AutomatedTestRunner::RegisterNode(&(testVar##_regnode));        \
			}                                                                          \
		};                                                                             \
		static testVar##_AutoRegistrar testVar##_autoreg_instance;                     \
	}

#endif // ZENITH_INPUT_SIMULATOR
