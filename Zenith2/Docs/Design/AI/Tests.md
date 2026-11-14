# AI design: test catalogue

Section file of `Zenith2/Docs/Design/AI.md`. It lists every AI test target, every named test with the case it asserts, the fixtures, and the gate step that runs each group. Test case names follow `"<Library>/<Type>/<Behaviour>"` and are cited as `Test_<Type>_<Behaviour>` (`Zenith2/Docs/Contracts.md` → "Test naming"); every name below is unique programme-wide (each type token begins `Nav`, `AI` or `Path`).

## Scope of this file

- Expectations are derived from the specification and the generated geometry (analytic path lengths, counts, byte equality between two runs), never recorded from a candidate implementation or committed as goldens; no AI test needs a recorded artefact, so AI has no `needs-human` recording ticket.
- No test substitutes a double for a concrete class: worlds are real headless worlds built through MOD's runtime test access (`Zenith2/Docs/Design/MOD.md` → "Zenith_RuntimeTestAccess.h") with the real Physics module and Jolt backend; the physics world, asset registry and memory governor are the real ones, and navigation loads go through the test world's real registry and loader. When a navigation load's completion reaches simulation is controlled by AI's own completion gate (`Zenith2/Docs/Design/AI/Navigation.md` → "Completion gate"; "Fixtures"), never by an ASSET control: ASSET's `Zenith_AssetKindLoaderHarness` (`Zenith2/Docs/Design/ASSET/Tests.md` → "Fixtures") builds its own registry, which is not a test world's, so it serves `Test_NavAsset_LoaderContract` alone. Cook steps get their contexts from ASSET's cook test-access functions.
- Shared test helpers are header-only under `Zenith2/Engine/ZenithAI/Tests/Support/` ([Z2-N-ARCH-255]); generated fixtures are written at test time under `${ZENITH_OUT}/test-fixtures/AI/` ([Z2-N-ARCH-257]).

## Test targets

| Target | Sources | Links | Presets |
|---|---|---|---|
| `ZenithAI_Tests` | `Zenith2/Engine/ZenithAI/Tests/*.cpp` | ZenithAI and its dependencies, ZenithPhysics_Jolt, ZenithECS_Custom | win-debug, win-release, win-asan, android-arm64 (the test APK device run) |
| `ZenithAI_Conformance` | `Zenith2/Engine/ZenithAI/Tests/Conformance/*.cpp` | as above plus ZenithECS_Flecs, ZenithECS_EnTT, ZenithPhysics_PhysX (where the preset supports it) | win-debug, win-release, win-asan |
| `ZenithCook_Tests` (AI part) | `Zenith2/Tools/ZenithCook/Tests/AI/*.cpp` | ZenithCook's test target: the importers and the `ai.navMesh` step | win-tools |
| `ZenithAuthoring_Tests` (AI part) | `Zenith2/Tools/Authoring/Tests/AI/Zenith_AIAuthoring_Tests.cpp` | ZenithAuthoring's own test target (the ANIM and AUDIO precedent): the functions of `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header" | win-tools |
| `ZenithEditor_Tests` (AI part) | `Zenith2/Tools/ZenithEditor/Tests/Navigation/*.cpp` | the editor test target | win-tools |
| `Zenith_Integration_AI` | `Zenith2/Tests/AI/` (`zenith_add_integration_tests(AI DEPENDS ZenithRuntime ZenithGameplay ZenithPhysics ZenithPhysics_Jolt ZenithPhysics_PhysX ZenithAnimation ZenithAI ZenithStreaming ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT)`) | the scenario and determinism suites | win-debug, win-release, win-asan |
| `Zenith_Bench_NavigationSelection` | `Zenith2/Benchmarks/NavigationSelection/` | both D-SEL-NAV candidates | win-release, android-arm64 |
| fuzz targets | `Zenith2/Engine/ZenithAI/Tests/Fuzz/` | ZenithAI | win-asan (libFuzzer) |

## Gate steps for AI diffs

