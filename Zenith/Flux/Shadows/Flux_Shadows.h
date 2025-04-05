#pragma once

#define ZENITH_FLUX_NUM_CSMS 3
#define ZENITH_FLUX_CSM_RESOLUTION 1024

#include "Flux/Flux.h"

class Flux_Shadows
{
public:
	static void Initialise();

	static void Render();

	static Flux_TargetSetup& GetCSMTargetSetup(const uint32_t uIndex);
	static Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex);
	static Flux_Texture& GetCSMTexture(const uint32_t u);
	static Flux_ConstantBuffer& GetShadowMatrixBuffer(const uint32_t u);

private:
	static void UpdateShadowMatrices();

	static constexpr float s_afCSMLevels[ZENITH_FLUX_NUM_CSMS + 1]{ 1000,200,50,1 };
};