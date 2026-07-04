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
#include "Common/ViewModes.slang"      // Stage 3b: FLUX_SC_VIEW_*_PERMITTED spec-constant view-mode mask
#include "Common/ShadowSampling.slang" // Stage 6: IShadowFilter seam (this pass conforms its own 4-tap tier below)
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
	// (the shadow/dynamic-light gates moved to the per-view g_xView.g_uViewFlags
	//  — each render view decides; pass constants carry only pass-local state)
	float  g_fAmbientFallbackIntensity;
	float2 g_xCSMTexelSize;	// 1 / CSM resolution (engine rule: dims via CBV)
};

struct PassParams
{
	ConstantBuffer<TranslucencyConstantsLayout> TranslucencyConstants;

	// (g_xShadowMatrices — all 4 cascade sun view×proj — is in the persistent VIEW set, Phase 5.4)

	// (IBL trio — g_xBRDFLUT + g_xIrradianceMap + g_xPrefilteredMap — is in the persistent VIEW set now, Phase 5.4)

	// (Clustered dynamic lights — g_xLightBuffer + g_xClusterLightCounts/Indices — are in the persistent VIEW set now, Phase 5.4.)
};
ParameterBlock<PassParams> g_xPassSet;

// ----- Per-draw constants + material table (set 4). Material textures are bindless (set 2). -----

struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
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
	o.position       = mul(GetViewProjMat(), float4(xWorldPos, 1.0));
	return o;
}

// ----- Shadow sampling (deferred recipe, texel size from CB) -----
// Stage 6: the 4-tap PCF tier is a subsystem-LOCAL IShadowFilter — declared and
// conformed to the Common seam (Common/ShadowSampling.slang) via `extension` right
// here, with ZERO edits to Common. This is the living proof that a subsystem plugs
// in its own filter through the seam. The cascade walk (loop + edge-band blend) in
// ComputeSunShadowFactor is this pass's own and stays put; it drives the 4-tap
// sample directly (the generic EvaluateCascade conformance is provided for any
// consumer wanting the IShadowFilter shape).

static const float SHADOW_MIN_BIAS        = 0.0005;
static const float SHADOW_MAX_BIAS        = 0.005;
static const float PCF_OFFSET_INNER       = 0.5;
static const float PCF_OFFSET_OUTER       = 1.5;
static const float PCF_SAMPLE_COUNT       = 16.0;
static const float CASCADE_BLEND_DISTANCE = 0.15;

// The translucent pass's cheaper shadow tier: a fixed 4-tap (2×2 Gather) box PCF.
// Holds only the CSM texel size (from TranslucencyConstants). SampleCascadeShadow is
// a VERBATIM move of the former free function — texelSize is the one former parameter
// that became a field; the sampling math is unchanged.
struct ShadowFilterSimple4Tap
{
	float2 texelSize;   // 1 / CSM resolution (engine rule: dims via CBV)

	float SampleCascadeShadow(int iCascade, float2 xSamplePos, float fBiasedDepth)
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
			float4 shadowDepths = GetCSM().Gather(float3(xSampleUV, float(iCascade)));

			float4 comparison = step(float4(fBiasedDepth, fBiasedDepth, fBiasedDepth, fBiasedDepth), shadowDepths);
			fShadow += dot(comparison, float4(1.0, 1.0, 1.0, 1.0));
		}

		return fShadow / PCF_SAMPLE_COUNT;
	}
};

