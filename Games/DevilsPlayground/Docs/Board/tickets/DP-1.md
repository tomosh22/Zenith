---
key: DP-1
type: EPIC
title: DevilsPlayground MVP
category: DevilsPlayground
parent: null
sprint: null
status: To Do
assignee: null
priority: "HIGH"
complexity: COMPLEX
risk: MEDIUM
storyPoints: null
position: 1
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: u4wjxfhy3swpf8zq61mzll8z
  risk: MEDIUM
  type: EPIC
  title: DevilsPlayground MVP
  number: 1
  flagged: false
  due_date: null
  position: 1
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: lrby3rinime7pp20c42qpvtg
  complexity: COMPLEX
  created_at: 2026-08-18T01:24:14.062973
  project_id: f1ecqdf4ab8xgdorovsz6t7v
  resolution: null
  updated_at: 2026-09-16T01:02:02.012
  assignee_id: null
  category_id: oqysvfbye2qm4qjyuz4rvoda
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:51:12.861Z"
---
## Goal
`Games/DevilsPlayground/Docs/MVPScope.md` §0 to §5: the MVP sentence, the §1.1 system list, and the §5 success criteria (headless MVP subset 100% pass; `Test_P4Playthrough_Night1WinGolden` under 150 s at fixed-dt 60 Hz; both loss playthroughs pass; a human completes the sentence in under five minutes; every §1.1 system has a simulator-driven test). `MvpRoadmap.md` is the sequenced task list with `MVP-N.N.N` ids (uniqueness-checked by `Tools/doc_lint.ps1` C3); Phases 0.0 to 2 and most of 4 are ticked, Phase 3 (asset substitution) is untouched behind a Mixamo login gate, and MVP-4.3.4 is the human demo. Every open MVP item is a child here.

DP runs `branching: direct` in `zagent.project.json` (the `Docs/CIPolicy.md` squash-merge PR text predates the Vault cutover and is history).

## Definition of Done
- [ ] Every child issue Done
- [ ] MVPScope §5 criteria 1 to 5 all true at once, recorded in `Docs/Status.md`
- [ ] The MVP demo is signed off by a person (DP-18)
