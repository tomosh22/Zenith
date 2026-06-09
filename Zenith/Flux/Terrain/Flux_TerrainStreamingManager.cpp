#include "Zenith.h"
#include "Flux/Flux_RendererImpl.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "DebugVariables/Zenith_DebugVariables.h"
// Wave 3: needed by the relocated per-frame culling/LOD drive (UpdateCullingAndLod records
// compute commands; UploadFrustumPlanesForFrame extracts the camera frustum).
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_GraphicsImpl.h" // FluxGraphics().GetCameraPosition() in UploadFrustumPlanesForFrame
#include "Maths/Zenith_FrustumCulling.h"
#include <algorithm>
#include <fstream>

using namespace Flux_TerrainConfig;

// ========== Debug Logging ==========
DEBUGVAR bool dbg_bLogTerrainStreaming = false;

// ========== Static Member Definitions ==========

// ========== Buffer Allocator Implementation ==========
// Best-fit allocator with coalescing. Free list sorted by offset.

Flux_TerrainBufferAllocator::Flux_TerrainBufferAllocator()
	: m_uTotalSize(0)
	, m_uUnusedSpace(0)
	, m_szDebugName("Unknown")
{
}

void Flux_TerrainBufferAllocator::Initialize(uint32_t uTotalSize, const char* szDebugName)
{
	m_uTotalSize = uTotalSize;
	m_uUnusedSpace = uTotalSize;
	m_szDebugName = szDebugName;

	m_axFreeBlocks.Clear();

	FreeBlock xInitialBlock;
	xInitialBlock.m_uOffset = 0;
	xInitialBlock.m_uSize = uTotalSize;
	m_axFreeBlocks.PushBack(xInitialBlock);
}

void Flux_TerrainBufferAllocator::Reset()
{
	m_axFreeBlocks.Clear();
	m_uTotalSize = 0;
	m_uUnusedSpace = 0;
}

uint32_t Flux_TerrainBufferAllocator::Allocate(uint32_t uSize)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_ALLOCATE);
	if (uSize == 0 || uSize > m_uUnusedSpace)
		return UINT32_MAX;

	// Best-fit: find smallest block that fits
	uint32_t uBestIdx = UINT32_MAX;
	uint32_t uBestSize = UINT32_MAX;

	for (uint32_t i = 0; i < static_cast<uint32_t>(m_axFreeBlocks.GetSize()); ++i)
	{
		if (m_axFreeBlocks.Get(i).m_uSize >= uSize && m_axFreeBlocks.Get(i).m_uSize < uBestSize)
		{
			uBestIdx = i;
			uBestSize = m_axFreeBlocks.Get(i).m_uSize;

			// Exact fit - can't do better
			if (uBestSize == uSize)
				break;
		}
	}

	if (uBestIdx == UINT32_MAX)
		return UINT32_MAX;

	uint32_t uAllocatedOffset = m_axFreeBlocks.Get(uBestIdx).m_uOffset;
	uint32_t uRemainder = m_axFreeBlocks.Get(uBestIdx).m_uSize - uSize;

	if (uRemainder > 0)
	{
		// Shrink the block in-place (keep sorted order since offset increases)
		m_axFreeBlocks.Get(uBestIdx).m_uOffset += uSize;
		m_axFreeBlocks.Get(uBestIdx).m_uSize = uRemainder;
	}
	else
	{
		// Exact fit - remove the block (order-preserving: free list stays sorted by offset)
		m_axFreeBlocks.Remove(uBestIdx);
	}

	m_uUnusedSpace -= uSize;
	return uAllocatedOffset;
}

void Flux_TerrainBufferAllocator::Free(uint32_t uOffset, uint32_t uSize)
{
	if (uSize == 0)
		return;

	m_uUnusedSpace += uSize;

	// Binary search for insertion point (list is sorted by offset)
	uint32_t uInsertIdx = 0;
	{
		uint32_t uLow = 0;
		uint32_t uHigh = static_cast<uint32_t>(m_axFreeBlocks.GetSize());
		while (uLow < uHigh)
		{
			uint32_t uMid = (uLow + uHigh) / 2;
			if (m_axFreeBlocks.Get(uMid).m_uOffset < uOffset)
				uLow = uMid + 1;
			else
				uHigh = uMid;
		}
		uInsertIdx = uLow;
	}

	// Insert the new free block at uInsertIdx, preserving the offset-sorted order.
	// Zenith_Vector has no insert-at-position, so grow by one (PushBack) then shift
	// the tail right by one slot to open a gap at uInsertIdx.
	FreeBlock xNewBlock;
	xNewBlock.m_uOffset = uOffset;
	xNewBlock.m_uSize = uSize;
	const uint32_t uOldCount = static_cast<uint32_t>(m_axFreeBlocks.GetSize());
	m_axFreeBlocks.PushBack(xNewBlock); // size now uOldCount + 1; placeholder slot at the end
	for (uint32_t i = uOldCount; i > uInsertIdx; --i)
	{
		m_axFreeBlocks.Get(i) = m_axFreeBlocks.Get(i - 1);
	}
	m_axFreeBlocks.Get(uInsertIdx) = xNewBlock;

	// Coalesce with right neighbor
	if (uInsertIdx + 1 < static_cast<uint32_t>(m_axFreeBlocks.GetSize()))
	{
		FreeBlock& xCurrent = m_axFreeBlocks.Get(uInsertIdx);
		FreeBlock& xRight = m_axFreeBlocks.Get(uInsertIdx + 1);
		if (xCurrent.m_uOffset + xCurrent.m_uSize == xRight.m_uOffset)
		{
			xCurrent.m_uSize += xRight.m_uSize;
			m_axFreeBlocks.Remove(uInsertIdx + 1);
		}
	}

	// Coalesce with left neighbor
	if (uInsertIdx > 0)
	{
		FreeBlock& xLeft = m_axFreeBlocks.Get(uInsertIdx - 1);
		FreeBlock& xCurrent = m_axFreeBlocks.Get(uInsertIdx);
		if (xLeft.m_uOffset + xLeft.m_uSize == xCurrent.m_uOffset)
		{
			xLeft.m_uSize += xCurrent.m_uSize;
			m_axFreeBlocks.Remove(uInsertIdx);
		}
	}
}

// ========== Streaming State Implementation ==========

void Flux_TerrainStreamingState::Initialize()
{

	// Pre-allocate chunk data scratch buffer for GPU upload.
	if (!m_pxCachedChunkData)
		m_pxCachedChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];

	m_xActiveChunkIndices.Clear();
	m_xActiveChunkIndices.Reserve(1024);

	m_xLastCameraPos    = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	m_iLastCameraChunkX = INT32_MIN;
	m_iLastCameraChunkY = INT32_MIN;
	m_uCurrentFrame     = 0;
	m_bChunkDataDirty.store(true, std::memory_order_release);

	m_uCachedHighLODVertexCount = 0;
	m_uCachedHighLODIndexCount  = 0;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
			m_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;

	m_bAABBsCached = false;

	m_xStats = Flux_TerrainStreamingStats();
	m_xStats.m_uVertexBufferTotalMB = STREAMING_VERTEX_BUFFER_SIZE_MB;
	m_xStats.m_uIndexBufferTotalMB  = STREAMING_INDEX_BUFFER_SIZE_MB;

	m_bRegistered = true; // live/owned (was: m_pxOwner = pxOwner)
}

