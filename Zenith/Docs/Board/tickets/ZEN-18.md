---
key: ZEN-18
type: TASK
title: "De-globalisation program spine: refreshed inventory, the injection shape, the ratchet's target and the phase split"
category: Engine
parent: "ZEN-24"
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: MODERATE
risk: HIGH
storyPoints: 5
position: 14
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: sf39sc18vofn5e67qcsm6yew
  risk: HIGH
  type: TASK
  title: Finish dependency injection refactor to remove g_xEngine references
  number: 18
  flagged: false
  due_date: null
  position: 14
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: MODERATE
  created_at: 2026-09-11T21:42:07.882508
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:42:53.261
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:38:11.620Z"
---
## Goal
The number of `g_xEngine` references must fall in favour of dependency injection. Today: 1,569 references across 227 engine files and 3,651 in games; `Tools/engine_singleton_allowlist.txt` (148 lines) is the shrink-only per-file COUNT ratchet the complexity gate enforces (`Tools/complexity_profiles.json`, `architecture.singleton_ratchet`: a file may never exceed its baseline, a new file gets zero), and the ECS leaf is already at zero by its own ratchet. `Docs/design/EngineRefactor_Phase-1_Inventory.md` is the plan: it inventories every mutable static and global by subsystem and sequences the work in phases with a per-phase surface estimate.

This ticket is the SPINE of that program, not the whole program: (1) re-run the inventory's call-site counts against HEAD and record them in the document (the 2026-05 numbers predate the `*Impl` collapse onto `g_xEngine`); (2) define the injection shape the phases will use, in the document, with a worked example on one small subsystem (the Phase 2 `FrameContext` candidate); (3) make the ratchet's target explicit: a unit that reads `Tools/engine_singleton_allowlist.txt` and reports the total, so each phase ticket's DoD can name the number it must reach; (4) split the remaining phases into the child tickets ZEN-29 to ZEN-36 (already filed) and confirm each one's file list against the refreshed counts.

No sweeping edit lands here: the phases do that. Anything engine-side here is the one worked example and the reporting unit.

## Definition of Done
- [ ] `Docs/design/EngineRefactor_Phase-1_Inventory.md` carries counts refreshed against HEAD, the injection pattern with one worked example, and a per-phase target count
- [ ] A unit reports the allowlist total so each phase can pin its target; the ratchet still refuses growth
- [ ] ZEN-29 to ZEN-36 each cite the refreshed count they start from and the target they must reach
- [ ] Every game's pinned baseline bumped from an OBSERVED run
