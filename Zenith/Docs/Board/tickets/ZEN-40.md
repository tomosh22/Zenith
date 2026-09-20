---
id: "4e8e8310-4a0c-47ae-8e05-eaedf2847a83"
key: "ZEN-40"
type: "BUG"
title: "Teardown scratch overflow makes every windowed run exit non-zero"
category: "Engine"
parent: "ZEN-25"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "HIGH"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 40
labels: ["tech-debt"]
fixVersions: []
blocks: []
updatedAt: "2026-09-20T02:15:49.779Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
`Docs/design/Photorealism.md` §4: *"The teardown scratch overflow (see §0). Cosmetic, but it makes every windowed run exit non-zero."* A windowed run that always exits non-zero is not cosmetic for a gate: every windowed verification (`zenith test <Game>` windowed, `phototour_run.ps1`, the `Vulkan_*_True` authoring boots the ZM tickets require) has to be read by its log rather than its exit code, which is the fail-open shape this repo keeps paying for (`zenith build` once reported success for an exe that could not start).

Find the overflow: which scratch allocator or arena is exceeded at shutdown, by which teardown order (asset handles releasing after the registry, or a render-resource release after the memory manager, in the pattern `d45bd7b3` fixed for handles). Fix the ORDER or the budget at its source, never by widening the exit-code check. Add a unit that drives the teardown sequence headless where the allocator is backend-neutral, and confirm the windowed run exits zero on RenderTest and Zenithmon.

## Definition of Done
- [ ] A windowed `Vulkan_vs2022_Debug_Win64_True` run of RenderTest and Zenithmon exits zero with no teardown overflow in the log
- [ ] The cause is fixed at its source and a headless unit guards the teardown order or budget
- [ ] `Docs/design/Photorealism.md` §0 and §4 drop the note
- [ ] Every game's pinned baseline bumped from an OBSERVED run

## Comments

ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**Done** — merged to `master` as `a1f7f1dd`. All four Definition of Done items met.

`Flux_PrimitivesImpl::ExecuteGBuffer` drained and `Clear()`d its six debug instance queues only inside `if (bDebugPrimitivesEnabled)`, and its early return ANDed the same flag in — so a disabled non-empty queue returned without clearing at all. A photo tour disables the option for its whole duration, so the queues grew unboundedly and the whole backlog recorded in one frame on restore: one `BindDrawConstants` per instance at 80 B → 128 B aligned, against a 131072 B per-worker Vulkan scratch partition, i.e. overflow at exactly 1024 draws. New `ConsumeFrameBoundary` consumes the frame boundary unconditionally, mirroring `Flux_TextImpl::DiscardPendingFrame`. The Vulkan allocator is the detector, not the defect, and is untouched.

Measured, `Vulkan_vs2022_Debug_Win64_True`: `RT_PhotoTour` **`0x80000003` → 0**, `ZM_PhotoTour_Test` **0**, no overflow in either log.

Gates: `-Phase verify` **all green**, `skipped: []`. Pins all OBSERVED from `Null_` runs, +3 each (backend-neutral engine unit): Combat 2703→2706, Zenithmon 4556→4559, RenderTest 2806→2809.

**Three things the ticket got wrong**, all recorded in the work log:

1. **The central claim is false.** "Every windowed run exits non-zero" — measured, `RT_GraphsValidateClean` and `ZM_BattleRoundTrip_Test` both exited 0 before the fix. Only the photo tours reproduced it. The edited docs say the accurate thing; the ticket title and its board mirror still do not.
2. **The proposed mechanism is wrong.** The Goal directed the work at a teardown-ORDER defect "in the pattern `d45bd7b3` fixed for handles". No such defect exists; it is an accumulation bug two subsystems away, in `Flux/Primitives`. This cost one full worker dispatch — the first worker correctly made zero edits and reported a corrected file list.
3. **DoD item 2 hedged "where the allocator is backend-neutral"**, expecting the Vulkan allocator, which is not. The real defect is CPU-side `Zenith_Vector` bookkeeping, so a genuinely headless unit was possible after all: three added, in `Zenith/Flux/Primitives/Flux_Primitives.Tests.inl`.

**Review** (risk MEDIUM) returned one BLOCKING finding and it was the orchestrator's, not the worker's: the Status.md narration claimed `doc_lint` does not reconcile the LIVE PIN against `unit_baselines.json`. It does — `Check-LivePin`, Check 7. Corrected.

**Durable findings: ZEN-D-005, ZEN-D-006.** (1) A frame-local render queue gated on a display option must be drained unconditionally — `Flux_Text` and `Flux_Quads` each get this right by different routes, `Flux_Primitives` got it wrong. (2) `doc_lint`'s LIVE-PIN check (C7) is unreachable from Engine-category tickets, which are precisely the ones that move every game's pin; the LIVE PIN was +5 stale since ZEN-12 as a result.

