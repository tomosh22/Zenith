---
id: "ce18f8ec-8319-4ade-8bf4-038b1f2d184a"
key: "ZEN-41"
type: "TASK"
title: "FluxCompiler lint: reject a raw normal-map decode outside Common/Material.slang"
category: "Engine"
parent: "ZEN-25"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 41
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T03:28:35.174Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
`Docs/design/Photorealism.md` §4: *"Nothing pairs a texture's exported compression with the decode its shader assumes. §1.10 was exactly that gap"* (BC5 normal maps decoded as three channels cost every terrain shadow, with all five heuristic units green). The decode now has one site (`UnpackNormalMapTS` in `Common/Material.slang`) and the encode is declared per texture in `TextureUsage.ztexdecl`, so the two can only disagree if somebody open-codes a sample again. Nothing mechanically stops that.

Add the FluxCompiler lint the document proposes: reject a raw `Sample(...).xyz * 2.0 - 1.0` (and the equivalent `* 2 - 1` spellings on a two- or three-component swizzle) anywhere outside `Common/Material.slang`, in the same shape as the existing spine lint in `Tools/` (`shader-validation.yml` runs the local script; the lint itself lives under `Tools/`, unprotected). Self-tests: a fixture shader with the open-coded decode fails, `Material.slang` passes, and a shader that calls `UnpackNormalMapTS` passes. Run it over every shipped `.slang` and fix any hit at its source.

## Definition of Done
- [ ] The lint rejects an open-coded normal decode outside `Common/Material.slang`, with self-tests for both directions
- [ ] Every shipped shader passes it, hits fixed at source
- [ ] `Docs/design/Photorealism.md` §4 drops the item and `Zenith/Flux/Shaders/CLAUDE.md` names the rule
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added

## Comments

ran on sonnet/low via SIMPLE+LOW · category Engine

**Done.** Committed to `master` as `8d77c948`. `Flux_NormalDecodeLint` is a pure text
scanner in the same shape as `Flux_SpineLint`, wired into the FluxCompiler walk beside it,
rejecting an open-coded `Sample|SampleLevel|Load(...).<xy|xyz|rg|rgb> * 2 - 1` normal decode
anywhere outside `Common/Material.slang`. Twelve units, both directions.

**The design point:** `x * 2.0 - 1.0` is overwhelmingly the UV→NDC idiom here — ~25
legitimate uses across Text, SSAO, IBL, Fog, Quads, Skybox and `Bindings.slang` — so a rule
matching the arithmetic would have failed the build in a dozen files. None involves a texture
sample, so the discriminator is the sample-plus-swizzle prefix, and the suite has negative
units for that class, not positive ones only.

Two genuine hits, both in `Common/MaterialSurface.slang`, routed through
`UnpackNormalMapTS`. Both were already `.xy`-only, so this is de-duplication, not a
rendering fix.

Gates: **18/18 green** on verify, 0 skipped, 0 flakes — 15 lines plus the Zenithmon category
unioned in on `Status.md`, which independently validated that game's pin and the LIVE PIN
narration. `zagent guard` clean.

**Pins +12 on all three rows, each OBSERVED from its own `Null_` run**, per the manifest's
rule that a computed pin is how a suite that also lost a test still ratchets green: Combat
2706 → **2718**, Zenithmon 4559 → **4571**, RenderTest 2809 → **2821**, all 0 failed.

**No gate runs FluxCompiler**, so nothing in the gate list executed the lint's walk. Two hand
runs stand in: 100 shaders scanned with 0 violations, and — because a clean result is
otherwise indistinguishable from a lint that matches nothing — an anti-vacuity probe
(an open-coded decode appended to a real shader) that made it exit 1 naming the file and
line. The probe also confirmed the allowlist discriminates `MaterialSurface.slang` from
`Common/Material.slang`. Recorded as `ZEN-D-021`.

