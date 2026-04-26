// =====================================================================
// Common.Volumetric — shared Slang module.
//
// Slang port of Flux_VolumetricCommon.fxh: Beer-Lambert extinction,
// phase functions (Henyey-Greenstein, Schlick, dual-lobe, Rayleigh),
// froxel coordinate conversions, in-scattering integration, height
// fog, and temporal jitter helpers.
//
// Constants describing scene-scale defaults stay public as runtime
// fallbacks — engine code typically supplies overrides through fog
// constant buffers, but we still need a sensible value for shaders
// that don't bind one.
// =====================================================================

import Common.PBR; // PI, etc.

// ----- Physical defaults (fallbacks when CB doesn't override) -----

public static const float FOG_AMBIENT_IRRADIANCE_RATIO_DEFAULT = 0.25;

// Beer-Lambert transmittance at density=0.001 over 1m is ~99.9% — below
// this the contribution is imperceptible. Used to skip near-empty samples.
public static const float FOG_DENSITY_SKIP_THRESHOLD     = 0.001;
// Transmittance 0.01 = 1% light passes through ≈ effectively opaque.
public static const float FOG_TRANSMITTANCE_EARLY_EXIT   = 0.01;

// Artistic — not physically derived. 0.01 maps fog features to ~100 m wide.
public static const float FOG_NOISE_WORLD_SCALE_DEFAULT  = 0.01;

// UI density slider (0..0.01) → physical extinction in m⁻¹.
public static const float FOG_UI_DENSITY_SCALE           = 100.0;

public float UIToPhysicalDensity(float uiDensity)
{
	return uiDensity * FOG_UI_DENSITY_SCALE;
}

// ----- Beer-Lambert -----

public float BeerLambert(float density, float distance)
{
	return exp(-density * distance);
}

public float BeerLambertExtinction(float sigmaS, float sigmaA, float distance)
{
	return exp(-(sigmaS + sigmaA) * distance);
}

// HZD-style powder term — softens very dense centres of clouds.
public float BeerLambertPowder(float density, float distance, float powder)
{
	float beer       = BeerLambert(density, distance);
	float beerPowder = BeerLambert(density * powder, distance);
	return lerp(beer, beerPowder, 0.5);
}

// ----- Phase functions (scattering direction distributions) -----

// Henyey-Greenstein. g = -1 (back) … 0 (isotropic) … 1 (forward).
public float HenyeyGreenstein(float cosTheta, float g)
{
	float g2    = g * g;
	float denom = 1.0 + g2 - 2.0 * g * cosTheta;
	return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
}

// Schlick approximation of HG — cheaper, visually close.
public float SchlickPhase(float cosTheta, float g)
{
	float k          = 1.55 * g - 0.55 * g * g * g;
	float kcosTheta  = k * cosTheta;
	float denom      = 1.0 + kcosTheta;
	return (1.0 - k * k) / (4.0 * PI * denom * denom);
}

// Dual-lobe — typical for clouds: strong forward + weak back scatter.
public float DualLobePhase(float cosTheta, float gForward, float gBackward, float blend)
{
	float phaseForward  = HenyeyGreenstein(cosTheta, gForward);
	float phaseBackward = HenyeyGreenstein(cosTheta, gBackward);
	return lerp(phaseBackward, phaseForward, blend);
}

