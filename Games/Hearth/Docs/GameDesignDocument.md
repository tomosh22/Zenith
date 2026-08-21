# Hearth — Game Design Document

| | |
|---|---|
| Working title | **Hearth** |
| Genre | Life simulation ("dollhouse sim") |
| Platforms | **Android (primary, landscape)** · Windows (secondary + development) |
| Business model | Free-to-play + **cosmetic-only** IAP · fully offline |
| Engine | Zenith (C++20, Flux renderer, custom ECS) |
| Status | v0.1 — design draft, 2026-08-21 |
| Companion doc | [TDD.md](TDD.md) — architecture, engine gap analysis, roadmap |

---

## 1. Vision & pillars

**Hearth is a dollhouse in your pocket: a little household that plays itself until you reach in.**

You look down into a home you built, at people you made, who are getting on with their lives — cooking badly, missing the bus, falling for the neighbor. Every intervention is one thumb gesture. Every session leaves the house a little more yours.

**Pillars** (every feature must serve at least one; anything that violates one is cut):

1. **Believable little lives.** Sims act sensibly on their own. Comedy and drama come from traits, needs, and circumstance colliding — never from scripted events. Watching is a valid way to play.
2. **Effortless meddling.** The whole live game is playable with one thumb: tap to direct, pinch to see, long-press to ask. No gesture requires precision; no menu is more than two taps deep.
3. **Creative ownership.** The house and household are the player's work. Build mode and Create-a-Sim are toys with instant feedback, not chores with forms.
4. **Respect the phone.** A 3-minute session accomplishes something. The game suspends and resumes losslessly, never demands attention with timers or notifications-bait, never shows an ad, and doesn't cook the battery.

**Tone:** warm, gently comic, cozy-domestic. Stylized low-poly with strong tint-driven palettes (see §15). The name is the theme: home fires, small comforts.

## 2. Audience & player fantasy

- **Primary:** cozy/simulation players on phones (Sims Mobile/FreePlay refugees tired of energy timers; Stardew/Animal Crossing players who want people-simulation) — sessions 3–20 minutes, mostly evenings.
- **Secondary:** PC life-sim players who want an honest, offline, buy-nothing-required Sims.
- **Fantasy:** *"I run this little world."* Competence and care, not challenge. The game never punishes absence — it pauses when you leave.

## 3. Core loops

**Minute loop — notice → direct → watch.**
A need bar dips or a thought bubble pops → tap the sim (or the fridge) → pie menu → queue an action → watch it play out, or pan away and trust autonomy. Payoff: need restored, skill ticks, a small story beat.

**Session loop (10–20 min) — run a day.**
One sim day is 24 real minutes at speed 1 (shorter with speed control). A typical session: morning routine → send to work (rabbit-hole) → speed up → evening skill or social time → spend the paycheck on one home improvement. Payoff: visible progression (skill level, promotion progress, a new object in the house).

**Meta loop (days → weeks) — build a life.**
Skills → promotions → funds → house expansion → deeper relationships → household milestones (move-in, partner, new household member). Long-term expression: the house itself, screenshotted and shown off.

The three loops nest cleanly: nothing in the meta loop requires grinding the minute loop — time-at-speed-3 plus occasional direction is always a valid strategy (Pillar 1).

## 4. Sims: needs, mood, traits, skills, careers

### 4.1 Needs — eight axes, two kinds

**Six decaying motives** drain over sim time and are restored by interactions:

| Motive | Decays over | Restored by | Crisis state (comedic, not lethal) |
|---|---|---|---|
| Hunger | ~16 sim-h | eating (quality × cooking skill) | fridge raid: sim autonomously eats anything, mood tanks |
| Energy | ~18 sim-h awake | sleep (bed quality), naps | collapses asleep where they stand |
| Bladder | ~8 sim-h | toilet | wets self → Hygiene crash + embarrassment moodlet |
| Hygiene | ~24 sim-h (faster after exercise) | shower/bath/sink | stink lines; other sims react, Social interactions degrade |
| Fun | ~14 sim-h of chores | TV, books, hobby objects, play | sulking; refuses low-Fun work interactions |
| Social | ~20 sim-h alone | conversations, calls | talks to self / to appliances (bark comedy) |

**Two ambient scores** don't decay — they are read from context and feed Mood directly:

- **Comfort** — what the sim is currently sitting/lying on (object stat).
- **Environment** — room score: light, decor value of objects, mess (dirty dishes, puddles), clutter.

### 4.2 Mood

Mood = weighted blend of the six motives + Comfort + Environment + active **moodlets** (timed modifiers from events: *Promoted!* +3 for a sim-day, *Wet Self* −2 for 4 sim-h). Mood is shown as the sim portrait's color halo (5 bands: Miserable → Glum → Fine → Happy → Beaming).

Mood is mechanical, not cosmetic:
- Gates interaction quality (skill XP multiplier ×0.5–×1.5).
- Gates career performance on departure (§4.5).
- At *Beaming*, sims occasionally perform trait-flavored **idle flourishes** (free content: a Bookworm reads standing up, a Glutton sneaks a snack) — the "alive dollhouse" texture.

### 4.3 Traits

Pick **2 at creation** (MVP pool of 8; v1.0: 12; v2.0: 20). A trait is three hooks: need-decay modifiers, autonomy-scoring biases, and a signature idle/bark set.

MVP 8: **Glutton** (Hunger decays fast, cooking Fun bonus) · **Early Bird** / **Night Owl** (Energy curve phase shift, mood bonus in their hours) · **Social Butterfly** (Social decays fast, group bonus) · **Loner** (Social decays slow, crowd penalty) · **Neat** (autonomously cleans, mess Environment penalty ×2) · **Slob** (immune to mess, hygiene slow) · **Bookworm** (reading Fun ×2, skill-from-books bonus).

### 4.4 Skills

Five skills, levels 1–10, radial-fill progression visible over the sim's head while training:

**Cooking** (meal quality, recipe unlocks) · **Fitness** (workout Fun, Energy ceiling) · **Charisma** (social success rates, career) · **Creativity** (easel/instrument objects, mood aura) · **Handiness** (repair broken objects, upgrade objects at v1.0).

Level-ups are celebrated (stinger + toast) and unlock interactions — the primary session-loop payoff.

### 4.5 Careers

Rabbit-hole model: the sim leaves the lot at shift start (despawns to a record), returns with pay. **Daily performance = mood at departure + relevant skill level**; performance fills a promotion meter. 5 levels per track: pay raise + one-time gift object per promotion.

MVP: **Culinary** (feeds Cooking). v1.0 adds **Business** (Charisma) and **Athlete** (Fitness). Shifts are sim-time (e.g., 9:00–15:00) so speed control works through them.

### 4.6 Aspirations (light)

Short goal chains ("Master the Stove": cook 5 meals → reach Cooking 3 → cook a feast) rewarding simoleons + an heirloom object. MVP ships 4 chains; they are the soft tutorial.

## 5. Autonomy vs. direction

- **Idle sims choose for themselves**: the highest-scoring advertised interaction nearby (see TDD §4 for the scoring model), with commitment inertia so they finish what they start instead of ping-ponging.
- **The player's queue always outranks autonomy.** Queued actions display as chips (§12); tap a chip to cancel.
- **Crisis override**: a critical motive (flashing red) interrupts autonomy and most queued actions — sims protect themselves (Pillar 1); the player can still cancel the override explicitly.
- **Free-will slider per sim**: *Full* (default) / *Low* (only crises) / *Off* (statue mode, for players who want total control).
- **Failure is comedy, not fail-state**: v1 sims cannot die. Crisis states (§4.1) are embarrassing, mood-costly, and screenshot-worthy. (Death/aging: deliberately cut, §16.)

## 6. Social & relationships

- Every pair of acquainted sims carries two axes: **Friendship** (−100…+100) and **Romance** (0…100).
- **Social interactions advertise on sims** the way object interactions advertise on objects — Chat, Compliment, Joke, Deep Talk, Flirt, Kiss, Argue (MVP set of 8; v1.0: 16). Success odds scale with Charisma, mood, and current axis values; failures (rejected flirt) apply moodlets both ways.
- Relationship levels gate interactions (Flirt unlocks above Friendship 20; Kiss above Romance 40) and unlock milestones: **Friend → Good Friend → Partner → Move-in** (v1.0).
- **Visitors** (P2): neighbor sims knock during social hours; greeting them starts a visit window. Household cap 6 sims (8 at v2.0).
- Group conversations (3+ sims sharing one social) are v2.0; MVP socials are pairwise.

