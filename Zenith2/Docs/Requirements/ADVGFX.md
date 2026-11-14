# ADVGFX requirement catalogue

## Purpose

This catalogue lists every testable obligation of the ADVGFX area (advanced graphics), one requirement per obligation: Stage 4 virtual texturing (Z2-I-ADVGFX-03, required), the Stage 5 path-traced reference (Z2-I-ADVGFX-05, required), the Stage 7 trees of hardware ray tracing (Z2-I-ADVGFX-01) and virtualized geometry (Z2-I-ADVGFX-02), both deferred with full requirement trees, and the exclusion of hair and groom (Z2-I-ADVGFX-04). Each requirement names the inventory items it covers (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "ADVGFX Advanced GFX"), the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it and the design headings that specify it in `Zenith2/Docs/Design/ADVGFX.md` and its section files. The design entry's "Requirement index" is the reverse map. A requirement whose stage is S1 serves the Stage 1 budget allocation (a single needs-human ruling with no harness half) that the S4 and S5 work depends on.

## Requirements

### Z2-R-ADVGFX-001 Software virtual texturing without sparse resources

- **Statement:** Virtual texturing is implemented with indirection page tables in an ordinary R32_UINT texture and one shared physical cache texture, and uses no sparse, reserved or tiled resource on either backend.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-013
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_StreamToResidentScene` on WARP and lavapipe; review of the device creation code (no sparse feature requested)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Virtual texturing model"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Why software page tables"

### Z2-R-ADVGFX-002 Virtual texture shape rules

- **Statement:** A virtual texture has power-of-two extents of 256 to 16,384 texels per axis (GEOM's and CONTENT's image limit), an aspect ratio of at most 8 : 1, one layer and a COLOUR, LINEAR, NORMAL or MASK usage, and the cook refuses any other texture marked virtual with ADVGFX_COOK_ERROR_VT_USAGE or ADVGFX_COOK_ERROR_VT_EXTENT before allocating any image.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_ShapeRulesEnforced` (32,768 × 4,096 fails; 256 × 256 and 16,384 × 2,048 succeed)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Virtual texture shape rules"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Cook errors"

### Z2-R-ADVGFX-003 Physical cache texture

- **Statement:** The physical cache is one 2D array texture of 4,080 × 4,080 texels per layer, one mip, BC7 sRGB with a BC7 UNORM second sampled view on Windows and ASTC 4 × 4 sRGB with an ASTC 4 × 4 UNORM second view on Android, imported every frame as Flux_FrameShared::m_xVTPhysical.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-058
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_MirrorMatchesGPU`; `Test_VirtualTexture_FootprintStaysInsidePage`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical cache texture"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical page addressing"

### Z2-R-ADVGFX-004 Physical cache sized once within its budget row

- **Statement:** Flux_Renderer::EnableVirtualTexturing sizes the physical cache, the page-table pool, the record buffer and the tail-texture reservation (render.vt.maxTailBytes) once from configuration, never resizes them, and refuses with FLUX_VT_ERROR_BUDGET_EXCEEDED when their device total exceeds Z2-B-ADVGFX-001, a device-only row, for the platform.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_BudgetRefusesOversizedPools`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical cache sizing"

### Z2-R-ADVGFX-005 Page-table pool texture

- **Statement:** The page-table pool is one R32_UINT texture with a full mip chain (2,048 per axis on desktop, 1,024 on the S24 by default), sampled with Load and written only by VTPageTableUpdate through one UAV view per mip.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_MirrorMatchesGPU`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Page-table texture"

### Z2-R-ADVGFX-006 Page-table entry encoding

- **Statement:** A page-table entry encodes physical page x in bits 0–4, y in bits 5–9, layer in bits 10–15, the resident page mip in bits 16–19, zeros in bits 20–30 and 1 in bit 31, through Flux_EncodeVTEntry and its decoders alone, on the CPU and in Slang.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_EntryEncodingRoundTrip`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Page-table entry encoding"; `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Flux_VirtualTexture header"

### Z2-R-ADVGFX-007 Every entry names a resident page

- **Statement:** Every entry of an active virtual texture names its own page when it is resident and otherwise its finest resident ancestor, and the root pages of an active virtual texture stay pinned, so no sample of an active virtual texture reads an unwritten entry.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_MirrorMapUnmap`; `Test_VirtualTexture_SampleMatchesCPUReference`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Page-table entry encoding"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Map and unmap algorithms"

### Z2-R-ADVGFX-008 Region allocation by a 2D buddy allocator

- **Statement:** Each active virtual texture owns one square, power-of-two, aligned region of the page-table pool allocated by Flux_VTRegionAllocator with deterministic lowest-address choice, merged back on free, and refused with FLUX_VT_ERROR_PAGE_TABLE_FULL when no block of its side is free.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_RegionAllocatorBuddy`; `Test_VirtualTexture_RegionAllocatorFullRefuses`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Region allocation"

### Z2-R-ADVGFX-009 CPU mirror is the authority for the page table

- **Statement:** The system keeps a CPU mirror of the used entries of every active region at every page mip (one entry per tile), cuts every page-table write from it, and at every submitted frame boundary the GPU page table equals the mirror in every row not marked dirty, and in every used entry when no row is dirty.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_MirrorMatchesGPU`; `Test_VirtualTexture_PageTableRecordCapDefers`; `Test_VirtualTexture_DeferredFrameKeepsPageTable`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "CPU mirror"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Deferred and failed frames"

### Z2-R-ADVGFX-010 Virtual texture records

- **Statement:** Each active virtual texture has one 32-byte Flux_VTRecord in a 4,096-entry persistent read-only record buffer sub-allocated slot by slot under [Z2-N-REND-548], written once at activation by an upload into a slot no handle references, with every field fixed for the texture's lifetime.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-058
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_ActivationReachesResident`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Virtual texture records"

### Z2-R-ADVGFX-011 Material handle encoding

- **Statement:** A material texture field bound to a resident virtual texture holds a handle whose word 0 is the tail texture's TEXTURE_SRV heap index and whose word 1 carries the slot in bits 0–11, zeros in bits 12–27 and the tag 0xA in bits 28–31, so a tag-blind reader samples the tail texture and Flux_SampleMaterial distinguishes the handle from an ordinary texture with no material permutation.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_MaterialHandleEncoding`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material handle encoding"

### Z2-R-ADVGFX-012 Usage fallback until resident

- **Statement:** Until a virtual texture is resident, after its activation fails and after it is unloaded, every material field naming it binds GEOM's usage fallback texture, and the binding changes by a reallocate-and-swap rewrite of the material-constants block.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_ActivationFailuresFallBack`; `Test_VirtualTexture_DisabledBindsFallback`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material handle encoding"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"

### Z2-R-ADVGFX-013 Flux_SampleVirtual algorithm

- **Statement:** Flux_SampleVirtual computes the anisotropic footprint from the explicit gradients, selects λ with the anisotropy cap, looks up the page table at floor(λ) and floor(λ) + 1, samples the physical cache with the rescaled gradients and blends the two by frac(λ), matching a CPU reference within 1/255 per channel.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_SampleMatchesCPUReference`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Flux_SampleVirtual algorithm"; `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "VirtualTexture shader module"

### Z2-R-ADVGFX-014 Wrap and clamp addressing

- **Statement:** A wrap virtual texture wraps the coordinate before the page lookup and carries wrapped borders, a clamp virtual texture saturates it and carries edge-replicated borders, and the physical cache is always sampled with uFLUX_SAMPLER_ANISO_CLAMP.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_WrapAndClampAddressing`; `Test_CookVirtualTexture_WrapBorders`; `Test_CookVirtualTexture_ClampRing`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Address modes"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Tile assembly"

### Z2-R-ADVGFX-015 Tail sampling and the root-to-tail blend

- **Statement:** A footprint at or coarser than the first tail mip samples the always-resident tail texture with the field's sampler, and a footprint between the root mip and the tail blends the root page and tail mip 0 continuously.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_TailSampling`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tail sampling"

### Z2-R-ADVGFX-016 Filter footprint stays inside the page

- **Statement:** The physical gradients are clamped so the major-axis footprint is at most 9 physical texels and the anisotropy is at most render.vt.maxAnisotropy, so no sample reads a texel of a neighbouring physical page.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FootprintStaysInsidePage`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Anisotropy cap and border"

### Z2-R-ADVGFX-017 VT filtering tolerance against streamed textures

- **Statement:** A fully resident virtual texture sampled at the test's isotropic and anisotropic footprints returns per-channel values within 2/255 (isotropic) and 6/255 (anisotropic) of GEOM's streamed texture cooked from the same source.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FilteringMatchesStreamedTexture`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Filtering tolerance against streamed textures"

### Z2-R-ADVGFX-018 Material input carries the pass's VT binding

- **Statement:** Flux_MaterialInput carries a Flux_VTBinding filled by the pass from its patched fields on m_xVTPhysical and m_xVTPageTable and from the record buffer's index, and EvaluateMaterial passes it to the public Flux_VTBeginInvocation of the Slang module VirtualTexture, which Sampling.slang and Material.slang import, so material modules keep their signature and their only write is the public reservoir mutator that X420 (b) admits into [Z2-N-REND-242].
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-015, Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_SampleMatchesCPUReference`; `Test_ShaderChecks_MaterialImplicitDerivative_Fails` stays green
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material input binding"

### Z2-R-ADVGFX-019 Every material-evaluating pass binds the VT pools

- **Statement:** While virtual texturing is enabled, every pass whose programs evaluate materials of the opaque or alphaTest domains declares reads of m_xVTPhysical and m_xVTPageTable through its optional fields and sets the pools-bound flag, so the graph orders it after every write of both pools; the transparent and decal passes bind no pool.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-055, Z2-D-056, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_EveryMaterialPassBindsPools`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material input binding"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Feedback fields of material passes"

### Z2-R-ADVGFX-020 VT bound only by opaque and alpha-tested materials

- **Statement:** Only opaque and alpha-tested materials may bind a virtual texture; the material cook refuses a virtual texture in a transparent or decal material.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** GEOM's material cook test for the refusal (row AV3 (a)); `Test_CookVirtualTexture_ShapeRulesEnforced`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Virtual texture shape rules"

### Z2-R-ADVGFX-021 Feedback request encoding