// Conform the local tier to the Common shadow-filter seam WITHOUT touching Common
// (the mirror of DeferredShading's ShadowFilterPCSS). EvaluateCascade is the full
// per-cascade unit — project + bounds + N·L bias + 4-tap — so this type satisfies the
// same IShadowFilter contract as the deferred tier. (This pass's own walk below calls
// SampleCascadeShadow directly because it also needs the projected sample position for
// its edge-band blend; the conformance exists to demonstrate the seam.)
extension ShadowFilterSimple4Tap : IShadowFilter
{
	bool EvaluateCascade(int iCascade, float3 xWorldPos, float3 xNormal, float fNdotL, out float fShadow)
	{
		fShadow = 1.0;

		float4 xShadowSpace  = mul(GetShadowMatrix(iCascade), float4(xWorldPos, 1.0));
		float2 xSamplePos    = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float  fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

		if (xSamplePos.x < 0.0 || xSamplePos.x > 1.0 ||
			xSamplePos.y < 0.0 || xSamplePos.y > 1.0) return false;
		if (fCurrentDepth < 0.0 || fCurrentDepth > 1.0) return false;

		float fBias        = max(SHADOW_MIN_BIAS, SHADOW_MAX_BIAS * (1.0 - fNdotL));
		float fBiasedDepth  = fCurrentDepth - fBias;
		fShadow = this.SampleCascadeShadow(iCascade, xSamplePos, fBiasedDepth);
		return true;
	}
}

float ComputeSunShadowFactor(float3 xWorldPos, float fNdotL)
{
	// Stage 3b: fully lit (no shadow) when shadows are not PERMITTED for this view
	// mode OR not runtime-enabled. In BASIC mode the spec constant is FALSE, so
	// `!false || ...` folds to true → the whole shadow evaluation below is stripped.
	if (!FLUX_SC_VIEW_SHADOWS_PERMITTED || (GetViewFlags() & FLUX_VIEW_FLAG_SHADOWS_ENABLED) == 0u) return 1.0;

	const uint uNumCSMs = 4;
	float4x4 axShadowMats[4] = {
		GetShadowMatrix(0),
		GetShadowMatrix(1),
		GetShadowMatrix(2),
		GetShadowMatrix(3),
	};

	float2 texelSize = g_xPassSet.TranslucencyConstants.g_xCSMTexelSize;
	float fShadowFactor = 1.0;

	// Stage 6: the 4-tap sample now comes from this pass's own ShadowFilterSimple4Tap
	// (the CSM texel size is its only state). The projection / bounds / bias / edge-band
	// blend walk is unchanged.
	ShadowFilterSimple4Tap xFilter;
	xFilter.texelSize = texelSize;

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

		float fShadow1 = xFilter.SampleCascadeShadow(iCascade, xSamplePos, fBiasedDepth);

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
				float fShadow2 = xFilter.SampleCascadeShadow(iCascade + 1, xNextSamplePos, fNextBiasedDepth);
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
	float2 xBRDF = GetBRDFLUT().Sample(float2(NdotV, fRoughnessForLUT)).rg;

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - s.metallic);

	float3 xDiffuseIBL = float3(0.0, 0.0, 0.0);
	if (g_xPassSet.TranslucencyConstants.g_bIBLDiffuseEnabled != 0u)
	{
		float3 xIrradiance = GetIrradianceMap().Sample(s.worldNormal).rgb;
		xDiffuseIBL        = kD * xIrradiance * s.albedo.rgb;
	}

	float3 xSpecularIBL = float3(0.0, 0.0, 0.0);
	if (g_xPassSet.TranslucencyConstants.g_bIBLSpecularEnabled != 0u)
	{
		float3 xReflect = reflect(-xViewDir, s.worldNormal);
		float  fLOD     = s.roughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xPrefiltered = GetPrefilteredMap().SampleLevel(xReflect, fLOD).rgb;
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
		float2 xCoatBRDF = GetBRDFLUT().Sample(float2(NdotV, fCoatRoughForLUT)).rg;
		float3 xReflect = reflect(-xViewDir, s.worldNormal);
		float  fCoatLOD = s.ccRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xCoatPrefiltered = GetPrefilteredMap().SampleLevel(xReflect, fCoatLOD).rgb;
		float3 xCoatSpecular = xCoatPrefiltered * (PBR_CLEARCOAT_F0 * xCoatBRDF.x + xCoatBRDF.y) * s.ccStrength;
		xBase = xBase * (1.0 - s.ccStrength * fCoatFresnel) + xCoatSpecular * fSpecularAO;
	}

	return xBase * g_xPassSet.TranslucencyConstants.g_fIBLIntensity;
}

