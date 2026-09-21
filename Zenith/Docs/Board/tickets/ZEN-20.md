---
key: ZEN-20
type: TASK
title: Remove bSubmitRenderWork from SubmitRenderWork
category: Engine
parent: "ZEN-25"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: SIMPLE
risk: "MEDIUM"
storyPoints: 3
position: 16
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: tagn27rzxh7msdw2blhwi3n0
  risk: HIGH
  type: TASK
  title: Remove bSubmitRenderWork from SubmitRenderWork
  number: 20
  flagged: false
  due_date: null
  position: 16
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T21:50:02.316464
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:51:28.9
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-21T01:27:56.202Z"
claimAttempt: null
---
## Goal
`Zenith_Core.cpp`: `UpdateEditorAndTuning(bool& bSubmitRenderWork, bool& bShouldUpdateGameLogic)` sets `bSubmitRenderWork = g_xEngine.Editor().Update()` under `ZENITH_TOOLS` and `SubmitRenderWork(bool bSubmitRenderWork)` returns early when it is false, after `UploadFrameConstants`. The flag is only ever false when the editor's update reports it does not want a render this frame (the deferred scene operations path). The question the ticket asks: can this boolean be removed and render work always submitted?

Answer it with evidence, not preference. Enumerate the frames on which `Editor().Update()` returns false (a deferred `LoadScene` drain, a pending scene operation), and establish what submitting the render graph on such a frame would do: render an in-between scene state, or nothing. If always-submit is safe, delete the parameter and the early return and let the editor's deferred operations gate themselves. If it is not, replace the boolean with the thing it actually means (a frame-state query on the editor or scene manager that the submit step reads itself) so the tick no longer threads a flag through three functions. The `Zenith_Editor` unit suite and the scene-load automated tests (`Editor.SceneSaveDeltaClassifiesPublish`, the DP and ZM scene-swap tests) are the guards.

## Definition of Done
- [ ] `bSubmitRenderWork` no longer exists as a threaded parameter: either render work is always submitted or the submit step reads the frame state itself, with the reason recorded in a comment at the seam
- [ ] Deferred scene loads and editor mode changes still render correctly: the editor unit suite and the scene-swap automated tests pass on every game
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added

## Comments

ran on sonnet/low via SIMPLE+MEDIUM · category Engine

**Done.** Committed to `master` as `b84e0adf`. The answer to the ticket's question is that
always-submit is safe, so `bSubmitRenderWork` is deleted outright rather than replaced by a
frame-state query.

All three deferred scene-load branches of `ProcessDeferredSceneOperations` are wholly
synchronous and finished before the flag was ever read, and the scene mutation itself runs
earlier in the frame and strictly before `SetRenderTasksActive(true)` — so a same-frame render
could never have seen an in-between scene. `Zenith_Editor::Update()` is now `void`,
`SubmitRenderWork` and `EndFrameSubmitAndPresent` take no parameter, and all three backends'
`EndFrame()` dropped it along with the `FluxBackendDevice` concept that constrains them. The
backend half turned out to be behaviour-preserving on its own: `HasRecordedFrameWork()` is
false whenever the old flag was, so the `&&` was already a no-op.

Gates: 15/15 green on `-Phase verify`, 0 skipped, 0 deferred, 0 flakes; unit gate 2706 ran
against baseline 2706, so no pin moved. `zagent guard` clean.

**The gates did not cover the behaviour that changed**, and that is worth reading before
anyone trusts the green. No Engine gate line executes an editor deferred scene load at all.
The reviewer found `Games/DevilsPlayground/Tests/Test_EditorSceneCycle.cpp` — four editor
teardowns with frames in flight, `requiresGraphics = false` — and the hand run returned
`[AutomatedTest] Test_EditorSceneCycle: PASSED (601 frames, 9467.9 ms)`. That is the real
evidence; recorded as `ZEN-D-016` so the next ticket in this area runs it too.

Review returned five non-blocking findings and no blocking ones. All five were acted on after
I checked each against the sources; one of them refuted a paragraph I had written myself
(`ZEN-D-018`).

