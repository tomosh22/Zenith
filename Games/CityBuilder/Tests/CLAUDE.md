# CityBuilder — Tests/

`Zenith_AutomatedTest` coverage for the game. Every test is gated `#ifdef ZENITH_INPUT_SIMULATOR`
and registered with `ZENITH_AUTOMATED_TEST_REGISTER`. A registration's 6th field is
`m_bRequiresGraphics` — **false = runs on the Null (headless) build**, **true = windowed only**
(it marks tests whose ASSERTIONS read GPU-produced output). CityBuilder has none — every test here
registers `false`, which is why the headless and windowed suites run the identical set.

Get the live list with `citybuilder.exe --list-automated-tests`; counts here are a
guide, not a contract (several files register many tests each, e.g. `CB_CityServices.cpp`).
Observed 2026-08-11 on `Null_vs2022_Debug_Win64_True`: **47 CityBuilder automated tests, 0 failed**
(headless and windowed run the same set — no CB test is `requiresGraphics`), and the engine unit
gate booted from the same exe reports `1591 ran, 1590 passed, 0 failed, 1 skipped`.

> ★ `zenith test CityBuilder` REWRITES the tracked repo-root file
> `cb_quicksave_freeform.dat` — `CB_HumanSession` drives the real F5 quick-save. After ANY CB test
> run, `git status` and `git checkout -- cb_quicksave_freeform.dat` before you commit anything.

## Running

```
# headless logic gate — MUST run in _True (the _True automation path guards the terrain GPU-culling
# init; in _False the City scene's terrain deserialization asserts "Invalid buffer VRAM handle" at boot)
msbuild Build/zenith_win64.sln /t:CityBuilder /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount
citybuilder.exe --all-automated-tests --exit-after-frames 6000 --fixed-dt 0.01666 --test-results-dir <dir>
# (run the Null_vs2022_Debug_Win64_True exe -- headless is a build config, not a flag)

# a single windowed test (build _False for clean screenshots — no ImGui)
citybuilder.exe --automated-test CB_HumanSession --exit-after-frames 9000 --fixed-dt 0.01666 \
    --skip-tool-exports --skip-unit-tests --test-results <json>

# or the runner (headless discovery only)
pwsh ./Tools/run_cb_tests.ps1 -Headless
pwsh ./Tools/run_cb_tests.ps1 -Filter CB_HumanSession
```

## Conventions (CRITICAL)

> ## ★ INPUT IN A CB TEST IS AN EDGE, NOT A LEVEL (C1a)
>
> Since the input migration, every control is a C2 ACTION registered in
> `../CB_Bindings.h`, and the key rows behind them are fed by the **ordered
> transition log**, not sampled as a level at close time.
>
> `Zenith_InputSimulator::SetKeyHeld` writes ONLY the simulator's held table. It
> reaches `Zenith_Input::IsKeyDown` and **nothing else** — the action layer never
> sees the key go down, so `IsHeld` / `GetAxis1D` / `GetAxis2D` answer "not held"
> for the whole window and the test fails pointing anywhere but at its input.
> Publish `SimulateKeyDown` / `SimulateKeyUp` instead (`SimulateKeyPress` is
> already an edge pair and was always fine). `CB_HumanSession`'s `A_HOLD` /
> `A_UNHOLD` actions were converted for exactly this reason — the Q/E camera
> ROTATE row is a `KeyAxis1D`, which is transition-fed.
>
> A **Step** that reads back an action edge sees the state closed at the PREVIOUS
> frame's 10e (a Step runs at `PumpAutomatedTest`, before this frame's step 7/8),
> so assert in the Step AFTER the one that injected — C1b. Game logic at step 11
> still sees the edge in the same frame it was injected.

- **No reentrant simulator in a Step.** A `Step` runs *inside* `Zenith_MainLoop`, so use only the
  state-setters — `SimulateMousePosition`, `SimulateMouseButtonDown/Up`, `SimulateMouseWheel`,
  `SimulateKeyPress`, `SimulateKeyDown/Up`, `SimulateGamepad*`. The frame-advancing verbs
  `SimulateMouseClick` / `StepFrame` re-enter `BeginFrame` → `vkWaitForFences` **deadlock**
  (windowed only). Call the picker / tools directly instead.
- **Headless logic tests build local instances** in `Setup`/`Verify` and assert on them — they do not
  need the live scene. Windowed tests drive the live `CB_CityManagerComponent` via the static accessors.
- **Determinism:** run with `--fixed-dt 0.01666`; the sim core has no RNG so a seeded city replays.
- `static const Zenith_AutomatedTest g_xT` lands in read-only memory — never `const_cast` + write back.

## Headless logic tests (logic-only)

