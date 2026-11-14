# PROC design — the human procedures for the landing and the seed

Companion to `Zenith2/Docs/Design/PROC.md`. This file gives the exact human procedures for the docs-and-config commit (A8a), the journal archival before filing, the 50-ticket dry run and the filing (A6), the board seed commit (A8b), the coverage commit (A8c) and the steps after the seed, each with its preconditions, its commands and its rollback (Z2-D-069, Z2-D-115). Every command runs from `C:\dev\Zenith` (the main checkout) in PowerShell 7 unless a step says otherwise; `$Z` below stands for `pwsh -NoProfile -File Tools\zagent\zagent.ps1`, written out in full where a step is copied verbatim.

## Scope of this file

Exactly three commits land on `master` for the documentation programme (Z2-D-115): A8a, A8b and A8c. The ZEN-114 to ZEN-119 commits land before A8a and are not part of the three. No cross-project link is ever made: Z2-D-112 is settled and its alternative of links added after the seed is rejected (ruling PROC-R1 (1)). Nothing in this file is run by `/tick`.

## Order of the landing

| # | Step | Commit | Lanes |
|---|---|---|---|
| 1 | ZEN-114 to ZEN-118 run by the loop; ZEN-119 committed and closed by the human ("Before A8a") | ZEN commits | may run |
| 2 | the pre-A8a checks ("Before A8a") | none | idle from here |
| 3 | A8a | docs and config | idle |
| 4 | journal archival | none (`.git/` only) | idle |
| 5 | A6 dry run on a scratch branch, then the branch deleted | none on `master` | idle |
| 6 | A6 filing on `z2-board-seed` | commits on the seed branch only | idle |
| 7 | A8b | `board: seed Z2 backlog` | idle |
| 8 | A8c | `Zenith2/Docs/Coverage.md` | idle |
| 9 | after the seed: delete the seed branch, resume the lanes | none | resume |
| 10 | start Stage 0, once ZEN-114 to ZEN-119 are Done | the sprint-start board write | may run |

The human's rulings Q5 (a) and Q6 (b) of 2026-10-16 require ZEN-114 to ZEN-119 to be Done before any Zenith2 work (Z2-R-PROC-001). That gate is enforced in three places, none of them a link:
1. `Zenith2/Docs/Roadmap.md` → "Stage 0 entry criteria" states it (the CROSS_DOC row of reconciliation R8).
2. The human's `zagent sprint start` for Stage 0 is the act that honours it. Until then no Z2 ticket is claimable: the A6 filing verifies every row `claimable:false`, and every Stage 0 PROC ticket is `needs-human` and behind Z2T-PROC-001.
3. The first Stage 0 ticket, Z2T-PROC-001, re-checks it mechanically as the first item of its Definition of Done: six read-only `zagent show ZEN-<n> --json` probes, each required to report `status: Done`.

## Before A8a

1. **The ZEN tooling tickets.** ZEN-114 to ZEN-118 are ordinary ZEN tickets and run through the loop. ZEN-119 is `needs-human`; its edits are already in the main working tree (the protocol, reviewer-charter and generator files, the regenerated harness files and `Games/Hearth/Docs/Board.md`). The human:
   1. reruns `pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1 -Harness claude -Check`, then `-Harness opencode -Check` and `-Harness codex -Check` (each exit 0), `pwsh -NoProfile -File Tools/zagent/Test-HarnessParity.ps1` and `pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1`;
   2. commits exactly those files (`git add` of the paths `git status --porcelain` lists outside `Zenith2/`; nothing under `Zenith2/`), message `ZEN-119: correct stale tick-protocol and reviewer-charter statements; regenerate harnesses` with the repository's attribution trailer;
   3. closes it: `pwsh -NoProfile -File Tools\zagent\zagent.ps1 move ZEN-119 "In Progress" --human`, then `pwsh -NoProfile -File Tools\zagent\zagent.ps1 finish ZEN-119 --status Done --human --comment <result.md> --worklog <worklog.md>`.
   ZEN-117 (create refuses at file time) should be Done before the A6 dry run, so the dry run exercises the same `create` code as the filing.
