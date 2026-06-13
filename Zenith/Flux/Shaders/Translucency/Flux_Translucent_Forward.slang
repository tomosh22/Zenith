// =====================================================================
// Flux_Translucent_Forward — Slang module.
//
// Subsystem: Translucency. Forward-lit pass for Translucent/Additive
// blend-mode materials (and the editor material preview, which reuses
// this program against its own targets). Evaluates the shared
// uber-material surface and lights it inline with the same recipe as
// the deferred composite: IBL ambient (BRDF LUT + irradiance +
// prefiltered, multiscatter, clear-coat lobe), the sun with 4-cascade
// PCF shadows, and the clustered dynamic lights. No SSR/SSGI (screen-
// space effects only track opaque geometry).
//
// Outputs premultipliable (colour, alpha) into the HDR scene target;
// the pipeline's blend state decides translucent vs additive.
//
// CSM texel size arrives through TranslucencyConstants — never
// GetDimensions() (engine rule: dims via CBV).
//
// set=1 bindings mirror Flux_StaticMesh_ToGBuffer (DrawConstants b0 +
// 9 material textures b1-9) so the C++ side shares the bind helper.
// =====================================================================

import Common.Frame;
import Common.DrawConstants;
import Common.Material;
import Common.MaterialSurface;
import Common.GBuffer;
import Common.PBR;
import Common.Lighting;

struct VsIn
{
	float3 a_xPosition  : POSITION;
	float2 a_xUV        : TEXCOORD0;
	float3 a_xNormal    : NORMAL;
	float3 a_xTangent   : TANGENT;
	float3 a_xBitangent : BINORMAL;
	float4 a_xColor     : COLOR;
};

