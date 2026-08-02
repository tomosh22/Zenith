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

**Last updated:** 2026-08-01 (audited against HEAD `c9d64994`). The S4
asset-generator contract remains unchanged: `ZM_BakeManifest` SHIPPED (ZM-D-085),
the full-family S4 asset gate was SIGNED OFF (ZM-D-088), and all four generator
families remain guarded and git-ignored. Since the previous 2026-07-21 (S6) pass
the COMMITTED inventory grew twice: `ZM_TrainerChallenge.bgraph` became the first
real behaviour-graph output (ZM-D-155, git-ignored like every other bake), and
`Assets/Scenes/ProfLab.zscen` became the FIFTH tracked scene (ZM-D-174). Sections
0.1, 0.2 and 5 carry those changes.

---

## 0. At-a-glance

### 0.1 The one rule that shapes everything

**Every asset is procedurally generated and baked to disk by ZENITH_TOOLS
builds, and almost nothing under `Games/Zenithmon/Assets/` is committed** --
the directory is git-ignored (repo norm, same as RenderTest/DP/CityBuilder).
A fresh checkout has essentially NO assets; a `Vulkan_vs2022_Debug_Win64_True`
build+run regenerates them. There is no artist, no outsourcing, no import
pipeline -- the "asset team" is generator/authoring code
(`Games/Zenithmon/Tools/` for the S4 families and
`Source/World/ZM_TerrainAuthoring` for S3 terrain).

**TWO deliberate exceptions, added by SC1b (ZM-D-145 / ZM-D-147) and widened to
every scene by ZM-D-148:**

| Committed file | Size | Why it is tracked |
|---|---|---|
| `Assets/Navmesh/Dawnmere.znavmesh` | ~365 KB | Loadable with NO GPU, NO terrain component and NO other asset — which is what makes navigation CI-verifiable for the first time. Byte-deterministic across bakes. |
| `Assets/Scenes/*.zscen` (all **five**) | 1.6–27 KB each | Scene content, loadable on a fresh checkout with no bake. `Dawnmere.zscen` matters most: a Null/CI boot never authors it (the Dawnmere block is windowed + all-warm gated), so without it CI has no Dawnmere scene and the authored navmesh component has no gate. The five are `FrontEnd` (27,125 B), `Battle` (4,965 B), `Dawnmere` (4,176 B), `PlayerHome` (1,800 B) and `ProfLab` (1,590 B, added by ZM-D-174). |

**Scene bytes are boot-shape-independent (ZM-D-148), which is what makes this
safe.** Scenes used to bake process-global entity SLOT indices into their file
indices, so a boot whose unit-test population differed re-authored different
bytes — measured at 4 bytes (Battle) and 12 (PlayerHome), and Dawnmere moved
too. The writer now emits dense authoring-order indices, pinned by
`Scene::SceneBytesAreIndependentOfSlotAllocation`. If a scene file ever shows up
modified in `git status` after a boot, that is a REGRESSION of that property —
investigate it rather than just re-committing.

**The one KNOWN violation of that rule is CLOSED as of ZM-D-179, and its cause was
NOT slot allocation.** From ZM-D-173 to ZM-D-178 a windowed tools boot left
`Assets/Scenes/Dawnmere.zscen` modified by exactly **2 bytes** (offsets 3627 / 3635
— the low-order mantissa of `Npc_RivalVesper`'s rotation quaternion).
`Zenith_TransformComponent::WriteToDataStream` was serializing the **live Jolt
body's** pose, so an authored rotation was a function of physics state; the bytes
committed at `a6c66b68` are the one boot in five whose state differed, and they are
provably not `sin`/`cos` of any single float yaw, which rules out codegen drift.
Serialization now emits the transform's own cached pose unless the body has moved
past the engine's own `PhysicsPoseDiffersFromCache` threshold. **A dirty `.zscen`
after a boot is therefore a real signal again — investigate it.** Full diagnosis:
**Q-2026-08-01-002** in [Questions.md](Questions.md) and ZM-D-179.

Everything else — creatures, humans, buildings, props, terrain (~641 MB) —
stays git-ignored. Adopting git-LFS for the heavy families is a separate, still
open user decision.

### 0.2 Headline counts

| Family | Count | Generator | Lands at |
|---|---|---|---|
| Creature species file sets | 152 species x 15 files = 2,280 (see 1.2) | ZM_CreatureGen + ZM_TextureSynth (SHIPPED) | S4 |
| Creature animation clips | 152 species x 6 = 912 .zanim | ZM_CreatureAnimGen (SHIPPED, baked into each species' bundle) | S4 |
| Human models | ~35 .zmodel on ONE shared skeleton + ONE shared 9-clip set | ZM_HumanGen | S4 |
| Building models | ~30 .zmodel | ZM_BuildingGen | S4 |
| Props | ~25 models (incl. ~6 battle-dome biome dressing sets) | ZM_PropGen | S4 |
| Terrain sets | 1 per outdoor scene, ~25 sets | ZM_TerrainAuthoring (engine E1 + E2 shipped) | S3 (three measurement terrain families complete; only Dawnmere has a preview scene) -> S9/S10 (all) |
| Scenes | ~40 .zscen (boot-authored via AddStep_*) | ZM_SceneAuthoring from ZM_WorldSpec | S0 (FrontEnd) onward |
| Behaviour graphs | **1 shipped** `.bgraph` (`game:Graphs/ZM_TrainerChallenge.bgraph`); more are future glue graphs | ZM_GraphAuthoring (Zenith_GraphBuilder DSL) | S7 item 3 SC7 (ZM-D-155); was deferred from S6 |

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
| Texture format | .ztxtr v2; creature albedo + shiny BC1 512x512, dex icon BC1 128x128; terrain Height R32F | |
| Materials | .zmtrl v5 | creature shiny is an INDEPENDENT material over the same mesh (its own _shiny.ztxtr), NOT a child of the base (ZM-D-065) |
| Scenes | .zscen v7 | |
| Winding | CCW, cross(C-A, B-A) faces outward | generator unit tests assert winding |
| Terrain grid | fixed 64x64 chunks x 64 m (compile-time) | rect export (E2) is how we avoid 12k files per terrain |
| Asset path prefixes | `game:` / `engine:` | |

---

## 1. Creatures (152 species)

### 1.1 Generation model (S4 -- SHIPPED, ZM-D-060..065)

Eight archetypes (QUADRUPED / BIPED / AVIAN / SERPENT / AQUATIC / INSECTOID /
BLOB / FLOATER-PLANTOID), **all 8 builders now wired**, each a single builder
producing mesh + skeleton
together via the StickFigure loft technique (rings along bone chains; master
reference: `Tools/Zenith_Tools_TestAssetExport.cpp`). A species recipe in
`ZM_SpeciesData` = archetype + evo stage + size class + family seed; derived
parameters (proportions, appendages, horns/ears/tails/wings, eye decal,
palette from type identity, pattern via ZM_TextureSynth). Evolution lines
share archetype + family seed, +1 elaboration tier per stage. Shiny =
hue-rotated albedo + an INDEPENDENT material, same mesh (ZM-D-065). The
8 -> 6 archetype-flex risk is now closed: all 8 archetypes shipped and every
one of the 152 species builds a valid creature (`CreatureGen_AllSpeciesBuildable`).

### 1.2 Per-species file set (THE contract -- 15 files per species)

**ZM_CreatureGen bundle (SHIPPED, ZM-D-065 -- 15 files per species).**
`ZM_BakeCreature` (TOOLS-only) writes all fifteen under `game:Creatures/<Name>/`
-- the 9-file mesh/skeleton/material/model/icon core, plus the 6 per-species
`.zanim` baked by `ZM_BakeCreatureClips` (SC6):

| File | Count per species | Format / notes |
|---|---|---|
| `<Name>.zmesh` | 1 | lofted skinned mesh, <= 30 bones (`uZM_GEN_CREATURE_BONE_CAP`) |
| `<Name>.zskel` | 1 | fixed per-archetype topology, single-rooted |
| `<Name>_albedo.ztxtr` | 1 | base albedo, BC1 512x512 |
| `<Name>_shiny.ztxtr` | 1 | hue-rotated shiny albedo (band [80,280) deg), BC1 512x512 |
| `<Name>_icon.ztxtr` | 1 | flat dex/party/box icon, BC1 128x128 |
| `<Name>.zmtrl` | 1 | base material, .zmtrl v5 |
| `<Name>_shiny.zmtrl` | 1 | shiny material, .zmtrl v5 -- an INDEPENDENT material over the same mesh (its own `_shiny.ztxtr`), NOT a child of the base (ZM-D-065) |
| `<Name>.zmodel` | 1 | base bundle (mesh + skeleton + material), .zmodel v2 -- self-lists all 6 clips below via `AddAnimationPath` (IDLE..FAINT order) |
| `<Name>_shiny.zmodel` | 1 | shiny bundle (same mesh + skeleton, shiny material), .zmodel v2 -- self-lists the same 6 clips |
| `<Name>_Idle.zanim` | 1 | Idle clip -- rotation-only, 24 ticks/sec, looping |
| `<Name>_Walk.zanim` | 1 | Walk clip -- rotation-only, 24 ticks/sec, looping |
| `<Name>_Attack.zanim` | 1 | Attack clip -- rotation-only, 24 ticks/sec, one-shot ends neutral |
| `<Name>_Special.zanim` | 1 | Special clip -- rotation-only, 24 ticks/sec, one-shot ends neutral |
| `<Name>_Hit.zanim` | 1 | Hit clip -- rotation-only, 24 ticks/sec, one-shot ends neutral |
| `<Name>_Faint.zanim` | 1 | Faint clip -- rotation-only, 24 ticks/sec, settles and clamps |

**On-disk layout (now final -- was TBD).** One directory per species; the
canonical asset ref is `game:Creatures/<SpeciesName>/<SpeciesName><suffix>.<ext>`
(`ZM_CreatureAssetPath`), and the tools bake mirrors it as the filesystem path
under `GAME_ASSETS_DIR` (`ZM_CreatureFsPath`). Every ref EMBEDDED in a baked
asset (albedo/shiny texture, mesh, skeleton, material) is a `game:` ref; only the
write targets are filesystem paths. All **152** species are **eagerly baked** (no
lazy per-encounter generation): the bundle family is **152 x 15 = 2,280 files**.
Measured budget ~2-4 min for the full creature family.

**Determinism / version stamp.** Every output byte is a pure function of the
species id (section 6.2); the generator version is `uZM_CREATUREGEN_VERSION`
(currently **3** -- SC6 bumped it because each baked `.zmodel` now carries 6
`AddAnimationPath` entries, so pre-anim bakes self-invalidate), golden-pinned --
a change to the generation algorithm bumps it and forces a cold family re-bake.
Locked by the `ZM_Gen` creature units ([TestPlan.md](TestPlan.md) 5.4).

**Animation clips (SHIPPED).** The 6 per-species `.zanim`
(Idle / Walk / Attack / Special / Hit / Faint) are baked by
`ZM_BakeCreatureClips` inside `ZM_BakeCreature` (after the mesh/texture bakes
create the species folder), and both `.zmodel` bundles (base + shiny) self-list
them via `AddAnimationPath` in IDLE..FAINT order. Clips are pure
`f(archetype, clip-id)` rotation-only at 24 ticks/sec, so a clip is
byte-identical across every species of an archetype. They are part of the
15-file per-species bundle above (**152 x 6 = 912** `.zanim` across the family).

---

## 2. Humans (~35 models)

> **★ ZM-D-181: THIS FAMILY NOW HAS A SHIPPED RUNTIME CONSUMER.** Until then the
> only thing that loaded a human `.zmodel` was the asset-gallery test. The six
> authored Dawnmere NPCs and the player in all three scenes now wear these models,
> animated Idle <-> Walk off commanded speed. Four things changed with it:
>
> * **The bind space is CENTRE-ANCHORED (generator v2).** v1 built feet-near-y=0;
>   Zenithmon's authored entity position is the body CENTRE, so the mesh moved to
>   meet it rather than the whole game moving to meet the mesh. It is a rigid
>   translation of the root bone (the only bone with parent -1) and every vertex,
>   so skinning and the rotation-only shared clips are mathematically untouched.
>   **The version bump is load-bearing**: every v1 bake on disk is feet-at-zero and
>   would render sunk into the floor.
> * **Body metrics are MEASURED over the body vertex PREFIX.** `ZM_MeasureHumanBody`
>   measures the six body loft parts only -- hair and attachments are excluded, so a
>   hat cannot decide how tall its wearer is -- and it measures BEFORE the anchor, so
>   it can never be circular. `fZM_HUMAN_CANONICAL_BODY_HEIGHT` (2.604300) and
>   `fZM_HUMAN_MESH_CENTRE_Y` (1.307005) are pinned by a boot unit that re-derives
>   both from a freshly built mesh. They are MODEL-space units, not metres.
> * **`fZM_HUMAN_VISUAL_SCALE`** (`Source/World/ZM_HumanBody.h`) is the uniform
>   authored transform scale that maps that ~2.6-unit body onto the game's 1.8 m
>   body contract.
> * **Readiness is a policy, and the bake is LAZY.** `ZM_AreHumanAssetsReady`
>   (`Source/World/ZM_HumanAssetPolicy.h`) asks the manifest; on a cold TOOLS tree it
>   spends one bake attempt and re-asks. On a `_False` build or a cold clone that
>   cannot bake, a resolved human draws the proportioned palette fallback block
>   instead. **Appearance is bake-dependent; GAMEPLAY DIMENSIONS ARE NOT** -- the
>   collider comes from the compiled body contract either way.


- **ONE shared skeleton** (generalized StickFigure) + **ONE shared 9-clip
  set** -- every human model binds the same .zskel and reuses the same
  .zanim files. No per-model clips.
- Generator: ZM_HumanGen (S4 -- SHIPPED, SC1..SC5) -- height / build / skin /
  hair / outfit parameters + attachment meshes.
- Roster (~35): player m/f, professor, mom, rival, 8 gym leaders, Elite 4 +
  champion, ~10 trainer classes, 6 townsfolk.

**Human family file sets (SHIPPED -- 10 shared files + 4 per model).**
`ZM_HumanGen` INVERTS the creature bundle: instead of a full per-model bundle,
the whole roster shares ONE rig and ONE clip set, baked ONCE, and each model
contributes only mesh + texture + material + model. `ZM_BakeHumanShared`
(TOOLS-only) writes the shared set ONCE under `game:Humans/Shared/`; `ZM_BakeHuman`
(TOOLS-only) writes the 4 per-model files under `game:Humans/<Name>/`. Per-model
variation is mesh-loft + texture ONLY -- NO per-model skeleton, NO per-model
clips (contrast creatures, which bake 15 files EACH, section 1.2).

**Shared set (baked ONCE for the whole roster, under `game:Humans/Shared/`):**

| File | Count | Format / notes |
|---|---|---|
| `Human.zskel` | 1 | the ONE shared 16-bone humanoid rig, single-rooted; every model binds it |
| `Human_Idle.zanim` | 1 | Idle clip -- rotation-only, 24 ticks/sec, looping |
| `Human_Walk.zanim` | 1 | Walk clip -- rotation-only, 24 ticks/sec, looping |
| `Human_Run.zanim` | 1 | Run clip -- rotation-only, 24 ticks/sec, looping |
| `Human_Talk.zanim` | 1 | Talk clip -- rotation-only, 24 ticks/sec, looping |
| `Human_Wave.zanim` | 1 | Wave clip -- rotation-only, 24 ticks/sec, one-shot returns to identity |
| `Human_Point.zanim` | 1 | Point clip -- rotation-only, 24 ticks/sec, one-shot returns to identity |
| `Human_Cheer.zanim` | 1 | Cheer clip -- rotation-only, 24 ticks/sec, one-shot returns to identity |
| `Human_Hurt.zanim` | 1 | Hurt clip -- rotation-only, 24 ticks/sec, one-shot returns to identity |
| `Human_Faint.zanim` | 1 | Faint clip -- rotation-only, 24 ticks/sec, settles into and holds its final pose |

That is 1 `.zskel` + 9 `.zanim` = **10 shared files**, baked once for the entire
roster (not per model).

**Per-model set (under `game:Humans/<Name>/`, 4 files each):**

| File | Count per model | Format / notes |
|---|---|---|
| `<Name>.zmesh` | 1 | lofted skinned humanoid mesh, the 16 SHARED bones |
| `<Name>_albedo.ztxtr` | 1 | base albedo, BC1 256x256 |
| `<Name>.zmtrl` | 1 | matte dielectric material, .zmtrl v5 -- albedo in BASE_COLOR |
| `<Name>.zmodel` | 1 | binds the shared `Human.zskel` by ref + self-lists all 9 shared `.zanim` in IDLE..FAINT order + one material -- NO per-model skeleton, NO per-model clips |

**Family total.** 10 shared files (baked once) + (~34 models x 4 per-model
files). Because the rig and clips are shared, the whole family is `10 + ~34 x 4`
rather than `~34 x 15`. Every ref EMBEDDED in a baked `.zmodel` (the shared
skeleton, the 9 shared clips, the per-model mesh/material) is a `game:` ref; only
the write targets are filesystem paths.

**Determinism / version stamp.** Every output byte is a pure function of the
roster id (section 6.2); the generator version is `uZM_HUMANGEN_VERSION`
(currently **1**), golden-pinned -- a change to the generation algorithm bumps it
and forces a cold family re-bake. Locked by the `ZM_Gen` HumanGen units
([TestPlan.md](TestPlan.md) 5.4).

---

## 3. Buildings (30) and props (25)

`ZM_BuildingGen` and `ZM_PropGen` (S4 -- SHIPPED, SC1..SC5) are the two
STATIC-model generators: each turns a roster row into a box composition lofted
via `ZM_StaticMesh` and bakes a **4-file STATIC bundle** per model with NO
skeleton and NO animation. This contrasts creatures (15 files EACH, section 1.2)
and humans (a shared rig + 4 per-model files, section 2): here every model is
fully self-contained in 4 files and carries no rig at all. Both families bake the
mesh through the shipped `ZM_GenBakeStaticMesh` bridge (skeleton-less) and the
albedo through `ZM_SynthBakeAlbedoBC1`; the baked `.zmodel` lists one submesh +
one material and carries NO skeleton path and NO animation paths. Box colliders
are authored at SCENE time for BOTH families, never baked into the model.

**Buildings (30 models).** `ZM_BuildingGen` (SHIPPED): parametric shells
(footprint / roof / facade texture with baked window + door decals). Roster:
4 house styles x 3 palettes (12), player home, lab, 8 themed gyms, Care Center,
Trade Post, League, Battle Tower -- 30 models. Box colliders are authored at
SCENE time (2-3 per building leaving door gaps), not baked. `ZM_BakeBuilding`
(TOOLS-only) writes the 4 files under `game:Buildings/<Name>/`:

| File | Count per building | Format / notes |
|---|---|---|
| `<Name>.zmesh` | 1 | skeleton-less static shell mesh, single submesh (`HasSkinning()` == false) |
| `<Name>_facade.ztxtr` | 1 | facade albedo (baked window + door decals), BC1 256x256 |
| `<Name>.zmtrl` | 1 | matte dielectric, .zmtrl v5 -- facade in BASE_COLOR, roughness 0.8 / metallic 0.0 |
| `<Name>.zmodel` | 1 | single-submesh mesh + one material, .zmodel v2 -- NO skeleton path, NO animation paths |

Family total: **30 buildings x 4 = 120 files**.

**Props (25 models).** `ZM_PropGen` (SHIPPED): 25 models -- fences, signs, lamps,
bridges, ledge lips, cave rocks, interior furniture, and the ~6 battle-dome biome
dressing sets (the battle scene is ONE scene; per-biome dressing is swapped at
runtime from these baked sets). Like buildings, props carry SCENE-authored box
colliders, not baked into the model. `ZM_BakeProp` (TOOLS-only) writes the 4
files under `game:Props/<Name>/`:

| File | Count per prop | Format / notes |
|---|---|---|
| `<Name>.zmesh` | 1 | skeleton-less static mesh, single submesh (`HasSkinning()` == false) |
| `<Name>_albedo.ztxtr` | 1 | base albedo, BC1 128x128 |
| `<Name>.zmtrl` | 1 | matte dielectric, .zmtrl v5 -- albedo in BASE_COLOR |
| `<Name>.zmodel` | 1 | single-submesh mesh + one material, .zmodel v2 -- NO skeleton path, NO animation paths |

Family total: **25 props x 4 = 100 files**.

**Determinism / version stamp.** Every output byte is a pure function of the
roster id (section 6.2); the generator versions are `uZM_BUILDINGGEN_VERSION`
(currently **1**) and `uZM_PROPGEN_VERSION` (currently **1**), golden-pinned -- a
change to either generation algorithm bumps its version and forces a cold family
re-bake. Locked by the `ZM_Gen` BuildingGen/PropGen units
([TestPlan.md](TestPlan.md) 5.4).

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
| Physics_X_Y.zmesh | exactly 1 per exported chunk | physics source; **density divisor 4** (289 verts / 1536 indices per chunk) since ZM-D-182, deliberately coarser than the HIGH render mesh. `Zenith_TerrainComponent` VALIDATES these counts against `Flux_TerrainVertexLayout` and rejects a mismatch, so a chunk baked at another divisor does not degrade -- the terrain gets no physics body at all |
| ZM_TerrainRecipe.manifest | 12-byte binary marker | terrain-family warm gate: ASCII `ZMTR`, little-endian version, little-endian required-output count; published atomically only after every required output validates |

> **★ THE WARM GATE IS `(version, COUNT)` -- IT DOES NOT HASH THE BYTES.** Any change that
> rewrites chunk CONTENTS while emitting the same number of files (a collision-density change
> being the exact case) leaves a stale tree reporting warm forever. **Bump
> `uZM_TERRAIN_MANIFEST_VERSION` in the same commit**, and bump the sibling markers in
> CityBuilder and RenderTest too -- see DecisionLog ZM-D-182 and
> `Zenith/Flux/Terrain/CLAUDE.md`. CI cannot catch a miss: `**/Assets/` is gitignored, so CI
> always bakes cold and always passes while every existing developer tree silently breaks.

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
- **Scene-owned traversal pair + feet marker:** the committed `Dawnmere.zscen`
  (tracked since ZM-D-148; it was an ignored output when this row was written)
  authors `TownCenterSpawn` with order-105 `ZM_SpawnPoint`, tag `TownCenter`,
  and transform **(512, 25.99055, 480)**. Spawn-marker transforms denote feet,
  not capsule centres. It also authors a `Player` at the centre
  **(512, 26.89055, 480)**, a dynamic capsule, and order-102
  `ZM_PlayerController`; its main camera carries order-103 `ZM_FollowCamera`
  with authored yaw 0. The exact surface sample plus the 0.9 m capsule
  half-extent produces that centre. A SINGLE reload replaces the scene-owned
  Player/camera while the persistent manager places and re-enables the new
  generation at the marker.
  ★ **UPDATED AT ZM-D-181 -- THE SCALE IS NO LONGER THE BODY.** The player's
  authored transform scale is now the UNIFORM `fZM_HUMAN_VISUAL_SCALE`
  (1.8 / 2.6043 ~ 0.691), which exists only to land the generated human MODEL on
  the body contract; it is **not** (0.8, 1.8, 0.8) and nothing may derive a body
  from it (a uniform scale degenerates a scale-derived capsule into a sphere).
  The 0.9 m half-extent above now comes from the compiled contract in
  `Source/World/ZM_HumanBody.h`, and the capsule is installed explicitly by
  `ZM_PlayerController::EnsureAndConfigureBody`. The order-107 `ZM_GreyboxVisual`
  no longer rebuilds a unit cube for a human: the six authored NPCs and the player
  wear the baked `ZM_HumanGen` models with an Idle/Walk animator, and the palette
  block survives only as the COLD-START FALLBACK when no human bake is loadable.
  Walls, floors, doors and lintels are still unit cubes and are unchanged.

### The Dawnmere Home, its entrance and its approach (ZM-D-173, re-shaped at ZM-D-181)

Every coordinate below is authored from ONE place --
`Source/World/ZM_DawnmerePlacement.h`'s Home block -- which the boot units
and both real-scene guards read as well. All Y values are DERIVED by fixed
formulas from ground heights MEASURED on the baked heightfield by
`ZM_DawnmereHomeGroundTruth_Test`; none is hand-tuned. **The formulas, not the
numbers, are the contract** -- the numbers below are what those formulas evaluate
to against the current measured table and move whenever it is re-measured.

★ **RE-MEASURED AT ZM-D-182**, because the terrain's collision divisor moved 8 -> 4
and the ground samples are real raycasts against the collision mesh. A recipe,
seed, flatten-radius **or density** change re-measures them; run
`ZM_DawnmereHomeGroundTruth_Test`, paste its `MEASURED FEET Y` lines into
`ZM_DawnmerePlacement.cpp`, and every Y below follows automatically.

★ **THE Y COLUMN BELOW IS NOT HAND-ARITHMETIC.** It is the oracle's own
`DERIVED authored Y` log line, verified against a freshly re-baked divisor-4
terrain on 2026-08-02 (`shell=27.544590 doorLeft=27.470591 doorRight=27.442789
lintel=28.970591 trigger=27.536751 spawnFeet=26.542120`, every row's
`tableError=0.00000`). If you ever need to refresh this table, read that line
rather than recomputing the formulas by hand.

| Entity | Authored transform | Scale | Notes |
|---|---|---|---|
| `DawnmereHomeShell` | **(384, 27.54459, 482.5)** | (17, 4, 13) | occupies **x 375.5..392.5, z 476..489**; Y = lowest of four measured footprint corners (25.59459) + half height 2.0 - 0.05 embed |
| `DawnmereHomeDoorLeft` | **(381.75, 27.47059, 476)** | (0.5, 2.5, 0.5) | jamb; own measured ground 26.22059 + half height 1.25 |
| `DawnmereHomeDoorRight` | **(386.25, 27.44279, 476)** | (0.5, 2.5, 0.5) | jamb; own measured ground 26.19279 + 1.25 |
| `DawnmereHomeDoorLintel` | **(384, 28.97059, 476)** | (5, 0.5, 0.5) | higher door ground (26.22059) + a full 2.5 m jamb + its own half thickness 0.25 |
| `HomeDoorTrigger` | **(384, 27.53675, 474)** | (4, 2.5, 2) | sensor, occupies **z 473..475**; measured ground 26.28675 + 1.25; targets **build 40 / `Door`** |
| `FromHomeSpawn` | **(384, 26.54212, 468)** | -- | FEET marker, the measured surface verbatim |

The entrance decoration plane is **z = 476**, which is the shell's **-Z** face.

**★ THE ENTRANCE IS A FRAME, NOT A DOOR PANEL.** The two jambs span x 381.5..382.0
and x 386.0..386.5, leaving a **4.0 m wide x 2.5 m tall opening** they do not fill,
bridged by the lintel. That aperture is deliberately the PlayerHome interior's own,
so the exterior and interior portals read as one home entrance rather than two
unrelated blockout scales. Nothing swings and nothing closes: the warp is the
sensor 2 m out, so the player is taken through before ever reaching the gap.

**Shape history.** Before ZM-D-173 the shell sat at z 436..476 with its entrance on
the **+Z** face, which put the whole building behind the player at the doorway.
ZM-D-173 moved it +40 m to z 476..516 at (16, 6, 40). **ZM-D-181 replaced that
16 x 6 x 40 m false depth with the present 17 x 4 x 13 m envelope** -- the rounded-up
outer envelope of PlayerHome's 16.5 x 12.5 m interior walls -- keeping the -Z
entrance at z = 476 so the open forecourt, the fixed-yaw camera direction, the
trigger and the return route all survive unchanged.

#### Camera-arm clearance contract (binding)

The overworld camera keeps the yaw captured from the scene (**0** for Dawnmere)
and resolves occlusion by raycasting from the player pivot to the desired camera
position. At that yaw, for a player standing on ground at height `feet`:

| Quantity | Value |
|---|---|
| player capsule centre | `feet + 0.9` (scale-derived half-extent) |
| camera pivot | centre + 0.6 = **`feet + 1.5`** |
| desired camera | centre + 3.0 vertically = **`feet + 3.9`**, and 5.5 m behind the authored forward |
| pivot -> camera distance | `sqrt(5.5^2 + 2.4^2)` = **6.0008 m** |
| **required clamped arm** | **>= 50% of that = 3.0004 m** |

**Collision padding is a LONGITUDINAL 0.20 m subtraction off the hit distance,
not a widened collision volume.** A solid hit before **~3.2004 m** therefore
violates the rule. `ZM_FollowCamera::ClampArmDistance` is the authority; the
tests call it rather than restating it.

**What enforces it, and how far that reaches.**
`ZM_DawnmereCameraClearance_Test` runs the shipped camera maths against the real
committed Dawnmere physics world at an authoritative sample table -- **308
samples** as of ZM-D-173: the town-centre -> door-staging drive at 1 m, the
staging -> trigger approach at 0.25 m, both segments of the Home dirt path
`(512,512) -> (454,486) -> (384,456)` at 1 m, and 1.5 m rings around
`FromHomeSpawn` and TownCenter. **That table is the enforceable boundary. It does
NOT prove every mathematically standable point in Dawnmere**, and it carries no
rings around NPCs on purpose -- a live NPC can legitimately occupy the camera ray
and would make a static-layout guard nondeterministic. **A newly authored region
must add its own critical routes and actor-free interaction approaches to that
table as part of authoring it.**

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

## 5. Scenes and graphs (boot-authored; the five shipped scenes are COMMITTED)

> **★ ZM-D-181 RE-AUTHORED THREE OF THE FIVE.** The six greybox NPC entities in
> `Dawnmere.zscen` are now six HUMAN-MODEL entities, and the Player in all three of
> `Dawnmere` / `PlayerHome` / `ProfLab` gained a `ZM_GreyboxVisual` so it is drawn
> at all (it never had a visual before). **What moved in the bytes is narrow and
> deliberate: the transform SCALE, and one added component.** Positions did not
> move, collider steps did not move, the rival's yaw step did not move, and the
> step ORDER did not move -- which is what keeps ZM-D-148's dense authoring-order
> file indices stable.
>
> New SHA256 (both consecutive authoring boots wrote identical bytes):
> `Dawnmere` `6817534989B1A083...`, `PlayerHome` `DBBFB78311A55BBF...`,
> `ProfLab` `1BCAABC9EA4A6FC5...`. `Battle` and `FrontEnd` are untouched.
>
> **`Assets/Navmesh/Dawnmere.znavmesh` is UNCHANGED (`DCAA8403...`), and it must
> be.** Its bake is a flat coverage grid that never reads a collider
> (`Source/Nav/ZM_NavBake.cpp:29-38`), so a navmesh that moved would mean something
> other than the scale had changed.
>
> **The model is NOT authored into the scene, deliberately.**
> `Zenith_ModelComponent::WriteToDataStream` serializes its MATERIALS verbatim,
> which would make the committed bytes a function of a gitignored `.zmtrl` bake.
> The model is added at runtime by `ZM_GreyboxVisual`, exactly as the unit cube
> always was.


~40 .zscen (0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road,
40+ interiors, 95 Tower) are authored from ZM_WorldSpec via shared AddStep_*
helpers. `.bgraph` glue graphs use ZM_GraphAuthoring; **one exists** —
`game:Graphs/ZM_TrainerChallenge.bgraph`, written by the tools boot's
`AddStep_GraphBuild` and loaded at runtime by
`ZM_Interactable::EnsureTrainerChallengeGraph` (ZM-D-155). It is an ordinary
git-ignored bake. The remaining NPC-role dispatch is still C++.

**Scenes are the exception to the never-commit rule (section 0.1).** The five
authored so far — `FrontEnd`, `Battle`, `Dawnmere`, `PlayerHome`, `ProfLab` —
are TRACKED (ZM-D-147 / ZM-D-148 / ZM-D-174); scenes not yet authored will be
regenerated by tools builds as before. `FrontEnd.zscen` (build index 0)
authors the non-transient `ZM_GameStateRoot` with order-104
`ZM_GameStateManager` plus a full-screen black order-10000 `WarpFade` UIOverlay;
runtime `OnStart` makes that root persistent and retires duplicate managers.
It also authors `ZM_MenuRoot`. Its root menu entries were the four S6 ones
(Party, Bag, Dex, Exit); S7 item 2 SC4 (ZM-D-140) INSERTED **Save** and **Quit**
before Exit, so the shipped inventory is now the **six** of
`ZM_MENU_ROOT_ITEM` — Party, Bag, Dex, Save, Quit, Exit — plus the S7 SC5 title
controls (`Continue` / `New Game`) sharing the same persistent root. Still no
Box entry: ZM-D-165 RE-DEFERRED the Box screen from S7 to **S9**.
`Dawnmere.zscen` (build index 2) authors the
exact `TownCenter` and `FromHome` feet markers, scene-owned Player/camera, and
live home-door edge described above. `PlayerHome.zscen` (build index 40) is
terrain-independent and authored on every tools boot, including headless: a
collidable greybox shell, scene-owned Player/camera, `Door` feet marker at
`(0,0,3.5)`, and `PlayerHomeExitTrigger` at `(0,1,5.2)` targeting
**build 2 / `FromHome`**. ZM-D-176 also made that `Door` marker the **New Game
entry point** (`ZM_GameStateManager::uNEW_GAME_BUILD_INDEX` = 40,
`szNEW_GAME_SPAWN_TAG` = `"Door"`), and tinted the seven shell blocks warm —
see the note under `ProfLab.zscen` below. `ProfLab.zscen` (build index 41,
ZM-D-174) is likewise terrain-independent and authored on every tools boot: the
same seven-block greybox shell at 20 x 16 m (vs PlayerHome's 16 x 12 m),
`ProfLabDoorSpawn` tagged `Door`, a scene-owned Player and `ProfLabCamera`, all
read from `Source/World/ZM_ProfLabPlacement.h`. **It carries NO
`ZM_WarpTrigger` on purpose** — `ZM_WorldSpec` declares ProfLab -> Dawnmere via
tag `FromLab`, but `Dawnmere.zscen` authors no such marker, so an exit
configured today would pass `IsWarpDestinationValid` and then park the warp
machine in `WAITING_FOR_SPAWN` forever. The exit, the `FromLab` marker and the
Dawnmere-side Lab door land together in a later sub-commit. All five scenes are
COMMITTED (section 0.1); the real Dawnmere -> PlayerHome -> Dawnmere route is
covered by the S3 P1. The measured
Thornacre and Route1 terrain families do **not** imply that their scenes,
trees, dressing, traversal, or gameplay content exist.

**The PlayerHome tint is DERIVED AT RUNTIME, not baked and not serialized
(ZM-D-176).** `ZM_GreyboxVisual` (order 107) asks
`ZM_IsPlayerHomeBlockName(entityName)` at `OnStart` and substitutes
`ZM_GetPlayerHomeInteriorTintColour()` (0.61, 0.57, 0.43) for the shared
`ZM_GetHumanPaletteFallbackColour()` grey. No `.ztxtr`, `.zmtrl` or `.zscen`
byte encodes it — the seven blockout entities are byte-identical to their
untinted selves, which is why ZM-D-176 moved no `PlayerHome.zscen` byte.
`fZM_GREYBOX_FALLBACK_*` is untouched and still worn by ProfLab's seven blocks,
Dawnmere's blockout and every prop.

S6 extends `Dawnmere.zscen` with four greybox NPC entities:
`Npc_Villager`, `Npc_TradePostClerk`, `Npc_Caretaker`, and `Npc_Wanderer`.
(TWO MORE were added later by S7: `Npc_Warden`, the story-gated lane warden
(item 2 SC1 / ZM-D-137), and the trainer `Npc_RivalVesper` (item 3 SC8 /
ZM-D-156). The current inventory is the **six** rows of
`Source/World/ZM_DawnmerePlacement.cpp`'s anchor table, not four --
`ZM_DAWNMERE_NPC_ID` in the matching header enumerates six ids before
`ZM_DAWNMERE_NPC_COUNT`, and a `static_assert` on the deduced table bound holds
the two in lockstep.)
The Villager, Trade Post clerk, and Caretaker are stationary. The Wanderer is
a dynamic capsule with a deterministic two-point north/south patrol at
`x=540`, `z=476..484`; its authored component data carries the waypoints and
tuning, while runtime cursor/dwell state is not an asset. The patrol uses no
RNG, halts for its own active dialogue, and resumes afterward. S6 itself emitted
no `.bgraph` and no navmesh file — the shipped patrol is serialized scene data,
not a substitute graph/navmesh asset. Both landed afterwards: the committed
`Assets/Navmesh/Dawnmere.znavmesh` at ZM-D-147, and the single
`ZM_TrainerChallenge.bgraph` at ZM-D-155.

`ZM_Interactable` scene serialization is v2: patrol enablement, waypoint count,
points and tuning persist as authored configuration. A v1 component stream
loads as a stationary fallback; no patrol points or runtime cursor/dwell state
are invented during compatibility loading.

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
count 1,155 for the 384-chunk route in a 12-byte atomic marker; section 4.3).
The creature generator already stamps its generation version via
`uZM_CREATUREGEN_VERSION` (currently 3; section 1.2), the human family likewise
stamps `uZM_HUMANGEN_VERSION` (currently 1; section 2), and the building and prop
families stamp `uZM_BUILDINGGEN_VERSION` (currently 1) and `uZM_PROPGEN_VERSION`
(currently 1; section 3). The full per-family `ZM_BakeManifest` marker is now
**SHIPPED (ZM-D-085)**: a per-family 12-byte `ZMBM` stamp (ASCII magic + u32-LE
generator version + u32-LE expected-file count) at `game:<Family>/.manifest`,
written atomically after a successful `ZM_BakeAll*` and read fail-open by
`ZM_BakeManifestCheck` (a family is warm -- and its bake skipped -- iff the stamp
is current AND every enumerated file is present non-empty). It mirrors the terrain
`ZMTR` marker (section 4.3) and completes the bake-governance scheme; a tools-only
`ZM_BakeAllAssets()` bakes all four generator families under it.

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
