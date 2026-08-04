#include "Zenith.h"

#include "Core/Zenith_BenchECS.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/Zenith_PlatformStdio.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Profiling/Zenith_Profiling.h"
#ifdef ZENITH_TOOLS
// The boot artifact embeds the automation steps executed so far — frame 1 runs step 1.
#include "Editor/Zenith_EditorAutomation.h"
#endif

#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#endif

// Writes every boot-profiling section, in the order the artifact presents them.
// Shared by stdout and the dump file so the two can never drift apart.
//
// Frame 1 is deliberately part of the PRIMARY artifact, not a footnote: automation
// steps execute inside the frame BEFORE submission and present, so whatever the game
// queued as step 1 (for Zenithmon, the navmesh bake) directly extends the time to
// first present. Reporting boot without it would understate the wait.
static void WriteBootProfileSections(FILE* pxFile)
{
	Zenith_Profiling& xProfiling = g_xEngine.Profiling();
	xProfiling.WriteBootReport(pxFile);
	xProfiling.WriteDisplayFrameZoneTable(pxFile, "Frame 1 (first production frame)");

#ifdef ZENITH_TOOLS
	g_xEngine.EditorAutomation().WriteStepsSoFar(pxFile);
#endif
}

// ONE idempotent coordinator, so no call site can lose the report. Whichever of the
// three fires first writes stdout AND the dump file exactly once:
//   (a) the main loop, on the first iteration after the first frame completes
//   (b) the --memory-capture loop, which exits without ever reaching the main loop
//   (c) Zenith_FullShutdown, covering --bench-ecs / --list-automated-tests / early
//       exits -- milestones that never happened simply print as N/A.
// Must run BEFORE Zenith_Shutdown in case (c): the profiler is gone after it.
// The coordinator's decision half, split out so the once-only contract is testable
// without writing files. Claims the latch and returns true on the first call that has
// a path; every later call — whatever its reason — returns false.
static bool ClaimBootProfileDump(const char* szPath, bool& bWrittenLatch)
{
	if (szPath == nullptr) return false;
	if (bWrittenLatch) return false;
	bWrittenLatch = true;
	return true;
}

static void TryWriteBootProfileDump(const char* szReason)
{
	const char* szPath = Zenith_CommandLine::GetBootProfileDumpPath();

	static bool ls_bWritten = false;
	if (!ClaimBootProfileDump(szPath, ls_bWritten)) return;

	Zenith_Log(LOG_CATEGORY_CORE, "Boot profile dump (%s) -> %s", szReason, szPath);

	WriteBootProfileSections(stdout);
	fflush(stdout);

	FILE* pxDumpFile = Zenith_PlatformStdio::OpenFile(szPath, "w");
	if (pxDumpFile != nullptr)
	{
		WriteBootProfileSections(pxDumpFile);
		fclose(pxDumpFile);
	}
}

// Phase 0: Zenith_Init / Zenith_Shutdown bodies moved into
// Zenith_Engine::Initialise / Shutdown (see Zenith_Engine.cpp). These
// stay as thin forwarders so every existing caller (Android_Main.cpp,
// AutomatedTest.cpp, this file's Zenith_Main below) keeps working.
void Zenith_Core::Zenith_Init(const Zenith_BootMarkerBundle* pxMarkers)
{
	g_xEngine.Initialise();

	// Boot is over. Sealing HERE (in the shared forwarder rather than in
	// Zenith_Main) covers Windows, Android and the automated-test driver alike, and
	// lands before --bench-ecs / --memory-capture by construction.
	g_xEngine.Profiling().EndBootCapture(pxMarkers);
}

void Zenith_Core::Zenith_Shutdown()
{
	g_xEngine.Shutdown();
}

// Single canonical "tear down everything Zenith_Main brought up" wrapper.
// Zenith_Init does NOT initialise the window (the window comes up before
// Init so dimensions are known to graphics options), and conversely
// Zenith_Shutdown does NOT destroy the window (so subsystems shutting down
// can still reach Zenith_Window::GetInstance during their own teardown).
// That ordering is fine for the steady-state main-loop exit, but early-
// exit paths (--list-automated-tests, test-not-found, no-tests-registered)
// previously had to know about BOTH steps and call them in sequence — which
// would silently rot if a future singleton was added with its own bracket.
// Funnel everything through this wrapper so the early-exit paths only need
// to call one function.
void Zenith_Core::Zenith_FullShutdown()
{
	// Last chance to emit the boot artifact: the early-exit paths (--bench-ecs,
	// --list-automated-tests, test-not-found) funnel through here and never reach the
	// main loop. No-op when the dump was already written, or never requested.
	TryWriteBootProfileDump("orderly shutdown");

	g_xEngine.Scenes().SetMainLoopRunning(false);
	Zenith_Shutdown();
	delete Zenith_Window::GetInstance();
}

