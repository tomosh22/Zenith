#pragma once

#include "Flux/Flux.h"

class Flux_Terrain
{
public:
	static void Initialise();
	static void Shutdown();
	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void RenderToGBuffer(void*);
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf);

	static void SubmitRenderToGBufferTask();
	static void WaitForRenderToGBufferTask();

	static Flux_Pipeline& GetShadowPipeline();
	static Flux_DynamicConstantBuffer& GetTerrainConstantsBuffer();

	/**
	 * Get the terrain culling compute pipeline
	 * Used by Zenith_TerrainComponent to bind the pipeline before dispatching culling
	 */
	static Flux_Pipeline& GetCullingPipeline();
};