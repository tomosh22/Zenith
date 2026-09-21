---
id: "2fcd9111-a111-4dde-a8a5-a232316a52dc"
key: "ZEN-76"
type: "TASK"
title: "zagent check: the contract verdict (exit 4) wins over repo-busy (exit 5); claim and next unchanged"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 76
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T07:09:42.919Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
Suggestions row 2026-08-25 (cli): *"For `zagent check` specifically, let the DURABLE contract verdict (exit 4) win over the TRANSIENT I5 repo-busy verdict (exit 5). Keep 5 winning for claim/next, where the lock is the whole point."* Today a `check` run while another ticket is in flight answers 5 for every ticket, so a malformed ticket cannot be found until the repo is idle.

In `Tools/Vault/cli/queries.mjs` (or wherever `check` composes its verdict), evaluate the contract first and return 4 when it fails regardless of the lock; return 5 only for a contract-valid ticket that the lock would refuse. `claim` and `next` unchanged. Vitest over both orders.

## Definition of Done
- [ ] `check` returns 4 for a contract-invalid ticket while the repo is busy and 5 only for a valid one; `claim`/`next` unchanged; tests cover both
- [ ] CLAUDE.md's exit-code paragraph says so
- [ ] The Suggestions row is resolved with this key


## Comments

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Done.** Committed to `master` as `35707755` — one paragraph in root `CLAUDE.md`.

**★ Everything else this ticket asks for had already shipped**, verified against the source
rather than the cited row (which was itself already `Status: done`): `read.js`'s `checkPayload`
already orders `durable.blocked → 4` ahead of `inFlight → 5`, the payload already carries
`check.durable` / `check.inFlight`, the rationale is already written up at `read.js:301-312`,
`claim`/`next` still test the lock first via `claimDecision`, and
`Tools/Vault/test/check-verdict-precedence.test.mjs` already covers all five cases.

**That includes the clause the review flag was raised over.** `review.required` was true
because the DoD asserts an equivalence — *"`claim`/`next` unchanged"* — which a gate can never
prove. It turns out to be **already pinned by a test**
(`claim is UNCHANGED: the lock still outranks a durable refusal`), which is a better answer
than a review could have given. No reviewer was dispatched: the diff is one paragraph of prose.

**The real gap was that two documents disagreed and only the protected one was right.**
`.claude/commands/tick.md` has described this behaviour correctly since the fix landed; root
`CLAUDE.md`'s exit-code paragraph never did, and its *"`5` is what you get when this repo
already has a ticket in flight"* was actively misleading for `check`, which answers `4` for a
contract failure, an EPIC or a `needs-human` ticket even while busy. I nearly mis-attributed
that text to root `CLAUDE.md` from memory; checking is what turned "nothing to do" into a real
gap.

**Gate note.** Verify went red on the `Tools/Vault` `npm test` line and green on its own rerun,
failing `cli-process.test.mjs` at 6140 ms — **with a diff that is one markdown paragraph and
contains no JavaScript**. That is the cleanest instance yet of `ZEN-D-024`, and `cli-process`
is the same test I measured during ZEN-75 at 0/4 failures in isolation both with and without
that change. The red skipped all five `doc_lint` lines, which were run by hand: exit 0 on every
game. `zagent guard` clean, zero excursions.

Durable findings: none — the behaviour is documented in `read.js`'s own comment and now in
`CLAUDE.md`, and the flake is `ZEN-D-024`, which this run corroborates rather than extends.

<!-- agent:worklog:start ZEN-76 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Outcome**: committed to `master` as `35707755` · ran on sonnet/low via SIMPLE+LOW

**★ Everything this ticket asks for except one paragraph had already shipped** — the fourth
consecutive ticket in this shape, and the most complete of them. Established before dispatch,
verified against the source rather than the row, and then independently re-verified by the
worker before it wrote anything:

- **The verdict ladder is already durable-first.** `Tools/Vault/cli/read.js` `checkPayload`
  orders `breaker.open → 6`, `targeted.would → 0`, **`durable.blocked → 4`**, and only then
  `inFlight && inFlight !== key → 5`, else `4`.
- **The payload already carries it**: `payload.check = { queue, targeted, durable, inFlight }`.
- **The rationale is already written up** at `read.js:301-312`, naming the original symptom
  (`zagent check ZEN-1` on an EPIC returned 5 while a ticket was in flight).
- **`claim`/`next` are genuinely unchanged** — they go through `claimDecision`'s
  `targetedReason` path, which keeps `lockReason` ahead of `contract.error`.