## 7. Build & Buy

One mode, two tabs, entered from the HUD hammer button. The lot is a **cell grid; walls live on cell edges** (rooms are exactly the cells they enclose — see TDD §5 for the model).

**Build tab:**
- **Walls**: drag along grid edges; live length label; rooms auto-detect and shade on completion. Paint per wall side.
- **Floors**: paint per cell (drag to fill; double-tap a room to flood-fill it).
- **Doors & windows**: slot into wall segments; doors are what make rooms routable — the ghost shows red until reachable.
- **Stairs & storeys** (v1.0): up to 3 storeys; stairs occupy a cell run and link floors.
- **Auto-roof**: per building — gable / hip / flat choice; regenerates as walls change.
- **Bulldoze** with rubble-free instant refund of wall/floor cost.

**Buy tab:**
- Catalog by room (Kitchen, Bathroom, Bedroom, Living, Outdoor) and by function (Seating, Surfaces, Beds, …). Cards show price, Comfort/Environment stats, and what the object *advertises* ("Fun +++, Social +").
- **Drag-ghost placement**: item follows the finger; grid-snapped; **green/red validity tint**; release drops a *tentative* placement with a confirm puck (✓ ✕ ⟳) — no fat-finger regret.
- 4-way rotation, move, **sell-back at 75%** (100% same-session), **eyedropper** to clone.
- **Undo/redo** covers everything in the mode.

**Economy hooks**: everything costs simoleons; the ghost shows affordability; bills (§13) scale with lot value — a mansion is a commitment.

## 8. Create-a-Sim (CAS)

A separate cozy stage (podium, warm light). MVP is **presets + palettes**, built for thumbs:

- **Body**: 3 builds × 2 frames (preset silhouettes, not sliders).
- **Head**: preset gallery (8 at MVP → 24 at v2.0).
- **Skin / hair / outfit tints**: swatch palettes (the engine tints one shared asset — swatches are effectively free content, see TDD §7).
- **Outfits**: tops / bottoms / shoes as modular parts; MVP 8/6/4 → scaling per §16.
- **Traits**: pick 2 of 8, with plain-language descriptions of what the trait *does*.
- **Name**: on-screen keyboard (game-rendered; Latin set at launch).
- **Randomize die** on every panel; a full random sim is always one tap.

Depth roadmap: v1.0 adds preset *variations* (bone-scale height/build nudges); v2.0+ evaluates true sculpting sliders (morph targets — an engine work package, TDD §3, taken only if retention data demands it).

## 9. World & lots

- One neighborhood: **Ember Lane** — a warm cul-de-sac. Lots are the play spaces; the street between them is a backdrop.
- **MVP: 2 lots** — the household's starter home + a small park (free Fun/Social objects, the "get out of the house" beat). **v1.0: 6 lots** (3 homes incl. two move-in-able, park, gym, café). v2.0: 10.
- **Travel** is a map sheet (illustrated card, tap a lot, ~2 s fade) — no streamed open world; each lot is self-contained. Off-lot household sims keep simulating **as records** (needs tick, career runs); non-household sims exist only as lot ambience.
- Community lots seed **visitor sims** from a small named-neighbor pool (persistent identities so relationships stick), plus background VAT ambience at P2+ (walkers on the street — set dressing, not simulated).

## 10. Time

- **1 sim-minute = 1 real second at speed 1** → a sim day = 24 real minutes.
- Speeds: **⏸ 0 · ▶ 1× · ▶▶ 3× · ▶▶▶ 10×** ("ultra"); ultra auto-drops to 1× on arrival/completion of the selected sim's action and on any crisis — fast-forward never skips the story beat.
- **Everything meaningful runs on sim time** (needs, shifts, bills, moodlets); real time drives only presentation.
- Day/night drives the sun and window light; sims have no hard curfew, but Energy and shifts make days naturally rhythmic.
- **Suspend stops the world.** Backgrounding the app checkpoints and freezes the clock. Nothing decays while you're away; there is nothing to "come back to before it's too late." This is a stated anti-FreePlay design position (Pillar 4), and it's what makes offline-first honest.
- Whole-household-asleep prompt: "sleep until morning?" (one tap skips to 7:00).

