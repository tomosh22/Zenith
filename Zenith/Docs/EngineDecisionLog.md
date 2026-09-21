# Engine -- Decision Log

**Purpose:** Append-only record of every non-trivial decision made about the
shared engine — `Zenith/**` and `Build/**` (Sharpmake), the code and project
structure every game links and compiles against. Future agents grep this when
investigating "why was X done this way?". This is the Engine category's
`decisionLog` (`zagent.project.json`), so it is what a loop worker on a `ZEN`
ticket reads with the Read tool — it has no shell and no network, so a ruling
recorded only on the board is invisible to it.

**Scope, and how it differs from a game's own log.** `Games/Zenithmon/Docs/DecisionLog.md`
and `Games/DevilsPlayground/Docs/DecisionLog.md` record decisions about one
game. This file is for decisions that are engine-wide by nature: a change to
`Zenith/**` or `Build/**`, a library-boundary or Sharpmake-project-membership
ruling, a cross-game convention, or a build-system invariant. A decision that
is really about one game's use of the engine belongs in that game's own log,
even if the diff happens to touch engine code to get there.

**Format:** One entry per decision, **newest first**. Fields per entry: date,
id, decision, why, tests-that-lock-it, reversibility. Ids are `ZEN-D-NNN`,
assigned in chronological order (so the highest number is at the top of the
file).

**What counts as non-trivial:** anything involving trade-offs, anything
another subsystem or game depends on, any library-split or project-membership
ruling, any change to what the gate lists in `zagent.project.json` cover, any
convention that binds future engine work. Tuning-value changes go in git
history, not here.

**On the numbering gap:** rulings up to at least `ZEN-D-015` were already
minted before this file existed — `zagent decide` had no configured
`decisionLog` for the `Engine` category (a `null` decisionLog resolved for
every `ZEN` ticket, the defect this file's existence fixes), so those earlier
entries live only in `Zenith/Docs/Board/tickets/*.md` and the Vault's
decisions database, never in a file. They are **not** backfilled here —
inventing their prose from a ticket cross-reference would be worse than the
gap it patches. Every entry from this file's creation onward lands in both
places.

---

<!-- Newest entries go directly below this line, in the format:

## YYYY-MM-DD -- ZEN-D-NNN -- <one-line summary of the decision>

**Decision:** <what was decided, stated as a ruling, not a narrative>

**Why:** <the trade-off or constraint that forced this choice>

**Tests that lock it:** <the unit(s)/gate(s) that would fail if this were violated>

**Reversibility:** <how costly it would be to undo, and what would need to change>

-->
