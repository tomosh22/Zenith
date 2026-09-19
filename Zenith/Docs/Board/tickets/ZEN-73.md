---
id: "0693dc2a-dd98-4e28-be6d-3f57346510ab"
key: "ZEN-73"
type: "TASK"
title: "doc_lint check C9: reconcile AssetManifest roster counts and generator version stamps against the headers"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 73
labels: ["docs"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:59.581Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Suggestions row 2026-08-25 (gates): *"doc_lint needs a check that reconciles AssetManifest.md's roster arithmetic and generator version stamps against the headers. C1 cannot be widened to cover them."* `Games/Zenithmon/Docs/AssetManifest.md` states per-family counts (152 species x 15 files, 34 humans, 30 buildings, 25 props) and the `uZM_*GEN_VERSION` stamps; every generator bump (ZM-D-219/221 bumped buildings 1 to 3, props 2 to 3, humans 3 to 6, creatures 3 to 4) has to be mirrored by hand and nothing reds when it is not.

Add doc_lint check C9 (C8 is the ArtBrief tick reconciliation): parse the roster counts and version stamps from `AssetManifest.md` and compare them to the compiled constants in the generator headers (`uZM_*GEN_VERSION`, the species/human/building/prop table sizes); report each mismatch by name. Self-test in `Tools/Test_*` harness style with a fixture that fails; run over Zenithmon and fix the current drift at its source.

## Definition of Done
- [ ] Check C9 reconciles every roster count and generator version stamp in `AssetManifest.md` against the headers, with a self-test
- [ ] Zenithmon passes it with the drift fixed in the document
- [ ] The Suggestions row is resolved with this key
- [ ] Every game's pinned baseline bumped from an OBSERVED run
