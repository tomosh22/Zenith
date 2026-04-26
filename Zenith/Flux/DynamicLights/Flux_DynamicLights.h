#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_DynamicLights
{
public:
	static void Initialise();
	static void BuildPipelines();
	static void Shutdown();
	static void Reset();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Called each frame to gather lights from scene
	static void GatherLightsFromScene();

	static bool IsInitialised() { return s_bInitialised; }

	static constexpr u_int uMAX_LIGHTS = 256;

private:
	static bool s_bInitialised;
};
