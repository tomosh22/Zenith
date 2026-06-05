#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_TerrainGen.h"

class Zenith_TerrainComponent;

// ============================================================================
// CB_RoadTerrain — roads MODIFY the terrain (Cities: Skylines-style): each road
// levels + slightly recesses its corridor, cutting the cross-slope so the road
// (and the frontage buildings) sit on a flat bed instead of a hillside.
//
// The surface height is the single source of truth used in two places that must
// agree: the CPU heightfield (CB_TerrainHeightfield → GetHeightAt → road ribbon +
// building placement) and the GPU terrain mesh (CarveTerrainMesh re-uploads the
// resident chunk vertices). Off-road, the surface is the CB_TerrainGen hill; near
// a road it snaps to the nearest centreline's height (minus a shallow bed depth).
// ============================================================================
namespace CB_RoadTerrain
{
	constexpr float FLATTEN_RADIUS = 20.0f;   // corridor half-width that gets levelled (road + frontage)
	constexpr float BED_DEPTH      = 0.5f;    // how far the levelled corridor sits below the surrounding ground

	struct RoadSample { float x; float z; float h; };   // a centreline point + its baseline hill height

	// Sample every active road centreline (point + baseline height) for nearest-point queries.
	inline void BuildSamples(const CB_RoadGraph& xGraph, Zenith_Vector<RoadSample>& xOut)
	{
		xOut.Clear();
		const uint32_t uSegs = xGraph.GetSegmentSlotCount();
		for (uint32_t s = 0; s < uSegs; ++s)
		{
			const CB_RoadSegment& xSeg = xGraph.GetSegment(s);
			if (!xSeg.m_bActive) continue;
			const uint32_t uN = 24u;
			for (uint32_t i = 0; i <= uN; ++i)
			{
				const Zenith_Maths::Vector2 xP = xSeg.m_xSpline.Evaluate(static_cast<float>(i) / static_cast<float>(uN));
				RoadSample xRS;
				xRS.x = xP.x;
				xRS.z = xP.y;
				xRS.h = CB_TerrainGen::HillNorm(xP.x, xP.y) * CB_TerrainGen::HEIGHT_SCALE;
				xOut.PushBack(xRS);
			}
		}
	}

	// World surface height at (wx,wz): the nearest centreline's height (levelled +
	// recessed) if within FLATTEN_RADIUS, else the baseline hill.
	inline float SurfaceHeight(const Zenith_Vector<RoadSample>& xSamples, float fWX, float fWZ)
	{
		float fBestD2 = FLATTEN_RADIUS * FLATTEN_RADIUS;
		float fH      = CB_TerrainGen::HillNorm(fWX, fWZ) * CB_TerrainGen::HEIGHT_SCALE;
		bool  bHit    = false;
		for (uint32_t i = 0; i < xSamples.GetSize(); ++i)
		{
			const RoadSample& xRS = xSamples.Get(i);
			const float dx = fWX - xRS.x;
			const float dz = fWZ - xRS.z;
			const float d2 = dx * dx + dz * dz;
			if (d2 < fBestD2) { fBestD2 = d2; fH = xRS.h; bHit = true; }
		}
		return bHit ? (fH - BED_DEPTH) : fH;
	}

	// Re-shape the CPU heightfield: hills everywhere, levelled under roads. So
	// GetHeightAt (road ribbon + building lots) follows the carved surface.
	inline void FlattenHeightfield(const CB_RoadGraph& xGraph, CB_TerrainHeightfield& xField)
	{
		Zenith_Vector<RoadSample> xSamples;
		BuildSamples(xGraph, xSamples);
		const float fInvScale = 1.0f / CB_TerrainGen::HEIGHT_SCALE;
		const uint32_t uSX = xField.GetSamplesX();
		const uint32_t uSZ = xField.GetSamplesZ();
		for (uint32_t uZ = 0; uZ < uSZ; ++uZ)
		{
			for (uint32_t uX = 0; uX < uSX; ++uX)
			{
				const float fY = SurfaceHeight(xSamples, static_cast<float>(uX) * 16.0f, static_cast<float>(uZ) * 16.0f);
				xField.SetNormalized(uX, uZ, fY * fInvScale);
			}
		}
	}

