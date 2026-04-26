// =====================================================================
// Flux_Fog — Slang module.
//
// Subsystem: Fog. Simple distance-based fog with Henyey-Greenstein
// sun-scatter. Beer-Lambert extinction along the camera-to-pixel ray,
// and the sun lobe colour is blended toward g_xSunColour by the HG
// phase function.
//
// Imports Common.Frame for FrameConstants + GetWorldPosFromDepthTex,
// and Common.Volumetric for HenyeyGreenstein.
// =====================================================================

import Common.Fullscreen;
import Common.Frame;
import Common.Volumetric;

struct FogConstantsLayout
{
	float4 g_xFogColour_Falloff;  // RGB = colour, W = falloff
	float  g_fPhaseG;             // HG asymmetry parameter
	float  _pad0;
	float  _pad1;
	float  _pad2;
};
[[vk::binding(1, 0)]] ConstantBuffer<FogConstantsLayout> FogConstants;
[[vk::binding(2, 0)]] Sampler2D g_xDepthTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float3 xWorldPos      = GetWorldPosFromDepthTex(g_xDepthTex, input.uv);
	float3 xCameraToPixel = xWorldPos - FrameConstants.g_xCamPos_Pad.xyz;
	float  fDist          = length(xCameraToPixel);

	// Beer-Lambert extinction (scalar form).
	float fFogAmount = 1.0 - exp(-fDist * FogConstants.g_xFogColour_Falloff.w);

	// Henyey-Greenstein sun scattering — replaces the legacy ad-hoc
	// pow(cosTheta, 8). g_fPhaseG ∈ [0, 0.95]: 0 = isotropic, 0.8 ≈ haze,
	// 0.95 ≈ Mie water droplets. Exposed via Render/Fog/Phase G debug var.
	float3 xViewDir   = normalize(xCameraToPixel);
	float  fCosTheta  = dot(xViewDir, -FrameConstants.g_xSunDir_Pad.xyz);

	float fPhase           = HenyeyGreenstein(fCosTheta, FogConstants.g_fPhaseG);
	float fPhaseMax        = HenyeyGreenstein(1.0, FogConstants.g_fPhaseG);
	float fPhaseNormalized = fPhase / fPhaseMax;

	float3 xFogColour = lerp(FogConstants.g_xFogColour_Falloff.xyz,
							  FrameConstants.g_xSunColour.xyz,
							  fPhaseNormalized);
	return float4(xFogColour, fFogAmount);
}
