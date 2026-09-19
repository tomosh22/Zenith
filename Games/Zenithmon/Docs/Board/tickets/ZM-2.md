---
id: "3f2c9a6e-5b1d-4d0e-9c7a-2e8f1b6a4d02"
key: "ZM-2"
type: "EPIC"
title: "S1 — Data core"
category: "Zenithmon"
parent: null
sprint: null
status: "Done"
assignee: null
priority: "HIGH"
complexity: "COMPLEX"
risk: "LOW"
storyPoints: null
position: 2
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T16:10:00.000Z"
flagged: false
severity: null
reproducibility: null
resolution: "DONE"
resolvedAt: "2026-07-10T00:00:00.000Z"
---
## Goal

Roadmap.md S1: "Data core (M) -- parallel with S3/S4". `ZM_Types` / `TypeChart` / `SpeciesData` / `MoveData` / `ItemData` / `AbilityData` / `NatureData` / `StatCalc` / `BattleRNG`, the `ZM_WorldSpec` skeleton and `ZM_DataRegistry` validation, as compiled `const` C-array tables in `Source/Data/`.

*Gate:* ~90 unit tests (chart matrix vs golden, stat/exp formula vectors, registry integrity). **MET 2026-07-10 -- 102 `ZM_*` `ZM_Data` unit tests.** Every item in the Roadmap section is ticked.

**BACKFILLED 2026-09-19 (ZM-64).** This epic was missing from the board: ZM issue numbers ran 1..71 with exactly one gap, at 2, and `Roadmap.md` and `Board.md` §2 both mapped S1 to `ZM-2`. The work itself completed on 2026-07-10 (S1 gate MET, ZM-D-020/021/023/029); the creation timestamp of this file is not the work date. Filed by hand because a `zagent create` can only ever append the next number.

## Definition of Done

- [x] Every S1 Roadmap item ticked (ZM-D-020, ZM-D-021, ZM-D-023, ZM-D-029)
- [x] S1 gate MET 2026-07-10 with 102 `ZM_Data` unit tests
