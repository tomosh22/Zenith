# RenderTest

The renderer/engine feature testbed: a 4096 m procedural terrain campus (seed
1337) hosting an IK step-cube deck, a material showcase, a gun pickup/drop
testbed, a jetpack, a StickFigure third-person player, and the **autonomous
tennis court** (two AI NPCs playing a rule-correct physics match under a
passive referee). Single scene, build index 0 (`Assets/Scenes/RenderTest.zscen`),
fully re-authored + saved by editor automation every tools boot.

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
inputs, same-frame nav-destination consumption). Pinned by
`RT_TennisDeterminismDigest`: an FNV-1a digest over 2400 fixed-dt frames of
[both brain RNG states, referee jitter RNG, phase/epoch/points/games/serve
state, quantized ball position], self-aligned on the first SERVING frame of
epoch 1. The pinned value `0x9551B81E8F74B8AE` was captured from the C++/BT
baseline (two identical runs) and the graph build reproduces it bit-for-bit.
Anything that changes brain-tick cadence, Selector gate order, or RNG draw
counts breaks this test — that is its job.

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
  TerrainEditorSmoke(+Showcase), TAAToggleStress, HumanShowcase, and the W3
  characterizations `Test_TennisCharacterization.cpp`:
  `RT_TennisMatchFlow` (match plays: phases, serve, receiver stand-in, point
  resolution), `RT_TennisDeterminismDigest` (the R2 gate above),
  `RT_PlayerActions` (walk-to-gun with real held input, E equip, LMB fire,
  R reload, E drop, T camera cycle — state-setters only, never the reentrant
  simulator helpers). All three reload scene 0 in their Boot step so the sim
  runs entirely under fixed dt.

### Recipes

```powershell
# Build (solution target; PowerShell, never Git-Bash for msbuild)
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe" `
  C:\dev\Zenith\Build\zenith_win64.sln -t:RenderTest `
  -p:Configuration=Vulkan_vs2022_Debug_Win64_True -p:Platform=x64 -m -v:minimal -nologo

# One characterization, WINDOWED (headless skips Flux; StickFigure GPU state)
cd Games\RenderTest\build\output\win64\vulkan_vs2022_debug_win64_true
.\rendertest.exe --automated-test RT_TennisDeterminismDigest --skip-unit-tests `
  --skip-tool-exports --fixed-dt 0.01666

# Full windowed batch (manual-only tests skip themselves)
.\rendertest.exe --all-automated-tests --skip-unit-tests --skip-tool-exports --fixed-dt 0.01666
```

Gotchas: the tools boot re-authors + saves the scene every run (budget minutes
of wall clock before the harness Setup fires); `--skip-tool-exports` needs one
prior full tools run to have baked the testbed assets; if the exe dies with
0xC0000135 recopy assimp DLLs from `Tools/Middleware/assimp/debug/bin` and
slang DLLs from `Middleware/slang/bin`; `--exit-after-frames` REPLACES every
test's maxFrames (don't pass it "for safety"); `TerrainEditorSmoke`'s
re-stream failure is pre-existing (proven by A/B on 2026-07-05, tracked
separately) and unrelated to the tennis conversion.

## Tennis CLI

`--rendertest-tennis-spectator`, `--rendertest-tennis-follow[=near|far]`,
`--tenniscam-x/y/z/yaw/pitch=`, `--rendertest-tennis-ikshowcase=serve|forehand|backhand`,
`--rendertest-tennis-telemetry[=<base>]` (recorder gated on scene name
"RenderTest"). T cycles the spectator camera at runtime (via the PlayerActions
graph). Match telemetry + analytics: `Components/RenderTest_TennisTelemetry.h`.