**A CLI defect found on the way.** `zagent decide` does not mint a decision ID — the caller must supply `ID` in the JSON body, and nothing says so. Its usage line advertises `--text`, which is rejected outright; a body without `ID` is accepted with exit 0 but stored with `ID: null`, and **five of the seven pre-existing decision rows are null-ID for exactly this reason**. Because `finish` requires `Durable findings` to be `none` or a `PROJ-D-N` reference, a tick that records a real finding without knowing this cannot finish at all — it refuses with exit 4, which is what happened here on the first attempt. Three orphan rows were written before the convention was worked out; decisions are append-only, so they remain.

<!-- agent:worklog:start ZEN-40 -->
## Work Log — 2026-09-20

ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**Outcome**: merged to `master` as `a1f7f1dd` · ran on sonnet/medium via MODERATE+MEDIUM

**What changed**

- `Zenith/Flux/Primitives/Flux_Primitives.cpp` — new `ConsumeFrameBoundary`, which `Clear()`s all six debug queues and both gameplay queues on EVERY call and copies to the render locals only when the option is on; `ExecuteGBuffer` now calls it unconditionally and early-returns on the post-drain locals.
- `Zenith/Flux/Primitives/Flux_PrimitivesImpl.h` — declares `ConsumeFrameBoundary` and the test-only `GetDebugInstanceQueueSizeForTesting`.
- `Zenith/Flux/Primitives/Flux_Primitives.Tests.inl` — NEW. Three headless `ZENITH_TEST`s (`FluxPrimitives`), included from the bottom of `Flux_Primitives.cpp`.
- `Zenith/Flux/Primitives/CLAUDE.md` — the accumulation gap is documented as fixed rather than open.
- `Tools/unit_baselines.json` — three pin bumps (below).
- `Games/Zenithmon/Docs/Status.md` — LIVE PIN + narration.
- `Tools/phototour_run.ps1` — the single-fault forgiveness removed.
- `Docs/design/Photorealism.md`, `Games/RenderTest/CLAUDE.md`, `Games/Zenithmon/Docs/TestPlan.md` — stop documenting a forgiven, now-fixed fault.

**The defect.** `ExecuteGBuffer` drained and `Clear()`d the six debug instance queues only inside `if (bDebugPrimitivesEnabled)`, and its early return ANDed the same flag into `bHaveDebugPrimitives`, so a disabled non-empty queue returned without clearing at all. A photo tour disables the option for its whole duration (`Games/RenderTest/Tests/PhotoTour.cpp:353-354`, restored at :572), so the queues grew unboundedly and the entire backlog recorded in one frame on restore. `EmitPrimitiveDraw` issues one `BindDrawConstants` per instance at 80 B, rounded to the device's `minUniformBufferOffsetAlignment` (128 here), against a 131072 B per-worker scratch partition — overflow at exactly 1024 draws. The Vulkan allocator is the DETECTOR, not the defect, and is untouched.

**Files** (`git diff --stat`)

```
 Docs/design/Photorealism.md                       |  26 +--
 Games/RenderTest/CLAUDE.md                        |  23 ++-
 Games/Zenithmon/Docs/Status.md                    |  49 ++++++
 Games/Zenithmon/Docs/TestPlan.md                  |  21 ++-
 Tools/phototour_run.ps1                           |  48 ++---
 Tools/unit_baselines.json                         |   6 +-
 Zenith/Flux/Primitives/CLAUDE.md                  |  68 +++++---
 Zenith/Flux/Primitives/Flux_Primitives.Tests.inl  | 142 ++++++++++++++++
 Zenith/Flux/Primitives/Flux_Primitives.cpp        | 114 ++++++++----
 Zenith/Flux/Primitives/Flux_PrimitivesImpl.h      |  27 +++
 10 files changed, 412 insertions(+), 114 deletions(-)
```

**Gates**

`-Phase measure` 8m17s exit 10 (pin bump) · `-Phase verify` 9m57s **exit 0, all gates green**, `skipped: []`, `headSha` matching HEAD. Gate list = Engine's own, plus the Zenithmon category unioned in by `zagent gates` because the diff touches `Games/Zenithmon/**`.

