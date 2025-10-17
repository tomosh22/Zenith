#pragma once

#define ZENITH_FLUX_NUM_CSMS 4
#define ZENITH_FLUX_CSM_RESOLUTION 1024

#include "Flux/Flux.h"

class Flux_Shadows
{
public:
	static void Initialise();

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

	static Flux_TargetSetup& GetCSMTargetSetup(const uint32_t uIndex);
	static Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex);
	static Flux_Texture& GetCSMTexture(const uint32_t u);
	static Flux_DynamicConstantBuffer& GetShadowMatrixBuffer(const uint32_t u);

private:
	static void UpdateShadowMatrices();
	static constexpr float s_afCSMLevels[ZENITH_FLUX_NUM_CSMS + 1]{ 2000, 100, 20, 10, 1 };
};