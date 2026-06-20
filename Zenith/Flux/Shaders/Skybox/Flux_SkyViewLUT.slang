// =====================================================================
// Flux_SkyViewLUT — Slang module.
//
// Subsystem: Skybox. Generates the low-res sky-view LUT: for each texel
// (lat-long view direction, Common.Atmosphere.SkyViewUvToDir) it runs the
// single-scatter raymarch ONCE this frame (sampling the transmittance LUT
// for the sun-ray term) and stores in-scatter RGB + view transmittance A.
// The fullscreen sky pass (Flux_Atmosphere) then samples this LUT instead
// of raymarching per screen pixel — decoupling atmosphere cost from screen
// resolution (~20K LUT pixels vs ~2M screen pixels).
//
// NO sun disk here (it would alias at low res) — the disk is added at full
// resolution by the fullscreen sky shader.
//
// Camera is the same fixed "100 m above ground on the planet axis" used by
// the legacy full-res solver, so the sky depends only on view + sun
// direction. Regenerated every frame so it tracks the moving sun.
// =====================================================================

import Common.Fullscreen;
import Common.Frame;        // FrameConstants (sun direction)
import Common.Atmosphere;

struct AtmosphereConstantsLayout
{
	float4 g_xRayleighScatter; // RGB = coefficients, W = scale height
	float4 g_xMieScatter;      // RGB = scatter, W = scale height
	float  g_fPlanetRadius;
	float  g_fAtmosphereRadius;
	float  g_fMieG;
	float  g_fSunIntensity;
	float  g_fRayleighScale;
	float  g_fMieScale;
	uint   g_uDebugMode;
	uint   g_uSkySamples;
	uint   g_uLightSamples;
	float2 g_xPad;
};
[[vk::binding(1, 0)]] ConstantBuffer<AtmosphereConstantsLayout> AtmosphereConstants;

// Precomputed sun-ray transmittance LUT (Common.Atmosphere).
[[vk::binding(2, 0)]] Sampler2D g_xTransmittanceLUT;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target0
{
	// Lat-long texel -> world view direction (shared mapping with the reader).
	float3 xRayDir = SkyViewUvToDir(input.uv);
	float3 xSunDir = normalize(-FrameConstants.g_xSunDir_Pad.xyz);

	// Same fixed camera height as the legacy full-res solver.
	float3 xCamPos = float3(0.0, AtmosphereConstants.g_fPlanetRadius + 100.0, 0.0);

	float3 xRayleighCoeff = AtmosphereConstants.g_xRayleighScatter.rgb * AtmosphereConstants.g_fRayleighScale;
	float  fMieCoeff      = AtmosphereConstants.g_xMieScatter.r        * AtmosphereConstants.g_fMieScale;
	float2 xScaleHeights  = float2(AtmosphereConstants.g_xRayleighScatter.w,
								   AtmosphereConstants.g_xMieScatter.w);

	float4 xResult = ComputeAtmosphereScatteringLut(
		xCamPos, xRayDir, xSunDir, 1e10,
		xRayleighCoeff, fMieCoeff, xScaleHeights,
		AtmosphereConstants.g_fMieG,
		AtmosphereConstants.g_fPlanetRadius, AtmosphereConstants.g_fAtmosphereRadius,
		AtmosphereConstants.g_fSunIntensity,
		AtmosphereConstants.g_uSkySamples, g_xTransmittanceLUT);

	// in-scatter RGB + view transmittance A
	return xResult;
}
