# Tools

The **asset bake**. Everything here turns source art, declarations and procedural
recipes into the `.zmesh` / `.zmodel` / `.zskel` / `.zanim` / `.ztex` / `.zmtrl`
files the runtime loads.

★ **THIS IS A BOOT PHASE, NOT A SEPARATE PROGRAM.** Every exporter runs inside
`Zenith_Engine::InitialiseAssets()`, wrapped in `#ifdef ZENITH_TOOLS` (a `_True`
configuration), **before Flux, before physics, before the ECS**. A `_False` build
compiles none of it and loads whatever is already on disk.
`--skip-tool-exports` skips the phase.

Because it runs pre-Flux there is **no device, no render graph and no scene**
here. An exporter that needs one is in the wrong place.

---

## The boot order, and why it is load-bearing

```
Zenith_Engine::InitialiseAssets()
  ExportAllMeshes()          Zenith_Tools_MeshExport
                               ExportMeshesInDirectory(game), then (engine)   -- Assimp
                               ImportGlbsInDirectory(game),   then (engine)   -- .glb
  ExportAllTextures()        Zenith_Tools_TextureExport
  ExportDefaultFontAtlas()   Zenith_Tools_FontExport
  GenerateTestAssets()       Zenith_Tools_TestAssetExport
      GenerateStickFigureAssets()        <-- writes the ONE humanoid rig + 17 clips
      ExportBoundHumanModels()           <-- binds artist humanoids to it
      GenerateProceduralTreeAssets()
      GenerateProceduralRockAssets()
      GenerateFallenTreeAssets()
      GenerateBushAssets()
      GenerateGrassAssets()
      GenerateRenderTestAssets()
```

★ **`ExportBoundHumanModels()` MUST FOLLOW `GenerateStickFigureAssets()`,** and
that ordering is the whole reason the human binder is not inside the `.glb`
importer. It skins an artist's humanoid to `StickFigure.zskel`, and on a cold tree
**that file does not exist yet** when `ImportGlbsInDirectory` runs several phases
earlier. Move either call and a fresh clone fails to bind, with a log line
blaming a missing rig.

★ **The `.glb` walk runs AFTER the Assimp walk** so that where a model somehow has
both sources, the `.glb` bundle is what survives.

---

## What each file does

| file | emits |
|---|---|
| `Zenith_Tools_MeshExport` | `ExportAllMeshes()` — the two directory walks above |
| `Zenith_Tools_AssimpConvert` | Assimp scene → `Zenith_MeshAsset` / `Zenith_SkeletonAsset`. **Requires Assimp headers included before it** |
| `Zenith_Tools_GlbImport` | binary glTF: `LoadGlbMesh`, `ExportGlbMaterials`, `ImportGlbsInDirectory` |
| `Zenith_Tools_MeshoptDecode` | the vertex/index codecs and filters for `EXT_meshopt_compression` (what gltfpack emits) |
| `Zenith_Tools_GltfExport` | the other direction — Zenith assets out to glTF 2.0 |
| `Zenith_Tools_TextureExport` | `ExportAllTextures()`, driven by a committed `TextureUsage.ztexdecl` |
| `Zenith_Tools_FontExport` | `ExportDefaultFontAtlas()` — MSDF atlas from a TTF |
| `Zenith_Tools_TerrainExport` | heightmap → terrain chunk geometry |
| `Zenith_Tools_TestAssetExport` | the humanoid rig, 17 clips, StickFigure's body, RenderTest's assets |
| `Zenith_Tools_HumanSkinBind` | orientation, normalise, sanity, fit check, weight solve |
| `Zenith_Tools_HumanModelExport` | the human binder's call site, routing and publication |
| `Zenith_Tools_TreeAssetExport` `…RockAssetExport` `…BushAssetExport` `…FallenTreeAssetExport` `…GrassAssetExport` | procedural prop/foliage sets: mesh, textures, material |

---

## Conventions every exporter here follows

**Staged publication with a commit marker.** Write to `.tmp`, rename into place,
and write the file a consumer keys off **last**. Delete that marker the moment a
rebake is *attempted*, so no failure path can leave a stale bundle that a
consumer cannot distinguish from a fresh one.

**"Does the file exist" is never a validity check.** That is the mistake the
LampPost incident was made of: `ZM_BakeProp` silently overwrote an imported model
on the same paths in the same boot, 6623 verts became 72, and the suite stayed
green throughout. Validate the *property you actually need* — `HasSkinning()`, a
vertex count, a bone set.

**A missing source is not a failure.** `**/Assets/**` is gitignored, so a fresh
clone (notably CI) legitimately has no art. Log, delete nothing, return.
`recursive_directory_iterator` **throws** on a missing path, and an unhandled
throw here kills the tools boot before any export runs.

**Refuse rather than guess.** Every one of these bakes feeds something no later
gate can see. When a measurement is ambiguous, fail with a message naming the
value and the bound — not a default.

**Declare what cannot be inferred, in a committed file beside the art.**
`TextureUsage.ztexdecl` is the pattern: colour space and compression are stated,
not guessed from a filename, and the declaration is re-included in `.gitignore`
so it survives a fresh clone even though the art it names does not. Parsing is
**strict** — an unknown key, a missing version or an unknown version fails the
file. A tolerant parser turns a typo into a silently different asset.

**No device, no scene.** See the top of this file.

---

## The humanoid pipeline

> Full reference, including the derivations and the failure history:
> **[Docs/HumanoidImport.md](../Docs/HumanoidImport.md)**. Everything below is
> what you need in order to *change* this code safely.

### One rig, and it is T-posed

`Zenith/Assets/Meshes/StickFigure/StickFigure.zskel` — **51 bones**, the only
humanoid skeleton in the engine. StickFigure, Zenithmon's NPCs, Combat, RenderTest
and every imported artist humanoid reference it byte for byte, and all 17
`StickFigure_*.zanim` clips drive all of them.

`Zenith_SkeletonAsset` owns the inverse-bind matrices, so **one skeleton means one
rest pose**: every mesh bound to it must be *modelled* in that pose. Artist
humanoids are T-posed, so the rig is. The entire rotation is:

| bone | bind local rotation |
|---|---|
| `LeftUpperArm` | `Rz(-90°)` |
| `RightUpperArm` | `Rz(+90°)` |
| everything else | identity |

`Zenith_HumanArmBindRotation` (in the all-config
`Zenith/AssetHandling/Zenith_HumanProportions.h`) is the single source of that,
read by the rig builder and both mesh generators so they cannot drift.

**Why not arms-down:** reaching it from a T-posed source bakes a 90° shoulder
rotation through the skin weights. A vertex at lever `r` in a blend band of width
`W` shears by `r·(π/2)/W`, so holding stretch under 25% needs **`W > 6r`** — a
band six times wider than its own distance from the joint. Measured, with dual
quaternions: **13% of shoulder edges past 25%, single edges past 8×**, permanently.
It reads as lumpy padded shoulders. Do not reintroduce this.

**Why the clips did not change:** `Flux_AnimationController` fills the pose from
the bind and `SampleFromClip` *overwrites* every channel a clip carries — a clip's
local rotation **replaces** the bind's. The T-pose survives only in the
inverse-bind matrices.

★ **THE ONE DEPENDENCY: EVERY CLIP MUST ANIMATE BOTH `UpperArm`s.** A bone a clip
omits keeps its bind local transform, and those two are the only non-identity
ones — so an omission leaves that arm sticking straight out for the clip's whole
duration. `GenerateStickFigureAssets` asserts it at bake time. **Adding a clip
means adding both channels.**

### The procedural humans are still authored arms-down

StickFigure (`Zenith_Tools_TestAssetExport.cpp`) and Zenithmon's ring humans
(`Games/Zenithmon/Source/Gen/ZM_HumanMesh.cpp`) keep their arms-down ring tables,
sculpts and proportion warp — all keyed off absolute Y. The T-pose is a **rigid
rotation of the arm vertex range about the shoulder**, applied afterwards, which
distorts nothing.

Three ways to get that wrong, all of which happened:

1. **Rotate the normals too**, unless something downstream recomputes them.
   StickFigure runs `ComputeHumanSmoothNormals` after and does not care; Zenithmon
   has no such pass, and stale normals were reported by `ZM_ValidateGenMesh` as
   *"inward winding"* — a true defect under a misleading name (its test is
   `dot(cross(C-A,B-A), avg vertex normal) > 0`).
2. **Do not rotate during Zenithmon's canonical-warp pass.**
   `ZM_CanonicalHumanWarp()` builds a reference mesh through the same function
   with the warp off and measures it `ARMS_DOWN`. Rotating there hands an
   arms-down scan a T-posed body; the warp built from it is wrong and every human
   ships with the wrong arm. It did — the wrist landed 2.5 units out on a
   2.6-unit body. Guarded by `s_bZM_MeasuringCanonicalWarp`.
3. **Pivot on the bone — except in Zenithmon**, where stature is a **Y-only mesh
   scale** the shared rig does not carry, so the pivot must be the *mesh's* own
   shoulder or the arm tears off the body.

The rotation turns *height above the joint* into *distance inboard along the
limb*, and *cx offset from the arm column* into *vertical droop* — which is why
the shoulder rings sit on the arm column and are lifted well above the joint, so
they bury inside the torso instead of landing in the armpit.

