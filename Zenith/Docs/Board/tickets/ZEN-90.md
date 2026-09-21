---
id: "1714567e-da89-4984-90b5-c3afc605caae"
key: "ZEN-90"
type: "BUG"
title: "zagent decide writes decision rows that zagent rows decisions cannot read"
category: "Engine"
parent: null
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "HIGH"
storyPoints: 5
position: 90
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T13:35:22.933Z"
flagged: false
severity: "S2_MAJOR"
reproducibility: null
dueDate: null
environment: null
claimAttempt: null
---
## Goal

`zagent decide` persists rows that `zagent rows decisions` cannot read. The two
commands resolve different stores.

MEASURED on 2026-09-19, filing four decisions across ZEN-7 and ZEN-12:

- all four calls returned **exit 0** with a `rowId`, and produced four
  `board: decide` commits
- all four ids (`ZEN-D-001` … `ZEN-D-004`) are present in `metadata.md`
- `zagent rows decisions`, with and without `--project ZEN`, returns only the
  same **7 pre-existing rows** and none of the four

The 7 readable rows carry `database_id: null` — a legacy direct knowledge
record. The four written rows went into the notion database titled
`Agent · Decisions` (`gm1dzh0b1o8u1p38sosvoco4`). So the write path takes the
notion-database branch while the read path returns the direct record.

**Why this matters beyond tidiness.** The tick protocol REQUIRES a
`Durable findings` answer in every terminal work log, naming either "none" or a
`PROJ-D-N` reference, and `zagent finish` refuses without one. But `finish`
validates only the SHAPE of that reference (a regex on `PROJ-D-N`), never its
existence. A tick can therefore satisfy the mandatory check with a reference no
documented read command can retrieve — which is the same "silence is illegal but
the answer is unchecked" hole the Durable-findings mechanism was built to close,
displaced one level up.

Secondary, lower priority: the first `decide` of the session also reformatted
`metadata.md` from pretty-printed to compact (9573 → 3645 lines, 638662 →
622129 bytes; byte delta consistent with newline and indent removal, not data
loss, and every later `decide` grew the file). It makes any `board:` commit's
diff unreviewable.

## Definition of Done

- [ ] A decision written by `zagent decide` is returned by `zagent rows decisions`
      in the same checkout, with and without `--project`
- [ ] The write path and the read path resolve the SAME store, and which one is
      stated in a comment where the resolution happens
- [ ] Pre-existing rows in the other store are either migrated or still readable —
      no decision that exists today becomes unreadable
- [ ] A test covers the round trip: write a decision, read it back by id, and
      fail if the read returns nothing
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit

## Comments

ran on opus/medium via MODERATE+HIGH · category Engine

**Done** — merged to `master` as `915ff411`, plus the data commit `9cb3712f`.

**Three of the five DoD boxes were already satisfied when this ticket was
claimed** (the write path was fixed 2026-09-20 in `fa8ad5b2`, after this ticket
was filed): the round trip works with and without `--project`, one resolution is
stated in a comment, and a round-trip test exists.

**DoD 3 was the open box and was real.** `ZEN-D-001..004` — four decisions filed
2026-09-19 against ZEN-7 and ZEN-12 — sat in the `acme-corp` fixture database,
readable by nothing. `ZEN-D-004` is load-bearing for the tick protocol's own
scene-authoring reasoning.

`Tools/Vault/scripts/adopt-stale-knowledge-rows.mjs` moves such rows into
whichever database the reader's own `databaseModel` resolves, re-keying values
**by property name**: the two databases share all nine names and share no
property id, so a `database_id`-only move would leave every cell keyed by an id
the destination does not define — the row lists correctly and reads empty.
It refuses rather than drops, validates inside `prepare` (ZEN-D-007) so a
refusal leaves the vault byte-identical, and goes through `executeOperation` +
`patchRecord` rather than serialising `metadata.md`.

**Run, not just written:** dry run (unnarrowed) planned 4 moves, all in
`decisions`; `--apply` moved 4; re-read shows 33 → **37 rows** with **9/9 fields
populated on each**; a second `--apply` moved 0 and made no commit; the stale
database is now empty.

**★ The verify phase records `exitCode: 4`, and this is still Done.** The
`Tools/Vault` vitest line went red then **green on the script's own re-run —
72/72 files, 829/829 tests**. `tick_gates.ps1` keeps the verdict RED on a
red→green flake by policy; the tick protocol's rule for that shape is to record
a Shortfalls row and continue, which is done. Four reasons it is not this change:
consecutive runs named DIFFERENT tests (the pre-change baseline named
`cli-process` + `settings-actions-refusals`; verify named `cli-process` +
`mutations`); both verify failures are named in `ZEN-D-024` with matching
durations; the baseline pair passed in isolation 28/28; and the green re-run's
829 is exactly the 826 baseline plus this ticket's 3 new tests, 0 failures. The
baseline was itself red on a clean tree (2 failed / 824 passed) and was measured
before dispatch so this comparison could be made rather than argued.

