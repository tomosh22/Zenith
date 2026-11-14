# TWOD design: rendering

Section file of `Zenith2/Docs/Design/TWOD.md`. It specifies how 2D content reaches the screen: the extractor that writes TWOD's render-world records, the sort keys and the batching algorithm, the pixel-perfect canvas (its geometry, snapping and exactness rules), the GPU resources and passes TWOD adds to Flux through the module's two view extensions, the shader programs and stream-sets, colour and HDR output, and the headless and device-loss behaviour. The public declarations are `Zenith2/Docs/Design/TWOD/PublicAPI.md → "Flux_Sprite2D header"`.

## Scope of this file

| Topic | Heading |
|---|---|
| what simulation hands to Flux each frame | "Extraction" |
| the order 2D content draws in | "Sort keys" |
| from sorted records to draws | "Batching algorithm" |
| when frame data is declared and written | "Frame data path" |
| the canvas extent, scale and placement | "Canvas geometry" |
| camera and sprite snapping | "Snapping" |
| which outputs are bit-exact and why | "Pixel exactness rules" |
| instance layouts | "Sprite stream-sets" |
| persistent GPU tile data | "Tile tables and the chunk pool" |
| the passes | "Sprite2DScatter pass", "Sprite2DCanvas pass", "Sprite2DComposite pass", "Sprite2DWorld pass" |
| Slang programs | "Shader programs" |
| SDR, HDR10 and scRGB output | "Colour and HDR output" |
| which views draw what | "View extensions and view kinds" |
| no device, lost device | "Headless and device loss" |
| what goes wrong and what happens | "Render failure behaviour" |

## Extraction

`TwoD.Extract@W<tag>` is the module's one render extractor per world, registered through `Zenith_WorldAttachContext::AddExtractor` (`Zenith2/Docs/Design/RPROD/Extractors.md → "Extractor registration"`) with `m_uCostHint` 300 µs. It runs in phase 12 on its world's sealed state and writes, through its own `Flux_RenderWorldWriter` partition, exactly one `Flux_RenderSprite2DList`, the tileset records and the chunk records of the frame.

<!-- elaboration -->
| Declared read | Properties | Why |
|---|---|---|
| `Zenith.TwoD.Sprite` | 1–13 | the sprite records |
| `Zenith.TwoD.SortGroup` | 1–3 (`ReadAnyRow`) | ancestors' groups |
| `Zenith.TwoD.SpriteAnimator` | 1, 2, 6, 8 | the flipbook frame drawn |
| `Zenith.TwoD.Skeleton` | 1–3 | the rig, skin and visibility |
| `Zenith.TwoD.Tilemap` | 1–7 | the tilemap draws |
| `Zenith.TwoD.Camera` | 1–11 | the canvas record |
| `Zenith.Render.Camera` | 1, 2, 11 by name | main-camera selection (`Flux_SelectMainCamera`) |
| `Zenith.WorldTransform`, `Zenith.PreviousWorldTransform` | 1–7, 1–3 | the interpolated render transform ([Z2-N-ARCH-643]; `Zenith2/Docs/Design/SCENE/Transforms.md → "Interpolation data"`) |
| `ReadResource` `Zenith.TwoD.Poses`, `Zenith.TwoD.TilemapEdits` | — | bone poses; edited chunks |

The extractor declares no write, no command and no ordering; `EmitInto` orders it before `Render.Seal@W0` ([Z2-N-ARCH-439]).

Algorithm (O(n) in visible 2D entities, plus O(e) in chunks edited this frame):

