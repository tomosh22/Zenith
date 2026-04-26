#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_DynamicConstantBuffer;

class Flux_StaticMeshes
{
public:
	static void Initialise();
	static void BuildPipelines();

	static void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	static Flux_Pipeline& GetShadowPipeline();

private:
	static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void* pUserData);
};