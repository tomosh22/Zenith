# The `/tick` protocol — how its own wording changed

`.claude/commands/tick.md` is loaded in full on **every** tick. It carries the
rules and the evidence for them; this file carries the third thing it had
accumulated — **the history of its own text.**

The distinction is the point, and it is not "keep it short":

| Stays in `tick.md` | Lives here |
| --- | --- |
| the RULE | — |
| the REASON — *"read `tick_gates.json`, because a shell exit code is corruptible; here is the run that reported 0 for a 10"* | — |
| — | the CHANGELOG — *"this paragraph used to say X, which was false"* |

A rule stripped of its reason gets ignored, and `tick.md` says so; that is why
the war stories stay. But a reader executing a tick does not need to know how
the instruction was worded last week, and two of these passages had grown into
corrections of themselves that a fast reader can invert — which is the same
"the older document competes with the current one" failure the protocol names
about tickets, applied to the protocol.

Nothing here is binding. If a rule in `tick.md` and a paragraph here disagree,
`tick.md` wins and this file is stale.

---

## The `definitionOfDone` array (step 5)

**Current rule:** inline the DoD from `body.md`, never from the
`definitionOfDone` array.

**Original reason, now gone:** `parseTicket` kept only the line each `- [ ]`
sat on, truncating every checkbox at its first newline. Under I3 that
mutilated the worker's spec, and the worker — having no shell and no board —
could not tell it had been cut. Fixed in saas `9d03869` (2026-08-25) and
verified against ZM-23, the ticket that exposed it.

What it cost while it lasted, kept as the worked example:

| `body.md` | `definitionOfDone[].text` |
| --- | --- |
| "…rewritten as a PER-ID claim check — **not bumped to a larger count** — and a row that omits its trailing trainer initializer is caught as a **DOUBLE CLAIM** on `ZM_TRAINER_RIVAL_VESPER`, proven by an **anti-vacuity arm**" | "…is rewritten as a PER-ID claim" |
| "…the separations they achieve against every existing row and against the blockout grey are **REPORTED from the shipped tables rather than asserted**" | "…the separations they achieve against" |

The first is the guard ZM-23 existed to install; the second is the one clause
preventing a self-referential assertion. Two other checks read that same text
truncated: `finish --status Done`'s unticked-box refusal, and the reachability
scan — so a protected path on a continuation line was invisible to the check
built to catch it.

**The rule survived the fix on its own merits** (the array holds checkboxes and
never the prose that qualifies them, and ZM-23's body carried two load-bearing
instructions *outside* any checkbox). It is stated in `tick.md` on those merits
now. This entry exists because the rule outlived its original reason **within
the hour**, which is exactly the failure this file is for.

## "Running a windowed test is not authoring" (step 2)

**Deleted, because it was false.** The paragraph asserted it while the same
section said *"the authoring runs at boot, before the test does"* — and only
one could hold.

Measured on ZM-65: `zenithmon.exe --automated-test
ZM_DawnmereRouteSeamGroundTruth_Test --skip-unit-tests` on a `Vulkan_*_True`
build re-authored the committed `Dawnmere.zscen` as an ordinary side effect of
booting, and `git status` went dirty on a tracked asset nobody meant to touch.
On a measure-then-freeze ticket that is a trap rather than a nuisance: the run
producing the measurement necessarily happens while the row still holds its
out-of-band sentinel, so that boot wrote the gate entity a million metres below
the world into a tracked 5,682-byte asset — byte count unchanged, because it is
a float field.

`tick.md` now states the rule positively (**a windowed `_True` boot authors
before it tests; check `git status` after every one**) with the ZM-65 evidence
attached.

## The `docs sync` timeout (step 7)

**Retired 2026-08-26.** Step 7 mandated `timeout 120 zagent docs sync --json`
and recorded *"timed out — the mirror is one commit behind"* as a legal and
expected outcome, on the belief that the command hung and had never once
completed across five consecutive ticks.

Both were wrong, and the second was a lie the protocol told itself: the mirror
was never behind. `docs sync` opened a WebSocket per page for **every** page —
199 on this checkout, 597 across the three projects that map to it — including
every page already identical to its source, at a handshake plus two settles
each and two sessions per replace. A no-op sync ran about ten minutes, so every
caller's bound killed it around page 40 of 199 and discarded the `--json`
report, which is emitted at the end. Hence the zero-byte logs. It had been
writing correctly throughout, which is why the mirror always looked current for
the docs that sort early (`Status`, `DecisionLog`) and never for `Conventions`.

