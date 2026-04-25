#include "Zenith.h"

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

	Zenith_DebugVariables::AddBoolean({ "Graphics", "AnimatedMeshes", "Enabled" }, xOpts.m_bAnimatedMeshesEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "CPUParticles", "Enabled" }, xOpts.m_bCPUParticlesEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "DynamicLights", "Visible" }, xOpts.m_bDynamicLightsVisible);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Fog", "Enabled" }, xOpts.m_bFogEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Gizmos", "Enabled" }, xOpts.m_bGizmosEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "GPUParticles", "Enabled" }, xOpts.m_bGPUParticlesEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Grass", "Enabled" }, xOpts.m_bGrassEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "HDR", "Bloom" }, xOpts.m_bHDRBloomEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "HiZ", "Enabled" }, xOpts.m_bHiZEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "IBL", "Enabled" }, xOpts.m_bIBLEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "InstancedMeshes", "Enabled" }, xOpts.m_bInstancedMeshesEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Primitives", "Enabled" }, xOpts.m_bPrimitivesEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Quads", "Enabled" }, xOpts.m_bQuadsEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SDFs", "Enabled" }, xOpts.m_bSDFsEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Shadows", "Enabled" }, xOpts.m_bShadowsEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Skybox", "Enabled" }, xOpts.m_bSkyboxEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SSAO", "Enabled" }, xOpts.m_bSSAOEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SSGI", "Enabled" }, xOpts.m_bSSGIEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SSR", "Enabled" }, xOpts.m_bSSREnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "StaticMeshes", "Enabled" }, xOpts.m_bStaticMeshesEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Terrain", "Enabled" }, xOpts.m_bTerrainEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Text", "Enabled" }, xOpts.m_bTextEnabled);

	Zenith_DebugVariables::AddBoolean({ "Graphics", "HDR", "AutoExposure" }, xOpts.m_bHDRAutoExposureEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "IBL", "Diffuse" }, xOpts.m_bIBLDiffuseEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "IBL", "Specular" }, xOpts.m_bIBLSpecularEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Skybox", "Atmosphere" }, xOpts.m_bSkyboxAtmosphereEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Skybox", "AerialPerspective" }, xOpts.m_bSkyboxAerialPerspectiveEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SSAO", "Blur" }, xOpts.m_bSSAOBlurEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SSR", "RoughnessBlur" }, xOpts.m_bSSRRoughnessBlurEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "SSGI", "Denoise" }, xOpts.m_bSSGIDenoiseEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Grass", "Wind" }, xOpts.m_bGrassWindEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "Grass", "Culling" }, xOpts.m_bGrassCullingEnabled);
	Zenith_DebugVariables::AddBoolean({ "Graphics", "InstancedMeshes", "GPUCulling" }, xOpts.m_bInstancedMeshGPUCullingEnabled);

	Zenith_DebugVariables::AddUInt32({ "Graphics", "Fog", "Technique" }, xOpts.m_uVolFogTechnique, 0, 3);
#endif
}
