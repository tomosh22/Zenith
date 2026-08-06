#pragma once

//=============================================================================
// Flux_GrassImpl — the GPU-driven grass feature.
//
// NOTHING about a blade is persisted. Every frame the pipeline runs
//
//   Reset (CS)  ->  Placement (CS)  ->  IndirectFixup (CS)  ->  indirect draws
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
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <string>

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

// One thread per lattice cell; the placement CS decodes TILE-MAJOR, so the
// dispatch is (tileCount * cells-per-tile) threads in 64-wide groups.
constexpr u_int uFLUX_GRASS_TILE_CELL_COUNT = Flux_GrassConfig::uTILE_CELLS * Flux_GrassConfig::uTILE_CELLS;
constexpr u_int uFLUX_GRASS_PLACEMENT_GROUP_SIZE = 64u;

constexpr u_int uFLUX_GRASS_MAX_MOVERS = 64u;

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
	bool BuildFromTerrainTextures(const std::string& strDir, const BuildParams& xParams);

	// The same build from raw pointers (editor live maps + headless tests). Invalid
	// input is rejected with the prior state intact.
	bool BuildFromMaps(const MapSet& xMaps, const BuildParams& xParams);

	// ===== Types =====
	// Re-uploads the GPU parameter block. Survives ClearSceneData.
	void SetTypeTable(const Flux_GrassTypeTable& xTable);
	const Flux_GrassTypeTable& GetTypeTable() const { return m_xTypeTable; }

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

	// ===== Displacement seam (consumed by a later phase) =====
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

	// EXPLICIT SLOW PATH — drains staged writes and idles the device. Downloads the
	// 320-byte indirect block and sums the 16 instance counts. Headless this is 0 by
	// construction (DownloadBufferData zero-fills without an allocator), so it is
	// windowed-only truth and must never be asserted on in a Null_ test.
	u_int ReadbackVisibleBladeCount();

	// ===== Shadows =====
	// Per-cascade caster draw, called from inside cascade uCascade's pass record by
	// a later phase. Draws the LO partition from indirect slot (2 + uCascade), so
	// only cascades 0-1 have a partition at all. No-op when unbuilt, when shadow
	// casting is off, or when the cascade partitions were not populated this frame.
	void RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade);

	// ===== Per-frame gather (main thread; hung on the Placement pass's .Prepare) =====
	// Single writer of the per-frame state; the record callbacks are pure readers.
	void GatherGrassFrame(void* pUserData);

	// ===== Graph trampoline surface =====
	// The four record callbacks are captureless file-statics in the frame TU (the
	// graph's OnRecord signature admits nothing else) and recover the feature through
	// g_xEngine. Every member stays private, so each pass's body is a method here and
	// the trampoline is a one-line forward.
	void RecordReset(Flux_CommandBuffer& xCmdBuf);
	void RecordPlacement(Flux_CommandBuffer& xCmdBuf);
	void RecordIndirectFixup(Flux_CommandBuffer& xCmdBuf);
	void RecordGBuffer(Flux_CommandBuffer& xCmdBuf);

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

	// Boot-time attempt at the game's authored .zdata type table. Absent or
	// rejected leaves the seeded defaults in place — no game ships one today, so
	// ABSENCE is the normal path and must not read as a failure. Pure CPU work
	// (the table is projected to the GPU by the next gather like any other), so it
	// runs identically in a Null_ build.
	void  LoadAuthoredTypeTable();

	// --- gather helpers (each is one step of GatherGrassFrame) ---
	// The three Stage* helpers are PURE builders of a CPU-side block; a single
	// UploadFrameBuffers then pushes them all, so the device reach lives in exactly
	// one place per frame.
	void  SelectTilesForFrame(const Zenith_Maths::Vector3& xCameraPos, const Zenith_Maths::Matrix4& xViewProjNoJitter);
	void  StageTileRecords(Flux_GrassTileGPU* paxOut) const;
	void  StagePlacementConstants(Flux_GrassPlacementConstantsGPU& xOut,
		const Zenith_Maths::Vector3& xCameraPos, const Zenith_Maths::Matrix4& xViewProjNoJitter) const;
	void  StageDrawConstants(Flux_GrassDrawConstantsGPU& xOut, const Zenith_Maths::Vector3& xCameraPos) const;
	void  UploadFrameBuffers(const Zenith_Maths::Vector3& xCameraPos, const Zenith_Maths::Matrix4& xViewProjNoJitter);
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
	// Built but NOT dispatched: the displacement field is a later phase. Building it
	// here keeps the pipeline set (and therefore the VRAM footprint the TAA toggle
	// stress test pins) constant across that phase landing.
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

	// ===== Frame-indexed dynamic inputs (graph-INVISIBLE by contract) =====
	Flux_DynamicReadWriteBuffer m_xTypeParamsBuffer;         // Flux_GrassTypeParamsGPU[16]
	Flux_DynamicReadWriteBuffer m_xTileBuffer;               // 16-byte tile records
	Flux_DynamicConstantBuffer  m_xPlacementConstantsBuffer;
	Flux_DynamicConstantBuffer  m_xDrawConstantsBuffer;
	Flux_DynamicConstantBuffer  m_xPrevWindConstantsBuffer;

	// ===== Map textures (fixed extents, updated in place at Build*) =====
	Flux_Texture m_xHeightTexture;         // R16_UNORM
	Flux_Texture m_xCoverageTexture;       // R8_UNORM
	Flux_Texture m_xTypeTexture;           // R8_UNORM, POINT-sampled
	Flux_Texture m_xDisplacementTexture;   // neutral until the displacement phase

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
	Flux_GrassTileList m_xTileList;
	bool  m_bVelocityLatched = false;   // frozen copy of IsVelocityMRTActive for the record
	// Frozen copy of the m_bGrassEnabled graphics option. The four record callbacks
	// and the cascade caster read THIS and never the option itself: the gather is
	// what decides there is nothing to place, so a toggle landing between the two
	// would leave a draw reading indirect args whose reset it had already skipped.
	bool  m_bEnabledLatched = false;
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

	// ===== Displacement movers =====
	Zenith_Vector<Mover> m_axMovers;
	u_int m_uMoverOverflowCount = 0u;
	// Metres of tip push a full-strength mover applies. It reaches the placement CS
	// through ONE slot — the push scale of the displacement constants — which is
	// exactly where m_bGrassDisplacementEnabled is applied, so the option is honoured
	// by the VALUE and never by a branch in the CS. Zero until the displacement phase
	// supplies a real field for it to scale.
	float m_fDisplacementPushScale = 0.0f;

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
	// Cascade frusta fed into the placement constants this frame. Zero until the
	// shadow phase supplies real cascade planes, which is what keeps the cascade
	// partitions out of the active-slot mask (culling them against a duplicated
	// CAMERA frustum would fill them with the wrong blades, not merely too few).
	u_int m_uCascadeFrustaCount = 0u;
};
