#pragma once

#include "Flux/Flux_Buffers.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Flux/Terrain/Flux_TerrainGPUStructs.h"
#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <cstdint>
#include <atomic>
#include <string>

// Use unified terrain configuration
using namespace Flux_TerrainConfig;

// Forward declarations
class Flux_MeshGeometry;
class Flux_RendererImpl;
class FrameContext;

// ========== Residency State ==========
enum class Flux_TerrainLODResidencyState : uint8_t
{
	NOT_LOADED = 0,
	RESIDENT
};

// ========== LOD Allocation Info ==========
struct Flux_TerrainLODAllocation
{
	uint32_t m_uVertexOffset = 0;
	uint32_t m_uVertexCount = 0;
	uint32_t m_uIndexOffset = 0;
	uint32_t m_uIndexCount = 0;
};

// ========== Per-Chunk Residency Tracking ==========
struct Flux_TerrainChunkResidency
{
	Flux_TerrainLODResidencyState m_aeStates[LOD_COUNT]      = {};
	Flux_TerrainLODAllocation     m_axAllocations[LOD_COUNT] = {};
};

// ========== Buffer Allocator ==========
class Flux_TerrainBufferAllocator
{
public:
	Flux_TerrainBufferAllocator();

	void Initialize(uint32_t uTotalSize, const char* szDebugName);
	void Reset();

	uint32_t Allocate(uint32_t uSize);
	void Free(uint32_t uOffset, uint32_t uSize);

	uint32_t GetUnusedSpace() const { return m_uUnusedSpace; }
	uint32_t GetTotalSpace() const { return m_uTotalSize; }
	uint32_t GetFragmentationCount() const { return static_cast<uint32_t>(m_axFreeBlocks.GetSize()); }

private:
	struct FreeBlock
	{
		uint32_t m_uOffset;
		uint32_t m_uSize;
	};

	Zenith_Vector<FreeBlock> m_axFreeBlocks;
	uint32_t m_uTotalSize;
	uint32_t m_uUnusedSpace;
	const char* m_szDebugName;
};

// ========== Per-Chunk Init Data ==========
struct Flux_TerrainChunkInitData
{
	uint32_t m_uVertexCount = 0;
	uint32_t m_uIndexCount  = 0;
	Zenith_AABB m_xAABB;
};

// ========== Stream-In Result ==========
enum class Flux_TerrainStreamInResult : uint8_t
{
	Success,
	MissingOrInvalidSource,
	AllocationFailure,
};

// ========== Streaming Stats ==========
struct Flux_TerrainStreamingStats
{
	uint32_t m_uHighLODChunksResident = 0;
	uint32_t m_uStreamsThisFrame = 0;
	uint32_t m_uEvictionsThisFrame = 0;
	uint32_t m_uVertexBufferUsedMB = 0;
	uint32_t m_uVertexBufferTotalMB = 0;
	uint32_t m_uIndexBufferUsedMB = 0;
	uint32_t m_uIndexBufferTotalMB = 0;
	uint32_t m_uVertexBufferFragments = 0;
	uint32_t m_uIndexBufferFragments = 0;
	uint32_t m_uVertexFragments = 0;
	uint32_t m_uIndexFragments = 0;
};

// ========== Per-Terrain Streaming State ==========
struct Flux_TerrainStreamingState
{
	std::string                 m_strTerrainAssetDirectory;
	Flux_TerrainBufferAllocator m_xVertexAllocator;
	Flux_TerrainBufferAllocator m_xIndexAllocator;
	Flux_TerrainChunkResidency  m_axChunkResidency[TOTAL_CHUNKS];
	Zenith_AABB                 m_axChunkAABBs[TOTAL_CHUNKS];
	bool                        m_bAABBsCached      = false;

	uint32_t                    m_uCachedHighLODVertexCount = 0;
	uint32_t                    m_uCachedHighLODIndexCount  = 0;

	std::atomic<bool>           m_bChunkDataDirty   { true };
	Zenith_TerrainChunkData*    m_pxCachedChunkData = nullptr;

	Zenith_Maths::Vector3       m_xLastCameraPos    { FLT_MAX, FLT_MAX, FLT_MAX };
	int32_t                     m_iLastCameraChunkX = INT32_MIN;
	int32_t                     m_iLastCameraChunkY = INT32_MIN;

	Zenith_Vector<uint32_t>     m_xActiveChunkIndices;
	uint32_t                    m_uActiveChunkRadius = 16;

	Flux_TerrainStreamingStats  m_xStats;

