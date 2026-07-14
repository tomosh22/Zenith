# Zenithmon Status

**Last updated:** 2026-07-14
**Stage:** S4 (Asset generators) -- in progress; S1/S2/S3 COMPLETE (S3 visual gate SIGNED OFF 2026-07-13).
**Build:** passing -- Vulkan_vs2022_Debug_Win64_True green; D3D12_vs2022_Debug_Win64_False null-backend link proof green (the new `Source/Gen/` pure library links in the non-tools config via the bake bridges' no-op `#else`).
**Tests:** boot unit gate **1804 ran / 1803 passed / 0 failed / 1 skipped** (baseline bumped 1773 -> 1804, in `.github/workflows/zm-tests.yml` too); `zenith test Zenithmon --headless` **6 passed / 0 failed**.

## Current task

Next unchecked S4 Roadmap item: **`ZM_CreatureGen`** -- all 8 archetypes (QUADRUPED/BIPED/AVIAN/SERPENT/AQUATIC/INSECTOID/BLOB/FLOATER-PLANTOID), mesh+skeleton via the `ZM_GenCommon` loft, palettes/patterns/shiny/dex-icons via `ZM_TextureSynth`, per species. This is the single biggest S4 work item (~7-8.5k lines) -- plan-first, fan out per archetype for AUTHORING only (builds stay serial on the orchestrator). `ZM_CreatureAnimGen`, `ZM_HumanGen`, `ZM_BuildingGen`/`ZM_PropGen`, and `ZM_BakeManifest` follow. The S4 GATE ends with the windowed species-gallery visual check -- a HARD-STOP for user sign-off (do NOT tick S4 without it).

## Last completed

**S4 box 1 -- `ZM_GenCommon` + `ZM_TextureSynth`** (this commit; ZM-D-059). The asset-gen foundation under `Games/Zenithmon/Source/Gen/`: pure deterministic library (RNG wrapping the golden PCG32, `ZM_GenHashName`/pinned `ZM_GEN_DOMAIN` seed derivation, `ZM_GenNoise`, loft -> POD `ZM_GenMesh`, `ZM_GenImage` + texel synth) compiled in ALL configs; only disk-bake bridges (`ZM_GenBakeMesh`/`ZM_SynthBake*`) are `#ifdef ZENITH_TOOLS` with non-tools no-ops. 31 `ZM_Gen` units. Reviewer verdict SHIP-WITH-FIXES applied in the same landing.

## Notes for next agent

- **Determinism is golden-pinned NOW:** the `ZM_GEN_DOMAIN` enum ordering + `ZM_GenDeriveSeed` fold are frozen (locked by `Seed_DeriveIsStableGolden`); changing them invalidates every future baked asset. Consumers derive seeds via `ZM_GenDeriveSeed(family, speciesId, evoStage, domain)` only.
- **Loft winding is Y-oriented:** rings are horizontal XZ cross-sections swept along `m_fY`; `ZM_EmitAndStitch` flips wall winding per pair when the sweep ascends so `cross(C-A,B-A)` is OUTWARD (repo cull rule). Author creature limbs as Y-monotonic ring chains; equal-Y consecutive rings are a documented degenerate case.
- **Format-doc debt for the S4 GATE (deferred per the at-stage-gate policy):** add the `ZM_Gen` test group + generator catalogue to `Docs/TestPlan.md` and the generated-asset catalogue/budgets to `Docs/AssetManifest.md`, and refresh `Docs/Shortfalls.md`, in the S4 stage-gate commit.
- **Orchestration pattern that worked (this session):** frozen-header design workflow (survey -> 3-architect panel -> synthesis) -> parallel authoring subagents against the frozen headers -> orchestrator builds/gates serially -> reviewer subagent. Keep the design's frozen artifacts under the session scratchpad if iterating.
- **Working model unchanged:** MASTER-ONLY (ZM-D-031); local gate (build + boot unit gate + headless) is the authority; `zm-tests` is the post-push backstop, fix forward on red. Sweep stray `zenithmon.exe` before ending each iteration.