| File | Covers |
|---|---|
| `CB_Boot.cpp` | scene boots: CityManager `OnStart` fired, `OnUpdate` ticking, main camera resolves. |
| `CB_SplineRoads.cpp` | `CB_Spline` (eval/length/distance) + `CB_RoadGraph` (snap, junction, remove). |
| `CB_ZoningLots.cpp` | frontage lots sync from segments, paint hits lots in radius, segment-remove frees lots. |
| `CB_BuildingGrow.cpp` | demand-driven growth (mixed R/C/I + utilities → buildings/pop/jobs); road-remove despawns. |
| `CB_CityServices.cpp` | the big sim suite — power gating, coverage, economy, congestion, pollution, budget/loans, utility reach, garbage+sewage, transit, **policies (city-wide + district)**, disasters (fire), freight, mail, transit-lines, conduits. |
| `CB_TrafficTest.cpp` | `CB_Traffic` A* shortest path, vehicles drive (no teleport), and **`CB_RoadJunctions`** (crossing grid → 1 component + ≥4 junctions, T-junction, A* corner-to-corner). |
| `CB_Terrain.cpp` | heightfield flat/raise/flatten brushes + terraform raise/lower + the live hill-shaped field. |
| `CB_RoadCarve.cpp` | road carve recesses the corridor by `BED_DEPTH`, off-road terrain preserved. |
| `CB_SaveLoadRoundtrip.cpp` | save/load round-trip (graph+zoning+buildings+districts+transit+conduits); re-serialize is byte-stable. |
| `CB_DayNightSpeed.cpp` | `CB_DayNight` clock + sun-direction math; `CB_SpeedMultiplier` table (0 / 0.5 / 1 / 2 / 4). |
| `CB_SimPad.cpp` | **the GAMEPAD column** of `../CB_Bindings.h`, end to end. The only test here that publishes a pad event, so every pad row would otherwise regress in silence. Proves the auto profile switch to `P_GAMEPAD` (nothing below resolves without it), then drives PAN (left stick → the live `CB_CameraController`'s target moves), ORBIT_RATE (right stick → its yaw moves), TOOL_NEXT (d-pad Right → a real tool change through `SelectUITool`) and PAUSE (pad Start → the sim clock). Also pins the DOCUMENTED d-pad/UI-nav overlap: the same press must raise BOTH `TOOL_NEXT` and the engine-reserved `UI_NAV_RIGHT`. Restores the tool + clock in `Verify`. |

## Live-scene tests (drive the loaded City scene + its camera; all still `m_bRequiresGraphics = false`)

| Test | File | ~Frames | What it does |
|---|---|---|---|
| **CB_HumanSession** | `CB_HumanSession.cpp` | 4000 (~67s) | **The headline pure-input playthrough.** Drives the game ONLY through `Zenith_InputSimulator` (mouse moves/clicks/wheel + keys, zero direct subsystem calls) to build a sizeable city and exercise EVERY mechanic. Authored as a flat `g_xScript` of `Act`s processed one/frame; `PROBE` actions snapshot state + `Verify` asserts. `SetMouseFrac` applies a `SPREAD` factor so the crossing grid's blocks are large enough for correct (non-overlapping) frontage. Asserts a solvent, served (30+ building) city AND that each mechanic fired AND road-graph parity (1 component, ≥4 junctions) AND traffic telemetry AND **placement-ghost telemetry** (`residentialToolGhosts>0`, `noToolGhosts==0`). |
| CB_CityGrow | `CB_CityGrow.cpp` | ~420 | free-form road grid + R/C/I zoning + utilities + services; asserts segments/buildings/pop/services after live growth. |
| CB_RoadDraw | `CB_RoadDraw.cpp` | ~240 | draw a curved, tangent-continuous multi-segment road via the live controller; asserts ≥3 segments + a curved one. |
| CB_RoadGhost | `CB_RoadGhost.cpp` | ~900 | the road-draw ghost preview: sweep the cursor (green preview), park on a node (cyan snap); asserts preview vertices. |
| CB_UIShowcase | `CB_UIShowcase.cpp` | ~900 | build a small city, rotate a simulated hover across toolbar buttons so icons + tooltips show (screenshot aid). |
| CB_TerrainShowcase | `CB_TerrainShowcase.cpp` | ~3000 | roads/zones on rolling terrain; tilts the camera low-oblique to show hill relief (default top-down masks it); holds for a screenshot. |

## Notes

- Windowed screenshots: capture via PowerShell `SetWindowPos` topmost + `CopyFromScreen` on the
  citybuilder window (PrintWindow is black for Vulkan); be DPI-aware. Mid-run frames can show a
  GDI-on-Vulkan capture speckle over the terrain — a capture artifact, not a render bug; capture many
  frames and pick clean ones (see memory `reference-screen-capture-and-primitive-winding`).
- Adding a test: drop a `.cpp` here, register inside `#ifdef ZENITH_INPUT_SIMULATOR`, run Sharpmake
  (`Build/Sharpmake_Build.bat`), rebuild. Set `m_bRequiresGraphics = true` only if it needs the live
  camera/window/picker.
