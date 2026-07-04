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

struct IrradianceConstantsLayout
{
	uint  g_uUseAtmosphere; // 1 = procedural sky, 0 = cubemap
	float g_fSunIntensity;
	uint  g_uFaceIndex;     // 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z
	float g_fPad;
};
struct PassParams
{
	ConstantBuffer<IrradianceConstantsLayout> IrradianceConstants;
	SamplerCube g_xSkyboxCubemap;
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
		// relative. g_xSunDir_Pad is the direction light TRAVELS, so we
		// negate to get the direction TO the sun.
		float3 xSunDir = normalize(-GetSunDir_Pad().xyz);
		AtmosphereResult xResult = ComputeAtmosphereScattering(
			float3(0.0, EARTH_RADIUS + IBL_REFERENCE_HEIGHT_METERS, 0.0),
			normalize(xDir),
			xSunDir,
			g_xPassSet.IrradianceConstants.g_fSunIntensity);
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
