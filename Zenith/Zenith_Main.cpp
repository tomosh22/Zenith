#include "Zenith.h"

#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Physics/Zenith_Physics.h"

static std::chrono::high_resolution_clock::time_point s_xLastFrameTime;

static void UpdateTimers()
{
	std::chrono::high_resolution_clock::time_point xCurrentTime = std::chrono::high_resolution_clock::now();

	Zenith_Core::SetDt(std::chrono::duration_cast<std::chrono::nanoseconds>(xCurrentTime - s_xLastFrameTime).count() / 1.e9);
	s_xLastFrameTime = xCurrentTime;

	Zenith_Core::AddTimePassed(Zenith_Core::GetDt());
}

void Zenith_MainLoop()
{
	UpdateTimers();
	Zenith_Window::GetInstance()->BeginFrame();
	Flux_MemoryManager::BeginFrame();
	if (!Flux_Swapchain::BeginFrame())
	{
		Flux_MemoryManager::EndFrame(false);
		return;
	}
	Flux_PlatformAPI::BeginFrame();

	Zenith_Physics::Update(Zenith_Core::GetDt());
	Zenith_Scene::GetCurrentScene().Update(Zenith_Core::GetDt());
	Flux_Graphics::UploadFrameConstants();
	Flux_Skybox::Render();
	Flux_StaticMeshes::Render();

	Flux_MemoryManager::EndFrame();
	Flux_Swapchain::CopyToFramebuffer();
	Flux_PlatformAPI::EndFrame();
	Flux_Swapchain::EndFrame();
}


int main()
{
#ifdef ZENITH_TOOLS
	extern void ExportAllMeshes();
	extern void ExportAllTextures();
	extern void ExportHeightmap();
	ExportAllMeshes();
	ExportAllTextures();
	ExportHeightmap();
#else
	s_xLastFrameTime = std::chrono::high_resolution_clock::now();
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Zenith_Physics::Initialise();
	Zenith_Core::Project_Startup();
	Flux::LateInitialise();
	
	while (true)
	{
		Zenith_MainLoop();
	}
#endif //ZENITH_TOOLS
	__debugbreak();
}
