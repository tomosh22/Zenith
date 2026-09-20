---
key: ZEN-14
type: TASK
title: Zenith library source files must be in their own dedicated directories
category: Engine
parent: "ZEN-24"
sprint: null
status: "To Do"
assignee: null
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
updatedAt: "2026-09-20T11:05:24.394Z"
claimAttempt: "70dc181f-edb9-4f5c-83a5-5130e507c66c"
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

Released without dispatching a worker. Claimed during a verification tick whose purpose was to confirm the 2026-09-20 protocol fixes; that tick found a blocking defect in the gate-selection path itself and is being spent on it instead.

**The defect:** `zagent.project.json` is documented as the single authority for gate lines ("sent to the board with every request... a stored copy goes stale, and a stale gate list is exactly how a green run ratchets the wrong pinned baseline"). Under the Vault backend the CLI does not read it. `policyOf()` in `Tools/Vault/cli/compat.mjs` resolves categories from `vault.config.json`'s embedded `policy.client.file` block — a snapshot taken at the cutover migration, with no sync mechanism.

Measured on this claim: the on-disk `zagent.project.json` carries 15 Engine gate lines and a `Tooling` category; `zagent gates ZEN-14` resolved 13 lines and no `Tooling`. Two fixes committed earlier today (`fa8ad5b2`) are therefore INERT — the `Tooling` category that was meant to close "no gate runs non-C++ code", and the `doc_lint` lines that close ZEN-D-006.

Nothing was dispatched and no file was edited for ZEN-14. Returned to To Do, unassigned.
