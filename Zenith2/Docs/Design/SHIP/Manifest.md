# SHIP design — the reproducibility and dependency manifest

Companion to `Zenith2/Docs/Design/SHIP.md`. This file specifies `Zenith2/ThirdParty/manifest.json` field by field, the validation the build and the setup script apply to it, the machine-wide dependency cache and its digest verification, the clean-machine setup script and the environment scripts, and the dependency-update procedure. It elaborates `Zenith2/Docs/Architecture/BuildSystem.md` → "Toolchain pins and the setup script", → "Third-party integration" and → "The dependency cache", and `Zenith2/Docs/Architecture.md` → "Third-party dependencies"; where this file adds a field or a rule those sections do not state, the addition is marked `<!-- elaboration -->` and listed, with the cross-document item (`reviews/CROSS_DOC_ITEMS.md` rows X220 and X239) that carries its core wording, in `Zenith2/Docs/Design/SHIP.md` → "Reconciliations with the core documents". Every SHIP ticket implementing such an addition stays `draft` until its row is applied.

## Scope of this file

| Topic | Section |
|---|---|
| the file, its two sections and how it is hashed | "Manifest file" |
| every `toolchain` key and its JSON shape | "The toolchain section" |
| every `dependencies` field, owner token, mode, target mapping, prebuilt item, recorded pin and licence field | "The dependencies section" |
| the one validator and its messages | "Manifest validation" |
| the exact entries the consumer areas asked for | "Consumer entries" |
| the cache, digest verification, extraction, tree digest, patches and the configure-time stamp check | "Dependency cache" |
| `Setup-Zenith2.ps1`, `Enter-Zenith2Env.ps1`, `Zenith2Env.psm1`, the offline Gradle home, the Android SDK, WSL and the development keystore | "Setup script" |
| upgrading a pin | "Dependency updates" |

## Manifest file

### Top-level shape

`Zenith2/ThirdParty/manifest.json` is one UTF-8 JSON file without a byte-order mark, LF line endings, tab indentation, at most 1 MiB. Its top level has exactly these keys, in this order:

```json
{
	"schema": "zenith2.third-party-manifest",
	"schemaVersion": 1,
	"toolchain": { "msvc": { }, "llvm": { } },
	"dependencies": [ ]
}
```

| Key | Rule |
|---|---|
| `schema` | the string `zenith2.third-party-manifest` |
| `schemaVersion` | the integer `1`; any other value is the validation error `schemaVersion: <n> is not supported` |
| `toolchain` | an object whose keys are exactly the keys of the table "Toolchain keys" (none missing, none extra) |
| `dependencies` | an array of dependency entries, sorted by byte order of `name`, each `name` unique |

The manifest is a tracked, unprotected file. It changes only in a diff that a human prepares, because only a human can download and verify an archive; a loop worker never edits a digest (the worker-evidence ruling: a worker never records a measured value).

### Canonicalisation and hashing

The evidence fingerprint hashes the two sections separately (`Zenith2/Docs/Architecture/BuildSystem.md` → "Toolchain pins and the setup script"; `Zenith2/Docs/Budgets.md` → "Evidence records and the input fingerprint", fields `toolchain` and `thirdParty`).

