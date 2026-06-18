// =====================================================================
// Common.MaterialSurface — shared Slang module.
//
// THE single material evaluation path for the parameter-based uber-
// material: every mesh shader (G-buffer writers, the forward translucent
// pass, the editor material preview) calls EvaluateMaterialSurface and
// either packs the result into the G-buffer (MakeGBuffer overload below)
// or lights it directly (forward).
//
// Feature gating is uniform-branch based (GetMaterialFlags from
// Common.DrawConstants): textures are always bound (1x1 engine defaults
// when unset), so disabled features cost one uniform branch.
//
// This function boundary is the future node-graph codegen target: a
// graph-authored material would replace the body of this evaluation
// while the surrounding pipeline (G-buffer packing, lighting) stays.
// =====================================================================

import Common.DrawConstants;
import Common.Material;
import Common.GBuffer;

public struct MaterialSurface
{
	public float4 albedo;        // rgb + alpha (post-cutoff)
	public float3 worldNormal;
	public float  roughness;
	public float  metallic;
	public float  specular;      // F0 scale (UE convention: F0 = 0.08 * specular)
	public float  ao;
	public float3 emissive;      // HDR
	public float  ccStrength;    // clear coat
	public float  ccRoughness;
	public uint   uShadingModel; // GBUFFER_SHADING_*
};

// Parallax occlusion mapping: standard layer march with view-angle-adaptive
// step count and a final secant interpolation between the last two samples.
// Heights are sampled with SampleLevel(0) — implicit derivatives are not
// defined inside the non-uniform march loop.
public float2 ParallaxOcclusionUV(Sampler2D xHeightTex, float2 xUV, float3 xViewTS,
									float fHeightScale, float fMinSteps, float fMaxSteps)
{
	// More steps at grazing angles, fewer head-on.
	float fNumLayers = lerp(fMaxSteps, fMinSteps, saturate(abs(xViewTS.z)));
	float fLayerDepth = 1.0 / fNumLayers;

	// Total UV shift across the full height range. Clamp z to avoid the
	// classic grazing-angle smear blow-up.
	float2 xTotalShift = (xViewTS.xy / max(abs(xViewTS.z), 0.08)) * fHeightScale;
	float2 xDeltaUV = xTotalShift / fNumLayers;

	float2 xCurUV = xUV;
	float fCurLayerDepth = 0.0;
	// Height map convention: 1 = surface top. March depth = 1 - height.
	float fCurDepth = 1.0 - xHeightTex.SampleLevel(xCurUV, 0).r;

	// Hard cap keeps the loop bounded whatever the CB says.
	[loop]
	for (int i = 0; i < 64; ++i)
	{
		if (fCurLayerDepth >= fCurDepth) break;
		xCurUV -= xDeltaUV;
		fCurDepth = 1.0 - xHeightTex.SampleLevel(xCurUV, 0).r;
		fCurLayerDepth += fLayerDepth;
	}

	// Interpolate between the last two layers for a smooth intersection.
	float2 xPrevUV = xCurUV + xDeltaUV;
	float fAfter = fCurDepth - fCurLayerDepth;
	float fBefore = (1.0 - xHeightTex.SampleLevel(xPrevUV, 0).r) - (fCurLayerDepth - fLayerDepth);
	float fWeight = fAfter / min(fAfter - fBefore, -1e-5);
	return lerp(xCurUV, xPrevUV, saturate(fWeight));
}

