---
key: ZEN-6
type: STORY
title: Headless runs must be able to CHANGE a committed .zscen, not only create a missing one
category: Engine
parent: null
sprint: null
status: Done
assignee: agent@zenith.invalid
priority: HIGH
complexity: COMPLEX
risk: HIGH
storyPoints: null
position: 1
labels:
  - engine-gap
fixVersions: []
blocks: []
links: []
source:
  id: wwb5ouei7dxiz4sj69yh1e5l
  risk: HIGH
  type: STORY
  title: Headless runs must be able to CHANGE a committed .zscen, not only create a missing one
  number: 6
  flagged: false
  due_date: null
  position: 1
  priority: HIGH
  severity: null
  parent_id: null
  sprint_id: null
  status_id: dxhlckk5kgxv4mgp4e8lcu7s
  complexity: COMPLEX
  created_at: 2026-08-22T20:52:31.26386
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: DONE
  updated_at: 2026-08-26T12:18:28.133
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: 2026-08-26T12:18:28.126
  story_points: null
  reproducibility: null
---
## Goal

A headless run must be able to CHANGE a committed `.zscen`, not only create a
missing one.

`Zenith_Editor::SaveActiveScene` (`Zenith/Editor/Zenith_Editor.cpp:1423`) wraps its
write in `if constexpr (Zenith_IsNullRenderer())` and refuses any save that would
alter an asset already on disk. **That guard is correct today and must not simply be
deleted.** Its reason is recorded directly above it: a Null-backend boot authors an
*incomplete* world, so serializing what it holds over a tracked asset silently
DELETES content. That is not hypothetical — it once dropped RenderTest's two
instanced-tree entities, roughly 323 KB of a 361 KB file, on every headless boot.

The root cause is that authoring steps needing a live GPU resource bail out
**entirely** instead of skipping only the GPU part.
`Zenith_TerrainEditor::EnsureTreeEntities`
(`Zenith/Editor/TerrainEditor/Zenith_TerrainEditor_Trees.cpp:78`) is the canonical
case — it returns `false` on its first line under Null, before any entity work, with
the comment *"Instance groups allocate GPU buffers on first spawn — a tools-bake
semantic that a Null build deliberately skips."*

So the fix is to separate two things those functions currently conflate:

- **creating entities and components** — backend-neutral, must always happen;
- **allocating the GPU resources those components would use** — legitimately skipped
  under Null.

Once a Null boot's in-memory scene is entity-complete, the publish guard has nothing
left to protect against.

**Not every `Zenith_IsNullRenderer()` bail is of this kind, and the difference is the
whole ticket.** `Zenith_Editor_SceneOps.cpp:212` skips Flush / WaitForGPUIdle /
ProcessDeferredDeletions — genuinely vacuous with no GPU, and touching no entity. Its
own comment says it is kept as a gate for cost, not correctness. That one is right as
written and must stay. Audit each bail on the authoring path and classify it before
changing it; the ones that matter are those that skip *entity or component creation*.

**Why this is worth doing.** Eight Zenithmon tickets are labelled `windowed` for no
reason other than that they re-author a scene — R1-2, R1-3, R1-6, R1-10 and their
proofs. Six of the ten tickets on the S8 critical path sit behind one of those, so
the chain alternates human and agent work and the loop can never get more than one
ticket ahead of a keyboard session. This ticket is what collapses that alternation.

**One caution for whoever takes it.** The determinism work that made authored bytes
config-independent (`ZENITH_AUTHORING_DETERMINISM_BEGIN` in `Zenith/Core/Zenith.h`, plus the
non-inline `Zenith_Maths::Authoring*` transcriptions) exists because `/fp:fast` let
the optimizer reassociate differently at `/Od` and `/O2`. Entity creation moving into
the Null path must not reintroduce a glm call on the authoring path — glm's operators
are header inlines that take their FP model from their own definition point and
ignore a `float_control` pin around the caller.

