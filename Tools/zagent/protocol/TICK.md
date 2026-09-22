Run **exactly ONE** ticket. Never start a second in the same tick.

# Where things live

Everything this tick needs is in THIS repo, **including the board**. It is not
a remote web application: the Vault IS this checkout, its rows are files here,
and every mutation is a commit. See step 0 for the three rules that follow.

| Thing | Home |
| --- | --- |
| The canonical protocol | `Tools/zagent/protocol/TICK.md` — the harness-neutral rules; each harness has its own entry point that resolves DISPATCH/AWAIT/ABORT/SCHEDULE/STOP against it |
| The worker and reviewer | role definitions generated per harness from the canonical charters in `Tools/zagent/protocol/roles/`, whose tool restriction is the enforcement |
| Gates, pinned baselines, conventions, categories | `zagent.project.json` (committed) |
| The client | `Tools/zagent/` — a shim that execs the Node CLI below |
| The board itself | `Tools/Vault/` (the CLI and server), `**/Docs/Board/tickets/` (ticket bodies), `metadata.md` (the knowledge databases) |
| Run scratch | `.zagent/last.json`, `.zagent/last.exit`, `.zagent/run/<KEY>/` (gitignored) |
| Model routing, guardrail floors | resolved by the Vault from `zagent.project.json` plus its own policy |

**Starting your harness in this repo is the whole setup** — no
`--add-dir`, no second checkout. `zagent` needs nothing but this
checkout and Node on `PATH`: no token, no URL, no board machine to
reach, because it always talks to the local Vault. `Tools/zagent/README.md`
covers the setup and `zagent doctor` names whatever piece is missing.

# Operating invariants

These are not style preferences. Each one exists because an unattended
loop without it lies to itself.

- **I1 — You own the gate; the worker only authors.** The WORKER role never
  builds, tests, runs, commits, or touches the board. Whether that is
  *enforced* structurally or only *declared* in the role's own text is
  `Worker tool denial` in your entry point (`enforced` | `declared`) — but
  state it in the prompt anyway, because a rule the worker can read is a
  rule it can plan around.

  **On a harness where the answer is `declared`, the role's own text is
  advisory, not a wall, and I1 is carried by WORKER-MOVED-NOTHING at step
  6.4 instead — that check is not optional there.** On a harness where the
  answer is `enforced`, the role definition's tool list IS the enforcement:
  widen it and I1 is gone with no other file changing and nothing failing
  loudly. Role definitions are protected by the harness's own
  protected-path mechanism for that reason.
- **I2 — Never trust "it works", and never trust "it cannot be done"
  either.** A worker's claim that gates pass is worthless by construction:
  it was forbidden from running them. Re-run everything yourself after
  integrating, even when the claim is plausible.

  **★ THE SECOND HALF IS NEW AND IT IS THE MORE CONSEQUENTIAL ONE.** This
  invariant was framed entirely against a SUCCESS claim, and the claim that
  actually decides a ticket's fate is the opposite: ZM-23's worker reported
  the ticket IMPOSSIBLE. Accepting that Blocks a ticket; rejecting it burns
  a dispatch. Recomputing the palette arithmetic independently took about
  two minutes and turned an assertion into a three-way corroboration — the
  worker's analysis, an independent script, and a repo document that
  predated the ticket. It held, and Blocking was right.

  The same applies to the REVIEWER. ZM-65's review returned three blocking
  findings, one of them refuting something the orchestrator had written
  confidently; it was checked against the terrain recipe before being acted
  on, and it held. **A claim that something is broken, or impossible, is
  still a claim.**
- **I3 — Inline the work into the prompt.** Never say "see the ticket" or
  "read Docs/X.md section 3". Paste the ticket body, the Definition of
  Done, the file list, the conventions **and every recorded RULING that
  governs the ticket's subject matter** INTO the worker prompt.
  `bodyPath` is a supplement, never the carrier.

  **★★ THE RULINGS CLAUSE IS THE ONE THAT WAS MISSING, AND IT COST A
  TICKET.** ZM-23 was dispatched with all four of the others pasted, plus
  three reconciliations. The worker still shipped a trainer row that
  contradicts `ZM-D-203` Ruling 3 — *"both get their own
  `ZM_STORY_FLAG_ID`"*, because a flagless trainer's prize money is
  **farmable across restarts** — and then PINNED the violation with a new
  test asserting it was correct design.

  Nothing else could see it. `contractValid` answers routing; the
  reachability scan reads paths; the drift resolver checks that citations
  RESOLVE, not that the ticket agrees with a decision. Three pre-existing
  units actively *require* the flagless row, so the violation was the only
  state that passed, and every gate went green. It surfaced solely because
  `risk MEDIUM` happens to sit in `reviewOn` — on a LOW-risk ticket it
  merges.

  **The worker has no shell and no board: anything not inlined does not
  exist for it.** `zagent.project.json` already declares `decisionLog` and
  `decisionIdPrefix`, so you are told where the rulings live — you were
  just never told to read them. Grep the decision log for the ticket's
  nouns before writing the prompt, and paste what you find VERBATIM;
  Status.md's "do NOT re-ask" list is the second place to look.
- **I4 — Single writer for shared state.** Only you write to the board.
  The worker returns _proposed_ work-log and decision text; you apply it.
- **I5 — Serial execution per repo.** One in-flight ticket per repo,
  enforced atomically by the CLI's claim transaction. If a claim is
  refused naming another key, that is I5 working — do not work around it.
- **I6 — Idempotence over memory.** A firing may crash at any point and
  the next must reconstruct from durable state alone: the board, git, and
  `.zagent/run/<KEY>/`. Never from anything you remember.
- **I7 — Never sign your own gate.** A `human-gate` ticket stops at
  In Review no matter how green the gates are, and no matter whether a
  human asked for it by name.

**Two standing rules about this file and the config:**

1. **Every gate line must be pre-authorised in your harness's own mechanism.** Gate
   lines live in `zagent.project.json`; what "pre-authorised" MEANS is your entry
   point's `Pre-authorisation` section, and it differs per harness. You do not need
   to know the others'. `Tools/zagent/Test-HarnessParity.ps1` checks all three, so a
   gate added without its authorisation reds a gate — rather than stalling an
   unattended loop at 3am on a permission prompt, which is what it used to do.
