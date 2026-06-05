# CityBuilder

A SimCity / Cities: Skylines-style city builder. **Free-form** curved-spline roads
placeable anywhere (not grid-aligned), **road-relative zoning** (R/C/I painted onto
frontage lots), **demand-driven building growth** with level-up, **utilities**
(power/water with capacity caps + network reach + **conduit chains** that extend reach),
**services** (police/fire/health/education/parks with coverage), **garbage** (landfills) +
**sewage** (treatment plants) + **public transport** (bus depots + **routed lines with
stops**) + **mail** (post offices), **freight**
(industry supplies commerce), **disasters** (building fires, fought by fire stations), a
**city economy** (build costs + tax income − upkeep → treasury, **loans**, **population-
milestone grants**, adjustable **tax rate**), **happiness**, **pollution**, **traffic
congestion**, **districts + policy
ordinances** (recycling / free-transit / pollution-control / parks-mandate, city-wide or
per-district), **rolling-hills terrain** that **roads carve** and the player can
**terraform**, **save/load**, and **visual traffic** (cars driving the splines, A*
routing). All gameplay logic is pure data/logic with headless `Zenith_AutomatedTest`
coverage; the presentation renders the city with engine debug primitives and lets
the player build with the mouse.

The original **grid** foundation (CB_CityGrid / CB_RoadNetwork / CB_BuildingManager /
CB_PresentationView / CB_SimulationTick) is retained but **gated off** behind
`CB_USE_LEGACY_GRID` (= 0 in `Source/CB_Config.h`); the free-form systems below
replace it. Full rebuild plan: `~/.claude/plans/citybuilder-full-tidy-hoare.md`.

## Build + autonomous test gate

```
cd Build && Sharpmake_Build.bat                        # regen after adding .cpp files
msbuild zenith_win64.sln /t:CityBuilder /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount:1
pwsh ./Tools/run_cb_tests.ps1 -Headless                # logic tests (CI-style)
pwsh ./Tools/run_cb_tests.ps1 -Filter CB_HumanSession  # windowed full human playthrough
pwsh ./Tools/run_cb_tests.ps1 -Filter CB_CityGrow      # windowed free-form render check
```

Gate = build-green + every test `passed:true`. Windowed tests are
`m_bRequiresGraphics=true` (the picker + render need the live camera + window), so
they are skipped headless and only run in the windowed pass.

## Controls

- **Camera:** right-drag orbit, middle-drag / WASD pan, wheel zoom, Q/E rotate.
- **Road (5):** left-click two ground points → a smooth tangent-continuous spline
  segment; endpoints snap to nearby nodes (junctions); right-click ends the road.
- **Zones (1 Residential · 2 Commercial · 3 Industrial · 4 Park):** drag-paint onto
  frontage lots along the roads.
- **Utilities/services:** 7 Power plant · 8 Water tower · 6 Services (click to place;
  press 6 again to cycle Police → Fire → Hospital → School → Landfill → Sewage Plant →
  Bus Depot → Post Office).
- **9 Bulldoze** (nearest road) · **0 None** · **P** pause.
- **T Terraform:** hold **LMB** to raise / **RMB** to lower the ground under the cursor.
- **B District:** left-click paints a circular district; **F1-F4** toggle the four policy
  ordinances (Recycling / Free Transit / Pollution Control / Parks Mandate) — on the
  current district while the District tool is active, else city-wide.
- **L Transit line:** left-click adds a stop to the current bus line; right-click starts a
  new line. Stops give transit *reach* — only people near a stop ride.
- **K Utility conduit:** left-click lays a conduit node; a connected chain carries power +
  water from a source out to buildings beyond its radius (energized = cyan in the overlay).

## Free-form architecture (`Source/`)