	// Persistent carve context: the road centreline samples + their world bbox. Rebuilt on a
	// road change (RebuildCarveContext) and referenced by the stream-in hook so a HIGH chunk
	// that streams in AFTER the road was placed still gets carved (the deformation survives
	// streaming, replacing the old re-carve countdown). Owned by the manager (it outlives the
	// registration; the engine holds a raw pointer to it).
	struct CarveContext
	{
		Zenith_Vector<RoadSample> m_xSamples;
		float m_fMinX = 0.0f, m_fMaxX = 0.0f, m_fMinZ = 0.0f, m_fMaxZ = 0.0f;  // road world bbox
		bool  m_bActive = false;                                               // any active roads?
	};

	inline void RebuildCarveContext(const CB_RoadGraph& xGraph, CarveContext& xCtx)
	{
		BuildSamples(xGraph, xCtx.m_xSamples);
		xCtx.m_bActive = (xCtx.m_xSamples.GetSize() > 0u);
		if (!xCtx.m_bActive) { return; }
		float fMnX = 1.0e30f, fMxX = -1.0e30f, fMnZ = 1.0e30f, fMxZ = -1.0e30f;
		for (uint32_t i = 0; i < xCtx.m_xSamples.GetSize(); ++i)
		{
			const RoadSample& xRS = xCtx.m_xSamples.Get(i);
			if (xRS.x < fMnX) { fMnX = xRS.x; }
			if (xRS.x > fMxX) { fMxX = xRS.x; }
			if (xRS.z < fMnZ) { fMnZ = xRS.z; }
			if (xRS.z > fMxZ) { fMxZ = xRS.z; }
		}
		xCtx.m_fMinX = fMnX; xCtx.m_fMaxX = fMxX; xCtx.m_fMinZ = fMnZ; xCtx.m_fMaxZ = fMxZ;
	}

	// Stream-in hook (matches the engine Flux_TerrainStreamingState::ChunkVertexHook signature):
	// re-shape a freshly-streamed HIGH chunk's verts to the live CPU heightfield (hills + road carve
	// + terraform) as a delta on the fine baked vertex, so all terrain deformation survives a
	// stream-out/in cycle. pUser is a const CB_TerrainHeightfield*. Defined in CB_RoadTerrain.cpp;
	// runs on the main thread (streaming is main-thread).
	void ChunkVertexCarveHook(void* pUser, uint32_t uChunkX, uint32_t uChunkY,
	                          void* pVertexData, uint32_t uNumVerts, uint32_t uVertexStride);

	// Register / clear the stream-in hook on a terrain's streaming state. The hook reads the live
	// heightfield (the engine holds a raw pointer — it must outlive the registration; unregister in
	// OnDestroy). Idempotent. Defined in the .cpp.
	void RegisterStreamHook(Zenith_TerrainComponent* pxTerrain, const CB_TerrainHeightfield& xField);
	void UnregisterStreamHook(Zenith_TerrainComponent* pxTerrain);

	// Force the resident HIGH chunks under the roads to re-stream (evict their HIGH LOD) so the
	// stream-in hook re-carves them on reload. This is the RACE-FREE way to deform already-resident
	// chunks: the engine's evict zeroes the chunk-data so the chunk is not drawn during the
	// evict->re-stream gap, and the reload uploads through the streaming path (never an in-place
	// write to a live, GPU-read chunk — which corrupts distant terrain even behind waitIdle).
	// Call after RegisterStreamHook on a road change. Defined in the .cpp.
	void ForceRestreamCarveChunks(const CarveContext& xCtx, Zenith_TerrainComponent* pxTerrain);

	// GPU: re-upload the resident terrain chunks under roads with the carved surface
	// (windowed only; defined in CB_RoadTerrain.cpp). Idempotent — regenerates each
	// affected chunk from its baked baseline + the current roads. Waits for the GPU before
	// writing (resident chunks are actively rendered — see CarveTerrainMesh's sync comment).
	void CarveTerrainMesh(const CB_RoadGraph& xGraph, Zenith_TerrainComponent* pxTerrain);

	// GPU: re-upload the resident terrain chunks overlapping a world rect so they match
	// the CPU heightfield directly (the terraform tool edits the field then calls this).
	// Windowed only; defined in CB_RoadTerrain.cpp. Same re-upload path as CarveTerrainMesh,
	// but the per-vertex height comes from xField.GetHeightAt (raw terraformed surface).
	void RefreshTerrainRegionFromField(const CB_TerrainHeightfield& xField, Zenith_TerrainComponent* pxTerrain,
	                                   float fMinX, float fMaxX, float fMinZ, float fMaxZ);
}
