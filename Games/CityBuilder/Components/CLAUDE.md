# CityBuilder — Components/ (behaviours)

The three `Zenith_ScriptBehaviour` classes that turn the headless `Source/` systems into a
playable game. Each follows the engine behaviour pattern:

```cpp
class CB_Foo_Behaviour ZENITH_FINAL : public Zenith_ScriptBehaviour
{
    friend class Zenith_ScriptComponent;
    ZENITH_BEHAVIOUR_TYPE_NAME(CB_Foo_Behaviour)
    CB_Foo_Behaviour(Zenith_Entity& /*xParent*/) {}   // do NOT forward to a base ctor
    ...
};
```

Every behaviour header is `#include`d from `../CityBuilder.cpp` so MSVC's dead-strip doesn't drop
the static-init registrar before link. Init happens in **`OnStart`** (the scene is *deserialized*,
so the behaviour attaches on load — `OnAwake` does not fire for deserialized entities).

See `../Source/CLAUDE.md` for the subsystems these drive and `../CLAUDE.md` for controls/overview.

---

## CB_CityManager_Behaviour.h — the orchestrator

The largest behaviour. It **owns every subsystem by value**, drives the whole frame, builds + updates
the HUD, and publishes static accessors so tests/automation can reach the live city.

### Owned subsystems (members)
- **Gameplay:** `CB_RoadController m_xRoadCtrl`, `CB_Zoning m_xZoning`, `CB_BuildingPlacement m_xBuild`, `CB_Districts m_xDistricts`, `CB_TransitLines m_xTransit`, `CB_Conduits m_xConduits`, `CB_Traffic m_xTraffic`, `CB_ToolSystem m_xTools`, `CB_TerrainHeightfield m_xHeightfield`, `CB_ESimSpeed m_eSpeed` (the sim clock).
- **Render-only:** a shared unit-cube `Flux_MeshGeometry` + `Flux_MeshInstance` + white `Zenith_MaterialAsset` (process-lifetime statics), plus `m_pxBuildingInst` on a transient `CityBuildings` entity (the instanced building meshes).
- Carve/traffic state: `m_xCarveCtx`, `m_pxTerrain`, `m_auTrafficOrigins/Dests`, `m_uLastCarveSegs`.

### Lifecycle
- **`OnStart`** — shape `m_xHeightfield` to `CB_TerrainGen::HillNorm`; reset road/zoning/districts/transit/conduits/build/traffic; publish all static pointers; create the windowed `CityBuildings` instanced-mesh entity (`EnsureBuildingMeshAssets`); point `m_xTools.SetTerrainField(&m_xHeightfield)` (terrain-aware picker).
- **`OnUpdate(fDt)`** — hotkeys (incl. pause/speed → `m_eSpeed`) → `m_xTools.Update()` → `m_xRoadCtrl.Update` → `m_xZoning.SyncToGraph` → `m_xBuild.Tick` (when `m_eSpeed != PAUSED`) → milestone checks. Then (windowed only): road carve on change (`FlattenHeightfield`+`RegisterStreamHook`+`ForceRestreamCarveChunks`), terraform, district/transit/conduit overlays, `m_xRoadCtrl.Render`, `m_xZoning.RenderOverlay`, **placement-ghost render when an R/C/I tool is active** (→ `s_uLastGhostCount`), `m_xBuild.RenderInstanced`, traffic update+render, HUD (re)build/update.
- **`OnDestroy`** — unregister the stream hook, null all static pointers.

### Helper methods
`NewCity()` (F8), `CycleSpeed(±1)`, `SaveCity()`/`LoadCity()` (`cb_quicksave_freeform.dat`, F5/F9), `BuildTrafficEndpoints()` (home/job lots → road nodes for OD trips), `UpdateTerraform()` (T tool brush + throttled re-stream), `RestreamTerraformRegion`, `UpdateDistrictTool()`/`RenderDistrictOverlay()`, `UpdateTransitTool()`/`RenderTransitOverlay()`/`RenderConduitOverlay()`, `EnsureTerrainPtr()`, `EnsureBuildingMeshAssets()`, `BuildGameUI`/`UpdateGameUI`/`SelectUITool`. Constants `TERRAFORM_RADIUS = 45.0`, `POP_PER_CAR = 30`.

