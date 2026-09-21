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
| The `/tick` protocol | `.claude/commands/tick.md` | It is this repo's operating procedure. |
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
