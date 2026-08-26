# RenderTest

The renderer/engine feature testbed: a 4096 m procedural terrain campus (seed
1337) hosting an IK step-cube deck, a material showcase, a gun pickup/drop
testbed, a jetpack, a StickFigure third-person player, and the **autonomous
tennis court** (two AI NPCs playing a rule-correct physics match under a
passive referee). Single scene, build index 0 (`Assets/Scenes/RenderTest.zscen`),
fully re-authored + saved by editor automation on **every** tools boot — windowed
or headless.

**A headless (`Null_`) boot authors the SAME scene a windowed one does**: 82
entities, 361753 bytes, `TerrainTrees_Trunk` / `_Leaves` and their ~323 KB of
instance data included. Entity and component creation is backend-neutral (ZEN-6);
only the GPU allocation underneath it is skipped, and on the Null backend that is
`Zenith_Null_MemoryManager` handing back dummy handles. So a headless boot
re-authors the committed bytes exactly and `Zenith_Editor::SaveActiveScene` logs
`[ScenePublish] IDENTICAL`, skipping the write because it would be a no-op.
Consequences worth knowing:

* **Re-authoring the scene does NOT need a GPU.** A headless tools boot picks up an
  authoring change and publishes it. What it must not do is publish a scene that is
  short of entities for a reason nobody intended — see the audit below.
* **Every publish is audited, on every backend.** `AuditScenePublish` diffs what the
  save would write against the file: `IDENTICAL` skips the write (and is the
  standing proof that headless authoring is not missing anything), `NO_FILE`
  creates, and a change that publishes FEWER entities than the asset holds is
  published but reported with both counts via `Zenith_Error`. Deleting an entity is
  a legitimate authoring change; doing so by accident is the historical defect.
* **Per-run harness entities are spawned transient, post-load**, never authored
  before `AddStep_SaveScene` (`RenderTest_EnterSmokePlayMode` creates
  `RenderTestSmokeRunner` this way). Authoring one would write it into the tracked
  asset on every `--rendertest-smoke` run.
* `RT_SceneAssetIntegrity` (`Tests/SceneAssetIntegrity.cpp`) guards all of the
  above by inspecting the file on disk after boot: both tree entities present, the
  campus + tennis testbed present, no `RenderTestSmokeRunner`. These assertions got
  STRONGER when the guard went away — a headless boot now actually writes the file,
  so "both tree entities are in the asset" is an end-to-end check rather than a
  near-vacuous one.

<details><summary>History: why a headless boot used to be forbidden from saving</summary>

`Zenith_TerrainEditor::EnsureTreeEntities` used to `return false` on its first line
under `Zenith_IsNullRenderer()`, so a headless boot authored the campus WITHOUT its
two instanced-tree entities (82 windowed, 80 headless) and serialized that subset
straight over the tracked asset — every headless run silently rewrote the committed
scene down to ~38 KB, and the only symptom was a dirty `git status` nobody was
reading. A publish guard in `SaveActiveScene` then refused any headless save that
would CHANGE an existing file, which made **re-authoring require a windowed boot**
and put a graphics driver in front of a scene edit. ZEN-6 fixed the cause instead:
the bail conflated "create scene data" with "allocate GPU buffers" and skipped the
wrong one. Keep the distinction in mind when adding an authoring step — a
`Zenith_IsNullRenderer()` bail is a defect whenever it skips entity or component
creation, and fine when it skips device traffic.

</details>

### Authoring is byte-reproducible across configurations — keep it that way

A Debug tools boot and a Release tools boot author **byte-identical** scene files
(verified: same MD5 from `Vulkan_vs2022_Debug_Win64_True` and
`Vulkan_vs2022_Release_Win64_True`). That is not free — it was bought, and it is
easy to lose again.

It used to be false. The project compiles `/fp:fast`, which lets the optimizer
reassociate, contract into FMA and substitute vectorized libm — and it does so
differently at `/Od` and `/O2`. So the two configs wrote **19266 different bytes**:
the 2520 tree instances (scatter position, height sample, per-instance scale and
yaw quaternion) plus the four guns' `rotZ 90°` and the two rackets' bone-attach
`rotX 180°`. The file ping-ponged in `git status` depending on which config you
last ran, which is exactly the trap `Games/Zenithmon/Docs/DecisionLog.md` ZM-D-183
describes.

