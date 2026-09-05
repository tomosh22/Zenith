# Humanoid Import

How an artist-authored humanoid `.glb` becomes a skinned, animated character in
this engine, and why every step of it is shaped the way it is.

> **The whole procedure, for a human:** drop a T-posed humanoid at
> `<root>/Assets/Meshes/Humans/<Name>/<Name>.glb` and boot. There is no sidecar
> to author, no declaration to get right, and no code to edit. Everything below
> is what happens next.

[`Tools/CLAUDE.md`](../Tools/CLAUDE.md) is the working guide for anyone editing this code; this page is
the reference it points at.

---

## 1. The shared rig

**There is exactly one humanoid skeleton in this engine:**
`Zenith/Assets/Meshes/StickFigure/StickFigure.zskel` — 51 bones, written by
`GenerateStickFigureAssets()`. StickFigure, Zenithmon's NPCs, Combat's
characters, RenderTest's player and every imported artist humanoid all reference
it, byte for byte, and all seventeen `StickFigure_*.zanim` clips drive all of
them.

### 1.1 It is T-POSED, and that is the load-bearing fact

`Zenith_SkeletonAsset` owns the inverse-bind matrices, so **one skeleton means
one rest pose**, and every mesh bound to it must be *modelled* in that pose.
Artist humanoids are authored T-posed, universally. So the rig is T-posed and the
**procedural** humans are the side that moved.

The rotation is one function, `Zenith_HumanArmBindRotation` in
`Zenith/AssetHandling/Zenith_HumanProportions.h`:

| bone | bind local rotation |
|---|---|
| `LeftUpperArm` | `Rz(-90°)` |
| `RightUpperArm` | `Rz(+90°)` |
| **every other bone in the rig** | identity |

Everything below the shoulder keeps the offsets the old arms-down rig had. A
child's offset is a model-space delta expressed in a frame that rotated with it,
so `R_parent⁻¹ · (child − parent)` gives back exactly the delta that was already
there — the elbow, wrist, hand and all thirty finger bones are untouched.

### 1.2 Why not an arms-down rest pose

Because reaching it from a T-posed source means **baking a 90° shoulder rotation
through the skin weights**, and no weighting makes that clean.

A vertex at lever `r` from the joint, inside a blend band of width `W`, shears by
about `r·(π/2)/W`. Holding edge stretch under 25% therefore needs **`W > 6r`** —
a blend band six times wider than its own distance from the joint, which cannot
exist. Measured on the real asset, with dual-quaternion skinning:

| region | edges | > 25% distorted |
|---|---|---|
| **shoulder** | 18,573 | **12.97%** |
| torso | 48,817 | 3.18% |
| arm, legs, head | 57,018 | ~0.02% |

Single edges stretched past **8×**, permanently, in the shipped mesh. It reads as
lumpy, padded shoulders. Dual quaternions do not rescue it; that table *is* the
dual-quaternion result.

> **Diagnosing this class of defect:** measure **per-edge length change**
> (`L_after / L_before` over the index buffer) and bucket the bad edges **by body
> region**. A rigid pose change preserves every edge exactly, so any deviation is
> distortion. Silhouette comparisons and cross-section measurements both reported
> "identical" while 13% of one region was shredded — they sampled *along* the
> limb and the damage was *across* the blend.

### 1.3 Why the clips did not have to change

`Flux_AnimationController` fills the output pose from the **bind** pose and then
`SampleFromClip` **overwrites** every channel a clip carries. A clip's local
rotation *replaces* the bind's rather than composing with it, so a T-posed
skeleton plays all seventeen clips to identical model-space poses. The T-pose
survives only in the inverse-bind matrices, where it belongs.

**The one dependency:** a bone a clip does *not* animate keeps its bind local
transform. The two `UpperArm`s are the only non-identity ones, so a clip that
omitted them would leave that arm sticking straight out for its whole duration.
All seventeen animate both, and `GenerateStickFigureAssets` asserts it at bake
time:

```
Zenith_Assert(xExport.pxClip->HasBoneChannel("LeftUpperArm") &&
              xExport.pxClip->HasBoneChannel("RightUpperArm"), ...)
```

The clips: `Idle Walk Run Attack1 Attack2 Attack3 Dodge Hit Death Aim Fire Reload
Jump Serve Forehand Backhand ReadyStance`.