1. **Main camera.** Gather `(Zenith.Render.Camera, simulation ID)` pairs and call `Flux_SelectMainCamera`; when it returns an index and that entity carries `Zenith.TwoD.Camera` with `Enabled`, build the canvas record (step 2), else `m_bHasCanvas = false` and count `twod.canvas.none_delta` once per frame with a camera present but no 2D camera.
2. **Canvas record.** Position: the camera entity's interpolated world position (x, y) from `Zenith_InterpolateWorldTransform`, widened to binary64; this clamped position `(cx, cy)` is also the list's reference point, against which every record origin of the list is stored (step 3). Bounds clamp when `BoundsEnabled`: with half extents `hx = ReferenceWidth / (2 × PPU)`, `hy = ReferenceHeight / (2 × PPU)`, `cx := (BoundsMax.x − BoundsMin.x ≤ 2hx) ? (BoundsMin.x + BoundsMax.x) / 2 : clamp(cx, BoundsMin.x + hx, BoundsMax.x − hx)`, likewise y. Colours: `Zenith_ColourToSrgb8Packed` of the letterbox and clear colours. The output view kind mask is `1 << VIEW_KIND_MAIN` (row TD8 adds the editor game view).
3. **Sprites.** A `ParallelFor` over the chunks of the query `WITH Zenith.TwoD.Sprite, Zenith.WorldTransform` (grain 1 chunk) writes each leaf's records into a frame-arena array per leaf; a visible sprite emits:
   - SIMPLE: one record; SLICED: nine records (corners unscaled, edges stretched along one axis, centre stretched along both; a zero border on a side removes that row or column, so 1, 2, 3, 4, 6 or 9 records); TILED: the frame repeated from the bottom-left of `Size`, the last row and column clipped by shrinking both the quad and its texel rectangle, at most `uTWOD_MAX_TILED_QUADS` (a larger tiling emits the first 1,024 and counts `twod.extract.tiled_clipped_delta`);
   - the frame drawn: the animator's `CurrentSprite` from the clip's atlas when the entity's `Zenith.TwoD.SpriteAnimator` is playing a flipbook state (`Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation"`), else the component's `Sprite` from `Atlas`;
   - the record's geometry: with the frame's pivot `(px, py)` in texels from the top-left, its trimmed size `(w, h)` and trim offset `(tx, ty)` in the untrimmed frame of height `H`, and `u = 1 / PPU_atlas`: the bottom-left of the trimmed quad in the entity's local XY plane is `L = ((tx − px) × u, (py − (ty + h)) × u)`; corners transform by the interpolated 2D matrix `M` (z = 0 plane of the world transform): `p = M·(L, 0)` in world metres, `basisX = M·(w·u, 0, 0)` (direction only, z dropped), `basisY = M·(0, h·u, 0)`; FlipX mirrors about the pivot (`L.x := −(L.x + w·u)` and the texel rectangle's x pair swapped), FlipY likewise in y;
   - the stored origin is camera-relative: `m_afOrigin = ((float)((double)p.x − cx), (float)((double)p.y − cy), p.z)` with `(cx, cy)` the list's reference point of step 2, or (0, 0) when the list has no canvas; the subtraction is exact in binary64 and the narrowed value is small near the camera, so snapping keeps full precision wherever the camera is. The absolute precision of `p` is the world transform's own (binary32), which large worlds keep small by rebasing (`Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D origin shift"`);
   - the texel rectangle from `Zenith_SpriteFrameView::m_auRect`; the page's `Flux_TextureID` from the page handle's resolved `Flux_TextureResource::GetID()` (an unresolved page writes the null ID);
   - flags: space, blend, snap (`PixelSnap`), point filter (the atlas's `SPRITE_ATLAS_FILTER_POINT`).
   An invisible sprite, one whose atlas is unresolved, or one whose name is not in the atlas emits nothing (`twod.render.missing_sprite_delta` for the last two).
4. **Skeletons.** For each entity with `Zenith.TwoD.Skeleton` and a pose in `Zenith.TwoD.Poses`: one record per visible slot of the skin, in the rig's slot draw order, each slot's attachment frame placed by its bone's world pose (`Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"`). The records of one entity are consecutive in its leaf's array, so the stable sort keeps the slot order among them.
5. **Sort keys.** Each record's `m_ulSortPrimary` is computed here ("Sort keys") and `m_ulSimulationID` set.
6. **Tilemaps.** For each entity with `Zenith.TwoD.Tilemap` whose map resolves: one `Flux_Sprite2DTilemapDraw` per map layer with its sort key (the component's `Layer` plus the layer's authored layer offset, clamped to 31; the component's `Order` plus the layer's order offset), the map's tileset key, the origin of cell (0, 0) (the map entity's interpolated world position, camera-relative as step 3, z plus the layer's z offset), the cell size, the layer's parallax factor and the tint with its alpha multiplied by the layer's opacity; the map key is `Flux_MakeModuleInstanceKey(Zenith_Hash64 of (simulation ID, world tag))`.
7. **Tileset records.** For each tileset referenced by a drawn map that this world has not sent since its last full-state extraction (or whose asset revision changed): one `Flux_RenderTilesetRecord` with every tile entry ("Tile tables and the chunk pool"); a tileset no map of the world references any more for 60 frames is sent with `m_bRemove`.
8. **Chunk records.** The module keeps, per world, a pending set of (map key, layer, chunk) entries ordered by (squared distance from the chunk centre to the canvas camera, map key, layer, chunk y, chunk x), filled when a map is first drawn (every non-empty chunk), when a tile edit changes a chunk (`Zenith2/Docs/Design/TWOD/Simulation.md → "Tile edit playback"`), and with every chunk of every map when `IsFullExtraction()` is true; it writes at most `twod.extract.maxChunkRecordsPerFrame` UPSERT records per frame from the front of the set (the effective cells: authored cells overlaid by the edit resource, with `m_ulContentHash` = `Zenith_Hash64` of the 4,096 cell bytes) and every REMOVE record of the frame (a removed map: one `REMOVE_MAP`; a chunk that became entirely empty: `REMOVE_CHUNK`). Each chunk is sent once per change: the renderer keeps it in its CPU chunk mirror and decides GPU residency itself ("Tile tables and the chunk pool"), so the extractor never learns of, and never needs to repair, an evicted or refused GPU slot.
9. **Write.** Concatenate the leaf arrays in leaf order into one frame-arena span and call `AddSprite2DList` once; then `AddTilesetRecord` and `AddTilemapChunk` for steps 7–8 in their order. A refused list (`FLUX_RENDER_WORLD_ERROR_SLOT_FULL`) draws no 2D content in that world for the frame and counts `twod.extract.slot_full_delta`; a refused chunk or tileset record stays in the pending set for the next frame.

The extractor runs in every configuration, headless included ([Z2-N-ARCH-785]; the RPROD ruling that every render path runs headless): headless, asset page handles resolve to no runtime object, every record carries a null page ID, and no render subgraph consumes the world. Extraction never writes simulation state.

## Sort keys

The 2D draw order is a total order on records, independent of chunk layout, worker count and extraction order:

| Bits of `m_ulSortPrimary` | Field | Value |
|---|---|---|
| 56–63 | layer | the sorting layer 0 … 31 of the record (or of its outermost enabled sort group) |
| 40–55 | order | `order + 32768` (the record's, or its group's, `sint16` order) |
| 16–39 | y key | for a layer listed in `twod.ySortLayers`: `(2^24 − 1) − clamp(q(y) − q(y_ref) + 2^23, 0, 2^24 − 1)` with `q(v) = ⌊(double)v × 256⌋` as a signed 64-bit integer, `y` the world y of the sort point (the entity's interpolated position, or its group root's) and `y_ref` the canvas camera y (0 without a canvas); otherwise `2^23`. Both terms are quantised to the same 1/256 m grid before the difference, so the key difference of two records never depends on the camera |
| 0–15 | sub-order | inside a sort group: the record's own `order + 32768`; outside: 32768 |

- The sort is ascending on `(m_ulSortPrimary, m_ulSimulationID)`, stable, so the records of one entity keep their emission order. Lower keys draw first (further back); a higher y draws earlier in a y-sorted layer, so lower objects overlap higher ones.
- The y key resolution is 1/256 m over ±32,768 m of the camera; beyond it the key saturates and ties break by simulation ID.
- A sort group is the outermost ancestor-or-self with an enabled `Zenith.TwoD.SortGroup`; nested groups flatten into it. The ancestor walk reads SCENE's hierarchy (`Zenith2/Docs/Design/SCENE/Transforms.md → "Hierarchy cache"`), at most 64 levels (a deeper chain uses the first 64 and counts `twod.render.sort_depth_delta`).
- Tilemap draws use the same key; all chunk draws of one tilemap layer share it and stay consecutive.
- `Test_Sprite2DSort_KeyFields` asserts each field's bits, and that two sprites 0.002 m apart in y keep their relative order while the camera y moves in 10⁻⁴ m steps over 1 m; `Test_Sprite2DSort_IndependentOfChunkLayout` extracts one scene with forced compaction and two chunk capacities and asserts identical sorted record sequences.

Sorting is an LSD radix sort over the 16 key bytes (simulation ID bytes 0–7 first, then the primary's), 8-bit digits, skipping a digit pass when every key holds the same digit: O(16n) worst case with n records, O(n) extra memory in the frame arena; a 262,144-record frame sorts in at most 16 passes.

## Batching algorithm

Per canvas, and per world list, after culling and sorting:

1. Walk the sorted records and tilemap chunk draws in order.
2. Start a batch when the program (sprite or tile chunk), the blend mode or the destination changes; texture pages never break a batch, because the page is an index into the frame's page table and every page is reached bindlessly (Z2-D-054).
3. A batch is `{ program, blend, first instance, instance count }`; its instances are contiguous in the frame's instance range.
4. Each batch is one `DrawIndexedInstanced(6, count, 0, 0, firstInstance)` over the six-index quad index buffer (`0, 1, 2, 2, 1, 3`, a 16-bit `Flux_PersistentReadOnlyBuffer` created by `Enable2D`) with the instance stream bound at the frame range's offset.

Complexity O(n). The batch count is the number of program and blend changes along the sorted sequence plus one: a scene of opaque-cutout sprites in one blend mode draws in one batch per run between tilemap layers. `flux.twod.batches` reports it; `Test_Sprite2DBatch_TextureChangeKeepsBatch` and `Test_Sprite2DBatch_BlendChangeSplits` assert rules 2 and 3.

## Frame data path

`Flux_Sprite2DRenderer` follows the build node's upload order (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`) exactly as UI's renderer does (`Zenith2/Docs/Design/UI/Rendering.md → "Flux UI renderer"`): nothing is written to ring memory before the frame's `ExtendReservation`.

| Call | Where | Work |
|---|---|---|
| `PrepareFrame(xWorld, xRing)` | `Flux.BuildGraph` step 4 (row TD1) | (a) apply every section's tileset records then chunk records in section and record order to the CPU mirrors ("Tile tables and the chunk pool"), then run the residency pass of that heading, producing the frame's scatter list; (b) build the frame page table: every distinct `Flux_TextureID` of the frame's records and tile entries, resolved through `Flux_TextureManager::Resolve` to the SRV, extent and sampler index (`uFLUX_SAMPLER_POINT_CLAMP` for point-filter records, else `uFLUX_SAMPLER_LINEAR_CLAMP`); a null or stale ID maps to `GetFallbackSRV(FLUX_TEXTURE_USAGE_COLOUR)`; at most 4,096 pages a frame (the 4,097th distinct page draws the fallback, `flux.twod.page_table_full_delta`); (c) match canvases: for each section with `m_bHasCanvas`, each view request of the section whose kind bit is in `m_uOutputViewKindMask` gets a canvas entry keyed by `Flux_MakeViewID(kind, owner, sub)`, with `Flux_ComputeCanvasGeometry` over the request's output extent (the viewport when non-zero), up to `m_uMaxCanvasesPerFrame` (the rest count `flux.twod.canvas_limit_delta` and their views draw canvas content as world); (d) per canvas: cull, sort, snap, generate each tilemap draw's chunk instances ("Tile tables and the chunk pool", rule "Chunk instances"), write the 40-byte canvas instances and 36-byte chunk instances into frame-arena staging, build batches; in a pure-2D view ("View extensions and view kinds") the section's world-space records join the canvas list, sorted with it by key and drawn unsnapped at their camera-relative positions, z ignored; (e) per section: the world list (records with the world space bit) for every view that is not pure-2D, and, when a view of the world extension's kinds has no canvas, the full list (every record), each culled by nothing, sorted, staged as 48-byte world instances (origin = reference point + stored origin in binary64, narrowed) with their batches; (f) cap: the instances of every list together are at most `m_uMaxSpritesPerFrame`; lists over the cap are dropped whole from the highest world tag down (`flux.twod.sprites_dropped_delta`); (g) `DeclareFrameData` once for the instances (16-byte aligned, owner `Sprite2D`), once for the page table (owner `Sprite2DPages`) and once for the scatter payload (owner `Sprite2DScatter`) |
| feature functions | step 9, through the view extensions | declare the passes; each imports the frame chunk with `ImportFrameChunk` and binds its batches' ranges |
| `WriteFrame(xRing)` | step 11 after `FITS` or `EXTENDED` | copy the staging arrays into `GetFrameData` of the three tickets and store each batch's first-instance index (the allocation offset / stride plus the batch's offset) in the frame, before any record job starts |
| `EndFrame(bSubmitted)` | step 13, every path | commit the chunk-pool and tile-table slot changes of a submitted frame; on a frame that was not submitted, roll the slot tables back to the previous frame's state and re-queue the frame's chunk and tileset updates at the front of the next frame's scatter list, so a deferred or failed frame loses no upload |

A deferred or failed extension writes no 2D byte and draws no 2D content that frame (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`); sprite lists are whole-frame state, replaced by the next frame's. `Test_Sprite2DRenderer_WritesAfterReservation` asserts the order on the software device through the real `Flux_UploadRing`.

## Canvas geometry

`Flux_ComputeCanvasGeometry(xCanvas, Wo, Ho)` with reference extent `(Wr, Hr)`, margin `m = (snap == SUBPIXEL) ? 1 : 0`, all integer arithmetic unsigned unless stated:

| Rule | Mode | Result |
|---|---|---|
| 1 | INTEGER_LETTERBOX, `k = min(⌊Wo/Wr⌋, ⌊Ho/Hr⌋) ≥ 1` | canvas `Wc = Wr`, `Hc = Hr`; world-to-canvas `s = PPU` |
| 2 | INTEGER_EXPAND, `k ≥ 1` (same k) | `Wc = min(⌊Wo/k⌋, 4096 − 2m)`, `Hc = min(⌊Ho/k⌋, 4096 − 2m)`, so `Wc ≥ Wr` and `Hc ≥ Hr`; `s = PPU` |
| 3 | NATIVE | `k = 1`, `Wc = min(Wo, 4096 − 2m)`, `Hc = min(Ho, 4096 − 2m)`, `s = PPU × Ho / Hr` (binary32) |
| 4 | every mode with k ≥ 1 | destination `Wd = k·Wc`, `Hd = k·Hc`, `dx = ⌊(Wo − Wd)/2⌋`, `dy = ⌊(Ho − Hd)/2⌋` (signed), bars outside it filled with the letterbox colour |
| 5 | modes 1 and 2 with `k = 0` (the output is smaller than the reference) | `Wc = Wr`, `Hc = Hr`, `f = min(Wo/Wr, Ho/Hr)` in binary32, `Wd = ⌊Wr·f⌋`, `Hd = ⌊Hr·f⌋`, `dx`, `dy` as rule 4, `m_fDownscale = f`; logged once per view and counted `flux.twod.canvas_downscaled_delta` |
| 6 | `Wo = 0` or `Ho = 0` | every extent 0: no pass |

<!-- elaboration -->
| Output | Reference | Mode | k | Canvas | Destination | Bars |
|---|---|---|---|---|---|---|
| 2560 × 1440 (reference desktop) | 320 × 180 | LETTERBOX | 8 | 320 × 180 | 2560 × 1440 at (0, 0) | none |
| 2340 × 1080 (S24) | 320 × 180 | LETTERBOX | 6 | 320 × 180 | 1920 × 1080 at (210, 0) | 210 px left and right |
| 2340 × 1080 (S24) | 320 × 180 | EXPAND | 6 | 390 × 180 | 2340 × 1080 at (0, 0) | none |
| 2560 × 1440 | 384 × 216 | LETTERBOX | 6 | 384 × 216 | 2304 × 1296 at (128, 72) | 128 px, 72 px |
| 1920 × 1080 | 384 × 216 | EXPAND | 5 | 384 × 216 | 1920 × 1080 at (0, 0) | none |
| 300 × 200 | 320 × 180 | LETTERBOX | 0 → rule 5 | 320 × 180 | 300 × 168 at (0, 16) | 16 px top and bottom |
| 2560 × 1440 | 1280 × 720, PPU 100 | NATIVE | 1 | 2560 × 1440 | 2560 × 1440 | none; s = 200 px/m |

`Test_Sprite2DCanvasGeometry_Rules` asserts every row of the table above and the rule 2 and 3 clamps at 4,096.

## Snapping

Canvas coordinates have their origin at the top-left of the canvas texture (margin included), +x right, +y down; one unit is one canvas texel. With `s` from "Canvas geometry" (binary64), the list's reference point `(cx, cy)` (binary64, the clamped canvas camera of "Extraction" step 2), the canvas centre `(cX, cY) = (⌊Wc/2⌋ + m, ⌊Hc/2⌋ + m)` and a record's camera-relative origin `o` (binary32, widened), every value below is computed in binary64 in the order written, never through a division by `s`:

| Snap mode | Camera remainder `(rx, ry)` in canvas texels | Composite offset (output pixels) |
|---|---|---|
| NONE | `(0, 0)` | `(0, 0)` |
| TEXEL | `nx = ⌊cx·s + 0.5⌋`, `ny = ⌊cy·s + 0.5⌋` (signed 64-bit); `rx = cx·s − nx`, `ry = cy·s − ny`, each in [−0.5, 0.5) | `(0, 0)` |
| SUBPIXEL | `bx = ⌊cx·s⌋`, `by = ⌊cy·s⌋`; `rx = cx·s − bx`, `ry = cy·s − by`, each in [0, 1) | `ox = ⌊rx·k⌋`, `oy = ⌊ry·k⌋` |

- A point at camera-relative position `o` maps to `X = o.x·s + rx + cX`, `Y = −o.y·s − ry + cY` (the canvas camera sits at the whole texel `n` or `b`, so the remainder shifts the picture by the camera's fraction), stored in binary32. This equals `X = (p.x − C.x)·s + cX` with `C.x = n / s`, but it never forms `n / s`, so a corner at a ±0.5 rounding boundary rounds the same way for every PPU, 24 and 100 included.
- A record with its snap flag set, on a camera whose mode is not NONE, is snapped as a whole: its top-left corner `T = canvas(o + basisY)` moves to `(⌊T.x + 0.5⌋, ⌊T.y + 0.5⌋)` and every corner moves by the same delta. A tilemap layer's chunk instances snap the same way through the canvas position of cell (0, 0)'s top-left, after parallax ("Tile tables and the chunk pool").
- The composite samples, for the output pixel at signed offset `(qx, qy)` inside the destination rectangle, canvas texel `ix = floor_div(qx + ox, k) + m`, `iy = floor_div(qy − oy, k) + m` in signed 32-bit integers, where `floor_div(a, b) = (a ≥ 0) ? a / b : −((−a + b − 1) / b)` for `b > 0` (so `floor_div(−1, k) = −1` and the first `oy` rows read the top margin row 0); under SUBPIXEL the canvas shows the camera at whole texels and the composite shifts the picture by whole output pixels, so motion is smooth at output resolution while every canvas texel stays a crisp `k × k` block.
- Far from the origin the stored origin `o` is camera-relative and exact when the record's world position `p` and the camera position are exact in binary32 (for example multiples of 2⁻¹⁰ m below 16,384 m), so snapping far from the origin reproduces the result at the origin bit for bit for positions on that grid, and otherwise to the binary32 precision of `p` (2⁻¹⁰ m in [8,192, 16,384) m), which rebasing keeps small ("Extraction" step 3).
- `Zenith2/Engine/Zenith2D/Tests/Support/Zenith_TwoDReferenceSnap.h` computes these formulas with the same binary64 operations in the same order, so it agrees bit for bit at every boundary; it is the oracle of the snapping and render tests.
- `Test_Sprite2DSnap_TexelCameraRounds`, `Test_Sprite2DSnap_SubpixelOffsetsAndMargin` and `Test_Sprite2DSnap_SpriteCornerRounds` assert the three rows and the rounding at ±0.5 boundaries at PPU 16, 24 and 100; `Test_Sprite2DSnap_FarFromOrigin` repeats them for the scene and camera offset by ±10,000 m with every coordinate a multiple of 2⁻¹⁰ m, where the camera-relative origin keeps every result bit-identical to the result at the origin.

## Pixel exactness rules

A canvas output is **pixel-exact** when, for every output pixel inside the destination, the output's stored bytes equal the bytes of the atlas texel that the integer mapping of "Snapping" selects. The conditions:

1. the atlas is `SPRITE_ATLAS_FILTER_POINT`, so its pages are `FLUX_TEXTURE_USAGE_COLOUR_EXACT`: uncompressed `R8G8B8A8_SRGB`, one mip, never streamed, never block-compressed (row TD4 asks GEOM for the usage class);
2. the camera's mode is INTEGER_LETTERBOX or INTEGER_EXPAND with `k ≥ 1` and snap TEXEL or SUBPIXEL;
3. the sprite's atlas PPU equals the camera's PPU, its rotation is a multiple of 90°, its scale is 1 or −1 per axis, and its snap flag is set;
4. the texel's alpha is 255 and the sprite's tint is `(255, 255, 255, 255)` in its packed form, with blend ALPHA or PREMULTIPLIED, and no record above it in sort order covers the pixel with a non-zero alpha;
5. the output mode is SDR (`B8G8R8A8_SRGB`), or the view outputs into a `R8G8B8A8_SRGB` texture.

Why it holds: the canvas is `R8G8B8A8_SRGB` (`Zenith2/Docs/Rendering.md → "Required format features"`; the row gains TWOD's uses through row TD5); sampling an sRGB texel decodes it exactly, a source alpha of 1 makes the blend `src × 1 + dst × 0`, the attachment encode of a value that decodes from an 8-bit code returns that code (the conversion's error bound is below half a code step), and the composite reads the canvas with `Load` (no filtering) and writes through the swapchain's sRGB view with the same encode. Canvas pixel centres sample texel centres because snapped corners are integers and the PPUs match.

- When a sprite breaks condition 3 it still draws (nearest sampling), and the extractor counts it: `twod.render.mixel_sprites_delta` (PPU mismatch or non-unit scale) or `twod.render.rotated_canvas_sprites_delta` (rotation off 90°).
- Semi-transparent texels and tinted sprites blend in linear light, the engine's working space (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Working colour space"`); they are colour-correct, not byte-exact.
- `Test_Sprite2DRender_PixelExactCanvas` (Render tests in `Zenith2/Docs/Design/TWOD/Tests.md`) renders a fixture atlas of every 8-bit value per channel at k = 1, 3 and 6 on WARP and lavapipe into an `R8G8B8A8_SRGB` view texture and compares every pixel to the CPU mapping of "Snapping": equal bytes, no tolerance, no committed golden.

## Sprite stream-sets

The SPRITE stream-set family (`STREAM_SET_FAMILY_SPRITE`, field value 3, `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set code fields"`) is TWOD's. TWOD declares three stream-sets in `Zenith2/Shaders/Common/SpriteStreamSets.slang`, outside the reachable-set rule as GEOM states; their codes keep `FAMILY` = 3 in bits 12–14 and carry a TWOD variant index in bits 0–11, every other bit zero (row TD6 asks GEOM's validator to accept exactly these):

