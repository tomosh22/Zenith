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

	// GPU-side LOD-selection hysteresis. SelectLOD's bare comparison
	//   distSq >= LOD_HIGH_MAX_DIST_SQ
	// has zero hysteresis — float-precision drift in dot(toCamera,...) for
	// chunks at the threshold radius (~1 km) flips the result frame-to-frame
	// and produces visible red↔green flicker in the LOD-debug visualisation
	// even when streaming is steady-state stable. Read the prior frame's
	// LOD as a stickiness bias inside a ±1% band around the threshold; CPU
	// streaming guarantees HIGH is still resident within 1.5× of threshold
	// (eviction hysteresis), so binding to the prior LOD inside this much
	// narrower band is safe — the chunk is guaranteed to have valid HIGH
	// geometry if priorLOD says it did. First-frame value is zero (HIGH)
	// from the nullptr-initialised buffer; converges within one frame.
	uint priorLOD     = LODLevelBuffer[chunkIndex];
	uint requestedLOD = SelectLOD(distSq);
	const float fHysteresisLow  = LOD_HIGH_MAX_DIST_SQ * 0.99;
	const float fHysteresisHigh = LOD_HIGH_MAX_DIST_SQ * 1.01;
	if (priorLOD == 0u && distSq < fHysteresisHigh) requestedLOD = 0u;
	if (priorLOD == 1u && distSq > fHysteresisLow ) requestedLOD = 1u;

	// Explicit LOD fallback. The CPU-side BuildChunkDataForGPU leaves
	// non-resident LOD slots at indexCount==0 (no longer falsifies HIGH
	// with LOW's allocation), so when the camera asks for HIGH but it
	// hasn't streamed in yet we drop to LOW here and record the *actual*
	// LOD that drew. That keeps the LODLevelBuffer debug visualisation
	// truthful: red/green colour matches the geometry vertex density.
	uint actualLOD    = requestedLOD;
	LODData selectedLOD = ChunkBuffer[chunkIndex].lods[actualLOD];

	if (selectedLOD.indexCount == 0u && requestedLOD == 0u)
	{
		// HIGH unavailable — fall back to LOW (always-resident).
		actualLOD = 1u;
		selectedLOD = ChunkBuffer[chunkIndex].lods[actualLOD];
	}

	if (selectedLOD.indexCount == 0u) return; // both LODs empty → skip

	uint outIndex;
	InterlockedAdd(visibleCount[0], 1u, outIndex);

	// Write the indirect draw + LOD level. firstInstance is the stable
	// chunk id, so the vertex shader indexes LODLevelBuffer by chunk rather
	// than by compacted draw slot. That keeps debug colour stable even when
	// draw compaction order changes frame-to-frame.
	DrawIndexedIndirectCommand cmd;
	cmd.indexCount    = selectedLOD.indexCount;
	cmd.instanceCount = 1u;
	cmd.firstIndex    = selectedLOD.firstIndex;
	cmd.vertexOffset  = (int)selectedLOD.vertexOffset;
	cmd.firstInstance = chunkIndex;
	IndirectCommandBuffer[outIndex] = cmd;

	LODLevelBuffer[chunkIndex] = actualLOD;
}
