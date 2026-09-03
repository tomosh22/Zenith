#pragma once

//=============================================================================
// Flux_GrassImpl — the GPU-driven grass feature.
//
// NOTHING about a blade is persisted. Every frame the pipeline runs
//
//   Reset (CS)  ->  Placement (CS)  ->  IndirectFixup (CS)  ->  indirect draws
//                                                          ->  Displacement (CS)
//
// and regenerates every blade from scratch. That is affordable because a blade
// is a pure function of its lattice node (Flux_GrassTypes.h keys all per-blade
// randomness off HashCoords), and it is REQUIRED because a blade that changed
// identity between frames would flicker under TAA. There is no CPU instance
// array, no chunk grid and no upload of blade data — the CPU's whole job is to
// choose which TILES to dispatch and to stage the constants.
//
// The CPU keeps three maps (coverage / type / height) in the exact quantized
// form the GPU textures hold, so the CPU query surface below and the placement
// CS read the same bytes rather than two copies that can drift.
//
// The ONE exception to "nothing persists" is the DISPLACEMENT field — a 256^2
// ping-pong of trail maps that decays across frames. It is deliberately not blade
// state: it is a property of the GROUND, sampled by whichever blade happens to
// stand on it this frame, so it never gives a blade an identity that could change
// between frames.
//
// LIFETIME. GPU resources are created ONCE in Initialise at fixed capacity and
// destroyed only in Shutdown — never per-toggle, never per-scene. The render
// graph keys barriers by C++ object address, so the wrapper objects must also
// outlive every graph rebuild. ClearSceneData drops scene state only; the type
// table, the wind state and every byte of VRAM survive it.
//=============================================================================

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Vegetation/Flux_GrassTypes.h"
#include "Flux/Vegetation/Flux_GrassTypeTable.h"
#include "Flux/InstancedMeshes/Flux_InstanceCulling.h"   // Flux_FrustumPlaneGPU (shared plane layout)
#include "AssetHandling/Zenith_AssetHandle.h"             // TextureHandle pins for the type table's textures
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <string>

// The gather reads the camera + cascade view-projections straight out of the
// registry; by reference only, so the declaration is enough.
class Flux_RenderViewRegistry;

//=============================================================================
// GPU contract — PINNED against Zenith/Flux/Shaders/Vegetation/*.slang.
//
// The partition bases and caps are the dangerous half: the placement CS writes a
// survivor at base[slot] + localIndex, so a capacity that disagrees with
// kauGRASS_PARTITION_BASE/CAP does not overflow the buffer, it writes one
// partition's blades into the NEXT partition's range and the cascade draws the
// camera's blades.
//=============================================================================

constexpr u_int uFLUX_GRASS_BLADE_POOL_CAPACITY    = 1048576u;   // blade records
constexpr u_int uFLUX_GRASS_VISIBLE_INDEX_CAPACITY = 1572864u;   // uints, four partitions
constexpr u_int uFLUX_GRASS_INDIRECT_SLOT_COUNT    = 16u;        // 4 live + 12 reserved
constexpr u_int uFLUX_GRASS_INDIRECT_WORDS         = 5u;         // VkDrawIndexedIndirectCommand
constexpr u_int uFLUX_GRASS_INDIRECT_STRIDE        = uFLUX_GRASS_INDIRECT_WORDS * 4u;   // bytes
constexpr u_int uFLUX_GRASS_LIVE_SLOT_COUNT        = 4u;

constexpr u_int uFLUX_GRASS_SLOT_CAMERA_HI = 0u;
constexpr u_int uFLUX_GRASS_SLOT_CAMERA_LO = 1u;
constexpr u_int uFLUX_GRASS_SLOT_CASCADE_0 = 2u;
constexpr u_int uFLUX_GRASS_SLOT_CASCADE_1 = 3u;
constexpr u_int uFLUX_GRASS_MAX_CASCADES   = 2u;   // slots 2-3; a third cascade has no partition

// Per-view frustum slots in the placement constants: camera, cascade 0, cascade 1,
// six INWARD planes each, view-major.
constexpr u_int uFLUX_GRASS_FRUSTUM_COUNT = 3u;
constexpr u_int uFLUX_GRASS_FRUSTUM_PLANES = uFLUX_GRASS_FRUSTUM_COUNT * 6u;
static_assert(uFLUX_GRASS_FRUSTUM_COUNT == 1u + uFLUX_GRASS_MAX_CASCADES,
	"frustum slot 0 is the camera and slots 1.. are the cascades one-for-one — a third cascade "
	"would need both a frustum slot and a partition, and the two must grow together");

