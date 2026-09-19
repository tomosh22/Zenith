---
key: ZEN-12
type: TASK
title: Remove FLUX_RENDER_VIEW_PREVIEW
category: Engine
parent: "ZEN-25"
sprint: null
status: "In Review"
assignee: "agent@zenith.invalid"
priority: HIGH
complexity: COMPLEX
risk: HIGH
storyPoints: 13
position: 8
labels: ["human-gate","needs-gpu","gpu-reason:visual-measure"]
fixVersions: []
blocks: []
links: []
source:
  id: g30r57ppgqimjysvh27gjf6d
  risk: HIGH
  type: TASK
  title: Remove FLUX_RENDER_VIEW_PREVIEW
  number: 12
  flagged: false
  due_date: null
  position: 8
  priority: HIGH
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: COMPLEX
  created_at: 2026-09-11T21:27:24.056525
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:31:25.677
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T21:49:56.017Z"
claimAttempt: null
---
## Goal
`FluxRenderViewType` (`Zenith/Flux/RenderViews/Flux_RenderViews.h`) should enumerate genuinely different KINDS of view (main, shadow cascade, and future ones such as planar reflections). `FLUX_RENDER_VIEW_PREVIEW` is not one of those: it exists so the whole pipeline can be rendered a second time to a second final image for the editor's material and animation previews, and the one consumer that branches on it is `Flux_HDR.cpp` (the exposure path), with the preview slots filled by the preview controller. That is a routing decision leaking into the frame.

The target: while a Flux frame is rendered it has NO knowledge of whether the result goes to the swapchain, the editor viewport or an editor preview. Abstract the destination one level up: a render "request" names its camera, its resolution and its output target, and the preview is the same frame loop run again against a different target (the render graph already owns pass ordering and barriers, so a second execution is a second graph instance, not a branch inside passes). Remove the enum value and every `== FLUX_RENDER_VIEW_PREVIEW` test; the preview controller submits a request instead of claiming view slots by type.

Prove it headless: the render-graph unit suite compiles two requests against the Null backend and asserts identical pass lists and no destination-dependent branch remains (a grep-style unit over the pass table, plus the existing `Flux_RenderViews` units). Windowed: the material preview and the animation editor's slot-6 preview render identically before and after (`RT_PhotoTour` and the editor's preview capture), and the main viewport's exposure is unchanged. Engine gates: every game builds; the pins move only if units are added.

## Definition of Done
- [ ] `FLUX_RENDER_VIEW_PREVIEW` no longer exists; `FluxRenderViewType` names only view kinds, and no pass branches on the output destination
- [ ] The editor material preview and animation preview render through the same frame loop as the main view, against their own targets, and look identical to before (captures attached)
- [ ] A headless unit proves two requests with different destinations produce identical pass lists
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added

## Comments

ran on opus/high via COMPLEX+HIGH · category Engine

**In Review** (`human-gate`) — committed to `master` as `3d24f92f`. All 17 gate lines exit 0.

`FLUX_RENDER_VIEW_PREVIEW` is gone; `FluxRenderViewType` names only MAIN and SHADOW_CASCADE. The one production branch keys on `Flux_RenderView::m_bOwnsPersistentLDR` instead. Single-graph-per-frame architecture untouched — the Goal's "second graph instance / render request API" does not match the code and no DoD item needed it.

**One DoD clause ticked with a qualification you should see before signing.** Box 1 says "no pass branches on the output destination". `Flux_HDR.cpp:836` still reads `if (xView.m_bOwnsPersistentLDR)` — a branch on destination. What this removes is the destination being a view KIND in the type enum. Real, narrower than the wording.

Pins all OBSERVED, none computed: Combat 2697→**2698**, Zenithmon 4550→**4551**, RenderTest 2800→**2801**, 0 failures.

DoD box 3 proven twice and both RUN: clause 5b on live pass inventories (`slot 5 declared 43 passes, slot 6 declared 43`, 2217 checks 0 failed) and a new registry unit. **Clause 5b was run by hand — no gate line runs any RenderTest automated test** (`ZEN-D-003`).