### The import, stage by stage

`Zenith_Tools_HumanModelExport::ExportBoundHumanModel(path)`. Each stage sets
`ExportResult::m_strFailureStage`:

| stage | does | refuses when |
|---|---|---|
| `skeleton` | loads the shared rig | absent or zero bones |
| `load` | `LoadGlbMesh` (meshopt decode, **no tangents yet**) | unparseable |
| *(fix)* | **winding**: signed volume measured; positive ⇒ reverse every triangle | — |
| `normalise` | orientation **measured**, then uniform scale + translate onto the rig's sole and height, centred on X/Z | facing not measurable |
| `measure` | `Zenith_MeasureHumanLandmarks(T_POSE)` | degenerate |
| `armsanity` | hand 0.10–0.15 of height; upper arm and forearm each 0.5–1.5× legacy | a mitten read as a wrist |
| `proportions` | `CheckLandmarksAgainstRig`, **4% of height** | a body the rig cannot describe |
| `solve` | 18 solve bones, capsule distance, smooth region gates | any vertex unclaimed |
| `validate` | weights sum to 1, indices in range, non-degenerate bounds | any of those |
| `mesh`/`materials`/`model` | staged publication | any write failing |

★ **NOTHING DEFORMS THE MESH** between load and publication except
normalisation's rigid rotate + uniform scale + translate.

**Orientation is measured, never declared.** The wider horizontal extent is the
arm-span axis; the foot/shin lever gives the sign (an ankle sits at the *back* of
a foot, which reaches ~3× further forward than back). An unmeasurable facing is a
**refusal** — a coin flip ships a character backwards, and that is the one error a
screenshot pass demonstrably does not catch, because at head-thumbnail size the
back of a head reads as a face. Left/right is not asked: a symmetric T-posed body
cannot answer it, and the two choices are visually identical.

**Routing is a directory.** `IsHumanoidSourcePath` — anything under
`Meshes/Humans/` belongs to the binder, and `ImportGlbsInDirectory` stands aside.
`ExportBoundHumanModels` scans **both** asset roots, because the generic walk
skips in both and scanning one would leave a game's humanoid picked up by nobody.

**The skeleton ref carries `engine:`.** `NormalizeAssetPath` leaves a bare
*relative* path alone, so a ref without the prefix loads, renders, and reports
**no skeleton** — while `HasSkinning` still returns true and every completeness
check passes.

### Things that will bite you

★ **A TEST MUST NOT LOCATE A JOINT BY HEIGHT.** Four did
(`|y - ElbowY()| < 0.09`) and every one broke the day the arm stopped hanging —
a T-posed elbow is a distance *out along X* at shoulder height, so a height band
finds the ribcage. Select on the **bone's own model-space origin**: correct in
either pose, and it cannot go stale.

★ **MEASURE A T-POSED MESH AS `T_POSE`.** In that mode the arm chain is reported
as **lateral reach**, not as heights.

★ **LOCATE A LOFT'S JOINTS FROM ITS SKIN WEIGHTS, NOT ITS SHAPE.** The ring whose
weights are 50/50 across a joint *is* that joint, by the author's own statement.
Three radius heuristics failed first. The T-pose scan uses crossings when the mesh
has weights and only falls back to a radius sweep for an unrigged import — that
sweep put StickFigure's elbow 11 cm and wrist 9 cm off their bones.

★ **THE PIPELINE'S REAL LIMIT IS `CheckLandmarksAgainstRig`.** A shared rig has
fixed joints, so a body it does not describe is refused *by name* rather than
mis-rigged. Lifting that means warping the import onto the rig's proportions the
way the procedural humans already are — `Zenith_HumanWarp` exists but reads
`ArmWeight` from *existing* weights, which an unrigged import lacks until after
the solve. **Not done.**

---

## Verifying a change here

Structural tests cannot see "the shoulders look wrong". Use the photo-tour
instruments, which frame the humans deterministically:

```bash
rendertest.exe --skip-unit-tests --automated-test RT_PhotoTour
```
```bash
zenithmon.exe --skip-unit-tests --automated-test ZM_PhotoTour_Test
```

Output: `Build/artifacts/<game>/phototour/run/*.tga` (BGRA). `rt_player_portrait`
is the imported male, `rt_tennis_courtside` the StickFigures, `dawnmere_npc_eye` a
Zenithmon human. **A/B against the reference human at the same camera, cropped
tight on the head**, and check where an attached prop lands — the jetpack is
authored on the Spine's back.

Unit suites (a `Null_` build; pins live in `Tools/unit_baselines.json` and nowhere
else):

```bash
zenith test Zenithmon
```

A change in this directory moves **every** game's baseline, because they all boot
the same exporters.
