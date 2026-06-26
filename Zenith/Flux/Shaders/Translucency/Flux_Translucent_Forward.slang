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

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.MaterialTable;
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

// ----- Pass constants + shared lighting resources (set 3) -----

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

struct PassParams
{
	ConstantBuffer<TranslucencyConstantsLayout> TranslucencyConstants;

	// All 4 cascade sun view×proj matrices in one StructuredBuffer (Phase 4a collapse).
	StructuredBuffer<float4x4> ShadowMatrices;

	Sampler2DArray g_xCSM;   // 4-cascade depth array (Phase 4b collapse)

	Sampler2D   g_xBRDFLUT;
	SamplerCube g_xIrradianceMap;
	SamplerCube g_xPrefilteredMap;

	StructuredBuffer<LightInstance> LightBuffer;
	StructuredBuffer<uint>          ClusterLightCounts;
	StructuredBuffer<uint>          ClusterLightIndices;
};
ParameterBlock<PassParams> g_xPassSet;

// ----- Per-draw constants + material table (set 4). Material textures are bindless (set 2). -----

struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	StructuredBuffer<MaterialGPU>       g_axMaterials;   // GPU material table (per-frame, bound per pass)
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float3 xWorldPos = mul(g_xDrawSet.DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	float3x3 xNormalMatrix = NormalMatrixFromModel(g_xDrawSet.DrawConstants.g_xModelMatrix);

	o.uv             = input.a_xUV;
	o.vertexColor    = input.a_xColor;
	o.worldNormal    = normalize(mul(xNormalMatrix, input.a_xNormal));
	o.worldTangent   = normalize(mul(xNormalMatrix, input.a_xTangent));
	float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
	o.bitangentSign  = sign(dot(xBitangent, cross(o.worldNormal, o.worldTangent)));
	o.worldPos       = xWorldPos;
	o.position       = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

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
		float4 shadowDepths = g_xPassSet.g_xCSM.Gather(float3(xSampleUV, float(iCascade)));

		float4 comparison = step(float4(fBiasedDepth, fBiasedDepth, fBiasedDepth, fBiasedDepth), shadowDepths);
		fShadow += dot(comparison, float4(1.0, 1.0, 1.0, 1.0));
	}

	return fShadow / PCF_SAMPLE_COUNT;
}