### 1.4 The procedural humans

StickFigure (`Tools/Zenith_Tools_TestAssetExport.cpp`) and Zenithmon's ring-form
humans (`Games/Zenithmon/Source/Gen/ZM_HumanMesh.cpp`) are still **authored and
warped arms-down** — every ring table, sculpt and warp pass keys off absolute Y
and would have to be rewritten otherwise. The T-pose is applied afterwards as a
**rigid rotation of the arm vertex range about the shoulder**, which distorts
nothing at all.

Three things this gets wrong if you are not careful, each of which did:

1. **The normals must turn with the positions**, unless something downstream
   recomputes them. StickFigure runs `ComputeHumanSmoothNormals` after its
   rotation and so does not care; Zenithmon has no such pass, and stale normals
   were reported by `ZM_ValidateGenMesh` as *inward winding* (its test is
   `dot(cross(C-A,B-A), avg vertex normal) > 0`) — a true report of a real defect
   under a thoroughly misleading name.
2. **Zenithmon's canonical-warp pass must not be rotated.**
   `ZM_CanonicalHumanWarp()` builds a reference mesh through the same function
   with the warp switched off and measures it `ARMS_DOWN`. Rotating during that
   pass hands an arms-down landmark scan a T-posed body: the arm chain comes back
   meaningless and every human then ships with the wrong arm. It did — the wrist
   landed 2.5 units out on a 2.6-unit body. Guarded by
   `s_bZM_MeasuringCanonicalWarp`.
3. **Pivot on the bone — except in Zenithmon.** The rig puts the elbow at
   `shoulder + (−L, 0, 0)`; the mesh's elbow only lands there if both turn about
   the same point. Zenithmon is the exception: stature there is a **Y-only mesh
   scale** the shared rig does not carry, so a 1.1× human's mesh shoulder sits
   ~20 cm above the bone and the pivot must be the *mesh's* own shoulder.

The shoulder rings are the only hand-authoring the T-pose needed. The rotation
turns *height above the joint* into *distance inboard along the limb*, and *cx
offset from the arm column* into *vertical droop* — so an arms-down deltoid, which
reaches inboard and caps the shoulder from above, lands in the armpit with the top
of the shoulder bare. The rings now sit **on** the arm column and are lifted well
above the joint so they bury inside the torso (whose half-width there is about
0.235).

---

## 2. The proportion table

`Zenith/AssetHandling/Zenith_HumanProportions.{h,cpp}` — all-config, so both the
Tools exporters and Zenithmon's pure generation library read the same numbers.

Every value is a **fraction of total height**, measured from the sole (0.0) to the
crown (1.0). That is what makes the table a statement about *proportion* rather
than about one mesh: a 2.6-unit rig and a 0.98-unit artist import have the same
shoulder fraction and completely different shoulder Y.

**The arm is three LENGTHS, not three heights** — shoulder-relative distances
along the limb — which is exactly why the same table describes a T-posed import
and an arms-down loft.

Rig space: `fZENITH_HUMAN_RIG_SOLE_Y = -1.0404`, `fZENITH_HUMAN_RIG_HEIGHT = 2.6012`.

### `Zenith_HumanProportionsRealistic()` — what every human is built to

| field | value | source |
|---|---|---|
| `m_fAnkleFrac` | 0.0742 | measured: leg radius minimum, lower 25% |
| `m_fKneeFrac` | derived | midpoint of hip and ankle |
| `m_fHipFrac` | *legacy* | **PINNED** — clips write absolute `Root` keys |
| `m_fSpineFrac` | *legacy* | **PINNED** — Idle writes an absolute `Spine` key |
| `m_fShoulderFrac` | 0.7735 | measured: T-pose arm centreline |
| `m_fNeckFrac` | 0.8477 | measured: radius minimum, shoulder..head band |
| `m_fHeadFrac` | *legacy* | **PINNED** — keeps the skull rigid under the warp |
| `m_fShoulderHalfXFrac` | 0.1212 | where the arm tube meets the torso |
| `m_fHipHalfXFrac` | *legacy* | **PINNED** |
| `m_fShoulderToElbowFrac` | 0.13466 | shoulder..wrist midpoint |
| `m_fShoulderToWristFrac` | 0.26933 | arm radius minimum in [0.58, 0.82] of the limb |
| `m_fShoulderToFingertipFrac` | 0.37129 | extreme along the arm axis |

