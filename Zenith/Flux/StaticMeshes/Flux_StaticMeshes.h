#pragma once

#include "Flux/Flux.h"

class Flux_StaticMeshes
{
public:
	static void Initialise();

	static void RenderToGBuffer();
	static void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf);

	static Flux_Pipeline& GetShadowPipeline();
};