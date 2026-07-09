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

**Last updated:** 2026-07-09 (S0 -- seeded from the approved plan; no
generator exists yet).

---

## 0. At-a-glance

### 0.1 The one rule that shapes everything

**Every asset is procedurally generated and baked to disk by ZENITH_TOOLS
builds. Nothing under `Games/Zenithmon/Assets/` is ever committed** -- the
directory is git-ignored (repo norm, same as RenderTest/DP/CityBuilder).
A fresh checkout has NO assets; a `Vulkan_vs2022_Debug_Win64_True` build+run
regenerates them. There is no artist, no outsourcing, no import pipeline --
the "asset team" is `Games/Zenithmon/Tools/` (`ZM_CreatureGen`, `ZM_HumanGen`,
`ZM_BuildingGen`, `ZM_PropGen`, `ZM_TerrainAuthoring`, `ZM_TextureSynth`,
`ZM_BakeManifest`).

### 0.2 Headline counts

| Family | Count | Generator | Lands at |
|---|---|---|---|
| Creature species file sets | ~150 species x 15 files (see 1.2) | ZM_CreatureGen (+ ZM_CreatureAnimGen, ZM_TextureSynth) | S4 |
| Creature animation clips | ~900 .zanim (6 per species) | ZM_CreatureAnimGen | S4 |
| Human models | ~35 .zmodel on ONE shared skeleton + ONE shared 9-clip set | ZM_HumanGen | S4 |
| Building models | ~30 .zmodel | ZM_BuildingGen | S4 |
| Props | ~25 models (incl. ~6 battle-dome biome dressing sets) | ZM_PropGen | S4 |
| Terrain sets | 1 per outdoor scene, ~25 sets | ZM_TerrainAuthoring (needs engine E1 + E2) | S3 (first set) -> S9/S10 (all) |
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

Requires engine changes **E1** (per-component serialized terrain-set name
replacing the hard-coded `Terrain/` path) and **E2** (rect chunk export) --
both land at the start of S3. Each outdoor scene owns a set under
`Assets/Terrain/<SetName>/` (e.g. `Terrain/Route01/`, `Terrain/HomeVillage/`).

### 4.1 Per-set file set

| File | Format | Notes |
|---|---|---|
| Height.ztxtr | R32F | |
| Splatmap_RGBA.ztxtr | RGBA | 4-material palette |
| GrassDensity.ztxtr | | doubles as the gameplay tall-grass encounter map (ZM_TallGrassSystem keeps its own CPU copy) |
| Render_X_Y.zmesh | per exported chunk | |
| Render_LOW_X_Y.zmesh | per exported chunk | |
| Physics_X_Y.zmesh | per exported chunk | |

**Rect export only (E2):** the engine grid is a fixed 64x64 chunk sheet; a
full export is ~12k files per terrain. We export only the authored rect
(routes ~16x24 chunks, towns ~16x16) -> ~25k files across all ~25 sets.
Non-anchor missing chunks skip-with-warning; the anchor chunk (0,0) is
hard-required and must always be inside the export rect.

### 4.2 Authoring recipe (per scene, in ZM_TerrainAuthoring)

ResetSession -> SetAssetSet -> GenerateProcedural(sceneSeed) -> route-corridor
flatten dabs along the authored polyline -> town pads at building footprints
-> Erode -> AutoSplat x4 + dirt-path splat -> GrassDensity brush patches
beside (never on) the path -> TreeBrush edges -> SaveTextures +
ExportChunksRect. Recipes are driven from ZM_WorldSpec rows, not bespoke
per-scene code.

---

## 5. Scenes and graphs (boot-authored, also git-ignored)

~40 .zscen (0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road,
40+ interiors, 95 Tower) authored from ZM_WorldSpec via shared AddStep_*
helpers, plus .bgraph glue graphs via ZM_GraphAuthoring. These follow the
same rule as everything else under Assets/: regenerated by tools builds,
never committed. As of S0 only FrontEnd.zscen (build index 0) exists.

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
This is the hardened RenderTest pattern; exact stamp file format TBD at S4.

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

Terrain bake time is the top-ranked project risk: measured with 3 scenes at
S3 before committing to all ~25 (fallback: multiple routes share one
terrain sheet).

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
