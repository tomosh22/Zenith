---
description: Run one agent ticket — the queue head, or a ticket you name
allowed-tools: Bash(zagent:*), Bash(pwsh -NoProfile*), Bash(git:*), Agent, Read, Write, Edit, ScheduleWakeup
argument-hint: [PROJECT_KEY | TICKET_KEY]
---

Run **exactly ONE** ticket. Never start a second in the same tick.

# Where things live

Everything this tick needs is in THIS repo. The board is a web
application that may be running on another machine entirely, and nothing
here knows or needs to know where its source is.

| Thing | Home |
| --- | --- |
| This skill | `.claude/commands/tick.md` |
| The worker and reviewer | `.claude/agents/zagent-worker.md`, `.claude/agents/zagent-reviewer.md` — subagent definitions whose tool lists omit Bash |
| Gates, pinned baselines, conventions, categories | `zagent.project.json` (committed, sent to the board with every request) |
| The client | `Tools/zagent/` — a PowerShell script, no Node |
| Run scratch | `.zagent/last.json`, `.zagent/run/<KEY>/` (gitignored) |
| The board account, model routing, guardrail floors | the board itself |

**A bare `claude` started in this repo is the whole setup** — no
`--add-dir`, no second checkout. `zagent` needs `ZAGENT_URL` and
`ZAGENT_TOKEN` on the machine; `Tools/zagent/README.md` covers that and
`zagent doctor` says so if either is missing.

# Operating invariants

These are not style preferences. Each one exists because an unattended
loop without it lies to itself.

- **I1 — You own the gate; the worker only authors.** Subagents never
  build, test, run, commit, or touch the board. This is enforced
  structurally — `zagent-worker` grants only `Read, Edit, Write, Grep,
  Glob`, so the worker has no Bash tool to run a gate with even if the
  ticket text tells it to — but state it in the prompt anyway, because a
  rule the worker can read is a rule it can plan around.

  **The tool list in those two agent files IS the enforcement.** Add Bash
  to `zagent-worker.md` and I1 is gone with no other file changing and
  nothing failing loudly. They are protected by `.claude/**` for that
  reason.
- **I2 — Never trust "it works".** A worker's claim that gates pass is
  worthless by construction: it was forbidden from running them. Re-run
  everything yourself after integrating, even when the claim is
  plausible.
- **I3 — Inline the work into the prompt.** Never say "see the ticket" or
  "read Docs/X.md section 3". Paste the ticket body, the Definition of
  Done, the file list and the conventions INTO the worker prompt.
  `bodyPath` is a supplement, never the carrier.
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

1. **Every gate line must be prefix-matched by an `allowed-tools` entry
   above.** Gate lines live in this repo's `zagent.project.json`. Adding
   a gate and not adding its prefix here stalls the unattended loop on a
   permission prompt. `.claude/settings.json` documents the same trap —
   its allowlist spells
   `pwsh -NoProfile -ExecutionPolicy Bypass -File Tools\run_unit_gate.ps1*`
   exactly, and dropping `-ExecutionPolicy Bypass` stalls it.
2. **You may never edit this file or `zagent.project.json`.** Both are in
   `protectedPaths` — `.claude/**` covers this one — and the board's
   floor is UNIONed with this repo's, so a repo cannot unprotect itself
   by editing the file that lists its protections. Workflow friction
   becomes a Suggestions row for a human to triage, never a commit. A
   loop that can rewrite its own gates does not have gates.

# Never

- No `psql`, no direct database access, no importing `@saas/database`.
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

Its scratch is HERE now: `.zagent/last.json` and `.zagent/run/<KEY>/`, at
the repo root, gitignored so `git status --porcelain` stays empty while a
ticket is in flight. The precondition check below depends on that.

Exits 3/4/5/6 are ordinary control flow, not failures. **Exit 7 is new
and is not one of them**: it means the board was unreachable. Treat it as
infrastructure, not as a verdict on the ticket — comment nothing, change
no status, and reschedule. A network blip written into a ticket as a
contract failure is a lie that outlives the blip.

So:

- **Branch on the exit code**, never on stdout prose.
- **Read the JSON from a file**, not from the pipe: every `--json` call
  writes `.zagent/last.json`, and a claim also writes
  `.zagent/run/<KEY>/ticket.json` and `.zagent/run/<KEY>/body.md`. A
  firing can crash between the call and the read, and a file survives
  that where a pipe does not.

---

# Steps

## 0. Pre-flight (first firing after a restart only)