	// First-UpdateStreaming heartbeat log gate (a flag, not a clock — the
	// periodic heartbeat cadence reads the engine frame index from
	// g_xEngine.Frame(), the single frame-index variable engine-wide).
	bool                        m_bFirstHeartbeatLogged = false;

	// Wave 3: replaces the old Zenith_TerrainComponent* m_pxOwner. That pointer was
	// never dereferenced — it was used purely as a live/registered flag (set on
	// Initialize/Register, cleared on Shutdown/Unregister) plus a now-removed
	// owner==component consistency check. A plain bool keeps the flag semantics while
	// letting this Flux state name no EntityComponent type.
	bool                        m_bRegistered = false;

	// ========== Game-supplied per-chunk vertex deformation hook (e.g. CityBuilder road carve) ==========
	// Called in StreamInLOD after the baked chunk mesh loads and BEFORE the GPU upload, so a
	// re-streamed HIGH chunk is re-deformed every time it loads (the deformation persists across
	// streaming — no game-side re-carve countdown). Streaming is main-thread (PreRenderUpdate), so
	// this runs on the main thread; the upload targets a deferred-safe slot (never-used, or evicted
	// MAX_FRAMES_IN_FLIGHT+1 frames ago so the GPU is guaranteed done reading it) — no GPU sync
	// needed here, unlike an in-place edit of an actively-rendered resident chunk. Null = no
	// deformation. Args: (pUser, chunkX, chunkY, pVertexData, numVerts, vertexStride).
	typedef void (*ChunkVertexHook)(void*, uint32_t, uint32_t, void*, uint32_t, uint32_t);
	ChunkVertexHook m_pfnChunkVertexHook   = nullptr;
	void*           m_pChunkVertexHookUser = nullptr;

	// ========== Unified Terrain Buffers (relocated from Zenith_TerrainComponent) ==========
	// Contains LOW LOD (always-resident) data at the beginning, followed by
	// streaming space for HIGH LOD. Owned here (one streaming state per terrain
	// component); the component forwards its buffer accessors to these members.
	Flux_VertexBuffer m_xUnifiedVertexBuffer;
	Flux_IndexBuffer  m_xUnifiedIndexBuffer;

	// Buffer sizes and layout information
	uint64_t m_ulUnifiedVertexBufferSize = 0;
	uint64_t m_ulUnifiedIndexBufferSize  = 0;
	uint32_t m_uVertexStride             = 0;
	uint32_t m_uLowLODVertexCount        = 0;   // Vertices reserved for LOW LOD at buffer start
	uint32_t m_uLowLODIndexCount         = 0;   // Indices reserved for LOW LOD at buffer start

	// ========== GPU-Driven Culling State (relocated from Zenith_TerrainComponent) ==========
	bool m_bCullingResourcesInitialized = false;

	// GPU buffers for culling.
	// m_xChunkDataBuffer is frame-indexed (one Flux_Buffer per frame in flight).
	// The buffer is rebuilt + host-uploaded every frame in
	// UpdateChunkLODAllocations, so frame N+1's CPU write to the underlying
	// memory must not race against frame N's GPU compute read. A single shared
	// buffer would race here: even though the per-frame fence at BeginFrame
	// guarantees the slot's previous use is complete, slot K's frame N+2 CPU
	// work runs concurrently with slot K+1's frame N+1 GPU work — and they hit
	// the same shared chunk-data memory. Frame indexing closes that race
	// entirely (one buffer per frame slot, CPU only ever writes the slot whose
	// fence we just waited on). Memory residency is host-visible (see
	// Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer), so the
	// upload skips the staging buffer too — eliminating the staging-reuse race
	// that the previous staged-upload path was exposed to.
	Flux_DynamicReadWriteBuffer m_xChunkDataBuffer;
	Flux_IndirectBuffer         m_xIndirectDrawBuffer;   // Indirect draw commands (written by compute)
	Flux_DynamicConstantBuffer  m_xFrustumPlanesBuffer;  // Camera frustum + position (read-only in compute)
	Flux_IndirectBuffer         m_xVisibleCountBuffer;   // Atomic counter for visible chunks
	Flux_ReadWriteBuffer        m_xLODLevelBuffer;       // LOD level for each chunk (visualization)

	void Initialize();
	void Shutdown();
};

// ---------------------------------------------------------------------------
// Wave 3 (PART B): per-terrain render inputs, gathered EC-side so Flux_Terrain names
// no Zenith_TerrainComponent. The Flux_TerrainStreamingState supplies every GPU buffer
// (unified vertex/index, indirect, visible-count, LOD-level, chunk-data, frustum) and
// the per-frame culling/LOD drive (the relocated manager methods); the materials +
// splatmap are the component's asset handles (asset types, not EC components).
// ---------------------------------------------------------------------------
class Zenith_MaterialAsset;
class Zenith_TextureAsset;

