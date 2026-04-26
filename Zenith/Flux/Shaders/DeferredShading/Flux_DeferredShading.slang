// =====================================================================
// Flux_DeferredShading — Slang module.
//
// Subsystem: DeferredShading. The lighting composite pass. Reads the
// G-buffer + depth + 4 cascaded shadow maps + BRDF LUT + irradiance +
// prefiltered cubemap + SSR + SSGI and outputs final HDR colour.
//
// Inlines what the GLSL split across DeferredShading_Lighting.fxh /
// _Shadows.fxh / _IBL.fxh — those headers were tightly coupled to the
// shader's binding table (they referenced g_xCSM0..3, g_xBRDFLUT etc
// directly), so a Slang port has no separate-module benefit.
//
// Imports Common.Frame, Common.PBR. Common.Volumetric/GBuffer/Atmos
// are NOT needed here — the deferred shader just reads the G-buffer.
// =====================================================================

import Common.Fullscreen;
import Common.Frame;
import Common.PBR;

struct DeferredShadingConstantsLayout
{
	uint  g_bVisualiseCSMs;
	uint  g_bIBLEnabled;
	uint  g_uDebugMode;            // 0=normal, 1=cyan, 2=depth, 3=diffuse
	uint  g_bIBLDiffuseEnabled;
	uint  g_bIBLSpecularEnabled;
	float g_fIBLIntensity;
	uint  g_bShowBRDFLUT;
	uint  g_bForceRoughness;
	float g_fForcedRoughness;
	uint  g_bSSREnabled;
	uint  g_bSSGIEnabled;
	float g_fAmbientFallbackIntensity;
};
[[vk::binding( 1, 0)]] ConstantBuffer<DeferredShadingConstantsLayout> DeferredShadingConstants;

// Shadow matrices — kept as separate CBs to match the legacy binding
// table. The TODO in the GLSL source notes these should fold into one
// buffer; preserving the layout for now so the C++ side doesn't shift.
struct ShadowMatrixLayout { float4x4 m_xMat; };
[[vk::binding( 2, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix0;
[[vk::binding( 3, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix1;
[[vk::binding( 4, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix2;
[[vk::binding( 5, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix3;

[[vk::binding( 6, 0)]] Sampler2D   g_xDiffuseTex;
[[vk::binding( 7, 0)]] Sampler2D   g_xNormalsAmbientTex;
[[vk::binding( 8, 0)]] Sampler2D   g_xMaterialTex;
[[vk::binding( 9, 0)]] Sampler2D   g_xDepthTex;

[[vk::binding(10, 0)]] Sampler2D   g_xCSM0;
[[vk::binding(11, 0)]] Sampler2D   g_xCSM1;
[[vk::binding(12, 0)]] Sampler2D   g_xCSM2;
[[vk::binding(13, 0)]] Sampler2D   g_xCSM3;

[[vk::binding(14, 0)]] Sampler2D   g_xBRDFLUT;
[[vk::binding(15, 0)]] SamplerCube g_xIrradianceMap;
[[vk::binding(16, 0)]] SamplerCube g_xPrefilteredMap;
[[vk::binding(17, 0)]] Sampler2D   g_xSSRTex;
[[vk::binding(18, 0)]] Sampler2D   g_xSSGITex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// ----- Shadow mapping (was DeferredShading_Shadows.fxh) -----

// Tuned for 2048x2048 CSMs at typical outdoor scene scale.
static const float SHADOW_MIN_BIAS         = 0.0005;
static const float SHADOW_MAX_BIAS         = 0.005;
static const float PCF_OFFSET_INNER        = 0.5;
static const float PCF_OFFSET_OUTER        = 1.5;
static const float PCF_SAMPLE_COUNT        = 16.0;
static const float CASCADE_BLEND_DISTANCE  = 0.15;

// PCF16 cascade sample. Four 4-tap textureGather calls (16 comparisons
// total) at offset corners; comparison vs biased depth → average.
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

		// GLSL textureGather(t, uv, 0) → red-channel gather of the 4 surrounding texels.
		// Slang's combined-sampler Gather() defaults to .r.
		if      (iCascade == 0) shadowDepths = g_xCSM0.Gather(xSampleUV);
		else if (iCascade == 1) shadowDepths = g_xCSM1.Gather(xSampleUV);
		else if (iCascade == 2) shadowDepths = g_xCSM2.Gather(xSampleUV);
		else                    shadowDepths = g_xCSM3.Gather(xSampleUV);

		float4 comparison = step(float4(fBiasedDepth, fBiasedDepth, fBiasedDepth, fBiasedDepth), shadowDepths);
		fShadow += dot(comparison, float4(1.0, 1.0, 1.0, 1.0));
	}

	return fShadow / PCF_SAMPLE_COUNT;
}

// ----- IBL composition (was DeferredShading_IBL.fxh) -----

static const float NORMAL_LENGTH_EPSILON   = 0.001;
static const float SSR_CONFIDENCE_EPSILON  = 0.001;

float3 ComputeIBLAmbient(float3 xNormal, float3 xViewDir, float3 xAlbedo,
						   float fMetallic, float fRoughness, float fAmbientOcclusion,
						   float2 xUV)
{
	float  NdotV = max(dot(xNormal, xViewDir), PBR_EPSILON);
	float3 F0    = lerp(PBR_DIELECTRIC_F0, xAlbedo, fMetallic);
	float3 F     = FresnelSchlickRoughness(NdotV, F0, fRoughness);

	float  fRoughnessForLUT = clamp(fRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	float2 xBRDF = g_xBRDFLUT.Sample(float2(NdotV, fRoughnessForLUT)).rg;

	// SSR specular contribution (Fresnel applied here, not in SSR shader,
	// so SSR + IBL share consistent Fresnel and energy is conserved).
	float3 xSSRSpecular   = float3(0.0, 0.0, 0.0);
	float  fSSRConfidence = 0.0;
	if (DeferredShadingConstants.g_bSSREnabled != 0)
	{
		float4 xSSR     = g_xSSRTex.Sample(xUV);
		fSSRConfidence  = xSSR.a;
		float3 xSSRFres = FresnelSchlick(NdotV, F0);
		xSSRSpecular    = xSSR.rgb * xSSRFres;
	}

	// IBL disabled — fallback to simple ambient + SSR specular.
	if (DeferredShadingConstants.g_bIBLEnabled == 0)
	{
		float3 xAmbient   = float3(DeferredShadingConstants.g_fAmbientFallbackIntensity).xxx * xAlbedo * fAmbientOcclusion;
		float3 xSSRFinal  = xSSRSpecular * fSSRConfidence * fAmbientOcclusion;
		return xAmbient + xSSRFinal;
	}

	// Degenerate normal guard — sky / unrendered pixels get zero ambient.
	if (length(xNormal) < NORMAL_LENGTH_EPSILON) return float3(0.0, 0.0, 0.0);

	// Energy-conserving split: kS via Fresnel, kD = 1 - kS, scale kD by
	// (1 - metallic) since metals have no diffuse contribution.
	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - fMetallic);

	float3 xDiffuseIBL = float3(0.0, 0.0, 0.0);
	if (DeferredShadingConstants.g_bIBLDiffuseEnabled != 0)
	{
		float3 xIrradiance = g_xIrradianceMap.Sample(xNormal).rgb;
		xDiffuseIBL        = kD * xIrradiance * xAlbedo;
	}

	// SSGI replaces (not adds to) IBL diffuse where confidence is high.
	float3 xFinalDiffuse = xDiffuseIBL;
	if (DeferredShadingConstants.g_bSSGIEnabled != 0)
	{
		float4 xSSGI           = g_xSSGITex.Sample(xUV);
		float  fSSGIConfidence = xSSGI.a;
		if (fSSGIConfidence > 0.001)
		{
			float3 xSSGIDiffuse = kD * xSSGI.rgb * xAlbedo;
			xFinalDiffuse       = lerp(xDiffuseIBL, xSSGIDiffuse, fSSGIConfidence);
		}
	}

	// Specular IBL via prefiltered cubemap + multiscatter BRDF.
	float3 xSpecularIBL = float3(0.0, 0.0, 0.0);
	if (DeferredShadingConstants.g_bIBLSpecularEnabled != 0)
	{
		float3 xReflect = reflect(-xViewDir, xNormal);
		float  fLOD     = fRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xPrefiltered = g_xPrefilteredMap.SampleLevel(xReflect, fLOD).rgb;
		xSpecularIBL = xPrefiltered * MultiscatterBRDF(F0, NdotV, fRoughness, xBRDF);
	}

	// SSR ↔ IBL specular blend by SSR confidence.
	float3 xFinalSpecular;
	if (DeferredShadingConstants.g_bSSREnabled != 0 && fSSRConfidence > SSR_CONFIDENCE_EPSILON)
		xFinalSpecular = lerp(xSpecularIBL, xSSRSpecular, fSSRConfidence);
	else
		xFinalSpecular = xSpecularIBL;

	// Specular AO remap (Lagarde/de Rousiers 2014) — drops specular at
	// grazing angles in occluded regions.
	float fSpecularAO = saturate(
		pow(NdotV + fAmbientOcclusion, exp2(-16.0 * fRoughness - 1.0))
		- 1.0 + fAmbientOcclusion);

	return (xFinalDiffuse * fAmbientOcclusion + xFinalSpecular * fSpecularAO) *
		   DeferredShadingConstants.g_fIBLIntensity;
}

// ----- Direct lighting (was DeferredShading_Lighting.fxh) -----

static const float DEFERRED_EPSILON       = PBR_EPSILON;
static const float DEFERRED_DENOM_EPSILON = PBR_EPSILON;

void CookTorrance_Directional(inout float4 xFinalColor,
								float4 xDiffuse,
								DirectionalLight xLight,
								float3 xNormal,
								float fMetal,
								float fRough,
								float3 xWorldPos)
{
	float3 xLightDir = normalize(xLight.m_xDirection.xyz);
	float3 xViewDir  = normalize(FrameConstants.g_xCamPos_Pad.xyz - xWorldPos);
	float3 xHalfDir  = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(xNormal, xLightDir), 0.0);
	float NdotV = max(dot(xNormal, xViewDir),  DEFERRED_EPSILON);
	float NdotH = max(dot(xNormal, xHalfDir),  0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	if (NdotL <= DEFERRED_EPSILON) return;

	float roughness = max(fRough, PBR_MIN_ROUGHNESS);

	float3 F0 = lerp(PBR_DIELECTRIC_F0, xDiffuse.rgb, fMetal);

	float  D = DistributionGGX(NdotH, roughness);
	float  G = GeometrySmith_Direct(NdotV, NdotL, roughness);
	float3 F = FresnelSchlick(HdotV, F0);

	float3 numerator   = D * G * F;
	float  denominator = 4.0 * NdotV * NdotL;
	float3 specular    = numerator / max(denominator, DEFERRED_DENOM_EPSILON);

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - fMetal);

	float3 diffuse  = kD * xDiffuse.rgb / PI;
	float3 radiance = xLight.m_xColour.rgb * xLight.m_xColour.a;
	float3 Lo       = (diffuse + specular) * radiance * NdotL;

	xFinalColor.rgb += Lo;
	xFinalColor.a    = 1.0;
}

// ----- Main composite -----

static const float BRDF_LUT_DISPLAY_SIZE   = 0.2;
static const float BRDF_LUT_DISPLAY_MARGIN = 0.02;
static const float LIGHT_INTENSITY_THRESHOLD = 0.1;

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	// BRDF LUT debug overlay (top-right corner).
	if (DeferredShadingConstants.g_bShowBRDFLUT != 0u)
	{
		float fSize    = BRDF_LUT_DISPLAY_SIZE;
		float fMargin  = BRDF_LUT_DISPLAY_MARGIN;
		float fMinX    = 1.0 - fMargin - fSize;
		float fMaxX    = 1.0 - fMargin;
		float fMinY    = 1.0 - fMargin - fSize;
		float fMaxY    = 1.0 - fMargin;

		bool bInRegion = (input.uv.x > fMinX) && (input.uv.x < fMaxX) &&
						 (input.uv.y > fMinY) && (input.uv.y < fMaxY);
		if (bInRegion)
		{
			float2 xLutUV = float2((input.uv.x - fMinX) / fSize,
									(input.uv.y - fMinY) / fSize);
			float2 xBRDF  = g_xBRDFLUT.Sample(xLutUV).rg;
			return float4(xBRDF.r, xBRDF.g, 0.0, 1.0);
		}
	}

	// Debug-mode 1: solid cyan (proves the pass is running).
	if (DeferredShadingConstants.g_uDebugMode == 1u) return float4(0.0, 1.0, 1.0, 1.0);

	float4 xDiffuse = g_xDiffuseTex.Sample(input.uv);
	float  fDepth   = g_xDepthTex.Sample(input.uv).r;

	if (DeferredShadingConstants.g_uDebugMode == 2u)
	{
		if (fDepth >= 1.0) return float4(1.0, 0.0, 0.0, 1.0); // sky → red
		float v = 1.0 - fDepth;
		return float4(v, v, v, 1.0);
	}

	if (DeferredShadingConstants.g_uDebugMode == 3u)
		return float4(xDiffuse.rgb, 1.0);

	// Sky pixels skip lighting and pass through the G-buffer diffuse value
	// (which the skybox pass wrote with emissive luminance encoded in
	// material.b, but this shader doesn't pull that — sky just shows the
	// stored diffuse).
	if (fDepth == 1.0) return xDiffuse;

	float4 xMaterial      = g_xMaterialTex.Sample(input.uv);
	float4 xNormalAmbient = g_xNormalsAmbientTex.Sample(input.uv);
	float3 xNormal        = normalize(xNormalAmbient.xyz);
	float  fAmbient       = xNormalAmbient.w;

	float3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, input.uv);

	float fRoughness = xMaterial.x;
	float fMetallic  = xMaterial.y;
	float fEmissive  = xMaterial.z;
	if (DeferredShadingConstants.g_bForceRoughness != 0u)
		fRoughness = DeferredShadingConstants.g_fForcedRoughness;

	float3 xViewDir = normalize(FrameConstants.g_xCamPos_Pad.xyz - xWorldPos);

	// IBL + SSR + SSGI ambient — produces the base colour before direct light.
	float3 xIBLAmbient = ComputeIBLAmbient(xNormal, xViewDir, xDiffuse.rgb,
											fMetallic, fRoughness, fAmbient, input.uv);
	float4 xColour = float4(xIBLAmbient, 1.0);

	// Cascaded shadow mapping. Walk cascades 0..3, pick the first one whose
	// projected sample lies inside its shadow map, blend with the next
	// cascade in the edge "blend zone" to hide cascade seams.
	DirectionalLight xLight;
	xLight.m_xColour    = FrameConstants.g_xSunColour;
	xLight.m_xDirection = float4(-FrameConstants.g_xSunDir_Pad.xyz, 0.0);

	const uint uNumCSMs = 4;
	float4x4 axShadowMats[4] = {
		ShadowMatrix0.m_xMat,
		ShadowMatrix1.m_xMat,
		ShadowMatrix2.m_xMat,
		ShadowMatrix3.m_xMat,
	};

	float  fShadowFactor = 1.0;
	float3 xLightDir     = normalize(xLight.m_xDirection.xyz);
	float  fNdotL        = max(dot(xNormal, xLightDir), 0.0);

	uint uW, uH;
	g_xCSM0.GetDimensions(uW, uH);
	float2 texelSize = 1.0 / float2((float)uW, (float)uH);

	for (int iCascade = 0; iCascade < (int)uNumCSMs; iCascade++)
	{
		float4 xShadowSpace = mul(axShadowMats[iCascade], float4(xWorldPos, 1.0));
		float2 xSamplePos   = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float  fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

		if (xSamplePos.x < 0.0 || xSamplePos.x > 1.0 ||
			xSamplePos.y < 0.0 || xSamplePos.y > 1.0) continue;
		if (fCurrentDepth < 0.0 || fCurrentDepth > 1.0) continue;

		if (DeferredShadingConstants.g_bVisualiseCSMs > 0u)
		{
			float v = (1.0 / (float)uNumCSMs) * (float)iCascade;
			return float4(v, v, v, v);
		}

		float fBias        = max(SHADOW_MIN_BIAS, SHADOW_MAX_BIAS * (1.0 - fNdotL));
		float fBiasedDepth = fCurrentDepth - fBias;

		// Cascade blend zone — when the sample is near the edge of the
		// current cascade, blend with the next one to hide the seam.
		float2 xEdgeDist     = min(xSamplePos, 1.0 - xSamplePos);
		float  fMinEdgeDist  = min(xEdgeDist.x, xEdgeDist.y);
		bool   bInBlendZone  = (fMinEdgeDist < CASCADE_BLEND_DISTANCE) &&
							   (iCascade < (int)uNumCSMs - 1);

		float fShadow1 = SampleCascadeShadow(iCascade, xSamplePos, fBiasedDepth, texelSize);

		if (bInBlendZone)
		{
			float fBlendWeight       = 1.0 - (fMinEdgeDist / CASCADE_BLEND_DISTANCE);
			float4 xNextShadowSpace  = mul(axShadowMats[iCascade + 1], float4(xWorldPos, 1.0));
			float2 xNextSamplePos    = xNextShadowSpace.xy / xNextShadowSpace.w * 0.5 + 0.5;
			float  fNextDepth        = xNextShadowSpace.z / xNextShadowSpace.w;

			if (xNextSamplePos.x >= 0.0 && xNextSamplePos.x <= 1.0 &&
				xNextSamplePos.y >= 0.0 && xNextSamplePos.y <= 1.0 &&
				fNextDepth >= 0.0 && fNextDepth <= 1.0)
			{
				float fNextBiasedDepth = fNextDepth - fBias;
				float fShadow2 = SampleCascadeShadow(iCascade + 1, xNextSamplePos,
													  fNextBiasedDepth, texelSize);
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

	// Direct light contribution + shadow blend. Shadow attenuates ONLY
	// the direct term — IBL ambient stays unchanged so shadowed pixels
	// don't go fully black.
	if (fNdotL > 0.0)
	{
		if (xLight.m_xColour.a > LIGHT_INTENSITY_THRESHOLD)
			CookTorrance_Directional(xColour, xDiffuse, xLight, xNormal, fMetallic, fRoughness, xWorldPos);
		xColour.rgb = xIBLAmbient + (xColour.rgb - xIBLAmbient) * fShadowFactor;
	}

	// Emissive bypasses lighting and shadows entirely.
	xColour.rgb += float3(fEmissive, fEmissive, fEmissive);
	xColour.a    = 1.0;
	return xColour;
}
