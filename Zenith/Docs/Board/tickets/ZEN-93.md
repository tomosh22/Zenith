---
id: "aee34c73-d26e-422c-9a3a-7f026f8d2719"
key: "ZEN-93"
type: "BUG"
title: "An unregistered --automated-test name exits 0 and reports nothing, so a stale capture reads as fresh evidence"
category: "Engine"
parent: null
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 93
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T16:41:13.239Z"
flagged: false
severity: "S2_MAJOR"
reproducibility: null
dueDate: null
environment: null
claimAttempt: null
---
## Goal

`--automated-test <name>` with a name that matches no registered test **exits 0,
writes nothing, and reports nothing at all.**

MEASURED on 2026-09-19: `devilsplayground.exe --automated-test
MaterialEditorLivePreview --skip-unit-tests` (the real name is
`Test_MaterialEditorLivePreview`) ran to completion, exited 0, and the output
contains zero occurrences of "unknown", "not found", or the name passed.

The failure mode this creates is worse than a silent no-op. The harness in
question writes its captures to fixed paths (`C:/tmp/mat_shot_*.tga`), so the
run left **three-week-old images from a previous session** sitting at exactly the
paths a caller would collect. A tick taking BEFORE/AFTER evidence and trusting
the exit code would have archived those as its baseline and diffed them against
a fresh AFTER, producing a large bogus difference on a path nothing had changed.
It was caught only by checking file mtimes.

This is the same family as the repo's existing
`--exit-after-frames is INERT without --automated-test` gotcha. Note that the
tick protocol's standing advice — "read the authoring log line, not the exit
code" — does not rescue this case, because there is no log line to read.

## Definition of Done

- [ ] A run naming an unregistered automated test FAILS, with a non-zero exit
      code distinguishable from a test that ran and failed
- [ ] It logs the name it was given and, where the registry makes it cheap, the
      closest registered names
- [ ] The same treatment covers any sibling selector that silently accepts an
      unmatched name (`--filter` and friends), or each survivor is recorded with
      a reason it does not need it
- [ ] A unit covers the unmatched-name path so the fail-open cannot return
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run

## Comments

ran on sonnet/low via SIMPLE+MEDIUM · category Engine

**Done** — merged to `master` as `32006579`. Verify: 18 lines, exit 0, 0 skipped,
0 flakes. Guard clean.

**The bug in the Goal was already fixed; the thing the DoD asks for was not.**
Reproduced the exact command first: it exits **2** and prints
`not found in registry`. `--automated-tests` refuses per name the same way, and
the runner's `-Filter` already throws on zero matches. So DoD 1–3 were done.

**DoD 4 was the real gap.** The only unit,
`FindNodeByNameMissesCleanly`, pins `FindNodeByName` returning null — **delete
the `ExitHarness(2)` call and it still passes.** The refusal was held by nothing.
`ExitHarness` is `[[noreturn]]` and calls `std::exit`, so a unit can never reach
it; the decision is now extracted into `BuildSingleTestResolution`, a returnable
`{ m_bFound, m_iExitCode, m_acMessage }`, and the call site only prints and
exits. **+7 units**, each with a named mutation it catches.

DoD 2's second half is implemented too — `IsCloseNameMatch` +
`CollectCloseNameSuggestions`, capped at 5, printed only when the registry holds
a near match, so a genuinely bogus name's output stays byte-identical. The
measured failure is the argument for it: the typed name differed from the real
one only by a missing `Test_` prefix.

**Pins, all OBSERVED:** Combat 2718 → **2725**, Zenithmon 4571 → **4578** (both
gate-verified in the verify phase), RenderTest 2821 → **2828** (hand run;
advisory row). All +7. **The worker reported "+8"** — it had grouped three
matcher tests under one bullet. The gate asserts `ran == baseline` exactly, so
taking its number would have redded a required check with zero failing tests.

**A fragility fixed in the worker's tests:** its registry fixture borrowed
`Test_MaterialEditorLivePreview`, a REAL registered DevilsPlayground test, and
asserted an exact match count of 1 — which is 2 in DP's registry and passed
elsewhere only by accident of which game booted it. Renamed to a token no game
registers; the realistic case stays pinned in the pure matcher unit.

**Residual gap, recorded rather than overclaimed:** the three-line call-site
wiring is unreachable from any unit, so an edit that forgot `ExitHarness` or
inverted the check would pass everything here and reproduce this ticket's exact
symptom. Only a process can observe it. Filed as **ZEN-106**, split because
wiring such a check into a gate list edits a protected path.

