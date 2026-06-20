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

import Common.Fullscreen;
import Common.Frame;
import Common.Volumetric;
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

// Precomputed low-res sky-view LUT (Common.Atmosphere). The full raymarch ran
// once this frame into it; the fullscreen sky pass just samples it per pixel.
[[vk::binding(2, 0)]] Sampler2D g_xSkyViewLUT;

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
	float3 xSunDir = normalize(-FrameConstants.g_xSunDir_Pad.xyz);

	// Camera lifted to a few metres above ground so the atmosphere solver
	// always sees a non-degenerate height. 100 m is consistent with the
	// IBL reference height (Common.PBR.IBL_REFERENCE_HEIGHT_METERS) but
	// kept literal here to match the legacy GLSL shader exactly.
	float3 xCamPos = float3(0.0, AtmosphereConstants.g_fPlanetRadius + 100.0, 0.0);

	float3 xRayleighCoeff = AtmosphereConstants.g_xRayleighScatter.rgb * AtmosphereConstants.g_fRayleighScale;
	float  fMieCoeff      = AtmosphereConstants.g_xMieScatter.r        * AtmosphereConstants.g_fMieScale;
	float2 xScaleHeights  = float2(AtmosphereConstants.g_xRayleighScatter.w,
									AtmosphereConstants.g_xMieScatter.w);

	// Fast path: sample the low-res sky-view LUT (the full raymarch ran once this
	// frame into g_xSkyViewLUT, itself sampling the transmittance LUT). Per-pixel
	// cost is now a single texture fetch. Debug modes below still run the full
	// solver for their visualisations. xCamPos/coeffs above feed those paths.
	float4 xScatterResult = g_xSkyViewLUT.Sample(SkyViewDirToUv(xRayDir));

	float3 xSkyColor = xScatterResult.rgb;
	// Sun disk attenuated by view transmittance — disappears behind the
	// atmosphere on the horizon as expected.
	float3 xSunDisk = RenderSunDisk(xRayDir, xSunDir, AtmosphereConstants.g_fSunIntensity * 5.0);
	xSkyColor += xSunDisk * xScatterResult.a;

	if (AtmosphereConstants.g_uDebugMode != SKYBOX_DEBUG_NONE)
	{
		switch (AtmosphereConstants.g_uDebugMode)
		{
			case SKYBOX_DEBUG_RAYLEIGH_ONLY:
			{
				float4 r = ComputeAtmosphereScattering(
					xCamPos, xRayDir, xSunDir, 1e10,
					xRayleighCoeff, 0.0,
					xScaleHeights, AtmosphereConstants.g_fMieG,
					AtmosphereConstants.g_fPlanetRadius, AtmosphereConstants.g_fAtmosphereRadius,
					AtmosphereConstants.g_fSunIntensity,
					AtmosphereConstants.g_uSkySamples, AtmosphereConstants.g_uLightSamples);
				xSkyColor = r.rgb;
				break;
			}
			case SKYBOX_DEBUG_MIE_ONLY:
			{
				float4 m = ComputeAtmosphereScattering(
					xCamPos, xRayDir, xSunDir, 1e10,
					float3(0.0, 0.0, 0.0), fMieCoeff,
					xScaleHeights, AtmosphereConstants.g_fMieG,
					AtmosphereConstants.g_fPlanetRadius, AtmosphereConstants.g_fAtmosphereRadius,
					AtmosphereConstants.g_fSunIntensity,
					AtmosphereConstants.g_uSkySamples, AtmosphereConstants.g_uLightSamples);
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
				float fNormSteps = (float)AtmosphereConstants.g_uSkySamples / 64.0;
				xSkyColor = DebugHeatmap(fNormSteps);
				break;
			}
			case SKYBOX_DEBUG_PHASE_FUNCTION:
			{
				float fCosTheta      = dot(xRayDir, xSunDir);
				float fRayleighPhase = RayleighPhase(fCosTheta);                       // Common.Volumetric
				float fMiePhase      = MiePhase(fCosTheta, AtmosphereConstants.g_fMieG); // Common.Atmosphere
				xSkyColor = float3(fRayleighPhase * 2.0, fMiePhase * 0.5, 0.0);
				break;
			}
		}
	}

	GBufferOut o;
	o.diffuse        = float4(xSkyColor, 1.0);
	o.normalsAmbient = float4(0.0, 0.0, 0.0, 0.0);
	// Sky pixels skip lighting (depth==1 early-out in deferred shading) —
	// the material/emissive targets just carry zeros.
	o.material       = float4(0.0, 0.0, 0.0, 0.0);
	o.emissive       = float4(0.0, 0.0, 0.0, 0.0);
	return o;
}
