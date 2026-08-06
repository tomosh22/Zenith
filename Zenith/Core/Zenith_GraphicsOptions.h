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
	// SET ONCE AT BOOT, both of them: a game picks its values inside
	// Project_SetGraphicsOptions and the renderer reads them from then on. Flipping
	// either mid-run is not a supported path — the Flux/Grass debug-variable tree is
	// the seam for live experimentation.
	bool m_bGrassDisplacementEnabled = true;
	bool m_bGrassShadowsEnabled = true;
	bool m_bInstancedMeshGPUCullingEnabled = true;

	uint32_t m_uVolFogTechnique = 0;

	// ---- IBL environment-capture budget (per-game, set once by Project_SetGraphicsOptions) ----
	// How far the sun may drift from the last CAPTURED target before a new IBL
	// generation is scheduled. Displacement ACCUMULATES against that target, so
	// this is "how stale may the ambient be", not "how fast may the sun move".
	// The default (~0.081 deg) is the historical dot-product threshold. A game
	// with a fast day (CityBuilder's 120 s cycle moves 3 deg/s) wants a LOOSER
	// value or it re-captures continuously; a game with a 40-minute day can
	// afford a tighter one. Clamped to a sane range by Flux_IBLEnvironment.
	float m_fIBLCaptureThresholdDegrees = 0.0811f;
	// Convolution passes executed per frame while a capture is in flight. A full
	// generation is 48 passes, so the default 8 converges in 6 frames; 4 halves
	// the per-frame cost and doubles the latency. Clamped to >= 1.
	// NOTE: this may legitimately be < 6, i.e. the irradiance cube's 6 faces can
	// straddle a frame boundary. That is safe ONLY because the cubes are double
	// buffered (Flux_IBLImpl publishes on completion) -- do not "optimise" the
	// double buffer away on the assumption that irradiance always completes in
	// one frame.
	uint32_t m_uIBLPassesPerFrame = 8;

	// ---- Atmosphere quality ----
	// Hillaire multiple-scattering LUT. Single-scatter alone loses the horizon
	// brightening and the ambient lift that makes twilight read correctly; the
	// LUT adds the infinite-order series for one 32x32 bake per medium change.
	// Off = the pre-existing single-scatter look (kept as an explicit escape
	// hatch for perf comparison and for pinning old captures).
	bool m_bAtmosphereMultiScatteringEnabled = true;

	// Fallback solid-sky colour (used only when the skybox is disabled). A
	// plausible daylight blue so IBL/ambient gets a real sky instead of black.
	Zenith_Maths::Vector3 m_xSkyboxColour = Zenith_Maths::Vector3(0.35f, 0.55f, 0.85f);
};
