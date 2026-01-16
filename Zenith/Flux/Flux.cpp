#include "Zenith.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Terrain/Flux_Terrain.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif
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
#include "Flux/InstancedMeshes/Flux_InstancedMeshes.h"

uint32_t Flux::s_uFrameCounter = 0;
std::vector<void(*)()> Flux::s_xResChangeCallbacks;
Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>> Flux::s_xPendingCommandLists[RENDER_ORDER_MAX];

void Flux::EarlyInitialise()
{
	Flux_PlatformAPI::Initialise();
	Flux_MemoryManager::Initialise();
	Flux_PlatformAPI::InitialiseScratchBuffers(); // Must be after memory manager init
}

void Flux::LateInitialise()
{
	Flux_MemoryManager::BeginFrame();
	Flux_Swapchain::Initialise();
	Flux_Graphics::Initialise();
#ifdef ZENITH_TOOLS
	Flux_PlatformAPI::InitialiseImGui();
	Flux_Gizmos::Initialise();

	// Initialize Slang compiler for runtime compilation
	Flux_SlangCompiler::Initialise();
	Flux_ShaderHotReload::Initialise();
#endif
	Flux_Shadows::Initialise();
	Flux_Skybox::Initialise();
	Flux_StaticMeshes::Initialise();
	Flux_AnimatedMeshes::Initialise();
	Flux_InstancedMeshes::Initialise();
	Flux_Terrain::Initialise();
	Flux_Primitives::Initialise();
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

void Flux::Shutdown()
{
	// Shutdown Flux subsystems (vertex/index/constant buffers, render attachments)
	// Order is reverse of initialization where dependencies exist
	Flux_ComputeTest::Shutdown();
	Flux_Text::Shutdown();
	Flux_Quads::Shutdown();
	Flux_Particles::Shutdown();
	Flux_SDFs::Shutdown();
	Flux_Primitives::Shutdown();
	Flux_Terrain::Shutdown();
	Flux_InstancedMeshes::Shutdown();
	Flux_Shadows::Shutdown();

#ifdef ZENITH_TOOLS
	// Shutdown shader hot reload and Slang compiler
	Flux_ShaderHotReload::Shutdown();
	Flux_SlangCompiler::Shutdown();

	Flux_Gizmos::Shutdown();
	Flux_PlatformAPI::ShutdownImGui();
#endif

	// Shutdown core graphics (render targets, depth buffer, quad mesh, frame constants)
	Flux_Graphics::Shutdown();

	// Shutdown memory manager (VMA allocator, handle registries)
	Flux_MemoryManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux shut down");
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
	static_assert(FLUX_NUM_WORKER_THREADS > 0, "FLUX_NUM_WORKER_THREADS must be positive");

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
	Zenith_Assert(uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS,
		"PrepareFrame: Thread index %u out of bounds (max %u)", uCurrentThreadIndex, FLUX_NUM_WORKER_THREADS);

	if (uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS)
	{
		xOutDistribution.auEndRenderOrder[uCurrentThreadIndex] = RENDER_ORDER_MAX;
		xOutDistribution.auEndIndex[uCurrentThreadIndex] = 0;
	}

	return true;
}