void Flux_TerrainStreamingState::Shutdown()
{

	if (m_pxCachedChunkData)
	{
		delete[] m_pxCachedChunkData;
		m_pxCachedChunkData = nullptr;
	}

	m_xActiveChunkIndices.Clear();
	m_xVertexAllocator.Reset();
	m_xIndexAllocator.Reset();
	m_bAABBsCached = false;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
			m_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;

	m_bRegistered = false; // (was: m_pxOwner = nullptr)
}

// ========== Streaming Manager Implementation ==========
//
// Mini-Checkpoint B: the manager no longer owns any streaming state.
// Each Zenith_TerrainComponent allocates and owns its own
// Flux_TerrainStreamingState; the manager keeps a non-owning registry
// of those states plus a "primary" pointer for legacy single-terrain
// public APIs that don't take a component argument.

void Flux_TerrainStreamingManagerImpl::Initialize()
{
	if (m_bInitialized)
		return;

	// Self-wire cross-subsystem deps once (see header note: Initialize is no-arg
	// and called from Zenith_TerrainComponent, not the composition root, so it
	// can't take injected params). The *Impl objects are all allocated up-front
	// in Zenith_Engine::Initialise, so storing the pointer here is safe even if
	// FluxRenderer hasn't finished its own init yet.
	m_pxFluxRenderer = &g_xEngine.FluxRenderer();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManagerImpl::Initialize()");

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Log Streaming" }, dbg_bLogTerrainStreaming);
#endif

	m_bInitialized = true;
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManager initialized");
}

void Flux_TerrainStreamingManagerImpl::Shutdown()
{
	if (!m_bInitialized)
		return;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManagerImpl::Shutdown()");

	m_xRegistryMutex.Lock();
	// All terrain components must have been destroyed (and unregistered)
	// before the manager shuts down. A non-empty registry means a leak —
	// component destructor not running, or unregister wired to the wrong
	// state. Loud failure is better than dangling pointers.
	Zenith_Assert(m_xRegistry.GetSize() == 0,
		"Flux_TerrainStreamingManagerImpl::Shutdown: %u terrain(s) still registered — destroy components before shutting down the manager",
		m_xRegistry.GetSize());
	m_xRegistry.Clear();
	m_xRegistryMutex.Unlock();

	m_pxFluxRenderer = nullptr;
	m_bInitialized = false;
}

void Flux_TerrainStreamingManagerImpl::RegisterTerrainBuffers(Flux_TerrainStreamingState* pxState, const Flux_TerrainChunkInitData* pxChunkInitData)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManagerImpl::RegisterTerrainBuffers()");

	Zenith_Assert(pxState != nullptr,
		"RegisterTerrainBuffers: null streaming state — constructor should have allocated one");

	Flux_TerrainStreamingState& xState = *pxState;
	xState.m_bRegistered = true; // (was: xState.m_pxOwner = pxTerrainComponent)

	m_xRegistryMutex.Lock();
	// Idempotent: re-registering an already-registered terrain refreshes its
	// allocators / AABBs (e.g. terrain regenerate path) without double-pushing.
	bool bAlreadyRegistered = false;
	for (u_int u = 0; u < m_xRegistry.GetSize(); u++)
	{
		if (m_xRegistry.Get(u) == &xState) { bAlreadyRegistered = true; break; }
	}
	if (!bAlreadyRegistered)
	{
		m_xRegistry.PushBack(&xState);
	}
	m_xRegistryMutex.Unlock();

	// Initialize allocators for the streaming region (after LOW LOD). The
	// vertex stride now lives on the state itself (Wave-18 relocation).
	const uint32_t uMaxStreamingVertices = static_cast<uint32_t>(STREAMING_VERTEX_BUFFER_SIZE / xState.m_uVertexStride);
	const uint32_t uMaxStreamingIndices  = static_cast<uint32_t>(STREAMING_INDEX_BUFFER_SIZE / sizeof(uint32_t));

	xState.m_xVertexAllocator.Initialize(uMaxStreamingVertices, "StreamingVertices");
	xState.m_xIndexAllocator .Initialize(uMaxStreamingIndices,  "StreamingIndices");

	// Initialize LOW LOD as RESIDENT for all chunks using pre-computed data
	// (vertex/index counts and AABBs were collected during LOW LOD loading
	// in InitializeRenderResources).
	uint32_t uCurrentLowLODVertexOffset = 0;
	uint32_t uCurrentLowLODIndexOffset  = 0;

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];
			const Flux_TerrainChunkInitData& xInitData = pxChunkInitData[uChunkIndex];

			// Mark LOW LOD as resident with its allocation info
			xResidency.m_aeStates[LOD_LOW] = Flux_TerrainLODResidencyState::RESIDENT;
			xResidency.m_axAllocations[LOD_LOW].m_uVertexOffset = uCurrentLowLODVertexOffset;
			xResidency.m_axAllocations[LOD_LOW].m_uVertexCount  = xInitData.m_uVertexCount;
			xResidency.m_axAllocations[LOD_LOW].m_uIndexOffset  = uCurrentLowLODIndexOffset;
			xResidency.m_axAllocations[LOD_LOW].m_uIndexCount   = xInitData.m_uIndexCount;

			uCurrentLowLODVertexOffset += xInitData.m_uVertexCount;
			uCurrentLowLODIndexOffset  += xInitData.m_uIndexCount;

			// HIGH LOD starts as NOT_LOADED
			xResidency.m_aeStates[LOD_HIGH] = Flux_TerrainLODResidencyState::NOT_LOADED;

			// Cache AABB from pre-computed data
			xState.m_axChunkAABBs[uChunkIndex] = xInitData.m_xAABB;
		}
	}
	xState.m_bAABBsCached = true;

	// New terrain GPU buffers are about to feed into the render graph; the
	// next graph compile must rebuild SetupRenderGraph so the per-component
	// Read/Write declarations pick up this terrain's buffers.
	m_pxFluxRenderer->RequestGraphRebuild();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain buffers registered: LOW LOD resident for all %u chunks (zero redundant file reads)", TOTAL_CHUNKS);
}

void Flux_TerrainStreamingManagerImpl::UnregisterTerrainBuffers(Flux_TerrainStreamingState* pxState)
{
	if (pxState == nullptr) return;


	m_xRegistryMutex.Lock();
	// Remove the component's state from the registry (only — never touch
	// other states; that was the cross-terrain-clearing bug fixed by this
	// refactor).
	for (u_int u = 0; u < m_xRegistry.GetSize(); u++)
	{
		if (m_xRegistry.Get(u) == pxState)
		{
			m_xRegistry.RemoveSwap(u);
			break;
		}
	}
	m_xRegistryMutex.Unlock();

	// Reset only this component's state. The component's destructor calls
	// state Shutdown / delete after returning from here.
	pxState->m_xVertexAllocator.Reset();
	pxState->m_xIndexAllocator.Reset();
	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
			pxState->m_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;
	pxState->m_bAABBsCached = false;
	pxState->m_bRegistered = false; // (was: pxState->m_pxOwner = nullptr)

	// Terrain GPU buffers are about to be queued for deferred deletion; the
	// next graph compile must drop their references from SetupRenderGraph.
	m_pxFluxRenderer->RequestGraphRebuild();
}