| Group | Gate step |
|---|---|
| every unit, conformance and integration test | `Test` (CTest), on every Windows preset the Build step built |
| Android compilation of ZenithAI | `Build -Preset android-arm64` (base category, every ticket) |
| fuzz targets and win-asan runs | `Sanitizers -Preset win-asan`, when the diff touches `Zenith2/Engine/ZenithAI/**` (the category paths AI asks PROC to add, cross-area row AI7) |
| S24 device run of `ZenithAI_Tests` | the Stage 4 acceptance device run (MOD's test-APK route) |
| `Bench_NavigationSelection_Smoke` | `Test` in the win-release preset (the benchmark target exists in win-release and android-arm64 only) |
| full selection measurement, budget measurements | the `needs-human` ruling tickets (reference hardware) |

## Type and table tests

| Test | File | Asserts |
|---|---|---|
| `Test_NavPolyRef_PackUnpack` | `Zenith_NavTypes_Tests.cpp` | every polygon-form (salt 1 … 65,535, slot 0 … 1,048,575, polygon 0 … 4,095) corner case round-trips with bits 48–63 zero and `IsLinkNode()` false; every link-node-form (salt 1 … 65,535, index 0 … 16,383) corner case round-trips through `MakeLinkNode`, `GetSalt` and `GetLinkIndex` with bit 63 set, bits 20–31 and 48–62 zero and `IsLinkNode()` true; null is 0 and neither form; `MakeLinkNode(0, 0)` is 1 << 63, `IsLinkNode()` true, salt 0, and resolves to no slot (the dead link node) |
| `Test_NavTileKey_PackOrder` | same | `Pack()` orders by (class, layer, tx, tz) for negative and positive coordinates; equal keys pack equal |
| `Test_NavQueryFilter_Validate` | same | costs 0 become 1.0; a cost 0.5, a NaN and a negative cost fail with `AI_ERROR_INVALID_DESC`; +∞ is accepted; `GetMinCost` ignores excluded areas |
| `Test_AIError_Strings` | same | `Zenith_ToString` returns the enumerator's spelling for every `Zenith_AIErrorCode` and a fixed text for an unknown value |
| `Test_NavTileTable_SlotReuseBumpsSalt` | `Zenith_NavRuntime_Tests.cpp` | removing a tile and adding another into the same slot makes every old reference fail with `AI_ERROR_STALE_POLY_REF` |
| `Test_NavTileTable_Capacity` | same | with `ai.nav.maxTiles` = 64, the 65th tile is refused with `AI_ERROR_TILE_CAPACITY`, counted, and nothing else changes |
| `Test_NavTileTable_DuplicateKeyRefused` | same | a second tile with a loaded key is refused with `AI_ERROR_TILE_DUPLICATE` and the first stays connected |
| `Test_NavTileTable_SettingsMismatch` | same | a tile naming another settings hash is refused with `AI_ERROR_SETTINGS_MISMATCH` and logged once |
| `Test_NavMesh_ClassResolution` | same | classes resolve by name and hash once settings are accepted; an unknown name gives an invalid index; before any tile `FindClass` is invalid |

## Bake tests

| Test | File | Asserts |
|---|---|---|
| `Test_NavBuild_ClassParamsValidation` | `Zenith_NavBuild_Tests.cpp` | each out-of-range member of `Zenith_NavClassParams` fails with its ordinal in `m_uDetail`; tile size 48 fails |
| `Test_NavBuild_FlatPlaneOneRegion` | same | a 32 m plane tile at 0.25 m cells gives one region, polygons covering the eroded area to within one cell, four portal sides |
| `Test_NavBuild_SlopeLimit` | same | a 44° ramp is walkable and a 46° ramp is not at max slope 45° |
| `Test_NavBuild_ClimbAndStairs` | same | 0.3 m steps connect at max climb 0.35 and do not at 0.25 |
| `Test_NavBuild_LowClearance` | same | a 1.7 m ceiling gap is removed for height 1.8 and kept for 1.6 |
| `Test_NavBuild_ErosionByRadius` | same | at cell size 0.05 m a 0.5 m doorway (10 cells) closes for radius 0.35 (7 cells eroded per side) and stays open with exactly 2 cells for radius 0.2 (4 per side), scenario N3 |
| `Test_NavBuild_AreaVolumesInOrder` | same | two overlapping volumes apply in (priority, ordinal) order, the one applied last winning; a null volume cuts a hole |
| `Test_NavBuild_PartitionsAgree` | same | watershed, monotone and layers partitions give polygon sets with equal covered area (within one cell per border) on the N1 plane |
| `Test_NavBuild_TwoStoreyLayers` | same | the LAYERS partition yields two layers at the column, connected only through the stair |
| `Test_NavBuild_DetailHeights` | same | on a 10° ramp the detail mesh heights are within `detailSampleMaxError` cell heights of the analytic plane at 100 sample points |
| `Test_NavBuild_PolygonsConvexAndBounded` | same | every output polygon has 3 … 6 vertices, is convex and passes checks T1 … T9 |
| `Test_NavBuild_Limits` | same | each bake limit of `Zenith2/Docs/Design/AI/Navigation.md` → "Bake limits" succeeds at the limit and fails with `AI_ERROR_BUILD_LIMIT` and the stage number at limit + 1, including a polygon with 127 detail vertices (passes, every detail index < 6 + 127) and 128 (fails, detail 9), and a polygon whose triangulation needs 255 triangles (passes) and 256 (fails, detail 9) |
| `Test_NavBake_TileBytesIdenticalAcrossRuns` | same | building one tile twice, and once with the input triangles permuted (then sorted by the input rule), gives byte-identical tile sections |
| `Test_NavBuild_EmptyInputEmptyTile` | same | an input with only unwalkable triangles returns zero polygons and success |
| `Test_NavBuild_KeepLayersRoundTrip` | same | with `m_bKeepLayers`, `Zenith_NavRebuildTileFromLayers` with no obstacle and the class's own `m_ePartition` reproduces the baked tile section byte for byte, once with a WATERSHED class and once with a MONOTONE class |
| `Test_NavCook_SettingsDocumentSchema` | `Zenith2/Tools/ZenithCook/Tests/AI/Zenith_AINavCook_Tests.cpp` | every schema rule of `Zenith2/Docs/Design/AI/Formats.md` → "Navigation settings document" and "Agent class object" (a fifth ground class, a duplicate area code, a fixed area name, an unknown flag) is an `AI_ERROR_DOCUMENT_SCHEMA` diagnostic naming the member path |
| `Test_NavCook_NavigationDocumentProducts` | same | a fixture navigation document over a 96 × 64 m region with one class declares the MANIFEST `navigation` (primary) and exactly the six 32 m tiles the pre-pass finds non-empty, with the keys `navTile:Humanoid/32/<tx>_<tz>_0` for the six hand-computed (tx, tz), and one volume region at (−128, 0, 256) m as `navVolume:Bird/0/-128000_0_256000`; a second volume element with `"index": 7` is keyed `navVolume:Bird/7/…`; the keys are stable across a reimport |
| `Test_NavCook_InputsFromScenesAndTerrain` | same | a tile bake reads the static colliders of the listed scene (and not its dynamic bodies or triggers) and the terrain's source geometry; the opened documents and dependency outputs are in the key |
| `Test_NavCook_IncrementalOnlyTouchedTiles` | same | moving one static collider re-keys exactly the tiles its AABB (with border) touches; the others hit the DDC |
| `Test_NavCook_ByteIdenticalAcrossWorkers` | same | cooking the fixture with 1 and 8 cook workers twice each gives identical bytes for every `.znav` |
| `Test_NavCook_SourceGeometryStep` | same | a `ZNSG` blob given to the step as the intermediate `ai.navSource` becomes a SOURCE_GEOMETRY file publishing `ai.navSource` with the same bytes |
| `Test_NavCook_SourceGeometryFromEnv` | `Zenith2/Tools/Authoring/Tests/AI/Zenith_AIAuthoring_Tests.cpp` (`ZenithAuthoring_Tests`) | `Zenith_AIWriteNavSourceIntermediate` over a generated 3 × 3-vertex `Zenith_NavigationGeometry` writes the `ZNSG` bytes of `Zenith2/Docs/Design/AI/Formats.md` → "Navigation source intermediate" field for field (tile-local vertices = world − origin), `Zenith_AIReadNavSourceIntermediate` reads them back, and a 263,170-vertex input is refused |
| `Test_NavCook_CellContentRecords` | same | `Zenith_AIMakeNavigationManifestView` over 40 `Zenith_WorldProduct`s (two classes, 32 m and 16 m tiles, one volume) sorts them by the contract order; `Zenith_AIWriteCellNavigationContent` for a 128 m cell writes, through a recording `Zenith_WorldCellContentWriter`, one `Zenith.AI.NavMeshTiles` entity of 16 handles for the 32 m class and four for the 16 m class in (layer, tx, tz) order, the volume entity whose minimum lies in the cell, with each derived ID equal to `Zenith_DeriveAuthoringID` computed by the test from the hand-written inputs (the cell scene GUID and the local-ID bytes of "Cell content" step 1, asserted literally), and nothing for the neighbour cell's tiles; a malformed key is `AI_ERROR_DOCUMENT_SCHEMA` |
| `Test_NavCook_DocumentMigrationChain` | same | `Zenith_RegisterAIDocumentTypes` registers the two document types at version 1; a synthetic version-0 fixture (generated at test time) migrates with preview, backup and unknown-member preservation ([Z2-N-CONTRACT-543]) |
| `Test_NavCook_DeclareProductsInProcess` | `Zenith2/Tools/Authoring/Tests/AI/Zenith_AIAuthoring_Tests.cpp` (`ZenithAuthoring_Tests`) | `Zenith_AIDeclareNavigationProducts` over an in-memory document whose `scenes` hold two product entries `{ "scene", "offset" }` and one path, with the manifest key `navigation/3_-2` and a recording `Zenith_AINavigationProductSink`: the manifest is declared non-primary under that key first, then the tiles and volumes in key order, each followed by its blob; the `ai.navTileJob` blobs equal, byte for byte, those the importer `ai.navigation` writes for the same document saved as a file (form 2 entries with the offsets as given, the path as form 1 with offset 0); a malformed `offset` (two components, a non-finite value) is `AI_ERROR_DOCUMENT_SCHEMA` naming `scenes[i].offset` |
| `Test_NavCook_InputsFromCookedSceneWithOffset` | `Zenith2/Tools/ZenithCook/Tests/AI/Zenith_AINavCook_Tests.cpp` | a static collider box at local (5, 0, 7) in a cooked scene product named with offset (1,024.0, 10.0, −2,048.0) bakes into the tile whose minimum corner is (1,024, −2,048) with its polygons cut at tile-local (5, 10, 7) to within one cell, exactly as the same box at absolute (1,029, 10, −2,041) in a source scene; the cooked scene's key hash is in the tile's key |
| `Test_NavCook_GatherInputsThroughSource` | same | `Zenith_AIGatherBakeInputs` over a recording `Zenith_AIBakeInputSource` opens exactly the job's scenes and reads exactly its source products and the manifest's `ai.navMappings`, in job order, and returns triangles sorted by (source kind, source persistent ID, triangle index) |

## Runtime navmesh tests

| Test | File | Asserts |
|---|---|---|
| `Test_NavRuntime_TileConnectionBothDirections` | `Zenith_NavRuntime_Tests.cpp` | adding a 2 × 2 grid of tiles in any of the 24 orders produces external-link lists byte-identical across the orders (compared as raw `Zenith_NavExternalLink` arrays with the slot cache field masked), each link present in both tiles and each list sorted by (polygon, edge, neighbour key, neighbour polygon); with the per-tile capacity forced to 4 through the test world's tile-table override on `BuildOverflowFixture` ("Fixtures": four 16 m tiles whose explicit polygons give each tile 7 to 9 candidates, polygons of two tiles competing for one neighbour's capacity), the 24 add orders and the sequence add-all, remove tile (0, 0), re-add it give byte-identical lists equal to the set the overflow oracle computes from the definition (rank by overlap in cells, then (polygon, edge, neighbour key, neighbour polygon); a candidate exists iff it ranks below 4 in both tiles), and `ai.nav.links_dropped` counts the same non-existing candidates in every order; the test also carries, as a comment, the hand derivation of that set from the fixture's polygons, which review checks against the oracle's output |
| `Test_NavRuntime_TileRemoveDisconnects` | same | removing a tile leaves no external link naming its slot in any neighbour |
| `Test_NavRuntime_StreamTileLoadUnload1000Cycles` | same | 1,000 randomised add/remove cycles (fixed seed) with 20 moving agents leave no live slot, asset reference, link record or allocation (the test main's leak check) |
| `Test_NavRuntime_OriginShiftKeepsCorridors` | same | after an origin shift S = (10,240, 0, −8,192) m played at sync point 1: every agent keeps its corridor references; every tile's L equals `(float)(absolute − O_new)` bit for bit; every corner, target, link endpoint and stuck anchor equals `(float)((double)p − S)` of its pre-shift value bit for bit; a script `AI.FindNearestPoint` in phase 2 of the rebase tick returns the pre-shift result minus S; a search active across the shift is restarted in that tick's phase 7 from its shifted descriptor (`ai.path.restarted_delta` + 1) and its corridor equals the unshifted run's translated by S; a blackboard POSITION value is shifted and a VECTOR value is not; Agent 20 and 44 and PerceptionState 23 were shifted by SCENE and every PerceivedTarget 3 of every perceiver's Memory by AI's listener, each bit for bit by the formula; at the first phase 7 after the shift no obstacle is dirty and no tile is queued for a rebuild (one MARK and one CARVE obstacle at rest, one of them a child of a moving building), and no link is re-registered; a script path request committed in the previous tick's phase 10 and still in the inbox plans from its shifted from; a save taken after the shift restores every carved tile with its saved check (no `ai.save.carve_mismatch`); `ai.nav.origin_late_delta` stays 0 |
| `Test_NavRuntime_TileJoinsAtPublication` | same | a tile entity activated at tick t is requested in t's merge node (one completion ID of `ASYNC_SOURCE_NAVIGATION`); the completion gate holds its completion until tick t + 8 (`m_axGateHolds`); the result is parked in phase 7 of t + 8 and published at that tick's boundary, and the tile joins in phase 7 of t + 9, never earlier; `m_uGateLateReleases` is 0 (the completion reached the gate before t + 8, the test's asserted precondition); `ai.nav.loads_in_flight` is 1 until the publication. That no phase node calls `GetState` or `Resolve` is a review check of ZenithAI's phase-node sources, not a test assertion |
| `Test_NavRuntime_TileJoinReplayedAtRecordedTick` | same | a run recorded with one tile load released by the completion gate at t + 5 replays with the gate releasing it at t + 1 (a hold of t + 1) and at t + 9 (a hold of t + 9): the tile joins at the recorded tick in both replays (the second replay holds the boundary until the gate releases the record, exactly as an unfinished load holds it) |
| `Test_NavRuntime_AbandonedTileLoadCompletedOnce` | same | an entity destroyed while its tile load is ISSUED (its completion held by a gate hold 100 ticks away), and another while PARKED_OK: each completion ID is completed exactly once (the abandoned payload, and the dropped parked result), `RequestCancel` is called on the first and its gate entry is dropped, nothing joins, and the result table holds no entry of either after the next boundary |
| `Test_NavRuntime_FailedTileLoadNotRetried` | same | a tile whose load fails is published as RECORDED with the 24-byte result, counted `ai.nav.loads_failed`, and not requested again until its entity's `InActiveRegion` turns false and true |
| `Test_NavRuntime_LoadsInFlightCapped` | same | with `ai.nav.maxLoadsInFlight` 16 and 40 tile needs in one tick, 16 completion IDs are allocated in collected order, 24 needs count `ai.nav.loads_deferred_delta` and issue at the following ticks in the same order |
| `Test_NavRuntime_InactiveRegionTileAbsent` | same | a tile on an entity with `InActiveRegion` false is neither requested nor added; when the flag turns true at tick t its load is requested in t's merge node (one completion ID), the completion gate holds the completion until t + 8, the result publishes at the boundary of t + 8, and the tile joins in phase 7 of t + 9, never earlier (`m_uGateLateReleases` 0); when the flag turns false again the tile is removed in that tick's phase 7 |
| `Test_NavRuntime_ManifestRequestedOnce` | same | a non-streamed world with one `Zenith.AI.NavMeshRef` runs 600 ticks: exactly one `ASYNC_SOURCE_NAVIGATION` completion ID is allocated, every manifest entry joins once, `ai.nav.tiles_refused` and the `AI_ERROR_TILE_DUPLICATE` log stay silent, and the supplier slot is JOINED from the publication on; replacing the handle by value with another manifest removes the first manifest's tiles and requests the second once |
| `Test_NavRuntime_LateShiftUsesAppliedOrigin` | same | with AI's listener suppressed for two consecutive shifts S1 = (2,048, 0, 0) and S2 = (0, 0, −2,048) (the test world's listener mask), the next `AI.NavIngest` applies the shift table once with S = S1 + S2 computed as O − `m_adAppliedOrigin`, `ai.nav.origin_late_delta` = 1, and every tile's L and agent corner equals the unsuppressed run's bit for bit |
| `Test_NavRuntime_EditWorldHasNavMeshNoAgents` | same | in worlds created with `WORLD_PURPOSE_EDIT` and `WORLD_PURPOSE_PREVIEW` (both with origin `WORLD_ORIGIN_STARTUP`) `AI.NavIngest` is the only AI node and no participant is registered; agents do not move and no path request is processed (row ED6); a `WORLD_PURPOSE_PLAY` world registers all seven nodes |
| `Test_NavFormats_TileSectionChecks` | `Zenith_NavFormats_Tests.cpp` | a generated valid tile passes; one mutation per check T1 … T9 fails with that check's detail, including T7 with a polygon of 128 detail vertices, one of 256 detail triangles and one detail index equal to the polygon's vertex count + detail vertex count |
| `Test_NavFormats_VolumeSectionChecks` | same | V1 … V3 likewise |
| `Test_NavFormats_InfoAndRoleRules` | same | a role missing its required section, or carrying a forbidden one, fails with the kind in the detail |
| `Test_NavFormats_ParserLimits` | same | every row of `Zenith2/Docs/Design/AI/Formats.md` → "Parser limits" at the limit passes and at limit + 1 fails |
| `Test_NavAsset_LoaderContract` | `Zenith_NavAsset_Tests.cpp` | through `Zenith_AssetKindLoaderHarness`, each role loads, reports resident bytes, honours cancel before and after section reads, and a corrupt file fails without an object (FAIL policy) |

## Obstacle and link tests

| Test | File | Asserts |
|---|---|---|
| `Test_NavObstacle_MarkBlocksPolygons` | `Zenith_NavObstacle_Tests.cpp` | a MARK cylinder flags exactly the polygons its inflated footprint overlaps; paths avoid them; moving it by less than `MoveThreshold` changes nothing |
| `Test_NavObstacle_MarkAreaOverride` | same | a MARK box with area `offPath` re-areas the polygons; the highest simulation ID wins where two overlap |
| `Test_NavObstacle_MarkTruncationOrder` | same | a MARK obstacle over 300 polygons of three tiles marks exactly the first 256 in (tile key, polygon) order whatever order the tiles were added in, counts `ai.nav.mark_truncated` once, and re-applies to the same 256 when a fourth tile is added under it |
| `Test_NavObstacle_ChildObstacleWorldPose` | same | a MARK obstacle parented to a building entity at local (2, 0, 0), the building moved to (10, 0, 0) at tick t, marks the polygons around (12, 0, 0) from tick t + 1 (the propagated `Zenith.WorldTransform`), never around (2, 0, 0); an agent following a child MoveTargetEntity replans toward the child's world position; a child link entity snaps its ends at its world pose |
| `Test_NavObstacle_DirtyByAppliedRecord` | same | an obstacle whose ECS change bits are all set but whose pose and properties equal its applied record is not dirty; one moved by `MoveThreshold` + 0.01 m, or whose HalfExtents changed, is; change bits are never read |
| `Test_NavCarve_CutsFootprint` | same | a CARVE box in a corridor leaves no polygon whose centroid lies inside the inflated footprint; the path detours (scenario N9) |
| `Test_NavCarve_RemovalRestoresBakedTile` | same | removing the obstacle restores the tile's polygons byte-identical to the baked section |
| `Test_NavCarve_FallbackToMark` | same | CARVE on a tile without layers MARKs instead and counts `ai.nav.carve_fallback_delta` |
| `Test_NavCarve_BudgetAndOrder` | same | with 10 queued tiles and `maxTileRebuildsPerTick` 4 the tiles rebuild over 3 ticks in (first-queued tick, key) order |
| `Test_NavCarve_DeterministicAcrossWorkers` | same | the tick each carved tile becomes visible and its bytes are equal on 1, 2, 8 and all workers |
| `Test_NavCarve_MatchesCookRebuild` | same | a runtime carve rebuild and the cook-side rebuild of the same layers and obstacles give identical tile bytes |
| `Test_NavCarve_InvalidatesCorridors` | same | an agent whose corridor crosses a carved tile replans in the same tick it sees the version change |
| `Test_NavLink_RegisterBothEnds` | `Zenith_NavLink_Tests.cpp` | a link whose ends snap within `SnapDistance` registers per class in `ClassMask`; one that does not stays unresolved and registers when its tile is added |
| `Test_NavLink_CrossCellBothTilesRequired` | same | a link across two tiles registers only while both are loaded and unregisters when either is removed |
| `Test_NavLink_JumpTraversal` | same | an agent crosses the N10 gap: `OffMeshLinkReached`, TRAVERSING_LINK for `Duration`, positions on the parabola within 1e−3 m, then MOVING |
| `Test_NavLink_ScriptedWaitsForCompletion` | same | a SCRIPTED link holds the agent in WAITING_LINK until `AI.CompleteOffMeshLink` plays back, then teleports it to the end |
| `Test_NavLink_DisabledLinkReplans` | same | disabling a link under a WAITING agent makes it replan from its position |
| `Test_NavLink_CapacityPerPoly` | same | a ninth link on one polygon is refused with `AI_ERROR_LINK_CAPACITY` |

## Path tests

| Test | File | Asserts |
|---|---|---|
| `Test_NavPath_PlaneDiagonalOptimal` | `Zenith_NavPath_Tests.cpp` | scenario N1: COMPLETE; the straight path has 2 corners; length within 1% of the analytic diagonal for every tile size |
| `Test_NavPath_LCorridorThreeCorners` | same | scenario N2: 3 corners; length within 2% of the analytic shortest path |
| `Test_NavPath_PartialTowardBest` | same | an unreachable end gives PARTIAL ending at the reachable point closest to it |
| `Test_NavPath_NoStartNoEnd` | same | a start outside every extent gives NO_START; an end outside gives NO_END with the best partial corridor |
| `Test_NavPath_NodePoolExhaustion` | same | with 256 nodes on the N7 terrain a long query returns PARTIAL with `AI_ERROR_NODE_POOL_EXHAUSTED` noted and limit NODE_POOL, ending at the first node it could not allocate (the node count is 256 and the remaining neighbours of that pop were not visited), never a crash |
| `Test_NavPath_StampTableNeverBinds` | same | a 2,048-node search over a 20 × 20 grid of 16 m tiles (400 tiles, about 8 polygons each) finishes with limit NODE_POOL, never earlier, holding one stamp per tile it created a node in (more than 64), and a 400-tile search with 8,192 nodes reaching the end is COMPLETE |
| `Test_NavPath_RelaxationRules` | same | on `BuildRelaxationFixture` ("Fixtures": explicit polygons in tile-local metres, stepped one expansion at a time and inspected through `GetSnapshot`), where polygon Q is first reached from polygon A through a 6 m portal and then from polygon B through a 1 m portal with a lower g: Q's position, g and parent are replaced by the B entry and its heap slot re-ordered; a closed polygon subsequently reached with a lower g is reopened and re-expanded; an equal g changes nothing; the best node is the polygon node of least (h, g, key) after each change and never a link node; a link node's key orders after every polygon key of equal f and h, and two link nodes by node index; the pop sequence and the expansion count equal the hand derivation the test carries as a comment (each pop with its node's g, h and f to 3 decimals, from the fixture's coordinates), which review checks; the design states no literal count |
| `Test_NavPath_SlicedEqualsUnsliced` | same | a search stepped 1, 7, 64 and 100,000 expansions at a time yields identical corridors and expansion counts |
| `Test_NavPath_IndependentOfSlotAssignment` | same | scenario N13: one tile set loaded in two orders gives corridors identical as (tile key, polygon) lists for 100 fixed pairs |
| `Test_NavPath_CostProfiles` | same | a road strip with cost 1.0 and grass at 2.0 makes the path follow the road when it is less than twice as long; water +∞ is never crossed by a walker and is by a swimmer profile |
| `Test_NavPath_TruncatedCorridor` | same | a path needing 300 polygons returns TRUNCATED with limit CORRIDOR and the first 256; the TRUNCATED result's `m_xEnd` is the point of the 256th polygon closest to the requested end; a tick-capped request (`maxTicksPerRequest` 1, `expansionsPerRequestPerTick` 32) returns PARTIAL with limit TICK_CAP; an unreachable end in a component of 40 polygons returns PARTIAL with limit NONE |
| `Test_NavPath_UnregisteredLinkNodeIsDead` | same | a JUMP link node is created in a search, and its link is disabled before the node is popped: the pop closes it with no neighbour created (expansions + 1), the search never creates or relaxes a node through that link again, and its corridor and expansion sequence after that tick equal those of a run whose link was unregistered at the same tick before any of its nodes existed, apart from the dead node itself; a second search pops the link node alive and creates its far polygon, then the link is disabled before the search finishes through it: `Finish` cuts the corridor before the dead link node, PARTIAL with limit NONE and `uNAV_PATH_RESULT_FLAG_DEAD_LINK`, ending at the point of the link's entry polygon closest to the end; an agent plan with that flag is re-queued from PLANNING (`ai.path.restarted_delta` + 1) and its new plan routes without the link; a dead link node waiting in the heap is ordered by its key with no link table read |
| `Test_NavPath_StaleDuringSearchRestarts` | same | removing a visited tile, or carve-swapping it, between two steps makes `Step` return `AI_ERROR_STALE_POLY_REF`; the restarted search succeeds or reports PARTIAL at the removal |
| `Test_NavPath_OverlayChangeDoesNotStale` | same | a MARK obstacle moving across an active search's frontier between two steps, a neighbour tile added and a link registered do not stale it; its next step expands under the new overlay |
| `Test_NavPath_SnapshotRoundTrip` | same | `GetSnapshot` of a search after 1, 7 and 300 expansions, converted to the save form and back and given to `RestoreSnapshot` on a mesh whose tiles occupy other slots, yields a search whose next 3 steps and corridor equal the original's bit for bit |
| `Test_NavPath_StraightPathFlags` | same | START and END flags on the first and last corners; OFFMESH_START at a link; AREA_CHANGE corners with the option |
| `Test_NavQuery_NearestPointTieBreak` | `Zenith_NavQuery_Tests.cpp` | two equidistant polygons in different tiles resolve to the lower (tile key, polygon) |
| `Test_NavQuery_QueryPolygonsExactFilter` | same | a test tile view whose `QueryPolygons` returns every polygon of the tile (a superset, as a coarser BV quantisation would) gives nearest points, start and end polygons and raycasts identical to the neutral view's on 1,000 fixed queries |
| `Test_NavQuery_RaycastWallNormal` | same | a ray into a wall returns the fraction to within 1e−4 and the wall's outward normal; a clear ray returns 1.0 |
| `Test_NavQuery_RandomPointDeterministic` | same | equal streams give equal points; 10,000 draws all lie on reachable polygons within the radius; disconnected islands are never chosen |
| `Test_NavQuery_MoveAlongSurfaceSlides` | same | moving into a wall at 45° slides along it; visited lists start at the start polygon |
| `Test_NavQuery_MoveAlongSurfaceIndependentOfAddOrder` | same | on a 3 × 3 tile grid with stacked layers at one portal, loaded in 6 different orders, 500 fixed (start, target) pairs give identical positions and visited lists as (tile key, polygon) sequences; the raycast over the same pairs likewise |
| `Test_PathService_DeterministicAcrossWorkers` | `Zenith_PathService_Tests.cpp` | after a warm-up that steps until `ai.nav.loads_in_flight` is 0 (every N7 tile joined; no load is issued afterwards), 200 mixed agent and script requests finish at the same ticks, relative to the end of the warm-up, with the same results on 1, 2, 8 and all workers |
| `Test_PathService_DeferralCounted` | same | with `maxActiveRequests` 4 and 10 requests, 6 are deferred the first tick (`ai.path.deferred_delta` = 6) and every request finishes |
| `Test_PathService_TickCapPartial` | same | a request that needs more than `maxTicksPerRequest` slices finishes PARTIAL at the cap |
| `Test_PathService_SupersededAgentPlan` | same | two MOVE_TOs for one agent in one sync point leave one queued plan; `ai.path.superseded_delta` counts the other |
| `Test_PathService_CancelReleasesNodePool` | same | destroying the owner of an active request returns its node block; 1,000 request/destroy cycles leave the arena's free list full; a `Zenith_NavPathSearch` given an arena whose `m_ulMaxBytes` is exactly B(8,192) = 983,040 bytes (`Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing") completes a search that creates all 8,192 nodes with no allocation after `Begin`, and one byte less fails `Begin` with `AI_ERROR_OUT_OF_MEMORY` before creating a node; `RestoreSnapshot` of a 8,192-node snapshot behaves alike |
| `Test_PathService_ScriptResultPublishedNextTick` | same | a behaviour's request committed in phase 2 of tick t finishes in phase 7 of t and is readable from tick t + 1; results publish in completion-ID order |
| `Test_PathService_ScriptRequestCancelledPayload` | same | a request whose instance is destroyed is completed CANCELLED and discarded by SCRIPT; no in-flight unit leaks |
| `Test_PathService_ReplayRecomputesSameTick` | same | under `--replay` of a recorded run every recomputed path result is ready at its recorded tick, so `CanRunTick` never holds |

## Agent and avoidance tests

| Test | File | Asserts |
|---|---|---|
| `Test_AIAgent_StateMachineTable` | `Zenith_AIAgent_Tests.cpp` | every row of `Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine" is driven once and reaches its next state and action |
| `Test_AIAgent_OneCompletionPerSerial` | same | 50 random sequences of MOVE_TO, STOP, PAUSE and arrival produce exactly one `MoveCompleted` per answered serial and `CompletedSerial` monotonic |
| `Test_AIAgent_SupersededMoveAnswered` | same | a MOVE_TO superseded in the same sync point is answered STOPPED in the next phase 7 |
| `Test_AIAgent_ArrivesWithinAcceptRadius` | same | an agent driven through the real Jolt character controller stops within AcceptRadius of the target on N1 and N2 |
| `Test_AIAgent_StuckFailsAfterTwoStrikes` | same | an agent blocked by a dynamic body fails after two stuck windows with one replan between |
| `Test_AIAgent_SuspendedOutsideActiveRegion` | same | `InActiveRegion` false suspends the agent and cancels its active plan; true resumes with a replan |
| `Test_AIAgent_TeleportDropsCorridor` | same | a teleport mid-move drops the corridor and replans from the target position; no velocity carries over |
| `Test_AIAgent_NotRootNeverMoved` | same | an agent parented to another entity is counted in `ai.agent.not_root` and receives no transform or controller command |
| `Test_AIAgent_ControllerOutputCommands` | same | a moving controller agent records DesiredVelocity, DesiredYawRadians and InputFlags with `USE_DESIRED_YAW`; an idle agent records one zero velocity and then nothing |
| `Test_AIAgent_KinematicOutputCommands` | same | a KINEMATIC ground agent's position follows the surface height within 0.01 m; LocalTransform commands are applied at the phase-7 sync point |
| `Test_AIAgent_TurnRateLimit` | same | a 180° reversal at MaxTurnRate π rad/s takes 1 s of ticks |
| `Test_AIAgent_RejectedBufferResends` | same | a merge rejection of an agent's buffer (the command capacity of Z2-B-ECS-004 forced to 0 for one tick) leaves its last-written values unchanged so the next tick re-records them |
| `Test_AIAgent_RejectedCompletionReemitted` | same | an agent arriving in a tick whose buffer is rejected: `CompletedSerial` is written that tick, no `MoveCompleted` is delivered, and the next tick delivers exactly one `MoveCompleted { ARRIVED }` for that serial (`ai.agent.reemitted_delta` + 1); four rejected ticks with two answers each keep all 8 pending and the next accepted tick delivers them in serial order; a fifth rejected tick with two more answers drops the two oldest, counted `ai.agent.completion_dropped_delta` = 2 |
| `Test_AIAgent_RootMotionControllerFollowsCorridor` | same | a controller in `CHARACTER_CONTROLLER_MODE_ROOT_MOTION_WITH_GRAVITY`, whose root motion a test phase-3 system writes from the previous tick's Agent 43 and 48 (`Zenith.Animation.RootMotion` DeltaPosition = the velocity in the entity frame × Δt, DeltaRotation = the yaw delta): AI records no DesiredVelocity, DesiredYawRadians or `USE_DESIRED_YAW` command; the agent reaches the N2 target within AcceptRadius within 30 s; with the stand-in delivering zero motion the move FAILS after two windows, and with it delivering half the requested speed no strike fires |
| `Test_AIAgent_KinematicTierThreeKeepsMoving` | same | a kinematic agent at tier 3 records a position command every tick (not 1 in 8), its position advancing by `v_held Δt` each tick on the surface, and a full update every 8 ticks |
| `Test_AIAgent_LongMoveContinues` | same | an agent without `ALLOW_PARTIAL` told to move 1,800 m across N7 at the default configuration receives a PARTIAL plan with limit NODE_POOL, moves, queues a continuation when within 8 m of the corridor's end with MoveStatus staying MOVING while it is queued and active, and arrives through at least 2 continuations, each ending at least 1 m nearer the target; a target on an unreachable plateau makes the move FAIL `UNREACHABLE` when a continuation gains less than 1 m; a target entity fleeing at 2 m/s straight away from the agent (a MoveTargetEntity whose retargets re-record the distance) is followed through at least 2 continuations without FAILED; with `ai.path.maxTicksPerRequest` 16 and an agent placed 0.5 m from a continuing corridor's end as its continuation replan is queued, the agent holds there with desired velocity zero, no `MoveCompleted` is emitted (neither ARRIVED nor `PARTIAL_ARRIVED`) until the continuation installs, and it then moves on |
| `Test_AIAgent_ContinuationStateCleared` | same | a continuing agent (flag set, distance d recorded) given in turn a STOP and an opposite 1,500 m MOVE_TO, a new MOVE_TO with no STOP, a teleport, a `SET_MEDIUM` change and a ragdoll enter and recover: after each, the record's `m_bContinuing` and `m_fContinuationDistance` are 0 and the next limited plan installs without the 1 m test, so the opposite move continues rather than failing `UNREACHABLE` at its first plan |
| `Test_AIAgent_ContinuationOneMetreRuleScope` | same | a continuing agent whose corridor is invalidated by a carve right after a continuation install, so that its REPLAN ends only 0.4 m nearer the target, installs it and re-records the distance (no FAILED); a first stuck strike's replan and a retarget re-record likewise; only a CONTINUATION plan gaining less than 1 m fails the move `UNREACHABLE`; `m_ePlanReason` equals the queuing cause in every case |
| `Test_AIAgent_PauseResumesPriorState` | same | PAUSE from TRAVERSING_LINK and from MOVING resumes to the same state with the link traversal and corridor intact |
| `Test_AIAgent_CapacityRefused` | same | the 17th agent with `ai.agent.maxAgents` 16 stays IDLE and is counted |
| `Test_AIOrca_HeadOnPassSafely` | `Zenith_AIOrca_Tests.cpp` | two agents walking head-on pass with a minimum separation ≥ 0.95 × (r1 + r2) |
| `Test_AIOrca_CircleSwap` | same | 32 agents on a 20 m circle swapping to antipodes reach their targets within 60 s with no pair closer than 0.9 × R |
| `Test_AIOrca_PriorityShare` | same | of two agents with priorities 255 and 0, the low-priority one makes at least 80% of the deviation |
| `Test_AIOrca_NonAvoidingNeighbour` | same | an agent takes full responsibility around a neighbour without the AVOIDANCE flag |
| `Test_AIOrca_InfeasibleFallback` | same | a boxed-in agent gets the minimum-violation velocity, never NaN, and never crosses a boundary segment |
| `Test_AIOrca_OrderIndependent` | same | the chosen velocities of 100 agents are bit-identical when their chunk order is reversed and when processed on 1 or 8 workers |
| `Test_AIOrca3D_SpheresAvoid` | same | two flyers on crossing 3D lines keep a separation ≥ 0.95 × R |
| `Test_AIOrca_AvoidsPlayerController` | same | a non-agent character controller walking head-on into an agent: the agent takes full responsibility (σ = 1) and keeps a separation ≥ 0.95 × R; the controller entity gets no runtime record and no AI command |
| `Test_AICrowdGrid_SortedAndComplete` | `Zenith_AICrowdGrid_Tests.cpp` | every agent appears once, sorted by (cell, simulation ID); neighbour queries return exactly the brute-force set within the radius |
| `Test_AILod_TiersAndHysteresis` | `Zenith_AILod_Tests.cpp` | distances 29, 31, 61 and 121 m give tiers 0, 1, 2, 3; a move from 31 to 29.5 m keeps tier 1 (hysteresis) and to 28 m changes it |
| `Test_AILod_DueTicksSpread` | same | at interval 4, a 400-agent population is due 100 per tick |
| `Test_AILod_DeterministicAcrossWorkers` | same | tiers and due ticks are equal on 1, 2, 8 and all workers and never depend on a camera |
| `Test_AILod_ScaledAwareness` | same | a perceiver at interval 4 reaches the same awareness after 40 ticks as one at interval 1 within 1e−5 |

## Volume tests

| Test | File | Asserts |
|---|---|---|
| `Test_NavVolume_BakeSolidInflation` | `Zenith_NavVolume_Tests.cpp` | a 1 m cube inflated by 0.3 m marks exactly the voxels whose centres lie within the inflated box at 0.5 m voxels |
| `Test_NavVolume_CollapsedNodes` | same | an empty 64 m region is one AIR node; one cube at its centre gives the analytic node count per level |
| `Test_NavVolume_WaterMedium` | same | voxels under a water box's surface are WATER, above AIR, inside solids SOLID |
| `Test_NavVolume_PathAroundPillar` | same | a flyer's path around a pillar is COMPLETE, every corner has line of sight to the next, length within 3% of the analytic tangent path |
| `Test_NavVolume_CanyonScenario` | same | scenario N12: a canyon the ground class cannot cross is crossed by the volume class |
| `Test_NavVolume_DiverStaysInWater` | same | an UNDERWATER non-amphibious path never enters AIR; an amphibious path surfaces once with the medium-change cost |
| `Test_NavVolume_LineOfSightDDA` | same | segments through collapsed nodes and leaf voxels agree with a brute-force voxel walk on 1,000 random segments |
| `Test_NavVolume_SearchDeterministic` | same | sliced and unsliced volume searches agree; results are equal on reloaded volumes |

## Perception tests

| Test | File | Asserts |
|---|---|---|
| `Test_AIPerception_ConeAndRange` | `Zenith_AIPerception_Tests.cpp` | targets inside the primary cone gain, inside only the peripheral cone gain at the scale, outside both never gain |
| `Test_AIPerception_LineOfSightBlocked` | same | a static wall on a SightBlockers layer stops gain; a wall on another layer does not; the target's own collider never blocks |
| `Test_AIPerception_SideRaysAroundCorner` | same | a target half behind a pillar is seen through a side ray when Radius > 0 |
| `Test_AIPerception_GainCurve` | same | with GainNear 0.8, GainFar 0.2, Visibility 1, SightRange 30 and the target in the primary cone, awareness from 0 after 1 s at distances 0, 15 and 30 m equals 0.8, 0.5 and 0.2 within 1e−5 |
| `Test_AIPerception_DecayAndForget` | same | an unseen target decays to 0 and is forgotten after MemorySeconds with reason 1 |
| `Test_AIPerception_LevelsEmitEvents` | same | crossing SuspiciousThreshold and reaching 1.0 emit `Zenith.AI.Stimulus` with the right From and To in entry order |
| `Test_AIPerception_HearingFalloff` | same | a noise of loudness 20 m heard at 10 m adds 0.25; at 21 m nothing; recorded in phase 10 it is heard the next tick |
| `Test_AIPerception_NoiseCapacityDropsInMergeOrder` | same | 300 noises in one tick keep the first 256 in merge order and count 44 dropped |
| `Test_AIPerception_DamageForcesAlert` | same | DAMAGE sets awareness 1.0 and the last known position to the instigator's |
| `Test_AIPerception_MemoryEviction` | same | a 17th target replaces the lowest-awareness entry only when it is more aware; the evicted one is forgotten with reason 4 |
| `Test_AIPerception_DestroyedTargetForgotten` | same | destroying a remembered target removes its entry with reason 2 at the next update |
| `Test_AIPerception_RayBudgetDivision` | same | B = 512: 100 due perceivers get 5 each and the 12 lowest simulation IDs one more; 600 due perceivers get base 0 and the 512 lowest simulation IDs one each, the other 88 defer their update (awareness kept, `ai.perception.rays_deferred_delta` counted); 10 due perceivers get base 8 and no more (the remainder 432 unused), never above 8; the total never exceeds 512 |
| `Test_AIPerception_TeamMask` | same | a source whose team bit is clear in PerceiveTeams is never a candidate; a perceiver never perceives itself |
| `Test_AIPerception_DeterministicAcrossWorkers` | same | 50 perceivers and 50 sources moving for 600 ticks give equal memories and events on 1, 2, 8 and all workers |
| `Test_AIPerception_RejectedEventsReemitted` | same | a perceiver whose buffer is rejected in the tick a target reaches SUSPICIOUS and in the tick another is forgotten: PerceptionState is committed both times, no event is delivered in those ticks, and the next accepted tick delivers one `Stimulus { From UNAWARE, To SUSPICIOUS }` and one `TargetForgotten` with the original reason; a target that went UNAWARE → SUSPICIOUS → ALERT across two rejected ticks yields one `Stimulus { UNAWARE, ALERT }` |

## Blackboard tests

| Test | File | Asserts |
|---|---|---|
| `Test_AIBlackboard_SetGetEveryKind` | `Zenith_AIBlackboard_Tests.cpp` | each value kind round-trips through a SET command and `Find` |
| `Test_AIBlackboard_VisibilityBySyncPoint` | same | a phase-2 SET is read in phase 10 of the same tick; a phase-10 SET from phase 1 of the next tick |
| `Test_AIBlackboard_ExpiryReadsAbsent` | same | an entry with ttl 1 s reads absent from its expiry tick and is not saved; an owner holding 64 keys of which 3 are expired accepts a new-key SET live, and a save taken at that tick and restored accepts it alike (the counts and the AI state hash equal in both); the prune removes expired records in (expiry, owner, key hash) order |
| `Test_AIBlackboard_InternFullRefused` | same | with the world's string table one entry from Z2-B-SCRIPT-022's capacity, a SET whose key and NAME value both need interning is refused whole and counted `ai.blackboard.intern_full_delta` |
| `Test_AIBlackboard_PositionAndVectorKinds` | same | `SetBlackboardPosition` stores kind 6 and `SetBlackboard` with a vector kind 3; `GetBlackboard` returns a `vector` for both |
| `Test_AIBlackboard_OwnerLimits` | same | the 65th key of an owner is refused and counted; the world capacity refuses likewise |
| `Test_AIBlackboard_DestroyedOwnerPruned` | same | destroying the owner removes its entries at `LIFECYCLE_EVENT_DESTROYED` |
| `Test_AIBlackboard_SharedSquadEntity` | same | two behaviours writing one squad entity's keys in one phase apply in merge order, the last in merge order winning |
| `Test_AIBlackboard_StaleEntityValueReadsNil` | same | an entity value whose entity was destroyed reads nil in Luau |
| `Test_AIBlackboard_RollbackDiscardsSet` | same | a behaviour that sets a key then raises an error leaves no entry (the command was in the rolled-back sub-buffer) |

## Script and library tests

| Test | File | Asserts |
|---|---|---|
| `Test_AIScript_BindingTableShape` | `Zenith_AIScript_Tests.cpp` | every binding of `Zenith2/Docs/Design/AI/Scripting.md` → "Binding functions" is registered with its effect, contexts and side-effect mask; `Seal` succeeds |
| `Test_AIScript_MisuseCodes` | same | each row of "Misuse codes" raises its code from the AI block row of the catalogue (read from the row, not a literal) with its text |
| `Test_AIScript_RequestPathRoundTrip` | same | a behaviour's `RequestPath` resolves to a `PathResult` whose eight fields decode the 416-byte payload exactly (`limit` from byte 2) (`corners.Points[i]` equal to the payload's corner i for i ≤ `cornerCount`), and `Agent.Corners` returns the same points as an array |
| `Test_AIScript_RequestPathFromSystemRejected` | same | `AI.RequestPath` from a chunk system raises SCRIPT's ZS1702 |
| `Test_AIScript_DecoderRejectsBadPayload` | same | a payload with status 99, limit 4, 33 corners, 415 bytes, or the publication kind `PUBLICATION_KIND_GENERIC_RECORDED` makes the decoder return false |
| `Test_AIScript_QueriesNeedUses` | same | a navigation query from a module without `Zenith.AI.World` in `Uses` raises SCRIPT's resource misuse error |
| `Test_AIScript_RandomPointUsesInvocationStream` | same | re-running a failed invocation draws the same random reachable point; the binding consumes exactly one `NextRandomDraw` per call |
| `Test_AIBehaviourTree_SequenceSelectorSemantics` | `Zenith_AIBehaviourTree_Tests.cpp` (drives the Luau library through a headless script world) | each node kind of the table follows its status rule on scripted child results |
| `Test_AIBehaviourTree_ReactiveAbortCallsAbort` | same | a higher-priority branch succeeding aborts the running action once and resets its subtree's slots |
| `Test_AIBehaviourTree_StateSurvivesRecover` | same | a tree mid-sequence is checkpointed, quarantined and restored through `OnRecover`, resuming at the same child |
| `Test_AIBehaviourTree_BuildLimits` | same | 257 nodes, depth 33 and a 17-child composite each fail the module load with the library's error text |
| `Test_AIBehaviourTree_TraceOnlyWhenTracing` | same | with tracing off `TraceDecision` is never called; with it on, once per status change |
| `Test_AIUtility_ScoringAndMomentum` | `Zenith_AIUtility_Tests.cpp` | the compensated product equals the formula; momentum keeps the current option until another exceeds it by 10%; ties go to option order; a `Logistic(4, 0.5)` curve evaluated by `U.Select` at x = 0.5 is exactly 0.5 (`Zenith_Exp(0) == 1` exactly, row AI22) and at 0, 0.25, 0.75, 1 equals `AI.Logistic(ctx, x, 4, 0.5)` called with the same invocation's `ctx`, bit for bit (the library calls no `math.exp`); `AI.SecondsToTicks(ctx, 0.5)` at 60 Hz is 30 |
| `Test_AIStateMachine_EnterExitOrder` | `Zenith_AIStateMachine_Tests.cpp` | a transition between nested states exits innermost-first to the common ancestor and enters outer-first to the initial leaf |
| `Test_AIStateMachine_TransitionLimit` | same | a fifth transition in one update raises the library's error |
| `Test_AIStateMachine_OnChangedMirrors` | same | `onChanged` receives the from and to paths once per change |
| `Test_AILibraries_StatelessAndStrict` | same | every file under `ZenithAI/Scripts/` is `--!strict` and passes the D-LUAU-STATELESS load check when required by a behaviour and by a chunk system |

## Save and determinism tests

| Test | File | Asserts |
|---|---|---|
| `Test_AISave_SectionRoundTrip` | `Zenith_AISave_Tests.cpp` | capture then restore of every list of the section reproduces the records field for field, including the 112-byte header's rebase count and applied origin, supplier records with every slot state, corridor entries and OFFMESH_START corners in link-node form, tile-local carved shapes and volume-search stamps |
| `Test_AISave_CorridorThroughLinkRestored` | same | an agent mid-corridor before a JUMP link, saved and restored, holds the same corridor (its link entry resolved by (link entity simulation ID, class) to the restored table's link-node reference), follows the same corners and starts the link traversal on the same tick as the uninterrupted run; the canonical AI state hash is equal at every tick for 300 ticks |
| `Test_AISave_RestoreAfterRebaseNoSecondShift` | same | a world that rebased once (count 1, origin (2,048, 0, 0)) runs 50 ticks, saves and restores: the header holds count 1 and the origin, `ai.nav.origin_late_delta` stays 0 at the first and every subsequent tick, and 300 ticks match the uninterrupted run's world and AI hashes; a section whose count differs from the restored `Zenith.WorldOrigin`, or whose applied origin differs with an equal count, fails with `SAVE_ERROR_CORRUPT` |
| `Test_AISave_RestoreKeepsSuppliersWithChangeBitsSet` | same | a streamed world with 20 tile suppliers (one slot FAILED, one IN_FLIGHT whose completion a gate hold keeps across the capture, so its load record is ISSUED), saved and restored with every change bit set: no tile is removed or re-requested at the first tick, the FAILED slot is not requested, the IN_FLIGHT load is re-issued under its ID, and, with the same hold set in both worlds after the restore, 300 ticks match the uninterrupted run |
| `Test_AISave_ActiveVolumeSearchRestored` | same | a flyer's volume search active across a capture (3 stamped volumes) restores with stamps naming the restored slots and salts, its next 3 steps and corners equal the uninterrupted run's, and removing one stamped volume after the restore stales it exactly as live |
| `Test_AISave_CorruptSectionRefused` | same | each reader check of "Save section layout" fails with `SAVE_ERROR_CORRUPT` |
| `Test_AISave_InFlightRequestsReissued` | same | 8 script requests in flight (2 in the inbox, 3 queued, 3 active) and 2 ISSUED navigation loads (held by the completion gate) are re-issued under their recorded completion IDs with `MarkReissued`; none is abandoned; a section listing an ID for which `WasInFlight` is false, or one `MarkReissued` refuses, fails `RestoreSection` with `SAVE_ERROR_CORRUPT` |
| `Test_AISave_ActiveSearchStateRestored` | same | a restored active search's snapshot equals the saved one field for field (nodes in index order, heap order, best node, expansions, stamps with the saved versions) although its tiles occupy other slots, and its next step pops the same node |
| `Test_AISave_SearchStraddlingOverlayChange` | same | a MARK obstacle moves between two slices of an active search; a save after the move, restored, gives the same next 3 slices and corridor as the uninterrupted run (a re-run to the saved expansion count under the new overlay would not, which the test also asserts, so the scenario is discriminating) |
| `Test_AISave_DeadLinkNodeRestored` | same | a capture holding a dead link node in an active search (open, in the heap) and one in an agent corridor deeper than entry 8 writes record kind 4 and entry kind 3 with every identity field zero; the restore rebuilds both as `MakeLinkNode(0, 0)`; the search's next 3 steps and the agent's next 300 ticks (its corridor invalidated when the dead entry reaches the first 8) equal the uninterrupted run's, and the canonical AI state hash is equal at every tick |
| `Test_AISave_QueuedCarveIsExact` | same | a capture with 6 queued rebuilds and `ai.nav.maxTileRebuildsPerTick` 4 reports `SAVE_FIDELITY_EXACT`, restores, drains the queue over 2 ticks in the saved order, and matches the uninterrupted run's world and AI hashes for 300 ticks |
| `Test_AISave_CarvedTwiceRestoresExact` | same | a tile carved twice (version 3) whose obstacle moved again with its re-carve still queued restores with version 3, the rebuilt section's check equal to the saved check, the old obstacle pose in its carved record, no extra dirty obstacle at the first tick although every change bit is set, and equal hashes for 300 ticks |
| `Test_AISave_SynchronousRestoreListsNavLoads` | same | a resume of a save whose tiles are not resident: the first `IsRestoreReady` returns false and `ListRestoreWaits` lists their load operations in ascending GUID; `Zenith_RuntimeSave_RestoreSynchronously` services them and reaches DONE before the first tick, which has every tile joined |
| `Test_AISave_RecookedTileReplans` | same | a tile whose content check changed between save and load is added with its new content, its agents drop their corridors and replan, the capture's own fidelity stays EXACT, and the participant calls `ReportCoherentRestore` once and counts `ai.save.coherent_fallbacks` |
| `Test_AISave_NoSectionReportsCoherent` | same | a save without the AI section restores, calls `ReportCoherentRestore`, and the world joins its tiles through the ordinary path |
| `Test_AISave_RestoreContinuesBitIdentical` | `Zenith2/Tests/AI/Zenith_AISaveScenario_Tests.cpp` | the scenario of `Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm": 300 ticks after restore match the uninterrupted run's world and AI hashes |
| `Test_AIState_HashCanonical` | `Zenith_AIState_Tests.cpp` | `ComputeStateHash` is independent of tile slot assignment, tile add order and agent table insertion history, ignores expired blackboard records not yet pruned, and changes when any agent, corridor, search node, ledger entry, load record, memory or blackboard value changes |
| `Test_AIDeterminism_ReplayMatrix` | `Zenith2/Tests/AI/Zenith_AIDeterminism_Tests.cpp` | the W-Characters-shaped scenario (100 agents, 20 perceivers, 4 carving obstacles, 2 links, 32 script path requests per second) replayed with 1, 2, 8 and all workers, forced compaction and chunk capacities 128 and 1,024, on each ECS backend with Jolt (and PhysX where the preset has it), with `ai.debug.views` 0 and no selected agent (tracing off, `Zenith2/Docs/Design/AI/Scripting.md` → "Debug trace helpers"), gives equal per-tick world and AI hashes per backend pair |
| `Test_AIDeterminism_ScriptFaultMatrix` | same | the same scenario with injected behaviour errors, budget exhaustion and merge rejections in the AI-driving behaviours gives equal hashes across the matrix |
| `Test_AIDeterminism_LiveRunsAgree` | same | two live headless runs (not replays) of the scenario on 2 and 8 workers, each with the completion gate's fixed delay (`m_uGateFixedDelay` 8: every navigation load's completion record released at its issue tick + 8, "Fixtures"), give equal world and AI hashes at every tick from tick 0 for 3,600 ticks and equal join ticks for every tile, with `m_uGateLateReleases` 0 in both (the asserted precondition): the path service, carving, perception and locomotion add no live-run divergence; divergence from real I/O timing is replay's (`Test_NavRuntime_TileJoinReplayedAtRecordedTick`) |
| `Test_AIScenario_ReproduceFromLog` | `Zenith2/Tests/AI/Zenith_AIScenario_Tests.cpp` | a recorded scenario log replays to the recorded final AI hash, and `ai.dump`'s printed hash equals it |

