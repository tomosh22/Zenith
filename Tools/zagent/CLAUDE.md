# Tools/zagent

The `/tick` protocol and the machinery that runs it on **three** agent
harnesses — Claude Code, OpenCode, Codex — from ONE canonical source. See
`Tools/zagent/README.md` for setup (`PATH`, `zagent doctor`) and the client
layout (`zagent.ps1`, `ZagentClient.psm1`, `build_prompt.ps1`). This file is
about `protocol/`: what is canonical, what is generated from it, and how to
change either without hand-editing a harness's own files.

## `protocol/` layout

| Path | What it is |
|---|---|
| `protocol/TICK.md` | **Canonical. The only copy.** The harness-neutral rules — nine steps, seven invariants (I1–I7). Names no harness, tool or model: only the five control verbs (`DISPATCH`/`AWAIT`/`ABORT`/`SCHEDULE`/`STOP`), the two roles (`WORKER`/`REVIEWER`) and the three tiers (`haiku`/`sonnet`/`opus`). Hand-edited directly. |
| `protocol/roles/worker.md`, `protocol/roles/reviewer.md` | **Canonical.** The two role charters, hand-edited. `worker.md` carries a `{{PROTECTED_PATHS}}` placeholder, filled at generation time from `vault.config.json`'s own **resolved** guardrail list — the one `zagent guard` actually enforces — not from `zagent.project.json`'s copy of it (see "protected paths" below for why those two lists are not the same thing). |
| `protocol/roles/generated/{worker,reviewer}.resolved.md` | **Generated.** The two charters above with the substitution applied, and nothing else. Nobody hand-edits these. Today's reader is OpenCode's `{file:...}` (`bodyDelivery: "reference"`) — it points at these files rather than embedding a copy the way Claude's and Codex's generated role files do. |
| `protocol/harnesses.json` | **Canonical, as DATA.** One profile per harness key (`claude`, `opencode`, `codex`) plus a `_roster` block that is documentation of the model wiring, not itself read by the generator. Every `$comment*`-prefixed key is a note explaining the value beside it — read those before changing a value; they carry the Stage 0 measurements this file is built from. |
| `protocol/entrypoint.md.tmpl` | **Canonical template.** The shared body every harness's generated entry point is built from (Controls table, Roles table, review policy, pre-authorisation, human prerequisites) — filled with `{{...}}` placeholders from the harness's own `harnesses.json` profile. Claude's entry point does not use this template (its `deliveryMode` is `"inline"` with a hand-shaped frontmatter block, not this shared shape) — OpenCode's and Codex's do. |
| `protocol/tier-mentions.allowlist` | **Canonical, hand-maintained ratchet.** Every bare `haiku`/`sonnet`/`opus` mention inside `TICK.md`'s own narrative prose (measured history — a ticket's routing line, a cost note — never an instruction) is listed here by file:line, so a NEW bare mention doesn't slip into the harness-neutral document unnoticed. Re-derive per its own header comment; a shrinking list is fine, a new unlisted mention fails the gate. |
| `Build-HarnessArtifacts.ps1` | **The generator.** Reads everything above plus `vault.config.json`, emits every harness's entry point and role files, and `git rm`s files a harness's own move superseded. Idempotent — `-Check` regenerates to memory and byte-compares against disk instead of writing, same shape as `zenith regen --check`. |
| `Test-HarnessParity.ps1` | **The gate.** Checks the three harnesses stay in parity with each other and with `TICK.md`, including that this file exists and names every harness key `harnesses.json` declares — so a fourth harness added to the roster without a documented row here fails a gate instead of silently drifting. |
| `.claude/commands/tick.md`, `.claude/agents/zagent-{worker-{sonnet,opus},reviewer-{low,medium,high}}.md` | **Generated.** Claude's five role files plus its entry point, `deliveryMode: "inline"` (TICK.md's body copied in whole). |
| `.opencode/opencode.json`, `.opencode/commands/tick.md` | **Generated.** OpenCode's five agents live inline inside the one JSON(C) config file, under its top-level `agent` map — there is no `.opencode/agent/` directory the way `.claude/agents/` is a directory; `roleDir` in `harnesses.json` names the config file itself. The entry point is `deliveryMode: "inline"`, like the other two — see "stub vs inline", below for why it is not the pointer form its `{file:...}` support would allow. |
| `.codex/agents/*.toml` (five), `.codex/skills/tick/SKILL.md`, `.codex/config.toml` | **Generated.** Codex's five role files (one `.toml` each), its entry point (a **skill**, not a command — Codex has no repo-level slash commands), and its pre-authorisation layer. `deliveryMode: "inline"`, same shape as Claude's. |

