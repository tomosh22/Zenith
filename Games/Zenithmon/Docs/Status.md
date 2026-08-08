# Zenithmon Status

**Last updated:** 2026-08-08

**★ LIVE BASELINE (OBSERVED 2026-08-08 on clean `Null_` builds):
ZM boot `3056 ran / 3054 passed / 0 failed / 2 skipped`; engine boot (Null Combat)
`1431 ran / 1430 passed / 0 failed / 1 skipped`; registry **55**.** This is the
current pin in `zm-tests.yml` (`-Baseline 3056`) and `run_unit_gate.ps1`
(default 1431). The move from 3049/1424 is **+7 ENGINE units** from the
compressed-vertex Phase 4 T4.a adversarial-review fix pass (2026-08-08): the
half codec gained hand-derived word pins and a frozen transcription of the
GPU-side Slang half codec (the licensed rounding-tie divergence is now pinned as
numbers), the fast-math-safe positive-finite predicate got its branch proofs,
over-unity bone-weight sums pre-scale proportionally instead of zeroing the
dominant influence, HALF4 positions beyond ±65504 assert at pack time, the
skin-output encoder is byte-compared against the static packer (defaults
included), and a negative-determinant (mirrored) bone blend now flips the
packed bitangent sign exactly as the uncompressed path did implicitly. Before
that, the move from 3044/1419 was **+5 NET ENGINE units** from the
compressed-vertex work's Phase 4 T4.a (2026-08-08): the mesh compression flip —
the static-mesh vertex went 72 -> 24 bytes (half4 position, half2 UV, snorm10
normal, snorm10 tangent whose w carries the bitangent SIGN, unorm8x4 colour; the
BINORMAL attribute is deleted and every consumer rebuilds it) and the
compute-skinning input 104 -> 32. The two mesh-family memcmp goldens now compare
against an INDEPENDENT transcription through `Flux_VertexCodec` instead of the
frozen float32 interleave loops (the flip changes those bytes by design), joined
by the skin-input/static prefix identity, the mirrored-frame bitangent sign, the
procedural mesh-pipeline pack, the packed skin-vertex codec round trip and the
one-byte bone sentinel. ZM's own registrations did not move. Before that, the move
from 3040/1415 was **+4 NET ENGINE units** from the
compressed-vertex work's Phase 3 T3.b (2026-08-08): the four hand-written
mesh-vertex interleave loops were replaced by two single writers — the packer for
the static stream (shaped by the shader-reflected table) and a named
builder for the skin-input stream — and the mesh-family suite was
rewritten from 3 hand-layout tests to 7, whose core is three byte-for-byte memcmp
goldens against a FROZEN transcription of the loop each writer replaced (the two
mesh-asset streams over 7 adversarial meshes, plus the procedural geometry
builder's dynamic layout with and without its bone tail). ZM's own registrations did not move. Before that, the move
from 3025/1400 was **+15 ENGINE units** from the
compressed-vertex work's Phase 3 T3.a (2026-08-08): `Flux_PackVertices`, the
reflection-driven replacement for the engine's hand-written vertex-interleave
loops, arrives with the suite that pins its whole-layout byte contract, the
canonical attribute defaults, `(semantic, index)` source keying, the 4-lane
TANGENT's derived handedness, the SNORM10 pre-normalisation and its zero-length
fallback, the lane pad/truncate rules, the POSITION quant box, and the no-op
shapes. ZM's own registrations did not move. Before that, the move from
3013/1388 was **+12 ENGINE units** from the
compressed-vertex work's Phase 2 T2.d review hardening (2026-08-08): the
vertex-layout-validation suite drives the boot tripwire's pure comparator
through every branch real boots never take (the OK path, both no-input
spellings, and the COUNT / STRIDE / ELEMENT / SEMANTIC mismatch categories).
ZM's own registrations did not move. Before that, the move from 3002/1377 was
**+11 ENGINE units** from the
compressed-vertex work's Phase 2 T2.b (2026-08-07): every generated
`Shaders/Generated/<Subsystem>.h` now carries a baked `kaxVertexAttribs[]` +
`kVertexLayout` per program (the empty spelling included), typed in the new
`Flux/Flux_VertexLayoutDesc.h` leaf, and the three terrain shaders spell their
packed normal/tangent as `[VtxFmt("snorm10_10_10_2")]` so their emitted table
reads exactly `Core/Zenith_TerrainChunkLayout.h`'s 28-byte stride. Its units pin
element-wise layout equality (drift in offset / type / semantic index / stride /
count), the canonical empty layout, the semantic vocabulary, and the codegen
emission itself — including the HARD failure on a semantic the vocabulary does
not name. ZM's own registrations did not move. Before that, the move from
2992/1367 was **+10 ENGINE units** from the
compressed-vertex work's Phase 2 T2.a (2026-08-07): the `.spv.refl` sidecar went
to **v6** with a per-program vertex-input table (name / semantic + index /
location / storage `ShaderDataType` / binding / byte offset) plus the two
per-binding strides, extracted from the linked program's VERTEX entry point. Its
suite pins the tight-pack offset/stride rule, the independent per-binding
packing, the empty table, the v6 DataStream round-trip, the adopt-once stage
merge, and the pure `[VtxFmt]` vocabulary / type inference / override validation.
ZM's own registrations did not move. Before that, the move from 2977/1352 was
**+15 ENGINE units** from the
compressed-vertex work's Phase 1 (2026-08-07): `Zenith/Flux/Flux_VertexCodec.h`
became the single home of every packed vertex attribute, and its suite pins the
half / snorm16 / unorm16 / unorm8 / uint8 round-trips and lane order, the
SNORM10_10_10_2 bit-identity against the terrain packer it adopted (the packer
had been duplicated between the exporter and the terrain editor), the position
quantisation box, and the bone weight/index lanes. ZM's own registrations did
not move. Before that, the move from 2975/1350 was **+2 ENGINE units** from the
engine-wide debug-variable audit remediation (2026-08-07): the `AI/*` debug
variables had never had a call site for `Zenith_AIDebugVariables::Initialise()`,
so none of the 20 toggles reached the panel; wiring them up added two routing
pins (master-toggle short-circuit, and safety against an un-`Initialise()`d
`Zenith_SquadManager`, whose visualiser used to assert and is now called every
frame by the engine). ZM's own registrations did not move. Before that, the
GPU-grass overhaul moved engine units in seven steps: Phase 7
+6 (types-authoring mapping/isolation/automation pins), Phase 6
+4 (displacement re-anchor/decay pins), Phase 5
+3 (shadow-casting truth table / slot-mask / registration-order pins), Phase 1
+20 (`FluxGrassTypes` pure suite), Phase 2 +2 (`FluxBufferReadback` zero-fill
contract), Phase 3 +4 (TerrainEditor GrassType map), Phase 4 (THE SWAP) +11 net
(3 legacy member-poking grass tests deleted, 14 added across shader mirrors /
impl surface / type-table serialization). ZM's own registrations never moved.
★ ZM's grass/battle suites were migrated to the new engine API in the swap
(`GetScheduledInstanceCount` exact-restore contracts held on first windowed run);
`ZM_DawnmereNpcGroundTruth_Test` was PRE-EXISTING red from ZM-D-184's spawn
clearance (all its inputs last touched `e2b074bf`) — never a grass regression,
and **FIXED 2026-08-07 by ZM-D-185**: the oracle demanded exact ground contact
from `Npc_RivalVesper`, i.e. the very defect ZM-D-184 fixed, so its
committed-bytes clause now expects `terrain + halfExtent + clearance` and reads
the clearance back out of the placement accessors. **Fixing it exposed a second,
independent staleness — ZM-D-186:** with the assertion finally honest, the W5
per-NPC feet table was found still holding ZM-D-182's pre-4 m-quad values
(`Npc_Warden` 98.8 mm off, two thirds of the 0.150 tolerance) because ZM-D-182
re-measured the Home table in the same file and not that one. All seven rows
re-measured and `Dawnmere.zscen` re-authored → SHA256 `3CAB927F…` (identical
across two windowed Debug boots); **`Dawnmere.znavmesh` did NOT move**, since 4
divides both 512 and 480 and the town centre is a shared vertex of both meshes.
**+20 engine units in one commit** (previous pins 2925 / 1300),
all in `Zenith/Flux/Vegetation/Flux_Grass.Tests.inl`: the `FluxGrassTypes` suite
covering the GPU-grass overhaul's Phase-1 pure functions (`Flux_GrassTypes.h`
tile selection / wind / map sampling / GPU-record packing) plus pinned
integer-hash vectors the new `Shaders/Common/Noise.slang` GPU mirror is held to.
(The prior +11 — terrain pipeline-variant 2x2 + `FluxTerrainSourceGrid` — is
retained in `zm-tests.yml`'s history block and `TestPlan.md`'s derivation.)

> **★ THE ZM PIN WAS ALREADY STALE BY 5 WHEN THIS LANDED, AND THAT IS THE THIRD
> TIME.** ZM sat at `-Baseline 2909` while HEAD really ran 2914: the 2026-08-05
> CommandLine ParseArgs units moved `run_unit_gate.ps1` 1284 -> 1289 and did not
> move `zm-tests.yml`, so `zm-tests` was RED on master before this work started —
> exactly the failure the block further down this file warns about. **ENGINE units
> land in EVERY game's boot count.** Adding one under `Zenith/` obliges you to move
> all four pinned sites in the same commit, from OBSERVED lines.

**★ ZM-D-183 (2026-08-04, UNCOMMITTED) -- THE RIVAL'S FACING IS NOW A FROZEN BIT
PATTERN, AND THE `Dawnmere.zscen` TRIPWIRE IS LIVE AGAIN.** `Dawnmere.zscen` was
coming back modified after every **Release** tools boot -- always the same 2 bytes,
`Npc_RivalVesper`'s rotation y and w -- because `ZM_DawnmereVesperFacing()` was a
runtime `std::atan2` + `glm::angleAxis`, and MSVC Debug and Release codegen disagree
on those by 1-2 ULP. A Release boot dirtied the file, a Debug boot restored it,
forever. **Both existing guards were structurally blind** (the bit-exact one compares
against a re-computation of itself IN THE SAME BINARY and only runs on the windowed
authoring boot CI never performs; the headless one uses `|dot| >= 0.999` against a
`1e-14` drift). Fixed by freezing the quaternion as four `std::bit_cast` constants
and authoring it through a new **`AddStep_SetTransformRotationQuat`** (verbatim, no
math -- the yaw step ran `angleAxis` engine-side, so freezing the yaw alone would
have changed nothing). **Boot units 2906 -> 2908** (ZM-D-184 then took it to
**2909**); engine count unmoved at **1284**. (Both figures are true of ZM-D-184's
commit and are NOT the current pin — see the LIVE BASELINE block at the top of this
file.) Verified: a windowed **Release** tools
boot now authors `3F7926D9 / 3E6B4456` -- the committed bytes -- and leaves the file
byte-identical.

> **★ IT IS NOT ZM-D-179, AND ASSUMING IT WAS COSTS A CYCLE.** The falling-body
> reading is disproved by the diff itself: `WriteToDataStream` takes position and
> rotation from the SAME source, so a moved body cannot write a drifted rotation
> beside a bit-identical position. `liveBody == authored` in both configs. See
> DecisionLog ZM-D-183 for the full evidence and the generalisable rule (**a guard
> that compares a value against a re-computation of itself cannot detect that the
> computation moved**).

> **★ ANY authored entity whose rotation lands in a COMMITTED scene must now use
> `AddStep_SetTransformRotationQuat` with a frozen constant.** Yaw/euler steps stay
> fine for transient or gitignored scenes. Vesper is this game's only non-identity
> authored rotation, which is why the four townsfolk never surfaced it.

**★ INSTRUMENTED (2026-08-04, UNCOMMITTED): `Zenith_ValidateTerrainPhysicsBodies`.**
Every runtime scene load AND editor Stopped->Playing now states terrain collision at
INFO before the next physics step:
`[TerrainPhysics] context='...' terrain='DawnmereTerrain' physicsGeometry=yes collider=yes terrainVolume=yes body=yes`.
**If the fall-through recurs, that line is the whole diagnosis** -- `body=NO` means the
terrain has no collision (asset/bake problem, see ZM-D-182); `body=yes` means the
terrain was fine and the cause is elsewhere. Proven non-vacuous: with
`Physics_0_0.zmesh` corrupted in place it reports `physicsGeometry=NO ... body=NO` and
errors with **"3 dynamic body(ies) in loaded scenes will FALL THROUGH THE WORLD"** --
confirming the failure mode hits the **Player and the Wanderer too**, not just Vesper.

> **★ IT IS A LOUD `Zenith_Error`, NOT AN ASSERT, ON THE COMMON CASE -- DELIBERATELY.**
> A cold or unbaked terrain tree is a legitimate developer state and `Zenith_Assert`
> breaks in EVERY configuration, so asserting on "no physics geometry" would break every
> cold box and take the unit gate down with it. The one case that DOES assert is the
> impossible one -- geometry loaded but still no body -- which no asset can cause.

**★ RESOLVED (2026-08-04, ZM-D-184, UNCOMMITTED) -- THE VESPER FALL-THROUGH.** Root
cause was **an unbounded physics substep loop**, not anything about Vesper. The first
two frames after a scene load take ~0.49 s, which `Zenith_Physics::Update` drained as
**~29 consecutive 1/60 s substeps**; bodies free-fell through that burst, and once a
capsule's LOWER SPHERE CENTRE passes below the terrain's one-sided mesh the contact
normal inverts and the solver expels it downward. Vesper sank 0.61 m by frame 2 and
was gone; **the player cleared it by ~2 cm on the same load**, which is exactly why it
looked like a one-character bug. Fixed by capping the loop at **8 substeps** (root)
AND giving the rival a half-extent of spawn clearance like the wanderer has always had
(defence in depth). **`Dawnmere.zscen` re-authored** -- new SHA256
`76E33E5318AF951C212533587F53F76A61F75F4FB64D734CBBFE92B03F3D8709`, proven by **three**
boots (Debug x2 + Release x1, all identical). Boot units 2908 -> **2909**.

> **★ NOT the serializer, NOT terrain collision, NOT the collider rebuild** -- all three
> were checked and eliminated. See DecisionLog ZM-D-184 for the captured
> `[FallenBody]` report and the arithmetic.

> **★ CONFIRMED IN REAL PLAY (2026-08-05), not only by the suite.** The user re-ran the
> ORIGINAL repro -- a windowed Release tools build, editor Play, in-game "Continue" ->
> autosave -- and Vesper no longer falls. That matters because the automated tests never
> reproduced the defect in the first place: they were green throughout the whole period
> the bug was live, so "the suite passes" was never evidence either way here. The
> reported repro is the authority for this one.

**★ CURRENT COMMITTED-ASSET HASHES (OBSERVED 2026-08-05, after the ZM-D-184
re-author).** Only `Dawnmere.zscen` moved; the other rows were re-hashed and match
their previously recorded values exactly.

| Asset | SHA256 |
|---|---|
| `Dawnmere.zscen` | `76E33E5318AF951C212533587F53F76A61F75F4FB64D734CBBFE92B03F3D8709` (**proven by THREE boots: Debug x2 + Release x1, all identical**) |
| `Battle.zscen` | `1BEB0615F7FE62D9439471A4123E1D2140C0053AEC2991B659F7A03288C8C60A` |
| `FrontEnd.zscen` | `F7209CF525A1C66CF5F95AB68F12814465E419B6DBE200A08939465E608C910B` |
| `PlayerHome.zscen` | `DBBFB78311A55BBF942A7A5BF9928F43E9493A10CDA89110515A3B6A7987C780` (unchanged) |
| `ProfLab.zscen` | `1BCAABC9EA4A6FC559727C9573F47F7B7304052C586FB1D0519ADAF73DB75856` (unchanged) |
| `Dawnmere.znavmesh` | `DCAA84035A258B12FA23627FF719C0567018470C8055A1E0FB54D6C1F1F96E1D` (unchanged) |

> **★ THE PROOF PROTOCOL NOW NAMES THE CONFIGURATION (the ZM-D-183 lesson).** Two boots
> with matching SHA256 prove determinism only WITHIN one build configuration -- the three
> boots that resolved Q-2026-08-02-001 were all Debug, which is exactly why ZM-D-183's
> per-config drift hid behind a green proof. This row was taken from **Debug x2 AND
> Release x1**, so it proves idempotence and cross-configuration agreement at once.