// --- Component-aware GPU-data overloads ---------------------------------

bool Flux_TerrainStreamingManagerImpl::IsChunkDataDirty(const Flux_TerrainStreamingState* pxState)
{
	if (!m_bInitialized || pxState == nullptr) return false;
	return pxState->m_bChunkDataDirty.load(std::memory_order_acquire);
}

void Flux_TerrainStreamingManagerImpl::ClearChunkDataDirty(Flux_TerrainStreamingState* pxState)
{
	if (pxState == nullptr) return;
	pxState->m_bChunkDataDirty.store(false, std::memory_order_release);
}

Zenith_TerrainChunkData* Flux_TerrainStreamingManagerImpl::GetCachedChunkDataBuffer(const Flux_TerrainStreamingState* pxState)
{
	if (pxState == nullptr) return nullptr;
	return pxState->m_pxCachedChunkData;
}

// For each active chunk: determine desired LOD; if HIGH-LOD is wanted but not
// resident, stream it in. Bound by MAX_UPLOADS_PER_FRAME and a 2× attempt cap
// (the latter prevents excessive disk I/O when many chunks need loading).
// MissingOrInvalidSource is non-fatal — the loop skips that chunk and
// continues. Only a real AllocationFailure (allocator full even after
// eviction) stops the loop for the frame.
void Flux_TerrainStreamingManagerImpl::RequestNearbyHighLOD(Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos, StreamingFrameDiagnostics& xDiagnostics)
{
	uint32_t uStreamsThisFrame        = 0;
	uint32_t uStreamAttemptsThisFrame = 0;
	bool     bAllocationFailed        = false;

	xDiagnostics.m_uActiveCount = static_cast<uint32_t>(xState.m_xActiveChunkIndices.GetSize());

	for (uint32_t uActiveIdx = 0; uActiveIdx < xState.m_xActiveChunkIndices.GetSize(); ++uActiveIdx)
	{
		if (uStreamsThisFrame >= MAX_UPLOADS_PER_FRAME || bAllocationFailed) break;
		if (uStreamAttemptsThisFrame >= MAX_UPLOADS_PER_FRAME * 2) break;

		const uint32_t uChunkIndex = xState.m_xActiveChunkIndices.Get(uActiveIdx);
		const float    fDistanceSq = GetChunkDistanceSq(xState, uChunkIndex, xCameraPos);
		const uint32_t uDesiredLOD = CalculateDesiredLOD(fDistanceSq);
		Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];

		if (uDesiredLOD != LOD_HIGH) continue;

		xDiagnostics.m_uDesiredHighCount++;
		if (xResidency.m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
		{
			xDiagnostics.m_uAlreadyResidentHighCount++;
			continue;
		}

		uStreamAttemptsThisFrame++;
		xDiagnostics.m_uStreamAttempts++;
		const Flux_TerrainStreamInResult eRes = StreamInLOD(xState, uChunkIndex, LOD_HIGH);
		if (eRes == Flux_TerrainStreamInResult::Success)
		{
			uStreamsThisFrame++;
			xDiagnostics.m_uStreamSuccesses++;
			xState.m_xStats.m_uStreamsThisFrame++;
			xState.m_bChunkDataDirty.store(true, std::memory_order_release);

			if (dbg_bLogTerrainStreaming)
			{
				uint32_t uChunkX, uChunkY;
				ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);
				Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Streamed in chunk (%u,%u) HIGH", uChunkX, uChunkY);
			}
		}
		else if (eRes == Flux_TerrainStreamInResult::MissingOrInvalidSource)
		{
			xDiagnostics.m_uMissingSourceCount++;
			// Skip this chunk and keep going. Don't increment uStreamsThisFrame
			// (no GPU upload happened) and don't flip bAllocationFailed (the
			// allocator is fine; the source is just missing/invalid). The
			// uStreamAttemptsThisFrame cap above still guards against
			// pathological "every chunk missing" disk-thrash.
			if (dbg_bLogTerrainStreaming)
			{
				uint32_t uChunkX, uChunkY;
				ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);
				Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Skipped chunk (%u,%u) HIGH — source missing or invalid", uChunkX, uChunkY);
			}
		}
		else  // AllocationFailure
		{
			xDiagnostics.m_uAllocationFailureCount++;
			bAllocationFailed = true;
			if (dbg_bLogTerrainStreaming)
			{
				uint32_t uChunkX, uChunkY;
				ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);
				Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Allocation failed for chunk (%u,%u), stopping this frame", uChunkX, uChunkY);
			}
		}
	}
}

// Evict HIGH LOD chunks beyond the eviction threshold (HIGH range × hysteresis).
// LOW LOD is always resident so eviction only applies to HIGH. Bounded by
// MAX_EVICTIONS_PER_FRAME to avoid mass pop-in when the camera teleports.
void Flux_TerrainStreamingManagerImpl::EvictDistantHighLOD(Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos)
{
	uint32_t uEvictionsThisFrame = 0;
	// LOD_EVICTION_HYSTERESIS is a linear ratio; the threshold lives in
	// squared-distance space so square the ratio before multiplying. The
	// previous code multiplied by the linear ratio and silently produced
	// √1.5 ≈ 1.225× radius instead of the documented 1.5×.
	const float fEvictionThreshold = LOD_HIGH_MAX_DISTANCE_SQ * SquaredHysteresis(LOD_EVICTION_HYSTERESIS);

	for (uint32_t i = 0; i < TOTAL_CHUNKS && uEvictionsThisFrame < MAX_EVICTIONS_PER_FRAME; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[i];
		if (xResidency.m_aeStates[LOD_HIGH] != Flux_TerrainLODResidencyState::RESIDENT) continue;

		const float fDistanceSq = GetChunkDistanceSq(xState, i, xCameraPos);
		if (fDistanceSq <= fEvictionThreshold) continue;

		EvictLOD(xState, i, LOD_HIGH);
		uEvictionsThisFrame++;
		xState.m_xStats.m_uEvictionsThisFrame++;
		xState.m_bChunkDataDirty.store(true, std::memory_order_release);

		if (dbg_bLogTerrainStreaming)
		{
			uint32_t uChunkX, uChunkY;
			ChunkIndexToCoords(i, uChunkX, uChunkY);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Evicted chunk (%u,%u) HIGH (dist=%.0f, threshold=%.0f)",
				uChunkX, uChunkY, sqrtf(fDistanceSq), sqrtf(fEvictionThreshold));
		}
	}
}

