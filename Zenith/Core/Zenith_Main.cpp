#include "Zenith.h"
#include "Zenith_Core.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/Skybox/Flux_Skybox.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
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
extern void Project_RegisterScriptBehaviours();
extern void Project_CreateScenes();
extern void Project_LoadInitialScene();
extern void Project_Shutdown();

static Zenith_GraphicsOptions s_xGraphicsOptions;

void Zenith_Core::Zenith_Init()
{
	// CRITICAL: Memory tracking must be initialized FIRST to capture all allocations
	Zenith_MemoryManagement::Initialise();

	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread(true);
	Zenith_TaskSystem::Inititalise();

	// Set asset directories before registry initialization
#ifdef GAME_ASSETS_DIR
	Zenith_AssetRegistry::SetGameAssetsDir(GAME_ASSETS_DIR);
#else
	Zenith_AssetRegistry::SetGameAssetsDir("./Assets/");
#endif
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

	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();
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
		Flux_Graphics::s_pxWaterNormalTexture = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/water/normal" ZENITH_TEXTURE_EXT);

		Flux_MemoryManager::EndFrame(false);
	}
	Flux::LateInitialise();

	// Apply project graphics options
	Flux_Fog::s_bEnabled = s_xGraphicsOptions.m_bFogEnabled;
	Flux_SSR::s_bEnabled = s_xGraphicsOptions.m_bSSREnabled;
	Flux_SSAO::s_bEnabled = s_xGraphicsOptions.m_bSSAOEnabled;
	Flux_SSGI::s_bEnabled = s_xGraphicsOptions.m_bSSGIEnabled;
	Flux_Skybox::s_bEnabled = s_xGraphicsOptions.m_bSkyboxEnabled;
	Flux_Skybox::s_xOverrideColour = s_xGraphicsOptions.m_xSkyboxColour;

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_Editor::Initialise();
	Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
	Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
	Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
	Zenith_DebugVariables::AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
#endif

	Project_RegisterScriptBehaviours();

	// Run unit tests BEFORE loading the game scene
	// This ensures tests don't corrupt game entities - scene loads fresh after tests
	Zenith_UnitTests::RunAllTests();

	// Create and register all project scenes (writes .zscen files, populates build index registry)
	Flux_MemoryManager::BeginFrame();
	Project_CreateScenes();
	Flux_MemoryManager::EndFrame(false);

	Flux_MemoryManager::BeginFrame();
	//#TO_TODO: Flux_Graphics::UploadFrameConstants crashes if we don't do this because there is no game camera
	Zenith_SceneManager::SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	Zenith_SceneManager::SetLoadingScene(true);
	Project_LoadInitialScene();
	Zenith_SceneManager::SetLoadingScene(false);
	Flux_MemoryManager::EndFrame(false);

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

void Zenith_Core::Zenith_Main()
{
	Project_SetGraphicsOptions(s_xGraphicsOptions);
	Zenith_Window::Inititalise("Zenith", s_xGraphicsOptions.m_uWindowWidth, s_xGraphicsOptions.m_uWindowHeight);
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