2. **The tree.** `git status --porcelain --untracked-files=all` lists only `?? Zenith2/…` paths (the A1 apply refuses anything else; `Dawnmere.zscen` must be untouched).
3. **Idle.** No `/tick` or `/loop` session is open in the main checkout or in either lane (`C:\dev\Zenith-lanes\ZEN`, `C:\dev\Zenith-lanes\ZM`), and the other /tick session has been told; `pwsh -NoProfile -File Tools\zagent\zagent.ps1 lane list --json` shows no lane with an `inFlight` key.
4. **The documents.** `node C:/dev/Zenith2-drafts/checker/check.mjs docs --repo C:/dev/Zenith` reports no error; its `grep-guard` rule is the check of Z2-D-100 over every tracked Zenith2 text.
5. **The A1 package.** `node C:/dev/Zenith2-drafts/a1/apply.mjs --dry-run --with-metadata` passes every precondition and prints the plan.
6. **The orchestrator's pre-A8a edits of unprotected files**, each checked by reading the file:
   - `Zenith2/CLAUDE.md`'s layout line names `Authoring`, `EditorCore`, `Maintenance` and `Security` under `Tools/` (EDIT-P1, row X387, SEC). The file becomes protected at Z2T-PROC-001, after which Z2T-PROC-008 makes the edit instead.
   - `Zenith2/.gitattributes` holds the `zenith-research` block of RESEARCH's extended P-RS4, in this order (row RS12):

     ```
     # zenith-research
     Docs/Research/** text eol=lf
     Docs/Research/**/archive/** filter=lfs diff=lfs merge=lfs -text
     ```

     The lane-side half of P-RS4 (`lfs.fetchexclude`) is set by `Provision-Lane.ps1`. The research guardrail globs of P-RS6 land with Z2T-PROC-001, not at A8a.
   - The `Zenith2Memory` paths of the A1 `zagent.project.json` hand-over equal the union of `sanitizer_paths.json` as `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection" lists it (cross-area note PG1 (a)).

## A8a docs and config commit

Preconditions: "Before A8a" complete. Commands:

1. `node C:/dev/Zenith2-drafts/a1/apply.mjs --yes --with-metadata` (never `--with-optional`: the delegation sentence lands with Z2T-PROC-006). The apply writes its backup under `C:/dev/Zenith2-drafts/a1/backup/<ts>/` and the board files, `metadata.md`, `zagent.project.json` and `vault.config.json` last.
2. `pwsh -NoProfile -File C:/dev/Zenith2-drafts/a1/verify.ps1` — every check PASS (doctor ok with Z2 in `projects`; `version list --project Z2` has eight rows; `gates` of the recorded ZEN and ZM sets unchanged; the blindness grep matches only `vault.config.json` and `zagent.project.json`; `Zenith2/out/` ignored; the board `-text`).
3. `pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1` and `pwsh -NoProfile -File Tools/zagent/Test-HarnessParity.ps1` pass.
4. `git add Zenith2 vault.config.json zagent.project.json metadata.md`, then `git status --porcelain` must show no path outside those and none under `Zenith2/Docs/Board/tickets/` (none exists yet).
5. `git commit` with the message `Zenith2: documentation, board registration and categories (A8a)` and the attribution trailer.
6. The `metadata.md` `jira.projects` row (Z2-D-088) is the one line `apply.mjs --with-metadata` inserts; the human's ruling Q1 (a) approved it, and the orchestrator performs it under the ruling PROTECTED-EDITS-BY-ORCHESTRATOR; `git diff HEAD~1 -- metadata.md` shows exactly one changed line.

Verification after the commit: `pwsh -NoProfile -File Tools\zagent\zagent.ps1 doctor --json` `ok: true`; `pwsh -NoProfile -File Tools\zagent\zagent.ps1 queue --project Z2 --json` exits 3 (no Z2 ticket exists yet).

Rollback (before any further commit): `node C:/dev/Zenith2-drafts/a1/rollback.mjs --dry-run`, then `--yes`, which restores the four files from the backup, then `git reset --soft HEAD~1` followed by `git restore --staged .` keeps the documentation untracked again; after a subsequent commit, revert instead: `git revert --no-edit <A8a sha>` with every lane idle. A `doctor` that is not ok after the apply means roll back at once, because every board reads the configuration.

## Journal archival before A6

The human's ruling Q2 (a) of 2026-10-16 (Z2-D-110): archive the complete Vault journals once, by hand, so each filing write stops re-reading them.

Preconditions: A8a landed; every lane and session idle; then journal recovery is run once and must succeed:

```powershell
pwsh -NoProfile -File Tools\zagent\zagent.ps1 check ZEN-114 --json
```

Exit 0 or 4 is fine; a `BRANCH_MOVED` or `RECOVERY_CONFLICT` error stops the procedure.

Archive:

```powershell
$ops = Join-Path (git rev-parse --git-common-dir) 'vault\operations'
$dst = Join-Path (git rev-parse --git-common-dir) ("vault\operations-archive-" + (Get-Date -Format 'yyyyMMdd'))
New-Item -ItemType Directory -Force $dst | Out-Null
$moved = 0; $kept = 0
Get-ChildItem $ops -Filter *.json | ForEach-Object {
  $j = Get-Content $_.FullName -Raw | ConvertFrom-Json
  if ($j.phase -eq 'complete') { Move-Item $_.FullName $dst; $moved++ } else { $kept++ }
}
"moved=$moved kept=$kept"
```

Verify: `(Get-ChildItem $ops -Filter *.json).Count` equals `kept` (normally 0); `pwsh -NoProfile -File Tools\zagent\zagent.ps1 doctor --json` is `ok: true`; `pwsh -NoProfile -File Tools\zagent\zagent.ps1 check ZEN-114 --json` exits 0 or 4, faster than before.

Recovery reads only `operations`, so archived complete journals cannot affect it; what is lost is `--operation-id` replay of an archived operation, harmless for completed work. Rollback: `Move-Item (Join-Path $dst '*.json') $ops`. Nothing tracked changes; keep the archive until A8b and A8c have landed.

## A6 dry run

Measures the filing rate after the archival and proves the squash path end to end (Z2-D-110).

1. Preconditions: every lane idle; `master` clean; the session checker's full drafts run is clean (`node C:/dev/Zenith2-drafts/checker/check.mjs drafts --drafts C:/dev/Zenith2-drafts/tickets --areas C:/dev/Zenith2-drafts/areas --repo C:/dev/Zenith --out C:/dev/Zenith2-drafts/checker/out/drafts`, exit 0) and its report is less than one hour old. Two session-tool changes, owned by the orchestrator (ruling PROC-R1, F11), must be in place:
   - The session filer and the checker's body module emit the trailer lines in the production order of `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "The managed region": `Requirements:`, `Decisions:`, `Open-Decisions:` (when non-empty), `Kind:` (every non-epic ticket), `Managed-Fields:`, `Draft-ID:`, `Spec-Maturity:`, `Cites:` and `Spec-Hash:`. The production coverage step fails a non-epic ticket without `Kind:` (`CV-KIND-MISSING`), and reconcile changes only the fields `Managed-Fields:` names.
   - The checker's drafts mode carries the rule `kind-trailer`. RESEARCH withdrew P-RS3's `decision-template` drafts rule (row RS12): the board-side check is the production `Coverage` step's decision-template run.

   Verification: the drafts run above is clean with both rules active, and the human opens three filed bodies of the dry run (one `implementation`, one `decision`, one epic) and checks the trailer order by eye.