void Flux_TerrainStreamingManagerImpl::UpdateStreamingForTerrain(Flux_TerrainStreamingState* pxState, const Zenith_Maths::Vector3& xCameraPos)
{
	if (!m_bInitialized)
	{
		if (dbg_bLogTerrainStreaming)
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] UpdateStreamingForTerrain early-out: streaming manager not initialized");
		return;
	}
	if (pxState == nullptr)
	{
		if (dbg_bLogTerrainStreaming)
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] UpdateStreamingForTerrain early-out: null streaming state");
		return;
	}
	Flux_TerrainStreamingState& xState = *pxState;
	if (!xState.m_bRegistered)
	{
		if (dbg_bLogTerrainStreaming)
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] UpdateStreamingForTerrain early-out: streaming state %p is unregistered", static_cast<void*>(pxState));
		return;
	}
	// Wave 3: the old "state's owner == passed component" consistency check is gone —
	// the caller now passes the state directly, so an owner/component mismatch is impossible.

	xState.m_uCurrentFrame++;
	xState.m_xStats.m_uStreamsThisFrame   = 0;
	xState.m_xStats.m_uEvictionsThisFrame = 0;
	xState.m_xLastCameraPos = xCameraPos;

	int32_t iCameraChunkX, iCameraChunkY;
	StreamingFrameDiagnostics xDiagnostics;
	ResolveCameraChunkCoords(xState, xCameraPos, iCameraChunkX, iCameraChunkY, xDiagnostics.m_uNearestChunkIndex);
	if (iCameraChunkX != xState.m_iLastCameraChunkX || iCameraChunkY != xState.m_iLastCameraChunkY)
	{
		RebuildActiveChunkSet(xState, iCameraChunkX, iCameraChunkY, xCameraPos);
		xState.m_iLastCameraChunkX = iCameraChunkX;
		xState.m_iLastCameraChunkY = iCameraChunkY;
		xState.m_bChunkDataDirty.store(true, std::memory_order_release);

		if (dbg_bLogTerrainStreaming)
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Camera moved to chunk (%d,%d), active set: %u chunks",
				iCameraChunkX, iCameraChunkY, static_cast<uint32_t>(xState.m_xActiveChunkIndices.GetSize()));
	}

	RequestNearbyHighLOD(xState, xCameraPos, xDiagnostics);
	EvictDistantHighLOD(xState, xCameraPos);
	xDiagnostics.m_uActiveCount = static_cast<uint32_t>(xState.m_xActiveChunkIndices.GetSize());
	xDiagnostics.m_uLowZeroCount = CountLowZeroChunks(xState);
	xDiagnostics.m_uHighResidentCount = CountHighResidentChunks(xState);

	if (xState.m_uCurrentFrame % 30 == 0)
	{
		UpdateStreamingStats(xState);
	}

#pragma warning ( push )
#pragma warning (disable : 4127)
	if (dbg_bLogTerrainStreaming && (xState.m_uCurrentFrame == 1 || (xState.m_uCurrentFrame % 60) == 0))
	{
		LogStreamingHeartbeat(xState, xCameraPos, iCameraChunkX, iCameraChunkY, xDiagnostics);
		if (xDiagnostics.m_uLowZeroCount > 0)
			LogLowZeroChunkCoordinates(xState);
	}
#pragma warning ( pop )
}

