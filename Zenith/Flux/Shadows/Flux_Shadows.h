#pragma once

#define ZENITH_FLUX_NUM_CSMS 4
#define ZENITH_FLUX_CSM_RESOLUTION 2048

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_Shadows
{
public:
	static void Initialise();
	static void Shutdown();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	static Flux_TargetSetup& GetCSMTargetSetup(const uint32_t uIndex);
	static Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex);
	static Flux_ShaderResourceView& GetCSMSRV(const uint32_t u);
	static Flux_DynamicConstantBuffer& GetShadowMatrixBuffer(const uint32_t u);

	static void UpdateShadowMatrices();

private:
	static constexpr float s_afCSMLevels[ZENITH_FLUX_NUM_CSMS + 1]{ 2000, 50, 10, 5, 1 };
};