2. `git switch -c z2-dryrun` in the main checkout.
3. `node C:/dev/Zenith2-drafts/filer/file.mjs --checked C:/dev/Zenith2-drafts/checker/out/drafts/report.drafts.json --limit 50 --gate-any --branch z2-dryrun`.
4. Read `C:/dev/Zenith2-drafts/filer/state/timing.json`: seconds per operation (mean, p50, p95) and the fit against the ticket count; the projected window for the full backlog must fit within the lanes' idle window the human accepts.
5. `git diff master...z2-dryrun --name-only` touches only `Zenith2/Docs/Board/**`.
6. Exercise the squash: `git switch master`, `git merge --squash z2-dryrun`, inspect `git status`, then discard it: `git reset --hard HEAD` (the index holds only the squashed board files; nothing else is lost because the tree was clean).
7. Delete the scratch branch only after an authoritative command ran on it: `git switch z2-dryrun`, `pwsh -NoProfile -File Tools\zagent\zagent.ps1 check Z2-1 --json` (exit 0 or 4; recovery completes), `git switch master`, `git branch -D z2-dryrun`.
8. Clear the filer state of the dry run (`C:/dev/Zenith2-drafts/filer/state/keys.json` and `keys.jsonl` moved aside), so the real filing starts at `Z2-1`.

