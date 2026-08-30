# Map Layout Playbook

**Purpose:** everything that bit us while re-laying-out Dawnmere, written so the
next layout change costs hours instead of days. Dawnmere has been re-laid-out
four times (v5 translate, v6 translate, v7 shrink + scenery, v8 compaction) and
**every one of them rediscovered something on this page the hard way.**

**Read this before** moving a building, an NPC, a lane, a pad, a spawn, or the
sheet dimensions of any Zenithmon map — and before authoring Route 1, Thornacre
or any new scene, because most of what follows is not Dawnmere-specific.

Sources: ZM-D-173, ZM-D-179, ZM-D-183, ZM-D-184, ZM-D-186, ZM-D-197, ZM-D-217,
ZM-D-218. This page is the digest; the DecisionLog entries are the argument.

---

## 1. The loop

A layout change is not one edit. It is a **fixed point** you iterate to, because
the compiled constants describe the terrain and the terrain is generated from the
compiled constants. Run it in this order; skipping a step silently poisons the
next one.

```
1.  Edit the recipe      Source/World/ZM_TerrainAuthoring.cpp  (+ .h if the
                         manifest version or the required-output count moves)
2.  Edit the placement   Source/World/ZM_DawnmerePlacement.{h,cpp}
3.  Edit the dressing    Source/World/ZM_DawnmereDressing.cpp  (keep-out, clumps,
                         scatter) -- the keep-out DERIVES from 1 and 2
4.  BUILD                Vulkan_vs2022_Debug_Win64_True     <-- check the exit code
5.  FORCE A RE-BAKE      zenithmon.exe --automated-test ZM_Boot_Test
                           --skip-unit-tests --zm-force-terrain-bake=Dawnmere
6.  RE-MEASURE the ground   the four oracles (section 3)
7.  PASTE the measurements  back into ZM_DawnmerePlacement.{h,cpp}
8.  BUILD again          <-- the pasted constants only take effect now
9.  AUTHOR TWICE         two consecutive boots; the second must log
                         [ScenePublish] IDENTICAL and the hashes must match
10. Null unit suite      Null_vs2022_Debug_Win64_True --exit-after-unit-tests
11. Automated batch      zenith test Zenithmon
12. Pin, docs, hashes    Tools/unit_baselines.json (ONLY if the count moved),
                         Status.md, DecisionLog.md
```

★ **Step 5 defers scene authoring.** The boot that forces a bake does not
publish scenes, so step 9 genuinely needs two more boots, not one.

★ **Steps 6-8 are not optional even when the tests pass.** The ground-truth
oracles have a tolerance; a row can drift inside it and still leave an authored
body penetrating the surface. See trap 3.10.

---

## 2. What re-derives what

The single most expensive class of mistake is changing one thing and not knowing
what it invalidated. This table is the dependency graph.

| If you change... | ...you MUST re-derive |
|---|---|
| Sheet dimensions (chunks / chunk size) | `uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT`, the manifest version, the erosion droplet count (it scales with AREA), the noise frequencies (they scale with the sheet), the preview camera, every landform/path/pad/dab coordinate, the navmesh polygon band in `ZM_Tests_NavEval.cpp` + `ZM_AutoTests_Navmesh.cpp` |
| Any landform, path, pad, or the noise | **all 23 measured ground columns** (section 3), and the terrain-recipe step-count pin in `ZM_Tests_TerrainAuthoring.cpp` |
| The town-centre spawn | rival Vesper's frozen facing bits (they are the bearing to it), every roster offset, the whiteout clearances, `fZM_DAWNMERE_TOWN_CENTER_FEET_Y` — which feeds the nav grid and therefore **re-bakes the committed navmesh** |
| Rival Vesper's XZ | his frozen facing bits, his shelf's centre, his sight-disc keep-out and therefore possibly a tree clump, the recorded neighbour clearances in `ZM_Tests_DawnmerePlacement.cpp` |
| Any NPC anchor | its measured feet height, the roster spread clause, the drive-leg clearances, the prop keep-out |
| A building shell or door | the Home/Lab ground tables, the camera-clearance clause, the pad that carries it |
| A pad or path radius | the grass dabs it erases (trap 3.7), the keep-out region, the props that can stand there |
| A drive leg (rows in `ZM_DawnmereDressing.cpp`, reached through the header) | nothing automatically — but see trap 3.5, because that table is now read by three different suites |