struct Flux_TerrainRenderRecord
{
	Flux_TerrainStreamingState* m_pxState        = nullptr;
	Zenith_MaterialAsset*       m_apxMaterials[4] = { nullptr, nullptr, nullptr, nullptr };
	Zenith_TextureAsset*        m_pxSplatmap      = nullptr;
	// Phase 4c: GPU material-table index per splat slot, resolved on the main
	// thread in Flux_TerrainImpl::PreRenderUpdate (null slots resolve to the
	// engine blank material). The worker ExecuteGBuffer copies these into the
	// terrain draw constants; the shader samples g_axMaterials[idx] -> g_axTextures.
	u_int                       m_auMaterialTableIndices[4] = { 0u, 0u, 0u, 0u };
};

// The EC side sets this to its gatherer (defined in Zenith_TerrainComponent.cpp, which
// owns the components); Flux_Terrain calls it each frame to rebuild its record list.
using Zenith_TerrainGatherFn = void (*)(Zenith_Vector<Flux_TerrainRenderRecord>& xOut);
extern Zenith_TerrainGatherFn g_pfnZenithTerrainGather;

// Phase 9: state + behaviour for the terrain streaming manager.
//
// Methods that touch the registry are non-static instance members
// (Initialize / Shutdown / Register* / Unregister* / UpdateStreaming* /
// BuildChunkData* / IsChunkDataDirty / ClearChunkDataDirty /
// GetCachedChunkDataBuffer / GetStateFor / IsInitialized).
//
// Methods that take a Flux_TerrainStreamingState& and operate purely on
// that state are kept as `static` member functions -- they're stateless
// utilities, not engine-state mutators.
class Flux_TerrainStreamingManagerImpl
{
public:
	Flux_TerrainStreamingManagerImpl() = default;
	~Flux_TerrainStreamingManagerImpl() = default;

	Flux_TerrainStreamingManagerImpl(const Flux_TerrainStreamingManagerImpl&) = delete;
	Flux_TerrainStreamingManagerImpl& operator=(const Flux_TerrainStreamingManagerImpl&) = delete;

	// ========== Lifecycle ==========
	void Initialize();
	void Shutdown();
	bool IsInitialized() const { return m_bInitialized; }

	// ========== Buffer Registration ==========
	// Wave 3: these take the Flux_TerrainStreamingState directly. The manager touched the
	// component ONLY to reach ->m_pxStreamingState (and m_pxOwner was write-only/vestigial),
	// so passing the state in lets this TU name no EntityComponent type. Callers that hold a
	// Zenith_TerrainComponent pass its (public) m_pxStreamingState.
	void RegisterTerrainBuffers(Flux_TerrainStreamingState* pxState, const Flux_TerrainChunkInitData* pxChunkInitData);
	void UnregisterTerrainBuffers(Flux_TerrainStreamingState* pxState);

	// ========== Main Update ==========
	void UpdateStreamingForTerrain(Flux_TerrainStreamingState* pxState, const Zenith_Maths::Vector3& xCameraPos);

	// ========== GPU Data Building ==========
	void BuildChunkDataForGPU(const Flux_TerrainStreamingState* pxState, Zenith_TerrainChunkData* pxChunkDataOut);
	bool IsChunkDataDirty(const Flux_TerrainStreamingState* pxState);
	void ClearChunkDataDirty(Flux_TerrainStreamingState* pxState); // mutates (.store) -> non-const
	Zenith_TerrainChunkData* GetCachedChunkDataBuffer(const Flux_TerrainStreamingState* pxState);

	// ========== Per-frame GPU culling/LOD drive ==========
	// Wave 3: relocated verbatim from Zenith_TerrainComponent — they operate purely on the
	// Flux_TerrainStreamingState (+ this manager / VulkanMemory / FluxGraphics), naming no EC
	// type, so the renderer can drive them through the state without including the component.
	void UpdateChunkLODAllocations(Flux_TerrainStreamingState& xState);
	void UploadFrustumPlanesForFrame(Flux_TerrainStreamingState& xState, const Zenith_Maths::Matrix4& xViewProjMatrix);
	void UpdateCullingAndLod(Flux_TerrainStreamingState& xState, Flux_CommandBuffer& xCmdList);

	// ========== Stats type alias ==========
	using StreamingStats = Flux_TerrainStreamingStats;

	// Resolve a component to its streaming state.
	// Wave 3: GetStateFor(component) removed — it was a dead component->state accessor with
	// no callers (callers hold the component and use its public m_pxStreamingState directly).