The fix has two halves, both in `Zenith/Core/Zenith.h`'s
`ZENITH_AUTHORING_DETERMINISM_BEGIN` and `Zenith/Maths/Zenith_Maths.h`'s
`Authoring*` helpers:

1. **Pin the FP model** around functions that compute serialized values —
   `Project_RegisterEditorAutomationSteps` here (its hill/tree ring strokes are
   `sinf`/`cosf` of a per-index angle), plus `ApplyTreeDab`, `SampleHeightNorm`,
   `BuildEulerRotation`/`BuildEulerOffsetMatrix` and `BuildMatrix` engine-side.
2. **Do not call glm from authoring math.** The pin does NOT reach glm's operators:
   they are header inlines that take their FP model from their own definition point
   and are shared as COMDATs with every `/fp:fast` TU. Pinning the callers alone
   left the tree yaw and the racket matrices still config-dependent. `angleAxis`,
   the quaternion product, `mat4_cast`/`translate`/`scale` and `radians` therefore
   go through `Zenith_Maths::Authoring*` — same formulas, transcribed from glm,
   compiled once under the pin. (`radians` was the last straggler: `ApplyTreeDab`'s
   slope threshold still called `glm::radians` inside the pin until ZEN-6, on the
   mistaken reasoning that the pin covered it.)

**If you add authoring code that computes a float landing in this scene, use those
helpers, and re-verify by authoring from both configs and comparing the bytes.**

## Behaviour Graphs (W3 conversion)

ALL discrete gameplay decisions live in two boot-authored graphs since the W3
adoption wave (doctrine: systems = C++ components, logic = graphs; runtime docs
in `Zenith/Scripting/CLAUDE.md`). Both are regenerated every tools boot via
`AddStep_GraphBuild` (builders in RenderTest.cpp) and attached by the scene
authoring; the old `Zenith_BehaviorTree` usage + `RenderTest_TennisBTNodes.h`
are DELETED.

| Graph | Attached to | Driven by | Logic |
|---|---|---|---|
| `RenderTest_TennisBrain.bgraph` | both tennis NPCs | its own ON_UPDATE tick chain | An authored accumulator reproduces the retired AIAgent 0.08 s interval EXACTLY (`RTTennisTickGate` mirrors the enable freeze; `AddBlackboardFloat(dt)` → `CompareBlackboardFloat(>=0.08)` → `Gate` → `SetBlackboardFloat(0)` = accumulate/fire/reset-to-zero), then a 3-pin `Selector`: serve (phase/IsServer/ServeBallParked engine gates → decide → position → arm) > rally (phase/IsMyBall gates → `RTTennisBallReachable` → move → decide → arm) > recover. |
| `RenderTest_PlayerActions.bgraph` | Player | engine ACTION sources | The discrete PRESS decisions only, each an `OnActionPressed` node naming a C2 action (input program B10) rather than a device code: INTERACT → `RTPlayerInteractGun`, RELOAD → `RTPlayerTryReload`, FIRE → `RTPlayerTryFire`, CYCLE_TENNIS_CAMERA → `RTPlayerCycleTennisCam`. That is why the pad column needs no second chain per row. Holds (MOVE / SPRINT / JUMP+jetpack / AIM) and all systems stay C++. |

### RNG-determinism contract (risk R2, discharged)

The tennis decide nodes consume the brains' per-side `TennisRng` streams only
on un-armed ticks, so the whole match is a deterministic function of tick
cadence + gate order. The graph tick runs at component order 60 — provably the
same frame position as the retired BT tick at AIAgent(90)-before-nav (same
inputs, same-frame nav-destination consumption).

