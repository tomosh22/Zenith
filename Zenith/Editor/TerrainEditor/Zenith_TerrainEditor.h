#pragma once

#ifdef ZENITH_TOOLS

#include "AssetHandling/Zenith_Image.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_SceneData.h"
#include <string>

class Zenith_TerrainComponent;
class Zenith_TextureAsset;
struct Flux_TerrainStreamingState;

//=============================================================================
// Zenith_TerrainEditor — in-editor terrain sculpting / painting subsystem.
//
// Owns the authoritative CPU images for the terrain being edited:
//   - heightfield  : 4096x4096 float, normalized [0,1] (== Height.ztxtr R32)
//   - splatmap     : 2048x2048 RGBA8 (4 material-layer weights)
//   - grass density: 1024x1024 float [0,1]
//
// CPU/GPU sync contract (NON-NEGOTIABLE): live height edits NEVER write a
// resident chunk of the terrain's unified vertex buffer in place. The editor
// registers Flux_TerrainStreamingState::m_pfnChunkVertexHook (which re-shapes
// chunk vertices from the live heightfield on stream-in, before the GPU
// upload) and force-EVICTS edited resident chunks so they re-stream through
// the hook — the engine's race-free path. The always-resident LOW LOD stays
// stale during live editing and is refreshed by the explicit bake.
//
// The editor stores only the target's EntityID and re-resolves the component
// (and its streaming state) every frame — never caching the state pointer
// across frames — so component destruction / scene change / play-mode
// backup-restore can never leave it dangling.
//=============================================================================

// Tool selection. SplatPaint paints the active splat layer; GrassDensity
// paints the grass-density map; TreePaint scatters instanced trees (Shift
// erases); everything else sculpts the heightfield.
enum class Zenith_TerrainBrushTool
{
	Raise,
	Lower,
	Smooth,
	Flatten,
	SetHeight,
	Noise,
	Terrace,
	Ramp,
	Stamp,
	SplatPaint,
	GrassDensity,
	TreePaint,
	Count
};

enum class Zenith_TerrainBrushFalloff
{
	Smooth,   // smoothstep from centre to edge
	Linear,
	Sphere,   // spherical cap (wide plateau, fast edge)
	Sharp,    // quadratic tip (narrow spike)
	Count
};

// Which CPU map a region edit touched — used by the undo command.
enum class Zenith_TerrainEditMap
{
	Height,        // floats
	Splat,         // RGBA8
	GrassDensity,  // floats
};

struct Zenith_TerrainBrushSettings
{
	Zenith_TerrainBrushTool    m_eTool    = Zenith_TerrainBrushTool::Raise;
	Zenith_TerrainBrushFalloff m_eFalloff = Zenith_TerrainBrushFalloff::Smooth;
	float m_fRadius        = 50.0f;   // world metres (== heightfield pixels)
	float m_fStrength      = 0.5f;    // 0..1
	float m_fTargetHeight  = 48.0f;   // world Y for Flatten / SetHeight
	float m_fNoiseScale    = 0.05f;   // Noise tool: feature frequency (cycles/m)
	float m_fNoiseAmount   = 8.0f;    // Noise tool: peak displacement (world m)
	float m_fTerraceStep   = 8.0f;    // Terrace tool: world metres per step
	float m_fRampHardness  = 1.0f;    // Ramp tool: blend sharpness
	u_int m_uSplatLayer    = 0;       // 0..3 (active material layer)
	float m_fGrassDensity  = 1.0f;    // GrassDensity tool paint target [0,1]

	// TreePaint tool.
	u_int m_uTreesPerDab   = 3;       // placement attempts per dab (scaled by strength)
	float m_fTreeScaleMin  = 0.85f;   // uniform instance scale range
	float m_fTreeScaleMax  = 1.35f;
	float m_fTreeSpacing   = 4.0f;    // minimum metres between trunks
	float m_fTreeMaxSlopeDeg = 38.0f; // skip placement on steeper ground
};

