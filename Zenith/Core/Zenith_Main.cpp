#include "Zenith.h"

#include "Core/Zenith_BenchECS.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Profiling/Zenith_Profiling.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#endif

// Phase 0: Zenith_Init / Zenith_Shutdown bodies moved into
// Zenith_Engine::Initialise / Shutdown (see Zenith_Engine.cpp). These
// stay as thin forwarders so every existing caller (Android_Main.cpp,
// AutomatedTest.cpp, this file's Zenith_Main below) keeps working.
void Zenith_Core::Zenith_Init()
{
	g_xEngine.Initialise();
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
	g_xEngine.Scenes().SetMainLoopRunning(false);
	Zenith_Shutdown();
	delete Zenith_Window::GetInstance();
}

#ifdef ZENITH_WINDOWS
void Zenith_Core::Zenith_Main()
{
	// Graphics options are populated inside Zenith_Init() for all platforms
	// but we need window dimensions before that, so call it here too (idempotent)
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());
	Zenith_CommandLine::Parse(__argc, __argv);
	Zenith_Window::Initialise("Zenith", Zenith_GraphicsOptions::Get().m_uWindowWidth, Zenith_GraphicsOptions::Get().m_uWindowHeight);
	Zenith_Init();

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
				g_xEngine.Profiling().BeginFrame();
				Zenith_Core::Zenith_MainLoop();
				g_xEngine.Profiling().EndFrame();
			}
			Zenith_MemoryManagement::WriteReport(stdout);
			fflush(stdout);
			FILE* pxCsv = nullptr;
			fopen_s(&pxCsv, "zenith_memory_dump.csv", "w");
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

	while (!Zenith_Window::GetInstance()->ShouldClose())
	{
		g_xEngine.Profiling().BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		if (bProfilingDump && (++uProfilingDumpFrame % 120u) == 0u)
		{
			g_xEngine.Profiling().WriteTextReport(stdout);
			fflush(stdout);
			FILE* pxDumpFile = nullptr;
			fopen_s(&pxDumpFile, "zenith_profiling_dump.txt", "w");
			if (pxDumpFile)
			{
				g_xEngine.Profiling().WriteTextReport(pxDumpFile);
				fclose(pxDumpFile);
			}
		}
#if ZENITH_MEMORY_TRACKING_ANY
		if (bMemoryDump && (++uMemoryDumpFrame % 120u) == 0u)
		{
			Zenith_MemoryManagement::WriteReport(stdout);
			fflush(stdout);
			FILE* pxMemTxt = nullptr;
			fopen_s(&pxMemTxt, "zenith_memory_dump.txt", "w");
			if (pxMemTxt)
			{
				Zenith_MemoryManagement::WriteReport(pxMemTxt);
				fclose(pxMemTxt);
			}
			FILE* pxMemCsv = nullptr;
			fopen_s(&pxMemCsv, "zenith_memory_dump.csv", "w");
			if (pxMemCsv)
			{
				Zenith_MemoryManagement::WriteReportCSV(pxMemCsv);
				fclose(pxMemCsv);
			}
		}
#endif
		g_xEngine.Profiling().EndFrame();
	}

	Zenith_FullShutdown();
}
#endif
