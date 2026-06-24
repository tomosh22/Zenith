# CityBuilder — Source/ (gameplay systems)

The pure data/logic layer of the CityBuilder game. Every system here is **engine-light**
(no `g_xEngine.*` calls except clearly-marked windowed render helpers) so it is
**headless unit-testable** — tests build local instances and assert on them. The behaviours
in `../Components/` own these systems and drive them each frame; the entry point
`../CityBuilder.cpp` bakes assets + creates the scene. See `../CLAUDE.md` for the game
overview and controls.

The city is built on **free-form spline roads** (curved roads placeable anywhere, not a grid) with
**road-relative zoning**, demand-driven growth, and a full city sim — all listed below.

Conventions (engine-wide): no `std::function` (function pointers), `std::vector`→`Zenith_Vector`,
no pimpl. Immediate-mode rendering via `g_xengine.Primitives()` is guarded by
`!Zenith_CommandLine::IsHeadless()` and lives only in the behaviours / `Render*` helpers.

---

## Roads (free-form spline network)

### CB_Spline.h
Header-only cubic **Bézier curve in the world XZ plane** (`.x`=X, `.y`=Z; Y is sampled from
terrain separately). Each road segment owns one. Pure math, no engine dep.
- `m_axControl[4]` — P0 start, P1/P2 handles, P3 end.
- `Evaluate(t)`, `Tangent(t)`, `UnitTangent(t)` (chord fallback), `Length(samples=24)`.
- `DistanceToPoint(xP, samples=24)` — min XZ distance to the curve (used by road carving + **lot clearance**).
- `ClosestParam(xP, samples=24)` — t of the closest point (T-junction placement).
- `SplitAt(t, left, right)` — exact de Casteljau split; `SubSpline(t0,t1)` — exact sub-curve.
- static `Straight(A,B)`, `Curved(A,dirA,B,dirB)` (handle length = |A→B|/3), `SegSegIntersect(p,p2,q,q2,&tP,&tQ)`, `Distance`, `PointSegmentDistance`.

### CB_RoadGraph.{h,cpp}
Topological network: ref-counted **nodes** + **segments** (spline + width by class), both soft-deleted
(`m_bActive`; slot indices stay stable as mesh-cache keys). **Auto-intersections** keep the graph
connected like SimCity/C:S.
- `enum CB_ERoadClass { CB_ROADCLASS_SMALL=0, CB_ROADCLASS_MEDIUM=1, CB_ROADCLASS_LARGE=2, CB_ROADCLASS_COUNT }` → widths **8 / 12 / 16 m** (`ClassWidth`).
- `CB_RoadNode { m_xPos, m_uRefs, m_bActive }`; `CB_RoadSegment { m_uNodeA, m_uNodeB, m_xSpline, m_fWidth, m_eClass, m_bActive }`.
- `AddNode`, `FindNodeNear`, `FindOrAddNode(snap)`, `FindOrSplitNodeAt(snap)` (snap node / T-split / new), `AddSegment`, `RemoveSegment` (frees orphan nodes at ref 0).
- **`AddSegmentWithJunctions(a,b,spline,class)`** — finds every crossing of the new centreline with active segments (16-sample polylines, skip t∈[0,0.02]∪[0.98,1]), **splits BOTH at each crossing**, inserts junction nodes, chains the new road through them (X-junctions).
- `SplitSegmentAt(seg,t)` (0.01<t<0.99) → new junction node (T-junctions).
- `FindNearestSegment(x,z,maxDist)`, `MinDistanceToAnyRoad(x,z)`, `GetTotalActiveLength()` (→ congestion capacity).
- Telemetry: `CountJunctions()` (nodes with ≥3 segments), `CountConnectedComponents()` (union-find), `CountSegmentsAtNode`.
- `Clear`, `WriteToDataStream`/`ReadFromDataStream`. `INVALID = 0xFFFFFFFF`.

