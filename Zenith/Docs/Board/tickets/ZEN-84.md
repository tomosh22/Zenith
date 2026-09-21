---
id: "1e3402c6-e052-4e38-a6d1-f8ca0c04367e"
key: "ZEN-84"
type: "TASK"
title: "RenderTest tennis integration units must not depend on an asset a prior tools boot wrote"
category: "Engine"
parent: "ZEN-1"
sprint: null
status: "Blocked"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 84
labels: ["flaky"]
fixVersions: []
blocks: []
updatedAt: "2026-09-21T11:28:37.788Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
A durable finding from the Behaviour Graph parity program (2026-09): *"Deleting on-disk `.bgraph` before a gate reds `RenderTestTennis::Integration{LoadWiring,BlackboardPublish}`: those units load an asset authored by a prior tools boot."* A boot unit that depends on a file a PREVIOUS run wrote is order-dependent: green on a warm tree, red on a fresh clone or after a clean, and the gate cannot tell which. The pin (`ran == baseline`) counts it either way.

Make the two units self-sufficient: author the graph they load inside the unit from the same builder the tools boot uses (or skip with a NAMED reason that the harness reports as a skip, never as a pass), and add a lint over the boot suite that reds when a unit opens a path under a gitignored asset root without first producing it. Run the RenderTest suite on a clean tree as the proof.

## Definition of Done
- [ ] `RenderTestTennis::Integration{LoadWiring,BlackboardPublish}` pass on a clean tree with no prior boot
- [ ] A lint reds a boot unit that reads a gitignored asset it did not produce
- [ ] Every game's pinned baseline bumped from an OBSERVED run

## Comments

ran on opus orchestrator + sonnet worker + reviewer · category Engine

**Done.** Committed to `master` as `424d868c`.

**Nothing here had already shipped** — unlike the last six tickets, the premise was true, and I
confirmed the mechanism rather than the symptom: `.bgraph` is gitignored, **zero of the 43** in
that directory are tracked, the asset is written only by `AddStep_GraphBuild`, and a unit-gate
boot never drains the automation queue — so the gate *cannot* produce it. The unit's own
assertion message confessed it ("asset authored by a prior tools boot"). The reviewer
independently verified the boot-order half.

**Clause 1.** `TennisMatchFixture` now authors the asset itself every construction, from the
production builder. Review found two defects in that fixture, one blocking: `ZENITH_ASSERT_TRUE`
is **record-and-continue**, so a failed `Build()` recorded the failure and then wrote the errored
definition to disk — inverting `Zenith_EditorAutomation.cpp:3480-3495`'s ruling, whose stated
reason ("a warm tree must fail the same way a cold clone does") is this ticket's own thesis.
Now mirrored: remove the stale asset, return before saving. Also added a `ForceUnload`, because
`AddGraphByAssetPath` reads through the registry cache and the "always fresh" comment was
otherwise false.

**Clause 2.** The lint was **rewritten**. The first version flagged any path-shaped literal —
**243 findings, and blind to the motivating case**, since the tennis path arrives via
`RenderTest_TennisAgentComponent::kszGraphAsset` and the literal never appears in the test file.
The replacement anchors on verified **readers**, resolves named constants including
class-scoped ones (`kszGraphAsset` is declared by eight components — a bare-name map called that
a collision and lost the case a second time), and strips comments before both scans.
**27 findings**, report-only.

★ **A COMMENT COUNTED AS A PRODUCER.** Mutation-checking found it: deleting the real `Save` left
the read `ok`, because the comment written to *explain this ticket's fix* named
`Zenith_AssetRegistry::Save()`. The better someone documented an order-dependence fix, the more
reliably the lint went quiet about that file.

**Clause 3 — no pin moves**, observed: no unit added or removed, and `-Phase measure` returned
exit 0, not 10.

