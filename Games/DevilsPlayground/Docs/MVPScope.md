# Devil's Playground — MVP Scope

**Document purpose:** A binding definition of what counts as **Minimum Viable Product** for *Devil's Playground*. Anything in here ships in MVP; anything not in here is **post-MVP**.

**Status:** Ratified 2026-05-11.
**Estimated runway:** 4 months of full-time autonomous-agent work from 2026-05-11.

---

## 0. The MVP Sentence

If a player can do **this one sentence** end-to-end, the MVP is shipped:

> *"I clicked a villager, watched a 30-second timer start, ran them across a single playable level collecting five glowing tokens through doors and a forge while a priest hunted me; I either placed all five at the pentagram and saw 'victory', or the priest caught me and I saw 'apprehended'."*

Every system, asset, and test in this scope exists to support that sentence. Everything else is a post-MVP feature.

---

## 1. What's In

### 1.1 Gameplay systems (ALL of these working)

| System | MVP scope | Notes |
|---|---|---|
| Possession | Click-to-possess, 30 s timer, voluntary switch (faint), burn-out (die), drop verb | Per [GDD §4.1](GameDesignDocument.md). |
| Possession range | 15 m from last-death anchor | Per GDD §4.1. |
| Possession cooldown | 1.5 s after voluntary switch | Prevents click-spam. |
| Demon-scent | Per-body, +0.3/possession, decay 0.05/s | Telemetry signal; hound effect deferred (no hounds in MVP). |
| Sprint | Shift/R1, 12 m/s, +3 s/s extra life-cost | Per GDD §4.2. |
| Walk-quiet | Ctrl/L1, 4 m/s, halves Aelfric's hearing range | Per GDD §4.2. |
| Movement | Camera-relative WASD on possessed body | Already prototyped. |
| Top-down orbit camera | Q/E rotate, mouse-wheel zoom | Already prototyped; expand zoom range. |
| Real pause | Esc freezes simulation; resume on Esc | Currently a fake overlay — needs real pause. |
| Real navmesh | Generated from static colliders; doors block path | Currently a flat polygon — full rebuild. |
| Witch-finder perception | Sight (25 m, 110° FOV) + hearing (30 m) | Already prototyped; remove omniscience fallback. |
| Witch-finder pursuit | NavMesh agent moves toward `BB.TargetWithDevil` | Already prototyped. |
| Witch-finder investigate | Move to last-heard sound; wait 2 s; clear | Already prototyped. |
| Witch-finder patrol | Random points within suspicion radius | Already prototyped. |
| **Apprehend** | Within 2 m of possessed body → 3 s channel → `DP_OnRunLost` | **THE MVP'S BIGGEST NEW GAMEPLAY MECHANIC.** Currently absent. |
| Items (Iron, Wood, Brass Key, Skeleton Key, 5 reagents) | Pickup on proximity, single-slot inventory, drop verb | Wood is new for MVP (used in forge recipes). |
| Hand-off chains | Drop in district A, possess different villager, pick up, walk to pentagram | Drop verb is new for MVP. |
| Forge crafting | Iron→Brass Key; Iron+Wood→Spike; Iron+Brass→Skeleton Key | Three recipes for MVP. |
| Doors (single + double) | Key-gated; SkeletonKey is universal | Already prototyped; needs real navmesh integration. |
| Chests | Open animation + procedural loot (Iron, Wood, or Brass Key) | Currently lid pivot stub. |
| Noise machines | Interact → emit perception stimulus | Already prototyped. |
| Pentagram win condition | Inscribe 5 reagents → `DP_OnVictory` | Already prototyped. |
| Fog of war | Per-frame rebuild, villager + light holes, memory fade | Currently no-op shader; needs real shader. |
| HUD (placeholder-art) | Life timer, held-item readout, reagent counter, status banner, whisper line | Currently text; MVP keeps text but with whisper line added. |
| Loss states | Apprehended, dawn timeout, no villagers left | Currently only soft-locks. |
| Pause menu | Resume / Restart / Quit-to-menu | New for MVP. |
| Main menu | Play / Quit | Currently single Play button; add Quit. |
| Save/load (single run) | Survive Alt-Tab without losing state | Just the active run; no meta-progression yet. |