Three excursions outside the file list, all explained in the work log:
`Zenith/Flux/Backend/Concepts/Flux_Concept_Device.h` (compile-forced by the signature change —
my file list missed it), plus `Zenith/Flux/CLAUDE.md` and
`Zenith/Editor/Zenith_AnimationPreviewSession.h` (two more stale rationales the reviewer found).

Durable findings: `ZEN-D-016`, `ZEN-D-017`, `ZEN-D-018`. Two Suggestions rows filed against
tick tooling.

<!-- agent:worklog:start ZEN-20 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+MEDIUM · category Engine

**Outcome**: committed to `master` as `b84e0adf` · ran on sonnet/low via SIMPLE+MEDIUM

**Answer to the question the ticket asked.** Always-submit is safe; the parameter is deleted
rather than replaced by a frame-state query. The flag was false on exactly the frames
`Zenith_Editor::Update()` returned false — one of the three deferred scene loads in
`ProcessDeferredSceneOperations` — and all three are wholly synchronous and finished by the
time it was read, so there was never an in-between scene for a same-frame render to see. The
scene MUTATION runs earlier in the frame and strictly before `SetRenderTasksActive(true)`,
which is the invariant that actually matters, and it is untouched.

Two things made the removal cleaner than the ticket assumed:

* The backend half was **already** behaviour-preserving on its own. `m_bHasRenderWork` is
  reset in `Flux_RendererImpl::BeginFrame()` every frame and set true only inside
  `RecordFrame` (reachable only from `ExecuteRenderGraph`, which the early return preceded),
  so `bSubmitRenderWork && HasRecordedFrameWork()` already equalled `HasRecordedFrameWork()`.
  The reviewer confirmed the swapchain-acquire skip cannot leave a stale `true`.
* In every `_False` configuration the flag was a compile-time-constant `true`: only the
  `#ifdef ZENITH_TOOLS` arm of `UpdateEditorAndTuning` ever wrote it. Non-tools and agde are
  provably unchanged.

**What changed**

- `Zenith/EngineComposition/Zenith_Core.cpp` — `UpdateEditorAndTuning` no longer derives a
  render-submit gate; `SubmitRenderWork` and `EndFrameSubmitAndPresent` take no parameter and
  the early return is gone; `Zenith_MainLoop` drops the local. Reason recorded in a comment at
  the seam (DoD box 1).
- `Zenith/Editor/Zenith_Editor.h` / `.cpp` — `Update()` is `void`; five `return true/false`
  became plain `return`. `Zenith_Core.cpp` was its sole caller.
- `Zenith/RenderBackends/{Vulkan,Null,D3D12}` — `EndFrame(bool)` became `EndFrame()`; the
  Vulkan body now reads `HasRecordedFrameWork()` alone.
- `Zenith/Flux/Backend/Concepts/Flux_Concept_Device.h` — the `FluxBackendDevice` concept's
  `requires` clause, which every backend is `static_assert`-checked against. **Excursion, see
  below.**
- `Zenith/Flux/Flux_PerFrame.cpp`, `Zenith/Flux/Flux_RendererImpl.h`, `Zenith/Input/CLAUDE.md`,
  `Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp` — comments and docs the change
  falsified. The RenderTest edit is comment-only; its settle-count logic is untouched.
- `Zenith/Flux/CLAUDE.md`, `Zenith/Editor/Zenith_AnimationPreviewSession.h` — two more stale
  "a frame the editor declines to render" rationales. **Excursions, see below.**

**Files** (`git show --stat b84e0adf`)

    14 files changed, 187 insertions(+), 65 deletions(-)
    Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp |  13 +--
    Zenith/Editor/Zenith_AnimationPreviewSession.h           |   7 +-
    Zenith/Editor/Zenith_Editor.cpp                          |  21 ++--
    Zenith/Editor/Zenith_Editor.h                            |  16 ++-
    Zenith/EngineComposition/Zenith_Core.cpp                 | 111 ++++++++++-----
    Zenith/Flux/Backend/Concepts/Flux_Concept_Device.h       |  10 +-
    Zenith/Flux/CLAUDE.md                                    |   4 +-
    Zenith/Flux/Flux_PerFrame.cpp                            |  13 ++-
    Zenith/Flux/Flux_RendererImpl.h                          |  10 +-
    Zenith/Input/CLAUDE.md                                   |  11 +-
    Zenith/RenderBackends/D3D12/Zenith_D3D12.h               |   4 +-
    Zenith/RenderBackends/Null/Zenith_Null.h                 |   4 +-
    Zenith/RenderBackends/Vulkan/Zenith_Vulkan.cpp           |  21 ++--
    Zenith/RenderBackends/Vulkan/Zenith_Vulkan.h             |   7 +-

