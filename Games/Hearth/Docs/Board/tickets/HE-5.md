---
id: "7c0424aa-0983-40cc-81e2-63324d3c9f38"
key: "HE-5"
type: "TASK"
title: "Scaffold Hearth: zenith new Hearth shaped to TDD §2, android:true, three committed scenes, boot tests, observed baseline row"
category: "Hearth"
parent: "HE-1"
sprint: "he-u2exlj3764t6lnl5ehdwdwea"
status: "To Do"
assignee: null
priority: "CRITICAL"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 8
position: 5
labels: []
fixVersions: []
blocks: [{"key":"HE-6","reason":"the human edits reference files the scaffold creates","raw":{"key":"HE-6","reason":"the human edits reference files the scaffold creates","raw":{"key":"HE-6","reason":"the human edits reference files the scaffold creates"},"invalid":false},"invalid":false},{"key":"HE-7","reason":"the docs describe a game that must exist first","raw":{"key":"HE-7","reason":"the docs describe a game that must exist first"},"invalid":false},{"key":"HE-8","reason":"the cores live in Source/ the scaffold creates"}]
updatedAt: "2026-09-19T19:24:13.686Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
TDD §14: *"`Games/Hearth/Hearth.zproj` `{schemaVersion:1, name:"Hearth", android:true}`; scaffold via `zenith new Hearth` (P0); layout per repo convention (`Hearth.cpp`, `HE_Bindings.h`, `Components/`, `Source/`, `Tests/`, `Assets/`, `Config/`, `Docs/`, `Android/`)."* Today `Games/Hearth/` holds only `Docs/`, so the Hearth category gates (`Build\regen.ps1`, `zenith build Hearth --headless`, `zenith test Hearth --headless`) fail on a fresh checkout and pass exactly when this diff lands. Run `zenith new Hearth` and shape the result to TDD §2: `Hearth.cpp` with `Project_*` hooks only, `Project_RegisterGameComponents` registering the seven §2 components as stubs at orders 100 to 106 with every header `#include`d there (MSVC dead-strip), `Project_SetGraphicsOptions` applying the §8 boot table; `Source/` folders `Core/ Grid/ Sim/ Build/ Econ/ Save/` plus `HE_Events.h` and `HE_Bindings.h`; the Gradle tree with `Zenith/Flux/Shaders` in the asset block (§12) and `screenOrientation=landscape`; `Tests/HE_Tests_Boot.cpp` (component orders) and `Tests/HE_AutoTests_Boot.cpp` (`HE_Boot_Test`, boot characterisation asserting a known shader loads); the three §14 scenes authored by `Zenith_EditorAutomation` steps on the first `_True` run with frozen quaternion constants; `CLAUDE.md` carrying §2 architecture, the determinism stance (§2) and the repo mandates; a `"Hearth"` row in `Tools/unit_baselines.json` OBSERVED from a `Null_` run and declared `advisory` with a reason until HE-6 gates it.

## Definition of Done
- [ ] `Build\regen.ps1`, `zenith build Hearth --headless` and `zenith test Hearth --headless` are green on a fresh checkout
- [ ] `HE_Boot_Test` passes headless and asserts a known shader loads; the boot units run under `--exit-after-unit-tests`
- [ ] `FrontEnd.zscen`, `Lot.zscen` and `CAS.zscen` are committed with frozen rotation constants and the ZM byte-stability test
- [ ] `CLAUDE.md` states the §2 architecture and mandates; `Hearth.zproj` carries `android: true` and the AGDE solution regenerates
- [ ] Hearth pinned baseline bumped from an OBSERVED Null run (the row is added by HE-5; until then this line is inert)
