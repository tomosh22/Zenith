---
id: "96b2443f-8e50-444e-8a6f-aa582252d57f"
key: "ZEN-79"
type: "BUG"
title: "A refused zagent claim must change nothing: reproduce the mutate-before-validate defect against Vault and fix it"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "HIGH"
complexity: "MODERATE"
risk: "HIGH"
storyPoints: 5
position: 79
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-20T02:51:36.670Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
links: [{"key":"ZEN-76","type":"RELATES_TO","reason":"Same observability gap from opposite ends. ZEN-76 is 'the contract verdict (exit 4) wins over repo-busy (exit 5)' for zagent check; that short-circuit is exactly why ZEN-79's defect cannot be reproduced live from inside a tick - the tick already holds a ticket, so any second claim returns 5 before the contract check is ever reached. ZEN-79's proof had to drive the write layer directly through dispatchWrite instead."}]
---
## Goal
Suggestions row 2026-08-24 (CRITICAL): *"zagent claim MUTATES BEFORE IT VALIDATES. A targeted claim on a needs-human ticket (reproduced on ZM-64) writes status To Do -> In Progress and assignee none -> the agent, THEN returns exit 4 refusing it."* That was the SQL board; the Vault CLI's `claim` is a new implementation (`Tools/Vault/cli/write.mjs`), and eight ZEN tickets were found assigned to the agent while still To Do on 2026-09-19, which is the same shape.

