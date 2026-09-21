---
id: "b700418d-1b50-4f43-a8a2-ee054ef5f4e6"
key: "ZEN-75"
type: "TASK"
title: "zagent writes .zagent/last.exit on every exit path"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 75
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T06:34:50.148Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
Suggestions row 2026-08-26 (cli): *"Write zagent's exit code to .zagent/last.exit on every exit path, so the protocol's most-broken rule becomes a file read like every other rule."* The client's usage line already names `.zagent/last.exit` as scratch, and the tick protocol branches on exit codes (3, 4, 5, 6, 7), so a path that exits without writing the file leaves the tick reading a stale code.

Audit every exit path in `Tools/Vault/cli/client.mjs` and `zagent.mjs` (success, usage error, refusal, thrown error, unreachable board 7) and write `last.exit` in one place (a `finally` around dispatch); Vitest asserts the file for each exit class including a thrown error.

## Definition of Done
- [ ] Every exit path writes `.zagent/last.exit`, proven by tests for each exit class
- [ ] The Suggestions row is resolved with this key


## Comments

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Done.** Committed to `master` as `8a585a8a`.

**The `client.mjs` half was already complete** — `scratchRoot` resolved before any branching,
`markRunning` up front, `finish()` as the single choke point, the dispatch in a `try/catch` so
a throw becomes a result rather than escaping. Verified from the code, not from the cited
Suggestions row (which was already `Status: done` describing it). `client.mjs` is not in this
commit.

**The real gap was the other file the ticket names.** `Tools/Vault/cli/zagent.mjs` — the
`npm run vault` entry point — wrote `last.json` and **never wrote `last.exit` on any of its six
exit paths**, including its thrown-error catch, so a tick reading that file after such a call
got whatever the previous `zagent` invocation left. Closed with one `persistExit` helper
mirroring `client.mjs`, all six returns routed through it, no control flow restructured. The
one path that genuinely cannot record (no `--vault`, so no `.zagent/`) is documented as the
exception rather than left silent.

Eight tests: six exit classes for `zagent.mjs`, plus the two the ticket named as missing for
`client.mjs` — a **thrown** dispatcher error and a **non-1 refusal code**. No exit-7 test:
`grep` finds no such code anywhere in `Tools/Vault/cli`, so writing one would invent coverage
for a code the CLI cannot produce.

**Both halves mutation-checked**, because a passing test is not a covering test: gutting
`zagent.mjs`'s recorder reds exactly the five cli-entry cases that assert a code (the
missing-`--vault` one correctly stays green), and making a thrown error escape `client.mjs`
reds exactly one tick-checks case.

**★ Read the gate story before trusting the commit.** `-Phase verify` was run three times and
never returned 0. Every sample failed on the `Tools/Vault` `npm test` line and **each named a
different test** (6.8 s, 16.1 s, 17.8 s — all among the slowest in the suite); sample 3's
rerun went green. I proved the change innocent rather than asserting it:
`cli-process.test.mjs` fails **0/4 in isolation with the change and 0/4 with `zagent.mjs`
reverted to HEAD**, `settings-actions` passes 3/3 alone, and the total lands at exactly
**822 = the 814 baseline + the 8 new tests**. One suspect deserved real scrutiny — *"keeps help
read-only"*, against a change that makes the help path write — but its assertion is a payload
field, not a filesystem check.

A red **skips every line behind it**, so all five `doc_lint` invocations were skipped in all
three samples. I ran them by hand: **exit 0 on all five games**. Every one of the 12 gate lines
has now been observed green, though never all within one phase. `zagent guard` clean, zero
excursions.

Recorded as `ZEN-D-024`, with the discriminator: a regression fails the *same* test every run,
so different tests across consecutive runs means look at load — and confirm by running the
suspect in isolation with and without the change. Note that the protocol's "red → red is a real
failure" rule is written for the same line failing twice; applied mechanically here it would
have Blocked a provably innocent ticket.

Second instance of `ZEN-D-023`: closing this invariant in the second writer deepens the
two-entry-point duplication rather than resolving it.