### CB_RoadMesh.h
Header-only namespace: turns a segment into a **terrain-following ribbon of triangles**.
- `SAMPLE_SPACING = 2.0f`; `SampleCount`, `RibbonVertexCount` (=samples×6).
- `BuildRibbon(seg, field, fYOffset, outTris)` — samples the spline, offsets ±`m_fWidth*0.5` perpendicular, lifts Y to `field.GetRenderSurfaceY` (the **fine GPU surface**, NOT the coarse field — coarse Y z-fights), winds +Y-facing tris.

### CB_RoadController.{h,cpp}
Owns the `CB_RoadGraph` and drives the **road / zone / service / bulldoze tools** + road-ribbon render +
the draw **ghost preview**.
- `Update(tools, field, zoning, build)` — per-frame tool dispatch (windowed): road = draw/continue, R/C/I = drag-paint zones, services = place, bulldoze = remove nearest. `Render()` submits ribbons (+ ghost).
- `HandleClick(wx,wz)` — anchor start, else snap/T-junction + build a tangent-continuous spline (`m_xLastDir`) + `AddSegmentWithJunctions`, continue from the end. `EndRoad()` (right-click/Esc).
- `BulldozeAt`, `SetRoadClass`/`GetRoadClass`, `GetGraph`, services sub-type cycle (`GetServiceType`/`NextServiceType`: Police→Fire→Hospital→School→Landfill→Sewage→Bus→Post on re-press of key 6), `RebuildMesh(field)`, `IsDrawing`.
- Test hooks: `SetHoverPointForAutomation(wx,wz)` (bypass picker), `GetPreviewTriVertCount`.
- Constants: `ROAD_Y_OFFSET = 0.30`, `SNAP_RADIUS = 7.0`, `MIN_SEGMENT = 2.0`, `ZONE_BRUSH_RADIUS = 22.0`.
- The ghost outline uses colour-keeping rings (flat lit fills wash to white — see memory `reference-screen-capture-and-primitive-winding`).

### CB_ToolSystem.{h,cpp}
Stateless tool state + the **terrain-aware mouse picker**. The manager calls `Update` each frame; it dispatches to the controller / zoning / building systems.
- `enum CB_ETool { CB_TOOL_NONE=0, CB_TOOL_ZONE_RES=1, CB_TOOL_ZONE_COM=2, CB_TOOL_ZONE_IND=3, CB_TOOL_ZONE_PARK=4, CB_TOOL_ROAD=5, CB_TOOL_POLICE=6, CB_TOOL_POWER=7, CB_TOOL_WATER=8, CB_TOOL_BULLDOZE=9, CB_TOOL_TERRAFORM=10, CB_TOOL_DISTRICT=11, CB_TOOL_TRANSIT=12, CB_TOOL_CONDUIT=13, CB_TOOL_COUNT }`. (1–3 cast 1:1 to `CB_EZoneType`.)
- `SetTool`/`GetTool`, `SetBrushRadius`, `SetTerrainField(field)`, `ToolName` (tooltip).
- **`PickGroundPoint(&x,&z)`** — unprojects the mouse ray and **ray-marches the heightfield** (1024 steps + 20-iter bisect vs `GetRenderSurfaceY`); falls back to a y=0 plane only if no field. This is the ONLY ray-vs-terrain intersection; without it the cursor drifts toward the horizon on hills. `Update()` only reads the tool-selection hotkeys; the free-form tools are applied by `CB_RoadController`.

---

## Zoning + buildings

