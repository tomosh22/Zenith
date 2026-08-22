# Zenithmon -- The Board

**Document purpose:** how the Markdown in this directory and the Jira board relate.
Read it once; after that `Roadmap.md` and the board carry the keys and this file only
answers "why is it arranged this way".

**Board:** project **`ZM`**, org `pink-goat`, on the machine named by `ZAGENT_URL`.
Engine work is **`ZEN`** and DevilsPlayground is **`DP`** -- one board per game area,
all three served by the same `C:\dev\Zenith` checkout.

---

## 1. What lives where

The split is between the WORK and the SPEC, and it is not a matter of taste --
each side owns what only it can own.

| | Authority | Why it cannot move |
|---|---|---|
| **Epics, stories, tasks, bugs, blockers, sprints, releases, estimates** | the **board** | A dependency recorded in a Markdown table cannot stop the loop claiming work out of order. The board's `BLOCKS` links can, and do. |
| **The design** -- `GameDesignDocument.md`, `Scope.md`, `SaveFormat.md`, `TestPlan.md`, `AssetManifest.md` | these **files** | A loop worker is spawned with no network and no shell. It reads `AgentBriefing.md` off disk with the Read tool. Anything it must obey has to be a file. |
| **Decisions** -- `DecisionLog.md` | this **file** | `zagent decide` appends here, and a Zenith session greps here. |
| **The pinned unit baselines** -- `Status.md` | this **file** | An authority you need a browser to read is not one a gate can be reconciled against. |

**The repo stays authoritative for everything it owns**, and `zagent docs sync`
mirrors this directory into Notion one way. Nothing writes back; `zagent docs write`
against a mirrored page is refused by name.

---

## 2. The stage epics

Every `## S<n>` heading in [Roadmap.md](Roadmap.md) is an epic, and carries its key.

| Stage | Epic | Stage | Epic |
|---|---|---|---|
| S0 | `ZM-1` | S7 | `ZM-8` |
| S1 | `ZM-2` | S8 | `ZM-9` |
| S2 | `ZM-3` | S9 | `ZM-10` |
| S3 | `ZM-4` | S10 | `ZM-11` |
| S4 | `ZM-5` | S11 | `ZM-12` |
| S5 | `ZM-6` | S12 | `ZM-13` |
| S6 | `ZM-7` | | |

Plus five epics for work the stage plan never had a home for. Each of these was
tracked in a document section with **no id at all**, which is exactly why none of it
could be scheduled, blocked, or reported on:

| Epic | Covers |
|---|---|
| `ZM-14` Engine gaps E1-E8 | [Shortfalls.md](Shortfalls.md) section 2 |
| `ZM-15` Tech debt & shortfalls | [Shortfalls.md](Shortfalls.md) sections 1 and 3 |
| `ZM-16` Open questions | the `[OPEN]` entries in [Questions.md](Questions.md) |
| `ZM-17` Test infrastructure & gates | the unit gate and its four pinned sites |
| `ZM-18` Visual polish debt | the open ZM-D-168 audit items |

**S0-S7 are on the board as Done epics, not omitted.** They are the audit trail the
board is supposed to be, and an epic tree starting at S8 would make this project look
like it began in August.

---

## 3. Issue types, and what each one means here

| Type | Used for |
|---|---|
| `EPIC` | a stage, or one of the five cross-cutting areas |
| `STORY` | a Roadmap checklist item -- player-visible capability |
| `TASK` | engineering work with no player-visible surface: a slice, a test, a refactor |
| `BUG` | something that behaves wrongly today, including a flaky test |
| `SUBTASK` | a step inside one of the above, when a slice needs splitting |

The hierarchy is **EPIC > STORY/TASK/BUG > SUBTASK** and is enforced -- both the board
and `zagent` refuse a parenting that skips or inverts a level.

---

## 4. Sizing: two fields, two jobs

**Never derive one from the other.**

| Field | Answers | Consumer |
|---|---|---|
| `complexity` + `risk` | how much thinking does this need, and what is its blast radius | the loop's model routing (`agent.config.json`) |
| `storyPoints` | how much of a sprint does this consume | burndown, velocity, capacity |

A re-estimate must not silently reroute work to a different model, and a model change
must not silently rewrite the sprint's capacity. Points are Fibonacci; an unestimated
issue is a real state, distinct from a zero-point one.

