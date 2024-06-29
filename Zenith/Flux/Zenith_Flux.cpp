#include "Zenith.h"

#include "Flux/Zenith_Flux.h"

uint32_t Zenith_Flux::s_uFrameIndex = 0;
uint32_t Zenith_Flux::s_uFrameCounter = 0;
std::vector<const Flux_CommandBuffer*> Zenith_Flux::s_xPendingCommandBuffers[RENDER_ORDER_MAX];

void Zenith_Flux::EarlyInitialise()
{
	Flux_PlatformAPI::Initialise();
	Flux_MemoryManager::Initialise();
}

void Zenith_Flux::LateInitialise()
{
	Flux_MemoryManager::BeginFrame();
	Flux_Swapchain::Initialise();
	Flux_MemoryManager::EndFrame(false);
}