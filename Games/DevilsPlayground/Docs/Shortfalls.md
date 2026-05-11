# Devil's Playground — Shortfalls & Gap Analysis

**Document purpose:** A frank, gap-by-gap audit of where the current `DevilsPlayground` prototype falls short of the shipping vision described in [GameDesignDocument.md](GameDesignDocument.md). Written for producers, leads, and external partners assessing the runway from "skeleton-grade port" to "ship-ready PC/console title."

**Scope:** Everything in the current `Games/DevilsPlayground/` tree as of 2026-05-11 (commit-ish: skeleton-grade UE port, M0/M0.5/M1 milestones complete).

**Verdict at a glance:**
- **What works today:** ~25% of the shipping scope. The bones are real.
- **What's stubbed but architected:** ~30%. Hook points exist, behaviour is placeholder.
- **What's not yet imagined in code:** ~45%. Net-new design from the GDD.

**Estimated time to bridge:** ~18 months with a team of 12 (see GDD §12). The systems gaps are tractable; the **content volume** and **art-direction** gaps dwarf everything else.

---

## 1. Headline gaps (the things that block shipping today)

These are the issues that, if left as-is, prevent the prototype from being demonstrable to a publisher as anything more than a tech sample.

### 1.1 The witch-finder cannot end the run

**Status:** Critical. **GDD ref:** §4.5.

The prototype's `Priest_Behaviour` perceives, pursues, and navigates — but **there is no collision check between the priest and the possessed villager** and no apprehend logic. The player literally cannot lose to the antagonist. The only loss states currently available are (a) running out of life-timer with no other villager nearby (which simply soft-locks rather than ending the run) and (b) wall-clock timeout outside the game.

For a stealth game whose entire dramatic premise is "an inquisitor hunts you," this is the gap. Until it lands, every other priest improvement is decoration.

**Bridge cost:** ~3 engineering weeks. Add an `Apprehend` BT branch that triggers on overlap with the possessed body's collider, with a 3 s channel that can be interrupted by switching bodies. Hook into a new `DP_OnRunLost` event.

### 1.2 No real navmesh — only a single 200×200 m polygon

**Status:** Critical. **GDD ref:** §6.1, §4.5.

`DP_AI::GetOrBuildLevelNavMesh` returns a synthetic flat quad. The priest can walk through any wall. `DPDoor::SyncNavMeshBlock` is wired up correctly but operates on a navmesh that doesn't have doorways to begin with, so it's a no-op. The path-finding tests pass because they only assert "distance to target decreases" — not "the path respects geometry."

This is the single largest blocker to any meaningful level design.

**Bridge cost:** ~6 engineering weeks. Two paths:
- **Generate from colliders.** Walk the scene's static collider set, voxelise, build a polygon soup, triangulate. Standard navmesh generator. Reuse-able for other Zenith games (Combat, AIShowcase).
- **Pre-bake assets.** Author `.znavmesh` files in the editor, ship as data. Less elegant but ships faster.

Recommend (a) — it pays back across the engine.

### 1.3 Pause is a lie

**Status:** Critical. **GDD ref:** §7.2.

`DPPauseMenuController_Behaviour` toggles a UI overlay text element when Esc is pressed. The game's simulation keeps running underneath. For a stealth game where every decision is time-priced, this is unshippable — players need to pause to think.

**Bridge cost:** ~1 engineering week. The engine has `Zenith_SceneManager::SetScenePaused` (or equivalent — verify). Wire the pause overlay to call it. Add an audio ducking pass.

### 1.4 No villager animations at all

**Status:** Critical-cosmetic. **GDD ref:** §8.3.

Villagers are static capsule colliders. They don't walk; their position is set by velocity each frame. There are no idle animations, no walk cycles, no possession-channel animations. The held-item visual is a tinted cube floating 1.6 m above the body. Everything in `DPMaterials.h/.cpp` is procedural tint generation on top of placeholder cubes.

This is not "polish" — it's the difference between "code prototype" and "game." No publisher demo can hide it.

**Bridge cost:** ~8 character-art weeks for 24 archetypes × 12 animations + integration. Engine animation system (`Flux/MeshAnimation/`) is mature; the work is asset authoring + retargeting.

