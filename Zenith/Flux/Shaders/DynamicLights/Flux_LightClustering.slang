// =====================================================================
// Flux_LightClustering — Slang module.
//
// Subsystem: DynamicLights. Per-cluster light culling for clustered
// deferred lighting. One thread per cluster (16x9x24 = 3456 total),
// dispatched as 24 workgroups of 16x9 threads. Each thread:
//   1. Reconstructs its cluster's view-space AABB.
//   2. Iterates the dynamic light list (uploaded by Flux_DynamicLights).
//   3. Tests sphere-vs-AABB (point/spot use a conservative bounding
//      sphere; directional lights are always added).
//   4. Writes matching indices into ClusterLightIndices and the count
//      into ClusterLightCounts.
//
// MAX_LIGHTS_PER_CLUSTER (64, defined in Common.Lighting) caps the
// per-cluster list length. Overflow is silently clamped — acceptable
// for v1, since the global cap is 256 and 64+ lights overlapping one
// cluster is implausible at typical scene density.
//
// Light positions are transformed into view space inside the loop so
// the cluster bounds don't need to be transformed back to world space.
// Engine convention is +Z forward in view space (see Flux/CLAUDE.md).
// =====================================================================

import Common.Frame;
import Common.Lighting;

[[vk::binding(1, 0)]] StructuredBuffer<LightInstance>   LightBuffer;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint>           ClusterLightCounts;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint>           ClusterLightIndices;

struct PushConstantsLayout
{
	uint m_uLightCount;
	uint m_uPad0;
	uint m_uPad1;
	uint m_uPad2;
};
[[vk::binding(4, 0)]] ConstantBuffer<PushConstantsLayout> PushConstants;

[shader("compute")]
[numthreads(CLUSTER_DIM_X, CLUSTER_DIM_Y, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	// dispatchId.xy = cluster XY in [0..CLUSTER_DIM_X, 0..CLUSTER_DIM_Y),
	// dispatchId.z = cluster Z slice. The C++ side dispatches
	// (1, 1, CLUSTER_DIM_Z) workgroups so each thread maps to exactly
	// one cluster.
	uint3 xClusterCoord = dispatchId;

	// Defensive — should never trip given the dispatch dims, but cheap.
	if (xClusterCoord.x >= CLUSTER_DIM_X ||
		xClusterCoord.y >= CLUSTER_DIM_Y ||
		xClusterCoord.z >= CLUSTER_DIM_Z) return;

	uint uClusterIdx = ClusterIndex(xClusterCoord);

	// Build cluster's view-space AABB from the inverse projection at
	// the cluster's UV corners and the slice's near/far Z planes.
	float3 xClusterMin;
	float3 xClusterMax;
	ComputeClusterAABB(xClusterCoord, xClusterMin, xClusterMax);

	uint uLocalCount = 0u;
	uint uLightCount = PushConstants.m_uLightCount;

	for (uint i = 0u; i < uLightCount; ++i)
	{
		if (uLocalCount >= MAX_LIGHTS_PER_CLUSTER) break;

		LightInstance xLight = LightBuffer[i];
		uint uType = uint(xLight.m_xTypeOuter.y);

		bool bInclude = false;

		if (uType == LIGHT_TYPE_DIRECTIONAL)
		{
			// Directional lights affect every cluster.
			bInclude = true;
		}
		else
		{
			// Point/spot — test bounding sphere vs cluster AABB.
			// Spot uses the spot's max-range sphere (conservative; a
			// tighter sphere-vs-cone test could reject more clusters
			// but adds branching for small wins given typical light
			// density).
			float3 xWorldPos = xLight.m_xPositionRange.xyz;
			float  fRange    = xLight.m_xPositionRange.w;

			// Transform light position into view space.
			float4 xViewPos4 = mul(FrameConstants.g_xViewMat, float4(xWorldPos, 1.0));
			float3 xViewPos = xViewPos4.xyz;

			bInclude = SphereIntersectsAABB(xViewPos, fRange, xClusterMin, xClusterMax);
		}

		if (bInclude)
		{
			ClusterLightIndices[uClusterIdx * MAX_LIGHTS_PER_CLUSTER + uLocalCount] = i;
			uLocalCount += 1u;
		}
	}

	ClusterLightCounts[uClusterIdx] = uLocalCount;
}
