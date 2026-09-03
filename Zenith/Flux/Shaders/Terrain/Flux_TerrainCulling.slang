// =====================================================================
// Flux_TerrainCulling — Slang module.
//
// Subsystem: Terrain. GPU-driven terrain chunk culling + 2-level LOD
// selection, for the CAMERA and for every active SHADOW CASCADE in one
// dispatch. Thread (x, y): x is the chunk slot, y the view row —
//   y == 0            the camera: AABB-vs-camera-frustum, distance LOD with
//                     hysteresis, atomic compaction into the camera indirect
//                     buffer, LODLevelBuffer write.
//   y == 1 + cascade  a shadow cascade: AABB-vs-that-cascade's light-space
//                     ortho box, the SAME camera-resolved LOD (so the caster is
//                     the receiver's geometry) unless the cascade is at/above the
//                     force-LOW threshold, atomic compaction into that cascade's
//                     SLOT of the shadow indirect buffer. Never touches
//                     LODLevelBuffer — that is the camera's hysteresis state.
//
// The rows above the active cascade count early-out, so an inactive cascade's
// slot keeps the all-zero state the reset pass wrote and its DrawIndexedIndirect-
// Count draws nothing. The slot layout (cascade-major, one full chunk-capacity
// range per cascade) is Flux_TerrainShadowCull.h's; the C++ static_asserts in
// Flux_Terrain.cpp pin TERRAIN_SHADOW_CULL_VIEWS and uFLUX_TERRAIN_TOTAL_CHUNKS
// to their C++ twins.
//
// LOD policy: HIGH (0) when distance² < 1e6 (~1 km), LOW (1) otherwise.
// Sorting is intentionally omitted — modern GPU early-Z handles a few
// percent of depth overdraw far cheaper than the O(n²) sort would.
//
// Spine include reserves sets 0/1/2 (GLOBAL/VIEW/BINDLESS) so this pass's own
// resources land in PassParams at set 3. This compute pass carries its own
// frustum/camera data in CameraBuffer / ShadowCullBuffer (not the VIEW set:
// one dispatch serves five views) but still includes the spine so the
// persistent set layout stays uniform across shaders.
// =====================================================================

#include "Common/Bindings.slang"

// Shared 20-byte indexed-indirect-command ABI (was declared inline here;
// moved to a terrain-local include so reset + culling share one definition
// pinned to the same C++ twin in Flux/Backend/Flux_IndirectDraw.h).
#include "Terrain/Flux_TerrainIndirectCommon.slang"

static const uint  TERRAIN_LOD_COUNT      = 2;
static const uint  TERRAIN_LOD_LOW        = 1;
static const float LOD_HIGH_MAX_DIST_SQ   = 1000000.0;
// One shadow view row per cascade. Mirrors uFLUX_TERRAIN_SHADOW_CULL_VIEWS
// (Flux_TerrainShadowCull.h) == ZENITH_FLUX_NUM_CSMS; pinned in Flux_Terrain.cpp.
static const uint  TERRAIN_SHADOW_CULL_VIEWS = 4;

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

struct FrustumPlane { float4 normalAndDistance; };

struct CameraBufferLayout
{
	FrustumPlane frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
	float4       cameraPosition;
};

// Mirrors Zenith_TerrainShadowCullGPU (Flux_TerrainGPUStructs.h). Cascade-major:
// cascade c's planes are frustumPlanes[c*6 .. c*6+5], same order as the camera's,
// extracted by the same CPU helper from the cascade's snapped ortho view-proj.
struct ShadowCullBufferLayout
{
	FrustumPlane frustumPlanes[TERRAIN_SHADOW_CULL_VIEWS * 6];
	uint4        params;   // x = active cascade count, y = force-LOW-LOD from this cascade index, zw = 0
};

struct PassParams
{
	StructuredBuffer<TerrainChunkData>            ChunkBuffer;
	ConstantBuffer<CameraBufferLayout>            CameraBuffer;
	RWStructuredBuffer<Flux_TerrainIndexedIndirectCommand> IndirectCommandBuffer;
	RWStructuredBuffer<uint>                       visibleCount;
	RWStructuredBuffer<uint>                       LODLevelBuffer;
	// Shadow-cascade slots, appended AFTER the camera set so bindings 0-4 keep
	// their numbers. ShadowIndirectCommandBuffer holds TERRAIN_SHADOW_CULL_VIEWS
	// consecutive full-capacity slots; ShadowVisibleCount one uint per cascade.
	ConstantBuffer<ShadowCullBufferLayout>         ShadowCullBuffer;
	RWStructuredBuffer<Flux_TerrainIndexedIndirectCommand> ShadowIndirectCommandBuffer;
	RWStructuredBuffer<uint>                       ShadowVisibleCount;
};
ParameterBlock<PassParams> g_xPassSet;

// Conservative AABB-vs-frustum test (Akenine-Möller style) against view row
// `view`'s six planes: row 0 reads the camera block, row 1+c cascade c's slice
// of the shadow block. Tests near/far first since distant terrain is the
// most-likely cull case. Valid for the cascades' ORTHO boxes too — Gribb-
// Hartmann extraction does not care about the projection kind.
FrustumPlane LoadPlane(uint view, uint plane)
{
	if (view == 0u)
	{
		return g_xPassSet.CameraBuffer.frustumPlanes[plane];
	}
	return g_xPassSet.ShadowCullBuffer.frustumPlanes[(view - 1u) * 6u + plane];
}

