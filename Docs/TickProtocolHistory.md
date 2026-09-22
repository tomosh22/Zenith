# The `/tick` protocol — how its own wording changed

`Tools/zagent/protocol/TICK.md` is loaded in full on **every** tick. It carries the
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
could not tell it had been cut. Fixed in upstream `9d03869` (2026-08-25) and
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

## The `$?` mechanism ("Running the CLI" section)

**Corrected, not deleted — the rule survived, the explanation did not.** This
section used to say *"in a POSIX shell `$?` is the LAST command's."* That is
true of a pipeline and false of a `;` suffix: `$?` is expanded BEFORE `echo`
runs, so a `;`-suffixed call actually holds the CORRECT code in the text
(`( exit 42 ) ; echo "$?"` → `42`). What lies is not the shell — it is the
harness's completion notification, which reports the COMPOUND statement's
exit code:

```
pwsh tick_gates.ps1 -Phase measure   →  file:      10
                                        JSON:      10
                                        harness:    0   ← the notification
```

That measure run was reporting `10 — MEASURED: a pin needs bumping`. An
orchestrator branching on the notification instead of the file reads *all
gates green*, skips the pin, and commits a stale baseline.

`.zagent/last.exit` (added afterward) makes the question moot for `zagent`
and `tick_gates.ps1` calls — the file is written before the value ever passes
through a shell — but the corrected mechanism stays here, because a wrong
explanation of a subtle POSIX rule is exactly the kind of thing that gets
copied forward confidently.

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

Fixed in upstream `05408ef`; five consecutive timeouts became **exit 0 in 9
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

## Three harnesses, one protocol (2026-09-21/22)

`Tools/zagent/protocol/TICK.md` stopped being Claude-Code-only prose and
became the harness-neutral canonical source for three harnesses — Claude
Code, OpenCode, Codex — each with its own generated entry point
(`Tools/zagent/Build-HarnessArtifacts.ps1`, profiles in
`Tools/zagent/protocol/harnesses.json`). `Tools/zagent/CLAUDE.md` is the
standing reference for the mechanism; this entry is the measured record
that justified the choices baked into it.

### The eleven Stage 0 probe results

Measured 2026-09-21, against Codex desktop build `26.915.31945` / bundled
CLI `codex-cli 0.155.0-alpha.9.2`, and the OpenCode desktop Electron app
`1.18.31` (**not** the separate standalone `opencode-cli.exe 1.14.33`,
an older, different binary). Full evidence trail: the probe transcript this
table summarises. Confidence is stated per the same three labels this
file's own header uses for the rest of its content:
`measured` / `inferred` / `unanswerable-without-human`.

