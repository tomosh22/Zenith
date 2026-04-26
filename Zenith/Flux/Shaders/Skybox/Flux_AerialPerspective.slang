// =====================================================================
// Flux_AerialPerspective — Slang module.
//
// Subsystem: Skybox. Distance-based atmospheric attenuation overlay.
// Reconstructs world position from the depth buffer, computes a
// simplified Rayleigh + Mie optical depth along the view ray, and
// outputs (in-scatter RGB, fog blend factor) to be alpha-blended over
// the HDR scene target.
//
// Imports Common.Frame (FrameConstants, sun, camera), Common.Volumetric
// (RayleighPhase) and Common.Atmosphere (GetDensity, MiePhase).
// =====================================================================

import Common.Fullscreen;
import Common.Frame;
import Common.Volumetric;
import Common.Atmosphere;

struct AtmosphereConstantsLayout
{
	float4 g_xRayleighScatter;
	float4 g_xMieScatter;
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
[[vk::binding(1, 0)]] ConstantBuffer<AtmosphereConstantsLayout> AtmosphereConstants;
[[vk::binding(2, 0)]] Sampler2D g_xDepthTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

static const uint SKYBOX_DEBUG_NONE         = 0;
static const uint SKYBOX_DEBUG_AERIAL_DEPTH = 5;

float3 GetWorldPosition(float2 xUV, float fDepth)
{
	float2 xNDC      = xUV * 2.0 - 1.0;
	float4 xClipSpace = float4(xNDC, fDepth, 1.0);
	float4 xViewSpace = mul(FrameConstants.g_xInvProjMat, xClipSpace);
	xViewSpace       /= xViewSpace.w;
	return mul(FrameConstants.g_xInvViewMat, xViewSpace).xyz;
}

// Simplified aerial perspective: optical depth uses camera-altitude
// density only (vs the full per-step integration the main atmosphere
// solver does). Cheap enough to run per-pixel; visually matches the
// reference image for typical ground-level scenes.
float4 ComputeAerialPerspective(float3 xCamPos,
								  float3 xWorldPos,
								  float3 xSunDir,
								  float3 xRayleighCoeff,
								  float  fMieCoeff,
								  float2 xScaleHeights,
								  float  fStrength)
{
	float3 xRayDir   = normalize(xWorldPos - xCamPos);
	float  fDistance = length(xWorldPos - xCamPos);

	// World units → kilometres (scaling for visual emphasis), with
	// strength multiplier for the artist's aerial slider.
	float fScaledDist = fDistance * 0.001 * fStrength;

	// Density evaluated at camera height — fast path that loses accuracy
	// for very tall ray segments. Could integrate along the ray if needed.
	float  fHeight = max(0.0, xCamPos.y);
	float2 xDensity = GetDensity(fHeight, xScaleHeights);

	// Optical depth (back to metres for the exponential).
	float fRayleighDepth = xDensity.x * fScaledDist * 1000.0;
	float fMieDepth      = xDensity.y * fScaledDist * 1000.0;

	float3 xTransmittance = exp(-xRayleighCoeff * fRayleighDepth
								  - fMieCoeff      * fMieDepth);

	// Per-channel in-scatter with phase-weighted contribution. The 0.1
	// factor on g_fSunIntensity is an empirical scaling from the GLSL
	// source — full atmospheric in-scatter would be too intense for
	// the aerial overlay.
	float fCosTheta      = dot(xRayDir, xSunDir);
	float fRayleighPhase = RayleighPhase(fCosTheta);                       // Common.Volumetric
	float fMiePhase      = MiePhase(fCosTheta, 0.76);                      // Common.Atmosphere

	float3 xInscatter = (xRayleighCoeff * fRayleighPhase + fMieCoeff * fMiePhase)
		* (1.0 - xTransmittance) * AtmosphereConstants.g_fSunIntensity * 0.1;

	float fFog = 1.0 - dot(xTransmittance, float3(0.333, 0.333, 0.333));
	return float4(xInscatter, fFog);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float fDepth = g_xDepthTex.Sample(input.uv).r;
	if (fDepth >= 1.0) return float4(0.0, 0.0, 0.0, 0.0); // sky → no aerial

	float3 xWorldPos = GetWorldPosition(input.uv, fDepth);
	float3 xCamPos   = FrameConstants.g_xCamPos_Pad.xyz;
	float3 xSunDir   = normalize(-FrameConstants.g_xSunDir_Pad.xyz);

	// (xCamPos.x, planet+camY, xCamPos.z) preserves XZ for correct
	// perspective while lifting Y into atmosphere space.
	// (Currently unused in the simplified solver but kept for parity
	// with the GLSL source if the integrator gets upgraded.)

	float3 xRayleighCoeff = AtmosphereConstants.g_xRayleighScatter.rgb * AtmosphereConstants.g_fRayleighScale;
	float  fMieCoeff      = AtmosphereConstants.g_xMieScatter.r        * AtmosphereConstants.g_fMieScale;
	float2 xScaleHeights  = float2(AtmosphereConstants.g_xRayleighScatter.w,
									AtmosphereConstants.g_xMieScatter.w);

	float4 xAerial = ComputeAerialPerspective(
		xCamPos, xWorldPos, xSunDir,
		xRayleighCoeff, fMieCoeff, xScaleHeights,
		AtmosphereConstants.g_fAerialStrength);

	if (AtmosphereConstants.g_uDebugMode == SKYBOX_DEBUG_AERIAL_DEPTH)
	{
		float  fDistance = length(xWorldPos - xCamPos);
		float  fNormDist = saturate(fDistance / 10000.0); // 10 km range
		float3 xHeatmap;
		if (fNormDist < 0.5) xHeatmap = lerp(float3(0, 0, 1), float3(0, 1, 0), fNormDist * 2.0);
		else                 xHeatmap = lerp(float3(0, 1, 0), float3(1, 0, 0), (fNormDist - 0.5) * 2.0);
		return float4(xHeatmap, 0.8);
	}

	return xAerial;
}