Review returned **no blocking findings** and caught that the old code passed a
null name straight to `printf`'s `%s` — undefined behaviour the extraction
removes. One of its claims I did not accept: it called the suggestions scope
creep, but DoD 2 asks for them in terms.

Durable finding: **`ZEN-D-032`**.

<!-- agent:worklog:start ZEN-93 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+MEDIUM · category Engine

**Outcome**: merged to `master` as `32006579` · ran on sonnet/low via SIMPLE+MEDIUM

**The bug in the Goal is already fixed; the thing the DoD asks for was not.**
Reproduced the ticket's exact command before dispatching anything
(`Null_vs2022_Debug_Win64_True`):

```
devilsplayground.exe --automated-test MaterialEditorLivePreview --skip-unit-tests
  -> EXIT 2
  -> ERROR: --automated-test 'MaterialEditorLivePreview' not found in registry.
```

| DoD | State on arrival |
|---|---|
| 1. Non-zero exit, distinguishable from ran-and-failed | **Already done** — `0` passed / `1` ran-and-failed / `2` unmatched / `3` infrastructure. Confirmed 2 by running it. |
| 2. Logs the name + closest registered names | **Half done** — it quotes the name; suggestions did not exist. |
| 3. Sibling selectors | **Already done** — `--automated-tests BogusA,BogusB` also exits 2 with a per-name error; the runner's `-Filter` throws `no tests discovered` (`ZenithTestHarness.psm1:280`). Both verified by running / reading. |
| 4. A unit so the fail-open cannot return | **THE GAP.** |
| 5. Pin bump from an OBSERVED run | follows from 4 |

**Why item 4 was the real work.** The only unit,
`FindNodeByNameMissesCleanly`, asserts `FindNodeByName` returns null for an
unknown name. **Delete the `ExitHarness(2)` call and it still passes.** It pins
the lookup, not the refusal — so the fail-open could have returned in full with
the one unit written to prevent it still green.

`ExitHarness` is `[[noreturn]]` and calls `std::exit`, so a unit can never invoke
it. The decision is now extracted into `BuildSingleTestResolution`, a plain
returnable `{ m_bFound, m_iExitCode, m_acMessage }`; the call site only prints and
exits.

**What changed**

- `Zenith/EngineComposition/Zenith_AutomatedTest.cpp` — `IsCloseNameMatch`
  (pure, case-insensitive, either-direction substring — deliberately not edit
  distance), `CollectCloseNameSuggestions` (registry walk, capped at 5),
  `SingleTestResolution` + `BuildSingleTestResolution`; the miss branch reduced
  to print + `ExitHarness`.
- `Zenith/EngineComposition/Zenith_AutomatedTest.Tests.inl` — **+7 units**, new
  `AutomatedTestSelection` suite.
- `Tools/unit_baselines.json` — three pins.
- `Games/Zenithmon/Docs/Status.md` — LIVE PIN + narration.

**Gates** — verify phase, 18 lines, **exit 0, 0 skipped, 0 deferred, 0 flakes**.
`zagent guard`: ok, selection current, verdict fresh. Measure exited **10** as
expected (a pin needed bumping).

**The pins, all OBSERVED, none computed**

| Game | Was | Now | Source |
|---|---|---|---|
| Combat | 2718 | **2725** | gate unit line, then re-verified green in verify |
| Zenithmon | 4571 | **4578** | hand `--exit-after-unit-tests`, then **gate-verified in verify** |
| RenderTest | 2821 | **2828** | hand `--exit-after-unit-tests`; advisory row, no gate reads it |

Zenithmon became gate-verified because editing `Games/Zenithmon/Docs/Status.md`
unioned the Zenithmon category in, which carries its own unit-gate line. The
Engine list alone gates Combat only.

