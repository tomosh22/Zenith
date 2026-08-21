# Foundry — Game Design Document

**Status:** DRAFT for review · **Date:** 2026-08-21 · **Working title:** Foundry (rename freely; roster-style single word)
**Companion:** [Technical Design Document](TDD.md) — architecture, engine-change register, milestones.
**Placement note:** these docs live at `Docs/Foundry/` because a `Games/Foundry/` directory may not exist until it carries a valid `.zproj` (the descriptor scan hard-errors on a game folder without one). When the game is scaffolded, move this folder to `Games/Foundry/Docs/`.

---

## 1. Vision

**Full-fat Factorio, born on a touchscreen.**

Foundry is a factory-automation game: mine ore, smelt it, assemble products, research technology, and grow a production web that eventually covers the map — while the pollution it exhales provokes the native life into attacking it. It is not a "mobile-lite" adaptation. Belts, inserters, fluids, trains, logistics drones, circuit logic, and hostile biters are all present, and factories are expected to reach six figures of placed machines.

### Pillars

| # | Pillar | What it means in practice |
|---|---|---|
| P1 | **Automate everything** | Every manual action exists only to be automated away. The player's output is a *system*, not actions-per-minute. |
| P2 | **Touch-native, not ported** | Every interaction is designed finger-first: no hover, no right-click, no keyboard. If a desktop convention survives, it survives because it is also the best touch design. |
| P3 | **Gigantic scale on a phone** | 100,000+ placed buildings at a rock-steady simulation rate on a mid-range phone. Scale is the fantasy; the sim architecture is the feature. |
| P4 | **The factory must survive** | Pollution has teeth. Nests grow, evolve, and send waves. Defense is part of the factory — walls, turrets, and the ammo lines that feed them — because there is no avatar to hide behind. |

### What Foundry is not (v1.0)

No multiplayer. No mod support (content is compiled-in). No player avatar. No nuclear power, no artillery, no cliffs. See §12 Scope ladder.

---

## 2. Platform & audience

