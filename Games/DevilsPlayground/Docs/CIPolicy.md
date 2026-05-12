# Devil's Playground -- CI Policy

**Document purpose:** Records the branch-protection ruleset on `master`, the required status checks, and the policy for changing them. Authored 2026-05-12 for MVP-0.0.6.

---

## Branch protection on `master`

Set via `gh api repos/tomosh22/Zenith/branches/master/protection -X PUT` (token scope: `repo`).

| Setting | Value |
|---|---|
| Required status checks (strict) | `dp-build`, `complexity-gate` |
| `strict` (up-to-date branch required) | true |
| Require linear history | true |
| Enforce on administrators | **false** (Tomos can bypass for emergencies) |
| Allow force push | false |
| Allow branch deletion | false |
| Required PR reviews | not enforced |
| Required conversation resolution | not enforced |
| Required signed commits | not enforced |

### Why these specific settings

- **`dp-build`** is the canonical compile-link gate for the DevilsPlayground project ([dp-pr.yml](../../../.github/workflows/dp-pr.yml)). Any PR that breaks the DP build fails this check.
- **`complexity-gate`** is the engine-wide complexity ceiling check ([complexity.yml](../../../.github/workflows/complexity.yml)). Any PR that adds a function past the [Tools/complexity_profiles.json `engine-ci` thresholds](../../../Tools/complexity_profiles.json) fails this check.
- **`dp-tests` is NOT a required check.** The `dp-tests.yml` workflow is a `workflow_dispatch`-only skeleton today; it cannot run on free GitHub-hosted runners because the engine needs a Vulkan device that those runners don't have. See [Questions.md Q-2026-05-12-007](Questions.md) for the resolution paths. **When dp-tests reactivates, this policy must be updated** to add it to the required-checks list.
- **`strict` (up-to-date branch)** forces PRs to be rebased against the latest master before merge -- prevents "you tested against stale master, your green CI doesn't mean it works against current master" regressions.
- **`enforce_admins=false`** lets Tomos bypass the gate manually for emergencies (e.g. an urgent build fix when CI itself is broken). The autonomy loop never has admin override -- so the gate is real for agent PRs.
- **`required_linear_history`** matches the `gh pr merge --squash` convention used by every autonomous-agent PR so far. Squash-merge produces linear history by construction.

## How to update this policy

1. **Add a new required check** (e.g. when `dp-tests` reactivates):
   ```powershell
   $payload = @{
       required_status_checks = @{
           strict = $true
           contexts = @('dp-build', 'complexity-gate', 'dp-tests')
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

`gh pr merge --auto --squash --delete-branch` queues a merge that fires automatically when all required status checks are green and other branch-protection requirements are satisfied. Before MVP-0.0.6 the repo had no required checks, so `--auto` fired immediately; PRs queued faster than CI completed. With this policy, `--auto` now WAITS for `dp-build` + `complexity-gate` to be green.

Direct `gh pr merge --squash` (no `--auto`) is rejected by the API if the required checks aren't passing. Use `--auto` from now on for autonomous-agent PRs.

## Why `dp-tests` was excluded

[Q-2026-05-12-007](Questions.md) is the canonical record. Summary: free GitHub windows runners have no GPU; the DP engine calls `vkEnumeratePhysicalDevices` at boot and deadlocks if no devices are found; reactivation requires one of (a) paid GPU runner [vetoed], (b) Mesa lavapipe + engine code changes, (c) an engine `--no-graphics` boot mode, or (d) a self-hosted GPU runner. Until one of those lands, requiring `dp-tests` would block every PR.

The autonomy loop accepts this gap: every PR auto-merges with build-only validation. Local `Tools/run_dp_tests.ps1` execution is the only test gate. Add a `dp-tests` requirement immediately when the gap closes.