<!-- elaboration -->
- `toolchainHash` is the lower-case hexadecimal SHA-256 of the **canonical form** of the `toolchain` value; `dependenciesHash` is the SHA-256 of the canonical form of the `dependencies` value; `manifestSha256` is the SHA-256 of the file's bytes as committed.
- The canonical form of a JSON value is its serialisation with object keys in byte order, no insignificant whitespace, strings escaped minimally (only `"`, `\` and control characters, as `\uXXXX` with lower-case hexadecimal), integers in shortest decimal form, `true`, `false`, `null`; arrays keep their order. Floating-point numbers do not occur in the manifest (validation rejects them, V29), and both canonical functions refuse one with a message naming the value, so a fixture that holds one is compared on refusal.
- The canonical form is computed by one function, `Get-Zenith2CanonicalJson` in `Zenith2/Tools/Setup/Zenith2Env.psm1` (§ "Zenith2Env module"), and by the CMake function `_zenith_manifest_canonical(<json> <out>)` in `Zenith2/CMake/ZenithThirdParty.cmake`; the test `Pwsh_Ship_ManifestCanonicalHashAgrees` proves the two produce byte-identical output over the committed manifest and over the fixture manifests of `Zenith2/CMake/Tests/Ship/`.
- Whitespace-only edits to the manifest change `manifestSha256` and no other hash, so reformatting never invalidates evidence.

## The toolchain section

### Toolchain pin object

Every toolchain key holds a pin object. The fields a pin object may carry:

| Field | Type | Meaning |
|---|---|---|
| `version` | string | the exact upstream version or release tag |
| `windows`, `linux` | archive object | the pinned archive for that host: `{ "url": string, "sha256": 64 lower-case hex, "stripPrefix": string, "format": "zip" \| "tar.gz" \| "tar.xz" \| "nupkg" \| "exe" }`; `exe` marks a self-contained installer or binary that is not extracted; a `linux` archive serves the WSL distro and the native Linux CI host alike (§ "Linux host mode") |
| `wsl` | package object | only on `python`: the interpreter installed inside the WSL distro and on the native Linux host from the pinned package repository, `{ "package": string, "version": string }` (exact Debian version string); no other key uses this field |
| `licence` | string | **required on every toolchain key except `vulkanProfilesGenerator`** (validation rule V40; the generator's terms are those of the `vulkan-profiles` entry it is linked to): the SPDX expression of the tool's own licence terms, governed by `licence_policy.json` → `allowedToolchain` (rule V27). It feeds the developer-facing repository inventory of `Zenith2/Docs/Design/SHIP/Release.md` → "Inventory selection algorithm" and is never shipped, so a toolchain pin carries no `licenceFile`: the inventory names the identifier and the archive URL. A file the toolchain contributes to a package is a `runtimeComponents` element with its own `licence`, `licenceFile` and `licenceSha256` (below) |
| `runtimeComponents` | array | redistributable runtime files the tool provides and a packaged build ships (§ "Toolchain runtime components") |

<!-- elaboration -->
A key that pins something other than an archive (the MSVC instance, the Android SDK packages, AGP) carries the specific fields of its row in "Toolchain keys" in place of `windows`/`linux`.

### Toolchain keys

The keys are exactly the rows of `Zenith2/Docs/Architecture/BuildSystem.md` → "The toolchain section", with these JSON shapes:

| Key | JSON shape | Notes |
|---|---|---|
| `msvc` | `{ "vsVersion": "17.x.y", "clVersion": "19.xx.yyyyy", "toolsetVersion": "14.xx.yyyyy", "windowsSdk": "10.0.xxxxx.0", "channelUri": string, "bootstrapper": archive object (`format: "exe"`), "components": [string, …], "licence": string, "runtimeComponents": [ … ] }` | `clVersion` is what `zenith_begin_build` compares with `CMAKE_CXX_COMPILER_VERSION` ([Z2-N-ARCH-194]): MSVC reports four dot-separated components (`19.xx.yyyyy.z`), and the comparison takes the first three components of the reported value and requires them to equal `clVersion` as strings; `toolsetVersion` and `windowsSdk` are what `Get-Zenith2MsvcEnvironment` passes to `VsDevCmd.bat` ([Z2-N-CONTRACT-429]); `components` are the Visual Studio installer component IDs, sorted by byte order |
| `llvm` | pin object with `windows` and `linux` archive objects | the LLVM project's release archives of one `version` (the Windows `clang+llvm-<version>-x86_64-pc-windows-msvc.tar.xz` and the Linux `LLVM-<version>-Linux-X64.tar.xz` or the release's equivalently named x86_64 Linux archive); installed into `toolchain/llvm/<version>/` on Windows, inside the WSL distro and on the native Linux CI host (§ "WSL provisioning", § "Linux host mode"), never from a distro package; provides `clang`, `clang++`, `clang-tidy`, `clang-format`, `llvm-readelf`, `llvm-readobj`, `llvm-nm`, `llvm-strip`, `llvm-symbolizer`, `llvm-pdbutil`, `llvm-objcopy`; the setup script checks each of these exists in the extracted `bin/`. `CMAKE_CXX_COMPILER_VERSION` of the Linux clang is compared with `version` component-wise over its three components (the X220 item that fixes this one form for BuildSystem's `llvm` row) |
| `cmake`, `ninja`, `sccache` | pin object with `windows` and `linux` | `version` is compared with the tool's `--version` output by `-Check` |
| `slang` | pin object with `windows` | offline tool only; consumed by the `slang` dependency entry through `fromToolchain` (§ "The prebuilt array") |
| `dxc` | pin object with `windows` | offline tool only; consumed by the `dxc` dependency entry through `fromToolchain` |
| `warp` | pin object with `windows` (`format: "nupkg"`) | consumed by the `warp` dependency entry through `fromToolchain` |
| `lavapipe` | pin object with `windows` and `linux` | the pinned build includes the Win32 WSI; consumed by the `lavapipe` dependency entry through `fromToolchain` |
| `ndk` | pin object with `windows`, plus `"pkgRevision": "27.2.12479018"`-shaped string | `pkgRevision` is the NDK's `source.properties` `Pkg.Revision`, the value the Android configure compares with `ANDROID_NDK_REVISION` ([Z2-N-ARCH-194]) and the value of `@ZENITH_NDK_VERSION@` |
| `jdk` | pin object with `windows` | JDK 17 |
| `gradle` | `{ "version": string, "distribution": archive object (`format: "zip"`), "licence": string }` | `distribution.sha256` is `@ZENITH_GRADLE_SHA256@` and the wrapper's `distributionSha256Sum` ([Z2-N-ARCH-862]) |
| `agp` | `{ "version": string, "licence": "Apache-2.0" }` | at least `8.5.1` ([Z2-N-ARCH-244]); its artefacts are verified by the Gradle verification metadata (§ "Offline Gradle home"), not by an archive hash |
| `androidSdk` | `{ "compileSdk": 35, "targetSdk": 35, "commandLineTools": archive object, "packages": [ { "id": "platforms;android-35", "path": "platforms/android-35", "treeSha256": 64 hex }, … ], "licence": string }` | `packages` holds exactly `platforms;android-<compileSdk>`, `build-tools;<version>` and `platform-tools`, sorted by `id`; `compileSdk` and `targetSdk` are `@ZENITH_COMPILE_SDK@` and `@ZENITH_TARGET_SDK@` |
| `wsl` | `{ "distro": "Ubuntu-24.04", "rootfs": archive object, "snapshot": "YYYYMMDDTHHMMSSZ", "packages": [ { "package": string, "version": string }, … ], "licence": string }` | the distro name `Sanitizers.ps1` passes to `wsl.exe -d` ([Z2-N-ARCH-217]); `snapshot` names the `snapshot.ubuntu.com` date every package is installed from (§ "Linux package snapshot"); `packages`, sorted by `package`, are the system packages the LLVM archive, the archive extraction and the `linux-tsan` build need: at least `git`, `libc6-dev`, `libstdc++-14-dev`, `python3` (at the `python` pin's `wsl` version), `tar`, `unzip` and `xz-utils`, and the rest the pinning ticket lists; the native Linux CI host installs the same list (§ "Linux host mode"); `licence` is the terms of the root filesystem image |
| `python` | pin object with `windows` and `wsl` | the analyzers' interpreter; also the developer-only Vulkan-Profiles generator's |
| `pwsh` | pin object with `windows` and `linux` | the gate steps' and SHIP scripts' interpreter; installed inside WSL and on the native Linux host from the `linux` archive so script-mode checks run on the `linux-tsan` host too |
| `vulkanProfilesGenerator` | `{ "version": string, "linkedEntry": "vulkan-profiles" }`, or `null` | developer-only; `null` exactly while the manifest has no `vulkan-profiles` entry (the Stage 0 manifest), otherwise the version equals that entry's `version` (validation rule V31) |

The values in this section are written by the Stage 0 reproducibility-manifest ticket (Roadmap Stage 0 deliverable 14) from verified downloads; this document fixes their shapes, never the literal versions.

### Toolchain licence terms

<!-- elaboration -->
Every toolchain key except `vulkanProfilesGenerator` carries `licence` (rule V40), and every such term must be in `licence_policy.json` → `allowedToolchain` (rule V27, § "Licence fields"). The terms below are the ones the deliverable-14 ruling records when it commits the Stage 0 manifest; the human who downloads each archive reads its licence and records the term found, and when a pinned release's terms differ from this table the ruling records the term found and this table is corrected in the same diff (Z2-D-085):

| Key | `licence` | Notes |
|---|---|---|
| `msvc` | `LicenseRef-Microsoft-Visual-Studio-Build-Tools` | the Build Tools licence the `-AcceptToolchainLicenses` switch accepts |
| `llvm` | `Apache-2.0 WITH LLVM-exception` | both archives |
| `cmake` | `BSD-3-Clause` | |
| `ninja` | `Apache-2.0` | |
| `sccache` | `Apache-2.0` | |
| `slang` | `Apache-2.0 WITH LLVM-exception` | the Slang release's `LICENSE` |
| `dxc` | `NCSA` | the DirectXShaderCompiler release's `LICENSE.TXT` |
| `warp` | `LicenseRef-Microsoft-WARP` | the NuGet package's licence |
| `lavapipe` | `MIT` | Mesa |
| `ndk` | `LicenseRef-Android-SDK-License` | |
| `jdk` | `GPL-2.0-only WITH Classpath-exception-2.0` | the OpenJDK distribution |
| `gradle` | `Apache-2.0` | |
| `agp` | `Apache-2.0` | |
| `androidSdk` | `LicenseRef-Android-SDK-License` | the agreement the `-AcceptAndroidSdkLicenses` switch accepts |
| `wsl` | `LicenseRef-Ubuntu-WSL-Image` | the root filesystem image |
| `python` | `PSF-2.0` | |
| `pwsh` | `MIT` | |
| `vulkanProfilesGenerator` | — (no `licence`; the `vulkan-profiles` entry's terms) | |

A `LicenseRef-<Id>` term of a toolchain pin needs no committed text, because nothing of the pin ships; the repository inventory lists it with the archive URL, and the person running `Setup-Zenith2.ps1` accepts the terms through the switches of § "Licence acceptance switches".

### Toolchain runtime components

<!-- elaboration -->
A packaged build ships files that come from the toolchain rather than from a dependency: the MSVC C++ runtime DLLs the Windows executable imports (the build uses `MultiThreadedDLL`, `Zenith2/Docs/Architecture/BuildSystem.md` → "MSVC flags for the win and win-asan presets"), and statically linked toolchain code whose licence a notice must state (the NDK's `libc++` through `ANDROID_STL c++_static`, and the compiler runtime builtins). Each is a `runtimeComponents` element of its toolchain key:

```json
{ "id": "msvc-crt", "platform": "windows", "kind": "app-local-dll",
  "files": [
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/concrt140.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/msvcp140.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/msvcp140_1.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/msvcp140_2.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/msvcp140_atomic_wait.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/msvcp140_codecvt_ids.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/vccorlib140.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/vcruntime140.dll",
    "VC/Redist/MSVC/<redistVersion>/x64/Microsoft.VC143.CRT/vcruntime140_1.dll"
  ],
  "licence": "LicenseRef-Microsoft-Visual-Cpp-Redistributable",
  "licenceFile": "ThirdParty/licences/LicenseRef-Microsoft-Visual-Cpp-Redistributable.txt",
  "licenceSha256": "<64 hex digits>" }
```

The `files` of `msvc-crt` are exactly the DLLs of the pinned toolset's `x64/Microsoft.VC143.CRT/` redistributable directory, byte-ordered (the nine names above for the current toolset family; the deliverable-14 human lists the directory of the pinned `<redistVersion>` and records every DLL in it), because the packaging step copies whichever of them the executable imports (`Zenith2/Docs/Design/SHIP/Release.md` → "Windows package"). The `ndk` key carries two `static-runtime` components, `ndk-compiler-rt` and `ndk-libcxx`, each `platform: "android"`, `files: []`, `licence: "Apache-2.0 WITH LLVM-exception"` and `licenceFile: "ThirdParty/licences/llvm-project-LICENSE.txt"`.

The committed licence texts under `Zenith2/ThirdParty/licences/` are the texts no pinned archive carries at a stable path. The Stage 0 set is exactly two files, added by the deliverable-14 ticket with their `licenceSha256` recorded by the human: `LicenseRef-Microsoft-Visual-Cpp-Redistributable.txt` (the redistributable terms, copied from the pinned Build Tools' licence) and `llvm-project-LICENSE.txt` (the `LICENSE.TXT` of the LLVM release whose version equals the `llvm` pin, the text of the NDK's `libc++` and compiler runtime). Each further file joins in the pinning or ruling ticket that first names it.

| Field | Rule |
|---|---|
| `id` | `^[a-z][a-z0-9-]{0,63}$`, unique across the section |
| `platform` | `windows` or `android` |
| `kind` | `app-local-dll` (copied next to the executable), `static-runtime` (linked into the binary; licence only) |
| `files` | for `app-local-dll`, paths relative to the toolchain install root; for `static-runtime`, an empty array |
| `licence`, `licenceFile`, `licenceSha256` | as in "Licence fields"; a `licenceFile` beginning `ThirdParty/licences/` is a committed text file in this repository (for licences whose text is not inside a pinned archive), otherwise a path inside the toolchain install root |

A runtime component ships, so its `licence` is governed by `licence_policy.json` → `allowedRuntime` like a `usage: runtime` dependency (validation rule V27); the `msvc-crt` licence `LicenseRef-Microsoft-Visual-Cpp-Redistributable` and the NDK runtime's `Apache-2.0 WITH LLVM-exception` (the `ndk` key's `static-runtime` components `ndk-libcxx` and `ndk-compiler-rt`) are in that list from the first policy (§ "Licence fields").

The packaging step copies exactly the `app-local-dll` files whose base names appear in the executable's import table (`llvm-readobj --coff-imports`), and `Zenith2/Docs/Design/SHIP/Release.md` → "Inventory selection algorithm" lists every component it copied and every `static-runtime` component of the platform.

## The dependencies section

### Dependency entry fields

The fields of `Zenith2/Docs/Architecture/BuildSystem.md` → "The manifest dependencies section", with the elaborations this design adds (marked "SHIP"):

| Field | Type | Presence | Rule |
|---|---|---|---|
| `name` | string | always | lower-case kebab case, `^[a-z0-9]+(-[a-z0-9]+)*$`, at most 64 characters (validation rule V9; the orchestrator's 2026-10-03 manifest-names ruling); unique; the value `THIRD_PARTY` names, matched exactly (no case folding) |
| `version` | string | always | the upstream release or tag, exactly |
| `source` | archive object | always except `prebuilt`/`host-tool` entries that use `fromToolchain` | `{ "url", "sha256", "stripPrefix", "format" }`; `url` is `https://`; `sha256` 64 lower-case hex; `stripPrefix` the single top-level directory of the archive, or `""` when the archive has none; `format` is `zip`, `tar.gz` or `tar.xz` (a source archive is never `nupkg` or `exe`); validation rule V4 covers it like every other archive object |
| `licenceElected` | string | SHIP; required exactly when `licence` contains `OR` | the one alternative of the `OR` expression under which Zenith2 uses the dependency (an expression without `OR`, equal to one of the top-level alternatives); validation rule V27 checks it against the licence policy and the notices state it (`Zenith2/Docs/Design/SHIP/Release.md` → "THIRD_PARTY_NOTICES.txt format") |
| `cmakeSourceDir` | string | optional, `cmake-subproject` only | a safe relative path (validation rule V36) |
| `prebuilt` | array | required for every `prebuilt` and `host-tool` entry (with or without `fromToolchain`); absent for every other mode | § "The prebuilt array"; for a `fromToolchain` entry each item carries `abi` and the layout fields only (rule V22) |
| `fromToolchain` | string | SHIP; optional, `prebuilt` and `host-tool` only | a key of the `toolchain` section whose archive supplies this entry's artefact (Slang, DXC, WARP, lavapipe); the entry then has no `source`, and each `prebuilt` item carries `abi` and only the layout fields (`include`, `link`, `runtime`, `runtimeSubdir`, `executables`), never `url`, `sha256`, `stripPrefix` or `format`; each item's `abi` must name a host the toolchain key has an archive for (`win-x64` needs a `windows` archive, `linux-x64` a `linux` archive): `lavapipe` therefore has a `win-x64` and a `linux-x64` item |
| `licence` | string | always | an SPDX license expression; `LicenseRef-<id>` for a licence SPDX does not list |
| `integration` | enum | always | `cmake-subproject`, `zenith-wrapper`, `header-only`, `prebuilt`, `host-tool` |
| `owner` | string or array | always | § "Owner values" |
| `platforms` | array | always | non-empty, unique subset of `android`, `linux`, `windows`, in any order (BuildSystem's row states a subset and no order; a pinning ticket that writes `["windows", "android"]`, Z2T-CORE-053, is valid); every reader treats it as a set |
| `language` | enum | always | `c`, `cpp`, `binary` (`binary` exactly for `prebuilt` and `host-tool`) |
| `options` | object | optional, `cmake-subproject` only | `{ "common": {…}, "windows": {…}, "android": {…}, "linux": {…} }`; every value a string; a value may contain the token `@ZENITH_TP_<ID>_DIR@` naming a `dependsOn` entry's cache directory (§ "Mode cmake-subproject" in `Zenith2/Docs/Design/SHIP/CMakeHelpers.md`) |
| `definitions` | object | SHIP; optional, `cmake-subproject` and `zenith-wrapper` | the same per-platform shape as `options`; each key–value pair becomes `<KEY>=<value>` in `target_compile_definitions(<target> PRIVATE …)` of every target of the entry (for example `LUA_USE_LONGJMP=0` on Luau) |
| `dependsOn` | array | SHIP; optional | names of other entries this entry's build consumes (`ktx-software` on `zstd`); the CMake walk creates every `dependsOn` entry first; the graph is acyclic |
| `licenceFile` | string | always | a safe relative path (V36) inside the extracted tree (or `ThirdParty/licences/…` as for runtime components) |
| `licenceSha256` | 64 hex | always | the SHA-256 of that file's bytes |
| `sanitizerExempt` | boolean | optional | `true` only for a `prebuilt` entry with `usage` `tests` or a runtime redistributable that nothing links ([Z2-N-ARCH-234]); requires `decision` |
| `decision` | string | with `sanitizerExempt`, or with `targets[].exceptions` | a `Z2-D-NNN` present in `Zenith2/Docs/DecisionLog.md` |
| `patches` | array | always (may be empty) | § "Patch entries" |
| `usage` | enum | always | `runtime`, `tools`, `tests` |
| `shipping` | enum | SHIP; optional, `usage: runtime` only | `included` (the default) or `excluded`; § "Shipping exclusion field" |
| `targets` | array | optional | § "The targets array" |
| `pins` | object | SHIP; optional | § "Recorded pins" |

<!-- elaboration -->
Every field the table marks "SHIP" is an addition to the BuildSystem table; its core wording is in `Zenith2/Docs/Architecture/BuildSystem.md` → "The manifest dependencies section", from cross-document item X220 (item 1) for the fields that row lists, X239 for `licenceElected`, `source.format`, `targets[].platforms` and the lower-case kebab names, and X268 for the toolchain pins' `licence`, the benchmark-target owner and the owner that is declared after its pin ([Z2-N-ARCH-200]), all three applied (`reviews/CROSS_DOC_ITEMS.md`; cross-area row SH1 points to all three). The scaffold's Stage 0 reader (which validates only the always-present fields) ignores them; it also requires `source` on every entry, so no `fromToolchain` entry is pinned before the SHIP ticket that replaces that reader lands (`Zenith2/Docs/Design/SHIP.md` → "Pin landing order").

### Owner values

| `owner` value | Meaning | Who may name the entry |
|---|---|---|
| a Zenith2 library, tool or tool-library name (`ZenithPlatform`, `ZenithCook`) | the one target whose `Internal/` may include the dependency's headers ([Z2-N-ARCH-200]) | that target, through `THIRD_PARTY` (usage `runtime` or `tools`) or `TEST_THIRD_PARTY` (usage `tests`); and any test, benchmark or tool target through its own `THIRD_PARTY` ([Z2-N-ARCH-200]'s exception) |
| a benchmark target name `Zenith_Bench_<Name>` (`Zenith_Bench_HeapSelection`) | a selection candidate owned by the one harness that measures it before its ruling, the form CORE's pinning ticket Z2T-CORE-053 writes; equivalent to the token `benchmark` restricted to that harness | that benchmark through its `THIRD_PARTY`; and any test or tool target through its own `THIRD_PARTY` |
| an array of library names | a `header-only` entry shared by sibling backends of one API library (`renderdoc-app`: `["ZenithRHI_D3D12", "ZenithRHI_Vulkan"]`), every element `KIND BACKEND` with the same `BACKEND_OF` | each listed library; test, benchmark and tool targets as above |
| `tests` | test infrastructure linked into every test, conformance, fuzz and integration target automatically when `autoLink` is true (doctest) | nothing names it |
| `benchmark` | linked only into benchmark targets: automatically into every `zenith_add_benchmark` target when `autoLink` is true (nanobench), otherwise only into a benchmark that names it in `THIRD_PARTY` (a selection candidate no engine library owns yet: `mimalloc`, `xxhash`, `yyjson`, `simdjson`, the D-SEL-UPSCALER (open, Stage 5) candidates) | `zenith_add_benchmark` targets |
| `test-runs` | a runtime redistributable no target links, staged next to test executables or referenced by the shared gate environment (WARP, lavapipe, the Vulkan validation layer) | nothing names it |
| `host` | a host executable or library built from a `cmake-subproject` and linked by no Zenith2 target (`luau-analyze` and the Luau analysis libraries); only inside `targets[]` | nothing names it |

<!-- elaboration -->
- `autoLink` (boolean, default `false`) is accepted only with the tokens `tests` and `benchmark`.
- The tokens are reserved: no library, tool or test target may be named `tests`, `benchmark`, `test-runs` or `host` (target names match `^Zenith[A-Za-z0-9]+(_[A-Za-z0-9]+)?$` anyway).
- Which declared targets may be owners is checked against the declaration store at the end of the configure (rules K1 to K5 of § "Validation rules"), never in script mode, because only the configure knows each target's kind.
- An owner may be named before the ticket that declares it lands: a pin is a `needs-human` diff and its owner's `zenith_add_library` is an ordinary loop ticket, so they cannot land together, and every consumer hand-off asks for its pin first (`Zenith2/Docs/Design/SHIP.md` → "Pin landing order"). Until the owner is declared the entry's targets are created and linked by nothing, the K rules skip that owner, and the configure prints one `STATUS` line per such owner (rule K4); the K rules apply in full from the configure in which the owner is declared.
- A benchmark that measures a library natively (`Zenith_Bench_ECSNative` and `Zenith_Bench_BudgetsECS` linking `flecs` and `entt`) names the entry in its own `THIRD_PARTY`; the entry keeps its one library owner, because [Z2-N-ARCH-200] already admits benchmark targets. No second owner is recorded in the manifest (the alternative to the ECS request for "second owners", recorded in `findings/A3_cross_area_notes.md` row SH7).
- When a selection decision admits a candidate into an engine library (D-SEL-HEAP (open, Stage 1) choosing mimalloc), the ticket implementing the ruling changes that entry's `owner` from its harness (`Zenith_Bench_HeapSelection`, or the token `benchmark`) to the library and its `usage` from `tools` or `tests` to `runtime` in the same diff ([Z2-N-ARCH-121]).

### Integration modes in detail

| Mode | Cache content | Targets created | Entry fields required | Exposed variables |
|---|---|---|---|---|
| `cmake-subproject` | the upstream source tree | the upstream project's own targets, through `add_subdirectory(… EXCLUDE_FROM_ALL)` | `source`; `targets` whenever the upstream target names differ from `name` | `ZENITH_TP_<ID>_DIR` |
| `zenith-wrapper` | the upstream source tree | the targets `ThirdParty/<name>/CMakeLists.txt` creates from `ZENITH_THIRD_PARTY_SOURCE_DIR` | `source`; `targets` when the wrapper creates more than one target or a target not named `name` | `ZENITH_TP_<ID>_DIR` |
| `header-only` | the upstream source tree | one `INTERFACE` target named `name`, include directory `<cache>/<include>` where `include` is `pins.includeDir` or the tree root | `source` | `ZENITH_TP_<ID>_DIR` |
| `prebuilt` | the extracted binary archive per ABI | one `INTERFACE` target named `name` carrying include directories and import libraries; runtime files recorded on it | `prebuilt` or `fromToolchain` | `ZENITH_TP_<ID>_DIR`, `ZENITH_TP_<ID>_RUNTIME_FILES` |
| `host-tool` | the extracted tool archive | one `IMPORTED` executable target `<name>::<exe stem>` per `executables` element, plus an `INTERFACE` target `name` when `link` is non-empty | `prebuilt` or `fromToolchain` | `ZENITH_TP_<ID>_DIR`, `ZENITH_TP_<ID>_EXE_<STEM>` |

`<ID>` is `name` upper-cased with every `-` replaced by `_` (`d3d12-agility-sdk` → `D3D12_AGILITY_SDK`, `ktx-software` → `KTX_SOFTWARE`, `luau` → `LUAU`). Because V9 admits only lower-case letters, digits and single hyphens, two distinct names never map to one ID; rule V12 still checks it, so a future change of V9 cannot introduce a collision silently.

The CMake behaviour of each mode is `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "ZenithThirdParty.cmake".

### The targets array

```json
"targets": [
	{ "target": "Luau.VM", "owner": "ZenithScript", "usage": "runtime", "exceptions": true },
	{ "target": "Luau.CodeGen", "owner": "ZenithScript", "usage": "runtime", "exceptions": true },
	{ "target": "Luau.Ast", "owner": "ZenithScriptCompiler", "usage": "tools", "exceptions": true },
	{ "target": "Luau.Compiler", "owner": "ZenithScriptCompiler", "usage": "tools", "exceptions": true },
	{ "target": "Luau.Config", "owner": "host", "usage": "tools", "exceptions": true },
	{ "target": "Luau.Analysis", "owner": "host", "usage": "tools", "exceptions": true },
	{ "target": "Luau.EqSat", "owner": "host", "usage": "tools", "exceptions": true },
	{ "target": "Luau.Analyze.CLI", "owner": "host", "usage": "tools", "exceptions": true, "hostExecutable": "luau-analyze", "platforms": ["windows"] }
]
```

(`Luau.EqSat` stands for the further library targets the pinned revision's `Luau.Analysis` links; the pinning ticket lists exactly the ones its revision has.)

| Field | Rule |
|---|---|
| `target` | the CMake target name the upstream (or wrapper) build creates; checked to exist after `add_subdirectory` on every platform of the element (`FATAL_ERROR "Zenith2: <name>: target <target> listed in targets does not exist"`) |
| `owner` | a library or tool name, or the token `host`; never an array, never `tests`, `benchmark` or `test-runs` |
| `usage` | `runtime`, `tools` or `tests`; a `runtime` target's owner is a runtime library (rule K5) |
| `exceptions` | SHIP; boolean, default `false`; `true` compiles the target with the exceptions-on row (`/EHs /D_HAS_EXCEPTIONS=1` on MSVC, `-fexceptions` on clang, [Z2-N-SCRIPT-027]) instead of the exceptions-off row; accepted only on the targets of the entry named `luau` (Z2-D-017, [Z2-N-ARCH-176]; validation rule V20), which needs no `decision` field because Z2-D-017 is the decision |
| `hostExecutable` | SHIP; optional; the output name of a host executable target (`luau-analyze`); the target's `RUNTIME_OUTPUT_DIRECTORY` becomes the host tools directory (`${ZENITH_OUT}/win/<Config>/bin`, so the Release build of `win-tools` lands in `ZENITH_HOST_TOOLS_DIR`) and `zenith_host_tools` depends on it; accepted only with `owner: "host"` and `usage: "tools"` |
| `platforms` | SHIP; optional; a non-empty unique subset of the entry's `platforms`, in any order (rule V33), default the entry's `platforms`; on a preset of another platform the element is ignored: its target is not required to exist, not linked and not counted. `Luau.Analyze.CLI` carries `["windows"]`, because `LUAU_BUILD_CLI` is `ON` on Windows alone and the Android and Linux trees create no CLI target |

- An entry without `targets` has exactly one target, named `name`.
- `THIRD_PARTY <name>` named by a target X links exactly the entry's targets whose `owner` is X (or all of them when the entry has no `targets` and X is the owner); naming an entry of which X owns no target is the configure error of [Z2-N-ARCH-200].
- The exception-enabled set is the table of `Zenith2/Docs/Scripting.md` → "Exception-enabled target list" and never grows here. The `luau` entry's `exceptions: true` elements realise exactly that table's Luau rows for the pinned revision: `Luau.VM`, `Luau.CodeGen`, `Luau.Ast`, `Luau.Compiler`, `Luau.Config`, `Luau.Analysis`, `Luau.Analyze.CLI`, and the further library targets in the link closure of `Luau.Analysis`. The configure checks it with the closure rule of `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Target mapping and host executables": an `exceptions: true` element outside those rows, or a non-`INTERFACE` target the entry creates that an exception-enabled element links and that is not itself listed with `exceptions: true`, stops the configure naming the target and stating that adding it to the list needs a decision recorded against Z2-D-017 ([Z2-N-SCRIPT-021]). A pinned revision whose `Luau.Analyze.CLI` links a CLI helper library outside `Luau.Analysis`'s closure therefore cannot be pinned without that ruling; the pinning ticket is `needs-human` and carries the ruling, or pins a revision that needs none.
- Every other target the entry creates (on Windows, the further CLI executables `LUAU_BUILD_CLI` adds, such as `Luau.Repl.CLI`, `Luau.Compile.CLI`, `Luau.Reduce.CLI`, `Luau.Bytecode.CLI`, `Luau.Ast.CLI`, and any library only they link) is left unlisted: it receives the ordinary third-party rows (exceptions off, RTTI off), no Zenith2 target can name it, and it is never built, because the subproject is added `EXCLUDE_FROM_ALL` and nothing depends on it. `zenith_end_build` fails the configure when any target outside the entry links one of them.

### The prebuilt array

```json
"prebuilt": [
	{
		"abi": "win-x64",
		"url": "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.x.y",
		"sha256": "<64 hex digits>",
		"stripPrefix": "",
		"format": "nupkg",
		"include": ["build/native/include"],
		"link": [],
		"runtime": ["build/native/bin/x64/D3D12Core.dll", "build/native/bin/x64/d3d12SDKLayers.dll"],
		"runtimeSubdir": "D3D12",
		"executables": []
	}
]
```

| Field | Rule |
|---|---|
| `abi` | `win-x64`, `android-arm64-v8a`, `android-x86_64`, `linux-x64`; unique within the array; must cover every platform of `platforms` that a preset of that platform builds (`windows` → `win-x64`; `android` → both Android ABIs only when the entry is used on Android; `linux` → `linux-x64`) |
| `url`, `sha256`, `stripPrefix`, `format` | the archive object fields; absent when the entry has `fromToolchain` |
| `include` | relative directories exposed as `SYSTEM` include directories of the `INTERFACE` target |
| `link` | relative paths of import or static libraries linked through the `INTERFACE` target (tools only: a `prebuilt` C or C++ library linked into a runtime target is the configure error of [Z2-N-ARCH-201] in sanitizer presets, and validation rule V22 refuses it in every preset for `usage: runtime`) |
| `runtime` | relative paths of files copied next to every executable whose link closure reaches the entry (§ "Runtime file staging" in `Zenith2/Docs/Design/SHIP/CMakeHelpers.md`); for `owner: "test-runs"` entries, next to every test, conformance, integration and benchmark executable |
| `runtimeSubdir` | the directory under the executable's directory that receives `runtime` files (`D3D12` for the Agility SDK, matching `szZENITH_D3D12_AGILITY_SDK_PATH`), or `""` for the executable's own directory |
| `executables` | `host-tool` only: relative paths of the tool executables (`bin/slangc.exe`, `bin/dxc.exe`); each becomes `<name>::<stem>` |
| `manualDownload` | SHIP; optional, in place of `url`, on an item of a `prebuilt` entry without `fromToolchain`: the file name (`^[A-Za-z0-9._-]+\.zip$`) of an archive that only a person with partner access can download; the item keeps `sha256`, `stripPrefix` and `format` (`zip`), and the setup script never downloads it (§ "Manual-download items") |
| `importLibrary` | SHIP; optional, `win-x64` items only: relative paths of import libraries, each `<stem>.lib` whose `<stem>.dll` is a `runtime` path of the same item (`steam_api64.lib` for `steam_api64.dll`); linked through the `INTERFACE` target like `link`, and admitted for `usage: runtime`, where `link` must stay empty, because an import library carries no compiled code of the dependency: the DLL it names is loaded by the OS loader and staged like every `runtime` file |

Every path of an item (`stripPrefix`, `include`, `link`, `importLibrary`, `runtime`, `executables`) and `runtimeSubdir` is a safe relative path (rule V36).

### Manual-download items

<!-- elaboration -->
Answers ONLINE's row ON9 (a) (A5 row OnB14): the Steamworks SDK is downloadable only by a logged-in Steamworks partner, so its entry cannot carry a public `url`, and a machine without partner access must still configure and build everything else.

- **Setup.** For an item with `manualDownload`, `Setup-Zenith2.ps1` looks for the file `thirdparty\manual\<manualDownload>` of the cache (§ "Cache layout") and never opens a network connection for it; `-MirrorRoot` is not consulted. A present file is verified, extracted and published exactly as a downloaded archive (§ "Digest verification algorithm"), so a SHA-256 mismatch exits 1 and publishes nothing. An absent file prints `Zenith2 setup: <name>: manual download <file> not present in <directory>; entry unavailable`, publishes nothing for the entry and is not a failure (exit 0), because only machines with partner access hold the file; with `-Only <name>` naming the entry the same absence exits 3 (missing prerequisite). `-Check` reports the entry `unavailable` and exits 0; `-Check -Only <name>` exits 3.
- **Configure.** An entry whose item for the preset's ABI has `manualDownload` and whose cache directory is absent is **unavailable**: it creates no target, `ZENITH_TP_<ID>_AVAILABLE` is `0` (it is `1` for every created entry), and the configure prints `Zenith2: <name>: manual download not in the dependency cache; entry unavailable` once. A library that names it in `THIRD_PARTY` is not created in that configure (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Name resolution", § "Unavailable entries" there). A present directory whose stamp disagrees still stops the configure, as for every entry.
- **Records.** `third_party_targets.json` holds no record for an unavailable entry, the licence inventory therefore lists it in no package, and the K rules evaluate its owner as for any entry (the owner is declared whether or not it is created).
- The manual-download form is accepted on `prebuilt` entries only (rule V4): a source archive, a `host-tool` item and a toolchain archive always carry a public `url`.

### Recorded pins

<!-- elaboration -->
`pins` holds facts about the pinned version that a human records when adding or upgrading the entry and that code reads at build time (§ "Pin header templates" of `Zenith2/Docs/Design/SHIP/CMakeHelpers.md`), or that a test re-measures to detect drift.

| Rule | Value |
|---|---|
| key | `^[a-z][A-Za-z0-9]{0,63}$`, not one of the reserved names `version`, `name`, `dir`, `source`, `licence`, `runtimeFiles`, `exe` |
| value | a string, a non-negative integer below 2⁶³, a boolean, an array of strings, or an object whose values are strings, integers or booleans (one level) |
| `includeDir` | the one key every mode reads: a `header-only` entry's include directory relative to the tree root; absent means the tree root itself (doctest: the upstream tree has `doctest/doctest.h` at its root, so every test's `#include <doctest/doctest.h>`, `Zenith2/Docs/WorkerGuide.md`, resolves against the root and the entry carries no `includeDir`) |