**Parked, not generated:** `unusedTiers` in each harness's `harnesses.json`
profile carries a tier absent from `roles.worker` (currently `haiku` on all
three) as a `{tier: {model: ...}}` map entry rather than deleting it —
re-introducing a retired tier as a worker later is one entry moving back
into `roles.worker`, not a re-derivation of what model it should carry. The
generator emits role files **only** from `roles.worker` / `roles.reviewerByRisk`,
so an entry sitting in `unusedTiers` produces nothing, on any harness,
regardless of what it names.

## The three permitted differences

`harnesses.json` lets exactly three things vary per harness, and the reason
the list stops there is the same reason `TICK.md` is a single file: a rule
that reads differently depending on which harness is running it is a rule
nobody can audit from the text alone.

1. **Which model fills each role.** The `roles.worker` / `roles.reviewerByRisk`
   roster — see "how to swap a model", below. This is the axis expected to
   change most often, which is why it has its own one-line procedure.
2. **How much review a dispatch gets.** `review.mode`: `"payload"` (review
   exactly when `review.required` — the OR of `reviewOn` and the
   Definition-of-Done assertion scan — is already true) or `"always"`
   (review every dispatch regardless). There is no `"never"` value: a
   harness profile may only WIDEN what the payload already asks for, never
   narrow it. Codex is pinned to `"always"` today, conditionally — see
   `$comment_review_mode` and `$comment_reviewerByRisk_LOW` in
   `harnesses.json` for the exact condition and what flips it back to
   `"payload"`.
3. **How the five control verbs and `loopMode` resolve to concrete
   mechanism.** The `controls` table (`DISPATCH`/`AWAIT`/`ABORT`/`SCHEDULE`/
   `STOP`, each a harness-specific mechanism string or `null` when none was
   found) and `loopMode` (`"in-harness"` or `"manual"`). `entrypoint.md.tmpl`
   says this out loud: *"When TICK.md and this section disagree about what a
   verb or role MEANS on this harness, this section wins — TICK.md does not
   know this harness exists."* That license is scoped to verb/role/tier
   **resolution** only.

Everything else — the protocol's steps, its seven invariants, the
vocabulary itself, which paths are protected, what "review is required"
means — is identical on all three harnesses by construction: it lives in
`TICK.md`, which every harness's entry point either copies whole
(`deliveryMode: "inline"`) or points at whole (`deliveryMode: "reference"`/
`"stub"`), never a per-harness excerpt. A harness that needs a fourth axis
of difference to run `/tick` at all is a sign the protocol itself needs to
change, not that this list needs a fourth entry.

## `effortSettable` does not exist, and why

There is no `effortSettable` key anywhere in `harnesses.json`, deliberately.
`resolveRouting` (`Tools/Vault/cli/legacy-pure/routing.mjs`) takes a
ticket's **effort** from its **complexity alone**
(`zagent.project.json` → `policy.resolved.routing.effortByComplexity`) and
escalates the **model** from complexity **and** risk together, as
independent axes — a `SIMPLE`+`HIGH` ticket routes to `model: opus` at
`effort: low`. Because those two axes are independent, no single per-tier
role definition could carry "the right effort" for every ticket routed to
it: the opus-tier worker role serves both a `COMPLEX` ticket (wants high
effort) and a `SIMPLE`+`HIGH` one (wants low effort) with the same role
file. A key with exactly one legal value per role would be dead data.
`routing.effort` stays advisory on **every** harness — inlined into the
worker's prompt as a number the worker reads, never wired into a role
definition or a harness config field.

Codex *can*, mechanically, pin `model_reasoning_effort` per agent role
(Stage 0 probe 2 — a normal `Config` field a role's `config_file` layer
inherits, same as any other harness-side model knob). That settability is
real and useless here for the reason above: there is nothing correct to pin
it to per role.

## `unusedTiers` is about the worker LADDER, not about models

Read `unusedTiers` as "this tier has no role a ticket can be routed to
**as a worker**", not as "this model is unused". Codex's `gpt-5.6-luna` is
the clearest case: it is **unreachable** as a worker (`haiku` is absent
from `roles.worker` — `TRIVIAL` was retired from the complexity vocabulary,
so nothing routes there any more) and simultaneously **live** as Codex's
LOW-risk reviewer, in the same file, a few keys away. "Is `luna` live?" has
two different correct answers depending on which role you mean — say which
one you mean when you ask, and when you answer.

## `harnesses.json` being a protected path does not make model-swapping harder

`Tools/zagent/protocol/**` (which includes `harnesses.json`) is in
`protectedPaths`. That binds the **loop** — an unattended `/tick` may never
edit it, on any harness, the same as `.claude/**` or `zagent.project.json`
itself. It does not bind the **repo owner**: a loop that could downgrade
its own reviewer model, or widen its own worker's tool access, would have
no reviewer and no enforcement worth the name. Swapping a model, adding a
harness, or changing a review policy is a human editing this file directly
and re-running the generator — nothing about the protected-path mechanism
makes that harder, slower, or in need of a ticket.