`Zenith_HumanProportionsLegacy()` re-expresses the rig's original twenty literals
exactly, and `LegacyProportionsReproduceShippedRig` pins that — so "did extracting
the table change the skeleton" is answered by a test rather than by reading.

`IsOrdered()` asserts the anchors ascend and the arm chain lengthens outward; a
table that fails it is not a body.

---

## 3. Where the import runs

Inside `Zenith_Engine::InitialiseAssets()` — **before Flux, before physics,
before the ECS**, wrapped in `#ifdef ZENITH_TOOLS` and skippable with
`--skip-tool-exports`. In order:

```
ExportAllMeshes()          Assimp walk (game tree, engine tree),
                           then ImportGlbsInDirectory (game tree, engine tree)
ExportAllTextures()
ExportDefaultFontAtlas()
GenerateTestAssets()       GenerateStickFigureAssets()      <-- writes the RIG
                           ExportBoundHumanModels()         <-- the human binder
                           GenerateProceduralTreeAssets()
                           GenerateProceduralRockAssets()
                           GenerateFallenTreeAssets()
                           GenerateBushAssets()
                           GenerateGrassAssets()
                           GenerateRenderTestAssets()
```

★ **THE ORDERING IS THE REASON THE BINDER IS A SEPARATE EXPORTER.** The generic
`.glb` walk runs inside `ExportAllMeshes`; the rig it would need to bind to is
written by `GenerateStickFigureAssets`, several phases later. On a cold tree the
rig **does not exist yet** when the glb importer runs. Hence a separate exporter,
called immediately after the rig is written, plus a routing rule that makes the
generic walk stand aside.

---

## 4. Routing: who owns a `.glb`

`ImportGlbsInDirectory` asks `Zenith_Tools_HumanModelExport::IsHumanoidSourcePath`
before touching any file. **Anything under `Meshes/Humans/` belongs to the human
binder** — separators are normalised to `/` first, because the walk hands over
native paths and a Windows backslash would match nothing.

`ExportBoundHumanModels()` then scans **both** asset roots:

```
<ENGINE_ASSETS_DIR>/Meshes/Humans/**/*.glb
<ZENITH_ROOT>/Games/<Project>/Assets/Meshes/Humans/**/*.glb
```

★ **BOTH ROOTS IS NOT OPTIONAL.** `ExportAllMeshes` walks the game tree as well as
the engine one, and the skip applies in whichever tree is being walked. Scanning
only the engine root would leave a game's own humanoid skipped by the generic
importer and picked up by nobody — present on disk, silently absent from the
build, with no error anywhere.

### Why the skip exists at all

Without it the generic path writes a **static** bundle first and the binder
overwrites it in the same boot — and if the binder then *fails*, an
existence-only check downstream picks up the static bundle and animates nothing.
That is the LampPost incident (`Games/Zenithmon/Tests/ZM_Tests_PropBake.cpp`):
`ZM_BakeProp` silently overwrote an imported model on the same paths in the same
boot, 6623 verts became 72, and the suite stayed green throughout.

This used to be a committed `.zbind` sidecar beside each humanoid. It was removed:
of its four keys, `skeleton` had become dead, `yaw` is now measured, `mirror` was
never measurable *and* never needed, and the arm-fraction overrides were unused
escape hatches. Saying "this directory is humanoids" once is strictly better than
saying "this file is a humanoid" once per file.

---

## 5. The pipeline, stage by stage

`Zenith_Tools_HumanModelExport::ExportBoundHumanModel(path)`. Every stage names
its `ExportResult::m_strFailureStage`, which is the diagnostic surface — branch on
it, and it is what a log line will tell you.

