# WORLD design: reproducible procedural generation

Section file of `Zenith2/Docs/Design/WORLD.md`. It specifies WORLD's generator framework: what a generator is, the static registry and its versions, the two execution modes (computed at every cook, or baked into generated region documents), the seed derivation from the world seed, the identity of generated entities, the four built-in generators, how outputs read earlier generators and authored content as obstacles, staleness of baked outputs, and the determinism argument and golden-output policy. ENV's placement algorithm `Zenith_Place` is ENV's (`Zenith2/Docs/Design/ENV/Procedural.md` → "Zenith_ProceduralPlacement header"); WORLD owns the orchestration ENV's boundary table assigns it (`Zenith2/Docs/Design/ENV/Procedural.md` → "Boundary with WORLD procedural generation"): which rule sets apply to which partitions and layers, when generation re-runs, how results are baked and locked for concurrent editing, and whole-world reproducibility from the world seed.

## Scope of this file

| Need | Heading |
|---|---|
| terms | "Generator terms" |
| the interface and registry | "Generator interface and registry" |
| cook and baked modes | "Execution modes" |
| seeds | "Seed derivation" |
| entity identities | "Output identity" |
| what a run does | "Running a generator" |
| the built-in generators | "Built-in generators" |
| obstacles and dependencies | "Obstacles and dependencies" |
| baked outputs going stale | "Generator outputs and staleness" |
| why outputs are reproducible | "Generator determinism" |
| goldens | "Golden outputs" |

## Generator terms

| Term | Meaning |
|---|---|
| generator | a registered algorithm with an ID (`world.scatter`) and one or more implemented versions; compiled into ZenithAuthoring, never loaded dynamically (Z2-D-024) |
| generator document | one configured use of a generator in a world (`Zenith2/Docs/Design/WORLD/Documents.md` → "Generator document"): version, mode, target layer, scope, salt, parameters |
| output | one hierarchy a generator produces: a prefab instance or a plain entity, with its output key, its owner point and its local transform relative to the anchor of the region that owns it |
| output key | a text that is a pure function of what the output is (a lattice index, a spline sample index, a placement instance key), never of how many outputs came before it or of which region computed it |
| output ordinal | the `uint64` a built-in derives from its output key to index its random draws ("Seed derivation") |
| owner point | the world XZ point that decides which region owns an output: the output's position before any random displacement (a lattice point, a spline sample's arc-length position); exactly one region contains it, so every output is emitted exactly once |
| region job | one generator document over one region of its plan: computes every output the region owns; the unit of baking and of parallel scheduling, never of seeding or identity |
| obstacle | a disc an output must avoid: the root of an authored hierarchy of a BAKED layer anywhere in the world, or an output of a generator it depends on, wherever it lies ("Obstacles and dependencies") |

