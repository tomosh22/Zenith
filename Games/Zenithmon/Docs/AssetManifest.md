# Zenithmon -- Asset Manifest (Generated-Asset Catalogue)

**Document purpose:** The complete catalogue of every baked asset Zenithmon
generates, the per-family file sets, the bake-governance rules (manifest
stamps, determinism, seeds), and the bake budgets. This is the CONTRACT the
generators are built against: the generators themselves land at S3 (terrain)
and S4 (creatures / humans / buildings / props), but the file sets, counts,
and invariants below are locked now so scene authoring, tests, and CI can be
written against them.

**Companion docs:** GameDesignDocument.md (what the assets depict),
Scope.md (what is cut), TestPlan.md (determinism + generator test specs),
BuildEnvironment.md (the tools build that runs the bake), CIPolicy.md
(why CI never sees these files), Roadmap.md (S3/S4 stage gates).

**Last updated:** 2026-07-13 (S3 -- FrontEnd/Dawnmere/PlayerHome now carry the
verified fade + live door round trip under ZM-D-057; all scene/terrain outputs
and visual-gate captures remain git-ignored).

---

## 0. At-a-glance

### 0.1 The one rule that shapes everything

**Every asset is procedurally generated and baked to disk by ZENITH_TOOLS
builds. Nothing under `Games/Zenithmon/Assets/` is ever committed** -- the
directory is git-ignored (repo norm, same as RenderTest/DP/CityBuilder).
A fresh checkout has NO assets; a `Vulkan_vs2022_Debug_Win64_True` build+run
regenerates them. There is no artist, no outsourcing, no import pipeline --
the "asset team" is generator/authoring code (`Games/Zenithmon/Tools/` for
the S4 families and `Source/World/ZM_TerrainAuthoring` for S3 terrain).

### 0.2 Headline counts

| Family | Count | Generator | Lands at |
|---|---|---|---|
| Creature species file sets | ~150 species x 15 files (see 1.2) | ZM_CreatureGen (+ ZM_CreatureAnimGen, ZM_TextureSynth) | S4 |
| Creature animation clips | ~900 .zanim (6 per species) | ZM_CreatureAnimGen | S4 |
| Human models | ~35 .zmodel on ONE shared skeleton + ONE shared 9-clip set | ZM_HumanGen | S4 |
| Building models | ~30 .zmodel | ZM_BuildingGen | S4 |
| Props | ~25 models (incl. ~6 battle-dome biome dressing sets) | ZM_PropGen | S4 |
| Terrain sets | 1 per outdoor scene, ~25 sets | ZM_TerrainAuthoring (engine E1 + E2 shipped) | S3 (three measurement terrain families complete; only Dawnmere has a preview scene) -> S9/S10 (all) |
| Scenes | ~40 .zscen (boot-authored via AddStep_*) | ZM_SceneAuthoring from ZM_WorldSpec | S0 (FrontEnd) onward |
| Behaviour graphs | .bgraph glue graphs (menu/NPC/cutscene) | ZM_GraphAuthoring (Zenith_GraphBuilder DSL) | S6 onward |

### 0.3 What is deliberately NOT an asset

- **Game data tables** (species stats, moves, items, abilities, natures, type
  chart, encounters, trainers, dex text, `ZM_WorldSpec`) are **compiled
  `const` C arrays** in `Source/Data/*.cpp` -- code, not disk assets. Zero
  file I/O in headless tests; validated by the `ZM_Tests_Data` suite.
- **Audio** -- the engine has no audio system. No audio assets exist or are
  planned. (See Scope.md.)
- **Nintendo IP** -- all ~150 species, all moves/abilities/towns use original
  names and original generated art. Mainline MECHANICS only.

### 0.4 Pipeline constraints (load-bearing engine limits)

