# EngineComposition

The engine's **composition root**: the translation units that wire every
subsystem together and own `g_xEngine`. Split out of `Zenith/Core/` (2026-09,
Phase 2 of the build-system refactor) so `Core` can eventually become a clean
bottom-layer library with no up-edges — the composition TUs are exactly what made
`Core` cyclic with everything above it, since each one `#include`s headers from
across the engine (Flux, Physics, EntityComponent, AI, Editor, ...) to construct
and wire those subsystems.

**This was originally a pure file move — no library-structure change**: the
aggregate `Zenith` Sharpmake project used to glob `Zenith/**` recursively
(`SourceRootPath = RootPath + "/Zenith"`), so every file here compiled into the
same `zenith.lib` it always did, alongside 330 other TUs the aggregate
subtracted back out with a long list of constructor `SourceFilesExcludeRegex`
lines.

**Phase 5.1 re-rooted the aggregate here.** `Build/Sharpmake_Zenith.cs` now
sets `SourceRootPath = RootPath + "/Zenith/EngineComposition"` directly — this
directory **is** `ZenithProject`'s entire source tree, not a subset of a wider
recursive glob. The aggregate's constructor carries **zero**
`SourceFilesExcludeRegex` lines any more (every one of them existed only to
subtract an already-extracted leaf library from the old `/Zenith` glob, and is
verified-dead now that the glob no longer reaches those directories at all).
The 7 `.cpp` + 5 `.Tests.inl` files below are still exactly what
`zenith_win64.vcxproj` compiles — narrowing the root changed nothing about
which files are members, only how many *other* files the aggregate used to
glob and then exclude. `Games/*/Build/*_win64.vcxproj` still reference this
project transitively through the one unchanged
`AddPublicDependency<ZenithProject>` edge every game/tool carries
(`ZenithEngineLibs.AddAllAsDependenciesOfAggregate` still names every other
leaf library publicly), so no game or tool file needed to change.

## Files

- `Zenith_Engine.cpp` - Defines `g_xEngine` and every `Zenith_Engine::X()` accessor body; the concrete wiring for the process-level engine owner
- `Zenith_Core.cpp` - Main loop (`Zenith_MainLoop`) and per-frame timer tick (`UpdateTimers`). Also the aggregate engine lib's PCH **Create** TU on win64 (`conf.PrecompSource` in `Build/Sharpmake_Zenith.cs`) — it already begins `#include "Zenith.h"` and compiles unconditionally
- `Zenith_Main.cpp` - Platform-neutral entry point / boot sequencing
- `Zenith_BenchECS.cpp` - ECS microbenchmark harness
- `Zenith_AutomatedTest.cpp` - `Zenith_AutomatedTestRunner` — the `--automated-test` driver used by every game's photo-tour / scripted-scenario harness
- `Zenith_UserSettings.cpp` - The engine-owned player-preference store (`g_xEngine.UserSettings()`); see `Zenith/Core/CLAUDE.md` → *Persisted settings* for the full contract
- `Zenith_EnginePCH.cpp` - Definition-free `#include "Zenith.h"` one-liner; exists **solely** as the AGDE/clang PCH Create TU (`conf.PrecompSource` when `target.Platform == Platform.agde`). clang precompiles the entire file passed to `-xc++-header`, so a definition-bearing Create TU (`Zenith_Core.cpp`) would bake its symbols into `Zenith.h.pch` and every consumer would re-emit them — hundreds of duplicate-symbol errors linking a game `.so`. MSVC's `/Yc` only captures the header prefix, so win64 is unaffected and keeps `Zenith_Core.cpp` as its Create TU. Do not add anything else to this file.
- `Zenith_UnitTests.Tests.inl`, `Zenith_TerrainDimensions.Tests.inl` - `#include`d from `Zenith_Core.cpp`
- `Zenith_Main.Tests.inl` - `#include`d from `Zenith_Main.cpp`
- `Zenith_UserSettings.Tests.inl` - `#include`d from `Zenith_UserSettings.cpp`
- `Zenith_AutomatedTest.Tests.inl` - `#include`d from `Zenith_AutomatedTest.cpp`

## Headers deliberately stay in `Zenith/Core/`

`Zenith_Engine.h` (446 includers), `Zenith_AutomatedTest.h` (231 includers) and the
rest of these TUs' headers were **not** moved — only the `.cpp` implementation
files and their `.Tests.inl` companions did. That means zero `#include` directives
across the engine needed to change for this split: every consumer still reaches
these headers via `Zenith/Core` on its include path
(`ConfigureCommonIncludePaths` in `Build/Sharpmake_Common.cs`), exactly as before.
The five `#include "Core/<name>.Tests.inl"` directives inside these `.cpp` files
are the only include sites the move touched — they now read
`#include "EngineComposition/<name>.Tests.inl"`.

## Why the split, not just a rename

`Core` is meant to be a bottom-layer (L0-adjacent) library: low-level utilities,
config, memory, threading — things with no reason to know about Flux, Physics or
the Editor. But the composition TUs *must* know about all of them, because wiring
the engine together is their entire job. Leaving them in `Core/` made the module
cyclic with every layer above it in the architecture gate
(`Tools/complexity_profiles.json` → `architecture.modules`); they are their own
module (`EngineComposition`, layer 5) precisely so that constraint can eventually
be enforced on `Core` itself. See `Tools/architecture_allowlist.txt` and the
`singleton_ratchet.exclude_globs` entry (these TUs — the ones that legitimately
construct and reach `g_xEngine` — are excluded from the per-file `g_xEngine`
count ratchet).
