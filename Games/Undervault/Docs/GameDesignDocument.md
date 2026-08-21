# Undervault — Game Design Document

**Working title:** Undervault
**Status:** v0.1 — pre-production reference GDD; each system becomes binding as
the roadmap in the [TDD](TechnicalDesignDocument.md) §20 implements it
**Author:** Design (Claude), from the user-approved plan, 2026-08-21
**Companion:** [TechnicalDesignDocument.md](TechnicalDesignDocument.md) —
architecture, performance budgets, and the **engine gap analysis** (what Zenith
must grow before this game can ship)
**Scope authority:** §19 (scope-tier matrix) is the binding in/out gate. The
body of this document details the MVP and v1.0 columns; nothing outside the
matrix is in scope, and this document cannot re-add anything the matrix cuts.
**Update policy:** revised at each roadmap phase gate. Once implementation
starts, tuning values live in compiled `constexpr` tables
(`Source/Sim/UV_ElementTable`, `Source/Data/UV_BuildingDefs`) and **the code
tables win**; this doc gets a fix-up in the same commit (Zenithmon precedent).
All numeric values in this document are provisional tuning anchors, not
promises.
**Placement note:** these docs live at `Docs/Undervault/` for now — a
`Games/Undervault/` folder may not exist until it carries a valid `.zproj`
(the descriptor scan errors on a game folder without one, and `regen.ps1`
then regenerates nothing). When `zenith new Undervault` scaffolds the game,
move this folder to `Games/Undervault/Docs/`.

---

## 1. Vision & pillars

### 1.1 Elevator pitch

**Undervault** is a survival colony simulation played entirely by touch: a
living, breathing terrarium in your pocket. Deep inside a sealed vault-world,
you are the Overseer — waking Vaulters from stasis, carving out living space,
and building the machines that keep air breathable, water flowing, and food on
the table. Every tile of rock, drop of water, and puff of carbon dioxide is
simulated; heat moves, gases mix, liquids pool, and every mistake is legible in
the world itself. Sessions are 5–20 minutes; the colony is always exactly where
you left it. Mobile-first (Android), premium, no ads.

### 1.2 Pillars

Every feature is tested against these three principles. If it serves none of
them, it is cut.

1. **Touch-native control.** Not a desktop UI shrunk down. One finger pans,
   two fingers zoom, a tap inspects, a drag paints work orders — the whole
   game is playable with a thumb and forefinger, every target is comfortably
   tappable (≥57 logical px, the engine-enforced floor), and no feature ever
   requires a keyboard (there is **no text entry anywhere** in the game — see
   §15).
2. **Legible simulation.** The player can always answer "why did that
   happen?" by looking. Gases shade the air, liquids have visible depth, heat
   has an overlay, and every Vaulter will tell you exactly what errand it
   chose and why. Simulation depth is worthless on a phone screen unless it
   is readable at arm's length — readability beats realism wherever the two
   fight.
3. **Colony as character.** The Vaulters — their names, traits, close calls,
   and the base's slowly accreting architecture — are the story. There is no
   scripted plot; there is the log of the time the water pump froze and
   Maru dug a channel to the hot springs with 4% oxygen left. Failure is
   survivable, dramatic, and always the player's own story.

### 1.3 Fiction in one paragraph

Generations ago, the Wardens sealed humanity's remnant inside the Undervault —
a world-sized cavern system bounded by indestructible **Wardstone** — and set
its people into stasis until the vault could sustain them. Something woke the
Overseer (the player) early. The stasis systems are failing, the vault's old
infrastructure is rubble, and the only way out is through: wake Vaulters,
rebuild, and make the deep places live again. The fiction justifies every
mechanical boundary: the map edge (Wardstone), the population source (the
Stasis Vault), the absence of a sky, weather, and rockets, and why nobody can
simply leave.

---

## 2. Reference frame & differentiation

**Genre:** systems-driven colony sim in the *Oxygen Not Included* mold — a 2D
side-view tile world where matter, heat, and gases are first-class simulated
citizens, colonists are indirectly controlled through work orders and
priorities, and the core loop is *dig → build → stabilise → expand → new
problem*.

**Original IP.** Mechanics of the genre are the reference; every name, place,
creature, artwork, and text string is original. No Klei names, assets, or
trade dress anywhere, including placeholders.

**What we deliberately cut (all tiers):** rockets/space/surface, radiation,
multi-world play, mod support, multiplayer.

**What we add over the genre baseline:**

- **Touch-native designation flow** — painting dig orders with a finger is
  the input the genre always wanted; the whole tool UX is designed around it
  (§11).
- **Mobile session shape** — a 5-minute cycle, autosave that never loses more
  than a minute, resume-exactly-where-you-were, and pacing tuned so that a
  commute-length session always completes something (§16).
