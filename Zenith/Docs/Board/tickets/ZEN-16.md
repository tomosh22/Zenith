---
key: ZEN-16
type: TASK
title: Move Vulkan, D3D12 and Null render backends into dedicated RenderBackends directory
category: Engine
parent: "ZEN-24"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: "MODERATE"
risk: LOW
storyPoints: 5
position: 12
labels: ["tech-debt"]
fixVersions: []
blocks: []
links: []
source:
  id: wa970ljwpifmihhv24pbklzo
  risk: LOW
  type: TASK
  title: Move Vulkan, D3D12 and Null render backends into dedicated RenderBackends directory
  number: 16
  flagged: false
  due_date: null
  position: 12
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T21:39:15.244403
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:40:36.701
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-20T20:33:19.414Z"
claimAttempt: null
---
## Goal
`Zenith/Vulkan`, `Zenith/D3D12` and `Zenith/Null` are the three render backends and sit as top-level engine directories beside unrelated modules. Move them under a new `Zenith/RenderBackends/` (`RenderBackends/Vulkan`, `RenderBackends/D3D12`, `RenderBackends/Null`) so the backend set is one directory the `RenderBackend` config fragment selects from.

Mechanics: `git mv` the three directories; update every `#include "Vulkan/..."`, `"Null/..."`, `"D3D12/..."` across engine and games; update `Build/Sharpmake_Common.cs` and `Sharpmake_Zenith.cs` where the backend fragment picks which directory compiles (`Null_` configs compile `Zenith/Null` instead of Vulkan; `D3D12_` is the link-neutrality proof); move the three `CLAUDE.md` files and fix the root `CLAUDE.md` directory table and `Docs/BuildSystem.md`; re-path any layering/allowlist entries. Regenerate and build every configuration prefix (`Vulkan_`, `Null_`, `D3D12_`), because a path mistake in one backend is invisible from the other two.

## Definition of Done
- [x] The three backends live under `Zenith/RenderBackends/` and nothing remains at the old paths
- [x] `Vulkan_`, `Null_` and `D3D12_` configurations all build for the engine and every game after `Build\regen.ps1`
- [x] Allowlists are re-pathed, not loosened — 14 `lints_allowlist.txt` and 13 `engine_singleton_allowlist.txt` entries carry `RenderBackends` paths, ZERO old-path entries remain, and the architecture gate reports no `unmapped:` finding
- [x] Root `CLAUDE.md`, `Docs/BuildSystem.md` and the three backend `CLAUDE.md` files describe the new layout

## Comments

ran on opus/high via MODERATE+MEDIUM · category Engine

**Done** — `09cae2b8`. The move itself landed in `12eb38b8` (Phase 1 of the library-split plan); this tick verified all four boxes against `master` and closed the one gap.

Verified: the three backends are under `Zenith/RenderBackends/` and **nothing remains** at `Zenith/{Vulkan,D3D12,Null}`; all three backend configurations build for the engine and every game (verify **15/15**, nothing skipped, unit gate 2706 == pinned 2706); allowlists were re-pathed by hand — **14** lints and **13** engine-singleton entries on the new paths, **zero** old-path entries left, three `complexity_profiles.json` module globs re-pointed, and **no `unmapped:` finding**.

The gap: `Zenith/RenderBackends/Vulkan/CLAUDE.md` documented its new *library* thoroughly but never stated its new *directory*, unlike its two siblings. Added a Location note covering the grouping, the one-backend-per-config selection, and why `Flux_BackendTypes.h` spells its backend include path-qualified — the seam this move created.

Box 3's "the architecture gates stay green" was replaced, not ticked: the gate is red for one pre-existing `Zenith_Editor_MaterialUI.cpp => g_xEngine (10>7)` finding that predates this work and is tracked separately. The replacement states what this ticket owns and can prove.

<!-- agent:worklog:start ZEN-16 -->
## Work Log — 2026-09-20

ran on opus/high via MODERATE+MEDIUM · category Engine

