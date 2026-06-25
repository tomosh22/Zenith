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

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2);
// replaces `import Common.Frame;` (GLOBAL set carries the sun direction).
#include "Common/Bindings.slang"
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
	uint   g_uDebugMode;
	uint   g_uSkySamples;
	uint   g_uLightSamples;
	float2 g_xPad;
};

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3.
// g_xTransmittanceLUT = precomputed sun-ray transmittance LUT (Common.Atmosphere).
struct PassParams
{
	ConstantBuffer<AtmosphereConstantsLayout> AtmosphereConstants;
	Sampler2D g_xTransmittanceLUT;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target0
{
	// Lat-long texel -> world view direction (shared mapping with the reader).
	float3 xRayDir = SkyViewUvToDir(input.uv);
	float3 xSunDir = normalize(-g_xGlobalSet.g_xGlobal.g_xSunDir_Pad.xyz);

	// Same fixed camera height as the legacy full-res solver.
	float3 xCamPos = float3(0.0, g_xPassSet.AtmosphereConstants.g_fPlanetRadius + 100.0, 0.0);

	float3 xRayleighCoeff = g_xPassSet.AtmosphereConstants.g_xRayleighScatter.rgb * g_xPassSet.AtmosphereConstants.g_fRayleighScale;
	float  fMieCoeff      = g_xPassSet.AtmosphereConstants.g_xMieScatter.r        * g_xPassSet.AtmosphereConstants.g_fMieScale;
	float2 xScaleHeights  = float2(g_xPassSet.AtmosphereConstants.g_xRayleighScatter.w,
								   g_xPassSet.AtmosphereConstants.g_xMieScatter.w);

	float4 xResult = ComputeAtmosphereScatteringLut(
		xCamPos, xRayDir, xSunDir, 1e10,
		xRayleighCoeff, fMieCoeff, xScaleHeights,
		g_xPassSet.AtmosphereConstants.g_fMieG,
		g_xPassSet.AtmosphereConstants.g_fPlanetRadius, g_xPassSet.AtmosphereConstants.g_fAtmosphereRadius,
		g_xPassSet.AtmosphereConstants.g_fSunIntensity,
		g_xPassSet.AtmosphereConstants.g_uSkySamples, g_xPassSet.g_xTransmittanceLUT);

	// in-scatter RGB + view transmittance A
	return xResult;
}