Gated **hermetically**, per clause, by the three `RT_TennisBrain*` automated
tests in `Tests/Test_TennisBrainContract.cpp`. None of them loads a scene, runs
physics, builds a navmesh or spawns a ball: each builds a one-entity scene,
instantiates the production graph definition straight from
`BuildGraph_RenderTestTennisBrain` (so there is no `.bgraph` on disk to go stale
and no dependency on a prior tools boot), and drives it by hand with scripted dt
and a scripted blackboard, reading the executed-node trace back out of
`Zenith_BehaviourGraph::GetRecentlyExecuted`.

| Test | Clause | Pins |
|---|---|---|
| `RT_TennisBrainTickCadence` | tick cadence | fire period 82 at dt=1/1024 (the 0.08 s threshold, to ~0.001 s); fire on tick 2 at dt=0.04 (the `>=` boundary — 0.04f doubles *exactly* to the float 0.08f); constant period 6 at dt=1/64 (reset-to-zero, not a subtractive carry); 42 ticks to fire after 40 accumulate + 200 parked ticks (FREEZE — reset would be 82, keep-accumulating 1) |
| `RT_TennisBrainGateOrder` | gate order | the authored tick spine `RTTennisTickGate>AddBlackboardFloat>CompareBlackboardFloat>Gate>SetBlackboardFloat>Selector` and each Selector pin's full chain, compared as strings; at runtime, serve-pin success never evaluates the lower pins, and a rally tick evaluates serve→rally→recover in that order |
| `RT_TennisBrainRngDraws` | RNG draw counts | zero draws on every accumulate-only tick and while parked or armed; first serve exactly 3 draws (placement coin + aim disc), second serve exactly 2, a neutral rally decision exactly 1; the near-side seed `0x1234567` |

Each test is a set of NAMED checks that all run and all report, so a break says
which clause moved and by how much — the design fix for the old digest, which
could only ever say "mismatch". The clauses are deliberately decoupled: only the
cadence test pins the threshold value, so a deliberate cadence change reddens
exactly one test.

Whole-run cost: **~0.08 s for all three** (1 frame each).

**Retired: `RT_TennisDeterminismDigest`.** It folded an FNV-1a digest over 2400
fixed-dt frames of the live match and compared it to a hard-pinned constant
(finally `0x4369AB2293ADFDDB`). It cost ~52 s and pinned an entire physics
simulation, so any legitimate gameplay or physics tweak broke it with no signal
about whether the break was intended — which trained people to re-pin rather
than investigate, and is how it stayed red-but-ignored for weeks
(Q-2026-07-21-002). Its one genuine defect find was a HARNESS bug, not a tennis
bug: the harness did not pin dt across the between-tests reset/settle window
(`ResetSimulatorAndCallSetup` falls through to `Stepping` in the same tick, so
`Setup`'s `SetFixedDt` landed after that frame's `UpdateTimers`, and Step 0 —
which loads the scene — ran game logic on a real frame time). The brains' 0.08 s
accumulator integrated that wall-clock residual, `RTTennisTickGate` freezes
rather than resets so it survived to the align frame, and the `>=0.08` threshold
quantised the continuous phase into ~3 discrete digests — a genuine flake that
looked like a stable wrong value. Fixed in `e2f5796c`: `Zenith_AutomatedTest`'s
`m_fFixedDt` now defaults to 1/60 instead of "unset", so `--fixed-dt` is no
longer needed for determinism anywhere.

### Node library (`Components/RenderTest_GraphNodes.h`)

Registered via `RenderTest_RegisterGraphNodes()` from
`Project_RegisterGameComponents`. Tennis node bodies are the retired BT leaves
VERBATIM with blackboard reads redirected at the graph blackboard.