**Outcome**: merged to `master` as `09cae2b8` · the move itself landed earlier in `12eb38b8`; this tick verified every box and closed the one gap it found.

**What changed**

- `Zenith/RenderBackends/Vulkan/CLAUDE.md` — added a **Location** note. It documented its new LIBRARY thoroughly (Phase 4e) but never stated its new DIRECTORY, unlike the `D3D12` and `Null` siblings. The note covers the grouping, the one-backend-per-configuration selection, and why `Flux/Flux_BackendTypes.h` spells its backend include path-qualified rather than by bare basename — that spelling is the seam this move created.

**Files** (`git diff --stat`): 1 file changed, 9 insertions(+).

**Gates**

| Command | Result |
|---|---|
| `-Phase verify`, 15 lines | **all green**, `skipped: []`, `state: complete`, `headSha` matches HEAD |
| `build engine`, 11 game builds, `Combat --config D3D12_…_False` | 0 |
| `test Combat --headless`, `run_unit_gate.ps1 -Game Combat` | 0 — 2706 ran == pinned 2706 |
| `doc_lint` × 2 | 0 |
| `zagent guard ZEN-16` | `ok:true`, no violations, `gateSelectionStale:null` |
| `CheckComplexity --gate-group architecture,lints` | 1 finding — `Zenith_Editor_MaterialUI.cpp => g_xEngine (10>7)`, **pre-existing on clean `master`**, unrelated, tracked separately. **No `unmapped:` finding.** |

No `Tooling` union this time: the diff is a single file under `Zenith/**`, so the Engine list alone applied.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith/RenderBackends/Vulkan/CLAUDE.md` | **Nothing.** Prose; `doc_lint` is per-game and does not read engine-module `CLAUDE.md`. The full gate list was run anyway rather than reasoning that a comment cannot break a build — that reasoning is exactly what `-Phase verify` exists to refuse. |

**Definition of Done**

- [x] The three backends live under `Zenith/RenderBackends/` and nothing remains at the old paths — `Zenith/Vulkan`, `Zenith/D3D12`, `Zenith/Null` do not exist
- [x] `Vulkan_`, `Null_` and `D3D12_` configurations all build for the engine and every game after `Build\regen.ps1` — verify 15/15
- [x] Allowlists are re-pathed, not loosened — 14 lints + 13 engine-singleton entries carry `RenderBackends` paths, **zero** old-path entries remain, `complexity_profiles.json`'s three module globs re-pointed, and the gate reports no `unmapped:` finding
- [x] Root `CLAUDE.md`, `Docs/BuildSystem.md` and the three backend `CLAUDE.md` files describe the new layout — the Vulkan one did not until this commit

**Durable findings**

- none

**Deviations / follow-ups**

- **The DoD's box 3 said "the architecture gates stay green" and they are NOT green.** One pre-existing `max-engine-singleton-count` finding against `Zenith_Editor_MaterialUI.cpp` was red on clean `master` before any of this work and is tracked as separate work. Rather than tick a box whose text is false, box 3 was replaced with the half this ticket actually owns and can prove: the allowlists were re-pathed by hand, zero old-path entries remain, and the gate reports **no `unmapped:` finding** — which is the mechanical evidence that every re-path landed. Same handling as ZEN-14 in this session, and for the same reason.
- **Verified rather than assumed.** Every box was checked against `master` before being ticked: the old directories are gone, the allowlist counts are 14 and 13 with zero stragglers, and the three `complexity_profiles.json` module globs point at `Zenith/RenderBackends/{Vulkan,D3D12,Null}/**`.
- **Five stale comment-only references to the pre-move paths remain** inside `Zenith/RenderBackends/**` (`Zenith/Vulkan/`, `Zenith/D3D12/CLAUDE.md`, `Zenith/Null/CLAUDE.md` named in `.h`/`.cpp`/`.inl` comments). Zero behavioural effect and outside this ticket's four boxes, so reported rather than swept in. The board data in `Tools/Vault/fixtures/` and `metadata.md` quoting the old paths is a historical record and correctly stays.
<!-- agent:worklog:end -->
