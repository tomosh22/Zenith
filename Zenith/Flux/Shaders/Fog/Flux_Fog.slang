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

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;
import Common.Volumetric;

struct FogConstantsLayout
{
	float4 g_xFogColour_Falloff;  // RGB = colour, W = falloff
	float  g_fPhaseG;             // HG asymmetry parameter
	float  _pad0;
	float  _pad1;
	float  _pad2;
};
// PASS frequency — declared AFTER the spine #include so it lands at set 3.
struct PassParams
{
	ConstantBuffer<FogConstantsLayout> FogConstants;
	Sampler2D g_xDepthTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float3 xWorldPos      = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, input.uv);
	float3 xCameraToPixel = xWorldPos - g_xViewSet.g_xView.g_xCamPos_Pad.xyz;
	float  fDist          = length(xCameraToPixel);

	// Beer-Lambert extinction (scalar form).
	float fFogAmount = 1.0 - exp(-fDist * g_xPassSet.FogConstants.g_xFogColour_Falloff.w);

	// Henyey-Greenstein sun scattering — replaces the legacy ad-hoc
	// pow(cosTheta, 8). g_fPhaseG ∈ [0, 0.95]: 0 = isotropic, 0.8 ≈ haze,
	// 0.95 ≈ Mie water droplets. Exposed via Render/Fog/Phase G debug var.
	float3 xViewDir   = normalize(xCameraToPixel);
	float  fCosTheta  = dot(xViewDir, -g_xViewSet.g_xView.g_xSunDir_Pad.xyz);

	float fPhase           = HenyeyGreenstein(fCosTheta, g_xPassSet.FogConstants.g_fPhaseG);
	float fPhaseMax        = HenyeyGreenstein(1.0, g_xPassSet.FogConstants.g_fPhaseG);
	float fPhaseNormalized = fPhase / fPhaseMax;

	float3 xFogColour = lerp(g_xPassSet.FogConstants.g_xFogColour_Falloff.xyz,
							  g_xViewSet.g_xView.g_xSunColour_Pad.xyz,
							  fPhaseNormalized);
	return float4(xFogColour, fFogAmount);
}
