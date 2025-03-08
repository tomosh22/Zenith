#pragma once

#define ZENITH_FLUX_NUM_CSMS 3
#define ZENITH_FLUX_CSM_RESOLUTION 1024

class Flux_Shadows
{
public:
	static void Initialise();

	static void Render();

	static class Flux_TargetSetup& GetCSMTargetSetup(const uint32_t uIndex);

	static Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex);

private:
	static void UpdateShadowMatrices();

	static constexpr float s_afCSMLevels[ZENITH_FLUX_NUM_CSMS + 1]{ 100,50,25,1 };
};