- **Statement:** A feedback request encodes the slot in bits 0–11, the page mip in bits 12–15, page x in bits 16–23 and page y in bits 24–31, with 0xFFFFFFFF meaning no request, through Flux_EncodeVTRequest and its decoder alone.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_RequestEncodingRoundTrip`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Feedback request encoding"

### Z2-R-ADVGFX-022 Feedback written by the material colour passes

- **Statement:** ForwardPlusOpaque, ForwardOpaquePerObject, GBuffer and MaterialResolve write VT feedback from the pass epilogue after EvaluateMaterial, the three pixel passes with early depth forced, and no other pass writes feedback.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-014
- **Budgets:** Z2-B-ADVGFX-003
- **Acceptance evidence:** `Test_VirtualTexture_EveryMaterialPassBindsPools`; `Test_VirtualTexture_FeedbackWrittenOncePerBlock`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Feedback writers"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Feedback fields of material passes"

### Z2-R-ADVGFX-023 One writer per feedback texel per frame

- **Statement:** Each frame exactly one pixel of every feedback block writes, at the Bayer position of the frame index for the configured feedback scale, so every pixel position of a block writes once per s² frames and no feedback texel has two writers.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FeedbackWrittenOncePerBlock`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Pixel selection and reservoir"

### Z2-R-ADVGFX-024 Deterministic reservoir choice among a pixel's requests

- **Statement:** A pixel that records n requests writes the one chosen by its hashed reservoir, each with probability 1/n, deterministically for the pixel and frame.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FeedbackReservoirUniform`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Pixel selection and reservoir"

### Z2-R-ADVGFX-025 Feedback buffers, clear and readback

- **Statement:** Each feedback view clears its feedback buffer with VTFeedbackClear before its first writer and reads it back with VTFeedbackReadback after its last, with at most four readbacks pending per view and a failed or skipped readback dropping that frame's feedback only.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-057
- **Budgets:** Z2-B-ADVGFX-003, Z2-B-RHI-011
- **Acceptance evidence:** `Test_VirtualTexture_DeferredFrameKeepsPageTable`; `Test_VirtualTexture_CountersEmitted`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Feedback buffers and readback"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "VTFeedbackClear"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "VTFeedbackReadback"

### Z2-R-ADVGFX-026 Views that write feedback

- **Statement:** Main, editor and texture views write feedback, at most eight per frame in view-list order; shadow, capture and preview views sample virtual textures without writing feedback.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_EveryMaterialPassBindsPools`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Views that write feedback"

### Z2-R-ADVGFX-027 Deterministic feedback processing

- **Statement:** Ready feedback is processed oldest first by appending, radix sorting and run-length uniquing the requests, so the (request, count) list depends only on the multiset of requests and never on the worker count or the order of readbacks within a frame.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FeedbackSortUniqueDeterministic`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Sort and unique"

### Z2-R-ADVGFX-028 Request validation and slot quarantine

- **Statement:** A request naming a slot that is not live, a mip at or above the slot's page mips or coordinates outside the mip is dropped and counted, and a freed slot is not reassigned for eight frames.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FeedbackValidationRejects`; `Test_VirtualTexture_SlotQuarantine`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Request validation"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Unload and slot quarantine"

### Z2-R-ADVGFX-029 Touch rule

- **Statement:** Each valid request touches every resident page on its chain from the requested page to the root, setting its last-touch frame and moving it to the LRU tail.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_TouchWalksAncestors`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Touch rule"

### Z2-R-ADVGFX-030 Refinement step

- **Statement:** A request whose page is not resident loads the page on its chain at mip max(m, r − 2), r the finest resident ancestor's mip, and never duplicates a target already in flight.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_RefinementStep`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Load target and refinement step"

### Z2-R-ADVGFX-031 Load priority order

- **Statement:** Load targets are issued in descending gap, descending request count, descending mip, then ascending slot, y and x order, so the issued set is deterministic for the same feedback.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_PriorityOrderDeterministic`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Priority order"

### Z2-R-ADVGFX-032 Physical page state machine

- **Statement:** Every physical page follows the FREE, UPLOADING, RESIDENT, PINNED and DRAINING states and the transitions of the physical page state machine, and returns to FREE on every failure and unload path.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_LoadUnload1000Cycles`; `Test_VirtualTexture_EvictionLRUAndGrace`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical page state machine"

### Z2-R-ADVGFX-033 Tile request state machine

- **Statement:** Every tile request follows the IDLE, READING, DECODING, DECODED, UPLOADING and CANCELLING states and transitions of the tile request state machine, and a cancelled request leaves no staging, read or upload behind.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_LoadUnload1000Cycles`; `Test_VirtualTexture_ReadAndDecodeFailures`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile request state machine"

### Z2-R-ADVGFX-034 Eviction policy

- **Statement:** When no page is free the victim is the least recently touched unpinned page outside the eight-frame grace window, ties broken by finer mip then slot, mip, y and x; nothing is evicted when every candidate is inside the window.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_EvictionLRUAndGrace`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Eviction policy"

### Z2-R-ADVGFX-035 Draining rule for evicted and unloaded pages

- **Statement:** A page leaves DRAINING for FREE only after submitted frames have applied every unmap row of its eviction, or at EndFrame(true) of the first submitted frame after its texture's unload, and a tile is uploaded only into a FREE page, so no frame samples a page through an entry naming its previous tile while its next tile is copied.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_EvictedPageNotSampledDuringRefill`; `Test_VirtualTexture_SplitUnmapRowsDrainByEpoch`; `Test_VirtualTexture_UnloadDrainsAtFirstSubmittedFrame`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Draining rule"

### Z2-R-ADVGFX-036 In-flight limits

- **Statement:** Tile reads in flight, tiles decoded and uploaded per frame and decoded tiles waiting for a page stay within render.vt.maxReadsInFlight, render.vt.tilesPerFrame (at most half of Z2-B-ADVGFX-002, raised to the row value only by the camera-cut boost) and twice Z2-B-ADVGFX-002.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-002
- **Acceptance evidence:** `Test_VirtualTexture_CameraCutBoost`; `Test_VirtualTexture_CountersEmitted`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "In-flight limits"

### Z2-R-ADVGFX-037 Tile reads through the asset reader

- **Statement:** A tile is read with Zenith_AssetReader::ReadRange of its stored bytes at its tile-index offset, with ASSET_PRIORITY_HIGH when the refinement gap is at least three mips and ASSET_PRIORITY_NORMAL otherwise.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_StreamToResidentScene`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile request state machine"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile I/O and decode"

### Z2-R-ADVGFX-038 Tile verification and decode in sub-jobs

- **Statement:** Completed tile reads are verified against their stored check and decompressed with Zenith_DecompressSection in ParallelFor sub-jobs of the build node, each into its own staging slot, independently of the worker count.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-004, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_ReadAndDecodeFailures`; `Test_VirtualTexture_StreamToResidentScene`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile I/O and decode"

### Z2-R-ADVGFX-039 Read and decode failures

- **Statement:** A failed read returns the tile to IDLE and counts Flux.VT.ReadFailures, and a tile that fails its check or decode counts Flux.VT.CorruptTiles and marks only that page corrupt while the virtual texture stays resident.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_ReadAndDecodeFailures`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile request state machine"; `Zenith2/Docs/Design/ADVGFX.md` → "Failure table"

### Z2-R-ADVGFX-040 Tile uploads in the VT_TILE class

- **Statement:** Tile uploads go through GEOM's streaming uploader in FLUX_UPLOAD_CLASS_VT_TILE, unsplittable, at most the frame's tile limit and never above Z2-B-ADVGFX-002 tiles per frame, and the frame's StreamingUpload pass declares COPY_DEST on m_xVTPhysical.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-002, Z2-B-GEOM-003
- **Acceptance evidence:** GEOM's `Test_TextureStreaming_UploadArbitration` with VT tiles queued; `Test_VirtualTexture_TileUploadThenSample`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile upload"

### Z2-R-ADVGFX-041 Map after upload completion

- **Statement:** A page is mapped into the page table only in a frame whose build finds its tile upload COMPLETE.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_TileUploadThenSample`; `Test_VirtualTexture_EvictedPageNotSampledDuringRefill`; RHI's `Test_GPUScene_VTTileUploadThenSample` ([Z2-N-REND-555]) cited, not owned
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical page state machine"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Map and unmap algorithms"

### Z2-R-ADVGFX-042 Map and unmap algorithms

- **Statement:** Map writes the page's entry into every subtree entry whose resident mip is coarser, and unmap replaces every subtree entry naming the page with its parent's entry, never disturbing an entry that names a finer resident page.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_MirrorMapUnmap`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Map and unmap algorithms"

### Z2-R-ADVGFX-043 Dirty rows and run records

- **Statement:** Changed rows are run-length encoded from the mirror into 16-byte Flux_VTPageTableRun records per mip, at most 65,536 per frame with rows carrying an eviction epoch first in ascending epoch order, declared at setup and written after the reservation is final; a cut is applied only at EndFrame(true).
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_VirtualTexture_PageTableRecordsRunLength`; `Test_VirtualTexture_PageTableRecordCapDefers`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Dirty rows and run records"

### Z2-R-ADVGFX-044 VTPageTableUpdate pass

- **Statement:** VTPageTableUpdate applies the frame's run records with one compute dispatch per mip that has records, after StreamingUpload and before every consumer, with no write race and no pass on a frame without records.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-055
- **Budgets:** Z2-B-ADVGFX-003
- **Acceptance evidence:** `Test_VirtualTexture_PageTableRecordsRunLength`; `Test_VirtualTexture_MirrorMatchesGPU`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "VTPageTableUpdate pass"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "VTPageTableUpdate pass table"; `Zenith2/Docs/Design/ADVGFX.md` → "Common prologue placement"

### Z2-R-ADVGFX-045 Frame-level flow on the build node

- **Statement:** The VT system runs BeginFrame at renderer step 3c, imports the pools and declares its pass at step 7, writes its frame data at step 11 and calls EndFrame on every path out of the build node at step 13, in the orders of the frame-level flow table.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_DeferredFrameKeepsPageTable`; `Test_VirtualTexture_StreamToResidentScene`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Frame-level flow"

### Z2-R-ADVGFX-046 Deferred and failed frames keep the page table consistent

- **Statement:** A deferred or failed frame keeps every dirty row dirty, every draining page draining and every mapping in the mirror, so after any sequence of submitted, deferred and failed frames the GPU page table equals the mirror in every row not marked dirty at every submitted frame boundary, and in every used entry when no row is dirty.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_DeferredFrameKeepsPageTable`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Deferred and failed frames"