### CB_Zoning.{h,cpp}
**Road-relative frontage zoning.** Each active segment generates a row of buildable **lots** on both
sides, oriented to face the road; the player paints R/C/I/Park; buildings grow in zoned+empty lots.
- `CB_Lot { m_xPos, m_xFaceDir, m_fWorldY, m_eZone, m_uDensity(0..2), m_uBuildingId, m_uSegment, m_bActive }`.
- `SyncToGraph(graph, field)` — each frame, diffs per-segment "has lots" flags; adds lots for new segments (`AddSegmentLots`), removes them for gone segments.
- **`IsLotPositionClear(pos, graph, fRoadClear, fMinLotDist)`** — a candidate lot is placed only if it is ≥ `fRoadClear` (5.5 m) beyond **every** road's half-width (so it clears its own road by ~2.5 m and stays off crossing/parallel roads + intersections) AND ≥ `fMinLotDist` (`LOT_SPACING*0.72 ≈ 10 m`) from every active lot. **This is the fix for zones overlapping roads / each other at junctions + close parallels.**
- `PaintZone(wx,wz,radius,zone,density)`, `RenderOverlay()` (filled slabs on zoned lots).
- **`RenderPlacementGhosts(eActiveZone)`** — flat slabs in the active tool's colour on every available (active, unzoned, unbuilt) lot, capped at 250; the "where can I place" affordance shown while an R/C/I tool is active. Returns the count drawn. `CountAvailableLots()` is the headless-safe count.
- `LOT_SPACING = 14.0`, `LOT_DEPTH = 14.0`. Lot `m_fWorldY` = `GetRenderSurfaceY` (fine surface). Serializable.

### CB_BuildingPlacement.{h,cpp}
**The city simulation.** Demand-driven R/C/I growth + level-up, service/utility placement, and every
derived system. Drives the RCI demand the HUD reads.
- `CB_Building { m_uLot, m_eType, m_uOccupants, m_uLevel(0..2), m_fGrowth, m_bActive, m_uFireTicks, m_xWorldPos }`; `CB_ServiceBuilding { m_xPos, m_fWorldY, m_eType, m_bActive }`.
- `Tick(zoning)` — one step (self-rate-limits to one growth pass per `GROW_INTERVAL=20` frames): despawn on removed lots → recompute utilities/coverage/garbage/sewage/mail/transit/pollution/congestion/freight/happiness/economy/policies → grow + level up (gated on power, water, treasury, land value). `PlaceService(type,x,z,y)`.
- `Render(zoning)` (primitive fallback) / **`RenderInstanced(zoning, pxInstances)`** — R/C/I as GPU-instanced cube meshes (per-instance albedo tint = zone colour green/blue/yellow; burning = orange) + services in their colours; the DevilsPlayground material approach (no emissive). Falls back to `Render` if `pxInstances` is null (headless).
- Counts: `GetActiveBuildings/Services`, `GetResidents/ComJobs/IndJobs/Jobs/Population`. Demand: `GetRes/Com/IndDemand` (0..1).
- Sim read-outs: `GetTreasury/Happiness/Power*/Water*/IsPowered/IsWatered/ServedFraction/Income/Upkeep/Congestion/Pollution/TaxRate/Debt/UtilityReach/Garbage/Sewage/TransitShare/Mail/FreightRatio/PolicyCoverage(policy)`.
- Budget: `SetTaxRate`(0.5..1.5 clamp), `TakeLoan`(+15% debt), `GrantFunds`. Infra: `SetRoadCapacity(len)` (×2 for congestion). Providers (optional): `SetDistricts`, `SetTransitLines`, `SetConduits`.
- Disasters: `GetActiveFires/FiresDestroyed`, `SetAutoDisasters(bOn)`, `TriggerFireAt(x,z)`.
- Constants: `GROW_INTERVAL=20`, `SPAWNS_PER_PASS=2`, `START_TREASURY=150000`, `BASELINE_POWER/WATER=40`, `ECON_RATE=0.10`; internal `POWER/WATER_REACH=320`, `FIRE_FIGHT_TICKS=3`, `FIRE_DESTROY_TICKS=8`. Serializable (buildings + services + treasury; occupants recomputed on load).

### CB_BuildingDefs.h
Header-only **constexpr tuning table** for 20 building types; pure lookup used only by `CB_BuildingPlacement`.
- `enum CB_EBuildingType { RES_LOW=0, RES_MED, RES_HIGH, COM_*, IND_*, POWER_PLANT, WATER_TOWER, POLICE, FIRE, HOSPITAL, SCHOOL, PARK, LANDFILL, SEWAGE_PLANT, BUS_DEPOT, POST_OFFICE=19, COUNT=20, NONE=0xFF }` (9 RCI + 11 services/utilities).
- `enum CB_EServiceType { NONE, POWER, WATER, POLICE, FIRE, HEALTH, EDUCATION, PARK, GARBAGE, SEWAGE, TRANSIT, MAIL, COUNT }`.
- `CB_BuildingDef` fields: `m_eType, m_eZone, m_uDensity, m_uMaxOccupants, m_fPowerUse` (negative = produced), `m_fWaterUse`, `m_fTaxRevenue, m_fUpkeep, m_fPollution` (negative = parks reduce), `m_eService, m_fServiceRadius`. Helpers `Get(type)`, `TypeForZone(zone,density)`, `Is{Residential,Commercial,Industrial,Service}`. **Exact numbers live in the table — read the file; do not hard-code values elsewhere.**