| Node | Carries |
|---|---|
| `RTTennisTickGate` | the AIAgent `m_bEnabled` early-return (referee parks agents outside SERVING/LIVE; freeze-not-reset accumulator semantics) |
| `RTTennisDecideServe` / `RTTennisDecideShot` | SelectServe/SelectShot on the brain RNG, IsArmed early-SUCCESS (the don't-re-roll-mid-swing guard) |
| `RTTennisPositionForServe` / `RTTennisMoveToIntercept` / `RTTennisRecoverToReady` | nav-destination + footwork-X staging (slab-projected, margin 1.0) |
| `RTTennisBallReachable` | the retired BallIsMine's systems half: awareness (`GetAwarenessOf`, threshold 0.25) + `PredictIntercept` reachability |
| `RTTennisArmServe` / `RTTennisArmSwing` | the body handshake: arm ONLY on a true RequestServe/RequestSwing, epoch-stamped from the blackboard |
| `RTPlayerInteractGun` / `RTPlayerTryReload` / `RTPlayerTryFire` / `RTPlayerCycleTennisCam` | the player press verbs (bodies moved intact into public `Try*` methods on the components) |

### Shims

- `RenderTest_TennisAgentComponent` (brain, order 135): per-side deterministic
  RNG (`0x1234567 ^ side*0x9E3779B9`, re-derived every OnStart), decided-shot
  storage + arm guard (`TryGetDecidedShot` consumed by the referee),
  `TennisPlayerState` refresh, sight config, and the graph-blackboard plumbing
  (`FindTennisGraph` / `WriteBB*` — the referee publishes through these).
- `RenderTest_TennisMatchComponent` (referee, order 130, OnLateUpdate only):
  unchanged except `PublishBlackboards` + `ResetForNewBall` now target the
  GRAPH blackboards (same 13 `RenderTest_TennisBB` keys; entity keys written
  only when valid — an unpublished key reads back as the INVALID sentinel).
- `Zenith_AIAgentComponent` SURVIVES on the NPCs as perception registrar + nav
  host (its enable flag doubles as the graph tick gate); it never gets a tree
  and its BT-asset string must stay empty (OnStart self-disable trap).

## Terrain indirect-count compatibility gate (Phase 7 / the A/B test)

The **graphics-required** `TerrainIndirectCompatibility` automated test
(`Tests/TerrainIndirectCompatibility.cpp`, `m_bRequiresGraphics = true`)
is the pixel-level proof the plan's acceptance criteria asks for. The
dedicated `RunTerrainIndirectCompatibility.ps1` wrapper launches THREE
separate processes (`--indirect-count-mode=auto`, `padded`, and `single`),
so worker recording never observes a mutable backend mode. Each arm writes
`terrain_<mode>.tga`, stdout/stderr, and `result_<mode>.json` under
`Build/artifacts/rendertest/terrain_indirect/`. Null and the reserved D3D12
stub skip; the wrapper requires a real Vulkan graphics result and hard-fails
on validation/assert/device-loss markers, missing artifacts, skipped/failed
JSON, or non-zero exit.

Three independent gates run in every arm:

1. **Terrain-specific non-vacuity** — the test forces TAA off and selects
   terrain debug mode 13, an unlit flat-magenta sentinel emitted only by the
   terrain G-buffer shaders. At least a fixed 3% of the inset editor viewport
   must match that sentinel; generic non-black final-frame pixels, sky, UI,
   and other meshes do not count. Teardown restores the prior debug mode and
   clears the TAA override.
2. **Stale-tail proof** — test-cadence `DownloadBufferData` walks the visible
   count and **all 4,096 indirect-command records** after a deterministic
   many -> few -> cull-all -> many sequence. Few must be strictly smaller
   than returned-many; cull-all must read count 0 with all 4,096 five-word
   records zero; both few and return tails must also be entirely zero.
3. **Tier + image equivalence** — the test computes the expected execution
   tier from the Vulkan backend's actual usable capabilities, `TOTAL_CHUNKS`,
   `ZERO_PADDED_TO_MAX`, and the immutable request, then requires exactly
   that telemetry counter and no other tier. The wrapper independently checks
   the logged tier and compares auto-vs-padded plus auto-vs-single over RGB
   only, cropped to the logged inset viewport (alpha/UI excluded), against
   checked-in fixed mean, p99.9, and maximum-delta budgets. A second
   terrain-sentinel silhouette gate caps symmetric-difference coverage and
   requires a minimum mask IoU, so a localized missing/ghost region cannot be
   diluted by unrelated viewport pixels. No first-run baseline or candidate-
   derived threshold exists.

   The wrapper additionally requires the three arms to run three **distinct**
   tiers — `auto` exactly `NATIVE_COUNT`, `padded` exactly `PADDED_MULTI`,
   `single` exactly `PADDED_SINGLE` — and fails loudly on a host that cannot
   supply all three. The C++ check alone is not sufficient for the A/B half:
   it requires each arm to run whatever tier the selector chose, and on a
   device without usable native count the selector legitimately gives `auto`
   → `PADDED_MULTI`, at which point auto-vs-padded compares the fallback
   against **itself** and passes vacuously on two byte-identical captures
   (likewise `padded` → `PADDED_SINGLE` on a device without
   `multiDrawIndirect`, colliding with the `single` arm). Such a device is
   not a valid host for this gate.

The wrapper also runs `--list-automated-tests` first to assert the test
is registered (the `ZENITH_AUTOMATED_TEST_REGISTER` macro fires from the
test .cpp's anonymous-namespace static-init, so moving the test TU into
the build's compiled-out region would silently drop it — the discovery
gate catches that).

The ordinary smoke matrix remains a resource/streaming/LOD gate rather than a
pixel test, but its forced arms are not flag-only: immediately before PASS the
runtime logs `RENDERTEST_SMOKE_INDIRECT_TIER` and independently requires the
Core request to match the immutable Vulkan override plus exactly one expected
recorder telemetry tier. The PowerShell runner parses the same evidence. Each
retry uses distinct attempt logs; a timed-out attempt is killed, drained, and
scanned for hard markers before any retry, so validation evidence is preserved.

The manual-only, graphics-required `TerrainIndirectPerformance` automated test
(`Tests/TerrainIndirectPerformance.cpp`) and
`RunTerrainIndirectPerformance.ps1` form the performance collector. The wrapper
defaults to the optimized Vulkan **Release Tools=False** executable and launches
one fresh process per repeat with a fixed 1/60 dt and one of three deterministic
photo-camera poses (`dense`, `horizon`, `culled`). Each repeat warms for at least
120 frames, then writes at least 300 consecutive per-frame rows containing the
three exact terrain GPU-pass timings, total GPU time, the matching CPU
`Flux Record Pass` label totals, GPU-capture serial, and indirect-recorder
telemetry deltas. The wrapper requires at least three repeats, validates test
discovery/result JSON/exit status/log markers/sample completeness/tier telemetry,
and emits raw repeat CSVs plus combined CSV and JSON (per-repeat and aggregate
median/p95) under
`Build/artifacts/rendertest/terrain_indirect/performance/`.
The `culled` pose uses pitch `1.50`, the same cull-all pose whose zero visible
count is independently proved by `TerrainIndirectCompatibility`; the performance
loop itself performs no count-buffer readback or GPU-idle stall.

The collector does not silently ratify budgets. Optimized Tools=False reports
are marked `budgetEligible=true` but still say an explicit baseline-ratification
change is required; Debug, Tools=True, custom, and `-DeveloperSanity` runs are
labelled diagnostic and `budgetEligible=false`. `-DeveloperSanity` is the only
way to lower the 120/300/3 protocol floor and exists solely for a quick launch,
parser, and artifact-schema check.

```powershell
# Full optimized protocol (builds Release Vulkan Tools=False by default)
Games\RenderTest\RunTerrainIndirectPerformance.ps1 -Mode auto -CameraCase dense

# Quick non-budgetable developer check
Games\RenderTest\RunTerrainIndirectPerformance.ps1 -Mode padded -CameraCase horizon `
  -Warmup 4 -Sample 4 -Repeats 1 -DeveloperSanity -NoBuild
```

## Tests

- **Units**: `Components/RenderTest_Tennis.Tests.inl` (pure decision cores +
  brain/referee relocation + standalone node tests via hand-built
  `Zenith_GraphContext` + the integration fixture, which attaches the real
  TennisBrain graph by path), `RenderTest_PlayerComponent.Tests.inl`
  (camera/movement input-sim tests + the fire/reload VERB tests — ★ its fixture
  drives the ACTION FRAME CONTRACT itself, because the components it steps read
  `g_xEngine.Actions()` and a unit never runs the main loop that opens and closes
  it; without that every action reads "not held" and the failures point anywhere
  but at input), `RenderTest_Testbed.Tests.inl`. Included at the bottom of RenderTest.cpp.
  Units-at-boot in rendertest.exe: run deliberately only (`--skip-unit-tests`
  everywhere else; the task_726cc81d layout corruption has tripped here on
  some layouts — 2026-07-05 post-conversion layout runs clean).
- **Automated tests** (`Tests/`): EngineBootShutdownSmoke, MaterialBattleTest,
  TerrainEditorSmoke(+Showcase), TAAToggleStress, HumanShowcase, the W3
  characterizations `Test_TennisCharacterization.cpp` — `RT_TennisMatchFlow`
  (match plays: phases, serve, receiver stand-in, point resolution) and
  `RT_PlayerActions` (walk-to-gun with real held input, E equip, LMB fire,
  R reload, E drop, T camera cycle — state-setters only, never the reentrant
  simulator helpers; ★ its steer publishes `SimulateKeyDown`/`Up` EDGES rather
  than `SetKeyHeld`, because MOVE's key rows are fed by ordered transitions and
  a level-only "hold" would reach `IsKeyDown` and nothing else — see ZM
  TestPlan C1a), both of which reload scene 0 in their Boot step so the sim
  runs entirely under fixed dt — and the hermetic `Test_TennisBrainContract.cpp`
  (`RT_TennisBrainTickCadence` / `RT_TennisBrainGateOrder` /
  `RT_TennisBrainRngDraws`, the R2 gate above), which load no scene at all —
  plus `RT_SceneAssetIntegrity` (`Tests/SceneAssetIntegrity.cpp`), which asserts on
  the scene FILE the boot left behind rather than on the loaded scene, so it reddens
  in whichever config damaged the asset — and `RT_SimPad_Test`
  (`Tests/Test_SimPad.cpp`), the GAMEPAD column of the action table end to end
  (see *Controls* below), and `TerrainIndirectCompatibility` (`Tests/TerrainIndirectCompatibility.cpp`,
  `m_bRequiresGraphics = true`, Phase 7 of the indirect-count compatibility plan
  — see the dedicated section above for its three-process wrapper + non-vacuity
  + stale-tail readback), plus the manual-only `TerrainIndirectPerformance`
  (`Tests/TerrainIndirectPerformance.cpp`) collector described above.

## Controls (the action table)

Every control is an ACTION registered in **`RenderTest_Bindings.h`** — the one
production file in this game allowed to spell a raw key, mouse button or pad
code (input program C2). Two profiles, `P_DESKTOP {KEYBOARD|MOUSE}` and
`P_GAMEPAD {GAMEPAD}`, and the active one switches automatically on the first
input from the other device.

| Action | Keyboard | Mouse | Gamepad |
|---|---|---|---|
| MOVE | W/A/S/D + arrows | — | Left stick |
| SPRINT | Shift (either) | — | L3 |
| JUMP ★ | Space | — | A |
| AIM | — | Right button | LT (0.55/0.45 hysteresis) |
| FIRE | — | Left button | RT (0.55/0.45 hysteresis) |
| INTERACT (gun pick-up/drop) | E | — | X |
| RELOAD | R | — | LB |
| CYCLE_TENNIS_CAMERA | T | — | R3 |
| LOOK_DELTA | — | Mouse movement | — |
| LOOK_RATE | — | — | Right stick |

★ JUMP is **one** action queried two ways: `WasPressedThisFrame` gives the jump
pop, `IsHeld` gives jetpack thrust — the retired C++ polled Space both ways on
the same frame and the design says they are one button.

LOOK is deliberately **two** actions because a mouse and a stick are different
quantities: LOOK_DELTA is the mouse's already-integrated DISPLACEMENT (pixels;
multiplying it by dt would make the camera frame-rate dependent) and LOOK_RATE
is the stick's deflection, which only becomes an angle after multiplying by dt.
`RenderTest_FollowCameraComponent` applies both every frame; a still mouse and a
resting stick each contribute exactly zero, so there is no mode to switch
between. The camera stays free-cursor: MOUSE_DELTA is not claim-filtered.

`RT_SimPad_Test` (`Tests/Test_SimPad.cpp`, `requiresGraphics=false`) is the only
thing that exercises the GAMEPAD column: it proves the auto profile switch, then
drives MOVE (left stick, steering to the pistol), INTERACT (pad X equips it),
FIRE (right trigger past the hysteresis band, clip decrements) and JUMP (pad A,
asserted both as an action edge and as a physical rise). The two graph-consumed
rows in it are the only guard on the `OnActionPressed` name strings, which have
no compile-time link to `RenderTest_Bindings.h`.

### Recipes

```powershell
# Build (solution target; PowerShell, never Git-Bash for msbuild)
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" `
  C:\dev\Zenith\Games\RenderTest\rendertest_win64.sln -t:RenderTest `
  -p:Configuration=Vulkan_vs2022_Debug_Win64_True -p:Platform=x64 -m -v:minimal -nologo

# One characterization, WINDOWED (headless skips Flux; StickFigure GPU state)
cd Games\RenderTest\build\output\win64\vulkan_vs2022_debug_win64_true
.\rendertest.exe --automated-test RT_TennisMatchFlow --skip-unit-tests --skip-tool-exports

# The three hermetic brain-contract tests (1 frame each; no scene, no physics)
.\rendertest.exe --automated-tests `
  RT_TennisBrainTickCadence,RT_TennisBrainGateOrder,RT_TennisBrainRngDraws `
  --skip-unit-tests --skip-tool-exports

# Full windowed batch (manual-only tests skip themselves)
.\rendertest.exe --all-automated-tests --skip-unit-tests --skip-tool-exports --fixed-dt 0.01666
```

Gotchas: the tools boot re-authors + saves the scene every run (budget minutes
of wall clock before the harness Setup fires); `--skip-tool-exports` needs one
prior full tools run to have baked the testbed assets; if the exe dies with
0xC0000135 recopy assimp DLLs from `Tools/Middleware/assimp/debug/bin` and
slang DLLs from `Middleware/slang/bin`; `--exit-after-frames` REPLACES every
test's maxFrames (don't pass it "for safety"); **a test whose Setup calls
`SetEditorMode(EditorMode::Stopped)` MUST restore `Playing` in its Teardown** —
the harness enters Playing exactly once at boot and `ResetSessionForNextTest`
deliberately never normalises the mode, so Stopped leaks into every following
test, where `Zenith_MainLoop` skips game logic entirely and the rebuilt world's
entities are awoken but never get `OnStart` (this is what made `HumanShowcase`
fail after `GrassShowcase` while passing alone; `MaterialBattleTest` had the same
latent leak). `TerrainEditorSmoke`'s
long-standing "did not re-stream HIGH after eviction" failure was FIXED
(2026-07-08): its sculpt site was a stale `(400,400)` left behind when the
campus was recentred from the `(256,256)` corner to the terrain centre
`(2048,2048)` — the site sat ~2300m from the now-centred editor camera
`(2048,52,2044)`, beyond the 1000m HIGH-LOD range, so the edited chunk never
streamed HIGH and the residency assert was unsatisfiable. The fix applies the
same `+fSHIFT (1792)` the sibling terrain tests use. NOTE: windowed RenderTest
boot needs the engine `Zenith/Assets/` textures (Cubemap/Fonts/Water/Particles)
present — if absent the skybox cubemap yields an "Invalid SRV" boot abort.

## Tennis CLI

`--rendertest-tennis-spectator`, `--rendertest-tennis-follow[=near|far]`,
`--tenniscam-x/y/z/yaw/pitch=`, `--rendertest-tennis-ikshowcase=serve|forehand|backhand`,
`--rendertest-tennis-telemetry[=<base>]` (recorder gated on scene name
"RenderTest"). T cycles the spectator camera at runtime (via the PlayerActions
graph). Match telemetry + analytics: `Components/RenderTest_TennisTelemetry.h`.