| Constraint | Value | Implication |
|---|---|---|
| Skeleton bone count | engine max 100; creature archetypes cap at <= 30 | fixed per-archetype bone topology so clips transfer within an archetype |
| Bone influences per vertex | <= 4 (loft technique uses <= 2 per ring) | |
| Texture format | .ztxtr v2; creature albedo BC1 512x512; terrain Height R32F | |
| Materials | .zmtrl v5 | shiny variants are child materials over the same mesh |
| Scenes | .zscen v7 | |
| Winding | CCW, cross(C-A, B-A) faces outward | generator unit tests assert winding |
| Terrain grid | fixed 64x64 chunks x 64 m (compile-time) | rect export (E2) is how we avoid 12k files per terrain |
| Asset path prefixes | `game:` / `engine:` | |

---

## 1. Creatures (~150 species)

### 1.1 Generation model (S4)

Eight archetypes (QUADRUPED / BIPED / AVIAN / SERPENT / AQUATIC / INSECTOID /
BLOB / FLOATER-PLANTOID), each a single builder producing mesh + skeleton
together via the StickFigure loft technique (rings along bone chains; master
reference: `Tools/Zenith_Tools_TestAssetExport.cpp`). A species recipe in
`ZM_SpeciesData` = archetype + evo stage + size class + family seed; derived
parameters (proportions, appendages, horns/ears/tails/wings, eye decal,
palette from type identity, pattern via ZM_TextureSynth). Evolution lines
share archetype + family seed, +1 elaboration tier per stage. Shiny =
hue-rotated albedo + child material, same mesh. Archetype count may flex
8 -> 6 without touching the dex data model (accepted risk mitigation).

### 1.2 Per-species file set (THE contract -- 15 files per species)

| File | Count per species | Notes |
|---|---|---|
| .zmesh | 1 | lofted skinned mesh |
| .zskel | 1 | <= 30 bones, fixed per-archetype topology |
| .zanim | 6 | Idle / Walk / Attack / Special / Hit / Faint -- archetype clip templates instantiated + exported PER SPECIES |
| .ztxtr (albedo) | 2 | normal + shiny (hue-rotated), BC1 512x512 |
| .ztxtr (dex icon) | 1 | flat icon for dex/party/box UI |
| .zmtrl | 2 | normal + shiny (child material) |
| .zmodel | 2 | normal + shiny bundles |

Totals: ~2,250 files, of which ~900 .zanim. All ~150 species are **eagerly
baked** (no lazy per-encounter generation); measured budget ~2-4 min for the
full creature family.

Exact directory layout and file-naming scheme under `Assets/`: TBD at S4
(fixed by the first ZM_CreatureGen PR; will be recorded here).

---

## 2. Humans (~35 models)

- **ONE shared skeleton** (generalized StickFigure) + **ONE shared 9-clip
  set** -- every human model binds the same .zskel and reuses the same
  .zanim files. No per-model clips.
- Generator: ZM_HumanGen (S4) -- height / build / skin / hair / outfit
  parameters + attachment meshes.
- Roster (~35): player m/f, professor, mom, rival, 8 gym leaders, Elite 4 +
  champion, ~10 trainer classes, 6 townsfolk.
- Per-model file breakdown (mesh/texture/material/model split): TBD at S4.

---

## 3. Buildings (~30) and props (~25)

- **Buildings** (ZM_BuildingGen, S4): parametric shells (footprint / roof /
  facade texture with baked window + door decals). Roster: 4 house styles x
  3 palettes (12), player home, lab, 8 themed gyms, Care Center, Trade Post,
  League, Battle Tower -- ~30 models. Box colliders are authored at SCENE
  time (2-3 per building leaving door gaps), not baked into the model.
- **Props** (ZM_PropGen, S4): ~25 -- fences, signs, lamps, bridges, ledge
  lips, cave rocks, interior furniture, and the ~6 battle-dome biome
  dressing sets (the battle scene is ONE scene; per-biome dressing is
  swapped at runtime from these baked sets).

---

## 4. Terrain sets (one per outdoor scene, ~25 sets)