## Failure injection tests

| Test | File | Asserts |
|---|---|---|
| `Test_AIModule_Start_FailsAtConfig_Unwinds` | `Zenith_AIModule_Tests.cpp` | a fault at start step 1 unwinds with nothing registered |
| `Test_AIModule_Start_FailsAtKindLoader_Unwinds` | same | step 2 likewise |
| `Test_AIModule_Start_FailsAtCommandKinds_Unwinds` | same | step 3 likewise |
| `Test_AIModule_Start_FailsAtScript_Unwinds` | same | step 4 likewise |
| `Test_AIModule_Start_FailsAtMemory_Unwinds` | same | step 5 likewise |
| `Test_AIModule_Attach_FailsAtWorldState_Unwinds` | same | attach step 2 fails and leaves no resource, node, listener or participant |
| `Test_AIModule_Attach_FailsAtResources_Unwinds` | same | attach step 3 likewise |
| `Test_AIModule_Attach_FailsAtNodes_Unwinds` | same | attach step 4 likewise |
| `Test_AIModule_Attach_FailsAtListener_Unwinds` | same | attach step 5 likewise |
| `Test_AIModule_Attach_FailsAtParticipant_Unwinds` | same | attach step 6 likewise |
| `Test_AIModule_Start_RejectsBadConfiguration` | same | each malformed configuration value of `Zenith2/Docs/Design/AI/PublicAPI.md` → "Configuration keys" fails start with `MODULE_ERROR_REGISTRATION` |
| `Test_AIModule_Start_RejectsQueueSmallerThanDemand` | same | `ai.path.maxQueuedRequests` = 1,024 with `ai.agent.maxAgents` 1,024 and a script pathfinding capacity of 32 fails start with `MODULE_ERROR_REGISTRATION` naming the three values; 1,056 starts, and a run with every agent planning and 32 script requests in flight never counts `ai.path.queue_full_delta` |
| `Test_AIModule_OutOfMemoryDuringCarve` | same | a heap fault during a carve rebuild keeps the previous polygons and counts `ai.nav.carve_failed_delta` |
| `Test_AIModule_OutOfMemoryDuringSearch` | same | a node-arena fault fails Begin with `AI_ERROR_OUT_OF_MEMORY` and the request completes INVALID_REQUEST |
| `Test_AIModule_MemoryPressureShedsNothingSimulated` | same | at WARNING and CRITICAL the module's simulation bytes are unchanged; only the development trace ring is released at CRITICAL |
| `Fuzz_NavTileSection` | `Zenith2/Engine/ZenithAI/Tests/Fuzz/Zenith_NavFuzz.cpp` | the tile section reader never crashes or reads out of bounds (win-asan, libFuzzer, seeds generated in code) |
| `Fuzz_NavVolumeSection`, `Fuzz_NavLayersSection`, `Fuzz_NavSourceIntermediate`, `Fuzz_NavSaveSection` | same | likewise for each reader |

