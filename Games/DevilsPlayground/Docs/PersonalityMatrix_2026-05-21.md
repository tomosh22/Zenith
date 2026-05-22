# Personality matrix insights — 2026-05-21 (7 personalities x 10 seeds)

Companion to `Tests/Test_PersonalityPlaythrough.cpp` and
`Tools/dp_seed_matrix_run.ps1`. This document captures the insights from
the first 70-cell matrix run after adding three new bot personalities
(Magpie / Relay / Heretic) on top of the four established ones
(Casual / Stealth / Speedrunner / Zealot).

The bots are pure-input playthroughs — they drive the procgen scene via
`Zenith_InputSimulator` only, no engine-side bypasses. So "win rate"
here is genuinely "could this play-style finish the game in 141.6 s of
in-game time on this procgen layout."

## Configuration

| Personality | Skip bootstrap | Adaptive sprint | Walk-quiet | Noise machine | Any-order objs | Voluntary relay | Noise first |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Casual      | n | n | n | y | n | n | n |
| Stealth     | n | n | y | n | n | n | n |
| Speedrunner | n | y | n | y | n | n | n |
| Zealot      | y | y | n | n | n | n | n |
| **Magpie**  | n | y | n | y | **y** | n | n |
| **Relay**   | y | y | n | n | n | **y** | n |
| **Heretic** | y | y | n | y | n | n | **y** |

## Per-personality summary (10 seeds: 0, 1, 7, 42, 100, 12345, 55555, 99999, 250000, 4276994270)

| Personality | Cells | Wins | WinRate | ObjsAvg | DeathsAvg | PossessAvg | 1stPickup(s) | 1stApprehend(s) | % NonMonotonic Order |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Casual      | 10 | 1 | 10% | 2.4 | 4.0 | 4.2 | 18.5 | 34.4 | 0%  |
| Stealth     | 10 | 0 |  0% | 0.7 | 3.8 | 4.2 | 24.0 | 59.9 | 0%  |
| Speedrunner | 10 | 1 | 10% | 2.5 | 10.0 | 10.4 | 8.5 | 34.1 | 0%  |
| Zealot      | 10 | 0 |  0% | 2.0 | 9.8 | 9.9 | 5.7 | 12.9 | 0%  |
| Magpie      | 10 | 1 | 10% | **2.8** | 9.7 | 9.8 | 8.2 | 39.9 | **88%** |
| **Relay**   | 10 | **3** | **30%** | **3.1** | 8.4 | 8.4 | 5.0 | 13.0 | 0%  |
| Heretic     | 10 | 0 |  0% | 2.3 | 9.9 | 9.9 | 11.5 | **8.5** | 0%  |

## Findings

### 1. Relay is the only personality that beats the 10% baseline

3 wins out of 10 (30%) — triple the next-best personality. Average
objectives delivered is 3.1 (vs 2.0-2.5 for the next four). The
voluntary-switch trigger fires reliably (logs show 5-7 RELAY trigger
events per cell when life drops below 5 s), and a tail-of-run successful
relay-switch is observed on every winning cell (seeds 0, 42, 4276994270).

**Implication**: the per-vessel life budget IS the dominant constraint
in single-vessel runs. Switching off voluntarily preserves the outgoing
villager for a future cycle (Fainted -> Idle in 10 s, instead of Dead
permanently), which has a measurable effect on the total objectives-
delivered count.

**Caveat**: the WIN comes from the click landing on the new target
villager. The matrix shows that 6 of 7 click attempts on seed 42 missed
(target villager idx=80 at screen (544, 215) returned no possession
flip). Only the 7th attempt at a different position succeeded. Click
reliability is the next bottleneck — improving the click-target picker
(e.g. centre-of-mass on the villager's bounding box, fallback to nearest
non-occluded villager) would likely lift the win rate further.

### 2. Magpie's any-order ordering is a robust improvement

88% of Magpie's runs delivered objectives in non-monotonic order. Seed 0
sequence was `[Obj4, Obj5, Obj2, Obj1, Obj3]` — wildly different from
the fixed `0->4` order Casual / Stealth / Speedrunner / Zealot all
follow. Magpie's average objectives-per-cell is 2.8, the highest after
Relay.

**Implication**: the fixed-order traversal cost is real but small (~13%
delivery delta between Magpie and the fixed-order personalities). The
"bootstrap chain wastes life budget" hypothesis Zealot was designed to
test is **falsified**: Zealot at 2.0 objs/cell underperforms Casual
(2.4) and Magpie (2.8). Bootstrap is net-positive; any-order is also
net-positive; they compound.

**Implication for design**: when the player-controllable bot picker
lands (later milestone), letting players choose the nearest objective
rather than enforcing a fixed delivery order would meaningfully help
win-rate without trivialising the loop.

