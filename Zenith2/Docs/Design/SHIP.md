# SHIP design — Shipping & maintenance

The design of the SHIP area: the reproducibility and dependency manifest, the clean-machine setup, the CMake helpers SHIP owns after the scaffold (the `THIRD_PARTY` behaviour and the flag rows among them), the Shipping configuration, the release pipeline (packaging, signing, the release manifest, symbols, the licence inventory, reproducibility), dependency updates, release documentation and upgrade guides, the compatibility demonstration, the CI and device matrices, and the Stage 6 shipping qualification. Workers implement against this file and its companions; the requirement catalogue is `Zenith2/Docs/Requirements/SHIP.md`.

## Purpose and scope

### What SHIP owns

| Capability | Inventory items | Specified in |
|---|---|---|
| reproducible builds: the manifest, the toolchain pins, the setup script, the environment scripts, the dependency cache and its digest verification, builds from source in sanitizer presets, byte-reproducible outputs | Z2-I-SHIP-01 | `Zenith2/Docs/Design/SHIP/Manifest.md`; `Zenith2/Docs/Design/SHIP/Release.md` → "Reproducible builds" |
| the CI matrix (specification of `zenith2-gates.yml`; PROC edits the protected file) | Z2-I-SHIP-02 | `Zenith2/Docs/Design/SHIP/Qualification.md` → "CI matrix" |
| the device matrix | Z2-I-SHIP-03 | `Zenith2/Docs/Design/SHIP/Qualification.md` → "Device matrix" |
| packaging: the Shipping configuration, the release pipeline, the Windows and Android packages | Z2-I-SHIP-04 | `Zenith2/Docs/Design/SHIP/Release.md` → "Shipping configuration", "Release pipeline" |
| signing and key custody | Z2-I-SHIP-05 | `Zenith2/Docs/Design/SHIP/Release.md` → "Signing" |
| release manifests | Z2-I-SHIP-06 | `Zenith2/Docs/Design/SHIP/Release.md` → "Release manifest" |
| symbol retention | Z2-I-SHIP-07 | `Zenith2/Docs/Design/SHIP/Release.md` → "Symbols" |
| the dependency inventory (the manifest itself, the repository inventory) | Z2-I-SHIP-08 | `Zenith2/Docs/Design/SHIP/Manifest.md` → "The dependencies section" |
| the licence inventory and notices | Z2-I-SHIP-09 | `Zenith2/Docs/Design/SHIP/Release.md` → "Licence inventory" |
| dependency updates | Z2-I-SHIP-10 | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency updates" |
| the compatibility policy demonstration | Z2-I-SHIP-11 | `Zenith2/Docs/Design/SHIP/Release.md` → "Compatibility policy demonstration" |
| release documentation | Z2-I-SHIP-12 | `Zenith2/Docs/Design/SHIP/Release.md` → "Release documentation" |
| upgrade guides | Z2-I-SHIP-13 | `Zenith2/Docs/Design/SHIP/Release.md` → "Upgrade guides" |
| the CMake helpers after the scaffold, the toolchain files, the generic registry functions | Z2-I-SHIP-01, -02, -08 (the build that serves them) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` |
| the Stage 6 shipping qualification and the SHIP budget rows | Z2-I-SHIP-03, -04 | `Zenith2/Docs/Design/SHIP/Qualification.md` → "Shipping qualification", "Measured rows at qualification" |

Directories: `Zenith2/Tools/Setup/`, `Zenith2/ThirdParty/` and `Zenith2/CMake/` (`Zenith2/Docs/Architecture.md` → "Repository layout", table "Ownership per directory"); inside `CMake/`, `ZenithAddGame.cmake` and the CMake fixture tests are MOD's (`Zenith2/Docs/Design/MOD.md` → "CMake helper files owned by MOD"), `ZenithShaders.cmake` RPROD's, and `ZenithBudgetTable.cmake`'s content CORE's.

### What SHIP does not own

| Not SHIP | Owner |
|---|---|
| the installation layout's content tree, archives, App Bundle modules, asset packs, patch and DLC manifests, `update.zman`, interrupted-update recovery | PACK |
| the API and version policy, upgrade rehearsals, deprecated-API removal, gate baseline upkeep | MAINT |
| every edit of `Zenith2/Tools/Gates/**`, `.github/**` and the protected configuration | PROC |
| `zenith_add_game`, `game.json`, the Gradle template, the Player | MOD |
| the in-Player budget-run mode, the workload runner, the evidence fingerprint computation, crash dumps | DEVT |
| security trust boundaries, update verification, fuzzing policy | SEC |
| each dependency's integration code inside its owner library | the owning area |

### The SHIP PACK MAINT and PROC split

The split follows the inventory rows (`Zenith2/Docs/Requirements/Inventory.md` → "SHIP Shipping & maintenance", → "PACK Packaging & content updates", and the MAINT and PROC tables of → "Inventory v1"):

| Concern | SHIP | PACK | MAINT | PROC |
|---|---|---|---|---|
| a packaged build | builds it in Shipping, adds executable, redistributables and licences, signs, retains symbols, writes `release.json`, verifies | lays out `Content/`, writes `mount-manifest.json`, builds the AAB and asset packs | — | — |
| a patch or content update | provides key custody and `release.json` | builds the patch, `update.zman`, recovery | — | — |
| a dependency upgrade | the manifest entry, the update tool, the evidence invalidation | — | the rehearsal cadence and policy | — |
| deprecation | the upgrade guide, the compile-fail demonstration | — | the removal tickets | — |
| CI | the matrix specification and `Pwsh_Ship_CiWorkflowShape` | — | — | the workflow file and `Run-Committed.ps1` |
| gates reading SHIP outputs | `declared_deps.json`, `third_party_targets.json`, the manifest | — | — | `Libgraph`, `ForbiddenCheck`, `TestSeparationCheck`, `ApkCheck` |

### Inventory items covered

Z2-I-SHIP-01 to Z2-I-SHIP-13, all required; the coverage table is `Zenith2/Docs/Requirements/SHIP.md` → "Coverage of inventory items".

### Document family

| File | Holds | Its headings |
|---|---|---|
| `Zenith2/Docs/Design/SHIP.md` (this file) | the skeleton, the public surface, state machines, error codes, stage plan, decisions, the requirement index | the headings of this file |
| `Zenith2/Docs/Design/SHIP/Manifest.md` | the manifest, the cache, the setup script, dependency updates | Scope of this file · Manifest file · The toolchain section · The dependencies section · Manifest validation · Consumer entries · Dependency cache · Setup script · Dependency updates |
| `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` | the CMake helpers SHIP owns | Scope of this file · Ownership of the CMake directory · zenith_begin_build and zenith_end_build · THIRD_PARTY behaviour · ZenithThirdParty.cmake · zenith_add_library completion · zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests · zenith_apply_flags · Toolchain files · Registry functions · declared_deps additions |
| `Zenith2/Docs/Design/SHIP/Release.md` | the Shipping configuration and the release pipeline | Scope of this file · Shipping configuration · Release pipeline · Signing · Release manifest · Symbols · Licence inventory · Reproducible builds · Release documentation · Compatibility policy demonstration |
| `Zenith2/Docs/Design/SHIP/Qualification.md` | the CI matrix, the device matrix, the qualification | Scope of this file · CI matrix · Device matrix · Shipping qualification · Measured rows at qualification |
| `Zenith2/Docs/Design/SHIP/Tests.md` | every SHIP test | Test registration · CMake script-mode tests · PowerShell tests · Integration suites · Stage acceptance items · Which gate step runs each |

### Reconciliations with the core documents

<!-- elaboration -->
SHIP elaborates the core documents and contradicts none. Every addition that needs core-document wording is a row of `reviews/CROSS_DOC_ITEMS.md`, the queue the core-document batch consumes (the freeze rule); cross-area row SH1 (`findings/A3_cross_area_notes.md`) only points to those rows. Core-document patch c (2026-10-03) applied X221, X222, X203, X231 and most of X220; core-document patch d (applied 2026-10-09) applied X215, X216 and X262, which SHIP conforms to (rows C19 to C21). Core-document patch e (2026-10-09) applied the rest of X220, X239, X256 and X268, so no row of the table below is pending; a SHIP ticket implementing an item of a row that is not applied would stay `draft` until it is.

| # | Core section | Addition | X row (state) | Where SHIP specifies it |
|---|---|---|---|---|
| C1 | `Zenith2/Docs/Architecture/BuildSystem.md` → "The manifest dependencies section" | the fields `fromToolchain`, `definitions`, `dependsOn`, `pins`, `shipping`, `targets[].exceptions`, `targets[].hostExecutable`, the `prebuilt` item fields, the owner tokens `tests`, `benchmark`, `test-runs`, `host` and `autoLink` | X220 item 1 (applied) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency entry fields" |
| C2 | → "The toolchain section" | the JSON shapes, `runtimeComponents`, `ndk.pkgRevision`; Slang, DXC, WARP and lavapipe through `fromToolchain` | X220 item 2 (applied, except the `llvm` row) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain keys" |
| C3 | → "The toolchain section" (row `llvm`) | the one form of the Linux and WSL compiler pin: a `linux` archive object of the LLVM release, extracted to `toolchain/llvm/<version>/` in WSL and on the native Linux host, never a distro package; `wsl.snapshot` and `wsl.packages` for the system packages | X220 item 2 remainder (applied by patch e; finding 4 decided it as stated here) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain keys", "Linux host mode" |
| C4 | → "The dependency cache" [Z2-N-ARCH-204] | `<sha256>-p<16 hex>`, `<abi>-<sha256>`, no directory for `fromToolchain`, the stamp beside the tree | X220 item 3 (applied) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Cache layout" |
| C5 | → "Toolchain pins and the setup script" [Z2-N-ARCH-194] | the compiler comparison in `zenith_begin_build` (MSVC over the first three components) | X220 item 4 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Global flag hygiene" |
| C6 | → "Signature", "Tools, benchmarks and integration suites" | `TEST_THIRD_PARTY`, `CONFORMANCE_LABELS` (the one label mechanism for conformance executables, X215's conformance half), `THIRD_PARTY` on `zenith_add_integration_tests`, `SLANG_SHADERS`/`SLANG_ALTERNATIVES` | X220 item 5 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Keywords added by SHIP", "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| C7 | → "Tools, benchmarks and integration suites" | `SLANG_ALTERNATIVES` is a list of D-SLANG alternative IDs defaulting to `1 2 3`; `ZENITH_PROBE_SLANG_VERSION` and `ZENITH_BENCH_SHADER_ALTERNATIVES`; rule 2 of name resolution skips an entry absent from the preset's platform for benchmarks, tools and integration suites | X239 item 4 (applied by patch e) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Name resolution", "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| C8 | → "Test, conformance and fuzz targets" [Z2-N-ARCH-256] | conformance executables created by `zenith_end_build`; per-backend discovery; `ZENITH_EXPECTED_BUILD`, `ZENITH_TEST_GENERATED_ROOT` | X220 item 6 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Conformance executables", "Test environment" |
| C9 | → "The declared_deps.json record" | `testThirdParty`, `conformanceLabels`, `benchmarks[]`, `integrationSuites[]`, `third_party_targets.json` | X220 item 7 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "declared_deps additions" |
| C10 | `Zenith2/Docs/Architecture.md` → "Third-party dependencies" [Z2-N-ARCH-121] | pre-ruling `owner: "benchmark"` candidate entries; the exceptions-and-RTTI-off admission rule | X220 item 8 (applied) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Owner values", "Selection-candidate entries" |
| C11 | `Zenith2/Docs/Scripting.md` → "Exception-enabled target list"; BuildSystem [Z2-N-ARCH-176], [177] | the table is **unchanged**: the manifest's `exceptions: true` elements realise exactly its Luau rows, enforced by the closure rule, and growing the set still needs a Z2-D-017 decision ([Z2-N-SCRIPT-021]); the core wording to add is one sentence naming the closure rule as the check | X220 item 9 remainder (applied by patch e through X256 (4), the closure-rule sentence of [Z2-N-SCRIPT-021]; finding 3 decided it as stated here) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "The targets array"; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Target mapping and host executables" |
| C12 | BuildSystem helper-module table | `CMake/ZenithBudgetTable.cmake` | X220 item 10 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Ownership of the CMake directory" |
| C13 | → "Compile and link flags", "Third-party targets and configuration variables" [Z2-N-ARCH-230], [258], [259]; Scripting [Z2-N-SCRIPT-027] | the third-party rows (exceptions on with `/D_HAS_EXCEPTIONS=1` for Luau), all five per-configuration variables, the reproducibility and hardening rows from Stage 1, the Linux build ID | X221 and X203 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Global flag hygiene", "Third-party flag rows", "Reproducibility flags", "Hardening flags" |
| C14 | → "Android packaging" (table "The Gradle build types") | `shipping` unsigned in Gradle, signed by the release pipeline; `release` signed with the development keystore | X222 (applied) | `Zenith2/Docs/Design/SHIP/Release.md` → "Android APK signing" |
| C15 | `Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-479]; BuildSystem table "Configurations" | asserts per configuration in every target; host tools use `ZENITH_CHECK` for Release invariants (Q71 (a)) | X231 (applied) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Assert define" |
| C16 | BuildSystem → "The manifest dependencies section" (`name`, `source`, `targets` rows and the example entry); Architecture.md "Selected dependencies" | entry names in lower-case kebab case (`sdl3`, `luau`, `entt`, `ktx-software`, `tracy`, `xxhash`) with exact matching; `source.format`; `licenceElected`; `targets[].platforms` | X239 items 1 to 3 (applied by patch e) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency entry fields", "Entry names used by consumers", "The targets array" |
| C17 | BuildSystem → "Third-party integration" | the ownership rules run at the end of the configure against the declaration store, `zenith_add_tool` recording `ZENITH_DECL_<Name>_TOOL_KIND` | X239 item 5 (applied by patch e, in [Z2-N-ARCH-200]) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules" |
| C18 | BuildSystem → "The toolchain section" (every row, the `wsl` shape), → "The manifest dependencies section" (`owner` row), [Z2-N-ARCH-200] (reworded) | every toolchain key except `vulkanProfilesGenerator` carries `licence` (the `wsl` row gains it); an `owner` may be a benchmark target `Zenith_Bench_<Name>` (the form of Z2T-CORE-053); an owner named by a pin before the ticket that declares it is reported, not refused, until it is declared | X268 (applied by patch e) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain licence terms", "Owner values", "Validation rules" |
| C19 | BuildSystem → "Signature", → "Tools, benchmarks and integration suites", [Z2-N-ARCH-251] | `TEST_LABELS` on `zenith_add_integration_tests`; every label list passed as the discovery call's `LABELS` property, doctest suite names merged as labels through `ADD_LABELS` | X215 (applied by patch d) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Keywords added by SHIP", "Discovery labels" |
| C20 | BuildSystem → "Configurations" ([Z2-N-ARCH-231]), the MSVC flag table, "Third-party targets and configuration variables", [Z2-N-ARCH-181], [191], [250], [259], the `shipping` row, the presets | the `ShippingTest` configuration of the `win` tree, `zenith_tests`, `win-shipping-test`, `$<CONFIG:Shipping,ShippingTest>` for excluded dependencies, `ZENITH_EXPECTED_BUILD=windows:shipping:none` | X216 (applied by patch d) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Global flag hygiene", "ShippingTest configuration" |
| C21 | BuildSystem [Z2-N-ARCH-250], [254], [180] | on Android presets `ZENITH_TEST_GENERATED_ROOT` and `ZENITH_SHADERS_GENERATED_DIR` name the test APK's `files/generated/Tests` and `files/shaders` device directories | X262 (applied by patch d) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Test environment" |
| C22 | BuildSystem → "The ZENITH_SUPPORTED matrix" | `ZenithAuthoring`, `ZenithEditorCore` and `ZenithNewGameScaffold` are `ZENITH_WINDOWS` (SEC row SEC20, accepted) | X444 (pending, SEC's row) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| C23 | `Zenith2/Docs/Requirements/SHIP.md` → Z2-R-SHIP-139 | every Maven runtime artefact in an APK is a licensed row of `maven-artefacts.json` | X456 (pending) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Maven artefact rows" |
| C24 | BuildSystem → "The manifest dependencies section" (`prebuilt` row), [Z2-N-ARCH-201]; Architecture.md [Z2-N-ARCH-125] | `manualDownload` and `importLibrary` items; an import library is not a prebuilt library for the sanitizer rule; the Maven rows state their licences | X457 (pending) | `Zenith2/Docs/Design/SHIP/Manifest.md` → "The prebuilt array", "Manual-download items", "Maven artefact rows" |

BuildSystem's normative range 151–259 is full (core-document patch c), so X239 words every addition as a rewording of an existing bullet or a table row, never as a new `[Z2-N-ARCH-NNN]`.

## Dependencies

### Areas SHIP depends on

| Area | What SHIP relies on | Cited heading |
|---|---|---|
| (build contract) | the helper signatures, the glob partition, presets, configurations, the flag tables, the manifest table, the cache rule | `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper", "Compile and link flags", "Presets", "Toolchain pins and the setup script", "Third-party integration" |
| (architecture) | the third-party table, the security architecture, the repository layout | `Zenith2/Docs/Architecture.md` → "Third-party dependencies", "Security architecture", "Repository layout" |
| (contracts) | the assert table, test separation, the compatibility policy, the shared gate environment | `Zenith2/Docs/Contracts.md` → "Errors and results", "Tests and code hygiene gates", "Compatibility policy per kind" |
| (budgets) | the SHIP rows, the measurement procedures, reference hardware, the production-quality acceptance | `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure", "Reference hardware", "Production-quality acceptance" |
| MOD | the scaffold's helpers (its generic fuzz rule `_zenith_register_fuzz_targets()` and the `Zenith_Fuzz_<Stem>.Smoke` entries included, Z2T-MOD-056), `zenith_add_game`, the Gradle generation and JNI staging, `composition.json` | `Zenith2/Docs/Design/MOD.md` → "Stage 0 scaffold", "The composition.json record"; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Build types and JNI staging", "The Apk target" |
| CORE | `Zenith_BuildInfo.h` and its enumerations; ZenithTestSupport's test main; the registry block functions `zenith_core_death_case`, `zenith_core_selftest_case`, `zenith_core_compile_fail` and `zenith_core_fuzz_target` (Z2T-CORE-002), which SHIP adopts as the programme's death-case and compile-fail registration | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information"; `Zenith2/Docs/Design/CORE.md` → "ZenithTestSupport target"; `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" |
| SAVE | `Zenith_GetRunningBuildID()` as the build ID SHIP's tools print | `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header" (cross-area row SV15) |
| SCRIPT | the Luau pin consumers, the firewall target list | `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility", "Luau integration and the exception firewall"; `Zenith2/Docs/Design/SCRIPT.md` → "Supported matrix and compile groups" |
| RHI | the Agility SDK version constant, the headless CI devices | `Zenith2/Docs/Design/RHI.md` → "Headless CI devices" |
| PLAT | the mount manifest and patch verification | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema", "Patch mount verification" |
| COMPRESS | the zlib and zstd entries | `Zenith2/Docs/Design/COMPRESS.md` → "Third-party integration of zlib", "Third-party integration of zstd" |
| PACK | `<Game>_Package`, the AAB target and its unsigned output, the update-manifest writer (assumed, pending) | `Zenith2/Docs/Design/SHIP/Release.md` → "Windows package", "Android package" (assumption blocks) |
| DEVT | the workload runner's `-Package` mode, the WSL user and `wsl.conf`, the Linux TSan host check | `Zenith2/Docs/Design/DEVT/QualityTools.md` → "WSL distro provisioning" (rows DV16, SH10) |
| MOD (hosted APKs) | the bindless probe's benchmark APK, the test-APK route for generated test inputs | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs" (row SH15) |
| SEC, CONTENT | the hardening evidence, the provenance licences (assumed, pending) | cross-area rows SH8, SH9 |

### Areas that depend on SHIP

Every area's library depends on the helpers; the manifest consumers are listed in `Zenith2/Docs/Design/SHIP/Manifest.md` → "Consumer entries".

### Consumer needs and the SHIP answer

| Consumer need (their name) | SHIP answer |
|---|---|
| CORE CB4: `ZENITH_EXPECTED_BUILD` per preset | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Test environment" |
| CORE CB5, ECS EB3, RHI RB7: `THIRD_PARTY` on `zenith_add_benchmark` | → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| CORE CB2 (rest): `ZenithTestSupport` on benchmarks and integration suites | same section, table "Links" column |
| CORE CB12: `Symbolize-Leaks.ps1` | `Zenith2/Docs/Design/SHIP/Release.md` → "Symbolization tools" |
| CORE CB6, CORE-P8: `mimalloc`, `xxhash`, `yyjson`, `simdjson` pins with owner "benchmark" | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Selection-candidate entries" |
| ECS EB1, SCRIPT SC13: conformance executables, `TEST_DEPENDS` in `<Name>_Conformance` | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Conformance executables" |
| ECS EB2: death-suite pass conditions, also inside conformance executables, and every death case of an executable registered | CORE's `zenith_core_death_case` registers any target's case (Z2T-CORE-002; ECS's tickets already call it for `ZenithECS_Conformance`); SHIP adds the registration of every case no block registered: → "Death-case registration", "Unlisted death-case discovery" |
| ECS-P1, E8, E16: flecs and EnTT pins, benchmarks as second owners | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Other consumer entries"; the second-owner request answered with a reasoned alternative in → "Owner values" (row SH7) |
| GEOM GB27, COMPRESS rows: fuzz partition, `THIRD_PARTY` on `zenith_add_tool` | the scaffold's generic fuzz rule (Z2T-MOD-056) creates every `Zenith_Fuzz_<Stem>`; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Fuzz and compile-fail partitions", "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| RHI RB7: `CONFORMANCE_LABELS` (label `z2-parity-backend`), `SLANG_SHADERS`/`SLANG_ALTERNATIVES` (default `1 2 3`), `ZENITH_PROBE_SLANG_VERSION`, a Windows-only entry on a multi-platform benchmark | → "Keywords added by SHIP"; → "Name resolution"; → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" (row SH18) |
| RHI-P2, R7: Vulkan and D3D12 entries, `renderdoc-app` shared by two backends, the Vulkan-Profiles generator pin | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Other consumer entries", "D3D12 Agility SDK entry", "Toolchain keys" |
| SCRIPT SB15, SCRIPT-P1: Luau `targets` mapping (the `luau` entry, `Luau.Analyze.CLI` on Windows alone, the closure rule that keeps the exception set at Scripting's table), `luau-analyze` host executable, `apiLinkage`, `nativePolicy`, bytecode versions, `LUA_USE_LONGJMP` off, the pin-header reader (no `configure_file` in ZenithScript), `ZENITH_TEST_GENERATED_ROOT` | → "Luau entry", "The targets array"; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Target mapping and host executables"; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Pin header templates", "Test environment"; the automated-boot test helper is MOD's runtime test seam (not SHIP) |
| ASSET-P3, GEOM-P1, GM11: `fastgltf`, `meshoptimizer`, `ktx-software` (one zstd), `mikktspace` | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Other consumer entries" |
| LIGHT-P2: upscaler candidate pins | → "Selection-candidate entries" |
| RPROD-P1, RPROD-P4, RP11: one Slang pin, DXC (in process), FLIP | → "Other consumer entries", "Test and benchmark entries" (row SH19) |
| MOD MB2, M20: JDK, SDK, Gradle, AGP and NDK pins, the cache paths, the offline Gradle home and its verification metadata | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain keys", "Offline Gradle home"; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Exposed variables" |
| RPROD RPB22: the `KIND GENERATED` branch of `zenith_add_library` calling `zenith_add_shaders`, and `ZENITH_SHADERS_GENERATED_DIR` on test targets | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Generated shader library", "Test environment" (SHIP owns both, ruling RPB22-SHIP) |
| CONTENT CT32 (b), CT16 (b): git-lfs installed and pulled on every gate host and lane; `ZenithContent merge --install` run by the setup | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Git prerequisites", "Setup steps" (steps 8a, 8b); Z2T-SHIP-107 |
| CONTENT CT28 (b): the decoder entries and `ZenithContentCore` as an owner of `fastgltf`, `simdjson` and `meshoptimizer` | answered by ruling CT28B-DEP-OWNERS (SHIP's row SH27): `fastgltf` and `meshoptimizer` owned by `ZenithContentCore`, `fastgltf` `dependsOn` `simdjson` (Z2T-SHIP-051); tool targets that call them directly name them as consumers in their own `THIRD_PARTY` (ruling MESHOPT-VIA-CONTENTCORE-2, resolver rule 3; no wrapper API); `simdjson` keeps CORE's candidate owner until D-SEL-JSON rules; the five decoders are Z2T-SHIP-110 to -114 (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Other consumer entries") |
| DEVT DvB13 (d): SHIP's own `RowSources.json` entries | Z2T-SHIP-108 (ruling DEVT-AREA-REGISTRATION) |
| JOBS J12, JB13: CTest labels | the scaffold's `TEST_LABELS` ([Z2-N-ARCH-251]); nothing further |
| COMPRESS-034: licence inventory in packaged builds | `Zenith2/Docs/Design/SHIP/Release.md` → "Licence inventory" |
| SAVE SV15: build identity flags from Stage 1; Android backup rules | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Reproducibility flags" (`/DEBUG:FULL` with `/Brepro`; `--build-id=sha1` on Android and Linux, every preset, from Stage 1); the backup-rule XML is MOD's template content (row SH3) and SHIP verifies it (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Android package checklist" item A13) |
| Architecture "Third-party dependencies", BuildSystem `owner` arrays for header-only entries | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Owner values" |
| SEC SEC20: `ZenithAuthoring`, `ZenithEditorCore` and `ZenithNewGameScaffold` built in `win-asan` | accepted: `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" (the static tool libraries paragraph); the matrix row is core-document item X444; the `SUPPORTED` lines are EDIT's and MOD's |
| SEC SEC6 (a) to (i): the tool-side fuzz helper, the smoke options, `toolFuzzTargets[]`, `ZenithSecurity.cmake`, the exposure checks, the update-key precondition, `HOST_PWSH` script tests | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Tool-side fuzz targets", "Fuzz smoke options", "zenith_register_script_test", "declared_deps additions", "Ownership of the CMake directory"; `Zenith2/Docs/Design/SHIP/Qualification.md` → "Windows package checklist", "Android package checklist"; `Zenith2/Docs/Design/SHIP/Release.md` → "New-Zenith2Release"; Z2T-SHIP-122, -123, -127, -128 |
| SEC SEC15: `monocypher` 4.0.2 and `libsodium` 1.0.20 for the Ed25519 harness | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Selection-candidate entries"; Z2T-SHIP-118, the `ISC` term by Z2T-SHIP-117 |
| PROC PG2, AUDIO AuB17: the D-SEL-CODEC candidates `opus`, `ogg`, `vorbis` | → "Selection-candidate entries"; Z2T-SHIP-119 |
| CINE CnB2 (ruling CINE-R1-RULINGS F10): `ffmpeg-libavcodec`, LGPL 2.1 build | → "Selection-candidate entries", "The ffmpeg-libavcodec entry"; Z2T-SHIP-120, the `LGPL-2.1-or-later` tools term by Z2T-SHIP-117 |
| UI UiB4: `harfbuzz`, `freetype`, `msdfgen`, `sheenbidi`, `icu4x-capi` | the first four: → "Other consumer entries", Z2T-SHIP-121, their terms by Z2T-SHIP-117; `icu4x-capi` refused as a `prebuilt` runtime entry (→ "Entry names used by consumers") |
| ONLINE ON9, OnB14: manual download, Maven artefacts, licence rulings, Steam packaging | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Manual-download items", "Maven artefact rows"; `Zenith2/Docs/Design/SHIP/Release.md` → "Windows package", "New-Zenith2Release"; Z2T-SHIP-124, -125, -126, -129 |

### Interfaces SHIP declares for other areas

- CMake: `zenith_stage_runtime_files`, `zenith_add_licence_inventory` (called by MOD's `zenith_add_game`), the registry functions `zenith_register_script_test`, `zenith_register_pwsh_test` and `zenith_register_fixture_set`, the tool-side fuzz helper `zenith_tool_fuzz_target` with its ZenithCook wrapper `zenith_cook_fuzz_target`, `zenith_fuzz_smoke_args`, the registry tail `_zenith_ship_registry_tail()`, the `ZENITH_TP_*` variables and pin-header templates.
- PowerShell: `Zenith2Env.psm1` (the environment and manifest readers, `Get-Zenith2BinaryIdentity`) and `Zenith2Release.psm1` (`Get-Zenith2ReleaseProfile` and `Get-Zenith2UpdateSigningKey`, used by PACK's update-manifest writer).
- Java (run by the pinned JDK, never compiled into a target): `Zenith2/Tools/Setup/Ed25519PublicKeyCheck.java`, which proves that an Ed25519 seed and a public key belong together (`Zenith2/Docs/Design/SHIP/Release.md` → "Update manifest signing hand-off").
- Files: `declared_deps.json` additions, `third_party_targets.json`, `release.json`, `licences.json`, the symbol store.

SHIP declares no C++ interface and no upward callback.

## Library and file layout

### Files owned by SHIP

```text
Zenith2/ThirdParty/
	CMakeLists.txt                         zenith_third_party_add_all() (scaffold)
	manifest.json                          `Zenith2/Docs/Design/SHIP/Manifest.md`
	licence_policy.json                    the allowed licence identifiers
	licences/*.txt                         licence texts no pinned archive carries (Stage 0: LicenseRef-Microsoft-Visual-Cpp-Redistributable.txt, llvm-project-LICENSE.txt)
	patches/<name>/NNNN-<slug>.patch
	gradle/verification-metadata.xml       the Gradle dependency verification
	gradle/seed/                           the cache-populating Android project
	gradle/maven-artefacts.json            the Maven artefact rows (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Maven artefact rows")
	<name>/CMakeLists.txt                  zenith-wrapper builds (zlib, volk, vma, d3d12ma, physx, imgui, mikktspace, yyjson, simdjson, monocypher, libsodium, vorbis, ffmpeg-libavcodec with its `config/<abi>/`, harfbuzz, sheenbidi), each added by the pinning or owner ticket that § "Pin landing order" names for its entry
Zenith2/CMake/
	ZenithBuild.cmake ZenithAddLibrary.cmake ZenithAddTool.cmake ZenithWarnings.cmake ZenithFlags.cmake
	ZenithSupported.cmake ZenithThirdParty.cmake toolchains/msvc-x64.cmake toolchains/clang-linux-x64.cmake
	Tests/RunShipFixture.cmake Tests/DiscoverDeathCases.cmake Tests/Ship/**  (and the SHIP block and the tail call of Tests/ZenithCMakeTests.cmake)
Zenith2/Tools/Setup/
	Setup-Zenith2.ps1 Enter-Zenith2Env.ps1 Zenith2Env.psm1 Zenith2Release.psm1
	Update-Zenith2Dependency.ps1 Update-Zenith2Toolchain.ps1 Update-Zenith2GradleVerification.ps1
	New-Zenith2Release.ps1 Sign-Zenith2Windows.ps1 Sign-Zenith2Android.ps1 Test-Zenith2Release.ps1
	Test-Zenith2Reproducibility.ps1 Resolve-Zenith2Address.ps1 Symbolize-Leaks.ps1 Remove-Zenith2Symbols.ps1
	New-Zenith2ReleaseNotes.ps1 New-Zenith2UpgradeGuide.ps1 Test-Zenith2ReleaseDocs.ps1
	compatibility_tests.json Ed25519PublicKeyCheck.java
	Tests/*.Tests.ps1 Tests/Fixtures/**
Zenith2/Docs/Release/
	README.md                              the release-document templates
	<Game>/<version>/ReleaseNotes.md release.json licences/<platform>.json reports/verify.json reports/reproducibility.json Qualification.json
	UpgradeGuides/Z2-Stage-<N-1>-to-Z2-Stage-<N>.md
```

SHIP has no C++ library, no `Internal/` code and no `Internal/Windows|Android|Linux` files; its one compiled-language input is the compile-fail source `Zenith2/CMake/Tests/Ship/DeprecatedUse_CompileFail.cpp.in`, which no glob compiles. The PowerShell scripts and `Ed25519PublicKeyCheck.java` (run by the pinned JDK's single-file source launcher, `java <file>.java`, never built by CMake) are not compiled sources of any target, so `SourceLayout` and `CompileCoverage` do not apply to them.

### Supported matrices

| Item | Presets |
|---|---|
| the helpers, the manifest validation, the licence inventory script | every configure preset |
| `CMake_Ship_*` checks | `win`, `win-asan`, `linux-tsan` |
| `Pwsh_Ship_*` checks | `win` |
| the release, signing and symbol scripts | the Windows host, run by a human |
| `zenith_tool_fuzz_target` targets and their smokes | `win-asan` (the declaration is recorded in `win`, `win-asan` and `linux-tsan`) |
| `HOST_PWSH` script-mode checks | `win` |

## Public API

### API surface kinds

SHIP's public surface is CMake functions, PowerShell commands and modules, and file formats. Each command takes named parameters only, writes UTF-8 without a BOM and LF line endings, prints diagnostics in the format of § "Message formats", and returns one of the exit codes of § "Script exit codes".

### CMake functions

```cmake
zenith_add_library(<Name> … [TEST_THIRD_PARTY <ManifestName>...] [CONFORMANCE_LABELS <Label>...])
zenith_add_tool(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [TEST_THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...])
zenith_add_benchmark(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...] [SLANG_SHADERS <Directory>] [SLANG_ALTERNATIVES <1|2|3>...])
zenith_add_integration_tests(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...] [TEST_LABELS <Label>...])
zenith_apply_flags(<Target> [THIRD_PARTY] [EXCEPTIONS])
zenith_stage_runtime_files(<ExecutableTarget> [TEST])
zenith_add_licence_inventory(<Game>)
zenith_third_party_add_all()
zenith_manifest_validate(<JsonText> <OutErrorsVariable>)
_zenith_ship_registry_tail()
zenith_register_script_test(<TestName> SCRIPT <Path.cmake> [HOST_PWSH] [ARGS <-Dvar=value>...] [LABELS <Label>...] [TIMEOUT <Seconds>] [CONFIGURATIONS <Config>...])
zenith_register_pwsh_test(<TestName> SCRIPT <Path.ps1> [ARGS <Arg>...] [LABELS <Label>...] [TIMEOUT <Seconds>] [CONFIGURATIONS <Config>...])
zenith_register_fixture_set(<AREA> <Set> <Target> <SetupCase> <Variable> [CONSUMER_SUITES <Suite>...] [FUZZ_CONSUMERS <Stem>...])
zenith_tool_fuzz_target(<Stem> TOOL <ToolTarget> TOOL_DIR <Directory> SOURCES <Path>... DEPENDS <Library>... [THIRD_PARTY <ManifestName>...])
zenith_cook_fuzz_target(<Stem> SOURCES <Path>... DEPENDS <Library>... [THIRD_PARTY <ManifestName>...])
zenith_fuzz_smoke_args(<Stem> <FuzzDir> <OutVariable>)
```

Script modes of `ZenithThirdParty.cmake` (run only when `CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE`, so an `include()` from another script defines the functions and runs nothing): `-DZENITH_MANIFEST_CHECK=<manifest path>` (the validator, rules V1 to V40, with relative files resolved against the manifest directory's parent; exit 1 on any error) and `-DZENITH_ROOT=<dir> -DZENITH_OUT=<dir> -DZENITH_PRESET=<preset> -DZENITH_LICENCE_GAME=<Game|*> -DZENITH_LICENCE_PLATFORM=<windows|android> -DZENITH_LICENCE_CONFIG=<Config>` (the licence inventory; every argument required). Their behaviour is `Zenith2/Docs/Design/SHIP/CMakeHelpers.md`; their arguments are validated and an unknown keyword is `FATAL_ERROR "<function>(<Name>): unknown argument <ARG>"`.

### PowerShell modules

| Module | Exported functions |
|---|---|
| `Zenith2/Tools/Setup/Zenith2Env.psm1` | the table of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Zenith2Env module" |
| `Zenith2/Tools/Setup/Zenith2Release.psm1` | `Get-Zenith2ReleaseProfile [-Channel <c>]`; `Get-Zenith2UpdateSigningKey -Profile <profile>` (the decrypted Ed25519 seed for PACK's writer, after the pinned JDK has checked it against the profile's public key, `Zenith2/Docs/Design/SHIP/Release.md` → "Update manifest signing hand-off"); `Invoke-Zenith2ReleaseStateMachine -Context <hashtable> -Actions <hashtable of scriptblocks> [-Resume] [-Restart]`; `Get-Zenith2ReleaseInventory -Staging <dir>`; `Test-Zenith2WindowsPackage -Staging <dir> [-ExposureScript <path>]` and `Test-Zenith2AndroidPackage -Staging <dir> [-ExposureScript <path>]` (each returning one `{ id, result, reason }` per checklist item; `-ExposureScript` defaults to the protected `Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1` that W5, W16, W17, A6, A15 and A16 call, and only SHIP's tests pass another path); `Write-Zenith2ReleaseManifest -Staging <dir>`; `Add-Zenith2Symbols -Store <dir> -Staging <dir>`; `New-Zenith2DeterministicZip -Root <dir> -Out <file>` |

### PowerShell commands

| Command | Parameters | Specified in |
|---|---|---|
| `Setup-Zenith2.ps1` | `-Check -Only -AcceptToolchainLicenses -AcceptAndroidSdkLicenses -SkipWsl -PruneCache -Report -MirrorRoot` | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Setup-Zenith2 parameters" |
| `Enter-Zenith2Env.ps1` | none (dot-sourced) | → "Enter-Zenith2Env" |
| `Update-Zenith2Dependency.ps1` | `-Name -Version -Url [-PinsReviewed] [-MirrorRoot]` | → "Update-Zenith2Dependency" |
| `Update-Zenith2Toolchain.ps1` | `-Key -Version [-Url <host>=<url>…] [-MirrorRoot]` | → "Update-Zenith2Dependency" |
| `Update-Zenith2GradleVerification.ps1` | none | → "Offline Gradle home" |
| `New-Zenith2Release.ps1` | `-Game -Channel [-Platforms] [-Configuration] [-Unsigned] [-Publish] [-SkipReproducibility] [-Resume] [-Restart]` | `Zenith2/Docs/Design/SHIP/Release.md` → "New-Zenith2Release" |
| `Sign-Zenith2Windows.ps1` | `-Path <file>… -Profile <path>` | → "Windows Authenticode" |
| `Sign-Zenith2Android.ps1` | `-Apk <file> -Out <file> -Profile <path>` or `-Bundle <file> -Out <file> -Profile <path>` | → "Android APK signing", "App Bundle upload signing" |
| `Test-Zenith2Release.ps1` | `-Staging <dir> [-Report <file>]` | `Zenith2/Docs/Design/SHIP/Qualification.md` → "Qualification record" |
| `Test-Zenith2Reproducibility.ps1` | `-Game [-Configuration] [-Platforms] [-Mode] [-OtherRoot] [-Report]` | `Zenith2/Docs/Design/SHIP/Release.md` → "Test-Zenith2Reproducibility" |
| `Resolve-Zenith2Address.ps1`, `Symbolize-Leaks.ps1`, `Remove-Zenith2Symbols.ps1` | as their sections | → "Symbolization tools", "Retention policy" |
| `New-Zenith2ReleaseNotes.ps1`, `New-Zenith2UpgradeGuide.ps1`, `Test-Zenith2ReleaseDocs.ps1` | `-Release <release.json>`; `-From -To -FixVersion`; `-Path [-Full]` | → "Release notes", "Upgrade guides", "Release documentation lint" |

### Script exit codes

```text
Setup-Zenith2.ps1:            0 ok · 1 mismatch or failed verification · 2 usage · 3 missing prerequisite · 4 licence switch required
Update-Zenith2Dependency.ps1: 0 ok · 1 download, digest or extraction failure · 2 usage · 5 licence text changed · 6 pins not reviewed · 7 a patch does not apply
New-Zenith2Release.ps1:       0 done · 1 precondition · 2 build or packaging · 3 manifest or verification · 4 signing · 5 symbols or publish · 6 usage
Test-Zenith2Release.ps1:      0 every automated item passes · 3 any item fails · 2 usage
Test-Zenith2Reproducibility.ps1, Test-Zenith2ReleaseDocs.ps1, Remove-Zenith2Symbols.ps1, the Tests scripts: 0 pass · 1 fail · 2 usage
Sign-Zenith2Windows.ps1, Sign-Zenith2Android.ps1: 0 signed and verified · 1 signing or verification failure · 2 usage
```

### Handle kinds used

None: SHIP has no C++ API (`Zenith2/Docs/Contracts.md` → "Handle kinds" applies to no SHIP type).

## Data structures and formats

### Formats index

| Format | Schema string | Version | Specified in |
|---|---|---|---|
| `Zenith2/ThirdParty/manifest.json` | `zenith2.third-party-manifest` | 1 | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Top-level shape" |
| the cache stamp `*.zenith2-stamp.json` | `zenith2.cache-stamp` | 1 | → "Stamp check at configure" |
| `licence_policy.json` | `zenith2.licence-policy` | 1 | → "Licence fields" |
| `third_party_targets.json` | records listed | — | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "End-of-build outputs" |
| `third_party_pending_owners.json` | records listed | — | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules" |
| `declared_deps.json` additions | `zenith2.declared-deps` | 1 (unchanged) | → "declared_deps additions" |
| the release profile | `zenith2.release-profile` | 1 | `Zenith2/Docs/Design/SHIP/Release.md` → "Key custody" |
| `release.json` | `zenith2.release` | 1 | → "release.json schema" |
| `reports/unsigned.json` of a release run | records listed | — | → "New-Zenith2Release" |
| the release archive `index.json` | records listed | — | → "Release archive index" |
| `state.json` of a release run | `zenith2.release-state` | 1 | § "Release pipeline state machine" |
| `licences.json` | `zenith2.licence-inventory` | 1 | → "licences.json format" |
| `THIRD_PARTY_NOTICES.txt` | — | — | → "THIRD_PARTY_NOTICES.txt format" |
| the symbol store `index.json` | rows listed | — | → "Symbol store layout" |
| `Qualification.json` | `zenith2.qualification` | 1 | `Zenith2/Docs/Design/SHIP/Qualification.md` → "Qualification record" |
| `compatibility_tests.json` | `zenith2.compatibility-tests` | 1 | `Zenith2/Docs/Design/SHIP/Release.md` → "Compatibility labels" |
| `Zenith2/ThirdParty/gradle/maven-artefacts.json` | `zenith2.maven-artefacts` | 1 | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Maven artefact rows" |

Every JSON file SHIP writes is written by `Write-Zenith2Json` (or the CMake equivalent with keys in specified order): tab indentation, LF, no BOM, rewritten only when its bytes change. A reader of any of these files refuses an unknown `schemaVersion` with `<file>: schemaVersion <n> is not supported`.

`state.json`: `{ "schema": "zenith2.release-state", "schemaVersion": 1, "arguments": { … the command line … }, "completed": ["PRECONDITIONS", …], "failed": null | { "state", "exitCode", "reason" } }`.

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Failure behaviour | Specified in |
|---|---|---|---|
| canonical JSON and the section hashes | O(n log n) in keys | — | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Canonicalisation and hashing" |
| manifest validation | O(entries × fields) | every violation collected, then the configure stops | → "Validation rules" |
| digest verification, extraction, tree digest, patching | O(bytes) | delete the partial, fail, publish nothing | → "Digest verification algorithm" |
| third-party walk with `dependsOn` ordering | O(entries + edges) | `FATAL_ERROR` at the first failing entry | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_third_party_add_all" |
| `THIRD_PARTY` name resolution | O(1) per name | the rule's message | → "Name resolution" |
| runtime staging closure | O(libraries + edges) | — | → "Runtime file staging" |
| licence inventory selection | O(libraries + edges + entries) | the build fails naming the component | `Zenith2/Docs/Design/SHIP/Release.md` → "Inventory selection algorithm" |
| reproducibility comparison | O(bytes) | report `differs` | → "Test-Zenith2Reproducibility" |
| upgrade-guide generation | O(diff) | — | → "Upgrade guides" |

### Release pipeline state machine

States in order: PRECONDITIONS → BUILD → STAGE → SYMBOLS → SIGN → ARCHIVE → MANIFEST → VERIFY → PUBLISH → DONE, so every state reads only outputs an earlier state finished (the archives come from the signed tree, `release.json` lists the archives, the verification reads `release.json`, PUBLISH requires the verification's `pass`; actions in `Zenith2/Docs/Design/SHIP/Release.md` → "New-Zenith2Release").

| State | Event | Next state | Action |
|---|---|---|---|
| any state S | action succeeds | the state after S (SIGN skipped with `-Unsigned`; PUBLISH skipped without `-Publish`) | append S to `completed` |
| any state S | action fails with exit code k | FAILED(S) | write `failed = { S, k, reason }`; exit k |
| FAILED(S) | re-run with `-Resume` and identical arguments | S | clear `failed`; the states in `completed` are not repeated |
| FAILED(S) | re-run with `-Resume` and different arguments | FAILED(S) | exit 6 (`arguments differ from the interrupted run; use -Restart`) |
| any | re-run with `-Restart` | PRECONDITIONS | delete the staging directory, then start |
| DONE | re-run | DONE | exit 0 without work when `release.json` exists and verifies; otherwise exit 1 (`staging directory modified after DONE`) |

An interruption (the process killed) leaves `state.json` naming the last completed state, which `-Resume` treats as FAILED of the following state.

### Dependency cache entry state machine

| State | Event | Next | Action |
|---|---|---|---|
| ABSENT | setup for the entry | DOWNLOADING | create `.partial\<name>-<sha256>.download` |
| DOWNLOADING | digest matches | EXTRACTING | — |
| DOWNLOADING | digest mismatch or download error | ABSENT | delete the download; exit 1 |
| EXTRACTING | members valid, extracted | PATCHING | — |
| EXTRACTING | a member rejected | ABSENT | delete the extraction |
| PATCHING | all patches apply | VERIFYING | — |
| PATCHING | a patch fails or mismatches | ABSENT | delete the extraction |
| VERIFYING | licence hashes match | PUBLISHED | write the stamp; rename the stamp into place, then the tree |
| VERIFYING | licence mismatch | ABSENT | delete the extraction |
| PUBLISHED | `-Check` re-hash equal | PUBLISHED | report `ok` |
| PUBLISHED | `-Check` re-hash differs | MODIFIED | report `tree modified`; exit 1 |
| MODIFIED | `-PruneCache` (unreferenced) or a human deletes it | ABSENT | — |

A published tree is never rewritten in place. A different dependency source, patch set or prebuilt archive resolves to a different directory; a toolchain tree is keyed by version alone, so a re-pinned archive of the same version is refused by its stamp until the human removes the old tree (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Cache layout"). An orphan stamp left by an interrupted publication is treated as ABSENT by the next setup, which deletes it first.

## Threading lifetime and error contract

### Concurrency of SHIP tools

- `Setup-Zenith2.ps1` holds the machine-wide `locks\setup.lock` for its whole run, so two setups (two lanes, a developer and CI) never write the cache at once; configures read the cache without the lock, because published trees are immutable and appear by atomic rename.
- `New-Zenith2Release.ps1` runs its builds while holding the machine-wide build slot (`Tools/build_lock.ps1 -Run`, the same slot the gate steps use), so a release build never races a lane's build.
- The CMake helpers run single-threaded inside the configure.

### Frame-graph nodes

None: SHIP adds no runtime code and no frame-graph node.

### Lock-order entries

None: SHIP adds no C++ lock (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract").

### Interruption and cancellation

- An interrupted setup leaves at most a `.partial\` entry, which the next run deletes before starting that entry; the published cache is unchanged.
- An interrupted release resumes through § "Release pipeline state machine".
- An interrupted signing leaves the unsigned input untouched (every signing tool writes a new output file), and the SIGN state reruns from the unsigned input.

### Error enumerations

SHIP's error surface is its exit codes (§ "Script exit codes") and its configure messages; it defines no `Zenith_Result` error type.

### Luau errors versus asserts

Not applicable: SHIP has no Luau surface and no engine code. Scripts never assert; every failure is a message and an exit code.

## Diagnostics and profiling

### Message formats

| Source | Format |
|---|---|
| configure (`CMake/*.cmake`) | `Zenith2: <subject>: <what>` at `FATAL_ERROR`, `SEVERE_ERROR` or `STATUS` (the manifest validator's form is `Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation error messages") |
| setup | `Zenith2 setup: <name> <version>: <what>`; one summary line per item (`ok`, `installed`, the failure) |
| release | `Zenith2 release: <state>: <what>` |
| verification | `<item id>: <pass|fail>: <reason>` |

### Reports

| Report | Path |
|---|---|
| setup report | `${ZENITH_OUT}/setup/setup-report.json` (`{ items: [{ key, kind, version, result, detail }] }`) |
| release verification | `<staging>/reports/verify.json` |
| reproducibility | `<staging>/reports/reproducibility.json` |
| repository licence inventory | `${ZENITH_OUT}/licences/repository.json` |
| the configure's third-party line | `Zenith2: third party: <n> entries, <m> targets, <k> skipped for <os>` |

### Profiler and telemetry

SHIP adds no profiler row, counter or telemetry. The Build step's `sccache --show-stats` line ([Z2-N-ARCH-233]) is the one build-performance signal, and SHIP's toolchain files keep sccache the launcher for every compiler.

## Authoring and cooking

SHIP has no editor surface, no importer and no cook step. Its authored inputs are the files under `Zenith2/ThirdParty/` (the manifest, the licence policy, patches, the Gradle verification metadata), `compatibility_tests.json` and the release documents under `Zenith2/Docs/Release/`; each is edited by a person in an ordinary diff and validated by the checks named in its section. Content licences come from the cook's provenance output (CONTENT, pending).

## Failure handling

### Missing or corrupt dependency inputs

| Case | Behaviour |
|---|---|
| a cache directory is absent | the configure stops with the setup message; nothing is downloaded |
| a cache stamp disagrees with the manifest | the configure stops with the stamp message |
| a published tree was modified | `-Check` reports `tree modified` and exits 1; the configure is unaffected (it does not re-hash) |
| the manifest is invalid | every violation printed; the configure stops before any target exists |
| a licence file changed in the archive | setup refuses to publish; `Update-Zenith2Dependency.ps1` exits 5 |
| a manifest owner is not declared yet (between a pinning ticket and its owner's ticket) | the configure continues; one `STATUS` line per owner, a record in `third_party_pending_owners.json`; a case-insensitive near miss of a declared name stops the configure (rule K4) |
| a `zenith-wrapper` entry's `ThirdParty/<name>/CMakeLists.txt` is absent | the entry creates no target, one `STATUS` line; a `THIRD_PARTY` naming it fails rule 1 |

### Network and download failures

A download that fails, redirects more than five times or returns a status other than 200 deletes its partial file and exits 1; the setup is restartable and skips published entries. No configure, build, gate step or CI job downloads.

### Allocation failure

Not applicable: SHIP has no runtime allocation. A host out of disk space fails the step that writes (setup, release) with the operating system's error in its message, and the state machines leave no partial publication.

### Device loss

Not applicable to SHIP's tools. A device loss during a qualification run is the RHI's recovery path; the run's evidence record is invalid (BR-INVALID) and the run is repeated.

### Interrupted runs

See § "Interruption and cancellation".

### Budget breach

A breach of a SHIP row measured at qualification follows its breach code (`Zenith2/Docs/Budgets.md` → "Breach consequences"): BR-ACCEPT rows block the Stage 6 acceptance and file a BUG on the owning area's epic; BR-INVALID rows invalidate the run; Z2-B-SHIP-012 is BR-REPORT.

### Signing and symbol failures

| Case | Behaviour |
|---|---|
| a password environment variable is unset | SIGN fails, exit 4, naming the variable; `Get-Zenith2UpdateSigningKey` fails the same way for the update key |
| the update key file is not an encrypted PKCS#8 key, or the Authenticode key is exportable | `Get-Zenith2ReleaseProfile` fails; PRECONDITIONS exits 1 |
| the verification did not pass | PUBLISH does not run; the run stops at VERIFY with exit 3 |
| the certificate thumbprint or key digest differs from the profile | SIGN fails, exit 4 |
| a symbol file's identity does not match its binary | SYMBOLS fails, exit 5; nothing is published |
| the symbol store holds a different file at a path | PUBLISH fails, exit 5 |
| a forbidden symbol survives in a Shipping binary | VERIFY fails (W5 or A6), exit 3 |

## Test strategy

Every SHIP test is listed with its file, case and gate step in `Zenith2/Docs/Design/SHIP/Tests.md`: the CMake script-mode checks (`CMake_Ship_*`) of the helpers, the manifest validator and the licence inventory; the PowerShell checks (`Pwsh_Ship_*`) of setup, signing, symbols, release documents, the compatibility list and the CI workflow; `CompileFail_Ship_DeprecatedUseIsError` (registered through CORE's `zenith_core_compile_fail`); and the registry tail's discovered death cases, which run every death case no block registered. They run in the `Test` step. Release-time and device checks are the Stage 6 qualification items of `Zenith2/Docs/Design/SHIP/Qualification.md`, run by a human in the stage acceptance, never by a worker (the worker-evidence ruling). Binary test inputs (archives, keystores, certificates, key files, git repositories) are generated at test time; committed fixtures are text (Q55); no fixture needs a worker to compute or record a digest (`Zenith2/Docs/Design/SHIP/Tests.md` → "Fixture digests").

## Budget rows

SHIP owns these rows of `Zenith2/Docs/Budgets.md` and measures them at qualification (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Measured rows at qualification"):

| Row | Name | Status |
|---|---|---|
| Z2-B-SHIP-001, Z2-B-SHIP-002 | Frame time p95, p99 | adopted |
| Z2-B-SHIP-003, Z2-B-SHIP-004, Z2-B-SHIP-005 | Initial-load exclusion window; minimum run duration; minimum frame count | adopted |
| Z2-B-SHIP-006 to Z2-B-SHIP-010 | Sustained floor and sustained run conditions | adopted |
| Z2-B-SHIP-011 | Dynamic-resolution floor | adopted |
| Z2-B-SHIP-012 | Hitch threshold | adopted |
| Z2-B-SHIP-013, Z2-B-SHIP-014 | Cold boot Windows, S24 | adopted |
| Z2-B-SHIP-015 to Z2-B-SHIP-018 | Hitch count and maximum frame time, warm and cold caches | pending, allocated by the `needs-human` ruling TASK "Stage 3 per-system budget ticket for SHIP" (Z2T-SHIP-040, `ready`), in the Stage 3 sprint under the orchestrator's ruling X365-SHIP-BUDGET-S3 (cross-document item X365 applied in core-doc patch 2026-10-16g: `Zenith2/Docs/Budgets.md` → "Producer tickets" names it, stage 3, [Z2-N-BUDGET-482]); its Z2-D-084 harness half is DEVT's workload runner (`Zenith2/Benchmarks/Run-Workload.ps1`, Z2T-DEVT-097 and -098, both Stage 3) over SHIP's `RowSources.json` entries (Z2T-SHIP-108), so SHIP files no harness ticket of its own |
| Z2-B-SHIP-019 | Open crash or blocker defects per slice | adopted |
| Z2-B-SHIP-020, -021, -022 | Severity-weighted open-defect caps | pending, allocated by the Stage 1 flagship acceptance tickets |
| Z2-B-SHIP-023 | Playthrough script duration | adopted |

The CI jobs and the setup script are held to the gate-time rows Z2-B-PROC-007 to Z2-B-PROC-031, which PROC owns; SHIP needs no new row.

## Stage plan

### Stage 0

Implemented by Roadmap Stage 0 deliverables under the PROC epic, against this design (`Zenith2/Docs/Roadmap.md` → "Stage 0 Tooling bring-up"), in the order of § "Stage 0 bootstrapping order". Z2-D-128 makes the scaffold the **last** Stage 0 ticket, so no SHIP loop ticket is a Stage 0 ticket:
- deliverable 14 (`needs-human`, the human downloads and verifies): `Zenith2/ThirdParty/manifest.json` with the full `toolchain` section (every key with its `licence`, `vulkanProfilesGenerator` `null`) and the doctest entry as its only `dependencies` element; `licence_policy.json` with its admitting ruling (Z2-R-SHIP-132), including `allowedToolchain`; the two committed licence texts under `Zenith2/ThirdParty/licences/` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain runtime components"); `Setup-Zenith2.ps1` (digest verification, extraction, stamps, `-Check`, the licence switches, `-MirrorRoot`, the bootstrap checks of step 2), `Enter-Zenith2Env.ps1`, `Zenith2Env.psm1`, the pinned sccache. The Linux host mode, WSL provisioning, the Android SDK and the Gradle home are SHIP tickets of Stage 1 and Stage 2 (below);
- deliverable 11 (`needs-human`, protected): `zenith2-gates.yml` per the CI matrix;
- the scaffold (Z2T-MOD-056, deliverable 17, the last Stage 0 ticket) lands the first helpers.

Stage 0's SHIP evidence is what the Stage 0 acceptance can produce without a SHIP test: the setup report of `Setup-Zenith2.ps1 -Check` on the acceptance machine and the CI runners, the committed files, and the green CI run (Z2-R-SHIP-142, Z2-R-SHIP-143, Z2-R-SHIP-132). Every SHIP test of the deliverable-14 code is registered by the first SHIP ticket of Stage 1.

API-first slices: the manifest schema and the bootstrap checks land before any consumer entry beyond doctest; the doctest entry is the one dependency the scaffold's reader, its three third-party rows and its `header-only` mode can carry green.

### Stage 0 bootstrapping order

<!-- elaboration -->
Roadmap deliverable 14 (the setup script) lands before deliverable 17 (the scaffold), and the scaffold is the last Stage 0 ticket (Z2-D-128), so from deliverable 14 until the first SHIP ticket of Stage 1 the setup script exists without SHIP's validator and without SHIP's tests:

1. Deliverable 14 lands the setup script, whose step 2 detects the absent validator and runs `Test-Zenith2ManifestBootstrap` (V1 to V4) with the message of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Setup steps"; a bootstrap failure exits 1, a pass continues, so the human can provision the Stage 0 machines.
2. The scaffold creates `ZenithThirdParty.cmake` with its Stage 0 reader and the registry file `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`. The Stage 0 reader holds no validator function, so setup step 2 keeps running the bootstrap checks (it tests for the function `zenith_manifest_validate` in the file, not for the file's existence).
3. Stage 1 opens. Its first SHIP ticket, T1 of § "Stage 1" ("SHIP manifest validator and setup tests", `blocked_by` Z2T-MOD-056), adds the validator and registers every test of the deliverable-14 files in the SHIP block of the registry. SHIP's A5 batch splits T1 into Z2T-SHIP-001 to -010 (the registry functions and runner, the deliverable-14 setup and manifest tests, the validator in three parts, the canonical form and the CI workflow shape test), so T1 lands as a sequence of diffs rather than one; setup step 2 runs the full validator in script mode from the first validator ticket (Z2T-SHIP-006) on, and every rule V1 to V40 applies once the last of them (Z2T-SHIP-008) is Done.

**Recorded exception to Zenith2 non-negotiable 10** ("tests in the same ticket as the code"), under Z2-D-128: the deliverable-14 code lands in a `needs-human` PROC ticket of Stage 0 before any test registry exists, so its tests cannot be registered in that diff, and Z2-D-128 closes Stage 0 with the scaffold, so the ticket that registers them is in Stage 1. The exception therefore spans from deliverable 14 (Stage 0) to T1 (the first SHIP tickets of Stage 1), covers the deliverable-14 files alone, and closes when the last T1 ticket that tests a deliverable-14 file (Z2T-SHIP-001 to -005, -010) is Done, each naming the deliverable-14 files it tests. The Stage 0 acceptance proves the deliverable-14 files by running them (Z2-R-SHIP-142); the Stage 1 acceptance proves them by their tests.

### Stage 1

Pulled forward because Stage 1 harnesses, the D-SLANG probe, MOD's first APKs and the first budget runs need it. The SHIP tickets land in this order; each is `blocked_by` the tickets it names, every one is `blocked_by` the scaffold (Z2T-MOD-056), and each stays `draft` only while a row of § "Reconciliations with the core documents" it implements is pending. The column "Blocks" names the other areas' tickets (and SHIP's own pinning tickets, Z2T-SHIP-043 to -062) that need each SHIP ticket; SHIP's A5 batch writes each as a `blocks` link from the SHIP ticket to the consumer, whose reason names the mechanism of the cell, so no signed-off ticket's text changes and a consumer that stays `draft` "until a producer ticket is linked" becomes `ready` when the link lands:

| # | SHIP ticket | Contents | `blocked_by` | Blocks (links written by SHIP's A5 batch; no signed-off ticket's text changes) |
|---|---|---|---|---|
| T1 | SHIP manifest validator and setup tests | the validator (V1 to V40) in `zenith_begin_build` and in script mode; `Pwsh_Ship_*` setup and manifest tests of the deliverable-14 code; `CMake_Ship_ManifestValid_Committed`, `CMake_Ship_ManifestRule_*`; `zenith_register_script_test`, `zenith_register_pwsh_test` and the SHIP block; `Pwsh_Ship_CiWorkflowShape` | Z2T-MOD-056 | — (SHIP's T2 and T7 carry the edge as their own `blocked_by`) |
| T2 | SHIP flag hygiene and flag rows | `zenith_begin_build`'s flag hygiene and compiler pin checks (the per-configuration variables of every configuration, `ShippingTest` included, and the imported-configuration mappings), the assert define, the third-party rows (exceptions and RTTI off, the Luau exceptions-on row), the reproducibility and hardening rows, the Linux `-Wl,--build-id=sha1` | T1 | Z2T-CORE-053 (its C++ wrapper source `simdjson.cpp` compiles under the third-party rows); every pinning ticket of a compiled entry in § "Pin landing order" (the COMPRESS, SCRIPT, ECS, PLAT, RHI, DEVT and ASSET/GEOM pinning tickets, and the PHYS, EDIT and AUDIO ones of Stages 2 to 5) |
| T3 | SHIP THIRD_PARTY completion | name resolution rules 1 to 6 with rule 2's platform skip, link and include rules, Shipping exclusion over `Shipping` and `ShippingTest`, target mapping and the Luau closure rule, `dependsOn`, `definitions`, pin header templates, the ownership rules K1 to K5 with owners declared after their pins, the reader that replaces the scaffold's (it accepts `fromToolchain` entries without `source`), a `zenith-wrapper` entry whose wrapper file is absent creating no target, `third_party_targets.json`, `third_party_pending_owners.json`, `ZENITH_DECL_<Lib>_THIRD_PARTY`, the exposed variables, `declared_deps.json` additions | T2 | Z2T-CORE-053; every pinning ticket of § "Pin landing order" (those of T2's cell, the test-infrastructure `nanobench` pinning ticket, the Slang and DXC pinning ticket and the RPROD test-pin ticket) |
| T4 | SHIP prebuilt and host-tool modes | the `prebuilt` and `host-tool` modes, `fromToolchain`, runtime file staging (`TEST`, the `Shipping` and `ShippingTest` selection of `pins.shippingRuntime`), `test-runs` staging | T3 | the RHI pinning ticket (its `prebuilt` and `fromToolchain` entries) and the Slang and DXC pinning ticket (`host-tool`) |
| T5 | SHIP test targets and registry tail | `TEST_THIRD_PARTY`, the discovery labels of [Z2-N-ARCH-251] (`TEST_LABELS`, `CONFORMANCE_LABELS`, the integration suites' `TEST_LABELS`, doctest suite names through `ADD_LABELS`), the conformance executables with `ZENITH_TEST_LINKS_<TARGET>` (EB1, SC13, X197); the per-preset `TEST_DEPENDS` linking of test targets (RB21): `<Name>_Tests` of every library (a tool has no `TEST_DEPENDS` argument) links only the `TEST_DEPENDS` entries the preset supports, skips the rest with no configure error and without removing `<Name>_Tests`, replacing the scaffold's unconditional link, and defines `ZENITH_TEST_LINKS_<TARGET>` (the target name upper-cased, BuildSystem's `TEST_DEPENDS` row: `ZENITH_TEST_LINKS_ZENITHRHI_D3D12`) to `1` or `0` for every entry (RB21, X197, [Z2-N-ARCH-172]; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Test third-party links"); the test environment (`ZENITH_EXPECTED_BUILD` with `shipping` in `ShippingTest`, CB4; `ZENITH_TEST_GENERATED_ROOT` with its Android device form, X262), `zenith_tests`, `zenith_register_fixture_set`, the registry tail (unlisted death-case discovery, the compile-fail lock and source check, fixture properties of area-made fuzz smokes) | T3, Z2T-CORE-002 (the registry block whose `<Target>.<Case>` and `z2-compile-fail` forms the tail reads) | Z2T-RHI-055 (cross-area row RB21: the per-preset `TEST_DEPENDS` skip and `ZENITH_TEST_LINKS_<TARGET>` of `ZenithFlux_Tests`) |
| T6 | SHIP tool, benchmark and integration-suite helpers | `THIRD_PARTY` and `TEST_THIRD_PARTY` on `zenith_add_tool`, `THIRD_PARTY` on `zenith_add_benchmark` and `zenith_add_integration_tests` (CB5, EB3), `ZENITH_DECL_<Name>_TOOL_KIND`, `ZENITH_DECL_Zenith_Bench_<Name>_BENCHMARK` and the `ZENITH_INTEGRATION_SUITES` list the registry tail enumerates, `SLANG_SHADERS`, `SLANG_ALTERNATIVES`, `ZENITH_PROBE_SLANG_VERSION` and `ZENITH_BENCH_SHADER_ALTERNATIVES` for the bindless probe | T3, T4 | Z2T-CORE-054, Z2T-CORE-056, Z2T-CORE-058 (CB5: `THIRD_PARTY` on `zenith_add_benchmark`), Z2T-ECS-005 (EB3), Z2T-RHI-001 (RB7: benchmark `THIRD_PARTY`, `SLANG_SHADERS`), Z2T-RPROD-032 (`zenith_add_tool(… THIRD_PARTY slang dxc …)`), the GB27 GEOM tickets Z2T-GEOM-029, -032, -039 (`THIRD_PARTY` on `zenith_add_tool`; the fuzz half of GB27 is the scaffold's, Z2T-MOD-056) |
| T7 | SHIP Android provisioning | the Android SDK provisioning, the offline Gradle home and its verification metadata, the development keystore, the Android exposed variables (`ZENITH_JDK_DIR`, `ZENITH_ANDROID_SDK_DIR`, `ZENITH_GRADLE_HOME`, `ZENITH_DEV_KEYSTORE`, `ZENITH_GRADLE_VERIFICATION_METADATA`), with their tests; MOD's Stage 1 Android tickets (Z2T-MOD-002, -003, -063) need them | T1 | Z2T-MOD-002, Z2T-MOD-003, Z2T-MOD-063 (they build and install APKs) |

Also in Stage 1:
- (moved to § "Stage 3 to Stage 5": cross-document item X365 is applied, core-doc patch 2026-10-16g, [Z2-N-BUDGET-482]) the `needs-human` ruling TASK "Stage 3 per-system budget ticket for SHIP" (Z2T-SHIP-040), allocating Z2-B-SHIP-015 to Z2-B-SHIP-018, `ready`;
- the human pinning tickets of § "Pin landing order", each after the SHIP tickets it needs and before the owners that wait for it;
- among them the test-infrastructure pinning ticket (`nanobench`, `needs-human`, `blocked_by` T3), SHIP's Z2T-SHIP-043, which carries the `nanobench` row of § "Pin landing order" and of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Test and benchmark entries" as its contents, and which `blocks` every benchmark ticket listed under "The nanobench pin";
- `Symbolize-Leaks.ps1` (CB12) once CORE's leak table lands;
- the `KIND GENERATED` branch of `zenith_add_library` and `ZENITH_SHADERS_GENERATED_DIR` on every test, conformance, benchmark and integration-suite target (cross-area row RPB22, which the orchestrator's RPB22-SHIP ruling gives to SHIP; Z2-R-SHIP-153; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Generated shader library", "Test environment"), Z2T-SHIP-106, `blocked_by` T5 (Z2T-SHIP-027) and T6 (Z2T-SHIP-033), fix version Stage 2, which `blocks` Z2T-RPROD-036 (the branch, which calls RPROD's `zenith_add_shaders`) and Z2T-RPROD-039, Z2T-RHI-120, Z2T-GEOM-059 and Z2T-MOD-062 (the definition).

The A5 batch splits each T-row of the table into tickets (the epic's sub-structure maps T1 to T7 onto Draft-IDs), and the order between rows holds between the groups: T2's first ticket (Z2T-SHIP-011) waits for the last T1 validator ticket (Z2T-SHIP-008), so no pin lands before every rule V1 to V40 exists.

T2 lands the reproducibility and hardening rows before any pin of a compiled dependency and before the first budget run ([Z2-N-ARCH-259]), and before SAVE's Stage 2 build identity reads `Zenith_GetRunningBuildID` in every preset ([Z2-N-ARCH-258]), so every budget allocation and evidence record from Stage 1 on is measured on the hardened Release. Gated by: D-SLANG (open, Stage 1) for which alternative the probe runs by default (not for which alternatives are compiled: all three are); D-SEL-HEAP, D-SEL-HASH, D-SEL-JSON (open, Stage 1) move candidate owners in their ruling diffs.

### Pin landing order

<!-- elaboration -->
A dependency pin is a `needs-human` diff (only a human downloads and verifies an archive), and the library that owns it is declared by an ordinary loop ticket of its area, so the two never land in one diff; every consumer hand-off asks for its pin first and keeps its owner ticket `draft` until the pin exists. SHIP's rules are therefore satisfiable at every intermediate landing: a pin waits only for the SHIP mechanisms of § "Stage 1" its mode needs, and between the pin and its owner the configure stays green because rule K4 reports an undeclared owner instead of refusing it (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules"). No dependency pin beyond doctest lands in deliverable 14: the scaffold's Stage 0 reader requires `source` on every entry, stops on a `zenith-wrapper` entry without its wrapper file, and applies only three third-party rows, which leave exceptions and RTTI on in a third-party compile group that `ForbiddenCheck` (F17, [Z2-N-SCRIPT-028]) refuses. The hand-offs that name "the Stage 0 reproducibility-manifest ticket" (COMPRESS-P1, ECS-P1, PLAT-P5, RHI-P2, RPROD-P1, SCRIPT-P1) are therefore executed by the pinning tickets below, which are SHIP's own `needs-human` tickets Z2T-SHIP-043 to -062 (the CORE selection candidates stay Z2T-CORE-053), each carrying its hand-off's contents; PROC files none of them. Each pinning ticket is linked `blocks` to the owner tickets listed (their bodies already wait for "the pin"), so no owner ticket's text changes.

| Pins | Mode | Pinning ticket (stage), `blocked_by` | Owner, declared by | Configure between the pin and the owner |
|---|---|---|---|---|
| `doctest` | `header-only` | deliverable 14 (Stage 0) | token `tests` (no declaration) | green: the scaffold's `header-only` mode |
| `nanobench` | `header-only` (`pins.includeDir: "src/include"`), token `benchmark`, `autoLink` | the test-infrastructure pinning ticket (Stage 1, Z2T-SHIP-043), T3 (`pins.includeDir`, which the scaffold's `header-only` mode ignores, so the tree root would not resolve `<nanobench.h>`) | token `benchmark` (no declaration) | green; before it, a `zenith_add_benchmark` target has no `nanobench` target to link, so every benchmark ticket is linked `blocked_by` it ("The nanobench pin" below) |
| `zlib` | `zenith-wrapper` | Z2T-SHIP-044 (Stage 1, hand-off COMPRESS-P1), T2, T3 | `ZenithCompression_Zlib`, Z2T-COMPRESS-007 (Stage 1), which also adds `Zenith2/ThirdParty/zlib/CMakeLists.txt` | green: the wrapper file is absent until Z2T-COMPRESS-007, so T3's absent-wrapper rule prints `Zenith2: zlib <version>: ThirdParty/zlib/CMakeLists.txt is not present yet; no target created` and K4 reports the owner; Z2T-COMPRESS-007 adds the file and its `THIRD_PARTY zlib` in one diff |
| `zstd` | `cmake-subproject` | Z2T-SHIP-044 | `ZenithCompression_Zstd`, Z2T-COMPRESS-008 (Stage 1) | green: the subproject is added `EXCLUDE_FROM_ALL` with T2's rows, nothing links it, K4 reports the owner |
| `luau` | `cmake-subproject` with `targets` | Z2T-SHIP-045 (Stage 1, hand-off SCRIPT-P1), T2, T3 | `ZenithScript`, Z2T-SCRIPT-002; `ZenithScriptCompiler`, Z2T-SCRIPT-003 (both Stage 1; Z2T-SCRIPT-004 and -006 name the entry); `host` elements need no declaration | green: T3's target-existence check and closure rule hold on the pinned revision, K4 reports both owners until each is declared |
| `flecs`, `entt` | `cmake-subproject`, `header-only` | Z2T-SHIP-046 (Stage 1, hand-off ECS-P1), T2, T3 | `ZenithECS_Flecs`, Z2T-ECS-073; `ZenithECS_EnTT`, Z2T-ECS-078 (both Stage 2) | green; the Stage 1 harness Z2T-ECS-005 names both through its benchmark `THIRD_PARTY`, which rule 3 admits whatever the owner's state |
| `sdl3` | `cmake-subproject` with `targets` | Z2T-SHIP-047 (Stage 1, hand-off PLAT-P5), T2, T3 | `ZenithPlatform`, Z2T-PLAT-005 (Stage 1) | green; MOD's Z2T-MOD-002 reads `pins.androidJavaDir` without linking |
| `vulkan-headers`, `vulkan-profiles` (with the `vulkanProfilesGenerator` pin, V31), `volk`, `vma`, `renderdoc-app`, `d3d12ma` | `header-only`, `zenith-wrapper` (the `volk`, `vma` and `d3d12ma` wrapper files in the pinning diff) | Z2T-SHIP-048 (Stage 1, hand-off RHI-P2), T2, T3 | `ZenithRHI_Vulkan`, Z2T-RHI-026; `ZenithRHI_D3D12`, Z2T-RHI-038 (both Stage 1); `renderdoc-app`'s owner array names both | green: K3 checks only the declared elements of the owner array; the probe Z2T-RHI-001 names `vulkan-headers` and `volk` through its benchmark `THIRD_PARTY` |
| `d3d12-agility-sdk`, `warp`, `lavapipe`, `vulkan-validation-layers` | `prebuilt` (`warp`, `lavapipe` through `fromToolchain`) | Z2T-SHIP-048, which therefore also waits for T4 | `ZenithRHI_D3D12`, Z2T-RHI-038; the token `test-runs` | green; the probe's `win` build links `d3d12-agility-sdk` and rule 2 skips it on `android-arm64`; WARP and lavapipe are staged next to test executables by T4 |
| `slang`, `dxc` | `host-tool` through `fromToolchain` | Z2T-SHIP-049 (Stage 1, hand-offs RPROD-P1 and RHI-P2's Slang half, one entry each), T3, T4 | `ZenithShaderCompiler`, Z2T-RPROD-032 (Stage 2) | green: the imported `slang::slangc` exists from the pin on, which the probe's `SLANG_SHADERS` commands (T6) run long before the owner exists; K4 reports the owner |
| `mimalloc`, `xxhash`, `yyjson`, `simdjson` | as § "Selection-candidate entries" of `Zenith2/Docs/Design/SHIP/Manifest.md` | Z2T-CORE-053 (Stage 1, `needs-human`), T2, T3 | `Zenith_Bench_HeapSelection`, Z2T-CORE-054; `Zenith_Bench_HashSelection`, Z2T-CORE-056; `Zenith_Bench_JsonSelection`, Z2T-CORE-058 (all Stage 1) | green: K4 reports each harness until T6's `zenith_add_benchmark` declares it |
| `tracy` | `cmake-subproject`, `shipping: excluded` | Z2T-SHIP-050 (Stage 2, hand-off of row DV2), T2, T3 | `ZenithProfiling`, the DEVT ticket that creates it (`Zenith2/Docs/Design/DEVT.md`) | green; K4 reports the owner |
| `fastgltf`, `meshoptimizer`, `ktx-software`, `mikktspace` | `cmake-subproject`, `zenith-wrapper` | Z2T-SHIP-051 (Stage 2, hand-offs ASSET-P3, GEOM-P1, row GM11; `fastgltf` `dependsOn` `simdjson`, so also after Z2T-CORE-053), T2, T3 | `fastgltf`, `meshoptimizer`: `ZenithContentCore`, Z2T-CONTENT-004 (ruling CT28B-DEP-OWNERS; under ruling MESHOPT-VIA-CONTENTCORE-2 the ZenithCook tool targets that call them directly name them in their own `THIRD_PARTY` as consumers, rule 3); `ktx-software`, `mikktspace`: `ZenithCook`, Z2T-ASSET-011 (Stage 2) | green; K4 reports each owner until declared |
| `stb-image`, `dr-wav`, `dr-flac`, `dr-mp3`, `stb-vorbis` | `header-only` | Z2T-SHIP-110 to -114 (Stage 2 sprint, fix version Stage 3, rows CT16 (a), CT28 (b)), T3 | `ZenithContentCore`, Z2T-CONTENT-004 | green; K4 reports the owner |
| `flip` | `header-only`, `usage: tests` | Z2T-SHIP-052 (Stage 2, hand-off RPROD-P4), T3 | `ZenithFlux`, Z2T-RPROD-008 (Stage 2) | green; K5 skips the undeclared owner |
| `monocypher`, `libsodium` | `zenith-wrapper` (the wrapper files in the pinning diff) | Z2T-SHIP-118 (Stage 1, row SEC15), T2, T3, the licence ruling Z2T-SHIP-117 (`ISC`) | `Zenith_Bench_Ed25519`, SEC's harness ticket (not yet drafted; it blocks on Z2T-SHIP-118) | green; K4 reports the harness until it is declared |
| `opus`, `ogg`, `vorbis` | `cmake-subproject`, `zenith-wrapper` (`vorbis`, `dependsOn` `ogg`) | Z2T-SHIP-119 (Stage 2, rows PG2, AuB17), T2, T3 | `Zenith_Bench_AudioCodec`, Z2T-AUDIO-004 | green; K4 reports the harness |
| `ffmpeg-libavcodec` | `zenith-wrapper` with recorded configuration headers | Z2T-SHIP-120 (Stage 2, row CnB2), T2, T3, Z2T-SHIP-117 (`LGPL-2.1-or-later`) | `Zenith_Bench_VideoDecoderSelection`, Z2T-CINE-010; retired or re-owned by Z2T-CINE-069 | green; K4 reports the harness |
| `harfbuzz`, `freetype`, `msdfgen`, `sheenbidi` | `zenith-wrapper`, `cmake-subproject` (`msdfgen` with `targets`) | Z2T-SHIP-121 (Stage 2, row UiB4), T2, T3, Z2T-SHIP-117 (the HarfBuzz and FreeType terms) | `ZenithUI`, its library ticket; Z2T-UI-029, -034, -035, -036 link them | green; K4 reports the owner until declared |
| `steamworks-sdk` | `prebuilt` with a manual-download item | Z2T-ONLINE-060 (Stage 6, `needs-human`, ONLINE's own pinning ticket), Z2T-SHIP-124 (the manual-download form), Z2T-SHIP-129 (the licence term) | `ZenithOnline_Steam`, Z2T-ONLINE-061 | green with or without the zip: an absent zip leaves the entry unavailable and `ZenithOnline_Steam` uncreated |
| `jolt`, `physx`, `imgui`, `miniaudio`, the upscaler candidates, the Ed25519 implementation, `rmlui` | their modes | Z2T-SHIP-057 (`jolt`, `physx`; Stage 1 sprint, fix version Stage 3), -058 (`imgui`), -059 (`miniaudio`; Stage 2), -060 (the upscaler candidates), -061 (re-owns the admitted Ed25519 candidate of -118), -062 (`rmlui`, the D-SEL-UIRT candidate; Stage 2 sprint); the UI data and ICU entries are -053 and -056; T2, T3 | PHYS, EDIT, AUDIO, LIGHT and PLAT tickets | green by the same rules |

**The nanobench pin.** Its pinning ticket `blocks` every ticket that creates a benchmark target. In the Stage 1 sprint: Z2T-ASSET-001, Z2T-COMPRESS-025, Z2T-CORE-054, Z2T-CORE-056, Z2T-CORE-058, Z2T-CORE-060, Z2T-CORE-062, Z2T-ECS-005, Z2T-GEOM-001, Z2T-GRAPH-005, Z2T-GRAPH-023, Z2T-JOBS-001, Z2T-LIGHT-001, Z2T-PLAT-001, Z2T-RHI-001, Z2T-RHI-004, Z2T-RPROD-001, Z2T-SCRIPT-012, Z2T-SCRIPT-014, Z2T-SCRIPT-016. In the Stage 2 to Stage 5 sprints: Z2T-ASSET-071, Z2T-ECS-088, Z2T-ECS-089, Z2T-ECS-090, Z2T-GEOM-059, Z2T-GRAPH-045, Z2T-JOBS-012, Z2T-JOBS-036, Z2T-LIGHT-116, Z2T-PLAT-074, Z2T-PLAT-110, Z2T-RPROD-121, Z2T-RPROD-123, Z2T-SCENE-054, Z2T-SCRIPT-078, Z2T-SCRIPT-113. SHIP's A5 batch writes these links with the pinning ticket's Draft-ID, like the "Blocks" column of § "Stage 1". The link into each `Zenith_Bench_<Name>` is made, before T6, by the scaffold's `zenith_add_benchmark` (BuildSystem's `zenith_add_benchmark` row, "executable `Zenith_Bench_<Name>` linking nanobench", a link by the target name `nanobench`), and from T6 on by SHIP's `zenith_add_benchmark` through the `autoLink` rule: every entry with `owner: "benchmark"` and `autoLink: true` that the manifest has for the preset's platform is linked into every benchmark (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Link and include rules"; nanobench is the one such entry).

**The order of Z2T-CORE-053.** Its readiness rule (row CB6: `ready` once the deliverable-14 Draft-ID joins its `blocked_by`) is not sufficient on its own. Before T2, `simdjson.cpp` (a C++ wrapper source of Z2T-CORE-053) would compile under the scaffold's three third-party rows, which keep the platform `CMAKE_CXX_FLAGS` `/GR /EHsc` that `ForbiddenCheck` F17 refuses on every subsequent diff; before T3, the scaffold's reader and `header-only` mode cannot carry its entries. SHIP's A5 batch therefore links T2 and T3 `blocks` Z2T-CORE-053 (column "Blocks" of § "Stage 1"), and Z2T-CORE-053 is not taken before both are Done.

The pending-owner record `${ZENITH_OUT}/third_party_pending_owners.json` names, at any commit, exactly the owners of this table whose declaring ticket has not landed; each stage acceptance from Stage 1 compares it with the table (an owner whose ticket is Done and still pending is a defect, filed as a BUG on the owner's epic).

### Stage 2

The fix version of Z2-I-SHIP-01: WSL provisioning and the native Linux host mode (with the `Sanitizers -Preset linux-tsan` activation of Z2-D-128's Stage 2 gate growth, which first needs a Linux host), and the Stage 2 acceptance's clean-machine setup and sanitizer builds from source. The two SHIP tickets, in the form of § "Stage 1"; both (Z2T-SHIP-064 and -065) are scheduled in the Stage 1 sprint with fix version Stage 2, because DEVT's Stage 1 `linux-tsan` bring-up (Z2T-DEVT-010) waits for them, and the needs-human provisioning Z2T-SHIP-066 stays in Stage 2:

| # | SHIP ticket | Contents | `blocked_by` | Blocks (links written by SHIP's A5 batch; no signed-off ticket's text changes) |
|---|---|---|---|---|
| T8 | SHIP Linux host mode | the native Linux host mode of `Setup-Zenith2.ps1` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Linux host mode", "Linux package snapshot", "Linux archive extraction"): `Install-Zenith2LinuxHost`, `Get-Zenith2LinuxCommandPlan`, `Expand-Zenith2LinuxArchive`, the Linux key set of `-Check` run on a Linux host (the key selection itself is `Get-Zenith2HostKeys`, which T1's `Pwsh_Ship_Setup_CheckHostKeys` already covers), with `Pwsh_Ship_LinuxHost_CommandPlan` (registered here with its `Native` cases) and `Pwsh_Ship_LinuxArchive_MemberRules` (Z2-R-SHIP-133) | T1 | T9 |
| T9 | SHIP WSL provisioning | the WSL step of a Windows host (`Zenith2/Docs/Design/SHIP/Manifest.md` → "WSL provisioning"): the distro import, the root and user steps through `wsl.exe -u root` and `-u zenith`, the `pwsh` bootstrap of step 5, the reuse of T8's `Install-Zenith2LinuxHost` for steps 3 to 7, extending `Pwsh_Ship_LinuxHost_CommandPlan` by its `Wsl` cases (Z2-R-SHIP-035) | T8 | the PROC `needs-human` ticket that activates `Sanitizers -Preset linux-tsan` (Z2-D-128's Stage 2 gate growth), which first needs a provisioned Linux host; DEVT's `Test-LinuxTsanHost.ps1` run of step 7 (row DV16 (b)) |

### Stage 3 to Stage 5

Five SHIP tickets are scheduled in the Stage 3 sprint: 040, 107, 108 and 109 below (fix version Stage 3), and the `imgui` pin Z2T-SHIP-058 of § "Pin landing order":
- the hitch-row budget ruling Z2T-SHIP-040 (Z2-B-SHIP-015 to -018), behind DEVT's `Run-Workload.ps1` (Z2T-DEVT-097, -098), once the WS-FRAME workloads have content to measure (ruling X365-SHIP-BUDGET-S3; the rows serve Z2-R-SHIP-119, Stage 6);
- SHIP's `RowSources.json` entries Z2T-SHIP-108 (ruling DEVT-AREA-REGISTRATION: each area registers its own rows; DEVT row DvB13 (d)), which `blocks` 040 and the Stage 6 measurement Z2T-SHIP-102;
- the git-lfs and content merge-driver setup step Z2T-SHIP-107 (ruling GIT-LFS-PREREQ; Z2-R-SHIP-154; `Zenith2/Docs/Design/SHIP/Manifest.md` → "Git prerequisites"), which Z2T-CONTENT-003 (the `.gitattributes` LFS block) and Z2T-CONTENT-011 (the recorded decoder fixtures) wait for (rows CT32, CT16 (b)), and its needs-human record on every gate host and lane, Z2T-SHIP-109.

Otherwise dependency pins requested by Stage 2 to 5 consumers are added by SHIP's pinning tickets of § "Pin landing order" against the schema (the post-signoff patch of 2026-10-17 adds the licence ruling Z2T-SHIP-117 and the pins Z2T-SHIP-118 to -121, Stage 1 and Stage 2 sprints, and the tool-side fuzz helper Z2T-SHIP-122 with the `HOST_PWSH` script tests Z2T-SHIP-123, Stage 2 sprint, before Z2T-ASSET-035; in the Stage 5 sprint, the manual-download form Z2T-SHIP-124, the Maven artefact rows Z2T-SHIP-125 and the Play and Steam licence ruling Z2T-SHIP-129); the licence inventory script and `zenith_add_licence_inventory` are pulled into the Stage 4 sprint (Z2T-SHIP-086 to -088) for the first flagship package.

### Stage 6

The release pipeline, signing and key custody, the release manifest with its reproducible form, symbols, the licence inventory and notices, the reproducibility check, dependency updates, release documents, the upgrade guides of every fix version from `Z2 Stage 2` (all written at the Stage 6 acceptance), the compatibility demonstration (its list and check `blocked_by` the SCRIPT ticket that registers `Test_Script_Binding_DeprecatedCallDiagnosed`, cross-area row SH14), the device matrix and the full qualification of all three flagship slices. The post-signoff patch of 2026-10-17 adds the Steam staging rule Z2T-SHIP-126, the exposure checks W16, W17, A15 to A18 Z2T-SHIP-127 and the update-key precondition Z2T-SHIP-128 (SEC row SEC6).

### Stage 7

Consoles add platforms to the device matrix and packaging only through their SDK-gated tickets (Z2-D-071); no SHIP requirement is deferred.

## Decisions

### Settled decisions governing SHIP

| ID | Governs |
|---|---|
| Z2-D-001 | CMake presets, globbed sources, one library per backend: the helpers |
| Z2-D-008 | the reproducibility manifest and the setup script |
| Z2-D-010 | the platforms and the device matrix |
| Z2-D-011 | dependencies built from source in sanitizer presets |
| Z2-D-012 | Budgets v1: the SHIP rows |
| Z2-D-017 | the Luau exception firewall: exceptions only on Luau targets |
| Z2-D-021 | the three flagship slices' qualification |
| Z2-D-025 | Slang and DXC offline only; the probe exemption |
| Z2-D-027 | Play distribution in Stage 6; APK and AAB signing |
| Z2-D-031, Z2-D-032 | the CI workflow and gate activation |
| Z2-D-042 | the hard gates SHIP feeds |
| Z2-D-045 | asserts for programmer errors, per configuration in every target ([Z2-N-CONTRACT-479] after the Q71 ruling) |
| Z2-D-048 | the compatibility policy demonstrated in Stage 6 |
| Z2-D-065 | evidence fingerprints over the manifest hashes |
| Z2-D-082 | third-party pinning, confinement and ownership |
| Z2-D-085, Z2-D-097 | living documents and rulings in the ticket's diff (licence policy changes are `needs-human` ruling tickets) |
| Z2-D-128 | Stage 0 PROC tickets are needs-human; the scaffold is last |

### Open decisions SHIP depends on

| Decision | Effect on SHIP |
|---|---|
| D-SLANG (open, Stage 1) | which alternative the bindless probe runs by default (RHI's option); the build compiles all three (`SLANG_ALTERNATIVES` default `1 2 3`) |
| D-SEL-HEAP, D-SEL-HASH, D-SEL-JSON (open, Stage 1) | each ruling moves its candidate entry's owner and usage |
| D-SEL-CRASH (open, Stage 2) | a Breakpad symbol tier if Crashpad is chosen |
| D-SEL-UPSCALER (open, Stage 5) | the upscaler candidate entries |
| D-SCRIPT-TXN refinement (open, Stage 1) | the Luau `nativePolicy` pin |

### Selections and classifications SHIP owns

None. SHIP's design choices (SymStore and `.build-id` layouts, SHA-256 alone, `/Brepro`, the licence allow-list) are fixed here; a change is a design change in a diff, and admitting a licence identifier records a decision (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Licence fields").

### Flagged proposals raised by SHIP

SHIP-1 (`findings/FLAGGED_PROPOSALS.md`) is withdrawn as resolved: the orchestrator's Q71 ruling (a) made `ZENITH_ASSERTS_ENABLED` a per-configuration value in every target (cross-document item X231, applied), so no inline entity has two definitions across tool and engine translation units; host tools write `ZENITH_CHECK` for invariants they need in Release (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Assert define").

## Requirement index

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-SHIP-001 | Manifest file shape | Zenith2/Docs/Design/SHIP/Manifest.md → "Top-level shape" |
| Z2-R-SHIP-002 | Section hashes over the canonical form | Zenith2/Docs/Design/SHIP/Manifest.md → "Canonicalisation and hashing" |
| Z2-R-SHIP-003 | Toolchain section keys and shapes | Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain keys"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain pin object" |
| Z2-R-SHIP-004 | Toolchain runtime components | Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain runtime components" |
| Z2-R-SHIP-005 | Dependency entry fields | Zenith2/Docs/Design/SHIP/Manifest.md → "Dependency entry fields"; Zenith2/Docs/Design/SHIP/Manifest.md → "Integration modes in detail" |
| Z2-R-SHIP-006 | Owner values and owner tokens | Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values" |
| Z2-R-SHIP-007 | One owner for natively measured libraries | Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Name resolution" |
| Z2-R-SHIP-008 | Target mapping of multi-target entries | Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables" |
| Z2-R-SHIP-009 | Luau entry and its target map | Zenith2/Docs/Design/SHIP/Manifest.md → "Luau entry"; Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables" |
| Z2-R-SHIP-010 | Luau recorded pins | Zenith2/Docs/Design/SHIP/Manifest.md → "Luau entry"; Zenith2/Docs/Design/SHIP/Manifest.md → "Recorded pins" |
| Z2-R-SHIP-011 | Exceptions only on Luau targets | Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Third-party flag rows" |
| Z2-R-SHIP-012 | Prebuilt items and toolchain-sourced entries | Zenith2/Docs/Design/SHIP/Manifest.md → "The prebuilt array"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain keys" |
| Z2-R-SHIP-013 | Agility SDK entry and its pins | Zenith2/Docs/Design/SHIP/Manifest.md → "D3D12 Agility SDK entry"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Runtime file staging" |
| Z2-R-SHIP-014 | Recorded pins rules | Zenith2/Docs/Design/SHIP/Manifest.md → "Recorded pins" |
| Z2-R-SHIP-015 | Selection-candidate entries | Zenith2/Docs/Design/SHIP/Manifest.md → "Selection-candidate entries" |
| Z2-R-SHIP-016 | Test and benchmark infrastructure entries | Zenith2/Docs/Design/SHIP/Manifest.md → "Test and benchmark entries" |
| Z2-R-SHIP-017 | Shipping-excluded dependencies | Zenith2/Docs/Design/SHIP/Manifest.md → "Shipping exclusion field"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Shipping-excluded dependencies" |
| Z2-R-SHIP-018 | Licence fields and the licence policy | Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain licence terms" |
| Z2-R-SHIP-019 | Patch entries | Zenith2/Docs/Design/SHIP/Manifest.md → "Patch entries"; Zenith2/Docs/Design/SHIP/Manifest.md → "Patch application" |
| Z2-R-SHIP-020 | One manifest validator | Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"; Zenith2/Docs/Design/SHIP/Manifest.md → "Validation error messages"; Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields" |
| Z2-R-SHIP-021 | Consumer entries carry the requested fields | Zenith2/Docs/Design/SHIP/Manifest.md → "Other consumer entries"; Zenith2/Docs/Design/SHIP/Manifest.md → "Test and benchmark entries"; Zenith2/Docs/Design/SHIP/Manifest.md → "Selection-candidate entries" |
| Z2-R-SHIP-022 | Cache layout and directory names | Zenith2/Docs/Design/SHIP/Manifest.md → "Cache layout" |
| Z2-R-SHIP-023 | Digest verification before use | Zenith2/Docs/Design/SHIP/Manifest.md → "Digest verification algorithm" |
| Z2-R-SHIP-024 | Safe archive extraction | Zenith2/Docs/Design/SHIP/Manifest.md → "Archive extraction rules"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux archive extraction" |
| Z2-R-SHIP-025 | Tree digest and stamp | Zenith2/Docs/Design/SHIP/Manifest.md → "Tree digest"; Zenith2/Docs/Design/SHIP/Manifest.md → "Stamp check at configure" |
| Z2-R-SHIP-026 | Licence file verification at setup | Zenith2/Docs/Design/SHIP/Manifest.md → "Digest verification algorithm"; Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields" |
| Z2-R-SHIP-027 | Configure-time stamp check | Zenith2/Docs/Design/SHIP/Manifest.md → "Stamp check at configure" |
| Z2-R-SHIP-028 | Setup script installs and verifies every pin | Zenith2/Docs/Design/SHIP/Manifest.md → "Setup-Zenith2 parameters"; Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps" |
| Z2-R-SHIP-029 | Check mode verifies without changing | Zenith2/Docs/Design/SHIP/Manifest.md → "Check mode" |
| Z2-R-SHIP-030 | Licence terms are accepted by the human | Zenith2/Docs/Design/SHIP/Manifest.md → "Licence acceptance switches" |
| Z2-R-SHIP-031 | Environment script | Zenith2/Docs/Design/SHIP/Manifest.md → "Enter-Zenith2Env"; Zenith2/Docs/Design/SHIP/Manifest.md → "Zenith2Env module" |
| Z2-R-SHIP-032 | One environment module | Zenith2/Docs/Design/SHIP/Manifest.md → "Zenith2Env module" |
| Z2-R-SHIP-033 | Offline Gradle home with verification metadata | Zenith2/Docs/Design/SHIP/Manifest.md → "Offline Gradle home" |
| Z2-R-SHIP-034 | Android SDK packages pinned by tree digest | Zenith2/Docs/Design/SHIP/Manifest.md → "Android SDK provisioning" |
| Z2-R-SHIP-035 | WSL provisioning | Zenith2/Docs/Design/SHIP/Manifest.md → "WSL provisioning"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux package snapshot"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux archive extraction" |
| Z2-R-SHIP-036 | Clean-machine reproducible setup | Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps"; Zenith2/Docs/Design/SHIP/Release.md → "Reproducibility definition" |
| Z2-R-SHIP-037 | Development signing keystore | Zenith2/Docs/Design/SHIP/Manifest.md → "Development signing keystore"; Zenith2/Docs/Design/SHIP/Release.md → "Key custody" |
| Z2-R-SHIP-038 | Dependency update tool | Zenith2/Docs/Design/SHIP/Manifest.md → "Update procedure"; Zenith2/Docs/Design/SHIP/Manifest.md → "Update-Zenith2Dependency" |
| Z2-R-SHIP-039 | Upgrade diff contents | Zenith2/Docs/Design/SHIP/Manifest.md → "What an upgrade diff contains" |
| Z2-R-SHIP-040 | Upgrades invalidate evidence | Zenith2/Docs/Design/SHIP/Manifest.md → "Evidence invalidation" |
| Z2-R-SHIP-041 | Global flag hygiene | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Global flag hygiene" |
| Z2-R-SHIP-042 | Toolchain pin checks at configure | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Toolchain files"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Global flag hygiene" |
| Z2-R-SHIP-043 | THIRD_PARTY name resolution | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Name resolution" |
| Z2-R-SHIP-044 | THIRD_PARTY links privately with system includes | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Link and include rules" |
| Z2-R-SHIP-045 | THIRD_PARTY on benchmarks tools and integration suites | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| Z2-R-SHIP-046 | Mode prebuilt | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Mode prebuilt" |
| Z2-R-SHIP-047 | Mode host-tool | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Mode host-tool" |
| Z2-R-SHIP-048 | Subproject options definitions and ordering | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Mode cmake-subproject"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_third_party_add_all" |
| Z2-R-SHIP-049 | Host executables join the host tools | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs" |
| Z2-R-SHIP-050 | Exposed toolchain and cache variables | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Exposed variables" |
| Z2-R-SHIP-051 | Pin header templates | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Pin header templates" |
| Z2-R-SHIP-052 | Runtime file staging | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Runtime file staging" |
| Z2-R-SHIP-053 | Third-party target record | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs" |
| Z2-R-SHIP-054 | Conformance executables | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Conformance executables" |
| Z2-R-SHIP-055 | Discovery labels | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Keywords added by SHIP"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Discovery labels" |
| Z2-R-SHIP-056 | Fuzz and compile-fail hand-off cannot drop a source | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Fuzz and compile-fail partitions"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry tail" |
| Z2-R-SHIP-057 | Expected-build test environment | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment" |
| Z2-R-SHIP-058 | Generic registry functions | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry functions"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Death-case registration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Compile-fail registration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Fuzz target registration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_register_fixture_set"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry tail" |
| Z2-R-SHIP-059 | declared_deps additions | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "declared_deps additions" |
| Z2-R-SHIP-060 | Assert define per configuration | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Assert define" |
| Z2-R-SHIP-061 | Third-party flag rows | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Third-party flag rows" |
| Z2-R-SHIP-062 | Reproducibility flags | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Reproducibility flags" |
| Z2-R-SHIP-063 | Hardening flags in Release and Shipping | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Hardening flags" |
| Z2-R-SHIP-064 | Shipping differs from Release only as listed | Zenith2/Docs/Design/SHIP/Release.md → "Differences from Release" |
| Z2-R-SHIP-065 | Development services absent from Shipping | Zenith2/Docs/Design/SHIP/Release.md → "Development services in Shipping" |
| Z2-R-SHIP-066 | Shipping link closure checks | Zenith2/Docs/Design/SHIP/Release.md → "Shipping link closure checks" |
| Z2-R-SHIP-067 | CI workflow triggers | Zenith2/Docs/Design/SHIP/Qualification.md → "Workflow triggers" |
| Z2-R-SHIP-068 | CI jobs | Zenith2/Docs/Design/SHIP/Qualification.md → "Jobs" |
| Z2-R-SHIP-069 | CI job steps | Zenith2/Docs/Design/SHIP/Qualification.md → "Job steps" |
| Z2-R-SHIP-070 | CI never signs measures or downloads | Zenith2/Docs/Design/SHIP/Qualification.md → "CI invariants" |
| Z2-R-SHIP-071 | Device matrix | Zenith2/Docs/Design/SHIP/Qualification.md → "Devices"; Zenith2/Docs/Design/SHIP/Qualification.md → "Support classes" |
| Z2-R-SHIP-072 | Evidence only from validated devices | Zenith2/Docs/Design/SHIP/Qualification.md → "Device matrix rules" |
| Z2-R-SHIP-073 | Release pipeline states | Zenith2/Docs/Design/SHIP/Release.md → "New-Zenith2Release"; Zenith2/Docs/Design/SHIP.md → "Release pipeline state machine" |
| Z2-R-SHIP-074 | Release preconditions | Zenith2/Docs/Design/SHIP/Release.md → "New-Zenith2Release" |
| Z2-R-SHIP-075 | Release staging layout and deterministic archives | Zenith2/Docs/Design/SHIP/Release.md → "Release staging layout" |
| Z2-R-SHIP-076 | Windows package contents | Zenith2/Docs/Design/SHIP/Release.md → "Windows package" |
| Z2-R-SHIP-077 | Android package | Zenith2/Docs/Design/SHIP/Release.md → "Android package" |
| Z2-R-SHIP-078 | Release measurement packages | Zenith2/Docs/Design/SHIP/Release.md → "Release staging layout"; Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification" |
| Z2-R-SHIP-079 | Key custody | Zenith2/Docs/Design/SHIP/Release.md → "Key custody" |
| Z2-R-SHIP-080 | Public releases need a public CA | Zenith2/Docs/Design/SHIP/Release.md → "Key custody" |
| Z2-R-SHIP-081 | Windows Authenticode signing | Zenith2/Docs/Design/SHIP/Release.md → "Windows Authenticode" |
| Z2-R-SHIP-082 | Android APK signing | Zenith2/Docs/Design/SHIP/Release.md → "Android APK signing" |
| Z2-R-SHIP-083 | App Bundle upload signing | Zenith2/Docs/Design/SHIP/Release.md → "App Bundle upload signing" |
| Z2-R-SHIP-084 | Update key consistency | Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off"; Zenith2/Docs/Design/SHIP/Release.md → "Key custody" |
| Z2-R-SHIP-085 | Release manifest schema | Zenith2/Docs/Design/SHIP/Release.md → "release.json schema"; Zenith2/Docs/Design/SHIP/Release.md → "Field rules" |
| Z2-R-SHIP-086 | Release manifest identities | Zenith2/Docs/Design/SHIP/Release.md → "Field rules"; Zenith2/Docs/Design/SHIP/Release.md → "Symbol identities" |
| Z2-R-SHIP-087 | Release archive index | Zenith2/Docs/Design/SHIP/Release.md → "Release archive index" |
| Z2-R-SHIP-088 | Symbol identities | Zenith2/Docs/Design/SHIP/Release.md → "Symbol identities" |
| Z2-R-SHIP-089 | Symbol store layout | Zenith2/Docs/Design/SHIP/Release.md → "Symbol store layout" |
| Z2-R-SHIP-090 | Symbols collected and checked | Zenith2/Docs/Design/SHIP/Release.md → "Collecting symbols" |
| Z2-R-SHIP-091 | Play native debug symbols | Zenith2/Docs/Design/SHIP/Release.md → "Play native debug symbols" |
| Z2-R-SHIP-092 | Symbol retention | Zenith2/Docs/Design/SHIP/Release.md → "Retention policy" |
| Z2-R-SHIP-093 | Symbolization tools | Zenith2/Docs/Design/SHIP/Release.md → "Symbolization tools" |
| Z2-R-SHIP-094 | Crash-symbol tier follows D-SEL-CRASH | Zenith2/Docs/Design/SHIP/Release.md → "Symbol store layout" |
| Z2-R-SHIP-095 | Licence inventory selection | Zenith2/Docs/Design/SHIP/Release.md → "Inventory selection algorithm" |
| Z2-R-SHIP-096 | An unstated licence blocks packaging | Zenith2/Docs/Design/SHIP/Release.md → "Inventory selection algorithm" |
| Z2-R-SHIP-097 | Notices file format | Zenith2/Docs/Design/SHIP/Release.md → "THIRD_PARTY_NOTICES.txt format" |
| Z2-R-SHIP-098 | Machine-readable licence inventory | Zenith2/Docs/Design/SHIP/Release.md → "licences.json format" |
| Z2-R-SHIP-099 | Licences ship in every package | Zenith2/Docs/Design/SHIP/Release.md → "Windows package"; Zenith2/Docs/Design/SHIP/Release.md → "Android package" |
| Z2-R-SHIP-100 | Repository dependency inventory | Zenith2/Docs/Design/SHIP/Release.md → "Inventory selection algorithm" |
| Z2-R-SHIP-101 | Third-party content licences | Zenith2/Docs/Design/SHIP/Release.md → "Content licences" |
| Z2-R-SHIP-102 | Reproducible outputs | Zenith2/Docs/Design/SHIP/Release.md → "Reproducibility definition" |
| Z2-R-SHIP-103 | Reproducibility check tool | Zenith2/Docs/Design/SHIP/Release.md → "Test-Zenith2Reproducibility" |
| Z2-R-SHIP-104 | Release notes | Zenith2/Docs/Design/SHIP/Release.md → "Release notes" |
| Z2-R-SHIP-105 | Known issues | Zenith2/Docs/Design/SHIP/Release.md → "Known issues" |
| Z2-R-SHIP-106 | Upgrade guides per fix version | Zenith2/Docs/Design/SHIP/Release.md → "Upgrade guides" |
| Z2-R-SHIP-107 | Upgrade guide generator | Zenith2/Docs/Design/SHIP/Release.md → "Upgrade guides" |
| Z2-R-SHIP-108 | Release documentation lint | Zenith2/Docs/Design/SHIP/Release.md → "Release documentation lint" |
| Z2-R-SHIP-109 | Compatibility test list | Zenith2/Docs/Design/SHIP/Release.md → "Compatibility labels"; Zenith2/Docs/Design/SHIP/Release.md → "Demonstration suite" |
| Z2-R-SHIP-110 | Deprecated use fails the build | Zenith2/Docs/Design/SHIP/Release.md → "Compatibility labels" |
| Z2-R-SHIP-111 | Compatibility demonstration at Stage 6 | Zenith2/Docs/Design/SHIP/Release.md → "Demonstration suite"; Zenith2/Docs/Design/SHIP/Qualification.md → "Security and compatibility items" |
| Z2-R-SHIP-112 | Qualification record | Zenith2/Docs/Design/SHIP/Qualification.md → "Qualification record" |
| Z2-R-SHIP-113 | Windows package checklist | Zenith2/Docs/Design/SHIP/Qualification.md → "Windows package checklist" |
| Z2-R-SHIP-114 | Android package checklist | Zenith2/Docs/Design/SHIP/Qualification.md → "Android package checklist" |
| Z2-R-SHIP-115 | Headless boot of the packaged build | Zenith2/Docs/Design/SHIP/Qualification.md → "Windows package checklist" |
| Z2-R-SHIP-116 | Device runs | Zenith2/Docs/Design/SHIP/Qualification.md → "Device run checklist" |
| Z2-R-SHIP-117 | Sustained thermal qualification | Zenith2/Docs/Design/SHIP/Qualification.md → "Device run checklist"; Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification" |
| Z2-R-SHIP-118 | Frame-rate qualification | Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification" |
| Z2-R-SHIP-119 | Hitch rows warm and cold | Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification" |
| Z2-R-SHIP-120 | Cold boot qualification | Zenith2/Docs/Design/SHIP/Qualification.md → "Device run checklist" |
| Z2-R-SHIP-121 | Flagship slice qualification | Zenith2/Docs/Design/SHIP/Qualification.md → "Flagship slice checklist" |
| Z2-R-SHIP-122 | Qualification budget runs measure packaged builds | Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification" |
| Z2-R-SHIP-123 | Security and setup items at qualification | Zenith2/Docs/Design/SHIP/Qualification.md → "Security and compatibility items" |
| Z2-R-SHIP-124 | Backup rules verified | Zenith2/Docs/Design/SHIP/Qualification.md → "Android package checklist" |
| Z2-R-SHIP-125 | CI workflow test guards the matrix | Zenith2/Docs/Design/SHIP/Qualification.md → "CI invariants"; Zenith2/Docs/Design/SHIP/Tests.md → "PowerShell tests" |
| Z2-R-SHIP-126 | Bindless probe build-time shaders | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| Z2-R-SHIP-127 | Generated test root | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment" |
| Z2-R-SHIP-128 | Ownership rules at the end of the configure | Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs" |
| Z2-R-SHIP-129 | Luau closure rule keeps the exception set | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables"; Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array" |
| Z2-R-SHIP-130 | Platform skip for benchmarks tools and integration suites | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Name resolution" |
| Z2-R-SHIP-131 | Licence expressions and elected alternatives | Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields" |
| Z2-R-SHIP-132 | Licence policy changes are human rulings | Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields" |
| Z2-R-SHIP-133 | Linux host mode | Zenith2/Docs/Design/SHIP/Manifest.md → "Linux host mode"; Zenith2/Docs/Design/SHIP/Manifest.md → "Check mode"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux package snapshot"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux archive extraction" |
| Z2-R-SHIP-134 | Setup before the validator exists | Zenith2/Docs/Design/SHIP.md → "Stage 0 bootstrapping order"; Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps" |
| Z2-R-SHIP-135 | Unlisted death cases are discovered and run | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Unlisted death-case discovery"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry tail"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Death-case registration" |
| Z2-R-SHIP-136 | Signed outputs and the reproducible form | Zenith2/Docs/Design/SHIP/Release.md → "Field rules"; Zenith2/Docs/Design/SHIP/Release.md → "New-Zenith2Release" |
| Z2-R-SHIP-137 | Update signing key encrypted at rest | Zenith2/Docs/Design/SHIP/Release.md → "Key custody"; Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off" |
| Z2-R-SHIP-138 | Executable imports resolve on a clean machine | Zenith2/Docs/Design/SHIP/Qualification.md → "Windows package checklist"; Zenith2/Docs/Design/SHIP/Release.md → "Windows package" |
| Z2-R-SHIP-139 | No Maven runtime artefact in the APK | Zenith2/Docs/Design/SHIP/Release.md → "Android package"; Zenith2/Docs/Design/SHIP/Qualification.md → "Android package checklist" |
| Z2-R-SHIP-140 | SHIP fixtures need no recorded digest | Zenith2/Docs/Design/SHIP/Tests.md → "Fixture digests" |
| Z2-R-SHIP-141 | Entry names in lower-case kebab case | Zenith2/Docs/Design/SHIP/Manifest.md → "Entry names used by consumers"; Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules" |
| Z2-R-SHIP-142 | Stage 0 reproducibility-manifest deliverable | Zenith2/Docs/Design/SHIP.md → "Stage 0"; Zenith2/Docs/Design/SHIP.md → "Stage 0 bootstrapping order"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain licence terms"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain runtime components" |
| Z2-R-SHIP-143 | Stage 0 CI workflow deliverable | Zenith2/Docs/Design/SHIP.md → "Stage 0"; Zenith2/Docs/Design/SHIP/Qualification.md → "CI matrix" |
| Z2-R-SHIP-144 | Owners declared after their pins | Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"; Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values"; Zenith2/Docs/Design/SHIP.md → "Pin landing order" |
| Z2-R-SHIP-145 | Pins land after the mechanisms they need | Zenith2/Docs/Design/SHIP.md → "Pin landing order"; Zenith2/Docs/Design/SHIP.md → "Stage 1"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_third_party_add_all" |
| Z2-R-SHIP-146 | Toolchain pin licences | Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain licence terms"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain pin object"; Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields" |
| Z2-R-SHIP-147 | Update key checked by the pinned JDK | Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off" |
| Z2-R-SHIP-148 | ShippingTest configuration in the helpers | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "ShippingTest configuration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Global flag hygiene"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs" |
| Z2-R-SHIP-149 | Integration-suite labels and suite labels | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Discovery labels"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| Z2-R-SHIP-150 | Generated test root on Android names the device | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment" |
| Z2-R-SHIP-151 | Committed release document set | Zenith2/Docs/Design/SHIP/Release.md → "Release archive index"; Zenith2/Docs/Design/SHIP/Release.md → "Release documentation lint" |
| Z2-R-SHIP-152 | TEST_DEPENDS per-preset linking of test targets | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test third-party links"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs"; Zenith2/Docs/Design/SHIP.md → "Stage 1" |
| Z2-R-SHIP-154 | git-lfs and the content merge driver on every gate host and lane | Zenith2/Docs/Design/SHIP/Manifest.md → "Git prerequisites"; Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps"; Zenith2/Docs/Design/SHIP/Manifest.md → "Check mode" |
| Z2-R-SHIP-153 | KIND GENERATED branch and the shaders directory on test targets | Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Generated shader library"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment"; Zenith2/Docs/Design/SHIP.md → "Stage 1" |