### Z2-R-ADVGFX-047 Kind loader for virtual textures

- **Statement:** Flux_VirtualTextureKindLoader serves ASSET_KIND_VIRTUAL_TEXTURE with the BeginLoad, OnSectionsLoaded, Activate, Cancel, Unload and DropCpuCopies behaviour of the activation table, registered by EnableVirtualTexturing.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_LoaderContract`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"

### Z2-R-ADVGFX-048 Activation reaches residency

- **Statement:** An activated virtual texture gets a slot, a region, its pinned root pages and its tail texture, uploads its tail, record and root tiles in the BLOCKING class, fills its region with the root entries, and reports GPU residency only after a submitted frame executed the fill.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_ActivationReachesResident`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"

### Z2-R-ADVGFX-049 Activation failures release and fall back

- **Statement:** Slots full, page table full, pool full, a failed activation upload and a corrupt root tile each release everything allocated, report FAILED with their Flux_VirtualTextureError and leave materials on the usage fallback.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_ActivationFailuresFallBack`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"; `Zenith2/Docs/Design/ADVGFX.md` → "Failure table"

### Z2-R-ADVGFX-050 Unload releases after retirement

- **Statement:** Unload pushes the object and the device generation onto flux.vt.unloadQueue and touches no VT state; the step-3c unload drain, run before the activation drain, cancels the texture's reads, decodes and uploads, discards its dirty rows, moves its pages to DRAINING until EndFrame(true) of the first submitted frame after that drain, frees its region and tail through deferred deletion, returns its host and tail bytes and quarantines its slot, and an entry older than the last device-loss recovery releases nothing.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_LoadUnload1000Cycles` (Unload from a second thread, `win-asan`); `Test_VirtualTexture_UnloadDrainsAtFirstSubmittedFrame`; `Test_VirtualTexture_UnloadAfterDeviceLossReleasesNothing`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Unload and slot quarantine"

### Z2-R-ADVGFX-051 1,000 load and unload cycles

- **Statement:** 1,000 load and unload cycles at randomised states leave no slot, region, page, staging, upload, read or allocation behind.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_LoadUnload1000Cycles`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Unload and slot quarantine"

### Z2-R-ADVGFX-052 Full residency mode

- **Statement:** In full residency mode every tile of every page mip becomes resident after activation, nothing is evicted, IsSettled turns true when nothing is in flight, and a texture that does not fit is FAILED with FLUX_VT_ERROR_POOL_FULL.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_FullModeSettles`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Residency modes"

### Z2-R-ADVGFX-053 VT parity scenes in full mode

- **Statement:** The VirtualTexture parity scene renders in full residency mode bit-identically with transient aliasing on and off and within the backend and path parity tolerances.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-057, Z2-D-063
- **Budgets:** Z2-B-RPROD-031
- **Acceptance evidence:** `Test_AliasingParity_VirtualTexture`, `Test_BackendParity_VirtualTexture` (`BackendParity` step); `Test_PathParity_VirtualTexture_<PathA>_<PathB>` (`PathParity`, Stage 5)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Residency modes"; `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Test targets and gate steps"

### Z2-R-ADVGFX-054 Streaming reaches the full-mode image

- **Statement:** In streaming mode the pages a static view of the synthetic 4,096 × 4,096 fixture requests become resident within 48 frames and the frame 64 image equals the full-mode image within 1/255.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-002
- **Acceptance evidence:** `Test_VirtualTexture_StreamToResidentScene`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Residency modes"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Frame-level flow"

### Z2-R-ADVGFX-055 Camera cut boost

- **Statement:** A camera cut raises the upload and decode limits from render.vt.tilesPerFrame to the platform's Z2-B-ADVGFX-002 value for render.vt.cutBoostFrames frames, never beyond that BR-DEFER row and never ahead of the BLOCKING upload class.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-002
- **Acceptance evidence:** `Test_VirtualTexture_CameraCutBoost`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Camera cuts"

### Z2-R-ADVGFX-056 Memory accounting

- **Statement:** The VT system reports its device-local bytes against Z2-B-ADVGFX-001, a device-only row, and its host bytes against the proposed row "Virtual texturing host memory", both under MEMORY_SUBSYSTEM_RENDERING.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_CountersEmitted`; `Test_VirtualTexture_HostCapRefusesActivation`; the RTX 3070 VT budget run
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Memory accounting and pressure"

### Z2-R-ADVGFX-057 Pressure shedding coarsens requests

- **Statement:** At WARNING the feedback bias rises by one mip, at CRITICAL by two with the read and upload limits halved, both revert one frame after the level falls, and pressure never resizes the VT pools.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_PressureBiasCoarsensRequests`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Memory accounting and pressure"

### Z2-R-ADVGFX-058 Device loss rebuilds virtual texturing

- **Statement:** Device-loss recovery re-creates the VT pools and record buffer, frees every page and mirror, and reloads every virtual texture through a device-recovery load, so 64 frames after recovery the full-mode frame equals the pre-loss frame.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_DeviceLossRebuilds`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Device loss recovery"

### Z2-R-ADVGFX-059 Disabled virtual texturing

- **Statement:** With render.vt.enabled false no VT device resource exists, every virtual texture load fails with FLUX_VT_ERROR_NOT_ENABLED, materials bind the usage fallback and no pass declares a VT field.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_DisabledBindsFallback`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Disabled virtual texturing"

### Z2-R-ADVGFX-060 Headless validate-only loading

- **Statement:** A headless registry serves virtual textures through ASSET's validate-only loader and creates no GPU object.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_HeadlessValidateOnly`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"

### Z2-R-ADVGFX-061 EnableVirtualTexturing creation and unwinding

- **Statement:** EnableVirtualTexturing validates its desc, creates the physical cache, page-table pool, record buffer and staging pool in that order, unwinds exactly the earlier steps on a failure, refuses a second call with FLUX_VT_ERROR_ALREADY_ENABLED, and is called at startup step 13 whether or not virtual texturing is enabled.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTextureSystem_Create_FailsAtPhysicalCache_Unwinds` and its three siblings
- **Design:** `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Flux_VirtualTexture header"; `Zenith2/Docs/Design/ADVGFX.md` → "ZenithRuntime call sites"

### Z2-R-ADVGFX-062 VT configuration keys

- **Statement:** The render.vt.* keys register with their types, per-platform defaults, domains and restart flags through Flux_RegisterVirtualTextureConfigKeys, and Flux_ReadVirtualTextureDesc reads them.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-002
- **Acceptance evidence:** `Test_VirtualTexture_ConfigKeysRegistered`
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Configuration keys"

### Z2-R-ADVGFX-063 VT public header

- **Statement:** Flux_VirtualTexture.h declares the constants, error enumeration, residency mode, desc, info, residency, resource, stats and system types, the codecs, the configuration functions, the three feature functions and the DEVT registration function exactly as the public API specifies, each type with one contract block.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-047, Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` gate steps over the header
- **Design:** `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Flux_VirtualTexture header"

### Z2-R-ADVGFX-064 VT shader module

- **Statement:** Zenith2/Shaders/Common/VirtualTexture.slang declares the shared VT structs and functions of the shader module table, free of derivative instructions, with the CPU and GPU codecs bit-identical.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_EntryEncodingRoundTrip`; `Test_VirtualTexture_SampleMatchesCPUReference`; `ShaderChecks` step
- **Design:** `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "VirtualTexture shader module"

### Z2-R-ADVGFX-065 VT counters

- **Statement:** The VT system emits every Flux.VT counter of the counter table once per render frame.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_CountersEmitted`
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Counters"

### Z2-R-ADVGFX-066 VT developer tools

- **Statement:** ADVGFX registers its console commands, overlay tab, debug views and frame-dump section through Flux_RegisterVirtualTextureDevTools and its entry in the DEVT area list.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_DevToolsRegistered`
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Console commands"; `Zenith2/Docs/Design/ADVGFX.md` → "Debug views"; `Zenith2/Docs/Design/ADVGFX.md` → "Frame dump section"; `Zenith2/Docs/Design/ADVGFX.md` → "On-screen profiler rows"

### Z2-R-ADVGFX-067 Editor surfaces for virtual textures

- **Statement:** The editor's asset browser and inspector show a virtual texture's shape, tiles, bytes and residency, and the import preset exposes texture.virtual and texture.virtualMipBias.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** EDIT's inspector test for kind 18 (row AV13); `Test_CookVirtualTexture_MipBiasRange`
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Editor surfaces"

### Z2-R-ADVGFX-068 S24 virtual texturing