**★ THE WORKER REPORTED "+8". IT IS +7, AND THE MEASUREMENT IS WHY THAT MATTERED.**
The diff adds exactly 7 `ZENITH_TEST` lines and all three observed runs moved by
exactly 7; the report had grouped three matcher tests under one bullet and
miscounted its own work. The gate asserts `ran == baseline` EXACTLY, so writing
8 would have redded a required check with zero failing tests. This is the entire
reason a worker is told never to quote a pin.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith_AutomatedTest.cpp` | The 7 new units, in every game's boot suite — they ran in Combat, Zenithmon and RenderTest observations. **Except the call-site wiring**, which no unit reaches; see the residual gap below. |
| `Zenith_AutomatedTest.Tests.inl` | Itself. Mutation-checked: deleting `m_iExitCode = 2` or the message construction turns `BuildSingleTestResolutionUnmatchedIsExitTwoWithMessage` red; deleting the found-path early return turns `BuildSingleTestResolutionFoundIsExitZero` red. |
| `Tools/unit_baselines.json` | The unit gates themselves — Combat and Zenithmon both re-ran green against the new numbers. |
| `Games/Zenithmon/Docs/Status.md` | `doc_lint.ps1 -Game Zenithmon` C7, which reconciles the LIVE PIN against the manifest. Ran green in verify. |

**Definition of Done**

- [x] A run naming an unregistered automated test FAILS, with a non-zero exit
      code distinguishable from a test that ran and failed — already true;
      re-verified by running it (exit 2 vs 1).
- [x] It logs the name it was given and, where the registry makes it cheap, the
      closest registered names — the name half was already true; the suggestion
      half is new (`IsCloseNameMatch` + `CollectCloseNameSuggestions`, capped at
      5, printed only when the registry actually holds a near match so a
      genuinely bogus name's output is byte-identical to before).
- [x] The same treatment covers any sibling selector — `--automated-tests`
      already refuses per name with exit 2; the runner's `-Filter` already
      throws. Both verified rather than assumed; nothing was needed.
- [x] A unit covers the unmatched-name path so the fail-open cannot return —
      7 units, each with a named mutation it catches. **Ticked with a stated
      limit**: they pin the DECISION, not the call-site wiring, which no unit can
      reach. See below; filed as ZEN-106 rather than left implicit.
- [x] Every affected game's pinned baseline bumped from an OBSERVED run — all
      three above, each from its own `Null_` run.

**Durable findings**

- `ZEN-D-032` — a unit that pins a LOOKUP does not pin the REFUSAL it feeds, and
  a `[[noreturn]]` call site is what pushes you into testing the easy half.
  Extract the decision as a returnable value; ask what mutation each unit
  catches; and never borrow a real registered name for a fixture.

**Deviations / follow-ups**

- **A fragility I found and fixed in the worker's tests.** Its registry test
  spliced a fixture named `Test_MaterialEditorLivePreview` — a REAL registered
  DevilsPlayground test — and asserted the match count is exactly 1. In DP's
  registry the walk finds two; it passes in Combat and Zenithmon only because
  neither registers that name, and only because no gate runs DP's units. Renamed
  the fixture to a token no game registers; the realistic `Test_`-prefix case
  stays pinned in the pure matcher unit, which touches no registry.
- **Residual gap, recorded not papered over (ZEN-106).** The three-line call-site
  wiring is unreachable from a unit. An edit that forgot `ExitHarness` or
  inverted the check would pass all 7 units and reproduce this ticket's exact
  symptom. Only a process can observe it. Split out because wiring such a check
  into a gate list edits `zagent.project.json`, a protected path.
- **A review claim I did NOT accept.** The reviewer called the "Did you mean"
  suggestions scope added beyond the DoD, "which was about pinning the existing
  refusal with a test". DoD item 2 says in terms: *"and, where the registry makes
  it cheap, the closest registered names."* They are required by the ticket.
- **A residual risk the reviewer disclosed, mostly closed empirically.** The unit
  asserting NO suggestion appears for `Zenith_NoSuchAutomatedTest_ZZZ` depends on
  no registered name colliding with that token. It could only check statically
  and left Zenithmon and RenderTest open — but both booted their full suites
  during the pin observations and reported **0 failed**, so the units demonstrably
  pass in all three pinned registries. I also dumped DP's 165 names and confirmed
  no collision (shortest `Hello_Test`). ScriptTest, TilePuzzle, Foundry and
  Undervault remain unchecked; no gate runs units for any of them.
- **A stale claim corrected in `Status.md`.** A paragraph there stated the Engine
  gate list "carries no `doc_lint` line at all, for any game". It now carries two
  (`ZEN-D-006`), and this ticket's own engine-only diff resolved both. Corrected
  with a dated note; the reasoning above it still holds and is kept.
- **The old code had undefined behaviour the extraction removed**: a null
  requested name went straight to `printf`'s `%s`. Found by the review.
<!-- agent:worklog:end -->