// The uber-material evaluation. xTBN columns are (T, B, N) in world space
// (BuildTBN from Common.Material); xWorldViewDir points from the surface
// toward the camera; vertex colour modulates only when its alpha > 0
// (legacy engine convention).
//
// May DISCARD (alpha cutoff > 0 = Masked blend mode; the C++ side writes
// cutoff 0 for Opaque/Translucent so they never discard).
public MaterialSurface EvaluateMaterialSurface(
	Sampler2D xBaseColorTex, Sampler2D xNormalTex, Sampler2D xRoughnessMetallicTex,
	Sampler2D xOcclusionTex, Sampler2D xEmissiveTex, Sampler2D xHeightTex,
	Sampler2D xDetailAlbedoTex, Sampler2D xDetailNormalTex, Sampler2D xDetailMaskTex,
	float2 xUVIn, float3x3 xTBN, float3 xWorldViewDir, float4 xVertexColor, bool bFrontFace)
{
	const uint uFlags = GetMaterialFlags();

	float2 xUV = TransformUV(xUVIn, GetUVTiling(), GetUVOffset());

	// --- Parallax occlusion mapping (shifts all subsequent samples) ---
	if ((uFlags & MATERIAL_FLAG_POM) != 0u)
	{
		// World -> tangent: TBN columns are the tangent basis, so the
		// transpose (here: mul(v, M)) projects a world vector onto it.
		float3 xViewTS = mul(xWorldViewDir, xTBN);
		xUV = ParallaxOcclusionUV(xHeightTex, xUV, xViewTS,
								   GetHeightScale(), GetPOMMinSteps(), GetPOMMaxSteps());
	}

	// --- Base colour + vertex tint + alpha test ---
	float4 xAlbedo = SampleDiffuseWithBaseColor(xBaseColorTex, xUV, DrawConstants.g_xBaseColor);
	if (xVertexColor.a > 0.0) xAlbedo *= xVertexColor;

	float fAlphaCutoff = GetAlphaCutoff();
	if (fAlphaCutoff > 0.0 && xAlbedo.a < fAlphaCutoff) discard;

	// --- Normal (strength-scaled, optionally detail-blended) ---
	// The primary normal slot is BC5 (R+G only); reconstruct Z = sqrt(1 - x^2 - y^2).
	// Correct for the RGBA8 default-normal too (its stored B is ignored/rebuilt).
	float2 xNormalXY = xNormalTex.Sample(xUV).xy * 2.0 - 1.0;
	float3 xNormalTS = float3(xNormalXY, sqrt(saturate(1.0 - dot(xNormalXY, xNormalXY))));
	xNormalTS.xy *= GetNormalStrength();

	if ((uFlags & MATERIAL_FLAG_DETAIL_MAPS) != 0u)
	{
		float2 xDetailUV = xUV * GetDetailTiling();
		float fDetailMask = xDetailMaskTex.Sample(xUV).r;

		// UDN-blend the detail normal on top of the base normal. Only X/Y are used
		// (Z stays the base normal's), so sample R/G directly — format-agnostic.
		float2 xDetailXY = xDetailNormalTex.Sample(xDetailUV).xy * 2.0 - 1.0;
		xDetailXY *= GetDetailNormalStrength() * fDetailMask;
		xNormalTS = float3(xNormalTS.xy + xDetailXY, xNormalTS.z);

		// x2 multiply detail albedo around mid-grey (Unity convention).
		float3 xDetailAlbedo = xDetailAlbedoTex.Sample(xDetailUV).rgb;
		float3 xOverlaid = xAlbedo.rgb * xDetailAlbedo * 2.0;
		xAlbedo.rgb = lerp(xAlbedo.rgb, xOverlaid, GetDetailAlbedoStrength() * fDetailMask);
	}

	float3 xWorldNormal = normalize(mul(xTBN, normalize(xNormalTS)));

	// Two-sided: light back faces correctly by flipping the shading normal.
	if ((uFlags & MATERIAL_FLAG_TWO_SIDED_NORMAL_FLIP) != 0u && !bFrontFace)
	{
		xWorldNormal = -xWorldNormal;
	}

	// --- Scalar channels ---
	float fRoughness, fMetallic;
	SampleRoughnessMetallic(xRoughnessMetallicTex, xUV,
							 GetRoughnessMultiplier(), GetMetallicMultiplier(),
							 fRoughness, fMetallic);

	// --- Geometric specular anti-aliasing (Kaplanyan/Tokuyoshi) ---
	// Sub-pixel normal variation from 2K + detail normal maps produces specular
	// sparkle / crawling highlights that edge-AA can't fix. Widen the GGX alpha
	// (roughness^2) by the screen-space normal-derivative variance so glints
	// average into the rough lobe. Applied once at the G-buffer write, on the
	// FINAL shading normal (not inside the POM march). Filament's defaults.
	{
		float3 dNdx = ddx(xWorldNormal);
		float3 dNdy = ddy(xWorldNormal);
		float  fVariance     = 0.25 * (dot(dNdx, dNdx) + dot(dNdy, dNdy));
		float  fKernelRough2 = min(2.0 * fVariance, 0.18);
		fRoughness = sqrt(saturate(fRoughness * fRoughness + fKernelRough2));
	}

	MaterialSurface s;
	s.albedo        = xAlbedo;
	s.worldNormal   = xWorldNormal;
	s.roughness     = fRoughness;
	s.metallic      = fMetallic;
	// Skin carries a per-texel specular map in RM.R (wet lips/eyes glossy, matte
	// cheeks/cloth). Other materials leave RM.R unused, so gate on the skin flag
	// and pass 1.0 through for them → uniform GetSpecular() unchanged (regression-safe).
	float fSpecTexel = ((uFlags & MATERIAL_FLAG_SKIN) != 0u) ? xRoughnessMetallicTex.Sample(xUV).r : 1.0;
	s.specular      = fSpecTexel * GetSpecular();
	s.ao            = SampleOcclusion(xOcclusionTex, xUV, GetOcclusionStrength());
	s.emissive      = SampleEmissiveRGB(xEmissiveTex, xUV, GetEmissiveColor(), GetEmissiveIntensity());
	s.ccStrength    = ((uFlags & MATERIAL_FLAG_CLEARCOAT) != 0u) ? GetClearCoatStrength() : 0.0;
	s.ccRoughness   = GetClearCoatRoughness();
	s.uShadingModel = ((uFlags & MATERIAL_FLAG_UNLIT) != 0u) ? GBUFFER_SHADING_UNLIT
					: ((uFlags & MATERIAL_FLAG_SKIN) != 0u)  ? GBUFFER_SHADING_SUBSURFACE
					: GBUFFER_SHADING_DEFAULT_LIT;
	return s;
}

// G-buffer packing for the uber-material path.
public GBufferOut MakeGBuffer(MaterialSurface s)
{
	return MakeGBuffer(s.albedo, s.worldNormal, s.ao, s.roughness, s.metallic,
						s.specular, s.emissive, s.ccStrength, s.ccRoughness, s.uShadingModel);
}
