---
id: "e0c2f5a9-499e-4c75-bae8-ed2632e960de"
key: "ZEN-92"
type: "TASK"
title: "Two of three capture harnesses cannot hold a BEFORE and an AFTER set at once"
category: "Engine"
parent: null
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 92
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T14:59:11.343Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
claimAttempt: "c240ca69-b6ab-4170-9869-5fa6ccbb4596"
---
## Goal

Of the three capture harnesses a single ticket's visual evidence can depend on,
only one can hold a BEFORE and an AFTER set at the same time.

| harness | writes to | separable runs? |
|---|---|---|
| `RT_PhotoTour` | `Build/artifacts/rendertest/phototour/<tag>/` via `--phototour-tag` | **yes** |
| `Test_MaterialEditorLivePreview` | `C:/tmp/mat_shot_*.tga`, hardcoded | no |
| `ZM_AnimEditorShowcase` | `Build/artifacts/zenithmon/animeditor/*.tga`, fixed | no |

So a before/after comparison across the material preview or the animation
preview requires the caller to copy artifacts out between runs by hand, every
time, and getting it wrong silently produces a comparison against the wrong
images rather than an error. `RT_PhotoTour` already shows the right shape.

The hardcoded `C:/tmp/` destination is separately worth removing: it is outside
the repo, outside `Build/artifacts/`, and therefore outside anything the
artifact conventions or `.gitignore` govern.

This surfaced on 2026-09-19 taking ZEN-12's evidence, where the BEFORE sets had
to be hand-copied into run scratch before the AFTER runs could be started.

## Definition of Done

- [ ] `Test_MaterialEditorLivePreview` and `ZM_AnimEditorShowcase` each accept a
      run tag and write under a per-tag directory, in the shape `RT_PhotoTour`
      already uses
- [ ] Neither writes outside `Build/artifacts/` any more
- [ ] With no tag supplied, each still writes somewhere predictable — a default
      tag, not a silent overwrite of the last run
- [ ] Running either twice with different tags leaves both sets on disk
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit
