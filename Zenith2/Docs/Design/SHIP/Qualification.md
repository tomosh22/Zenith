# SHIP design — CI matrix, device matrix and shipping qualification

Companion to `Zenith2/Docs/Design/SHIP.md`. This file specifies the CI matrix that `.github/workflows/zenith2-gates.yml` implements (Z2-I-SHIP-02), the device matrix and its support classes (Z2-I-SHIP-03), and the Stage 6 shipping qualification as testable checklists, together with the qualification record and the budget rows the qualification measures. The workflow file and `Run-Committed.ps1` are protected PROC files (Roadmap Stage 0 deliverables 8 and 11); this file is their specification, and the PROC tickets implement it.

## Scope of this file

| Section | Specifies |
|---|---|
| "CI matrix" | triggers, jobs, job steps and invariants of `zenith2-gates.yml` |
| "Device matrix" | every device class, what it may be used for, and the rules |
| "Shipping qualification" | the qualification record and every checklist item, automated or human |
| "Measured rows at qualification" | the SHIP budget rows and where each is measured |

## CI matrix

### Workflow triggers

The workflow runs only when the user pushes (Z2-D-031):

```yaml
on:
  push:
    paths:
      - "Zenith2/**"
      - ".github/workflows/zenith2-gates.yml"
      - "zagent.project.json"
      - "vault.config.json"
      - "Tools/tick_gates.ps1"
      - "Tools/Vault/**"
      - "Tools/build_lock.ps1"
  workflow_dispatch: {}
concurrency:
  group: zenith2-gates-${{ github.ref }}
  cancel-in-progress: true
permissions:
  contents: read
```

There is no `pull_request`, `schedule` or `release` trigger, no secret, and no write permission.

### Jobs