## Debug and editor tests

| Test | File | Asserts |
|---|---|---|
| `Test_AIDebug_ViewsDrawRecords` | `Zenith_AIDebug_Tests.cpp` | each `Zenith_AIDebugView` bit set alone makes `AI.DebugDraw` record at least one primitive of its view on a fixture world (counted through `Zenith_DebugDraw::GetStats`); with the recorder disabled (`SetEnabled(false)`), or `ai.debug.views` 0, the stage records nothing |
| `Test_AIDebug_DumpSorted` | same | `ai.dump` writes agents, tiles and blackboards sorted by their keys and prints the value of `ComputeStateHash` |
| `Test_AIDebug_CountersPublished` | same | every counter of `Zenith2/Docs/Design/AI.md` → "Counters" is published once per tick with its declared kind through the counter hook |
| `Test_AIDebug_TraceStagedPerVM` | same | `TraceDecision` from a chunk system on 8 workers and a behaviour on the main thread in one phase writes only its VM's staging buffer (the harness checks each buffer's writer thread); the merge at `AI.NavIngest` fills each agent's ring in (simulation ID, tick, VM index, sequence) order whatever the worker count; a 257th agent reuses the ring with the oldest last record |
| `Test_AIEditor_ExtensionRegisters` | `Zenith2/Tools/ZenithEditor/Tests/Navigation/Zenith_AIEditor_Tests.cpp` | the AI editor extension registers its two panel types, its gizmo providers for `Zenith.AI.OffMeshLink`, `.NavObstacle`, `.NavAreaVolume` and volume regions, and its asset-kind handler for kind 15, with no duplicate |
| `Test_AIEditor_RegenerateAfterTerrainStroke` | same | a terrain stroke over two tiles rebakes exactly those tiles in process and the edit world's navmesh shows the new polygons the next frame |
| `Test_AIEditor_DebuggerPanelModel` | same | the debugger panel's model for a selected play-world agent lists its state, corridor length, memory entries, blackboard keys and the last trace records in tick order |

