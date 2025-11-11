#pragma once

#include "Flux/Flux.h"

class Flux_Terrain
{
public:
	static void Initialise();

	static void RenderToGBuffer(void*, u_int uInvocationIndex, u_int uNumInvocations);
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf);

	static void SubmitRenderToGBufferTask();
	static void WaitForRenderToGBufferTask();

	static Flux_Pipeline& GetShadowPipeline();
	static Flux_DynamicConstantBuffer& GetTerrainConstantsBuffer();
};