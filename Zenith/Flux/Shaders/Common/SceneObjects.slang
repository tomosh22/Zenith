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
    uint     m_uFlags;           // offset 64
    uint     m_uBonePaletteRef;  // offset 68 — reserved (Stage 5)
    uint     m_uPad0;            // offset 72
    uint     m_uPad1;            // offset 76
};                               // size 80

struct GPUSceneDrawItem
{
    uint   m_uObjectIndex;       // offset  0 — index into the object array
    uint   m_uBucketIndex;       // offset  4 — owning bucket (one indirect draw)
    uint   m_uColorTintPacked;   // offset  8 — per-instance tint (RGBA8)
    uint   m_uFlags;             // offset 12
    float4 m_xLocalBoundsSphere; // offset 16 — submesh local sphere (center.xyz, radius.w)
};                               // size 32

#endif // FLUX_COMMON_SCENEOBJECTS_SLANG
