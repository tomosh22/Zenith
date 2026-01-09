#include "Zenith.h"
#include "Core/Zenith_Core.h"
#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#endif
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Flux/Flux_Graphics.h"
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

#ifdef ZENITH_TOOLS
extern void ExportAllMeshes();
extern void ExportAllTextures();
extern void ExportHeightmap();
extern void ExportDefaultFontAtlas();
#endif


int main()
{
	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread(true);
	Zenith_TaskSystem::Inititalise();
	Zenith_UnitTests::RunAllTests();

#ifdef ZENITH_TOOLS
	ExportAllMeshes();
	//ExportAllTextures();
	//ExportHeightmap();
	ExportDefaultFontAtlas();  // Generate font atlas from TTF
#endif
	
	Zenith_MemoryManagement::Initialise();
	
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();

	//#TO_TODO: move somewhere sensible
	{
		Flux_MemoryManager::BeginFrame();
		Zenith_AssetHandler::TextureData xCubemapTexData = Zenith_AssetHandler::LoadTextureCubeFromFiles(
			ENGINE_ASSETS_DIR"Textures/Cubemap/px" ZENITH_TEXTURE_EXT,
			ENGINE_ASSETS_DIR"Textures/Cubemap/nx" ZENITH_TEXTURE_EXT,
			ENGINE_ASSETS_DIR"Textures/Cubemap/py" ZENITH_TEXTURE_EXT,
			ENGINE_ASSETS_DIR"Textures/Cubemap/ny" ZENITH_TEXTURE_EXT,
			ENGINE_ASSETS_DIR"Textures/Cubemap/pz" ZENITH_TEXTURE_EXT,
			ENGINE_ASSETS_DIR"Textures/Cubemap/nz" ZENITH_TEXTURE_EXT
		);
		Flux_Graphics::s_pxCubemapTexture = Zenith_AssetHandler::AddTexture(xCubemapTexData);
		xCubemapTexData.FreeAllocatedData();

		Zenith_AssetHandler::TextureData xWaterNormalTexData = Zenith_AssetHandler::LoadTexture2DFromFile(ENGINE_ASSETS_DIR"Textures/water/normal" ZENITH_TEXTURE_EXT);
		Flux_Graphics::s_pxWaterNormalTexture = Zenith_AssetHandler::AddTexture(xWaterNormalTexData);
		xWaterNormalTexData.FreeAllocatedData();
		Flux_MemoryManager::EndFrame(false);
	}

	Flux::LateInitialise();

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_Editor::Initialise();
	Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
	Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
	Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
	Zenith_DebugVariables::AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
#endif

	// Initialize asset database for GUID-based asset references
	Zenith_AssetDatabase::Initialize(ENGINE_ASSETS_DIR);

	Flux_MemoryManager::BeginFrame();
	extern void Project_RegisterScriptBehaviours();
	extern void Project_LoadInitialScene();
	Project_RegisterScriptBehaviours();

	//#TO_TODO: Flux_Graphics::UploadFrameConstants crashes if we don't do this because there is no game camera
	Project_LoadInitialScene();
	Flux_MemoryManager::EndFrame(false);
	Zenith_Core::g_xLastFrameTime = std::chrono::high_resolution_clock::now();

	while (!Zenith_Window::GetInstance()->ShouldClose())
	{
		Zenith_Profiling::BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		Zenith_Profiling::EndFrame();
	}

	//--------------------------------------------------------------------------
	// Shutdown sequence - reverse order of initialization
	// Critical: Must wait for GPU before destroying resources it's using
	//--------------------------------------------------------------------------
	Zenith_Log(LOG_CATEGORY_CORE, "Beginning shutdown sequence...");

	// 1. Wait for GPU to finish all pending work
	Zenith_Vulkan::WaitForGPUIdle();

#ifdef ZENITH_TOOLS
	// 2. Shutdown editor (processes pending deletions, cleans up editor state)
	Zenith_Editor::Shutdown();
#endif

	// 3. Reset scene to release all resources before subsystem shutdown
	// Must happen before physics (colliders need to remove bodies) and before
	// memory manager (model/mesh components hold VRAM handles)
	Zenith_Scene::GetCurrentScene().Reset();

	// 4. Shutdown physics system
	Zenith_Physics::Shutdown();

	// 5. Shutdown Vulkan memory manager (destroys VMA allocator)
	Zenith_Vulkan_MemoryManager::Shutdown();

	// 6. Shutdown task system (terminates worker threads)
	Zenith_TaskSystem::Shutdown();

	// 6. Cleanup window (destructor handles GLFW termination)
	delete Zenith_Window::GetInstance();

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
}