## A6 filing

1. Preconditions: as the dry run, plus the archival done and the dry-run state cleared.
2. `git switch -c z2-board-seed` from `master` at A8a.
3. `node C:/dev/Zenith2-drafts/filer/file.mjs --checked C:/dev/Zenith2-drafts/checker/out/drafts/report.drafts.json --branch z2-board-seed`.
4. On a stop (exit 1): fix the named cause and re-run the same command with a fresh `--checked` report; the filer is keyed on Draft-ID and resumes. A journal left mid-operation is recovered first with `pwsh -NoProfile -File Tools\zagent\zagent.ps1 check Z2-1 --json` (Z2-D-095).
5. Verify on the seed branch: `pwsh -NoProfile -File Tools\zagent\zagent.ps1 doctor --json` ok; `node C:/dev/Zenith2-drafts/checker/check.mjs filed --repo C:/dev/Zenith --map C:/dev/Zenith2-drafts/filer/state/keys.json --drafts C:/dev/Zenith2-drafts/tickets --areas C:/dev/Zenith2-drafts/areas --out C:/dev/Zenith2-drafts/checker/out/filed` exit 0; `git diff master...z2-board-seed --name-only` touches only `Zenith2/Docs/Board/**` (Z2-D-101: mandatory); `pwsh -NoProfile -File Tools\zagent\zagent.ps1 queue --project Z2 --json` lists every row `claimable:false`; every sprint PLANNED.

Abandoning: never delete the seed branch first. `git switch z2-board-seed`, `pwsh -NoProfile -File Tools\zagent\zagent.ps1 check Z2-1 --json` so recovery completes or surfaces `RECOVERY_CONFLICT`, then `git switch master` and `git branch -D z2-board-seed`. `master` never holds a half-filed board.

## A8b seed commit

1. `git switch master`.
2. `git merge --squash z2-board-seed`.
3. `git diff --cached --name-only` lists only `Zenith2/Docs/Board/**`.
4. Commit with the subject `board: seed Z2 backlog`, a blank line, and the body line `Vault-Operation: z2-seed-<YYYYMMDD>` at the start of a line (the board-only classification reads exactly that, Z2-D-101), then the attribution trailer.
5. `pwsh -NoProfile -File Tools\zagent\zagent.ps1 doctor --json` ok on `master`.

Rollback before A8c: `git reset --hard HEAD~1` (the board-only commit is the tip and nothing else changed). After A8c or any board write: `git revert --no-edit <A8b sha>` with every lane idle, committed with a `board: revert Z2 seed` subject and a `Vault-Operation:` line, then repeat A6.

