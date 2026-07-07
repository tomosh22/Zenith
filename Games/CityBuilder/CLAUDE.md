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
**terraform**, **save/load**, and **demand-driven traffic** (SimCity/C:S model: cars are
home→job/shop **trips** routed by A*, scaling with population, congesting busy roads — a
bare network carries none). All gameplay logic is pure data/logic with headless `Zenith_AutomatedTest`
coverage; the presentation renders buildings as colour-tinted instanced cube meshes and everything
else (roads, zone overlay, placement ghosts, services, traffic) with engine debug primitives, and lets
the player build with the mouse.


## Documentation map

This file is the overview (build, controls, terrain, city systems, conventions). The per-file detail
lives in per-directory docs (engine convention):

| Doc | Covers |
|---|---|
| [`Source/CLAUDE.md`](Source/CLAUDE.md) | every gameplay system — roads (spline/graph/mesh/controller/tools), zoning, buildings + defs, the sim, districts/policies/transit/conduits, traffic, terrain (gen/heightfield/carve/modifier), save/serialize, zones/sim-speed/events/telemetry/icons/camera/day-night. Public types, key API, constants, invariants. |
| [`Components/CLAUDE.md`](Components/CLAUDE.md) | the 3 game components — `CB_CityManagerComponent` (orchestrator: owned subsystems, lifecycle, **static accessors**, HUD, hotkeys), `CB_CityCameraComponent`, `CB_DayNightCycleComponent`. |
| [`Tests/CLAUDE.md`](Tests/CLAUDE.md) | the test suite — headless logic vs windowed, how to run (incl. the `_True`-only headless gate), the no-reentrant-simulator rule, and `CB_HumanSession` (the pure-input playthrough). |

### Directory layout
```
Games/CityBuilder/
  CityBuilder.cpp        # Project_* hooks: terrain/material/icon asset bake + City scene creation
  CLAUDE.md              # this overview
  Source/                # headless gameplay systems (CB_*.{h,cpp}) — see Source/CLAUDE.md
  Components/            # the 3 game ECS components — see Components/CLAUDE.md
  Tests/                 # Zenith_AutomatedTest coverage — see Tests/CLAUDE.md
  Assets/                # Scenes (City.zscen), Terrain (baked heightmap + chunk meshes), UI/Icons
  build/                 # generated solution output
```
`CityBuilder.cpp` bakes the terrain (4096px heightmap from `CB_TerrainGen::HillNorm` + 4 materials +
splatmap, marker-gated `terrain_hills_vN`) and the 20 toolbar icons (`CB_UI_ICONS_VERSION`, tools-build),
registers the 3 game components, disables SS*/shadows/fog (keeps skybox + IBL), and loads the City scene
(build index 0).

## Build + autonomous test gate

```
zenith regen                                           # regen after adding .cpp files
msbuild Games\CityBuilder\citybuilder_win64.sln /t:CityBuilder /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount
zenith test CityBuilder --headless                     # logic tests (CI-style)
zenith test CityBuilder --filter CB_HumanSession       # windowed full human playthrough
zenith test CityBuilder --filter CB_CityGrow           # windowed free-form render check
```

Gate = build-green + every test `passed:true`. Windowed tests are
`m_bRequiresGraphics=true` (the picker + render need the live camera + window), so
they are skipped headless and only run in the windowed pass.

## Controls