## Definition of Done

- [ ] Every authoring step that currently skips ENTITY or COMPONENT creation under
      the Null backend instead creates them and skips only the GPU resource
      allocation; `EnsureTreeEntities` is the first, and the audit of the rest is
      recorded
- [ ] A test asserts that a Null tools boot produces the expected entity set for the
      RenderTest and Zenithmon authoring paths, naming the entities rather than
      counting them
- [ ] A headless re-author of an already-committed scene reproduces the committed
      bytes EXACTLY, for every scene both games commit — a no-op re-author is the
      completeness proof, and it needs no windowed run to check
- [ ] The headless publish guard in `Zenith_Editor::SaveActiveScene` is removed, and
      the reasoning is recorded in the decision log
- [ ] Authoring math on the newly reachable path calls no glm operator, so Debug and
      Release still author byte-identical scenes
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run

<!-- agent:worklog:start ZEN-6 -->
## Work Log — 2026-08-26

**Outcome**: merged to `master` as `cbebea74` · ran on opus/high via COMPLEX+HIGH · category Engine

**What changed**

- `Zenith/Editor/TerrainEditor/Zenith_TerrainEditor_Trees.cpp` — `EnsureTreeEntities` no longer returns `false` on its first line under Null; it creates `TerrainTrees_Trunk` / `TerrainTrees_Leaves` and their components on every backend. `ApplyTreeDab`'s slope threshold moved from `glm::radians` to `Zenith_Maths::AuthoringRadians`. `ResolveTreeComponent` now takes the expected entity NAME and rejects a mismatch.
- `Zenith/Editor/Zenith_Editor.cpp` — the `if constexpr (Zenith_IsNullRenderer())` publish refusal and `HeadlessMayPublishScene` are gone. `AuditScenePublish` replaces them: the same `CompareWithFile` comparison, on **every** backend, which logs `[ScenePublish] CREATED|IDENTICAL|CHANGED`, still skips a byte-identical write, and still reports an entity-dropping publish as a `Zenith_Error` — reporting rather than refusing.
- `Zenith/Editor/TerrainEditor/Zenith_TerrainEditor.h` — the two entity-name constants and `EnsureTreeEntities` are public seams so a unit can pin them.
- `Zenith/Editor/Zenith_Editor.Tests.inl` — `HeadlessSaveNeverRewritesSceneAsset` (which pinned the removed policy) replaced by `ScenePublishWritesOnEveryBackend`; new `TreeAuthoringIsBackendNeutral`; `SceneSaveDeltaClassifiesPublish` untouched.
- `Zenith/Editor/TerrainEditor/Zenith_TerrainEditor_Bake.cpp` — comments only: the ZEN-6 audit table, all five surviving editor sites.
- `Games/RenderTest/RenderTest.cpp`, `Games/RenderTest/Tests/SceneAssetIntegrity.cpp` — comments only. **No assertion changed.** The byte count `361721` corrected to `361753`.
- `Tools/unit_baselines.json` — Combat 1650→**1651**, Zenithmon 3419→**3420**, RenderTest 1741→**1742**. All three OBSERVED.
- Living docs corrected to describe the post-ZEN-6 world, each keeping the old rule as marked history: `Zenith/Editor/CLAUDE.md`, `Zenith/Null/CLAUDE.md`, `Games/RenderTest/CLAUDE.md`, `Docs/GameProjects.md`, `Games/Zenithmon/CLAUDE.md`, root `CLAUDE.md`, `Games/Zenithmon/Docs/AgentBriefing.md`, `Games/Zenithmon/Docs/Status.md`, and two `ZenithECS` comments.

**Files** (`git diff --stat`) — 19 files, ~640 insertions. No new `.cpp`, so no regen needed.

**Gates** — 14 lines, all exit 0, `skipped: []`

