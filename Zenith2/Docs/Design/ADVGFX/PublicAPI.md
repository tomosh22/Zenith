# ADVGFX design: public API

Section file of `Zenith2/Docs/Design/ADVGFX.md`. It holds every public declaration of the area, header by header, in Zenith Hungarian (Z2-D-034), with one contract block per type in the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`. Every fallible public function returns `Zenith_Result<T, E>` (`Zenith2/Docs/Contracts.md → "Errors and results"`). The Stage 4 virtual texturing headers and the Stage 5 path-tracer headers are complete; the Stage 7 headers are the declarations the deferred trees fix now, to the depth "Stage 7 declarations" states. The physical-accuracy runner and the image metrics are test support compiled into `ZenithFlux_Tests` alone, not a library's public headers, because the metrics use the `flip` header, whose manifest row links it into that target only (`Zenith2/Docs/Design/SHIP/Manifest.md`); they keep contract blocks here so the tickets that write them and the tests that call them use fixed names.

## Header map

| Header | Library | Declares | Stage |
|---|---|---|---|
| `Zenith2/Engine/ZenithFlux/Flux_VirtualTexture.h` | ZenithFlux | the VT constants, `Flux_VirtualTextureError`, `Flux_VTResidencyMode`, `Flux_VirtualTextureDesc`, `Flux_VirtualTextureInfo`, `Flux_VirtualTextureResidency`, `Flux_VirtualTextureResource`, `Flux_VirtualTextureStats`, `Flux_VirtualTextureSystem`, the entry and request codecs, the configuration functions, the three feature functions | S4 |
| `Zenith2/Shaders/Common/VirtualTexture.slang` | ZenithShadersGenerated (the `[FluxShared]` structs) | `Flux_VTRecord`, `Flux_VTBinding`, `Flux_VTPageTableRun`, `Flux_VTPageTableMipRange`, `Flux_SampleVirtual`, `Flux_VTRecordRequest`, `Flux_VTWriteFeedback`, `Flux_VTDecodeEntry`, `Flux_VTEncodeRequest` | S4 |
| `Zenith2/Tools/ZenithPathTracer/Flux_PathTracer.h` | ZenithPathTracer | the tracer's enumerations, settings, scene builder, scene, tracer and result | S5 |
| `Zenith2/Tools/ZenithPathTracer/Flux_PathTraceReference.h` | ZenithPathTracer | the `.zref` reader and writer, the fingerprint | S5 |
| `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/Runner/Flux_PhysicalAccuracyRunner.h` | test support of `ZenithFlux_Tests` (not a public header) | the comparison settings, the image metrics and the runner LIGHT's and ENV's comparison tests call | S5 |
| `Zenith2/Engine/ZenithRHI/Zenith_RHIRayTracing.h` | ZenithRHI | the ray-tracing capability, acceleration-structure descriptions and the build records | S7 |
| `Zenith2/Engine/ZenithFlux/Flux_RayTracing.h` | ZenithFlux | `Flux_RayTracingScene`, the RT feature settings and feature functions | S7 |
| `Zenith2/Engine/ZenithFlux/Flux_VirtualGeometry.h` | ZenithFlux | `Flux_VirtualGeometryResource`, `Flux_VirtualGeometrySystem` and its feature functions | S7 |

## Flux_VirtualTexture header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_VirtualTexture.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_ConfigStore.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithFlux/Flux_TextureResource.h"      // GEOM's Flux_TextureUsage
#include "ZenithFlux/Flux_RenderGraphFwd.h"       // Flux_RenderGraph, Flux_RGBuffer (RPROD)

class Flux_ViewContext;
struct Flux_FrameShared;
class Zenith_JobContext;
struct Zenith_DevToolsAreaServices;
class Zenith_DevToolsAreaTokens;

inline constexpr uint32 uFLUX_VT_TILE_TEXELS = 128;
inline constexpr uint32 uFLUX_VT_BORDER_TEXELS = 4;
inline constexpr uint32 uFLUX_VT_PAGE_TEXELS = 136;
inline constexpr uint32 uFLUX_VT_PAGE_BYTES = 18496;
inline constexpr uint32 uFLUX_VT_LAYER_PAGES_PER_AXIS = 30;
inline constexpr uint32 uFLUX_VT_LAYER_TEXELS = 4080;
inline constexpr uint32 uFLUX_VT_PAGES_PER_LAYER = 900;
inline constexpr uint64 ulFLUX_VT_LAYER_BYTES = 16646400;
inline constexpr uint32 uFLUX_VT_MAX_LAYERS = 64;
inline constexpr uint32 uFLUX_VT_MIN_EXTENT_LOG2 = 8;
inline constexpr uint32 uFLUX_VT_MAX_EXTENT_LOG2 = 14;
inline constexpr uint32 uFLUX_VT_MAX_ASPECT_LOG2 = 3;
inline constexpr uint32 uFLUX_VT_MAX_PAGE_MIPS = 8;
inline constexpr uint32 uFLUX_VT_MAX_TAIL_MIPS = 10;
inline constexpr uint32 uFLUX_VT_MAX_TILES = 21845;
inline constexpr uint32 uFLUX_VT_MAX_REGION_SIDE = 128;
inline constexpr uint32 uFLUX_VT_MAX_ROOT_TILES = 8;
inline constexpr uint32 uFLUX_VT_MAX_TAIL_BYTES = 43792;
inline constexpr uint32 uFLUX_VT_MAX_SLOTS = 4096;
inline constexpr uint32 uFLUX_VT_RECORD_BYTES = 32;
inline constexpr uint32 uFLUX_VT_HANDLE_TAG = 0xA;
inline constexpr uint32 uFLUX_VT_FEEDBACK_NONE = 0xFFFFFFFFu;
inline constexpr uint32 uFLUX_VT_REFINE_STEP = 2;
inline constexpr uint32 uFLUX_VT_EVICT_GRACE_FRAMES = 8;
inline constexpr uint32 uFLUX_VT_FEEDBACK_RING = 4;
inline constexpr uint32 uFLUX_VT_MAX_FEEDBACK_VIEWS = 8;
inline constexpr uint32 uFLUX_VT_MAX_PAGE_TABLE_RECORDS = 65536;
inline constexpr uint32 uFLUX_VT_SLOT_QUARANTINE_FRAMES = 8;

/// @contract enum Flux_VirtualTextureError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are stable; they appear as loader codes in asset errors and telemetry.
/// @endcontract
enum Flux_VirtualTextureError : uint8
{
	FLUX_VT_ERROR_OUT_OF_MEMORY = 1,     // a host allocation failed
	FLUX_VT_ERROR_DEVICE = 2,            // a device creation call failed (the RHI error is logged)
	FLUX_VT_ERROR_BUDGET_EXCEEDED = 3,   // the configured pools exceed Z2-B-ADVGFX-001, or the host worst case the host row
	FLUX_VT_ERROR_INVALID_DESC = 4,      // a desc member outside its domain
	FLUX_VT_ERROR_ALREADY_ENABLED = 5,   // EnableVirtualTexturing called twice
	FLUX_VT_ERROR_NOT_ENABLED = 6,       // a VT load while render.vt.enabled is false
	FLUX_VT_ERROR_SLOTS_FULL = 7,        // 4,096 active VTs, or every free slot in quarantine
	FLUX_VT_ERROR_PAGE_TABLE_FULL = 8,   // no free region of the VT's side
	FLUX_VT_ERROR_POOL_FULL = 9,         // the root pages (or every page in full mode) cannot be had
	FLUX_VT_ERROR_CORRUPT = 10,          // a reader row V1–V9 failed
	FLUX_VT_ERROR_CORRUPT_TILE = 11,     // a root tile failed its check or decode (V10)
	FLUX_VT_ERROR_FORMAT_UNSUPPORTED = 12, // the cooked format is not the platform's
	FLUX_VT_ERROR_UPLOAD_FAILED = 13,    // an activation upload reported FAILED
	FLUX_VT_ERROR_DEVICE_LOST = 14,
	FLUX_VT_ERROR_SHUTTING_DOWN = 15,
	FLUX_VT_ERROR_HOST_BYTES_FULL = 16,  // activating the VT would pass render.vt.maxIndexBytes
	FLUX_VT_ERROR_TAIL_BYTES_FULL = 17,  // activating the VT would pass render.vt.maxTailBytes
};

/// @contract enum Flux_VTResidencyMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Flux_VTResidencyMode : uint8
{
	FLUX_VT_RESIDENCY_MODE_STREAM = 1,   // feedback-driven (every Player and editor run)
	FLUX_VT_RESIDENCY_MODE_FULL = 2,     // every tile resident after activation; tests, parity and references
};

/// @contract value Flux_VirtualTextureDesc
/// Ownership: Plain value read from configuration by Flux_ReadVirtualTextureDesc.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Layers, texels, tiles, frames.
/// Errors: EnableVirtualTexturing returns FLUX_VT_ERROR_INVALID_DESC for a member outside its domain (a tiles-per-frame
///   value above half of Z2-B-ADVGFX-002 included) and FLUX_VT_ERROR_BUDGET_EXCEEDED when the pools exceed
///   Z2-B-ADVGFX-001 or the host worst case exceeds the proposed host row.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_VirtualTextureDesc
{
	bool m_bEnabled = true;                     // render.vt.enabled
	uint32 m_uPhysicalLayers = 12;             // render.vt.physicalLayers, 1 … 64 (4 on Android)
	uint32 m_uPageTableExtent = 2048;          // render.vt.pageTableExtent, 512 | 1024 | 2048 | 4096 (1024 on Android)
	uint32 m_uFeedbackScale = 8;               // render.vt.feedbackScale, 8 | 16 (16 on Android)
	uint32 m_uTilesPerFrame = 32;              // render.vt.tilesPerFrame, 1 … 512 (12 on Android); at most half of Z2-B-ADVGFX-002
	uint32 m_uMaxReadsInFlight = 128;          // render.vt.maxReadsInFlight, 8 … 1024 (64 on Android)
	uint32 m_uMaxAnisotropy = 8;               // render.vt.maxAnisotropy, 1 | 2 | 4 | 8 (4 on Android)
	float m_fFeedbackMipBias = 0.0f;            // render.vt.mipBias, [-2, 4]
	uint32 m_uCutBoostFrames = 30;             // render.vt.cutBoostFrames, 0 … 240
	Flux_VTResidencyMode m_eResidencyMode = FLUX_VT_RESIDENCY_MODE_STREAM;   // render.vt.residencyMode
	uint64 m_ulMaxIndexBytes = 33554432;       // render.vt.maxIndexBytes (8 MiB on Android): tile indices and mirrors
	uint64 m_ulMaxTailBytes = 8388608;         // render.vt.maxTailBytes (2 MiB on Android): tail textures, inside Z2-B-ADVGFX-001
	uint32 m_uMaxFeedbackWidth = 3840;         // render.vt.maxFeedbackWidth (2340 on Android): sizes the sort arena
	uint32 m_uMaxFeedbackHeight = 2160;        // render.vt.maxFeedbackHeight (1080 on Android)
	uint64 m_ulBudgetBytes = 0;                // Z2-B-ADVGFX-001 (device bytes) for the build platform (the proposed value while pending)
	uint64 m_ulHostBudgetBytes = 0;            // the proposed host row "Virtual texturing host memory" for the build platform
	uint32 m_uTileUploadRowValue = 0;          // Z2-B-ADVGFX-002 for the build platform: the camera-cut boost's limit
};

/// @contract value Flux_VirtualTextureInfo
/// Ownership: Plain value copied out of a resource.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None; a snapshot of fields fixed at load.
/// Units: Texels; mips; tiles.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_VirtualTextureInfo
{
	Flux_TextureUsage m_eUsage = FLUX_TEXTURE_USAGE_COLOUR;
	Zenith_RHIFormat m_eFormat = ZENITH_RHI_FORMAT_UNDEFINED;
	uint32 m_uWidthLog2 = 0;
	uint32 m_uHeightLog2 = 0;
	uint32 m_uPageMips = 0;
	uint32 m_uTailMips = 0;
	uint32 m_uTileCount = 0;
	bool m_bSRGB = false;
	bool m_bWrap = false;
};

/// @contract value Flux_VirtualTextureResidency
/// Ownership: Plain value copied out of a resource.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None; a snapshot of the build node's state at the last BeginFrame.
/// Units: Pages; tiles.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_VirtualTextureResidency
{
	uint32 m_uResidentPages = 0;      // including the pinned roots
	uint32 m_uPinnedPages = 0;
	uint32 m_uPagesInFlight = 0;      // READING, DECODING, DECODED or UPLOADING
	uint32 m_uCorruptPages = 0;
	bool m_bResident = false;          // activation complete: GetMaterialHandle returns the VT handle
	bool m_bFailed = false;
};

/// @contract type Flux_VirtualTextureResource
/// Ownership: Created by Flux_VirtualTextureKindLoader when the asset system loads a .zvt; shared through
///   Zenith_AssetHandle<Flux_VirtualTextureResource>; owns its tile index, its CPU mirror, its slot, region and pages
///   through Flux_VirtualTextureSystem, which releases them at Unload.
/// Thread affinity: ANY_THREAD for every member; values change on the Flux.BuildGraph node.
/// Invalidation: GetMaterialHandle changes at activation, after a failure, at unload and after device-loss recovery;
///   GEOM's material manager re-reads it on each of those events.
/// Units: Texels; pages.
/// Errors: load failures through GetResidency().m_bFailed; nothing asserts on file data.
/// Version: since Z2 Stage 4.
/// @endcontract
class Flux_VirtualTextureResource
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_VIRTUAL_TEXTURE;
	Flux_VirtualTextureInfo GetInfo() const;
	Flux_VirtualTextureResidency GetResidency() const;
	uint32 GetSlot() const;                              // 0xFFFFFFFF until activation
	Zenith_RHIDescriptorHandle GetMaterialHandle() const; // the VT handle when resident; {0, 0} otherwise
	Zenith_Colour GetAverageColour() const;               // linear, from the info record
};

/// @contract value Flux_VirtualTextureStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None; a snapshot taken at EndFrame.
/// Units: Counts; bytes; frames.
/// Errors: None.
/// Version: since Z2 Stage 4. Every counter of Zenith2/Docs/Design/ADVGFX.md "Diagnostics and profiling" in value form.
/// @endcontract
struct Flux_VirtualTextureStats
{
	uint32 m_uActiveTextures = 0;
	uint32 m_uPhysicalPages = 0;
	uint32 m_uResidentPages = 0;
	uint32 m_uFreePages = 0;
	uint32 m_uDrainingPages = 0;
	uint32 m_uRequestsThisFrame = 0;          // unique valid requests processed
	uint32 m_uFeedbackRejected = 0;
	uint32 m_uReadsInFlight = 0;
	uint32 m_uTilesUploaded = 0;
	uint32 m_uEvictions = 0;
	uint32 m_uEvictionStarved = 0;
	uint32 m_uPageTableRecords = 0;
	uint32 m_uReadFailures = 0;
	uint32 m_uCorruptTiles = 0;
	uint32 m_uFeedbackSkipped = 0;
	uint64 m_ulDeviceBytes = 0;
	uint64 m_ulHostBytes = 0;
	uint32 m_uUploadStarvedFrames = 0;
	uint32 m_uFirstHopFramesP95 = 0;          // over the last 600 completed hops
	uint32 m_uFinestMipFramesP95 = 0;         // over the last 600 mip-0 pages reached from the root
};

/// @contract type Flux_VirtualTextureSystem
/// Ownership: Created by Flux_Renderer::EnableVirtualTexturing as a Zenith_Owned member of the renderer (rows AV1 (a),
///   AV2) on the main thread at startup step 13, destroyed in the renderer's shutdown before GEOM's managers; owns the
///   physical cache, the page-table pool, the record buffer, the staging pool, the region allocator, the kind loader
///   and every slot.
/// Thread affinity: RENDER_SUBMIT_THREAD for BeginFrame, ImportResources, WriteFrameData, EndFrame and the feature
///   functions (the Flux.BuildGraph node); ANY_THREAD for GetStats, IsSettled and IsEnabled; MAIN_THREAD for
///   SetPressureBias (the rendering memory consumer's notification in the frame prologue).
/// Invalidation: None across frames.
/// Units: Pages; tiles; bytes; frames.
/// Errors: per-texture failures move the texture to the usage fallback and count Flux.VT.* counters;
///   OnDeviceRecovered returns FLUX_VT_ERROR_DEVICE or FLUX_VT_ERROR_OUT_OF_MEMORY when re-creating a pool fails;
///   no other member returns an error after creation.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Flux_VirtualTextureSystem
{
public:
	bool IsEnabled() const;
	void BeginFrame(Zenith_JobContext& xContext);                           // renderer step 3c
	void ImportResources(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared);   // step 7
	void WriteFrameData();                                                   // step 11, after FITS or EXTENDED
	void EndFrame(bool bSubmitted);                                          // step 13, every path
	void SetPressureBias(uint32 uBias);                                     // 0, 1 (WARNING), 2 (CRITICAL)
	void OnCameraCut();                                                      // from the view list, step 5
	void OnDeviceLost();                                                     // recovery step 3
	Zenith_Result<void, Flux_VirtualTextureError> OnDeviceRecovered(Zenith_RHIDevice& xDevice);   // recovery step 4
	bool IsSettled() const;                                    // no tile in flight
	Flux_VirtualTextureStats GetStats() const;
	const Flux_VirtualTextureDesc& GetDesc() const;
};

// Codecs (CPU halves of VirtualTexture.slang)
constexpr uint32 Flux_EncodeVTEntry(uint32 uPhysicalPage, uint32 uResidentMip);
constexpr uint32 Flux_DecodeVTEntryPage(uint32 uEntry);
constexpr uint32 Flux_DecodeVTEntryMip(uint32 uEntry);
constexpr uint32 Flux_EncodeVTRequest(uint32 uSlot, uint32 uMip, uint32 uPageX, uint32 uPageY);
struct Flux_VTRequest { uint32 m_uSlot = 0; uint32 m_uMip = 0; uint32 m_uPageX = 0; uint32 m_uPageY = 0; };
constexpr Flux_VTRequest Flux_DecodeVTRequest(uint32 uRequest);
constexpr Zenith_RHIDescriptorHandle Flux_MakeVTMaterialHandle(uint32 uSlot, uint32 uTailSRV);   // {uTailSRV, 0xA << 28 | uSlot}
constexpr bool Flux_IsVTMaterialHandle(Zenith_RHIDescriptorHandle xHandle);

// Configuration
Zenith_Result<void, Zenith_ConfigError> Flux_RegisterVirtualTextureConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform);                                         // called by Zenith_RegisterFluxConfigKeys
Flux_VirtualTextureDesc Flux_ReadVirtualTextureDesc(const Zenith_ConfigStore& xConfig);

// Feature functions (RPROD's path functions call them, rows AV1 (a), AV1 (f))
void Flux_AddVTPageTableUpdate(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared, Flux_VirtualTextureSystem& xVT);
Flux_RGBuffer Flux_AddVTFeedbackClear(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView,
	Flux_VirtualTextureSystem& xVT);                                         // null handle: the view writes no feedback
void Flux_AddVTFeedbackReadback(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGBuffer xFeedback,
	Flux_VirtualTextureSystem& xVT);

// DEVT area registration (Zenith2/Docs/Design/DEVT.md "Upward interfaces DEVT declares")
void Flux_RegisterVirtualTextureDevTools(const Zenith_DevToolsAreaServices& xServices, void* pContext,
	Zenith_DevToolsAreaTokens& xTokensOut);
```

