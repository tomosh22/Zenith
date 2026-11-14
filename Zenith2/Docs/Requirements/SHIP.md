# SHIP requirement catalogue

## Purpose

This catalogue breaks the SHIP inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "SHIP Shipping & maintenance") into testable requirements `Z2-R-SHIP-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves (its fix version; a ticket may be scheduled earlier by the pull-forward rule), its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of `Zenith2/Docs/Design/SHIP.md` and its companion files that specify it; the design's "Requirement index" is the reverse map. SHIP owns every requirement here. Where a requirement is implemented by a PROC ticket because the file it changes is protected (the CI workflow, the gate steps that read SHIP's outputs) or by a needs-human ticket because only a human can download, verify, sign or run on a device, that is a property of the ticket, not of ownership. The split from PACK, MAINT and PROC follows the inventory rows: `Zenith2/Docs/Design/SHIP.md` → "The SHIP PACK MAINT and PROC split".

## Requirements

### Z2-R-SHIP-001 Manifest file shape

- **Statement:** `Zenith2/ThirdParty/manifest.json` is one UTF-8 JSON file without a byte-order mark, at most 1 MiB, with exactly the top-level keys `schema` (`zenith2.third-party-manifest`), `schemaVersion` (1), `toolchain` and `dependencies`, in that order.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestValid_Committed`, `CMake_Ship_ManifestRule_V01`, `CMake_Ship_ManifestRule_V02`, `Pwsh_Ship_ManifestText_BomRefused`, `Pwsh_Ship_ManifestText_TooLargeRefused`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Top-level shape"

### Z2-R-SHIP-002 Section hashes over the canonical form

- **Statement:** `toolchainHash` and `dependenciesHash` are the SHA-256 of the canonical JSON form of their sections and `manifestSha256` the SHA-256 of the file bytes, computed identically by `Get-Zenith2CanonicalJson` and the CMake function `_zenith_manifest_canonical`, so a whitespace-only edit changes only `manifestSha256`.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ManifestCanonicalHashAgrees` (both implementations equal each other and the hand-written `canonical-small.expected.txt`), `Pwsh_Ship_ManifestHashes_FingerprintFixtures` (hashes recomputed at test time by .NET SHA-256, relations asserted, no committed digest), Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Canonicalisation and hashing"

### Z2-R-SHIP-003 Toolchain section keys and shapes

- **Statement:** The `toolchain` section has exactly the keys of the BuildSystem toolchain table (`msvc`, `llvm`, `cmake`, `ninja`, `sccache`, `slang`, `dxc`, `warp`, `lavapipe`, `ndk`, `jdk`, `gradle`, `agp`, `androidSdk`, `wsl`, `python`, `pwsh`, `vulkanProfilesGenerator`), each with the JSON shape of the table "Toolchain keys", including `msvc.clVersion`, `toolsetVersion` and `windowsSdk`, `llvm` as `windows` and `linux` archive objects (never a distro package), `wsl.snapshot`, `wsl.packages` and `wsl.licence`, `ndk.pkgRevision`, `gradle.distribution.sha256`, a `licence` on every key except `vulkanProfilesGenerator`, and `vulkanProfilesGenerator` `null` exactly while no `vulkan-profiles` entry exists.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V03` to `CMake_Ship_ManifestRule_V08`, `CMake_Ship_ManifestRule_V31`, `CMake_Ship_ManifestRule_V40`, `CMake_Ship_ManifestValid_Committed` (the Stage 0 manifest with the generator pin `null`, registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain keys"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain pin object"

### Z2-R-SHIP-004 Toolchain runtime components

- **Statement:** Redistributable or statically linked toolchain runtime code a packaged build carries (every MSVC CRT DLL the pinned toolset can make an executable import, the NDK libc++ and compiler builtins) is recorded as a `runtimeComponents` element of its toolchain key with an id, platform, kind, file list and licence fields, and its licence is governed by `allowedRuntime`.
- **Covers:** Z2-I-SHIP-08, Z2-I-SHIP-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_Selection` (the platform's components listed), `CMake_Ship_ManifestRule_V35`, `CMake_Ship_ManifestRule_V27` (a component outside `allowedRuntime` refused), Test step; W3, W11 and W15 of the Stage 6 qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain runtime components"

### Z2-R-SHIP-005 Dependency entry fields

- **Statement:** Every `dependencies` entry carries the always-present fields of the table "Dependency entry fields" with their types (`source` with `url`, `sha256`, `stripPrefix` and `format`), entries are sorted by byte order of `name` with unique names and unique IDs, every path field is a safe relative path, enumerated fields take only their listed values, and the optional fields (`cmakeSourceDir`, `options`, `definitions`, `dependsOn`, `fromToolchain`, `prebuilt`, `targets`, `pins`, `shipping`, `sanitizerExempt`, `decision`, `licenceElected`) appear only with the modes and usages the table allows.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V04`, `CMake_Ship_ManifestRule_V10` to `CMake_Ship_ManifestRule_V15`, `CMake_Ship_ManifestRule_V28`, `CMake_Ship_ManifestRule_V33`, `CMake_Ship_ManifestRule_V34`, `CMake_Ship_ManifestRule_V36`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Dependency entry fields"; Zenith2/Docs/Design/SHIP/Manifest.md → "Integration modes in detail"

### Z2-R-SHIP-006 Owner values and owner tokens

- **Statement:** An entry's `owner` is one Zenith2 target name (a library, a tool, or a benchmark target `Zenith_Bench_<Name>`, the form of Z2T-CORE-053), an array of sibling `KIND BACKEND` libraries (header-only entries only), or one of the reserved tokens `tests`, `benchmark` and `test-runs` (and `host` inside `targets`), its shape agrees with its usage, and `autoLink` is accepted only with `tests` and `benchmark`.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V16`, `CMake_Ship_ManifestRule_V17`, `CMake_Ship_ManifestRule_V18`, `CMake_Ship_ManifestKindRule_K2` (a `tools` entry owned by a declared benchmark accepted), `CMake_Ship_BenchmarkThirdParty`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values"

### Z2-R-SHIP-007 One owner for natively measured libraries

- **Statement:** A benchmark that links a library natively (`Zenith_Bench_ECSNative`, `Zenith_Bench_BudgetsECS` linking flecs and EnTT) names the entry in its own `THIRD_PARTY` and the entry keeps its single library owner; no second owner is recorded in the manifest.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_BenchmarkThirdParty` (a benchmark naming a library-owned entry links it), Test step; `Libgraph` accepting the benchmark's includes through `declared_deps.json`.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Name resolution"

### Z2-R-SHIP-008 Target mapping of multi-target entries

- **Statement:** An entry with several CMake targets lists each in `targets` with its own `owner`, `usage` and optional `platforms`, every listed target exists after the entry is added on each platform of its element, and `THIRD_PARTY <name>` links exactly the targets the naming library owns among the elements present on the preset.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_TargetsOwnership`, `CMake_Ship_TargetsElementPlatforms`, `CMake_Ship_ManifestRule_V19`, `CMake_Ship_ManifestRule_V33`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables"

### Z2-R-SHIP-009 Luau entry and its target map

- **Statement:** The `luau` entry maps `Luau.VM` and `Luau.CodeGen` to `ZenithScript` (runtime), `Luau.Ast` and `Luau.Compiler` to `ZenithScriptCompiler` (tools), and `Luau.Config`, `Luau.Analysis`, each further library target `Luau.Analysis` links, and `Luau.Analyze.CLI` (with `platforms: ["windows"]` and `hostExecutable: "luau-analyze"`) to `host` (tools), with `exceptions: true` on every one, `LUA_USE_LONGJMP=0` in `definitions`, and `LUAU_BUILD_CLI` on for Windows alone, so no Android or Linux configure requires a CLI target; the other CLI targets stay unlisted, unbuilt and unlinked.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_TargetsOwnership`, `CMake_Ship_TargetsElementPlatforms`, `CMake_Ship_LuauClosureRule`, `CMake_Ship_HostExecutableInHostTools`, Test step; SCRIPT's `Libgraph` ownership check of `Luau.VM` and `Luau.CodeGen`.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Luau entry"; Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables"

### Z2-R-SHIP-010 Luau recorded pins

- **Statement:** The `luau` entry's `pins` carry `commit` (40 hex, equal to the archive's commit), `releaseTag` (equal to `version`), `apiLinkage` (`cxx` or `c`) with sorted `apiLinkageSymbols`, `nativePolicy` with exactly `android` and `windows` (each `disabled`, `toolsOnly` or `all`), and integers `bytecodeMin` ≤ `bytecodeTarget` ≤ `bytecodeMax`.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V32`, `CMake_Ship_ManifestRule_V38`, `CMake_Ship_PinTemplateLuau`, Test step; SCRIPT's `Zenith_Script_LuauPinRecord_Check`.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Luau entry"; Zenith2/Docs/Design/SHIP/Manifest.md → "Recorded pins"

### Z2-R-SHIP-011 Exceptions only on Luau targets

- **Statement:** `targets[].exceptions: true` is accepted only on targets of the entry named `luau`, those targets compile with `/EHs /D_HAS_EXCEPTIONS=1` (`-fexceptions`) and RTTI off, and every other third-party target compiles with exceptions and RTTI off.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V20`, `CMake_Ship_ThirdPartyFlagRows`, Test step; `ForbiddenCheck` F17.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Third-party flag rows"

### Z2-R-SHIP-012 Prebuilt items and toolchain-sourced entries

- **Statement:** Every `prebuilt` and `host-tool` entry carries a `prebuilt` array: with one item per ABI it is used on holding the archive fields, `include`, `link`, `runtime`, `runtimeSubdir` and `executables`, or, for an entry that takes its archive from a `toolchain` key through `fromToolchain`, items holding `abi` (a host the key has an archive for) and the layout fields only, so Slang, DXC, WARP and lavapipe are pinned once; a runtime-usage entry links no prebuilt library.
- **Covers:** Z2-I-SHIP-08, Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-008, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V14`, `CMake_Ship_ManifestRule_V22` (including a `fromToolchain` item with `url` and a `linux-x64` item on a key without a `linux` archive), `CMake_Ship_ThirdPartyFromToolchain`, `CMake_Ship_FromToolchainWithoutSource`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "The prebuilt array"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain keys"

### Z2-R-SHIP-013 Agility SDK entry and its pins

- **Statement:** The `d3d12-agility-sdk` entry is `prebuilt`, owned by `ZenithRHI_D3D12`, `sanitizerExempt` with its decision, stages `D3D12Core.dll` and `d3d12SDKLayers.dll` into `D3D12/` in development configurations and `pins.shippingRuntime` (`D3D12Core.dll`) in `Shipping` and `ShippingTest`, and records `pins.sdkVersion`; it is pinned in Stage 1 after the `prebuilt` mode exists, for the bindless probe and the D3D12 backend.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V32`, `CMake_Ship_RuntimeStaging`, Test step; W3 and W4 of the Stage 6 qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "D3D12 Agility SDK entry"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Runtime file staging"

### Z2-R-SHIP-014 Recorded pins rules

- **Statement:** `pins` keys match `^[a-z][A-Za-z0-9]{0,63}$` outside the reserved names, values are strings, non-negative integers, booleans, string arrays or one-level objects of those, and the manifest contains no floating-point number.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V29`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Recorded pins"

### Z2-R-SHIP-015 Selection-candidate entries

- **Statement:** The CORE selection candidates (`mimalloc`, `xxhash`, `yyjson`, `simdjson`) are entries owned by their harness benchmark target `Zenith_Bench_<Name>` with `usage: tools`, exactly as Z2T-CORE-053 writes them; the D-SEL-UPSCALER candidates take the token `benchmark` with `usage: tests`; `autoLink` is absent; each is named only by its harness, and the ruling ticket moves the winner's owner and usage in its diff.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-084, D-SEL-HEAP (open, Stage 1), D-SEL-HASH (open, Stage 1), D-SEL-JSON (open, Stage 1), D-SEL-UPSCALER (open, Stage 5)
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_BenchmarkThirdParty`, `CMake_Ship_ManifestKindRule_K2`, `CMake_Ship_OwnerDeclaredLater` (a benchmark-owned candidate reported until its harness is declared), Test step; the CORE and LIGHT harness tickets' `--smoke` runs.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Selection-candidate entries"

### Z2-R-SHIP-016 Test and benchmark infrastructure entries

- **Statement:** doctest (`owner: "tests"`, `autoLink`, its include directory the tree root so `<doctest/doctest.h>` resolves), nanobench (`owner: "benchmark"`, `autoLink`, `pins.includeDir: "src/include"`, pinned in Stage 1 by the test-infrastructure pinning ticket after T3, which every benchmark ticket is `blocked_by`), `flip` (owner `ZenithFlux`, linked through `TEST_THIRD_PARTY`), and WARP, lavapipe and the Vulkan validation layer (`owner: "test-runs"`, WARP and lavapipe with `fromToolchain` items per ABI) are manifest entries with `usage: "tests"`, so their licences and digests are pinned like every other dependency.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestValid_Committed`, `CMake_Ship_ManifestRule_V32` (no `includeDir` required of doctest), `CMake_Ship_TestThirdPartyNotInLibrary`, `CMake_Ship_RuntimeStaging`, Test step; the Stage 1 probe smoke on WARP and lavapipe (Z2T-RHI-001).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Test and benchmark entries"

### Z2-R-SHIP-017 Shipping-excluded dependencies

- **Statement:** A runtime entry with `shipping: "excluded"` (`tracy`, the RenderDoc in-application API `renderdoc-app`) is linked through `$<$<NOT:$<CONFIG:Shipping,ShippingTest>>:…>` with an unconditional include directory and is absent from every Shipping licence inventory; for a header-only entry, whose conditional link removes nothing, the owner's `if constexpr` entry and the forbidden-symbol list carry the guarantee.
- **Covers:** Z2-I-SHIP-08, Z2-I-SHIP-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ShippingExcludedLinkIsConditional`, `CMake_Ship_LicenceInventory_ShippingExcludesTracy`, Test step; W5 and A6 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Shipping exclusion field"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Shipping-excluded dependencies"

### Z2-R-SHIP-018 Licence fields and the licence policy

- **Statement:** Every dependency entry and toolchain runtime component records an SPDX expression, a licence file and its SHA-256, every toolchain pin except `vulkanProfilesGenerator` records an SPDX expression (no file: nothing of it ships), and each is allowed by the list of `Zenith2/ThirdParty/licence_policy.json` that governs it (`allowedRuntime` for runtime entries and toolchain runtime components, `allowedTools`, `allowedTests`, `allowedContent` for content components, `allowedToolchain` for toolchain pins), the initial lists including the MSVC CRT and NDK runtime licences and every term of "Toolchain licence terms".
- **Covers:** Z2-I-SHIP-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V26`, `CMake_Ship_ManifestRule_V27` (a toolchain pin outside `allowedToolchain` refused), `CMake_Ship_ManifestRule_V40`, `CMake_Ship_LicenceInventory_RefusesDisallowedLicence`, `CMake_Ship_ManifestValid_Committed` (the committed policy admits every committed entry, pin and component), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain licence terms"

### Z2-R-SHIP-019 Patch entries

- **Statement:** A local change to a dependency is a patch file under `ThirdParty/patches/<name>/` listed in the entry with its SHA-256 and the decision that approved it, applied in array order to the verified tree, and the patch set is part of the cache directory name.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V25`, `CMake_Ship_PatchedEntryDirectoryName` (the patch-set digest computed in the case), `Pwsh_Ship_DigestVerify_PatchApplied`, `Pwsh_Ship_DigestVerify_PatchMismatchRefused`, `Pwsh_Ship_DigestVerify_PatchDoesNotApply`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Patch entries"; Zenith2/Docs/Design/SHIP/Manifest.md → "Patch application"

### Z2-R-SHIP-020 One manifest validator

- **Statement:** The structural manifest rules V1 to V40 are the one CMake function `zenith_manifest_validate`, run at every configure in `zenith_begin_build` and by the setup script in script mode (relative files resolved against `ZENITH_ROOT`, or the manifest directory's parent in script mode), collecting every violation in manifest order in the one message format; the text checks (BOM, duplicate key, size) are `Test-Zenith2ManifestText`, held for the committed manifest by a Test-step check because the configure cannot make them.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V01` to `CMake_Ship_ManifestRule_V40`, `CMake_Ship_ManifestCollectsAllErrors`, `Pwsh_Ship_ManifestText_DuplicateKeyRefused`, `Pwsh_Ship_ManifestText_Committed`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); the Configure step on every ticket.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"; Zenith2/Docs/Design/SHIP/Manifest.md → "Validation error messages"; Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"

