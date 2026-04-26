// =====================================================================
// Common.DrawConstants — shared Slang module.
//
// Per-draw material constants (128 bytes), bound at set=1 binding=0 in
// the mesh path. Layout matches C++ MaterialDrawConstants in
// Flux_MaterialBinding.h.
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
	public float4x4 g_xModelMatrix;       // 64 bytes
	public float4   g_xBaseColor;         // 16 bytes — RGBA tint
	public float4   g_xMaterialParams;    // 16 bytes — (metallic, roughness, alphaCutoff, occlusionStrength)
	public float4   g_xUVParams;          // 16 bytes — (tilingX, tilingY, offsetX, offsetY)
	public float4   g_xEmissiveParams;    // 16 bytes — (R, G, B, intensity) or VAT params for instanced
};

// Bound at set=1 binding=0 by every mesh-path shader. Placed here so a
// single import gets both the type and the binding — every consumer
// references the same physical slot in the descriptor set.
[[vk::binding(0, 1)]] public ConstantBuffer<DrawConstantsLayout> DrawConstants;

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
