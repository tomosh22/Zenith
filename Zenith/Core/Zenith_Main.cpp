#include "Zenith.h"
#include "Zenith_Core.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#endif
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "Zenith_OS_Include.h"

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
	ExportAllMeshes();
	ExportAllTextures();
	//ExportHeightmap();
	ExportDefaultFontAtlas();  // Generate font atlas from TTF
	GenerateTestAssets();      // Generate procedural test assets (StickFigure, Tree)
#endif

	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::EarlyInitialise...");
	Flux::EarlyInitialise();
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Physics::Initialise...");
	Zenith_Physics::Initialise();
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: SceneManager::Initialise...");
	Zenith_SceneManager::Initialise();

	//#TO_TODO: move somewhere sensible
	{
		Flux_MemoryManager::BeginFrame();
		Zenith_AssetRegistry::InitializeGPUDependentAssets();  // Must be after Flux::EarlyInitialise()

		// Load cubemap texture
		Flux_Graphics::s_pxCubemapTexture = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
		if (Flux_Graphics::s_pxCubemapTexture)
		{
			Flux_Graphics::s_pxCubemapTexture->LoadCubemapFromFiles(
				ENGINE_ASSETS_DIR"Textures/Cubemap/px" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nx" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/py" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/ny" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/pz" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nz" ZENITH_TEXTURE_EXT
			);
		}

		// Load water normal texture
		Flux_Graphics::s_pxWaterNormalTexture = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Water/normal" ZENITH_TEXTURE_EXT);

		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::LateInitialise...");
	Flux::LateInitialise();

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions::RegisterDebugVariables();
	Zenith_Editor::Initialise();
	Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
	Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
	Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
	Zenith_DebugVariables::AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
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
	Zenith_TestRunner::Instance().RunAllTests();
#endif

#ifdef ZENITH_TOOLS
	// Initialize game-specific resources (geometry, materials, prefabs, particle configs)
	// Must be inside BeginFrame/EndFrame for GPU resource allocation
	Flux_MemoryManager::BeginFrame();
	Project_InitializeResources();
	Flux_MemoryManager::EndFrame(false);

	// Register automation steps and begin execution (one step per frame in main loop)
	Project_RegisterEditorAutomationSteps();
	Zenith_EditorAutomation::Begin();
#else
	// Non-tools: load pre-generated scene files
	// Run a tools build first to generate .zscen files
	Flux_MemoryManager::BeginFrame();
	Zenith_SceneManager::SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	Zenith_SceneManager::SetLoadingScene(true);
	Project_LoadInitialScene();
	Zenith_SceneManager::SetLoadingScene(false);
	Flux_MemoryManager::EndFrame(false);
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
	Flux_PlatformAPI::WaitForGPUIdle();

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

	// 6. Shutdown asset registry (unloads all assets)
	Zenith_AssetRegistry::Shutdown();

	// 7. Shutdown Flux (all subsystems + graphics + memory manager)
	Flux::Shutdown();

	// 8. Shutdown task system (terminates worker threads)
	Zenith_TaskSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
}

#ifdef ZENITH_WINDOWS
void Zenith_Core::Zenith_Main()
{
	// Graphics options are populated inside Zenith_Init() for all platforms
	// but we need window dimensions before that, so call it here too (idempotent)
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());
	Zenith_Window::Inititalise("Zenith", Zenith_GraphicsOptions::Get().m_uWindowWidth, Zenith_GraphicsOptions::Get().m_uWindowHeight);
	Zenith_Init();

	while (!Zenith_Window::GetInstance()->ShouldClose())
	{
		Zenith_Profiling::BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		Zenith_Profiling::EndFrame();
	}

	Zenith_Shutdown();
	delete Zenith_Window::GetInstance();
}
#endif