### Z2-R-SHIP-021 Consumer entries carry the requested fields

- **Statement:** The entries other areas asked for (`sdl3`, `flecs`, `entt`, `fastgltf`, `meshoptimizer`, `ktx-software` against the manifest zstd, `mikktspace`, `renderdoc-app`, the Vulkan and D3D12 libraries, `slang` and `dxc` with one `win-x64` item each, `dxc` linked in process by `ZenithShaderCompiler`, `zlib`, `zstd` with `licenceElected`, `tracy` on all three platforms with DEVT's options, the CORE selection candidates in Z2T-CORE-053's form) carry the integration, owner, usage, targets, options and pins of "Other consumer entries" and "Selection-candidate entries"; their versions and digests are written only from verified downloads by the human pinning tickets of "Pin landing order".
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestValid_Committed` over the committed manifest after each pinning ticket, Test step; `Setup-Zenith2.ps1 -Check` in the Stage 1 acceptance.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Other consumer entries"; Zenith2/Docs/Design/SHIP/Manifest.md → "Test and benchmark entries"; Zenith2/Docs/Design/SHIP/Manifest.md → "Selection-candidate entries"

### Z2-R-SHIP-022 Cache layout and directory names

- **Statement:** Verified dependencies live in the machine-wide cache `%LOCALAPPDATA%\Zenith2\thirdparty\<name>\<dir>\` (`$HOME/.zenith2/` in WSL and on Linux) with `<dir>` `<sha256>`, `<sha256>-p<16 hex>` or `<abi>-<sha256>` and the stamp beside the tree, toolchains under `toolchain\<key>\<version>\` (a re-pinned archive of one version refused by its stamp), and published trees never modified by the setup script.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_PatchedEntryDirectoryName`, `Pwsh_Ship_DigestVerify_PublishesStampAndTree`, `Pwsh_Ship_DigestVerify_ExistingDirWithOtherStampRefused`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Cache layout"

### Z2-R-SHIP-023 Digest verification before use

- **Statement:** Every archive is verified against its manifest SHA-256 before extraction, a mismatch deletes the download and fails naming both digests, and no SHA-1 or MD5 value is ever trusted.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DigestVerify_MismatchDeletesDownload`, `Pwsh_Ship_DigestVerify_PublishesStampAndTree`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Digest verification algorithm"

### Z2-R-SHIP-024 Safe archive extraction

- **Statement:** Extraction rejects absolute paths, drive letters, `..` components, hard-link and device members, any symbolic link on a Windows host and, on a Linux host, any symbolic link whose target is absolute or leaves the extraction root (a relative in-tree link is accepted and contributes a `link` line to the tree digest), and any top-level entry outside `stripPrefix`; it publishes nothing on a rejection and sets every extracted file time to 1980-01-01T00:00:00Z; on a Linux host it lists and extracts with the snapshot-pinned system `tar` and `unzip`.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DigestVerify_ExtractionRejectsTraversal`, `Pwsh_Ship_DigestVerify_StripPrefixEnforced`, `Pwsh_Ship_LinuxArchive_MemberRules`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Archive extraction rules"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux archive extraction"

### Z2-R-SHIP-025 Tree digest and stamp

- **Statement:** Each published tree has a stamp with `sourceSha256`, `patchSetSha256`, `treeSha256` (the SHA-256 of the byte-ordered `<file sha256> <path>` lines and, in Linux trees, `link <target> <path>` lines) and `licenceSha256`, published by renaming the stamp first and the tree second, so no reader sees a directory without its stamp and the next setup deletes an orphan stamp.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DigestVerify_PublishesStampAndTree`, `Pwsh_Ship_DigestVerify_StampPublishedFirst`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Tree digest"; Zenith2/Docs/Design/SHIP/Manifest.md → "Stamp check at configure"

### Z2-R-SHIP-026 Licence file verification at setup

- **Statement:** The setup script verifies every entry's `licenceFile` (and additional licence files) against its recorded SHA-256 before publishing the tree, and `-Check` repeats the check.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DigestVerify_LicenceMismatchRefused`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Digest verification algorithm"; Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"

### Z2-R-SHIP-027 Configure-time stamp check

- **Statement:** `CMake/ZenithThirdParty.cmake` stops the configure when an entry's cache stamp disagrees with the manifest's `sourceSha256` or `patchSetSha256` or when a directory has no stamp, and never downloads anything.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_StampMismatch`, `CMake_Ship_StamplessDirectoryRefused`, the scaffold's `CMake_Stage0_ThirdPartyCacheMissing`, Test step (registered by SHIP's Stage 1 ticket T3, which lands the stamp check).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Stamp check at configure"

### Z2-R-SHIP-028 Setup script installs and verifies every pin

- **Statement:** `Setup-Zenith2.ps1` installs or verifies every toolchain pin and dependency of its host (Windows or Linux) in the order of "Setup steps" or "Linux host mode", holding a machine-wide lock, writing only under the cache root, the one validation-layer registry value and `${ZENITH_OUT}/setup/`, with the documented exit codes.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DigestVerify_PublishesStampAndTree`, `Pwsh_Ship_Setup_LockSerialises`, `Pwsh_Ship_Setup_CheckHostKeys`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); `Setup-Zenith2.ps1 -Check` exit 0 on the Stage 0 acceptance machine.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Setup-Zenith2 parameters"; Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps"

### Z2-R-SHIP-029 Check mode verifies without changing

- **Statement:** `Setup-Zenith2.ps1 -Check` changes nothing and exits 0 only when every pin and dependency of its host's key set, every dependency tree (re-hashed), licence file, git version, `vm.mmap_rnd_bits` and the checkout-hash computation match, listing every mismatch otherwise; it does not require the interpreter running it to be the pinned `pwsh`.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DigestVerify_CheckDetectsModifiedTree`, `Pwsh_Ship_Setup_CheckHostKeys`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); the CI jobs' step 2.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Check mode"

### Z2-R-SHIP-030 Licence terms are accepted by the human

- **Statement:** Installing the MSVC Build Tools, the JDK, the NDK, WARP, lavapipe and the Android SDK packages requires the licence-acceptance switch typed by the human; without it the step exits 4 and installs nothing.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_Setup_LicenceSwitchRequired`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Licence acceptance switches"

### Z2-R-SHIP-031 Environment script

- **Statement:** `Enter-Zenith2Env.ps1` sets `ZENITH_CHECKOUT_HASH` through `Get-Zenith2CheckoutHash`, imports the pinned MSVC environment on Windows, prepends the pinned tool directories, sets `ANDROID_NDK_HOME`, `JAVA_HOME` and `ZENITH2_DEV_KEYSTORE` on Windows and `SCCACHE_DIR`, `SCCACHE_CACHE_SIZE=60G` and `ZENITH2_TOOLCHAIN_ROOT` everywhere, verifies that `pwsh` resolves to the pinned interpreter, and changes no file.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-028
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_EnterEnv_SetsVariables`, `Pwsh_Ship_CheckoutHashMatchesSpec`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Enter-Zenith2Env"; Zenith2/Docs/Design/SHIP/Manifest.md → "Zenith2Env module"

### Z2-R-SHIP-032 One environment module

- **Statement:** `Zenith2/Tools/Setup/Zenith2Env.psm1` exports exactly the functions of the table "Zenith2Env module", and no SHIP script re-implements one of them.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CheckoutHashMatchesSpec`, `Pwsh_Ship_ManifestCanonicalHashAgrees`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); review of each SHIP script's imports.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Zenith2Env module"

### Z2-R-SHIP-033 Offline Gradle home with verification metadata

- **Statement:** The setup script populates a `GRADLE_USER_HOME` keyed by the AGP version and the hash of `Zenith2/ThirdParty/gradle/verification-metadata.xml` from the seed project, proves it complete with an `--offline` run, and every Gradle build verifies every resolved artefact against that metadata; it lands in SHIP's Stage 1 ticket T7, before MOD's Stage 1 APK tickets.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ExposedVariables`, Test step; MOD's Stage 1 `--offline` Minimal APK build (Z2T-MOD-003) and the Android Build step's APK build (`ApkCheck`).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Offline Gradle home"

### Z2-R-SHIP-034 Android SDK packages pinned by tree digest

- **Statement:** Each pinned Android SDK package is installed by the pinned `sdkmanager` and then verified against its recorded SHA-256 tree digest, and only after `-AcceptAndroidSdkLicenses`; it lands in SHIP's Stage 1 ticket T7.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_Setup_LicenceSwitchRequired`, Test step; `Setup-Zenith2.ps1 -Check` in the Stage 1 acceptance.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Android SDK provisioning"

### Z2-R-SHIP-035 WSL provisioning

