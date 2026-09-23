# `zagent` — the Vault board client

## Zenith Vault

Zenith Vault is the agent board for this checkout — there is exactly one
backend, it lives at `Tools/Vault/`, and it needs no network credential:
Vault uses this checkout as its own single authoritative working tree,
and `zagent.project.json` declares direct integration for every
category, including DevilsPlayground.

Start the local Vault app from the repository root:

```powershell
node Tools/Vault/server/http.mjs --vault . --port 4317
```

It listens at `http://127.0.0.1:4317`. The CLI and browser use the same
Vault files and operation lock. Vault requires Node 22 and the
dependencies under `Tools/Vault`; it needs no board URL, bearer token,
board account, or other remote credential. See
[Vault](../Vault/README.md) for the app, tests, migration, and recovery
boundary.

## `zagent.ps1` — a dispatch shim, nothing else

`Tools/zagent/zagent.ps1` is a thin PowerShell shim. It resolves this
repo's root from its own location (`$PSScriptRoot/../..` — `Tools/zagent`
always lives two levels under the repo root, regardless of the caller's
current directory) and hands off every argument, verbatim, to:

```
node Tools/Vault/cli/client.mjs --vault <repoRoot> <args...>
```

then exits with that process's own exit code. It adds nothing of its
own beyond one failure mode: if `node` or `Tools/Vault/cli/client.mjs`
cannot be found, it prints a message and exits `1`.

There used to be a second, remote-HTTP backend here, selected by a
`Tools/zagent/backend.json` config file — a PowerShell client speaking
to a board that could run on an entirely different machine, needing a
board URL and a per-machine bearer token configured as environment
variables on this one. It, `backend.json`, and every line of
`zagent.ps1`/`ZagentClient.psm1` reachable only through it were removed:
there is now exactly one backend and nothing left to select.
`Tools/Vault/cli/tick-checks.mjs` is the live equivalent of the
tick-protocol checks that backend used to carry (file-list excursions,
commit completeness, `blocked-scope` defensibility, gate-verdict
freshness), wired into `Tools/Vault/cli/write.mjs` and `client.mjs`.

Put `Tools\zagent` on `PATH` so a bare `zagent` resolves. **Prepend it,
do not append it** — `PATH` is ordered, and a machine that once had an
older `zagent` installed (a pnpm/npm global shim, say) still has that
shim earlier in the list. Appending leaves the stale one winning.

```powershell
$path = [Environment]::GetEnvironmentVariable('PATH', 'User')
[Environment]::SetEnvironmentVariable('PATH', "C:\dev\Zenith\Tools\zagent;$path", 'User')
```

Confirm the right one won — this should print a path inside this repo:

```powershell
(Get-Command zagent).Source
```

Verify with `zagent doctor`.

## Where things live

| | Where | Why |
|---|---|---|
| Gates, pinned baselines, conventions, categories | `zagent.project.json`, repo root, **committed** | A pinned unit count already lives in `Status.md`, `zm-tests.yml` and `run_unit_gate.ps1`. A fourth copy on a machine you cannot see could only be remembered. |
| The `/tick` protocol | `Tools/zagent/protocol/TICK.md` | It is this repo's operating procedure — harness-neutral, the only copy. |
| The three per-harness entry points | `.claude/commands/tick.md`, `.opencode/commands/tick.md`, `.codex/skills/tick/SKILL.md` | **All generated**, never hand-edited — see `Tools/zagent/CLAUDE.md`. `/tick` runs today on Claude Code (`/tick`), OpenCode (`/tick`), and Codex (`$tick`, a skill mention — Codex has no repo-level slash commands). |
| The generator + the harness profiles | `Tools/zagent/Build-HarnessArtifacts.ps1`, `Tools/zagent/protocol/harnesses.json` | Turns the canonical protocol + role charters into every harness's own entry point and role files. `-Harness <id>` scopes a run to one harness; `-Check` reports staleness without writing. |
| The cross-harness gate | `Tools/zagent/Test-HarnessParity.ps1` | Checks the three harnesses stay in parity with each other and with `TICK.md`. |
| Run scratch | `.zagent/`, **gitignored** | The tick treats a dirty tree as fatal, so `git status --porcelain` must never see it. A `claim`/`next` **through `zagent` (i.e. `cli/client.mjs`)** that finds an existing `.zagent/run/<KEY>/` renames it to `<KEY>.prev/` (replacing an older `.prev`) BEFORE writing the fresh scratch, and reports `archivedPriorRun` on the result — so a stale `body.md`/`ticket.json`/gate verdict from a previous attempt is never sitting where the new one is read from. ★ The `npm run vault` entry point (`cli/zagent.mjs`) writes the same run scratch and does **not** archive, so a claim made through it can leave a stale prior run in place. |
| The agent account, model routing, guardrail floors | the board | Policy, not topology. |

`zagent.project.json` is **sent with every request** rather than
registered once. A stored copy goes stale, and a stale gate list is
exactly how a green run ratchets the wrong baseline.

## Client layout and tests

