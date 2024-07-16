#include "Zenith.h"

#include "Zenith_OS_Include.h"
#include "Flux/Flux.h"
#include "Flux/Skybox/Flux_Skybox.h"

void Zenith_MainLoop()
{
	Zenith_Window::GetInstance()->BeginFrame();
	Flux_MemoryManager::BeginFrame();
	if (!Flux_Swapchain::BeginFrame())
	{
		Flux_MemoryManager::EndFrame(false);
		return;
	}
	Flux_PlatformAPI::BeginFrame();
	Flux_Skybox::Render();
	Flux_MemoryManager::EndFrame();
	Flux_Swapchain::CopyToFramebuffer();
	Flux_PlatformAPI::EndFrame();
	Flux_Swapchain::EndFrame();
}

int main()
{
	Zenith_Window::Inititalise("Zenith", 1280, 720);
	Flux::EarlyInitialise();
	Flux::LateInitialise();
	Zenith_Core::Project_Startup();
	while (true)
	{
		Zenith_MainLoop();
	}
	__debugbreak();
}