`zagent doctor`. A failing check here explains most "the loop did
nothing for an hour" incidents. Report and stop if the agent account,
the lanes, or a gate executable is missing.

The report is assembled from BOTH sides: the board answers for the
database, the lanes and the agent account; this machine answers for the
repo's branch and cleanliness and whether each gate executable resolves.
A board cannot answer the second set — it would be reporting about its
own disk — so a `zagent doctor` that shows only board rows means the
client half never ran.

**Check that mechanically; do not eyeball it.** A `doctor` whose client
half never ran still exits **0**, so branching on the exit code alone
sails straight past the one check written to catch this:

```
zagent doctor | Select-String -Quiet 'client repo:|repo branch:|repo clean:'
```

False → **you are running the wrong `zagent`** and must stop. The usual
cause is a stale shim earlier on `PATH` — an older client that talks
straight to the database instead of over HTTP, which also violates the
`Never` list above. Resolve `zagent` and confirm it is this repo's
`Tools/zagent/zagent.ps1` before going further. `PATH` is ordered, so
appending this repo's directory is not enough when a stale entry already
sits ahead of it; it has to come first.

The `project <KEY> gates` row is the one that most often explains a
silent tick: a project resolving to no gate lines can never merge.

## 1. Pick the ticket

The argument disambiguates itself: ticket keys always end in `-<digits>`,
and project keys never can. Digits ARE legal inside a project key, so
match on the suffix, not on "letters".

- `$1` matches `/-\d+$/` → **targeted**: `zagent claim $1 --json`
- `$1` is a project key, or absent → **queue**:
  `zagent next --json` (the repo declares its own project)

Then read `.zagent/last.json` and branch on the exit code:

| Exit | Meaning                   | Do                                                                                                    |
| ---- | ------------------------- | ----------------------------------------------------------------------------------------------------- |
| 0    | Claimed, contract valid   | → step 2                                                                                              |
| 3    | Queue empty               | `ScheduleWakeup{noop:true, delaySeconds:900}`, stop                                                   |
| 4    | Claimed, contract invalid | → step 2                                                                                              |
| 5    | Repo busy, or would steal | Report which key holds it. Queue mode: `ScheduleWakeup{noop:true, delaySeconds:300}`. Targeted: stop. |
| 6    | Circuit breaker open      | Report, `ScheduleWakeup{stop:true}`. A loop that keeps failing should stop, not spin.                 |
| 7    | Board unreachable         | Nothing was claimed. Change no status, comment nothing. `ScheduleWakeup{noop:true, delaySeconds:300}`. |

## 2. Contract and label checks

**Contract invalid (exit 4).** Read `contractError` from the payload.

- Queue mode: `zagent comment <KEY> --text "<contractError>"`, then
  `zagent move <KEY> Blocked`. Then step 9.
- Targeted mode: **report the error and change nothing.** A human is
  waiting and just asked for this ticket by name — do not Block it.

**Reachability — read the Definition of Done BEFORE dispatching a worker.**
`contractValid: true` means the ticket can be ROUTED, not that it can be
DONE. Validation checks category, complexity and risk; nothing checks
whether the work is inside what an agent bound by `protectedPaths` can
reach. Three shapes are provably impossible, and each one costs a claim, a
slot in the one-ticket-per-repo lock, and a worker dispatch if you only
notice afterwards:

| DoD says | Why it cannot be done |
| --- | --- |
| adds or removes a **test** | the pin bump needs `.github/**` and `zagent.project.json` — see the baseline note in step 6 |
| touches **CI**, `.claude/**`, or `zagent.project.json` | `guard` refuses it at 6.4 |
| "a **user decision** recorded", or any ruling the ticket does not already contain | nobody in this loop is the user |

Check those three against the DoD text, and spot-check the ticket's central
claim against the repo — **every ticket run so far has had a body that
drifted from the code**: one cited a file with zero matches, one described
a trigger condition that had not occurred, one named a single assertion
where the code has two, one quoted a hardcoded path that had already been
parameterised. A body is a description of the past, not of `master`.

If the work is unreachable, **Block it now** with a work log naming which
of the three it is and what a human must do, and file the human half as its
own ticket. Do not dispatch a worker to discover this expensively, and do
not let it consume fix-forward attempts.

**A DoD that names a pin SITE is stale, not unreachable.** Ten open tickets
still say "bumped in Status.md AND zm-tests.yml AND zagent.project.json",
which was true before the baseline refactor and now points at two protected
paths. The pin has ONE home — `Tools/unit_baselines.json`. **Do not Block on
this and do not inline it verbatim**: correct it in the worker prompt, say
you did in the work log, and let the Suggestions row fix the template.
Blocking a ticket over a stale sentence, when the work it describes is a
one-line edit to an unprotected file, is the contract failing the ticket
rather than the reverse.

