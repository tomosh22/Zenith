---
id: "70275e86-1551-4338-b3dd-46850484f354"
key: "DP-8"
type: "STORY"
title: "MVP-3.1 villager skinned meshes: idle, walk and run driven by movement, archetype tint, all-archetypes-instantiable test"
category: "DevilsPlayground"
parent: "DP-1"
sprint: "dp-dp-mvp-phase-3-assets-and-acceptance"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 8
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:55:12.898Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
MvpRoadmap §3.1 (MVP-3.1.1 to 3.1.4): villagers stop being tinted capsules. Wire `DPVillager_Behaviour` to the imported skinned mesh and animator from DP-7 with idle, walk and run clips driven from the velocity the behaviour already sets each frame; tint per archetype through the engine per-instance tint lane (ZEN-48) or, until it lands, `DPMaterials::GetOrCreateColouredVariant`; keep the capsule collider and every placement contract unchanged so the 133-plus test suite and the personality matrix are untouched.

Tests (AssetTestPlan §2, TestPlan §3.1): `Test_AssetScene_AllArchetypesInstantiable` spawns each MVP archetype with its mesh and asserts the model, skeleton and clip set resolve; an existing possession test asserts the animator state follows possessed movement (idle at rest, walk under input, run under sprint); `Shortfalls.md` §1.4 closes. The held-item socket moves from the floating cube position to the rig's hand bone in DP-11. DP gates: `zenith build DevilsPlayground --headless`, the D3D12 link proof and `zenith test DevilsPlayground --headless`; the DP suite has no pinned unit-count row, so no pin bump applies unless `Zenith/**` is touched. Conventions: `Games/DevilsPlayground/Docs` are binding (AgentBriefing, TestPlan §0 verification contract, test-first inside the session), `Config/*.json` carries every numeric value, no paid or external services, and `Tools/check_acceptance_drift.ps1` must stay within 20% per test.

## Definition of Done
- [ ] All four MVP archetypes render as skinned humans on the shared rig with idle, walk and run driven by movement, tinted per archetype
- [ ] `Test_AssetScene_AllArchetypesInstantiable` and an animator-state arm pass headless; the full suite and the 10-seed matrix are unchanged
- [ ] `Shortfalls.md` §1.4 and `MvpRoadmap.md` §3.1 reflect the shipped state