```
CB_Config.h                # CB_USE_LEGACY_GRID switch (0 = free-form)
CB_Spline.h                # cubic Bézier in XZ: Evaluate/Tangent/UnitTangent/Length/Distance
CB_RoadGraph.{h,cpp}       # nodes + spline segments; snap/junction/ref-count/bulldoze; serialize
CB_RoadMesh.h              # terrain-following road-ribbon triangle generation
CB_RoadController.{h,cpp}  # owns the graph; the road/zone/service/bulldoze tools (reads the
                           #   camera-unproject picker); services sub-type cycle; ribbon render
CB_Zoning.{h,cpp}          # per-segment frontage lots; paint R/C/I; zone overlay; serialize
CB_BuildingPlacement.{h,cpp} # the city sim: demand-driven growth + level-up; service-building
                           #   placement; utilities/coverage/garbage/sewage/transit/pollution/
                           #   congestion/happiness/economy/policy-effects; render; serialize
CB_Policy.h                # the four policy ordinances (enum + names)
CB_Districts.h             # painted regions + city/district policy masks; GetPolicyMaskAt(x,z);
                           #   the sim scales each policy by its building-coverage fraction; serialize
CB_TransitLines.h          # bus lines = ordered stops; IsNearAnyStop gates transit ridership reach
CB_Conduits.h              # utility conduits; Energize() floods power/water along connected chains,
                           #   IsPowered/IsWatered extends source reach to far buildings; serialize
CB_Traffic.{h,cpp}         # vehicle pool driving the spline network + A* (FindPath) over the graph
CB_TerrainGen.h            # the ONE shared hill-height function (bake + heightfield agree); flip-proof
CB_RoadTerrain.{h,cpp}     # roads CARVE the terrain: SurfaceHeight (hill, levelled+recessed under
                           #   roads) → FlattenHeightfield (CPU) + CarveTerrainMesh (re-upload GPU chunks)
CB_SaveLoadFreeform.h      # serialize the whole free-form city (graph + zoning + buildings)
CB_Serialize.h             # Zenith_Vector <-> Zenith_DataStream helpers
CB_TerrainHeightfield.h    # CPU heightfield: GetHeightAt + runtime deform brushes (raise/lower/
                           #   flatten/smooth; shaped to CB_TerrainGen hills; drives the terraform tool)
CB_BuildingDefs.h          # 20 building types (RCI + power/water/police/fire/hospital/school/park/
                           #   landfill/sewage/bus-depot/post-office) + tuning (constexpr table)
CB_ToolIcons.h             # toolbar icon filenames + hover-tooltip text (shared: generator + HUD)
CB_ToolIconGen.h           # procedural toolbar-icon drawing (tools-build): white glyph + outline -> RGBA8
```

## HUD / toolbar (`CB_CityManager_Behaviour::BuildGameUI`)

The in-game HUD (built on the Zenith UI suite, rendered in ALL configs incl. `_False`)
is a SimCity/C:S-style layout: a top info bar (treasury/tax/pop/jobs/happiness/buildings/
services) + speed controls, a top-right info panel (power/water/pollution/traffic/garbage/
sewage/fires), a bottom-left RCI demand meter, and a **bottom tool palette** — one button
per tool with a **procedurally-generated icon** + a **hover tooltip** describing the tool.
The UI **rebuilds on canvas-size change** (`m_fUIBuiltW/H`) so the pixel-anchored layout
tracks window resize / DPI.

Icons are drawn at tools-build (`CB_EnsureUIIcons` in `CityBuilder.cpp`, fired from
`Project_RegisterScriptBehaviours` under `ZENITH_TOOLS`, version-marker-gated) into
`Assets/UI/Icons/cb_<name>.ztxtr` and shown via a **`UIImage` overlay** centred on each
button — NOT `UIButton`'s `ICON_ONLY` (which doesn't mark the texture bindless, so it never
renders; `UIImage` does). See memory `reference-zenith-ui-icon-textures`. Capture
screenshots **DPI-aware** (`SetProcessDPIAware()`) or the bottom/right of the HUD is cropped
out of the shot (see `reference-screen-capture-and-primitive-winding`). The windowed
`CB_UIShowcase` test builds a small city + rotates a simulated hover across tools so the
icons + tooltips can be screenshotted.

