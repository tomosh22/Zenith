# ADVGFX design: virtual texturing runtime

Section file of `Zenith2/Docs/Design/ADVGFX.md`. It specifies the Stage 4 virtual texturing (VT) runtime: the physical cache, the page-table pool, the virtual texture records, the material handle encoding, sampling, feedback, residency and eviction, tile I/O and upload, page-table updates, activation, the frame-level flow, failure behaviour and the pass tables. The cooked format and the cook are `Zenith2/Docs/Design/ADVGFX/VTCook.md`; the public declarations are `Zenith2/Docs/Design/ADVGFX/PublicAPI.md`; the named tests are `Zenith2/Docs/Design/ADVGFX/Tests.md`. The specification this file elaborates is `Zenith2/Docs/Rendering.md → "GPU scene tables and pools"` (the VT physical and page-table pool of [Z2-N-REND-533] and [Z2-N-REND-551]), `Zenith2/Docs/Rendering/GpuResources.md → "Pools"` and `Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary"`. Every obligation here is traceable to a `Z2-R-ADVGFX-NNN` requirement of `Zenith2/Docs/Requirements/ADVGFX.md`; elaborations are marked `<!-- elaboration -->`, and assumptions about other areas are fenced `<!-- assumption: pending <AREA> -->` with their row (AV1 … AV19; AV16 is withdrawn) in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| the model, the terms and every constant | "Virtual texturing model" |
| which textures may be virtual | "Virtual texture shape rules" |
| GPU memory | "Physical cache", "Page-table pool", "Virtual texture records" |
| how a material names a virtual texture | "Material handle encoding" |
| shader sampling | "Sampling" |
| what the GPU asks for | "Feedback", "Feedback processing" |
| what stays resident | "Residency and eviction" |
| getting a tile onto the GPU | "Tile I/O and decode", "Tile upload", "Page-table updates" |
| per-frame order | "Frame-level flow" |
| asset lifetime | "Activation and unload" |
| failure paths | "Camera cuts", "Deferred and failed frames", "Memory accounting and pressure", "Device loss recovery", "Disabled virtual texturing" |
| the S24 | "Mobile parameters" |
| the passes VT adds | "Pass tables" |

## Virtual texturing model

<!-- elaboration -->
Zenith2's VT is software virtual texturing with indirection page tables, in the family of id Tech 5's and UE's runtime VT: a large texture is cut at cook time into fixed-size tiles with borders, the GPU samples it through a page table that maps each virtual page to a page of one shared physical cache, the material passes record which pages they wanted (feedback), and the CPU streams the wanted tiles in and evicts the least recently wanted ones. It uses no sparse (reserved, tiled) resources, because neither `VP_ZENITH2_baseline` nor the D3D12 baseline of `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"` requires them and neither WARP nor every Adreno driver exposes them; page tables in an ordinary texture work on every device the baseline admits.

### Terms

| Term | Meaning |
|---|---|
| virtual texture (VT) | one asset of kind `ASSET_KIND_VIRTUAL_TEXTURE` (code 18, `Flux.VirtualTexture`, `.zvt`), one logical 2D texture of extent W × H texels at mip 0 |
| tile | the cooked unit: the texels of one 128 × 128 square of one VT mip plus a 4-texel border on every side, 136 × 136 texels, block-compressed |
| page | a virtual page (slot s, page mip m, page x, page y) of a VT, or a physical page (a 136 × 136 slot of the physical cache); a resident virtual page occupies exactly one physical page |
| page mip | a VT mip m whose extent is at least 128 texels on its shorter axis: m = 0 … M − 1 |
| root mip | page mip M − 1, whose shorter axis is exactly one page; its pages (1 to 8) are the root pages, pinned while the VT is active |
| tail | the mips m ≥ M (shorter axis below 128 texels), cooked as one ordinary small texture that is always resident |
| slot | the VT's index 0 … 4,095 in the record buffer, assigned at activation |
| region | the square, power-of-two-sized, aligned rectangle of the page-table pool that holds the VT's page table at every page mip |
| entry | one R32_UINT texel of the page-table pool: the physical page that serves a virtual page (the page itself when resident, else its finest resident ancestor) |
| request | one feedback value: (slot, page mip, page x, page y) |
| touch | marking a resident page as wanted in the current feedback frame |

### Constants

`Zenith2/Engine/ZenithFlux/Flux_VirtualTexture.h` declares these, and `Zenith2/Shaders/Common/VirtualTexture.slang` mirrors the shader-visible ones through `[FluxShared]` constants (`Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`):

| Constant | Value | Meaning |
|---|---|---|
| `uFLUX_VT_TILE_TEXELS` | 128 | interior texels per tile axis |
| `uFLUX_VT_BORDER_TEXELS` | 4 | border texels on each side of a tile |
| `uFLUX_VT_PAGE_TEXELS` | 136 | `128 + 2 × 4`; a multiple of the 4 × 4 block of BC7 and ASTC 4 × 4 |
| `uFLUX_VT_PAGE_BYTES` | 18,496 | `(136 / 4)² × 16`: one block-compressed tile |
| `uFLUX_VT_LAYER_PAGES_PER_AXIS` | 30 | physical pages per layer axis |
| `uFLUX_VT_LAYER_TEXELS` | 4,080 | `30 × 136` |
| `uFLUX_VT_PAGES_PER_LAYER` | 900 | `30²` |
| `ulFLUX_VT_LAYER_BYTES` | 16,646,400 | `4,080²` bytes (one byte per texel for both block formats) |
| `uFLUX_VT_MAX_LAYERS` | 64 | physical layers at most (6 bits of the entry) |
| `uFLUX_VT_MIN_EXTENT_LOG2` | 8 | 256 texels: the smallest VT axis |
| `uFLUX_VT_MAX_EXTENT_LOG2` | 14 | 16,384 texels: the largest VT axis, GEOM's and CONTENT's image limit (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`) |
| `uFLUX_VT_MAX_ASPECT_LOG2` | 3 | an aspect ratio of at most 8 : 1 |
| `uFLUX_VT_MAX_PAGE_MIPS` | 8 | M for a 16,384-texel shorter axis: `14 − 6` |
| `uFLUX_VT_MAX_TAIL_MIPS` | 10 | tail mips for a 16,384 × 2,048 VT |
| `uFLUX_VT_MAX_TILES` | 21,845 | tiles of a 16,384 × 16,384 VT, the most any VT has |
| `uFLUX_VT_MAX_REGION_SIDE` | 128 | page-table region side of a VT whose longer axis is 16,384 |
| `uFLUX_VT_MAX_SLOTS` | 4,096 | active VTs at most (12 bits of a request) |
| `uFLUX_VT_RECORD_BYTES` | 32 | one `Flux_VTRecord` |
| `uFLUX_VT_FEEDBACK_NONE` | `0xFFFFFFFF` | "no request" in a feedback buffer |
| `uFLUX_VT_REFINE_STEP` | 2 | page mips a load may skip below the resident ancestor |
| `uFLUX_VT_EVICT_GRACE_FRAMES` | 8 | a page touched within the last 8 feedback frames is never evicted |
| `uFLUX_VT_FEEDBACK_RING` | 4 | feedback readbacks in flight per view |
| `uFLUX_VT_MAX_FEEDBACK_VIEWS` | 8 | views that write feedback in one frame |
| `uFLUX_VT_MAX_PAGE_TABLE_RECORDS` | 65,536 | page-table run records per frame (with the 16 mip ranges, 1,048,832 bytes of frame data) |
| `uFLUX_VT_SLOT_QUARANTINE_FRAMES` | 8 | frames a freed slot waits before reuse (frames in flight 3 + feedback ring 4 + 1) |

### Why software page tables

<!-- informative -->
The alternatives were sparse resources (`VK_EXT`/core sparse residency and D3D12 reserved resources), which give hardware page tables and hardware filtering across pages but are absent from the baseline profiles, slow to map on several drivers and unavailable on WARP; and texture arrays of whole mips per texture, which is GEOM's streaming and does not scale to 16 K textures. Software page tables cost one dependent texel load and a manual trilinear blend per sample, which the budget row `Z2-B-ADVGFX-003` and the material passes' rows absorb.
<!-- /informative -->

## Virtual texture shape rules

A texture may be cooked as virtual when every rule below holds; the cook refuses any other with the error named ("Cook errors" of `Zenith2/Docs/Design/ADVGFX/VTCook.md`).

| Rule | Value | Error |
|---|---|---|
| usage | `FLUX_TEXTURE_USAGE_COLOUR`, `_LINEAR`, `_NORMAL` or `_MASK` (GEOM's usage classes, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`) | `ADVGFX_COOK_ERROR_VT_USAGE` |
| extent | W = 2^w and H = 2^h with 8 ≤ w, h ≤ 14 | `ADVGFX_COOK_ERROR_VT_EXTENT` |
| aspect | `|w − h| ≤ 3` | `ADVGFX_COOK_ERROR_VT_EXTENT` |
| layers | 1 | `ADVGFX_COOK_ERROR_VT_EXTENT` |
| domain of the materials that bind it | `opaque` or `alphaTest` only; a `transparent` or `decal` material that binds a VT fails its own cook (assumption pending GEOM, row AV3) | GEOM's `GEOM_COOK_ERROR_MATERIAL_TEXTURE_USAGE` |

Derived quantities, for a VT of log2 extents w and h:

| Quantity | Formula | Example 4,096 × 4,096 | Example 16,384 × 2,048 |
|---|---|---|---|
| page mips M | `min(w, h) − 6` | 6 | 5 |
| pages at mip m | `(2^(w − 7 − m)) × (2^(h − 7 − m))` | 32 × 32 at m = 0 | 128 × 16 at m = 0 |
| root pages | `2^(|w − h|)` | 1 | 8 |
| tiles | `Σ_{m<M} pagesX(m) × pagesY(m)` | 1,365 | 2,728 |
| region side S | `2^(max(w, h) − 7)` | 32 | 128 |
| tail mip 0 extent | `2^(w − M) × 2^(h − M)` | 64 × 64 | 512 × 64 |
| tail mips | `max(w, h) − M + 1` | 7 | 10 |