// One thread per lattice cell; the placement CS decodes TILE-MAJOR, so the
// dispatch is (tileCount * cells-per-tile) threads in 64-wide groups.
constexpr u_int uFLUX_GRASS_TILE_CELL_COUNT = Flux_GrassConfig::uTILE_CELLS * Flux_GrassConfig::uTILE_CELLS;
constexpr u_int uFLUX_GRASS_PLACEMENT_GROUP_SIZE = 64u;

constexpr u_int uFLUX_GRASS_MAX_MOVERS = 64u;

// One thread per displacement texel, 8x8 groups (Flux_Grass_Displacement.slang).
constexpr u_int uFLUX_GRASS_DISPLACEMENT_GROUP_SIZE = 8u;

// Flux_GrassWindBlock (Flux_GrassCommon.slang). Three float4s rather than the
// all-scalar Flux_WindConstants, whose std140 constant-buffer size differs from
// its C++ sizeof — three vectors are unambiguous under every layout rule.
struct Flux_GrassWindBlockGPU
{
	Zenith_Maths::Vector4 m_xDirXZ_Strength_Time{ 0.0f };     //  0 : xy = heading, z = strength, w = time
	Zenith_Maths::Vector4 m_xFreq_Scroll_Gust_Seed{ 0.0f };   // 16 : x = 1/m, y = m/s, z = exponent, w = asfloat(seed)
	Zenith_Maths::Vector4 m_xDetailFreq_Speed_Amp{ 0.0f };    // 32 : x = 1/m, y = rad/s, z = tip amplitude
};
static_assert(sizeof(Flux_GrassWindBlockGPU) == 48,
	"the wind block is three float4s — it is also the whole GrassPrevWindConstants CB");

// GrassPlacementConstantsLayout (Flux_Grass_Placement.slang).
struct Flux_GrassPlacementConstantsGPU
{
	Flux_FrustumPlaneGPU   m_axFrustumPlanes[uFLUX_GRASS_FRUSTUM_PLANES];   //   0 : 288 B, INWARD normals
	Flux_GrassWindBlockGPU m_xWind;                                         // 288 :  48 B
	Zenith_Maths::Vector4  m_xCameraPosWS_HiRadius{ 0.0f };                 // 336
	Zenith_Maths::Vector4  m_xCoverageMapParams{ 0.0f };                    // 352 : world, 1/world, densityScale, bias
	Zenith_Maths::Vector4  m_xTypeMapParams{ 0.0f };                        // 368 : world, 1/world, texel UV, type count
	Zenith_Maths::Vector4  m_xHeightMapParams{ 0.0f };                      // 384 : world, 1/world, height scale m, bias m
	Zenith_Maths::Vector4  m_xSlopeStep_Pad{ 0.0f };                        // 400 : x = central-difference step m
	Zenith_Maths::Vector4  m_xDisplacementParams{ 0.0f };                   // 416 : xy = origin XZ, z = world size, w = push scale
	Zenith_Maths::Vector4  m_xLoRadius_MaxDist_Density_Pad{ 0.0f };         // 432
	Zenith_Maths::UVector4 m_xTileCount_Seed_ActiveSlots_PoolCap{ 0u };     // 448
};
static_assert(sizeof(Flux_GrassPlacementConstantsGPU) == 464,
	"GrassPlacementConstants is a pinned 464 bytes (Flux_Grass_Placement.slang)");

// Flux_GrassDrawConstantsLayout (Flux_GrassCommon.slang) — shared VERBATIM by the
// G-buffer, velocity and shadow programs.
struct Flux_GrassDrawConstantsGPU
{
	Flux_GrassWindBlockGPU m_xWind;                                              //  0 : 48 B
	Zenith_Maths::Vector4  m_xCameraPosWS_HiRadius{ 0.0f };                      // 48
	Zenith_Maths::Vector4  m_xMinPixelWidth_Thicken_NormalBlend_LODBand{ 0.0f }; // 64
	Zenith_Maths::Vector4  m_xMaxDrawDistance_GlossCut_DebugMode_Pad{ 0.0f };    // 80 : z = asfloat(debug mode)
};
static_assert(sizeof(Flux_GrassDrawConstantsGPU) == 96,
	"GrassDrawConstants is a pinned 96 bytes (Flux_GrassCommon.slang)");