- **Gentler failure curve** — the genre's death spirals stay, but the ramp is
  slower and every spiral has a legible escape hatch (§10).

**Platforms:** Android (ship target); Windows (development, testing, CI —
the same build with mouse-as-touch). iOS is out of scope (the engine has no
iOS platform layer; see TDD §21 appendix).

---

## 3. The world

### 3.1 Shape

The world is a sealed 2D grid of cells, side-view, gravity pointing down.

| Map | Cells | Purpose | Tier |
|---|---|---|---|
| Small | 96 × 160 = 15,360 | Tutorial / short runs | MVP |
| Standard | 128 × 192 = 24,576 | The default colony | MVP |
| Deep | 192 × 288 = 55,296 | Tablets / long runs | v1 (Tier-A devices) |

The border is **Wardstone**: indestructible, perfectly insulating, and the
fiction's hard wall. There is no "outside".

### 3.2 Biomes

World generation lays biome bands around a guaranteed temperate start chamber
(see TDD §10 for generation invariants). Each biome is an element palette, a
temperature band, and a promise about what the player will find there.

| Biome | Solids | Fluids/gases | Temp band | Offers | Threatens |
|---|---|---|---|---|---|
| **Hearth** (start, centre) | Sandstone, Dirt, Coal, Copper Ore | Water pockets, Oxygen pockets, CO₂ | 15–30 °C | Safe expansion, algae, first water | Nothing much — that's the point |
| **Mirefen** (swamp) | Clay, Mire (biomass), Iron Ore | Polluted Water, Polluted Oxygen (v1) | 25–35 °C | Farming inputs, iron, water volume | Stink, disease (post), morale |
| **Frostreach** (cold) | Ice, Snow, Granite | Frozen pockets, thin O₂ | −30–0 °C | Cooling, fresh water (melt), preservation | Freezing crops/pipes, hypothermia (v1) |
| **The Forge** (hot) | Obsidian, Granite, Iron/Copper veins | Magma pockets (v1), Hydrogen | 60–200+ °C | Metal riches, geothermal power (v1) | Heat creep, burns, melted infrastructure |
| **Nullrock seams** | Nullrock (near-perfect insulator) | — | — | Natural thermal walls; the map's "room dividers" | Blocks heat you *wanted* to move |

### 3.3 Fog of the unexplored

Undug rock more than 2 cells from any revealed cell renders dark; overlays do
not read through it. Revealing is permanent per save. The fog is a pacing
tool: biome edges glint through (ore sparkle, temperature shimmer at Forge
borders) to bait expansion.

---

## 4. Elements & matter

### 4.1 The one-element rule

Every cell holds exactly **one element** with a **mass** (kg) and a
**temperature**. Solids fill the cell; liquids pool and stack by mass; gases
spread to fill space. Two different substances never share a cell — displacing,
sinking, and rising are how the world stays legible. (This is also the
load-bearing simulation rule; TDD §3–4.)

### 4.2 Element roster

~28 elements at v1. Real-world chemistry keeps real names (oxygen is oxygen);
invented minerals carry the vault's own names. States: ■ solid / ● liquid /
○ gas.

| Element | State | Tier | Notes (provisional tuning anchors) |
|---|---|---|---|
| Oxygen | ○ | MVP | Breathable ≥ 150 g/cell; rises above CO₂ |
| Carbon Dioxide | ○ | MVP | Vaulter exhaust (2 g/s); sinks; pits form in low points |
| Hydrogen | ○ | MVP | Lightest — collects at ceilings; future fuel (v1) |
| Polluted Oxygen | ○ | v1 | Off-gasses from Polluted Water and Mire; morale hit; deodorise |
| Steam | ○ | v1 | Water above 100 °C; scalds; geothermal working fluid |
| Vacuum | — | MVP | The absence entry; perfect insulator |
| Water | ● | MVP | The colony's blood: drinking, farming, machines |
| Polluted Water | ● | MVP | Abundant in Mirefen; farm-usable, off-gasses at surface (v1) |
| Magma | ● | v1 | The Forge's floor; ultimate hazard + power source |
| Sandstone / Granite / Obsidian | ■ | MVP | Dig-and-build stock; hardness tiers (dig time) |
| Dirt / Clay | ■ | MVP | Farm inputs; low-grade build material |
| Coal | ■ | MVP | Fuel (v1 Bio Boiler); MVP: dig-and-store |
| Copper Ore / Iron Ore | ■ | MVP | Machines and wire require refined-free ore (MVP builds straight from ore) |
| Ice / Snow | ■ | MVP | Frostreach stock; melts to Water |
| Algae | ■ | MVP | Algae Pod fuel → oxygen |
| Mire | ■ | MVP | Swamp biomass; compost input; off-gasses (v1) |
| Sand | ■ | MVP | Filtration medium (v1 Deodorizer) |
| Nullrock | ■ | MVP | Near-zero conductivity; undiggable at MVP (diggable v1, never buildable) |
| Wardstone | ■ | MVP | World border. Indestructible, inert, absolute |