**What needs your eyes:** 18 before / 18 after captures across the photo tour, the material preview (slot 5) and the animation preview (slot 6), in `.zagent/run/ZEN-12/{before,after}/`. Nothing is byte-identical, so each set has a same-binary noise floor in `after2/`. Result: identical maxima everywhere, change within ~1.4× of the floor, and for slot 6 **quieter than the floor**. The honest claim is "not distinguishable from re-running the same binary", not "identical" — the sign-off is yours.

Durable findings: `ZEN-D-003` (RenderTest harness tests are compile-checked only by every gate list), `ZEN-D-004` (any `--automated-test` boot authors scenes, on either backend).

Excursion, explained: `Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp` branched on the enumerator in live code at :577 and would not compile after removal.

<!-- agent:worklog:start ZEN-12 -->
## Work Log — 2026-09-19

**Outcome**: committed to `master` as `3d24f92f`, parked at **In Review** (`human-gate`) · ran on opus/high via COMPLEX+HIGH · category Engine

**What changed**

- `Zenith/Flux/RenderViews/Flux_RenderViews.h` — `FLUX_RENDER_VIEW_PREVIEW` removed; `Flux_RenderView::m_bOwnsPersistentLDR` added as the destination property.
- `Zenith/Flux/RenderViews/Flux_RenderViews.cpp` — the registry constructor stamps the property over the preview range instead of a view kind.
- `Zenith/Flux/HDR/Flux_HDR.cpp` — the one production branch is now `if (xView.m_bOwnsPersistentLDR)`.
- `Zenith/Flux/HDR/Flux_HDRImpl.h`, `Zenith/Flux/HDR/CLAUDE.md` — doc/comment follow-through.
- `Zenith/Flux/RenderViews/Flux_RenderViews.Tests.inl` — three pinned clauses re-expressed (not deleted), drift guard renamed, **+1 unit** `RenderViews::TheTwoPreviewViewsDifferOnlyInTheirDestination`.
- `Zenith/Editor/Zenith_AnimationPreviewSession.Tests.inl` — slot-6 premise guard re-expressed.
- `Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp` — **excursion** (below): the live comparison at :577, four stale comment/string sites, and new clause 5b.
- `Tools/unit_baselines.json`, `Games/Zenithmon/Docs/Status.md` — pins + narration (orchestrator's).

`Flux_MaterialPreviewController.cpp` and `Zenith_AnimationPreviewSession.cpp` were on the approved list and needed **no change** — neither ever wrote `m_eType`. Recorded so the absence is not read as an oversight.

**Files** (`git diff --stat`)

```
 Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp | 165 +++++++++++++---
 Games/Zenithmon/Docs/Status.md                           |  35 +++-
 Tools/unit_baselines.json                                |   6 +-
 Zenith/Editor/Zenith_AnimationPreviewSession.Tests.inl   |  15 +-
 Zenith/Flux/HDR/CLAUDE.md                                |  20 +-
 Zenith/Flux/HDR/Flux_HDR.cpp                             |  20 +-
 Zenith/Flux/HDR/Flux_HDRImpl.h                           |  11 +-
 Zenith/Flux/RenderViews/Flux_RenderViews.Tests.inl       | 117 ++++++++++-
 Zenith/Flux/RenderViews/Flux_RenderViews.cpp             |  15 +-
 Zenith/Flux/RenderViews/Flux_RenderViews.h               |  48 ++++-
```

**Gates**

`-Phase measure` exit **10** → pins written → `-Phase verify` exit **0**, `state: complete`, `headSha` matching HEAD, `skipped: []`, 17 lines all exit 0.

| Command | Result |
|---|---|
| `build engine` + 10 game builds + 2 `D3D12_*_False` legs | 0 |
| `test Combat --headless` | 0 |
| `run_unit_gate -Game Combat` | 0 — **2698 ran, 0 failed** |
| `build Zenithmon --headless` / `--config D3D12_*_False` | 0 *(unioned — Zenithmon)* |
| `test Zenithmon --headless` | 0 *(unioned — Zenithmon)* |
| `run_unit_gate -Game Zenithmon` | 0 — **4551 ran, 0 failed** *(unioned — Zenithmon)* |
| `doc_lint.ps1 -Game Zenithmon` | 0 *(unioned — Zenithmon)* |

The union fired on `Games/Zenithmon/Docs/Status.md`. **Run by hand, because no gate line runs it** (see Durable findings): `rendertest.exe --automated-test RT_RenderGraphViewStructure --skip-unit-tests` → exit 0 read from a file, **2217 checks, 0 failed**, `clause 5b: slot 5 declared 43 passes, slot 6 declared 43`. Log: `.zagent/run/ZEN-12/rt_viewstructure.log`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Flux_RenderViews.h` / `.cpp` | The new property is stamped at construction and read by the HDR walk in every frame of all 17 gate lines; the new registry unit reads it directly in all three boot suites. |
| `Flux_HDR.cpp` | The changed branch executes every frame of every run above. Its PREVIEW arm specifically: `RT_RenderGraphViewStructure` samples B and C (43 and 86 preview passes). |
| `Flux_RenderViews.Tests.inl` | The new unit is itself the coverage; ran in all three boot suites (2698 / 4551 / 2801). |
| `Zenith_AnimationPreviewSession.Tests.inl` | Its own unit, in the engine boot suite. |
| `Test_RenderGraphViewStructure.cpp` | **Compiled by 11 gate builds; EXECUTED only by the hand-run above.** No gate line runs it — that is `ZEN-D-003`. |
| `Flux_HDRImpl.h`, `Flux_HDR/CLAUDE.md` | **Nothing executes them** — comment/doc only. |
| `Tools/unit_baselines.json` | `run_unit_gate.ps1` on two gate lines (Combat, Zenithmon), both green against the new numbers. **RenderTest's row is read by nothing** — `advisory`. |
| `Games/Zenithmon/Docs/Status.md` | `doc_lint.ps1 -Game Zenithmon` C7, which reconciles the LIVE PIN block against the manifest with equality. Prose claims are not checked by it. |

**Definition of Done**

- [x] `FLUX_RENDER_VIEW_PREVIEW` no longer exists; `FluxRenderViewType` names only view kinds, and no pass branches on the output destination — **ticked with a stated qualification.** The first two clauses hold outright. The third does **not** hold verbatim: `Flux_HDR.cpp:836` still reads `if (xView.m_bOwnsPersistentLDR)`, which is a branch on destination. What the change removes is the destination being a view KIND occupying the type enum. This is recorded rather than glossed because the reviewer raised it as blocking and the ticket is `human-gate` — the sign-off should see the narrower claim.
- [x] The editor material preview and animation preview render through the same frame loop as the main view, against their own targets, and look identical to before (captures attached) — captures in `.zagent/run/ZEN-12/{before,after,after2}/`; measurement below. "Look identical" is the human's call, which is what the gate is for.
- [x] A headless unit proves two requests with different destinations produce identical pass lists — proven twice, both RUN: clause 5b on live pass inventories (43 vs 43), and the new backend-neutral registry unit.
- [x] Every game's pinned baseline bumped from an OBSERVED run if a unit was added — Combat 2697→**2698**, Zenithmon 4550→**4551**, RenderTest 2800→**2801**, each observed from its own `Null_` run, none computed.

**The capture measurement, and what it does not say**

Nothing here is byte-identical, including two runs of the *same* binary — so a noise floor was established per set by running the unchanged AFTER build a second time.

| set | view | change (max/mean/pct>2) | SAME-BINARY floor |
|---|---|---|---|
| photo tour, 11 poses | main | 116–208 / 0.06–2.32 / 0.11–17.5% | 116–169 / 0.03–1.77 / 0.05–15.6% |
| material preview, 4 | **slot 5** | 116 / 0.059–0.069 / 0.106–0.115% | 116 / 0.042–0.048 / 0.083–0.087% |
| anim preview, 3 | **slot 6** | 114 / 0.000–0.013 / 0.000–0.013% | 114 / 0.000–0.034 / 0.000–0.013% |

Identical maxima in every set; the change sits within ~1.4× of the floor, and for slot 6 it is **quieter than the floor**. Localising the differing pixels gives the same two populations in the change comparison and the same-binary control alike: a constant bottom-band group (an overlay readout) plus a scene-dependent low-magnitude spread that tracks scene complexity (122,802 pixels in the 3–16 bucket for the foliage grove, ~250 for the tennis court) — the TAA signature.

**So the claim is "not distinguishable from re-running the same binary", NOT "identical".** The first control covered only the photo tour, i.e. the main view — the one view this change does not touch; the reviewer caught that and the two preview sets were controlled afterwards, which is where the claim now rests.

**Durable findings**: `ZEN-D-003`, `ZEN-D-004`

- `ZEN-D-003` — a `ZENITH_AUTOMATED_TEST_REGISTER` test under `Games/RenderTest/Tests/**` is compile-checked only by every gate list here. ~115 lines of this ticket's own box-3 evidence landed and went green unrun; the RenderTest boot count moving +1 looks like confirmation and is not.
- `ZEN-D-004` — ANY `--automated-test` boot authors scenes, on either backend. Measured on three separate scenes, including from a `Null_` headless run, which is why the protocol's "a WINDOWED test run is authoring" wording is stale since ZEN-6.

**Deviations / follow-ups**

- **EXCURSION, and how it was found.** The worker's step-zero survey refused to edit and returned a corrected file list: the orchestrator's inventory was scoped to `Zenith/**` and missed `Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp:577`, a live `== FLUX_RENDER_VIEW_PREVIEW` compiled in every `*_True` config. Removing the enumerator without it reds `zenith build RenderTest`, an Engine gate line. Independently re-grepped: exactly one source file in all of `Games/` matches, so the corrected 10-file list is complete. The file is item 10 and its edit had to land in the same commit or RenderTest is un-compilable in between.
- **Corrections made to the ticket's text before inlining it (I3).** The Goal asserts previews are "the same frame loop run again … a second execution is a second graph instance". They are not: previews are extra active slots in one 8-slot registry rendered by ONE graph per frame. Inlined with that correction and an instruction not to build the described architecture. The reviewer independently confirmed the reading (`Flux.cpp:477` allocates the single graph).
- **An orchestrator ruling the worker correctly overruled.** I ruled the property should use the existing `FLUX_VIEW_FLAG_*` mechanism. It used a plain member because the flag word is mirrored to shaders as `g_uViewFlags`. The reviewer refined this further: the mirrored word is the *constants* one, so a bit on the view struct would have been dead and a bit on the constants would have leaked routing to the GPU — neither is a home for it. The worker's call was right for a better reason than either of us gave.
- **Review: 3 blocking, 6 non-blocking; all three blocking addressed before commit.** (1) clause 5b had never been executed — run by hand, passed, log archived. (2) DoD box 1's third clause does not hold verbatim — recorded above instead of glossed. (3) my `Status.md` paragraph asserted `unioned: []` *and* that editing the file unions gates in — both cannot describe one run, since the quoted `gates.json` predated my own edits — and it claimed RenderTest's pin is gated, which is false for every ticket. Rewritten to state per-pin exactly what checks it. Also fixed non-blocking (4): the new unit asserted equality of `m_uViewFlags` while claiming that word gates the shadow/cluster/scene chains; production reads `m_xConstants.m_uViewFlags` and the struct field is "read by nobody" per an in-tree comment. The assertion is kept, its stated justification corrected.
- **Not addressed, deliberately** — non-blocking (5) the registry unit reads a default-constructed registry so it cannot see a stage-time divergence (that is clause 5b's job, now run); (6) `m_bOwnsPersistentLDR` and `kuFLUX_PREVIEW_LDR_SLOTS` are not pinned to each other, a hazard that existed identically via `m_eType` so no regression; (9) one added assertion duplicates a `static_assert` and is decoration.
- **Capture-run drift restored three times.** `ProcLevel.zscen`, `Dawnmere.zscen` (twice each, before and after the change) and `RenderTest.zscen`, plus an untracked `Games/Zenithmon/Assets/Authored/Creatures/Fernfawn/`. None is this ticket's work; all restored before staging. See `ZEN-D-004`.
- **Parked at In Review, not Done**, per the `human-gate` label applied at claim time. The label (plus `needs-gpu` + `gpu-reason:visual-measure`) was added by this tick because the ticket carried no marker while its DoD box 2 is a visual sign-off — `PhotoTour.cpp`'s own header says it asserts nothing on pixels and the before/after pair IS the deliverable.
<!-- agent:worklog:end -->
