# Zenithmon -- One-Time Human Setup Checklist

**Document purpose:** The one-time pre-flight for the Zenithmon project. Every
item below needs either a repo-administration action, local software
installation, or a local interactive run -- none of which happen as a side
effect of ordinary development, so each needs deliberately doing ONCE.
(The original text said "none can be completed by an agent"; the Verified-by
column contradicts it -- every row was in fact closed by a user-directed Claude
session. What is genuinely user-only is the DECISION to change repo
administration, not the mechanics.) Sessions check this file at start; if a box
that gates the current work is unticked, stop and surface it in Questions.md.

**Usage:** Tick the box and fill in the "verified by / date" column as each
item is completed.

**Companion docs:** BuildEnvironment.md (versions + commands referenced
below), CIPolicy.md (what zm-tests is and why it must become a required
check).

**Last updated:** 2026-08-01 (doc audit at `c9d64994`). Section A item 2
RE-VERIFIED LIVE against the GitHub API on that date -- `required_status_checks
.contexts == ["zm-tests"]`, `strict=false`, `enforce_admins=false`, no required
reviews -- exactly as recorded. Section B re-verified on this machine (Vulkan
SDK `1.4.313.1`, `Middleware\slang\bin\slang.dll` present, git 2.46.2, .NET
runtime without SDK). Section C's first row was corrected: its
`FrontEnd.zscen` premise has been false since ZM-D-148, and a Null-config row
was added. Original entry: 2026-07-10 (S0 close -- ALL items verified).

---

## A. GitHub repository configuration

| Done | Item | How to verify / do it | Verified by / date |
|---|---|---|---|
| [x] | **GitHub Actions enabled** on the repo. The pre-existing gates (dp-tests, cb-tests, complexity-gate, layering-gate, memory-gate, engine-gate, shader-validation, doc-lint -- see CIPolicy.md section 3) should already be running on PRs, which proves this. | Repo -> Actions tab shows recent workflow runs; or Settings -> Actions -> General -> "Allow all actions" | Claude session / 2026-07-10 (all gates ran on PRs #143/#144) |
| [x] | **Add `zm-tests` to master branch-protection required checks** -- AFTER the first green zm-tests run. NOTE (2026-07-10): master had NO branch protection and no rulesets at all, so classic protection was CREATED via the API (user-directed): required contexts `[zm-tests]`, `strict=false`, `enforce_admins=false` (owner direct pushes bypass -- the repo's established workflow). | `gh api repos/tomosh22/Zenith/branches/master/protection` lists `zm-tests` under required_status_checks.contexts | Claude session (user-directed) / 2026-07-10 |

Historical ordering note: item 2 was deliberately completed after the first
green PR run -- setting a required check before the check name existed would
have deadlocked non-admin merges.

## B. Local development environment (per BuildEnvironment.md section 1)

| Done | Item | How to verify | Verified by / date |
|---|---|---|---|
| [x] | **Visual Studio 2022 toolset** installed with the "Desktop development with C++" workload (this machine uses the 18/Insiders MSBuild layout -- either works). | `vswhere.exe -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath` returns a path | Claude session / 2026-07-10 (all S0 builds via 18/Insiders MSBuild) |
| [x] | **Vulkan SDK** installed. CI pins 1.3.290.0; this machine has **1.4.313.1** (newer than the pin -- fine; the pin matters only for the CI runner provisioning). | `Test-Path "$env:VULKAN_SDK\Include\vulkan\vulkan.h"` is true | Claude session / 2026-07-10 (1.4.313.1) |
| [x] | **Slang 2026.1** DLL tree available. | `Test-Path Middleware\slang\bin\slang.dll` is true | Claude session / 2026-07-10 |
| [x] | **.NET runtime** (for `dotnet exec` -- Sharpmake regen's fallback path; the full SDK is NOT required since regen executes the tracked prebuilt dll) and **Git 2.30+**. | `dotnet exec Sharpmake\Sharpmake.Application.dll` works (exercised by the regen fallback); `git --version` >= 2.30 | Claude session / 2026-07-10 (runtime present, SDK absent -- sufficient; git 2.46.2) |

## C. First local build + bake (per BuildEnvironment.md section 3)

| Done | Item | How to verify | Verified by / date |
|---|---|---|---|
| [x] | **First Vulkan_True build + run executed once locally** -- `zenith build Zenithmon` (config `Vulkan_vs2022_Debug_Win64_True`) then `zenith run Zenithmon`. The tools boot bakes the generated meshes/textures/anims/terrains, which are git-ignored and therefore absent on a fresh checkout. **★ Corrected 2026-08-01:** this row used to say the boot bakes `FrontEnd.zscen` and that "without this, `_False` builds have no scene to load" -- false since ZM-D-148. All five `.zscen` and `Dawnmere.znavmesh` are TRACKED, so a fresh clone has its scenes; what it lacks is the art. | `Games\Zenithmon\Assets\` gains the baked mesh/texture/terrain output; the exe boots to the FrontEnd title screen. (`Test-Path ...Scenes\FrontEnd.zscen` is NOT a valid check any more -- it is true straight out of a clone.) | Claude session / 2026-07-10 |
| [x] | **Null (headless) config built once locally** -- `zenith build Zenithmon --headless`. Headless is a BUILD CONFIG (`Null_vs2022_Debug_Win64_True`), and test DISCOVERY always uses that exe, so *every* `zenith test` invocation -- windowed included -- fails without it. **Added 2026-08-01;** it was implicit in the S0 row below and is a distinct artifact. | `zenith test Zenithmon --headless` runs instead of erroring "test discovery needs the Null build" | Claude session / 2026-08-01 (implied by every gate run since the Null config landed) |
| [x] | **Headless test suite green locally** -- `zenith test Zenithmon --headless` (or the pwsh form from BuildEnvironment.md section 4.1) exits 0. S0 expectation: 1 automated test passed / 0 failed, plus the boot unit tests reported at engine boot. **Those S0 figures are a 2026-07-10 record, not a current baseline** -- read live counts from Status.md's CURRENT BASELINE block and the `zm-tests.yml` pin. | Runner prints the pass/fail summary and exits 0 | Claude session / 2026-07-10 (1/1 automated; 1070 boot units, 0 failed) |

---

## After all boxes ticked

The project is fully unblocked: master protection records `zm-tests` as a
required check (section A), and any local session can build, bake, run, and
test (sections B + C). Under ZM-D-031, agents work directly on `master` and
never create branches, PRs, or worktrees; Sharpmake regen, building, testing,
the authoritative local gate, and the direct-master commit/push are automatic
parts of the normal session workflow.
