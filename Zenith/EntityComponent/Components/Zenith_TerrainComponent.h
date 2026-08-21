#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include <string>

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
class Flux_MeshGeometry;
class Flux_VertexBuffer;
class Flux_IndexBuffer;
class Flux_IndirectBuffer;
class Flux_ReadWriteBuffer;
struct Flux_TerrainChunkInitData;
struct Flux_TerrainStreamingState;
struct Zenith_FrustumPlaneGPU;
class Zenith_Image;

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
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
	// owned state and nulls the source. (Wave 3: the Flux streaming state no longer
	// carries a Zenith_TerrainComponent back-pointer — it was never dereferenced, so
	// there is nothing to repoint on move.) Copy is deleted outright — a terrain
	// component must never be duplicated (two owners of the same GPU buffers).
	Zenith_TerrainComponent(Zenith_TerrainComponent&& xOther) noexcept;
	Zenith_TerrainComponent& operator=(Zenith_TerrainComponent&& xOther) noexcept;
	Zenith_TerrainComponent(const Zenith_TerrainComponent&) = delete;
	Zenith_TerrainComponent& operator=(const Zenith_TerrainComponent&) = delete;

private:
	friend class Zenith_UnitTests;
	friend class Zenith_TerrainEditor;

	static uint32_t s_uInstanceCount;
	static void IncrementInstanceCount();
	static void DecrementInstanceCount();
	void ReadSerializedFields(Zenith_DataStream& xStream);
	static bool TryLoadTerrainChunkSource(const std::string& strPath, uint32_t uExpectedVertexCount,
		uint32_t uExpectedIndexCount, bool bRequireNormals, Flux_MeshGeometry& xGeometryOut);

	static constexpr uint32_t uMAX_SPARSE_WARNING_SAMPLES = 8u;
	struct TerrainSparseLoadDiagnostics
	{
		bool m_bAnchorLoaded = false;
		uint32_t m_uSkippedCount = 0;
		uint32_t m_uSampleCount = 0;
		uint32_t m_auSampleX[uMAX_SPARSE_WARNING_SAMPLES] = {};
		uint32_t m_auSampleY[uMAX_SPARSE_WARNING_SAMPLES] = {};
	};
	using TerrainChunkLoadCallback = bool(*)(void*, uint32_t, uint32_t, Flux_MeshGeometry&);
	static bool CombineTerrainChunkGridCore(uint32_t uGridSize,
		uint32_t uTotalVerts, uint32_t uTotalIndices,
		TerrainChunkLoadCallback pfnLoadChunk, void* pLoadContext,
		Flux_TerrainChunkInitData* pxChunkInitData,
		Flux_MeshGeometry*& pxCombinedGeometryOut,
		TerrainSparseLoadDiagnostics& xDiagnosticsOut);
	bool LoadAndCombineLowLODChunksCore(uint32_t uGridSize,
		uint32_t uTotalVerts, uint32_t uTotalIndices,
		TerrainChunkLoadCallback pfnLoadChunk, void* pLoadContext,
		Flux_TerrainChunkInitData* pxChunkInitData,
		Flux_MeshGeometry*& pxLowLODGeometryOut,
		TerrainSparseLoadDiagnostics& xDiagnosticsOut);
	bool LoadCombinedPhysicsGeometryCore(uint32_t uGridSize,
		TerrainChunkLoadCallback pfnLoadChunk, void* pLoadContext,
		TerrainSparseLoadDiagnostics& xDiagnosticsOut);
	static void LogSparseLoadDiagnostics(const char* szSourceKind,
		const TerrainSparseLoadDiagnostics& xDiagnostics);

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

	// ===== Ground-height query (Shortfalls E8 TIER 1, ZM-D-173 / task_0515a49e) =====
	//
	// "What is the COLLISION surface height at world XZ?", answered from the COMBINED
	// PHYSICS MESH as a triangle lookup -- no Jolt, no body filtering.
	//
	// READ THE CAPITALISED WORD IN THAT QUESTION: THIS IS NOT THE GROUND THE PLAYER
	// SEES, and the difference is not a rounding error. The physics mesh is baked at
	// FOUR-METRE quads and the HIGH render mesh at ONE-METRE quads
	// (Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_VERTEX_COUNT ==
	// CalculateChunkVertexCount(4u), against uHIGH_CHUNK_VERTEX_COUNT ==
	// CalculateChunkVertexCount(1u)), and that layout header states the gap is
	// deliberate -- "collision deliberately remains lower density than the nearby HIGH
	// render mesh". What comes back is therefore the 4 m CHORD across the rendered
	// surface: BELOW the visible ground over a convex ridge, ABOVE it in a concave
	// dip, equal to it only where the ground is planar across the quad. The error is
	// bounded by local relief over 4 m, and an object placed at the returned height
	// sinks into or floats over the drawn terrain by exactly that much.
	//
	// SO IT CANNOT MEASURE A VISUAL GAP, and that rules out one caller by name.
	// ZM-D-173's door-jamb residual is *how far the jamb sits from the ground the
	// player sees* -- which is the very quantity a 4 m proxy is wrong by, so a probe
	// here answers it with an error the size of the thing being measured. Use this
	// where the question is about PHYSICS (where will a body come to rest, does a
	// capsule clear this step, is this column inside the collision world); do not use
	// it to close a seam someone can look at. A render-accurate height is Shortfalls
	// E8 TIER 2 (below), not this.
	//
	// WHY THIS API EXISTS RATHER THAN A RAYCAST, and why a caller must not quietly
	// go back to one: a raycast returns the first BODY below a point, not the
	// terrain surface, so anything standing on the ground occludes the query. That
	// is not a corner case, it is the normal case, and it is not filterable:
	//   * Zenith_Physics::Raycast / Zenith_PhysicsQuery::RaycastIgnoring take
	//     exactly ONE ignore entity, so two overlapping bodies over a column are
	//     unmeasurable, full stop;
	//   * restarting the ray below a hit does not rescue it, because an object
	//     STANDING on the ground has its underside AT the surface -- and anything
	//     deliberately embedded (a shell sunk 0.05 m so no visible gap opens) has
	//     it BELOW the surface, so the restart begins underneath the terrain.
	// The query below reads the terrain's own COLLISION SURFACE rather than a body,
	// so none of that applies: the occlusion problem is gone outright. (It buys no
	// accuracy against the RENDERED surface -- see the block above.)
	//
	// STILL TIER 1, FOR TWO REASONS NOW. (a) It reads m_pxPhysicsGeometry, which only
	// LoadCombinedPhysicsGeometry fills. A scene LOAD does fill it
	// (ReadFromDataStream calls it), so a runtime probe on a loaded terrain is
	// answered; but the editor's Add-Component path constructs the component
	// through the deserialization ctor WITHOUT ever deserializing, so a probe on a
	// freshly added terrain MISSES -- this is not yet an authoring-time query, and
	// callers must gate on the bool, always. (b) It answers the COLLISION surface,
	// per the block at the top, so it cannot answer a question about the RENDERED
	// one. TIER 2 -- keep or load the heightfield -- is what would fix BOTH: a
	// heightfield can be held without any combined mesh existing (that is TIER 2's
	// whole premise for (a)), and it is the source BOTH densities are sampled from
	// (Tools/Zenith_Tools_TerrainExport.cpp's GenerateFullTerrain takes the same
	// heightmap image and a density divisor), so it carries no chord error at all.
	// NOTE this
	// component holds no height data at runtime: it loads baked mesh chunks, and
	// Terrain/<Set>/Height.ztxtr is read only by the TOOLS editor path. Tier 2 is
	// also what would retire the measure-once-and-freeze-as-a-constant dance that
	// Games/Zenithmon/Source/World/ZM_DawnmerePlacement.h is built around, and is
	// still booked as Shortfalls E8 (Games/Zenithmon/Docs/Shortfalls.md).
	//
	// FAILURE IS NOT HEIGHT 0.0, and that distinction is the whole point. THREE
	// distinct conditions return false, and every one of them leaves fHeightOut
	// UNTOUCHED:
	//   1. ABSENT physics geometry (see HasPhysicsGeometry above) -- every chunk's
	//      source mesh failed to load, so there is nothing to scan;
	//   2. an XZ OUTSIDE the combined mesh's footprint;
	//   3. an XZ over a HOLE INSIDE that footprint. CombineTerrainChunkGridCore
	//      SKIPS any chunk whose source mesh fails to load (it records the coord in
	//      TerrainSparseLoadDiagnostics and LogSparseLoadDiagnostics warns), so a
	//      sparse or partial bake produces a combined mesh with missing chunks --
	//      and at this API a probe over one is indistinguishable from case 2.
	// A caller that reads false as "outside the world" therefore misreads a sparse
	// bake as a SMALLER world; one that reads it as height 0.0 authors geometry into
	// a hole (ZM-D-173). [[nodiscard]] is deliberate: ignoring the answer is a
	// compiler diagnostic, not a silent zero. A genuine ground height of 0.0 still
	// arrives with TRUE.
	//
	// POSITIONS ARE READ RAW AND ARE NEVER TRANSFORMED. This query uses the physics
	// mesh's own vertex positions verbatim, and so does
	// Zenith_ColliderComponent::CreateTerrainShape when it hands Jolt the same
	// triangles -- but the BODY is created at the entity's GetPosition() /
	// GetRotation(). So the number agrees with physics, and with the renderer, ONLY
	// while the terrain entity's transform is identity. That holds for every terrain
	// authored today and NOTHING ENFORCES IT: give a terrain entity a transform and
	// this query keeps answering, in the wrong space, with no diagnostic.
	//
	// Cost is a linear scan over the mesh's triangles, cut by a per-triangle XZ
	// bounding-box reject. It is O(triangles) per call and does no allocation --
	// fine for one-off probes and physics-space placement, NOT for a per-frame
	// per-agent query over a full terrain. The box is NOT purely an accelerator, and
	// saying so precisely matters: the barycentric test alone admits a thin band about
	// 1e-5 of a triangle wide OUTSIDE the triangle (the tolerance that stops a point
	// on an interior edge falling down a zero-width crack), and the box CLIPS that
	// band. That is the behaviour we want -- it is what keeps the tolerance from
	// extrapolating a height off the rim of the mesh -- but it CHANGES the answer at
	// a boundary rather than merely speeding the scan up.
	[[nodiscard]] bool TryGetGroundHeightAt(float fWorldX, float fWorldZ, float& fHeightOut) const;

	// The same query against a caller-supplied geometry. Static, so the lookup is
	// exercisable (and reusable) without a live component; a NULL pxGeometry, or
	// one carrying no CPU position/index streams, IS the absent-geometry case and
	// returns false. Out-of-line (.cpp) because Flux_MeshGeometry is only
	// forward-declared here. Everything the block above says about the COLLISION
	// surface, the three false cases and untransformed positions applies here too --
	// this is the same lookup with the geometry supplied rather than owned.
	[[nodiscard]] static bool TryGetGroundHeightFromGeometry(const Flux_MeshGeometry* pxGeometry,
		float fWorldX, float fWorldZ, float& fHeightOut);

	// The pure triangle-lookup core: find the triangle whose XZ projection contains
	// (fWorldX, fWorldZ) and interpolate BARYCENTRICALLY across it, so an interior
	// point gets the exact plane height rather than a nearest-vertex
	// approximation. Split out with raw-array parameters so it names no Flux type,
	// is testable against hand-built geometry, and is usable by anything holding an
	// indexed triangle soup. A trailing PARTIAL index triple is dropped rather than
	// read, as is any triangle with an out-of-range index or zero XZ area.
	//
	// MULTI-HIT POLICY, AND THE PRECONDITION IT RESTS ON -- read this before reusing
	// it on a soup that is not a heightfield. The FIRST containing triangle in INDEX
	// ORDER wins. That is safe for a heightfield, which has at most one triangle over
	// any XZ column: the only points two of its triangles both contain lie on a
	// shared edge or vertex, where both interpolate the same shared vertices and
	// therefore agree, so the early-out cannot make the answer depend on index order.
	// Hand it a soup with an OVERHANG -- a bridge, a cave roof, any closed mesh --
	// and that premise is false: you get whichever surface the index buffer happens
	// to list first, and index order is not a defensible *ground* policy. "The
	// highest surface below the probe" would be one; this core does not implement it
	// and does not detect the case.
	[[nodiscard]] static bool TryGetGroundHeightFromTriangles(
		const Zenith_Maths::Vector3* pxPositions, uint32_t uVertexCount,
		const uint32_t* puIndices, uint32_t uIndexCount,
		float fWorldX, float fWorldZ, float& fHeightOut);

	// Material accessors (4-material palette)
	static constexpr u_int TERRAIN_MATERIAL_COUNT = 4;
	Zenith_MaterialAsset* GetMaterial(u_int uIndex) const { Zenith_Assert(uIndex < TERRAIN_MATERIAL_COUNT, "Invalid material index"); return m_axMaterials[uIndex].GetDirect(); }
	MaterialHandle& GetMaterialHandle(u_int uIndex) { Zenith_Assert(uIndex < TERRAIN_MATERIAL_COUNT, "Invalid material index"); return m_axMaterials[uIndex]; }

	// Splatmap texture (RGBA8, weights for 4 materials)
	Zenith_TextureAsset* GetSplatmapTexture() const { return m_xSplatmap.Resolve(); }
	TextureHandle& GetSplatmapHandle() { return m_xSplatmap; }

	static bool IsValidTerrainAssetSetName(const std::string& strSet);
	static bool TryResolveTerrainAssetDirectory(const std::string& strSet, std::string& strDirectoryOut);
	bool SetTerrainAssetSet(const std::string& strSet);
	const std::string& GetTerrainAssetSet() const;
	std::string GetTerrainAssetDirectory() const;

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
	void UpdateCullingAndLod(Flux_CommandBuffer& xCmdList);

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
	bool IsTerrainInitializedForEditor() const;
	// Captureless thunk + opaque context, matching TerrainChunkLoadCallback above.
	// The lease is strictly synchronous, so the context only has to outlive the
	// enclosing call — callers stack-allocate a small struct and pass its address.
	using TerrainDirectoryOperation = bool(*)(void*, const std::string&);
	// Opens a handle-bound lease on the existing game directory, its Assets child,
	// the Terrain root, and selected target for the complete synchronous operation.
	// Missing ignored directories are created one checked segment at a time;
	// handles deny delete sharing so none can be replaced during the callback.
	static bool WithPreparedTerrainAssetDirectory(const std::string& strAssetSet,
		const std::string& strTerrainRoot, TerrainDirectoryOperation pfnOperation,
		void* pOperationContext);
	// Atomically renames one simple child filename to another while the same
	// handle-bound target lease remains active. This is the publication primitive
	// for completion markers whose final rename must not relax directory sharing.
	static bool RenamePreparedTerrainAssetFileAtomically(const std::string& strAssetSet,
		const std::string& strTerrainRoot, const std::string& strSourceFilename,
		const std::string& strDestinationFilename);
	// Empty-set textures use the legacy Assets/Textures/Terrain sibling and need
	// their own lease. Named textures delegate to the named Terrain target above.
	static bool WithPreparedTerrainTextureDirectory(const std::string& strAssetSet,
		const std::string& strTerrainRoot, TerrainDirectoryOperation pfnOperation,
		void* pOperationContext);

	// End-to-end regeneration from an in-memory heightfield (the terrain
	// editor's bake): cleanup -> delete terrain files -> ExportHeightmapFromMat
	// -> reload physics -> re-init render. Same sequence as the panel's
	// Regenerate button with the in-memory image as the export source.
	bool RegenerateFromHeightfield(const Zenith_Image& xHeightfield);

