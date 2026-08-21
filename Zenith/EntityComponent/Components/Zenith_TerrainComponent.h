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

	// One physics chunk's real-vertex-data XZ footprint, recorded by
	// CombineTerrainChunkGridCore at the exact point it appends that chunk's
	// triangles to the combined physics mesh, plus the [m_uFirstIndex,
	// m_uFirstIndex + m_uIndexCount) run those triangles occupy in the combined
	// index buffer. Declared PUBLIC and this early (rather than beside the
	// ground-height query below, where the reject built from it is explained in
	// full) so CombineTerrainChunkGridCore's private declaration further down can
	// name it as a parameter type, and so a hand-built span array is
	// constructible from a pure unit test -- a nested type's accessibility
	// follows wherever it is FIRST declared, so it cannot be forward-declared
	// private and "made public" by a later redefinition.
	//
	// BOTH FIELDS ARE OBSERVED, NEVER COMPUTED FROM A CHUNK COORDINATE, AND THAT
	// IS THE WHOLE POINT OF THE TYPE. A sparse bake -- one where some chunk's
	// source mesh failed to load -- is a supported, warned-about state
	// (TerrainSparseLoadDiagnostics / LogSparseLoadDiagnostics), and
	// CombineTerrainChunkGridCore reserves NOTHING for a chunk it skipped: the
	// next chunk's triangles are appended immediately after the previous
	// SURVIVING chunk's. So the mapping from grid coordinate to index run is not
	// a formula. Deriving m_uFirstIndex as, say, (x * gridSize + y) * indicesPerChunk
	// would be correct for a dense bake and WRONG for every chunk after the first
	// hole, by exactly (skipped so far) * indicesPerChunk -- which either points
	// the query at a DIFFERENT chunk's triangles (a wrong ground height, or a
	// false "no ground" because that chunk's triangles do not cover this XZ) or
	// runs past the end of the index buffer, where
	// TryGetGroundHeightFromTrianglesChunked's malformed-span guard silently
	// skips it and reports no ground over ground that exists. Likewise the XZ
	// bounds come from that chunk's own vertex positions rather than its nominal
	// world cell, so a chunk whose real data does not fill its cell still gets a
	// bound that contains every one of its triangles.
	//
	// ALLOCATION CONTRACT, and why there are deliberately no default member
	// initializers. The table is one bulk block from
	// Zenith_MemoryManagement::Allocate (LoadCombinedPhysicsGeometryCore) --
	// malloc underneath, so NO constructor runs over it and an NSDMI here would
	// be dead code that reads as a guarantee the memory does not carry. Entries
	// are therefore INDETERMINATE until written: exactly the first
	// *puChunkSpanCountOut of them are initialised, in append order, and only
	// that prefix may be read. Every consumer (TryGetGroundHeightFromTrianglesChunked,
	// TryGetGroundHeightAt) is bounded by that count for this reason. The type
	// stays an all-scalar aggregate so the bulk allocation is legitimate and so a
	// test can brace-initialise one.
	struct PhysicsChunkSpan
	{
		float m_fMinX;
		float m_fMaxX;
		float m_fMinZ;
		float m_fMaxZ;
		uint32_t m_uFirstIndex;
		uint32_t m_uIndexCount;
	};

