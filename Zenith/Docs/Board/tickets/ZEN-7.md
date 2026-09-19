---
key: ZEN-7
type: BUG
title: ZENITH_ASSERT_EQ_FLOAT passes on NaN at every epsilon, so a degenerate float is indistinguishable from a correct one
category: Engine
parent: "ZEN-1"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: HIGH
complexity: MODERATE
risk: HIGH
storyPoints: 5
position: 3
labels:
  - tech-debt
fixVersions: []
blocks: []
links: []
source:
  id: w51x4uz7k2xgkknylk3ysgya
  risk: HIGH
  type: BUG
  title: ZENITH_ASSERT_EQ_FLOAT passes on NaN at every epsilon, so a degenerate float is indistinguishable from a correct one
  number: 7
  flagged: false
  due_date: null
  position: 3
  priority: HIGH
  severity: S2_MAJOR
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: MODERATE
  created_at: 2026-08-23T00:54:01.552949
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-23T00:54:01.552949
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: ALWAYS
updatedAt: "2026-09-19T20:43:59.255Z"
claimAttempt: null
---
## Goal

`ZENITH_ASSERT_EQ_FLOAT` passes on NaN, unconditionally, at every epsilon
including `0.0f`.

`Zenith/Core/Zenith_TestFramework.cpp` implements the comparison as:

```cpp
if (std::fabs(fA - fB) > fEps)   // → the failure branch
```

`NaN > fEps` is **false** for every `fEps`, so the failure branch is never taken
when either side is NaN. A test whose only guard on a float is
`ZENITH_ASSERT_EQ_FLOAT` therefore cannot distinguish "correct value" from
"degenerate value", which is the single case such an assertion is most often
reached for.

This is not theoretical for this repo. Float degeneracy is a recurring, expensive
failure mode here — ZM-D-179 and ZM-D-183 are both authored-transform drift, the
Dawnmere placement headers carry an explicit `fZM_..._UNMEASURED` sentinel chosen
to be finite *precisely so no accessor can hand a NaN to a transform*, and
`ZM_Tests_DawnmerePlacement.cpp` defends its W5 units with standalone
`std::isfinite` sweeps. So the convention of adding a separate finiteness clause
already exists in the places somebody thought about it. Nothing enforces it, and
nothing tells an author that the assertion they reached for does not do what its
name suggests.

Found while adding coverage for ZM-20's `ZM_GetDawnmereFromRoute1SpawnFeet()`:
the new unit had to open with an explicit finiteness clause *before* its three
exact-equality clauses, or a NaN would have satisfied all three.

**The blast radius is the interesting part, and it is why this is not a one-line
fix.** Making the macro reject NaN will red every test currently passing on one.
Each of those is either a latent defect this ticket has just surfaced, or a test
that deliberately compares NaN and needs re-writing to say so. Both outcomes are
wins; neither should be silently absorbed by loosening the fix.

Note the *non*-float `ZENITH_ASSERT_EQ` is unaffected — it compares with `==` on
its operand type and has no epsilon.

## Definition of Done

- [ ] `ZENITH_ASSERT_EQ_FLOAT` FAILS when either operand is NaN, at every
      epsilon including `0.0f`, and its message says NaN was the reason rather
      than reporting a magnitude comparison
- [ ] The same treatment is applied to any sibling float comparison in the test
      framework that shares the `fabs(a - b) > eps` shape, or each survivor is
      recorded with a reason it does not need it
- [ ] A test in the framework's own suite asserts the new behaviour in both
      directions — a NaN on either side fails, and a genuine near-equal pair
      inside epsilon still passes
- [ ] Every test the change reds is triaged in the work log as either a latent
      defect now caught or a deliberate NaN comparison rewritten to say so; none
      is fixed by widening the epsilon or reverting the guard
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run

## Comments

ran on opus/medium via MODERATE+HIGH · category Engine

**Done** — merged to `master` as `49bc4a2b`. All 17 gate lines exit 0.

