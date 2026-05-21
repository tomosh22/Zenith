#pragma once

// Unified terrain configuration - single source of truth for all terrain constants
#include "Flux/Terrain/Flux_TerrainConfig.h"
using namespace Flux_TerrainConfig;

#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Maths/Zenith_FrustumCulling.h"

// Forward declarations
class Flux_CommandList;
struct Flux_TerrainChunkInitData;
struct Flux_TerrainStreamingState;

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

// LOD data for a single level
struct Zenith_TerrainLODData
{
	uint32_t m_uFirstIndex;    // Starting index in the index buffer for this LOD
	uint32_t m_uIndexCount;    // Number of indices to draw for this LOD
	uint32_t m_uVertexOffset;  // Base vertex offset (always 0 for combined mesh)
	float m_fMaxDistance;      // Maximum distance (squared) at which this LOD is used
};

// Chunk data structure that gets uploaded to GPU
// Must match the GLSL struct in Flux_TerrainCulling.comp
struct Zenith_TerrainChunkData
{
	Zenith_Maths::Vector4 m_xAABBMin;                   // xyz = min corner, w = padding
	Zenith_Maths::Vector4 m_xAABBMax;                   // xyz = max corner, w = padding
	Zenith_TerrainLODData m_axLODs[LOD_COUNT];          // LOD mesh data (HIGH=0, LOW=1)
};

// Frustum plane structure for GPU upload
struct Zenith_FrustumPlaneGPU
{
	Zenith_Maths::Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
};

// Camera culling data structure for GPU upload
struct Zenith_CameraDataGPU
{
	Zenith_FrustumPlaneGPU m_axFrustumPlanes[6];  // 6 frustum planes
	Zenith_Maths::Vector4 m_xCameraPosition;      // xyz = camera position, w = padding
};

class Zenith_TerrainComponent
{
public:
	// Default constructor for deserialization. ReadFromDataStream populates
	// the rest of the members from the saved data; defined out-of-line in
	// the .cpp so the per-terrain Flux_TerrainStreamingState (forward-
	// declared above) can be allocated here without pulling its full
	// definition into this header.
	Zenith_TerrainComponent(Zenith_Entity& xEntity);

	// Full constructor for runtime creation
	Zenith_TerrainComponent(Zenith_MaterialAsset& xMaterial0, Zenith_MaterialAsset& xMaterial1, Zenith_Entity& xEntity);

	~Zenith_TerrainComponent();

private:
	static uint32_t s_uInstanceCount;
	static void IncrementInstanceCount();
	static void DecrementInstanceCount();

public:
	const Flux_VertexBuffer& GetUnifiedVertexBuffer() const { return m_xUnifiedVertexBuffer; }
	const Flux_IndexBuffer& GetUnifiedIndexBuffer() const { return m_xUnifiedIndexBuffer; }

	// Returns true once render geometry is in a usable state. Set to false
	// only when the LOW LOD load for chunk (0,0) — the canonical chunk
	// whose vertex layout sets the global stride — fails. Downstream code
	// (streaming registration, culling resource init, render-graph
	// declarations) is gated on this so an unusable terrain renders as
	// nothing instead of crashing.
	bool IsRenderGeometryUsable() const { return !m_bTerrainGeometryUnusable; }

	// Physics geometry can be absent if every chunk's source mesh failed to
	// load. Callers that dereference GetPhysicsMeshGeometry() must gate on
	// this query — the alternative is a crash on the first physics body
	// build for the terrain.
	bool HasPhysicsGeometry() const { return m_pxPhysicsGeometry != nullptr; }
	const Flux_MeshGeometry& GetPhysicsMeshGeometry() const { return *m_pxPhysicsGeometry; }
	// Material accessors (4-material palette)
	static constexpr u_int TERRAIN_MATERIAL_COUNT = 4;
	Zenith_MaterialAsset* GetMaterial(u_int uIndex) const { Zenith_Assert(uIndex < TERRAIN_MATERIAL_COUNT, "Invalid material index"); return m_axMaterials[uIndex].GetDirect(); }
	MaterialHandle& GetMaterialHandle(u_int uIndex) { Zenith_Assert(uIndex < TERRAIN_MATERIAL_COUNT, "Invalid material index"); return m_axMaterials[uIndex]; }