| # | Question | Verdict | Confidence |
|---|---|---|---|
| 1 | Codex shell + spawn denial via an agent `config_file` | Mechanism is real (`codex_agent_roles` crate; `shell_tool`/`multi_agent`/`multi_agent_v2` are genuine toggleable features). The live behavioural run hit the account's usage cap before a model turn completed. Conservative reading kept: `workerToolDenial: "declared"`. | inferred |
| 2 | Codex per-agent `model_reasoning_effort` | A normal `Config` field a role's `config_file` layer inherits, same as any other harness-side model knob. Informational — gates nothing, because no per-role effort value is correct in the first place (see `Tools/zagent/CLAUDE.md` → "`effortSettable` does not exist"). | inferred |
| 3 | OpenCode repo-level `permission: {bash: deny, task: deny}` | Works: a top-level `Config` field, deep-merged project-over-global, enforced by a real allow/deny/ask ruleset at tool-execution time. | measured |
| 4 | OpenCode `{file:}` resolution from a repo-level config | Resolves against **the declaring config file's own directory**, applied to the raw text before JSONC parsing — so `.opencode/opencode.json`'s `{file:../Tools/...}` lands on the real repo path. → `bodyDelivery: "reference"`. | measured |
| 5 | Directory spelling (agent/agents, command/commands) | OpenCode accepts singular OR plural for all three. Codex accepts **plural only** (`.codex/agents`). Plural pinned everywhere, matching `.claude/agents/`. | measured (OpenCode); measured+inferred (Codex) |
| 6 | Codex pre-authorisation | `approval_policy` is a real, `-c`/TOML-settable config key. `trust_level` (the setting that actually relaxes approvals per project) is **machine-local**, under `~/.codex/config.toml`'s `[projects.'<path>']`, and is **not repo-committable**. `[windows] sandbox = "elevated"` parses but its backing feature is disabled in this build — it enforces nothing today. | measured (flags); inferred (repo-level `.codex/config.toml` layering, at the time of this probe — see correction below) |
| 7 | Codex Automations (scheduled repeat invocation) | Feature exists and is enabled, but no CLI or config-file surface was found to author one — GUI-only. → `loopMode: "manual"`, `controls.SCHEDULE: null`. | inferred |
| 8 | OpenCode `opencode-go` model ids | `opencode-go/glm-5.3`, `opencode-go/glm-5.3-flash`, `opencode-go/deepseek-v4.1-flash` all confirmed present in `~/.cache/opencode/models.json`. | measured |
| 9 | Does OpenCode read a project-level `.opencode/opencode.jsonc`? | Half right: the project-config candidates are exactly `./opencode.json`, `./opencode.jsonc`, and `.opencode/opencode.json` — the `.jsonc` spelling **inside** `.opencode/` is not a candidate. All three go through the same JSONC-tolerant parser regardless of extension, so comments are legal in `.opencode/opencode.json` despite the `.json` name. | measured |
| 10 | `subagent_depth` | **Present and enforced**, contradicting the plan's premise that it was absent. Default `1`; enforcement walks `parentID`. At the default, a depth-0 orchestrator may spawn a depth-1 subagent, which may not itself spawn further — plus a second layer, an auto-added `task: deny` permission on the spawned child. `1` is already the smallest value that still lets an orchestrator dispatch — shipped as-is, not raised. | measured |
| 11 | Codex's repo-level entry point | `.codex/skills/tick/SKILL.md`, invoked `$tick` — **not** `.agents/skills/`, which an earlier plan had assumed. Two independent code paths in the bundled app agree: the "loaded skill" activity literally shells `cat .codex/skills/${id}/SKILL.md`, and the install helper joins the project root with `.codex/skills` when installing into a project. `AGENTS.md` and `.codex/config.toml` are real, always-scanned per-project artifacts, but `AGENTS.md` is always-on context, not an on-demand runner the way a skill is. | measured (directory + discovery); inferred, strongly (the exact `$name` invocation grammar — seen in UI mention-classification code, not a written spec) |

Also measured, standalone: Codex model ids `gpt-5.6-luna` / `gpt-5.6-terra`
/ `gpt-5.6-sol` all present in `~/.codex/models_cache.json`; Codex's
`features.multi_agent` is not explicitly set in the user's own config but
defaults to `true` (stable).

### Four places the design's premises were wrong, and measurement corrected them

1. **OpenCode's project config file is `.opencode/opencode.json`, not
   `.jsonc`.** The design assumed the `.jsonc` spelling; probe 9 found the
   `.opencode/`-scoped candidate is `.json` only (root-level `opencode.jsonc`
   is separately valid, but that is not the same path).
2. **`{file:}` DOES resolve from a repo-level config** — against the
   *declaring file's own directory*, not against the process cwd or the
   repo root. The design had not confirmed this resolved at all from a
   project-scoped config; probe 4 confirmed it does, and pinned exactly
   which directory it resolves against.
3. **`subagent_depth` is present and enforced**, with a default of `1`
   already being the smallest value that lets an orchestrator dispatch —
   the design's premise was that this setting was absent from the OpenCode
   versions in play (1.14.x/1.18.x). It is not, on either.
4. **Codex's repo entry point is `.codex/skills/tick/SKILL.md`, invoked
   `$tick`** — not `.agents/skills/`, which the design had assumed as the
   directory. Probe 11 found two independent code paths agreeing on
   `.codex/skills/`.

### The stub-vs-inline decision, and why it is provisional

The design wanted a real measurement — two or three ticks run each way —
of whether a worker follows a **stub** entry point (a pointer telling it to
go read `TICK.md` itself) as reliably as an **inline** one (the full body
copied in). **That measurement has not been run**, and until it is, every
harness ships `deliveryMode: "inline"` — the form ALREADY PROVEN IN
PRODUCTION, since `.claude/commands/tick.md` was a full inline copy before
this work started. Both forms are a one-line branch in
`Build-HarnessArtifacts.ps1`, so running the deferred measurement later and
flipping a harness is cheap.

**OpenCode shipped as `"stub"` for part of this pass, and was changed back.**
It looked like the one harness where a pointer was safe, because it has a real
`{file:...}` reference mechanism (probe 4). Two things made that wrong:

- **Its stub entry point did not use `{file:...}` at all.** The emitted file
  told the session to go *read* `TICK.md` in full. `{file:...}` is what
  delivers the ROLE bodies (`bodyDelivery: "reference"`, which is correct and
  stays) — a different axis entirely from how the ENTRY POINT is delivered.
  Conflating the two is what made a pointer look measured when it was not.