**★ HISTORICAL (kept for the reasoning trail): what this looked like before it was
diagnosed.** Reported alongside
the above and initially assumed to share its cause -- it does not, and ZM-D-183 does
not address it. NOT reproduced across: `ZM_RivalVesperAuthored_Test` (Debug/Null x2
and Release -- `idleMax=0.0002 m` against a 0.35 tolerance), `ZM_NpcWander_Test`,
`ZM_SaveContinue_Test` (both configs), and `ZM_DawnmereNpcGroundTruth_Test`
(`rayHit=1` at every anchor including Vesper's). The terrain bake on the tree is
current (all three manifests `ZMTR` v2, 256 Dawnmere physics chunks at 28,201 bytes).

> **★ THE TERRAIN-WIDE HYPOTHESIS IS DEAD (2026-08-04, observed by the user).** The
> first theory was that the terrain's combined physics body was absent for the opening
> frames of a runtime load, dropping every dynamic body. **In the reported repro the
> PLAYER does not fall -- only Vesper does.** A player standing on that terrain proves
> the terrain body exists, so this is a PER-BODY defect, not a whole-world one. The
> candidates are now Vesper-specific: tunnelling at walk speed, a collider rebuild, a
> body spawned inside/beneath geometry, or a per-tick velocity write defeating contact
> resolution (`ZM_Interactable::HoldTrainerStation` rewrites XZ velocity every tick and
> copies Y verbatim; `ApplyDrivenBodySetup` locks all three rotation axes and calls
> `EnforceUpright`). NOT yet bisected.

> **★ ALSO A RED HERRING: `Chunk (X,Y) HIGH source is missing or invalid`.** These
> appear in bulk in every Dawnmere log and are EXPECTED, not a symptom. The engine grid
> is 64x64 and Zenithmon bakes a 16x16 export rect, while the streaming manager scans a
> 16-chunk radius around the camera -- so it constantly asks for render chunks outside
> the baked region. They say nothing about the camera's position and nothing about
> physics (these are RENDER chunks; collision is one combined body).

**★ INSTRUMENTED, AND IT HAS ALREADY CAUGHT THE FALL ONCE (`Zenith_FallenBodyWatch`).**
Three report kinds, all non-fatal: a **fall onset** (sustained descent AND a downward
speed a walk cannot reach), a **collider rebuild**, and **left the world** (Y < -50).

**★ WHAT THE CAPTURED REPORT ESTABLISHES (2026-08-04, user's Release tools run):**
```
'Npc_RivalVesper' (entity 17) has left the world: y=-50.17 ...
pos=(490.01, -50.17, 524.00) vel=(0.00, -36.13, 0.00) framesSinceLoad=242 timeSinceLoad=4.50s
```
* **He never moved horizontally** -- `pos.xz` is his authored `(490, 524)` exactly, and
  `vel.xz` is exactly zero (so `HoldTrainerStation` was ticking him throughout). **He
  did not walk off anything.**
* Back-solving the fall with Jolt's default linear damping (0.05): `v=36.13 m/s` is
  ~4.07 s of falling over ~75.7 m, so he **started at y ~ 25.5-26.8 -- his authored
  pose -- about 0.43 s (~23 frames) after the load.** He stood correctly, then sank.
* That rules out BOTH earlier candidates: not spawned-inside-geometry (that would be
  `framesSinceLoad` ~0), and not tunnelling at walk speed (he never moved in XZ).

**★ WHAT A HEALTHY RUN SHOWS, so the next capture is read correctly:** every Zenithmon
human -- player, wanderer AND Vesper -- has its **collider rebuilt at
`framesSinceLoad~2`** (the explicit `ZM_HumanBody.h` capsule replacing the authored
one), at uniform scale 0.691, volumeType 3. **This is normal and is NOT the fault**;
it happens identically on runs where nobody falls, and it is ~20 frames before the
observed onset. Do not chase it on its own.

> **★ THE ONSET DETECTOR NEEDS THE SPEED CLAUSE, AND HERE IS WHY.** A first version
> keyed only on "0.5 m of continuous descent" and duly reported the player and the
> wanderer on a perfectly healthy run -- **a character walking down a slope descends
> continuously for metres** (observed -1.0 to -3.2 m/s). Free fall is separable by
> SPEED, not by distance: the real fall measured -36 m/s. The threshold is 8 m/s.
> If this is ever re-tuned, re-check it against a run where a character walks downhill.

> The `3840 chunk(s) skipped` line in every terrain log is CORRECT and not a symptom:
> the engine grid is 64x64 = 4096 and Zenithmon bakes a deliberate 16x16 export rect
> (chunks 0..15 -> world `[0,1024)^2`). Vesper stands in chunk (7,8).

**★ CURRENT BASELINE -- USE THESE NUMBERS, not the older ones quoted further
down this file (RE-OBSERVED 2026-08-03 after the boot-profiling work, on a
Null_ build):**
ZM headless registry **55 passed / 0 failed**; ZM boot unit gate
**2893 ran / 2891 passed / 0 failed / 2 skipped** (`zm-tests.yml` pinned to
**2893**); engine boot unit gate, Null Combat, **1271 ran / 1270 passed / 0
failed / 1 skipped** (`run_unit_gate.ps1` default `-Baseline 1271`). One skip in
each is the quarantined `GraphComponent::RegistryWideNodeRoundTrip`
(task_726cc81d); ZM carries a second, ZM-side skip.

**Boot profiling moves both inventories by the SAME +23**, because every new unit
is an engine unit: 18 boot-capture units + 1 CommandLine boot-flag unit
(`Zenith/Profiling/Zenith_Profiling.Tests.inl`), 1 dump-coordinator latch unit
(`Zenith/Core/Zenith_Main.Tests.inl`), 1 present-latch unit, 1
automation-session-gating unit, and 2 boot-timeline helper units. The recorded
pins moved by 27 rather than 23 because **they were already stale by 4**: commit
`33531b6a` (shadow-cascade ordering) added four engine units and updated neither
pin, so HEAD was really 2868 / 1246, not 2864 / 1242. A follow-up added one more
engine unit (`Core::UnitTestTimingReport`, for the `--unit-test-timings`
plumbing), taking both pins to **2892 / 1270**; then one more
(`CommandLine::ExitAfterUnitTestsIsNotAnAutomatedTestRun`) with the
`--exit-after-unit-tests` gate fix, taking them to **2893 / 1271**.
**ZM-D-182 moves the ZM boot inventory only: 2863 -> 2864** (+1
`HumanVisual_RestartPreservesTheAnimatorRig`, the only unit that reaches the WARM
human branch). It changes no unit under `Zenith/`, so the engine baseline remains
**1242** and the registered-test inventory remains **55**.
Before it, the world-scale update had moved the ZM boot inventory 2861 -> 2863
(+1 measured creature origin-policy unit, +1 PlayerHome/exterior-envelope unit).

**★ ZM-D-182 ALSO BUMPS EVERY TERRAIN BAKE STAMP, AND THAT COSTS A BOOT.** The
physics collision divisor moved 8 -> 4, which rewrites every `Physics_*.zmesh`
without changing the FILE COUNT -- so no stamp would have invalidated, and a
warm tree would have loaded terrain with **no physics body at all**
(`TryReadTerrainChunkSnapshot` rejects a mismatched chunk; the game then runs
with characters falling through the world). `uZM_TERRAIN_MANIFEST_VERSION` is now
**2**, so **the next tools boot re-bakes all three recipes (several minutes)**.
Dawnmere's re-bake is redundant -- it was already divisor-4 -- but the stamp is
per-version, not per-content, and a redundant bake is byte-identical and safe.
The bake boot does NOT re-author any scene (`m_bAuthorDawnmereScene` requires
`m_bAllWarm`), so no committed `.zscen` moves; re-authoring still needs the
separate `sceneAuthoring=AUTHOR_DAWNMERE` boot. CityBuilder (`terrain_hills_v5`)
and RenderTest (`terrain_proc_v7`) were bumped in the same change. See DecisionLog
ZM-D-182 and `Zenith/Flux/Terrain/CLAUDE.md`.

**★ WORLD-SCALE AUTHORING PROOF (2026-08-02).** Two consecutive warm windowed
`_True` boots ran `sceneAuthoring=AUTHOR_DAWNMERE` with `warmMask=0x7` and wrote
identical scene bytes. The only exterior re-author was Dawnmere, whose 17 x 13 m
Home facade now encloses the PlayerHome interior's 16.5 x 12.5 m wall envelope;
the interior scenes did not move in this pass.

> **★ THE `Dawnmere.zscen` ROW WAS STALE AND IS NOW RE-PROVEN (2026-08-02, ZM-D-182).**
> The row read `D1464B77...5118` while the file on disk hashed `E7413197...9716`. That was
> raised as Q-2026-08-02-001 and deliberately NOT re-pinned on sight -- a scene-byte change
> owes two authoring boots with identical SHA256, and pasting the on-disk value would have
> manufactured a proof nobody obtained. **The proof was then actually run** (see the table
> below): Dawnmere authoring is DETERMINISTIC, the old row simply described an earlier
> authoring pair and was never refreshed after the Home facade re-shape. Q-2026-08-02-001 is
> RESOLVED.

**★ AUTHORING PROOF RE-RUN 2026-08-02 AFTER ZM-D-182 (supersedes the pre-ZM-D-182 table).**
Three windowed `Vulkan_..._True` boots of
`zenithmon.exe --automated-test ZM_Boot_Test --skip-unit-tests`:

| Boot | `[ZM Terrain] Batch result` | Dawnmere.zscen |
|---|---|---|
| 1 | `warmMask=0x0, queueMask=0x7, queued=3, sceneAuthoring=DEFERRED` | untouched (a bake boot correctly authors nothing) |
| 2 | `warmMask=0x7, queueMask=0x0, sceneAuthoring=AUTHOR_DAWNMERE` | `E7413197...9716` |
| 3 | `warmMask=0x7, queueMask=0x0, sceneAuthoring=AUTHOR_DAWNMERE` | `E7413197...9716` (**identical**) |

Boot 1 also re-baked all three terrain recipes at the new collision density -- Route1 and
Thornacre went from 7,785-byte (divisor-8) physics chunks to 28,201-byte (divisor-4) ones,
which is ZM-D-182's stamp bump doing exactly its job. **All five scenes were byte-stable
across boots 2 and 3**, so boot-shape independence holds at the new density too.

| Asset | SHA256 |
|---|---|
| `Dawnmere.zscen` | **SUPERSEDED at ZM-D-184** -- was `E7413197...9716`; see the CURRENT table at the top of this file |
| `PlayerHome.zscen` | `DBBFB78311A55BBF942A7A5BF9928F43E9493A10CDA89110515A3B6A7987C780` |
| `ProfLab.zscen` | `1BCAABC9EA4A6FC559727C9573F47F7B7304052C586FB1D0519ADAF73DB75856` |
| `Dawnmere.znavmesh` | `DCAA84035A258B12FA23627FF719C0567018470C8055A1E0FB54D6C1F1F96E1D` (**unchanged**) |

**ZM-D-181 touches BOTH sides, so both boot baselines move:** ZM 2849 -> **2861**
(+12: 2 human-generator units, 5 engine collider units, 5 human-visual units) and
the engine 1237 -> **1242** (+5, the explicit-collider-dimension units). The
registry moves **56 -> 55**: ZM-D-181 added no automated test and DELETED one
(`ZM_NpcRenderedPalette_Test` -- it measured pixel separation between palette
blocks that the game no longer draws; see TestPlan 5.8).
ZM-D-179 had moved both together (+2 each) for the two new
`Physics::TransformSerialization*` units; ZM-D-176 moved the boot baseline
**2840 -> 2847** (+7) and the registry **54 -> 56** (`ZM_InteriorTint_Test`,
`ZM_InteriorTintPixels_Test`); ZM-D-175 had moved it **2825 -> 2840** (+15) with
the registry unmoved at 54; ZM-D-174 stood at registry 54 / boot 2825.

**★ ZM-D-181 DELIBERATELY MOVED SCENE BYTES -- THE FIRST CHANGE SINCE ZM-D-176 TO
DO SO ON PURPOSE.** Three of the five committed scenes were re-authored, because
the six Dawnmere NPCs and the player stopped being unit cubes and became the
generated human models S4 has been baking since 2026-07-16. The authored change is
narrow by construction: **only `AddStep_SetTransformScale` moved** (from the
0.8 x 1.8 x 0.8 body box to the uniform `fZM_HUMAN_VISUAL_SCALE`), plus a
`ZM_GreyboxVisual` component added to the three Player entities. Positions,
collider steps, the rival's yaw step, the component lists and the step ORDER are
byte-identical.

Post-authoring SHA256 (two consecutive windowed `_True` authoring boots wrote
**identical** bytes, which is the idempotence proof ZM-D-148 requires):

| Scene | SHA256 | Moved? |
|---|---|---|
| `Battle.zscen` | `1BEB0615F7FE62D9...` | no |
| `Dawnmere.zscen` | `6817534989B1A083...` | **yes** |
| `FrontEnd.zscen` | `F7209CF525A1C66C...` | no |
| `PlayerHome.zscen` | `DBBFB78311A55BBF...` | **yes** |
| `ProfLab.zscen` | `1BCAABC9EA4A6FC5...` | **yes** |

**`Dawnmere.znavmesh` is UNCHANGED (`DCAA8403...`), and that is a REQUIREMENT, not
an observation.** Its bake is a flat coverage grid that never reads a collider
(`Source/Nav/ZM_NavBake.cpp:29-38`), so a navmesh that moved would mean something
else moved with it. Verify it every time.

**★ THE HUMAN BODY IS NO LONGER A FUNCTION OF TRANSFORM SCALE.** ZM-D-181's
load-bearing change is `Source/World/ZM_HumanBody.h`: one compiled statement of
how big a person is (1.8 m tall, 0.8 m footprint, a 0.4 / 0.5 capsule), installed
into the Jolt body EXPLICITLY through the new
`Zenith_ColliderComponent::SetExplicit{CapsuleDimensions,BoxHalfExtents}`. It had
to move: the authored scale now belongs to the MODEL and is UNIFORM, and a uniform
scale degenerates a scale-derived capsule into a sphere. The consequence worth
repeating is that **gameplay dimensions no longer depend on whether the human bake
exists** -- a cold clone draws a proportioned palette block and measures the same
body.

**`run_unit_gate.ps1`'s default is the ENGINE cross-game number and MUST NOT
move for a game-only change** -- ZM-D-174 touched no file under `Zenith/`, so it
bumped only the three ZM-side sites. ZM-D-181 DOES touch `Zenith/`, so it moves.

**★ THE 2026-08-01 SESSION RE-MEASURED BEFORE EDITING A LINE, AND THIS TIME THE
THREE SITES AGREED.** A clean build of both configs read **2817 / 2815 / 0 / 2**
and registry **53/0** -- exactly what this block, `zm-tests.yml` and
`Shortfalls.md` all claimed. That is the first session in three where the pinned
numbers survived contact with a fresh binary. The habit is still mandatory: it
cost one build to confirm, and the two occasions it was skipped both shipped a
red `zm-tests`.

**★★ AND THE NUMBER THIS FILE CARRIED BEFORE ZM-D-173 WAS WRONG IN A WAY THE
'ALL THREE OR NONE' RULE BELOW DOES NOT CATCH.** A clean HEAD build measured
immediately before that work reported **2809 ran, 2 skipped** -- while this
block said 2759, `Shortfalls.md` said 2759, and `zm-tests.yml` was pinned to
**2804**. So `zm-tests` was ALREADY RED on master, and the second skip had
appeared with no document recording it. The rule assumes the three sites drift
APART; here all three were stale together against the actual binary, which only
a MEASUREMENT finds. **Measure the baseline on a clean build at the START of a
session, before trusting any of the three.** ZM-D-173 set all of them from
OBSERVED lines; only +8 of the move (2809 -> 2817) was earned by that change.

**★ ZM-D-173 (2026-07-31) IS AN ENGINE CHANGE AND TOOK THE CROSS-GAME GATE.**
Ordinary `Zenith_Physics::Raycast` calls now IGNORE SENSOR BODIES -- every shipped
caller is a line-of-sight/occlusion query and a sensor is a volume you walk
through -- so all four engine sensor units land in every game's boot total
(+4 each: Combat 1231 -> 1235, DevilsPlayground 1232 -> 1236, RenderTest
1322 -> 1326). It also RELOCATED Dawnmere's Home +40 m in Z with its terrain
pad, so the entrance faces the -Z side the fixed-yaw camera trails into, and
added two permanent automated guards (registry 51 -> 53). The scene and navmesh
were re-authored and are hash-stable across two equivalent boots.
**ZM-D-171 established registry 51 and baselines 2751/1173**: registry 50 -> 51 via
`ZM_ShellLighting_Test`, while both boot baselines moved +9. **ZM-D-172 leaves the ZM registry at
51 and moves only the boot baselines +8**, for the scene-Sun and IBL-regeneration units: 2751 ->
2759 and 1173 -> 1181. Every pinned site was updated from an OBSERVED line in the same commit:
this block, `zm-tests.yml` `-Baseline`, `Shortfalls.md`'s bullet, and
`run_unit_gate.ps1`'s default. Both were ENGINE changes and took the full cross-game gate; see
ZM-D-171 and ZM-D-172 for their observed matrices.

**★ THIS BLOCK AND `zm-tests.yml`'s PIN MOVE TOGETHER OR NOT AT ALL, and on
2026-07-29 they did not.** This block read 2722 while the workflow was already
pinned to 2742 -- so the file that shouts "use these numbers" was itself the
stale one, and `Shortfalls.md` had a THIRD figure (2731). Any commit that adds a
boot unit updates: the OBSERVED line here, the `-Baseline` in
`.github/workflows/zm-tests.yml`, and the baseline bullet in `Shortfalls.md`.
Never write a PREDICTED count into any of the three.

**★ AND `doc-lint` WILL NOT CATCH THIS. `Tools/doc_lint.ps1` hardcodes
`$docsDir = Games/DevilsPlayground/Docs` and never reads `Games/Zenithmon/Docs`
at all** -- its "test count consistency across Status / TestPlan /
BuildEnvironment / AgentBriefing / Shortfalls" check scores DP's five files, not
this game's. A green `doc-lint` is therefore ZERO evidence about any Zenithmon
doc. Every drift below was found by hand, twice; treat these docs as unlinted.

**★ THE REGISTRY MOVED AT SC8 (46 -> 47) BUT NOT AT SC7, AND BOTH ARE CORRECT.**
SC7's three new end-to-end phases live INSIDE the existing
`ZM_TrainerSightWalkUp_Test` registration, so its count stayed 46 while boot
units moved +13. SC8 adds a genuinely NEW registration
(`ZM_RivalVesperAuthored_Test`) because its subject is a different entity making
a different claim. Before "fixing" a registry count that did not move, check
whether the coverage rode an existing registration.

**★ SIXTH TRIPWIRE -- A NEGATIVE CONTROL PROVES NOTHING UNTIL IT FLIPS.** SC8's plan
called for one: make the new test fail against the old scene bytes, re-author, watch
it pass. It DID fail first -- and then **failed again after the re-author**. A
fail-then-fail is not a control, it is a masked defect. Run ONLY the "after" and one
red test gets blamed on the re-author not working; run ONLY the "before" and a
coincidence gets recorded as proof. **It is the PAIR that carries the information.**
The masked defect here was real and is documented in ZM-D-156: an AABB collider
forces its body to identity rotation, wiping the authored yaw out of the SAVED
BYTES while every boot unit stayed green.

**★ AND ITS COROLLARY: A UNIT SUITE CANNOT SEE A SCENE-BYTE DEFECT.** SC8's boot
units reason about the COMPILED placement constants -- position, bearing, clearances
-- all of which were correct (`placementErrX/Z = 0.0000`) while the authored rotation
on disk was identity. Only the windowed round trip reads what was actually saved.
When a sub-commit writes a scene file, the windowed run is not a formality.

**★ FIFTH TRIPWIRE -- A SURVIVING MUTATION HAS TWO POSSIBLE CAUSES AND YOU MUST
DETERMINE WHICH.** (SC8 reproduced this a second time: reverting the collider to AABB
SURVIVED, not because the facing clause lacks teeth, but because the mutation cannot
manifest without RE-AUTHORING the scene -- which a battery does not do. The clause's
teeth were proven live instead: AABB-authored scene FAILS, OBB-authored scene PASSES.) SC7's battery had two survivals with OPPOSITE meanings. One was a
real coverage gap: transposing the challenge-lines selector's null-check and clamp
survived because the only pair that discriminates them is `(nullptr, count > cap)`
and the clause used a count UNDER the cap -- **its failure text named the exact
hazard while its fixture could not reach it.** The other was a BAD MUTATION:
re-spelling the shared node-type-name constant redded nothing, and correctly so,
because every site reads that one constant so the two sides cannot diverge (the real
hazard is a hard-coded literal at one site, which does red). **Calling both "no
teeth" would have libelled a good test; calling both "bad mutation" would have
shipped a hole.** Ask, every time: *can this mutation actually produce the behaviour
the test is meant to catch?*

**★ FOURTH TRIPWIRE -- A MUTATION HARNESS MUST PARSE THE OBSERVED RESULT, NOT
THE EXPECTATION.** SC6's battery script scraped `(\d+) failed` from
`run_unit_gate.ps1`'s output and matched the **"wanted 2682 ran, 0 failed"**
clause instead of the observed tally, so all three unit mutations were initially
labelled SURVIVED -- i.e. "these tests have no teeth" -- when every one of them
had in fact redded. The `baseline NOT met` line and the pass-count delta
(2681 -> 2680 / 2679 / 2680) are what settled it. This is the same
passes-because-it-never-really-checked defect the project hunts in game code,
reproduced in the tooling: anchor result parsing to the OBSERVED line, and never
to a string the harness also prints when describing what it wanted.

*How they moved this session:* 2547 -> **2589** was the 07-27 engine commits
(harness world reset, harness dt pinning, navmesh RNG determinism) adding units,
and `e687d095` left BOTH baselines one short, so `engine-gate` and `zm-tests`
were RED on master until **ZM-D-149** fixed them forward (1163 -> 1164,
2588 -> 2589). 2589 -> **2607** is SC2's own +18 units (ZM-D-150), and
2607 -> **2638** is SC3's +31 (ZM-D-151), 2638 -> **2644** is SC4's +6
(ZM-D-152), and 2644 -> **2657** is SC5's +13 (ZM-D-153, which also took the
automated registry **44 -> 45**).

**★ THIRD TRIPWIRE -- A MUTATION BATTERY MUST CHECK THE BUILD'S EXIT CODE.** Two
SC4 batteries mutated the gate to `if (false)` / `if (true)` and then to a
hard-coded flag argument. Both FAILED TO COMPILE (unreachable `return`, and an
unreferenced formal parameter -- C2220, warnings are errors here), so the gate
booted the STALE exe and reported a perfectly green 2644/2643/0. That is
indistinguishable at a glance from "the mutation did not red, so the test has no
teeth" -- i.e. it would have libelled a good test. Mutations must keep every
parameter referenced (transpose arguments, invert a returned expression) AND the
harness must hard-fail on a non-zero build exit.

**★ SECOND GATE TRIPWIRE:** a new `.cpp` under `Games/Zenithmon/` needs
`Build\regen.ps1` BEFORE the build, and the failure mode is SILENT. Sharpmake
globs the game tree at GENERATION time and everything it emits is gitignored, so
a skipped regen builds GREEN with the new TU simply absent -- and if nothing else
references its symbols there is no link error either. **The unit-count delta is
the only proof the regen took** (SC2 expected +18 and got +18; SC3 expected +31
and got +31).

**★ GATE-ORDER TRIPWIRE (cost a wasted 300 s boot to rediscover):** the boot unit
gate must run AFTER `zenith test <Game> --headless`, never before, on a freshly
built Null exe. `zenith test` heals the output directory's DLLs; a fresh `Null_*`
build lacks `libcurl-d.dll`, so booting the exe first dies in the loader with
ZERO stdout and the gate reports the misleading "no 'Unit tests complete' line in
boot output" rather than a load failure.

**Stage:** **★★ S7 IS COMPLETE (2026-07-29). Both open boxes are ticked and the stage gate is
annotated MET.** Item 3 closed by SC1-SC3 (ZM-D-163/166/167): the two verbs W3 had cut on evidence,
`freeze input` and `approach`, now ship, and the rival physically walks to the player. "Rival
battle 1" was reworded to what actually ships and ticked under the user's 2026-07-29 ruling, with
its two GDD deviations booked as S8 debt rather than hidden (Route-1 placement, and the
counter-starter rule -- which was booked NOWHERE until the pre-tick audit found it). Observed at
closure: headless **49/0**, full windowed Vulkan **49/0**, boot **2742 / 2741 / 0 / 1**.
**★ THE CAMERA CUT IS NOT CLAIMED BY THAT TICK** -- it is absent from `Roadmap.md:104`'s text, was a
W3 aspiration only, and stays booked in Shortfalls 1.8 (with ZM-D-159's "needs a real engine feature"
claim corrected: `ZM_FollowCamera` is a Zenithmon component and the sole writer of the camera pose).
**Next: S8, whose go/no-go gate FOLLOWS its four content items rather than preceding them.**

**★★ A VISUAL AUDIT ON 2026-07-29 (ZM-D-168) FOUND FOUR THINGS THE GREEN SUITE CANNOT SEE. READ
THIS BEFORE QUOTING ANY VISUAL CLAIM.** Ten windowed tests, 1212 captured frames, **all ten PASSED
while every item below was true**:
1. **NPC blockouts are invisible in play.** Rendered vertical faces sample **0.004-0.055** against
   terrain at **0.44**; rendered NPC-to-NPC separations are **0.017-0.041** against the 0.15 margin
   the tests certify at `vsNearestNpc=0.2124`. The tests sample the MATERIAL colour and never read a
   pixel. **The cause is shading, not the roster:** the top face lights, the sides do not (near-
   overhead sun, no meaningful ambient term), and sides are all a player sees. ZM-D-160/164 and this
   file overstated the user-visible effect; the roster fix is correct but moot until shading is done.
2. **Creature models and the battle HUD were UNVERIFIED by pixels -- RESOLVED 2026-07-30 (ZM-D-170),
   AND THE AUDIT'S NULL WAS AN OBSERVATION MISS, NOT AN ABSENCE.** Both halves draw. Six arms on
   `ZM_BattleMenuRun_Test` read the swapchain TGA it was **already writing**: HP bar chroma
   **G-R +0.428**, the three root buttons at luminance **0.586-0.631** against a **0.276** panel
   interior, the text log **522** glyph-white px against **0** in an adjacent control box, and both
   platforms carrying a rendered `Fernfawn` (body vs local background **0.191-0.241**, the two bodies
   alike to **0.140**, each **0.918 / 1.052** clear of its slab). Scope is narrower than the old
   bullet implied: this pins the CREATURE MODEL family for ONE species, not S4's five generators.
3. **The spotted marker draws, but reads as a sphere + diagonal stroke,** not an exclamation mark.
4. **Dawnmere reads as an open field, not a town** -- no buildings; the Trade Post and Care Center
   are dialogue only.
**★ ITEMS 1 AND 3 ABOVE WERE RESOLVED 2026-07-30 BY ZM-D-169, ON PIXELS** (NPC bodies now read at
eye level -- min rendered separation **0.2001** against a 0.15 floor, per-body RGB 0.34-0.84 per
channel where the audit measured 0.004-0.055; the marker now reads as an upright exclamation mark,
**118 px spanning 7x28**). **★ ITEM 2 WAS RESOLVED 2026-07-30 BY ZM-D-170, ALSO ON PIXELS, AND IT
WAS NOT A DEFECT** -- the HUD and both creature models were drawing the whole time; the audit's null
was an observation miss, and the real hole was that a capture already on disk had no assertion
touching it. **Item 4 (Dawnmere reads as an open field) remains open.** **★ AND THE SHADING CAUSE
BEHIND ITEM 1 WAS FIXED ON 2026-07-30 BY ZM-D-171 (corrected here 2026-08-01).** This block used to
end "the shading CAUSE behind item 1 is still unfixed -- only the six NPCs were given an emissive
floor". The engine's ambient is now physically grounded (virtual-ground bounce in the IBL cubes,
the 0.5 IBL fudge deleted, the sun key derived from the same atmosphere the sky renders), measured
on the DawnmereHomeShell by `ZM_ShellLighting_Test`: sun-averted vertical face luminance
**0.061 -> 0.1672**, unlit/lit **0.124 -> 0.3688**, blue/red cast **~7 -> 1.13**. Every blockout
benefits, not just NPCs, **and ZM-D-169's NPC emissive floor was DELETED with it.** The residual is
ART DATA, not lighting: minimum RENDERED pairwise separation under honest lighting is **0.0763**,
so the palette re-author needed to reach 0.15 on screen stays booked in Shortfalls 1.8-4.

**★★ DO NOT USE `Tools\capture_viewport.ps1` TO JUDGE A SHORT BEAT. This file used to advise
`-IntervalMs 60` or lower; THE SCRIPT CANNOT DELIVER THAT.** Measured 2026-07-29/30: it was asked
for 40 ms and delivered **206 ms** at 2560x1440, and 81 ms at 1280x800 -- PNG encode dominates the
loop, so the interval parameter is a floor the script never reaches at usable resolutions. dt is
pinned to 1/30, so a 0.35 s beat is ~11 frames and you sample 1-2 of them. **For any beat shorter
than about a second, use the frame-exact engine path instead:** `Flux_Screenshot::RequestDump` from
inside the frame you care about, consumed once per `EndFrame` by `Zenith_Vulkan_Swapchain` (a no-op
on Null, so gate on `Zenith_IsNullRenderer()`). `ZM_NpcRenderedPalette_Test` and
`ZM_RivalVesperAuthored_Test` both do this now. **And build the colour predicate from the bytes the
dump wrote, never from the colour you submitted** -- see ZM-D-169: a marker submitting linear
`(1.0, 0.82, 0.08)` unlit lands at `RGB(208, 182, 97)`, and two "low blue" scans reported ZERO
matches across 539 frames of a marker that was rendering perfectly.
Evidence: `Build/artifacts/evidence_final/` and `Build/artifacts/zenithmon/visual_audit/`
(both git-ignored).