void Flux_TerrainStreamingManagerImpl::UpdateStreamingStats(Flux_TerrainStreamingState& xState)
{
	uint32_t uHighLODResident = 0;
	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		if (xState.m_axChunkResidency[i].m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
			uHighLODResident++;

	xState.m_xStats.m_uHighLODChunksResident = uHighLODResident;

	const uint32_t uVertexBytesUsed = (xState.m_xVertexAllocator.GetTotalSpace() - xState.m_xVertexAllocator.GetUnusedSpace()) * VERTEX_STRIDE_BYTES;
	const uint32_t uIndexBytesUsed  = (xState.m_xIndexAllocator.GetTotalSpace()  - xState.m_xIndexAllocator.GetUnusedSpace())  * 4;

	xState.m_xStats.m_uVertexBufferUsedMB = uVertexBytesUsed / (1024 * 1024);
	xState.m_xStats.m_uIndexBufferUsedMB  = uIndexBytesUsed / (1024 * 1024);
	xState.m_xStats.m_uVertexFragments    = xState.m_xVertexAllocator.GetFragmentationCount();
	xState.m_xStats.m_uIndexFragments     = xState.m_xIndexAllocator.GetFragmentationCount();
}

uint32_t Flux_TerrainStreamingManagerImpl::CalculateDesiredLOD(float fDistanceSq)
{
	return (fDistanceSq < LOD_HIGH_MAX_DISTANCE_SQ) ? LOD_HIGH : LOD_LOW;
}

bool Flux_TerrainStreamingManagerImpl::TryAllocateStreamingSpace(Flux_TerrainStreamingState& xState, uint32_t uNumVerts, uint32_t uNumIndices, const Zenith_Maths::Vector3& xCameraPos, StreamingAllocation& xAllocOut)
{
	uint32_t uVertexOffset = xState.m_xVertexAllocator.Allocate(uNumVerts);
	uint32_t uIndexOffset  = xState.m_xIndexAllocator .Allocate(uNumIndices);

	if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
	{
		// Clean up any partial allocation before eviction attempt
		if (uVertexOffset != UINT32_MAX) xState.m_xVertexAllocator.Free(uVertexOffset, uNumVerts);
		if (uIndexOffset  != UINT32_MAX) xState.m_xIndexAllocator .Free(uIndexOffset,  uNumIndices);

		// Try to evict distant chunks to free space
		if (!EvictToMakeSpace(xState, uNumVerts, uNumIndices, xCameraPos))
			return false;

		// Retry allocation after eviction
		uVertexOffset = xState.m_xVertexAllocator.Allocate(uNumVerts);
		uIndexOffset  = xState.m_xIndexAllocator .Allocate(uNumIndices);

		if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
		{
			if (uVertexOffset != UINT32_MAX) xState.m_xVertexAllocator.Free(uVertexOffset, uNumVerts);
			if (uIndexOffset  != UINT32_MAX) xState.m_xIndexAllocator .Free(uIndexOffset,  uNumIndices);
			return false;
		}
	}

	xAllocOut.m_uVertexOffset = uVertexOffset;
	xAllocOut.m_uIndexOffset  = uIndexOffset;
	return true;
}

Flux_TerrainStreamInResult Flux_TerrainStreamingManagerImpl::StreamInLOD(Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, uint32_t uLODLevel)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_STREAM_IN_LOD);

	// Static method (no `this`): recover the singletons once here — these are
	// the legitimate single re-entry points for a method that can't hold the
	// injected member pointers, exactly like a render-graph trampoline.
	auto& xSelf        = g_xEngine.TerrainStreaming();
	auto& xVulkanMemory = g_xEngine.FluxMemory();

	if (!xSelf.m_bInitialized || !xState.m_bRegistered)
		return Flux_TerrainStreamInResult::AllocationFailure;

	// Only HIGH LOD can be streamed (LOW is always resident). Treat as a
	// configuration error rather than a missing source.
	if (uLODLevel != LOD_HIGH)
		return Flux_TerrainStreamInResult::AllocationFailure;

	// Pre-check: if we know the typical chunk size, check whether there's
	// enough space before doing expensive disk I/O. All HIGH LOD chunks are
	// roughly the same size, so the cached count is a good predictor.
	// Previously this returned failure outright when the allocator was tight;
	// now it routes through EvictToMakeSpace first and only refuses if even
	// eviction can't free enough space — matches the eviction-aware retry in
	// TryAllocateStreamingSpace and avoids dropping chunks the allocator
	// could have made room for.
	if (xState.m_uCachedHighLODVertexCount > 0 && xState.m_uCachedHighLODIndexCount > 0)
	{
		if (xState.m_xVertexAllocator.GetUnusedSpace() < xState.m_uCachedHighLODVertexCount ||
			xState.m_xIndexAllocator .GetUnusedSpace() < xState.m_uCachedHighLODIndexCount)
		{
			if (!EvictToMakeSpace(xState,
				xState.m_uCachedHighLODVertexCount,
				xState.m_uCachedHighLODIndexCount,
				xState.m_xLastCameraPos))
			{
				return Flux_TerrainStreamInResult::AllocationFailure;
			}
		}
	}

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	// Build mesh file path - HIGH LOD uses Render_X_Y.zmesh
	std::string strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ZENITH_MESH_EXT;

	// Check if file exists
	std::ifstream lodFile(strChunkPath);
	if (!lodFile.good())
		return Flux_TerrainStreamInResult::MissingOrInvalidSource;

	// Load mesh to get size requirements, false so we don't upload to GPU
	Flux_MeshGeometry xChunkMesh;
	Flux_MeshGeometry::LoadFromFile(strChunkPath.c_str(), xChunkMesh, 0, false);
	if (xChunkMesh.GetNumVerts() == 0)
		return Flux_TerrainStreamInResult::MissingOrInvalidSource;

	const uint32_t uNumVerts   = xChunkMesh.GetNumVerts();
	const uint32_t uNumIndices = xChunkMesh.GetNumIndices();

	// Cache the chunk size for future pre-checks (all chunks are roughly the same size)
	if (xState.m_uCachedHighLODVertexCount == 0)
	{
		xState.m_uCachedHighLODVertexCount = uNumVerts;
		xState.m_uCachedHighLODIndexCount  = uNumIndices;
	}

	// Try to allocate streaming space (handles eviction and retry internally)
	StreamingAllocation xAlloc;
	if (!TryAllocateStreamingSpace(xState, uNumVerts, uNumIndices, xState.m_xLastCameraPos, xAlloc))
		return Flux_TerrainStreamInResult::AllocationFailure;

	// Calculate absolute offsets in unified buffer (streaming region starts
	// after LOW LOD). The LOW-LOD counts, vertex stride and unified buffers now
	// live ON the streaming state (Wave-18 relocation) — no pxOwner hop needed.
	const uint32_t uAbsoluteVertexOffset = xState.m_uLowLODVertexCount + xAlloc.m_uVertexOffset;
	const uint32_t uAbsoluteIndexOffset  = xState.m_uLowLODIndexCount  + xAlloc.m_uIndexOffset;

	// Calculate byte offsets
	const uint32_t uVertexStride       = xState.m_uVertexStride;
	const uint64_t ulVertexDataSize    = static_cast<uint64_t>(uNumVerts)   * uVertexStride;
	const uint64_t ulVertexOffsetBytes = static_cast<uint64_t>(uAbsoluteVertexOffset) * uVertexStride;
	const uint64_t ulIndexDataSize     = static_cast<uint64_t>(uNumIndices) * sizeof(uint32_t);
	const uint64_t ulIndexOffsetBytes  = static_cast<uint64_t>(uAbsoluteIndexOffset)  * sizeof(uint32_t);

	// Game-supplied per-chunk vertex deformation (e.g. the CityBuilder road carve): re-apply it
	// to this freshly-loaded chunk's verts BEFORE the upload, so the deformation survives a
	// stream-out/stream-in cycle (no game-side re-carve countdown). The upload below targets a
	// deferred-safe slot (just allocated, or evicted long enough ago that the GPU is done reading
	// it), so — unlike an in-place edit of an actively-rendered resident chunk — this needs no GPU
	// sync. Runs on the main thread (streaming is driven from PreRenderUpdate).
	if (xState.m_pfnChunkVertexHook != nullptr)
	{
		xState.m_pfnChunkVertexHook(xState.m_pChunkVertexHookUser, uChunkX, uChunkY,
			xChunkMesh.m_pVertexData, uNumVerts, uVertexStride);
	}

	// Upload to GPU
	xVulkanMemory.UploadBufferDataAtOffset(
		xState.m_xUnifiedVertexBuffer.GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_pVertexData,
		ulVertexDataSize,
		ulVertexOffsetBytes
	);

	xVulkanMemory.UploadBufferDataAtOffset(
		xState.m_xUnifiedIndexBuffer.GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_puIndices,
		ulIndexDataSize,
		ulIndexOffsetBytes
	);

	// Update residency
	Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];
	xResidency.m_axAllocations[uLODLevel].m_uVertexOffset = uAbsoluteVertexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uVertexCount  = uNumVerts;
	xResidency.m_axAllocations[uLODLevel].m_uIndexOffset  = uAbsoluteIndexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uIndexCount   = uNumIndices;
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::RESIDENT;

	// xChunkMesh automatically destroyed when going out of scope
	return Flux_TerrainStreamInResult::Success;
}

void Flux_TerrainStreamingManagerImpl::EvictLOD(Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, uint32_t uLODLevel)
{
	// Static method (no `this`): recover the singleton once (legitimate single
	// re-entry — see StreamInLOD).
	auto& xSelf = g_xEngine.TerrainStreaming();

	if (!xSelf.m_bInitialized || !xState.m_bRegistered)
		return;

	Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] != Flux_TerrainLODResidencyState::RESIDENT)
		return;

	// Free allocations (convert absolute to relative offsets). LOW-LOD counts
	// now live on the state itself (Wave-18 relocation).
	Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLODLevel];
	const uint32_t uRelativeVertexOffset = xAlloc.m_uVertexOffset - xState.m_uLowLODVertexCount;
	const uint32_t uRelativeIndexOffset  = xAlloc.m_uIndexOffset  - xState.m_uLowLODIndexCount;

	xState.m_xVertexAllocator.Free(uRelativeVertexOffset, xAlloc.m_uVertexCount);
	xState.m_xIndexAllocator .Free(uRelativeIndexOffset,  xAlloc.m_uIndexCount);

	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;

	// Mark the chunk-data buffer dirty so the next UpdateChunkLODAllocations
	// re-uploads with this chunk's lods[uLOD] zeroed. Critical for the
	// EvictToMakeSpace path: it calls EvictLOD in a loop without setting
	// dirty itself. Without this, the chunk buffer keeps stale RESIDENT
	// entries for chunks that have been evicted, the compute keeps reading
	// the stale offsets, and the slot-reuse-by-another-chunk produces the
	// "stretched triangle to the previous chunk's location" spike. The
	// EvictDistantHighLOD path already sets dirty redundantly at its call
	// site — that's harmless and stays.
	xState.m_bChunkDataDirty.store(true, std::memory_order_release);
}