public float RayleighPhase(float cosTheta)
{
	return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

// ----- Froxel (frustum-voxel) coordinate conversions -----

public static const float SAFE_NEAR_Z       = 0.001;
public static const float SAFE_DEPTH_RATIO  = 1.001;

public int3 UVDepthToFroxel(float2 uv, float linearDepth, int3 gridSize, float nearZ, float farZ)
{
	nearZ       = max(nearZ, SAFE_NEAR_Z);
	farZ        = max(farZ, nearZ * SAFE_DEPTH_RATIO);
	linearDepth = max(linearDepth, SAFE_NEAR_Z);

	int x = (int)(uv.x * (float)gridSize.x);
	int y = (int)(uv.y * (float)gridSize.y);

	// Exponential Z slicing — more resolution near the camera.
	float expDepth = log(linearDepth / nearZ) / log(farZ / nearZ);
	expDepth = saturate(expDepth);
	int z = (int)(expDepth * (float)(gridSize.z - 1));

	return int3(clamp(x, 0, gridSize.x - 1),
				clamp(y, 0, gridSize.y - 1),
				clamp(z, 0, gridSize.z - 1));
}

public float3 FroxelToWorld(int3 froxelCoord, int3 gridSize, float4x4 invViewProj, float nearZ, float farZ)
{
	nearZ = max(nearZ, SAFE_NEAR_Z);
	farZ  = max(farZ, nearZ * SAFE_DEPTH_RATIO);

	float2 uv  = (float2(froxelCoord.xy) + 0.5) / float2(gridSize.xy);
	float2 ndc = uv * 2.0 - 1.0;

	float sliceNorm   = ((float)froxelCoord.z + 0.5) / (float)gridSize.z;
	float linearDepth = nearZ * pow(farZ / nearZ, sliceNorm);

	float4 clipPos = float4(ndc, 0.0, 1.0);
	float4 viewPos = mul(invViewProj, clipPos);
	float3 viewDir = normalize(viewPos.xyz / viewPos.w);
	return viewDir * linearDepth;
}

public float FroxelSliceToDepth(int slice, int numSlices, float nearZ, float farZ)
{
	nearZ = max(nearZ, SAFE_NEAR_Z);
	farZ  = max(farZ, nearZ * SAFE_DEPTH_RATIO);
	float sliceNorm = ((float)slice + 0.5) / (float)numSlices;
	return nearZ * pow(farZ / nearZ, sliceNorm);
}

public int DepthToFroxelSlice(float depth, int numSlices, float nearZ, float farZ)
{
	nearZ = max(nearZ, SAFE_NEAR_Z);
	farZ  = max(farZ, nearZ * SAFE_DEPTH_RATIO);
	depth = max(depth, SAFE_NEAR_Z);
	float sliceNorm = log(depth / nearZ) / log(farZ / nearZ);
	return clamp((int)(sliceNorm * (float)numSlices), 0, numSlices - 1);
}

// ----- In-scattering integration -----

// Physically-conserved single-scattering: scattered radiance = albedo *
// extinction * phase * transmittance. albedo = sigma_s / (sigma_s + sigma_a)
// guarantees scattered ≤ extinct.
public float3 InScatteringStep(float3 lightColor,
								 float  density,
								 float  scatteringCoeff,
								 float  absorptionCoeff,
								 float  phase,
								 float  transmittance)
{
	float sigmaT = scatteringCoeff + absorptionCoeff;
	float albedo = (sigmaT > 0.0001) ? (scatteringCoeff / sigmaT) : 0.0;
	float scattering = density * scatteringCoeff * phase;
	return lightColor * albedo * scattering * transmittance;
}

// Legacy overload — zero absorption (pure scattering, albedo = 1).
public float3 InScatteringStep(float3 lightColor,
								 float  density,
								 float  scatteringCoeff,
								 float  phase,
								 float  transmittance)
{
	return InScatteringStep(lightColor, density, scatteringCoeff, 0.0, phase, transmittance);
}

public float ExponentialShadow(float depth, float shadowMapDepth, float c)
{
	return saturate(exp(-c * (depth - shadowMapDepth)));
}

// ----- Height fog -----

public float HeightFogDensity(float worldY, float baseHeight, float falloff)
{
	return exp(-max(0.0, worldY - baseHeight) * falloff);
}

public float GroundFogDensity(float worldY, float groundLevel, float thickness, float density)
{
	float heightAboveGround = worldY - groundLevel;
	if (heightAboveGround < 0.0)        return density;
	if (heightAboveGround > thickness)  return 0.0;
	float t = heightAboveGround / thickness;
	return density * (1.0 - t * t); // quadratic falloff
}

// ----- Temporal jitter -----

public float3 ApplyTemporalJitter(float3 rayOrigin, float3 rayDir, float2 jitter, float stepSize)
{
	float jitterAmount = (jitter.x * 0.5 + 0.5) * stepSize;
	return rayOrigin + rayDir * jitterAmount;
}

// Blue-noise dither offset — uses pixel coord modulo texture dimensions
// then applies golden-ratio temporal shift. Sampled at LOD 0 via texel-
// centre UV so the result matches GLSL `texelFetch` regardless of the
// sampler's filter mode.
public float BlueNoiseOffset(Sampler2D blueNoiseTex, int2 pixelCoord, int frameIndex)
{
	uint uW, uH;
	blueNoiseTex.GetDimensions(uW, uH);
	int2   coord    = pixelCoord % int2((int)uW, (int)uH);
	float2 noiseUV  = (float2(coord) + 0.5) / float2((float)uW, (float)uH);
	float  noise    = blueNoiseTex.SampleLevel(noiseUV, 0).r;
	float  goldenRatio = 1.61803398875;
	return frac(noise + (float)frameIndex * goldenRatio);
}
