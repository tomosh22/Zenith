#include "Zenith.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"

uint32_t Flux::s_uFrameCounter = 0;
std::vector<void(*)()> Flux::s_xResChangeCallbacks;



void Flux::EarlyInitialise()
{
	Flux_PlatformAPI::Initialise();
	Flux_MemoryManager::Initialise();
}

void Flux::LateInitialise()
{
	Flux_MemoryManager::BeginFrame();
	Flux_Swapchain::Initialise();
	Flux_Graphics::Initialise();
	Flux_Skybox::Initialise();
	Flux_StaticMeshes::Initialise();
	Flux_MemoryManager::EndFrame(false);
}

void Flux::OnResChange()
{
	for (void(*pfnCallback)() : s_xResChangeCallbacks)
	{
		pfnCallback();
	}
}