## Integration suite

| Test | File | Asserts |
|---|---|---|
| `Test_AIIntegration_StreamedWorldNavigation` | `Zenith2/Tests/AI/Zenith_AIStreaming_Tests.cpp` | in a streamed fixture world, agents walking across cells keep moving while NAVIGATION cells load ahead; a request into an unloaded region is PARTIAL at its boundary |
| `Test_AIIntegration_CrowdOfHundred` | `Zenith2/Tests/AI/Zenith_AICrowd_Tests.cpp` | 100 agents with goals scripted as W-Characters' (fixed seed) all arrive or report UNREACHABLE within 120 s; no two overlap by more than 10% of R for more than 1 s |
| `Test_AIIntegration_LuauBrainEndToEnd` | `Zenith2/Tests/AI/Zenith_AIBrain_Tests.cpp` | a guard behaviour built with `@ai/BehaviourTree`, perception and blackboards patrols, detects a target, chases it, loses it and returns, with the event sequence expected by its script |
| `Test_AIIntegration_TransitionDiscardsPathRequests` | `Zenith2/Tests/AI/Zenith_AITransition_Tests.cpp` | 32 in-flight path requests at a world transition are never published and `world.async_discarded` counts them (AI's half of GAME's `Test_Scenario_WorldTransitionWithInFlightAsync`) |
| `Test_AIIntegration_AuthorityCensusRows` | same suite links ZenithAI; the check is `Test_AuthorityCensus_OpenWorldModules` (GAME) | AI's census rows of `Zenith2/Docs/Design/AI/PublicAPI.md` → "Authority census rows" match its registered types |