### 4.3 Phase change as spectacle

Phase transitions are showcase moments, tuned to be *witnessed*: ice melting
into a Frostreach dig, a flooded corridor flashing to steam at the Forge edge,
CO₂ pooling visibly in the bottom of a shaft. Transition points carry a ±3 K
hysteresis so cells don't flicker at the boundary (TDD §4). Latent heat is
simplified at MVP (instant transition, energy ledgered from v1).

---

## 5. Vaulters

### 5.1 Overview

Vaulters are the colonists: woken from the **Stasis Vault**, never directly
controlled, always one tap away from explaining themselves. Roster arc: wake
**3** at new game, grow to a soft cap of **12** (UI comfort), hard cap **16**.
Every 3 cycles the Stasis Vault offers a **choice of three** candidates (with
visible traits) or a care package of supplies — the genre's population valve,
reskinned to the fiction.

### 5.2 Needs

| Need | Fed by | Starved by | Failure state |
|---|---|---|---|
| **Breath** | Oxygen ≥150 g in the occupied cell (100 g/s intake) | CO₂/vacuum/thin air | Suffocation meter (60 s) → death |
| **Calories** | Meals (1,000 kcal per cycle burn) | No food | Starvation over ~3 cycles → death |
| **Bladder** | Latrine / Toilet | No facilities | Accident → mess + morale hit to witnesses |
| **Stamina** | Sleep in a Cot during Sleep blocks | Missed sleep, interrupted nights | Exhaustion: −50% work speed, naps on the floor |
| **Morale** | Needs met, meals at a table, room bonuses (§8.6), variety | Accidents, deaths witnessed, floor-sleeping, raw food | See §5.5 — slowdown, then refusal. Never rampage (MVP/v1) |

### 5.3 Traits

Each Vaulter wakes with one positive and one negative trait from a pool of ~12
(MVP). Traits are legible one-liners, not stat soup: *Deep Sleeper* (never
woken by noise), *Iron Gut* (raw food, no morale hit), *Early Riser* (+work
speed first 2 blocks), *Claustrophobic* (morale drain in 1-wide corridors),
*Torchbearer* (small light radius — helps early dark digs), *Butterfingers*
(drops carried loads on scare), *Bottomless* (1.5× calories), *Nightowl*
(inverted schedule preference).

### 5.4 Skills

Seven chore families double as skills: **Dig, Build, Farm, Cook, Research,
Operate, Tidy** (sweep/deliver/store). Skill grows by doing (no XP screen):
each level −10% errand time in that family, visible as a small badge on the
Vaulter card. v1 adds **Care** (medic) and per-skill morale expectations
(a master builder wants a nicer bunk).

### 5.5 Morale model (gentle by design)

Morale is 0–100. Effects are graduated and always recoverable:

- **≥70** — +10% work speed, occasional whistling.
- **30–70** — normal.
- **<30** — −25% work speed; "Downcast" thought bubble names the top cause.
- **<15** — refuses new errands during Work blocks; retreats to cot/mess
  area; recovers on any Break/Sleep block or a met need.

No destructive tantrums at MVP/v1 (post-tier revisits). Death of a colony
member hits every witness −20 for a cycle and is memorialised in the Vault Log.

### 5.6 Schedules

The cycle (§16.1) divides into **12 blocks of 25 s**. Each Vaulter has a
paintable schedule row of block types: **Work / Break / Sleep** (MVP; v1 adds
*Bathtime* priority block). Default: 7 Work, 2 Break, 3 Sleep. Group presets
(v1) let the player stagger shifts so the base never sleeps all at once.

---

## 6. Errands & priorities

### 6.1 The contract

The player **never** moves a Vaulter. The player paints **designations**
(dig, sweep, priority, cancel) and places **buildings**; the sim turns those
into **errands**; Vaulters claim errands one at a time by a scoring rule the
player can predict and inspect.

### 6.2 Scoring (predictable, in this order)

1. **Personal chore-family priority** — each Vaulter's per-family setting:
   ▲ preferred / ● normal / ▼ reluctant / ✕ never (the Priorities screen, a
   7×N grid of large toggle cells).
2. **Player priority** — every designation and building carries a priority
   **1–9** (default 5), set by the paint tool or the inspector.
3. **Distance** — nearest reachable first (region check first, so
   unreachable work is never chosen and is flagged in the alerts strip).

Ties break deterministically (lowest cell index). One errand, one owner:
claims are exclusive, released on completion, death, interruption, or
unreachability (the TDD §6 claim table).

### 6.3 Designation tools