float ComputeSunShadowFactor(float3 xWorldPos, float fNdotL)
{
	if (g_xPassSet.TranslucencyConstants.g_bShadowsEnabled == 0u) return 1.0;

	const uint uNumCSMs = 4;
	float4x4 axShadowMats[4] = {
		g_xPassSet.ShadowMatrices[0],
		g_xPassSet.ShadowMatrices[1],
		g_xPassSet.ShadowMatrices[2],
		g_xPassSet.ShadowMatrices[3],
	};

	float2 texelSize = g_xPassSet.TranslucencyConstants.g_xCSMTexelSize;
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
	if (g_xPassSet.TranslucencyConstants.g_bIBLEnabled == 0u)
	{
		return float3(g_xPassSet.TranslucencyConstants.g_fAmbientFallbackIntensity).xxx * s.albedo.rgb * s.ao;
	}

	float  NdotV = max(dot(s.worldNormal, xViewDir), PBR_EPSILON);
	float3 F0    = ComputeF0(s.albedo.rgb, s.metallic, s.specular);
	float3 F     = FresnelSchlickRoughness(NdotV, F0, s.roughness);

	float  fRoughnessForLUT = clamp(s.roughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	float2 xBRDF = g_xPassSet.g_xBRDFLUT.Sample(float2(NdotV, fRoughnessForLUT)).rg;

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - s.metallic);

	float3 xDiffuseIBL = float3(0.0, 0.0, 0.0);
	if (g_xPassSet.TranslucencyConstants.g_bIBLDiffuseEnabled != 0u)
	{
		float3 xIrradiance = g_xPassSet.g_xIrradianceMap.Sample(s.worldNormal).rgb;
		xDiffuseIBL        = kD * xIrradiance * s.albedo.rgb;
	}

	float3 xSpecularIBL = float3(0.0, 0.0, 0.0);
	if (g_xPassSet.TranslucencyConstants.g_bIBLSpecularEnabled != 0u)
	{
		float3 xReflect = reflect(-xViewDir, s.worldNormal);
		float  fLOD     = s.roughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xPrefiltered = g_xPassSet.g_xPrefilteredMap.SampleLevel(xReflect, fLOD).rgb;
		xSpecularIBL = xPrefiltered * MultiscatterBRDF(F0, NdotV, fRoughnessForLUT, xBRDF);
	}

	// Specular AO remap (Lagarde/de Rousiers 2014).
	float fSpecularAO = saturate(
		pow(NdotV + s.ao, exp2(-16.0 * s.roughness - 1.0))
		- 1.0 + s.ao);

	float3 xBase = xDiffuseIBL * s.ao + xSpecularIBL * fSpecularAO;

	// Clear-coat IBL lobe.
	if (s.ccStrength > 0.0 && g_xPassSet.TranslucencyConstants.g_bIBLSpecularEnabled != 0u)
	{
		float fCoatFresnel = PBR_CLEARCOAT_F0 + (1.0 - PBR_CLEARCOAT_F0) * pow(saturate(1.0 - NdotV), PBR_FRESNEL_POWER);
		float fCoatRoughForLUT = clamp(s.ccRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
		float2 xCoatBRDF = g_xPassSet.g_xBRDFLUT.Sample(float2(NdotV, fCoatRoughForLUT)).rg;
		float3 xReflect = reflect(-xViewDir, s.worldNormal);
		float  fCoatLOD = s.ccRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xCoatPrefiltered = g_xPassSet.g_xPrefilteredMap.SampleLevel(xReflect, fCoatLOD).rgb;
		float3 xCoatSpecular = xCoatPrefiltered * (PBR_CLEARCOAT_F0 * xCoatBRDF.x + xCoatBRDF.y) * s.ccStrength;
		xBase = xBase * (1.0 - s.ccStrength * fCoatFresnel) + xCoatSpecular * fSpecularAO;
	}

	return xBase * g_xPassSet.TranslucencyConstants.g_fIBLIntensity;
}

// ----- Sun direct lighting (deferred recipe over a MaterialSurface) -----

float3 ComputeSunDirect(MaterialSurface s, float3 xViewDir, float3 xWorldPos)
{
	float3 xLightDir = normalize(-g_xGlobalSet.g_xGlobal.g_xSunDir_Pad.xyz);
	float3 xHalfDir  = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(s.worldNormal, xLightDir), 0.0);
	float NdotV = max(dot(s.worldNormal, xViewDir),  PBR_EPSILON);
	float NdotH = max(dot(s.worldNormal, xHalfDir),  0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	if (NdotL <= PBR_EPSILON) return float3(0.0, 0.0, 0.0);
	if (g_xGlobalSet.g_xGlobal.g_xSunColour_Pad.a <= 0.1) return float3(0.0, 0.0, 0.0);

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

	float3 xRadiance = g_xGlobalSet.g_xGlobal.g_xSunColour_Pad.rgb * g_xGlobalSet.g_xGlobal.g_xSunColour_Pad.a;
	float fShadow = ComputeSunShadowFactor(xWorldPos, NdotL);
	return Lo * xRadiance * NdotL * fShadow;
}

[shader("fragment")]
float4 fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace) : SV_Target
{
	float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3 xViewDir = normalize(g_xViewSet.g_xView.g_xCamPos_Pad.xyz - input.worldPos);

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = g_xDrawSet.g_axMaterials[dc.g_uMaterialIndex];

	// Bindless material textures (draw-uniform index → no NonUniformResourceIndex).
	Sampler2D xBaseColorTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
	Sampler2D xNormalTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.y];
	Sampler2D xRoughnessMetallicTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.z];
	Sampler2D xOcclusionTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.w];
	Sampler2D xEmissiveTex          = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.x];
	Sampler2D xHeightTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.y];
	Sampler2D xDetailAlbedoTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.z];
	Sampler2D xDetailNormalTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.w];
	Sampler2D xDetailMaskTex        = g_xBindlessSet.g_axTextures[m.g_xTexIdx2.x];

	MaterialSurface xSurface = EvaluateMaterialSurface(
		xBaseColorTex, xNormalTex, xRoughnessMetallicTex,
		xOcclusionTex, xEmissiveTex, xHeightTex,
		xDetailAlbedoTex, xDetailNormalTex, xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace, m);

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
	if (g_xPassSet.TranslucencyConstants.g_bDynamicLightsEnabled != 0u)
	{
		float fViewZ = mul(g_xViewSet.g_xView.g_xViewMat, float4(input.worldPos, 1.0)).z;
		float2 xScreenUV = input.position.xy * g_xViewSet.g_xView.g_xRcpScreenDims;
		uint3 xClusterCoord = ClusterCoordFromUVDepth(xScreenUV, fViewZ);
		uint  uClusterIdx   = ClusterIndex(xClusterCoord);
		uint  uLightCount   = g_xPassSet.ClusterLightCounts[uClusterIdx];

		float3 xDynLights = float3(0.0, 0.0, 0.0);
		for (uint i = 0u; i < uLightCount; ++i)
		{
			uint uLightIdx = g_xPassSet.ClusterLightIndices[uClusterIdx * MAX_LIGHTS_PER_CLUSTER + i];
			LightInstance xL = g_xPassSet.LightBuffer[uLightIdx];
			xDynLights += EvaluateLight(xL, input.worldPos, xSurface.worldNormal, xViewDir,
			                            xSurface.albedo.rgb, xSurface.metallic, xSurface.roughness,
			                            xSurface.specular, g_xPassSet.g_xBRDFLUT);
		}
		xColour += xDynLights;
	}

	// Emissive bypasses lighting.
	xColour += xSurface.emissive;

	return float4(xColour, fAlpha);
}