#ifdef ZENITH_WINDOWS
void Zenith_Core::Zenith_Main()
{
	// Boot markers the engine cannot take itself: everything here happens before
	// Zenith_Init, so the profiler does not exist yet. Local storage handed to
	// Zenith_Init by pointer — nothing static, nothing outliving this frame.
	Zenith_BootMarkerBundle xBootMarkers;
	xBootMarkers.m_uProcessStartTicks = Zenith_Profiling_Detail::GetTimestamp();

	// Graphics options are populated inside Zenith_Init() for all platforms
	// but we need window dimensions before that, so call it here too (idempotent)
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());
	Zenith_CommandLine::Parse(__argc, __argv);

	xBootMarkers.Add("WindowCreateBegin", Zenith_Profiling_Detail::GetTimestamp());
	Zenith_Window::Initialise("Zenith", Zenith_GraphicsOptions::Get().m_uWindowWidth, Zenith_GraphicsOptions::Get().m_uWindowHeight);
	xBootMarkers.Add("WindowCreateEnd", Zenith_Profiling_Detail::GetTimestamp());

	Zenith_Init(&xBootMarkers);

	// One reach for the whole function (the loops below hit it several times each).
	Zenith_Profiling& xProfiling = g_xEngine.Profiling();

	// --bench-ecs: run the GPU-free ECS micro-benchmark once (after engine init
	// so the scene system / component registry are live) then exit cleanly.
	// Mirrors the run-then-exit pattern used by --list-automated-tests: go
	// through Zenith_FullShutdown so GPU/Jolt/audio/window resources release in
	// the normal order, then std::exit(0). When the flag is absent, behaviour is
	// completely unchanged.
	for (int i = 1; i < __argc; ++i)
	{
		if (std::strcmp(__argv[i], "--bench-ecs") == 0)
		{
			Zenith_BenchECS_Run();
			Zenith_Core::Zenith_FullShutdown();
			std::exit(0);
		}
	}

	// --exit-after-unit-tests: the boot ZENITH_TEST batch has already run and logged
	// its tally (it lives inside Zenith_Init, in InitialiseProject), so there is
	// nothing left for the unit gate to wait for. Exit through the normal ordered
	// teardown, exactly like --bench-ecs above.
	//
	// This flag exists because `--exit-after-frames N` looks like it should do this
	// and DOESN'T: it is consumed only inside Zenith_AutomatedTestRunner's Stepping
	// phase, so without an --automated-test selection flag the runner is inactive,
	// Tick() early-outs, and the flag is inert. Tools/run_unit_gate.ps1 passed it and
	// consequently idled until its watchdog killed the process — which is why every
	// unit-gate run cost the entire -TimeoutSec regardless of the result.
	//
	// Deliberately AFTER the --bench-ecs block: both are run-then-exit switches, and
	// if somebody passes both, the benchmark they explicitly asked for still runs.
	if (Zenith_CommandLine::IsExitAfterUnitTestsRequested())
	{
		// NOTE the wording: must NOT contain "unit tests complete". run_unit_gate.ps1
		// scrapes the tally with a case-insensitive match on that phrase and takes the
		// LAST hit, so a chattier message here silently shadows the real tally line.
		Zenith_Log(LOG_CATEGORY_UNITTEST, "--exit-after-unit-tests: boot batch finished, shutting down");
		Zenith_Core::Zenith_FullShutdown();
		std::exit(0);
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// EXT-3a: parse harness CLI flags AFTER Zenith_Init (so the registry has
	// been populated by static initializers and `--list-automated-tests` can
	// dump the full list) but BEFORE the main loop (so `--automated-test`
	// activates the runner before the first MainLoop tick).
	Zenith_AutomatedTestRunner::ParseCommandLine(__argc, __argv);
#endif

	// B4: signal that the main loop is now running. Read by
	// LoadScene to assert it's only invoked during
	// bootstrap (Zenith_Init or earlier), never from gameplay code.
	g_xEngine.Scenes().SetMainLoopRunning(true);

	// --profiling-dump: every N frames, dump the live profiling report (CPU zones
	// across all threads + per-pass GPU timings) both to stdout and to a truncated
	// "zenith_profiling_dump.txt" in the working dir (the file is fflush/fclose'd so
	// it survives even a hard process kill, unlike block-buffered stdout). The dump
	// runs BEFORE EndFrame, so it drains the in-flight frame's completed zones; the
	// drain is non-destructive (the events still publish at EndFrame), so the
	// in-engine timeline is unaffected.
	bool bProfilingDump = false;
	for (int i = 1; i < __argc; ++i)
		if (std::strcmp(__argv[i], "--profiling-dump") == 0) { bProfilingDump = true; break; }
	u_int uProfilingDumpFrame = 0;

#if ZENITH_MEMORY_TRACKING_ANY
	// --memory-dump: every 120 frames, dump the memory report (per-category + unified
	// sources) to stdout, a truncated zenith_memory_dump.txt, AND a machine-readable
	// zenith_memory_dump.csv (the feed the CI budget gate consumes). Mirrors --profiling-dump.
	bool bMemoryDump = false;
	for (int i = 1; i < __argc; ++i)
		if (std::strcmp(__argv[i], "--memory-dump") == 0) { bMemoryDump = true; break; }
	u_int uMemoryDumpFrame = 0;

	// --memory-capture[=N]: run N headless frames so allocations settle, dump the memory
	// report (stdout) + the machine-readable zenith_memory_dump.csv (the CI budget-gate
	// LIVE-mode feed), then exit cleanly through the normal teardown. Deterministic and
	// bounded — the Tier-A capture (CPU categories + Jolt; VRAM is 0 headless). Mirrors
	// the --bench-ecs run-then-exit pattern.
	for (int i = 1; i < __argc; ++i)
	{
		if (std::strncmp(__argv[i], "--memory-capture", 16) == 0)
		{
			u_int uCaptureFrames = 300;
			const char* pxEq = std::strchr(__argv[i], '=');
			if (pxEq != nullptr)
			{
				const int iN = std::atoi(pxEq + 1);
				if (iN > 0) { uCaptureFrames = static_cast<u_int>(iN); }
			}
			for (u_int f = 0; f < uCaptureFrames && !Zenith_Window::GetInstance()->ShouldClose(); ++f)
			{
				xProfiling.BeginFrame();
				Zenith_Core::Zenith_MainLoop();
				xProfiling.EndFrame();
				// Same trigger point as the main loop: right after the first frame
				// completes. This loop exits straight into teardown, so without its own
				// call site the artifact would only ever come from the shutdown
				// fallback — with a colder, less useful first-frame picture.
				if (f == 0) TryWriteBootProfileDump("first frame complete (--memory-capture)");
			}
			Zenith_MemoryManagement::WriteReport(stdout);
			fflush(stdout);
			FILE* pxCsv = Zenith_PlatformStdio::OpenFile("zenith_memory_dump.csv", "w");
			if (pxCsv != nullptr)
			{
				Zenith_MemoryManagement::WriteReportCSV(pxCsv);
				fclose(pxCsv);
			}
			Zenith_Core::Zenith_FullShutdown();
			std::exit(0);
		}
	}
#endif

	// Fires the boot artifact once, on the first loop iteration that completes a frame.
	bool bBootDumpPending = true;

	while (!Zenith_Window::GetInstance()->ShouldClose())
	{
		xProfiling.BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		if (bProfilingDump && (++uProfilingDumpFrame % 120u) == 0u)
		{
			xProfiling.WriteTextReport(stdout);
			fflush(stdout);
			FILE* pxDumpFile = Zenith_PlatformStdio::OpenFile("zenith_profiling_dump.txt", "w");
			if (pxDumpFile)
			{
				xProfiling.WriteTextReport(pxDumpFile);
				fclose(pxDumpFile);
			}
		}
#if ZENITH_MEMORY_TRACKING_ANY
		if (bMemoryDump && (++uMemoryDumpFrame % 120u) == 0u)
		{
			Zenith_MemoryManagement::WriteReport(stdout);
			fflush(stdout);
			FILE* pxMemTxt = Zenith_PlatformStdio::OpenFile("zenith_memory_dump.txt", "w");
			if (pxMemTxt)
			{
				Zenith_MemoryManagement::WriteReport(pxMemTxt);
				fclose(pxMemTxt);
			}
			FILE* pxMemCsv = Zenith_PlatformStdio::OpenFile("zenith_memory_dump.csv", "w");
			if (pxMemCsv)
			{
				Zenith_MemoryManagement::WriteReportCSV(pxMemCsv);
				fclose(pxMemCsv);
			}
		}
#endif
		xProfiling.EndFrame();

		// The boot artifact's first-frame section reads the DISPLAY snapshot, which
		// EndFrame above has just published — so this must come after it, not before.
		if (bBootDumpPending)
		{
			bBootDumpPending = false;
			TryWriteBootProfileDump("first frame complete");
		}
	}

	Zenith_FullShutdown();
}
#endif

// Hosted here (not in a game TU) because this TU is always linked — the entry point
// and the Zenith_Init/Shutdown forwarders live in it — so the static registrar
// cannot be dead-stripped.
#ifdef ZENITH_TESTING
#include "Core/Zenith_Main.Tests.inl"
#endif