<!-- informative -->
Restricting VTs to power-of-two extents with a bounded aspect makes every page mip a whole number of pages on both axes, so no page is partially filled, no UV is rescaled inside a page and the region is a quadtree; the content that needs other shapes (decals, UI, sprites) is GEOM's streamed or resident textures.
<!-- /informative -->

## Physical cache

### Physical cache texture

One physical cache texture serves every VT. It is a 2D array texture created by `Flux_VirtualTextureSystem` through `Zenith_RHIDevice::CreateTexture` (`Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHIResources header"`):

| `Zenith_RHITextureDesc` member | Windows | Android |
|---|---|---|
| `m_szName` | `"Flux.VT.Physical"` | same |
| `m_eDimension` | 2D array | same |
| `m_eFormat` | `ZENITH_RHI_FORMAT_BC7_UNORM_SRGB` | `ZENITH_RHI_FORMAT_ASTC_4X4_UNORM_SRGB` |
| second view format | `ZENITH_RHI_FORMAT_BC7_UNORM` | `ZENITH_RHI_FORMAT_ASTC_4X4_UNORM` |
| `m_xExtent` | 4,080 × 4,080 | same |
| `m_uDepthOrLayers` | `render.vt.physicalLayers` | same |
| `m_uMipCount` | 1 | same |
| `m_uUsage` | `SAMPLED | TRANSFER_DST | MUTABLE_FORMAT | CONCURRENT_QUEUES` | same |
| `m_uQueueMask` | graphics, async compute, copy | same |

<!-- assumption: pending RHI -->
A mutable-format image whose two formats are both block-compressed and both sampled is new to `Zenith_RHITextureDesc`: today `m_eStorageViewFormat` names "the second view format of a mutable-format image" for GB0's storage view. ADVGFX assumes the field (or a renamed `m_eSecondViewFormat`) also admits a second *sampled* view whose format is in the same compatibility class as `m_eFormat` (BC7 UNORM and sRGB; ASTC 4 × 4 UNORM and sRGB), created with `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT` and a two-entry `VkImageFormatListCreateInfo` on Vulkan and a `_TYPELESS` resource with two SRV formats on D3D12 (rows AV4 (a)).
<!-- /assumption -->

The texture is imported into every frame graph as `Flux_FrameShared::m_xVTPhysical` ([Z2-N-REND-605]) and marked with `MarkOutput` on frames whose `StreamingUpload` writes it ([Z2-N-REND-295]). Two SRV views of it are written once at creation: `m_xPhysicalSRGBView` (colour VTs) and `m_xPhysicalLinearView` (linear, normal and mask VTs). A pass reads it through two parameter-struct fields declared on `m_xVTPhysical` with the two view formats ("Material input binding").

### Physical page addressing

Physical page index p ∈ [0, layers × 900):

| Field | Formula |
|---|---|
| layer | `p / 900` |
| page y | `(p % 900) / 30` |
| page x | `p % 30` |
| texel origin of the page | `(x × 136, y × 136)` in that layer |
| texel origin of the tile interior | `(x × 136 + 4, y × 136 + 4)` |

### Physical cache sizing

The cache is sized once, at `Flux_Renderer::EnableVirtualTexturing`, and never grows or shrinks while the renderer lives.

<!-- elaboration -->
| Quantity | Desktop default | S24 default | Source |
|---|---|---|---|
| layers | 12 | 4 | `render.vt.physicalLayers` |
| physical pages | 10,800 | 3,600 | layers × 900 |
| physical cache bytes | 199,756,800 (190.5 MiB) | 66,585,600 (63.5 MiB) | layers × `ulFLUX_VT_LAYER_BYTES` |
| page-table pool bytes | 22,369,620 (21.3 MiB, extent 2,048, 12 mips) | 5,592,404 (5.3 MiB, extent 1,024, 11 mips) | `4 × Σ extent² / 4^k` |
| record buffer bytes | 131,072 | 131,072 | 4,096 × 32 |
| fixed pools | 222,257,492 (211.96 MiB) | 72,309,076 (68.96 MiB) | the three above |
| tail texture reservation | 8,388,608 (8 MiB) | 2,097,152 (2 MiB) | `render.vt.maxTailBytes`: the cap on the tail textures of all active VTs |
| GPU total | 230,646,100 (219.96 MiB) | 74,406,228 (70.96 MiB) | the sum |
| proposed `Z2-B-ADVGFX-001` (device bytes only) | 224 MiB | 72 MiB | pending; the Stage 1 per-system budget ticket for ADVGFX adopts or replaces it |

`Z2-B-ADVGFX-001` counts device bytes only: it is a member of `MEM-WIN-VRAM` (`Zenith2/Docs/Budgets.md → "Allocation sums"`), and on the S24 the GPU total above is what MEM-S24 counts for it. The tail textures are persistent read-only textures (`Flux_PersistentResources::CreateTexture`) created one per active VT, at most 43,792 bytes each; their bytes are charged to the tail reservation at activation and returned at unload, so they count against `Z2-B-ADVGFX-001` and against no GEOM row. Without the cap they would be bounded only by the slot count (4,096 slots × 43,792 bytes = 179,372,032 bytes for 2,048 × 256 VTs, whose small tile charge the host cap does not limit); with it, the desktop reservation holds 191 tails of the largest size or 1,528 of a square VT, and the S24's 47 or 382. The host bytes of VT (tile indices, mirrors, LRU nodes, staging and the feedback sort arena) are a separate proposed row, "Virtual texturing host memory" ("Host memory bound").

`EnableVirtualTexturing` computes the GPU total from the configured layers, page-table extent and tail reservation and refuses with `FLUX_VT_ERROR_BUDGET_EXCEEDED` when it exceeds the adopted `Z2-B-ADVGFX-001` value for the platform (while the row is pending, the proposed value above is the bound), so a configuration can never allocate past its memory row.

## Page-table pool

### Page-table texture

| Property | Value |
|---|---|
| format | `ZENITH_RHI_FORMAT_R32_UINT` (sampled, storage write and typed load are granted by "Required format features" of `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`) |
| extent | `render.vt.pageTableExtent` × the same: 2,048 desktop, 1,024 S24 |
| mips | `log2(extent) + 1` (12 and 11) |
| usage | `SAMPLED | STORAGE | CONCURRENT_QUEUES` |
| graph residency | imported every frame as `Flux_FrameShared::m_xVTPageTable`; marked with `MarkOutput` on frames whose `VTPageTableUpdate` writes it |
| SRV | one view of every mip, read with `Load(int3(x, y, mip))`; never filtered |
| UAVs | one view per mip ([Z2-N-REND-051]), written by `VTPageTableUpdate` alone |

The page table is a storage texture, not a render target, so the 32-bit render-target rule ([Z2-N-REND-918]) does not apply to it.

### Page-table entry encoding

```
bits 0–4    physical page x       (0 … 29)
bits 5–9    physical page y       (0 … 29)
bits 10–15  physical layer        (0 … 63)
bits 16–19  resident page mip r   (0 … 15): the page mip of the physical page the entry names
bits 20–30  zero
bit  31     1: written. An entry of a region that is in use is always written.
```

- The entry of virtual page (m, x, y) names the page itself when it is resident (r = m), else its finest resident ancestor (r > m); the root pages are pinned, so every entry of an active VT names a page.
- `Flux_EncodeVTEntry(uint32 uPhysicalPage, uint32 uResidentMip)` and `Flux_DecodeVTEntry` (CPU, `Flux_VirtualTexture.h`) and `Flux_VTDecodeEntry` (Slang, `VirtualTexture.slang`) are the only encoders and decoders; `Test_VirtualTexture_EntryEncodingRoundTrip` round-trips every field at its limits on both sides.

### Region allocation

`Flux_VTRegionAllocator` (`Zenith2/Engine/ZenithFlux/Internal/VirtualTexture/Flux_VTRegionAllocator.h`) is a 2D buddy (quadtree) allocator over the page-table pool's mip-0 texels.

| Operation | Algorithm | Complexity |
|---|---|---|
| `Allocate(uSideLog2)` | blocks of side `2^k` for k = 0 … log2(extent) are kept in one free list per k, sorted by (y, x) ascending so the choice is deterministic; take the lowest-address free block of side `2^s`; if none, split the lowest-address free block of the smallest larger side into four quadrants (order: (0,0), (1,0), (0,1), (1,1)), push the three unused ones and recurse; no larger free block is `FLUX_VT_ERROR_PAGE_TABLE_FULL` | O(log extent) splits, O(log n) list insertions |
| `Free(xRegion)` | push the block; while its three buddies are free, remove them and merge into the parent | O(log extent) |
| alignment | a block of side S starts at a multiple of S, so its mip-k rectangle starts at `origin >> k` with side `S >> k`, integral for every page mip |
| capacity | 256 regions of side 128 (VTs whose longer axis is 16,384) or 4,096 of side 32 (4,096 × 4,096 VTs) on desktop; 64 of side 128 or 1,024 of side 32 on the S24 |

A VT's region side is S = `2^(max(w, h) − 7)`; a non-square VT uses the top-left `pagesX(m) × pagesY(m)` entries of each mip rectangle and leaves the rest unwritten and unread.

### CPU mirror

`Flux_VirtualTextureSystem` keeps, per active VT, a CPU copy of the used part of its region at every page mip: the top-left `pagesX(m) × pagesY(m)` entries of mip m, one entry per tile, so `tiles × 4` bytes (21,845 entries, 87,380 bytes, for a 16,384 × 16,384 VT; 1,365 entries, 5,460 bytes, for a 4,096 × 4,096 one). The mirror is the authority for what the GPU page table holds, and the page-table update records are cut from it ("Page-table updates").

