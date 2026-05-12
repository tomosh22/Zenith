# Devil's Playground — Game Design Document

**Working title:** Devil's Playground
**Status:** v0.1 — shipping vision built on top of the existing skeleton-grade prototype
**Author:** Design (Claude)
**Last updated:** 2026-05-11
**Target platforms:** PC (Steam, primary), Xbox Series X|S, PlayStation 5, Nintendo Switch 2
**Target storefront price:** $19.99 USD at launch (premium one-time purchase)
**Actual dev model:** 1 full-time developer + autonomous Claude Code agents. Zero external spend until art assets begin. **MVP runway: 3.5–4.5 months (optimistic) or 5.5–7 months (if Mixamo spike fails — navmesh-generator risk retired after round-4 finding that `Zenith_NavMeshGenerator` already exists in engine).** Full-game runway: 12–24 months. See [MVPScope.md](MVPScope.md) and [MvpRoadmap.md](MvpRoadmap.md) for the executable plan; see §12 of this doc for the actual production model.

---

## 1. Executive Summary

### 1.1 Elevator pitch

You are a newly-summoned demon, bodiless and weak, abandoned in a 17th-century mountain village by the cult that called you forth. To return to Hell on your own terms you must complete the *Rite of Unmaking* at the pentagram beneath the chapel — but you can only manifest by force-possessing the villagers themselves, and each soul you wear burns away within thirty seconds. A travelling **witch-finder** prowls the village with hounds and lanterns, and he can hear every door you slam.

**Devil's Playground** is a **top-down stealth-puzzle roguelite** about juggling a chain of disposable bodies, racing a punishing burn-out timer, and outmanoeuvring a perceptive AI antagonist across a single procedurally-mixed village every run.

### 1.2 USP — what this game does that no other game does

1. **Possession-chain stealth.** Most stealth games give you one body. Here, *every* NPC is a body. The strategic question isn't "where do I hide?" — it's "which body do I burn next, and how do I leave the last one positioned so I can come back to its spot?"
2. **Two clocks at once.** A long-form objective clock ("collect the five reagents and reach the pentagram") collides with a brutally short possession clock ("this body dies in thirty seconds"). Every action must be priced in *both* currencies.
3. **An antagonist that hears the consequences of your plans.** The witch-finder isn't a tile-mover with a sight cone — he investigates noise, follows hounds toward the demon-scent of recently-possessed villagers, and remembers where he last saw you. Whole runs are spent shaping his attention.
4. **A run takes 30–60 minutes.** Long enough to feel like a story, short enough that a bad opening doesn't sink the night. Roguelite meta-progression converts losses into permanent unlocks.

### 1.3 Target audience

- **Primary (core):** Stealth and tactics fans (*Hitman, Invisible Inc., Shadow Tactics, Mark of the Ninja*) who want a short-session game with the same density of micro-decisions.
- **Secondary (adjacent):** Roguelite players (*Hades, Slay the Spire, Inscryption*) drawn by short runs and meta-unlocks.
- **Tertiary (mood):** Folk-horror, A24 horror, *The Witch (2015)*, *Hereditary*, *Apostle* fans drawn by atmosphere and theme. Marketing assets lean here.

### 1.4 Pillars

These are the four design principles every feature is tested against. If a feature doesn't serve one of these, it gets cut.

1. **Every body is a clock.** Movement, planning, decisions — everything is priced in seconds-of-life and bodies-spent. Idle moments are punished.
2. **The witch-finder is a sense, not a path.** He never "patrols a route." He always reacts to something the player did. Players should be able to point at the screen and say *"that's why he's there."*
3. **No safe haven.** The fog of war is a thread the player constantly re-weaves; possessing a villager near a torch is safer but louder; the chapel is bright but draws the priest. Every advantage costs.
4. **Loss tells you something.** When you die, a single sentence on the death screen names the mistake. ("You possessed Hannah for fifteen seconds with the chapel door open behind you.") Roguelite progression flows from that lesson.

---

## 2. World & Story

### 2.1 Setting

**The village of Vexholme**, a fictionalised composite of 1670s Pennine highland settlements — stone cottages, a slate-roofed chapel, a smithy with a permanent fire, a millrace, a hanging tree on the moor edge. Drowned in fog seven nights of every year. The villagers don't speak of those nights; they hang fresh bundles of rowan above every doorway.

Tonight is the first of seven.

### 2.2 Premise

A cabal of three hermits — **the Order of the Unstaked Heart** — has summoned the player-demon at the standing stones above Vexholme. Their fourth member, a girl named **Joan Trew**, broke her oath at the last moment and ran for the chapel; the summoning collapsed before the demon was given a body. The demon now exists as a roaming presence — visible only as a curl of frost over the body it most recently rode.