struct Zenith_TerrainProceduralParams
{
	u_int m_uSeed        = 1337;
	float m_fBaseHeight  = 0.09f;     // normalized [0,1]
	float m_fAmplitude   = 0.12f;     // normalized peak-to-trough around base
	float m_fFrequency   = 0.0008f;   // base noise frequency (cycles per metre)
	u_int m_uOctaves     = 6;
	float m_fLacunarity  = 2.0f;
	float m_fGain        = 0.5f;
	float m_fRidgedBlend = 0.0f;      // 0 = pure FBM, 1 = pure ridged
};

struct Zenith_TerrainErosionParams
{
	u_int m_uSeed               = 1337;
	u_int m_uHydraulicDroplets  = 100000;
	u_int m_uThermalIterations  = 2;
	float m_fTalusAngleDeg      = 38.0f;  // thermal: slopes steeper than this slump
	// Brush-local variant: when m_bRegionOnly, droplets spawn inside the disc
	// and thermal relaxation is clamped + falloff-masked to it.
	bool  m_bRegionOnly     = false;
	float m_fRegionCentreX  = 0.0f;
	float m_fRegionCentreZ  = 0.0f;
	float m_fRegionRadius   = 256.0f;
};

// Per-material-slot auto-splat rule (slope/height classification, UE5
// landscape-auto-material style). Weights are feathered with smoothstep,
// jittered with seeded hash noise, then normalized across the 4 slots.
struct Zenith_TerrainAutoSplatRule
{
	bool  m_bEnabled       = false;
	float m_fHeightMin     = 0.0f;    // world metres
	float m_fHeightMax     = 512.0f;
	float m_fHeightFeather = 16.0f;
	float m_fSlopeMinDeg   = 0.0f;
	float m_fSlopeMaxDeg   = 90.0f;
	float m_fSlopeFeather  = 4.0f;
	float m_fWeight        = 1.0f;
	float m_fNoiseJitter   = 0.0f;    // 0..1
	u_int m_uNoiseSeed     = 7;
};

// Per-frame editor context handed in by Zenith_Editor::Update. Keeps the
// terrain editor from reaching back into Zenith_Editor for viewport/camera
// state (Zenith_Editor.cpp is engine-singleton-ratchet-counted; this isn't).
struct Zenith_TerrainEditorFrameContext
{
	bool m_bViewportHovered = false;
	bool m_bViewportFocused = false;
	Zenith_Maths::Vector2 m_xViewportPos  = { 0.0f, 0.0f };
	Zenith_Maths::Vector2 m_xViewportSize = { 0.0f, 0.0f };
	Zenith_Maths::Matrix4 m_xViewMatrix   = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xProjMatrix   = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector3 m_xCameraPos    = { 0.0f, 0.0f, 0.0f };
	bool m_bEditorStopped = false;   // terrain editing suspends outside Stopped
};

class Zenith_TerrainEditor
{
public:
	// Fixed terrain dimensions (static_asserted against Flux_TerrainConfig in
	// the .cpp — this header deliberately includes no Flux header).
	static constexpr u_int uHEIGHTFIELD_SIZE   = 4096;
	static constexpr u_int uSPLATMAP_SIZE      = 2048;
	static constexpr u_int uGRASS_DENSITY_SIZE = 1024;
	static constexpr u_int uCHUNK_GRID         = 64;
	static constexpr u_int uCHUNK_COUNT        = uCHUNK_GRID * uCHUNK_GRID;
	static constexpr float fTERRAIN_WORLD_SIZE = 4096.0f;
	static constexpr float fTERRAIN_MAX_HEIGHT = 512.0f;
	static constexpr u_int64 ulUNDO_BUDGET_BYTES = 256ull * 1024ull * 1024ull;

	Zenith_TerrainEditor() = default;
	~Zenith_TerrainEditor() = default;
	Zenith_TerrainEditor(const Zenith_TerrainEditor&) = delete;
	Zenith_TerrainEditor& operator=(const Zenith_TerrainEditor&) = delete;