| Tool | Gesture | Effect |
|---|---|---|
| **Dig** | drag-paint / drag-rect | Marks solid cells; yields 50% mass as debris item |
| **Cancel** | drag-paint | Clears designations/blueprints under the brush |
| **Deconstruct** | tap building / drag | Refunds 100% materials (MVP simplicity) |
| **Priority** | drag-paint with 1–9 selector | Re-stamps priority on everything under the brush |
| **Sweep** | drag-paint | Marks debris for storage delivery |

All tools follow the §11.4 tool-mode flow (explicit enter/exit, two-finger
navigation always live, Apply/Undo bar).

---

## 7. Building catalog

Single authority table in code (`UV_BuildingDefs`, one `constexpr` table —
CityBuilder precedent); this section is its design mirror. Materials: most
buildings are built **from a chosen solid** (mass drawn from storage via
deliver errands); at MVP material choice affects only cost and look, from v1
thermal properties matter (Nullrock-insulated builds).

### 7.1 MVP catalog (~22 buildings)

| Building | Domain | Size (w×h) | Power | Heat | Notes |
|---|---|---|---|---|---|
| Ladder | Logistics | 1×1 | — | — | The vertical spine; climbable |
| Standard Tile | Structure | 1×1 | — | — | Walkable floor/wall; blocks fluids |
| Airlock Door | Structure | 1×2 | — | — | Manual; Vaulter-passable, blocks fluids when shut |
| Storage Bin | Storage | 1×1 | — | — | 500 kg, filterable by element |
| Crank Wheel | Power | 2×2 | +400 W while cranked | + | Operate errand; the bootstrap generator |
| Battery Cell | Power | 1×2 | stores 20 kJ | + | Trickle self-drain |
| Wire | Power | 1×1 | carries 1 kW | — | Overloads above rating (§8.4) |
| Lamp | Comfort | 1×1 | −25 W | + | Light radius; morale + work speed in light (v1 numeric) |
| Research Bench | Research | 2×2 | −120 W | + | Study errand → research points |
| Algae Pod | Life support | 1×2 | — | — | 500 g algae/cycle → 40 g/s O₂ |
| Gloomsprout Bed | Food | 1×1 | — | — | Grows Gloomsprout (raw 400 kcal); needs Dirt + 20 kg water/cycle |
| Mush Press | Food | 2×2 | −240 W | + | Polluted-water mush → Pressmush bar 800 kcal (morale −) |
| Mess Table | Comfort | 2×1 | — | — | Eating at a table = morale +; seats 2 |
| Ration Box | Food | 1×1 | — | — | Slows spoilage 4× (unpowered) |
| Cot | Comfort | 2×1 | — | — | Sleep target; floor-sleeping is the morale stick |
| Latrine | Hygiene | 1×2 | — | — | Outhouse; fills, needs empty-errand (Dirt out) |
| Wash Basin | Hygiene | 1×2 | — | — | 5 kg water/use; post-latrine wash stops (v1) germ vector |
| Water Pump | Plumbing | 2×2 | −240 W | + | 500 g/s from its cell into pipes |
| Liquid Pipe | Plumbing | 1×1 | — | — | Carries 10 kg packets (§8.5) |
| Liquid Vent | Plumbing | 1×1 | — | — | Emits pipe contents into the world |
| Compost Heap | Farm | 2×1 | — | — | Mire/spoiled food → Dirt |
| Stasis Vault | Colony | 4×3 | — | — | Pre-built, indestructible; the population valve (§5.1) |

### 7.2 v1.0 additions (headline rows)

Electrolyzer (water → O₂ + hydrogen, the mid-game oxygen answer) · Gas Pump /
Gas Pipe / Gas Vent · Air Deodorizer (sand + polluted O₂ → clean) · Insulated
Tile / Insulated Pipe (Nullrock) · Bio Boiler (coal/mire 600 W) · Heavy Wire
(20 kW) + Transformer · Plumbed Toilet + Shower + Sink loop · Fridge (powered,
cooled preservation) · Grill (Emberfruit + recipes, morale food) · Fire Pole
(fast descent) · Thermo Sensor + Valve (automation-lite: one sensor, one
actuator, no logic gates) · Geothermal Tap (tamed Forge heat → Steam → power) ·
Med Cot + Care skill · Decor furnishings set (banners, planters).

### 7.3 Post-launch candidates

Germs & illness system · Vaultlife (creatures: Burrowers, Drifters) + ranching ·
full automation logic (gates, ribbon) · conveyor rails · large-map tablet mode ·
Vault Archive meta-unlocks.

---

## 8. Deep systems

Player-facing behaviour only; algorithms and budgets live in TDD §4/§8.

### 8.1 Gases & pressure