## Generator interface and registry

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringWorld.h (excerpt; the full declarations are PublicAPI.md "Generator types")
class Zenith_WorldGenerator
{
public:
	virtual ~Zenith_WorldGenerator() = default;
	virtual const Zenith_WorldGeneratorDesc& GetDesc() const = 0;
	// Validates the parameter object for uVersion; every finding is a WORLD_ERROR_GENERATOR_INVALID diagnostic with a JSON pointer.
	virtual Zenith_Result<void, Zenith_WorldError> ValidateParameters(uint32 uVersion,
		const Zenith_JsonDocument& xDocument, Zenith_JsonNode xParameters, Zenith_WorldDiagnostics& xDiagnostics) const = 0;
	// The region coordinates this document's outputs can be owned by, beyond the authored regions of "all" scope ("Running a generator" step 2).
	virtual Zenith_Result<void, Zenith_WorldError> ListParameterRegions(uint32 uVersion, const Zenith_WorldDesc& xWorld,
		const Zenith_JsonDocument& xDocument, Zenith_JsonNode xParameters, Zenith_Arena& xArena,
		Zenith_Span<const Zenith_WorldRegionCoord>& axOut) const = 0;
	// Produces every output the context's region owns. Pure and single-threaded: reads only xContext; writes only into xContext's output.
	virtual Zenith_Result<void, Zenith_WorldError> Generate(uint32 uVersion, Zenith_WorldGeneratorContext& xContext) const = 0;
};
```

- The registry is the constant table `Zenith_GetWorldGenerators()` (a `Zenith_Span<const Zenith_WorldGenerator* const>` over function-local `constexpr`-constructed instances, ascending ID), compiled into ZenithAuthoring; a game adds no generator (games contain no C++, Z2-D-002, and modding is excluded, Z2-D-075). `Zenith_FindWorldGenerator(strID)` is a binary search.
- `Zenith_WorldGeneratorDesc { const char* const m_szID = nullptr; const uint32 m_uOldestVersion = 0; const uint32 m_uLatestVersion = 0; const char* const m_szDescription = nullptr; }`. Every version from `m_uOldestVersion` to `m_uLatestVersion` stays implemented: a version's algorithm is frozen once it ships, and a changed algorithm is a new version. A generator document pins its version; moving it to a newer version is an authored edit the editor previews as an output diff before it is applied ("Upgrading a generator version").
- Retiring a version follows the source-API deprecation rule (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind"): one fix version of deprecation (W31 becomes a warning naming the replacement), then the version is removed together with a migration step of `Zenith.World.GeneratorDocument` that moves every document still pinned to it onto the oldest remaining version, which EDIT's migration flow previews.

### Upgrading a generator version

The command `world.generator.upgrade(document, version)` runs the document's region jobs at both versions in memory (a background computation, "Bake panel and commands" in `Zenith2/Docs/Design/WORLD/Editing.md`), shows per region the added, removed and moved outputs (moved: same output key, position differing by more than 1 mm or yaw by more than 0.01°), and on confirmation writes the new `generatorVersion` (and, in baked mode, the regenerated outputs) in one record. Upgrading never happens implicitly: a tool that implements version 3 keeps generating version 2 for a document pinned to 2.

## Execution modes

| Mode | When the output is computed | Where it lives | Use |
|---|---|---|---|
| `cook` | in every partition import (`Zenith2/Docs/Design/WORLD/Partition.md` → "Partition algorithm" step 4), and in the editor's preview | nowhere on disk: only in the cooked cells and navigation input scenes | dressing that should always follow its inputs (fences along an authored path, scatter that follows terrain edits) |
| `baked` | only when an author runs `world.generate` | generated region documents `Generated/<GeneratorKey>/<x>_<z>.scene.json`, listed in the world document with `generator` set, plus the `outputs` record of the generator document | content that must not move when an input or a hash library changes, content that designers inspect in the editor, and content that must ship unchanged across a generator upgrade |

A baked output is a SCENE document: the editor shows and selects it, and an author may detach it ("Detaching an output") but never edit it in place without the editor marking it EDITED.

### Detaching an output

`world.generate.detach(generator, region)` moves the generated region's hierarchies into the authored region document of the generator's layer at the same coordinate (creating it when absent) in one history record, removes the generated region object and its `outputs` record, and leaves the entity IDs unchanged, so saves and references keep working; the content is then authored and the generator no longer owns it. Regeneration of that region is refused until the author removes the detached entities or changes the scope.

## Seed derivation

Every random value of a generator document comes from `Zenith_RandomSource` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource") with:

- seed `ulSeed` = the world document's `seed`;
- stream `ulStream` = `Zenith_Hash64U64(ulSalt, Zenith_Hash64(generatorDocumentID bytes, ulWORLD_GENERATOR_SALT))`, one stream per generator document, where `ulSalt` is the document's `seedSalt`, the ID bytes its 16-byte cooked binary form and `ulWORLD_GENERATOR_SALT = 0x576F726C6447656E` ("WorldGen");
- the counter set explicitly to `outputOrdinal × 16 + k` before draw k (k = 0 … 15), as ENV's placement keys its draws (`Zenith2/Docs/Design/ENV/Procedural.md` → "Seeded streams"). The output ordinal is a function of the output key alone: `world.lattice` and `world.probeGrid` `j × 65,536 + i` (the key's value), `world.spline` the sample index s. The draws of an output therefore depend on nothing but the world seed, the document and the output itself: every region job that evaluates output o draws the same values for it, whatever region it runs for, how the world is divided into regions and which regions are generated.

A generator that calls ENV's `Zenith_Place` passes `Zenith_Hash64U64(ulStream, ulWORLD_PLACEMENT_SALT)` as `Zenith_PlacementDesc::m_ulSeed` (`ulWORLD_PLACEMENT_SALT = 0x576F726C64506C63`, "WorldPlc"), so two generator documents over one terrain never share a placement stream, and ENV's per-cell keying (`Zenith2/Docs/Design/ENV/Procedural.md` → "Cross-cell determinism") makes each placement cell's result independent of the region job that requested it. The draws depend on `Zenith_Hash64`, whose algorithm is D-SEL-HASH (open, Stage 1): outputs are reproducible for a fixed build and hash library, and a ruling that changes the hash moves every cook-mode output (a cooked-data change, re-cooked) while baked outputs stay where they were baked.

## Output identity

An output hierarchy's root is either a prefab instance (ID `Zenith_WorldGeneratedID(generatorDocumentID, outputKey)`) or a plain entity with that ID; the entities a prefab instance creates derive from the instance ID as SCENE derives them (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"). `Zenith_WorldGeneratedID(G, o)` = `Zenith_DeriveAuthoringID(G, Zenith_WorldKeyID(o))` with `o` the generator's output key: a decimal `uint32` for `world.lattice`, `world.spline` and `world.probeGrid`, and the text `<cx>_<cz>/<k>` for `world.scatter` ("world.scatter"); every built-in's output key is unique within its generator document, so the region key is not part of the identity. Consequently:

- re-running a generator with unchanged inputs reproduces every ID, so persistent entities, saved deltas and cross-cell references to generated content survive regeneration;
- an input change that removes an output removes exactly its ID; one that adds an output adds a new ID; outputs that remain keep theirs, including an output whose position moves across a region border (it moves from one generated region document to another with its ID, `Test_WorldGenerator_InputChangeKeepsSurvivingIDs`);
- IDs are SHA-256-derived and depend on no open decision.

## Running a generator

`Zenith_PrepareWorldGenerator(const Zenith_WorldGeneratorRun&, Zenith_Arena&, Zenith_WorldDiagnostics&)` and `Zenith_RunWorldGeneratorRegion(const Zenith_WorldGeneratorPlan&, uint32 uRegion, Zenith_Arena&, Zenith_WorldGeneratorRegionResult&)` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Generator types"), for one generator document D:

1. **Prepare: validate.** Look up D's generator and version (W31); validate D's parameters (W32); open the input documents the parameters name (rule sets, terrain documents) through the source access; load a terrain a `surface` names with `Zenith_WorldLoadTerrain` (`Zenith2/Docs/Design/WORLD/Partition.md` → "Terrain loading"), so every tile it reads is a recorded input and the surface is ENV's placement surface over that document.
2. **Prepare: region set.** D's region set is the coordinates whose rectangle intersects D's scope, in (z, x) order; scope `"all"` means every coordinate holding an authored region of any BAKED layer, together with every coordinate `ListParameterRegions` returns for D: `world.lattice` and `world.probeGrid` every region intersecting the parameter `bounds`; `world.spline` every region intersecting the axis-aligned box of some segment of `points` grown on X and Z by `|lateralOffset| + jitter` (a conservative cover: a region of the set that owns no sample produces nothing); `world.scatter` none beyond the authored ones. At most `uWORLD_MAX_REGIONS` coordinates.
3. **Prepare: obstacles.** Build the world's obstacle array once ("Obstacles and dependencies"): every authored obstacle and every dependency output, world-absolute, sorted by (x, z, ID). Each region job's **obstacle set** is the elements whose disc centre lies within `G + r` of its region rectangle (the distance from the centre to the nearest point of the half-open rectangle, computed in `double`; r the element's radius), found by binary search on x over `[min.x − G − 64, max.x + G + 64)` and then filtered exactly; G is the built-in's **reach** of "Obstacles and dependencies". The plan (`Zenith_WorldGeneratorPlan`) is immutable from here.
4. **Run each region job.** `Zenith_RunWorldGeneratorRegion` builds a `Zenith_WorldGeneratorContext` (region coordinate and anchor, the random source of "Seed derivation", D's parameters, the region's obstacles, the opened input documents, the output sink with capacity `uWORLD_MAX_GENERATED_PER_REGION`) and calls `Generate`, which computes every output whose **owner point** lies in the region's half-open rectangle and emits it with that owner point. A region job touches nothing but its own context and result, so the caller decides how jobs are scheduled: the importer runs them inline in plan order; the editor submits each as one tool-priority background job of its job system ("World editor extension" in `Zenith2/Docs/Design/WORLD/Editing.md`); tests drive them through `Zenith_JobSystem::RunRoot` with a `ParallelFor` over the region indices. No region job forks, joins or waits.
5. **Check.** Each region job sorts its outputs by output key and checks that every owner point lies inside its region (a generator that emits an output it does not own is a generator defect: `WORLD_ERROR_GENERATOR_FAILED`) and that the keys are unique (a duplicate is `WORLD_ERROR_GENERATOR_FAILED`). An output's position may lie outside its region by its random displacement (at most the parameter `jitter` per axis); it stays with its owning region, which is why R3 allows that margin for generated region documents (`Zenith2/Docs/Design/WORLD/Documents.md` → "Region documents"). A region job may own no output (a spline's conservative cover, a lattice region its bounds miss); its result has an empty output span and its fingerprint, and in baked mode it writes no generated document and no region object ("Generator outputs and staleness"). `Zenith_RunWorldGenerator` is the convenience form: prepare, then every region job inline, returning the outputs per region in plan order.

Exceeding the output capacity is `WORLD_ERROR_GENERATOR_LIMIT` naming the region: the generator keeps the first outputs in key order and the cook fails, so content is fixed rather than silently truncated (ENV's rule for placement capacity).

<!-- informative -->
The old engine's terrain tools made one lesson concrete: Zenith's Flatten brush moved a texel 35% of the way to its target per dab, so a two-dab "pad" never levelled anything, while SetHeight assigned the target outright (`Zenith/Editor/TerrainEditor/Zenith_TerrainEditor_Brushes.cpp`). A procedural step that is a *rate* gives a result that depends on how many times it ran; every WORLD generator is an *assignment*: its output is a pure function of its inputs, running it twice gives the same output as running it once (`Test_WorldGenerator_RunTwiceIdentical`, `Test_WorldGenerator_BakedRerunIgnoresOwnOutput`), and regeneration replaces the previous output instead of accumulating on it. The two mechanisms that would reintroduce a rate are excluded by construction: an output's draws are keyed by the output, not by the region computing it, and a baked generator never reads its own previous output as an obstacle.
<!-- /informative -->

## Built-in generators

Every built-in is version 1 at Stage 4. Parameter members not listed in a built-in's table are refused (W32); units are metres and degrees. Every built-in accepts `obstacleRadius` (number metres, 0 … 64, default 0.5): the radius of the disc an authored or dependency obstacle presents to it ("Obstacles and dependencies"); the table rows below omit it.

### world.lattice

A regular grid of prefab instances, the generator whose output is computable by hand (it draws no random value when `jitter` is 0).

| Parameter | Type | Range | Meaning |
|---|---|---|---|
| `prefabs` | ordered array of prefab GUIDs | 1 … 64 | cycled by lattice index |
| `bounds` | `{ "min": [x, z], "max": [x, z] }` world metres | min < max | the lattice's extent |
| `spacing` | `[sx, sz]` | each 0.25 … 1,024 | lattice pitch |
| `height` | number | any | world Y of every output |
| `jitter` | number | 0 … min(sx, sz) / 2 | random XZ offset amplitude |
| `yawDegrees` | number | −360 … 360 | yaw of every output |

Algorithm per region job: lattice point (i, j) is at `(min.x + (i + 0.5)·sx, min.z + (j + 0.5)·sz)` for `i ∈ [0, ceil((max.x − min.x)/sx))`, `j` likewise, computed in `double`; it is the output's owner point, so the region keeps the points that lie in its rectangle; output key and ordinal `j × 65,536 + i` (both below 65,536, else W32); jitter draws k = 0, 1 give `(2u − 1) × jitter` on X, then Z; prefab `prefabs[(i + j) mod count]`; rotation `Zenith_MakeAxisAngle((0, 1, 0), yawDegrees × π / 180)` (CORE's constructor, `Zenith2/Docs/Design/CORE/Maths.md`, which evaluates its half-angle sine and cosine with CORE's deterministic functions); scale 1. A point whose jittered position an obstacle's disc contains (distance below the disc radius) is skipped.

### world.spline

Instances along a polyline.

| Parameter | Type | Range | Meaning |
|---|---|---|---|
| `prefab` | prefab GUID | — | |
| `points` | ordered array of `[x, y, z]` world metres | 2 … 1,024 | the polyline |
| `spacing` | number | 0.25 … 1,000 | arc-length step |
| `lateralOffset` | number | −100 … 100 | offset to the right of the path direction |
| `jitter` | number | 0 … spacing / 2 | random along-path offset amplitude |
| `yawJitterDegrees` | number | 0 … 180 | random yaw amplitude |
| `alignToPath` | boolean | — | yaw follows the segment direction |

Algorithm per region job: the samples s = 0 … floor(L / spacing) (L the total length, accumulated in `double` segment by segment) are evaluated in ascending s; sample s's **owner point** is the point at arc length `s × spacing` by linear interpolation on the segment holding it (segment k covers the half-open arc-length range `[start_k, end_k)`, except the last segment, which also holds its end, so a sample exactly at an interior vertex belongs to the segment that starts there: in the example of "Generator document example" the sample s = 32 at vertex (90, 0, 10) lies on segment 1 and takes its direction, yaw 0°, and its right vector), plus `lateralOffset` along that segment's right vector (+Y × direction); the region job keeps exactly the samples whose owner point lies in its rectangle, so every sample is emitted by exactly one region (`Test_WorldGenerator_SplineBorderSampleEmittedOnce`). Output key and ordinal s. Its position is the owner point plus `(2u − 1) × jitter` along the segment direction (draw k = 0); its yaw is `Zenith_Atan2(dir.x, dir.z)` in degrees when aligned, else 0, plus `(2u − 1) × yawJitterDegrees` (draw k = 1), its rotation `Zenith_MakeAxisAngle((0, 1, 0), yaw × π / 180)`. Obstacles skip samples as for the lattice.

### world.scatter

Prefab instances placed by ENV's algorithm.

| Parameter | Type | Meaning |
|---|---|---|
| `ruleSets` | ordered array of placement document paths (`*.placement.json`, ENV) | 1 … 16 rule sets, in order (`Zenith2/Docs/Design/ENV/Procedural.md` → "Placement rule document") |
| `types` | ordered array of `{ "type": GUID, "prefab": GUID, "footprintRadius": n, "scaleRange": [a, b], "alignToNormal": n }` | every `foliageType` the rule sets name maps to one prefab; the other members become `Zenith_PlacementFoliageTypeInfo` |
| `surface` | `{ "kind": "terrain", "terrain": GUID }` or `{ "kind": "plane", "height": n }` | the terrain document's placement surface (ENV's adapter over the document), or WORLD's `Zenith_WorldPlanePlacementSurface` at that height (no layers, no density, no masks: `GetMissingInputs` returns all three bits) |
| `exclusionsFromLayers` | array of layer `id`s (ascending; each a layer of the world, W32) | layers whose `Zenith.Environment.PlacementExclusion` entities become `Zenith_PlacementExclusion` shapes; a layer rename changes nothing here |

Algorithm per region job: the placement cells are the 128 m ENV cells (`Zenith2/Docs/Design/ENV/Procedural.md` → "Placement model") whose minimum corner lies in the region (the region size is a multiple of 128 m, W4), with the grid origin at the world origin; the owner point of an instance is its placement cell's minimum corner, so every placement cell, and every instance in it, belongs to exactly one region. One `Zenith_Place` call per region job with those cells, `pxJobs` null (the job is single-threaded), the seed of "Seed derivation", the job's obstacle set as `Zenith_PlacementObstacle` discs (world metres narrowed to `float` once) and the job's exclusion set as `m_axExclusions` (`GetExclusions`, "Obstacles and dependencies"); each placed instance becomes a prefab instance of its type's prefab with its position, rotation and scale; the output key is the text `<cx>_<cz>/<k>`, with (cx, cz) the placement cell and k the decimal `Zenith_PlacementInstanceKey` of the instance, unique within its cell (`Zenith2/Docs/Design/ENV/Procedural.md` → "Output ordering and capacity"), so the key is unique within the generator document; `world.scatter` is the one generator whose output keys are not a single integer, and its outputs sort by (cz, cx, k). ENV's cross-cell determinism makes each cell's result independent of the other cells requested (`Zenith2/Docs/Design/ENV/Procedural.md` → "Cross-cell determinism"), so splitting the world into region jobs changes no output.

### world.probeGrid

The probe-placement half of the lighting workflow (LIGHT assigns probe placement to WORLD, `Zenith2/Docs/Design/LIGHT.md` → "Purpose and scope"): reflection-probe entities on a grid above a surface.

| Parameter | Type | Range | Meaning |
|---|---|---|---|
| `bounds` | as `world.lattice` | min < max | the grid's extent |
| `spacing` | number | 8 … 512 | horizontal pitch |
| `heightAboveSurface` | number | 0.5 … 100 | capture centre above the surface |
| `surface` | as `world.scatter` | — | height sampling |
| `influenceHalfExtent` | `[x, y, z]` | each 1 … 512 | the probe's influence box |
| `captureFarMetres` | number | 10 … 5,000 | capture far plane |
| `interior` | boolean | — | the LIGHT interior flag |

Output per lattice point the region owns (as `world.lattice` with `spacing` on both axes and no jitter, key and ordinal `j × 65,536 + i`): one plain entity named `Probe <i>_<j>` with `Zenith.LocalTransform` at the surface height plus `heightAboveSurface` and `Zenith.Lighting.ReflectionProbe` (`Zenith2/Docs/Design/LIGHT.md` → "Light component schemas"): property 1 `CaptureMode` the baked mode, 3 `InfluenceHalfExtent`, 8 `Interior`, 11 `CaptureFarMetres`, the others at their defaults; property 2 `BakedAsset` is written by the partition from the lighting bake.

## Obstacles and dependencies

- **Authored obstacles**: the root of every hierarchy of every BAKED layer, in every authored region document of the world, whose resolved components carry `Zenith.Render.MeshInstance` or `Zenith.Physics.Collider`, as a disc at the root's world XZ position with radius the job's `obstacleRadius`; hierarchies of layers listed in `exclusionsFromLayers` contribute shapes instead. Generated region documents (every region object whose `generator` is non-null) are never authored obstacles, of any generator: a baked generator's own previous output, or another generator's, is not an input of a run, so a rerun cannot avoid what it placed before (`Test_WorldGenerator_BakedRerunIgnoresOwnOutput`) and no generator depends on another outside the `dependsOn` DAG.
- **Generated obstacles**: the outputs of every generator in `dependsOn`, over the dependency's whole region set, as discs of the same radius at each output's position. A cook-mode dependency contributes the outputs its run computes over its whole region set in the same import (or the same editor plan operation); a baked dependency contributes the outputs recorded in all of its generated region documents. A baked generator depends on baked generators only (W36), so every obstacle a baked generator reads is document data, and a baked generator generated for a subset of its regions still reads every obstacle of the world.
- **Reach.** An output is skipped when an obstacle's disc contains the position it would take, which lies up to its random displacement from its owner point; ENV's placement looks further, across its kept rings. A job's reach G bounds how far from its region an obstacle can still decide one of its outputs:

| Built-in | G | Why |
|---|---|---|
| `world.lattice` | `√2 × jitter` | X and Z are jittered independently by up to `jitter` each |
| `world.spline` | `jitter` | the displacement is along the segment direction only |
| `world.probeGrid` | 0 | no displacement |
| `world.scatter` | `9 × 128 m` = 1,152 m | a Euclidean chain bound, not a count of Chebyshev rings: a region's output depends on candidates whose decisions chain through earlier stages' kept instances, each cross-rule hop reaching at most `exclusionRadius + F` ≤ 64 + 64 = 128 m, with at most 8 excluding stages, plus one same-rule spacing hop of `minDistance` ≤ 64 m, so a candidate that can decide an output lies within 8 × 128 + 64 = 1,088 m of the region; an obstacle removes such a candidate within `exclusionRadius` (≤ 64 m) plus the obstacle's radius, which the set's `+ r` term carries, so 1,088 + 64 = 1,152 m (`Zenith2/Docs/Design/ENV/Procedural.md` → "Cross-cell determinism", "Cross-rule exclusion", "Same-rule spacing") |

  The same table bounds the **displacement** of an output from its owner point (lattice `√2 × jitter`, spline `jitter`, probe grid 0; scatter instances lie inside their placement cell, inside the region), which the bake planner's generator-to-lighting edge uses (`Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake planner"). A region job's obstacle set ("Running a generator" step 3) is every obstacle whose disc centre lies within `G + r` of the region rectangle. Every obstacle that can contain a position an output of the region takes is in the set, and the skip test is an exact distance comparison, so which obstacles an output sees is a function of positions alone, never of the region size or of which region owns a dependency output (`Test_WorldGenerator_RegionSizeInvariant`). Taking extra obstacles is harmless: one farther than its reach decides nothing.