struct VsOut
{
	float4 position        : SV_Position;
	float2 uv              : TEXCOORD0;
	float3 worldNormal     : TEXCOORD1;
	float3 worldPos        : TEXCOORD2;
	float3 worldTangent    : TEXCOORD3;
	float  bitangentSign   : TEXCOORD4;
	float4 vertexColor     : TEXCOORD5;
};

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float3 xWorldPos = mul(DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	float3x3 xNormalMatrix = NormalMatrixFromModel(DrawConstants.g_xModelMatrix);

	o.uv             = input.a_xUV;
	o.vertexColor    = input.a_xColor;
	o.worldNormal    = normalize(mul(xNormalMatrix, input.a_xNormal));
	o.worldTangent   = normalize(mul(xNormalMatrix, input.a_xTangent));
	float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
	o.bitangentSign  = sign(dot(xBitangent, cross(o.worldNormal, o.worldTangent)));
	o.worldPos       = xWorldPos;
	o.position       = mul(FrameConstants.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

// ----- Pass constants (set=0) -----

struct TranslucencyConstantsLayout
{
	uint   g_bIBLEnabled;
	uint   g_bIBLDiffuseEnabled;
	uint   g_bIBLSpecularEnabled;
	float  g_fIBLIntensity;
	uint   g_bShadowsEnabled;
	uint   g_bDynamicLightsEnabled;	// preview disables the scene's cluster walk
	float  g_fAmbientFallbackIntensity;
	float2 g_xCSMTexelSize;	// 1 / CSM resolution (engine rule: dims via CBV)
};
[[vk::binding( 1, 0)]] ConstantBuffer<TranslucencyConstantsLayout> TranslucencyConstants;

struct ShadowMatrixLayout { float4x4 m_xMat; };
[[vk::binding( 2, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix0;
[[vk::binding( 3, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix1;
[[vk::binding( 4, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix2;
[[vk::binding( 5, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix3;

[[vk::binding( 6, 0)]] Sampler2D   g_xCSM0;
[[vk::binding( 7, 0)]] Sampler2D   g_xCSM1;
[[vk::binding( 8, 0)]] Sampler2D   g_xCSM2;
[[vk::binding( 9, 0)]] Sampler2D   g_xCSM3;

[[vk::binding(10, 0)]] Sampler2D   g_xBRDFLUT;
[[vk::binding(11, 0)]] SamplerCube g_xIrradianceMap;
[[vk::binding(12, 0)]] SamplerCube g_xPrefilteredMap;

[[vk::binding(13, 0)]] StructuredBuffer<LightInstance> LightBuffer;
[[vk::binding(14, 0)]] StructuredBuffer<uint>          ClusterLightCounts;
[[vk::binding(15, 0)]] StructuredBuffer<uint>          ClusterLightIndices;

// ----- Material textures (set=1, static-mesh layout) -----

[[vk::binding( 1, 1)]] Sampler2D g_xBaseColorTex;
[[vk::binding( 2, 1)]] Sampler2D g_xNormalTex;
[[vk::binding( 3, 1)]] Sampler2D g_xRoughnessMetallicTex;
[[vk::binding( 4, 1)]] Sampler2D g_xOcclusionTex;
[[vk::binding( 5, 1)]] Sampler2D g_xEmissiveTex;
[[vk::binding( 6, 1)]] Sampler2D g_xHeightTex;
[[vk::binding( 7, 1)]] Sampler2D g_xDetailAlbedoTex;
[[vk::binding( 8, 1)]] Sampler2D g_xDetailNormalTex;
[[vk::binding( 9, 1)]] Sampler2D g_xDetailMaskTex;

// ----- Shadow sampling (deferred recipe, texel size from CB) -----

static const float SHADOW_MIN_BIAS        = 0.0005;
static const float SHADOW_MAX_BIAS        = 0.005;
static const float PCF_OFFSET_INNER       = 0.5;
static const float PCF_OFFSET_OUTER       = 1.5;
static const float PCF_SAMPLE_COUNT       = 16.0;
static const float CASCADE_BLEND_DISTANCE = 0.15;

float SampleCascadeShadow(int iCascade, float2 xSamplePos, float fBiasedDepth, float2 texelSize)
{
	float fShadow = 0.0;

	float2 axOffsets[4] = {
		float2(-PCF_OFFSET_OUTER, -PCF_OFFSET_OUTER) * texelSize,
		float2( PCF_OFFSET_INNER, -PCF_OFFSET_OUTER) * texelSize,
		float2(-PCF_OFFSET_OUTER,  PCF_OFFSET_INNER) * texelSize,
		float2( PCF_OFFSET_INNER,  PCF_OFFSET_INNER) * texelSize,
	};

	for (int i = 0; i < 4; i++)
	{
		float2 xSampleUV = xSamplePos + axOffsets[i];
		float4 shadowDepths;
		if      (iCascade == 0) shadowDepths = g_xCSM0.Gather(xSampleUV);
		else if (iCascade == 1) shadowDepths = g_xCSM1.Gather(xSampleUV);
		else if (iCascade == 2) shadowDepths = g_xCSM2.Gather(xSampleUV);
		else                    shadowDepths = g_xCSM3.Gather(xSampleUV);

		float4 comparison = step(float4(fBiasedDepth, fBiasedDepth, fBiasedDepth, fBiasedDepth), shadowDepths);
		fShadow += dot(comparison, float4(1.0, 1.0, 1.0, 1.0));
	}

	return fShadow / PCF_SAMPLE_COUNT;
}

float ComputeSunShadowFactor(float3 xWorldPos, float fNdotL)
{
	if (TranslucencyConstants.g_bShadowsEnabled == 0u) return 1.0;

	const uint uNumCSMs = 4;
	float4x4 axShadowMats[4] = {
		ShadowMatrix0.m_xMat,
		ShadowMatrix1.m_xMat,
		ShadowMatrix2.m_xMat,
		ShadowMatrix3.m_xMat,
	};

	float2 texelSize = TranslucencyConstants.g_xCSMTexelSize;
	float fShadowFactor = 1.0;

	for (int iCascade = 0; iCascade < (int)uNumCSMs; iCascade++)
	{
		float4 xShadowSpace = mul(axShadowMats[iCascade], float4(xWorldPos, 1.0));
		float2 xSamplePos   = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float  fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

		if (xSamplePos.x < 0.0 || xSamplePos.x > 1.0 ||
			xSamplePos.y < 0.0 || xSamplePos.y > 1.0) continue;
		if (fCurrentDepth < 0.0 || fCurrentDepth > 1.0) continue;

		float fBias        = max(SHADOW_MIN_BIAS, SHADOW_MAX_BIAS * (1.0 - fNdotL));
		float fBiasedDepth = fCurrentDepth - fBias;

		float2 xEdgeDist    = min(xSamplePos, 1.0 - xSamplePos);
		float  fMinEdgeDist = min(xEdgeDist.x, xEdgeDist.y);
		bool   bInBlendZone = (fMinEdgeDist < CASCADE_BLEND_DISTANCE) &&
							  (iCascade < (int)uNumCSMs - 1);

		float fShadow1 = SampleCascadeShadow(iCascade, xSamplePos, fBiasedDepth, texelSize);

		if (bInBlendZone)
		{
			float fBlendWeight      = 1.0 - (fMinEdgeDist / CASCADE_BLEND_DISTANCE);
			float4 xNextShadowSpace = mul(axShadowMats[iCascade + 1], float4(xWorldPos, 1.0));
			float2 xNextSamplePos   = xNextShadowSpace.xy / xNextShadowSpace.w * 0.5 + 0.5;
			float  fNextDepth       = xNextShadowSpace.z / xNextShadowSpace.w;

			if (xNextSamplePos.x >= 0.0 && xNextSamplePos.x <= 1.0 &&
				xNextSamplePos.y >= 0.0 && xNextSamplePos.y <= 1.0 &&
				fNextDepth >= 0.0 && fNextDepth <= 1.0)
			{
				float fNextBiasedDepth = fNextDepth - fBias;
				float fShadow2 = SampleCascadeShadow(iCascade + 1, xNextSamplePos, fNextBiasedDepth, texelSize);
				fShadowFactor = lerp(fShadow1, fShadow2, fBlendWeight);
			}
			else
			{
				fShadowFactor = fShadow1;
			}
		}
		else
		{
			fShadowFactor = fShadow1;
		}

		break;
	}

	return fShadowFactor;
}

// ----- IBL ambient (deferred recipe minus SSR/SSGI) -----

float3 ComputeForwardIBLAmbient(MaterialSurface s, float3 xViewDir)
{
	if (TranslucencyConstants.g_bIBLEnabled == 0u)
	{
		return float3(TranslucencyConstants.g_fAmbientFallbackIntensity).xxx * s.albedo.rgb * s.ao;
	}

	float  NdotV = max(dot(s.worldNormal, xViewDir), PBR_EPSILON);
	float3 F0    = ComputeF0(s.albedo.rgb, s.metallic, s.specular);
	float3 F     = FresnelSchlickRoughness(NdotV, F0, s.roughness);

	float  fRoughnessForLUT = clamp(s.roughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	float2 xBRDF = g_xBRDFLUT.Sample(float2(NdotV, fRoughnessForLUT)).rg;

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - s.metallic);

	float3 xDiffuseIBL = float3(0.0, 0.0, 0.0);
	if (TranslucencyConstants.g_bIBLDiffuseEnabled != 0u)
	{
		float3 xIrradiance = g_xIrradianceMap.Sample(s.worldNormal).rgb;
		xDiffuseIBL        = kD * xIrradiance * s.albedo.rgb;
	}

	float3 xSpecularIBL = float3(0.0, 0.0, 0.0);
	if (TranslucencyConstants.g_bIBLSpecularEnabled != 0u)
	{
		float3 xReflect = reflect(-xViewDir, s.worldNormal);
		float  fLOD     = s.roughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xPrefiltered = g_xPrefilteredMap.SampleLevel(xReflect, fLOD).rgb;
		xSpecularIBL = xPrefiltered * MultiscatterBRDF(F0, NdotV, fRoughnessForLUT, xBRDF);
	}

	// Specular AO remap (Lagarde/de Rousiers 2014).
	float fSpecularAO = saturate(
		pow(NdotV + s.ao, exp2(-16.0 * s.roughness - 1.0))
		- 1.0 + s.ao);

	float3 xBase = xDiffuseIBL * s.ao + xSpecularIBL * fSpecularAO;

	// Clear-coat IBL lobe.
	if (s.ccStrength > 0.0 && TranslucencyConstants.g_bIBLSpecularEnabled != 0u)
	{
		float fCoatFresnel = PBR_CLEARCOAT_F0 + (1.0 - PBR_CLEARCOAT_F0) * pow(saturate(1.0 - NdotV), PBR_FRESNEL_POWER);
		float fCoatRoughForLUT = clamp(s.ccRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
		float2 xCoatBRDF = g_xBRDFLUT.Sample(float2(NdotV, fCoatRoughForLUT)).rg;
		float3 xReflect = reflect(-xViewDir, s.worldNormal);
		float  fCoatLOD = s.ccRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xCoatPrefiltered = g_xPrefilteredMap.SampleLevel(xReflect, fCoatLOD).rgb;
		float3 xCoatSpecular = xCoatPrefiltered * (PBR_CLEARCOAT_F0 * xCoatBRDF.x + xCoatBRDF.y) * s.ccStrength;
		xBase = xBase * (1.0 - s.ccStrength * fCoatFresnel) + xCoatSpecular * fSpecularAO;
	}

	return xBase * TranslucencyConstants.g_fIBLIntensity;
}

// ----- Sun direct lighting (deferred recipe over a MaterialSurface) -----

float3 ComputeSunDirect(MaterialSurface s, float3 xViewDir, float3 xWorldPos)
{
	float3 xLightDir = normalize(-FrameConstants.g_xSunDir_Pad.xyz);
	float3 xHalfDir  = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(s.worldNormal, xLightDir), 0.0);
	float NdotV = max(dot(s.worldNormal, xViewDir),  PBR_EPSILON);
	float NdotH = max(dot(s.worldNormal, xHalfDir),  0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	if (NdotL <= PBR_EPSILON) return float3(0.0, 0.0, 0.0);
	if (FrameConstants.g_xSunColour.a <= 0.1) return float3(0.0, 0.0, 0.0);

	float roughness = max(s.roughness, PBR_MIN_ROUGHNESS);
	float3 F0 = ComputeF0(s.albedo.rgb, s.metallic, s.specular);

	float  D = DistributionGGX(NdotH, roughness);
	float  G = GeometrySmith_Direct(NdotV, NdotL, roughness);
	float3 F = FresnelSchlick(HdotV, F0);

	float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, PBR_EPSILON);

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - s.metallic);
	float3 Lo = kD * s.albedo.rgb / PI + specular;

	if (s.ccStrength > 0.0)
	{
		float fBaseAttenuation;
		float3 xCoat = ClearCoatLobe_Direct(NdotH, NdotV, NdotL, HdotV,
											 s.ccStrength, s.ccRoughness, fBaseAttenuation);
		Lo = Lo * fBaseAttenuation + xCoat;
	}

	float3 xRadiance = FrameConstants.g_xSunColour.rgb * FrameConstants.g_xSunColour.a;
	float fShadow = ComputeSunShadowFactor(xWorldPos, NdotL);
	return Lo * xRadiance * NdotL * fShadow;
}

[shader("fragment")]
float4 fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace) : SV_Target
{
	float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3 xViewDir = normalize(FrameConstants.g_xCamPos_Pad.xyz - input.worldPos);

	MaterialSurface xSurface = EvaluateMaterialSurface(
		g_xBaseColorTex, g_xNormalTex, g_xRoughnessMetallicTex,
		g_xOcclusionTex, g_xEmissiveTex, g_xHeightTex,
		g_xDetailAlbedoTex, g_xDetailNormalTex, g_xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace);

	float fAlpha = xSurface.albedo.a;

	// Unlit: base colour + emissive, no lighting.
	if (xSurface.uShadingModel == GBUFFER_SHADING_UNLIT)
	{
		return float4(xSurface.albedo.rgb + xSurface.emissive, fAlpha);
	}

	float3 xColour = ComputeForwardIBLAmbient(xSurface, xViewDir);
	xColour += ComputeSunDirect(xSurface, xViewDir, input.worldPos);

	// Clustered dynamic lights — same cluster walk as the deferred composite,
	// view-space Z computed from the world position (forward has no depth tex).
	if (TranslucencyConstants.g_bDynamicLightsEnabled != 0u)
	{
		float fViewZ = mul(FrameConstants.g_xViewMat, float4(input.worldPos, 1.0)).z;
		float2 xScreenUV = input.position.xy * FrameConstants.g_xRcpScreenDims;
		uint3 xClusterCoord = ClusterCoordFromUVDepth(xScreenUV, fViewZ);
		uint  uClusterIdx   = ClusterIndex(xClusterCoord);
		uint  uLightCount   = ClusterLightCounts[uClusterIdx];

		float3 xDynLights = float3(0.0, 0.0, 0.0);
		for (uint i = 0u; i < uLightCount; ++i)
		{
			uint uLightIdx = ClusterLightIndices[uClusterIdx * MAX_LIGHTS_PER_CLUSTER + i];
			LightInstance xL = LightBuffer[uLightIdx];
			xDynLights += EvaluateLight(xL, input.worldPos, xSurface.worldNormal, xViewDir,
			                            xSurface.albedo.rgb, xSurface.metallic, xSurface.roughness,
			                            xSurface.specular, g_xBRDFLUT);
		}
		xColour += xDynLights;
	}

	// Emissive bypasses lighting.
	xColour += xSurface.emissive;

	return float4(xColour, fAlpha);
}