// ----- Sun direct lighting (deferred recipe over a MaterialSurface) -----

float3 ComputeSunDirect(MaterialSurface s, float3 xViewDir, float3 xWorldPos)
{
	float3 xLightDir = normalize(-GetSunDir_Pad().xyz);
	float3 xHalfDir  = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(s.worldNormal, xLightDir), 0.0);
	float NdotV = max(dot(s.worldNormal, xViewDir),  PBR_EPSILON);
	float NdotH = max(dot(s.worldNormal, xHalfDir),  0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	if (NdotL <= PBR_EPSILON) return float3(0.0, 0.0, 0.0);
	if (GetSunColour_Pad().a <= 0.1) return float3(0.0, 0.0, 0.0);

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

	float3 xRadiance = GetSunColour_Pad().rgb * GetSunColour_Pad().a;
	float fShadow = ComputeSunShadowFactor(xWorldPos, NdotL);
	return Lo * xRadiance * NdotL * fShadow;
}

[shader("fragment")]
float4 fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace) : SV_Target
{
	float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3 xViewDir = normalize(GetCamPos_Pad().xyz - input.worldPos);

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = GetMaterial(dc.g_uMaterialIndex);

	// Bindless material textures (draw-uniform index → no NonUniformResourceIndex).
	Sampler2D xBaseColorTex         = GetBindlessTexture(m.g_xTexIdx0.x);
	Sampler2D xNormalTex            = GetBindlessTexture(m.g_xTexIdx0.y);
	Sampler2D xRoughnessMetallicTex = GetBindlessTexture(m.g_xTexIdx0.z);
	Sampler2D xOcclusionTex         = GetBindlessTexture(m.g_xTexIdx0.w);
	Sampler2D xEmissiveTex          = GetBindlessTexture(m.g_xTexIdx1.x);
	Sampler2D xHeightTex            = GetBindlessTexture(m.g_xTexIdx1.y);
	Sampler2D xDetailAlbedoTex      = GetBindlessTexture(m.g_xTexIdx1.z);
	Sampler2D xDetailNormalTex      = GetBindlessTexture(m.g_xTexIdx1.w);
	Sampler2D xDetailMaskTex        = GetBindlessTexture(m.g_xTexIdx2.x);

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
	// Stage 3b: ANDed under FLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED — BASIC mode bakes
	// it FALSE, stripping this cluster-light block.
	if (FLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED && ((GetViewFlags() & FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED) != 0u))
	{
		float fViewZ = mul(GetViewMat(), float4(input.worldPos, 1.0)).z;
		float2 xScreenUV = input.position.xy * GetRcpScreenDims();
		uint3 xClusterCoord = ClusterCoordFromUVDepth(xScreenUV, fViewZ);
		uint  uClusterIdx   = ClusterIndex(xClusterCoord);
		uint  uLightCount   = GetClusterLightCount(uClusterIdx);

		float3 xDynLights = float3(0.0, 0.0, 0.0);
		for (uint i = 0u; i < uLightCount; ++i)
		{
			uint uLightIdx = GetClusterLightIndex(uClusterIdx * MAX_LIGHTS_PER_CLUSTER + i);
			LightInstance xL = GetLight(uLightIdx);
			xDynLights += EvaluateLight(xL, input.worldPos, xSurface.worldNormal, xViewDir,
			                            xSurface.albedo.rgb, xSurface.metallic, xSurface.roughness,
			                            xSurface.specular, GetBRDFLUT());
		}
		xColour += xDynLights;
	}

	// Emissive bypasses lighting.
	xColour += xSurface.emissive;

	return float4(xColour, fAlpha);
}