To return to Hell on its own terms (and not as a wounded vagrant kicked back through the Veil by the next priest with a relic), the demon must complete the **Rite of Unmaking** itself: gather the five *cursed reagents* the cabal left scattered through the village in the chaos of the summoning, place them on the **pentagram** the cabal carved beneath the chapel's altar stone, and detonate the rite before dawn.

The **witch-finder**, **Pursuivant Aelfric Crowe**, was already on his way to Vexholme to question Joan Trew — he was three days east when the summoning happened, and he felt it through his St. George medal. He arrives at dusk. He brings two hounds named Aram and Tobit, a lantern, a flintlock he hasn't reloaded in years, and the unswerving belief that every villager he meets might already be a body the demon is wearing.

### 2.3 Narrative arc

The game is structured as **Seven Nights** — seven roguelite runs, each in the same village with different procedural arrangement, different objectives, and an escalating sequence of complications. Between nights, the player returns to the **Liminal**, a fog-shrouded crossroads where the demon meets the hermits of the Order. Each hermit unlocks a permanent ability, item type, or modifier.

- **Night 1 — The Summoning.** Tutorial. The pentagram is half-formed. Only two reagents are needed. The witch-finder is delayed by the storm. Aelfric arrives in the final minute. Whether you win or lose, Night 1 ends in cutscene: Joan Trew has been arrested.
- **Night 2 — The Inquest.** Joan is being interrogated in the longhouse. Aelfric is more cautious; new mechanic: villagers can be *blessed* by Aelfric, refusing possession for 60 s.
- **Night 3 — The Hounds.** Aram and Tobit gain a sense-cone of their own. They can be lured with raw meat from the smokehouse.
- **Night 4 — The Bell.** A new villager arrives: a deaf bell-ringer who is the only safe-to-possess body when the chapel bell is being rung.
- **Night 5 — The Pyre.** Joan is to be burned at dawn. Multiple new objectives: free Joan from the longhouse, hide the pyre's flint, then complete the rite.
- **Night 6 — The Reckoning.** Two witch-finders arrive. The fog thins, exposing more of the map by default. Crafting becomes essential.
- **Night 7 — The Door.** Final fixed run. No procedural variation. The pentagram is complete. The chapel is sealed. The cabal has been hanged. You play it alone.

Each Night has a **win** and a **lose** path that *both* progress the story (a lose closes one door of escape; a win closes another). Both endings of all seven Nights interleave to produce one of nine endings to the campaign.

### 2.4 Characters

- **The Demon** (the player). Never named. No portrait. Visible only as frost on its current host. Voiced only by Latin whispers laid under the music when possession is held to its dying second.
- **Pursuivant Aelfric Crowe.** Antagonist. Mid-fifties, gaunt, scar across the bridge of his nose from a bull-baiting in his youth. Career inquisitor. Sane. Soft-spoken. *Always* right that there's a demon in the village; *often* wrong about which body it's in. (Design: never let the player feel he is unfair. He must always be acting on a perceivable cue.)
- **Joan Trew.** Cabal member who broke. Eighteen. Half-shaved head. The player never plays as her in the main campaign — she is a non-possessable villager, marked by a different silhouette and a softer outline. She is **the win condition** in Night 5.
- **The Order of the Unstaked Heart.** Three hermits: **Wynstan** (the maker — unlocks crafting), **Mereworth** (the eye — unlocks the seer-villagers), **Old Bett** (the breath — unlocks the deep-fog night).
- **The villagers.** ~24 distinct archetypes, of which 12–18 appear in any given Night. See §6.2.

---

## 3. Game Pillars in Practice — the 30-second loop

Every player decision in *Devil's Playground* is shaped by these three nested timers:

| Timer | Length | What it gates |
|---|---|---|
| **Possession** | 30 s (modifiable by villager archetype: 20–45 s) | Current host's life. Resets to full on a fresh possession. |
| **Night** | 25–35 min (per run) | The Pursuivant gains a new advantage at the 5/10/15/20 min marks. |
| **Campaign** | 7 nights | Permanent state transitions in the village. |

The smallest loop — the **moment-to-moment loop** — is:

1. **Scan.** Pan the camera. Find the next reagent, the next safe body, the nearest distraction.
2. **Route.** Pick the next possession in the chain. *Where* will I leave this body when its timer runs out?
3. **Burn.** Possess the chosen villager. Sprint. Open a door. Pick up an item. Drop it for the next host.
4. **Observe.** Did the witch-finder hear you? Where are his hounds? Did Aelfric just enter a building you need?
5. **Switch.** Hand off the item to the next body. Repeat.

The whole game lives inside this loop. Every system below exists to make that loop richer, harder, or more legible.

---

## 4. Core Mechanics

### 4.1 Possession

**Selection.** Cursor-driven. Left-click any villager within line-of-sight of the demon's current "frost-anchor" (initially the standing stones; thereafter the last-possessed body's death location). A second click cancels.

