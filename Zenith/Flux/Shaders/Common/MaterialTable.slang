// =====================================================================
// Common.MaterialTable — shared Slang module (importable; no bindings).
//
// The GPU material record (MaterialGPU) + the material-parameter accessors.
// One record per material lives in the GLOBAL set's g_axMaterials structured
// buffer (declared in Common/Bindings.slang); a draw selects its record by
// index (DrawConstantsLayout.g_uMaterialIndex). The 9 texture slots are
// bindless indices into the BINDLESS set's g_axTextures[] table — the entry
// shader resolves each Sampler2D from g_axTextures[m.<slot>Idx] and passes
// them to EvaluateMaterialSurface (Common.MaterialSurface).
//
// This module declares NO ParameterBlock / resource, so importing it does NOT
// disturb the spine space assignment (0/1/2). It is the SINGLE source of the
// MaterialGPU type — imported by Common/Bindings.slang (for the g_axMaterials
// member) and Common.MaterialSurface (for the accessors) so the type identity
// matches across module boundaries.
//
// Mirrors C++ Flux_MaterialGPU (Flux/Flux_MaterialGPU.h) byte-for-byte.
// =====================================================================

// Per-material GPU record. All-16-byte-aligned members (float4 / uint4) so the
// std430 structured-buffer layout matches the C++ POD with no scalar packing.
public struct MaterialGPU
{
	public float4 g_xBaseColor;        // RGBA tint
	public float4 g_xMaterialParams;   // metallic, roughness, alphaCutoff, occlusionStrength
	public float4 g_xUVParams;         // tilingX, tilingY, offsetX, offsetY
	public float4 g_xEmissiveParams;   // R, G, B, intensity
	public float4 g_xMaterialParams2;  // specular, normalStrength, clearCoatStrength, clearCoatRoughness
	public float4 g_xParallaxParams;   // heightScale, pomMinSteps, pomMaxSteps, unused
	public float4 g_xDetailParams;     // detailTilingX, detailTilingY, detailNormalStrength, detailAlbedoStrength
	public float4 g_xFlagsParams;      // MaterialDrawFlags (uint bits in .x), unused x3
	public uint4  g_xTexIdx0;          // bindless: baseColor, normal, roughnessMetallic, occlusion
	public uint4  g_xTexIdx1;          // bindless: emissive, height, detailAlbedo, detailNormal
	public uint4  g_xTexIdx2;          // bindless: detailMask, unused x3
};

// Material parameter accessors. Replace the old Common.DrawConstants accessors:
// the per-draw constants no longer carry material scalars (only model matrix +
// material index + VAT), so these now read from the per-material GPU record.
public float  GetMetallicMultiplier(MaterialGPU m)    { return m.g_xMaterialParams.x; }
public float  GetRoughnessMultiplier(MaterialGPU m)   { return m.g_xMaterialParams.y; }
public float  GetAlphaCutoff(MaterialGPU m)           { return m.g_xMaterialParams.z; }
public float  GetOcclusionStrength(MaterialGPU m)     { return m.g_xMaterialParams.w; }
public float2 GetUVTiling(MaterialGPU m)              { return m.g_xUVParams.xy; }
public float2 GetUVOffset(MaterialGPU m)              { return m.g_xUVParams.zw; }
public float3 GetEmissiveColor(MaterialGPU m)         { return m.g_xEmissiveParams.rgb; }
public float  GetEmissiveIntensity(MaterialGPU m)     { return m.g_xEmissiveParams.w; }
public float  GetSpecular(MaterialGPU m)              { return m.g_xMaterialParams2.x; }
public float  GetNormalStrength(MaterialGPU m)        { return m.g_xMaterialParams2.y; }
public float  GetClearCoatStrength(MaterialGPU m)     { return m.g_xMaterialParams2.z; }
public float  GetClearCoatRoughness(MaterialGPU m)    { return m.g_xMaterialParams2.w; }
public float  GetHeightScale(MaterialGPU m)           { return m.g_xParallaxParams.x; }
public float  GetPOMMinSteps(MaterialGPU m)           { return m.g_xParallaxParams.y; }
public float  GetPOMMaxSteps(MaterialGPU m)           { return m.g_xParallaxParams.z; }
public float2 GetDetailTiling(MaterialGPU m)          { return m.g_xDetailParams.xy; }
public float  GetDetailNormalStrength(MaterialGPU m)  { return m.g_xDetailParams.z; }
public float  GetDetailAlbedoStrength(MaterialGPU m)  { return m.g_xDetailParams.w; }
public uint   GetMaterialFlags(MaterialGPU m)         { return asuint(m.g_xFlagsParams.x); }
