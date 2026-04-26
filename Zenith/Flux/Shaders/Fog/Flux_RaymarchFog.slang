// =====================================================================
// Flux_RaymarchFog — Slang module.
//
// Subsystem: Fog. Per-pixel ray-marched volumetric fog (technique 2).
// Walks the camera ray from origin to the depth-buffer hit, sampling
// density (from a 3D noise volume + height falloff), running Henyey-
// Greenstein scattering, sampling cascaded shadow maps for volumetric
// shadows, and accumulating Beer-Lambert transmittance.
//
// Imports Common.Frame (FrameConstants, GetWorldPosFromDepthTex) and
// Common.Volumetric (phase functions, in-scattering, height fog,
// BlueNoiseOffset, density / transmittance thresholds).
// =====================================================================

import Common.Fullscreen;
import Common.Frame;
import Common.Volumetric;

struct RaymarchConstantsLayout
{
	float4 u_xFogColour;
	float4 u_xFogParams;        // x = density, y = scattering, z = absorption, w = max distance
	float4 u_xNoiseParams;      // x = scale, y = speed, z = detail, w = time
	float4 u_xHeightParams;     // x = base height, y = falloff
	uint   u_uNumSteps;
	uint   u_uDebugMode;
	uint   u_uFrameIndex;
	float  u_fPhaseG;
	// Volumetric shadow runtime params (unified with Froxel fog).
	float  u_fVolShadowBias;
	float  u_fVolShadowConeRadius;
	float  u_fAmbientIrradianceRatio;
	float  u_fNoiseWorldScale;
};
[[vk::binding(1, 0)]] ConstantBuffer<RaymarchConstantsLayout> RaymarchConstants;
[[vk::binding(2, 0)]] Sampler2D u_xDepthTexture;
[[vk::binding(3, 0)]] Sampler3D u_xNoiseTexture3D;
[[vk::binding(4, 0)]] Sampler2D u_xBlueNoiseTexture;
[[vk::binding(5, 0)]] Sampler2D u_xCSM0;
[[vk::binding(6, 0)]] Sampler2D u_xCSM1;
[[vk::binding(7, 0)]] Sampler2D u_xCSM2;
[[vk::binding(8, 0)]] Sampler2D u_xCSM3;

