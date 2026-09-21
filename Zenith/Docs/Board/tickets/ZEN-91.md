---
id: "24cfb1a0-ea58-4b9a-9a79-3782f7c4109d"
key: "ZEN-91"
type: "BUG"
title: "Any --automated-test boot re-authors committed scenes on either backend, breaking the stated boot-shape-independence invariant"
category: "Engine"
parent: null
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 91
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T13:37:11.078Z"
flagged: false
severity: "S2_MAJOR"
reproducibility: null
dueDate: null
environment: null
claimAttempt: "b9cb0e8b-6689-443c-9d3d-a363e1cb1f3f"
---
## Goal

**Any `--automated-test` boot re-authors committed scenes, on EITHER backend, on
an otherwise clean tree, with no source change involved.**

MEASURED repeatedly on 2026-09-19 while taking before/after evidence for ZEN-12,
each time starting from a clean `git status`:

| run | config | left modified |
|---|---|---|
| `devilsplayground.exe --automated-test Test_MaterialEditorLivePreview` | `Vulkan_*_True` | `Games/DevilsPlayground/Assets/Scenes/ProcLevel.zscen` |
| `zenithmon.exe --automated-test ZM_AnimEditorShowcase` | `Vulkan_*_True` | `Games/Zenithmon/Assets/Scenes/Dawnmere.zscen`, plus an untracked `Games/Zenithmon/Assets/Authored/Creatures/Fernfawn/` |
| `rendertest.exe --automated-test RT_RenderGraphViewStructure` | **`Null_*_True`, headless** | `Games/RenderTest/Assets/Scenes/RenderTest.zscen` |

The first two happened identically BOTH before and after ZEN-12's source change,
which is what establishes this as drift from the shipped binaries rather than
from any diff.

Two things follow.

**1. `Games/Zenithmon/CLAUDE.md` states an invariant that is currently false on
`master`**: *"Scene bytes are boot-shape-independent … so a boot must NOT leave a
scene modified in `git status`. If one ever does, that is a regression of that
property -- investigate it rather than just re-committing."* Three scenes across
three games do exactly that today.

**2. The tick protocol's warning is scoped to the wrong thing.** It reads "A
WINDOWED TEST RUN IS AUTHORING, BEFORE IT TESTS". ZEN-6 removed the headless
`.zscen` publish guard, so a `Null_` boot authors too — demonstrated by the
RenderTest row above. Any tick that runs an automated test for evidence and then
stages with `git add -A` sweeps unrelated committed scenes into its own commit.
Nothing mechanical catches this: `zagent guard` sees the files only while they
are still dirty, and the excursion check reports files that reach the commit
without distinguishing ticket work from capture drift.

This ticket is the investigation that `Games/Zenithmon/CLAUDE.md` asks for.

## Definition of Done

- [ ] The cause of each of the three scenes' drift is identified and stated —
      whether it is one cause or three
- [ ] A boot that loads and runs an automated test leaves every committed
      `.zscen` byte-identical, or each survivor is recorded with the reason its
      re-authoring is legitimate and the invariant's wording is corrected to match
- [ ] The `Authored/Creatures/Fernfawn/` files a showcase run creates are either
      not created, or created somewhere gitignored, or documented as intended
      output
- [ ] Proven by booting each of the three automated tests twice from a clean
      tree and showing `git status` empty after both
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit
