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
import Common.Lighting;
import Common.GBuffer;	// UnpackGBufferFlags + shading-model tags

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
	uint  g_bSSAOEnabled;          // screen-space AO modulates the ambient term only
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

// Clustered dynamic lights — populated each frame by Flux_LightClustering.
// LightBuffer is the full list (point + spot + directional, type-tagged).
// The cluster buffers index into LightBuffer per-pixel.
[[vk::binding(19, 0)]] StructuredBuffer<LightInstance> LightBuffer;
[[vk::binding(20, 0)]] StructuredBuffer<uint>          ClusterLightCounts;
[[vk::binding(21, 0)]] StructuredBuffer<uint>          ClusterLightIndices;

// 4th G-buffer target: HDR emissive RGB + clear-coat roughness in A.
[[vk::binding(22, 0)]] Sampler2D   g_xGBufferEmissiveTex;

// Screen-space ambient occlusion (half-res R8). Applied to the ambient/IBL
// term only — direct sun + dynamic lights are physically unoccluded by SSAO.
[[vk::binding(23, 0)]] Sampler2D   g_xSSAOTex;

// Cascaded-shadow sampling parameters. Mirrors Flux_ShadowCascadeSamplingData +
// Flux_ShadowSamplingConfig on the C++ side. Laid out as pure float4s so the
// HLSL constant-buffer packing is unambiguous and matches the C++ POD byte-for-
// byte (no scalar-straddle padding surprises).
struct ShadowSamplingLayout
{
	float4 g_xCascadeSplitViewDepth; // per-cascade far split, camera view-space linear depth (+Z fwd)
	float4 g_xCascadeWorldPerTexel;  // per-cascade world units per shadow texel
	float4 g_xCascadeDepthRange;     // per-cascade light-space ortho depth range (world units)
	float4 g_xParams0;               // x=resolution y=rcpResolution z=normalOffsetTexels w=(spare; depth bias is fixed-function)
	float4 g_xParams1;               // x=pcfRadiusTexels y=sunAngularRadius z=cascadeBlendFraction w=tapCount
	float4 g_xParams2;               // x=pcssEnabled (0/1) ; yzw spare
};
[[vk::binding(24, 0)]] ConstantBuffer<ShadowSamplingLayout> ShadowSampling;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// ============================ Cascaded shadow mapping ============================
// Method: stabilised bounding-sphere cascades (fit + texel-snapped on the CPU) +
// normal-offset receiver bias + optimised multi-tap PCF over a Vogel disk, with
// an optional PCSS blocker search for contact-hardening soft shadows. All sample
// patterns are deterministic frame-to-frame (this engine has no TAA), so nothing
// crawls or sparkles.

static const float SHADOW_GOLDEN_ANGLE = 2.39996323;   // rad — Vogel disk spiral
static const float SHADOW_MAX_PENUMBRA_TEXELS = 16.0;  // clamp on PCSS softening
static const uint  SHADOW_BLOCKER_TAPS = 16u;

// Per-pixel kernel rotation via interleaved gradient noise (Jimenez). Stable for
// a given pixel/frame, so it decorrelates the disk without introducing temporal
// shimmer the way a time-varying jitter would.
float ShadowKernelRotation(float2 xPixel)
{
	float fIGN = frac(52.9829189 * frac(dot(xPixel, float2(0.06711056, 0.00583715))));
	return fIGN * 6.2831853;
}

float2 VogelDiskSample(uint uIndex, uint uCount, float fRotation)
{
	float fR     = sqrt((float(uIndex) + 0.5) / float(uCount));
	float fTheta = float(uIndex) * SHADOW_GOLDEN_ANGLE + fRotation;
	return float2(fR * cos(fTheta), fR * sin(fTheta));
}

float4 GatherCascadeDepth(int iCascade, float2 xUV)
{
	if      (iCascade == 0) return g_xCSM0.Gather(xUV);
	else if (iCascade == 1) return g_xCSM1.Gather(xUV);
	else if (iCascade == 2) return g_xCSM2.Gather(xUV);
	else                    return g_xCSM3.Gather(xUV);
}

// The per-cascade sun view×proj lives in four separate CBs (matching the legacy
// binding table); Slang can't index a ConstantBuffer by a runtime value, so
// switch explicitly.
float4x4 ShadowMatrixForCascade(int iCascade)
{
	if      (iCascade == 0) return ShadowMatrix0.m_xMat;
	else if (iCascade == 1) return ShadowMatrix1.m_xMat;
	else if (iCascade == 2) return ShadowMatrix2.m_xMat;
	else                    return ShadowMatrix3.m_xMat;
}

// Multi-tap PCF. Each Vogel tap issues a 4-texel Gather and compares each texel
// against the (already biased) receiver depth — comparing raw depths individually
// (never averaging depth before the compare) is what keeps shadow edges correct.
// fRadiusTexels controls penumbra width.
float SampleShadowPCF(int iCascade, float2 xUV, float fCompareDepth, float fRadiusTexels, uint uTaps, float fRotation)
{
	float fRcpRes = ShadowSampling.g_xParams0.y;
	float fSum    = 0.0;
	for (uint i = 0u; i < uTaps; ++i)
	{
		float2 xOffset = VogelDiskSample(i, uTaps, fRotation) * fRadiusTexels * fRcpRes;
		float4 xDepths = GatherCascadeDepth(iCascade, xUV + xOffset);
		float4 xLit    = step(float4(fCompareDepth, fCompareDepth, fCompareDepth, fCompareDepth), xDepths);
		fSum          += dot(xLit, float4(0.25, 0.25, 0.25, 0.25));
	}
	return fSum / float(uTaps);
}

// PCSS blocker search: average depth of occluders nearer than the receiver, over
// a disk scaled by the sun's angular size. Returns false in bFound when the
// receiver is unoccluded (→ skip the expensive filter, surface is fully lit).
float BlockerSearch(int iCascade, float2 xUV, float fReceiverDepth, float fSearchRadiusTexels, float fRotation, out bool bFound)
{
	float fRcpRes     = ShadowSampling.g_xParams0.y;
	float fBlockerSum = 0.0;
	float fCount      = 0.0;
	for (uint i = 0u; i < SHADOW_BLOCKER_TAPS; ++i)
	{
		float2 xOffset = VogelDiskSample(i, SHADOW_BLOCKER_TAPS, fRotation) * fSearchRadiusTexels * fRcpRes;
		float4 xDepths = GatherCascadeDepth(iCascade, xUV + xOffset);
		[unroll]
		for (int k = 0; k < 4; ++k)
		{
			if (xDepths[k] < fReceiverDepth)
			{
				fBlockerSum += xDepths[k];
				fCount      += 1.0;
			}
		}
	}
	bFound = fCount > 0.5;
	return bFound ? (fBlockerSum / fCount) : 0.0;
}

