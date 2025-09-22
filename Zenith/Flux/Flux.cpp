#include "Zenith.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Text/Flux_Text.h"
#include "Flux/Quads/Flux_Quads.h"

uint32_t Flux::s_uFrameCounter = 0;
std::vector<void(*)()> Flux::s_xResChangeCallbacks;
Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>> Flux::s_xPendingCommandLists[RENDER_ORDER_MAX];

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
	Flux_Fog::Initialise();
	Flux_SDFs::Initialise();
	Flux_Particles::Initialise();
	Flux_Quads::Initialise();
	Flux_Text::Initialise();
	Flux_MemoryManager::EndFrame(false);
}

void Flux::OnResChange()
{
	for (void(*pfnCallback)() : s_xResChangeCallbacks)
	{
		pfnCallback();
	}
}

void Flux::RegisterBindlessTexture(Flux_Texture* pxTex, uint32_t uIndex)
{
	Platform_RegisterBindlessTexture(pxTex, uIndex);
}