### Static accessors (for tests / automation / `CB_TerrainModifier`)
```
GetActive()              → CB_CityManager_Behaviour*   (live manager, null if none)
GetActiveRoadController() → CB_RoadController*
GetActiveZoning()        → CB_Zoning*
GetActiveBuild()         → CB_BuildingPlacement*
GetActiveDistricts()     → CB_Districts*
GetActiveTransit()       → CB_TransitLines*
GetActiveConduits()      → CB_Conduits*
GetActiveTraffic()       → CB_Traffic*
GetActiveHeightfield()   → CB_TerrainHeightfield*
GetUpdateCount()         → uint32_t            (frames ticked — liveness)
GetLastGhostCount()      → uint32_t            (placement ghosts drawn last frame; 0 unless an R/C/I tool is active)
```
Member accessors also exist (`GetSpeed`/`SetSpeed`, `GetTools`, `GetBuild`, `GetZoning`, `GetRoadController`, …) for in-process use.

### HUD (built on the Zenith UI suite, renders in ALL configs incl. `_False`)
SimCity/C:S layout: top info bar (treasury/tax/pop/jobs/happiness/buildings/services) + speed buttons,
top-right info panel (power/water/pollution/traffic/garbage/sewage/fires), bottom-left RCI demand meter,
and a bottom **tool palette** — one button per tool with a procedurally-generated icon (`UIImage` overlay,
NOT `UIButton ICON_ONLY`) + a hover tooltip. Rebuilds on canvas-size change (`m_fUIBuiltW/H`) so it tracks
resize/DPI. Tool/speed button callbacks are static, take `void*` userdata (tool packed as byte0=tool,
byte1=service).

### Hotkeys (all real bindings — also the surface `CB_HumanSession` drives)
`P` pause · `R` road class · `-`/`=` tax · `G` loan · `,`/`.` speed · `F5`/`F9` save/load · `F8` new city ·
`F` ignite fire · `1`–`9`/`0`/`T`/`B`/`L`/`K` tool select · `F1`–`F4` district policies (current district if
the B tool is active, else city-wide).

### Gotchas
- Buildings render as **instanced cube meshes** (per-instance albedo tint = zone colour, no emissive — DevilsPlayground style); the `CityBuildings` entity is transient (not serialized). `m_pxBuildingInst` is created windowed only.
- All `g_xEngine.*` render/picker calls are inside `!Zenith_CommandLine::IsHeadless()` guards; the sim/logic runs headless.
- Terrain carve is race-free via the streaming hook + eviction only — never an in-place write to a resident chunk.

---

## CB_CityCamera_Behaviour.h — RTS camera

Drives a `CB_CameraController` from input and writes the result into the entity's `Zenith_CameraComponent`
each frame. `OnAwake` resets the controller (fresh per Play session); `OnUpdate` applies input; `GetController()`
+ static `GetActive()` expose it (the showcase tests tilt the active camera for screenshots).

- **Controls:** right-drag orbit (yaw+pitch), `Q`/`E` yaw, middle-drag pan, `W`/`A`/`S`/`D` pan, wheel zoom.
- Tunables: `m_fZoomSpeed=20`, `m_fKeyRotateSpeed=1.5`, `m_fMouseRotateSpeed=0.005`, `m_fPanSpeed=0.6`, `m_fMouseDragPanSpeed=0.0015`. Pan + zoom scale by orbit distance so the feel is consistent across zoom.

---

## CB_DayNightCycle_Behaviour.h — day/night

Advances a `CB_DayNight` clock each frame and (windowed only) drives the skybox sun intensity
(`0.2 + 2.6 × elevation`). `OnStart` creates + publishes the clock; `OnUpdate` advances + applies;
`OnDestroy` clears the static pointer. `GetCycle()` + static `GetActive()`. Headless: the clock still
advances; only the skybox call is skipped.