- **Camera:** right-drag orbit, middle-drag / WASD pan, wheel zoom, Q/E rotate.
- **Road (5):** SimCity/C:S-style — left-click a start point, then a **ghost preview**
  (a green spline footprint, **cyan** when it would snap to an existing node) follows the
  cursor; left-click again confirms the segment and continues from there; right-click ends
  the road. Endpoints snap to nearby nodes; **AUTO-INTERSECTIONS** form like SimCity/C:S —
  where a road crosses an existing road both split at the crossing + share a new junction node
  (X-junction), and a road ending mid-span on another splits it (T-junction), so the network is
  always a connected graph (`CB_RoadGraph::AddSegmentWithJunctions` / `SplitSegmentAt` /
  `FindOrSplitNodeAt`; `CB_Spline::SplitAt`/`SubSpline`/`SegSegIntersect`). Curves are tangent-continuous.
  The ghost is `CB_RoadController`'s `RebuildPreview` (hover from `PickGroundPoint` each
  frame, or `SetHoverPointForAutomation` in tests — see `CB_RoadGhost`); it is drawn as a
  colour-keeping sphere outline because flat lit primitives wash to white (memory
  `reference-screen-capture-and-primitive-winding`).
- **Zones (1 Residential · 2 Commercial · 3 Industrial · 4 Park):** drag-paint onto
  frontage lots along the roads. While an R/C/I tool is selected the game renders **ghosts
  of the available placement lots** (a flat slab in the tool's colour on every open, unzoned
  frontage lot — `CB_Zoning::RenderPlacementGhosts`), so you can see exactly where a zone can
  go before painting; they clear as lots are zoned/built or when the tool is deselected.
  Telemetry: `CB_CityManagerComponent::GetLastGhostCount()` (asserted in `CB_HumanSession`).
- **Utilities/services:** 7 Power plant · 8 Water tower · 6 Services (click to place;
  press 6 again to cycle Police → Fire → Hospital → School → Landfill → Sewage Plant →
  Bus Depot → Post Office).
- **9 Bulldoze** (nearest road) · **0 None** · **P** pause · **, / .** speed down/up.
- **T Terraform:** hold **LMB** to raise / **RMB** to lower the ground under the cursor.
- **Economy / session hotkeys:** **R** cycle road class (small→medium→large) · **- / =** tax
  down/up · **G** take a development loan · **F5 / F9** quick save / load · **F8** new (clear)
  city · **F** ignite a fire under the cursor (disaster drill). The whole game is
  keyboard-operable so a player — or the `CB_HumanSession` test — can drive every mechanic
  from the keyboard + mouse, purely through `Zenith_InputSimulator`.
- **B District:** left-click paints a circular district; **F1-F4** toggle the four policy
  ordinances (Recycling / Free Transit / Pollution Control / Parks Mandate) — on the
  current district while the District tool is active, else city-wide.
- **L Transit line:** left-click adds a stop to the current bus line; right-click starts a
  new line. Stops give transit *reach* — only people near a stop ride.
- **K Utility conduit:** left-click lays a conduit node; a connected chain carries power +
  water from a source out to buildings beyond its radius (energized = cyan in the overlay).

## Architecture (`Source/`)

