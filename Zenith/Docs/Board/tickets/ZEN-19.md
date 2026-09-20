---
key: ZEN-19
type: TASK
title: Flux_ShadowsImpl::UpdateShadowMatrices should be called from within Flux_Shadows instead of in SubmitRenderWork
category: Engine
parent: "ZEN-25"
sprint: null
status: "Blocked"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: 3
position: 15
labels: ["blocked-scope"]
fixVersions: []
blocks: []
links: []
source:
  id: uyt3pthedn8wjnbnh2byeen3
  risk: LOW
  type: TASK
  title: Flux_ShadowsImpl::UpdateShadowMatrices should be called from within Flux_Shadows instead of in SubmitRenderWork
  number: 19
  flagged: false
  due_date: null
  position: 15
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T21:46:30.691434
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:48:45.365
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-20T21:38:03.176Z"
claimAttempt: null
---
## Goal
`Zenith_Core.cpp` hoists `g_xEngine.Shadows().UpdateShadowMatrices()` into the main-thread seam ("Stage 2: hoisted UpdateShadowMatrices (pre-gather)") and includes `Flux/Shadows/Flux_ShadowsImpl.h` to do it. The shadow system should own when its matrices update; the frame loop should not reach into a Flux subsystem's implementation header. `Flux_RenderViews.h` records the constraint that makes the hoist exist: cascade views activate where their constants are staged, and the gather that follows needs the matrices already computed.

Find the clean seam: a `Flux_Shadows` entry point called from Flux's own pre-gather step (or the render-view update that already runs before the gather), so `Zenith_Core.cpp` drops the include and the call. **If there is no clean way to do this, add a comment to this ticket saying why and let the human address it. Do not perform a hacky, pointless refactor** (the ticket's own instruction). The proof is that the shadow cascades are unchanged: the CSM units and `RT_PhotoTour`'s shadowed poses render identically.

## Definition of Done
- [ ] `Zenith_Core.cpp` no longer includes `Flux_ShadowsImpl.h` or calls `UpdateShadowMatrices`; the shadow system updates its own matrices at the same point in the frame, or a comment on this ticket records why that is not clean
- [ ] Shadow cascades are unchanged: CSM units green and a photo-tour comparison shows no difference
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added

## Comments

ran on sonnet/low via SIMPLE+RISK LOW · category Engine

**Blocked (`blocked-scope`) with all 15 gate lines green.** Committed to `master` as `bfb44120`;
nothing pushed, so a rejection is one `git revert bfb44120`.

**What landed.** `Zenith_Core.cpp` drops `#include "Flux/Shadows/Flux_ShadowsImpl.h"` and
`#include "Core/Zenith_GraphicsOptions.h"` and calls a new one-line
`Flux_RendererImpl::UpdateShadowCascadeMatrices()` instead; `Flux_ShadowsImpl::UpdateShadowMatrices`
self-gates on `m_bShadowsEnabled` as its first statement, byte-identical to the self-gate
`ExecuteShadowCascade` already carries in the same file. Frame order, cascade fit maths and
observable behaviour are unchanged.

**Why it is not Done.** DoD item 1's second clause — *"the shadow system updates its own matrices
at the same point in the frame"* — is false. The call is still a line in `SubmitRenderWork`, under
a new name, and the entry point belongs to `Flux_RendererImpl` (the renderer facade), not to the
shadow system. The ticket's title is *"should be called from within Flux_Shadows instead of in
SubmitRenderWork"*. The fallback clause is not claimed either, because a clean route does appear to
exist and was not tried.

**What a human needs to rule on** (filed as a Questions row): should `Flux_FeatureRegistry` grow an
optional fifth per-frame trampoline — a main-thread pre-gather hook — so `Shadows` *declares* this
step the way it already declares `Initialise`/`SetupRenderGraph`/`Shutdown`/`BuildPipelines`? That
is the only candidate that makes the shadow system genuinely own *when*, and it changes the
`FluxRenderFeature` concept for every Flux feature — beyond this ticket's SIMPLE/LOW sizing. Note
that even that route leaves one line in `SubmitRenderWork` (a `RunPreGather()` walk), so DoD clause
B may be unsatisfiable as literally worded and may itself need rewording.

**★ Fix the ticket body before re-filing.** Its Goal offers *"or the render-view update that already
runs before the gather"* as an alternative seam. That is factually wrong and implementing it would
be a **regression**: `UploadRenderViewConstants` runs inside render-graph `Execute`, *after* the
Prepare/gather callbacks, so it would hand the unified cull last frame's cascade frusta — exactly
the bug the existing hoist prevents. Recorded as `ZEN-D-014`.