<!-- agent:worklog:start ZEN-75 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Outcome**: committed to `master` as `8a585a8a` · ran on sonnet/low via SIMPLE+LOW

**★ The `client.mjs` half was already complete, and the cited Suggestions row was already
resolved saying so** — the third consecutive ticket in this shape. Verified against the code
rather than the row: `scratchRoot` resolved before any command branching, `markRunning`
invalidating the slot up front, `finish()` as the single choke point every return funnels
through, the dispatch wrapped in `try/catch` so a throw becomes a result rather than escaping,
and `finish` itself retrying if `persistLast` throws. The Goal's suggested "a `finally` around
dispatch" is satisfied by an equivalent structure. **`client.mjs` is untouched by this commit
— it is not in the changed set.**

**The real gap was the other file the ticket names.** `Tools/Vault/cli/zagent.mjs` — the
`npm run vault` entry point, documented in `package.json`, `Tools/Vault/README.md` and the e2e
spec — wrote `last.json` and **never wrote `last.exit` on any of its six exit paths**,
including its thrown-error catch. A tick reading `.zagent/last.exit` after one of those calls
read whatever the previous `zagent` invocation left: exactly the stale-code failure this
ticket exists to remove.

**What changed**

- `Tools/Vault/cli/zagent.mjs` — one `persistExit` helper mirroring `client.mjs`'s `finish()`,
  `scratchRoot` resolved once before any branching, all six returns routed through it. No
  control flow restructured. The missing-`--vault` refusal cannot record (there is no
  `.zagent/` to write into) and is routed through anyway for uniformity, commented as the one
  documented exception rather than left as a silent hole.
- `Tools/Vault/test/cli-entry.test.mjs` — six exit classes for `zagent.mjs`.
- `Tools/Vault/test/tick-checks.test.mjs` — the two classes the ticket named as missing for
  `client.mjs`, added to the existing "every exit records, nothing stale survives" block: a
  **thrown** dispatcher error and a **non-1 refusal code** (5), verified to land verbatim.

**No exit-7 test, deliberately.** The Goal lists "unreachable board 7" as an exit class;
`grep` finds no exit code 7 anywhere in `Tools/Vault/cli/*.mjs`. There is no network hop left
since the Vault became this checkout. Writing it would invent coverage for a code the CLI
cannot produce.

**Gates** — this needs reading in full, because the phase never returned 0.

`-Phase verify` was run three times. Every sample failed on the **`Tools/Vault` `npm test`**
line, and **each named a different test**:

| sample | failing test(s) | duration | rerun |
|---|---|---|---|
| 1 | `cli-process.test.mjs > …keeps help read-only` | 6823 ms | red |
| 2 | `settings-actions.test.mjs > …preserve imported rows…` | 16102 ms | red |
| 3 | `…catalog families` + `cli-process.test.mjs` | 17821 / 7052 ms | **green** |

Sample 3 is literally red→green, which the protocol defines as a flake and says to continue
past. The other two are the shape the protocol has no term for — red→red on *different*
tests.

**I proved the change innocent rather than arguing it.**

- `cli-process.test.mjs` in isolation: **0/4 failures with the change, 0/4 with
  `zagent.mjs` reverted to HEAD**.
- `settings-actions.test.mjs` in isolation: **3/3 pass**.
- Suite total **822 = the 814 baseline + exactly the 8 new tests**, with 820–821 passing in
  every sample. No test disappeared.
- One suspect was worth real scrutiny and got it: *"keeps help read-only"* is uncomfortably
  close to a change that makes the help path **write** `last.exit`. Its assertion is
  `help.payload.readOnly === true` — a payload field, not a filesystem check — so the change
  cannot violate it.

Recorded as `ZEN-D-024`, including how to tell this apart from a regression.

**A red skips every line behind it**, so all five `doc_lint` invocations were skipped in each
of the three samples. I ran them by hand rather than gamble on a fourth six-minute roll:

| gate line | result |
|---|---|
| `Test_TickGates.ps1`, `Test_UnitBaselineManifest.ps1`, `Test_T0Harness_…`, `Test_DocLintKeyCitations.ps1` | PASS (all three samples) |
| `Test-ZagentClient.ps1`, `Test-BuildPrompt.ps1` | PASS (all three samples) |
| `Tools/Vault` `npm test` | **PASS on sample 3's rerun**; 820–821/822 in every sample, failures load-dependent |
| `doc_lint.ps1` × 5 games | **PASS — run by hand, exit 0 each** (Zenithmon, DevilsPlayground, Foundry, Undervault, Hearth) |
| **hand run** `vitest run test/cli-entry.test.mjs test/tick-checks.test.mjs` | **42/42** |

So every one of the 12 gate lines has been observed green, though never all in one phase.

`zagent guard ZEN-75`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `cli/zagent.mjs` (`persistExit`) | The six new `cli-entry.test.mjs` cases, which the `npm test` gate line runs. |
| `test/cli-entry.test.mjs`, `test/tick-checks.test.mjs` | The `npm test` gate line. |

★ **Mutation-checked, both halves**, because a passing test is not a covering test:

| mutation | result |
|---|---|
| gut `zagent.mjs`'s recorder (`if (scratchRoot)` → `if (false)`) | **5 of 12** cli-entry tests red — exactly those asserting a recorded code; the missing-`--vault` case correctly stayed green |
| make a thrown dispatch error escape `client.mjs` (catch → rethrow) | **exactly 1 of 30** tick-checks tests red — the new thrown-error case |

**Definition of Done**

- [x] Every exit path writes `.zagent/last.exit`, proven by tests for each exit class — six
      classes for `zagent.mjs` plus the two missing `client.mjs` classes, all mutation-checked.
      The one path that cannot record (no `--vault`, so no `.zagent/`) is documented as such
      rather than papered over. The ticket's seventh class, exit 7, does not exist.
- [x] The Suggestions row is resolved — **already was, and I did not re-resolve it**, for the
      reason recorded as `ZEN-D-022`: its Resolution accurately describes the `client.mjs`
      implementation, and overwriting it with a `zagent.mjs` narrative would destroy a better
      record than the replacement.

**Durable findings**: `ZEN-D-024`

- `ZEN-D-024` — the Vault suite cannot currently produce a reliable green under the full-suite
  run on this machine; four distinct slow tests were observed timing out across five runs in
  one session, each passing in isolation and in the runs where the others failed. Includes the
  discriminator: a regression fails the *same* test every time, so different tests across
  consecutive runs means look at load, and confirm by running the suspect in isolation with
  and without the change.

**Deviations / follow-ups**

- **Zero excursions.** All three changed files are on `filelist.txt`.
- **The protocol's flake rule does not cover this shape, and applying it mechanically would
  have been wrong.** It says "red → red is a real failure. Now fix forward… on the _same_
  failure." Samples 1 and 2 were red→red on *different* failures, and fix-forwarding would
  have meant editing a change I had already shown, by isolation runs in both directions, could
  not be the cause. I proceeded on the evidence and documented it rather than following the
  letter into a wrong Block. Worth a protocol amendment; filed as part of `ZEN-D-024`.
- **This is the second instance of `ZEN-D-023`** (two CLI entry points, divergent guarantees).
  Closing the `last.exit` invariant in the second writer deepens the duplication rather than
  resolving it; converging or retiring `cli/zagent.mjs` remains the structural fix and is not
  this ticket.
- **An asymmetry the worker surfaced and did not hide:** `zagent.mjs`'s `scratchRoot` derives
  from the raw `--vault` value, while `persistMutationScratch` writes `last.json` under
  `authority` (possibly redirected for a registered worktree), so the two could in principle
  land under different roots for one invocation. This mirrors `client.mjs`'s existing shape
  and was copied deliberately per the instruction not to invent a second idiom; it is
  pre-existing and untouched.
- **Not checked by anyone:** whether any consumer of `zagent.mjs` actually reads
  `.zagent/last.exit` today (the `zagent` shim execs `client.mjs`, so the new writes may have
  no reader yet — they close an invariant rather than fix an observed misread); and whether
  the four flaky tests share a root cause beyond being slow.
<!-- agent:worklog:end -->