// Flux_GrassTileGPU (Flux_GrassCommon.slang). The lattice base is in GLOBAL
// HI-LATTICE UNITS, not metres, so both LODs share one lattice->world mapping and
// a LO tile's nodes are exactly the (even,even) subset of the HI lattice.
struct Flux_GrassTileGPU
{
	int   m_iLatticeBaseX  = 0;
	int   m_iLatticeBaseZ  = 0;
	u_int m_uLatticeStride = 1u;
	u_int m_uIsLoLOD       = 0u;
};
static_assert(sizeof(Flux_GrassTileGPU) == 16, "the tile record is four 32-bit slots");

// GrassDisplacementConstantsLayout (Flux_Grass_Displacement.slang).
//
// The scroll rides as a TEXEL count rather than being re-derived in the CS from
// the two metre origins: the host snapped the anchor to whole texels precisely so
// the offset would be an integer, and a float division there could land it a
// half-ULP off and turn a lossless slide into a one-texel smear.
struct Flux_GrassDisplacementConstantsGPU
{
	Zenith_Maths::Vector4  m_xPrevOriginXZ_Size_Pad{ 0.0f };   //  0 : xy = prev anchor origin (m), z = map size (m)
	Zenith_Maths::Vector4  m_xNextOriginXZ_Size_Pad{ 0.0f };   // 16 : xy = this frame's origin (m), z = map size (m)
	Zenith_Maths::Vector4  m_xDecay_Texel_ShiftXZ{ 0.0f };     // 32 : retention, texel size (m), prev->next texel shift
	Zenith_Maths::UVector4 m_xResolution_MoverCount{ 0u };     // 48 : xy = resolution, z = mover count
};
static_assert(sizeof(Flux_GrassDisplacementConstantsGPU) == 64,
	"GrassDisplacementConstants is a pinned 64 bytes (Flux_Grass_Displacement.slang)");

// One mover as the displacement CS reads it. A single float4 IS the whole record —
// the shader declares StructuredBuffer<float4> for the same reason, so there is no
// second layout to keep in step.
struct Flux_GrassMoverGPU
{
	Zenith_Maths::Vector4 m_xPosXZ_Radius_Strength{ 0.0f };   // xy = world XZ (m), z = radius (m), w = [0,1] strength
};
static_assert(sizeof(Flux_GrassMoverGPU) == 16, "a mover is one float4 — world XZ, radius, strength");

class Flux_GrassImpl
{
public:
	Flux_GrassImpl() = default;
	~Flux_GrassImpl() = default;

	Flux_GrassImpl(const Flux_GrassImpl&) = delete;
	Flux_GrassImpl& operator=(const Flux_GrassImpl&) = delete;

	// ===== Feature lifecycle (registry-driven) =====
	void Initialise();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void Shutdown();

	// Scene-lifecycle hook. The engine fires this on scene load and it DOUBLE-FIRES
	// at boot, so it must be idempotent and safe before Initialise — it touches CPU
	// state only.
	void Reset();

	// Unconditional full scene clear: maps, tiles, movers and stats all go, which
	// leaves zero blades. The TYPE TABLE and the WIND state persist (they are
	// authored/global, not scene-owned) and so does every GPU allocation, which is
	// released only by Shutdown.
	void ClearSceneData();

	// ===== Feeding =====
	struct BuildParams
	{
		float m_fDensityScale = 1.0f;
	};

	// Raw CPU maps, all covering [0, fWorldSize] on both axes from the world origin.
	// The data is COPIED (quantized to the GPU texture formats) — no pointer is
	// retained past the call.
	struct MapSet
	{
		const float*  pHeight   = nullptr;   // uHeightSize^2 heightfield, METRES
		u_int         uHeightSize   = 0u;
		const float*  pCoverage = nullptr;   // uCoverageSize^2 density [0,1]
		u_int         uCoverageSize = 0u;
		const u_int8* pType     = nullptr;   // uTypeSize^2 type indices; null => all type 0
		u_int         uTypeSize     = 0u;
		float         fWorldSize = 0.0f;     // world metres the maps cover (square)
	};

	// Loads GrassDensity / GrassType / Height .ztxtr from a terrain texture
	// directory. GrassType is OPTIONAL (absent => every texel type 0); a missing or
	// malformed Height/GrassDensity is a hard failure that leaves the prior state
	// completely untouched.
	// fWorldSize is the SQUARE authoring domain the maps in strDir span -- the
	// owning terrain's Zenith_TerrainDimensions::MaxWorldSize(). It used to be
	// read from the fixed config constant inside, which silently placed every
	// grass blade on a 4096m footprint regardless of how big the terrain was.
	bool BuildFromTerrainTextures(const std::string& strDir, float fWorldSize, const BuildParams& xParams);