**Evidence for "cascades unchanged".** Not the CSM units — all five are pure flag-bit assertions
that never call `UpdateShadowMatrices` and would pass if it were deleted. Established by inspection
(the gate is above `BeginProfileZone` so no state moves on the disabled path; exactly one caller
repo-wide; shadows-OFF consumers untouched) and corroborated by a **three-run** photo tour whose
residual orders itself by temporal separation between runs (`B-vs-A 0.66 > B-vs-A2 0.50 >
A-vs-A2 0.34`, same shape across all nine shots containing motion) rather than by the code change.
The two motion-free shots are identical in all three pairings.

Full detail, gate table and coverage row in the work log.

<!-- agent:worklog:start ZEN-19 -->
## Work Log — 2026-09-20

ran on sonnet/low via SIMPLE+LOW · category Engine

**Outcome**: committed to `master` as `bfb44120` · **Blocked (`blocked-scope`)** — all 15 gate
lines green, but Definition of Done item 1 is **not met** and the remaining half needs a human
ruling. Nothing is pushed; a rejection is one `git revert bfb44120`.

**What changed**

- `Zenith/EngineComposition/Zenith_Core.cpp` — dropped `#include "Flux/Shadows/Flux_ShadowsImpl.h"`
  and `#include "Core/Zenith_GraphicsOptions.h"`; the call is now an unconditional
  `g_xEngine.FluxRenderer().UpdateShadowCascadeMatrices()` at the identical point in the frame.
- `Zenith/Flux/Flux_RendererImpl.h` / `Zenith/Flux/Flux.cpp` — new one-line
  `Flux_RendererImpl::UpdateShadowCascadeMatrices()` forwarding to
  `g_xEngine.Shadows().UpdateShadowMatrices()`. That TU already included the shadows header.
- `Zenith/Flux/Shadows/Flux_Shadows.cpp` — `UpdateShadowMatrices` self-gates on
  `m_bShadowsEnabled` as its **first statement, above `BeginProfileZone`**, byte-identical to
  `ExecuteShadowCascade`'s existing self-gate at line 194.
- `Zenith/Flux/Flux_Graphics.cpp` — **orchestrator edit, from review finding 4.** Its comment
  asserted `UpdateShadowMatrices` "never runs while shadows are OFF". After this change it *is*
  called and early-outs. The conclusion the comment draws is still correct; the stated premise
  was not. Reworded to "never STAGES".