Gases spread from high to low mass, lighter gases rise above heavier
(hydrogen ceiling pockets, CO₂ pit floors). Breathability is a per-cell
threshold shown by the Oxygen overlay. Over-pressurised rooms (≥ ~2.5 kg/cell)
stop O₂ producers — pressure management is a real layout problem, readable as
colour saturation.

### 8.2 Liquids

Liquids fall, pool, and level out; a full cell can hold up to ~1,000 kg with
mild over-compression at depth. Water finds the low point of your base — the
classic self-inflicted flood is fully supported and fully legible.

### 8.3 Heat & insulation

Every element conducts; machines and Vaulters emit; nothing magically deletes
heat. Nullrock seams and (v1) insulated builds are the thermal walls; Frostreach
is the natural heat sink; the Forge creeps outward if you tunnel carelessly.
MVP keeps machine heat gentle; v1's Insulation + Geothermal research turns
heat into the mid-game boss.

### 8.4 Power

Generators feed circuits; batteries buffer; consumers draw. A circuit's load
above its weakest wire's rating for **6 consecutive sim ticks** burns a random
wire segment on it (repair errand). Rating tiers: Wire 1 kW (MVP), Heavy Wire
20 kW + Transformer isolation (v1). The Power overlay shows circuits, load vs
capacity, and the segment at risk.

### 8.5 Plumbing (and v1 ventilation)

Pipes move discrete packets — **10 kg liquid / 1 kg gas** per segment per sim
tick — from pumps toward consumers/vents. Junction arbitration is strict
round-robin, so flow puzzles behave identically every time (determinism is a
feature the player can feel). Pipe contents carry temperature; (v1) freezing
or boiling in-pipe breaks the segment.

### 8.6 Rooms

An enclosed space (walls/doors, ≤128 cells) becomes a **room**; the Rooms
overlay names it and the bonus. MVP room types:

| Room | Requirements | Bonus |
|---|---|---|
| Barracks | ≥1 Cot, 12–64 cells, door | +5 morale on sleep |
| Mess Hall | Mess Table, 12–64 cells | +5 morale on meals |
| Washroom | Latrine + Basin, door | Privacy: bladder trips 25% faster |
| Archive | Research Bench, door | +10% research speed |

### 8.7 Germs

Post-tier only. The v1 world plants the hooks (Polluted Oxygen, wash basins,
Mirefen) without the disease sim.

---

## 9. Research & progression

Research points come from Study errands at the Research Bench (Archive
Fragments, 25/cycle-of-study). The MVP tree is deliberately linear — five
rungs, each unlocking a visible life upgrade; v1 branches it.

**MVP (linear):**
1. **Cultivation** — Gloomsprout Bed, Compost Heap
2. **Meal Prep** — Mush Press, Ration Box
3. **Hydraulics** — Water Pump, Liquid Pipe, Liquid Vent
4. **Power Storage** — Battery Cell, Lamp
5. **Vault Systems** — Airlock Door, Wash Basin (caps the MVP arc)

**v1 branches:** Airworks (gas plumbing, Electrolyzer, Deodorizer) ·
Insulation (insulated builds) · Combustion (Bio Boiler, Heavy Wire,
Transformer) · Comforts (Grill, Shower, plumbed Toilet, decor) · Geothermal
(Steam, Geothermal Tap) · Care (Med Cot).

**Post:** Vault Logic (automation), Conveyance, Husbandry.

---

## 10. Threats & failure

### 10.1 The threat roster

| Threat | Vector | Early warning | Escape hatch |
|---|---|---|---|
| Suffocation | CO₂ pits, thin air, over-expansion | O₂ overlay, per-Vaulter breath bar, alert | Algae Pods; dig up, not down; door off the pit |
| Flooding | Careless digs, pump misuse | Visible water level rising | Water flows predictably; one tile stops it |
| Heat creep | Machine clustering, Forge tunnels | Temp overlay trend arrows (v1) | Spacing, Nullrock lines, (v1) insulation |
| Starvation | Farm collapse, spoilage | Kcal counter + trend on HUD strip | Mush Press eats abundant polluted water |
| Freezing (v1) | Frostreach expansion | Temp overlay | Lamps/heat leak on purpose |
| Morale spiral | Compounding neglect | Downcast bubbles name causes | §5.5 floors: refusal, never rampage; any met need recovers |

### 10.2 Failure & the Vault Log

Vaulter death is permanent (stasis offers replacements; the loss is morale +
labour). Colony wipe ends the run with the **Vault Log** — an epitaph screen
summarising cycles survived, population peak, cause of collapse, and the
run's three "moments" (auto-captured stats) — then offers the same seed or a
new one. No meta-progression at v1 (post-tier: Vault Archive cosmetic
unlocks).

Design rule: **every spiral must be escapable at the moment the player
notices it**, and the notice must come from the world (visible gas, visible
water, a bubble) before it comes from a popup.