## How to swap a model

One line, then one command:

1. Edit the model string in `harnesses.json`, inside the target harness's
   own `roles.worker.<tier>.model` or `roles.reviewerByRisk.<risk>.model`
   (and update the matching line in the `_roster` comment block at the top
   of the file — it is documentation of intent, not the read path, but a
   drifted comment is worse than none).
2. Run `pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1 -Harness <id>`,
   naming the harness you just edited.

`-Harness <id>` exists **specifically** for this operation: passing it
restricts generation (and, under `-Check`, comparison) to that one
harness's own artifacts, each emitter reading only its own
`harnesses.<id>` branch of the JSON — so a Claude-only model swap can never
produce a diff in OpenCode's or Codex's generated files. Omitting
`-Harness` defaults to `claude`; there is no "generate all three" mode,
because a change to one harness's roster is not a reason to touch the
other two on disk.

## How to add a fourth harness

1. **Profile it.** Run the harness's own equivalent of Stage 0's probes:
   does it deny worker shell/spawn tools structurally or only by
   convention, does it support project-scoped agent/role config, does it
   read a repo-relative file reference at config time (`bodyDelivery:
   "reference"`) or need the charter's text embedded (`bodyDelivery:
   "generated"`), what pre-authorisation mechanism exists for gate
   commands. Write the findings down (a `Docs/TickProtocolHistory.md`
   entry, dated, with a confidence label — `measured`/`inferred`/
   `unanswerable-without-human` — is the standing convention; see that
   file's 2026-09-21/22 entry for the shape).
2. **Add `protectedPaths` entries in `vault.config.json`** (both
   `policy.board.guardrails.protectedPaths` and
   `policy.resolved.guardrails.protectedPaths` — the two copies
   `guardrailsOf` actually reads) for whatever directory the new harness's
   config/role files live under, mirroring `.codex/**` / `.opencode/**`
   above. Skipping this means the loop could edit its own new harness's
   role definitions — the exact hole ZEN-6's predecessor closed for the
   other two.
3. **Work out its pre-authorisation mechanism** and record it under
   `preauth` in the new harness's `harnesses.json` profile (`kind` +
   `sources`), same shape as the other three.
4. **Decide `loopMode`.** `"in-harness"` only if you found a real, scriptable
   or in-app mechanism to re-invoke `/tick` unattended (Claude Code's
   `ScheduleWakeup` is the only one that qualifies today); otherwise
   `"manual"`, and say so in `humanPrerequisites`.
5. **Run `Tools/zagent/Test-HarnessParity.ps1`.** It asserts this file
   documents every harness key `harnesses.json` declares — so a fourth
   harness added to the roster with no matching section here fails a gate
   immediately, rather than drifting silently the way a fourth copy of a
   pinned baseline used to.

## Sharp edges

- **Never hand-edit a generated file.** Every file under "Generated" above
  is overwritten byte-for-byte the next time `Build-HarnessArtifacts.ps1`
  runs; a hand edit survives exactly until someone regenerates, at which
  point it is gone with no warning beyond `-Check` having already been
  green before your edit and red after it.
- **The "stub vs inline" `deliveryMode` choice is provisional, not a
  preference — and all three harnesses are `"inline"` today.** OpenCode was
  briefly `"stub"`, because it is the one harness with a real `{file:...}`
  reference mechanism. Two things made that wrong. Its stub entry point did
  **not** use `{file:...}` at all — it told the session to go *read* `TICK.md`
  in full — so it depended on that harness pulling ~1950 lines in a single
  read; and the 2000-line figure bounding that is **Claude Code's** Read-tool
  default, never measured for OpenCode. `"stub"` is unmeasured on every
  harness: the design wanted two or three real ticks run each way and that
  comparison has not happened, so `"inline"` is the form with production
  evidence behind it. Flipping any harness back is one key in
  `harnesses.json`; the emitters read it, and `Test-HarnessParity.ps1`'s
  headroom assertion **re-arms the moment some harness is `"stub"`** and
  fails with a margin someone has to justify. That is the signal that should
  gate the decision — not a green gate achieved by shrinking `TICK.md`, which
  is history, not documentation.
- **A literal `'''` inside either role charter breaks Codex's TOML
  emission.** `developer_instructions` is written as a TOML multi-line
  *literal* string (`'''...'''`) specifically to survive the backslashes,
  backticks and double quotes the charters already contain; the one thing
  that form cannot survive is the delimiter itself appearing in the body.
  `Build-HarnessArtifacts.ps1` asserts this on every run, for every
  harness, not just when generating Codex's artifacts — so the charter
  authors don't need to remember which downstream harness would break.