- **Exclusion shapes** (`world.scatter` only): the run's `m_axExclusions` holds every `Zenith.Environment.PlacementExclusion` entity of the world's authored region documents, and prepare keeps, as one world-absolute exclusion array, those of hierarchies of a layer listed in `exclusionsFromLayers`, converted to a `Zenith_PlacementExclusion` box, disc or polyline with its entity's world transform exactly as ENV converts an exclusion source's component (`Zenith2/Docs/Design/ENV/Procedural.md` → "Authored exclusion shapes"), in `double` and narrowed to ENV's `float` once, each with its source entity ID (`Zenith_WorldExclusion`), sorted by (shape kind, entity ID). A job's **exclusion set** is the shapes whose XZ bounds (a disc's centre ± radius; a box's centre ± `sqrt(hx² + hz²)`; a polyline's points' bounding box grown by half its width) lie within the scatter's G of its region rectangle, in the array's order, read through `Zenith_WorldGeneratorContext::GetExclusions` and passed to `Zenith_Place` as `m_axExclusions`. A moved road exclusion therefore reaches every baked region whose output it can change, through the fingerprint (`Test_WorldGenerator_ExclusionMoveStalesBakedScatter`).
- **Surface reach.** A built-in that samples a terrain surface reads it up to its surface reach S from its region: `world.scatter` S = 9 × 128 m = 1,152 m (the candidate chain bound of its G row, 1,088 m, rounded up to whole 128 m tiles, because survivors' height, slope, density and mask samples decide the chain), `world.probeGrid` S = 0 (each owner point is its sample point), `world.lattice` and `world.spline` none (they read no surface). The tiles a job hashes, and the importer records, are those intersecting the region rectangle grown by S + 128 m on each side (the 128 m covering a bilinear sample's neighbour tile).
- Obstacles are in world-absolute metres (`Zenith_WorldObstacle`, `double` XZ), sorted by (x, z, ID) before a job reads them, so the job's result does not depend on gathering order; a generator subtracts its region anchor in `double` where it compares an obstacle with an anchor-relative position.