★ **A terrain edit and a placement edit are the same commit.** The scene bytes
and the heightfield are two halves of one artefact; landing one without the other
leaves a tracked asset describing a world that no longer exists.

---

## 3. The traps, indexed by symptom

### 3.1 "I flattened it and it is not flat"

**Symptom:** ground under an authored anchor is metres off the pad's target;
`ZM_DawnmereNpcGroundTruth_Test` reports a large spread across the roster.

**Cause:** `Flatten` is a RATE, not an assignment:
`h += (target - h) * w * strength * 0.35` **per dab**, and a "pad" issues exactly
TWO dabs (pre- and post-erosion). Even at the dab centre that converges 57.8%; at
half the radius, ~32%. **A pad has never flattened anything.** It looked fine for
several map versions only because the base noise happened to sit near the target
there.

**Fix:** add a **SetHeight shelf** to the landform table.
`SetHeight` is `h += (target - h) * min(1, w * strength * 2)` — the kernel that
actually assigns. Do **not** widen or strengthen the pad.

**Also documented at:** the `Zenith_TerrainBrushTool` enum in
`Zenith/Editor/TerrainEditor/Zenith_TerrainEditor.h`, which is the engine-side
home for this fact.

### 3.2 "The terrain is now perfectly flat and a test says that is wrong"

**Symptom:** every measured column reads the same value to 5 decimal places;
`ZM_DawnmereNpcGroundTruth_Test` clause (d) reds with *"the live terrain is FLAT
... six measured constants buy nothing"*, or
`LabGroundSamples_NoRowSilentlyRepeatsAnother` reds on tied rows.

**Cause:** the shelf saturated. `min(1, w * strength * 2)` hits 1 wherever
`w * strength >= 0.5`, and a fraction of 1 assigns the target outright.

**Fix:** **every shelf must be strength < 0.5.** At 0.42-0.45 the ground keeps its
roll and the anchors still land within the height band that gameplay needs.

★ **Both of those clauses were correct and were left alone.** An anti-vacuity
clause can be falsified by a change that is otherwise an improvement. Ask which of
the two is wrong before reaching for the tolerance.

### 3.3 "An armed trainer never spots the player"

**Symptom:** `ZM_RivalVesperAuthored_Test` reports *"the walk-up STALLED"*, with
`sightEnabled=true`, `state=WATCHING`, `facingAbsDot=1.00000`, exact placement,
and the player driven to within centimetres. Every clause the test prints is
green. The message blames obstacles; there are none.

**Cause (v8, and the expensive one):** `DriveTowardXZ` — the helper every walk in
these suites uses — is **camera-relative and quantised to eight directions**. It
holds W/A/S/D, never a steering angle, and the camera swings to follow the
player's own heading. **The player therefore walks a pursuit curve that cuts the
corner**, and the lateral offset grows with the length of the walk. The trainer's
facing is derived from the STRAIGHT line, so the two disagree by more than the
30-degree cone allows.

Measured on a 48.8 m diagonal approach — note that the dot **decays** as the
player closes, because they are arriving from the side they drifted to:

| gap | 24.0 m | 12.0 m | 10.3 m | 8.5 m | 5.0 m | 0.8 m |
|---|---|---|---|---|---|---|
| coneDot | 0.974 | 0.888 | **0.853** | **0.853** | 0.787 | 0.543 |

(the cone admits `>= 0.86603`; it is already outside before entering the 8 m
sight range.)

