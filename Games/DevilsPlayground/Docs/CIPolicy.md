# Devil's Playground -- CI Policy

**Document purpose:** Records the branch-protection ruleset on `master`, the required status checks, and the policy for changing them. Authored 2026-05-12 for MVP-0.0.6. End-to-end gate verified by the MVP-0.0.7 smoke PR (this comment is the smoke change).

---

## Branch protection on `master`

Set via `gh api repos/tomosh22/Zenith/branches/master/protection -X PUT` (token scope: `repo`).

| Setting | Value |
|---|---|
| Required status checks (strict) | `complexity-gate`, `dp-tests` (`dp-build` removed 2026-06-07 — see below) |
| `strict` (up-to-date branch required) | true |
| Require linear history | true |
| Enforce on administrators | **false** (Tomos can bypass for emergencies) |
| Allow force push | false |
| Allow branch deletion | false |
| Required PR reviews | not enforced |
| Required conversation resolution | not enforced |
| Required signed commits | not enforced |

### Why these specific settings

- **`dp-build`** *(removed 2026-06-07, commit `45deaa4a`)* was the standalone DevilsPlayground compile-link gate. It was dropped as redundant: [dp-tests.yml](../../../.github/workflows/dp-tests.yml) builds the identical `vs2022_Debug_Win64_True` config before running the suite, so any DP build break already fails `dp-tests`. The DP build is now gated by `dp-tests` alone.
- **`complexity-gate`** is the engine-wide complexity ceiling check ([complexity.yml](../../../.github/workflows/complexity.yml)). Any PR that adds a function past the [Tools/complexity_profiles.json `engine-ci` thresholds](../../../Tools/complexity_profiles.json) fails this check.
- **`dp-tests`** ([dp-tests.yml](../../../.github/workflows/dp-tests.yml)) is the headless DP test suite. Re-added as a required check 2026-05-13 (second attempt) after PR #14's `SET_MODEL_MATERIAL` softening empirically resolved the CI asset-gap concern: on a fresh GPU-less runner with no `.zmodel` files, the engine `--headless` mode + the `m_bRequiresGraphics` skip-list combine to give **36 passed, 0 failed** (24 actual pass + 12 skipped). The first re-add attempt earlier 2026-05-13 was reverted because the SET_MODEL_MATERIAL hard assert had crashed authoring before any test ran; with that commit in master, dp-tests is reliable as a gate. Tests that genuinely require a GPU or mesh data (fog passes, materials, full playthrough, double-door, forge, villager-tuning migration -- 12 total) are tagged `m_bRequiresGraphics=true`; the harness emits `"skipped": true` for them and skip counts as pass. CI artefact: `dp-test-results` (per-test JSON).
- **`strict` (up-to-date branch)** forces PRs to be rebased against the latest master before merge -- prevents "you tested against stale master, your green CI doesn't mean it works against current master" regressions.
- **`enforce_admins=false`** lets Tomos bypass the gate manually for emergencies (e.g. an urgent build fix when CI itself is broken). The autonomy loop never has admin override -- so the gate is real for agent PRs.
- **`required_linear_history`** matches the `gh pr merge --squash` convention used by every autonomous-agent PR so far. Squash-merge produces linear history by construction.

## How to update this policy

1. **Add a new required check** (e.g. when `dp-tests` reactivates):
   ```powershell
   $payload = @{
       required_status_checks = @{
           strict = $true
           contexts = @('complexity-gate', 'dp-tests')
       }
       enforce_admins = $false
       required_pull_request_reviews = $null
       restrictions = $null
       required_linear_history = $true
       allow_force_pushes = $false
       allow_deletions = $false
   } | ConvertTo-Json -Depth 5
   gh api -X PUT 'repos/tomosh22/Zenith/branches/master/protection' --input - <<< $payload
   ```

2. **Drop a required check** (e.g. retiring `complexity-gate` if engine refactors flatten the ceiling): same command, drop the entry from `contexts`.

3. **Loosen for an emergency** (e.g. urgent hotfix when CI is unhealthy): temporarily delete protection via
   ```
   gh api -X DELETE 'repos/tomosh22/Zenith/branches/master/protection'
   ```
   then re-apply via the PUT above when the emergency clears. Log the gap in `DecisionLog.md`.

## How auto-merge interacts with this policy

`gh pr merge --auto --squash --delete-branch` queues a merge that fires automatically when all required status checks are green and other branch-protection requirements are satisfied. Before MVP-0.0.6 the repo had no required checks, so `--auto` fired immediately; PRs queued faster than CI completed. With this policy, `--auto` now WAITS for `complexity-gate` + `dp-tests` to be green.

Direct `gh pr merge --squash` (no `--auto`) is rejected by the API if the required checks aren't passing. Use `--auto` from now on for autonomous-agent PRs.

## Why `dp-tests` was excluded

[Q-2026-05-12-007](Questions.md) is the canonical record. Summary: free GitHub windows runners have no GPU; the DP engine calls `vkEnumeratePhysicalDevices` at boot and deadlocks if no devices are found; reactivation requires one of (a) paid GPU runner [vetoed], (b) Mesa lavapipe + engine code changes, (c) an engine `--no-graphics` boot mode, or (d) a self-hosted GPU runner. Until one of those lands, requiring `dp-tests` would block every PR.

The autonomy loop accepts this gap: every PR auto-merges with build-only validation. Local `Tools/run_dp_tests.ps1` execution is the only test gate. Add a `dp-tests` requirement immediately when the gap closes.