bool TestAABBFrustum(uint view, float3 aabbMin, float3 aabbMax)
{
	float3 center  = (aabbMin + aabbMax) * 0.5;
	float3 extents = (aabbMax - aabbMin) * 0.5;

	const int kPlaneOrder[6] = { 4, 5, 0, 1, 2, 3 };
	for (int i = 0; i < 6; i++)
	{
		FrustumPlane p     = LoadPlane(view, (uint)kPlaneOrder[i]);
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
	uint view       = dispatchId.y;   // 0 = camera, 1 + c = shadow cascade c

	uint chunkCount, stride;
	g_xPassSet.ChunkBuffer.GetDimensions(chunkCount, stride);
	if (chunkIndex >= chunkCount) return;

	// Inactive cascade rows (shadows off, or fewer cascades staged) do nothing,
	// leaving the slot at the reset pass's all-zero state.
	if (view != 0u && (view - 1u) >= g_xPassSet.ShadowCullBuffer.params.x) return;

	float4 aabbMin = g_xPassSet.ChunkBuffer[chunkIndex].aabbMin;
	float4 aabbMax = g_xPassSet.ChunkBuffer[chunkIndex].aabbMax;
	if (!TestAABBFrustum(view, aabbMin.xyz, aabbMax.xyz)) return;

	// LOD is a CAMERA-distance decision for every row: the shadow caster should be
	// the same surface the G-buffer receiver is, or the receiver self-shadows
	// wherever it dips below a coarser caster (acne no bias can fix).
	float3 chunkCenter = (aabbMin.xyz + aabbMax.xyz) * 0.5;
	float3 toCamera    = g_xPassSet.CameraBuffer.cameraPosition.xyz - chunkCenter;
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
	//
	// The shadow rows read priorLOD too, racing the camera row's write of the
	// new value. That race is benign BY CONSTRUCTION: this decision is a fixed
	// point of its own output (feeding the freshly-written LOD back in as
	// "prior" yields the same LOD for the same distance), so a shadow row
	// observing either the old or the new value resolves the same actualLOD the
	// camera row did. Only the camera row WRITES.
	uint priorLOD     = g_xPassSet.LODLevelBuffer[chunkIndex];
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
	LODData selectedLOD = g_xPassSet.ChunkBuffer[chunkIndex].lods[actualLOD];

	if (selectedLOD.indexCount == 0u && requestedLOD == 0u)
	{
		// HIGH unavailable — fall back to LOW (always-resident).
		actualLOD = 1u;
		selectedLOD = g_xPassSet.ChunkBuffer[chunkIndex].lods[actualLOD];
	}

	// Shadow rows at/above the force-LOW cascade cast the always-resident LOW
	// mesh regardless of camera distance (Flux_TerrainShadowCasterLOD is the
	// CPU twin of this branch). The far cascade is where a caster/receiver LOD
	// mismatch is cheapest to hide and HIGH is most expensive to draw.
	if (view != 0u && (view - 1u) >= g_xPassSet.ShadowCullBuffer.params.y)
	{
		actualLOD = TERRAIN_LOD_LOW;
		selectedLOD = g_xPassSet.ChunkBuffer[chunkIndex].lods[actualLOD];
	}

	if (selectedLOD.indexCount == 0u) return; // both LODs empty → skip

	// The indirect draw + (camera only) LOD level. firstInstance is the stable
	// chunk id, so the vertex shader indexes LODLevelBuffer by chunk rather
	// than by compacted draw slot. That keeps debug colour stable even when
	// draw compaction order changes frame-to-frame.
	Flux_TerrainIndexedIndirectCommand cmd;
	cmd.indexCount    = selectedLOD.indexCount;
	cmd.instanceCount = 1u;
	cmd.firstIndex    = selectedLOD.firstIndex;
	cmd.vertexOffset  = (int)selectedLOD.vertexOffset;
	cmd.firstInstance = chunkIndex;

	uint outIndex;
	if (view == 0u)
	{
		InterlockedAdd(g_xPassSet.visibleCount[0], 1u, outIndex);
		g_xPassSet.IndirectCommandBuffer[outIndex] = cmd;
		g_xPassSet.LODLevelBuffer[chunkIndex] = actualLOD;
	}
	else
	{
		// Cascade c's slot is the c-th full chunk-capacity range of the shadow
		// buffer (uFLUX_TERRAIN_TOTAL_CHUNKS records, pinned to TOTAL_CHUNKS in
		// Flux_Terrain.cpp) — one contiguous range per cascade so each cascade
		// pass draws with two byte offsets and nothing else changes.
		uint cascade = view - 1u;
		InterlockedAdd(g_xPassSet.ShadowVisibleCount[cascade], 1u, outIndex);
		g_xPassSet.ShadowIndirectCommandBuffer[cascade * uFLUX_TERRAIN_TOTAL_CHUNKS + outIndex] = cmd;
	}
}