| # | stage | what it does | refuses when |
|---|---|---|---|
| 0 | *(none)* | a **missing source is not a failure** — the asset tree is gitignored, so a fresh clone legitimately has no art. Log and return. | — |
| 1 | `skeleton` | loads the shared `StickFigure.zskel` | rig absent or zero bones |
| 2 | `load` | `LoadGlbMesh` — meshopt decode | unparseable |
| — | *(correction)* | **winding**, see §5.1 | never; it is a fix |
| 3 | `normalise` | `DetectAndNormaliseIntoRigSpace`, see §5.2 | facing not measurable |
| 4 | `measure` | `Zenith_MeasureHumanLandmarks(T_POSE)` | degenerate scan |
| 5 | `armsanity` | `CheckArmChain`, see §5.4 | a mitten read as a wrist |
| 6 | `proportions` | `CheckLandmarksAgainstRig`, see §5.5 | a body the rig cannot describe |
| 7 | `solve` | `SolveHumanSkinWeights`, see §5.6 | any vertex unclaimed |
| 8 | `validate` | `ValidateBoundMesh`, see §5.7 | any invariant broken |
| 9 | `mesh` / `materials` / `model` | staged publication, see §6 | any write failing |

★ **NOTHING BETWEEN `LoadGlbMesh` AND PUBLICATION DEFORMS THE MESH**, except
normalisation's rigid rotate + uniform scale + translate. That is the entire point
of the T-posed rig, and it is what makes an imported character's shoulders look
like the artist's shoulders.

### 5.1 Winding

Zenith's outward normal is `cross(C-A, B-A)`; glTF winds the other way. An
inside-out mesh **is invisible to every geometric check** — no vertex moves,
normals come from the file and stay correct, so lighting is fine and nothing
errors. Backface culling keeps the *far* surface, so you see through the model to
its far side: on a prop, a subtle wrongness nobody names; on a **character**, it
reads as *facing backwards*, with anything attached behind them drawing in front.

That shipped once, and survived a screenshot pass, because the picture is of a
plausible person looking the wrong way rather than of anything broken.

Measured, not assumed: the **signed volume** of a closed mesh states its winding
directly, so a source that already winds the engine's way is left alone. The
reference is the shipped StickFigure, whose sum is negative under
`Σ dot(a, cross(b, c))`. Positive means glTF order, and every triangle's second
and third indices are swapped.

### 5.2 Orientation — measured, never declared

Two independent readings, both anatomical:

- **Which horizontal axis is left/right** — a T-posed humanoid's arm span is
  several times its front-to-back depth, so the **wider horizontal extent is the
  left/right axis**. There is no plausible humanoid for which it is not, and
  §5.5 refuses the bodies that could be marginal.
- **Which way along the other axis is forward** — the ankle sits at the **back**
  of the foot, so from the shin's own axis a foot reaches about **three times
  further forward than back**. `Zenith_MeasureHumanLandmarks` reports this as
  `m_fFacingSign`.

The mesh is centred on X and Z by the normalise, so the 180° correction is a
negation of both — a proper rotation, so it preserves winding and needs no index
flip, and it leaves the sole plane and the uniform scale exactly where they were.

★ **AN UNMEASURABLE FACING IS A REFUSAL, NOT A GUESS.** A coin flip here ships a
character backwards, and that is the one error a screenshot pass demonstrably does
not catch: at head-thumbnail size the back of a head reads as a face.

★ **IT IS NOT "the heel is the taller end".** That is true of a bare foot and
false of a trainer with a built-up toe box, and it gave *opposite* answers on the
two meshes here while looking equally confident on both.

★ **LEFT/RIGHT IS NOT A GEOMETRIC FACT, AND IS NO LONGER ASKED.** A symmetric
T-posed body gives geometry no way to tell its own left from its right — but the
knob was never needed either: the two choices produce visually identical
characters and differ only in which arm leads a one-handed clip. The engine
convention decides it.

`OrientationIsRecoveredFromAnyCardinalYaw` pins this from all four cardinal
starting yaws, including the two where the arm span starts on Z.

### 5.3 Normalisation

Rotate by the detected yaw, then **uniformly** scale and translate so the sole
lands on `fZENITH_HUMAN_RIG_SOLE_Y` and total height on
`fZENITH_HUMAN_RIG_HEIGHT`, centred on X and Z. Normals are rotated with the
positions.

The scale is uniform **always**: a per-axis fit would squash a body to the rig's
aspect ratio, which is precisely the deformation this whole exercise exists to
avoid.

### 5.4 Arm sanity

The arm is the measurement most likely to be wrong, so it is the one that gets
asserted. A landmark scan that catches a chunky mitten instead of a wrist reports
a hand half the length of the forearm, and **nothing downstream would notice** —
the model would simply be rigged with a comically long hand and every gate would
stay green.