**Historical note on how S7 nearly closed wrong -- keep this, it is the reason the stage is honest:**
this file previously claimed "S7 COMPLETE" for five commits while
previously claimed "S7 COMPLETE (items 1-4)"; the user REJECTED that claim on
2026-07-29, and the Roadmap's own text proves it. `Roadmap.md:104` (S7 item 3)
specifies `forward cone + occlusion raycast -> FREEZE INPUT -> APPROACH -> dialogue ->
forced battle -> defeat flags + prize money`. Every link in that chain shipped EXCEPT
`freeze input` and `approach` -- both **CUT ON EVIDENCE** by W3 (ZM-D-159). So the
`- [ ]` at `Roadmap.md:104` and the `- [ ]` at `Roadmap.md:161` are **HONEST, not stale
bookkeeping**, and an audit that reads only the DecisionLog will wrongly conclude they
"should be ticked" -- one did, this session, and was overruled by the box's own text.
**★ THE LESSON: WHEN STATUS.MD'S PROSE AND A ROADMAP CHECKBOX DISAGREE, THE CHECKBOX
WINS.** Prose accumulates optimism across sessions and each session inherits the last
one's summary; the box is what `StartPrompts.md` prompt 0 step 3 actually reads, and it
is scored against the item's LITERAL text. Pre-S8 known-limit closure W1-W5 did all land
(ZM-D-157/158/159/160/161), but **closing five recorded limits is not the same as closing
the stage.** S0-S6 remain complete.
**★ THE LAST TWO SENTENCES OF THIS NOTE EXPIRED ON 2026-07-29 AND ARE CORRECTED HERE (2026-08-01).**
They read *"No S8 content begins, and the S8 go/no-go is NOT the next step -- FINISHING S7 is."*
S7 closed later that same day (SC1-SC3, ZM-D-163/166/167), so the instruction now points at
finished work -- the exact failure mode this historical note exists to warn about, reproduced by
the note itself. **What survives unchanged: the go/no-go is still NOT the next step.** S8's four
content items (the four unticked boxes under Roadmap's **"S8 -- Vertical slice, go/no-go"** heading)
are, and the gate FOLLOWS them. *(Cited by NAME, not line: every line-number citation in this file
has drifted at least once, which is the lesson recorded further down.)*
**★ GATE OBSERVED AT ZM-D-179 (2026-08-01) -- the CROSS-GAME one, because it is an ENGINE change
in `Zenith/EntityComponent`. Every figure below is an OBSERVED line.** ZM headless **56 / 0**;
full windowed Vulkan **56 / 0, all 56 RAN**; ZM boot gate **2849 / 2847 / 0 / 2** (PASS);
engine boot gate on Null Combat **1237 / 1236 / 0 / 1** (PASS); Combat headless **14 / 0**,
CityBuilder **46 / 0**, DevilsPlayground **159 / 0**, RenderTest **11 / 0**; TilePuzzle Null_True
builds clean; SentinelECS / SentinelPhysics / SentinelAI all built and all **exit 0**;
`zenith regen --check` in sync. **Teeth mutation-proven:** transposing the new
`if (!PhysicsPoseDiffersFromCache(...))` polarity reds **exactly** the two new units
(2849 / 2845 / **2** / 2) -- both arms of each -- and nothing else; restoring returns to 0 failed.
Ratchets both stay pre-existing RED and this commit adds nothing: `architecture,lints` fails on
`Zenith_TerrainComponent` (EC->Flux edge + `std::vector`/`std::function`) and per-file `g_xEngine`
counts in `Zenith_GraphicsOptions.cpp` (33>32) and `Zenith_TerrainEditor.cpp` (22>21);
`complexity` on `ParseCommandLine`, `ValidateTerrainGridTopology`, `ZENITH_PROPERTY` and
duplicate-clusters=10. **Not one failing finding names a file this commit touches** --
`Zenith_TransformComponent.cpp` is absent from both lists, because the fix reuses the existing
getters and adds no `g_xEngine` token. **And `Dawnmere.zscen` now reproduces:** two consecutive
windowed authoring boots both wrote SHA256 `F403A489D0B11C77...`, which is what is committed here,
so a boot leaves `git status` clean again.

**Build:** GREEN on the ZM-D-148 diff (scene authoring made boot-shape-independent; all four ZM scenes now TRACKED) on top of SC1b commit B (ZM-D-147 -- baked navmesh persistence). Engine-wide, so it owed and got the full gate: `Build\regen.ps1` GREEN + `zenith regen --check` in sync; engine lib + SentinelECS/Physics/AI (all three exes exit 0); Zenithmon Vulkan_True + Null_True; Combat / CityBuilder / DevilsPlayground / RenderTest / TilePuzzle Null_True.
**Tests (commit B):** Null batches, ALL 0-failed: **ZM 44/44** (registry 42 -> 44; both new navmesh tests RUN, not skipped), CB **45/45**, DP **158/158**, RT 9/9, Combat 14/14. Full **windowed Vulkan ZM 44/44, 0 skipped, 0 failed**. Boot unit gates on the NULL exes: engine **1093 -> 1121** (Combat) and ZM **2515 -> 2546** -- both pinned from the OBSERVED line. Windowed RenderTest 8 passed / 1 failed, only the documented pre-existing `RT_TennisDeterminismDigest` (Q-2026-07-21-002). Ratchets (`architecture,lints` and `complexity`) are **byte-identical to a pristine-HEAD worktree** -- both stay pre-existing RED, nothing added; two findings this commit DID introduce (an `Editor/` include and a `g_xEngine` reach from EntityComponent) were fixed, not allow-listed. **Asset-less CI condition reproduced locally** (`Zenith/Assets` hidden): ZM 44/44 and both unit gates unchanged; restored by MERGE and `diff -rq`-verified, since the run re-created only 60 of the 89 files and a naive rename-back would have clobbered the tree. **Teeth mutation-proven ×6** (see ZM-D-147; m1 re-run on the final build reds exactly the 3 serialization units).

**★ ZM-D-148 (2026-07-26, user-directed follow-up):** scene files no longer encode PROCESS-GLOBAL entity slot indices -- `Zenith_Entity::WriteToDataStream` now emits dense authoring-order file indices (`Zenith_EntityFileIndexMap`). Reproduced first (a boot with the unit suite vs `--skip-unit-tests`): Battle, PlayerHome **and Dawnmere** all differed, so the scene ZM-D-147 committed was itself at risk. After the fix all four hash identically across both boot shapes and survive a full windowed batch byte-unchanged, so **all four scenes are tracked** and `.zscen` is re-included repo-wide. The loader needed no change (file indices were always opaque keys) and OLD scenes still load -- proven by Combat 14/14, CB 45/45, DP 158/158 against their un-re-authored files. Baselines **1121 -> 1122** / **2546 -> 2547**; mutation-proven (reverting the writer reds exactly `Scene::SceneBytesAreIndependentOfSlotAllocation`). **RenderTest diagnosed too:** the "2 failed" is 1 real failure + a synthetic `<batch:exit=1>` harness marker, and `RT_TennisDeterminismDigest` is a TEST-ISOLATION defect -- it produces the EXACT pinned digest standalone and a stable wrong one only in batch, on both backends; RenderTest registers no `BetweenTestsHook`. Both logged questions are now RESOLVED.

**CI (post-push, `a6f8b048`):** `zm-tests`, `engine-gate`, `dp-tests`, `cb-tests`, `shader-validation`, `doc-lint`, `Memory Gate` all **GREEN** -- every gate that was green before this commit still is, first attempt, no fix-forward needed. `Complexity Gate` + `layering-gate` remain red and were verified **byte-identical to the previous commit's own CI logs** (the same pre-existing `Zenith_TerrainComponent` findings), so this commit added nothing to either. **★ The claim that mattered, confirmed on the runner:** on a fresh checkout whose ONLY Zenithmon assets are the two committed files, `ZM_NavmeshAsset_Test` (5 frames) and `ZM_DawnmereHeadless_Test` (3 frames) both **RAN and PASSED** -- not skipped -- inside a 44/44 suite, with the boot gate at **2546 ran / 2545 passed / 0 failed**. Navigation is CI-verifiable for the first time.

**Prior (commit A):** **Headless now means the `Null_*` build config -- there is no `--headless` flag.** Null batches, ALL 0-failed: **ZM 42/42 (31 RUN, was 3)**, CB **45/45 (45 RUN)**, DP **158/158 (138 RUN)**, RT 9/9, Combat 14/14. Full **windowed Vulkan ZM 42/42, 0 skipped, 0 failed**; windowed RenderTest 8/1 with only the documented pre-existing `RT_TennisDeterminismDigest` red (Q-2026-07-21-002) and `TerrainEditorSmoke` GREEN. Boot unit gates on the NULL exes: engine **1093** (Combat) and ZM **2515** -- both pinned from the OBSERVED line (`run_unit_gate.ps1` default and `zm-tests.yml`). The -14 vs the Vulkan counts (1103 / 2525) is exactly the Vulkan-only test set (11 `Flux_SlangProbes` + 3 `DetermineImageViewType`); the +4 is this commit's new terrain units. **★ Teeth mutation-proven:** leaving `Zenith_Null_MemoryManager::InitialiseIndirectBuffer`'s VRAM handle invalid reds EXACTLY `Terrain::CullingResourceInitSurvivesOnCurrentBackend` (1093/1091/1) and nothing else; restored -> 1093/1092/0. Ratchets: `architecture,lints` findings are **byte-identical to pristine HEAD** (all 3 pre-existing); `complexity` was ALREADY RED on master (same 2 cognitive findings) and moves duplicate clusters 9 -> 10, the accepted cost of the deliberate D3D12/Null twinning.
**CI (post-push, 3 commits -- `2628add3` then two fix-forwards):** `zm-tests`, `dp-tests`, `cb-tests`, `engine-gate`, `scaffold-smoke`, `shader-validation`, `doc-lint`, `Memory Gate` all **GREEN**. `Complexity Gate` + `layering-gate` remain red -- **pre-existing on master** (confirmed from CI history across the preceding commits AND from a pristine-HEAD worktree that reproduces the identical findings). **★ THREE CI-ONLY DEFECTS the local gate could not see**, all because this machine has `Zenith/Assets` baked while a fresh checkout does not (it is gitignored): (1) `Import-Module Build\...` without a leading `.\` is read as a MODULE NAME, killing four gates; (2) unset pinned cubemap/water textures reached the recorder's `BindSRV` assert -- a process KILL -- now falling back to the procedural white texture; (3) `Zenith_AssetHandle` caches only SUCCESS, so the missing default font was re-resolved EVERY frame by `Zenith_FontAsset::GetActiveOrDefaultMetrics` (~25k attempts in 180 s) and dp/cb TIMED OUT rather than failing. **★ The method that found them: reproduce CI's condition locally** (`Zenith/Assets` temporarily hidden, batches re-run) rather than iterating through 20-minute CI round trips -- CB 45/45, DP 158/158, ZM 42/42, zero asserts under that condition. Two asserts observed and deliberately NOT scoped in: asset-less Combat (missing `.zanim`) and RenderTest (null `LOAD_MODEL`); no workflow runs an automated batch for either game.

**★ ZM-D-180 (2026-08-01): floating NPC name tags shipped, NO baseline moved.** Every
`ZM_Interactable` now draws its `ZM_NpcData::m_szDisplayName` above its head each frame
(`ZM_Interactable::SubmitNameText`, called from `OnUpdate`), reusing RenderTest's tennis
scoreboard world-to-screen text idiom (project through `GetViewProjMatrix()`, submit via
`Zenith_UICanvas::SubmitText`) rather than a new engine primitive. No new component, no
serialized field, no scene bytes moved. Headless registry stayed **56/0**; a full
Dawnmere authoring boot and a windowed `ZM_NpcTalk_Test` (85 frames with the tag ticking
on-screen next to the villager) both ran clean. Detail in `DecisionLog.md` ZM-D-180. This
is orthogonal to the S8 content items below -- not part of that gate.

## Current task

**★★ S7 IS CLOSED (2026-07-29, ZM-D-167). Both boxes at `Roadmap.md:104` and `:172`
are ticked and the gate at `Roadmap.md:185` is annotated MET.** This section previously
said "FINISH S7" and stayed that way after the stage closed; it is the section a session
acts on, so it outranked the Stage line above it and pointed the next session at finished
work. Fixed 2026-07-29.

**★★ THE ZM-D-168 VISUAL-AUDIT FOLLOW-UP LANDED 2026-07-30 AS ZM-D-169** (built, fully gated
including the CROSS-GAME engine gate, and committed). Shortfalls **1.8-3c is CLOSED on pixels**
and the suite has its first pixel-level assertions. Observed: ZM headless **50/0**, full windowed
**50/0 with ZERO skipped**, ZM boot **2742 / 2741 / 0 / 1 UNMOVED**, engine boot **1164 / 1163 /
0 / 1 UNMOVED**.

**★★ AND ZM-D-170 LANDED 2026-07-30 ON TOP OF IT, CLOSING AUDIT FINDING 2 ON PIXELS.** GAME-ONLY
(one file: `Tests/ZM_AutoTests_BattleMenu.cpp`), so no cross-game gate was owed. Observed: ZM headless
**50/0**, full windowed **50/0 with ZERO skipped**, ZM boot **2742 / 2741 / 0 / 1 UNMOVED** -- **no
count moved, so none of the four baseline sites was edited.** The battle HUD and both arena creature
models are now asserted in pixels rather than from test names. **★ THE TRANSFERABLE FINDING IS NOT
THE RESULT, IT IS THE SHAPE OF THE HOLE:** `ZM_BattleMenuRun_Test` had been dwelling 90 frames and
writing a real swapchain TGA for a whole commit while asserting only that the FILE EXISTED. Evidence
produced and never read reads as coverage and is not. Before trusting any visual claim, grep for a
capture that no assertion opens. Second finding, from the mutation battery: **one arm per claim is not
enough** -- with both models dropped, the body-vs-background arm still PASSED on the player side
(0.834/0.927, that point lands on pale stone) and only the arm written as a sample-placement guard
caught it. Full detail in ZM-D-170.

**★ THE CURRENT TASK IS S8's FOUR CONTENT ITEMS** (`Roadmap.md:209-212`; the older `:198-201`
citation drifted when the file was edited). **The S8 go/no-go is NOT the next step** -- per
`Roadmap.md:207-214` that gate FOLLOWS those four items rather than preceding them, and it is a
HUMAN stop that no agent may sign.

**★★ S8 ITEM 1 ("Intro -> lab -> starter choice", `Roadmap.md:209`) IS IN PROGRESS. FOUR
SUB-COMMITS HAVE LANDED (ZM-D-174/175/176/177); THE BOX IS NOT TICKED AND MUST NOT BE.**

*LANDED so far:*
- **ZM-D-174** -- the `ProfLab` interior shell authored to `Assets/Scenes/ProfLab.zscen`
  (committed, per ZM-D-148), build index **41 registered** (closing a live warp wedge), 8 placement
  boot units, `ZM_ProfLabWarp_Test`.
- **ZM-D-175** -- the starter **DATA layer** and the seed split: `ZM_MakeStarterGameState` DELETED
  into `ZM_MakeNewGameState()` + `ZM_ApplyStarterChoice()`, plus the empty-party battle predicate
  and trainer engage gate landed while provably inert. Behaviour-preserving.
- **ZM-D-176** -- a **USER SCOPE RULING**: New Game now begins in **PlayerHome** (build 40, tag
  `"Door"`), and the home interior is tinted warm so it no longer reads as the same greybox room as
  ProfLab. The whiteout constants deliberately did NOT follow.
- **ZM-D-177** -- the tint pixel probe's absolute framebuffer bounds retracted as a false premise;
  its relative separation kept unchanged.

*STILL NOT BUILT -- what item 1 needs before its box may be ticked:* the Dawnmere lab **EXTERIOR**
+ door trigger + `FromLab` spawn; **Professor Aster** (NPC row + palette + authored into ProfLab --
note the review found Mom/Maren's palette collides at EXACTLY 0.0000 with the Villager's, so any
new human owes a palette re-author); the **starter-choice SCREEN** -- the model and the grant both
ship, so only the presenter is missing; and the **intro beat** itself. There is no cutscene or
movie system anywhere in the codebase, and none is in scope: "Intro" here means a playable
gameplay beat.

**★ SC1 CLOSED A LIVE WEDGE THAT WAS ALREADY SHIPPED.** `ZM_WorldSpec` has carried the ProfLab row
(build index 41, INTERIOR, tag `"Door"`) for some time, and `IsWarpDestinationValid` consults ONLY
that compiled tag list -- never the actual scene registry. So `RequestWarp(41, "Door")` returned
**true** on master while index 41 was unregistered, parking the machine forever in
`WAITING_FOR_SPAWN` with the player frozen behind a fully opaque fade. No test could see it because
boot units run BEFORE `Project_LoadInitialScene`. Registering 41 closes it.

**★ AND THE ONE THING SC1 DELIBERATELY DID NOT DO: it did not tick anything on the strength of a
green test.** `ZM_ProfLabWarp_Test` was mutation-proven live, not argued: pointing
`RegisterSceneBuildIndex(41, ...)` at `PlayerHome` still completes a warp, spawns the player, readies
the camera and re-enables movement -- every generic clause stays green -- and the test **RED** anyway
on its ProfLab-specific entity clause (observed `FAIL exit=1`, 31 frames), then went green again on
revert. A generic "a warp completed" assertion would have survived that mutation and proved nothing.
**Booked honestly:** the harness swallows the child process's stdout and
`ZM_ProfLabWarp_Test.json` reports `"failures": []`, so the failure text the test carefully composes
is not readable from the result artifact -- a real instance of the project's own
"evidence produced but never read" pattern, now recorded in Shortfalls rather than left as a surprise.

**★★ AND SC1's GATE FOUND A PRE-EXISTING DEFECT ON master THAT IS NOT SC1's -- PROVEN BY A CONTROL
THAT DID NOT FLIP. See Q-2026-08-01-002.** A windowed boot re-authors `Dawnmere.zscen` and leaves it
dirty, violating CLAUDE.md's "a boot must NOT leave a scene modified" invariant. Exactly **two bytes**
differ (offsets 3627/3635), both low-order mantissa bytes of `Npc_RivalVesper`'s authored rotation
quaternion -- 1 ULP in y, ~10 ULP in w; position, scale, every other entity and the navmesh are
byte-identical. **Both arms were run rather than one:** two windowed boots of the SC1 build agreed
byte-for-byte, and a `git stash`-ed, regenerated, rebuilt **clean HEAD** produced the *same* hash
`F403A489D0B11C77...`. The control REPRODUCED instead of flipping, so ProfLab changes Dawnmere's
bytes not at all -- master's committed scene simply no longer reproduces from master's own source.
`Dawnmere.zscen` was RESTORED and excluded from the commit; the cause is un-diagnosed and booked.

**★★ DIAGNOSED AND FIXED 2026-08-01 AS ZM-D-179 -- AND THE FRAMING ABOVE WAS BACKWARDS.** The
question asked why today's boot no longer reproduces master's bytes. `git show <commit>:<path>` at
each of the four commits that ever wrote this file answers it the other way round: the quaternion is
`0x3F7926D9 / 0x3E6B4456` at `012b04bc`, `dcabda50` **and** `1abbc440`, and
`0x3F7926D8 / 0x3E6B444C` only at `a6c66b68` (ZM-D-173). Today's boot makes it four-for-five.
**HEAD's committed bytes are the outlier; the source never changed.**
- **Cause: (b).** `Zenith_TransformComponent::WriteToDataStream` serialized the **live Jolt body's**
  pose (`GetRotation` returns the body whenever one exists), so a tracked asset was a function of
  physics state. Vesper is the only authored entity here with both a non-identity rotation and a body
  that keeps one -- the DYNAMIC CAPSULE ZM-D-156 forced on him. Jolt's quaternion paths are not
  value-preserving at float precision, and `Zenith_Physics::EnforceUpright` (called on this body by
  `ZM_Interactable::ApplyDrivenBodySetup`) round-trips quat -> forward -> `JPH::ATan2` ->
  `sRotation` through Jolt's own polynomial trig.
- **Cause (a) is DISPROVED, not merely unsupported.** The committed pair is not `sin`/`cos` of any
  single float yaw -- `y` implies a yaw of `0x402B6372/73`, `w` implies `0x402B6375/76`, three yaw
  ULPs apart. No codegen of `angleAxis(<a float yaw>)` emits a self-inconsistent pair, so the bytes
  never came out of the authoring computation. **"It's only two mantissa bits of codegen drift" was
  the wrong ending, and the bytes say so.**
- **The fix** serializes the transform's own cached pose unless the body moved past
  `PhysicsPoseDiffersFromCache` -- the engine's OWN threshold, the one the post-physics sweep already
  uses to decide whether to believe the body at all. Two engine units pin both branches
  (mutation-proven: transposing the polarity reds exactly those two and nothing else), and a
  tools-only guard serializes the rival's transform for real and bit-compares it with
  `ZM_DawnmereVesperFacing()` immediately before `AddStep_SaveScene`.
- **★ ON THIS MACHINE THE DIVERGENCE IS DORMANT** -- an instrumented boot logs
  `authored == serialised == liveBody == (0, 3F7926D9, 0, 3E6B4456)` -- so the fix moved **no byte**
  of today's output. The bytes committed with ZM-D-179 are the ones four of the five boots produced.
- **What was NOT established, and should not be quietly upgraded later:** the ZM-D-173 binary cannot
  be re-run, so *which* physics write produced those exact bits was never witnessed.

**★ ZM-D-169 LEFT TWO THINGS OPEN AND ZM-D-171 CLOSED BOTH ON 2026-07-30. Corrected here
2026-08-01 -- this block still asserted both as open, in the section a session acts on.** What it
said, and what is actually true:
1. **"The greybox SHADING defect is NOT fixed."** It is, at ENGINE level. ZM-D-171 made the
   ambient physically grounded, so vertical faces carry real sky+ground light -- measured, not
   argued: sun-averted face luminance **0.061 -> 0.1672**, unlit/lit **0.124 -> 0.3688**, blue/red
   **~7 -> 1.13** on the DawnmereHomeShell (`ZM_ShellLighting_Test`). It benefits every blockout --
   walls, floors, doors, lintels, props -- not only the six NPCs.
2. **"`m_eHuman` no longer solely determines how an NPC reads on screen."** It does again.
   ZM-D-171 **DELETED** ZM-D-169's emissive floor and its `ResolveNpcReadabilityTint` second
   colour source, restoring W4's "one row, one appearance" single-source property, so
   `Npc_AuthoredAppearancesAreMutuallyDistinct` is once more scoring the only table that decides
   appearance.
**★ WHAT IS GENUINELY STILL OPEN IS NARROWER AND IS ART DATA, NOT LIGHTING:** the authored
palette's minimum RENDERED pairwise separation under honest lighting is **0.0763**
(Caretaker/Wanderer) -- the 0.15 framebuffer promise was only ever met via the emissive hack.
★ **SUPERSEDED AT ZM-D-181:** the palette is no longer what an NPC normally wears (it is the
COLD-START FALLBACK's colour), and `ZM_NpcRenderedPalette_Test` -- which pinned this at a 0.04
floor -- was DELETED rather than re-baselined against content nobody had characterised. The
0.0763/0.15 figures remain true of the fallback. Booked in Shortfalls 1.8-4.
**Nothing now reads real swapchain pixels off an NPC body**; see TestPlan 5.8.

**THE S7 QUEUE AS IT NOW STANDS -- ALL FIVE ITEMS RESOLVED OR RE-HOMED. Kept for the
audit trail; nothing here is live work:**

1. **`Roadmap.md:104` -- `freeze input` + `approach` -- ✅ DONE 2026-07-29 (SC1-SC3,
   ZM-D-163/166/167).** Both cut verbs ship: SC1 the pure callerless
   `ZM_TRAINER_SIGHT_APPROACHING` state + approach maths, SC2 the `ZM_TrainerCinematicLatch`
   freeze owner (arbitrated by NAME in the one existing guard, deliberately un-refcounted),
   SC3 the live drive on a `CAPSULE`/`DYNAMIC` Vesper. Observed: 7.575 m -> 2.105 m into a
   2.0 m standoff in 43 frames / 1.467 s, `worstBackstep=0.0000`, player frozen all 43
   frames and released in 1, `facingAbsDot=1.00000` off the RE-AUTHORED bytes. **★ THE
   CAMERA CUT IS NOT CLAIMED BY THAT TICK** -- absent from the box's text, a W3 aspiration
   only, still unbuilt, still booked in Shortfalls 1.8. The recorded "needs a real
   engine/game-camera feature" blocker was DISPROVED: `ZM_FollowCamera` is a Zenithmon
   component (order 103) and the sole writer of the camera pose.
2. **Spotted marker off the DEBUG primitives channel** (Shortfalls 1.8-3c) -- **✅ DONE
   2026-07-30 (ZM-D-169, committed `ffcc20af`); 1.8-3c IS CLOSED, ON PIXELS.** This item read
   "⏳ IN FLIGHT, UNCOMMITTED. Not done; do not tick 1.8-3c" until 2026-08-01, describing a
   working tree that had been built, gated and committed two days earlier -- and pointing at an
   "In-flight working tree" section that no longer exists. Observed at closure:
   `ZM_RivalVesperAuthored_Test` holds `Graphics/Primitives/Enabled` FALSE for the whole run and
   a frame-exact swapchain dump on a real SPOTTED frame carries **118 marker-hue px spanning
   7x28**, that hue unique frame-wide; mutation-proven by restoring the old
   `if (!m_bPrimitivesEnabled) return;`. **The W3 property was preserved and still binds:**
   the gameplay submit RETURNS a count measured off Flux's own CPU instance queues and callers
   `+=` it -- a bare `++` beside the call is exactly the defect that left every test green with
   nothing drawn.
3. **Duplicate NPC appearance -- ✅ DONE 2026-07-29 (ZM-D-164).** `ZM_NPC_ROUTE_WARDEN` now
   names the new append-only `ZM_HUMAN_TOWN_WARDEN` (`WORKER`+`BLONDE`), nearest authored
   neighbour 0.20661 against the 0.15 floor. **★ IT WAS NOT ONE TOKEN AND THE RECORDED FIX
   WAS FALSE:** `ZM_HUMAN_TRAINER_RANGER` sits **0.0873** from the Villager and RED the
   gate when built -- the "it can only RAISE the count, so it cannot red anything" claim in
   this file, `Shortfalls.md` and ZM-D-160 read a two-armed unit as a pure counter. The
   boot unit was also RATCHETED (`uMIN_DISTINCT_APPEARANCES` `5u` -> `ZM_NPC_COUNT`) after
   the old literal was *demonstrated* inert against the very regression it guarded.
   Observed: boot **2731 / 2730 / 0 / 1 -- zero delta**, headless 49/0, windowed gallery
   1/0 (run deliberately: a 36th human staled the HUMANS bake manifest and the headless
   gate is blind to that, since the gallery skips for "requires graphics", never staleness).
4. **The BOX (storage) screen -- ✅ RESOLVED 2026-07-29 by explicit RE-DEFERRAL (ZM-D-165,
   Q-2026-07-29-001).** It is now booked at `Roadmap.md:212` under S9, with the reason
   recorded: the storage MODEL has persisted 16x30 boxes since ZM-D-136, so what is missing
   is only a presenter in the S6 `Source/UI/ZM_UI_*` by-value idiom (no schema change, no
   ECS order, no serialization bump -- it lands whenever with zero rework). It belongs in S9
   because a box is only testable once the player can exceed a full party. **Do not let this
   deferral expire unremarked a second time.**
5. **Doc reconciliation -- ✅ DONE 2026-07-29, verified item by item rather than assumed:**
   the S7 `*Gate:*` line now carries its MET annotation (`Roadmap.md:185`), restoring the
   S0/S1/S2/S4/S5/S6 pattern; `Shortfalls.md`'s verdict block was re-dated and its
   "next autonomous work" corrected (it had gone stale a SECOND time -- see the tripwire
   below); the "36 tests / 2392 units" figures are covered by the trust-this-line disclaimer
   in `Shortfalls.md`'s "CURRENT VERIFIED BASELINE" bullet, whose own number is now 2742
   (cited by NAME, not line -- the line numbers in this list drifted the moment the block was
   edited, which is why the original item 5 pointed at `Roadmap.md:163` for a gate that had
   moved to `:185`); the false open-gap entry for
   Q-2026-07-21-001 is gone; this file's "Open Questions" pointer was rebuilt (it had named
   2 CLOSED ids and omitted 9 open ones); and
   `Tests/ZM_AutoTests_SaveContinue.cpp:25` no longer claims graphics-required -- its
   registration passes `false`, so it RUNS on the Null backend and IS CI-visible.

**★ THE TRIPWIRE THIS ITEM EARNED: A PROSE SUMMARY GOES STALE TWICE.** `Shortfalls.md`'s
verdict block was rewritten on 2026-07-29 by ZM-D-165 *specifically because* it had been
eight days stale -- and by the end of the same day it was wrong again, still asserting "S7
IS NOT COMPLETE" and still listing SC2/SC3 as remaining after both had landed. Three files
also held three different boot baselines at once (2722 / 2731 / 2742). **The structural
lesson: a stage-closing commit must update the Roadmap checkbox AND every prose summary
that names the stage, in the same commit** -- otherwise the next session inherits a
confident sentence that outranks the box, which is the exact failure ZM-D-162 was written
to prevent and which recurred anyway.

**Baseline at S7 closure (2026-07-29, ZM-D-167), superseding the pre-SC1 figures this
paragraph used to carry:** Vulkan_True and Null_True both exit 0; headless registry
**49 passed / 0 failed**; full windowed Vulkan **49 passed / 0 failed**; boot unit gate
**2742 ran / 2741 passed / 0 failed / 1 skipped** -- equal to the pinned `zm-tests.yml`
baseline, so the tree is green and unchanged.

## The ZM-D-168 follow-up -- BUILT, GATED AND COMMITTED 2026-07-30 as ZM-D-169

**★ THIS SECTION IS NO LONGER LIVE WORK.** It is kept because the inventory and the three traps
below are the record of what was landed and why; the OBSERVED results are at the end of it and the
full entry is ZM-D-169. The prose below still describes the diff in the future tense of the session
that wrote it -- read it as a description of what shipped, not as a plan. Modified:
`Components/ZM_Interactable.{h,cpp}`, `Tests/ZM_AutoTests_BattleMenu.cpp`,
`Tests/ZM_AutoTests_RivalVesper.cpp`, `Tests/ZM_Tests_Interactable.cpp`, `Zenithmon.cpp`,
`Tools/capture_viewport.ps1`, and **six `Zenith/Flux/Primitives` + `Zenith/Flux/Shaders`
files**. New and untracked: `Tests/ZM_TestTGAHelpers.h`.

It is the follow-up to ZM-D-168's visual audit, in two threads:

1. **The SPOTTED marker moves off the debug-primitives channel** (targets Shortfalls
   1.8-3c + audit finding 3). `Flux_PrimitivesImpl` gains two dedicated GAMEPLAY queues plus
   `SubmitGameplayCylinderAndSphere()`, which returns 1 only if BOTH queues grew -- the W3
   measured-off-Flux property, preserved. `ExecuteGBuffer` now drains the gameplay queues
   UNCONDITIONALLY (the early-return fires only when debug and gameplay are both empty),
   closing both the lost-cue and the queue-leak halves. The shader gains
   `m_fEmissiveIntensity`; non-zero selects `GBUFFER_SHADING_UNLIT`, gameplay draws pass
   0.5, debug draws pass 0 and keep the old matte path. The stem changes from Flux's flat
   debug LINE quad to a solid CYLINDER, which is the audit's "sphere + diagonal stroke".
2. **The suite gets its first pixel-level assertions** (audit findings 1 and 2, whose stated
   rule was "rendering claims need pixels"). New `ZM_TestTGAHelpers.h` reads
   `Flux_Screenshot`'s BGRA TGA; new **`ZM_NpcRenderedPalette_Test`** loads committed
   Dawnmere, teleports the six live NPC model entities into an eye-level lineup, dumps the
   real framebuffer and measures mean RGB + rendered pairwise separation per body;
   `ZM_AutoTests_BattleMenu.cpp` captures an `ACTION_ROOT` frame for the HUD half.

**★ THREE THINGS THE NEXT SESSION MUST NOT GET WRONG -- ALL THREE HELD; kept as the record:**
- **This touches `Zenith/Flux`, so it is an ENGINE change and owes the CROSS-GAME gate,** not
  just Zenithmon's. The engine baseline **1164** is in scope for the first time since W1-W5
  (every one of which was game-only). Combat / CityBuilder / DevilsPlayground / RenderTest /
  TilePuzzle all render debug primitives. **DONE:** three sentinels built + all exit 0, engine
  boot units **1164 / 1163 / 0 / 1 UNMOVED**, all five games clean on `Null_True`, Combat headless
  **14/14**.
- **`ZM_NpcRenderedPalette_Test` is a NEW registration, so the registry moves 49 -> 50** --
  headless and windowed both. The rewritten interactable unit MODIFIES an existing unit
  rather than adding one, so boot may stay 2742; take the OBSERVED line, never this sentence.
  **BOTH PREDICTIONS HELD:** registry **50** in both configs, boot **2742 UNMOVED**.
- **`Build\regen.ps1` is NOT owed.** The only new file is a HEADER. Regen is for a new `.cpp` or
  folder. **Correction (verified by grep, 2026-07-29):** this said "included by two existing TUs";
  `ZM_TestTGAHelpers.h` is included by exactly ONE -- `Tests/ZM_AutoTests_RivalVesper.cpp`.
  `ZM_AutoTests_BattleMenu.cpp` takes `Flux/Flux_Screenshot.h` and checks its capture with
  `DiskFilePresent`, never reading a pixel. The no-regen conclusion is unaffected; the count was not.

**★ AND ONE DESIGN CALL THAT NEEDS A RULING, NOT A SILENT LANDING.** Audit finding 1's cause
was diagnosed as a SHADING gap -- near-overhead sun, no meaningful ambient/indirect term, so
the vertical faces a player actually sees get almost no light. The tree does not fix that.
Instead `ZM_GreyboxVisual::ApplyAppearance` gives NPCs an emissive floor: 20% of the authored
`m_eHuman` palette colour blended with 80% of a NEW row-keyed `ResolveNpcReadabilityTint`
(six hard-coded hues). That is a game-side readability workaround, and it introduces a
SECOND colour source alongside W4's palette -- so `m_eHuman` no longer solely determines how
an NPC reads on screen. Book it as such in ZM-D-169; do not let it read as "the shading
defect is fixed", and do not let it silently supersede W4.

**★ GATED AND LANDED 2026-07-29 as ZM-D-169. THE PREDICTIONS ABOVE ARE SUPERSEDED BY THE
OBSERVED LINES BELOW; read these, not them.**
- **`zenith build Zenithmon` (Vulkan_True) and `--headless` (Null_True): both clean.** No regen
  was run and none was needed.
- **Headless registry: `50 passed, 0 failed (of 50)`.** The predicted 49 -> 50 HELD --
  `ZM_NpcRenderedPalette_Test` is a genuinely new registration. It SKIPS headless
  (`m_bRequiresGraphics = true`), and a skip counts as a pass, so the headless 50 does not mean it
  ran there.
- **Full windowed Vulkan: `50 passed, 0 failed (of 50)`, ZERO skipped** -- every test including
  both new pixel tests ran for real.
- **Boot unit gate (Null exe, `-TimeoutSec 600`): `2742 ran, 2741 passed, 0 failed, 1 skipped`.
  UNMOVED.** The prediction held: the interactable unit was REWRITTEN, not added. **So no baseline
  moved and nothing was edited in any of the three sites** -- `Status.md`, `zm-tests.yml` and
  `Shortfalls.md` all already read 2742 and still do. The three-way drift this file warns about was
  avoided by changing nothing, which is the correct action when the count does not move.
- **The two risks flagged before the run are both DISPROVED, not merely unobserved.**
  (1) `NRPHoldLiveNpcLineup`'s per-frame `SetPosition` holds against the physics-to-transform sync:
  all six bodies were sampled at their projected NDC and returned body colour, not background.
  (2) `ZM_BattleMenuRun_Test`'s 90-frame `ACTION_ROOT` dwell fits inside the battle leg's deadline
  -- it passed windowed with the dwell asserted at exactly 90.
- **W4's `blocksOffGrey=0` arm is unmoved, as predicted:** observed
  `blocks=4 blocksOffGrey=0 npcBodies=6 npcStillGrey=0 paletteError=0.000000 vsGrey=0.6366
  vsNearestNpc=0.2124`. The emissive floor's `SetEmissiveIntensity(1.0f)` really is a no-op for
  non-NPC blockouts, because 1.0 is already the `Zenith_MaterialParamTable` default and the emissive
  COLOUR stays `(0,0,0)`.
- **★ 1.8-3c IS NOW CLOSED, ON PIXELS.** `ZM_RivalVesperAuthored_Test` holds
  `Graphics/Primitives/Enabled` FALSE for the whole run and now takes a frame-exact
  `Flux_Screenshot::RequestDump` from inside a real SPOTTED frame. Observed: **`SPOTTED marker
  OBSERVED IN PIXELS: 118 marker-hue px spanning 7x28 in a 1280x720 swapchain capture`** -- an
  upright exclamation mark over the rival, so the CHANNEL (1.8-3c) and the SHAPE (audit finding 3)
  are both settled by the same capture. **Mutation-proven:** restoring the old
  `if (!m_bPrimitivesEnabled) return;` reds it with "reached Flux's queues but NOT the framebuffer:
  0 marker-hue pixels" and exit 1; reverting returns to green.
- **★ AND THE AUDIT'S "UNEXPLAINED NULL RESULT" WAS THE MEASURING APPARATUS, TWICE OVER. Read this
  before trusting any future screen scrape.** The marker was drawing the whole time.
  1. **The colour predicate was wrong.** A gameplay draw submitting linear `(1.0, 0.82, 0.08)`
     unlit at 1.5x sounds like "saturated yellow, almost no blue". Measured off the actual bytes it
     is **`RGB(208, 182, 97)` -- blue/red 0.47, not 0.08.** Two hand-rolled scans keyed on "low
     blue" reported **zero marker pixels across 539 frames** of two different tests while it
     rendered perfectly. The false negative was one step from being written up as a render defect.
  2. **The sampling rate was unreachable.** `capture_viewport.ps1 -IntervalMs 40` delivered
     **206 ms** at 2560x1440 and 81 ms at 1280x800 -- PNG encode dominates the loop. Its own header
     advises "60 ms or lower", which the script cannot honour at full size.
  **The rule: a null result from a hand-rolled screen scrape is evidence about the scrape.** Use the
  engine's frame-exact dump and derive the predicate from the bytes it writes. The new assertion
  does both, and records that the marker hue is UNIQUE frame-wide (118 matches, all inside the
  marker's own 7x28 box, zero elsewhere) so the threshold is defensible rather than tuned.
- **CROSS-GAME gate, owed because this touches `Zenith/Flux`:** SentinelECS / SentinelPhysics /
  SentinelAI all built (`Vulkan_..._False`) and all **exit 0**; engine boot units on Null Combat
  **1164 / 1163 / 0 / 1 UNMOVED**; Combat / CityBuilder / DevilsPlayground / RenderTest /
  TilePuzzle all **clean on `Null_True`**; and because the change is in the SHARED primitives
  drain, three of them were also RUN headless rather than only compiled -- Combat **14/14**,
  CityBuilder **45/45**, DevilsPlayground **158/158**.
- **Ratchets: both still pre-existing RED, and this commit adds nothing to either.**
  `architecture,lints` fails on `Zenith_TerrainComponent` (EC->Flux edge + `std::vector` /
  `std::function`) and per-file `g_xEngine` counts in `Zenith_TerrainEditor.cpp` and
  `Zenith_EditorAutomation.cpp`; `complexity` fails on `ParseCommandLine`,
  `ValidateTerrainGridTopology`, `ZENITH_PROPERTY` and duplicate-clusters=10.
  **Every failing finding names a file this commit does not touch**, and duplicate clusters are at
  the 10 this file already recorded. The `Flux/Primitives/Flux_PrimitivesImpl.h` mentions in the
  report are pre-existing ALLOW-LISTED EC->Flux edges (`Zenith_ColliderComponent`,
  `Zenith_AIWorldHooksInstall`, `Zenith_PhysicsDebugDraw`) plus informational file-size rows --
  not new findings. `ExecuteGBuffer` grew but is not in the refactor queue's top 20 and added no
  duplicate cluster despite the two new emissive-parameter render calls. **NOTE the evidence
  standard here is weaker than the pristine-HEAD-worktree comparison earlier commits used:**
  ZM-D-031 forbids worktrees, so this is a finding-by-finding check of the failure list, not a
  byte-identical diff of two reports.
- **`Games/Zenithmon/Assets` byte-clean** after the full windowed batch -- no scene moved, which is
  the ZM-D-148 boot-shape-independence property still holding.

**★ AND ONE ENGINE DEFECT FOUND BY READING, WHICH EXPLAINS AUDIT FINDING 3 EXACTLY.**
`Flux_PrimitivesImpl::RenderLinePrimitives` translates the line quad to `m_xStart` while
`GenerateUnitLine` spans local y in `[-1, 1]`, so `AddLine(A,B)` draws from `A - dir*len/2` to
`A + dir*len/2` -- centred on the START, overhanging behind it and stopping short of `B`.
`RenderCylinderPrimitives` translates to the MIDPOINT, so the two verbs do not cover the same
segment. The old marker asked for a stem at `top+0.55 .. top+1.20` and got `top+0.225 .. top+0.875`,
straight through the dot at `top+0.25`: that is the audit's "gold sphere with a diagonal stroke",
and the flat +Z-facing quad (`ComputeYAxisAlignment` never rolls toward the camera) is the rest of
it. **Deliberately NOT fixed here** -- it changes debug draws in all five other games and every
`Add*` composite built on `AddLine` (cross/circle/arrow/cone/arc/polygon/grid/axes), so it owes its
own cross-game gate. Booked as `task_33ee8059`. The cylinder swap in this diff sidesteps it.

## S7 closure detail and standing lessons (HISTORICAL -- none of this is live work)

Everything from here down is the record of how S7 closed and what it bound for future work.
It is not a task list. **The live task is S8's four content items (the unticked boxes under
Roadmap's "S8 -- Vertical slice, go/no-go" heading; item 1 is IN PROGRESS, items 2-4 unstarted) -- see
"Current task" above.** This sentence used to point at an "In-flight working tree" section, which
was removed when ZM-D-169 was committed on 2026-07-30, leaving a dangling pointer; fixed
2026-08-01.

**★ QUEUE ITEM 1 IS COMPLETE -- SC1 (ZM-D-163), SC2 (ZM-D-166) AND SC3 (ZM-D-167) ALL
LANDED, AND SC3 CLOSED S7.** SC2 adds
`ZM_TrainerCinematicLatch` as a FOURTH freeze owner, arbitrated by name in
`ZM_UI_MenuStack::UnfreezePlayer`'s existing guard, **with no refcount on purpose** (one `End()`
always releases; a counter turns one missed `End()` into a permanently frozen player) and
`IsActive()` spelled against the registry so an unnameable owner reads INACTIVE. Observed:
headless **49/0** with `ZM_TrainerSightWalkUp_Test` **754 -> 782 frames** (the frame delta is how
we know the new phase RAN), boot **2731 -> 2735 / 2734 / 0 / 1**. **★ SC3 OWES A DEBT FROM SC2:**
deleting the between-tests latch reset currently reds NOTHING, because `Begin()` has no runtime
caller yet. Phase 7a1 leaves a tripwire armed -- **SC3 must add a batched case that exits
mid-`APPROACHING`**, which is what finally gives that mutation teeth.

**★ SC1 DETAIL (ZM-D-163).** `ZM_TRAINER_SIGHT_APPROACHING` is
appended at ordinal 4 with the pure `ZM_StepTrainerApproach`, **callerless** (the ZM-D-153
shape), so `m_bApproachPossible` defaulting false leaves all 26 shipped FSM units unmodified.
Observed: headless **49/0** (unmoved), boot **2722 -> 2731 / 2730 passed / 0 failed / 1
skipped**, every committed `.zscen` byte-unchanged after a boot running all 216 authoring
steps. **All of SC2 and SC3 have since landed (ZM-D-166/167); nothing remains for queue item
1.** Two corrections this paragraph's original text got wrong and which are worth keeping:
Vesper did NOT stay OBB -- SC3 moved him to `CAPSULE`/`DYNAMIC` precisely so he could be
driven, and his authored yaw survived it (`facingAbsDot=1.00000` off the re-authored bytes).
And **SC4 (the camera cut) was never part of `Roadmap.md:104`'s text**, so it did not block
the tick; it is unbuilt and stays booked in Shortfalls 1.8.

**★ SC4 REVERSES A RECORDED DECISION ON NEW EVIDENCE, AND THAT IS DELIBERATE.** ZM-D-159 cut
the camera cut as needing "a real engine/game-camera feature". A code survey found that
unsupported: `ZM_FollowCamera` is a **Zenithmon component (order 103)** and is the SOLE writer
of the camera pose, so the override belongs INSIDE it and **no `Zenith/` change is required**.
Reversing a logged decision on evidence is what the DecisionLog is for; SC4's own entry must
say so plainly.

**★ THREE TRAPS THIS SESSION PAID FOR -- read before repeating the work:**
1. **A totality walker that does not iterate `_STATE_COUNT` is not one.**
   `Fsm_StepNeverAssertsOnAnyDegenerateInput` hand-builds four seed fixtures; only its NAME
   walks iterate the enum. A newly appended state was invisible to it while looking covered.
   **The next appended state must add its own sweep.**
2. **One sample per arm measures the machine, not the change.** The boot suite was timed at
   175s (2722 units) then 193s (2731) and the delta blamed on the new tests -- then the
   *trimmed* build measured 229/235s with 83% fewer operations. Variance exceeds the effect.
   The suite is **per-test-fixture bound** (`Zenith_TestResetGlobalState` runs once per test;
   2722 x ~64ms is essentially the whole runtime), so test BODIES are noise and adding a unit
   costs ~64ms regardless of what it does. Take a second sample before accusing a diff.
3. **A file restored with `Copy-Item` keeps its old `LastWriteTime`, so MSBuild skips it.**
   That leaves a STALE `.obj` and a green build against a binary lacking the code under test.
   It only surfaced as a link error because a second TU in the same build had also changed.
   Touch restored files, or clean. Same silent-success family as a skipped `regen.ps1`.

**★ AND A PRE-EXISTING CI LANDMINE, NOW FIXED (ZM-D-163).** `run_unit_gate.ps1` defaults to a
**180s** watchdog and `zm-tests.yml` passed no `-TimeoutSec`. The watchdog is a HANG guard, but
because the units line must be logged before the kill it silently caps the suite's RUNTIME --
and the suite measures **175-235s**, i.e. it straddles the default and lands on whichever side
machine load decides. Losing that race reports `no 'Unit tests complete' line in boot output`,
which reads as a crash or loader failure and points at the wrong culprit. Now `-TimeoutSec 600`
in the workflow, with the coupling documented in the script header. **Unrelated to SC1; it
would have bitten whichever commit next added units.**

**What the vertical actually does now, end to end:** the player walks around Dawnmere;
rival Vesper stands AUTHORED in the town at (490, 524) facing the spawn approach; when
the player enters his 8 m / 60-degree forward cone with an unblocked line of sight a
yellow exclamation mark appears over his head for 0.35 s -- the player is never frozen
and can still walk out of it, which cancels cleanly -- then he speaks a challenge line,
the battle fades in, the player fights him, and on a win takes +500 money and sets
`RIVAL1_DEFEATED` -- after which he never re-engages, and that silence survives a
save/reload.

**★ THE HONEST "WHAT IS STILL MISSING" LIST -- read this BEFORE signing the go/no-go.
The vertical is real but it is not the full mainline beat, and it should not be
oversold:**
1. **PARTLY CLOSED by W3 (ZM-D-159).** The exclamation mark SHIPS -- a 0.35 s
   cancellable `ZM_TRAINER_SIGHT_SPOTTED` state drawing an asset-free yellow line+sphere
   over every sighted trainer, silent rows included, never freezing the player. **The
   camera cut and the approach walk were CUT ON EVIDENCE and remain missing:**
   `ZM_FollowCamera::OnLateUpdate` owns and overwrites the camera every frame with no
   override stack, and moving the OBB-authored stationary Vesper correctly needs
   dynamic-capsule/nav ownership, avoidance and freeze coordination. He still does not
   walk to you and the camera does not move. **And the marker rides the DEBUG primitives
   channel**, so a tools user who unchecks `Graphics/Primitives/Enabled` loses a gameplay
   cue; promoting it to a real UI/mesh surface is deferred.
2. **CLOSED by W4 (ZM-D-160).** `ZM_NpcData::m_eHuman` now feeds a TOTAL palette in
   `ZM_HumanAppearance`, and `ZM_GreyboxVisual` paints each NPC body with its own row's
   colour while every non-NPC blockout keeps the shipped grey byte for byte. Measured on
   the COMMITTED rival: `vsGrey=0.6366`, `vsNearestNpc=0.2124` against a 0.15 margin.
   **One content collision remains and is booked, not hidden:** `Npc_Wanderer` and
   `Npc_Warden` both stand on `ZM_HUMAN_TOWN_ELDER`, so six authored rows wear five
   appearances and those two are pixel-identical. The rival is distinct from all five, so
   W4's claim holds; the one-token roster fix is named in Shortfalls 1.8.
3. **Route 1 does not exist**, so the GDD's canonical location for rival battle 1
   ("Route 1, L5") does not either. The Dawnmere placement is a RECORDED deviation
   (Q-D / ZM-D-156) carrying a re-placement debt.
4. **The challenge `.bgraph` is gitignored**, so a fresh CI checkout loses the BARK and
   keeps the BATTLE (SC7's deliberate fail-open). The bark beat is exercised locally
   only, never in CI.
5. **No test proves the authored Vesper carries no graph slot at RUNTIME** -- it cannot,
   because the runtime attach is idempotent by path. The property rests on an
   authoring-time assert plus the boot-stability check.

**W5 closure, observed rather than inferred -- and it found a bigger defect than the limit
described.** There was never a "sample once": `fZM_DAWNMERE_TOWN_CENTER_FEET_Y` is a
hard-coded literal and all six NPCs plus both serialized patrol waypoints reused it. The
sampler could not have been called at authoring time anyway -- the editor add path uses
the terrain component's deserialization ctor, which never loads physics geometry, so
there is NO terrain body during authoring and a raycast would MISS. Heights are therefore
MEASURED at runtime and FROZEN as constants, which is also what the header's binding note
requires (committed `.zscen` bytes reproducible from compiled constants, not from a
gitignored bake). **★ THE OBSERVED TRUTH: the warden stood 1.368 m and the caretaker
1.095 m off their own ground, with a live terrain spread of 1.782 m under the roster.**
Dawnmere's square is not flat. **★ THE OBB TRAP IS UNREACHABLE BY CONSTRUCTION here:**
every change is one float inside an existing `AddStep_SetTransformPosition` argument at
plan time and `SetPosition` appears nowhere in the diff -- the detector
(`RVFacingAbsDot >= 0.999` off the SAVED rotation) still ran and passed against the
re-authored bytes. **★ THE CONTROL FLIPPED:** built with the placeholder table the gate was
2722/2719/**2 failed** (exactly the spread and distinctness units); pasting the seven
measured constants and changing nothing else took it to 2722/2721/**0**. Scene re-authored
with the full proof -- two `AUTHOR_DAWNMERE` boots, identical SHA256 `3874943E...`, exactly
one tracked asset moved, navmesh byte-unchanged, re-hash after the batches identical.
Boot **2716 -> 2722**, registry **48 -> 49**.

**W4 closure, observed rather than inferred.** `m_eHuman` feeds a TOTAL palette added to
the EXISTING `Source/Gen/ZM_HumanAppearance.{h,cpp}` (no new TU), derived from the SAME
outfit/hair tables the SC3 albedo painter uses so the blockout previews the eventual
human. `ZM_GreyboxVisual` resolves sibling `ZM_Interactable` -> row -> `m_eHuman` ->
palette; every non-NPC blockout keeps the shipped grey exactly (`blocksOffGrey=0`).
**★ THE ORDER TRAP, CHECKED NOT ASSUMED:** `OnStart` dispatches in ascending
serialization order, so the greybox (107) starts BEFORE `ZM_Interactable` (113). That
would be fatal for the TRAINER id, which IS derived in `OnStart` -- but the NPC ROW
arrives from `ReadFromDataStream` or the authoring step, so the greybox reads only the
row and never `GetTrainerId()`. The cost is that the stale-row CLAMP has not run yet,
which is why the explicit bounds check and the palette's totality are load-bearing.
**★ AN HONEST COVERAGE BOUNDARY:** `ZM_GreyboxVisual` is file-local to `Zenithmon.cpp`
and cannot be named from a `Tests/` TU, so NO boot unit can construct one -- the live
material scan is its only coverage, and mutation M1 demonstrates exactly that (automated
RED at 355 frames, boot gate cleanly GREEN). Boot **2712 -> 2716**, registry unchanged
**48**, four mutations proved teeth.

**W3 closure, observed rather than inferred.** `ZM_TRAINER_SIGHT_SPOTTED` is appended
(ordinal 3, session-only, serialized nowhere) between first sight and SC7's handoff. Arm
order is **cancel -> busy -> fail-open -> accumulate**: lost sight or a closed engagement
gate cancels to WATCHING clearing the partial timer; a busy channel PAUSES without
consuming the sighting and deliberately OUTRANKS the fail-open (raising into a busy
channel is silently dropped, so the fail-open is a FREE-TICK guarantee); a corrupt
duration fails open, decided BEFORE the state entry so `m_uSpottedCount` can never book a
beat no frame could show. **★ THE REVIEW'S HEADLINE, and the reason this is not a
proxy test:** the submit counter every automated assertion reads was first written as a
bare `++` BESIDE the submit call, so deleting the call would have left the whole live
contract green with nothing drawn. `SubmitTrainerSpottedIndicator` now RETURNS a value
measured off Flux's own CPU instance queues and the caller `+=`s it -- mutation M6
(remove the two `Add*` calls) reds one boot unit AND both automated tests, which before
the fix would have stayed green. **CUT ON EVIDENCE, not forgotten:** the camera cut
(`ZM_FollowCamera::OnLateUpdate` owns the camera every frame, no override stack) and the
approach walk (OBB-authored stationary Vesper would need dynamic-capsule/nav ownership,
avoidance and freeze coordination). Boot **2708 -> 2712**, registry unchanged **48**,
six mutations proved teeth.

**W2 closure, observed rather than inferred.** `ZM_RivalVesperWhiteout_Test` starts from
the canonical level-5 Fernfawn state, physically walks to committed Vesper, selects a
legitimate learned move through the live HUD, and reaches a natural ENEMY win with
Catch and Run unavailable. It proves no loss payout/flag/EXP; observes the pending
whiteout in the director/manager order boundary; dirties durable HP, PP and status;
then requires a full heal, exactly one TownCenter load, independently computed capsule
placement, a freshly derived WATCHING Vesper, and 200 no-input frames without a
re-challenge. The first attempt also disproved a fixture assumption: move slot 0 made
the player win with 13 HP, so the final test follows observed battle behavior instead
of forcing a result.

**W1 closure, observed rather than inferred.** `ResolveTurn` now performs a canonical
`DoSwitch` after `TURN_END` and the whole-party terminal scan whenever a fainted active
has a live reserve. Enemy selection comes from the trainer AI's private RNG/tactical
policy; player selection currently auto-promotes the lowest live reserve. Rambler
Perrin's complete two-member row is driven through a real trainer round trip, where the
lead `FAINT` is followed by exactly one reserve `SWITCH_IN` and the battle still pays
out; the presented switch also reloads the arena entity from the reserve's creature
model path. This retires the hard process break, lead-only clamp and stale-lead visual.
A player-facing party choice screen is not part of this repair; the engine path is live
for both sides.

**★ WHAT SC8 BINDS FOR ANY FUTURE SCENE WORK:**
- **An AABB collider DESTROYS an authored rotation.** `Zenith_ColliderComponent` forces
  an AABB body to identity (axis-aligned by definition) and the physics-to-transform sync
  writes it back. Any authored entity that must FACE anywhere needs
  `COLLISION_VOLUME_TYPE_OBB`. Vesper's call site carries a "do not tidy this back to
  AABB" warning; heed it.
- **A sub-commit that rewrites a committed scene owes the full operational proof:** a
  windowed `_True` boot logging `sceneAuthoring=AUTHOR_DAWNMERE` (a `DEFERRED` boot
  silently does nothing and looks successful), SHA256, a SECOND authoring boot, identical
  hashes, only the intended asset changed, and a re-hash after the batches to prove no
  play-session save baked a `Zenith_GraphComponent` payload in.
- **`ZM_TRAINER_RIVAL_VESPER == 0`** -- a `ZM_NpcData` row omitting its trailing
  initializer silently becomes the rival, with no compiler warning.

**Prior: S7 item 3 SC7 COMPLETE (ZM-D-155).** The game's FIRST `.bgraph` -- the trainer
challenge bark -- landed with ZERO scene bytes and ZERO new ECS orders.
`SC7_Plan.md` was consumed and DELETED in that commit, as it instructed.
**NEXT = SC8: rival Vesper authored in Dawnmere + trainer-id persistence.**

**★ SC7's SCOPE WAS CONTRADICTED ACROSS TWO BINDING DOCS AND THE USER DECIDED IT
(2026-07-28). Do not re-open this.** `DecisionLog.md`'s ZM-D-143 sequence block and
`Questions.md` **Q-B** both defined SC7 as the trainer-DEFEAT beat (`SetStoryFlag`
-> `AwardPrizeMoney`, fired from SC5's win callsite), while `SC6_Plan.md` -- and
therefore `Roadmap.md` and this file -- said the spot-bark beat. Both were
implementable and Q-B was a USER-ADOPTED default, so it was escalated to the user
rather than resolved by an agent. **Verdict: the bark**, because the defeat beat is
already shipped and test-locked in C++ as SC5's `ZM_ApplyTrainerResultToGameState`.
The full ruling, including the honest caveat that NEITHER shape gives the graph "one
genuine decision", is recorded against Q-B in `Questions.md`. The
defeat-beat-as-graph is NOT scheduled.

**The honest one-line description of what the vertical does in-game after SC7:** "a
trainer who sees you speaks, then battles you." There is still no walk-up-to-you
approach -- the battle starts from where the player stands -- and no trainer is
placed in a scene yet. Both are SC8's.

**What SC7 pinned that SC8 must not re-litigate:**
- **★ AUTHORED VESPER MUST NOT CARRY AN ATTACHED GRAPH SLOT.** The graph is attached
  at RUNTIME from `TickTrainerSight`, an `OnUpdate`-only path; `Zenith_Core.cpp:138`
  gates `Scenes().Update` on `EditorMode::Playing` and the boot authoring pass runs
  Stopped, so a `Zenith_GraphComponent` cannot exist during `AddStep_SaveScene`.
  `AddStep_AttachGraph` on `ZM_MenuRoot` was REJECTED (it would add an order-60
  payload to the committed `FrontEnd.zscen`). Vesper picks the graph up through the
  same runtime attach and `Dawnmere.zscen` never moves.
- **The battle is GRAPH-INDEPENDENT and FAILS OPEN.** `.bgraph` files are gitignored
  and tools-authored, so a `_False`/Android build or a fresh CI checkout has none.
  The FSM's `CHALLENGING` state raises the encounter anyway when
  `m_fChallengeConfirmSeconds` expires. **That window's polarity is DELIBERATELY
  OPPOSITE to `m_fRaiseConfirmSeconds`** (degenerate raise window = stay silent;
  degenerate challenge window = raise immediately). Do NOT unify them into a shared
  helper -- a unit pins both side by side precisely to stop that.
- **`m_bChallengeAvailable` DEFAULTS FALSE**, which is what keeps all 16 SC6 FSM
  units passing unmodified and stops a silent trainer paying half a second of dead
  air. `ZM_TRAINER_ROUTE1_RAMBLER` ships ZERO lines on purpose as the production
  instance of that arm.
  **★ AMENDED BY W3 (ZM-D-159): the zero-dead-air rule now applies to the BARK ONLY.**
  Every trainer, silent rows included, first runs the shared 0.35 s SPOTTED beat; a
  silent row still skips CHALLENGING entirely. The unit that pinned the old rule was
  RENAMED, not deleted (`Fsm_SilentTrainerIsByteForByteSC6` ->
  `Fsm_SilentTrainerShowsSpottedThenRaisesWithoutChallenge`) -- do not "restore" the
  original claim that a silent trainer reaches the encounter on its first Step.
- **A three-node `Query -> Branch -> Bark` graph was REJECTED** and should stay
  rejected: the only branchable condition ("does this trainer have lines?") must
  already be decided in C++ so the FSM can skip its window, and a graph duplicating
  a C++ gate is less correct. The graph owns the BEAT, not a decision.
- **Order 112 < 113 is load-bearing**: MenuStack closes/unfreezes at 112 and the
  withheld `Dispatch` fires at 113 in the SAME frame (measured
  `barkToBattleFrames=1`). SC7 added NO new freeze owner and SC8 must not either.

**★ A CORRECTED FALSE CLAIM, worth remembering because it was stated authoritatively
in two files:** `GetUnresolvedCount() == 0u` CANNOT catch a typo'd node-type name for
an in-process build. `Zenith_GraphBuilder::Node` looks the type up in the registry
and, on a miss, logs "unknown node type", latches `m_bErrors` and returns id 0 -- the
node never reaches the definition, so the count stays 0 and `Build()` returns false
instead. **The real guards are `Build() == true` / `HasErrors() == false`.**
`GetUnresolvedCount()` earns its keep only against a stale or hand-edited LOADED
`.bgraph`.

**RESOLVED by W1 / ZM-D-157:** the forced-replacement path and full trainer cap
landed together; this historical SC7 warning is no longer live.

**What SC6 pinned that SC7-SC8 must not re-litigate:**
- **Cheap-gate-first IS the cost control.** There is no raycast budget anywhere in
  this engine, so the SC3 pure cone runs FIRST and the ray is issued ONLY on a cone
  pass. Do not add an unconditional raycast, and do not add a raycast budget.
- **`TickTrainerSight` runs BEFORE `UpdateWander` and OUTSIDE its
  `if (!m_bWanderEnabled) return;` bail.** SC8's Vesper is STATIONARY, so folding
  the call inside the walker leaves every stationary trainer permanently blind
  **while every unit test still passes** (the units drive the FSM directly, not
  through `OnUpdate`). Mutation M1 exists solely to pin this -- do not "tidy" it.
- **The flagged/flagless gate asymmetry is deliberate** (Q-2026-07-28-001, now
  ANSWERED): a row with a defeat flag keys on that flag and ignores the latch; a
  flagless row keys on the process-global session latch, because
  `ZM_IsStoryFlagSet(state, NONE)` reads false forever and the prize would
  otherwise be farmable. Losing to Vesper leaves him re-battleable; losing to the
  rambler does not. It is documented in the gate's own header so nobody "fixes" it.
- **Only ONE body can be filtered per raycast.** The trainer is ignored by id and
  the player's capsule is excused by comparing `RaycastResult::m_xHitEntity` -- no
  distance tolerance. Fail polarity: non-finite CLOSED, no live simulation OPEN,
  coincident endpoints clear WITHOUT casting.
- **`TryResolveActivePlayerPose` is GONE** (no-legacy). Use the public
  `ZM_InteractionRuntime::TryResolveActivePlayer`.
- SC6 adds **no second freeze owner**; `ZM_BattleTransition::OnTrainerEncounterEvent`
  + `TryParkOverworldPlayer` still own that seam.

**★ THE OPEN DEBT SC8 OWNS: an authored trainer id does NOT survive save/reload.**
SC6 serializes nothing (`uSERIALIZATION_VERSION` stays `2u`) because the
per-component size prefix is computed from what is written, so one new field would
grow the five `ZM_Interactable` payloads inside the COMMITTED `Dawnmere.zscen`.
**Verified, not assumed:** a full windowed Vulkan batch left `Games/Zenithmon/Assets`
byte-clean. **SC8 should prefer the zero-byte route** -- a `ZM_TRAINER_ID` column at
the END of `ZM_NpcData`, derived in `OnStart` -- over a v3 payload bump.

**Two limitations recorded honestly, not papered over:**
1. **There is no occlusion coverage in Dawnmere itself.** The occlusion proof is a
   hermetic boot unit against an explicitly created box; the end-to-end test walks a
   CLEAR line. Terrain is NOT an occluder on a fresh CI checkout (its physics
   geometry is a gitignored baked asset), so "a wall blocks a trainer in the actual
   town" is reasoned, not measured. Do not claim otherwise.
2. **Another NPC's static AABB, or the `HomeDoorTrigger` box, counts as an
   occluder.** Arguably correct, but SC8 must keep it in mind when placing Vesper:
   a trainer behind the door trigger will appear inexplicably blind.

**RESOLVED by W1 / ZM-D-157:** the forced-replacement path and full trainer cap
landed together; this historical SC6 warning is no longer live.

**Historical SC5 warning -- RESOLVED by W1 / ZM-D-157.** `ResolveTurn` now
promotes a live reserve atomically, and the trainer cap was raised in that same
commit. The former next-action hard break no longer exists.

**Also open for SC6:** a flagless trainer row (`ZM_TRAINER_ROUTE1_RAMBLER`, which
carries `ZM_STORY_FLAG_NONE` on purpose) has no defeat-flag brake, so SC6's
re-engagement gate must key on a runtime latch for such rows -- see
Q-2026-07-28-001.

**Prior:** SC4 COMPLETE (ZM-D-152) -- the HUD Run-gate. It was the prerequisite for SC5: trainer forced-battle entry (`ZM_OnTrainerEncounter` + a 2nd `ZM_BattleTransition` subscription, leaving the wild validation path untouched), the `ZM_BattleDirector` trainer arm building the fixed enemy party and passing the row's `ZM_AI_TIER` to `Begin`, and `ZM_ApplyTrainerResultToGameState` (win -> `AddMoney` + set the defeat flag; loss -> the existing whiteout). Rides ECS orders 110/111.**

**SC4 removed the last blocker:** a trainer battle can now be begun without the
menu being able to break the process. SC5 is the first sub-commit that may
actually call `BuildTrainerBattleConfig()` in production.

**What SC4 pinned that SC5-SC8 must not re-litigate:** the Run gate is the
CONFIRM-boundary guard in `MenuConfirm`'s `ACTION_ROOT` arm, never the button
colour (the dim tint is cosmetic, labelled as such in three places); the root
list and every cursor index are flee-INDEPENDENT and
`MenuRootItemCount`/`MenuRootItemAtIndex`/`MenuItemCount` keep byte-identical
bodies (a removal gate could make `ZM_AutoTests_BattleMenu.cpp`'s DOWN-press loop
non-terminating); and the rule is read through
`ZM_BattleDirectorCore::IsFleeAllowed()` off the live config, never copied into
the UI. **Also discovered: `ZM_MakeTowerBattleConfig()` has long shipped
`m_bCanFlee = false`** with no production caller, so the Battle Tower would have
inherited the identical latent break the moment S11 wires it -- SC4 pre-empts
both.

**What SC3 pinned that SC4-SC8 must not re-litigate:** there is exactly ONE
facing-cone test in the game (`ZM_IsFacingXZ`, shared by the interaction picker
and the trainer sight predicate) and ONE flattening policy (`ZM_FlattenXZ`) --
do not hand-write a second dot product; the sight tuning has THREE parameters
(range, half-angle cosine, and an absolute vertical band -- see the ruling in
ZM-D-151); occlusion is NOT in the pure predicate and enters at SC6 as a glue-
layer probe filter; and the trainer config is built beside the wild one, never
by modifying it.

**Prior:** SC2 COMPLETE (ZM-D-150) -- the `ZM_TrainerData` roster + `ZM_STORY_FLAG_RIVAL1_DEFEATED` at wire bit 6.

**What SC2 pinned that SC3-SC8 must not re-litigate:** a trainer party row stores
ONLY `{species, level}` per member (`ZM_BuildWildEnemySpec` derives the rest with
zero randomness, so that pair already IS a fixed team); the roster is TWO rows on
purpose (Vesper carries a defeat flag, "Rambler Perrin" carries
`ZM_STORY_FLAG_NONE`, so both column shapes stay covered); every accessor is
TOTAL and returns the inert `UNKNOWN` row rather than asserting; and
`ZM_IsMilestoneStoryFlag` was deliberately NOT extended to the new flag --
uncalled surface ships with its producer at SC5/SC6.

**Prior:** S7 item 3 SC1b COMPLETE -- both commits landed (ZM-D-146 Null backend, ZM-D-147 navmesh persistence).

**Commit A shipped the platform SC1b needs, and it is engine-wide:**
- **`Zenith/Null` -- a GPU-less render backend, and headless is now a BUILD CONFIG.** `--headless` is deleted end-to-end. A `Null_*` config defines `ZENITH_NULL_RENDERER`, compiles `Zenith/Null` instead of Vulkan and hides the window. The difference that matters: the old flag SKIPPED the render paths, the Null backend RUNS them against no-ops -- so a headless run exercises the same code a windowed one does. `zenith build|test <G> --headless` survives as a CONFIG SELECTOR; **test discovery always uses the Null exe**. Compile-time checks use the constexpr `Zenith_IsNullRenderer()`.
- **Q-2026-07-21-001 CLOSED.** The headless-terrain assert was never about missing baked content (already graceful) -- it was the culling-buffer allocation, and it reproduced with terrain FULLY BAKED. CityBuilder now authors its terrain entity in EVERY config and runs 45/45.
- **`m_bRequiresGraphics` re-audited: 74 -> 25.** The flag now means "the assertions read GPU-produced output". **Zenithmon's headless suite went from executing 3 of 42 tests to 31** -- its old "42/42 green" was covering three tests, because a skip counts as a pass.

**Commit B (ZM-D-147) shipped the reusable ENGINE feature, with Zenithmon as its first consumer:**
- **`Zenith_NavMeshBaker`** (tools-time: generate -> serialize -> write -> **read back + memcmp**, because `WriteFile` returns void so the bytes on disk are the only truthful success signal), **`Zenith_NavMeshStats`** (the numbers the editor panel formats, so it cannot drift from the mesh), a **hardened validating `Zenith_NavMesh` load path** (every count/index checked BEFORE any `Reserve` or indexed `Get`; a refused load leaves an EMPTY mesh, never a half-populated one), and **`Zenith_NavMeshComponent` at ENGINE order 96** which OWNS its mesh -- no cache, no statics, lifetime = the scene's -- plus a full TOOLS debugging panel (stats, six visualisation toggles, point + path probes). **+28 engine units.**
- **Zenithmon consumes it:** `Source/Nav/ZM_NavBake` bakes Dawnmere at 16 m from SC1's pure coverage grid; the bake step sits in the ALWAYS-RUN FrontEnd authoring section (the Dawnmere block needs warm terrain, which a fresh clone's first boot does not have) and is skipped on Null builds, so CI loads COMMITTED bytes and never re-authors them.
- **TRACKED: `Dawnmere.znavmesh` + `Dawnmere.zscen` only** -- the four-scene plan was cut on evidence, see the churn finding below.
- **Registry 42 -> 44**, both new tests RUNNING headless (no `RequestSkip`, no `m_bRequiresGraphics` -- a deliberate TestPlan C6 deviation, because a committed asset's absence is a defect and a skip counts as a pass).

**★ THE HEADLINE: the adversarial review found FIVE real defects that a fully green gate had missed.** Six games building, 44/44 headless and windowed, 2546 boot units, ratchets byte-identical to pristine HEAD -- and still: (1) the reader required `neighbourCount == vertexCount`, but **`StitchPortalAt` deliberately appends a phantom neighbour past the vertex count** and DP's `DPDoor` uses it, so baking and reloading a stitched mesh would have died process-level **on correct data** (the baker's read-back `memcmp` can never catch this class -- it compares bytes, it does not re-parse); (2) `SetAssetRef` + the deferred `OnStart` **double-loaded**, freeing and re-allocating and dangling any cached `GetNavMesh()`; (3) `m_bMovedOut` was never cleared by the load path, leaking on a revived moved-from component; (4) a failed bake **left the corrupt file on disk** -- and it writes straight into the committed asset; (5) two tests could not fail (a literal `ZENITH_ASSERT_TRUE(true, ...)`, and comments claiming a component-count check detects a MESH leak, which it cannot). All five fixed, with a new regression unit pinning the phantom-neighbour round trip.

**★ AND the `.zscen` determinism probe was under-powered.** It repeated ONE boot shape three times and reported byte-identical scenes, so all four were staged for commit. The review found `Battle.zscen`/`PlayerHome.zscen` had since gone `AM` (4 and 12 entity-index-shaped bytes). Mechanism, confirmed by experiment: **authoring bakes in entity indices assigned during that boot, and the boot-time unit suite allocates entities first** -- re-running the original boot shape reproduced the staged bytes exactly. So any commit adding an entity-creating boot unit re-authors different scene bytes. **Consequence:** FrontEnd/Battle/PlayerHome stay ignored (CI re-authors them anyway); `Dawnmere.zscen` is tracked by EXACT PATH because it is the one scene a Null/CI boot never authors. Logged as Q-2026-07-25-001.

Then SC2 `ZM_TrainerData` + `ZM_STORY_FLAG_RIVAL1_DEFEATED` -> SC3 sight cone -> SC4 HUD Run-gate -> SC5 forced-battle entry -> SC6 sight FSM/occlusion glue -> SC7 first `.bgraph` -> SC8 rival Vesper. Eight design defaults are logged in **Q-2026-07-24-002**. ECS order 113 remains the last occupied GAME order (next free **114**); the navmesh component took an ENGINE order (96). Continue autonomously; the next human stop is the **S8 vertical-slice go/no-go**.

**PER-SC GATE -- run in this exact order, every time:** `Build\regen.ps1` (ONLY when a new .cpp or folder was added) -> `zenith build Zenithmon` -> `zenith build Zenithmon --headless` (the Null exe every gate below runs) -> `zenith test Zenithmon --headless` -> `Tools\run_unit_gate.ps1 -Exe <NULL exe> -Baseline <N> -TimeoutSec 300` (the 300 s timeout-kill is EXPECTED -- `--exit-after-frames` only applies while the test harness is stepping, so a no-test boot never exits on its own) -> full windowed `zenith test Zenithmon`. **Two standing tripwires:** (a) never write a PREDICTED unit count into `zm-tests.yml` -- only the OBSERVED one; (b) the engine baseline (now **1121**) moves only when an explicitly-scoped engine change owns the cross-game gate.

**S7 ITEM 2 SUB-COMMIT PLAN (6 total -- COMPLETE):**
- **SC1 DONE (ZM-D-137)** -- `ZM_StoryFlags` identity registry + flag-gated NPC lines + the `Npc_Warden` row.
- **SC2 DONE (ZM-D-138)** -- `ZM_SaveSlots`, the typed slot/disk layer over the frozen codec (Save0-2 + Auto).
- **SC3 DONE (ZM-D-139)** -- world-position capture, resume placement, quit-to-FrontEnd, the milestone autosave latch.
- **SC4 DONE (ZM-D-140)** -- the save/load slot presenter, manual Save0-2 flow and root-menu Save/Quit.
- **SC5 DONE (ZM-D-141)** -- the title menu, New Game, transactional Continue, the slot-operation test observer, and the disk-authentic Continue gate (`ZM_SaveContinue_Test`).
- **SC6 DONE (ZM-D-142)** -- the milestone-autosave producer gate `ZM_MilestoneAutosave_Test` (test-only; the disk-backed restoration gate landed with SC5). See "Last completed".

**S7 ITEM 1 (complete):** SC1 (ZM-D-135) froze `ZM_GameState`'s LAYOUT -- reach it with named free functions, never new members -- and SC2 (ZM-D-136) froze the pure transactional 11-module schema-v1 codec plus the exact **824-byte** v1 artifact. Every incompatible change from here owes a version bump + a literal historical-blob migration test IN THE SAME COMMIT.

**Architecture (fixed, do not re-litigate):** exactly **ONE ECS order is consumed for interaction -- 113 (`ZM_Interactable`)**; the NPC walker is a by-value member of it and `ZM_InteractionRuntime` is a by-value member of `ZM_PlayerController`. **Next free ECS order: 114.** Screens are by-value non-ECS presenters on `ZM_UI_MenuStack` (order 112), so a new screen is one arm per dispatch switch and costs no ECS order. **Save layering is FOUR tiers and the DIRECTORY IS THE BOUNDARY:** `Source/Party/ZM_GameState` (frozen model) -> `Source/Core/ZM_SaveSchema` (pure ZMSV codec, names no file or slot) -> `Source/Save/ZM_SaveSlots` (slots, files, the engine save layer; adds nothing to the payload) -> `Source/Save/ZM_ResumePoint` + `ZM_Autosave` (the pure placement and autosave DECISIONS, naming no ECS type), with `ZM_GameStateManager` as the one impure ECS/physics/scene reach on top. Interaction is a forward CONE, never a raycast -- not for a headless reason (**physics IS live headless**; ZM-D-127 corrected that false claim) but because the cone stays pure and unit-testable; S7's trainer occlusion ray enters as a probe filter in the GLUE layer, leaving the pure picker untouched. Five authored Dawnmere NPCs only (villager / Trade Post clerk / Caretaker / wanderer / **warden**) -- populated towns are S9/S10. **"Trade Post"**, never "Mart", in data/entity/asset names. NO RNG in the walker (TestPlan C8).

**S6 CLOSURE RULING (ZM-D-134, still binding):** behaviour graphs and navmesh-driven wandering were deferred to S7 -- `ZM_GraphAuthoring` is not written and S6 ships a bounded 3-arm C++ role dispatch behind one `Interact()` seam. `Zenith_NavMeshGenerator::GenerateFromGeometry` is terrain-capable when supplied suitable triangles, but `Zenith_AINavGeometry::GenerateFromScene` does **not** harvest streamed terrain geometry or a heightfield. S7 item 3 owns the first useful graph integration plus the terrain-triangle/grid-coverage and `.znavmesh` evaluation. `MasterPlan.md` is historical/read-only.

**PER-SC GATE -- run in this exact order, every time:** `Build\regen.ps1` (ONLY when a new .cpp or folder was added) -> `zenith build Zenithmon` -> `zenith test Zenithmon --headless` (heals DLLs) -> `Tools\run_unit_gate.ps1 -Exe ... -Baseline <N> -TimeoutSec 300` (the 300 s timeout-kill is EXPECTED) -> full windowed `zenith test Zenithmon`. **Two standing tripwires:** (a) never write a PREDICTED unit count into `zm-tests.yml` -- only the OBSERVED one from the boot log; (b) the engine baseline **1121 must remain unchanged** unless an explicitly-scoped engine change owns the cross-game gate.

## Last completed

**★ THIS SECTION'S "Prior:" CHAIN STOPS AT ZM-D-153 AND WAS NEVER EXTENDED. It is a
HISTORICAL chain, not the latest work; corrected 2026-08-01.** The actual last completed work
is **ZM-D-173 (2026-07-31)** -- engine raycasts ignore sensor bodies, Dawnmere's Home relocated
+40 m with its terrain pad, registry 51 -> 53, boot units 2809 -> 2817. Everything since
ZM-D-153 is recorded in the top-of-file blocks and in `DecisionLog.md` (newest first): ZM-D-154
through ZM-D-167 closed S7, ZM-D-168 was the visual audit, ZM-D-169/170 landed the first pixel
assertions, ZM-D-171/172 the physically-grounded lighting, ZM-D-173 the above. **The chain below
is left intact as the S7-era record; do not read its top entry as current.**

**S7 item 3 SC5 -- TRAINER FORCED-BATTLE ENTRY + PRIZE/DEFEAT WRITE-BACK
(ZM-D-153).** The vertical's core. New pure leaf
`Source/Battle/ZM_TrainerBattle.{h,cpp}` (party built from the row through the
shipped deterministic `ZM_BuildWildEnemySpec`; a domain-salted FNV-1a seed
provably disjoint from the wild seed space) lets boot units prove the party, tier
and seed WITHOUT starting a battle. `ZM_OnTrainerEncounter` + a SECOND
`ZM_BattleTransition` subscription (order 110) leave the wild event, validator and
subscriber untouched; a 5-line trainer PREFIX to `ZM_BattleDirector::RunSetup`
(order 111) dispatches and returns, leaving the shipped wild body byte-unchanged.
`ZM_ApplyTrainerResultToGameState` routes through the SAME
`ZM_ClassifyBattleResult` the wild path uses and credits via `AddMoney` (the sole
cap enforcer), reporting the OBSERVED delta so a saturated credit is honest.
`ZM_GameState` gains no member. +13 units (**2644 -> 2657**), +1 automated test
(registry **44 -> 45**, running headless in 86 frames), windowed **45/45**.
Mutation-proven: party bound 2 reds 2 units, inverted win check reds 5, doubled
prize reds 3.

**★ THE REVIEW FOUND A LATENT PROCESS BREAK, NOT A STYLE ISSUE:** the engine has
no faint-replacement, and SC2's roster already authored a 2-member trainer, so
SC5 as first written would have broken the process on the first enemy KO. See
the clamp note above. **A second finding killed a vacuous assertion:** the
"row's AI tier reached `Begin`" clause could not fail, because Vesper's GREEDY is
simultaneously the wild arm's literal and the pre-`Begin` default -- fixed by a
second round trip on a RANDOM-tier row plus two anti-vacuity guards.

Prior: **S7 item 3 SC4 -- THE HUD RUN-GATE (ZM-D-152).** A REFUSE-IN-PLACE guard in
`MenuConfirm`'s `ACTION_ROOT` arm, consulting the new total pure predicate
`MenuRootItemIsAllowed(eItem, bCanCatch, bCanFlee)` and returning the established
`{CONFIRM_NONE}` refusal BEFORE the Fight/Catch/Run if-chain -- so a forbidden
entry can never become a `ZM_BattleAction`, however the cursor got there and
whether or not anything was rendered. `ZM_BattleDirectorCore::IsFleeAllowed()`
surfaces the rule off the live config (sibling of `IsCatchAllowed()`, ZM-D-131).
5 files modified, no new TU (no regen owed), +6 units (**2638 -> 2644**),
headless and windowed both **44/44**.

**Review closed the hole that mattered:** nothing proved a MOVE still submits
when `m_bCanFlee` is false -- the only action a trainer battle has left once Run
is gated -- so hoisting the guard to the top of `MenuConfirm` would have left the
suite green while making a no-flee battle UNPLAYABLE. **Mutation-proven
two-sided:** transposing the two flags reds 6 units, inverting the RUN arm reds
10.

Prior: **S7 item 3 SC3 -- THE PURE TRAINER SIGHT CONE + THE ONE SHARED CONE PRIMITIVE
(ZM-D-151).** `Source/Interaction/ZM_TrainerSightLogic.{h,cpp}` ships
`ZM_TrainerSightTuning` and two TOTAL predicates (forward-vector and quaternion
forms). The cone maths was **EXTRACTED, not duplicated**: `ZM_FlattenXZ` and the
new `ZM_IsFacingXZ` were promoted out of `ZM_InteractionLogic.cpp`'s anonymous
namespace to external linkage, and the picker's fused block became one
`if (!ZM_IsFacingXZ(...)) { continue; }`. **All 44 shipped interaction units were
left untouched and stayed green** -- that is the behaviour-preservation net.
`ZM_BattleDirector::BuildTrainerBattleConfig()` lands beside the wild helper
(which is not touched by one character) and deliberately has NO CALLER until
SC4's Run-gate. +31 units, baseline **2607 -> 2638**; headless and windowed both
**44/44**.

**★ THE MUTATION BATTERY RETRACTED A CLAIM RATHER THAN CONFIRMING ONE.** Three
mutations, each rebuilt and re-gated: dropping `std::fabs` from the sight
vertical band RED, flipping that band's `>` to `>=` RED -- and respelling the
extracted comparison `dot >= min` back to `!(dot < min)` **GREEN, when the
authored comments asserted in three files that this was "THE ONE DELIBERATE
BEHAVIOUR CHANGE SC3 MAKES"**. It is not: on every input the suite exercises,
non-finite included, the two spellings answer the same, so the extraction is not
known to differ from the pre-SC3 block on ANY input. All three comment blocks
were corrected to the measured result (and claim no mechanism, since none was
measured); the unit was kept and re-documented as a fail-closed CONTRACT pin
rather than a mutation pin. **Do not restore the retracted narrative.**

Prior: **S7 item 3 SC2 -- THE `ZM_TrainerData` ROSTER + `ZM_STORY_FLAG_RIVAL1_DEFEATED`
(ZM-D-150).** Pure, headless, `Games/Zenithmon`-only: zero engine files, zero new
ECS orders (114 still next-free), no save-schema version bump.
`Source/Data/ZM_TrainerData.{h,cpp}` ships the append-only `ZM_TRAINER_ID`
(`ZM_TRAINER_NONE = ZM_TRAINER_COUNT`, so one `<` rejects the sentinel and all
garbage together), a deduced-bound `s_axTrainers[]` under a row-count
`static_assert`, and four TOTAL accessors that return an inert `UNKNOWN` row with
a NON-fatal `Zenith_Error` instead of asserting. Two rows: **Vesper** (one L5
KINDLET -- the Fire counterpart to the Grass FERNFAWN starter -- 500 prize,
`RIVAL1_DEFEATED`, GREEDY) and **"Rambler Perrin"** (two L4 Route-1 species, 120
prize, `ZM_STORY_FLAG_NONE`, RANDOM), because one row would leave the
no-defeat-flag column arm untested. `ZM_STORY_FLAG_RIVAL1_DEFEATED = 6u` is dense
and append-only; `COUNT`/`NONE` follow to 7 automatically and the name-table
`static_assert` makes a forgotten registry row a COMPILE error.
**+18 boot units** (17 `ZM_Data` + 1 `ZM_Story` freezing all seven wire bits as
hand-typed literals), baseline **2589 -> 2607**; headless registry unmoved at
**44** (correct -- a pure data sub-commit adds no automated test); full windowed
**44/44**. **Adversarial review killed a toothless unit before it landed**
(`Trainer_EveryPartyMemberHasAMoveAtItsAuthoredLevel` was a tautology:
`ZM_BuildWildEnemySpec` copies species/level straight through and `ZM_Learnsets`
always emits a level-1 STAB pick) and caught that two new TUs make
`Build\regen.ps1` MANDATORY -- no other TU references the new accessors, so
without it the build goes green with the whole table silently absent. **The +18
delta is the proof regen took; +1 would have meant it did not.**

Prior: **S7 item 3 SC1b -- BAKED NAVMESH PERSISTENCE AS A REUSABLE ENGINE FEATURE
(ZM-D-147); SC1b COMPLETE.** Engine: `Zenith_NavMeshBaker` (bake + read-back
verify), `Zenith_NavMeshStats`, a hardened validating `Zenith_NavMesh` load path
(bool-returning, asserts + logs + defined failure at every rejection, all
validation ordered ahead of every `Reserve`/`Get`), `Zenith_NavMeshComponent` at
ENGINE order **96** owning its mesh with a full TOOLS debugging panel, a
flags-driven `DebugDraw`, `ZENITH_NAVMESH_EXT`, and **+28 engine units**.
Zenithmon: `Source/Nav/ZM_NavBake`, the always-run bake step, the authored
Dawnmere holder entity, **+3 boot units** and **+2 automated tests** (registry
**42 -> 44**, both RUNNING headless). Tracked: `Dawnmere.znavmesh` (SHA256
`A783FB0A...`) and `Dawnmere.zscen` (`7337853F...`). Baselines **1093 -> 1121**
(engine) and **2515 -> 2546** (ZM boot), both from the OBSERVED line. Six
mutations proved teeth. **The review pass found five real defects behind a fully
green gate** -- the `StitchPortalAt` phantom-neighbour invariant (which would
have killed DP's stitched meshes on reload), a `SetAssetRef`/`OnStart`
double-load, an un-cleared `m_bMovedOut`, a failed bake leaving a corrupt file
on the tracked asset, and two tests that could not fail -- all fixed, plus the
`.zscen` churn finding that cut the tracked-scene set from four to one.

Prior: **S7 item 3 SC1 -- NAVMESH TERRAIN-SOURCE EVALUATION SPIKE (ZM-D-144).** Pure,
headless. `Source/Nav/ZM_NavEval.{h,cpp}` harvests a FLAT coverage grid (2
tris/quad) at Dawnmere's sampled ground height (25.98577) over the recipe's
**1024 m export sub-rect** and feeds the raw soup to
`Zenith_NavMeshGenerator::GenerateFromGeometry` -> a walkable navmesh (~4225 polys
at a 16 m cell). It NEVER constructs a live `Zenith_TerrainComponent` (sidesteps
Q-2026-07-21-001's headless VRAM assert) and reads NO disk asset; it fails closed
(NEVER asserts on args) on any cell size finer than the generator's `iMaxDim=1024`
clamp (recommended floor ~1.0 m). **`.znavmesh` persistence + runtime routing
DEFERRED** (Q-A) -- this is the EVALUATION, answered YES. 4 `ZM_Nav` boot units
(band **3969..4489** matched the real generator output; fail-closed 0.3 m
rejection + 8 m control; all-vertical -> zero walkable + upward control; the
1024 m sub-rect vs the 4096 m grid). **Teeth mutation-proven** (winding flip ->
units 1/2-ctrl/3-ctrl red, unit 4 green; restored). Boot **2521 -> 2525 / 2524 /
0 / 1**; `zm-tests.yml` 2521 -> 2525; headless registry unchanged **42/42**;
engine ref **1103** untouched. **NEXT = SC2.**

Prior: **S7 item 2 SC6 -- THE MILESTONE-AUTOSAVE TEST CLOSURE; S7 ITEM 2 COMPLETE
(ZM-D-142).** Landed the windowed `ZM_MilestoneAutosave_Test`
(`Tests/ZM_AutoTests_SaveAutosave.cpp`), the milestone-autosave PRODUCER gate --
a **TEST-ONLY** change (no production or engine code ships). Found IN FLIGHT and
uncommitted on master at session start (a prior session's work); an orchestrated
forensic assessment (spec + build-correctness + test-teeth lenses,
cross-verified) found it build-correct with **NO phantom seam** (unlike SC5's
in-flight file) and genuinely passable, so it went straight to the gate.

**What it pins BEYOND SC3** (which sampled only the autosave counter + the Auto
slot STATUS enum): (1) FULL DURABLE STATE -- a real `SCENE_ENTERED` arrival's
Auto slot is `ReadState`'d back FROM DISK and field-compared to a scrambled live
state (party/boxes/dex/story bits/badges/money/world position+yaw), not just
`ProbeSlot==READY`; (2) EXACTLY-ONCE twinned with SC5's slot observer -- one
`WRITE_STATE` on AUTO (==1) alongside the +1 counter; (3) POSITIVE DRAIN-PHASE --
the write is attributed to the `OnUpdate` IDLE drain via menu-term isolation
(`blocker==NONE` with the ROOT menu open); (4) ATTRIBUTABLE BLOCKED REAL ARRIVAL
-- a genuine arrival under a live non-transition blocker with a proven-capturable
player writes NOTHING (byte-identical Auto file, zero WRITE, EMPTY); (5) LATCH
RE-ARM + consume-before-attempt (`NoRetryWatch` -- no disk-hammering retry).

**Disk-authentic + stack-safe:** `ProbeSlot`/`ReadState` fall through to the real
file (the RAM readback stash is never staged), defeating the `DontDestroyOnLoad`
hazard; 12 per-phase driver functions with the two large `ZM_GameState`
instances as FILE-SCOPE globals avoid SC5's 1.31 MB monolithic-Step overflow.

**★ THE TEETH ARE MUTATION-PROVEN.** Five production mutations, each rebuilt in
isolation, all confirmed RED, then reverted + re-gated GREEN: DROP-CAPTURE reds
the disk-content/scene-tag/resume-valid asserts (the SC3 status-only green hole)
while counter+READY stay green; DROP MENU CONSULT reds the menu-probe +
blocked-arrival asserts; CONSUME-AFTER-SUCCESS reds `NoRetryWatch`; DELETE THE
DRAIN reds the `SamplePositive` poll deadline early at **45 frames**; DOUBLE-WRITE
reds the observer `traceExact==1`. Two `counterDelta` booleans are tautological
given their poll gates -- no false-green (the deadline carries the teeth),
deliberately not churned (matching ZM-D-141's low/low disposition).

**Evidence.** Regen GREEN (new untracked TU); build GREEN; headless **42/42, 0
failed** (registry **41 -> 42**; the windowed test skips headless); boot unit
gate **2521 / 2520 / 0 / 1 documented skip -- NO delta** (only a
`ZENITH_AUTOMATED_TEST_REGISTER`, so `zm-tests.yml` is NOT bumped and the engine
reference **1103** is unchanged); focused windowed **134 frames**; full windowed
**42/42 passed, 0 failed, 0 skipped, 0 zero-frame** (42 clean result JSONs); save
directory EMPTY; no stray `zenithmon.exe`. **★ CI-INVISIBILITY:**
`m_bRequiresGraphics=true`, so the headless backstop skips it -- teeth exist only
under the LOCAL windowed gate with `--filter ZM_MilestoneAutosave_Test`.
**Contracts held:** no production code changed; `ZM_SaveSchema` + 824-byte v1
golden, `ZM_GameState` layout, `ZM_SaveSlots` framing, and the latch/drain
byte-untouched; `uSERIALIZATION_VERSION` 1; **no new ECS order (114 next-free)**;
a phantom `Zenith_TerrainComponent.cpp` working-tree entry (blob byte-identical
to HEAD) was restored, not committed.

Prior: **S7 item 2 SC5 -- THE TITLE MENU, NEW GAME, TRANSACTIONAL CONTINUE, THE
SLOT-OPERATION TEST OBSERVER, AND THE DISK-AUTHENTIC CONTINUE GATE (ZM-D-141).**
Found IN FLIGHT and uncommitted on master (runtime complete; the observer seam
the new windowed test referenced had never been written, the test was logically
unpassable and partly vacuous); finished, gate-proven and landed by this
session. `Source/UI/ZM_UI_TitleMenu.{h,cpp}` is the FrontEnd title presenter --
by-value, non-ECS, one arm per dispatch switch on `ZM_UI_MenuStack` (order 112),
so **113 remains last occupied and 114 next-free**. TITLE is AMBIENT: auto-raised
only on a settled FrontEnd with an empty stack and no warp/battle owning the
screen, and force-closed the instant any of that stops being true, so it can
never fight the pause menu or a transition. Continue is visible iff ANY slot
probes non-EMPTY (DAMAGED counts, matching `AnySlotOccupied`); New Game is
always live; navigation is rebuilt every Present so no link targets a hidden
Continue, and focus is repaired onto the live default.

**Continue is transactional and reads disk exactly once.**
`ZM_GameStateManager::RequestContinue` runs `ZM_SaveSlots::ReadState` into a
LOCAL candidate, `QueueResume(candidate.m_xWorldPosition)` through SC3's
ordinary validated placement path, then publishes the candidate LAST; any
failure returns the exact `Zenith_ErrorCode` with live state and the transition
machine untouched. The Yes/No load prompt arms only from LOAD mode against a
row that still probes READY; YES performs the one definitive `RequestContinue`.
`RequestNewGame()` builds the starter, queues the Dawnmere warp, and publishes
the starter LAST. No second codec, slot reader, or placement path; LOAD remains
ungated by the overworld-only SAVE predicate.

**The slot layer gained a test-only operation observer** --
`ZM_SAVE_SLOT_OPERATION_FOR_TESTS` {PROBE_SLOT, READ_STATE, WRITE_STATE}, a
plain-function-pointer `SetOperationObserverForTests`, one event ON ENTRY per
public API call (a refused attempt is still observed once) -- because the
disk-authentic claim needs disk-layer evidence twinned with the menu's
`GetLoadReadCount()`. The global defaults nullptr so shipped behaviour is
byte-for-byte unchanged when unset, and `DeleteAllSlotsForTests()` clears it
FIRST (the between-tests hook needed no edit; TestPlan C3 holds).

**★ THE HEADLINE DEFECT: the monolithic-`Step` idiom hit the stack reserve.**
`Step_ZMSaveContinue`'s 29 phases aggregated ~six `ZM_GameState` locals
(~150-200 KB each: 6-mon party + 16x30 box storage) into a measured
**1,312,136-byte** /Od frame against the exe's **1 MB** stack reserve -- the
process died in `__chkstk` on the FIRST Step call (exit -1073741571 =
STATUS_STACK_OVERFLOW, diagnosed via crash-dump analysis). Fixed structurally:
28 per-phase driver functions (each <= ~2 `ZM_GameState`), a thin dispatch
Step, and a `SCResetGameState` helper for Setup. **Standing rule: any
multi-phase automated test touching `ZM_GameState` uses per-phase functions,
never one giant Step.**

**Evidence.** **6** `ZM_Title` + **2** `ZM_MenuStack` boot units took the gate
**2513 -> 2521**: **2521 ran / 2520 passed / 0 failed / 1 documented skip**;
`zm-tests.yml` bumped from the OBSERVED line. Registry **40 -> 41** with
`ZM_SaveContinue_Test` (**247 frames**): real-input New Game publishes a fresh
starter over an installed canary; a busy queue refuses `RequestContinue` with
exactly one READ + `QUEUE_FULL`; after quit-to-title, Continue stays visible
with ONLY a DAMAGED slot on disk; the Auto fixture is restored from
pre-deletion bytes; DAMAGED/EMPTY rows refuse with a plain line, never an armed
choice; pre-Yes the live state is still the scramble; the Yes window performs
exactly ONE `READ_STATE` on AUTO and ZERO writes; published state equals the
saved fixture and NOT the scramble; the restored pose lands within 0.05/0.10/
0.05 of saved, >= 2 m from both TownCenter and the scramble pose. Regen GREEN;
build GREEN; headless **41/41**; focused `ZM_SaveContinue_Test` **247**,
`ZM_RootQuitAndBlockedSave_Test` **158**, `ZM_SaveMenuFlow_Test` **98**; full
windowed **41/41 passed, 0 failed, 0 skipped, 0 zero-frame**; save directory
EMPTY. Adversarial review CLEAN (two low/low nits: per-frame `SetFocusable`
writes in title `Present`; `std::filesystem` in the test-only fixture path --
both logged, deliberately not churned). **Contracts held:** `ZM_SaveSchema` +
824-byte v1 golden byte-untouched; `ZM_GameState` layout frozen; `ZM_SaveSlots`
framing + write-answers-from-re-probe untouched; `uSERIALIZATION_VERSION` stays
1; no new ECS order.

**Item-2 boundary (now closed):** SC6 (ZM-D-142) shipped
`ZM_MilestoneAutosave_Test`, completing all six sub-commits; the disk-backed
restoration gate had landed here as `ZM_SaveContinue_Test`. **S7 item 2 is
COMPLETE.**

Prior: **S7 item 2 SC4 -- THE SAVE/LOAD SLOT PRESENTER, MANUAL SAVE FLOW AND
ROOT-MENU SAVE/QUIT (ZM-D-140).** `Source/UI/ZM_UI_SaveSlots.{h,cpp}` is the ONE
by-value, non-ECS presenter for both SAVE and LOAD: four always-present rows
backed by uncached `EMPTY / READY / DAMAGED` probes plus Back; SAVE mode writes
only Save0-2 (EMPTY immediately, READY/DAMAGED only after an input-driven Yes/No
overwrite confirmation; DAMAGED is surfaced, never repaired/deleted/auto-
overwritten); `Auto` is read-only in SAVE and loadable when READY in LOAD;
`ResolveLiveSaveBlocker` is checked at SAVE opening AND at the irreversible
`blocker -> CaptureWorldPosition -> WriteState` boundary; ROOT gained Save/Quit
with immediate focused-Save rehome to Quit under a live blocker; Quit is an
ACTION (Yes -> SC3's playerless `RequestQuitToFrontEnd()`). Evidence: **23**
save-screen + **5** menu-stack units (boot **2513 / 2512 / 0 / 1**), registry
**40**, headless **40/40**, focused **98**/**146** frames, full windowed
**40/40**, save dir empty.

Prior: **S7 item 2 SC3 -- WORLD-POSITION CAPTURE, RESUME PLACEMENT, QUIT-TO-FRONTEND AND
THE MILESTONE AUTOSAVE LATCH (ZM-D-139).**
New `Source/Save/ZM_ResumePoint.{h,cpp}` (pure validation + world-position
construction + the yaw conversions) and `Source/Save/ZM_Autosave.{h,cpp}` (the
autosave policy + its one live entry point), plus ~450 added lines on
`Components/ZM_GameStateManager.{h,cpp}` (capture, the resume latch, the
playerless destination, the autosave drain). **Before this,
`ZM_GameState::m_xWorldPosition` was written by NO runtime code anywhere in the
tree, and nothing recorded which spawn tag the player had arrived at** --
`m_szTargetSpawnTag` is the tag of an IN-FLIGHT warp and `ResetTransitionState`
memsets it the moment the warp finishes.

**The pure/impure split is why any of it is testable.** Validation, world-position
construction, the yaw conversions and the autosave predicate name NO ECS type,
component, scene handle or physics body, so the whole decision surface is pinned
by headless boot units with no scene loaded (`ZM_SpawnPoint::IsTagValid` is passed
IN as a bool for exactly that reason). Everything impure -- the unique player, its
body pose, the active scene, `ResolveLiveSaveBlocker` -- lives on the manager and
calls DOWN. Every function in both new TUs is TOTAL: the units feed them NaNs,
oversized tags, the UNSET sentinel and unresolvable build indices ON PURPOSE, and
`ZM_ValidateResume` evaluates SCENE -> TAG -> TRANSFORM so `ZM_GetWorldSpec`
(which asserts fatally on `ZM_SCENE_NONE`) is only ever reached after the index
resolved.

**`SaveFormat.md`'s transform-vs-spawn-tag TBD is RESOLVED: TRANSFORM-FIRST,
SPAWN-TAG FALLBACK -- and the fallback costs nothing because it is already on the
path.** A resume rides the ORDINARY validated `TryQueueWarp`: same fade, same
single load, same marker placement. `INVALID_TRANSFORM` is RECOVERABLE (scene+tag
are a complete destination, so the marker placement simply stands, with no second
placement path to keep in sync); `INVALID_SCENE`/`INVALID_TAG` refuse. The
interesting tag failure is a tag ANOTHER scene offers -- it passes grammar and the
warp validator, then WEDGES the transition in `WAITING_FOR_SPAWN` forever -- which
is why validity is checked against the destination's `ZM_WorldSpec` tag list.

**Two conventions that must never be mixed.** The captured position is the capsule
CENTRE (`Zenith_Physics::GetBodyPosition`, written straight back on restore);
spawn MARKERS store FEET and `CalculateSpawnCenter` adds the 0.9 m half-extent for
the authored 1.8 m player -- the two meet in that one function and nowhere else,
and confusing them is a silent 0.9 m sink/float. And the restore uses
**`SetBodyPosition`, NEVER `TeleportBody`**, because TeleportBody forces IDENTITY
rotation and would discard the yaw; rotation is written AFTER position, and the
restored yaw SURVIVES `ZM_PlayerController`'s per-frame `EnforceUpright` because
that rebuilds a Y-axis-only quaternion and PRESERVES yaw -- so restoring facing is
a real contract, not best effort. Yaw is `atan2` of the quaternion-rotated +Z,
never `glm::eulerAngles` (which collapses past 90 degrees off +Z). Placement sits
AFTER the marker teleport and BEFORE the camera barrier, and ends with
`SyncPhysicsPoseAndInvalidate` because `SetBodyPosition`/`SetBodyRotation`
deliberately do NOT fire the pose-changed hook `TeleportBody` does. Capture is
transactional, and falls back to the scene's FIRST offered tag when no transition
recorded an arrival -- load-bearing, not defensive: the boot path and every direct
`LoadSceneByIndex` enter without a warp, and the codec rejects an empty tag on a
set scene index, so such a game would otherwise be UNSAVEABLE.

**Quit-to-FrontEnd needed TWO bypasses, not one.** FrontEnd authors no Player, no
`ZM_SpawnPoint` and no `ZM_FollowCamera`, and `AdvanceFadeIn` carries two
INDEPENDENT barriers: `TryResolveFrozenTargetPlayer` (which bounces the state back
to `WAITING_FOR_SPAWN`, which bounces straight back) and, separately,
`HasUniqueReadyFollowCamera`. Patching only the spawn poll would have left the
transition ping-ponging forever on a permanently opaque screen. The playerless
flag is latched ONLY on `TryQueueWarp`'s accept line and cleared only by
`ResetTransitionState`; the arrival tail is SHARED by both paths so there is
exactly one, not two that drift.

**The autosave latch is edge-triggered and drained from `OnUpdate`.** It asks
SC2's blocker policy rather than re-deriving one, comparing against `NONE` rather
than listing arms (so a blocker appended later is honoured with no edit), and adds
only "no menu open" on top. It cannot fire from the fade-in tail --
`ResolveLiveSaveBlocker` consults `IsWarpInProgress()`, true for EVERY non-IDLE
state, so an in-tail autosave always resolves `WARP` and silently never saves --
so the arrival latches and `OnUpdate` drains once IDLE, consuming the latch BEFORE
the attempt (a retry is a disk-hammering loop, not a recovery). All five milestone
arms ship in the enum so it never needs renumbering, but exactly ONE
(`SCENE_ENTERED`) is LIVE today.

**★★ DEFECT 1 -- THE RESUME LATCH WAS SPENT BY THE FIRST PLACEMENT ATTEMPT.** It
was cleared BEFORE validating and applying the pose. But
`PollForSpawnAndPlacePlayer` can run MORE THAN ONCE per transition -- both
`AdvanceFadeIn` and `PollForCameraAndBeginFadeIn` push the state back to
`WAITING_FOR_SPAWN` when the frozen player id stops matching -- and every pass
re-runs the marker teleport. On any second pass that teleport would stand as the
final placement while the resume no longer applied, silently dropping the player
on the default spawn with nothing in the log: green today, flaky by construction.
Fixed by letting the latch die with the TRANSITION (`ResetTransitionState` already
clears it on both the success and cancel paths), so it can neither outlive its
transition nor retry forever, and every entry re-validates the pose.

**★★ DEFECT 2 -- THE MILESTONE-AUTOSAVE PRODUCER WAS PINNED BY NOTHING.** Deleting
the whole drain block left every test green, and `ZM_GetAutosaveCount()` was read
by NO test at all. Closed by asserting the counter delta AND the Auto slot's
probed status in both windowed tests (+1/READY on arrival, +0/EMPTY across the
quit).

**★★ DEFECT 3 -- THE QUIT-TO-TITLE "MUST NOT AUTOSAVE" ASSERTIONS COULD NOT CATCH
THE MUTATIONS THEIR OWN COMMENT CLAIMED.** The refusal there is OVER-DETERMINED:
the blocker policy refuses first, but deleting that policy (or whitelisting the
not-overworld blocker) just falls through to the playerless capture guard, so the
counter never moves either way and the test stays green. The comment was corrected
to say what the assertions really pin, AND a genuine integration negative was
added: with the player alive in Dawnmere and a warp in progress, the test proves
capture WOULD have succeeded, then proves `ZM_TryAutosave` refused and the counter
did not move (`blocker=3` WARP, `captureWouldWork=true`, `refused=true`, autosaves
`0->0`). The positive half is the arrival's `+1`, so neither half is satisfiable
by an autosave that never fires.

**★★ DEFECT 4 -- A FUNCTION SHIPPED WITH NO CALLER AND NO TEST** (a story-flag
autosave trigger resolver, not in the SC3 spec). Deleted; it lands WITH its real
producer, in the sub-commit that first calls it. **Also fixed:** a world-extent
unit that bracketed the guard only to `(512, 1e9]` -- the constant could have been
loosened from 4096 to 1e8 with everything still green -- now bracketed by a
hand-written 5000.0f rejection fixture; a comment naming the wrong guard as the
rejecting mechanism; three stale line citations; a missing explicit include; and a
one-frame transform-cache staleness after the pose write.

**Convention drift, honestly scoped:** three save-area test files were using
`std::vector` (forbidden -- "no `std::` containers, use `Zenith_Vector`") and were
converted to `Zenith_Vector<u_int8>` or a fixed array. A FOURTH,
`Tests/ZM_Tests_SaveSchema.cpp`, predates this work, was left OUT OF SCOPE and is
tracked separately -- the drift is **not** fully cleared.

**Evidence.** +27 pure `ZM_Save` units in `Tests/ZM_Tests_ResumePoint.cpp` and 2
new registered windowed tests in `Tests/ZM_AutoTests_SaveResume.cpp`. Regen GREEN;
build GREEN; boot **2485 / 2484 / 0 / 1**; `zm-tests.yml` bumped **2458 -> 2485**
from the OBSERVED line; engine reference **1103 UNCHANGED** (no engine file
touched, so no cross-game sweep was owed); headless **38/0**; windowed **38/0/0**,
zero skips; registered automated tests **36 -> 38**; save directory verified EMPTY.
`ZM_QuitToFrontEnd_Test` **38 frames** (players 1->0, loads 0->1, peak alpha 1.0,
final alpha 0.0, final state IDLE, autosaves 0->0, Auto slot EMPTY).
`ZM_ResumePlacement_Test` **236 frames**: spawn (512.000, 26.886, 480.000),
captured (518.092, 27.149, 471.476), planarErr 0.0000 (< 0.050), vertErr 0.0000
(< 0.100), yawErr 0.0000 (< 0.050), **10.477 m from the spawn** (> 2.000 required,
so the restore is provably NOT the spawn teleport), loads 1->2 (a REAL scene load,
so the value came from disk and not from RAM survival), autosaves 0->1, Auto slot
READY. **Mutation-verified:** suppressing the `SetBodyPosition` write turns
`ZM_ResumePlacement_Test` RED; restored, green. **Contracts held:**
`ZM_SaveSchema` untouched (824-byte golden unchanged), `ZM_SaveSlots` consumed as
shipped, `ZM_GameState` layout untouched, no new ECS order (next free **114**), no
`uSERIALIZATION_VERSION` bump -- every new manager member is SESSION state. No
commit, push or CI result is claimed.

---

Prior: **S7 item 2 SC2 -- THE TYPED SLOT/DISK LAYER OVER THE FROZEN CODEC
(ZM-D-138).** `Source/Save/ZM_SaveSlots.{h,cpp}` sits ON TOP of the ZMSV codec and
adds nothing to the payload: four slots (`ZM_SAVE_SLOT_0/1/2` + `AUTO`), a
three-state probe (`EMPTY`/`READY`/`DAMAGED`), typed `WriteState`/`ReadState`
returning `Zenith_Status`, `AnySlotOccupied`/`AnySlotReady`, `DeleteSlotFile`, and
one pure save-blocker predicate with a fixed precedence. The header names no ECS
type, scene, UI element or component; the .cpp's ONE live reach is
`ResolveLiveSaveBlocker`. **The one framing it adds:** `[u32 little-endian ZMSV
byte length][ZMSV blob]`, written BYTE BY BYTE, because `ZM_SaveSchema::Read`
demands an EXACT length while the engine's two Load paths DISAGREE about
`GetCapacity()` (disk wraps an exactly-payload-sized buffer; the staged readback
hands over a default-constructed OWNING stream whose capacity is the whole
1024-byte allocation). It carries no magic or version and leaves the **824-byte**
v1 golden untouched. **Two orderings are load-bearing:** the payload is STAGED AND
VALIDATED before `Zenith_SaveData::Save` is called at all (Save creates the file
the instant it is called), and Save's return is DISCARDED (a literal `true` on
every path), so success comes ONLY from a RE-PROBE. A DAMAGED slot is surfaced,
NEVER repaired, deleted or auto-overwritten; `AnySlotOccupied` COUNTS a damaged
slot while `AnySlotReady` is stricter. **Four defects, all now standing rules
below:** the test-slot interlock was keyed on `IsAutomatedTestRun()`, false during
the boot run -- simultaneously a total coverage hole (every disk unit skipped, and
a skip counts as a PASS) and a data-loss hazard against the player's real save
files; the layer's own rejection branches were dead to the tests (a disk byte-flip
fails the ENGINE CRC before the read callback runs); the verify re-probe was
unpinned (mutation-verified RED once a fixture reached it); and the
too-small-for-a-prefix guard had zero coverage until a hand-built `.zsave` with a
matching CRC reached it. +33 units; boot **2458 / 2457 / 0 / 1**; windowed
**36/0/0**.

Prior: **S7 item 2 SC1 -- STORY-FLAG IDENTITY REGISTRY + FLAG-GATED NPC LINES
(ZM-D-137).** `Source/Data/ZM_StoryFlags.{h,cpp}` gives the ZM-D-135 bitset an
identity: a SAVE-STABLE `enum ZM_STORY_FLAG_ID : u_int` whose **value IS the
persisted bit index in save-schema module 4**, six flags dense from zero, APPEND
ONLY, with a DEDUCED-bound compiled row table (a missing row is a COMPILE error),
free-function accessors (because `ZM_GameState` is frozen) and a TOTAL fail-closed
`ZM_StoryGatePasses`. `ZM_IsMilestoneStoryFlag` is authored beside it and remains
UNCONSUMED -- SC3's autosave ships its own trigger enum, and the story-flag
producer lands with the S8 story beats. First gameplay consumer: `ZM_NpcData`
gains three fields APPENDED AT THE END, a pure `ZM_SelectNpcLines` picks the set,
and the `ZM_NPC_RAISE_DIALOGUE` arm reads LIVE flags; **gating selects CONTENT,
never which seam a role talks through.** Fifth Dawnmere NPC `Npc_Warden` is the
first gated row. **Three defects, all now standing rules below:** a
`Zenith_Assert` on a unit-pinned input destroyed the WHOLE boot gate; the new NPC
row silently disarmed a roster unit by supplying a second TALKER; and the gated
branch was pinned by nothing until two mutation-verified phases went onto the
HEADLESS `ZM_NpcDispatch_Test`. +33 units; boot **2425 / 2424 / 0 / 1**; windowed
**36/0/0** with every S6 frame count held exactly.

Prior: **S7 item 1 SC2 -- SCHEMA-V1 CODEC FREEZE (ZM-D-136).**
`ZM_SaveSchema::{Write,Read}` is the pure inner-payload boundary over
`ZM_GameState`: 11 ordered independently length-framed modules, explicit
little-endian fixed widths, schema/module version 1, a 61-byte monster record,
append-transactional writes and exact-length transactional reads. Dex accepts
current/older roster counts and rejects newer with `VERSION_MISMATCH`; StoryFlags
writes its high-water count; Options is a counted uint16 TLV list requiring
exactly one known text-speed tag. The codec owns no slots, disk I/O, ECS or scene
behavior. 29 schema + 2 literal-golden units took the gate to **2392 / 2391 / 0 /
1**; the complete v1 golden is exactly **824 bytes** and represents v1, not a
fabricated v0 migration. All five builds, headless **36/0**, windowed **36/0/0**
and a complete cross-game sweep were green. Because the codec must distinguish a
growable owned stream from a fixed wrapped buffer, engine `Zenith_DataStream`
gained the read-only `OwnsData()` query -- that moved engine 1097 -> 1103.

Prior: **S7 item 1 SC1 -- DURABLE-MODEL FREEZE (ZM-D-135).** `ZM_GameState` owns
the complete module inventory: party plus deterministic transactional 16x30 boxes,
seen/caught dex, 4096 story bits, 8 badges, bag/full-width money, daycare, tower
current/best/seed, unset world position and NORMAL-default options. Catch
placement is party-first then first-free box while dex marking remains invariant;
caught battle records normalize `ABILITY_NONE` to the species regular ability. 18
new `ZM_Save` units (**2361 / 2360 / 0 / 1**). **The layout is frozen: reach it
with free functions, never new members.**

Prior: **S6 item 3 SC9 -- FULL STAGE CLOSURE (ZM-D-134); S6 COMPLETE.** The
five-build matrix, boot **2343 / 2342 / 0 / 1**, headless **36/0**, windowed
**36/0/0** and the six exact S6 filters (UI **158**, Talk **85**, Shop **286**,
Heal **315**, Interact **749**, Wander **830**) were green and non-skipped.

Prior: **S6 item 3 SC8 -- THE AUTHORED WANDERER (ZM-D-133).** `ZM_NpcWalkerLogic`
is a deterministic pure two-waypoint walker (fixed points, arrival dwell, explicit
halt, no RNG, XZ-only steering, patrol velocity preserving the body's Y);
`ZM_Interactable` serialization went **v2** with v1 data as a stationary
fail-closed fallback. **★ Its first placement intersected the ONE-SIDED terrain
mesh**, letting the dynamic capsule penetrate from the non-colliding side and fall
instead of patrol -- fixed with real clearance above sampled terrain, never by
pinning Y or teleporting. `Npc_Wanderer` sits at **x=540, z=476..484**.

Prior: **RENDERTEST CANARY RESTORED + ECS DUPLICATE-ORDER ENGINE GAP CLOSED
(ZM-D-132).** RenderTest's terrain is `_True`-baked (seed 1337), so
`TerrainEditorSmoke` PASSES windowed as a real canary. **★ HEADLESS still asserts
`Invalid buffer VRAM handle` at
`Zenith_TerrainComponent::InitializeCullingResources()`** -- an open ENGINE gap
(Q-2026-07-21-001), so terrain has no CI coverage on a GPU-less runner. **★ Also:
an engine baseline is pinned in a script DEFAULT (`Tools/run_unit_gate.ps1`) as
well as in per-game workflow args.**

Prior: **KNOWN-BUG SWEEP (ZM-D-131) -- four verified defects fixed, GAME-ONLY:**
the traversal drive picked keys in the WRONG FRAME in both remaining copies (both
now project onto the LIVE camera basis; `ZM_PlayerHomeRoundTrip` moved 673 ->
**831** frames); `ZM_NpcDispatch_Test` now asserts WHICH screen each role raises;
and the battle menu's unconditional Catch is gated on `IsCatchAllowed()`, pinned
by a key-edge unit added after the review caught the fix was UNPINNED.

Prior (S6 item 3, condensed -- full detail in DecisionLog ZM-D-124..130):
**SC7** `ZM_S6InteractGate_Test` in **749** frames; its finding -- **movement is
CAMERA-RELATIVE, so world-space key choice is correct only for a single leg from
rest** -- is the standing rule below. **SC6** `ZM_NpcShop_Test` (286) +
`ZM_NpcHeal_Test` (315) in one TU carrying the shared `WalkContext`/`TickWalk`
machine at fixed **1/60**. **SC5** three NPCs authored into the real Dawnmere
block + `ZM_NpcTalk_Test` (85); its blocker -- an NPC dead centre on a corridor
stopped the capsule 108 m short, `DriveTowardXZ` having NO obstacle avoidance --
is the standing check-traversal-routes rule. **SC4** `ZM_Interactable` (order 113)
+ `ZM_InteractionRuntime` + the headless `ZM_NpcDispatch_Test`; corrected the
false "physics is dead headless" premise by mutation. **SC3** the `ZM_NpcData`
content table with both row caps pinned to their UI limits at COMPILE time (both
UI guards are ALL-OR-NOTHING -- an oversized list is REJECTED, not truncated).
**SC2** the pure candidate picker (nearest wins, ties to the lowest index).
**SC1** `ZENITH_KEY_E` + pure `ZM_ShouldInteract` returning a REASON +
`ZM_InputActions.h` as the single source of every binding.

Prior (S6 item 2, condensed -- ZM-D-112..122): the consolidated `ZM_S6UIGate_Test`
(item 2 COMPLETE); Care Center heal as a dialogue yes/no CHOICE; shop
`ZM_ShopLogic` + `ZM_UI_Shop`; `ZM_UI_Bag`; `ZM_UI_Dex`; `ZM_UI_Party` +
generalized screen dispatch; `ZM_Bag` + money; `ZM_UI_DialogueBox`;
`ZM_UI_MenuStack` + an engine ECS fix; `Zenith_UIGridLayoutGroup`; and the S5
STAGE GATE sign-off.

## Notes for next agent (S7)

- **★★ NEW -- A PASSING PROBE IS NOT EVIDENCE OF DETERMINISM IF YOU ONLY VARIED
  THE REPETITION COUNT.** SC1b's `.zscen` probe ran the SAME boot shape three
  times, got byte-identical files, and cleared four scenes for tracking. A
  different boot shape falsified it within the hour: scene authoring bakes in
  entity indices assigned during that boot, and the boot-time unit suite
  allocates entities first, so **adding one entity-creating boot unit re-authors
  different scene bytes**. Before a probe clears a decision, ask what the claim
  is quantified over ("every boot") and vary THAT, not the number of runs. Only
  `Dawnmere.zscen` is tracked, by exact path; the three scenes CI re-authors for
  itself stay ignored (Q-2026-07-25-001).
- **★★ NEW -- A READ-BACK `memcmp` CANNOT VALIDATE A FORMAT.** The navmesh baker
  verifies its write by re-reading the file and comparing bytes, which is the
  only truthful success signal available (`WriteFile` returns void) -- but it
  never re-PARSES. That is exactly why a reader rule that was too strict
  (`neighbourCount == vertexCount`, which `StitchPortalAt` legitimately breaks)
  sailed through every bake. If a writer and a reader can disagree, something in
  the loop has to actually parse.
- **★★ NEW -- CHECK WHETHER A MUTATION LANDED ON A LIVE FIELD BEFORE CONCLUDING
  "NO TEETH".** Corrupting the middle byte of the committed `.znavmesh` left the
  test green -- because that byte is a per-polygon cached centre, which
  `ComputeSpatialData` recomputes on every load. The format documents those as
  dead bytes. Re-aimed at a vertex INDEX it kills the run at the assert; re-aimed
  at a vertex COORDINATE it produces a clean assertion failure. An inert mutation
  is a question, not an answer.

- **★★ NEW -- A MONOLITHIC AUTOMATED-TEST `Step` IS ONE STACK FRAME, AND /Od
  MAKES THAT FRAME THE SUM OF EVERY LOCAL IN EVERY PHASE.** SC5's
  `Step_ZMSaveContinue` aggregated ~six `ZM_GameState` locals (~150-200 KB each:
  6-mon party + 16x30 boxes) into a measured **1,312,136-byte** frame against
  the exe's **1,048,576-byte** stack reserve -- exit -1073741571
  (STATUS_STACK_OVERFLOW) in `__chkstk` on the FIRST Step call, before any test
  logic ran. Diagnose with the crash dump (`%LOCALAPPDATA%\CrashDumps` + cdb
  `.ecxr; kc`) -- the harness only reports `FAIL exit=-1073741571` with no
  result JSON. The fix is structural, not a budget: one driver function PER
  PHASE in the anonymous namespace (each holding <= ~2 `ZM_GameState`), the
  Step reduced to a thin switch. `ZM_AutoTests_SaveContinue.cpp` is now the
  canonical shape. Runtime frames on the save path each hold at most ONE
  `ZM_GameState` (~150-200 KB) -- safe, but never put two in one function.
- **★★ NEW -- THE SLOT-OPERATION OBSERVER EXISTS FOR DISK-LAYER ATTRIBUTION.**
  `ZM_SaveSlots::SetOperationObserverForTests` fires exactly one
  `ZM_SAVE_SLOT_OPERATION_FOR_TESTS` event ON ENTRY per public `ProbeSlot` /
  `ReadState` / `WriteState` call (a refused attempt still counts once; a
  write's verify re-probe is a separate PROBE event on the same slot; deletes
  are NOT observed). It defaults nullptr (shipped behaviour byte-unchanged) and
  `DeleteAllSlotsForTests()` clears it FIRST, so the between-tests hook resets
  it without naming it. A 4-row screen refresh = exactly 4 PROBE events in
  ordinal order; the whole title-Continue Yes window = 8 probes + exactly 1
  READ on AUTO + 0 writes (probes are refresh cadence -- record, never
  exact-pin).
- **★★ NEW -- A LATCH CONSUMED AT THE TOP OF A FUNCTION ITS OWN STATE MACHINE CAN
  RE-ENTER IS A RACE WAITING TO HAPPEN.** SC3's resume latch was cleared before the
  pose was validated and applied, but `PollForSpawnAndPlacePlayer` runs MORE THAN
  ONCE per transition (both `AdvanceFadeIn` and `PollForCameraAndBeginFadeIn` push
  the state back to `WAITING_FOR_SPAWN` when the frozen player id stops matching),
  and every pass re-runs the marker teleport -- so a spent latch silently leaves
  the player on the default spawn, only on the runs where the bounce happens.
  Before writing "consume the latch here", ask which callers can re-enter this
  function within the SAME logical operation. Prefer tying the latch's lifetime to
  the OPERATION (SC3: `ResetTransitionState`, which already runs on both the
  success and cancel paths) and making every entry re-validate, so re-application
  is idempotent and there is still no retry-forever risk.
- **★★ NEW -- WHEN TWO INDEPENDENT GUARDS BOTH PRODUCE THE RIGHT ANSWER, A TEST
  THAT ONLY OBSERVES THE OUTCOME PINS NEITHER.** SC3's quit-to-title "must NOT
  autosave" assertions were satisfied by an OVER-DETERMINED refusal (the blocker
  policy first, the playerless capture guard behind it), so every mutation their
  comment claimed to catch left them green. The fix is BOTH halves: say honestly
  in the comment what the assertion really pins, and add a negative staged so
  exactly ONE guard can be responsible -- SC3 proves the capture WOULD have
  succeeded on the same frame, then proves the blocker refused and the counter did
  not move. Pair every such negative with a POSITIVE (the same trigger DOES fire
  once the blocker clears), or the negative is satisfiable by a feature that never
  runs at all.
- **★★ NEW -- `Zenith_Vector`'s SINGLE-ARGUMENT CONSTRUCTOR TAKES A CAPACITY, NOT
  A SIZE**, unlike `std::vector`. `Zenith_Vector<T> x(n)` is EMPTY with room for n;
  use `Resize(n)` for n live elements, and note there is no range-insert (SC3 added
  a local `AppendBytes` loop). `Tests/ZM_Tests_SaveSchema.cpp` is the one remaining
  save-area file still on `std::vector` and is tracked separately -- convert it
  with that trap in mind. No `std::` containers anywhere else.
- **★★ THE `DontDestroyOnLoad` RAM-SURVIVAL HAZARD IS NOW PINNED, BUT THE PATTERN
  IS PERMANENT.** `ZM_GameStateManager`'s FrontEnd re-author path destroys the
  duplicate rather than reseeding (`OnStart` retires the duplicate entity and
  returns), so the live `ZM_GameState` survives quit-to-title ENTIRELY IN RAM and
  a naive "save -> quit -> continue" test passes GREEN against a Continue that
  reads ZERO bytes from disk. SC5's `ZM_SaveContinue_Test` is the canonical
  inoculation: scramble the live state, PROVE the scramble took, and assert the
  published state equals the saved fixture AND NOT the scramble, twinned with
  the slot-layer observer's exactly-one-READ / zero-WRITE window. Any FUTURE
  disk-backed test (SC6's autosave work included) must copy that shape; SC3's
  pose-based proof works for the same reason (a scene reload genuinely destroys
  and rebuilds the body).
- **★ SCHEMA V1 IS FROZEN (ZM-D-136), `ZM_GameState`'s LAYOUT IS FROZEN
  (ZM-D-135), AND THE SLOT LAYER IS ITS ONLY DISK OWNER (ZM-D-138).**
  Preserve the exact 11-module order, fixed widths, 61-byte monster encoding,
  statuses and transactional cursor/destination behavior in `SaveFormat.md`; every
  incompatible change owes a real version bump + a literal historical-blob
  migration test in the same commit. There is no v0. Reach the frozen model with
  named FREE FUNCTIONS (the `ZM_StoryFlags` pattern), never new members. Do NOT
  fold slot, ECS, scene or UI concerns down into `ZM_SaveSchema`, `ZM_SaveSlots`,
  `ZM_ResumePoint` or `ZM_Autosave` -- the directory IS the boundary, and the pure
  Save TUs name no ECS type on purpose (that is what makes them boot-unit
  testable). Pass live ECS answers IN as plain bools, as `RequestResume` does with
  `ZM_SpawnPoint::IsTagValid`.
- **★ THE SLOT LAYER'S CONTRACTS SC5-SC6 MUST NOT BREAK:** the 4-byte
  little-endian length prefix exists because the engine's two load paths disagree
  about `GetCapacity()` while the codec demands an EXACT length -- never pass
  `GetCapacity()` as that length and never branch on `OwnsData()`. `WriteState`
  answers from its RE-PROBE, never from `Zenith_SaveData::Save`'s return (a
  literal `true` on every path). A DAMAGED slot is SURFACED, never repaired or
  auto-overwritten, and COUNTS as occupied for Continue visibility.
  `ResolveLiveSaveBlocker` is the ONE permission predicate. SC4's menu save asks
  it at BOTH SAVE opening and the irreversible boundary, with the fixed order
  `blocker -> CaptureWorldPosition -> WriteState`; SC5/SC6 must preserve that and
  must not gate LOAD on the overworld-only SAVE predicate.
- **★ A HIDDEN FOCUSED ROOT ENTRY MUST BE REHOMED BEFORE INPUT DISPATCH.** SC4
  proves the live transition: focus Save, make a WARP blocker arise, hide and
  unfocus Save, move focus immediately to Quit, and rebuild both navigation
  directions across live entries only. Leaving focus on the hidden element makes
  focused-name dispatch stale; leaving an explicit link pointed at it swallows the
  arrow press because the engine performs no spatial fallback.
- **★ SC3'S PLACEMENT CONTRACTS (now recorded in `SaveFormat.md`):** saved
  positions are the capsule CENTRE while spawn markers store FEET, and
  `CalculateSpawnCenter` is the ONLY place the two meet (mixing them is a silent
  0.9 m sink/float). Restore with `SetBodyPosition` + `SetBodyRotation`, NEVER
  `TeleportBody` (it forces identity rotation and eats the yaw), rotation AFTER
  position, and follow it with `SyncPhysicsPoseAndInvalidate` -- those two setters
  deliberately do not fire the pose-changed hook `TeleportBody` does. Yaw is
  `atan2` of the quaternion-rotated +Z; `glm::eulerAngles(q).y` collapses past 90
  degrees off +Z and is banned. Placement runs AFTER the marker teleport and BEFORE
  the camera barrier.
- **★ STORY-FLAG INDICES ARE WIRE FORMAT.** `Source/Data/ZM_StoryFlags.h` is the
  authoritative index registry: append only, dense from zero, never renumber or
  reuse. Module 4 sizes itself from the highest SET index, so one sparse index
  costs ceil bytes in EVERY save forever. Reserve a flag by adding a row, never by
  leaving a gap; renaming a debug name is free.
- **★ A TOTAL FUNCTION MUST NEVER `Zenith_Assert` ON ITS ARGUMENTS.**
  `Zenith/Core/Zenith.h:138` defines `ZENITH_ASSERT` unconditionally immediately
  ABOVE its own `#ifdef ZENITH_ASSERT`, so the definition at `:140` always wins
  and `Zenith_DebugBreak()` fires in EVERY configuration. Units run at BOOT, so an
  assert on an input a unit deliberately supplies kills the process and **the
  whole boot gate is lost -- no "Unit tests complete" line prints at all**, which
  reads as a build failure rather than one red unit. Diagnose mis-authored data
  with a non-fatal `Zenith_Error(LOG_CATEGORY_GAMEPLAY, ...)` -- there is no
  `LOG_CATEGORY_GAME` -- and return the defined fail-closed answer. This is why
  `ZM_ValidateResume` orders itself SCENE -> TAG -> TRANSFORM: `ZM_GetWorldSpec`
  asserts on `ZM_SCENE_NONE`, so it may only be reached after the index resolved.
- **★ A NEW BRANCH IS PINNED BY NOTHING UNTIL A FIXTURE REACHES IT, AND A LOWER
  LAYER WILL HAPPILY REJECT YOUR FIXTURE FIRST.** The reusable levers:
  `Zenith_SaveData::SetReadbackForTest` stages bytes consulted AHEAD of the file
  and BYPASSES the engine CRC gate that otherwise rejects a corrupted fixture
  before the game's read callback runs; a hand-built `.zsave` with a matching
  `ComputeCRC32` is the only way to reach the disk path's exact-capacity
  behaviour; and every such fixture needs a WELL-FORMED CONTROL ARM so the verdict
  is attributable to YOUR layer. Related: bracket a numeric guard from BOTH sides
  with HAND-WRITTEN literals -- SC3's extent unit only tested 1e9, which left the
  4096 constant loosenable to 1e8 with everything green, and an expectation
  spelled against the production constant can never fail at all.
- **★ ADDING A DATA ROW CAN DISARM AN EXISTING TEST WITHOUT TOUCHING IT.** A fifth
  NPC row gave a roster unit a second TALKER, so that unit's own advertised
  mutation stopped redding it. When you append to a compiled data table, re-read
  every unit that WALKS it and ask what its stated mutation still proves. Fixture
  rows a test depends on must be a named, explicitly-spelled subset, not "whatever
  the table happens to contain".
- **★ `zenith test <Game>` runs the EXISTING exe -- it does NOT relink after an
  ENGINE-lib change.** For cross-game engine regression you MUST
  `zenith build <Game>` FIRST, THEN test, or you validate a STALE exe.
- **★★ THE TRAVERSAL DRIVE MUST BE CAMERA-RELATIVE.** Player movement is
  camera-relative and `ZM_FollowCamera` re-aims from a LAGGING camera-to-player
  vector, so the world-space meaning of W/A/S/D rotates as the player turns.
  Picking walk keys from raw world dx/dz is correct ONLY for a single leg from
  rest. `Tests/ZM_AutoTests_NpcServices.cpp` is the canonical source for any new
  walk (ZM-D-130/131) -- there is NO shared test header (`Tests/` is `.cpp` only)
  and every sibling helper sits in a per-file anonymous namespace, so copy the
  camera-relative drive, the stall watchdog and the physics-motion evidence
  (SC3's `ZM_AutoTests_SaveResume.cpp` does exactly that, and deliberately leaves
  the NPC-approach stages behind rather than shipping dead code).
- **★ CHECK EXISTING TRAVERSAL ROUTES BEFORE PLACING ANYTHING SOLID.**
  `DriveTowardXZ` has no obstacle avoidance, so a solid AABB on a corridor an
  existing windowed test walks blind will kill that test at its frame cap. SC1's
  warden derives every separation explicitly (18 m off the z=480 Home corridor,
  34 m off the x=512 spawn corridor, 20.0 m to the nearest NPC) -- copy that
  style, and re-derive from scratch if anything moves. The same arithmetic applies
  to warp SENSORS: SC3's walk target sits ~130 m from `HomeDoorTrigger` because
  `ZM_WarpTrigger` resets its overlap latch in `OnStart`, so a freshly loaded
  scene starts UNLATCHED and a pose restored inside that volume instantly
  re-warps.
- **★ MUTATION-TEST any test you claim has teeth** -- break the thing it should
  catch, rebuild, confirm RED (exit=1), restore, re-gate. Six consecutive
  sub-commits; four times it revealed the test proved nothing.
- **★ The recurring review win is finding tests that CANNOT FAIL** -- loops
  bounded by a count just asserted zero; "unchanged" asserts on state never
  populated; a totality test whose expectation calls the function under test; an
  expectation DERIVED from the production table it is meant to pin; a producer no
  assertion observes at all (SC3's autosave counter). Check for vacuity EXPLICITLY
  on every SC, naming per unit the change that would red it. Corollary: **never
  ship a function with no caller and no test** -- it lands with its producer.
- **★ THE RECURRING DEFECT CLASS: confidently-worded FALSE claims inside
  argumentative comment passages.** SC1 found seven copies of one (`QueueLines`
  "would crash" on a null array -- it rejects it and the real consequence is a
  MUTE NPC); SC2 found a "would walk far past the buffer" claim that ignored the
  frozen codec's own equivalent bound; SC3 found a comment naming the wrong guard
  as the rejecting mechanism, plus three stale line citations. Verify every claim
  a comment makes about another function BY READING that function, and re-check
  every `File.cpp:NNN` citation you copy forward.
- **★ UI element ownership:** `AddElement` pushes into BOTH `m_xAllElements` and
  `m_xRootElements` while `Clear()` deletes only `m_xAllElements`, so a merely-
  `AddChild`'d element **LEAKS** and an `AddElement`+`AddChild` element is walked
  TWICE -- `Zenith_UICanvas::ReparentElement` is the only correct path. And
  `SetVisible` notifies the parent (a grid re-runs layout), so write child
  visibility **ONLY ON CHANGE**, in Hide as well as Present.
- **★ ENGINE UI NAV RULE:** never wire bake-time `SetNavigation` links into a pool
  whose members are shown/hidden at RUNTIME -- `NavigateDown` consults the explicit
  link FIRST and only falls back to the spatial search when it is null, so a link
  into a hidden element silently swallows the press. `SetNavigation` is also not
  serialized, so bake-time links exist only in tools builds.
- **★ Windowed-gate rule:** a test that PARKS the canvas focus programmatically
  proves NOTHING about navigation -- drive real arrow-key edges, deadline-guarded,
  with flags that **DEFAULT TO FAILING**, and give every waiting phase its OWN
  deadline and diagnostic (being ended by the harness frame cap says nothing about
  WHERE the test stalled).
- **★ The dialogue-answer trap, BOTH halves:** (a) a prompt raised over an EMPTY
  stack pops to empty on resolve, which `CloseMenu()`s and `Reset()`s the box,
  clearing its answer, all in ONE `OnUpdate` -- read
  `ZM_UI_MenuStack::GetLastDialogueAnswer()`, NEVER `GetDialogue().GetChoice()`.
  (b) that host latch is NOT per-test state, so also assert
  `!IsDialogueAwaitingChoice()` before trusting it; do NOT "capture before and
  require a change".
- **★ ANY per-test mutation of persistent state must be RESTORED on EVERY exit
  path**, including a mid-phase failure -- and **a new stateful game component
  MUST be wired into the between-tests hook** in `Zenithmon.cpp` (SC2 added
  `ZM_SaveSlots::DeleteAllSlotsForTests()` there, BEFORE
  `Zenith_SaveData::ClearForTest`). Watch WHERE the reset sits: SC3's autosave
  counter is a PROCESS global, so `ZM_ResetAutosaveForTests()` had to go ABOVE
  `ResetRuntimeStateForTests`'s no-manager early-out, or a batched run that has
  just force-loaded FrontEnd inherits the previous test's count.
- **★ Dispatch by the FOCUSED ELEMENT'S NAME**, never `SetOnClick(this)` -- a
  `this` userdata dangles on ECS pool relocation. Screen presenters are NON-ECS
  `Source/UI/` classes owned BY VALUE by the MenuStack.
- **★ Positional aggregate tables: append new columns AT THE END** -- every
  `ZM_NpcData` row is a positional initializer, so a mid-struct field shifts each
  trailing value one column left with NO compile error.
- **★ No teleportation for movement** -- use `Zenith_Physics`, never
  `SetPosition`, **even in tests**. (A one-time load/warp SPAWN placement is not
  movement; that is the one sanctioned direct body write, and SC3 keeps it inside
  the transition machine.)
- **★ CI-VISIBILITY:** `zm-tests` runs HEADLESS and a SKIP counts as a PASS, so
  every `m_bRequiresGraphics = true` test (the walk-up family; SC3's
  `ZM_ResumePlacement_Test` / `ZM_QuitToFrontEnd_Test`; and SC4's
  `ZM_SaveMenuFlow_Test` / `ZM_RootQuitAndBlockedSave_Test`) is carried by the
  LOCAL WINDOWED gate only. CI-visible coverage must live in a headless test or a
  boot unit.
- **Diagnosing a windowed test:** game `Zenith_Log(LOG_CATEGORY_UNITTEST)` output
  is NOT in harness stdout -- run the exe directly, `<exe> --automated-test <Name>
  --exit-after-frames N` (pair the flags; bare idles forever). Visual evidence:
  `Flux_Screenshot::RequestDump` in a Step -> BGRA TGA -> PNG.
- **New files -> `Build\regen.ps1`.** Sharpmake globs each game's tree recursively
  and the generated projects are gitignored, so a new FOLDER without a regen
  link-fails in a way that looks like a code defect (SC2's `Source/Save/`).
- **Working model:** MASTER-ONLY (ZM-D-031); the LOCAL gate is the authority;
  `zm-tests` is a post-push backstop (fix forward on red, never revert or
  force-push). Only the orchestrator builds/tests/commits; subagents author and
  never build. Sweep stray `zenithmon.exe` processes before ending. NEVER commit
  baked assets or `Build/artifacts`. `zenith test Combat` drops a stray
  `EnemyBase.zpfb` in the repo root -- delete it, never commit it.
- **Open Questions -- ★ REBUILT 2026-07-29; the previous list named TWO CLOSED ids and omitted
  NINE genuinely open ones, i.e. it was wrong in both directions.** `Questions.md` is the
  authority; this is a pointer, not a copy, and it goes stale the moment it tries to be a copy.
  Currently `[OPEN]`: **Q-2026-07-29-001** (the BOX screen's S7 deferral -- acted on, re-deferred
  to S9); Q-2026-07-25-002 (other games' `.zscen` + git-LFS for heavy bakes); Q-2026-07-24-002
  (S7 item 3's eight adopted defaults; its Q-A part is resolved); Q-2026-07-24-001 (repo-root
  `AGENTS.md` deletion); Q-2026-07-18-001 (S5 item 5 scoping); Q-2026-07-17-002 (S5 item 4
  scoping); **Q-2026-07-17-001** (`ZM_BattleTransition::BiomeForScene` is a hard-coded table, not
  a `ZM_WorldSpec` column -- **harmless today but it WILL bite S8/S9, where every new route or
  town needs a hand-edit instead of a WorldSpec row; worth closing before content scale-up**);
  Q-2026-07-12-005 (Battle Tower tuning); Q-2026-07-12-003 (`ZM_BattleAI` rulings);
  Q-2026-07-12-002 (`ZM_ExpAndLevel` sequencing); Q-2026-07-12-001 (`ZM_ValidateEventStream`
  rule 7 -- explicitly flagged non-blocking and "a good small standalone task");
  Q-2026-07-10-004 (unit-test verification gap + baseline-ratchet churn). **CLOSED, and wrongly
  listed as open until now:** Q-2026-07-21-001 (`[CLOSED 2026-07-25]`, ZM-D-146 -- the Null
  backend) and Q-2026-07-21-002 (`[RESOLVED 2026-07-26]` -- a test-ISOLATION defect, not a
  determinism regression). None of the open items blocks anything; the protocol is best-guess
  and proceed.
- **The next VISUAL hard-stop is the S8 vertical-slice go/no-go** (manual
  playthrough sign-off). S6/S7 have no visual gate -- the loop runs through them
  automatically.
