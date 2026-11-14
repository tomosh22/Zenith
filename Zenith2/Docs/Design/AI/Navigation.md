# AI design: navigation meshes and paths

Section file of `Zenith2/Docs/Design/AI.md`. It specifies the navigation data model, the bake pipeline and its stage contracts, the runtime tile table (streaming, connection, removal, origin shift), dynamic obstacles (mark and carve), off-mesh links, the path search and its determinism rules, the path service that runs every path request inside the frame graph, the spatial queries, and the D-SEL-NAV (open, Stage 1/2) selection harness and candidate mapping. Volumes (flying and underwater navigation) are `Zenith2/Docs/Design/AI/Volumes.md`; byte layouts are `Zenith2/Docs/Design/AI/Formats.md`.

## Scope of this file

<!-- informative -->
The old engine baked one `.znavmesh` per scene from static colliders with a Recast-style pipeline whose polygon stage emitted one quad per span, searched it with an unsliced A* that blocked the calling thread for batched queries (`Zenith_Pathfinding::FindPathsBatch`), carved doors by toggling a BLOCKED flag on polygons, and drew random points from a per-instance xorshift stream (`Zenith/AI/Navigation/CLAUDE.md`). Zenith2 keeps the stage sequence and the blocked-flag idea and changes the rest: tiles instead of one mesh, so streaming, carving and incremental cooking work per tile; a neutral, versioned tile format the selected library reads and writes; searches sliced inside a frame-graph node with a deterministic budget, so no job waits and the publication tick of every result is a pure function of the simulation; and randomness from the counter-based streams.
<!-- /informative -->

## Navigation data model

### Tile grid and tile keys

- A ground class's navmesh is a set of tiles on a square grid of edge `m_fTileSize` (16, 32, 64 or 128 m, so a tile never straddles an ENV 128 m cell, `Zenith2/Docs/Design/ENV/Bake.md` → "Environment cells and STREAM cells") whose origin is the absolute world origin: tile (tx, tz) covers absolute x in `[tx × size, (tx + 1) × size)` and z likewise.
- Several tiles may exist at one (tx, tz) for one class: a multi-storey column bakes into layers 0 … `uAI_NAV_MAX_LAYERS_PER_COLUMN` − 1 when the class partition is `NAV_PARTITION_LAYERS` (stage 6), numbered by ascending minimum height; with the other partitions a column has exactly one tile, layer 0.
- `Zenith_NavTileKey` (class, layer, tx, tz) is the tile's identity in files, saves, sort orders and tie-breaks; `Pack()` gives the 64-bit total order.
- Tile coordinates are 16-bit signed: ±32,768 tiles of 16 m reach ±524 km, beyond every world `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems" and STREAM's large-coordinate handling admit.

### Polygon references

- A loaded tile occupies one slot of the world's tile table (`ai.nav.maxTiles` slots, at most `uAI_NAV_MAX_TILES_PER_WORLD`); each slot carries a 16-bit salt that starts at 1 and increments (wrapping 65,535 → 1) when the slot is freed, so a `Zenith_NavPolyRef` naming a freed or re-used slot is stale.
- Slot allocation takes the lowest free slot; slot numbers depend on load order, which no result may depend on: every ordering and tie-break uses `(Zenith_NavTileKey::Pack(), polygon index)` ("Tie-breaking"), and saves store `(tile key, polygon index)` with the tile's content hash (`Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm").
- A carve swap replaces a tile's polygon data in place: the slot is kept and the salt incremented, so every reference into the pre-carve polygons becomes stale at once and holders re-resolve by position ("Carve procedure").

### Agent classes

