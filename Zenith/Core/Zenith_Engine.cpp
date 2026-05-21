#include "Zenith.h"

#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/FrameContext.h"
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "TaskSystem/Zenith_TaskSystemImpl.h"
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

// The single engine instance. constinit guarantees zero static-init
// cost — every member of Zenith_Engine has a constant default
// initialiser (currently none, but the rule must hold as members are
// added in later phases).
constinit Zenith_Engine g_xEngine;

namespace
{
	bool HasCommandLineFlag(const char* szFlag)
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
}

FrameContext& Zenith_Engine::Frame()
{
	Zenith_Assert(m_pxFrame != nullptr,
		"Zenith_Engine::Frame() called before Initialise() or after Shutdown(). "
		"Frame timing is unavailable outside the engine lifetime.");
	return *m_pxFrame;
}

Zenith_MultithreadingImpl& Zenith_Engine::Threading()
{
	// NOTE: no assert here. Threading() is read on the IsMainThread
	// hot path (every Zenith_Assert that gates on main-thread fires
	// it), and the engine guarantees m_pxThreading exists before any
	// thread can call RegisterThread / IsMainThread (see Initialise).
	return *m_pxThreading;
}

Zenith_TaskSystemImpl& Zenith_Engine::Tasks()
{
	// No assert: SubmitTask is a hot path, and Zenith_Engine::Initialise
	// allocates m_pxTasks before Zenith_TaskSystem::Inititalise() (the
	// forwarder that brings worker threads online).
	return *m_pxTasks;
}

void Zenith_Engine::Initialise()
{
	// Phase 2: per-frame timing state lives here now. Construct
	// FIRST so any subsystem that reads dt during init sees a sane
	// zero. Last-frame time is bumped at the end of Initialise()
	// below, matching the historical Zenith_Init behaviour.
	Zenith_Assert(m_pxFrame == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFrame = new FrameContext();

	// Phase 3a: multithreading registry (thread-ID allocator +
	// main-thread ID) lives on the engine now. Allocate BEFORE
	// Zenith_Multithreading::RegisterThread(true) below, which reads
	// from g_xEngine.Threading() to issue the main thread's ID.
	Zenith_Assert(m_pxThreading == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxThreading = new Zenith_MultithreadingImpl();

	// Populate graphics options from the game project FIRST
	// Must happen before any Flux initialisation reads from Zenith_GraphicsOptions::Get()
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());

	// CRITICAL: Memory tracking must be initialized FIRST to capture all allocations
	Zenith_MemoryManagement::Initialise();

	Zenith_Multithreading::RegisterThread(true);
	Zenith_Profiling::Initialise();

	// Phase 3b: per-Engine TaskSystem state. Allocate BEFORE
	// Zenith_TaskSystem::Inititalise() below, which spawns worker
	// threads whose ThreadFunc reads from g_xEngine.Tasks().
	Zenith_Assert(m_pxTasks == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTasks = new Zenith_TaskSystemImpl();

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
	if (HasCommandLineFlag("--skip-tool-exports"))
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
	if (HasCommandLineFlag("--skip-unit-tests"))
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

	m_pxFrame->SetLastFrameTime(std::chrono::high_resolution_clock::now());
}

void Zenith_Engine::Shutdown()
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

	// 10. Free the per-Engine TaskSystem state. Must come AFTER
	// Zenith_TaskSystem::Shutdown above -- the forwarder there reads
	// from g_xEngine.Tasks() to drive Shutdown.
	delete m_pxTasks;
	m_pxTasks = nullptr;

	// 11. Tear down per-frame timing state. Done late so any
	// subsystem shutdown that needs to log dt or read accumulated
	// time still can.
	delete m_pxFrame;
	m_pxFrame = nullptr;

	// 12. Tear down the multithreading registry. Done after the task
	// system shutdown (step 9) so worker threads aren't still calling
	// IsMainThread while we're freeing the registry.
	delete m_pxThreading;
	m_pxThreading = nullptr;

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
}