	// The same build from raw pointers (editor live maps + headless tests). Invalid
	// input is rejected with the prior state intact.
	bool BuildFromMaps(const MapSet& xMaps, const BuildParams& xParams);

	// ===== Types =====
	// Copies + validates the table, then resolves every entry's texture PATHS to
	// bindless slots (acquiring the texture assets and pinning them on this impl
	// until the next SetTypeTable / ReleaseAssetReferences). Re-uploaded by the next
	// gather. Survives ClearSceneData.
	void SetTypeTable(const Flux_GrassTypeTable& xTable);
	const Flux_GrassTypeTable& GetTypeTable() const { return m_xTypeTable; }

	// Drop the type-table texture pins. Called from Flux_RendererImpl's pre-registry
	// release window, NOT from Shutdown: the feature-walk Shutdown runs after
	// Zenith_AssetRegistry is gone.
	void ReleaseAssetReferences();

	// ===== Tuning =====
	// The debug variables bind these members BY REFERENCE, so nothing here is
	// re-stamped per frame and a value written from game code survives.
	void  SetDensityScale(float fScale);
	float GetDensityScale() const { return m_fDensityScale; }
	void  SetMaxDistance(float fDistance);
	float GetMaxDistance() const { return m_fMaxDistance; }
	void  SetWindDirection(float fYawRad);
	float GetWindYawRadians() const;
	void  SetWindStrength(float fStrength);
	float GetWindStrength() const { return m_xWind.m_fStrength; }
	// The debug-mode variable is ImGui-only, so a test that wants to sweep the
	// views drives them here instead (the same runtime-override shape TAA's
	// SetEnabled has). The value reaches the draw CB through the next gather.
	void  SetDebugMode(u_int uMode) { m_uDebugMode = uMode; }
	u_int GetDebugMode() const { return m_uDebugMode; }
	// The DisableShadowCasting debug variable is ImGui-only, so an A/B — or a test —
	// drives it here for the same reason SetDebugMode exists. Third input to
	// IsShadowCastingEnabled(), and it gates GENERATION as well as the draw.
	void  SetDisableShadowCasting(bool bDisable) { m_bDisableShadowCasting = bDisable; }
	bool  IsShadowCastingDisabled() const { return m_bDisableShadowCasting; }
	// Same latch idiom as SetDisableShadowCasting, for the same reason: the debug
	// variable is ImGui-only, so a capture sweep drives the orbiter from here. While
	// on, every gather submits ONE synthetic mover circling the camera's ground point.
	void  SetDebugOrbitDisplacer(bool bEnable) { m_bDebugOrbitDisplacer = bEnable; }
	bool  IsDebugOrbitDisplacerEnabled() const { return m_bDebugOrbitDisplacer; }

	// ===== Displacement =====
	struct Mover
	{
		Zenith_Maths::Vector3 m_xPos{ 0.0f, 0.0f, 0.0f };
		float m_fRadius = 0.0f;
		float m_fStrength = 0.0f;
	};

	// Immediate-mode: the list is consumed and cleared by every frame's gather, so
	// a mover must be resubmitted each frame it should exist. Over the cap the
	// submission is DROPPED and counted rather than growing the list.
	void SubmitMover(const Mover& xMover);
	u_int GetMoverCount() const { return m_axMovers.GetSize(); }
	u_int GetMoverOverflowCount() const { return m_uMoverOverflowCount; }

	// ===== CPU queries =====
	// ALL of these return 0 when unbuilt. (The retired SampleDensityMap returned a
	// neutral 1.0 for "no map", which silently made an unbuilt world read as fully
	// covered; a caller with no data has to decide what that means.)
	float  SampleGrassCoverage(float fWorldX, float fWorldZ) const;
	u_int8 SampleGrassType(float fWorldX, float fWorldZ) const;      // nearest-texel, never interpolated
	float  SampleGrassHeight(float fWorldX, float fWorldZ) const;    // bilinear, metres

	// ===== Stats / observability =====
	bool  IsBuilt() const { return m_bBuilt; }
	bool  HasCoverageMap() const { return m_uCoverageSize > 0u; }
	u_int GetCoverageMapSize() const { return m_uCoverageSize; }
	float GetCoverageWorldSize() const { return m_fMapWorldSize; }