Fixed in saas `05408ef`; five consecutive timeouts became **exit 0 in 9
seconds**. `Bash(timeout *)` stays in `allowed-tools` — the rule it illustrates
(a mandated command whose first token is not prefix-matched stalls the loop on
a permission prompt) is untouched.

## Hand-assembled changed-set spellings (step 6.4)

**Both spellings the step used to prescribe were wrong, and neither failed
loudly:**

- `<baseBranch>...HEAD` is **empty** at guard time — in `direct` mode nothing
  is committed yet, and in `branch` mode step 4 only ran `switch -c`. Empty
  file → `guard` exits 1 → **every ticket in every category** Blocks with its
  work sitting green and finished in the tree.
- `git diff --name-only` reports only **tracked modifications**, so a file the
  worker CREATED never reaches the check. That one fails **open**, and was
  invisible on any ticket that only edits existing files — it survived two
  green ticks before a file-creating ticket exposed it.

Now `Get-WorkingTreeChanges`, with assertions covering created files, renames
(both sides), quoted paths and gitignored scratch.

## The `changed.txt` docs-mirror match (step 7)

**Deleted.** The step used to say to re-derive whether a docs sync was needed
by matching `changed.txt` in the shell. A PowerShell implementation of that is
wrong in a way nothing notices: `(Get-Content f) -match '…'` returns a
**boolean** for a one-line file and a filtered **array** for a longer one, so
counting the result reports a phantom match on every SINGLE-file change. The
consequence — one needless sync — is benign, which is exactly why it would have
gone on being wrong forever. `guard` answers it now (`docsMirrorNeeded`).

## "Release to To Do" without a label (step 2)

**Amended when To Do became the queue.** The step used to say a self-deferred
ticket should simply be released to To Do, which worked while To Do sat
OUTSIDE the queue. Once To Do *was* the queue, a bare release put the ticket
straight back at the head, so the next firing claimed it again, forever — a
tick burning a claim, a dispatch slot and a wakeup on the same card
indefinitely with nothing ever going red. `deferred` is filtered from the claim
query; `--assignee none` is required alongside it, because the claim wrote the
assignee and `move` only changes status.

## The four gate rules that became a script (step 6.2)

They lived here as four paragraphs, each added after the rule it describes was
forgotten once. They are `Tools/tick_gates.ps1` now, and its header carries
them: regen when a created source file is present (and treat a non-zero regen
as fatal), assert every created `.cpp` by name in the build log, run the unit
gate alone first, and own each gate as a child process waited on unbounded.

## "A headless run may CREATE a `.zscen` but never CHANGE one" (step 2)

**Deleted 2026-08-26, because the guard it described was removed** (ZEN-6, `cbebea74`).

The paragraph read: *"that guard is about the BACKEND, not about a person. It is
`Zenith_Editor.cpp:1425`, inside `if constexpr (Zenith_IsNullRenderer())` … A
`Vulkan_` build authors the whole thing and needs no guard."* Every clause was
true when written, and it was the standing justification for labelling a
scene-authoring ticket `needs-gpu`.

The guard existed because `Zenith_TerrainEditor::EnsureTreeEntities` refused to run
on the Null backend, so a headless tools boot authored an INCOMPLETE world and
`SaveActiveScene` had to refuse any save that would overwrite a tracked asset with
that subset — it once dropped RenderTest's two instanced-tree entities, ~323 KB of
a 361,753-byte file, on every headless boot. ZEN-6 fixed the cause rather than the
symptom: entity creation is backend-neutral now and only the GPU allocation is
skipped, which the Null memory manager was already doing.

**The consequence for the protocol is larger than the deleted sentence.** "It
re-authors a committed scene" was the commonest reason for `needs-gpu`, and six of
the ten tickets on Zenithmon's S8 critical path sat behind the label on that
reasoning alone. Step 2 now says to read WHY a ticket carries it, and to unlabel one
that carries it only for scene authoring.

★ The one thing that survived: **the unit-gate boot authors nothing.** It exits
before the editor-automation queue drains, emits no `[ScenePublish]` line and leaves
a clean tree — so a clean tree is not evidence that a re-author reproduced the
bytes. That trap nearly fooled the tick that landed ZEN-6, and it is now stated at
the same place.

## Amendments to the "never edit this file" rule

`tick.md` says the loop may never edit `.claude/**` or `zagent.project.json`,
and that rule is unchanged and mechanically enforced by `zagent guard`. It
binds the LOOP. It does not bind the repo owner, who has directed three rounds
of edits outside a tick — 2026-08-24 (nineteen findings, after three live
ticks), 2026-08-24 again, and 2026-08-26 (this round, after three more).
Recorded because a file that says "never edited" beside a git history showing
edits is worse than either.