---

## 11. Touch UX & HUD

### 11.1 Postures

Landscape only (manifest-locked, Zenithmon precedent). Two-handed phone grip
is the design target: thumbs reach the bottom corners; the bottom 20% of the
screen is the interaction band; the top strip is read-only status. All
interactive targets ≥ **57 logical px** (the engine's enforced minimum touch
target, DPI-scaled).

### 11.2 Gesture table

| Gesture | Context | Action |
|---|---|---|
| **One-finger drag** | default | Pan camera |
| **One-finger drag** | tool armed | Paint the tool's designation |
| **Two-finger pinch/pan** | always (even in tools) | Zoom to focal point + pan |
| **Tap** | world | Select cell/building/Vaulter → inspector sheet |
| **Tap** | UI | Activate control |
| **Long-press (500 ms)** | world | Quick-inspect tooltip without opening the sheet |
| **Flick** | default | Pan with inertia (decelerating glide) |
| **Double-tap** | world | Zoom step in toward point (v1 nicety) |
| **Back gesture / button** | anywhere | Close top sheet/tool; at root: pause menu |

Rationale: pan is one finger (the most common action gets the cheapest
gesture); tools borrow that finger, so **two-finger navigation stays live
inside every tool** — you never leave dig mode just to look at something.

### 11.3 HUD (landscape phone)