`risk` never blocks a merge -- it buys more thinking, and escalates the model one tier.

---

## 5. Bug fields

`severity` is **how badly it behaves**; `priority` is **when we intend to fix it**.
They are conflated constantly and they are not the same axis: a trivial-severity
glitch on the title screen can be CRITICAL priority, and an S1 in a system nobody
reaches this milestone can be LOW.

| | |
|---|---|
| `S1_CRITICAL` | crash, data loss, or a hard block on progression |
| `S2_MAJOR` | a feature is broken, with no reasonable workaround |
| `S3_MINOR` | wrong behaviour with a workaround, or a visual defect |
| `S4_TRIVIAL` | cosmetic; nobody is stopped by it |

`reproducibility` is `ALWAYS` / `INTERMITTENT` / `ONCE` / `CANNOT_REPRODUCE`, and
`environment` is free text because the useful answer is a build-config name -- e.g.
`Null_vs2022_Debug_Win64_True`.

---

## 6. Blockers are mechanical, not decorative

A `BLOCKS` link is the one thing on this board that changes what the loop DOES.

```
zagent link ZM-20 blocks ZM-21      # R1-2 must land before R1-3
zagent blocked --project ZM         # everything waiting, and on what
```

`claimQueueHead` refuses any issue with an unfinished `BLOCKS` predecessor, and a
targeted claim on one comes back exit 4 naming the ticket to finish first. Before
this, the R1 chain was a `deps` **column in a Markdown table** -- so nothing stopped
the loop taking R1-6 before R1-5 existed, running the gates green against a
half-built world, and reporting success.

A blocker clears on reaching a **DONE-category status**, not a lane literally named
Done -- a project may rename its lanes.

Cycles are refused at link time. Every issue in a cycle would be permanently
unclaimable, and the queue would report empty with work sitting in it.

**On the board, every blocked row NAMES what blocks it** -- the card, the backlog
row and the roadmap row all read `Blocked by ZM-20`, and the tooltip points at the
issue's **Linked issues** panel, which is where the link can be removed. A badge
that only said `Blocked` would be a dead end: correct, and useless.

---

## 6a. The workflow is enforced now

A status is no longer a field you can set to anything. The board's
`jira.workflow_transitions` rows used to be written and read by nothing,
so **any move was legal from anywhere**; they are now checked by every
writer -- the board's dropdown, its drag-and-drop, and `zagent move` /
`zagent finish`. An illegal move is a refusal with a reason, not a
silent success.

```
zagent transitions ZM-22     # what this ticket may do from where it is
```

The default graph for the five lanes, which every ZM/ZEN/DP project now
carries:

| From | May reach |
|---|---|
| To Do | In Progress |
| In Progress | To Do, In Review, Done |
| In Review | In Progress, Done |
| Done | To Do, In Progress (reopen) |
| Blocked | To Do, In Progress |
| **anywhere** | Blocked, Done |

**There were six lanes.** `Ready for Agent` sat between To Do and In
Progress, and a card waited there until a person dragged it across --
which put a human in front of every ticket. Migration `0026` retired it
and carried the cards into To Do. **`To Do` IS the queue now**: a ticket
filed there is claimable the moment it lands, and the loop takes it To Do
-> Done with nobody in the middle.

Edit it at **Project settings -> Workflow** on the board. Defining a
workflow is ADMIN-only; using one is not -- the same split categories
and releases already carry, and for the same reason: a workflow the loop
agent could rewrite is a workflow that gates nothing.

**`claim` is deliberately exempt.** It is the queue mechanism, not a
move somebody chose -- a workflow edit that dropped one edge would stall
the queue with work in it and nothing saying why.

`zagent move` needs the claim too, with ONE exception: a move to **To
Do** on a ticket nobody holds. Putting a card back in the queue by
definition happens before ownership exists, and the advertised set and
the accepted set have to agree.

Three kinds of rule can hang off an edge, and they are Jira's:
a **condition** (who may move it) hides the option, a **validator**
(what must be true first -- no open blockers, a required field) shows it
disabled with the reason, and a **post-function** runs afterwards. The
default graph carries NONE of them: the edges are the enforcement, and
a default that quietly refused a Done with open blockers would change
what the loop can do without anybody choosing it.