Engine changes **E1 and E2 are shipped**: every `Zenith_TerrainComponent`
owns a serialized terrain-set name and all runtime streaming/physics/render
paths resolve through it; authoring can export only a validated chunk rect.
Each outdoor scene owns a set under `Assets/Terrain/<SetName>/` (e.g.
`Terrain/Dawnmere/`, `Terrain/Route01/`).

The set name is either empty, meaning the backward-compatible legacy
`Assets/Terrain/` root, or matches
`[A-Za-z0-9][A-Za-z0-9_-]{0,63}` exactly. Named sets resolve only to that
single direct child below `Assets/Terrain/`; separators, dots, whitespace,
absolute paths, drive/UNC prefixes, control/non-ASCII bytes and names longer
than 64 bytes are rejected transactionally. Terrain-component serialization
v4 appends the set name after the complete v3 payload. Readers of v1-v3, and
v4 payloads containing an invalid set, safely select the empty legacy set.

The terrain editor stages `SetAssetSet` without retargeting an initialized
live component. `BakeFull` validates and writes the staged textures, then
commits that same set immediately before synchronous mesh cleanup/export and
physics/render regeneration; failed bakes retain dirty session state. Named
sets co-locate textures and meshes in their set directory, while the empty
legacy set preserves the historical split (`Textures/Terrain/` for textures,
`Terrain/` for meshes). Cleanup is non-recursive and removes only direct
generated `.zmesh` files after canonical containment checks. Rect-export
validation and target resolution happen before cleanup, directory creation,
editor-map allocation, or component/streaming mutation, so invalid requests
are transactional.

Authoring automation uses `AddStep_TerrainSetAssetSet(szSet)`. Its queued
action owns the argument bytes and validates/preflights before staging. It
stamps an uninitialized selected component so a following scene save persists
the set; initialized components may only change sets through `BakeFull`.
`AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` queues the bounded
mesh export with all four signed coordinates preserved and validated before a
standalone editor session can open.

### 4.1 Per-set file set

| File | Format | Notes |
|---|---|---|
| Height.ztxtr | R32F | |
| Splatmap_RGBA.ztxtr | RGBA | 4-material palette |
| GrassDensity.ztxtr | R32F | doubles as the gameplay tall-grass encounter map (ZM_TallGrassSystem keeps its own CPU copy) |
| Render_X_Y.zmesh | exactly 1 per exported chunk | HIGH render source; a missing/invalid sparse source becomes `SOURCE_UNAVAILABLE` at runtime |
| Render_LOW_X_Y.zmesh | exactly 1 per exported chunk | LOW render source |
| Physics_X_Y.zmesh | exactly 1 per exported chunk | physics source |
| ZM_TerrainRecipe.manifest | 12-byte binary marker | terrain-family warm gate: ASCII `ZMTR`, little-endian version, little-endian required-output count; published atomically only after every required output validates |

**Rect export only (E2):** bounds are inclusive, non-normalizing, and must
satisfy `0 <= min <= max < 64` on both axes while containing the hard-required
anchor chunk `(0,0)`. An accepted rectangle of width `W` and height `H` writes
exactly `3 * W * H` direct `.zmesh` files with their absolute fixed-grid
coordinates; the complete 64x64 sheet is exactly 12,288 files. This crops the
existing fixed 4096x4096 m grid for bake/file-count purposes and does **not**
resize it; per-instance extent remains deferred E6 work. The measured crop
classes are routes at 16x24 chunks and towns at 16x16. Eleven towns plus 14
routes project to 24,676 terrain-family files; the GDD's exact 15-route count
projects to 25,832.

Component initialization still hard-requires the anchor and skips other
missing render/physics chunks with warnings. Dynamic HIGH streaming uses a
bounded parser for the shared canonical 28-byte terrain vertex layout and
fixed HIGH counts. A missing, truncated, malformed, wrong-layout, or
out-of-range-index HIGH file causes no allocation or eviction: the chunk keeps
LOW residency and is marked `SOURCE_UNAVAILABLE`. Classification warns once,
is not retried on later frames, and is reset by terrain teardown/regeneration.
Source probing is capped at 32 attempts per frame independently of the
existing eight successful uploads, preventing sparse holes from starving a
later valid chunk.