| File | |
|---|---|
| `zagent.ps1` | the dispatch shim described above |
| `ZagentClient.psm1` | the handful of pure helpers `build_prompt.ps1` still calls standalone: `Find-ClientRepo`, `ConvertTo-PosixPath`, `Write-StdErr`, `Get-ScratchRoot`, `Write-LastExit` |
| `build_prompt.ps1` | assembles `.zagent/run/<KEY>/prompt.md` and `filelist.txt` for step 5 of `/tick`, deterministically. Invoked directly with `pwsh -File`, never through `zagent.ps1` |
| `Test-ZagentClient.ps1` | the assertions over `ZagentClient.psm1`'s surviving helpers, plus a couple of `zagent.project.json` sanity checks that happen to live here |
| `Test-BuildPrompt.ps1` | the assertions over `build_prompt.ps1`, run as a real `pwsh -File` child process per assertion (never dot-sourced), because `exit` inside the script under test would otherwise tear down the test runner too |

Each test script **prints its own count** — do not pin one here, this
line has been stale before:

```
pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1
pwsh -NoProfile -File Tools/zagent/Test-BuildPrompt.ps1
```

The `zagent.ps1`/`ZagentClient.psm1` split exists so the helpers are
reachable from a test: `zagent.ps1` runs a command the moment it is
invoked, which makes every function inside it untestable directly.
Plain assert scripts rather than Pester, matching
`Tools/Test_T0Harness_SessionCloseFlagsExist.ps1` — the only PowerShell
this repo requires is the `pwsh` every gate already needs.

## Lanes

A **lane** is a second, persistent, *registered* git worktree bound to
one board project — so that project's `/tick` queue can run alongside
the main checkout's without two ticks fighting over the same tree. Off
by default. Full protocol (what a step means differently inside a lane):
`Tools/zagent/protocol/TICK.md#lane-mode`. This section is the how-to.

**Enable it.** Hand-edit `zagent.project.json`'s `lanes.enabled` to
`true` and commit — `lanes.enabled` is a protected path, so no tick can
do this to itself; it is a deliberate human decision every time.

**Add a lane.**

```
zagent lane add <PROJECT>        # e.g. zagent lane add ZM
```

This runs `git worktree add --detach <lanes.root>/<PROJECT> <baseBranch>`
(`lanes.root` defaults to `C:/dev/Zenith-lanes`), then
`Tools/lane_init.ps1`, which:

- copies every ignored runtime input the worktree needs but does not
  track — the Slang/VulkanSDK/Sharpmake vendored binaries, and every
  ignored file under each `Games/*/Assets` and `Zenith/Assets` — never
  tracked files, ~14 GB;
- runs `Build\regen.ps1 -AllowLinkedWorktree`;
- builds and unit-gates every game listed in `Tools/unit_baselines.json`,
  requiring `ran == pin` for each.

Expect this to take a while (a full regen + build + gate pass per game)
and to use another ~20-30 GB of disk. The lane is registered in
`<git-common-dir>/vault/lanes.json` **only if all of that passes** — if
`lane_init.ps1` fails partway, the worktree exists on disk but is NOT
registered; either fix the failure and re-run `zagent lane add
<PROJECT>`, or remove the worktree by hand
(`git worktree remove --force <path>`) and start over. `zagent lane
which --json` (below) confirms whether a given directory ended up
registered.

**Per-harness setup, once a lane exists:**

- **Claude Code** — start `claude` in the lane's own directory (a
  genuinely separate `cwd`, not `--add-dir` on the main checkout) and run
  `/loop /tick` there. This is the one harness with a real in-harness
  scheduler, so it can drain that lane's queue unattended. Claude Code
  needs a per-directory trust grant for the lane first: start `claude`
  INTERACTIVELY in the lane directory once
  and accept the workspace-trust dialog — grant it the same way you
  trusted the main checkout, once per lane. Until that happens, Claude
  Code ignores every `permissions.allow` entry in `.claude/settings.json`
  and `.claude/settings.local.json`, so pre-authorised gate and `zagent`
  commands prompt or are refused; a headless `claude -p` launched into an
  untrusted lane behaves the same way (observed).
- **OpenCode** — open the OpenCode desktop app on the lane's directory
  and re-invoke `/tick` there by hand for each firing (`loopMode:
  "manual"` — no in-app scheduler reaches this checkout). If the app has
  its own per-session worktree feature, turn it off so it does not layer
  a second worktree on top of the lane's own (confirm the exact setting
  name in your app version).
- **Codex** — open the Codex desktop app on the lane's directory and
  re-invoke `$tick` there by hand for each firing, same manual pacing as
  OpenCode. Codex needs a per-path trust entry for the lane's directory
  before it will run anything there — grant it the same way you trusted
  the main checkout, once per lane.

**Check what a session is running as.** `zagent lane which --json`
always exits 0 and answers `{mode, project, path, inFlight}` — `mode` is
`main`, `lane`, or `unregistered`. Run it from an unregistered linked
worktree and every mutating command (`claim`, `next`, `check`, `finish`,
`abandon`, `gates`, `guard`) refuses with exit 1; read commands still
work.