Three ticket citations did not hold and were corrected before dispatch: the spine lint is not
in `Tools/`, `shader-validation.yml` runs FluxCompiler rather than a script, and DoD box 3's
`Zenith/Flux/Shaders/CLAUDE.md` **does not exist** — the rule's home is `SHADER_STYLE.md`,
which already carried it.

Four regenerated `.spv` are in the commit. Consecutive runs are byte-stable, so the output is
deterministic and the committed versions were stale; one of them
(`Flux_UnifiedMesh_Reset.csMain.spv`) includes only `Bindings.slang` and so was stale before
this ticket. One excursion: a comment on `Flux_SpineLint.h`'s `Detail` namespace, which now
has two consumers.

Durable finding: `ZEN-D-021`.

<!-- agent:worklog:start ZEN-41 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Engine

**Outcome**: committed to `master` as `8d77c948` · ran on sonnet/low via SIMPLE+LOW

Added `Flux_NormalDecodeLint` — a pure text scanner in the same shape as
`Flux_SpineLint`, wired into the FluxCompiler walk beside it — rejecting an open-coded
`Sample|SampleLevel|Load(...).<xy|xyz|rg|rgb> * 2 - 1` normal decode anywhere outside
`Common/Material.slang`.

**What changed**

- `Zenith/Flux/Slang/Flux_NormalDecodeLint.h` (new) — the scanner: `ScanSource`,
  `IsMaterialSlangFile`, `RuleName`, `Violation`/`Rule`. Device-free and I/O-free, so it
  is both unit-testable and callable from the FluxCompiler walk.
- `Zenith/Flux/Slang/Flux_NormalDecodeLint.Tests.inl` (new) — 12 units covering both
  directions, including the false-positive class explicitly.
- `Zenith/Flux/Slang/Flux_ShaderCatalog.cpp` — hosts the new `.inl` beside the spine-lint
  tests.
- `FluxCompiler/FluxCompiler.cpp` — a second lint-walk block after the spine-lint one.
- `Zenith/Flux/Shaders/Common/MaterialSurface.slang` — the two genuine hits routed through
  `UnpackNormalMapTS`.
- `Docs/design/Photorealism.md`, `Zenith/Flux/Shaders/SHADER_STYLE.md` — the gap closed and
  the rule recorded as enforced.
- `Zenith/Flux/Slang/Flux_SpineLint.h` — one comment. **Excursion, see below.**
- `Tools/unit_baselines.json`, `Games/Zenithmon/Docs/Status.md` — the pin, mine.
- Four regenerated `.spv`. **See below.**

**The design point.** `x * 2.0 - 1.0` is overwhelmingly the UV→NDC idiom in this codebase —
~25 legitimate uses across Text, Grass, Particles, SSAO, IBL, Quads, Fog, Skybox and
`Bindings.slang`. A rule matching the arithmetic would have failed the build in a dozen
files at once. None of those involves a texture sample, so the discriminator is the
**sample-plus-swizzle prefix**, and the suite has negative units for that class rather
than positive ones only.

**Gates** — `-Phase verify`: `state: complete`, `exitCode: 0`, **18/18 green, 0 skipped,
0 deferred, 0 flakes**, `headSha` matched HEAD. `zagent gates` **unioned in the Zenithmon
category** (matched on `Games/Zenithmon/Docs/Status.md`), adding the Zenithmon build,
`D3D12_*_False` compile, test run, unit gate and `doc_lint` — 15 lines became 18. That
union is what independently validated the hand-observed Zenithmon pin and the LIVE PIN
narration; I did not have to trust my own measurement for that row.