```
CB_Zones.h                 # CB_EZoneType (Residential/Commercial/Industrial/Park) — shared enum
CB_SimSpeed.h              # CB_ESimSpeed + CB_SpeedMultiplier (pause/normal/fast/ultra clock)
CB_Spline.h                # cubic Bézier in XZ: Evaluate/Tangent/UnitTangent/Length/Distance
CB_RoadGraph.{h,cpp}       # nodes + spline segments; snap, AUTO-INTERSECTIONS (AddSegmentWithJunctions /
                           #   SplitSegmentAt / FindOrSplitNodeAt → X + T junctions), ref-count/bulldoze;
                           #   connectivity telemetry (CountConnectedComponents / CountJunctions); serialize
CB_RoadMesh.h              # terrain-following road-ribbon triangle generation
CB_ToolSystem.{h,cpp}      # CB_ETool enum (14 tools) + the terrain-aware mouse picker
                           #   (PickGroundPoint ray-marches the heightfield); tool-selection hotkeys
CB_RoadController.{h,cpp}  # owns the graph; the road/zone/service/bulldoze tools (reads the
                           #   camera-unproject picker); services sub-type cycle; ribbon render
CB_Zoning.{h,cpp}          # per-segment frontage lots (placed with IsLotPositionClear: kept clear of
                           #   EVERY road carriageway + intersections + other lots, so zones/buildings never
                           #   overlap a road or each other — esp. at junctions + between close parallels);
                           #   paint R/C/I; zone overlay; placement-zone ghosts (RenderPlacementGhosts: open
                           #   lots when an R/C/I tool is active); serialize
CB_BuildingPlacement.{h,cpp} # the city sim: demand-driven growth + level-up; service-building
                           #   placement; utilities/coverage/garbage/sewage/transit/pollution/
                           #   congestion/happiness/economy/policy-effects; render; serialize
CB_Policy.h                # the four policy ordinances (enum + names)
CB_Districts.h             # painted regions + city/district policy masks; GetPolicyMaskAt(x,z);
                           #   the sim scales each policy by its building-coverage fraction; serialize
CB_TransitLines.h          # bus lines = ordered stops; IsNearAnyStop gates transit ridership reach
CB_Conduits.h              # utility conduits; Energize() floods power/water along connected chains,
                           #   IsPowered/IsWatered extends source reach to far buildings; serialize
CB_Traffic.{h,cpp}         # demand-driven OD-trip traffic (SimCity/C:S): homes→jobs/shops routed by A*
                           #   (FindPath), count scales with population, per-segment congestion + telemetry
                           #   (CB_TrafficStats). Manager passes origin/dest nodes from built lots; needs a
                           #   CONNECTED road graph — auto-intersections keep crossing roads connected
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

## HUD / toolbar (`CB_CityManagerComponent::BuildGameUI`)

The in-game HUD (built on the Zenith UI suite, rendered in ALL configs incl. `_False`)
is a SimCity/C:S-style layout: a top info bar (treasury/tax/pop/jobs/happiness/buildings/
services) + speed controls, a top-right info panel (power/water/pollution/traffic/garbage/
sewage/fires), a bottom-left RCI demand meter, and a **bottom tool palette** — one button
per tool with a **procedurally-generated icon** + a **hover tooltip** describing the tool.
The UI **rebuilds on canvas-size change** (`m_fUIBuiltW/H`) so the pixel-anchored layout
tracks window resize / DPI.

Icons are drawn at tools-build (`CB_EnsureUIIcons` in `CityBuilder.cpp`, fired from
`Project_RegisterGameComponents` under `ZENITH_TOOLS`, version-marker-gated) into
`Assets/UI/Icons/cb_<name>.ztxtr` and shown via a **`UIImage` overlay** centred on each
button — NOT `UIButton`'s `ICON_ONLY` (which doesn't mark the texture bindless, so it never
renders; `UIImage` does). See memory `reference-zenith-ui-icon-textures`. Capture
screenshots **DPI-aware** (`SetProcessDPIAware()`) or the bottom/right of the HUD is cropped
out of the shot (see `reference-screen-capture-and-primitive-winding`). The windowed
`CB_UIShowcase` test builds a small city + rotates a simulated hover across tools so the
icons + tooltips can be screenshotted.

Game components (`Components/`): `CB_CityManagerComponent` owns every subsystem and drives
sim + tools + render + HUD + traffic each frame; `CB_CityCameraComponent` (RTS camera);
`CB_DayNightCycleComponent` (advances the clock + drives the sky sun intensity).

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
  `CB_HumanSession` (the headline **pure-input** playthrough: drives the game ONLY through
  `Zenith_InputSimulator` — simulated mouse moves/clicks/wheel + key presses, with NO direct
  subsystem calls — to build a sizeable city and exercise EVERY mechanic: roads (all 3
  classes), R/C/I zones + parks, all 11 utility/service types (8 via the key-6 service
  cycle, power + water on keys 7/8, parks via zoning), districts +
  the 4 policies, transit lines, conduits, terraform, bulldoze, loans/tax, speed/pause,
  save/load + a fire drill. Authored as a flat input "script" (`g_xScript`) processed one
  action per frame; PROBE actions snapshot state + Verify asserts a solvent, served city
  (~60 buildings) AND that each mechanic fired. ~43s windowed).

## Conventions (engine-wide; see root CLAUDE.md)

- No `std::function`; `std::vector`→`Zenith_Vector`; no pimpl. Subsystems are headless-
  testable — tests build local instances in `Verify`.
- Game components: plain classes (no base) satisfying the component contract — ctor takes
  `Zenith_Entity&`, `WriteToDataStream`/`ReadFromDataStream`, `RenderPropertiesPanel` under
  `ZENITH_TOOLS`; registered via `ZENITH_REGISTER_COMPONENT(Type, "Name", order)` in
  `CityBuilder.cpp` + mirrored into the editor registry under `ZENITH_TOOLS`. Every new
  component header must be `#include`d from `CityBuilder.cpp` (MSVC dead-strips
  unreferenced `.obj`s). CityManager init is in `OnStart`. Components publishing statics
  that hold member addresses need hand-written moves (see `Components/CLAUDE.md`).
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