**Range.** Possession range is **15 metres** from the anchor. Outside that range the villager is greyed out. The anchor moves to wherever the previous body died. **Strategic consequence:** if you burn a body in the middle of an open field, your next anchor is in the middle of an open field.

**Cost.** Free. The cost is the timer.

**Resistance.** Most villagers possess instantly. Three subtypes resist:
- **The Devout** (clutches a rosary) — a 0.8 s possession channel. Interrupted by Aelfric line-of-sight.
- **The Children** — possession works but life timer is halved (15 s).
- **Joan Trew & the Pursuivant himself** — never possessable.

**Burn-out.** Life timer counts to zero → the body collapses → the demon's frost-anchor lands where the body fell. The death is visible from anywhere (a thin grey vapour rises 8 m). Aelfric's perception system gets a free "investigate" stimulus from any death within his hearing range (30 m). **This is the strongest "you-must-plan-ahead" pressure in the game.**

**Possession-switch.** Releasing the current body before its timer runs out (right-click) does **not** trigger the death-stimulus — the body simply faints, takes 10 s to recover, then walks back to its idle spot. Aelfric still investigates if he sees it faint within 20 m.

**The Possession Cooldown.** New for shipping: after every voluntary switch, a 1.5 s "untethered" gap during which the demon cannot possess. During this gap a translucent frost-trail is visible to Aelfric within 25 m. *Reason: prevents the player from machine-gun-clicking villagers to skip the timer mechanic entirely.*

### 4.2 Movement & Camera

**Movement.** WASD camera-relative on PC, left analog stick on console. Default **8 m/s** (a brisk jog). Three speed states:
- **Walk** (4 m/s) — held Ctrl / L1. Quiet. Aelfric's hearing range halves against it.
- **Jog** (8 m/s) — default.
- **Sprint** (12 m/s) — held Shift / R1. Costs **3 s/sec** life-timer drain. The body literally burns faster.

Sprint is the closest the game gets to a "panic button." It exists to make recovery from a bad plan *possible* — not *cheap*.

**Camera.** Top-down orbit, locked pitch (configurable in accessibility — see §10), zoomable 30–80 m on PC, 35–60 m on console. Q/E rotate orbit. **Mouse wheel zooms**; on controller, right-stick clicks rotate, right-stick up/down zooms. Camera **does not auto-follow** the possessed body — players can scan ahead while the body keeps walking.

**Optional follow mode.** Accessibility toggle. Defaults off because tactical scans depend on the camera being free.

### 4.3 Items, Inventory & Reagents

**Inventory is one slot per body.** The body holds one item. Switching bodies switches inventories. To carry two items, possess two bodies — and burn them in the right order.

**Item categories:**

| Category | Tags | Purpose |
|---|---|---|
| **Reagents** | Five distinct per Night (procedurally drawn from a pool of ~14 by Night 7) | Objectives. Must be placed on the pentagram. |
| **Tools** | Iron, Wood, Lantern, Bell-Wax | Smithy/forge inputs and ad-hoc utilities. |
| **Keys** | Brass Key, Skeleton Key | Brass = single-use. Skeleton = reusable, found only on Aelfric or in late-Night chests. |
| **Distractions** | Wind-Up Music Box, Tin Whistle, Tinder-Bundle | Sound or fire stimuli for the priest. |
| **Charms** | Rowan Sprig, St. George Medal, Salt Pouch | Negate one demon-detection event each. Single-use. |

**Dropping.** Tap G / square to drop on the ground. Item stays. Visible to Aelfric only at <5 m or when carried.

**Hand-off chains.** The core puzzle pattern of the game. A villager picks up a reagent in the smithy, walks to the millrace, drops it on the ground. Their timer expires. You possess a villager already at the millrace, pick up the reagent, walk it to the pentagram. Mastering chains is *the* skill curve.

### 4.4 Interactables

**Universal interact:** F on PC, X / Square on console. Within 2 m of a flagged target, the verb appears as an on-screen prompt with the time-cost suffixed.

| Interactable | Verb | Cost | Notes |
|---|---|---|---|
| **Door, single** | Open / Close | 0.4 s | Opening a door audible to Aelfric at 12 m. Closing at 8 m. Locked → consumes key from inventory. |
| **Door, double** | Open / Close | 0.5 s | Same; louder (15 m hearing range). |
| **Chest** | Open | 0.8 s | Reveals 1 procedurally-rolled item. Loud (20 m). |
| **Forge** | Use | 2.0 s | Iron + Fire-source = Brass Key. (Hammer audible at 30 m. The smithy is the loudest place in the game.) |
| **Bell-rope** | Pull | 0.5 s | Rings the chapel bell. Audible from the entire map. Aelfric *forced* to investigate. |
| **Hearth-fire** | Smother | 1.5 s | Removes a major light source. Carving a hole in Aelfric's lantern-perception map. |
| **Lantern (held)** | Drop / Pick-up | 0.2 s | Light source attached to a villager. Becomes free-floating when dropped. |
| **Confessional** | Hide | 1.0 s | Possessed body sits inside. Aelfric cannot detect possession through walls. Burns life-timer at 0.5×. |
| **Well** | Drown self | 2.0 s | Trades 0 seconds of remaining life for 0 noise on death. Use to extinguish a body's frost-anchor in an inconvenient place. |
| **Pentagram** | Inscribe reagent | 1.5 s | The win action. |
| **Distraction prop** | Trigger | 0.3 s | Music box plays for 8 s; tin whistle for 2 s; tinder-bundle ignites for 30 s. |