**Landing: `zagent integrate <KEY>`.** Only runs from a registered lane —
merges that ticket's `agent/<KEY>` onto `baseBranch` and syncs the main
checkout. `payload.result` names the outcome; the full ladder (identical
to `Tools/zagent/protocol/TICK.md`'s own copy) is in
[`../Vault/spec/command-matrix.md`](../Vault/spec/command-matrix.md)'s
`### integrate` section:

| `result` | exit | what happens next |
| --- | --- | --- |
| `LANDED` | 0 | merged onto `baseBranch`; the ticket finishes Done |
| `REGATE` | 20 | re-merge `master` + re-verify (2 attempts total) |
| `STALE_VERDICT` | 21 | re-verify only, without re-merging (once per firing) |
| `NOT_GREEN` | 22 | Blocked as a failure |
| `CONFLICT` | 23 | Blocked `integration-conflict` |
| `TREE_UNSTABLE` | 24 | Blocked `integration-conflict`, directly |
| `MAIN_DIRTY` | 25 | wait row — see troubleshooting below |
| `MAIN_NOT_ON_BASE` | 26 | wait row — see troubleshooting below |
| `UNAPPROVED` | 27 | re-parked to In Review |
| `LOCKED` / `RACED` | 5 | retried inside the same tick, at most 2 attempts |

**Human-gate approval, end to end.** A `human-gate` ticket in a lane
stops at *In Review* instead of Done, same as main mode. To let it
proceed: review `agent/<KEY>` in the lane's worktree, then run

```
zagent update <KEY> --label approved --human
```

The lane's next `/tick <KEY>` re-claims the ticket, restores its scratch,
lands it via `integrate`, and finishes Done. No agent identity may ever
apply the `approved` label itself — see I7 in `TICK.md`.

**Troubleshooting — one human action per code:**

| Situation | Human action |
| --- | --- |
| `MAIN_DIRTY` | Commit, stash, or finish your own overlapping edits in the main checkout. The lane retries on its next firing, up to `lanes.integrateWaitMinutes` (default 240) since it first waited, then Blocks `integration-conflict` instead. |
| `MAIN_NOT_ON_BASE` | Switch the main checkout back onto `baseBranch` (`git switch master`). |
| Build slot busy (`tick_gates` exit 11) | Another checkout or lane holds the one machine-wide build slot (`Tools/build_lock.ps1`, `Local\ZenithBuildSlot`) — see `<git-common-dir>/zenith-build-slot.json` for who, and either wait or investigate if it looks stuck. |
| Tree unstable (`tick_gates` exit 12) | A gate writes non-deterministic tracked output across two verify passes — fix the gate; re-running and hoping only wastes the retry `tick_gates` already spent. |
| `CONFLICT` / label `integration-conflict` | The ticket's own edit collided with a board file `master` also changed. Resolve it by hand on `agent/<KEY>` in the lane's worktree, then re-run `/tick <KEY>`. |
| `RECOVERY_CONFLICT` | An external change raced the Vault's own crash recovery. Inspect the journal named in the error under `.git/vault/operations/` and resolve by hand. |

**Disabling.** Remove every registered lane first —
`zagent lane add`/`zagent lane remove` both refuse outright while
`lanes.enabled` is `false`, so there is no way to tear one down after
disabling:

```
zagent lane remove <PROJECT>      # for every lane in lanes.json
```

Then hand-edit `lanes.enabled` back to `false` and commit.

**Disk hygiene.** `zagent lane remove <PROJECT>` refuses if the lane's
worktree has uncommitted changes, or unlanded `agent/<PROJECT>-*`
branches — land or discard them first. Once it proceeds, it purges the
lane's own `Build/output`, `Build/obj` and `Build/artifacts` (root and
per-game) before removing the git worktree, so a removed lane does not
leave its build output behind.

## Sharp edges

- **`zagent.cmd` must keep CRLF line endings.** A Windows batch file
  with LF-only endings is outside what cmd.exe's parser guarantees, and
  the symptom is a bare `zagent …` through the shim that never returns —
  observed as the SECOND shim invocation within one console session
  hanging past five minutes, every time, while the same script through
  `pwsh -File` directly never hung once. It reached disk with LF bytes
  under `core.autocrlf=true` while still looking "clean" to git.
  `.gitattributes` pins `Tools/zagent/zagent.cmd text eol=crlf`, so every
  checkout puts CRLF on disk regardless of the cloner's setting.
- **A separator mismatch in `ConvertTo-PosixPath` matches nothing.** The
  docs mirror and the board key an uploaded tree off these strings, so a
  value that normalises differently on a second pass silently mirrors
  zero pages and reports success. `Test-ZagentClient.ps1` pins this.
- **`Find-ClientRepo` reads the cwd, not `$PSScriptRoot`.** It walks up
  looking for `zagent.project.json`, so `zagent owns ZEN-11` from
  `Games/Zenithmon` finds the same file as one run from the repo root.
  `zagent.ps1`'s own `--vault` root is computed differently (from its
  own location, not the cwd) — see `zagent.ps1` above — because the
  Vault client's location relative to the repo is a fixed fact, not one
  that depends on where the caller happens to be standing.
