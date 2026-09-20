# Unit-Test Harness

## Overview

Small, shared support code for the engine's negative-path / automated-test
harness: assert-capture atomics (`Zenith_AssertCaptureScope` — lets a test
assert that a `ZENITH_ASSERT` fires without crashing the process) plus a
handful of test-only headers consumed by other subsystems' own test files
(`Zenith_AITests.h`, `Zenith_TempScene.h`, `Zenith_UnitTests.h`).

## Files

- `Zenith_AssertCapture.h` / `.cpp` — the assert-capture scope + hit-count atomics.
- `Zenith_AITests.h` — shared AI test helpers (consumed by AI test files; not compiled here).
- `Zenith_TempScene.h` — a scoped temporary-scene helper for ECS-backed tests (header-only; not compiled here).
- `Zenith_UnitTests.h` — misc shared test declarations.

`Zenith_EditorTestFixture.h`/`.cpp` used to live here too; it moved to
`Zenith/Editor/` (Phase 4b) — see that directory's own `CLAUDE.md`. It was the
sole reason this directory used to reach into `Editor/`, `EntityComponent/`,
and `Input/`; with it gone, this directory's own header graph is clean.

## Build (Sharpmake library)

Compiled into `ZenithUnitTests` (`Build/Sharpmake_ZenithUnitTests.cs`) — 1 TU
(`Zenith_AssertCapture.cpp`), no PCH (below the ≥4-TU threshold; compiles
`Zenith.h` textually). Depends only on `ZenithCore` (mandatory). Not linked by
any Sentinel (`Tests/Sentinel*`) — those link only `ZenithCore` + one leaf
(`ZenithECS`/`ZenithPhysics`/`ZenithAI`), so each Sentinel's own
`sentinel_platform.cpp` still supplies its own trivial
`g_uAssertCaptureHitCount`/`g_bAssertCaptureActive` definitions rather than
linking this library; that duplication is deliberate and unaffected by this
library's extraction (see each Sentinel's own header comment).