	//--------------------------------------------------------------------------
	// Session lifecycle
	//--------------------------------------------------------------------------

	// Open an editing session on a terrain entity: copies the component's
	// persisted asset set, resets all CPU images to clean defaults, overlays
	// the saved textures that exist for that set, and registers the stream-in
	// vertex hook on the component's streaming state.
	void Open(Zenith_EntityID uTerrainEntity);

	// Component-less authoring session (editor automation before the terrain
	// entity exists). CPU + disk only — no hook, no eviction, no GPU paths.
	// Inherently headless-safe.
	void OpenStandalone();

	// Clear the hook and force-evict session-dirty chunks so visuals revert
	// to the baked data. CPU images remain in memory, but Open always reseeds
	// them from clean defaults + the persisted set.
	void Close();

	bool IsActive() const { return m_bActive; }
	bool IsStandalone() const { return m_bActive && m_uTargetEntity == INVALID_ENTITY_ID; }
	bool HasUnbakedChanges() const { return m_bSessionDirty; }
	Zenith_EntityID GetTargetEntity() const { return m_uTargetEntity; }

	// Service work, called once per frame from Zenith_Editor::Update in EVERY
	// editor mode: re-resolves the target, keeps the hook pinned, pops the
	// pending-evict queue (budgeted), steps sliced erosion, and flushes dirty
	// paint textures to the GPU. Running in Play mode too means unbaked
	// terrain edits stay visible while playtesting (UE-style).
	void ServiceUpdate();

	// Interactive viewport work (brush input + cursor), called once per frame
	// from Zenith_Editor::Update in Stopped mode only, BEFORE gizmo
	// interaction / object picking.
	void UpdatePerFrame(const Zenith_TerrainEditorFrameContext& xCtx);

	// True while a painted splat edit is awaiting its GPU re-upload (flushed
	// by ServiceUpdate; windowed only).
	bool HasPendingSplatUpload() const { return m_bSplatGPUDirty; }

	// True while the terrain editor owns viewport clicks (edit mode armed and
	// the viewport hovered, or a stroke is mid-drag) — Zenith_Editor skips
	// gizmo interaction + object picking for the frame.
	bool ConsumedViewportInput() const { return m_bConsumedViewportInput; }

	// Edit-mode arm/disarm (panel toggle). Open() arms it.
	bool IsEditModeEnabled() const { return m_bEditModeEnabled; }
	void SetEditModeEnabled(bool bEnabled) { m_bEditModeEnabled = bEnabled; }

	// Asset-set bake target. The empty set preserves RenderTest and existing
	// games' split legacy layout (meshes in Terrain/, textures in
	// Textures/Terrain/). Named sets keep all terrain assets together below
	// Terrain/<Set>/.
	bool SetAssetSet(const std::string& strSet);
	const std::string& GetAssetSet() const;
	std::string GetMeshAssetDirectory() const;
	std::string GetTextureAssetDirectory() const;
	const std::string& GetAssetSetValidationError() const { return m_strAssetSetValidationError; }

	//--------------------------------------------------------------------------
	// Scriptable editing API — the ImGui panel and editor automation share
	// these exact entry points (single code path).
	//--------------------------------------------------------------------------

	// Stroke bracketing: BeginStroke starts undo-region accumulation,
	// EndStroke pushes one undo command covering the whole stroke.
	void BeginStroke();
	void ApplyBrushDab(Zenith_TerrainBrushTool eTool, float fWorldX, float fWorldZ,
		float fRadius, float fStrength, float fToolValue);
	void EndStroke();
	bool IsStrokeActive() const { return m_bStrokeActive; }

	// Configure the TreePaint brush (the ImGui panel and editor automation share
	// this). Pure field writes into m_xBrush, plus a re-seed of the scatter RNG
	// so a re-authored scene paints byte-identically (uSeed == 0 keeps a fixed
	// default seed; the xorshift state must never be zero).
	void SetTreeBrushSettings(u_int uTreesPerDab, float fScaleMin, float fScaleMax,
		float fSpacing, float fMaxSlopeDeg, u_int uSeed);

#ifdef ZENITH_TESTING
	// Test-only: inspect the TreePaint scatter RNG state (seeded by
	// SetTreeBrushSettings) to pin the byte-stable-save contract.
	u_int GetTreeRngState_ForTest() const { return m_uTreeRngState; }
#endif