	// Splatmap texture (RGBA8, weights for 4 materials)
	Zenith_TextureAsset* GetSplatmapTexture() const { return Zenith_AssetRegistry::Get<Zenith_TextureAsset>(m_xSplatmap.GetPath()); }
	TextureHandle& GetSplatmapHandle() { return m_xSplatmap; }

	// Backward compatibility wrappers
	Zenith_MaterialAsset* GetMaterial0() const { return m_axMaterials[0].GetDirect(); }
	Zenith_MaterialAsset* GetMaterial1() const { return m_axMaterials[1].GetDirect(); }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }
	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// ========== GPU-Driven Culling API ==========

	/**
	 * Initialize GPU culling resources for this terrain component
	 * Allocates GPU buffers and builds chunk AABB + LOD metadata
	 * Called automatically by the full constructor after mesh loading
	 */
	void InitializeCullingResources();

	/**
	 * Destroy GPU culling resources
	 * Called automatically by destructor
	 */
	void DestroyCullingResources();

	/**
	 * Upload this frame's camera frustum planes + position to the per-component
	 * frustum-planes constant buffer. Called once per frame from
	 * g_xEngine.Terrain().PreRenderUpdate (Prepare phase) so the host transfer
	 * write is colocated with the chunk-data upload — both are then visible
	 * to the render-graph barrier synthesiser via MarkBufferHostWritten.
	 *
	 * @param xViewProjMatrix Camera view-projection matrix for frustum extraction
	 */
	void UploadFrustumPlanesForFrame(const Zenith_Maths::Matrix4& xViewProjMatrix);

	/**
	 * Update culling and LOD selection for this terrain component
	 * Records a compute dispatch to the provided command list
	 *
	 * IMPORTANT: Assumes the terrain culling compute pipeline is already bound by Flux_Terrain
	 * before calling this method. This method only records dispatch commands and buffer bindings.
	 *
	 * @param xCmdList Command list to record dispatch commands into
	 */
	void UpdateCullingAndLod(Flux_CommandList& xCmdList);

	/**
	 * Get the indirect draw buffer for rendering
	 * Contains VkDrawIndexedIndirectCommand structs written by the compute shader
	 */
	const Flux_IndirectBuffer& GetIndirectDrawBuffer() const { return m_xIndirectDrawBuffer; }

	/**
	 * Get the visible chunk count buffer (for indirect draw count)
	 */
	const Flux_IndirectBuffer& GetVisibleCountBuffer() const { return m_xVisibleCountBuffer; }

	/**
	 * Get the maximum number of draw commands (= total chunks)
	 */
	uint32_t GetMaxDrawCount() const { return TOTAL_CHUNKS; }

	/**
	 * Get the LOD level buffer for visualization
	 */
	Flux_ReadWriteBuffer& GetLODLevelBuffer() { return m_xLODLevelBuffer; }

	/**
	 * Update chunk LOD allocations in GPU buffer based on current streaming manager state
	 * Called each frame after streaming manager updates
	 */
	void UpdateChunkLODAllocations();

