---
key: ZEN-22
type: TASK
title: Remove const char* szName overloads of Flux_ShaderBinder functions
category: Engine
parent: "ZEN-25"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: "MODERATE"
risk: LOW
storyPoints: 5
position: 18
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: rzbp81ydr07zulz53kepq6vz
  risk: LOW
  type: TASK
  title: Remove const char* szName overloads of Flux_ShaderBinder functions
  number: 22
  flagged: false
  due_date: null
  position: 18
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T22:02:03.289092
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T22:04:07.141
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-21T02:25:44.034Z"
claimAttempt: null
---
## Goal
`Zenith/Flux/Slang/Flux_ShaderBinder.h` still declares the by-name overloads `BindCBV`, `BindSRV`, `BindUAV_Texture`, `BindUAV_Buffer`, `BindSRV_Buffer` and `BindDrawConstants` taking `const char* szName`, resolved at call time through `ResolveNamedBinding(const Flux_ShaderReflection*, const char*)`, and grants `friend class Zenith_UnitTests` so a unit can call the resolver with a synthetic reflection. The binding-model overhaul made `Flux_BindingHandle&` the API: a handle is resolved once from reflection, so a by-name call per bind is both slower and a second way to spell the same thing.

Remove every `const char* szName` overload; migrate any remaining caller in engine or games to a `Flux_BindingHandle` resolved at setup; delete `ResolveNamedBinding` and the `Zenith_UnitTests` friend declaration once nothing needs them, and move whatever the resolver unit proved onto the handle-resolution path so the coverage is not lost. Shader validation (`shader-validation.yml`'s local equivalent in the Flux binding-validation units) is the guard that a binding that used to resolve by name still resolves by handle.

## Definition of Done
- [ ] No `const char*` binding overload remains on `Flux_ShaderBinder`; every engine and game caller binds through `Flux_BindingHandle`
- [ ] `ResolveNamedBinding` and the `Zenith_UnitTests` friend declaration are deleted, and the resolver's unit coverage lives on the handle path
- [ ] Every game builds and its render tests pass; binding validation units green
- [ ] Every game's pinned baseline bumped from an OBSERVED run

## Comments

ran on sonnet/medium via MODERATE+LOW · category Engine

**Done.** Committed to `master` as `a9a24132`. The six `const char* szName` `Bind*` overloads,
`ResolveNamedBinding` and the `friend class Zenith_UnitTests` grant are gone from
`Flux_ShaderBinder`, and all **71** remaining by-name call sites are migrated onto typed
`Flux_BindingHandle` binds — 31 in `Flux_UnifiedMesh.cpp`, 27 in `Flux_Grass_Frame.cpp`, 13 in
`Flux_TAA.cpp`. There were **no game callers** to migrate, despite the header comment claiming
the overloads survived for out-of-tree shaders.

**The one load-bearing decision.** `Flux_GrassImpl::RecordGBuffer` picks its shader at record
time from the velocity latch, and `Grass_ToGBufferVelocity` shifts `BladePool`/`VisibleIndices`/
`TypeParams` one slot relative to `Grass_ToGBuffer` — so it is now two explicit arms rather than
one shared bind list. A single namespace there would have compiled clean and mis-bound three
descriptors. `UnifiedMesh` needed no split; only its two velocity-only handles differ.

**Two ticket-text corrections**, both inlined into the worker prompt: nothing called
`ResolveNamedBinding` except the by-name overloads, so DoD box 2's "move the resolver's unit
coverage" had nothing to move (what was there is a tombstone for an earlier removal); and box 4's
pin is the orchestrator's.

Gates: **15/15 green** on verify, 0 skipped, 0 flakes. Unit gate **2706 ran / 2706 baseline** —
no pin move, observed in both phases. `zagent guard` clean. Zero excursions.

**Read this before trusting the green.** Every gate line runs the `Null_` backend, which issues
no real descriptor writes, so the 15 green gates say nothing about whether these 71 binds are
correct. Three things stand in for that:

* all **75** handle references verified to resolve in the namespace they are used from
  (scope-aware — `US` is aliased to two different namespaces in one file);
* all **67** single-line removed/added pairs keep the same binding name, 0 mismatches; the
  4 multi-line pairs checked by hand;
* **RenderTest built on `Vulkan_vs2022_Debug_Win64_True` with `VK_LAYER_KHRONOS_validation`
  enabled**: `GrassShowcase` PASSED (158 frames), `TAATemporalStability` PASSED (497),
  `HumanShowcase` PASSED (1021) — **zero VUIDs**. That is the only evidence here that exercises
  the descriptor writes at all.

One defect caught in the worker's output and fixed: its rewritten usage example cited
`DS::hPushConstants`, which does not exist in `DeferredShading` — repointed at a namespace that
has it, with a note on why.

Durable findings: `ZEN-D-019` (two handle naming conventions — knowing only `hg_x` resolves 17
of 47 and makes this ticket look unbuildable), `ZEN-D-020` (same handle name, different
set/binding per program namespace; wrong namespace is invisible to every gate).

<!-- agent:worklog:start ZEN-22 -->
## Work Log — 2026-09-21

ran on sonnet/medium via MODERATE+LOW · category Engine

**Outcome**: committed to `master` as `a9a24132` · ran on sonnet/medium via MODERATE+LOW

Removed the six `const char* szName` `Bind*` overloads from `Flux_ShaderBinder`, together with
`ResolveNamedBinding`, its `ResolvedBinding` struct and the `friend class Zenith_UnitTests`
grant, and migrated all 71 remaining by-name call sites onto typed `Flux_BindingHandle` binds
resolved from each program's generated header.

**What changed**

- `Zenith/Flux/Slang/Flux_ShaderBinder.h` / `.cpp` — the six by-name overloads,
  `ResolveNamedBinding` and the friend grant deleted; the class comment and `// Usage:` block
  rewritten to the handle-only API.
- `Zenith/Flux/UnifiedMesh/Flux_UnifiedMesh.cpp` — 31 sites. `BindBucketVAT` now takes a
  `const Flux_BindingHandle&` instead of a `Flux_Shader&`, since its whole job was to be shared
  across three programs.
- `Zenith/Flux/Vegetation/Flux_Grass_Frame.cpp` — 27 sites, plus the two-arm split described
  below.
- `Zenith/Flux/TAA/Flux_TAA.cpp` — 13 sites.
- `Zenith/EngineComposition/Zenith_UnitTests.Tests.inl` — the tombstone comment (which
  described `ResolveNamedBinding` in the present tense) and the top-of-file comment (which
  cited `Flux_ShaderBinder.h` as a live `friend class Zenith_UnitTests` example) both corrected.

**The one load-bearing decision.** `Flux_GrassImpl::RecordGBuffer` chose its shader at record
time (`bVelocity ? m_xGBufferVelocityShader : m_xGBufferShader`) and bound through that
reference. `Grass_ToGBufferVelocity` inserts `hGrassPrevWindConstants` at binding 1 and shifts
`hBladePool` / `hVisibleIndices` / `hTypeParams` up one slot relative to `Grass_ToGBuffer`, so a
single hard-coded namespace would compile clean and mis-bind three descriptors on whichever path
it was wrong for. That block is now two explicit arms with a comment at the seam.
`UnifiedMesh`'s equivalent needed no split — only `hPrevTransforms` and `hPrevSkinnedPosArena`
differ between its two G-buffer programs, which I verified independently of the worker's claim.

**Files** (`git show --stat a9a24132`)

    6 files changed, ~145 insertions(+), ~269 deletions(-)
    Zenith/EngineComposition/Zenith_UnitTests.Tests.inl |  26 ++--
    Zenith/Flux/Slang/Flux_ShaderBinder.cpp             | 104 ---------
    Zenith/Flux/Slang/Flux_ShaderBinder.h               |  76 +------
    Zenith/Flux/TAA/Flux_TAA.cpp                        |  30 ++--
    Zenith/Flux/UnifiedMesh/Flux_UnifiedMesh.cpp        |  96 +++++----
    Zenith/Flux/Vegetation/Flux_Grass_Frame.cpp         |  77 ++++---

**Gates** — `-Phase verify`: `state: complete`, `exitCode: 0`, **15/15 green, 0 skipped,
0 deferred, 0 flakes**, `headSha` matched HEAD. `zagent gates` unioned in nothing (the diff is
entirely inside Engine's own `paths`).

| Command | Result |
|---|---|
| `zenith build engine` | PASS |
| `zenith build Combat --headless` | PASS |
| `zenith build Combat --config D3D12_vs2022_Debug_Win64_False` | PASS — the non-tools compile |
| `zenith build {Zenithmon,CityBuilder,RenderTest,TilePuzzle,DevilsPlayground,ScriptTest,Foundry,Undervault} --headless` | PASS — 8 lines |
| `zenith test Combat --headless` | PASS |
| `run_unit_gate.ps1 -Game Combat` | PASS — **2706 ran / 2706 baseline**, 0 failed, 1 skipped |
| `doc_lint.ps1 -Game Zenithmon` / `-Game DevilsPlayground` | PASS |
| **hand run** `rendertest.exe` on `Vulkan_vs2022_Debug_Win64_True`, `VK_LAYER_KHRONOS_validation` enabled | **`GrassShowcase: PASSED` (158 frames) · `TAATemporalStability: PASSED` (497 frames) · `HumanShowcase: PASSED` (1021 frames) — ZERO VUIDs** |

`zagent guard ZEN-22`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Flux_Grass_Frame.cpp` (27 binds + the velocity split) | **No gate.** Executed by the hand-run `GrassShowcase` on Vulkan with validation layers. |
| `Flux_TAA.cpp` (13 binds) | **No gate.** Executed by the hand-run `TAATemporalStability`. |
| `Flux_UnifiedMesh.cpp` (31 binds, `BindBucketVAT` signature) | **No gate.** Executed by the hand-run `HumanShowcase` (skinned meshes drive the skinning + VAT paths). |
| `Flux_ShaderBinder.h` / `.cpp` (deletions) | Compiled by all 11 gate builds; the surviving handle overloads execute on every frame of every gate, but on the `Null_` backend, which issues no real descriptor writes. |
| `Zenith_UnitTests.Tests.inl` | Comment only. |

★ **Every gate line in this repo runs the `Null_` backend, which writes no descriptors — so
the 15 green gates say nothing about whether these 71 binds are correct.** That is why the
Vulkan validation run above exists, and it is the only evidence in this ticket that the
descriptors are right. Recorded as `ZEN-D-020`.

Two mechanical checks stood in for the review that `review.required: false` did not ask for:

* every one of **75** `Namespace::hIdent` references resolves in the namespace it is used from
  — scope-aware, because `Flux_UnifiedMesh.cpp` legitimately aliases `US` to
  `UnifiedMesh_Skinning` at :927 and to `UnifiedMesh_ToShadowmap` at :1174;
* every **67** single-line removed/added pair keeps the same binding name (0 mismatches); the
  4 multi-line pairs and both `BindBucketVAT` call sites checked by hand.

**Definition of Done**

- [x] No `const char*` binding overload remains on `Flux_ShaderBinder`; every engine and game
      caller binds through `Flux_BindingHandle`. Verified by repo-wide grep: zero
      `Bind*(shader, "name", …)` remain. There were **no game callers** to migrate.
- [x] `ResolveNamedBinding` and the `Zenith_UnitTests` friend declaration are deleted. The
      clause "and the resolver's unit coverage lives on the handle path" had **nothing to
      move** — nothing called the resolver except the by-name overloads themselves (see
      Deviations). The tombstone comment now records that.
- [x] Every game builds and its render tests pass; binding validation units green. All 11
      builds green; `Flux, ValidateStagedBindings` and the rest of the unit suite green
      (2706/2706). RenderTest's own automated tests are compile-only under every gate
      (`ZEN-D-003`), so the three that matter here were run by hand on Vulkan.
- [x] Every game's pinned baseline bumped from an OBSERVED run — **observed: no move.** The
      unit gate reported `2706 ran` against baseline `2706` and passed, in both the measure and
      verify phases. No test was added or removed, so `Tools/unit_baselines.json` is untouched
      and no `Status.md` LIVE PIN narration changed (both `doc_lint` C7 checks green).

**Durable findings**: `ZEN-D-019`, `ZEN-D-020`

- `ZEN-D-019` — the generated headers use two handle naming conventions (`hg_x<Name>` for the
  shared spine, `h<Name>` case-preserved for program-owned bindings). Knowing only the first
  resolves 17 of 47 and makes this ticket look like it needs a codegen change.
- `ZEN-D-020` — the same handle identifier carries different set/binding values in different
  program namespaces; a wrong namespace compiles, passes the kind assertion, passes every gate,
  and is wrong only on a GPU. Code that picks its shader at record time must pick its handles
  the same way.

**Deviations / follow-ups**

- **Zero excursions.** Every change is inside the six files on `filelist.txt`.
- **A defect I introduced-then-caught in the worker's output.** Its rewritten `// Usage:` block
  cited `DS::hPushConstants`, which does not exist in `DeferredShading` (it lives in
  DynamicLights / Particles / SSGI / SSR). Shipping a doc reference that does not resolve, in
  the very change that removes stale docs, would have been poor. I repointed the example at
  `Flux_Generated_SSR::SSR_DenoiseH::hPushConstants` and added a line saying why the scratch-UBO
  example comes from another subsystem — DeferredShading's equivalent is
  `hDeferredShadingConstants`, and only some programs spell theirs `pushConstants`. I verified
  the remaining cited identifiers (`hg_xView`, `hg_xDiffuseTex`) and both friend grants the
  tombstone now cites (`FrameContext.h`, `Flux_RenderGraph.h`) do exist.
- **Corrections I made to the ticket's own text before inlining it**, both stated in the worker
  prompt and both confirmed by the worker on read:
  1. The Goal says the friend grant exists *"so a unit can call the resolver with a synthetic
     reflection"*. The grant existed; no such unit did. What sat at
     `Zenith_UnitTests.Tests.inl:20419` was a tombstone for the *previous* removal (the name
     cache), not a live test. So DoD box 2's "move the coverage" clause was unsatisfiable as
     written, and I told the worker not to fabricate a replacement test to satisfy it.
  2. DoD box 4's unconditional pin bump is the orchestrator's measurement; the worker was told
     not to quote a number, not even an inferred one.
- **A survey error of mine, caught before dispatch.** My first pass grepped only `hg_x` and
  concluded 30 of 47 bindings had no generated handle — i.e. that the ticket needed a
  FluxCompiler codegen change and was unbuildable as filed. That was one step from a scope
  block on a ticket that is entirely mechanical. See `ZEN-D-019`.
- **`BindBucketVAT`'s signature changed** from `(Flux_ShaderBinder&, Flux_Shader&, const
  Flux_AnimationTexture*)` to `(Flux_ShaderBinder&, const Flux_BindingHandle&, …)`. Inside a
  listed file and forced by the migration — the helper existed only to be shared between three
  programs. I checked both call sites and confirmed `hg_xAnimationTex` is `{4,2}` in all three
  UnifiedMesh programs, so neither caller can be wrong.
- **Minor, not acted on:** `Flux_UnifiedMesh.cpp` reuses the alias `US` for two different
  namespaces in two different functions. Legal and correctly scoped, but it means a reader
  grepping `US::` in that file gets two meanings — it is what made my first verification script
  report five false positives. Left alone as out of scope for this ticket.
- **Not checked by anyone:** whether the removed by-name path had any out-of-tree consumer
  (nothing in this repo, but the header claimed it existed for game shaders); performance of the
  migration (it should be strictly faster — a reflection hash lookup per bind is gone); and the
  agde/Android build, which is not in any gate list.
<!-- agent:worklog:end -->