**★★ WHY THE CURVE IS SO WIDE: THE DEAD ZONE IS A DISTANCE, NOT AN ANGLE.** Each
axis independently compares its own remaining error against 0.08 **metres**, so
the moment BOTH axes are outside it the driver commits to a full 45-degree
diagonal however lopsided the two errors are. A target 40 m ahead and 0.5 m to
the side is 0.7 degrees off dead ahead — and is walked at 45 degrees off. The
player then closes the small axis in a fraction of a second, drops to the
single-key direction, drifts back out, and repeats. **The worst-case heading
error is 45 degrees, not the 22.5 you would get from a nearest-of-eight
chooser** — a unit asserting 22.5 was written and went red at 44.0.

**Fix:** place the trainer on a **fixed point of that driver** — due N/S/E/W of
the approach origin, or on an exact 45-degree diagonal. There the off-axis error
is zero (or exactly equal), so the driver holds one key (or two) all the way in,
both camera orientations agree, and the lateral offset never opens. Rival Vesper
is due west of the town-centre spawn for exactly this reason; the argument is
spelled out in `ZM_DawnmerePlacement.h`.

★ **THIS IS A COMPILED FACT NOW, NOT A PARAGRAPH.** `Tests/ZM_Tests_WalkDrive.cpp`
asserts it directly: `FixedPointsDriveAStraightLine` over every axis, every
diagonal and every resting camera orientation, and — the half that matters more —
`AnOrdinaryBearingIsNotAFixedPointAndTheErrorIsBounded` plus
`ATinyLateralErrorStillBuysAFullDiagonal`, which fail if the driver ever grows a
proportional or angular term. Without those, a placement rule that leans on the
fixed points would quietly become cargo cult the day somebody improved the
driver.

**Diagnose it in one run:** `RVLogSightGateBreakdown` (in
`Tests/ZM_AutoTests_RivalVesper.cpp`) prints the four FSM inputs, the cone
arithmetic (`coneDot`, `forward`) and the camera basis on every failing exit of
the approach. Read it before forming a hypothesis.

★ **`facingAbsDot` cannot see a wrong facing.** It compares the authored rotation
against `ZM_DawnmereVesperFacing()` — a value against a re-computation of itself.
It proves transcription, never correctness.

★ **THERE IS ONE DRIVER NOW** — `Tests/ZM_TestWalkDrive.{h,cpp}` — and every
suite calls it through a thin local wrapper that keeps its own name and its own
`Clear*Input()`. It used to be EIGHT byte-identical copies, which is why the
camera-basis correction at ZM-D-130/131 had to be applied three separate times
and this finding landed on seven suites nobody had looked at.

★ **THE CLEAR STAYS IN THE WRAPPER.** Every original opened with its own
`Clear*Input()`, and they are not interchangeable — one also releases RIGHT
SHIFT, two reset a walk-state struct's held-key mirror. The first draft of the
consolidation moved them out on the assumption that call sites cleared before
driving. They do not: last frame's keys stayed held and **six automated tests
went red in one batch**. If you add a caller, give it a clear.

### 3.4 "A traversal test times out naming a distance"

**Symptom:** a suite that never mentions NPCs or scenery dies at its frame cap
reporting how far the player got.

**Cause:** `DriveTowardXZ` has **no obstacle avoidance**. Anything with a
collider on the line — an NPC body, a boulder, a stump, a fallen log, a tree
trunk — wedges the walk, and the failure names the distance rather than the
blocker.

**Fix:** keep colliders off the drive legs. `ZM_DawnmereKeepOutClearance` derives
the refused region from the recipe's pads/paths, the placement header's
anchors/markers, and the drive-leg table (six legs as of v8).

### 3.5 "A keep-out is refusing ground for no reason" / "the villager fails a clearance clause"

**Cause:** a drive leg that no test actually drives. v8 briefly carried a
`townCentre->routeArrival` leg — the whole length of the route lane — inferred
from the map. `ZM_SeamRoundTrip_Test` does not walk north out of town: it
**warps** onto the departure scene's own spawn tag and drives ~12 m from there.
The invented leg sterilised an 84 m strip and failed the villager, who has stood
on the lane centreline since S6 and is on nothing anything drives.

**Rule:** **a leg belongs in that table only if some test DRIVES it. Read the
test, not the map.**

### 3.6 "A trainer is blind and the scatter clauses are all green"

