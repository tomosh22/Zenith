---
id: "97666154-84e7-4a08-8950-b5ab65befa01"
key: "ZEN-85"
type: "TASK"
title: "Make the aux tools (FluxCompiler, font libraries) build green so a whole solution can be built"
category: "Engine"
parent: "ZEN-1"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 85
labels: ["tech-debt"]
fixVersions: []
blocks: []
updatedAt: "2026-09-21T11:51:32.620Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: "b425b559-8fb4-4c57-9d35-5cb4a9e288cb"
---
## Goal
CLAUDE.md: *"Always build with `/t:<Game>`, never the whole solution: the aux tools (FluxCompiler / font libs) present in the sln are pre-existing-red in `ToolsEnabled=True`."* A solution that cannot be built whole hides every error in the projects nobody targets, and `release-build.yml` and the packaging path have to enumerate targets by hand.

Make FluxCompiler and the font dependency projects (`Sharpmake_FreeType.cs`, `Sharpmake_Msdfgen.cs`, `Sharpmake_MsdfAtlasGen.cs`) build green in every `ToolsEnabled=True` configuration, or exclude them from the configurations they cannot support with the reason in the Sharpmake file; then build `Build\zenith_engine_win64.sln` and one game solution WHOLE as the proof and record it in `Docs/BuildSystem.md`, retiring the "never the whole solution" rule.

## Definition of Done
- [ ] The engine solution and one game solution build whole in `Vulkan_vs2022_Debug_Win64_True` with zero errors
- [ ] `Docs/BuildSystem.md` and root `CLAUDE.md` drop the never-whole-solution rule or state exactly which project is excluded and why
- [ ] Every game's pinned baseline bumped from an OBSERVED run