**Evidence.** Cold tree, run twice: `.bgraph` deleted, rebuilt, unit gate **exit 0,
`2821 ran, 2820 passed, 0 failed, 1 skipped`** (skip pre-existing), pin unchanged, fixture
recreated the asset. Lint exit 0, 47 bare + 11 class-qualified constants. Assert script
**64/64**. The DoD's claim demonstrated on its own case: removing only the real `Save`, comments
intact, turns the line `ok` → **FINDING** (27→28). Mutations red precisely — and corrected the
worker's predicted one (a `[^\r\n]`→`''` stripper reds only the length assertion, not the
line-number one, because newlines survive).

★ **The lint found a real second instance**: `Test_DPGraphNodeWorldPins.cpp:124` reads untracked
`game:Graphs/DP_Door.bgraph`, written only by `DevilsPlayground.cpp:2431`'s `AddStep_GraphBuild`
— the identical mechanism, in a game nobody was looking at. Filed separately.

**Two corrections to my own evidence.** I told the worker `SetPath` had 335 call sites; it has
**5** in the tracked population — my tally swept `.claude/worktrees/`. And my hypothesis that the
18 `zz_unit_*` GraphEditor findings author via an editor save was wrong: those paths must NOT
exist. Both caught by others, both fixed.

**Gate note.** Verify exit 4 on the `Tools/Vault` `npm test` line —
`cli-process.test.mjs` at 5351 ms, 825/826, **green on rerun (exit 0)**. First name on
`ZEN-D-024`'s list, fourth consecutive tick. The diff is one `.inl` and two `.ps1` — zero
JavaScript. The red skipped 3 `doc_lint` lines; run by hand, **exit 0 on all three**.
`zagent guard` clean. `registry: {"Combat": 17}` populated live for the first time, closing
ZEN-78's open item.

**One declared excursion**: `Tools/Test_CheckBootUnitAssets.ps1` is not on `filelist.txt` — I
asked for it, since a new `Tools/*.ps1` without assert coverage breaches house convention.

<!-- agent:worklog:start ZEN-84 -->
## Work Log — 2026-09-21

ran on opus/orchestrator + sonnet worker + reviewer · category Engine

**Outcome**: committed to `master` as `424d868c`.

★ **Unlike the last six tickets in this queue, nothing here had already shipped.** The
premise was true and I confirmed the MECHANISM rather than the symptom before dispatching:
`.bgraph` is gitignored (`.gitignore:134 **/Assets/**`), **zero of the 43** `.bgraph` files
in `Games/RenderTest/Assets/Graphs/` are tracked, the asset is written only by
`AddStep_GraphBuild` (`RenderTest.cpp:2460`), and a unit-gate boot **never drains** the
automation queue — so the unit gate *cannot* produce it. The unit's own assertion message
confessed it: `"TennisBrain graph attached (asset authored by a prior tools boot)"`.

The reviewer independently confirmed the boot-order half, which I had expected to be the
weak link: units run at `Zenith_Engine.cpp:971`, `Project_RegisterEditorAutomationSteps()`
only at `:993`, and `--exit-after-unit-tests` exits at `Zenith_Main.cpp:187` before the main
loop. So the cold-tree measurement proves what it appears to prove.

### Clause 1 — the fixture

`TennisMatchFixture` now calls `EnsureTennisBrainGraphAssetAuthored()`, building from the
production `BuildGraph_RenderTestTennisBrain` and saving through `Zenith_AssetRegistry::Save`
— unconditionally, every construction. A conditional skip would still be order-dependent,
just quieter.

**Two defects were found in that fixture after it was first written, both by review:**

1. ★ **BLOCKING — it wrote an invalid `.bgraph` when `Build()` failed.**
   `ZENITH_ASSERT_TRUE` is **record-and-continue** (`Zenith_TestFramework.h:275` expands to a
   plain `AssertTrue(...)` call, no control flow), so a failed build recorded the failure and
   then fell through and saved the errored definition. `Zenith_EditorAutomation.cpp:3480-3495`
   does the opposite deliberately, and its comment is this ticket's own thesis: *"a .bgraph
   with errors is never written, and the STALE one from an earlier boot is removed so a warm
   tree fails the same way a cold clone does"*. The fixture broke that rule in the same commit
   that cited it. Now mirrored: remove the stale asset, return before saving.
   **Latent, not live** — `Build()` returns true today, so every gate stayed green.