- **Statement:** Virtual texturing runs on the S24 with the mobile parameters (ASTC 4 × 4, four layers, page-table extent 1,024, feedback scale 16, anisotropy cap 4, 24 tiles per frame) on Forward+.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-010
- **Budgets:** Z2-B-ADVGFX-001, Z2-B-ADVGFX-002, Z2-B-ADVGFX-003
- **Acceptance evidence:** the S24 VT run and the S24 VT correctness device step (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Mobile parameters"; `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Device runs"

### Z2-R-ADVGFX-069 VT budget measurement

- **Statement:** The Stage 4 RTX 3070 and S24 budget runs measure Z2-B-ADVGFX-001, Z2-B-ADVGFX-002, Z2-B-ADVGFX-003, the host row and the two latency rows on W-Outdoor and W-OpenWorld with Zenith_Bench_VirtualTexture in its budget-run mode, confirming the values the Stage 1 ruling allocated.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-ADVGFX-001, Z2-B-ADVGFX-002, Z2-B-ADVGFX-003
- **Acceptance evidence:** the evidence records of the RTX 3070 VT budget run and the S24 VT run under `Zenith2/Docs/Research/` and the budget-run outputs
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Rows ADVGFX owns"; `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Benchmarks and harnesses"

### Z2-R-ADVGFX-070 VT first-hop latency

- **Statement:** The p95 number of frames from the first frame whose feedback requests a non-resident page to the first frame that samples its load target is reported as Flux.VT.FirstHopFramesP95 and measured against the pending first-hop latency row, whose value lies above the structural floor of 2F + 1 frames.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the RTX 3070 and S24 VT budget runs against the pending row "Virtual texturing first-hop latency" (X422)
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Pending row needs"

### Z2-R-ADVGFX-071 Teleport residency includes VT

- **Statement:** The MP-STREAM teleport measurement includes the virtual texture pages of the destination view through Flux.VT counters.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** the W-OpenWorld MP-STREAM run (DEVT's runner, row AV10)
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Telemetry"

### Z2-R-ADVGFX-072 VT budget harness

- **Statement:** Zenith_Bench_VirtualTexture under Zenith2/Benchmarks/VirtualTexture/ lands with the Stage 4 VT implementation, flies over a VT-backed terrain fixture with a CTest smoke that asserts no failure counter rises, and its measurement mode is the Stage 4 budget run.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-084
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Zenith_Bench_VirtualTexture` CTest smoke in the `Test` step
- **Design:** `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Benchmarks and harnesses"

### Z2-R-ADVGFX-073 VT budget rows allocated

- **Statement:** The Stage 1 per-system budget ticket for ADVGFX, a single needs-human ruling with no harness half, writes values for Z2-B-ADVGFX-001, Z2-B-ADVGFX-002 and Z2-B-ADVGFX-003 from the derived sizing of the design and published practice; the Stage 4 budget run confirms them.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** Z2-B-ADVGFX-001, Z2-B-ADVGFX-002, Z2-B-ADVGFX-003
- **Acceptance evidence:** the adopted rows in `Zenith2/Docs/Budgets.md` and the DecisionLog entry of the ruling ticket, whose basis is "Physical cache sizing" and published practice
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Rows ADVGFX owns"; `Zenith2/Docs/Design/ADVGFX.md` → "Stage 1"

### Z2-R-ADVGFX-074 Import preset flag selects a virtual texture

- **Statement:** The import preset key texture.virtual makes CONTENT's image importer declare an ASSET_KIND_VIRTUAL_TEXTURE product with the same GUID and the same geom.textureSource intermediate a streamed texture would have.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** CONTENT's importer test for the flag (row AV6); `Test_CookVirtualTexture_ShapeRulesEnforced`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Import preset flag"

### Z2-R-ADVGFX-075 Cook pipeline steps

- **Statement:** Cook_VirtualTexture reads and validates the intermediate, checks the shape rules, builds and encodes the mip chain with GEOM's cook functions, assembles and compresses the tiles and writes the .zvt with its cook-report entry, in the order of the cook pipeline table.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_TailAndRootSections`; `Test_CookVirtualTexture_FormatsPerPlatform`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Cook pipeline"

### Z2-R-ADVGFX-076 Tile assembly from encoded mips

- **Statement:** Tiles are cut from the encoded block grid of their mip, 34 × 34 blocks each, with wrapped blocks for wrap textures and clamp-ring blocks for clamp textures, so a tile's border blocks equal its neighbours' interior blocks byte for byte.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_TileBordersMatchNeighbours`; `Test_CookVirtualTexture_ClampRing`; `Test_CookVirtualTexture_WrapBorders`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Tile assembly"

### Z2-R-ADVGFX-077 Per-tile compression

- **Statement:** Each tile is compressed alone with the asset's compression setting and stored NONE when that is not smaller, with its own Zenith_CompressedSectionDesc in its index entry.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_TileCompressionAndStoreFallback`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Tile compression"

### Z2-R-ADVGFX-078 Deterministic VT cook

- **Statement:** Clean and incremental cooks of the same virtual texture produce byte-identical .zvt files, and the derived-data key covers every input of the determinism table.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_Deterministic`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Determinism and derived-data keys"

### Z2-R-ADVGFX-079 Cooked virtual texture format

- **Statement:** A .zvt is ASSET's container with magic ZVTX, kind 18, format version 1 and exactly the INFO, TILE_INDEX, TAIL, ROOT and TILE_DATA sections with the documented layouts, little-endian and written field by field.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_TailAndRootSections`; `Test_VirtualTexture_ReaderRejectsCorruption`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "The cooked virtual texture format"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Info section"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Tile index section"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Tail section"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Root section"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Tile data section"

### Z2-R-ADVGFX-080 VT reader validation

- **Statement:** The VT reader applies rows V1 to V10 in order after ASSET's checks and returns the first failure as a Flux_VirtualTextureError, never asserting on file data.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_ReaderRejectsCorruption`; fuzz target `Zenith_Fuzz_VirtualTextureFile`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Reader validation"

### Z2-R-ADVGFX-081 VT parser limits

- **Statement:** Every .zvt parser limit is a constant of Flux_VTReader.h tested at the limit and at limit + 1.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_LimitsAtAndPastLimit`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Parser limits"

### Z2-R-ADVGFX-082 VT engine fallbacks

- **Statement:** Cook_VirtualTexture declares the role-0 and per-usage engine fallback virtual textures, and a cook missing one fails with COOK_ERROR_FALLBACK_MISSING.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_EngineFallbacksDeclared`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Engine fallbacks"

### Z2-R-ADVGFX-083 VT cook report entry

- **Statement:** The cook report carries one virtualTextures entry per cooked virtual texture with every documented field.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_ReportEntry`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Cook report entry"

### Z2-R-ADVGFX-084 VT cook errors

- **Statement:** The VT cook errors map onto COOK_ERROR_STEP_FAILED with the ADVGFX code in the detail field, naming the product, the source, the rule and the value.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_ShapeRulesEnforced`; `Test_CookVirtualTexture_MipBiasRange`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Cook errors"

### Z2-R-ADVGFX-085 Platform formats of virtual textures

- **Statement:** Windows cooks produce BC7 virtual textures and Android cooks ASTC 4 × 4, sRGB for colour, and the runtime refuses the other platform's file with FLUX_VT_ERROR_FORMAT_UNSUPPORTED.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_FormatsPerPlatform`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Info section"; `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Reader validation"

### Z2-R-ADVGFX-086 Path tracer role and boundaries

- **Statement:** The path tracer serves physical-accuracy validation only, shares EvaluateMaterial and LIGHT's BRDF terms with the raster paths, runs as compute on WARP and lavapipe, and is never part of a shipping renderer.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** review against "Role and boundaries"; `Test_PathTracer_SelfWhiteFurnace` on WARP and lavapipe
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Role and boundaries"

### Z2-R-ADVGFX-087 ZenithPathTracer library and link closure

- **Statement:** ZenithPathTracer is a tool-side static library under Zenith2/Tools/ZenithPathTracer/ that declares DEPENDS ZenithCore ZenithRHI ZenithVertexFormat ZenithCompression ZenithFlux ZenithShadersGenerated and no other edge, supports win-debug, win-release, win-tools and win-asan, is linked by ZenithFlux_Tests through TEST_DEPENDS and by ZenithPathTracer_Tests alone, and the Libgraph gate step fails any Player, game or runtime target that depends on it.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (Shipping link closure); `CompileCoverage`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Library targets and programs"; `Zenith2/Docs/Design/ADVGFX.md` → "ZenithPathTracer files"

### Z2-R-ADVGFX-088 Tracer programs compiled offline

- **Statement:** The tracer's programs under Zenith2/Shaders/Tests/PathTracer/ are compiled offline by ZenithShaderCompiler, PTMaterialResolve enumerates every material variant of the cooked content, and Run creates every PSO before its first iteration.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-025, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_RunIsDeterministic`; `ShaderChecks` step
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Library targets and programs"

### Z2-R-ADVGFX-089 Path tracer public API

- **Statement:** Flux_PathTracer.h and Flux_PathTraceReference.h declare the types and functions of the public API exactly, each type with one contract block whose thread affinity begins with a vocabulary keyword, each header including the header of every type it names, and the runner and metrics declarations are test support of ZenithFlux_Tests (Tests/PhysicalAccuracy/Runner/) with the same contract blocks.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` over the two headers; review of the test-support header
- **Design:** `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Flux_PathTracer header"; `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Flux_PathTraceReference header"; `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Flux_PhysicalAccuracyRunner header"

### Z2-R-ADVGFX-090 Reference scene documents

- **Statement:** A reference scene is a parity scene document carrying a pathTrace object whose keys follow the documented rules; the runner and Zenith_Bench_PathTraceReference both read it through the header-only scene-document helper, whose instance, light and camera lists equal RPROD's parity loader's, so the raster run and the trace are built from identical content.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-004, Z2-B-ADVGFX-005, Z2-B-ADVGFX-006, Z2-B-ADVGFX-007, Z2-B-ADVGFX-008
- **Acceptance evidence:** `Test_PTSceneDocument_MatchesParityLoader`; `Test_PhysicalAccuracy_RunnerModes`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference scenes"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Scene-document helper"

### Z2-R-ADVGFX-091 Scene builder

- **Statement:** Flux_PathTraceSceneBuilder decodes meshes through ZenithVertexFormat, accepts terrain triangles, lights, environments and the camera, refuses skinned, procedural, transparent and decal content, and validates every input.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_SelfLambertPlane`; `Test_PathTracer_SelfRayDifferentials`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Scene builder"

### Z2-R-ADVGFX-092 Terrain in reference scenes

- **Statement:** The tracer traces ENV's terrain through its LOD-0 collision tiles with the terrain material, so the TerrainVT scene compares raster and trace on the same surface.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** `Test_EnvRender_TerrainVTReference` (ENV's, through the runner)
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Scene builder"

### Z2-R-ADVGFX-093 Deterministic binned SAH BVH

- **Statement:** The BVH is a binary binned-SAH tree with 32 bins, leaves of at most four triangles and depth at most 64, built deterministically, and inputs beyond its limits fail with FLUX_PT_ERROR_BVH_LIMIT.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PTBVH_BuildDeterministic`; `Test_PTBVH_SAHLeafRules`; `Test_PTBVH_LimitsRefused`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "BVH"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Builder"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Node and triangle layout"

### Z2-R-ADVGFX-094 Watertight traversal

- **Statement:** Traversal uses the watertight ray-triangle test, nearer-child-first order, a 64-entry stack and a scale-aware origin offset, and finds the same closest hit as a brute-force search.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PTTraversal_WatertightEdges`; `Test_PTTraversal_ClosestHitMatchesBruteForce`; `Test_PTTraversal_AnyHitShadow`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Traversal"

### Z2-R-ADVGFX-095 Wavefront pipeline

- **Statement:** One iteration runs PTRayGen, PTIntersect, PTClassify, PTMaterialResolve per variant, PTShade and PTShadowIntersect for each bounce and PTAccumulate, as compute passes of the Flux render graph each batch builds inside Flux_Renderer::RunToolGraph, with synthesised barriers.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-055, Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_RunIsDeterministic`; the self-check tests; sync validation clean
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Wavefront pipeline"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Path state"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Kernels in order"

### Z2-R-ADVGFX-096 Materials evaluated with shared code and analytic gradients

- **Statement:** PTMaterialResolve evaluates every hit with EvaluateMaterial, primary-ray differentials and secondary ray cones, and treats alpha-tested hits below the cutoff as absent.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_SelfRayDifferentials`; `Test_PTTraversal_AnyHitShadow`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Materials and gradients"

### Z2-R-ADVGFX-097 Virtual textures in the tracer

- **Statement:** A traced virtual-texture material samples through Flux_SampleVirtual in full residency mode with feedback off.
- **Covers:** Z2-I-ADVGFX-05, Z2-I-ADVGFX-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_VirtualTextureFullMode`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Materials and gradients"

### Z2-R-ADVGFX-098 BSDF evaluation and sampling

- **Statement:** The tracer evaluates LIGHT's full BRDF through Flux_EvaluateBRDF, samples it by the specular/diffuse mixture with GGX visible-normal and cosine sampling, and weights light and BSDF samples with the power heuristic.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PTSampling_GGXVNDFPdfIntegratesToOne`; `Test_PTSampling_CosinePdf`; `Test_PTSampling_MISWeightsSumToOne`; `Test_PathTracer_SelfWhiteFurnace`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "BSDF evaluation and sampling"

### Z2-R-ADVGFX-099 Lights and the environment

- **Statement:** The tracer samples point, spot, sphere, rect, disc and directional lights in LIGHT's light model and units and the environment through an alias table, as the lights table specifies.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PTSampling_SphereLightSolidAngle`; `Test_PTSampling_AliasTableMatchesDistribution`; `Test_PathTracer_SelfSphereLightFormFactor`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Lights and the environment"

### Z2-R-ADVGFX-100 Four integrators

- **Statement:** The tracer provides the radiance, ambient-visibility, light-visibility and specular-reflection integrators with the documented outputs.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_IntegratorAmbientVisibilityAnalytic`; `Test_PathTracer_IntegratorLightVisibilityAnalytic`; `Test_PathTracer_IntegratorSpecularMirror`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Integrators"

### Z2-R-ADVGFX-101 Counter-based random numbers

- **Statement:** Every random number is pcg4d of (pixel, sample, dimension) with the documented dimension allocation, so results are independent of the batch size and kernel order.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PTRandom_SequencesIndependentOfBatch`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Random numbers"

### Z2-R-ADVGFX-102 Accumulation and convergence criterion

- **Statement:** Batch sums are accumulated on the CPU in double, and a run is converged when it has at least the minimum samples and the converged-pixel fraction meets the scene's criterion.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-006, Z2-B-ADVGFX-007
- **Acceptance evidence:** `Test_PTAccumulate_DoubleSumsAndStandardError`; `Test_PTConvergence_CriterionAndCaps`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Accumulation and convergence"

### Z2-R-ADVGFX-103 Wall-clock and resolution caps

- **Statement:** A run that reaches its wall-clock cap or maximum samples before convergence returns NOT_CONVERGED and its comparison fails, and no scene traces above its resolution cap.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-004, Z2-B-ADVGFX-005
- **Acceptance evidence:** `Test_PTConvergence_CriterionAndCaps`; the acceptance-mode reports
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Accumulation and convergence"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Proposed per-scene caps"

### Z2-R-ADVGFX-104 Tracer determinism

- **Statement:** Two runs of the same scene and settings on one backend, device, driver and adapter version return bit-identical results.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_RunIsDeterministic`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Determinism"

### Z2-R-ADVGFX-105 Self-check scenes

- **Statement:** The white-furnace, Lambert-plane, sphere-light form-factor and ray-differential self-check scenes converge to their closed forms within three standard errors.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_SelfWhiteFurnace`; `Test_PathTracer_SelfLambertPlane`; `Test_PathTracer_SelfSphereLightFormFactor`; `Test_PathTracer_SelfRayDifferentials`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Gate and acceptance runs"

### Z2-R-ADVGFX-106 Raster capture for comparison

- **Statement:** The runner captures the raster path's R11G11B10 HDR scene colour after TAA and before exposure, decoded to float and divided by the pre-exposure, after 64 frames of the parity jitter sequence with the scene's features off and virtual textures settled in full mode, within the stated quantisation bounds.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicalAccuracy_RasterCaptureUnits`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Raster capture"

### Z2-R-ADVGFX-107 Comparison tonemap

- **Statement:** Both images are compared after the operator T(c) = srgbEncode(1 − exp(−e c)) at the scene's exposure.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PTMetrics_FlipMatchesReferenceImplementation`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Comparison tonemap"

### Z2-R-ADVGFX-108 Error metrics

- **Statement:** The runner computes FLIP mean, relative luminance error, mean absolute error, misclassified fraction and the z-above-4 fraction over the scene's mask exactly as defined, in test support compiled into ZenithFlux_Tests alone.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** `Test_PTMetrics_FlipMatchesReferenceImplementation`; `Test_PTMetrics_RelativeLuminanceAnalytic`; `Test_PTMetrics_MaskRespected`; `Test_PTMetrics_ZScoreSyntheticNoise`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Error metrics"

### Z2-R-ADVGFX-109 Every available raster path compared

- **Statement:** Each reference scene compares every raster path its pathTrace object lists that the platform provides.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-035
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** LIGHT's and ENV's comparison tests in the Stage 5 acceptance suite, one report per path
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference scenes"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Gate and acceptance runs"

### Z2-R-ADVGFX-110 Reference file format

- **Statement:** A .zref holds the documented 160-byte header and the compressed float16 mean and standard-error planes, and its reader refuses any corrupt file with FLUX_PT_ERROR_REFERENCE_CORRUPT.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PTReference_RoundTrip`; `Test_PTReference_ReaderRejectsCorruption`; fuzz target `Zenith_Fuzz_PathTraceReference`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference files"

### Z2-R-ADVGFX-111 Reference manifest and fingerprint

- **Statement:** References.json lists every committed reference with its stored bytes and an optional pendingRecording key, and each reference's fingerprint is SHA-256 over the seven documented inputs.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_PTReference_FingerprintCoversInputs`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference manifest and fingerprint"

### Z2-R-ADVGFX-112 Reference recording harness

- **Statement:** Zenith_Bench_PathTraceReference, which declares ZenithPathTracer and ZenithDocument among its dependencies, reads every reference scene through the scene-document helper, traces it to convergence with Flux_PathTracer on WARP and lavapipe and writes the references into the build output, with a CTest smoke that records and reads back a self-check scene.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_PathTraceReference` CTest smoke in the `Test` step
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference capture procedure"; `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Benchmarks and harnesses"

### Z2-R-ADVGFX-113 References recorded by a needs-human ticket

- **Statement:** Committed references are produced by running the recording harness and committed, LFS-tracked, by a needs-human recording ticket, never by a worker; a change that stales a reference lands under the pendingRecording protocol and its re-record is a needs-human ticket of that shape.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the committed `References.json` and `.zref` files of the recording ticket; `Test_PhysicalAccuracy_ReferencesFresh`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference capture procedure"

### Z2-R-ADVGFX-114 Reference freshness check

- **Statement:** Test_PhysicalAccuracy_ReferencesFresh recomputes every reference's fingerprint and fails on a missing file, on a mismatch without a pendingRecording key (naming the scene and the first differing input) and on a marker over a fresh entry.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicalAccuracy_ReferencesFresh`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference capture procedure"

### Z2-R-ADVGFX-115 Gate-mode comparison

- **Statement:** In gate mode each comparison test compares its raster capture with the committed reference and traces 32 samples at most 128 × 128 against the 2 × 2 box-downsampled reference with the z test, within the 30-minute phase cap, and skips with a recorded reason a scene whose reference is marked pendingRecording.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-028
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** `Test_PhysicalAccuracy_RunnerModes`; LIGHT's and ENV's comparison tests in the `Test` step
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Gate and acceptance runs"

### Z2-R-ADVGFX-116 Acceptance-mode in-run convergence

- **Statement:** In the Stage 5 acceptance suite every reference scene is traced to convergence in-run on WARP and on lavapipe under its caps and every listed raster path is compared with that result within the scene's tolerance.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-083
- **Budgets:** Z2-B-ADVGFX-004, Z2-B-ADVGFX-005, Z2-B-ADVGFX-006, Z2-B-ADVGFX-007, Z2-B-ADVGFX-008
- **Acceptance evidence:** the Stage 5 acceptance suite reports (`Run-Committed.ps1 -Acceptance -Stage 5`)
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Gate and acceptance runs"

### Z2-R-ADVGFX-117 Reports and failure artefacts

- **Statement:** Every runner call writes report.json, and a failure also retains both images, both tonemapped images, the FLIP, ratio and z maps and the graph dump.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicalAccuracy_FailureRetainsArtefacts`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reports and artefacts"

### Z2-R-ADVGFX-118 Path tracer failure handling

- **Statement:** Invalid input, limits, allocation failure, device loss, the wall-clock cap and cancellation behave as the failure table states, and every allocation failure unwinds what the call allocated.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_CancelReleases`; the five `Test_PathTracer_*_FailsAt*_Unwinds` cases
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Path tracer failure handling"

### Z2-R-ADVGFX-119 Per-scene caps and tolerances proposed

- **Statement:** Each reference scene carries the proposed resolution, wall-clock (one value per scene, lavapipe's), sample, convergence and tolerance values of the caps table until the Stage 1 per-system budget ticket for ADVGFX adopts or replaces them, with the two values per scene of Z2-B-ADVGFX-007 and Z2-B-ADVGFX-008 noted on those rows.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-012
- **Budgets:** Z2-B-ADVGFX-004, Z2-B-ADVGFX-005, Z2-B-ADVGFX-006, Z2-B-ADVGFX-007, Z2-B-ADVGFX-008
- **Acceptance evidence:** the adopted rows in `Zenith2/Docs/Budgets.md`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Proposed per-scene caps"

### Z2-R-ADVGFX-120 Compute traversal is the reference

- **Statement:** The tracer traverses only the software BVH in compute at every stage unless flagged proposal ADVGFX-1 is approved, and every acceptance run uses the compute traversal.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicalAccuracy_RunnerModes`; review of the runner's traversal selection
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Compute and hardware traversal"

### Z2-R-ADVGFX-121 Approximation-error references

- **Statement:** The tracer's visibility and specular integrators produce the references LIGHT's approximation-error scenes compare GTAO, contact shadows, SSR and SSGI against.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-008
- **Acceptance evidence:** LIGHT's `AE_GTAO`, `AE_ContactShadows`, `AE_SSR`, `AE_SSGI` in the Stage 5 acceptance suite through the runner
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Integrators"; `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference scenes"

### Z2-R-ADVGFX-122 RT classification and scope decision

- **Statement:** Hardware ray tracing is delivered in Stage 7 through inline ray queries alone unless D-RT-SCOPE rules otherwise, with no Stage 0 to 6 dependency on it.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** the D-RT-SCOPE ruling entry in `Zenith2/Docs/DecisionLog.md` and its revalidation ticket
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Classification and gating"; `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Decision D-RT-SCOPE"

### Z2-R-ADVGFX-123 D-RT-SCOPE harness and ruling

- **Statement:** A harness under Zenith2/Benchmarks/RayTracing/ measures each RT feature's GPU time, memory and physical-accuracy gain, and a needs-human ruling ticket applies the acceptance rule, whose memory bound is absolute (512 MiB desktop, 128 MiB S24) and whose accuracy rule requires an error at most the fallback's on every scene that exercises the feature and strictly below on at least one.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_RayTracing` CTest smoke; the ruling's report under `Zenith2/Docs/Research/`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Decision D-RT-SCOPE"

### Z2-R-ADVGFX-124 RT capability query

- **Statement:** The RHI reports ray-query and acceleration-structure support in Zenith_RHIRayTracingCapabilities from the Vulkan extensions and the D3D12 raytracing tier, never adding them to the baseline profile.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_RTCapabilities_VertexFormatsReported`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Device capability"; `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "Zenith_RHIRayTracing header"

### Z2-R-ADVGFX-125 Acceleration-structure resources

- **Statement:** Acceleration structures are generational device handles with queried sizes, transient scratch, compaction of static builds and deferred retirement.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** RT unit tests on devices that report ray queries
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Acceleration-structure resources"

### Z2-R-ADVGFX-126 Acceleration-structure descriptor space

- **Statement:** Acceleration structures are bindless resources of FLUX_DESCRIPTOR_SPACE_ACCEL_STRUCTURE reached through 8-byte parameter fields.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-054, D-SLANG (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Binding_*` extended to the fifth space
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Descriptor index space for acceleration structures"

### Z2-R-ADVGFX-127 Acceleration-structure barriers are synthesised

- **Statement:** Build, build-read and trace-read accesses of acceleration structures are graph accesses whose barriers the graph synthesises.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** sync validation clean on every RT test
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Builds and barrier states"

### Z2-R-ADVGFX-128 Static bottom-level structures

- **Statement:** Each resident mesh LOD gets a bottom-level structure built from GEOM's pools in place with the quantisation box folded into the instance transform.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_RTBLAS_QuantisationFoldedIntoTransform`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Static meshes"

### Z2-R-ADVGFX-129 Skinned and procedural bottom-level structures

- **Statement:** Skinned instances refit their structures every frame their pose changes and rebuild every 64 frames, and procedural geometry rebuilds in the frame it regenerates.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_RTSkinned_RefitFollowsPose`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Skinned and procedural geometry"

### Z2-R-ADVGFX-130 Top-level structure from the GPU scene

- **Statement:** RTInstanceBuild writes one instance record per traced GPU-scene instance in compute and TLASBuild rebuilds the top-level structure every frame.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_RTInstanceBuild_MatchesGPUScene`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Top-level structure"

### Z2-R-ADVGFX-131 Ray queries with alpha-tested candidates

- **Statement:** Tracing passes use inline ray queries and resolve alpha-tested candidates through the per-material-slot alpha records, bounded by 64 candidates per ray.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_RTAlpha_CandidateSkipsBelowCutoff`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Ray queries in passes"

### Z2-R-ADVGFX-132 Hit shading

- **Statement:** Reflection and GI rays are shaded through a classify and per-variant resolve with EvaluateMaterial and EvaluateLightingEx over Flux_WorldLightSource.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** RT reflections against the tracer in the physical-accuracy suite
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Hit shading"

### Z2-R-ADVGFX-133 RT shadows

- **Statement:** RT shadows trace the sun and up to eight local shadowed lights and replace their shadow-map sample with a denoised visibility mask.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** physical-accuracy comparison with RT shadows on; RT on/off parity
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT shadows"

### Z2-R-ADVGFX-134 RT reflections

- **Statement:** RT reflections trace half-resolution GGX rays for surfaces at or below the roughness cutoff and replace SSR and probe specular there.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** physical-accuracy comparison with RT reflections on; parity
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT reflections"

### Z2-R-ADVGFX-135 RT ambient occlusion

- **Statement:** RT ambient occlusion traces one cosine ray per pixel within LIGHT's GTAO radius in place of GTAO.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `AE_GTAO` with RT AO on
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT ambient occlusion"

### Z2-R-ADVGFX-136 RT diffuse GI

- **Statement:** RT diffuse GI traces one half-resolution bounce and supplies the indirect irradiance in place of SSGI.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `AE_SSGI` with RT GI on
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT diffuse GI"

### Z2-R-ADVGFX-137 RT capability-matrix rows and fallbacks

- **Statement:** Every RT feature has a capability-matrix row whose cells read pending D-RT-SCOPE until the ruling, runs only where the device, tier and view feature mask allow, and otherwise runs its tested raster fallback.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_MatrixMatchesData` with the RT rows; RT-off runs of every RT test scene
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT capability matrix rows and fallbacks"

### Z2-R-ADVGFX-138 RT validation

- **Statement:** RT passes run with synchronization validation clean, and the RT unit tests, physical-accuracy comparisons and parity of the validation table pass.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** the tests of "RT validation"
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT validation"

### Z2-R-ADVGFX-139 RT budgets and device runs

- **Statement:** The RT rows of the pending table are allocated by the Stage 7 budget ticket for ADVGFX and measured by needs-human RTX 3070 runs (and S24 runs when admitted).
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the Stage 7 RT budget run records
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT budgets"; `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "RT validation"

### Z2-R-ADVGFX-140 Hardware traversal for the path tracer

- **Statement:** If flagged proposal ADVGFX-1 is approved (pending Q100), the tracer may traverse with ray queries over the same triangles where the device reports them, falls back to compute otherwise, never uses hardware traversal for acceptance, and matches the compute traversal statistically; otherwise this requirement is withdrawn.
- **Covers:** Z2-I-ADVGFX-01, Z2-I-ADVGFX-05
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7), Z2-D-037
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_HardwareMatchesComputeTraversal`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Compute and hardware traversal"

### Z2-R-ADVGFX-141 VG classification and paths

- **Statement:** Virtualized geometry is delivered in Stage 7, drawn as clusters by the visibility-buffer path alone, with every other path and the S24 drawing the fallback LOD chain.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7), Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** VG parity against the fallback in the visibility-buffer path parity
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG classification and gating"; `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG path integration"

### Z2-R-ADVGFX-142 VG asset kind and format

- **Statement:** A virtualized mesh cooks to a .zvgeo of the requested kind with the INFO, CLUSTERS, GROUPS, HIERARCHY and PAGES sections.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7), Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** VG cook tests
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG cooked format"

### Z2-R-ADVGFX-143 Cluster DAG cook

- **Statement:** The cook builds 128-triangle clusters, groups them, simplifies each group with locked boundaries and records monotonic errors, deterministically.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualGeometry_ErrorMonotonic`; `Test_CookVirtualGeometry_BoundariesLocked`; `Test_CookVirtualGeometry_Deterministic`; `Test_CookVirtualGeometry_RootFitsRootPages`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "Cluster DAG cook"

### Z2-R-ADVGFX-144 Cluster page streaming

- **Statement:** Cluster pages stream from culling feedback with pinned roots, LRU eviction with a grace window, refinement only when every child page is resident, and cancellation that leaves nothing behind.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualGeometry_LoadUnload1000Cycles`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "Cluster page streaming"

### Z2-R-ADVGFX-145 Crack-free LOD selection

- **Statement:** A cluster is drawn when its projected error is at most the threshold and its parent's exceeds it, with group-shared errors so neighbours agree.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualGeometry_NoCracksAcrossLevels`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "LOD selection"

### Z2-R-ADVGFX-146 VG culling

- **Statement:** VG culling traverses the group hierarchy and tests clusters two-phase against HiZ with frustum and cone tests, appending survivors to the visible-meshlet list.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7), Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** culling tests with VG instances; a fully occluded VG instance draws no cluster
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG culling"

