# Zenith2 Budgets

Budgets v1. Every numeric limit Zenith2 is measured against, the procedure that measures it, the workloads it is measured in, and what happens when it is breached. Budget rows carry IDs `Z2-B-<AREA>-NNN`; tickets cite them in their `## Budgets` section.

## Purpose versioning and change rule

### Purpose

<!-- informative -->
This document is the single home of every budget in Zenith2. A budget row is a named, measurable quantity with a limit, a unit, a measurement procedure, a platform, a tolerance, an owning area, the workloads it is measured in, and a defined consequence when the limit is breached. Tickets cite rows by ID. The acceptance mode of the coverage gate step checks that every row a stage's tickets cite exists here and carries a concrete value. Research measurements of architectural alternatives (ECS backends, shading paths) are not budgets: they are governed by `Zenith2/Docs/Rendering.md → "Shading path research methodology"` and by the research procedure in this document, and their conclusions are decision records.
<!-- /informative -->

- [Z2-N-BUDGET-001] Every numeric budget limit (a performance, memory, package, storage, capacity, iteration-time or gate-time limit) that Zenith2 code, content or tooling is held to lives in this document as a budget row with an ID of the form `Z2-B-<AREA>-NNN`; a limit stated anywhere else (a design doc, a ticket, a test) cites the row ID instead of restating the number.
- [Z2-N-BUDGET-002] `<AREA>` in a row ID is one of the 41 area codes (CORE, JOBS, PLAT, ECS, SCENE, STREAM, ASSET, PACK, RHI, GEOM, LIGHT, ENV, ANIM, PHYS, AI, GAME, SAVE, AUDIO, UI, VFX, CINE, EDIT, DEVT, SHIP, ADVGFX, NET, MOD, GRAPH, COMPRESS, SCRIPT, TWOD, CONTENT, WORLD, RPROD, CHAR, ONLINE, SEC, MAINT, RESEARCH, SCOPE, PROC), and that area is the row's owner.
- [Z2-N-BUDGET-003] Procedure parameters (repetition counts, sampling rates, and the thresholds and constants inside a measurement procedure or a breach rule) are not budget limits: they are listed in the table "Procedure parameters" under "Measurement procedure catalogue", and a change to one follows the workload change rule (a decision plus a version bump of every workload whose procedures use it).
- [Z2-N-BUDGET-004] Row IDs are assigned only in this document, are never renumbered, and are never reused for a different quantity, including after the row is removed by a decision.

### Version and adoption

| Item | Value |
|---|---|
| Document version | v1 |
| Adopted by | Z2-D-012 (Budgets v1 adopted; a change needs a decision plus a workload version bump) |
| Workload versions in force | W-Indoor v1, W-Outdoor v1, W-Characters v1, W-Load v1, W-Editor v1, W-Dense v1, W-UI2D v1, W-OpenWorld v1, W-Cinematic v1, W-Game2D v1 |
| Row count | see "Row index" |

- [Z2-N-BUDGET-005] A change to an adopted row's value, unit, tolerance, platform, workload list or measurement procedure requires a new decision, appended to `Zenith2/Docs/DecisionLog.md` in that same diff with the next explicit `Z2-D-NNN` as `Zenith2/Docs/DecisionLog.md → "How this log is written"` defines (`zagent decide --ID Z2-D-NNN` is only an optional knowledge-row mirror and never writes the log), and, in the same diff, a version bump of every workload the row is measured in and a major version bump of this document.
- [Z2-N-BUDGET-006] A change to a workload's content, scripted path, duration, graphics settings or metrics requires a decision and a version bump of that workload (`v1` to `v2`), and evidence recorded against the old workload version is invalid for the new one.
<!-- elaboration -->
- [Z2-N-BUDGET-007] Allocating a pending row (writing its first value and changing its status to `adopted`) is done by the row's named producer ticket (a `needs-human` ruling ticket, see "Producer ticket shape") in the same diff that writes the value and appends the allocating decision to `Zenith2/Docs/DecisionLog.md` with the next explicit `Z2-D-NNN`, and bumps this document's minor version (`v1` to `v1.1`, and so on) without a workload version bump, because no measured workload changes.
- [Z2-N-BUDGET-008] A row is removed only by a decision; the removed row's ID, name and last value are recorded in that decision's DecisionLog entry and the ID is not reassigned.

### Row fields

- [Z2-N-BUDGET-009] Every row table in this document has exactly the columns of the table below, in this order, and the `Row index` repeats the ID, Name, Status, Value, Unit and Producer columns of every row.

| Column | Content |
|---|---|
| ID | `Z2-B-<AREA>-NNN`, three digits, assigned per area in order of first assignment and never renumbered; a row added after v1 takes its area's next free number wherever it appears in the document |
| Name | the descriptive row name other documents use in prose |
| Status | `adopted` (a number exists) or `pending` (a producer ticket allocates it) |
| Value | the limit; `pending` for a pending row; where the row covers two platforms, one value per platform (`Win: …; S24: …`); a memory row resident on both sides of the Windows CPU/GPU split, as listed in "Allocation sums", carries `Win RAM: …; Win VRAM: …; S24: …`; where a row covers several workloads, paths or tiers, the allocation writes one value per cell |
| Unit | the unit of the value, always explicit (`ms`, `s`, `min`, `frames`, `fps`, `GB`, `MB`, `bytes`, `count`, `safepoints`, `%`, `°C`) |
| Procedure | the measurement procedure ID (`MP-…`), defined in "Measurement procedure catalogue" |
| Platform | `Win` (the reference desktop, measured on both the Vulkan and the D3D12 backend), `S24` (Galaxy S24 Ultra, Vulkan), `Repo` (the repository or a checkout), or `Host` (the gate host) |
| Tolerance | the allowance applied before a measured statistic counts as a breach; `0 (hard)` for a hard limit |
| Workloads | the workloads or workload sets the row is measured in (see "Workload sets") |
| Breach | the breach consequence code (see "Breach consequences") |
| Producer | for an adopted row, the decision that adopted it; for a pending row, the ticket that allocates it |

- [Z2-N-BUDGET-010] The Producer cell of an adopted row reads `Z2-D-NNN (plan text)` when the value is stated verbatim in the text that decision settles, and `derived from Z2-D-NNN` when this document infers the value from that decision; the bullet that explains a derived value is marked as an elaboration.
- [Z2-N-BUDGET-011] A row is breached when its measured statistic exceeds `Value + Tolerance` for an upper limit, or falls below `Value − Tolerance` for a lower limit (a row whose name ends in "floor" or whose value is written `≥`).
<!-- elaboration -->
- [Z2-N-BUDGET-012] A row whose Platform column reads `Win` is measured on the reference desktop under both the Vulkan and the D3D12 backend, and each backend's measurement is judged against the row separately.
- [Z2-N-BUDGET-013] The measured statistic of a row is the one its procedure defines; where a procedure defines no statistic, the statistic is the maximum over the measured interval.

### Row status and pending rows

- [Z2-N-BUDGET-014] A row is `adopted` when its Value holds a number and `pending` when its Value reads `pending`; there is no third status.
- [Z2-N-BUDGET-015] Every pending row names its producer ticket in the Producer column; the default producer is "Stage 1 per-system budget ticket for <AREA>", and every other producer is listed in the table "Producer tickets" under "Row index".
- [Z2-N-BUDGET-016] A ticket whose `## Budgets` section cites a pending row is `blocks`-linked from that row's producer ticket and is not claimable until the producer has written the value and a revalidation pass has restored the citing ticket's `Spec-Maturity: ready`.
- [Z2-N-BUDGET-017] In acceptance mode (`-Stage N`), the coverage gate step treats a cited row as existing only when it is present in this document with status `adopted`; a pending row cited by a stage ≤ N ticket fails stage N's acceptance.
- [Z2-N-BUDGET-018] A ticket body never contains a budget number that is not also a row value here; it cites the row ID, and where the row is pending it says "pending, allocated by <producer>".

### Producer ticket shape

- [Z2-N-BUDGET-019] Every producer that allocates a row is a `needs-human` TASK whose title begins with the row's exact producer name; it writes the values, the allocation table and the DecisionLog entry in one diff, and no agent claims it.
- [Z2-N-BUDGET-020] A "Stage 1 per-system budget ticket for <AREA>" is split per Z2-D-084 only where Stage 1 has something to measure for that area (a probe on the Stage 0 scaffold, a pulled-forward Stage 2 library, or the reference engine at `C:/dev/Zenith/Zenith`): an ordinary harness TASK titled "Stage 1 per-system budget harness for <AREA>" builds the measurement harness under `Zenith2/Benchmarks/Budgets/<AREA>/`, with a short CTest smoke run inside the normal Test step, and the `needs-human` ruling ticket that carries the producer name is `blocks`-linked from it.
- [Z2-N-BUDGET-021] Every other per-system producer is a single `needs-human` ruling ticket whose basis is reference-engine data or published data; the table "Per-system producer shape" says which areas have a harness half, and the "Producer tickets" table under "Row index" repeats it in its `Harness half` column.
- [Z2-N-BUDGET-022] A harness whose measurement basis names a workload's content scale is `blocks`-linked from that workload's Stage 1 workload scale ticket: the ECS harness from "Stage 1 workload scale ticket for W-Dense", and the ANIM harness from "Stage 1 workload scale ticket for W-Characters".
- [Z2-N-BUDGET-023] The per-system ruling tickets are ordered by `blocks` links: the CORE and JOBS rulings are blocked by no other allocation; the RPROD ruling is `blocks`-linked from the CORE and JOBS rulings; every other per-system ruling is `blocks`-linked from the CORE, JOBS and RPROD rulings, because MP-ALLOC checks every allocation against the reserve and headroom rows those three adopt; a ruling whose row is bounded by another area's row, such as Z2-B-STREAM-006 by Z2-B-ECS-003 ([Z2-N-BUDGET-473]), is also `blocks`-linked from that area's ruling; the "Producer tickets" table lists these links in its `Blocked by` column.
- [Z2-N-BUDGET-024] Where a harness half exists, the human runs its full measurement on the reference desktop and the S24 and commits the report under `Zenith2/Docs/Research/Budgets/` in the ruling ticket's diff; the loop's worker never allocates a row.
- [Z2-N-BUDGET-025] The W-Editor row, flagship acceptance, Stage 0 gate-row, gate-activation and Stage 6 Play-limit producers are single `needs-human` tickets with no harness half; the gate producers are PROC tickets that time the step's cold run in the same human session that activates it.
- [Z2-N-BUDGET-482] SHIP's per-system ruling, the "Stage 3 per-system budget ticket for SHIP", allocates Z2-B-SHIP-015 to -018 at Stage 3, not Stage 1: its Z2-D-084 harness half is DEVT's `Zenith2/Benchmarks/Run-Workload.ps1` (drafted in the Stage 3 sprint), and the WS-FRAME workloads it measures have no measurable content before Stage 3; it is `blocks`-linked from DEVT's `Run-Workload.ps1` tickets and from SHIP's own `RowSources.json` entries ticket as well as from the CORE, JOBS and RPROD rulings (ruling X365-SHIP-BUDGET-S3, 2026-10-16).
- [Z2-N-BUDGET-463] The Stage 1 per-system budget ticket for ASSET is the `needs-human` ruling that allocates the rows the table "Producer tickets" lists for it; its harness half, the "Stage 1 per-system budget harness for ASSET", is the ticket that delivers `Zenith_Bench_AssetLoading` under `Zenith2/Benchmarks/AssetLoading/` (the ASSET benchmark, not a harness under `Zenith2/Benchmarks/Budgets/ASSET/`) with its `--smoke` CTest run, per `Zenith2/Docs/Design/ASSET.md` → "Stage 1".

### Per-system producer shape

<!-- elaboration -->
| Area | Harness half | What Stage 1 can measure |
|---|---|---|
| CORE | yes | `Check-Allocation.ps1` and its fixtures; one probe, `Budgets_CoreArenaPoolProbe`: a synthetic frame-allocation pattern over `Zenith_Arena` and `Zenith_Pool` from the pulled-forward Core containers, measuring block overhead and fragmentation for Z2-B-CORE-004 and Z2-B-CORE-005; the other CORE rows are allocated from reference-engine data |
| JOBS | yes | the scheduler-only micro-workload `Budgets_JobsSchedulerOnly` on the reference engine's `Zenith/TaskSystem` at `C:/dev/Zenith/Zenith`, for Z2-B-JOBS-001; the reference engine's queue depths and job memory, for the queue-capacity and resident-memory rows |
| GRAPH | yes | the pulled-forward `ZenithGraph` schedule compilation |
| ECS | yes | native custom, flecs and EnTT micro-benchmarks of the W-Dense component mix |
| SCRIPT | yes | the D-LUAU-STATELESS (open, Stage 1) prototype VM: safepoint counts, VM heap sizes, snapshot and checkpoint bytes |
| RHI | yes | the D-SLANG (open, Stage 1) bindless probe on the Player stub: descriptor heap sizes, upload-ring throughput |
| RPROD | yes | the reference engine's Flux culling, prepass and forward passes on the reference desktop and the S24 |
| LIGHT | yes | the reference engine's shadow, SSAO, fog, TAA and post passes |
| GEOM | yes | the reference engine's vertex pools, texture residency and compute skinning |
| ANIM | yes | the reference engine's skeletal animation update at the W-Characters scale |
| COMPRESS | yes | the pulled-forward `ZenithCompression` benchmark `Zenith_Bench_Compression` at `Zenith2/Benchmarks/Compression/`, for the two decompression throughput floors |
| PLAT | yes | the MP-MEM S24 sampler (the `VmRSS` and kgsl readers and the runner-side `dumpsys` sampler) and its device test, plus the input-phase cost of the reference engine's input layer for Z2-B-PLAT-001; Z2-B-PLAT-002 to Z2-B-PLAT-006 are allocated at Stage 1 from reference-engine, derived and published bases, the PLAT harnesses under `Zenith2/Benchmarks/Platform/` (`Zenith_Bench_Platform` with its `pump`, `critical-read`, `resume-write`, `startup-build` and `memory` cases: the harness extension and comparator ticket Z2T-PLAT-100 and the reference-hardware run Z2T-PLAT-101) confirm them by measurement at Stage 2, and the WS-FRAME and WS-SLICE workload measurements refine them from Stage 3 |
| PHYS | yes | the D-SEL-PHYSX-ANDROID (open, Stage 1) harness: Jolt and PhysX step times |
| ASSET | yes | the ASSET benchmark `Zenith_Bench_AssetLoading` at `Zenith2/Benchmarks/AssetLoading/`: decode throughput and per-slice cost of generated cooked-shaped sections through the pulled-forward `ZenithCompression` on both backends, and file-read throughput and latency, for Z2-B-ASSET-001, Z2-B-ASSET-002 and the staging and in-flight rows that are members of Z2-B-ASSET-001 |
| ADVGFX, AI, AUDIO, CHAR, CINE, CONTENT, DEVT, EDIT, ENV, GAME, PACK, SAVE, SCENE, STREAM, TWOD, UI, VFX, WORLD | no | nothing Zenith2-specific; the ruling cites reference-engine or published data |
| SHIP | no | nothing: its rows are measured over the WS-FRAME workloads, which have no measurable content before Stage 3, so its ruling is the Stage 3 per-system budget ticket for SHIP ([Z2-N-BUDGET-482]) |

### Allocation rule for pending rows

<!-- informative -->
The adopted totals (frame rate, memory, boot time) are the envelope. The Stage 1 per-system budget tickets split that envelope into per-system rows. The rules below keep the split consistent, so that every per-system row being met implies that the totals are met.
<!-- /informative -->

