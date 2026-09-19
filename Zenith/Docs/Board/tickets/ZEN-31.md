---
id: "812f0f96-1131-42dd-9a09-55145fac19ef"
key: "ZEN-31"
type: "STORY"
title: "De-globalisation phase 5: SceneManager callback registries and the entity store"
category: "Engine"
parent: "ZEN-24"
sprint: "zen-zen-de-globalisation-program"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 31
labels: []
fixVersions: []
blocks: [{"key":"ZEN-32","reason":"Inventory phase order: Editor statics wrap SceneManager callbacks that phase 5 re-homes."}]
updatedAt: "2026-09-19T17:41:59.467Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
De-globalisation program (ZEN-18 spine; `Docs/design/EngineRefactor_Phase-1_Inventory.md`), **phase 5: SceneManager callback registries and the entity store.** Inventory surface: about 184 callback registration and unregistration sites, five internal SceneManager subsystems, and the entity arrays (the EntityStore migration the inventory analyses). Replace each `g_xEngine.X()` reach and each module-scope mutable static in this surface with the injection shape ZEN-18 records (constructor or explicit `Initialise(...)` wiring from the composition root; per-frame state on a passed context), keeping behaviour identical.

Rules for every phase: the per-file ratchet in `Tools/engine_singleton_allowlist.txt` is SHRUNK for each touched file in the same commit (never grown; the gate refuses growth, and a file left at its old count is a phase that did not finish); `max-static-violations` stays green because the statics are removed, not renamed; the reporting unit from ZEN-18 shows the total falling from the count ZEN-18 records for this surface toward the phase target. Where a subsystem is reached from game code, the game call sites move in the same commit so every game still builds (the Engine gates compile every `.zproj` game). The EntityStore migration has a .zscen serialization impact (inventory section 'EntityStore migration analysis'): any change that could move serialized entity indices needs the dense-index rules of ZM-D-148 and the two-boot byte-identity proof on every committed scene of every game before it merges.

Verification: the engine boot suite and every game's boot suite are the behavioural guard (the change is wiring, not logic); the architecture gates (`Tools/complexity_profiles.json` `architecture,lints` group) are the structural guard. No test is deleted or loosened to make a count pass.

## Definition of Done
- [ ] Every site in this phase's surface reaches its dependency by injection; the phase's files read lower in `Tools/engine_singleton_allowlist.txt` and none higher
- [ ] The static-violation and singleton ratchets are green without any allowlist growth, and the ZEN-18 reporting unit shows the total at or below the phase target
- [ ] Every game builds and every boot suite passes with no behaviour change
- [ ] Every game's pinned baseline bumped from an OBSERVED run
