# What Claude Code Needs to Build *Devil's Playground* Autonomously

**Document purpose:** An honest self-audit. The previous five documents describe the game, gaps, tests, assets, and asset tests. This one names what's still missing for Claude Code to execute the full 18-month plan **without human intervention** — and which of those gaps are fillable, which are irreducibly human, and which can be partially closed with tooling investment.

**Author:** Claude Code, reflecting on its own constraints
**Companion docs:** [GameDesignDocument.md](GameDesignDocument.md) · [Shortfalls.md](Shortfalls.md) · [TestPlan.md](TestPlan.md) · [AssetManifest.md](AssetManifest.md) · [AssetTestPlan.md](AssetTestPlan.md)
**Last updated:** 2026-05-11

---

## 0. The Honest Bottom Line

If you handed me everything in this document tomorrow, I could probably take the project ~70% of the way to ship.

The remaining 30% — the part that determines whether the game is *good* — has irreducible human components: composition, voice acting, art direction, and the question that no test can answer, "is this fun?" Those can be **structurally bounded** (I describe how below) but not eliminated.

This document is organised into three tiers:

1. **Tier A — Information you can hand me as data, which lets me execute autonomously.** Numbered lists, JSON files, decisions. Cheap to produce, large multiplier.
2. **Tier B — Capabilities that require engineering or tooling investment, which expand what "autonomous" can mean.** Telemetry, autonomous playtest agents, decision-logging.
3. **Tier C — Things only humans can do.** Composition, art direction, "is this fun." I describe how to minimise these but not how to eliminate them.

---

## 1. Tier A — Information You Can Hand Me as Data

These are the cheapest wins. None of them require new code; they're just **decisions written down**.

### 1.1 Framing decisions to ratify (or change)

The GDD I wrote made several strong directional bets. Before I write a line of code, you should ratify or change each one:

| Decision | My current bet | Cost to flip |
|---|---|---|
| Genre | Stealth-puzzle roguelite | Re-write GDD §1–5. ~2 days of doc work. |
| Setting | 1670s English moor village | Re-write GDD §2. ~1 day. |
| Target platforms | PC primary, consoles secondary, no mobile | Mobile would re-shape input, fog-of-war, run length. ~4 days re-doc. |
| Single vs. multiplayer | Single-player only | Co-op would invert the possession mechanic. New game. |
| Monetisation | Premium $19.99, no microtransactions | F2P would re-shape progression. Different game. |
| Run length | 25–35 min per Night | 60-min "long-form" or 5-min "snackable" changes everything. |
| Campaign length | 7 Nights / 6–10 h main path | 3 Nights or 14 Nights are both viable; just scope. |
| Tone | Folk horror, dread, occasional dread spikes | Pulp horror, psychological horror, comedy horror are all coherent alternatives. |
| Team & budget | 12 FTE, 18 months, ~$3M | Solo project, AAA, or external publisher all dramatically reshape the plan. |

**What to give me:** A signed-off version of GDD §1.1–§1.4 (the elevator pitch, USP, audience, pillars). Even a one-line "I confirm this framing" is enough for me to commit to it across all five docs. If any answer changes, the whole pack downstream re-aligns.

### 1.2 Tuning tables (the single biggest data unlock)

Every numerical value in the GDD is currently embedded in prose. To execute autonomously I need them in one structured file so I can read them, write tests against them, and update them in one place.

**Format:** `Games/DevilsPlayground/Config/Tuning.json` (or `.zdata` if we want it engine-loadable). One example shape:

```json
{
  "possession": {
    "life_timer_default_s": 30.0,
    "life_timer_min_s": 15.0,
    "life_timer_max_s": 45.0,
    "channel_devout_s": 0.8,
    "cooldown_after_switch_s": 1.5,
    "range_from_anchor_m": 15.0,
    "demon_scent_per_possession": 0.3,
    "demon_scent_decay_per_s": 0.05,
    "demon_scent_hound_bark_threshold": 0.5
  },
  "movement": {
    "walk_speed_mps": 4.0,
    "jog_speed_mps": 8.0,
    "sprint_speed_mps": 12.0,
    "sprint_life_cost_extra_per_s": 3.0
  },
  "priest": {
    "sight_range_m": 25.0,
    "sight_fov_deg": 110.0,
    "sight_peripheral_deg": 130.0,
    "hearing_range_m": 30.0,
    "walk_speed_mps": 4.0,
    "pursue_speed_mps": 7.0,
    "apprehend_range_m": 2.0,
    "apprehend_channel_s": 3.0,
    "apprehend_channel_cruel_s": 1.5,
    "investigate_memory_s": 8.0,
    "investigate_memory_cautious_s": 30.0
  },
  "hounds": {
    "tether_range_m": 15.0,
    "bark_range_m": 6.0,
    "sniff_range_m": 12.0
  },
  "night": {
    "dawn_timer_s": 1800.0,
    "modifier_hound_release_s": 300.0,
    "modifier_flintlock_s": 600.0,
    "modifier_bless_s": 900.0,
    "modifier_exorcise_s": 1200.0
  }
  // ... etc, ~80–120 values total
}
```