## 11. Touch UX & control scheme

Landscape, two-handed for build, one-thumb sufficient for live mode. Every target ≥ 57 logical px (engine minimum). All gestures have desktop mirrors (parenthesized) so the game is fully playable windowed.

**Camera (all modes):**

| Gesture | Action |
|---|---|
| 1-finger drag on world | pan (LMB-drag / WASD) |
| pinch | zoom 4–40 m (wheel) |
| 2-finger drag | orbit yaw + pitch 15–70° (RMB-drag) |
| floor chevrons (HUD) | active storey up/down (PgUp/PgDn) — drives the cutaway |

**Live mode:**

| Gesture | Action |
|---|---|
| tap sim / object | select + **pie menu** of available interactions |
| tap ground (sim selected) | *Go Here* marker |
| long-press object/sim | info card (stats, needs, relationship) |
| tap portrait | select + snap camera to that sim |
| tap need bar | queue the best fixing interaction (shortcut) |

**Pie menu**: radial, max 6 slices + *More…* page; opens under the thumb, never under the finger that tapped; slice icons + labels; one tap to queue, drag-release-to-slice also works (power users).

**Build/Buy mode:** catalog drag-ghost + confirm puck (§7); wall tool = touch-down on an edge, drag, live length; two-finger gestures stay camera (tools are one-finger only, so camera vs. tool is never ambiguous).

**View controls:** walls **down** (default cutaway) / walls **up**; roof auto-hides when the camera is inside or below it (see TDD §8 for the mechanism).

## 12. UI / HUD inventory

Screens & surfaces (wireframe-level; all layouts safe-area aware):

- **Live HUD**: bottom-left sim portraits w/ mood halos (household switcher) · top-center clock + date + speed buttons · top-right funds · bottom-right mode buttons (Build, Map, Menu) · top-left **action-queue chips** for the selected sim · **needs drawer** — swipe up on the selected portrait for the 6 bars + Comfort/Environment readout.
- **Pie menu** (§11).
- **Catalog** (buy): left category rail · scrollable card grid (engine GridLayout + ScrollView) · price/stat card face · owned-pack badge.
- **Build toolbar**: tool strip (wall, paint, floor, door, window, stairs*, roof, bulldoze, eyedropper, undo/redo).
- **CAS**: panel tabs (Body/Head/Color/Outfit/Traits/Name), swatch grids, randomize die.
- **Map sheet**: illustrated lot cards + travel.
- **Save/slots**: 3 manual slots + autosave, each with household name, funds, sim-date, timestamp.
- **Settings**: audio sliders (Music/SFX/UI/Ambient), quality tier (Auto/Low/Mid/High), haptics toggle, language (post-v1), credits, licenses.
- **Pack shelf** (§13): a catalog tab, not a popup.
- **Notifications**: toast stack top-center (promotion, level-up, visitor knock); tap to jump; never modal.
- **Onboarding**: contextual one-line coach marks (first 10 minutes), all skippable; no forced tutorial level.

## 13. Economy & monetization