The renderer members ADVGFX writes the bodies of, declared by RPROD (ruling ENABLE-FEATURES; row AV1 (a)):

<!-- assumption: pending RPROD -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h (RPROD's header; the two members RPROD-014's revision declares)
Zenith_Result<Flux_VirtualTextureSystem*, Flux_VirtualTextureError> EnableVirtualTexturing(
	const Flux_VirtualTextureDesc& xDesc);          // main thread, startup step 13, once; always called (rows AV1 (a), AV2)
Flux_VirtualTextureSystem* GetVirtualTextureSystem();   // null before EnableVirtualTexturing
```
<!-- /assumption -->

`EnableVirtualTexturing` validates the desc, computes the GPU total, creates the physical cache, the page-table pool, the record buffer and the staging pool in that order (each step a failure-injection point `0x41560001` … `0x41560004`, 'AV', with a `Test_VirtualTextureSystem_Create_FailsAt<Step>_Unwinds` case per step, named `…FailsAtPhysicalCache…`, `…FailsAtPageTable…`, `…FailsAtRecordBuffer…`, `…FailsAtStagingPool…`, [Z2-N-CONTRACT-341]), registers `Flux_VirtualTextureKindLoader` with the asset registry and returns the borrowed system; with `m_bEnabled` false it creates nothing on the device, registers the loader in refusing mode, and returns a system whose `IsEnabled()` is false.

## VirtualTexture shader module

```slang
// Zenith2/Shaders/Common/VirtualTexture.slang
module VirtualTexture;                       // imported by Sampling.slang and Material.slang; imports neither
[FluxShared] struct Flux_VTRecord { uint m_uRegionOrigin; uint m_uShape; uint m_uSlot; float m_fMipBias;
	uint m_uReserved0; uint m_uReserved1; uint m_uReserved2; uint m_uReserved3; };               // 32 bytes
