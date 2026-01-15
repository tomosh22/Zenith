#pragma once

#include "Flux/Flux.h"

class Flux_DynamicConstantBuffer;

class Flux_StaticMeshes
{
public:
	static void Initialise();
	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void RenderToGBuffer(void*);
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	static void SubmitRenderToGBufferTask();
	static void WaitForRenderToGBufferTask();

	static Flux_Pipeline& GetShadowPipeline();
};