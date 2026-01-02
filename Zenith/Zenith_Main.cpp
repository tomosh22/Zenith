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
#include "Flux/Flux_Graphics.h"
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_TOOLS
extern void ExportAllMeshes();
extern void ExportAllTextures();
extern void ExportHeightmap();
#endif

int main()
{
#ifdef ZENITH_TOOLS
	ExportAllMeshes();
	//ExportAllTextures();
	//ExportHeightmap();
#endif

	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread(true);
	Zenith_MemoryManagement::Initialise();
	Zenith_TaskSystem::Inititalise();
	Zenith_UnitTests::RunAllTests();
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();

	//#TO_TODO: move somewhere sensible
	{
		Flux_MemoryManager::BeginFrame();
		//#TO_TODO: engine should have its own versions of these
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

		Zenith_AssetHandler::TextureData xWaterNormalTexData = Zenith_AssetHandler::LoadTexture2DFromFile(ENGINE_ASSETS_DIR"Textures/Water/normal" ZENITH_TEXTURE_EXT);
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
#endif

	Flux_MemoryManager::BeginFrame();
	extern void Project_RegisterScriptBehaviours();
	extern void Project_LoadInitialScene();
	Project_RegisterScriptBehaviours();

	//#TO_TODO: Flux_Graphics::UploadFrameConstants crashes if we don't do this because there is no game camera
	Project_LoadInitialScene();
	Flux_MemoryManager::EndFrame(false);
	Zenith_Core::s_xLastFrameTime = std::chrono::high_resolution_clock::now();

	//#TO_TODO: exit properly
	while (true)
	{
		Zenith_Profiling::BeginFrame();
		Zenith_Core::Zenith_MainLoop();
		Zenith_Profiling::EndFrame();
	}
	Zenith_DebugBreak();
}