- A world has at most `uAI_MAX_GROUND_CLASSES` (4) ground classes and `uAI_MAX_VOLUME_CLASSES` (2) volume classes, defined once in the game's navigation settings document (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation settings document") and cooked into its SETTINGS file.
- Every TILE, VOLUME and MANIFEST file names the settings asset (persistent ID) and the settings hash (`Zenith_Hash64` of the SETTINGS file's class, area and profile sections in that order) and declares a hard dependency on it, so ZenithAssets makes the settings asset resident before any of its tiles.
- The first navigation file a world accepts fixes the world's settings (classes, area table, cost profiles): class index i is the i-th class of the settings file (ground classes first). A file naming another settings asset or hash is refused with `AI_ERROR_SETTINGS_MISMATCH` (counted `ai.nav.settings_mismatch`, logged once per asset); the world keeps its settings until it is destroyed.
- Agents name their class by `ClassName`; it resolves through `Zenith_NavMesh::FindClass` once settings exist, and until then (or for an unknown name) the agent is SUSPENDED with `PathStatus` `NO_NAVMESH` (`Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine").

### Area codes and costs

| Code | Name | Source | Baked flags | Default cost |
|---|---|---|---|---|
| 0 | `ground` | ENV layer area, static colliders | WALK | 1.0 |
| 1 | `road` | ENV layer area | WALK | 1.0 (profiles may prefer roads by raising other costs) |
| 2 | `offPath` | ENV layer area | WALK | 2.0 |
| 3 | `unwalkable` | ENV layer area, slope | input only: becomes `uAI_NAV_AREA_NULL` | — |
| 4 | `water` | ENV water bodies | SWIM | +∞ for walkers (profile 0), 1.0 for swimmers |
| 5 … 62 | game areas | the settings document's area table, area volumes, obstacles | the area's declared flags (WALK, SWIM, DOOR, JUMP) | the area's declared default |
| 63 | null | bake mark | never stored | — |

- Codes 0 … 4 equal ENV's `Zenith_NavigationArea` values (`Zenith2/Docs/Design/ENV/Bake.md` → "Area flags"); a game area name may not repeat a fixed name.
- A cost profile is an array of 64 costs plus include and exclude flags (`Zenith_NavQueryFilter`); profile 0 is the area table's defaults, profiles 1 … 7 are named in the settings document. An agent's `CostProfile` selects its filter; a script request names a profile by name.
- Costs below 1.0 are refused by `Zenith_NavQueryFilter::Validate`, so the heuristic stays admissible ("Cost and heuristic").

## Bake pipeline

### Bake inputs

A tile bake (the cook step `ai.navMesh` for one `navTile:<class>/<tileSize>/<tx>_<tz>_<layer>` product, `Zenith2/Docs/Design/AI.md` → "Cook steps and derived-data keys") gathers, inside the tile's bounds grown by the border (`Zenith_NavGetTileBorderCells` × cell size on each xz side, and the navigation document's vertical bounds):

| Input | Source | Area code |
|---|---|---|
| terrain triangles | the SOURCE_GEOMETRY files of ENV's `navigation:<x>_<z>` products the navigation document's terrains cover, read with `ReadDependencyOutput` (published property `ai.navSource`) | the per-triangle ENV code |
| static collider geometry | every entity of the listed scenes whose PHYS static collider component references a collision shape asset: its neutral geometry (`Zenith2/Docs/Design/PHYS/Formats.md` → "Neutral geometry section") transformed by the entity's world transform, read with `ReadDependencyOutput` | `ground`, or the navigation document's per-physics-material mapping `areaByMaterial` |
| blockers | static colliders whose layer is in the navigation document's `blockerLayers` | `unwalkable` |
| area volumes | `Zenith.AI.NavAreaVolume` components of the listed scenes whose `ClassMask` has the class bit | the named area |

- Dynamic bodies, triggers, character controllers and colliders on layers in `ignoredLayers` are never inputs.
- A `scenes` element of the navigation document is one of two forms (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema"): a **path** names a scene source document, read with `Zenith_CookContext::OpenSourceDocument`, whose entity positions are absolute; a **product entry** `{ "scene": <GUID>, "offset": [x, y, z] }` names a cooked scene product (WORLD's anchor-relative `navInput:<x>_<z>` scenes, row WD23), read as that product's cooked output with `ReadDependencyOutput(GUID, "")` and decoded with SCENE's `Zenith_DecodeCookedSceneFile` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneCooker header"), whose entity positions are relative to `offset` (absolute metres, binary64). Either way entity world transforms are composed from the scene's local transforms down the hierarchy (`Zenith2/Docs/Design/AI.md` → "Assumed interfaces", SCENE row), and every opened document and dependency output enters the step's key, so editing a collider re-bakes exactly the tiles it touches.
- Triangles are translated into tile-local coordinates in `double` — for a path scene `absolute − tile origin`, for a product entry `(offset + composed position) − tile origin`, the addition and the subtraction both in binary64 — then narrowed to `float` once; so precision is that of a 128 m frame wherever the tile lies, and an anchor-relative scene bakes at its world-absolute place.
- Inputs are sorted before stage 1 by (source kind, source persistent ID, triangle index), so the bake's output does not depend on scene enumeration order.

### Bake stages

Each stage's contract is fixed here; its implementation is the D-SEL-NAV winner's ("Candidate mapping"). Cells are `cs × cs` columns (`m_fCellSize`), heights are integer multiples of `ch` (`m_fCellHeight`); W and H are the tile's cell counts including the border: `ceil(tileSize / cs) + 2 × border`.

| # | Stage | Input → output | Rule | Complexity |
|---|---|---|---|---|
| 1 | Rasterize | triangles → span heightfield (per column a list of solid spans `[smin, smax]` in ch units, 13-bit each, with an area) | a triangle is walkable when its unit normal's y ≥ `(float)Zenith_Cos(maxSlope)` (CORE's deterministic cosine, computed once per class, ruling X314-FMA3); conservative clipping of each triangle into the columns it overlaps; spans of one column merge when they overlap; when two merged spans' tops are within `ceil(maxClimb / ch)` the higher-priority area wins (priority = area code, null lowest) | O(Σ cells covered per triangle) |
| 2 | Filter | heightfield → heightfield | (a) low-hanging obstacles: a non-walkable span whose top is within climb of the walkable span below becomes walkable; (b) ledges: a walkable span with a neighbour drop greater than climb, or whose reachable neighbours' heights differ by more than climb, becomes null; (c) low clearance: a walkable span whose clearance to the next span is below `ceil(agentHeight / ch)` becomes null | O(W × H × spans per column) |
| 3 | Compact | heightfield → compact heightfield (walkable spans only, with clearance and a connection to each of the 4 neighbours when \|Δy\| ≤ climb and the overlap of clearances ≥ height) | | O(W × H × spans) |
| 4 | Erode | compact → compact | every span within `ceil(agentRadius / cs)` cells of a null or border span becomes null (two-pass chamfer distance, weights 2 straight, 3 diagonal) | O(spans) |
| 5 | Mark areas | compact → compact | area volumes in ascending (priority, ordinal): each span whose column centre lies in the volume's xz footprint and whose y lies in its vertical range takes the volume's area; a `null` volume removes the span | O(volumes × footprint cells) |
| 6 | Partition | compact → regions | WATERSHED: distance field (chamfer), then flood fill of level sets in descending distance, 8 expansion iterations per level; MONOTONE: row sweep merging with the previous row's regions; LAYERS: monotone partition per height layer. Regions smaller than `m_uRegionMinArea` that touch no border are removed; regions smaller than `m_uRegionMergeArea` merge into the neighbour sharing the longest edge (ties: lower region ID) | WATERSHED O(spans × levels), MONOTONE O(spans) |
| 7 | Contours | regions → contours | trace each region's boundary, keep the vertices where the neighbouring region or area changes, simplify with Douglas–Peucker at `m_fEdgeMaxError` cells, split edges longer than `m_fEdgeMaxLength`, remove degenerate segments | O(boundary cells × log) |
| 8 | Polygonize | contours → polygon mesh (≤ 6 vertices per convex polygon, shared vertices welded, neighbour per edge, portal edges flagged with their tile side 0 x−, 1 z+, 2 x+, 3 z−) | ear-clipping triangulation of each contour (ear chosen by shortest diagonal, ties by vertex index), then greedy merging of adjacent polygons into convex ones while ≤ 6 vertices, longest shared edge first | O(n²) per contour, n ≤ 1,024 |
| 9 | Detail | polygons + compact → detail triangles (≤ `uAI_NAV_MAX_DETAIL_VERTS_PER_POLY` = 127 detail vertices and ≤ `uAI_NAV_MAX_DETAIL_TRIS_PER_POLY` = 255 detail triangles per polygon, Recast's per-polygon caps, so a detail triangle's `uint8` local index addresses the polygon's ≤ 6 vertices plus its ≤ 127 detail vertices, 133 in all, and its `uint8` count holds the triangulation) | sample heights on the polygon at `m_fDetailSampleDistance` cells; insert the sample of largest error while its error exceeds `m_fDetailSampleMaxError` heights and fewer than 127 samples are inserted; Delaunay triangulation of the polygon's vertices plus the inserted samples; a triangulation of more than 255 triangles fails the stage; distance 0 skips the stage (flat polygons) | O(samples × log) per polygon |
| 10 | Assemble | polygon mesh + detail → tile section (Formats.md "Tile section"): border cells removed, vertices quantised (x, z in cells relative to the tile minimum, y in cell heights relative to the tile's minimum y, `uint16` each), polygon flags from the area table, a bounding-volume tree over the polygons | median-split BV tree on the longest axis, 2 × polygons − 1 nodes, leaves in polygon order | O(p log p) |

- A stage that exceeds a limit (more than `uAI_NAV_MAX_POLYS_PER_TILE` polygons or `uAI_NAV_MAX_VERTS_PER_TILE` vertices, a contour of more than 1,024 vertices, more than 127 detail vertices or 255 detail triangles in a polygon, a span height beyond 13 bits) fails with `AI_ERROR_BUILD_LIMIT` and `m_uDetail` = the stage number; the cook step reports the tile and the limit and fails (`COOK_ERROR_STEP_FAILED`), naming the class parameter to change (cell size, tile size).
- An input with no walkable span after stage 4 yields an empty tile; the importer declares no product for a tile its coarse pre-pass finds empty, and the step writes nothing for an empty result (`ai.navMesh` reports it at INFO).
- With `m_bKeepLayers`, stage 4's compact heightfield (before stage 5) is encoded into the layers section (Formats.md "Layers section") together with the tile's area volumes, for runtime carving.

### Bake determinism

- Every stage is a pure function of its inputs and the class parameters: no clock, no address, no hash-map iteration, sorted inputs, fixed tie-breaks (named in each row above), fixed-order loops, and the floating-point environment of `Zenith2/Docs/Contracts.md` → "Floating-point environment".
- So a tile's bytes are identical across cook worker counts and runs on one build and platform family (`Test_NavBake_TileBytesIdenticalAcrossRuns`, `Test_NavCook_ByteIdenticalAcrossWorkers`); Android never bakes (the cook runs on Windows).
- The runtime carve rebuild and the cook's rebuild are one function on one platform: `Test_NavCarve_MatchesCookRebuild` compares a runtime rebuild with the cook-side `Zenith_NavRebuildTileFromLayers` of the same layers and obstacles on the same host and build. Cross-platform carve equality (a rebuild on the S24 against one on Windows) is not promised ([Z2-N-ARCH-882] promises determinism per build and platform family only); a restore rebuilds carved tiles on the restoring platform, and a carved section whose check differs from the saved one makes that restore COHERENT (`Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm").
- The key of each tile item: the step's `m_uVersion`, `uAI_NAV_FORMAT_VERSION`, `uZENITH_ENGINE_COOK_VERSION`, the class parameters and settings hash, the tile key, the SHA-256 of each opened scene document, the key hash of each dependency output read (collision shapes, terrain sources), and, after the ruling, the pinned version of `recast-detour` as a declared tool version when the Recast/Detour candidate is selected.

### Bake limits

| Quantity | Limit | Error |
|---|---|---|
| input triangles per tile | 4,194,304 | `AI_ERROR_BUILD_LIMIT` (detail 1) |
| cells per tile side W | 2,048 | `AI_ERROR_BUILD_LIMIT` (detail 1) |
| spans per tile | 16,777,216 | `AI_ERROR_BUILD_LIMIT` (detail 3) |
| regions per tile | 65,535 | `AI_ERROR_BUILD_LIMIT` (detail 6) |
| vertices per contour | 1,024 | `AI_ERROR_BUILD_LIMIT` (detail 7) |
| polygons per tile | 4,096 | `AI_ERROR_BUILD_LIMIT` (detail 8) |
| vertices per tile | 16,384 | `AI_ERROR_BUILD_LIMIT` (detail 10) |
| detail vertices per polygon | 127 (`uAI_NAV_MAX_DETAIL_VERTS_PER_POLY`) | `AI_ERROR_BUILD_LIMIT` (detail 9) |
| detail triangles per polygon | 255 (`uAI_NAV_MAX_DETAIL_TRIS_PER_POLY`) | `AI_ERROR_BUILD_LIMIT` (detail 9) |

Each limit is tested at the limit and at limit + 1 by `Test_NavBuild_Limits` ([Z2-N-ARCH-953]).

## Runtime tile table

### Navigation asset requests

A navigation file reaches simulation only at a tick boundary, in completion-ID order, so the tick a tile joins never depends on I/O timing (Z2-D-053; [Z2-N-ARCH-881]'s "asynchronous results published at tick boundaries in completion-ID order, logged"). Phase nodes never call `Zenith_AssetRegistry::GetState` or `Resolve` (`Zenith2/Docs/Design/ASSET/Loading.md` → "Publication to simulation"). The mechanism is ANIM's for asset bindings (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Asset bindings"), under its own source kind `ASYNC_SOURCE_NAVIGATION`, value 13 of SCRIPT's registry (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values"; row AI18 answered), success `PUBLICATION_KIND_GENERIC_RECOMPUTED` with no payload, failure `PUBLICATION_KIND_GENERIC_RECORDED` with ASSET's 24-byte `Zenith_AssetLoadResultRecord`. The sink is MOD's (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink"): attach step 3 (`Zenith2/Docs/Design/AI.md` → "Module start and world attach") declares the source with `Zenith_ModuleSourceDesc{ ai.nav.maxLoadsInFlight, 24, MODULE_SOURCE_SITE_MERGE_CHAIN }` and registers the world's `Zenith_AINavPublicationHandler` with `SetSourceHandler`; completion IDs come from `AllocateCompletionID` on the merge chain alone; results go through `Park`; MOD's forwarder calls the handler's `OnPublished` at tick-boundary step 1 in completion-ID order, live and in replay.

A **load record** (`Zenith_AINavLoadRecord`, `Zenith2/Docs/Design/AI/Internals.md` → "Navigation load records") exists per requested navigation file: completion ID, asset GUID, requester simulation ID, kind (MANIFEST, TILE, VOLUME), state, the operation handle. The records are module state of `Zenith.AI.World`, sorted by completion ID, at most `ai.nav.maxLoadsInFlight` (default 1,024, range 16 … 16,384) from allocation until publication (a parked result counts):

| State | Event | Next | Action |
|---|---|---|---|
| — | a need collected by `AI.NavIngest` step 3 and admitted by the merge node | ISSUED | completion ID allocated for `ASYNC_SOURCE_NAVIGATION`; `Zenith_AssetLoader::RequestLoad({ handle, ASSET_PRIORITY_HIGH, 0, completion queue ai.loads.w<tag>, completion ID, "ai.nav" })` |
| ISSUED | `RequestLoad` returned an error | PARKED_FAILED | the ID is parked at once as `PUBLICATION_KIND_GENERIC_RECORDED` with the 24-byte result built from the `Zenith_AssetError`, so it is completed exactly once (X255) |
| ISSUED | its completion record drained, `SUCCEEDED` | PARKED_OK | `Park(id, ASYNC_SOURCE_NAVIGATION, PUBLICATION_KIND_GENERIC_RECOMPUTED, empty)` |
| ISSUED | its completion record drained, `FAILED` | PARKED_FAILED | `Park(id, …, PUBLICATION_KIND_GENERIC_RECORDED, the 24-byte result)` |
| ISSUED | the requester's supplier record no longer holds the asset in that slot ("Supplier records": entity destroyed or its component removed, the slot's handle changed by value, `InActiveRegion` false) | ABANDONED | `RequestCancel` on the operation handle; the ID completed through the sink with the zero-length "abandoned" payload (SAVE's convention, `Zenith2/Docs/Design/SAVE/Formats.md` → "Async section"); ASSET's subsequent completion record of that operation is dropped at drain |
| PARKED_OK, PARKED_FAILED | the requester goes before publication | ABANDONED | nothing more is completed: the parked result is dropped by the handler |
| PARKED_OK | published (handler, tick boundary) | — | `Resolve` the asset; validate role and settings (step 4 of "Tile add procedure"); push `{ GUID, requester, slot, kind, handle }` onto the world's **join list** in (kind, then for tiles the tile key, for volumes (class, region minimum), for manifests the GUID) order; a MANIFEST's supplier slot becomes JOINED here and its entries are pushed instead (below); the record and its in-flight unit are released |
| PARKED_FAILED | published | — | the requester's supplier slot becomes FAILED ("Supplier records": not requested again until that slot's handle changes by value or the record is removed and re-created, which an `InActiveRegion` that turns false and true does); `ai.nav.loads_failed` + 1, logged once per asset; record released |
| ABANDONED | published | — | dropped; record released |

`AI.NavIngest` declares `ConsumesMergeCapacity` and so has a merge node (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Merge nodes and the merge chain"); its merge function takes the needs step 3 of "Tile add procedure" collected, in their collected order, and for each while the in-flight count is below `ai.nav.maxLoadsInFlight` allocates its completion ID with `AllocateCompletionID(ASYNC_SOURCE_NAVIGATION)` and issues it; a need beyond the count, or one the sink refuses with `PUBLICATION_SINK_ERROR_FULL` (MOD's first-fit acceptance test: a refusal changes nothing, `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Allocation sites and acceptance"), allocates no ID, stays NEED in its supplier slot and is collected again by the next run (`ai.nav.loads_deferred_delta`). The sink's other errors cannot arise on this site (`REPLAYING` is a prologue refusal, `DETACHED` needs a retired table, which runs no merge node, and `SITE` is an engine defect the sink asserts); each is treated as FULL (the need deferred, counted in `ai.nav.loads_deferred_delta`), never as an allocation. Which needs issue, and so every completion ID, is a function of committed state alone.

A MANIFEST's entries are hard dependencies of the manifest (`Zenith2/Docs/Design/AI/Formats.md` → "Manifest section"), so its publication makes every entry join-ready: the handler pushes each entry onto the join list (requester and slot the manifest's) without a request of its own and sets the manifest's supplier slot JOINED, so the manifest is requested exactly once ("Supplier records"). Cell entities' handles are the cell scene's references, already resident when STREAM activates the cell (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Navigation coordination"). The timing is therefore, for a NAVIGATION cell activated at sync point 1 of tick t (its tile entities spawned at that sync point's step 6): the need is collected by `AI.NavIngest` in phase 7 of t and issued by its merge node in t; the completion record is drained in the first phase 7 after the asset service completed it (t + 1 at the earliest), parked there, published at that tick's boundary, and the tile joins in the next phase 7 (t + 2 at the earliest). An agent of a GAMEPLAY cell, which STREAM activates at the earliest one tick after the navigation cell it depends on, is SUSPENDED from its first phase 7 until its tile joins and then replans (`Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine"); cross-area row AI21 asks STREAM to state this instead of "an agent's tile is active before the agent exists".

Under `--replay` each publication is applied at its recorded tick and the handler holds the tick until the local load completes (ASSET's rule), so a replay joins every tile on the recorded tick. Two **live** runs with real I/O publish a load on whichever tick its completion was drained, so they diverge from the first such difference — through the agents SUSPENDED for different durations, and everything downstream — and stay diverged (the rule `Zenith2/Docs/Design/SAVE.md` → "Determinism obligations" states for every I/O-timed source); no window after the loads settle repairs that. Live-run agreement is therefore asserted from a deterministic synchronisation point instead: `Test_AIDeterminism_LiveRunsAgree` runs both live runs with the completion gate's **fixed delay** (`m_uGateFixedDelay` 8: every navigation load's completion record is released in the first phase 7 at or after its issue tick + 8, "Completion gate"), so every release, publication and join tick is a function of the issue tick, which is a function of committed state, in both runs, as long as every record reached the gate before its release tick, which the test asserts (`m_uGateLateReleases` = 0); the comparison then covers the whole run from tick 0. Divergence caused by real I/O timing is covered by replay instead (`Test_NavRuntime_TileJoinReplayedAtRecordedTick`, [Z2-N-SCRIPT-338]).

### Completion gate

The gate is AI's own control over when a drained ASSET completion reaches the load records, as `m_uLinkCapacityOverride` is AI's own control over link capacity; it uses no ASSET test control and doubles no ZenithAssets class (ruling FINAL-DOUBLES). Its members are those of `Zenith_AIWorldState` (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AIWorldState header"):

1. **Admit.** `AI.NavIngest` step 2 copies every drained `Zenith_AssetLoadCompletion` whose user tag names an ISSUED load record into the gate list (`m_axGate`, capacity `ai.nav.maxLoadsInFlight`, reserved at attach, kept sorted by completion ID). A record of an ABANDONED load is dropped at the drain, as before, and abandoning a load drops its gate entry if it has one.
2. **Release.** In production the two test members are unset (`m_axGateHolds` empty, `m_uGateFixedDelay` 0) and every entry is released in the same step. Otherwise an entry with a hold (`m_axGateHolds`: completion ID → release tick, sorted by completion ID) is released in the first phase 7 at or after its release tick. Without a hold, a non-zero `m_uGateFixedDelay` releases an entry in the first phase 7 at or after its load record's issue tick (`m_ulIssueTick`) + the delay, and a zero one releases it at once. An entry admitted after the tick it would have been released at is released on admission and counted in `m_uGateLateReleases`, a test observation that no module code reads.
3. **Advance.** Released entries advance their load records in ascending completion ID: SUCCEEDED → PARKED_OK, FAILED → PARKED_FAILED, each parked as the state table states.

While its completion is held, a load record stays ISSUED: simulation has not observed the asset's completion. So a capture during a hold saves it as ISSUED (save list 3) and the restore re-issues it (`Zenith2/Docs/Design/AI/Agents.md` → "Restore" step 3). The gate's entries and test members are outside the save and the canonical hash, and in production the gate is empty after every phase 7. Under `--replay` a held record holds the tick boundary exactly as an unfinished load does: its RECOMPUTED publication has no local completion parked yet, so the table holds the tick until the gate releases the record and AI parks it ([Z2-N-SCRIPT-341]).

### Supplier records

`Zenith.AI.World` holds one **supplier record** (`Zenith_AINavSupplier`, `Zenith2/Docs/Design/AI/Internals.md` → "Navigation load records") per entity that carries `Zenith.AI.NavMeshRef`, `Zenith.AI.NavMeshTiles` or `Zenith.AI.NavVolumeRef` and is in an active region (`Zenith.Streaming.RegionBound` absent, or `InActiveRegion` true), sorted by simulation ID, saved (`Zenith2/Docs/Design/AI/Formats.md` → "Save section layout" list 4) and hashed. A record holds the component kind (1 manifest, 2 tiles, 3 volume) and, per handle slot in property order (one slot for kinds 1 and 3; `Count` ≤ 16 slots for kind 2), the asset GUID (zero for a null handle) and a state:

| Slot state | Meaning | Next |
|---|---|---|
| NEED | to be requested | IN_FLIGHT when the merge node issues it; stays NEED when deferred |
| IN_FLIGHT | a live load record names this (requester, slot) | JOINED at the join (tiles, volumes) or at the publication (manifest); FAILED at a failed publication |
| JOINED | the asset's tile or volume is loaded from it, or the manifest's entries were pushed | stays until the slot's GUID changes or the record is removed |
| FAILED | its load failed or its join was refused (`AI_ERROR_SETTINGS_MISMATCH`, `AI_ERROR_TILE_DUPLICATE`, `AI_ERROR_TILE_CAPACITY`, a wrong role) | never requested again while the slot keeps this GUID |

**Comparison by value.** Each tick `AI.NavIngest` step 3 compares, for every such entity in ascending simulation ID, the current handle GUIDs (read from the component by value) with the record. A slot whose GUID is unchanged keeps its state; a slot whose GUID changed (or that appeared or disappeared with `Count`) releases the old GUID — every loaded tile or volume whose recorded (supplier simulation ID, supplier slot) is this (entity, slot) is removed ("Tile remove procedure"), which for a manifest slot is every tile and volume its entries supplied, and a live load record of the slot is ABANDONED — and takes the new GUID in state NEED (a zero GUID takes no state). An entity that no longer qualifies (destroyed, component removed, `InActiveRegion` false) has its record removed with the same releases. ECS change tracking is never consulted, so a restored world whose change bits are all set keeps every supplier, tile and FAILED slot exactly as the uninterrupted run (`Test_AISave_RestoreKeepsSuppliersWithChangeBitsSet`), and a non-streamed world's manifest is requested once, never again after it published (`Test_NavRuntime_ManifestRequestedOnce`).

### Tile add procedure

`AI.NavIngest` (phase 7, first AI node), in this order every tick:

1. **Join.** For every entry of the join list (filled by the publication handler at the previous tick boundary), in its order: run steps 4–7 below for a TILE (a VOLUME joins the volume set, Volumes.md "Volume model"; a MANIFEST's entries were expanded by the handler). The list is empty afterwards.
2. **Drain.** Drain `ai.loads.w<tag>` into the node arena and pass every record through the world's **completion gate** ("Completion gate" below); each record the gate releases advances its load record ("Navigation asset requests"), in completion-ID order.
3. **Collect.** Update the supplier records by value comparison ("Supplier records"; so a tile is requested in the tick its cell's entity first qualifies, and joins after its publication) and append every slot in state NEED, in (simulation ID, slot) order, to the collected-needs array for the merge node. The asset's role is not known here, since nothing is loaded and phase nodes never read an asset's state; it is checked at the join (step 4) and, for a manifest, at its publication.
4. **Validate** the published file: role (a TILE or VOLUME asset of another role than the supplying component's also counts `ai.nav.wrong_role`), settings (`AI_ERROR_SETTINGS_MISMATCH`), tile key unique among loaded tiles (`AI_ERROR_TILE_DUPLICATE`, the duplicate is ignored), tile section checks (Formats.md "Tile section", already run by the loader); a failure is counted (`ai.nav.tiles_refused`), logged once per asset, and makes the supplier slot FAILED.
5. **Slot.** Take the lowest free slot (none: `AI_ERROR_TILE_CAPACITY`, counted, refused, the supplier slot FAILED); record the tile key, the asset handle (one reference held while loaded), the supplier's simulation ID and supplier slot, the decoded section pointers, the world-local origin ("Origin shift"), version 1, an overlay of zero flags; the supplier slot becomes JOINED.
6. **Connect** it ("Tile connection procedure"), re-apply every enabled MARK obstacle whose footprint overlaps it ("Mark procedure" step 2; a CARVE obstacle queues the tile for a rebuild), and resolve every unresolved off-mesh link whose endpoint lies in it ("Link registration").
7. Increment the mesh's change serial.

Steps 1–7 run on `AI.NavIngest`'s own job before its carve sub-jobs ("Carve procedure"); removals ("Tile remove procedure") run after step 3 and before the obstacles of "Mark procedure".

### Tile connection procedure

- For each of the tile's portal edges on side s, the neighbour column is (tx, tz) stepped toward s; every loaded tile of the same class in that column (any layer) is a candidate.
- Two portal edges connect when they lie on the shared tile boundary, their projections on the boundary axis overlap by more than 0.01 m, and their heights at the overlap's ends differ by at most the class's max climb; each connected portal records the neighbour reference and the overlap interval `[tmin, tmax]` (quantised to 1/255 of the edge), in both tiles, so a connection always exists in both directions.
- Connections are kept per tile in an external-link list of the slot (capacity 4 × polygons of the tile) whose entries (`Zenith_NavExternalLink`, 24 bytes: polygon index `uint16`, edge index `uint8`, tmin `uint8`, tmax `uint8`, reserved `uint8` × 3, neighbour tile key `Pack()` `uint64`, neighbour polygon index `uint16`, reserved `uint16`, neighbour slot `uint32` — a lookup cache written at connect, never compared or ordered by) are **kept sorted** by (polygon index, edge index, neighbour key `Pack()`, neighbour polygon index): a connect inserts each new link at its sorted position (binary search, then a shift of the tail) and a disconnect removes it in place. The list's contents and order are therefore a function of the set of loaded tiles alone, never of the order they were added or of their slots: a live streamed world and a restore that adds tiles in key order hold byte-identical lists (`Test_NavRuntime_TileConnectionBothDirections`). Every iteration over a polygon's neighbours — the A* expansion, surface movement, the raycast, the random-point flood — visits its in-tile neighbours by ascending edge index, then its external links in this sorted order, then the off-mesh links it starts in link-entity simulation-ID order.
- **Overflow is a set function.** A tile's list capacity is `4 × P` (P its polygon count). The **candidates** of a tile T are every connection between a portal edge of T and a portal edge of a loaded tile of a neighbour column that meets the rule above. The candidates of T are ranked by (overlap length in cells descending — an exact integer, the same from both sides, because adjacent tiles of one class share the cell grid along their common boundary —, then polygon index, edge index, neighbour key `Pack()`, neighbour polygon index ascending); a candidate **exists** (is in both tiles' lists) if and only if its rank is below the capacity in the ranking of **both** of its tiles. Existence is thus a function of the set of loaded tiles alone: it does not depend on the order tiles were added or removed, and a dropped candidate is re-admitted as soon as the rankings admit it. The candidates that do not exist are counted once per recomputation in `ai.nav.links_dropped`.
- **Recomputation.** At every tile add and remove, AI recomputes the candidate ranking of the added or removed tile (none after a removal) and of every loaded tile in its four neighbour columns, in tile-key order; then for each candidate of a recomputed tile it re-evaluates existence against the current ranking of the other tile and inserts or removes the link in both lists (sorted insertion and in-place removal as above). A tile two columns away keeps its ranking (its candidates did not change), but its list can gain or lose a link to a recomputed neighbour, which this step writes. A restore that adds tiles in key order therefore ends with exactly the lists of the live streamed world (`Test_NavRuntime_TileConnectionBothDirections`).
- Connecting a tile is O(portal edges × neighbour portals) per side to enumerate candidates, plus O(L log L) per recomputed tile for its ranking (at most 5 columns × `uAI_NAV_MAX_LAYERS_PER_COLUMN` tiles), plus O(log L) per insertion and O(L) for the shift, L ≤ 4 × 4,096.

### Tile remove procedure

A tile is removed when its supplier slot is released ("Supplier records": the entity that supplied it is destroyed or loses its component, its `InActiveRegion` turns false, or the slot's handle changes by value — never on a change bit), or the world is destroyed:

1. Disconnect: remove every external link of the four neighbour columns that names this tile's key (the sorted lists stay sorted), then recompute the neighbours' rankings ("Tile connection procedure", overflow rule), which may re-admit candidates the removed tile had crowded out.
2. Unregister every off-mesh link with an endpoint in it (the links return to the unresolved set).
3. Release the asset handle; free the slot (salt + 1); drop the overlay, any carved replacement, the tile's carve-queue entry and its carved record ("Carve procedure"); every MARK obstacle that had marked polygons of it is re-applied ("Mark procedure" step 2), so its mark set stays the first 256 of the loaded tiles.
4. Increment the change serial.

A removal while the tile's load is still in flight abandons the load record instead ("Navigation asset requests"). Every corridor and search that visited the slot sees a stale reference at its next use and replans or restarts (`Zenith2/Docs/Design/AI/Agents.md` → "Path corridor"; "Per-tick processing"); an agent standing on the removed tile has no polygon and becomes SUSPENDED until a tile under it returns.

`Test_NavRuntime_StreamTileLoadUnload1000Cycles` adds and removes randomised tile sets 1,000 times with agents moving across them and asserts no live tile slot, asset reference, link record or allocation remains after the last removal.

### Streamed tiles

<!-- assumption: pending WORLD, STREAM (rows AI11, WD11) -->
- In a streamed world the navigation tiles are the content of NAVIGATION-role cells (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Navigation coordination"): for each navigation cell and each ground class whose tiles intersect it, WORLD's partition cook writes one entity carrying `Zenith.AI.NavMeshTiles` with the handles of the class's tiles whose minimum corner lies in the cell (a 128 m cell holds 16 tiles of 32 m, 64 of 16 m, so a 16 m tile size writes four entities per class and cell), and one `Zenith.AI.NavVolumeRef` entity per volume region whose minimum corner lies in the cell.
- AI provides that contribution as the ZenithAuthoring function `Zenith_AIWriteCellNavigationContent(const Zenith_AINavigationManifestView&, const Zenith_WorldCellBounds&, Zenith_WorldCellContentWriter&)` over WORLD's two types (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Cell content writer"), with the view built by `Zenith_AIMakeNavigationManifestView(Zenith_Span<const Zenith_WorldProduct>, Zenith_Arena&)` from the product list of the derived navigation documents WORLD's importer read; both are declared in `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header". The view needs nothing but the product keys, because a tile product key carries its class tile size and a volume product key its region's minimum corner (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema"). Entity order is ascending (class name bytes, then (layer, tx, tz), the tile key's `Pack()` order within a class), then volumes in (class name bytes, region index).
- GAMEPLAY cells depend on the navigation cells beneath them, so an agent's tile is requested by the tick the agent's cell activates at the latest; it joins at its publication ("Navigation asset requests"), and the agent is SUSPENDED for the ticks in between. An off-mesh link joining two cells registers only when both cells' tiles are present ("Link registration").
<!-- /assumption -->
- In a world without streaming, one entity of the persistent scene carries `Zenith.AI.NavMeshRef` naming the navigation document's MANIFEST, whose hard references make every tile and volume resident with it; the manifest is requested once and its supplier slot stays JOINED ("Supplier records").
- Both kinds of entity may coexist; a tile key supplied twice is `AI_ERROR_TILE_DUPLICATE` for the second supplier.

### Origin shift

- Tile vertices are tile-local quantised; each loaded tile stores its absolute origin as three `double`s and its world-local origin `L = (float)(absolute − O)` (computed in binary64, narrowed last), where O is the current offset of the world resource `Zenith.WorldOrigin` (declared by ZenithScene, written only by SCENE's shift playback; `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "SCENE world origin"). Volume regions store their absolute minimum the same way.
- **When AI shifts.** At the sync point of the shift, like PHYS (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift"): SCENE plays the shift last in step 7 of sync point 1 (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebase rule") and then dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED` with the played `Zenith_WorldOriginShift` as payload; AI's lifecycle listener (interest bit `LIFECYCLE_EVENT_ORIGIN_SHIFTED`) shifts AI's state on that sync-point node, so every phase from 2 to 12 of the rebase tick — script queries through `AI.FindNearestPoint`, `AI.Raycast`, `AI.RandomReachablePoint`, `AI.SampleMedium` in phase 2, and every AI node in phase 7 — sees shifted tiles together with shifted transforms (`Test_NavRuntime_OriginShiftKeepsCorridors`).

<!-- assumption: pending SCENE (rows AI19, PH20; core-doc item X264 (j)) -->
The `Scene.ShiftWorldOrigin` playback dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED` through the world's `Zenith_LifecycleDispatcher` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleListener header") with a null entity and `m_pPayload` pointing at the played `Zenith_WorldOriginShift`, after it has shifted every transform and every component property registered with `PROPERTY_FLAG_WORLD_POSITION` (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "ECS world-position flag").
<!-- /assumption -->

SCENE's signed-off step 6 (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shift playback") visits flagged **component** properties only, never the elements of a container, so AI shifts its one container-held position itself (row AI19 (b)'s fallback, adopted as the design): `Zenith.AI.PerceivedTarget` 3 LastKnownPosition carries no flag.

- **What the listener shifts**, with S the shift (an exact integer per axis), in this order, every position by SCENE's formula `(float)((double)p − S)` so a module-state position and the transform it was taken from move identically:

| State | Rule |
|---|---|
| every loaded tile's and volume's L | recomputed `(float)(absolute − O_new)` from the stored absolute origin |
| every agent runtime record (ascending simulation ID) | the corner cache positions, `m_xTarget`, the link traversal start and end, the stuck-window start, the volume corners, the root-motion stuck anchor (`Zenith2/Docs/Design/AI/Internals.md` → "Agent runtime record") |
| the path service (`Zenith.AI.PathService`) and the path inbox | every queued and active descriptor's start and end (agent plans and the 64-byte script payloads), and the from and to of every path-inbox entry (script requests committed on the previous tick's phase-10 merge chain, which `AI.NavRequests` has not yet drained, `Zenith2/Docs/Design/AI/Internals.md` → "Path request consumer"); every ACTIVE search is marked restart-pending: `AI.NavRequests` re-begins it from its shifted descriptor in step 4 of the rebase tick, counted `ai.path.restarted_delta` (a search's node positions would otherwise mix two frames; the shift tick is a deterministic tick, so the restart is too) |
| the obstacle applied records ("Mark procedure") | every applied position, so the first comparison after the shift against the shifted pose (SCENE shifted the root `Zenith.LocalTransform` and every `Zenith.WorldTransform` by the same formula) stays bit-equal for an obstacle that did not move: no obstacle becomes dirty and no carve is queued by a shift (the PHYS precedent of `Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift" step 3) |
| the carved records ("Carve procedure") | none: their obstacle sets are stored tile-local, which a shift does not change |
| the link registrations ("Link registration") | every registered entity position, so no link appears to have moved and none is re-registered; the snapped ends are tile-local and need nothing |
| the noise buffer | every pending entry's position |
| the blackboard store | every value of kind 6 POSITION (`Zenith2/Docs/Design/AI/Agents.md` → "Blackboard records"); values of kind 3 VECTOR (directions, offsets, any non-positional triple) are never shifted |
| `Zenith.AI.PerceivedTarget` 3 LastKnownPosition | for every live entity of the playback access's entity table in ascending simulation ID (SCENE's step 4 order) that has `Zenith.AI.PerceptionState`: `ReadContainer` of property 30 Memory, every element's LastKnownPosition by the formula, `SetContainer` with the same count, through the listener's `Zenith_WorldPlaybackAccess` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_World header"), rewriting in place (the storage rule assumed below); O(perceivers × 16) |
| the crowd and stimulus grids | none: rebuilt from shifted transforms in the tick's phase 7 |
| component properties | none by AI: `Zenith.AI.Agent` 20 MoveTarget and 44 NextCorner and `Zenith.AI.PerceptionState` 23 PrimaryLastKnownPosition carry `PROPERTY_FLAG_WORLD_POSITION` (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Agent properties", "Perception properties"), so SCENE has shifted them in the same playback; velocities (Agent 30, 43), yaws and entity-local points (OffMeshLink 5, 6) are translation-invariant and unflagged |

<!-- assumption: pending ECS (row AI24) -->
An equal-count `Zenith_WorldPlaybackAccess::SetContainer` at a sync point, from a lifecycle listener, rewrites the container's elements in place and allocates nothing. ECS documents container storage as reserved at merge (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_World header"; `Zenith2/Docs/Design/ECS/Backends.md` → "Container heap": `AllocateReserved` within the reserved bytes), and a listener's call has no merge, so AI asks ECS to state this rule for every backend (cross-area row AI24). [Z2-N-ARCH-544] already allows a listener's property writes. The fallback, recording the rewrite into the follow-up command buffer that applies at the next sync point, is rejected: phase 2 of the rebase tick runs before that sync point, so its behaviours would read unshifted last known positions beside shifted transforms. If ECS answers otherwise, this row changes with its answer.
<!-- /assumption -->

- **Rebase bookkeeping (the PHYS rule).** The listener then stores `m_uAppliedRebaseCount = Zenith.WorldOrigin::m_uRebaseCount` and `m_adAppliedOrigin` = the resource's origin (three binary64), both in `Zenith.AI.World`, and counts `ai.nav.origin_shifts` (cumulative). `AI.NavIngest` compares the count with the resource's every tick: a difference (a shift whose event did not reach AI) applies the same table at ingest with `S = O − m_adAppliedOrigin` per axis in binary64 (exact: both are sums of integer shifts, so two or more missed shifts apply as their sum), then stores both values, counted `ai.nav.origin_late_delta` and logged once at error severity; it is a defect path, never the normal one (`Test_NavRuntime_LateShiftUsesAppliedOrigin`).
- Both values are saved in the section header and restored before any tile is added (`Zenith2/Docs/Design/AI/Agents.md` → "Restore" step 2), so a restored world whose origin has rebased N times holds count N and the applied origin O, and its first ingest shifts nothing a second time (`Test_AISave_RestoreAfterRebaseNoSecondShift`). At a phase-11 or out-of-tick capture the saved count always equals the resource's (the listener ran at sync point 1 and the late path at phase 7), so a restore whose saved count differs from the restored `Zenith.WorldOrigin` count, or whose saved applied origin differs from the restored origin with equal counts, is `SAVE_ERROR_CORRUPT`.
- An AI command recorded in phase 1 of the rebase tick (a noise, a move) plays before the shift in sync point 1 and is shifted with the state above; one recorded in phases 2 to 10 was recorded by code that read the shifted O. No vertex is rewritten and no reference changes, so corridors survive a rebase.
- Off-mesh link endpoints come from entity transforms, which the rebase already shifted; registered links store tile-local endpoints and need nothing.

## Dynamic obstacles

### Obstacle modes

| Mode | Effect | Cost | Requires |
|---|---|---|---|
| MARK | every polygon whose footprint overlaps the obstacle's footprint (inflated by the class radius) and whose height range overlaps the obstacle's (± climb) gets `NAV_POLY_FLAG_BLOCKED`, or, with a non-null `Area`, the overlay area override | O(polygons under the obstacle's AABB, through the BV tree) | nothing |
| CARVE | every carve-capable tile under the obstacle is rebuilt from its layers with the footprint cut, so paths go round the obstacle's exact shape | one tile rebuild (stages 6 … 10) per affected tile | the tile's layers section (the navigation document's `carveRegions`) |

An obstacle's footprint is its shape at the entity's **world pose** (position and yaw; a box with pitch or roll uses its xz-projected AABB rotated by yaw). The world pose of every entity AI places in the world by its transform — obstacles, link entities, LOD foci, move-target entities and stimulus sources — follows one rule (the stimulus grid's, `Zenith2/Docs/Design/AI/Agents.md` → "Stimulus grid"): a hierarchy root's is its `Zenith.LocalTransform` 1, 2 (current, written by phase 6); a child's is its `Zenith.WorldTransform` (the previous tick's phase-8 propagation, a one-tick lag), so a door obstacle parented to a building carves where the door is. Both are world-local and both are shifted by SCENE at a rebase.

### Mark procedure

1. Each tick `AI.NavIngest` reads every `Zenith.AI.NavObstacle` (ascending simulation ID) and its world pose ("Obstacle modes"), and compares it with the obstacle's **applied record** (`Zenith_AIObstacleApplied`, `Zenith2/Docs/Design/AI/Internals.md` → "Obstacle and carve records": the world-local pose position and yaw, shape, mode, area, half extents, `MoveThreshold` and `Enabled` it was last applied with, held in `Zenith.AI.World` and saved). The obstacle is dirty when it has no applied record and is enabled (it appeared), it has one and is gone or disabled (it disappeared), its position moved by more than `MoveThreshold` from the applied position or its yaw by more than `MoveThreshold` / max(half extent x, z) radians, or any applied property differs by value. ECS change tracking is never consulted, so a restored world, whose change bits may all be set, re-dirties exactly the obstacles the uninterrupted run would (`Test_AISave_CarvedTwiceRestoresExact`). A dirty obstacle's applied record is replaced by its current values (removed when it disappeared).
2. **Applying a MARK obstacle** (when dirty, and when a tile under its footprint is added, removed or carve-swapped): clear the overlay bits it set, then gather every polygon of every loaded tile of every class whose footprint (inflated by the class radius) and height range (± climb) overlap the obstacle at its applied pose, through each overlapping tile's BV tree; sort the gathered (tile key `Pack()`, polygon index) pairs ascending and keep the first 256 (`uAI_NAV_MAX_MARK_POLYS`) as the obstacle's mark set, the rest counted once per application in `ai.nav.mark_truncated`; set the bits of the mark set. The mark set is therefore a function of the loaded tiles and the applied pose alone, whatever the BV traversal or the order tiles arrived in.
3. Overlay bits combine as the OR of every obstacle covering the polygon; an area override is the override of the covering obstacle with the highest simulation ID.
4. The change serial increments once per tick in which any overlay changed.

### Carve procedure

1. A dirty CARVE obstacle marks every carve-capable tile its inflated footprint overlaps, at its previous applied pose and at its new one, as needing a rebuild; a tile without a layers section is MARKed instead (`ai.nav.carve_fallback_delta`).
2. The rebuild queue holds tile keys, deduplicated, ordered by (the tick the tile was first queued, tile key); it is saved with each entry's first-queued tick, so its order after a restore equals the uninterrupted run's.
3. Each tick the first `ai.nav.maxTileRebuildsPerTick` tiles of the queue are rebuilt as sub-jobs of `AI.NavIngest` (`ParallelFor`, one tile per sub-job): each runs `Zenith_NavRebuildTileFromLayers` with the **obstacle set** of that rebuild — every enabled CARVE obstacle whose applied record's footprint overlaps the tile, sorted by simulation ID, each converted to a `Zenith_NavObstacleShape` in tile-local metres from its applied pose (`((double)p + O) − tile absolute origin` per axis in binary64, O the current `Zenith.WorldOrigin` offset, narrowed once) — into a per-sub-job arena.
4. After the join, the node swaps each rebuilt tile in, in tile-key order: the slot keeps its key and external connections are rebuilt for it ("Tile connection procedure"), its salt increments and its **version** increments (the version counts the swaps of the tile since it was added, 1 when added), its MARK obstacles are re-applied; its **carved record** (`Zenith_AICarvedTile`, `Zenith2/Docs/Design/AI/Internals.md` → "Obstacle and carve records": key, version, the obstacle set of this rebuild as the exact tile-local `Zenith_NavObstacleShape` values the rebuild was given — tile-local, so an origin shift never changes them — and the check (`Zenith_Hash64` truncated to 32 bits, Formats.md "Common rules") of the rebuilt tile section) replaces the previous one. A rebuild that fails keeps the previous polygons and record and counts `ai.nav.carve_failed_delta`.
5. Obstacles removed or disabled queue the tile again, so it returns to its baked shape: a tile whose obstacle set would be empty is restored from the asset's baked tile section without a rebuild, its version increments, and its carved record is dropped.

- The carved records are exactly what a restore needs to reproduce the carved polygons the live world holds: an obstacle that moved after a tile's last rebuild, with that tile's re-carve still queued, is in the record at the shape that rebuild used, not at its current pose, and a save taken after any number of origin shifts restores the same shapes bit for bit (`Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm" step 4). Salts are never saved; references are re-resolved by (tile key, polygon index).

- Because rebuilds happen in the node and the queue order is a function of committed state, the tick at which each carved tile becomes visible is deterministic for any worker count (`Test_NavCarve_DeterministicAcrossWorkers`).
- A tile rebuild at the W-Outdoor defaults (32 m tile, 0.25 m cells, 135 × 135 cells with border) is about 18,000 spans; its cost is within the AI phase CPU row Z2-B-AI-002 (pending, allocated by the Stage 1 per-system budget ticket for AI), and the per-tick rebuild cap is the pending row need "Navigation tile rebuilds per tick" (`Zenith2/Docs/Design/AI.md` → "Pending row needs").

### Obstacles at restore

Obstacle components are saved with the world, and the AI section saves the applied records, the rebuild queue and the carved records; at restore the AI participant rebuilds every carved tile from its saved obstacle set before the first tick, sets its saved version and compares the rebuilt section's check with the saved check (`Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm"), so a loaded world holds exactly the carved polygons, versions, queue and overlay the saved world held, and drains the queue under the same per-tick cap from the first tick (`Test_AISave_QueuedCarveIsExact`).

## Off-mesh links

### Link kinds

| Kind | Traversal | Agent state | Cost |
|---|---|---|---|
| WALK | AI moves the agent from start to end in a straight line at the agent's speed scale over `Duration` | TRAVERSING_LINK | area cost × straight distance |
| JUMP | parabola from start to end with apex `JumpHeight` above the higher endpoint, over `Duration` | TRAVERSING_LINK | area cost × straight distance + 1 m equivalent |
| SCRIPTED | AI stops the agent at the start, emits `Zenith.AI.OffMeshLinkReached`, waits for `AI.CompleteOffMeshLink`, then places the agent at the end | WAITING_LINK | area cost × straight distance |
| TELEPORT | AI records `Zenith_RecordTeleport` to the end point at the next sync point | TRAVERSING_LINK for one tick | area cost × 1 m |

### Link registration

- A link entity's start and end (its entity-local points transformed by its world pose, "Obstacle modes") are snapped to the nearest polygon of each class in `ClassMask` within `SnapDistance` (`Zenith_NavFindNearestPoint` with extents (d, 2 × climb, d)); when both ends snap for a class the link is registered for it: a link node joins the world's link table (at most `uAI_NAV_MAX_LINKS_PER_WORLD`), storing each snapped end as (tile key, polygon index, tile-local position) and converting to world-local through the tile's L at use, and an edge from the start polygon (and from the end polygon when bidirectional) to the link node. A link whose two ends snap to the same polygon is not registered (`AI_ERROR_LINK_INVALID`, counted `ai.nav.links_unresolved`). Each link table slot carries a 16-bit salt (1 … 65,535, incremented when the slot is freed, as tile slots), so a `Zenith_NavPolyRef` in link-node form (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_AITypes header") naming an unregistered or re-used link is stale. A link node is identified everywhere outside the table (search nodes, corridors, corners, saves, the hash) by (link entity simulation ID, class index), never by its table index. The registration also records the entity's world-local position at registration (the **registered entity position**), which the origin-shift listener shifts.
- A polygon carries at most `uAI_NAV_MAX_LINKS_PER_POLY` (8) links; a ninth is refused (`AI_ERROR_LINK_CAPACITY`, counted).
- Links whose endpoints do not snap stay in the unresolved set and are retried when any tile is added; a link whose entity's world pose moves by more than its snap distance from the registered entity position is re-registered; a disabled link (`Enabled` false) is unregistered but keeps its entity.
- Registration order is ascending simulation ID of the link entity, so link node indices, which A* never uses for ordering, are nonetheless deterministic.
- A link whose two ends lie in tiles of different streamed cells is registered only while both tiles are loaded, in both directions at once (the both-directions rule of `Zenith2/Docs/Design/STREAM/Runtime.md` → "Navigation coordination").

### Link traversal state machine

| State | Event | Next | Action |
|---|---|---|---|
| following corridor | next corner is OFFMESH_START within `max(radius, 0.25 m)` | traversal start | record the link entity; emit `Zenith.AI.OffMeshLinkReached` |
| traversal start | kind WALK or JUMP and `AUTO_LINKS` | TRAVERSING_LINK | suspend the controller (`InputFlags` SUSPENDED command); t = 0 |
| traversal start | kind SCRIPTED, or `AUTO_LINKS` clear | WAITING_LINK | stop; desired velocity zero |
| traversal start | kind TELEPORT | TRAVERSING_LINK | record the teleport; next tick → after link |
| TRAVERSING_LINK | t ≥ Duration | after link | position = end; resume the controller |
| TRAVERSING_LINK | each tick | TRAVERSING_LINK | position = interpolation at t / Duration (WALK linear; JUMP parabola), written as `Zenith.LocalTransform` position by command |
| WAITING_LINK | `AI.CompleteOffMeshLink` played back | after link | position = end by teleport command |
| any | link unregistered (tile removed, link disabled) | replan | a TRAVERSING agent completes at the end point it already targets; a WAITING agent replans from its position |
| after link | — | following corridor | the corridor advances past the link node |

## Path search

### Search graph

- Nodes are polygons and link nodes; a polygon's neighbours are its in-tile neighbours, its external connections (portals to other tiles) and the links it starts.
- A neighbour passes the filter when its polygon flags (baked | overlay) pass the include/exclude test and its area's cost is finite.
- A node's position is the point where the search entered it from its current parent (the **entry position**): for a polygon entered through an in-tile edge, the midpoint of that edge; through an external connection, the midpoint of the edge clamped to the connection's overlap interval `[tmin, tmax]` (the midpoint of the overlap segment); for a polygon entered from a link node, the link's exit end; for a link node, the link's entry end; for the start polygon, the start point. All are world-local, converted from tile-local through the tile's L.
- A link node has one neighbour: the polygon at the end opposite the one its parent lies in (the end polygon when entered from the start polygon, the start polygon when a bidirectional link is entered from its end polygon). Its **entry end** is the end in its parent's polygon and its **exit end** the other; both change with the parent when a relaxation replaces it ("Search state and slicing").
- **Dead link nodes.** A link node created by a search can outlive its link's registration: the link is disabled or re-registered, or the tile of its far end is removed while no polygon of that tile was ever created in the search (so no stamp of it stales the search). Its link-node reference then no longer resolves: the link table slot is free or its salt differs ("Link registration"). Such a node is a **dead link node**. It is never a neighbour, because the filter requires the link registered and enabled, and a new registration in the same slot carries a different salt and so a different reference. A dead link node already in the pool keeps its record (reference, position, g, parent, flags) unchanged. It is closed when popped and expands nothing (expansion step 1), and `Finish` never writes it into a corridor ("Search state and slicing", Finish row). Its tie-break key needs no lookup ("Tie-breaking"), so the open heap stays totally ordered while it waits there. A save writes it as the dead entry, and a restore rebuilds it as `Zenith_NavPolyRef::MakeLinkNode(0, 0)`: salt 0 matches no slot (salts are 1 … 65,535), so it stays dead exactly as it is live (`Zenith2/Docs/Design/AI/Formats.md` → "Save section layout", node reference entry kind 3).

### Cost and heuristic

- g(child) = g(parent) + |pos(child) − pos(parent)| × cost(area of parent) for a polygon parent; for a link-node parent, g(child) = g(link node) + the link's traversal cost from "Link kinds" (area cost × |exit end − entry end|, plus 1 m equivalent for JUMP; area cost × 1 m for TELEPORT) with pos(child) the exit end; for the end polygon, `Finish` adds |end − pos(end node)| × cost(area of end polygon).
- h(node) = |end − pos(node)| × hScale, hScale = 0.999 × the filter's minimum included cost, so h never overestimates the remaining cost of any path through the entry positions (every cost ≥ 1.0 is validated).
- f = g + h, computed in binary32 in that order; distances are Euclidean in 3D, `Zenith_Length` of the binary32 difference.
- A* with node reopening over these entry positions returns the least-cost corridor among those its relaxation rule can represent (each polygon keeps the entry position of its cheapest known parent): it is **optimal over the portal-midpoint graph with node reopening**, not the Euclidean shortest path, which the funnel extraction approaches; N1's and N2's 1% and 2% bounds are on the straight path ("Conformance scenarios").

### Search state and slicing

`Zenith_NavPathSearch` states:

| State | Event | Next | Action |
|---|---|---|---|
| IDLE | `Begin` with a valid desc | OPEN | find start and end polygons (`Zenith_NavFindNearestPoint` with the extents); no start → FINISHED(NO_START); no end → search toward the raw end point (status NO_END at Finish unless a better polygon is reached); push the start node |
| OPEN | `Step(n)` | OPEN or FINISHED | the **expansion step** below, at most n times; it finishes COMPLETE when it pops the end polygon and PARTIAL (`AI_ERROR_NODE_POOL_EXHAUSTED` noted, limit NODE_POOL) at the first node it cannot allocate |
| OPEN | open list empty | FINISHED | PARTIAL toward the best node |
| any | the mesh change serial differs and a visited slot is stale | STALE | `Step` returns `AI_ERROR_STALE_POLY_REF` |
| FINISHED | `Finish` | FINISHED | walk parents from the final node (the end polygon node, or the best node, a polygon node) into the corridor, reversed; when the chain holds a dead link node ("Search graph"), the corridor is cut before the first one in start-to-end order: status PARTIAL, limit NONE, `m_uResultFlags` bit `uNAV_PATH_RESULT_FLAG_DEAD_LINK`, the end point the point of the last kept polygon closest to the requested end (the route the search found ran through a link that was unregistered while it ran, so over the graph as it stands the end was not reached); more than the span's capacity → the first `capacity` polygons, status TRUNCATED, limit CORRIDOR; the result's `m_eLimit` names the limit that ended the search ("Partial results") |

**Expansion step** (one pop; every float in binary32, every order a total order):

1. Pop the heap top b ("Tie-breaking"); clear its open bit and set its closed bit; expansions + 1. When b is a link node whose reference no longer resolves (a **dead link node**, "Search graph": its link table slot is free or carries another salt), the step ends here: b stays closed, expands nothing and creates or changes no node.
2. If b is a polygon node of the end polygon: the final node is b; FINISHED (COMPLETE); the step ends.
3. For each neighbour n of b in the canonical neighbour order — b a polygon: its in-tile neighbours by ascending edge index, then its external links in their sorted order ("Tile connection procedure"), then, when the descriptor's `m_bUseLinks` is set, the registered links of the search's class with an end in b's polygon (a unidirectional link only from its start) in link entity simulation-ID order; b a link node: its one neighbour ("Search graph") — skipping b's own parent node and every n that fails the filter (a polygon: its flags (baked | overlay) and a finite area cost; a link node: the link registered for the search's class and enabled and its area's cost finite, so a dead link node is never a neighbour):
   1. Compute n's entry position p, `g_new` ("Cost and heuristic") and `h_new` = |end − p| × hScale.
   2. Look n up in the visited lookup by its reference (the polygon's `Zenith_NavPolyRef`, or the link node's link-node form) and apply the first matching row:

| n is | Condition | Action |
|---|---|---|
| absent | the node count equals `m_uMaxNodes` | note pool exhaustion (flags bit 1), state FINISHED (PARTIAL, limit NODE_POOL); the step ends at once, the remaining neighbours unvisited (the first exhaustion ends the search) |
| absent | otherwise | create node `m_uNodeCount` (reference, p, `g_new`, parent b, open bit), insert it in the lookup, add its tile's stamp when the tile has none (polygon nodes; below), push it on the heap, run the best-node update |
| open | `g_new < g(n)` | replace its position by p, g by `g_new` and parent by b; restore the heap at its heap index by a sift-up then a sift-down (f can rise, because h changed with the position); run the best-node update |
| closed | `g_new < g(n)` | **reopen**: replace position, g and parent as above, clear the closed bit, set the open bit, push it; run the best-node update |
| open or closed | `g_new ≥ g(n)` (equality included) | no change |

4. When the heap is empty after a step and the search is not FINISHED: FINISHED (PARTIAL toward the best node, limit NONE: the open list was exhausted, the end is unreachable over the loaded tiles).

- **Best node.** The best node is the **polygon** node minimising (h, g, tie-break key) over every polygon node created so far, with h computed from its current position; a link node never becomes the best, so a PARTIAL corridor always ends in a polygon and "the best polygon" of "Partial results" is always defined. The **best-node update** of a created or changed polygon node c (a link node runs none): when c is not the current best, c becomes the best if its (h, g, key) is strictly smaller; when c is the current best, the best is recomputed by a scan of every polygon node in index order, O(N) (a relaxation can raise the best's h). The start node, a polygon node, is the first best.
- **Optimality and reopening.** Reopening a closed node whose g decreases is Detour's rule; without it N1's and N2's analytic bounds fail where a polygon is first reached through a long portal (`Test_NavPath_RelaxationRules`).
- **Tile stamps.** A stamp (`Zenith_NavSearchTileStamp`: tile key, salt, version) is added when a polygon node is created in a tile that has no stamp yet, the start node included. Stamps are therefore never more numerous than nodes, nor than loaded tiles: the stamp table's capacity is `S = min(m_uMaxNodes, ai.nav.maxTiles)` entries in the search's arena, with a membership lookup (`Zenith_HashMap` from tile key `Pack()` to stamp index, never iterated), so it can never be full before the node pool is and **truncates no search**: a search's reach is bounded by the node pool, the tick cap and the corridor capacity alone, each named in the result ("Partial results").
- STALE is detected at the next `Step` after the mesh change serial moved: the search compares every stamp, in stamp-array order, with its tile's current state, and a removed tile, a re-used slot (salt) or a changed version is STALE. A restored search's stamps take the restored tiles' salts and the saved versions. A MARK overlay change, a neighbour tile added next to an expanded polygon or a link registered or unregistered does **not** stale the search (a link unregistered under it leaves dead link nodes, "Search graph"): its graph is allowed to change while it runs, and its state after k expansions is a pure function of the navmesh **history** during the search (which overlay, tiles and links each expansion saw), the descriptor and k. A save therefore serializes the search state itself, never its expansion count alone (`Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm" step 5; `Test_AISave_SearchStraddlingOverlayChange`).
- Node record (`Zenith_NavSearchNode`, 32 bytes, public in `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavQuery header"): node reference (8: a `Zenith_NavPolyRef`, polygon form or link-node form), position (12), g (4), parent index (4, 0xFFFFFFFF for the start node), flags (4: bit 0 open, bit 1 closed).
- The open list is a binary heap of node indices ordered by "Tie-breaking", with a heap-index array (`uint32` per node, 0xFFFFFFFF when not in the heap) so a relaxation finds a node's heap slot in O(1); a visited-node lookup (`Zenith_HashMap` from node reference to node index) is used for membership only and is never iterated, so it, the heap-index array and the stamp lookup are rebuilt from the node array, the heap array and the stamps at restore without changing a result.
- **Arena block.** Both lookups are `Zenith_HashMap<uint64, uint32>`s (declared as `Zenith_NavSearchLookup`, `Zenith2/Docs/Design/AI/Internals.md` → "Search tile view"). Their storage is a dense array of 16-byte entries (an 8-byte key, a 4-byte value and 4 bytes of alignment padding) plus a `uint64` slot array. Its slot count is a power of two of at least 16, and an insert grows it when count + 1 > slots × 3 / 4 (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet", Insert step 3). A lookup that holds n entries without ever growing therefore needs **L(n) = 16 n + 8 P(n)** bytes, where P(n) is the smallest power of two ≥ max(16, ⌈4 (n + 1) / 3⌉); the + 1 is one entry of headroom over the growth rule. A search's block for N nodes and S = min(N, `ai.nav.maxTiles`) stamps is
  **B(N) = 40 N + 16 S + L(N) + L(S)** bytes: nodes 32 N, heap 4 N and heap indices 4 N; stamps 16 S; the node lookup L(N) and the stamp lookup L(S).
  - At the default N = S = 2,048: ⌈4 × 2,049 / 3⌉ = 2,732, so P = 4,096 and L(2,048) = 32,768 + 32,768 = 65,536 bytes. B = 81,920 + 32,768 + 65,536 + 65,536 = 245,760 bytes = 240 KiB = 120 N. The 64 active requests of the default `ai.path.maxActiveRequests` take 64 × 245,760 = 15,728,640 bytes = 15 MiB, within Z2-B-AI-001 ("In-memory sizes that matter" of `Zenith2/Docs/Design/AI.md`).
  - At the maximum N = 8,192 with `ai.nav.maxTiles` ≥ 8,192 (so S = 8,192): ⌈4 × 8,193 / 3⌉ = 10,924, so P = 16,384 and L = 131,072 + 131,072 = 262,144 bytes. B = 327,680 + 131,072 + 524,288 = 983,040 bytes = 960 KiB = 120 N.
  - Every part starts 8-byte aligned and has a size that is a multiple of 8 (N ≥ 256), so the block needs no alignment padding: an arena whose `m_ulMaxBytes` is exactly B(N) holds the whole search ([Z2-N-CONTRACT-261] counts requested sizes plus alignment padding).
- **Reservation.** `Begin` and `RestoreSnapshot` carve every part from the node arena before they create a node, and call `TryReserve(N)` on the node lookup and `TryReserve(S)` on the stamp lookup. Neither map can then grow inside the arena (a growth there would abandon its old arrays in the arena). A block smaller than B(N) fails `Begin` or `RestoreSnapshot` with `AI_ERROR_OUT_OF_MEMORY` before the first node, and a begun search never fails for memory (`Test_PathService_CancelReleasesNodePool`). The path service sizes every active slot's block as B(`ai.path.maxSearchNodes`). A volume search's block (`Zenith2/Docs/Design/AI/Volumes.md` → "Volume search") is smaller for every admitted configuration, so one block size serves both kinds.

### Tie-breaking

The heap orders by (f ascending, h ascending, then the node's **tie-break key** ascending). A polygon node's key is (its tile key `Pack()`, its polygon index, 0, 0); a link node's key is (0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF, 1, its node index), so every link node orders after every polygon node of equal f and h, and two link nodes order by the index at which the search created them. A link node's key is read from the node record alone, never from the link table: a dead link node ("Search graph") keeps its key while it waits in the heap, and a restored search, whose nodes keep their saved indices, orders exactly as the uninterrupted one. The node index is a function of the expansion sequence, which is itself independent of slots by induction (every earlier comparison used slot-independent keys), so the order still depends on no tile slot, link table index or session; a link node's identity outside the search stays (link entity simulation ID, class). The best node uses the polygon key after (h, g) and ranges over polygon nodes alone ("Search state and slicing"). No comparison reads a slot number, a pointer or an insertion order, so a search returns the same corridor whatever tile slots its tiles occupy — after a reload or in another session (`Test_NavPath_IndependentOfSlotAssignment`, which loads one tile set in two orders and compares corridors by (key, polygon)).

### Partial results

- Every result names its **limit** (`Zenith_NavPathResult::m_eLimit`, `Zenith_NavPathLimit`, `Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavQuery header"): NONE (COMPLETE, or PARTIAL because the open list was exhausted: the end is unreachable over the loaded tiles), NODE_POOL (PARTIAL at the first node the pool could not allocate), TICK_CAP (PARTIAL because the path service finished it at `ai.path.maxTicksPerRequest`, "Per-tick processing" step 6), CORRIDOR (TRUNCATED: more polygons than the corridor span). A limit other than NONE says the end may still be reachable, so agents continue such a move rather than fail it (`Zenith2/Docs/Design/AI/Agents.md` → "Path corridor", continuation).
- PARTIAL ends at the best node, which is always a polygon node; its end point is the point of the best polygon closest to the requested end. A TRUNCATED result's end point is defined the same way: the point of its last corridor polygon (the 256th) closest to the requested end, and that is the `m_xEnd` the result carries and the "corridor end point" of `Zenith2/Docs/Design/AI/Agents.md` → "Path corridor". A PARTIAL cut before a dead link node ("Search state and slicing", Finish row) ends at the point of its last kept polygon closest to the requested end.
- NO_END with a reachable polygon nearest the raw end point behaves as PARTIAL toward it.
- An agent without `ALLOW_PARTIAL` treats PARTIAL as unreachable (`Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine").
- A search that crosses an unloaded tile simply cannot expand into it, so a request toward an unstreamed area returns PARTIAL at the streamed boundary (the rule `Zenith2/Docs/Design/STREAM/Runtime.md` → "Navigation coordination" assigns to AI).

### Straight path extraction

`Zenith_NavFindStraightPath` is the funnel algorithm over the corridor's portals:

1. The first corner is the start (START flag).
2. Keep an apex, a left and a right portal edge; for each next portal, tighten the right side if the new right point is inside the funnel (cross product sign with an epsilon of 1e−6 m²), else, if it crosses the left side, emit the left point as a corner, restart from it; likewise for the left.
3. A link node portal emits its start as a corner with OFFMESH_START and restarts after the link at its end.
4. With `uNAV_STRAIGHT_PATH_AREA_CROSSINGS` a corner is added where the area changes; with `ALL_CROSSINGS` at every portal.
5. The last corner is the end (END flag). Corners beyond the span's capacity are not written; the count returned is the written count.
6. Corner heights come from the detail mesh at each corner's xz (`Zenith_NavMesh::GetPolyHeight`).

Complexity O(corridor length) amortised.

### Surface movement

`Zenith_NavMoveAlongSurface` runs a breadth-first search from the start polygon over at most 16 polygons, keeping the polygon whose footprint contains the target projection, or, failing that, the point of the visited boundary closest to the target; a wall slide is the closest point on the blocking edge. The result's visited list is what the agent's corridor uses to advance (`Zenith2/Docs/Design/AI/Agents.md` → "Path corridor").

- **Visit order.** The FIFO queue starts with the start polygon; a polygon's neighbours are enqueued in the canonical neighbour order of "Tile connection procedure" (in-tile neighbours by ascending edge index, then the sorted external links; off-mesh links are never followed), each only when its shared edge (clamped to the external link's overlap interval) passes within the segment's 2D distance bound (the start-to-target distance plus the class radius) and the polygon passes the filter and is not yet visited. The search stops when the queue is empty or 16 polygons are visited.
- **Choice.** Among visited polygons whose footprint contains the target's xz projection, the first in visit order wins; with none, the closest boundary point to the target among visited polygons' wall edges (an edge with no passable neighbour), ties by the lower (tile key `Pack()`, polygon index, edge index). The visited list returned is the parent chain from the chosen polygon back to the start, start first.
- So the result is a function of the loaded tiles and the arguments alone, never of tile add order or slots (`Test_NavQuery_MoveAlongSurfaceIndependentOfAddOrder`).

## Path service

### Request sources

| Source | Created by | Owner | Result goes to |
|---|---|---|---|
| agent plan | `AI.NavRequests` when an agent's `MoveSerial` changed, its corridor became invalid, its entity target moved more than `ai.agent.retargetDistance`, or its replan timer fired | the agent (simulation ID) | the agent's runtime record (no publication) |
| script request | `AI.RequestPath` (`Zenith2/Docs/Design/AI/Scripting.md` → "Path request payload and result"), a `SIDE_EFFECT_KIND_ASYNC_REQUEST` record of `ASYNC_SOURCE_PATHFINDING` committed in merge order; ZenithRuntime's multiplexer calls AI's `Zenith_AIPathRequestConsumer::Consume(worldTag, kind, completionID, payload)` on the committing merge node | the requesting behaviour instance | a publication through the module publication sink, `PUBLICATION_KIND_GENERIC_RECOMPUTED` |
| volume plan | as agent plan, for FLYING and UNDERWATER agents | the agent | as agent plan |

`ASYNC_SOURCE_PATHFINDING` is SCRIPT's registry value for `ctx:RequestPath` (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"); AI cites the enumerator and declares no value of its own.

### Request queue

- One queue per world in the `Zenith.AI.PathService` resource: capacity `ai.path.maxQueuedRequests`; entries (64 bytes): kind, owner simulation ID or completion ID, the request descriptor's index in a descriptor table, the enqueue tick.
- **The script inbox.** Script requests never touch the resource from another phase ([Z2-N-ARCH-488]): `Zenith_AIPathRequestConsumer::Consume`, called on the committing merge node of phase 2 or 10 (or the merge chain of any phase after them), appends the request to the world's **path inbox** (`Zenith_AIPathInbox`, `Zenith2/Docs/Design/AI/Internals.md` → "Path request consumer"), module state outside every world resource, written only on the world's merge chain (one merge node at a time, so no lock) and drained by `AI.NavRequests` step 1, which declares no access to it beyond its own node. Its capacity is the pathfinding value of Z2-B-SCRIPT-026, the same bound SCRIPT applies at the merge, so `Consume` never finds it full.
- Script capacity is bounded before the inbox: a script request is accepted at its merge only within the pathfinding value of Z2-B-SCRIPT-026 (pending, allocated by the Stage 1 per-system budget ticket for SCRIPT), and a rejected one rolls back its entity (BR-MERGE, ruling Q19 (a)). Agent plans enter in `AI.NavRequests` itself, before the tick's processing, in ascending simulation ID; an agent has at most one queued or active plan, and a newer plan of the same agent replaces the older one in place (`ai.path.superseded_delta`).
- **Capacity invariant.** Module start refuses (`MODULE_ERROR_REGISTRATION`, logged naming the three values) a configuration where `ai.path.maxQueuedRequests < ai.agent.maxAgents + P`, with P the pathfinding value of Z2-B-SCRIPT-026 the start context reports (`Zenith2/Docs/Design/AI.md` → "Assumed interfaces", row AI1): every agent has at most one plan and the script requests in flight never exceed P, so the queue can never be full. The "queue full" branch is kept as a counted defence (`ai.path.queue_full_delta`: the plan is deferred to the next tick and the agent stays PLANNING); it is unreachable while the invariant holds (`Test_AIModule_Start_RejectsQueueSmallerThanDemand`).

### Per-tick processing

`AI.NavRequests` (phase 7, after `AI.NavIngest`):

1. Drain the path inbox and this tick's new agent plans into the queue in the order (script requests in completion-ID order, then agent plans in simulation-ID order); the inbox is empty afterwards.
2. Cancel queued or active requests whose owner is gone (destroyed agent, destroyed or quarantined script instance) or whose agent was stopped; a cancelled script request is completed with status CANCELLED ("Cancellation").
3. Fill the active set from the queue head up to `ai.path.maxActiveRequests` (Z2-B-AI-003, BR-DEFER): the rest stay queued and each counts once per tick in `ai.path.deferred_delta`.
4. Begin every newly active request, and re-begin every active request marked restart-pending by an origin shift ("Origin shift"), from its descriptor (`Zenith_NavPathSearch::Begin` or `Zenith_NavVolumeSearch::Begin`) with its node pool from the service's request arena (one block per active slot, `ai.path.maxSearchNodes` nodes); a restart keeps the request's ticks-active count.
5. Step every active request by `ai.path.expansionsPerRequestPerTick` expansions, as sub-jobs (`ParallelFor` over the active set, grain 4). A request that returned STALE restarts once this tick from Begin (counted `ai.path.restarted_delta`); a second stale result in one tick defers it to the next.
6. After the join, in active-set order: a finished request, or one that reached `ai.path.maxTicksPerRequest` (finished early: PARTIAL toward its best node, limit TICK_CAP), is finished (`Finish`, then `Zenith_NavFindStraightPath` for script requests): an agent plan writes its corridor into the service's per-agent result slot for `AI.Locomotion`; a script request writes its payload (`Zenith2/Docs/Design/AI/Formats.md` → "Script result payloads") and parks it through the module publication sink with its completion ID.
7. Compact the active set and the queue, preserving order.

- Every input of steps 1–7 is committed simulation state, every ordering is a total order on it, and each search's state is a function of the navmesh history it saw and its expansion count, so the tick a request finishes on, and its result, are the same for every worker count and thread interleaving: the path service adds no live-run divergence of its own (`Test_PathService_DeterministicAcrossWorkers`, `Test_AIDeterminism_LiveRunsAgree` under the completion gate's fixed delay); live runs with real I/O diverge where a navigation load publishes at an I/O-timed tick, which replay covers ("Navigation asset requests").
- Script results therefore publish at the tick boundary after the tick their search finished, in completion-ID order (Z2-D-053, [Z2-N-SCRIPT-335]); under `--replay` the recomputed result is ready at the same tick as recorded, so `CanRunTick` never holds a tick for a path result.
- The expansion budget per tick is bounded by `maxActiveRequests × expansionsPerRequestPerTick` (32,768 at the defaults), the pending row need "Path node expansions per tick".

### Path request state machine

| State | Event | Next | Action |
|---|---|---|---|
| QUEUED | reaches the active set | ACTIVE | Begin (a Begin failure → FINISHED with the mapped status: INVALID_REQUEST, NO_NAVMESH, NO_START) |
| QUEUED | owner gone, or superseded | CANCELLED | script: complete CANCELLED; agent: dropped |
| ACTIVE | Step finished, or the tick cap reached | FINISHED | Finish; deliver |
| ACTIVE | Step stale | ACTIVE | restart from Begin (at most once per tick) |
| ACTIVE | owner gone | CANCELLED | as above; the node pool block returns to the arena free list |
| FINISHED | delivered | — | the slot frees; a script result's in-flight unit of Z2-B-SCRIPT-026 returns when SCRIPT releases the result ([Z2-N-SCRIPT-342]) |
| any | the world transitions away | — | every request is discarded; completions are not published ([Z2-N-ARCH-558]) |

### Publication of script results

- AI's start declares `DeclareScriptAsyncSource(ASYNC_SOURCE_PATHFINDING, uAI_PATH_RESULT_BYTES)`, registers its consumer with `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, …)` (routed by byte 0 through ZenithRuntime's multiplexer, `Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"), and registers the result decoder with `SetAsyncResultDecoder(ASYNC_SOURCE_PATHFINDING, …)` on its binding table (`Zenith2/Docs/Design/AI/Scripting.md` → "Path request payload and result").
- AI allocates no completion ID: SCRIPT assigns it at commit ([Z2-N-SCRIPT-334]). AI parks each result with `Park(completionID, ASYNC_SOURCE_PATHFINDING, PUBLICATION_KIND_GENERIC_RECOMPUTED, payload)` on the single module publication sink (ruling Q76 (a); `Zenith2/Docs/Design/AI.md` → "Assumed interfaces").
- A result for a script instance destroyed after its request committed is still parked; SCRIPT publishes and discards it (`Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results", tick-boundary publication step 4).

### Cancellation

- A path request is cancelled when its owner is destroyed or quarantined, when the agent receives a new MOVE_TO or a STOP, and when its world transitions away.
- A cancelled script request is completed with a CANCELLED payload so its ticket resolves; a cancelled agent plan produces no event beyond the `MoveCompleted` the STOP or the new MOVE_TO already implies (`Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine").
- The search's node pool block returns to the service's free list in the same step; nothing a cancelled request allocated outlives the tick (`Test_PathService_CancelReleasesNodePool`).
- The GAME scenario `Test_Scenario_WorldTransitionWithInFlightAsync` (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Scenario WorldTransitionWithInFlightAsync") issues 32 script path requests in world A in the transition tick: AI's part is that A's path service discards every request at the swap and that none of the 32 is ever published into either world.

## Spatial queries

### Nearest point

`Zenith_NavFindNearestPoint` queries the BV trees of every loaded tile of the class whose bounds overlap the box (centre ± half extents), tests each candidate polygon's footprint and detail height, and returns the closest point by 3D distance; ties by (tile key, polygon index). O(log p + candidates) per tile.

### Navmesh raycast

`Zenith_NavRaycast` walks polygons from the start along the 2D segment: for each polygon it finds the exit edge by clipping the segment against the polygon (Cyrus–Beck), moves through a passable neighbour or stops at a wall (fraction, wall normal) or at the end; when several neighbours share the exit edge (external links of one portal edge to stacked layers or split neighbour edges), the first in the canonical neighbour order whose overlap interval contains the exit point and whose polygon passes the filter is taken; visited polygons are written to the caller's span, and a full span stops the walk with the fraction reached. Used by corridor optimisation and by `AI.Raycast` scripts.

### Random reachable point

1. Breadth-first flood from the start polygon over passable neighbours whose nearest point to the centre is within the radius, at most 1,024 polygons, visiting each polygon's neighbours in the canonical neighbour order of "Tile connection procedure" (off-mesh links are not followed).
2. Choose a polygon with probability proportional to its area: one draw `u = NextUnitDouble()` against the prefix sums of areas in visit order.
3. Choose a point inside it: draw two more numbers and use the triangle-fan sampling (triangle by area, then barycentric √r1 rule).
4. Keep the point only when within the radius of the centre; otherwise repeat steps 2–3 at most 8 times, then return the polygon's nearest point to the centre.

Draws come from the caller's `Zenith_RandomSource` (a script's counter-based `ctx.Random` stream, or an agent stream keyed by its simulation ID), so the point is a function of the stream state (`Test_NavQuery_RandomPointDeterministic`).

## Navigation selection

### Candidate mapping

D-SEL-NAV (open, Stage 1/2) chooses who implements the stage contracts of "Bake stages" and the **runtime navmesh storage** the queries read: AI's own decoded neutral tile, or a `dtNavMesh` tile built from the same neutral section. By the orchestrator's ruling AI-R1 (finding 9), both candidates run AI's own search loop and AI's own straight-path, raycast, surface-movement, nearest-point and random-point code over the candidate's storage, so the search contract (the node pool limit and PARTIAL rule, the open-list tie-break, the best-node rule, the canonical neighbour order) is identical in both, no Detour source is patched, and SHIP's manifest entry carries no `patches[]` (so rule V25 of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Patch entries" has nothing to check). The neutral tile format, the tile table, the external-link lists, the overlay, links, obstacles' scheduling, the path service, volumes and every API of `Zenith2/Docs/Design/AI/PublicAPI.md` are AI's whichever candidate wins.

| Function | Recast/Detour candidate | Custom candidate |
|---|---|---|
| stages 1–4 | `rcRasterizeTriangles`, `rcFilterLowHangingWalkableObstacles`, `rcFilterLedgeSpans`, `rcFilterWalkableLowHeightSpans`, `rcBuildCompactHeightfield`, `rcErodeWalkableArea` | `ZenithAI/Internal/Build/Zenith_NavRasterize.cpp`, `Zenith_NavFilter.cpp`, `Zenith_NavCompact.cpp` |
| stage 5 | `rcMarkConvexPolyArea` per volume footprint | `Zenith_NavAreas.cpp` |
| stage 6 | `rcBuildDistanceField` + `rcBuildRegions`; `rcBuildRegionsMonotone`; `rcBuildLayerRegions` | `Zenith_NavRegions.cpp` |
| stages 7–9 | `rcBuildContours`, `rcBuildPolyMesh`, `rcBuildPolyMeshDetail` (`maxVertsPerPoly` 6; Recast's own per-polygon detail caps are 127 vertices and 255 triangles, the limits of "Bake limits") | `Zenith_NavContours.cpp`, `Zenith_NavPolyMesh.cpp`, `Zenith_NavDetail.cpp` |
| stage 10 | AI's `Zenith_NavAssemble.cpp` from `rcPolyMesh`/`rcPolyMeshDetail` (never `dtCreateNavMeshData`'s bytes, which are a C layout, Z2-D-038) | `Zenith_NavAssemble.cpp` |
| runtime storage | `dtNavMesh` tiles built at tile add from the neutral section by `Zenith_NavDetourBridge.cpp` (`dtCreateNavMeshData` in memory, never on disk); the bridge implements the search's tile view (`Zenith_NavTileView`, `Zenith2/Docs/Design/AI/Internals.md` → "Search tile view") over `dtMeshTile`'s `verts`, `polys`, `detailMeshes`, `detailVerts`, `detailTris` and `bvTree` arrays through Detour's public tile API (`dtNavMesh::getTileAt`, `getTileAndPolyByRef`) | the decoded neutral section, viewed by `Zenith_NavTileView` directly |
| search, straight path, raycast, surface move, nearest and random point | AI's `Zenith_NavSearch.cpp`, `Zenith_NavStraightPath.cpp`, `Zenith_NavRaycast.cpp`, `Zenith_NavSurface.cpp`, `Zenith_NavNearest.cpp`, `Zenith_NavRandom.cpp` over the view; `dtNavMeshQuery` is not used | the same files over the view |
| tile connections | AI's sorted external-link lists ("Tile connection procedure"); `dtNavMesh::connectExtLinks`' per-polygon `dtLink` chains (built in add order) are never read | the same lists |

<!-- informative -->
The Detour behaviours AI's loop replaces, each of which would make the Recast/Detour candidate fail N13 or N14 by construction or change a result the contract fixes (from the upstream `DetourNavMeshQuery.cpp`, `DetourNode.cpp` and `DetourNavMesh.cpp`): (1) `dtNavMeshQuery` continues past an exhausted node pool, skipping the neighbour and setting `DT_OUT_OF_NODES`, where the contract finishes PARTIAL at the first exhaustion; (2) its best node keeps the first node of least heuristic (strict `<`), where the contract breaks ties by g and then (tile key, polygon); (3) its open list (`dtNodeQueue`) orders by total cost alone, so equal-f nodes pop in an order fixed by the heap's push and pop history, which depends on link order; (4) `connectExtLinks` threads each polygon's links in the order neighbour tiles were added, so a streamed world and a restored one iterate neighbours differently; (5) `moveAlongSurface` follows that link order with a 48-entry stack. With one loop the harness measures what each ruling would ship: the bake (stages 1–10), the storage's memory and its query cost, never a difference in search semantics.
<!-- /informative -->

<!-- elaboration -->
- Neither candidate's DetourTileCache or DetourCrowd is used: carving runs AI's carve procedure over AI's layers section, and local avoidance is AI's own (`Zenith2/Docs/Design/AI/Agents.md` → "Avoidance"), because both must meet the entity-boundary rules of the frame phases, which neither component's single-threaded, completion-ordered update loop does (the scope note of `Zenith2/Docs/Design/AI.md` → "Open decisions").
- With Recast/Detour, its sources compile inside `ZenithAI` only (`THIRD_PARTY recast-detour`), never into a public header; with the custom candidate there is no third-party dependency.

### Conformance scenarios

The D-SEL-NAV acceptance rule names "the AI navigation conformance scenarios"; these are they, each a generated fixture (no committed geometry; `Zenith2/Docs/Design/AI/Tests.md` → "Fixtures") run against both candidates by the harness and against the selected implementation by `ZenithAI_Tests` afterwards. Expectations are derived from the geometry, never recorded from a candidate.

| # | Scenario | Asserts |
|---|---|---|
| N1 | flat 64 × 64 m plane, one tile size per run (16, 32, 64) | one connected region per tile; every tile connects to its 4 neighbours; a corner-to-corner path is COMPLETE and its straight length is within 1% of the analytic diagonal |
| N2 | L-shaped corridor 2 m wide | the straight path has exactly 3 corners; its length is within 2% of the analytic shortest path around the inner corner offset by the agent radius |
| N3 | two rooms joined by a 0.5 m door, cell size 0.05 m (the door is 10 cells), agent radius 0.35 | erosion is `ceil(r / cs)` cells per side: radius 0.35 erodes 7 per side and closes the door (PARTIAL, limit NONE); radius 0.2 erodes 4 per side and leaves 2 cells open (COMPLETE) |
| N4 | stairs: 20 steps of 0.3 m rise | COMPLETE with max climb 0.35; PARTIAL with max climb 0.25 |
| N5 | 46° slope ramp, max slope 45° | the ramp is not walkable; a 44° ramp is |
| N6 | two-storey building (LAYERS partition) | two layers at the column; a path from ground to upper floor uses the stair, never a straight drop |
| N7 | a 4 km² terrain tile set at W-Outdoor scale (generated heightfield, 32 m tiles, 0.25 m cells) | bake completes within the limits, and the precondition holds that every tile has at most 96 polygons (asserted on the bake before any search); **short pairs**: start/end pairs are drawn (fixed seed) on walkable polygons at most 256 m apart, and a drawn pair is kept only when it meets the **asserted precondition** of the cost oracle (`Zenith2/Docs/Design/AI/Tests.md` → "Fixtures"): the oracle reaches the end, its least cost C_ref ≤ 1.25 D (D the straight start–end distance) and its corridor holds at most 256 polygons. Drawing stops at 100 kept pairs or 10,000 draws (fewer than 100 kept fails the fixture, not the candidate). The kept pairs are searched with `ai.path.maxSearchNodes` 8,192 and `ai.path.maxTicksPerRequest` 600 (both keys' maxima), and every one is COMPLETE with limit NONE (never PARTIAL by the pool or the tick cap, never TRUNCATED); 20 further unreachable pairs (fixed seed, drawn until the oracle does not reach them) are PARTIAL with limit NONE when the oracle's start component holds at most 8,192 polygons and NODE_POOL otherwise. **Long pairs**: 20 pairs (fixed seed) at least 1,500 m apart at the default configuration, each run as one `Zenith_NavPathSearch` stepped as the path service steps it (`ai.path.expansionsPerRequestPerTick` per step, at most `ai.path.maxTicksPerRequest` steps, then `Finish`). Only **limit consistency** is asserted, because the ellipse bounds what A* may expand from above and never from below, so a long pair over large flat polygons may legitimately complete: COMPLETE ⇒ limit NONE and corridor ≤ 256; TRUNCATED ⇒ limit CORRIDOR and corridor = 256; PARTIAL with NODE_POOL ⇒ node count (`GetSnapshot().m_axNodes`) = the pool; PARTIAL with TICK_CAP ⇒ steps = the cap; PARTIAL with NONE ⇒ the oracle does not reach the end, so never PARTIAL with NONE for a reachable pair. Every result is identical on 1, 2, 8 and all workers |
| N8 | tiled streaming: add and remove tiles in random order (fixed seed) | connections exist in both directions after every add; none survives a remove; paths across a removed tile are PARTIAL at the boundary |
| N9 | carving: a 2 m box obstacle dropped into a corridor | the carved tile has no polygon inside the obstacle footprint inflated by the radius; the path detours; removing the obstacle restores the baked tile bytes |
| N10 | off-mesh links: a 3 m gap with a JUMP link | without the link PARTIAL, with it COMPLETE through the link node |
| N11 | agent sizes: classes of radius 0.35 and 0.9 on one corridor of width 1.5 m | the 0.9 class has no path, the 0.35 class has |
| N12 | non-ground: a flying volume over a canyon | the volume path (Volumes.md) is COMPLETE where the ground path is not |
| N13 | determinism: N7's pairs on a reloaded tile set loaded in reverse key order, on the candidate as it would ship (its storage under AI's loop) | corridors identical by (key, polygon); every tile's external-link list byte-identical between the two load orders |
| N14 | async query cost: 100 agents on N7 for 3,600 ticks (fixed seed) at the default configuration, agent k (k = 0 … 99, its ordinal in spawn order) replanning on the ticks where (tick + k) mod 16 = 0 toward a target drawn on a polygon within 64 m of it that meets the cost oracle's asserted precondition C_ref ≤ 1.25 × 64 m = 80 m (redrawn, fixed seed, until it does), the stand-in for W-Characters' 100 agents | pass: no plan is finished by a limit (every result COMPLETE with limit NONE: never the tick cap, the node pool or the corridor), `ai.path.deferred_delta` stays 0, and every plan's queue-plus-active latency is at most `maxTicksPerRequest` (16) ticks; measured and compared between candidates: p50 and p95 plan latency in ticks, p50 and p95 expansions per completed plan, p95 µs per 1,000 expansions, and AI's share of the phase-7 span (`AI.NavRequests` zone) |

<!-- informative -->
Why N7's and N14's expectations hold by construction, derived from the asserted precondition and not from the terrain's luck. A* never pops a node whose f exceeds the cost C* of the corridor it returns, so with h ≥ 0.999 × the straight distance every popped node lies in the ellipse of foci start and end and focal sum C* / 0.999, and every created node in one ring of polygons around it. The cost oracle runs the relaxation rule of "Search state and slicing" with h = 0 and no pool, and both it and the search return the least cost that rule can represent ("Cost and heuristic"), so C* = C_ref up to binary32 summation order. A kept N7 short pair (D ≤ 256 m, C_ref ≤ 1.25 D) therefore has an ellipse of semi-axes 0.625 D and 0.375 D, π × 0.625 × 0.375 × D² ≈ 0.74 D² ≈ 48,000 m² at D = 256 m. With its ring that is at most 70 tiles of 32 m, so at most 70 × 96 = 6,720 created polygon nodes against the pool of 8,192, and the precondition bounds the corridor at 256 polygons, the span. N14's targets (C_ref ≤ 80 m, D ≤ 64 m) give an ellipse of about 3,000 m², at most 9 tiles and 864 nodes against the default 2,048, finished within 2 to 4 ticks of 512 expansions. Agent k replans on ticks with (tick + k) mod 16 = 0, and 100 = 6 × 16 + 4, so exactly 7 plans arrive on a tick for residues 0 … 3 of (−tick) mod 16 and 6 otherwise, whatever the agents' simulation IDs. At most 7 × 4 = 28 are active against the 64 of `ai.path.maxActiveRequests`, and none is deferred. The tile-stamp table can never bind: it holds min(nodes, loaded tiles) entries ("Search state and slicing"). A long pair (D ≥ 1,500 m) has an ellipse of more than 1.6 km², which bounds what A* may expand from above only: on open terrain with h close to the distance A* expands a narrow band, so a long pair may complete within 2,048 nodes and 256 corridor polygons, and its row asserts limit consistency rather than a particular limit.
<!-- /informative -->

### Harness

- `Zenith2/Benchmarks/NavigationSelection/` builds `Zenith_Bench_NavigationSelection` (`zenith_add_benchmark(NavigationSelection … THIRD_PARTY recast-detour)`), which links both candidates behind the stage contracts and the one search loop (the custom candidate's bake stages and neutral storage as the prototype of `ZenithAI/Internal/Build/`, and the shared loop `Zenith_NavSearch.cpp` with its `Zenith_NavTileView` over each storage, compiled from the harness's own `Internal/Custom/` and `Internal/Shared/` copies until the ruling) and runs scenarios N1–N14 on each, measuring bake time per tile, tile bytes, runtime memory per tile (storage only: the loop, the node pools and the link lists are the same in both), query cost (median and p95 µs per 1,000 expansions) and N14's distributions, on the reference desktop and the S24, 10 repetitions, medians with 95% confidence intervals ([Z2-N-ARCH-983]).
- N7 (a 4 km² terrain tile set at W-Outdoor scale) and N14 (100 agents) are the harness's stand-ins for the two workloads the acceptance rule names, because W-Characters gains AI only at Stage 4 (`Zenith2/Docs/Design/MOD.md`: "`AI` joins at Stage 4"); the harness reports AI's share of a phase-7 span (`AI.NavIngest` plus `AI.NavRequests` zones), never Z2-B-AI-002's whole-phase value, which also contains GAME's nodes (row GA18).
- A `--smoke` run (N1, N2, N10 on one tile size, one repetition) is the CTest `Bench_NavigationSelection_Smoke`; the benchmark target exists in the win-release and android-arm64 presets only, so the smoke runs in the win-release `Test` step.
- The manifest entry `recast-detour` (lower-case kebab, `Zenith2/Docs/Design/SHIP/Manifest.md` → "Selection-candidate entries") is pinned with `owner` the harness target, `usage: tools`, licence Zlib (Recast Navigation's licence, which `licence_policy.json` `allowedTools` admits) and no `patches[]`; the ruling ticket moves `owner` to `ZenithAI` and `usage` to `runtime` when Recast/Detour wins ([Z2-N-ARCH-121]), or removes the entry when the custom candidate wins.
- The human runs the full measurement, commits `Zenith2/Docs/Research/Navigation/NavigationSelection.md` with the raw data, and appends the ruling to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` in the ruling ticket's diff (Z2-D-084, Z2-D-097).

### Ruling consequences

| Outcome | Edits the ruling ticket makes or schedules |
|---|---|
| Recast/Detour | manifest owner and usage move; `ZenithAI`'s `zenith_add_library` gains `THIRD_PARTY recast-detour`; the custom bake prototype and the neutral-storage view are deleted from the harness; the shared loop moves to `ZenithAI/Internal/Search/`; this file's "Candidate mapping" keeps the Recast column only; `Zenith_NavDetourBridge.cpp` (storage and view) is ticketed |
| Custom | the manifest entry is removed; the bake prototype moves to `ZenithAI/Internal/Build/` and the shared loop with the neutral view to `ZenithAI/Internal/Search/`; the Recast column and the bridge are deleted |
| either | the revalidation ticket for (AI, Stage 4, D-SEL-NAV) re-checks every AI Stage 4 ticket body; ENV's navigation hook and WORLD's navigation baking unblock ([Z2-N-ARCH-981]) |