Three `doc_lint` lines were SKIPPED behind the red — Foundry, Undervault,
Hearth — and were run by hand afterwards: all three `ALL CHECKS PASS`. The
25-line gate list is fully accounted for. `zagent guard` passed
(`gateVerdictFresh` checks `state`/`headSha`, not the exit code, and is not
being used as a claim of green).

Review (risk HIGH + a DoD asserting something about the tests) returned **zero
blocking findings** and verified the result against the live `metadata.md` bytes.
Three of its non-blocking findings were applied: a fabricated fact in a committed
comment, a malformed `--database` that silently widened to all five knowledge
databases instead of refusing, and a name-collision that would have dropped a
property silently. It also corrected me: `ZEN-D-004`'s Category was **not**
transformed — the SELECT round trip was byte-lossless.

Durable finding: **`ZEN-D-029`**. Follow-up: **ZEN-104** (guard the migration's
direction; cover the SELECT path).

<!-- agent:worklog:start ZEN-90 -->
## Work Log — 2026-09-21

ran on opus/medium via MODERATE+HIGH · category Engine

**Outcome**: merged to `master` as `915ff411`, plus the data commit `9cb3712f`
· ran on opus/medium via MODERATE+HIGH

**Three of the five DoD boxes were already satisfied when this ticket was
claimed.** The Goal describes 2026-09-19; the write path was fixed on 2026-09-20
in `fa8ad5b2`. Verified before dispatching anything: `zagent decide` wrote
`ZEN-D-027` and `ZEN-D-028` earlier the same day and `zagent rows decisions`
returns both, with and without `--project`; `write.mjs:1407-1423` carries the
single-resolution comment and `knowledgeDatabase()` calls the reader's own
`databaseModel`; and `decide-reference.test.mjs:155` is
`test('ZEN-90: a decision written is a decision readable', …)`.

**The one open box was DoD 3**, and it was real. `metadata.md` holds exactly two
databases titled `Agent · Decisions`: `roqon5olpfp13m5xe4d9miq1` (org
`pink-goat`, live, 33 rows — what every read returns) and
`gm1dzh0b1o8u1p38sosvoco4` (org `acme-corp`, a migration fixture, 4 rows,
readable by nothing). Those four are `ZEN-D-001..004`, real decisions filed
2026-09-19 against ZEN-7 and ZEN-12. `ZEN-D-004` — *"ANY `--automated-test` boot
authors scenes, on EITHER backend"* — is load-bearing for the scene-authoring
reasoning in the tick protocol itself.

**What changed**

- `Tools/Vault/scripts/adopt-stale-knowledge-rows.mjs` (new) — moves rows of a
  same-titled non-live knowledge database into whichever one `databaseModel`
  resolves, re-keying values **by property name**. Dry run by default;
  `--apply` writes.
- `Tools/Vault/test/decide-reference.test.mjs` — 3 new tests; the existing
  fixture gained two opt-in knobs that default to the previous behaviour.
- `Tools/Vault/cli/write.mjs` — comment only, correcting two wrong row counts.
- `Tools/Vault/README.md` — **an excursion outside the file list, made
  deliberately**; see Deviations.

**Files** (`git diff --stat`)

```
 Tools/Vault/README.md                              |  17 ++
 Tools/Vault/cli/write.mjs                          |  12 +-
 Tools/Vault/scripts/adopt-stale-knowledge-rows.mjs | 345 +++++++++++++++++++++
 Tools/Vault/test/decide-reference.test.mjs         | 104 ++++++-
```

**The migration was run, not just written**

| Step | Result |
|---|---|
| Dry run, unnarrowed | 4 moves, **all in `decisions`**, refs `ZEN-D-001..004`, 9 properties each, tree unchanged |
| `--apply` | 4 moved; one commit `9cb3712f board: adopt-stale-knowledge-rows` |
| Re-read via `zagent rows decisions` | 33 → **37 rows**; all four present; **9/9 non-empty fields each** |
| Second `--apply` | moved 0, exit 0, **no new commit** |
| Stale database after | **0 rows**; live 37 |