2. **The registry cache made the "always fresh" comment false.** `AddGraphByAssetPath` reads
   through `GetView<T>`, which returns the cached asset, so every fixture after the first read
   the cache rather than the bytes just written. Fixed with a `ForceUnload` after the save,
   matching the lint's own exemplar of the correct shape
   (`Zenith_TerrainEditor.Tests.inl:1008`, *"★ THE EVICTION IS THE ASSERTION"*).

### Clause 2 — the lint, which was rewritten once and then fixed four times

The first implementation flagged any path-shaped literal in a test file lacking a `Save(`:
**243 findings, and blind to the case the ticket is about.** Two sampled findings were
`SetGraphName(...)` and `SetTexturePath(...)` — in-memory setters touching no disk — and the
tennis path reaches its call site through `RenderTest_TennisAgentComponent::kszGraphAsset`,
so the literal never appears in the test file at all.

I prototyped and measured the replacement before specifying it. Final shape: two passes,
driven by `git ls-files`; pass 1 resolves named `const char*` constants (bare **and**
class-qualified); pass 2 flags a gitignored asset path that is the first argument of a
verified **reader**, in a file carrying neither producer signature. Both scans run against a
comment-stripped view.

| fix | why |
|---|---|
| read-anchoring | 243 → 27 findings |
| class-qualified constants | `kszGraphAsset` is declared by **eight** components; a bare-name map called it a collision and skipped it — losing the motivating case a second time |
| `Authored/` regex | `(^\|[\\/])Authored` never matched `engine:Authored/...`, because the root prefix's **colon** precedes the segment. Pre-existing, inherited, untested |
| second producer signature | `RunAutomation(` **and** `AddStep_*Save\|Build(`, both required — queuing a step is not producing an asset, which is this ticket's defect restated |
| comment stripping | see below |

★ **A COMMENT COUNTED AS A PRODUCER, AND THE FAILURE WAS SELF-DEFEATING.** Mutation-checking
the acceptance property found it: deleting the real `Save` at `:1571` left the read reported
`ok`, because the comment at `:1544` — written to *explain this ticket's fix* — named
`Zenith_AssetRegistry::Save()`. The better someone documented an order-dependence fix, the
more reliably the lint went quiet about that file. Both scans now use an offset-preserving
stripped view (comment spans blanked to spaces, newlines kept, so line numbers are untouched).

**`AddMeshByPath` was dropped from the reader list**: it is `SetPath` + `PushBack`, no disk —
a setter with a longer name. **`Acquire<T>` was added** (verified to reach the same
`GetInternal` as `GetView`). **`Get<T>` was not added because it does not exist** — five
documents reference `Zenith_AssetRegistry::Get<T>` and there are zero call sites; the real
name is `GetView<T>`. Filed as a follow-up.

### Clause 3 — the pin

**No pin moves, and that is an observed result, not an inference.** No unit was added or
removed (`git diff | grep -c '^+.*ZENITH_TEST'` → 0), and `-Phase measure` returned **exit 0**,
not 10.

### Evidence

- **Cold tree, run twice** (once after the first fixture, once after the blocking fix): deleted
  `RenderTest_TennisBrain.bgraph`, rebuilt `Null_vs2022_Debug_Win64_True`, ran the unit gate →
  **exit 0, `2821 ran, 2820 passed, 0 failed, 1 skipped`** both times, pin unchanged at 2821,
  and the fixture **recreated the asset** each time. The 1 skip is pre-existing — the only
  `ZENITH_SKIP` in RenderTest is `RenderTest_Testbed.Tests.inl:109`.
- **Lint**: exit 0, 1755 tracked source files, 47 bare + **11 class-qualified** constants
  resolved, 2 bare names skipped as colliding, **27 findings**.
- **Assert script**: **64/64, exit 0.**
- ★ **The DoD's own claim, demonstrated on the ticket's own case**: removing only the real
  `Save` — with every explanatory comment left in place — turns the tennis line from `ok` to
  **`FINDING`** (27 → 28).