**What to give me:** Either confirm the GDD values, or hand me a tuning JSON with your preferred numbers. I will:

1. Generate a code-side `DP_Tuning::Get<float>("possession.life_timer_default_s")` accessor.
2. Update every behaviour to read from it.
3. Generate a test (`Test_P1Tuning_ValuesInBand`) that asserts each value is in its sanity range.
4. Update [TestPlan.md](TestPlan.md) to reference values from the JSON, not from prose.

**Why this is the biggest unlock:** every other tuning question downstream of this ("does the priest feel fair?") becomes a one-line change in a JSON file, runnable through the automated test suite, with the playtesting question structurally framed.

### 1.3 Per-archetype design table

The GDD lists 13 example archetypes by name; the rest are gestured at. To build them I need a complete table. Format:

```json
{
  "archetypes": [
    {
      "id": "Farmhand",
      "display_name_key": "archetype.farmhand.name",
      "life_timer_s": 30.0,
      "walk_speed_mps": 4.0,
      "jog_speed_mps": 8.0,
      "sprint_speed_mps": 12.0,
      "possession_channel_s": 0.0,
      "demon_scent_floor": 0.0,
      "abilities": [],
      "restrictions": [],
      "rarity_per_night": 1.0,
      "min_spawns_per_night": 2
    },
    {
      "id": "Child",
      "life_timer_s": 15.0,
      "abilities": ["invisible_to_hounds"],
      "restrictions": ["cannot_carry_tools"]
    }
    // ... 22 more
  ]
}
```

**What to give me:** the full 24-row table. I can generate a starting draft if you want, but you should review/approve each archetype's mechanical identity before I start hard-coding tests against them.

### 1.4 Per-reagent design table

Same idea, 14 rows. Each reagent needs:

```json
{
  "id": "BurialCoin",
  "display_name_key": "reagent.burial_coin.name",
  "pickup_channel_s": 1.0,
  "spawn_candidate_districts": ["longhouse"],
  "spawn_candidate_anchors": ["longhouse_pillow_a", "longhouse_pillow_b"],
  "special_behaviour": "none",
  "rarity": 0.2
}
```