### 4.2 Authoring recipe (per scene, in ZM_TerrainAuthoring)

ResetSession -> SetAssetSet -> GenerateProcedural(sceneSeed) -> route-corridor
flatten dabs along the authored polyline -> town pads at building footprints
-> Erode -> AutoSplat x4 + dirt-path splat -> GrassDensity brush patches
beside (never on) the path -> checked terminal SaveTextures +
ExportChunksRect + manifest publication. Recipes are driven from ZM_WorldSpec
rows. Tree placement is a future content pass; the three S3 measurement
recipes intentionally contain no trees. Only Dawnmere currently warm-authors
a preview scene; Thornacre and Route1 are terrain measurements, not playable
or dressed scenes.

### 4.3 Measured terrain families

- **Identity:** WorldSpec terrain set `Dawnmere`; FNV-1a seed `0x7BF32CA4`;
  authored coordinates `0..1024`; inclusive export rectangle `(0,0)..(15,15)`.
- **Exact ignored family:** 256 `Render`, 256 `Render_LOW`, and 256 `Physics`
  meshes; `Height.ztxtr`, `Splatmap_RGBA.ztxtr`, and
  `GrassDensity.ztxtr`; one `ZM_TerrainRecipe.manifest`. That is **771
  required outputs + the marker = 772 files** under
  `Assets/Terrain/Dawnmere/`.
- **Atomic warm gate:** the marker is exactly 12 bytes: `ZMTR`, version 1,
  required-output count 771. A cold/forced run removes stale completion state;
  finalization writes a temporary marker and atomically renames it only after
  all 771 files exist and are non-empty. A warm run revalidates the marker and
  every output before authoring `Assets/Scenes/Dawnmere.zscen`.
- **Historical first-bake timings:** the original ZM-D-053 standalone cold
  bake was **63.671 s** and its warm graphics boot was **14.614 s**. The later
  calibrated study reran Dawnmere under the same harness as the other two
  recipes and measured **59.035 s**; the two observations are deliberately
  distinguished rather than rewriting the first result.
- **Grass:** the scene-owned `ZM_TerrainGrass` loads the 1024x1024 CPU density
  map on Awake, does not touch Flux headless, and regenerates from terrain
  physics on a graphics boot. First load and reload each generated/uploaded
  exactly **200,159 blades from 5,133 triangles**, then FrontEnd teardown
  cleared all Dawnmere grass state.
- **Scene-owned traversal pair + feet marker:** the ignored `Dawnmere.zscen`
  authors `TownCenterSpawn` with order-105 `ZM_SpawnPoint`, tag `TownCenter`,
  and transform **(512, 25.98577, 480)**. Spawn-marker transforms denote feet,
  not capsule centres. It also authors a `Player` at the scale-derived centre
  **(512, 26.88577, 480)** with transform scale
  **(0.8, 1.8, 0.8)**, a dynamic generic capsule, and order-102
  `ZM_PlayerController`; its main camera carries order-103 `ZM_FollowCamera`
  with authored yaw 0. The exact surface sample plus the 0.9 m capsule
  half-extent produces that centre. A SINGLE reload replaces the scene-owned
  Player/camera while the persistent manager places and re-enables the new
  generation at the marker. Dawnmere now additionally authors a replaceable
  greybox home shell at `(384,27.440985,456)`, a `FromHome` feet marker at
  **(384,26.590313,482)**, and the live `HomeDoorTrigger` at
  `(384,27.161484,476)` targeting **build 40 / `Door`**. The order-107
  `ZM_GreyboxVisual` marker rebuilds unit-cube visuals at runtime; these
  transitional blocks create no baked model/material files and are explicitly
  replaced by the S4 art pipeline without changing collision or traversal.