The invariant between the two is stated per row, because the record cap of "Dirty rows and run records" can leave rows dirty across a submitted frame: at every submitted frame boundary, the GPU page table equals the mirror in every used row that is not marked dirty, and equals it in every used entry when no row is dirty. `Test_VirtualTexture_MirrorMatchesGPU` reads the GPU page table back after a scripted sequence of maps and evictions and requires every used entry of every row not marked dirty to equal the mirror bit for bit; `Test_VirtualTexture_PageTableRecordCapDefers` and `Test_VirtualTexture_DeferredFrameKeepsPageTable` assert both halves.

## Virtual texture records

The record buffer is one `Flux_PersistentReadOnlyBuffer` of 4,096 × 32 bytes created at `EnableVirtualTexturing` (`Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`). It is a persistent pool sub-allocated under [Z2-N-REND-548]: a record is written once, at activation, by a `RequestUpload` into a slot no material handle references yet, and becomes referenceable when that upload is `COMPLETE` and the slot is acquired; it is never rewritten while the slot is live, because every field is fixed for the VT's lifetime. Rendering's pool list does not name the record buffer yet; cross-doc item X420 (g) adds it to the Pools row.

```slang
// Zenith2/Shaders/Common/VirtualTexture.slang ([FluxShared]; generated into Flux_SharedTypes.generated.h)
[FluxShared] struct Flux_VTRecord          // 32 bytes
{
	uint m_uRegionOrigin;      // page-table mip-0 texel of the region: x bits 0–15, y bits 16–31
	uint m_uShape;             // bits 0–3 w − 8, bits 4–7 h − 8, bits 8–11 M, bits 12–15 tail mip count,
	                           // bit 16 sRGB (sample the sRGB view), bit 17 wrap addressing, bits 18–31 zero
	uint m_uSlot;              // the record's own slot, written into requests
	float m_fMipBias;          // the VT's cooked mip bias (0 unless the cook sets one), added to λ for sampling and feedback
	uint m_uReserved0;         // zero
	uint m_uReserved3;         // zero (the tail texture's index is word 0 of the material handle)
	uint m_uReserved1;         // zero
	uint m_uReserved2;         // zero
};
```

Because the record buffer is `Flux_PersistentReadOnly`, a shader reaches it through a raw heap index ([Z2-N-REND-068]); its `FLUX_DESCRIPTOR_SPACE_BUFFER_SRV` index is fixed for the renderer's lifetime, so each pass carries it once, in `Flux_VTBinding::m_uRecordBufferSRV` ("Material input binding"), and no handle repeats it.

## Material handle encoding

A material texture field holds a `Zenith_RHIDescriptorHandle` (`m_uIndex`, `m_uReserved`; `Zenith2/Docs/Rendering.md → "Binding model"`). For a field bound to a VT, GEOM's material manager writes the handle `Flux_VirtualTextureResource::GetMaterialHandle()` returns:

| Word | Bits | Content |
|---|---|---|
| `m_uIndex` | 0–31 | the `FLUX_DESCRIPTOR_SPACE_TEXTURE_SRV` index of the VT's tail texture, a real heap index written at activation |
| `m_uReserved` | 0–11 | the VT's slot |
| `m_uReserved` | 12–27 | zero |
| `m_uReserved` | 28–31 | `0xA`, the VT tag `uFLUX_VT_HANDLE_TAG` |

Word 0 is a real heap index, as [Z2-N-REND-068] requires of a handle in material constants: a reader that is not tag-aware (GEOM's CPU walks over material fields, the editor's texture inspector, material preview, a pass that binds no VT pool) samples the always-resident tail texture, a correct if blurry image, and never an unrelated live texture. An ordinary texture handle has `m_uReserved` = 0, so the tag nibble distinguishes the two kinds with no material permutation, which is what lets ENV's `Material_TerrainLayered` bind virtual or streamed layer textures through the same fields (`Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"`).

<!-- assumption: pending RHI, RPROD -->
Two things are new to the binding model: (a) `Zenith_RHIDescriptorHandle::m_uReserved` is documented "0"; a VT handle in material constants uses it, which RHI's contract block must admit for handles reached through material constants (row AV4 (c)); (b) a shader reads both words of a `DescriptorHandle<T>` field through a new D-SLANG-neutral accessor `uint2 Flux_HandleWords<T>(DescriptorHandle<T> xHandle)` in `Zenith2/Shaders/Common/Bindless.slang`, which the D-SLANG (open, Stage 1) ruling realises for whichever lowering it picks, as it realises the four accessors of [Z2-N-REND-056] (rows AV1 (d), AV4 (c); cross-doc item X420 (a)). That the accessor can read a non-zero second word under every D-SLANG alternative is not yet shown: row AV18 asks RHI and RPROD to add the probe case `Test_BindlessProbe_HandleWordsRoundTrip` (an 8-byte handle with a non-zero second word in a material-constants buffer, read in pixel and compute stages on the four probe targets, both words bit-exact, and an ordinary handle reading `y == 0`) to the probe table and to D-SLANG's acceptance rule, with ADVGFX among D-SLANG's consumers blocked. Every VT ticket that uses `Flux_HandleWords` is `decision-pending` on D-SLANG until that ruling lands.
<!-- /assumption -->

The handle is valid only while the VT is `RESIDENT` ("Activation steps"); until then, after a failure and after unload, the material manager writes GEOM's usage fallback (`Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"`), and it rewrites the affected material-constants blocks by GEOM's reallocate-and-swap path in both directions (row AV3 (b)).

## Sampling

### Flux_SampleVirtual algorithm

