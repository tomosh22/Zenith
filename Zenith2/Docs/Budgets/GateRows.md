# Zenith2 Budgets: gate budget rows

<!-- informative -->
This file belongs to the `Zenith2/Docs/Budgets.md` document family and shares its normative tag family `Z2-N-BUDGET`, its row-ID scheme, its row columns and its grammar. It holds the four subsections of `Zenith2/Docs/Budgets.md → "Gate budget rows"`, split out of the entry document for length: the warm and cold phase caps, the per-step hard timeouts, the mechanisms that keep gates fast, and the CI integration smoke. Every row defined here is listed in `Zenith2/Docs/Budgets.md → "Row index"` with every other row, and the entry document's rules apply to these rows as to its own ([Z2-N-BUDGET-483]).
<!-- /informative -->

## Phase caps

- [Z2-N-BUDGET-341] Each warm gate phase run (measure, and verify) is capped at 30 minutes of wall-clock by the generic `phaseTimeoutMinutes` setting; the cap is enforced, not only reported (Z2-D-028).
- [Z2-N-BUDGET-342] The single tree-change retry is counted separately from the phase it retries, with the same 30-minute cap.
- [Z2-N-BUDGET-343] When a phase exceeds its cap, `tick_gates.ps1` kills the running line's process tree, fails the phase with exit 4, records the killed line in `steps`, and never flake-retries a phase-cap kill.
- [Z2-N-BUDGET-344] A phase that starts from an empty or invalidated build tree (a new lane, or a preset or CMake change), detected by the Build step, gets the separate, larger cold cap of the row "Cold phase cap", and the gate output reports that the cold cap applied.
- [Z2-N-BUDGET-345] Stage acceptance suites are exempt from the 30-minute cap; they run through `Run-Committed.ps1 -Acceptance -Stage N` under the `Zenith2Acceptance` category's longer `phaseTimeoutMinutes`, which is the row "Acceptance phase timeout".

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-PROC-002 | Warm phase cap measure | adopted | 30 | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Z2-D-028 (plan text) |
| Z2-B-PROC-003 | Warm phase cap verify | adopted | 30 | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Z2-D-028 (plan text) |
| Z2-B-PROC-004 | Tree-change retry cap | adopted | 30 | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Z2-D-028 (plan text) |
| Z2-B-PROC-005 | Cold phase cap | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the tick_gates external-build mode and phaseTimeoutMinutes |
| Z2-B-PROC-006 | Acceptance phase timeout | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |

## Per-step hard timeouts

- [Z2-N-BUDGET-346] Each gate step has its own hard timeout, sized for a cold build (a new lane, or a preset or CMake change); incremental builds are expected far below it, and a step that exceeds its timeout fails the phase like a phase-cap kill.
- [Z2-N-BUDGET-347] `Zenith2Memory`'s hand-over line is `pwsh -NoProfile -File Zenith2\Tools\Gates\Test.ps1 -Label memory`, a re-run of the base Test step filtered to the CTest tests labelled `memory`; its timeout is Z2-B-PROC-011.
- [Z2-N-BUDGET-348] `ShaderChecks.ps1` (in `Zenith2Render`'s gates) and `EcsConformance.ps1` (in `Zenith2ECS`'s gates, running the ECS conformance suite against the custom, flecs and EnTT backend targets) are gate lines from hand-over, written by the Stage 0 base gate-step ticket, and pass with a report line while no shader source or ECS backend target exists; their timeout rows are allocated when a real run first exists and are sized for all three backends.
- [Z2-N-BUDGET-349] A step's timeout row is allocated by the PROC ticket that activates the step, from a measured cold run of that step on the gate host, and the allocation table records the measured cold time.

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-PROC-007 | Step timeout Configure | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-008 | Step timeout Build win-debug | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-009 | Step timeout Build win-release | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-010 | Step timeout Build android-arm64 | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 2 PROC ticket activating Android build and ApkCheck |
| Z2-B-PROC-011 | Step timeout Test | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-012 | Step timeout SourceLayout | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-013 | Step timeout CompileCoverage | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-014 | Step timeout Libgraph | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-015 | Step timeout Complexity | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-016 | Step timeout ForbiddenCheck | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-017 | Step timeout Format | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-018 | Step timeout HeaderSelfContained | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-019 | Step timeout ApiContractCheck | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-020 | Step timeout TestSeparationCheck | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-022 | Step timeout Coverage | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the production coverage checker |
| Z2-B-PROC-021 | Step timeout ReviewSignals | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-023 | Step timeout ApkCheck | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 2 PROC ticket activating Android build and ApkCheck |
| Z2-B-PROC-024 | Step timeout ShaderChecks | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-031 | Step timeout EcsConformance | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 0 PROC ticket for the base Z2 gate step scripts and Run-Committed.ps1 |
| Z2-B-PROC-025 | Step timeout Sanitizers win-asan | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 2 PROC ticket activating Sanitizers |
| Z2-B-PROC-026 | Step timeout Sanitizers linux-tsan | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 2 PROC ticket activating Sanitizers |
| Z2-B-PROC-027 | Step timeout BackendParity | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 3 PROC ticket activating BackendParity |
| Z2-B-PROC-028 | Step timeout IntegrationSmoke | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 3 PROC ticket adding IntegrationSmoke to Run-Committed.ps1 |
| Z2-B-PROC-029 | Step timeout PathParity | pending | pending | min | MP-GATE | Host | 0 (hard) | — | BR-GATE | Stage 5 PROC ticket activating PathParity |