	// Reset the three CPU maps to their defaults (flat-0 heightfield, full
	// layer-0 splat, zero grass density) WITHOUT touching disk. From-scratch
	// authoring scripts run this first so re-running a recipe stays
	// byte-identical even when a previous bake's textures exist on disk
	// (Open/OpenStandalone seed the session from them). Clears the undo stack.
	void ResetImagesToDefaults();

	// Whole-field seeded procedural generation. NOT undoable (clears the undo
	// stack) — deterministic: same params => byte-identical heightfield.
	void GenerateProcedural(const Zenith_TerrainProceduralParams& xParams);

	// Hydraulic + thermal erosion. bSynchronous runs the full budget in one
	// call (automation); otherwise it is sliced across frames on the main
	// thread (never a background task — the stream-in hook reads the
	// heightfield from PreRenderUpdate the same frame). Whole-field runs are
	// NOT undoable (clear the undo stack); region runs are not undoable either
	// (droplets escape the region rect) — both clear.
	void RunErosion(const Zenith_TerrainErosionParams& xParams, bool bSynchronous);
	bool IsErosionRunning() const { return m_bErosionRunning; }
	float GetErosionProgress() const;

	// Auto-splat: classify every splat texel from the heightfield by the
	// 4 per-slot rules. Undoable (one full-splat snapshot).
	void SetAutoSplatRule(u_int uSlot, const Zenith_TerrainAutoSplatRule& xRule);
	const Zenith_TerrainAutoSplatRule& GetAutoSplatRule(u_int uSlot) const;
	void RunAutoSplat();

	// Copy/stamp: capture a heightfield disc into the stamp buffer; the Stamp
	// tool then stamps it (additive, falloff-masked) at the dab position.
	void SampleStamp(float fCentreX, float fCentreZ, float fRadius);
	bool HasStamp() const { return m_uStampSize > 0; }
	void ClearStamp();

	//--------------------------------------------------------------------------
	// Persistence (Zenith_TerrainEditor_Bake.cpp)
	//--------------------------------------------------------------------------

	// Write Height.ztxtr (R32) / Splatmap_RGBA.ztxtr (RGBA8) /
	// GrassDensity.ztxtr (R32) into GetTextureAssetDirectory().
	bool SaveTextures();

	// Regenerate the brush-indicator decal texture into
	// <ENGINE_ASSETS>/Textures/Brushes/. A generated artifact (gitignored) —
	// called at EVERY editor boot from Zenith_Engine::InitialiseProject so
	// the file on disk always matches the current generator.
	static void RegenerateBrushTextures();

	// Component-less chunk-mesh export from the live heightfield into
	// GetMeshAssetDirectory(). Automation path.
	void BakeMeshes();

	// Full bake on the target component: SaveTextures -> Regenerate pattern
	// (cleanup / delete files / export / physics reload / render re-init) ->
	// re-register the hook -> clear session-dirty -> rebuild grass.
	void BakeFull();

	//--------------------------------------------------------------------------
	// Queries
	//--------------------------------------------------------------------------

	// Bilinear height sample, world Y. World X/Z == heightfield pixel coords.
	float SampleHeightWorld(float fWorldX, float fWorldZ) const;
	// Bilinear normalized sample at heightmap pixel coordinates.
	float SampleHeightNorm(float fPixelX, float fPixelZ) const;

	// Ray-march the live heightfield (AABB clip, fixed 2m steps, bisection
	// refine). The heightfield — not physics, not the GPU mesh — is the
	// authority, so the cursor tracks unbaked edits exactly.
	bool RaycastHeightfield(const Zenith_Maths::Vector3& xOrigin,
		const Zenith_Maths::Vector3& xDir, Zenith_Maths::Vector3& xHitOut) const;