- **Primary: Android**, landscape orientation, phones ≥ 6" and tablets. Touch only — no keyboard, mouse, or gamepad assumptions anywhere in the design.
- **Secondary: Windows** (development, CI, and desktop play with mouse; the touch design maps cleanly onto mouse, never the reverse).
- **Business:** premium single purchase. No ads, no IAP, no server dependency. Fully offline.
- **Audience:** automation-game players on mobile (underserved: the genre's benchmarks are desktop) and Factorio players who want a real one on the couch/commute.
- **Session shape:** meaningful progress in 5 minutes; a sitting is typically 15–40. The design never punishes putting the phone down (§8).

---

## 3. Core loop

```
        ┌───────────────────────────────────────────────────────────┐
        │                                                           ▼
   SURVEY ──► EXTRACT ──► PROCESS ──► ASSEMBLE ──► RESEARCH ──► EXPAND
  (find ore)  (drills)   (furnaces)  (assemblers)   (labs)    (new area,
        ▲                                                      new tech)
        │                                                           │
        └────────────────────── DEFEND ◄────────────────────────────┘
                        (pollution provokes waves;
                     walls/turrets/ammo are factory too)
```

Minute-to-minute: identify a bottleneck → place machines and belts to relieve it → watch the numbers move. Hour-to-hour: unlock a new tier (oil, trains, drones, circuits) that restructures how the factory is built. Game-to-game: the win condition (§7) demands every system working at once.

---

## 4. World

- **Procedurally generated, effectively unbounded.** The world is a flat plain streamed in chunks; there is no map edge a player will ever reach. Generation is seed-deterministic: the same seed always yields the same world.
- **Resources:** iron ore, copper ore, coal, stone as mineable patches; crude oil as pumpjack seeps; water as lakes (offshore pumps). Patch richness and distance scale outward — the starting area guarantees iron/copper/coal/stone/water within reach, oil slightly out.
- **Trees** absorb pollution and are clearable (tap-harvest or deconstruction tool); they are the early game's pollution buffer and a soft wall against expansion.
- **Native life:** nests (spawners) dot the world, denser away from spawn, with **worms** (static ranged defenders) guarding mature colonies. See §5.11.
- **World-gen options** at new game: resource richness, enemy density, evolution speed, and a **Peaceful toggle** (no attacks; nests passive until provoked) — the accessibility valve and the performance floor.
- **No terrain elevation.** The ground is flat by design (readability from a top-down camera, and belt/rail logic stays 2D). Water is the only impassable terrain.

---

## 5. Systems

All numbers below are design anchors, tuned in content tables (see TDD §17); rates are quoted per second but stored per-tick (30 ticks/s).

### 5.0 The Core and the Build Reserve *(replaces the avatar's inventory and hand-crafting)*

There is no avatar, so Foundry answers "where do building materials live?" with two linked concepts:

- **The Foundry Core** is the starting structure and the heart of the base. It accepts items via belts/inserters like any container, and it is where the endgame Ark is assembled (§7).
- **The Build Reserve** is the pool of items available for *construction*. It is the sum of everything stored in the Core plus every **Depot** (a researched storage building flagged as build-accessible). Placing a building anywhere on the map instantly draws its cost from the Reserve; deconstructing returns items to it. Fiction: the Core teleports construction material — the sim never simulates per-placement delivery, which keeps building snappy and the simulation honest elsewhere.
- **The Core Fabricator** replaces Factorio's hand-crafting: a built-in, slow, one-at-a-time crafter inside the Core. Queue basic recipes (belts, drills, furnaces, inserters…) from the Reserve's raw materials. It bootstraps the first machines and stays useful as a "just need two more" convenience; it is deliberately too slow to live on.
- **Bootstrap loop:** tap-harvest surface rocks and trees for starter ore/wood → queue a burner drill + stone furnace in the Core Fabricator → place them on an ore patch → feed plates back to the Core → automate from there.

### 5.1 Mining & smelting

- **Burner drill** (coal-fired, slow) → **Electric drill** (standard). Drills output directly onto a belt or into an adjacent machine/container.
- **Stone furnace** (coal) → **Steel furnace** (coal, faster) → **Electric furnace** (no fuel logistics, module-ready in v1.x).
- Smelting chains: ore → plate (iron, copper), iron → steel (5:1, slow), stone → brick.

### 5.2 Belts, splitters, undergrounds

- Belts carry items on **two independent lanes**. Three tiers: **Mk1 1.875 tiles/s (15 items/s)**, **Mk2 3.75 t/s (30/s)**, **Mk3 5.625 t/s (45/s)** — Factorio-parity speeds chosen because they are integer-exact in the sim (TDD §4).
- **Splitters** split/merge with strict round-robin fairness, with input/output priority and a single item filter (per side) as researched upgrades.
- **Underground belts** jump 4/6/8 tiles by tier. Sideloading works and is lane-precise.
- Belt behavior is deterministic and lossless: items never vanish, overflow backs up, gaps compress.

### 5.3 Inserters

- Three tiers: **Inserter** (~1 swing/s), **Long inserter** (2-tile reach), **Fast inserter** (~2.3 swings/s). All electric (the burner tier is cut; the Core Fabricator covers bootstrap).
- Inserters pick from belts (lane-aware), machines, and containers, and place into the same. Filter inserter is a researched upgrade of Fast.

### 5.4 Assemblers & recipes

- **Assembler Mk1/Mk2/Mk3** (speed 0.5 / 0.75 / 1.25; Mk2+ accept fluid inputs). Recipe chosen per-machine from a searchable, category-tabbed picker (§6.4).
- ~90 items, ~110 recipes at v1.0. Representative chains: gears/cables → electronic circuits → advanced circuits (needs plastic) → processing units (needs sulfuric acid); engine units; batteries; ammo lines; drone frames.

### 5.5 Power

- Arc: **burner boot** (drills/furnaces on coal) → **steam** (offshore pump → boiler → steam engine) → **solar + accumulator** fields.
- **Power poles** (small/medium/large) define coverage circles and wire reach; machines inside coverage draw from that **electric network**. Multiple isolated networks are legal and visible.
- Brownout model: when demand exceeds supply, every consumer on the network runs at the same satisfaction fraction — machines visibly slow rather than hard-stop. The **power graph** (per-network production/consumption history) is one tap from any pole.

### 5.6 Research

- **Labs** consume **science packs**: **Automation (red), Logistics (green), Military (black), Chemical (blue)**, and endgame **Ark Science (white)** — 5 tiers, ~60 technologies.
- Research unlocks buildings, recipes, and capability upgrades (splitter filters, blueprint tools, assault drones, zoom-tier overlays…). One active research at a time; a queue (depth 5) is an early QoL unlock.

### 5.7 Fluids

- **Pipes** (and underground pipes) form **fluid networks**: each network holds one fluid type and equalizes instantly (per-network volume model — TDD §9). Mixing is prevented at connect time with a clear error.
- Producers/consumers: **offshore pump** (water), **pumpjack** (crude), **boiler** (water+fuel→steam), **refinery** (crude→petroleum/heavy/light), **chemical plant** (plastic, sulfur, acid, lubricant), **pump** (network isolation + throughput boost + tank loading).
- **Storage tank** buffers; **fluid wagon** moves fluids by rail. Network extent is capped (~320 tiles) — beyond that, use trains or parallel networks (mirrors Factorio 2.0's model, and keeps the sim honest).

### 5.8 Trains

- **Rails** are placed from a discrete piece catalog (straight, 45° diagonal, curve) via drag — the router auto-chooses pieces along the drag path. Junctions form wherever rails meet.
- **Stations** sit beside rails and are **icon-named** (choose 1–3 icons, e.g. ⛏🔩 = "iron pickup") — no text entry anywhere in the train system (mobile: no IME dependency; see TDD register #14).
- **Signals**: block signals and chain signals, placed on rail edges. A **block-coloring overlay** while in signal mode shows exactly what each signal protects — the teachable moment Factorio hides in tooltips.
- **Trains** = locomotive + cargo/fluid wagons. Schedules are ordered station lists with wait conditions (full / empty / item count / seconds). Automatic pathing with block/chain reservation; deadlocks are the player's puzzle, with a "train stuck" alert (§6.6).
- v1.0 keeps trains single-headed (locomotive forward; add a rear locomotive for reversibility).

### 5.9 Logistics drones

- **Drone Port** projects a coverage field; overlapping fields merge into a **logistics network**.
- Container roles within coverage: **Provider** (offer), **Requester** (demand, with per-item set-points), **Storage** (overflow), plus the Core/Depots which are always providers.
- **Carrier drones** fly point-to-point (straight lines, no collision), respecting a per-network throughput budget; they recharge at ports (simplified: nearest port with a free slot, event-scheduled).
- Drones deliberately arrive late-game as the anti-spaghetti tool; belts remain the throughput king (drone throughput is capped well below a Mk3 belt).

### 5.10 Circuit network

- **Red and green wires** connect containers, belts (read contents/hold), inserters (enable/disable, set filter), pumps, power switches, lamps, and machines (read/enable).
- **Combinators:** arithmetic, decider, constant. Signals are (type, integer) pairs; networks sum signals; evaluation has a fixed 1-tick propagation delay (deterministic, TDD §12).
- Touch-first wiring: enter **wiring mode**, tap source → tap target; wires render as catenaries color-coded by network; tapping any wired entity shows its live signal table.
- Circuits are optional depth: nothing in the main progression *requires* a combinator, but stock-based enable conditions (e.g. "make ammo only under 500") are taught by one early objective.

### 5.11 Biters, pollution & defense

- **Pollution** is exhaled by drills, furnaces, boilers, assemblers (scaled by activity), spreads chunk-to-chunk as a cloud, is absorbed by trees and nests, and is visible as a Far-zoom overlay (§6.2) — the factory's footprint is literally on the map.
- **Nests** absorb pollution and convert it into **attack waves** aimed at the polluters. Colonies expand slowly into unpolluted land over time.
- **Species:** biters (melee) and spitters (ranged), each in small/medium/big; **worms** defend nests. **Evolution** (0→1, driven by time + total pollution + nests destroyed) shifts wave composition toward bigger units.
- **Defense is structural** (no avatar, no run-and-gun): **walls** and **gates**, **gun turrets** (consume magazine ammo — an ammo *production line* is the real defense tech), **laser turrets** (consume power — trade logistics for electric load), **radar** (map visibility + early warning).
- **Offense — clearing land to expand** (the no-avatar answer, explicit):
  1. **Turret creep** (always available): build turrets/walls forward under fire; power/ammo them; grind the nest down. Deliberate, expensive, works from minute one of military tech.
  2. **Assault drones** (researched, black science): build strike drones into a **Drone Bay**, then **tap a target zone**; the squadron flies out, attacks everything in the zone until destroyed or done, and does not return. Ammo-free but consumes the drones — offense priced in production, like everything else.
  3. **Artillery** is v1.x (§12).
- **Threat pacing:** a grace period (no waves) for the first ~20–30 minutes scaled by pollution output; wave size/composition follows absorbed pollution, capped by the active-unit budget (TDD §13) so the sim never melts under its own siege.

### 5.12 Blueprints, copy/paste, undo, deconstruction

- **Undo/redo** (50 steps): every placement, deletion, and configuration change reverses honestly (resources refunded/re-drawn from the Reserve). Undo is a first-class HUD button — mis-taps are a fact of touch life.
- **Copy/paste:** marquee-select a region → stamp it elsewhere (with rotation); settings-only paste ("paintbrush") copies a machine's recipe/filters/conditions onto same-type targets by drag.
- **Blueprints:** save a selection to the **Blueprint Library** with an icon name; stamp with rotation/flip. Missing items place **ghosts** (translucent markers) that fill automatically when the Reserve can afford them (nearby-first), so stamping ahead of production is planning, not an error.
- **Deconstruction planner:** marquee-mark for removal (with filters: only trees, only belts…); marked entities dismantle over a short timer and refund to the Reserve.

---

## 6. Touch UX — the flagship chapter

Design law: **one thumb navigates, two fingers command, nothing requires precision under 57 logical px** (the engine's established minimum touch target). No interaction depends on hover, right-click, double-click-and-hold, or a keyboard.

### 6.1 Control vocabulary

| Gesture | Meaning |
|---|---|
| 1-finger drag (world) | Pan camera |
| Pinch | Zoom (continuous, through three view tiers §6.2) |
| Tap (entity) | Select → open Inspector (§6.4) |
| Tap (empty ground, build mode) | Place at ghost position |
| Drag (build mode, linear buildings) | Lay a belt/pipe/wall/rail run with auto-orient + auto-bend |
| Long-press (entity) | Radial quick menu: rotate · copy settings · deconstruct · disable |
| Long-press (empty) | Radial: paste last · blueprint here · survey info |
| Double-tap | Quick zoom toggle between the two most recent zoom levels |
| Two-finger tap | Undo (mirrors the HUD button; muscle-memory accelerator) |

### 6.2 Camera & the three view tiers

Continuous pinch-zoom slides between three *representations* (not just scales — see TDD §15):

1. **Workshop** (≲40 tiles across): full 3D machines, items visible on belts, inserter arms animating. The "watch it work" view.
2. **Logistics** (~40–150 tiles): buildings simplified, belts render as flow-colored ribbons (contents as tint density), trains/drones visible. The planning view — most building happens here.
3. **Map** (≳150 tiles): chunk-resolution tiles, resource patches, pollution overlay, radar coverage, train lines, alert markers. This *is* the map screen — there is no separate map UI; pinch out far enough and you are looking at it.

The camera is a fixed steep-pitch top-down view (no free rotation; a 4×90° orientation snap is a settings option, default locked — rotation destroys grid muscle-memory on touch).

### 6.3 Build flow

- The **palette** is a bottom-edge drawer: category tabs (logistics / production / power / fluids / rail / military / circuits) → item grid. Tap an item to enter **build mode**; the drawer collapses to a slim strip.
- In build mode, a **ghost** of the building tracks the finger with a **placement reticle offset ~1.5 tiles above the fingertip** (the finger never hides the target cell). Grid snap is magnetic; validity is live (green/red with a reason line: "blocked", "needs ore patch", "outside Reserve range" never exists — Reserve is global).
- **Release ≠ commit.** Drag positions the ghost; lifting the finger commits *only* in tap-to-place; for drag-runs (belts/pipes/walls/rails) the run commits on release with an **Undo toast** for 3 s. Rotation is a fixed on-screen button (and two-finger-tap-hold rotates in place).
- **Mass work is drag-shaped:** drag-to-place runs, drag deconstruction marquee, drag paintbrush for settings. Nothing at scale is tap-tap-tap.

### 6.4 Selection & inspection (the no-hover answer)

- Tap any entity → **Inspector panel** docks to the screen's short edge (right on landscape): identity, recipe/filters (tap to edit via the searchable recipe picker), contents, throughput sparkline, power satisfaction, and context actions. The factory stays visible and interactive beside it.
- **Hold-to-peek:** press-and-hold any entity ≥350 ms without releasing → transient info card (contents/status) at the reticle; release without lifting into a swipe = no selection change. This replaces hover-tooltips.
- The Inspector is also where alerts explain themselves ("no power: network 2 at 40%").

### 6.5 Fat-finger law

- Every interactive on-screen element ≥ **57 logical px** (engine's density-scaled floor); world-entity tap targets get slop proportional to zoom.
- Destructive actions (deconstruct area, delete blueprint) are drag-confirm or two-step; **never** a bare tap.
- **Undo is the safety net, not confirmation dialogs** — the game prefers "act, then undo" over "are you sure?".

### 6.6 Alerts & attention (god-view + biters makes this critical)

- Alert stack top-left: attack in progress, turret out of ammo, machine starved (aggregated), train stuck, power brownout. **Tap an alert → camera jumps there**; back-tap returns.
- Attacks additionally paint an edge-of-screen directional glow. Haptic pulse on first contact (once engine support lands — TDD register #11).
- Alert aggregation is aggressive (one "12 furnaces starved — iron" not twelve toasts), with per-category mute.

### 6.7 What was deliberately not done

- No virtual joystick anywhere (nothing to steer).
- No gesture with more than two fingers; no shake; no tilt.
- No text input in the entire v1.0 game (icon-naming everywhere) — see TDD register #14.

---

## 7. Progression & pacing

### First 15 minutes (scripted objectives, not a forced tutorial)

1. Harvest rocks/trees (tap) → 2. Fabricate + place a burner drill on iron → 3. Furnace the ore, feed plates to the Core → 4. First belt run → 5. Electric power (offshore pump→boiler→engine) → 6. Automate red science → 7. First research completes → the objective pane hands the player to the tech tree. Each step is one new interaction, taught by doing.

### Arc to the win

Red science (automation basics) → green (logistics: trains teased, splitter tools) → black (military: turrets, walls, assault drones) → oil + blue (fluids, plastics, advanced circuits, drones, circuits mastery) → **Ark Science** → assemble the **Ark** at the Core: a multi-stage megaproject (structure → systems → launch) each stage consuming bulk goods that stress every system built so far — the "launch the rocket" equivalent. Launching the Ark wins the game; the save continues (megabase mode) with a post-win stat card.

Expected first-win time: 25–40 hours. Evolution and pollution keep mid-game pressure honest; the grace period keeps the first hour pure building.

---

## 8. Mobile session design

- **Autosave:** on every app pause/background (synchronous, guaranteed — the process may be killed after backgrounding) and every 2 minutes during play (asynchronous, hitch-free). See TDD §16.
- **Backgrounded = paused.** The sim does not run while the app is away, and there is **no offline progress** — stated in-game plainly ("The factory sleeps when you do"). Rationale: with biters, offline simulation is hostile (come back to ash); with offline *production* only, it's an idle game. Foundry is neither.
- **Resume-exact:** reopening lands exactly where the player left, same camera, with a one-line status strip (uptime, research %, alerts pending).
- **Battery stance:** render at 60 fps by default with a **30 fps battery option**; simulation is 30 UPS always. Thermal degradation ladder (resolution/effects, never sim rate) — TDD §1/§15.
- Sessions are interruption-proof by construction: no unpausable moments, no timed missions, nothing lost by a phone call.

---

## 9. Art direction

- **Low-poly kit-of-parts:** machines are chunky, silhouette-first, assembled from a shared shape vocabulary (drum, hopper, arm, stack). Readability beats fidelity at every zoom.
- **Silhouette + color = identity.** Each machine class owns a silhouette; tiers within a class are colorways (Mk1 mustard / Mk2 crimson / Mk3 cobalt — matching belt-tier colors). Item icons use a **colorblind-safe palette** with shape redundancy (no two common items differ by hue alone).
- **State at a glance:** working machines animate (per-instance animation frames); starved/blocked/unpowered machines show a desaturated tint + status glyph (the per-instance tint channel — TDD §15).
- **The ground is quiet:** desaturated terrain, subtle grid in build mode only, so the factory carries the color.
- Belt items render as the item's icon-mesh (a flat-shaded 3D nugget) at Workshop zoom; as lane tint at Logistics zoom.
- Pollution renders as an honest desaturating haze on the Map tier — the factory visibly stains the world.

---

## 10. Audio direction

*(Engine dependency: Zenith currently has **no audio system** — this is register item #1 in the TDD. Game code emits through the existing `Zenith_AudioBus::EmitSound` seam from day one, so all content below is authorable before the backend exists and simply becomes audible when it lands.)*

- **The factory is the soundtrack:** layered machine hum built from what's actually on screen (furnace roar, assembler clatter, belt whisper), density-mixed so a megabase is a wall of industry, not 10,000 one-shots. Sparse ambient music under it, ducking under alerts.
- **Positional one-shots:** placement thunk, inserter tick, train horn, turret fire — attenuated by camera distance/zoom tier.
- **Threat language:** distinct stingers for wave-spawned / first-contact / breach; the player should recognize danger with the screen off.
- Mix targets mobile speakers first (mid-forward, limited dynamics), with a headphone-wide option.

---

## 11. UI screen inventory

| Screen | Content | Notes |
|---|---|---|
| HUD | Alert stack, research pill, undo/redo, palette drawer, tool strip (select/deconstruct/copy/blueprint/wire) | Chrome minimal; factory is the screen |
| Build palette | Category tabs × item grid, search | Bottom drawer, one-thumb reachable |
| Inspector | Selected entity: recipe/filters/contents/throughput/power/actions | Docked panel, world stays live |
| Recipe picker | Category tabs, search-by-icon, ingredient/product preview | Used by assemblers, fabricator, requesters |
| Tech tree | **Vertical tier list** (5 science tiers as sections, techs as cards with prereq lines) — not a free-pan node graph | Scrollable list beats graph navigation on a phone |
| Train schedule | Station-icon list + wait-condition rows; drag to reorder | Icon-only naming |
| Logistics panel | Per-network: drones, ports, request/provide table | |
| Circuit panel | Per-network signal table; combinator editor (three-row form) | |
| Power graph | Per-network production/consumption history | One tap from any pole |
| Blueprint library | Grid of stamps, icon-rename, delete (two-step) | |
| Map overlays | Pollution / power coverage / logistics coverage / radar — toggle chips on Map tier | |
| Save slots | 3 manual + autosave rotation; damaged-slot state distinct from empty | Pattern per Zenithmon save UX |
| Settings | Battery 30/60, camera snap, colorblind palettes, alert mutes, peaceful (world-gen only) | |
| Objectives | First-15-minutes script + milestone checklist | Dismissible forever |

---

## 12. Scope ladder

**v1.0 (this document):** everything in §5 — core automation, fluids, trains (block + chain signals), logistics drones, circuits, full biters, blueprints/undo/deconstruction, Ark win.

**Cut from v1.0 (explicitly, with reasons):**
- **Multiplayer** — the deterministic sim keeps the door open (TDD §3); shipping it is a separate project.
- **Mods / data files** — content is compiled tables (TDD §17); modding is a distribution and API commitment v1 cannot carry.
- **Nuclear power** — the solar/steam arc carries v1's power story; nuclear is v1.x's headline feature.
- **Artillery** — assault drones + turret creep cover offense; artillery is v1.x pacing relief for megabase players.
- **Cliffs/elevation** — flat world is a readability and sim-simplicity choice, not a deferral.
- **Avatar** — permanent design identity, not a cut.
- **Portrait orientation** — landscape-only v1 (engine games precedent; portrait is a full re-layout, v1.x candidate for tablets).

**v1.x candidates in rough order:** nuclear, artillery + spidertron-analog siege drone, rich modules/beacons, portrait/tablet layouts, cloud save, MP.

---

## 13. Player-facing performance promises

These are the promises the engineering budget (TDD §1) exists to keep:

- **Scale:** 100,000+ placed buildings and 250,000+ items in flight, simulated every tick — no "sleeping far base" cheat; the whole factory always runs.
- **Steady:** 30 simulation updates/s, always, on a mid-range 2024-era phone (Snapdragon 7-series class, 8 GB); 60 fps rendering on the same hardware at default settings.
- **Instant resume**, autosaves you never feel (≤100 ms), and a pause-save that survives the OS killing the app.
- **Deterministic:** same seed + same inputs = same factory, bit-for-bit, on every device. Saves never corrupt from replaying differently.
- Battery: a 30-minute session at battery settings targets ≤ 12% drain on the reference device (measured from M2 onward).
