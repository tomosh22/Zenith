---
key: ZEN-3
type: TASK
title: The RenderTest pin is gated by no workflow, so it can drift silently
category: Engine
parent: null
sprint: null
status: To Do
assignee: agent@saas.invalid
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: null
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
---
## Goal

`Tools/unit_baselines.json` pins three games — Zenithmon 3364, Combat 1648, RenderTest 1739 — but **CI gates only two of them**:

| Workflow | Gates |
|---|---|
| `engine-gate.yml:191` | `-Game Combat` |
| `zm-tests.yml:683` | `-Game Zenithmon` |
| `scaffold-smoke.yml` | Combat, via `test_scaffold.ps1` |

Nothing passes `-Game RenderTest`. So the RenderTest row is verified only when somebody runs it by hand.

That matters because the gate asserts `ran == baseline` EXACTLY. An unread pin cannot red, so it drifts silently — and the next person to run a RenderTest sweep gets a failure that has nothing to do with their change. This is the same failure the manifest was created to end, surviving in the one row no workflow reads.

RenderTest is not incidental here: it boots the same engine suite, so **every backend-neutral engine unit moves its row**. ZM-49 (+6) and ZEN-2 (+4) both did, and both were caught only because a human ran the sweep deliberately.

Found by the ZEN-2 reviewer pass.

**This needs a human: `.github/**` is a protected path, so the agent loop is refused by `zagent guard`.** Note this is not a pure documentation change — adding a gate leg costs CI minutes and can red the build, so it wants a deliberate decision about which workflow owns it.

## Definition of Done

- [ ] Some workflow runs `run_unit_gate.ps1 -Game RenderTest` against a `Null_` RenderTest exe
- [ ] A drifted RenderTest pin fails that check rather than surviving
- [ ] `Tools/unit_baselines.json`'s `$coverage` note is updated or removed once the row is genuinely gated