**Cause:** **a tree is not subject to the prop keep-out.** The tree brush is a
TERRAIN tool (`AddStep_TerrainSetTreeBrush` + a `TreePaint` stroke) and knows
nothing about `ZM_DawnmereDressing`, so `ZM_ScatterDawnmerePropsStep`'s refusals
never reach it — and tree trunks carry colliders. A trunk anywhere on the line
makes `ZM_ProbeTrainerSightLine` report blocked and the trainer never reacts.

**Fix:** tree clump discs must clear the hard keep-out, which carries the
trainer's sight radius (`fZM_SIGHT_MAX_DISTANCE + 4`, spelled against the
constant so raising the sight range reds here).
`TreeClumps_AreEntirelyOutsideTheTownKeepOut` is the clause.

### 3.7 "Three encounter tests suddenly cannot find grass"

**Symptom:** *"no cardinal direction from the spawn reaches a grass tile"*.

**Cause:** a pad's grass-erase clears its **FLATTEN** radius, not its dirt
radius. Widening a pad silently mows a much bigger lawn than it paves.

**Fix:** move the grass dab clear of the pad's flatten radius and re-check the
cardinal probes (they log `hitDist` / `hitDensity`).

### 3.8 "The building will not come any closer"

**Cause:** a clause coupling you have not noticed. v7's Lab sat far out on a
diagonal because `LabDirtPath_ClearsTheShellByTheShippedCameraClamp` required
camera clearance *behind* the shell, which forced the lane to run past the
building and dragged the building away from everything else.

**Fix:** find the clause that is really constraining the position, and ask what
property it was protecting. Ending the lane at the **forecourt** removed the
coupling entirely; the clause's anti-vacuity half was **re-derived** (the lane
must reach the door and arrive from the -Z side) rather than deleted.

### 3.9 "A clause reds again and I have already lowered its floor once"

**Cause:** a floor that moves every time it reds is a ratchet, and after two
moves it asserts nothing. The Plaza verge clause went 32 -> 16 -> 8 of 64
bearings across two map versions.

**Fix:** on the second move, give the clause a **companion measured over the
whole map** whose answer does not depend on how many anchors happen to cross one
ring — for that clause, "foliage-only area >= 1% of the sheet". Keep the specific
one too; it names the place the mechanism was built for.

### 3.10 "An authored NPC drifts off his anchor before anything moves him"

**Symptom:** *"drifted 0.87 m off his authored XZ BEFORE he had ever been asked
to walk (tolerance 0.350)"*.

**Cause:** an authored **dynamic** capsule standing on unflattened ground slides.
A pad is not grading (3.1), so "he is inside the pad" is not the same as "he is on
level ground". A stale measured feet height does it too: the body is authored at
the old surface, penetrates the new one and is shoved out.

**Fix:** give every authored dynamic body a SetHeight shelf, and re-measure its
ground column after every heightfield change.

★ **Do not assume the drift explains a blindness in the same run.** Those look
like one defect and are two: the same run reported `watchFacingMinDot=1.00000`,
i.e. the facing never moved, because the walk-up installs a yaw lock. Two real
defects sharing one symptom line are not evidence for each other.

### 3.11 "The roster's feet heights span too much"

**Symptom:** `DawnmereNpcFeetHeights_SpreadProvesTheyAreNotOneSharedValue` reds
on the UPPER bound.

**Cause:** the spread has to stay under `fZM_SIGHT_MAX_VERTICAL` (2.0 m) — an NPC
whose ground is further than the trainer cone's vertical band from another's is
one the picker or the cone cannot reach. **The town shelf is therefore sized by
the sight cone, not by the square.** A first draft spanned 2.02 m.

**Fix:** centre the shelf on the town's real centroid and widen it so the
outermost anchors sit at a similar falloff — not merely so it contains the plaza.
The clause's LOWER bound (spread >= 0.05) still has to hold, so keep strength
< 0.5 (3.2). Those two bounds are the band the shelf lives in: level enough for
the vertical band, uneven enough to still be terrain.

### 3.12 "The rotation drifts / the scene file ping-pongs in git status"