## Navigation conformance suite

`ZenithAI_Conformance` runs scenarios N1 … N14 of `Zenith2/Docs/Design/AI/Navigation.md` → "Conformance scenarios" against the selected implementation, one test each: `Test_NavConformance_N01_FlatPlane`, `Test_NavConformance_N02_LCorridor`, `Test_NavConformance_N03_Doorway`, `Test_NavConformance_N04_Stairs`, `Test_NavConformance_N05_Slope`, `Test_NavConformance_N06_TwoStorey`, `Test_NavConformance_N07_Terrain`, `Test_NavConformance_N08_Streaming`, `Test_NavConformance_N09_Carving`, `Test_NavConformance_N10_OffMeshLink`, `Test_NavConformance_N11_AgentSizes`, `Test_NavConformance_N12_Volume`, `Test_NavConformance_N13_ReloadOrder`, `Test_NavConformance_N14_ReplanBudget`; the harness runs the same functions against each candidate before the ruling.

## Benchmarks and device runs

| Run | Where | Produces |
|---|---|---|
| `Bench_NavigationSelection_Smoke` | `Test` step | the harness runs end to end (N1, N2, N10, one repetition) |
| D-SEL-NAV full measurement | the `needs-human` ruling ticket, reference desktop and S24 | `Zenith2/Docs/Research/Navigation/NavigationSelection.md` with raw data |
| AI budget measurement | the Stage 1 per-system budget ticket for AI (reference-engine and published data, no harness half) | the values of Z2-B-AI-001 … -003 |
| Stage 4 confirmation | W-Characters and W-OpenWorld budget runs (`--budget-run`) | MP-SUBMEM for Z2-B-AI-001, MP-CPUZONE over phase 7 for Z2-B-AI-002, MP-COUNTER `ai.path.active` for Z2-B-AI-003 |
| Stage 6 qualification | the same runs on the S24 in its ordinary state and on the reference desktop | the qualification evidence records |
| S24 unit run | the Stage 4 acceptance device run of `ZenithAI_Tests` through MOD's test APK | pass/fail |

