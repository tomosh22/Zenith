// =====================================================================
// Common.DrawConstants — shared Slang module.
//
// Per-draw material constants (192 bytes), bound at set=1 binding=0 in
// the mesh path. Layout matches C++ MaterialDrawConstants in
// Flux_MaterialBinding.h — the first five fields must never move (the
// InstancedMeshes vertex path aliases g_xEmissiveParams as VAT params).
//
// The 5th field (g_xEmissiveParams) is repurposed by InstancedMeshes
// vertex shaders as VAT animation texture parameters — both
// interpretations read the same 16 bytes; the C++ caller writes the
// appropriate flavour.
//
// Helper accessors mirror the GLSL macro names from MaterialCommon.fxh.
// =====================================================================

public struct DrawConstantsLayout
{
	public float4x4 g_xModelMatrix;       // 64 bytes — offset   0
	public float4   g_xBaseColor;         // 16 bytes — offset  64 — RGBA tint
	public float4   g_xMaterialParams;    // 16 bytes — offset  80 — (metallic, roughness, alphaCutoff, occlusionStrength)
	public float4   g_xUVParams;          // 16 bytes — offset  96 — (tilingX, tilingY, offsetX, offsetY)
	public float4   g_xEmissiveParams;    // 16 bytes — offset 112 — (R, G, B, intensity) or VAT params for instanced
	public float4   g_xMaterialParams2;   // 16 bytes — offset 128 — (specular, normalStrength, clearCoatStrength, clearCoatRoughness)
	public float4   g_xParallaxParams;    // 16 bytes — offset 144 — (heightScale, pomMinSteps, pomMaxSteps, unused)
	public float4   g_xDetailParams;      // 16 bytes — offset 160 — (detailTilingX, detailTilingY, detailNormalStrength, detailAlbedoStrength)
	public float4   g_xFlagsParams;       // 16 bytes — offset 176 — (MaterialDrawFlags as uint bits, unused x3)
};

// Bound at set=1 binding=0 by every mesh-path shader. Placed here so a
// single import gets both the type and the binding — every consumer
// references the same physical slot in the descriptor set.
[[vk::binding(0, 1)]] public ConstantBuffer<DrawConstantsLayout> DrawConstants;

// Material feature flag bits — MUST match MaterialDrawFlags in
// Flux_MaterialBinding.h.
public static const uint MATERIAL_FLAG_UNLIT                 = 1u << 0;
public static const uint MATERIAL_FLAG_TWO_SIDED_NORMAL_FLIP = 1u << 1;
public static const uint MATERIAL_FLAG_POM                   = 1u << 2;
public static const uint MATERIAL_FLAG_DETAIL_MAPS           = 1u << 3;
public static const uint MATERIAL_FLAG_CLEARCOAT             = 1u << 4;

// Material parameter accessors — shorter call sites, matching the
// GLSL macros from MaterialCommon.fxh.
public float  GetMetallicMultiplier()    { return DrawConstants.g_xMaterialParams.x; }
public float  GetRoughnessMultiplier()   { return DrawConstants.g_xMaterialParams.y; }
public float  GetAlphaCutoff()           { return DrawConstants.g_xMaterialParams.z; }
public float  GetOcclusionStrength()     { return DrawConstants.g_xMaterialParams.w; }
public float2 GetUVTiling()              { return DrawConstants.g_xUVParams.xy; }
public float2 GetUVOffset()              { return DrawConstants.g_xUVParams.zw; }
public float3 GetEmissiveColor()         { return DrawConstants.g_xEmissiveParams.rgb; }
public float  GetEmissiveIntensity()     { return DrawConstants.g_xEmissiveParams.w; }
public float  GetSpecular()              { return DrawConstants.g_xMaterialParams2.x; }
public float  GetNormalStrength()        { return DrawConstants.g_xMaterialParams2.y; }
public float  GetClearCoatStrength()     { return DrawConstants.g_xMaterialParams2.z; }
public float  GetClearCoatRoughness()    { return DrawConstants.g_xMaterialParams2.w; }
public float  GetHeightScale()           { return DrawConstants.g_xParallaxParams.x; }
public float  GetPOMMinSteps()           { return DrawConstants.g_xParallaxParams.y; }
public float  GetPOMMaxSteps()           { return DrawConstants.g_xParallaxParams.z; }
public float2 GetDetailTiling()          { return DrawConstants.g_xDetailParams.xy; }
public float  GetDetailNormalStrength()  { return DrawConstants.g_xDetailParams.z; }
public float  GetDetailAlbedoStrength()  { return DrawConstants.g_xDetailParams.w; }
public uint   GetMaterialFlags()         { return asuint(DrawConstants.g_xFlagsParams.x); }