- **The limit it was being judged against is the wrong tool’s.** The 2000-line
  window is **Claude Code’s** `Read` default. Nobody has measured OpenCode’s.
  So the one harness using stub was depending on an unmeasured read limit,
  bounded by a number borrowed from a harness that does not use stub at all.

**Read-window headroom — a measurement with an expiry date, not a property.**
117 lines when first checked; **98** after the protocol was neutralised (1902
lines); **48** now, at 1952 lines, having moved twice during this pass alone.
The trend is the finding: the protocol grows, because nearly every addition to
it is a recorded failure someone paid for.

**`Test-HarnessParity.ps1` binds that assertion only while some harness is
`"stub"`.** With all three `"inline"` it prints the margin as INFORMATIONAL and
passes; flip any harness to `"stub"` and it fails immediately, naming the
harness and the margin. That round-trip was verified in both directions on this
pass. **The gate is deliberately shaped so the cheap way out is not available:**
it went red once here and the wrong fix — trimming `TICK.md` to make it green —
was attempted and reverted. Shrinking the protocol to silence the tripwire
destroys the history the tripwire exists to protect. If it reds, either measure
the harness’s real read window and justify the number, or go back to `"inline"`.

### Still open — needs a human

| Item | Exact step |
|---|---|
| Codex probe 1's behavioural confirmation | The account hit its usage cap mid-probe. After it resets, from a scratch (non-project) directory: `codex exec -c features.shell_tool=false -c approval_policy="never" -s read-only -C <scratch> "try to run 'dir' via a shell tool and report exactly what happened"`, then again with `-c features.multi_agent=false -c features.multi_agent_v2=false` and a prompt asking it to spawn a sub-agent. If BOTH refuse, `codex.workerToolDenial` upgrades to `"enforced"` in `harnesses.json`. |
| Whether a repo-level `.codex/config.toml` is truly loaded as a config layer | Strongly evidenced (the app's own UI text offers to edit "the project's `.codex/config.toml`", immediately followed by a TOML parse and a layered config resolver call) but not behaviourally confirmed end to end. Create `.codex/config.toml` with `approval_policy = "never"` in a **throwaway repo, never Zenith**, and check whether `codex exec` there picks up the override with no `-c` flag. |
| Whether Codex Automations can target a repo skill | GUI-only surface, no CLI or config-file authoring path was found. Open the Codex desktop app, Settings → Automations (or wherever the current build puts it), and check whether an automation can point at `.codex/skills/tick/SKILL.md` rather than only an ad-hoc prompt. If it can, `codex.loopMode` flips from `"manual"` to `"in-harness"`. |

None of the three blocks anything downstream: `workerToolDenial: "declared"`
and `loopMode: "manual"` are the conservative defaults that hold until one
of these is re-run and comes back the other way.

### The conditional on Codex's LOW-risk reviewer

`harnesses.json` sends Codex's LOW-risk reviews to its **cheapest** model
(`gpt-5.6-luna`), which is defensible only because Codex is also the
harness most likely to run `review.mode: "always"` — if every dispatch is
reviewed, LOW-risk reviews are the highest-volume, most-routine band, and a
cheaper model there is a deliberate cost trade. That argument is
**conditional on probe 1 staying open**: if probe 1 is re-run and Codex
turns out to *enforce* shell/spawn denial, `review.mode` drops from
`"always"` to `"payload"`, LOW-risk reviews become as rare as on the other
two harnesses, and the volume premise evaporates — leaving only "the
smallest model reviews the band the DoD-assertion scan targets", which the
protocol argues *against* (a LOW-risk ticket's `review.required` is set
precisely because its DoD asserts something a gate cannot settle — exactly
the finding a small model is least equipped to catch). The fix, when that
day comes, is one line: `harnesses.json`'s
`harnesses.codex.roles.reviewerByRisk.LOW.model` → `gpt-5.6-terra`. Written
down here, beside the roster, rather than in a stage report nobody re-reads.

## Amendments to the "never edit this file" rule

`tick.md` says the loop may never edit `.claude/**` or `zagent.project.json`,
and that rule is unchanged and mechanically enforced by `zagent guard`. It
binds the LOOP. It does not bind the repo owner, who has directed three rounds
of edits outside a tick — 2026-08-24 (nineteen findings, after three live
ticks), 2026-08-24 again, and 2026-08-26 (this round, after three more).
Recorded because a file that says "never edited" beside a git history showing
edits is worse than either.
