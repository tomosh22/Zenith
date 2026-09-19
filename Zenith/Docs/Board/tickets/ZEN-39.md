---
id: "4ee4b997-bd33-4589-b679-8a26427fdb43"
key: "ZEN-39"
type: "STORY"
title: "Unified mesh Stage 6: crowd batching via DrawIndexedIndirectCount per bucket"
category: "Engine"
parent: "ZEN-25"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 39
labels: ["deferred"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:33:53.550Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The unified GPU opaque-mesh pipeline shipped Stages 0 to 5 (per-object descriptor churn to zero, GPU cull to indirect draws, compute skinning as the sole animated path) and deferred **Stage 6, crowd batching**: drop the per-instance discriminator, let the cull emit N per-instance commands per bucket, and draw each bucket with `DrawIndexedIndirectCount` (which both backends already implement) so a hundred identical skinned villagers cost one indirect draw rather than one command each. Deferred because nothing in the tree yet draws a crowd; Zenithmon's towns and Hearth's neighbourhoods will.

Scope: extend the cull shader to write per-instance commands into a per-bucket region with an atomic count; bind the count buffer to `DrawIndexedIndirectCount`; keep the shadow cascade passes on the same command stream (the Stage 5 review found the cascade passes drawing the skinned arena without a declared visibility, so every new buffer declares its render-graph access); headless units over the command layout and the bucket offsets (`Flux_BuildBucketOffsets` pattern) with the raw-mirror technique that pinned the skinning kernel; a windowed A/B on RenderTest and Zenithmon proving identical frames and a measured draw-count reduction. Both backends keep matching signatures.

`deferred`: skipped by the queue until a game presents a crowd; claim by name when one does.

## Definition of Done
- [ ] Per-bucket `DrawIndexedIndirectCount` draws replace the per-instance discriminator on every unified-mesh pass including the cascades, with every new buffer's render-graph access declared
- [ ] Headless units pin the command layout and count arithmetic; a windowed A/B shows identical frames and records the draw-count reduction
- [ ] Every game's pinned baseline bumped from an OBSERVED run
