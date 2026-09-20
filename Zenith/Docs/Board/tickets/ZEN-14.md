---
key: ZEN-14
type: TASK
title: Zenith library source files must be in their own dedicated directories
category: Engine
parent: "ZEN-24"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: MODERATE
risk: MEDIUM
storyPoints: 8
position: 10
labels: ["tech-debt"]
fixVersions: []
blocks: []
links: []
source:
  id: m3b3a8je19zyw6kued14n5nf
  risk: MEDIUM
  type: TASK
  title: Zenith library source files must be in their own dedicated directories
  number: 14
  flagged: false
  due_date: null
  position: 10
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: MODERATE
  created_at: 2026-09-11T21:34:05.259819
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:38:09.874
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-20T11:37:35.525Z"
claimAttempt: "1ace13f0-259b-42d4-a11f-d7b1c3432390"
---
## Goal
The leaf libraries (ZenithECS, ZenithPhysics, ZenithAI) keep their source files under the main `Zenith/` tree, so the aggregate `Zenith` project's source glob SEES them and `Build/Sharpmake_Zenith.cs` has to build-exclude them by regex (`SourceFilesBuildExcludeRegex.Add(@".*\\ZenithECS\\.*\.cpp$")` and the equivalent Physics and AI exclusions). Files a library does not own should not be in its directory in the first place.

Move each leaf library's sources into a physically separate directory the aggregate glob never reaches (proposed `Zenith/Libs/ZenithECS/`, `Zenith/Libs/ZenithPhysics/`, `Zenith/Libs/ZenithAI/`, or a sibling of `Zenith/` if the include-root convention prefers it), update the three `Sharpmake_Zenith*.cs` and the Sentinel projects, fix every `#include` path, delete the exclusion regexes, and regenerate. The Sentinel leaf-proof executables (`Build/Sharpmake_SentinelECS.cs` and siblings) are the mechanical proof that the leaf still links against ZenithBase alone.

Constraints: the ECS-leaf `g_xEngine` ratchet (`Tools/complexity_profiles.json` leaf_ratchet) and the layering allowlists key on paths, so their allowlist files move in the same commit and the gates stay green rather than being widened; `Docs/BuildSystem.md`, the directory table in root `CLAUDE.md` and each leaf's `CLAUDE.md` describe the new layout. Every game and the engine solution regenerate and build.

## Definition of Done
- [ ] The three leaf libraries live outside the aggregate engine glob and `Sharpmake_Zenith.cs` carries no build-exclude regex for them
- [ ] All three Sentinel proofs, `zenith build engine` and every game build green after `Build\regen.ps1`
- [ ] Layering and singleton allowlists are re-pathed, not loosened; the architecture gates stay green
- [ ] Root `CLAUDE.md`, `Docs/BuildSystem.md` and the leaf `CLAUDE.md` files describe the new layout

## Comments

Blocked on a one-line prerequisite in a protected path. No file was edited for this ticket.

## The contradiction is structural, not a choice of directory

The DoD requires the three leaf libraries to live **outside the aggregate engine glob**. That glob is `SourceRootPath = RootPath + "/Zenith"` (`Build/Sharpmake_Zenith.cs:177`) and it is recursive, so "outside the aggregate glob" means, exactly, "outside `Zenith/`".

The Engine category in `zagent.project.json` owns exactly `Zenith/**`. So the two constraints are mutually exclusive: any directory that satisfies the DoD is a directory the Engine gate list no longer covers.

This also disposes of the Goal's first proposed target. **`Zenith/Libs/ZenithECS/` does not work** — it is still under `Zenith/`, so the recursive glob still sees it and the build-exclude regex would still be required. Only the Goal's parenthetical alternative ("a sibling of `Zenith/`") can satisfy the DoD.

## Measured

`effectiveGates` with the ticket filed under a non-Engine category, so the union is what rescues an engine edit:

| Changed path | Gate lines | Unioned |
|---|---|---|
| `Zenith/ZenithECS/Zenith_Scene.h` (today) | 18 | Engine |
| `Zenith/AI/Zenith_AIWorldHooks.cpp` (today) | 18 | Engine |
| `Libs/ZenithECS/Zenith_Scene.h` (after move) | **5** | **(none)** |
| `Libs/AI/Zenith_AIWorldHooks.cpp` (after move) | **5** | **(none)** |
| `Libs/Physics/Zenith_Physics.cpp` (after move) | **5** | **(none)** |
| `Tools/doc_lint.ps1` (control) | 16 | Tooling |