| Code | Canonical name | Stream 4 (per instance), fields in order | Stride |
|---|---|---|---|
| `0x3001` (the family default) | `sprite:canvas` | `INSTANCE_RECT` float32x4 (origin x, origin y, basis X x, basis X y) in canvas texels; `INSTANCE_DATA0` float32x2 (basis Y x, basis Y y); `INSTANCE_DATA1` uint16x4 texel rectangle (already flipped); `INSTANCE_DATA2` uint32 page-table index; `INSTANCE_COLOR` unorm8x4 tint (sRGB-encoded RGB, linear alpha) | 40 |
| `0x3002` | `sprite:world` | `INSTANCE_RECT` float32x4 (origin x, y, z, basis X x) in metres; `INSTANCE_DATA0` float32x4 (basis X y, basis Y x, basis Y y, 0); `INSTANCE_DATA1` uint16x4 texel rectangle; `INSTANCE_DATA2` uint32 page-table index; `INSTANCE_COLOR` unorm8x4 tint | 48 |
| `0x3003` | `sprite:tilechunk` | `INSTANCE_RECT` float32x4 (cell (0, 0) of the chunk: x, y, z, 0) in canvas texels or metres; `INSTANCE_DATA0` float32x2 cell size; `INSTANCE_DATA1` uint32 chunk-pool slot; `INSTANCE_DATA2` uint32 tile-table base; `INSTANCE_COLOR` unorm8x4 tint | 36 |