### Z2-R-ADVGFX-147 VG rasterisation routes

- **Statement:** VG clusters rasterise through the hardware route, and through the software route only if D-VGEO-RASTER adopts it, into the visibility buffer with the existing ID format.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** the VG parity and crack tests on both routes
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG rasterisation"

### Z2-R-ADVGFX-148 D-VGEO-RASTER harness and ruling

- **Statement:** A harness under Zenith2/Benchmarks/VirtualGeometry/ measures the candidates and a needs-human ruling ticket applies the acceptance rule, whose equivalence criterion is image-space parity (FLIP mean within 0.01 of candidate 1 and no interior hole), not identical visibility IDs.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7), Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_VirtualGeometry` CTest smoke; the ruling report
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "Decision D-VGEO-RASTER"

### Z2-R-ADVGFX-149 VG validation

- **Statement:** The VG cook, crack, parity and streaming tests of the validation table pass on WARP and lavapipe.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7), Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** the tests of "VG validation"
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG validation"

### Z2-R-ADVGFX-150 VG budgets and device runs

- **Statement:** The VG rows of the pending table are allocated by the Stage 7 budget ticket for ADVGFX and measured by needs-human RTX 3070 runs.
- **Covers:** Z2-I-ADVGFX-02
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-VGEO-RASTER (open, Stage 7), Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the Stage 7 VG budget run records
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG budgets"; `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "VG validation"

