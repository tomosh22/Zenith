// =====================================================================
// Common.DrawConstants — shared Slang module.
//
// Per-draw material constants (208 bytes), bound at set=1 binding=0 in
// the mesh path. Layout matches C++ MaterialDrawConstants in
// Flux_MaterialBinding.h. Every mesh G-buffer/shadow writer — including
// InstancedMeshes — uses this same layout; g_xEmissiveParams carries real
// emissive. InstancedMeshes' VAT params ride the appended g_xVATParams field
// (0 for non-instanced) — not a separate constant buffer, because the backend
// allows only one scratch/draw-constants buffer per descriptor set.
//
// Helper accessors mirror the GLSL macro names from MaterialCommon.fxh.
// =====================================================================

public struct DrawConstantsLayout
{
	public float4x4 g_xModelMatrix;       // 64 bytes — offset   0
	public float4   g_xBaseColor;         // 16 bytes — offset  64 — RGBA tint
	public float4   g_xMaterialParams;    // 16 bytes — offset  80 — (metallic, roughness, alphaCutoff, occlusionStrength)
	public float4   g_xUVParams;          // 16 bytes — offset  96 — (tilingX, tilingY, offsetX, offsetY)
	public float4   g_xEmissiveParams;    // 16 bytes — offset 112 — (R, G, B, intensity)
	public float4   g_xMaterialParams2;   // 16 bytes — offset 128 — (specular, normalStrength, clearCoatStrength, clearCoatRoughness)
	public float4   g_xParallaxParams;    // 16 bytes — offset 144 — (heightScale, pomMinSteps, pomMaxSteps, unused)
	public float4   g_xDetailParams;      // 16 bytes — offset 160 — (detailTilingX, detailTilingY, detailNormalStrength, detailAlbedoStrength)
	public float4   g_xFlagsParams;       // 16 bytes — offset 176 — (MaterialDrawFlags as uint bits, unused x3)
	public float4   g_xVATParams;         // 16 bytes — offset 192 — InstancedMeshes VAT (texW, texH, vat-enabled, unused); 0 for non-instanced
};

// NO file-scope binding. A bare global resource would claim descriptor space 0
// and break the ParameterBlock spine (Common/Bindings.slang — sets 0/1/2 are the
// fixed GLOBAL/VIEW/BINDLESS persistent sets). Each mesh-path shader instead
// declares its own ConstantBuffer<DrawConstantsLayout> as a member of its DRAW
// ParameterBlock and passes the loaded value (DrawConstantsLayout) to the
// accessors / evaluator below.

// Material feature flag bits — MUST match MaterialDrawFlags in
// Flux_MaterialBinding.h.
public static const uint MATERIAL_FLAG_UNLIT                 = 1u << 0;
public static const uint MATERIAL_FLAG_TWO_SIDED_NORMAL_FLIP = 1u << 1;
public static const uint MATERIAL_FLAG_POM                   = 1u << 2;
public static const uint MATERIAL_FLAG_DETAIL_MAPS           = 1u << 3;
public static const uint MATERIAL_FLAG_CLEARCOAT             = 1u << 4;
public static const uint MATERIAL_FLAG_SKIN                  = 1u << 5;

// Material parameter accessors — shorter call sites, matching the GLSL macros
// from MaterialCommon.fxh. Each takes the per-draw constants by value (the
// caller loads its DRAW-block ConstantBuffer once).
public float  GetMetallicMultiplier(DrawConstantsLayout dc)    { return dc.g_xMaterialParams.x; }
public float  GetRoughnessMultiplier(DrawConstantsLayout dc)   { return dc.g_xMaterialParams.y; }
public float  GetAlphaCutoff(DrawConstantsLayout dc)           { return dc.g_xMaterialParams.z; }
public float  GetOcclusionStrength(DrawConstantsLayout dc)     { return dc.g_xMaterialParams.w; }
public float2 GetUVTiling(DrawConstantsLayout dc)              { return dc.g_xUVParams.xy; }
public float2 GetUVOffset(DrawConstantsLayout dc)              { return dc.g_xUVParams.zw; }
public float3 GetEmissiveColor(DrawConstantsLayout dc)         { return dc.g_xEmissiveParams.rgb; }
public float  GetEmissiveIntensity(DrawConstantsLayout dc)     { return dc.g_xEmissiveParams.w; }
public float  GetSpecular(DrawConstantsLayout dc)              { return dc.g_xMaterialParams2.x; }
public float  GetNormalStrength(DrawConstantsLayout dc)        { return dc.g_xMaterialParams2.y; }
public float  GetClearCoatStrength(DrawConstantsLayout dc)     { return dc.g_xMaterialParams2.z; }
public float  GetClearCoatRoughness(DrawConstantsLayout dc)    { return dc.g_xMaterialParams2.w; }
public float  GetHeightScale(DrawConstantsLayout dc)           { return dc.g_xParallaxParams.x; }
public float  GetPOMMinSteps(DrawConstantsLayout dc)           { return dc.g_xParallaxParams.y; }
public float  GetPOMMaxSteps(DrawConstantsLayout dc)           { return dc.g_xParallaxParams.z; }
public float2 GetDetailTiling(DrawConstantsLayout dc)          { return dc.g_xDetailParams.xy; }
public float  GetDetailNormalStrength(DrawConstantsLayout dc)  { return dc.g_xDetailParams.z; }
public float  GetDetailAlbedoStrength(DrawConstantsLayout dc)  { return dc.g_xDetailParams.w; }
public uint   GetMaterialFlags(DrawConstantsLayout dc)         { return asuint(dc.g_xFlagsParams.x); }
// InstancedMeshes VAT params (0 for non-instanced draws → VAT disabled).
public float2 GetVATTextureSize(DrawConstantsLayout dc)        { return dc.g_xVATParams.xy; }
public bool   GetVATEnabled(DrawConstantsLayout dc)            { return dc.g_xVATParams.z > 0.0; }