// Full per-cascade shadow evaluation: normal-offset bias → project → bounds test
// → PCSS/PCF. Writes 1=lit … 0=shadowed into fShadow and returns whether the
// receiver projected inside this cascade's map.
bool EvaluateCascadeShadow(int iCascade, float3 xWorldPos, float3 xNormal, float fNdotL, float fRotation, out float fShadow)
{
	fShadow = 1.0;

	float fWorldPerTexel = ShadowSampling.g_xCascadeWorldPerTexel[iCascade];
	float fDepthRange    = ShadowSampling.g_xCascadeDepthRange[iCascade];
	float fNormalOffset  = ShadowSampling.g_xParams0.z;
	float fPCFRadius     = ShadowSampling.g_xParams1.x;
	float fSunAngle      = ShadowSampling.g_xParams1.y;
	// Clamp the tap count to a sane, bounded range — guarantees the PCF loop
	// terminates and never divides by zero even if the CB were ever unpopulated.
	uint  uTaps          = clamp(uint(ShadowSampling.g_xParams1.w), 1u, 32u);
	bool  bPCSS          = ShadowSampling.g_xParams2.x > 0.5;

	// Normal-offset bias: shift the receiver along its normal by a few texels,
	// scaled by sin(angle-to-light) so grazing surfaces (worst acne) get the most
	// push and forward-facing ones get almost none. Per-cascade texel size makes
	// this self-scaling across cascades — no per-cascade tuning needed.
	float fSinTheta   = sqrt(saturate(1.0 - fNdotL * fNdotL));
	float3 xOffsetPos = xWorldPos + xNormal * (fWorldPerTexel * fNormalOffset * (fSinTheta + 0.15));

	float4 xShadowClip = mul(ShadowMatrixForCascade(iCascade), float4(xOffsetPos, 1.0));
	float3 xProj       = xShadowClip.xyz / xShadowClip.w;
	float2 xUV         = xProj.xy * 0.5 + 0.5;
	float  fDepth      = xProj.z;

	if (xUV.x < 0.0 || xUV.x > 1.0 || xUV.y < 0.0 || xUV.y > 1.0) return false;
	if (fDepth < 0.0 || fDepth > 1.0)                              return false;

	// No depth bias is applied here — it is baked into the stored caster depth by
	// the rasterizer's fixed-function slope-scaled depth bias (vkCmdSetDepthBias on
	// the shadow-caster pipelines). The receiver compares its true depth.
	float fCompareDepth = fDepth;

	float fRadiusTexels = fPCFRadius;
	if (bPCSS)
	{
		// Largest plausible penumbra (blocker at the far end of the depth range),
		// in texels — also used as the blocker-search radius.
		float fMaxPenumbra = clamp(fSunAngle * fDepthRange / fWorldPerTexel,
								   fPCFRadius, SHADOW_MAX_PENUMBRA_TEXELS);
		bool  bFound;
		float fAvgBlocker = BlockerSearch(iCascade, xUV, fCompareDepth, fMaxPenumbra, fRotation, bFound);
		if (!bFound) return true; // unoccluded → fully lit

		// Convert NDC blocker→receiver gap to a world distance (ortho depth is
		// linear), then to a penumbra radius via the sun's angular size.
		float fGapWorld     = max(fCompareDepth - fAvgBlocker, 0.0) * fDepthRange;
		float fPenumbraWorld = fGapWorld * fSunAngle;
		fRadiusTexels = clamp(fPenumbraWorld / fWorldPerTexel, fPCFRadius, fMaxPenumbra);
	}

	fShadow = SampleShadowPCF(iCascade, xUV, fCompareDepth, fRadiusTexels, uTaps, fRotation);
	return true;
}

// ----- IBL composition (was DeferredShading_IBL.fxh) -----

static const float NORMAL_LENGTH_EPSILON   = 0.001;
static const float SSR_CONFIDENCE_EPSILON  = 0.001;