**Forge crafting.** Currently single-recipe Iron→Key. Expand to a six-recipe table (Iron+Wood = Spike; Iron+Brass Key = Skeleton Key; etc.). All recipes are loud — the forge is a high-risk, high-reward hub.

### 4.5 The Witch-Finder (Pursuivant Aelfric Crowe)

**Design imperative:** Aelfric must be *interpretable*. Every action of his must trace back to a stimulus the player can see in the world. He is the antagonist; he is also the level designer's most expressive instrument.

**Architecture.**

- **Sensors.** Sight, hearing, hound-track, demon-scent.
- **Memory.** Last-known-positions: of the demon's frost-trail (decays over 30 s), of suspicious bodies (the last three villagers who entered the chapel), of fresh deaths.
- **Behavior tree.** Selector → [Apprehend, Pursue, Investigate, Search, Patrol].
- **Speed.** 4 m/s walking, 7 m/s pursuing. Slower than a possessed villager — but he never tires.

**Sight.** 25 m, 110° FOV, 130° peripheral. Line-of-sight strict. Eye-height 1.6 m (matches the prototype).

**Hearing.** 30 m base, doubled in fog (sound carries further when sight is short). Threshold low — even closing a door qualifies. Each stimulus tagged with loudness; he prioritises by max-loudness, then proximity.

**Hound-track.** New for shipping. The two hounds (Aram, Tobit) wander within 15 m of Aelfric. If they get within 6 m of a body whose **demon-scent** is above 0.5, they bark — a stimulus that registers to Aelfric *and* to the player (loud, distinct audio cue).

**Demon-scent.** A per-villager value (0.0–1.0). +0.3 per possession event on a given body. Decays at 0.05/sec. A body that's been possessed three times in a row is a beacon. **This is the game's most important new system relative to the prototype.** It penalises optimal-play (chain-possess the same nearby villager repeatedly) and rewards exploration of the body pool.

**Apprehend.** New in shipping. If Aelfric reaches an actively-possessed body, he restrains it (3.0 s channel). Successful apprehension = run loss. The body the demon was wearing is permanently removed from the village (executed at dawn). **This adds a hard fail-state the prototype lacks.**

**Aelfric tells.** The player sees:
- His **lantern cone** (a visible volumetric cone).
- An **alert state icon** above his head (calm / suspicious / pursuing / apprehending).
- His **last-investigated waypoint** marked with a faint X on the ground (decays in 8 s).
- A **whisper readout** at the bottom of the screen describing his last decision ("He heard the chapel door close.") — accessibility-on by default, can be turned off for higher difficulty.

### 4.6 Fog of War