- **Top strip (read-only):** O₂ average %, kcal stock + trend, power
  stored/capacity, cycle clock (cycle # + block dial), alert badges.
- **Top-right:** pause ▸ 1× ▸ 2× ▸ 3× speed cluster (always visible — the most
  used control in the genre).
- **Bottom-left:** tool dock — Dig, Sweep, Priority, Cancel, Deconstruct.
- **Bottom-right:** Build (opens build menu), Overlays, Vaulters (roster).
- **Left edge, collapsed:** alert stack (tap = jump camera to cause).
- **Bottom sheet (on selection):** the inspector — identity, status, the
  errand ("Digging Sandstone — priority 6 — because: Dig ▲, nearest"),
  buttons (priority stepper, deconstruct, empty, etc.). Drag up for detail,
  down to dismiss.

### 11.4 Tool mode flow

Entering a tool (Dig etc.): the dock highlights, a thin coloured frame marks
the mode, an **Apply / Undo / Done** bar slides in above the dock, and
one-finger drag paints with the brush preview under the finger offset ~48 px
above the touch point (finger never hides the work). Rect mode toggle for big
orders. **Done** (or Back) exits. Sim **auto-pauses on entering the Build
menu** and on any full-screen sheet (toggleable in settings); paint tools run
live by default.

### 11.5 Build flow

Build button → category tabs (Structure, Power, Plumbing, Food, Comfort,
Research, Storage) → card grid (icon, name, cost, one-line role) → placement
mode: ghost follows the finger (same 48 px offset), material chip, rotate
button where applicable, red/green validity with a one-word reason
("Unsupported", "Blocked", "No room"), **Confirm** stamps the blueprint.
Placement repeats until Done — laying ten ladders is ten taps, not ten menu
trips.

### 11.6 Screen inventory

HUD · Build menu · Inspector sheet · Vaulter roster + Vaulter detail (needs,
traits, skills, schedule row) · Priorities screen (chore-family grid) ·
Schedule screen (12-block painter) · Research screen (tree) · Overlay picker ·
Cycle summary · Pause/settings · New game (map size, seed **number picker +
randomise** — no text field) · Vault Log (run end). Wireframe pass happens at
vertical-slice; widget mapping is TDD §14.

---

## 12. Overlays & readability

Overlay mode tints the world; the active overlay is named in a pill at top
centre (tap = off). MVP: **Oxygen** (breathability + pressure saturation),
**Temperature** (blue→red ramp + (v1) trend arrows), **Power** (circuit
colour, load bar per circuit, at-risk segment pulse), **Plumbing** (pipe
contents + flow direction ticks), **Priority** (1–9 numerals stamped on
designations), **Rooms** (tint + name label). v1: **Ventilation**, **Light**.

Readability rules: every overlay must survive (a) 6-inch phone at arm's
length, (b) min zoom, (c) the two common colour-vision deficiencies —
**each ramp dual-encodes** (colour + saturation/pattern/numeral), verified
with a simulated-CVD screenshot pass each milestone.

---

## 13. Art direction

### 13.1 Procedural 3D kit, orthographic camera

The user-selected presentation is **3D meshes in a fixed orthographic
side-view** ("2.5D"): the world renders through Zenith's real deferred
pipeline — instanced tile meshes, modelled buildings, skeletal-animated
Vaulters — locked to an ortho camera so it *reads* as a crisp 2D board.

Per the house pattern (Zenithmon pillar): **the generators are the art
direction**. Zero hand-made art: tiles, building meshes, props, and Vaulter
bodies/animation rigs are procedurally generated and baked to disk by
`ZENITH_TOOLS` builds, deterministically from seeds/recipes. Re-baking is
byte-identical.

### 13.2 Look targets

- **Tiles:** bevel-edged unit blocks, per-biome palette + material grain from
  the recipe; dug faces darker than natural faces so player-made space reads
  instantly. Silhouette rule: any two adjacent element types must separate at
  min zoom (≈8 px/cell) by value, not just hue.
- **Buildings:** chunky low-poly, one strong silhouette per building (the
  build-menu icon IS the ortho render), function-forward (moving parts move:
  the Crank Wheel visibly cranks).
- **Vaulters:** ~1.6 cells tall, big-headed stylisation for face-readable
  emotion at distance; one shared rig, per-Vaulter palette + 2–3 shape
  params; animation set per skill family (dig swing, build hammer, carry,
  climb, sleep, downcast idle).
- **Light:** no sun. The vault is dark; **light is warmth and safety** —
  ambient floor + point lights (lamps, machines, magma glow) doing the
  emotional work. Deferred clustered lighting makes this nearly free (TDD
  §12); shadows stay off (budget + readability).
- **Liquids/gases:** cell-space effects (fill surfaces with slight animated
  meniscus, gas as low-contrast tinted haze + drifting motes) — rendered by
  the custom cell-data pass, not meshes (TDD §12.4).

### 13.3 Camera

Fixed-angle orthographic side view, no rotation. Zoom range ≈8 px/cell
(overview: full standard-map width on a phone) to ≈96 px/cell (portrait-level
close-up). Depth exists only as parallax nicety: backwall plane slightly
darker, foreground props slightly proud.

---

## 14. Audio direction

> **Engine reality:** Zenith has **no audio system today** — no backend,
> mixer, decoder, or asset path on any platform. Building one is a P1 engine
> work item (TDD §2, gap #6). The vertical slice will be silent; Alpha lands
> the audio MVP.

Direction, for when it lands: quiet, tactile, diegetic-first.

- **Ambient bed** per biome + depth (Hearth hum, Mirefen drips, Frostreach
  wind-through-cracks, Forge sub-rumble), crossfaded by camera position.
- **Machine voices:** every powered building has a loop (the base *sounds*
  alive when power flows and goes silent in a blackout — the scariest sound
  in the game is nothing).
- **UI foley:** soft mechanical ticks for taps, a satisfying stamp for
  designations, distinct chirps per alert severity (learnable eyes-free).
- **Music:** sparse ambient motifs at emotional beats only (cycle start, new
  Vaulter, first death) — battery-aware by design (§16.4): no continuous
  score.
- Mix buses: SFX / Ambience / UI / Music with independent sliders (16-voice
  MVP mixer per TDD gap #6).

---

## 15. Onboarding

- **First-cycle script ("Warden Protocols"):** a 10-step objective checklist
  woven into cycle 1–3 — *wake the Vaulters → dig to the water pocket →
  build a ladder → place the Algae Pod → build 3 Cots → seat a Mess Table →
  crank the wheel → store 6,000 kcal → survive to cycle 3*. Each step
  highlights its tool the first time (pulse on the dock icon), never locks
  the sandbox.
- **Contextual toasts:** one-line, once-ever tips triggered by state (first
  CO₂ pocket seen → "Carbon dioxide sinks. Air flows over it — Vaulters
  don't."), dismiss on tap, all revisitable in a Log tab.
- **No text entry anywhere.** Vaulter names come from a curated pool with a
  reroll die; colony name is picked from generated pairs; the world seed is a
  **numeric picker + randomise**. This is a hard rule: the engine has no
  soft-keyboard/IME support, and the design treats that as a constraint to
  design *around* permanently (TDD §2 gap #20), not a missing feature.

---

## 16. Session & retention design

### 16.1 Time

One **cycle = 300 s of sim time** (= 1,500 base ticks at 5 Hz): five minutes
at 1×, under two at 3×. A commute session (10 min) completes 2–6 cycles —
enough for a project (dig the well, plumb the farm) to finish. The cycle is a
shift-clock, not a day (no sky): block dial on the HUD, schedule §5.6 keyed
to it.

### 16.2 The autosave contract

The colony must never lose meaningful progress to the OS:

- Autosave every cycle boundary **and** every 60 s of real time (whichever
  first), plus on every backgrounding once the engine's save-on-background
  hook lands (TDD §2 gap #8 — until then the 60 s cadence bounds loss).
- Resume opens **exactly** where the player left: camera, open sheet, armed
  tool, sim paused.
- Manual save slots: 3 + autosave (Zenithmon slot pattern).

### 16.3 Cycle summary

At each cycle boundary: a dismissible one-card summary (kcal net, O₂ net,
power net, notable events), never blocking at 2×/3× beyond a toast. It doubles
as the retention beat: "one more cycle" should always be a visible, cheap
decision.

### 16.4 Battery posture

30 fps default; the sim itself idles at ~0.4 ms/frame amortised (TDD §15).
No push notifications at v1; nothing runs in the background — the vault
waits, frozen, exactly as sealed vaults should.

---

## 17. Accessibility

- **Touch targets:** engine-enforced 57 logical px minimum everywhere; audit
  each milestone with the touch-target debug overlay (TOOLS build).
- **Text scale:** 0.85×–1.4× slider; HUD reflows (the strip drops to icons +
  values at 1.4×).
- **Colour vision:** every overlay and alert dual-encoded (§12); CVD
  simulation screenshot pass per milestone.
- **Left-hand mode:** mirrors the dock/build corners.
- **No twitch inputs:** nothing in the game requires speed or precision —
  pause is always one tap, every gesture has a large-slop fallback (tap-tap
  rect placement instead of drag-paint works for every tool).
- **Readability floor:** min-zoom legibility rule (§13.2) is an accessibility
  rule, not just art direction.

---

## 18. Business posture

Premium, **$6.99** launch anchor. No ads, no IAP, no server dependency at v1
(fully offline). Google Play (Android 10+, arm64-v8a; device floor per TDD
§15 tiers). Windows build is a development vehicle, not a storefront SKU, at
v1. Post-launch: content updates on the post-tier list; tablet "Deep map"
mode is the natural paid-update candidate if v1 lands.

---

## 19. Scope-tier matrix (BINDING)

The in/out gate. A feature not on this table is out. Moving a row needs a
deliberate doc revision, not an implementation-time decision.

| Feature | MVP (Vertical Slice → Alpha) | v1.0 (ship) | Post |
|---|---|---|---|
| Map sizes | Small, Standard | + Deep (Tier A) | Custom sizes |
| Elements | ~22 (§4.2 MVP rows) | ~28 (+ steam, magma, polluted O₂…) | Full exotic set |
| Gas/liquid/thermal sim | ✅ core | + latent heat ledger, in-pipe states | — |
| Phase transitions | melt/freeze basic | full table + hysteresis ledger | — |
| Vaulters | 3→12, needs, traits, skills, schedules | + Care skill, group presets | Relationships |
| Morale | slowdown/refusal floor | + decor, expectations | Stress behaviours |
| Errands/priorities | full model (§6) | + per-building enable toggles | Priority automation |
| Buildings | ~22 (§7.1) | + ~18 (§7.2) | §7.3 |
| Power | wire, battery, crank, overload | + heavy wire, transformer, Bio Boiler, geothermal | Logic automation |
| Plumbing | liquid loop (§7.1) | + gas loop, insulated, in-pipe temp | Conveyors |
| Rooms | 4 types | + 3 (Med, Great Hall, Archive II) | Full room zoo |
| Research | 5-rung linear | branched tree (§9) | Post branches |
| Germs | — | hooks only | Full disease sim |
| Creatures | — | — | Vaultlife + ranching |
| Overlays | O₂, Temp, Power, Plumbing, Priority, Rooms | + Ventilation, Light | Automation, Germs |
| Audio | — (silent slice) | full MVP audio (§14) | Adaptive score |
| Onboarding | Warden Protocols + toasts | polish + Log tab | Scenario tutorials |
| Platforms | Windows dev + Android sideload | Google Play (AAB) | Tablets first-class; iOS **only if** an engine iOS layer materialises |
| Meta | — | Vault Log | Vault Archive unlocks |

---

## 20. KPIs & playtest plan

**KPIs (internal-track beta):**

- Crash-free sessions ≥ **99.5%** (the engine risk register drives this —
  TDD §19).
- Median session ≥ **8 min**; sessions/DAU ≥ 2 (the session-shape thesis).
- **Cycle-10 rate** ≥ 35% of started colonies (retention proxy: the player
  who reaches cycle 10 has internalised the loop).
- Tutorial completion (Warden Protocols) ≥ 70%.
- Thermal/battery complaint rate ≈ 0 (30 fps posture holds); battery soak
  ≤ 12%/hr on the Tier-A reference device.

**Playtest cadence:** hands-on gesture test at vertical slice (does pan/zoom/
paint feel right on a real phone — the whole game rests on §11); first-cycle
protocol test at Alpha (10 fresh players, no prompting, measure §15 checklist
completion); soak + device matrix at Beta (TDD §16 device checklist).

---

*Companion: [TechnicalDesignDocument.md](TechnicalDesignDocument.md) — the
architecture that implements this design, its performance budgets, and the
engine gap analysis (the Zenith work this game requires).*