`Flux_SampleMaterial(xTexture, uSampler, xUV, xGrad)` (`Zenith2/Shaders/Common/Sampling.slang`, [Z2-N-REND-245]) tests `(Flux_HandleWords(xTexture).y >> 28) == 0xA` and `Flux_VTPoolsBound()` (bit 1 of the invocation's stored `Flux_VTBinding`, "Material input binding"); when both hold it returns `Flux_SampleVirtual(...)` from `VirtualTexture.slang`, and otherwise it samples word 0 as an ordinary texture, as it does today, which for a VT handle is the tail texture. A pass that binds no VT pool therefore never enters the VT path, whatever handle reaches it (`Test_VirtualTexture_UnboundPoolsFallToTail`). The VT branch is uniform for a draw (every pixel of a draw binds the same material), so it costs no divergence; its cost in code size and registers is "Cost of the VT branch".

`float4 Flux_SampleVirtual(uint2 xHandle, float2 xUV, Flux_UVGradients xGrad)`, where `xBinding` below is the invocation's stored binding (module-internal):

| Step | Operation |
|---|---|
| 1 | load the record: `xRec = Flux_BufferSRV(xBinding.m_uRecordBufferSRV).Load<Flux_VTRecord>(32 × (xHandle.y & 0xFFF))` |
| 2 | decode the shape: w, h, M, tail mips T, sRGB, wrap; `W = 2^w`, `H = 2^h` |
| 3 | address: `u = wrap ? xUV − floor(xUV) : saturate(xUV)` (gradients are unchanged) |
| 4 | footprint in mip-0 texels: `dx = xGrad.m_xDdx × (W, H)`, `dy = xGrad.m_xDdy × (W, H)`; `Lmaj = max(|dx|, |dy|)`, `Lmin = min(|dx|, |dy|)` |
| 5 | anisotropy: `N = min(ceil(Lmaj / max(Lmin, 1e-6)), xBinding.m_uMaxAnisotropy)`; `λ = log2(max(Lmaj / N, 1)) + xRec.m_fMipBias` |
| 6 | feedback (colour passes only, "Feedback writers"): `mReq = clamp(floor(λ + xBinding.m_fFeedbackBias), 0, M − 1)`; `Flux_VTRecordRequest(Flux_VTEncodeRequest(slot, mReq, floor(u × pagesX(mReq)), floor(u × pagesY(mReq))))` |
| 7 | `m0 = floor(λ)`, `f = λ − m0` |
| 8 | `m0 ≥ M`: return the tail sample of "Tail sampling" |
| 9 | `s0 = PageSample(m0)`; if `f < 1/256`: return s0 |
| 10 | `s1 = (m0 + 1 < M) ? PageSample(m0 + 1) : TailSample(mip 0 of the tail)`; return `lerp(s0, s1, f)` |

`PageSample(m)`:

| Step | Operation |
|---|---|
| 1 | `pages = (W >> (7 + m), H >> (7 + m))`; `p = min(floor(u × pages), pages − 1)` |
| 2 | `e = Load(xBinding.m_xPageTable, int3((regionOrigin >> m) + p, m))`; decode x, y, layer, r |
| 3 | `pagesR = (W >> (7 + r), H >> (7 + r))`; `local = u × pagesR − floor(u × pagesR)` (the position inside the resident page, which for r > m is the ancestor of p) |
| 4 | `physUV = ((x × 136 + 4 + local.x × 128) / 4,080, (y × 136 + 4 + local.y × 128) / 4,080)` |
| 5 | physical gradients: `g = xGrad × (W >> r, H >> r) / 4,080`, then the anisotropy cap of "Anisotropy cap and border" |
| 6 | `SampleGrad(sRGB ? xBinding.m_xPhysicalSRGB : xBinding.m_xPhysicalLinear, uFLUX_SAMPLER_ANISO_CLAMP, float3(physUV, layer), g.ddx, g.ddy)` |

Cost per sample: one record load (cached; uniform per draw), one or two entry loads and one or two filtered samples; a `FLUX_TEXTURE_USAGE_NORMAL` VT returns RG and the caller reconstructs Z as for a streamed normal map.

### Cost of the VT branch

No material permutation separates VT from ordinary sampling, so both branches are compiled into every `Flux_SampleMaterial` call site of every material program: the VT branch is two dependent loads and a few tens of ALU operations per sample, and ENV's layered terrain material has about 12 call sites. A VT-free scene never takes the branch, but its registers are allocated. The S24 cost is measured, not assumed: the S24 VT device run (`Zenith2/Docs/Design/ADVGFX/Tests.md → "Device runs"`) adds an A/B of `ForwardPlusOpaque` GPU time on a VT-free scene with `render.vt.enabled` true versus false, and the difference is held under the proposed row "VT branch cost in VT-free material passes (S24)" (`Zenith2/Docs/Design/ADVGFX.md → "Pending row needs"`). With `render.vt.enabled` false the branch is still compiled; the row exists to bound that cost.

### Address modes

| Cooked address hint | Sampling | Borders (cook) |
|---|---|---|
| wrap (0) | step 3 wraps u into [0, 1) before the page lookup | each tile's border holds the texels across the VT edge modulo the extent |
| clamp (1) | step 3 saturates u | the border of an edge tile holds edge-replicated texels |

Sampler overrides of a material document (`Zenith2/Docs/Design/GEOM/Textures.md → "Sampler policy"`) select the tail's sampler alone; the physical cache is always sampled with `uFLUX_SAMPLER_ANISO_CLAMP` (6), because the borders, not the sampler, provide the addressing.

### Tail sampling

`TailSample` samples the tail texture `Flux_TextureSRV<float4>(xHandle.x)` (word 0 of the handle) with `SampleGrad(sampler of the field, u, xGrad.m_xDdx, xGrad.m_xDdy)`; the tail's mip 0 is VT mip M, so the hardware's own mip selection over the tail's T mips equals λ − M. A wrap VT's field sampler is the wrap form of its usage default.

### Anisotropy cap and border

A border of 4 texels keeps the filter footprint inside the page when the major-axis half-footprint is at most 4.5 physical texels. Step 5 of `PageSample` therefore clamps: when `max(|g.ddx|, |g.ddy|) × 4,080 > 9`, both gradient vectors are scaled by `9 / (max length × 4,080)`, and `render.vt.maxAnisotropy` (8 desktop, 4 S24) bounds N. A steep anisotropic footprint at the finer mip of a trilinear pair is therefore under-filtered by at most a factor of 16 / 9; `Test_VirtualTexture_FootprintStaysInsidePage` proves no sample reads a texel of a neighbouring page for 65,536 random (u, gradient) pairs (the physical cache filled with a page-ID pattern, the result decoded back to the expected ID).

### Filtering tolerance against streamed textures

The VT error tolerance ENV's assumption names (`Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"`, row EN19): one 2,048 × 2,048 source cooked both as a GEOM streamed texture and as a VT, fully resident, sampled at 16,384 (u, gradient) pairs with isotropic footprints of 1/4 to 64 texels and 4,096 anisotropic pairs up to N = 8, returns per-channel results within 2/255 of the streamed texture for isotropic footprints and within 6/255 for anisotropic ones, in UNORM8 of the encoded value (`Test_VirtualTexture_FilteringMatchesStreamedTexture`, WARP and lavapipe).

### Material input binding

<!-- assumption: pending RPROD, X420 (b) -->
`Flux_SampleVirtual` needs the pass's physical-cache and page-table views, which a material's `Evaluate(Flux_MaterialInput, Params)` signature does not carry. ADVGFX adds one member to Rendering's `Flux_MaterialInput` (`Zenith2/Docs/Rendering.md → "Materials"`, "Surface function contract"):

```slang
struct Flux_VTBinding
{
	DescriptorHandle<Texture2DArray<float4>> m_xPhysicalSRGB;    // the pass's patched field on m_xVTPhysical, sRGB view
	DescriptorHandle<Texture2DArray<float4>> m_xPhysicalLinear;  // the pass's patched field, UNORM view
	DescriptorHandle<Texture2D<uint>> m_xPageTable;              // the pass's patched field on m_xVTPageTable
	uint m_uMaxAnisotropy;                                       // render.vt.maxAnisotropy
	float m_fFeedbackBias;                                       // render.vt.mipBias + the pressure bias ("Memory accounting and pressure")
	uint m_uFeedbackFlags;                                       // bit 0: this pass records requests; bit 1: this pass binds the VT pools;
	                                                             // bits 8–15: log2 of the feedback scale
	uint m_uRecordBufferSRV;                                     // FLUX_DESCRIPTOR_SPACE_BUFFER_SRV index of the record buffer
	uint m_uFeedbackJitter;                                      // the frame's J of "Pixel selection and reservoir": x bits 0–7, y bits 8–15
};
// Flux_MaterialInput gains:  Flux_VTBinding m_xVirtual;
```

All invocation-private VT state lives in the Slang module `VirtualTexture` (`Zenith2/Shaders/Common/VirtualTexture.slang`, which opens with `module VirtualTexture;`): the copy of the invocation's `Flux_VTBinding`, the writing flag and the request reservoir, each declared `internal`, so they are visible inside that module alone. The module exposes only public functions: `Flux_VTBeginInvocation(xBinding, xPixel)` stores the binding and sets the writing flag, `Flux_VTPoolsBound()` reads bit 1 of the stored flags, `Flux_SampleVirtual` samples through the stored binding, `Flux_VTRecordRequest(uRequest)` updates the reservoir and `Flux_VTWriteFeedback(...)` stores it. `Sampling.slang` and `Material.slang` both `import VirtualTexture;`, and `VirtualTexture` imports neither, so the modules form no cycle ([Z2-N-REND-105]: one module per `Common/` file). `EvaluateMaterial` (RPROD's dispatcher in `Material.slang`, not a material module) calls `Flux_VTBeginInvocation(xIn.m_xVirtual, the invocation's pixel)` before it calls `Flux_ActiveMaterial::Evaluate`, so material modules keep their signature. They do not stay pure in the sense [Z2-N-REND-242] states today: a module's `Flux_SampleMaterial` call reaches the public mutator `Flux_VTRecordRequest`, so [Z2-N-REND-242] is amended by X420 (b) to admit that one write. The guarantee is exactly this: no module other than `VirtualTexture` can name the reservoir variable or the stored binding; a material module can still reach the mutator through `Flux_SampleMaterial`, which is the amended write. `Test_VirtualTextureShader_ReservoirNotNameableFromModules` (`ZenithShaderCompiler_Tests`, `Zenith2/Docs/Design/ADVGFX/Tests.md → "Virtual texture tests"`) proves the visibility rule with a compile-fail fixture and a positive control. The pinned Slang version (D-SLANG (open, Stage 1)) must accept the `module` declaration and `internal`; the test fails if it does not. Every pass whose programs evaluate materials of the `opaque` or `alphaTest` domains declares three `[Optional]` fields on the two pool resources while VT is enabled, fills `m_xVirtual` from them and sets bit 1 of `m_uFeedbackFlags` (rows AV1 (b), AV1 (c)); `Test_VirtualTexture_EveryMaterialPassBindsPools` inspects a compiled frame with VT enabled and requires every such pass to read both `m_xVTPhysical` and `m_xVTPageTable`. The transparent and decal passes bind no pool and leave bit 1 clear (AV-R5).
<!-- /assumption -->

## Feedback

### Feedback request encoding

```
bits 0–11   slot          (0 … 4,095)
bits 12–15  page mip m    (0 … 8)
bits 16–23  page x at m   (0 … 255)
bits 24–31  page y at m   (0 … 255)
0xFFFFFFFF  no request (page mip 15 never exists, so the value is unambiguous)
```

`Flux_EncodeVTRequest` / `Flux_DecodeVTRequest` (CPU) and `Flux_VTEncodeRequest` (Slang) are the only codecs (`Test_VirtualTexture_RequestEncodingRoundTrip`).

### Feedback writers

Feedback is written by the colour passes that evaluate opaque and alpha-tested materials, so VT adds no geometry pass:

| Pass (`Zenith2/Docs/Rendering.md → "Per-pass specifications"`) | Path | How the request reaches the buffer |
|---|---|---|
| `ForwardPlusOpaque` | Forward+ | pixel shader epilogue after `EvaluateMaterial` |
| `ForwardOpaquePerObject` | Forward | pixel shader epilogue |
| `GBuffer` | Deferred | pixel shader epilogue |
| `MaterialResolve` | visibility buffer | compute, one thread per pixel of the bin, after `EvaluateMaterial` |