**In-game economy (simoleons §):**
- Start: §20,000 after the furnished starter home.
- Income: career pay (MVP Culinary L1 §280/shift → L5 §1,150) + aspiration rewards.
- Outgoings: catalog purchases; **bills = 0.5% of lot value every 3 sim-days** (auto-paid; unpayable bills just block Buy mode — no repo man, per Pillar 1's comedy-not-punishment stance).
- Objects depreciate to a 75% sell-back floor.

**Monetization — cosmetic packs, offline, no dark patterns:**
- **What's sold**: themed content packs only — e.g., *Retro Kitchen*, *Garden Party*, *Cozy Loft* — each 12–20 build/buy objects + 4–8 CAS items. Pack objects have stats **within the free catalog's bands** (a §450 pack fridge ≡ a §450 free fridge): packs are style, never power.
- **How it's sold**: one-time Google Play purchases; entitlements stored locally; packs ship with the app (Play Asset Delivery) so purchases work **offline** and restore with the Play account.
- **Where it's surfaced**: a *Packs* tab in the catalog + CAS ("shelf, not popup"). Locked items are browsable and ghost-placeable-preview, clearly badged. No interstitials, no currency doubling, no bundles-of-currency at all — **there is no premium currency**.
- **Hard commitments (pillar-protected, printed in the store description)**: no ads · no energy/timers · no pay-for-power · no loot boxes/gacha · full game playable forever at §0.
- **Free cadence promise**: every paid pack releases alongside 2–3 free items, so the base catalog visibly grows too.
- Windows build: same packs via a simple key file at v1 (Steam later, out of scope here).

## 14. Audio direction

(The engine gains its audio system as work package HE-ENG-08 — see TDD §3/§10; this section sizes the content to that system's v1: bus mixer, 32 voices, positional 2D attenuation, streaming music.)

- **Voice**: "Hearthish" — short pitch-shifted gibberish syllable barks (8–20 per emotion class), comedic, language-neutral. No real VO.
- **Music**: 3 beds at MVP (menu/CAS warm acoustic · live-mode daytime · night variant), streamed, crossfaded on mode/time changes. Build mode: sparse marimba layer over the live bed.
- **SFX (~60 at MVP)**: UI ticks/confirms; per-interaction effects fired from animation events (chop, sizzle, shower, snore); build-mode placement thunks and wall-drag zips; stingers (level-up, promotion, relationship milestone).
- **Ambient positional loops**: fridge hum, TV murmur, outdoor birds by day / crickets by night — quiet, few, battery-cheap.
- Mix: Ambient −12 dB under SFX; music ducks −6 dB during barks; everything user-slidable (§12 Settings).

## 15. Art direction & readability

- **Stylized low-poly, tint-driven.** Shared base meshes + palette tints deliver variety (engine tint lane, TDD §3 HE-ENG-09) — the art budget goes into silhouettes and animation, not texture sets.
- Warm global palette; interiors read via IBL ambient + lamp emissives (no per-lamp shadows on mobile — TDD §8); the cutaway (walls-down default) is the signature view and must always look intentional, never broken.
- Readability rules: selected sim rim-tint; need crises glow on the portrait *and* the sim; interactable objects pulse subtly when a sim is selected and idle.
- Sims: ~1.7k-vert class bodies on the shared 51-bone-class skeleton (StickFigure lineage), exaggerated proportions for phone-screen readability.

## 16. Content scope

| Content | MVP (P1) | v1.0 (P2) | v2.0 (P3+) |
|---|---|---|---|
| Interactions | **25** | 60 | 120 |
| Buy-catalog objects | **40** | 120 | 300 |
| Traits | **8** | 12 | 20 |
| Skills | **5** | 5 | 7 |
| Careers | **1** | 3 | 5 |
| Lots | **2** | 6 | 10 |
| Social interactions | **8** | 16 | 30 |
| CAS: heads / hair / tops / bottoms | 8/6/8/6 | 16/14/20/16 | 24/24/40/30 |
| Aspiration chains | 4 | 10 | 20 |
| Music beds / SFX / barks | 3/60/40 | 5/120/80 | 8/200/150 |

**Cut & deferred (deliberate, with reasons):**
- **Death & aging** — v1 sims are adults forever; crisis-comedy replaces mortality stakes (revisit v2 with player data).
- **Babies/toddlers/kids** — animation + interaction cost is a full extra game; adults-only v1.
- **Pets** — same reason; most-requested DLC candidate.
- **Weather/seasons** — rendering + sim cost with no core-loop payoff at MVP.
- **Pools** — build-mode special-casing (water volumes) out of proportion to use.
- **Generations/genetics** — requires aging; deferred with it.
- **Multiplayer / house gallery / any online** — the game is offline by design (locked decision); no networking exists in the engine and none is planned for v1.
- **CJK localization** — font pipeline cost (TDD §3 HE-ENG-20); extended-Latin locales first.
- **iOS** — no engine platform layer exists; Android-first is a scope decision, not a preference.
- **Mod support** — data-table architecture keeps the door open; not a v1 commitment.

## 17. Phasing

Development phases, MVP definition, exit criteria, and the engine work that gates each phase live in **[TDD.md](TDD.md) §17 (roadmap)** and §3 (engine work packages). Summary: **P0** foundations (engine WPs + sim cores) → **P1 vertical slice = MVP**: *one sim, one house* — CAS-lite, 25 interactions, build/buy, save, full touch UX, audio v1, 30 fps on a real device → **P2** households/social/town → **P3** retail (packs + billing, release engineering, localization plumbing).