	// Lattice cells summed over the tiles scheduled for the CURRENT frame — the
	// dispatched placement work, not a blade count (the surviving blade count is a
	// GPU quantity; see ReadbackVisibleBladeCount).
	//
	// DETERMINISM IS A CONTRACT, not an implementation detail: for a fixed camera
	// and fixed maps this value is exactly reproducible, because the tile scheduler
	// it derives from is a pure function with a total order over its output. Five
	// Zenithmon suites assert an EXACT restore of this number across battle
	// transitions, so anything that makes tile selection frame-order- or
	// float-accumulation-dependent breaks them.
	u_int GetScheduledInstanceCount() const { return m_uScheduledInstanceCount; }

	// Tiles actually dispatched this frame (post-cull, post-cap).
	u_int GetVisibleTileCount() const { return m_uVisibleTileCount; }
	// Tiles that survived culling BEFORE the 256-tile cap. Greater than
	// GetVisibleTileCount() exactly when the budget dropped tiles.
	u_int GetTileCount() const { return m_uTileCount; }
	u_int GetSubmittedDrawCount() const { return m_uSubmittedDrawCount; }
	float GetBufferUsageMB() const;

	// Bit i set <=> the placement CS was told to fill partition i this frame. Slots
	// 0-1 (camera HI/LO) are unconditional; slots 2-3 (cascades 0-1) require shadow
	// casting to be enabled AND the last gather to have staged real cascade frusta.
	u_int GetActiveSlotMask() const { return ComputeActiveSlotMask(); }

	// EXPLICIT SLOW PATH — drains staged writes and idles the device. Downloads the
	// 320-byte indirect block and sums the 16 instance counts. Headless this is 0 by
	// construction (DownloadBufferData zero-fills without an allocator), so it is
	// windowed-only truth and must never be asserted on in a Null_ test.
	u_int ReadbackVisibleBladeCount();

	// ===== Shadows =====
	// Per-cascade caster draw, recorded from inside cascade uCascade's pass by
	// Flux_ShadowsImpl::ExecuteShadowCascade. Draws the LO partition from indirect
	// slot (2 + uCascade), so only cascades 0-1 have a partition at all. No-op when
	// unbuilt, when shadow casting is off, or when the cascade partitions were not
	// populated this frame.
	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade);

	// The three persistent buffers a cascade pass must declare a READ on so the graph
	// orders reset -> placement -> fixup ahead of it and synthesises the
	// WRITE_UAV -> READ barriers. Non-const because Flux_RenderGraph::ReadBuffer takes
	// a mutable Flux_Buffer& (traffic is keyed by object address). This is the only
	// window into the private GPU state and it exists because the declaration has to
	// be made by the pass's OWNER, which is Flux_ShadowsImpl.
	Flux_Buffer& GetBladePoolBuffer()    { return m_xBladePoolBuffer.GetBuffer(); }
	Flux_Buffer& GetVisibleIndexBuffer() { return m_xVisibleIndexBuffer.GetBuffer(); }
	Flux_Buffer& GetIndirectArgsBuffer() { return m_xIndirectArgsBuffer.GetBuffer(); }

	// ===== Per-frame gather (main thread; hung on the Placement pass's .Prepare) =====
	// Single writer of the per-frame state; the record callbacks are pure readers.
	void GatherGrassFrame(void* pUserData);

	// ===== Graph trampoline surface =====
	// The record callbacks are captureless file-statics in the frame TU (the graph's
	// OnRecord signature admits nothing else) and recover the feature through
	// g_xEngine. Every member stays private, so each pass's body is a method here and
	// the trampoline is a one-line forward.
	void RecordReset(Flux_CommandBuffer& xCmdBuf);
	void RecordPlacement(Flux_CommandBuffer& xCmdBuf);
	void RecordIndirectFixup(Flux_CommandBuffer& xCmdBuf);
	void RecordGBuffer(Flux_CommandBuffer& xCmdBuf);
	void RecordDisplacement(Flux_CommandBuffer& xCmdBuf);

	bool IsGPUReady() const { return m_bGPUResourcesReady; }
	bool IsShadowCastingEnabled() const;