[FluxShared] struct Flux_VTPageTableRun { uint m_uX; uint m_uYMip; uint m_uLength; uint m_uValue; };  // 16 bytes
[FluxShared] struct Flux_VTPageTableMipRange { uint m_uFirstRecord; uint m_uRecordCount; uint m_uFirstEntry; uint m_uEntryCount; };
struct Flux_VTBinding;                       // "Material input binding" of VirtualTexturing.md

struct Flux_VTEntry { uint m_uPhysicalX; uint m_uPhysicalY; uint m_uLayer; uint m_uResidentMip; };
Flux_VTEntry Flux_VTDecodeEntry(uint uEntry);
uint Flux_VTEncodeRequest(uint uSlot, uint uMip, uint uPageX, uint uPageY);
// invocation-private state, visible inside this module alone
internal static Flux_VTBinding s_xBinding;   // the invocation's binding, stored by Flux_VTBeginInvocation
internal static bool s_bWriting;             // this invocation is its block's writer this frame
internal static uint s_uCount;               // the request reservoir: requests seen …
internal static uint s_uChosen;              // … and the chosen one (0xFFFFFFFF: none)

// the public functions, the only way in
public void Flux_VTBeginInvocation(Flux_VTBinding xBinding, uint2 xPixel);   // EvaluateMaterial calls it; stores the binding
                                                              // and marks the invocation writing or non-writing