**Cause:** a runtime-computed rotation. `std::atan2` and `sin`/`cos` differ by
1-2 ULP between MSVC's Debug and Release codegen, so a Release tools boot authors
a rotation 1 ULP off and a Debug boot puts it back, forever.

**Fix:** **freeze the quaternion as bit patterns** and author it through
`AddStep_SetTransformRotationQuat`, which performs no math.
`ZM_DawnmereVesperFacing()` is the worked example.

★ **Compute the new bits OFFLINE in float32 rather than reading them off a
boot.** The authoring step writes them verbatim, so the pre-save guard compares
the value against itself and a boot can only add a transcription risk.

★ Never re-derive a facing via `glm::eulerAngles(quat).y` — it collapses past 90
degrees off +Z and has already cost this repo a full debugging cycle. Use
`ZM_ForwardFromRotation`, which rotates the +Z basis vector. **Yaw 0 faces +Z**,
and the derivation is `atan2(dx, dz)` — **X first**.

### 3.13 "A guard says everything matches and the world is still wrong"

**Cause:** a guard comparing a value against a re-computation of itself. Both
`ZM_VerifyAuthoredRivalFacingStep` and `facingAbsDot` do this. They prove
transcription; they cannot prove correctness.

**Fix:** compare against something that does not move with the code — frozen bits
(3.12), the committed file bytes (`ZM_Tests_CommittedSceneBytes.cpp`), or a
measurement of the live world.

### 3.14 "A path bake asserts in one configuration and not the other"

**Cause:** a path leg length that lands exactly on a `ceilf(len / spacing)`
boundary. `/fp:fast` may substitute a reciprocal square root and flip the sample
count.

**Fix:** nudge the waypoint off the boundary. The Home path's mid-point is
deliberately not on a round number for this reason.

### 3.15 "A Z-gap clause is failing something that is obviously fine"

**Symptom:** a clause of the form `|npcZ - townCentreZ| > k` reds, or reports a
NEGATIVE clearance, for an NPC that is nowhere near anything.

**Cause:** the clause is a **proxy from a town whose corridors ran along lines of
constant Z**. None of Dawnmere's have since ZM-D-173. Three such proxies survived
into v8; they are wrong in both directions — one failed the warden at a 4 m Z gap
while he stands 18.0 m from the leg measured properly, another reported **-6.0 m**
for a rival 31.0 m clear, and all three would have passed an NPC standing ON a
diagonal.

**Fix:** measure perpendicular distance to the legs in the
`ZM_DawnmereDressing` drive-leg table, which is the one place they are described. Do not
relax the constant.

★ **This is the recurring shape of a stale test in this codebase**: a cheap proxy
that was conservative under an assumption nobody wrote down, still green for
years after the assumption died. When a clause reds on something obviously fine,
suspect the clause's premise before the geometry.

---

## 4. The four ground-truth oracles

They are **automated tests, not units**. Run them windowed with
`--skip-unit-tests` after every bake. Each logs `measured=` / `paste=` lines on
EVERY run, pass or fail, so a single run gives you all 23 numbers.

| Test | Columns | Paste into |
|---|---|---|
| `ZM_DawnmereNpcGroundTruth_Test` | 7 (the roster) | the W5 block in `ZM_DawnmerePlacement.h` |
| `ZM_DawnmereHomeGroundTruth_Test` | 10 | the Home block in `ZM_DawnmerePlacement.h` |
| `ZM_DawnmereLabGroundTruth_Test` | 10 | the SC-D lab table in `ZM_DawnmerePlacement.cpp` |
| `ZM_DawnmereRouteSeamGroundTruth_Test` | 2 | the route-seam table in `ZM_DawnmerePlacement.cpp` |

★ **They pass on a tolerance, so a green run is not proof the tables are
current.** Read the `tableError=` figures; anything non-zero should be pasted.

★ **Why these are compiled constants and not a live sample:** the committed
`.zscen` bytes must be reproducible from compiled constants (a terrain bake is
gitignored, so sampling it would make a tracked file depend on an untracked one),
and there is no terrain physics body at authoring time to sample anyway.