private:
	// --- build helpers ---
	bool  QuantizeHeightMap(const float* pfHeight, u_int uSize, float fWorldSize);
	void  QuantizeCoverageMap(const float* pfCoverage, u_int uSize);
	void  CopyTypeMap(const u_int8* pucType, u_int uSize);
	void  BuildTileHeightGrid(const float* pfHeight, u_int uSize, float fWorldSize);
	void  UploadMapTextures();
	void  CreateGPUResources();
	void  DestroyGPUResources();
	void  CreateMapTextures();
	void  DestroyMapTextures();
	// The two committed trail maps. Built unconditionally (including headless — a
	// render-target allocation carries no staging cost, unlike the 34 MB map set) so
	// the render-graph declaration is identical in every config.
	void  CreateDisplacementMaps();
	void  DestroyDisplacementMaps();

	// Boot-time attempt at the game's authored .zdata type table. Absent or
	// rejected leaves the seeded defaults in place — a game without one is the
	// normal path and must not read as a failure. Pure CPU work (the table is
	// projected to the GPU by the next gather like any other), so it runs
	// identically in a Null_ build.
	void  LoadAuthoredTypeTable();

	// The one Flux_GrassTypeTable::TextureResolver: acquires the texture asset at
	// strPath, marks it bindless (REPEAT for vein/gloss, CLAMP for the ramp), pins
	// the handle in m_axTypeTextureHandles and returns its slot. A texture that
	// fails to load returns UNBOUND, so the fragment stage skips the sample.
	static u_int ResolveTypeTexture(const std::string& strPath, FluxGrassTextureSlot eSlot, void* pUser);
	void  ResolveTypeTextures();

	// --- gather helpers (each is one step of GatherGrassFrame) ---
	// The three Stage* helpers are PURE builders of a CPU-side block; a single
	// UploadFrameBuffers then pushes them all, so the device reach lives in exactly
	// one place per frame.
	// Freezes this frame's per-view matrices (and m_uCascadeFrustaCount) so tile
	// selection and the GPU plane block cull against the IDENTICAL views — two
	// separate fetches could straddle a stage and disagree.
	void  StageFrustumViewProjs(const Flux_RenderViewRegistry& xViews);
	void  SelectTilesForFrame(const Zenith_Maths::Vector3& xCameraPos);
	void  StageTileRecords(Flux_GrassTileGPU* paxOut) const;
	void  StagePlacementConstants(Flux_GrassPlacementConstantsGPU& xOut,
		const Zenith_Maths::Vector3& xCameraPos) const;
	void  StageDrawConstants(Flux_GrassDrawConstantsGPU& xOut, const Zenith_Maths::Vector3& xCameraPos) const;
	// dt is a PARAMETER, not a clock read, for the same reason AdvanceWind's time is:
	// the decay has to key off the frame's staged delta so a fixed-dt capture replays
	// the identical trail.
	void  StageDisplacementConstants(Flux_GrassDisplacementConstantsGPU& xOut, float fDeltaSeconds) const;
	void  StageMoverRecords(Flux_GrassMoverGPU* paxOut) const;
	// Flips the ping-pong and re-snaps the anchor. THE single writer of both, and it
	// must run before anything stages a constant block: the placement pass samples the
	// map the PREVIOUS frame wrote, so it needs the pre-flip anchor.
	void  AdvanceDisplacementAnchor(const Zenith_Maths::Vector3& xCameraPos);
	void  SubmitDebugOrbitMover(const Zenith_Maths::Vector3& xCameraPos);
	void  UploadFrameBuffers(const Zenith_Maths::Vector3& xCameraPos, float fDeltaSeconds);
	void  AdvanceWind(float fTimeSeconds);
	float ComputeBladeHeadroom() const;
	u_int ComputeActiveSlotMask() const;

	// --- map views onto the quantized CPU copies (no allocation, no copy) ---
	Flux_GrassMap MakeHeightMapView() const;
	Flux_GrassMap MakeCoverageMapView() const;
	Flux_GrassMap MakeTypeMapView() const;

#ifdef ZENITH_TOOLS
	void RegisterDebugVariables();
