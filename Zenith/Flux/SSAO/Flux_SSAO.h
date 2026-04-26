#pragma once

#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_SSAO
{
public:
	static void Initialise();
	static void Shutdown();
	static void BuildPipelines();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);
};