`ZENITH_ASSERT_EQ_FLOAT` and `ZENITH_ASSERT_NEAR_VEC3` both tested `fabs(a - b) > eps`, which is unreachable when either operand is NaN, so both passed unconditionally on a degenerate float at every epsilon including `0.0f`. Both now test NaN first and say so in the failure message; the finite path is byte-identical. `AssertNearVec3` checks per component. Infinity behaviour unchanged.

+2 backend-neutral engine units. Pins **all three OBSERVED, none computed**: Combat 2695→2697, Zenithmon 4548→4550, RenderTest 2798→2800, zero failures in each.

**Nothing redded**, so DoD item 4's triage set is empty — on the paths these three suites execute. Each run also skipped 1–2 tests, so this is not a statement about all ~2650 call sites.

Durable findings: `ZEN-D-001` (a `Zenith_AssertCaptureScope` test that asserts its hit count inside the scope is fail-open — the repo's current convention, including the file cited as the worked example), `ZEN-D-002` (`ZENITH_ASSERT_NE` is the one comparison assertion of six that fails OPEN on NaN, deliberately, now documented).

Review: no blocking findings, 8 non-blocking. Three were against the orchestrator's own prose and are fixed in this commit.

Excursion outside the worker's file list: `Docs/Roadmap_Engine.md` (ZEN-7's checkbox → `[x]`, so `zagent board status` does not report drift), plus `Tools/unit_baselines.json` and `Games/Zenithmon/Docs/Status.md`, which are the orchestrator's by protocol.

<!-- agent:worklog:start ZEN-7 -->
## Work Log — 2026-09-19

**Outcome**: merged to `master` as `49bc4a2b` · ran on opus/medium via MODERATE+HIGH · category Engine

**What changed**