| Command | Result |
|---|---|
| `zenith build engine` | ok |
| `zenith build Combat --headless` | ok |
| `zenith build Combat --config D3D12_..._False` | ok |
| `zenith build Zenithmon --headless` | ok |
| `zenith build CityBuilder --headless` | ok |
| `zenith build RenderTest --headless` | ok |
| `zenith build TilePuzzle --headless` | ok |
| `zenith build DevilsPlayground --headless` | ok |
| `zenith build ScriptTest --headless` | ok |
| `zenith build Foundry --headless` | ok |
| `zenith build Undervault --headless` | ok |
| `zenith test Combat --headless` | ok (15.9s) |
| `run_unit_gate -Game Combat` | ok (2706) |
| `zenith build Zenithmon --config D3D12_..._False` | ok *(unioned: Zenithmon)* |
| `zenith test Zenithmon --headless` | ok (237.4s) *(unioned: Zenithmon)* |
| `run_unit_gate -Game Zenithmon` | ok (4559) *(unioned: Zenithmon)* |
| `doc_lint -Game Zenithmon` | ok *(unioned: Zenithmon)* |

Windowed verification, `Vulkan_vs2022_Debug_Win64_True`, measured before and after:

| Run | Before | After |
|---|---|---|
| `rendertest --automated-test RT_PhotoTour --skip-unit-tests` | `-2147483645` (`0x80000003`), overflow in log | **0**, no overflow, 11 shots |
| `zenithmon --automated-test ZM_PhotoTour_Test --skip-unit-tests` | n/a | **0**, no overflow |
| `rendertest RT_GraphsValidateClean` | 0 | — |
| `zenithmon ZM_BattleRoundTrip_Test` | 0 | — |

**Pins** — all three OBSERVED from `Null_` runs, none computed. A backend-neutral engine unit moves every game's row.

