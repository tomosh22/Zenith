#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "CityBuilder/Source/CB_RoadTerrain.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Flux/Terrain/Flux_TerrainVertexQuant.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "Flux/Flux_BackendTypes.h"
#include <string>

namespace
{
	using namespace Flux_TerrainConfig;

	int ClampI(int v, int lo, int hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }

	// Re-flatten ONE resident chunk LOD: load its baked baseline mesh, snap each
	// vertex's Y to the carved surface (the position lane is decoded, its Y rewritten
	// and re-encoded against the same authored box, so the UV/normal/tangent bytes and
	// the XZ words are preserved), regenerate the AABB, and re-upload in place.
	// Off-road vertices keep their baseline height → safe degradation.
	void CarveChunkLOD(Flux_TerrainStreamingState& xState, uint32_t uCX, uint32_t uCZ, uint32_t uIdx,
	                   uint32_t uLOD, bool bLowPrefix, const Zenith_Vector<CB_RoadTerrain::RoadSample>& xSamples)
	{
		Flux_TerrainChunkResidency& xRes = xState.m_axChunkResidency[uIdx];
		if (xRes.m_aeStates[uLOD] != Flux_TerrainLODResidencyState::RESIDENT) { return; }

		std::string strPath = xState.m_strTerrainAssetDirectory + "Render_"
		                    + (bLowPrefix ? "LOW_" : "") + std::to_string(uCX) + "_" + std::to_string(uCZ) + ZENITH_MESH_EXT;
		Flux_MeshGeometry xMesh;
		Flux_MeshGeometry::LoadFromFile(strPath.c_str(), xMesh, 0, false);
		const uint32_t uNum    = xMesh.GetNumVerts();
		const uint32_t uStride = xState.m_uVertexStride;
		if (uNum == 0 || xMesh.m_pVertexData == nullptr || uStride == 0) { return; }

		const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant();
		Zenith_AABB xAABB;
		xAABB.Reset();
		for (uint32_t i = 0; i < uNum; ++i)
		{
			u_int8* pVertex = xMesh.m_pVertexData + static_cast<size_t>(i) * uStride;
			Zenith_Maths::Vector3 xPos = Flux_ReadTerrainVertexPosition(pVertex, xQuant);
			xPos.y = CB_RoadTerrain::SurfaceHeight(xSamples, xPos.x, xPos.z);
			Flux_WriteTerrainVertexPosition(pVertex, xPos, xQuant);
			xAABB.ExpandToInclude(xPos);
		}

		const Flux_TerrainLODAllocation& xAlloc = xRes.m_axAllocations[uLOD];
		const uint32_t uAbsVertex = (uLOD == LOD_HIGH) ? (xState.m_uLowLODVertexCount + xAlloc.m_uVertexOffset)
		                                               : xAlloc.m_uVertexOffset;
		const uint64_t ulOffsetBytes = static_cast<uint64_t>(uAbsVertex) * static_cast<uint64_t>(uStride);
		const uint64_t ulSizeBytes   = static_cast<uint64_t>(uNum) * static_cast<uint64_t>(uStride);

		g_xEngine.FluxMemory().UploadBufferDataAtOffset(
			xState.m_xUnifiedVertexBuffer.GetBuffer().m_xVRAMHandle,
			xMesh.m_pVertexData, ulSizeBytes, ulOffsetBytes);

		xState.m_axChunkAABBs[uIdx] = xAABB;
	}

