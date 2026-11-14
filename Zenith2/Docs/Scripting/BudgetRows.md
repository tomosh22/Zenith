# Zenith2 Scripting — budget rows

Companion to `Zenith2/Docs/Scripting.md` (its "Budgets safepoints and the watchdog" section names every budget row by a descriptive name; this file maps each name to the row of `Zenith2/Docs/Budgets.md`). Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Budget rows used

Each row below is named in prose by its descriptive name; the table gives the row as `Zenith2/Docs/Budgets.md` names it and the heading it lives under. Every row's value is set by the Stage 1 per-system budget ticket for SCRIPT (or its area's ticket); the breach code of each row is the one `Zenith2/Docs/Budgets.md` gives it.

| Descriptive name in this document | Budgets row (ID, name) | Budgets heading | Enforced by |
|---|---|---|---|
| the main VM memory cap row | Z2-B-SCRIPT-009 "World main VM memory cap" | "Scripting rows" | `Zenith_ScriptAlloc`, fatal (`BR-FATAL`) |
| the worker VM memory cap row | Z2-B-SCRIPT-010 "Per-worker chunk-system VM memory cap" | "Scripting rows" | `Zenith_ScriptAlloc`, fatal (`BR-FATAL`) |
| the worker VM memory total row | Z2-B-SCRIPT-011 "VM memory times workers per world" | "Scripting rows" | world creation refuses a configuration above it (`BR-REFUSE`) |
| the script GC tuning row | Z2-B-SCRIPT-017 "Script GC tuning parameters" | "Scripting rows" | VM creation |
| the script GC step row | Z2-B-SCRIPT-007 "Per-world GC step budget" | "Scripting rows" | `StepGC` calls |
| the Luau GC time row | Z2-B-SCRIPT-008 "Luau GC time per frame" | "Scripting rows" | the on-screen profiler and workload runs |
| the chunk-system per-entity safepoint row | Z2-B-SCRIPT-002 "Chunk-system safepoint budget per entity" | "Scripting rows" | interrupt callback |
| the behaviour per-invocation safepoint row | Z2-B-SCRIPT-001 "Behaviour callback safepoint budget" | "Scripting rows" | interrupt callback |
| the recovery safepoint row | Z2-B-SCRIPT-018 "Recovery safepoint budget per OnRecover or Migrate step" | "Scripting rows" | interrupt callback |
| the module-load safepoint row | Z2-B-SCRIPT-019 "Module-load safepoint budget per module scope" | "Scripting rows" | interrupt callback |
| the script safepoints per tick row | Z2-B-SCRIPT-020 "Script safepoints per world per tick" | "Scripting rows" | reported (`BR-REPORT`) |
| the script CPU time row | Z2-B-SCRIPT-004 "Early scripts phase CPU", Z2-B-SCRIPT-005 "Late scripts phase CPU", Z2-B-SCRIPT-006 "Chunk-system CPU per tick" | "Scripting rows" | the on-screen profiler and workload runs |
| the script watchdog row | Z2-B-SCRIPT-003 "Script wall-clock watchdog per invocation" | "Scripting rows" | watchdog, fatal |
| the per-entity snapshot cap row | Z2-B-SCRIPT-012 "Snapshot memory per entity" | "Scripting rows" | `SNAPSHOT_CAP` error |
| the per-tick snapshot total row | Z2-B-SCRIPT-013 "Snapshot memory per tick" | "Scripting rows" | reported (`BR-REPORT`) |
| the checkpoint cost row | Z2-B-SCRIPT-014 "Checkpoint serialization CPU per tick" plus Z2-B-SCRIPT-021 "Checkpoint bytes per tick" | "Scripting rows" | reported |
| the checkpoint slot size row | Z2-B-SCRIPT-015 "Checkpoint reserved memory per behaviour instance" | "Scripting rows" | two enforcement points: a load error when a declared schema's maximum exceeds it; `SCRIPT_ERROR_KIND_CHECKPOINT` (the `BR-LUAU` breach Budgets gives the row) when a payload at run time exceeds the slot |
| the command-queue capacity row | Z2-B-ECS-004 "Structural command capacity per tick" | "Per-system rows" | merge-time rejection |
| the event-queue capacity row | Z2-B-GAME-003 "Event queue capacity per tick" | "Per-system rows" | merge-time rejection |
| the container storage capacity row | Z2-B-SCRIPT-016 "Container storage capacity per tick" | "Scripting rows" | merge-time rejection |
| the string-intern capacity row | Z2-B-SCRIPT-022 "String-intern capacity per world session" | "Scripting rows" | merge-time rejection |
| the audio and VFX trigger capacity rows | Z2-B-SCRIPT-023 "Audio trigger capacity per tick", Z2-B-SCRIPT-024 "VFX trigger capacity per tick" | "Scripting rows" | merge-time rejection |
| the save-request capacity row | Z2-B-SCRIPT-025 "Save-request capacity per tick" | "Scripting rows" | merge-time rejection |
| the per-source async capacity row | Z2-B-SCRIPT-026 "Per-source asynchronous request capacity in flight" (script path requests, streaming loads, deferred physics queries, online calls) | "Scripting rows" | merge-time rejection |
| the script hot-reload row | Z2-B-EDIT-002 "Script hot reload" | "Editor iteration rows" | W-Editor runs |

- [Z2-N-SCRIPT-921] Every descriptive budget-row name used in the Scripting document family maps to exactly one row of this table, and a row this family enforces behaves as the breach code `Zenith2/Docs/Budgets.md` → "Scripting rows" (or the heading named in the table) gives it.