struct ShadowMatrixLayout { float4x4 u_xShadowMat; };
[[vk::binding( 9, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix0;
[[vk::binding(10, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix1;
[[vk::binding(11, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix2;
[[vk::binding(12, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix3;

static const uint DEBUG_RAYMARCH_STEP_COUNT          = 9;
static const uint DEBUG_RAYMARCH_ACCUMULATED_DENSITY = 10;
static const uint DEBUG_RAYMARCH_NOISE_SAMPLE        = 11;
static const uint DEBUG_RAYMARCH_JITTER_PATTERN      = 12;
static const uint DEBUG_RAYMARCH_SHADOW              = 13;

static const int VOL_SHADOW_SAMPLES = 4;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float SampleVolumetricShadow(float3 worldPos)
{
	float4x4 axShadowMats[4] = {
		ShadowMatrix0.u_xShadowMat,
		ShadowMatrix1.u_xShadowMat,
		ShadowMatrix2.u_xShadowMat,
		ShadowMatrix3.u_xShadowMat,
	};

	const float2 axOffsets[4] = {
		float2(-0.7071, -0.7071),
		float2( 0.7071, -0.7071),
		float2(-0.7071,  0.7071),
		float2( 0.7071,  0.7071),
	};

	for (int iCascade = 0; iCascade < 4; iCascade++)
	{
		float4 xShadowSpace = mul(axShadowMats[iCascade], float4(worldPos, 1.0));
		float2 xSamplePos   = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float  fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

		if (xSamplePos.x < 0.0 || xSamplePos.x > 1.0 ||
			xSamplePos.y < 0.0 || xSamplePos.y > 1.0 ||
			fCurrentDepth < 0.0 || fCurrentDepth > 1.0) continue;

		float fBiasedDepth = fCurrentDepth - RaymarchConstants.u_fVolShadowBias;
		float fShadow      = 0.0;
		for (int i = 0; i < VOL_SHADOW_SAMPLES; i++)
		{
			float2 xSampleUV = xSamplePos + axOffsets[i] * RaymarchConstants.u_fVolShadowConeRadius;
			float  fShadowDepth;
			if      (iCascade == 0) fShadowDepth = u_xCSM0.Sample(xSampleUV).r;
			else if (iCascade == 1) fShadowDepth = u_xCSM1.Sample(xSampleUV).r;
			else if (iCascade == 2) fShadowDepth = u_xCSM2.Sample(xSampleUV).r;
			else                    fShadowDepth = u_xCSM3.Sample(xSampleUV).r;
			fShadow += (fBiasedDepth <= fShadowDepth) ? 1.0 : 0.0;
		}
		return fShadow / (float)VOL_SHADOW_SAMPLES;
	}
	return 1.0;
}

float SampleDensity(float3 worldPos)
{
	float baseDensity   = UIToPhysicalDensity(RaymarchConstants.u_xFogParams.x);
	float heightDensity = HeightFogDensity(worldPos.y,
											RaymarchConstants.u_xHeightParams.x,
											RaymarchConstants.u_xHeightParams.y);

	// Animated 3D noise — Y component drifts over time. World-scale brings
	// noise frequency into a consistent range regardless of camera position.
	float3 noiseCoord = worldPos * RaymarchConstants.u_xNoiseParams.x;
	noiseCoord.y    += RaymarchConstants.u_xNoiseParams.w;
	float  noise      = u_xNoiseTexture3D.Sample(noiseCoord * RaymarchConstants.u_fNoiseWorldScale).r;

	return baseDensity * heightDensity * noise;
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	float3 worldPos  = GetWorldPosFromDepthTex(u_xDepthTexture, input.uv);
	float3 cameraPos = FrameConstants.g_xCamPos_Pad.xyz;

	float3 rayDir      = normalize(worldPos - cameraPos);
	float  maxDistance = min(length(worldPos - cameraPos), RaymarchConstants.u_xFogParams.w);

	int2  pixelCoord = (int2)fragCoord.xy;
	float jitter     = BlueNoiseOffset(u_xBlueNoiseTexture, pixelCoord, (int)RaymarchConstants.u_uFrameIndex);

	if (RaymarchConstants.u_uDebugMode == DEBUG_RAYMARCH_JITTER_PATTERN)
		return float4(jitter, jitter, jitter, 1.0);

	float stepSize  = maxDistance / (float)RaymarchConstants.u_uNumSteps;
	float rayOffset = jitter * stepSize;

	float3 accumulatedLight        = float3(0.0, 0.0, 0.0);
	float  accumulatedTransmittance = 1.0;
	float  accumulatedDensity      = 0.0;
	float  lastNoiseSample         = 0.0;
	float  lastShadowSample        = 1.0;
	uint   actualSteps             = 0;

	float3 lightDir   = normalize(-FrameConstants.g_xSunDir_Pad.xyz);
	float3 lightColor = FrameConstants.g_xSunColour.rgb;

	for (uint i = 0; i < RaymarchConstants.u_uNumSteps; i++)
	{
		float t = rayOffset + (float)i * stepSize;
		if (t >= maxDistance) break;

		float3 samplePos = cameraPos + rayDir * t;
		float  density   = SampleDensity(samplePos);
		lastNoiseSample  = density;

		if (density > FOG_DENSITY_SKIP_THRESHOLD)
		{
			actualSteps++;
			accumulatedDensity += density * stepSize;

			float shadowTerm = SampleVolumetricShadow(samplePos);
			lastShadowSample = shadowTerm;

			float extinction = BeerLambertExtinction(
				RaymarchConstants.u_xFogParams.y * density,
				RaymarchConstants.u_xFogParams.z * density,
				stepSize);

			float cosTheta = dot(rayDir, lightDir);
			float phase    = HenyeyGreenstein(cosTheta, RaymarchConstants.u_fPhaseG);

			// Shadow attenuates only the direct light. The Common.Volumetric
			// helper applies energy-conserving albedo normalisation.
			float3 inScatter = InScatteringStep(
				lightColor * shadowTerm,
				density,
				RaymarchConstants.u_xFogParams.y,
				RaymarchConstants.u_xFogParams.z,
				phase,
				accumulatedTransmittance);

			accumulatedLight        += inScatter * stepSize;
			accumulatedTransmittance *= extinction;

			if (accumulatedTransmittance < FOG_TRANSMITTANCE_EARLY_EXIT) break;
		}
	}

	if (RaymarchConstants.u_uDebugMode == DEBUG_RAYMARCH_STEP_COUNT)
	{
		float  stepRatio = (float)actualSteps / (float)RaymarchConstants.u_uNumSteps;
		float3 heatColor = lerp(float3(0.0, 1.0, 0.0), float3(1.0, 0.0, 0.0), stepRatio);
		return float4(heatColor, 1.0);
	}
	if (RaymarchConstants.u_uDebugMode == DEBUG_RAYMARCH_ACCUMULATED_DENSITY)
	{
		float densityVis = 1.0 - exp(-accumulatedDensity * 0.5);
		return float4(densityVis, densityVis, densityVis, 1.0);
	}
	if (RaymarchConstants.u_uDebugMode == DEBUG_RAYMARCH_NOISE_SAMPLE)
		return float4(lastNoiseSample, lastNoiseSample, lastNoiseSample, 1.0);
	if (RaymarchConstants.u_uDebugMode == DEBUG_RAYMARCH_SHADOW)
		return float4(lastShadowSample, lastShadowSample, lastShadowSample, 1.0);

	float3 ambientLight = RaymarchConstants.u_xFogColour.rgb * RaymarchConstants.u_fAmbientIrradianceRatio;
	float3 finalFog     = accumulatedLight + ambientLight * (1.0 - accumulatedTransmittance);
	float  fogAlpha     = 1.0 - accumulatedTransmittance;
	return float4(finalFog, fogAlpha);
}