**Gates** — `-Phase verify`: `state: complete`, `exitCode: 0`, 15/15 green, **0 skipped,
0 deferred, 0 flakes**, `headSha` matched HEAD. `zagent gates` unioned in nothing: the diff
stayed inside Engine's own `paths`, and the Engine list already names every game.

| Command | Result |
|---|---|
| `zenith build engine` | PASS (23.5s) |
| `zenith build Combat --headless` | PASS (7.2s) |
| `zenith build Combat --config D3D12_vs2022_Debug_Win64_False` | PASS (8.6s) — the non-tools compile |
| `zenith build {Zenithmon,CityBuilder,RenderTest,TilePuzzle,DevilsPlayground,ScriptTest,Foundry,Undervault} --headless` | PASS — 8 lines, all green |
| `zenith test Combat --headless` | PASS (15.9s) |
| `run_unit_gate.ps1 -Game Combat` | PASS — **2706 ran / 2706 baseline**, 2705 passed, 0 failed, 1 skipped |
| `doc_lint.ps1 -Game Zenithmon` | PASS (15.1s) |
| `doc_lint.ps1 -Game DevilsPlayground` | PASS (40.6s) |
| **hand run** `devilsplayground.exe --automated-test Test_EditorSceneCycle --skip-unit-tests` | **PASS — `[AutomatedTest] Test_EditorSceneCycle: PASSED (601 frames, 9467.9 ms)`** |

