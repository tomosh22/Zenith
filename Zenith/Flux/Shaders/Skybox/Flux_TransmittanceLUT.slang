// =====================================================================
// Flux_TransmittanceLUT — Slang module.
//
// Subsystem: Skybox. Generates the atmosphere transmittance LUT: for each
// texel (uv.x <- sun/view-zenith cosine mu, uv.y <- altitude fraction) it
// integrates the optical depth from that point to the top of the
// atmosphere and stores exp(-tau) (per-channel transmittance). Sampled by
// the atmosphere solver (Common.Atmosphere.SampleTransmittanceLut) to
// replace the per-pixel inner light-ray march.
//
// Camera-independent: depends only on AtmosphereConstants (scattering
// coefficients/scales, scale heights, planet/atmosphere radii). Regenerated
// only when those change (see Flux_SkyboxImpl::UpdateGraphPassEnables).
// =====================================================================

import Common.Fullscreen;
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
	float  g_fAerialStrength;
	uint   g_uDebugMode;
	uint   g_uSkySamples;
	uint   g_uLightSamples;
	float2 g_xPad;
};
[[vk::binding(0, 0)]] ConstantBuffer<AtmosphereConstantsLayout> AtmosphereConstants;

// Integration sample count for the LUT bake. Higher than the per-frame
// light-sample count since this runs rarely (param change only); changing
// it is a transmittance invalidator (see UpdateGraphPassEnables).
static const uint kTRANSMITTANCE_SAMPLES = 64u;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target0
{
	float fPlanetRadius     = AtmosphereConstants.g_fPlanetRadius;
	float fAtmosphereRadius = AtmosphereConstants.g_fAtmosphereRadius;

	// Invert TransmittanceLutUv: uv.y -> radius, uv.x -> mu in [-1,1].
	float fRadius = lerp(fPlanetRadius, fAtmosphereRadius, input.uv.y);
	float fMu     = input.uv.x * 2.0 - 1.0;

	float3 xRayleighCoeff = AtmosphereConstants.g_xRayleighScatter.rgb * AtmosphereConstants.g_fRayleighScale;
	float  fMieCoeff      = AtmosphereConstants.g_xMieScatter.r        * AtmosphereConstants.g_fMieScale;
	float2 xScaleHeights  = float2(AtmosphereConstants.g_xRayleighScatter.w,
								   AtmosphereConstants.g_xMieScatter.w);

	float3 xTransmittance = TransmittanceToAtmosphereTop(
		fRadius, fMu, xRayleighCoeff, fMieCoeff, xScaleHeights,
		fPlanetRadius, fAtmosphereRadius, kTRANSMITTANCE_SAMPLES);

	return float4(xTransmittance, 1.0);
}