#endif

	// ===== Shaders / pipelines (created in Initialise, rebuilt by hot-reload) =====
	Flux_Shader   m_xResetShader;
	Flux_Pipeline m_xResetPipeline;
	Flux_RootSig  m_xResetRootSig;
	Flux_Shader   m_xPlacementShader;
	Flux_Pipeline m_xPlacementPipeline;
	Flux_RootSig  m_xPlacementRootSig;
	Flux_Shader   m_xFixupShader;
	Flux_Pipeline m_xFixupPipeline;
	Flux_RootSig  m_xFixupRootSig;
	Flux_Shader   m_xDisplacementShader;
	Flux_Pipeline m_xDisplacementPipeline;
	Flux_RootSig  m_xDisplacementRootSig;

	Flux_Shader   m_xGBufferShader;
	Flux_Pipeline m_xGBufferPipeline;           // 4 core MRTs
	Flux_Shader   m_xGBufferVelocityShader;
	Flux_Pipeline m_xGBufferVelocityPipeline;   // 5 MRTs (core + velocity)
	Flux_Shader   m_xShadowShader;
	Flux_Pipeline m_xShadowPipeline;            // depth only

	// ===== Persistent GPU resources (graph-tracked) =====
	Flux_ReadWriteBuffer m_xBladePoolBuffer;      // Flux_GrassBladeInstance[pool capacity]
	Flux_ReadWriteBuffer m_xVisibleIndexBuffer;   // uint[capacity], four fixed partitions
	Flux_ReadWriteBuffer m_xBladeCounterBuffer;   // uint[1] pool append cursor
	Flux_IndirectBuffer  m_xIndirectArgsBuffer;   // VkDrawIndexedIndirectCommand[16]
	// The blade index buffer IS the shader's index table, uploaded verbatim. There is
	// no vertex buffer at all: SV_VertexID delivers the fetched index value.
	Flux_IndexBuffer     m_xBladeIndexBuffer;

	// ===== Displacement field — the ONLY persistent GPU state grass carries =====
	// Two committed 256^2 RG16F images whose prev/next roles swap every frame. The
	// graph declares BOTH read-modify-write on the displacement pass and BOTH as a
	// previous-frame read on the placement pass, so the declaration is parity-blind
	// and the flip is invisible to it — the alternative, declaring one read and one
	// written, would need a different declaration on odd and even frames, and the
	// graph is compiled once per build, not once per frame.
	Flux_RenderAttachment m_axDisplacementMaps[2];

	// ===== Frame-indexed dynamic inputs (graph-INVISIBLE by contract) =====
	Flux_DynamicReadWriteBuffer m_xTypeParamsBuffer;         // Flux_GrassTypeParamsGPU[16]
	Flux_DynamicReadWriteBuffer m_xTileBuffer;               // 16-byte tile records
	Flux_DynamicReadWriteBuffer m_xMoverBuffer;              // Flux_GrassMoverGPU[64]
	Flux_DynamicConstantBuffer  m_xPlacementConstantsBuffer;
	Flux_DynamicConstantBuffer  m_xDrawConstantsBuffer;
	Flux_DynamicConstantBuffer  m_xPrevWindConstantsBuffer;
	Flux_DynamicConstantBuffer  m_xDisplacementConstantsBuffer;

	// ===== Map textures (fixed extents, updated in place at Build*) =====
	Flux_Texture m_xHeightTexture;         // R16_UNORM
	Flux_Texture m_xCoverageTexture;       // R8_UNORM
	Flux_Texture m_xTypeTexture;           // R8_UNORM, POINT-sampled
	// Neutral 1x1 zero field. Bound by the placement CS in place of the real trail
	// map whenever displacement is off, so "no push" is a property of the DATA and
	// not of a branch the CS would have to take.
	Flux_Texture m_xDisplacementTexture;

	// ===== CPU maps — the SAME quantized bytes the textures hold =====
	Zenith_Vector<u_int16> m_auHeightTexels;
	Zenith_Vector<u_int8>  m_aucCoverageTexels;
	Zenith_Vector<u_int8>  m_aucTypeTexels;
	u_int m_uHeightSize   = 0u;
	u_int m_uCoverageSize = 0u;
	u_int m_uTypeSize     = 0u;
	float m_fMapWorldSize = 0.0f;
	// Height texels are unorm16 over [bias, bias + scale] metres, so the fixed-point
	// range tracks the terrain actually loaded instead of a global constant.
	float m_fHeightScale = 1.0f;
	float m_fHeightBias  = 0.0f;

	// ===== Coarse per-cell height band (tile AABB culling) =====
	Zenith_Vector<float> m_afTileMinY;
	Zenith_Vector<float> m_afTileMaxY;
	u_int m_uHeightGridCells = 0u;
	float m_fHeightGridCellSize = 0.0f;

	// ===== Per-frame frozen state =====
	// Per-view NoJitter view-projections, FRUSTUM-SLOT major: [0] camera, [1] cascade
	// 0, [2] cascade 1. Slots past m_uCascadeFrustaCount hold a COPY of the camera's
	// (see StageFrustumViewProjs) so an inactive slot degrades to "cull like the
	// camera" rather than culling against uninitialised planes.
	Zenith_Maths::Matrix4 m_axFrustumViewProjs[uFLUX_GRASS_FRUSTUM_COUNT] = {};
	Flux_GrassTileList m_xTileList;
	bool  m_bVelocityLatched = false;   // frozen copy of IsVelocityMRTActive for the record
	// Frozen copy of the m_bGrassEnabled graphics option. Every record callback
	// and the cascade caster read THIS and never the option itself: the gather is
	// what decides there is nothing to place, so a toggle landing between the two
	// would leave a draw reading indirect args whose reset it had already skipped.
	bool  m_bEnabledLatched = false;
	// Frozen copy of m_bGrassDisplacementEnabled, read by the displacement record and
	// by the placement bind. Latched for the same reason m_bEnabledLatched is: the
	// gather is what decides which map the placement CS samples, so a toggle landing
	// between gather and record would bind a map the constants do not describe.
	bool  m_bDisplacementLatched = false;
	// Has a displacement dispatch ever produced the map the NEXT frame will read?
	// A freshly created image holds UNDEFINED bytes, and in an RG16F those can decode
	// as NaN — which no amount of decay ever removes, and which would then ride into
	// every blade's push. Until a real dispatch has filled it the source is treated as
	// absent: the placement CS binds the neutral texture and the displacement CS is
	// told to scroll a whole map, so every source texel resolves out of range.
	//
	// Two flags because they answer different questions on the same frame — the same
	// split TAA's history validity uses. This one is "will next frame's source be
	// real", written at the end of the gather; the latch below is "is THIS frame's
	// source real", which is what the records need.
	bool  m_bDisplacementFieldValid = false;
	bool  m_bDisplacementSourceValidThisFrame = false;
	u_int m_uScheduledInstanceCount = 0u;
	u_int m_uVisibleTileCount = 0u;
	u_int m_uTileCount = 0u;
	u_int m_uSubmittedDrawCount = 0u;

	// ===== Wind =====
	Flux_WindConstants m_xWind;
	Flux_WindConstants m_xPrevWind;     // last frame's block — the velocity VS's only prev state
	float m_fWindYawDeg = 0.0f;         // THE wind heading; the direction vector is derived each frame

	// ===== Types =====
	Flux_GrassTypeTable m_xTypeTable;
	// Owning pins for every texture the live table resolved: the bindless slots
	// in m_xTypeTable are only valid while the assets they index stay loaded.
	Zenith_Vector<TextureHandle> m_axTypeTextureHandles;

	// ===== Displacement movers + ping-pong state =====
	Zenith_Vector<Mover> m_axMovers;
	u_int m_uMoverOverflowCount = 0u;
	// Which of m_axDisplacementMaps the displacement pass FILLS this frame; the other
	// is the source both it and the placement CS read. Flipped once per gather.
	u_int m_uDisplacementWriteIndex = 0u;
	// The anchors the source and destination maps are aligned to. Two, not one: the
	// re-anchor is defined by the pair, and the placement CS's world -> UV mapping has
	// to use the SOURCE anchor or every blade would sample the field one scroll off.
	Flux_GrassDisplacementAnchor m_xDisplacementAnchorPrev;
	Flux_GrassDisplacementAnchor m_xDisplacementAnchorNext;
	// Gain applied to the sampled field before the placement CS saturates it, and the
	// ONE slot m_bGrassDisplacementEnabled is applied to — so the option is honoured by
	// the VALUE and never by a branch in the CS.
	//
	// NOT metres. The field stores a [0,1]-magnitude push vector and the blade's
	// response is a POSE change, not a translation: at an effective push of 1 the blade
	// turns fully to face away from the mover, gains 1.2 rad of tilt and loses 35% of
	// its height. 1.0 therefore means "a full-strength mover flattens what it stands
	// on"; halve it for a springier field, raise it to flatten from further out.
	float m_fDisplacementPushScale = 1.0f;

	// ===== Tuning (debug-var bound by reference) =====
	float m_fDensityScale = 1.0f;
	float m_fMaxDistance = Flux_GrassConfig::fDEFAULT_MAX_DISTANCE;

	// ===== Debug =====
	u_int m_uDebugMode = 0u;
	bool  m_bFreezeCulling = false;
	bool  m_bShowTileGrid = false;
	bool  m_bDisableShadowCasting = false;
	bool  m_bForceLoBlades = false;
	bool  m_bDebugOrbitDisplacer = false;

	// ===== Flags =====
	bool  m_bBuilt = false;
	bool  m_bGPUResourcesReady = false;
	// REAL cascade frusta staged into the placement constants this frame — never a
	// duplicated camera frustum, which is why the mask keys off this count rather
	// than off the option alone: culling a cascade partition against the camera's
	// frustum would fill it with the WRONG blades, not merely too few. Zero whenever
	// shadow casting is off, the cascade views are inactive, or no frame was staged.
	u_int m_uCascadeFrustaCount = 0u;
};