<!-- assumption: pending RPROD, X420 (c) -->
Each of the four passes gains the `[Optional]` fields `Zenith_RHIUAV<Zenith_RHIByteBuffer> m_xVTFeedback`, `uint m_uVTFeedbackStride` (zero when the view writes no feedback) and `uint m_uVTFeedbackJitter` (the frame's J of "Pixel selection and reservoir"), and the three pixel-shader passes carry `[earlydepthstencil]`: they already test depth `EQUAL` with no discard and write no depth ([Z2-N-REND-916], [Z2-N-REND-917]), so forcing early depth changes no result and keeps early-Z with the UAV write (row AV1 (e)).
<!-- /assumption -->

The prepass, shadow depth, capture and preview passes sample VTs without recording requests. The transparent and decal passes never sample a VT: their materials cannot bind one (AV-R5), and a VT handle that reaches them through a runtime override samples its tail texture, because they bind no pool ("Flux_SampleVirtual algorithm").

### Pixel selection and reservoir

| Element | Rule |
|---|---|
| feedback scale s | 8 (desktop default) or 16 (S24 default), `render.vt.feedbackScale` |
| feedback texel | pixel (px, py) belongs to feedback texel `(px >> log2 s, py >> log2 s)` |
| the writing pixel | each frame one pixel per s × s block writes: `(px mod s, py mod s) == J(frameIndex)`, where J walks the s × s Bayer order: `J(k) = bayer_s(k mod s²)`, so every pixel position of a block is visited once in 64 (s = 8) or 256 (s = 16) frames |
| the reservoir | `Flux_VTRecordRequest` keeps `(count, chosen)`; the n-th recorded request replaces `chosen` when `hash32(px, py, frameIndex, n) mod n == 0` (PCG hash, `Zenith2/Shaders/Common/Random.slang`), so each of a pixel's n requests is chosen with probability 1/n, deterministically for a given pixel and frame |
| early return | `Flux_VTBeginInvocation` marks the invocation non-writing when `(px mod s, py mod s) ≠ J` (J from the binding's `m_uFeedbackJitter`) or bit 0 of the flags is clear, and `Flux_VTRecordRequest` returns at once in a non-writing invocation, so 63 of 64 pixels (s = 8) record nothing |
| the write | `Flux_VTWriteFeedback(xFeedback, uStride, xPixel)` in the pass epilogue: the writing pixel stores `chosen` (or nothing when count is 0) at `(py >> log2 s) × stride + (px >> log2 s)` |
| uniqueness | exactly one pixel per block writes in a frame, and each pixel is shaded by exactly one material pass of the opaque bucket set under the equal-depth test, so no feedback texel has two writers |

### Feedback buffers and readback

| Element | Rule |
|---|---|
| buffer | one graph transient per feedback view, `ceil(renderWidth / s) × ceil(renderHeight / s) × 4` bytes (230,400 for 2,560 × 1,440 at s = 8; 22,440 for 1,755 × 810 at s = 16) |
| first access | `VTFeedbackClear` writes `0xFFFFFFFF` over the whole buffer (`[FullCoverage]`, the first-write rule of [Z2-N-REND-432]) |
| readback | `VTFeedbackReadback`: `Flux_RenderGraph::AddReadback("VT.Feedback", buffer, whole range)` after the view's last feedback writer; one `Flux_ReadbackHandle` per view per frame, at most `uFLUX_VT_FEEDBACK_RING` (4) pending per view; a fifth is not declared and the frame writes no feedback for that view (`Flux.VT.FeedbackSkipped`) |
| failure | `FLUX_READBACK_ERROR_POOL_FULL`, `_FRAME_DEFERRED`, `_FRAME_FAILED` and device loss drop that frame's feedback for the view; nothing else changes (`Zenith2/Docs/Rendering/GpuResources.md → "Readbacks"`) |
| `Flux_ViewTargets` | the path function threads the buffer as `Flux_ViewTargets::m_xVTFeedback` (assumption pending RPROD, row AV1 (f), cross-doc item X420 (j)) |

### Views that write feedback

| View kind | Writes feedback | Why |
|---|---|---|
| `VIEW_KIND_MAIN`, `VIEW_KIND_EDITOR`, `VIEW_KIND_TEXTURE` | yes, up to `uFLUX_VT_MAX_FEEDBACK_VIEWS` (8) per frame in view-list order | the views a player or author looks at |
| `VIEW_KIND_SHADOW` | no | depth only |
| `VIEW_KIND_CAPTURE`, `VIEW_KIND_PREVIEW` | no | they sample what is resident; a capture is round-robined and never steers residency |
| `VIEW_KIND_STEREO` | not available before Stage 7 ([Z2-N-REND-602]) | — |

## Feedback processing

### Sort and unique

On `Flux.BuildGraph` (step 3c of "Frame-level flow"), every feedback readback that is `READY` is consumed in (frame index, view-list position) order, oldest first:

| Step | Operation | Complexity |
|---|---|---|
| 1 | append every value other than `0xFFFFFFFF` from every ready buffer into the sort arena, until it is full; values past its capacity are dropped and counted in `Flux.VT.FeedbackSkipped` | O(n) |
| 2 | LSD radix sort, four stable passes of 8 bits, into the arena's second half | O(n), n ≤ the arena's capacity |
| 3 | run-length unique into (request, count) pairs, in ascending request order | O(n) |
| 4 | `Release()` every consumed readback | O(views) |

The sort arena is allocated once, at `EnableVirtualTexturing`, with capacity `uFLUX_VT_MAX_FEEDBACK_VIEWS × ceil(W / s) × ceil(H / s)` requests for the extent `render.vt.maxFeedbackExtent` W × H (3,840 × 2,160 at s = 8 on desktop: 1,036,800 requests, 4,147,200 bytes per half; 2,340 × 1,080 at s = 16 on the S24: 79,968 requests, 319,872 bytes per half), and holds two halves for the sort. The result depends only on the multiset of requests, never on GPU timing within a readback or on the worker count.

### Request validation

A request is dropped and counted in `Flux.VT.FeedbackRejected` when its slot is not live, its page mip is not below the slot's M, or its page coordinates are outside `pagesX(m) × pagesY(m)`; a GPU defect or a slot freed after the readback's frame cannot steer residency. A freed slot is not reassigned for `uFLUX_VT_SLOT_QUARANTINE_FRAMES` (8) frames, longer than any readback can stay pending (3 frames in flight plus the 4-deep ring), so a request never names a newer VT that reused its slot.

### Touch rule

For each valid request (s, m, x, y): walk the chain (m, x, y), (m + 1, x / 2, y / 2), … up to the root mip; every resident page on the chain gets `m_ulLastTouchFrame` = the current feedback frame and moves to the tail of the LRU list. Ancestors are touched because they are the fallback of every pixel that wants a finer page that is not resident. The walk is at most 8 steps per request.

### Load target and refinement step

For a request whose own page is not resident and not in flight, with finest resident ancestor at mip r: the load target is the page on the chain at mip `t = max(m, r − uFLUX_VT_REFINE_STEP)`. Refinement therefore proceeds at most two mips per round, which bounds the visible pop and keeps the coarse levels that most pixels fall back to resident first. A target already in flight is not duplicated.

### Priority order

Candidate targets are sorted by this key, and loads are issued from the front while the in-flight limits allow:

| Key part | Order | Why |
|---|---|---|
| 1 | `r − t` descending | the largest gap between what is shown and what is wanted first |
| 2 | request count descending (the summed counts of the requests that chose this target) | screen coverage |
| 3 | `t` descending | coarser first among equals |
| 4 | slot, then y, then x ascending | a total order, so the issued set is deterministic for the same feedback |

### Request latency floor

With F frames in flight (2 or 3, [Z2-N-REND-481]), one refinement step (a hop) takes this long when the pipeline runs at full depth (the CPU F frames ahead of the GPU) and every read completes within one frame; a CPU-bound frame sees the readback `READY` sooner, so these figures are the pipelined latency at full depth, not a hard floor:

| Event | Frame |
|---|---|
| a colour pass writes the request | f |
| the readback is `READY` at step 3c; the request is processed and the read issued | f + F |
| the read is polled complete, decoded and its upload enqueued and staged by that frame's `StreamingUpload` | f + F + 1 |
| the upload is polled `COMPLETE` at step 3c, the page mapped, its rows cut and applied in that frame's prologue, and the page first sampled | f + 2F + 1 |

The pipelined first-hop latency is therefore 2F + 1 frames (5 at F = 2, 7 at F = 3). Reaching the finest mip from the root takes `ceil(gap / uFLUX_VT_REFINE_STEP)` hops, where the gap is the root mip's index: 4 hops for a 16,384 × 16,384 VT (mips 7 → 5 → 3 → 1 → 0), so a pipelined finest-mip latency of 4 × (2F + 1) = 20 frames at F = 2 and 28 at F = 3. The reservoir and the one-writer-per-block rule add frames on top of both figures. The two latency rows of `Zenith2/Docs/Design/ADVGFX.md → "Pending row needs"` are proposed above these figures: `Flux.VT.FirstHopFramesP95` (frames from the first frame whose feedback requests a non-resident page to the first frame that samples its load target) and `Flux.VT.FinestMipFramesP95` (frames from the first request of a mip-0 page whose finest resident ancestor is the root to the first frame that samples mip 0).

## Residency and eviction

### Physical page state machine

| State | Event | Next | Action |
|---|---|---|---|
| `FREE` | a decoded tile is assigned the page ("Tile upload") | `UPLOADING` | record the owner (slot, m, x, y); `RequestUpload` the tile into the page |
| `UPLOADING` | the upload reaches `COMPLETE` (polled at step 3c) | `RESIDENT` | map: mirror update and dirty rows ("Map and unmap algorithms"); append to the LRU tail with the current feedback frame |
| `UPLOADING` | the upload fails or is cancelled, or the owner VT is unloaded | `FREE` | count `Flux.VT.UploadFailures` on failure; the mirror never named the page |
| `RESIDENT` | chosen as a victim ("Eviction policy") | `DRAINING` (evicted) | take the next eviction epoch e ("Eviction epochs"); unmap: mirror update and dirty rows tagged with e ("Map and unmap algorithms"); remove from the LRU |
| `RESIDENT` | the owner VT is unloaded | `DRAINING` (unloaded) | no mirror update (the region is being freed); record the unload frame index |
| `DRAINING` (evicted) | `EndFrame(true)` publishes an applied epoch of at least e | `FREE` | push onto the free list (lowest index first) |
| `DRAINING` (unloaded) | `EndFrame(true)` of the first submitted frame after the step-3c drain that processed the unload | `FREE` | push onto the free list (lowest index first) |
| `DRAINING` | a deferred or failed frame (`EndFrame(false)`) | `DRAINING` | the rows stay dirty for the next frame ("Deferred and failed frames") |
| `PINNED` | entered from `UPLOADING` for a root page; left only by unload | `DRAINING` (unload) | never in the LRU, never a victim |
| any | device loss | `FREE` | "Device loss recovery" |

### Tile request state machine

| State | Event | Next | Action |
|---|---|---|---|
| `IDLE` | chosen as a load target | `READING` | `Zenith_AssetReader::ReadRange(asset, tile offset, stored bytes, staging, priority)` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"`); priority `ASSET_PRIORITY_HIGH` when `r − t ≥ 3`, else `ASSET_PRIORITY_NORMAL` |
| `READING` | the read's operation handle reports complete (polled) | `DECODING` | queue the tile for this frame's decode sub-jobs |
| `READING` | the read fails | `IDLE` | `Flux.VT.ReadFailures`, logged once per VT; the page may be requested again after `uFLUX_VT_EVICT_GRACE_FRAMES` |
| `DECODING` | the decode sub-job verifies the stored check and decompresses into tile staging ("Tile I/O and decode") | `DECODED` | — |
| `DECODING` | the check fails or the decode fails | `IDLE` | `Flux.VT.CorruptTiles`, logged once per VT, and the VT is marked corrupt for that page until a reload |
| `DECODED` | a physical page is free or a victim can be evicted | `UPLOADING` | the page state machine's first row |
| `DECODED` | no page can be had this frame | `DECODED` | `Flux.VT.EvictionStarved`; kept at most 2 × the platform's `Z2-B-ADVGFX-002` value (128 desktop, 48 S24) tiles, beyond which the lowest-priority decoded tile is dropped back to `IDLE` |
| any before `UPLOADING` | the VT is unloaded, or shutdown begins | `CANCELLING` | `RequestCancel` the read; discard staging when it completes |
| `CANCELLING` | the read reports complete or cancelled | (destroyed) | staging returns to the system's staging pool |

### Eviction policy

When a decoded tile needs a page and the free list is empty, the victim is the head of the LRU list (oldest `m_ulLastTouchFrame`), skipping pages whose last touch is within `uFLUX_VT_EVICT_GRACE_FRAMES` of the current feedback frame; ties in touch frame are broken by finer page mip first, then (slot, mip, y, x) ascending, because the list is kept in that order when several pages are touched in one frame. If the head is within the grace window, nothing is evicted this frame. A page whose descendants are resident may be evicted: their entries keep naming them, and only the entries that named the victim fall back to its ancestor.

### Draining rule

A page leaves `DRAINING` for `FREE` only after submitted frames have executed every one of its unmap rows (an evicted page, "Eviction epochs") or after the first submitted frame after the drain that processed its VT's unload (an unloaded page, whose materials GEOM has already rebound to the fallback, row AV3 (b)), and a tile is uploaded only into a `FREE` page. So no GPU frame ever samples a page through an entry that names its previous tile while the copy of its next tile is in flight: the frames that could hold such an entry precede the submitted frame that rewrote it, and the graph orders the next frame's copy after their reads of the imported physical cache ([Z2-N-REND-542]'s cross-frame ordering, which the physical cache inherits as a tracked imported resource). `Test_VirtualTexture_EvictedPageNotSampledDuringRefill` forces an eviction and a refill of the same page across a deferred frame and requires every sampled texel to carry the expected page ID. `Test_VirtualTexture_SplitUnmapRowsDrainByEpoch` evicts pages whose unmap rows exceed one frame's record cap and requires each page to stay `DRAINING` until the frame that applies its last row has run `EndFrame(true)`, and no earlier; `Test_VirtualTexture_UnloadDrainsAtFirstSubmittedFrame` unloads a resident VT across a deferred frame and requires its pages to become `FREE` at `EndFrame(true)` of the first submitted frame after the drain that processed the unload, and not before.

### In-flight limits

| Limit | Desktop | S24 | Key |
|---|---|---|---|
| tile reads in flight | 128 | 64 | `render.vt.maxReadsInFlight` |
| tiles decoded per frame | the frame's upload limit | same | — |
| tiles uploaded per frame | 32 | 12 | `render.vt.tilesPerFrame`, at most half the platform's `Z2-B-ADVGFX-002` value |
| tiles uploaded per frame during a camera-cut boost | 64 (proposed `Z2-B-ADVGFX-002`) | 24 | the row value ("Camera cuts") |
| decoded tiles waiting for a page | 128 | 48 | 2 × the row value |
| staging pool | `(reads + 2 × row) × 2 × 18,496` = 9,469,952 B | 4,143,104 B | derived |

## Tile I/O and decode

| Step | Rule |
|---|---|
| read | `ReadRange` returns the stored bytes of one tile from the `.zvt` at the offset of its tile-index entry (`Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile index section"`); the asset system holds a reference to the VT asset until the read completes |
| decode placement | the completed tiles of the frame are decoded by `Zenith_JobContext::ParallelFor` sub-jobs of the `Flux.BuildGraph` node (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"`), one tile per iteration, each into its own staging slot, joined before step 3c continues, so the result is independent of the worker count |
| verify | `Zenith_VerifySection` semantics over the tile: stored size equals the descriptor's stored bytes, and the stored check (low 32 bits of `Zenith_Hash64`) matches the entry (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Parsing and validation functions"`) |
| decompress | `Zenith_DecompressSection` with the entry's 24-byte `Zenith_CompressedSectionDesc` and the renderer's codec set into 18,496 bytes of staging (`Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"`); a `NONE` tile is copied |
| output | 18,496 bytes: 34 × 34 blocks in row-major block order, exactly the layout a buffer-to-texture copy of a 136 × 136 block-compressed region reads |

## Tile upload

A decoded tile assigned page p is uploaded through GEOM's streaming uploader in class `FLUX_UPLOAD_CLASS_VT_TILE` (`Zenith2/Docs/Design/GEOM/Textures.md → "Streaming upload arbitration"`):

| Element | Value |
|---|---|
| source | the tile's staging, valid until the operation reports `STAGED` |
| destination | the texture region (physical cache, mip 0, layer of p, origin `(x × 136, y × 136)`, extent 136 × 136) |
| splittable | no (18,496 bytes, far below `Z2-B-RHI-006`) |
| share | at most the frame's tile limit ("In-flight limits", never above `Z2-B-ADVGFX-002`) and the bytes the higher classes left ([Z2-N-REND-495] caps the ring), with a floor of `ceil(Z2-B-ADVGFX-002 / 4)` tiles per frame (16 desktop, 295,936 bytes; 6 S24, 110,976 bytes) that the class receives whatever the higher classes take (assumption pending GEOM, row AV19) |
| graph access | the frame's `StreamingUpload` pass declares `COPY_DEST` on `m_xVTPhysical` for the layers it writes, so every reader that frame is ordered after it and the imported-state table carries the state to the next frame |

<!-- assumption: pending GEOM, RHI -->
GEOM's `Flux_StreamingUploader` exposes no public enqueue member today. ADVGFX assumes `Zenith_Result<Flux_UploadOperation, Flux_UploadError> Flux_StreamingUploader::Enqueue(Flux_UploadClass eClass, const Flux_UploadRequestDesc& xDesc)` on the build node, and a texture-region form of `Flux_UploadDestination` whose texture is a graph-imported pool texture rather than a not-yet-referenced persistent texture (rows AV3 (c), AV4 (b)).
<!-- /assumption -->

<!-- assumption: pending GEOM -->
GEOM's "Streaming upload arbitration" gives `FLUX_UPLOAD_CLASS_VT_TILE`, the last of five classes, only the bytes the other classes leave, so a teleport that fills the ring with texture and mesh streaming can starve VT with no bound. Row AV19 asks GEOM for a per-frame floor of `ceil(Z2-B-ADVGFX-002 / 4)` `VT_TILE` tiles reserved ahead of the other streaming classes (never ahead of `BLOCKING`), and for the class limit to equal the row value so the two always agree. `Flux.VT.UploadStarvedFrames` counts frames in which the class received fewer tiles than the smaller of the floor and the decoded tiles waiting.
<!-- /assumption -->

## Page-table updates

### Map and unmap algorithms

For page (m, x, y) of slot s with region origin O and entry value `E = Flux_EncodeVTEntry(p, m)`:

| Operation | For every page mip k = m, m − 1, … 0 | Complexity |
|---|---|---|
| map | over the subtree rectangle `[x·2^(m−k), (x+1)·2^(m−k)) × [y·2^(m−k), (y+1)·2^(m−k))` of mip k: every mirror entry whose resident mip is greater than m becomes E; every changed row is marked dirty | `(4^(m+1) − 1) / 3` entries |
| unmap | let A be the mirror entry at mip m + 1, `(x / 2, y / 2)` (the finest resident ancestor of p); over the same rectangles every mirror entry equal to E becomes A | the same |

Entries equal to E are exactly the subtree entries whose finest resident page is p, so unmap never disturbs an entry that names a finer resident descendant, and map never overwrites one. Roots are never unmapped while their VT is active. The initial fill at activation writes the root entry into every entry of the region at every page mip.

### Dirty rows and run records

Per VT and page mip, the system keeps a dirty-row bit set over the region's rows. At step 11 ("Frame-level flow"), every dirty row, in the cut order of "Eviction epochs", is run-length encoded from the mirror into records; a row is cut whole or not at all, and the cut stops at the first row whose records do not fit under the cap:

```slang
[FluxShared] struct Flux_VTPageTableRun   // 16 bytes
{
	uint m_uX;          // page-table texel x at the record's mip (absolute, origin included)
	uint m_uYMip;       // y bits 0–15, page mip bits 16–19
	uint m_uLength;     // entries in the run, 1 … extent
	uint m_uValue;      // the entry every texel of the run takes
};
```

The frame's records are grouped by mip, ascending, with a per-mip `Flux_VTPageTableMipRange { m_uFirstRecord; m_uRecordCount; m_uFirstEntry; m_uEntryCount; }` (16 bytes) computed by a prefix sum over run lengths. At most `uFLUX_VT_MAX_PAGE_TABLE_RECORDS` (65,536) records are written in one frame; the remaining dirty rows stay dirty for the next frame, so an eviction whose rows did not fit leaves its page `DRAINING` ("Eviction epochs") and a map whose rows did not fit leaves its entries naming the ancestor until a submitted frame applies them. Both are safe. A cut becomes applied only at `EndFrame(true)`; `EndFrame(false)` leaves every row it cut dirty.

### Eviction epochs

| Element | Rule |
|---|---|
| epoch | each eviction takes the next value of a 64-bit counter that starts at 1 and never repeats while the renderer lives |
| row tag | a dirty row carries the minimum epoch of the unmaps that changed it since it was last applied; a row dirtied only by maps and initial fills carries none |
| cut order | rows carrying an epoch first, by ascending epoch, then (slot, mip, row); then every other dirty row in (slot, mip, row) order |
| applied epoch | at `EndFrame(true)`, after the frame's cut is marked applied: `E = (minimum epoch over rows still dirty) − 1`, or the last epoch taken when no dirty row carries one; E is published and never decreases |
| exit | an evicted page whose epoch e satisfies `e ≤ E` becomes `FREE` |
| unload | the unload drain discards every dirty row of the unloaded VT with its epoch tags (its region is freed, so no mirror will cut them), so E never waits on them |

Every row an unmap of epoch e changed either was applied by a submitted frame or is still dirty and carries an epoch of at most e, which would make E smaller than e; so `e ≤ E` holds exactly when every row of that unmap has been applied by a submitted frame, however many frames the cap split them over. The declared bytes are `16 × records + 16 × 16` per frame, declared at setup through `Flux_UploadRing::DeclareFrameData` and written at order 5 through `GetFrameData` (`Zenith2/Docs/Rendering/GpuResources.md → "Upload ring"`).

### VTPageTableUpdate pass

One compute dispatch per page mip that has records this frame, in ascending mip order, each writing one UAV view of that mip: thread i of the dispatch finds its record by binary search over the run prefix sums of its mip (as the GPU-scene scatter finds its record, `Zenith2/Docs/Rendering/GpuResources.md → "Scatter upload"`) and writes `m_uValue` at `(m_uX + i − runStart, y, mip)`. Records of one frame never cover one texel twice (each dirty row is encoded once from the final mirror), so the pass has no write race and its result is independent of thread order. A frame with no records adds no pass ([Z2-N-REND-281]). The pass table is "VTPageTableUpdate pass table".

## Frame-level flow

On the `Flux.BuildGraph` node of `Zenith2/Docs/Design/RPROD/Renderer.md → "Render frame sequence"`, VT adds these steps (row AV1 (a)):

| Renderer step | VT work | Order inside the step |
|---|---|---|
| 3c (after GEOM's step 3, before step 4) | `Flux_VirtualTextureSystem::BeginFrame(xContext)`: drain the activation queue ("Activation steps"); poll ready feedback readbacks and process them ("Feedback processing"); poll tile reads and run the decode sub-jobs; poll tile uploads and map the completed ones; assign pages to decoded tiles (evicting) and `Enqueue` their uploads; issue new reads in priority order | in the order listed |
| 7 (graph construction) | `ImportResources(xGraph, xShared)`: import the physical cache and the page-table pool into `Flux_FrameShared::m_xVTPhysical` and `m_xVTPageTable`; declare `VTPageTableUpdate` and its frame data | before step 8's feature functions |
| 8 (common prologue) | `VTPageTableUpdate` runs after `StreamingUpload` and before every consumer ("Common prologue placement" in `Zenith2/Docs/Design/ADVGFX.md`) | — |
| 9 (per view) | the path function calls `Flux_AddVTFeedbackClear` before the view's first feedback writer and `Flux_AddVTFeedbackReadback` after its last one, for a view of "Views that write feedback" | — |
| 11 (after `FITS` or `EXTENDED`) | `WriteFrameData()`: cut the run records from the mirror into the declared range | — |
| 13 (every path out of the node) | `EndFrame(bSubmitted)` | the last VT call of the frame |

## Activation and unload

### Activation steps

`Flux_VirtualTextureKindLoader` implements `Zenith_AssetKindLoader` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"`) for kind 18, in the shape of GEOM's loaders (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`):

| Member or step | Behaviour |
|---|---|
| `BeginLoad` | run `Flux_ReadVirtualTextureView` over the cooked header; request the `INFO`, `TILE_INDEX`, `TAIL` and `ROOT` sections in one round into staging of their decoded sizes; returns within 1 ms |
| `OnSectionsLoaded` | run the section rows of the reader (`Zenith2/Docs/Design/ADVGFX/VTCook.md → "Reader validation"`); build the `Flux_VirtualTextureResource` (tile index kept on the CPU, 40 bytes per tile); push it onto `flux.vt.activationQueue` (bounded, capacity = the loader's `m_uMaxConcurrentLoads` = 256, so the push cannot fail); `Activate(pObject)` |
| the activation drain (step 3c) | charge the VT's tile index and mirror (`44 × tiles` bytes) to the host budget ("Host memory bound"; past `render.vt.maxIndexBytes`: `FLUX_VT_ERROR_HOST_BYTES_FULL`) and its tail bytes to the tail reservation (past `render.vt.maxTailBytes`: `FLUX_VT_ERROR_TAIL_BYTES_FULL`); allocate a slot (lowest free not in quarantine; none: `FLUX_VT_ERROR_SLOTS_FULL`), a region (`FLUX_VT_ERROR_PAGE_TABLE_FULL`), the root pages (evicting unpinned pages; fewer than the root count obtainable: `FLUX_VT_ERROR_POOL_FULL`) and the tail texture (`Flux_PersistentResources::CreateTexture`, the tail mips); `RequestUpload` the tail, the record and the root tiles in class `FLUX_UPLOAD_CLASS_BLOCKING`; mark every used row of the region dirty with the root entries (the initial fill) |
| residency | when the tail, the record and every root tile are `COMPLETE` and a submitted frame has executed the initial fill, `ReportGpuResident` (`Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"`); the VT is `RESIDENT` and `GetMaterialHandle()` returns the VT handle |
| failure after activation | any error of the drain row, an upload `FAILED`, or a corrupt root tile: release everything allocated, `ReportGpuFailed(code)`; the asset stays ACTIVE with residency FAILED and materials keep the usage fallback |
| failure before activation | a reader row or staging allocation: `Fail(code)` with the `Flux_VirtualTextureError` value |
| `Cancel` | before activation (on ZenithAssets' thread, which then owns the object alone): release staging, `AcknowledgeCancel()` |
| `Unload` | runs on ZenithAssets' slice with no lock held (`Zenith2/Docs/Design/ASSET/Loading.md → "Unload"`) and touches no VT state: it validates the object, pushes `{object, device generation}` onto `flux.vt.unloadQueue` and returns |
| the unload drain (step 3c, before the activation drain) | for each entry: an object still in the activation queue is marked unloaded and skipped by the activation drain; an entry whose device generation predates the last device-loss recovery releases nothing, because `OnDeviceLost` already discarded its slot, region and pages; otherwise GEOM has already rewritten every binding to the fallback (row AV3 (b)), and the drain cancels the VT's reads, decodes and queued uploads, discards its dirty rows and their epochs, moves its pages to `DRAINING` (unloaded; `FREE` at `EndFrame(true)` of the first submitted frame after this drain), frees the region and the tail through deferred deletion with the current frame's last-use values, returns its tile-index, mirror and tail bytes to their budgets and quarantines the slot; every entry ends with `AcknowledgeUnload()` |
| `DropCpuCopies` | reads the object's atomic FAILED flag (acquire) and frees the tile index only when it is set; a FAILED object has no reads or decodes, so nothing on the build node reads its index; a RESIDENT VT needs its tile index to stream, so it drops nothing |
| headless | a headless registry serves kind 18 with ASSET's validate-only loader (`Zenith2/Docs/Design/ASSET/Loading.md → "Headless loading"`), so a headless run loads and validates every VT and creates nothing on a GPU |

### Unload and slot quarantine

`flux.vt.unloadQueue` is a CORE `Zenith_BoundedQueue` (multi-producer, single-consumer) of capacity 256 with `QUEUE_FULL_POLICY_REJECT`, which a push never meets: the asset system calls `Unload` at most once per object, and at most `m_uMaxConcurrentLoads` (256) kind-18 objects are between their last frame of use and their acknowledgement. The device generation is the renderer's counter of completed device-loss recoveries, read when `Unload` runs.

A freed slot keeps `m_ulFreedFrame`; it is reassigned only when the current frame index exceeds `m_ulFreedFrame + uFLUX_VT_SLOT_QUARANTINE_FRAMES`, and its record slot is rewritten only then, by a new upload. `Test_VirtualTexture_LoadUnload1000Cycles` loads and unloads VTs at randomised states (queued, reading roots, resident with tiles in every state) 1,000 times, with `Unload` called from a second thread as ZenithAssets does, under the `win-asan` preset as its sanitizer run (ZenithFlux has no `linux-tsan` leg), and requires no slot, region, page, staging, upload, read or allocation left behind, and no request ever accepted for a quarantined slot.

### Residency modes

| Mode (`render.vt.residencyMode`) | Behaviour | Used by |
|---|---|---|
| `stream` (default) | feedback-driven, as this file specifies | every Player and editor run |
| `full` | at activation, after the roots, every tile of every page mip is loaded in mip-descending order with the in-flight limits; feedback is ignored and nothing is evicted; a VT whose tiles do not fit the free pages is FAILED with `FLUX_VT_ERROR_POOL_FULL` | parity scenes, render tests and the physical-accuracy suite, which need residency independent of timing ([Z2-N-REND-942]); `Flux_VirtualTextureSystem::IsSettled()` is true when no tile of any VT is in flight |

## Camera cuts

When a view's `Flux_ViewRequest::m_bCameraCut` is set, the tile upload and decode limits rise from `render.vt.tilesPerFrame` (32 desktop, 12 S24) to the platform's `Z2-B-ADVGFX-002` value (64, 24) for `render.vt.cutBoostFrames` (30) frames; reads in flight are already at their limit. `Z2-B-ADVGFX-002` is `BR-DEFER` with a hard tolerance of 0 (`Zenith2/Docs/Budgets.md`), and the boost reaches it and never passes it: the row value is the boosted peak, `EnableVirtualTexturing` refuses with `FLUX_VT_ERROR_INVALID_DESC` a `render.vt.tilesPerFrame` above half the row value, and the boosted tiles still pass GEOM's arbitration, so they never starve the `BLOCKING` class. `Flux.VT.CutBoostFrames` counts the boosted frames.

## Deferred and failed frames

A deferred or failed frame ([Z2-N-REND-484], `Zenith2/Docs/Rendering/GpuResources.md → "Frame deferral"`) executes no `VTPageTableUpdate` and no feedback write. `EndFrame(false)` therefore keeps every dirty row dirty, keeps every `DRAINING` page draining, and keeps mapped pages mapped in the mirror; the next submitted frame cuts its records from the same mirror, so after any sequence of submitted, deferred and failed frames the invariant of "CPU mirror" holds at every submitted frame boundary: the GPU page table equals the mirror in every row not marked dirty, and everywhere when no row is dirty (`Test_VirtualTexture_DeferredFrameKeepsPageTable`). Readbacks declared by the deferred frame fail with `FLUX_READBACK_ERROR_FRAME_DEFERRED` and their feedback is dropped. CPU work already done at step 3c (reads issued, uploads enqueued) stands: an upload enqueued on a deferred frame is serviced at the next frame begin as for any other request.

## Memory accounting and pressure

| Element | Rule |
|---|---|
| subsystem | `MEMORY_SUBSYSTEM_RENDERING` through RHI's rendering memory consumer, category VT (`Zenith2/Docs/Rendering/GpuResources.md → "GPU memory rules"`, [Z2-N-REND-505]) |
| device-local bytes | the physical cache, the page-table pool, the record buffer and the tail textures (within their reservation): the GPU total of "Physical cache sizing" |
| host bytes | tile indices, mirrors, LRU nodes (32 bytes per physical page), the staging pool and the feedback sort arena; reported under `MEMORY_SUBSYSTEM_RENDERING`, category VT, against the proposed host row "Virtual texturing host memory" ("Host memory bound"), never against `Z2-B-ADVGFX-001` |
| growth | none on the device: both pools are fixed allocations, so a breach of the device row is a configuration error caught at `EnableVirtualTexturing`; on the host only the tile indices and mirrors grow, under the cap of "Host memory bound" |
| warning level | the feedback bias rises by 1 (requests one mip coarser), which lowers I/O, staging and uploads while the cache keeps its size |
| critical level | the feedback bias rises by 2, the read and upload limits halve, and `full` residency mode is unaffected |
| revert | one frame after the level falls, as every consumer ([Z2-N-ARCH-821]) |

<!-- assumption: pending ARCH, X421 (a) -->
`Zenith2/Docs/Architecture.md → "Memory governor"` (table "Shedding per subsystem") is the single source of shedding ([Z2-N-ARCH-822]); its Rendering row does not name VT. The two actions above join that row through cross-doc item X421 (a) ("the virtual-texture request mip bias rises by 1 at WARNING and by 2 at CRITICAL, the VT read and upload limits halve at CRITICAL, and the VT pools are fixed allocations that pressure never resizes"); until it lands, the VT tickets that implement them are blocked by that core-doc batch.
<!-- /assumption -->

### Host memory bound

| Component | Bytes | Desktop default | S24 default |
|---|---|---|---|
| staging pool | "In-flight limits" | 9,469,952 | 4,143,104 |
| LRU nodes | 32 × physical pages | 345,600 | 115,200 |
| feedback sort arena | 2 × 4 × the arena capacity ("Sort and unique") | 8,294,400 | 639,744 |
| fixed subtotal | allocated at `EnableVirtualTexturing` | 18,109,952 | 4,898,048 |
| tile indices and mirrors | 44 bytes per tile of every active VT (40 index, 4 mirror), capped by `render.vt.maxIndexBytes` | at most 33,554,432 (32 MiB) | at most 8,388,608 (8 MiB) |
| worst case | the sum | 51,664,384 (49.27 MiB) | 13,286,656 (12.67 MiB) |
| proposed row "Virtual texturing host memory" | pending, a member of MEM-WIN-RAM and MEM-S24 (cross-doc item X422) | 50 MiB | 13 MiB |

A 4,096 × 4,096 VT charges 60,060 bytes and a 16,384 × 16,384 one 961,180 bytes, so the desktop cap admits 558 of the former or 34 of the latter at once, and the S24 cap 139 of the former. Activation refuses a VT whose charge would pass the cap with `FLUX_VT_ERROR_HOST_BYTES_FULL` (the asset stays ACTIVE with residency FAILED and its materials keep the usage fallback); `EnableVirtualTexturing` refuses with `FLUX_VT_ERROR_BUDGET_EXCEEDED` a configuration whose worst case exceeds the host row. The proposed row leaves 764,416 bytes (desktop) and 344,832 bytes (S24) above the worst case for the per-slot bookkeeping. `Test_VirtualTexture_HostCapRefusesActivation` fills the cap with synthetic VTs and requires the next activation to fail with that code and change nothing.

## Device loss recovery

In the renderer's recovery (`Zenith2/Docs/Rendering/GpuResources.md → "Device loss"`, step 4), before any VT asset reloads: every read is cancelled, staging returns to its pool, every page is `FREE`, every mirror and region is discarded, the physical cache, the page-table pool and the record buffer are re-created on the new device, and every pending readback has already failed ([Z2-N-REND-501]). The asset system's residency set then reloads each VT through a device-recovery load, which runs the activation steps again (new slot, region and record; the material manager rewrites its handles), and residency rebuilds from feedback. `Test_VirtualTexture_DeviceLossRebuilds` (`Zenith_RHITestHooks::SimulateDeviceLost` on both backends) requires the frame 64 frames after recovery, in `full` mode, to equal the pre-loss frame bit for bit.

## Disabled virtual texturing

`render.vt.enabled` false (or a run that creates no renderer): `EnableVirtualTexturing` creates no GPU resource and registers the kind loader in refusing mode, whose `BeginLoad` calls `Fail(FLUX_VT_ERROR_NOT_ENABLED)`; ASSET's SUBSTITUTE policy for kind 18 applies, and every material field that names a VT binds GEOM's usage fallback because no VT is `RESIDENT`. The material passes then bind no VT field and write no feedback. `Test_VirtualTexture_DisabledBindsFallback`.

## Mobile parameters

VT runs on the S24 (ruling AV-R4 of `Zenith2/Docs/Design/ADVGFX.md → "Decisions"`), with the same code and these defaults:

| Element | S24 value |
|---|---|
| physical format | ASTC 4 × 4 sRGB with an ASTC 4 × 4 UNORM second view (`Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`, the ASTC rows) |
| layers | 4 (3,600 pages, 63.5 MiB; fixed pools 72,309,076 B with the page-table pool and records; GPU total 74,406,228 B with the 2 MiB tail reservation) |
| page-table extent | 1,024 |
| feedback scale | 16 |
| anisotropy cap | 4 |
| tiles per frame | 12 (24 during a camera-cut boost) |
| reads in flight | 64 |
| path | Forward+ alone, so `ForwardPlusOpaque` is the one feedback writer |
| host memory | 13,286,656 B worst case with `render.vt.maxIndexBytes` 8 MiB ("Host memory bound") |
| working set | 1,755 × 810 pixels × 12 VT samples (layered terrain) × 4/3 (mip chain) × 1.25 (trilinear pairs) / 128² texels per page ≈ 1,735 pages of the 3,600, before props |

The S24 frame and memory evidence is the `needs-human` device run of `Zenith2/Docs/Design/ADVGFX/Tests.md → "Device runs"` (`gpu-reason:visual-measure`).

## Pass tables

Each table follows the per-pass conventions of `Zenith2/Docs/Rendering/PerPass/Common.md → "Conventions"`. They are the payload of cross-doc item X420 (e) for `Zenith2/Docs/Rendering/PerPass/Common.md`, where [Z2-N-REND-795] places every pass; until that batch lands, the VT tickets cite these tables and X420.

### VTFeedbackClear

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VirtualTexture/VTFeedbackClear.slang` → `Flux_VTFeedbackClearParams` |
| Feature function | `Flux_RGBuffer Flux_AddVTFeedbackClear(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_VirtualTextureSystem& xVT)` |
| Kind | compute, 256 threads per group, one thread per 4 feedback texels (a `uint4` store) |
| Queue | G (A when the graph finds a gap; `FLUX_PASS_FLAG_ASYNC_COMPUTE` is not set, because the pass is not A-marked in Rendering's frame tables) |
| Capability | all tiers |
| Budget row | `Z2-B-ADVGFX-003` Virtual texturing feedback and page-table update GPU |
| Permutations | none |

| Field | Type | Access | Attributes |
|---|---|---|---|
| `m_xFeedback` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | UAV, C | `[FullCoverage]` |
| `m_uTexelCount` | `uint` | — | |

### VTPageTableUpdate pass table

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/VirtualTexture/VTPageTableUpdate.slang` → `Flux_VTPageTableUpdateParams` |
| Feature function | `void Flux_AddVTPageTableUpdate(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared, Flux_VirtualTextureSystem& xVT)` |
| Kind | compute, 64 threads per group, one thread per written entry, one dispatch per page mip with records |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-ADVGFX-003` |
| Permutations | none |

| Field | Type | Access | Attributes |
|---|---|---|---|
| `m_xPageTableMip` | `Zenith_RHIUAV<Zenith_RHITex2D>` | UAV, C (one mip) | |
| `m_xRuns` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_VTPageTableRun>>` | SRV, C (the frame chunk) | |
| `m_uRunBase`, `m_uRunCount`, `m_uEntryCount` | `uint` | — | |

### VTFeedbackReadback

| Property | Value |
|---|---|
| Program | none |
| Feature function | `void Flux_AddVTFeedbackReadback(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGBuffer xFeedback, Flux_VirtualTextureSystem& xVT)` |
| Kind | readback (`AddReadback`) of the whole feedback buffer |
| Queue | G or C, as the graph assigns readbacks |
| Capability | all tiers |
| Budget row | `Z2-B-RHI-011` GPU readback pool size (the pool), `Z2-B-ADVGFX-003` (the copy) |
| Permutations | none |

### Feedback fields of material passes

| Pass | Added fields | Access |
|---|---|---|
| every pass with an alpha-test permutation or material variants, which today is `Prepass` (alpha-tested bucket), `DeferredDepthPrepass` (alpha-tested bucket), `ShadowDepth` (alpha-tested bucket), `VisibilityRaster` (`bPERM_ALPHA_TEST` and its material variants), `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer` and `MaterialResolve` | `[Optional] Zenith_RHISRV<Zenith_RHITex2DArray> m_xVTPhysicalSRGB`, `[Optional] Zenith_RHISRV<Zenith_RHITex2DArray> m_xVTPhysicalLinear`, `[Optional] Zenith_RHISRV<Zenith_RHITex2D> m_xVTPageTable` | SRV, P (C for `MaterialResolve`) |
| `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer`, `MaterialResolve` | `[Optional] Zenith_RHIUAV<Zenith_RHIByteBuffer> m_xVTFeedback`, `uint m_uVTFeedbackStride`, `uint m_uVTFeedbackJitter` | UAV, P (C) |
