// =====================================================================
// Flux_UnifiedMesh_Culling — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// GPU frustum culling, ONE thread per (VIEW, DRAW-ITEM) pair over the whole scene
// (64 per workgroup). Each draw-item carries its own local-space bounding sphere
// and an objectIndex; the thread transforms the sphere by the object's world
// matrix, tests it against the thread's VIEW frustum (6 planes), and on survival
// appends the global draw-item index into that view's bucket partition of the
// shared visible-index buffer + atomically bumps that (view,bucket) indirect
// instanceCount.
//
// VIEW dimension: FIXED render-view slots (Flux/RenderViews/Flux_RenderViews.h —
// 0 = camera, 1..N = shadow cascades, then the preview). The thread id decodes to
// (v, gid) view-major: v = tid / totalDrawItems, gid = tid % totalDrawItems. Each
// ACTIVE view has its own 6 frustum planes (extracted from its staged view-proj on
// the CPU); inactive slots inside [0, numViews) are skipped via activeViewMask.
// Each draw-item also carries a per-view-slot VISIBILITY MASK (high 16 bits of
// m_uFlags): scene content targets the camera + cascades; the material-preview
// mesh targets only the preview slot.
//
// Output partitioning: per view, the visible-index buffer holds a totalDrawItems-
// wide slice (base v*totalDrawItems) split into per-bucket sub-ranges by
// bucketOffset[] (a CPU prefix-sum of per-bucket draw-item counts, view-invariant).
// A survivor for (view v, bucket b) lands at
// visibleIndex[v*totalDrawItems + bucketOffset[b] + localSlot], where localSlot is
// the atomic increment of that command's instanceCount (indirect[(v*numBuckets+b)*5+1])
// — so instanceCount doubles as both the per-(view,bucket) output cursor AND the
// VkDrawIndexedIndirectCommand instanceCount the GBuffer/shadow draw reads.
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"

// Must match kuUNIFIED_MAX_VIEWS in Flux_UnifiedMesh.cpp (fixed render-view slot capacity).
static const uint kUNIFIED_MAX_VIEWS = 8u;

struct FrustumPlane
{
    float4 normalAndDistance; // xyz = normal, w = distance; planes point INWARD
};

struct CullingConstantsLayout
{
    FrustumPlane frustumPlanes[kUNIFIED_MAX_VIEWS * 6]; // per view slot: Left,Right,Bottom,Top,Near,Far
    float4       cameraPosition;
    uint         totalDrawItemCount;  // per-view draw-item count (per-view stride)
    uint         numViews;            // 1 + highest active view slot this frame
    uint         numBuckets;          // bucket slot count (per-view indirect stride)
    uint         numObjects;          // object-array size — bounds-guards the Objects[] read
    uint         activeViewMask;      // bit i ⇔ view slot i active (skip inactive holes)
    uint         pad0;
    uint         pad1;
    uint         pad2;
};

struct PassParams
{
    ConstantBuffer<CullingConstantsLayout> CullingConstants;
    RWStructuredBuffer<GPUSceneObject>     Objects;       // dynamic (host-uploaded)
    RWStructuredBuffer<GPUSceneDrawItem>   DrawItems;     // dynamic (host-uploaded)
    RWStructuredBuffer<uint>               bucketOffset;  // dynamic [numBuckets] prefix-sum (view-invariant)
    RWStructuredBuffer<uint>               visibleIndex;  // persistent [numViews*totalDrawItems] output
    RWStructuredBuffer<uint>               indirect;      // persistent [numViews*numBuckets*5] instanceCount cursor
};
ParameterBlock<PassParams> g_xPassSet;

// True if the bounding sphere is at least partially inside view v's frustum.
bool TestSphereFrustum(uint v, float3 center, float radius)
{
    const int kPlaneOrder[6] = { 4, 5, 0, 1, 2, 3 }; // near, far, then sides
    uint base = v * 6u;
    for (int i = 0; i < 6; i++)
    {
        FrustumPlane p = g_xPassSet.CullingConstants.frustumPlanes[base + (uint)kPlaneOrder[i]];
        if (dot(p.normalAndDistance.xyz, center) + p.normalAndDistance.w < -radius) return false;
    }
    return true;
}

// Max basis-vector length in the upper-3x3 — largest world-space scale, so the
// bounding sphere stays conservative under non-uniform scale.
float GetMaxScale(float4x4 m)
{
    float scaleX = length(float3(m._m00, m._m10, m._m20));
    float scaleY = length(float3(m._m01, m._m11, m._m21));
    float scaleZ = length(float3(m._m02, m._m12, m._m22));
    return max(scaleX, max(scaleY, scaleZ));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
    uint totalDrawItems = g_xPassSet.CullingConstants.totalDrawItemCount;
    uint tid = dispatchId.x;
    if (tid >= totalDrawItems * g_xPassSet.CullingConstants.numViews) return;

    uint v   = tid / totalDrawItems;       // render-view slot (0 = camera)
    uint gid = tid - v * totalDrawItems;   // draw-item index within the view's slice

    // Inactive view slots inside [0, numViews) — e.g. cascades off while the
    // preview is active — cull nothing (their passes never read the partition).
    if ((g_xPassSet.CullingConstants.activeViewMask & (1u << v)) == 0u) return;

    GPUSceneDrawItem di = g_xPassSet.DrawItems[gid];

    // Per-draw-item view mask (high 16 bits of m_uFlags): the item participates
    // only in the view slots its producer targeted. Tested BEFORE the object
    // fetch — the mask alone excludes preview-only items from every scene view
    // (and scene items from the preview) without touching the object record.
    if (((di.m_uFlags >> GPUSCENE_DRAWITEM_VIEWMASK_SHIFT) & (1u << v)) == 0u) return;

    // Defensive bounds on the DATA-DRIVEN indices before any UAV access. A correct host
    // build keeps objectIndex < numObjects and bucketIndex < numBuckets by construction,
    // but the Objects[] read and the indirect/visibleIndex writes below are UAVs — an
    // out-of-range bucketIndex would be an OOB UAV WRITE (GPU memory corruption / TDR).
    // This converts a stale buffer slot or a future gather bug into a safe skip.
    uint numBuckets = g_xPassSet.CullingConstants.numBuckets;
    if (di.m_uObjectIndex >= g_xPassSet.CullingConstants.numObjects) return;
    if (di.m_uBucketIndex >= numBuckets) return;

    GPUSceneObject obj = g_xPassSet.Objects[di.m_uObjectIndex];

    float4 sph = di.m_xLocalBoundsSphere;
    float3 worldCenter = mul(obj.m_xModelMatrix, float4(sph.xyz, 1.0)).xyz;
    float  worldRadius = sph.w * GetMaxScale(obj.m_xModelMatrix);

    if (!TestSphereFrustum(v, worldCenter, worldRadius)) return;

    uint b = di.m_uBucketIndex;

    // Atomically reserve this survivor's slot within (view v, bucket b).
    // indirect[(v*numBuckets+b)*5+1] is the VkDrawIndexedIndirectCommand instanceCount
    // — incrementing it returns the per-command local slot AND grows the draw's
    // instance count in one operation.
    uint localSlot;
    InterlockedAdd(g_xPassSet.indirect[(v * numBuckets + b) * 5u + 1u], 1u, localSlot);
    g_xPassSet.visibleIndex[v * totalDrawItems + g_xPassSet.bucketOffset[b] + localSlot] = gid;
}