	friend class Zenith_UnitTests;

	// ========== State ==========
	bool                                       m_bInitialized = false;
	Zenith_Vector<Flux_TerrainStreamingState*> m_xRegistry;
	Zenith_Mutex                               m_xRegistryMutex;

	// ========== Injected cross-subsystem deps (self-wired in Initialize) ==========
	// Self-wired (set once at the top of Initialize from g_xEngine) rather than
	// injected via the Initialize signature: Initialize() is no-arg and called
	// lazily from Zenith_TerrainComponent (not the engine composition root), so
	// the signature can't take params without breaking those out-of-scope call
	// sites. One boundary reach per dep, zero call-site change.
	Flux_RendererImpl*                         m_pxFluxRenderer = nullptr;
	FrameContext*                              m_pxFrame        = nullptr;

	// ========== Internal pure helpers (still static -- operate on State, not the manager) ==========
	struct StreamingAllocation
	{
		uint32_t m_uVertexOffset;
		uint32_t m_uIndexOffset;
	};

	struct StreamingFrameDiagnostics
	{
		uint32_t m_uNearestChunkIndex = 0;
		uint32_t m_uActiveCount = 0;
		uint32_t m_uDesiredHighCount = 0;
		uint32_t m_uAlreadyResidentHighCount = 0;
		uint32_t m_uStreamAttempts = 0;
		uint32_t m_uStreamSuccesses = 0;
		uint32_t m_uMissingSourceCount = 0;
		uint32_t m_uAllocationFailureCount = 0;
		uint32_t m_uLowZeroCount = 0;
		uint32_t m_uHighResidentCount = 0;
	};

	static float GetChunkDistanceSq(const Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, const Zenith_Maths::Vector3& xWorldPos);
	static bool TryAllocateStreamingSpace(Flux_TerrainStreamingState& xState, uint32_t uNumVerts, uint32_t uNumIndices, const Zenith_Maths::Vector3& xCameraPos, StreamingAllocation& xAllocOut);
	static void UpdateStreamingStats(Flux_TerrainStreamingState& xState);
	static void RequestNearbyHighLOD(Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos, StreamingFrameDiagnostics& xDiagnostics);
	static void EvictDistantHighLOD(Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos);
	static Flux_TerrainStreamInResult StreamInLOD(Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, uint32_t uLODLevel);
	static void EvictLOD(Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, uint32_t uLODLevel);
	static bool EvictToMakeSpace(Flux_TerrainStreamingState& xState, uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded, const Zenith_Maths::Vector3& xCameraPos);
	static uint32_t CalculateDesiredLOD(float fDistanceSq);
	static void RebuildActiveChunkSet(Flux_TerrainStreamingState& xState, int32_t iCameraChunkX, int32_t iCameraChunkY, const Zenith_Maths::Vector3& xCameraPos);
	static uint32_t FindNearestChunkByAABB(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xWorldPos);
	static void ResolveCameraChunkCoords(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY, uint32_t& uNearestChunkIndex);
	static uint32_t CountLowZeroChunks(const Flux_TerrainStreamingState& xState);
	static uint32_t CountHighResidentChunks(const Flux_TerrainStreamingState& xState);
	static void LogLowZeroChunkCoordinates(const Flux_TerrainStreamingState& xState, uint32_t uMaxToLog = 16);
	static void LogStreamingHeartbeat(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos, int32_t iCameraChunkX, int32_t iCameraChunkY, const StreamingFrameDiagnostics& xDiagnostics);
	static void BuildChunkDataForGPU_Internal(const Flux_TerrainStreamingState& xState, Zenith_TerrainChunkData* pxChunkDataOut);

	static Zenith_Maths::Vector3 GetChunkCenter(const Flux_TerrainStreamingState& xState, uint32_t uChunkX, uint32_t uChunkY);
	static void WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY);

	static inline uint32_t ChunkCoordsToIndex(uint32_t uChunkX, uint32_t uChunkY)
	{
		return uChunkX * CHUNK_GRID_SIZE + uChunkY;
	}

	static inline void ChunkIndexToCoords(uint32_t uChunkIndex, uint32_t& uChunkX, uint32_t& uChunkY)
	{
		uChunkX = uChunkIndex / CHUNK_GRID_SIZE;
		uChunkY = uChunkIndex % CHUNK_GRID_SIZE;
	}

	static const char* GetLODName(uint32_t uLOD)
	{
		static const char* names[] = { "HIGH", "LOW" };
		return (uLOD < LOD_COUNT) ? names[uLOD] : "LOD?";
	}
};