## Generator outputs and staleness

A baked region job's **fingerprint** is the SHA-256 (`Zenith_WorldFingerprintBuilder`, `Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Fingerprint builder") over, in this order: the ASCII tag `"ZWGEN1"` (`AddText`); the generator ID (`AddText`) and version (`AddU32`); the generator document's canonical bytes with its `outputs` and `name` members removed (`AddBytes`; the display name is not an input, so a rename changes no fingerprint); the world seed (`AddU64`); the region coordinate (`AddU32` of x then z, as `uint32`); `AddU32(count)` and the SHA-256 of every input document the job opened (rule sets, terrain document, prefabs named by parameters), in ascending path order (`AddText` of the path, `AddDigest`); `AddU32(0)` when no `surface` names a terrain, else `AddU32(1)`, `AddU32(count)` and the SHA-256 of every tile document of that terrain whose 128 m tile intersects the job's region rectangle grown by its surface reach S plus 128 m on each side ("Obstacles and dependencies"), in ascending tile path order (`AddText` of the path relative to the terrain document, `AddDigest`), so a sculpt or paint stroke that a regeneration could answer differently makes the region STALE (`Test_WorldGenerator_FingerprintCoversTerrainTiles`); `AddU32(count)` and the job's obstacle set ("Running a generator" step 3; sorted, each as three `AddF64Bits` — x, z, radius widened from `float` — and `AddID`); `AddU32(count)` and the job's exclusion set (each as `AddU32` of its shape kind, `AddID` of its entity, `AddF64Bits` of centre X, centre Z, half extents X and Z, yaw, radius and width widened from `float`, then `AddU32(count)` of its points and two `AddF64Bits` per point); `uWORLD_GENERATOR_FRAMEWORK_VERSION` (1, `AddU32`). Every list is preceded by its count and the surface block by its presence flag, so the sequence is prefix-free. Every value entering it is document data: authored positions, parameter values, digests, and baked dependencies' recorded positions (W36 keeps cook-mode outputs, which are `Zenith_Hash64`-dependent, out of every baked fingerprint). The fingerprint therefore contains no `Zenith_Hash64` value, directly or through a position derived from one, and is stable across the D-SEL-HASH ruling, while a cook-mode job's output is not: a ruling that changes the hash leaves baked outputs FRESH and their content unchanged, which is the point of baking.