| Row | Before | After | How |
|---|---|---|---|
| Combat | 2703 | 2706 | MEASURED, `-Phase measure` |
| Zenithmon | 4556 | 4559 | MEASURED, `-Phase measure` (unioned gate) |
| RenderTest | 2806 | 2809 | MEASURED by hand — `advisory`, no gate runs this row |

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith/Flux/Primitives/Flux_Primitives.cpp` | all three new `FluxPrimitives` units drive `ConsumeFrameBoundary` directly; `ExecuteGBuffer`'s call site is exercised at runtime by both windowed photo tours |
| `Zenith/Flux/Primitives/Flux_PrimitivesImpl.h` | same three units (declarations only) |
| `Zenith/Flux/Primitives/Flux_Primitives.Tests.inl` | is the test; confirmed reached — the unit count moved +3 on all three games |
| `Tools/unit_baselines.json` | read by both unit gates, which passed at the new values |
| `Games/Zenithmon/Docs/Status.md` | `doc_lint -Game Zenithmon` Check 7 reconciles the LIVE PIN against the manifest — ran and passed |
| `Games/Zenithmon/Docs/TestPlan.md` | `doc_lint -Game Zenithmon` (link/claim checks) |
| **`Tools/phototour_run.ps1`** | **NOTHING.** No gate line runs this script, and the before/after tours above invoked the exe directly, not the wrapper. Its exit-code branch is verified only by a PowerShell parse check and by reading — the removed `$knownTeardown` has no dangling reference and `$exitCode` still flows to `exit $exitCode`. Unexecuted. |
| **`Docs/design/Photorealism.md`**, **`Games/RenderTest/CLAUDE.md`**, **`Zenith/Flux/Primitives/CLAUDE.md`** | **NOTHING.** `doc_lint` is invoked `-Game Zenithmon` only; no gate lints engine-level docs or RenderTest's. |

**Definition of Done**

- [x] A windowed `Vulkan_vs2022_Debug_Win64_True` run of RenderTest and Zenithmon exits zero with no teardown overflow in the log — measured, table above.
- [x] The cause is fixed at its source and a headless unit guards the teardown order or budget — `ConsumeFrameBoundary`, plus three backend-neutral units. The ticket's Goal hedged this with "where the allocator is backend-neutral", expecting the Vulkan allocator; the real defect is CPU-side `Zenith_Vector` bookkeeping, so a genuinely headless unit was possible.
- [x] `Docs/design/Photorealism.md` §0 and §4 drop the note.
- [x] Every affected game's pinned baseline bumped from an OBSERVED run — all three, table above.

**Durable findings**: ZEN-D-005, ZEN-D-006

- **ZEN-D-005 — a frame-local render queue gated on a display option must be drained UNCONDITIONALLY.** `Flux_Text` gets this right via `DiscardPendingFrame`; `Flux_Quads` gets it right by gating submission instead; `Flux_Primitives` did neither. The option controls what is DRAWN, never whether the backlog is RETIRED. Corollary: the Vulkan scratch allocator resets only in `BeginFrame()`, so it detects this class of bug rather than causing it — raising `uSCRATCH_BUFFER_SIZE` only moves the number in the assert, which `Photorealism.md` had already observed (1→4→16 MB) without finding the producer.
- **ZEN-D-006 — `doc_lint.ps1` Check 7 (`Check-LivePin`) reconciles Status.md's LIVE PIN against `unit_baselines.json`, but no Engine-category gate ever runs `doc_lint`, for any game.** So the check written to catch pin-narration drift is unreachable from exactly the tickets most likely to cause it — a backend-neutral engine unit moves every game's row. Measured here: the LIVE PIN was +5 stale (4551 vs 4556) since ZEN-12. This tick ran C7 only because its diff happens to touch `Games/Zenithmon/Docs/`. Not fixed here: `zagent.project.json` is a protected path.

**Deviations / follow-ups**

- **Files changed outside `.zagent/run/ZEN-40/filelist.txt`, deliberately and explained.** `filelist.txt` records the FIRST dispatch's list, which was scoped to `Zenith/Vulkan/**` from the ticket's own framing. The defect is not there. The first worker made zero edits and reported a corrected list; the second dispatch used it. The six paths outside the recorded list are `Zenith/Flux/Primitives/Flux_Primitives.cpp`, `Zenith/Flux/Primitives/Flux_PrimitivesImpl.h`, `Zenith/Flux/Primitives/Flux_Primitives.Tests.inl`, `Zenith/Flux/Primitives/CLAUDE.md`, `Tools/unit_baselines.json` (the pin, which is the orchestrator's by protocol) and `Games/Zenithmon/Docs/Status.md` (the narration, likewise). No file in `Zenith/Vulkan/**` was touched at all.
- **The ticket's central claim was false and was corrected before dispatch.** Title and `Photorealism.md` §4 both said the overflow made "every windowed run" exit non-zero. Measured: `RT_GraphsValidateClean` and `ZM_BattleRoundTrip_Test` exited 0 before the fix; only the tours reproduced it. The corrected statement was inlined into the worker prompt as measured evidence and is what the edited docs now say.
- **The ticket's proposed MECHANISM was also wrong.** The Goal directed the worker to a teardown-ORDER defect "in the pattern `d45bd7b3` fixed for handles". There is no ordering defect; it is an accumulation bug two subsystems away. This cost one full dispatch. Nothing in the tick protocol checks a ticket's proposed diagnosis — the drift resolver checks that citations resolve, not that they are right.
- **`build_prompt.ps1` did not emit the blast-radius survey clause** the protocol mandates as the worker's first instruction (open ticket ZEN-89, reproduced). It was prepended by hand for both dispatches. It is what produced the corrected file list.
- **One BLOCKING review finding, and it was the orchestrator's, not the worker's.** The first draft of the Status.md narration asserted that `doc_lint` does not compare the LIVE PIN against the manifest. False — `Check-LivePin` does exactly that. A fabricated mechanism, written without opening the file. Corrected, and the real reason (gate selection) recorded as durable finding 2. Two non-blocking findings also applied: the 128 B alignment is a runtime device property, not a constant, and two docs said "hundreds" of draws where the arithmetic gives 1024.
- **Pre-existing, not fixed here:** `Docs/Roadmap_Engine.md:52` and `Zenith/Docs/Board/tickets/ZEN-40.md` still carry the ticket's original false title. Both are board-generated artifacts; editing them by hand would fight the board's own sync.
- **Pre-existing, not fixed here:** a windowed Zenithmon boot rewrites `Games/Zenithmon/Assets/Scenes/Dawnmere.zscen` to the same entity count and byte count with different content. It happened on two separate boots this tick and was restored with `git checkout --` both times; it is unrelated to this change and is not in this commit.
- **`Flux_PrimitivesImpl::Clear()` remains unwired** (no production caller) — pre-existing, noted by the worker, left alone.
- **`zagent decide` does not mint a decision ID; the caller must supply one, and nothing says so.** Its usage message says only `Pass --file <path> or --text "…"`, `--text` is then rejected outright ("needs a JSON body"), and a `--file` whose JSON omits `ID` is accepted with exit 0, a `rowId` and a commit — but with `ID: null`. Five of the seven pre-existing rows in the decisions database are null-ID for exactly this reason. Since `zagent finish` requires the work log's `Durable findings` line to be `none` or a `PROJ-D-N` reference, a tick that records a real finding without knowing to pass `ID` cannot finish at all: it refuses with exit 4. Reproduced here — three orphan rows were written before the convention was worked out (rowIds `5b0539bf…`, `0d1f9fca…`, `e83238ae…`); the authoritative rows are ZEN-D-005 and ZEN-D-006, written with explicit IDs. Decisions are append-only, so the orphans remain. Worth a Suggestions row against the CLI: either mint the next `<PROJECT>-D-N` automatically or refuse a body with no `ID`.
<!-- agent:worklog:end -->