bool Flux_TerrainStreamingManagerImpl::EvictToMakeSpace(Flux_TerrainStreamingState& xState, uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded, const Zenith_Maths::Vector3& xCameraPos)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_EVICT);

	// Build list of eviction candidates (resident HIGH LODs that are far from camera)
	struct EvictionCandidate
	{
		uint32_t m_uChunkIndex;
		float    m_fDistanceSq;
	};

	Zenith_Vector<EvictionCandidate> candidates;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[i];

		// Only HIGH LOD can be evicted
		if (xResidency.m_aeStates[LOD_HIGH] != Flux_TerrainLODResidencyState::RESIDENT)
			continue;

		float fDistanceSq = GetChunkDistanceSq(xState, i, xCameraPos);

		// Only consider for eviction if beyond HIGH LOD's range. Same fix as
		// EvictDistantHighLOD: square the linear hysteresis ratio so the
		// threshold matches the documented 1.2× radius (was effectively
		// √1.2 ≈ 1.095×).
		if (fDistanceSq > LOD_HIGH_MAX_DISTANCE_SQ * SquaredHysteresis(LOD_FORCED_EVICTION_HYSTERESIS))
		{
			EvictionCandidate candidate;
			candidate.m_uChunkIndex = i;
			candidate.m_fDistanceSq = fDistanceSq;
			candidates.PushBack(candidate);
		}
	}

	if (candidates.GetSize() == 0)
		return false;

	// Sort by distance (farthest first)
	std::sort(candidates.begin(), candidates.end(), [](const EvictionCandidate& a, const EvictionCandidate& b) {
		return a.m_fDistanceSq > b.m_fDistanceSq;
	});

	// Evict until we have enough space
	uint32_t uVertexSpaceFreed = 0;
	uint32_t uIndexSpaceFreed  = 0;
	uint32_t uEvictionsThisCall = 0;

	for (const EvictionCandidate& candidate : candidates)
	{
		if (uVertexSpaceFreed >= uVertexSpaceNeeded && uIndexSpaceFreed >= uIndexSpaceNeeded)
			break;

		if (uEvictionsThisCall >= MAX_EVICTIONS_PER_FRAME)
			break;

		Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[candidate.m_uChunkIndex];
		Flux_TerrainLODAllocation&  xAlloc     = xResidency.m_axAllocations[LOD_HIGH];

		uVertexSpaceFreed += xAlloc.m_uVertexCount;
		uIndexSpaceFreed  += xAlloc.m_uIndexCount;

		EvictLOD(xState, candidate.m_uChunkIndex, LOD_HIGH);
		uEvictionsThisCall++;
	}

	return (uVertexSpaceFreed >= uVertexSpaceNeeded && uIndexSpaceFreed >= uIndexSpaceNeeded);
}

Zenith_Maths::Vector3 Flux_TerrainStreamingManagerImpl::GetChunkCenter(const Flux_TerrainStreamingState& xState, uint32_t uChunkX, uint32_t uChunkY)
{
	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);

	if (xState.m_bAABBsCached)
	{
		const Zenith_AABB& xAABB = xState.m_axChunkAABBs[uChunkIndex];
		return (xAABB.m_xMin + xAABB.m_xMax) * 0.5f;
	}

	// Fallback (shouldn't happen after initialization)
	const float fX = (static_cast<float>(uChunkX) + 0.5f) * CHUNK_WORLD_SIZE;
	const float fZ = (static_cast<float>(uChunkY) + 0.5f) * CHUNK_WORLD_SIZE;
	const float fY = MAX_TERRAIN_HEIGHT * 0.5f;

	return Zenith_Maths::Vector3(fX, fY, fZ);
}

float Flux_TerrainStreamingManagerImpl::GetChunkDistanceSq(const Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, const Zenith_Maths::Vector3& xWorldPos)
{
	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);
	Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(xState, uChunkX, uChunkY);
	return glm::distance2(xWorldPos, xChunkCenter);
}

void Flux_TerrainStreamingManagerImpl::WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY)
{
	iChunkX = static_cast<int32_t>(xWorldPos.x / CHUNK_WORLD_SIZE);
	iChunkY = static_cast<int32_t>(xWorldPos.z / CHUNK_WORLD_SIZE);

	// Clamp to valid range (use std:: prefix to avoid Windows macro conflicts)
	if (iChunkX < 0) iChunkX = 0;
	if (iChunkX >= static_cast<int32_t>(CHUNK_GRID_SIZE)) iChunkX = static_cast<int32_t>(CHUNK_GRID_SIZE - 1);
	if (iChunkY < 0) iChunkY = 0;
	if (iChunkY >= static_cast<int32_t>(CHUNK_GRID_SIZE)) iChunkY = static_cast<int32_t>(CHUNK_GRID_SIZE - 1);
}

uint32_t Flux_TerrainStreamingManagerImpl::FindNearestChunkByAABB(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xWorldPos)
{
	uint32_t uBestChunkIndex = 0;
	float fBestDistanceSq = FLT_MAX;

	for (uint32_t uChunkIndex = 0; uChunkIndex < TOTAL_CHUNKS; ++uChunkIndex)
	{
		const float fDistanceSq = GetChunkDistanceSq(xState, uChunkIndex, xWorldPos);
		if (fDistanceSq < fBestDistanceSq)
		{
			fBestDistanceSq = fDistanceSq;
			uBestChunkIndex = uChunkIndex;
		}
	}

	return uBestChunkIndex;
}

void Flux_TerrainStreamingManagerImpl::ResolveCameraChunkCoords(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY, uint32_t& uNearestChunkIndex)
{
	if (xState.m_bAABBsCached)
	{
		uNearestChunkIndex = FindNearestChunkByAABB(xState, xWorldPos);
		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(uNearestChunkIndex, uChunkX, uChunkY);
		iChunkX = static_cast<int32_t>(uChunkX);
		iChunkY = static_cast<int32_t>(uChunkY);
		return;
	}

	WorldPosToChunkCoords(xWorldPos, iChunkX, iChunkY);
	uNearestChunkIndex = ChunkCoordsToIndex(static_cast<uint32_t>(iChunkX), static_cast<uint32_t>(iChunkY));
}

uint32_t Flux_TerrainStreamingManagerImpl::CountLowZeroChunks(const Flux_TerrainStreamingState& xState)
{
	uint32_t uLowZeroCount = 0;
	for (uint32_t uChunkIndex = 0; uChunkIndex < TOTAL_CHUNKS; ++uChunkIndex)
	{
		const Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];
		if (xResidency.m_aeStates[LOD_LOW] != Flux_TerrainLODResidencyState::RESIDENT ||
			xResidency.m_axAllocations[LOD_LOW].m_uIndexCount == 0)
		{
			uLowZeroCount++;
		}
	}
	return uLowZeroCount;
}