- **Statement:** The setup script imports the pinned WSL distro, creates its `zenith` user and `wsl.conf` as root, runs every root step of the Linux host mode (the snapshot sources and pinned packages, the persisted `vm.mmap_rnd_bits=28`) through `wsl.exe -u root` and every user step through `wsl.exe -u zenith`, downloads and verifies every `linux` archive on the Windows side, extracts them in the distro with the system `tar`/`unzip`, publishes the pinned `pwsh` first with its tree digest computed in the distro by the pinned image's own tools and its stamp written from the Windows side, and runs the remaining user steps under that `pwsh`.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_LinuxHost_CommandPlan`, `Pwsh_Ship_LinuxArchive_MemberRules`, Test step; `Setup-Zenith2.ps1 -Check` in the Stage 2 acceptance (the `linux-tsan` build from the provisioned distro).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "WSL provisioning"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux package snapshot"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux archive extraction"

### Z2-R-SHIP-036 Clean-machine reproducible setup

- **Statement:** A clean Windows machine provisioned by `Setup-Zenith2.ps1` alone (with the licence switches) configures and builds every preset its host supports, and every C and C++ dependency compiles from its pinned source with the sanitizer flags in `win-asan`, `linux-tsan` and the Hwasan configuration.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** The Stage 2 acceptance record (`needs-human`): the setup report and the sanitizer builds of a freshly provisioned machine.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps"; Zenith2/Docs/Design/SHIP/Release.md → "Reproducibility definition"

### Z2-R-SHIP-037 Development signing keystore

- **Statement:** The setup script creates one development keystore per machine under the cache root with the fixed public password, the Gradle `release` build type signs with it, and it never signs a Shipping artefact; it lands in SHIP's Stage 1 ticket T7.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ExposedVariables` (`ZENITH_DEV_KEYSTORE`), Test step; MOD's Stage 1 `release` APK (Z2T-MOD-003).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Development signing keystore"; Zenith2/Docs/Design/SHIP/Release.md → "Key custody"

### Z2-R-SHIP-038 Dependency update tool

- **Statement:** `Update-Zenith2Dependency.ps1` rewrites exactly the version, source and licence-hash fields of one entry, preserving every other byte of the manifest, re-applies the entry's patches to the new tree, and exits 5 when the licence text changed, 6 when the entry has pins and `-PinsReviewed` is absent, and 7 (rewriting nothing) when a patch no longer applies, with the stated precedence.
- **Covers:** Z2-I-SHIP-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_UpdateDependency_RewritesEntryOnly`, `Pwsh_Ship_UpdateDependency_LicenceChangeExit5`, `Pwsh_Ship_UpdateDependency_PinsReviewRequired`, `Pwsh_Ship_UpdateDependency_PatchRebaseExit7`, `Pwsh_Ship_UpdateDependency_ExitPrecedence`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Update procedure"; Zenith2/Docs/Design/SHIP/Manifest.md → "Update-Zenith2Dependency"

### Z2-R-SHIP-039 Upgrade diff contents

- **Statement:** A dependency or toolchain upgrade lands as one diff containing the rewritten entry and every dependent pin, patch, policy, verification-metadata, generated-file and design-doc change, passing every conformance suite of the owning libraries.
- **Covers:** Z2-I-SHIP-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** The upgrade ticket's gate run (every owning library's conformance suite in the Test step); review against the table "What an upgrade diff contains".
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "What an upgrade diff contains"

### Z2-R-SHIP-040 Upgrades invalidate evidence

- **Statement:** A change to either manifest section changes its hash, which the evidence fingerprint includes, and SHIP provides the fingerprint fixture manifests (base, dependency bump, toolchain bump), whose hashes every consumer computes at test time; no fixture records a hash.
- **Covers:** Z2-I-SHIP-10, Z2-I-SHIP-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ManifestHashes_FingerprintFixtures`, Test step; the coverage step's fingerprint-invalidation tests.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Evidence invalidation"

### Z2-R-SHIP-041 Global flag hygiene

- **Statement:** `zenith_begin_build` removes `/EHsc`, `/EHs`, `/GR`, `/W3`, `-fexceptions` and `-frtti` from the inherited language flags and sets every configuration's flag variables to the table of BuildSystem → "Third-party targets and configuration variables", so third-party targets build every configuration optimised and every other flag comes from `zenith_apply_flags`.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-045, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_FlagHygiene`, Test step; `ForbiddenCheck` F17 on the configured tree.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Global flag hygiene"

### Z2-R-SHIP-042 Toolchain pin checks at configure

- **Statement:** The configure stops when the first three components of the MSVC `CMAKE_CXX_COMPILER_VERSION` differ from `msvc.clVersion`, the Linux clang version from the `llvm` pin, or `ANDROID_NDK_REVISION` from `ndk.pkgRevision`, with the message of [Z2-N-ARCH-194], the comparison running in `zenith_begin_build`.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ToolchainCompilerPinMismatch`, Test step (registered by SHIP's Stage 1 ticket T2, which lands the compiler pin checks).
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Toolchain files"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Global flag hygiene"

### Z2-R-SHIP-043 THIRD_PARTY name resolution

- **Statement:** `THIRD_PARTY` and `TEST_THIRD_PARTY` resolve every name by the six rules of "Name resolution", matching the lower-case manifest name exactly, treating a benchmark-target owner `Zenith_Bench_<Name>` as the benchmark `zenith_add_benchmark(<Name>)` creates and an owner not yet declared like a declared one, and stop the configure with the rule's message on the first failure.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_NameResolution_Rule1` (including a mixed-case spelling) to `CMake_Ship_NameResolution_Rule6`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Name resolution"

### Z2-R-SHIP-044 THIRD_PARTY links privately with system includes

- **Statement:** Every resolved third-party target is a `PRIVATE` link of the naming target with its include directories as `SYSTEM PRIVATE`, `TEST_THIRD_PARTY` reaches only the test, conformance and fuzz targets, and `autoLink` entries are linked by target creation.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_TargetsOwnership`, `CMake_Ship_TestThirdPartyNotInLibrary`, Test step; `Libgraph` leakage rule.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Link and include rules"

### Z2-R-SHIP-045 THIRD_PARTY on benchmarks tools and integration suites

- **Statement:** `zenith_add_benchmark`, `zenith_add_tool` and `zenith_add_integration_tests` implement `THIRD_PARTY` (and the tool `TEST_THIRD_PARTY`) with the usages rule 4 admits for each, so a selection harness links a candidate no engine library owns and a tool links its importers; benchmarks also link `ZenithTestSupport` and nanobench, `zenith_add_tool` records its tool kind and `zenith_add_benchmark` its benchmark target in the declaration store, and `zenith_add_integration_tests` accepts `TEST_LABELS`.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_BenchmarkThirdParty`, `CMake_Ship_ToolThirdParty`, `CMake_Ship_DiscoveryLabels`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests"

### Z2-R-SHIP-046 Mode prebuilt

- **Statement:** A `prebuilt` entry creates one `INTERFACE` target for the preset's ABI with its include and link lists and runtime-file properties, and a prebuilt library with a non-empty link list named by a runtime target is a configure error in sanitizer presets; the mode lands in SHIP's Stage 1 ticket T4, before the RHI pinning ticket that adds its first entries.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ThirdPartyPrebuilt`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Mode prebuilt"

### Z2-R-SHIP-047 Mode host-tool

- **Statement:** A `host-tool` entry creates `<name>::<stem>` imported executables (and an `INTERFACE` link target when it has libraries) on the `win` preset alone, every custom command that runs one names the tool file in `DEPENDS`, and the mode lands in SHIP's Stage 1 ticket T4, before the Slang pin the Stage 1 bindless probe compiles with.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ThirdPartyHostTool`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Mode host-tool"

### Z2-R-SHIP-048 Subproject options definitions and ordering

- **Statement:** Entries are created after their `dependsOn` entries, option values resolve `@ZENITH_TP_<ID>_DIR@` tokens of `dependsOn` entries only, and `definitions` become compile definitions of every target of the entry.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_DependsOnOrderAndToken`, `CMake_Ship_DefinitionsApplied`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Mode cmake-subproject"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_third_party_add_all"

### Z2-R-SHIP-049 Host executables join the host tools

- **Statement:** A third-party `hostExecutable` target (`luau-analyze`) is built with its output name into the host tools directory of the `win` tree and is a dependency of `zenith_host_tools`.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_HostExecutableInHostTools`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs"

### Z2-R-SHIP-050 Exposed toolchain and cache variables

- **Statement:** `ZenithThirdParty.cmake` defines `ZENITH_TP_<ID>_DIR` for every created entry, `ZENITH_TOOLCHAIN_<KEY>_DIR` for every pin, and on Android presets `ZENITH_JDK_DIR`, `ZENITH_ANDROID_SDK_DIR`, `ZENITH_GRADLE_HOME`, `ZENITH_DEV_KEYSTORE` and `ZENITH_GRADLE_VERIFICATION_METADATA`, each checked to exist.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ExposedVariables`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Exposed variables"

### Z2-R-SHIP-051 Pin header templates

- **Statement:** Every `*.generated.h.in` under a library or tool directory is configured automatically (no `configure_file` in any library's `CMakeLists.txt`) with the `ZENITH_TP_<ID>_*` variables of its `THIRD_PARTY` entries into `${ZENITH_OUT}/generated/<Target>/<Target>/<rel>/`, private to the target and attributed to it, and a reference to an undefined variable is a configure error.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_PinTemplateLuau`, `CMake_Ship_PinTemplateUndefinedVariable`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Pin header templates"

### Z2-R-SHIP-052 Runtime file staging

- **Statement:** `zenith_stage_runtime_files` copies the runtime files of every prebuilt entry an executable's declared closure reaches (the libraries' `ZENITH_DECL_<Lib>_THIRD_PARTY`, recorded by SHIP's `zenith_add_library` completion; and, for test executables, every `test-runs` entry) beside it after every build, selecting `pins.shippingRuntime` in `Shipping` and `ShippingTest`, and records them in `ZENITH_RUNTIME_FILES`.
- **Covers:** Z2-I-SHIP-04, Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_RuntimeStaging`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Runtime file staging"

### Z2-R-SHIP-053 Third-party target record

- **Statement:** `zenith_end_build` writes `${ZENITH_OUT}/third_party_targets.json` with one byte-ordered record per created third-party target (entry, target, owner, usage, shipping, exceptions, runtime files), rewritten only when its bytes change.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_Selection` (reads it), `CMake_Ship_DeclaredDepsAdditions`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs"

### Z2-R-SHIP-054 Conformance executables

- **Statement:** `zenith_end_build` creates `<Api>_Conformance` from the API's conformance sources and every supported backend's adapter sources, `<Name>_Conformance` for other libraries with their `TEST_DEPENDS` targets and `ZENITH_TEST_LINKS_<TARGET>` definitions, and `ZenithGraph_Conformance`, before it includes the test registry, registering each in host presets and, for a non-API library with backends among its `TEST_DEPENDS`, once per backend with `--zenith-backend` and the `[<Backend>]` suffix.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ConformanceTargets`, `CMake_Ship_TestLinksDefinitions`, Test step; every family's conformance run in the Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Conformance executables"

### Z2-R-SHIP-055 Discovery labels

- **Statement:** Every doctest discovery call the helpers make passes the executable's labels as its `LABELS` property — `TEST_LABELS` for `<Name>_Tests`, the unioned `CONFORMANCE_LABELS` for a conformance executable (RHI's `z2-parity-backend`), the suite's `TEST_LABELS` for `Zenith_Integration_<Name>` — and `ADD_LABELS 1`, so every doctest suite name is a further label merged into the same property ([Z2-N-ARCH-251]).
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ConformanceLabels`, `CMake_Ship_DiscoveryLabels`, Test step; `ctest -L <label>` selections of the gate lines.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Keywords added by SHIP"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Discovery labels"

### Z2-R-SHIP-056 Fuzz and compile-fail hand-off cannot drop a source

- **Statement:** `zenith_add_library` hands every `Tests/Fuzz/*.cpp` source to the scaffold's generic fuzz rule, which creates a target and a `.Smoke` entry for every source no area helper made a target for, and every `*_CompileFail.cpp.in` source to the registry, whose tail fails the configure when a handed-over compile-fail source has no byte-identical registered copy.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_CompileFailSourceCheck`, Test step; the scaffold's `CMake_Stage0_FuzzTargetRule`.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Fuzz and compile-fail partitions"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry tail"

### Z2-R-SHIP-057 Expected-build test environment