| Command | Result |
|---|---|
| `zenith build engine` + 10 game builds `--headless` | PASS |
| `zenith build Combat --config D3D12_vs2022_Debug_Win64_False` | PASS — the non-tools compile |
| `zenith build Zenithmon --config D3D12_vs2022_Debug_Win64_False` | PASS — unioned in |
| `zenith test Combat --headless` / `zenith test Zenithmon --headless` | PASS |
| `run_unit_gate.ps1 -Game Combat` | PASS — **2718 ran / 2718 baseline**, 0 failed |
| `run_unit_gate.ps1 -Game Zenithmon` | PASS — **4571 ran / 4571 baseline**, 0 failed (unioned in) |
| `doc_lint.ps1 -Game Zenithmon` / `-Game DevilsPlayground` | PASS — C7 reconciled the LIVE PIN block |
| **hand run** `fluxcompiler.exe` (`Vulkan_vs2022_Debug_Win64_True`) | **`Normal-decode lint: scanned 100 .slang files, 0 violation(s)`** |
| **hand run** same, with an open-coded decode appended to a real shader | **exit 1** — `ERROR [normal-decode] …/MaterialSurface.slang(259): open-coded normal decode …` |

`zagent guard ZEN-41`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Flux_NormalDecodeLint.h` | The 12 new units (in-process), **and** the FluxCompiler walk in the two hand runs above. No gate line runs the walk — see `ZEN-D-021`. |
| `Flux_NormalDecodeLint.Tests.inl` | The unit gate, in all three games (+12 each). The `.inl`-is-included tripwire in `tick_gates.ps1` confirmed it compiles rather than silently vanishing. |
| `FluxCompiler/FluxCompiler.cpp` | **Compile-only under every gate.** Executed solely by the two hand runs. |
| `Flux_ShaderCatalog.cpp` | Compiled by all 11 builds; the include is exercised by the unit gate. |
| `MaterialSurface.slang` | Compiled to SPIR-V by the FluxCompiler runs; **not executed** — no gate renders. The change is arithmetically identical to what it replaced (see below). |
| `Flux_SpineLint.h` | Comment only. |
| `Photorealism.md`, `SHADER_STYLE.md`, `Status.md` | Docs. `doc_lint -Game Zenithmon` reads the LIVE PIN block; nothing reads the other two. |
| The four `.spv` | Regenerated, byte-stable across consecutive runs; not executed by any gate. |

★ **The shader edits are behaviour-preserving by inspection, not by test.**
`UnpackNormalMapTS` is `xSampled.xy * 2.0 - 1.0` then
`float3(xXY, sqrt(saturate(1 - dot(xXY,xXY))))` — the same arithmetic the two
`MaterialSurface.slang` sites spelled inline. Both were already `.xy`-only, so neither had
the three-channel §1.10 bug. This is de-duplication, and nothing in the gate list renders a
pixel to confirm it.

**Definition of Done**

- [x] The lint rejects an open-coded normal decode outside `Common/Material.slang`, with
      self-tests for both directions — 12 units: positive (2- and 3-component, `rg`,
      `SampleLevel`, `Load`, whitespace/newline tolerance, line-number accuracy) and
      negative (the `Material.slang` exemption, an `UnpackNormalMapTS` caller, the
      five-shape UV→NDC class, comment immunity, string immunity, and an
      `IsMaterialSlangFile` unit asserting `MaterialSurface.slang` does **not** match).
- [x] Every shipped shader passes it, hits fixed at source — FluxCompiler scanned all 100
      `.slang` files with 0 violations; the two hits found on introduction were fixed.
- [x] `Docs/design/Photorealism.md` §4 drops the item, and the rule is named in
      `Zenith/Flux/Shaders/SHADER_STYLE.md`. **The ticket cited
      `Zenith/Flux/Shaders/CLAUDE.md`, which does not exist** — see Deviations.
- [x] Every game's pinned baseline bumped from an OBSERVED run — Combat 2706 → **2718**,
      Zenithmon 4559 → **4571**, RenderTest 2809 → **2821**. **All three observed from
      their own `Null_` run, none computed**, per the manifest's own rule that a computed
      pin is how a suite that also lost a test still ratchets green. Combat came from
      `-Phase measure`; Zenithmon and RenderTest by hand, because the Engine gate list
      carries only the Combat row. Every run reported 0 failed.

**Durable findings**: `ZEN-D-021`

- `ZEN-D-021` — no gate in any category runs FluxCompiler, so neither the shader lints'
  walk nor the "generated shader tree is clean after a canonical run" invariant is checked
  locally; only `shader-validation.yml` does, and CI does not run from a tick. A shader
  lint can therefore be committed green and vacuous, and the `.spv` tree can drift stale,
  with every gate passing.

**Deviations / follow-ups**

- **One excursion, explained: `Zenith/Flux/Slang/Flux_SpineLint.h`** — a comment only. The
  new lint reuses that header's `StripCommentsAndStrings`, `IsIdentChar` and three
  `Detail::` helpers rather than re-deriving ~80 lines of comment/string stripping (two
  strippers would be two copies of one subtle contract). But the namespace is literally
  named `Detail`, which implies a single consumer, so I noted at that namespace that it now
  has two and that the compiler — not the name — is what will report a break. The worker
  flagged this coupling itself and asked whether full independence was wanted; I judged
  reuse correct and the comment sufficient. Restructuring the helpers into a shared header
  would be the tidier end state and is not this ticket.
- **Four regenerated `.spv` are in the commit**, and they are not all this ticket's work.
  Running FluxCompiler dirtied `Flux_Translucent_Forward.fsMain.spv`,
  `Flux_UnifiedMesh_{Reset.csMain,ToGBuffer.fsMain,ToGBufferVelocity.fsMain}.spv`. I
  hash-compared consecutive runs: **all four byte-stable**, so the output is deterministic
  and the committed versions were **stale**, not nondeterministic. Three are plausibly
  downstream of the `MaterialSurface.slang` edit. The fourth is not —
  `Flux_UnifiedMesh_Reset.slang` includes only `Common/Bindings.slang`, untouched here — so
  its staleness predates this ticket. They are committed because
  `shader-validation.yml` gates the generated tree being clean after a canonical run;
  restoring them would have preserved a state where that check fails.
- **Corrections I made to the ticket's own text before inlining it**, all three confirmed by
  the worker on read:
  1. The Goal says the spine lint lives *"in `Tools/`"* and that `shader-validation.yml`
     *"runs the local script"*. Neither holds: the lint is
     `Zenith/Flux/Slang/Flux_SpineLint.h`, a header-only C++ scanner, and the workflow runs
     FluxCompiler itself. There is no PowerShell shader lint.
  2. **DoD box 3 names a file that does not exist.** There is no
     `Zenith/Flux/Shaders/CLAUDE.md`. The rule's home is `SHADER_STYLE.md`, which already
     carried it in full. Creating a CLAUDE.md to satisfy the citation would have split
     shader documentation across two files, so the worker was told not to.
  3. Box 4's pin is mine.
- **One judgement call I overrode.** The worker struck the `Photorealism.md` bullet through
  and asked whether to delete it instead. The section is headed `## 4. Known gaps` and
  every other bullet is an open gap, so a struck-through "Closed" entry contradicts the
  heading, and the DoD says *drops* the item. I removed it, leaving an HTML comment
  recording the closure and why it was removed rather than struck. Nothing is lost: the
  narrative now lives in `SHADER_STYLE.md` and the lint header, and §1.10 still records the
  bug that bought the rule.
- **`review.required` was false** (risk LOW) and I did not dispatch a reviewer. In its place
  I ran the lint over the real tree in both directions, which is the check a read could not
  have made.
- **Not checked by anyone:** whether the lint's rule has false negatives beyond the
  spellings enumerated (a decode written across two statements, or via a helper that
  returns the sampled `float4`, would pass); the rendering equivalence of the two
  `MaterialSurface.slang` edits beyond arithmetic inspection; and whether the three
  MaterialSurface-downstream `.spv` differ only as that edit implies.
<!-- agent:worklog:end -->
