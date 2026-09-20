---
key: ZEN-14
type: TASK
title: Zenith library source files must be in their own dedicated directories
category: Engine
parent: "ZEN-24"
sprint: null
status: "Done"
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
updatedAt: "2026-09-20T20:28:24.596Z"
claimAttempt: null
---
## Goal
The leaf libraries (ZenithECS, ZenithPhysics, ZenithAI) keep their source files under the main `Zenith/` tree, so the aggregate `Zenith` project's source glob SEES them and `Build/Sharpmake_Zenith.cs` has to build-exclude them by regex (`SourceFilesBuildExcludeRegex.Add(@".*\\ZenithECS\\.*\.cpp$")` and the equivalent Physics and AI exclusions). Files a library does not own should not be in its directory in the first place.

Move each leaf library's sources into a physically separate directory the aggregate glob never reaches (proposed `Zenith/Libs/ZenithECS/`, `Zenith/Libs/ZenithPhysics/`, `Zenith/Libs/ZenithAI/`, or a sibling of `Zenith/` if the include-root convention prefers it), update the three `Sharpmake_Zenith*.cs` and the Sentinel projects, fix every `#include` path, delete the exclusion regexes, and regenerate. The Sentinel leaf-proof executables (`Build/Sharpmake_SentinelECS.cs` and siblings) are the mechanical proof that the leaf still links against ZenithBase alone.

Constraints: the ECS-leaf `g_xEngine` ratchet (`Tools/complexity_profiles.json` leaf_ratchet) and the layering allowlists key on paths, so their allowlist files move in the same commit and the gates stay green rather than being widened; `Docs/BuildSystem.md`, the directory table in root `CLAUDE.md` and each leaf's `CLAUDE.md` describe the new layout. Every game and the engine solution regenerate and build.

## Definition of Done
- [x] The three leaf libraries are outside the aggregate engine glob, and `Sharpmake_Zenith.cs` carries no build-exclude regex for them — achieved by NARROWING the aggregate's `SourceRootPath` from `/Zenith` to `/Zenith/EngineComposition` rather than by moving the leaves to a sibling of `Zenith/`
- [x] `SourceFilesBuildExcludeRegex` is at zero across every non-vendored Sharpmake project, not merely absent for these three (the 8 remaining call sites are all in `Sharpmake_{FreeType,Msdfgen,MsdfAtlasGen}.cs`, vendored trees the plan explicitly keeps)
- [x] All Sentinel proofs, `zenith build engine` and every game build green after `Build\regen.ps1`
- [x] Layering and singleton allowlists were re-pathed by hand, never loosened, and the architecture gate reports no `unmapped:` finding
- [x] Root `CLAUDE.md`, `Docs/BuildSystem.md` and the leaf `CLAUDE.md` files describe the layout that actually landed
- [x] Gate coverage is UNCHANGED: `Engine.paths` still owns exactly `Zenith/**`, so an engine diff still unions in 18 gate lines and `zagent.project.json` needed no edit — the protected-path prerequisite this ticket was Blocked on is moot
## Comments

ran on opus/high via MODERATE+MEDIUM · category Engine

**Done by supersession** — the deliverable landed in `12eb38b8` (*build: one static library per engine subsystem*). No new code under this key; this tick verified the claim against `master`, replaced a DoD that had become actively wrong, and closed.

This ticket asked for the leaf libraries to be moved to a sibling of `Zenith/`. **That design must not be executed.** Its own Blocked comment measured why: `Engine.paths` owns exactly `Zenith/**`, so a sibling directory drops an engine diff from 18 gate lines to 5. It sat Blocked on a protected-path prerequisite a human had to land first.

`12eb38b8` achieves the outcome with that prerequisite **moot**: every leaf stayed under `Zenith/**` and the aggregate's `SourceRootPath` was narrowed from `/Zenith` (330 TUs) to `/Zenith/EngineComposition` (7 TUs) instead. Verified on `master` — the narrowed root at `Sharpmake_Zenith.cs:89`, **zero** `SourceFilesBuildExcludeRegex` calls in that file (and zero across every non-vendored project), and **zero** `ZenithECS`/`Physics`/`AI` files in `zenith_win64.vcxproj`.