- **Statement:** Every registered test carries `ZENITH_EXPECTED_BUILD=<platform>:<configuration>:<sanitizer>` with the tokens of "Test environment", evaluated per configuration, `ShippingTest` reporting `shipping`.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ExpectedBuildValues`, `Pwsh_Ship_ExpectedBuildEnvironment` (Debug, Release and ShippingTest), Test step; CORE's `Test_BuildInfo_SanitizerKind_MatchesPreset`.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment"

### Z2-R-SHIP-058 Generic registry functions

- **Statement:** Death, sanitizer-defect and self-test cases are registered with CORE's `zenith_core_death_case` and `zenith_core_selftest_case` (`<Target>.<Case>`), compile-fail tests with CORE's `zenith_core_compile_fail`, and fuzz targets by the scaffold's generic rule (`Zenith_Fuzz_<Stem>.Smoke`), all unchanged; SHIP adds `zenith_register_script_test`, `zenith_register_pwsh_test` and `zenith_register_fixture_set` with the presets of "Registry functions", and the registry tail adds `RESOURCE_LOCK zenith2-build-tree` to every registry test that runs `cmake --build` in the build tree (every `z2-compile-fail` entry, and every literal-named `add_test(NAME <n> COMMAND ${CMAKE_COMMAND} --build …)` of the registry file) and applies fixture-set properties to area-made fuzz smokes.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_RegisterFixtureSet`, `CMake_Ship_CompileFailLock`, `CMake_Ship_RegistryTail`, Test step; CORE's registrations of Z2T-CORE-002 configure unchanged.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry functions"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Death-case registration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Compile-fail registration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Fuzz target registration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_register_fixture_set"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry tail"

### Z2-R-SHIP-059 declared_deps additions

- **Statement:** `declared_deps.json` records `testThirdParty` and `conformanceLabels` on libraries, `testThirdParty` on tools, and `benchmarks[]` and `integrationSuites[]` records with their `thirdParty`, deterministically, so libgraph's third-party ownership rule sees every permitted consumer.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_DeclaredDepsAdditions`, Test step; the `Libgraph` step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "declared_deps additions"

### Z2-R-SHIP-060 Assert define per configuration

- **Statement:** `zenith_apply_flags` defines `ZENITH_ASSERTS_ENABLED` as 1 in Debug, Sanitize and Hwasan and 0 in Release, Shipping and ShippingTest in every Zenith2 target, `zenith_add_tool` targets included, so no inline entity has two definitions in one link; host tools use `ZENITH_CHECK` for Release invariants.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_AssertDefinePerConfiguration`, Test step; CORE's `Test_BuildInfo_ExactlyOnePlatformAndConfiguration`.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Assert define"

### Z2-R-SHIP-061 Third-party flag rows

- **Statement:** Third-party targets — every non-`INTERFACE`, non-`IMPORTED` target an entry creates, in nested directories too — receive the sanitizer, runtime-library, debug-information and reproducibility rows plus exceptions off and RTTI off (`/EHs-c- /D_HAS_EXCEPTIONS=0 /GR-`, `-fno-exceptions -fno-rtti`), the Luau targets exceptions on (`/EHs /D_HAS_EXCEPTIONS=1`, `-fexceptions`) with RTTI off.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-045, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ThirdPartyFlagRows`, `CMake_Ship_ThirdPartyFlagsNested`, Test step; `ForbiddenCheck` F17.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Third-party flag rows"

### Z2-R-SHIP-062 Reproducibility flags

- **Statement:** From Stage 1, every Zenith2 and third-party target compiles and links with `/Brepro` on MSVC (executables with `/PDBALTPATH:%_PDB%`) and with `-ffile-prefix-map` for the checkout and output roots and `-fno-record-gcc-switches` on clang, and every Android and Linux executable and shared object links `-Wl,--build-id=sha1`, so the build ID is non-zero in every preset before SAVE's Stage 2 reads it.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ReproAndHardeningRows`, Test step; `Test-Zenith2Reproducibility.ps1` in the Stage 6 acceptance; SAVE's `Test_BuildID_RunningIsStable`.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Reproducibility flags"

### Z2-R-SHIP-063 Hardening flags in Release and Shipping

- **Statement:** From Stage 1, before the first budget run, in Release, Shipping and (MSVC) ShippingTest, every Zenith2 target compiles with `/guard:cf` (MSVC) or `-fstack-protector-strong -D_FORTIFY_SOURCE=2` (Android), every Zenith2 executable and shared object links `/guard:cf /CETCOMPAT /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT` or `-z relro -z now -z noexecstack`, and no third-party target receives a hardening row.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ReproAndHardeningRows`, `Pwsh_Ship_ReleaseChecks_WindowsBinary`, `Pwsh_Ship_ReleaseChecks_AndroidParsers`, Test step; W6 and A11 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Hardening flags"

### Z2-R-SHIP-064 Shipping differs from Release only as listed

- **Statement:** The Shipping configuration compiles with Release's optimisation, reproducibility, hardening and assert settings and differs from Release exactly in the rows D1 to D14 of "Differences from Release".
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-005, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ReproAndHardeningRows`, `CMake_Ship_ShippingExcludedLinkIsConditional`, Test step; W5, W7, W12, W13 and A6, A7 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Differences from Release"

### Z2-R-SHIP-065 Development services absent from Shipping

- **Statement:** Development services reach their implementation only inside `if constexpr (!Zenith_IsShippingBuild())`, list their entry symbols for the Shipping symbol check, and no source is excluded from the Shipping configuration by the build system.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** The `Libgraph` Shipping symbol check; W5 and A6 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Development services in Shipping"

### Z2-R-SHIP-066 Shipping link closure checks

- **Statement:** A game's Shipping link closure contains no script compiler, test, conformance, fuzz, benchmark or tool target and no development-service symbol, checked by `Libgraph` and `TestSeparationCheck` on every ticket and repeated on the packaged binary.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** The `Libgraph` and `TestSeparationCheck` steps; W5 and A6 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Shipping link closure checks"

### Z2-R-SHIP-067 CI workflow triggers

- **Statement:** `.github/workflows/zenith2-gates.yml` runs on push for the paths of "Workflow triggers" and on manual dispatch only, cancels a superseded run of the same ref, reads contents only and uses no secret.
- **Covers:** Z2-I-SHIP-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-031
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CiWorkflowShape`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); the Stage 0 acceptance's green CI run.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Workflow triggers"

### Z2-R-SHIP-068 CI jobs

- **Statement:** The workflow has exactly the jobs `plan`, `windows` and `linux` of "Jobs", the `linux` job running only when a `linux-tsan` line is active, each job's timeout the sum of its lines' step-timeout rows, on self-hosted runners provisioned once by a human.
- **Covers:** Z2-I-SHIP-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-031, Z2-D-032
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CiWorkflowShape`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Jobs"

### Z2-R-SHIP-069 CI job steps

- **Statement:** Each CI job checks out with LFS at `fetch-depth: 1`, runs `Setup-Zenith2.ps1 -Check` with the runner's own `pwsh` before building, enters the environment and runs `Run-Committed.ps1` for its host in one shell, and uploads the gate reports, in that order.
- **Covers:** Z2-I-SHIP-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-031, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CiWorkflowShape`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Job steps"

### Z2-R-SHIP-070 CI never signs measures or downloads

- **Statement:** CI runs the union of active gate lines except acceptance, never signs a release artefact, publishes, reaches a release profile or key, downloads a dependency or measures a budget row; the only signatures it makes are SHIP's signing tests', with a self-signed code-signing certificate each test creates with `CertificateRequest`, exports to a temporary PFX under its own `test-fixtures` directory (never into a certificate store) and deletes with that directory.
- **Covers:** Z2-I-SHIP-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-031, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CiWorkflowShape` (no `secrets.`, no release command), Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); PROC's subset test in `Zenith2/Tools/Gates/Tests`.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "CI invariants"

### Z2-R-SHIP-071 Device matrix

- **Statement:** The device matrix is exactly the table "Devices", with `DEV-WIN-REF` and `DEV-S24` the only validated devices, and every other class used only as "Support classes" allows.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The release notes' `## Platforms and builds` and `## System requirements` sections (`Pwsh_Ship_ReleaseDocs_Committed`), Test step; the Stage 6 qualification record naming a validated device per item.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Devices"; Zenith2/Docs/Design/SHIP/Qualification.md → "Support classes"

### Z2-R-SHIP-072 Evidence only from validated devices

- **Statement:** Every qualification item and budget measurement names one validated device, a record from any other device is refused for it, and promoting a device to validated is a decision changing the reference-hardware section in the same diff.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** The Stage 6 acceptance's check of each qualification item's device; acceptance-mode evidence validation.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Device matrix rules"

### Z2-R-SHIP-073 Release pipeline states

- **Statement:** `New-Zenith2Release.ps1` runs the states PRECONDITIONS, BUILD, STAGE, SYMBOLS, SIGN, ARCHIVE, MANIFEST, VERIFY, PUBLISH, DONE in order, so each reads only outputs an earlier state finished, records each in `state.json`, publishes only after a passing verification, resumes with `-Resume` from the failed state and restarts with `-Restart`, with the documented exit codes.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleasePipeline_ResumeFromState`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "New-Zenith2Release"; Zenith2/Docs/Design/SHIP.md → "Release pipeline state machine"

### Z2-R-SHIP-074 Release preconditions

- **Statement:** A release starts only from a clean tree, a passing `Setup-Zenith2.ps1 -Check`, a valid release profile for its channel and a game version not yet released on that channel.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleasePipeline_ResumeFromState` (PRECONDITIONS failure), `Pwsh_Ship_ReleaseProfile_PublicRequiresPublicCA`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "New-Zenith2Release"

### Z2-R-SHIP-075 Release staging layout and deterministic archives

- **Statement:** A release stages into `${ZENITH_OUT}/Release/<Game>/<version>/<channel>-<tree12>/` with the layout of "Release staging layout", including the pre-signing digests and the two reports, and its zip archives are byte-identical for the same tree.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DeterministicZip_TwiceIdentical`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Release staging layout"

### Z2-R-SHIP-076 Windows package contents

- **Statement:** The Windows package holds exactly the executable, the imported MSVC CRT DLLs, `D3D12/D3D12Core.dll`, PACK's `Content/` tree and SHIP's `Licences/` files, and every import of the executable is a packaged file or a system DLL.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseChecks_WindowsLayout`, `Pwsh_Ship_ReleaseChecks_WindowsImports`, Test step; W1, W3, W4 and W15 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Windows package"

### Z2-R-SHIP-077 Android package

- **Statement:** The shipping APK is the Gradle `shipping` build type's unsigned output, aligned and signed by the release pipeline, carrying the Shipping licence inventory under `assets/licences/`, and its Gradle project resolves no Maven runtime artefact.
- **Covers:** Z2-I-SHIP-04, Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** A1 to A8, A10 and A14 of the qualification record (`needs-human` Stage 6 acceptance); `Pwsh_Ship_ReleaseChecks_AndroidParsers`, `Pwsh_Ship_GradleTemplateDeclaresNoRuntimeDependency`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Android package"

### Z2-R-SHIP-078 Release measurement packages

- **Statement:** A `-Configuration Release` run produces an unsigned, never-public measurement package of the same tree under `<channel>-<tree12>-release/`, which install-size and qualification budget runs measure.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleasePipeline_ResumeFromState` (Release configuration case), Test step; the Stage 6 budget evidence records naming the measurement package.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Release staging layout"; Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification"

### Z2-R-SHIP-079 Key custody

- **Statement:** Release, upload, Authenticode and update-manifest keys live outside every checkout (every `git worktree list --porcelain` worktree) and `Zenith2/out/`, are named by the user-level release profile, are protected at rest by a password in an environment variable or by non-exportable certificate-store storage, are never reached by CI, and passwords are read only from the environment variables the profile names.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseProfile_PathsInsideCheckoutRefused`, `Pwsh_Ship_ReleaseProfile_PasswordNeverInProfile`, `Pwsh_Ship_ReleaseProfile_PlaintextKeyRefused`, `Pwsh_Ship_ReleaseProfile_ExportableAuthenticodeRefused`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Key custody"

### Z2-R-SHIP-080 Public releases need a public CA

- **Statement:** A `public` release requires `trust: publicCA` and a timestamp URL; `test` trust is valid only for `internal` and `candidate`, and `-Unsigned` only for `internal`.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseProfile_PublicRequiresPublicCA`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Key custody"

### Z2-R-SHIP-081 Windows Authenticode signing

- **Statement:** The game executable is signed with SHA-256 Authenticode by the profile's certificate, verified, and a byte-flipped copy reports `HashMismatch`; signing keeps the executable's build ID.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_SignedExeKeepsBuildId`, `Pwsh_Ship_Authenticode_TamperDetected`, Test step; W2 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Windows Authenticode"

### Z2-R-SHIP-082 Android APK signing

- **Statement:** The shipping APK is aligned with `zipalign -P 16`, signed with v2 and v3 schemes (v1 off, minSdk 34) by the RSA release key, and verified by `apksigner` against the profile's certificate digest and key algorithm and by `zipalign -c -P 16`.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ApksignerOutputParse`, Test step; A1 and A2 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Android APK signing"