The complete ZM-D-054 measurement registry is workspace-local and ignored:

| Terrain set | Kind / crop | Chunks | Required outputs + marker | Family bytes | Calibrated process wall | Internal recipe timer |
|---|---|---:|---:|---:|---:|---:|
| `Dawnmere` | Town, 16x16 | 256 | 771 + 1 = **772 files** | **204,684,116** | **59.035 s** | **42.588 s** |
| `Thornacre` | Town, 16x16 | 256 | 771 + 1 = **772 files** | **204,684,116** | **69.979 s** | **53.657 s** |
| `Route1` | Route, 16x24 | 384 | 1,155 + 1 = **1,156 files** | **262,985,940** | **80.804 s** | **64.541 s** |
| **Measured total** | | **896** | **2,700 files** | **672,354,172** | | |

Each row is one isolated ignored directory under `Assets/Terrain/<set>/`.
The family count is `3 * chunks + 3 textures + 1 marker`; it excludes scene
files. All three selected bakes exited 0, validated every required non-empty
output, and published their marker last. An all-warm same-harness boot took
**16.874 s**, reported all three families warm, and queued zero terrain
recipes.

---

## 5. Scenes and graphs (boot-authored, also git-ignored)

~40 .zscen (0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road,
40+ interiors, 95 Tower) authored from ZM_WorldSpec via shared AddStep_*
helpers, plus .bgraph glue graphs via ZM_GraphAuthoring. These follow the
same rule as everything else under Assets/: regenerated by tools builds,
never committed. `FrontEnd.zscen` (build index 0) authors the non-transient
`ZM_GameStateRoot` with order-104 `ZM_GameStateManager` plus a full-screen black
order-10000 `WarpFade` UIOverlay; runtime `OnStart` makes that root persistent
and retires duplicate managers. `Dawnmere.zscen` (build index 2) authors the
exact `TownCenter` and `FromHome` feet markers, scene-owned Player/camera, and
live home-door edge described above. `PlayerHome.zscen` (build index 40) is
terrain-independent and authored on every tools boot, including headless: a
collidable greybox shell, scene-owned Player/camera, `Door` feet marker at
`(0,0,3.5)`, and `PlayerHomeExitTrigger` at `(0,1,5.2)` targeting
**build 2 / `FromHome`**. All three scenes are ignored outputs; their real
Dawnmere -> PlayerHome -> Dawnmere route is covered by the S3 P1. The measured
Thornacre and Route1 terrain families do **not** imply that their scenes,
trees, dressing, traversal, or gameplay content exist.

The S3 visual-gate evidence is also ignored and never staged:
`Build/artifacts/zenithmon/s3/visual/01_dawnmere_exterior_terrain_grass_camera.png`,
`02_playerhome_interior.png`, and
`03_dawnmere_return_camera_reacquired.png`. Capture
`capture_final_posthitch_20260713_183717` ran the definitive binary's round-trip
test to PASS (**673 frames / 14619.2 ms**, exit 0). The three valid, ignored,
inspected 1280x720 PNGs have SHA-256 values, in filename order,
`9FEFA6E1B20CB9F1647F19A0416FCD6A80ACA653EB6EEEFE6A86DD722790A1DF`,
`13104E86246748BF58AF200DFAC213C2A6B6595A81086E30346B75857280B90E`, and
`B0D49B1CE41ACB98AA184E55ECB1531D34DC76009C3BED0CBD67CCD61C3B4B41`.
Definitive machine-readable authority JSON lives under
`Build/artifacts/zenithmon/s3/final/post_overlay_hitch_fix/` (headless,
windowed, and rendertest subdirectories): **12 parsed / 12 passed / 0 failed**,
likewise ignored.

---

## 6. Bake governance

### 6.1 Manifest stamps (ZM_BakeManifest)

Regeneration is gated per FAMILY (creatures / humans / buildings / props /
each terrain set) by a manifest stamp recording:

