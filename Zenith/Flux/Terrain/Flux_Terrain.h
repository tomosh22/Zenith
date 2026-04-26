#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_DynamicConstantBuffer;

class Flux_Terrain
{
public:
	static void Initialise();
	static void BuildPipelines();
	static void Shutdown();
	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// CPU-side per-frame update (streaming, LOD, culling dispatch)
	static void PreRenderUpdate();

	static Flux_Pipeline& GetShadowPipeline();
	static Flux_DynamicConstantBuffer& GetTerrainConstantsBuffer();

	/**
	 * Get the terrain culling compute pipeline
	 * Used by Zenith_TerrainComponent to bind the pipeline before dispatching culling
	 */
	static Flux_Pipeline& GetCullingPipeline();

	static u_int& GetDebugMode();
	static bool& GetWireframeMode();

private:
	static void ExecuteCulling(Flux_CommandList* pxCmdList, void* pUserData);
	static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void* pUserData);
};