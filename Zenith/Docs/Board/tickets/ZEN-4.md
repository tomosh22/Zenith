---
key: ZEN-4
type: TASK
title: Make the physics-chunk-span lifetime invariant simple instead of subtle
category: Engine
parent: "ZEN-24"
sprint: null
status: Done
assignee: agent@zenith.invalid
priority: MEDIUM
complexity: SIMPLE
risk: MEDIUM
storyPoints: 3
position: 1
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: jm1q8yy282bk1d2o8z80b1qx
  risk: MEDIUM
  type: TASK
  title: Make the physics-chunk-span lifetime invariant simple instead of subtle
  number: 4
  flagged: false
  due_date: null
  position: 1
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: dxhlckk5kgxv4mgp4e8lcu7s
  complexity: SIMPLE
  created_at: 2026-08-21T11:50:31.454154
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: DONE
  updated_at: 2026-08-21T12:49:26.593
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: 2026-08-21T12:49:26.588
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T18:03:34.514Z"
---
## Goal

ZEN-2 added a per-chunk span table (`m_pxPhysicsChunkSpans`) alongside `m_pxPhysicsGeometry`. Its lifetime invariant is currently **subtle rather than simple**, and the header says so because the simple version is not true:

`Zenith_TerrainComponent_Editor.cpp` (`CleanupPriorGenerationForRegenerate`, ~1162-1167) deletes and nulls `m_pxPhysicsGeometry` and leaves the span table standing. That is *safe today* for a two-step reason: the query gates on null geometry first, and `LoadCombinedPhysicsGeometryCore` — the only site that assigns a fresh mesh — calls `FreePhysicsChunkSpans()` unconditionally. So a leftover table is a delayed free, not a use-after-free.

But "safe because of how two other functions happen to behave" is a worse invariant than "every site that discards the geometry frees the table", and the ZEN-2 review had to reason through it to confirm it holds. The editor TU can call `FreePhysicsChunkSpans()` — it is a private member of the same class and there was never a technical barrier, only that the file was outside that ticket's scope.

Note the review also found the *original* safety argument was wrong on its premise: "its caller always then calls `LoadCombinedPhysicsGeometry()`" is false, because `Zenith_TerrainComponent_Editor.cpp:1056-1061` returns early when `DeleteExistingTerrainFilesInDirectory` fails. The conclusion survived; the reasoning did not. That is exactly the kind of invariant worth making mechanical.

**Second, smaller item in the same file family.** `Zenith_TerrainComponent.cpp:732-739` still opens its AABB comment with "This is an accelerator". It is not *only* an accelerator: it CLIPS the ±`fBARYCENTRIC_EPSILON` band, which is what prevents rim extrapolation, so it changes the answer rather than merely speeding it up. The precise wording already landed in the header and `Components/CLAUDE.md` under ZM-49; the `.cpp` line was outside that fix-forward's file list.

## Definition of Done

- [ ] Every site that discards `m_pxPhysicsGeometry` also frees the span table, including the editor path
- [ ] The header states the SIMPLE invariant, because it is now true
- [ ] `Zenith_TerrainComponent.cpp`'s AABB comment says the box clips the epsilon band rather than only accelerating
- [ ] No behaviour change beyond the earlier free: the query still gates on null geometry, and no test count moves

<!-- agent:worklog:start ZEN-4 -->
ran on sonnet/low via SIMPLE+MEDIUM · category Engine

**Done** — merged to `master` as `71bbd6ab`. **Claimed via `zagent next` in queue mode**, the first tick this session not to name a key.

`CleanupPriorGenerationForRegenerate` now frees `m_pxPhysicsChunkSpans` in the same block it discards `m_pxPhysicsGeometry`, so the header states the **simple** invariant because it is finally true. Previously it held only via a two-step argument spanning the query's null-gate and `LoadCombinedPhysicsGeometryCore`'s unconditional clear — and the original argument for it was **wrong on its premise** (it claimed the caller always reloads; there is an early-return path when `DeleteExistingTerrainFilesInDirectory` fails). The conclusion survived by a different mechanism than the one anybody had checked.

The reviewer independently traced all eight discard/transfer sites and confirmed both members end null-or-matched together after each, and that `FreePhysicsChunkSpans()` is null-safe, idempotent, and compiled in every configuration.

**The blocking finding was the absence of evidence, not a defect.** The fixed line had **zero execution coverage**, and the one adjacent test asserts the operation is rejected *before* cleanup runs — checking a sentinel file still exists specifically to prove cleanup was never reached. Green gates and unmoved pins said nothing about it.

Fix-forward 1/3 added `TerrainPhysicsGeometryCleanup/CleanupPriorGenerationForRegenerateFreesGeometryAndSpans`, which drives the function directly against members populated through a **real** `LoadCombinedPhysicsGeometryCore` call, asserting both non-null *before* cleanup so the after-check cannot pass vacuously.

The `#ifdef ZENITH_TOOLS` guard matters for a sharper reason than "it would compile out": `Zenith_TerrainComponent.cpp` includes the `.inl` unconditionally, so without it this would **break the build in every `ToolsEnabled=False` config for every game**.

Pins +1 each, observed: **3366 / 1650 / 1741**. Gates: 6 legs green.

Note the DoD said "no test count moves" — that was the filer's assumption and it was wrong. Preserving a count at the cost of covering the line this ticket exists to fix is the inverse of what anyone wants.

Filed **ZEN-5**: `Zenith_TerrainComponent.h:522` has `//private:` commented out, so the members this invariant governs are public and nothing enforces the "every site in this file is the only writer" premise it rests on. Found independently by three agents.
<!-- agent:worklog:end -->
