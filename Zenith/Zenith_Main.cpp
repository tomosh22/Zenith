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
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"
#include "StateMachine/Zenith_StateMachine.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_TOOLS
extern void ExportAllMeshes();
extern void ExportAllTextures();
extern void ExportHeightmap();
#endif


static bool s_bDVSTest0 = false;
static bool s_bDVSTest1 = false;
static bool s_bDVSTest2 = false;
static bool s_bDVSTest3 = false;
static Zenith_Maths::Vector3 s_xDVSTest4 = { 1,2,3 };

int main()
{
	//ExportAllMeshes();
	//ExportAllTextures();
	//ExportHeightmap();

	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread();
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
			ASSETS_ROOT"Textures/Cubemap/px.ztx",
			ASSETS_ROOT"Textures/Cubemap/nx.ztx",
			ASSETS_ROOT"Textures/Cubemap/py.ztx",
			ASSETS_ROOT"Textures/Cubemap/ny.ztx",
			ASSETS_ROOT"Textures/Cubemap/pz.ztx",
			ASSETS_ROOT"Textures/Cubemap/nz.ztx"
		);
		Zenith_AssetHandler::AddTexture("Cubemap", xCubemapTexData);
		xCubemapTexData.FreeAllocatedData();
		
		Zenith_AssetHandler::TextureData xWaterNormalTexData = Zenith_AssetHandler::LoadTexture2DFromFile(ASSETS_ROOT"Textures/water/normal.ztx");
		Zenith_AssetHandler::AddTexture("Water_Normal", xWaterNormalTexData);
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

	Zenith_StateMachine::Project_Initialise();
	Flux_MemoryManager::BeginFrame();
	Zenith_StateMachine::s_pxCurrentState->OnEnter();
	Flux_MemoryManager::EndFrame(false);
	Zenith_Core::s_xLastFrameTime = std::chrono::high_resolution_clock::now();

	//#TO_TODO: exit properly
	while (true)
	{
		Zenith_Profiling::BeginFrame();
		Zenith_StateMachine::Update();
		Zenith_Profiling::EndFrame();
	}
	__debugbreak();
}