Reproduce against Vault: `zagent claim` on a `needs-human` ticket, on an EPIC, and on a ticket blocked by an open predecessor; assert that on exit 4 no field changed and no `board:` commit was made. Fix any path that writes before the contract check (validation must run inside `runOperation`'s prepare before any change is produced), and add Vitest coverage for all three refusals asserting zero changes.

## Definition of Done
- [ ] A refused claim (needs-human, epic, blocked) changes no field and makes no commit, proven by tests
- [ ] The eight stale assignments found on 2026-09-19 have a recorded cause (this defect or another) on this ticket
- [ ] The Suggestions row is resolved with this key


## Comments

ran on opus/medium via MODERATE+HIGH · category Engine

**Done** — merged to `master` as `7580898d`. All three Definition of Done items met. **No production code changed, because there is no defect.**

The CRITICAL Suggestions row this ticket cites described the OLD SQL board. It does not reproduce against Vault. Verified at the crux: `executeOperation` (`Tools/Vault/server/operations.mjs`) calls `options.prepare(index)` at line **321**, writes the journal at **368** and applies/commits at **369** — so no vault byte and no commit can exist before `prepare` returns a change set. Every refusal in `claim`'s prepare (`cli/write.mjs:621-656`) throws before the `changes` array is built at **653**. The only pre-`prepare` writes are the operation lock ref and `.git/vault/authority.json`; `recoverLocked` can commit there, but only to finish a *previous* operation's already-validated journal.

**The proof**: `Tools/Vault/test/claim-refusal-atomicity.test.mjs`, 5 tests. `needs-human`, EPIC, blocked-by-open-predecessor and queue-`next`-with-nothing-claimable each assert exit code, byte-identical vault files, unchanged `status`/`assignee`/`claimAttempt`, no `claimAttempt` in raw bytes, unchanged `git status`, unmoved HEAD and no added commit subject. The fixture sets `writes.commit = true` — unlike the other self-built CLI fixtures — so the no-commit assertion has something to detect, and a **fifth control test** runs the identical probe around an ACCEPTED claim so the four refusal tests cannot pass vacuously.

**The eight stale assignments have a cause, and it is not this code path.** ZEN-3, 12, 14, 16, 18, 19, 20 and 22 arrived already `To Do` + assigned at `3f7b3a59` *"Activate Vault from the frozen production board snapshot"* (2026-09-16), imported from the SQL board where the defect *was* real; `688273cc` then renamed `agent@saas.invalid` → `agent@zenith.invalid`, which is why they read as the current agent's. Eight `board: update` commits cleared them on 2026-09-19. Fossils, not a live defect — independent corroboration of the code reading.

**Gates** all green (`measure` exit 0, no pin movement; `verify` exit 0, 13 steps, nothing skipped; `guard` clean). **Review** (risk HIGH) returned no findings at all, having traced every assertion to source and confirmed the refusal regexes match the errors actually thrown.

**★ Worth flagging: no gate executed this diff.** `Tools/**` is in no category's `paths` and no gate line runs Vitest, so eleven C++ game builds went green without touching the change. The only real verification is a hand-run `npm test`: **`1 failed | 719 passed (720)`** against a pre-change baseline of **`1 failed | 714 passed (715)`** — +5 passing, zero regressions, the one failure (`test/saas-guard.test.mjs`) pre-existing and unrelated.

**Durable findings: ZEN-D-007, ZEN-D-008.** (1) The Vault write layer validates before it mutates, structurally — `runOperation` is the single door, so any future write command that validates *after* building its change set would silently reintroduce the SQL board's defect. (2) Board state imported by `3f7b3a59` is not evidence about Vault behaviour; check whether an anomalous row predates the activation snapshot before calling it a live defect.

Linked `relates ZEN-76`: repo-busy (exit 5) short-circuiting the contract check (exit 4) is exactly why this ticket could not be reproduced live from inside a tick.

<!-- agent:worklog:start ZEN-79 -->
## Work Log — 2026-09-20

ran on opus/medium via MODERATE+HIGH · category Engine

**Outcome**: merged to `master` as `7580898d` · ran on opus/medium via MODERATE+HIGH

**The finding: there is no defect to fix.** The CRITICAL Suggestions row this ticket cites was about the OLD SQL board. It does not reproduce against the Vault CLI, and no production file changed.

Verified at the crux, independently of the worker: `executeOperation` (`Tools/Vault/server/operations.mjs`) calls `options.prepare(index)` at line **321**; the journal is written at **368** and applied/committed at **369**. No vault byte and no commit can exist before `prepare` returns a change set. Inside `claim`'s prepare (`Tools/Vault/cli/write.mjs:621-656`) every refusal — invalid vault, missing ticket, breaker, contract verdict, transition, assigned-to-other — throws before the `changes` array is built at line **653**. The only pre-`prepare` writes are the operation lock ref and `.git/vault/authority.json`, neither board state nor a commit; `recoverLocked` can commit there, but only to finish a *previous* operation's already-validated journal.

**What changed**

- `Tools/Vault/test/claim-refusal-atomicity.test.mjs` — NEW, the only file. Five Vitest cases.

The worker created a new file rather than extending `project-action-refusals.test.mjs` for two reasons I checked and accept: that file exercises `server/project-actions.mjs` (the browser/API surface), not the CLI `dispatchWrite` path this ticket is about; and its fixture pins `writes.commit = false`, which would make the load-bearing "no commit" assertion untestable.

**Files** (`git diff --stat`)

```
 Tools/Vault/test/claim-refusal-atomicity.test.mjs | 258 ++++++++++++++++++++++
 1 file changed, 258 insertions(+)
```

**Gates**

`-Phase measure` 81s **exit 0** (no pin movement — Vitest tests are not C++ boot units) · `-Phase verify` **exit 0, all gates green**, 13 steps, `skipped: []`, `headSha` matching HEAD. `guard` exit 0, no violations, `gateSelectionStale: null`.

| Command | Result |
|---|---|
| `zenith build engine` + 10 further game builds | ok |
| `zenith build Combat --config D3D12_..._False` | ok |
| `zenith test Combat --headless` | ok |
| `run_unit_gate -Game Combat` | ok, unchanged (2706) |

No category was unioned in: the diff touches only `Tools/Vault/**`, which no category's `paths` covers.

**★ The gates executed none of this diff, and that is not a lapse — it is the gate list.** `Tools/**` is deliberately in no category's `paths` (so a one-line pin bump does not rebuild Combat), and no gate line anywhere runs Vitest. Eleven C++ game builds went green without touching a line of the change. The only real verification is the orchestrator's hand-run:

| `npm test` in `Tools/Vault` | Result |
|---|---|
| BEFORE (baseline, taken before dispatch) | `1 failed \| 714 passed (715)` |
| AFTER | `1 failed \| 719 passed (720)` |

**+5 passing, zero regressions.** The single failure is `test/saas-guard.test.mjs:83`, pre-existing on a clean tree and unrelated — it greps tracked files for the literal `saas`, which the new file never contains.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Tools/Vault/test/claim-refusal-atomicity.test.mjs` | **No gate.** Executed only by `npm test` / `npx vitest run`, run by hand (5/5 pass in 5.85s standalone; +5 in the full suite). Drives `dispatchWrite` — the same entry point real CLI invocations use — against a hermetic `fs.mkdtemp` git repo outside the checkout. |

**Definition of Done**

- [x] A refused claim (needs-human, epic, blocked) changes no field and makes no commit, proven by tests — five tests: those three shapes plus queue-`next`-with-nothing-claimable, plus a control around an ACCEPTED claim. Each refusal asserts exit code, byte-identical vault files, unchanged `status`/`assignee`/`claimAttempt`, no `claimAttempt` in raw bytes, unchanged `git status --porcelain`, unmoved HEAD and no added commit subject.
- [x] The eight stale assignments found on 2026-09-19 have a recorded cause — below, with git evidence. The cause is **not** this defect.
- [x] The Suggestions row is resolved with this key — row `h9vd7jt0tevtzwmdw5980ibb` moved to `done` with the reason.

**The eight stale assignments: cause established from git history.**

ZEN-3, ZEN-12, ZEN-14, ZEN-16, ZEN-18, ZEN-19, ZEN-20 and ZEN-22 arrived **already** in the `To Do` + assigned state at commit `3f7b3a59` *"Activate Vault from the frozen production board snapshot"* (2026-09-16), imported wholesale from the old SQL board — the one where mutate-before-validate was genuinely real. Eleven ZEN tickets came in already-assigned; the other three had since reached Done, leaving eight. Commit `688273cc` then renamed the retired identity `agent@saas.invalid` → `agent@zenith.invalid` across board data, which is why they read as assigned to the *current* agent. Eight `board: update` commits cleared them on 2026-09-19.

They are **fossils of the original defect, not evidence of a new one** — which corroborates the code reading above from an entirely independent direction. This also refutes the worker's own stated hypothesis (a hand edit or a status move that failed to clear the assignee); it labelled that a hypothesis rather than a finding, which is why checking it was cheap.

**Review** (required: risk HIGH is in `reviewOn`). Returned **no blocking and no non-blocking findings**, having traced every assertion to the production source it claims to exercise — `write.mjs`, `read.js`, `compat.mjs`, `legacy-pure/contract.mjs`, `operations.mjs`, `scanner.js`. It specifically confirmed the four refusal regexes match the errors actually thrown (so no test is passing on a different refusal than the one it names), that `writes.commit = true` makes the no-commit assertion real rather than trivial, that the control test is isomorphic and correctly inverted, and that each test builds a fresh temp repo outside the live checkout. It named six categories it did not check, including that it executed nothing itself.

**Durable findings**: ZEN-D-007, ZEN-D-008

- **ZEN-D-007 — the Vault write layer validates before it mutates, structurally.** `prepare` at `operations.mjs:321`, journal at 368, commit at 369. `runOperation` is the single door every CLI mutation passes through, so the rule generalises past `claim`: put validation inside `prepare`, before the change set is built, and atomicity is free. A future write command that validates *after* building `changes` would silently reintroduce the SQL board's defect.
- **ZEN-D-008 — board state imported by `3f7b3a59` is not evidence about Vault's behaviour.** Check whether an anomalous row predates the activation snapshot before treating it as a live CLI defect; a `+status:`/`+assignee:` pair in that commit means the state was inherited, not produced. This ticket was filed partly on exactly that misattribution.

**Deviations / follow-ups**

- **No file changed outside the recorded list**, and `filelist.txt` was not written for this ticket (the prompt was hand-assembled with the survey clause, since `build_prompt.ps1` does not emit it — ZEN-89). The one changed file, `Tools/Vault/test/claim-refusal-atomicity.test.mjs`, was named in the dispatch as a permitted new file.
- **A live reproduction is impossible from inside a tick, and that is ZEN-76.** The tick holds ZEN-79, so any second `claim` returns exit 5 (repo busy) before the contract check is reached — the lock short-circuits the exact path this ticket wants to observe. The tests therefore drive `dispatchWrite` directly. Linked `ZEN-79 relates ZEN-76` with that reason.
- **`Restore-RefusedClaim` no longer exists.** `.claude/commands/tick.md` step 2 states the client rolls a refused claim back and "prints what it did on stderr — read that line rather than assuming it happened". `grep -rn "Restore-RefusedClaim" Tools/` returns nothing; it went when `zagent.ps1` became a thin Node shim. The protocol instructs the orchestrator to read a line that can never be printed, describing the safety net for precisely this ticket's subject. Worth a Suggestions row against `tick.md`, which the loop may not edit.
- **Board writes must not straddle the gate run.** Resolving the Suggestions row mid-`measure` was nearly done and would have moved HEAD, tripping `guard`'s `headSha` freshness fold and failing a green ticket for an unrelated reason. Every board mutation on this backend is a commit. The protocol invites `comment`/`update`/label writes during step 6 and never warns about this.
- **The relevant suite was already red on arrival** (`1 failed | 714 passed`). The protocol covers red→green as a flake and red→red as real, both assuming a green starting point; nothing tells you to baseline the suite first. Doing so is the only reason the after-state could be stated as "zero regressions" rather than guessed at.
<!-- agent:worklog:end -->
