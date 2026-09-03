// =====================================================================
// Flux_Atmosphere — Slang module.
//
// Subsystem: Skybox. Procedural physically-based sky via Rayleigh + Mie
// scattering. Reads the camera ray direction from RayDir() (Common.Frame)
// and runs ComputeAtmosphereScattering with engine-tunable coefficients,
// then adds the limb-darkened sun disk attenuated by transmittance.
//
// Output: G-buffer (diffuse / normals+ambient / material). Sky has zero
// surface normals and emissive luminance equal to its colour magnitude.
//
// Imports Common.Frame, Common.Volumetric (for RayleighPhase used by
// the SCATTER_DIRECTION debug overlay), and Common.Atmosphere.
// =====================================================================

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2);
// replaces `import Common.Frame;`. The other imports are pure-function modules.
#include "Common/Bindings.slang"
import Common.Fullscreen;
import Common.Volumetric;
import Common.Atmosphere;
import Common.Noise;      // Flux_NoiseFBM: the cloud layer

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
	uint   g_uMultiScatteringEnabled;
	float  g_fPad;
};

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3.
// g_xSkyViewLUT = precomputed low-res sky-view LUT (Common.Atmosphere); the full
// raymarch ran once this frame into it, the fullscreen sky pass just samples it.
struct PassParams
{
	ConstantBuffer<AtmosphereConstantsLayout> AtmosphereConstants;
	Sampler2D g_xSkyViewLUT;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// Skybox debug enum — must stay aligned with Skybox_DebugMode in
// Flux_Skybox.h.
static const uint SKYBOX_DEBUG_NONE              = 0;
static const uint SKYBOX_DEBUG_RAYLEIGH_ONLY     = 1;
static const uint SKYBOX_DEBUG_MIE_ONLY          = 2;
static const uint SKYBOX_DEBUG_TRANSMITTANCE     = 3;
static const uint SKYBOX_DEBUG_SCATTER_DIRECTION = 4;
static const uint SKYBOX_DEBUG_SUN_DISK          = 5;
static const uint SKYBOX_DEBUG_LUT_PREVIEW       = 6;
static const uint SKYBOX_DEBUG_RAY_STEPS         = 7;
static const uint SKYBOX_DEBUG_PHASE_FUNCTION    = 8;

// =====================================================================
// Cloud layer.
//
// A cloudless sky is the single most reliably synthetic thing in an outdoor
// frame: a real sky is almost never an unbroken gradient, and clouds are what
// give the eye a sense of scale, distance and weather. This is a LAYER model,
// not a volumetric march — the view ray is intersected with a single slab at
// cloud altitude and the density is a domain-warped fBm evaluated there. That
// buys the two cues that matter (silhouette shape and sun-side/shadow-side
// shading) at a couple of noise evaluations per pixel, where a true volumetric
// would cost a march per pixel and need its own temporal denoiser.
//
// Everything is in the sky pass only. The IBL capture convolves the clear-sky
// medium, so the ambient does not flicker as clouds drift.
// =====================================================================
static const float CLOUD_ALTITUDE      = 2600.0;   // metres above the camera
static const float CLOUD_COVERAGE      = 0.62;     // calibrated against the fBm's MEASURED distribution (mean 0.47, p90 0.63, max 0.82),
                                                   // not against the nominal [0,1]: at 0.46 the cut left 99% of the sky under alpha 0.05, i.e. no clouds at all.
                                                   // 0.62 gives scattered cumulus - ~19% of sky above alpha 0.3, the rest open blue.
static const float CLOUD_SHARPNESS     = 1.0;      // edge hardness of the coverage cut
static const float CLOUD_SCALE         = 0.0013;   // world metres -> noise units. At 0.00028 the base octave's wavelength was ~3.5 km,
                                                   // so only two or three cells spanned the visible sky and the layer read as a uniform haze rather than as cloud SHAPES.
static const float CLOUD_WARP          = 0.55;     // domain warp strength (billow)
static const float CLOUD_DENSITY       = 1.5;      // optical depth multiplier
static const float CLOUD_WIND_SPEED    = 1.4;      // metres/second (slow: TAA reprojects the sky camera-only)
static const float CLOUD_HORIZON_FADE  = 0.055;    // ray.y below this fades the layer out

// Coverage-shaped fBm density at a point on the cloud slab, in [0,1].
float CloudDensityAt(float2 xPos, float fTime)
{
	float2 xUV = xPos * CLOUD_SCALE + float2(fTime * CLOUD_WIND_SPEED * CLOUD_SCALE, 0.0);
	// One warp pass turns round fBm blobs into the billowed, sheared shapes real
	// cumulus have; without it the layer reads as noise rather than as weather.
	float fWarpX = Flux_NoiseFBM(xUV.x * 2.1 + 11.0, xUV.y * 2.1 - 7.0, 91u, 2u, 2.0, 0.5) - 0.5;
	float fWarpY = Flux_NoiseFBM(xUV.x * 2.1 - 3.0, xUV.y * 2.1 + 5.0, 137u, 2u, 2.0, 0.5) - 0.5;
	float2 xWarped = xUV + float2(fWarpX, fWarpY) * CLOUD_WARP;

	float fBase = Flux_NoiseFBM(xWarped.x, xWarped.y, 17u, 5u, 2.1, 0.55);
	// Coverage cut: everything below the threshold is clear sky, and the
	// remainder is renormalised so the surviving cloud keeps its full density
	// range instead of being a faint wash.
	float fCut = saturate((fBase - (1.0 - CLOUD_COVERAGE)) / max(CLOUD_COVERAGE, 1e-3));
	return pow(fCut, CLOUD_SHARPNESS);
}

// Returns cloud colour in .rgb and coverage alpha in .a for a view ray.
float4 SampleCloudLayer(float3 xRayDir, float3 xSunDir, float3 xSkyColour, float fSunIrradiance, float fTime)
{
	// Rays at or below the horizon never reach the slab; fade rather than cut so
	// the layer does not end on a hard line.
	float fUp = xRayDir.y;
	if (fUp <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);
	float fHorizon = smoothstep(0.0, CLOUD_HORIZON_FADE, fUp);
	if (fHorizon <= 0.0) return float4(0.0, 0.0, 0.0, 0.0);

	// Slab intersection. The distance grows without bound toward the horizon,
	// which is exactly the perspective compression real cloud decks show.
	float  fDist = CLOUD_ALTITUDE / fUp;
	float2 xHit  = xRayDir.xz * fDist;

	float fDensity = CloudDensityAt(xHit, fTime);
	if (fDensity <= 0.001) return float4(0.0, 0.0, 0.0, 0.0);

	// Lighting: step toward the sun along the slab and read the density there as
	// an optical-depth proxy, then Beer-Lambert. The "powder" term darkens the
	// centre of thick masses so a cloud reads as a solid body rather than a decal.
	float2 xToSun    = normalize(xSunDir.xz + float2(1e-4, 0.0));
	float  fShadowed = CloudDensityAt(xHit + xToSun * 260.0, fTime);
	float  fOptical  = (fDensity * 0.35 + fShadowed * 0.65) * CLOUD_DENSITY * 3.0;
	float  fBeer     = exp(-fOptical);
	float  fPowder   = 1.0 - exp(-fOptical * 2.0);
	float  fLight    = fBeer * lerp(1.0, fPowder, 0.55);

	// Forward scattering: the silver lining when looking toward the sun.
	float fCosTheta = dot(xRayDir, xSunDir);
	float fForward  = HenyeyGreenstein(fCosTheta, 0.72) / HenyeyGreenstein(1.0, 0.72);

	// The sun term uses the irradiance anchor (not the disc radiance): a cloud is
	// an extended scatterer lit by the whole solar irradiance, so scaling it by
	// the disc's radiance would make the deck brighter than the sun itself.
	float3 xSunLit  = float3(1.0, 0.97, 0.92) * (fSunIrradiance * 0.12) * fLight * (0.35 + 0.65 * fForward);
	// Ambient: the sky itself lights the shadowed underside, tinted slightly blue.
	float3 xAmbient = xSkyColour * (0.55 + 0.45 * (1.0 - fDensity));

	float fAlpha = saturate(fDensity * fHorizon);
	return float4(xSunLit + xAmbient, fAlpha);
}

float3 DebugHeatmap(float fValue)
{
	fValue = saturate(fValue);
	if (fValue < 0.5) return lerp(float3(0, 0, 1), float3(0, 1, 0), fValue * 2.0);
	return lerp(float3(0, 1, 0), float3(1, 0, 0), (fValue - 0.5) * 2.0);
}

struct GBufferOut
{
	float4 diffuse        : SV_Target0;
	float4 normalsAmbient : SV_Target1;
	float4 material       : SV_Target2;
	float4 emissive       : SV_Target3;
};

[shader("fragment")]
GBufferOut fsMain(FullscreenVsOut input)
{
	float3 xRayDir = RayDir(input.uv);
	float3 xSunDir = normalize(-GetSunDir_Pad().xyz);

	// Camera lifted to a few metres above ground so the atmosphere solver
	// always sees a non-degenerate height. 100 m is consistent with the
	// IBL reference height (Common.PBR.IBL_REFERENCE_HEIGHT_METERS) but
	// kept literal here to match the legacy GLSL shader exactly.
	float3 xCamPos = float3(0.0, g_xPassSet.AtmosphereConstants.g_fPlanetRadius + 100.0, 0.0);

	float3 xRayleighCoeff = g_xPassSet.AtmosphereConstants.g_xRayleighScatter.rgb * g_xPassSet.AtmosphereConstants.g_fRayleighScale;
	float  fMieCoeff      = g_xPassSet.AtmosphereConstants.g_xMieScatter.r        * g_xPassSet.AtmosphereConstants.g_fMieScale;
	float2 xScaleHeights  = float2(g_xPassSet.AtmosphereConstants.g_xRayleighScatter.w,
									g_xPassSet.AtmosphereConstants.g_xMieScatter.w);

	// Fast path: sample the low-res sky-view LUT (the full raymarch ran once this
	// frame into g_xSkyViewLUT, itself sampling the transmittance LUT). Per-pixel
	// cost is now a single texture fetch. Debug modes below still run the full
	// solver for their visualisations. xCamPos/coeffs above feed those paths.
	float4 xScatterResult = g_xPassSet.g_xSkyViewLUT.Sample(SkyViewDirToUv(xRayDir));

	float3 xSkyColor = xScatterResult.rgb;

	// Cloud deck, composited over the clear sky and BEFORE the sun disc, so a
	// cloud in front of the sun occludes it the way it must.
	float4 xCloud = SampleCloudLayer(xRayDir, xSunDir, xSkyColor,
									  g_xPassSet.AtmosphereConstants.g_fSunIntensity,
									  GetTimeSeconds());
	xSkyColor = lerp(xSkyColor, xCloud.rgb, xCloud.a);

	// Sun disk attenuated by view transmittance — disappears behind the
	// atmosphere on the horizon as expected — and by any cloud in front of it.
	float3 xSunDisk = RenderSunDisk(xRayDir, xSunDir, g_xPassSet.AtmosphereConstants.g_fSunIntensity);
	xSkyColor += xSunDisk * xScatterResult.a * (1.0 - xCloud.a);

	if (g_xPassSet.AtmosphereConstants.g_uDebugMode != SKYBOX_DEBUG_NONE)
	{
		switch (g_xPassSet.AtmosphereConstants.g_uDebugMode)
		{
			case SKYBOX_DEBUG_RAYLEIGH_ONLY:
			{
				float4 r = ComputeAtmosphereScattering(
					xCamPos, xRayDir, xSunDir, 1e10,
					xRayleighCoeff, 0.0,
					xScaleHeights, g_xPassSet.AtmosphereConstants.g_fMieG,
					g_xPassSet.AtmosphereConstants.g_fPlanetRadius, g_xPassSet.AtmosphereConstants.g_fAtmosphereRadius,
					g_xPassSet.AtmosphereConstants.g_fSunIntensity,
					g_xPassSet.AtmosphereConstants.g_uSkySamples, g_xPassSet.AtmosphereConstants.g_uLightSamples,
					0.0); // real terrain supplies the ground in the visible sky pass
				xSkyColor = r.rgb;
				break;
			}
			case SKYBOX_DEBUG_MIE_ONLY:
			{
				float4 m = ComputeAtmosphereScattering(
					xCamPos, xRayDir, xSunDir, 1e10,
					float3(0.0, 0.0, 0.0), fMieCoeff,
					xScaleHeights, g_xPassSet.AtmosphereConstants.g_fMieG,
					g_xPassSet.AtmosphereConstants.g_fPlanetRadius, g_xPassSet.AtmosphereConstants.g_fAtmosphereRadius,
					g_xPassSet.AtmosphereConstants.g_fSunIntensity,
					g_xPassSet.AtmosphereConstants.g_uSkySamples, g_xPassSet.AtmosphereConstants.g_uLightSamples,
					0.0); // real terrain supplies the ground in the visible sky pass
				xSkyColor = m.rgb;
				break;
			}
			case SKYBOX_DEBUG_TRANSMITTANCE:
			{
				float t = xScatterResult.a;
				xSkyColor = float3(t, t, t);
				break;
			}
			case SKYBOX_DEBUG_SCATTER_DIRECTION:
			{
				float fCosTheta = dot(xRayDir, xSunDir);
				float fAngle    = acos(clamp(fCosTheta, -1.0, 1.0)) / 3.14159265;
				xSkyColor = DebugHeatmap(fAngle);
				break;
			}
			case SKYBOX_DEBUG_SUN_DISK:
			{
				float fCosAngle  = dot(xRayDir, xSunDir);
				float fSunAngle  = acos(clamp(fCosAngle, -1.0, 1.0));
				float fNormAngle = fSunAngle / 0.02;
				float v = 1.0 - saturate(fNormAngle);
				xSkyColor = float3(v, v, v);
				break;
			}
			case SKYBOX_DEBUG_RAY_STEPS:
			{
				float fNormSteps = (float)g_xPassSet.AtmosphereConstants.g_uSkySamples / 64.0;
				xSkyColor = DebugHeatmap(fNormSteps);
				break;
			}
			case SKYBOX_DEBUG_PHASE_FUNCTION:
			{
				float fCosTheta      = dot(xRayDir, xSunDir);
				float fRayleighPhase = RayleighPhase(fCosTheta);                       // Common.Volumetric
				float fMiePhase      = MiePhase(fCosTheta, g_xPassSet.AtmosphereConstants.g_fMieG); // Common.Atmosphere
				xSkyColor = float3(fRayleighPhase * 2.0, fMiePhase * 0.5, 0.0);
				break;
			}
		}
	}

	GBufferOut o;
	// The sky's HDR radiance travels in the EMISSIVE target (RGBA16F): the
	// diffuse target is 8-bit and would clip the sun and the bright horizon
	// to 1.0 before they ever reached the HDR scene. Deferred shading returns
	// the emissive value for depth==1 pixels; the diffuse copy is a clamped
	// preview for the G-buffer debug views only.
	o.diffuse        = float4(saturate(xSkyColor), 1.0);
	o.normalsAmbient = float4(0.0, 0.0, 0.0, 0.0);
	o.material       = float4(0.0, 0.0, 0.0, 0.0);
	o.emissive       = float4(xSkyColor, 1.0);   // alpha 1: the pipeline blend state is alpha-blend, so alpha 0 would write nothing
	return o;
}
