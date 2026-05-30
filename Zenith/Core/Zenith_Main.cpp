#include "Zenith.h"

#include "Core/Zenith_BenchECS.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "Profiling/Zenith_Profiling.h"

#include <cstdlib>
#include <cstring>

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#endif

extern void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions);

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
	Zenith_Window::Inititalise("Zenith", Zenith_GraphicsOptions::Get().m_uWindowWidth, Zenith_GraphicsOptions::Get().m_uWindowHeight);
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
		// --bench-sim: WS12 parallel-sim bench (serial vs parallel timing + an
		// in-bench byte-identity assert). Same GPU-free run-then-exit pattern.
		if (std::strcmp(__argv[i], "--bench-sim") == 0)
		{
			Zenith_BenchSim_Run();
			Zenith_Core::Zenith_FullShutdown();
			std::exit(0);
		}
		// --check-sim-determinism: WS12 heavy determinism soak (the gate/proof the
		// orchestrator runs explicitly). Runs serial-vs-parallel x16 over 256
		// Tween entities and prints a PASS/FAIL line; exit code is 0 on PASS, 1 on
		// FAIL so the soak can gate CI.
		if (std::strcmp(__argv[i], "--check-sim-determinism") == 0)
		{
			const bool bPass = Zenith_CheckSimDeterminism_Run();
			Zenith_Core::Zenith_FullShutdown();
			std::exit(bPass ? 0 : 1);
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

	while (!Zenith_Window::GetInstance()->ShouldClose())
	{
		g_xEngine.Profiling().BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		g_xEngine.Profiling().EndFrame();
	}

	Zenith_FullShutdown();
}
#endif