	bool HasCursor() const { return m_bCursorValid; }
	const Zenith_Maths::Vector3& GetCursorPos() const { return m_xCursorPos; }

	const Zenith_Image& GetHeightfield() const { return m_xHeightfield; }
	const Zenith_Vector<u_int8>& GetSplatmap() const { return m_xSplatmap; }
	const Zenith_Image& GetGrassDensity() const { return m_xGrassDensity; }

	//--------------------------------------------------------------------------
	// Undo support (used by Zenith_UndoCommand_TerrainEdit)
	//--------------------------------------------------------------------------

	// Copy / write a sub-rect of one of the CPU maps as raw bytes (floats for
	// Height/GrassDensity, RGBA8 for Splat). WriteMapRegion re-marks dirty
	// chunks / GPU-dirty flags so undo/redo restores visuals too.
	void CopyMapRegion(Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0,
		u_int uW, u_int uH, Zenith_Vector<u_int8>& xOut) const;
	void WriteMapRegion(Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0,
		u_int uW, u_int uH, const u_int8* pData);

	// Live-undo-bytes accounting (commands report their footprint; the editor
	// clears the undo stack with a log when a new command would bust the
	// budget). Called from the undo command's ctor/dtor.
	void OnUndoCommandAllocated(u_int64 ulBytes) { m_ulUndoBytesLive += ulBytes; }
	void OnUndoCommandFreed(u_int64 ulBytes) { m_ulUndoBytesLive = (ulBytes > m_ulUndoBytesLive) ? 0 : m_ulUndoBytesLive - ulBytes; }

	//--------------------------------------------------------------------------
	// Stream-in vertex hook (matches Flux_TerrainStreamingState::ChunkVertexHook).
	// pUser = this. Rewrites Y (absolute, from the live heightfield) and
	// recomputes packed normals/tangents — but ONLY for session-dirty chunks,
	// so untouched chunks keep their baked bytes exactly.
	//--------------------------------------------------------------------------
	static void ChunkVertexHook(void* pUser, uint32_t uChunkX, uint32_t uChunkY,
		void* pVerts, uint32_t uNumVerts, uint32_t uStride);

	bool IsChunkSessionDirty(u_int uChunkIndex) const
	{
		return (m_aulSessionDirtyBits[uChunkIndex >> 6] & (1ull << (uChunkIndex & 63))) != 0;
	}

	// Brush falloff curve, fNormDistance in [0,1] (0 = centre). Public for the
	// unit tests; pure function.
	static float EvaluateFalloff(Zenith_TerrainBrushFalloff eFalloff, float fNormDistance);

	// Mark a heightfield-pixel rect edited: expands by a 2px margin (shared
	// border verts + normal taps), sets pending-evict + session-dirty bits on
	// every overlapped chunk, expands chunk AABBs (expand-only during live
	// editing; bake recomputes exact), and flags the chunk-data re-upload.
	void MarkHeightRegionDirty(float fMinPxX, float fMinPxZ, float fMaxPxX, float fMaxPxZ);

	//--------------------------------------------------------------------------
	// Settings (the panel binds these directly)
	//--------------------------------------------------------------------------
	Zenith_TerrainBrushSettings    m_xBrush;
	Zenith_TerrainProceduralParams m_xProceduralParams;
	Zenith_TerrainErosionParams    m_xErosionParams;
	Zenith_TerrainAutoSplatRule    m_axAutoSplatRules[4];

	// Bake / save status line for the panel (mirrors the component panel's
	// export-status pattern).
	std::string m_strStatus;

private:
	//--------------------------------------------------------------------------
	// Internals — Zenith_TerrainEditor.cpp
	//--------------------------------------------------------------------------

	// Re-resolve the target component from m_uTargetEntity. Returns nullptr
	// (and logs once) when the entity/component no longer exists; never
	// caches across frames.
	Zenith_TerrainComponent* ResolveTargetComponent() const;
	Flux_TerrainStreamingState* ResolveTargetState() const;

