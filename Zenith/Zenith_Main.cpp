#include "Zenith.h"
#include "Core/Zenith_Core.h"
#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif
#include "AssetHandling/Zenith_AssetHandler.h"
#include "StateMachine/Zenith_StateMachine.h"
#include "Physics/Zenith_Physics.h"
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
	Zenith_UnitTests::RunAllTests();
	//ExportAllMeshes();
	//ExportHeightmap();
	Zenith_Window::Inititalise("Zenith", 256, 192);
	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();
	Flux_MemoryManager::BeginFrame();
	//#TO_TODO: engine should have its own default cubemap
	Zenith_AssetHandler::AddTextureCube("Cubemap",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/px.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/nx.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/py.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/ny.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/pz.ztx",
		"C:/dev/Zenith/Games/Test/Assets/Textures/Cubemap/nz.ztx"
	);
	Zenith_AssetHandler::AddTexture2D("Water_Normal", "C:/dev/Zenith/Games/Test/Assets/Textures/water/normal.ztx");
	Flux_MemoryManager::EndFrame(false);
	Flux::LateInitialise();

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES

	Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
	Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
	Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
#endif

	Zenith_StateMachine::s_pxCurrentState->OnEnter();
	Zenith_Core::s_xLastFrameTime = std::chrono::high_resolution_clock::now();
	Flux_Graphics::UploadFrameConstants();
	while (true)
	{
		Zenith_StateMachine::Update();
	}
	__debugbreak();
}