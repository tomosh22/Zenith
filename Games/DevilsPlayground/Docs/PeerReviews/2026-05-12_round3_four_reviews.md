# Peer Reviews — Round 3, 2026-05-12

Four additional peer reviews ran after the round-2 reconciliation + user directives (navmesh re-inverted to auto-generated, engine work re-authorised for autonomous agents, InputsForAutonomy deleted). All four reviewed the post-round-2 state.

---

## Cross-reviewer convergence

| Theme | R1 | R2 | R3 | R4 |
|---|:--:|:--:|:--:|:--:|
| MVP-0.1.1 test-first vs auto-merge contradiction | C1 | — | — | — |
| Stale Sexton ref in MvpRoadmap §0.2.1 | — | C1 | I | S6 |
| Bootstrap infrastructure scripts still don't exist | C2 | — | — | — |
| ManualSetupChecklist has wrong runner path | I | — | — | — |
| Navmesh generator still risky despite user override | C3 | C3 | C1 | C1/W |
| Tier 4 tests' grid A* will route through walls | — | — | C4 | S4 |
| Audio system absence makes MVP not-fun | — | — | C2 | — |
| Build lock vulnerable to orphan compiler processes | S4 | — | C3 | I |
| Test instrumentation hooks scoped without engine bootstrap docs | — | C5 | S2 | — |
| 18-month full-game claim unsupportable | C6 | — | — | — |
| Test count drift (28 vs 34) | I | I | I | I |
| Save versioning missing | S6 | — | — | S7 |
| STL conventions inconsistent with prototype | — | S1 | — | I |
| Asset license provenance system missing | C5 | — | — | — |
| Human-gate tasks not marked | — | C2 | — | C3 |

V = verdict. C = critical. S = substantive. I = inconsistency. W = "what I'd change first."

## Round-3 verdicts

- **R1:** "Qualified no. Playable MVP plausible, not on 4-month path. 18-month premium ship not credible without scope cuts, paid art/audio help, harder build/test/provenance gate."
- **R2:** "Qualified yes — playable MVP achievable in 18 months. 4-month estimate is fiction. Compounding schedule risk."
- **R3:** "Qualified no. 7-9 months with fallback navmesh path; 18-month vision credible only if generator converges, audio lands, and asset cliff doesn't break tests."
- **R4:** "Will not ship a playable game in 18 months under autonomous-agent model as written. Human-led execution with agent-assist could ship MVP in 7-9 months."

## What this round changes

Round-2 reviewers found that the *plan was internally inconsistent*. Round-3 reviewers found that **the round-2 reconciliation itself left stale residue**: documents updated in different orders, the Sexton→Beggar swap didn't propagate to MvpRoadmap §0.2.1, the test-count drift persisted, and the autonomy chain now contains explicit human-gate tasks that aren't marked as such.

The round-3 reconciliation focuses on:

1. **Surgical fixes to specific stale references** (Sexton, runner path, test count, count errors). These are the cheapest critical fixes — 30 seconds to a few minutes each.
2. **HUMAN_GATE markers** on tasks that require human authentication or web-UI access (MVP-0.0.5, MVP-0.0.6, MVP-3.0.1, MVP-1.2.0). The autonomy claim must be qualified honestly.
3. **MVP-0.1.1 ordering fix** so test-first doesn't produce a compile-failing PR (which can't auto-merge on green CI).
4. **Navmesh-aware test pathfinder task** added before Tier 4 — otherwise the playthrough tests pass by wall-clipping.
5. **Save-game versioning** scoped into MVP-1.10.

User directives from earlier today (navmesh auto-generation; engine work autonomous) preserved. Reviewer recommendations to revert these were noted but not actioned — that's the user's call.

See `DecisionLog.md` 2026-05-12 round-3 entry for the per-change record.