## Fixtures

| Fixture | Location | Content |
|---|---|---|
| geometry builders | `Zenith2/Engine/ZenithAI/Tests/Support/Zenith_NavTestGeometry.h` (header-only) | the analytic shapes of N1 … N12: plane, L corridor, two rooms and a door, stairs, ramps, two-storey building, terrain from `h(x, z) = 20 sin(x / 97) cos(z / 113) + 5 sin(x / 17 + z / 23)` with cliffs where `|∇h| > 1.2`, a gap, a canyon, a pillar |
| reachability and cost oracle | `Zenith2/Engine/ZenithAI/Tests/Support/Zenith_NavTestGeometry.h` (header-only) | `Zenith_NavTestReach Zenith_NavTestReachable(const Zenith_NavMesh&, Zenith_NavPolyRef xStart, const Zenith_Vector3& xStartPoint, const Zenith_NavQueryFilter&)`: a Dijkstra over the polygon graph of `Zenith_NavMeshInternal::GetNeighbours` (`Zenith2/Docs/Design/AI/Internals.md` → "Search tile view": in-tile neighbours and external links, no off-mesh links), with entry positions as "Search graph" and costs as "Cost and heuristic" of `Zenith2/Docs/Design/AI/Navigation.md`, each polygon keeping the entry of its cheapest known parent and reopened when that improves (the search's relaxation rule with h = 0), no node pool, `std::vector`, `std::priority_queue` and `std::unordered_map` (test code). `Zenith_NavTestReach` holds the start's component as a polygon set and, per reached polygon, its least g and parent; `GetCost(xEndPoly, xEnd)` returns C_ref = g + \|end − entry\| × the end polygon's area cost (`Finish`'s rule) and `GetCorridorLength(xEndPoly)` the polygon count of its parent chain. N7 and N14 derive reachability, component sizes and their asserted preconditions (C_ref ≤ 1.25 D, corridor ≤ 256) from it, independently of the search |
| completion gate | AI's own `Zenith_AIWorldState` test members (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AIWorldState header"; algorithm `Zenith2/Docs/Design/AI/Navigation.md` → "Completion gate"), set by the test world on the main thread with no frame running | `m_axGateHolds` (a named completion ID released at a given tick: the tile-join, inactive-region, replay, abandonment and save tests) and `m_uGateFixedDelay` (every navigation completion released at its issue tick + the delay: `Test_AIDeterminism_LiveRunsAgree` with 8); every test that sets either asserts `m_uGateLateReleases` = 0, its precondition that each completion reached the gate before its release tick. No ASSET control and no double of a ZenithAssets class is involved (FINAL-DOUBLES) |
| explicit tile writer and the two hand fixtures | `Zenith2/Engine/ZenithAI/Tests/Support/Zenith_NavTestTileWriter.h` (header-only) | `Zenith_NavTestWriteTileFile(const Zenith_NavTestTileDesc&, std::vector<uint8>&)` writes a TILE-role `.znav` (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation file", "Tile section") from explicit convex polygons given in tile-local metres on the cell grid: neighbours from shared edges (exact vertex equality), portal sides from edges on the tile boundary, flat detail (no detail vertices), the BV tree of bake stage 10; the result passes T1 … T9 and loads through the kind loader like a baked tile. `BuildRelaxationFixture` (one or more 16 m tiles whose polygons give the events `Test_NavPath_RelaxationRules` names: an open replacement through a 6 m then a 1 m portal, a reopening, an equal-g relaxation and one JUMP link) and `BuildOverflowFixture` (four 16 m tiles in a 2 × 2 grid whose portal edges give each tile 7 to 9 candidates) are written with it, their coordinates chosen by the test author and listed in the test with the hand derivation; `Zenith_NavTestOverflowOracle(Zenith_Span<const Zenith_NavTestTileDesc>, uint32 uCapacity)` computes the existing candidate set straight from the definition of "Tile connection procedure" (every candidate of every loaded pair, ranked in both tiles) for the comparison |
| test worlds | `Zenith2/Engine/ZenithAI/Tests/Support/Zenith_AITestWorld.h` (header-only) | a headless world with the Physics (Jolt) and AI modules through MOD's runtime test access, static colliders from the geometry builders, tiles built in memory with `Zenith_NavBuildTile` and loaded through the kind loader; test controls over AI internals the tests include (`ZenithAI/Internal/`, allowed to its own tests): the tile table's per-tile link capacity override (`Zenith_AIWorldState` test member `m_uLinkCapacityOverride`, 0 = 4 × polygons), a suppression mask of AI's lifecycle listener (`m_uListenerSuppressMask`, origin-shift bit only) and the completion gate's members (above) |
| cook fixture game | generated at test time under `${ZENITH_OUT}/test-fixtures/AI/CookGame/` by the cook tests' `FIXTURES_SETUP` | a settings document, a navigation document, two scenes with static colliders, area volumes and a link, and a terrain document |
| scenario logs | generated at test time under `${ZENITH_OUT}/test-fixtures/AI/Scenarios/` by recording the scenario once in the same run | replay logs for `Test_AIScenario_ReproduceFromLog` |
| save fixtures | generated at test time from the section writer | `Test_AISave_*` |
| fuzz seeds | built in code by each fuzz target | seeds |