1. the family's **generator version** (bumped whenever generator code changes
   output), and
2. **file existence** for the family's complete expected file set.

Stamp valid + all files present -> family skipped (warm boot). Stamp missing,
version-mismatched, or any file absent -> the whole family regenerates.
This is the hardened RenderTest pattern. The three measured families lock the
terrain-family format now (`ZMTR`, v1, count 771 for each 256-chunk town and
count 1,155 for the 384-chunk route in a 12-byte atomic marker; section 4.3);
the non-terrain S4 family marker format remains TBD until those generators land.

### 6.2 Determinism invariant (tested)

**Same seed -> byte-identical re-bake.** This is a hard, tested invariant:

- Seeds derive ONLY from stable IDs and names (species ID, family seed,
  scene name/seed from ZM_WorldSpec) -- never from time, pointers, iteration
  order, or any global RNG.
- S4 gate: generator unit tests assert same-seed byte-identical output
  (plus winding/bounds/weights-sum/bone-cap checks, shiny-differs,
  clip-channels-match-skeleton).
- S9 gate: full bake-determinism check -- re-run the tools boot, assert zero
  diffs (byte-hash snapshot recipe).

### 6.3 Budgets

| Budget | Target |
|---|---|
| Full cold bake (everything absent) | 30-50 min (terrain dominates) |
| Creature family alone | ~2-4 min |
| Warm boot (all stamps valid) | seconds |
| Dawnmere first standalone cold / warm graphics observations (ZM-D-053) | 63.671 s / 14.614 s |
| Calibrated selected cold walls: Dawnmere / Thornacre / Route1 | 59.035 s / 69.979 s / 80.804 s |
| Internal recipe timers: Dawnmere / Thornacre / Route1 | 42.588 s / 53.657 s / 64.541 s |
| All three terrain stamps valid, same-harness warm boot | 16.874 s; zero terrain recipes queued |

The three-real-recipe measurement is complete (ZM-D-054). Using the two-town
wall mean and Route1, the 11-town + 14-route planning model projects **24,676
files / 5,933,328,436 bytes** (5.933 GB / 5.526 GiB), a deliberately
conservative repeated-process **30m 40.833s**, and a one-boot/net **23m
55.857s**. The GDD's exact 11-town + 15-route sensitivity projects **25,832
files / 6,196,314,376 bytes** (6.196 GB / 5.771 GiB), **32m 01.637s** repeated
and **24m 59.787s** net. The net model subtracts the shared 16.874-second warm
baseline from each calibrated wall, scales the terrain work, then adds the
baseline once.

This closes the terrain measurement risk enough to continue S3, but it is not
a completed full-project bake benchmark: the 30-50 minute target includes all
assets, the other generators are unbuilt, there is no explicit byte cap, and
"seconds" warm is qualitative. The sample has two towns and one route, and the
projection assumes later 16x16 / 16x24 crop classes; it is not a statistical
confidence bound. The `~25` planning case and exact 26-outdoor GDD sensitivity
therefore remain explicit. One terrain set per outdoor scene/route remains a
hard requirement; if future full-bake evidence is too slow, optimize the
pipeline rather than share terrain sets.

### 6.4 CI interaction

CI runners never bake and never see Assets/ (fresh checkout, GPU-less).
Every asset/scene-dependent automated test must exists-guard and
RequestSkip. See CIPolicy.md.

---

## 7. Anti-patterns

- Do NOT commit anything under `Games/Zenithmon/Assets/`.
- Do NOT seed any generator from wall-clock, pointer values, or unordered
  container iteration -- it breaks the byte-identical invariant.
- Do NOT add lazy/on-demand generation paths -- everything is eagerly baked
  under the manifest-stamp scheme so runtime never generates.
- Do NOT move data tables (species/moves/etc.) to disk files -- they are
  compiled C arrays by locked decision.
- Do NOT hand-edit a baked file to fix a bug -- fix the generator, bump its
  version, re-bake.
