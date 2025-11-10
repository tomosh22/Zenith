#pragma once

#include "Flux/Flux.h"

class Flux_AnimatedMeshes
{
public:
	static void Initialise();

	static void RenderToGBuffer(void*);
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

	static Flux_Pipeline& GetShadowPipeline();
};