---

## 5. Reading the failures

The single biggest time sink in this work is a failure message that describes the
symptom's neighbourhood rather than its cause.

| It says | It may actually be |
|---|---|
| "the walk-up STALLED" | the cone bearing (3.3) — not an obstacle |
| "timed out at distance X" | a collider on the leg (3.4) |
| "the warp never fired" | a prop between the staging waypoint and the door |
| "the terrain is FLAT" | a saturated shelf (3.2) |
| "no cardinal direction reaches grass" | a pad's flatten radius (3.7) |
| "the rival drifted" | unflattened ground **or** a stale feet height (3.10) |
| a negative clearance | a dead Z-gap proxy (3.15) |

★ **Check WHICH SCENE a capture is of before reading terrain off it.** A
screenshot of "the north walk" showed a ravine and produced a plausible,
independently-true diagnosis and two changes that both measured well — and the
frame was **Route 1**, because `ZM_SeamRoundTrip_Test` warps out of town on its
second leg. The tell, read too late: the capture came back **pixel-identical
across two terrain edits and a forced re-bake**. Both changes were reverted rather
than kept with the justification rewritten. `[TerrainPhysics] context='...zscen'`
is printed on every warp — read it.

★ **A test's own failure ordering is not a priority ordering.** The rival test
prints the stall first and the drift eleven lines later; the drift was the
smaller of the two problems and the stall's message pointed at the wrong thing.
Read the whole block.

---

## 6. Process notes

* ★★ **A failed build does not stop you running a stale exe.** This cost a full
  diagnostic cycle: a build errored, the run went ahead on the previous binary,
  and the near-identical result (0.8227 vs 0.8230 m of drift) read as "the fix
  did nothing". **Gate the run on the build's exit code**, and if a result looks
  suspiciously unchanged, check the exe's timestamp.
* **Force one re-bake:** `--zm-force-terrain-bake=Dawnmere`. That boot then
  DEFERS scene authoring, so a second boot is needed to author.
* **Windowed automated runs need `--skip-unit-tests`** — the SaveData sandbox
  unit fails by design under the harness.
* **Screenshots:** `--screenshot <path> --screenshot-frame N`, written as 32-bit
  BGRA TGA. **N counts render frames from process start, including the ~500 spent
  authoring and baking**, so early numbers land on the front end. Dawnmere is up
  by roughly frame 650. A frame past the end of the run writes nothing at all,
  which is how you find the run's length.
* **Capture from the `_False` (tools-disabled) build**, run directly. `_True`
  shows the editor and shrinks the viewport to about a third of the window, and
  `_False` also proves the COMMITTED scene bytes render — it authors nothing.
* **Two-boot byte-stability proof:** hash `Dawnmere.zscen` and
  `Dawnmere.znavmesh`, boot, hash again. The boot should log
  `[ScenePublish] IDENTICAL`.
* **The pin has one home**, `Tools/unit_baselines.json`, and the gate asserts
  `ran == baseline` EXACTLY. Only bump it from an OBSERVED `Null_` run — a Vulkan
  exe reports a different number.
* **`ZM_InteriorTintPixels_Test` fails and is pre-existing** (interior lighting,
  the last `requiresGraphics` rot item). Registry 70: 69 pass. Do not chase it as
  a regression from a layout change.

---

## 7. Known shortfalls this page does not fix

* ~~`DriveTowardXZ` is duplicated eight times~~ **— CLOSED 2026-08-30.** It is
  `Tests/ZM_TestWalkDrive.{h,cpp}` now, with eight unit tests of its own, and the
  quantisation is a pure function that can be probed at every bearing without
  booting a scene. What is left is the eight thin wrappers, which exist only to
  keep each suite's local name and its own `Clear*Input()`.
* **The tree brush cannot see the game's keep-out.** The coupling is maintained by
  hand and enforced only by
  `TreeClumps_AreEntirelyOutsideTheTownKeepOut`.
* **Nothing scans the recipe for pads used as if they were shelves.** 3.1 is a
  reading rule, not a mechanical one.