### Z2-R-ADVGFX-151 Hair and groom excluded

- **Statement:** Strand hair and groom rendering are excluded: no ADVGFX requirement, ticket or code implements them, and hair cards are CHAR's classified alternative.
- **Covers:** Z2-I-ADVGFX-04
- **Stage:** S4
- **Classification:** excluded
- **Decisions:** Z2-D-043, Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** the rationale of "Hair and groom" and Z2-D-043, Z2-D-078
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Hair and groom"

### Z2-R-ADVGFX-152 No Luau surface

- **Statement:** ADVGFX registers no Luau binding and raises no misuse code, so it reserves no misuse block.
- **Covers:** Z2-I-ADVGFX-03, Z2-I-ADVGFX-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** review: no ADVGFX file registers a binding or raises a misuse code
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Luau errors and asserts"

### Z2-R-ADVGFX-153 Presentation-only virtual texturing

- **Statement:** No simulation-visible value depends on virtual texture residency, feedback or memory pressure.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** review: the VT system reads only the render world and writes only GPU resources; `Test_MemoryGovernor_ShedsNoSimulationState` with VT enabled
- **Design:** `Zenith2/Docs/Design/ADVGFX.md` → "Cancellation and shutdown"

### Z2-R-ADVGFX-154 Eviction epochs are assigned per eviction