The dry run was run **unnarrowed on purpose**: with `--database` omitted the
script plans over all five knowledge databases, and a move in any database other
than `decisions` would have been unmeasured territory. The plan showed 4 moves in
`decisions` and nothing else, which made the broad run provably equivalent to the
narrow one. The reviewer confirmed this independently from the file: the other
four stale databases' `database_id` occurrence counts equal their
property-definition counts exactly, so they held zero rows.

**Gates**

The diff touches `Tools/Vault/**`, so `zagent gates` unioned in the **Tooling**
category: 25 lines, up from the Engine category's 15. That is the union doing its
job — it added the vitest suite and the `Tools/zagent` assert scripts, which are
the only lines that execute any of this diff.

| Command | Result |
|---|---|
| 11 × `zenith build …` (incl. `D3D12_…_False`) | 0 |
| `zenith test Combat --headless` | 0 |
| `run_unit_gate.ps1 -Game Combat` | 0 — **2718 ran == 2718 baseline** |
| `Test_TickGates` / `Test_UnitBaselineManifest` / `Test_T0Harness_…` / `Test_DocLintKeyCitations` | 0 |
| `Test-ZagentClient.ps1` / `Test-BuildPrompt.ps1` | 0 |
| `doc_lint.ps1` — Zenithmon, DevilsPlayground | 0 |
| **`Set-Location Tools/Vault; npm test`** | **exit 1, then exit 0 on the script's own re-run — a FLAKE. See below.** |
| `doc_lint.ps1` — Foundry, Undervault, Hearth | **SKIPPED behind the red; run by hand, all three `ALL CHECKS PASS`** |

**★ THE VERIFY PHASE RECORDS `exitCode: 4`, AND THIS TICKET IS STILL BEING CLOSED
Done. The reasoning, in full, because it is the one judgement here that could be
wrong:**

`tick_gates.ps1` re-ran the red line automatically and got **exit 0 — 72/72 test
files, 829/829 tests**. Its policy is then `[flake] SECOND run was green --
recorded as a possible flake, verdict for this line stays RED (first run's exit
1)`. The tick protocol's own rule for that shape is the opposite: *"red → green
is a FLAKE. Do not treat it as a pass and do not treat it as a failure: record a
Shortfalls row naming the test, the exit code and both outcomes, say so in the
work log, and continue. It costs no fix-forward attempt."* The Shortfalls row is
filed; this is the work-log half.

Four independent reasons the red is not this change:

1. **Different tests on consecutive runs**, which is `ZEN-D-024`'s stated
   signature. The pre-change baseline named `cli-process.test.mjs` **and
   `settings-actions-refusals.test.mjs`**; the verify run named
   `cli-process.test.mjs` **and `mutations.test.mjs`**.
2. **Both verify-run failures are named in `ZEN-D-024` itself** — 6552 ms here vs
   6823/7052 ms there, and 17544 ms vs 17821 ms.
3. **The baseline pair passed in isolation** before any change landed: 28/28,
   exit 0.
4. **The green re-run totals 829 = the 826 baseline + this ticket's 3 new
   tests, with 0 failures.** Both the "nothing regressed" and "the new tests
   pass" claims come from that one number.

And the baseline itself was **already red on a clean tree** — 2 failed / 824
passed of 826 — measured before the worker was dispatched, precisely so this
comparison could be made rather than argued.

