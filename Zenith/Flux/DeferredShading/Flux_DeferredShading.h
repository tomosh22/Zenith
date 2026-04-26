#pragma once
#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_DeferredShading
{
public:
	static void Initialise();
	static void BuildPipelines();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);
};