The pins each consumer reads are listed with its entry in § "Consumer entries". A pin a test re-measures (Luau's `apiLinkage`, Vulkan-Profiles' `jsonSha256`) fails that test when the measurement differs, never silently updates.

### Shipping exclusion field

`shipping: "excluded"` marks a runtime dependency compiled into non-Shipping configurations alone ([Z2-N-ARCH-124] for `tracy`; the RenderDoc in-application API, `renderdoc-app`). The owner's link becomes `$<$<NOT:$<CONFIG:Shipping,ShippingTest>>:<target>>` (BuildSystem's `shipping` row; `ShippingTest`, the `win`-tree configuration that builds test targets with Shipping's code, [Z2-N-ARCH-231], excludes the dependency exactly as `Shipping` does, so a Shipping-behaviour test sees the shipped link), and the licence inventory of a Shipping package omits the entry (`Zenith2/Docs/Design/SHIP/Release.md` → "Inventory selection algorithm"). The owner reaches the dependency only from code inside `if constexpr (!Zenith_IsShippingBuild())` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information").

- For a compiled entry (`tracy`), a Shipping link that still references the dependency fails with an unresolved symbol instead of shipping it.
- For a `header-only` entry (`renderdoc-app`) the conditional link removes nothing, because the dependency has no object code: its header compiles into the owner in every configuration. The guarantee then rests on the owner's `if constexpr` entry and on `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` listing the owner's capture entry points (`Libgraph`, [Z2-N-ARCH-964]) and on W5 and A6 of the release verification, which scan the retained symbols for those names and for the `RENDERDOC_` prefix.

### Licence fields