Behaviours (`Components/`): `CB_CityManager_Behaviour` owns every subsystem and drives
sim + tools + render + HUD + traffic each frame; `CB_CityCamera_Behaviour` (RTS camera);
`CB_DayNightCycle_Behaviour` (advances the clock + drives the sky sun intensity).

The **RCI demand** is super-critical (residents → jobs → more residents), so a mixed
R/C/I district grows until a constraint bites: **power/water capacity** (place plants/
towers to lift the cap), **solvency** (`treasury > 0`), or **land** (zoned lots). High
density (level-up) additionally needs **land value** (happiness from service coverage).

## Tests (`Tests/`)

- **Free-form headless logic:** `CB_SplineRoads`, `CB_ZoningLots`, `CB_BuildingGrow`,
  `CB_CityServices` (power-gating / coverage / economy / congestion / pollution / budget /
  utility-reach / **garbage+sewage** / **transit** / **policies city-wide + per-district** /
  **disasters (fire)** / **freight** / **mail** / **transit-lines** / **conduits**),
  `CB_Terrain` (heightfield brushes +
  **terraform raise/lower**), `CB_SaveLoadRoundtrip` (now also round-trips districts +
  policies), `CB_TrafficTest` (A* + drive).
- **Windowed:** `CB_RoadDraw` (curved road), `CB_CityGrow` (full free-form render),
  `CB_HumanSession` (the headline ~15s playthrough: draw a road + place a plant with the
  real tool+picker, lay a road grid, zone R/C/I, power/water/service it, watch it grow to
  a sizeable city, zone a second wave, pause/resume, bulldoze + rebuild, save + load).
- **Legacy** logic tests still run; the legacy windowed `CB_Play` is gated behind
  `CB_USE_LEGACY_GRID`.

## Conventions (engine-wide; see root CLAUDE.md)

- No `std::function`; `std::vector`→`Zenith_Vector`; no pimpl. Subsystems are headless-
  testable — tests build local instances in `Verify`.
- Behaviours: `class CB_Foo_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour` +
  `friend class Zenith_ScriptComponent;` + `ZENITH_BEHAVIOUR_TYPE_NAME(...)`; ctor takes
  `Zenith_Entity&`. Every new behaviour header must be `#include`d from `CityBuilder.cpp`
  (MSVC dead-strips unreferenced `.obj`s). CityManager init is in `OnStart`.
- Rendering is immediate-mode `g_xEngine.Primitives()` each frame, guarded by
  `!Zenith_CommandLine::IsHeadless()`; a `.cpp` calling `g_xEngine.X()` must include the
  full `*Impl` header (e.g. `Flux/Primitives/Flux_PrimitivesImpl.h`).
- **No reentrant simulator in Steps:** windowed tests use only the state-setters
  (`SimulateMousePosition` / `SimulateMouseButtonDown/Up` / `SimulateKeyPress` /
  `SetKeyHeld`); `SimulateMouseClick` / `StepFrame` deadlock the GPU.

## Terrain (hilly + carved)

