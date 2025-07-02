#include "Zenith.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/Water/Flux_Water.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Text/Flux_Text.h"

uint32_t Flux::s_uFrameCounter = 0;
std::vector<Zenith_Callback<void>> Flux::s_xResChangeCallbacks;

void Flux::EarlyInitialise()
{
	Flux_PlatformAPI::Initialise();
	Flux_MemoryManager::Initialise();
}

void Flux::LateInitialise()
{
	Flux_MemoryManager::BeginFrame();
	Flux_Swapchain::Initialise();
#ifdef ZENITH_TOOLS
	Flux_PlatformAPI::InitialiseImGui();
#endif
	Flux_Graphics::Initialise();
	Flux_Shadows::Initialise();
	Flux_Skybox::Initialise();
	Flux_StaticMeshes::Initialise();
	Flux_AnimatedMeshes::Initialise();
	Flux_Terrain::Initialise();
	Flux_DeferredShading::Initialise();
	Flux_Water::Initialise();
	Flux_Fog::Initialise();
	Flux_SDFs::Initialise();
	Flux_Particles::Initialise();
	Flux_Text::Initialise();
	Flux_MemoryManager::EndFrame(false);
}

void Flux::OnResChange()
{
	for (Zenith_Callback<void>& xCallback : s_xResChangeCallbacks)
	{
		xCallback.Execute();
	}
}