Some reagents have special behaviours (bog-water evaporates after 8 s; bell's soul rings the bell on pickup). Those need to be enumerated as a closed set so I can implement them all.

### 1.5 Building / district / map data

The hand-authored map (GDD §6.1) is described prose-only. To author it I need:

**Either** a Vexholme reference map (sketch, hand-drawn, even a napkin scan) showing district boundaries and approximate building positions.

**Or** explicit consent to design it myself, with you reviewing my proposal before I commit. (I'll happily generate it, but a map is a single creative decision that's worth your time.)

### 1.6 Per-cutscene script

14 cutscenes (GDD §2.3). Each needs:

- A 4–8 sentence script.
- Identified speakers (Aelfric, hermits, narrator).
- The story beat it conveys.
- Branching choice (does the cutscene fork on win vs lose?).

**What to give me:** Either a written script for each, or a one-paragraph treatment per Night that I expand into a draft script for your review.

### 1.7 Localisation source

I can generate placeholder English strings and structure the loc system; I cannot decide which 8 languages we ship. The plausible-default set is `en, fr, de, es, it, ja, ko, zh-Hans` (Steam top-8) — confirm or replace.

For each shipping language: a vendor relationship (or a budget so I can engage one). LQA is a separate spend.

### 1.8 Run-result narrative copy

Each of the 9 possible campaign endings (GDD §2.3) needs ~300 words of ending text. ~2,700 words total. Either:

- You write them.
- You give me a one-paragraph treatment per ending and I draft from there for your review.

### 1.9 Accessibility settings defaults

The GDD §10 list is comprehensive but doesn't commit defaults. For each accessibility toggle, what's the default state? Examples that need a call:

- Whisper-line: on or off?
- Camera follow: on or off?
- Time-scale: 1.0 default; min-clamp at 0.6 or lower?
- Subtitles: on or off?

A 30-row JSON file solves this.

---

## 2. Tier B — Tooling Investment That Expands What "Autonomous" Means

These aren't decisions; they're engineering work that unlocks new categories of autonomous operation.

### 2.1 An autonomous playtest agent (~6 engineering weeks)

The single biggest "fun" gap is that I can't tell whether a feature *feels good*. But I can tell whether a feature is *winnable, learnable, and varied* — and those three properties correlate strongly with "fun" for the genre we're shipping.

**The agent:** A bot that plays scripted-or-randomised runs through the game, recording for each:

- Run duration (median, 90th percentile, max).
- Win rate (target: 35–55% for skilled play; tunable).
- Death cause distribution (we want a mix; if 90% of deaths are "apprehended in the open," players will feel cheated).
- Decision diversity (do bot strategies branch, or does every run play the same?).
- Aelfric reaction frequency (we want him on screen ~60% of the time as a perception/threat).
- Reagent-collection time per reagent (catches "the bog-water reagent is too hard" early).

The bot runs **nightly across 1,000 randomised seeds**. The aggregate stats land in `dp_playtest_summary.json`. I read it and make tuning decisions: if win-rate dropped from 45% to 18% on the latest commit, something regressed. If reagent-3 takes 4× longer than reagent-1 to collect, it's mis-tuned.

**Architecture:** The bot extends the existing `Zenith_InputSimulator` with a state-machine player that picks the closest visible villager, possesses, walks toward the nearest objective via navmesh, picks up, inscribes. Plus a controlled-randomisation layer that explores alternative strategies. ~6 engineering weeks of work; pays back permanently.

**Limitation:** The bot is a *consistency* signal, not a *fun* signal. It will tell me when a feature regresses against design intent; it cannot tell me the feature was a good idea to begin with. That bridge is in Tier C.

### 2.2 Telemetry + reasoning agent (~3 engineering weeks)

A lighter-weight version: an in-game telemetry stream that captures every player decision, plus a Claude-readable analysis loop that summarises trends weekly.

```cpp
// Every gameplay event gets logged with rich context:
DP_Telemetry::Record({
    .event = "possession_switch",
    .villager_archetype = "Devout",
    .reason = "burnout",   // vs. "voluntary", "apprehended"
    .demon_scent = 0.42f,
    .time_since_last_switch_s = 6.3f,
    .aelfric_distance_m = 18.5f,
});
```

The telemetry pipeline writes structured JSON per run. Once human playtesters land in Phase 3, a single Claude session can:

- Parse 100 playtest sessions.
- Surface "the Devout archetype is possessed 5× more than any other on Night 3."
- Cross-reference with success rate.
- Propose tuning changes.
- Generate the test that confirms the change.

This converts human playtester sessions from "a lead reads the feedback form" into "Claude reads 1,000 telemetry records and produces a ranked tuning proposal." Massive multiplier on human feedback.

### 2.3 Replay-debug tool (~3 engineering weeks)

Per GDD §13.1 and [TestPlan.md §5.5](TestPlan.md): a tool that captures every input and game-state event in a rolling 10-second buffer, with a "save replay" command. When a test fails or a bug is reported, the replay is the artefact.

I can implement this. Once it exists, **failures stop being mysterious** — I open the JSON replay, walk frame-by-frame, see exactly what input the test simulator pushed and exactly how the game responded. The "I can't reproduce it" failure mode is engineered away.

### 2.4 Decision-logging for me (~1 engineering week)

A meta-tool. Every decision I make during development — a tuning change, a refactor, a test added — gets logged with a one-paragraph justification, the test that proved it correct, and the test that prevents the regression.

The log lives in `Docs/DecisionLog.md`. Future sessions of mine read it to avoid re-litigating settled questions. Future humans read it to audit my choices.

This is `auto memory` (already wired up for me in this session) but project-scoped instead of conversation-scoped. ~1 week of tooling to format it as a queryable artefact.

### 2.5 LLM-driven NPC dialogue (~4 engineering weeks)

GDD §2.4 — villagers don't speak; Aelfric has only ~20 lines. We *could* use an LLM to generate dynamic Aelfric mutters that respond to the live game state ("Cold here… that body walked past me an hour ago…"). The base-game's voice cost stays bounded because the LLM-generated lines go through TTS (lower-fidelity than the human-recorded 20 lines), but the **density of audio in the world** improves dramatically.

This is creative-direction risk: LLM dialogue can break tone. Mitigations:

- A blacklist of words/themes that auto-rejects a line.
- Generated lines pre-cached and reviewed in batches before shipping.
- Player-toggleable in settings ("AI commentary on/off").

I list this as a Tier B capability, not Tier A, because it requires both engineering work and a creative-direction commitment from the user. Worth discussing.

### 2.6 Automated balance regression detection (~2 engineering weeks)

Every nightly build runs the autonomous playtest agent on the night's commit AND the previous green build. If any aggregate metric moves by > 15% from the baseline, the build is flagged for review. (Examples: win rate, reagent-collection time, Aelfric apprehend success rate.)

This catches "I tuned the priest's sight range up by 10% and now nobody wins" within 24 h of the commit.

---

## 3. Tier C — Things Only Humans Can Do

I want to be honest about the parts I genuinely cannot do, even with maximal tooling. These are the irreducible human inputs.

### 3.1 Composition

The game's music. ~45 minutes of original score across menu, gameplay-dynamic, between-Night ambient, and cutscenes. Composition is a creative discipline; my contribution is at best generating a temp track via royalty-free libraries. I can:

- Spec the brief in detail.
- Provide reference tracks.
- Manage the back-and-forth with the composer through your relay (you forward composer's stems to me; I evaluate against the brief).
- Cue the score against gameplay (decide *when* each track plays).

I **cannot** compose. A human composer takes ~4 months on a budget of ~$30–80k.

### 3.2 Voice acting

Aelfric has 20 lines; cabal hermits + Joan have ~9 lines; demon Latin chorale has 6 stems. A voice director, casting decisions, and a recording session are required. I can:

- Write the lines.
- Provide direction notes.
- Use TTS for placeholder VO during development.

I **cannot** record human VO. Plan ~$8–20k for cast + studio + direction across all roles.

### 3.3 Concept art

The fundamental human creative input. I can describe the painting; I cannot paint it.

I can use external image-generation tools if you authorise it, but image generation is unreliable for production-quality concept art with consistent character continuity. A human concept artist with 4–6 months of runway at a senior-rate is the baseline plan.

Mitigation: a single concept lead can support the whole production (~$70–110k/year FTE).

### 3.4 Final visual art direction

Even with detailed concept art, the per-shot creative judgement of "this hearth needs to be 20% smaller and the candle needs to be warmer" is a human discipline. An art director or environment-art lead drives this for 18 months.

### 3.5 "Is this fun?"

The hard one. Even with autonomous playtest bots, telemetry, and replay-debug tools, the question of whether a player enjoys 30 seconds of running a body to a pentagram is a human judgement.

The **structured** path to closing this gap, from cheapest to most expensive:

| Approach | Cost | Signal quality |
|---|---|---|
| Self-test (one designer plays daily) | Free | Low — designer-fatigue and proximity bias |
| Internal playtest (team plays Fridays) | Team's time | Moderate — but team is captive |
| Friends-and-family closed alpha | Travel + sandwiches | Moderate — pleasant lying |
| Discord closed beta (1,000 testers) | ~3 mo of community management | Good — anonymous, large-N |
| Steam Next Fest demo | Mostly free | Very good — real strangers |
| Hired UX researcher with 12-person panel | ~$15k | Very good — qualitative depth |

I can prepare every artefact for each of these (build, telemetry, surveys, briefings) but the human stays in the loop.

### 3.6 Publisher / platform negotiation

Steam keys, console SKU registration, age ratings (ESRB, PEGI, CERO), publisher contracts, marketing campaigns — these are human-relationship work I have no agency in.

### 3.7 Legal & contractual

Music licensing (any temp tracks you can't ship), font licensing (the display font), composer contracts, asset-store EULAs, voice-actor contracts, NDA's with playtesters. A human-or-paralegal review pass per category.

### 3.8 Cert pre-flight on physical dev kits

Switch / PlayStation / Xbox cert checks for hot-unplug, suspend/resume on actual hardware, retail-disc loading. I can validate the in-process proxies (per TestPlan §5.3) but the final cert pass requires dev kits + human operators.

---

## 4. Information About the *Current* State

Before I begin the 18-month plan, I need a current-state snapshot. Each of these is a single command-line answer (or one screenshot's worth of info):

### 4.1 Build state

- Does the prototype build today on this machine?
- Does the non-tools build (per Memory) work yet, or is it still broken?
- Are there uncommitted changes I should know about?

### 4.2 Test state

- Do all 28 existing tests pass right now?
- If any fail, which ones, and is the failure expected (e.g., a feature was disabled)?

### 4.3 Recent activity

- What was the user last working on? (`git log --oneline -20` answers this.)
- Are there in-flight branches I shouldn't touch?

### 4.4 Team & schedule

- Am I working alone (solo dev style) or with a team?
- If a team: who else commits, and how do I avoid conflicts?
- Is there a release calendar? Milestones?
- Hours of expected progress per week?

### 4.5 Backlog

- What's the highest-priority task right now?
- Is there an existing issue tracker (GitHub Issues? Linear? Jira?) I should be reading?

A single `Docs/Status.md` updated weekly answers all of 4.1–4.5 in under 200 words.

---

## 5. External Integrations I Need Credentials/Access For

Each of these is a one-time setup that, once done, never blocks me again.

| Integration | Why | What to provide |
|---|---|---|
| **GitHub Actions CI** | Run the test suite on every PR | Repo admin access to add workflows; secrets for Steam upload (later) |
| **Steam Partner** | Build upload, store page, Steamworks SDK | Steam dev account, app ID, depot keys |
| **Xbox / PS / Switch dev kits** | Console cert pass | First-party developer accounts + physical kits |
| **Mixamo / Synty / Kenney** | Source S0/S1 placeholders | Account credentials or downloaded packs in `Vendor/` |
| **PolyHaven** | Free HDRIs and textures | Just need permission to download (CC0) |
| **Freesound.org** | Temp SFX | Account |
| **FreeMusicArchive** | Temp music | Account |
| **TTS provider** (Azure / ElevenLabs) | Placeholder Aelfric VO | API key with monthly budget cap |
| **Localisation vendor** | LQA at ship | Vendor relationship (~$15k/lang) |
| **Composer** | Final music | Contract |
| **VO studio + cast** | Final VO | Contract |
| **External art shops** | S2 asset detailing | Contracts |

**The minimum viable set for me to start Phase 1 today:** GitHub Actions access + Mixamo account + Freesound/FMA accounts. Everything else can wait.

---

## 6. Process & Cadence Decisions

How do you want to work with me? These are small decisions that compound over 18 months.

### 6.1 Session model

- **Long-running single agent:** I work on one task per session, you review, next session resumes. Best for high-quality changes.
- **Background loop:** I run autonomous over hours/days, reporting progress. Best for content-volume work (parameterised tests, asset linting).
- **Mixed:** I default to long-running for design-load-bearing work, switch to background for content sweeps.

**Recommendation:** Mixed. The decision-log (§2.4) keeps state coherent across sessions.

### 6.2 Commit cadence

- **One commit per task** vs. **batched commits**? PR-per-feature vs. trunk-based?
- Should I push, or just produce diffs you push?
- Force-push policy?

**Recommendation:** One commit per logical unit, PR-per-feature, never force-push without your direction. Matches industry best practice.

### 6.3 Approval policy

- Do I need approval before merging? Or can I auto-merge on green CI?
- Does some category of change (e.g., changes to `Tuning.json`) require explicit approval?

**Recommendation:** Auto-merge on green CI for code and tests. Manual approval for `Tuning.json`, all asset additions, all design-doc changes.

### 6.4 Communication artefacts

- Where do I report progress? (A `Docs/Status.md` file you `cat` weekly? A Slack channel? GitHub issues?)
- Where do I ask questions? (A `Docs/Questions.md` you respond to async?)

**Recommendation:** A `Docs/Status.md` for completed work, a `Docs/Questions.md` for blocked items, both updated as the natural by-product of each session.

### 6.5 Escalation protocol

- When I'm stuck, what do I do? Wait? Make a best guess and flag it? Pivot to a different task?
- When I make a judgement call, do you want to review it, or trust me?

**Recommendation:** Make best guess, flag in `Docs/Questions.md` with reasoning, continue. Review in batch.

---

## 7. Things I'd Like You to Decide Now

A direct list. If you can answer these in one session, I can start tomorrow on Phase 1.

1. **Do you confirm the GDD's framing?** (Stealth-puzzle roguelite, 1670s English moor, PC primary, premium $19.99, 18 months.)
2. **What's the budget envelope?** Solo / hobby / lean studio (3-5 ppl) / mid-tier (10-15 ppl) / AAA?
3. **How many hours/week are you committing?** Are you the sole human, or coordinating with others?
4. **Pick a target launch date.** Even a guess. (It anchors all milestone planning.)
5. **Do I have permission to spend on external services?** (TTS API, asset packs, eventual composer/VO.) If yes, what cap?
6. **Auto-merge on green CI: yes or no?**
7. **First milestone target?** "Phase 1 in 4 months" is my plan; you can tighten or loosen.
8. **Do you want me to run autonomously between sessions (background agent mode), or only when invoked?**

---

## 8. What I Would Do With Everything

A worked example, end-to-end, if you handed me Tier A + Tier B in a single session.

### Day 1

1. Read `Tuning.json`, `Archetypes.json`, `Reagents.json`. Generate `DP_Tuning::Get<T>(key)` accessor.
2. Update every existing behaviour to read from tuning.
3. Generate `Test_P1Tuning_ValuesInBand`. CI passes.
4. Open `Docs/Status.md`, log: "Tuning system online. Existing behaviour reads from data."

### Days 2–10

1. Implement the Phase 1 foundation gaps from [Shortfalls.md §1](Shortfalls.md): pause, apprehend, real navmesh, voluntary switch + drop, possession cooldown.
2. Write the Tier 1 tests from [TestPlan.md §2](TestPlan.md) *before* each implementation. Watch them fail. Implement. Watch them pass.
3. Each task: one commit, one PR, auto-merge on green.
4. `Docs/Status.md` updated daily.

### Days 11–30

1. Phase 1 polish: priest omniscience fallback removed, `.zscen` re-bake CI, save/load scaffolding.
2. Implement the asset linter (§3 of [AssetTestPlan.md](AssetTestPlan.md)).
3. Source Mixamo + Synty packs. Import. Linter passes.
4. Implement the autonomous playtest agent (Tier B §2.1).
5. First end-to-end playthrough by the bot. Win rate logged. Calibration begins.

### Days 31–90

1. Phase 2 begins: archetypes, hounds, reagents, charms, distractions, fog, Liminal.
2. Each system follows the same pattern: tuning entry → tests written → implementation → green CI → status log update.
3. Audio system engine work begins (this is the only true blocker since it needs you to either staff it or commission it).
4. Composer brief sent (if budget approved).

### Day ~120

Phase 1 complete. The game has real loss states. The bot can win Night 1. The art is at S0. The HUD is wireframe. The audio is silent. **It is a real game.**

### Months 5–18

Same loop. Each phase's tests written first, implementation drives the test to green, content added until inventory tests assert completeness. Autonomous playtest agent flags any regression in fun-proxies (win rate, decision diversity). Telemetry from any human playtester sessions you arrange feeds the tuning loop. Concept art, music, VO, and final art arrive on a parallel track managed via the briefs I write and the back-and-forth you relay.

### Month 18

Gold master. Cert. Ship.

---

## 9. Summary

The single highest-leverage thing you can do is **convert my prose-embedded design decisions into structured data files**. The Tuning JSON, the archetype table, the reagent table, the cutscene scripts. With those in hand, my 18-month plan is mostly mechanical execution.

The single highest-leverage **engineering investment** is the **autonomous playtest agent** plus the **telemetry pipeline**. Together they convert "is this fun?" from an opaque question into a quantified, claudia-readable signal that I can iterate against between human playtest sessions.

The single thing that **cannot be removed** from the human side is **art direction and composition**. Plan for those as full creative-discipline relationships with humans on contract. Everything around them — briefs, scheduling, integration, testing — I can run.

Hand me a `Tuning.json`, a `Archetypes.json`, a `Reagents.json`, a `Cutscenes.json`, ratification of the GDD framing, and a budget for Mixamo + an API key for TTS — and I can begin Phase 1 tomorrow.

The five-document pack plus this one is the **complete production plan**. Everything else is execution.