- `Zenith/Core/Zenith_TestFramework.cpp` — `AssertEqFloat` and `AssertNearVec3` evaluate a NaN predicate first and OR it into the failure condition, with a branched diagnostic naming NaN and the offending operand. The finite path's format strings and argument lists are byte-identical to before (reviewer confirmed character by character). `AssertNearVec3` tests all three components of both vectors.
- `Zenith/Core/Zenith_TestFramework.h` — documentation only. Records the new contract and the DoD-item-2 survivors.
- `Zenith/Core/Zenith_UnitTests.Tests.inl` — **+2 units**, `Core::AssertEqFloatFailsOnNaN` and `Core::AssertNearVec3FailsOnNaN`, plus a `volatile`-routed `TestFrameworkRuntimeNaN()` helper so the value under test is a runtime one.
- `Tools/unit_baselines.json` — the three pins (orchestrator's; see Deviations).
- `Games/Zenithmon/Docs/Status.md` — LIVE PIN block + narration (orchestrator's).
- `Docs/Roadmap_Engine.md` — ZEN-7's checkbox flipped to `[x]` (orchestrator's; **excursion**, see Deviations).

**Files** (`git diff --stat`)

```
 Docs/Roadmap_Engine.md                 |   2 +-
 Games/Zenithmon/Docs/Status.md         |  27 +++++-
 Tools/unit_baselines.json              |   6 +-
 Zenith/Core/Zenith_TestFramework.cpp   |  71 +++++++++++++--
 Zenith/Core/Zenith_TestFramework.h     |  32 +++++++
 Zenith/Core/Zenith_UnitTests.Tests.inl | 156 +++++++++++++++++++++++++++++++++
```

**Gates**

`-Phase measure` exit **10** (pin needed bumping) → pins written → `-Phase verify` exit **0**, `state: complete`, `headSha` matching HEAD, `skipped: []`. 17 lines, all exit 0.

| Command | Result |
|---|---|
| `build engine` | 0 |
| `build Combat --headless` | 0 |
| `build Combat --config D3D12_..._False` | 0 |
| `build Zenithmon / CityBuilder / RenderTest / TilePuzzle / DevilsPlayground / ScriptTest / Foundry / Undervault --headless` | 0 (8 lines) |
| `test Combat --headless` | 0 |
| `run_unit_gate -Game Combat` | 0 — **2697 ran, 0 failed** |
| `build Zenithmon --config D3D12_..._False` | 0 *(unioned in — category Zenithmon)* |
| `test Zenithmon --headless` | 0 *(unioned in — category Zenithmon)* |
| `run_unit_gate -Game Zenithmon` | 0 — **4550 ran, 0 failed** *(unioned in — category Zenithmon)* |
| `doc_lint.ps1 -Game Zenithmon` | 0 *(unioned in — category Zenithmon)* |

The last four were added by `zagent gates`' union because the diff reached `Games/Zenithmon/Docs/**`. That is what certified the Zenithmon pin, closing a reviewer finding (below) without a hand re-run.

RenderTest's pin has **no gate line in any category** — it is an `advisory` row in `unit_baselines.json`. It was measured by hand from its own `Null_` exe (2800 ran, 0 failed) and is therefore the one number here not covered by an automated check.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith_TestFramework.cpp` | Both branches executed. The NaN branch by the two new units directly; the finite branch by every one of the ~2650 existing `ZENITH_ASSERT_EQ_FLOAT`/`NEAR_VEC3` call sites across all three suites (2697 + 4550 + 2800 units, 0 failures) — which is also what proves the finite path did not regress. |
| `Zenith_UnitTests.Tests.inl` | The two new units ARE the coverage; they ran in all three suites. |
| `Zenith_TestFramework.h` | **Nothing executes it** — comment-only. Compiled by all 11 builds and both `D3D12_*_False` legs. |
| `Tools/unit_baselines.json` | `run_unit_gate.ps1` reads it on gate lines 13 and 16, both exit 0 against the new numbers. |
| `Games/Zenithmon/Docs/Status.md` | `doc_lint.ps1 -Game Zenithmon`, exit 0. **It does not read prose claims** — it checks registry counts, roadmap ids, superseded markers, file-existence claims, links and pin equality. The narrative accuracy of this block rests on the review, not on a gate. |
| `Docs/Roadmap_Engine.md` | **Nothing in this gate list reads it.** `doc_lint` is Zenithmon-scoped, and `zagent board status --project ZEN --roadmap Roadmap_Engine.md` is not a gate line. UNCOVERED. |

**Definition of Done**

- [x] `ZENITH_ASSERT_EQ_FLOAT` FAILS when either operand is NaN, at every epsilon including `0.0f`, and its message says NaN was the reason rather than reporting a magnitude comparison
- [x] The same treatment is applied to any sibling float comparison in the test framework that shares the `fabs(a - b) > eps` shape, or each survivor is recorded with a reason it does not need it — `AssertNearVec3` was the one sibling and is fixed. The six template comparisons are the survivors; their recorded reason was corrected after review, because five fail CLOSED on NaN but `ZENITH_ASSERT_NE` fails OPEN and the original reason did not say so (`ZEN-D-002`).
- [x] A test in the framework's own suite asserts the new behaviour in both directions — a NaN on either side fails, and a genuine near-equal pair inside epsilon still passes
- [x] Every test the change reds is triaged in the work log as either a latent defect now caught or a deliberate NaN comparison rewritten to say so; none is fixed by widening the epsilon or reverting the guard — **nothing redded**, so the triage set is empty. See the caveat under Deviations: that is a statement about the paths these three suites execute, not about all ~2650 call sites.
- [x] Every affected game's pinned baseline bumped from an OBSERVED run — Combat 2695→**2697**, Zenithmon 4548→**4550**, RenderTest 2798→**2800**, each observed from its own `Null_` run, none computed.

**Durable findings**: `ZEN-D-001`, `ZEN-D-002`

- `ZEN-D-001` — a `Zenith_AssertCaptureScope` test that asserts its hit count INSIDE the scope is fail-open, because `HandleFailure` swallows the checking assertion too. That is the shape most existing capture tests use, including the one cited to the worker as the worked example. Found by the worker; verified against `Zenith_AssertCapture.cpp`.
- `ZEN-D-002` — `ZENITH_ASSERT_NE` is the one comparison assertion of six that fails OPEN on NaN, deliberately, now documented at the macro.

**Deviations / follow-ups**

- **EXCURSION outside the file list**: the worker's list was `Zenith_TestFramework.cpp`, `Zenith_TestFramework.h`, `Zenith_UnitTests.Tests.inl`. The commit also changes `Tools/unit_baselines.json` and `Games/Zenithmon/Docs/Status.md` (both the orchestrator's by protocol — a pin is a measurement only a gate run produces) and **`Docs/Roadmap_Engine.md`**, which is a genuine excursion: the reviewer pointed out that file's rule is `[x]` = Done with gates green and pins bumped, so leaving it unticked would make `zagent board status --project ZEN` report drift the moment this ticket closed. The worker changed nothing outside its three files.
- **Corrections made to the ticket's own text before inlining it (I3).** The ticket cites `ZM-D-179` and `ZM-D-183`. Both are real and both are EVIDENCE that float degeneracy is expensive here; neither GOVERNS how `AssertEqFloat` must behave. They were inlined with that qualification rather than as binding judgements. Nothing else in the body had drifted — the quoted line 327 and the implied sibling both resolved exactly as written, which is unusual.
- **DoD items 4 and 5 were withheld from the worker** as orchestrator work (item 4 needs a gate run it cannot perform; item 5 is a measurement). Its contribution to item 4 was a static survey, which found no wrapper macro anywhere and ranked the call sites that feed a NaN into an epsilon assertion. None of them redded.
- **The "nothing redded" result is narrower than it looks.** Each of the three runs also reported 1–2 SKIPPED tests, and a call site that would red only on a path no unit exercises is invisible to this result. The honest claim is that no NaN reached an epsilon assertion on the paths these suites execute. `Status.md` was corrected to say that after the review caught the stronger version.
- **Review findings applied** (no blocking findings; 8 non-blocking). Three were against the orchestrator's own text and are fixed in this commit: a stale `Last updated: 2026-09-14` banner three lines above a `LIVE PIN (UPDATED 2026-09-19)`; the overreaching causal claim above; and the survivor comment that declared `ZENITH_ASSERT_NE` safe without noting it fails open. A fourth — that Zenithmon's 4550 had no surviving artifact, because `unit_gate_boot.log` is single-slot and the RenderTest run had overwritten it — was answered by re-running the ZM gate against the bumped pin (PASS, log archived to `.zagent/run/ZEN-7/unit_gate_zenithmon.log`) and then independently by verify's unioned gate line 16.
- **Open, not done here**: sweeping the existing `Zenith_AssertCaptureScope` tests onto the non-fail-open shape (`ZEN-D-001`) is a separate diff across many test files. `Games/Zenithmon/CLAUDE.md:239` still quotes the old pins in prose and `:226` quotes a unit count that already disagreed with the pin before this ticket — pre-existing drift in a file this diff does not touch. And `Zenith/UnitTests/Zenith_AssertCapture.cpp:8-10` has an EMPTY `if (m_bPrevActive) { }` block where the header documents that a nested scope "logs an error"; it logs nothing. All three are out of scope for ZEN-7.
- **Tooling gaps hit while running this tick** (filed separately as suggestions, not fixed here, since `.claude/**` and `zagent.project.json` are protected): `build_prompt.ps1` emits no blast-radius survey clause although the protocol mandates it as the worker's first instruction; the `Engine` category declares no `decisionLog`/`decisionIdPrefix`, so the `ZEN-D-*` series above was established by this tick rather than following an existing convention.
<!-- agent:worklog:end -->
