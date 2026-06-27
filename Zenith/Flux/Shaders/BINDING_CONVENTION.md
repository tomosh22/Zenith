# Shader Binding Convention — RETIRED (superseded by the enforced frequency spine)

This document used to *describe* the pre-overhaul descriptor model (a set-0 `FrameConstants`
grab-bag, set-1 per-draw + material textures, terrain's 20 texture bindings, `Common.fxh` /
`DrawConstants.fxh`, and "retrofit target for Phase 5" divergences). **That model no longer
exists.** The Flux binding model is now a frequency-based spine that is declared once and
*enforced in code*, so there is nothing to document descriptively here.

The canonical, authoritative sources are:

- **The spine** — `Flux/Shaders/Common/Bindings.slang`: the GLOBAL (set 0) / VIEW (set 1) /
  BINDLESS (set 2) `ParameterBlock`s that every shader `#include`s (textually, first). Each
  shader then declares its own local PASS (set 3) and DRAW (set 4) blocks.
- **Enforcement (build-fail / boot-assert):**
  - `Flux_ShaderCatalog::ValidateFrequencyTaxonomy` (FluxCompiler) — asserts the 0/1/2 spine
    and zero `[[vk::binding]]` in any shader.
  - `Flux_PersistentSetLayouts::ValidateCanonicalGroup` (boot) — asserts the persistent
    GLOBAL/VIEW descriptor-set layout matches the spine.
  - `Flux_ViewSetBinding` — enforces the per-pass graph `Read()` obligation for VIEW/GLOBAL
    graph-tracked resources (so a resource sampled from the persistent set can't skip its
    barrier).
- **Persistent VIEW membership** — `Flux/Flux_PersistentSetLayouts.h` (the C++-authoritative
  manifest, including the "deliberately NOT promoted" rationale for per-pass transients and
  per-camera resources) + `Flux/Flux_ViewSetBinding.cpp` (the registry).

See `Flux/CLAUDE.md` for the renderer overview and each feature's `Flux_<Feature>_Shaders.h`
for its program declarations.
