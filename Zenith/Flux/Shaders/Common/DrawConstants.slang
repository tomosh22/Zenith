// =====================================================================
// Common.DrawConstants — shared Slang module.
//
// Per-draw constants (96 bytes) — model matrix + the material-table index +
// InstancedMeshes VAT params. Layout matches C++ MeshDrawConstants in
// Flux_MaterialBinding.h. Material SCALARS no longer live here: they moved to
// the per-material GPU record (Common.MaterialTable / Flux_MaterialGPU), which
// a draw selects via g_uMaterialIndex. This keeps the per-draw constants small
// and lets many draws share one material record.
//
// Every mesh G-buffer/shadow writer — including InstancedMeshes — uses this same
// layout. InstancedMeshes' VAT params ride the appended g_xVATParams field
// (0 for non-instanced) — not a separate constant buffer, because the backend
// allows only one scratch/draw-constants buffer per descriptor set.
// =====================================================================

public struct DrawConstantsLayout
{
	public float4x4 g_xModelMatrix;       // 64 bytes — offset  0
	public uint     g_uMaterialIndex;     //  4 bytes — offset 64 — index into g_axMaterials
	public uint     g_uPad0;              //  4 bytes — offset 68
	public uint     g_uPad1;              //  4 bytes — offset 72
	public uint     g_uPad2;              //  4 bytes — offset 76
	public float4   g_xVATParams;         // 16 bytes — offset 80 — InstancedMeshes VAT (texW, texH, vat-enabled, unused); 0 for non-instanced
};

// NO file-scope binding. A bare global resource would claim descriptor space 0
// and break the ParameterBlock spine (Common/Bindings.slang — sets 0/1/2 are the
// fixed GLOBAL/VIEW/BINDLESS persistent sets). Each mesh-path shader instead
// declares its own ConstantBuffer<DrawConstantsLayout> as a member of its DRAW
// ParameterBlock and passes the loaded value (DrawConstantsLayout) to the
// VAT accessors below.

// Material feature flag bits — MUST match MaterialDrawFlags in
// Flux_MaterialBinding.h. Read from the material record via GetMaterialFlags
// (Common.MaterialTable); the bit constants live here because the shaders that
// test them already import Common.DrawConstants.
public static const uint MATERIAL_FLAG_UNLIT                 = 1u << 0;
public static const uint MATERIAL_FLAG_TWO_SIDED_NORMAL_FLIP = 1u << 1;
public static const uint MATERIAL_FLAG_POM                   = 1u << 2;
public static const uint MATERIAL_FLAG_DETAIL_MAPS           = 1u << 3;
public static const uint MATERIAL_FLAG_CLEARCOAT             = 1u << 4;
public static const uint MATERIAL_FLAG_SKIN                  = 1u << 5;

// InstancedMeshes VAT params (0 for non-instanced draws → VAT disabled). These
// stay on the per-draw constants — they are per-draw-call (the VAT animation
// texture dimensions), not per-material.
public float2 GetVATTextureSize(DrawConstantsLayout dc)        { return dc.g_xVATParams.xy; }
public bool   GetVATEnabled(DrawConstantsLayout dc)            { return dc.g_xVATParams.z > 0.0; }