| State of a baked region job | Condition | Shown and reported as |
|---|---|---|
| FRESH | its `outputs` record exists, the fingerprint matches, and the generated document is not EDITED (the paragraph after this table); a record with `scene` null (the job produced no output) is FRESH while its fingerprint matches and no generated document exists | — |
| STALE | the fingerprint differs | W34; the bake panel lists it |
| MISSING | no record, or a record with a non-null `scene` whose generated document is absent | W35 |
| EDITED | the document's content differs from what the record describes (below) | W24; `world.generate` refuses the region unless its argument `overwriteEdited` is true |
| ORPHANED | a record or generated document for a region outside the current scope | the bake panel offers to delete it |

**Edited detection.** When the document's SCENE `schemaVersion` equals the record's `sceneSchemaVersion`, the document is EDITED exactly when the SHA-256 of its bytes differs from `digest`. When it differs (a SCENE migration, or CONTENT's merge driver re-canonicalising the file at a newer version, rewrote it), a byte digest says nothing about hand edits, so the reader instead regenerates the region job in memory from the current inputs, writes it with `Zenith_WriteWorldGeneratedDocument` at the document's own schema version, and compares the canonical bytes of the two: equal is FRESH (and `world.generate` refreshes the record's `digest` and `sceneSchemaVersion` without rewriting the document), different is EDITED. A fingerprint mismatch is STALE first, so the comparison runs only for jobs whose inputs are unchanged.

`world.generate(generator, regions, overwriteEdited, force)` selects the STALE and MISSING regions of `regions` (the whole scope when `regions` is empty), or every region of the selection when `force` is true, skipping EDITED ones unless `overwriteEdited`; it computes their region jobs in the background and then applies, as one history record, each generated document, its region object in the world document and the generator document's `outputs` record; a region job with no output writes only its record (`"scene": null, "digest": null, "entities": 0` and the fingerprint) and deletes the region's previous generated document and region object when one exists, so the conservative spline cover creates no empty tracked file ("Bake panel and commands" in `Zenith2/Docs/Design/WORLD/Editing.md`).

## Generator determinism

For a fixed build and hash library, a generator's output is identical for any worker count, any region processing order, any subset of regions generated and any region size, because: every draw is counter-based and keyed by (world seed, generator document, salt, output ordinal, k); each output is owned by exactly one region through its owner point; the obstacles are a sorted pure function of the documents; each job writes only its own result and sorts its output by output key; output IDs are derived from output keys alone; ENV's `Zenith_Place` is called single-threaded and carries its own cross-cell determinism (`Zenith2/Docs/Design/ENV/Procedural.md` → "Determinism argument"); floating-point work runs in the fixed environment ([Z2-N-CONTRACT-360]) and every transcendental (the spline's yaw, ENV's placement rotations from its random yaw, CORE's random unit vectors) is CORE's deterministic implementation, never a CRT function (ruling X314-FMA3; ENV's half is row WD33), so the bytes are identical on every machine that runs the build. `Test_WorldGenerator_WorkerCountInvariant` runs every built-in over a fixture world through `RunRoot` with 1, 2, 8 and all workers and inline, with shuffled region orders, and compares outputs; `Test_WorldGenerator_RegionSubsetMatchesFullRun` generates one region alone and compares it with that region of a full run; `Test_WorldGenerator_RegionSizeInvariant` generates one document over worlds of region size 128, 256 and 512, with a lattice `jitter` of 200 m and an authored obstacle 150 m outside a region border, and compares the union of outputs by key. Equality across platforms beyond the Windows tools is not promised ([Z2-N-ARCH-882]); generation runs in the Windows tools.

CORE declares the deterministic transcendentals in `Zenith2/Engine/ZenithCore/Zenith_Transcendental.h` (`Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions"; row WD29 answered): WORLD calls `Zenith_Atan2(double, double)`, `Zenith_Sin(double)` and `Zenith_Cos(double)`, and `Zenith_RandomSource::UnitVector` uses `Zenith_SinCos(double)`. The generator tickets are blocked by CORE's Z2T-CORE-100.

## Golden outputs

Under the GOLDEN-FILES ruling, outputs derivable from this specification are committed golden files edited by hand, and others are recorded by a `needs-human` ticket. Goldens sit in `Zenith2/Tools/Authoring/Tests/Fixtures/WORLD/Golden/`, the ZenithAuthoring fixture root of `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 2:

| Golden | Kind | Content |
|---|---|---|
| `lattice-v1.json` | hand-written from "world.lattice" | the positions, yaws, prefab indices and output keys of a 4 × 3 lattice with `jitter` 0 over a two-region world; the IDs are checked separately against `Zenith_WorldGeneratedID` |
| `spline-v1.json` | hand-written | the samples of the three-point polyline of "Generator document example" with `jitter` 0 and `yawJitterDegrees` 0, including the sample s = 32 exactly at the interior vertex (90, 0, 10), which takes segment 1's yaw 0° ("world.spline") |
| `probegrid-v1.json` | hand-written | a plane-surface probe grid |
| `recorded/*.json` | recorded by the `needs-human` ticket "WORLD generator golden recording", blocked by the D-SEL-HASH ruling and by the generator tickets | lattice and spline with jitter, and scatter over the ENV fixture terrain, each frozen as the version-1 output |

Hand-written goldens hold decimals, not bits: a comparison takes positions within 1e-5 m per axis and yaws within 1e-4° (the spline's arc-length positions and its `Zenith_Atan2` yaws are not exactly representable decimals), and compares output keys, IDs and prefab indices exactly. The recorded goldens and the test that compares a run with them, `Test_WorldGenerator_RecordedGoldensUnchanged`, land together in the recording ticket, so the test never exists without its files and never skips; it compares outputs bit for bit, because the recording and the run are the same build. A worker never records a golden (WORKER-EVIDENCE).
