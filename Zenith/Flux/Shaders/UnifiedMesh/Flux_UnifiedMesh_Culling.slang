// =====================================================================
// Flux_UnifiedMesh_Culling — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// GPU frustum culling, ONE thread per DRAW-ITEM (submesh) over the whole scene
// (64 per workgroup). Each draw-item carries its own local-space bounding sphere
// and an objectIndex; the thread transforms the sphere by the object's world
// matrix, tests it against the 6 frustum planes, and on survival appends the
// global draw-item index into its bucket's partition of the shared visible-index
// buffer + atomically bumps that bucket's indirect instanceCount.
//
// Output partitioning: the visible-index buffer is sized to the total draw-item
// count and split into per-bucket sub-ranges by bucketOffset[] (a CPU prefix-sum
// of per-bucket draw-item counts). A survivor for bucket b lands at
// visibleIndex[bucketOffset[b] + localSlot], where localSlot is the value of the
// atomic increment of the bucket's instanceCount (indirect[b*5+1]) — so that
// instanceCount doubles as both the per-bucket output cursor AND the
// VkDrawIndexedIndirectCommand instanceCount the GBuffer draw reads.
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"

struct FrustumPlane
{
    float4 normalAndDistance; // xyz = normal, w = distance; planes point INWARD
};

struct CullingConstantsLayout
{
    FrustumPlane frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
    float4       cameraPosition;
    uint         totalDrawItemCount;
    uint         pad0;
    uint         pad1;
    uint         pad2;
};

struct PassParams
{
    ConstantBuffer<CullingConstantsLayout> CullingConstants;
    RWStructuredBuffer<GPUSceneObject>     Objects;       // dynamic (host-uploaded)
    RWStructuredBuffer<GPUSceneDrawItem>   DrawItems;     // dynamic (host-uploaded)
    RWStructuredBuffer<uint>               bucketOffset;  // dynamic [numBuckets] prefix-sum
    RWStructuredBuffer<uint>               visibleIndex;  // persistent [totalDrawItems] output
    RWStructuredBuffer<uint>               indirect;      // persistent [numBuckets*5] instanceCount cursor
};
ParameterBlock<PassParams> g_xPassSet;

// True if the bounding sphere is at least partially inside the frustum.
bool TestSphereFrustum(float3 center, float radius)
{
    const int kPlaneOrder[6] = { 4, 5, 0, 1, 2, 3 }; // near, far, then sides
    for (int i = 0; i < 6; i++)
    {
        FrustumPlane p = g_xPassSet.CullingConstants.frustumPlanes[kPlaneOrder[i]];
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
    uint gid = dispatchId.x;
    if (gid >= g_xPassSet.CullingConstants.totalDrawItemCount) return;

    GPUSceneDrawItem di  = g_xPassSet.DrawItems[gid];
    GPUSceneObject   obj = g_xPassSet.Objects[di.m_uObjectIndex];

    float4 sph = di.m_xLocalBoundsSphere;
    float3 worldCenter = mul(obj.m_xModelMatrix, float4(sph.xyz, 1.0)).xyz;
    float  worldRadius = sph.w * GetMaxScale(obj.m_xModelMatrix);

    if (!TestSphereFrustum(worldCenter, worldRadius)) return;

    uint b = di.m_uBucketIndex;

    // Atomically reserve this survivor's slot within the bucket. indirect[b*5+1] is
    // the VkDrawIndexedIndirectCommand instanceCount — incrementing it returns the
    // per-bucket local slot AND grows the draw's instance count in one operation.
    uint localSlot;
    InterlockedAdd(g_xPassSet.indirect[b * 5u + 1u], 1u, localSlot);
    g_xPassSet.visibleIndex[g_xPassSet.bucketOffset[b] + localSlot] = gid;
}