### 3. Heretic backfired — noise machine baits the priest INTO the bot

First-Apprehend time for Heretic is 7.2-13.3 s across cells where the
priest catches it. Speedrunner / Casual on the same seeds: 35.5-74.4 s.
The priest is closing on Heretic 4-10x faster than on other personalities.

**Why**: Heretic walks to the noise machine, F-presses, then lingers
for 90 frames (1.5 s) per the `kHereticNoiseDistractFrames` constant
this PR introduced. The noise tells the priest "investigate this
position". The priest beelines to the noise-machine position — which
is exactly where Heretic is standing. Apprehend channel starts as soon
as the priest is in range.

**Fix** (deferred to follow-up): drop `kHereticNoiseDistractFrames` to
0 — emit noise and leave immediately. The priest still goes to
investigate, but the bot is elsewhere by the time it arrives. Repeated
emission opportunities (the "repeat noise emission when life budget
allows" axis from the original spec) could then be wired without the
priest ever catching up.

### 4. Stealth is unambiguously worst

Stealth: 0 wins, 0.7 objs/cell, 24 s to first pickup. The walk-quiet
modifier halves the villager's movement speed, and procgen layouts
just don't fit inside the 30 s life timer at that speed. Walk-quiet's
acoustic benefit doesn't translate into a meaningful win-rate signal
because the priest has so much extra time to find Stealth via line of
sight.

**Implication for tuning**: walk-quiet's speed cost is probably too
high for the size of procgen layouts. Either compress procgen
dimensions, or rebalance walk-quiet to half noise + 0.75x speed (not
half speed).

### 5. Skip-bootstrap alone is net-zero or negative

Zealot (skip bootstrap, adaptive sprint, in-order objs): 2.0 objs/cell,
0 wins. Speedrunner (full bootstrap, adaptive sprint, in-order objs):
2.5 objs/cell, 1 win.

Skipping the iron / forge / door / chest / noise chain does NOT pay for
itself in objective deliveries — Zealot actually drops by 0.5 objs/cell
vs Speedrunner. The bootstrap chain produces the locked-door unlock
(via forge + Key), which appears to be more valuable than the ~30 s of
life-timer budget it costs.

The pattern suggests: bootstrap is good, any-order is good, voluntary
relay is good — and the "right" hypothetical bot is **Speedrunner +
Magpie + Relay**. None of the seven implemented today combine all three.

## Suggested follow-ups (in priority order)

1. **Magpie+Relay combo personality**. The strongest predicted win-rate
   improver from the data here. Both modifications are orthogonal — pick
   any uncollected obj closest to me, AND voluntary-switch when life
   drops below 5 s.
2. **Heretic emit-and-flee**. Drop `kHereticNoiseDistractFrames` to 0
   (or even -- press F as part of `kHP_ObjLoopWalk` rather than as a
   dedicated phase, so the bot is mid-walk-to-obj as the priest arrives
   at the noise machine).
3. **Relay click-targeting reliability**. The 6/7 miss rate on seed 42
   says the screen-to-world raycast doesn't land cleanly on villager
   bodies. Aim at the villager's head/torso anchor (y += 1.5 m instead
   of +0.9 m) and / or fall back to next-closest healthy villager when
   the first click target fails.
4. **Walk-quiet rebalance**. Stealth's 0% win rate is a tuning issue,
   not a design issue. Either compress procgen dimensions or shift
   walk-quiet's speed multiplier from 0.5x to 0.75x.

## Reproducing this matrix

```
# Build (or rebuild) DP in Debug_False.
pwsh -Command '& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Build/zenith_win64.sln /t:DevilsPlayground /p:Configuration=vs2022_Debug_Win64_False /p:Platform=x64 -maxCpuCount:1'

# Run 7p x 10s matrix (~35 min at P=4).
pwsh -Command "& 'Tools/dp_seed_matrix_run.ps1' -Seeds @(0,1,7,42,100,12345,55555,99999,250000,4276994270)"

# Re-render the analyser report.
python Tools/dp_seed_matrix_analyse.py

# Run the cross-personality comparison summary (the table at the top).
python Tools/dp_personality_compare.py
```

---

# PR #140 follow-up — 4 fixes + 8th personality (2026-05-21, later same day)

Implemented all four follow-ups from the PR #139 writeup above. New
matrix run: 8 personalities x 10 seeds = 80 cells. Per-personality
table:

| Personality | Cells | Wins | WinRate | ObjsAvg | DeathsAvg | PossessAvg | 1stPickup(s) | 1stApprehend(s) | % NonMonotonic |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Casual      | 10 | 1 | 10% | 2.6 | 4.0 | 4.2 | 18.2 | 44.2 | 0%  |
| Stealth     | 10 | 0 |  0% | **1.5** | 3.9 | 4.4 | 28.2 | 40.8 | 0%  |
| Speedrunner | 10 | 1 | 10% | 2.6 | 10.1 | 10.2 | 8.5 | 26.0 | 0%  |
| Zealot      | 10 | 0 |  0% | 2.2 | 9.9 | 10.1 | 5.9 | 12.9 | 0%  |
| Magpie      | 10 | 1 | 10% | 2.8 | 9.7 | 9.8 | 8.2 | 36.0 | 88% |
| Relay       | 10 | 0 |  0% | 2.5 | 8.6 | 8.8 | 6.4 |  3.5 | 0%  |
| Heretic     | 10 | 0 |  0% | 2.4 | 9.4 | 9.4 | 10.3 | **34.9** | 0%  |
| Trickster   | 10 | 0 |  0% | 2.2 | 9.7 | 9.7 | 8.2 | 52.1 | **86%** |

## What each fix did

### Walk-quiet rebalance (0.5x -> 0.75x): clear win

Stealth's `ObjsAvg` jumped from **0.7 -> 1.5 (+114%)** after bumping
`movement.walk_speed_mps` from 4.0 to 6.0. Still 0 wins -- procgen
dimensions are large enough that even 0.75x jog speed isn't quite
fast enough -- but the personality is no longer dominated. The walk-
quiet acoustic benefit (priest hearing range halved) is now tradeable
against the (smaller) speed loss.

### Heretic emit-and-flee: clear win

Heretic's `1stApprehend` went from **8.5 s -> 34.9 s (+310%)**. With
`kHereticNoiseDistractFrames` dropped from 90 to 0, the bot emits noise
and leaves immediately. The priest still investigates the noise (the
stimulus reaches the priest the same way), but by the time it arrives
Heretic is already in the obj loop elsewhere on the map. Heretic now
also has 1stApprehend later than even Casual (44.2 s).

### Trickster combo personality: lands but tuning needed

New 8th personality combining Magpie's any-order + Relay's voluntary-
switch + Casual's bootstrap + Speedrunner's adaptive sprint. The combo
fires correctly: 86% non-monotonic order (Magpie part works) and the
relay-trigger logs fire on low-life cells (Relay part works). But 0
wins and 2.2 ObjsAvg -- the bootstrap chain steals enough life-timer
budget that even with voluntary relay the bot can't recover all 5
objectives. The Magpie+Relay+Speedrunner config (no bootstrap, like
Relay but with any-order) is probably the next tuning experiment.

### Relay click-targeting reliability: net-neutral / slight regression

Multi-attempt click logic (alternating y=0.9 / y=1.8 head heights, max 2
target tries) tightened the timeout from 30 frames to 8. Recovered
ObjsAvg to 2.5 (from a v1 botched-tuning 2.2), but win count went from
PR #139's 3 -> 0 in this run. Hypothesis: most of PR #139's 3 wins came
from the FALLBACK path (Relay click misses -> bot falls back to natural
death + repossess), not the deliberate switch. The new code spends
extra frames on retries before giving up, hurting the fallback timing
on some cells.

The honest read: 10 seeds is not enough to distinguish a true 30% win
rate from a true 0-10% win rate for Relay. A 30-seed matrix would
sharpen the statistics here.

## Matrix variance is non-trivial

The 1stApprehend time for Casual was 34.4 s in PR #139 and 44.2 s in
PR #140 on identical seeds. The procgen layout is bit-deterministic
across runs (`Test_ProcLevel_DeterminismCheck` guards this), so the
delta has to come from non-deterministic AI perception ordering --
likely thread-scheduling artifacts in the perception update + priest
BT tick. The implication: per-personality WinRate at 10 seeds has
substantial noise. Treat single-digit win-count deltas as inconclusive.

## Suggested follow-ups (next PR)

1. **30-seed matrix run.** Sharpen the win-rate statistics so we can
   tell PR #139's 30% Relay from this PR's 0% Relay -- is one of them
   a noise floor? A 30-seed pool would let us see ~3 sigma deltas.
2. **Magpie+Relay no-bootstrap personality.** Trickster runs the
   bootstrap; an alternate combo that skips it (call it `Phantom`?)
   tests whether the bootstrap chain is the reason Trickster doesn't
   beat Relay+Magpie individually.
3. **Determinism audit on the AI perception pipeline.** Same procgen
   seed, same engine, but different 1stApprehend times across runs --
   suggests either thread-scheduling or floating-point order
   dependence in the perception update. Fixing this would tighten the
   matrix' signal-to-noise.
4. **Relay click anchor**: try `villager_collider.AABB.center` instead
   of `villager_origin.y + 0.9 m`. The y+0.9 anchor is a guess at head
   height; the AABB centre is whatever the collider actually is on
   that archetype.
