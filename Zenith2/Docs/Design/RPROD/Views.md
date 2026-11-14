# RPROD design: render views, depth modes, shading paths and view extensions

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies the public view types with their contract blocks, the view registry and per-view persistent state, view-list construction with shadowed-light selection and the round-robin scheduler, the depth-mode operations, shading-path selection and the capability matrix data file, advanced multi-view, and the view extension point through which the editor and modules add passes to views.

## Scope of this file

The specification is `Zenith2/Docs/Rendering.md → "Render views"` (every subsection) and "Shading paths and capability matrix". `Flux_CapabilityTier` is used throughout as the ZenithFlux alias of RHI's `Zenith_RHICapabilityTier` (`Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHICapabilities header"`, cross-doc item X73), with `FLUX_CAPABILITY_TIER_DESKTOP`/`_MOBILE` the aliases of `ZENITH_RHI_CAPABILITY_TIER_DESKTOP`/`_MOBILE`. Declarations the specification gives are cited, not repeated; this file adds contract blocks and the members and types the specification leaves unnamed, each marked `<!-- elaboration -->`. Code lives in `Zenith2/Engine/ZenithFlux/Internal/Views/` and `Zenith2/Engine/ZenithFlux/Internal/Paths/`.

## Flux_View header

`Zenith2/Engine/ZenithFlux/Flux_View.h` declares, exactly as `Zenith2/Docs/Rendering.md → "Render views"` gives them, `Flux_View`, `Flux_ViewCamera`, `Flux_ViewExtent`, `Flux_ViewOutputs`, `Flux_ViewID`, `Flux_MakeViewID`, `Flux_ViewPersistentState`, `Flux_ViewKind` and `Flux_ViewFeature`, and adds the types below. `Flux_ViewOutputs::m_xAtlasTile` is of LIGHT's type `Flux_AtlasTile`, declared once in LIGHT's leaf header `Flux_ShadowAtlas.h` (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_ShadowAtlas header"`); this header includes it and declares no second tile type.

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_View.h (additions)
#pragma once
#include "ZenithCore/Zenith_Maths.h"
#include "ZenithFlux/Flux_DepthMode.h"
#include "ZenithFlux/Flux_Quality.h"
#include "ZenithFlux/Flux_ShadingPath.h"
#include "ZenithFlux/Flux_LightSettings.h"         // LIGHT's Flux_CameraLightingSettings
#include "ZenithFlux/Flux_ShadowAtlas.h"           // LIGHT's leaf header: Flux_AtlasTile ("Flux_AtlasTile include" below)
#include "ZenithRHI/Zenith_DeviceHandle.h"

enum Flux_ProjectionKind : uint8 { FLUX_PROJECTION_PERSPECTIVE = 1, FLUX_PROJECTION_ORTHOGRAPHIC = 2 };
enum Flux_ViewOutputKind : uint8 { FLUX_VIEW_OUTPUT_SWAPCHAIN = 1, FLUX_VIEW_OUTPUT_TEXTURE, FLUX_VIEW_OUTPUT_CUBEMAP_FACE, FLUX_VIEW_OUTPUT_SHADOW_ATLAS_TILE };

struct Flux_Viewport { uint32 m_uX = 0; uint32 m_uY = 0; uint32 m_uWidth = 0; uint32 m_uHeight = 0; };   // output pixels; 0 extent: the whole output

struct Flux_ViewLens                          // what Flux_BuildViewList builds a request's projections from, with the view's depth mode
{
	float m_fFovYRadians = 1.0471976f;        // perspective: the vertical field of view, (0, π)
	float m_fOrthoHeightMetres = 10.0f;       // orthographic: the view volume's height, > 0
	float m_fAspectOverride = 0.0f;           // 0: the render extent's width / height
};

enum Flux_ViewError : uint8
{
	FLUX_VIEW_ERROR_KIND_UNAVAILABLE = 1,     // VIEW_KIND_STEREO before Stage 7 ([Z2-N-REND-602])
	FLUX_VIEW_ERROR_INVALID_REQUEST,          // a request with a zero extent, a missing output texture or a NaN camera
	FLUX_VIEW_ERROR_DEPENDENCY_CYCLE,         // texture views reading each other (Stage 5, "Advanced multi-view")
	FLUX_VIEW_ERROR_REGISTRY_FULL,            // more than uFLUX_MAX_PERSISTENT_VIEWS live persistent states
	FLUX_VIEW_ERROR_OUT_OF_MEMORY,            // Flux_ViewRegistry::Create's Zenith_TryNew or a table reservation failed ([Z2-N-CONTRACT-340])
};

/// The render-world element through which simulation asks for a view (Rendering.md "Purpose and scope",
/// Flux_RenderWorldSection::m_axViewRequests). Main, editor, capture, preview and texture requests only: shadow views
/// are derived by Flux_BuildViewList.
struct Flux_ViewRequest
{
	Flux_ViewKind m_eKind;
	uint64 m_ulOwnerID;                      // the owner of the table "View identity and persistent state"
	uint32 m_uSubIndex;
	Flux_ViewCamera m_xCamera;                // the request fills m_xView, m_fNear, m_fFar, m_eProjection; the view list the rest
	Flux_ViewLens m_xLens;                    // the projection parameters ("View list construction" step 4)
	Flux_ViewOutputs m_xOutputs;
	uint32 m_uOutputWidth;
	uint32 m_uOutputHeight;
	Flux_Viewport m_xViewport;
	uint32 m_uFeatureMaskSet;                // Flux_ViewFeature bits a camera enables beyond its kind (depth of field, motion blur)
	uint32 m_uFeatureMaskClear;              // bits it disables
	uint32 m_uUpdateIntervalFrames;          // capture and texture views: the requested interval, 1 = every frame
	Flux_QualityOverrides m_xQualityOverrides;
	uint32 m_uEditorViewportID;              // VIEW_KIND_EDITOR: the viewport; 0 otherwise
	uint32 m_uReadsViewCount;                // texture-view inputs this view samples (Stage 5)
	Flux_ViewID m_axReadsViews[4];
	Flux_CameraLightingSettings m_xCameraLighting;   // LIGHT's (row L2): exposure, depth of field, grading overrides of the camera
	bool m_bCameraCut;                        // set by Render.ExtractCameras on a cut request, a camera switch, and a teleport of the
	                                          // camera's entity or an ancestor (LIGHT row L17); copied into m_xCamera.m_bCameraCut
};

inline constexpr uint32 uFLUX_VIEW_STATE_RETIRE_FRAMES = 60;      // [Z2-N-REND-600]
inline constexpr uint32 uFLUX_MAX_PERSISTENT_VIEWS = 256;
inline constexpr uint32 uFLUX_MAX_VIEWS_PER_FRAME = 128;           // the hard ceiling above every tier cap
```

Contract blocks:

```cpp
/// @contract value Flux_View
/// Ownership: Plain value built by Flux_BuildViewList in the frame arena; m_pxPersistent borrows state owned by
///   Flux_ViewRegistry.
/// Thread affinity: IMMUTABLE after Flux_BuildViewList returns ([Z2-N-REND-590]); feature functions read it on the build
///   node and record jobs read it through Flux_RasterContext::GetView.
///   Members non-const: Flux_BuildViewList fills each view field by field in the frame arena.
/// Invalidation: Frame-scoped; the persistent-state pointer is valid for the frame and invalid after the registry
///   retires the view.
/// Units: Matrices map metres (world) to clip space with the convention of Zenith2/Docs/Design/CORE/Maths.md
///   "Coordinate systems"; extents in pixels; m_fDynamicResolutionScale in (0, 1].
/// Errors: None; invalid requests are rejected by Flux_BuildViewList.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract

/// @contract value Flux_ViewID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: None.
/// Units: None; a 64-bit hash of (kind, owner, sub-index) ([Z2-N-REND-597]).
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized: it is recomputed from its inputs every frame, so a D-SEL-HASH (open,
///   Stage 1) change moves every ID at once without breaking stability within a build.
/// @endcontract

/// @contract value Flux_ViewRequest
/// Ownership: Plain value copied into the render world through Flux_RenderWorldWriter::AddViewRequest: by the engine camera
///   extractor Render.ExtractCameras for main views and gameplay texture views (Zenith2/Docs/Design/RPROD/Extractors.md
///   "View request fill"), by the owning area's extractor for editor, capture and preview requests.
/// Thread affinity: IMMUTABLE once the render world is sealed.
///   Members non-const: the extractor fills each request field by field before AddViewRequest copies it.
/// Invalidation: None.
/// Units: As Flux_View.
/// Errors: A request that fails the checks of "View list construction" step 1 is dropped with FLUX_VIEW_ERROR_INVALID_REQUEST
///   logged once per owner and counted in Flux.Views.Rejected.
/// Version: since Z2 Stage 2.
/// @endcontract
// Flux_ViewCamera, Flux_ViewExtent, Flux_ViewOutputs, Flux_Viewport and Flux_ViewPersistentState carry the
// value block of Flux_View; the four enumerations carry an IMMUTABLE enum block; Flux_ViewError's Errors field lists
// when each enumerator is returned (the comments above).
```

### Flux_AtlasTile include

`Flux_View.h` needs `Flux_AtlasTile` complete (a member of `Flux_ViewOutputs`). LIGHT's `Flux_Shadows.h` includes `Flux_RenderGraph.h` and uses `Flux_ViewCamera`, `Flux_ViewContext` and `Flux_ViewList`, so including it from `Flux_View.h` would make an include cycle. LIGHT's revision answered reconciliation row RP18 (its rows L29 and L38): `Flux_AtlasTile`, the allocator, `Flux_ShadowViewKind`, `Flux_ShadowViewPlan` and `Flux_RenderLightKey` are declared in the leaf header `ZenithFlux/Flux_ShadowAtlas.h`, which includes only ZenithCore and `Flux_LightingErrors.h` (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_ShadowAtlas header"`). `Flux_View.h` includes it for the tile and `Flux_ViewContext.h` includes it for `Flux_ViewList::m_axShadowPlans`, neither through `Flux_Shadows.h`, so no cycle forms. The types stay declared once, by LIGHT.

`Flux_ViewRequest::m_xCameraLighting` is LIGHT's row L2, in Rendering's render-world text since X103 (b); `Flux_ViewRequest` is this design's type, so the member lands with it.

## Flux_ViewContext header

`Zenith2/Engine/ZenithFlux/Flux_ViewContext.h` declares `Flux_FrameShared`, `Flux_ViewContext`, `Flux_ViewTargets`, `Flux_BuildViewList` and `Flux_BuildView` exactly as `Zenith2/Docs/Rendering.md → "Render views"` gives them, and adds:

<!-- elaboration -->
```cpp
struct Flux_ViewList
{
	Zenith_Span<const Flux_View> m_axViews;               // in the order of Rendering's table "View list"
	uint32 m_uMainCount;                                  // 0 or 1
	uint32 m_uShadowFirst, m_uShadowCount;
	uint32 m_uEditorFirst, m_uEditorCount;
	uint32 m_uCaptureFirst, m_uCaptureCount;
	uint32 m_uPreviewFirst, m_uPreviewCount;
	uint32 m_uTextureFirst, m_uTextureCount;
	uint32 m_uDeferredCount;                              // requests over a cap, counted in Flux.Views.Deferred
	Zenith_Span<const Flux_ShadowViewPlan> m_axShadowPlans; // per shadow view, in list order: LIGHT's plan (light slot, kind, cascade or face, tile, matrices, record index)
	Zenith_Span<const Flux_TransparentList> m_axTransparent;   // per view: its sorted Flux_TransparentEntry span and runs (Culling.md)
};

class Flux_ViewRegistry       // owned by Flux_Renderer
{
public:
	static Zenith_Result<Zenith_Owned<Flux_ViewRegistry>, Flux_ViewError> Create(Zenith_RHIDevice& xDevice, Flux_PersistentResourcesForViews& xFactory,
		Flux_LightingSystem* pxLighting);                                           // borrowed, outlives the registry; null until the renderer creates LIGHT's system (Stage 3)
	Flux_LightingSystem* GetLightingSystem();                         // for Flux_BuildViewList steps 4 and 5; null: scale 1.0, no jitter, no shadow views
	Zenith_Result<Flux_ViewPersistentState*, Flux_ViewError> Acquire(Flux_ViewID xID, Flux_ViewKind eKind, uint32 uRenderWidth, uint32 uRenderHeight, uint64 ulFrameIndex);
	void EndFrame(uint64 ulFrameIndex);          // flips ping-pong indices, retires states unused for 60 frames
	void Invalidate(Flux_ViewID xID);             // camera cut, path switch
	void ReleaseKinds(uint32 uKindMask);         // pressure action 12 (capture, preview)
	void SetCreationDeferred(bool bDeferred);     // pressure action 13
	uint32 GetLiveCount() const;
	uint64 GetPersistentBytes() const;   // reported against Z2-B-RHI-001
};
```

`Flux_PersistentResourcesForViews` is the narrow factory the registry creates history textures and buffers through: a thin wrapper, in `Internal/Views/`, over `Zenith_RHIDevice::CreateTexture`/`CreateBuffer` with `DEVICE_DEDICATED` memory and registration in the imported-state table (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Imported-state table"`), so histories are ordinary imports ([Z2-N-REND-295]). Registration happens in `Acquire` on the `Flux.BuildGraph` node, the imported-state table's single writer (RHI row R17 (b)); a retired history is unregistered only by the retirement listener, and its release outside the render-submit thread goes through `Zenith_RHIRetirementQueue::RequestReleaseWithDescriptor`, so its descriptor slot takes the drain values (RHI row R17 (c); `Zenith2/Docs/Design/RHI/Memory.md → "Descriptor heap and slot allocation"`).

```cpp
/// @contract type Flux_ViewRegistry
/// Ownership: Created by Flux_Renderer through Create; owns every Flux_ViewPersistentState and its GPU resources;
///   borrows the renderer's Flux_LightingSystem when one exists (LIGHT's, created before it and destroyed after it; a
///   null pointer means no lighting system, and the view list then uses scale 1.0, no jitter and no shadow views); destroyed with
///   the renderer after the last frame retired ([Z2-N-REND-598]).
/// Thread affinity: OWNER_THREAD (the Flux.BuildGraph node); ReleaseKinds and SetCreationDeferred are called by the
///   rendering memory consumer on the main thread in the frame prologue, when no render subgraph runs.
/// Invalidation: A state pointer is valid until EndFrame retires its view; its resources retire through deferred
///   deletion with the last-use values of the last frame that used them ([Z2-N-REND-600]).
/// Units: Frames; bytes.
/// Errors: Create returns FLUX_VIEW_ERROR_OUT_OF_MEMORY when its own allocation fails; Acquire returns
///   FLUX_VIEW_ERROR_REGISTRY_FULL beyond 256 live states; a GPU allocation failure maps to the view rendering with an
///   invalid history this frame and retrying next frame.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
```

## Flux_DepthMode header

`Zenith2/Engine/ZenithFlux/Flux_DepthMode.h` declares `Flux_DepthMode` exactly as Rendering gives it and the operations [Z2-N-REND-595] names:

<!-- elaboration -->
```cpp
enum Flux_ZDirection : uint8 { FLUX_Z_DIRECTION_STANDARD = 1, FLUX_Z_DIRECTION_REVERSED = 2 };
enum Flux_DepthFormat : uint8 { FLUX_DEPTH_FORMAT_D24S8 = 1, FLUX_DEPTH_FORMAT_D32F_S8 = 2 };
enum Flux_DepthModeReason : uint8 { FLUX_DEPTH_MODE_REASON_DEFAULT = 1, FLUX_DEPTH_MODE_REASON_PRECISION = 2, FLUX_DEPTH_MODE_REASON_CAPABILITY = 3 };
struct Flux_ClearValue;   // Flux_RenderGraph.h, which includes this header; complete wherever GetClearValue is called or defined

/// @contract type Flux_DepthModeOps
/// Ownership: A struct of static pure functions; never instantiated.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres for planes and linear depth; device depth in [0, 1].
/// Errors: A near plane ≤ 0 or a far plane ≤ near (finite) is ZENITH_ASSERT; the view list rejects such requests first.
/// Version: since Z2 Stage 2. The one place a depth compare operator, clear value or linearisation is chosen ([Z2-N-REND-595]).
/// @endcontract
struct Flux_DepthModeOps
{
	// The operations [Z2-N-REND-595] names, with its names
	static float GetClearDepth(const Flux_DepthMode& xMode);                       // 1.0 standard, 0.0 reversed
	static Zenith_RHICompareOp GetOpaqueCompare(const Flux_DepthMode& xMode);      // LESS_OR_EQUAL / GREATER_OR_EQUAL
	static Zenith_RHICompareOp GetColourCompare(const Flux_DepthMode& xMode);      // EQUAL
	static Zenith_RHICompareOp GetInvertedCompare(const Flux_DepthMode& xMode);    // GREATER_OR_EQUAL / LESS_OR_EQUAL
	static Zenith_Matrix4 BuildProjection(const Flux_DepthMode& xMode, Flux_ProjectionKind eProjection,
		float fFovYOrHeight, float fAspect, float fNear, float fFar);                              // Zenith_MakePerspective* / Zenith_MakeOrthographic
	static float LineariseDepth(const Flux_DepthMode& xMode, Flux_ProjectionKind eProjection, float fDevice, float fNear, float fFar);
	// Elaboration members
	static Zenith_RHIFormat GetFormat(const Flux_DepthMode& xMode);                // D24_UNORM_S8_UINT / D32_FLOAT_S8X24_UINT
	static Flux_ClearValue GetClearValue(const Flux_DepthMode& xMode);             // { depth GetClearDepth(xMode), stencil 0 }
};

/// @contract functions Flux_DepthModeSelection
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres.
/// Errors: As Flux_DepthModeOps.
/// Version: since Z2 Stage 2.
/// @endcontract
Flux_DepthMode Flux_SelectDepthMode(Flux_ViewKind eKind, Flux_ProjectionKind eProjection, float fNear, float fFar,
	bool bDeviceHasD24S8, float fPrecisionToleranceMetres);
float Flux_D24FarStepMetres(float fNear, float fFar);         // Δz(f) = 2^-24 · f · (f − n) / n
```

`Flux_SelectDepthMode` implements [Z2-N-REND-592] to [Z2-N-REND-594] in this order: an orthographic view or any `VIEW_KIND_SHADOW` view takes standard Z; a perspective non-shadow view takes reversed Z when `fFar` is infinite or `Flux_D24FarStepMetres(fNear, fFar)` exceeds the tolerance (`Z2-B-RPROD-029`, from `r.depth.precisionToleranceMetres`, reason `PRECISION`), else standard Z; the format is D32F_S8 for every reversed view and for a standard view when `bDeviceHasD24S8` is false (reason `CAPABILITY`), else D24S8 (reason `DEFAULT`). The projection builders are CORE's (`Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`), the Slang twins are `Zenith2/Shaders/Common/DepthMode.slang`, and `Test_View_DepthModeOpsMatchSlang` compares both at 1,000 depths per mode.

## Flux_ShadingPath header

`Zenith2/Engine/ZenithFlux/Flux_ShadingPath.h` declares `Flux_ShadingPath` and `Flux_SelectShadingPath` exactly as Rendering gives them ([Z2-N-REND-690], [Z2-N-REND-691]) and adds the capability-matrix queries:

<!-- elaboration -->
```cpp
enum Flux_PathFeature : uint8 { /* one enumerator per row of Rendering's capability matrix, FLUX_PATH_FEATURE_ prefix, in row order */ };
enum Flux_MatrixCell : uint8 { FLUX_MATRIX_CELL_NO = 0, FLUX_MATRIX_CELL_YES = 1, FLUX_MATRIX_CELL_CONDITIONAL = 2 };   // CONDITIONAL: "when m_bMeshShaders", "prepass"-style qualified yes
enum Flux_MatrixColumn : uint8 { FLUX_MATRIX_COLUMN_FORWARD_PLUS_WIN = 0, FLUX_MATRIX_COLUMN_FORWARD_PLUS_S24, FLUX_MATRIX_COLUMN_FORWARD_WIN,
	FLUX_MATRIX_COLUMN_DEFERRED_WIN, FLUX_MATRIX_COLUMN_VISIBILITY_BUFFER_WIN, FLUX_MATRIX_COLUMN_COUNT };

/// @contract functions Flux_CapabilityMatrix
/// Ownership: A constexpr table generated at build time from Internal/Paths/CapabilityMatrix.json.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The JSON, Rendering's table and this table change together ([Z2-N-REND-695]).
/// @endcontract
Flux_MatrixCell Flux_GetMatrixCell(Flux_PathFeature eFeature, Flux_MatrixColumn eColumn);
Flux_MatrixColumn Flux_GetMatrixColumn(Flux_ShadingPath ePath, Flux_CapabilityTier eTier);
uint32 Flux_GetPathFeatureMask(Flux_ShadingPath ePath, Flux_CapabilityTier eTier);     // Flux_ViewFeature bits the path provides
Zenith_StringView Flux_GetParityRule(Flux_PathFeature eFeature);                        // the "Parity comparison" cell
```

`Flux_SelectShadingPath(eKind, xSettings, eTier)` returns Forward+ for every view on mobile; on desktop Forward for preview and capture views, and `Flux_RendererSettings::m_eShadingPath` otherwise ([Z2-N-REND-691]). Interim rule (ruling Q83 (a), 2026-10-09): until the Forward path lands at Stage 5 (`Flux_BuildForwardView` with its passes), desktop preview and capture views also return Forward+, because the Forward path function is a no-pass stub until then and would leave every desktop capture and preview view, LIGHT's probe captures included, drawing nothing; the capture and preview feature masks are subsets Forward+ already serves on mobile. `Test_Paths_PreviewUsesForwardPlusUntilForwardPath` pins the interim rule; the diff that lands the Forward path switches those views to Forward, replaces that test with `Test_Paths_PreviewUsesForward` and deletes this paragraph's interim sentences. Rendering's [Z2-N-REND-691] carries the matching stage note (`Zenith2/Docs/Rendering.md`, cross-doc item X266, applied by core-doc patch g); a desktop setting of a path the matrix marks unavailable cannot occur, because `r.shadingPath` validates against the matrix at the configuration store (an enumeration key whose allowed values are generated from the matrix).

## Capability matrix data file

<!-- elaboration -->
`Zenith2/Engine/ZenithFlux/Internal/Paths/CapabilityMatrix.json`, schema version 1:

```json
{
	"schemaVersion": 1,
	"columns": ["forwardPlusWin", "forwardPlusS24", "forwardWin", "deferredWin", "visibilityBufferWin"],
	"rows": [
		{ "feature": "HIZ_INSTANCE_CULLING", "label": "Two-phase HiZ instance culling", "cells": ["yes", "yes", "yes", "yes", "yes"],
		  "viewFeature": "FLUX_VIEW_FEATURE_HIZ_OCCLUSION", "parity": "draw counts equal across paths" },
		{ "feature": "MESH_SHADERS", "label": "Mesh shaders", "cells": ["no", "no", "no", "no", "conditional:m_bMeshShaders"],
		  "viewFeature": null, "parity": "fallback compared with mesh-shader output: bit-exact visibility ID" }
	]
}
```

| Rule | Specification |
|---|---|
| rows | exactly the rows of Rendering's matrix, in order, with `label` equal to the row's first cell text |
| cells | `yes`, `no`, or `conditional:<condition>` for a qualified yes (`when m_bMeshShaders`, `prepass`, `depth only`, `G-buffer pass`, `reconstructed from the visibility ID`, `no until D-TBDR`) |
| generator | `Internal/Paths/GenerateCapabilityMatrix.cmake`, output `${ZENITH_OUT}/generated/ZenithFlux/Flux_CapabilityMatrix.generated.h` (declared through `GENERATES`); a malformed file fails the build naming the line |
| consumers | `Flux_SelectShadingPath`, feature masks ([Z2-N-REND-601]), the path functions (a "no" feature adds no pass, [Z2-N-REND-696]), the PathParity comparison rules |
| test | `Test_Paths_MatrixMatchesData` reads `Zenith2/Docs/Rendering.md` from the source tree (the test target's `ZENITH_SOURCE_DIR` definition), parses the "Capability matrix" table and compares every row and cell with the generated table ([Z2-N-REND-697]) |

## View registry

| Event | Action |
|---|---|
| first appearance of an ID | create the kind's histories ("Persistent state by kind"), `m_bHistoryValid = false`; under pressure action 13 the state is not created and the view renders with an invalid history every frame until the level falls |
| each frame the view is listed | `m_ulLastUsedFrame = frame`; histories valid unless one of the invalidation events below occurred this frame |
| camera cut (`m_bCameraCut`) | `m_bHistoryValid = false` for this frame; the TAA, exposure, SSR, velocity and HiZ histories are treated as absent ([Z2-N-REND-009], [Z2-N-REND-599]) |
| output-extent change | recreate every extent-dependent resource at the new extent; `m_bHistoryValid = false` ([Z2-N-REND-599]) |
| render-extent change alone (a dynamic-resolution step) | keep `m_bHistoryValid`, the output-extent histories (TAA, exposure) and the UV-sampled SSR previous colour; recreate each render-extent history (velocity, the HiZ half becoming current, and in Stage 5 SSGI) at the new extent when it next becomes current; the consumers resample the previous half by UV with its own extent ([Z2-N-REND-599], [Z2-N-REND-612]; LIGHT: `Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic resolution and history validity"`; HiZ: `Zenith2/Docs/Design/RPROD/Culling.md → "HiZ pyramid resources"`) |
| shading-path change | `m_bHistoryValid = false` ([Z2-N-REND-692]) |
| `EndFrame` | `m_uHistoryIndex ^= 1` for every listed view; a state unused for 60 consecutive frames is released through deferred deletion ([Z2-N-REND-600]) |
| pressure action 12 | capture and preview states released; they render their next frame invalid |

The two extent rows above are [Z2-N-REND-599] as X104 narrowed it (LIGHT row L18); [Z2-N-REND-611]'s `Test_View_OutputExtentChangeResetsHistory` carries the case "a dynamic-resolution step from 1.0 to 0.9 keeps `m_bHistoryValid`".

### Persistent state by kind

| Kind | TAA ×2 (RGBA16F, output extent) | HiZ ×2 (RG16F) | visibility bits ×2 | exposure (16 B) | SSR colour (R11G11B10F) | velocity ×2 (RG16F) | fog scattering ×2 (RGBA16F 3D, the froxel grid) | SSGI ×2 (RGBA16F, half render extent; Stage 5) |
|---|---|---|---|---|---|---|---|---|
| `VIEW_KIND_MAIN`, `VIEW_KIND_EDITOR`, `VIEW_KIND_TEXTURE` | yes | yes | yes | yes | yes | yes | when the mask carries `FLUX_VIEW_FEATURE_VOLUMETRIC_FOG` | desktop tiers with SSGI enabled |
| `VIEW_KIND_SHADOW` | no | where the tier enables shadow HiZ | where the tier enables shadow HiZ | no | no | no | no | no |
| `VIEW_KIND_CAPTURE` | no | yes | yes | no | no | no | no (a capture takes no fog history) | no |
| `VIEW_KIND_PREVIEW` | no | no | no | no | no | no | no | no |

The last two columns are LIGHT's histories (rows L7 and L15): `Flux_ViewPersistentState::m_axFogScattering[2]` and, Stage 5, `m_axSSGIHistory[2]` are members of Rendering's struct since X103 (a), and X105 added the SSGI pass to the desktop frames. The registry creates them for the kinds above through `Flux_PersistentResourcesForViews`, at the extents LIGHT's settings give (`Flux_FogSettings::m_auGrid`; half the render extent), and releases them with the other histories.

Each editor viewport's state is its own, so no history resource is ever shared between views ([Z2-N-REND-598]); `Test_View_EditorHistoryIndependentOfMain` renders the main view and two editor viewports and asserts the main view's TAA history bit-identical to a run without the editor viewports.

## View list construction

`Flux_BuildViewList(xWorld, xSettings, xRegistry, xFrameArena)` runs on the build node:

| Step | Action |
|---|---|
| 1 | gather every section's view requests in world-tag order; drop invalid ones (a zero output extent on an output other than `FLUX_VIEW_OUTPUT_SWAPCHAIN`, a missing output texture, a NaN or non-finite view matrix or lens, near ≤ 0) with `FLUX_VIEW_ERROR_INVALID_REQUEST`; reject `VIEW_KIND_STEREO` with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE` until Stage 7 ([Z2-N-REND-602]) |
| 2 | order the requests: the main view, then editor (ascending viewport ID), captures, previews, texture views, as Rendering's table "View list" fixes ([Z2-N-REND-603]) |
| 3 | apply the caps: the quality tier's `m_uMaxViewsPerFrame` (`Z2-B-RPROD-022`) and `m_uMaxTextureAndCaptureViews` (`Z2-B-RPROD-023`); capture and texture requests are chosen by "Round-robin scheduler"; the main view and every shadow view of a visible shadowed light are never deferred ([Z2-N-BUDGET-262]); deferred requests count in `Flux.Views.Deferred` ([Z2-N-REND-608]) |
| 4 | per surviving request: `Flux_MakeViewID`, `Flux_SelectShadingPath`, `Flux_SelectDepthMode`; a `FLUX_VIEW_OUTPUT_SWAPCHAIN` output with a zero size takes the main swapchain's current extent and a zero viewport the whole output; `m_xCamera.m_xProjection = Flux_DepthModeOps::BuildProjection(mode, m_eProjection, m_xLens.m_fFovYRadians or m_fOrthoHeightMetres, aspect, m_fNear, m_fFar)` with aspect `m_fAspectOverride` or the render extent's ratio, `m_xJitteredProjection` from it by LIGHT's jitter (`Zenith2/Docs/Design/LIGHT/Temporal.md`; equal to `m_xProjection` while `GetLightingSystem()` is null), `m_xCamera.m_bCameraCut` = the request's `m_bCameraCut`; `Flux_View::m_xCameraLighting` = the request's `m_xCameraLighting` (LIGHT row L36; the `Flux_View` member is cross-doc item X148); the extent (`ceil(output × s)` per axis, [Z2-N-REND-591], where s is `xRegistry.GetLightingSystem()->GetDynamicResolution(xID).GetScale()` for a view LIGHT gives a dynamic-resolution controller, which alone writes the scale and clamps it to the floor, and 1.0 for every other view and while `GetLightingSystem()` is null), the feature mask ([Z2-N-REND-601]: the kind's default ∩ the tier's enabled features ∩ `Flux_GetPathFeatureMask`, then the request's set and clear bits), the previous-frame camera members from the registry's copy of last frame's camera, `xRegistry.Acquire` |
| 5 | shadow views: "Shadowed-light selection", inserted after the main view (list order 2 of Rendering's table) |
| 6 | transparent collection for every view with `FLUX_VIEW_FEATURE_TRANSPARENT` (`Zenith2/Docs/Design/RPROD/Culling.md → "Transparent collection and emission"`) |
| 7 | the view list is immutable from here ([Z2-N-REND-590]) |

## Composite-only editor view

<!-- assumption: pending core-doc (X259 (a), X302) -->
RPROD answers A3 cross-area row ED33 (a) with the first of the two mechanisms X259 (a) offers: a `VIEW_KIND_EDITOR` request whose effective feature mask is exactly `FLUX_VIEW_FEATURE_EDITOR_OVERLAY` is a composite-only view. The editor's shell view is that view: it presents the whole editor window, viewport ID `uEDITOR_SHELL_VIEWPORT_ID` = 0, output `FLUX_VIEW_OUTPUT_SWAPCHAIN`. `Flux_BuildView` gives it a frame of three parts:

1. A clear of the output to opaque black (the ImGui pass then covers the window).
2. `Flux_CallViewExtensions(FLUX_EXTENSION_POINT_OVERLAY)`, the editor's ImGui pass.
3. `Flux_AddPresent`.

It runs no culling, no prologue feature, no scene or lighting pass and no temporal pass, and keeps no history: the registry creates no persistent state for it. View list step 3 orders it first among editor views (its viewport ID is the lowest) and never defers it: it counts against `m_uMaxViewsPerFrame`, and when the cap is reached a later view is deferred instead. A second composite-only request in one frame is refused with `FLUX_VIEW_ERROR_INVALID_REQUEST`. `Test_EditorShellView_CompositeOnly` (WARP, lavapipe) builds a frame with one shell request and one scene view, and asserts that the shell's passes are exactly the clear, the extension pass and `Present`, that the graph holds no culling pass for it, and that with a cap of 1 the scene view is the one deferred. Until the core documents adopt X259 (a) and X302, this section is RPROD's answer recorded for them.
<!-- /assumption -->

## Source world tag and scene-free views

<!-- elaboration -->
<!-- assumption: pending core-doc (X454 (b), (c)) -->
`Flux_ViewContext` gains `uint32 m_uSourceWorldTag`: the `m_uWorldTag` of the render-world section whose `m_axViewRequests` held the view's request, kept by "View list construction" step 1 and copied when step 9 of `Flux.BuildGraph` builds the context; `uFLUX_NO_SOURCE_WORLD` (`0xFFFFFFFF`) for a shadow view, which no request names. Feature functions that read a world's per-world records (UI's composite, TWOD's 2D passes) select them by this tag (rows UI8 (d), TD7).

`Flux_View.h` declares `inline constexpr uint32 uFLUX_VIEW_SCENE_FEATURE_MASK = 0x00023FFFu;`, the bits of every scene pass (`HIZ_OCCLUSION` to `TONEMAP`, and `SKY`). A view of kind `VIEW_KIND_MAIN`, `VIEW_KIND_EDITOR` or `VIEW_KIND_TEXTURE` whose effective feature mask holds `FLUX_VIEW_FEATURE_COLOUR` and no bit of that mask is **scene-free** (TWOD's pure-2D view, `(uFeatureMask & uTWOD_PURE_2D_FEATURE_CLEAR_MASK) == 0`, is one: the two constants are equal, and `Test_View_SceneFeatureMaskMatchesTwoD` compares them):

1. `Flux_BuildView` calls no path function: the view gets no culling, prepass, opaque, lighting, sky, SSR, transparent or particle pass, and the `AFTER_OPAQUE`, `TRANSPARENT` and `BEFORE_UPSCALE` extension points are not called.
2. It declares a clear of `m_xOutput` to opaque black, then calls `Flux_BuildCommonEpilogue`, whose bit-gated rows add no pass, so the frame is the clear, row 6b, the `BEFORE_UI` extensions, `UIComposite`, the `OVERLAY` extensions, `DebugOverlay` and `Present`.
3. The registry creates no persistent state for it.

The composite-only shell view ("Composite-only editor view") has no `COLOUR` bit and keeps its own three-part frame. `Test_View_SceneFreeViewDeclaresNoScenePass` asserts the frame of a scene-free main view on WARP and lavapipe.
<!-- /assumption -->

## Shadowed-light selection

Shadowed-light selection and shadow-atlas allocation happen on the CPU during view-list construction ([Z2-N-REND-604]). The priority rule, the caps, the atlas layout and the allocator are LIGHT's, declared once in LIGHT's shadow design (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadowed-light budget"`, "Atlas allocation", "Flux_Shadows header"); RPROD's view list calls them and turns their output into shadow views:

| Step | Action |
|---|---|
| 1 | after step 4 of "View list construction" has built the non-shadow views, `Flux_BuildViewList` calls `xRegistry.GetLightingSystem()->PlanShadows(xNonShadowViews, xFrameArena)` (the lighting system is non-null from Stage 3; with a null one the step adds no shadow view) (`Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`), which selects the shadowed lights by LIGHT's priority and caps (`Flux_ShadowSettings::m_uMaxShadowedLocalLights`; directional lights always, at most two), allocates their tiles with `Flux_ShadowAtlasAllocator` (a buddy quadtree in size-descending order with halving retries) and publishes one `Flux_ShadowViewPlan` per shadow view in view-list order, with its `Flux_ShadowViewRecord` |
| 2 | a `PlanShadows` error (`FLUX_LIGHTING_ERROR_INVALID_SETTINGS`) leaves the frame with no shadow view, logged once in `flux.views`; a light LIGHT could not place is unshadowed and counted by LIGHT (`Flux.Shadows.Dropped`), and its record carries `m_uShadowIndex = 0xFFFFFFFF` |
| 3 | for each plan, the list gets one `VIEW_KIND_SHADOW` view: identity `Flux_MakeViewID(VIEW_KIND_SHADOW, the light's key, plan.m_uSubIndex)`, camera from the plan's matrices, depth mode from `Flux_SelectDepthMode` (standard Z always, [Z2-N-REND-593]), output `FLUX_VIEW_OUTPUT_SHADOW_ATLAS_TILE` with `m_xAtlasTile = plan.m_xTile`, feature mask `HIZ_OCCLUSION` when `m_xLight.m_xShadows.m_bShadowHiZ`; `Flux_ViewList::m_axShadowPlans` holds the plans in the same order |
| 4 | the tile rectangles reach the light records through LIGHT's staging: `PlanShadows` restages every light whose shadow index changed with `Flux_GPUScene::StageLightRecord` (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow planning in view-list construction"` step 5), and RHI's scatter uploads the staged records before any shadow view renders ([Z2-N-REND-604]) |

The shadow views of visible shadowed lights are never deferred by the per-tier view cap ([Z2-N-BUDGET-262]); LIGHT's local cap is what bounds them. The counter, the tie-break (priority descending, then `Flux_RenderLightKey` ascending) and the packer are LIGHT's alone; RPROD declares no shadow planner, no tile type and no packer.

The headings cited above, `PlanShadows`' signature and the plan output's home (`Flux_LightingFrame::m_axShadowViews`) are LIGHT's landed revision (rows L29 and L38 answering RP18); this section replaces the planner assumption of row RP7, which is withdrawn.

## Round-robin scheduler

Capture and texture requests are scheduled across frames ([Z2-N-REND-609]):

| Step | Action |
|---|---|
| 1 | each request keeps, in the registry, `m_ulLastRenderedFrame` and its interval (`m_uUpdateIntervalFrames`, doubled under memory-pressure action 6) |
| 2 | due requests: `frame − m_ulLastRenderedFrame ≥ interval` (a never-rendered request is due) |
| 3 | order due requests by (frames overdue descending, view ID ascending) and take them up to the tier cap remaining after the main, shadow and editor views |
| 4 | a texture view read by a view rendered this frame whose own request is not taken keeps its last output (its output texture is an import whose version 0 is last frame's content) |

## Advanced multi-view

Stage 5 ([Z2-N-REND-610]) adds, without changing any Stage 2 signature:

| Capability | Realisation |
|---|---|
| many simultaneous captures and probes | `Flux_ViewGroup`: requests sharing an owner and a group flag (the six faces of a probe) are culled against the union of their frusta once on the CPU (a coarse bounds pre-pass over the GPU-scene candidate list) and share one shadow-view set; per-tier caps count a group of six as six views |
| texture views reading texture views | a request lists up to four views it samples (`m_axReadsViews`); the builder orders dependent texture views after their inputs, rejects a cycle with `FLUX_VIEW_ERROR_DEPENDENCY_CYCLE` (every view on the cycle renders its last output), and limits the chain depth to 2 |
| stereo-ready paths | every path and feature function takes one `Flux_ViewContext` and reads nothing shared between views except through `Flux_FrameShared`, and culling outputs are per view; `VIEW_KIND_STEREO` (Stage 7, Z2-D-072) will be a view group of two sharing phase-1 culling, and the Stage 5 test `Test_View_GroupSharesCullingWithoutCrossTalk` proves a two-view group renders each view identically to rendering it alone |

## Off-centre lenses

<!-- elaboration -->
<!-- assumption: pending CORE (row SP3) -->
From Stage 5 `Flux_ViewLens` carries off-centre tangents (row SP1 (d); readiness check XRR-V5 of `Zenith2/Docs/Design/SCOPE/XrReadiness.md → "Multi-view checks"`):

```cpp
// Flux_ViewLens gains (Stage 5)
	float m_fTanLeft = 0.0f;                  // tan of the half-angle on each side; left and down negative
	float m_fTanRight = 0.0f;
	float m_fTanUp = 0.0f;
	float m_fTanDown = 0.0f;                  // all four zero: the symmetric lens of m_fFovYRadians
```

"View list construction" step 4 builds a perspective view's projection from the four tangents when any is non-zero, through CORE's builders `Zenith_MakePerspectiveOffCentreStandardZ`, `Zenith_MakePerspectiveOffCentreReversedZ` and `Zenith_MakePerspectiveOffCentreReversedZInfinite` (`Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`, chosen by the view's depth mode as `Flux_DepthModeOps::BuildProjection` chooses the symmetric ones); `m_fFovYRadians` and `m_fAspectOverride` are then ignored, and LIGHT's jitter is applied to the result as to a symmetric projection. An orthographic view ignores the tangents. A lens with `m_fTanLeft ≥ m_fTanRight`, `m_fTanDown ≥ m_fTanUp` or a non-finite tangent is `FLUX_VIEW_ERROR_INVALID_REQUEST`. `Test_View_OffCentreLensProjection` compares the matrix of each depth mode with the builders' rows.
<!-- /assumption -->

## View test access

<!-- elaboration -->
A module-driven stereo test outside ZenithFlux reads what a frame rendered through one public test-support header (row GmB28; ruling Q93 (b) pattern), Stage 7:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ViewTestAccess.h
class Flux_ViewTestAccess
{
public:
	static bool GetLastFrameViewCamera(const Flux_Renderer& xRenderer, Flux_ViewID xView, Flux_ViewCamera& xOut);   // false: not rendered in the last completed frame
	static bool GetLastFrameXrEyePose(const Flux_Renderer& xRenderer, uint8 uEye, Zenith_Matrix4& xOut);           // the eye's view matrix of the last expanded stereo frame
	static uint64 GetHistoryResetCount(const Flux_Renderer& xRenderer, Flux_ViewID xView);                        // frames whose m_bHistoryValid was cleared
};
```

The renderer keeps the last completed frame's view cameras (at most `uFLUX_MAX_VIEWS_PER_FRAME`), the two eye matrices and a per-view history-reset count in a side table written on the build node and read on the main thread between frames; `uEye` above 1 returns false. It is a public test-support header in the pattern of SCOPE's `Zenith_XrTestAccess.h`, read by tests alone; the side table is written in every configuration and costs at most 128 cameras per frame.

## View extensions

<!-- elaboration -->
The editor overlays of `VIEW_KIND_EDITOR` (gizmos, selection outline, grid, `Zenith2/Docs/Design/EDIT.md`, pending) and module-added passes (2D sprites, `Zenith2/Docs/Design/TWOD.md`, pending) reach a view's graph through one interface ZenithFlux declares, so the callback points downward ([Z2-N-ARCH-078]):

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ViewExtension.h
enum Flux_ExtensionPoint : uint8
{
	FLUX_EXTENSION_POINT_AFTER_OPAQUE = 1,       // after the opaque colour pass and sky, before SSR
	FLUX_EXTENSION_POINT_TRANSPARENT = 2,        // after TransparentForward, before Particles
	FLUX_EXTENSION_POINT_BEFORE_UPSCALE = 3,     // render extent, before TemporalAA
	FLUX_EXTENSION_POINT_OVERLAY = 4,            // output extent, after UIComposite, before DebugOverlay
	FLUX_EXTENSION_POINT_BEFORE_UI = 5,          // output extent, after TonemapAndGrade (or where it would run) and VideoComposite, before UIComposite (Stage 3)
};

/// @contract interface Flux_ViewExtension
/// Ownership: Implemented and owned by a module or the editor; registered with Flux_Renderer::RegisterViewExtension at
///   startup step 15 (module start) and unregistered before its owner is destroyed.
/// Thread affinity: ANY_THREAD_EXCEPT(AddPasses). AddPasses runs on the Flux.BuildGraph node, once per view whose kind is in
///   GetViewKindMask, in registration order.
/// Invalidation: None.
/// Units: None.
/// Errors: AddPasses declares passes through the public graph API alone; its errors are graph diagnostics.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callback; Flux_Renderer, which invokes AddPasses, declares NONE
///   (Zenith2/Docs/Design/RPROD/Renderer.md "Flux_Renderer header"), so AddPasses may not call back into the renderer.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_ViewExtension
{
public:
	virtual ~Flux_ViewExtension() = default;
	virtual Flux_ExtensionPoint GetPoint() const = 0;
	virtual uint32 GetViewKindMask() const = 0;              // 1 << Flux_ViewKind
	virtual void AddPasses(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets) = 0;
};
```

<!-- elaboration -->
At `FLUX_EXTENSION_POINT_OVERLAY` the targets an extension receives still carry the view's depth: `xTargets.m_xDepth` is the depth target of the render extent, written by the path's depth passes, which an extension may declare as an SRV read (A3 cross-area row ED12 (c)). Overlay passes run at the output extent, so an extension samples the depth at output coordinates only in a view whose dynamic-resolution scale is 1.0 (render extent equal to output extent); the editor's scene views fix that scale. `Test_ViewExtension_OverlayReadsDepth` registers an `OVERLAY` extension whose pass reads `m_xDepth` and asserts on WARP and lavapipe that the read is ordered after every depth write of the view, with synchronisation validation clean.

### Before-UI extension point

<!-- elaboration -->
`FLUX_EXTENSION_POINT_BEFORE_UI` (row TD2) is called by `Flux_BuildCommonEpilogue` as epilogue row 6c: after `TonemapAndGrade` (row 6) and `VideoComposite` (row 6b), before `UIComposite` (row 7), at the output extent, with `xTargets.m_xOutput` the display-referred output (`Zenith2/Docs/Design/RPROD/Passes.md → "Common epilogue rows 6b and 6c"`). In a view whose `FLUX_VIEW_FEATURE_TONEMAP` bit is clear it is called where tonemapping would run, so the extension can be the output's first writer (TWOD's canvas composite). `Test_ViewExtension_BeforeUIOrder` asserts both placements.

A view whose `FLUX_VIEW_FEATURE_EDITOR_OVERLAY` bit is clear calls no extension registered by the editor (the editor's extensions report `VIEW_KIND_EDITOR` alone). At most 32 extensions are registered; the 33rd registration returns `FLUX_RENDERER_ERROR_EXTENSION_LIMIT`.