**System.** Per-frame clear-and-rebuild (matches the prototype). Visibility radii are gathered from:
- **Possessed body:** 8 m (was 3 m in prototype — increased for legibility).
- **Other villagers:** 1.5 m circle around each (so the player always knows where the bodies are, but not what they're doing).
- **Lights:** torch / lantern / hearth, sized by their falloff.
- **Aelfric & hounds:** 0 m. The player **never** has fog reveal from the priest's position. He must be located by visual edge — when his lantern cone slices into a foggy area, his silhouette and cone are revealed; otherwise his last seen position decays after 5 s.

**Memory fog.** Tiles seen in the last 10 s are clearly visible; tiles seen 10–30 s ago are dimmed; tiles never seen this session are full grey. (Standard fog-of-war approach borrowed from *Invisible Inc.*)

**The Chapel.** Lit by 30+ candles. The brightest building. Also the densest acoustic environment (echo-doubled hearing for Aelfric). Designed as the climactic tension space.

### 4.7 Possession-burn telegraphs

Visual telegraphs the player needs at a glance:

- **Body has 30 s — 15 s left:** thin frost outline (white).
- **15 s — 5 s left:** outline pulses bluer; body's breath plumes in mid-air.
- **<5 s:** body's eyes flicker. Walk speed drops 25%.
- **At death:** body collapses; frost spreads 1.5 m on the ground; vapour rises 8 m for 2 s. The vapour is the **next possession's anchor visible to Aelfric**.

---

## 5. Run Structure

### 5.1 A single Night

1. **Opening cutscene** (5–10 s) — the demon awakens in the standing stones. Aelfric's lantern is visible on the eastern moor edge.
2. **Onboarding** (first 60 s of every Night, mutable) — a single villager walks past the stones, the player is invited to click them.
3. **The Hunt** (20–40 min).
4. **The Closing** (1–3 min) — when the last reagent is placed, dawn breaks over the chapel; the demon ascends, the run ends.

### 5.2 Win / Loss states

**Win:** Inscribe all five reagents at the pentagram before dawn.
**Loss A:** Run out of villagers (every villager has been killed by burn-out, executed at dawn, or apprehended). The pool starts at 14–18 per Night.
**Loss B:** Apprehended by Aelfric while possessed.
**Loss C:** Dawn breaks before the rite completes (hard time limit per Night — visible as a sun-rise gauge in top-left).

### 5.3 Difficulty curve

The Pursuivant gains modifiers at fixed in-Night times:

| Time | Modifier |
|---|---|
| 0:00 | Lantern only. |
| 5:00 | Hounds released. |
| 10:00 | Aelfric draws his flintlock. Range threat: 15 m, 1 shot, 4 s reload. |
| 15:00 | Aelfric blesses one villager per minute (becomes un-possessable for 60 s). |
| 20:00 | Aelfric performs an exorcism ritual at the pentagram, "decontaminating" it for 30 s. Any reagent inscribed during this window is lost. |

Players who race to the early reagents avoid the worst modifiers but skip the safer mid-Night opportunities. Players who plan carefully arrive at the pentagram around the 18-minute mark. **Designed tension at minute 15–20.**

### 5.4 Meta-progression

Between Nights, the player visits the **Liminal** — a fog-bound space where the three hermits offer permanent unlocks paid for in **Knots** (the run currency, earned 1 per reagent inscribed + bonus for unbroken hand-off chains + bonus for hounds neutralised).

**Tracks:**

- **Wynstan's Forge** (crafting). Unlocks: new recipes, faster crafting, +1 inventory slot for crafted items only.
- **Mereworth's Eye** (perception). Unlocks: seer-villagers (reveal Aelfric's last-known-position for 10 s on possession), longer fog memory, lantern silhouette filter.
- **Old Bett's Breath** (movement). Unlocks: stoneborn-villagers (45 s timer), 30% sprint efficiency, possession-cooldown reduced from 1.5 s to 1.0 s.

**Cosmetic unlocks** (no gameplay effect): demon visual variants, frost colours, villager hat sets, Aelfric's coat colours. All free with play; no monetisation.

### 5.5 Procedural elements (the Night-to-Night variance)

To prevent a single run from feeling identical to the next while still allowing players to learn the village:

- **Reagent locations.** Five of 25 candidate spawn anchors selected per Night.
- **Locked-door set.** Two of seven major buildings randomly locked, requiring keys or alternate routes.
- **Villager set.** 14–18 of 24 archetypes drawn. Some archetypes (the blacksmith, the chapel-sexton) are guaranteed.
- **Aelfric variant.** Three sub-personalities (Aelfric the Cautious, Aelfric the Cruel, Aelfric the Drunk) shift his prioritisation. Drawn once per Night.
- **Weather.** Three states: clear (low fog, normal stealth), mist (high fog, deeper memory-fade), rain (loud — masks footsteps but reveals breath in the air at 15 m).

The map **layout itself does not procgen** — Vexholme is hand-authored. We want players to internalise its geography. Re-shuffling props makes each Night feel new without sabotaging mastery.

---

## 6. Content

### 6.1 Setting: Vexholme village (1 map, hand-authored)