- **Mutation-checked**, each redding precisely: a whole-comment-deleting stripper → exactly the
  line-number and length assertions; a `[^\r\n]`→`''` stripper → exactly the length assertion
  (**not** the line-number one, because newlines survive — the worker's predicted mutation was
  wrong here and the measurement corrected it).

### ★ The lint found a real, independent second instance

`Games/DevilsPlayground/Tests/Test_DPGraphNodeWorldPins.cpp:124` reads
`game:Graphs/DP_Door.bgraph`, which is **untracked** (zero `.bgraph` files are tracked
anywhere) and written only by `DevilsPlayground.cpp:2431`'s `AddStep_GraphBuild` — the
identical mechanism. Same for `DP_DoubleDoor.bgraph`. That is the strongest validation clause 2
could get: the lint locates the defect it was built from, in a game nobody was looking at.
Filed as a follow-up rather than fixed here (different category, different gates).

**One correction to my own evidence**: I told the worker `SetPath` had **335** call sites. It
has **5** in the tracked candidate population. My tally swept `.claude/worktrees/` (1475+
`.Tests.inl` copies) because it excluded `obj|output|.vs|dist|Middleware|.git` but not
`.claude`. The reviewer caught it in the shipped header. The 243 → 27 figures are unaffected —
the old script *did* exclude `.claude` at top level.

### Gates

`-Phase verify`: **exit 4**, `Tools/Vault` `npm test` red on
`cli-process.test.mjs > process CLI reads gates from --file…` at 5351 ms, **825/826** — and
**green on its own rerun (exit 0, 329.3 s)**, recorded as a flake. That is the **first name on
`ZEN-D-024`'s list and the fourth consecutive tick it has flaked on**. Innocence is airtight:
the entire diff is one C++ `.inl` and two PowerShell files — **zero JavaScript, nothing under
`Tools/Vault/`**.

The red skipped 3 gate lines (`doc_lint` for Foundry, Undervault, Hearth); **run by hand, exit
0 on all three**. 22 of the other 23 steps green.

`zagent guard ZEN-84`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

★ **`registry: {"Combat": 17}` populated live for the first time** — this was the open item
ZEN-78 recorded (*"the next Engine-category tick will be the first real one — worth a glance"*),
and 17 matches what that ticket measured against the live checkout. Closed by observation.

### Definition of Done

- [x] The two units pass on a clean tree with no prior boot — observed twice, cold.
- [x] A lint reds a boot unit that reads a gitignored asset it did not produce —
      `Tools/check_boot_unit_assets.ps1`, demonstrated on this ticket's own case by mutation.
- [x] Every game's pinned baseline bumped from an OBSERVED run — **none moved**; no unit was
      added or removed and measure returned exit 0.

### Deviations

- **One declared excursion**: `Tools/Test_CheckBootUnitAssets.ps1` is not on `filelist.txt`. I
  asked for it explicitly — a new `Tools/*.ps1` with no assert-script coverage would breach the
  house convention every sibling script follows. Declaring rather than hiding it.
- **The lint ships report-only.** 27 findings stand untriaged, and the largest family (18
  `zz_unit_*.bgraph` reads in `Zenith_EditorPanel_GraphEditor.Tests.inl`) is a **false-positive
  class**: those paths must NOT exist, exercising `OpenAsset`'s "new asset" branch
  (`:323-326` says so). `-Fatal` wants those triaged first. My own hypothesis that they author
  through an editor save path was wrong; the worker read the source and corrected me.
- **Not verified by anyone**: whether the DP findings actually red a cold DP gate. I confirmed
  the mechanism, not the symptom, and did not run DevilsPlayground.

**Durable findings**: ZEN-D-026 — a textual check that asks "does this file do X" can be
silenced by prose describing X, and the prose most likely to silence it is the comment
explaining the very fix the check rewards. Found by mutation, not by reading the lint.
<!-- agent:worklog:end -->