//private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	MaterialHandle m_axMaterials[4];
	TextureHandle m_xSplatmap;

	// ========== Unified Terrain Buffers (owned by this component) ==========
	// Contains LOW LOD (always-resident) data at the beginning, followed by streaming space for HIGH LOD
	// These buffers are registered with Flux_TerrainStreamingManager for LOD streaming
	Flux_VertexBuffer m_xUnifiedVertexBuffer;
	Flux_IndexBuffer m_xUnifiedIndexBuffer;

	// Buffer sizes and layout information
	uint64_t m_ulUnifiedVertexBufferSize = 0;
	uint64_t m_ulUnifiedIndexBufferSize = 0;
	uint32_t m_uVertexStride = 0;
	uint32_t m_uLowLODVertexCount = 0;   // Vertices reserved for LOW LOD at buffer start
	uint32_t m_uLowLODIndexCount = 0;    // Indices reserved for LOW LOD at buffer start

	// Set by LoadAndCombineLowLODChunks when chunk (0,0)'s LOW LOD source
	// fails to load. Without (0,0) we have no canonical vertex layout, so
	// the rest of the render geometry pipeline (unified buffers, streaming
	// registration, culling resources) is intentionally short-circuited.
	bool m_bTerrainGeometryUnusable = false;

	// ========== GPU-Driven Culling State ==========
	bool m_bCullingResourcesInitialized = false;

	// Per-component streaming state. Heap-allocated by the constructors
	// (forward-declared type so this header doesn't need the full struct).
	// Owned by this component — the manager keeps a non-owning pointer to
	// the same instance in its registry / primary slot. Destroyed in the
	// destructor after UnregisterTerrainBuffers(this) takes it out of the
	// registry so cross-thread access via the manager can't see a dead
	// state.
	Flux_TerrainStreamingState* m_pxStreamingState = nullptr;

	// GPU buffers for culling
	// Frame-indexed (one Flux_Buffer per frame in flight). The buffer is
	// rebuilt + host-uploaded every frame in UpdateChunkLODAllocations, so
	// frame N+1's CPU write to the underlying memory must not race against
	// frame N's GPU compute read. A single shared buffer would race here:
	// even though the per-frame fence at BeginFrame guarantees the slot's
	// previous use is complete, slot K's frame N+2 CPU work runs concurrently
	// with slot K+1's frame N+1 GPU work — and they hit the same shared
	// chunk-data memory. Frame indexing closes that race entirely (one buffer
	// per frame slot, CPU only ever writes the slot whose fence we just
	// waited on). Memory residency is host-visible (see
	// Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer), so the
	// upload skips the staging buffer too — eliminating the staging-reuse
	// race that the previous staged-upload path was exposed to.
	Flux_DynamicReadWriteBuffer m_xChunkDataBuffer;
	Flux_IndirectBuffer m_xIndirectDrawBuffer;      // Indirect draw commands (written by compute)
	Flux_DynamicConstantBuffer m_xFrustumPlanesBuffer; // Camera frustum + position (read-only in compute)
	Flux_IndirectBuffer m_xVisibleCountBuffer;      // Atomic counter for visible chunks
	Flux_ReadWriteBuffer m_xLODLevelBuffer;         // LOD level for each chunk (visualization)

	// Helper methods for culling
	void BuildChunkData();
	void ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Zenith_FrustumPlaneGPU* pxOutPlanes);

	// Helper method to initialize render resources (called by constructor and deserialization)
	void InitializeRenderResources();
	void CalculateLowLODBufferSizes(uint32_t& uTotalVertsOut, uint32_t& uTotalIndicesOut) const;
	void LoadAndCombineLowLODChunks(uint32_t uTotalVerts, uint32_t uTotalIndices, Flux_TerrainChunkInitData* pxChunkInitData, Flux_MeshGeometry*& pxLowLODGeometryOut);
	void InitializeUnifiedBuffers(const Flux_MeshGeometry& xLowLODGeometry);

	// Helper method to load and combine all physics chunks
	void LoadCombinedPhysicsGeometry();

	// Version-dispatched material deserialization helpers. Split out of
	// ReadFromDataStream so the top-level function reads as a short version
	// table rather than 100+ lines of branching material setup.
	void AssignTerrainMaterialSlot(u_int uSlot, const std::string& strEntityName, Zenith_DataStream& xStream);
	void ReadMaterialsV3(const std::string& strEntityName, Zenith_DataStream& xStream);
	void ReadMaterialsV2(const std::string& strEntityName, Zenith_DataStream& xStream);
	void ReadMaterialsV1Legacy(Zenith_DataStream& xStream);
	void BackfillMissingMaterialSlots(const std::string& strEntityName);

#ifdef ZENITH_TOOLS
	// Editor UI — main entry point; section helpers below.
	void RenderPropertiesPanel();

private:
	void RenderTerrainCreationSection();
	void RenderTerrainRegenerationSection();
	void RenderTerrainStatisticsSection();
	void RenderDebugVisualizationSection();
	void RenderMaterialPalette();
	void RenderSplatmapSlot();

	// Cleanup helper used by Regenerate to tear down prior GPU / physics / buffer state
	// before re-exporting. Split out because the 5-step sequence is easy to get wrong.
	void CleanupPriorGenerationForRegenerate();

	// End-to-end regeneration pipeline triggered by the editor's "Regenerate
	// Terrain" button. Owns the cleanup → delete-files → export → reload-physics
	// → re-init-render sequence and updates s_strTerrainExportStatus throughout.
	void RunTerrainRegeneration(const std::string& strOutputDir);
	// Allocate fresh material asset into any empty slot in m_axMaterials, named
	// after the owning entity. Called pre-render-init during regeneration.
	void EnsureMaterialSlotsPopulated();
#endif

};