### 1.2 Content (just enough for the MVP sentence)

| Content | MVP count | Post-MVP target |
|---|---:|---:|
| Villager archetypes | **4** (Farmhand, Sexton, Devout, Child) | 24 |
| Reagents | **5** (Caul, Hare's Tongue, Bog-Water, Burial-Coin, Bell's Soul) | 14 |
| Witch-finder | **1** (canonical Aelfric, no variants) | 3 variants + hounds |
| Forge recipes | **3** (Iron→Key, Iron+Wood→Spike, Iron+Brass→Skeleton) | 6 |
| Door variants | **2** (single + double) | 6 |
| Chest variants | **1** (placeholder) | 4 |
| Noise machine variants | **1** (placeholder) | 3 |
| Level | **1** — the existing `GameLevel` scene | Full Vexholme (200×200 m, 6 districts) |
| Distractions, charms | **0** (post-MVP) | Music box, tin whistle, tinder, rowan, salt, St. George medal |
| Cutscenes | **0** (post-MVP) | 14 |

### 1.3 Assets (S0 only)

Per [AssetManifest.md §0.2](AssetManifest.md) — S0 sourced placeholders.

- Mixamo Y-Bot for all villagers, colour-tagged via the existing `DPMaterials::GetOrCreateColouredVariant` system per archetype.
- Mixamo "gunslinger"-style character for Aelfric (taller, dark robe).
- Kenney.nl "Survival Kit" / "Medieval Kit" for items, doors, chests, forge.
- Engine procedural sky for skybox.
- Existing prototype's torch + directional light setup.
- Tinted cubes for everything else (continuing the prototype's pattern).
- **No audio.** The audio system is engine work that lands post-MVP. Audio emission events fire (the `Zenith_AudioBus::GetEmittedSoundsForTest` instrumentation hook from [TestPlan §0.4](TestPlan.md)) so tests pass, but speakers stay silent.

### 1.4 Tests (TDD-driven)

| Tier | MVP scope |
|---|---|
| Tier 0 (Harness) | All 6 |
| Tier 1 (Foundation) | All ~30 |
| Tier 2 (Depth) | **Subset:** Devout channel, Child half-timer, fog memory, fog-aelfric-not-revealed, possession cooldown, demon-scent accumulation, sprint cost |
| Tier 3 (Content) | **Subset:** archetype-timer parameterised (4 archetypes), reagent inventory (5 reagents), naming-convention linter |
| Tier 4 (Ship) | **MVP only:** `Test_P4Playthrough_Night1WinGolden`, `Test_P4Playthrough_Night1LossByApprehend`, `Test_P4Playthrough_Night1LossByDawn` |

**Total MVP tests:** ~50 (from the ~250-test ship target). Total MVP test runtime: ~10 min batch.

### 1.5 The MVP definition-of-done

**Demo script:** an autonomous test agent (or any human) can run `Tools/run_dp_tests.ps1 -Tier 0,1 -Filter "Mvp" -Headless` and see **100% pass**, plus `Test_P4Playthrough_Night1WinGolden` finishes in under 9000 frames.

Plus: I can `git pull`, build, run, and personally complete the MVP sentence in <5 minutes.

---

## 2. What's Out (post-MVP)

To be explicit. Every item below is **deferred** to a later milestone.

### 2.1 Content deferred

- 20 of 24 villager archetypes.
- 9 of 14 reagents.
- 3 priest variants (Cautious / Cruel / Drunk).
- Hounds (Aram & Tobit).
- Mid-Night escalation (flintlock, bless, exorcise).
- 6 of 7 Nights of the campaign.
- Joan Trew, the Order of the Unstaked Heart, all narrative arc.
- Cutscenes.
- Daily Vexholme.
- Hauntings.
- Endless Vexholme.

### 2.2 Systems deferred

- Liminal hub.
- Knot currency.
- Meta-progression (hermit unlock trees).
- Cross-Night save/load.
- Procedural shuffler (reagent anchors, locked doors, villager set, weather).
- Charms (rowan, salt, St. George medal).
- Distractions (music box, tin whistle, tinder bundle, raw meat).
- Confessional hide-mechanic.
- Well drown verb.
- Replay debugger (tooling).
- Telemetry pipeline.
- Autonomous playtest bot.

### 2.3 Tech deferred

- Audio system (engine work). MVP is silent + emission-event instrumentation only.
- Real volumetric fog shader. MVP runs a cheap "fog hole gather → uniform black fog" pass.
- Animation event system. MVP villagers move via velocity, no anim events.
- Real animation state machines per archetype. MVP uses one shared idle/walk/jog/sprint anim set.
- Localisation. MVP is English-only with strings hardcoded (but reading from a single `Strings.json` for the future loc pipeline).
- Console platform layers (Switch / Xbox / PlayStation).
- Console certification.
- Save corruption recovery.
- Accessibility full pass.
- LLM-driven Aelfric mutters.

### 2.4 Assets deferred

- All S1 and S2 art.
- Final UI (MVP keeps text overlays).
- Final SFX / VO / music (MVP is silent).
- Final particle systems (MVP uses generic placeholder).
- Final fonts.
- Cutscene cinematics.
- Marketing assets.

---

## 3. The MVP Build Definition

When someone asks "what does the MVP look like?" the answer is:

- A single `.exe` for Windows x64 Debug Tools build, in `Games/DevilsPlayground/Build/output/win64/vs2022_debug_win64_true/devilsplayground.exe`.
- Boots to a main menu (text-only).
- Click Play. Loads the GameLevel scene.
- 30-second possession timer. Real priest with apprehend. Real navmesh with door-blocking. Real pause on Esc.
- 5 reagents on the map. 4 archetypes of villagers (Mixamo-rigged, colour-tagged).
- Win = inscribe all 5 reagents at the pentagram.
- Lose = apprehended, or dawn (10-minute internal timer), or no villagers left.
- All ~50 MVP tests green in CI.

That's it. That's the MVP.

---

## 4. The Anti-Goals

To prevent scope creep. The MVP **deliberately** does not include:

1. **Polish.** The MVP is *mechanically complete*, not *fun*. Tuning happens post-MVP with telemetry data from real (test-bot or human) play.
2. **Beauty.** S0 placeholders look ugly. That's the contract.
3. **Sound.** Silent. Audio is the largest engine sub-system; deferring it post-MVP gates engineering on one thing at a time.
4. **Story.** No cutscenes. No narrative arc. The player understands the goal from the HUD alone.
5. **Variety.** One level. One priest. Four archetypes. Anyone who plays the MVP twice will see the same content. That's expected.
6. **Replayability.** No procedural shuffle. No daily seed. No leaderboards.

If any of these creep into MVP-scope discussions during the next 4 months, **redirect**: "post-MVP."

---

## 5. Success Criteria

The MVP is done when **all of these are true simultaneously**:

1. ✅ `Tools/run_dp_tests.ps1 -Headless` reports 100% pass on the MVP test subset.
2. ✅ `Test_P4Playthrough_Night1WinGolden` finishes a complete bot-driven win run in < 150 seconds @ fixed-dt 60 Hz.
3. ✅ `Test_P4Playthrough_Night1LossByApprehend` and `Test_P4Playthrough_Night1LossByDawn` both pass.
4. ✅ A human (the user) can build, run, and personally complete the MVP sentence in under 5 minutes of play.
5. ✅ Every system in §1.1 has at least one passing automated test that exercises it via `Zenith_InputSimulator`.
6. ✅ `Docs/Status.md` reports the build state as "MVP complete, ready for tuning phase."
7. ✅ `Docs/DecisionLog.md` records every non-trivial decision made during the MVP build.

At that point, we open the post-MVP roadmap.