<!-- elaboration -->
- [Z2-N-BUDGET-026] Memory rows are allocated against three bounds: on Windows, the rows of bound `MEM-WIN-RAM` sum to at most Z2-B-CORE-002 (8 GB) × (1 − Z2-B-CORE-011) and the rows of bound `MEM-WIN-VRAM` to at most Z2-B-CORE-003 (6 GB) × (1 − Z2-B-CORE-011); on the S24, the rows of bound `MEM-S24` sum to at most Z2-B-CORE-001 (3.0 GB) × (1 − Z2-B-CORE-011); the reserve covers fragmentation, driver overhead and measurement noise.
<!-- elaboration -->
- [Z2-N-BUDGET-027] Per-system CPU rows are allocated per thread path of the compiled CPU frame schedule: the rows of bound `CPU-SIM` (the simulation critical path, phases 1 to 12 and the engine services scheduled with them) sum to at most Z2-B-SHIP-001 × (1 − Z2-B-JOBS-003), and the rows of bound `CPU-RENDER` (render-graph build, recording and submit) meet the same bound separately, per platform, because pipelining overlaps the two paths (Z2-D-050).
<!-- elaboration -->
- [Z2-N-BUDGET-028] GPU per-pass rows are allocated per (platform, shading path): the rows of the bound `GPU-<PATH>` (common prologue, shadow views, the path's per-view passes for the main view, common epilogue) sum to at most Z2-B-SHIP-001 × (1 − Z2-B-RPROD-028) on each platform the path runs on (Forward+ and Forward on Windows and the S24; Deferred and Visibility buffer on Windows only).

<!-- informative -->
Summing per-node or per-pass p95 values is a planning bound, not a theorem: the p95 of a per-frame sum can exceed the sum of the parts' p95 values when the parts peak on different frames. The bound is conservative in the usual case, where scene load drives every pass up and down together; the frame-time rows stay the binding check, and the allocation merely makes a frame-time breach attributable.
<!-- /informative -->

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-CORE-011 | Memory allocation reserve fraction | pending | pending | % of platform total | MP-ALLOC | Win, S24 | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for CORE |
| Z2-B-JOBS-003 | CPU critical-path allocation headroom | pending | pending | % of frame-time p95 | MP-ALLOC | Win, S24 | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for JOBS |
| Z2-B-RPROD-028 | GPU frame allocation headroom | pending | pending | % of frame-time p95 | MP-ALLOC | Win, S24 | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for RPROD |

- [Z2-N-BUDGET-029] The allocation check (MP-ALLOC) is run by every per-system ruling ticket before it lands, and a ruling whose allocation breaks any bound does not land.
- [Z2-N-BUDGET-030] The bounds and their exact member rows are the table "Allocation sums"; every row of unit `MB` under "Memory budgets", "Rendering rows" or "Scripting rows", and every row of unit `ms` whose name ends in "CPU" or "GPU" or contains "CPU per", is either a member of a bound or listed under "Excluded from every bound" with its reason, and `Check-Allocation.ps1` exits 2 when it finds such a row in neither list.

### Allocation sums

Member cells use exactly these tokens, separated by `, ` or `; `: `<ID>`; `<ID> ×<n>`; `max(<ID>, <ID>, …)`; the labels `dual-resident:`, `prologue:`, `per view:` and `epilogue:`; and two back-references, `the prologue and epilogue of GPU-FORWARD_PLUS` and `every member of MEM-WIN-RAM and MEM-WIN-VRAM, each once, at its S24 value`. `×2` counts a row twice (the HiZ pyramid is built twice per view); `max(…)` adds the largest of the listed rows; a dual-resident row contributes its `Win RAM` value to `MEM-WIN-RAM`, its `Win VRAM` value to `MEM-WIN-VRAM` and its `S24` value to `MEM-S24`.

| Bound | Platforms | Member rows | Limit |
|---|---|---|---|
| MEM-WIN-RAM | Win | Z2-B-CORE-004, Z2-B-CORE-005, Z2-B-CORE-012, Z2-B-CORE-013, Z2-B-ECS-001, Z2-B-SCENE-001, Z2-B-STREAM-001, Z2-B-ASSET-001, Z2-B-PHYS-001, Z2-B-AI-001, Z2-B-AUDIO-001, Z2-B-SAVE-001, Z2-B-DEVT-001, Z2-B-JOBS-004, Z2-B-RHI-006, Z2-B-RHI-011, Z2-B-RHI-012, Z2-B-PLAT-006, Z2-B-SCRIPT-009, Z2-B-SCRIPT-011, Z2-B-SCRIPT-013, Z2-B-PACK-009; dual-resident: Z2-B-RPROD-002, Z2-B-VFX-001, Z2-B-ANIM-001, Z2-B-UI-001, Z2-B-CINE-001; max(Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008) | Z2-B-CORE-002 × (1 − Z2-B-CORE-011) |
| MEM-WIN-VRAM | Win | Z2-B-GEOM-001, Z2-B-GEOM-002, Z2-B-GEOM-008, Z2-B-LIGHT-018, Z2-B-LIGHT-019, Z2-B-RHI-001, Z2-B-RHI-002, Z2-B-RPROD-001, Z2-B-ADVGFX-001, Z2-B-RHI-010; dual-resident: Z2-B-RPROD-002, Z2-B-VFX-001, Z2-B-ANIM-001, Z2-B-UI-001, Z2-B-CINE-001; max(Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008) | Z2-B-CORE-003 × (1 − Z2-B-CORE-011) |
| MEM-S24 | S24 | every member of MEM-WIN-RAM and MEM-WIN-VRAM, each once, at its S24 value; max(Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008) | Z2-B-CORE-001 × (1 − Z2-B-CORE-011) |
| CPU-SIM | Win, S24 | Z2-B-PLAT-002, Z2-B-PLAT-001, Z2-B-ENV-002, Z2-B-SCRIPT-004, Z2-B-ANIM-002, Z2-B-GAME-001, Z2-B-PHYS-002, Z2-B-ANIM-003, Z2-B-AI-002, Z2-B-SCENE-002, Z2-B-SCENE-003, Z2-B-GAME-002, Z2-B-SCRIPT-005, Z2-B-RPROD-003, Z2-B-ECS-002, Z2-B-JOBS-001, Z2-B-SCRIPT-008, Z2-B-STREAM-002, Z2-B-AUDIO-002, Z2-B-UI-002, Z2-B-VFX-002, Z2-B-VFX-005, Z2-B-CINE-002, Z2-B-DEVT-002, Z2-B-ASSET-008 | Z2-B-SHIP-001 × (1 − Z2-B-JOBS-003) |
| CPU-RENDER | Win, S24 | Z2-B-RHI-004, Z2-B-RHI-005, Z2-B-TWOD-001 | Z2-B-SHIP-001 × (1 − Z2-B-JOBS-003) |
| GPU-FORWARD_PLUS | Win, S24 | prologue: Z2-B-RPROD-004, Z2-B-GEOM-004, Z2-B-VFX-003, Z2-B-GEOM-005, Z2-B-LIGHT-001, Z2-B-LIGHT-002, Z2-B-ADVGFX-003, Z2-B-LIGHT-017; per view: Z2-B-RPROD-005, Z2-B-RPROD-006, Z2-B-RPROD-007 ×2, Z2-B-RPROD-008, Z2-B-LIGHT-003, Z2-B-LIGHT-004, Z2-B-LIGHT-005, Z2-B-LIGHT-006, Z2-B-LIGHT-016, Z2-B-RPROD-009, Z2-B-ENV-001, Z2-B-LIGHT-007, Z2-B-RPROD-010, Z2-B-VFX-004, Z2-B-ENV-004; epilogue: Z2-B-LIGHT-010, Z2-B-LIGHT-011, Z2-B-LIGHT-012, Z2-B-LIGHT-013, Z2-B-LIGHT-014, Z2-B-LIGHT-015, Z2-B-UI-003, Z2-B-DEVT-003 | Z2-B-SHIP-001 × (1 − Z2-B-RPROD-028) |
| GPU-FORWARD | Win, S24 | the prologue and epilogue of GPU-FORWARD_PLUS; per view: Z2-B-RPROD-005, Z2-B-RPROD-006, Z2-B-RPROD-007 ×2, Z2-B-RPROD-008, Z2-B-RPROD-011, Z2-B-LIGHT-004, Z2-B-LIGHT-005, Z2-B-LIGHT-006, Z2-B-LIGHT-016, Z2-B-RPROD-012, Z2-B-ENV-001, Z2-B-LIGHT-007, Z2-B-RPROD-010, Z2-B-VFX-004, Z2-B-ENV-004 | Z2-B-SHIP-001 × (1 − Z2-B-RPROD-028) |
| GPU-DEFERRED | Win | the prologue and epilogue of GPU-FORWARD_PLUS; per view: Z2-B-RPROD-005, Z2-B-RPROD-006, Z2-B-RPROD-007 ×2, Z2-B-RPROD-008, Z2-B-RPROD-014, Z2-B-GEOM-006, Z2-B-LIGHT-005, Z2-B-LIGHT-006, Z2-B-LIGHT-016, Z2-B-LIGHT-003, Z2-B-LIGHT-004, Z2-B-LIGHT-008, Z2-B-ENV-001, Z2-B-LIGHT-007, Z2-B-RPROD-010, Z2-B-VFX-004, Z2-B-ENV-004 | Z2-B-SHIP-001 × (1 − Z2-B-RPROD-028) |
| GPU-VISBUF | Win | the prologue and epilogue of GPU-FORWARD_PLUS; per view: Z2-B-RPROD-015, Z2-B-RPROD-016, Z2-B-RPROD-007 ×2, Z2-B-RPROD-017, Z2-B-LIGHT-003, Z2-B-RPROD-018, Z2-B-RPROD-019, Z2-B-LIGHT-005, Z2-B-LIGHT-016, Z2-B-LIGHT-004, Z2-B-LIGHT-009, Z2-B-ENV-001, Z2-B-LIGHT-007, Z2-B-RPROD-010, Z2-B-VFX-004, Z2-B-ENV-004 | Z2-B-SHIP-001 × (1 − Z2-B-RPROD-028) |

Excluded from every bound, with the reason:

| Reason | Rows |
|---|---|
| a per-heap residency cap over rows already in `MEM-WIN-VRAM`; adding it would count them twice | Z2-B-RHI-003 |
| editor or tool process only; neither is measured against the Player totals | Z2-B-EDIT-001, Z2-B-EDIT-011, Z2-B-EDIT-015, Z2-B-WORLD-002, Z2-B-WORLD-004 |
| covered by Z2-B-SCRIPT-011 (the per-worker cap times the worker count) | Z2-B-SCRIPT-010 |
| per-entity, per-instance and per-tick caps drawn from arenas and pools already counted (Z2-B-CORE-005, Z2-B-SCRIPT-013) | Z2-B-SCRIPT-012, Z2-B-SCRIPT-015, Z2-B-SCRIPT-016 |
| bandwidth per frame, not residency | Z2-B-GEOM-003, Z2-B-RPROD-026 |
| on-disk size, not residency | Z2-B-RPROD-030 |
| CPU spent inside the script phase spans Z2-B-SCRIPT-004 and Z2-B-SCRIPT-005 | Z2-B-SCRIPT-006, Z2-B-SCRIPT-014 |
| occasional frames, governed by the hitch rows | Z2-B-SAVE-002, Z2-B-GRAPH-001 |
| a dedicated thread outside the frame graph's simulation and render paths, with its own row (the real-time mixer thread; the video decode thread) | Z2-B-AUDIO-003, Z2-B-CINE-003 |
| a fatal watchdog, not an allocation | Z2-B-SCRIPT-003 |
| a share of a member row that already counts it (Z2-B-ASSET-001 for the asset admission, staging and cache budgets; Z2-B-STREAM-001 for the environment CPU data; Z2-B-ANIM-001 for the animation stream cache) | Z2-B-ASSET-005, Z2-B-ASSET-006, Z2-B-ASSET-007, Z2-B-ENV-006, Z2-B-ANIM-006 |
| measured inside the span or pass of a member row (Z2-B-RPROD-003 for the environment extractors, Z2-B-GEOM-005 for detail foliage generation, Z2-B-RHI-004 for LIGHT's graph-build work, the prepass, opaque and shadow-view rows for the terrain draws, Z2-B-RPROD-003 for the animation extractor and for the modular part extraction) | Z2-B-ENV-003, Z2-B-ENV-005, Z2-B-LIGHT-020, Z2-B-GEOM-007, Z2-B-ANIM-009, Z2-B-CHAR-003 |
| a second measurement of a member row under a different setting (Z2-B-ANIM-002 with significance reduction enabled) | Z2-B-ANIM-004 |

### Allocation file and check

- [Z2-N-BUDGET-031] Every per-system ruling ticket commits its allocation as `Zenith2/Docs/Research/Budgets/Stage1/<AREA>/allocation.json` in the format of the block "Allocation file format", listing one entry per (row, platform, path) cell it allocates and one `sums` entry per bound any of its rows belongs to; a row of "Approximation error rows" (platform `Host`, a member of no bound, [Z2-N-BUDGET-281]) has no `rows` entry, and its ruled values live in its Value cell in this document alone.
- [Z2-N-BUDGET-032] `pwsh -NoProfile -File Zenith2/Benchmarks/Budgets/Check-Allocation.ps1 [-Area <AREA>] [-Final]` reads the "All rows" table and the "Allocation sums" table of `Zenith2/Docs/Budgets.md` and every `Zenith2/Docs/Research/Budgets/Stage1/*/allocation.json`, recomputes every bound, writes `Zenith2/out/<checkout-hash>/budgets/allocation-check.json`, and exits 0 when every bound holds, 1 when a bound is broken, and 2 when a file is malformed or an allocated value differs from the row's value in `Budgets.md`.
- [Z2-N-BUDGET-033] Without `-Final`, a pending member row contributes 0 and its bound reports `incomplete: true`; with `-Final`, which the Stage 1 acceptance runs, a pending member row fails its bound.
- [Z2-N-BUDGET-034] `Check-Allocation.ps1` and its fixtures under `Zenith2/Benchmarks/Budgets/Fixtures/` are built by the Stage 1 per-system budget harness for CORE, which registers the CTest test `Budgets_CheckAllocation_Fixtures`: one passing fixture, one fixture breaking each bound kind (memory, CPU, GPU), one with a value mismatch and one malformed file, each asserting the documented exit code.
- [Z2-N-BUDGET-035] Units are converted before summing: `GB` and `MB` to bytes by 10^9 and 10^6, `KB` by 10^3, `%` rows to fractions; a member row with two units (for example `entries, MB`) contributes its memory-unit value; a member row with no memory unit in a memory bound, or not `ms` in a CPU or GPU bound, makes the check exit 2.

Allocation file format:

```json
{
	"schema": "zenith2.allocation.v1",
	"area": "CORE",
	"budgetsVersion": "v1.1",
	"decision": "Z2-D-NNN",
	"rows": [
		{ "id": "Z2-B-CORE-004", "platform": "Win RAM", "path": null, "value": 64, "unit": "MB", "basis": "harness", "evidence": "Zenith2/Docs/Research/Budgets/Stage1/CORE/<record>.json" }
	],
	"sums": [
		{ "bound": "MEM-WIN-RAM", "total": 8000000000, "headroomRow": "Z2-B-CORE-011", "limit": 7200000000, "sum": 5100000000, "incomplete": true, "pass": true }
	]
}
```

`platform` is one of `Win`, `Win RAM`, `Win VRAM`, `S24` (`Host` is not a value: an approximation-error row is ruled in its Value cell, not in this file); `path` is one of `FORWARD_PLUS`, `FORWARD`, `DEFERRED`, `VISBUF` or `null`; `basis` is one of `harness`, `reference-engine`, `published`, `derived`; `evidence` is a repo-relative path or a citation.
- [Z2-N-BUDGET-036] A producer ticket that allocates rows commits, with the values, its `allocation.json` (see "Allocation file and check") showing each bound its rows belong to against its limit, and the basis of every split (a Stage 1 harness measurement, a measurement of the reference engine at `C:/dev/Zenith/Zenith`, or published reference-engine data, each cited), and the evidence records of any measurement it used.
- [Z2-N-BUDGET-037] Per-system rows are split from the adopted totals in Stage 1, before most systems exist; the first stage acceptance that measures a system confirms its rows, and a measured breach there follows the regression rule (fix, or re-budget by a decision).

### Breach consequences

| Code | Consequence |
|---|---|
| BR-ACCEPT | The breach blocks the stage acceptance of every stage whose acceptance measures the row. A BUG ticket is filed on the owning area's epic, citing the row ID, the evidence record and the workload version. The block clears only when a new evidence record shows the row met, or a decision re-budgets the row (with a workload version bump). |
| BR-INVALID | The run that breached is invalid. No evidence record from it is accepted for any row, and the run is repeated. |
| BR-LUAU | A controlled Luau error is raised inside the offending entity's transaction. The entity's writes and side effects roll back, the error is reported, and the failing system or behaviour is disabled from the next tick boundary until reload (`Zenith2/Docs/Scripting.md → "Commit and failure semantics"`). |
| BR-MERGE | The entity is rejected at the deterministic merge point in merge order, rolled back as an entity-level failure, and a diagnostic is logged (`Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"`). |
| BR-FATAL | A fatal, non-replayable diagnostic event, never a recoverable rollback (`Zenith2/Docs/Scripting.md → "Budgets safepoints and the watchdog"`). |
| BR-STALL | The producer waits until capacity frees; an exhaustion telemetry counter increments and is shown by the on-screen profiler. |
| BR-REFUSE | A single request larger than the limit is split where the API supports splitting, and otherwise refused with a `Zenith_Result` error. |
| BR-GOVERNOR | The memory governor raises the pressure level and the affected subsystems shed per their defined policy (`Zenith2/Docs/Architecture.md → "Memory governor"`). |
| BR-COOK | A cook-time error; the cook fails and names the asset and the row. |
| BR-GATE | The gate phase fails with exit 4; the running line's process tree is killed (exit 124), the line is recorded in `steps`, and a phase-cap kill is never flake-retried. |
| BR-DEFER | The request is deferred to a subsequent frame (round-robin); a deferral counter increments. |
| BR-DROP | The excess is dropped, not deferred, and a dropped counter increments: fixed simulation steps beyond the cap are not run and their accumulated time is discarded (`Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`, counter `sim.dropped_ticks`); a full log ring drops its oldest records (counter `log.router.dropped`). |
| BR-LOAD | The module load fails with a `LOAD` error and the module is not loaded; the world continues without it (`Zenith2/Docs/Scripting.md → "Modules, bindings and generated types"`). |
| BR-TEST | The test or suite that measures the row fails, which fails the gate step or acceptance suite running it. |
| BR-FALLBACK | The engine selects the defined fallback configuration the row's bullet names, and a counter shown by the on-screen profiler records each selection. |
| BR-REPORT | Reported in the run's evidence record and the on-screen profiler; the row gates nothing. |

- [Z2-N-BUDGET-038] Every row carries exactly one breach code from the table "Breach consequences", and the engine, the tools and the gate steps implement that consequence exactly as the table defines it.
- [Z2-N-BUDGET-039] A benchmark regression beyond its row's tolerance blocks the stage acceptance until it is fixed or re-budgeted by a decision; no stage acceptance ticket is completed while a BR-ACCEPT breach against a row it measures is open.
- [Z2-N-BUDGET-040] A regression BUG ticket filed for a BR-ACCEPT breach carries `--severity S2_MAJOR` when the breach is within 10% of the row's value and `S1_CRITICAL` when it exceeds 10% or the row is a hard platform total (frame-rate, memory total, cold boot), cites the row ID in `## Budgets`, and names the evidence record path in `## Validation`.
- [Z2-N-BUDGET-041] Runtime breach consequences (BR-LUAU, BR-MERGE, BR-FATAL, BR-STALL, BR-REFUSE, BR-GOVERNOR, BR-DEFER, BR-DROP, BR-COOK, BR-LOAD, BR-FALLBACK) are exercised by tests that force the breach, and those tests are listed in the owning area's design doc.

### Tolerance defaults

<!-- elaboration -->
| Row kind | Default tolerance |
|---|---|
| Adopted platform totals (frame time, sustained floor, memory totals, cold boot, package limits) | 0 (hard) |
| Per-system CPU and GPU time rows | 5% of the value, or 0.05 ms, whichever is larger |
| Memory rows other than totals | 2% of the value, or 4 MB, whichever is larger |
| Count and capacity rows | 0 (hard) |
| Editor iteration and walkthrough rows | 10% of the value |
| Install-size and storage rows | 1% of the value |
| Gate time rows | 0 (hard) |

<!-- elaboration -->
- [Z2-N-BUDGET-042] A row whose Tolerance column reads `default` uses the default tolerance of its row kind from the table "Tolerance defaults"; a producer ticket may set a narrower per-row tolerance and records it in the row, and a wider one needs a decision.

## Frame-rate definitions and measurement procedure

### Frame-rate definitions

- [Z2-N-BUDGET-061] "60 fps" means that, over a run's measured interval, the p95 frame time is at most 16.7 ms and the p99 frame time is at most 20.0 ms (rows "Frame time p95" and "Frame time p99").
<!-- elaboration -->
- [Z2-N-BUDGET-062] Frame time is the interval between the timestamps of two consecutive main-view presents, each taken on the render-submit thread immediately after the present call returns, with the monotonic high-resolution clock (`QueryPerformanceCounter` on Windows, `CLOCK_MONOTONIC` on Android), in nanoseconds; frame `i`'s time is `t(i) − t(i−1)`.
<!-- elaboration -->
- [Z2-N-BUDGET-063] The p95 and p99 of a run are computed by the nearest-rank method over the frame times of the measured interval: sort the `N` frame times ascending; p95 is the value at 1-based rank `ceil(0.95 × N)` and p99 the value at rank `ceil(0.99 × N)`; no interpolation, no outlier removal.
<!-- elaboration -->
- [Z2-N-BUDGET-064] Every frame-rate workload (W-Indoor, W-Outdoor, W-Characters, W-Dense, W-UI2D, W-OpenWorld, W-Cinematic, W-Game2D) targets "60 fps" on both production platforms, and the frame-time rows apply to every one of them.
- [Z2-N-BUDGET-065] Background streaming, runtime asset uploads, and shader or PSO compilation and creation always count toward the frame-time statistics; the only frames excluded from a run are those of the initial-load exclusion window.

### Measured interval and the initial-load exclusion

- [Z2-N-BUDGET-066] Only the first 10 s after the initial load is excluded from a run's frame statistics, once per run; no other interval is excluded for any reason, including a subsequent load, a world transition, a teleport or a PSO creation.
<!-- elaboration -->
- [Z2-N-BUDGET-067] The initial load ends at the first present after the Player emits its world-ready telemetry marker `PLAYER_EVENT_WORLD_READY` for the workload's start scene (`Zenith2/Docs/Architecture.md → "Composition root and Player"`), and that present's timestamp is recorded as `tReadyNs`; the exclusion window is `[tReady, tReady + 10 s)` on the present-timestamp clock, and the measured interval is every frame whose present timestamp is at or after `tReady + 10 s` until the run's scripted path ends.
<!-- elaboration -->
- [Z2-N-BUDGET-068] Frames presented before `tReady` belong to the boot measurement of W-Load and are not part of any frame-rate statistic.
- [Z2-N-BUDGET-069] Every run reports its excluded intervals in its evidence record (field `excludedIntervals`, a list of `{startNs, endNs, reason}`), and a run whose record lists any excluded interval other than the single initial-load window is invalid (BR-INVALID).

### Run length

- [Z2-N-BUDGET-070] A workload run lasts at least 120 s and at least 7,200 frames; a run shorter than either is invalid (rows "Minimum measured run duration" and "Minimum measured run frame count").
<!-- elaboration -->
- [Z2-N-BUDGET-071] Both minimums apply to the measured interval, after the initial-load exclusion, so a run's total wall-clock length is at least 130 s plus its boot.
- [Z2-N-BUDGET-072] A workload whose scripted path is shorter than the minimum run length loops its path from the start point until both minimums are met, and the evidence record states the loop count.

### Warm-up

- [Z2-N-BUDGET-073] The warm-up of every workload run is the initial-load exclusion window; qualification runs use no other warm-up.
<!-- elaboration -->
- [Z2-N-BUDGET-074] A research run additionally discards one complete warm-up repetition, run immediately before its measured repetitions on the same boot of the device, so the measured repetitions start with the OS page cache, driver caches and clocks in steady state.
- [Z2-N-BUDGET-075] Cache state is a run parameter, never a warm-up side effect: a run is either cold-cache or warm-cache, as defined in "Cache state pairs", and the evidence record states which.

### Repetitions and statistics

- [Z2-N-BUDGET-076] Research runs use at least 10 repetitions per (workload, platform, backend, configuration) and report medians with 95% confidence intervals, plus the run-to-run variance.
<!-- elaboration -->
- [Z2-N-BUDGET-077] The 95% confidence interval of a research median is the distribution-free order-statistic interval: for `n` repetitions sorted ascending, the interval is `[x(j), x(k)]` with `j` and `k` the ranks given by the binomial(n, 0.5) quantiles at 0.025 and 0.975; for `n = 10` that is `[x(2), x(9)]`.
<!-- elaboration -->
- [Z2-N-BUDGET-078] Qualification runs use at least 3 repetitions per (workload, platform, backend, cache state), and every repetition individually meets every row it measures; a single failing repetition is a breach, and a median is never used to pass a qualification row.
- [Z2-N-BUDGET-079] Raw data of research runs (per-frame capture files and evidence records) is committed under `Zenith2/Docs/Research/`, and the report states the hardware, driver, OS build, clock settings and repetition count.

### Controlled and ordinary conditions

- [Z2-N-BUDGET-080] Research measurements run under controlled conditions: fixed hardware, drivers and clocks; locked desktop GPU and CPU clocks; and Android fixed-performance mode where the device exposes it.
- [Z2-N-BUDGET-081] Research reports still state run-to-run variance, because Android fixed-performance mode can overheat the device and does not remove scheduling noise.
- [Z2-N-BUDGET-082] Shipping qualification runs devices in their ordinary state, including thermal throttling, with no clock locking and no fixed-performance mode.
<!-- elaboration -->
- [Z2-N-BUDGET-083] On the reference desktop, controlled conditions lock the GPU core and memory clocks with `nvidia-smi --lock-gpu-clocks=<base>,<base>` and `--lock-memory-clocks=<rated>,<rated>`, and fix the CPU at its base frequency through a power plan with minimum and maximum processor state at the same value; ordinary conditions use the "Balanced" power plan and no locks.
<!-- elaboration -->
- [Z2-N-BUDGET-084] On the S24, controlled conditions enable fixed-performance mode with `adb shell cmd power set-fixed-performance-mode-enabled true` before the run and disable it after; ordinary conditions leave it disabled, with adaptive brightness off and screen brightness fixed at 50% for both.
- [Z2-N-BUDGET-085] Every evidence record states its condition class in the field `conditions` (`controlled` or `ordinary`), and a qualification row is never met by a controlled-conditions record.

### Dynamic-resolution floor rule

- [Z2-N-BUDGET-086] The dynamic-resolution floor is 75% of the output resolution: the reference desktop outputs 2560×1440 and the S24 outputs 2340×1080 (row "Dynamic-resolution floor").
<!-- elaboration -->
- [Z2-N-BUDGET-087] This document reads the 75% as applying per axis (its interpretation of the plan text of Z2-D-012), so the floor is 1920×1080 on the reference desktop and 1755×810 on the S24; the dynamic-resolution controller never renders the main view below the floor in a budget run.
- [Z2-N-BUDGET-088] A run that hits the floor (renders at least one measured frame at the floor scale) and still misses its frame-rate target fails; dynamic resolution never excuses a missed target.
<!-- elaboration -->
- [Z2-N-BUDGET-089] Every run records a histogram of the main view's per-frame render scale in 1% buckets (field `dynresHistogram`) and the count of measured frames at the floor (field `framesAtFloor`).

### Sustained thermal run procedure

- [Z2-N-BUDGET-090] "Never below 45" means that the 1-second rolling average frame rate stays at or above 45 fps through a 30-minute run, on an S24 that is unplugged, in 25 °C ambient air, and starts the run at 80% battery (rows "Sustained floor" to "Sustained run power state").
<!-- elaboration -->
- [Z2-N-BUDGET-091] The 1-second rolling average at present `i` is the number of presents with timestamps in `(t(i) − 1 s, t(i)]`; it is evaluated at every present of the measured interval, and a single evaluation below 45 is a breach.
<!-- elaboration -->
- [Z2-N-BUDGET-092] The sustained run procedure is, in order: charge the S24 to at least 85%, unplug it, let it idle with the screen off in the 25 °C room until the battery reads 80% and the device's skin temperature has been within 1 °C of ambient for 10 minutes; start the workload with ordinary conditions; run the looped scripted path for 30 minutes of measured interval; record `ambientC`, `batteryStartPct`, `batteryEndPct`, and the Android thermal status (`PowerManager.getCurrentThermalStatus`) sampled once per second.
<!-- elaboration -->
- [Z2-N-BUDGET-093] The sustained run is measured in every flagship slice and in W-Outdoor and W-Characters (workload set WS-SUSTAIN), and it runs on the S24 only; the reference desktop has no sustained-floor row.
- [Z2-N-BUDGET-094] The initial-load exclusion applies to the sustained run like any other run, so the 30 minutes are counted from `tReady + 10 s`.

### Evidence records and the input fingerprint

<!-- informative -->
An evidence record is the machine-readable result of one measured run. Stage acceptance reads evidence records, never prose. Validity is decided by an input fingerprint over everything the result depended on, not by how recent the file is, so a change anywhere in the inputs invalidates the evidence even if the file was written a minute ago.
<!-- /informative -->

- [Z2-N-BUDGET-095] Every measured run writes one evidence record as UTF-8 JSON, with exactly the fields of the block "Evidence record format" below; a run with no evidence record has measured nothing.
- [Z2-N-BUDGET-096] Every field of the block is present in every record: a field the run does not measure (for example `gfxSamples` on Windows, or `sustained` outside a sustained run) carries its default (`null`, `[]` or `0` as the block shows), never absent, so one reader parses every record.
- [Z2-N-BUDGET-097] An evidence record states the tested tree SHA and build configuration (CMake preset and configuration), the workload name and version, the platform and backend, the result per row, and the artifact location of the raw capture.
- [Z2-N-BUDGET-098] An evidence record stores an input fingerprint composed of: blob hashes of every source in the transitive target-dependency closure of the code under test (from the CMake File API plus libgraph); the toolchain and `Zenith2/ThirdParty/manifest.json` hashes; the shader-blob set hash; the content and workload version hashes; and the test-harness version.
- [Z2-N-BUDGET-099] Acceptance mode recomputes the fingerprint of every evidence record it reads and rejects any record whose fingerprint does not match the current inputs; recency never makes a record valid.
- [Z2-N-BUDGET-100] Tests in the coverage gate step prove that a change only in a dependency, a toolchain bump, a middleware bump and a content bump each invalidate an evidence record.
<!-- elaboration -->
- [Z2-N-BUDGET-101] Evidence records of loop and CI runs are written under `Zenith2/out/<checkout-hash>/evidence/<workload>/<platform>-<backend>/<utc-timestamp>-r<repetition>.json` and are never tracked; evidence records of human-run reference-hardware and device measurements are committed under `Zenith2/Docs/Research/Budgets/Stage<N>/<workload>/` together with their raw capture files, compressed with zstd.

Evidence record format:

```json
{
	"schema": "zenith2.evidence.v1",
	"treeSha": "<40-hex tree SHA of the tested tree>",
	"preset": "win-release",
	"configuration": "Release",
	"workload": "W-Outdoor",
	"workloadVersion": 1,
	"platform": "Win",
	"backend": "Vulkan",
	"device": { "cpu": "Ryzen 7 5800X", "gpu": "RTX 3070", "ramGB": 32, "driver": "<version>", "os": "<build>" },
	"conditions": "ordinary",
	"cacheState": "warm",
	"repetition": 1,
	"fingerprint": {
		"closure": "<sha256 over sorted (path, blob sha) of the target closure>",
		"toolchain": "<sha256 of the reproducibility manifest toolchain section>",
		"thirdParty": "<sha256 of Zenith2/ThirdParty/manifest.json>",
		"shaderBlobs": "<sha256 over the cooked shader blob set>",
		"content": "<sha256 over the cooked content set of the workload>",
		"workloadDefinition": "<sha256 of the workload definition file>",
		"harness": "<harness version string>"
	},
	"tReadyNs": 0,
	"excludedIntervals": [ { "startNs": 0, "endNs": 10000000000, "reason": "initial-load" } ],
	"measuredFrames": 7200,
	"measuredSeconds": 120.0,
	"loops": 1,
	"dynresHistogram": [ 0 ],
	"framesAtFloor": 0,
	"framesInFlight": 2,
	"hitchCount": 0,
	"maxFrameMs": 0.0,
	"gfxSource": null,
	"gfxSamples": [ { "ns": 0, "bytes": 0, "source": "kgsl" } ],
	"gfxSampleCostNs": [ 0 ],
	"sustained": { "ambientC": 25.0, "batteryStartPct": 80, "batteryEndPct": 0, "thermalStatus": [ { "ns": 0, "status": 0 } ] },
	"latencySamples": [ { "injectNs": 0, "displayNs": 0 } ],
	"walkthroughSteps": [ { "step": 1, "seconds": 0.0 } ],
	"rows": [ { "id": "Z2-B-SHIP-001", "statistic": "p95", "measured": 15.9, "unit": "ms", "value": 16.7, "tolerance": 0, "result": "pass" } ],
	"artifact": "<path of the raw per-frame capture file>",
	"result": "pass"
}
```

- [Z2-N-BUDGET-102] The block's field defaults and sources: `framesInFlight` is the configured value (Z2-B-RHI-008); `hitchCount` and `maxFrameMs` are the MP-HITCH results of the measured interval, repeated as `rows[]` entries against the hitch rows; `gfxSource` is `"kgsl"`, `"dumpsys"` or `null` (Windows); `gfxSamples` and `gfxSampleCostNs` are `[]` on Windows; `sustained` is `null` outside an MP-SUSTAIN run, and inside one `thermalStatus` holds the 1 Hz `PowerManager.getCurrentThermalStatus` samples; `latencySamples` holds one entry per MP-LATENCY event, `[]` when not measured; `walkthroughSteps` holds MP-WALKTHROUGH per-step times, `[]` otherwise.
- [Z2-N-BUDGET-103] A `rows[]` entry's `statistic` is one of `p95`, `p99`, `max`, `min`, `count`, `total`, `rollingMin`, `totalWorkP95`, `median`, `pass`; the CPU total-work figure of MP-CPUZONE is a `rows[]` entry with `statistic: "totalWorkP95"` against the node's CPU row and `result: "report"`.
<!-- elaboration -->
- [Z2-N-BUDGET-104] The raw per-frame capture of a run is a CSV with the header `frameIndex,presentNs,frameMs,cpuSimMs,cpuRenderMs,gpuMs,scaleX,scaleY,excluded,hitch`, one line per presented frame, written next to its evidence record.

### Measurement procedure catalogue

<!-- elaboration -->
- [Z2-N-BUDGET-105] Every row's Procedure column names one of the procedures in the table below, and each procedure is run by the Player through the budget-run mode (`Zenith2/Docs/Architecture.md → "Composition root and Player"`) with the command-line switches `--budget-run <workload> --workload-version <N> --evidence-dir <path> --cache-state cold|warm --conditions controlled|ordinary --repetition <n>`, driven by the workload runner `Zenith2/Benchmarks/Run-Workload.ps1`.

| ID | Procedure | Statistic |
|---|---|---|
| MP-FRAME | Present-to-present frame times over the measured interval, as defined in "Frame-rate definitions" | p95 or p99 by nearest rank, per repetition |
| MP-SUSTAIN | The sustained thermal run procedure | minimum 1-s rolling average fps |
| MP-DYNRES | The per-frame main-view render scale | minimum scale per axis; frames at floor |
| MP-HITCH | Frames of the measured interval whose frame time is greater than 33.3 ms, and the maximum frame time | count; maximum |
| MP-BOOT | Wall-clock from process creation (Windows `CreateProcess` return in the runner; Android `am start -W` launch timestamp) to the first present after world-ready of the workload's start scene | seconds, per repetition |
| MP-CACHE | A cold-cache and a warm-cache run of the same build and workload (see "Cache state pairs") | the per-row statistics of each run |
| MP-LATENCY | Input-to-photon latency of injected input events (see "Input-to-photon latency") | p95 over events |
| MP-CPUZONE | Profiler zones of the CPU frame-graph node or engine stage the row names: per frame, the span from node start to node completion including joined sub-jobs | p95 span over the measured interval |
| MP-GPUPASS | The render graph's automatic timestamp query pair around the pass (or merged rendering scope) the row names | p95 over the measured interval, per (path, view kind) |
| MP-BANDWIDTH | GPU memory traffic of the named passes: Adreno performance counters on the S24 (through the Android GPU inspector counter set), vendor counters on the desktop | bytes per frame, p95 |
| MP-MEM | Process memory. S24: `VmRSS` from `/proc/self/status` sampled in-process at 10 Hz, plus the Graphics term (the Adreno kgsl/dmabuf allocations `VmRSS` does not contain), taken as the rule "S24 Graphics term" defines. Windows: `PROCESS_MEMORY_COUNTERS_EX::WorkingSetSize` and `PrivateUsage` at 10 Hz, reporting the larger | Windows: maximum over the whole run including boot. S24: the maximum, over the Graphics sample times, of that Graphics sample plus the largest `VmRSS` sample since the previous Graphics sample; a run with `gfxSource: "dumpsys"` and fewer than two Graphics samples (a short W-Load boot) uses the final cross-check sample plus the largest `VmRSS` of the whole run |
| MP-GPUMEM | Device-local GPU memory sampled every frame: Vulkan `VK_EXT_memory_budget` `heapUsage` summed over device-local heaps; D3D12 `IDXGIAdapter3::QueryVideoMemoryInfo` local segment `CurrentUsage` | maximum over the run |
| MP-SUBMEM | The memory governor's per-subsystem accounting, sampled every frame | maximum over the run |
| MP-TRANSIENT | The render graph's transient placement report per frame, with aliasing on and with aliasing off | maximum bytes over the run |
| MP-COUNTER | The named telemetry counter, sampled every frame | maximum or total over the run, as the row states |
| MP-SCRIPT | The script runtime's per-invocation safepoint counters and per-phase wall-clock zones | maximum safepoints per invocation; p95 phase span |
| MP-STREAM | Scripted teleports and rapid traversal (see "Teleport and rapid traversal") | seconds to resident; count of frames with residency misses |
| MP-EDIT | Editor operation timing: wall-clock from the triggering event to the editor's completion event for that operation | median of 5 repetitions, and every repetition within value plus tolerance |
| MP-WALKTHROUGH | A timed scripted task walkthrough run by a human in the editor | wall-clock per walkthrough |
| MP-GATE | `tick_gates.ps1` and `Run-Committed.ps1` step and phase wall-clock timings from `tick_gates.json` | wall-clock per phase or step |
| MP-PKG | Package and install size measurement (see "Package and storage budgets") | bytes |
| MP-PKGVERIFY | One 1 GiB asset pack hashed through `Zenith_AssetPackInstaller`'s verification pipeline on the device | median MB/s of 10 repetitions |
| MP-STORAGE | Repository and checkout storage measurement (see "Repository asset storage") | bytes |
| MP-ALLOC | `pwsh -NoProfile -File Zenith2/Benchmarks/Budgets/Check-Allocation.ps1 -Area <AREA>` (with `-Final` at the Stage 1 acceptance), computing every bound of "Allocation sums" from the rows' values | per bound: the sum against its limit, pass or fail, and `incomplete` |
| MP-DEPTHPREC | The analytic D24 precision check of a view: the worst-case linear-depth step at the far distance from the view's near and far planes under standard Z | metres |
| MP-DISKCACHE | The summed size of the persisted pipeline-cache and PSO-library files after a warm-cache run | bytes |
| MP-PARITY | The BackendParity and PathParity image comparisons of a parity scene: surface-data channels, then final images by FLIP and per-channel difference | FLIP mean and per-channel maximum per scene |
| MP-JOBOVERHEAD | The per-frame counter `jobs.overhead_ns` over the measured interval, plus the scheduler-only micro-workload `Budgets_JobsSchedulerOnly` | p95 of each |
| MP-DECOMP | The COMPRESS benchmark `Zenith_Bench_Compression` (`Zenith2/Benchmarks/Compression/`, nanobench) one-shot decompress throughput, under the procedure of "Decompression throughput floors" | median MB/s of at least 10 repetitions |
| MP-DEFECT | The open BUG tickets of a flagship slice's epic set, weighted by severity (see "Bug bar and severity weights") | weighted sum |
| MP-PATHTRACE | The path tracer's per-scene run on WARP or lavapipe | resolution, wall-clock, spp, variance, error |
| MP-APPROX | The approximation-error test the row names (see "Approximation error rows"): the feature's output against its reference (a high-step or CPU integrator, a fitted set, or a golden capture) on the owning area's error scenes, run on WARP and lavapipe or on the host | FLIP mean, per-channel maximum, metres, or the row's stated criteria, per scene |

### Procedure parameters

<!-- elaboration -->
| Parameter | Value | Used by | Basis |
|---|---|---|---|
| Research repetitions | ≥ 10 | research runs | plan text (Z2-D-064) |
| Research warm-up repetitions discarded | 1 | research runs | elaboration |
| Qualification repetitions | ≥ 3, every one passing | qualification runs | elaboration |
| MP-MEM in-process sampling rate | 10 Hz | MP-MEM | elaboration |
| MP-MEM S24 Graphics sampling | kgsl counters at 10 Hz where readable, otherwise `dumpsys meminfo` every 10 s; one `dumpsys` cross-check at run end | MP-MEM | orchestrator ruling |
| MP-MEM kgsl/`dumpsys` agreement | ≤ 5% | MP-MEM | elaboration |
| MP-MEM device test | 256 MB image; rise ≥ 230 MB; `VmRSS` rise < 32 MB | MP-MEM harness | elaboration |
| Percentile method | nearest rank, no interpolation, no outlier removal | MP-FRAME, MP-CPUZONE, MP-GPUPASS, MP-LATENCY | elaboration |
| Sustained-run pre-charge and idle target | charge to ≥ 85%, idle to 80% | MP-SUSTAIN | elaboration (80% start is plan text) |
| MP-EDIT repetitions | 5 | MP-EDIT | elaboration |
| MP-STREAM teleports per run | 5, destinations ≥ 1 km apart | MP-STREAM | elaboration |
| MP-LATENCY injection interval | 2 s | MP-LATENCY | elaboration |
| MP-LATENCY physical-confirmation tolerance | 5 ms | Stage 6 latency confirmation | elaboration |
| W-Indoor visible-triangle coverage | ≥ 90% of measured frames | W-Indoor validity | elaboration |
| Dynamic-resolution histogram bucket | 1% | MP-DYNRES | elaboration |
| Sustained-run pre-soak | 10 min within 1 °C of ambient | MP-SUSTAIN | elaboration |
| Screen brightness during runs | 50%, adaptive off | every S24 run | elaboration |
| Regression severity boundary | 10% over the row's value | BUG severity under "Breach consequences" | elaboration |
| Gate headroom trigger | > 80% of a warm cap on 3 consecutive tickets | the MAINT headroom ticket | elaboration |

- [Z2-N-BUDGET-106] A row is measured only with the procedure its Procedure column names; a measurement taken any other way is not evidence for that row.
- [Z2-N-BUDGET-107] The on-screen profiler and the Tracy client are disabled during every budget run except for rows whose procedure reads their data, and the profiler's own overhead rows are measured by comparing an enabled and a disabled run.
<!-- elaboration -->
- [Z2-N-BUDGET-108] Every budget run uses a Release build (`win-release` or `android-arm64` Release); Debug and sanitizer builds are never used for budget evidence.

### Frame-rate rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-SHIP-001 | Frame time p95 | adopted | 16.7 | ms | MP-FRAME | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-SHIP-002 | Frame time p99 | adopted | 20.0 | ms | MP-FRAME | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-SHIP-003 | Initial-load exclusion window | adopted | 10, once per run | s | MP-FRAME | Win, S24 | 0 (exact) | WS-FRAME | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-004 | Minimum measured run duration | adopted | ≥ 120 | s | MP-FRAME | Win, S24 | 0 (hard) | WS-FRAME | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-005 | Minimum measured run frame count | adopted | ≥ 7,200 | frames | MP-FRAME | Win, S24 | 0 (hard) | WS-FRAME | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-006 | Sustained floor | adopted | ≥ 45, 1-s rolling average | fps | MP-SUSTAIN | S24 | 0 (hard) | WS-SUSTAIN | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-SHIP-007 | Sustained run duration | adopted | ≥ 30 | min | MP-SUSTAIN | S24 | 0 (hard) | WS-SUSTAIN | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-008 | Sustained run ambient temperature | adopted | 25 | °C | MP-SUSTAIN | S24 | ±1 | WS-SUSTAIN | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-009 | Sustained run starting battery | adopted | 80 | % | MP-SUSTAIN | S24 | ±2 | WS-SUSTAIN | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-010 | Sustained run power state | adopted | unplugged | — | MP-SUSTAIN | S24 | 0 (exact) | WS-SUSTAIN | BR-INVALID | Z2-D-012 (plan text) |
| Z2-B-SHIP-011 | Dynamic-resolution floor | adopted | ≥ 75 (of the Win 2560×1440 and S24 2340×1080 outputs) | % of output | MP-DYNRES | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-SHIP-012 | Hitch threshold | adopted | 33.3 | ms | MP-HITCH | Win, S24 | 0 (exact) | WS-FRAME | BR-REPORT | Z2-D-012 (plan text) |

<!-- informative -->
The tolerances of the sustained-run ambient temperature and starting battery rows are elaborations of the plan's conditions; they bound how precisely a human operator has to reproduce them before a run is valid.
<!-- /informative -->

## Reference hardware

### Reference desktop

| Item | Value |
|---|---|
| CPU | Ryzen 7 5800X-class (8 cores, 16 threads) |
| RAM | 32 GB |
| GPU | NVIDIA RTX 3070 (8 GB) |
| Output | 2560×1440 (1440p) |
| Backends | Vulkan and D3D12, selected at runtime |
| OS | Windows 11, build pinned per measurement in the evidence record |

- [Z2-N-BUDGET-121] Every `Win` row is measured on the reference desktop: a Ryzen 7 5800X-class CPU, 32 GB of RAM and an RTX 3070, at 1440p output.
- [Z2-N-BUDGET-122] Graphics settings are fixed per workload version: the quality tier, dynamic-resolution enablement, upscaler, shading path and every per-feature toggle of a budget run come from the workload definition, never from the user's settings file or command line.
<!-- elaboration -->
- [Z2-N-BUDGET-123] The reference desktop's GPU driver and OS build are recorded in every evidence record; a driver change between the evidence records a stage acceptance compares is permitted only when the acceptance report states it, because the fingerprint does not cover the driver.

### Reference Android device

| Item | Value |
|---|---|
| Device | Samsung Galaxy S24 Ultra (Snapdragon 8 Gen 3, Adreno 750) |
| Output | 2340×1080 |
| OS | Android 14 or newer (minSdk 34), page size 16 KB compliant builds |
| Backend | Vulkan (the baseline profile including `shaderFloat16`) |

- [Z2-N-BUDGET-124] Every `S24` row is measured on the Galaxy S24 Ultra, the only Android device Zenith2 is validated on; measurements from any other Android device are best-effort data and are never evidence for a row.

### Headless CI hosts

<!-- informative -->
WARP and lavapipe run every workload and flagship game headless for correctness in the CI integration smoke and for the path tracer's physical-accuracy runs. Neither is a budget platform: their timings are not frame-rate evidence. The only budget rows measured on them are the path tracer's per-scene caps and the gate rows.
<!-- /informative -->

- [Z2-N-BUDGET-125] Timings measured on WARP, lavapipe, the `linux-tsan` host or any sanitizer build are never evidence for a frame-rate, CPU, GPU or memory row; the path tracer cap rows and the gate rows, whose platform is `Host`, are the only rows measured there.

## Memory budgets

### Unit conventions

<!-- elaboration -->
- [Z2-N-BUDGET-141] In this document `GB` means 10^9 bytes and `MB` means 10^6 bytes; every memory value is compared against a measurement in bytes, so the S24 total of 3.0 GB is 3,000,000,000 bytes.

### Totals

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-CORE-001 | S24 process RSS | adopted | 3.0 | GB | MP-MEM | S24 | 0 (hard) | WS-FRAME, W-Load | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-CORE-002 | Windows process RAM | adopted | 8.0 | GB | MP-MEM | Win | 0 (hard) | WS-FRAME, W-Load | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-CORE-003 | Windows VRAM | adopted | 6.0 | GB | MP-GPUMEM | Win | 0 (hard) | WS-FRAME, W-Load | BR-ACCEPT | Z2-D-012 (plan text) |

- [Z2-N-BUDGET-142] The S24 process RSS stays at or below 3.0 GB, and on Windows the process stays at or below 8 GB of RAM and 6 GB of VRAM, for the whole of every workload run including boot.
- [Z2-N-BUDGET-143] The memory totals apply to the runtime Player; the editor (`win-tools`) is budgeted by the editor rows and the "Editor caches" row, and is not measured against the Windows totals.

### Memory governor inputs

- [Z2-N-BUDGET-144] The memory governor reads its per-subsystem budgets (rendering, streaming, audio, scripts, editor caches and every other per-subsystem memory row) and its pressure thresholds from the rows of this section, through a generated table, never from constants written in engine code (`Zenith2/Docs/Architecture.md → "Memory governor"`).
<!-- elaboration -->
- [Z2-N-BUDGET-145] The generated table is `Zenith2/Engine/ZenithCore/Internal/Generated/Zenith_MemoryBudgets.generated.h`, produced at configure time from the `Row index` of this document by the budget-table generator specified in `Zenith2/Docs/Design/CORE.md`; it holds, per platform, one `uint64` byte value per memory row, keyed by row ID, and a row still `pending` generates as the platform total with a `pending` flag set, so a pending row never sheds and is reported.
- [Z2-N-BUDGET-146] Every transition peak (streaming double residency, world transition, texture swap) is budgeted explicitly by its own row, and the governor's pressure evaluation includes the transition in progress.
- [Z2-N-BUDGET-147] Tests force each pressure level (normal, warning, critical) and assert each subsystem's defined shedding; the thresholds they use are the pressure-threshold rows.

### Per-subsystem memory rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-CORE-004 | General heap unattributed residual | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-005 | Frame and graph arenas | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-012 | String intern table capacity | pending | pending (entries; MB of text) | entries, MB | MP-SUBMEM | Win, S24 | 0 (hard) | WS-FRAME | BR-REFUSE | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-013 | Log router ring bytes | pending | pending (at least 1 MiB, 1,048,576 bytes) | MB | MP-SUBMEM | Win, S24 | 0 (hard) | WS-FRAME | BR-DROP | Stage 1 per-system budget ticket for CORE |
| Z2-B-ECS-001 | Component storage per world | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for ECS |
| Z2-B-SCENE-001 | Scene and prefab runtime data | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for SCENE |
| Z2-B-STREAM-001 | Streaming CPU-side resident and in-flight cell data | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for STREAM |
| Z2-B-ENV-006 | Environment resident CPU data | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for ENV |
| Z2-B-ASSET-001 | Asset system resident data and decode buffers | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME, W-Load | BR-GOVERNOR | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-005 | Asset in-flight load bytes | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-STREAM, W-Load | BR-DEFER | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-006 | Asset I/O staging bytes | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-STREAM, W-Load | BR-STALL | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-007 | Asset keep-alive cache bytes | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-DROP | Stage 1 per-system budget ticket for ASSET |
| Z2-B-GEOM-001 | Unified vertex and index pools | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-3D | BR-GOVERNOR | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-002 | Texture residency pool | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-008 | Material constants memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for GEOM |
| Z2-B-RHI-001 | Persistent render targets and view histories | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-002 | Bindless descriptor heaps | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-003 | GPU residency budget per heap | pending | pending | MB | MP-GPUMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for RHI |
| Z2-B-RPROD-001 | GPU scene tables | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-3D | BR-GOVERNOR | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RHI-012 | GPU-scene CPU pending store | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-3D | BR-GOVERNOR | Stage 1 per-system budget ticket for RHI |
| Z2-B-LIGHT-018 | Reflection probe and environment memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-3D | BR-GOVERNOR | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-019 | Baked GI resident memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-OpenWorld, W-Indoor | BR-GOVERNOR | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-RPROD-002 | PSO cache and pipeline objects | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME, W-Load | BR-GOVERNOR | Stage 1 per-system budget ticket for RPROD |
| Z2-B-ADVGFX-001 | Virtual texturing physical cache and page tables | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-VFX-001 | Particle pools and CPU particle data | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for VFX |
| Z2-B-ANIM-001 | Animation clips, skeletons and pose buffers | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Characters, W-Cinematic, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for ANIM |
| Z2-B-ANIM-006 | Animation stream cache bytes | pending | pending (one value per platform, the value of `animation.streaming.cacheBytes`) | MB | MP-SUBMEM | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-GOVERNOR | ANIM Stage 4 budget ruling ticket |
| Z2-B-PHYS-001 | Physics world memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Characters, W-Dense, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for PHYS |
| Z2-B-TWOD-005 | 2D physics world memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-UI2D, W-Game2D | BR-GOVERNOR | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-007 | TWOD module resident memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-UI2D, W-Game2D | BR-GOVERNOR | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-008 | 2D renderer memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-UI2D, W-Game2D | BR-GOVERNOR | Stage 1 per-system budget ticket for TWOD |
| Z2-B-AI-001 | Navigation mesh and pathfinding memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Characters, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for AI |
| Z2-B-AUDIO-001 | Audio banks, streams and mixer | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-SLICE | BR-GOVERNOR | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-UI-001 | UI, fonts and glyph atlases | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-UI2D, WS-SLICE | BR-GOVERNOR | Stage 1 per-system budget ticket for UI |
| Z2-B-CINE-001 | Cinematic timelines and video decode buffers | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Cinematic | BR-GOVERNOR | Stage 1 per-system budget ticket for CINE |
| Z2-B-SAVE-001 | Save serialization buffers | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for SAVE |
| Z2-B-JOBS-004 | Job system resident memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for JOBS |
| Z2-B-DEVT-001 | Profiler and telemetry buffers | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for DEVT |
| Z2-B-PLAT-006 | Platform and input memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for PLAT |
| Z2-B-EDIT-001 | Editor caches | pending | pending | MB | MP-SUBMEM | Win | default | W-Editor | BR-GOVERNOR | Stage 1 per-system budget ticket for EDIT |

- [Z2-N-BUDGET-472] The Stage 1 per-system budget ticket for CORE allocates Z2-B-CORE-005 "Frame and graph arenas" with, as an input, the render-world slot-arena demand each render-world writer states ([Z2-N-REND-017]), VFX's per-world maxima being `Zenith2/Docs/Design/VFX/Simulation.md` → "CPU particle output"; the later VFX ticket confirms that demand and does not rewrite the CORE row.
- [Z2-N-BUDGET-148] The row "String intern table capacity" bounds the process-wide Core string intern table (the configuration keys `strings.maxStrings` and `strings.maxTextBytes`, supplied by ZenithCore's platform configuration fragments); an intern request beyond either cap returns an error (BR-REFUSE); it is distinct from Z2-B-SCRIPT-022, the per-world-session capacity class the script merge point checks.
- [Z2-N-BUDGET-149] The row "Log router ring bytes" bounds the runtime log ring size `log.ringBytes` that the composition root applies at startup; it is allocated at least as large as the 1 MiB bootstrap ring `uLOG_RING_BYTES_DEFAULT`, which is a build constant used from startup until the resize and is not this row; when the ring is full the oldest records are dropped and counted in `log.router.dropped` (BR-DROP).
- [Z2-N-BUDGET-150] Every allocation the general heap, an arena or a pool makes is attributed to exactly one per-subsystem memory row (or to a scripting memory row) by the memory governor's per-subsystem accounting, and the unattributed residual row bounds whatever the accounting cannot attribute.
<!-- elaboration -->
- [Z2-N-BUDGET-151] S24 Graphics term: at boot the Player tests whether `/sys/class/kgsl/kgsl/proc/<pid>/gpumem_mapped` and `/sys/class/kgsl/kgsl/proc/<pid>/gpumem_unmapped` are readable; if they are, it reads both in-process at 10 Hz (each file holds one decimal byte count) and the Graphics term is their sum (`gfxSource: "kgsl"`); if they are not, the runner samples `adb shell dumpsys meminfo <pid>` every 10 s from the return of `am start -W` (process start) and the Graphics term is that sample (`gfxSource: "dumpsys"`).
- [Z2-N-BUDGET-152] The `dumpsys meminfo <pid>` parse rule is: find the line matching `^\s*App Summary\s*$`, then the first following line matching `^\s*Graphics:\s+(\d+)\b`; the captured number is kB and is multiplied by 1,024 to give bytes; output without such a line is a failed sample and makes the run invalid (BR-INVALID).
- [Z2-N-BUDGET-153] Whichever source is primary, the runner takes one `dumpsys meminfo <pid>` sample at the end of the run as a cross-check, which is also the run's final Graphics sample; with `gfxSource: "kgsl"`, a disagreement above 5% between the last kgsl sum and the cross-check makes the run invalid.
- [Z2-N-BUDGET-154] Every Graphics sample is recorded in the evidence record as `gfxSamples[]: {ns, bytes, source}`, and every `dumpsys` call's duration as `gfxSampleCostNs`; the kgsl source is preferred because a `dumpsys` call binds into the measured process and can perturb the frame-time and hitch rows of the same run.
- [Z2-N-BUDGET-155] The row "S24 process RSS" is judged against the MP-MEM S24 statistic, `VmRSS` plus the Graphics term, so its 3.0 GB covers CPU and GPU memory together and every GPU-resident row counts toward it; host-visible GPU memory the CPU has touched can appear in both terms, and the sum is taken as measured, which overcounts conservatively.
<!-- elaboration -->
- [Z2-N-BUDGET-156] The MP-MEM harness carries a device test: on the S24 it allocates a 256 MB device-local, never host-mapped Vulkan image, forces its physical commit by recording one `vkCmdClearColorImage` over the whole image and waiting for that submission's fence, and then asserts that the MP-MEM S24 statistic has risen by at least 230 MB while `VmRSS` alone has risen by less than 32 MB, and, where both sources are available, that the kgsl sum and a `dumpsys` sample agree within 5%; it is built by the Stage 1 per-system budget harness for PLAT, which builds the MP-MEM S24 sampler under `Zenith2/Benchmarks/Budgets/PLAT/`, and run by the human as a device step of the PLAT ruling ticket; the Player's budget-run mode adopts that sampler in Stage 2.
- [Z2-N-BUDGET-157] On the S24 every GPU-resident row counts toward the S24 process RSS row; on Windows, GPU-resident rows count toward the VRAM total and CPU-resident rows toward the RAM total, and a row whose data lives in both is allocated one value per side.
- [Z2-N-BUDGET-159] The row "GPU-scene CPU pending store" bounds the general-heap mirror `Flux_GPUScene` keeps of staged records and pending frees: its maximum is the sum over the four GPU-scene tables of capacity times record size (the whole table set staged on a full-state world) plus the staging hash map; it is a CPU-resident row of `MEMORY_SUBSYSTEM_RENDERING`, a member of `MEM-WIN-RAM` and `MEM-S24`, and is distinct from Z2-B-RPROD-001, the VRAM row of the tables themselves.
- [Z2-N-BUDGET-160] The rows "Asset in-flight load bytes", "Asset I/O staging bytes" and "Asset keep-alive cache bytes" bound the configuration keys `asset.load.maxInFlightBytes`, `asset.io.stagingBytes` and `asset.cache.maxBytes`; each is a share of Z2-B-ASSET-001, and the three values plus the registry's resident data and decode buffers stay within Z2-B-ASSET-001's value.
- [Z2-N-BUDGET-161] A load whose admission would exceed "Asset in-flight load bytes" waits in admission and is admitted when bytes return (BR-DEFER); a read that would exceed "Asset I/O staging bytes" is not issued until staging frees (BR-STALL); an insertion that would exceed "Asset keep-alive cache bytes" evicts least-recently-used cached assets until it fits, counted in the cache's eviction counter (BR-DROP).
- [Z2-N-BUDGET-162] The row "Platform and input memory" bounds the platform layer's event ring, lifecycle transition log, input binding table, mount tables and I/O queues, attributed to `MEMORY_SUBSYSTEM_OTHER`, and is a member of `MEM-WIN-RAM` and `MEM-S24`.
- [Z2-N-BUDGET-163] The row "Material constants memory" bounds the GPU material constants pool reported by `Flux.Materials.ConstantsBytes` (`MEMORY_SUBSYSTEM_RENDERING`); the pool grows by resize-and-copy only up to this row, and it is a member of `MEM-WIN-VRAM` and `MEM-S24`.
- [Z2-N-BUDGET-164] The rows "Reflection probe and environment memory" (captured and filtered probe and environment textures) and "Baked GI resident memory" (resident irradiance volumes and lightmap pages) are GPU-resident LIGHT rows, members of `MEM-WIN-VRAM` and `MEM-S24`.
- [Z2-N-BUDGET-165] The row "Environment resident CPU data" bounds the environment module's CPU-side terrain, foliage and atmosphere data, attributed to the `MEMORY_SUBSYSTEM_STREAMING` share; it is a share of Z2-B-STREAM-001 and is excluded from every bound, because Z2-B-STREAM-001 already counts it.

### Transition peaks and pressure thresholds

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-CORE-006 | Streaming double-residency peak | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-007 | World-transition peak | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-OpenWorld, W-Cinematic, W-Game2D | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-008 | Texture-swap peak | pending | pending | MB | MP-SUBMEM | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-009 | Memory pressure warning threshold | pending | pending | % of platform total | MP-MEM | Win, S24 | 0 (hard) | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-010 | Memory pressure critical threshold | pending | pending | % of platform total | MP-MEM | Win, S24 | 0 (hard) | WS-FRAME | BR-GOVERNOR | Stage 1 per-system budget ticket for CORE |

- [Z2-N-BUDGET-158] The governor's pressure level is the higher of the OS-signalled level (Android `onTrimMemory` levels mapped per `Zenith2/Docs/Architecture.md → "Memory governor"`) and the budget-derived level from the two threshold rows, evaluated every frame.

## Package and storage budgets

### Android package

- [Z2-N-BUDGET-171] The Android App Bundle's base module stays within Google Play's base-module download limit, and every other piece of content ships through Play Asset Delivery asset packs (row "AAB base module download size").
- [Z2-N-BUDGET-172] The exact base-module limit is pinned at Stage 6 from Google Play's current documentation by the Stage 6 PACK ticket that pins it, which records the documentation URL and the retrieval date in the row's allocation table.
<!-- elaboration -->
- [Z2-N-BUDGET-173] MP-PKG measures the base module's download size with `bundletool get-size total --apks=<apks> --modules=base --dimensions=ALL` (without `--modules`, bundletool sizes every module installed at install time, install-time asset packs included; the PACK ticket confirms the flag against the pinned bundletool's `help get-size`) over the split APKs generated for the S24 device spec, and reports the maximum; the check runs in the Stage 6 shipping acceptance for every flagship slice.

### Windows install size

- [Z2-N-BUDGET-174] Windows install size is a budget row per flagship slice (rows "Windows install size OpenWorldSlice", "Windows install size CinematicSlice" and "Windows install size Game2D").
<!-- elaboration -->
- [Z2-N-BUDGET-175] MP-PKG measures Windows install size as the sum of file sizes of the directory produced by the packaging step for the slice's `win-release` build, excluding symbol files (`*.pdb`), which symbol retention stores separately.

### Repository asset storage

- [Z2-N-BUDGET-176] Source-asset storage is budgeted: the Git LFS policy and per-game quotas are rows of this section.
- [Z2-N-BUDGET-177] Large binary content is LFS-tracked, because lane worktrees duplicate the working tree; a binary file at or above the LFS tracking threshold row committed outside LFS is a failure of the content validation that the CONTENT area delivers.
- [Z2-N-BUDGET-178] The storage cost of a Z2 lane checkout is measured and recorded in the row "Lane checkout storage per Z2 lane".
<!-- elaboration -->
- [Z2-N-BUDGET-179] MP-STORAGE measures a game's source-asset storage as the sum of the LFS object sizes (`git lfs ls-files --size --long`) plus the tracked non-LFS blob sizes under `Zenith2/Games/<Name>/`, and a lane checkout's storage as the on-disk size of the lane worktree directory after `lane_init`, including ignored build outputs of one warm `win-debug` build.

### Package and storage rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-PACK-001 | AAB base module download size | pending | pending (Google Play's base-module download limit, pinned at Stage 6) | MB | MP-PKG | S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-002 | Windows install size OpenWorldSlice | pending | pending | GB | MP-PKG | Win | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-003 | Windows install size CinematicSlice | pending | pending | GB | MP-PKG | Win | default | W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-004 | Windows install size Game2D | pending | pending | GB | MP-PKG | Win | default | W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-005 | Play asset pack download size | pending | pending | MB | MP-PKG | S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-006 | Play asset packs total download size | pending | pending | MB | MP-PKG | S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-007 | Play asset pack count | pending | pending | count | MP-PKG | S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-008 | Patch download size per release | pending | pending | MB | MP-PKG | Win | default | WS-SLICE | BR-REPORT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-009 | Package service resident memory | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-010 | Asset pack verification throughput floor | pending | pending | MB/s | MP-PKGVERIFY | Win, S24 | default | — | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-011 | Cold-boot update verification time | pending | pending (one value at the first start after the install, one at the second) | ms per GiB | MP-BOOT | Win, S24 | default | W-Load | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-CONTENT-001 | LFS tracking threshold | pending | pending | bytes | MP-STORAGE | Repo | 0 (hard) | — | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-002 | Source-asset quota OpenWorldSlice | pending | pending | GB | MP-STORAGE | Repo | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-003 | Source-asset quota CinematicSlice | pending | pending | GB | MP-STORAGE | Repo | default | W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-004 | Source-asset quota Game2D | pending | pending | GB | MP-STORAGE | Repo | default | W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-005 | Source-asset quota WorkloadIndoor | pending | pending | GB | MP-STORAGE | Repo | default | W-Indoor | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-006 | Source-asset quota WorkloadOutdoor | pending | pending | GB | MP-STORAGE | Repo | default | W-Outdoor | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-007 | Source-asset quota WorkloadCharacters | pending | pending | GB | MP-STORAGE | Repo | default | W-Characters | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-008 | Source-asset quota WorkloadDense | pending | pending | GB | MP-STORAGE | Repo | default | W-Dense | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-009 | Source-asset quota WorkloadUI2D | pending | pending | GB | MP-STORAGE | Repo | default | W-UI2D | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-PROC-001 | Lane checkout storage per Z2 lane | pending | pending | GB | MP-STORAGE | Host | default | — | BR-REPORT | Stage 0 PROC lane ticket (zagent lane add Z2) |
| Z2-B-ASSET-010 | Local DDC size | pending | pending | GB | MP-STORAGE | Host | default | — | BR-DROP | Stage 1 per-system budget ticket for ASSET |
| Z2-B-SAVE-004 | Save file size per slot | pending | pending | MB | MP-COUNTER | Win, S24 | default | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for SAVE |

- [Z2-N-BUDGET-180] The row "Local DDC size" is the configuration key `cook.ddc.localMaxBytes` of a host's local derived-data cache; it is storage on the cook host, never summed into a memory total, and a store that would exceed it first evicts least-recently-used entries to 90% of the row (BR-DROP), as `Zenith2/Docs/Design/ASSET.md` specifies.
- [Z2-N-BUDGET-484] The PACK rows of `Zenith2/Docs/Design/PACK.md` → "Pending new rows": the three Play rows (asset pack download size, total download size and pack count) are download sizes that exist only after Gradle and bundletool, so they are BR-ACCEPT, and `<Game>_Bundle` additionally refuses an adopted value as a build failure (`Invoke-Zenith2Package.ps1 -Bundle`, exit code 8, naming the row); "Package service resident memory" is the MP-SUBMEM size of the installer's records and verification arena, the delivery cells, the event queue, the world records, the delivery-state stamp table and the verified stamp file copy, fixed at `Create` (no governor consumer, so BR-ACCEPT), and a member of `MEM-WIN-RAM` and so of `MEM-S24`; "Asset pack verification throughput floor" is MP-PKGVERIFY; "Cold-boot update verification time" is W-Load's MP-BOOT run on an install carrying a committed 1 GiB content patch and an installed 1 GiB Windows DLC (Win), or a delivered 1 GiB fast-follow pack (S24), minus the same run without them, divided by the GiB present, median of 10 repetitions, measured at the first start after the install (full hash) and at the second (stamp checks only, `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache"), each reported separately; W-Load's cold-boot rows Z2-B-SHIP-013 and Z2-B-SHIP-014 are measured with no update installed, and this row bounds the increment an installed update adds and is never removed.

## Workload definitions

### Workload rules

- [Z2-N-BUDGET-191] Every workload is versioned (`v1` is the version in force for all ten), and its definition fixes its content, content scale, scripted path, duration, graphics settings and metrics; any change to one of them is a version bump governed by the change rule of this document.
<!-- elaboration -->
- [Z2-N-BUDGET-192] Each workload's machine-readable definition is `Zenith2/Games/<Game>/Workload/<WorkloadName>.workload.json`, with the fields `name`, `version`, `game`, `startScene`, `scriptedPath` (the path of the input-replay or camera-path file), `loopUntilMinimums` (bool), `graphics` (quality tier, shading path, dynamic resolution on or off, upscaler), `rows` (the row IDs measured), and `metrics` (the procedure IDs run); the workload-definition hash in the evidence fingerprint is the SHA-256 of this file.
- [Z2-N-BUDGET-193] A workload's scripted path is deterministic: it is an input-replay or camera-path file played back on fixed simulation ticks, so two runs of the same build and workload version execute the same simulation.
- [Z2-N-BUDGET-194] The content scale of W-Dense and W-UI2D, and the scripted paths and scene content of every workload, are set by their Stage 1 workload scale tickets, which are planned producers that block their consumers.
- [Z2-N-BUDGET-195] A "Stage 1 workload scale ticket for <W>" is a `needs-human` ruling TASK, parented to the epic of the area that owns the workload's primary rows (W-Indoor and W-Outdoor: RPROD; W-Characters: ANIM; W-Dense: ECS; W-UI2D: UI), which commits `Zenith2/Docs/Research/Workloads/<W>.md` (the numeric content scale, the scripted-path specification, the start-scene description and the loop rule) and the `name`, `version`, `game`, `graphics`, `rows` and `metrics` fields of the workload's `*.workload.json`, leaving `startScene` and `scriptedPath` empty.
- [Z2-N-BUDGET-196] The workload's scene and replay are authored in Stage 3 by the focused workload-game ticket, which the scale ticket `blocks`; that ticket fills `startScene` and `scriptedPath` to the committed specification and changes no number of it without a workload version bump.
- [Z2-N-BUDGET-197] The focused workloads live in the focused workload games `Zenith2/Games/WorkloadIndoor`, `WorkloadOutdoor`, `WorkloadCharacters`, `WorkloadDense` and `WorkloadUI2D`, and the flagship workloads in `Zenith2/Games/OpenWorldSlice`, `CinematicSlice` and `Game2D`; like every game, they contain no C++.

### Workload sets

| Set | Members | Used by |
|---|---|---|
| WS-FRAME | W-Indoor, W-Outdoor, W-Characters, W-Dense, W-UI2D, W-OpenWorld, W-Cinematic, W-Game2D | frame-rate, memory-total, hitch, latency and whole-frame rows |
| WS-3D | W-Indoor, W-Outdoor, W-Characters, W-Dense, W-OpenWorld, W-Cinematic | 3D rendering and geometry rows |
| WS-SLICE | W-OpenWorld, W-Cinematic, W-Game2D | slice-level rows (audio, package, bug bar) |
| WS-SUSTAIN | W-OpenWorld, W-Cinematic, W-Game2D, W-Outdoor, W-Characters | the sustained thermal run |
| WS-STREAM | W-Outdoor, W-OpenWorld | teleport and rapid-traversal streaming rows |

### W-Indoor

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/WorkloadIndoor` |
| Content scale | 200 lights, of which 32 are shadowed; 1M visible triangles |
| Duration | the minimum run length (120 s and 7,200 frames of measured interval), looped path |
| Scripted path | a camera path through interconnected rooms, set by the Stage 1 workload scale ticket for W-Indoor |
| Metrics | MP-FRAME, MP-HITCH, MP-DYNRES, MP-MEM, MP-GPUMEM, MP-SUBMEM, MP-CPUZONE, MP-GPUPASS, MP-TRANSIENT, MP-BANDWIDTH (S24), MP-LATENCY, MP-CACHE |
| Primary rows | frame-time rows; light clustering, shadow views, forward opaque and every per-view pass row; transient memory peak; HiZ mid-frame bandwidth |

<!-- elaboration -->
- [Z2-N-BUDGET-198] W-Indoor v1 keeps at least 1M triangles visible (after culling, as counted by the drawn-instance counters) on at least 90% of measured frames, and at least 200 lights and 32 shadowed lights inside the view frustum's light list on every measured frame; the Stage 1 W-Indoor ticket authors the path to meet this, and a run whose counters miss it is invalid.

### W-Outdoor

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/WorkloadOutdoor` |
| Content scale | 4 km² of terrain (2 km × 2 km); 100k foliage instances; 2 km view distance |
| Duration | the minimum run length, one traversal leg per 120 s |
| Scripted path | a traversal at 10 m/s across the terrain, plus the scripted teleports of MP-STREAM, set by the Stage 1 workload scale ticket for W-Outdoor |
| Metrics | as W-Indoor, plus MP-STREAM and MP-SUSTAIN |
| Primary rows | frame-time rows; streaming, virtual texturing and texture-residency rows; terrain and foliage GPU rows; teleport and rapid-traversal rows |

- [Z2-N-BUDGET-199] W-Outdoor v1 traverses its terrain at 10 m/s, so 120 s of measured interval covers 1.2 km; the path never re-enters a cell it left during the same loop.

### W-Characters

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/WorkloadCharacters` |
| Content scale | 100 skinned characters with 60 bones each; 100 AI agents |
| Duration | the minimum run length |
| Scripted path | a fixed camera orbit over the crowd with scripted AI goals, set by the Stage 1 workload scale ticket for W-Characters |
| Metrics | as W-Indoor, plus MP-SUSTAIN |
| Primary rows | animation, movement arbitration, physics, AI and navigation CPU rows; compute skinning GPU row; skinned-vertex pool memory |

- [Z2-N-BUDGET-200] Every one of the 100 skinned characters of W-Characters v1 is animated and skinned every frame at full update rate in the measured runs of the per-system rows, so the rows measure the unreduced cost; significance-based update-rate reduction (an ANIM implementation ticket) is measured separately by the animation budgeting rows it produces.

### W-Load

| Field | Value |
|---|---|
| Version | v1 |
| Game | each flagship slice; before Stage 4, W-Outdoor as the proxy |
| Content scale | the slice's full start scene |
| Duration | one boot per repetition |
| Scripted path | none; the run ends at the first present after world-ready |
| Metrics | MP-BOOT, MP-MEM, MP-GPUMEM, MP-CACHE |
| Primary rows | cold boot rows; memory totals during boot |

- [Z2-N-BUDGET-201] Cold boot takes at most 10 s on Windows and at most 15 s on the S24 (rows "Cold boot Windows" and "Cold boot S24").
<!-- elaboration -->
- [Z2-N-BUDGET-202] A cold boot is the first launch of the Player after a device reboot, with the OS file cache cold and the engine's PSO and driver caches in the state the installed build leaves them after one prior launch; the first launch after install is the cold-cache run of MP-CACHE and is reported against the cold-cache rows, not the cold-boot rows.

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-SHIP-013 | Cold boot Windows | adopted | 10 | s | MP-BOOT | Win | 0 (hard) | W-Load | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-SHIP-014 | Cold boot S24 | adopted | 15 | s | MP-BOOT | S24 | 0 (hard) | W-Load | BR-ACCEPT | Z2-D-012 (plan text) |

### W-Editor

| Field | Value |
|---|---|
| Version | v1 |
| Game | OpenWorldSlice opened in `ZenithEditor` (`win-tools`); before Stage 4, the Stage 2 minimal game |
| Content scale | the slice's authoring content |
| Duration | five repetitions of each editor operation |
| Scripted path | an editor automation script that triggers each operation of "Editor iteration rows" in turn |
| Metrics | MP-EDIT, MP-WALKTHROUGH, MP-SUBMEM |
| Primary rows | the editor iteration and walkthrough rows |

- [Z2-N-BUDGET-203] W-Editor's iteration times are set by the Stage 1 W-Editor row tickets, and W-Editor rows are measured on Windows only.

### W-Dense

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/WorkloadDense` |
| Content scale | a dense simulation; the entity count, component mix, chunk-system count and physics body count are set by the Stage 1 workload scale ticket for W-Dense |
| Duration | the minimum run length |
| Scripted path | set by the Stage 1 workload scale ticket for W-Dense |
| Metrics | MP-FRAME, MP-HITCH, MP-CPUZONE, MP-SCRIPT, MP-SUBMEM |
| Primary rows | ECS, jobs, scripting and physics rows; it is also the ECS research workload of `Zenith2/Docs/Rendering.md → "Shading path research methodology"` |

- [Z2-N-BUDGET-204] W-Dense is run once per ECS backend (custom, flecs, EnTT) for qualification, and every backend a game can select meets the W-Dense rows.

### W-UI2D

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/WorkloadUI2D` |
| Content scale | a UI-heavy 2D project; element counts, text volume, sprite counts and animation counts set by the Stage 1 workload scale ticket for W-UI2D |
| Duration | the minimum run length |
| Scripted path | set by the Stage 1 workload scale ticket for W-UI2D |
| Metrics | MP-FRAME, MP-HITCH, MP-CPUZONE, MP-GPUPASS, MP-SUBMEM, MP-LATENCY |
| Primary rows | UI, 2D and text rows; UI composite GPU row |

### W-OpenWorld

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/OpenWorldSlice` |
| Content scale | the slice's content per `Zenith2/Docs/ContentSpecs.md`, set by the Stage 1 flagship acceptance ticket for OpenWorldSlice |
| Duration | the slice's 20 to 30 minute playthrough script, split into measured segments of at least the minimum run length |
| Scripted path | the OpenWorldSlice playthrough script |
| Metrics | every procedure |
| Primary rows | every row of WS-FRAME, WS-SLICE, WS-SUSTAIN and WS-STREAM |

### W-Cinematic

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/CinematicSlice` |
| Content scale | set by the Stage 1 flagship acceptance ticket for CinematicSlice |
| Duration | the slice's 20 to 30 minute playthrough script |
| Scripted path | the CinematicSlice playthrough script, including every cinematic and set-piece |
| Metrics | every procedure except MP-STREAM |
| Primary rows | every row of WS-FRAME, WS-SLICE and WS-SUSTAIN; cinematic, animation, lighting and audio rows |

### W-Game2D

| Field | Value |
|---|---|
| Version | v1 |
| Game | `Zenith2/Games/Game2D` |
| Content scale | set by the Stage 1 flagship acceptance ticket for Game2D in `Zenith2/Docs/Research/Workloads/W-Game2D.md` (the file shape of [Z2-N-BUDGET-195]), including the 2D physics scale D-SEL-2DPHYS measures; `Zenith2/Docs/ContentSpecs.md` keeps its fixed section list ([Z2-N-BUDGET-410]) and gains no Game2D section |
| Duration | the slice's 20 to 30 minute playthrough script |
| Scripted path | the Game2D playthrough script, including the accessibility and localization flows |
| Metrics | MP-FRAME, MP-HITCH, MP-SUSTAIN, MP-MEM, MP-GPUMEM, MP-SUBMEM, MP-CPUZONE, MP-GPUPASS, MP-LATENCY, MP-CACHE, MP-PKG, MP-DEFECT |
| Primary rows | every row of WS-FRAME, WS-SLICE and WS-SUSTAIN; 2D, UI and text rows |

- [Z2-N-BUDGET-205] The three flagship workloads (W-OpenWorld, W-Cinematic, W-Game2D) are equally binding: no stage acceptance that measures flagship workloads is met while any one of them misses a row it measures.
<!-- elaboration -->
- [Z2-N-BUDGET-206] A flagship playthrough is measured as consecutive segments, each segment meeting the minimum run length, with the initial-load exclusion applied once at the start of the playthrough only; the segment boundaries are the playthrough script's checkpoints.

## Per-system rows

### CPU row statistic

<!-- elaboration -->
- [Z2-N-BUDGET-221] A per-system CPU row is measured with MP-CPUZONE: per frame, the wall-clock span of the named CPU frame-graph node from its start to its completion, including every dynamic sub-job it fans out and joins; the statistic is the p95 of that span over the measured interval.
<!-- elaboration -->
- [Z2-N-BUDGET-222] Every CPU row's evidence record also reports the node's total work (the sum of all its zones' durations across worker threads) per frame as a BR-REPORT figure, as a `rows[]` entry with `statistic: "totalWorkP95"`, so parallel efficiency is visible without being budgeted.
- [Z2-N-BUDGET-223] Every CPU frame-graph node and every sub-job emits a profiler zone named after the node, so every per-system CPU row is measurable without additional instrumentation (`Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`).

### Frame-phase CPU rows

The rows follow the twelve frame phases of `Zenith2/Docs/Architecture.md → "Frame phases and authority"`. The early and late script phases (2 and 10) are budgeted in "Scripting rows".

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-PLAT-001 | Input phase CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for PLAT |
| Z2-B-ENV-002 | Environment update CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for ENV |
| Z2-B-ANIM-002 | Animation phase CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters, W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for ANIM |
| Z2-B-GAME-001 | Movement arbitration phase CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters, W-OpenWorld, W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for GAME |
| Z2-B-PHYS-002 | Physics step CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters, W-Dense, W-OpenWorld, W-Cinematic, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for PHYS |
| Z2-B-ANIM-003 | Post-physics blending CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters, W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for ANIM |
| Z2-B-ANIM-004 | Animation budgeting reduced-rate CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters, W-OpenWorld | BR-REPORT | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-005 | Animation full-rate evaluations per tick cap | pending | pending (one value per platform, the value of `animation.budget.maxFullRateEvaluationsPerTick`) | count | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DEFER | ANIM Stage 4 budget ruling ticket |
| Z2-B-AI-002 | AI and navigation phase CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for AI |
| Z2-B-SCENE-002 | Transform propagation CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for SCENE |
| Z2-B-GAME-002 | Event delivery CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for GAME |
| Z2-B-SAVE-002 | Save-snapshot point CPU per requested save | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for SAVE |
| Z2-B-RPROD-003 | Render extraction CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-ENV-003 | Environment extraction CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-REPORT | Stage 1 per-system budget ticket for ENV |
| Z2-B-ANIM-009 | Animation extraction CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-3D | BR-REPORT | Stage 1 per-system budget ticket for ANIM |
| Z2-B-CHAR-003 | Modular part extraction overhead | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Characters | BR-REPORT | Stage 1 per-system budget ticket for CHAR |
| Z2-B-ECS-002 | Sync-point command playback CPU per tick | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for ECS |

- [Z2-N-BUDGET-481] The row "AI and navigation phase CPU" (Z2-B-AI-002) is the p95 MP-CPUZONE span of the whole of frame phase 7, which also contains GAME's `Gameplay.Rules` and `Gameplay.Cameras`; GAME's pending sub-row "Gameplay framework CPU" is reported inside it (BR-REPORT) and is never summed into `CPU-SIM` a second time (`Zenith2/Docs/Design/GAME.md` → "Rows measured against").
- [Z2-N-BUDGET-475] The row "Physics step CPU" (Z2-B-PHYS-002) is the sum, per tick, of the MP-CPUZONE spans of `Physics.Step`, `Physics.SyncTransforms`, `Physics.RagdollBlend` and `Physics.DeferredQueries`, excluding `Physics.CharacterMove`, which Z2-B-GAME-001 covers; its value is the selected backend's per platform; its W-Game2D column measures those four nodes when the game selects the `Physics` module, and is dropped from the row in D-SEL-2DPHYS's (open, Stage 2) ruling diff if that ruling places 2D physics in TWOD's library.
- [Z2-N-BUDGET-224] The save-snapshot row is measured only on frames at which a save is requested, and the playthrough scripts of W-OpenWorld request at least one save per segment; a save-snapshot frame counts toward the hitch rows like any other frame.
- [Z2-N-BUDGET-468] The rows "Animation budgeting reduced-rate CPU", "Animation full-rate evaluations per tick cap", "Animation stream cache bytes", "Animation stream stalls per run" and "Clip compression error" are allocated by the ANIM Stage 4 budget ruling ticket (`needs-human`, ANIM's epic, fix version `Z2 Stage 4`, blocked by ANIM's update-rate budgeting, clip streaming and clip compression tickets), because their mechanisms land at Stage 4; the reduced-rate row is phase 3's MP-CPUZONE span with significance reduction enabled, reported beside Z2-B-ANIM-002 (measured unreduced) and excluded from every bound; the per-platform values of the evaluations cap and the stream cache are the values of `animation.budget.maxFullRateEvaluationsPerTick` and `animation.streaming.cacheBytes`; the stream cache is a member of Z2-B-ANIM-001; "Animation extraction CPU" is the span of `Animation.ExtractPoses`, inside the span of Z2-B-RPROD-003, so it is reported and excluded from every bound; the Stage 1 per-system budget ticket for ANIM allocates it beside Z2-B-ANIM-001 … -003.
- [Z2-N-BUDGET-239] The row "Environment update CPU" is the summed MP-CPUZONE span of the four phase-1 environment nodes and is a member of `CPU-SIM`; the row "Environment extraction CPU" is the span of the two environment render extractors, which run inside the span of Z2-B-RPROD-003, so it is reported (BR-REPORT) and excluded from every bound.

### Engine service CPU rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-JOBS-001 | Scheduler overhead per frame | pending | pending | ms | MP-JOBOVERHEAD | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for JOBS |
| Z2-B-JOBS-002 | Bounded catch-up fixed steps per frame | pending | pending | count | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-DROP | Stage 1 per-system budget ticket for JOBS |
| Z2-B-GRAPH-001 | CPU frame-graph schedule recompile | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-OpenWorld, W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for GRAPH |
| Z2-B-RHI-004 | Render graph build and compile CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-005 | Command recording and submit CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RHI |
| Z2-B-STREAM-002 | Streaming CPU per frame | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-STREAM | BR-ACCEPT | Stage 1 per-system budget ticket for STREAM |
| Z2-B-AUDIO-002 | Audio update CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-003 | Audio mixer callback CPU per period | pending | pending | % of the audio the callback rendered | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-004 | Real voice count | pending | pending | count | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-DEFER | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-007 | Audio stream decode CPU | pending | pending | ms | MP-COUNTER | Win, S24 | 10% | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-008 | Streamed voice count | pending | pending | count | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-DEFER | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-009 | Audio instance capacity per world | pending | pending | count | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-REFUSE | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-010 | Audio command queue capacity per frame | pending | pending | commands | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-DROP | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-011 | Audio occlusion tests per frame | pending | pending | rays | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-DEFER | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-UI-002 | UI update and layout CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-UI2D, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for UI |
| Z2-B-VFX-002 | CPU particle update | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-005 | VFX lifecycle CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for VFX |
| Z2-B-TWOD-001 | Sprite batching and 2D preparation CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-UI2D, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-004 | 2D physics step CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-UI2D, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-006 | 2D animation CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-UI2D, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for TWOD |
| Z2-B-CINE-002 | Timeline evaluation CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for CINE |
| Z2-B-CINE-003 | Video decode CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for CINE |
| Z2-B-JOBS-005 | Job queue capacities | pending | pending (one value each for the worker deque, the injection queue, the affinity queues and the background queues, high and low; the tools-host queue of tool hosts is outside the row (design constant `uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY`, 1,024)) | jobs | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-REFUSE | Stage 1 per-system budget ticket for JOBS |
| Z2-B-DEVT-002 | On-screen profiler overhead CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for DEVT |
| Z2-B-ASSET-008 | Asset service CPU per frame | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for ASSET |
| Z2-B-PLAT-002 | Platform pump CPU | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for PLAT |
| Z2-B-SCENE-003 | Scene activation slice per frame prologue | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-OpenWorld, W-Cinematic, W-Game2D | BR-DEFER | Stage 1 per-system budget ticket for SCENE |
| Z2-B-LIGHT-020 | LIGHT CPU on Flux.BuildGraph | pending | pending | ms | MP-CPUZONE | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |

- [Z2-N-BUDGET-225] The on-screen profiler is available in game and in editor, on Windows and on the S24, as a runtime toggle with no build flag; it is available in development builds and disabled by default in shipping builds, and its enabled overhead is bounded by the on-screen profiler overhead rows.
- [Z2-N-BUDGET-226] The on-screen profiler overlay shows the live values of the budget procedures: the CPU frame-graph timeline per worker (MP-CPUZONE), GPU time per render-graph pass from the graph's automatic timestamp queries (MP-GPUPASS), frame-time history with hitch markers (MP-FRAME, MP-HITCH), memory including heap, pools, GPU heaps and aliasing savings (MP-SUBMEM, MP-GPUMEM, MP-TRANSIENT), counters including draws, culled instances, streaming and uploads (MP-COUNTER), and Luau time and GC (MP-SCRIPT), each labelled with the row IDs it feeds; Tracy remains the deep-capture tool.
<!-- elaboration -->
- [Z2-N-BUDGET-227] Scheduler overhead (Z2-B-JOBS-001) is not a single profiler zone, so it is measured with MP-JOBOVERHEAD: the per-frame counter `jobs.overhead_ns` (the sum over threads of iteration wall time minus busy and idle time, i.e. scheduler bookkeeping) at its p95 over the measured interval of every WS-FRAME workload, plus the scheduler-only micro-workload `Budgets_JobsSchedulerOnly`, which isolates the scheduler from job bodies.
- [Z2-N-BUDGET-228] `Budgets_JobsSchedulerOnly` runs, per frame for 7,200 frames, a fan-out of 1,024 empty jobs joined by one continuation and a chain of 64 dependent empty continuations, on 1, 2, 8 and all workers; with empty bodies the frame's wall time is scheduler overhead, reported at p95 per worker count; it is built by the Stage 1 per-system budget harness for JOBS, which runs it on the reference engine's `Zenith/TaskSystem` in Stage 1, and runs on `ZenithJobs` from Stage 2.
- [Z2-N-BUDGET-229] A job queue at its capacity row follows its declared full policy (`Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`); the breach code records the `REJECT` case, in which the submission is refused and the submitter runs the job inline, and the `<queue>.full` counter increments.
- [Z2-N-BUDGET-230] The job system's resident memory (the per-thread deques, the job queues, the background operation table and its ticket array, and the per-slot arenas, reported as `jobs.memory_bytes`; a tool host's tools-host background queue and its table slots are counted there too but exist in no WS-FRAME workload) is bounded by the row "Job system resident memory", and the design's per-slot arena caps are defaults bound to that row.
- [Z2-N-BUDGET-231] The audio mixer callback row is a hard real-time limit measured by AUDIO's callback-timing instrument read as counters (`audio.mixer.callback_share_run_max`, `audio.mixer.overruns`; `Zenith2/Docs/Design/AUDIO.md` → "Measuring the mixer callback row"), never by profiler zones on the device thread: the measured quantity per callback is share = callback duration ÷ duration of the audio the callback rendered (blocks × 256 / 48,000 s, or the delivered frames / 48,000 s when it rendered no block), the statistic is the MAXIMUM of that share over the run, and one threshold, `audio.mixer.overrunSharePercent`, equals the row's value, so a single callback above it is an overrun and a breach.
- [Z2-N-BUDGET-240] The row "Platform pump CPU" is the MP-CPUZONE span of the `Platform.Pump` zone (frame prologue step 1, main thread, counter `platform.pump_us`), measured on Windows and the S24, and is a member of `CPU-SIM`.
- [Z2-N-BUDGET-241] The row "Asset service CPU per frame" is the MP-CPUZONE span of the epilogue engine node `Assets.Service`, which bounds its own work by the configuration key `asset.service.frameBudgetUs` allocated from this row; it is a member of `CPU-SIM`.
- [Z2-N-BUDGET-242] The row "LIGHT CPU on Flux.BuildGraph" is the summed span of LIGHT's zones on the render subgraph's `Flux.BuildGraph` node and its sub-jobs; that work runs inside the span of Z2-B-RHI-004, so the row is excluded from every bound and is judged on its own.
- [Z2-N-BUDGET-469] The AUDIO rows "Audio stream decode CPU" (MP-COUNTER `audio.decode.us_delta`, p95 over frames), "Streamed voice count" (`audio.streams.active`), "Audio instance capacity per world" (`audio.instances.live`), "Audio command queue capacity per frame" (`audio.commands.high_water`) and "Audio occlusion tests per frame" (`audio.occlusion.rays_delta`), each a maximum over the run unless stated, are allocated by the Stage 1 per-system budget ticket for AUDIO, with the counters, statistics and breach consequences of `Zenith2/Docs/Design/AUDIO.md` → "Pending new rows".
- [Z2-N-BUDGET-477] The row "Sprite batching and 2D preparation CPU" (Z2-B-TWOD-001) is the MP-CPUZONE sum, per frame, of the spans of `TwoD.Extract`, `Flux.Sprite2D.Prepare` and `Flux.Sprite2D.Write`; the row "2D physics step CPU" is the per-tick sum of the spans of `TwoD.Physics2DStep` and `TwoD.Physics2DSyncTransforms`, and the row "2D animation CPU" the per-tick sum of `TwoD.Animate` and `TwoD.AnimateSkeletons` (`Zenith2/Docs/Design/TWOD.md` → "Pending row needs").
- [Z2-N-BUDGET-478] The row "2D sprite passes GPU" times the passes `Sprite2DScatter`, `Sprite2DCanvas` and `Sprite2DWorld`, and the row "2D canvas composite GPU" the pass `Sprite2DComposite`; the row "2D physics world memory" is the MP-SUBMEM of the selected 2D physics backend's resident bytes, "TWOD module resident memory" that of the module's record tables, poses, overlays and memory consumer, and "2D renderer memory" the MP-SUBMEM of the RENDERING subsystem reported through `Flux_Sprite2DStats::m_ulResidentBytes` (the CPU chunk mirror, the GPU chunk pool and the tile table); D-SEL-2DPHYS (open, Stage 2) compares against the 2D physics step and world memory rows (`Zenith2/Docs/DecisionLog.md` → "D-SEL-2DPHYS"); each W-Game2D cell is measured at the content scale the Stage 1 flagship acceptance ticket for Game2D commits, each W-UI2D cell at the scale of UI's Stage 1 workload scale ticket for W-UI2D.
- [Z2-N-BUDGET-470] The row "VFX lifecycle CPU" is the MP-CPUZONE span of the zone `VFX.Lifecycle` and is a member of `CPU-SIM`; the row "VFX trigger queue capacity per world" is the per-world trigger queue of ZenithVFX, whose counter `vfx.triggers.dropped` stays 0 for every script-accepted trigger, and its value is a script reserve of Z2-B-SCRIPT-024 × Z2-B-JOBS-002 plus the C++ share the Stage 1 per-system budget ticket for VFX rules (`Zenith2/Docs/Design/VFX.md` → "Pending row needs").
- [Z2-N-BUDGET-464] The row "Scene activation slice per frame prologue" is the MP-CPUZONE span of the `Scene.ActivateSlice` zone (frame prologue step 7, main thread: the sliced out-of-tick activation of a prepared scene into an isolated world before a world-transition swap, `Zenith2/Docs/Design/SCENE/Loading.md`), measured on Windows and the S24 in the workloads that perform world transitions; the configuration key `scene.activationBudgetUs` takes the row's value in microseconds as its engine-default layer, and a slice that reaches it stops at the next chunk boundary and continues in the next prologue (BR-DEFER, counted by the deferral counter); it is a member of `CPU-SIM`, allocated by the Stage 1 per-system budget ticket for SCENE beside Z2-B-SCENE-001 and Z2-B-SCENE-002.

### Decompression throughput floors

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-COMPRESS-001 | zstd decompression throughput floor | pending | pending | MB/s | MP-DECOMP | Win, S24 | 5% | — | BR-ACCEPT | Stage 1 per-system budget ticket for COMPRESS |
| Z2-B-COMPRESS-002 | zlib decompression throughput floor | pending | pending | MB/s | MP-DECOMP | Win, S24 | 5% | — | BR-ACCEPT | Stage 1 per-system budget ticket for COMPRESS |

- [Z2-N-BUDGET-232] The decompression throughput floors are lower limits: a codec whose measured one-shot decompress throughput falls below its row minus the tolerance breaches it.
- [Z2-N-BUDGET-233] MP-DECOMP runs `Zenith_Bench_Compression` (`Zenith2/Benchmarks/Compression/`, built with nanobench) in a Release build under controlled conditions, with one discarded warm-up repetition and at least 10 repetitions of at least 11 nanobench epochs each, and the statistic is the median one-shot decompress throughput (decoded bytes ÷ median call time, MB/s of 10^6 bytes) at the codec's default level.
- [Z2-N-BUDGET-234] The corpus is the benchmark corpus `Zenith_CompressionBenchCorpus` (classes `text`, `json`, `json-small`, `floats`, `runs`, `random`, `mixed`, generated at 16 MiB per class), and each floor row is judged on the `mixed` class, with every other class reported; the S24 run pushes the Android build and corpus to `/data/local/tmp/` and runs it through `adb shell`.
- [Z2-N-BUDGET-235] The benchmark report (`schema: "zenith2.bench.compression.v1"`), committed under `Zenith2/Docs/Research/Compression/Stage<N>/<platform>/` with a `report.md` summary, is the raw artifact of the run; the run's evidence record (the block "Evidence record format", with `workload: null`) names it in `artifact` and carries the two floor results as `rows[]` entries with `statistic: "median"`.
<!-- elaboration -->
- [Z2-N-BUDGET-236] The Stage 1 per-system budget ticket for COMPRESS is the `needs-human` ruling that allocates both floors; its harness half, the "Stage 1 per-system budget harness for COMPRESS", is the ticket that delivers `Zenith_Bench_Compression` under `Zenith2/Benchmarks/Compression/` (the COMPRESS benchmark, not a harness under `Zenith2/Benchmarks/Budgets/COMPRESS/`) with its `--smoke` CTest run.

### Shared capacity rows

<!-- informative -->
Shared capacity (entity IDs, event queues, container storage) is checked at the deterministic merge point in merge order, not reserved first-come during parallel execution. These rows are the capacities the merge checks against. Every queue in Zenith2 is bounded; the rows below are the bounded queues whose capacity a script or gameplay author can reach.
<!-- /informative -->

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-ECS-003 | Spawn capacity per tick | pending | pending | entities | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for ECS |
| Z2-B-ECS-004 | Structural command capacity per tick | pending | pending | commands | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for ECS |
| Z2-B-GAME-003 | Event queue capacity per tick | pending | pending | events | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for GAME |
| Z2-B-ASSET-002 | Decode queue depth | pending | pending | decode units | MP-COUNTER | Win, S24 | 0 (hard) | WS-STREAM, W-Load | BR-STALL | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-003 | Asset registry slot capacity | pending | pending | assets | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-REFUSE | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-004 | Asset loads in flight | pending | pending | loads | MP-COUNTER | Win, S24 | 0 (hard) | WS-STREAM, W-Load | BR-DEFER | Stage 1 per-system budget ticket for ASSET |
| Z2-B-STREAM-003 | Concurrent cell loads in flight | pending | pending | cells | MP-COUNTER | Win, S24 | 0 (hard) | WS-STREAM | BR-DEFER | Stage 1 per-system budget ticket for STREAM |
| Z2-B-AI-003 | Asynchronous path requests in flight | pending | pending | requests | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DEFER | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-004 | Navigation tile rebuilds per tick | pending | pending (design default 4) | tiles | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DEFER | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-005 | Path node expansions per tick | pending | pending (design default 32,768) | expansions | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DEFER | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-006 | Sight rays per tick | pending | pending (design default 512) | rays | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DEFER | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-007 | Agent capacity per world | pending | pending (design default 1,024) | agents | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-REFUSE | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-008 | Noise stimuli per tick | pending | pending (design default 256) | noises | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DROP | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-009 | Navigation loads in flight | pending | pending (design default 1,024) | loads | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-DEFER | Stage 1 per-system budget ticket for AI |
| Z2-B-STREAM-006 | Cell activation entities per tick | pending | pending | entities | MP-COUNTER | Win, S24 | 0 (hard) | WS-STREAM | BR-DEFER | Stage 1 per-system budget ticket for STREAM |
| Z2-B-PHYS-003 | Physics body capacity per world | pending | pending (design default 65,536) | bodies | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-Dense, W-OpenWorld | BR-REFUSE | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-004 | Physics constraint capacity per world | pending | pending (design default 16,384) | constraints | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-Dense, W-OpenWorld | BR-REFUSE | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-005 | Physics contact records per tick | pending | pending (design default 8,192) | records | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-Dense, W-OpenWorld | BR-DROP | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-006 | Active ragdolls per world | pending | pending (design default 32) | ragdolls | MP-COUNTER | Win, S24 | 0 (hard) | W-Characters, W-OpenWorld | BR-REFUSE | Stage 1 per-system budget ticket for PHYS |
| Z2-B-CINE-004 | Cinematic instances per world | pending | pending | instances | MP-COUNTER | Win, S24 | 0 (hard) | W-Cinematic | BR-REFUSE | Stage 1 per-system budget ticket for CINE |

- [Z2-N-BUDGET-473] The row "Cell activation entities per tick" is the value of `stream.activation.maxEntitiesPerTick`, below Z2-B-ECS-003: a cell activation that would exceed it in a tick continues in the next tick (BR-DEFER); the cook's per-cell entity limit is min(16,384, the configured `ecs.maxSpawnsPerTick`), so the row stays below Z2-B-ECS-003 (`Zenith2/Docs/Design/STREAM.md` → "Budget rows").
- [Z2-N-BUDGET-474] The four PHYS capacity rows are the values of `physics.maxBodies`, `physics.maxConstraints`, `physics.maxContactRecordsPerTick` and `physics.maxActiveRagdolls` (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md`): a body or constraint creation beyond its capacity fails with `PHYSICS_ERROR_BODY_CAPACITY` or `PHYSICS_ERROR_CONSTRAINT_CAPACITY` (BR-REFUSE), a contact record beyond the per-tick capacity drops the record with the greatest sort key, counted in `physics.contacts.dropped_delta` (BR-DROP), and a ragdoll enter beyond the cap is refused (BR-REFUSE); each is allocated by the PHYS per-system ticket.
- [Z2-N-BUDGET-479] The six AI capacity rows are per world and are the values of AI's configuration keys (`Zenith2/Docs/Design/AI.md` → "Pending row needs"): "Navigation tile rebuilds per tick" is `ai.nav.maxTileRebuildsPerTick`, "Path node expansions per tick" the per-world search expansion budget (64 searches × 512 expansions by default), "Sight rays per tick" `ai.perception.maxRaysPerTick` and "Noise stimuli per tick" `ai.perception.maxNoisesPerTick`; work beyond the first three continues in a later tick (BR-DEFER) and a noise beyond the fourth is dropped (BR-DROP); "Agent capacity per world" is `ai.agent.maxAgents`, and an agent creation beyond it is refused (BR-REFUSE); "Navigation loads in flight" is `ai.nav.maxLoadsInFlight`, the load records counted by `ai.nav.loads_in_flight` from completion-ID allocation to publication, and a load beyond it waits for a free record (BR-DEFER). Each is allocated by the AI per-system ticket.
- [Z2-N-BUDGET-480] The row "Asynchronous path requests in flight" (Z2-B-AI-003) is the path service's active set, counted by `ai.path.active`: a request beyond it is deferred (BR-DEFER, counted by `ai.path.deferred_delta`), whether a C++ caller or a script made it; a script's path request is first capped at the merge by Z2-B-SCRIPT-026's pathfinding value (ruling Q19 (a)), and an accepted request then enters the active set like any other.
- [Z2-N-BUDGET-488] The row "Video decode CPU" is the MP-CPUZONE span of the `Zenith.VideoDec` thread's decode zone per decoded second of 1080p30 video, on a dedicated thread outside the frame graph's simulation and render paths, so it is a member of no CPU bound; the row "Cinematic instances per world" is the value of `cine.maxInstancesPerWorld`, and a start beyond it is refused (BR-REFUSE) (`Zenith2/Docs/Design/CINE.md` → "Pending row needs").
- [Z2-N-BUDGET-237] A shared-capacity row's value is identical on every worker count and chunk layout, and the merge consumes it in merge order, so the entity that is rejected when capacity runs out is the same on every run of the same replay.
- [Z2-N-BUDGET-238] Decompression back-pressure uses the decode queue depth row: I/O is throttled by decode capacity, and a full decode queue stalls the I/O issuer instead of growing.
- [Z2-N-BUDGET-243] The decode queue depth row counts decode units, each one reserved read or one undecoded section held between its read and its decode, which is the unit of the configuration key `asset.decode.queueDepth`.
- [Z2-N-BUDGET-244] The row "Asset registry slot capacity" is the configuration key `asset.registry.maxAssets`: the registry never grows, and a registration that finds no free slot fails with `REGISTRY_FULL` (BR-REFUSE); the row "Asset loads in flight" is `asset.load.maxInFlight`, and a load beyond it waits in admission holding no load record until one returns (BR-DEFER).

### Latency and duration rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-ASSET-009 | Critical asset load latency | pending | pending | ms | MP-COUNTER | Win, S24 | default | W-Load | BR-ACCEPT | Stage 1 per-system budget ticket for ASSET |
| Z2-B-PLAT-003 | Async I/O critical read latency | pending | pending | ms | MP-COUNTER | Win, S24 | default | WS-STREAM, W-Load | BR-ACCEPT | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-004 | Resume-state write duration | pending | pending | ms | MP-COUNTER | S24 | default | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-005 | Configuration and bindings build time at startup | pending | pending | ms | MP-CPUZONE | Win, S24 | default | W-Load | BR-ACCEPT | Stage 1 per-system budget ticket for PLAT |
| Z2-B-ANIM-007 | Animation stream stalls per run | pending | pending | count | MP-COUNTER | Win, S24 | 0 (hard) | W-Cinematic | BR-ACCEPT | ANIM Stage 4 budget ruling ticket |
| Z2-B-SAVE-003 | Save write duration per save | pending | pending | ms | MP-COUNTER | Win, S24 | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-005 | Save load duration per save | pending | pending | ms | MP-COUNTER | Win, S24 | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-006 | World restore duration per 10,000 entities | pending | pending | ms | MP-COUNTER | Win, S24 | default | W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for SAVE |
| Z2-B-AUDIO-006 | Audio output latency | pending | pending | ms | MP-COUNTER | Win, S24 | 10% | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-012 | Audio stream underruns per run | pending | pending | count | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |

- [Z2-N-BUDGET-245] The row "Critical asset load latency" is the p95, over the W-Load repetitions, of the time from the request to the activation of a `CRITICAL`-priority load of the W-Load start scene's largest asset.
- [Z2-N-BUDGET-246] The row "Async I/O critical read latency" is the p95 of the gauge `io.critical_latency_us` (queue to completion of a critical read) measured while a saturating background load runs, on Windows and the S24; the PLAT harness under `Zenith2/Benchmarks/Platform/` supplies the saturating load.
- [Z2-N-BUDGET-465] The SAVE rows "Save write duration per save" (counter `save.write.latency_us`), "Save load duration per save" (`save.load.last_us`), "World restore duration per 10,000 entities" (`save.restore.us_per_10k_entities`) and "Save file size per slot" (`save.write.last_bytes`, in "Package and storage rows") are MP-COUNTER rows whose statistic is the maximum over the run, allocated by the Stage 1 per-system budget ticket for SAVE (`Zenith2/Docs/Design/SAVE.md` → "Pending row needs"); the replay log has no row, being bounded by the configuration limit `replay.maxLogBytes`.
- [Z2-N-BUDGET-466] The row "Audio output latency" is the p95 over the run of `audio.device.latency_us`, read once per 100 ms after a 10 s warm-up, on Windows with a wired output and on the S24 speaker; the row "Audio stream underruns per run" is the total of `audio.streams.underruns` over the run, 0 tolerated.
- [Z2-N-BUDGET-467] The row "Animation stream stalls per run" is the total of `anim.stream.stalls_delta` over a W-Cinematic run, 0 tolerated in the flagship.
- [Z2-N-BUDGET-247] The row "Resume-state write duration" is the S24 time of the resume-state write performed on backgrounding, from the start of the write to its durable completion; the row "Configuration and bindings build time at startup" is the MP-CPUZONE span of startup step 6, which builds the configuration store and the input bindings, on Windows and the S24.

## Rendering rows

### GPU row statistic

- [Z2-N-BUDGET-251] Every GPU pass row is measured with MP-GPUPASS from the timestamp queries the render graph inserts automatically around every pass; no pass adds its own timing code.
<!-- elaboration -->
- [Z2-N-BUDGET-252] Where the graph compiler merges consecutive raster passes into one rendering scope, the merged scope is timed as one unit and is judged against the sum of the merged passes' rows; the frame dump names the merge.
- [Z2-N-BUDGET-253] A pass row is measured for the main view in every shading path that contains the pass; the allocation writes one value per (platform, shading path) cell, and the Deferred and Visibility-buffer cells exist on Windows only, because those paths are desktop only and Forward+ is the only path on Android.
- [Z2-N-BUDGET-254] Every pass in `Zenith2/Docs/Rendering.md → "Per-pass specifications"` names exactly one GPU pass row of this section as its budget row.

### Common prologue pass rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-004 | GPU-scene scatter upload GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-GEOM-003 | Streaming upload bytes per frame | pending | pending | MB | MP-COUNTER | Win, S24 | 0 (hard) | WS-STREAM | BR-DEFER | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-004 | Compute skinning and morph targets GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Characters, W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for GEOM |
| Z2-B-VFX-003 | Particle simulation GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for VFX |
| Z2-B-GEOM-005 | Procedural geometry regeneration GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for GEOM |
| Z2-B-LIGHT-001 | Light and decal list build GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-002 | Shadow views total GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |

### Per-view Forward+ pass rows

- [Z2-N-BUDGET-255] The rows of this table also budget the same pass kinds in the other shading paths wherever a path contains them (Forward reuses the prepass, HiZ, culling, fog, SSAO, screen-space shadows, sky, SSR, transparent and particle rows; Deferred reuses those same rows plus clustering; Visibility buffer reuses the HiZ, clustering, fog, SSAO, sky, SSR, transparent and particle rows, and its culling and raster have their own rows); the table "Allocation sums" is the authoritative membership of each path.

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-005 | Instance culling phase 1 GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-006 | Depth velocity normal prepass GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-007 | HiZ pyramid build GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-008 | Instance culling phase 2 GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-LIGHT-003 | Light clustering GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-004 | Volumetric fog GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-005 | SSAO GTAO and denoise GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-006 | Screen-space shadows GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Outdoor, W-OpenWorld, W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-RPROD-009 | Forward opaque GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-ENV-001 | Sky and atmosphere GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Outdoor, W-OpenWorld, W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for ENV |
| Z2-B-LIGHT-007 | SSR trace denoise and composite GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-RPROD-010 | Transparent forward GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-VFX-004 | Particles and ribbons draw GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for VFX |

### Forward path pass rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-011 | Per-object light list build GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-012 | Forward per-object opaque GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-013 | Per-object light cap | pending | pending | lights per object | MP-COUNTER | Win, S24 | 0 (hard) | WS-3D | BR-DEFER | Stage 1 per-system budget ticket for RPROD |

- [Z2-N-BUDGET-256] In the Forward path, lights beyond the per-object light cap are dropped from that object's list by the selection order `Zenith2/Docs/Rendering.md → "Frame anatomy Forward"` defines, and a per-frame counter reports the number of dropped (object, light) pairs.

### Deferred path pass rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-014 | G-buffer pass GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-GEOM-006 | Deferred decals GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for GEOM |
| Z2-B-LIGHT-008 | Deferred lighting GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |

### Visibility buffer path pass rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-015 | Meshlet culling GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-016 | Visibility raster GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-017 | Material classification GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-018 | Material resolve GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-019 | Velocity reconstruction GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-LIGHT-009 | Visibility buffer lighting GPU | pending | pending | ms | MP-GPUPASS | Win | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |

### Common epilogue pass rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-LIGHT-010 | TAA or temporal upscaler GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-011 | Auto-exposure GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-012 | Depth of field GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Cinematic | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-013 | Motion blur GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-014 | Bloom GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-015 | Tonemap and colour grading GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-UI-003 | UI composite GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-UI2D, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for UI |
| Z2-B-TWOD-002 | 2D sprite passes GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-UI2D, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-003 | 2D canvas composite GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-UI2D, W-Game2D | BR-ACCEPT | Stage 1 per-system budget ticket for TWOD |
| Z2-B-DEVT-003 | Debug overlays and on-screen profiler GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for DEVT |
| Z2-B-RPROD-020 | Frame pacing present-interval deviation | pending | pending | ms | MP-FRAME | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |

<!-- elaboration -->
- [Z2-N-BUDGET-257] The frame pacing row's statistic is the p95 of `abs(frameMs(i) − targetIntervalMs)` over the measured interval, where the target interval is the display refresh interval divided into the frame-rate target (16.67 ms at 60 fps on a 60 Hz or 120 Hz display).

### Upload ring and GPU memory rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RHI-006 | Upload ring hard cap | pending | pending | MB | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME, W-Load | BR-REFUSE | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-007 | Upload ring stall frames | pending | pending | frames | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-008 | Frames in flight | adopted | 2 to 3, configurable | frames | MP-COUNTER | Win, S24 | 0 (exact) | WS-FRAME | BR-INVALID | derived from Z2-D-050 |
| Z2-B-RPROD-021 | CPU render lag | adopted | 1 | frames | MP-COUNTER | Win, S24 | 0 (exact) | WS-FRAME | BR-INVALID | Z2-D-050 (plan text) |
| Z2-B-RHI-011 | GPU readback pool size | pending | pending | MB | MP-SUBMEM | Win, S24 | default | WS-FRAME | BR-DEFER | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-009 | Bindless heap capacity per kind | pending | pending (one value each for sampled textures, storage textures, buffers and samplers) | descriptors | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-REFUSE | Stage 1 per-system budget ticket for RHI |

- [Z2-N-BUDGET-258] The upload ring stalls until space frees, and grows only up to the upload ring hard cap row; a single request larger than the cap is split, or else refused with an error, and ring exhaustion increments a telemetry counter.
- [Z2-N-BUDGET-271] The row "Upload ring stall frames" counts the frames of the measured interval on which the upload ring stalled or the frame was deferred because the graph compile's `ExtendReservation` returned `DEFERRED` (counter `Flux.Upload.ReservationDeferredFrames`, `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`); a frame with both counts once, and its breach consequence stays BR-ACCEPT.
- [Z2-N-BUDGET-259] Frames in flight are configurable between 2 and 3; every budget run records the configured value in its evidence record, and each workload definition fixes it.
- [Z2-N-BUDGET-260] The CPU side of the frame pipeline has exactly one frame of render lag: simulation of frame N+1 overlaps rendering of frame N and never runs further ahead (Z2-D-050), and GPU frames in flight are counted separately.
- [Z2-N-BUDGET-261] The bindless heap capacities are fixed per platform at device creation from the bindless heap capacity row, and a descriptor allocation beyond a heap's capacity returns an error instead of growing the heap.

### Views permutations and PSO rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-022 | View count per frame per quality tier | pending | pending (one value per quality tier of QualityMatrix.md) | views | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-DEFER | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-023 | Texture views and captures per frame | pending | pending (one value per quality tier) | views | MP-COUNTER | Win, S24 | 0 (hard) | WS-3D | BR-DEFER | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-029 | D24 depth-precision tolerance | pending | pending (below the 0.477 m ceiling) | m | MP-DEPTHPREC | Win, S24 | 0 (hard) | WS-3D | BR-FALLBACK | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-030 | On-disk pipeline-cache size | pending | pending | MB | MP-DISKCACHE | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-024 | Shader permutation limit per game | pending | pending | permutations | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-COOK | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-025 | Missing-PSO events per qualification run | adopted | 0 | events | MP-COUNTER | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | derived from Z2-D-025 |

- [Z2-N-BUDGET-262] The view count per frame is capped per quality tier by the view count row; texture views and captures beyond their per-frame row are round-robined across frames, and the main view and the shadow views of visible shadowed lights are never deferred.
- [Z2-N-BUDGET-263] The D24 precision check computes, for each view with standard Z and D24, the worst-case linear-depth step of a 24-bit depth buffer at the view's far distance from its near and far planes (MP-DEPTHPREC); a view whose step exceeds the "D24 depth-precision tolerance" row uses reversed Z with D32F_S8 instead (BR-FALLBACK), as `Zenith2/Docs/Rendering.md → "Render views"` defines.
- [Z2-N-BUDGET-264] The D24 depth-precision tolerance row is allocated below 0.477 m, the ceiling `Zenith2/Docs/Rendering.md → "Render views"` derives for its reference outdoor view; the allocation table states the value's basis.
- [Z2-N-BUDGET-265] The on-disk pipeline-cache size row bounds the persisted Vulkan pipeline cache and D3D12 PSO library files of one game per driver version, measured after a warm-cache run of every workload the game carries (MP-DISKCACHE).
- [Z2-N-BUDGET-266] The cook reports the shader permutation count per game, including every stream-set layout that a vertex precision fallback creates, and a game whose count exceeds the shader permutation limit row fails its cook.
<!-- elaboration -->
- [Z2-N-BUDGET-267] A qualification run records zero missing-PSO events (the draw-skip and magenta-fallback counters of the missing-PSO fallback both read zero at the end of the run), because PSO precaching makes a miss a defect; development runs report the counters as BR-REPORT figures.

### HiZ on tile-based GPUs

<!-- informative -->
On tile-based GPUs, two-phase HiZ culling forces a mid-frame depth store and reload between the prepass phases, which costs bandwidth on the S24. Zenith2 does not merge passes into tile-local scopes until the Stage 5 decision D-TBDR (open, Stage 5) is measured on the S24 (Z2-D-036). The row below budgets the bandwidth in the meantime, and the D-TBDR (open, Stage 5) measurement covers it.
<!-- /informative -->

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-RPROD-026 | HiZ mid-frame depth store and reload bandwidth | pending | pending | MB per frame | MP-BANDWIDTH | S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |

- [Z2-N-BUDGET-268] The HiZ mid-frame depth store and reload bandwidth row is measured on the S24 with Adreno counters over the prepass phase 1 store, the HiZ build read and the prepass phase 2 reload, and the D-TBDR (open, Stage 5) measurement reports it for every candidate configuration.

### Virtual texturing and path tracer rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-ADVGFX-002 | Virtual texturing tile uploads per frame | pending | pending | tiles | MP-COUNTER | Win, S24 | 0 (hard) | WS-STREAM | BR-DEFER | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-003 | Virtual texturing feedback and page-table update GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-STREAM | BR-ACCEPT | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-004 | Path tracer resolution cap per reference scene | pending | pending (one value per reference scene) | px | MP-PATHTRACE | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-005 | Path tracer wall-clock cap per reference scene | pending | pending (one value per reference scene) | s | MP-PATHTRACE | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-006 | Path tracer minimum samples per pixel | pending | pending (one value per reference scene) | spp | MP-PATHTRACE | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-007 | Path tracer convergence variance threshold | pending | pending (one value per reference scene) | variance | MP-PATHTRACE | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-RPROD-031 | Per-scene parity tolerances | pending | pending (one FLIP mean and one per-channel threshold per parity scene, per comparison: backend and path) | FLIP mean, per-channel | MP-PARITY | Win, Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for RPROD |
| Z2-B-ADVGFX-008 | Physical-accuracy error tolerance per reference scene | pending | pending (one value per reference scene) | FLIP mean | MP-PATHTRACE | Host | 0 (hard) | — | BR-ACCEPT | Stage 1 per-system budget ticket for ADVGFX |

- [Z2-N-BUDGET-269] The per-scene parity tolerances row holds one tolerance pair per parity scene for each comparison the BackendParity and PathParity steps make, and a comparison outside its pair fails the step (BR-TEST); features the capability matrix excludes from a comparison carry no pair.
- [Z2-N-BUDGET-270] WARP and lavapipe path tracer runs are capped in resolution and wall-clock time per reference scene by the path tracer cap rows; the tracer runs to its convergence criterion (per-pixel variance estimate below the variance threshold row, with at least the minimum samples per pixel row), and a run that reaches its wall-clock cap first fails.

### Lighting environment and terrain pass rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-ENV-004 | Water surface and underwater GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-005 | Detail foliage generation GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-REPORT | Stage 1 per-system budget ticket for ENV |
| Z2-B-GEOM-007 | Terrain geometry GPU | pending | pending | ms | MP-GPUPASS (A/B difference, [Z2-N-BUDGET-276]) | Win, S24 | default | W-Outdoor, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for GEOM |
| Z2-B-LIGHT-016 | SSGI GPU | pending | pending | ms | MP-GPUPASS | Win | default | W-Cinematic, W-Indoor | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-017 | Probe capture and filter GPU | pending | pending | ms | MP-GPUPASS | Win, S24 | default | WS-3D | BR-ACCEPT | Stage 1 per-system budget ticket for LIGHT |

- [Z2-N-BUDGET-272] The row "SSGI GPU" times the `SSGI` pass (trace and denoise) of every desktop shading path on Windows; the S24 runs no SSGI, so the row has no S24 cell, and the pass's own Rendering per-pass table names it as its budget row.
- [Z2-N-BUDGET-273] The row "Probe capture and filter GPU" times the common-prologue `SkyCapture` and `ProbeFilter` passes per frame, including the reflection-probe captures the per-frame capture count of Z2-B-RPROD-023 admits; the sky passes themselves stay under Z2-B-ENV-001.
- [Z2-N-BUDGET-274] The row "Water surface and underwater GPU" times the water surface passes and the `Env_AddUnderwater` composite of the main view in every shading path, and from Stage 5 the prologue `Env_AddOceanFFT` dispatches of the FFT ocean tier; it is a per-view member of every `GPU-<PATH>` bound.
- [Z2-N-BUDGET-283] The row Z2-B-ENV-001 "Sky and atmosphere GPU" times LIGHT's sky pass `SkyAtmosphere` together with ENV's cloud passes: `Env_AddCloudLayer`, recorded once per frame before `SkyAtmosphere`, and from Stage 5 `Env_AddVolumetricClouds` and `Env_AddCloudShadowMap`.
- [Z2-N-BUDGET-275] The row "Detail foliage generation GPU" is the detail foliage share of the procedural geometry regeneration passes timed by Z2-B-GEOM-005; it is reported (BR-REPORT) and excluded from every bound.
- [Z2-N-BUDGET-276] The row "Terrain geometry GPU" is the time of the terrain node draws inside the prepass, opaque and shadow-view passes, measured on W-Outdoor and W-OpenWorld; it is judged on its own and excluded from every bound, because those passes' own rows already contain it. Its value is an A/B difference, because MP-GPUPASS times whole passes and no pass holds the terrain draws alone: per (path, view kind), the p95 of the prepass, opaque and shadow-view pass timestamps of the workload run minus the same p95 of the same scripted run with terrain draws disabled by GEOM's development-only key `geom.terrain.enabled` = false (`Zenith2/Docs/Design/GEOM.md`), 10 repetitions of each, reported as the median and 95% interval of the per-repetition differences ([Z2-N-BUDGET-107]'s enabled and disabled comparison is the precedent).

### Rendering capacity and latency rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-ENV-007 | Placed foliage instances resident cap | pending | pending | instances | MP-COUNTER | Win, S24 | 0 (hard) | W-Outdoor, W-OpenWorld | BR-DROP | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-008 | Foliage activation catch-up frames | pending | pending | frames | MP-STREAM | Win, S24 | 0 (hard) | WS-STREAM | BR-ACCEPT | Stage 1 per-system budget ticket for ENV |
| Z2-B-GEOM-009 | Texture streaming request-to-resident latency | pending | pending | frames | MP-COUNTER | Win, S24 | default | WS-STREAM | BR-ACCEPT | Stage 1 per-system budget ticket for GEOM |
| Z2-B-LIGHT-021 | Shadowed local lights per frame per tier | pending | pending (one value per quality tier) | lights | MP-COUNTER | Win, S24 | 0 (hard) | W-Indoor | BR-DROP | Stage 1 per-system budget ticket for LIGHT |

- [Z2-N-BUDGET-277] The row "Shadowed local lights per frame per tier" is the per-tier value of the configuration key `render.quality.maxShadowedLights`; a local light beyond it renders unshadowed for that frame and is counted in `Flux.Shadows.Dropped` (BR-DROP).
- [Z2-N-BUDGET-278] The row "Placed foliage instances resident cap" is the configuration key `environment.foliage.instanceCap` (W-Outdoor's 100,000 placed instances is its sizing case); instances beyond it are shed render-only first, in ascending thinning rank, and counted in `environment.foliage.instances_shed` (BR-DROP).
- [Z2-N-BUDGET-279] The row "Foliage activation catch-up frames" is the number of frames after a foliage cell's activation within which `environment.foliage.activation_backlog` drains back to zero in the MP-STREAM teleport and rapid-traversal runs.
- [Z2-N-BUDGET-280] The row "Texture streaming request-to-resident latency" is the p95, in frames, from a texture residency requirement change to the frame on which the swap to the required mip level completes, over the WS-STREAM runs.

### Approximation error rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-ENV-009 | Cloud layer approximation error | pending | pending | FLIP mean | MP-APPROX | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-010 | Volumetric cloud approximation error | pending | pending | FLIP mean | MP-APPROX | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-011 | Underwater composite error | pending | pending | FLIP mean | MP-APPROX | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-012 | Ocean FFT to query-set height error | pending | pending | m | MP-APPROX | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-013 | Environment render test tolerances | pending | pending (one FLIP mean and one per-channel threshold per golden capture) | FLIP mean, per-channel | MP-APPROX | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for ENV |
| Z2-B-ANIM-008 | Clip compression error | pending | pending (one value each for the default ε and the cinematic preset) | m | MP-APPROX | Host | 0 (hard) | — | BR-TEST | ANIM Stage 4 budget ruling ticket |
| Z2-B-LIGHT-022 | SSGI error budget | pending | pending (one value each for energy, accuracy, stability and off-screen) | ×, % | MP-APPROX | Host | 0 (hard) | — | BR-TEST | Stage 1 per-system budget ticket for LIGHT |

- [Z2-N-BUDGET-281] Every row of this table is a tolerance of an approximation-error test (MP-APPROX), never a time or memory budget; the test named by the owning design compares the feature with its reference and fails (BR-TEST) when the measured error exceeds the row.
- [Z2-N-BUDGET-282] The rows and their tests are: "Cloud layer approximation error" (`Test_Clouds_LayerMatchesReference`, against a CPU integrator of the same model at 256 steps); "Volumetric cloud approximation error" (`Test_Clouds_VolumetricMatchesHighStepReference`, against the same pass at 512 steps with no temporal reuse); "Underwater composite error" (`Test_Underwater_CompositeMatchesReference`, against a 1,024-step CPU single-scattering march over a flat seabed); "Ocean FFT to query-set height error" (`Test_Water_FftFitWithinBudget`, against the fitted Gerstner set, in metres); "Environment render test tolerances" (the FLIP and per-channel thresholds of the environment render tests' golden captures); "SSGI error budget" (the energy, accuracy, stability and off-screen criteria of the SSGI approximation-error scenes, one value per criterion); "Clip compression error" (`Test_ClipCompression_ErrorBoundHeldEveryFrame`, the measured hierarchical error of every joint at every frame against the default ε and the cinematic preset, in metres).

## Scripting rows

### Script CPU budgets

<!-- informative -->
The script CPU budget is a safepoint count, not a time. Luau's interrupt fires at safepoints (loop back-edges, calls and returns), and the count is deterministic for the same bytecode, so a budget exhaustion is replayable. Wall-clock time is only a fatal watchdog. Time spent inside C++ bindings is not counted by safepoints, which is why the watchdog remains the backstop there.
<!-- /informative -->

- [Z2-N-BUDGET-291] Script CPU budgets count safepoints (Z2-D-052); the safepoint budget rows are the limits the interrupt callback enforces, and exhausting one raises a controlled Luau error inside the invocation's transaction.
<!-- elaboration -->
- [Z2-N-BUDGET-292] The chunk-system safepoint budget applies per entity (the counter resets at the start of each entity's logical transaction), so whether a budget is exhausted never depends on chunk size or layout; the behaviour safepoint budget applies per behaviour callback (this follows from Z2-D-019 and Z2-D-051 and is confirmed or refined by D-SCRIPT-TXN refinement (open, Stage 1), whose ruling the Stage 1 per-system budget ticket for SCRIPT is `blocks`-linked from).
- [Z2-N-BUDGET-293] Tripping the wall-clock watchdog is a fatal, non-replayable diagnostic event, never a recoverable rollback, and its threshold is the watchdog row.
- [Z2-N-BUDGET-294] The script phase CPU rows are measured in wall-clock with MP-SCRIPT for profiling and budgeting only; no runtime decision reads wall-clock time except the watchdog.

### Luau memory and garbage collection

- [Z2-N-BUDGET-295] Every world has a per-world GC step budget row, in kilobytes of collector work per VM per tick, and the script runtime steps the Luau garbage collector of each of the world's VMs within that allowance every tick: a worker VM's allowance resets at its first chunk-system sub-job of the tick and the main VM's at the world's tick boundary, and the allowance is four times the row's value while the memory governor reports `MEMORY_PRESSURE_CRITICAL` (`Zenith2/Docs/Scripting.md → "VM model and threading"`).
- [Z2-N-BUDGET-296] Luau memory caps are enforced through the allocator each VM is created with: the world main VM against its row, and each per-worker chunk-system VM against the per-worker row; hitting a VM memory cap is fatal and not replayable (BR-FATAL: `SCRIPT_ERROR_KIND_VM_MEMORY`, the world halts), never a controlled Luau error, because a VM's heap occupancy depends on which invocations shared it and so varies with worker count (`Zenith2/Docs/Scripting.md → "VM model and threading"`).
- [Z2-N-BUDGET-297] The VM memory cap rows are sized with headroom over the per-entity snapshot and behaviour-state rows, so content within those budgets never reaches a VM cap, and a workload run that reaches one fails its acceptance.
- [Z2-N-BUDGET-298] World creation refuses a configuration whose per-worker VM cap times its worker count exceeds the row "VM memory times workers per world" (BR-REFUSE).
- [Z2-N-BUDGET-299] One VM per worker thread per world is loaded lazily for chunk systems (Z2-D-018), and the product of the per-worker VM memory cap and the world's worker count is capped by the row "VM memory times workers per world".

### Snapshot and checkpoint rows

- [Z2-N-BUDGET-300] Snapshot memory has a hard per-entity cap and a per-tick total, both rows of this section; exceeding the per-entity cap raises a Luau error for that entity, which triggers the entity rollback.
- [Z2-N-BUDGET-301] The per-entity snapshot cap is per entity, never per call, so it does not vary with chunk size.
- [Z2-N-BUDGET-302] The behaviour-state checkpoint is serialized into reserved memory inside the transaction before commit, only when the declared state is dirty and at most once per tick, and its cost is budgeted by the checkpoint cost rows.

### Scripting row table

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-SCRIPT-001 | Behaviour callback safepoint budget | pending | pending | safepoints | MP-SCRIPT | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-LUAU | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-002 | Chunk-system safepoint budget per entity | pending | pending | safepoints | MP-SCRIPT | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-LUAU | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-003 | Script wall-clock watchdog per invocation | pending | pending | ms | MP-SCRIPT | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-FATAL | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-004 | Early scripts phase CPU | pending | pending | ms | MP-SCRIPT | Win, S24 | default | W-Dense, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-005 | Late scripts phase CPU | pending | pending | ms | MP-SCRIPT | Win, S24 | default | W-Dense, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-006 | Chunk-system CPU per tick | pending | pending | ms | MP-SCRIPT | Win, S24 | default | W-Dense, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-007 | Per-world GC step budget | pending | pending | KB of GC work per VM per tick | MP-SCRIPT | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-REPORT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-008 | Luau GC time per frame | pending | pending | ms | MP-SCRIPT | Win, S24 | default | W-Dense, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-009 | World main VM memory cap | pending | pending | MB | MP-SUBMEM | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-FATAL | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-010 | Per-worker chunk-system VM memory cap | pending | pending | MB | MP-SUBMEM | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-FATAL | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-011 | VM memory times workers per world | pending | pending | MB | MP-SUBMEM | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-REFUSE | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-012 | Snapshot memory per entity | pending | pending | KB | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-LUAU | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-013 | Snapshot memory per tick | pending | pending | MB | MP-COUNTER | Win, S24 | default | W-Dense, WS-SLICE | BR-REPORT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-014 | Checkpoint serialization CPU per tick | pending | pending | ms | MP-SCRIPT | Win, S24 | default | W-Dense, WS-SLICE | BR-ACCEPT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-015 | Checkpoint reserved memory per behaviour instance | pending | pending | KB | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-LUAU | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-016 | Container storage capacity per tick | pending | pending | KB | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-017 | Script GC tuning parameters | pending | pending (goal %, step multiplier, step size in KB) | %, ×, KB | MP-SCRIPT | Win, S24 | 0 (exact) | W-Dense, WS-SLICE | BR-REPORT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-018 | Recovery safepoint budget per OnRecover or Migrate step | pending | pending | safepoints | MP-SCRIPT | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-LUAU | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-019 | Module-load safepoint budget per module scope | pending | pending | safepoints | MP-SCRIPT | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-LOAD | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-020 | Script safepoints per world per tick | pending | pending | safepoints | MP-SCRIPT | Win, S24 | default | W-Dense, WS-SLICE | BR-REPORT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-021 | Checkpoint bytes per tick | pending | pending | KB | MP-COUNTER | Win, S24 | default | W-Dense, WS-SLICE | BR-REPORT | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-022 | String-intern capacity per world session | pending | pending (entries; KB) | entries, KB | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-023 | Audio trigger capacity per tick | pending | pending | triggers | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-024 | VFX trigger capacity per tick | pending | pending | triggers | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-VFX-006 | VFX trigger queue capacity per world | pending | pending | triggers | MP-COUNTER | Win, S24 | 0 (hard) | W-Cinematic, W-OpenWorld | BR-DROP | Stage 1 per-system budget ticket for VFX |
| Z2-B-SCRIPT-025 | Save-request capacity per tick | pending | pending | requests | MP-COUNTER | Win, S24 | 0 (hard) | WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-026 | Per-source asynchronous request capacity in flight | pending | pending (one value each for script path requests, streaming loads, deferred physics queries and online calls) | requests | MP-COUNTER | Win, S24 | 0 (hard) | W-Dense, WS-SLICE | BR-MERGE | Stage 1 per-system budget ticket for SCRIPT |

Rows the scripting runtime reads that live under other headings (cross-listed here, defined once where they are):

| Scripting use | Row | Heading |
|---|---|---|
| command-queue capacity at the merge point | Z2-B-ECS-004 Structural command capacity per tick | "Per-system rows" |
| event-queue capacity at the merge point | Z2-B-GAME-003 Event queue capacity per tick | "Per-system rows" |
| entity slots for script spawns | Z2-B-ECS-003 Spawn capacity per tick | "Per-system rows" |
| asynchronous path requests from scripts | Z2-B-SCRIPT-026 Per-source asynchronous request capacity in flight (its pathfinding value, at the merge); an accepted request then counts in Z2-B-AI-003 Asynchronous path requests in flight ([Z2-N-BUDGET-480]) | "Scripting row table", "Per-system rows" |
| script hot reload in the editor | Z2-B-EDIT-002 Script hot reload | "Editor iteration rows" |

<!-- elaboration -->
- [Z2-N-BUDGET-303] The per-entity snapshot cap is the only snapshot limit a script can trip: exceeding it raises a `SNAPSHOT_CAP` Luau error inside that entity's transaction (BR-LUAU); the per-tick snapshot total is a measured and reported row (BR-REPORT) that never causes a rollback, because the per-chunk snapshot total depends on chunk layout (`Zenith2/Docs/Scripting.md → "Budgets safepoints and the watchdog"`; confirmed by D-SCRIPT-TXN refinement (open, Stage 1)).
- [Z2-N-BUDGET-471] The value of Z2-B-SCRIPT-025 "Save-request capacity per tick" is SAVE's `save.maxRequestsPerTick`, one number: script requests above it are merge rejections (BR-MERGE) and C++ requests above it return `SAVE_ERROR_REQUEST_LIMIT`, and SAVE declares no second limit.
- [Z2-N-BUDGET-304] The string-intern, audio-trigger, VFX-trigger, save-request and per-source asynchronous capacity rows are capacity classes checked at the merge point in merge order (BR-MERGE), as `Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"` defines; the recovery and module-load safepoint rows are enforced by the same interrupt callback as the invocation safepoint rows.

## Editor iteration rows

### Iteration targets

- [Z2-N-BUDGET-321] Editor iteration targets are the numeric W-Editor rows: script hot reload, shader blob reload, asset reimport, scene open and play-in-editor start, each with a Windows limit.
<!-- elaboration -->
- [Z2-N-BUDGET-322] Each iteration row's operation is timed by MP-EDIT from the triggering event to the operation's completion event: script hot reload from the source file's save to the first tick that runs the new bytecode; shader blob reload from the `.slang` save to the first frame drawn with the rebuilt PSOs; asset reimport from the source file's save (or the reimport command) to the first frame showing the reimported asset; scene open from the open command to the first editor frame with the scene fully loaded; play-in-editor start from the Play command to the first simulated tick of the play world.
- [Z2-N-BUDGET-323] A shader parameter-struct layout change needs a C++ rebuild and is outside the shader blob reload row; the editor reports that a rebuild is needed instead of timing a reload.

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-EDIT-002 | Script hot reload | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 W-Editor row ticket for script hot reload |
| Z2-B-EDIT-003 | Shader blob reload | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 W-Editor row ticket for shader blob reload |
| Z2-B-EDIT-004 | Asset reimport | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 W-Editor row ticket for asset reimport |
| Z2-B-EDIT-005 | Scene open | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 W-Editor row ticket for scene open |
| Z2-B-EDIT-006 | Play-in-editor start | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 W-Editor row ticket for play-in-editor start |
| Z2-B-ASSET-011 | Incremental no-op cook time per slice | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for ASSET |
| Z2-B-EDIT-011 | Terrain edit delta memory per undo step | pending | pending | MB | MP-SUBMEM | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-012 | Terrain sculpt preview latency | pending | pending | ms | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-013 | Editor frame CPU | pending | pending | ms | MP-CPUZONE | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-014 | Editor projection apply CPU per 1,000 changes | pending | pending | ms | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-015 | Editor undo history memory | pending | pending | MB | MP-SUBMEM | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-ANIM-010 | Animation graph preview latency | pending | pending | ms | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-CONTENT-010 | Batch reference repair time per 1,000 documents | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-011 | Semantic merge time of a 16 MiB scene | pending | pending | ms | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-012 | Shared DDC hit ratio on a warm incremental cook | pending | pending | % | MP-COUNTER | Win | default | — | BR-REPORT | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CHAR-002 | Lip-sync import time per audio minute | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for CHAR |
| Z2-B-WORLD-001 | World partition import time | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-002 | World partition import memory | pending | pending | MB | MP-SUBMEM | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-003 | Lighting bake time per cell | pending | pending | s | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-004 | Lighting bake tracer memory | pending | pending | MB | MP-SUBMEM | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-005 | Edit-world region open latency | pending | pending | ms | MP-EDIT | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for WORLD |

- [Z2-N-BUDGET-327] The row "Incremental no-op cook time per slice" is the MP-EDIT time of an incremental cook of a flagship slice whose sources are unchanged since the previous cook, from the cook command to its completion; it is part of the asset reimport row Z2-B-EDIT-004's cost and is allocated by the ASSET per-system ticket.
- [Z2-N-BUDGET-476] The editor rows of `Zenith2/Docs/Design/EDIT.md` → "Pending row needs" are allocated by the EDIT per-system ticket: "Editor frame CPU" is the median frame's MP-CPUZONE span of `Editor.Frame` with W-Editor's scene open; "Editor projection apply CPU per 1,000 changes" is the MP-EDIT time of applying 1,000 projected changes; "Editor undo history memory" is the MP-SUBMEM size of the undo history, editor only and outside the Player totals ([Z2-N-BUDGET-143]), and its value sets `editor.undo.maxBytes`; "Animation graph preview latency" (ANIM's row) is the MP-EDIT time from a graph edit to the first frame drawing the recompiled pose.
- [Z2-N-BUDGET-328] The row "Terrain edit delta memory per undo step" bounds the editor memory one terrain sculpt or paint undo step holds, and the row "Terrain sculpt preview latency" is the MP-EDIT time from a brush stroke sample to the first editor frame that shows its displaced terrain; both are editor-only rows of the W-Editor workload, allocated by the EDIT per-system ticket.
- [Z2-N-BUDGET-485] The CONTENT editor rows of `Zenith2/Docs/Design/CONTENT.md` → "Pending new rows": "Batch reference repair time per 1,000 documents" is the MP-EDIT time of a move of 1,000 assets referenced from 1,000 scene documents of the W-Editor workload, each scene referencing 10 of the assets spread across the folder, from `Plan` to COMPLETED (planning, chunk packing and the per-chunk repetition of repairs included), and the measurement reports the plan's document-repair count beside the time; "Semantic merge time of a 16 MiB scene" is the MP-EDIT time of `ZenithContent merge` over the W-Editor workload's largest scene with 1% of its entities changed on each side; "Shared DDC hit ratio on a warm incremental cook" is the cook report's `ddcSharedHits` over `items` on a second host after one host cooked the slice, a reported row (BR-REPORT).
- [Z2-N-BUDGET-486] The WORLD rows of `Zenith2/Docs/Design/WORLD.md` → "Pending row needs" are tool-side rows of the W-Editor workload on OpenWorldSlice's world: "World partition import time" is the MP-EDIT time of the `world.world` import, clean and after a one-region edit (`Zenith2/Docs/Design/WORLD/Partition.md` → "Partition cost and scaling"); "Lighting bake time per cell" and "Edit-world region open latency" are MP-EDIT times; "World partition import memory" and "Lighting bake tracer memory" are the MP-SUBMEM peaks of tool processes, outside every Player bound.
- [Z2-N-BUDGET-487] The row "Lip-sync import time per audio minute" is the MP-EDIT time of `char.lipsync` imports divided by the imported audio minutes, and the row "Modular part extraction overhead" is the MP-CPUZONE cost of `Animation.ExtractPoses` added by the W-Characters modular variant's part entities, reported inside the span of Z2-B-RPROD-003 and excluded from every bound (`Zenith2/Docs/Design/CHAR.md` → "Pending row needs").

### Usability walkthrough targets

- [Z2-N-BUDGET-324] Editor usability acceptance is a set of scripted task walkthroughs with time targets (build a scene, author a prefab variant, iterate a script, reimport an asset), each a row of this section, run by a human with MP-WALKTHROUGH.
- [Z2-N-BUDGET-325] The complete character workflow (import, rig and retarget, author the animation graph, preview, in game) and the complete audio workflow (import, bank and event authoring, mix, audition in context) are timed scripted walkthroughs with their own rows.
<!-- elaboration -->
- [Z2-N-BUDGET-326] Each walkthrough's script is a numbered step list committed at `Zenith2/Docs/Research/Walkthroughs/<RowName>.md`, written by the walkthrough row's producer; the human follows it without prior rehearsal on the build under test, and the evidence record stores the per-step times.

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-EDIT-007 | Walkthrough build a scene | pending | pending | min | MP-WALKTHROUGH | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-008 | Walkthrough author a prefab variant | pending | pending | min | MP-WALKTHROUGH | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-009 | Walkthrough iterate a script | pending | pending | min | MP-WALKTHROUGH | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-010 | Walkthrough reimport an asset | pending | pending | min | MP-WALKTHROUGH | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for EDIT |
| Z2-B-CHAR-001 | Walkthrough complete character workflow | pending | pending | min | MP-WALKTHROUGH | Win | default | CinematicSlice | BR-ACCEPT | Stage 1 per-system budget ticket for CHAR |
| Z2-B-AUDIO-005 | Walkthrough complete audio workflow | pending | pending | min | MP-WALKTHROUGH | Win | default | W-Editor | BR-ACCEPT | Stage 1 per-system budget ticket for AUDIO |

## Gate budget rows

<!-- informative -->
The gate budget rows are specified in `Zenith2/Docs/Budgets/GateRows.md`, split out for length, under the headings "Phase caps", "Per-step hard timeouts", "Gates stay fast" and "CI integration smoke"; their rows are listed in "Row index" with every other row.
<!-- /informative -->

- [Z2-N-BUDGET-483] `Zenith2/Docs/Budgets/GateRows.md` is part of this document: every rule here that names this document, its rows, its row tables or its sections applies to that file's rows, tables and sections exactly as to this file's, and its rows appear in "Row index" like every other row.

## Measured in every workload run

<!-- informative -->
Six measurements are taken in every workload run, whatever else the run is for. Stage 1 sets their limit rows. Until then these rows are pending, and the tickets citing them are blocked by their producers.
<!-- /informative -->

- [Z2-N-BUDGET-371] Every workload run measures: cold versus warm caches (shader, PSO and DDC); teleport and rapid-traversal streaming (where the workload streams); transient memory peaks; input-to-photon latency; individual hitches (the count of frames over 33.3 ms, and the maximum frame time); and, in W-Editor, editor iteration (script, shader and asset hot reload).

### Cache state pairs

<!-- elaboration -->
- [Z2-N-BUDGET-372] A cold-cache run starts with the engine's PSO cache files, the Vulkan pipeline cache or D3D12 PSO library, the driver shader cache and the derived-data cache (DDC) deleted for the build under test, exactly as after a fresh install; a warm-cache run starts after one complete run of the same build and workload version has populated them.
- [Z2-N-BUDGET-373] Every frame-rate workload is measured as a cold-cache and warm-cache pair; the warm-cache run is judged against every frame-rate, hitch and per-system row, and the cold-cache run against the cold-cache hitch rows and the frame-time rows.

### Teleport and rapid traversal

<!-- elaboration -->
- [Z2-N-BUDGET-374] MP-STREAM performs, in each WS-STREAM workload run, five scripted teleports to fixed destinations at least 1 km apart and one rapid-traversal leg at the workload's maximum traversal speed; teleport-to-resident time runs from the teleport command's sync point to the first frame at which every cell and texture the destination view requires is resident at its required LOD and mip, and a residency miss is a frame that draws any required asset below its required LOD or mip.
- [Z2-N-BUDGET-375] A teleport resets physics velocity, interpolation, animation root and the render motion-vector history, so the frames after a teleport show no velocity smear; the frames after a teleport count toward the frame-rate and hitch rows like any other.

### Transient memory peaks

- [Z2-N-BUDGET-376] The transient memory peak is measured with aliasing on (the budgeted configuration) and with aliasing off (reported), and the on-screen profiler shows transient memory with and without aliasing, and the savings.

### Input-to-photon latency

- [Z2-N-BUDGET-377] The input-to-photon latency row accounts for the extra frame of CPU render lag that pipelining through render extraction adds (Z2-D-050), and for the GPU frames in flight; its allocation table decomposes the value into input sampling, simulation, the pipelining frame, GPU frames in flight, and presentation.
<!-- elaboration -->
- [Z2-N-BUDGET-378] MP-LATENCY injects one synthetic input event every 2 s at the platform layer with a recorded timestamp, identifies the first present whose simulation consumed it (through an input-sequence number carried from the Input phase into `Flux_RenderWorld` and on to the present), and takes the display time of that present from `VK_KHR_present_wait` on Windows Vulkan, `IDXGISwapChain::GetFrameStatistics` on D3D12 and `VK_GOOGLE_display_timing` on the S24; the latency of the event is the display time minus the injection timestamp.
<!-- elaboration -->
- [Z2-N-BUDGET-379] Stage 6 qualification confirms MP-LATENCY against a physical measurement (a photodiode or 240 fps camera over the display with a hardware input trigger), run by a human for each platform; the engine-side estimate is evidence only once a qualification report shows it within 5 ms of the physical measurement.

### Hitches

- [Z2-N-BUDGET-380] A hitch is a measured frame whose frame time is greater than 33.3 ms; each run reports the hitch count and the maximum frame time of its measured interval.

### Measured-in-every-run rows

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-SHIP-015 | Hitch count warm caches | pending | pending (one value per workload) | frames | MP-HITCH | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-016 | Maximum frame time warm caches | pending | pending (one value per workload) | ms | MP-HITCH | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-017 | Hitch count cold caches | pending | pending (one value per workload) | frames | MP-CACHE | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-018 | Maximum frame time cold caches | pending | pending (one value per workload) | ms | MP-CACHE | Win, S24 | 0 (hard) | WS-FRAME | BR-ACCEPT | Stage 3 per-system budget ticket for SHIP |
| Z2-B-STREAM-004 | Teleport-to-resident time | pending | pending | s | MP-STREAM | Win, S24 | default | WS-STREAM | BR-ACCEPT | Stage 1 per-system budget ticket for STREAM |
| Z2-B-STREAM-005 | Rapid-traversal residency miss frames | pending | pending | frames | MP-STREAM | Win, S24 | 0 (hard) | WS-STREAM | BR-ACCEPT | Stage 1 per-system budget ticket for STREAM |
| Z2-B-RHI-010 | Transient memory peak | pending | pending | MB | MP-TRANSIENT | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RHI |
| Z2-B-RPROD-027 | Input-to-photon latency | pending | pending | ms | MP-LATENCY | Win, S24 | default | WS-FRAME | BR-ACCEPT | Stage 1 per-system budget ticket for RPROD |

## Production-quality acceptance

### Separation from research measurements

- [Z2-N-BUDGET-401] Production-quality acceptance is defined in Stage 1, implemented incrementally, and kept separate from the research measurements of ECS backends and shading paths; a research result never satisfies a production-quality acceptance item, and a production-quality measurement never substitutes for a research run.
- [Z2-N-BUDGET-402] Production-quality acceptance comprises exactly: the feature × platform quality and fallback matrix (`Zenith2/Docs/QualityMatrix.md`); the content specifications (`Zenith2/Docs/ContentSpecs.md`); the editor iteration targets (the W-Editor rows); and the acceptance package of each of the three flagship slices.

### QualityMatrix schema

<!-- informative -->
`Zenith2/Docs/QualityMatrix.md` is written by the Stage 1 QualityMatrix ticket. For every rendering, animation, audio and UI feature it gives the technique per quality tier on Windows and on the S24, its fallback, and the visual acceptance reference: a golden capture plus a FLIP threshold, or a human sign-off for a subjective feature. The schema below is exact so that the coverage gate step and the acceptance runner can parse it.
<!-- /informative -->

- [Z2-N-BUDGET-403] `Zenith2/Docs/QualityMatrix.md` has exactly these `##` sections, in order: `## Purpose`, `## Quality tiers`, `## Rendering features`, `## Animation features`, `## Audio features`, `## UI features`, `## Change rule`.
- [Z2-N-BUDGET-404] `## Quality tiers` holds one table with the columns `| Tier ID | Platform | Description | Output resolution | Dynamic resolution range | Upscaler | Frames in flight |`, with Tier IDs of the form `QUALITY_TIER_<PLATFORM>_<NAME>` (for example `QUALITY_TIER_S24_DEFAULT`), and is the tier list that the per-tier rows of this document (view count, texture views and captures) allocate against.
- [Z2-N-BUDGET-405] Each feature section holds one table with the columns `| Feature ID | Feature | Area | Requirements | Tier ID | Windows technique | S24 technique | Fallback | Fallback trigger | Acceptance kind | Reference | Threshold | Budget rows |`, one row per (feature, tier).
- [Z2-N-BUDGET-406] Feature IDs have the form `Z2-Q-<AREA>-NNN`, are unique across the file, are never reused, and every feature's `Requirements` cell lists the `Z2-R-<AREA>-NNN` requirement IDs it realises.
- [Z2-N-BUDGET-407] `Acceptance kind` is `golden` or `human`: a `golden` row's `Reference` is the repo-relative path of its golden capture and its `Threshold` is a FLIP mean threshold plus per-channel thresholds written `FLIP<=x; R<=a G<=b B<=c`; a `human` row's `Reference` names the flagship slice and playthrough segment where the human reviews it, and its `Threshold` reads `human sign-off`.
<!-- elaboration -->
- [Z2-N-BUDGET-408] Golden captures live under `Zenith2/Games/<Game>/Acceptance/Goldens/<FeatureID>_<TierID>_<Win|S24>.png`, are LFS-tracked, and are regenerated only by a ticket that cites the feature ID and records the reason in `## Compatibility`.
- [Z2-N-BUDGET-409] `Fallback` names the technique used when the tier's technique is unavailable, and `Fallback trigger` names the capability flag, pressure level or thermal state that selects it; a feature with no fallback reads `none` in both cells and its row states why.

### ContentSpecs schema

<!-- informative -->
`Zenith2/Docs/ContentSpecs.md` is written by the Stage 1 ContentSpecs ticket. It is the representative content scale that "production quality" is measured against: texel density and texture budgets; triangle, LOD and material complexity per asset class; character rig and bone counts, blend-shape counts, animation layers and IK chains per character class; audio voice counts and banks; and UI element counts.
<!-- /informative -->

- [Z2-N-BUDGET-410] `Zenith2/Docs/ContentSpecs.md` has exactly these `##` sections, in order: `## Purpose`, `## Asset classes`, `## Texel density and texture budgets`, `## Geometry and LOD complexity`, `## Material complexity`, `## Character classes`, `## Audio`, `## UI`, `## Validation`, `## Change rule`.
- [Z2-N-BUDGET-411] `## Asset classes` holds `| Class ID | Asset class | Examples | Owning area | Slices |`, with Class IDs of the form `CONTENT_CLASS_<NAME>`.
- [Z2-N-BUDGET-412] `## Texel density and texture budgets` holds `| Class ID | Texel density (texels per metre) | Maximum texture resolution | Textures per material | Windows compression | S24 compression | Resident texture memory per instance (MB) |`, where the compression cells name BC7/BC5/BC4/BC6H on Windows and an ASTC block size on the S24.
- [Z2-N-BUDGET-413] `## Geometry and LOD complexity` holds `| Class ID | LOD0 triangles maximum | LOD count | Triangle ratio per LOD step | LOD screen-size thresholds | Materials per mesh maximum | Vertex precision fallbacks allowed |`.
- [Z2-N-BUDGET-414] `## Material complexity` holds `| Class ID | Texture samples maximum | Permutations maximum | Two-sided allowed | Alpha-tested allowed | Transparent allowed |`.
- [Z2-N-BUDGET-415] `## Character classes` holds `| Character class ID | Bones maximum | Blend shapes maximum | Animation layers maximum | IK chains maximum | LOD0 triangles maximum | Update-rate reduction tiers |`, with IDs of the form `CHARACTER_CLASS_<NAME>`, and the W-Characters character (60 bones) is one of its classes. Each class row's content-rule selector in `Zenith2/Docs/ContentSpecs.rules.json` is the `.meta` label `character-<name>` (lower case, `^character-[a-z0-9-]{1,22}$`), the label CHAR's import wizard writes from the game's character project document (`Zenith2/Docs/Design/CHAR/Facial.md` → "Character project settings document"), so CHAR's per-game classes and the ContentSpecs rows meet in one string; CHAR's class presets `hero`, `npc` and `crowd` (`Zenith2/Docs/Design/CHAR/Debugging.md` → "Character class presets") are classes of the table, and the Update-rate reduction tiers cell of a class whose preset forces full rate (the default `hero`) lists tier 1 only, every other class 1, 2, 4, 8.
- [Z2-N-BUDGET-416] `## Audio` holds `| Slice | Real voices maximum | Virtual voices maximum | Streamed voices maximum | Banks | Bank memory maximum (MB) |`, one row per flagship slice.
- [Z2-N-BUDGET-417] `## UI` holds `| Slice or screen | UI elements maximum | Text runs maximum | Visible glyphs maximum | Draw batches maximum |`.
- [Z2-N-BUDGET-418] Every numeric cell of ContentSpecs.md is checked by the content validation the CONTENT and ASSET areas deliver, at cook time, and a cooked asset that exceeds its class's limit fails the cook (BR-COOK); `## Validation` names the validator for each table.
- [Z2-N-BUDGET-419] The sum of the ContentSpecs memory figures of a slice's content fits inside the slice's per-subsystem memory rows; the ContentSpecs ticket and the per-system budget tickets for GEOM, ANIM, AUDIO and UI reconcile the two in their allocation tables.

### Flagship slice acceptance package

- [Z2-N-BUDGET-420] The three flagship slices (OpenWorldSlice, CinematicSlice, Game2D) are equally binding (Z2-D-021), and each has, written by its Stage 1 flagship acceptance ticket: a playthrough script, a feature checklist mapped to requirement IDs, per-platform budgets, a bug bar, and a human-review sign-off list for its subjective quality items.
<!-- elaboration -->
- [Z2-N-BUDGET-421] A slice's acceptance package lives under `Zenith2/Games/<Slice>/Acceptance/` as `Playthrough.md` (the human-readable script), `Playthrough.replay` (the input-replay file the Player plays back), `FeatureChecklist.md`, `Budgets.md` (the list of row IDs that apply to the slice, never numbers), `BugBar.md` and `SignOff.md`.

### Playthrough script format

- [Z2-N-BUDGET-422] A playthrough script is a scripted walkthrough of 20 to 30 minutes (row "Playthrough script duration"), divided into segments, and plays back deterministically from its replay file on fixed simulation ticks.
<!-- elaboration -->
- [Z2-N-BUDGET-423] `Playthrough.md` holds one table `| Segment | Start (mm:ss) | Duration (s) | Checkpoint | Content exercised | Feature IDs | Requirement IDs | Assertions |`, where `Checkpoint` names the save or checkpoint the segment starts from, so a segment can be replayed alone, and `Assertions` lists the correctness asserts the integration smoke and the acceptance run check at the segment's end.
- [Z2-N-BUDGET-424] Every segment is at least the minimum run length, so each segment is a valid measured interval on its own.

### Feature checklist format

- [Z2-N-BUDGET-425] `FeatureChecklist.md` holds one table `| Item | Requirement IDs | Feature IDs | Segment | Evidence kind | Evidence |`, where `Evidence kind` is `automated` (an assert or evidence record), `golden` (a QualityMatrix golden) or `human` (a SignOff.md item), and every requirement the slice's area owners assigned to the slice appears in at least one item.

### Per-platform budgets for a slice

- [Z2-N-BUDGET-426] A slice's per-platform budgets are the rows of this document whose Workloads column names the slice's workload or a workload set containing it, plus the QualityMatrix rows of the slice's tiers; the slice's `Budgets.md` lists those row IDs and Feature IDs and never restates a value.

| Slice | Workload | Rows that apply |
|---|---|---|
| OpenWorldSlice | W-OpenWorld | every row naming W-OpenWorld, WS-FRAME, WS-3D, WS-SLICE, WS-SUSTAIN or WS-STREAM |
| CinematicSlice | W-Cinematic | every row naming W-Cinematic, WS-FRAME, WS-3D, WS-SLICE or WS-SUSTAIN |
| Game2D | W-Game2D | every row naming W-Game2D, WS-FRAME, WS-SLICE or WS-SUSTAIN |

### Bug bar and severity weights

- [Z2-N-BUDGET-427] Each slice's bug bar is zero open crash or blocker defects and a severity-weighted open-defect count at or below the slice's cap row.
<!-- elaboration -->
- [Z2-N-BUDGET-428] Defects are BUG tickets on the Z2 board whose body names the slice (or whose parent epic's work the slice exercises) and whose status is not in a DONE category; `S1_CRITICAL` (crash, data loss, or a hard block on progression) counts as a crash or blocker, and the weighted count is `5 × S2_MAJOR + 2 × S3_MINOR + 1 × S4_TRIVIAL`.

| Severity | Meaning | Weight | Bar |
|---|---|---|---|
| S1_CRITICAL | crash, data loss, or a hard block on progression | — | zero open |
| S2_MAJOR | a feature is broken, with no reasonable workaround | 5 | counted toward the cap |
| S3_MINOR | a feature is degraded, with a workaround | 2 | counted toward the cap |
| S4_TRIVIAL | cosmetic | 1 | counted toward the cap |

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-SHIP-019 | Open crash or blocker defects per slice | adopted | 0 | defects | MP-DEFECT | Win, S24 | 0 (hard) | WS-SLICE | BR-ACCEPT | Z2-D-012 (plan text) |
| Z2-B-SHIP-020 | Severity-weighted open-defect cap OpenWorldSlice | pending | pending | weighted defects | MP-DEFECT | Win, S24 | 0 (hard) | W-OpenWorld | BR-ACCEPT | Stage 1 flagship acceptance ticket for OpenWorldSlice |
| Z2-B-SHIP-021 | Severity-weighted open-defect cap CinematicSlice | pending | pending | weighted defects | MP-DEFECT | Win, S24 | 0 (hard) | W-Cinematic | BR-ACCEPT | Stage 1 flagship acceptance ticket for CinematicSlice |
| Z2-B-SHIP-022 | Severity-weighted open-defect cap Game2D | pending | pending | weighted defects | MP-DEFECT | Win, S24 | 0 (hard) | W-Game2D | BR-ACCEPT | Stage 1 flagship acceptance ticket for Game2D |
| Z2-B-SHIP-023 | Playthrough script duration | adopted | 20 to 30 | min | MP-FRAME | Win, S24 | 0 (hard) | WS-SLICE | BR-INVALID | Z2-D-012 (plan text) |

### Human sign-off items

- [Z2-N-BUDGET-429] `SignOff.md` holds one table `| Item | Feature IDs | Segment | Platform | Question the reviewer answers | Reviewer | Date | Verdict |`, the human reviewer runs the named segment on the named platform, and a slice's acceptance is met only when every item's Verdict reads `accepted`.
- [Z2-N-BUDGET-430] Sign-off items cover only subjective quality; anything a golden capture, an assert or an evidence record can check is not a sign-off item.
- [Z2-N-BUDGET-431] A sign-off is performed through a `needs-human` ticket, and the human records the verdict in `SignOff.md` in that ticket's diff.

### OpenWorldSlice acceptance

- [Z2-N-BUDGET-432] OpenWorldSlice's acceptance package exercises streaming, world authoring, AI and navigation at scale, persistent-world saves, and the Luau quest, dialogue and inventory samples, which are Luau gameplay modules inside the slice and never engine features (Z2-D-077).
<!-- elaboration -->
- [Z2-N-BUDGET-433] OpenWorldSlice's sign-off list includes at least: traversal feel and camera behaviour; streaming pop-in visibility at the traversal speed; lighting and atmosphere over a day/night transition; AI crowd believability; quest and dialogue flow readability; and save/load continuity after a mid-streaming save.

### CinematicSlice acceptance

- [Z2-N-BUDGET-434] CinematicSlice's acceptance package exercises character and animation quality, cinematics, lighting and image quality, audio, and set-pieces, including cinematic start, interrupt and skip with gameplay-state restore.
<!-- elaboration -->
- [Z2-N-BUDGET-435] CinematicSlice's sign-off list includes at least: facial animation and lip-sync; character skin, hair-card and cloth appearance within the classified simulations; cinematic camera framing and depth of field; the audio mix and music transitions; set-piece readability; and the absence of visible pops on cinematic skip.

### Game2D acceptance

- [Z2-N-BUDGET-436] Game2D's acceptance package exercises 2D, UI, accessibility flows and localization, including the whole menus, settings and first 10 minutes completed with a screen reader, with remapped controls, with reduced motion and with a colour-blind mode, each a scripted acceptance run.
<!-- elaboration -->
- [Z2-N-BUDGET-437] Game2D's sign-off list includes at least: pixel-perfect sprite presentation at every supported output scale; UI navigation and focus clarity with a controller and with touch; screen-reader narration usefulness; colour-blind mode legibility; and pseudo-localized and right-to-left layouts.

### Animation budgeting and PSO preparation

- [Z2-N-BUDGET-438] Animation budgeting (significance-based update-rate reduction) and PSO preparation (precaching from recorded and predicted PSO sets, the missing-PSO fallback, and no first-use stalls) are implementation tickets in ANIM and RPROD respectively, not only performance targets; their rows are the animation phase rows and the missing-PSO and cold-cache rows of this document.

### Performance acceptance evidence

- [Z2-N-BUDGET-439] The performance part of every stage acceptance is: every workload that exists at that stage measured per this document on the reference desktop (both backends) and the S24, with evidence records whose fingerprints match the accepted tree, meeting every adopted row they measure.
- [Z2-N-BUDGET-440] The Stage 6 acceptance additionally includes the sustained thermal runs of WS-SUSTAIN, the package and install-size rows, and the physical confirmation of MP-LATENCY.
- [Z2-N-BUDGET-441] Every stage sprint's mid-sprint integration check measures the flagship slices' playthrough scripts (or, before the slices exist, the workloads that do) on the reference desktop and the S24 against these rows and files every breach as a BUG ticket, so drift is caught before the stage acceptance.

## Row index

The table below lists every budget row of this document, in ID order within each area. It is the table the coverage gate step, the memory-governor table generator and the ticket drafters read.

- [Z2-N-BUDGET-461] The Row index lists every row of this document exactly once, with the same ID, name, status, value, unit and producer as the row's own table, and is regenerated in the same diff as any row change.

### Summary by area

| Area | Adopted | Pending | Total |
|---|---|---|---|
| ADVGFX | 0 | 8 | 8 |
| AI | 0 | 9 | 9 |
| ANIM | 0 | 10 | 10 |
| ASSET | 0 | 11 | 11 |
| AUDIO | 0 | 12 | 12 |
| CHAR | 0 | 3 | 3 |
| CINE | 0 | 4 | 4 |
| COMPRESS | 0 | 2 | 2 |
| CONTENT | 0 | 12 | 12 |
| CORE | 3 | 10 | 13 |
| DEVT | 0 | 3 | 3 |
| ECS | 0 | 4 | 4 |
| EDIT | 0 | 15 | 15 |
| ENV | 0 | 13 | 13 |
| GAME | 0 | 3 | 3 |
| GEOM | 0 | 9 | 9 |
| GRAPH | 0 | 1 | 1 |
| JOBS | 0 | 5 | 5 |
| LIGHT | 0 | 22 | 22 |
| PACK | 0 | 11 | 11 |
| PHYS | 0 | 6 | 6 |
| PLAT | 0 | 6 | 6 |
| PROC | 4 | 27 | 31 |
| RHI | 1 | 11 | 12 |
| RPROD | 2 | 29 | 31 |
| SAVE | 0 | 6 | 6 |
| SCENE | 0 | 3 | 3 |
| SCRIPT | 0 | 26 | 26 |
| SHIP | 16 | 7 | 23 |
| STREAM | 0 | 6 | 6 |
| TWOD | 0 | 8 | 8 |
| UI | 0 | 3 | 3 |
| VFX | 0 | 6 | 6 |
| WORLD | 0 | 5 | 5 |
| All | 26 | 316 | 342 |

### All rows

| ID | Name | Status | Area | Value | Unit | Producer |
|---|---|---|---|---|---|---|
| Z2-B-ADVGFX-001 | Virtual texturing physical cache and page tables | pending | ADVGFX | pending | MB | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-002 | Virtual texturing tile uploads per frame | pending | ADVGFX | pending | tiles | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-003 | Virtual texturing feedback and page-table update GPU | pending | ADVGFX | pending | ms | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-004 | Path tracer resolution cap per reference scene | pending | ADVGFX | pending (one value per reference scene) | px | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-005 | Path tracer wall-clock cap per reference scene | pending | ADVGFX | pending (one value per reference scene) | s | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-006 | Path tracer minimum samples per pixel | pending | ADVGFX | pending (one value per reference scene) | spp | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-007 | Path tracer convergence variance threshold | pending | ADVGFX | pending (one value per reference scene) | variance | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-ADVGFX-008 | Physical-accuracy error tolerance per reference scene | pending | ADVGFX | pending (one value per reference scene) | FLIP mean | Stage 1 per-system budget ticket for ADVGFX |
| Z2-B-AI-001 | Navigation mesh and pathfinding memory | pending | AI | pending | MB | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-002 | AI and navigation phase CPU | pending | AI | pending | ms | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-003 | Asynchronous path requests in flight | pending | AI | pending | requests | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-004 | Navigation tile rebuilds per tick | pending | AI | pending (design default 4) | tiles | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-005 | Path node expansions per tick | pending | AI | pending (design default 32,768) | expansions | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-006 | Sight rays per tick | pending | AI | pending (design default 512) | rays | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-007 | Agent capacity per world | pending | AI | pending (design default 1,024) | agents | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-008 | Noise stimuli per tick | pending | AI | pending (design default 256) | noises | Stage 1 per-system budget ticket for AI |
| Z2-B-AI-009 | Navigation loads in flight | pending | AI | pending (design default 1,024) | loads | Stage 1 per-system budget ticket for AI |
| Z2-B-ANIM-001 | Animation clips, skeletons and pose buffers | pending | ANIM | pending | MB | Stage 1 per-system budget ticket for ANIM |
| Z2-B-ANIM-002 | Animation phase CPU | pending | ANIM | pending | ms | Stage 1 per-system budget ticket for ANIM |
| Z2-B-ANIM-003 | Post-physics blending CPU | pending | ANIM | pending | ms | Stage 1 per-system budget ticket for ANIM |
| Z2-B-ANIM-004 | Animation budgeting reduced-rate CPU | pending | ANIM | pending | ms | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-005 | Animation full-rate evaluations per tick cap | pending | ANIM | pending (one value per platform, the value of `animation.budget.maxFullRateEvaluationsPerTick`) | count | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-006 | Animation stream cache bytes | pending | ANIM | pending (one value per platform, the value of `animation.streaming.cacheBytes`) | MB | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-007 | Animation stream stalls per run | pending | ANIM | pending | count | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-008 | Clip compression error | pending | ANIM | pending (one value each for the default ε and the cinematic preset) | m | ANIM Stage 4 budget ruling ticket |
| Z2-B-ANIM-009 | Animation extraction CPU | pending | ANIM | pending | ms | Stage 1 per-system budget ticket for ANIM |
| Z2-B-ANIM-010 | Animation graph preview latency | pending | ANIM | pending | ms | Stage 1 per-system budget ticket for EDIT |
| Z2-B-ASSET-001 | Asset system resident data and decode buffers | pending | ASSET | pending | MB | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-002 | Decode queue depth | pending | ASSET | pending | decode units | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-003 | Asset registry slot capacity | pending | ASSET | pending | assets | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-004 | Asset loads in flight | pending | ASSET | pending | loads | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-005 | Asset in-flight load bytes | pending | ASSET | pending | MB | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-006 | Asset I/O staging bytes | pending | ASSET | pending | MB | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-007 | Asset keep-alive cache bytes | pending | ASSET | pending | MB | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-008 | Asset service CPU per frame | pending | ASSET | pending | ms | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-009 | Critical asset load latency | pending | ASSET | pending | ms | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-010 | Local DDC size | pending | ASSET | pending | GB | Stage 1 per-system budget ticket for ASSET |
| Z2-B-ASSET-011 | Incremental no-op cook time per slice | pending | ASSET | pending | s | Stage 1 per-system budget ticket for ASSET |
| Z2-B-AUDIO-001 | Audio banks, streams and mixer | pending | AUDIO | pending | MB | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-002 | Audio update CPU | pending | AUDIO | pending | ms | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-003 | Audio mixer callback CPU per period | pending | AUDIO | pending | % of the audio the callback rendered | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-004 | Real voice count | pending | AUDIO | pending | count | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-005 | Walkthrough complete audio workflow | pending | AUDIO | pending | min | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-006 | Audio output latency | pending | AUDIO | pending | ms | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-007 | Audio stream decode CPU | pending | AUDIO | pending | ms | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-008 | Streamed voice count | pending | AUDIO | pending | count | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-009 | Audio instance capacity per world | pending | AUDIO | pending | count | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-010 | Audio command queue capacity per frame | pending | AUDIO | pending | commands | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-011 | Audio occlusion tests per frame | pending | AUDIO | pending | rays | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-AUDIO-012 | Audio stream underruns per run | pending | AUDIO | pending | count | Stage 1 per-system budget ticket for AUDIO |
| Z2-B-CHAR-001 | Walkthrough complete character workflow | pending | CHAR | pending | min | Stage 1 per-system budget ticket for CHAR |
| Z2-B-CHAR-002 | Lip-sync import time per audio minute | pending | CHAR | pending | s | Stage 1 per-system budget ticket for CHAR |
| Z2-B-CHAR-003 | Modular part extraction overhead | pending | CHAR | pending | ms | Stage 1 per-system budget ticket for CHAR |
| Z2-B-CINE-001 | Cinematic timelines and video decode buffers | pending | CINE | pending | MB | Stage 1 per-system budget ticket for CINE |
| Z2-B-CINE-002 | Timeline evaluation CPU | pending | CINE | pending | ms | Stage 1 per-system budget ticket for CINE |
| Z2-B-CINE-003 | Video decode CPU | pending | CINE | pending | ms | Stage 1 per-system budget ticket for CINE |
| Z2-B-CINE-004 | Cinematic instances per world | pending | CINE | pending | instances | Stage 1 per-system budget ticket for CINE |
| Z2-B-COMPRESS-001 | zstd decompression throughput floor | pending | COMPRESS | pending | MB/s | Stage 1 per-system budget ticket for COMPRESS |
| Z2-B-COMPRESS-002 | zlib decompression throughput floor | pending | COMPRESS | pending | MB/s | Stage 1 per-system budget ticket for COMPRESS |
| Z2-B-CONTENT-001 | LFS tracking threshold | pending | CONTENT | pending | bytes | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-002 | Source-asset quota OpenWorldSlice | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-003 | Source-asset quota CinematicSlice | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-004 | Source-asset quota Game2D | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-005 | Source-asset quota WorkloadIndoor | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-006 | Source-asset quota WorkloadOutdoor | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-007 | Source-asset quota WorkloadCharacters | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-008 | Source-asset quota WorkloadDense | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-009 | Source-asset quota WorkloadUI2D | pending | CONTENT | pending | GB | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-010 | Batch reference repair time per 1,000 documents | pending | CONTENT | pending | s | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-011 | Semantic merge time of a 16 MiB scene | pending | CONTENT | pending | ms | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CONTENT-012 | Shared DDC hit ratio on a warm incremental cook | pending | CONTENT | pending | % | Stage 1 per-system budget ticket for CONTENT |
| Z2-B-CORE-001 | S24 process RSS | adopted | CORE | 3.0 | GB | Z2-D-012 (plan text) |
| Z2-B-CORE-002 | Windows process RAM | adopted | CORE | 8.0 | GB | Z2-D-012 (plan text) |
| Z2-B-CORE-003 | Windows VRAM | adopted | CORE | 6.0 | GB | Z2-D-012 (plan text) |
| Z2-B-CORE-004 | General heap unattributed residual | pending | CORE | pending | MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-005 | Frame and graph arenas | pending | CORE | pending | MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-006 | Streaming double-residency peak | pending | CORE | pending | MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-007 | World-transition peak | pending | CORE | pending | MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-008 | Texture-swap peak | pending | CORE | pending | MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-009 | Memory pressure warning threshold | pending | CORE | pending | % of platform total | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-010 | Memory pressure critical threshold | pending | CORE | pending | % of platform total | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-011 | Memory allocation reserve fraction | pending | CORE | pending | % of platform total | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-012 | String intern table capacity | pending | CORE | pending (entries; MB of text) | entries, MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-CORE-013 | Log router ring bytes | pending | CORE | pending (at least 1 MiB, 1,048,576 bytes) | MB | Stage 1 per-system budget ticket for CORE |
| Z2-B-DEVT-001 | Profiler and telemetry buffers | pending | DEVT | pending | MB | Stage 1 per-system budget ticket for DEVT |
| Z2-B-DEVT-002 | On-screen profiler overhead CPU | pending | DEVT | pending | ms | Stage 1 per-system budget ticket for DEVT |
| Z2-B-DEVT-003 | Debug overlays and on-screen profiler GPU | pending | DEVT | pending | ms | Stage 1 per-system budget ticket for DEVT |
| Z2-B-ECS-001 | Component storage per world | pending | ECS | pending | MB | Stage 1 per-system budget ticket for ECS |
| Z2-B-ECS-002 | Sync-point command playback CPU per tick | pending | ECS | pending | ms | Stage 1 per-system budget ticket for ECS |
| Z2-B-ECS-003 | Spawn capacity per tick | pending | ECS | pending | entities | Stage 1 per-system budget ticket for ECS |
| Z2-B-ECS-004 | Structural command capacity per tick | pending | ECS | pending | commands | Stage 1 per-system budget ticket for ECS |
| Z2-B-EDIT-001 | Editor caches | pending | EDIT | pending | MB | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-002 | Script hot reload | pending | EDIT | pending | s | Stage 1 W-Editor row ticket for script hot reload |
| Z2-B-EDIT-003 | Shader blob reload | pending | EDIT | pending | s | Stage 1 W-Editor row ticket for shader blob reload |
| Z2-B-EDIT-004 | Asset reimport | pending | EDIT | pending | s | Stage 1 W-Editor row ticket for asset reimport |
| Z2-B-EDIT-005 | Scene open | pending | EDIT | pending | s | Stage 1 W-Editor row ticket for scene open |
| Z2-B-EDIT-006 | Play-in-editor start | pending | EDIT | pending | s | Stage 1 W-Editor row ticket for play-in-editor start |
| Z2-B-EDIT-007 | Walkthrough build a scene | pending | EDIT | pending | min | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-008 | Walkthrough author a prefab variant | pending | EDIT | pending | min | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-009 | Walkthrough iterate a script | pending | EDIT | pending | min | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-010 | Walkthrough reimport an asset | pending | EDIT | pending | min | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-011 | Terrain edit delta memory per undo step | pending | EDIT | pending | MB | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-012 | Terrain sculpt preview latency | pending | EDIT | pending | ms | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-013 | Editor frame CPU | pending | EDIT | pending | ms | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-014 | Editor projection apply CPU per 1,000 changes | pending | EDIT | pending | ms | Stage 1 per-system budget ticket for EDIT |
| Z2-B-EDIT-015 | Editor undo history memory | pending | EDIT | pending | MB | Stage 1 per-system budget ticket for EDIT |
| Z2-B-ENV-001 | Sky and atmosphere GPU | pending | ENV | pending | ms | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-002 | Environment update CPU | pending | ENV | pending | ms | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-003 | Environment extraction CPU | pending | ENV | pending | ms | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-004 | Water surface and underwater GPU | pending | ENV | pending | ms | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-005 | Detail foliage generation GPU | pending | ENV | pending | ms | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-006 | Environment resident CPU data | pending | ENV | pending | MB | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-007 | Placed foliage instances resident cap | pending | ENV | pending | instances | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-008 | Foliage activation catch-up frames | pending | ENV | pending | frames | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-009 | Cloud layer approximation error | pending | ENV | pending | FLIP mean | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-010 | Volumetric cloud approximation error | pending | ENV | pending | FLIP mean | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-011 | Underwater composite error | pending | ENV | pending | FLIP mean | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-012 | Ocean FFT to query-set height error | pending | ENV | pending | m | Stage 1 per-system budget ticket for ENV |
| Z2-B-ENV-013 | Environment render test tolerances | pending | ENV | pending (one FLIP mean and one per-channel threshold per golden capture) | FLIP mean, per-channel | Stage 1 per-system budget ticket for ENV |
| Z2-B-GAME-001 | Movement arbitration phase CPU | pending | GAME | pending | ms | Stage 1 per-system budget ticket for GAME |
| Z2-B-GAME-002 | Event delivery CPU | pending | GAME | pending | ms | Stage 1 per-system budget ticket for GAME |
| Z2-B-GAME-003 | Event queue capacity per tick | pending | GAME | pending | events | Stage 1 per-system budget ticket for GAME |
| Z2-B-GEOM-001 | Unified vertex and index pools | pending | GEOM | pending | MB | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-002 | Texture residency pool | pending | GEOM | pending | MB | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-003 | Streaming upload bytes per frame | pending | GEOM | pending | MB | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-004 | Compute skinning and morph targets GPU | pending | GEOM | pending | ms | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-005 | Procedural geometry regeneration GPU | pending | GEOM | pending | ms | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-006 | Deferred decals GPU | pending | GEOM | pending | ms | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-007 | Terrain geometry GPU | pending | GEOM | pending | ms | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-008 | Material constants memory | pending | GEOM | pending | MB | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GEOM-009 | Texture streaming request-to-resident latency | pending | GEOM | pending | frames | Stage 1 per-system budget ticket for GEOM |
| Z2-B-GRAPH-001 | CPU frame-graph schedule recompile | pending | GRAPH | pending | ms | Stage 1 per-system budget ticket for GRAPH |
| Z2-B-JOBS-001 | Scheduler overhead per frame | pending | JOBS | pending | ms | Stage 1 per-system budget ticket for JOBS |
| Z2-B-JOBS-002 | Bounded catch-up fixed steps per frame | pending | JOBS | pending | count | Stage 1 per-system budget ticket for JOBS |
| Z2-B-JOBS-003 | CPU critical-path allocation headroom | pending | JOBS | pending | % of frame-time p95 | Stage 1 per-system budget ticket for JOBS |
| Z2-B-JOBS-004 | Job system resident memory | pending | JOBS | pending | MB | Stage 1 per-system budget ticket for JOBS |
| Z2-B-JOBS-005 | Job queue capacities | pending | JOBS | pending (one value each for the worker deque, the injection queue, the affinity queues and the background queues, high and low; the tools-host queue of tool hosts is outside the row (design constant `uJOBS_DEFAULT_TOOL_BACKGROUND_CAPACITY`, 1,024)) | jobs | Stage 1 per-system budget ticket for JOBS |
| Z2-B-LIGHT-001 | Light and decal list build GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-002 | Shadow views total GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-003 | Light clustering GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-004 | Volumetric fog GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-005 | SSAO GTAO and denoise GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-006 | Screen-space shadows GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-007 | SSR trace denoise and composite GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-008 | Deferred lighting GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-009 | Visibility buffer lighting GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-010 | TAA or temporal upscaler GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-011 | Auto-exposure GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-012 | Depth of field GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-013 | Motion blur GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-014 | Bloom GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-015 | Tonemap and colour grading GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-016 | SSGI GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-017 | Probe capture and filter GPU | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-018 | Reflection probe and environment memory | pending | LIGHT | pending | MB | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-019 | Baked GI resident memory | pending | LIGHT | pending | MB | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-020 | LIGHT CPU on Flux.BuildGraph | pending | LIGHT | pending | ms | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-021 | Shadowed local lights per frame per tier | pending | LIGHT | pending (one value per quality tier) | lights | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-LIGHT-022 | SSGI error budget | pending | LIGHT | pending (one value each for energy, accuracy, stability and off-screen) | ×, % | Stage 1 per-system budget ticket for LIGHT |
| Z2-B-PACK-001 | AAB base module download size | pending | PACK | pending (Google Play's base-module download limit, pinned at Stage 6) | MB | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-002 | Windows install size OpenWorldSlice | pending | PACK | pending | GB | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-003 | Windows install size CinematicSlice | pending | PACK | pending | GB | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-004 | Windows install size Game2D | pending | PACK | pending | GB | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-005 | Play asset pack download size | pending | PACK | pending | MB | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-006 | Play asset packs total download size | pending | PACK | pending | MB | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-007 | Play asset pack count | pending | PACK | pending | count | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-008 | Patch download size per release | pending | PACK | pending | MB | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-009 | Package service resident memory | pending | PACK | pending | MB | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-010 | Asset pack verification throughput floor | pending | PACK | pending | MB/s | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-011 | Cold-boot update verification time | pending | PACK | pending (one value at the first start after the install, one at the second) | ms per GiB | Stage 1 per-system budget ticket for PACK |
| Z2-B-PHYS-001 | Physics world memory | pending | PHYS | pending | MB | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-002 | Physics step CPU | pending | PHYS | pending | ms | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-003 | Physics body capacity per world | pending | PHYS | pending (design default 65,536) | bodies | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-004 | Physics constraint capacity per world | pending | PHYS | pending (design default 16,384) | constraints | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-005 | Physics contact records per tick | pending | PHYS | pending (design default 8,192) | records | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PHYS-006 | Active ragdolls per world | pending | PHYS | pending (design default 32) | ragdolls | Stage 1 per-system budget ticket for PHYS |
| Z2-B-PLAT-001 | Input phase CPU | pending | PLAT | pending | ms | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-002 | Platform pump CPU | pending | PLAT | pending | ms | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-003 | Async I/O critical read latency | pending | PLAT | pending | ms | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-004 | Resume-state write duration | pending | PLAT | pending | ms | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-005 | Configuration and bindings build time at startup | pending | PLAT | pending | ms | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-006 | Platform and input memory | pending | PLAT | pending | MB | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PROC-001 | Lane checkout storage per Z2 lane | pending | PROC | pending | GB | Stage 0 PROC lane ticket (zagent lane add Z2) |
| Z2-B-PROC-002 | Warm phase cap measure | adopted | PROC | 30 | min | Z2-D-028 (plan text) |
| Z2-B-PROC-003 | Warm phase cap verify | adopted | PROC | 30 | min | Z2-D-028 (plan text) |
| Z2-B-PROC-004 | Tree-change retry cap | adopted | PROC | 30 | min | Z2-D-028 (plan text) |
| Z2-B-PROC-005 | Cold phase cap | pending | PROC | pending | min | Stage 0 PROC ticket for the tick_gates external-build mode and phaseTimeoutMinutes |
| Z2-B-PROC-006 | Acceptance phase timeout | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-007 | Step timeout Configure | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-008 | Step timeout Build win-debug | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-009 | Step timeout Build win-release | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-010 | Step timeout Build android-arm64 | pending | PROC | pending | min | Stage 2 PROC ticket activating Android build and ApkCheck |
| Z2-B-PROC-011 | Step timeout Test | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-012 | Step timeout SourceLayout | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-013 | Step timeout CompileCoverage | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-014 | Step timeout Libgraph | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-015 | Step timeout Complexity | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-016 | Step timeout ForbiddenCheck | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-017 | Step timeout Format | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-018 | Step timeout HeaderSelfContained | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-019 | Step timeout ApiContractCheck | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-020 | Step timeout TestSeparationCheck | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-021 | Step timeout ReviewSignals | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-022 | Step timeout Coverage | pending | PROC | pending | min | Stage 0 PROC ticket for the production coverage checker |
| Z2-B-PROC-023 | Step timeout ApkCheck | pending | PROC | pending | min | Stage 2 PROC ticket activating Android build and ApkCheck |
| Z2-B-PROC-024 | Step timeout ShaderChecks | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-025 | Step timeout Sanitizers win-asan | pending | PROC | pending | min | Stage 2 PROC ticket activating Sanitizers |
| Z2-B-PROC-026 | Step timeout Sanitizers linux-tsan | pending | PROC | pending | min | Stage 2 PROC ticket activating Sanitizers |
| Z2-B-PROC-027 | Step timeout BackendParity | pending | PROC | pending | min | Stage 3 PROC ticket activating BackendParity |
| Z2-B-PROC-028 | Step timeout IntegrationSmoke | pending | PROC | pending | min | Stage 3 PROC ticket adding IntegrationSmoke to Run-Committed.ps1 |
| Z2-B-PROC-029 | Step timeout PathParity | pending | PROC | pending | min | Stage 5 PROC ticket activating PathParity |
| Z2-B-PROC-030 | Integration smoke frame count | adopted | PROC | 300 | frames | derived from Z2-D-031 |
| Z2-B-PROC-031 | Step timeout EcsConformance | pending | PROC | pending | min | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-RHI-001 | Persistent render targets and view histories | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-002 | Bindless descriptor heaps | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-003 | GPU residency budget per heap | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-004 | Render graph build and compile CPU | pending | RHI | pending | ms | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-005 | Command recording and submit CPU | pending | RHI | pending | ms | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-006 | Upload ring hard cap | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-007 | Upload ring stall frames | pending | RHI | pending | frames | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-008 | Frames in flight | adopted | RHI | 2 to 3, configurable | frames | derived from Z2-D-050 |
| Z2-B-RHI-009 | Bindless heap capacity per kind | pending | RHI | pending (one value each for sampled textures, storage textures, buffers and samplers) | descriptors | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-010 | Transient memory peak | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-011 | GPU readback pool size | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RHI-012 | GPU-scene CPU pending store | pending | RHI | pending | MB | Stage 1 per-system budget ticket for RHI |
| Z2-B-RPROD-001 | GPU scene tables | pending | RPROD | pending | MB | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-002 | PSO cache and pipeline objects | pending | RPROD | pending | MB | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-003 | Render extraction CPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-004 | GPU-scene scatter upload GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-005 | Instance culling phase 1 GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-006 | Depth velocity normal prepass GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-007 | HiZ pyramid build GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-008 | Instance culling phase 2 GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-009 | Forward opaque GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-010 | Transparent forward GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-011 | Per-object light list build GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-012 | Forward per-object opaque GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-013 | Per-object light cap | pending | RPROD | pending | lights per object | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-014 | G-buffer pass GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-015 | Meshlet culling GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-016 | Visibility raster GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-017 | Material classification GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-018 | Material resolve GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-019 | Velocity reconstruction GPU | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-020 | Frame pacing present-interval deviation | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-021 | CPU render lag | adopted | RPROD | 1 | frames | Z2-D-050 (plan text) |
| Z2-B-RPROD-022 | View count per frame per quality tier | pending | RPROD | pending (one value per quality tier of QualityMatrix.md) | views | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-023 | Texture views and captures per frame | pending | RPROD | pending (one value per quality tier) | views | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-024 | Shader permutation limit per game | pending | RPROD | pending | permutations | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-025 | Missing-PSO events per qualification run | adopted | RPROD | 0 | events | derived from Z2-D-025 |
| Z2-B-RPROD-026 | HiZ mid-frame depth store and reload bandwidth | pending | RPROD | pending | MB per frame | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-027 | Input-to-photon latency | pending | RPROD | pending | ms | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-028 | GPU frame allocation headroom | pending | RPROD | pending | % of frame-time p95 | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-029 | D24 depth-precision tolerance | pending | RPROD | pending (below the 0.477 m ceiling) | m | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-030 | On-disk pipeline-cache size | pending | RPROD | pending | MB | Stage 1 per-system budget ticket for RPROD |
| Z2-B-RPROD-031 | Per-scene parity tolerances | pending | RPROD | pending (one FLIP mean and one per-channel threshold per parity scene, per comparison: backend and path) | FLIP mean, per-channel | Stage 1 per-system budget ticket for RPROD |
| Z2-B-SAVE-001 | Save serialization buffers | pending | SAVE | pending | MB | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-002 | Save-snapshot point CPU per requested save | pending | SAVE | pending | ms | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-003 | Save write duration per save | pending | SAVE | pending | ms | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-004 | Save file size per slot | pending | SAVE | pending | MB | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-005 | Save load duration per save | pending | SAVE | pending | ms | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SAVE-006 | World restore duration per 10,000 entities | pending | SAVE | pending | ms | Stage 1 per-system budget ticket for SAVE |
| Z2-B-SCENE-001 | Scene and prefab runtime data | pending | SCENE | pending | MB | Stage 1 per-system budget ticket for SCENE |
| Z2-B-SCENE-002 | Transform propagation CPU | pending | SCENE | pending | ms | Stage 1 per-system budget ticket for SCENE |
| Z2-B-SCENE-003 | Scene activation slice per frame prologue | pending | SCENE | pending | ms | Stage 1 per-system budget ticket for SCENE |
| Z2-B-SCRIPT-001 | Behaviour callback safepoint budget | pending | SCRIPT | pending | safepoints | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-002 | Chunk-system safepoint budget per entity | pending | SCRIPT | pending | safepoints | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-003 | Script wall-clock watchdog per invocation | pending | SCRIPT | pending | ms | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-004 | Early scripts phase CPU | pending | SCRIPT | pending | ms | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-005 | Late scripts phase CPU | pending | SCRIPT | pending | ms | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-006 | Chunk-system CPU per tick | pending | SCRIPT | pending | ms | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-007 | Per-world GC step budget | pending | SCRIPT | pending | KB of GC work per VM per tick | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-008 | Luau GC time per frame | pending | SCRIPT | pending | ms | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-009 | World main VM memory cap | pending | SCRIPT | pending | MB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-010 | Per-worker chunk-system VM memory cap | pending | SCRIPT | pending | MB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-011 | VM memory times workers per world | pending | SCRIPT | pending | MB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-012 | Snapshot memory per entity | pending | SCRIPT | pending | KB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-013 | Snapshot memory per tick | pending | SCRIPT | pending | MB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-014 | Checkpoint serialization CPU per tick | pending | SCRIPT | pending | ms | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-015 | Checkpoint reserved memory per behaviour instance | pending | SCRIPT | pending | KB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-016 | Container storage capacity per tick | pending | SCRIPT | pending | KB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-017 | Script GC tuning parameters | pending | SCRIPT | pending (goal %, step multiplier, step size in KB) | %, ×, KB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-018 | Recovery safepoint budget per OnRecover or Migrate step | pending | SCRIPT | pending | safepoints | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-019 | Module-load safepoint budget per module scope | pending | SCRIPT | pending | safepoints | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-020 | Script safepoints per world per tick | pending | SCRIPT | pending | safepoints | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-021 | Checkpoint bytes per tick | pending | SCRIPT | pending | KB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-022 | String-intern capacity per world session | pending | SCRIPT | pending (entries; KB) | entries, KB | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-023 | Audio trigger capacity per tick | pending | SCRIPT | pending | triggers | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-024 | VFX trigger capacity per tick | pending | SCRIPT | pending | triggers | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-025 | Save-request capacity per tick | pending | SCRIPT | pending | requests | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SCRIPT-026 | Per-source asynchronous request capacity in flight | pending | SCRIPT | pending (one value each for script path requests, streaming loads, deferred physics queries and online calls) | requests | Stage 1 per-system budget ticket for SCRIPT |
| Z2-B-SHIP-001 | Frame time p95 | adopted | SHIP | 16.7 | ms | Z2-D-012 (plan text) |
| Z2-B-SHIP-002 | Frame time p99 | adopted | SHIP | 20.0 | ms | Z2-D-012 (plan text) |
| Z2-B-SHIP-003 | Initial-load exclusion window | adopted | SHIP | 10, once per run | s | Z2-D-012 (plan text) |
| Z2-B-SHIP-004 | Minimum measured run duration | adopted | SHIP | ≥ 120 | s | Z2-D-012 (plan text) |
| Z2-B-SHIP-005 | Minimum measured run frame count | adopted | SHIP | ≥ 7,200 | frames | Z2-D-012 (plan text) |
| Z2-B-SHIP-006 | Sustained floor | adopted | SHIP | ≥ 45, 1-s rolling average | fps | Z2-D-012 (plan text) |
| Z2-B-SHIP-007 | Sustained run duration | adopted | SHIP | ≥ 30 | min | Z2-D-012 (plan text) |
| Z2-B-SHIP-008 | Sustained run ambient temperature | adopted | SHIP | 25 | °C | Z2-D-012 (plan text) |
| Z2-B-SHIP-009 | Sustained run starting battery | adopted | SHIP | 80 | % | Z2-D-012 (plan text) |
| Z2-B-SHIP-010 | Sustained run power state | adopted | SHIP | unplugged | — | Z2-D-012 (plan text) |
| Z2-B-SHIP-011 | Dynamic-resolution floor | adopted | SHIP | ≥ 75 (of the Win 2560×1440 and S24 2340×1080 outputs) | % of output | Z2-D-012 (plan text) |
| Z2-B-SHIP-012 | Hitch threshold | adopted | SHIP | 33.3 | ms | Z2-D-012 (plan text) |
| Z2-B-SHIP-013 | Cold boot Windows | adopted | SHIP | 10 | s | Z2-D-012 (plan text) |
| Z2-B-SHIP-014 | Cold boot S24 | adopted | SHIP | 15 | s | Z2-D-012 (plan text) |
| Z2-B-SHIP-015 | Hitch count warm caches | pending | SHIP | pending (one value per workload) | frames | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-016 | Maximum frame time warm caches | pending | SHIP | pending (one value per workload) | ms | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-017 | Hitch count cold caches | pending | SHIP | pending (one value per workload) | frames | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-018 | Maximum frame time cold caches | pending | SHIP | pending (one value per workload) | ms | Stage 3 per-system budget ticket for SHIP |
| Z2-B-SHIP-019 | Open crash or blocker defects per slice | adopted | SHIP | 0 | defects | Z2-D-012 (plan text) |
| Z2-B-SHIP-020 | Severity-weighted open-defect cap OpenWorldSlice | pending | SHIP | pending | weighted defects | Stage 1 flagship acceptance ticket for OpenWorldSlice |
| Z2-B-SHIP-021 | Severity-weighted open-defect cap CinematicSlice | pending | SHIP | pending | weighted defects | Stage 1 flagship acceptance ticket for CinematicSlice |
| Z2-B-SHIP-022 | Severity-weighted open-defect cap Game2D | pending | SHIP | pending | weighted defects | Stage 1 flagship acceptance ticket for Game2D |
| Z2-B-SHIP-023 | Playthrough script duration | adopted | SHIP | 20 to 30 | min | Z2-D-012 (plan text) |
| Z2-B-STREAM-001 | Streaming CPU-side resident and in-flight cell data | pending | STREAM | pending | MB | Stage 1 per-system budget ticket for STREAM |
| Z2-B-STREAM-002 | Streaming CPU per frame | pending | STREAM | pending | ms | Stage 1 per-system budget ticket for STREAM |
| Z2-B-STREAM-003 | Concurrent cell loads in flight | pending | STREAM | pending | cells | Stage 1 per-system budget ticket for STREAM |
| Z2-B-STREAM-004 | Teleport-to-resident time | pending | STREAM | pending | s | Stage 1 per-system budget ticket for STREAM |
| Z2-B-STREAM-005 | Rapid-traversal residency miss frames | pending | STREAM | pending | frames | Stage 1 per-system budget ticket for STREAM |
| Z2-B-STREAM-006 | Cell activation entities per tick | pending | STREAM | pending | entities | Stage 1 per-system budget ticket for STREAM |
| Z2-B-TWOD-001 | Sprite batching and 2D preparation CPU | pending | TWOD | pending | ms | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-002 | 2D sprite passes GPU | pending | TWOD | pending | ms | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-003 | 2D canvas composite GPU | pending | TWOD | pending | ms | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-004 | 2D physics step CPU | pending | TWOD | pending | ms | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-005 | 2D physics world memory | pending | TWOD | pending | MB | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-006 | 2D animation CPU | pending | TWOD | pending | ms | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-007 | TWOD module resident memory | pending | TWOD | pending | MB | Stage 1 per-system budget ticket for TWOD |
| Z2-B-TWOD-008 | 2D renderer memory | pending | TWOD | pending | MB | Stage 1 per-system budget ticket for TWOD |
| Z2-B-UI-001 | UI, fonts and glyph atlases | pending | UI | pending | MB | Stage 1 per-system budget ticket for UI |
| Z2-B-UI-002 | UI update and layout CPU | pending | UI | pending | ms | Stage 1 per-system budget ticket for UI |
| Z2-B-UI-003 | UI composite GPU | pending | UI | pending | ms | Stage 1 per-system budget ticket for UI |
| Z2-B-VFX-001 | Particle pools and CPU particle data | pending | VFX | pending | MB | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-002 | CPU particle update | pending | VFX | pending | ms | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-003 | Particle simulation GPU | pending | VFX | pending | ms | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-004 | Particles and ribbons draw GPU | pending | VFX | pending | ms | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-005 | VFX lifecycle CPU | pending | VFX | pending | ms | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-006 | VFX trigger queue capacity per world | pending | VFX | pending | triggers | Stage 1 per-system budget ticket for VFX |
| Z2-B-WORLD-001 | World partition import time | pending | WORLD | pending | s | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-002 | World partition import memory | pending | WORLD | pending | MB | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-003 | Lighting bake time per cell | pending | WORLD | pending | s | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-004 | Lighting bake tracer memory | pending | WORLD | pending | MB | Stage 1 per-system budget ticket for WORLD |
| Z2-B-WORLD-005 | Edit-world region open latency | pending | WORLD | pending | ms | Stage 1 per-system budget ticket for WORLD |

### Producer tickets

- [Z2-N-BUDGET-462] Every distinct producer named by a pending row is listed below with the rows it allocates, and the `needs-human` ruling ticket that allocates those rows carries exactly this producer name as its title prefix; its harness ticket, where "Producer ticket shape" gives it one, carries the same name with `ticket` replaced by `harness`.

| Producer ticket | Stage | Harness half | Blocked by | Rows |
|---|---|---|---|---|
| Stage 0 PROC lane ticket (zagent lane add Z2) | 0 | — | — | Z2-B-PROC-001 |
| Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 | 0 | — | — | Z2-B-PROC-006, Z2-B-PROC-007, Z2-B-PROC-008, Z2-B-PROC-009, Z2-B-PROC-011, Z2-B-PROC-012, Z2-B-PROC-013, Z2-B-PROC-014, Z2-B-PROC-015, Z2-B-PROC-016, Z2-B-PROC-017, Z2-B-PROC-018, Z2-B-PROC-019, Z2-B-PROC-020, Z2-B-PROC-021, Z2-B-PROC-024, Z2-B-PROC-031 |
| Stage 0 PROC ticket for the production coverage checker | 0 | — | — | Z2-B-PROC-022 |
| Stage 0 PROC ticket for the tick_gates external-build mode and phaseTimeoutMinutes | 0 | — | — | Z2-B-PROC-005 |
| Stage 1 W-Editor row ticket for asset reimport | 1 | — | — | Z2-B-EDIT-004 |
| Stage 1 W-Editor row ticket for play-in-editor start | 1 | — | — | Z2-B-EDIT-006 |
| Stage 1 W-Editor row ticket for scene open | 1 | — | — | Z2-B-EDIT-005 |
| Stage 1 W-Editor row ticket for script hot reload | 1 | — | — | Z2-B-EDIT-002 |
| Stage 1 W-Editor row ticket for shader blob reload | 1 | — | — | Z2-B-EDIT-003 |
| Stage 1 flagship acceptance ticket for CinematicSlice | 1 | — | — | Z2-B-SHIP-021 |
| Stage 1 flagship acceptance ticket for Game2D | 1 | — | — | Z2-B-SHIP-022 |
| Stage 1 flagship acceptance ticket for OpenWorldSlice | 1 | — | — | Z2-B-SHIP-020 |
| Stage 1 per-system budget ticket for ADVGFX | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-ADVGFX-001, Z2-B-ADVGFX-002, Z2-B-ADVGFX-003, Z2-B-ADVGFX-004, Z2-B-ADVGFX-005, Z2-B-ADVGFX-006, Z2-B-ADVGFX-007, Z2-B-ADVGFX-008 |
| Stage 1 per-system budget ticket for AI | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003, Z2-B-AI-004, Z2-B-AI-005, Z2-B-AI-006, Z2-B-AI-007, Z2-B-AI-008, Z2-B-AI-009 |
| Stage 1 per-system budget ticket for ANIM | 1 | yes | Stage 1 per-system budget harness for ANIM (itself blocked by Stage 1 workload scale ticket for W-Characters); Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-ANIM-001, Z2-B-ANIM-002, Z2-B-ANIM-003, Z2-B-ANIM-009 |
| Stage 1 per-system budget ticket for ASSET | 1 | yes | Stage 1 per-system budget harness for ASSET; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-ASSET-001, Z2-B-ASSET-002, Z2-B-ASSET-003, Z2-B-ASSET-004, Z2-B-ASSET-005, Z2-B-ASSET-006, Z2-B-ASSET-007, Z2-B-ASSET-008, Z2-B-ASSET-009, Z2-B-ASSET-010, Z2-B-ASSET-011 |
| Stage 1 per-system budget ticket for AUDIO | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-AUDIO-001, Z2-B-AUDIO-002, Z2-B-AUDIO-003, Z2-B-AUDIO-004, Z2-B-AUDIO-005, Z2-B-AUDIO-006, Z2-B-AUDIO-007, Z2-B-AUDIO-008, Z2-B-AUDIO-009, Z2-B-AUDIO-010, Z2-B-AUDIO-011, Z2-B-AUDIO-012 |
| Stage 1 per-system budget ticket for CHAR | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-CHAR-001, Z2-B-CHAR-002, Z2-B-CHAR-003 |
| Stage 1 per-system budget ticket for CINE | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-CINE-001, Z2-B-CINE-002, Z2-B-CINE-003, Z2-B-CINE-004 |
| Stage 1 per-system budget ticket for COMPRESS | 1 | yes | Stage 1 per-system budget harness for COMPRESS; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-COMPRESS-001, Z2-B-COMPRESS-002 |
| Stage 1 per-system budget ticket for CONTENT | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-CONTENT-001, Z2-B-CONTENT-002, Z2-B-CONTENT-003, Z2-B-CONTENT-004, Z2-B-CONTENT-005, Z2-B-CONTENT-006, Z2-B-CONTENT-007, Z2-B-CONTENT-008, Z2-B-CONTENT-009, Z2-B-CONTENT-010, Z2-B-CONTENT-011, Z2-B-CONTENT-012 |
| Stage 1 per-system budget ticket for CORE | 1 | yes | Stage 1 per-system budget harness for CORE | Z2-B-CORE-004, Z2-B-CORE-005, Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008, Z2-B-CORE-009, Z2-B-CORE-010, Z2-B-CORE-011, Z2-B-CORE-012, Z2-B-CORE-013 |
| Stage 1 per-system budget ticket for DEVT | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-DEVT-001, Z2-B-DEVT-002, Z2-B-DEVT-003 |
| Stage 1 per-system budget ticket for ECS | 1 | yes | Stage 1 per-system budget harness for ECS (itself blocked by Stage 1 workload scale ticket for W-Dense); Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-ECS-001, Z2-B-ECS-002, Z2-B-ECS-003, Z2-B-ECS-004 |
| Stage 1 per-system budget ticket for EDIT | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-EDIT-001, Z2-B-EDIT-007, Z2-B-EDIT-008, Z2-B-EDIT-009, Z2-B-EDIT-010, Z2-B-EDIT-011, Z2-B-EDIT-012, Z2-B-EDIT-013, Z2-B-EDIT-014, Z2-B-EDIT-015, Z2-B-ANIM-010 |
| Stage 1 per-system budget ticket for ENV | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-ENV-001, Z2-B-ENV-002, Z2-B-ENV-003, Z2-B-ENV-004, Z2-B-ENV-005, Z2-B-ENV-006, Z2-B-ENV-007, Z2-B-ENV-008, Z2-B-ENV-009, Z2-B-ENV-010, Z2-B-ENV-011, Z2-B-ENV-012, Z2-B-ENV-013 |
| Stage 1 per-system budget ticket for GAME | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD; Stage 1 workload scale ticket for W-Dense; Stage 1 workload scale ticket for W-Characters; Stage 1 flagship acceptance ticket for OpenWorldSlice | Z2-B-GAME-001, Z2-B-GAME-002, Z2-B-GAME-003 |
| Stage 1 per-system budget ticket for GEOM | 1 | yes | Stage 1 per-system budget harness for GEOM; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-GEOM-001, Z2-B-GEOM-002, Z2-B-GEOM-003, Z2-B-GEOM-004, Z2-B-GEOM-005, Z2-B-GEOM-006, Z2-B-GEOM-007, Z2-B-GEOM-008, Z2-B-GEOM-009 |
| Stage 1 per-system budget ticket for GRAPH | 1 | yes | Stage 1 per-system budget harness for GRAPH; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-GRAPH-001 |
| Stage 1 per-system budget ticket for JOBS | 1 | yes | Stage 1 per-system budget harness for JOBS | Z2-B-JOBS-001, Z2-B-JOBS-002, Z2-B-JOBS-003, Z2-B-JOBS-004, Z2-B-JOBS-005 |
| Stage 1 per-system budget ticket for LIGHT | 1 | yes | Stage 1 per-system budget harness for LIGHT; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-LIGHT-001, Z2-B-LIGHT-002, Z2-B-LIGHT-003, Z2-B-LIGHT-004, Z2-B-LIGHT-005, Z2-B-LIGHT-006, Z2-B-LIGHT-007, Z2-B-LIGHT-008, Z2-B-LIGHT-009, Z2-B-LIGHT-010, Z2-B-LIGHT-011, Z2-B-LIGHT-012, Z2-B-LIGHT-013, Z2-B-LIGHT-014, Z2-B-LIGHT-015, Z2-B-LIGHT-016, Z2-B-LIGHT-017, Z2-B-LIGHT-018, Z2-B-LIGHT-019, Z2-B-LIGHT-020, Z2-B-LIGHT-021, Z2-B-LIGHT-022 |
| Stage 1 per-system budget ticket for PACK | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-PACK-002, Z2-B-PACK-003, Z2-B-PACK-004, Z2-B-PACK-008, Z2-B-PACK-009, Z2-B-PACK-010, Z2-B-PACK-011 |
| Stage 1 per-system budget ticket for PHYS | 1 | yes | Stage 1 per-system budget harness for PHYS; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-PHYS-001, Z2-B-PHYS-002, Z2-B-PHYS-003, Z2-B-PHYS-004, Z2-B-PHYS-005, Z2-B-PHYS-006 |
| Stage 1 per-system budget ticket for PLAT | 1 | yes | Stage 1 per-system budget harness for PLAT; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-PLAT-001, Z2-B-PLAT-002, Z2-B-PLAT-003, Z2-B-PLAT-004, Z2-B-PLAT-005, Z2-B-PLAT-006 |
| Stage 1 per-system budget ticket for RHI | 1 | yes | Stage 1 per-system budget harness for RHI; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-RHI-001, Z2-B-RHI-002, Z2-B-RHI-003, Z2-B-RHI-004, Z2-B-RHI-005, Z2-B-RHI-006, Z2-B-RHI-007, Z2-B-RHI-009, Z2-B-RHI-010, Z2-B-RHI-011, Z2-B-RHI-012 |
| Stage 1 per-system budget ticket for RPROD | 1 | yes | Stage 1 per-system budget harness for RPROD; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS | Z2-B-RPROD-001, Z2-B-RPROD-002, Z2-B-RPROD-003, Z2-B-RPROD-004, Z2-B-RPROD-005, Z2-B-RPROD-006, Z2-B-RPROD-007, Z2-B-RPROD-008, Z2-B-RPROD-009, Z2-B-RPROD-010, Z2-B-RPROD-011, Z2-B-RPROD-012, Z2-B-RPROD-013, Z2-B-RPROD-014, Z2-B-RPROD-015, Z2-B-RPROD-016, Z2-B-RPROD-017, Z2-B-RPROD-018, Z2-B-RPROD-019, Z2-B-RPROD-020, Z2-B-RPROD-022, Z2-B-RPROD-023, Z2-B-RPROD-024, Z2-B-RPROD-026, Z2-B-RPROD-027, Z2-B-RPROD-028, Z2-B-RPROD-029, Z2-B-RPROD-030, Z2-B-RPROD-031 |
| Stage 1 per-system budget ticket for SAVE | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-SAVE-001, Z2-B-SAVE-002, Z2-B-SAVE-003, Z2-B-SAVE-004, Z2-B-SAVE-005, Z2-B-SAVE-006 |
| Stage 1 per-system budget ticket for SCENE | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-SCENE-001, Z2-B-SCENE-002, Z2-B-SCENE-003 |
| Stage 1 per-system budget ticket for SCRIPT | 1 | yes | Stage 1 per-system budget harness for SCRIPT; Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD; the D-SCRIPT-TXN refinement (open, Stage 1) ruling | Z2-B-SCRIPT-001, Z2-B-SCRIPT-002, Z2-B-SCRIPT-003, Z2-B-SCRIPT-004, Z2-B-SCRIPT-005, Z2-B-SCRIPT-006, Z2-B-SCRIPT-007, Z2-B-SCRIPT-008, Z2-B-SCRIPT-009, Z2-B-SCRIPT-010, Z2-B-SCRIPT-011, Z2-B-SCRIPT-012, Z2-B-SCRIPT-013, Z2-B-SCRIPT-014, Z2-B-SCRIPT-015, Z2-B-SCRIPT-016, Z2-B-SCRIPT-017, Z2-B-SCRIPT-018, Z2-B-SCRIPT-019, Z2-B-SCRIPT-020, Z2-B-SCRIPT-021, Z2-B-SCRIPT-022, Z2-B-SCRIPT-023, Z2-B-SCRIPT-024, Z2-B-SCRIPT-025, Z2-B-SCRIPT-026 |
| Stage 1 per-system budget ticket for STREAM | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD; Stage 1 per-system budget ticket for ECS | Z2-B-STREAM-001, Z2-B-STREAM-002, Z2-B-STREAM-003, Z2-B-STREAM-004, Z2-B-STREAM-005, Z2-B-STREAM-006 |
| Stage 1 per-system budget ticket for TWOD | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-TWOD-001, Z2-B-TWOD-002, Z2-B-TWOD-003, Z2-B-TWOD-004, Z2-B-TWOD-005, Z2-B-TWOD-006, Z2-B-TWOD-007, Z2-B-TWOD-008 |
| Stage 1 per-system budget ticket for UI | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-UI-001, Z2-B-UI-002, Z2-B-UI-003 |
| Stage 1 per-system budget ticket for VFX | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD; Stage 1 per-system budget ticket for SCRIPT | Z2-B-VFX-001, Z2-B-VFX-002, Z2-B-VFX-003, Z2-B-VFX-004, Z2-B-VFX-005, Z2-B-VFX-006 |
| Stage 1 per-system budget ticket for WORLD | 1 | no | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD | Z2-B-WORLD-001, Z2-B-WORLD-002, Z2-B-WORLD-003, Z2-B-WORLD-004, Z2-B-WORLD-005 |
| Stage 2 PROC ticket activating Android build and ApkCheck | 2 | — | — | Z2-B-PROC-010, Z2-B-PROC-023 |
| Stage 2 PROC ticket activating Sanitizers | 2 | — | — | Z2-B-PROC-025, Z2-B-PROC-026 |
| Stage 3 PROC ticket activating BackendParity | 3 | — | — | Z2-B-PROC-027 |
| Stage 3 PROC ticket adding IntegrationSmoke to Run-Committed.ps1 | 3 | — | — | Z2-B-PROC-028 |
| Stage 3 per-system budget ticket for SHIP | 3 | no (DEVT's `Zenith2/Benchmarks/Run-Workload.ps1` is the harness) | Stage 1 per-system budget ticket for CORE; Stage 1 per-system budget ticket for JOBS; Stage 1 per-system budget ticket for RPROD; DEVT's `Run-Workload.ps1` tickets; SHIP's `RowSources.json` entries ticket | Z2-B-SHIP-015, Z2-B-SHIP-016, Z2-B-SHIP-017, Z2-B-SHIP-018 |
| ANIM Stage 4 budget ruling ticket | 4 | no | ANIM's update-rate budgeting, clip streaming and clip compression tickets | Z2-B-ANIM-004, Z2-B-ANIM-005, Z2-B-ANIM-006, Z2-B-ANIM-007, Z2-B-ANIM-008 |
| Stage 5 PROC ticket activating PathParity | 5 | — | — | Z2-B-PROC-029 |
| Stage 6 PACK ticket pinning the Play base-module limit | 6 | — | — | Z2-B-PACK-001, Z2-B-PACK-005, Z2-B-PACK-006, Z2-B-PACK-007 |
