#pragma once

#include "Maths/Zenith_Maths.h"

struct Zenith_GraphicsOptions
{
	static Zenith_GraphicsOptions& Get();
	static void RegisterDebugVariables();

	uint32_t m_uWindowWidth = 1280;
	uint32_t m_uWindowHeight = 720;

	bool m_bAnimatedMeshesEnabled = true;
	bool m_bCPUParticlesEnabled = true;
	bool m_bDynamicLightsVisible = true;
	bool m_bFogEnabled = true;
	bool m_bGizmosEnabled = true;
	bool m_bGPUParticlesEnabled = true;
	bool m_bGrassEnabled = true;
	bool m_bHDRBloomEnabled = true;
	bool m_bHiZEnabled = true;
	bool m_bIBLEnabled = true;
	bool m_bInstancedMeshesEnabled = true;
	bool m_bPrimitivesEnabled = true;
	bool m_bQuadsEnabled = true;
	bool m_bSDFsEnabled = true;
	bool m_bShadowsEnabled = true;
	bool m_bSkyboxEnabled = true;
	bool m_bSSAOEnabled = true;
	bool m_bSSGIEnabled = false;
	bool m_bSSREnabled = true;
	bool m_bStaticMeshesEnabled = true;
	bool m_bTerrainEnabled = true;
	bool m_bTextEnabled = true;

	bool m_bHDRAutoExposureEnabled = true;
	bool m_bIBLDiffuseEnabled = true;
	bool m_bIBLSpecularEnabled = true;
	bool m_bSkyboxAtmosphereEnabled = false;
	bool m_bSkyboxAerialPerspectiveEnabled = true;
	bool m_bSSAOBlurEnabled = true;
	bool m_bSSRRoughnessBlurEnabled = true;
	bool m_bSSGIDenoiseEnabled = true;
	bool m_bGrassWindEnabled = true;
	bool m_bGrassCullingEnabled = true;
	bool m_bInstancedMeshGPUCullingEnabled = true;

	uint32_t m_uVolFogTechnique = 0;

	Zenith_Maths::Vector3 m_xSkyboxColour = Zenith_Maths::Vector3(0.f);
};
