// =====================================================================
// Flux_TerrainCulling — Slang module.
//
// Subsystem: Terrain. GPU-driven terrain chunk culling + 2-level LOD
// selection. One thread per chunk; AABB-vs-frustum test, distance-
// based LOD pick, atomic compaction into the indirect-draw buffer.
//
// LOD policy: HIGH (0) when distance² < 1e6 (~1 km), LOW (1) otherwise.
// Sorting is intentionally omitted — modern GPU early-Z handles a few
// percent of depth overdraw far cheaper than the O(n²) sort would.
// =====================================================================

static const uint  TERRAIN_LOD_COUNT      = 2;
static const float LOD_HIGH_MAX_DIST_SQ   = 1000000.0;

struct LODData
{
	uint  firstIndex;
	uint  indexCount;
	uint  vertexOffset;
	float maxDistanceSq;
};

struct TerrainChunkData
{
	float4  aabbMin;                     // xyz = min, w padding
	float4  aabbMax;
	LODData lods[TERRAIN_LOD_COUNT];
};

// VkDrawIndexedIndirectCommand layout (vertexOffset is signed int).
struct DrawIndexedIndirectCommand
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int  vertexOffset;
	uint firstInstance;
};

struct FrustumPlane { float4 normalAndDistance; };

[[vk::binding(0, 0)]] StructuredBuffer<TerrainChunkData>          ChunkBuffer;

struct CameraBufferLayout
{
	FrustumPlane frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
	float4       cameraPosition;
};
[[vk::binding(1, 0)]] ConstantBuffer<CameraBufferLayout>          CameraBuffer;

[[vk::binding(2, 0)]] RWStructuredBuffer<DrawIndexedIndirectCommand> IndirectCommandBuffer;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint>                       visibleCount;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint>                       LODLevelBuffer;

// Conservative AABB-vs-frustum test (Akenine-Möller style). Tests
// near/far first since distant terrain is the most-likely cull case.
bool TestAABBFrustum(float3 aabbMin, float3 aabbMax)
{
	float3 center  = (aabbMin + aabbMax) * 0.5;
	float3 extents = (aabbMax - aabbMin) * 0.5;

	const int kPlaneOrder[6] = { 4, 5, 0, 1, 2, 3 };
	for (int i = 0; i < 6; i++)
	{
		FrustumPlane p     = CameraBuffer.frustumPlanes[kPlaneOrder[i]];
		float3 planeNormal = p.normalAndDistance.xyz;
		float  planeDist   = p.normalAndDistance.w;
		float  radius      = dot(extents, abs(planeNormal));
		float  distance    = dot(planeNormal, center) + planeDist;
		if (distance < -radius) return false;
	}
	return true;
}

uint SelectLOD(float distanceSq)
{
	return (uint)(distanceSq >= LOD_HIGH_MAX_DIST_SQ);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint chunkIndex = dispatchId.x;

	uint chunkCount, stride;
	ChunkBuffer.GetDimensions(chunkCount, stride);
	if (chunkIndex >= chunkCount) return;

	float4 aabbMin = ChunkBuffer[chunkIndex].aabbMin;
	float4 aabbMax = ChunkBuffer[chunkIndex].aabbMax;
	if (!TestAABBFrustum(aabbMin.xyz, aabbMax.xyz)) return;

	float3 chunkCenter = (aabbMin.xyz + aabbMax.xyz) * 0.5;
	float3 toCamera    = CameraBuffer.cameraPosition.xyz - chunkCenter;
	float  distSq      = dot(toCamera, toCamera);

	uint lodLevel = SelectLOD(distSq);
	LODData selectedLOD = ChunkBuffer[chunkIndex].lods[lodLevel];
	if (selectedLOD.indexCount == 0) return; // skip invalid/empty LOD

	uint outIndex;
	InterlockedAdd(visibleCount[0], 1u, outIndex);

	// Write the indirect draw + LOD level. firstInstance is repurposed
	// as the lod-index slot the vertex shader reads back from
	// LODLevelBuffer (the visualization channel).
	DrawIndexedIndirectCommand cmd;
	cmd.indexCount    = selectedLOD.indexCount;
	cmd.instanceCount = 1u;
	cmd.firstIndex    = selectedLOD.firstIndex;
	cmd.vertexOffset  = (int)selectedLOD.vertexOffset;
	cmd.firstInstance = outIndex;
	IndirectCommandBuffer[outIndex] = cmd;

	LODLevelBuffer[outIndex] = lodLevel;
}
