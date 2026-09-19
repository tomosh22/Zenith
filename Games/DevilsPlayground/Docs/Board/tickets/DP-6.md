---
id: "b478ff3f-82de-4691-a3bc-62ff9b6b672f"
key: "DP-6"
type: "TASK"
title: "Rule on the villager rig source: Mixamo drop, or the engine's shared StickFigure rig through the glb importer"
category: "DevilsPlayground"
parent: "DP-1"
sprint: "dp-dp-mvp-phase-3-assets-and-acceptance"
status: "To Do"
assignee: null
priority: "CRITICAL"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 6
labels: ["needs-human","question"]
fixVersions: []
blocks: [{"key":"DP-7","reason":"The spike imports along the route the ruling chooses (Mixamo drop vs shared StickFigure rig); running it before the ruling would spike the wrong route."}]
updatedAt: "2026-09-19T17:58:30.307Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
MVP-3.0.1 has been a HUMAN_GATE since 2026-05-11 because *"downloading from Mixamo requires Adobe authentication; the agent cannot log in."* Since then the engine gained what the roadmap assumed it lacked: a real `.glb` import path (`Tools/Zenith_Tools_GlbImport`, 2026-08-30, writes the same per-model file set a generator writes), one T-posed humanoid rig shared by every game (`engine:Meshes/StickFigure/StickFigure.zskel`, 51 bones, 17 authored clips, ZM-D-223 and `231bb69c`), and Zenithmon's procedural human generator on that rig. Phase 3 can therefore proceed WITHOUT Mixamo: villagers as generated or hand-made `.glb` humans bound to the shared rig.

The ruling is the user's (MVPScope §2.4 and AssetManifest.md fix the MVP art policy): (a) keep Mixamo (a person drops the FBX or GLB into `Games/DevilsPlayground/Assets/SourceArt/`; the loop does the rest), or (b) switch Phase 3 to the shared StickFigure rig with generated or hand-made bodies, no external login. Record it in `Docs/DecisionLog.md` and amend `MvpRoadmap.md` §3.0 and §3.1 to match (ZM-D-162 discipline: amend the line, never tick words it did not satisfy). `needs-human`.

## Definition of Done
- [ ] A DecisionLog entry records option (a) or (b) with the reason
- [ ] `MvpRoadmap.md` §3.0 and §3.1 are amended to the chosen route and DP-7 and DP-8 cite the entry
