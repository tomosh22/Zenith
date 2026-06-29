#pragma once

#include "Maths/Zenith_Maths.h"

struct Zenith_GraphicsOptions
{
	static Zenith_GraphicsOptions& Get();
	static void RegisterDebugVariables();

	uint32_t m_uWindowWidth = 1280;
	uint32_t m_uWindowHeight = 720;

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
	// Gates the forward Translucency pass. (Was m_bStaticMeshesEnabled: it historically also gated
	// the now-deleted StaticMeshes opaque feature; the opaque model meshes now render through the
	// UnifiedMesh path, which has its own enablement, so this toggle's sole remaining effect is the
	// translucent draw — named for what it actually does.)
	bool m_bTranslucencyEnabled = true;
	bool m_bTerrainEnabled = true;
	bool m_bTextEnabled = true;

	bool m_bHDRAutoExposureEnabled = true;
	bool m_bIBLDiffuseEnabled = true;
	bool m_bIBLSpecularEnabled = true;
	// Atmosphere on by default: the IBL ambient already bakes from the physically
	// based atmosphere (sun-intensity 20), so a flat cubemap sky was incoherent
	// with the lighting. Drawing the atmosphere makes sky + ambient one coherent
	// system -- the strongest single outdoor photoreal cue, and it already runs.
	bool m_bSkyboxAtmosphereEnabled = true;
	bool m_bSSAOBlurEnabled = true;
	bool m_bSSRRoughnessBlurEnabled = true;
	bool m_bSSGIDenoiseEnabled = true;
	bool m_bGrassWindEnabled = true;
	bool m_bGrassCullingEnabled = true;
	bool m_bInstancedMeshGPUCullingEnabled = true;

	uint32_t m_uVolFogTechnique = 0;

	// Fallback solid-sky colour (used only when the skybox is disabled). A
	// plausible daylight blue so IBL/ambient gets a real sky instead of black.
	Zenith_Maths::Vector3 m_xSkyboxColour = Zenith_Maths::Vector3(0.35f, 0.55f, 0.85f);
};
