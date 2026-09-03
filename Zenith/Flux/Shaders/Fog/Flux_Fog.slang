// =====================================================================
// Flux_Fog — Slang module.
//
// Subsystem: Fog. The default distance-fog technique is AERIAL PERSPECTIVE:
// the same Rayleigh + Mie medium the sky is rendered with, integrated along
// the camera-to-pixel segment. Distant terrain loses contrast, shifts blue
// and gains a sun-forward haze exactly as the sky above it does, so the
// ground and the sky read as ONE atmosphere. This replaced a constant-colour
// exponential fog whose default falloff produced ~7% fog at the far plane —
// no depth cue at all — in a colour unrelated to the sky.
//
// Radiometry, per channel:
//   T          = exp(-(tauR + tauM))                       view transmittance
//   L_sun      = (bR*PR + bM*PM) / bExt * E_sun * (1 - T)  single-scattered sun
//   L_ambient  = (bR + bM)       / bExt * L_sky * (1 - T)  isotropic sky bounce
// with the optical depths taking the exponential density profile between the
// camera's and the pixel's altitude analytically. E_sun is the engine's derived
// ground-level sun (the atmosphere anchor x transmittance, GetSunColourIntensity)
// and L_sky is the irradiance cube's zenith value (E/pi, i.e. mean sky radiance).
//
// The pass alpha-blends over the lit HDR scene: alpha = 1 - luminance(T) and
// colour = inscatter / alpha reproduces scene*T + inscatter for a scalar T.
//
// FogConstants keep their historical GPU layout: g_xFogColour_Falloff.w is now
// the ground-level MIE EXTINCTION coefficient (1/m) — the haze density knob — and
// the rgb colour is unused by this technique (it was the fog's flat colour).
//
// Imports Common.Frame for FrameConstants + GetWorldPosFromDepthTex,
// and Common.Volumetric for HenyeyGreenstein.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;
import Common.Volumetric;

struct FogConstantsLayout
{
	float4 g_xFogColour_Falloff;  // RGB = (unused legacy colour), W = ground-level Mie extinction (1/m)
	float  g_fPhaseG;             // HG asymmetry parameter of the Mie lobe
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

// Sea-level Rayleigh scattering (Bruneton / Nishita, 1/m) and the two scale
// heights — the same medium Flux_SkyboxImpl.h authors for the sky.
static const float3 AERIAL_RAYLEIGH_SCATTER = float3(5.8e-6, 13.5e-6, 33.1e-6);
static const float  AERIAL_RAYLEIGH_SCALE_H = 8000.0;
static const float  AERIAL_MIE_SCALE_H      = 1200.0;
// Mie single-scattering albedo (the rest of the extinction is absorption).
static const float  AERIAL_MIE_ALBEDO       = 0.9;
static const float  AERIAL_PI               = 3.14159265359;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// Integral of exp(-h/H) along a straight segment from altitude h0 to h1 of
// length fLength, i.e. the density-weighted path length. Analytic; the
// near-horizontal case falls back to the endpoint density to avoid the 0/0.
float DensityWeightedLength(float fH0, float fH1, float fLength, float fScaleHeight)
{
	float fDeltaH = fH1 - fH0;
	if (abs(fDeltaH) < 0.01)
	{
		return fLength * exp(-max(fH0, 0.0) / fScaleHeight);
	}
	float fE0 = exp(-max(fH0, 0.0) / fScaleHeight);
	float fE1 = exp(-max(fH1, 0.0) / fScaleHeight);
	return fLength * fScaleHeight * (fE0 - fE1) / fDeltaH;
}

float RayleighPhaseFn(float fCosTheta)
{
	return (3.0 / (16.0 * AERIAL_PI)) * (1.0 + fCosTheta * fCosTheta);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	// The sky already carries its own atmosphere; only surfaces receive haze.
	float fDepth = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	if (fDepth >= 1.0)
	{
		return float4(0.0, 0.0, 0.0, 0.0);
	}

	float3 xWorldPos      = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, input.uv);
	float3 xCamPos        = GetCamPos_Pad().xyz;
	float3 xCameraToPixel = xWorldPos - xCamPos;
	float  fDist          = length(xCameraToPixel);
	if (fDist < 1e-3)
	{
		return float4(0.0, 0.0, 0.0, 0.0);
	}
	float3 xViewDir = xCameraToPixel / fDist;

	// Medium coefficients at sea level; the knob scales the Mie (haze) term.
	float  fMieExtinction0 = max(g_xPassSet.FogConstants.g_xFogColour_Falloff.w, 0.0);
	float  fMieScatter0    = fMieExtinction0 * AERIAL_MIE_ALBEDO;
	float3 xRayleigh0      = AERIAL_RAYLEIGH_SCATTER;

	// Altitude-aware optical depths over the segment (world Y is altitude).
	float fPathR = DensityWeightedLength(xCamPos.y, xWorldPos.y, fDist, AERIAL_RAYLEIGH_SCALE_H);
	float fPathM = DensityWeightedLength(xCamPos.y, xWorldPos.y, fDist, AERIAL_MIE_SCALE_H);
	float3 xTauR = xRayleigh0 * fPathR;
	float  fTauM = fMieExtinction0 * fPathM;
	float3 xTransmittance = exp(-(xTauR + fTauM));

	// Effective (path-averaged) scattering coefficients for the in-scatter ratio.
	float3 xScatR   = xRayleigh0 * (fPathR / fDist);
	float  fScatM   = fMieScatter0 * (fPathM / fDist);
	float  fExtM    = fMieExtinction0 * (fPathM / fDist);
	float3 xExtinct = max(xScatR + fExtM, 1e-9);

	// Sun single scattering.
	float3 xToSun    = normalize(-GetSunDir_Pad().xyz);
	float  fCosTheta = dot(xViewDir, xToSun);
	float  fPhaseR   = RayleighPhaseFn(fCosTheta);
	float  fPhaseM   = HenyeyGreenstein(fCosTheta, g_xPassSet.FogConstants.g_fPhaseG);
	float4 xSun      = GetSunColour_Pad();
	float3 xSunE     = xSun.rgb * xSun.a;
	float3 xOneMinusT = 1.0 - xTransmittance;
	float3 xInscatterSun = (xScatR * fPhaseR + fScatM * fPhaseM) / xExtinct * xSunE * xOneMinusT;

	// Sky (multiple-scatter / ambient) term: isotropic, so the phase integrates
	// to one. The irradiance cube stores E/pi, i.e. the mean radiance.
	float3 xSkyRadiance  = GetIrradianceMap().SampleLevel(float3(0.0, 1.0, 0.0), 0.0).rgb;
	float3 xInscatterSky = (xScatR + fScatM) / xExtinct * xSkyRadiance * xOneMinusT;

	float3 xInscatter = xInscatterSun + xInscatterSky;

	// Alpha-blend composite: alpha = 1 - T (luminance), colour = inscatter / alpha.
	float fAlpha = saturate(1.0 - dot(xTransmittance, float3(0.2126, 0.7152, 0.0722)));
	if (fAlpha < 1e-4)
	{
		return float4(0.0, 0.0, 0.0, 0.0);
	}
	return float4(xInscatter / fAlpha, fAlpha);
}
