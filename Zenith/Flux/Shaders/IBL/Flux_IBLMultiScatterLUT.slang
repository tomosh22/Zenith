// =====================================================================
// Flux_IBLMultiScatterLUT — Slang module.
//
// Subsystem: IBL. The ENVIRONMENT CAPTURE's own Hillaire multiple-
// scattering LUT, baked from the generation's FROZEN snapshot medium.
//
// Why this is a separate program from Skybox/Flux_MultiScatterLUT rather
// than a shared LUT:
//   - the Skybox bakes from the LIVE medium (the visible sky must track an
//     authored change the frame it happens), while
//   - the capture bakes from the frozen snapshot (a mid-generation medium
//     change must not leak into faces still being convolved -- that is the
//     entire point of the snapshot).
// One shared LUT would force one of them to be wrong. The maths is shared
// (Common.MultiScatter); only the constant source differs. At 32x32
// RGBA16F the duplicate costs ~8 KB.
// =====================================================================

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
	float  g_fGroundAlbedo;      // the AUTHORED capture ground albedo
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