- **Statement:** Each eviction takes the next value of a 64-bit epoch counter that starts at 1 and never repeats while the renderer lives, and the evicted page is DRAINING with that epoch.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_SplitUnmapRowsDrainByEpoch`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Eviction epochs"

### Z2-R-ADVGFX-155 Unloaded pages drain at the first submitted frame after the unload drain

- **Statement:** The pages of an unloaded virtual texture become FREE at EndFrame(true) of the first submitted frame after the step-3c drain that processed its unload, and not before, because GEOM has already rebound its materials to the usage fallback.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_UnloadDrainsAtFirstSubmittedFrame`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical page state machine"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"

### Z2-R-ADVGFX-156 Unbound pools fall to the tail

- **Statement:** Flux_SampleMaterial enters the virtual-texture path only when the handle carries the VT tag and the invocation's binding has the pools-bound flag, and otherwise samples word 0 as an ordinary texture, which for a VT handle is its tail texture.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_UnboundPoolsFallToTail`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Flux_SampleVirtual algorithm"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Feedback writers"

### Z2-R-ADVGFX-157 Invocation state visible to the VirtualTexture module alone

- **Statement:** The stored binding, the writing flag and the request reservoir are declared internal to the Slang module VirtualTexture, so a module that names any of them fails with the visibility diagnostic while the same reference inside the module compiles; other modules reach them only through the public functions.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTextureShader_ReservoirNotNameableFromModules` (`ZenithShaderCompiler_Tests`)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material input binding"; `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` → "VirtualTexture shader module"

### Z2-R-ADVGFX-158 VT_TILE upload floor

- **Statement:** With GEOM's floor applied (row AV19), the VT_TILE class receives at least ceil(Z2-B-ADVGFX-002 / 4) tiles per frame whatever the other streaming classes take, and a frame in which it receives fewer than the smaller of that floor and the decoded tiles waiting is counted in Flux.VT.UploadStarvedFrames.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** Z2-B-ADVGFX-002
- **Acceptance evidence:** `Test_VirtualTexture_UploadStarvedCounted`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Tile upload"

### Z2-R-ADVGFX-159 VT finest-mip latency

- **Statement:** The p95 number of frames from the first request of a mip-0 page whose finest resident ancestor is the root to the first frame that samples mip 0 is reported as Flux.VT.FinestMipFramesP95 and measured against the pending finest-mip latency row, whose value lies above the structural floor of ceil(gap / 2) × (2F + 1) frames.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the RTX 3070 and S24 VT budget runs against the pending row "Virtual texturing finest-mip latency" (X422)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Request latency floor"; `Zenith2/Docs/Design/ADVGFX.md` → "Pending row needs"

### Z2-R-ADVGFX-160 Handle second word readable under D-SLANG

- **Statement:** Flux_HandleWords reads both words of a material-constants handle bit-exactly in pixel and compute stages on the four probe targets under the lowering D-SLANG picks, proven by the probe case RHI and RPROD add (row AV18), and every VT ticket that uses it is decision-pending until that ruling lands.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, D-SLANG (open, Stage 1), Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_BindlessProbe_HandleWordsRoundTrip` (RHI's probe table, row AV18)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material handle encoding"

### Z2-R-ADVGFX-161 VT branch cost on the S24

- **Statement:** On the S24, ForwardPlusOpaque GPU time on a VT-free scene with render.vt.enabled true exceeds the time with it false by at most the pending row "VT branch cost in VT-free material passes (S24)".
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the A/B of the S24 VT run against the pending row (X422)
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Cost of the VT branch"; `Zenith2/Docs/Design/ADVGFX.md` → "Pending row needs"

### Z2-R-ADVGFX-162 Stale reference landing protocol

- **Statement:** A ticket whose change stales a reference marks each staled References.json entry with the pendingRecording key of a pre-filed needs-human recording ticket that it blocks, and the freshness test and that scene's gate comparison then skip with a recorded reason instead of failing.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicalAccuracy_PendingRecordingProtocol`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Stale reference landing protocol"

### Z2-R-ADVGFX-163 Acceptance refuses pending recordings

- **Statement:** Stage 5 acceptance and every acceptance-mode run fail while any References.json entry carries pendingRecording.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Test_PhysicalAccuracy_PendingRecordingProtocol`; the Stage 5 acceptance suite reports
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Stale reference landing protocol"

### Z2-R-ADVGFX-164 Reference size budget and LFS tracking

- **Statement:** A full recording of both backends is at most 16 MiB stored, refused by the recording harness and by the freshness test otherwise, and every .zref is LFS-tracked through one .gitattributes line so the references satisfy [Z2-N-BUDGET-177] whatever Z2-B-CONTENT-001 is.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-CONTENT-001
- **Acceptance evidence:** `Test_PTReference_RecordingSetSizeRefused`; `Test_PhysicalAccuracy_ReferencesFresh` (stored bytes); review of the recording ticket's `.gitattributes` line
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference size budget"

### Z2-R-ADVGFX-165 VT host memory bound

- **Statement:** Tile indices and mirrors of active virtual textures are charged 44 bytes per tile against render.vt.maxIndexBytes, an activation past it fails with FLUX_VT_ERROR_HOST_BYTES_FULL and changes nothing else, and EnableVirtualTexturing refuses a configuration whose host worst case exceeds the proposed host row.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_HostCapRefusesActivation`; `Test_VirtualTexture_BudgetRefusesOversizedPools`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Host memory bound"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Memory accounting and pressure"

### Z2-R-ADVGFX-166 Downsampled reference standard error

- **Statement:** A gate trace compared with a 2 × 2 box-downsampled reference uses the downsampled mean and SE_down = sqrt(Σ SE²) / 4 in its z test.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** Z2-B-ADVGFX-007
- **Acceptance evidence:** `Test_PTMetrics_DownsampledReferenceSE`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Gate and acceptance runs"

### Z2-R-ADVGFX-167 Tracer execution substrate

- **Statement:** Every tracer batch runs as one call of RPROD's Flux_Renderer::RunToolGraph from MAIN_THREAD between render frames, VT settlement is pumped by SettleToolFrames before the first batch, the tracer's materials hold tool material slots acquired outside every frame and released with the tracer, and any tool-graph error ends the run with a mapped Flux_PathTraceError.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_PathTracer_BatchRunsOutsideFrameLoop`; `Test_PathTracer_VirtualTextureFullMode`
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Execution substrate"

### Z2-R-ADVGFX-168 Synthetic block-grid cook test path

- **Statement:** Zenith_VTCookTestAccess::CookFromBlockGrid cooks a virtual texture from a deterministic pre-encoded block grid through tile assembly and compression alone, after the shape checks, giving a file that passes every reader row.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_CookVirtualTexture_SyntheticBlockGridPath`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Synthetic block-grid test path"

### Z2-R-ADVGFX-169 Test-time VT fixtures fit the test timeout

- **Statement:** The real block encoder cooks only the small fixtures (at most 2,048 × 256 and 512 × 512), the residency fixtures use the synthetic path at most 4,096 × 4,096, the fixture set is declared as ADVGFX_VTFixtures / AdvgfxVTFixtures / ZENITH2_ADVGFX_VT_FIXTURES with consuming suite z2-advgfx-cooked, every fixture setup and test case finishes within the 60-second CTest timeout, and only the 16,384 × 16,384 shape is confined to Zenith_Bench_VirtualTexture.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** the `ADVGFX_VTFixtures` setup entry (`Test_VirtualTexture_CookFixtures`) and the `z2-advgfx-cooked` entries passing under the 60-second CTest timeout in the `Test` step ([Z2-N-CONTRACT-409])
- **Design:** `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Fixtures"; `Zenith2/Docs/Design/ADVGFX/Tests.md` → "Test targets and gate steps"

### Z2-R-ADVGFX-170 Physical-accuracy metrics stay in a test target

- **Statement:** The image metrics and the runner are compiled into ZenithFlux_Tests alone, the one target that links the flip header, and no library includes flip.
- **Covers:** Z2-I-ADVGFX-05
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-037, Z2-D-043, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` and `ForbiddenCheck` (third-party headers); review of the test-support directory
- **Design:** `Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Library targets and programs"

### Z2-R-ADVGFX-171 Full-precision positions in bottom-level structures

- **Statement:** A mesh with FLOAT32x3 positions gets a bottom-level structure built with the R32G32B32_SFLOAT vertex format and the identity quantisation box.
- **Covers:** Z2-I-ADVGFX-01
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-043, D-RT-SCOPE (open, Stage 7)
- **Budgets:** none
- **Acceptance evidence:** `Test_RTBLAS_Float32PositionsIdentityBox`
- **Design:** `Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Static meshes"

### Z2-R-ADVGFX-172 Dirty rows carry the minimum unapplied epoch and cut in epoch order

- **Statement:** A dirty row carries the minimum epoch of the unmaps that changed it since it was last applied, and the cut takes rows carrying an epoch first in ascending epoch order, then every other dirty row in (slot, mip, row) order, each row whole or not at all, stopping at the first row that does not fit under the record cap.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_PageTableRecordCapDefers`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Eviction epochs"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Dirty rows and run records"

### Z2-R-ADVGFX-173 Applied epoch publication and page exit

- **Statement:** At EndFrame(true) the system publishes the applied epoch as one less than the minimum epoch over rows still dirty (or the last epoch taken when none carries one), never decreasing it, and an evicted page becomes FREE exactly when its epoch is at most the published value.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_SplitUnmapRowsDrainByEpoch`; `Test_VirtualTexture_PageTableRecordCapDefers`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Eviction epochs"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Draining rule"

### Z2-R-ADVGFX-174 Non-writing invocations record nothing

- **Statement:** An invocation that is not its feedback block's writer this frame, or whose binding does not record requests, returns from Flux_VTRecordRequest at once and writes no feedback.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualTexture_NonWritingInvocationRecordsNothing`; `Test_VirtualTexture_FeedbackWrittenOncePerBlock`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Pixel selection and reservoir"

### Z2-R-ADVGFX-175 Tail textures inside the device row

- **Statement:** The tail textures of active virtual textures are charged at activation to render.vt.maxTailBytes, a reservation inside the Z2-B-ADVGFX-001 device total, and an activation that would pass it fails with FLUX_VT_ERROR_TAIL_BYTES_FULL and changes nothing else.
- **Covers:** Z2-I-ADVGFX-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-043, Z2-D-012
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_VirtualTexture_TailReservationRefusesActivation`
- **Design:** `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Physical cache sizing"; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Activation steps"

## Coverage of inventory items

| Inventory item | Classification | Stage | Requirements |
|---|---|---|---|
| Z2-I-ADVGFX-01 RT | deferred | S7 | Z2-R-ADVGFX-122, Z2-R-ADVGFX-123, Z2-R-ADVGFX-124, Z2-R-ADVGFX-125, Z2-R-ADVGFX-126, Z2-R-ADVGFX-127, Z2-R-ADVGFX-128, Z2-R-ADVGFX-129, Z2-R-ADVGFX-130, Z2-R-ADVGFX-131, Z2-R-ADVGFX-132, Z2-R-ADVGFX-133, Z2-R-ADVGFX-134, Z2-R-ADVGFX-135, Z2-R-ADVGFX-136, Z2-R-ADVGFX-137, Z2-R-ADVGFX-138, Z2-R-ADVGFX-139, Z2-R-ADVGFX-140, Z2-R-ADVGFX-171 |
| Z2-I-ADVGFX-02 virtualized geometry | deferred | S7 | Z2-R-ADVGFX-141, Z2-R-ADVGFX-142, Z2-R-ADVGFX-143, Z2-R-ADVGFX-144, Z2-R-ADVGFX-145, Z2-R-ADVGFX-146, Z2-R-ADVGFX-147, Z2-R-ADVGFX-148, Z2-R-ADVGFX-149, Z2-R-ADVGFX-150 |
| Z2-I-ADVGFX-03 virtual texturing | required | S4 | Z2-R-ADVGFX-001, Z2-R-ADVGFX-002, Z2-R-ADVGFX-003, Z2-R-ADVGFX-004, Z2-R-ADVGFX-005, Z2-R-ADVGFX-006, Z2-R-ADVGFX-007, Z2-R-ADVGFX-008, Z2-R-ADVGFX-009, Z2-R-ADVGFX-010, Z2-R-ADVGFX-011, Z2-R-ADVGFX-012, Z2-R-ADVGFX-013, Z2-R-ADVGFX-014, Z2-R-ADVGFX-015, Z2-R-ADVGFX-016, Z2-R-ADVGFX-017, Z2-R-ADVGFX-018, Z2-R-ADVGFX-019, Z2-R-ADVGFX-020, Z2-R-ADVGFX-021, Z2-R-ADVGFX-022, Z2-R-ADVGFX-023, Z2-R-ADVGFX-024, Z2-R-ADVGFX-025, Z2-R-ADVGFX-026, Z2-R-ADVGFX-027, Z2-R-ADVGFX-028, Z2-R-ADVGFX-029, Z2-R-ADVGFX-030, Z2-R-ADVGFX-031, Z2-R-ADVGFX-032, Z2-R-ADVGFX-033, Z2-R-ADVGFX-034, Z2-R-ADVGFX-035, Z2-R-ADVGFX-036, Z2-R-ADVGFX-037, Z2-R-ADVGFX-038, Z2-R-ADVGFX-039, Z2-R-ADVGFX-040, Z2-R-ADVGFX-041, Z2-R-ADVGFX-042, Z2-R-ADVGFX-043, Z2-R-ADVGFX-044, Z2-R-ADVGFX-045, Z2-R-ADVGFX-046, Z2-R-ADVGFX-047, Z2-R-ADVGFX-048, Z2-R-ADVGFX-049, Z2-R-ADVGFX-050, Z2-R-ADVGFX-051, Z2-R-ADVGFX-052, Z2-R-ADVGFX-053, Z2-R-ADVGFX-054, Z2-R-ADVGFX-055, Z2-R-ADVGFX-056, Z2-R-ADVGFX-057, Z2-R-ADVGFX-058, Z2-R-ADVGFX-059, Z2-R-ADVGFX-060, Z2-R-ADVGFX-061, Z2-R-ADVGFX-062, Z2-R-ADVGFX-063, Z2-R-ADVGFX-064, Z2-R-ADVGFX-065, Z2-R-ADVGFX-066, Z2-R-ADVGFX-067, Z2-R-ADVGFX-068, Z2-R-ADVGFX-069, Z2-R-ADVGFX-070, Z2-R-ADVGFX-071, Z2-R-ADVGFX-072, Z2-R-ADVGFX-073, Z2-R-ADVGFX-074, Z2-R-ADVGFX-075, Z2-R-ADVGFX-076, Z2-R-ADVGFX-077, Z2-R-ADVGFX-078, Z2-R-ADVGFX-079, Z2-R-ADVGFX-080, Z2-R-ADVGFX-081, Z2-R-ADVGFX-082, Z2-R-ADVGFX-083, Z2-R-ADVGFX-084, Z2-R-ADVGFX-085, Z2-R-ADVGFX-097, Z2-R-ADVGFX-152, Z2-R-ADVGFX-153, Z2-R-ADVGFX-154, Z2-R-ADVGFX-155, Z2-R-ADVGFX-156, Z2-R-ADVGFX-157, Z2-R-ADVGFX-158, Z2-R-ADVGFX-159, Z2-R-ADVGFX-160, Z2-R-ADVGFX-161, Z2-R-ADVGFX-165, Z2-R-ADVGFX-168, Z2-R-ADVGFX-169, Z2-R-ADVGFX-172, Z2-R-ADVGFX-173, Z2-R-ADVGFX-174, Z2-R-ADVGFX-175 |
| Z2-I-ADVGFX-04 hair/groom | excluded | — | Z2-R-ADVGFX-151 |
| Z2-I-ADVGFX-05 path-traced reference | required | S5 | Z2-R-ADVGFX-086, Z2-R-ADVGFX-087, Z2-R-ADVGFX-088, Z2-R-ADVGFX-089, Z2-R-ADVGFX-090, Z2-R-ADVGFX-091, Z2-R-ADVGFX-092, Z2-R-ADVGFX-093, Z2-R-ADVGFX-094, Z2-R-ADVGFX-095, Z2-R-ADVGFX-096, Z2-R-ADVGFX-097, Z2-R-ADVGFX-098, Z2-R-ADVGFX-099, Z2-R-ADVGFX-100, Z2-R-ADVGFX-101, Z2-R-ADVGFX-102, Z2-R-ADVGFX-103, Z2-R-ADVGFX-104, Z2-R-ADVGFX-105, Z2-R-ADVGFX-106, Z2-R-ADVGFX-107, Z2-R-ADVGFX-108, Z2-R-ADVGFX-109, Z2-R-ADVGFX-110, Z2-R-ADVGFX-111, Z2-R-ADVGFX-112, Z2-R-ADVGFX-113, Z2-R-ADVGFX-114, Z2-R-ADVGFX-115, Z2-R-ADVGFX-116, Z2-R-ADVGFX-117, Z2-R-ADVGFX-118, Z2-R-ADVGFX-119, Z2-R-ADVGFX-120, Z2-R-ADVGFX-121, Z2-R-ADVGFX-140, Z2-R-ADVGFX-152, Z2-R-ADVGFX-162, Z2-R-ADVGFX-163, Z2-R-ADVGFX-164, Z2-R-ADVGFX-166, Z2-R-ADVGFX-167, Z2-R-ADVGFX-170 |

## Excluded and deferred

| Capability | Classification | Decision | Rationale |
|---|---|---|---|
| hair and groom (strand hair, grooms) | excluded (Z2-R-ADVGFX-151) | Z2-D-043, Z2-D-078 | the programme reaches shipping qualification without a hair pipeline; CHAR classifies hair cards as the required alternative and shells as excluded, needing only existing material features |
| hardware ray tracing | deferred to Stage 7 (Z2-R-ADVGFX-122, Z2-R-ADVGFX-123, Z2-R-ADVGFX-124, Z2-R-ADVGFX-125, Z2-R-ADVGFX-126, Z2-R-ADVGFX-127, Z2-R-ADVGFX-128, Z2-R-ADVGFX-129, Z2-R-ADVGFX-130, Z2-R-ADVGFX-131, Z2-R-ADVGFX-132, Z2-R-ADVGFX-133, Z2-R-ADVGFX-134, Z2-R-ADVGFX-135, Z2-R-ADVGFX-136, Z2-R-ADVGFX-137, Z2-R-ADVGFX-138, Z2-R-ADVGFX-139, Z2-R-ADVGFX-140, Z2-R-ADVGFX-171) | Z2-D-043; D-RT-SCOPE (open, Stage 7) | full requirement tree specified now; leaf tickets sit in the Stage 7 sprint; the scope decision narrows the feature set |
| virtualized geometry | deferred to Stage 7 (Z2-R-ADVGFX-141, Z2-R-ADVGFX-142, Z2-R-ADVGFX-143, Z2-R-ADVGFX-144, Z2-R-ADVGFX-145, Z2-R-ADVGFX-146, Z2-R-ADVGFX-147, Z2-R-ADVGFX-148, Z2-R-ADVGFX-149, Z2-R-ADVGFX-150) | Z2-D-043; D-VGEO-RASTER (open, Stage 7) | full requirement tree specified now; leaf tickets sit in the Stage 7 sprint |
| the hardware traversal option of the path tracer | deferred to Stage 7, conditional on flagged proposal ADVGFX-1 | Z2-D-037, Z2-D-043 | Z2-D-037 fixes the reference as compute; the option is an accelerator only and lands only if the human approves the proposal |
| sparse (reserved) resources for virtual texturing | not used | Z2-D-013, Z2-D-043 | absent from the baseline profiles and from WARP; software page tables cover every device |
| virtual textures in transparent and decal materials | not supported | Z2-D-043 | those passes write no feedback and bind no VT pool; their textures stay streamed textures of GEOM, and a VT handle reaching them samples its tail |