---

## City systems (read by CB_BuildingPlacement's sim)

### CB_Policy.h
The four ordinances: `enum CB_EPolicy { CB_POLICY_RECYCLING=0, CB_POLICY_FREE_TRANSIT=1, CB_POLICY_POLLUTION_CONTROL=2, CB_POLICY_PARKS_MANDATE=3, CB_POLICY_COUNT }`; `CB_PolicyBit(p)` (1<<i), `CB_PolicyName(p)`. Each policy's effect is scaled by the fraction of buildings it covers.

### CB_Districts.h
Painted **circular regions** carrying city-wide + per-district policy masks. `CB_District { m_fCentreX/Z, m_fRadius, m_uPolicyMask, m_bActive }`. `PaintDistrict(x,z)` (grows nearest within `MERGE_RADIUS=160`, else new at `DEFAULT_RADIUS=120`), `Set/ToggleCityPolicy`, `Set/ToggleDistrictPolicy`, **`GetPolicyMaskAt(x,z)`** (city mask OR every containing district's mask — the sim's query). Serializable.

### CB_TransitLines.h
Bus lines = ordered **stops**. `CB_TransitStop { m_fX, m_fZ, m_uLine }`. `StartLine()`, `AddStop(x,z)`, **`IsNearAnyStop(x,z)`** (within `STOP_REACH=150` — gates which population actually rides; without lines the depot-capacity model applies). Serializable.

### CB_Conduits.h
Player-laid **utility conduits** that flood power+water along a connected chain. `CB_Conduit { m_fX,m_fZ, m_bPowered, m_bWatered }`. `AddConduit(x,z)`, **`Energize(powerSources, waterSources)`** (seed within `SOURCE_DIST=200`, flood along `LINK_DIST=130` edges), `IsPowered(x,z)`/`IsWatered(x,z)` (within `SERVE_DIST=150`). Extends utility reach far beyond a source's radius. Serializable.

### CB_Traffic.{h,cpp}
**Demand-driven OD-trip traffic** (SimCity/C:S model — NOT a decorative pool). Each vehicle is a home→job/shop trip routed by A* over the graph; concurrent trips scale with population; busy segments congest.
- `CB_Vehicle { m_bActive, m_uSeg, m_uFromNode, m_uGoalNode, m_fT, m_fSpeed, m_fTripTime, m_uPathLen, m_uPathIdx, m_auPath[MAX_TRIP_SEGS], m_xPos, m_xFwd, m_xColor }`.
- `CB_TrafficStats { m_uActive, m_uTripsStarted, m_uTripsCompleted, m_uMaxSegmentLoad, m_uCongestedSegs, m_fAvgTripTime }` — the **telemetry that proves the model**.
- `Update(graph, field, fDt, originNodes, destNodes, targetVehicles)` — advance trips (congestion slows speed), despawn arrivals, spawn new trips up to the target. Empty origins/dests → zero traffic. `Render()`, `GetActiveVehicleCount`, `GetStats`. static `FindPath(graph,start,goal,&segs)` (A*, edge cost = segment length).
- `MAX_VEHICLES=80`, `MAX_TRIP_SEGS=64` (renamed off Win32 `MAX_PATH`). Per-class segment capacity gates congestion. The manager supplies origin/dest nodes (`BuildTrafficEndpoints`) + a population-scaled target (`POP_PER_CAR=30`).

---

## Terrain

