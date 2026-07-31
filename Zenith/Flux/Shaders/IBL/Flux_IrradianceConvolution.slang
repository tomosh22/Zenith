// =====================================================================
// Flux_IrradianceConvolution — Slang module.
//
// Subsystem: IBL. Pre-computes the diffuse irradiance cubemap for IBL
// from either a procedural atmosphere (Common.Atmosphere) or a static
// skybox cubemap. One face per dispatch — face index supplied via
// IrradianceConstants.g_uFaceIndex.
//
// Cosine-weighted Hammersley importance sampling: the cos(theta) factor
// is baked into the PDF, so the result is the simple average of the
// 64 samples without an extra π / multiplier.
// =====================================================================

#include "Common/Bindings.slang"
import Common.Fullscreen;
import Common.PBR;
import Common.Atmosphere;

// The whole GENERATION SNAPSHOT, frozen on the CPU (Flux_IBLPassConstants).
// Nothing here is read from the per-frame spine: the sun direction is the
// captured one, not the VIEW set's live sun, so a 6-frame generation cannot
// mix faces convolved from two different sun positions, and the atmosphere
// medium is the authored one, not the Atmosphere.slang defaults.
struct IrradianceConstantsLayout
{
	float4 g_xSunDirection;        // direction the light TRAVELS into the scene; w unused
	float  g_fSunIntensity;        // the engine's one radiometric anchor (captured, never authored)
	float  g_fRayleighScale;       // ---- authored atmosphere medium ----
	float  g_fMieScale;
	float  g_fMieG;
	float  g_fRayleighScaleHeight;
	float  g_fMieScaleHeight;
	float  g_fGroundAlbedo;        // virtual ground the capture integrates
	uint   g_uUseAtmosphere;       // 1 = procedural sky, 0 = cubemap
	uint   g_uMultiScatteringEnabled;
	uint   g_uFaceIndex;           // 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z
	float  g_fPad0;
	float  g_fPad1;
};
struct PassParams
{
	ConstantBuffer<IrradianceConstantsLayout> IrradianceConstants;
	SamplerCube g_xSkyboxCubemap;
	// The CAPTURE's own multi-scatter LUT, baked from the generation snapshot's
	// medium -- deliberately not the Skybox's live one, which would let an
	// authored atmosphere change leak into faces mid-generation.
	Sampler2D g_xMultiScatterLUT;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

static const uint SAMPLE_COUNT = 64u;

// Map (UV, face) → world-space cubemap direction. Engine convention
// matches the GLSL original: +Y is "up", and faces follow the standard
// Vulkan/D3D12 cubemap face ordering.
float3 CubemapFaceDirection(float2 xUV, uint uFace)
{
	float2 xCoord = xUV * 2.0 - 1.0;
	float3 xDir;
	switch (uFace)
	{
		case 0u: xDir = float3( 1.0, -xCoord.y, -xCoord.x); break; // +X
		case 1u: xDir = float3(-1.0, -xCoord.y,  xCoord.x); break; // -X
		case 2u: xDir = float3( xCoord.x,  1.0,  xCoord.y); break; // +Y
		case 3u: xDir = float3( xCoord.x, -1.0, -xCoord.y); break; // -Y
		case 4u: xDir = float3( xCoord.x, -xCoord.y,  1.0); break; // +Z
		case 5u: xDir = float3(-xCoord.x, -xCoord.y, -1.0); break; // -Z
		default: xDir = float3(0.0, 1.0, 0.0); break;
	}
	return normalize(xDir);
}

float3 SampleEnvironment(float3 xDir)
{
	if (g_xPassSet.IrradianceConstants.g_uUseAtmosphere > 0u)
	{
		// IBL samples the atmosphere from a fixed reference height — the
		// IBL textures are pre-computed environment lighting, not camera-
		// relative. g_xSunDirection is the CAPTURED direction light TRAVELS,
		// so we negate to get the direction TO the sun. Deliberately NOT the
		// VIEW set's live per-frame sun accessor: reading that here let one
		// amortised 6-frame generation span several sun positions.
		float3 xSunDir = normalize(-g_xPassSet.IrradianceConstants.g_xSunDirection.xyz);
		EnvironmentCaptureMedium xMedium;
		xMedium.fRayleighScale       = g_xPassSet.IrradianceConstants.g_fRayleighScale;
		xMedium.fMieScale            = g_xPassSet.IrradianceConstants.g_fMieScale;
		xMedium.fMieG                = g_xPassSet.IrradianceConstants.g_fMieG;
		xMedium.fRayleighScaleHeight = g_xPassSet.IrradianceConstants.g_fRayleighScaleHeight;
		xMedium.fMieScaleHeight      = g_xPassSet.IrradianceConstants.g_fMieScaleHeight;
		xMedium.fGroundAlbedo        = g_xPassSet.IrradianceConstants.g_fGroundAlbedo;
		AtmosphereResult xResult = ComputeEnvironmentCaptureScattering(
			float3(0.0, EARTH_RADIUS + IBL_REFERENCE_HEIGHT_METERS, 0.0),
			normalize(xDir),
			xSunDir,
			g_xPassSet.IrradianceConstants.g_fSunIntensity,
			xMedium,
			g_xPassSet.g_xMultiScatterLUT,
			g_xPassSet.IrradianceConstants.g_uMultiScatteringEnabled != 0u);
		return xResult.xColor;
	}
	return g_xPassSet.g_xSkyboxCubemap.Sample(xDir).rgb;
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float3 xNormal = CubemapFaceDirection(input.uv, g_xPassSet.IrradianceConstants.g_uFaceIndex);

	// Tangent basis from the surface normal. Special-case near-pole to
	// avoid degenerate cross-product.
	float3 xUp    = abs(xNormal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	float3 xRight = normalize(cross(xUp, xNormal));
	xUp           = cross(xNormal, xRight);

	float3 xIrradiance = float3(0.0, 0.0, 0.0);

	for (uint i = 0u; i < SAMPLE_COUNT; i++)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);

		// Cosine-weighted hemisphere transform: cosθ = sqrt(1 - u₁),
		// sinθ = sqrt(u₁), φ = 2π·u₂.
		float fCosTheta = sqrt(1.0 - Xi.x);
		float fSinTheta = sqrt(Xi.x);
		float fPhi      = TWO_PI * Xi.y;

		float3 xTangentSample = float3(fSinTheta * cos(fPhi),
										fSinTheta * sin(fPhi),
										fCosTheta);
		float3 xSampleDir = xTangentSample.x * xRight +
							xTangentSample.y * xUp +
							xTangentSample.z * xNormal;

		// Cosine weight is baked into the PDF — no explicit cos multiplier.
		xIrradiance += SampleEnvironment(xSampleDir);
	}

	// Cosine-weighted importance sampling: the π factors in the integral
	// and PDF cancel, so the result is the simple sample average.
	xIrradiance /= (float)SAMPLE_COUNT;
	return float4(xIrradiance, 1.0);
}