```slang
// Zenith2/Shaders/Common/SpriteStreamSets.slang
[StreamSet(0x3001)] struct Flux_SpriteCanvasInstance
{
	[PerInstance] [Stream(4)] [VtxFmt("float32x4")] float4 m_xOriginBasisX : INSTANCE_RECT;
	[PerInstance] [Stream(4)] [VtxFmt("float32x2")] float2 m_xBasisY : INSTANCE_DATA0;
	[PerInstance] [Stream(4)] [VtxFmt("uint16x4")] uint4 m_xTexelRect : INSTANCE_DATA1;
	[PerInstance] [Stream(4)] [VtxFmt("uint32")] uint m_uPage : INSTANCE_DATA2;
	[PerInstance] [Stream(4)] [VtxFmt("unorm8x4")] float4 m_xTint : INSTANCE_COLOR;
};
// Flux_SpriteWorldInstance (0x3002) and Flux_SpriteTileChunkInstance (0x3003) follow the table above in the same form.
```

- Shader input locations: `INSTANCE_RECT` 8, `INSTANCE_DATA0` 9, `INSTANCE_DATA1` 10, `INSTANCE_DATA2` 11, `INSTANCE_DATA3` 12 (unused), `INSTANCE_COLOR` 13 (GEOM's table "8 onwards, assigned by the owning family").
- No vertex-rate stream exists: the vertex shader derives the corner `(u, v) = (vertexID & 1, vertexID >> 1)` of the quad from the index buffer's vertex IDs. Stream 7 is never used (GEOM's `ZSC_ERROR_STREAM_SET_LOCATION` rule, `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`).
- These declarations replace the illustrative `Flux_SpriteInstance` of `Zenith2/Docs/Rendering/VertexData.md → "Vertex declaration in Slang"` (cross-doc item in "Cross-area reconciliation" of the entry document).
- `Test_SpriteStreamSet_LayoutsMatchTable` (ZenithShaderCompiler tests) asserts the generated layout entries of the three codes against this table.

