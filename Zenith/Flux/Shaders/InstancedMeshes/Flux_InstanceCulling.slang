// =====================================================================
// Flux_InstanceCulling — Slang module.
//
// Subsystem: InstancedMeshes. GPU frustum culling. One thread per
// instance (64 per workgroup); tests bounding-sphere vs each of the 6
// frustum planes, writes the surviving instance indices to a compact
// list, and atomically bumps the indirect-draw instance count so the
// subsequent DrawIndexedIndirect call sees the right count.
//
// matches Vulkan VkDrawIndexedIndirectCommand layout:
// [0]=indexCount, [1]=instanceCount, [2]=firstIndex, [3]=vertexOffset,
// [4]=firstInstance — only [1] is updated here.
// =====================================================================

struct FrustumPlane
{
	float4 normalAndDistance; // xyz = normal, w = distance; planes point INWARD
};

struct CullingConstantsLayout
{
	FrustumPlane frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
	float4       cameraPosition;
	uint         totalInstanceCount;
	uint         meshIndexCount;
	float        boundingSphereRadius;
	float        padding;
};
[[vk::binding(0, 0)]] ConstantBuffer<CullingConstantsLayout> CullingConstants;

[[vk::binding(1, 0)]] RWStructuredBuffer<float4x4> TransformBuffer;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint4>    AnimDataBuffer;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint>     VisibleIndexBuffer;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint>     visibleCount;
[[vk::binding(5, 0)]] RWStructuredBuffer<uint>     indirectInstanceCount;

// True if the bounding sphere is at least partially inside the frustum.
// Tests near plane first — most rays-of-cuts come from the near plane.
bool TestSphereFrustum(float3 center, float radius)
{
	// Order matters for early-out: near, far, then sides.
	const int kPlaneOrder[6] = { 4, 5, 0, 1, 2, 3 };
	for (int i = 0; i < 6; i++)
	{
		FrustumPlane p = CullingConstants.frustumPlanes[kPlaneOrder[i]];
		if (dot(p.normalAndDistance.xyz, center) + p.normalAndDistance.w < -radius) return false;
	}
	return true;
}

// Max basis-vector length in the upper-3x3 — gives the largest world-
// space scale so the bounding sphere stays conservative under non-
// uniform scale.
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
	uint idx = dispatchId.x;
	if (idx >= CullingConstants.totalInstanceCount) return;

	// Per-instance enable bit (bit 0 of animData.w) lets the CPU
	// disable instances without resizing the array.
	uint flags = AnimDataBuffer[idx].w;
	if ((flags & 1u) == 0u) return;

	float4x4 transform   = TransformBuffer[idx];
	float3   worldCenter = float3(transform._m03, transform._m13, transform._m23);
	float    worldRadius = CullingConstants.boundingSphereRadius * GetMaxScale(transform);

	if (!TestSphereFrustum(worldCenter, worldRadius)) return;

	// Atomically allocate an output slot. visibleCount[0] is the running
	// total; indirectInstanceCount[1] is the VkDrawIndexedIndirectCommand
	// instanceCount field (subsequent indirect draw reads it directly).
	uint outIndex;
	InterlockedAdd(visibleCount[0], 1u, outIndex);
	VisibleIndexBuffer[outIndex] = idx;

	uint dummy;
	InterlockedAdd(indirectInstanceCount[1], 1u, dummy);
}