## Gates stay fast

- [Z2-N-BUDGET-350] Gates stay fast through four mechanisms, each of which Z2 gate steps preserve: incremental builds; change-triggered heavy suites (sanitizers through `Zenith2Memory`, parity through `Zenith2Render`); the 30-minute phase cap; and stage acceptance suites run outside the loop.
- [Z2-N-BUDGET-351] A compiler cache (sccache) is pinned for MSVC and NDK clang and shared across lanes, and `win-debug` always builds with `/Gy-` so that libgraph's symbol stage reuses the win-debug build with no fourth build flavour.
- [Z2-N-BUDGET-352] Android arm64 compile and the APK check are part of the base `Zenith2` category's gates on every ticket from Stage 2, and their step timeouts count toward the warm phase caps.
<!-- elaboration -->
- [Z2-N-BUDGET-353] Every gate run's `tick_gates.json` step timings are the MP-GATE evidence for the gate rows; a warm phase whose measured time exceeds 80% of its cap on three consecutive tickets triggers a MAINT ticket to restore headroom, filed by the tick orchestrator.

## CI integration smoke

- [Z2-N-BUDGET-354] The CI integration smoke boots every workload and flagship game in the device-free headless runtime configuration (no Flux, no window, no RHI device, `Zenith2/Docs/Architecture.md → "Multiplayer foundations"` [Z2-N-ARCH-785]), runs 300 frames of its scripted playthrough, and passes only when every correctness assert of that game passes (row "Integration smoke frame count"); WARP and lavapipe host the GPU test executables and render tests ([Z2-N-ARCH-793]), never this smoke.
- [Z2-N-BUDGET-355] The integration smoke is a CI-only step of `Run-Committed.ps1`, outside every category's `gates` list: a Stage 3 PROC ticket adds it to `Run-Committed.ps1`'s CI-only step list, `/tick` never runs it, and it runs in `zenith2-gates.yml` when a pushed change touches an engine path: `Zenith2/Engine/**`, `Zenith2/Player/**`, `Zenith2/Shaders/**`, `Zenith2/Tools/ZenithCook/**`, `Zenith2/Games/**` (a workload's scripted path or content) or `Zenith2/ThirdParty/manifest.json`.
- [Z2-N-BUDGET-356] The integration smoke's step timeout row bounds its CI job and is outside the 30-minute warm phase caps, and its timings are never evidence for a frame-rate row.

| ID | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|---|
| Z2-B-PROC-030 | Integration smoke frame count | adopted | 300 | frames | MP-GATE | Host | 0 (exact) | WS-FRAME | BR-TEST | derived from Z2-D-031 |
