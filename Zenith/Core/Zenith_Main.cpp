#include "Zenith.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Flux/Flux_Graphics.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#endif
#include "Physics/Zenith_Physics.h"
#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_WINDOWS
#include <cstring>
#endif

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#endif

#ifdef ZENITH_TOOLS
extern void ExportAllMeshes();
extern void ExportAllTextures();
extern void ExportHeightmap();
extern void ExportDefaultFontAtlas();
extern void GenerateTestAssets();
#endif

extern void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions);
extern const char* Project_GetGameAssetsDir();
extern void Project_RegisterScriptBehaviours();
extern void Project_Shutdown();

#ifdef ZENITH_TOOLS
extern void Project_InitializeResources();
extern void Project_RegisterEditorAutomationSteps();
#endif
extern void Project_LoadInitialScene();

static bool Zenith_HasCommandLineFlag(const char* szFlag)
{
#ifdef ZENITH_WINDOWS
	for (int i = 1; i < __argc; i++)
	{
		if (std::strcmp(__argv[i], szFlag) == 0)
			return true;
	}
#else
	(void)szFlag;
#endif
	return false;
}

void Zenith_Core::Zenith_Init()
{
	// Populate graphics options from the game project FIRST
	// Must happen before any Flux initialisation reads from Zenith_GraphicsOptions::Get()
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());

	// CRITICAL: Memory tracking must be initialized FIRST to capture all allocations
	Zenith_MemoryManagement::Initialise();

	Zenith_Multithreading::RegisterThread(true);
	Zenith_Profiling::Initialise();
	Zenith_TaskSystem::Inititalise();

	// Set asset directories before registry initialization
	// Game assets dir comes from the game project (each game defines GAME_ASSETS_DIR)
	Zenith_AssetRegistry::SetGameAssetsDir(Project_GetGameAssetsDir());
#ifdef ENGINE_ASSETS_DIR
	Zenith_AssetRegistry::SetEngineAssetsDir(ENGINE_ASSETS_DIR);
#else
	Zenith_AssetRegistry::SetEngineAssetsDir("./Zenith/Assets/");
#endif
	Zenith_AssetRegistry::Initialize();

#ifdef ZENITH_TOOLS
	if (Zenith_HasCommandLineFlag("--skip-tool-exports"))
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Tool asset exports skipped by --skip-tool-exports");
	}
	else
	{
		ExportAllMeshes();
		ExportAllTextures();
		//ExportHeightmap();
		ExportDefaultFontAtlas();  // Generate font atlas from TTF
		GenerateTestAssets();      // Generate procedural test assets (StickFigure, Tree)
	}
#endif

	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::EarlyInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::EarlyInitialise();
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Physics::Initialise...");
	Zenith_Physics::Initialise();
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: SceneManager::Initialise...");
	Zenith_SceneManager::Initialise();

	//#TO_TODO: move somewhere sensible
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
		Zenith_AssetRegistry::InitializeGPUDependentAssets();  // Must be after Flux::EarlyInitialise()

		// Load cubemap texture (pinned)
		if (Zenith_TextureAsset* pxCubemap = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
		{
			pxCubemap->LoadCubemapFromFiles(
				ENGINE_ASSETS_DIR"Textures/Cubemap/px" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nx" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/py" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/ny" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/pz" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nz" ZENITH_TEXTURE_EXT
			);
			Flux_Graphics::s_xCubemapTexture.Set(pxCubemap);
		}

		// Load water normal texture (pinned)
		if (Zenith_TextureAsset* pxWaterNormal = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Water/normal" ZENITH_TEXTURE_EXT))
		{
			Flux_Graphics::s_xWaterNormalTexture.Set(pxWaterNormal);
		}

		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::LateInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::LateInitialise();
	}

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions::RegisterDebugVariables();
	if (!Zenith_CommandLine::IsHeadless())
	{
		Zenith_Editor::Initialise();
		Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
		Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
		Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
		Zenith_DebugVariables::AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
	}
#endif

	Project_RegisterScriptBehaviours();

#ifdef ZENITH_TOOLS
	// Sync registered script behaviours to disk: write missing game:Scripts/<TypeName>.zscript
	// files for each behaviour that auto-registered via the ZENITH_BEHAVIOUR_TYPE_NAME macro,
	// and rename orphan .zscript files (whose behaviour is no longer registered) to .stale.
	// Must run AFTER static-init / Project_RegisterScriptBehaviours and BEFORE the test runner
	// (so tests can resolve .zscript assets via Zenith_AssetRegistry::Get) and before any scene load.
	Zenith_ScriptAsset::SyncRegisteredTypesToDisk();
#endif

	// Run unit tests BEFORE loading the game scene
	// This ensures tests don't corrupt game entities - scene loads fresh after tests
#ifdef ZENITH_TESTING
	if (Zenith_HasCommandLineFlag("--skip-unit-tests"))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "Unit tests skipped by --skip-unit-tests");
	}
	else
	{
		Zenith_TestRunner::Instance().RunAllTests();
	}