	// As CarveChunkLOD, but the vertex height comes straight from the heightfield
	// (the terraform tool's edited surface) instead of the road samples.
	void RefreshChunkLODFromField(Flux_TerrainStreamingState& xState, uint32_t uCX, uint32_t uCZ, uint32_t uIdx,
	                              uint32_t uLOD, bool bLowPrefix, const CB_TerrainHeightfield& xField)
	{
		Flux_TerrainChunkResidency& xRes = xState.m_axChunkResidency[uIdx];
		if (xRes.m_aeStates[uLOD] != Flux_TerrainLODResidencyState::RESIDENT) { return; }

		std::string strPath = xState.m_strTerrainAssetDirectory + "Render_"
		                    + (bLowPrefix ? "LOW_" : "") + std::to_string(uCX) + "_" + std::to_string(uCZ) + ZENITH_MESH_EXT;
		Flux_MeshGeometry xMesh;
		Flux_MeshGeometry::LoadFromFile(strPath.c_str(), xMesh, 0, false);
		const uint32_t uNum    = xMesh.GetNumVerts();
		const uint32_t uStride = xState.m_uVertexStride;
		if (uNum == 0 || xMesh.m_pVertexData == nullptr || uStride == 0) { return; }

		const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant();
		Zenith_AABB xAABB;
		xAABB.Reset();
		for (uint32_t i = 0; i < uNum; ++i)
		{
			u_int8* pVertex = xMesh.m_pVertexData + static_cast<size_t>(i) * uStride;
			Zenith_Maths::Vector3 xPos = Flux_ReadTerrainVertexPosition(pVertex, xQuant);
			// Apply ONLY the player's edit (field minus the pristine hill the field began as)
			// on top of the FINE baked vertex height. Off-edit the delta is 0, so the baked
			// 1m hill is preserved exactly → watertight seams with non-refreshed neighbours,
			// no coarse-field faceting. (Replacing the height with the 16m field caused the holes.)
			const float fDelta = xField.GetHeightAt(xPos.x, xPos.z) - CB_TerrainGen::HillFieldSample(xPos.x, xPos.z);
			xPos.y = xPos.y + fDelta;
			Flux_WriteTerrainVertexPosition(pVertex, xPos, xQuant);
			xAABB.ExpandToInclude(xPos);
		}

		const Flux_TerrainLODAllocation& xAlloc = xRes.m_axAllocations[uLOD];
		const uint32_t uAbsVertex = (uLOD == LOD_HIGH) ? (xState.m_uLowLODVertexCount + xAlloc.m_uVertexOffset)
		                                               : xAlloc.m_uVertexOffset;
		const uint64_t ulOffsetBytes = static_cast<uint64_t>(uAbsVertex) * static_cast<uint64_t>(uStride);
		const uint64_t ulSizeBytes   = static_cast<uint64_t>(uNum) * static_cast<uint64_t>(uStride);

		g_xEngine.FluxMemory().UploadBufferDataAtOffset(
			xState.m_xUnifiedVertexBuffer.GetBuffer().m_xVRAMHandle,
			xMesh.m_pVertexData, ulSizeBytes, ulOffsetBytes);

		xState.m_axChunkAABBs[uIdx] = xAABB;
	}
}

