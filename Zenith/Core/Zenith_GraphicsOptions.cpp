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

	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "CPUParticles", "Enabled" }, xOpts.m_bCPUParticlesEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "DynamicLights", "Visible" }, xOpts.m_bDynamicLightsVisible);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Fog", "Enabled" }, xOpts.m_bFogEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Gizmos", "Enabled" }, xOpts.m_bGizmosEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "GPUParticles", "Enabled" }, xOpts.m_bGPUParticlesEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Grass", "Enabled" }, xOpts.m_bGrassEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "HDR", "Bloom" }, xOpts.m_bHDRBloomEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "HiZ", "Enabled" }, xOpts.m_bHiZEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "IBL", "Enabled" }, xOpts.m_bIBLEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "InstancedMeshes", "Enabled" }, xOpts.m_bInstancedMeshesEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Primitives", "Enabled" }, xOpts.m_bPrimitivesEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Quads", "Enabled" }, xOpts.m_bQuadsEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SDFs", "Enabled" }, xOpts.m_bSDFsEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Shadows", "Enabled" }, xOpts.m_bShadowsEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Skybox", "Enabled" }, xOpts.m_bSkyboxEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SSAO", "Enabled" }, xOpts.m_bSSAOEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SSGI", "Enabled" }, xOpts.m_bSSGIEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SSR", "Enabled" }, xOpts.m_bSSREnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Translucency", "Enabled" }, xOpts.m_bTranslucencyEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Terrain", "Enabled" }, xOpts.m_bTerrainEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Text", "Enabled" }, xOpts.m_bTextEnabled);

	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "HDR", "AutoExposure" }, xOpts.m_bHDRAutoExposureEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "IBL", "Diffuse" }, xOpts.m_bIBLDiffuseEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "IBL", "Specular" }, xOpts.m_bIBLSpecularEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Skybox", "Atmosphere" }, xOpts.m_bSkyboxAtmosphereEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SSAO", "Blur" }, xOpts.m_bSSAOBlurEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SSR", "RoughnessBlur" }, xOpts.m_bSSRRoughnessBlurEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "SSGI", "Denoise" }, xOpts.m_bSSGIDenoiseEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Grass", "Wind" }, xOpts.m_bGrassWindEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "Grass", "Culling" }, xOpts.m_bGrassCullingEnabled);
	g_xEngine.DebugVariables().AddBoolean({ "Graphics", "InstancedMeshes", "GPUCulling" }, xOpts.m_bInstancedMeshGPUCullingEnabled);

	g_xEngine.DebugVariables().AddUInt32({ "Graphics", "Fog", "Technique" }, xOpts.m_uVolFogTechnique, 0, 3);
#endif
}