Also sanity-check SEQUENCE, and read the body for the ticket's OWN timing —
several here defer themselves in prose that no field captures:

- a ticket from a later sprint than the one in progress may be unreachable
  because what it measures does not exist yet (ZM-45's budgets, ZM-43's
  chapters);
- a ticket may say outright it should not be built yet. ZM-48 reads
  *"Zenithmon does NOT need this now … lands when populated-world NPC
  pathing actually needs it"*; ZM-47 is *"DEFERRED post-Zenithmon"*. Claim,
  comment, release to To Do — do not dispatch. **An autonomous loop widening
  scope on a self-deferred ticket is worse than one that does nothing**, and
  the deferral is invisible to `contractValid`.

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

What stays out of reach on any machine is **authoring**: a headless run may
CREATE a `.zscen` but never CHANGE one, which is what the `windowed` label
means and why it is filtered from the queue. Running a windowed TEST is not
authoring.

**`windowed` label** (`windowed: true` in the payload, and exit 4). The
ticket needs a person at a keyboard — a headless run cannot produce its
deliverable, most often because it re-authors a committed `.zscen`,
which a headless run may create but never change. Queue mode never sees
one: the claim query filters it out. If a TARGETED claim returns it,
**report and change nothing** — the same as any exit 4 in targeted mode.
Do not dispatch a worker; green gates on this ticket would mean the
deliverable is missing, not that the work is done.

**`human-gate` label** (`humanGate: true` in the payload). Run the whole
tick normally — preconditions, worker, gates, evidence — but at step 7
move to **In Review** instead of Done and never merge. The reporter is
notified automatically by `finish`. Then continue: queue mode schedules
the next wakeup as usual, because the gated ticket is out of the queue by
construction and one gated ticket must not starve the board. Targeted
mode stops after it.

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

You do **not** implement the ticket. Assemble
`.zagent/run/<KEY>/prompt.md` and spawn a worker at the routed model.

The prompt must open with these clauses, filled in from the payload:

> **DO NOT** attempt to run gates, build, test, commit, push, or touch
> the board — you have no shell; the orchestrator does all of that.
> **Files you may edit:** \<exhaustive list\>.
> **Conventions:** \<`conventions` from the payload, verbatim\>.
> **Report back:** files changed, a one-paragraph summary, proposed
> work-log text, and any decisions, questions, shortfalls or workflow
> suggestions you want recorded.

Then inline (I3): the `## Goal`, the `## Definition of Done` items, the
file list, and any relevant repo conventions. `bodyPath` is a supplement.

Dispatch shape — **the `Agent` tool, never a shelled-out CLI**:

```
Agent{
  subagent_type: 'zagent-worker',
  model:         <routing.model>,      # 'haiku' | 'sonnet' | 'opus'
  description:   '<KEY> — <short title>',
  prompt:        <the full text of .zagent/run/<KEY>/prompt.md>,
  run_in_background: true,
}
```

- **`zagent-worker` has no Bash tool**, so I1 holds no matter what the
  ticket text tells the worker. That is the whole reason the dispatch
  names an agent type rather than passing a tool list here: a tool list
  written at the call site can be edited by whoever writes the call.
- **Pass the prompt text, not a path.** `prompt.md` on disk is the
  durable copy that makes a crashed firing reconstructible (I6); the
  Agent call still carries the whole thing inline (I3).
- **Write the returned report to `.zagent/run/<KEY>/report.json`**
  yourself when it lands. Nothing else does, and step 8 reads it.
- **`run_in_background: true`**, then wait for the completion
  notification. Subagents outlive any single foreground call, and a
  COMPLEX worker routinely runs for many minutes.
- If elapsed wall-clock exceeds `guardrails.ticketTimeoutMinutes`, stop
  the agent (`TaskStop`) and treat it as a failed attempt (→ Blocked,
  with a timeout note).
- **Effort is not settable per dispatch.** The `Agent` tool takes a model
  but no effort parameter; reasoning effort comes from the agent
  definition. `routing.effort` is therefore advisory — record it in the
  routing line (step 8) so a mis-sized ticket is still visible, but do
  not pretend it was applied.
- `workerTools` in the payload is a **cross-check, not a control**. If it
  names a tool `zagent-worker` does not grant, that is a contract
  mismatch worth a Suggestions row — not a reason to widen the agent.