`zagent guard` returned `ok: true`, `gateSelectionStale: false`,
`gateVerdictFresh: true`. **Note that `gateVerdictFresh` checks `state` and
`headSha`, not the exit code** — a passing guard here is not a claim the gates
were green, and is not being used as one.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Tools/Vault/scripts/adopt-stale-knowledge-rows.mjs` | 3 new tests in `decide-reference.test.mjs` (round trip asserting VALUES under live property ids, idempotence, dry-run byte-identity, refusal-by-property-name), **plus a real run against live board data** — dry run, apply, re-read, second apply. The `--database` refusal paths were smoke-tested directly (`--database` as last arg → exit 1; `--database ,,` → exit 1; `--database decisions` → exit 0), reading the exit code without a pipe. |
| `Tools/Vault/test/decide-reference.test.mjs` | The vitest suite runs it; the green re-run's 829 total confirms all three new tests pass. |
| `Tools/Vault/cli/write.mjs` | **Comment-only.** Every vitest case that imports the module parses it, which is the real risk for this edit; no behaviour is exercised because none changed. |
| `Tools/Vault/README.md` | **Nothing.** `doc_lint.ps1` is scoped to `Games/<Game>/Docs`. Prose, verified by reading. |

★ The SELECT code path (`labelOf` / `cellFor`) has **no test**, and it is the path
that actually ran against live data. It was verified empirically instead — all
four rows re-read with 9/9 fields — and filed as **ZEN-104** for a proper test.

**Definition of Done**

- [x] A decision written by `zagent decide` is returned by `zagent rows decisions`
      in the same checkout, with and without `--project` — already true on
      arrival; re-verified with `ZEN-D-027`/`ZEN-D-028` (33 rows either way).
- [x] The write path and the read path resolve the SAME store, and which one is
      stated in a comment where the resolution happens — already true;
      `knowledgeDatabase()` imports the reader's `databaseModel`, and the comment
      at `write.mjs:1409-1430` now states it with corrected numbers.
- [x] Pre-existing rows in the other store are either migrated or still readable —
      no decision that exists today becomes unreadable — **this was the open box.**
      All four migrated; stale database emptied; each re-read with all nine
      fields populated.
- [x] A test covers the round trip: write a decision, read it back by id, and
      fail if the read returns nothing — already true at
      `decide-reference.test.mjs:155`; the reviewer confirmed the new round-trip
      test is **not vacuous** (a `database_id`-only move would fail it, because
      `dispatchRows` keys cells by the destination's property ids).
- [x] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit — the condition does not hold. No C++ changed
      and no engine unit moved; the OBSERVED `Null_` run reports Combat
      **2718 ran == 2718 baseline**. `Tools/unit_baselines.json` untouched. The
      +3 are vitest tests, which no pin counts.

**Durable findings**

- `ZEN-D-029` — two same-titled knowledge databases share property NAMES but no
  property IDS, so relocating a row by `database_id` alone is silent total data
  loss that still passes a row-count check; and `databaseModel`'s resolution is
  the only definition of "live", which makes a migration's direction a property
  of the checkout's config rather than of the script.

**Deviations / follow-ups**

- **An excursion, made deliberately and reported rather than reverted:**
  `Tools/Vault/README.md` is not in `filelist.txt`. The reviewer found the new
  script undocumented beside `scripts/recover.mjs`, which uses the identical
  dry-run-then-`--apply` convention, and the worker had declined to add it
  precisely because it was out of bounds. A tool that rewrites board data being
  absent from the one file that lists such tools is worth 17 lines; it unions in
  no new gate, since `Tools/Vault/**` was already pulling the Tooling category.
- **Three review findings applied by the orchestrator after the review, each
  re-certified by the verify phase that followed:**
  1. A **fabricated fact in a committed comment** — the script asserted the two
     databases' option ids "differ exactly as their property ids do". They do
     not: option ids *and* labels are identical here; only property ids differ.
     The code was right, the stated reason was false.
  2. **A malformed `--database` silently WIDENED to all five knowledge
     databases** instead of refusing — `--database` as the final argument reads
     `undefined` and fell through to "plan over everything". In a tool that
     rewrites board data, a typo that broadens scope is the wrong direction to
     fail. Now refuses (exit 1), with `undefined` still meaning "flag absent"
     via an explicit sentinel.
  3. **A name-normalisation collision would have dropped a property silently**,
     against the script's own header promise to refuse rather than drop. Now
     refuses on either side. Unreachable in this vault — all nine names in all
     five live databases normalise distinctly — so it guards a future rename.
- **A correction to something I wrote.** I reported that `ZEN-D-004`'s Category
  was transformed from `engine` to `Engine`. It was not: the row stores the same
  option id, and `Engine` is what `readKnowledgeCell` renders from it. The SELECT
  round trip was byte-lossless, which is a stronger result than I claimed. The
  reviewer caught this.
- **Two findings deliberately not fixed here, filed as `ZEN-104`:** nothing
  guards the *direction* of the move (on a checkout resolving the other
  organisation, one `--apply` sweeps 37 live rows backwards, and nothing fails
  because the move stays reader-consistent), and the SELECT path has no test.
- **The 7 null-ID rows in the live database were not touched**, deliberately.
  The passage at `decide-reference.test.mjs:194-205` calls repairing them a human
  judgement call, not a script: a byte-identical duplicate of an already-minted
  row must not receive a second reference. Their count is unchanged.
- **Corrections made to the ticket's own text before inlining it (I3):** the
  Goal's central measurement ("returns only the same 7 pre-existing rows") was
  reproduced and found false, so the measured evidence was inlined as the
  specification with the worker told explicitly to prefer it over the Goal.
- The `zagent decide` warning about ZEN's `Engine` category declaring no
  `decisionLog` reproduced again. Already filed as a Suggestions row under
  ZEN-85; not re-filed.
<!-- agent:worklog:end -->
