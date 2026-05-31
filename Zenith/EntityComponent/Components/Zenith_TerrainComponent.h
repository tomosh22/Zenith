#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"

// Forward declarations only — this header includes NO Flux header (Wave-18
// ownership-relocation). The Flux GPU state (unified vertex/index buffers,
// per-frame culling buffers, the 4 GPU-layout structs, and all the terrain
// config constants) lives on the OWNING Flux side now — in
// Flux_TerrainStreamingState (Flux/Terrain/Flux_TerrainStreamingManagerImpl.h)
// and Flux_TerrainGPUStructs.h. This component is a thin handle whose public
// buffer/stride accessors are defined out-of-line in the .cpp and forward into
// *m_pxStreamingState; the accessor *signatures* only need these
// forward-declarations of the Flux buffer-wrapper types (a forward declaration
// is NOT an #include, so it introduces no cross-layer coupling — the layering
// gate scans #include edges, not forward decls).
class Flux_CommandList;
class Flux_MeshGeometry;
class Flux_VertexBuffer;
class Flux_IndexBuffer;
class Flux_IndirectBuffer;
class Flux_ReadWriteBuffer;
struct Flux_TerrainChunkInitData;
struct Flux_TerrainStreamingState;
struct Zenith_FrustumPlaneGPU;

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

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

	// The component owns two raw pointers (m_pxStreamingState,
	// m_pxPhysicsGeometry) plus the material/splat handles, and has a user
	// destructor that frees them. The implicitly-generated move would be a
	// shallow pointer copy, so a pool relocation (swap-and-pop / Grow) would
	// double-free both the streaming state and the physics geometry when the
	// moved-from temporary destructs. Define an explicit move that STEALS the
	// owned state, nulls the source, and REPOINTS the streaming state's owner
	// back-pointer at the moved-to component (the manager registry / per-frame
	// resolver dereference m_pxOwner, so it must always point at the live
	// component). Copy is deleted outright — a terrain component must never be
	// duplicated (two owners of the same GPU buffers).
	Zenith_TerrainComponent(Zenith_TerrainComponent&& xOther) noexcept;
	Zenith_TerrainComponent& operator=(Zenith_TerrainComponent&& xOther) noexcept;
	Zenith_TerrainComponent(const Zenith_TerrainComponent&) = delete;
	Zenith_TerrainComponent& operator=(const Zenith_TerrainComponent&) = delete;

private:
	static uint32_t s_uInstanceCount;
	static void IncrementInstanceCount();
	static void DecrementInstanceCount();

public:
	// Buffer accessors forward into the owning Flux_TerrainStreamingState.
	// Out-of-line (.cpp) because the buffer-wrapper types are only
	// forward-declared in this header — the bodies need the full state type,
	// which only the .cpp pulls in. Behaviour is identical to the previous
	// inline by-value-member accessors.
	const Flux_VertexBuffer& GetUnifiedVertexBuffer() const;
	const Flux_IndexBuffer& GetUnifiedIndexBuffer() const;

	// Vertex stride of the unified buffer (bytes). Added so render-side
	// consumers (RenderTest.cpp) that previously read m_uVertexStride directly
	// keep a stable accessor now that the field lives on the streaming state.
	uint32_t GetVertexStride() const;

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
	// Out-of-line (.cpp): dereferencing the forward-declared Flux_MeshGeometry
	// to return a reference needs the full type.
	const Flux_MeshGeometry& GetPhysicsMeshGeometry() const;
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
	 * Forwards into the owning Flux_TerrainStreamingState (out-of-line).
	 */
	const Flux_IndirectBuffer& GetIndirectDrawBuffer() const;

	/**
	 * Get the visible chunk count buffer (for indirect draw count)
	 * Forwards into the owning Flux_TerrainStreamingState (out-of-line).
	 */
	const Flux_IndirectBuffer& GetVisibleCountBuffer() const;

	/**
	 * Get the maximum number of draw commands (= total chunks)
	 * Out-of-line (.cpp): the value is Flux_TerrainConfig::TOTAL_CHUNKS, which
	 * this header no longer names.
	 */
	uint32_t GetMaxDrawCount() const;

	/**
	 * Get the LOD level buffer for visualization
	 * Forwards into the owning Flux_TerrainStreamingState (out-of-line).
	 */
	Flux_ReadWriteBuffer& GetLODLevelBuffer();

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

	// Set by LoadAndCombineLowLODChunks when chunk (0,0)'s LOW LOD source
	// fails to load. Without (0,0) we have no canonical vertex layout, so
	// the rest of the render geometry pipeline (unified buffers, streaming
	// registration, culling resources) is intentionally short-circuited.
	bool m_bTerrainGeometryUnusable = false;

	// Per-component streaming state. Heap-allocated by the constructors
	// (forward-declared type so this header doesn't need the full struct).
	// Owned by this component. As of Wave-18 it ALSO owns the relocated Flux
	// GPU state: the unified vertex/index buffers, the per-frame culling
	// buffers (chunk-data / indirect / frustum / visible-count / LOD-level),
	// the unified-buffer scalars (sizes / stride / LOW-LOD counts) and the
	// m_bCullingResourcesInitialized flag. The manager keeps a non-owning
	// pointer to the same instance in its registry; the per-frame render path
	// resolves it via the m_pxOwner back-pointer (O(1), no map lookup).
	// Destroyed in the destructor after UnregisterTerrainBuffers(this) takes
	// it out of the registry so cross-thread access via the manager can't see
	// a dead state. The component's public buffer/stride accessors forward
	// into *m_pxStreamingState.
	Flux_TerrainStreamingState* m_pxStreamingState = nullptr;

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