- `Zenith/Flux/Shadows/CLAUDE.md` — call chain updated, **plus two orchestrator corrections from
  review finding 5**: an invented rationale ("the toggle-OFF side lives in `Flux_Graphics.cpp`
  *for the same reason: the decision belongs to Flux*") was replaced with the actual reason
  (nothing stages cascade slots while shadows are off — a necessity, not a precedent), and a
  standing note added that this section describes an **interim** state, not a settled design.
  The same invented sentence was removed from `Flux_RendererImpl.h`.

**Files** (`git show --stat bfb44120`)

```
 Zenith/EngineComposition/Zenith_Core.cpp | 12 +++++------
 Zenith/Flux/Flux.cpp                     |  9 ++++++++
 Zenith/Flux/Flux_Graphics.cpp            |  9 +++++---
 Zenith/Flux/Flux_RendererImpl.h          | 18 ++++++++++++++++
 Zenith/Flux/Shadows/CLAUDE.md            | 37 +++++++++++++++++++++++++++-----
 Zenith/Flux/Shadows/Flux_Shadows.cpp     | 11 ++++++++++
 6 files changed, 81 insertions(+), 15 deletions(-)
```

No excursions: all six are on `.zagent/run/ZEN-19/filelist.txt`.

**Gates**

`-Phase measure` exit 0 (12 of 15 lines; 3 deferred to verify by design), pin already correct.
`-Phase verify` exit 0, **15/15 lines, `skipped: []`, `state: complete`, `headSha` == HEAD.**
`zagent guard ZEN-19` exit 0, no violations, `docsMirrorNeeded: false`.
`gatesSource: category`, `unioned: []` — the diff is entirely inside `Zenith/**`, which the
Engine category already owns, so nothing was unioned in.

| Command | Result |
|---|---|
| `zenith build engine` | pass |
| `zenith build Combat --headless` | pass |
| `zenith build Combat --config D3D12_vs2022_Debug_Win64_False` | pass |
| `zenith build {Zenithmon,CityBuilder,RenderTest,TilePuzzle,DevilsPlayground,ScriptTest,Foundry,Undervault} --headless` | pass (8 lines) |
| `zenith test Combat --headless` | pass |
| `run_unit_gate.ps1 -Game Combat` | pass — **pin unchanged, no unit added or removed** |
| `doc_lint.ps1 -Game Zenithmon` | pass |
| `doc_lint.ps1 -Game DevilsPlayground` | pass |
| *(extra, not a gate line)* `zenith build RenderTest --config Vulkan_vs2022_Release_Win64_True` | pass ×2 (before/after, for the photo tour) |
| *(extra)* `phototour_run.ps1 -Game RenderTest` ×3 | exit 0 ×3 |

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith/Flux/Shadows/Flux_Shadows.cpp` (new self-gate) | **Nothing in any unit.** All five `FluxShadows` units in `Flux_Shadows.Tests.inl` are pure flag-bit/fold assertions; none calls `UpdateShadowMatrices`, none builds a cascade, and all five would pass if the function were deleted. Executed only by the three windowed `RT_PhotoTour` boots run here. |
| `Zenith/EngineComposition/Zenith_Core.cpp` (rerouted call) | Every boot of every game — compiled and linked by all 11 builds, executed per frame by `zenith test Combat --headless` and the three tour runs. Behaviour not asserted anywhere. |
| `Zenith/Flux/Flux.cpp`, `Zenith/Flux/Flux_RendererImpl.h` (new forwarder) | Same — reached every frame, asserted by nothing. |
| `Zenith/Flux/Flux_Graphics.cpp` | Comment only. |
| `Zenith/Flux/Shadows/CLAUDE.md` | `doc_lint.ps1` (structure only; it reads no assertion in the prose). |

**So "CSM units green" is not evidence for DoD item 2 and was not treated as such.** The
equivalence rests on inspection, corroborated three ways: (a) the self-gate is the first
statement, so the profile zone stays balanced and no counter/dirty-flag/debug-variable read sits
above it — "entered and returned" is byte-for-byte "never entered"; (b) a repo-wide grep finds
exactly one caller, so the gate cannot silently suppress a second ungated one; (c) shadows-OFF
consumers are untouched — `SetViewActive(..., true)` at `Flux_Shadows.cpp:490` still never runs
while off, so `Flux_Graphics.cpp`'s deactivation and the unified cull's active-view mask see
exactly what they saw before.

**Photo tour (DoD item 2, second half) — three runs, and the result is a control, not a vibe**

`Flux/Shadows/CLAUDE.md` warns that a *pair* of tour runs is not a controlled A/B (wind is driven
from process start with a variable boot frame count; the run-to-run noise floor is 0.9–1.3× the
effect being measured). So a third run of the **same post-change binary** was taken to measure
that floor directly. Mean absolute per-pixel difference, cropped viewport:

| shot | B-vs-A | B-vs-A2 | A-vs-A2 |
|---|---|---|---|
| rt_grove_eye | 2.571 | 1.999 | 1.282 |
| rt_player_portrait | 1.784 | 1.487 | 1.047 |
| rt_landscape_eye | 1.570 | 1.273 | 0.881 |
| rt_player_feet | 1.516 | 1.331 | 0.971 |
| rt_player_face | 1.544 | 1.277 | 0.904 |
| rt_grove_canopy | 1.394 | 1.050 | 0.642 |
| rt_material_showcase | 0.729 | 0.599 | 0.416 |
| rt_campus_overview | 0.664 | 0.504 | 0.337 |
| rt_material_closeup | 0.589 | 0.508 | 0.386 |
| rt_tennis_courtside | 0.009 | 0.009 | 0.008 |
| rt_tennis_spectator | **0.000** | **0.000** | **0.000** |

The residual orders itself by **temporal separation between runs** — `B-vs-A > B-vs-A2 > A-vs-A2`
— identically across all nine shots containing moving foliage or animated characters. If the code
change caused it, `B-vs-A` and `B-vs-A2` would be equal (both compare pre-change to post-change)
and `A-vs-A2` would be ~0; it is not. The only two shots with no motion in frame are identical
(`rt_tennis_spectator`, zero in all three pairings) or at the noise floor. **This does not detect
a change and cannot prove absence**; it is corroboration for the inspection above. The instrument
that could settle it is the in-run `--phototour-shadows=ab`, which measures shadows-on vs
shadows-off within a single process and so has no wind-phase confound — but that answers a
different question than "did ZEN-19 alter the cascades", so it was not run.

**Definition of Done**

- [ ] **NOT MET.** `Zenith_Core.cpp` no longer includes `Flux_ShadowsImpl.h` or calls
      `UpdateShadowMatrices` — **that clause is true** (verified: no *header* anywhere in the
      tree includes `Flux_ShadowsImpl.h`, so the complete type really is out of that TU, not just
      the directive). But the second clause — "the shadow system updates its own matrices at the
      same point in the frame" — is **false**. The frame loop still decides *when*, on the same
      line, in the same position; the entry point it calls belongs to `Flux_RendererImpl`, the
      renderer facade, not to the shadow system. The ticket's own title is *"should be called
      from within Flux_Shadows instead of in SubmitRenderWork"*, and it is still called from
      `SubmitRenderWork` under a new name. The fallback clause ("a comment records why that is
      not clean") is **not claimed either**, because a clean route does appear to exist and was
      not tried — see the Questions row.
- [x] Shadow cascades are unchanged — as far as anything here can establish. Evidenced by
      inspection (three independent checks above), corroborated by the controlled three-run photo
      tour. **Explicitly NOT evidenced by "CSM units green"**, which is worth nothing here.
- [x] Every game's pinned baseline bumped from an OBSERVED run if a unit was added — **vacuous**:
      no unit was added or removed. The measure phase observed the Combat count and reported
      "pins already correct"; `Tools/unit_baselines.json` is untouched.

**Durable findings**: `ZEN-D-014` — `Flux_GraphicsImpl::UploadRenderViewConstants` is NOT a
pre-gather seam and must never be used as one; it runs inside render-graph `Execute`, after the
Prepare/gather callbacks. Recorded because **ZEN-19's own Goal proposes it as one**, and acting
on that suggestion would move the cascade matrices after the unified-cull Prepare and reintroduce
the last-frame-frusta bug the existing hoist exists to prevent.

**Deviations / follow-ups**

- **Why Blocked rather than Done, and why `blocked-scope` rather than a failure block.** The
  gates are green and the code is safe; what is unmet is the ticket's *contract*. The ticket is
  also not buildable as filed: its Goal names an alternative seam that does not exist
  (`ZEN-D-014`). Per the protocol, a scope block is one where re-running changes nothing — that
  is this. The `tick_gates.json` verdict is exit 0, so the label is permitted.
- **Corrections made to the ticket's own text before inlining it (I3).** The Goal's parenthetical
  *"or the render-view update that already runs before the gather"* was **not** passed to the
  worker as a suggestion; the prompt instead carried the verified ordering constraint and an
  explicit instruction that the frame order must not move. The ticket body still carries the
  false sentence — deliberately left for the human who will re-scope it, and flagged in the
  Questions row rather than edited under them mid-ruling.
- **The `Engine` category declares no `decisionLog`/`decisionIdPrefix`** in
  `zagent.project.json`, so I3's "grep the decision log for the ticket's nouns" had nothing to
  grep. The rulings that actually govern this ticket live in `Zenith/Flux/Shadows/CLAUDE.md` and
  in code comments; they were collected by hand into `.zagent/run/ZEN-19/rulings.md` and inlined
  verbatim. Already tracked as ZEN-82 / ZEN-96.
- **Pre-existing, out of scope, not fixed**: `Zenith/Flux/Shadows/Flux_Shadows.cpp` lines 4–5
  include `Flux/Shadows/Flux_ShadowsImpl.h` twice. Harmless under `#pragma once`; predates this
  ticket.
- **Environment**: `Tools/phototour_crop.py` needs Pillow and it was not installed on this
  machine — the first tour captured all 11 shots, reported `DONE`, and then exited 1 in the crop
  step. `python -m pip install pillow` fixed it; the captures were re-cropped without re-running
  the tour. Nothing in the repo declares this dependency.
- **Review**: dispatched (`review.required: true`) and returned one BLOCKING plus four advisory
  findings. Both of its load-bearing factual claims were independently re-verified before being
  acted on (I2): the `UploadRenderViewConstants` call chain, and the `Flux_FeatureRegistry`
  trampoline shape. Findings 4 and 5 were applied in this commit; finding 1 is the block above;
  finding 3 became `ZEN-D-014`; finding 6 is the duplicate include, left alone.
<!-- agent:worklog:end -->
