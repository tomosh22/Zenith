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