## Tile tables and the chunk pool

The renderer keeps every tile chunk and tileset the extractors sent in CPU mirrors, and keeps on the GPU only the chunks that views need, in a fixed pool with least-recently-wanted eviction. The extractor sends each chunk once per change ("Extraction" step 8); residency is decided here alone, so it never needs feedback to simulation.

| Store | Element | Capacity (desc member) | Allocation |
|---|---|---|---|
| chunk mirror (CPU, `MEMORY_SUBSYSTEM_RENDER` heap of the renderer) | one chunk = 1,024 `uint32` cells (4,096 bytes) plus its content hash | `m_uTileChunkMirrorChunks` (default 8,192 = 32 MiB), reserved at `Enable2D` | a slot per (map key, layer, chunk x, chunk y) from a free list; an open-addressed hash map (capacity 2 × mirror chunks, `Zenith_Hash64` of the 24-byte key) finds it |
| chunk index (CPU) | per (map key, layer): an array of `{ sint32 cy, cx; uint32 mirrorSlot; uint32 gpuSlot (0xFFFFFFFF none); uint64 lastWantedFrame; }` sorted by (cy, cx) | grows with the mirror; reserved at `Enable2D` to the mirror capacity in total | inserted and removed with the mirror slot, O(n) per change |
| chunk pool (GPU) | one chunk = 1,024 cells, row-major from the chunk's bottom row (4,096 bytes) | `m_uTileChunkPoolChunks` (default 4,096 = 16 MiB) | lowest free slot; eviction below |
| tile table (GPU, with its CPU mirror) | `Flux_TilesetTileEntry`, 24 bytes | `m_uTileTableEntries` (default 262,144 = 6 MiB) | one contiguous range per tileset, first-fit over a sorted free-range list; a tileset needs `1 + tiles + animation frames` entries |

Both GPU buffers are created at `Enable2D` and imported into every frame's graph as ordinary graph buffers (the pool shape of `Zenith2/Docs/Rendering/GpuResources.md → "Pools"`), never reached through the GPU-scene tables.