uint32_t Flux_TerrainStreamingManagerImpl::CountHighResidentChunks(const Flux_TerrainStreamingState& xState)
{
	uint32_t uHighResidentCount = 0;
	for (uint32_t uChunkIndex = 0; uChunkIndex < TOTAL_CHUNKS; ++uChunkIndex)
	{
		if (xState.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
			uHighResidentCount++;
	}
	return uHighResidentCount;
}

void Flux_TerrainStreamingManagerImpl::LogLowZeroChunkCoordinates(const Flux_TerrainStreamingState& xState, uint32_t uMaxToLog)
{
	uint32_t uLogged = 0;
	for (uint32_t uChunkIndex = 0; uChunkIndex < TOTAL_CHUNKS; ++uChunkIndex)
	{
		const Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];
		if (xResidency.m_aeStates[LOD_LOW] == Flux_TerrainLODResidencyState::RESIDENT &&
			xResidency.m_axAllocations[LOD_LOW].m_uIndexCount != 0)
		{
			continue;
		}

		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[Terrain] LOW zero-count chunk (%u,%u): state=%u, indexCount=%u",
			uChunkX, uChunkY,
			static_cast<uint32_t>(xResidency.m_aeStates[LOD_LOW]),
			xResidency.m_axAllocations[LOD_LOW].m_uIndexCount);

		uLogged++;
		if (uLogged >= uMaxToLog)
		{
			const uint32_t uTotal = CountLowZeroChunks(xState);
			if (uTotal > uLogged)
				Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] LOW zero-count chunk log truncated: %u more chunk(s)", uTotal - uLogged);
			return;
		}
	}
}

void Flux_TerrainStreamingManagerImpl::LogStreamingHeartbeat(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos, int32_t iCameraChunkX, int32_t iCameraChunkY, const StreamingFrameDiagnostics& xDiagnostics)
{
	uint32_t uNearestChunkX, uNearestChunkY;
	ChunkIndexToCoords(xDiagnostics.m_uNearestChunkIndex, uNearestChunkX, uNearestChunkY);

	Zenith_Log(LOG_CATEGORY_TERRAIN,
		"[Terrain] Streaming heartbeat state=%p camera=(%.1f, %.1f, %.1f) cameraChunk=(%d,%d) nearestChunk=(%u,%u) active=%u desiredHigh=%u alreadyResidentHigh=%u attempts=%u successes=%u missing=%u allocFailures=%u LOWZero=%u HIGHResident=%u",
		static_cast<const void*>(&xState),
		xCameraPos.x, xCameraPos.y, xCameraPos.z,
		iCameraChunkX, iCameraChunkY,
		uNearestChunkX, uNearestChunkY,
		xDiagnostics.m_uActiveCount,
		xDiagnostics.m_uDesiredHighCount,
		xDiagnostics.m_uAlreadyResidentHighCount,
		xDiagnostics.m_uStreamAttempts,
		xDiagnostics.m_uStreamSuccesses,
		xDiagnostics.m_uMissingSourceCount,
		xDiagnostics.m_uAllocationFailureCount,
		xDiagnostics.m_uLowZeroCount,
		xDiagnostics.m_uHighResidentCount);
}

void Flux_TerrainStreamingManagerImpl::RebuildActiveChunkSet(Flux_TerrainStreamingState& xState, int32_t iCameraChunkX, int32_t iCameraChunkY, const Zenith_Maths::Vector3& xCameraPos)
{
	xState.m_xActiveChunkIndices.Clear();

	const int32_t iRadius  = static_cast<int32_t>(xState.m_uActiveChunkRadius);
	const int32_t iGridMax = static_cast<int32_t>(CHUNK_GRID_SIZE - 1);

	// Calculate bounds (avoid Windows min/max macro conflicts)
	int32_t iMinX = iCameraChunkX - iRadius;
	int32_t iMaxX = iCameraChunkX + iRadius;
	int32_t iMinY = iCameraChunkY - iRadius;
	int32_t iMaxY = iCameraChunkY + iRadius;

	if (iMinX < 0) iMinX = 0;
	if (iMaxX > iGridMax) iMaxX = iGridMax;
	if (iMinY < 0) iMinY = 0;
	if (iMaxY > iGridMax) iMaxY = iGridMax;

	// Build (chunk, distSq) pairs over the rectangle, sort ascending, then
	// drop the indices into the active vector. Sorting nearest-first means
	// the per-frame MAX_UPLOADS_PER_FRAME budget gets spent on chunks the
	// camera can actually see, instead of being burned on the corner of the
	// raster-order rectangle. Sort runs once per rebuild (rebuild fires only
	// when the camera crosses a chunk boundary), not per frame.
	struct ChunkDist { uint32_t m_uChunkIndex; float m_fDistSq; };
	Zenith_Vector<ChunkDist> xCandidates;
	const int32_t iWidth  = iMaxX - iMinX + 1;
	const int32_t iHeight = iMaxY - iMinY + 1;
	if (iWidth > 0 && iHeight > 0)
	{
		xCandidates.Reserve(static_cast<u_int>(iWidth) * static_cast<u_int>(iHeight));
	}
	for (int32_t x = iMinX; x <= iMaxX; ++x)
	{
		for (int32_t y = iMinY; y <= iMaxY; ++y)
		{
			const uint32_t uChunkIndex = ChunkCoordsToIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
			ChunkDist xCD;
			xCD.m_uChunkIndex = uChunkIndex;
			xCD.m_fDistSq     = GetChunkDistanceSq(xState, uChunkIndex, xCameraPos);
			xCandidates.PushBack(xCD);
		}
	}
	std::sort(xCandidates.begin(), xCandidates.end(), [](const ChunkDist& a, const ChunkDist& b)
	{
		return a.m_fDistSq < b.m_fDistSq;
	});
	xState.m_xActiveChunkIndices.Reserve(xCandidates.GetSize());
	for (const ChunkDist& xCD : xCandidates)
		xState.m_xActiveChunkIndices.PushBack(xCD.m_uChunkIndex);
}

// Populate one per-LOD slot of a chunk's GPU data from its residency state.
// Slots without a resident allocation are zeroed — the compute shader's
// "indexCount == 0 → fall back / skip" path takes over. Do NOT mirror LOW's
// allocation into HIGH; that desyncs the LOD debug colour from the geometry
// the indirect draw actually consumes.
static void BuildLODSlotForChunk(uint32_t uLOD, const Flux_TerrainChunkResidency& xResidency, Zenith_TerrainLODData& xLODOut)
{
	xLODOut.m_fMaxDistance = LOD_MAX_DISTANCE_SQ[uLOD];

	if (xResidency.m_aeStates[uLOD] != Flux_TerrainLODResidencyState::RESIDENT)
	{
		xLODOut.m_uFirstIndex   = 0;
		xLODOut.m_uIndexCount   = 0;
		xLODOut.m_uVertexOffset = 0;
		return;
	}

	const Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLOD];
	xLODOut.m_uFirstIndex = xAlloc.m_uIndexOffset;
	xLODOut.m_uIndexCount = xAlloc.m_uIndexCount;
	// LOW LOD uses combined buffer with rebased indices (vertexOffset = 0).
	// HIGH LOD uses individually uploaded chunks with 0-based indices (need offset).
	xLODOut.m_uVertexOffset = (uLOD == LOD_LOW) ? 0 : xAlloc.m_uVertexOffset;
}