void CB_RoadTerrain::CarveTerrainMesh(const CB_RoadGraph& xGraph, Zenith_TerrainComponent* pxTerrain)
{
	if (pxTerrain == nullptr) { return; }
	Flux_TerrainStreamingState* pxState = pxTerrain->m_pxStreamingState;
	if (pxState == nullptr) { return; }

	Zenith_Vector<CB_RoadTerrain::RoadSample> xSamples;
	CB_RoadTerrain::BuildSamples(xGraph, xSamples);
	if (xSamples.GetSize() == 0u) { return; }

	// The roads' world bounding box → the chunk range to touch (the city is small).
	float fMinX = 1.0e30f, fMaxX = -1.0e30f, fMinZ = 1.0e30f, fMaxZ = -1.0e30f;
	for (uint32_t i = 0; i < xSamples.GetSize(); ++i)
	{
		const CB_RoadTerrain::RoadSample& xRS = xSamples.Get(i);
		if (xRS.x < fMinX) { fMinX = xRS.x; }
		if (xRS.x > fMaxX) { fMaxX = xRS.x; }
		if (xRS.z < fMinZ) { fMinZ = xRS.z; }
		if (xRS.z > fMaxZ) { fMaxZ = xRS.z; }
	}
	const float fM = CB_RoadTerrain::FLATTEN_RADIUS + 2.0f;
	const int iMax = static_cast<int>(CHUNK_GRID_SIZE) - 1;
	const int iCXMin = ClampI(static_cast<int>((fMinX - fM) / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCXMax = ClampI(static_cast<int>((fMaxX + fM) / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCZMin = ClampI(static_cast<int>((fMinZ - fM) / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCZMax = ClampI(static_cast<int>((fMaxZ + fM) / CHUNK_SIZE_WORLD), 0, iMax);

	// The terrain unified vertex buffer is host-visible and is READ by the GPU every frame
	// (including the frames still in flight). We're about to overwrite resident chunk verts
	// in place via memcpy — that races the in-flight GPU read and corrupts terrain. Wait for
	// the GPU to finish before touching the buffer. (Infrequent — only on a road change.)
	g_xEngine.FluxBackend().WaitForGPUIdle();

	for (int cx = iCXMin; cx <= iCXMax; ++cx)
	{
		for (int cz = iCZMin; cz <= iCZMax; ++cz)
		{
			const uint32_t uIdx = ChunkCoordsToIndex(static_cast<uint32_t>(cx), static_cast<uint32_t>(cz));
			// HIGH only. The LOW LOD is a single COMBINED mesh (not per-chunk allocated like
			// HIGH/streaming), so re-uploading it per-chunk wrote to the wrong place and
			// corrupted DISTANT chunks (speckled-garbage patches). The carve is sub-pixel at
			// LOW range anyway — leave LOW as the baked baseline.
			CarveChunkLOD(*pxState, static_cast<uint32_t>(cx), static_cast<uint32_t>(cz), uIdx, LOD_HIGH, false, xSamples);
		}
	}

	pxState->m_bChunkDataDirty.store(true, std::memory_order_release);
}

// Stream-in hook (engine calls this from StreamInLOD, main thread, after the baked chunk mesh
// loads + before the GPU upload). Re-shape the freshly-loaded HIGH chunk's verts to the live CPU
// heightfield (which holds the hills + road carve + terraform), so ALL terrain deformation
// survives a stream-out/in cycle. Applied as a DELTA on the fine baked vertex (field minus the
// pristine 16m hill the field began as): off-edit the delta is exactly 0 so the baked 1m hill is
// preserved (no coarse-field faceting / seams); on-edit (road or terraform) the offset is applied.
// The engine uploads to a deferred-safe slot, so no GPU sync is needed here. This is the single
// race-free path for both the road carve and the terraform tool.
void CB_RoadTerrain::ChunkVertexCarveHook(void* pUser, uint32_t /*uChunkX*/, uint32_t /*uChunkY*/,
                                          void* pVertexData, uint32_t uNumVerts, uint32_t uVertexStride)
{
	const CB_TerrainHeightfield* pxField = static_cast<const CB_TerrainHeightfield*>(pUser);
	if (pxField == nullptr || pVertexData == nullptr || uNumVerts == 0u || uVertexStride == 0u) { return; }

	u_int8* pBytes = static_cast<u_int8*>(pVertexData);
	const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant();
	for (uint32_t i = 0; i < uNumVerts; ++i)
	{
		// Decode the quantised position, move Y only, re-encode against the same
		// authored box; UV/normal/tangent bytes and the XZ words are preserved.
		u_int8* pVertex = pBytes + static_cast<size_t>(i) * uVertexStride;
		Zenith_Maths::Vector3 xPos = Flux_ReadTerrainVertexPosition(pVertex, xQuant);
		const float fDelta = pxField->GetHeightAt(xPos.x, xPos.z) - CB_TerrainGen::HillFieldSample(xPos.x, xPos.z);
		xPos.y = xPos.y + fDelta;
		Flux_WriteTerrainVertexPosition(pVertex, xPos, xQuant);
	}
}

void CB_RoadTerrain::RegisterStreamHook(Zenith_TerrainComponent* pxTerrain, const CB_TerrainHeightfield& xField)
{
	if (pxTerrain == nullptr) { return; }
	Flux_TerrainStreamingState* pxState = pxTerrain->m_pxStreamingState;
	if (pxState == nullptr) { return; }
	// Publish the user pointer before the function pointer (the engine null-checks the fn).
	pxState->m_pChunkVertexHookUser = const_cast<void*>(static_cast<const void*>(&xField));
	pxState->m_pfnChunkVertexHook   = &CB_RoadTerrain::ChunkVertexCarveHook;
}

void CB_RoadTerrain::UnregisterStreamHook(Zenith_TerrainComponent* pxTerrain)
{
	if (pxTerrain == nullptr) { return; }
	Flux_TerrainStreamingState* pxState = pxTerrain->m_pxStreamingState;
	if (pxState == nullptr) { return; }
	pxState->m_pfnChunkVertexHook   = nullptr;
	pxState->m_pChunkVertexHookUser = nullptr;
}

void CB_RoadTerrain::ForceRestreamCarveChunks(const CarveContext& xCtx, Zenith_TerrainComponent* pxTerrain)
{
	if (pxTerrain == nullptr || !xCtx.m_bActive) { return; }
	Flux_TerrainStreamingState* pxState = pxTerrain->m_pxStreamingState;
	if (pxState == nullptr) { return; }

	// The road world bbox → the chunk range under the roads.
	const float fM = CB_RoadTerrain::FLATTEN_RADIUS + 2.0f;
	const int iMax = static_cast<int>(CHUNK_GRID_SIZE) - 1;
	const int iCXMin = ClampI(static_cast<int>((xCtx.m_fMinX - fM) / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCXMax = ClampI(static_cast<int>((xCtx.m_fMaxX + fM) / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCZMin = ClampI(static_cast<int>((xCtx.m_fMinZ - fM) / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCZMax = ClampI(static_cast<int>((xCtx.m_fMaxZ + fM) / CHUNK_SIZE_WORLD), 0, iMax);

	// Evict each currently-resident HIGH chunk under the roads. UpdateStreamingForTerrain will
	// re-stream them (they're near the camera) and the registered hook re-carves each on reload —
	// the engine's race-free path. We do NOT write the live buffer in place here.
	for (int cx = iCXMin; cx <= iCXMax; ++cx)
	{
		for (int cz = iCZMin; cz <= iCZMax; ++cz)
		{
			const uint32_t uIdx = ChunkCoordsToIndex(static_cast<uint32_t>(cx), static_cast<uint32_t>(cz));
			if (pxState->m_axChunkResidency[uIdx].m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				g_xEngine.TerrainStreaming().EvictLOD(*pxState, uIdx, LOD_HIGH);
			}
		}
	}
}

void CB_RoadTerrain::RefreshTerrainRegionFromField(const CB_TerrainHeightfield& xField, Zenith_TerrainComponent* pxTerrain,
                                                   float fMinX, float fMaxX, float fMinZ, float fMaxZ)
{
	if (pxTerrain == nullptr) { return; }
	Flux_TerrainStreamingState* pxState = pxTerrain->m_pxStreamingState;
	if (pxState == nullptr) { return; }

	const int iMax = static_cast<int>(CHUNK_GRID_SIZE) - 1;
	const int iCXMin = ClampI(static_cast<int>(fMinX / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCXMax = ClampI(static_cast<int>(fMaxX / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCZMin = ClampI(static_cast<int>(fMinZ / CHUNK_SIZE_WORLD), 0, iMax);
	const int iCZMax = ClampI(static_cast<int>(fMaxZ / CHUNK_SIZE_WORLD), 0, iMax);

	// Same GPU sync as CarveTerrainMesh: these are resident, actively-rendered chunks, so an
	// in-place host-visible re-upload races the in-flight GPU read without this wait.
	g_xEngine.FluxBackend().WaitForGPUIdle();

	for (int cx = iCXMin; cx <= iCXMax; ++cx)
	{
		for (int cz = iCZMin; cz <= iCZMax; ++cz)
		{
			const uint32_t uIdx = ChunkCoordsToIndex(static_cast<uint32_t>(cx), static_cast<uint32_t>(cz));
			// HIGH only — see CarveTerrainMesh: the per-chunk LOW re-upload corrupts the
			// combined LOW mesh (distant speckles). Terraform shows at HIGH range; LOW keeps the baseline.
			RefreshChunkLODFromField(*pxState, static_cast<uint32_t>(cx), static_cast<uint32_t>(cz), uIdx, LOD_HIGH, false, xField);
		}
	}

	pxState->m_bChunkDataDirty.store(true, std::memory_order_release);
}
