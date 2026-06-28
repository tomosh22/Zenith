// =====================================================================
// Common/SceneObjects — GPU-scene record structs for the unified GPU-driven
// opaque-mesh pipeline (Flux_UnifiedMesh, Stage 1).
//
// These MUST stay byte-identical to the C++ mirrors in Flux/Flux_GPUScene.h
// (Flux_GPUSceneObject 80B / Flux_GPUSceneDrawItem 32B, both static_assert-
// locked). std430 structured-buffer layout: every member is naturally aligned
// (uint @4, float4/float4x4 @16) so there are no scalar-packing surprises and
// the C++ pad fields line up with the trailing uints here.
//
//   GPUSceneObject  : one per scene item   — the object's world transform.
//   GPUSceneDrawItem: one per SUBMESH      — objectIndex + owning bucket +
//                     the submesh's local-space bounding sphere (cull primitive).
//
// #include'd (not import'd) by the unified reset / cull / GBuffer shaders.
// =====================================================================

#ifndef FLUX_COMMON_SCENEOBJECTS_SLANG
#define FLUX_COMMON_SCENEOBJECTS_SLANG

struct GPUSceneObject
{
    float4x4 m_xModelMatrix;     // offset  0 — object world transform
    uint     m_uFlags;           // offset 64 — FLUX_GPUSCENE_OBJFLAG_* (bit 0 = VAT)
    uint     m_uBonePaletteRef;  // offset 68 — reserved (Stage 5)
    uint     m_uVATAnimPacked;   // offset 72 — VAT animIndex (low 16) | frameCount (high 16)
    uint     m_uVATAnimTime;     // offset 76 — VAT normalised anim time (float bits)
};                               // size 80

// Object flag bits (mirror uFLUX_GPUSCENE_OBJFLAG_* in Flux_GPUScene.h).
static const uint FLUX_GPUSCENE_OBJFLAG_VAT = 0x1u;

struct GPUSceneDrawItem
{
    uint   m_uObjectIndex;       // offset  0 — index into the object array
    uint   m_uBucketIndex;       // offset  4 — owning bucket (one indirect draw)
    uint   m_uColorTintPacked;   // offset  8 — per-instance tint (RGBA8)
    uint   m_uFlags;             // offset 12
    float4 m_xLocalBoundsSphere; // offset 16 — submesh local sphere (center.xyz, radius.w)
};                               // size 32

#endif // FLUX_COMMON_SCENEOBJECTS_SLANG