- `licence` is an SPDX license expression (`MIT`, `BSD-3-Clause`, `Apache-2.0 WITH LLVM-exception`, `MIT OR Apache-2.0`). A licence not on the SPDX list is `LicenseRef-<Id>` with `<Id>` matching `^[A-Za-z0-9.-]+$`, and its full text is the `licenceFile`.
- `licenceFile` is the licence text the notices reproduce verbatim. For an archive that carries several licence files (a multi-licensed project, a bundled sub-library), the entry names the top-level one and adds `pins.additionalLicenceFiles` (array of paths) with `pins.additionalLicenceSha256` (array, same order); the notices reproduce all of them.
- `licenceSha256` is checked by `Setup-Zenith2.ps1` on extraction and by `-Check` ([Z2-N-ARCH-125]'s inventory relies on it).
<!-- elaboration -->
- The licences admitted are the allow-list `Zenith2/ThirdParty/licence_policy.json`:

```json
{
	"schema": "zenith2.licence-policy",
	"schemaVersion": 1,
	"allowedRuntime": ["Apache-2.0", "Apache-2.0 WITH LLVM-exception", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0", "LicenseRef-Microsoft-Agility-SDK", "LicenseRef-Microsoft-Visual-Cpp-Redistributable", "MIT", "MIT-0", "Zlib"],
	"allowedTools": ["Apache-2.0", "Apache-2.0 WITH LLVM-exception", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0", "MIT", "MIT-0", "NCSA", "Zlib"],
	"allowedTests": ["Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0", "LicenseRef-Microsoft-WARP", "MIT", "MIT-0", "Zlib"],
	"allowedContent": ["CC0-1.0", "CC-BY-4.0", "OFL-1.1"],
	"allowedToolchain": ["Apache-2.0", "Apache-2.0 WITH LLVM-exception", "BSD-3-Clause", "GPL-2.0-only WITH Classpath-exception-2.0", "LicenseRef-Android-SDK-License", "LicenseRef-Microsoft-Visual-Studio-Build-Tools", "LicenseRef-Microsoft-WARP", "LicenseRef-Ubuntu-WSL-Image", "MIT", "NCSA", "PSF-2.0"]
}
```

- The file has exactly these seven keys, in this order. Each list holds **licence terms**: an SPDX identifier, a `LicenseRef-<Id>`, or one `<identifier> WITH <exception>` pair, sorted by byte order. Which list governs a component: `allowedRuntime` for `usage: runtime` entries and every toolchain runtime component (both ship); `allowedTools` for `usage: tools` entries; `allowedTests` for `usage: tests` entries; `allowedContent` for the third-party content components of `Zenith2/Docs/Design/SHIP/Release.md` → "Content licences"; `allowedToolchain` for the `licence` of every toolchain pin (§ "Toolchain licence terms"), which is never shipped, so `allowedToolchain` may admit terms (a GPL-with-exception JDK, the Android SDK agreement, the Build Tools licence) that no shipping list admits.
- `allowedToolchain` above is exactly the set of terms of § "Toolchain licence terms", so the committed Stage 0 manifest passes V27 and V40; it is part of the deliverable-14 ruling, which stays `needs-human` and records each term the human found (a found term that differs from the table changes both in that ruling's diff).
- An expression is evaluated over its terms (rule V27): a term is allowed when it is in the governing list; `A AND B` is allowed when both operands are; `A OR B` is allowed when at least one alternative is, and the entry then records the alternative it uses as `licenceElected`, which must itself be allowed; parentheses group as SPDX defines. `zstd` (`BSD-3-Clause OR GPL-2.0-only`) is therefore admitted with `licenceElected: "BSD-3-Clause"`, and the notices state both the upstream expression and the elected alternative.
- Admitting a licence term is a licence-compliance ruling, never an ordinary edit: a diff that changes `licence_policy.json` is a `needs-human` ruling ticket whose diff appends its `Z2-D-NNN` entry to `Zenith2/Docs/DecisionLog.md` (Z2-D-097) naming each added term. A loop ticket never edits the file; cross-area row SH16 proposes to PROC that `Zenith2/ThirdParty/licence_policy.json` join the Z2 categories' `reviewPaths`, so a diff touching it is always reviewed. The initial list above is admitted by the Stage 0 reproducibility-manifest ticket's ruling (Roadmap Stage 0 deliverable 14, `needs-human`), which records the terms of every dependency of `Zenith2/Docs/Architecture.md` → "Third-party dependencies" and of every toolchain pin (§ "Toolchain licence terms").
- Paths: at configure, `licence_policy.json`, every `patches[].file` and every `ThirdParty/licences/…` `licenceFile` resolve against `ZENITH_ROOT` (the `Zenith2/` directory); in script mode (`-DZENITH_MANIFEST_CHECK=<manifest path>`) they resolve against the parent of the manifest's directory, which is the same `Zenith2/` directory for the committed manifest and the case directory's root for a fixture (`Zenith2/CMake/Tests/Ship/Manifest/V<nn>/` holds `manifest.json` under a `ThirdParty/` subdirectory with its own `licence_policy.json` when the rule reads one). Rules V25 and V27 name the resolved path in their message when the file is absent (`file <path> does not exist`).

### Patch entries

```json
"patches": [ { "file": "ThirdParty/patches/ktx-software/0001-use-external-zstd.patch", "sha256": "<64 hex digits>", "decision": "Z2-D-NNN" } ]
```

- `file` is a repo-relative path (relative to `Zenith2/`) under `ThirdParty/patches/<name>/`, named `NNNN-<slug>.patch`, applied in array order; `sha256` is the patch file's SHA-256; `decision` is the ruling that approved the change ([Z2-N-ARCH-199]).
- A patch is a unified diff with paths relative to the extracted tree root (`a/<path>`, `b/<path>`), applied by `git apply --unsafe-paths --directory=<extracted root> -p1 --whitespace=nowarn` from the pinned working directory (§ "Patch application").

## Manifest validation

### Validation rules

<!-- elaboration -->
There is one implementation of the structural rules: `zenith_manifest_validate(<json> <out_errors>)` in `Zenith2/CMake/ZenithThirdParty.cmake`, which evaluates rules V1 to V40. It runs in two places: `zenith_begin_build` runs it at every configure before any target exists (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Global flag hygiene"), and the setup script runs the same function in CMake script mode (`cmake -DZENITH_MANIFEST_CHECK=<path> -P Zenith2/CMake/ZenithThirdParty.cmake`), so the configure and the setup script agree on every V rule. Two further kinds of check sit elsewhere, and the setup script does not see either:

- **Text checks** CMake's JSON reader cannot make — a duplicated object key, a byte-order mark and the 1 MiB limit — are `Test-Zenith2ManifestText` in `Zenith2Env.psm1` (`ConvertFrom-Json -AsHashtable` raises on a duplicate key). The setup script runs it before the validator; the configure cannot, so the committed manifest is held to it by `Pwsh_Ship_ManifestText_Committed`, which runs it over `Zenith2/ThirdParty/manifest.json` in every `Test` step.
- **Ownership rules** K1 to K5 need each owner's target kind, which exists only after every `zenith_add_library`, `zenith_add_tool` and `zenith_add_benchmark` call has run. `zenith_end_build` evaluates them against the declaration store (`ZENITH_DECL_<Lib>_KIND`, written by `zenith_add_library` before its `SUPPORTED` check, `Zenith2/Docs/Design/MOD/GameBuild.md`; `ZENITH_DECL_<Name>_TOOL_KIND`, `EXECUTABLE` or `LIBRARY`, written by SHIP's `zenith_add_tool` the same way; and `ZENITH_DECL_Zenith_Bench_<Name>_BENCHMARK`, `1`, written by SHIP's `zenith_add_benchmark` the same way). Because the store holds every declared target whether or not the preset creates it, the K rules give the same verdict in every configure preset. Script mode evaluates no K rule and says so in its last line (`Zenith2: ThirdParty/manifest.json: structure valid; ownership rules K1-K5 run at configure`).
- **Owners declared after their pins.** A target-name owner absent from the store is not an error: it is the state between a pinning ticket and the ticket that declares the owner (`Zenith2/Docs/Design/SHIP.md` → "Pin landing order"), and every configure of that stretch must stay green. K1, K2, K3 and K5 evaluate only declared owners (an owner array is checked over its declared elements); K4 reports each undeclared owner at `STATUS` and refuses only a near miss. A misspelt owner is therefore caught three ways: by K4's case-insensitive near-miss refusal; by name-resolution rule 3 the moment the intended owner names the entry (`… is owned by <owner>; only the owner, test, benchmark and tool targets may name it`, `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Name resolution"); and by the pending-owner record `${ZENITH_OUT}/third_party_pending_owners.json` (`[{ "entry", "owner" }]`, byte-ordered), which each stage acceptance from Stage 1 compares with the table of `Zenith2/Docs/Design/SHIP.md` → "Pin landing order" (an owner listed there whose declaring ticket is Done is a defect).

| Rule | Check |
|---|---|
| V1 | top-level keys exactly `schema`, `schemaVersion`, `toolchain`, `dependencies` |
| V2 | `schema` and `schemaVersion` as § "Top-level shape" |
| V3 | `toolchain` has every key of "Toolchain keys" and no other |
| V4 | every archive object (`source`, every `prebuilt` item with its own archive, every toolchain archive): `url` begins `https://`; `sha256` matches `^[0-9a-f]{64}$`; `format` is one of `zip`, `tar.gz`, `tar.xz`, `nupkg`, `exe`, and a `source` archive's `format` is `zip`, `tar.gz` or `tar.xz`; an item of a `prebuilt` entry carries exactly one of `url` and `manualDownload`, `manualDownload` matches `^[A-Za-z0-9._-]+\.zip$` with `format` `zip`, and no other archive object carries `manualDownload` |
| V5 | `msvc.clVersion` matches `^19\.[0-9]{2}\.[0-9]{5}$`; `toolsetVersion` matches `^14\.[0-9]{2}\.[0-9]{5}$`; `windowsSdk` matches `^10\.0\.[0-9]{5}\.0$` |
| V6 | `agp.version` is at least `8.5.1` by component-wise numeric comparison |
| V7 | `ndk.pkgRevision` matches `^27\.[0-9]+\.[0-9]+$` and `ndk.version` is `r27c` |
| V8 | `androidSdk.packages` holds exactly the three package kinds, sorted by `id` |
| V9 | every entry `name` matches `^[a-z0-9]+(-[a-z0-9]+)*$` and has at most 64 characters |
| V10 | `dependencies` sorted by byte order of `name`; names unique |
| V11 | every always-present field of "Dependency entry fields" present with its JSON type |
| V12 | entry IDs (`<ID>`) unique |
| V13 | `integration` and `language` agree (`binary` exactly for `prebuilt` and `host-tool`) |
| V14 | `source` present unless `fromToolchain` is; `fromToolchain` names a `toolchain` key with a `windows` or `linux` archive |
| V15 | `cmakeSourceDir` only on `cmake-subproject`; `options` only on `cmake-subproject`; `definitions` only on `cmake-subproject` and `zenith-wrapper`; every `options` and `definitions` value a string |
| V16 | `owner` is a target-name-shaped string (`^Zenith[A-Za-z0-9]+(_[A-Za-z0-9]+)?$`), a benchmark target name (`^Zenith_Bench_[A-Za-z0-9]+$`), a token of "Owner values", or (for `header-only` only) an array of at least two distinct target-name-shaped strings, sorted |
| V17 | `autoLink` only with `tests` or `benchmark` |
| V18 | owner shape agrees with usage: a `usage: runtime` entry's owner is a target name or an owner array, never a token and never a benchmark target name; a `usage: tests` entry's owner is a target name, a benchmark target name, `tests`, `benchmark` or `test-runs`; a `usage: tools` entry's owner is a target name or a benchmark target name; `host` appears only inside `targets[]` (the kind half is K1 to K5) |
| V19 | `targets[]` fields as "The targets array"; every `owner` of a `targets` element is a target name or `host`; `target` values unique within the entry |
| V20 | `exceptions: true` only on targets of the entry named `luau` |
| V21 | `hostExecutable` only with `owner: "host"`, `usage: "tools"`, unique across the manifest |
| V22 | `prebuilt[]` present, non-empty and with the fields of "The prebuilt array" on every `prebuilt` and `host-tool` entry, absent on every other entry; an item of an entry without `fromToolchain` has its own archive fields, an item of a `fromToolchain` entry has `abi` and no `url`, `sha256`, `stripPrefix` or `format`, and its `abi` names a host the toolchain key has an archive for (`win-x64` ↔ `windows`, `linux-x64` ↔ `linux`); `abi` unique within the array; `link` empty for `usage: runtime`; `importLibrary` only on `win-x64` items, each path ending `.lib` whose file name with `.dll` in place of `.lib` is the file name of a `runtime` path of the same item |
| V23 | `sanitizerExempt` only on `prebuilt` entries with an empty `link` in every item, and with `decision` |
| V24 | every `decision` value matches `^Z2-D-[0-9]{3}$` |
| V25 | `patches[]`: `file` under `ThirdParty/patches/<name>/` named `NNNN-<slug>.patch`, `sha256` hex, `decision` present; the file exists |
| V26 | `licence` parses as an SPDX expression (identifiers `^[A-Za-z0-9.+-]+$`, operators `AND`, `OR`, `WITH`, parentheses); `licenceElected`, when present, parses and contains no `OR` |
| V27 | the licence of every entry, toolchain pin and toolchain runtime component is allowed by the list § "Licence fields" names for it (`allowedToolchain` for toolchain pins), evaluated over the expression (`AND` needs both operands, `OR` one alternative, a `WITH` pair is one term); with `OR`, `licenceElected` equals one top-level alternative and is itself allowed; `licence_policy.json` exists at the resolved path and has the schema of § "Licence fields" (its seven keys in order), each list sorted and duplicate-free |
| V28 | `shipping` only on `usage: runtime` entries |
| V29 | `pins` keys and values as "Recorded pins"; no floating-point number anywhere in the file |
| V30 | `dependsOn` names existing entries; the dependency graph is acyclic |
| V31 | `toolchain.vulkanProfilesGenerator` is `null` exactly when no `vulkan-profiles` entry exists; otherwise its `version` equals that entry's `version` and `linkedEntry` is `vulkan-profiles` |
| V32 | the consumer-specific pins of the table below are present, with their types, on every entry the table names that the manifest contains (an absent entry is not an error) |
| V33 | `platforms` of every entry and of every `targets[]` element: non-empty, values in `android`, `linux`, `windows`, unique, in any order; a `targets[]` element's `platforms` a subset of its entry's |
| V34 | enumerated values: `integration`, `usage`, `language`, `shipping`, `prebuilt[].abi`, `runtimeComponents[].platform` and `.kind` take only the values of their tables; `autoLink`, `sanitizerExempt` and `exceptions` are booleans |
| V35 | every `runtimeComponents` element has the fields of § "Toolchain runtime components" with their types; `id` unique across the section; `files` non-empty for `app-local-dll` and empty for `static-runtime`; `licenceSha256` 64 lower-case hex |
| V36 | every path field (`cmakeSourceDir`, `licenceFile`, `stripPrefix`, the `prebuilt` item paths (`importLibrary` included) and `runtimeSubdir`, `runtimeComponents[].files` and `.licenceFile`, `pins.includeDir`, `pins.additionalLicenceFiles`, `pins.shippingRuntime`, `patches[].file`) is a safe relative path: non-empty unless the field admits `""` (`stripPrefix`, `runtimeSubdir`), `/` separators only (no backslash), no leading `/`, no drive letter (`^[A-Za-z]:`), no `.` or `..` component, no empty component |
| V37 | `pins.additionalLicenceFiles` and `pins.additionalLicenceSha256` both absent or both arrays of equal length; every hash 64 lower-case hex |
| V38 | the `luau` entry's pin relations: `pins.commit` is 40 lower-case hex and occurs in `source.url` and `source.stripPrefix`; `pins.releaseTag` equals `version`; `bytecodeMin ≤ bytecodeTarget ≤ bytecodeMax` |
| V39 | `licenceElected` present exactly when `licence` contains `OR` |
| V40 | every toolchain key except `vulkanProfilesGenerator` carries `licence`, a string that parses as V26 states; `vulkanProfilesGenerator` carries none |
| K1 | (end of configure) the owner of a `usage: runtime` entry, every declared element of an owner array, and the owner of every `usage: runtime` `targets[]` element, when declared, is a library of kind `ENGINE`, `MODULE`, `BACKEND` or `PLAYER` |
| K2 | (end of configure) the owner of a `usage: tools` entry or `targets[]` element, when declared, is a library, a tool (`ZENITH_DECL_<Name>_TOOL_KIND` set) or, for an entry owner of the benchmark-target shape, a benchmark (`ZENITH_DECL_<Owner>_BENCHMARK` set); a benchmark-target-shaped owner declared as anything else is refused |
| K3 | (end of configure) every declared element of an owner array is a `KIND BACKEND` library and all declared elements have the same `BACKEND_OF` ([Z2-N-ARCH-200]) |
| K4 | (end of configure) every target-name owner of the manifest (entry `owner`, owner-array element, `targets[].owner`) that no `zenith_add_library`, `zenith_add_tool` or `zenith_add_benchmark` call declares is reported, never refused: one `STATUS` line `Zenith2: <name>: owner <owner> is not declared yet; its targets are linked by nothing` per (entry, owner), and one record in `third_party_pending_owners.json`. It is refused, naming the declared spelling, only when it equals a declared name compared case-insensitively (`Zenith2: ThirdParty/manifest.json: dependencies[<i>] (<name>).owner: K4: owner <owner> is not declared; <Declared> is (names are case-sensitive)`), so a case slip fails in every preset while an owner whose ticket has not landed keeps every configure green |
| K5 | (end of configure) the owner of a `usage: tests` entry, when a declared target name, is a library, a tool or a benchmark, and an entry whose `targets[]` mixes usages has each declared element's owner checked under its own usage |

Rule V9 is the manifest-names rule of the orchestrator's 2026-10-03 ruling; it takes the slot earlier drafts left unused, so the V sequence has no gap.

The consumer-specific pins of V32:

| Entry | Required pins |
|---|---|
| `luau` | `commit`, `releaseTag` (strings), `apiLinkage` (`cxx` or `c`), `apiLinkageSymbols` (array of strings, sorted), `nativePolicy` (object with exactly `android` and `windows`, each `disabled`, `toolsOnly` or `all`), `bytecodeTarget`, `bytecodeMin`, `bytecodeMax` (integers) |
| `d3d12-agility-sdk` | `sdkVersion` (integer), `shippingRuntime` (array of paths, each also in `prebuilt[0].runtime`) |
| `vulkan-profiles` | `jsonSha256` (64 lower-case hex) |
| `lavapipe` | `icdManifest` (object with exactly `windows` and `linux`, each a path) |
| `vulkan-validation-layers` | `layerManifest` (object with exactly `windows` and `linux`, each a path) |
| `sdl3` | `androidJavaDir` (path) |
| `nanobench`, `entt` | `includeDir` (path); `doctest` carries none (the tree root, § "Recorded pins") |

### Validation error messages

Every failure is collected (the validator does not stop at the first) and reported in manifest order, one line each, in the one format

```text
Zenith2: ThirdParty/manifest.json: <path>: <rule>: <what>
```

where `<path>` is a JSON pointer-like path (`dependencies[12] (luau).pins.bytecodeMin`, `toolchain.msvc.clVersion`), `<rule>` the rule ID (`V20`, `K3`) and `<what>` a fixed text from the rule (`expected 64 lower-case hexadecimal digits, found "ABC"`). The configure prints every V line with `message(SEVERE_ERROR …)` in `zenith_begin_build` and then stops with `FATAL_ERROR "Zenith2: ThirdParty/manifest.json: <n> error(s)"`; it prints every K line the same way in `zenith_end_build` and stops with the same summary. The script mode prints the V lines on standard error and exits 1. A clean manifest prints nothing at configure; script mode prints only its last line.

## Consumer entries

The entries other areas asked for, with the exact field values their designs read. Digests and versions are filled by the human pinning ticket; everything else here is binding.

### Luau entry

Asked by SCRIPT (hand-off SCRIPT-P1, cross-area row SB15; `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility", [Z2-N-SCRIPT-032], [Z2-N-SCRIPT-501]).

```json
{
	"name": "luau",
	"version": "0.6xx",
	"source": { "url": "https://github.com/luau-lang/luau/archive/<commit>.tar.gz", "sha256": "<64 hex digits>", "stripPrefix": "luau-<commit>", "format": "tar.gz" },
	"licence": "MIT",
	"integration": "cmake-subproject",
	"owner": "ZenithScript",
	"platforms": ["android", "linux", "windows"],
	"language": "cpp",
	"options": {
		"common": { "LUAU_BUILD_CLI": "OFF", "LUAU_BUILD_TESTS": "OFF", "LUAU_BUILD_WEB": "OFF", "LUAU_STATIC_CRT": "OFF", "LUAU_EXTERN_C": "OFF" },
		"windows": { "LUAU_BUILD_CLI": "ON" }
	},
	"definitions": { "common": { "LUA_USE_LONGJMP": "0" } },
	"targets": [ "… the elements of § The targets array: Luau.VM, Luau.CodeGen, Luau.Ast, Luau.Compiler, Luau.Config, Luau.Analysis, each further library target Luau.Analysis links, and Luau.Analyze.CLI with platforms ["windows"] …" ],
	"licenceFile": "LICENSE.txt",
	"licenceSha256": "<64 hex digits>",
	"patches": [],
	"usage": "runtime",
	"pins": {
		"commit": "<40 lower-case hex digits>",
		"releaseTag": "0.6xx",
		"apiLinkage": "cxx",
		"apiLinkageSymbols": ["lua_pcall", "lua_error", "luau_compile"],
		"nativePolicy": { "android": "toolsOnly", "windows": "toolsOnly" },
		"bytecodeTarget": 6,
		"bytecodeMin": 3,
		"bytecodeMax": 6
	}
}
```

| Pin | Read by | Rule |
|---|---|---|
| `commit` | the `.zlbc` Luau pin hash (first 8 bytes of the commit, [Z2-N-SCRIPT-501]) through `@ZENITH_TP_LUAU_COMMIT@` and `@ZENITH_TP_LUAU_COMMIT_FIRST8_LE@`; `Zenith_Script_PinQualified_Check` | 40 lower-case hex; equals the commit in `source.url` and `stripPrefix` |
| `releaseTag` | `Zenith_Script_LuauPinRecord_Check` | equals `version` |
| `apiLinkage`, `apiLinkageSymbols` | `Test_Script_Firewall_LuauApiLinkageRecorded` ([Z2-N-SCRIPT-033]) | `cxx` or `c`; the inspected symbol names, sorted |
| `nativePolicy` | the configuration key `script.nativePolicy`'s per-platform default ([Z2-N-SCRIPT-309]) | an object with exactly `android` and `windows`, each `disabled`, `toolsOnly` or `all`; `toolsOnly` until the D-SCRIPT-TXN refinement (open, Stage 1) rules the native parity test |
| `bytecodeTarget`, `bytecodeMin`, `bytecodeMax` | `uSCRIPT_LUAU_BYTECODE_MIN`/`_MAX` in SCRIPT's `Zenith_ScriptLuauPin.generated.h.in` (`LBC_VERSION_TARGET`, `LBC_VERSION_MIN`, `LBC_VERSION_MAX` of the pinned revision) | integers with `bytecodeMin ≤ bytecodeTarget ≤ bytecodeMax`; a test (`Zenith_Script_LuauPinRecord_Check`, SCRIPT's) compares them with the pinned `Bytecode.h` |

`LUAU_BUILD_CLI` is `ON` on Windows alone, because only the `win` tree builds the host executable `luau-analyze`; the Android and Linux trees create none of the CLI targets, which is why `Luau.Analyze.CLI` carries `"platforms": ["windows"]` and no other preset requires it to exist. The pins and their relations are validation rules V32 and V38. `LUAU_EXTERN_C` stays `OFF` until `apiLinkage` is measured; a measured `c` changes it to `ON` in the same diff that records the pin.

### D3D12 Agility SDK entry

Asked by RHI (hand-off RHI-P2; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` constant `uZENITH_D3D12_AGILITY_SDK_VERSION`).

| Field | Value |
|---|---|
| `name` | `d3d12-agility-sdk` |
| `integration`, `language` | `prebuilt`, `binary` |
| `owner`, `usage`, `platforms` | `ZenithRHI_D3D12`, `runtime`, `["windows"]` |
| `prebuilt[0]` | `abi: win-x64`, `format: nupkg`, `include: ["build/native/include"]`, `link: []`, `runtime: ["build/native/bin/x64/D3D12Core.dll", "build/native/bin/x64/d3d12SDKLayers.dll"]`, `runtimeSubdir: "D3D12"` |
| `sanitizerExempt`, `decision` | `true` and the decision ID the RHI pinning ticket records ([Z2-N-ARCH-127]) |
| `licence` | `LicenseRef-Microsoft-Agility-SDK` |
| `pins.sdkVersion` | the SDK's `D3D12SDKVersion` integer (for example `615`), read by RHI's template as `@ZENITH_TP_D3D12_AGILITY_SDK_SDK_VERSION@` |

`d3d12SDKLayers.dll` is a development-time debug layer: the Shipping package stages only `D3D12Core.dll` (`Zenith2/Docs/Design/SHIP/Release.md` → "Windows package"), recorded as `pins.shippingRuntime: ["build/native/bin/x64/D3D12Core.dll"]`.

### Selection-candidate entries

Asked by CORE (hand-off CORE-P8, pinned by CORE's `needs-human` ticket Z2T-CORE-053: `mimalloc`, `xxhash`, `yyjson`, `simdjson`) and LIGHT (hand-off LIGHT-P2: the D-SEL-UPSCALER (open, Stage 5) candidates). SHIP accepts Z2T-CORE-053's entries exactly as that ticket writes them: each CORE candidate is owned by its one harness's benchmark target, `usage: tools`, `autoLink` absent, and named by that harness through `zenith_add_benchmark(… THIRD_PARTY <name>)` (Z2T-CORE-054, -056, -058). The LIGHT candidates, whose harness is not yet a declared target, take the token `benchmark` with `usage: tests`:

| Entry | Integration | Owner | Usage | Platforms | Licence | Harness |
|---|---|---|---|---|---|---|
| `mimalloc` | `cmake-subproject` (`MI_BUILD_SHARED OFF`, `MI_BUILD_TESTS OFF`, `MI_OVERRIDE OFF`) | `Zenith_Bench_HeapSelection` | `tools` | `windows`, `android` | MIT | the D-SEL-HEAP (open, Stage 1) harness |
| `xxhash` | `header-only` (the release also provides `xxhsum`, which the D-SEL-HASH ruling uses outside the build) | `Zenith_Bench_HashSelection` | `tools` | all three | BSD-2-Clause | the D-SEL-HASH (open, Stage 1) harness |
| `yyjson` | `zenith-wrapper` (one C source; `Zenith2/ThirdParty/yyjson/CMakeLists.txt` in Z2T-CORE-053's diff) | `Zenith_Bench_JsonSelection` | `tools` | all three | MIT | the D-SEL-JSON (open, Stage 1) harness |
| `simdjson` | `zenith-wrapper` (the single-header amalgamation `simdjson.h`/`simdjson.cpp`; `Zenith2/ThirdParty/simdjson/CMakeLists.txt` in the same diff; the third-party rows compile it with exceptions off, so the amalgamation is built with `SIMDJSON_EXCEPTIONS=0`, the form that compiles under `/EHs-c-` and `-fno-exceptions`, by the admission rule of § "Third-party flag rows" in `Zenith2/Docs/Design/SHIP/CMakeHelpers.md`) | `Zenith_Bench_JsonSelection` | `tools` | all three | Apache-2.0 | the D-SEL-JSON (open, Stage 1) harness |
| `fidelityfx-fsr2`, `fidelityfx-fsr3` | `header-only` (shader and header source compiled offline through ZenithShaderCompiler) | `benchmark` | `tests` | `windows` | MIT | the D-SEL-UPSCALER (open, Stage 5) harness (`Zenith2/Docs/Design/LIGHT/Temporal.md` → "Upscaler selection") |
| `monocypher` | `zenith-wrapper` over `src/monocypher.c` and `src/optional/monocypher-ed25519.c` (one static target `monocypher`; `Zenith2/ThirdParty/monocypher/CMakeLists.txt` in the pinning diff) | `Zenith_Bench_Ed25519` | `tools` | all three | `BSD-2-Clause OR CC0-1.0`, `licenceElected: "BSD-2-Clause"` (V39) | the Ed25519 selection harness (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Selection harness"; row SEC15), Z2T-SHIP-118 |
| `libsodium` | `zenith-wrapper` over the `crypto_sign/ed25519` and `crypto_hash/sha512` sources and the sources they reach (one static target `libsodium`, `SODIUM_STATIC` defined; `Zenith2/ThirdParty/libsodium/CMakeLists.txt` in the pinning diff) | `Zenith_Bench_Ed25519` | `tools` | all three | `ISC`, admitted by the licence ruling Z2T-SHIP-117 | as above, Z2T-SHIP-118 |
| `opus` | `cmake-subproject`, the upstream target `opus` | `Zenith_Bench_AudioCodec` | `tools` | all three | BSD-3-Clause | the D-SEL-CODEC (open, Stage 2) harness (Z2T-AUDIO-004; hand-off AUDIO-P1, row PG2), Z2T-SHIP-119 |
| `ogg` | `cmake-subproject`, the upstream target `ogg` | `Zenith_Bench_AudioCodec` | `tools` | all three | BSD-3-Clause | as above |
| `vorbis` | `zenith-wrapper`: one static target `vorbis` holding the decoder and the `vorbisenc` encoder sources of `lib/` (no `vorbisfile`), `dependsOn: ["ogg"]`, linking the `ogg` target (`Zenith2/ThirdParty/vorbis/CMakeLists.txt` in the pinning diff) | `Zenith_Bench_AudioCodec` | `tools` | all three | BSD-3-Clause | as above |
| `ffmpeg-libavcodec` | `zenith-wrapper`: one static target `ffmpeg-libavcodec` holding the libavcodec and libavutil sources an LGPL-2.1 configuration selects (§ "The ffmpeg-libavcodec entry") | `Zenith_Bench_VideoDecoderSelection` | `tools` | `android`, `windows` | `LGPL-2.1-or-later`, admitted to `allowedTools` by Z2T-SHIP-117 | the D-SEL-VIDEO (open, Stage 2) harness (Z2T-CINE-010; A5 row CnB2), Z2T-SHIP-120 |

The `tools` usage of the CORE candidates is governed by `allowedTools`, which holds all four terms; the later candidates' terms that `allowedTools` lacks (`ISC`, `LGPL-2.1-or-later`) are admitted by the licence ruling Z2T-SHIP-117 before their pins. Each selection's ruling, or the ticket it names, re-owns the admitted candidate and removes the others in one diff ([Z2-N-ARCH-121]): Z2T-SHIP-061 for the Ed25519 candidates (row SEC15), Z2T-CINE-069 for `ffmpeg-libavcodec` (ruling CINE-R1-RULINGS F10: SHIP pins it, Z2T-CINE-069 retires it or, as Z2T-CINE-004 rewrites it, re-owns it to `ZenithCinematics`). Because the owner is a benchmark target, rule K2 checks it against `ZENITH_DECL_Zenith_Bench_<Name>_BENCHMARK` once the harness is declared and reports it as pending before (§ "Validation rules").

DLSS and XeSS get entries only if their licences allow a pinned, redistributable SDK under `licence_policy.json`; until a human adds them, the LIGHT harness reports them `unavailable`, as LIGHT-P2 states.

### The ffmpeg-libavcodec entry

<!-- elaboration -->
FFmpeg has no CMake build, so the entry is a `zenith-wrapper` whose configuration is recorded, not run at configure:

- `source` is the upstream release tarball `https://ffmpeg.org/releases/ffmpeg-<version>.tar.xz` (`format: "tar.xz"`, `stripPrefix: "ffmpeg-<version>"`), `language: c`, `licenceFile: "COPYING.LGPLv2.1"`, `patches: []`.
- `pins.configureArguments` is an object with exactly the keys `win-x64`, `android-arm64-v8a` and `android-x86_64`, each the argument string the person passed to the pinned tree's `configure` for that ABI. Every string contains `--disable-gpl --disable-nonfree --disable-version3 --disable-programs --disable-doc --disable-network --disable-autodetect --disable-everything --disable-avdevice --disable-avformat --disable-avfilter --disable-swresample --disable-swscale --disable-postproc --enable-static --disable-shared --enable-avcodec --enable-avutil --enable-decoder=h264,hevc --enable-parser=h264,hevc`; the `win-x64` string adds `--toolchain=msvc --enable-d3d11va --enable-hwaccel=h264_d3d11va,hevc_d3d11va --disable-x86asm`, both Android strings add `--target-os=android --enable-cross-compile --enable-jni --enable-mediacodec --enable-decoder=h264_mediacodec,hevc_mediacodec`, and `android-x86_64` adds `--disable-x86asm` (no NASM is pinned).
- The wrapper directory holds, per ABI, the files that `configure` generated: `Zenith2/ThirdParty/ffmpeg-libavcodec/config/<abi>/config.h`, `config_components.h`, `libavutil/avconfig.h`, `libavutil/ffversion.h`, `libavcodec/codec_list.c`, `libavcodec/parser_list.c`, `libavcodec/bsf_list.c`, and `sources.cmake` (the C and, for `android-arm64-v8a`, `.S` sources the generated `ffbuild/config.mak` selects, as one `set(ZENITH_FFMPEG_SOURCES …)`). `CMakeLists.txt` compiles them from `ZENITH_THIRD_PARTY_SOURCE_DIR` with `config/<abi>` first on the include path and `HAVE_AV_CONFIG_H` defined, under the third-party rows.
- The licence evidence is in the generated headers: every `config.h` carries `#define FFMPEG_LICENSE "LGPL version 2.1 or later"`, `#define CONFIG_GPL 0`, `#define CONFIG_NONFREE 0` and `#define CONFIG_VERSION3 0`; `CMake_Ship_ManifestValid_Committed` does not read them, so the pinning ticket's Definition of Done checks them.

### Test and benchmark entries

| Entry | Integration | Owner | Usage | Notes |
|---|---|---|---|---|
| `doctest` | `header-only` | `tests`, `autoLink: true` | `tests` | the Stage 0 entry the scaffold creates (Z2T-MOD-056); no `pins.includeDir`: the include directory is the tree root, which holds `doctest/doctest.h`, so `#include <doctest/doctest.h>` resolves in every test |
| `nanobench` | `header-only` | `benchmark`, `autoLink: true` | `tests` | `pins.includeDir: "src/include"` (the upstream tree's `src/include/nanobench.h`), so a benchmark writes `#include <nanobench.h>` |
| `flip` | `header-only` | `ZenithFlux` | `tests` | NVIDIA LDR-FLIP's C++ header (cross-area row RP11, hand-off RPROD-P4); linked into `ZenithFlux_Tests` through `TEST_THIRD_PARTY flip` |
| `warp` | `prebuilt`, `fromToolchain: "warp"` | `test-runs` | `tests` | `platforms: ["windows"]`; one item `{ "abi": "win-x64", "include": [], "link": [], "runtime": ["build/native/bin/x64/d3d10warp.dll"], "runtimeSubdir": "", "executables": [] }`; `sanitizerExempt` with its decision |
| `lavapipe` | `prebuilt`, `fromToolchain: "lavapipe"` | `test-runs` | `tests` | `platforms: ["linux", "windows"]`; two items, `win-x64` and `linux-x64`, each with empty `include`, `link`, `runtime` and `executables` and `runtimeSubdir: ""`; `pins.icdManifest: { "windows": "share/vulkan/icd.d/lvp_icd.x86_64.json", "linux": "share/vulkan/icd.d/lvp_icd.x86_64.json" }`, read by the shared gate environment for `VK_DRIVER_FILES` ([Z2-N-CONTRACT-434]); `sanitizerExempt` |
| `vulkan-validation-layers` | `prebuilt` | `test-runs` | `tests` | `pins.layerManifest` (the `VK_LAYER_KHRONOS_validation` JSON path); installed by the setup script where the loader finds it ([Z2-N-CONTRACT-434]) |

### Other consumer entries

| Entry | Asked by | Shape |
|---|---|---|
| `sdl3` | PLAT (hand-off PLAT-P5) | the BuildSystem example with the lower-case name (cross-document item X239), `source.format: "tar.gz"`, and `targets: [ { "target": "SDL3-static", "owner": "ZenithPlatform", "usage": "runtime" } ]`; `pins.androidJavaDir: "android-project/app/src/main/java/org/libsdl/app"` (MOD's Gradle generation copies from it) |
| `flecs` | ECS (hand-off ECS-P1) | `cmake-subproject`, `owner: ZenithECS_Flecs`, MIT, `options.common: { "FLECS_STATIC": "ON", "FLECS_SHARED": "OFF", "FLECS_PIC": "OFF", "FLECS_TESTS": "OFF" }`, `targets: [ { "target": "flecs_static", "owner": "ZenithECS_Flecs", "usage": "runtime" } ]`; built from source in every sanitizer preset ([Z2-N-ARCH-201]) |
| `entt` | ECS (hand-off ECS-P1) | `header-only`, `owner: ZenithECS_EnTT`, MIT, `pins.includeDir: "src"` |
| `fastgltf`, `meshoptimizer` | CONTENT (rows CT16 (a), CT28 (b); orchestrator ruling CT28B-DEP-OWNERS), formerly ASSET (hand-off ASSET-P3) and GEOM (hand-off GEOM-P1) | `cmake-subproject`, `owner: ZenithContentCore` (every glTF parse and meshopt decode lives in ContentCore; ContentCore is the one owner, and under ruling MESHOPT-VIA-CONTENTCORE-2 every other tool target that calls `meshopt_*` or fastgltf directly, ZenithCook with GEOM's cook steps among them, lists the entry in its own `THIRD_PARTY` as a consumer, which name-resolution rule 3 admits for a tool target whatever the owner; there is no ContentCore wrapper API, rows ShB17, ShB18), `usage: tools`, `platforms: ["windows"]`; `fastgltf` carries `dependsOn: ["simdjson"]` and `FASTGLTF_DOWNLOAD_SIMDJSON` `OFF`, so it builds against the manifest's one simdjson. `simdjson` keeps the `benchmark` candidate owner of § "Selection-candidate entries" (Z2T-CORE-053) until D-SEL-JSON (open, Stage 1) rules; the ruling's diff (CORE's) then sets `owner: ZenithContentCore`, `usage: tools` and `SIMDJSON_EXCEPTIONS` `OFF`, or keeps CORE's owner if simdjson becomes the runtime JSON library; until then SHIP's design does not name simdjson in ContentCore's `THIRD_PARTY` (it reaches it through fastgltf; Z2T-CONTENT-007 names it, which row ShB16 asks CONTENT to confirm), and Z2T-SHIP-051 adds the `simdjson::simdjson` alias to CORE-053's wrapper file when the pinned fastgltf needs it (Z2T-SHIP-051) |
| `ktx-software` | ASSET (hand-off ASSET-P3), GEOM (hand-off GEOM-P1) | `cmake-subproject`, `owner: ZenithCook`, `usage: tools`; `ktx-software` carries `dependsOn: ["zstd"]` and builds against the manifest's zstd through `@ZENITH_TP_ZSTD_DIR@` in its options, so `ZenithCook` links one zstd copy (COMPRESS's one-copy requirement) |
| `stb-image`, `dr-wav`, `dr-flac`, `dr-mp3`, `stb-vorbis` | CONTENT (rows CT16 (a), CT28 (b)) | `header-only`, `owner: ZenithContentCore`, `usage: tools`, `platforms: ["windows"]`, `language: c`, one entry per library: stb-image 2.30 and stb-vorbis 1.22 from the `nothings/stb` repository, dr-wav 0.13.17, dr-flac 0.12.43 and dr-mp3 0.6.40 from `mackron/dr_libs`, each at the commit whose file carries that version; dual licences carry `licenceElected` (V39) (Z2T-SHIP-110 to -114) |
| `bundletool` | PACK (cross-area row PK7) | `host-tool`, `owner: ZenithCook`, `usage: tools`, `platforms: ["windows"]`, `language: binary`, Apache-2.0; one `any` item whose archive is the upstream release's `bundletool-all-<version>.jar` (`format: "exe"`: a binary that is not extracted; `stripPrefix` `""`) with `executables: ["bundletool-all-<version>.jar"]`; PACK's `Zenith2Package.psm1` runs it with the pinned JDK as `java -jar <ZENITH_TP_BUNDLETOOL_DIR>/bundletool-all-<version>.jar`, so nothing else on the machine provides bundletool |
| `mikktspace` | GEOM (cross-area row GM11) | `zenith-wrapper` (`mikktspace.c`), Zlib, `owner: ZenithCook`, `usage: tools` |
| `renderdoc-app` | RHI (cross-area row R7, X78) | `header-only`, MIT, `owner: ["ZenithRHI_D3D12", "ZenithRHI_Vulkan"]`, `usage: runtime`, `shipping: excluded` (the header-only case of § "Shipping exclusion field") |
| `vulkan-headers`, `volk`, `vulkan-profiles`, `vma`, `d3d12ma` | RHI (hand-off RHI-P2) | `vulkan-headers` and `vulkan-profiles` `header-only` owned by `ZenithRHI_Vulkan`; `volk`, `vma`, `d3d12ma` `zenith-wrapper`; `vulkan-profiles` carries `pins.jsonSha256` (the SHA-256 of `VP_ZENITH2_baseline.json` that `Test_VulkanProfile_GeneratedMatchesJson` compares) |
| `slang` | RPROD (hand-off RPROD-P1), RHI | `host-tool`, `fromToolchain: "slang"`, `owner: ZenithShaderCompiler`, `usage: tools`, `platforms: ["windows"]`; one `win-x64` item with `executables: ["bin/slangc.exe"]`, `include: ["include"]`, `link: ["lib/slang.lib"]`, `runtime: ["bin/slang.dll", "bin/slang-glslang.dll"]`; one Slang entry serves both the shader compiler and the RHI bindless probe |
| `dxc` | RPROD (hand-off RPROD-P1) | `host-tool`, `fromToolchain: "dxc"`, `owner: ZenithShaderCompiler`, `usage: tools`, `platforms: ["windows"]`, one `win-x64` item; DXC is called in process (`DxcCreateInstance`, `IDxcCompiler3`) from `ZenithShaderCompiler`'s `Internal/Zenith_ShaderDxc.cpp`, so the entry carries `include: ["inc"]`, `link: ["lib/x64/dxcompiler.lib"]`, `runtime: ["bin/x64/dxcompiler.dll", "bin/x64/dxil.dll"]` (staged next to `ZenithShaderCompiler.exe`, `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Mode host-tool"), and `executables: ["bin/x64/dxc.exe"]` for developers' diagnostics only; cross-area row SH19 asks RPROD to confirm the in-process call |
| `zlib`, `zstd` | COMPRESS | exactly `Zenith2/Docs/Design/COMPRESS.md` → "Third-party integration of zlib" and → "Third-party integration of zstd" (the zlib wrapper file `Zenith2/ThirdParty/zlib/CMakeLists.txt` is added by the owner's ticket Z2T-COMPRESS-007 after the pin, the state `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_third_party_add_all" carries green); a `zstd` entry that records the upstream dual expression `BSD-3-Clause OR GPL-2.0-only` carries `licenceElected: "BSD-3-Clause"` (V39) |
| `harfbuzz` | UI (A5 row UiB4; `Zenith2/Docs/Design/UI/Text.md` → "Text stack selection") | `zenith-wrapper` over the single-unit build `src/harfbuzz.cc` (one static target `harfbuzz`; `Zenith2/ThirdParty/harfbuzz/CMakeLists.txt` in the pinning diff), `owner: ZenithUI`, `usage: runtime`, `platforms: ["android", "windows"]`, `language: cpp`, `definitions.common: { "hb_calloc_impl": "Zenith_UIHbCalloc", "hb_free_impl": "Zenith_UIHbFree", "hb_malloc_impl": "Zenith_UIHbMalloc", "hb_realloc_impl": "Zenith_UIHbRealloc" }` (HarfBuzz's allocator override: four `extern "C"` functions ZenithUI defines in `Internal/Text/Backends/`, resolved at the final link); the licence term `COPYING` states (SPDX `MIT-Modern-Variant` for HarfBuzz's text, which UI's table lists as MIT), admitted to `allowedRuntime` by Z2T-SHIP-117; the pinned tree in the cache is also the source a person builds `hb-shape` from for Z2T-UI-095, which no Zenith2 target builds |
| `freetype` | UI (A5 row UiB4) | `cmake-subproject`, the upstream target `freetype`, `owner: ZenithUI`, `usage: runtime`, `platforms: ["android", "windows"]`, `language: c`, `options.common: { "BUILD_SHARED_LIBS": "OFF", "FT_DISABLE_BROTLI": "ON", "FT_DISABLE_BZIP2": "ON", "FT_DISABLE_HARFBUZZ": "ON", "FT_DISABLE_PNG": "ON", "FT_DISABLE_ZLIB": "ON" }`; the dual expression `LICENSE.TXT` states (expected `FTL OR GPL-2.0-or-later`) with `licenceElected: "FTL"`, admitted to `allowedRuntime` by Z2T-SHIP-117; ZenithUI's allocator redirect is FreeType's own `FT_New_Library` with an `FT_Memory`, so the entry needs no build option for it |
| `msdfgen` | UI (A5 row UiB4) | `cmake-subproject`, `options.common: { "BUILD_SHARED_LIBS": "OFF", "MSDFGEN_BUILD_STANDALONE": "OFF", "MSDFGEN_CORE_ONLY": "ON", "MSDFGEN_DYNAMIC_RUNTIME": "ON", "MSDFGEN_INSTALL": "OFF", "MSDFGEN_USE_OPENMP": "OFF", "MSDFGEN_USE_SKIA": "OFF", "MSDFGEN_USE_VCPKG": "OFF" }` (`MSDFGEN_DYNAMIC_RUNTIME` keeps the `MultiThreadedDLL` runtime every target uses), `targets: [ { "target": "msdfgen-core", "owner": "ZenithUI", "usage": "runtime" } ]`, MIT, `platforms: ["android", "windows"]`; tool targets that generate MSDF atlases name it as consumers (name-resolution rule 3) |
| `sheenbidi` | UI (A5 row UiB4) | `zenith-wrapper` over the single-unit build `Source/SheenBidi.c` with `definitions.common: { "SB_CONFIG_UNITY": "1" }` (one static target `sheenbidi`, include directory `Headers`; `Zenith2/ThirdParty/sheenbidi/CMakeLists.txt` in the pinning diff), Apache-2.0, `owner: ZenithUI`, `usage: runtime`, `platforms: ["android", "windows"]`, `language: c` |
| `steamworks-sdk` | ONLINE (row ON9, A5 row OnB14) | `prebuilt`, `owner: ZenithOnline_Steam`, `usage: runtime`, `platforms: ["windows"]`, `language: binary`, one `win-x64` item `{ "abi": "win-x64", "manualDownload": "<the SDK zip's file name>", "sha256": "<64 hex>", "stripPrefix": "<its top-level directory>", "format": "zip", "include": ["public/steam"], "link": [], "importLibrary": ["redistributable_bin/win64/steam_api64.lib"], "runtime": ["redistributable_bin/win64/steam_api64.dll"], "runtimeSubdir": "", "executables": [] }` (§ "Manual-download items"); the licence term the licence ruling Z2T-SHIP-129 admits, its text committed under `ThirdParty/licences/` by the pinning ticket Z2T-ONLINE-060 |
| `tracy` | DEVT (rows DV2; `Zenith2/Docs/Design/DEVT/Profiler.md` → "Tracy forwarding") | `cmake-subproject`, `owner: ZenithProfiling`, BSD-3-Clause, `usage: runtime`, `shipping: excluded`, `platforms: ["android", "linux", "windows"]` (ZenithProfiling is `ZENITH_ALL` and builds in `linux-tsan`, so rule 1 resolves `THIRD_PARTY tracy` there), `options.common: { "TRACY_DELAYED_INIT": "ON", "TRACY_ENABLE": "ON", "TRACY_MANUAL_LIFETIME": "ON", "TRACY_NO_BROADCAST": "ON", "TRACY_NO_CODE_TRANSFER": "ON", "TRACY_NO_CRASH_HANDLER": "ON", "TRACY_NO_EXIT": "OFF", "TRACY_NO_FRAME_IMAGE": "ON", "TRACY_NO_SAMPLING": "ON", "TRACY_NO_SYSTEM_TRACING": "ON", "TRACY_ON_DEMAND": "ON", "TRACY_ONLY_LOCALHOST": "ON" }` and no per-platform options (DEVT: the `linux` options are identical), `targets: [ { "target": "TracyClient", "owner": "ZenithProfiling", "usage": "runtime" } ]` |

### Entry names used by consumers

<!-- elaboration -->
Every entry name is lower-case kebab case (V9) and `THIRD_PARTY` matches it exactly. This table is the complete list of manifest names the Zenith2 documents and tickets use, with the owner each entry has; the entries marked "pinning ticket" are added by the human pinning ticket of their owner against this schema (`Zenith2/Docs/Architecture.md` → "Third-party dependencies"). The spellings consumers still owe are listed line by line in § "Consumer rename sweep", which is the input of the manifest-name rename sweep:

| Manifest name | Owner | Consumer spelling today |
|---|---|---|
| `luau` | per target (§ "Luau entry") | owes: `Luau` (§ "Consumer rename sweep") |
| `sdl3` | ZenithPlatform | owes: `SDL3` |
| `entt` | ZenithECS_EnTT | owes: `EnTT` |
| `ktx-software` | ZenithCook | owes: `KTX-Software` (ASSET's lines and one GEOM DoD line; GEOM's `THIRD_PARTY` line already spells `ktx-software`) |
| `tracy` | ZenithProfiling | owes: `Tracy` on one line of `Zenith2/Docs/Design/DEVT.md` (its `THIRD_PARTY` lines spell `tracy`) |
| `rmlui` | ZenithUI's D-SEL-UIRT (open, Stage 1/2) candidate, pinned before the ruling (Z2T-SHIP-062; the ruling Z2T-UI-064 removes it if the custom runtime wins) | owes: `RmlUi` |
| `xxhash` | `Zenith_Bench_HashSelection` until D-SEL-HASH (open, Stage 1) | spelled correctly, nothing owed (`Zenith2/Docs/Design/CORE.md`, Z2T-CORE-053, -056, -057; the capitalised "xxHash" there names the project in prose) |
| `jolt` | ZenithPhysics_Jolt (pinning ticket) | spelled correctly, nothing owed (`Zenith2/Docs/Design/PHYS.md` lines 120, 187; `Zenith2/Docs/Design/PHYS/Backends.md` → "Third-party pins") |
| `physx` | ZenithPhysics_PhysX (pinning ticket) | spelled correctly, nothing owed (`Zenith2/Docs/Design/PHYS.md` lines 121, 189, 228) |
| `imgui` | ZenithEditor (pinning ticket) | spelled correctly, nothing owed (`Zenith2/Docs/Design/EDIT.md` line 108) |
| `miniaudio` | ZenithAudio (pinning ticket) | spelled correctly, nothing owed (`Zenith2/Docs/Requirements/AUDIO.md` line 13, `Zenith2/Docs/Design/AUDIO.md`) |
| `stb-image`, `dr-wav`, `dr-flac`, `dr-mp3`, `stb-vorbis` | ZenithContentCore (Z2T-SHIP-110 to -114) | spelled correctly in CONTENT's documents (row CT28 (b)), nothing owed |
| `mimalloc`, `yyjson`, `simdjson` | their harness benchmarks (§ "Selection-candidate entries") | spelled correctly, nothing owed (Z2T-CORE-053 to -058) |
| `monocypher`, `libsodium` | `Zenith_Bench_Ed25519` until the Ed25519 selection rules (Z2T-SHIP-118; Z2T-SHIP-061 re-owns the admitted one) | spelled correctly in SEC's documents, nothing owed |
| `opus`, `ogg`, `vorbis` | `Zenith_Bench_AudioCodec` until D-SEL-CODEC (open, Stage 2) rules (Z2T-SHIP-119) | spelled correctly in Z2T-AUDIO-004, nothing owed |
| `ffmpeg-libavcodec` | `Zenith_Bench_VideoDecoderSelection` until Z2T-CINE-069 (Z2T-SHIP-120) | spelled correctly in CINE's documents and tickets, nothing owed |
| `harfbuzz`, `freetype`, `msdfgen`, `sheenbidi` | ZenithUI's D-SEL-TEXT (open, Stage 1/2) candidates (Z2T-SHIP-121); Z2T-UI-136 deletes the losers | spelled correctly in UI's tickets, nothing owed |
| `steamworks-sdk` | ZenithOnline_Steam (Z2T-ONLINE-060) | spelled correctly, nothing owed |
| `zlib`, `zstd`, `flecs`, `fastgltf`, `meshoptimizer`, `mikktspace`, `renderdoc-app`, `vulkan-headers`, `volk`, `vulkan-profiles`, `vma`, `d3d12ma`, `d3d12-agility-sdk`, `slang`, `dxc`, `doctest`, `nanobench`, `flip`, `warp`, `lavapipe`, `vulkan-validation-layers`, `fidelityfx-fsr2`, `fidelityfx-fsr3` | as above | spelled correctly, nothing owed |

The candidates of the other open selections (`Zenith2/Docs/Architecture.md` → "Dependencies admitted by open selections") take their upstream project's name in lower-case kebab case in the ticket that pins them (`recast-detour`, Z2T-AI-001; `crashpad`, `box2d`). `icu4x-capi`, UI's B2 candidate of D-SEL-TEXT, is refused as a `prebuilt` runtime entry: rule V22 admits no `link` for `usage: runtime`, [Z2-N-ARCH-201] builds every C and C++ dependency of a runtime target from source in the sanitizer presets, and the manifest has no Rust toolchain key from which `icu_capi` could be built; the candidate needs either a toolchain key and an integration mode for a Cargo build (a core-document change UI would request) or its removal from D-SEL-TEXT (A5 row UiB4).

### Consumer rename sweep

<!-- elaboration -->
The exact spellings other areas' documents and tickets must change so that SHIP's names, label and keywords configure, verified line by line on 2026-10-09 (the same list, with the tokens to replace, is `reviews/SHIP_rename_consumers.md` in the drafts tree). SHIP edits none of them; the orchestrator's manifest-name rename sweep and the core-document batch apply them. Each row is one replacement:

| File (lines) | Old | New | Carried by |
|---|---|---|---|
| `Zenith2/Docs/Design/SCRIPT.md` (148, 193) | `THIRD_PARTY Luau` | `THIRD_PARTY luau` | SH17 |
| `Zenith2/Docs/Requirements/SCRIPT.md` (15, 26) | `THIRD_PARTY Luau` | `THIRD_PARTY luau` | SH17 |
| Z2T-SCRIPT-002 (48), Z2T-SCRIPT-003 (34), Z2T-SCRIPT-004 (31, 36), Z2T-SCRIPT-006 (30, 35) | `THIRD_PARTY Luau` | `THIRD_PARTY luau` | SH17 |
| `Zenith2/Docs/Design/PLAT.md` (148), `Zenith2/Docs/Requirements/PLAT.md` (13), Z2T-PLAT-005 (32, 61) | `THIRD_PARTY SDL3` | `THIRD_PARTY sdl3` | SH17 |
| `Zenith2/Docs/Design/ECS.md` (199), Z2T-ECS-005 (38), Z2T-ECS-078 (40), Z2T-ECS-089 (37) | `THIRD_PARTY … EnTT` | `THIRD_PARTY … entt` | SH17 |
| `Zenith2/Docs/Design/ASSET.md` (245), Z2T-ASSET-011 (30) | `THIRD_PARTY … KTX-Software` | `THIRD_PARTY … ktx-software` | SH17 |
| Z2T-GEOM-039 (56, DoD) | "the `Libgraph` step shows `KTX-Software` linked by ZenithCook" | `ktx-software` (lines 29, 31 and 49 name the project in prose and owe nothing) | SH17 (this row) |
| `Zenith2/Docs/Design/UI/Layout.md` (127) | `THIRD_PARTY RmlUi` | `THIRD_PARTY rmlui` | SH17 |
| `Zenith2/Docs/Design/DEVT.md` (77) | "Third party: `Tracy` (owner `ZenithProfiling` …). The manifest entry's platforms …" | `tracy` | SH21 |
| `Zenith2/Docs/Architecture/BuildSystem.md` (656, the example entry) | `"name": "SDL3"` | `"name": "sdl3"`, plus `source.format` | X239 (1), applied by patch e |
| `Zenith2/Docs/Architecture/BuildSystem.md` (256, [Z2-N-ARCH-176]) | "the manifest entry `Luau`" | "the manifest entry `luau`" | X256 (1), applied by patch e |
| `Zenith2/Docs/Architecture/BuildSystem.md` (126, the `Signature` table's `CONFORMANCE_LABELS` row) | `CONFORMANCE_LABELS backend-parity` | `CONFORMANCE_LABELS z2-parity-backend` | X256 (2), applied by patch e |
| Z2T-RHI-101 (27, 33, 41, 49) | `backend-parity` (the label and `ctest -L backend-parity`) | `z2-parity-backend` | SH18 |
| Z2T-RHI-106 (27, 34) | `backend-parity` (the label and `ctest -L backend-parity`) | `z2-parity-backend` | SH18 (the 2026-10-02 parity-label ruling) |
| Z2T-ENV-111 (40, 45, 57) | the `backend-parity` label | `z2-parity-backend` | SH18 (the 2026-10-02 parity-label ruling) |

Owe nothing, verified on the same date: Z2T-SCRIPT-106 line 36 ("its `configure_file` in the ZenithScript CMake through the manifest reader the MOD helper exposes" is satisfied by the helper's automatic template configure, so the ticket writes no `configure_file` line; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Pin header templates", row SH13); Z2T-RHI-001 line 35 (`SLANG_ALTERNATIVES 2` is a valid list that compiles alternative 2, the probe's default; the D-SLANG case ticket that runs alternatives 1 and 3 passes `SLANG_ALTERNATIVES 1 2 3` or omits the argument, row SH18 (b), advisory); the RHI backends' `THIRD_PARTY` lines (`Zenith2/Docs/Design/RHI/Vulkan.md`, `Zenith2/Docs/Design/RHI/D3D12.md`, Z2T-RHI-026, Z2T-RHI-038), `Zenith2/Docs/Design/RPROD/PSO.md` and Z2T-RPROD-032 (`slang dxc`), `Zenith2/Docs/Design/DEVT.md` line 192 and `Zenith2/Docs/Requirements/DEVT.md` line 15 (`tracy`), the CORE selection tickets, COMPRESS's `zlib`/`zstd`, Z2T-ECS-073 (`flecs`), Z2T-GEOM-032 and Z2T-GEOM-039 line 37, `Zenith2/Docs/Design/PHYS.md`, `Zenith2/Docs/Design/EDIT.md`, the AUDIO documents, and Z2T-RPROD-108's title (the phrase "backend-parity device record" is prose, not the label). Z2T-CORE-002's 130 `zenith_core_death_case` registrations, its `zenith_core_compile_fail` callers and Z2T-MOD-056's `Zenith_Fuzz_<Stem>.Smoke` names owe nothing either: SHIP adopts those forms (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Registry functions").

## Dependency cache

### Cache layout

```text
%LOCALAPPDATA%\Zenith2\                      Windows root; $HOME/.zenith2/ in WSL and on CI Linux
	thirdparty\<name>\<dir>\                  one verified dependency tree ([Z2-N-ARCH-204])
	thirdparty\<name>\<dir>.zenith2-stamp.json  its stamp, beside (never inside) the tree
	thirdparty\.partial\                      downloads and extractions in progress
	thirdparty\manual\<file>                  an archive a person placed for a manual-download item (§ "Manual-download items")
	toolchain\<key>\<version>\                every installed toolchain pin
	toolchain\<key>\<version>.zenith2-stamp.json
	gradle-home\<agp>-<metadata hash 16>\     the offline GRADLE_USER_HOME
	android-sdk\<compileSdk>-<packages hash 16>\
	keys\development.keystore                 the development signing key (§ "Development signing keystore")
	sccache\                                  [Z2-N-ARCH-198]
	locks\setup.lock                          the setup script's machine-wide lock
```

<!-- elaboration -->
- `<dir>` is `<source.sha256>` for an entry with no patches, and `<source.sha256>-p<first 16 hex digits of patchSetSha256>` for a patched entry (§ "Patch application"), so two lanes whose manifests differ only in patches never share a tree. For a `fromToolchain` entry the tree is the toolchain install directory and no `thirdparty\` directory exists.
- For `prebuilt` and `host-tool` entries with their own archives, `<dir>` is `<abi>-<sha256 of that abi's archive>`.
- Toolchain trees are keyed by `<version>` alone, because several tools are found by version-named paths (the toolchain files, `Enter-Zenith2Env.ps1`). A re-pinned archive of the same version therefore resolves to the same directory, and its stamp, which records the old archive's `sourceSha256`, refuses it: setup step 1 exits 1 with the stamp message, and the human deletes the old tree (or runs `-PruneCache`) before the new archive is published.
- The cache is shared by every checkout and lane on the machine and is append-only for the setup script: it never modifies or deletes a published tree; `Setup-Zenith2.ps1 -PruneCache` (human-run) deletes trees that no manifest of any registered checkout references, listing each.

### Digest verification algorithm

For one archive object `A` (an entry's `source`, a `prebuilt` item, or a toolchain archive), `Setup-Zenith2.ps1` performs, holding `locks\setup.lock` (a file opened with exclusive share mode; a second setup waits up to 30 minutes, then exits 1 with `Zenith2 setup: another setup holds <path>`):

1. If a stamp `<D>.zenith2-stamp.json` exists without the directory `D` (an interrupted publication), delete the orphan stamp and continue at step 2. If `D` exists without a stamp (only a hand edit produces this), exit 1 with `Zenith2 setup: <name>: cache directory <D> has no stamp; delete it and re-run`. If the published directory `D` exists: read its stamp; if `stamp.sourceSha256 == A.sha256` and `stamp.patchSetSha256` equals the manifest's, the archive is verified (step 9); otherwise exit 1 with `Zenith2 setup: <name>: cache directory <D> has a stamp for <stamp.sourceSha256>; refusing to overwrite`.
2. Download `A.url` (or copy `<MirrorRoot>/<A.sha256>` when `-MirrorRoot` is given) into `.partial\<name>-<sha256>.download` with `Invoke-WebRequest -UseBasicParsing -MaximumRedirection 5`, TLS 1.2 or newer, no proxy override; HTTP status other than 200 deletes the file and fails (`download failed: <status>`).
3. Compute the SHA-256 of the downloaded bytes with `Get-FileHash -Algorithm SHA256` (streamed, never loaded whole), lower-cased.
4. Compare with `A.sha256`; on mismatch delete the download and exit 1 with `Zenith2 setup: <name> <version>: SHA-256 mismatch: expected <expected>, got <actual>; the download was deleted` ([Z2-N-ARCH-196]).
5. Extract into `.partial\<name>-<sha256>\` (§ "Archive extraction rules").
6. Apply the patches (§ "Patch application").
7. Verify the licence: `licenceFile` exists in the tree and its SHA-256 equals `licenceSha256`, and every `pins.additionalLicenceFiles` element matches its hash; a mismatch deletes the extraction and fails (`licence file <path>: SHA-256 mismatch`).
8. Compute the tree digest (§ "Tree digest"), write the stamp to `.partial\`, then publish **the stamp first and the tree second**: rename the stamp to `<D>.zenith2-stamp.json`, then rename the extraction to `D` (`Move-Item` within one volume; each rename is retried three times at 1 s intervals on a sharing violation, then fails). A configure that runs between the two renames sees a stamp without its directory and stops with the ordinary "missing from the dependency cache" message; nothing ever sees a published directory without its stamp.
9. Delete the download.

`-Check` repeats steps 1, 7 and the tree digest over the published directory without downloading, and reports, for every entry, `ok`, `missing`, `stamp mismatch`, `tree modified` or `licence mismatch`.

The digest algorithm is SHA-256 alone (FIPS 180-4), computed by .NET's implementation in PowerShell and by `file(SHA256)`/`string(SHA256)` in CMake; no MD5 or SHA-1 value is ever trusted, including the SHA-1 checksums `sdkmanager` repository files carry (§ "Android SDK provisioning").

### Archive extraction rules

- Formats on a Windows host: `zip` and `nupkg` (a zip) with `[System.IO.Compression.ZipFile]`; `tar.gz` and `tar.xz` with the pinned `cmake -E tar xf <archive>` (run in the empty target directory, the member list read first with `cmake -E tar tvf`); `exe` is not extracted.
- Formats on a Linux host (WSL or native, § "Linux archive extraction"): `tar.gz` and `tar.xz` with the system `tar` (`tar -tvf` for the member list, then `tar -xf`), `zip` with the system `unzip` (`unzip -Z -l` for the list, then `unzip -q`), both from the snapshot-pinned `wsl.packages`; the pinned `cmake` is not used there because it is one of the archives being installed.
- Before writing any member, every member name is checked: no absolute path, no drive letter, no `..` component, no backslash in a tar member, no hard link or device member; any such member fails the extraction (`archive member <name> rejected: <reason>`), which protects the cache against path traversal in a hostile archive.
- A symbolic-link member is refused on a Windows host. On a Linux host it is accepted only when its target is a relative path that, resolved against the link's own directory, stays inside the extraction root (`clang++` → `clang-<major>` in the LLVM release archive is the case this admits); an absolute target or one that leaves the root fails with `archive member <name> rejected: symbolic link target <target> leaves the tree`.
- `stripPrefix`, when non-empty, must be the one top-level directory of the archive; its contents become the tree root. A second top-level entry fails (`archive has entries outside stripPrefix <p>`).
- File times are not preserved meaningfully: every extracted file's modification time is set to 1980-01-01T00:00:00Z, so no build sees a dependency file as newer than its outputs after a re-extraction.

### Tree digest

`treeSha256` is the SHA-256 of the UTF-8 text formed by one line per regular file of the tree, `<file sha256> <relative path>\n`, and one line per symbolic link (Linux trees only), `link <target> <relative path>\n`, where the relative path uses `/`, all lines are ordered by byte order of the relative path, a link is never followed, and directories contribute nothing. Hidden files are included; the stamp, which lives outside the tree, is not. The CMake helper never computes it (too slow for every configure); `-Check` and the acceptance run's `Setup-Zenith2.ps1 -Check` do.

### Patch application

1. `patchSetSha256` is the SHA-256 of the lines `<patch sha256> <patch file>\n` in array order; with no patches it is the SHA-256 of the empty string.
2. Each patch file's SHA-256 is verified against the manifest before use.
3. Each patch is applied with `git apply --unsafe-paths --directory=<extraction root relative to the cwd> -p1 --whitespace=nowarn <file>` from the parent of the extraction; a failing apply deletes the extraction and fails (`patch <file> does not apply to <name> <version>`).
4. Git is a prerequisite (version 2.40 or newer), checked by `-Check`; it is not a manifest pin because it never contributes bytes to a build output (§ "Git prerequisites", which holds `git-lfs` on the same terms).

### Stamp check at configure

`CMake/ZenithThirdParty.cmake` resolves an entry's directory from the manifest (§ "Cache layout"), then reads `<dir>.zenith2-stamp.json` and requires `sourceSha256` and `patchSetSha256` to equal the manifest's values, stopping with `FATAL_ERROR "Zenith2: <name> <version>: dependency cache stamp does not match ThirdParty/manifest.json; run Tools/Setup/Setup-Zenith2.ps1"` otherwise. An absent directory keeps the scaffold's message ([Z2-N-ARCH-204]); a directory present without its stamp (a hand edit, never a setup run, § "Digest verification algorithm" step 8) stops with `FATAL_ERROR "Zenith2: <name> <version>: dependency cache entry <dir> has no stamp; delete it and run Tools/Setup/Setup-Zenith2.ps1"`. The stamp's format:

```json
{
	"schema": "zenith2.cache-stamp",
	"schemaVersion": 1,
	"name": "zstd",
	"version": "1.5.7",
	"sourceSha256": "<64 hex>",
	"patchSetSha256": "<64 hex>",
	"treeSha256": "<64 hex>",
	"licenceSha256": "<64 hex>",
	"setupScriptVersion": 1,
	"createdUtc": "2026-10-02T12:00:00Z"
}
```

## Setup script

### Setup-Zenith2 parameters

```text
pwsh -NoProfile -File Zenith2/Tools/Setup/Setup-Zenith2.ps1
	[-Check]                         verify only; changes nothing; exit 0 when everything matches, 1 otherwise
	[-Only <key-or-name>[,…]]        restrict to listed toolchain keys and dependency names
	[-AcceptToolchainLicenses]       required to install MSVC Build Tools, the JDK, the NDK and lavapipe/WARP
	[-AcceptAndroidSdkLicenses]      required to install the Android SDK packages
	[-SkipWsl]                       Windows hosts only: do not provision WSL (CI Windows runners, machines without WSL)
	[-PruneCache]                    delete unreferenced cache trees (§ "Cache layout"); lists each
	[-Report <path>]                 also write the JSON report (default ${ZENITH_OUT}/setup/setup-report.json)
	[-MirrorRoot <dir>]              read every archive from <dir>/<sha256> instead of its url (offline mirrors and the tests); the digest check is unchanged
```

Exit codes: 0 success (or everything matches in `-Check`); 1 a mismatch, a failed download or a failed verification; 2 a usage error (including `-SkipWsl` on a Linux host); 3 a missing prerequisite (Windows 11 or Ubuntu 24.04, `git`, `git-lfs` on a Windows host (§ "Git prerequisites"), administrator rights for the MSVC step, the WSL feature for the WSL step, `sudo` for the Linux host's package step); 4 a licence-acceptance switch required but not given (the script lists which).

The script selects its mode from the host: `$IsWindows` runs the Windows host steps below; `$IsLinux` runs § "Linux host mode"; any other host exits 3.

### Git prerequisites

<!-- elaboration -->
`git` and `git-lfs` are **checked prerequisites**, never pins of the `toolchain` section: neither contributes a byte to a build output, and both are installed by the human with the host's own Git (Git for Windows ships git-lfs), so the setup verifies a documented minimum version instead of downloading a pinned archive (orchestrator ruling GIT-LFS-PREREQ of 2026-10-16; the BuildSystem wording is cross-document item X372).

| Prerequisite | Minimum | Checked by | Why |
|---|---|---|---|
| `git` | 2.40 | step 1 and `-Check` (`git --version`) | patch application (§ "Patch application"), the checkout hash, the gate steps |
| `git-lfs` | 3.4.0 | step 8a and `-Check` (`git lfs version`), Windows hosts | the LFS objects of tracked binaries under the Git LFS storage policy of `Zenith2/Docs/Budgets.md` → "Package and storage budgets" (CONTENT's recorded fixtures, cross-area row CT32) |

The Linux host mode does not check `git-lfs`: no CONTENT test runs in `linux-tsan`. A test that reads an LFS pointer where it expects an object fails with a message naming `Setup-Zenith2.ps1 -Only git-lfs` (CONTENT's tests, row CT32 (b)).

### Setup steps

On a Windows host, in this order, each skipped when `-Only` excludes it and each idempotent:

1. Validate prerequisites; acquire `locks\setup.lock`.
2. Read the manifest with `Test-Zenith2ManifestText` (BOM, duplicate keys, size), then install the `cmake` pin (the one pin read before validation, through `ConvertFrom-Json`), then run the one validator in script mode (§ "Validation rules"); a validation failure exits 1 before anything else is installed. While the validator does not exist yet — `Zenith2/CMake/ZenithThirdParty.cmake` is absent (the deliverable-14 tree, which lands before the scaffold) or holds no `function(zenith_manifest_validate` line (the scaffold's Stage 0 reader, until SHIP's Stage 1 ticket T1) — step 2 runs `Test-Zenith2ManifestBootstrap` instead of the validator — the top-level keys (V1, V2), the toolchain keys (V3) and the archive objects (V4), the checks the setup needs to install anything — and prints `Zenith2 setup: manifest validator not present (CMake/ZenithThirdParty.cmake); bootstrap checks only` (the toolchain `licence` values are also checked to be present strings, V40, because deliverable 14 records them); a bootstrap failure exits 1, a pass continues. From the first configure that has the validator on, every V rule applies (§ "Stage 0 bootstrapping order" of `Zenith2/Docs/Design/SHIP.md`).
3. Install the toolchain pins: `ninja`, `sccache`, `llvm`, `python`, `pwsh`, `jdk`, `ndk`, `slang`, `dxc`, `warp`, `lavapipe` by the digest verification algorithm into `toolchain\<key>\<version>\`; `msvc` by running the verified bootstrapper `--installPath %LOCALAPPDATA%\Zenith2\toolchain\msvc\<vsVersion> --quiet --norestart --nocache --channelUri <channelUri> --add <component>…` (elevated), then checking `cl.exe` reports `clVersion`.
4. Install the Android SDK (§ "Android SDK provisioning") and the offline Gradle home (§ "Offline Gradle home").
5. Provision WSL (§ "WSL provisioning") unless `-SkipWsl`.
6. Verify and publish every dependency entry for every platform the host's presets build (Windows hosts: entries with `windows` or `android` in `platforms`; the WSL step: entries with `linux`); a manual-download item is read from `thirdparty\manual\` only, and its absence is reported, not failed (§ "Manual-download items").
7. Install the Vulkan validation layer where the loader finds it (`HKCU\SOFTWARE\Khronos\Vulkan\ExplicitLayers` value pointing at the pinned layer JSON on Windows, `$HOME/.local/share/vulkan/explicit_layer.d/` in WSL) ([Z2-N-CONTRACT-434]).
8. Create the development keystore (§ "Development signing keystore").
8a. `git-lfs` (Z2-R-SHIP-154): `git lfs version` at least the minimum of § "Git prerequisites" (else exit 3 naming `-Only git-lfs`), then `git lfs install --local` and `git lfs pull` in the checkout; with `-MirrorRoot` the pull is skipped (no network).
8b. `content-merge` (CONTENT row CT16 (b)): when the `win-tools` build holds `<out root>/win/Release/bin/ZenithContent.exe`, run its `merge --install --repo <checkout>` (`Zenith2/Docs/Design/CONTENT/Merge.md` → "Git integration"); otherwise print that the tool is not built yet, report the item `skipped` and change nothing. The setup script provisions the toolchain before any build, so a fresh machine runs `Setup-Zenith2.ps1 -Only content-merge` once more after its first `win-tools` build; the environment variable `ZENITH2_SETUP_CONTENT_TOOL` (a test seam read by this step alone, never set by `Enter-Zenith2Env.ps1`, a gate step or CI) names another tool command.
9. Write the report and print one line per item: `ok <key> <version>`, `installed <key> <version>`, or the failure.

The script writes only under `%LOCALAPPDATA%\Zenith2\`, `$HOME/.zenith2/` in WSL, the one registry value of step 7 and `${ZENITH_OUT}/setup/` (the report); it never writes inside the checkout outside `out/`, with one named exception: steps 8a and 8b write git's own state of the clone (the local `.git/config` keys of `git lfs install --local` and of the merge driver, the LFS hooks and the LFS object store under `.git/`) and the working-tree files of LFS-tracked paths, which `git lfs pull` turns from pointers into their committed objects, leaving `git status` clean.

### Check mode

`-Check` performs steps 1–2 and, for every pin and entry of the host's mode, the verification half of its install step, and also checks: `git --version` ≥ 2.40; the WSL distro's (or, on a Linux host, the machine's) `vm.mmap_rnd_bits` ≤ 28 ([Z2-N-ARCH-219]); that the pinned `pwsh` tree exists and verifies; and that `Enter-Zenith2Env.ps1` would compute the same checkout hash as `Get-Zenith2CheckoutHash` ([Z2-N-ARCH-226]). It does not require the interpreter running it to be the pinned `pwsh`: CI and a fresh machine start it with the host's own `pwsh` before `Enter-Zenith2Env.ps1` has put the pinned one first on `PATH`, and `Enter-Zenith2Env.ps1` step 5 is the check that every subsequent command resolves the pinned interpreter. Its report lists every mismatch; the CI jobs and the stage acceptance run it first (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Job steps").

| Host | Keys and entries `-Check` verifies | Keys it never examines |
|---|---|---|
| Windows | every toolchain key; every entry whose `platforms` contains `windows` or `android`; the items `git-lfs` (`ok`, `missing`, `old`, `not installed locally`, `pointers present`) and `content-merge` (`ok`, `skipped` while `ZenithContent` is not built, `missing`); with WSL provisioned (no `-SkipWsl`), the `wsl` key, the Linux archives inside the distro and the entries with `linux` | none |
| Linux (§ "Linux host mode") | `cmake`, `ninja`, `sccache`, `llvm`, `pwsh`, `lavapipe` (their `linux` archives), `python` and `wsl.packages` (the installed package versions, `dpkg-query -W -f '${Version}' <package>`), every entry whose `platforms` contains `linux` | `msvc`, `slang`, `dxc`, `warp`, `ndk`, `jdk`, `gradle`, `agp`, `androidSdk`, `wsl.rootfs`, `vulkanProfilesGenerator`, and every entry without `linux` |

### Linux host mode

<!-- elaboration -->
The native Linux CI runner of the `linux` job ([Z2-N-ARCH-221]; `Zenith2/Docs/Design/SHIP/Qualification.md` → "Jobs") is an Ubuntu 24.04 machine, provisioned once by a human running `pwsh -NoProfile -File Zenith2/Tools/Setup/Setup-Zenith2.ps1` on it as an ordinary user with `sudo` (the host's own `pwsh` 7.4 or newer bootstraps; the pinned one is installed in step 4). In this mode the script performs, each step idempotent and skipped when `-Only` excludes it, every root step through `sudo` and every other step as the invoking user:

1. Validate prerequisites (Ubuntu 24.04, `git` ≥ 2.40, `sudo -n true` succeeds after the human's one `sudo -v`); acquire `$HOME/.zenith2/locks/setup.lock`.
2. `Test-Zenith2ManifestText` and `Test-Zenith2ManifestBootstrap` (V1 to V4 and V40, the checks the steps up to the `cmake` install need; no `cmake` is on a fresh host yet); a failure exits 1.
3. **Root:** point apt at the snapshot and install `wsl.packages` (§ "Linux package snapshot").
4. **User:** for each of `pwsh`, `cmake`, `ninja`, `sccache`, `llvm` and `lavapipe`, in that order: download its `linux` archive and verify it (steps 1 to 4 of § "Digest verification algorithm"), extract it with `Expand-Zenith2LinuxArchive` (§ "Linux archive extraction") into `$HOME/.zenith2/.partial/<key>-<sha256>/`, compute the tree digest and publish the stamp and the tree into `$HOME/.zenith2/toolchain/<key>/<version>/` (steps 7 to 9), the paths the toolchain file `clang-linux-x64.cmake` and `Enter-Zenith2Env.ps1` read. Once `cmake` is published, run the one validator in script mode with it (as Windows step 2 does); a validation failure exits 1 before any further step.
5. **Root:** write `/etc/sysctl.d/99-zenith2-tsan.conf` with the single line `vm.mmap_rnd_bits = 28` and run `sysctl --system` ([Z2-N-ARCH-219]).
6. **User:** publish the dependency entries whose `platforms` contain `linux` into `$HOME/.zenith2/thirdparty/` by the digest verification algorithm, extraction through `Expand-Zenith2LinuxArchive`.
7. **User:** install the Vulkan validation layer JSON into `$HOME/.local/share/vulkan/explicit_layer.d/`.
8. Write the report as step 9 of § "Setup steps".

Windows host steps 4, 5 and 8 (the Android SDK and Gradle home, WSL, the development keystore) and every Windows-only key do not exist in this mode; `-SkipWsl` is a usage error (exit 2). The WSL step of a Windows host (§ "WSL provisioning") runs the same steps 3 to 7 in the distro, so the two Linux environments are provisioned by one code path, `Install-Zenith2LinuxHost` in `Zenith2Env.psm1`, whose command list is `Get-Zenith2LinuxCommandPlan` (§ "Zenith2Env module").

### Linux package snapshot

<!-- elaboration -->
Root step 3 makes apt resolve every package from one dated snapshot of the Ubuntu archive, so the native host and every WSL distro install byte-identical system files:

1. Write `/etc/apt/sources.list.d/zenith2-snapshot.sources` (mode 0644) with exactly this deb822 text, `<snapshot>` being `wsl.snapshot`:

   ```text
   Types: deb
   URIs: https://snapshot.ubuntu.com/ubuntu/<snapshot>/
   Suites: noble noble-updates noble-security
   Components: main universe
   Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
   ```

2. Rename the distribution's own `/etc/apt/sources.list.d/ubuntu.sources` to `ubuntu.sources.zenith2-disabled` (apt reads only `*.sources` and `*.list` files there), so no package resolves from the live archive.
3. `apt-get -o Acquire::Check-Valid-Until=false update` (a snapshot's `Release` files are past their validity date by design), then `apt-get install -y --no-install-recommends <package>=<version>…` with every `wsl.packages` element.
4. Verify with `dpkg-query -W -f '${Version}' <package>` that every package reports exactly its pinned version; a difference fails the step (`Zenith2 setup: <package>: installed <found>, pinned <version>`), exit 1.

The snapshot service publishes the archive as of the timestamp in the URL path (`https://snapshot.ubuntu.com/ubuntu/<YYYYMMDDTHHMMSSZ>/`); the archive key is the distribution's own keyring, so no key is downloaded.

### Linux archive extraction

<!-- elaboration -->
`Expand-Zenith2LinuxArchive` extracts a verified archive on a Linux host, natively or inside the distro, without the pinned `cmake` (which may be the archive being installed):

1. List the members with `tar -tvf <archive>` (`tar.gz`, `tar.xz`) or `unzip -Z -l <archive>` (`zip`); parse each line's type character (`-` file, `d` directory, `l` symbolic link with its `->` target, `h` hard link, `b`, `c`, `p` device or pipe) and name, and apply the member rules of § "Archive extraction rules" (Linux form: relative in-tree symbolic links accepted).
2. Create the empty destination and extract with `tar -xf <archive> -C <destination> --no-same-owner` or `unzip -q <archive> -d <destination>`.
3. Apply `stripPrefix` by moving the single top-level directory's contents up one level (a second top-level entry fails as on Windows), then set every file's modification time to 1980-01-01T00:00:00Z with `touch -h -d 1980-01-01T00:00:00Z`.

Inside a WSL distro the function runs `wsl.exe -d <distro> -u zenith -- tar …` with the archive read from its Windows location through `/mnt/<drive letter lower-cased>/<path with forward slashes>` (the distro's default `metadata` automount, cross-area row DV16 (a)), so the archive is downloaded and verified once, on the Windows side, by the same digest code as every other archive.

### Licence acceptance switches

Installing the MSVC Build Tools, the JDK, the NDK, the Android SDK packages, WARP and lavapipe accepts their licence terms. The setup script never accepts terms on the user's behalf: each of those steps requires the switch the table names, typed by the human who runs the script, and without it the step stops with exit 4 and prints the licence's URL. `-Check` needs no switch. CI runners are provisioned by a human once with the switches; the CI jobs run `-Check` only.

| Step | Switch | Terms |
|---|---|---|
| `msvc`, `jdk`, `ndk`, `warp`, `lavapipe` | `-AcceptToolchainLicenses` | the Visual Studio Build Tools licence, the OpenJDK distribution's terms, the Android NDK terms, the WARP NuGet licence, the Mesa licence |
| `androidSdk` | `-AcceptAndroidSdkLicenses` | the Android SDK licence agreement (written into `<sdk>/licenses/` as `sdkmanager --licenses` would, only after the switch) |

### Enter-Zenith2Env

`Zenith2/Tools/Setup/Enter-Zenith2Env.ps1` prepares the current PowerShell session for a configure, a build or a gate step (it is dot-sourced: `. Zenith2/Tools/Setup/Enter-Zenith2Env.ps1`). It:

1. computes `ZENITH_CHECKOUT_HASH` with `Get-Zenith2CheckoutHash` ([Z2-N-ARCH-158], [Z2-N-ARCH-226]);
2. on Windows, imports the pinned MSVC environment with `Import-Zenith2MsvcEnvironment` (the same `VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=<toolsetVersion> -winsdk=<windowsSdk>` capture as the gate module's `Get-Zenith2MsvcEnvironment`, [Z2-N-CONTRACT-428]);
3. prepends to `PATH` the pinned `cmake`, `ninja`, `sccache`, `llvm` and `pwsh` `bin` directories, in that order;
4. sets (Windows) `ANDROID_NDK_HOME`, `JAVA_HOME` (for the Gradle step) and `ZENITH2_DEV_KEYSTORE`, and (every host) `SCCACHE_DIR` (§ "Cache layout") and `SCCACHE_CACHE_SIZE=60G` ([Z2-N-ARCH-232]) and `ZENITH2_TOOLCHAIN_ROOT`;
5. fails (exit 1, the session unchanged) when `Setup-Zenith2.ps1 -Check -Only cmake,ninja,sccache,llvm,pwsh,msvc` would fail (`msvc` omitted on Linux), printing the mismatch, or when `(Get-Command pwsh).Source` after step 3 is not the pinned `pwsh` executable, so every command the session runs after it (the gate steps of the CI job, a configure) uses the pinned interpreter.

It changes no file. A second call in the same session is a no-op for variables already equal.

### Zenith2Env module

`Zenith2/Tools/Setup/Zenith2Env.psm1` exports exactly these functions; every SHIP script imports it and no SHIP script re-implements one of them:

| Function | Returns |
|---|---|
| `Get-Zenith2CheckoutHash [-Root <path>]` | the 16-hex checkout hash of [Z2-N-ARCH-159] |
| `Get-Zenith2OutRoot [-Root <path>]` | `Zenith2/out/<hash>` |
| `Get-Zenith2Manifest [-Root <path>]` | the parsed manifest (ordered hashtables), after `Test-Zenith2ManifestText` |
| `Test-Zenith2ManifestText -Path <path>` | throws on a BOM, a duplicate key or a file above 1 MiB, naming the key path |
| `Get-Zenith2CanonicalJson -Value <object>` | the canonical form of § "Canonicalisation and hashing" |
| `Get-Zenith2ManifestHashes [-Root <path>] [-Path <manifest file>]` | `{ toolchainHash, dependenciesHash, manifestSha256 }` of `<Root>/Zenith2/ThirdParty/manifest.json`, or of the file `-Path` names (the fingerprint fixtures under `Zenith2/CMake/Tests/Ship/FingerprintInputs/`); giving both is a usage error |
| `Get-Zenith2CacheRoot` | `%LOCALAPPDATA%\Zenith2` or `$HOME/.zenith2` |
| `Get-Zenith2DependencyDirectory -Name <name> [-Abi <abi>]` | the verified tree of an entry, or throws `not in the dependency cache` |
| `Get-Zenith2ToolchainDirectory -Key <key>` | the install directory of a toolchain pin |
| `Get-Zenith2ToolPath -Tool <llvm-symbolizer\|llvm-readelf\|llvm-readobj\|llvm-pdbutil\|llvm-strip\|apksigner\|zipalign\|jarsigner\|keytool\|java\|signtool\|cmake\|ninja>` | the absolute path of a pinned tool executable (`java` is the pinned JDK's `bin/java.exe`) |
| `Import-Zenith2MsvcEnvironment` | sets the captured MSVC variables in the session |
| `Get-Zenith2BinaryIdentity -Path <file>` | `{ kind: "pe"\|"elf", buildId: <hex>, pdbName, pdbGuid, pdbAge, timeDateStamp, sizeOfImage }` (`Zenith2/Docs/Design/SHIP/Release.md` → "Symbol identities") |
| `Write-Zenith2Json -Path <path> -Value <object>` | writes JSON with keys in insertion order, tab indentation, LF, UTF-8 without BOM, only when the bytes change |
| `Get-Zenith2HostKeys -HostOs <windows\|linux>` | the toolchain keys and the entry platforms `-Check` and the install steps cover on that host (§ "Check mode"), so the selection is one function and is tested directly |
| `Test-Zenith2ManifestBootstrap -Path <path>` | the deliverable-14 bootstrap checks of § "Setup steps" step 2 (V1 to V4 and V40 only), for use while the validator does not exist; throws naming the first violation |
| `Install-Zenith2LinuxHost -Manifest <object> [-Check] [-Wsl <distro>] [-ArchiveRoot <dir>]` | the Linux steps 3 to 7 of § "Linux host mode", natively or, with `-Wsl`, in the distro as § "WSL provisioning" splits them (root steps through `wsl.exe -d <distro> -u root`, user steps through `-u zenith`); returns one report item per key and entry; with `-ArchiveRoot` (the in-distro half of § "WSL provisioning" step 5) it reads already-verified archives from that directory and downloads nothing |
| `Expand-Zenith2LinuxArchive -Archive <path> -Format <tar.gz\|tar.xz\|zip> -StripPrefix <p> -Destination <dir> [-Distro <distro>]` | lists and checks the members (§ "Archive extraction rules"), then extracts with the system `tar` or `unzip`, natively or inside the distro as user `zenith`; throws naming the first rejected member |
| `Get-Zenith2LinuxCommandPlan -Manifest <object> -Mode <Native\|Wsl> [-Distro <distro>]` | the ordered list of `{ user, command }` records `Install-Zenith2LinuxHost` executes, built without running anything, so the root and user split and the apt sources text are tested directly |

### Offline Gradle home

<!-- elaboration -->
Answers MOD's assumption in `Zenith2/Docs/Design/MOD/GameBuild.md` → "The Apk target" (cross-area row M20).

- The Gradle dependency verification file is `Zenith2/ThirdParty/gradle/verification-metadata.xml` (Gradle's own format, `<verify-metadata>true</verify-metadata>`, `<verify-signatures>false</verify-signatures>`, SHA-256 checksums of every artefact AGP and its transitive dependencies resolve for the template's build types). MOD's Gradle generation copies it into the generated project's `gradle/verification-metadata.xml` (cross-area row SH3), so every Gradle build verifies every artefact it resolves and fails on an unlisted or mismatched one.
- `Zenith2/ThirdParty/gradle/seed/` is a minimal Android project (one empty activity, no native code) declaring exactly the plugins and dependencies `Zenith2/Player/GradleTemplate/` declares, with the pinned AGP; it exists only to populate the cache. <!-- elaboration --> It also declares PACK's bundle-project runtime dependency `implementation("com.google.android.play:asset-delivery:<version>")` (cross-area row PK7), so the artefact and its transitive `com.google.android.play:core-common` are in the offline home and in `verification-metadata.xml`. The version is recorded once, as the seed's `gradle.properties` key `zenith.playAssetDeliveryVersion`, which PACK's bundle generation reads; the licence term is the Play Core Software Development Kit Terms of Service, listed in the licence inventory's Gradle artefact rows.
- The setup step: create `gradle-home\<agp>-<first 16 hex of the metadata file's SHA-256>\`; run the seed project's wrapper (the pinned distribution, verified against `gradle.distribution.sha256`) once online with `GRADLE_USER_HOME` set to it and `--no-daemon --dependency-verification strict assembleDebug assembleRelease`; then run it again with `--offline` to prove the cache is complete; write the stamp. `ZENITH_GRADLE_HOME` (exposed by `CMake/ZenithThirdParty.cmake`, `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Exposed variables") is this directory.
- Updating the metadata (an AGP or template plugin change) is `pwsh -NoProfile -File Zenith2/Tools/Setup/Update-Zenith2GradleVerification.ps1`, human-run and online: it runs the seed project with `--write-verification-metadata sha256 help assembleDebug assembleRelease` in a fresh temporary Gradle home and writes the result over `verification-metadata.xml` for the human to review and commit.
- Every Maven artefact the generated Gradle projects resolve into a shipped APK is a row of `Zenith2/ThirdParty/gradle/maven-artefacts.json` (§ "Maven artefact rows"), and the seed project declares every direct row, so the offline Gradle home and the verification metadata hold each one.

### Maven artefact rows

<!-- elaboration -->
Answers ONLINE's row ON9 (b) (`play-services-games-v2`, `billing`) and the licence half of PACK's row PK7 (`asset-delivery`, `core-common`) (A5 rows OnB14, PkB6). A Maven artefact is never a `dependencies` entry of the manifest: Gradle resolves it into the offline Gradle home, and `verification-metadata.xml` holds its SHA-256. What a manifest entry would add, its licence and which games ship it, is the row:

```json
{
	"schema": "zenith2.maven-artefacts",
	"schemaVersion": 1,
	"artefacts": [
		{ "coordinate": "com.android.billingclient:billing:<version>", "versionKey": "zenith.playBillingVersion", "selectedBy": ["ZenithOnline_GooglePlay"], "licence": "<term>", "licenceFile": "ThirdParty/licences/<file>", "licenceSha256": "<64 hex>" }
	]
}
```

| Field | Rule |
|---|---|
| `coordinate` | `<group>:<artefact>:<version>` with an exact version (no `+`, range, `latest.*` or `-SNAPSHOT`); unique; the array sorted by byte order of `coordinate` |
| `versionKey` | for an artefact the seed project declares directly, the `gradle.properties` key of `Zenith2/ThirdParty/gradle/seed/` that holds its version (`zenith.playAssetDeliveryVersion`), whose value equals the coordinate's version; `null` for a transitive artefact |
| `selectedBy` | the non-empty, byte-ordered list of library names whose presence in a game's link closure puts the artefact into that game's generated Gradle project; a transitive artefact carries the union of the lists of the direct artefacts that reach it |
| `licence`, `licenceFile`, `licenceSha256` | the licence fields of § "Licence fields", governed by `allowedRuntime`; the term is the one the artefact's POM `<licenses>` element names, and `licenceFile` is that text, copied byte for byte under `ThirdParty/licences/` |

- The rows list every artefact on a generated project's `shippingRuntimeClasspath`, direct and transitive, for every library a row's `selectedBy` names; the person who adds a direct row records its transitive closure from `gradlew :app:dependencies --configuration releaseRuntimeClasspath` of the seed in the same diff.
- `Test-Zenith2MavenArtefacts` (`Zenith2Env.psm1`) checks the schema, the order, the coordinate form, each `licenceSha256` against its file, each licence against `allowedRuntime` by V27's evaluation, each `versionKey` against the seed's `gradle.properties`, and that every coordinate has a `<component group=… name=… version=…>` element in `verification-metadata.xml`; the `Test` step runs it over the committed file (`Pwsh_Ship_MavenArtefacts_Committed`), and `Setup-Zenith2.ps1` runs it at step 2 after the validator.
- The licence inventory of an Android package of game `G` lists every row whose `selectedBy` meets `G`'s closure as a `gradle-artefact` component (`Zenith2/Docs/Design/SHIP/Release.md` → "Inventory selection algorithm"), and checklist item A14 compares the release's resolved classpath with exactly those rows (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Android package checklist"); a Windows package lists none.
- `Update-Zenith2GradleVerification.ps1` regenerates `verification-metadata.xml` from the seed, which declares every direct row, so a row and its checksums always land in the same diff.

### Android SDK provisioning

1. Verify and extract `androidSdk.commandLineTools` into `android-sdk\<dir>\cmdline-tools\latest\`.
2. Without `-AcceptAndroidSdkLicenses`, stop with exit 4.
3. For each package: run the pinned `sdkmanager --sdk_root=<sdk> --install "<id>"` (the `id` names an exact package version, for example `build-tools;35.0.0`), then compute the tree digest (§ "Tree digest") of `<sdk>/<path>/` and compare it with the package's `treeSha256`; a mismatch deletes the package directory and fails (`Android SDK package <id>: installed tree does not match the pinned treeSha256`). The pin is the SHA-256 tree digest recorded when the package was pinned; the SHA-1 checksums `sdkmanager` checks itself are never relied on.
4. `ZENITH_ANDROID_SDK_DIR` is the SDK root; `apksigner`, `zipalign` and `aapt2` come from its `build-tools\<version>\`.

### WSL provisioning

Run by `Setup-Zenith2.ps1` on a Windows host unless `-SkipWsl`; every command goes through `wsl.exe` with an explicit user, so no step relies on a `sudo` rule inside the distro:

1. Import the pinned root filesystem (`wsl --import <distro> %LOCALAPPDATA%\Zenith2\wsl\<distro> <verified rootfs>`) when the distro is absent; an existing distro of that name is used and verified.
2. On first import, as root (`wsl.exe -d <distro> -u root -- …`): create the non-root default user `zenith` (`useradd --create-home --shell /bin/bash zenith`, with the `sudo` rule for `sysctl` alone that cross-area row DV16 (a) asks for DEVT's own tools), write DEVT's `Tools/DevTool/Wsl/wsl.conf` to `/etc/wsl.conf` (systemd on, `appendWindowsPath=false`, `metadata` automount, `[user] default=zenith`), then `wsl --terminate <distro>` so the configuration applies.
3. Root steps of § "Linux host mode", each as `wsl.exe -d <distro> -u root -- bash -c "<command>"`: step 3 (the snapshot sources and `wsl.packages`, § "Linux package snapshot") and step 5 (the sysctl file and `sysctl --system`).
4. Windows side: download and verify, by steps 1 to 4 of § "Digest verification algorithm", every `linux` toolchain archive (`pwsh`, `cmake`, `ninja`, `sccache`, `llvm`, `lavapipe`) and every source archive of an entry whose `platforms` contain `linux`, into `%LOCALAPPDATA%\Zenith2\.partial\linux\<name>-<sha256>.<format>`.
5. User steps as `zenith` (`wsl.exe -d <distro> -u zenith -- …`). First the `pwsh` bootstrap, driven by `Setup-Zenith2.ps1` on the Windows side because no pinned `pwsh` exists in the distro yet: (i) `Expand-Zenith2LinuxArchive` extracts the verified `pwsh` archive into `$HOME/.zenith2/.partial/pwsh-<sha256>/` with the system `tar` (§ "Linux archive extraction"); (ii) the tree digest of § "Tree digest" is computed inside the distro by the pinned root filesystem image's own `find`, `sort`, `readlink`, `cut` and `sha256sum` (its coreutils and findutils): the module writes the script below to the standard input of `wsl.exe -d <distro> -u zenith -- bash -s -- <tree>` (no quoting crosses the Windows command line) and captures the digest text it prints, which is exactly the text that `-Check` recomputes with .NET (byte order, `/` separators, links never followed); the module computes that text's SHA-256 with .NET over the captured UTF-8 bytes; (iii) the module writes the stamp (`treeSha256` that value) to `.partial\linux\pwsh.zenith2-stamp.json` on the Windows side; (iv) inside the distro, `cp` copies the stamp to `$HOME/.zenith2/.partial/` (the same volume as its destination), then `mv` renames the stamp to `$HOME/.zenith2/toolchain/pwsh/<version>.zenith2-stamp.json` and the tree to `$HOME/.zenith2/toolchain/pwsh/<version>/`, stamp first, as step 8 of § "Digest verification algorithm" orders. Every further user step then runs inside the distro under that pinned interpreter, `wsl.exe -d <distro> -u zenith -- <pinned pwsh> -NoProfile -Command "Import-Module /mnt/<checkout>/Zenith2/Tools/Setup/Zenith2Env.psm1; Install-Zenith2LinuxHost -Manifest (Get-Zenith2Manifest -Root /mnt/<checkout>) -ArchiveRoot /mnt/<drive>/…/.partial/linux"`, which performs user steps 4, 6 and 7 of § "Linux host mode" from the archives step 4 verified (extraction, patches, licence check, tree digest, stamp, publication) and downloads nothing.

   The digest script of (ii), read by `bash -s` with the tree as `$1`:

   ```sh
   cd "$1" || exit 1
   LC_ALL=C find . -mindepth 1 \( -type f -o -type l \) -printf '%P\n' | LC_ALL=C sort |
   while IFS= read -r p; do
   	if [ -L "$p" ]; then printf 'link %s %s\n' "$(readlink "$p")" "$p"
   	else printf '%s %s\n' "$(sha256sum "$p" | cut -d' ' -f1)" "$p"; fi
   done
   ```

6. Delete the verified downloads of step 4 once every publication succeeded.
7. `-Check` additionally runs DEVT's `Tools/DevTool/Scripts/Test-LinuxTsanHost.ps1` inside the distro as `zenith` (cross-area row DV16 (b)).

### Development signing keystore

<!-- elaboration -->
The Gradle `release` build type signs with the development key (the table "The Gradle build types" under `Zenith2/Docs/Architecture/BuildSystem.md` → "Android packaging"): `keys\development.keystore`, created once per machine by the pinned JDK's `keytool -genkeypair -keystore <path> -storetype PKCS12 -alias zenith2-development -keyalg RSA -keysize 3072 -validity 10000 -dname "CN=Zenith2 Development" -storepass zenith2-development -keypass zenith2-development`. The password is fixed and public because the key signs nothing that is distributed; the key is never committed and never used for a `shipping` artefact (`Zenith2/Docs/Design/SHIP/Release.md` → "Key custody"). `Enter-Zenith2Env.ps1` exports its path as `ZENITH2_DEV_KEYSTORE`, which MOD's `<Name>_Apk` target passes to Gradle (cross-area row SH3).

## Dependency updates

### Update procedure

A dependency update (Z2-I-SHIP-10) is one human-prepared diff, in this order:

1. `pwsh -NoProfile -File Zenith2/Tools/Setup/Update-Zenith2Dependency.ps1 -Name <name> -Version <version> -Url <url>` downloads the new archive, computes its SHA-256, extracts it to `.partial\`, computes `licenceSha256` of the entry's `licenceFile` (and fails when the file moved, naming it), compares the new licence text with the old one, and rewrites the entry's `version`, `source.url`, `source.sha256`, `source.stripPrefix`, `source.format` and `licenceSha256`, preserving every other field and the file's formatting (one entry replaced in place, keys in the existing order).
2. If the licence text changed, the script prints both texts' SHA-256 and the unified diff of the two texts, and sets exit code 5; the human reviews it, and a licence whose SPDX identifier changed needs `licence_policy.json` to admit it (rule V27).
3. Every pin of § "Recorded pins" that depends on the version (Luau's `commit`, `releaseTag`, bytecode versions; the Agility `sdkVersion`; Vulkan-Profiles' `jsonSha256`) is updated by the human in the same diff; the script lists the entry's `pins` keys and refuses to finish (exit 6) until `-PinsReviewed` is passed.
4. An entry with `patches`: the script applies each patch, in array order, to the new tree in `.partial` exactly as § "Patch application" does; when every patch applies, the patch files and `patchSetSha256` are unchanged and the new tree resolves to `<new sha256>-p<16 hex>`; when one does not, the script exits 7 naming it (`patch <file> does not apply to <name> <version>; rebase it`), rewrites nothing, and the human rebases the patch (a new file with a new `sha256`, under the same or a new `decision`) and re-runs.
5. `Setup-Zenith2.ps1 -Only <name>` publishes the new tree.
6. The diff lands through an ordinary gated ticket, so every conformance suite of the owning libraries runs on it ([Z2-N-ARCH-126]); a Luau upgrade also runs SCRIPT's re-qualification list ([Z2-N-SCRIPT-502], `Zenith_Script_PinQualified_Check`).

### Update-Zenith2Dependency

| Exit code | Meaning |
|---|---|
| 0 | the entry was rewritten; licence unchanged; pins reviewed |
| 1 | download, digest or extraction failure (nothing rewritten) |
| 2 | usage error, or `-Name` is not a manifest entry |
| 5 | the licence text changed (entry rewritten; review required) |
| 6 | the entry has pins and `-PinsReviewed` was not given (entry rewritten) |
| 7 | a patch of the entry does not apply to the new version (nothing rewritten) |

When several conditions hold the lowest-numbered failure among 1, 2 and 7 wins and nothing is rewritten; otherwise, with the entry rewritten, exit 5 takes precedence over 6 (a licence change is reviewed first), and the output always lists every condition that held, so a run that exits 5 also prints the pins still to review.

The script never edits `toolchain` pins; a toolchain update is the same procedure with `Update-Zenith2Toolchain.ps1 -Key <key> -Version <version> [-Url <url> per host]`, which also rewrites `version`, the archive objects and `clVersion`/`toolsetVersion`/`pkgRevision` where the key has them (read from the installed tool).

### What an upgrade diff contains

| Always | When applicable |
|---|---|
| the rewritten manifest entry | changed `pins`; new or rebased `patches` with their decision IDs; `licence_policy.json` together with its DecisionLog ruling, a `needs-human` diff (Z2-R-SHIP-132, § "Licence fields"); updated `verification-metadata.xml` (AGP, Gradle); regenerated committed files the dependency feeds (the Vulkan-Profiles generated headers); the owning area's design-doc section naming the version (Z2-D-085) |

The upgrade's ticket is a SHIP ticket (MAINT owns the rehearsal policy that schedules upgrades, Z2-I-MAINT-03); because only a human can download and verify an archive, the manifest half is `needs-human`, and the loop ticket that follows it adapts code to API changes against the already-pinned entry.

### Evidence invalidation

Any change to `dependencies` changes `dependenciesHash`, and any change to `toolchain` changes `toolchainHash`; both are fingerprint inputs, so every evidence record that depended on the old values is rejected by acceptance mode ([Z2-N-BUDGET-098], [Z2-N-BUDGET-099], Z2-D-065). The coverage gate step's fingerprint tests (a middleware bump and a toolchain bump each invalidate a record, [Z2-N-BUDGET-100]) use fixture manifests from `Zenith2/CMake/Tests/Ship/FingerprintInputs/`, which SHIP provides: `base.json`, `dependency-bump.json` (one `source.sha256` changed) and `toolchain-bump.json` (one `clVersion` changed). No fixture records a hash: a consumer computes the three hashes of each fixture at test time and asserts the relations (the dependency bump changes `dependenciesHash` and `manifestSha256` only; the toolchain bump `toolchainHash` and `manifestSha256` only), and SHIP's own `Pwsh_Ship_ManifestHashes_FingerprintFixtures` cross-checks the PowerShell hashes against SHA-256 values computed by .NET over the canonical text the independent CMake implementation writes (`Zenith2/Docs/Design/SHIP/Tests.md` → "Fixture digests"), so no worker ever records a digest.