`zagent guard ZEN-20`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith_Core.cpp` (always-submit path) | Every headless boot — `Null_*_True` defines `ZENITH_TOOLS`, so `SubmitRenderWork()` runs each frame of `test Combat` and every game boot. |
| `Zenith_Core.cpp` (the REMOVED skip branch) | **Nothing in the gate list.** Combat has no test that drives a deferred editor scene load. Covered only by the hand-run `Test_EditorSceneCycle` above — see `ZEN-D-016`. |
| `Zenith_Editor.cpp` / `.h` (`Update()` to void) | Every frame of every headless tools boot; its scene-load early-return arm only by the hand run. |
| `Zenith_Null.h::EndFrame()` | Every frame of every headless gate. |
| `Zenith_Vulkan.cpp::EndFrame()` | **Compile-only.** No gate builds or runs a `Vulkan_` config. |
| `Zenith_D3D12.h::EndFrame()` | **Compile-only** (the `D3D12_..._False` build). No gate runs a D3D12 exe. |
| `Flux_Concept_Device.h` | Genuinely verified at compile time — `static_assert` in `Flux_BackendConformance.cpp`, in all three backend builds. |
| `Flux_PerFrame.cpp`, `Flux_RendererImpl.h`, `Flux/CLAUDE.md`, `Input/CLAUDE.md`, `Zenith_AnimationPreviewSession.h`, `Test_RenderGraphViewStructure.cpp` | Comment/doc only — nothing executes them. `Test_RenderGraphViewStructure` is built by the Engine gates and run by none (`ZEN-D-003`). |

**Definition of Done**

- [x] `bSubmitRenderWork` no longer exists as a threaded parameter — render work is always
      submitted, and the reason is recorded in a comment at the seam (`SubmitRenderWork` in
      `Zenith_Core.cpp`), with matching notes on `Zenith_Editor::Update()` and each backend.
- [x] Deferred scene loads and editor mode changes still render correctly — the editor unit
      suite is inside the 2706/2706 Combat unit gate, all 11 games build, and
      `Test_EditorSceneCycle` (four teardowns, two of them Play-to-Stop backup restores, with
      frames in flight) passes headless.
- [x] Every game's pinned baseline bumped from an OBSERVED run if a unit was added — **no unit
      was added or removed**, observed: the unit gate reported `2706 ran` against baseline
      `2706` and passed. No pin moved, so `Tools/unit_baselines.json` is untouched and no
      `Status.md` LIVE PIN narration changed (both `doc_lint` C7 checks green).

**Durable findings**: `ZEN-D-016`, `ZEN-D-017`, `ZEN-D-018`

- `ZEN-D-016` — no Engine gate executes an editor deferred scene load; `Test_EditorSceneCycle`
  (DevilsPlayground, `requiresGraphics = false`) is the cheap headless cover and must be run
  by hand. Includes two mechanical traps: the verdict goes to stdout not the log file, and the
  boot re-authors `ProcLevel.zscen` (`ZEN-D-004`).
- `ZEN-D-017` — the three deferred scene-load branches do not share a mechanism;
  `HandlePendingSceneLoad` bypasses `Zenith_SceneSystem::LoadScene` entirely and dispatches no
  lifecycle. Distinguish SYNCHRONY (all three) from LIFECYCLE DISPATCH (two of three).
- `ZEN-D-018` — `Flux_Primitives` has producers on both sides of the `SubmitRenderWork` seam,
  so `ZEN-D-005`'s hazard reaches further than a debug-variable toggle. Never gate
  `ExecuteRenderGraph` on a per-frame skip.

**Deviations / follow-ups**

- **Three excursions outside `filelist.txt`, all explained, none reverted.**
  1. `Zenith/Flux/Backend/Concepts/Flux_Concept_Device.h` (worker's) — the `FluxBackendDevice`
     concept hard-codes `{ t.EndFrame(b) }` and every backend is `static_assert`-checked
     against it, so changing the three backends without it fails to compile in every
     configuration. Forced by the signature change the ticket asked for. **My file list missed
     it**, and the worker's step-zero survey found it mid-task rather than before its first
     edit — it flagged that itself rather than proceeding silently.
  2. `Zenith/Flux/CLAUDE.md` (mine) and
  3. `Zenith/Editor/Zenith_AnimationPreviewSession.h` (mine) — two more
     "a frame the editor declines to render" PULL-over-PUSH rationales that this change
     falsifies, found by the reviewer. The PULL design stays correct for the unit-test case;
     only the stale justification changed.
- **Review: no blocking findings, five non-blocking, all five acted on.** I verified each
  against the sources before acting rather than taking them on trust. One of them — finding 2
  — **refuted a paragraph I had written myself**: I had asserted the old early return "was not
  an instance of" `ZEN-D-005`. It was (`ZEN-D-018`). Also corrected: a false Awake/OnEnable
  citation in two places (`ZEN-D-017`), an inverted subject ("the call it used to gate off runs
  strictly before render tasks go active" describes `Editor::Update()`, not `SubmitRenderWork`),
  and a "deferred scene load/**reset**" op that does not exist.
- **Corrections to the ticket's own text before inlining it:** none were needed. The DoD named
  no pin site and no protected path, and `drift.txt` was absent (no unresolved citations). The
  Goal's central claim held up — I verified it independently before dispatch and inlined my own
  measured evidence (E1 to E8) alongside it.
- **Capture drift restored twice.** Each `Test_EditorSceneCycle` boot re-authored
  `Games/DevilsPlayground/Assets/Scenes/ProcLevel.zscen` (`ZEN-D-004`). Not this ticket's work;
  `git restore`d both times, before staging. The commit contains no asset change.
- **Two Suggestions rows filed**: `build_prompt.ps1` comma-splits `-Files` but not `-Rulings`,
  so the invocation form `tick.md` step 5 mandates cannot work for rulings; and ZEN's
  `decisionLog` (`Zenith/Docs/EngineDecisionLog.md`) holds one placeholder and zero real
  entries while 18 `ZEN-D-*` rows live on the board — so a tick following I3 literally finds
  nothing, and `zagent decide` warns that a loop worker resolving rulings from disk will never
  see these rows at all.
- **Not checked by anyone** (the reviewer's own list, carried forward): performance of the
  extra graph execute on a scene-load frame; `Zenith_UISystem::Update` internals; terrain
  streaming asynchrony after a load; the agde/Android build beyond the compile-time-constant
  argument.
<!-- agent:worklog:end -->
