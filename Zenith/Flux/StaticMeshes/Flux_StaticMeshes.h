#pragma once

#include "Flux/Flux.h"

class Flux_StaticMeshes
{
public:
	static void Initialise();

	static void RenderToGBuffer(void*);
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf);

	static void SubmitRenderToGBufferTask();
	static void WaitForRenderToGBufferTask();

	static Flux_Pipeline& GetShadowPipeline();
};