### Resolution is set by the transition

`resolution` stopped being a field anybody can write. The invariant is
**resolved if and only if the status is DONE-category**, imposed by the
transition itself -- so Jira's classic hole, a ticket sitting in Done
with nothing saying whether it was built or abandoned, cannot happen
here.

Moving a ticket into Done asks which resolution; moving it back out
clears it. `zagent resolve` survives only as a CORRECTION -- changing
which resolution a finished ticket carries. Setting one on an unfinished
ticket, or clearing one on a finished ticket, is refused by name.

### The board is also configurable now

Separate from the workflow, and the split matters: the workflow says
which statuses EXIST and which moves are legal, the board says how they
are DRAWN. **Project settings -> Board settings** carries columns (each
drawing one or more statuses), WIP limits, swimlanes by epic or
assignee, quick filters, and which fields a card shows. `KANBAN` vs
`SCRUM` finally means something too -- a SCRUM board shows only the
active sprint.

A status no column draws keeps its issues OFF the board, as Jira does,
and the board says so in a banner naming them. If a ZM ticket ever seems
to vanish, that banner is the first place to look.

### Who may move a ticket

**Project settings -> Permissions.** `ZM`, `ZEN` and `DP` all run with
NO permission scheme, which means the organisation roles decide exactly
as they always have -- an ADMIN or OWNER can do everything, a MEMBER
(which the loop agent is) can do the work but not administer, and a
VIEWER can only read. Nothing about the loop changed.

Attaching a scheme is opt-in and per project. Two things to know before
doing so: an EMPTY scheme grants nothing to **anybody**, including an
owner, and revoking `TRANSITION_ISSUE` from the agent's role would stop
`zagent move` and `zagent finish` with a permission error rather than a
workflow one.

## 7. The three safety labels

| Label | Meaning |
|---|---|
| `human-gate` | the loop does the work and parks it at In Review. It never signs its own gate (I7). |
| `windowed` | the loop **cannot** do the work. Filtered out of the queue in SQL and refused by a targeted claim. |
| `deferred` | "not yet". Filtered out of the queue in SQL, and a targeted claim takes it **anyway**. |

`windowed` exists because a headless run may **CREATE** a `.zscen` but never **CHANGE**
one. A slice that re-authors a committed scene would either no-op or trip the publish
guard -- and the units that would notice are compiled constants that stay green, so
the failure looks like a clean gate run with the deliverable missing.

`ZM-20`, `ZM-21`, `ZM-24`, `ZM-26`, `ZM-29`, `ZM-30`, `ZM-44`, `ZM-58`, `ZM-59`,
`ZM-63` and `ZM-64` carry it today.

`deferred` exists because To Do became the queue. Several tickets defer THEMSELVES in
prose no field captures -- `ZM-48` reads *"Zenithmon does NOT need this now"*, `ZM-47`
is *"DEFERRED post-Zenithmon"* -- and the tick used to handle one by releasing it from
`Ready for Agent` back to To Do, which took it OUT of the queue. That same release now
puts it back at the HEAD, so an unattended loop would claim and release it forever with
no gate ever going red. `ZM-47` and `ZM-48` carry it.

**It is not `Blocked`.** Nothing is wrong with these tickets, and three of them would
trip `maxConsecutiveBlocked` and stop the whole queue over work that is merely early.

**And it is the one label a targeted claim ignores**, which is the difference from
`windowed`: `windowed` says the machine cannot produce the deliverable, so naming the
ticket changes nothing, while a deferral is a judgement about TIMING and `/tick ZM-48`
is how a person overrules it.

> **This used to be `--assignee` alone**, which protected those tickets only because
> the queue skips assigned tickets -- a default-allow shape where the safe state was
> the one nobody typed. The labels are default-deny, greppable, and enforced.

