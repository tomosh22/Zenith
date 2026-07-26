#pragma once

#ifdef ZENITH_INPUT_SIMULATOR

#include <cstdint>

/*
 * Zenith_AutomatedTest (EXT-3a)
 *
 * Headless test harness driven by command-line flags:
 *   --automated-test <name>    : pick one named test
 *   --automated-tests <a,b,c>  : run exactly these tests, in the given order,
 *                                in ONE process. Duplicates are allowed and
 *                                each occurrence runs ("A,A" is a self-
 *                                contamination probe) -- note both occurrences
 *                                write the same <name>.json, last-wins.
 *                                Manual-only tests run when named explicitly,
 *                                exactly as --automated-test has always done.
 *                                Any unknown name: every unknown is printed,
 *                                nothing runs, exit code 2.
 *   --all-automated-tests      : run every registered test sequentially in
 *                                ONE process (skips per-test boot ~20s)
 *   --batch-order <spec>       : reorder the --all-automated-tests suite.
 *                                "reverse", or "rotate:<N>" (rotate left by N,
 *                                normalised modulo the suite size, so
 *                                rotate:0 is registration order). Negative or
 *                                malformed specs are rejected with exit 2, as
 *                                is use without --all-automated-tests.
 *   --list-automated-tests     : print all registered test names + exit code 0
 *   --exit-after-frames <N>    : tick at most N frames then exit
 *   --test-results <path>      : write JSON {name, passed, frames, failures} to <path>
 *                                (single-test mode only)
 *   --test-results-dir <dir>   : in any MULTI-test mode (--all-automated-tests
 *                                or --automated-tests), write per-test JSON to
 *                                <dir>/<name>.json
 *   --fixed-dt <secs>          : wraps Zenith_InputSimulator::SetFixedDt
 *
 * --automated-test / --automated-tests / --all-automated-tests are mutually
 * exclusive: each one BUILDS the ordered execution list, so supplying more
 * than one is an ambiguous request (exit code 2), not a merge.
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
 *   2 = test name not found / no tests registered for batch mode /
 *       CLI usage error (mode flags combined, bad --batch-order spec)
 *   3 = INFRASTRUCTURE failure: the harness could not build a clean world for
 *       the next test (boot scene would not load, or the post-reset settle
 *       timed out). Distinct from 1 on purpose -- the remaining tests were NOT
 *       RUN, so this is a harness fault rather than N test failures. Also
 *       written to <results dir>/_infrastructure.json with the phase, reason
 *       and the test we were about to run, plus one
 *       {skipped:true, skipReason:"infrastructure"} record per unrun test.
 *
 * BETWEEN-TEST ISOLATION
 *
 * Every test -- batch member, batch-first, and single-test -- runs against a
 * world the ENGINE built, not one its predecessor left behind. Before each
 * test the harness normalises input + fixed-dt, calls
 * Zenith_SceneSystem::ResetWorldForNextTest (which destroys EVERY scene,
 * including the persistent one), runs engine hygiene for the few things no
 * scene owns, reloads the boot scene, and settles.
 *
 * A test therefore needs NO cleanup for anything owned by an entity, a scene,
 * or its own locals. It needs a Teardown (below) only for state it installed
 * that no lifecycle owns: a global toggle, an observer function pointer, a
 * pinned fixed-dt. Process statics holding WORLD state are the defect -- put
 * that state on a component or the scene instead.
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

	// Tests that READ PIXELS -- screenshots, bitmap asserts, A/B image captures
	// -- set this to true. A Null (GPU-less) build rasterises nothing, so the
	// harness SKIPS them there and they run only in a windowed Vulkan build.
	// Defaults to false, and that default is now the norm: everything else,
	// including code that merely touches Flux state, runs fine on the Null
	// backend (its calls are no-ops that still execute every callback).
	bool m_bRequiresGraphics    = false;

	// Manual-only tests are excluded from the --all-automated-tests batch
	// (what CI and run_dp_tests.ps1 use): the harness marks them SKIPPED
	// (counts as passed, JSON skipped=true) instead of running them. They are
	// long-running balance / seed-matrix harnesses (the DP personality
	// playthroughs, ~30-145s each) with no per-commit signal. A direct
	// --automated-test <name> still runs them in full -- as does the
	// seed-matrix tooling, which invokes them by name. Defaults to false.
	bool m_bManualOnly          = false;

	// Optional teardown. Runs EXACTLY ONCE per test that reached Setup, after
	// the pass/fail/timeout/skip determination, before the results JSON is
	// written and before the harness resets the world. Normal harness paths
	// only -- a process-fatal assert cannot run it.
	//
	// This is where a test undoes anything it installed that is NOT owned by an
	// entity, a scene, or the test's own locals: a global debug toggle it
	// flipped, an observer function pointer it installed, a fixed-dt it pinned.
	// Anything world-shaped needs no teardown -- the harness destroys the world
	// between tests.
	//
	// Deliberately the LAST member: the existing aggregate initializers are
	// positional with trailing members omitted, so a defaulted final member
	// leaves every one of them compiling unchanged.
	void (*m_pfnTeardown)()     = nullptr;
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

	// Runtime skip: a test's Setup calls this to skip itself when an environment
	// prerequisite is missing (e.g. generated game assets absent on a fresh CI
	// checkout, where the test would otherwise fail for lack of geometry rather
	// than a real regression). The harness finalises it as SKIPPED at the next
	// phase boundary -- same bucket as the graphics / manual-only skips (JSON
	// skipped=true, counts as passed), just decided at runtime instead of from a
	// struct flag. No-op if no test is currently mid-run.
	void RequestSkip(const char* szReason);

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