- **Applying records** (`PrepareFrame` (a)). UPSERT: find or take the chunk's mirror slot and copy the cells; when the chunk is GPU-resident and the content hash changed, queue it for re-upload at the front of the upload queue; a full mirror refuses the record (`flux.twod.mirror_full_delta`; the chunk draws empty until a subsequent UPSERT finds room). `REMOVE_CHUNK` frees the mirror slot, the index entry and the GPU slot; `REMOVE_MAP` does so for every entry of every layer of the map key. A tileset record writes its CPU tile-table mirror and queues its range for upload; a removal frees the range.
- **Wanted set** (the residency pass, once per frame after (a)). For each canvas view and each tilemap draw of its section (canvas space), the visible chunk range is the canvas rectangle mapped to the layer's map-local cell space: canvas texels `[0, Wc + 2m) × [0, Hc + 2m)` → camera-relative metres by the inverse of "Snapping" (`o.x = (X − rx − cX) / s`, `o.y = −(Y + ry − cY) / s`), minus the draw's parallaxed origin (rule "Parallax"), divided by the cell size and by 32, floored; the wanted range is the visible range grown by one chunk on every side. A world-space tilemap drawn on a pure-2D view's canvas ("Frame data path" (d)) uses this canvas rule. Each tilemap draw a non-pure-2D view draws (`Sprite2DWorld`: the section's world list, or every draw of the section in a view without a canvas, "Frame data path" (e), which is how a canvas-space map reaches the editor's scene view) uses the footprint rule: the wanted range is the chunks whose cell rectangle intersects the view frustum's footprint on the layer's plane z = `z_l` (the draw's absolute origin z), plus one chunk on every side. The matrix is `VP = m_xProjection · m_xView` of the request's `Flux_ViewCamera` (`Zenith2/Docs/Rendering.md → "Flux_View"`), with the unjittered projection, inverted in binary64; it maps absolute world points. The eight frustum corners are `VP⁻¹ · (±1, ±1, d, 1)` for the request's near and far NDC depths `d` (reversed Z included), each divided by its w; for an infinite far plane (w = 0 at the far corners) the four side edges are rays from the near corners in the far corners' homogeneous directions. Each of the twelve edges (four near, four far when finite, four side) that crosses `z = z_l` contributes its intersection point; the footprint is the axis-aligned rectangle of those points (an empty set: the layer is outside the frustum and nothing of it is wanted). When the far plane is infinite and at least one side ray does not reach `z = z_l` while another edge does, the plane's visible region is unbounded and the footprint is the map's authored bounds; the nearest-first upload order below then bounds the cost. The footprint is clipped to the map's authored cell bounds and converted to map-local cells by subtracting the draw's absolute origin (the list's reference point `(cx, cy)` plus the draw's stored `m_afOrigin`, formed in binary64 exactly as "Frame data path" (e) forms world sprite instances; the reference point is (0, 0) for a section without a canvas), dividing by the cell size and by 32, floored. Every resident or mirrored chunk inside a wanted range gets `lastWantedFrame` = this frame.
- **Uploads.** Wanted chunks that are mirrored but not GPU-resident are queued in the order (visible before margin, squared distance in chunks from the canvas centre, or for a world view from the camera position projected onto the layer's plane, map key, layer, cy, cx); up to `m_uMaxChunkUploadsPerFrame` are uploaded this frame through the scatter list `{ source offset, destination buffer, destination offset, words }` that the "Sprite2DScatter pass" applies. A chunk takes the lowest free GPU slot; with none free it evicts the resident chunk with the oldest `lastWantedFrame` that is older than this frame (ties to the lowest key), counting `flux.twod.chunks_evicted_delta`; when every resident chunk is wanted this frame the upload is refused (`flux.twod.chunks_refused_delta`) and retried next frame. A wanted chunk not yet resident draws empty that frame (`flux.twod.chunks_pending_delta`).
- **Chunk instances.** For a tilemap draw in a canvas: walk the draw's (map key, layer) index in ascending (cy, cx), restricted to the visible range by binary search on cy, and emit one 36-byte chunk instance per GPU-resident entry, in that order; all chunk instances of one draw share its sort key and stay consecutive. For a draw in a world view: every resident entry inside the draw's wanted range, same order, each instance's origin in metres staged from the draw's absolute origin (reference point plus stored origin in binary64, narrowed), as world sprite instances are.
- **Parallax.** A layer with parallax `(px, py)` is drawn at the origin `o + ((1 − px)·cx, (1 − py)·cy)` (camera-relative, binary64, `(cx, cy)` the list's reference point), then mapped and snapped by "Snapping" like any record, so a layer of parallax 0 stays fixed on the canvas and a layer of 1 moves with the world; in world views parallax is ignored (treated as 1). Opacity reaches the GPU as the tint's alpha ("Extraction" step 6).
- **Sizing.** A canvas of `Wc × Hc` texels with cell size `w` texels wants at most `(⌈(Wc + 2m) / 32w⌉ + 3) × (⌈(Hc + 2m) / 32h⌉ + 3)` chunks per drawn layer; the pool default holds that for 16 layers at 4,096 × 4,096 texels with 16-texel cells (16 × 11 × 11 = 1,936 chunks), so refusal needs a configuration the defaults never produce. A world view wants only its frustum footprint, so a world-space map of up to `uTWOD_MAX_TILEMAP_CHUNKS` (16,384) chunks is never wanted whole unless the frustum covers it; a footprint that still exceeds the pool (a low grazing camera over a vast map) uploads its nearest chunks first by the order above and counts the rest refused, recovering as the view moves, never a permanent truncation of a fixed subset. The Game2D content scale (row TD17) records the slice's peak mirrored and wanted chunk counts.
- **Retirement.** A freed GPU slot or range is reused only after every queue completed the frames that read it (`Zenith2/Docs/Rendering/GpuResources.md → "Retirement"`): the renderer tags freed slots with the frame index and returns them to the free list once `PrepareNextFrame` reports that frame retired.
- **Device loss.** `OnDeviceRecovered` empties the GPU pool's and tile table's slot maps and marks every index entry non-resident; the mirrors are CPU memory and survive, so the residency pass re-uploads wanted chunks at the paced rate and every tileset range at once, without needing the full-state extraction ([Z2-N-REND-015]) that the recovery also produces (its UPSERTs only refresh the mirror).

`Test_Sprite2DRender_PoolPressureRecovers` configures a 64-chunk pool, a 400-chunk map and a camera that pans across it and back: it asserts every visible chunk draws its cells within `⌈wanted / maxChunkUploadsPerFrame⌉` frames of becoming wanted, evictions only of unwanted chunks, zero refusals while the wanted set fits, refusals counted when a 100-chunk wanted set is forced, and full recovery when the camera returns. `Test_TilemapDraw_ParallaxFormula` draws a layer at parallax (0.5, 0) under a moving camera and asserts its chunk instances against the formula. `Test_Sprite2DRender_WorldTilemapWantsFootprint` asserts the world-view footprint rule on a 16,384-chunk world-space map.

## Sprite2DScatter pass

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Sprite2D/Sprite2DScatter.slang` → `Flux_Sprite2DScatterParams` |
| Feature function | called by `Flux_AddSprite2DCanvas` and `Flux_AddSprite2DWorld`, whichever runs first in the frame; declared once per frame |
| Kind | compute, one thread per 16-byte word of the scatter payload, 64 threads per group |
| Queue | G |
| Capability | all tiers |
| Budget row | the pending row "2D sprite passes GPU" (proposed ID TWOD-002, "Budget rows" of the entry document) |
| Permutations | none |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xFrameChunk` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the frame chunk) | read | compute |
| `m_xChunkPool`, `m_xTileTable` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | write | compute |
| data | `uint32 m_uRunBase`, `m_uRunCount`, `m_uWordCount` | — | — |

The pass adds nothing in a frame whose scatter list is empty; its writes of the pool and table precede every reader by the graph's hazard edges.

## Sprite2DCanvas pass

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Sprite2D/Sprite2D.slang` → `Flux_Sprite2DParams` (sprite batches) and `Zenith2/Shaders/Passes/Sprite2D/Sprite2DTiles.slang` → `Flux_Sprite2DTilesParams` (chunk batches), both in the pass's declared shader set |
| Feature function | `Flux_AddSprite2DCanvas` (the canvas extension at `FLUX_EXTENSION_POINT_BEFORE_UI`) |
| Kind | raster; one direct `DrawIndexedInstanced` per batch, in batch order; no depth attachment |
| Queue | G |
| Capability | all tiers; main view (and the editor game view, row TD8) |
| Budget row | the pending row "2D sprite passes GPU" (proposed ID TWOD-002) |
| Permutations | `uPERM_BLEND` {ALPHA, PREMULTIPLIED, ADDITIVE, MULTIPLY} (raster blend state, not a shader permutation: one PSO per blend), `bPERM_CANVAS` true |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xPageTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (frame chunk range: per page `{ Zenith_RHIDescriptorHandle srv; uint32 width, height, sampler; }`) | read | vertex, pixel |
| `m_xChunkPool`, `m_xTileTable` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (tiles program) | read | pixel |
| data | `Zenith_RHIFloat4x4 m_xCanvasProjection` (canvas texels to clip: x' = 2x/W − 1, y' = 1 − 2y/H), `uint32 m_uAnimationTimeMs` (the list's tick × tick duration, modulo 2^32), `uint32 m_uFlags` | — | — |

| Target | Format | Load / store |
|---|---|---|
| the canvas, a transient `Flux_RGTexture` of `(Wc + 2m) × (Hc + 2m)` | `R8G8B8A8_SRGB` | `CLEAR` to the canvas record's clear colour premultiplied (`(decode(rgb) × a, a)` in linear light from `m_uClearSrgb`), `STORE` |

The canvas holds premultiplied linear colour by construction. The pixel stage outputs `(c × a, a)` for ALPHA, ADDITIVE and MULTIPLY input (straight-alpha texels times the decoded tint) and the texel's own premultiplied `(c, a)` times the tint (`c × t.rgb × t.a`, `a × t.a`) for PREMULTIPLIED, except that MULTIPLY outputs `(lerp(1, c, a), a)`; the fixed-function blend state, one PSO per mode, is:

| Blend mode | Colour source, destination factor | Alpha source, destination factor | Result |
|---|---|---|---|
| ALPHA, PREMULTIPLIED | `ONE`, `ONE_MINUS_SRC_ALPHA` | `ONE`, `ONE_MINUS_SRC_ALPHA` | premultiplied over: colour `c·a + d·(1 − a)`, coverage `a + d.a·(1 − a)` |
| ADDITIVE | `ONE`, `ONE` | `ZERO`, `ONE` | colour `c·a + d`; coverage unchanged, so additive light over a transparent clear composites additively over 3D |
| MULTIPLY | `DST_COLOR`, `ZERO` | `ZERO`, `ONE` | colour `d · lerp(1, c, a)`; coverage unchanged: it multiplies canvas content only, and over the transparent clear (premultiplied black) it changes nothing |

`Test_Sprite2DRender_CanvasOverScene` draws a sprite of alpha 0.5 in each mode over a 3D scene colour through the over composite and compares every pixel with a linear-light oracle computed in the test (within 1/255 after encoding).

- The sprite program's vertex stage places corner `(u, v)` at `origin + u·basisX + v·basisY` (y negated into the canvas's +y-down space by the projection), outputs the texel coordinate `lerp(rect.x0, rect.x1, u)`, `lerp(rect.y1, rect.y0, v)` and the page index; the pixel stage samples the page with the page's static sampler at `texel / (width, height)`, multiplies by the decoded tint and writes the premultiplied output of the table above. It never `discard`s ([Z2-N-REND-916] binds opaque pipelines; these are blended, depth-less pipelines and need no discard); a transparent texel writes alpha 0 through the blend. The raster state culls nothing: a flipped sprite's basis reverses the quad's winding, and [Z2-N-REND-923] binds opaque and alpha-tested PSOs, which no 2D PSO is.
- The tiles program draws one quad per chunk instance covering 32 × 32 cells; the pixel stage computes the cell `(⌊lx / cellSize.x⌋, ⌊ly / cellSize.y⌋)` from the local position, reads the cell from the chunk pool (`slot × 1024 + y × 32 + x`), returns transparent for tile 0, applies the cell's flip and transpose bits to the in-tile coordinate, resolves an animated tile's frame `(timeMs / frameMs) mod frameCount`, reads the tile entry at `base + tile` and samples its page.
- Exactly one canvas pass per canvas view; nothing is declared for a view without a canvas or with an empty canvas list (the composite still runs when the view's TONEMAP bit is clear, to fill the output, "Sprite2DComposite pass").

## Sprite2DComposite pass

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Sprite2D/Sprite2DComposite.slang` → `Flux_Sprite2DCompositeParams` |
| Feature function | `Flux_AddSprite2DCanvas` |
| Kind | fullscreen over the view's viewport |
| Queue | G |
| Capability | all tiers; HDR10 and scRGB from Stage 5 with LIGHT's output modes |
| Budget row | the pending row "2D canvas composite GPU" (proposed ID TWOD-003) |
| Permutations | `uPERM_OUTPUT` {SDR, HDR10, scRGB}; `bPERM_OVER` {false: overwrite, the view's TONEMAP bit is clear; true: blend over, the bit is set} |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xCanvas` | `Zenith_RHISRV<Zenith_RHITex2D>` | read (`Load`) | pixel |
| data | `sint32 m_iDestX`, `m_iDestY`, `uint32 m_uDestWidth`, `m_uDestHeight`, `m_uScale`, `m_uMargin`, `m_uOffsetX`, `m_uOffsetY`, `float m_fDownscale`, `Zenith_RHIFloat4 m_xLetterbox` (linear), `float m_fNitsForOne` (`r.hdr.uiNits`), `uint32 m_uOutputMode` | — | — |

| Target | Format | Load / store |
|---|---|---|
| view output (`xTargets.m_xOutput`) | as "TonemapAndGrade" of `Zenith2/Docs/Rendering/PerPass/Common.md` | `bPERM_OVER` false: `DONT_CARE` with `FullCoverage()`; true: `LOAD`; `STORE` |

Per output pixel `(px, py)` of the viewport: `(qx, qy) = (px − dx, py − dy)` as signed 32-bit integers; outside `[0, Wd) × [0, Hd)` the pixel is the letterbox colour (overwrite) or left unchanged (over); inside, the canvas texel `(floor_div(qx + ox, k) + m, floor_div(qy − oy, k) + m)` of "Snapping" is loaded, and under rule 5 the texel `(min(⌊(float)qx / f⌋, Wc − 1) + m, min(⌊(float)qy / f⌋, Hc − 1) + m)` with the SUBPIXEL offsets ignored (a downscaled canvas is already off the pixel grid; the clamp absorbs a binary32 rounding up at the last pixel). The loaded premultiplied value is written through `Flux_EncodeOutput(rgb, fNitsForOne, uOutputMode)` (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"`), in SDR the plain linear value the sRGB view encodes: the overwrite form writes it as is (the canvas composited over black), the over form blends `dst = src + dst × (1 − src.a)` with blend state `ONE`, `ONE_MINUS_SRC_ALPHA`. Index arithmetic is signed 32-bit integer with `floor_div`, and every computed index lies in `[0, Wc + 2m) × [0, Hc + 2m)`. `Test_Sprite2DComposite_SubpixelMarginRows` sets `oy = k − 1` at k = 6 and asserts that output rows 0 … `oy − 1` read canvas row 0 and that the last output column reads the right margin column.

## Sprite2DWorld pass

| Property | Value |
|---|---|
| Program | `Sprite2D.slang` and `Sprite2DTiles.slang` with `bPERM_CANVAS` false |
| Feature function | `Flux_AddSprite2DWorld` (the world extension at `FLUX_EXTENSION_POINT_TRANSPARENT`) |
| Kind | raster; one `DrawIndexedInstanced` per batch in sort order |
| Queue | G |
| Capability | all tiers; main, editor and texture views |
| Budget row | the pending row "2D sprite passes GPU" (proposed ID TWOD-002) |
| Permutations | `uPERM_BLEND` as the canvas pass; the depth comparison from the view's `Flux_DepthModeOps::GetColourCompare`'s less-or-equal form for standard Z and its mirror for reversed Z (`Zenith2/Docs/Design/RPROD/Views.md → "Flux_DepthMode header"`) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| as the canvas pass, plus `Zenith_RHIFloat4x4 m_xViewProjection` (the view's jittered view-projection) | | | |

| Target | Format | Load / store |
|---|---|---|
| `xTargets.m_xSceneColour` | `R11G11B10F` | `LOAD`, `STORE` |
| `xTargets.m_xDepth` | the view's depth format | `READ_ONLY`, `LOAD` |

- World sprites draw unlit in linear light, depth-tested and not depth-written, in sort order (never by depth), with the colour blend factors of the canvas pass's table (the scene colour `R11G11B10F` has no alpha channel); they write no velocity, so a view with TAA shows ghosting on fast-moving world sprites (documented limitation; 2D views clear `FLUX_VIEW_FEATURE_TEMPORAL_AA` through `uTWOD_PURE_2D_FEATURE_CLEAR_MASK`).
- `Flux_AddSprite2DWorld` declares nothing in a pure-2D view: that view's scene colour is never presented (its TONEMAP bit is clear), so its world-space records are drawn on the canvas instead ("Frame data path" (d)). `Test_Sprite2DRender_WorldSpriteInPure2DView` draws a `SPRITE2D_SPACE_WORLD` sprite in a view with the pure-2D mask and asserts it appears in the output at its sort position.
- A view with no canvas draws the full list (canvas-space records at their world positions, unsnapped), which is how the editor's scene view shows a 2D game's canvas content.

## Shader programs

| File | Entry points | Generated header |
|---|---|---|
| `Zenith2/Shaders/Common/SpriteStreamSets.slang` | stream-set structs (no entry point) | `Flux_StreamSets.generated.h` entries 0x3001–0x3003 |
| `Zenith2/Shaders/Passes/Sprite2D/Sprite2D.slang` | `Sprite2DVS`, `Sprite2DPS` | `Flux/Shaders/Sprite2D.generated.h` |
| `Zenith2/Shaders/Passes/Sprite2D/Sprite2DTiles.slang` | `Sprite2DTilesVS`, `Sprite2DTilesPS` | `Flux/Shaders/Sprite2DTiles.generated.h` |
| `Zenith2/Shaders/Passes/Sprite2D/Sprite2DComposite.slang` | `Sprite2DCompositePS` (with the fullscreen triangle vertex stage) | `Flux/Shaders/Sprite2DComposite.generated.h` |
| `Zenith2/Shaders/Passes/Sprite2D/Sprite2DScatter.slang` | `Sprite2DScatterCS` | `Flux/Shaders/Sprite2DScatter.generated.h` |
| `Zenith2/Shaders/Common/Sprite2DCommon.slang` | `Sprite2D_DecodeTint`, `Sprite2D_ApplyCellFlips`, `Sprite2D_CanvasTexel` (shared helpers, also the CPU reference's formulas) | — |

- Colour, tint and blend arithmetic use `half` (native 16-bit, [Z2-N-REND-922]); positions, texel coordinates, cell arithmetic and the composite's indices are `float` or `uint`.
- PSO set: canvas 2 programs × 4 blends = 8; world 2 × 4 × the view's depth configurations (standard D24S8, standard D32F_S8, reversed D32F_S8) = 24; composite 3 × 2 = 6; scatter 1: 39 PSOs, every one in the game's recorded precache set (`Zenith2/Docs/Rendering/ShaderSystem.md → "PSO precache sets"`).

## Colour and HDR output

| Output mode | Canvas | Composite writes | Exact |
|---|---|---|---|
| SDR (`B8G8R8A8_SRGB`) | `R8G8B8A8_SRGB` | the linear value; the sRGB view encodes | yes, under "Pixel exactness rules" |
| HDR10 (`A2B10G10R10_UNORM`) | as SDR | `Flux_EncodeOutput(linear, r.hdr.uiNits, HDR10)`: PQ at the UI paper white | no; a sign-off item of the Game2D slice (Stage 5) |
| scRGB (`R16G16B16A16_SFLOAT`) | as SDR | `Flux_EncodeOutput(linear, r.hdr.uiNits, SCRGB)` | no; Stage 5 |

2D content is authored SDR; in HDR modes it is placed at the UI paper white (`r.hdr.uiNits`), the same brightness LIGHT gives UI, so a 2D game's sprites and its UI match.

## View extensions and view kinds

The module registers two `Flux_ViewExtension`s at module start step 7 (`Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`):

| Extension (Zenith2D `Internal/Zenith_Sprite2DExtensions.cpp`) | `GetPoint` | `GetViewKindMask` | `AddPasses` |
|---|---|---|---|
| `Zenith_Sprite2DCanvasExtension` | `FLUX_EXTENSION_POINT_BEFORE_UI` (new point 5, row TD2) | MAIN, EDITOR | `Flux_AddSprite2DCanvas` |
| `Zenith_Sprite2DWorldExtension` | `FLUX_EXTENSION_POINT_TRANSPARENT` | MAIN, EDITOR, TEXTURE | `Flux_AddSprite2DWorld` |

- Both functions find the view's 2D data by the view's source world tag (`Flux_ViewContext` member pending RPROD, the same row UI asked for, UI8 (d)) and the view ID.
- A view is **pure-2D** when its effective feature mask (after its camera's `FeatureMaskSet` and `FeatureMaskClear`) has none of the bits of `uTWOD_PURE_2D_FEATURE_CLEAR_MASK`: `(uFeatureMask & uTWOD_PURE_2D_FEATURE_CLEAR_MASK) == 0`. This is the one definition every TWOD text and row TD7 use; a camera whose `FeatureMaskClear` is the mask produces such a view.
- `Flux_AddSprite2DCanvas` on a view whose `TONEMAP` bit is clear always declares the composite, even with no canvas (letterbox colour black, nothing inside), so the output always has a full-coverage first writer when the 2D module runs; with the bit set and no canvas it declares nothing.
- Capture, preview and shadow views draw no 2D content. Stereo is Stage 7 and outside TWOD.

<!-- assumption: pending RPROD (rows TD1, TD2, TD7) -->
RPROD adds `FLUX_EXTENSION_POINT_BEFORE_UI = 5` to `Flux_ExtensionPoint`, calls the extensions registered there from `Flux_BuildCommonEpilogue` after `TonemapAndGrade` (or where it would run when the TONEMAP bit is clear) and before `Flux_AddUIComposite`, declares `Flux_Renderer::Enable2D` and the owned member, calls `PrepareFrame` at `Flux.BuildGraph` step 4, `WriteFrame` at step 11 and `EndFrame` at step 13, sets `Flux_FrameShared::m_px2DFrame`, and exposes the view's source world tag on `Flux_ViewContext`; a pure-2D view (the definition above) compiles with every 3D pass culled.
<!-- /assumption -->

## Headless and device loss

- Headless (no `Flux_Renderer`): module start step 7 registers no extension and calls no `Enable2D`; the extractor still runs and the sealed world is never consumed ("Extraction").
- Device loss: `Flux_Sprite2DRenderer::OnDeviceRecovered` drops the GPU chunk pool and tile table contents and keeps the CPU mirrors; the residency pass re-uploads every tileset range in the first recovered frame and the wanted chunks at `maxChunkUploadsPerFrame` per frame, so a wanted chunk draws empty for at most `⌈wanted chunks / maxChunkUploadsPerFrame⌉` frames; the full-state world the recovery requests ([Z2-N-REND-015]) re-sends every chunk to the mirror, which changes nothing when the cells are unchanged.
- Swapchain recreation changes the output extent: the next frame's canvas geometry follows; no 2D history exists, so nothing is invalidated.

## Render failure behaviour

| Failure | Behaviour | Counter |
|---|---|---|
| page texture not resident, failed or null | the white usage fallback for that frame | `flux.twod.missing_page_delta` |
| more than 4,096 pages in a frame | the excess pages draw the fallback | `flux.twod.page_table_full_delta` |
| sprite list over `m_uMaxSpritesPerFrame` | whole lists dropped from the highest world tag | `flux.twod.sprites_dropped_delta` |
| more canvas views than `m_uMaxCanvasesPerFrame` | the excess views draw canvas content as world | `flux.twod.canvas_limit_delta` |
| chunk mirror full | the refused chunk draws empty until a subsequent UPSERT finds room | `flux.twod.mirror_full_delta` |
| GPU chunk pool full of wanted chunks | the upload waits a frame; the chunk draws empty meanwhile | `flux.twod.chunks_refused_delta`, `flux.twod.chunks_pending_delta` |
| an unwanted resident chunk evicted | none visible | `flux.twod.chunks_evicted_delta` |
| tile table full | the tileset's maps draw empty | `flux.twod.tile_table_full_delta` |
| tilemap draw for an unknown map or tileset key | nothing drawn | `flux.twod.tilemap_unknown_delta` |
| output smaller than the reference | rule 5 downscale | `flux.twod.canvas_downscaled_delta` |
| render-world slot full | the world draws no 2D content | `twod.extract.slot_full_delta` |
| a deferred or failed frame | no 2D bytes written; slot changes rolled back and re-queued | `Flux.Upload.FramesDeferred` (RHI's) |