<!-- elaboration -->
| Job | Runs on | Condition | Runs | Never runs |
|---|---|---|---|---|
| `plan` | `ubuntu-24.04` | always | `Run-Committed.ps1 -ListJobs -Json`, which reads the union of every Z2 category's committed gate lines except `Zenith2Acceptance` (Z2-D-032) and emits `windows` (always `true` while any line is active), `linux` (`true` exactly when a `Sanitizers -Preset linux-tsan` line is active, [Z2-N-ARCH-221]) and the two jobs' `timeout-minutes` | builds or tests |
| `windows` | `[self-hosted, windows, x64, zenith2]` | `needs: plan` | every active line whose host is Windows: the base steps, the Android build and `ApkCheck` once active (Stage 2), `Sanitizers -Preset win-asan` once active, `BackendParity`, `PathParity` and the integration smoke on WARP and lavapipe once active | `Zenith2Acceptance` lines; any S24 or reference-hardware step |
| `linux` | `[self-hosted, linux, x64, zenith2]` | `needs: plan`, `if: needs.plan.outputs.linux == 'true'` | the `linux-tsan` lines, natively (no WSL), on an Ubuntu 24.04 host provisioned by `Setup-Zenith2.ps1` in its Linux host mode (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Linux host mode") | GPU tests (the tree contains no GPU library, [Z2-N-ARCH-173]) |

- The runners are self-hosted because the pinned MSVC instance, NDK, JDK, lavapipe and WARP are provisioned once by a human with the licence-acceptance switches (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Licence acceptance switches"); a hosted runner would have to accept licences on every run.
- Each job's `timeout-minutes` is the sum of the step timeouts of the lines it runs (rows Z2-B-PROC-007 to Z2-B-PROC-031 of `Zenith2/Docs/Budgets.md` → "Gate budget rows"), as `plan` emits it, so CI introduces no time limit of its own.
- Every job runs in the repository's checkout path on the runner, so the checkout hash and the build trees are the runner's own and are reused between runs (warm builds, the shared sccache).

### Job steps

| # | `windows` | `linux` |
|---|---|---|
| 1 | `actions/checkout` with `lfs: true`, `fetch-depth: 1` (no gate step reads history: `Pwsh_Ship_ReleaseDocs_Committed` runs the history-free items of the release documentation lint, `Zenith2/Docs/Design/SHIP/Release.md` → "Release documentation lint") | the same |
| 2 | `pwsh -NoProfile -File Zenith2/Tools/Setup/Setup-Zenith2.ps1 -Check -SkipWsl` with the runner's own `pwsh` (`-Check` does not require the pinned interpreter, `Zenith2/Docs/Design/SHIP/Manifest.md` → "Check mode"); a non-zero exit fails the job before anything builds | `pwsh -NoProfile -File Zenith2/Tools/Setup/Setup-Zenith2.ps1 -Check` on the Linux host (the Linux key set of "Check mode") |
| 3 | `. Zenith2/Tools/Setup/Enter-Zenith2Env.ps1` in the shell of step 4 (one `run:` block holds steps 3 and 4, so the `pwsh` of step 4 resolves to the pinned interpreter that step 3 put first on `PATH` and verified) | the same, in `pwsh` |
| 4 | `pwsh -NoProfile -File Zenith2/Tools/Gates/Run-Committed.ps1 -Host windows` | `pwsh -NoProfile -File Zenith2/Tools/Gates/Run-Committed.ps1 -Host linux` |
| 5 | `actions/upload-artifact` of `Zenith2/out/*/gates/**` (`if: always()`), name `zenith2-gates-windows` | the same of `$HOME/.zenith2/out/*/gates/**`, name `zenith2-gates-linux` |

### CI invariants

- CI runs exactly the union of the active Z2 gate lines except `Zenith2Acceptance`, and a local `/tick` runs a path-selected subset of it; the PROC test in `Zenith2/Tools/Gates/Tests` asserts the subset property (Z2-D-031).
- CI never signs a release artefact, never publishes, never reaches a release profile or a release, upload, Authenticode or update-signing key, never runs `New-Zenith2Release.ps1`, and never downloads a dependency (step 2 is `-Check`). The only signatures CI makes are those of SHIP's signing tests, which sign copies of `Minimal.exe` with a self-signed code-signing certificate the test creates with `CertificateRequest`, exports to a temporary PFX under its own `${ZENITH_OUT}/test-fixtures/SHIP/<Case>/` directory (never into a certificate store) for `signtool sign /f`, and deletes with that directory when the case ends (`Pwsh_Ship_SignedExeKeepsBuildId`, `Pwsh_Ship_Authenticode_TamperDetected`).
- CI never measures a budget row: WARP, lavapipe and the CI hosts are not budget platforms ([Z2-N-BUDGET-125]); the only rows the CI steps are held to are the gate-time rows.
- `Pwsh_Ship_CiWorkflowShape` (SHIP, Test step, `win`) reads `.github/workflows/zenith2-gates.yml` from `ZENITH_REPO_ROOT` and asserts the triggers, the three job names, their `runs-on` labels, the `needs` and `if` conditions, the step commands of § "Job steps" and the absence of `secrets.` references; the PROC ticket that edits the workflow keeps it green.

## Device matrix

### Devices

`Zenith2/Docs/Budgets.md` → "Reference hardware" fixes the two validated devices; this table is the complete matrix:

| ID | Device | OS | GPU | Backends | Support class | Used for |
|---|---|---|---|---|---|---|
| `DEV-WIN-REF` | the reference desktop: Ryzen 7 5800X-class, 32 GB, RTX 3070, 2560×1440 | Windows 11 | NVIDIA | Vulkan and D3D12 | validated | every `Win` budget row; Windows qualification; reference-hardware rulings |
| `DEV-S24` | Samsung Galaxy S24 Ultra (Snapdragon 8 Gen 3, Adreno 750), 2340×1080 | Android 14 or newer | Adreno 750 | Vulkan | validated | every `S24` budget row; Android qualification; the HWASan APK; device tests (Z2-D-010) |
| `DEV-WIN-AMD` | a desktop with an AMD RDNA 2 or newer GPU | Windows 11 | AMD | Vulkan and D3D12 | parity record | the cross-API parity record that AMD takes the D32F_S8 depth path (`Zenith2/Docs/Rendering.md` → "Render views"); never a budget row |
| `DEV-WARP` | WARP (pinned NuGet) on any Windows host | Windows | software | D3D12 | CI correctness | tests, the integration smoke, the path tracer caps |
| `DEV-LAVAPIPE` | lavapipe (pinned Mesa) on a Windows or Linux host | Windows, Linux | software | Vulkan | CI correctness | as `DEV-WARP` |
| `DEV-EMU` | the Android x86_64 emulator | Android 14 | host-emulated | Vulkan | smoke only | emulator smoke runs ([Z2-N-ARCH-216]); never acceptance, budget or parity |
| `DEV-ADRENO` | other Snapdragon devices with Adreno GPUs, Android 14 or newer | Android 14+ | Adreno | Vulkan | best effort | bug reports; never evidence |
| `DEV-MALI` | devices with Mali GPUs | — | Mali | — | unsupported | nothing (Z2-D-010) |

### Support classes

| Class | Meaning |
|---|---|
| validated | budgets, qualification and device evidence come from it; a defect found on it is triaged at its severity |
| parity record | one named record is taken from it; no budget, no qualification |
| CI correctness | correctness asserts and the CI smoke run on it; its timings are never evidence ([Z2-N-BUDGET-125]) |
| smoke only | it may boot the game; nothing it produces is evidence |
| best effort | the game may run; defects found only there are filed `S4_TRIVIAL` unless they reproduce on a validated device |
| unsupported | no test, no triage; release notes say so |

### Device matrix rules

- A qualification item or a budget row names one validated device; an evidence record from any other device is refused for it ([Z2-N-BUDGET-124]).
- The `## Platforms and builds` and `## System requirements` sections of every release note restate this matrix (`Zenith2/Docs/Design/SHIP/Release.md` → "Release notes").
- Adding a device to `validated` is a decision recorded in `Zenith2/Docs/DecisionLog.md` (it changes Z2-D-010's scope) and a change to `Zenith2/Docs/Budgets.md` → "Reference hardware" in the same diff.

## Shipping qualification

### Qualification record

<!-- elaboration -->
Each qualified release has `Zenith2/Docs/Release/<Game>/<version>/Qualification.json`, written in two passes:

1. `Test-Zenith2Release.ps1 -Staging <dir>` (run by the release pipeline's VERIFY state, `Zenith2/Docs/Design/SHIP/Release.md` → "New-Zenith2Release") writes every automated item's result into `reports/verify.json`;
2. the human running the Stage 6 acceptance copies it into `Qualification.json` and fills every `device` and `human` item with its result and evidence path.

```json
{
	"schema": "zenith2.qualification",
	"schemaVersion": 1,
	"game": "OpenWorldSlice",
	"version": "1.2.0",
	"releaseJsonSha256": "<64 hex>",
	"items": [
		{ "id": "W1", "kind": "automated", "result": "pass", "evidence": "reports/verify.json#W1", "note": "" },
		{ "id": "D3", "kind": "device", "result": "pass", "evidence": "Zenith2/Docs/Research/Budgets/Stage6/W-OpenWorld/…", "note": "" }
	],
	"result": "pass"
}
```

`result` is `pass` exactly when every item is `pass`; an item may be `waived` only with a `note` naming the `Z2-D-NNN` that waives it. `Test-Zenith2Release.ps1` exits 0 when every automated item passes, 3 otherwise, and lists each failure as `<id>: <reason>`.

### Windows package checklist

| ID | Item | Check (automated unless marked) |
|---|---|---|
| W1 | the package matches `release.json` | every file `release.json` lists exists with the recorded SHA-256 and size, every signed file's `unsignedSha256` equals `reports/unsigned.json`, and no unrecorded file exists in the staging layout outside `state.json`, `release.json` and `reports/` |
| W2 | the executable is signed | § "Windows Authenticode" of `Zenith2/Docs/Design/SHIP/Release.md`; `trust: publicCA` for `public` |
| W3 | only allowed files | the paths of the table under `Zenith2/Docs/Design/SHIP/Release.md` → "Windows package" and nothing else |
| W4 | no development artefact | no `*.pdb`, `*.ilk`, `*.lib`, `*.exp`, `*.map`, no executable other than `<Game>.exe`, no `d3d12SDKLayers.dll`, no `steam_appid.txt` in any case (ONLINE row ON9 (d)) |
| W5 | no development service or test code | (a Shipping-binary check: Release keeps Tracy and the development services by design, D2 and D6) the protected `Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1 -Image <Game>.exe -Symbols <retained pdb> -Platform windows` reports no E2 finding over `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Release-time audit tool"; SEC row SEC6 (e), (h)), and SHIP's own match over `llvm-pdbutil dump -publics <retained pdb>` finds no symbol matching `doctest::`, `tracy::`, `___tracy_` or `Zenith_ScriptDap` |
| W6 | hardened image | `llvm-readobj --file-headers --coff-load-config`: `IMAGE_DLL_CHARACTERISTICS_DYNAMIC_BASE`, `HIGH_ENTROPY_VA`, `NX_COMPAT` and `GUARD_CF` set, and the load configuration's `GuardFlags` contain `CF_INSTRUMENTED`; `llvm-readobj --coff-debug-directory`: a debug-directory entry of type `IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS` (20) whose value contains `IMAGE_DLLCHARACTERISTICS_EX_CET_COMPAT` (0x1), which is where `/CETCOMPAT` records it (not the load configuration) |
| W7 | windowed subsystem | `Subsystem: IMAGE_SUBSYSTEM_WINDOWS_GUI` |
| W8 | symbols retained | the retained PDB's GUID and age equal the executable's RSDS record; the store holds both tiers; `llvm-symbolizer` resolves `Zenith_PlayerMain` |
| W9 | content matches the build | `Content/mount-manifest.json` `buildId` equals the executable's build ID and `releaseLine` equals `release.json`'s |
| W10 | update key | the 32 bytes of the release profile's `updateManifest.publicKey` (whose SHA-256 is `publicKeySha256`) occur exactly once in the executable's image, so the Player verifies patches against the key whose private half the operator holds |
| W11 | licences | `Licences/licences.json` equals the inventory recomputed by § "Inventory selection algorithm" for the release's tree; every `Licences/<name>.txt` hash equals its `licenceSha256`; `THIRD_PARTY_NOTICES.txt` lists every component |
| W12 | boots headless | `<Game>.exe --headless --exit-after-ticks 300 --timeout-seconds 120` in a fresh copy of the package directory exits 0 |
| W13 | no loose mount | `<Game>.exe --assets-root <any dir> --headless --exit-after-ticks 1` exits 2 |
| W14 | reproducible | `reports/reproducibility.json` `result` is `identical` (or `reported` for the Windows files of a `CrossCheckout` run) under the definition of `Zenith2/Docs/Design/SHIP/Release.md` → "Reproducibility definition" (internal releases may skip it) |
| W15 | imports resolve on a clean machine | every DLL the import table of `<Game>.exe` names (`llvm-readobj --coff-imports`, delay-load imports included) is either a file of the package or a system DLL: an `api-ms-win-*`/`ext-ms-win-*` API set, or a file present in `%SystemRoot%System32` whose Authenticode signer is Microsoft and whose name does not match the Visual C++ redistributable pattern (a case-insensitive regular expression: one of the prefixes `msvcp`, `vcruntime`, `concrt`, `vccorlib`, `vcamp`, `vcomp`, then `[0-9_]*`, an optional `d`, then `.dll`), because the release machine has the redistributable installed and a player's machine may not |
| W16 | no forbidden string | the same script's E3 layer reports no finding: no entry of `Zenith2/Tools/Gates/shipping_forbidden_strings.json` occurs in `<Game>.exe` as UTF-8, UTF-16LE or bytes (SEC row SEC6 (e)) |
| W17 | no listener import | the same script's E4 layer reports no finding: the import table names no socket-listening function the listener-import scan forbids (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Listener-import scan") |

### Android package checklist

| ID | Item | Check |
|---|---|---|
| A1 | signed with the release key | `apksigner verify --print-certs --min-sdk-version 34`: v2 and v3 `true`, signer SHA-256 equal to `releaseCertSha256`, key algorithm `RSA` |
| A2 | 16 KB zip alignment | `zipalign -c -P 16 -v 4` exits 0 ([Z2-N-ARCH-235]) |
| A3 | 16 KB ELF alignment | `llvm-readelf -l` of every `.so` in the APK: every `PT_LOAD` alignment ≥ `0x4000` ([Z2-N-ARCH-214]) |
| A4 | one native library | the APK holds exactly `lib/arm64-v8a/lib<Game>.so` ([Z2-N-ARCH-854]) |
| A5 | entry point exported | `llvm-readelf --dyn-syms` lists `SDL_main` as a defined default-visibility function ([Z2-N-ARCH-236]) |
| A6 | no development service or test code | `Test-Zenith2ShippingExposure.ps1 -Image <the retained unstripped lib<Game>.so> -Platform android` reports no E2 finding, and SHIP's match of W5's patterns over `llvm-nm --defined-only` of the same object finds none |
| A7 | not debuggable | `aapt2 dump xmltree --file AndroidManifest.xml <apk>`: no `android:debuggable`, and no `android:extractNativeLibs="true"` ([Z2-N-ARCH-245]) |
| A8 | symbols retained | the packaged object's build-ID note equals the retained unstripped object's; the store's `.build-id` tier holds it; `llvm-symbolizer` resolves `Zenith_PlayerMain` |
| A9 | bundle signed | when an App Bundle exists: `jarsigner -verify -strict` exits 0 and the signer's SHA-256 equals `uploadCertSha256` |
| A10 | licences | `assets/licences/licences.json` and the notices as W11 for `android` |
| A11 | hardened image | `llvm-readelf -l -d`: a `GNU_RELRO` segment, `BIND_NOW` (or `FLAGS_1` containing `NOW`), and a `GNU_STACK` segment without `E` |
| A12 | content matches the build | the APK asset `mount-manifest.json` `buildId` equals the shared object's build ID (its first 16 bytes, 32 hex digits) |
| A13 | backup rules exclude transient save files | `aapt2 dump xmltree` of the manifest names `android:dataExtractionRules` and `android:fullBackupContent`; the referenced XML resources exclude `saves/resume.zsave`, `*.z2journal`, `*.z2tmp` and `*.corrupt` and include the rest of `saves/` (SAVE's rule, cross-area row SV15; the template content is MOD's) |
| A14 | exactly the selected Maven runtime artefacts | `gradlew :app:dependencies --configuration shippingRuntimeClasspath --offline` in the release's generated Gradle project lists exactly the coordinates of the `maven-artefacts.json` rows the inventory selected for the game, no other and none missing, each at its row's version (`Zenith2/Docs/Design/SHIP/Release.md` → "Android package"); a game that selects no row lists no dependency |
| A15 | no forbidden string | the same script's E3 layer over the unstripped object reports no finding (W16's list) |
| A16 | no listener import | the same script's E4 layer over the object's dynamic imports reports no finding |
| A17 | permissions allowlisted | `aapt2 dump permissions <apk>`: the permission set equals the union of the `Zenith2/Tools/Security/android_permissions.json` `permissions[]` entries of the game's selected modules for build type `shipping`, and holds no `android.permission.INTERNET` unless such an entry allows it (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Android manifest audit") |
| A18 | exported components allowlisted | `aapt2 dump xmltree --file AndroidManifest.xml <apk>`: every component with `android:exported="true"` is the launcher activity or a class of the same file's `exportedComponents[]` |

### Device run checklist

Run by the human on the validated devices (`needs-human`, recorded in `Qualification.json` with kind `device`):

| ID | Device | Item |
|---|---|---|
| D1 | `DEV-WIN-REF` | the Windows archive extracted into a new directory; the game started from Explorer reaches world-ready on Vulkan and, with `--set rhi.backend=d3d12`, on D3D12, and exits through the menu with exit code 0 |
| D2 | `DEV-S24` | the signed APK installed with `adb install`; the game started from the launcher reaches world-ready, survives one suspend and resume, and exits with no logcat `FATAL` or `ZENITH … FAILED` line |
| D3 | `DEV-S24` | the sustained 30-minute run of every WS-SUSTAIN workload in ordinary conditions (rows Z2-B-SHIP-006 to Z2-B-SHIP-010; the procedure of `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure"), on the Release measurement package of the same tree |
| D4 | `DEV-S24` | memory pressure, thermal adaptation and audio interruption runs (the PLAT and AUDIO scenarios the Stage 6 acceptance lists in `Zenith2/Docs/Roadmap.md` → "Stage 6 Shipping qualification") |
| D5 | both | cold boot within Z2-B-SHIP-013 (Windows) and Z2-B-SHIP-014 (S24) from the installed package, MP-BOOT, three repetitions |
| D6 | `DEV-S24` | PACK's App Bundle, split-APK, asset-pack and installed-update tests (Z2-D-027), recorded by PACK's items and referenced here |

### Flagship slice checklist

For each of OpenWorldSlice, CinematicSlice and Game2D (Z2-D-021), the items of `Zenith2/Docs/Budgets.md` → "Production-quality acceptance":

| ID | Item | Evidence |
|---|---|---|
| F1 | the full playthrough of `Acceptance/Playthrough.md` completes on both validated devices from the packaged builds | the run's evidence records |
| F2 | every `FeatureChecklist.md` item passes | automated, golden and human items as listed |
| F3 | every per-platform budget row of the slice is met | evidence records from the Release measurement package (§ "Measured rows at qualification") |
| F4 | the bug bar: zero open S1 defects (Z2-B-SHIP-019) and the weighted count at or below the slice's cap (Z2-B-SHIP-020, -021 or -022), by MP-DEFECT | the defect count recorded at acceptance |
| F5 | every `SignOff.md` item signed | the human sign-off record |
| F6 | the playthrough script lasts 20 to 30 minutes (Z2-B-SHIP-023) | the run's measured duration |

### Security and compatibility items

| ID | Item | Evidence |
|---|---|---|
| S1 | SEC's hardening evidence: every fuzz target's full run without a finding, the tampered-patch cases rejected, the Shipping symbol check clean for every game ([Z2-N-ARCH-960]) | SEC's acceptance items, referenced |
| S2 | the compatibility demonstration passes (`Zenith2/Docs/Design/SHIP/Release.md` → "Demonstration suite") | the `ctest -R` run's JUnit report |
| S3 | `Setup-Zenith2.ps1 -Check` passes on the release machine and the manifest hashes in `release.json` equal the tree's | the setup report |
| S4 | the release notes and the upgrade guide of the release's fix version pass `Test-Zenith2ReleaseDocs.ps1 -Full` in a full clone | the lint output |

## Measured rows at qualification

Every SHIP budget row is measured with its own procedure on a validated device, from a Release measurement package of the tree that the Shipping release was built from ([Z2-N-BUDGET-108]: budget runs use Release builds; the budget-run switches are absent from Shipping, [Z2-N-ARCH-878]):

| Rows | Procedure | Device | Workloads | Breach |
|---|---|---|---|---|
| Z2-B-SHIP-001, -002 (frame time p95, p99) | MP-FRAME | `DEV-WIN-REF` (both backends), `DEV-S24` | WS-FRAME | BR-ACCEPT |
| Z2-B-SHIP-003, -004, -005 (exclusion window, run duration, frame count) | MP-FRAME | both | WS-FRAME | BR-INVALID |
| Z2-B-SHIP-006 (sustained floor) | MP-SUSTAIN | `DEV-S24` | WS-SUSTAIN | BR-ACCEPT |
| Z2-B-SHIP-007 to -010 (sustained run conditions) | MP-SUSTAIN | `DEV-S24` | WS-SUSTAIN | BR-INVALID |
| Z2-B-SHIP-011 (dynamic-resolution floor) | MP-DYNRES | both | WS-FRAME | BR-ACCEPT |
| Z2-B-SHIP-012 (hitch threshold) | MP-HITCH | both | WS-FRAME | BR-REPORT |
| Z2-B-SHIP-013, -014 (cold boot) | MP-BOOT | `DEV-WIN-REF`, `DEV-S24` | W-Load | BR-ACCEPT |
| Z2-B-SHIP-015 to -018 (hitch counts and maximum frame times, warm and cold caches) | MP-HITCH, MP-CACHE | both | WS-FRAME | BR-ACCEPT |
| Z2-B-SHIP-019 to -022 (bug bar) | MP-DEFECT | — | WS-SLICE | BR-ACCEPT |
| Z2-B-SHIP-023 (playthrough duration) | MP-FRAME | both | WS-SLICE | BR-INVALID |

<!-- elaboration -->
The rows a workload run samples per frame are bound in DEVT's `Zenith2/Benchmarks/Workloads/RowSources.json` by SHIP's own entries (orchestrator ruling DEVT-AREA-REGISTRATION; `Zenith2/Docs/Design/DEVT/Evidence.md` → "Row source bindings"), because the runner refuses a row without one:

| Row | `kind` | `statistic` | `unit` | Added by |
|---|---|---|---|---|
| Z2-B-SHIP-001 | `frameTime` | `p95` | `ms` | Z2T-SHIP-108 |
| Z2-B-SHIP-002 | `frameTime` | `p99` | `ms` | Z2T-SHIP-108 |
| Z2-B-SHIP-006 | `frameTime` | `rollingMin` | `fps` | Z2T-SHIP-108 |
| Z2-B-SHIP-011 | `dynres` | `min` | `%` | Z2T-SHIP-108 |
| Z2-B-SHIP-015, Z2-B-SHIP-017 | `hitch` | `count` | `frames` | Z2T-SHIP-040, the ruling that allocates them, in its own diff before its runs |
| Z2-B-SHIP-016, Z2-B-SHIP-018 | `frameTime` | `max` | `ms` | Z2T-SHIP-040, likewise |

Warm and cold rows share a source: the run's `-CacheState` tells them apart. The procedure and condition rows (Z2-B-SHIP-003 to -005, -007 to -010, -012) are applied by the runner and the procedure and never sampled, and the cold-boot rows (-013, -014, MP-BOOT) are timed from the runner's launch records (Z2T-DEVT-098), so none has an entry; the workloads of the hitch-row ruling and of the Stage 6 measurement name only the sampled rows. Cross-area row ShB16 asks DEVT whether an MP-BOOT binding is wanted.

The workload runner (`Zenith2/Benchmarks/Run-Workload.ps1`, [Z2-N-BUDGET-105]) launches the Release measurement package's executable (cross-area row SH10, answered by DEVT row DV16 (c): `-Package <staging dir>`, Release only, refusing a Shipping `release.json` and a package whose tree differs from the checkout's), so the measured binary is a packaged build of the qualified tree, never a development tree build.
