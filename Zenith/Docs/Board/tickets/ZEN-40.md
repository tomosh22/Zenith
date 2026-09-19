---
id: "4e8e8310-4a0c-47ae-8e05-eaedf2847a83"
key: "ZEN-40"
type: "BUG"
title: "Teardown scratch overflow makes every windowed run exit non-zero"
category: "Engine"
parent: "ZEN-25"
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 40
labels: ["tech-debt"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:33:57.117Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4: *"The teardown scratch overflow (see §0). Cosmetic, but it makes every windowed run exit non-zero."* A windowed run that always exits non-zero is not cosmetic for a gate: every windowed verification (`zenith test <Game>` windowed, `phototour_run.ps1`, the `Vulkan_*_True` authoring boots the ZM tickets require) has to be read by its log rather than its exit code, which is the fail-open shape this repo keeps paying for (`zenith build` once reported success for an exe that could not start).

Find the overflow: which scratch allocator or arena is exceeded at shutdown, by which teardown order (asset handles releasing after the registry, or a render-resource release after the memory manager, in the pattern `d45bd7b3` fixed for handles). Fix the ORDER or the budget at its source, never by widening the exit-code check. Add a unit that drives the teardown sequence headless where the allocator is backend-neutral, and confirm the windowed run exits zero on RenderTest and Zenithmon.

## Definition of Done
- [ ] A windowed `Vulkan_vs2022_Debug_Win64_True` run of RenderTest and Zenithmon exits zero with no teardown overflow in the log
- [ ] The cause is fixed at its source and a headless unit guards the teardown order or budget
- [ ] `Docs/design/Photorealism.md` §0 and §4 drop the note
- [ ] Every game's pinned baseline bumped from an OBSERVED run
