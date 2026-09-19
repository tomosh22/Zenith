---
id: "f79e2004-1249-47ac-b636-0ab8b7975c7a"
key: "UV-6"
type: "TASK"
title: "Scaffold Undervault: replace the static visual study with the TDD §1 skeleton, android:true, boot tests and an observed baseline row"
category: "Undervault"
parent: "UV-1"
sprint: "uv-wv005sc30zutg2g8hpg9tfd8"
status: "To Do"
assignee: null
priority: "CRITICAL"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 8
position: 6
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T19:05:58.706Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Games/Undervault` today is a static visual study (commit `84e2fd12`; `Undervault.cpp` selects one of three Blender-generated cutaways by `UNDERVAULT_MOCKUP`, `Tests/Undervault_Boot.cpp` verifies that model, and its GLB and HUD assets are gitignored), so on a fresh checkout `zenith test Undervault --headless` has no gameplay tests and the category gates are red or vacuous. Replace it with the TDD §1 skeleton so the gates go green without Blender, keeping `Docs/`, `Mockups/`, `SourceArt/` and the study README (moved to `Docs/VisualStudy.md`):

- `Undervault.cpp`: `Project_*` hooks only; `Project_RegisterGameComponents` registers `UV_WorldComponent` (order 100), `UV_VaulterComponent` (110), `UV_BuildingComponent` (120), `UV_CameraHostComponent` (130), `UV_UIRootComponent` (140) as stubs, every header `#include`d there (MSVC dead-strip); `Project_SetGraphicsOptions` applies the §12.2 kill-list.
- `Source/UV_Bindings.h` with the §13.1 profiles and actions; `Source/Sim/UV_SimClock` stub advancing a tick; `Source/` folder tree per §1.1.
- `Undervault.zproj` flips `"android": true`; the Gradle tree is copied from `Games/Zenithmon/Android/` (`com.zenith.undervault`, asset dirs include `Zenith/Flux/Shaders`, §17).
- `Tests/UV_Tests_Boot.cpp` (component orders, clock advances) and `Tests/UV_AutoTests_Boot.cpp` (`UV_Boot_Test` at `--fixed-dt` 1/60); the boot scene `Main.zscen` authored by `Zenith_EditorAutomation` steps on the first `_True` run.
- `CLAUDE.md` rewritten with the §1.3 house rules and the §4.4 determinism rules.
- A `"Undervault"` row in `Tools/unit_baselines.json`, OBSERVED from a `Null_` run, declared `advisory` with a reason until UV-7 gates it.

## Definition of Done
- [ ] `Build\regen.ps1`, `zenith build Undervault --headless` and `zenith test Undervault --headless` are green on a checkout with no Blender output
- [ ] `UV_Boot_Test` passes headless and the boot units run under `--exit-after-unit-tests`
- [ ] `Undervault.cpp` contains no mockup selection, GLB import or HUD texture code; the study README survives as `Docs/VisualStudy.md`
- [ ] `CLAUDE.md` states the house rules and determinism rules; `Undervault.zproj` carries `android: true` and `foundry`-style AGDE regen succeeds
- [ ] Undervault pinned baseline bumped from an OBSERVED Null run (the row is added by UV-6; until then this line is inert)