private:
	friend class Zenith_UnitTests;
	friend class Zenith_TerrainEditor;
	// Defined ONLY in Zenith_TerrainComponent.Tests.inl, which this component's
	// own .cpp includes. It drives CombineTerrainChunkGridCore directly -- with a
	// load callback that deliberately fails one middle chunk -- so the span
	// RECORDER has coverage rather than only the query that consumes a span
	// table. That needs the private core and the private diagnostics type, and
	// the .inl is at namespace scope in the .cpp rather than a member of
	// Zenith_UnitTests (whose Core .inl cannot host it: this component's tests
	// live beside the component). A friend declaration of a type that is never
	// defined in a given TU is legal and affects nothing but access.
	friend struct Zenith_TerrainChunkSpanRecorderTests;
	// Defined ONLY in Zenith_TerrainComponent.Tests.inl, ZENITH_TOOLS-gated --
	// both the struct's own definition there and its one reason to exist,
	// CleanupPriorGenerationForRegenerate (declared further down, inside this
	// header's own "private:" block under #ifdef ZENITH_TOOLS). ZEN-4
	// fix-forward coverage: the review that landed the ZEN-2 physics-geometry
	// lifetime fix found the fixed line had ZERO execution coverage -- the one
	// caller-side test (Zenith_UnitTests.Tests.inl's preflight-rejection block)
	// explicitly proves cleanup is NEVER reached, so it pins the opposite of
	// what this friend's test exercises. Reaches m_pxPhysicsGeometry /
	// m_pxPhysicsChunkSpans / m_uPhysicsChunkSpanCount plus
	// LoadCombinedPhysicsGeometryCore (to populate them for real, the same
	// in-memory-fixture pattern Zenith_TerrainChunkSpanRecorderTests above
	// uses) and CleanupPriorGenerationForRegenerate itself (to free them).
	// Declared unconditionally, matching Zenith_TerrainEditor above -- a friend
	// of a type this TU never defines (not building this .cpp at all, or
	// building it without ZENITH_TOOLS) is legal and affects nothing, per the
	// same reasoning already given for Zenith_TerrainChunkSpanRecorderTests.
	friend struct Zenith_TerrainCleanupPhysicsGeometryTests;

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
		TerrainSparseLoadDiagnostics& xDiagnosticsOut,
		// Optional span table. Captures, in append order, the XZ footprint (from
		// each chunk's OWN real vertex data) and the resulting
		// [firstIndex, indexCount) run of every chunk actually appended to the
		// combined mesh -- a chunk that was SKIPPED contributes no entry at all,
		// not a placeholder, which is exactly why the offsets cannot be derived
		// from a grid coordinate (see PhysicsChunkSpan). Only the physics combine
		// (LoadCombinedPhysicsGeometryCore) passes one -- the LOW LOD/render
		// combine has no per-chunk query to accelerate.
		//
		// THE THREE ARE ONE PARAMETER IN THREE PIECES, and the core ENFORCES that
		// rather than describing it: the table is written only when BOTH pointers
		// are non-null, and a write is refused (with an assert) once
		// *puChunkSpanCountOut reaches uChunkSpanCapacity, so a caller that sized
		// its buffer wrong gets a diagnostic instead of a heap overwrite. Pass the
		// element count of pxChunkSpansOut, which must be at least
		// uGridSize*uGridSize -- the core appends at most one span per grid cell.
		// *puChunkSpanCountOut is reset to 0 and then counts up as chunks are
		// appended.
		//
		// All three are defaulted rather than mandatory so the existing
		// Zenith_UnitTests.Tests.inl call sites (Core/, friended onto this core to
		// pin the sparse/anchor/dense combine behaviour directly) keep compiling
		// unchanged -- they exercise the combine, not the ground-height query, and
		// have no reason to pass a span table.
		PhysicsChunkSpan* pxChunkSpansOut = nullptr, uint32_t uChunkSpanCapacity = 0u,
		uint32_t* puChunkSpanCountOut = nullptr);
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
	// Cost, since the chunk-granularity reject (ZEN-2): TryGetGroundHeightAt first
	// rejects against each combined PHYSICS chunk's OWN real-vertex-data XZ bounds
	// -- a handful of float compares per chunk, at most CHUNK_GRID_SIZE^2 (4096)
	// of them -- and only then runs the ORIGINAL per-triangle scan below,
	// restricted to the one (or, exactly at a shared seam, two) chunk(s) whose
	// bounds contain the point. That is O(chunks) + O(triangles in the matching
	// chunk) rather than O(every triangle in the mesh): about 512 triangles
	// scanned (one physics chunk's worth) instead of up to ~2.1M (4096 chunks x
	// 512 triangles each), with the per-triangle maths below completely
	// unchanged. Still no allocation PER QUERY -- the per-chunk table
	// (PhysicsChunkSpan / m_pxPhysicsChunkSpans) is built ONCE, when the physics
	// geometry itself is combined (LoadCombinedPhysicsGeometryCore), not on each
	// call.
	//
	// The coarser per-chunk box can never clip TIGHTER than the per-triangle one
	// immediately below: a triangle's own XZ bounds are always a subset of its
	// owning chunk's (the chunk bound is generated from the SAME real vertex
	// data, a superset of every one of its triangles' vertices), so restricting
	// the scan to chunks whose bound contains the point can never exclude a
	// triangle the unrestricted scan would have found -- see
	// TryGetGroundHeightFromTrianglesChunked below for the argument in full, and
	// PhysicsChunkSpan (near the constructors) for why BOTH the bounds and the
	// index offsets are observed at append time rather than computed from a chunk
	// coordinate, which is unsafe under a sparse bake.
	//
	// The static TryGetGroundHeightFromGeometry / TryGetGroundHeightFromTriangles
	// entry points below stay UNACCELERATED by design: they have no owning
	// component to hold a per-chunk table, so a caller reaching them directly (a
	// hand-built mesh, a test) still pays the full per-triangle scan, cut only by
	// the per-triangle XZ bounding-box reject described there. That reject is NOT
	// purely an accelerator either, and saying so precisely matters: the
	// barycentric test alone admits a thin band about 1e-5 of a triangle wide
	// OUTSIDE the triangle (the tolerance that stops a point on an interior edge
	// falling down a zero-width crack), and the box CLIPS that band. That is the
	// behaviour we want -- it is what keeps the tolerance from extrapolating a
	// height off the rim of the mesh -- but it CHANGES the answer at a boundary
	// rather than merely speeding the scan up.
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

	// PhysicsChunkSpan (one physics chunk's real-vertex-data XZ footprint plus
	// its run in the combined index buffer) is declared PUBLIC further up, near
	// the constructors -- it must be fully visible before CombineTerrainChunkGridCore's
	// PRIVATE declaration below can name it as a parameter type, and a nested
	// type's accessibility follows wherever it is first declared. Its doc comment
	// there is where both of its contracts are written out in full: WHY both
	// fields are observed at append time rather than computed from a chunk
	// coordinate (the sparse-bake trap), and the allocation contract (one bulk
	// Allocate, no constructor, hence deliberately no default member
	// initializers, hence only the first *puChunkSpanCountOut entries may be
	// read).
	//
	// The chunk-accelerated counterpart of TryGetGroundHeightFromTriangles just
	// above: for each recorded span, an inclusive four-compare XZ reject against
	// ITS OWN real-vertex-data bounds, and only on a match does it run the
	// UNMODIFIED per-triangle scan (TryGetGroundHeightFromTriangles), restricted
	// to that span's [m_uFirstIndex, m_uFirstIndex + m_uIndexCount) run. Every
	// other argument (positions, vertex count, the full index buffer) is passed
	// straight through unrestricted, so an index inside the matching span's run
	// still resolves against the SAME full position array the unrestricted scan
	// would use.
	//
	// PROVABLY THE SAME ANSWER AS THE UNRESTRICTED SCAN, for two reasons taken
	// together. (1) A triangle's own XZ bounds are always a SUBSET of its owning
	// chunk's span bounds -- the span is generated from that chunk's full vertex
	// set, a superset of any one triangle's three vertices -- so a span whose
	// bounds reject the point can only be rejecting chunks that provably contain
	// no containing triangle; the true first-hit triangle's chunk can never be
	// one of them. (2) Candidate spans are tried in ARRAY order and, within one,
	// triangles are tried in their existing INDEX order (TryGetGroundHeightFromTriangles
	// is untouched), so restricting to the candidate set and concatenating their
	// runs in span order reproduces exactly the index-order scan the
	// unrestricted function performs, restricted to a set that is proven to
	// contain the answer. At a shared chunk seam, more than one span's bounds
	// contain the point (both chunks' real vertex data include the shared edge),
	// and TryGetGroundHeightFromTriangles's own documented multi-hit policy
	// already covers that case: both sides interpolate the same shared vertices
	// and agree, so it does not matter which candidate span's scan reaches it
	// first. A point inside no span's bounds provably lies over either a hole in
	// a sparse bake or outside the mesh's footprint -- exactly the two
	// TryGetGroundHeightAt already documents as returning false.
	[[nodiscard]] static bool TryGetGroundHeightFromTrianglesChunked(
		const Zenith_Maths::Vector3* pxPositions, uint32_t uVertexCount,
		const uint32_t* puIndices, uint32_t uIndexCount,
		const PhysicsChunkSpan* pxChunkSpans, uint32_t uChunkSpanCount,
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

private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	// Per-chunk acceleration for TryGetGroundHeightAt (see its cost note above).
	// Built ONCE by LoadCombinedPhysicsGeometryCore. Non-null only if a physics
	// combine both succeeded AND the accelerator's own (much smaller) allocation
	// succeeded; TryGetGroundHeightAt falls back to the unrestricted,
	// unaccelerated scan when this is null, so its absence changes speed, never
	// correctness.
	//
	// THE TABLE'S LIFETIME MATCHES THE MESH'S: every site that discards
	// m_pxPhysicsGeometry also frees this table, so a stale table can never
	// describe a mesh that no longer exists. That was NOT always true --
	// Zenith_TerrainComponent_Editor.cpp's CleanupPriorGenerationForRegenerate
	// used to delete and null the geometry and leave the table standing, safe
	// only because TryGetGroundHeightAt gates on the null geometry before ever
	// consulting the table, and because the next LoadCombinedPhysicsGeometryCore
	// call would clear the table unconditionally before combining again. It now
	// frees the table itself, right where it frees the geometry, so that
	// two-step reasoning is no longer load-bearing.
	//
	// LoadCombinedPhysicsGeometryCore -- the only site that assigns a freshly
	// combined mesh -- still clears the table unconditionally before every
	// combine attempt, so a table from an earlier, possibly differently-sparse,
	// attempt is never consulted against a later one. TryGetGroundHeightAt still
	// gates on m_pxPhysicsGeometry == nullptr first regardless of the table's
	// state. The move constructor / move assignment transfer the pointer and
	// its count together for the same reason.
	//
	// ZEN-5: genuinely private as of this commit. Every write site is this
	// class's own member functions (Zenith_TerrainComponent.cpp /
	// Zenith_TerrainComponent_Editor.cpp), verified repo-wide before this
	// change -- so the lockstep invariant stated above ("every site that
	// discards m_pxPhysicsGeometry also frees this table") now has the
	// compiler behind it instead of resting on this comment alone.
	PhysicsChunkSpan* m_pxPhysicsChunkSpans = nullptr;
	uint32_t m_uPhysicsChunkSpanCount = 0u;
	MaterialHandle m_axMaterials[4];
	TextureHandle m_xSplatmap;

	// Set by LoadAndCombineLowLODChunks when chunk (0,0)'s LOW LOD source
	// fails to load. Without (0,0) we have no canonical vertex layout, so
	// the rest of the render geometry pipeline (unified buffers, streaming
	// registration, culling resources) is intentionally short-circuited.
	bool m_bTerrainGeometryUnusable = false;

public:
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
	//
	// DELIBERATELY PUBLIC (ZEN-5) -- the one member of this block that the
	// `//private:` this ticket fixes could NOT simply cover, and that is a
	// recorded decision, not an oversight. Three external, non-friend TUs
	// dereference it directly rather than through an accessor --
	// Games/RenderTest/RenderTest.cpp, Games/CityBuilder/Source/CB_RoadTerrain.cpp
	// and Games/RenderTest/Tests/TerrainEditorSmoke.cpp -- and
	// Flux/Terrain/Flux_TerrainStreamingManagerImpl.h documents the same
	// contract from the manager's side ("Callers that hold a
	// Zenith_TerrainComponent pass its (public) m_pxStreamingState"). Making
	// this private would mean adding an accessor AND rewriting every one of
	// those call sites across two different games plus an engine automated
	// test -- none of them a Zenith_TerrainComponent file -- or friending
	// three unrelated TUs one at a time, which is the same problem with extra
	// steps. Left public on purpose; narrowing it is a separate ticket with
	// its own file list, not a side effect of this one.
	Flux_TerrainStreamingState* m_pxStreamingState = nullptr;

private:
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
	// Frees m_pxPhysicsChunkSpans (if any) and resets m_uPhysicsChunkSpanCount to
	// 0. Called before every fresh combine attempt (a retry must not consult a
	// span table built from a previous, possibly differently-sparse, combine)
	// and everywhere m_pxPhysicsGeometry itself is discarded within this file.
	//
	// ZEN-5: genuinely PRIVATE now, not merely "within this file" by
	// convention -- a private member function of this class, reachable only
	// from this class's own member functions. Every direct caller is exactly
	// that (this .cpp's constructors/destructor/move-ops/loaders and
	// Zenith_TerrainComponent_Editor.cpp's CleanupPriorGenerationForRegenerate).
	// No friend calls it directly: Zenith_TerrainCleanupPhysicsGeometryTests
	// (below) reaches it only indirectly, through
	// CleanupPriorGenerationForRegenerate (itself private); the sibling
	// Zenith_TerrainChunkSpanRecorderTests never reaches it at all.
	void FreePhysicsChunkSpans();

	// Version-dispatched material deserialization helpers. Split out of
	// ReadFromDataStream so the top-level function reads as a short version
	// table rather than 100+ lines of branching material setup.
	void AssignTerrainMaterialSlot(u_int uSlot, const std::string& strEntityName, Zenith_DataStream& xStream);
	void ReadMaterialsV3(const std::string& strEntityName, Zenith_DataStream& xStream);
	void ReadMaterialsV2(const std::string& strEntityName, Zenith_DataStream& xStream);
	void ReadMaterialsV1Legacy(Zenith_DataStream& xStream);
	void BackfillMissingMaterialSlots(const std::string& strEntityName);

#ifdef ZENITH_TOOLS
public:
	// Editor UI entry points reached from OUTSIDE this class (and outside its
	// friends), so -- unlike the helper methods above -- they stay public
	// rather than following that block into private: (ZEN-5, verified
	// repo-wide before this change):
	//   * RenderPropertiesPanel() is required public by the generic
	//     component-panel dispatch every component type participates in --
	//     ZenithECS/Internal/Zenith_ComponentPool.h's Zenith_Component concept
	//     checks `t.RenderPropertiesPanel()` on every registered component
	//     type, and EntityComponent/Zenith_ComponentEditorRegistry.h's "Add
	//     Component" panel calls it through that same generic interface.
	//     Private here would be a Terrain-only special case in a pattern
	//     every other component follows;
	//   * IsTerrainInitializedForEditor() is read directly by
	//     Zenith/Editor/Zenith_EditorAutomation.cpp;
	//   * WithPreparedTerrainAssetDirectory() and
	//     RenamePreparedTerrainAssetFileAtomically() are called directly by
	//     Games/Zenithmon/Source/World/ZM_TerrainAuthoring.cpp.
	// Zenith_TerrainEditor's own calls to all four are already covered by the
	// friend declaration near the top of this class -- it is specifically the
	// callers outside that friend list, spanning two games plus one engine
	// automation TU, that keep this sub-block public.
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

private:
	// Unlike the four methods above, these two have zero callers outside this
	// class and Zenith_TerrainEditor (already a friend) -- verified repo-wide
	// (ZEN-5) -- so they follow the helper methods above into private rather
	// than joining the public sub-block.
	//
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