**A dead subagent is infrastructure, not a verdict.** `Agent` returns
null if the worker dies on a terminal API error or is skipped mid-run.
That is the worker-side twin of `zagent` exit 7, and it gets the same
answer: **do not treat it as a red.** A red means the gates ran and
disagreed with the work. A null means no work happened at all, and
fix-forwarding it just re-runs the same failure `fixForwardAttempts`
times before parking an innocent ticket as Blocked — then does it to the
next ticket, and the next, until `maxConsecutiveBlocked` trips the
breaker on three tickets that were never even attempted. So on a null:
comment what happened, **move the ticket back to its pre-claim status
rather than Blocked**, and stop the tick. Report it as infrastructure.

Whatever the worker claims about correctness is discarded (I2). Its
report is _proposed text_ that you apply (I4).

## 6. Verify

1. `zagent owns <KEY>` — exit 5 → abandon path (step 9).
2. **`zagent gates <KEY>`**, then run every command it prints, **in
   order, spelled exactly**, appending combined output to
   `.zagent/run/<KEY>/gates.log`. First non-zero stops.

   **The list comes from that command, not from the claim payload.** The
   payload's gates were chosen by the ticket's filing CATEGORY, which
   records who asked for the work rather than what it touched. `gates`
   takes the same working-tree changed set `guard` computes and UNIONS in
   every category whose declared `paths` the diff actually reached — so a
   `Zenithmon` ticket that edits `Zenith/**` builds and tests Combat and
   checks the engine pin too. The payload's list is always a PREFIX of
   what comes back, so this widens verification and never narrows the
   contract.

   Four consecutive tickets edited engine code under a one-game gate
   list. ZM-50's own Goal said *"the fix is engine-side"* while its
   category said otherwise, and only the category had any mechanical
   effect. Running the extra gates by hand — which is what that run
   did — is a convention, and a convention is exactly the thing that
   holds until the one time nobody does it.

   Never paraphrase a gate line and never re-derive one from
   `zagent.project.json` by hand.
3. **Reviewer pass** when the ticket's `risk` is in `reviewOn`: dispatch
   `Agent{subagent_type: 'zagent-reviewer', model: <routing.model>}` with
   the diff inlined in the prompt. `zagent-reviewer` is read-only by
   definition (`Read, Grep, Glob`), so it cannot quietly "fix" what it
   finds and hand you back a diff you did not gate. Findings go in the
   work log; a finding that contradicts a gate result blocks. A null
   return here is infrastructure, same as step 5 — it is not a pass.
4. `zagent guard <KEY>` — **with the key, and with no `--file`**.
   Non-zero → Blocked regardless of gate colour.

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
   that list.** Both spellings this step used to prescribe were wrong and
   neither failed loudly:

   - `<baseBranch>...HEAD` is **empty** at guard time — in `direct` mode
     you have not committed, and in `branch` mode step 4 only ran
     `switch -c`, so the branch sits at `baseBranch` with zero commits.
     Empty file → `guard` exits 1 → **every ticket in every category**
     Blocks with its work sitting green and finished in the tree.
   - `git diff --name-only` reports only **tracked modifications**, so a
     file the worker CREATED never reaches the check. A new
     `.claude/agents/anything.md` sails straight past it. That one fails
     **open**, and is invisible on any ticket that only edits existing
     files — which is why it survived two green ticks before a
     file-creating ticket exposed it.

   The logic now lives in `Get-WorkingTreeChanges`, with assertions
   covering created files, renames (both sides — moving a file OUT of a
   protected directory still touches it), quoted paths, and gitignored
   scratch. A guard whose input is assembled by hand is a guard with an
   untested step in front of it.

   Still write the list to `.zagent/run/<KEY>/changed.txt` for step 7's
   docs-mirror check and for the work log. A **clean tree here means the
   worker changed nothing** — a failed attempt, which `guard` now reports
   in those words rather than as a guard failure.

5. `zagent owns <KEY>` again, immediately before writing anything.

An empty gate list can never merge. If `gates` is `[]`, Block it.

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

So when the gate reds with **zero failures**:

1. Read the exact count off the gate (`… N ran, … 0 failed`). That number
   is the deliverable, and only a real suite run produces it.