public bool Flux_VTPoolsBound();                              // bit 1 of the stored flags
public float4 Flux_SampleVirtual(uint2 xHandleWords, float2 xUV, Flux_UVGradients xGrad);   // samples through the stored binding
public void Flux_VTRecordRequest(uint uRequest);              // updates the reservoir; returns at once when not writing
public void Flux_VTWriteFeedback(RWByteAddressBuffer xFeedback, uint uStride, uint2 xPixel);   // pass epilogue
```

The `static` declarations are the invocation's private globals (thread-local in every stage); `internal` keeps them out of every other module, so `Sampling.slang`, `Material.slang`, a material module and a pass can reach them only through the public functions (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material input binding"`). Every function is free of derivative instructions (the gradients arrive in `Flux_UVGradients`), so pixel and compute stages share it; the VT branch of `Flux_SampleMaterial` is the only caller of `Flux_SampleVirtual` (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Flux_SampleVirtual algorithm"`).

## Flux_PathTracer header

```cpp
// Zenith2/Tools/ZenithPathTracer/Flux_PathTracer.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Maths.h"
#include "ZenithRHI/Zenith_RHIDevice.h"
#include "ZenithFlux/Flux_View.h"
#include "ZenithFlux/Flux_Lights.h"

class Flux_Renderer;
class Flux_MeshResource;
class Flux_MaterialResource;
class Flux_EnvironmentMapResource;
class Zenith_Heap;

/// @contract enum Flux_PathTraceIntegrator
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5. Written into .zref headers; values never change.
/// @endcontract
enum Flux_PathTraceIntegrator : uint8
{
	FLUX_PT_INTEGRATOR_RADIANCE = 1,             // path tracing with next-event estimation and MIS; RGB
	FLUX_PT_INTEGRATOR_AMBIENT_VISIBILITY = 2,   // cosine-weighted visibility within a radius; one channel
	FLUX_PT_INTEGRATOR_LIGHT_VISIBILITY = 3,     // visibility of one light from the primary hit; one channel
	FLUX_PT_INTEGRATOR_SPECULAR_REFLECTION = 4,  // the specular lobe's reflected radiance at the primary hit; RGB
};

/// @contract enum Flux_PathTraceTraversal
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5. One value: the compute traversal is the whole design (Z2-D-037).
/// @endcontract
enum Flux_PathTraceTraversal : uint8
{
	FLUX_PT_TRAVERSAL_COMPUTE_BVH = 1,           // the software BVH, every device (Z2-D-037)
};

/// @contract enum Flux_PathTraceOutcome
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_PathTraceOutcome : uint8
{
	FLUX_PT_OUTCOME_CONVERGED = 1,       // the convergence criterion held after at least the minimum samples
	FLUX_PT_OUTCOME_NOT_CONVERGED = 2,   // the wall-clock cap or the maximum samples came first; a comparison fails ([Z2-N-REND-946])
	FLUX_PT_OUTCOME_CANCELLED = 3,
};

/// @contract enum Flux_PathTraceError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_PathTraceError : uint8
{
	FLUX_PT_ERROR_OUT_OF_MEMORY = 1,
	FLUX_PT_ERROR_INVALID_SETTINGS = 2,        // a settings member outside its domain
	FLUX_PT_ERROR_SCENE_INVALID = 3,           // a builder input failed validation (the log names it)
	FLUX_PT_ERROR_BVH_LIMIT = 4,               // more than 16,777,216 triangles, or depth above 64
	FLUX_PT_ERROR_MATERIAL_UNSUPPORTED = 5,    // a material variant with no compiled tracer resolve program
	FLUX_PT_ERROR_DEVICE = 6,                  // a device call failed
	FLUX_PT_ERROR_DEVICE_LOST = 7,
	FLUX_PT_ERROR_REFERENCE_CORRUPT = 8,       // a .zref failed its reader
	FLUX_PT_ERROR_REFERENCE_STALE = 9,         // a .zref fingerprint differs from the recomputed one
	FLUX_PT_ERROR_IO = 10,
	FLUX_PT_ERROR_TOOL_GRAPH = 11,             // Flux_Renderer::RunToolGraph refused or failed (row AV17); the log names its error
	FLUX_PT_ERROR_VT_NOT_SETTLED = 12,         // Flux_Renderer::SettleToolFrames returned FLUX_TOOL_GRAPH_ERROR_VT_NOT_SETTLED
};

/// @contract value Flux_PathTraceSettings
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Pixels; samples; milliseconds; metres (m_fAmbientRadiusMetres).
/// Errors: Flux_PathTracer::Run returns FLUX_PT_ERROR_INVALID_SETTINGS for any member outside its domain.
/// Version: since Z2 Stage 5. Every member is an input of the reference fingerprint.
/// @endcontract
struct Flux_PathTraceSettings
{
	Flux_PathTraceIntegrator m_eIntegrator = FLUX_PT_INTEGRATOR_RADIANCE;
	Flux_PathTraceTraversal m_eTraversal = FLUX_PT_TRAVERSAL_COMPUTE_BVH;
	uint32 m_uWidth = 256;                     // ≤ Z2-B-ADVGFX-004 for the scene; 16 … 1,024
	uint32 m_uHeight = 256;
	uint32 m_uMinSamplesPerPixel = 256;        // Z2-B-ADVGFX-006; a multiple of m_uBatchSamples
	uint32 m_uMaxSamplesPerPixel = 65536;
	uint32 m_uBatchSamples = 64;               // samples between convergence checks; 16 … 256
	float m_fRelativeStandardError = 0.01f;     // Z2-B-ADVGFX-007: SE of the luminance mean below this fraction of the mean…
	float m_fAbsoluteStandardErrorFraction = 1.0e-4f;   // …or below this fraction of the 99th-percentile luminance
	float m_fConvergedPixelFraction = 0.995f;   // …for at least this fraction of pixels
	uint32 m_uWallClockCapMs = 600000;         // Z2-B-ADVGFX-005
	uint32 m_uMaxBounces = 8;                  // RADIANCE: 1 = direct lighting and environment only
	uint32 m_uRussianRouletteBounce = 3;       // first bounce at which roulette may end a path
	uint64 m_ulSeed = 0x5A32505452454631ull;   // 'Z2PTREF1'
	float m_fAmbientRadiusMetres = 1.0f;        // AMBIENT_VISIBILITY: LIGHT's GTAO radius of the scene
	uint32 m_uLightIndex = 0;                  // LIGHT_VISIBILITY: the scene light whose visibility is traced
};

/// @contract value Flux_PathTraceTriangleInput
/// Ownership: Plain value; the spans are borrowed for the duration of AddTriangles, which copies them.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Metres (world space); unit vectors.
/// Errors: AddTriangles returns FLUX_PT_ERROR_SCENE_INVALID for a non-finite value, an index out of range or
///   mismatched span lengths.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_PathTraceTriangleInput
{
	Zenith_Span<const Zenith_Vector3> m_axPositionsWS;
	Zenith_Span<const Zenith_Vector3> m_axNormalsWS;
	Zenith_Span<const Zenith_Vector4> m_axTangentsWS;      // xyz tangent, w the bitangent sign
	Zenith_Span<const Zenith_Vector2> m_axUV0;
	Zenith_Span<const Zenith_Vector2> m_axUV1;             // may be empty
	Zenith_Span<const uint32> m_auIndices;                // 3 per triangle
	const Flux_MaterialResource* m_pxMaterial = nullptr;
	uint32 m_uInstanceCustomData = 0;                     // Flux_MaterialInput::m_uInstanceIndex's custom data
};

/// @contract type Flux_PathTraceSceneBuilder
/// Ownership: Owned by the caller; produces a Flux_PathTraceScene through Build and is single-use.
/// Thread affinity: OWNER_THREAD (the caller's).
/// Invalidation: Every Add call after Build returns FLUX_PT_ERROR_SCENE_INVALID.
/// Units: Metres; the light units of Zenith2/Docs/Design/LIGHT/Lighting.md "Light units".
/// Errors: each Add validates its input; Build returns the BVH and memory errors.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 5.
/// @endcontract
class Flux_PathTraceSceneBuilder
{
public:
	explicit Flux_PathTraceSceneBuilder(Zenith_Heap& xHeap);
	Zenith_Result<void, Flux_PathTraceError> AddMeshInstance(const Flux_MeshResource& xMesh, uint32 uLOD,
		const Zenith_Matrix4& xLocalToWorld, Zenith_Span<const Flux_MaterialResource* const> axSectionMaterials,
		uint32 uCustomData);           // decodes the cooked streams through ZenithVertexFormat
	Zenith_Result<void, Flux_PathTraceError> AddTriangles(const Flux_PathTraceTriangleInput& xInput);   // terrain and generated geometry
	Zenith_Result<void, Flux_PathTraceError> AddLight(const Flux_RenderLightDelta& xLight);
	Zenith_Result<void, Flux_PathTraceError> SetEnvironment(const Flux_EnvironmentMapResource* pxEnvironment,
		float fIntensity);              // null: black environment
	Zenith_Result<void, Flux_PathTraceError> SetUniformEnvironment(const Zenith_Vector3& xRadiance);   // furnace scenes
	void SetCamera(const Flux_ViewCamera& xCamera);   // the unjittered projection is used
	Zenith_Result<Zenith_Owned<Flux_PathTraceScene>, Flux_PathTraceError> Build();
};

/// @contract type Flux_PathTraceScene
/// Ownership: Created by Flux_PathTraceSceneBuilder::Build as a Zenith_Owned; owns the CPU BVH, the triangle and
///   attribute arrays, the light list and the environment alias table; uploads them on first Run.
/// Thread affinity: IMMUTABLE after Build; Run reads it.
///   Private members non-const: the first Run records the uploaded GPU copies.
/// Invalidation: None.
/// Units: Metres.
/// Errors: None after Build.
/// Version: since Z2 Stage 5.
/// @endcontract
class Flux_PathTraceScene
{
public:
	uint32 GetTriangleCount() const;
	uint32 GetNodeCount() const;
	uint32 GetLightCount() const;
	uint32 GetMaterialVariantCount() const;
	Zenith_Span<const uint64> GetMaterialVariantIDs() const;   // ascending; fingerprint input
};

/// @contract value Flux_PathTraceResult
/// Ownership: Owned by the caller; its arrays are allocated from the heap the tracer was created with.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Radiance in the scene's linear units (not pre-exposed); visibility in [0, 1]; milliseconds.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_PathTraceResult
{
	Flux_PathTraceOutcome m_eOutcome = FLUX_PT_OUTCOME_NOT_CONVERGED;
	uint32 m_uWidth = 0;
	uint32 m_uHeight = 0;
	uint32 m_uChannels = 0;                    // 3 (RADIANCE, SPECULAR_REFLECTION) or 1
	uint32 m_uSamplesPerPixel = 0;
	uint32 m_uWallClockMs = 0;
	float m_fConvergedPixelFraction = 0.0f;
	Zenith_Array<float> m_afMean;               // width × height × channels, row-major from the top
	Zenith_Array<float> m_afStandardError;      // width × height: standard error of the luminance (or scalar) mean
};

/// @contract value Flux_PathTracerDesc
/// Ownership: Plain value; the pointers are borrowed for the tracer's lifetime.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: Flux_PathTracer::Create returns FLUX_PT_ERROR_INVALID_SETTINGS for a null pointer.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_PathTracerDesc
{
	Zenith_RHIDevice* m_pxDevice = nullptr;
	Flux_Renderer* m_pxRenderer = nullptr;      // its shader library, PSO table, tool-graph entry, material slots and VT system
	Zenith_Heap* m_pxHeap = nullptr;
};

/// @contract type Flux_PathTracer
/// Ownership: Created by Create as a Zenith_Owned; owns its programs' PSOs, the path, hit and accumulation buffers
///   and their device memory; released in its destructor after the device's queues completed its submissions.
/// Thread affinity: MAIN_THREAD. Create, Run and the destructor run on the thread that drives the renderer's frames,
///   between render frames; each batch runs as one Flux_Renderer::RunToolGraph call, which executes it on the
///   render-submit role (RPROD's "Tool graph entry", row AV17). Cancel is ANY_THREAD.
/// Invalidation: None.
/// Units: Samples; milliseconds.
/// Errors: Run returns Flux_PathTraceError; NOT_CONVERGED and CANCELLED are outcomes of a successful Run.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 5. Tool-side: never linked by a Player or runtime target ([Z2-N-REND-948]).
/// @endcontract
class Flux_PathTracer
{
public:
	static Zenith_Result<Zenith_Owned<Flux_PathTracer>, Flux_PathTraceError> Create(const Flux_PathTracerDesc& xDesc);
	~Flux_PathTracer();
	Zenith_Result<Flux_PathTraceResult, Flux_PathTraceError> Run(const Flux_PathTraceScene& xScene,
		const Flux_PathTraceSettings& xSettings);
	void Cancel();                                   // the current Run returns CANCELLED at its next batch boundary
};
```

## Flux_PathTraceReference header

```cpp
// Zenith2/Tools/ZenithPathTracer/Flux_PathTraceReference.h
#pragma once
#include "ZenithPathTracer/Flux_PathTracer.h"
#include "ZenithCore/Zenith_Hash.h"          // Zenith_Hash128
#include "ZenithCore/Zenith_Sha256.h"
#include "ZenithCore/Zenith_Span.h"          // Zenith_ByteSpan, Zenith_Span
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCompression/Zenith_CompressionCodecSet.h"

inline constexpr uint32 uFLUX_PT_VERSION = 1;               // bumped by any change to the tracer's arithmetic; a fingerprint input
inline constexpr uint16 uFLUX_PT_REFERENCE_FORMAT_VERSION = 1;

/// @contract value Flux_PathTraceFingerprintInputs
/// Ownership: Plain value; spans borrowed for the call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5. "Reference manifest and fingerprint" of PathTracer.md fixes the byte stream.
/// @endcontract
struct Flux_PathTraceFingerprintInputs
{
	Zenith_ByteSpan m_xSceneDocument;                         // the committed scene JSON bytes
	Zenith_Span<const Zenith_Hash128> m_axContentHashes;   // every referenced asset's catalogue content hash, sorted by GUID
	Zenith_Span<const uint64> m_aulProgramBlobHashes;        // tracer programs and resolve variants, ascending
	Flux_PathTraceSettings m_xSettings;
	Zenith_RHIBackend m_eBackend = ZENITH_RHI_BACKEND_VULKAN;
	Zenith_StringView m_strSoftwareAdapterVersion;            // the pinned WARP or lavapipe version string
};

/// @contract value Flux_PathTraceReference
/// Ownership: Owns its arrays (heap of the reader call).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: As Flux_PathTraceResult.
/// Errors: None after a successful read.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_PathTraceReference
{
	Flux_PathTraceResult m_xResult;
	Zenith_Sha256Value m_xFingerprint;
	Zenith_RHIBackend m_eBackend = ZENITH_RHI_BACKEND_VULKAN;
	char m_acDevice[32] = {};
	uint64 m_ulDriverVersion = 0;
	uint32 m_uTracerVersion = 0;
};

Zenith_Sha256Value Flux_ComputeReferenceFingerprint(const Flux_PathTraceFingerprintInputs& xInputs);
Zenith_Result<void, Flux_PathTraceError> Flux_WritePathTraceReference(const Flux_PathTraceReference& xReference,
	const Zenith_CompressionCodecSet& xCodecs, Zenith_Array<uint8>& auOut);
Zenith_Result<Flux_PathTraceReference, Flux_PathTraceError> Flux_ReadPathTraceReference(Zenith_ByteSpan xBytes,
	const Zenith_CompressionCodecSet& xCodecs, Zenith_Heap& xHeap);
```

## Flux_PhysicalAccuracyRunner header

```cpp
// Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/Runner/Flux_PhysicalAccuracyRunner.h (test support of ZenithFlux_Tests)
#pragma once
#include "ZenithPathTracer/Flux_PathTraceReference.h"

/// @contract value Flux_CompareSettings
/// Ownership: Plain value; the mask span is borrowed for the call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Pixels per degree; exposure multipliers.
/// Errors: Flux_CompareImages returns FLUX_PT_ERROR_INVALID_SETTINGS for a size mismatch.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_CompareSettings
{
	float m_fExposure = 1.0f;                  // the scene's fixed exposure, applied by the comparison tonemap
	float m_fPixelsPerDegree = 67.0f;          // RPROD's parity value
	Zenith_Span<const uint8> m_auMask;        // empty: every pixel; else one byte per pixel, non-zero = compared
	float m_fRelativeLuminanceFloor = 1.0e-3f; // pixels darker than this fraction of the reference maximum are excluded from the RLE sum
};

/// @contract value Flux_ImageMetrics
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: FLIP in [0, 1]; ratios; counts.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ImageMetrics
{
	float m_fFlipMean = 0.0f;                  // LDR-FLIP mean over the mask
	float m_fRelativeLuminanceError = 0.0f;    // Σ|Y − Y_ref| / Σ Y_ref over the mask
	float m_fMeanAbsoluteError = 0.0f;         // one-channel images: mean |x − x_ref|
	float m_fMisclassifiedFraction = 0.0f;     // one-channel images: fraction with (x > 0.5) ≠ (x_ref > 0.5)
	float m_fZAbove4Fraction = 0.0f;           // trace against reference: fraction of pixels with z > 4
	uint32 m_uComparedPixels = 0;
};

Zenith_Result<Flux_ImageMetrics, Flux_PathTraceError> Flux_CompareImages(Zenith_Span<const float> afImage,
	Zenith_Span<const float> afReference, uint32 uWidth, uint32 uHeight, uint32 uChannels,
	const Flux_CompareSettings& xSettings);
Zenith_Result<Flux_ImageMetrics, Flux_PathTraceError> Flux_CompareTraceToReference(const Flux_PathTraceResult& xTrace,
	const Flux_PathTraceReference& xReference, const Flux_CompareSettings& xSettings);   // fills m_fZAbove4Fraction;
	// a reference larger than the trace is 2 × 2 box-downsampled first, with SE_down = sqrt(Σ SE²) / 4


/// @contract enum Flux_PhysicalAccuracyMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_PhysicalAccuracyMode : uint8
{
	FLUX_PA_MODE_GATE = 1,         // compare against the committed reference; trace the smoke budget
	FLUX_PA_MODE_ACCEPTANCE = 2,   // trace to convergence in-run, then compare (Z2-B-ADVGFX-004 … -008); fails while any
	                               // References.json entry carries pendingRecording
};
// Recording is not a runner mode: Zenith_Bench_PathTraceReference calls Flux_PathTracer and Flux_WritePathTraceReference directly.

/// @contract value Flux_PhysicalAccuracyScene
/// Ownership: Plain value naming a committed scene; strings are string literals.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: As the scene document.
/// Errors: RunScene returns FLUX_PT_ERROR_SCENE_INVALID for an unknown scene or a missing pathTrace object.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_PhysicalAccuracyScene
{
	const char* m_szScenePath = nullptr;       // repo-relative path of the *.scene.json
	Flux_ShadingPath m_eRasterPath = FLUX_SHADING_PATH_FORWARD_PLUS;
};

/// @contract value Flux_PhysicalAccuracyOutcome
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: As Flux_ImageMetrics.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_PhysicalAccuracyOutcome
{
	bool m_bPassed = false;
	Flux_PathTraceOutcome m_eTraceOutcome = FLUX_PT_OUTCOME_NOT_CONVERGED;
	Flux_ImageMetrics m_xRasterMetrics;        // raster against the reference
	Flux_ImageMetrics m_xTraceMetrics;         // the smoke or acceptance trace against the committed reference
	float m_fFlipTolerance = 0.0f;             // Z2-B-ADVGFX-008 for the scene (the proposed value while pending)
	float m_fRelativeLuminanceTolerance = 0.0f; // the second value of the scene's Z2-B-ADVGFX-008 row
	bool m_bSkippedPendingRecording = false;   // gate mode: the reference is marked pendingRecording
};

/// @contract type Flux_PhysicalAccuracyRunner
/// Ownership: Owned by the calling test; borrows the renderer and the tracer for its lifetime.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: RunScene returns Flux_PathTraceError for a setup failure; a metric outside its tolerance is
///   m_bPassed false with every artefact written ("Reports and artefacts" of PathTracer.md); a reference marked
///   pendingRecording is a skip with its reason in gate mode and a failure in acceptance mode.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 5.
/// @endcontract
class Flux_PhysicalAccuracyRunner
{
public:
	Flux_PhysicalAccuracyRunner(Flux_Renderer& xRenderer, Flux_PathTracer& xTracer, Flux_PhysicalAccuracyMode eMode);
	Zenith_Result<Flux_PhysicalAccuracyOutcome, Flux_PathTraceError> RunScene(const Flux_PhysicalAccuracyScene& xScene);
};
```

## Stage 7 declarations

The deferred trees fix their API at this depth now, so their Stage 7 tickets are written against names that do not move; members marked `// detail at Stage 7` are completed by the Stage 7 API tickets under the open decisions of `Zenith2/Docs/Design/ADVGFX.md → "Decisions"`. Every type carries its contract block now; a field of a block that reads "shape fixed by the Stage 7 API ticket" is completed by that ticket and by no other.

### Zenith_RHIRayTracing header

<!-- assumption: pending RHI -->
```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIRayTracing.h (Stage 7; RHI accepts the header into its family, row AV4 (d))
/// @contract value Zenith_RHIRayTracingCapabilities
/// Ownership: Plain value inside Zenith_RHICapabilities.
/// Thread affinity: IMMUTABLE after device creation.
///   Members non-const: device creation fills it field by field.
/// Invalidation: None; re-read after device-loss recovery.
/// Units: Primitives; instances; bytes (alignments).
/// Errors: None.
/// Version: since Z2 Stage 7; shape fixed by the Stage 7 API ticket.
/// @endcontract
struct Zenith_RHIRayTracingCapabilities          // appended to Zenith_RHICapabilities as m_xRayTracing
{
	bool m_bRayQuery = false;                    // VK_KHR_acceleration_structure + VK_KHR_ray_query, or D3D12 raytracing tier 1.1
	bool m_bRayTracingPipelines = false;         // reported, never used (D-RT-SCOPE (open, Stage 7))
	uint64 m_ulMaxGeometryPrimitives = 0;
	uint32 m_uMaxInstances = 0;
	uint32 m_uScratchAlignment = 0;
	uint32 m_uAccelerationStructureAlignment = 256;
};
/// @contract value Zenith_RHIAccelStructTag
/// Ownership: Empty tag type of Zenith_RHIAccelerationStructureHandle (a device handle, Contracts.md "Handle kinds").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7.
/// @endcontract
struct Zenith_RHIAccelStructTag {};
using Zenith_RHIAccelerationStructureHandle = Zenith_DeviceHandle<Zenith_RHIAccelStructTag>;
/// @contract enum Zenith_RHIAccelerationStructureKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7.
/// @endcontract
enum Zenith_RHIAccelerationStructureKind : uint8 { ZENITH_RHI_ACCEL_KIND_BOTTOM = 1, ZENITH_RHI_ACCEL_KIND_TOP = 2 };
/// @contract enum Zenith_RHIAccelerationBuildFlags
/// Ownership: Plain enumeration (bit flags).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7.
/// @endcontract
enum Zenith_RHIAccelerationBuildFlags : uint8 { ZENITH_RHI_ACCEL_BUILD_FAST_TRACE = 1, ZENITH_RHI_ACCEL_BUILD_FAST_BUILD = 2,
	ZENITH_RHI_ACCEL_BUILD_ALLOW_UPDATE = 4, ZENITH_RHI_ACCEL_BUILD_ALLOW_COMPACTION = 8 };
/// @contract value Zenith_RHITriangleGeometryDesc
/// Ownership: Plain value; the buffers are borrowed for the build that reads it.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes (offsets, strides); vertices; indices.
/// Errors: GetAccelerationStructureSizes refuses a format outside the three listed; shape fixed by the Stage 7 API ticket.
/// Version: since Z2 Stage 7.
/// @endcontract
struct Zenith_RHITriangleGeometryDesc
{
	Zenith_RHIBufferHandle m_xVertexBuffer; uint64 m_ulVertexOffset = 0; uint32 m_uVertexStride = 0; uint32 m_uVertexCount = 0;
	Zenith_RHIFormat m_eVertexFormat = ZENITH_RHI_FORMAT_UNDEFINED;   // SNORM16x4 positions (the default stream), FLOAT16x4 (skinned
	                                                                 // pool) or R32G32B32_SFLOAT (FLOAT32x3, a mesh whose cook precision check failed)
	Zenith_RHIBufferHandle m_xIndexBuffer; uint64 m_ulIndexOffset = 0; uint32 m_uIndexCount = 0;   // 16-bit indices
	bool m_bOpaque = true;
};
/// @contract value Zenith_RHIAccelerationStructureSizes
/// Ownership: Plain value returned by GetAccelerationStructureSizes.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 7.
/// @endcontract
struct Zenith_RHIAccelerationStructureSizes { uint64 m_ulResultBytes = 0; uint64 m_ulBuildScratchBytes = 0; uint64 m_ulUpdateScratchBytes = 0; };
// Zenith_RHIDevice additions: GetAccelerationStructureSizes, CreateAccelerationStructure, DestroyAccelerationStructure.
// Zenith_RHICommandList additions, reachable from ZenithFlux/Internal/Graph/ alone (as barriers, [Z2-N-REND-006]):
//   BuildAccelerationStructures(Zenith_Span<const Zenith_RHIAccelerationBuild>), CopyAccelerationStructureCompact(...).
```
<!-- /assumption -->

### Flux_RayTracing header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_RayTracing.h (Stage 7)
/// @contract enum Flux_RayTracingFeature
/// Ownership: Plain enumeration (bit flags).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7; which features ship where is D-RT-SCOPE (open, Stage 7).
/// @endcontract
enum Flux_RayTracingFeature : uint32
{
	FLUX_RT_FEATURE_SHADOWS = 1u << 0,
	FLUX_RT_FEATURE_REFLECTIONS = 1u << 1,
	FLUX_RT_FEATURE_AMBIENT_OCCLUSION = 1u << 2,
	FLUX_RT_FEATURE_DIFFUSE_GI = 1u << 3,
};
/// @contract value Flux_RayTracingSettings
/// Ownership: Plain value read from configuration.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Rays per pixel; roughness.
/// Errors: None; members outside their domain are clamped by the configuration schema.
/// Version: since Z2 Stage 7; shape fixed by the Stage 7 API ticket.
/// @endcontract
struct Flux_RayTracingSettings
{
	uint32 m_uFeatures = 0;                     // render.quality.rt.features, intersected with the capability matrix
	uint32 m_uShadowRaysPerPixel = 1;
	float m_fReflectionRoughnessCutoff = 0.4f;   // rougher surfaces keep IBL and SSR
	uint32 m_uGIRaysPerPixel = 1;               // detail at Stage 7
};
/// @contract type Flux_RayTracingScene
/// Ownership: Owned by the renderer; owns every bottom-level and top-level structure and their device memory.
/// Thread affinity: RENDER_SUBMIT_THREAD (the Flux.BuildGraph node); GetResidentBytes ANY_THREAD.
/// Invalidation: structures of an unloaded mesh retire through deferred deletion.
/// Units: Bytes.
/// Errors: a failed build leaves the instance out of the TLAS and is counted; shape fixed by the Stage 7 API ticket.
/// Version: since Z2 Stage 7.
/// @endcontract
class Flux_RayTracingScene                  // BLAS per (mesh, LOD); TLAS per frame from the GPU scene
{
public:
	bool IsAvailable() const;
	Flux_RGAccelerationStructure ImportTLAS(Flux_RenderGraph& xGraph);   // built this frame by Flux_AddTLASBuild
	uint64 GetResidentBytes() const;
};
void Flux_AddBLASBuilds(Flux_RenderGraph& xGraph, Flux_RayTracingScene& xScene, const Flux_FrameShared& xShared);
void Flux_AddTLASBuild(Flux_RenderGraph& xGraph, Flux_RayTracingScene& xScene, const Flux_FrameShared& xShared);
Flux_RGTexture Flux_AddRTShadows(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets, const Flux_RayTracingSettings& xSettings);
Flux_RGTexture Flux_AddRTReflections(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets, const Flux_RayTracingSettings& xSettings);
Flux_RGTexture Flux_AddRTAmbientOcclusion(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets, const Flux_RayTracingSettings& xSettings);
Flux_RGTexture Flux_AddRTDiffuseGI(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets, const Flux_RayTracingSettings& xSettings);
```

### Flux_VirtualGeometry header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_VirtualGeometry.h (Stage 7)
/// @contract type Flux_VirtualGeometryResource
/// Ownership: Created by the VG kind loader; shared through Zenith_AssetHandle<Flux_VirtualGeometryResource>.
/// Thread affinity: ANY_THREAD reads; values change on the Flux.BuildGraph node.
/// Invalidation: page residency changes every frame; the counts are fixed at load.
/// Units: Clusters; pages.
/// Errors: load failures through the asset system; shape fixed by the Stage 7 API ticket.
/// Version: since Z2 Stage 7.
/// @endcontract
class Flux_VirtualGeometryResource          // kind ASSET_KIND_VIRTUAL_GEOMETRY (requested, row AV5 (b))
{
public:
	uint32 GetClusterCount() const;
	uint32 GetPageCount() const;
	const Flux_MeshResource* GetFallbackMesh() const;   // the LOD chain non-VG paths draw
};
/// @contract type Flux_VirtualGeometrySystem
/// Ownership: Owned by the renderer; owns the VG page pool and its streaming state.
/// Thread affinity: RENDER_SUBMIT_THREAD for the frame members; GetResidentBytes ANY_THREAD.
/// Invalidation: None across frames.
/// Units: Bytes; pages.
/// Errors: per-asset failures fall back to the asset's LOD chain; shape fixed by the Stage 7 API ticket.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 7.
/// @endcontract
class Flux_VirtualGeometrySystem
{
public:
	void BeginFrame(Zenith_JobContext& xContext);                     // page residency from culling feedback
	void ImportResources(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared);
	void EndFrame(bool bSubmitted);
	uint64 GetResidentBytes() const;
};
void Flux_AddVGCulling(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets, uint32 uPhase);
void Flux_AddVGRaster(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets, uint32 uPhase);
```