Hand-authored single map, ~200×200 m playable area (double the prototype's 100×100). Six "districts," each tonally distinct:

1. **The Standing Stones.** Demon's starting anchor. Wind, cairns. Sparse villager spawns.
2. **The Yards & Cottages.** Civilian heart. Densest villager spawns. Most hand-off opportunities.
3. **The Smithy & Mill.** Loud district. Forge + millrace = perpetual stimulus saturation; the priest can't pinpoint specific sounds here.
4. **The Chapel & Graveyard.** Bright, candle-saturated, climactic. Bell-tower is the loudest single feature on the map.
5. **The Longhouse.** Aelfric's base. Joan Trew is imprisoned here from Night 2 on.
6. **The Moor Edge.** Hanging tree. Foggiest. Lowest villager density. The hounds patrol here.

### 6.2 Villager archetypes (24 designed; 14–18 per Night)

Selected highlights — each unique combination of **timer**, **movement**, and **ability**:

| Archetype | Timer | Notes / Ability |
|---|---|---|
| Farmhand | 30 s | Baseline. Many spawn. |
| Carter | 30 s | +25% walk speed. |
| Smith's Apprentice | 30 s | Free forge crafting (no time cost). |
| Devout | 30 s | 0.8 s possession channel. Aelfric trusts him — never interrogated. |
| Beggar | 25 s | Sleeps on the ground when un-possessed. Aelfric won't search beggars. |
| Sexton | 30 s | Can enter the chapel without being seen. |
| Child | 15 s | Half timer. But: invisible to hounds. Cannot carry tools (only reagents). |
| Old Bett-touched villager | 45 s | Long timer. Slow (4 m/s). |
| Mereworth-touched seer | 30 s | On possession, reveal Aelfric's last position for 10 s. |
| Stonemason | 30 s | Can climb low walls. Off-piste routes. |
| Drunk | 30 s | Wanders erratically when un-possessed. Aelfric ignores drunks. |
| Bell-Ringer (deaf) | 30 s | Immune to bell-ring panic effect. |
| Joan Trew | — | Non-possessable. Win-condition in Night 5. |

Each archetype also has a **demon-scent floor** (resistance to becoming a beacon — Devout = high floor, Child = no floor at all). The full table is in the design appendix (not in this doc).

### 6.3 Reagents (14 designed; 5 drawn per Night)

Each reagent is a small, tactile prop with a hand-painted icon. Examples:

- **The Caul.** Found in the smithy's smoke-loft. Wrapped in linen.
- **A Hare's Tongue.** Found in the chapel's offertory bowl.
- **Three Drops of Bog-Water.** Must be carried in a sealed phial; if dropped, evaporates in 8 s.
- **The Bell's Soul.** A struck bell-clapper. Picking it up rings the bell.
- **A Burial-Coin.** Found on Joan Trew's pillow in the longhouse. Highest-difficulty draw.

Each reagent has at most one valid spawn district, picked from its candidate set. The pickup interaction is a 1.0 s channel — slower than a regular item, telegraphing the reagent's importance.

### 6.4 Witch-finder variants

- **Aelfric the Cautious.** Always investigates. Slow to pursue. Long memory.
- **Aelfric the Cruel.** Apprehends on suspicion alone. Apprehension channel is 1.5 s, not 3.0.
- **Aelfric the Drunk.** Skips every fourth stimulus. Hounds compensate (extra-aggressive).

Each variant is signposted in the Night-start menu so the player can plan.

---

## 7. UI / UX

### 7.1 HUD

The current prototype's HUD (life bar, held-item readout, objective counter, status banner) is the right *scaffold* — but rebuild as art-direction-matched parchment overlays, not ASCII.

- **Top-left:** Sun gauge (dawn timer). Subtle gradient from full moon to first light.
- **Top-right:** Reagents collected (5 wax-sealed circles; fills to red wax when inscribed).
- **Bottom-left:** Current body life (a candle that burns down, not a bar; flame colour shifts green→yellow→blue as it dies). Body name underneath in handwritten serif. **Demon-scent indicator** as a fine soot stain spreading on the candle's base.
- **Bottom-centre:** Held item glyph + name.
- **Bottom-right:** Aelfric awareness icon. Calm = ink-quill. Suspicious = single eye opened. Pursuing = eye + hand reaching. Apprehending = eye + manacle.
- **Whisper line** (configurable): a single italic sentence telling the player what Aelfric most recently did.

### 7.2 Menus

- **Main menu.** A single image: the demon's frost over the standing stones at twilight. Three options: Begin a Night / Liminal / Settings.
- **Liminal.** The meta-progression hub. Three hermits visible. Each is a clickable shrine with their unlocks.
- **Pause.** Pauses time. Esc / Start. **Critically, this must actually pause** (the prototype shows the overlay but doesn't stop simulation). Pause-during-input-decision is a stealth-game expectation.

### 7.3 Tutorialisation

No discrete tutorial mode. Night 1 is the tutorial — every mechanic is introduced in a scripted moment within the first 6 minutes, in the same village the player will play for 30 hours.

Each scripted moment has:
- A **single-line hint** in the whisper area, dismissible.
- A **gentle pause** of the world's time clock during the hint's introduction.
- An **observable visual** the hint references.

The tutorial never *takes control* away from the player. (The prototype's `EditorAutomation` system can be repurposed to drive tutorial NPCs, but the player remains in command.)

---

## 8. Art Direction

### 8.1 Reference

**Visual:** *Inside* (Playdead) for silhouette discipline. *Darkwood* for top-down dread. *Carrion* for the contrast between dark interiors and warm point-lights. *The Witch (2015)* for colour palette: bone, fog, slate, and a single red.

**The single red:** the demon's frost outline pulses a deep arterial red as the timer dies. The pentagram is red. Aelfric's coat lining is red. Otherwise, the palette is washed out. Players' eyes should always know where to land.

### 8.2 Style

Stylised painterly 3D, top-down. **Not** photorealistic — but not chunky-cute either. Closer to *Manor Lords* than *Banished*. Slate, brick, linen, soot. Volumetric lantern light with strong bloom. Volumetric fog with a low ceiling (~5 m) to keep the top-down camera honest.

### 8.3 Animation

- **Villagers:** ~12 base animations × 24 archetypes × variations = ~150 distinct anim states. Idle, walk, jog, sprint, possessed-walk (subtle stagger), faint, collapse, carry.
- **Aelfric:** Bespoke. 30+ animations. Lantern-sweep, kneel-to-investigate, raise-flintlock, restrain.
- **Hounds:** 20+ animations. Sniff, bark, lunge, lie-down.
- **Demon possession transition:** A signature visual. ~0.4 s. Body briefly silvered; frost spirals up from the ground; eyes go white-blue. Plays on every possession — needs to never get old. (See *Hades*' boon-pickup animation for the benchmark.)

### 8.4 Audio

**Music.** Pärt-adjacent choral textures with low drones; sparingly tonal. Never melodic during gameplay. Two pieces escalate based on Aelfric's alert state. (Reference: Disasterpeace's *Hyper Light Drifter*, Mica Levi's *Under the Skin*.)

**SFX.** The acoustic *signature* of each interactable is critical. The chapel door is a different sound than the longhouse door; the player must learn these so they can identify which door Aelfric just heard.

**VO.** No villager dialogue (they're not characters; they're vessels). Aelfric has ~20 lines, delivered as soft mutterings the player overhears at <8 m: "Cold here…", "Lord show me the body…", "Aram, here, boy."

The **demon never speaks**.

---

## 9. Modes

### 9.1 Campaign (primary)

Seven Nights. ~6–10 hours main path. Branching ending state. Designed for 1 full playthrough.

### 9.2 Daily Vexholme (post-launch)

A single seeded Night, same for every player worldwide, refreshed at 00:00 UTC. Leaderboards: fewest deaths, fastest time, perfect chains. Roguelite staple.

### 9.3 Hauntings (post-launch DLC, free)

Single-Night challenges with hand-crafted constraints. "The Mute Night" — every villager has the deaf bell-ringer's silence. "The Long Sabbat" — 60-minute timer, 10 reagents needed. Drip-fed monthly for 6 months post-launch.

### 9.4 Endless Vexholme

Unlocked after campaign. Infinite Nights, each escalating. Score-attack. Replaces the historical narrative beats with abstract modifiers.

---

## 10. Accessibility

Non-negotiable. Tested against AbleGamers and CVAA from week one.

- **Camera tilt:** Adjustable from 0° (pure top-down) to 80° (oblique). Default 60°.
- **Camera auto-follow:** Toggleable.
- **Colorblind modes:** Three (Deuteranopia, Protanopia, Tritanopia). The single-red palette is replaced with a single-cyan or single-yellow per mode.
- **Hold vs. Toggle:** Every held action (Sprint, Walk-quiet, Possess-channel) has a toggle option.
- **Whisper line:** Always on by default. Names every Aelfric decision in plain English.
- **Time scale:** Adjustable from 0.6× to 1.0×. Achievements still available below 1.0×.
- **High-contrast outline mode:** Adds chunky outlines to all interactables and bodies.
- **Single-stick play:** Designed for one-handed controller use. Camera follow-locks, possession is auto-selected.
- **Subtitles:** Aelfric's mutters and the choral whispers are subtitle-captioned with directional indicators.

---

## 11. Monetisation

**One-time premium purchase. No microtransactions. No live-service.**

Post-launch:
- **Free DLC** (Hauntings, Daily seed, leaderboards) for 6 months.
- **One paid expansion** (~$9.99, ~6 months post-launch) — *The Eighth Night.* New map, new antagonist, new villager archetypes, new ending state. Optional.
- **No cosmetic store. No battle pass. No currency.**

The pitch to publishers: this is a *premium-priced, replayable, finite-content* game. The replay value lives in the procedural Nightly and the Daily. We are not in the live-service competition; we are in the "*Hades* / *Inscryption* / *Outer Wilds*" competition.

---

## 12. Production scope (actual)

**This GDD describes a game that traditionally requires ~12 people and 18 months at ~$3M. The actual production is one full-time developer working with autonomous Claude Code agents, with zero external spend until art assets begin. The numbers below reflect that reality.**

The 12-person table that appeared in earlier drafts is preserved at the end of this section as "traditional sizing" — informational context, **not** a planning anchor. Compare your pace against the MVP roadmap's task-level estimates, not against a team schedule.

### 12.1 Actual production model

| Role | Who | Notes |
|---|---|---|
| Producer / designer | User (Tomos) | Full-time. Reviews PRs from agents. Owns scope decisions and tuning judgement calls. |
| Engineering | Autonomous Claude Code orchestrator + subagents | Test-first, auto-merge on green CI. See [OrchestratorPlaybook.md](OrchestratorPlaybook.md). |
| Art (S0 / S1) | Mixamo + Kenney + Sketchfab CC0 + in-house authored low-poly | No external spend. See [AssetManifest.md](AssetManifest.md). |
| Art (S2 final) | Deferred until MVP ships; first paid art relationship at that point | User decides who/when. |
| Audio | None during MVP (silent + emission events). Composer/VO/mix all post-MVP. | First paid audio relationship after MVP playable validates the design. |
| QA | Automated test suite (~250 tests at ship); user as final playtest authority | See [TestPlan.md](TestPlan.md). |
| Cert / platforms | Post-MVP only. MVP is PC (Windows) only. | Console layers + cert are engine-team work outside this project. |

### 12.2 MVP runway (the only schedule that matters now)

| Phase | Target | Scope |
|---|---|---|
| Phase 0.0 Bootstrap | 1 week | CI, runner flags, build env verify, smoke PR. See [MvpRoadmap.md](MvpRoadmap.md) Phase 0.0. |
| Phase 0.1 Plumbing | 2 weeks | Tuning, archetype, instrumentation hooks. |
| Phase 1 Foundation gameplay | **4–6 weeks** | Pause, navmesh (now ~3-5 days — the engine generator already exists; see round-4 finding), apprehend, switch, drop, cooldown, scent, sprint, range, omniscience removal, save. **Round-4 peer review found `Zenith_NavMeshGenerator` is a fully-implemented Recast-style pipeline in the engine with unit tests.** MVP-1.2 is integration, not authoring. Original 6-12 week navmesh estimate retired. |
| Phase 2 Depth | 4 weeks | Archetypes, reagents, forge, fog, HUD upgrades. |
| Phase 3 Asset substitution | 3 weeks | Mixamo villagers, Kenney items. Blocked on MVP-3.0.1 spike. |
| Phase 4 MVP acceptance | 2 weeks | The 4 playthrough tests. |

**Honest MVP runway (re-estimated 2026-05-12 round-4 after `Zenith_NavMeshGenerator` discovery):**
- **Optimistic path: 14–18 weeks (3.5–4.5 months).** Navmesh generator integrates cleanly; Mixamo spike works first try.
- **Pessimistic path: 22–28 weeks (5.5–7 months).** Navmesh-generator integration uncovers DP-specific collider issues requiring engine fixes; Mixamo retargeting fails and tinted-cubes ship through MVP.
- The earlier "7-9 month worst case" framing was anchored on writing a navmesh generator from scratch. With the generator existing in-engine, that scenario is retired.

### 12.3 Traditional team sizing (informational; not the plan)

If a publisher funded this project at industry-standard staffing, the table below shows what shape that would take. It is **not** the user's current plan; it exists here to give context if the user ever pitches the project externally or scales the team post-MVP.

| Discipline | Traditional headcount | What they'd do |
|---|---|---|
| Design | 2 | Lock designs, tune, balance |
| Engineering (gameplay) | 3 | Features, AI, content tooling |
| Engineering (engine) | 1 | Navmesh, fog, performance, ports |
| Art (env) | 2 | Vexholme blockout → final → polish |
| Art (char) | 2 | 24 villagers + Aelfric + hounds + animations |
| Audio | 1 + composer | SFX bible, music dev, recording, mix |
| QA | 1 → 3 by ship | Smoke, Nightly, cert |

Traditional total: ~12 people, 18 months, ~$3M ex-marketing. **This is not the actual plan.** Solo-dev-plus-AI is the actual plan; the MVP-roadmap task estimates are the authoritative time budgets.

---

## 13. Risk register (top three)

1. **Aelfric is the entire game's antagonist.** If players don't find him *fair*, the game fails. Mitigation: every decision he makes must be reflected in the whisper line for the first three Nights, and decisions are auditable via a "replay last 10 seconds" debug tool that ships disabled but is the dev team's primary tuning instrument.
2. **Run length variance.** Players who can't crack the puzzle may sit in a Night for an hour. The 25-minute dawn cap is therefore *load-bearing* — it must feel like a finale, not a punishment.
3. **Possession-chain confusion.** Hand-off chains require the player to remember where a body is. Mitigation: the **frost-anchor indicator** is always visible on the minimap; the last three possession sites are marked with fading red dots.

---

## 14. Appendices (referenced, not included here)

- **A.** Complete villager archetype table (timers, abilities, demon-scent floors).
- **B.** Reagent spawn matrix.
- **C.** Witch-finder behaviour-tree spec.
- **D.** Audio bible (SFX-per-interactable).
- **E.** Localisation plan (8 languages at launch).
- **F.** Console-port compliance checklist.

---

*The current prototype implements roughly 25% of the systems described in this document. See [Shortfalls.md](Shortfalls.md) for a gap analysis.*