| quantity | accepted range |
|---|---|
| hand length (wrist → fingertip), as a fraction of height | 0.10 – 0.15 |
| upper arm, against the legacy rig's | 0.5× – 1.5× |
| forearm, against the legacy rig's | 0.5× – 1.5× |

### 5.5 Does this body fit the rig?

`CheckLandmarksAgainstRig`, tolerance **4% of height** (about 7 cm on an adult).

**One shared skeleton means fixed joint positions**, and that is a real constraint
on what may be imported, not a formality. A mesh whose knee sits 15 cm from the
rig's knee bone will bend at its thigh no matter how good the weight solve is. So
the mismatch is caught here and reported **by name** — *"its shoulder is at 0.71
of height and the rig's is at 0.77"* — which is something a person can act on,
where a silently badly-rigged character is not.

Checked: ankle, knee, hip, shoulder, neck, and shoulder-to-wrist. **An anchor the
mesh does not have is skipped, not failed** — a leg that tapers to a point has no
ankle seam to find, and treating "not modelled" as "wrong proportions" would
refuse a body for something it never claimed.

> **This is the pipeline's real limit.** A materially different body — a child, a
> stylised long-limbed character — is refused rather than mis-rigged. Lifting it
> means running the proportion warp (`Zenith_HumanWarp`) on the import too, the
> way the procedural humans already are. The machinery exists, but it reads
> `ArmWeight` from *existing* skin weights, which an unrigged import does not have
> until after the solve — so wiring it means solve → warp → keep weights. **Not
> done.**

### 5.6 The weight solve

Distance to the bone **segment** with a normalised radial falloff, over 18 solve
bones (16 core + 2 toes), region-gated, solved on position-welded vertices,
smoothed, top-4, normalised, sorted descending.

| bone | child | radius |
|---|---|---|
| `Root` | `Spine` | 0.100 |
| `Spine` | `Neck` | 0.105 |
| `Neck` | `Head` | 0.055 |
| `Head` | — | 0.070 |
| `{Left,Right}UpperArm` | `…LowerArm` | 0.050 |
| `{Left,Right}LowerArm` | `…Hand` | 0.040 |
| `{Left,Right}Hand` | — | 0.045 |
| `{Left,Right}UpperLeg` | `…LowerLeg` | 0.075 |
| `{Left,Right}LowerLeg` | `…Foot` | 0.055 |
| `{Left,Right}Foot` | `…Toe` | 0.050 |
| `{Left,Right}Toe` | — | 0.040 |

Bones outside that set — jaw, eyes, and the thirty finger joints — are simply
never assigned weight by the solve; they still exist on the rig and still animate.

**A power law with a hard cut, never `1/dⁿ`.** An inverse power never reaches
zero, so a hand vertex keeps a trace of the head forever — tiny, normalised away
to near nothing, and still enough to make the fingers twitch when the head turns.

**A bone is a capsule, not a ball.** Clamping the segment projection would give
every bone a hemispherical cap of the *full* influence radius beyond each end —
for the upper arm, a 22 cm ball centred on the shoulder joint, reaching up into
the trapezius. The end-cap taper is what keeps the deltoid attached to the arm
without letting the arm own the shoulder cap.

**A region gate is a ramp, not an `if`.** The gates began as hard `continue`s
("an arm bone may not claim anything inboard of 0.75 of the shoulder half-width").
At that boundary a bone's weight does not fade in — it *jumps* from zero to
whatever the distance falloff happens to give, which near the deltoid is about
0.4. Two vertices 8 mm apart ended up with arm weights of 0.00 and 0.38. **The
overlap of two gates is not a blend if each gate is a step**; what makes the
result continuous is each gate's own edge being smooth.

**Normalise before smoothing.** Raw falloff values are peaky (a fourth power near
a hard cut), so averaging them leaves the *normalised* field — the one anything
downstream reads — still wobbling by ±0.1 between neighbours.

**Every smoothing iteration re-applies the gates.** Averaging with a neighbour on
the far side of a gate boundary walks weight straight back across it.

**No fallback bone.** A vertex no bone claimed is a hole in the gates or the
radii; pinning it to `Root` would hide that behind a patch of skin riding the
hips. The solve fails instead.

**No intermediate "fitted" rig.** Weights are bone *indices* that ship beside the
**shipped** rig's inverse-bind matrices, so solving against a rig fitted to the
mesh's own measurements and then binding to a different one deforms the mesh at
rest by exactly the difference between them. Solve against the rig the mesh will
actually be skinned by; §5.5 is what makes that safe.