The hills are gentle rolling (`HillNorm` ≈ 20..150m, ~10..18° slopes — "slight", not
mountainous), with higher-frequency terms (~0.55..1.1km wavelengths) so there is visible
local relief near the city, not just one ~4km swell (which reads flat). They only read from
an oblique camera — the default near-top-down view masks gentle slopes (see the windowed
`CB_TerrainShowcase` test, which tilts `CB_CityCameraComponent::GetActive()` low + zoomed
out for a screenshot).

To change the terrain shape: edit `HillNorm` + **bump the marker version** in
`CityBuilder.cpp` (`terrain_hills_vN.marker`, currently **v4**) — that forces a one-time
re-bake on the next **windowed `_True`** run (`CB_EnsureTerrainAssets` re-writes the 4096px
heightmap + chunk meshes; takes a few minutes). `_False` only loads the baked chunks, so a
`_True` run must bake first.

The player can also **terraform** (the `T` tool, or `CB_CityManagerComponent::TerraformAt`
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

## Building rendering (current)

R/C/I buildings render as **GPU-instanced cube meshes** with a lit PBR material, coloured per zone via
the per-instance albedo tint — **residential green, commercial blue, industrial yellow** (burning =
orange); service buildings take their own colours. This is the DevilsPlayground material approach
(albedo, **no emissive**) and replaced the old washed-out debug-primitive boxes. The shared unit-cube
mesh + white material are process-lifetime singletons; the instances live on a transient `CityBuildings`
entity, rebuilt each frame from the live city by `CB_BuildingPlacement::RenderInstanced` (the manager
passes `m_pxBuildingInst`). Roads/zone-overlay/placement-ghosts/services/traffic still use
`g_xEngine.Primitives()`. See `Source/CLAUDE.md` (CB_BuildingPlacement) + `Components/CLAUDE.md`.

## Remaining / deferred

- **Real textured ART models** for buildings + vehicles — asset-blocked (no external spend, CC0/free
  only). Buildings already use instanced cube *meshes* with materials (above); the deferred item is
  authored detailed models. Vehicles are still debug primitives.
- Road carves now **persist** across streaming via the `StreamInLOD` engine hook (see Terrain).
  A *terraformed* (non-road) HIGH chunk that re-streams still reverts — the hook re-applies the
  road `SurfaceHeight`, not the player's terraform delta; terraform is best near the resident city.
- Literal feature parity with two commercial games is open-ended (cargo-by-rail, district
  styles, detailed citizen lifepaths, …). Every system named across the build rounds —
  zoning, roads, utilities + conduits, all services, garbage/sewage/mail, transit + lines,
  freight, disasters, districts + policies, terrain carve + terraform, budget/loans — is
  implemented + headless-tested + exercised by `CB_HumanSession`.