float3 ComputeIBLAmbient(float3 xNormal, float3 xViewDir, float3 xAlbedo,
						   float fMetallic, float fRoughness, float fAmbientOcclusion,
						   float2 xUV, float fSpecular, float fClearCoat, float fClearCoatRoughness)
{
	float  NdotV = max(dot(xNormal, xViewDir), PBR_EPSILON);
	float3 F0    = ComputeF0(xAlbedo, fMetallic, fSpecular);
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

	float3 xBase = xFinalDiffuse * fAmbientOcclusion + xFinalSpecular * fSpecularAO;

	// Clear-coat IBL lobe: second prefiltered sample at the coat's own
	// roughness, fixed F0 = 0.04, base attenuated for energy conservation.
	if (fClearCoat > 0.0 && DeferredShadingConstants.g_bIBLSpecularEnabled != 0)
	{
		float fCoatFresnel = PBR_CLEARCOAT_F0 + (1.0 - PBR_CLEARCOAT_F0) * pow(saturate(1.0 - NdotV), PBR_FRESNEL_POWER);
		float fCoatRoughForLUT = clamp(fClearCoatRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
		float2 xCoatBRDF = g_xBRDFLUT.Sample(float2(NdotV, fCoatRoughForLUT)).rg;
		float3 xReflect = reflect(-xViewDir, xNormal);
		float  fCoatLOD = fClearCoatRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		float3 xCoatPrefiltered = g_xPrefilteredMap.SampleLevel(xReflect, fCoatLOD).rgb;
		float3 xCoatSpecular = xCoatPrefiltered * (PBR_CLEARCOAT_F0 * xCoatBRDF.x + xCoatBRDF.y) * fClearCoat;
		xBase = xBase * (1.0 - fClearCoat * fCoatFresnel) + xCoatSpecular * fSpecularAO;
	}

	return xBase * DeferredShadingConstants.g_fIBLIntensity;
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
								float3 xWorldPos,
								float fSpecularParam,
								float fClearCoat,
								float fClearCoatRoughness,
								uint  uShadingModel)
{
	float3 xLightDir = normalize(xLight.m_xDirection.xyz);
	float3 xViewDir  = normalize(FrameConstants.g_xCamPos_Pad.xyz - xWorldPos);
	float3 xHalfDir  = normalize(xLightDir + xViewDir);

	float fRawNdotL = dot(xNormal, xLightDir);
	float NdotL = max(fRawNdotL, 0.0);
	float NdotV = max(dot(xNormal, xViewDir),  DEFERRED_EPSILON);
	float NdotH = max(dot(xNormal, xHalfDir),  0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	if (NdotL <= DEFERRED_EPSILON) return;

	float roughness = max(fRough, PBR_MIN_ROUGHNESS);

	float3 F0 = ComputeF0(xDiffuse.rgb, fMetal, fSpecularParam);

	float  D = DistributionGGX(NdotH, roughness);
	float  G = GeometrySmith_Direct(NdotV, NdotL, roughness);
	float3 F = FresnelSchlick(HdotV, F0);

	float3 numerator   = D * G * F;
	float  denominator = 4.0 * NdotV * NdotL;
	float3 specular    = numerator / max(denominator, DEFERRED_DENOM_EPSILON);

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - fMetal);

	float3 diffuse  = kD * xDiffuse.rgb / PI;
	float3 Lo       = diffuse + specular;

	// Clear-coat second lobe (energy-conserving: base attenuated by the
	// coat's Fresnel-weighted strength).
	if (fClearCoat > 0.0)
	{
		float fBaseAttenuation;
		float3 xCoat = ClearCoatLobe_Direct(NdotH, NdotV, NdotL, HdotV,
											 fClearCoat, fClearCoatRoughness, fBaseAttenuation);
		Lo = Lo * fBaseAttenuation + xCoat;
	}

	float3 radiance = xLight.m_xColour.rgb * xLight.m_xColour.a;
	if (uShadingModel == GBUFFER_SHADING_SUBSURFACE)
	{
		// Cheap subsurface: the diffuse term wraps past the terminator and shifts
		// warm where light grazes (red scatter under thin skin); specular keeps the
		// hard NdotL. No extra G-buffer target. Default-lit path is untouched.
		const float fWrap = 0.5;
		float  fWrapNdotL   = saturate((fRawNdotL + fWrap) / (1.0 + fWrap));
		float3 xScatterTint = lerp(float3(0.80, 0.32, 0.24), float3(1.0, 1.0, 1.0), saturate(fRawNdotL * 1.4 + 0.30));
		xFinalColor.rgb += (diffuse * fWrapNdotL * xScatterTint + specular * NdotL) * radiance;
	}
	else
	{
		xFinalColor.rgb += Lo * radiance * NdotL;
	}
	xFinalColor.a    = 1.0;
}

// ----- Screen-space contact shadows -----
// Short view-space ray-march from the surface toward the sun. Catches small-
// scale occlusion (foot-meets-ground, prop-on-surface, normal-mapped detail)
// that CSM texels (~cm near, ~dm far) cannot resolve -- the "floating object"
// tell. Reuses the already-bound scene depth; min()'d into the CSM factor.
float ScreenSpaceContactShadow(float3 xWorldPos, float3 xLightDir)
{
	// Deliberately SHORT range: a tight feet-meets-ground contact darken, not a
	// full character shadow (CSM does the big shadow). The march is also applied
	// on the character's own surface, so it self-occludes (arm under shoulder);
	// kept short + soft + UN-jittered so that self-occlusion reads as smooth AO
	// rather than the speckly noise a per-pixel jitter produced (no TAA to
	// denoise it). The big silhouette artifact is solved by the short range, so
	// the jitter is no longer needed.
	const int   iSteps   = 10;
	const float fMaxDist = 0.10;   // ~10cm: immediate contact only
	const float fThick   = 0.05;   // thin occluder window (don't grab the whole body)
	const float fBias    = 0.025;  // skip very-close self-occlusion (acne + limb seams)

	float fStepLen = fMaxDist / float(iSteps);
	float3 xStepW  = xLightDir * fStepLen;
	float3 xPosW   = xWorldPos + xLightDir * 0.02; // fixed lift off the surface
	[unroll]
	for (int i = 0; i < iSteps; ++i)
	{
		xPosW += xStepW;
		float3 xViewPos = mul(FrameConstants.g_xViewMat, float4(xPosW, 1.0)).xyz;
		if (xViewPos.z <= 0.0) break;               // behind camera (+Z forward)
		float3 xScreen = ViewToScreen(xViewPos);    // .xy = UV
		float2 xUV = xScreen.xy;
		if (xUV.x < 0.0 || xUV.x > 1.0 || xUV.y < 0.0 || xUV.y > 1.0) break;

		float fSceneViewZ = DepthToViewZ(xUV, g_xDepthTex.Sample(xUV).r);
		float fDelta = xViewPos.z - fSceneViewZ;    // >0: ray sample is behind scene geometry
		if (fDelta > fBias && fDelta < fThick)
			// Soft + distance-faded: nearer contact darker, capped at 55% so it
			// reads as gentle grounding AO, never a hard black silhouette.
			return 1.0 - 0.55 * (1.0 - float(i) / float(iSteps));
	}
	return 1.0;
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
	float4 xEmissiveCC    = g_xGBufferEmissiveTex.Sample(input.uv);
	float3 xNormal        = normalize(xNormalAmbient.xyz);
	float  fAmbient       = xNormalAmbient.w;

	// Fold screen-space AO into the baked ambient term so it darkens ONLY the
	// ambient/IBL contribution (ComputeIBLAmbient below), never direct light.
	// Half-res R8; a bilinear tap is plenty given the blur already smoothed it.
	if (DeferredShadingConstants.g_bSSAOEnabled != 0u)
		fAmbient *= g_xSSAOTex.Sample(input.uv).r;

	float3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, input.uv);

	float fRoughness = xMaterial.x;
	float fMetallic  = xMaterial.y;
	float fSpecular  = xMaterial.z;
	uint  uShadingModel;
	float fClearCoat;
	UnpackGBufferFlags(xMaterial.w, uShadingModel, fClearCoat);
	float3 xEmissive          = xEmissiveCC.rgb;
	float  fClearCoatRoughness = xEmissiveCC.a;
	if (DeferredShadingConstants.g_bForceRoughness != 0u)
		fRoughness = DeferredShadingConstants.g_fForcedRoughness;

	// G-buffer channel debug views (diagnostics): 4=metallic 5=roughness 6=AO
	// 7=world normal 8=N·L(sun) 10=shading-model tag.
	if (DeferredShadingConstants.g_uDebugMode == 4u) return float4(fMetallic.xxx, 1.0);
	if (DeferredShadingConstants.g_uDebugMode == 5u) return float4(fRoughness.xxx, 1.0);
	if (DeferredShadingConstants.g_uDebugMode == 6u) return float4(fAmbient.xxx, 1.0);
	if (DeferredShadingConstants.g_uDebugMode == 7u) return float4(xNormal * 0.5 + 0.5, 1.0);
	if (DeferredShadingConstants.g_uDebugMode == 8u)
	{
		float fNdotLDbg = max(dot(xNormal, normalize(-FrameConstants.g_xSunDir_Pad.xyz)), 0.0);
		return float4(fNdotLDbg.xxx, 1.0);
	}
	if (DeferredShadingConstants.g_uDebugMode == 10u)
	{
		// red=default-lit, green=unlit, blue=subsurface
		float3 xTag = (uShadingModel == GBUFFER_SHADING_UNLIT) ? float3(0,1,0)
					: (uShadingModel == GBUFFER_SHADING_SUBSURFACE) ? float3(0,0,1) : float3(1,0,0);
		return float4(xTag, 1.0);
	}

	// Unlit surfaces bypass all lighting: base colour + emissive, done.
	if (uShadingModel == GBUFFER_SHADING_UNLIT)
		return float4(xDiffuse.rgb + xEmissive, 1.0);

	float3 xViewDir = normalize(FrameConstants.g_xCamPos_Pad.xyz - xWorldPos);

	// IBL + SSR + SSGI ambient — produces the base colour before direct light.
	float3 xIBLAmbient = ComputeIBLAmbient(xNormal, xViewDir, xDiffuse.rgb,
											fMetallic, fRoughness, fAmbient, input.uv,
											fSpecular, fClearCoat, fClearCoatRoughness);
	float4 xColour = float4(xIBLAmbient, 1.0);

	// Cascaded shadow mapping. Cascade is selected by view-space depth against the
	// CPU-computed split distances (stable: selection doesn't depend on the
	// jittery projection bounds), then sampled with normal-offset bias + PCSS/PCF,
	// and cross-faded into the next cascade across a band to hide the seam.
	DirectionalLight xLight;
	xLight.m_xColour    = FrameConstants.g_xSunColour;
	xLight.m_xDirection = float4(-FrameConstants.g_xSunDir_Pad.xyz, 0.0);

	const int  iNumCSMs    = 4;
	float3     xLightDir   = normalize(xLight.m_xDirection.xyz);
	float      fNdotL      = max(dot(xNormal, xLightDir), 0.0);
	float      fShadowFactor = 1.0;

	// View-space linear depth (+Z forward) drives cascade selection.
	float fViewZ = DepthToViewZ(input.uv, fDepth);

	int iCascade = iNumCSMs - 1;
	[unroll]
	for (int c = 0; c < iNumCSMs; ++c)
	{
		if (fViewZ <= ShadowSampling.g_xCascadeSplitViewDepth[c]) { iCascade = c; break; }
	}

	if (DeferredShadingConstants.g_bVisualiseCSMs > 0u)
	{
		float v = (1.0 / float(iNumCSMs)) * float(iCascade);
		return float4(v, v, v, 1.0);
	}

	// Only resolve the sun shadow on surfaces that actually face the sun; a
	// surface with NdotL<=0 is self-shadowed (handled by the NdotL term below).
	if (fNdotL > 0.0)
	{
		float fRotation = ShadowKernelRotation(input.uv * float2(FrameConstants.g_xScreenDims));

		float fShadow;
		bool  bInside = EvaluateCascadeShadow(iCascade, xWorldPos, xNormal, fNdotL, fRotation, fShadow);
		// Sphere-fit cascades cover the whole slice, but normal-offset can push a
		// grazing receiver just past the edge — fall back to the coarser cascade.
		if (!bInside && iCascade + 1 < iNumCSMs)
			bInside = EvaluateCascadeShadow(iCascade + 1, xWorldPos, xNormal, fNdotL, fRotation, fShadow);

		fShadowFactor = bInside ? fShadow : 1.0;

		// Cross-fade into the next cascade over a band near this cascade's far
		// split so the resolution step is invisible.
		if (bInside && iCascade + 1 < iNumCSMs)
		{
			float fSplit = ShadowSampling.g_xCascadeSplitViewDepth[iCascade];
			float fBand  = fSplit * ShadowSampling.g_xParams1.z;
			float fStart = fSplit - fBand;
			if (fViewZ > fStart && fBand > 1e-4)
			{
				float fW = saturate((fViewZ - fStart) / fBand);
				float fNextShadow;
				if (EvaluateCascadeShadow(iCascade + 1, xWorldPos, xNormal, fNdotL, fRotation, fNextShadow))
					fShadowFactor = lerp(fShadowFactor, fNextShadow, fW);
			}
		}
	}

	// Screen-space contact shadows refine the CSM result for small-scale
	// occlusion. Only meaningful on lit surfaces; take the darker of the two.
	if (fNdotL > 0.0)
		fShadowFactor = min(fShadowFactor, ScreenSpaceContactShadow(xWorldPos, xLightDir));

	// Debug: 9 = sun shadow factor (white=lit, black=shadowed); 11 = IBL ambient only.
	if (DeferredShadingConstants.g_uDebugMode == 9u)  return float4(fShadowFactor.xxx, 1.0);
	if (DeferredShadingConstants.g_uDebugMode == 11u) return float4(xIBLAmbient, 1.0);

	// Direct light contribution + shadow blend. Shadow attenuates ONLY
	// the direct term — IBL ambient stays unchanged so shadowed pixels
	// don't go fully black.
	if (fNdotL > 0.0)
	{
		if (xLight.m_xColour.a > LIGHT_INTENSITY_THRESHOLD)
			CookTorrance_Directional(xColour, xDiffuse, xLight, xNormal, fMetallic, fRoughness, xWorldPos,
									  fSpecular, fClearCoat, fClearCoatRoughness, uShadingModel);
		xColour.rgb = xIBLAmbient + (xColour.rgb - xIBLAmbient) * fShadowFactor;
	}

	// Clustered dynamic lights — point, spot, and any non-sun
	// directional lights. Inserted AFTER the sun-shadow resolution
	// (so CSM shadows don't accidentally attenuate dynamic lights, which
	// have nothing to do with the sun shadow map) and BEFORE emissive
	// (which is unlit by design).
	{
		float fViewZ = DepthToViewZ(input.uv, fDepth);
		uint3 xClusterCoord = ClusterCoordFromUVDepth(input.uv, fViewZ);
		uint  uClusterIdx   = ClusterIndex(xClusterCoord);
		uint  uLightCount   = ClusterLightCounts[uClusterIdx];

		float3 xDynLights = float3(0.0, 0.0, 0.0);
		for (uint i = 0u; i < uLightCount; ++i)
		{
			uint uLightIdx = ClusterLightIndices[uClusterIdx * MAX_LIGHTS_PER_CLUSTER + i];
			LightInstance xL = LightBuffer[uLightIdx];
			xDynLights += EvaluateLight(xL, xWorldPos, xNormal, xViewDir,
			                            xDiffuse.rgb, fMetallic, fRoughness, fSpecular,
			                            g_xBRDFLUT);
		}
		xColour.rgb += xDynLights;
	}

	// Emissive bypasses lighting and shadows entirely — full HDR colour
	// (values above 1 survive into the HDR scene and drive bloom).
	xColour.rgb += xEmissive;
	xColour.a    = 1.0;
	return xColour;
}