2. **You may never edit this file or `zagent.project.json`.** Both are in
   `protectedPaths`, and that list is not enforced by a union with the
   board's own floor: `guardrailsOf` (`Tools/Vault/cli/compat.mjs`) resolves
   protected paths as `project.protectedPaths ?? boardGuardrails ??
   clientFile.protectedPaths` — a FALLBACK CHAIN, not a union — and
   `boardGuardrails` (`vault.config.json`'s own list) is always present, so
   this file's list never wins and enforces nothing on its own; it survives
   only by staying a strict subset. `vault.config.json` is itself in that
   enforced list now, which is what makes "may never edit" true rather than
   aspirational. Workflow friction becomes a Suggestions row for a human to
   triage, never a commit. A loop that can rewrite its own gates does not
   have gates.

   > **This rule binds the LOOP, and the owner has amended the file three
   > times outside a tick** (2026-08-24 twice, 2026-08-26). `zagent guard`
   > still refuses any diff touching this repo's protected paths under a
   > ticket, which is the part that matters. Recorded because a file saying
   > "never edited" beside a git history showing edits is worse than either;
   > `Docs/TickProtocolHistory.md` has the rounds and what each changed.

# Never

- No `psql`, no direct database access, no importing `@upstream/database`.
  Every board interaction is a `zagent …` call.
- No `git push`. No `--force`. No `reset --hard`. No deleting a branch.
- No `git switch -c`, `git worktree`, or `gh pr` when `branching` is
  `"direct"`.
- No editing files outside the ticket's `repo`.
- No second ticket in this tick.
- No stashing and no resetting to work around a dirty tree.

# Running the CLI, and reading its output

**`zagent …`, bare**, from this repo or any subdirectory of it — it walks
up for `zagent.project.json` the way git walks up for `.git`. If it is
not on PATH, `Tools/zagent/README.md` has the one-time setup.

Its scratch is HERE now: `.zagent/last.json`, `.zagent/last.exit` and
`.zagent/run/<KEY>/`, at the repo root, gitignored so `git status
--porcelain` stays empty while a ticket is in flight. The precondition
check below depends on that.

Exits 3/4/5/6 are ordinary control flow, not failures. **Exit 7 is new
and is not one of them**: it means the board was unreachable. Treat it as
infrastructure, not as a verdict on the ticket — comment nothing, change
no status, and reschedule. A network blip written into a ticket as a
contract failure is a lie that outlives the blip.

**★★ READ `.zagent/last.exit`. That is the primary instruction in this
section now — everything below it is what to do when nothing wrote one
for you.** Every `zagent` call, on **either backend** (the live local
Vault client in `Tools/Vault/cli/client.mjs` and the legacy PowerShell
one in `Tools/zagent/zagent.ps1` alike), writes its own exit code to this
one-line file (`"<code>\n"`) on every exit path, including the early
ones, before the value ever has to pass back through a shell that could
corrupt it. **Unlike `last.json`, which is only written when you pass
`--json`, `last.exit` is written unconditionally, every call.** Read it
the same way you read `last.json`:

```
zagent guard <KEY>
$code = Get-Content .zagent/last.exit          # trust this, not $LASTEXITCODE
```

This is no longer a discipline to hold under pressure; it is a channel
built because the discipline did not survive contact with an unattended
loop. **The measured history, kept because it explains why the file
exists:** three ordinary shell idioms broke this in one tick, and the
third would have committed a RED ticket as Done with evidence claiming
the gates passed:

```
zagent guard ZM-21 | head            # $? is HEAD's (0), not guard's (1)
pwsh tick_gates.ps1 … ; echo "$?"    # the run had exited 10, and you read 0
pwsh tick_gates.ps1 … ; echo "$?"    # …and here 4 — A GATE HAD FAILED
```

**★ THE NOTIFICATION LIES, NOT JUST THE SHELL.** A completion notification
reports the COMPOUND statement's exit code (`; echo "$?"` reports `echo`'s 0,
not the command's), and a pipeline destroys `$?` at the source the same way.
*"completed (exit code 0)"* is never independent confirmation for a command
whose own code did not land in a file. `Docs/TickProtocolHistory.md` has the
measured proof and the wrong explanation of POSIX `$?` this replaced.

**This still leaves real cases with no `.exit` file at all — an ad-hoc
command that isn't `zagent` or `tick_gates.ps1`, chiefly a bare `msbuild`
invocation or a game `.exe` run outside the harness.** For those, the old
discipline still applies in full, because nothing else will catch the same
mistake for you:

- **Branch on the exit code**, never on stdout prose.
- **Read the JSON from a file**, not from the pipe, when the command
  writes one: every `zagent --json` call writes `.zagent/last.json`, and
  a claim also writes `.zagent/run/<KEY>/ticket.json` and
  `.zagent/run/<KEY>/body.md`. A firing can crash between the call and
  the read, and a file survives that where a pipe does not.
- **★★ NEVER PIPE OR SUFFIX A CALL WHOSE EXIT CODE YOU INTEND TO READ, WHEN
  NOTHING WRITES IT TO A FILE FOR YOU.** Write the code to a file yourself
  and read it in a separate step:

  ```
  msbuild <sln> /t:<Game> ; echo $LASTEXITCODE > exit.txt
  echo $? > exit.txt                   # right: a separate step, then read the file
  ```

---

# Steps

## 0. Pre-flight (first firing after a restart only)

**There is ONE backend: the local Vault, and it IS this checkout.** No token,
no URL, no board machine to reach. `zagent.ps1` is a thin shim that execs
`node Tools/Vault/cli/client.mjs --vault <repoRoot>` and exits with its code.

> ★★ **This section used to gate the Vault preflight on
> `Tools/zagent/backend.json`, with a "legacy HTTP backend" written out below
> it. That file was DELETED when the HTTP backend was removed**, so the
> condition could never be true and every tick was routed into the dead
> branch — which then mandated a `doctor` row-count check that now returns 0,
> whose stated consequence was *"you are running the wrong `zagent`, stop"*.
> A literal reading halted every tick, and blamed a stale `PATH` shim that was
> not the cause. The legacy branch is gone from this file;
> `Docs/TickProtocolHistory.md` keeps it. If you find yourself reading about
> tokens, lanes or an agent account, the doc is stale.

Run it and read the JSON, not prose rows:

```
zagent doctor --json
```

Require **exit 0** (from `.zagent/last.exit`), `ok: true`, and an empty
`diagnostics` array. The payload also carries `projects` and `ticketCount`,
which is the cheap sanity check that the vault actually loaded:

```json
{"ok":true,"diagnostics":[],"projects":["ZEN","ZM","DP","UV","HE","FD"],"ticketCount":544}
```

A failing check here explains most "the loop did nothing for an hour"
incidents. Report and stop if `ok` is false or any diagnostic is present.

**And `zagent` must be run from inside the checkout.** The client walks up from
the CURRENT directory for `zagent.project.json`, the way git walks up for
`.git`. From outside it cannot find the vault at all.

**`Tools/zagent/zagent`** — extensionless, LF-pinned in `.gitattributes` — is
what makes a bare `zagent` work in a POSIX shell as well as in PowerShell. If
that file is missing, a Git Bash tick will find something else on `PATH`.

### ★★ The board COMMITS INTO THIS REPOSITORY. Three rules follow.

Every board mutation — `claim`, `next`, `comment`, `move`, `update`, `link`,
`decide`, `finish` — writes a file and makes a commit (`board: next`,
`board: decide`, …). Ticket bodies live in `**/Docs/Board/tickets/`, the
knowledge databases in `metadata.md` at the repo root. On the audit that found
this, **11 of the last 12 commits on `master` were board commits**.

1. **Step 3's precondition checks the TREE, and board writes do not dirty it.**
   They commit. So `git status --porcelain` stays empty while HEAD moves under
   you. That is fine — it is what the precondition wants — but do not read a
   clean tree as "nothing has happened since I started".
2. **★ NO BOARD WRITE BETWEEN `-Phase measure` AND `zagent guard`.** `guard`
   overrides a would-be PASS whose `tick_gates.json` `headSha` no longer
   matches HEAD (step 6.4). A board write in that window moves HEAD and turns
   a green run into a guard failure for a reason that has nothing to do with
   the code. Step 6 invites `comment`/`update`/label writes at several points:
   do them BEFORE `measure` or AFTER `guard`. Resolving a Suggestions row
   mid-`measure` is exactly how this was found.
3. **Your ticket commit and the board's commits interleave**, so
   `<baseBranch>...HEAD` is not "what this ticket changed". Use the working
   tree (step 6.4) and `changed.txt`, which is what `guard` and `finish`
   already do. Measured: two `board: decide` commits landed between a ticket
   commit and `finish` and the commit-completeness check was unaffected,
   because it reads `changed.txt` rather than a commit range.

## 1. Pick the ticket

The argument disambiguates itself: ticket keys always end in `-<digits>`,
and project keys never can. Digits ARE legal inside a project key, so
match on the suffix, not on "letters".

- `$1` matches `/-\d+$/` → **targeted**: `zagent claim $1 --harness <id> --json`
- `$1` is a project key → **queue, THAT project**:
  `zagent next --project $1 --harness <id> --json`
- `$1` absent → **queue, every project this repo declares**, in declaration
  order: `zagent next --harness <id> --json`

**`--harness <id>` is the key this entry point is generated for**
(`harnesses.json`'s own top-level key, e.g. `claude`). The Vault RECORDS it,
never branches on it — the same opaque treatment `workerTools` gets — and
refuses a value `harnesses.json` does not declare. `build_prompt.ps1` copies
it into `predispatch.json` (step 5); `finish`'s routing-line assertion
(step 8) checks the work log against that same recorded value.

**Pass `--project`. Both branches used to spell the same command**, which made
`/tick ZM` a lie: with no `--project`, `next` walks all three declared projects
in order and takes the first claimable ticket anywhere. A `/tick ZM` in a session
where ZEN had work claimed ZEN-1, consuming the one-ticket-per-repo lock on a
board the caller had just named a different one.

Then read `.zagent/last.json` and branch on the exit code. Each disposition
below is written to `.zagent/next.json`, not called directly:

```jsonc
{ "disposition": "again" | "wait" | "stop", "delaySeconds": 60, "reason": "queue empty" }
```

| `next.json` | `loopMode: in-harness` | `loopMode: manual` |
|---|---|---|
| `again` | `SCHEDULE{noop:false, delaySeconds:<n>}` | report; the human re-invokes |
| `wait` | `SCHEDULE{noop:true, delaySeconds:<n>}` | report the delay; the human waits |
| `stop` | `SCHEDULE{stop:true}` | report and end |

**The tick itself reads `next.json` and makes the SCHEDULE call** when
`loopMode` is `in-harness`. An external loop driver reads nothing here, and
building against a model where it does would leave the in-harness case
silently un-paced. Behaviour where SCHEDULE exists is unchanged; what
changes is that the disposition is now durable on disk, where today it is a
tool call leaving no trace at all — which also satisfies invariant I6.

| Exit | Meaning                   | Do                                                                                                    |
| ---- | ------------------------- | ----------------------------------------------------------------------------------------------------- |
| 0    | Claimed, contract valid   | → step 2                                                                                              |
| 3    | Queue empty               | Disposition `wait` (delaySeconds 900), stop                                                          |
| 4    | Claimed, contract invalid | → step 2                                                                                              |
| 5    | Repo busy, or would steal | Report which key holds it. Queue mode: disposition `wait` (delaySeconds 300). Targeted: disposition `stop`. |
| 6    | Circuit breaker open      | Report, disposition `stop`. A loop that keeps failing should stop, not spin.                         |
| 7    | Board unreachable         | Nothing was claimed. Change no status, comment nothing. Disposition `wait` (delaySeconds 300).        |

## 2. Contract and label checks

**Contract invalid (exit 4).** Read `contractError` from the payload.

- Queue mode: `zagent comment <KEY> --text "<contractError>"`, then
  `zagent move <KEY> Blocked`. Then step 9.
- Targeted mode: **report the error and change nothing.** A human is
  waiting and just asked for this ticket by name — do not Block it.

  **A refused claim changes nothing, and that is now PROVEN rather than
  hoped for.** `executeOperation` (`Tools/Vault/server/operations.mjs`) calls
  `prepare` before it writes the journal or commits, and every refusal inside
  `claim`'s prepare throws before the change set is built — so no field moves
  and no commit exists. Pinned by
  `Tools/Vault/test/claim-refusal-atomicity.test.mjs`: `needs-human`, EPIC,
  blocked-by-predecessor and queue-empty each assert byte-identical vault
  files, unchanged `status`/`assignee`/`claimAttempt`, unmoved HEAD and no
  added commit, with a control test around an ACCEPTED claim so none of that
  can pass vacuously. See `ZEN-D-007`.

  > ★ **This paragraph used to say the opposite, and then to describe a
  > rollback that no longer exists.** The mutate-before-validate defect was
  > real on the OLD SQL board; the Vault CLI is a different implementation and
  > does not have it. The rollback it told you to read a stderr line from —
  > `Restore-RefusedClaim` — went when `zagent.ps1` became a Node shim, so the
  > instruction was to verify something that could never print. The eight ZEN
  > tickets once found "assigned while still To Do" were imported in that state
  > by the activation snapshot (`ZEN-D-008`), not produced here.

**Reachability — read the Definition of Done BEFORE dispatching a worker.**
`contractValid: true` used to mean only that the ticket could be ROUTED —
it has a category, a complexity and a risk — and said nothing about
whether the work is inside what an agent bound by `protectedPaths` can
reach. Twice in one session that gap cost a claim, a slot in the
one-ticket-per-repo lock and a worker dispatch each.

**The board scans the DoD for protected paths and refuses at FILE
time**, the same way `needs-human` does. It refuses whenever the ticket
would be immediately claimable, which — since *To Do* became the queue —
is every unassigned, un-`needs-human` ticket, whatever lane it is filed
into. A ticket that reaches you with one anyway, filed before the check
existed, comes back as an ordinary exit 4 naming the path and the
pattern. Handle it as any contract failure; do not re-derive the rule.

**A `REACHABILITY` line on `show`/`check` is the near miss.** It fires when
the GOAL names a protected path and the DoD names none — a ticket that is
contract-VALID and says in its own words that it needs a human. ZEN-3's
Goal reads *"This needs a human: `.github/**` is a protected path, so the
agent loop is refused by `zagent guard`"*, and it passed every check,
because its DoD says only "Some workflow runs …". It is a warning rather
than a refusal on purpose: a Goal quoting a protected path as background
is ordinary writing, and refusing that would make the check something
people route around. When you see one, read the Goal — and if it is right,
`zagent update <KEY> --label needs-human` and release the ticket.

That leaves the shapes no scanner can see, and these you do still read
for:

| The ticket | Why it cannot be done |
| --- | --- |
| DoD says "a **user decision** recorded", or any ruling the ticket does not already contain | nobody in this loop is the user |
| DoD says **CI** / "the workflow" / "the pin", naming no file | the same impossibility, said in prose — the scan needs a path to match |
| **The ticket's stated SCOPE contradicts its own GOAL** | it cannot be finished as filed, whoever runs it |

**★★ THAT THIRD ROW IS NEW, AND IT IS THE ONE THAT COST A DISPATCH.** ZM-22's
goal was *"wild encounters LIVE on Route 1"* while the R1 slice table budgeted
R1-4 as re-authoring **nothing** — and making encounters fire required scene
content. Both statements were months old and could not both hold. The
contradiction was spotted at reconciliation and the response was to encode the
doubt into the worker prompt as a STOP CONDITION and dispatch anyway. It worked,
and it cost a full opus-tier dispatch to confirm what was already known.

**Resolve it HERE instead.** Three moves, in preference order: relabel the
ticket (`needs-gpu` and do both halves — you probably have a GPU); rescope it
with `zagent update <KEY> --dod <file>`; or Block it *before* dispatching and
file the properly-scoped replacement. Encoding the doubt into a worker prompt
is not one of them — step 2 says plainly **"do not dispatch a worker to
discover this expensively"**.

In this repo it is even mechanically checkable: the slice table's "re-authors"
column and the ticket's Goal are both structured text.

The pin bump is NO LONGER one of these: it has one unprotected home,
`Tools/unit_baselines.json`, and a DoD naming it files and runs cleanly.
See the baseline note in step 6.

Check those against the DoD text.

**Two shapes that were rows here are now refused by the claim query**,
so you no longer read for them. An EPIC comes back exit 4 from `zagent
check`, on the queue AND by name — **including while another ticket holds the
repo lock, which was not true until ZEN-76 was fixed.** `check`'s exit ladder
tested the lock before the contract, so with anything in flight it answered 5
(repo busy) for an EPIC, a `needs-human` ticket or a contract failure alike:
it reported the obstacle that clears by itself and hid the one that does not,
in exactly the situation a running loop is always in. It now prefers the
DURABLE verdict and carries it on the payload as `check.durable`, while still
reporting the lock as `check.inFlight`. `claim` and `next` are deliberately
unchanged — they mutate, and refusing to steal outranks everything. An EPIC is
a rollup container, and its
"Every child issue Done" would be inlined into a worker prompt as the
specification under I3. A ticket in a PLANNED sprint is skipped by the
queue but still claimable BY NAME on purpose: `/tick ZM-46` is how you
pull work forward, and `check`/`show` print an `OUT OF STAGE` line when
it applies. `sprint` is on the claim payload now, so the sequence check
needs no `zagent sprint list` and no squint at Goal prose.

Together they cost three claims in one session before any of that
existed: ZM-17 arrived at `sonnet/medium` already **100% (1/1)** done,
6 of 15 claimable ZM tickets were epics, and the queue offered S10 and
S12 work while S8 was active.

**And read the drift warning.** A claim now carries the paths and symbols
the body says exist, and the client resolves them against this checkout —
so an unresolved citation is printed on stderr and left in
`.zagent/run/<KEY>/drift.txt` rather than depending on you to think of
looking. **Every ticket body examined so far had drifted from the code —
seven for seven**: one cited a file with zero matches, one described a
trigger condition that had not occurred, one named a single assertion
where the code has two, one quoted a hardcoded path that had already been
parameterised, and one was entirely already done. A body is a description
of the past, not of `master`.

It is ADVISORY and must stay that way — a ticket may legitimately name
what it is about to CREATE. What it buys is that you cannot inline a body
into a worker prompt (I3) without having been told which half of it no
longer resolves. When the warning names a path that MOVED, correct it in
the prompt and say so in the work log.

If the work is unreachable, **Block it now** with a work log naming which
of the three it is and what a human must do, and file the human half as its
own ticket. Do not dispatch a worker to discover this expensively, and do
not let it consume fix-forward attempts.

**A DoD that names a pin SITE is stale, not unreachable.** Ten open tickets
still say "bumped in Status.md AND zm-tests.yml AND zagent.project.json",
which was true before the baseline refactor and now points at two protected
paths — so the reachability check refuses them at claim time, correctly and
unhelpfully. The pin has ONE home: `Tools/unit_baselines.json`. **Do not
Block on this and do not inline it verbatim**: correct it in the worker
prompt to "every affected game's pinned baseline bumped from an OBSERVED
run", say you did in the work log, and fix the ticket body with
`zagent update <KEY> --dod <file>` if a human is not mid-edit on it.
Blocking a ticket over a stale sentence, when the work it describes is a
one-line edit to an unprotected file, is the contract failing the ticket
rather than the reverse.

The template no longer names sites at all (root `CLAUDE.md`). A DoD that
names them is duplicating a fact that moves, which is the same duplication
the manifest refactor removed from the build — re-introduced one ticket at
a time.

Read the body for the ticket's OWN timing — several defer themselves in
prose that no field captures:

- a ticket from a later sprint is skipped by the queue and flagged
  `OUT OF STAGE` on a targeted claim. That is the mechanism; the
  judgement is still yours, and it is usually that what the ticket
  measures does not exist yet (ZM-45's budgets, ZM-43's chapters);
- a ticket may say outright it should not be built yet. ZM-48 reads
  *"Zenithmon does NOT need this now … lands when populated-world NPC
  pathing actually needs it"*; ZM-47 is *"DEFERRED post-Zenithmon"*. Claim,
  comment, **`zagent update <KEY> --label deferred --assignee none`**,
  release to To Do — do not dispatch.

  **`--assignee none` is not optional either.** The claim wrote
  `assignee_id`; `zagent move` only changes status; and the claim query
  skips ANY assigned ticket. So a release without it puts the card back in
  the lane permanently invisible to the queue — which looks like the
  deferral working, right up until somebody wants the ticket back. Four
  tickets were stranded exactly that way in one session, one of them
  carrying no label at all, so the stale assignee was the only thing
  holding it out and nothing would ever have returned it. **An autonomous loop widening scope on a self-deferred
  ticket is worse than one that does nothing**, and the deferral is
  invisible to `contractValid`.

  **The label is not optional and releasing without it is a live loop.**
  To Do IS the queue, so a bare release puts the ticket straight back at
  the head and the next firing claims it again, and the one after that,
  forever — a tick that burns a claim, a dispatch slot and a wakeup on the
  same card indefinitely, with nothing ever going red. `deferred` is
  filtered out of the claim query exactly like `needs-human`, so the card
  stays visible in To Do and stops being taken.

  It is **not** `Blocked`. Nothing is wrong with these tickets, and three
  of them would trip `maxConsecutiveBlocked` and stop the whole queue
  over work that is merely early.

  A TARGETED claim still takes a `deferred` ticket — unlike `needs-human`,
  which is refused either way. The asymmetry is deliberate: `needs-human`
  says the machine cannot produce the deliverable, while `deferred` is a
  judgement about timing, and `/tick ZM-48` is how somebody overrules it.
  If you are handed one by name, read the deferral, decide, and say in
  the work log which way you went and why.

And watch for a DoD whose only reachable branch is DESTRUCTIVE. ZM-56 is
"the test passes windowed, or is retired with a recorded reason" — a
`requiresGraphics` test. Handed "fix it or delete it" with no way to test a
fix, an agent deletes coverage. **When only the destructive branch is
verifiable, that is not a choice — take neither, and say why.**

**But check WHOSE reach before you decide.** "The headless loop cannot do
this" and "I cannot do this" are different sets, and collapsing them
declines work that is perfectly doable:

| | |
| --- | --- |
| An unattended CI loop | no display, no GPU — `requiresGraphics` is genuinely out |
| A `/tick` on a developer box | a GPU is usually right there |

ZM-56 was written off as unreachable on exactly that confusion, then done
by building the `Vulkan_` config and running
`--automated-test <name> --skip-unit-tests`. `--skip-unit-tests` is
required: the SaveData sandbox unit fails BY DESIGN under the harness.
So before declaring a graphics-gated ticket unreachable, check for a GPU —
and if there is one, the windowed run is often the *only* way to get the
measurement the ticket is actually asking for.

**★★ AND SCENE AUTHORING IS NO LONGER OUT OF REACH AT ALL. ZEN-6 REMOVED THE
`.zscen` PUBLISH GUARD (2026-08-26, `cbebea74`).** A headless run may now CHANGE
a committed scene, not only create a missing one — entity creation is
backend-neutral now and only the GPU allocation is skipped. Proven rather than
assumed: a `Null_*_True` boot re-authors every scene both RenderTest and
Zenithmon publish and reports `[ScenePublish] IDENTICAL` for all of them. This
reverses what the section used to say and was the commonest reason a
scene-authoring ticket got `needs-gpu` (below); `Docs/TickProtocolHistory.md`
has the removed guard and the RenderTest incident that motivated it.

★ **One trap survives.** The UNIT-GATE boot authors nothing — it exits before
the editor-automation queue drains, so it emits no `[ScenePublish]` line and
leaves a clean tree. **A clean tree is therefore not evidence that a re-author
reproduced the bytes.** Drive an authoring boot with
`--automated-test <T> --skip-unit-tests` and read the marker.

**★★ A WINDOWED TEST RUN IS AUTHORING, BEFORE IT TESTS. Check `git status`
after every one.** A `Vulkan_*_True` boot re-authors committed scenes as an
ordinary side effect of booting — measured on ZM-65, where the run that
PRODUCED a required measurement wrote the gate entity a million metres below
the world into a tracked asset, byte-identically reproducible and wrong.
`Docs/TickProtocolHistory.md` has the full measurement.

**The two-boot proof does NOT catch this.** Two pre-freeze boots are
byte-identical to each other — reproducibly WRONG. Reproducibility and
correctness are different properties and that proof only establishes the first.

**`needs-gpu` label** (`needsGpu: true` in the payload). Claimed like any other
ticket — **a GPU is assumed available** and this label gates nothing. What it
tells you is HOW to build: the deliverable needs a `Vulkan_*_True` build and a
windowed run, not the `--headless` `Null_` config every gate line defaults to.

**★★ APPLYING THE LABEL NOW REQUIRES A MACHINE-READABLE REASON, IN THE SAME
WRITE.** `zagent update <KEY> --label needs-gpu` refuses unless exactly one
companion `gpu-reason:*` label lands in the same write. Not retroactive: a
ticket that already carries `needs-gpu` from before this rule existed is
untouched until somebody writes to its labels again. The four values, and
what each asserts:

| Label | Means |
| --- | --- |
| `gpu-reason:render-capture` | the deliverable IS a rendered image — a screenshot, a photo-tour frame |
| `gpu-reason:visual-measure` | a human or a pixel-diff has to look at what rendered, not just that something did |
| `gpu-reason:requires-graphics-test` | a `requiresGraphics` automated test is the only verification path |
| `gpu-reason:legacy-scene-reauthor` | pre-ZEN-6 reasoning that no longer holds (below) — kept so an old ticket's label can be read without guessing why it was applied |

**★★ AND SINCE ZEN-6, `gpu-reason:legacy-scene-reauthor` NAMES A REASON THAT NO
LONGER APPLIES.** "It re-authors a committed scene" was the commonest reason for
the label, and it stopped being one on 2026-08-26 — a headless boot may now
change a committed `.zscen` (see the guard note above). So when you claim a
ticket carrying `needs-gpu` + `gpu-reason:legacy-scene-reauthor`, **read WHY it
was applied**: if the deliverable genuinely also needs a graphics driver for some
other reason, relabel it with the companion that actually applies and build
Vulkan/boot windowed as below. If scene re-authoring was the only reason, do it
headless, say so in the work log, and remove both labels —
`zagent update <KEY> --unlabel needs-gpu,gpu-reason:legacy-scene-reauthor`.

Several open Zenithmon tickets predate ZEN-6 and still carry the label on the old
reasoning. It is worth removing when you notice one — six of the ten tickets on
Zenithmon's S8 critical path sat behind that label, which is the cost ZEN-6
existed to remove.

**★★ DO NOT ASK PERMISSION TO RUN A VULKAN BUILD OR A WINDOWED BOOT.** They are
ordinary steps of a `needs-gpu` tick, exactly as `zenith build --headless` is of
any other, and stopping to confirm puts a person back in front of a ticket the
label exists to hand to the loop — which is the whole defect `windowed` had.
Build it, boot it, gate it, commit it. Say in the work log that a window opened;
do not wait for someone to say yes.

**★ AND HERE IS THE COMMAND, because it is written down nowhere else.** Root
`CLAUDE.md`'s CLI table shows only `zenith build <Game>`, and every gate line
uses `--headless`; the `--config` flag lives in `Tools/ZenithCli/ZenithCli.psm1`.
A step that says *do not hesitate* should not send you reading a module to find
out how:

```
zenith build <Game> --config Vulkan_vs2022_Debug_Win64_True   # windowed/authoring
<exe> --automated-test <TestName> --skip-unit-tests            # drive the boot
zenith build <Game> --config Null_vs2022_Debug_Win64_False     # the non-tools path
```

The first is the build this label exists to select. The second is how a windowed
exe ever EXITS (see the three mechanics below). The third compiles the
`#ifdef ZENITH_TOOLS` `#else` branch, which **nothing used to build**: every gate
line is `--headless`, i.e. `Null_*_True` with tools ON, so a non-tools break
merged green and — because `push` is false — CI never ran to catch it either.
Each category's gate list now carries one `D3D12_*_False` compile, copied from
the step CI already had (`zm-tests.yml`), so you no longer have to remember this.

The pin still comes from a `Null_` run. `Tools/unit_baselines.json` is explicit
that a Vulkan exe reports higher for the same tree (a standing +37 for
Zenithmon), so the shape is: **Vulkan to author, Null to verify and pin.**

Three mechanics that decide whether the windowed run works at all, all measured
on ZM-20:

- **A bare windowed exe NEVER EXITS.** `--exit-after-frames` is silently ignored
  without `--automated-test`, and a tools build then "idles in the editor
  forever". Drive the boot with a short automated test — the authoring runs at
  boot, before the test does:
  `zenithmon.exe --automated-test ZM_Boot_Test --skip-unit-tests`.
- **`--skip-unit-tests` is not optional.** A failing boot unit aborts the boot
  BEFORE scene authoring runs, and the needle for the thing you are authoring is
  red until you author it — so without the flag the unit blocks its own fix,
  forever. `zenith test` passes it for you; a bare exe does not.
- **Read the authoring log line, not the exit code.** ZM-20's proof was
  `sceneAuthoring=AUTHOR_DAWNMERE, warmMask=0x7, queued=0`. The failure mode is
  `DEFERRED`, which authors nothing, exits 0, and looks successful — the same
  fail-open shape as a skipped regen. Then **boot a second time and compare
  hashes**: the bytes must be reproducible, and a scene built with
  `AddStep_CreateScene` rather than loaded means the second boot is genuine
  input-independence rather than repetition. **Take both boots AFTER the last
  source change** — see the trap above.

**★★ THE ORDER, WRITTEN DOWN, BECAUSE TWO OF THESE STEPS ARE THE SAME COMMAND.**
Those mechanics are four facts and not a sequence, and on a measure-then-freeze
ticket the sequence is where the damage is. ZM-65's was nine steps:

1. build `Vulkan_*_True`
2. windowed boot → take the MEASUREMENT *(this also authors the sentinel — above)*
3. freeze the printed literal into the table
4. **rebuild** — the frozen value is compiled in
5. windowed boot → re-author CORRECTLY, and confirm the oracle went green
6. windowed boot again → byte-identity proof
7. hash → the `Status.md` committed-asset table
8. build `Null_` + `tick_gates -Phase measure` → the pin
9. `tick_gates -Phase verify`

**Steps 2 and 5 are the same command and print the same success signature**
(`sceneAuthoring=AUTHOR_DAWNMERE, warmMask=0x7, queued=0`), differing only in
whether step 3 happened between them. Hash after step 2 — the natural move,
since the log says the authoring succeeded — and you write a garbage hash into
the living docs describing a scene with an entity underground, with every later
gate green against it. The freeze goes BETWEEN the two boots; the hash comes
from the second pair.

**`needs-human` label** (`needsHuman: true` in the payload, and exit 4). No
machine can produce the deliverable — it is a person's judgement: a visual
sign-off, a ruling the ticket does not contain. Queue mode never sees one; if a
TARGETED claim returns it, **report and change nothing**, as with any exit 4 in
targeted mode. Do not dispatch a worker.

These two were ONE label until they were split. `windowed` meant "the loop
cannot finish this" and collected unrelated reasons under it: of 12 such
tickets, 8 wanted only a graphics driver and 4 wanted a person — and two of
those four had nothing to do with rendering at all (one's deliverable was a
board row, the other's a `.github/**` workflow). Six of the ten tickets on
Zenithmon's S8 critical path were behind the label; one of them actually needed
a human.

**`human-gate` label** (`humanGate: true` in the payload). Run the whole
tick normally — preconditions, worker, gates, evidence — but at step 7
move to **In Review** instead of Done. The reporter is
notified automatically by `finish`. Then continue: queue mode schedules
the next wakeup as usual, because the gated ticket is out of the queue by
construction and one gated ticket must not starve the board. Targeted
mode stops after it.

**★ In `direct` mode, "never merge" has no meaning, so do not try to honour
it.** This used to read "move to In Review and never merge", which is coherent
only under `branching: "branch"`, where the work sits on an unmerged branch.
Zenithmon and Engine are both `direct`: step 4 does nothing and step 7 commits
onto `baseBranch` in place, so there is no branch to withhold. The two
reachable states are to commit and park at In Review, or to leave the work
uncommitted — and the second is worse than anything it prevents, because the
next tick's step-3 precondition treats a dirty tree as fatal and refuses the
whole queue.

So in `direct` mode: **commit, then park at In Review.** Nothing is pushed
(`push` is false), the human reviews a commit on a local mainline, and a
rejection is a `git revert`. What `human-gate` buys there is that the ticket is
not CLOSED and the reporter is told — not that the code is withheld.

## 3. Preconditions

In `repo`:

```
git -C <repo> status --porcelain     # must be empty
git -C <repo> rev-parse --abbrev-ref HEAD   # must equal <baseBranch>
```

Dirty tree or wrong branch → comment + Blocked, stop. **Never stash,
never reset.** For `branching: "direct"` this check is the only thing
standing between you and someone else's uncommitted work: treat a dirty
tree as fatal, not as something to work around.

## 4. Branch

- `branching: "branch"` → `git -C <repo> switch -c <branch>`
- `branching: "direct"` → do nothing; work on `baseBranch` in place.

## 5. Dispatch the worker

You do **not** implement the ticket. Run `Tools/zagent/build_prompt.ps1` to
assemble `.zagent/run/<KEY>/prompt.md` (and `.zagent/run/<KEY>/filelist.txt`
beside it — read by the excursion check below), then spawn a worker at the
routed model with that file's contents:

```
pwsh -NoProfile -File Tools\zagent\build_prompt.ps1 -Key <KEY> `
  -Files <file1>,<file2>,... -Boundary "<one-sentence stop condition>" `
  [-Rulings <path1>,<path2>,...]
```

**This run also writes `.zagent/run/<KEY>/predispatch.json`** — the
WORKER-MOVED-NOTHING baseline I1 relies on at step 6.4: `head`, `branch`,
the sorted `git status --porcelain` line set, the board row's
`status`/`assignee`/`updatedAt`, the claimed `harness`, and `at`.
**Deliberately the LAST write before dispatch, never `claim`'s own** — a
claim is itself a board commit, so a claim-time baseline always sees a HEAD
that has since moved; step 4's `git switch -c` runs between claim and here
too, so a claim-time baseline would report a phantom `branchMoved` on
every `branching: "branch"` ticket. Taken last, the only moves it can
attribute to the worker are ones the worker itself made.

**Six remembered rules for hand-assembling this prompt are now two
judgement calls plus a deterministic slice of two files already on disk.**
The script reads `.zagent/run/<KEY>/ticket.json` and `body.md` — written by
`claim`, which must already have run — and does the mechanical half:
the five fixed opening clauses (DO NOT run gates/build/test/commit/push;
Files you may edit; Conventions, verbatim; Report back; Boundary), slicing
the `## Goal` and `## Definition of Done` sections out of `body.md`
(never the `definitionOfDone` array — below), quoting `conventions` from
`ticket.json` verbatim, and inlining any `-Rulings` files verbatim under a
`## Rulings that govern this work` heading. Exit codes: `0` ok, `1` on any
failure (message on stderr, and recorded to `.zagent/last.exit` plus a
per-ticket `.zagent/run/<KEY>/build_prompt.exit` — see "Running the CLI,
and reading its output" above).

Two inputs stay yours, because neither is mechanical — the script only
formats and writes what you hand it:

- **`-Files`** — the exhaustive list of files the worker may edit.
  Deriving it is everything in the section below; the script cannot do
  this part.
- **`-Boundary`** — the one-sentence stop condition. "The known edge of
  this ticket" is a reading of the ticket, not a fact a script can
  compute.

I3 still applies — `bodyPath` is a supplement, never the carrier — the
script is simply what does the carrying now.

**★★ THE SCRIPT REFUSES RATHER THAN FALL BACK TO THE `definitionOfDone`
ARRAY, WHICH IS THE ONE RULE IT EXISTS TO ENFORCE.**

**The array holds the checkboxes and nothing else — never the prose that
qualifies them.** ZM-23's body carries *"What the rulings changed about this
slice's scope"* and *"★ ITEMS 1 AND 2 ARE ONE INDIVISIBLE COMMIT"* **outside**
any checkbox, and both are load-bearing instructions. I3 says paste the ticket
BODY; the array is for COUNTING boxes, not for quoting them. When `body.md`
has no `## Definition of Done` heading (any level), the script exits 1 rather
than silently reach for the array instead: *"Refusing to fall back to the
ticket's definitionOfDone array — that array holds only checkbox text and
drops any prose between or around the boxes (see ZM-23). Fix the body, or if
this ticket genuinely has no DoD section, that is itself something to report
rather than paper over."*

A mutilated spec used to be invisible to the worker, which has no shell and no
board and cannot tell it was cut — so nothing downstream reported it either.
Now nothing downstream gets the chance to receive one: the script has no code
path that falls back to the array at all, which is a stronger guarantee than
"prefers `body.md`". `Docs/TickProtocolHistory.md` keeps the worked example,
from when `parseTicket` truncated every checkbox at its first newline.

**How to derive `-Files`, because the list is load-bearing in
BOTH directions.** Start from the DoD's nouns and follow them into the
repo — but **grep the SYMBOL, never the prose.**

**★★ THAT DISTINCTION COST 4 OF 8 SITES ON ZM-69.** The ticket was a
REVERSAL: Thornacre carried a deliberate three-site tripwire asserting it has
no gym door, and `ZM_ThornacrePlacement.h` says so out loud — *"adding a gym
door means a human edits this line … so the edit reds instead of shipping."*
The orchestrator's survey enumerated files that MENTION "gym door" and found
four. It missed `ZM_Tests_SceneRegistry.cpp`, which never uses that phrase —
it says *"the Gym"* — and which holds the one omission that mattered: without
a `{ ZM_SCENE_GYM1, "Gym1" }` row the authored door is ACCEPTED, stalls in
`WAITING_FOR_SCENE`, and is a black screen **with every unit still green**,
including the three units asserting the gym's absence, which stay green
because the absence stays real.

The sites that matter ASSERT AN ABSENCE, and an absence is spelled as an
identifier (`ZM_SCENE_GYM1`, `ZM_IsSceneRegisteredForLoad`), never as the
English description of the feature. Nouns are prose. **On any ticket that
reverses a recorded assertion, the survey pattern is the symbol.**

Then make one deliberate judgement: **if a file would let the worker
decide something the ticket does not contain, leave it out and say why in
the Boundary.** On ZM-23, adding `ZM_HumanData.*` would have let the worker
invent a new authored outfit colour and "finish" the ticket — a content
decision no one had made. Leaving it out is what turned that into a
reported blocker instead of a silent one.

The opposite error costs a dispatch and is just as easy. That same
ticket's fix-forward prompt said *"check whether any unit pins
`ZM_STORY_FLAG_COUNT` and update it if so"* while the exhaustive list
omitted the only file that pins it — two instructions that cannot both be
obeyed, and the worker correctly refused to edit anything. **A step that
instructs an update must name the file that receives it, or say "report,
do not edit".** When you cannot tell which way to err, err narrow and say
in the Boundary that a wider list is available on request: a worker that
stops and reports costs a message, one that improvises costs a revert.

**★ AND OPEN THE WORKER'S PROMPT WITH THE BLAST-RADIUS SURVEY, AS ITS FIRST
INSTRUCTION.** Deriving the list is your job, but VERIFYING it is work you are
the worst-placed party to do: you must survey before you have read the code the
ticket is about, while holding the whole tick's state. The worker has the task
context and a fresh budget. So make the list a hypothesis the worker is required
to falsify before it edits anything:

> **Step zero, before you edit anything.** For every identifier and every
> behaviour you intend to change, grep the whole of `<game>` and confirm each hit
> is inside the list below. Read the UNITS that reference them, not just the file
> names — a test can pin your change without naming it in a way that greps.
> **If ANY hit lands outside the list, STOP and report the complete corrected
> list before making a single edit.** Say what the survey found even if it found
> nothing.

**`build_prompt.ps1` EMITS THIS CLAUSE NOW — you no longer paste it by hand.**
It was mandated here and absent from the script (ZEN-89), so every tick that
trusted the script silently lost the one control that catches an under-scoped
list; `Test-BuildPrompt.ps1` now asserts the prompt OPENS with it, not merely
that it contains it. **So run `build_prompt.ps1` for every dispatch, including
a fix-forward.** A hand-written prompt also skips `filelist.txt`, and `finish`
then reports excursions as UNVERIFIED rather than clean — which is what
happened on two of the three tickets in the 2026-09-20 audit.

★ **A STALE `filelist.txt` IS WORSE THAN AN ABSENT ONE, AND NOTHING TELLS THEM
APART.** `finish` warns loudly when the file is missing. When a fix-forward
dispatch reuses the file list from the FIRST dispatch — which is what happens
if you re-run the worker without re-running the script — the file is present,
stale, and silently describes the wrong scope: on ZEN-40 it would have flagged
six legitimate files as excursions. Re-run `build_prompt.ps1` with the
corrected `-Files` before every re-dispatch.

Three tickets paid for this clause. ZM-23 under-scoped despite a careful survey,
because a ruling asserted the missing file was unaffected. On ZM-67 the same
prompt clause found a comment that the ticket's own change had falsified, in a
file the worker was told not to edit — it reported it instead of editing it,
which is exactly the intended behaviour. A survey that costs ten worker-minutes
is cheaper than a dispatch.

**★★ AND ON ZM-69 IT WAS THE ONLY CONTROL THAT COULD HAVE CAUGHT THE DEFECT.**
The file list was short by two files, and one omission was fatal: `Gym1` had no
row in `ZM_SceneRegistry.h`, and `IsWarpDestinationValid` consults only the
compiled world table, never the registry — so the authored door would have been
ACCEPTED, stalled in `WAITING_FOR_SCENE`, and shown a black screen **with every
unit green**, including the three units asserting the gym's absence, which stay
green because the absence stays real.

Nothing else in the pipeline could see it. `contractValid` answers routing. The
reachability scan reads `protectedPaths` and nothing here is protected. The
drift resolver checks that citations RESOLVE, not that a list is COMPLETE. And
no gate can catch a runtime stall. **A ticket can be contract-valid, fully
reachable, and still unbuildable as scoped** — and `contractValid` has no term
for "the file list is sufficient", nor could it have one.

★ **Note what the worker had to do to find it: READ a file it was forbidden to
EDIT.** A file list bounds writes, not reads, and the survey must read wider
than it edits. Say so in the prompt if the list is narrow.

**Reconcile the DoD against the repo BEFORE you inline it — the ticket
is the older document.** I3 means whatever you paste becomes the
worker's instructions verbatim, so a stale line does not sit there
harmlessly: it competes with everything else in the prompt, and the
worker cannot tell which half is current because it has no shell and no
board. Every body examined so far had drifted, seven for seven.

Three things to check, in this order:

1. `.zagent/run/<KEY>/drift.txt`, if the claim wrote one — it names
   every cited path and symbol that does not resolve here, and where a
   file of that name actually lives.
   **`citations.lines` is the half nothing can check.** A line number is
   true only relative to a commit nobody recorded, so it goes stale on the
   next edit ABOVE it and no resolver can tell. ZEN-5's body cited line
   522 for a specifier that was at 540; the drift check was silent and the
   orchestrator caught it only by opening the file. If the payload lists
   any, open the file and correct them before inlining.
2. Pin SITES. A DoD naming `Status.md`/`zm-tests.yml`/`zagent.project.json`
   predates the baseline refactor; inline "every affected game's pinned
   baseline bumped from an OBSERVED run" instead.
3. The ticket's central claim. A DoD saying "X has no Y" when X grew a Y
   last week describes work that is already done, and a worker handed
   that will invent something to do.

   **★★ AND ITS PROPOSED MECHANISM, WHICH IS A SEPARATE CLAIM AND THE MORE
   EXPENSIVE ONE TO GET WRONG.** A Goal does not only assert a symptom; it
   usually asserts a CAUSE, and nothing else in this pipeline checks that.
   `contractValid` answers routing. The drift resolver checks that citations
   RESOLVE, not that they are true. The reachability scan reads paths. A
   plausible, confidently-worded, wrong diagnosis passes all three and then —
   because I3 inlines the Goal verbatim — becomes a standing instruction
   competing with the evidence.

   Measured on ZEN-40, whose Goal read *"which teardown order (asset handles
   releasing after the registry, or a render-resource release after the memory
   manager, in the pattern `d45bd7b3` fixed for handles)"* and named the
   Vulkan allocator. There was no ordering defect at all: the bug was
   unbounded accumulation in `Flux_Primitives`, two subsystems away, and the
   allocator was the DETECTOR. That cost a full dispatch — recovered only
   because the worker hit its boundary and reported a corrected file list
   instead of editing files that did not contain the bug.

   Its symptom claim was false too, in the same ticket: *"every windowed run
   exits non-zero"*, when two of three measured windowed runs exited 0 and
   only the photo tours reproduced. **Reproduce the symptom yourself before
   you inline a cause**, and if the ticket's mechanism does not survive
   contact with the code, say so in the prompt as measured evidence and let
   the worker read the code rather than the theory.
4. **Any claim YOU are about to make about what existing code DOES —
   checked against the code, never against its comment.** This file
   already distrusts documents; it applies that suspicion only to tickets
   and docs, and the stale document on ZM-65 was a comment *inside the
   file the worker was about to edit*. The oracle's block comment says it
   measures "ONE column"; the code already walked the whole table. That
   sentence went into the prompt as "make it walk the table", and the
   worker had to push back. No harm because it did — but an instruction to
   restructure working code is exactly what a more compliant worker
   IMPLEMENTS, and the churn would have gated green.

**★★ AND RULE 4 IS NOT A DoD RULE. EVERY FACTUAL CLAIM IN THE PROMPT IS YOURS.**
It is written above as item 4 of a list headed *"Reconcile the DoD against the
repo"*, which reads as scoped to the TICKET's claims — and the claims that
actually bite are the ones you wrote yourself. Most of a good prompt is
scaffolding: *"X does Y"*, *"follow the pattern in Z"*, *"the helper returns a
bool"*. **No other step checks any of it.**

Measured on ZM-67, in a single prompt, on the tick immediately after this failure
was written up one level down:

- *"include an assertion that fails if the dump call returns false"* —
  `Flux_Screenshot::RequestDump` returns **void**.
- *"put them under the exe's own output directory, **as the gallery test does**"* —
  the gallery test writes to `<repo>/Build/artifacts/…`, not the exe dir.

The worker flagged both instead of obeying them. Note the asymmetry if it had
not: the void-return instruction fails **loudly** at compile, the wrong directory
**works** and quietly writes outside the gitignored root. So open the file before
you assert what is in it — including, and especially, in the sentences that are
not about the ticket at all.

**★★ A RULING'S "WHAT SURVIVES" LIST IS A SURVEY, NOT A RULING — AND I3 DOES NOT
COVER IT.** I3 says paste the governing rulings verbatim, and their authority is
the whole reason they exist. But a decision record contains two different kinds
of sentence, and only one of them is a decision:

| | |
| --- | --- |
| the **judgement** — *"append an outfit slot"*, *"retire phase 8"* | binding, paste it verbatim |
| the **survey** — *"what survives"*, *"what is unaffected"*, *"the blast radius is"* | a claim about what the code does TODAY, and exactly the half that goes stale |
| the **why** — *"because X is not authored yet"* | the judgement's PRECONDITION, and a ticket may have discharged it |

**★★ THE THIRD ROW IS NEW, AND WITHOUT IT A LEGITIMATE TICKET READS AS A RULING
VIOLATION.** `ZM-D-196` Ruling 2 says *"Explicitly **NO gym door**"*. Pasted as a
judgement — which is exactly what "paste the ruling verbatim" naturally produces,
because the Decision line is the ruling — it forbids ZM-69 outright. Its **why**
says *"Gym1 … declared but unauthored, and `IsWarpDestinationValid` consults only
the compiled tag list — so a door to an unauthored scene is a permanent black
screen"*. ZM-69 **authors Gym1**. The ruling is a precondition that the ticket
satisfies, not a prohibition it breaks, and the tripwires guarding it may move
now precisely because they could not before.

So **always paste the WHY beside the Decision**, and ask which of the three kinds
of sentence you are relying on. A judgement whose stated reason no longer holds
is not binding in the same way — but only its reason can tell you that, and a
verbatim Decision line hides it.

ZM-D-208 recorded that *"the FSM latch in `ZM_Tests_TrainerSightFsm.cpp`"*
survives Ruling 3. It was pasted verbatim, correctly, and used to bound the
worker's file list — the file is absent from that list **because the ruling says
it survives**. It does not: three `Gate_*` units there pin the flagless arm
against the Rambler's **roster row**. Two red; the third stays green while its
own `"the FLAGLESS rambler"` label becomes false. The ruling surveyed at FILE
granularity and the breakage was at UNIT granularity.

Cost: one opus dispatch, one slot in the one-ticket-per-repo lock, and a second
consecutive Block on a ticket that had been re-filed specifically to fix the
first one. **Paste the ruling verbatim AND re-derive its survey against the code
before you let it bound anything.** This protocol already distrusts citations; a
ruling's survey is a citation wearing a ruling's clothes.

The mechanical form is cheap, and it is what the ZM-23 worker itself proposed:
**grep the whole game for every identifier the DoD will change, and confirm each
hit is inside the file list.** One `rg ZM_TRAINER_ROUTE1_RAMBLER` would have
found it in a second, before the claim. Better still, put that grep in the
WORKER's prompt as its first instruction — see the note on the file list above.
It has the task context and a fresh budget; you have neither, and you must survey
before you have read the code the ticket is about.

**Write down every correction in the work log.** A prompt that silently
disagreed with the ticket is indistinguishable, afterwards, from a
worker that ignored it.

**★ EXCURSIONS OUTSIDE THIS LIST ARE ALLOWED, AND NOW CHECKED, NOT JUST
AVOIDED.** `build_prompt.ps1` writes `.zagent/run/<KEY>/filelist.txt`, and
`zagent finish` (step 8) diffs it against the changed set before it will
close the ticket: a changed file neither on the list nor named (a plain,
case-insensitive substring match) in the `--comment`/`--worklog` text
refuses. Going outside the list is not forbidden — the survey above is done
before you have read the code the ticket is about, and a worker with task
context may correctly find it needs a file the survey missed — only doing so
*silently* is. So when the worker's report says it reached outside the list
(see the WORKER role definition), carry that explanation into the
work log verbatim — that is what satisfies the check. **Do not revert the
work to dodge this**; a reverted, hidden excursion is worse than an explained
one. A missing `filelist.txt` (an older run, a hand-run tick) WARNS instead
of refusing: there is nothing to compare against, so the excursion state is
UNVERIFIED, not clean.

**★★ A DoD THAT ASKS FOR *BEFORE* EVIDENCE ORDERS YOUR DISPATCHES. DECIDE THE
STAGES BEFORE THE FIRST ONE.** This step reads as one dispatch per ticket, and
nothing in it warns that an evidence-producing DoD item constrains the ORDER in
which things may be authored.

ZM-67's DoD ended *"Before/after captures of all three props recorded in the work
log, so the visual sign-off has something to judge"*, on a `human-gate` ticket —
so that evidence **is** the deliverable. No capture harness existed. **A BEFORE
image can only be taken from a tree that has the harness and does NOT yet have
the change**, and one worker authors both at once. A loop dispatching once lands
them together, captures only an AFTER, and files a work log whose BEFORE row is
empty or — worse — reconstructed from prose. On a `human-gate` ticket that
silently destroys the thing the gate exists for.

So: **the harness that produces the evidence must land, and be run, before the
change it measures.** ZM-67 ran as stage 1 (the harness alone, with the
appearance explicitly frozen and the boundary saying so) → build → capture BEFORE
→ stage 2 (the visual work) → build → capture AFTER. Both stages land in ONE
commit; the staging is a dispatch ordering, not a property of the repo, and the
harness's own comments must not claim otherwise.

**This is the same shape step 6.2 already documents one level up**, where the
ZM-65 nine-step order exists precisely because *"steps 2 and 5 are the same
command and print the same success signature"* and the freeze has to go BETWEEN
them. That lesson was learned for **pins** and never generalised to **evidence**.
Three practical consequences, all measured:

- **Copy the BEFORE artifacts out of the shared output directory** before the
  AFTER run — a harness that clears stale captures at `Setup` will delete them,
  and a stale TGA pasted into a work log as "BEFORE" is worse than a missing one.
- **The framing is the contract.** Two captures are comparable only while the
  lens derivation and the shot stems are unchanged. Say so in stage 2's boundary.
- **Look at the pixels.** Structural checks — the file landed, it re-loads, the
  subject's patch is in bounds — are all true of a photograph of the wrong thing.

Dispatch shape — **your harness's own subagent mechanism, never a shelled-out CLI**:

```
DISPATCH{ role: WORKER, tier: <routing.model>,   # a TIER, never a model name
          title: '<KEY> — <short title>',
          prompt: <the full text of .zagent/run/<KEY>/prompt.md>,
          mode: background }
```

What each verb IS on the harness you are running under is the `Controls` table of
your entry point. A verb your entry point does not name is absent here, and each
clause below says what to do when it is.

- **DISPATCH names a ROLE and a TIER, never a tool list.** A tool list written at
  the call site can be edited by whoever writes the call; a role definition cannot.
- **The WORKER cannot run a gate, and cannot dispatch anything of its own.**
  Whether those absences are *enforced* by your harness or only *declared* in the
  role's text is `Worker tool denial` in your entry point. When it reads
  **declared**, I1 is carried by WORKER-MOVED-NOTHING at step 6.4 instead, and that
  check is not optional on such a harness.
- **Pass the prompt text, not a path.** `prompt.md` on disk is the
  durable copy that makes a crashed firing reconstructible (I6); the
  dispatch call still carries the whole thing inline (I3).
- **Write the returned report to `.zagent/run/<KEY>/report.json`**
  yourself when it lands. Nothing else does, and step 8 reads it.
- **mode: background**, then AWAIT. If your entry point names no background mode,
  run it in the foreground and say so in the work log.
- If elapsed wall-clock exceeds `guardrails.ticketTimeoutMinutes`, ABORT and treat
  it as a failed attempt. **If your entry point names no ABORT, the cap is advisory
  here** — record that it was exceeded and not enforced, rather than pretending it
  was. A guardrail overridden without a record is the state this replaced.

  **That number is already resolved for THIS ticket's complexity** —
  read it, do not re-derive it and do not substitute your own. It was
  one flat 30 for every ticket and was never once enforced, because
  enforcing it would have been wrong: ZEN-2's opus worker ran 43 minutes,
  45% over, and produced the chunk-span design that shipped. Killing it
  would have destroyed correct work and cost a full re-dispatch, so the
  guardrail was skipped and silently stopped existing. **A limit that is
  right to ignore is worse than no limit** — it trains you to skip the
  check, and then it is not there on the run that genuinely wedges.

  The tiers come from measurement, not taste: every observed run over 15
  minutes was opus on a COMPLEX ticket, and every sonnet/haiku run
  finished well inside 30.

  It is still a wall-clock cap, which is the wrong SHAPE for "wedged" — a
  COMPLEX ticket working steadily for 40 minutes is not wedged, while a SIMPLE
  one silent for 10 is. **No harness's dispatch mechanism exposes that
  signal**, so this is the closest enforceable approximation on all three.
- **`routing.effort` is ADVISORY, on every harness.** Effort comes from the role
  definition; it cannot come from the dispatch, because effort and model escalate
  independently (effort from complexity, model from complexity *and* risk), so no
  per-tier role can carry the right effort for every ticket routed to it. Record it
  in the routing line so a mis-sized ticket stays visible, and do not pretend it was
  applied.
- `workerTools` in the payload is a **cross-check, not a control**. If it
  names a tool the WORKER role does not grant, that is a contract
  mismatch worth a Suggestions row — not a reason to widen the role.

**A dead dispatch is infrastructure, not a verdict.** A DISPATCH returns
null if the worker dies on a terminal API error or is skipped mid-run.
That is the worker-side twin of a `zagent` infrastructure failure — a
transport or environment problem, not a gate disagreement — and it gets
the same answer: **do not treat it as a red.** A red means the gates ran and
disagreed with the work. A null means no work happened at all, and
fix-forwarding it just re-runs the same failure `fixForwardAttempts`
times before parking an innocent ticket as Blocked — then does it to the
next ticket, and the next, until `maxConsecutiveBlocked` trips the
breaker on three tickets that were never even attempted. So on a null:
comment what happened, **move the ticket back to `previousStatus` rather
than Blocked**, and stop the tick. Report it as infrastructure.

`previousStatus` is on the claim payload, and that is the ONLY place the
value survives: both claim paths move the row to In Progress and then
read it back, so once the claim returns, nothing else can answer the
question. Read it out of `.zagent/run/<KEY>/ticket.json` — I6 forbids
reconstructing it from what you remember, and "it was probably To Do" is
wrong for every ticket a human re-ran out of Blocked.

Whatever the worker claims about correctness is discarded (I2). Its
report is _proposed text_ that you apply (I4).

## 6. Verify

**★ THE ORDER IS TOTAL, AND IT IS NOT THE NUMBERING BELOW.** `verify` cannot sit
inside 6.2, because 6.2's own prose says it certifies *"the reviewer's doc
findings"* and the reviewer is 6.3. It also cannot come after 6.4, because
`guard` compares the current changed set against the recording `zagent gates`
made and only a `gates` run refreshes that — measured on ZM-65, `gates.json` at
guard time held a 4-file changed set while the tree had 6, both additions being
edits this step had just instructed. Benign there because the gate LIST was
unchanged; not benign on a ticket whose post-measure edit reaches a new area. So:

```
owns → measure → your edits → reviewer → apply findings → verify → guard → coverage → owns
```

1. `zagent owns <KEY>` — exit 5 → abandon path (step 9).
2. **`Tools/tick_gates.ps1`, backgrounded, in two phases:**

   ```
   pwsh -NoProfile -File Tools/tick_gates.ps1 <KEY> -Phase measure
   ...write the pin, the Status.md narration, the reviewer's doc findings...
   pwsh -NoProfile -File Tools/tick_gates.ps1 <KEY> -Phase verify
   ```

   It runs `zagent gates <KEY>` and then every line that prints, in order,
   appending to `.zagent/run/<KEY>/gates.log`. First non-zero stops.

   **★★ READ `.zagent/run/<KEY>/tick_gates.json`. Full stop — not "the exit
   code, or the JSON".** The shell's exit code is corruptible by a pipe or a
   trailing `echo` and the harness notification repeats the corrupted value;
   the JSON is not. It is written on every completion path including the
   top-level catch, and it now carries the verdict AS DATA:

   ```json
   { "state": "complete", "exitCode": 4, "headSha": "7d3d6c49…",
     "startedUtc": "…", "finishedUtc": "…", "steps": [ … ], "skipped": [ … ] }
   ```

   Codes: `0` green · `2` regen failed · `3` a created file never reached the
   compiler · `4` a gate line failed · `5` no gate list · **`10` MEASURED: a
   pin needs bumping — not a failure.**

   Three fields exist because reading this file used to be ambiguous — and
   two of the three no longer have to be read *here*, because `guard` (step
   6.4 below) now reads them for you before it will report a pass:

   - **`state`** is `running` while a phase is in flight. A killed run leaves
     `running`, not the previous run's verdict. **`guard` now folds this into
     its own exit** — a would-be PASS whose verdict is not `state: complete`
     is overridden into a failure. See 6.4 for the mechanism.
   - **`headSha`** is the commit the phase gated. **If it does not match
     current HEAD, the verdict describes a different tree — treat it as
     absent.** This directory has held a stale `all gates green` from an
     unrelated tree at claim time, and a superseded `exit 4` mid-run.
     **Also folded into `guard` now**, for the same reason: a stale verdict
     from a different tree must not read as "verified".
   - **`skipped`** names the gate lines that never ran, and this one is
     **still yours to read by eye** — the freshness fold answers "is the
     verdict current", not "did every gate in it actually run". A failing
     line stops the list, and the remainder used to be silently absent — on
     one ticket that meant the unit gate and `doc_lint` never ran, so a
     freshly-bumped pin was unconfirmed and the linter had never seen the
     edited docs.

   **A red gate VOIDS every gate behind it.** Read `skipped` before concluding
   anything about what was verified — `guard` does not do this part for you.

   Four rules the script enforces, each added after it was forgotten once.
   Its header carries them; the failures are why:

   | The script | The failure it closes |
   | --- | --- |
   | regens when the tree holds a created source file, and treats a non-zero regen as FATAL | Sharpmake bakes the file list into the vcxproj, so a new `.cpp` enters the build only at a regen — and `regen.ps1` exits 3 regenerating NOTHING while stale projects stay on disk. ZM-27: 3 new files, build exit 0, zero occurrences of the new TU in the log |
   | asserts every created `.cpp` BY NAME in the build log | a green build proves what was IN the project compiled. The unit count still moves, by however many tests landed in files that already existed — and the pin bump then ratchets it in |
   | asserts every created `.inl` is `#include`d by something tracked | an `.inl` is not its own TU, so the by-name build-log check cannot see it — and every Flux unit suite in this repo is a `<Module>.Tests.inl` included from the bottom of its `.cpp`. Create one, forget the `#include`, and the build is green, the unit count does not move, and nothing says a word. Measured on ZEN-40, where the script printed "no created source files; skipping regen" with a new `.Tests.inl` in its own changed list |
   | runs the unit gate ALONE first, and stops | the count must be OBSERVED before it can be narrated, so the doc edit necessarily lands after a gate run. ZM-27 spent ~25 minutes re-certifying three comment edits only `doc_lint` reads |
   | owns each gate as a child process, waits unbounded, sweeps stray game exes on every exit path | `zenith test Zenithmon --headless` runs past 20 minutes. A killed gate reports no exit code — a timeout is indistinguishable from a failure — and leaves the exe holding build outputs |

   **★★ ASK WHETHER THE GATE LIST EXECUTES THE DIFF AT ALL — a green run over
   code you did not change is not verification.** The union covers whole
   directories, not languages, and for a long time nothing in any list ran
   anything that was not C++. Two of the three tickets in the 2026-09-20 audit
   went green on eleven C++ game builds having executed **zero lines** of their
   own diff: ZEN-79 delivered Vitest coverage in `Tools/Vault`, ZEN-8 a
   `doc_lint` check plus a `Tools/Test_*.ps1` assert script. In both cases the
   only real verification was a hand run.

   The `Tooling` category now owns `Tools/*.ps1`, `Tools/zagent/**` and
   `Tools/Vault/**` and runs the assert scripts, the Vault Vitest suite and
   `doc_lint` for every game, so that hole is closed for `Tools/`. The habit is
   not: when the gate list finishes, check that something in it actually ran
   the changed lines, and if nothing did, **run the suite yourself and record
   it in the Coverage row** rather than reporting the gates' colour. That row
   exists for exactly this, and "nothing" is a legal answer.

   (`Tools/unit_baselines.json` is deliberately NOT in the `Tooling` paths: a
   one-line pin bump must not drag a five-minute Vault suite behind it, the
   same reasoning that keeps `Tools/**` out of `Engine`.)

   **Never hand-assemble the list, and never run a line in the foreground.**
   `zagent gates` takes the same changed set `guard` computes and UNIONS in
   every category whose declared `paths` the diff reached, so a `Zenithmon`
   ticket that edits `Zenith/**` builds and tests Combat and checks the engine
   pin too. It only ever ADDS — the ticket's own list stays a prefix — and it
   RECORDS the selection, which `guard` re-derives and compares against.

   That union exists because four consecutive tickets edited engine code under
   a one-game gate list; ZM-50's own Goal said *"the fix is engine-side"* while
   its category said otherwise, and only the category had mechanical effect.
   The Engine list additionally names every game, which the union cannot
   supply: a public header's blast radius is everything that INCLUDES it, and
   `Games/Combat` — once the only game those gates built — references
   `Zenith_TerrainComponent` in zero files while three other games reference it
   in twenty.

3. **Reviewer pass** when `review.required` is true in the payload.

   ```
   DISPATCH{ role: REVIEWER, risk: <the ticket's risk>,   # LOW | MEDIUM | HIGH
             title: '<KEY> — review',
             prompt: <names .zagent/run/<KEY>/diff.patch BY PATH>,
             mode: background }
   ```

   Run the REVIEWER when `review.required` is true in the payload, **OR** when your
   entry point's `Review policy` reads `always`. The two are ORed and a profile can
   only ADD review, never remove it.

   **The REVIEWER is selected by RISK, not by tier** — your entry point's
   `Reviewer by risk` table. Risk is a sizing field and a poor predictor of *whether* a
   ticket needs scrutiny (which is why `review.required` does not use it), but once a
   review is happening it is a reasonable predictor of *how much reviewer* it deserves.
   **A LOW-risk ticket can still require a review**, via the DoD-assertion scan, so the
   table has a LOW row.

   **★★ DISPATCH IT ONLY AFTER EVERY EDIT OF YOURS IS DONE** — the pin, the
   `Status.md` narration, the hash table, everything. Not in parallel with
   them, however tempting the wall-clock saving is. **The REVIEWER reads
   a LIVE working tree and cannot know which commit it is on.** Dispatched
   alongside orchestrator edits it will read a file mid-write and report
   correct values as stale — measured: a reviewer opened with a blocking
   *"the LIVE PIN and SHA256 table are stale relative to disk"*, citing exact
   line numbers, having seen a block added second but not a change made
   first. Its citations no longer matched the file, which was the only tell.
   An orchestrator that believed it would have "fixed" correct values or
   Blocked a green ticket, and an unattended loop cannot tell that apart from
   a real finding.

   That is why this is step 6.3 — after the gates, before the guard. The
   ordering is not incidental.

   **★★ THE REVIEW COVERS THE WHOLE DIFF — YOUR OWN EDITS INCLUDED. Say so in
   the prompt.** On ZM-65 the review returned three BLOCKING findings and
   **all three were the orchestrator's**, not the worker's: a banner left
   announcing a row unfrozen fifty lines above the frozen row; a decision-log
   entry describing a "source-only" commit that "moves no scene byte", in a
   commit that re-authored a tracked binary; and **a fabricated causal
   explanation** committed to two living docs, which the terrain recipe
   inverts. The worker's code was clean on every claim.

   Everything else in this protocol points at the worker — I1 says the worker
   only authors, I2 says never trust it, and this step says "review the diff",
   which a prompt will happily narrow to "review the worker's changes" and
   thereby exclude every one of those three. **Your edits are the ones no other
   control covers:** the worker has exited, the gates cannot read prose, and
   `doc_lint`'s checks are registry counts, roadmap ids, superseded markers,
   file-existence claims, links and pin equality — **none of them reads whether
   a comment's assertion matches the line beneath it.** And you write the
   highest-leverage text in the repo: the pin narration, the asset hashes, the
   decision-log entry that becomes the binding record.

   Two habits close most of it before the reviewer ever runs. **After a freeze,
   grep every file it touched for the value you just changed** — this file
   already states that rule for `Status.md`, and two of the three findings were
   one `grep -n "UNMEASURED"` away. And **treat an invented MECHANISM as a claim
   needing a source**: "because the containments differ" had no citation because
   there was nothing to cite.

   **Write the diff to `.zagent/run/<KEY>/diff.patch` and give the reviewer
   THAT PATH.** Do not paste the diff into the prompt: it exceeds a single
   read for any COMPLEX ticket (2,260 lines on one) and costs the relay
   twice, once to read and once to send. The reviewer can read files itself,
   so it consumes the diff on its own budget — and the step's actual reason for
   "inline it" (the REVIEWER cannot run commands and cannot *produce* a diff) is
   fully satisfied by handing it a pre-computed one. Say in the prompt that
   the patch file is immutable and the working tree is not.

   Inline `review.reasons` — they name the CLAIM to check.
   The REVIEWER role is read-only by definition, so
   it cannot quietly "fix" what it finds and hand you back a diff you
   did not gate. Findings go in the work log; a finding that contradicts
   a gate result blocks. A null return here is infrastructure, same as
   step 5 — it is not a pass.

   **Do not re-derive this from `risk` and `reviewOn`.** They used to be
   the whole rule, which conflates two different questions: risk is a
   SIZING field — how much model does this deserve — while scrutiny is a
   property of what the ticket CLAIMS. ZEN-2 was filed LOW, honestly, and
   its Definition of Done said the recorder was "provably unchanged"; the
   review that ran anyway found the recorder had zero coverage while the
   test file asserted otherwise. `review.required` is now the OR of
   `reviewOn` and a scan of the DoD for claims a gate cannot settle — an
   equivalence, an absence, or an assertion about the tests themselves.

   **★ A THIRD TERM BELONGS IN THAT OR AND IS NOT THERE YET:
   *the ticket's subject matter is governed by a recorded decision*.**
   ZM-23's DoD makes no equivalence, absence or test claim. It was reviewed
   purely because `MEDIUM` sits in `reviewOn` — the luck of a sizing field
   chosen before anyone saw a diff — and that review is the only thing that
   caught it contradicting `ZM-D-203` Ruling 3. The term is computable from
   the same decision-log match I3 now requires, and it beats risk as a
   predictor because it keys on what the ticket TOUCHES rather than on how
   big someone guessed it was. Until the board computes it: **if a ruling
   governs the subject matter, review it whatever `review.required` says.**

   **★ A ZERO-DIFF TICKET HAS NOTHING TO REVIEW. Do the review's JOB instead, and
   say so.** When the worker changed no files, `review.required` is still true and
   there is no patch — so dispatching a reviewer hands it an empty diff, which
   returns nothing and reads exactly like a pass. Skipping the step silently is no
   better: the board then shows a required review that never happened.

   In its place, do what the step is FOR: **verify the worker's central claim
   yourself against the code** (I2's second half), and record in the work log that
   you did and what you checked. On ZM-23 that meant opening the three units the
   worker said would break and confirming them at the exact cited lines, then
   recomputing the palette arithmetic independently — about four minutes, and it
   turned an assertion into a corroboration. An impossibility accepted on hearsay
   Blocks a ticket; one rejected without checking burns a dispatch.

   **And ask the reviewer, when there IS a diff, to name the categories it did
   NOT check.** A category that came back clean and a category nobody looked at
   are different facts, and only one of them is otherwise visible. On ZM-67 that
   section is what surfaced the fact that **no gate compiles the non-tools path** —
   nothing else in the pipeline would have said so.
4. `zagent guard <KEY>` — **with the key, and with no `--file`**.
   Non-zero → Blocked regardless of gate colour.

   **★★ `guard` ALSO OVERRIDES A PASS WHOSE GATE VERDICT ISN'T FRESH.**
   Before returning its own 0, it re-reads this key's `tick_gates.json`: a
   `state` that isn't `complete`, or a `headSha` that doesn't match current
   HEAD, turns a would-be PASS into a failure (`gateVerdictFresh: false` on
   the payload, naming why). It only ever turns a WOULD-BE PASS into a
   failure, never the reverse: a guard refusal that already fired for its own
   reason is untouched, because staleness has nothing to add to a refusal
   that already exists.

   The key is what makes step 6.2 mechanical rather than remembered.
   `guard` re-derives the gate selection from the ticket and the CURRENT
   changed set and compares it against the `gates.json` that `zagent
   gates` recorded: no recording, or a recording that no longer covers
   the diff, is a non-zero exit naming the gates that never ran. It is
   checked here because `guard` is already the mandatory merge-blocking
   step — a rule enforced at a check that already fails is a rule, and
   the same rule in this paragraph is a hope. The failure it catches is
   silent by construction: every gate that DID run passes, so the ticket
   merges green with a whole area unverified.

   With no `--file`/`--text` the client computes the changed set itself
   from the working tree, which is where the work is at this point in
   BOTH branching modes (the commit is step 7). **Do not hand-assemble
   that list.** Both hand spellings anyone reaches for are wrong and
   neither fails loudly — `<baseBranch>...HEAD` is EMPTY at guard time
   (nothing is committed yet), which Blocks every ticket in every
   category, and `git diff --name-only` cannot see a file the worker
   CREATED, which fails OPEN. `Docs/TickProtocolHistory.md` has the
   detail.

   The logic lives in `Get-WorkingTreeChanges`, with assertions
   covering created files, renames (both sides — moving a file OUT of a
   protected directory still touches it), quoted paths, and gitignored
   scratch. A guard whose input is assembled by hand is a guard with an
   untested step in front of it.

   Still write the list to `.zagent/run/<KEY>/changed.txt` for step 7's
   docs-mirror check and for the work log. A **clean tree here means the
   worker changed nothing** — a failed attempt, which `guard` now reports
   in those words rather than as a guard failure.

   **★★ `guard` IS ALSO WHERE WORKER-MOVED-NOTHING FIRES** — the
   compensating control I1 promises on a `declared` harness. It compares
   `predispatch.json` (step 5's baseline) against the tree now and returns
   `{ok, state, violations}`, `state` one of `clean` / `violated` /
   `unverified` (a missing baseline is never read as clean). A `violated`
   result — commit (`headMoved`), branch/switch (`branchMoved`), stage
   (`indexStaged`), a board call (`boardMoved`), or scratch litter
   (`scratchMutated`) — turns a would-be PASS into a failure, the same
   one-directional override `gateVerdictFresh` uses. `unverified` (no
   baseline) is **tolerated at `guard`**; what it means is decided at
   `finish` instead (step 8), by harness.

   **NARROWER than I1, deliberately.** It cannot see a worker that ran a
   BUILD — build output is gitignored, invisible to `git status` — so on a
   `declared` harness a green result is evidence the worker did not commit,
   branch, stage or touch the board, never proof it had no shell at all.
   That is a real exposure, not a theoretical one: this repo has already
   measured a worker build running concurrently with anything else and
   producing **zero-byte saved assets**.

5. **Answer the coverage question, in writing.** For every source file
   the diff touches: *does anything that just ran actually EXECUTE the
   line I changed?* Name the test per file in the work log's
   **Coverage** row, or say plainly that nothing covers it.

   Green gates answer a narrower question than they look like they
   answer. ZEN-2's Definition of Done said the telemetry recorder was
   "provably unchanged"; the review found the recorder had ZERO
   coverage while the test file asserted it was covered. Every gate
   passed. A suite that never reaches a line cannot notice it changed,
   and a pinned unit COUNT going green says only that the same number
   of tests ran.

   `review.required` catches the subset where the DoD makes the claim
   out loud (step 6.3). Nothing catches the rest, which is why this is
   a written answer rather than a thought: an empty **Coverage** row on
   the board is visible, and a skipped consideration is not.

6. `zagent owns <KEY>` again, immediately before writing anything.

**Nothing you edit after a gate run is covered by that run**, and `-Phase
verify` exists to be that last run: it re-derives the gate list against the
CURRENT diff and re-runs everything, so the pin, the `Status.md` narration and
the reviewer's doc findings are all certified by it. Edit anything after it and
you run it again.

`guard <KEY>` catches the expensive half of that — an edit that pulled in an
area whose gates never ran — and says nothing when the edit stays inside the
same area, because the gate LIST is unchanged. That residue is judgement, and
it is the only reason this paragraph survives.

**A unit-gate line pins a baseline, and the gate asserts `ran ==
Baseline` EXACTLY.** So a ticket that ADDS or REMOVES units reds the gate
with zero failing tests. That is the gate working.

**Fix it in ONE file: `Tools/unit_baselines.json`.** That is the only place
any pin is written. The gate lines and the workflows name the GAME
(`-Game Zenithmon`), and `run_unit_gate.ps1` resolves the number from the
manifest — so a count change is a one-line edit to an UNPROTECTED file that
`zagent guard` permits.

It did not use to be. The number was duplicated into
`.github/workflows/zm-tests.yml` and `zagent.project.json`, both
`protectedPaths`, so bumping a pin was work the loop could never do and
every test-adding ticket was unreachable — in a repo whose conventions ask
for a test with all new code. If you find yourself reading that a pin
cannot be bumped, the doc is stale.

So when `-Phase measure` exits **10**:

1. It has already read the exact count off the gate and printed it. Only a real
   suite run produces that number.
2. **YOU write it into `Tools/unit_baselines.json`**, in the SAME commit as the
   tests that moved it. It does not breach I1: the worker cannot run a gate, so
   a worker that "bumps" a pin is guessing. Recording a measurement is not
   authoring.

   **★★ AND THAT IS A RULE, NOT AN EXCEPTION FOR ONE FILE.** This step used to
   call the pin *"the one edit the orchestrator makes itself"*, which is the
   principle stated as a special case. Generalised:
   **any value that can only be produced by RUNNING something is yours to
   record, and the worker must be told to leave the file's declared sentinel in
   place.** At least three exist:

   | The measurement | Produced by |
   | --- | --- |
   | the unit pin | a `Null_` gate run |
   | a measured ground row (`ZM_Dawnmere*` seam / lab / home tables) | a windowed raycast oracle |
   | a committed-asset SHA256 in `Status.md` | a windowed re-author |

   **★★ AND THE CLAUSE IS "DO NOT QUOTE A NUMBER", NOT "DO NOT EDIT THE FILE".**
   ZM-67's worker was told to leave `Tools/unit_baselines.json` untouched. It
   complied exactly — and then wrote **`3395 → 3397, INFERRED`** into its proposed
   work-log text. Both halves were wrong: the manifest read 3399, and the observed
   run came back 3401. It had grepped `Status.md`, which narrates every pin move it
   has ever made, so the file offers 3394, 3395, 3399 **and 3397 — a *passed*
   count that looks exactly like a pin.**

   Harmless there, because this step makes the pin yours and demands an OBSERVED
   run. But step 8 builds the work log FROM the worker's report, so a tick that
   pastes the proposal publishes a false pin narration into the human-facing
   authority, where the next worker reads it. Tell the worker:

   > The pin is the orchestrator's. Do not edit it and **do not quote a number for
   > it, not even an inferred one** — say `+N units` and let the measurement supply
   > the total.

   `+2 units` was the genuinely useful half of what it told me, and it was right.

   **It is greppable, so do not rely on noticing it.** This repo marks every one
   the same way — an out-of-band sentinel constant plus a comment reading
   `TO FREEZE: run <TEST>`. Grep the ticket's file list for `TO FREEZE` before
   you dispatch; a hit means the ticket contains a measurement you own, and the
   worker prompt must say so. On ZM-65 that was found only by reading a comment
   block on the way past.
3. **A backend-neutral ENGINE unit moves every game's row**, not just the one
   you ran. Bump them all, and name in the work log which you MEASURED and which
   you inferred.
4. `Games/*/Docs/Status.md` narrates pins for humans and is read by no gate.
   Update it when the ticket touches that game.

   **★ IT HOLDS DATA AS WELL AS NARRATION — TWO SITES, NEITHER GATED.**
   `Status.md` carries a committed-asset SHA256 table separate from its prose.
   On ZM-20 the prose was updated and the table row left reading the pre-change
   hash, so the file contradicted itself four hundred lines apart — and the
   table's own text names the cost: *"the next agent to run the two-boot proof
   would diff against these rows, see a mismatch, and start hunting a
   determinism bug that does not exist"*. `doc_lint.ps1` has zero matches for
   `SHA256|hash|zscen`. Whenever you touch a doc, **grep it for the value you
   just changed** rather than patching the paragraph you happened to be reading.

Then run `-Phase verify`. The pin is only correct if the gate that reads it goes
green, and a second red there means the count moved again — a flaky or
order-dependent registration, which is a different problem from the one you just
fixed.

**★ YES, VERIFY RE-RUNS THE SLOW SUITE EVEN WHEN YOUR ONLY POST-MEASURE EDIT WAS
A DOC. That cost is deliberate; do not "optimise" it away.** It was raised in the
2026-08-24 audit and deliberately not changed. The tempting version — skip `build`
and `test` when nothing a compiler reads changed since `measure` — is a
conditional gate skip inside a script whose entire header is four rules that each
exist because a gate was skipped once. It would also have applied to almost
nothing in practice: ZM-65's post-measure edits were comment-only `.cpp`/`.h`
changes, which still need a build to prove they are comment-only. **The Blocked
path pays the same price as the green path, and paying it is cheaper than getting
the skip condition subtly wrong.** If you re-open this, the burden is a test that
fails when the condition is wrong, not an argument that it usually is not.

**Never "fix" this by reverting the worker's tests.** A green gate bought
by deleting coverage is the worst outcome available here, and on the board
it is indistinguishable from success.

## 7. Integrate

**★★ FIRST: GREEN IS NOT THE SAME QUESTION AS DONE.** A worker can return a
CORRECT PARTIAL with a verified impossibility: the gates go green on what
landed, and half the Definition of Done is unmet. That is the same defect
step 6.5 names one level down — green gates answer a narrower question than
they look like they answer — and it means status is a three-way decision, not
Green-or-Red:

- **Green AND every DoD box ticked** → commit, `finish Done`. Below.
- **Green but a DoD box unmet** → commit anyway (the work is correct as far as
  it goes, and `direct` mode cannot leave it dirty), then `finish Blocked` with
  a work log naming which items are unmet and why, plus a Questions row for
  whatever must be decided.
- **Red** → the fix-forward path further down.

`zagent finish <KEY> --status Done` refuses when the work log's
`## Definition of Done` section leaves any box unticked, naming every one —
**Done-only**, so `Blocked` and `"In Review"` still accept an unmet box, which
is why the choice above still matters. **Decide it here anyway**: the check
cannot tell an honestly-unticked box from one ticked to dodge it, and arriving
at a refusal you did not expect is how a tick starts editing checkboxes to get
past a gate instead of picking the honest status.

**★★ AND WHEN YOU BLOCK, SAY WHICH KIND OF BLOCK IT IS.**
`zagent update <KEY> --label blocked-scope` whenever the ticket was **not
buildable as filed** — a scope gap, a missing file list, a ruling nobody has
made — as opposed to a run that failed.

The two land in the same lane and want opposite fixes. A FAILURE block wants
the ticket re-run; a SCOPE block wants its ruling answered, and re-running it
changes nothing. The board now reads the label: a scope block does not count
toward `maxConsecutiveBlocked`, and is still reported, so three
under-specified tickets no longer trip a breaker whose message says the loop
is thrashing.

**Only label what you can defend.** The marker is explicit precisely so it
cannot be inferred — "it filed a Questions row" and "it committed nothing"
are both true of a tick that simply failed, and both would let a thrashing
loop past the breaker by accident. If the gates went red, it is a failure
block whatever else is also true of it.

**★★ AND NOW IT IS CHECKED, NOT JUST ASKED.** `zagent update <KEY> --label
blocked-scope` reads the same `tick_gates.json` verdict a human would, before
the label ever lands, and refuses when it was genuinely RED (exit 2/3/4 — a
gate actually ran and actually failed). A green verdict, an absent one, a
still-`running` one, or exit 10 (a pin needing a bump) all still permit the
label — nothing on record contradicts it. An unparsable `tick_gates.json`
refuses too, the other way: "cannot tell" must not read as permission any more
than as refusal.

ZM-69 is the worked example: claimed, surveyed, found to be missing
`ZM_SceneRegistry.h` — without which the authored door stalls in
`WAITING_FOR_SCENE`, a black screen with every unit green — released in
minutes with two rulings filed and a corrected file list, zero fix-forward
attempts consumed.

**And when the disputed value IS the thing being landed, do not land it.**
ZM-23's forager row was correct code whose *flag column* was exactly what a
ruling governed; committing it would have put a farmable trainer, a comment
asserting that as design, and a test pinning it onto `master`, so the next slice
would revert three things before writing the right one. A revert to a CLEAN tree
satisfies step 3's precondition just as well as a commit does — better, since it
lands nothing false. Preserve the diff in the run scratch and say so in the work
log.

**Green** → stage everything the worker produced, then commit:

```
git -C <repo> add -A
git -C <repo> commit -m "<KEY>: <title>"
```

**`commit -am` is wrong: `-a` stages modifications to files git already
tracks, and a file the worker CREATED is untracked and simply left behind.**
`add -A` is what the guard at step 6.4 already listed, so the two stay
consistent — use it the first time and the rest of this note never applies.

**★★ IF IT HAPPENS ANYWAY, `finish --status Done` NOW CATCHES IT** rather than
letting it surface on somebody else's next clone. The check diffs
`.zagent/run/<KEY>/changed.txt` — the same list `guard` (step 6.4) recorded
from the pre-commit working tree — against `git diff-tree --no-commit-id
--name-only -r HEAD`, and refuses naming any file recorded as changed but
absent from `HEAD`, with the fix (`git add -A`, then a new or amended commit).
Like the DoD check, this fires only on a Done finish — Blocked and In Review
are unaffected — and a missing or empty `changed.txt` (an older run, a
hand-run tick) WARNS rather than refuses: nothing was recorded, so
completeness is UNVERIFIED, not confirmed.

Then:

- `branch` mode: `git -C <repo> switch <baseBranch>` and
  `git -C <repo> merge --ff-only <branch>`. Base moved and ff impossible
  → Blocked. No rebase, no merge commit.
- `direct` mode: already there.
- `push` is false — nothing reaches a remote.

**Red** → **first, re-run the FAILING GATE LINE ONLY, once.** This suite carries
an intermittent hard abort: a verify run died after 9 of 68 tests at
`0x80000003` (`EXCEPTION_BREAKPOINT`), and the identical command on the
identical tree then returned 68 passed / 0 failed. The pre-change tree also
produced one red, with a *different* exit code and a *different* abort point.
A red is therefore not reliably a fact about the tree.

- **red → green** is a **FLAKE**. Do not treat it as a pass and do not treat it
  as a failure: record a Shortfalls row naming the test, the exit code and both
  outcomes, say so in the work log, and continue. It costs no fix-forward
  attempt.
- **red → red** is a real failure. Now fix forward, at most
  `guardrails.fixForwardAttempts` times on the _same_ failure.

Without this, flakes consume the fix-forward budget, `maxConsecutiveBlocked`
trips the breaker on tickets that were never broken, and an unattended loop
resolves the ambiguity against the ticket every time.

**★★ BOTH BRANCHES ASSUME THE SUITE WAS GREEN WHEN YOU ARRIVED. BASELINE IT
BEFORE YOU DISPATCH.** Nothing above tells you to, and on a ticket whose
deliverable is a test that is the difference between "zero regressions" and a
guess. Measured on ZEN-79: `npm test` in `Tools/Vault` was **already red on a
clean tree** — `1 failed | 714 passed (715)`, in the retired-name guard under
`Tools/Vault/test/` —
before a line was changed. Taking that number first is the only reason the
after-state could be stated as *"+5 passing, zero regressions, same single
pre-existing failure"* rather than argued about.

Run whatever suite the ticket's deliverable actually lives in, BEFORE the
worker touches anything, and write the number in the work log. This applies
hardest where no gate runs that suite — see the `Tooling` category in
`zagent.project.json` for which ones now do.

**And the assert text will not be there.** `Zenith_DebugBreak()` kills the
process with the log buffer unflushed, so a hard abort leaves the gate output
and `Logs/zenith_*.log` both stopping mid-sentence. `--assertions-log` does NOT
help — it is a harness feature that records test-level failures from result
JSONs, and on a hard abort there is no JSON to derive a row from. Reproduce
with `zenith test <Game> --headless --tests A,B,C` for an ordered minimal case,
`--per-process` to isolate, `--batch-order reverse|rotate:N` to test
order-dependence.

**Then park: Blocked plus a Questions row.**

> **★★ IN `direct` MODE, COMMIT BEFORE YOU PARK.** There is no branch — the
> work sits **uncommitted**, and step 3's precondition treats a dirty tree as
> fatal. So parking Blocked without committing means the next tick claims a
> ticket, finds this one's work, and Blocks it too. And the one after that. At
> `maxConsecutiveBlocked: 3`, **one red ticket Blocks three innocent tickets and
> trips the breaker**, each carrying a work log blaming a dirty tree it never
> touched.
>
> This is the same argument `human-gate` already makes below, and it is
> *stronger* here: a gated ticket is green, whereas a red one may sit dirty for
> days. So commit onto `baseBranch` with a `WIP(<KEY>): <title> — GATES RED`
> subject, then move to Blocked. Nothing is pushed; a rejection is one
> `git revert`. Leaving the tree dirty is worse than anything it prevents.

**Docs mirror.** Only after a Green commit lands — never on a Blocked or
abandoned ticket, whose edits may be half-done or uncommitted.

With the Vault backend, the repository Markdown is already authoritative
and there is no remote mirror. `guard`'s `docsMirrorNeeded` is always
`false` under this backend (`mirrorPaths` in `Tools/Vault/cli/local.mjs`
returns `[]` whenever `writes.enabled` is true). Skip this section and
proceed to step 8; do not call `docs sync` — it is `removed-transport`
(`Tools/Vault/spec/command-matrix.md`) and refuses unconditionally.
`Docs/TickProtocolHistory.md` keeps the retired per-page WebSocket
mirror mechanics this section used to describe, for history only.

## 8. Write back

Build `.zagent/run/<KEY>/worklog.md` and `.zagent/run/<KEY>/result.md` from
data you already have — `git rev-parse HEAD`, `git diff --stat`,
`gates.log`, the routing line, the DoD checkboxes. Both open with the
routing line:

```
ran on <harness>:<model>/<effort> via <COMPLEXITY>+<RISK> · category <name>
```

so a mis-sized, mis-categorized, or mis-harnessed ticket is visible on the
board after the fact. `<harness>` is your entry point's own id, the same
one passed to `claim --harness` at step 1.

**`finish` checks `<harness>` against `predispatch.json.harness`, three-
valued, not throw-or-pass.** A mismatch refuses — a routing line claiming
`codex` on a ticket `predispatch.json` recorded as claimed under `claude`
is exactly what nothing else here would catch, because the work log is
free text nobody else re-derives. A missing `harness` field (or
no `predispatch.json` at all) reports **`unverified`**, the same shape
`workerMovedNothing` uses and for the same reason: on an `enforced`
harness an absent baseline is tolerated, so `unverified` must never itself
become a refusal. The work log's shape:

```markdown
## Work Log — <date>

**Outcome**: merged to `<base>` as `<sha>` · ran on <harness>:<model>/<effort> via <C>+<R>

**What changed**

- `path` — one line each.

**Files** (`git diff --stat`)
...

**Gates**
| Command | Result |
|---|---|
<!-- mark any line unioned in by `zagent gates` with the category it came from -->

**Coverage**
| Changed file | What executes the changed line |
|---|---|
<!-- "nothing" is a legal answer and the one worth writing down -->

**Definition of Done**

- [x] …

**Durable findings**

- none, or: `<PROJ-D-N>`[, `<PROJ-D-N>` …] — the decision reference(s)
  `zagent decide` recorded for anything learned here that outlives this ticket.

**Deviations / follow-ups**

- None, or: what was assumed, what was left undone and why.
- Every correction you made to the ticket's own text before inlining it.
```

**★★ A FINDING THAT OUTLIVES THE TICKET IS A DECISION, NOT A WORK-LOG LINE, AND
THE ANSWER IS NOW REQUIRED, NOT JUST INVITED.** I3 tells every tick to grep
`decisionLog` for the rulings that govern its subject — and no step in this
file used to tell you to WRITE one. So a worker's durable discovery landed in
**Deviations / follow-ups**, on a ticket that is about to close, which the
next worker never reads. The grep and the thing it greps for were never
connected.

**`zagent finish` now refuses any terminal status — Done, Blocked, In
Review — whose work log carries no `Durable findings` answer.** Nobody can
verify the JUDGEMENT — "was there really nothing durable here" is not
computable — so, exactly like the **Coverage** row below, the mechanism is
to make SILENCE illegal rather than to grade the answer. The required
shape, read from the work log text itself:

- a line reading `**Durable findings**: none` (or naming decision
  reference(s), e.g. `**Durable findings**: ZM-D-208, ZM-D-209`), OR
- a `## Durable findings` heading (any level) with the answer inline on
  that same line or on the next non-blank line(s) beneath it.

The label may be a heading or a bold run-in label with an optional
trailing colon (`Durable findings:`), matched case-insensitively. The
answer must be exactly `none` or one or more references shaped like
`PROJ-D-N` (e.g. `ZM-D-208`) — anything else refuses by name:

- no line at all: *"the work log carries no "Durable findings" line — add
  `**Durable findings**: none` or name the decision reference(s) recorded
  via `zagent decide` (e.g. ZM-D-208)"*
- a label with nothing after it: *"the "Durable findings" line has no
  content — say "none" or name the decision reference(s)"*
- content that is neither "none" nor a decision reference: *"the "Durable
  findings" line ("`<text>`") names neither "none" nor a decision reference
  like ZM-D-208 — say one or the other"*

ZM-28's worker found that **appending a move is not inert**: `ZM_Learnsets.cpp`
rebuilds every species' level-up list by scanning the WHOLE move table and
sorting it, and `ZM_SpeciesData.cpp` scans it again for egg moves. No
blast-radius grep finds that — the coupling is by table scan, not by identifier
— and **seven more gym reward moves remain**, so it recurs seven times.

So: when the tick learns something TRUE OF THE CODEBASE rather than of this
ticket, `zagent decide` it. The test is whether the next worker on a different
ticket would need it. A ticket-specific deviation stays in the work log; a fact
about how the repo behaves goes where I3 will find it.

Record the **why** as carefully as the decision — see the three kinds of
sentence at step 5. A future tick will read this entry the way you read
ZM-D-196, and only its reason will tell them whether it still binds.

**Coverage** is required and "nothing" is a legal answer. It is there
because green gates answer a narrower question than they appear to:
ZEN-2's recorder had zero coverage while its test file said otherwise,
and every gate passed. An empty row on the board is visible; a skipped
consideration is not.

A work log is required on **every** terminal path, not just Done — a
Blocked ticket's log saying how far it got is exactly the thing a human
has to pick up.

Then one call closes the ticket:

```
zagent finish <KEY> --status Done|Blocked|"In Review" \
  --comment .zagent/run/<KEY>/result.md \
  --worklog .zagent/run/<KEY>/worklog.md
```

The comment, the description append and the status move land in a single
transaction, so a ticket can never show Done with no evidence attached.
Do not overwrite the description yourself — `finish` appends below the
spec inside sentinels and replaces its own previous block on a re-run.

## 9. Close out

Run the project's `cleanup` commands (the stray-process sweep — orphaned
game exes lock build outputs for the next iteration).

`tick_gates.ps1` already sweeps on every exit path, including a crash, so
this is the belt to its braces — it catches an exe started outside the gate
run (a windowed authoring boot on a `needs-gpu` ticket, say). This step used
to be the ONLY sweep, and it runs after the gates: an exe left alive by a cut
-off gate failed the NEXT build, for a reason naming a file nobody touched.

- **Queue mode** → write disposition `again` (delaySeconds 60) to `.zagent/next.json`.
- **Targeted mode** → write disposition `stop` and report. You were
  asked for one specific ticket, not for a loop; rescheduling would
  re-run the same key forever.
- **Abandon path** (ownership lost at any checkpoint): write the work log
  as far as it got, then
  `zagent abandon <KEY> --comment .zagent/run/<KEY>/worklog.md`, sweep, then
  write disposition `again` to `.zagent/next.json`.

  `abandon` is client-composed and has no `--status`, `--assignee` or
  `--label` flag at all — it posts the text through the ordinary `comment`
  mutation, so the two things this release must never touch are not choices
  it is capable of making, not just ones it is asked not to. Somebody else
  holds the ticket now: the human owns the description, and the assignee and
  status stay exactly where they were. Use `zagent update` yourself,
  deliberately, only if you actually mean to reassign or reopen it — that is
  the one release where "deliberate" matters more than "fast".

`.zagent/next.json`'s `disposition` is read by the tick itself. On a harness
whose entry point declares `loopMode: in-harness`, the tick turns it into the
matching SCHEDULE call from the table in step 1; on `loopMode: manual` there
is no SCHEDULE at all, and a bare invocation simply ends after reporting the
disposition instead of scheduling anything.
