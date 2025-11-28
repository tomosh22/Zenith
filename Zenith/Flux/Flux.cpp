#include "Zenith.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Text/Flux_Text.h"
#include "Flux/Quads/Flux_Quads.h"
#include "Flux/ComputeTest/Flux_ComputeTest.h"

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
	Flux_Primitives::Initialise();  // Debug primitives - initialized after terrain
	Flux_DeferredShading::Initialise();
	Flux_SSAO::Initialise();
	Flux_Fog::Initialise();
	Flux_SDFs::Initialise();
	Flux_Particles::Initialise();
	Flux_Quads::Initialise();
	Flux_Text::Initialise();
	Flux_ComputeTest::Initialise();
	Flux_MemoryManager::EndFrame(false);
}

void Flux::OnResChange()
{
	for (void(*pfnCallback)() : s_xResChangeCallbacks)
	{
		pfnCallback();
	}
}

bool Flux::PrepareFrame(Flux_WorkDistribution& xOutDistribution)
{
	xOutDistribution.Clear();
	
	// Count total commands across all render orders
	for (u_int uRenderOrder = RENDER_ORDER_MEMORY_UPDATE + 1; uRenderOrder < RENDER_ORDER_MAX; uRenderOrder++)
	{
		const Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>>& xCommandLists = s_xPendingCommandLists[uRenderOrder];
		
		for (u_int i = 0; i < xCommandLists.GetSize(); i++)
		{
			xOutDistribution.uTotalCommandCount += xCommandLists.Get(i).first->GetCommandCount();
		}
	}
	
	// Early exit if there are no commands
	if (xOutDistribution.uTotalCommandCount == 0)
	{
		return false;
	}
	
	// Distribute work across threads based on command count
	const u_int uTargetCommandsPerThread = (xOutDistribution.uTotalCommandCount + FLUX_NUM_WORKER_THREADS - 1) / FLUX_NUM_WORKER_THREADS;
	u_int uCurrentThreadIndex = 0;
	u_int uCurrentThreadCommandCount = 0;
	
	// Set the first thread's start position
	xOutDistribution.auStartRenderOrder[0] = RENDER_ORDER_MEMORY_UPDATE + 1;
	xOutDistribution.auStartIndex[0] = 0;
	
	// Iterate through all render orders and command lists to distribute work
	for (u_int uRenderOrder = RENDER_ORDER_MEMORY_UPDATE + 1; uRenderOrder < RENDER_ORDER_MAX; uRenderOrder++)
	{
		const Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>>& xCommandLists = s_xPendingCommandLists[uRenderOrder];
		
		for (u_int uIndex = 0; uIndex < xCommandLists.GetSize(); uIndex++)
		{
			const u_int uCommandCount = xCommandLists.Get(uIndex).first->GetCommandCount();
			
			// If adding this item would exceed the target and we have more threads available, move to next thread
			if (uCurrentThreadCommandCount > 0 && 
			    uCurrentThreadCommandCount + uCommandCount > uTargetCommandsPerThread && 
			    uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS - 1)
			{
				// Finalize current thread's range
				xOutDistribution.auEndRenderOrder[uCurrentThreadIndex] = uRenderOrder;
				xOutDistribution.auEndIndex[uCurrentThreadIndex] = uIndex;
				
				// Move to next thread
				uCurrentThreadIndex++;
				uCurrentThreadCommandCount = 0;
				xOutDistribution.auStartRenderOrder[uCurrentThreadIndex] = uRenderOrder;
				xOutDistribution.auStartIndex[uCurrentThreadIndex] = uIndex;
			}
			
			uCurrentThreadCommandCount += uCommandCount;
		}
	}
	
	// Finalize the last thread's range (end is exclusive, so point to start of next render order)
	if (uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS)
	{
		xOutDistribution.auEndRenderOrder[uCurrentThreadIndex] = RENDER_ORDER_MAX;
		xOutDistribution.auEndIndex[uCurrentThreadIndex] = 0;
	}
	
	return true;
}