| Command | Result |
|---|---|
| `build engine` | 46.3s |
| `build Combat --headless` / `--config D3D12_*_False` | 5.9s / 14.2s |
| `build Zenithmon` / `CityBuilder` / `RenderTest` / `TilePuzzle` / `DevilsPlayground` `--headless` | all green — **every game compiles** |
| `test Combat --headless` | 19.4s |
| `run_unit_gate -Game Combat` | **1651 ran / 1650 passed / 0 failed** |
| `build Zenithmon --config D3D12_*_False` | 17.6s · *unioned in by `zagent gates`* |
| `test Zenithmon --headless` | 1307.5s · *unioned in* |
| `run_unit_gate -Game Zenithmon` | **3420 / 3418 / 0** · *unioned in* |
| `doc_lint -Game Zenithmon` | 2.0s · *unioned in* |

The last four were unioned in because the diff reached `Games/Zenithmon/**` (the pin narration and the briefing). `zagent guard` clean, `gateSelectionStale: null`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith_TerrainEditor_Trees.cpp` (`EnsureTreeEntities`, `ResolveTreeComponent`) | `Editor.TreeAuthoringIsBackendNeutral` — names both entities, asserts each carries the component and is non-transient, asserts idempotent adoption. Plus every RenderTest authoring boot |
| `Zenith_TerrainEditor_Trees.cpp` (`AuthoringRadians` swap) | **No unit executes it.** Covered by the hand-run byte-identity proof below — the value is a rejection threshold, so any change to it moves all 2,520 instances and the scene ceases to be `IDENTICAL` |
| `Zenith_Editor.cpp` (`AuditScenePublish`) | `Editor.ScenePublishWritesOnEveryBackend` + `SceneSaveDeltaClassifiesPublish` (all three delta results, both `WouldDropEntities` arms, the transient rule) |
| `Zenith_TerrainEditor.h` | compiled into every `*_True` config of all six games |
| `SceneAssetIntegrity.cpp`, `RenderTest.cpp` | **Nothing — comment-only changes.** `RT_SceneAssetIntegrity`'s own assertions are unchanged, but they were near-vacuous headless (a headless boot never wrote the file) and are now real |
| `Tools/unit_baselines.json` | both unit gates, which assert `ran == baseline` EXACTLY |
| all `CLAUDE.md` / `Docs/**` | `doc_lint` C7 for the pin block. **The PROSE is read by no gate** — which is why the reviewer covering my edits is the only control on it |

**Definition of Done**

- [x] Every authoring step that skips ENTITY or COMPONENT creation under Null instead creates them and skips only the GPU allocation; `EnsureTreeEntities` is the first, and the audit of the rest is recorded
- [x] A test asserts that a Null tools boot produces the expected entity set for the RenderTest and Zenithmon authoring paths, naming the entities rather than counting them
- [x] A headless re-author of an already-committed scene reproduces the committed bytes EXACTLY, for every scene both games commit
- [x] The headless publish guard in `Zenith_Editor::SaveActiveScene` is removed, and the reasoning is recorded in the decision log
- [x] Authoring math on the newly reachable path calls no glm operator, so Debug and Release still author byte-identical scenes
- [x] Every affected game's pinned baseline bumped from an OBSERVED run

**Item 3 — proven by observation, and the first attempt was worthless**

A `Null_vs2022_Debug_Win64_True` boot driven with `--automated-test <T> --skip-unit-tests`
reported `[ScenePublish] IDENTICAL` for **every scene both games publish**: RenderTest at
**82 entities / 361,753 bytes** — byte-for-byte the committed file, trees included, against
the ~38 KB subset whose loss motivated the guard — and Zenithmon's `FrontEnd`, `PlayerHome`,
`Battle`, `ProfLab`. Zero `CHANGED`; `git status` clean.

★ **My first attempt at this proof was worthless and looked exactly like success.** I used
the unit-gate boot, which exits before the editor-automation queue drains: it authored
nothing, emitted no `[ScenePublish]` line, and left a clean tree. **A clean tree is the
fail-open state for this ticket.** That is now recorded in `AgentBriefing.md` so the next
worker does not repeat it.

**Review — three BLOCKING findings, all fixed at fix-forward 1 of 3**

- **Item 5 was NOT met, and the worker's reason for leaving it was false.**
  `Zenith_Maths::AuthoringRadians` already existed (`Zenith_Maths.h:84`, defined inside the
  determinism pin at `Zenith_Maths.cpp:117`) with glm's identical source expression — there
  was nothing to transcribe. The caller's `float_control` pragma cannot reach glm's
  template, which is a COMDAT shared with every `/fp:fast` TU; `Zenith_Maths.h:63-68`
  records that as MEASURED. **After the swap the scene re-authored to the same 361,753
  bytes**, so item 5 is met with evidence — and the committed bytes are confirmed NOT to
  have come from the unpinned expression, which would have been a larger finding.
- **The living docs still stated the removed rule as engine law.** The worst was
  `Games/Zenithmon/Docs/AgentBriefing.md`, which a loop worker reads off disk as binding —
  left alone, the next ZM worker would refuse to re-author headless and nullify this
  ticket's entire payoff. My own `Status.md` also contradicted itself in two places, and
  root `CLAUDE.md` justified the `needs-gpu`/`needs-human` split with a guard that no longer
  exists.
- **A byte count I wrote disagreed with two code comments.** The file is **361,753**; both
  said `361721`, and one was inside a comment block this diff had already touched.

Plus N1, taken further than asked: the cached tree entity IDs could alias a live unrelated
entity after `ResetForNextTest` wipes generations (the test's own comment asserted the
generational argument was sufficient — it is not, across an entity-store reset). The fix
went to `ResolveTreeComponent` rather than the test teardown, closing the window for every
caller.

**Deviations / follow-ups**

- **Corrections to the ticket's own text before inlining it (I3).** None needed — ZEN-6's
  four checkable citations all resolved. Its three line citations were not checked (they
  cannot be), and `Zenith_Editor.cpp:1423` was accurate.
- **My pre-derived audit table was wrong by one and the worker corrected it.** I listed
  seven `Zenith_IsNullRenderer()` sites in `Zenith/Editor/`; there are eight — I missed one
  inside the very test being replaced. Handing it over as "a hypothesis to falsify" is what
  made that correction happen rather than a silent inheritance.
- **ZEN-6 declares no `decisionLog`.** The Engine category has none, so DoD item 4's
  "recorded in the decision log" had no repo-local home. Recorded to the board's Decisions
  database, and the Zenithmon-visible consequence narrated in `Status.md` where a ZM tick
  greps. **`zagent.project.json`'s Engine category wants a `decisionLog`** — filed as a
  Suggestions row.
- **Stale references I could not fix from inside a tick**, both `protectedPaths` and both
  correctly refused by `zagent guard`: `.github/workflows/zm-tests.yml:450,457` and
  `.claude/commands/tick.md:536` — the latter cites the removed guard as the reason
  `needs-gpu` exists. Also `Games/Hearth/Docs/TDD.md:250`, which architects an unbuilt game
  around the removed rule, and the append-only pin history in `Docs/BuildSystem.md:289-290`
  and `Tools/run_unit_gate.ps1:189-196` naming the deleted test.
- **The resolver hardening ships without a dedicated unit, deliberately.** Adding one would
  be +1 on every game's pin, and all three were already measured and written. A follow-up.
- **New cost on the WINDOWED path**: `AuditScenePublish` runs on every backend, so every
  `SaveActiveScene` now pays a full in-memory serialize plus a file read. And a windowed
  IDENTICAL save skips the write and no longer logs `[EditorOp] Saved scene to '%s'` —
  anything grepping for that line will miss.
- **`zagent docs sync` completed in 24 seconds**, 7 pages updated, exit 0.
<!-- agent:worklog:end -->