### CB_TerrainGen.h
The **single shared hill-height function** used by BOTH the offline 4096px bake AND the runtime CPU field,
so baked GPU mesh + gameplay queries agree. Symmetric about centre (flip-proof).
- `HillNorm(x,z)` → normalized height (~0.04..0.29), fine (1 m) resolution.
- `HillFieldSample(x,z)` → the coarse 16 m CPU-field world-Y baseline (so `field − HillFieldSample` = the player's terraform delta to add onto the fine baked vertex).
- `TERRAIN_CENTRE = 2048`, `HEIGHT_SCALE = 512`. Gentle rolling hills (~10–18° slopes, ~0.55–1.1 km local relief) that read only from an oblique camera.

### CB_TerrainHeightfield.h
Game-owned **CPU-authoritative** height field — the source of truth for all gameplay height queries +
the terraform brush target. Pure logic, serializable.
- `Init(sx,sz,cell,ox,oz,scale)` (manager uses 257×257 @ 16 m, scale 512); `SetNormalized`/`Normalized` (shape it to `HillNorm`), `GetHeightAt(x,z)` (bilinear), **`GetRenderSurfaceY(x,z)`** (fine baked baseline + field delta — matches the GPU stream-in hook, so primitives sit flush, no z-fight).
- `CB_TerrainBrush { m_eMode(RAISE/LOWER/FLATTEN/SMOOTH), m_fCentreX/Z, m_fRadius, m_fStrength, m_fTargetWorldY }`; `ApplyBrush(brush)`, `GetDirtyCount`/`ClearDirty` (deferred GPU sync). Serializable.

### CB_RoadTerrain.{h,cpp}
Roads **carve** the terrain (level + recess a corridor). `SurfaceHeight` is the single truth used by both
the CPU field and the GPU mesh; **all GPU edits go through the race-free engine streaming hook** (NEVER an
in-place write to a live resident chunk — that corrupts distant terrain; see memory `reference-terrain-runtime-deform`).
- `CarveContext { m_xSamples, bbox, m_bActive }`; `BuildSamples`, `SurfaceHeight(samples,x,z)` (nearest centreline within `FLATTEN_RADIUS=20` → levelled − `BED_DEPTH=0.5`, else hill), `FlattenHeightfield(graph, field)`, `RebuildCarveContext`.
- `ChunkVertexCarveHook` (engine `m_pfnChunkVertexHook`, pUser = the heightfield) re-shapes each HIGH chunk on stream-in as a delta on the fine baked vertex (off-edit delta = 0 → seams preserved). `RegisterStreamHook`/`UnregisterStreamHook`; **`ForceRestreamCarveChunks(ctx, terrain)`** evicts resident chunks so they re-stream + re-carve. The old in-place `CarveTerrainMesh`/`RefreshTerrainRegionFromField` paths are superseded + dead (pending deletion).

### CB_TerrainModifier.h
Thin game-side forwarder to the **active** heightfield (published by the manager). `GetActive()`, `GetHeightAt`, `ApplyBrush`, `FlattenForBuilding`, `FlattenForRoad`. Safe no-ops when no CityManager is live (unit tests use their own field).

---

## Save / serialize

### CB_SaveLoadFreeform.h
Round-trips the whole free-form city in one stream: graph + zoning + buildings/services + treasury +
districts + transit + conduits (cross-references by stable slot index). `Save(...)`, `Load(...)→bool`,
`SaveToFile`/`LoadFromFile`. **`SAVE_VERSION = 4`** (v2 districts, v3 transit, v4 conduits); version-checked on load. Quick-save file: `cb_quicksave_freeform.dat`.

### CB_Serialize.h
`CB_Serialize::WriteVec<T>` / `ReadVec<T>` — POD `Zenith_Vector` ↔ `Zenith_DataStream` helpers used by every subsystem's stream I/O.

---

## Infra / UI / presentation

### CB_Zones.h
`enum CB_EZoneType { CB_ZONE_NONE, CB_ZONE_RESIDENTIAL, CB_ZONE_COMMERCIAL, CB_ZONE_INDUSTRIAL, CB_ZONE_PARK, CB_ZONE_COUNT }` — the shared land-use enum (zoning, building defs, tools, events, HUD).

### CB_SimSpeed.h
`enum CB_ESimSpeed { CB_SIM_PAUSED=0, CB_SIM_SLOW=1, CB_SIM_NORMAL=2, CB_SIM_FAST=3, CB_SIM_ULTRA=4, CB_SIM_SPEED_COUNT }` + `inline float CB_SpeedMultiplier(speed)` (0 / 0.5 / 1 / 2 / 4). The manager holds the current speed (`m_eSpeed`); growth is gated on `!= PAUSED` and the traffic dt is scaled by the multiplier.

### CB_Events.h
Plain POD event payloads dispatched via `Zenith_EventDispatcher`: `CB_OnToolSelected`, `CB_OnRoadPlaced`, `CB_OnZonePainted`, `CB_OnServicePlaced`, `CB_OnBuildingGrew`, `CB_OnBulldozed`, `CB_OnPauseToggled`, `CB_OnMilestone`, `CB_OnSaved`, `CB_OnLoaded`. Dispatch is a cheap no-op when nobody subscribes.

### CB_Telemetry.{h,cpp}
CityBuilder layer over the engine `Zenith_Telemetry` recorder. `Begin/End/NextFrame/ShouldSampleThisFrame/SampleCity/EmitSessionEnd`; `Hooks` (RAII subscribes to all `CB_On*` → forwards to the recorder); `Summarize()→Summary` (peak pop/buildings, event counts) + `LogSummary`. Writes `.ztlm` + `.json` (+ optional CSV). 10 Hz sampling at fixed 60 fps.

### CB_ToolIcons.h
Runtime-safe shared metadata (icon base-name + tooltip) for the 20 toolbar buttons. `Def { szIcon, szTooltip }`, `All(&count)`. **Order must match `CB_CityManagerComponent::ToolDescs()`** so tool indices align. Icons load at runtime from `game:UI/Icons/cb_<name>.ztxtr`.

### CB_ToolIconGen.h
Tools-build-only procedural icon drawing: pure-CPU `Canvas` primitives (`rect/disc/seg/poly/tri/star`), `DrawGlyph(idx)` (white-on-transparent + dark outline), 4× supersample + alpha-weighted `Downsample`, `RenderToolIcon(idx,outSize,&out)`. 20 hardcoded glyphs (a new tool needs a new case).

### CB_CameraController.h
Pure RTS orbit/pan/zoom math (no engine/input types), unit-testable. State `m_xTarget`(=2048,0,2048), `m_fDistance`(400), `m_fYaw`, `m_fPitch`(0.95). `Zoom`/`Rotate`/`Pan` (clamped: dist 30..2500, pitch 0.20..1.45), `ComputeCamera(&pos,&yaw,&pitch)` in `Zenith_CameraComponent` convention. Applied by `CB_CityCameraComponent`.

### CB_DayNight.h
Pure day clock + sun math. `m_fTimeOfDay`(0..1), `m_fDayLengthSecs`(120). `Advance(dt)`, `IsDay()` (0.25<t<0.75), `GetSunElevation()` (sine, 0 night → 1 noon), `GetSunDirection()` (normalized light dir). Applied by `CB_DayNightCycleComponent`.

---

## Cross-cutting invariants

- **Fine vs coarse terrain Y:** roads, lots, buildings, ghosts, and vehicles all use `GetRenderSurfaceY` (fine GPU surface), never the coarse `GetHeightAt`, or they z-fight on hills.
- **Lots never overlap roads/each other:** all placement goes through `IsLotPositionClear`; a tight road grid simply yields fewer lots (the human-session layout is spread out to compensate).
- **GPU terrain edits are race-free only:** through the `StreamInLOD` hook + eviction; never write a live resident chunk in place.
- **Headless safety:** every system computes its logic headless; only the behaviours' `Render*`/picker paths touch the GPU, guarded by `!IsHeadless()`. The terrain GPU-culling init asserts under `--headless` only when the City scene's terrain is deserialized in `_False`; the `_True` automation path guards it, which is why the headless gate runs in `_True`.