2. **YOU bump the row, not the worker** — `Tools/unit_baselines.json`, in
   the SAME commit as the tests that moved it. This is the one edit the
   orchestrator makes itself, and it does not breach I1: the worker cannot
   run a gate, so it cannot know the number, and a worker that "bumps" a
   pin is guessing. Recording a measurement you just took is not
   authoring. Do NOT send the worker back for it — that costs a dispatch
   to produce a number it would have to invent.

   Re-run the gate after bumping. A pin is only correct if the gate that
   reads it goes green, and a second red here means the count moved again
   (a flaky or order-dependent registration), which is a different problem
   from the one you just fixed.
3. **A backend-neutral ENGINE unit moves every game's row**, not just the
   one you ran. Bump them all, and say so in the work log — you will only
   have observed one of them, so name which you measured and which you
   inferred.
4. `Games/*/Docs/Status.md` still narrates pins for humans and is NOT
   read by any gate. Update it when the ticket touches that game, but a
   stale line there reds nothing.

**Never "fix" this by reverting the worker's tests.** A green gate bought
by deleting coverage is the worst outcome available here, and on the board
it is indistinguishable from success.

## 7. Integrate

**Green** → stage everything the worker produced, then commit:

```
git -C <repo> add -A
git -C <repo> commit -m "<KEY>: <title>"
```

**`commit -am` is wrong and its failure is silent.** `-a` stages
modifications to files git already tracks; a file the worker CREATED is
untracked and is simply left behind. A ticket whose new header is
`#include`d by the source files it also edited would land a commit on
`master` that **does not compile from a clean checkout** — and every gate
would have certified it green, because the gates ran against a working
tree where the file existed. Nothing downstream catches it: the tree is
clean afterwards, `finish` records Done, and the break surfaces on
somebody else's next clone. `add -A` is what the guard at step 6.4 already
listed, so the two stay consistent.

Then:

- `branch` mode: `git -C <repo> switch <baseBranch>` and
  `git -C <repo> merge --ff-only <branch>`. Base moved and ff impossible
  → Blocked. No rebase, no merge commit.
- `direct` mode: already there.
- `push` is false — nothing reaches a remote.

**Red** → fix forward, at most `guardrails.fixForwardAttempts` times on
the _same_ failure, then park: Blocked plus a Questions row. Three
attempts then park beats a thrashing loop.

**Docs mirror.** Only after a Green commit lands — never on a Blocked or
abandoned ticket, whose edits may be half-done or uncommitted. Check
`.zagent/run/<KEY>/changed.txt` (already written at step 6.4) against
`/(^|\/)Docs\//` or `/(^|\/)CLAUDE\.md$/`. No match → skip, silently; a
ticket that never touched a doc has nothing to mirror. A match →
`zagent docs sync --json`, appended to `.zagent/run/<KEY>/docs-sync.log`.

Never block the tick on this. `docs sync` reaches a Notion project over a
WebSocket the tick has no other reason to depend on, and a mirror that is
one commit behind is a stale Notion page — recoverable at the next sync
that touches the same file. A commit that already landed on `master`
staying uncommitted-to-Notion is not a failure mode this step exists to
prevent. So: log the outcome into the work log's **Deviations /
follow-ups** (created/updated page counts, or the error text on a
non-zero exit), and continue to step 8 regardless.

## 8. Write back

Build `.zagent/run/<KEY>/worklog.md` and `.zagent/run/<KEY>/result.md` from
data you already have — `git rev-parse HEAD`, `git diff --stat`,
`gates.log`, the routing line, the DoD checkboxes. Both open with the
routing line:

```
ran on <model>/<effort> via <COMPLEXITY>+<RISK> · category <name>
```

so a mis-sized or mis-categorized ticket is visible on the board after
the fact. The work log's shape:

```markdown
## Work Log — <date>

**Outcome**: merged to `<base>` as `<sha>` · ran on <model>/<effort> via <C>+<R>

**What changed**

- `path` — one line each.

**Files** (`git diff --stat`)
...

**Gates**
| Command | Result |
|---|---|

**Definition of Done**

- [x] …

**Deviations / follow-ups**

- None, or: what was assumed, what was left undone and why.
```

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

- **Queue mode** → `ScheduleWakeup{noop:false, delaySeconds:60}`.
- **Targeted mode** → `ScheduleWakeup{stop:true}` and report. You were
  asked for one specific ticket, not for a loop; rescheduling would
  re-run the same key forever.
- **Abandon path** (ownership lost at any checkpoint): write the work log
  as far as it got, post it as a **comment only** — the human owns the
  description now — leave the branch, change no status, sweep, then
  `ScheduleWakeup{noop:false}`.

`ScheduleWakeup` only exists when the session is driven by `/loop`; a
bare `/tick` invocation simply ends instead of scheduling.
