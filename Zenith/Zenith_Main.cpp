#include "Zenith.h"

#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "EntityComponent/Zenith_Scene.h"

static float s_fDt = 0.;
static std::chrono::high_resolution_clock::time_point s_xLastFrameTime;

static void UpdateDt()
{
	std::chrono::high_resolution_clock::time_point xCurrentTime = std::chrono::high_resolution_clock::now();

	s_fDt = std::chrono::duration_cast<std::chrono::nanoseconds>(xCurrentTime - s_xLastFrameTime).count() / 1.e9;
	s_xLastFrameTime = xCurrentTime;
}

void Zenith_MainLoop()
{
	UpdateDt();
	Zenith_Window::GetInstance()->BeginFrame();
	Flux_MemoryManager::BeginFrame();
	if (!Flux_Swapchain::BeginFrame())
	{
		Flux_MemoryManager::EndFrame(false);
		return;
	}
	Flux_PlatformAPI::BeginFrame();

	Zenith_Scene::GetCurrentScene().Update(s_fDt);
	Flux_Graphics::UploadFrameConstants();
	Flux_Skybox::Render();
	Flux_StaticMeshes::Render();

	Flux_MemoryManager::EndFrame();
	Flux_Swapchain::CopyToFramebuffer();
	Flux_PlatformAPI::EndFrame();
	Flux_Swapchain::EndFrame();
}

#ifndef ZENITH_TOOLS
int main()
{
	s_xLastFrameTime = std::chrono::high_resolution_clock::now();
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Zenith_Core::Project_Startup();
	Flux::LateInitialise();
	
	while (true)
	{
		Zenith_MainLoop();
	}
	__debugbreak();
}
#endif