	void RegisterHook();
	void ClearHook();

	void EnsureImagesAllocated();
	void FillImagesWithDefaults();  // no dirty/undo side effects
	void LoadImagesFromAssets();   // seed from saved textures (or defaults)

	void ProcessPendingEvictions();  // <= MAX_EVICTIONS_PER_FRAME per frame
	void ForceEvictSessionDirtyChunks();  // Close()/reopen: revert / re-apply visuals

	void HandleViewportInput(const Zenith_TerrainEditorFrameContext& xCtx);
	void DrawBrushCursor() const;

	// Per-tool indicator colour shared by the decal indicator + the line-ring
	// fallback.
	Zenith_Maths::Vector3 GetToolColour() const;

	// Arms the Flux decal editor slot with the brush indicator for this frame
	// (projected per-pixel onto the rendered terrain). Falls back to the line
	// ring while the generated brush texture is unavailable.
	void DrawBrushIndicatorDecal();

	void UpdateChunkAABB(Flux_TerrainStreamingState& xState, u_int uChunkX, u_int uChunkZ);

	// Stroke-undo accumulation (region union + before-capture).
	void AccumulateUndoRect(Zenith_TerrainEditMap eMap, u_int uX0, u_int uY0, u_int uX1, u_int uY1);
	void PushStrokeUndoCommand();

	//--------------------------------------------------------------------------
	// Brush kernels — Zenith_TerrainEditor_Brushes.cpp
	//--------------------------------------------------------------------------
	void ApplyHeightDab(Zenith_TerrainBrushTool eTool, float fPxX, float fPxZ,
		float fRadius, float fStrength, float fToolValue);
	void ApplySplatDab(float fPxX, float fPxZ, float fRadius, float fStrength, u_int uLayer);
	void ApplyGrassDab(float fPxX, float fPxZ, float fRadius, float fStrength, float fTargetDensity);

	// TreePaint: scatters (or erases, bErase) instanced trees on the terrain.
	// Trees live as two scene entities ("TerrainTrees_Trunk"/"_Leaves", one
	// Zenith_InstancedMeshComponent each — instance groups are single-material,
	// so opaque bark + alpha-tested leaves need a pair) kept in strict
	// LOCKSTEP: every spawn/remove hits both with identical arguments so
	// instance IDs stay aligned. Windowed-only (instances need GPU buffers);
	// NOT part of the terrain undo system.
	void ApplyTreeDab(float fWorldX, float fWorldZ, float fRadius, float fStrength, bool bErase);
	bool EnsureTreeEntities();
	void TickTreeSway(float fDt);   // editor-mode VAT time advance (Playing uses OnUpdate)

	//--------------------------------------------------------------------------
	// Erosion slicing state — Zenith_TerrainEditor_Erosion.cpp
	//--------------------------------------------------------------------------
	void StepErosionSlice();
	void RunHydraulicDroplets(u_int uFirstDroplet, u_int uCount);
	void RunThermalRows(u_int uFirstRow, u_int uRowCount);

	//--------------------------------------------------------------------------
	// Grass rebuild (stroke end / bake) — Zenith_TerrainEditor_Bake.cpp
	//--------------------------------------------------------------------------
	void RebuildGrass();
	bool ResolveValidatedTargetForTerrainRoot(const std::string& strTerrainRoot,
		std::string& strDirectoryOut) const;
	bool SaveTexturesForTerrainRoot(const std::string& strTerrainRoot);
	void BakeMeshesForTerrainRoot(const std::string& strTerrainRoot);
	bool BakeFullForTerrainRoot(const std::string& strTerrainRoot);

	//--------------------------------------------------------------------------
	// Data
	//--------------------------------------------------------------------------
	bool m_bActive = false;
	bool m_bEditModeEnabled = false;
	bool m_bConsumedViewportInput = false;
	bool m_bSessionDirty = false;          // any unbaked edit this session
	Zenith_EntityID m_uTargetEntity = INVALID_ENTITY_ID;
	std::string m_strAssetSet;
	std::string m_strAssetSetValidationError;