## A8c coverage commit

1. Generate `Zenith2/Docs/Coverage.md` with the real keys: `node C:/dev/Zenith2-drafts/checker/check.mjs filed --repo C:/dev/Zenith --map C:/dev/Zenith2-drafts/filer/state/keys.json --drafts C:/dev/Zenith2-drafts/tickets --areas C:/dev/Zenith2-drafts/areas --out C:/dev/Zenith2-drafts/checker/out/filed`, then copy its `Coverage.md` to `Zenith2/Docs/Coverage.md`; the file is in the format of `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage.md format", so the production coverage step regenerates it byte for byte.
2. Regenerate the normative catalogue with `node C:/dev/Zenith2-drafts/checker/check.mjs docs --repo C:/dev/Zenith --write-normative` when its tracked copy changed.
3. `git add Zenith2/Docs/Coverage.md Zenith2/Docs/Requirements/Normative.md`; commit `Zenith2: requirement coverage report (A8c)`. It is not board-only, so it lands before any lane resumes.

Rollback: `git revert --no-edit <A8c sha>` and regenerate.

## After the seed

1. Delete the seed branch: `git switch z2-board-seed`, `pwsh -NoProfile -File Tools\zagent\zagent.ps1 check Z2-1 --json`, `git switch master`, `git branch -D z2-board-seed`.
2. Resume the lanes and tell the other /tick session.
3. Stage 0 starts only when ZEN-114 to ZEN-119 are Done. The human runs this probe, which is read-only and fails closed. A non-zero exit of any `show`, including exit 3 for a missing ticket, an `error` member, a missing `status`, or any status other than `Done` stops the start:

   ```powershell
   foreach ($n in 114..119) {
     $out = pwsh -NoProfile -File Tools\zagent\zagent.ps1 show "ZEN-$n" --json
     if ($LASTEXITCODE -ne 0) { "ZEN-${n}: show exited $LASTEXITCODE"; exit 1 }
     $j = $out | ConvertFrom-Json
     if ($j.PSObject.Properties['error'] -or -not $j.PSObject.Properties['status'] -or $j.status -ne 'Done') { "ZEN-${n}: not Done"; exit 1 }
   }
   'ZEN-114..119 Done'
   ```

   The same probe is the first Definition-of-Done item of Z2T-PROC-001. When it prints `ZEN-114..119 Done`, the human runs `pwsh -NoProfile -File Tools\zagent\zagent.ps1 sprint start "Stage 0 — Tooling bring-up" --project Z2`, then works the PROC tickets from Z2T-PROC-001 in a human-driven session. Z2T-PROC-001 repeats the six probes as its first DoD item, so a ZEN ticket reopened in between still stops Stage 0.

## Rollback

| Step | Rollback | Never |
|---|---|---|
| A8a | `rollback.mjs --yes`, then `git reset --soft HEAD~1`, or `git revert` once anything followed | edit `vault.config.json` by hand while lanes run |
| archival | move the archived journals back | delete the archive before A8b and A8c |
| dry run | `check Z2-1` on the branch, then delete it | delete the branch before recovery ran on it |
| filing | resume, or abandon as "A6 filing" states | switch to `master` with a journal mid-operation |
| A8b | `git reset --hard HEAD~1` while it is the tip, `git revert` afterwards | rewrite `master` history once a lane fired |
| A8c | `git revert` and regenerate | edit `Coverage.md` by hand |
| sprint start | `zagent sprint` cannot un-start a sprint: the human leaves every Stage 0 ticket unclaimed until the failed ZEN ticket is Done again, which Z2T-PROC-001's probe enforces | start Stage 0 while any of ZEN-114 to ZEN-119 is not Done; link a ZEN ticket to a Z2 ticket (Z2-D-112) |
