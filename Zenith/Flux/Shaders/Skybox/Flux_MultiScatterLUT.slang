// =====================================================================
// Flux_MultiScatterLUT — Slang module.
//
// Subsystem: Skybox. Bakes the Hillaire multiple-scattering LUT for the
// VISIBLE SKY, from the LIVE atmosphere medium. The IBL bakes its own copy
// from its frozen generation snapshot (IBL/Flux_IBLMultiScatterLUT) --
// they cannot share one LUT without one of them integrating the wrong
// medium, so they share the maths instead (Common.MultiScatter).
//
// Camera-independent AND sun-direction-independent (the sun-zenith cosine
// is an axis of the LUT, not the frame's sun): depends only on the medium,
// so it is baked on exactly the same cadence as the transmittance LUT.
// =====================================================================

// Spine #include keeps PassParams at set 3, matching every other converted
// shader. This bake reads no frame/view state.
#include "Common/Bindings.slang"
import Common.Fullscreen;
import Common.Atmosphere;
import Common.MultiScatter;

struct MultiScatterConstantsLayout
{
	float4 g_xRayleighScatter;   // RGB = coefficients (already density-scaled), W = scale height
	float4 g_xMieScatter;        // RGB = scatter (already density-scaled), W = scale height
	float  g_fPlanetRadius;
	float  g_fAtmosphereRadius;
	float  g_fGroundAlbedo;      // the bounce that feeds back into the sky
	float  g_fPad;
};

struct PassParams { ConstantBuffer<MultiScatterConstantsLayout> MultiScatterConstants; };
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target0
{
	float fPlanetRadius     = g_xPassSet.MultiScatterConstants.g_fPlanetRadius;
	float fAtmosphereRadius = g_xPassSet.MultiScatterConstants.g_fAtmosphereRadius;

	// Same parameterisation as the transmittance LUT so the two agree texel for
	// texel: uv.y -> radius, uv.x -> sun-zenith cosine.
	float fRadius = lerp(fPlanetRadius, fAtmosphereRadius, input.uv.y);
	float fMuSun  = input.uv.x * 2.0 - 1.0;

	float3 xPsiMs = ComputeMultiScatterPsi(
		fRadius, fMuSun,
		g_xPassSet.MultiScatterConstants.g_xRayleighScatter.rgb,
		g_xPassSet.MultiScatterConstants.g_xMieScatter.r,
		float2(g_xPassSet.MultiScatterConstants.g_xRayleighScatter.w,
			   g_xPassSet.MultiScatterConstants.g_xMieScatter.w),
		fPlanetRadius, fAtmosphereRadius,
		g_xPassSet.MultiScatterConstants.g_fGroundAlbedo);

	return float4(xPsiMs, 1.0);
}
