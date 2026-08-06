#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

Zenith_GraphicsOptions& Zenith_GraphicsOptions::Get()
{
	static Zenith_GraphicsOptions s_xInstance;
	return s_xInstance;
}

void Zenith_GraphicsOptions::RegisterDebugVariables()
{
#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions& xOpts = Get();
	// One hoisted reference for the whole registration table: every line
	// below reaches the same subsystem, and the per-file engine-singleton
	// ratchet counts every token.
	Zenith_DebugVariables& xDebugVars = g_xEngine.DebugVariables();

	xDebugVars.AddBoolean({ "Graphics", "CPUParticles", "Enabled" }, xOpts.m_bCPUParticlesEnabled);
	xDebugVars.AddBoolean({ "Graphics", "DynamicLights", "Visible" }, xOpts.m_bDynamicLightsVisible);
	xDebugVars.AddBoolean({ "Graphics", "Fog", "Enabled" }, xOpts.m_bFogEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Gizmos", "Enabled" }, xOpts.m_bGizmosEnabled);
	xDebugVars.AddBoolean({ "Graphics", "GPUParticles", "Enabled" }, xOpts.m_bGPUParticlesEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Grass", "Enabled" }, xOpts.m_bGrassEnabled);
	xDebugVars.AddBoolean({ "Graphics", "HDR", "Bloom" }, xOpts.m_bHDRBloomEnabled);
	xDebugVars.AddBoolean({ "Graphics", "HiZ", "Enabled" }, xOpts.m_bHiZEnabled);
	xDebugVars.AddBoolean({ "Graphics", "IBL", "Enabled" }, xOpts.m_bIBLEnabled);
	xDebugVars.AddBoolean({ "Graphics", "InstancedMeshes", "Enabled" }, xOpts.m_bInstancedMeshesEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Primitives", "Enabled" }, xOpts.m_bPrimitivesEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Quads", "Enabled" }, xOpts.m_bQuadsEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SDFs", "Enabled" }, xOpts.m_bSDFsEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Shadows", "Enabled" }, xOpts.m_bShadowsEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Skybox", "Enabled" }, xOpts.m_bSkyboxEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SSAO", "Enabled" }, xOpts.m_bSSAOEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SSGI", "Enabled" }, xOpts.m_bSSGIEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SSR", "Enabled" }, xOpts.m_bSSREnabled);
	xDebugVars.AddBoolean({ "Graphics", "Translucency", "Enabled" }, xOpts.m_bTranslucencyEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Terrain", "Enabled" }, xOpts.m_bTerrainEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Text", "Enabled" }, xOpts.m_bTextEnabled);

	xDebugVars.AddBoolean({ "Graphics", "HDR", "AutoExposure" }, xOpts.m_bHDRAutoExposureEnabled);
	xDebugVars.AddBoolean({ "Graphics", "IBL", "Diffuse" }, xOpts.m_bIBLDiffuseEnabled);
	xDebugVars.AddBoolean({ "Graphics", "IBL", "Specular" }, xOpts.m_bIBLSpecularEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Skybox", "Atmosphere" }, xOpts.m_bSkyboxAtmosphereEnabled);
	// The multiple-scattering escape hatch is only an escape hatch if it can be
	// reached at runtime. The LUTs themselves do not change (the flag gates
	// whether the solvers SAMPLE them, and the sky CB is re-uploaded every
	// frame), but the flag is part of the IBL capture snapshot, so toggling it
	// also re-captures the ambient -- an A/B is a toggle, not a rebuild.
	xDebugVars.AddBoolean({ "Graphics", "Skybox", "MultiScattering" }, xOpts.m_bAtmosphereMultiScatteringEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SSAO", "Blur" }, xOpts.m_bSSAOBlurEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SSR", "RoughnessBlur" }, xOpts.m_bSSRRoughnessBlurEnabled);
	xDebugVars.AddBoolean({ "Graphics", "SSGI", "Denoise" }, xOpts.m_bSSGIDenoiseEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Grass", "Wind" }, xOpts.m_bGrassWindEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Grass", "Displacement" }, xOpts.m_bGrassDisplacementEnabled);
	xDebugVars.AddBoolean({ "Graphics", "Grass", "Shadows" }, xOpts.m_bGrassShadowsEnabled);
	xDebugVars.AddBoolean({ "Graphics", "InstancedMeshes", "GPUCulling" }, xOpts.m_bInstancedMeshGPUCullingEnabled);

	xDebugVars.AddUInt32({ "Graphics", "Fog", "Technique" }, xOpts.m_uVolFogTechnique, 0, 3);
#endif
}
