#pragma once

#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_SSAO
{
public:
	static bool s_bEnabled;

	static void Initialise();
	static void Shutdown();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);
};
