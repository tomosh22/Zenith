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

import Common.DrawConstants;   // MATERIAL_FLAG_* bit constants
import Common.MaterialTable;   // MaterialGPU + material-param accessors
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

// Single authoritative masked-alpha cutout — the ONE place the engine samples base
// colour and applies the Masked-mode discard, so the G-buffer mesh path and the shadow
// path cut identically. Always samples; multiplies the per-draw base-colour tint and
// (legacy convention) vertex colour when its alpha > 0; discards when the resulting alpha
// is below the cutoff. EvaluateMaterialSurface calls this unconditionally for its own
// transformed xUV (the G-buffer always needs base colour); the shadow fragment shaders
// wrap the call in their own GetAlphaCutoff(m) > 0 guard so opaque casters never sample
// and keep their depth-only cost. May DISCARD (cutoff > 0 = Masked). The per-material
// GPU record (MaterialGPU) is passed in — the entry shader loads it from g_axMaterials.
public float4 SampleAlbedoWithCutout(Sampler2D xBaseColorTex, float2 xUV, float4 xVertexColor, MaterialGPU m)
{
	float4 xAlbedo = SampleDiffuseWithBaseColor(xBaseColorTex, xUV, m.g_xBaseColor);
	if (xVertexColor.a > 0.0) xAlbedo *= xVertexColor;
	float fAlphaCutoff = GetAlphaCutoff(m);
	if (fAlphaCutoff > 0.0 && xAlbedo.a < fAlphaCutoff) discard;
	return xAlbedo;
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
	float2 xUVIn, float3x3 xTBN, float3 xWorldViewDir, float4 xVertexColor, bool bFrontFace,
	MaterialGPU m)
{
	const uint uFlags = GetMaterialFlags(m);

	float2 xUV = TransformUV(xUVIn, GetUVTiling(m), GetUVOffset(m));

	// --- Parallax occlusion mapping (shifts all subsequent samples) ---
	if ((uFlags & MATERIAL_FLAG_POM) != 0u)
	{
		// World -> tangent: TBN columns are the tangent basis, so the
		// transpose (here: mul(v, M)) projects a world vector onto it.
		float3 xViewTS = mul(xWorldViewDir, xTBN);
		xUV = ParallaxOcclusionUV(xHeightTex, xUV, xViewTS,
								   GetHeightScale(m), GetPOMMinSteps(m), GetPOMMaxSteps(m));
	}

	// --- Base colour + vertex tint + alpha test (shared with the shadow path) ---
	float4 xAlbedo = SampleAlbedoWithCutout(xBaseColorTex, xUV, xVertexColor, m);

	// --- Normal (strength-scaled, optionally detail-blended) ---
	// The primary normal slot is BC5 (R+G only); reconstruct Z = sqrt(1 - x^2 - y^2).
	// Correct for the RGBA8 default-normal too (its stored B is ignored/rebuilt).
	float2 xNormalXY = xNormalTex.Sample(xUV).xy * 2.0 - 1.0;
	float3 xNormalTS = float3(xNormalXY, sqrt(saturate(1.0 - dot(xNormalXY, xNormalXY))));
	xNormalTS.xy *= GetNormalStrength(m);

	if ((uFlags & MATERIAL_FLAG_DETAIL_MAPS) != 0u)
	{
		float2 xDetailUV = xUV * GetDetailTiling(m);
		float fDetailMask = xDetailMaskTex.Sample(xUV).r;

		// UDN-blend the detail normal on top of the base normal. Only X/Y are used
		// (Z stays the base normal's), so sample R/G directly — format-agnostic.
		float2 xDetailXY = xDetailNormalTex.Sample(xDetailUV).xy * 2.0 - 1.0;
		xDetailXY *= GetDetailNormalStrength(m) * fDetailMask;
		xNormalTS = float3(xNormalTS.xy + xDetailXY, xNormalTS.z);

		// x2 multiply detail albedo around mid-grey (Unity convention).
		float3 xDetailAlbedo = xDetailAlbedoTex.Sample(xDetailUV).rgb;
		float3 xOverlaid = xAlbedo.rgb * xDetailAlbedo * 2.0;
		xAlbedo.rgb = lerp(xAlbedo.rgb, xOverlaid, GetDetailAlbedoStrength(m) * fDetailMask);
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
							 GetRoughnessMultiplier(m), GetMetallicMultiplier(m),
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
	s.specular      = fSpecTexel * GetSpecular(m);
	s.ao            = SampleOcclusion(xOcclusionTex, xUV, GetOcclusionStrength(m));
	s.emissive      = SampleEmissiveRGB(xEmissiveTex, xUV, GetEmissiveColor(m), GetEmissiveIntensity(m));
	s.ccStrength    = ((uFlags & MATERIAL_FLAG_CLEARCOAT) != 0u) ? GetClearCoatStrength(m) : 0.0;
	s.ccRoughness   = GetClearCoatRoughness(m);
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

// =====================================================================
// Surface-model seam (Slang interface).
//
// A "surface model" is anything that produces the shared MaterialSurface for a
// fragment; the G-buffer packing (MakeGBuffer above) and the lighting stay OUTSIDE it.
// This interface is the extension point a subsystem with a bespoke surface — e.g.
// Terrain's 4-material splatmap blend — plugs into WITHOUT editing Common: it declares
// its own conforming type via `extension` next to its shader (see
// Flux_Terrain_ToGBuffer.slang's TerrainSplatSurface). ISurfaceModel is used as a
// generic/extension CONSTRAINT only, never as an existential value, so it adds no
// reflection.
// =====================================================================
public interface ISurfaceModel
{
	MaterialSurface Evaluate();
}

// The canonical uber-material conformance: the same parameter-based 9-texture
// evaluation every mesh G-buffer writer uses (EvaluateMaterialSurface above), wrapped
// as an ISurfaceModel. It holds its inputs as fields so the interface method is nullary;
// Evaluate() forwards to EvaluateMaterialSurface with the body unchanged. (A mesh shader
// may build one and call Evaluate() instead of calling EvaluateMaterialSurface directly;
// the two are identical.)
public struct UberMaterialSurface : ISurfaceModel
{
	public Sampler2D baseColorTex;
	public Sampler2D normalTex;
	public Sampler2D roughnessMetallicTex;
	public Sampler2D occlusionTex;
	public Sampler2D emissiveTex;
	public Sampler2D heightTex;
	public Sampler2D detailAlbedoTex;
	public Sampler2D detailNormalTex;
	public Sampler2D detailMaskTex;
	public float2     uv;
	public float3x3   tbn;
	public float3     worldViewDir;
	public float4     vertexColor;
	public bool       frontFace;
	public MaterialGPU material;

	public MaterialSurface Evaluate()
	{
		return EvaluateMaterialSurface(
			baseColorTex, normalTex, roughnessMetallicTex,
			occlusionTex, emissiveTex, heightTex,
			detailAlbedoTex, detailNormalTex, detailMaskTex,
			uv, tbn, worldViewDir, vertexColor, frontFace, material);
	}
}