★ **A ticket BODY that says it needs a person is NOT a marker**, and retiring the
hand-off lane made that difference load-bearing for the first time. `ZM-9` (*"no agent
may sign it"*), `ZM-18` and `ZM-60` (*"a human visual sign-off"*) each said so in their
own words and carried nothing mechanical, and all three sat in To Do -- which the loop
now claims from. They are labelled now. Nothing scans prose, so a NEW ticket in that
shape needs its label applying when you file it.

Descriptive labels carry no behaviour: `question`, `tech-debt`, `flaky`, `visual`,
`engine-gap`, `content`, `docs`.

---

## 8. Releases

A **version** is what a build contains; a **sprint** is when work happened. One
version spans many sprints, which is why the stages are epics and these are not.

| Version | Gate |
|---|---|
| **Vertical Slice** | S8: mini-playthrough new-game -> Badge 1 green, plus manual visual sign-off |
| **World Complete** | S9 + S10: every warp edge walked by a traversal test |
| **Content Complete** | S11: 100-streak simulation invariants, 7-battle tower run |
| **Ship Candidate** | S12: full suite green, playthrough bot completes, budgets met |

---

## 8a. Sprints

**Every stage is also a sprint, one for one with its epic.** A sprint holds exactly
that epic's children -- there is no judgement in the membership, so the two can never
describe different scopes -- and its goal is the stage's own `*Gate:*` line from
[Roadmap.md](Roadmap.md), so "what would close this sprint" and "what would tick this
stage" cannot drift into two answers.

| Sprint | Stage epic |
|---|---|
| `S8 — Vertical slice, go/no-go` | `ZM-9` |
| `S9 — World buildout A` | `ZM-10` |
| `S10 — World buildout B` | `ZM-11` |
| `S11 — Post-game` | `ZM-12` |
| `S12 — Full playthrough & hardening` | `ZM-13` |
| `Cross-cutting — no stage` | the five in §2 that have none, plus anything parentless |

**The epics themselves carry no sprint.** An epic spans sprints; putting `ZM-9` inside
the sprint its own children fill would double-count the stage in every report.

**S0-S7 have no sprint, deliberately.** They completed before the board existed in this
shape and hold no child tickets, so a sprint each would be empty -- and because a sprint
records `completedAt` when it is closed, not when the work happened, eight of them
would land eight zero-point entries on the same day of the velocity chart. The Done
epics in §2 are the audit trail for those stages; sprints would only add noise.

**One is ACTIVE at a time**, which the board enforces -- `sprint start` refuses while
another is active. `S8` is it.

★ **The sprints are on the BACKLOG page, not the board.** `ZM`'s board is `KANBAN`, and
per §6a a KANBAN board draws every issue while a SCRUM board draws only the active
sprint. Looking for the sprint structure on the board tab and finding an undifferentiated
wall of cards is the expected result, not a sign that no sprints exist.

★ **`zagent sprint` cannot rename or delete one.** It is
`list|create|start|complete|add|remove`; renaming and deleting exist only as server
actions in the app, so those two are done in the browser.

---

## 9. Filing a ticket

The description **is** the spec, and the loop refuses anything it cannot route.

```markdown
## Goal

<what outcome -- quote this directory rather than paraphrasing it>

## Definition of Done

- [ ] <observable outcome>
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run
```

**Never name a pin SITE in a Definition of Done.** That line used to read
*"bumped in Status.md AND zm-tests.yml AND zagent.project.json"*, which was true until
the baseline got one home (`Tools/unit_baselines.json`) and now points at two
`protectedPaths`. A DoD is inlined into the worker's prompt VERBATIM (I3), so a stale
one does not sit there harmlessly -- it competes with the tick's own instructions, and
the worker has no shell and no board with which to tell which half is current. Say the
OUTCOME; the plumbing is allowed to move.

The reachability scan now REFUSES a DoD naming a protected path at file time, so a
ticket written from the old template comes back exit 4 rather than failing hours later.

Omit `## Gates`. A ticket's own gate list **replaces** the category's rather than
adding to it, so pasting the Zenithmon list into a body is how a pinned baseline goes
stale -- and `echo ok` in a body would merge on `echo ok`. `create` refuses it.

Always pass `--category Zenithmon`; it selects the gate list, the conventions inlined
into the worker's prompt, the branching mode, **and now the board**.

---

## 10. Keeping this file and the board honest

```
zagent report epic --project ZM        # per-epic progress
zagent report velocity --project ZM    # completed sprints only
zagent history ZM-20                   # the field trail
```

If [Roadmap.md](Roadmap.md) and the board disagree, **the checkbox wins, scored
against its literal text** (ZM-D-162) -- and the board is what needs correcting. The
board is the queue and the audit log; this directory is the spec.