The ground is **rolling hills** (`CB_TerrainGen::HillNorm` — one symmetric, flip-proof
function used by BOTH the offline 4096px heightmap bake AND the runtime `CB_TerrainHeightfield`,
at the bake's 512 height scale, so roads/buildings conform to the rendered mesh). **Roads
carve it** (`CB_RoadTerrain`, windowed) via GPU **vertex deformation**, done the RACE-FREE way:
ALL terrain edits go through the engine's terrain-STREAMING path — never an in-place write to a
live, GPU-read resident chunk (that corrupts DISTANT terrain with speckled sky-holes even behind a
`waitIdle`; the streaming system isn't built for mid-frame in-place edits — this was the
long-standing bug). The CPU `CB_TerrainHeightfield` is the single source of truth (hills +
`FlattenHeightfield` road recess + terraform brushes). Mechanism:
1. An **engine stream-in hook** — `Flux_TerrainStreamingState::m_pfnChunkVertexHook`, called in
   `StreamInLOD` after a chunk's baked mesh loads + before its GPU upload — re-shapes every HIGH
   chunk to the live heightfield as a DELTA on the fine baked vertex (`field - HillFieldSample`;
   off-edit delta == 0 so the baked 1m hill is preserved exactly). The game registers
   `CB_RoadTerrain::ChunkVertexCarveHook` (pUser = the `CB_TerrainHeightfield`; the engine holds a
   raw pointer, unregistered in `OnDestroy`). So newly-streamed chunks are always carved.
2. On a road change (or terraform) the chunks already resident under the edit are evicted
   (`ForceRestreamCarveChunks` / `RestreamTerraformRegion` → engine `EvictLOD`) so they re-stream
   and the hook re-shapes them on reload. Eviction zeroes the chunk-data so the chunk isn't drawn
   during the evict->reload gap — no race. (The old in-place `CarveTerrainMesh` /
   `RefreshTerrainRegionFromField` + `waitIdle` paths are superseded + dead, pending deletion.)

To change the terrain shape: edit `HillNorm` + delete
`Terrain/terrain_hills_v1.marker` (forces a one-time re-bake on the next windowed `_True` run).

The player can also **terraform** (the `T` tool, or `CB_CityManager_Behaviour::TerraformAt`
for automation): a raise/lower brush edits the heightfield, then `RestreamTerraformRegion`
re-streams the brushed chunks so the same stream-in hook re-shapes them (race-free, identical
machinery to the road carve).
Caveat: `CB_RoadTerrain::FlattenHeightfield` (run on a road change) rewrites the whole field
from hill+road, so terraform off-road is wiped by a later road edit — terraform last, or away
from where you'll lay road.

## City systems (sim)

The sim (`CB_BuildingPlacement::UpdateSimState`, each growth pass) derives, on top of RCI
growth: **utilities** (power/water caps + network reach), **5 coverage services**,
**garbage** (people generate it; landfills collect — overflow erodes happiness), **sewage**
(water-use generates it; treatment plants clear it), **mail** (post offices collect it),
**public transport** (bus depots carry commuters → less road congestion; **transit LINES**
with stops gate which population actually rides — routing matters), **utility conduits**
(`CB_Conduits` floods power/water from a source along a connected chain to far buildings), **freight**
(industrial goods supply commerce — undersupply cuts commercial tax income), **disasters**
(buildings catch fire; a covering fire station extinguishes them, otherwise the building is
razed — `ProcessDisasters` / `TriggerFireAt` / `GetFiresDestroyed`), **pollution** (industry
up, parks down), **congestion** (people vs road length, minus transit), **economy** (tax ×
rate, upkeep, loans, milestone grants), and **policy ordinances**. Each policy (`CB_Policy`) is applied scaled by the
fraction of buildings it covers — a **district** (`CB_Districts`) policy hits only its area,
a city-wide policy hits everything. Happiness folds in coverage, utilities, congestion,
pollution, garbage, sewage and the parks-mandate policy.

## Remaining / deferred

- **Real textured/instanced building + vehicle models** — asset-blocked (no external spend,
  CC0/free only); the asset-free substitute is procedural building variety (`EmitBuilding`).
- Road carves now **persist** across streaming via the `StreamInLOD` engine hook (see Terrain).
  A *terraformed* (non-road) HIGH chunk that re-streams still reverts — the hook re-applies the
  road `SurfaceHeight`, not the player's terraform delta; terraform is best near the resident city.
- Literal feature parity with two commercial games is open-ended (cargo-by-rail, district
  styles, detailed citizen lifepaths, …). Every system named across the build rounds —
  zoning, roads, utilities + conduits, all services, garbage/sewage/mail, transit + lines,
  freight, disasters, districts + policies, terrain carve + terraform, budget/loans — is
  implemented + headless-tested + exercised by `CB_HumanSession`.
