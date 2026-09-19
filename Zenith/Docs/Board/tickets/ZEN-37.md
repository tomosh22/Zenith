---
id: "c8df09aa-c216-453d-a668-af2e285db996"
key: "ZEN-37"
type: "TASK"
title: "Break the SceneManager/SceneData include cycle: extract the component-pool types (T2.4)"
category: "Engine"
parent: "ZEN-24"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: 8
position: 37
labels: ["tech-debt","deferred"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:33:46.244Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/Onboarding/JuniorExplorationFollowup.md` triage row: *"`SceneManager.h` <-> `SceneData.h` include cycle: deferred, load-bearing for template instantiation; the documented `T2.4` task to extract `Zenith_ComponentPool*` types is a separate larger refactor."* The junior exploration already split `Zenith_SceneManager.h` into the public API, `Zenith_SceneManagerGuards.h` and `Zenith_SceneManagerInternal.h`; the cycle with `Zenith_SceneData.h` survived because the component-pool templates are defined where both headers can see them.

Do T2.4: extract the `Zenith_ComponentPool*` types into their own header under `Zenith/ZenithECS/` that neither `SceneManager.h` nor `SceneData.h` includes the other through; make the dependency one-directional and prove it with the layering gate (a forbidden-edge entry for the reverse direction in `Tools/layering_allowlist.txt`, so the cycle cannot return silently). The ECS leaf's Sentinel build (`Build/Sharpmake_SentinelECS.cs`) is the compile proof that the extraction is leaf-clean, and the ECS unit suite (`Zenith_Query`, `Zenith_Scene`, pool growth and relocation units, including `ModelSpaceOffsetSurvivesPoolGrowth`) is the behavioural proof. `Zenith/ZenithECS/CLAUDE.md` describes the header graph afterwards.

## Definition of Done
- [ ] `Zenith_SceneManager.h` and `Zenith_SceneData.h` no longer include each other; the pool types live in their own header and the direction is asserted by the layering gate
- [ ] SentinelECS links and the ECS unit suite passes with no behaviour change
- [ ] `Zenith/ZenithECS/CLAUDE.md` shows the new header graph
- [ ] Every game's pinned baseline bumped from an OBSERVED run