### 1.5 Items, doors, chests are cubes with tints

**Status:** Critical-cosmetic. **GDD ref:** §4.3, §4.4, §8.

The item-tag → tint-colour system (`DPMaterials::GetOrCreateColouredVariant`) is clever debug rendering but it's the entire current visual language of items. Iron, key, skeleton key, and all five objectives are visually distinguished by hue alone. Doors are scaled cubes. The chest's lid does not actually open (`m_fOpenT` is incremented but no transform applied).

**Bridge cost:** ~6 art-weeks for the full prop set (~40 unique props at shipping). Includes modelling, texturing, light-baked variants.

---

## 2. Mechanics gaps (the design surface the prototype hasn't touched)

These are systems described in the GDD that don't exist in code at all yet. None are "blockers" individually, but the cumulative absence is what makes the prototype 25%-complete rather than 60%.

### 2.1 Possession depth

| GDD feature | Prototype state |
|---|---|
| 24 villager archetypes with distinct timers/abilities | 1 archetype. All villagers identical. |
| Possession range tied to last death anchor | No range limit; any villager anywhere on the map can be clicked. |
| Possession channel (0.8 s for Devout) | Instant for all. |
| Possession cooldown (1.5 s after voluntary switch) | None. Players can chain-click. |
| Demon-scent (per-body, decays) | No tracking of possession history per body. |
| Voluntary switch (faint vs. die) | Cannot voluntarily switch — switching during life-timer just transfers state immediately, body stays put. |
| Sprint as life-cost mechanic | Single speed. No sprint, no walk. |

**Bridge cost:** ~6 engineering weeks for the new state-machine + ~4 design weeks for archetype tuning.

### 2.2 The witch-finder is half a character

The prototype's priest covers perception (sight + hearing), patrolling, investigating, and pursuing. That's roughly 60% of the GDD's antagonist. Missing:

- **Apprehend** (see §1.1).
- **Hounds** (Aram & Tobit). Separate agents with their own perception cones. No prototype for any of this.
- **Demon-scent tracking.** Tied to §2.1.
- **Three personality variants** (Cautious / Cruel / Drunk). Single configuration in code.
- **Memory.** Last-investigated waypoint, last-suspicious-villager. Currently each tick is stateless beyond the BB.
- **Mid-Night escalation** (lantern → hounds → flintlock → bless → exorcise).
- **Whisper-line narration.** The priest's behaviour is not surfaced to the player at all. Players cannot understand why he's at a given location.

**Bridge cost:** ~10 engineering weeks for all of the above, plus a parallel design pass on tuning.

### 2.3 No meta-progression / Liminal

The GDD's roguelite layer — the **Liminal** hub, three hermit trees, the Knot currency, permanent unlocks — does not exist in the prototype. There's no save system for run results, no currency, no unlock graph, no menu surfacing.

**Bridge cost:** ~8 engineering weeks. Save/load + menu integration is the lion's share. The unlocks themselves leverage existing systems (timer modifications, ability flags on villagers).

### 2.4 Procedural Night-to-Night variance

The prototype loads a single hand-authored `L_GameLevel` scene. There is no:
- Reagent spawn shuffler.
- Locked-door randomiser.
- Villager archetype drawer.
- Aelfric variant picker.
- Weather state.

`Project_RegisterEditorAutomationSteps` is the right hook for the shuffler (it already authors the scene declaratively) — but the steps are currently deterministic.

**Bridge cost:** ~4 engineering weeks. The architecture is there.

### 2.5 Reagent system is five duplicates of one thing

`DP_ItemTag::Objective1` through `Objective5` are five interchangeable identifiers. The GDD's reagent design (each with a unique pickup channel, a candidate-spawn-set, a special behaviour like the bog-water evaporating in 8 s) does not exist. The pentagram accepts any objective tag without distinguishing them.

**Bridge cost:** ~3 engineering weeks + ~4 design weeks (per-reagent flavour).

### 2.6 Crafting is a single hardcoded recipe

`DPForge_Behaviour` does Iron → Key. The GDD's six-recipe table (Iron+Wood, Iron+Brass, etc.) requires a recipe-data system and craft UI. The forge has no animation, no time cost, no sound — the player taps F and the item appears.

**Bridge cost:** ~3 engineering weeks.

### 2.7 No charms, no distractions beyond noise machines

The GDD posits charms (rowan, salt, St. George medal), distraction props (music box, tin whistle, tinder-bundle), and a confessional hide-mechanic. None of this exists in code. The single noise machine (`DummyNoiseMachine_Behaviour`) is the entire surface area of "player tools to manipulate the priest."

**Bridge cost:** ~5 engineering weeks for the systems, ~3 design weeks for tuning.

### 2.8 No alternate win conditions, no Joan Trew, no narrative

The prototype's win condition is: 5 objectives → pentagram → victory event. The GDD's Night 5 ("free Joan from the longhouse") and Night 7 ("the chapel is sealed") require:
- Non-possessable NPCs.
- NPC-state-driven objectives (Joan's location, Joan's restraint state).
- Narrative scaffolding (cutscenes, between-Night interstitials, branching ending state).

**Bridge cost:** ~6 engineering weeks for narrative-scaffolding systems + writing/cutscene authoring is its own production discipline (~12 weeks).

### 2.9 Camera is intentionally locked

The orbit camera is fixed-pitch at ~1.2 rad (~69°). The GDD requires adjustable pitch (0°–80° accessibility), an optional follow mode, and console-input-mapped rotation. The prototype supports Q/E rotate, mouse-wheel zoom, no follow, no tilt.

**Bridge cost:** ~2 engineering weeks.

### 2.10 Inventory is one slot — but with no "drop" or "give" verb

Held items are managed via `DP_Player::SetHeldItem` / `RemoveHeldItem`. There is no drop-on-ground verb (G key in GDD). There's no give-to-other-villager (which the GDD doesn't strictly require but hand-off chains depend on dropping in a location for a later possession to grab).

**Bridge cost:** ~2 engineering weeks. Inventory & pickup are well-architected; drop just needs a UI verb + a 3D world-pickup re-anchor.

---

## 3. Engine / tech gaps

Issues in the engine or its DevilsPlayground integration that limit shipping quality.

### 3.1 Volumetric fog shader is not implemented

`DPFogPass.h/.cpp` registers a no-op render pass and disables the engine's existing 6-pass fog system. The GDD's fog-of-war design (memory fog, light-driven visibility, fog-of-war on Aelfric only by edge visibility, weather variants) needs an actual shader. The `Flux/Fog/CLAUDE.md` system probably has reusable volumetric-fog code; the question is whether re-using the engine fog and overlaying fog-of-war as a separate post-pass is cheaper than the current "disable engine fog, do everything in DP" approach.

**Bridge cost:** ~6 engineering weeks for a custom shader pass with memory-fog and per-villager visibility holes.

### 3.2 Particle effects are placeholder

`DPFogPass::RegisterParticleConfigs` exists, the witch's spawn coordinate is exposed, and a single PFX_Witch config is registered. There is no actual particle work — possession transitions, frost trails, lantern dust-motes, hearth smoke, hound breath in cold air. The GDD calls for ~25 distinct particle systems.

**Bridge cost:** ~6 art-weeks (FX artist) + ~2 engineering weeks for system hooks.

### 3.3 Audio is silent