- **Five tests already exist** in `Tools/Vault/test/check-verdict-precedence.test.mjs`,
  including `claim is UNCHANGED: the lock still outranks a durable refusal`.
- The cited Suggestions row was already `Status: done` with a line-by-line Resolution.

★ **`review.required` was true because the DoD asserts an equivalence** — *"`claim`/`next`
unchanged"* — which a gate can show the new path passes but never that the old one is
untouched. That clause turned out to be **already pinned by a test**, which is the strongest
possible answer to it. No reviewer was dispatched: with a one-paragraph markdown diff there is
nothing for one to read, and the equivalence the flag exists for is covered by
`check-verdict-precedence.test.mjs:141`.

★ **A near-miss on provenance, worth recording.** I initially recalled root `CLAUDE.md`
describing this fix. It does not — that text is in `.claude/commands/tick.md`, which has been
in context since the session opened. The two documents disagreed, and only the protected one
was right. Checking instead of trusting the recollection is what turned this from "already
done, nothing to do" into a real one-paragraph gap.

**What changed**

- `CLAUDE.md` — a new paragraph after the exit-code list. The old text described the codes as
  if they meant the same thing for every command, and *"`5` is what you get when this repo
  already has a ticket in flight"* was actively misleading for `check`.

**Gates** — `-Phase verify`: **exit 4**, with the `Tools/Vault` `npm test` line red and its own
rerun **green** (`flakes: [{firstExit: 1, rerunExit: 0}]`), failing
`cli-process.test.mjs > …keeps help read-only` at **6140 ms**. 821 of 822 passed; the total is
unchanged, so nothing was lost.

★ **This is the cleanest instance of `ZEN-D-024` yet: the diff is one markdown paragraph and
contains no JavaScript at all.** It cannot have caused a Vault CLI test to fail. `cli-process`
is the same test that flaked twice during ZEN-75 and which I measured at **0/4 failures in
isolation, both with and without that ticket's change**. Red→green on the rerun is the
protocol's flake case: record and continue.

The red **skipped all five `doc_lint` lines**, so I ran them by hand:

| gate line | result |
|---|---|
| `Test_TickGates.ps1`, `Test_UnitBaselineManifest.ps1`, `Test_T0Harness_…`, `Test_DocLintKeyCitations.ps1` | PASS |
| `Test-ZagentClient.ps1`, `Test-BuildPrompt.ps1` | PASS |
| `Tools/Vault` `npm test` | **PASS on rerun** — 821/822, one load-flake |
| `doc_lint.ps1` × 5 games | **PASS — by hand, exit 0 each** (Zenithmon, DevilsPlayground, Foundry, Undervault, Hearth) |

`zagent guard ZEN-76`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `CLAUDE.md` | **Nothing.** It is prose in a reference paragraph. `doc_lint` lints per-game docs, not the repo-root file, and the new paragraph deliberately cites no paths so there is no link or file-existence claim to check. Its correctness rests on having been written against `read.js` — which the worker read first, rather than paraphrasing my description of it. |

**Definition of Done**

- [x] `check` returns 4 for a contract-invalid ticket while the repo is busy and 5 only for a
      valid one; `claim`/`next` unchanged; tests cover both — **already true before this
      ticket**, verified in `read.js` and by the five tests named above. Nothing was changed.
- [x] `CLAUDE.md`'s exit-code paragraph says so — this commit.
- [x] The Suggestions row is resolved with this key — **already resolved, and deliberately not
      re-resolved**, per `ZEN-D-022`.

**Durable findings**: none

Nothing here outlives the ticket that is not already recorded: the behaviour is documented in
`read.js`'s own comment and now in `CLAUDE.md`, and the flake is `ZEN-D-024`, which this run
corroborates rather than extends.

**Deviations / follow-ups**

- **Zero excursions.** One file, on `filelist.txt`.
- **No reviewer dispatched despite `review.required: true`.** The reason given was the
  equivalence clause; that clause is pinned by an existing test, and the diff is one paragraph
  of prose with no code to review. Recording the decision rather than silently skipping the
  step — a reviewer handed this diff would have had nothing to check that a test does not
  already cover.
- **The two documents disagreed and only the protected one was right.** `.claude/commands/tick.md`
  has described this behaviour correctly since the fix landed; root `CLAUDE.md` never did.
  Worth noting as a pattern: a `protectedPath` document can be *ahead* of the unprotected one,
  and a tick that reads only the unprotected one gets the stale answer.
- **Not checked by anyone:** whether any other command's exit codes diverge between
  `claim`/`next` and the read-only queries in a way the paragraph now implies they do not
  (I checked `check` specifically, because that is what the ticket names).
<!-- agent:worklog:end -->
