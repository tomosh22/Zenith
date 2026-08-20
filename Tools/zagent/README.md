# `zagent` — the agent board's client

The autonomous loop's board is a web application. It may run on **an
entirely different machine**, and this repo does not know where its
source lives — there is deliberately no setting for that.

This directory is the whole client: a PowerShell script that speaks HTTP
to the board, plus a `.cmd` shim so `zagent …` works bare. No Node, no
`node_modules`, no package manager. `pwsh` is already a hard dependency
of every gate and every `zenith.ps1` command, so the client adds nothing
new to a build machine.

## Setup

Two environment variables, once per machine:

```powershell
[Environment]::SetEnvironmentVariable('ZAGENT_URL',   'https://board.example.com', 'User')
[Environment]::SetEnvironmentVariable('ZAGENT_TOKEN', 'zag_…',                     'User')
```

Put `Tools\zagent` on `PATH` so a bare `zagent` resolves. **Prepend it, do
not append it** — `PATH` is ordered, and a machine that once had an older
`zagent` installed (a pnpm/npm global shim, say) still has that shim
earlier in the list. Appending leaves the stale one winning, and the
symptom is not an error: the old client answers, talks straight to the
database instead of over HTTP, and `zagent doctor` still exits 0 while
silently omitting every client-side row.

```powershell
$path = [Environment]::GetEnvironmentVariable('PATH', 'User')
[Environment]::SetEnvironmentVariable('PATH', "C:\dev\Zenith\Tools\zagent;$path", 'User')
```

Confirm the right one won — this should print a path inside this repo:

```powershell
(Get-Command zagent).Source
```

The token is minted **on the board machine**, which is the only place
that can reach the database without already holding one:

```
zagent auth mint --name <this-machine>
```

It is shown exactly once — only its SHA-256 is stored — and revoking it
is `zagent auth revoke <name>` on the board. Per-machine, so a laptop
that walks off is one row, not a secret rotation.

Verify with `zagent doctor`, which merges the board's checks with this
machine's.

## What lives where

| | Where | Why |
|---|---|---|
| Gates, pinned baselines, conventions, categories | `zagent.project.json`, repo root, **committed** | A pinned unit count already lives in `Status.md`, `zm-tests.yml` and `run_unit_gate.ps1`. A fourth copy on a machine you cannot see could only be remembered. |
| The `/tick` protocol | `.claude/commands/tick.md` | It is this repo's operating procedure. |
| Run scratch | `.zagent/`, **gitignored** | The tick treats a dirty tree as fatal, so `git status --porcelain` must never see it. |
| The agent account, model routing, guardrail floors | the board | Policy, not topology. |

`zagent.project.json` is **sent with every request** rather than
registered once. A stored copy goes stale, and a stale gate list is
exactly how a green run ratchets the wrong baseline.

## Exit codes are the interface

Branch on them, never on the text:

| | |
|---|---|
| `0` | ok |
| `3` | nothing to claim |
| `4` | contract invalid |
| `5` | ownership lost / repo busy |
| `6` | circuit breaker open |
| `1` | error |
| `7` | **could not reach the board** |

`7` is this client's own and is distinct on purpose. "The board said no"
and "I never reached the board" demand opposite responses from an
unattended loop, and collapsing them into `1` is how a network blip gets
written into a ticket as a contract failure.

## Layout, and running its tests

| File | |
|---|---|
| `zagent.ps1` | argv, environment, transport, main flow |
| `ZagentClient.psm1` | the pure helpers — path resolution, JSON shaping, payload assembly, applying the board's writes, the client half of `doctor` |
| `Test-ZagentClient.ps1` | the assertions over the module — it prints its own count, which is why one is not pinned here |

```
pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1
```

The split exists so the helpers are reachable from a test: `zagent.ps1`
runs a command the moment it is invoked, which makes every function
inside it untestable. A plain assert script rather than Pester, matching
`Tools/Test_T0Harness_SessionCloseFlagsExist.ps1` — the only PowerShell
this repo requires is the `pwsh` every gate already needs.

## Sharp edges

- **`ConvertTo-Json` defaults to `-Depth 2`.** Anything deeper is
  serialised as a type name. Every call here passes `-Depth 100`.
- **A PowerShell function unrolls a returned collection**, so a
  one-element array comes back as a scalar and `cleanup: ["…"]` goes on
  the wire as a bare string. `Remove-Annotations` returns `,@(…)` for
  exactly this reason; the board's strict schema catches it, but the
  error ("Expected array, received string") does not point at the
  language feature that caused it.
- **`$`-prefixed keys are comments** in `zagent.project.json`, stripped
  at every depth. JSON has no comments and that file pins baselines —
  the numbers most in need of a note beside them.
- **`Get-ClientChecks` returns ONE pipeline item on purpose.** `,@(…)`
  keeps `.Count` honest for a single-row result — a bare hashtable would
  answer with its KEY count — at the cost that `| ForEach-Object` sees
  the whole array. Assign it and iterate.
- **`Set-StrictMode` turns a missing property into a throw**, so every
  optional field in the project file is read through
  `PSObject.Properties['name']`. A file that legally omits `baseBranch`
  must make `doctor` report, not crash.
