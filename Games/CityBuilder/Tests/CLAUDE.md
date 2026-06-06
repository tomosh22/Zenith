# CityBuilder — Tests/

`Zenith_AutomatedTest` coverage for the game. Every test is gated `#ifdef ZENITH_INPUT_SIMULATOR`
and registered with `ZENITH_AUTOMATED_TEST_REGISTER`. A registration's 6th field is
`m_bRequiresGraphics` — **false = headless logic** (runs under `--headless`), **true = windowed**
(needs the live camera/window + picker; skipped headless).

The headless gate (run in `_True`) is currently **675 engine unit tests + 45 CityBuilder automated
tests, all green**. Get the live list with `citybuilder.exe --list-automated-tests`; counts here are a
guide, not a contract (several files register many tests each, e.g. `CB_CityServices.cpp`).

## Running

```
# headless logic gate — MUST run in _True (the _True automation path guards the terrain GPU-culling
# init; in _False the City scene's terrain deserialization asserts "Invalid buffer VRAM handle" at boot)
msbuild Build/zenith_win64.sln /t:CityBuilder /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount:1
citybuilder.exe --all-automated-tests --headless --exit-after-frames 6000 --fixed-dt 0.01666 --test-results-dir <dir>

# a single windowed test (build _False for clean screenshots — no ImGui)
citybuilder.exe --automated-test CB_HumanSession --exit-after-frames 9000 --fixed-dt 0.01666 \
    --skip-tool-exports --skip-unit-tests --test-results <json>

# or the runner (headless discovery only)
pwsh ./Tools/run_cb_tests.ps1 -Headless
pwsh ./Tools/run_cb_tests.ps1 -Filter CB_HumanSession
```

## Conventions (CRITICAL)

- **No reentrant simulator in a Step.** A `Step` runs *inside* `Zenith_MainLoop`, so use only the
  state-setters — `SimulateMousePosition`, `SimulateMouseButtonDown/Up`, `SimulateMouseWheel`,
  `SimulateKeyPress`, `SetKeyHeld`. The frame-advancing verbs `SimulateMouseClick` / `StepFrame`
  re-enter `BeginFrame` → `vkWaitForFences` **deadlock** (windowed only). Call the picker / tools
  directly instead.
- **Headless logic tests build local instances** in `Setup`/`Verify` and assert on them — they do not
  need the live scene. Windowed tests drive the live `CB_CityManager_Behaviour` via the static accessors.
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

## Windowed tests (`m_bRequiresGraphics = true`)

| Test | File | ~Frames | What it does |
|---|---|---|---|
| **CB_HumanSession** | `CB_HumanSession.cpp` | 4000 (~67s) | **The headline pure-input playthrough.** Drives the game ONLY through `Zenith_InputSimulator` (mouse moves/clicks/wheel + keys, zero direct subsystem calls) to build a sizeable city and exercise EVERY mechanic. Authored as a flat `g_xScript` of `Act`s processed one/frame; `PROBE` actions snapshot state + `Verify` asserts. `SetMouseFrac` applies a `SPREAD` factor so the crossing grid's blocks are large enough for correct (non-overlapping) frontage. Asserts a solvent, served (~60-building) city AND that each mechanic fired AND road-graph parity (1 component, ≥4 junctions) AND traffic telemetry AND **placement-ghost telemetry** (`residentialToolGhosts>0`, `noToolGhosts==0`). |
| CB_CityGrow | `CB_CityGrow.cpp` | ~520 | free-form road grid + R/C/I zoning + utilities + services; asserts segments/buildings/pop/services after live growth. |
| CB_RoadDraw | `CB_RoadDraw.cpp` | ~300 | draw a curved, tangent-continuous multi-segment road via the live controller; asserts ≥3 segments + a curved one. |
| CB_RoadGhost | `CB_RoadGhost.cpp` | ~1000 | the road-draw ghost preview: sweep the cursor (green preview), park on a node (cyan snap); asserts preview vertices. |
| CB_UIShowcase | `CB_UIShowcase.cpp` | ~1000 | build a small city, rotate a simulated hover across toolbar buttons so icons + tooltips show (screenshot aid). |
| CB_TerrainShowcase | `CB_TerrainShowcase.cpp` | ~3200 | roads/zones on rolling terrain; tilts the camera low-oblique to show hill relief (default top-down masks it); holds for a screenshot. |

## Notes

- Windowed screenshots: capture via PowerShell `SetWindowPos` topmost + `CopyFromScreen` on the
  citybuilder window (PrintWindow is black for Vulkan); be DPI-aware. Mid-run frames can show a
  GDI-on-Vulkan capture speckle over the terrain — a capture artifact, not a render bug; capture many
  frames and pick clean ones (see memory `reference-screen-capture-and-primitive-winding`).
- Adding a test: drop a `.cpp` here, register inside `#ifdef ZENITH_INPUT_SIMULATOR`, run Sharpmake
  (`Build/Sharpmake_Build.bat`), rebuild. Set `m_bRequiresGraphics = true` only if it needs the live
  camera/window/picker.