Landing the move as specified removes 13 of 18 gate lines from every future change to the engine's ECS, Physics and AI leaves — the ZM-50 defect the `paths` union was built to prevent, in the "a shorter gate list still goes green" shape recorded as ZEN-D-010.

## Why the loop cannot fix it itself

The fix is to add `Libs/**` to `Engine.paths`. That file is protected, and the guard refuses it:

```
$ zagent guard ZEN-14 --json     # with zagent.project.json dirty
{"ok":false,"violations":[{"path":"zagent.project.json","pattern":"zagent.project.json"}], ...}
exit 1
```

## The human prerequisite (one line, safe to do now)

Add the chosen sibling root to `Engine.paths` in `zagent.project.json`:

```json
"paths": ["Zenith/**", "Libs/**"]
```

This is safe to land **before** any file moves — it matches nothing today, so it changes no current gate selection. Once it is in, this ticket is fully loop-completable and can be returned to To Do. Please also confirm the directory name, since `Engine.paths` and the Sharpmake include root have to agree: `Libs/ZenithECS`, `Libs/Physics`, `Libs/AI` keeps all ~900 existing `#include "ZenithECS/…"` / `"Physics/…"` / `"AI/…"` directives resolving unchanged, provided `ConfigureCommonIncludePaths` gains `RootPath + "/Libs"`.

## Three things for whoever picks this up

1. **The ticket's constraint list is incomplete.** It names the leaf ratchets and the layering allowlists. It misses two engine-wide scopes in `Tools/complexity_profiles.json` that are also keyed on `Zenith/**` and would silently stop covering the leaves: the `singleton_ratchet` glob (line 135) and the lints `scope_globs` (line 148). Both are unprotected, so the loop can re-path them — but they must be in the same commit or the g_xEngine and lint coverage narrows with every gate green. `Tools/lints_allowlist.txt` carries 8 entries with leaf paths; the other six allowlists carry none.

2. **A comment in `Sharpmake_Zenith.cs` is false.** Beside the Physics exclusion it reads *"Anchored on \Physics\ so it does NOT touch Middleware\JoltPhysics-5.4.0\ (no \Physics\ path segment there)."* There are ~900 Jolt `.cpp` under `Middleware/JoltPhysics-5.4.0/Jolt/Physics/`, which that regex matches. The exclusion is harmless only because the aggregate never globs Jolt (`AdditionalSourceRootPaths` adds imgui and Tools, not Jolt) — i.e. the conclusion is right for a different reason than the one written down. Worth correcting while the block is being edited.

3. **`Build/**` is owned by no category**, so a Sharpmake-only diff unions in zero gates (measured above: `Build/Sharpmake_Zenith.cs` → 5). That is a pre-existing gap of the same family as this one and is filed separately.

## Not done, deliberately

A third mechanism would satisfy the DoD text without moving anything: narrow the aggregate's `SourceRootPath` and enumerate the ~25 engine subdirectories as `AdditionalSourceRootPaths`, leaving the leaves simply unlisted. It needs no file moves, no protected-path edit and loses no gate coverage — but it contradicts the Goal's stated intent (physical directory separation) and replaces 3 regexes with a ~25-entry hand-maintained membership list, which is the fragility this repo has been bitten by elsewhere. Flagging it rather than substituting it; that is a call for the ticket's author.
Released without dispatching a worker. Claimed during a verification tick whose purpose was to confirm the 2026-09-20 protocol fixes; that tick found a blocking defect in the gate-selection path itself and is being spent on it instead.

**The defect:** `zagent.project.json` is documented as the single authority for gate lines ("sent to the board with every request... a stored copy goes stale, and a stale gate list is exactly how a green run ratchets the wrong pinned baseline"). Under the Vault backend the CLI does not read it. `policyOf()` in `Tools/Vault/cli/compat.mjs` resolves categories from `vault.config.json`'s embedded `policy.client.file` block — a snapshot taken at the cutover migration, with no sync mechanism.

Measured on this claim: the on-disk `zagent.project.json` carries 15 Engine gate lines and a `Tooling` category; `zagent gates ZEN-14` resolved 13 lines and no `Tooling`. Two fixes committed earlier today (`fa8ad5b2`) are therefore INERT — the `Tooling` category that was meant to close "no gate runs non-C++ code", and the `doc_lint` lines that close ZEN-D-006.

Nothing was dispatched and no file was edited for ZEN-14. Returned to To Do, unassigned.