private:
	void RenderTerrainCreationSection();
	void RenderTerrainRegenerationSection();
	void RenderTerrainStatisticsSection();
	void RenderDebugVisualizationSection();
	void RenderMaterialPalette();
	void RenderSplatmapSlot();

	// Cleanup helper used only inside a held terrain-directory lease. Split out
	// because the 5-step sequence is easy to get wrong.
	void CleanupPriorGenerationForRegenerate();
	// Purely non-destructive canonical target check. Kept private so production
	// uses the project-root resolver above; Zenith_UnitTests friendship may pass
	// an isolated Build/artifacts root/target to exercise junction rejection.
	static bool ValidateTerrainAssetSetTarget(const std::string& strAssetSet,
		const std::string& strTerrainRoot, const std::string& strResolvedTarget);
	// Production wrapper holds a handle-bound directory lease for the complete
	// deletion. Zenith_UnitTests friendship reaches only the core for an isolated
	// Build/artifacts sandbox seam.
	static bool DeleteExistingTerrainFilesForAssetSet(const std::string& strAssetSet,
		const std::string& strResolvedDirectory);
	static bool DeleteExistingTerrainFilesInDirectory(const std::string& strDirectory);

	// End-to-end regeneration pipeline triggered by the editor's "Regenerate
	// Terrain" button. Owns the cleanup → delete-files → export → reload-physics
	// → re-init-render sequence and updates s_strTerrainExportStatus throughout.
	bool RunTerrainRegeneration(const std::string& strOutputDir);
	// Shared regeneration body. Non-null pxHeightfield exports from the
	// in-memory image; null falls back to the panel's heightmap path field.
	bool RunTerrainRegenerationInternal(const std::string& strOutputDir, const Zenith_Image* pxHeightfield);
	bool RunTerrainRegenerationInternalForTerrainRoot(const std::string& strTerrainRoot,
		const std::string& strOutputDir, const Zenith_Image* pxHeightfield);
	// Allocate fresh material asset into any empty slot in m_axMaterials, named
	// after the owning entity. Called pre-render-init during regeneration.
	void EnsureMaterialSlotsPopulated();
#endif

private:
	// Kept private so every mutation passes through the validating setter.
	std::string m_strTerrainAssetSet;
};