Male's report: 27,084 verts over 23,567 unique positions, 18 solve bones,
**0 fallbacks**, weight sums `[1.000000, 1.000000]`, max bone index 20.

### 5.7 Validation

Everything a consumer would be entitled to assume, checked **before anything
reaches disk**:

- non-empty mesh and index buffer
- skinning arrays sized to the vertex count
- every vertex's weights sum to 1 ± 1e-4
- no bone index at or past the rig's bone count
- non-degenerate bounds on all three axes

"Does the file exist" is *not* a validity check. That is the mistake the LampPost
incident was made of.

---

## 6. Publication

The `.zmodel` is the **commit marker**, and the protocol matters:

1. It is deleted the moment a bind is *attempted* — not at publish time. Once
   there is a source to bind, whatever `.zmodel` is sitting there describes a
   previous run and a consumer cannot tell it apart from a fresh one. So every
   failure path leaves **no** marker rather than an old one.
2. The mesh is written to `.tmp` and renamed into place.
3. Textures and the `.zmtrl` go to their **final** names. The material file
   *embeds* texture paths, so staging them under temporary names would publish a
   material pointing at files that no longer exist. They are prerequisites, and
   the marker is what makes them invisible until they are all there.
4. The `.zmodel` is written **last**. Any abort removes it and both temporaries.

`std::filesystem::rename` replaces an existing file on Windows (it is
`MoveFileEx(..., MOVEFILE_REPLACE_EXISTING)` underneath), which is not what POSIX
rename semantics would lead you to expect.

### What lands on disk

For `Meshes/Humans/Male/Male.glb`:

```
Male.zmesh     27,084 verts, skinned, skeleton ref -> engine:Meshes/StickFigure/StickFigure.zskel
Male.zmodel    the marker; same skeleton ref, one mesh binding, the material refs
Male.zmtrl     the material
<4 textures>
```

★ **THE `engine:` PREFIX IS LOAD-BEARING.**
`Zenith_AssetRegistry::NormalizeAssetPath` converts an absolute path into a
prefixed one and leaves a bare *relative* path exactly as it found it — so a model
whose skeleton ref is `Meshes/StickFigure/StickFigure.zskel` loads, renders, and
reports **no skeleton**. The mesh still says `HasSkinning` (that only asks whether
the string is empty), the bundle passes every completeness check, and the
character stands in its bind pose while the animator logs "ModelComponent reports
no skeleton" at info level. Which is exactly what it did.

---

## 7. Consuming the result

`Games/RenderTest/RenderTest.cpp` shows the pattern:

```cpp
// Prefer the artist-authored human when it is there AND it is real.
if (!std::filesystem::exists(strMalePath)) { return; }
Zenith_ModelAsset* pxMale = ...;
if (pxMale == nullptr || pxMale->GetSkeletonPath().empty() || pxMale->GetNumMeshes() == 0u) { ... }
// ...and the mesh must actually HasSkinning().
```

★ **"IS THERE A FILE" IS NOT THE CHECK.** The binder writes its `.zmodel` last
precisely so a half-built bundle is invisible — but a **static** bundle (which the
generic importer would write if the routing rule ever broke) is a complete,
loadable, entirely unskinned model. Validating that the mesh actually
`HasSkinning()` is what tells those two apart, and it is the difference between a
player that animates and one that stands frozen in its bind pose while every test
stays green.

`--player-model=stickfigure` opts back out to the generated human.

---

## 8. Adding a humanoid

1. Author or obtain a **T-posed** humanoid, roughly adult proportions.
2. Save it as `<root>/Assets/Meshes/Humans/<Name>/<Name>.glb`, in either the
   engine tree or a game's. Orientation does not matter — it is measured.
3. Boot any `ZENITH_TOOLS` build.
4. Read the log. Success looks like:

```
HUMAN_BIND: orientation MEASURED - span x 0.1920 z 0.9662, facing sign -1.000, yaw 270.0 deg applied
HUMAN_BIND: normalised into rig space - yaw 90.0 deg, uniform scale 2.651861 ...
HUMAN_BIND: arm sanity - hand 0.1020 of height, upper arm 0.876x legacy, forearm 1.168x legacy
HUMAN_BIND: solved 27084 verts over 23567 unique positions, 18 solve bones; fallbacks 0, ...
HUMAN_BIND: published ...Male.zmodel - 27084 verts skinned to 51 bones on the SHARED rig (mesh UNDEFORMED), 4 texture(s)
```