#endif

#ifdef ZENITH_TOOLS
	// Initialize game-specific resources (geometry, materials, prefabs, particle configs)
	// Must be inside BeginFrame/EndFrame for GPU resource allocation
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
	}
	Project_InitializeResources();
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::EndFrame(false);
	}

	// Register automation steps and begin execution (one step per frame in main loop)
	Project_RegisterEditorAutomationSteps();
	Zenith_EditorAutomation::Begin();
#else
	// Non-tools: load pre-generated scene files
	// Run a tools build first to generate .zscen files
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
	}
	Zenith_SceneManager::SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	{
		Zenith_SceneManager::LifecycleDeferralGuard xLoadingGuard(Zenith_SceneLifecycleScheduler::s_bIsLoadingScene);
		Project_LoadInitialScene();
	}
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Assert(Zenith_SceneManager::GetActiveScene().IsValid(),
		"No scene loaded. Run a ZENITH_TOOLS build first to generate .zscen files.");
#endif

	Zenith_Core::g_xLastFrameTime = std::chrono::high_resolution_clock::now();
}

void Zenith_Core::Zenith_Shutdown()
{
	//--------------------------------------------------------------------------
	// Shutdown sequence - reverse order of initialization
	// Critical: Must wait for GPU before destroying resources it's using
	//--------------------------------------------------------------------------
	Zenith_Log(LOG_CATEGORY_CORE, "Beginning shutdown sequence...");

	// 1. Wait for GPU to finish all pending work
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_PlatformAPI::WaitForGPUIdle();
	}

#ifdef ZENITH_TOOLS
	// 2. Shutdown editor (processes pending deletions, cleans up editor state)
	Zenith_Editor::Shutdown();
#endif

	// 3. Shutdown SceneManager (unloads all scenes, releases resources)
	// Must happen before physics (colliders need to remove bodies) and before
	// memory manager (model/mesh components hold VRAM handles)
	Zenith_SceneManager::Shutdown();

	// 4. Shutdown physics system
	Zenith_Physics::Shutdown();

	// 5. Project shutdown - clean up game-specific resources
	Project_Shutdown();

	// 6. Release Flux's asset-system references BEFORE the registry shuts down.
	// Flux statics hold TextureHandle / MaterialHandle defaults that must drop their
	// refs while the registry still owns its assets — Flux::Shutdown() runs too late.
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::ReleaseAssetReferences();
	}

	// 7. Shutdown asset registry (unloads all assets)
	Zenith_AssetRegistry::Shutdown();

	// 8. Shutdown Flux (all subsystems + graphics + memory manager)
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::Shutdown();
	}

	// 9. Shutdown task system (terminates worker threads)
	Zenith_TaskSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
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
	Zenith_SceneManager::SetMainLoopRunning(false);
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

#ifdef ZENITH_INPUT_SIMULATOR
	// EXT-3a: parse harness CLI flags AFTER Zenith_Init (so the registry has
	// been populated by static initializers and `--list-automated-tests` can
	// dump the full list) but BEFORE the main loop (so `--automated-test`
	// activates the runner before the first MainLoop tick).
	Zenith_AutomatedTestRunner::ParseCommandLine(__argc, __argv);
#endif

	// B4: signal that the main loop is now running. Read by
	// LoadSceneBlockingForBootstrap to assert it's only invoked during
	// bootstrap (Zenith_Init or earlier), never from gameplay code.
	Zenith_SceneManager::SetMainLoopRunning(true);

	while (!Zenith_Window::GetInstance()->ShouldClose())
	{
		Zenith_Profiling::BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		Zenith_Profiling::EndFrame();
	}

	Zenith_FullShutdown();
}
#endif