// Populate one chunk's full GPU data: AABB (if cached) + every LOD slot.
static void BuildOneChunkData(const Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, Zenith_TerrainChunkData& xChunkData)
{
	const Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];

	if (xState.m_bAABBsCached)
	{
		const Zenith_AABB& xAABB = xState.m_axChunkAABBs[uChunkIndex];
		xChunkData.m_xAABBMin = Zenith_Maths::Vector4(xAABB.m_xMin, 0.0f);
		xChunkData.m_xAABBMax = Zenith_Maths::Vector4(xAABB.m_xMax, 0.0f);
	}

	for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
	{
		BuildLODSlotForChunk(uLOD, xResidency, xChunkData.m_axLODs[uLOD]);
	}
}

// Shared body for BuildChunkDataForGPU: walks one state's chunk residency
// and emits per-chunk GPU data. Both the component-aware overload and the
// legacy primary-resolving overload route through here so the chunk-data
// shape lives in one place.
void Flux_TerrainStreamingManagerImpl::BuildChunkDataForGPU_Internal(const Flux_TerrainStreamingState& xState, Zenith_TerrainChunkData* pxChunkDataOut)
{
	// Diagnostic counters — should let us tell the difference between
	// "LOW geometry never loaded" and "HIGH streaming isn't catching up".
	// Logged at most once every 30 frames to avoid spam.
	uint32_t uLowZeroCountChunks    = 0;  // expected: 0 for healthy terrain
	uint32_t uHighResidentChunks    = 0;  // expected: climbs near camera

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			const uint32_t uChunkIndex = x * CHUNK_GRID_SIZE + y;
			Zenith_TerrainChunkData& xChunkData = pxChunkDataOut[uChunkIndex];

			BuildOneChunkData(xState, uChunkIndex, xChunkData);

			const Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIndex];
			if (xChunkData.m_axLODs[LOD_LOW].m_uIndexCount == 0) uLowZeroCountChunks++;
			if (xResidency.m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT) uHighResidentChunks++;
		}
	}

	// Diagnostic surface — only fires when the streaming logger is on, so
	// it doesn't spam the normal log. LOW zero-count chunks should be 0;
	// any other value points directly at "the visible holes in the terrain
	// are LOW geometry that never loaded". HIGH resident climbing near the
	// camera (and back to 0 far from it) is the steady-state expectation.
	if (dbg_bLogTerrainStreaming)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[Terrain] BuildChunkDataForGPU: LOW zero-count=%u (expect 0), HIGH resident=%u",
			uLowZeroCountChunks, uHighResidentChunks);
		if (uLowZeroCountChunks > 0)
			LogLowZeroChunkCoordinates(xState);
	}
}

void Flux_TerrainStreamingManagerImpl::BuildChunkDataForGPU(const Flux_TerrainStreamingState* pxState, Zenith_TerrainChunkData* pxChunkDataOut)
{
	if (!m_bInitialized || pxState == nullptr)
		return;
	BuildChunkDataForGPU_Internal(*pxState, pxChunkDataOut);
}



// ============================================================================
// Wave 3: per-frame GPU culling/LOD drive, relocated VERBATIM from
// Zenith_TerrainComponent (UpdateChunkLODAllocations / UploadFrustumPlanesForFrame /
// UpdateCullingAndLod). They operate purely on the Flux_TerrainStreamingState (+ this
// manager / VulkanMemory / FluxGraphics) -- no EntityComponent type -- so Flux_Terrain
// can drive them through the state instead of the component. Same GPU commands/uploads.
// ============================================================================

void Flux_TerrainStreamingManagerImpl::UpdateChunkLODAllocations(Flux_TerrainStreamingState& xState)
{
	if (!xState.m_bCullingResourcesInitialized)
	{
		return; // Terrain not ready for rendering yet
	}

	// Always rebuild + upload (the per-component dirty-flag short-circuit produced a
	// stretched-triangle spike; see the original component note). 256 KB/terrain/frame.
	Zenith_TerrainChunkData* pxChunkData = GetCachedChunkDataBuffer(&xState);
	bool bUsedCachedBuffer = (pxChunkData != nullptr);
	if (pxChunkData == nullptr)
	{
		pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	}

	BuildChunkDataForGPU(&xState, pxChunkData);

	g_xEngine.FluxMemory().UploadBufferDataAtOffset(
		xState.m_xChunkDataBuffer.GetBuffer().m_xVRAMHandle,
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TOTAL_CHUNKS,
		0);

	if (!bUsedCachedBuffer)
	{
		delete[] pxChunkData;
	}

	ClearChunkDataDirty(&xState);
}

void Flux_TerrainStreamingManagerImpl::UploadFrustumPlanesForFrame(Flux_TerrainStreamingState& xState, const Zenith_Maths::Matrix4& xViewProjMatrix)
{
	if (!xState.m_bCullingResourcesInitialized)
	{
		return;
	}

	Zenith_CameraDataGPU xCameraData;
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);

	for (int i = 0; i < 6; ++i)
	{
		xCameraData.m_axFrustumPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance);
	}

	const Zenith_Maths::Vector3 xCameraPos = g_xEngine.FluxGraphics().GetCameraPosition();
	xCameraData.m_xCameraPosition = Zenith_Maths::Vector4(xCameraPos, 0.0f);

	g_xEngine.FluxMemory().UploadBufferDataAtOffset(xState.m_xFrustumPlanesBuffer.GetBuffer().m_xVRAMHandle, &xCameraData, sizeof(Zenith_CameraDataGPU), 0);
}

void Flux_TerrainStreamingManagerImpl::UpdateCullingAndLod(Flux_TerrainStreamingState& xState, Flux_CommandList& xCmdList)
{
	if (!xState.m_bCullingResourcesInitialized)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManagerImpl::UpdateCullingAndLod() called before culling resources initialised");
		return;
	}

	// Pipeline already bound by Flux_Terrain; frustum/visible-count prepared upstream.
	xCmdList.AddCommand<Flux_CommandBindSRV_Buffer>(xState.m_xChunkDataBuffer.GetSRV(), Flux_BindingSlot{ 0, 0, true });
	xCmdList.AddCommand<Flux_CommandBindCBV>(&xState.m_xFrustumPlanesBuffer.GetCBV(), 1);
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&xState.m_xIndirectDrawBuffer.GetUAV(), 2);
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&xState.m_xVisibleCountBuffer.GetUAV(), 3);
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&xState.m_xLODLevelBuffer.GetUAV(), 4);

	uint32_t uNumWorkgroups = (TOTAL_CHUNKS + 63) / 64;
	xCmdList.AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
}