	// Brush-indicator decal texture, lazily resolved on first cursor draw
	// (the file is regenerated at boot by RegenerateBrushTextures). Owning handle so
	// the texture survives UnloadUnused while the editor holds it.
	TextureHandle m_xBrushIndicatorTexture;
	bool m_bBrushIndicatorLoadAttempted = false;

	// TreePaint targets — EntityIDs only (revalidated every use, same contract
	// as the terrain target). Created on first paint in the active scene.
	Zenith_EntityID m_uTreeTrunkEntity = INVALID_ENTITY_ID;
	Zenith_EntityID m_uTreeLeavesEntity = INVALID_ENTITY_ID;
	u_int m_uTreeRngState = 0x51A7E5u;   // interactive scatter randomness

	Zenith_Image m_xHeightfield;           // 4096x4096 float [0,1]
	Zenith_Vector<u_int8> m_xSplatmap;     // 2048x2048x4 RGBA8
	Zenith_Image m_xGrassDensity;          // 1024x1024 float [0,1]

	u_int64 m_aulPendingEvictBits[uCHUNK_COUNT / 64] = {};
	u_int64 m_aulSessionDirtyBits[uCHUNK_COUNT / 64] = {};

	bool m_bSplatGPUDirty = false;
	bool m_bGrassDirty = false;

	// Brush cursor (viewport ray hit on the live heightfield).
	bool m_bCursorValid = false;
	Zenith_Maths::Vector3 m_xCursorPos = { 0.0f, 0.0f, 0.0f };

	// Stroke state: dabs are applied at fixed world-spacing along the cursor
	// path (radius * 0.25) while LMB is held.
	bool m_bStrokeActive = false;
	bool m_bStrokeHasLastPos = false;
	Zenith_Maths::Vector2 m_xStrokeLastPos = { 0.0f, 0.0f };

	// First-dab anchor for the Ramp tool (corridor interpolates from the
	// stroke-start height to each dab's pre-edit height).
	bool m_bStrokeStartCaptured = false;
	Zenith_Maths::Vector2 m_xStrokeStartPos = { 0.0f, 0.0f };
	float m_fStrokeStartHeightNorm = 0.0f;

	// Per-stroke undo accumulation: union texel rect per touched map, plus
	// pre-stroke byte captures of every 64x64 tile a dab touched (captured at
	// most once per stroke, BEFORE the kernel writes it). EndStroke composes
	// the before-region from the tiles, so memory scales with touched area —
	// never the whole 64 MB map.
	struct StrokeUndoRegion
	{
		bool m_bTouched = false;
		u_int m_uX0 = 0, m_uY0 = 0, m_uX1 = 0, m_uY1 = 0;  // inclusive texel rect
		Zenith_Vector<u_int64> m_xCapturedTileBits;        // tile-grid bitset
		Zenith_Vector<u_int>   m_xTileIndices;             // capture order
		Zenith_Vector<u_int8>  m_xTileData;                // pre-stroke tile bytes
	};
	StrokeUndoRegion m_axStrokeUndo[3];   // indexed by Zenith_TerrainEditMap
	u_int64 m_ulUndoBytesLive = 0;

	// Stamp buffer (square). Stamping is additive relative to the captured
	// region's MINIMUM height (the feature's base), so cloning a hill onto
	// flat ground reproduces the hill instead of cancelling at its peak.
	Zenith_Vector<float> m_xStampData;
	u_int m_uStampSize = 0;
	float m_fStampReferenceHeight = 0.0f;

	// Erosion slicing.
	bool m_bErosionRunning = false;
	Zenith_TerrainErosionParams m_xActiveErosion;
	u_int m_uErosionDropletsDone = 0;
	u_int m_uErosionThermalRowsDone = 0;   // across all iterations
	u_int m_uErosionThermalRowsTotal = 0;

	friend class Zenith_UndoCommand_TerrainEdit;
	friend class Zenith_UnitTests;
};

#endif // ZENITH_TOOLS