The prototype has no audio integration in DP-specific code (engine has a sound system but DP doesn't drive it). No SFX for doors, footsteps, forge, bell, Aelfric mutters, hounds. No music. No spatialised sound.

**Bridge cost:** ~12 weeks (audio lead + composer) for a complete bible.

### 3.4 Editor automation system is the right tool, used the wrong way

`Project_RegisterEditorAutomationSteps` authors scenes declaratively via `AddStep_*` calls. This is the **canonical pattern** for Zenith scenes (per memory: "Marble.cpp is the canonical template"). The DP project uses it correctly today.

But for shipping, scenes are loaded from `.zscen` files, not authored at runtime. The Sharpmake configurations gate this on `ZENITH_TOOLS`. Currently the project is well-architected for tools-mode authoring but the **non-tools-mode pre-baked scene assets are out of date** the moment the authoring code changes. Workflow risk.

**Bridge cost:** ~1 engineering week to add a "bake all scenes" CI step that re-runs the editor-automation chain and writes `.zscen` files automatically. Also confirms scenes load correctly without `ZENITH_TOOLS`.

### 3.5 Save / load doesn't exist

There's no save-game system in DP or visible in the broader Zenith engine touched by this project. For a roguelite with permanent meta-progression, save/load is foundational. Cross-platform cloud save adds complexity.

**Bridge cost:** ~4 engineering weeks for local save + ~3 weeks for cloud save (Steam, Xbox Live, PSN).

### 3.6 No localisation pipeline

All strings are hardcoded `const char*` literals. Whisper-line narration, item names, Aelfric VO subtitles, menu text, archetype names — all need to flow through a localisation system. The GDD targets 8 languages at launch.

**Bridge cost:** ~3 engineering weeks for the loc table system + per-language localisation budget (~$15k/language for ~5000 words).

### 3.7 Performance is not validated for console targets

The prototype runs in debug builds on PC and has automated test coverage at ~60 fps with fixed dt. The shipping targets (Switch 2 at 30 fps, Series S at 60 fps with dynamic res) have not been profiled. The fog-hole gather (up to DP_FOG_MAX_HOLES = 60 entries/frame, scene-wide light enumeration) and the perception system's stimulus broadcast (currently O(agents × stimuli)) are both candidates for hot-path optimisation.

**Bridge cost:** ~6 engineering weeks for a full perf pass, allocated across the 18-month dev runway.

### 3.8 Non-tools build is broken (engine-wide)

Per memory: `feedback_msvc_static_init_dead_strip.md` notes that `ZENITH_REGISTER_COMPONENT` doesn't fire if the `.obj` is unreferenced. This bites DP because component registration (e.g., `Zenith_AIAgentComponent`) requires runtime force-add — already worked around in `Priest_Behaviour::OnAwake`. But the underlying issue (per `project_nontools_build_broken.md`) is that **only `*_True` configs build right now**. Shipping needs `*_False`.

**Bridge cost:** Unknown — this is engine-level. Likely ~4 engineering weeks to audit dead-stripped registrations and add force-anchor mechanisms.

### 3.9 No console-specific platform layer for DP

The project supports Android (per Sharpmake configs) but the GDD targets Switch 2 / Series X|S / PS5. Each requires a platform layer in `Zenith_<Platform>` (akin to `Zenith_Windows_*`). Game-specific work is minor; engine-layer work is significant and outside the DP project's scope.

**Bridge cost:** ~6 months engine-team work across all three platforms. Out of scope for this analysis but on the critical path.

---

## 4. Content volume gaps

The GDD describes a world. The prototype contains a fraction of that world.

| Content category | GDD target | Prototype state | Gap |
|---|---|---|---|
| Map | 1 × hand-authored 200×200 m, 6 districts | 1 × 100×100 m, undistricted | 4× area, 6 districts to author. |
| Villager archetypes | 24 designed, 14–18 per Night | 1 type, 14 instances | 23 designs + asset work. |
| Reagents | 14 unique, 5 per Night | 5 interchangeable | 9 new designs + asset work. |
| Interactables (props) | ~12 unique kinds + variants | 6 (door, double door, chest, forge, pentagram, noise machine) | 6+ new kinds. |
| Aelfric variants | 3 | 1 | 2 new BT configurations. |
| Reagent pickup variants | Per-reagent (e.g., bog-water evaporates) | Single behaviour | 14 special behaviours. |
| Particle systems | ~25 | 1 (witch placeholder) | 24 to author. |
| Distinct SFX | ~80 (per interactable + Aelfric VO + ambient) | 0 | 80. |
| Music | 2 dynamic tracks + interstitial bed | 0 | All. |
| Cutscenes | ~14 (2 per Night) | 0 | All. |
| Tutorial moments | 6 in Night 1 | 0 | All. |
| Localisation | 8 languages × ~5000 words | 0 | ~40k words to translate + LQA. |
| Menus | Main, Liminal, Pause, Settings, Daily, Hauntings | Main (1 button), Pause (overlay text) | 4 net-new menus. |

The honest summary: **the prototype contains the engineering pattern for each of these categories, but ~3% of the content volume.**

---

## 5. QA & production gaps

### 5.1 Automated test coverage is strong — for what's there

The 28-test harness (`Tests/`) is genuinely impressive for a skeleton-grade port. Tests cover: scene loading, possession round-trip, life timer, pickup, victory, perception bridge, pursuit, fog-pass contract, material side-table, gym scenes for each subsystem. The harness supports batch mode for fast iteration.

This is the project's largest **strength** carried over from the M0/M0.5/M1 milestones, and shipping should preserve it. The pattern (gym scenes per subsystem + integration tests for the full game level) generalises directly.

**Risk:** As content scales, gym scenes will lag. Discipline needed to keep them in sync.

### 5.2 No human-driven QA process

Automated tests catch regressions in implemented mechanics; they don't catch design issues, balance issues, or "the priest feels unfair" issues. There is no playtest pipeline, no telemetry, no replay-debug tool.

**Bridge cost:** ~3 engineering weeks for the **last 10 seconds** replay debugger (called out in GDD §13 as load-bearing for Aelfric tuning) and a basic telemetry stub. Playtest pipeline is a producer task; ~$50k/year budget for external playtester sessions.

### 5.3 No CI for `.zscen` re-baking

See §3.4. Without this, content drift between authoring code and baked assets is a constant low-grade risk.

### 5.4 No certification pre-flight for consoles

The console certification checklists (TCR for Xbox, TRC for Switch, TRC for PS) impose hundreds of requirements (controller-disconnect behaviour, suspend/resume, friend-list integration, achievement plumbing, save-data corruption recovery). None of this is present in DP today. **Bridge: 3 months of pre-cert work in months 14–18 of the dev runway.**

---

## 6. Design risks the prototype has already surfaced

These aren't gaps so much as **warning lights** the prototype has illuminated:

### 6.1 The 30-second timer might be wrong

The prototype's tests had to bump villager move speed from 4 m/s to 8 m/s because the `HumanPlaythrough_Test` couldn't finish within budget at 4 m/s. The implication: at the original tuning, just **traversing the map within one possession** was hard. At 8 m/s it's easier but feels less weighty.

**Risk:** the 30-second timer is the game's defining parameter. It needs ~50 hours of playtesting to land. The prototype suggests it may need to be context-modulated (longer in the chapel, shorter on the moor, etc.). Plan accordingly in design budget.

### 6.2 Priest's "knows who is possessed" fallback

In `Priest_Behaviour::BridgePerceptionToBlackboard`, when no perception target qualifies, the priest **falls back to `DP_Player::GetPossessedVillager()` directly** — meaning even out of sight, the priest knows which body is possessed. The comment notes this is to make the harness pursuit test deterministic.

**Design risk:** in shipping, this fallback is **wrong** for the game's tension. The witch-finder should sometimes guess wrong. Removing the fallback is one line of code; rewriting the harness pursuit test to seed perception properly is ~2 engineering days. **Do this in week one of the post-M1 sprint.**

### 6.3 Held-item visual is a floating cube above the body

A small bug-shaped tell: the prototype's held-item visual hovers 1.6 m above the villager rather than being held in their hand. When the villager mesh + animation system arrives, this needs to migrate to a bone-attached socket. Currently the position is set every frame in `DPVillager_Behaviour::PositionHeldItemVisual`.

**Cost:** ~1 engineering day, but worth flagging now so the animation team plans for it.

### 6.4 14 vs. 17 villager mismatch

The CLAUDE.md says "14 villagers" but the GameLevel spawns "17 villagers" per the file map. The prototype has drifted from its own documentation in just the engineering of the level — a small symptom of the gap between authoring code and pre-baked scenes (§3.4).

### 6.5 No item drop on possession switch

When the player switches possession, the old body still holds its item. The GDD assumes drop-on-the-ground is a verb (G key). Currently held items follow the villager forever, including after death. This is the **biggest single design surface missing** from hand-off chains.

---

## 7. Bridge plan — phased

A suggested phasing for the 18-month runway, prioritising shippability:

### Phase 1 — Foundation (months 0–4)

- Real navmesh generation (§1.2).
- Apprehend / run-lose state (§1.1).
- Real pause (§1.3).
- Voluntary possession switch + drop verb (§6.5, §2.10).
- Possession cooldown & demon-scent (§2.1).
- `.zscen` re-bake CI (§3.4, §5.3).
- Remove priest's omniscience fallback (§6.2).
- Save/load scaffolding (§3.5).

**Output:** A 5-minute demo that demonstrates the *core loop* with real stakes. Not pretty. Not deep. But losable.

### Phase 2 — Depth (months 4–10)

- Villager archetypes (5 to start, 12 by end of phase) (§2.1).
- Witch-finder hounds + variants + escalation (§2.2).
- Reagent uniqueness (§2.5).
- Crafting expansion (§2.6).
- Charms & distractions (§2.7).
- Liminal hub + meta-progression for one track (§2.3).
- Animation + character art baseline for 5 villagers + Aelfric (§1.4).
- Volumetric fog shader (§3.1).
- Music dev start (§3.3).

**Output:** A 30-minute vertical slice playable by external partners. Demonstrably *the game*, just not full content.

### Phase 3 — Content (months 10–14)

- All 24 villager archetypes (§4 content).
- All 14 reagents (§4 content).
- Vexholme map full polish (§4 content).
- Procedural shufflers (§2.4).
- Particles (§3.2).
- SFX bible (§3.3).
- Localisation pipeline (§3.6) + first 3 languages.

**Output:** Content-complete build. All systems shipped, all archetypes implemented.

### Phase 4 — Ship (months 14–18)

- Cutscenes / narrative (§2.8).
- Tutorialisation (§7.3 in GDD).
- Console platform layers (§3.9) — done in parallel by engine team months 8–18.
- Console certification (§5.4).
- Localisation finalisation.
- Performance optimisation pass (§3.7).
- Beta + playtest cycle.
- Day-1 patch backlog.

**Output:** Ship. Day-1 build for all four platforms.

---

## 8. What we should keep

Not all of the prototype is a list of things to fix. The following are genuine **assets** that the shipping team should preserve:

1. **The `Project_*` lifecycle hooks** (9 entry points, mirroring Combat). Clean architecture. Don't deviate.
2. **The `DP_*` namespace public-interface contract** (Source/PublicInterfaces.h/cpp). The discipline of behaviours-talk-only-via-namespace is preserved-worthy.
3. **The automated test harness** (28 tests, batch mode, gym scenes). One of the strongest QA stories of any in-development game I've seen at this stage. Build on it; don't replace it.
4. **The editor-automation scene authoring pattern.** Declarative `AddStep_*` calls reading like a recipe. Bake into `.zscen` for ship, but keep the authoring DSL for live iteration.
5. **The perception-system bridge pattern** in `Priest_Behaviour::BridgePerceptionToBlackboard`. The right shape for the GDD's Aelfric variants — just needs the omniscience-fallback removed.
6. **The fog-hole clear-and-rebuild strategy.** Simple, deterministic, no subscription overhead. Keep.
7. **The material tinting system** for debug visualisation. The shipping prop set will replace it visually, but keep the tinting code path as a debug-only build-flag mode for tooling.
8. **The source-bug guards.** `RemoveHeldItem` null-deref fix, `FindItemByTag` miss-return, `OnExitRange` event-handle correctness. Each is the kind of subtle correctness work that pays back for the entire dev cycle. Preserve.

---

## 9. Summary

**The prototype is good at being a prototype.** It demonstrates that the core loop *can* be built on the Zenith engine. The system architecture, namespace contracts, test harness, and authoring pattern are shipping-quality patterns at a non-shipping content level.

**The prototype is not good at being a game.** Every single one of the game's load-bearing dramatic moments — the priest catching you, the body burning out at the wrong moment, the chapel bell ringing for the first time, the hand-off chain succeeding by 2 seconds — is currently a placeholder, a no-op, or a thing you'd have to imagine while looking at debug capsules and tinted cubes.

The work between here and ship is real, sized, and tractable: **roughly 18 months and a team of 12**, with the majority of effort going into content, animation, audio, and the antagonist's full character — *not* into rearchitecting what's there. The bones are real. The flesh is what's missing.
