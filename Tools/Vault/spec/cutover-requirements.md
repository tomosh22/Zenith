# Stage 4 cutover requirements

Status: Stage 4 review and disposition, updated 2026-09-16. The original
review preceded `write-design.md`; the engine, mutation CLI and isolated
rehearsal are now implemented. This document retains the review criteria and
records the chosen direct-checkout policy. It is not evidence of live cutover.

## 1. DP integration contract must be chosen

The repository currently names three different DP integration contracts:

- `zagent.project.json:36-40` sets the checkout default to `master`,
  `branching: "direct"`, and `push: false`.
- `zagent.project.json:163-176` overrides that for `DevilsPlayground` with
  `branching: "branch"` and `branchPrefix: "agent/"`.
- `Tools/zagent/protocol/TICK.md#6-verify` requires `switch <baseBranch>` plus
  `merge --ff-only <branch>`, while
  `Games/DevilsPlayground/Docs/CIPolicy.md:7-30,59-69` describes protected
  `master` and squash PRs. `Games/DevilsPlayground/Docs/Status.md:14` still
  describes direct-to-master operation.

### Resolved disposition

The Vault writer uses the canonical checkout directly for every project,
including DP. Generated mutable policies set `branching: "direct"`, remove
the legacy `branchPrefix`, and keep `push: false`; the old DP branch setting is
retained only as historical input and must not be copied into a Vault config.
The Vault engine does not create, switch, merge, or delete branches. This is
the chosen local contract, rather than an assertion that the legacy branch and
squash-PR paths are compatible with board commits.

The direct-mode throwaway test must cover: clean checkout → claim → feature
edits and gates → board close-out; a moved checkout or base reference → refusal
with scratch preserved; and a crash after the feature commit but before
close-out → recovery without replaying or losing that commit. The current
`push: false` policy has no remote PR or authentication step.

The disposition is an implementation choice, not production cutover evidence.
The remaining live blockers are a frozen writer set, no ticket in progress, one
fresh snapshot with complete entity/reference audit, retained dump and backup
hashes, and an explicit switch commit. Until those are recorded, the launcher
must remain on the legacy writer.

## 2. Human takeover is an ownership transition

The plan explicitly says a human takeover is a live board mutation that cannot
be buffered (`the-current-staged-work-stateful-kitten.md:231-252`). The
write design must carry an attempt ID and expected owner/revision through every
mutating call. A prior `owns` result is only a check, not permission to write
later.

At minimum, exercise these cases in a throwaway clone:

| Race | Required result |
| --- | --- |
| Human changes assignee/status before the worker returns | The next ownership check aborts. The agent posts its work log as a comment only, changes no status or assignee, and preserves branch/scratch. |
| Takeover while gates or review are running | Do not integrate or finish when they return. A gate result is not ownership. Preserve the branch and make the takeover visible to the human. |
| Takeover after feature commit but before close-out | The agent must not overwrite the human’s state. If the commit already landed, report the exact commit and leave status/assignee untouched. |
| Human edits a ticket file during an operation | The stale hash/revision check rejects the save; no agent write may overwrite the edit. |

This path is different from an ordinary failure while the agent still owns the
ticket. An ordinary failure may restore the persisted `previousStatus` and
clear the assignee; the takeover path must keep the new assignee. The only
durable source for `previousStatus` is the claimed ticket scratch described in
`Tools/zagent/protocol/TICK.md#5-dispatch-the-worker`. `Tools/zagent/protocol/TICK.md#7-integrate` requires comment-only
abandonment and explicitly forbids clearing the assignee.

`human-gate` is a third state, not a takeover: the agent may complete the
technical work and gates, then park at `In Review`; it must never auto-close or
retry after a human decision. The chosen DP integration policy must state
whether the human reviews the feature branch or the integrated commit.

## 3. Gate verdicts need a precise consumption boundary

`Tools/tick_gates.ps1` writes `state`, `exitCode`, `headSha`, `steps`,
`skipped`, and `deferred`; it writes `state: running` before work and rotates
prior scratch. The current script does not provide the proposed `inputsSha256`
content binding. Stage 4 must choose either that mutable-tree change detector or
an immutable gate snapshot before describing a result as certified.

If the change-detector option is chosen, the contract must require all of the
following before integration:

- `state` is `complete`, `exitCode` is zero, `headSha` is the expected base
  commit, and the verdict is consumed before any source edit or commit;
- the verify phase, rather than the measure phase, ran every selected gate;
  `deferred` is not proof and a non-empty `skipped` list voids all later gates;
- the recorded `gates.json` selection still matches the current changed set;
- the digest includes status codes plus content for existing paths, so deletes
  and renames cannot disappear, and explicitly excludes only documented
  gate-produced scratch/build outputs;
- any tracked file produced by a gate is part of the candidate and forces a
  re-verification if it was not present in the measured set.

The result must say that this detects changes that persist to consumption. It
must also test and document the known blind spots: a change restored before
consumption, ignored build artifacts that affect a rebuild, and changes after
verdict consumption. A killed process must leave `running` and never be read as
green. A watchdog timeout, if enabled, is an infrastructure failure, not a
green verdict. The mandatory order remains
`owns → measure → edits → reviewer → verify → guard → coverage → owns` from
`Tools/zagent/protocol/TICK.md#5-dispatch-the-worker`; the final ownership check must cover the
integration/finish race.

Required verdict tests include: edit during verify; delete and rename; ignored
gate output; tracked gate output; mutate-and-restore; hard kill; a failing gate
with named `skipped` lines; a clean measure with deferred lines; stale
`headSha`; and a changed gate selection. The DP gate fixture must exercise the
configured headless build, D3D12 false build, and headless test lines from
`zagent.project.json:171-175`.

## 4. Cutover and recovery are separate from readiness

Stage 4 must not claim cutover completion. The plan’s sequence is: freeze the
tick loop and the previous board's UI, require no ticket `In Progress`, export once from one
snapshot, verify counts/entity/reference parity, retain `pg_dump`, tag the
pre-cutover SHA, switch writers in one commit, and watch one small chosen first
ticket (`the-current-staged-work-stateful-kitten.md:285-310`). There must be no
dual writes.

Recovery must be tested at the cutover boundary:

- before the first Vault claim: restore the dump, revert the switch commit, and
  resume the previous board application;
- after the first claim: use forward recovery through the committed `board:`
  changes (for example, `git revert` to the cutover tag), with no automatic
  reconciliation back into Postgres.

The write design is not ready for approval until it specifies the shared lock
for CLI/app/worktree writers, operation IDs, expected before/after hashes,
atomic same-directory replacement, stale-save behavior, recovery after partial
multi-file completion, and path-scoped commits. These are explicit plan
requirements (`the-current-staged-work-stateful-kitten.md:267-281`), not
implementation details that can be deferred to the editor.