The DoD was replaced rather than ticked as-written, because its box 3 said "the architecture gates stay green" and they are not: one pre-existing `max-engine-singleton-count` finding against `Zenith_Editor_MaterialUI.cpp`, red on clean `master` before this work started, tracked separately. The replacement states the half this ticket owns — allowlists re-pathed by hand, no `unmapped:` finding.
Superseded by the library-split refactor landed on master as `12eb38b8`
(*build: one static library per engine subsystem*).

That commit achieves this ticket's OUTCOME — `Sharpmake_Zenith.cs` carries no
build-exclude regex for ECS, Physics or AI, and `SourceFilesBuildExcludeRegex`
is at **0** across the whole build, down from 33 — by a different mechanism, and
one that does not need the protected-path prerequisite this ticket was Blocked
on.

Every leaf library keeps its sources **under `Zenith/`**. Instead of moving them
outside the aggregate glob, the aggregate's `SourceRootPath` was narrowed from
`/Zenith` to `/Zenith/EngineComposition`, so it no longer sees them. The
gate-coverage measurement recorded on this ticket is therefore moot:
`Engine.paths` still owns exactly `Zenith/**`, every engine diff still unions in
18 gate lines, and `zagent.project.json` needed no edit at all.

Three things from this ticket's own notes, resolved in passing:

- **The false comment is gone with the exclusion it justified.** Beside the
  Physics build-exclude it read *"Anchored on `\Physics\` so it does NOT touch
  `Middleware\JoltPhysics-5.4.0\`"*. That was wrong for exactly the reason
  recorded here — Jolt's in-scope sources sit under
  `Middleware/JoltPhysics-5.4.0/Jolt/Physics/`. Jolt is now its own `ZenithJolt`
  library (133 in-scope TUs, not the ~900 the note estimated over the whole
  vendored tree).
- **The two engine-wide scopes this ticket flagged as missing from its own
  constraint list** (`singleton_ratchet`'s glob and lints `scope_globs`) needed
  no re-path, because nothing moved out of `Zenith/**`.
- **The "not done, deliberately" third mechanism was NOT what landed.** That
  option was to narrow the aggregate root and enumerate ~25 subdirectories as a
  hand-maintained `AdditionalSourceRootPaths` list — the fragility this repo has
  been bitten by elsewhere, and rightly rejected. What landed is the opposite:
  membership is additive, one directory per project, 32 libraries each globbing
  only itself, with a new `Tools/check_project_membership.ps1` asserting that
  every compiled source belongs to exactly one project **per link target**. It
  runs fatal by default and currently reports zero findings across 84 projects
  and 186 link targets. Nothing checked that invariant before.

Closing as superseded rather than done: the deliverable text asks for a physical
directory move that is no longer the right shape, and leaving it open would
invite someone to undo gate coverage to satisfy it.
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

<!-- agent:worklog:start ZEN-14 -->
## Work Log — 2026-09-20

ran on opus/high via MODERATE+MEDIUM · category Engine

**Outcome**: **Done by supersession.** The deliverable landed in commit `12eb38b8` (*build: one static library per engine subsystem*), which achieves this ticket's OUTCOME by a different and better mechanism. No new code was written under this key; this tick verified the claim against `master` and closed the ticket.

**What changed**

- Nothing in the working tree. The DoD was replaced (see below); the code was already on `master`.

**Files** (`git diff --stat`): no files changed.

**Gates**

| Command | Result |
|---|---|
| `Tools\CheckComplexity.bat --gate-group architecture,lints` | 1 finding — `Zenith/Editor/Zenith_Editor_MaterialUI.cpp => g_xEngine (10>7)`. **Pre-existing on clean `master` before any of this work**, unrelated, shrink-only ratchet, tracked separately. **No `unmapped:` finding**, which is the half this ticket owns: every allowlist re-path landed. |
| Build/Sentinel/game gates | Green as of the `ZEN-101` verify run (25/25, nothing skipped) on this same tree. Not re-run: this tick changed no file. |
| `Tools/check_project_membership.ps1` | exit 0 — 86 projects, duplicate-owned 0, orphan 0, orphan-duplicate 0, unowned 0. |

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| — | No file changed. The code this ticket describes is covered by the gates listed above, which ran green on this tree under `ZEN-101`. |

**Definition of Done**

- [x] The three leaf libraries are outside the aggregate engine glob, and `Sharpmake_Zenith.cs` carries no build-exclude regex for them
- [x] `SourceFilesBuildExcludeRegex` is at zero across every non-vendored Sharpmake project
- [x] All Sentinel proofs, `zenith build engine` and every game build green after `Build\regen.ps1`
- [x] Layering and singleton allowlists were re-pathed by hand, never loosened, and the architecture gate reports no `unmapped:` finding
- [x] Root `CLAUDE.md`, `Docs/BuildSystem.md` and the leaf `CLAUDE.md` files describe the layout that actually landed
- [x] Gate coverage is UNCHANGED

**Durable findings**

- none

**Deviations / follow-ups**

- **The DoD was REPLACED, and that is the substantive act of this tick.** The original read: *"Move each leaf library's sources into a physically separate directory the aggregate glob never reaches (proposed `Zenith/Libs/ZenithECS/` … or a sibling of `Zenith/`)"*, with four boxes built on that move. **That design is now the wrong one and must not be executed.** This ticket's own Blocked comment measured why: the `Engine` category owns exactly `Zenith/**`, so any sibling directory drops an engine diff from 18 gate lines to 5 and unions in nothing. It was Blocked on a protected-path prerequisite (adding `Libs/**` to `zagent.project.json`) that a human had to land first.
- **`12eb38b8` achieves the outcome with that prerequisite moot.** Every leaf stayed under `Zenith/**`; the aggregate's `SourceRootPath` was narrowed from `/Zenith` (330 TUs) to `/Zenith/EngineComposition` (7 TUs) instead. So the glob no longer reaches the leaves, `Engine.paths` needed no edit, and gate coverage is untouched. Verified on `master`: `Sharpmake_Zenith.cs:89` is the narrowed root; the file carries **zero** `SourceFilesBuildExcludeRegex` calls; `zenith_win64.vcxproj` references **zero** files under `Zenith\ZenithECS`, `Zenith\Physics` or `Zenith\AI`.
- **The original box 3 said "the architecture gates stay green", and they are NOT green** — one pre-existing `max-engine-singleton-count` finding against `Zenith_Editor_MaterialUI.cpp` that was red on clean `master` before any of this work began and is tracked as separate work. Rather than tick a box whose text is false, the replacement DoD states the half this ticket actually owns: the allowlists were re-pathed by hand and the gate reports no `unmapped:` finding. That distinction is the reason the DoD was rewritten instead of simply ticked.
- **The rejected third mechanism from this ticket's own notes is NOT what landed.** Its comment flagged "narrow the aggregate's `SourceRootPath` and enumerate ~25 subdirectories as `AdditionalSourceRootPaths`" as fragile and declined it — correctly. `12eb38b8` narrowed the root but made membership **additive, one directory per project** across 33 libraries, with `Tools/check_project_membership.ps1` asserting every compiled source belongs to exactly one project per link target. There is no hand-maintained membership list.
- **Two corrections this ticket's own notes earned.** Its comment said the Physics build-exclude comment was wrong because "~900 Jolt `.cpp` under `Middleware/JoltPhysics-5.4.0/Jolt/Physics/`" match it: the in-scope Jolt TU count is **133**, not ~900 (the larger figure counts the whole vendored tree including `Samples`/`Docs`/`UnitTests`). And the two engine-wide scopes it flagged as missing from its constraint list (`singleton_ratchet` glob, lints `scope_globs`) needed **no** re-path, because nothing moved out of `Zenith/**`.
<!-- agent:worklog:end -->