5. Reference it as `engine:Meshes/Humans/<Name>/<Name>.zmodel` (or the game
   equivalent) and give the entity an animator pointed at the shared clips.

**If it refuses**, the failure stage names the reason. The two you are most likely
to hit are `proportions` (§5.5 — the body is not the shape this rig describes) and
`normalise` (the feet are not recognisable enough to orient from).

---

## 9. Verifying by eye

Structural tests cannot see "the shoulders look wrong" — that is what started
this. The photo-tour instruments frame the humans deterministically:

| command | shot |
|---|---|
| `rendertest --skip-unit-tests --automated-test RT_PhotoTour` | `rt_player_portrait`, `rt_player_face`, `rt_player_feet` (the imported male), `rt_tennis_courtside` (StickFigures) |
| `zenithmon --skip-unit-tests --automated-test ZM_PhotoTour_Test` | `dawnmere_npc_eye` (a Zenithmon human, close) |

Output lands under `Build/artifacts/<game>/phototour/run/` as BGRA TGA.

★ **A/B A NEW CHARACTER AGAINST THE REFERENCE HUMAN AT THE SAME CAMERA, CROPPED
TIGHT ON THE HEAD.** The check "do both humans show a face at this camera" is what
let a backwards character through: at head-thumbnail size the back of a head reads
as a face. Check where an attached prop lands too — the jetpack is authored on the
Spine's back.

---

## 10. Invariants

Break any of these and the failure is quiet:

1. **`GenerateStickFigureAssets()` runs before `ExportBoundHumanModels()`.** The
   rig must be on disk before anything binds to it.
2. **Every clip animates both `UpperArm`s.** Asserted at bake time.
3. **The mesh's rest pose matches the rig's.** One skeleton, one rest pose — a
   second rest pose means a second skeleton, which is the thing this design exists
   to avoid.
4. **`Meshes/Humans/` is the routing rule, in both asset roots.** The generic walk
   stands aside there and only the binder writes those bundles.
5. **The `.zmodel` is written last and deleted on every failure path.**
6. **Skeleton refs carry the `engine:` prefix.**
7. **A test must not locate a joint by height.** Four did (`|y - ElbowY()| < 0.09`)
   and every one broke the day the arm stopped hanging. Select on the bone's own
   model-space origin — correct in either pose, and it cannot go stale.
8. **Measure a T-posed mesh as `T_POSE`.** In that mode the arm chain is reported
   as *lateral reach*, not as heights.

---

## 11. Where the code and tests live

| file | what |
|---|---|
| `Zenith/AssetHandling/Zenith_HumanProportions.{h,cpp}` | the table, the T-pose bind rotation, the shoulder pivot |
| `Zenith/AssetHandling/Zenith_SkinDeform.{h,cpp}` | landmark measurement, the proportion warp |
| `Tools/Zenith_Tools_GlbImport.{h,cpp}` | `LoadGlbMesh`, `ExportGlbMaterials`, the generic walk, the winding fix |
| `Tools/Zenith_Tools_HumanSkinBind.{h,cpp}` | orientation, normalise, sanity, fit check, weight solve |
| `Tools/Zenith_Tools_HumanModelExport.{h,cpp}` | the call site, routing, publication |
| `Tools/Zenith_Tools_TestAssetExport.cpp` | the rig, the clips, StickFigure's geometry |
| `Games/Zenithmon/Source/Gen/ZM_HumanMesh.cpp` | Zenithmon's ring-form humans |

Tests: `Zenith_SkinDeform.Tests.inl`, `Zenith_Tools_HumanSkinBind.Tests.inl`,
`Zenith_Tools_HumanModelExport.Tests.inl`,
`Zenith_Tools_TestAssetExport.Tests.inl`, `Zenith/Core/Zenith_UnitTests.Tests.inl`
(`StickFigureMeshJointAlignment`), `Games/Zenithmon/Tests/ZM_Tests_HumanGen.cpp`.

Pinned unit baselines live in `Tools/unit_baselines.json` and nowhere else.
