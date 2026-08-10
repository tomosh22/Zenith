# RenderTest

The renderer/engine feature testbed: a 4096 m procedural terrain campus (seed
1337) hosting an IK step-cube deck, a material showcase, a gun pickup/drop
testbed, a jetpack, a StickFigure third-person player, and the **autonomous
tennis court** (two AI NPCs playing a rule-correct physics match under a
passive referee). Single scene, build index 0 (`Assets/Scenes/RenderTest.zscen`),
fully re-authored + saved by editor automation every **windowed** tools boot.

**A headless (`Null_`) boot authors the same scene MINUS its instanced trees** —
`Zenith_TerrainEditor::EnsureTreeEntities` refuses to run without a GPU, so the
`TerrainTrees_Trunk` / `_Leaves` entities (82 authored entities windowed, 80
headless; ~323 KB of the 361721-byte file) never exist. It therefore does not
publish: `Zenith_Editor::SaveActiveScene`'s publish guard refuses the lossy save
(logging both entity counts) and the run loads the committed scene instead, trees
included. Until that guard existed, EVERY headless run silently rewrote the tracked
asset down to ~38 KB. Consequences worth knowing:

* **Re-authoring the scene needs a WINDOWED tools boot.** A headless boot will not
  pick up an authoring change — it will log `REFUSED headless save` and load the
  committed bytes. That is the intended failure mode, not a bug.
* **Per-run harness entities are spawned transient, post-load**, never authored
  before `AddStep_SaveScene` (`RenderTest_EnterSmokePlayMode` creates
  `RenderTestSmokeRunner` this way). Authoring one would write it into the tracked
  asset on every `--rendertest-smoke` run.
* `RT_SceneAssetIntegrity` (`Tests/SceneAssetIntegrity.cpp`) guards all of the
  above by inspecting the file on disk after boot: both tree entities present, the
  campus + tennis testbed present, no `RenderTestSmokeRunner`.

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
   compiled once under the pin.

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
| `RenderTest_PlayerActions.bgraph` | Player | engine input sources | The discrete PRESS decisions only: E → `RTPlayerInteractGun`, R → `RTPlayerTryReload`, LMB press → `RTPlayerTryFire`, T → `RTPlayerCycleTennisCam`. Holds (WASD / Shift sprint / Space jump+jetpack / RMB ADS) and all systems stay C++. |

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

## Tests

- **Units**: `Components/RenderTest_Tennis.Tests.inl` (pure decision cores +
  brain/referee relocation + standalone node tests via hand-built
  `Zenith_GraphContext` + the integration fixture, which attaches the real
  TennisBrain graph by path), `RenderTest_PlayerComponent.Tests.inl`
  (camera/movement input-sim tests + the fire/reload VERB tests),
  `RenderTest_Testbed.Tests.inl`. Included at the bottom of RenderTest.cpp.
  Units-at-boot in rendertest.exe: run deliberately only (`--skip-unit-tests`
  everywhere else; the task_726cc81d layout corruption has tripped here on
  some layouts — 2026-07-05 post-conversion layout runs clean).
- **Automated tests** (`Tests/`): EngineBootShutdownSmoke, MaterialBattleTest,
  TerrainEditorSmoke(+Showcase), TAAToggleStress, HumanShowcase, the W3
  characterizations `Test_TennisCharacterization.cpp` — `RT_TennisMatchFlow`
  (match plays: phases, serve, receiver stand-in, point resolution) and
  `RT_PlayerActions` (walk-to-gun with real held input, E equip, LMB fire,
  R reload, E drop, T camera cycle — state-setters only, never the reentrant
  simulator helpers), both of which reload scene 0 in their Boot step so the sim
  runs entirely under fixed dt — and the hermetic `Test_TennisBrainContract.cpp`
  (`RT_TennisBrainTickCadence` / `RT_TennisBrainGateOrder` /
  `RT_TennisBrainRngDraws`, the R2 gate above), which load no scene at all —
  plus `RT_SceneAssetIntegrity` (`Tests/SceneAssetIntegrity.cpp`), which asserts on
  the scene FILE the boot left behind rather than on the loaded scene, so it reddens
  in whichever config damaged the asset.

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
