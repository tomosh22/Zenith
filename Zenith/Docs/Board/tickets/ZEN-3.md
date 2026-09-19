---
key: ZEN-3
type: TASK
title: The RenderTest pin is gated by no workflow, so it can drift silently
category: Engine
parent: null
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: 2
position: 1
labels:
  - needs-human
fixVersions: []
blocks: []
links: []
source:
  id: kd3x69nimhh8yyyvmfvcgdp8
  risk: LOW
  type: TASK
  title: The RenderTest pin is gated by no workflow, so it can drift silently
  number: 3
  flagged: false
  due_date: null
  position: 1
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-08-21T10:57:39.776034
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-21T11:47:04.53
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:37:05.474Z"
---
## Goal
`Tools/unit_baselines.json` pins three games and CI gates two of them: `engine-gate.yml` runs `run_unit_gate.ps1 -Game Combat`, `zm-tests.yml` runs `-Game Zenithmon`, and nothing runs `-Game RenderTest`. The file's own `advisory` block records why: engine-gate compiles RenderTest only in `Vulkan_vs2022_Debug_Win64_True` as a compile proof, hosted runners have no Vulkan ICD so the exe never runs, and pinning from a Vulkan exe is forbidden (it reports higher than Null for the same tree). Gating the row needs a SECOND, `Null_`-config RenderTest build in engine-gate, an extra compile of a game that job already compiles once.

Every backend-neutral engine unit moves the RenderTest row (ZM-49, ZEN-2, ZEN-4, ZEN-6 all did), and each was caught only because a person ran the sweep. `Test_UnitBaselineManifest.ps1` enforces that an ungated row is declared advisory, so this is a declared debt, not a silent one.

**`needs-human`:** `.github/**` is a protected path, and adding a gate leg costs CI minutes and can red the build, so it wants a deliberate decision about which workflow owns it.

## Definition of Done
- [ ] Some workflow runs `run_unit_gate.ps1 -Game RenderTest` against a `Null_` RenderTest exe
- [ ] A drifted RenderTest pin fails that check rather than surviving
- [ ] The `advisory` entry for RenderTest in `Tools/unit_baselines.json` is removed once the row is genuinely gated, and `Test_UnitBaselineManifest.ps1` stays green