### Z2-R-SHIP-083 App Bundle upload signing

- **Statement:** PACK's App Bundle is signed with the upload key by `jarsigner` (SHA256withRSA, SHA-256 digests) and verified strictly against the profile's upload certificate.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** A9 of the qualification record (`needs-human` Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "App Bundle upload signing"

### Z2-R-SHIP-084 Update key consistency

- **Statement:** PACK's update-manifest writer obtains the Ed25519 seed only through `Get-Zenith2UpdateSigningKey`, which decrypts the encrypted PKCS#8 key with the password from the profile's environment variable and has the pinned JDK prove the seed matches the profile's public key, and the release verification checks that the public key compiled into the executable is the profile's.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_UpdateSigningKey_Decrypts`, `Pwsh_Ship_Ed25519PublicKeyCheck_Rfc8032`, Test step; W10 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off"; Zenith2/Docs/Design/SHIP/Release.md → "Key custody"

### Z2-R-SHIP-085 Release manifest schema

- **Statement:** `release.json` has exactly the keys of "release.json schema" in order, every inapplicable value `null`, byte-ordered per-platform files (package, APK, AAB, `symbols` files, platform archive) and a top-level `files` list for the cross-platform symbols archive, each with SHA-256, size, `runDependent` (signed files, MSVC PDBs and archives containing either) and `unsignedSha256`, no timestamp and no verification result, and its reproducible form (run-dependent digests nulled) has the same `reproducibleSha256` for two runs over one tree.
- **Covers:** Z2-I-SHIP-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseManifest_SchemaAndOrder`, `Pwsh_Ship_ReleaseManifest_TwoSigningsSameReproducibleDigest`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "release.json schema"; Zenith2/Docs/Design/SHIP/Release.md → "Field rules"

### Z2-R-SHIP-086 Release manifest identities

- **Statement:** `release.json` records the commit and tree, the manifest hashes, the composition hashes, the engine cook version, the content hashes and each platform's build ID, and each build ID equals the mount manifest's `buildId`.
- **Covers:** Z2-I-SHIP-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-065, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseManifest_SchemaAndOrder`, Test step; W9 and A12 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Field rules"; Zenith2/Docs/Design/SHIP/Release.md → "Symbol identities"

### Z2-R-SHIP-087 Release archive index

- **Statement:** Publishing appends the release to `<releaseArchive>/index.json` with the SHA-256 of its `release.json`, its `reproducibleSha256` and the SHA-256 of `reports/verify.json`, refuses to overwrite an existing release directory, and the release's committed file set under `Zenith2/Docs/Release/<Game>/<version>/` is exactly `ReleaseNotes.md`, `release.json`, `licences/<platform>.json` per platform, `reports/verify.json`, `reports/reproducibility.json` and, after qualification, `Qualification.json`.
- **Covers:** Z2-I-SHIP-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleasePipeline_ResumeFromState` (PUBLISH refusal case), `Pwsh_Ship_ReleaseDocs_LintCases` (the complete set passes, a missing file fails), Test step; `Pwsh_Ship_ReleaseDocs_Committed`.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Release archive index"

### Z2-R-SHIP-088 Symbol identities

- **Statement:** The Windows build ID is the PE's CodeView RSDS GUID and the Android build ID the ELF build-ID note, read by `Get-Zenith2BinaryIdentity` and printed as lower-case hex in file order, matching SAVE's `Zenith_GetRunningBuildID()`.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_BinaryIdentity_MinimalExe`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Symbol identities"

### Z2-R-SHIP-089 Symbol store layout

- **Statement:** The symbol store uses the SymStore tiers for PDBs and executables and the `.build-id` tier for Android objects, indexes every file in `index.json`, refuses different content at an existing path and republishes idempotently.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_SymbolStore_StoresAndResolvesMinimal`, `Pwsh_Ship_SymbolStore_RefusesDifferentContent`, `Pwsh_Ship_SymbolStore_IdempotentRepublish`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Symbol store layout"

### Z2-R-SHIP-090 Symbols collected and checked

- **Statement:** A release retains the PDB and the unstripped Android object, checks they match the shipped binaries' identities, and checks `llvm-symbolizer` resolves `Zenith_PlayerMain` from them.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_SymbolStore_StoresAndResolvesMinimal`, Test step; W8 and A8 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Collecting symbols"

### Z2-R-SHIP-091 Play native debug symbols

- **Statement:** Each Android release produces a deterministic `native-debug-symbols.zip` of the unstripped object for the human to upload with the App Bundle; no Zenith2 tool uploads to a remote service.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_DeterministicZip_TwiceIdentical`, Test step; the Stage 6 acceptance record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Play native debug symbols"

### Z2-R-SHIP-092 Symbol retention

- **Statement:** Symbols of every public release are retained indefinitely, `Remove-Zenith2Symbols.ps1` refuses a public release and removes an internal or candidate one with its index rows.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_SymbolStore_PublicRetained`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Retention policy"

### Z2-R-SHIP-093 Symbolization tools

- **Statement:** `Resolve-Zenith2Address.ps1` resolves one address from the store, and `Zenith2/Tools/Setup/Symbolize-Leaks.ps1` appends function and file to each `<module>+0x<offset>` frame of a Debug leak report, leaving unresolved frames unchanged and counted.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_SymbolizeLeaks`, `Pwsh_Ship_SymbolStore_StoresAndResolvesMinimal`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Symbolization tools"

### Z2-R-SHIP-094 Crash-symbol tier follows D-SEL-CRASH

- **Statement:** When D-SEL-CRASH selects Crashpad, a Breakpad `.sym` tier produced by a pinned `dump_syms` joins the symbol store in the ruling's diff; the custom minidump alternative adds no tier.
- **Covers:** Z2-I-SHIP-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008, D-SEL-CRASH (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_SymbolStore_StoresAndResolvesMinimal` extended by the ruling ticket, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Symbol store layout"

### Z2-R-SHIP-095 Licence inventory selection

- **Statement:** The inventory of a game, platform and configuration lists exactly the runtime entries owned by libraries in the game's declared closure (excluding `shipping: excluded` ones in Shipping), their `dependsOn` closure, the platform's toolchain runtime components and the game's third-party content, never a tools or test entry, and no Gradle-resolved component because the APK resolves none; the script mode takes every input path as a required argument.
- **Covers:** Z2-I-SHIP-09, Z2-I-SHIP-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_Selection`, `CMake_Ship_LicenceInventory_ShippingExcludesTracy`, `CMake_Ship_LicenceInventory_ScriptModeArguments`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Inventory selection algorithm"

### Z2-R-SHIP-096 An unstated licence blocks packaging

- **Statement:** The `<Game>_Licences` target fails the build when a component has no licence, a licence the governing policy list does not allow, or a licence file whose SHA-256 differs from the manifest.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_RefusesBadLicenceHash`, `CMake_Ship_LicenceInventory_RefusesDisallowedLicence`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Inventory selection algorithm"

### Z2-R-SHIP-097 Notices file format

- **Statement:** `THIRD_PARTY_NOTICES.txt` lists every component in the order and format of "THIRD_PARTY_NOTICES.txt format", reproducing each licence text verbatim and stating the elected alternative of every `OR` expression.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceNoticesFormat`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "THIRD_PARTY_NOTICES.txt format"

### Z2-R-SHIP-098 Machine-readable licence inventory

- **Statement:** `licences.json` records each component's name, version, kind, licence, licence file and hash, source and owners, and ships with a byte copy of every licence file.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_Selection`, Test step; W11 and A10 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "licences.json format"

### Z2-R-SHIP-099 Licences ship in every package

- **Statement:** Every packaged build carries its configuration's inventory: `Licences/` in the Windows package and `assets/licences/` in the APK.
- **Covers:** Z2-I-SHIP-09, Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** W11 and A10 of the qualification record (`needs-human` Stage 6 acceptance); `Pwsh_Ship_ReleaseChecks_WindowsLayout`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Windows package"; Zenith2/Docs/Design/SHIP/Release.md → "Android package"

### Z2-R-SHIP-100 Repository dependency inventory

- **Statement:** The same script writes `${ZENITH_OUT}/licences/repository.json` covering every manifest entry and toolchain pin, the dependency inventory the release notes cite.
- **Covers:** Z2-I-SHIP-08, Z2-I-SHIP-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_Selection` (repository mode case), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Inventory selection algorithm"

### Z2-R-SHIP-101 Third-party content licences

- **Statement:** Cooked third-party content whose provenance names a licence appears in the inventory as a `content` component with its licence text and attribution.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LicenceInventory_Selection` (content case over a fixture `provenance-licences.json`), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Content licences"

### Z2-R-SHIP-102 Reproducible outputs

- **Statement:** Two clean builds of one tree at one checkout path with the pinned toolchain produce byte-identical unsigned game executables and Android shared objects; builds at two checkout paths produce identical Android objects, and Windows executables are compared report-only across checkout paths.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Test-Zenith2Reproducibility.ps1` reports (`Rebuild` and `CrossCheckout`) in the Stage 6 acceptance record (`needs-human`); W14 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Reproducibility definition"

### Z2-R-SHIP-103 Reproducibility check tool

- **Statement:** `Test-Zenith2Reproducibility.ps1` builds twice with the compiler cache disabled, compares every output byte for byte (Windows executables report-only in `CrossCheckout`) and reports the first differing offset and strings per file.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReproducibilityCompare_IdenticalAndDiffers`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Test-Zenith2Reproducibility"

### Z2-R-SHIP-104 Release notes

- **Statement:** Every public release and every acceptance candidate has `Zenith2/Docs/Release/<Game>/<version>/ReleaseNotes.md` with exactly the sections of "Release notes", written after VERIFY passed and linted with `-Full` by the operator before the commit, beside the rest of the committed file set of "Release archive index".
- **Covers:** Z2-I-SHIP-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseDocs_Committed`, `Pwsh_Ship_ReleaseDocs_LintCases`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Release notes"

### Z2-R-SHIP-105 Known issues

- **Statement:** The release notes' `## Known issues` table lists every open S2 or lower defect of the game's slices at release time, and an open S1 defect blocks the release.
- **Covers:** Z2-I-SHIP-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-SHIP-019
- **Acceptance evidence:** `Pwsh_Ship_ReleaseDocs_LintCases`, Test step; F4 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Known issues"

### Z2-R-SHIP-106 Upgrade guides per fix version

- **Statement:** Every fix version from `Z2 Stage 2` to `Z2 Stage 6` has `Zenith2/Docs/Release/UpgradeGuides/Z2-Stage-<N-1>-to-Z2-Stage-<N>.md` with exactly the sections of "Upgrade guides", generated from the two stage-acceptance commits and completed by the release operator, all written at the Stage 6 acceptance; subsequent fix versions' guides at their own stage acceptance.
- **Covers:** Z2-I-SHIP-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_UpgradeGuideGenerator`, `Pwsh_Ship_ReleaseDocs_Committed`, Test step; S4 of the Stage 6 qualification record (`Test-Zenith2ReleaseDocs.ps1 -Full`).
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Upgrade guides"

### Z2-R-SHIP-107 Upgrade guide generator

- **Statement:** `New-Zenith2UpgradeGuide.ps1` lists removed and added `[[deprecated]]` declarations, authoring migration fixtures added, cook and save version changes, manifest and toolchain changes between two commits.
- **Covers:** Z2-I-SHIP-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_UpgradeGuideGenerator`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Upgrade guides"

### Z2-R-SHIP-108 Release documentation lint

- **Statement:** `Test-Zenith2ReleaseDocs.ps1` refuses a release document with a missing or misordered section, a placeholder token (the regular expressions `T[B]D`, `T[O]DO`) or operator marker line, an incomplete committed file set, a committed `licences/<platform>.json` whose SHA-256 differs from `release.json`'s, or a licence component of those copies missing from the table, from the checkout alone; with `-Full` it also refuses an upgrade-guide item missing for its commits and fails loudly when either commit is unreachable, so the Test step passes on a `fetch-depth: 1` checkout; no pipeline state runs it.
- **Covers:** Z2-I-SHIP-12, Z2-I-SHIP-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseDocs_LintCases`, `Pwsh_Ship_ReleaseDocs_Committed`, `Pwsh_Ship_ReleaseDocs_FullRequiresHistory`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Release documentation lint"

### Z2-R-SHIP-109 Compatibility test list

- **Statement:** `Zenith2/Tools/Setup/compatibility_tests.json` names, for each kind of the compatibility policy, only tests some area registers, by their CTest names (doctest names `<Library>/<Type>/<Behaviour>`, registry entries by their `add_test` names), with `modules` empty by policy and every other kind non-empty, and every listed name is registered in the `win` tree as `<n>`, as `<n>[<Backend>]`, or inside a registered suite entry whose suite listing names it; the ticket landing the list and its check is blocked by SCRIPT's ticket that registers the Luau-API deprecation test, so the check is never red by design.
- **Covers:** Z2-I-SHIP-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CompatibilityKindsCovered`, Test step, green from the diff that adds it because that ticket is `blocked_by` the SCRIPT ticket registering `ZenithScript/Script/Binding_DeprecatedCallDiagnosed` (cross-area row SH14).
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Compatibility labels"; Zenith2/Docs/Design/SHIP/Release.md → "Demonstration suite"

### Z2-R-SHIP-110 Deprecated use fails the build

- **Statement:** A call to a `[[deprecated]]` declaration fails to compile under the warnings-as-errors flags, demonstrating the source-API deprecation policy.
- **Covers:** Z2-I-SHIP-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `CompileFail_Ship_DeprecatedUseIsError`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Compatibility labels"

### Z2-R-SHIP-111 Compatibility demonstration at Stage 6

- **Statement:** The Stage 6 acceptance runs every listed compatibility test with `ctest -C Release` in the `win` tree and `ctest -C Sanitize` in the `win-asan` tree (whose only configuration is `Sanitize`), and records the result in the qualification record.
- **Covers:** Z2-I-SHIP-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** S2 of the qualification record (`needs-human` Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Demonstration suite"; Zenith2/Docs/Design/SHIP/Qualification.md → "Security and compatibility items"

### Z2-R-SHIP-112 Qualification record

- **Statement:** Each qualified release has `Qualification.json` with every checklist item's kind, result and evidence, passing only when every item passes or is waived by a recorded decision.
- **Covers:** Z2-I-SHIP-03, Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** The Stage 6 acceptance record; `Test-Zenith2Release.ps1` writing the automated half.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Qualification record"

### Z2-R-SHIP-113 Windows package checklist

- **Statement:** A Windows package passes W1 to W15 of "Windows package checklist" before it is published.
- **Covers:** Z2-I-SHIP-04, Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseChecks_WindowsLayout`, `Pwsh_Ship_ReleaseChecks_WindowsBinary`, `Pwsh_Ship_ReleaseChecks_WindowsImports`, Test step; `reports/verify.json` of each release in the Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Windows package checklist"

### Z2-R-SHIP-114 Android package checklist

- **Statement:** An Android package passes A1 to A14 of "Android package checklist" before it is published.
- **Covers:** Z2-I-SHIP-04, Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseChecks_AndroidParsers`, Test step; `reports/verify.json` of each release in the Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Android package checklist"

### Z2-R-SHIP-115 Headless boot of the packaged build

- **Statement:** The packaged Windows executable boots headless for 300 ticks and exits 0 from a fresh copy of the package, and refuses `--assets-root` with exit 2.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** W12 and W13 of the qualification record (Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Windows package checklist"

### Z2-R-SHIP-116 Device runs

- **Statement:** The human runs D1 to D6 of "Device run checklist" on the validated devices from the packaged builds and records each in the qualification record.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014
- **Acceptance evidence:** D1 to D6 of the qualification record (`needs-human` Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Device run checklist"

### Z2-R-SHIP-117 Sustained thermal qualification

- **Statement:** Every WS-SUSTAIN workload holds the sustained floor through the 30-minute S24 run in ordinary conditions under the run conditions of the sustained rows, measured from the Release measurement package.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-006, Z2-B-SHIP-007, Z2-B-SHIP-008, Z2-B-SHIP-009, Z2-B-SHIP-010
- **Acceptance evidence:** MP-SUSTAIN evidence records under `Zenith2/Docs/Research/Budgets/Stage6/` (`needs-human`); D3 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Device run checklist"; Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification"

### Z2-R-SHIP-118 Frame-rate qualification

- **Statement:** Every WS-FRAME workload meets the frame-time p95 and p99 rows and the dynamic-resolution floor on both validated devices (both backends on Windows), in runs meeting the exclusion-window and run-length rows, reporting hitches.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-001, Z2-B-SHIP-002, Z2-B-SHIP-003, Z2-B-SHIP-004, Z2-B-SHIP-005, Z2-B-SHIP-011, Z2-B-SHIP-012
- **Acceptance evidence:** MP-FRAME, MP-DYNRES and MP-HITCH evidence records of the Stage 6 acceptance (`needs-human`).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification"

### Z2-R-SHIP-119 Hitch rows warm and cold

- **Statement:** Every WS-FRAME workload meets the hitch-count and maximum-frame-time rows with warm and with cold caches.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-015, Z2-B-SHIP-016, Z2-B-SHIP-017, Z2-B-SHIP-018
- **Acceptance evidence:** MP-HITCH and MP-CACHE evidence records of the Stage 6 acceptance (`needs-human`).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification"

### Z2-R-SHIP-120 Cold boot qualification

- **Statement:** Each flagship slice's installed package cold-boots within the Windows and S24 cold-boot rows in three repetitions.
- **Covers:** Z2-I-SHIP-03, Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014
- **Acceptance evidence:** MP-BOOT evidence records of the Stage 6 acceptance (`needs-human`); D5 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Device run checklist"

### Z2-R-SHIP-121 Flagship slice qualification

- **Statement:** Each flagship slice passes F1 to F6 of "Flagship slice checklist" from its packaged builds.
- **Covers:** Z2-I-SHIP-03, Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-SHIP-019, Z2-B-SHIP-020, Z2-B-SHIP-021, Z2-B-SHIP-022, Z2-B-SHIP-023
- **Acceptance evidence:** F1 to F6 of each slice's qualification record (`needs-human` Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Flagship slice checklist"

### Z2-R-SHIP-122 Qualification budget runs measure packaged builds

- **Statement:** Every SHIP budget row is measured with its own procedure on a validated device by the workload runner launching the Release measurement package of the qualified tree.
- **Covers:** Z2-I-SHIP-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-065
- **Budgets:** Z2-B-SHIP-001
- **Acceptance evidence:** The Stage 6 evidence records' `preset`, `treeSha` and artifact path naming the measurement package.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Measured rows at qualification"

### Z2-R-SHIP-123 Security and setup items at qualification

- **Statement:** The qualification record includes SEC's hardening evidence, the compatibility demonstration, a passing `Setup-Zenith2.ps1 -Check` on the release machine with manifest hashes equal to the release's, and the release documents passing `Test-Zenith2ReleaseDocs.ps1 -Full` in a full clone.
- **Covers:** Z2-I-SHIP-04, Z2-I-SHIP-11, Z2-I-SHIP-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** S1 to S4 of the qualification record (`needs-human` Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Security and compatibility items"

### Z2-R-SHIP-124 Backup rules verified

- **Statement:** The shipping APK's backup rules exclude the transient save files SAVE names and include the rest of the save directory.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseChecks_AndroidParsers` (A13 fixtures), Test step; A13 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Android package checklist"

### Z2-R-SHIP-125 CI workflow test guards the matrix

- **Statement:** `Pwsh_Ship_CiWorkflowShape` reads the committed workflow and fails on any departure from the CI matrix, so a PROC edit of the workflow keeps the specification.
- **Covers:** Z2-I-SHIP-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-031
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_CiWorkflowShape`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "CI invariants"; Zenith2/Docs/Design/SHIP/Tests.md → "PowerShell tests"

### Z2-R-SHIP-126 Bindless probe build-time shaders

- **Statement:** `zenith_add_benchmark(BindlessProbe … SLANG_SHADERS <dir> [SLANG_ALTERNATIVES <1|2|3>…])` compiles every probe shader with the pinned `slangc` once per listed alternative (default all three) to SPIR-V and DXIL on the `win` preset, defines `ZENITH_PROBE_SLANG_VERSION` and `ZENITH_BENCH_SHADER_ALTERNATIVES`, and any other benchmark passing `SLANG_SHADERS` fails the configure.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-025, D-SLANG (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_SlangShadersReservedForProbe`, `CMake_Ship_ProbeSlangVersionDefined`, Test step; the RHI bindless probe's `--smoke` run.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests"

### Z2-R-SHIP-127 Generated test root

- **Statement:** Every `<Name>_Tests` and conformance executable receives `ZENITH_TEST_GENERATED_ROOT` naming `${ZENITH_OUT}/generated/<Name>/Tests`, the one directory generated test inputs are written into.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_TestGeneratedRootDefined`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment"

### Z2-R-SHIP-128 Ownership rules at the end of the configure

- **Statement:** Rules K1 to K5 check every declared manifest owner against the declaration store in `zenith_end_build` — runtime owners are runtime libraries, tools owners libraries, tools or (for the benchmark-target shape) benchmarks, owner arrays sibling backends — giving the same verdict in every preset; an owner no call declares yet is reported at `STATUS` and recorded in `third_party_pending_owners.json`, never refused, except a case-insensitive near miss of a declared name; the script mode says it does not evaluate them.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestKindRule_K1` to `CMake_Ship_ManifestKindRule_K5`, `CMake_Ship_OwnerDeclaredLater`, `CMake_Ship_ManifestScriptModeSkipsKindRules`, Test step; the Configure step on every ticket.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs"

### Z2-R-SHIP-129 Luau closure rule keeps the exception set

- **Statement:** An `exceptions: true` element outside Scripting's exception-enabled rows, or a target an exception-enabled element links that is not itself listed, stops the configure naming Z2-D-017; unlisted `Luau.*` targets nothing links stay exceptions-off and unbuilt, and any outside target linking one stops the configure.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_LuauClosureRule`, Test step; `ForbiddenCheck` F17.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Target mapping and host executables"; Zenith2/Docs/Design/SHIP/Manifest.md → "The targets array"

### Z2-R-SHIP-130 Platform skip for benchmarks tools and integration suites

- **Statement:** A `THIRD_PARTY` name on `zenith_add_benchmark`, `zenith_add_tool` or `zenith_add_integration_tests` whose entry does not support the preset's platform is skipped with a status line instead of failing the configure, while an unknown name still fails on every preset.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_NameResolution_PlatformSkip`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Name resolution"

### Z2-R-SHIP-131 Licence expressions and elected alternatives

- **Statement:** A licence expression is allowed when its terms are: `AND` needs both operands, `OR` one alternative, which the entry records as `licenceElected` and which must itself be allowed; `licenceElected` is present exactly when the expression contains `OR`.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V27` (its `OR`, `AND` and `WITH` cases), `CMake_Ship_ManifestRule_V39`, `CMake_Ship_LicenceNoticesFormat`, Test step (V27 and V39 registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"

### Z2-R-SHIP-132 Licence policy changes are human rulings

- **Statement:** A diff that changes `Zenith2/ThirdParty/licence_policy.json` is a `needs-human` ruling ticket that appends a `Z2-D-NNN` entry naming each admitted licence term to `Zenith2/Docs/DecisionLog.md`; no loop ticket edits the file.
- **Covers:** Z2-I-SHIP-09
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-097, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** The Stage 0 reproducibility-manifest ruling's DecisionLog entry; the ticket-filing review of every subsequent policy change (cross-area row SH16 proposes the file for `reviewPaths`).
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"

### Z2-R-SHIP-133 Linux host mode

- **Statement:** On an Ubuntu 24.04 host `Setup-Zenith2.ps1` installs, through `sudo` for the root steps alone, the pinned packages from the pinned snapshot (the deb822 sources of "Linux package snapshot") and the persisted kernel setting, and as the invoking user the `linux` archives of `pwsh`, `cmake`, `ninja`, `sccache`, `llvm` and `lavapipe` where the Linux toolchain file reads them, the Linux dependency trees and the validation layer, by the same function the WSL step uses, and `-Check` verifies exactly the Linux key set.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-031
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_Setup_CheckHostKeys`, `Pwsh_Ship_LinuxHost_CommandPlan`, Test step; the `linux` CI job's step 2 and the Stage 2 acceptance's `linux-tsan` build on the native runner.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Linux host mode"; Zenith2/Docs/Design/SHIP/Manifest.md → "Check mode"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux package snapshot"; Zenith2/Docs/Design/SHIP/Manifest.md → "Linux archive extraction"

### Z2-R-SHIP-134 Setup before the validator exists

- **Statement:** While SHIP's validator does not exist (Roadmap deliverable 14 before the scaffold, and the scaffold's Stage 0 reader until T1), setup step 2 runs the bootstrap checks V1 to V4 and V40, prints that the validator is absent and continues on a pass; the deliverable-14 code's tests are registered by T1, the first SHIP ticket of Stage 1, the one recorded exception to tests in the same ticket, spanning from deliverable 14 in Stage 0 to T1 under Z2-D-128.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-128, Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ManifestBootstrap`, Test step (registered by SHIP's Stage 1 ticket T1, the first SHIP ticket after the scaffold); the Stage 0 acceptance setup report (Z2-R-SHIP-142).
- **Design:** Zenith2/Docs/Design/SHIP.md → "Stage 0 bootstrapping order"; Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps"

### Z2-R-SHIP-135 Unlisted death cases are discovered and run

- **Statement:** For every `<Lib>_Tests`, tool `<Tool>_Tests`, conformance and `Zenith_Integration_<Name>` executable in a host preset, the registry tail arranges, with no build event attached to the executable, that CTest lists the executable's `z2-death-check`, `z2-death-assert` and (when `ZENITH_SANITIZER` is set) `z2-sanitizer-defect` cases when it reads the test list and registers each case no block registered as `<Exe>.<Case>` (nor under its long-form name whose derived `Test_*` ID is registered), as `<Exe>.<Case>` with the suite's pass condition taken from one variable per suite, `z2-death-assert` cases only in the asserts-enabled configurations, and lists the discovered cases in a review record, so every death case runs and no signed-off ticket's registrations are refused.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_DeathCaseDiscovery`, `CMake_Ship_RegistryTail` (no `add_custom_command(TARGET …)` recorded), Test step; every discovered entry in the Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Unlisted death-case discovery"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Registry tail"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Death-case registration"

### Z2-R-SHIP-136 Signed outputs and the reproducible form

- **Statement:** The SIGN state records every file's pre-signing SHA-256, `release.json` marks signed files and archives containing them `runDependent` with their `unsignedSha256`, and the reproducible form excludes exactly the run-dependent digests and signing details.
- **Covers:** Z2-I-SHIP-06, Z2-I-SHIP-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseManifest_TwoSigningsSameReproducibleDigest`, `Pwsh_Ship_ReleaseChecks_WindowsLayout` (W1 `unsignedSha256` case), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Field rules"; Zenith2/Docs/Design/SHIP/Release.md → "New-Zenith2Release"

### Z2-R-SHIP-137 Update signing key encrypted at rest

- **Statement:** The Ed25519 update-signing key is an encrypted PKCS#8 file whose password comes from an environment variable, the Authenticode key is non-exportable, `Get-Zenith2ReleaseProfile` refuses a plaintext update key or an exportable Authenticode key, and `Get-Zenith2UpdateSigningKey` returns the seed only after the pinned JDK's `Ed25519PublicKeyCheck.java` reports that it matches `updateManifest.publicKey`.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseProfile_PlaintextKeyRefused`, `Pwsh_Ship_ReleaseProfile_ExportableAuthenticodeRefused`, `Pwsh_Ship_UpdateSigningKey_Decrypts`, `Pwsh_Ship_Ed25519PublicKeyCheck_Rfc8032` (RFC 8032 TEST 1, hand-copied), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Key custody"; Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off"

### Z2-R-SHIP-138 Executable imports resolve on a clean machine

- **Statement:** Every DLL a packaged Windows executable imports, delay-load imports included, is a file of the package or a Microsoft-signed system DLL or API set that is not a Visual C++ redistributable.
- **Covers:** Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseChecks_WindowsImports`, Test step; W15 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Qualification.md → "Windows package checklist"; Zenith2/Docs/Design/SHIP/Release.md → "Windows package"

### Z2-R-SHIP-139 No Maven runtime artefact in the APK

- **Statement:** The Gradle template declares no `implementation`, `api` or `runtimeOnly` dependency and the release's `shippingRuntimeClasspath` resolves none, so the licence inventory accounts for every byte of the APK.
- **Covers:** Z2-I-SHIP-09, Z2-I-SHIP-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_GradleTemplateDeclaresNoRuntimeDependency`, `Pwsh_Ship_ReleaseChecks_AndroidParsers` (A14 cases), Test step; A14 of the qualification record.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Android package"; Zenith2/Docs/Design/SHIP/Qualification.md → "Android package checklist"

### Z2-R-SHIP-140 SHIP fixtures need no recorded digest

- **Statement:** No committed SHIP fixture contains a digest of real bytes: digests are computed at test time by a function not under test, written as arbitrary literals where only compared with each other, replaced by relations, or derived from hand-written specification text.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** The fixture layout of `Zenith2/Docs/Design/SHIP/Tests.md` reviewed in each SHIP test ticket; `Pwsh_Ship_ManifestHashes_FingerprintFixtures`, `Pwsh_Ship_CheckoutHashMatchesSpec`, `CMake_Ship_LicenceNoticesFormat`, Test step (each registered in Stage 1 or a subsequent stage by the SHIP ticket that lands the code it tests).
- **Design:** Zenith2/Docs/Design/SHIP/Tests.md → "Fixture digests"

### Z2-R-SHIP-141 Entry names in lower-case kebab case

- **Statement:** Every manifest entry name matches `^[a-z0-9]+(-[a-z0-9]+)*$` (at most 64 characters), `THIRD_PARTY` matches it exactly, and the manifest name of every dependency any Zenith2 document or ticket names is the one listed in "Entry names used by consumers".
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V09` (registered by SHIP's Stage 1 ticket T1), `CMake_Ship_NameResolution_Rule1` (mixed-case spelling refused; registered by T3), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Entry names used by consumers"; Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"

### Z2-R-SHIP-142 Stage 0 reproducibility-manifest deliverable

- **Statement:** Roadmap Stage 0 deliverable 14 lands `Zenith2/ThirdParty/manifest.json` with the full `toolchain` section (every key's `licence`, `vulkanProfilesGenerator` `null`) and the doctest entry as its only dependency, `licence_policy.json` with its ruling and `allowedToolchain`, the two committed licence texts, `Setup-Zenith2.ps1`, `Enter-Zenith2Env.ps1`, `Zenith2Env.psm1` and the pinned sccache, and `Setup-Zenith2.ps1 -Check` exits 0 on the Stage 0 acceptance machine and the CI runners.
- **Covers:** Z2-I-SHIP-01, Z2-I-SHIP-08, Z2-I-SHIP-09
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-128, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** The Stage 0 acceptance: the setup report `${ZENITH_OUT}/setup/setup-report.json` of each machine and the committed files; the scaffold's green Configure over the committed manifest.
- **Design:** Zenith2/Docs/Design/SHIP.md → "Stage 0"; Zenith2/Docs/Design/SHIP.md → "Stage 0 bootstrapping order"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain licence terms"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain runtime components"

### Z2-R-SHIP-143 Stage 0 CI workflow deliverable

- **Statement:** Roadmap Stage 0 deliverable 11 lands `.github/workflows/zenith2-gates.yml` with the triggers, jobs, job steps and invariants of "CI matrix", and a push runs it green at the Stage 0 acceptance; its shape test is registered by T1 in Stage 1.
- **Covers:** Z2-I-SHIP-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-031, Z2-D-032, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** The Stage 0 acceptance's green CI run; `Pwsh_Ship_CiWorkflowShape` from T1 on (Z2-R-SHIP-125).
- **Design:** Zenith2/Docs/Design/SHIP.md → "Stage 0"; Zenith2/Docs/Design/SHIP/Qualification.md → "CI matrix"

### Z2-R-SHIP-144 Owners declared after their pins

- **Statement:** A manifest entry may name an owner that no `zenith_add_library`, `zenith_add_tool` or `zenith_add_benchmark` call declares yet: its targets are created and linked by nothing, the K rules skip that owner, the configure prints one `owner <owner> is not declared yet` line per entry and owner and records it in `third_party_pending_owners.json`, and every configure between a pinning ticket and its owner's ticket stays green.
- **Covers:** Z2-I-SHIP-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_OwnerDeclaredLater`, `CMake_Ship_ManifestKindRule_K4`, Test step; the Stage 1 acceptance's comparison of the pending-owner record with "Pin landing order".
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Validation rules"; Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values"; Zenith2/Docs/Design/SHIP.md → "Pin landing order"

### Z2-R-SHIP-145 Pins land after the mechanisms they need

- **Statement:** No dependency pin beyond doctest lands in Stage 0; every pinning ticket of "Pin landing order" is `blocked_by` the SHIP Stage 1 tickets its entries' modes need (T2 for any compiled entry, T3 for every entry, T4 for `prebuilt` and `host-tool` entries) and `blocks` the owner tickets that wait for it, the `nanobench` pinning ticket `blocks` every ticket that creates a benchmark target, each SHIP Stage 1 ticket `blocks` the consumers of the "Blocks" column of "Stage 1" (Z2T-CORE-053 waits for T2 and T3, not only for its deliverable-14 readiness rule), and a `zenith-wrapper` entry whose wrapper file its owner's ticket adds in a subsequent diff creates no target until that file exists.
- **Covers:** Z2-I-SHIP-08, Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_WrapperFileAbsent`, `CMake_Ship_FromToolchainWithoutSource`, Test step; the board's `blocked_by` and `blocks` links of every pinning ticket and every SHIP Stage 1 ticket, checked at the Stage 1 acceptance.
- **Design:** Zenith2/Docs/Design/SHIP.md → "Pin landing order"; Zenith2/Docs/Design/SHIP.md → "Stage 1"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_third_party_add_all"

### Z2-R-SHIP-146 Toolchain pin licences

- **Statement:** Every toolchain key except `vulkanProfilesGenerator` carries a `licence` term, recorded by the human who downloads its archive, governed by `licence_policy.json` → `allowedToolchain`, listed in the repository inventory with the archive URL and never shipped; a `LicenseRef-` term of a pin needs no committed text.
- **Covers:** Z2-I-SHIP-09, Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ManifestRule_V27` (toolchain cases), `CMake_Ship_ManifestRule_V40`, `CMake_Ship_ManifestValid_Committed`, Test step; the deliverable-14 ruling's DecisionLog entry.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain licence terms"; Zenith2/Docs/Design/SHIP/Manifest.md → "Toolchain pin object"; Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"

### Z2-R-SHIP-147 Update key checked by the pinned JDK

- **Statement:** `Zenith2/Tools/Setup/Ed25519PublicKeyCheck.java`, run by the pinned JDK's source launcher with the seed on standard input, proves with the standard `java.security` Ed25519 API (sign a fixed message with the seed, verify with the given public key) that a 32-byte seed and a public key belong together, exiting 0 on a match, 1 on a mismatch and 2 on malformed input, and zeroes the seed before it exits.
- **Covers:** Z2-I-SHIP-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_Ed25519PublicKeyCheck_Rfc8032` (RFC 8032 TEST 1, hand-copied; a changed key mismatches; malformed inputs exit 2), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off"

### Z2-R-SHIP-148 ShippingTest configuration in the helpers

- **Statement:** In the `win` tree's `ShippingTest` configuration every helper behaves as in `Shipping` — flag rows, per-configuration variables, imported-configuration mapping, hardening, excluded dependencies, runtime staging — `ZENITH_EXPECTED_BUILD` reports `shipping`, and `zenith_end_build`'s `zenith_tests` target, the one target of `win-shipping-test`, builds every test, conformance and integration-suite target and no game, Player, tool or benchmark.
- **Covers:** Z2-I-SHIP-04, Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_ShippingTestConfiguration`, `CMake_Ship_FlagHygiene`, `CMake_Ship_ExpectedBuildValues`, `Pwsh_Ship_ExpectedBuildEnvironment`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "ShippingTest configuration"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Global flag hygiene"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs"

### Z2-R-SHIP-149 Integration-suite labels and suite labels

- **Statement:** `zenith_add_integration_tests(… TEST_LABELS <label>…)` labels every test discovered from `Zenith_Integration_<Name>`, and every discovery call gives each test its doctest suite names as further labels through `ADD_LABELS`, never replacing the call's own labels.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_DiscoveryLabels`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Discovery labels"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests"

### Z2-R-SHIP-150 Generated test root on Android names the device

- **Statement:** On Android presets `ZENITH_TEST_GENERATED_ROOT` of every `<Name>_Tests` and conformance executable is `/sdcard/Android/data/org.zenith2.test.<target lower>/files/generated/Tests`, produced by the same function as the host form, so the test APK's pushed copy is read unchanged.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_TestGeneratedRootDefined` (both Android presets), Test step.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment"

### Z2-R-SHIP-151 Committed release document set

- **Statement:** The committed release documents of a release are exactly `ReleaseNotes.md`, `release.json`, one `licences/<platform>.json` per platform (byte copies of the packages' inventories, whose SHA-256 `release.json` records), `reports/verify.json`, `reports/reproducibility.json` and, after qualification, `Qualification.json`, under `Zenith2/Docs/Release/<Game>/<version>/`, so the release-notes lint checks components against committed copies.
- **Covers:** Z2-I-SHIP-12, Z2-I-SHIP-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-085, Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_ReleaseDocs_LintCases`, `Pwsh_Ship_ReleaseDocs_Committed`, Test step.
- **Design:** Zenith2/Docs/Design/SHIP/Release.md → "Release archive index"; Zenith2/Docs/Design/SHIP/Release.md → "Release documentation lint"

### Z2-R-SHIP-152 TEST_DEPENDS per-preset linking of test targets

- **Statement:** `<Name>_Tests` of every library links only the `TEST_DEPENDS` entries the preset supports, skips the others with no configure error and without removing `<Name>_Tests` (replacing the scaffold's unconditional link), defines `ZENITH_TEST_LINKS_<TARGET>` (the target name upper-cased, for example `ZENITH_TEST_LINKS_ZENITHRHI_D3D12`) to `1` for every linked entry and `0` for every skipped one, decides the links in `zenith_end_build` once every directory has been added, and stops the configure only for an entry that names no declared library or tool library.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_TestLinksDefinitions` (the `android-arm64` case: `TEST_DEPENDS ZenithRHI_Vulkan ZenithRHI_D3D12` links `ZenithRHI_Vulkan` alone with `ZENITH_TEST_LINKS_ZENITHRHI_VULKAN=1`, `ZENITH_TEST_LINKS_ZENITHRHI_D3D12=0` and no `FATAL_ERROR`), Test step; registered by SHIP's Stage 1 ticket T5, which `blocks` Z2T-RHI-055 (cross-area row RB21).
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test third-party links"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "End-of-build outputs"; Zenith2/Docs/Design/SHIP.md → "Stage 1"

### Z2-R-SHIP-153 KIND GENERATED branch and the shaders directory on test targets

- **Statement:** `zenith_add_library(<Name> KIND GENERATED SHADERS <Directory> DEPENDS …)` creates the `INTERFACE` target `<Name>` with the include directory `${ZENITH_OUT}/generated/<Name>` and its `DEPENDS` links, calls `zenith_add_shaders(<Name> <Directory>)` and records the target and its generated files for `declared_deps.json`, running none of the later steps; it refuses `SHADERS` on any other kind and `KIND GENERATED` without `SHADERS`, and stops the configure naming `zenith_add_shaders (CMake/ZenithShaders.cmake)` while that function is not defined. Every test, conformance, benchmark and integration-suite target receives `ZENITH_SHADERS_GENERATED_DIR`, `${ZENITH_OUT}/generated/ZenithShadersGenerated` on host presets and `/sdcard/Android/data/org.zenith2.test.<target lower>/files/shaders` on Android presets, from the same function as the generated test root; no library, tool, game, Player or generated target receives it.
- **Covers:** Z2-I-SHIP-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `CMake_Ship_KindGeneratedBranch` and `CMake_Ship_ShadersGeneratedDirDefined`, Test step; the Stage 2 SHIP evidence record (Z2T-SHIP-067) reads both forms from the compile commands.
- **Design:** Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Generated shader library"; Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "Test environment"; Zenith2/Docs/Design/SHIP.md → "Stage 1"

### Z2-R-SHIP-154 git-lfs and the content merge driver on every gate host and lane

- **Statement:** On every Windows gate host and lane, `Setup-Zenith2.ps1` checks that `git-lfs` is present at the documented minimum version 3.4.0 (exit 3 naming the step otherwise), runs `git lfs install --local` and `git lfs pull` in the checkout, and, once the `win-tools` build holds `ZenithContent.exe`, runs its `merge --install` so CONTENT's merge driver is configured; `-Check` reports both items; git-lfs is a checked prerequisite, never a toolchain pin, and these steps are the one named exception to the rule that the setup never writes inside the checkout outside `out/` (git's own state and LFS-tracked working-tree files only).
- **Covers:** Z2-I-SHIP-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Ship_GitLfs_PullsObjects`, `Pwsh_Ship_GitLfs_MissingRefused`, `Pwsh_Ship_GitLfs_CheckReportsPointers` and `Pwsh_Ship_ContentMerge_InstallsDriver`, Test step; the Stage 3 acceptance re-runs `-Only git-lfs,content-merge` and `-Check` on the gate machine, the CI runner and the lanes.
- **Design:** Zenith2/Docs/Design/SHIP/Manifest.md → "Git prerequisites"; Zenith2/Docs/Design/SHIP/Manifest.md → "Setup steps"; Zenith2/Docs/Design/SHIP/Manifest.md → "Check mode"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-SHIP-01 | reproducible builds | Z2-R-SHIP-001, Z2-R-SHIP-002, Z2-R-SHIP-003, Z2-R-SHIP-012, Z2-R-SHIP-019, Z2-R-SHIP-020, Z2-R-SHIP-022, Z2-R-SHIP-023, Z2-R-SHIP-024, Z2-R-SHIP-025, Z2-R-SHIP-027, Z2-R-SHIP-028, Z2-R-SHIP-029, Z2-R-SHIP-030, Z2-R-SHIP-031, Z2-R-SHIP-032, Z2-R-SHIP-033, Z2-R-SHIP-034, Z2-R-SHIP-035, Z2-R-SHIP-036, Z2-R-SHIP-040, Z2-R-SHIP-041, Z2-R-SHIP-042, Z2-R-SHIP-050, Z2-R-SHIP-054, Z2-R-SHIP-055, Z2-R-SHIP-056, Z2-R-SHIP-057, Z2-R-SHIP-058, Z2-R-SHIP-060, Z2-R-SHIP-062, Z2-R-SHIP-102, Z2-R-SHIP-103, Z2-R-SHIP-127, Z2-R-SHIP-133, Z2-R-SHIP-134, Z2-R-SHIP-135, Z2-R-SHIP-136, Z2-R-SHIP-140, Z2-R-SHIP-142, Z2-R-SHIP-145, Z2-R-SHIP-146, Z2-R-SHIP-148, Z2-R-SHIP-149, Z2-R-SHIP-150, Z2-R-SHIP-152, Z2-R-SHIP-153, Z2-R-SHIP-154 |
| Z2-I-SHIP-02 | CI matrix | Z2-R-SHIP-067, Z2-R-SHIP-068, Z2-R-SHIP-069, Z2-R-SHIP-070, Z2-R-SHIP-125, Z2-R-SHIP-143 |
| Z2-I-SHIP-03 | device matrix | Z2-R-SHIP-071, Z2-R-SHIP-072, Z2-R-SHIP-112, Z2-R-SHIP-116, Z2-R-SHIP-117, Z2-R-SHIP-118, Z2-R-SHIP-119, Z2-R-SHIP-120, Z2-R-SHIP-121, Z2-R-SHIP-122 |
| Z2-I-SHIP-04 | packaging | Z2-R-SHIP-052, Z2-R-SHIP-063, Z2-R-SHIP-064, Z2-R-SHIP-065, Z2-R-SHIP-066, Z2-R-SHIP-073, Z2-R-SHIP-074, Z2-R-SHIP-075, Z2-R-SHIP-076, Z2-R-SHIP-077, Z2-R-SHIP-078, Z2-R-SHIP-099, Z2-R-SHIP-112, Z2-R-SHIP-113, Z2-R-SHIP-114, Z2-R-SHIP-115, Z2-R-SHIP-120, Z2-R-SHIP-121, Z2-R-SHIP-123, Z2-R-SHIP-124, Z2-R-SHIP-138, Z2-R-SHIP-139, Z2-R-SHIP-148 |
| Z2-I-SHIP-05 | signing | Z2-R-SHIP-037, Z2-R-SHIP-077, Z2-R-SHIP-079, Z2-R-SHIP-080, Z2-R-SHIP-081, Z2-R-SHIP-082, Z2-R-SHIP-083, Z2-R-SHIP-084, Z2-R-SHIP-113, Z2-R-SHIP-114, Z2-R-SHIP-137, Z2-R-SHIP-147 |
| Z2-I-SHIP-06 | release manifests | Z2-R-SHIP-085, Z2-R-SHIP-086, Z2-R-SHIP-087, Z2-R-SHIP-136, Z2-R-SHIP-151 |
| Z2-I-SHIP-07 | symbol retention | Z2-R-SHIP-062, Z2-R-SHIP-088, Z2-R-SHIP-089, Z2-R-SHIP-090, Z2-R-SHIP-091, Z2-R-SHIP-092, Z2-R-SHIP-093, Z2-R-SHIP-094 |
| Z2-I-SHIP-08 | dependency inventory | Z2-R-SHIP-001, Z2-R-SHIP-002, Z2-R-SHIP-004, Z2-R-SHIP-005, Z2-R-SHIP-006, Z2-R-SHIP-007, Z2-R-SHIP-008, Z2-R-SHIP-009, Z2-R-SHIP-010, Z2-R-SHIP-011, Z2-R-SHIP-012, Z2-R-SHIP-013, Z2-R-SHIP-014, Z2-R-SHIP-015, Z2-R-SHIP-016, Z2-R-SHIP-017, Z2-R-SHIP-019, Z2-R-SHIP-020, Z2-R-SHIP-021, Z2-R-SHIP-043, Z2-R-SHIP-044, Z2-R-SHIP-045, Z2-R-SHIP-046, Z2-R-SHIP-047, Z2-R-SHIP-048, Z2-R-SHIP-049, Z2-R-SHIP-051, Z2-R-SHIP-052, Z2-R-SHIP-053, Z2-R-SHIP-059, Z2-R-SHIP-061, Z2-R-SHIP-095, Z2-R-SHIP-100, Z2-R-SHIP-126, Z2-R-SHIP-128, Z2-R-SHIP-129, Z2-R-SHIP-130, Z2-R-SHIP-141, Z2-R-SHIP-142, Z2-R-SHIP-144, Z2-R-SHIP-145 |
| Z2-I-SHIP-09 | licence inventory | Z2-R-SHIP-004, Z2-R-SHIP-017, Z2-R-SHIP-018, Z2-R-SHIP-026, Z2-R-SHIP-030, Z2-R-SHIP-095, Z2-R-SHIP-096, Z2-R-SHIP-097, Z2-R-SHIP-098, Z2-R-SHIP-099, Z2-R-SHIP-100, Z2-R-SHIP-101, Z2-R-SHIP-131, Z2-R-SHIP-132, Z2-R-SHIP-139, Z2-R-SHIP-142, Z2-R-SHIP-146 |
| Z2-I-SHIP-10 | dependency updates | Z2-R-SHIP-038, Z2-R-SHIP-039, Z2-R-SHIP-040 |
| Z2-I-SHIP-11 | compatibility policy | Z2-R-SHIP-109, Z2-R-SHIP-110, Z2-R-SHIP-111, Z2-R-SHIP-123 |
| Z2-I-SHIP-12 | documentation | Z2-R-SHIP-104, Z2-R-SHIP-105, Z2-R-SHIP-108, Z2-R-SHIP-123, Z2-R-SHIP-151 |
| Z2-I-SHIP-13 | upgrade guides | Z2-R-SHIP-106, Z2-R-SHIP-107, Z2-R-SHIP-108 |

## Excluded and deferred

Every SHIP inventory item is classified required, and no SHIP requirement is deferred, excluded, game-side or research. The boundaries that keep the area to its scope are capabilities owned elsewhere:

| Not in SHIP | Owner | Rationale or decision |
|---|---|---|
| the installation layout's content tree, `.zpak` archives, chunking, App Bundle modules, Play Asset Delivery packs, bundletool split-APK tests, installed-update migration, patch and DLC manifests, `update.zman`, interrupted-update recovery | PACK (Z2-I-PACK-01 to Z2-I-PACK-16) | Z2-D-027; SHIP builds, signs, inventories and verifies what PACK lays out |
| the supported API and version policy, dependency-upgrade rehearsals and their cadence, deprecated-API removal, subsystem ownership, gate baseline and profile upkeep | MAINT (Z2-I-MAINT-01 to Z2-I-MAINT-08) | Z2-D-048, Z2-D-049; SHIP provides the update mechanism and the upgrade guides |
| editing the protected gate steps and the CI workflow | PROC (Z2-I-PROC-01 to Z2-I-PROC-03) | Z2-D-128; SHIP specifies the CI matrix and the inputs the gates read |
| the evidence-fingerprint computation, the workload runner and the in-Player budget-run mode | DEVT, RESEARCH | Z2-D-065; SHIP's qualification consumes their records |
| crash dumps and the crash handler | DEVT (Z2-I-DEVT-19, Z2-I-DEVT-20) | D-SEL-CRASH (open, Stage 2); SHIP retains the symbols they need |
| trust boundaries, parser limits, fuzzing, authenticated update verification | SEC | Z2-D-042; SHIP's qualification cites SEC's evidence |
| uploading builds or symbols to a store or remote service | none | no Zenith2 tool uploads anything; the human uploads through the store's console |
| consoles and their certification | SCOPE | Z2-D-071 (Stage 7, SDK-gated) |
