# SHIP design — the Shipping configuration and the release pipeline

Companion to `Zenith2/Docs/Design/SHIP.md`. This file specifies the exact differences of the `Shipping` configuration, the release pipeline that turns one committed tree into packaged, signed Windows and S24 builds, key custody, the release manifest, symbol identities and the symbol store, the licence inventory and notices, the reproducibility check, the release documentation and upgrade guides, and the Stage 6 demonstration of the per-kind compatibility policy. It elaborates `Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags" (table "Configurations"), `Zenith2/Docs/Architecture.md` → "Security architecture" and → "Third-party dependencies" ([Z2-N-ARCH-125]), and `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind". Additions are marked `<!-- elaboration -->`.

## Scope of this file

| Section | Specifies |
|---|---|
| "Shipping configuration" | every difference from Release, how development services are excluded, the link-closure checks |
| "Release pipeline" | `New-Zenith2Release.ps1`, the staging layout, the Windows and Android packages |
| "Signing" | key custody, Authenticode, APK and App Bundle signing, the update-manifest key hand-off |
| "Release manifest" | `release.json` |
| "Symbols" | identities, the store, collection, Play symbols, retention, symbolisation |
| "Licence inventory" | the selection algorithm, `THIRD_PARTY_NOTICES.txt`, `licences.json`, content licences |
| "Reproducible builds" | the definition and the check |
| "Release documentation" | release notes, known issues, upgrade guides, the lint |
| "Compatibility policy demonstration" | the per-kind test list and its check |

The SHIP, PACK split inside a package: PACK owns the installation layout's content tree (`Content/`, the archives, `mount-manifest.json`, App Bundle modules, asset packs, patch and DLC manifests, `update.zman`); SHIP owns the build that feeds it, the files SHIP adds beside it (executable, runtime redistributables, `Licences/`), signing, symbols, the release manifest and the verification (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", rows SHIP and PACK).

## Shipping configuration

### Differences from Release

`Shipping` and `Release` compile with the same optimisation flags (`/O2 /Ob2 /Gy /Gw`, `-O2`), the same reproducibility and hardening rows (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_apply_flags") and the same asserts setting, so a Release budget run measures the code Shipping distributes ([Z2-N-BUDGET-108]). The complete list of differences:

| # | Aspect | Release | Shipping | Mechanism |
|---|---|---|---|---|
| D1 | configuration define | `ZENITH_CONFIG_RELEASE=1` | `ZENITH_CONFIG_SHIPPING=1`; `Zenith_IsShippingBuild()` is `true` | [Z2-N-ARCH-247], `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" |
| D2 | development services: on-screen profiler overlay, development console, Luau DAP server, remote profiler, editor live link, script file watcher, cheat commands | present (overlay off by default) | absent | `if constexpr (!Zenith_IsShippingBuild())` at every entry, dead-stripped by `/OPT:REF` and `--gc-sections`; the `Libgraph` Shipping symbol check (§ "Shipping link closure checks") |
| D3 | loose mount, `--assets-root` | available | absent; the option is `COMMAND_LINE_ERROR_UNKNOWN_OPTION` (exit 2) | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema"; [Z2-N-ARCH-878] |
| D4 | budget-run switches (`--budget-run` and its companions, `--evidence-dir`) | available | `COMMAND_LINE_ERROR_UNKNOWN_OPTION` | [Z2-N-ARCH-878] |
| D5 | script bytecode source | package, patch and loose mounts | package and patch mounts only | [Z2-N-ARCH-957] |
| D6 | `shipping: excluded` dependencies (Tracy, the RenderDoc in-application API) | linked | not linked (`$<$<NOT:$<CONFIG:Shipping,ShippingTest>>:…>`) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Shipping-excluded dependencies"; [Z2-N-ARCH-124] |
| D7 | GPU validation layers and capture tools | enabled by configuration key | never enabled | `Zenith2/Docs/Design/RHI.md` (`Test_Validation_ShippingIgnoresKey`) |
| D8 | Windows subsystem | console | `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup` | BuildSystem MSVC `Shipping` row |
| D9 | build preset targets | everything | `zenith_players` only (`win-shipping`, `android-arm64-shipping`) | [Z2-N-ARCH-181] |
| D10 | Android Gradle build type | `release`: development key, not debuggable | `shipping`: built unsigned by Gradle, signed by the release pipeline with the release key; `getArguments()` returns no argument | § "Android APK signing"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Android packaging" |
| D11 | missing-PSO fallback, shader compile failure | magenta fallback, visible | the draw (or material bucket) is skipped, counted | `Zenith2/Docs/Rendering.md` → "Pipeline state objects" |
| D12 | script recovery default | manual | automatic (60 ticks, 3 recoveries) | `Zenith2/Docs/Scripting.md` → "Quarantine and hot reload" |
| D13 | Windows runtime staging | every `runtime` file (including `d3d12SDKLayers.dll`) | `pins.shippingRuntime` files only (`D3D12Core.dll`) | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Runtime file staging" |
| D14 | symbols in the package | the PDB sits beside the executable in the build tree | never in the package; retained in the symbol store | § "Symbols" |

`ShippingTest` ([Z2-N-ARCH-231], X216) is not a release configuration: it builds Shipping's code into test, conformance and integration-suite targets of the `win` tree alone, never a game executable, so the release pipeline never builds or packages it; its rows equal `Shipping`'s in every column of this table. Asserts are off in both (`ZENITH_ASSERTS_ENABLED=0` in every target of the configuration, host tools included; `ZENITH_CHECK`, `ZENITH_FATAL`, `ZENITH_UNREACHABLE` on), as `Zenith2/Docs/Contracts.md` → "Errors and results" ([Z2-N-CONTRACT-479], per configuration since the Q71 ruling) states; debug information is generated in both (`/Z7` and `/DEBUG:FULL`; `-g`), because the symbol store needs it.

### Development services in Shipping

<!-- elaboration -->
The established pattern, used by PLAT, SCRIPT, RHI and ASSET, is the only one SHIP admits:

1. A development service's public entry begins `if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(<AREA>_ERROR_NOT_IN_SHIPPING); }` (or creates nothing), so in Shipping no call reaches its implementation.
2. Its implementation's entry symbols are listed in `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` (a PROC edit per entry, [Z2-N-ARCH-963]).
3. The `Libgraph` step fails when a listed symbol survives in a Shipping executable ([Z2-N-ARCH-964]); the release verification repeats that check on the packaged binary (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Windows package checklist" item W5, "Android package checklist" item A6).

No source file is excluded from the Shipping configuration by the build system, so every source compiles in every configuration of its matrix ([Z2-N-ARCH-171]) and `CompileCoverage` sees it.

### Shipping link closure checks

| Check | Owner | Failure |
|---|---|---|
| the Shipping link closure of every game contains no `ZenithScriptCompiler`, `Luau.Compiler`, `Luau.Ast`, test, conformance, fuzz or benchmark target, `ZenithTestSupport`, or tool target | `Libgraph` ([Z2-N-ARCH-240], [Z2-N-ARCH-241]) | gate failure |
| no `shipping_forbidden_symbols.json` symbol in a Shipping executable | `Libgraph` ([Z2-N-ARCH-964]) | gate failure |
| no doctest symbol in the `win-release` Player link map | `TestSeparationCheck` ([Z2-N-CONTRACT-414]) | gate failure |
| the packaged binary repeats the symbol check against its retained symbols | the release verification (W5, A6) | release refused |

## Release pipeline

### New-Zenith2Release

```text
pwsh -NoProfile -File Zenith2/Tools/Setup/New-Zenith2Release.ps1
	-Game <Name>
	-Channel internal|candidate|public
	[-Platforms windows,android]        default: the game's game.json platforms
	[-Configuration Shipping|Release]   default Shipping; Release produces a measurement package (§ "Release staging layout")
	[-Unsigned]                         internal channel only: skip signing (release.json records "unsigned")
	[-Publish]                          also copy to the release archive and the symbol store of the release profile
	[-SkipReproducibility]              internal channel only
	[-Resume] [-Restart]                § "Release pipeline state machine" of Zenith2/Docs/Design/SHIP.md
```

<!-- elaboration -->
The release pipeline is run by a human (it uses signing keys, which never reach a loop worker or CI). It is a state machine (`Zenith2/Docs/Design/SHIP.md` → "Release pipeline state machine"); each state writes its result to `<run>/state.json`, where the run directory `<run>` is `${ZENITH_OUT}/Release/<Game>/<version>/<channel>-<tree12>/` for a `-Configuration Shipping` run and `${ZENITH_OUT}/Release/<Game>/<version>/<channel>-<tree12>-release/` for a `-Configuration Release` run (one rule for `state.json`, the staging layout and the published copy), so an interrupted run resumes from the last completed state with the same arguments (`-Resume`) or starts over (`-Restart`). `<version>` is `game.json`'s `version`; `<tree12>` the first 12 hex digits of the tested tree SHA.

The states run in this order, and each reads only what an earlier state has finished: the archives are written from the signed tree, `release.json` records every file including the archives, and the verification reads the finished `release.json`, which therefore never records the verification's own result (that is `reports/verify.json`, whose SHA-256 the release archive index records, § "Release archive index").

| State | Action | Exit code on failure |
|---|---|---|
| PRECONDITIONS | `git status --porcelain --untracked-files=all` empty outside `Zenith2/out/`; `Setup-Zenith2.ps1 -Check` exit 0; the release profile valid for the channel (§ "Key custody"); `game.json` `version` not already released on this channel (no `release.json` for it in the release archive); `version` greater than every `version` of the release archive index (§ "Release archive index") with the same `game` and the same MAJOR.MINOR, whatever its build or channel (cross-area row PK25), else `release: version <v> not greater than <w> of line <MAJOR.MINOR>` naming the conflicting row; the release profile's `updateManifest.publicKey` equal to the `publicKey` of the one `active` record of `Zenith2/Tools/Security/update_keys.json` whose `game` is the game and whose `releaseLine` is the MAJOR.MINOR of `version`, else `release: update key of the profile is not the active key of <game> <line>` (no active record: `release: no active update key for <game> <line>`; SEC row SEC6 (f)) | 1 |
| BUILD | `cmake --preset win` and `--preset android-arm64` (configure), `cmake --build --preset win-tools`, `cmake --build --preset win-shipping`, `cmake --build --preset android-arm64-shipping`, then the game's `<Game>_Licences` target and PACK's `<Game>_Package` target for each platform (§ "Windows package", § "Android package"); `-Configuration Release` uses `win-release` and `android-arm64` | 2 |
| STAGE | copy the package trees into the staging layout; a `steam_appid.txt` (any case) in the Shipping build's `bin` directory or in a package tree is refused with `release: steam_appid.txt in a Shipping tree: <path>` (ONLINE row ON9 (d)) | 2 |
| SYMBOLS | collect symbols into `symbols/` (§ "Collecting symbols") | 5 |
| SIGN | record the SHA-256 of every file it is about to sign into `reports/unsigned.json` (`[{ path, sha256 }]`, byte-ordered), then sign (§ "Signing"); with `-Unsigned` it records the file and signs nothing | 4 |
| ARCHIVE | write the deterministic archives of the signed package trees and of `symbols/` (§ "Release staging layout") | 2 |
| MANIFEST | write `release.json` (§ "release.json schema") over every file of the staging layout except `state.json`, `release.json` itself and `reports/` | 3 |
| VERIFY | run the reproducibility check unless skipped (§ "Test-Zenith2Reproducibility", writing `reports/reproducibility.json`), then `Test-Zenith2Release.ps1` over the staging tree and its `release.json` (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Windows package checklist", "Android package checklist"), writing `reports/verify.json` | 3 |
| PUBLISH | with `-Publish`, and only when `reports/verify.json` reports `pass`: copy the staging directory to `<releaseArchive>/<Game>/<version>/<run name>/` (`<run name>` is the last component of `<run>`) and the symbols into the store (§ "Symbol store layout"), refusing to overwrite an existing directory, and append the index row (§ "Release archive index") | 5 |
| DONE | print the summary | — |

Exit codes: 0 done; 1 precondition; 2 build or packaging; 3 manifest or verification; 4 signing; 5 symbols or publish; 6 usage (an unknown game, `-Unsigned` or `-SkipReproducibility` on a non-internal channel, `-Resume` with different arguments). Every failure prints `Zenith2 release: <state>: <reason>` and leaves `state.json` naming the failed state.

### Release staging layout

```text
<run>/                                      ${ZENITH_OUT}/Release/<Game>/<version>/<channel>-<tree12>[-release]/
	state.json                                  the pipeline state (never archived, never in release.json)
	release.json                                the release manifest
	windows/<Game>/                             the Windows package tree (§ "Windows package")
	android/<Game>-<version>.apk                the signed shipping APK
	android/<Game>-<version>.aab                PACK's App Bundle, upload-signed (when PACK's bundle target exists)
	symbols/windows/<Game>.pdb                  the retained PDB
	symbols/android/arm64-v8a/lib<Game>.so      the unstripped shared object
	symbols/android/native-debug-symbols.zip    Play's native symbol upload (§ "Play native debug symbols")
	archives/<Game>-<version>-windows-x64.zip   the Windows package, deterministic
	archives/<Game>-<version>-symbols.zip       the symbols, deterministic
	reports/unsigned.json                       the pre-signing digests (state SIGN)
	reports/reproducibility.json                the reproducibility report (state VERIFY)
	reports/verify.json                         the verification report (state VERIFY)
```

- A deterministic archive is written with the pinned `cmake -E tar cf <archive> --format=zip --mtime=1980-01-01T00:00:00Z --files-from=<list>`, where the list holds every file of the tree in byte order of its relative path; two runs over the same tree produce byte-identical archives (`Pwsh_Ship_DeterministicZip_TwiceIdentical`).
- A `-Configuration Release` run writes the same layout, `state.json` included, under its run directory `<channel>-<tree12>-release/`, never signs, records `"configuration": "Release"`, and is never published on the `public` channel: it is the measurement package that the Windows install-size row's procedure measures ([Z2-N-BUDGET-175]) and that qualification budget runs launch.

### Windows package

<!-- assumption: pending PACK -->
PACK's design (drafted after SHIP) defines the target `<Game>_Package`, which writes the installation layout of the cooked content to `${ZENITH_OUT}/Packages/<Game>/windows/<Config>/` (`Content/mount-manifest.json` with `buildId` from `Get-Zenith2BinaryIdentity` of the built executable, the archives); SHIP assumes that target and that directory (cross-area row SH5 (a)).
<!-- /assumption -->

The Windows package tree `windows/<Game>/` holds exactly:

| Path | Source |
|---|---|
| `<Game>.exe` | `${ZENITH_OUT}/win/Shipping/bin/<Game>.exe` |
| `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll` and any further `msvc-crt` file (each only when the executable imports it) | the `msvc-crt` runtime component (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain runtime components"), selected by `llvm-readobj --coff-imports`; the component's `files` list every redistributable DLL the pinned toolset can make an executable import |
| `D3D12/D3D12Core.dll` | the executable's staged `ZENITH_RUNTIME_FILES` for Shipping (`pins.shippingRuntime`) |
| the `runtime` files of every other `usage: runtime` `prebuilt` entry in the executable's closure, under their `runtimeSubdir` (`steam_api64.dll` beside the executable when the composition links `ZenithOnline_Steam`, ONLINE row ON9 (d)) | the executable's staged `ZENITH_RUNTIME_FILES` for Shipping |
| `Content/…` | PACK's installation layout |
| `Licences/THIRD_PARTY_NOTICES.txt`, `Licences/licences.json`, `Licences/<name>.txt` | the `<Game>_Licences` output for `Shipping` (§ "Licence inventory") |

Nothing else, and never a `steam_appid.txt` (the Steam client's development app-ID file, which a development run may write beside the executable; W4): the verification fails on any other file (W3), and on any import of `<Game>.exe` that is neither a packaged file nor a system DLL (W15, `Zenith2/Docs/Design/SHIP/Qualification.md` → "Windows package checklist"), so a package that would fail to start on a machine without the Visual C++ redistributable is refused on the release machine, which has it. The Vulkan loader is the system's (`vulkan-1.dll` from the GPU driver); volk loads it at run time, so no Vulkan runtime is shipped.

### Android package

<!-- assumption: pending PACK -->
PACK's design (drafted after SHIP) defines the App Bundle target, its unsigned output path `${ZENITH_OUT}/Packages/<Game>/android/<Config>/<Game>-unsigned.aab`, its asset packs and the bundletool split-APK tests (Z2-D-027); SHIP assumes that path, signs the bundle (§ "App Bundle upload signing") and verifies it (A9) (cross-area row SH5 (b)).
<!-- /assumption -->

- The APK is the Gradle `shipping` build type's output `app-shipping-unsigned.apk` from `<Game>_Apk` built in the `android-arm64-shipping` preset (`assembleShipping`), aligned and signed by § "Android APK signing", renamed `<Game>-<version>.apk`.
- Its `assets/licences/` holds the `<Game>_Licences` output for `Shipping`, which reaches the APK because the generated Gradle project's `shipping` build type lists `${ZENITH_OUT}/Games/<Game>/android-arm64/licence-assets/Shipping` among its `assets.srcDirs` (cross-area row SH3 to MOD).
- The generated Gradle project resolves exactly the Maven artefacts of `Zenith2/ThirdParty/gradle/maven-artefacts.json` whose `selectedBy` meets the game's link closure (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Maven artefact rows"; ONLINE's `play-services-games-v2` and `billing` with `ZenithOnline_GooglePlay`, PACK's `asset-delivery` with its delivery library; A5 rows OnB14, PkB6), and a game that links no selecting library resolves none. The template itself declares the Android Gradle Plugin and nothing in an `implementation`, `api` or `runtimeOnly` configuration (SDL's Java activity is copied as source from `pins.androidJavaDir`, not resolved); the dependency lines are added by MOD's Gradle generation per composition. `Pwsh_Ship_GradleTemplateDeclaresNoRuntimeDependency` reads MOD's template `Zenith2/Player/GradleTemplate/app/build.gradle.kts.in` (`Zenith2/Docs/Design/MOD/GameBuild.md`, Z2T-MOD-002) in every `Test` step and fails on any such dependency line, and checklist item A14 compares the release's own resolved classpath (`gradlew :app:dependencies --configuration shippingRuntimeClasspath --offline`) with the selected rows, so every byte of the APK is accounted for by the licence inventory. A Maven runtime library is added only as a row, in a `needs-human` diff that also admits its licence term when `allowedRuntime` lacks it (cross-area row SH15).

## Signing

### Key custody

<!-- elaboration -->
Every signing key is protected at rest by a secret that is not in a file: a password held in an environment variable at the moment of signing, or the Windows certificate store's non-exportable key storage. A loop worker can read any file on the machine with its `Read` tool, but it has no shell, cannot read environment variables and cannot call the certificate store or a decryption routine, so no key is usable from its reach; CI never holds any of them (`Zenith2/Docs/Design/SHIP/Qualification.md` → "CI invariants").

| Key | Where it lives | Protected by | Who uses it | Never |
|---|---|---|---|---|
| development keystore | `%LOCALAPPDATA%\Zenith2\keys\development.keystore`, per machine, fixed public password (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Development signing keystore") | nothing (it signs nothing distributed) | the Gradle `release` build type | signs a `shipping` artefact; leaves the machine |
| Android release key (APK signing) | a PKCS#12 keystore at the path the release profile names, outside every checkout; an RSA key of at least 3072 bits (§ "Android APK signing") | the keystore password, `releasePasswordEnv` | `Sign-Zenith2Android.ps1` | in the repository, in `Zenith2/out/`, in CI |
| Android upload key (App Bundle) | the same, a separate keystore | `uploadPasswordEnv` | `Sign-Zenith2Android.ps1 -Bundle` | as above |
| Windows Authenticode certificate | the Windows certificate store (`Cert:\CurrentUser\My`), selected by thumbprint, its private key imported **non-exportable** | the store; `Get-Zenith2ReleaseProfile` refuses a certificate whose private key is exportable (the CNG key's export policy, or `CspKeyContainerInfo.Exportable` for a CSP key) | `Sign-Zenith2Windows.ps1` | exported to a file |
| update-manifest Ed25519 private key | an encrypted PKCS#8 PEM file (`-----BEGIN ENCRYPTED PRIVATE KEY-----`, PBES2 with PBKDF2-HMAC-SHA256 of at least 600,000 iterations and AES-256-CBC) at the path the release profile names, outside every checkout | the password in `updateManifest.passwordEnv` | PACK's update-manifest writer, through `Get-Zenith2UpdateSigningKey` (§ "Update manifest signing hand-off", cross-area row SH5 (c)) | stored unencrypted anywhere; in the repository; its public half is the `constexpr` key compiled into the Player ([Z2-N-ARCH-955]) |

The release profile is `%APPDATA%\Zenith2\release-profile.json`, read by `Get-Zenith2ReleaseProfile` (exported by `Zenith2/Tools/Setup/Zenith2Release.psm1`):

```json
{
	"schema": "zenith2.release-profile",
	"schemaVersion": 1,
	"releaseArchive": "D:/Zenith2/Releases",
	"symbolStore": "D:/Zenith2/Symbols",
	"windows": {
		"authenticodeThumbprint": "<40 hex>",
		"trust": "publicCA",
		"timestampUrl": "http://timestamp.acs.microsoft.com"
	},
	"android": {
		"releaseKeystore": "D:/Zenith2/Keys/release.p12",
		"releaseKeyAlias": "zenith2-release",
		"releaseCertSha256": "<64 hex>",
		"releasePasswordEnv": "ZENITH2_ANDROID_RELEASE_PASSWORD",
		"uploadKeystore": "D:/Zenith2/Keys/upload.p12",
		"uploadKeyAlias": "zenith2-upload",
		"uploadCertSha256": "<64 hex>",
		"uploadPasswordEnv": "ZENITH2_ANDROID_UPLOAD_PASSWORD"
	},
	"updateManifest": {
		"encryptedPrivateKeyFile": "D:/Zenith2/Keys/update-ed25519.p8.pem",
		"passwordEnv": "ZENITH2_UPDATE_KEY_PASSWORD",
		"publicKey": "<base64 of the 32-byte Ed25519 public key>",
		"publicKeySha256": "<64 hex>"
	}
}
```

- Passwords are never stored in the profile; each is read from the environment variable it names, at the moment of signing, and is passed to the tool as `env:<NAME>` (apksigner, jarsigner's `-storepass:env`), never on a command line.
- `trust` is `publicCA` or `test`. A `test` profile (a self-signed code-signing certificate the human created) is valid for the `internal` and `candidate` channels only; `public` requires `publicCA` and a `timestampUrl`.
- Every path in the profile must resolve outside every registered checkout and outside `Zenith2/out/`; `Get-Zenith2ReleaseProfile` fails (`release profile: <key> points inside a checkout`) otherwise. The registered checkouts are every `worktree <path>` line of `git worktree list --porcelain` run in the release checkout (the main checkout and every lane and linked worktree of the repository), and the comparison is case-insensitive over fully resolved paths.
- `Get-Zenith2ReleaseProfile` also refuses (`release profile: updateManifest.encryptedPrivateKeyFile is not an encrypted PKCS#8 key`) a key file whose first line is not `-----BEGIN ENCRYPTED PRIVATE KEY-----`, which rejects a plaintext PKCS#8 PEM, a raw 32- or 64-byte key and any other form, and an exportable Authenticode key as stated in the table.
- A profile is required for every channel except `internal` with `-Unsigned`.

### Windows Authenticode

`Sign-Zenith2Windows.ps1 -Path <file>… -Profile <profile>` signs each Zenith2-built PE (the game executable; never a Microsoft redistributable, which is already signed) with the pinned Windows SDK's `signtool sign /fd SHA256 /sha1 <thumbprint> /td SHA256 /tr <timestampUrl> <file>` (`/tr` and `/td` omitted for `test` trust without a timestamp URL), then verifies:

- `publicCA`: `signtool verify /pa /tw <file>` exits 0 and `Get-AuthenticodeSignature` reports `Valid`;
- `test`: `Get-AuthenticodeSignature` reports `Valid`, or `UnknownError` whose status message names an untrusted root, and in both cases `SignerCertificate.Thumbprint` equals the profile's;
- in both, a copy of the signed file with one byte of its code section flipped reports `HashMismatch` (the check that the signature covers the image).

Signing happens after symbol collection, because signing changes the PE's bytes but not its CodeView record (the RSDS GUID and the PDB match are unaffected; `Pwsh_Ship_SignedExeKeepsBuildId`).

### Android APK signing

`Sign-Zenith2Android.ps1 -Apk <unsigned.apk> -Out <signed.apk> -Profile <profile>`:

1. `zipalign -P 16 -f 4 <unsigned> <aligned>` (the pinned build-tools; 16 KB page alignment for uncompressed native libraries, [Z2-N-ARCH-235]).
2. `apksigner sign --ks <releaseKeystore> --ks-key-alias <releaseKeyAlias> --ks-pass env:<releasePasswordEnv> --v1-signing-enabled false --v2-signing-enabled true --v3-signing-enabled true --min-sdk-version 34 --out <signed> <aligned>` (v1 is unnecessary at minSdk 34).
3. Verify: `apksigner verify --verbose --print-certs --min-sdk-version 34 <signed>` reports `Verified using v2 scheme (APK Signature Scheme v2): true` and the v3 line `true`, its `Signer #1 certificate SHA-256 digest` equals `releaseCertSha256`, and its `Signer #1 key algorithm` is `RSA`; `zipalign -c -P 16 -v 4 <signed>` exits 0.

The release key is RSA (3072 bits or more) because APK Signature Scheme v2 and v3 signatures with RSA PKCS#1 v1.5 are deterministic: the same aligned input and key give the same signed bytes, whereas an ECDSA signature is randomised. The signed APK is still listed as run-dependent in `release.json` (§ "Field rules"), so the reproducibility claim never rests on it. The Gradle `shipping` build type carries no `signingConfig` (cross-area row SH3 to MOD), so the release key never reaches Gradle.

### App Bundle upload signing

`Sign-Zenith2Android.ps1 -Bundle <unsigned.aab> -Out <signed.aab> -Profile <profile>` signs PACK's bundle with the pinned JDK's `jarsigner -keystore <uploadKeystore> -storetype PKCS12 -storepass:env <uploadPasswordEnv> -sigalg SHA256withRSA -digestalg SHA-256 <aab> <uploadKeyAlias>` and verifies with `jarsigner -verify -strict -verbose -certs <aab>` (exit 0, every entry signed, the signer's SHA-256 fingerprint equal to `uploadCertSha256`). `jarsigner` writes a signing-time attribute, so a signed bundle is run-dependent. Google Play re-signs delivered APKs with its app signing key; the upload key only authenticates the upload.

### Update manifest signing hand-off

`update.zman` (format: PACK; verification: PLAT and SEC; [Z2-N-ARCH-955]) is signed by PACK's update-manifest writer. The writer never opens the key file: it calls

```powershell
Get-Zenith2UpdateSigningKey -Profile <profile object>   # exported by Zenith2/Tools/Setup/Zenith2Release.psm1
```

which reads `updateManifest.encryptedPrivateKeyFile`, reads the password from the environment variable `updateManifest.passwordEnv` (failing, exit code 4 of the calling script, `update signing key: environment variable <NAME> is not set`, when it is absent), decrypts the PKCS#8 structure with `System.Security.Cryptography.Pkcs.Pkcs8PrivateKeyInfo.DecryptAndDecode` (PowerShell 7's .NET, assembly `System.Security.Cryptography.Pkcs`, which PowerShell 7 ships), checks that its `AlgorithmId` is Ed25519 (OID `1.3.101.112`) and that its `PrivateKeyBytes` are the RFC 8410 `CurvePrivateKey` encoding, an OCTET STRING of 32 bytes (`04 20` followed by the seed), then proves that the seed and `updateManifest.publicKey` belong together with the pinned JDK (below), and returns the seed as a `byte[]`, which the caller zeroes (`[Array]::Clear`) after signing.

.NET has no Ed25519 implementation, so the public-key check is made by the pinned JDK (toolchain key `jdk`, JDK 17, whose `SunEC` provider implements Ed25519 since JDK 15), which every Windows release machine has because `Setup-Zenith2.ps1` installs it for the Android build. The check is the committed single-file Java program `Zenith2/Tools/Setup/Ed25519PublicKeyCheck.java`, run by the JDK's source launcher, never compiled by CMake and never part of a target:

```text
<Get-Zenith2ToolPath -Tool java> -Xshare:off Zenith2/Tools/Setup/Ed25519PublicKeyCheck.java <publicKey base64>
	stdin:  the 32-byte seed as 64 lower-case hexadecimal digits and one LF (never on the command line)
	stdout: one line, "match" or "mismatch"
	exit:   0 match · 1 mismatch · 2 malformed input (a seed that is not 64 lower-case hex digits, a key that does not decode to 32 bytes)
```

Its algorithm, using only the standard `java.security` API:

1. Read the seed line from standard input; parse it into a 32-byte array; decode the argument with `java.util.Base64.getDecoder()`; either malformed is exit 2.
2. `KeyFactory xFactory = KeyFactory.getInstance("Ed25519")`; the private key is `xFactory.generatePrivate(new EdECPrivateKeySpec(NamedParameterSpec.ED25519, seed))`; the public key is `xFactory.generatePublic(new X509EncodedKeySpec(spki))`, where `spki` is the 12-byte SubjectPublicKeyInfo prefix `30 2a 30 05 06 03 2b 65 70 03 21 00` followed by the 32 public-key bytes.
3. Sign the fixed message `zenith2 update key check` (UTF-8) with `Signature.getInstance("Ed25519")` and the private key, and verify the signature with the public key. Ed25519 signing is deterministic (RFC 8032), and verification succeeds exactly when the public key is the one the seed derives, so `true` prints `match` and exits 0, `false` prints `mismatch` and exits 1.
4. Fill the seed array with zeros before exiting.

`Get-Zenith2UpdateSigningKey` fails (`update signing key: the key file does not match updateManifest.publicKey`, exit code 4 of the calling script) on exit 1 and passes any exit 2 message through. SHIP's verification also checks that the profile's `publicKey` (whose SHA-256 is `publicKeySha256`) is the key compiled into the release's Player (W10), so a release is never built against a key whose private half the operator does not hold. The same JDK's `Signature` API is the Ed25519 signer available to PACK's update-manifest writer, which cannot sign in PowerShell either (cross-area row SH5 (c)).

The tests use RFC 8032's first test vector (§ 7.1, "TEST 1"), copied by hand from the RFC, never computed: seed `9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60`, public key `d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a` (`Zenith2/Docs/Design/SHIP/Tests.md` → "PowerShell tests": `Pwsh_Ship_Ed25519PublicKeyCheck_Rfc8032`, `Pwsh_Ship_UpdateSigningKey_Decrypts`).

## Release manifest

### release.json schema

```json
{
	"schema": "zenith2.release",
	"schemaVersion": 1,
	"game": "OpenWorldSlice",
	"displayName": "Open World Slice",
	"version": "1.2.0",
	"releaseLine": "1.2",
	"channel": "candidate",
	"configuration": "Shipping",
	"source": { "commit": "<40 hex>", "treeSha": "<40 hex>", "fixVersion": "Z2 Stage 6" },
	"manifest": { "manifestSha256": "<64 hex>", "toolchainHash": "<64 hex>", "dependenciesHash": "<64 hex>" },
	"composition": { "compositionHash": { "android": "<16 hex>", "windows": "<16 hex>" }, "compositionSha256": "<64 hex of composition.json>" },
	"engineCookVersion": 12,
	"contentHash": { "android": "<16 hex>", "windows": "<16 hex>" },
	"platforms": [
		{
			"platform": "windows",
			"preset": "win",
			"buildId": "<32 hex: the RSDS GUID bytes>",
			"files": [
				{ "path": "archives/OpenWorldSlice-1.2.0-windows-x64.zip", "sha256": "<64 hex>", "bytes": 98765432, "kind": "archive", "runDependent": true, "unsignedSha256": null },
				{ "path": "windows/OpenWorldSlice/OpenWorldSlice.exe", "sha256": "<64 hex>", "bytes": 12345678, "kind": "executable", "runDependent": true, "unsignedSha256": "<64 hex>" }
			],
			"signing": { "kind": "authenticode", "trust": "publicCA", "signerSha1": "<40 hex>", "timestamped": true },
			"symbols": [ { "binary": "OpenWorldSlice.exe", "kind": "pdb", "pdbName": "OpenWorldSlice.pdb", "pdbGuid": "<32 hex>", "pdbAge": 1, "storePath": "OpenWorldSlice.pdb/<GUID><AGE>/OpenWorldSlice.pdb", "sha256": "<64 hex>" } ],
			"licences": { "path": "windows/OpenWorldSlice/Licences/licences.json", "sha256": "<64 hex>", "components": 17 }
		}
	],
	"files": [
		{ "path": "archives/OpenWorldSlice-1.2.0-symbols.zip", "sha256": "<64 hex>", "bytes": 456789012, "kind": "archive", "runDependent": true, "unsignedSha256": null }
	]
}
```

### Field rules

| Field | Rule |
|---|---|
| `version`, `releaseLine` | `game.json` `version`; `releaseLine` is its `MAJOR.MINOR`, the value PLAT's mount manifest and the patch rules use ([Z2-N-CONTRACT-528]) |
| `channel` | `internal`, `candidate` or `public` |
| `source.commit` | `git rev-parse HEAD`; `treeSha` is `git rev-parse HEAD^{tree}`; the tree is clean (state PRECONDITIONS) |
| `manifest.*` | `Get-Zenith2ManifestHashes` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Canonicalisation and hashing") |
| `composition` | read from `${ZENITH_OUT}/Games/<Game>/composition.json` (`Zenith2/Docs/Design/MOD.md` → "The composition.json record") |
| `engineCookVersion` | `uZENITH_ENGINE_COOK_VERSION`, read from the cooked catalogue header of the package (ASSET's offset-8 field) |
| `contentHash` | the mount manifest's `contentHash` per platform |
| `platforms[].buildId` | the 16-byte build ID `Zenith_GetRunningBuildID()` returns for the executable (§ "Symbol identities"), 32 lower-case hex digits; equals the mount manifest's `buildId` (W9); a property of the unsigned image, so signing never changes it |
| `platforms[].files[]` | every file of the staging layout that belongs to that platform — its package tree, its APK and AAB, its `symbols/<platform>/` files and its platform archive — byte-ordered by `path`; `kind` is `executable`, `shared-object`, `redistributable`, `content`, `licence`, `apk`, `aab`, `symbols`, `archive` |
| `files[]` (top level) | every file of the staging layout that belongs to no one platform: today exactly `archives/<Game>-<version>-symbols.zip`, which holds both platforms' `symbols/`; same record shape and order; together with every `platforms[].files[]` it lists every file of the layout except `state.json`, `release.json` and `reports/` (W1) |
| `files[].unsignedSha256` | for a file the SIGN state signed, its SHA-256 before signing (from `reports/unsigned.json`); `null` for every other file |
| `files[].runDependent` | `true` for every signed file, for every MSVC PDB (`kind: symbols`; `/Brepro` makes the PE a function of its inputs but MSVC does not promise byte-identical PDBs), and for every archive that contains a run-dependent file (the Windows package archive, the symbols archive); `false` otherwise, which includes the unstripped and the stripped Android shared objects and `native-debug-symbols.zip`, covered by § "Reproducibility definition" |
| `signing` | `authenticode` (Windows), `apk-v2v3` and `aab-jar` (Android), or `{ "kind": "unsigned" }` |
| `symbols[]` | one per retained debug file (§ "Symbol identities") |

Every key is present (a value that does not apply is `null`, never absent), keys appear in the order shown, and `release.json` is written by `Write-Zenith2Json`; it contains no timestamp and no verification result (those are `reports/verify.json` and `reports/reproducibility.json`).

**What is reproducible.** An Authenticode signature with an RFC 3161 timestamp embeds the timestamp token in the signed PE, and `jarsigner` records a signing time, so the bytes of a signed file — and of an archive containing one — differ between two signings of the same input. These, with the MSVC PDBs and every archive containing one of them, are exactly the `runDependent` files. The **reproducible form** of `release.json` is its canonical JSON (`Get-Zenith2CanonicalJson`) with `sha256` and `bytes` set to `null` on every `runDependent` file (platform and top-level lists alike), `symbols[].sha256` of every PDB set to `null`, and `signing.timestamped`, `signing.signerSha1` set to `null`; its SHA-256 is `reproducibleSha256`. Two runs of the pipeline over the same tree, toolchain and channel produce the same `reproducibleSha256`, and every signed file's `unsignedSha256` equals across them (each is a function of the reproducible build, § "Reproducibility definition"); `Pwsh_Ship_ReleaseManifest_TwoSigningsSameReproducibleDigest` signs one input twice and asserts both. PACK's patch builder reads two `release.json` files (the installed and the new release) to compute a patch (cross-area row SH5 (d)).

### Release archive index

The manifest's own integrity is its SHA-256, recorded in the release archive's index `<releaseArchive>/index.json`: `[{ "game", "version", "channel", "tree12", "releaseJsonSha256", "reproducibleSha256", "verifyJsonSha256", "published" }]`, byte-ordered by `game`, `version`, `channel`; `-Publish` appends one row (`published` is the UTC date of the publication, the one timestamp, which lives outside every release). The release documents are committed by the human to `Zenith2/Docs/Release/<Game>/<version>/` for every `public` release and every `candidate` used for a stage acceptance, as exactly this file set (the one list `Zenith2/Docs/Design/SHIP.md` → "Files owned by SHIP" also shows):

| File | Copied from | Written when |
|---|---|---|
| `ReleaseNotes.md` | `New-Zenith2ReleaseNotes.ps1 -Release <run>/release.json`, completed by the operator (§ "Release notes") | after VERIFY passed |
| `release.json` | `<run>/release.json`, byte copy | after VERIFY passed |
| `licences/<platform>.json`, one per platform of the release | the package's `Licences/licences.json` (Windows) and `assets/licences/licences.json` (Android), byte copies; their SHA-256 equal `release.json`'s `platforms[].licences.sha256` | after VERIFY passed |
| `reports/verify.json`, `reports/reproducibility.json` | `<run>/reports/`, byte copies | after VERIFY passed |
| `Qualification.json` | `Zenith2/Docs/Design/SHIP/Qualification.md` → "Qualification record" | at the stage acceptance that qualifies the release |

`reports/unsigned.json` stays in the release archive and is not committed (`release.json` carries every `unsignedSha256` it records).

## Symbols

### Symbol identities

| Platform | Binary identity | Debug file identity | Read by |
|---|---|---|---|
| Windows | the PE's CodeView RSDS record: GUID (16 bytes), age, PDB file name (`<Game>.pdb`, `/PDBALTPATH:%_PDB%`); and the PE `TimeDateStamp` plus `SizeOfImage` | the PDB's GUID and age (`llvm-pdbutil dump -summary`) | `Get-Zenith2BinaryIdentity` (`llvm-readobj --coff-debug-directory`) |
| Android | the ELF `NT_GNU_BUILD_ID` note, 20 bytes (`-Wl,--build-id=sha1`) | the unstripped object's note (identical bytes) | `Get-Zenith2BinaryIdentity` (`llvm-readelf --notes`) |

<!-- elaboration -->
The 16-byte build ID that SAVE's `Zenith_GetRunningBuildID()` returns (the ZRPL, ZSAV and resume identities) is the RSDS GUID on Windows and the first 16 bytes of the build-ID note on Android (`Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header"); SHIP's tools print it the same way, as the 32 lower-case hex digits of those bytes in file order, so a release manifest, a mount manifest, a replay header and a crash report name one build identically. With `/Brepro` and the reproducibility flags both are functions of the binary's bytes.

### Symbol store layout

```text
<symbolStore>/
	<pdb name>/<GUID uppercase hex, no dashes><age, uppercase hex, no leading zero>/<pdb name>
	<exe name>/<TimeDateStamp, 8 uppercase hex><SizeOfImage, lowercase hex>/<exe name>
	.build-id/<first 2 hex of the 40-hex build ID>/<remaining 38 hex>.debug
	index.json
```

- The Windows tiers are the SymStore layout (so Visual Studio, WinDbg and `llvm-symbolizer --pdb` find files by GUID); the executable is stored too, because minidump symbolisation reads unwind data from the image.
- The Android tier is the `.build-id` layout GDB, LLDB and debuginfod clients use.
- `index.json` lists every stored file: `[{ "path", "sha256", "game", "version", "channel", "tree12", "platform" }]`, byte-ordered by `path`; storing a file whose path exists with a different SHA-256 is refused (`symbol store: <path> exists with different content`), and an identical file is a no-op, so republishing is idempotent.

When D-SEL-CRASH (open, Stage 2) selects Crashpad, a third tier of Breakpad `.sym` files (`<module>/<id>/<module>.sym`, produced by the pinned `dump_syms` the ruling ticket adds to the manifest) joins this layout in the ruling's diff; the custom minidump writer alternative needs no further tier.

### Collecting symbols

1. Windows: copy `${ZENITH_OUT}/win/<Config>/bin/<Game>.pdb` to `symbols/windows/`; check its GUID and age equal the executable's RSDS record; copy the executable's identity into `release.json`.
2. Android: copy the unstripped `${ZENITH_OUT}/Games/<Game>/android-arm64/jni/<Config>/arm64-v8a/lib<Game>.so` (the shipping build type's JNI staging, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Build types and JNI staging") to `symbols/android/arm64-v8a/`; extract the packaged (AGP-stripped) `lib/arm64-v8a/lib<Game>.so` from the signed APK; check both build-ID notes are identical.
3. Check that `llvm-symbolizer` resolves the address of `Zenith_PlayerMain` (read from the debug file's symbol table with `llvm-nm`) to a frame named `Zenith_PlayerMain` with a file under `Zenith2/Player/` (Android: the `Zenith2/` prefix of `-ffile-prefix-map`).

### Play native debug symbols

`symbols/android/native-debug-symbols.zip` holds `arm64-v8a/lib<Game>.so` (the unstripped object, full debug information), built with the deterministic zip rule; the human uploads it with the App Bundle in the Play Console. No tool in Zenith2 uploads anything to a remote service.

### Retention policy

<!-- elaboration -->
- Every published release's symbols are retained in the store for as long as any build of its `releaseLine` can still be installed by a player, and in every case for every `public` release indefinitely.
- `Remove-Zenith2Symbols.ps1 -Release <game>/<version>/<channel>-<tree12>` removes one release's files (and their `index.json` rows) only when its channel is `internal` or `candidate` and no `public` release shares a stored file; for a `public` release it refuses (exit 1, `symbols of public releases are retained`).
- The store is outside every checkout; backing it up is part of the release operator's routine and not a gate.

### Symbolization tools

| Tool | Input | Output |
|---|---|---|
| `Resolve-Zenith2Address.ps1 -Store <dir> -BuildId <hex> -Offset 0x<hex>` (Android) or `-Pdb <name> -Guid <hex> -Age <n> -Rva 0x<hex>` (Windows) | one address | `function file:line` per inlined frame, through `llvm-symbolizer` |
| `Symbolize-Leaks.ps1 -Report <file> -SymbolDir <dir> [-Out <file>]` | a Debug leak report whose frames are `<module>+0x<offset>` lines (Android and Linux, [Z2-N-CONTRACT-258]) | the same report with ` <function> <file>:<line>` appended to each frame line, the module looked up by base name in `-SymbolDir` (a build tree's library directory or a symbol-store `.build-id` root when the report carries build IDs) |

`Symbolize-Leaks.ps1` lives at `Zenith2/Tools/Setup/Symbolize-Leaks.ps1`, the path `Zenith2/Docs/Contracts/Memory.md` names; a frame whose module is not found keeps its line unchanged and the script exits 0 with the count of unresolved frames printed (CORE's CB12).

## Licence inventory

### Inventory selection algorithm

<!-- elaboration -->
Implements [Z2-N-ARCH-125]. The inventory of game `G` on platform `P` in configuration `C` is computed by `cmake -DZENITH_ROOT=<Zenith2 directory> -DZENITH_OUT=<out root> -DZENITH_PRESET=<the preset whose tree holds declared_deps.json> -DZENITH_LICENCE_GAME=G -DZENITH_LICENCE_PLATFORM=P -DZENITH_LICENCE_CONFIG=C -P Zenith2/CMake/ZenithThirdParty.cmake` (every argument required; a missing one is `Zenith2: licence inventory: -D<NAME> is required`, exit 1; the script-mode main runs only under `if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)`, so `RunShipFixture.cmake`'s `include(ZenithThirdParty.cmake)` defines the functions without running it), run by the custom target `<G>_Licences` that `zenith_add_licence_inventory(<G>)` creates (MOD's `zenith_add_game` calls it, cross-area row SH2), writing `${ZENITH_OUT}/Games/<G>/<preset>/licences/<Config>/` and, on Android, the same files under `licence-assets/<Config>/licences/`:

1. Read `declared_deps.json`, `third_party_targets.json` and the manifest.
2. Roots: `ZenithPlayer`, `ZenithRuntime`, the libraries of `G`'s resolved modules and backends for `P` (the `games[]` record and the libraries' `moduleName`, `backendOf`, `backendName`), both compression backends, and the RHI backends of `P`.
3. Closure: the transitive `depends` of the roots over `libraries[]`.
4. Components: for every library `L` in the closure and every entry named in `L`'s `thirdParty`, the entry is a component when at least one of its created targets (from `third_party_targets.json`) has `owner == L` and `usage == runtime`, unless the entry has `shipping: excluded` and `C` is `Shipping`; then add every `dependsOn` entry of a component, transitively.
5. Toolchain runtime components: every `runtimeComponents` element of `P` (on Windows the `app-local-dll` elements are listed whether or not the packaging step copies them, because every MSVC `MultiThreadedDLL` executable imports `vcruntime140.dll`).
6. Content components: the third-party content records of `G`'s cooked catalogue (§ "Content licences"). Gradle-resolved components, on Android only: every row of `Zenith2/ThirdParty/gradle/maven-artefacts.json` whose `selectedBy` names a library of the closure (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Maven artefact rows"), as `kind: gradle-artefact` with `name` the row's `<group>:<artefact>` and `version` its version; checklist item A14 confirms the release resolved exactly these.
7. Refuse (exit 1, `<Game>_Licences` fails the build) when any component has an empty licence expression, a licence not allowed by `licence_policy.json` under the list that governs it (`allowedRuntime` for runtime entries, Gradle-resolved components and toolchain runtime components, `allowedContent` for content components; `Zenith2/Docs/Design/SHIP/Manifest.md` → "Licence fields"), or a `licenceFile` whose SHA-256 differs from `licenceSha256` ("a dependency whose licence the inventory cannot state blocks packaging", [Z2-N-ARCH-125]).
8. Write the three outputs below; components are ordered by `name` compared case-insensitively, ties by byte order.

Tools-only (`ZenithCook`'s importers, ImGui, Slang, DXC) and test entries (doctest, WARP, lavapipe) never appear in a shipped inventory; they are listed in the repository inventory `${ZENITH_OUT}/licences/repository.json`, written by the same script with `-DZENITH_LICENCE_GAME=*` and covering every entry and toolchain pin, which the release documentation's "Third-party components" section cites.

### THIRD_PARTY_NOTICES.txt format

UTF-8 without BOM, LF line endings:

```text
<displayName> <version> — third-party notices
This product includes the following third-party components.

===============================================================================
<name> <version>
SPDX-License-Identifier: <licence expression>
Elected: <licenceElected>                    (present only when the expression contains OR)
Source: <source url or toolchain archive url>
Used by: <owner libraries, comma-separated, byte-ordered>

<the licence file's text, verbatim>

===============================================================================
<next component>
```

The separator line is 79 `=` characters. A component with `pins.additionalLicenceFiles` appends each additional text after a line `--- <path> ---`. The file ends with one LF.

### licences.json format

```json
{
	"schema": "zenith2.licence-inventory",
	"schemaVersion": 1,
	"game": "OpenWorldSlice",
	"platform": "windows",
	"configuration": "Shipping",
	"components": [
		{ "name": "sdl3", "version": "3.2.x", "kind": "dependency", "licence": "Zlib", "licenceElected": null, "licenceFile": "Licences/sdl3.txt", "licenceSha256": "<64 hex>", "sourceUrl": "<url>", "sourceSha256": "<64 hex>", "owners": ["ZenithPlatform"] }
	]
}
```

`kind` is `dependency`, `toolchain-runtime`, `gradle-artefact` or `content` (a `gradle-artefact` component's `sourceUrl` is `maven:<coordinate>` and its `sourceSha256` the artefact's SHA-256 in `verification-metadata.xml`); `Licences/<name>.txt` is a byte copy of the licence file (named by the component's `name`, with characters outside `[A-Za-z0-9._-]` replaced by `_`).

### Content licences

<!-- assumption: pending CONTENT -->
Third-party content (fonts, textures, audio under CC-BY, OFL or similar) carries its licence in the origin metadata whose schema CONTENT owns (`Zenith2/Docs/Design/ASSET.md` → "Purpose and scope" names the `.meta` `provenance` member ASSET carries unchanged). SHIP assumes that the cook writes, per game and platform, `${ZENITH_OUT}/Cooked/<Game>/<platform>/provenance-licences.json` = `[{ "asset", "name", "licence", "licenceText", "attribution" }]` for every cooked asset whose provenance names a licence other than the project's own, and lists each as a `content` component (cross-area row SH9).
<!-- /assumption -->

## Reproducible builds

### Reproducibility definition

<!-- elaboration -->
Z2-I-SHIP-01's "reproducible builds" has two parts:

1. **Reproducible toolchain and inputs** (Stage 0 and Stage 2): every input of a build is pinned by `ThirdParty/manifest.json` and provisioned by `Setup-Zenith2.ps1` on a clean machine ([Z2-N-ARCH-195]); every C and C++ dependency builds from its pinned source in the sanitizer presets ([Z2-N-ARCH-201]).
2. **Reproducible outputs** (the flags from Stage 1, the check at Stage 6): two clean builds of the same tree at the same checkout path with the same pinned toolchain produce byte-identical unsigned game executables (Windows, `Shipping` and `Release`) and byte-identical unstripped and stripped shared objects (Android), and therefore identical build IDs (signed outputs are compared through `unsignedSha256` and `reproducibleSha256`, § "Field rules"); builds of the same tree at two different checkout paths produce byte-identical Android shared objects (the clang prefix maps remove both paths). For Windows executables built at two different checkout paths the comparison is **report-only**: MSVC embeds source paths that `/Brepro` does not remove, and paths of different lengths shift every following offset, so no byte-level rule can separate a path difference from a real one; the check reports the differing files, the first differing offset and the differing strings, and its `result` for those files is `reported`, which never fails the run.

### Test-Zenith2Reproducibility

```text
pwsh -NoProfile -File Zenith2/Tools/Setup/Test-Zenith2Reproducibility.ps1
	-Game <Name> [-Configuration Shipping|Release] [-Platforms windows,android]
	[-Mode Rebuild|CrossCheckout] [-OtherRoot <checkout path>] [-Report <path>]
```

- `Rebuild`: with `SCCACHE_DISABLE=1` in the environment, delete `${ZENITH_OUT}/win`, `${ZENITH_OUT}/android-arm64`, `${ZENITH_OUT}/generated` and `${ZENITH_OUT}/Games/<Game>`; configure and build the game; copy the outputs aside; repeat the deletion and the build; compare byte for byte.
- `CrossCheckout`: build the game in this checkout and in `-OtherRoot` (another registered checkout or lane at the same commit, prepared by the human), and compare: Android shared objects byte for byte, Windows executables report-only (§ "Reproducibility definition").
- The report `{ "mode", "result": "identical"|"differs"|"reported", "files": [ { "path", "sha256A", "sha256B", "firstDifferingOffset", "differingStrings": [ … ] } ] }` lists every compared file; exit 0 when the result meets the definition above, 1 otherwise.
- The check is human-run (it needs two clean builds, longer than a gate phase) in the Stage 6 release pipeline and the Stage 6 acceptance; `Pwsh_Ship_ReproducibilityCompare_*` tests its comparison logic on generated files in the Test step.

## Release documentation

### Release notes

<!-- elaboration -->
Every `public` release, and every `candidate` used for a stage acceptance, has `Zenith2/Docs/Release/<Game>/<version>/ReleaseNotes.md`, created from the generated skeleton `New-Zenith2ReleaseNotes.ps1 -Release <release.json>`, with exactly these `##` sections in order:

| Section | Content | Generated part |
|---|---|---|
| `## Summary` | what the release is, its channel and release line | the identity line (game, version, channel, commit) |
| `## Platforms and builds` | per platform: the package, its build ID, signing, the device-matrix support class (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Devices") | the table from `release.json` |
| `## System requirements` | the Windows and Android requirements derived from the device matrix (Windows 11, a Vulkan 1.3 or D3D12 SM 6.6 GPU; Android 14, arm64, the S24 Ultra validated) | the table |
| `## Changes` | user-visible changes since the previous release of the release line | the commit list `git log --format="%h %s" <previous commit>..<commit> -- Zenith2/` |
| `## Fixed defects` | the BUG tickets closed since the previous release | — |
| `## Known issues` | open defects that ship (§ "Known issues") | — |
| `## Compatibility` | save compatibility, cooked-content and patch compatibility (one release line), upgrade guide link | the save and engine cook versions from the build |
| `## Third-party components` | the components and licences of `licences.json` | the table |
| `## Verification` | the qualification record's result and the evidence paths | from `reports/verify.json` |

The other committed files of the release are listed in § "Release archive index"; the release notes are written after VERIFY passed and linted by the operator with `Test-Zenith2ReleaseDocs.ps1 -Full` before the commit (§ "Release documentation lint"), never by a state of the pipeline.

### Known issues

`## Known issues` holds one table `| ID | Severity | Platform | Summary | Workaround |`, one row per open BUG ticket of the game's slices at release time whose severity is S2 or lower (an open S1 blocks the release through the bug bar, Z2-B-SHIP-019); an empty table is written as the single row `| none | — | — | — | — |`.

### Upgrade guides

<!-- elaboration -->
Z2-I-SHIP-13. Every fix version from `Z2 Stage 2` to `Z2 Stage 6` has `Zenith2/Docs/Release/UpgradeGuides/Z2-Stage-<N-1>-to-Z2-Stage-<N>.md`, written for a person maintaining a game under `Zenith2/Games/` across the fix version, created from `New-Zenith2UpgradeGuide.ps1 -From <commit> -To <commit> -FixVersion "Z2 Stage <N>"`, where the two commits are the ones the stage acceptance records of `Z2 Stage <N-1>` and `Z2 Stage <N>` name. The generator lands with the Stage 6 release documentation, and the Stage 6 acceptance (a `needs-human` ticket run by the release operator) writes all five guides, Stage 1 to 2 through Stage 5 to 6, from those recorded commits; no earlier stage schedules a guide, and every subsequent fix version's guide is written at that version's own stage acceptance. Each guide has exactly these `##` sections:

| Section | Generated from |
|---|---|
| `## Summary` | — |
| `## Removed APIs` | C++ declarations carrying `[[deprecated(` at `-From` and absent at `-To` (a diff scan of `Zenith2/Engine/**/*.h` and `Zenith2/Player/*.h`) |
| `## Deprecated APIs` | `[[deprecated("use <Replacement>; removed in Z2 Stage <N+1>")]]` added between the two commits, with the replacement and removal stage parsed from the message ([Z2-N-CONTRACT-522]) |
| `## Luau API changes` | the Luau-facing deprecations and removals (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", table row "Luau-facing API"), from the binding-set diff SCRIPT's deprecation diagnostics enumerate (cross-area row SH9) |
| `## game.json changes` | changes to `Zenith2/Docs/Architecture/BuildSystem.md`'s `game.json` table between the commits, and any `schemaVersion` change with its `ZenithNewGame --migrate` step |
| `## Authoring document migrations` | migration fixtures added under `**/Tests/Fixtures/Migration/<Type>/v<N>.json` ([Z2-N-CONTRACT-548]): each new version of each type |
| `## Cooked data` | a change of `uZENITH_ENGINE_COOK_VERSION` (every game re-cooks, [Z2-N-CONTRACT-529]) |
| `## Saves` | save format version changes (SAVE's version constants) and the migration steps added |
| `## Dependencies` | manifest `dependencies` entries added, removed or changed (`name old → new`), with licence changes |
| `## Toolchain` | `toolchain` pin changes; whether `Setup-Zenith2.ps1` must be re-run |
| `## Steps` | the ordered steps a game maintainer performs; always ends with "run `Setup-Zenith2.ps1 -Check`, regenerate, re-cook, run the game's tests" |

The generator writes the generated parts and marks each human-written part with the single line `Written by the release operator.`; the lint below refuses a published guide that still contains that line.

### Release documentation lint

`Test-Zenith2ReleaseDocs.ps1 -Path <file-or-directory> [-Full]` checks every `ReleaseNotes.md` and upgrade guide under the path:

1. the `##` sections exactly as the tables above, in order;
2. no line matching the placeholder regular expressions `T[B]D` or `T[O]DO`, and no `Written by the release operator.` line;
3. release notes: the committed file set of § "Release archive index" is complete beside them (`release.json`, one `licences/<platform>.json` per `release.json` platform, `reports/verify.json`, `reports/reproducibility.json`); `release.json`'s `version` and `game` match the directory; each `licences/<platform>.json` has the SHA-256 that `release.json` records for that platform's `licences.sha256`; and every component name of every `licences/<platform>.json` appears in `## Third-party components`;
4. with `-Full` only, upgrade guides: every item the generator would list for the recorded commits appears in its section (the guide carries the two commits in a first line `From <commit> to <commit>`). Item 4 needs both commits and the history between them; `-Full` first checks `git cat-file -e <commit>^{commit}` for each and fails loudly (`<guide>: commit <c> is not reachable; run with a full clone (git fetch --unshallow)`, exit 1) when either is missing, never skipping silently.

Items 1 to 3 read only files of the checkout, so they work on any checkout, including CI's `fetch-depth: 1` clone (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Job steps") and a lane. `Pwsh_Ship_ReleaseDocs_Committed` runs items 1 to 3 over `Zenith2/Docs/Release/` in the Test step, so a committed release document that drifts structurally fails a gate; with no release document committed it checks only the directory's `README.md` (the templates). Item 4 runs where the full history exists: the release operator's checkout, where the operator runs `-Full` over the release's notes and its fix version's guide before committing them (the notes are written from `release.json` after VERIFY, so no pipeline state can lint them), and the Stage 6 acceptance (qualification item S4, `-Full`).

## Compatibility policy demonstration

### Compatibility labels

<!-- elaboration -->
Z2-I-SHIP-11 is demonstrated by tests other areas own, one set per kind of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" ([Z2-N-CONTRACT-551]). The list is data, `Zenith2/Tools/Setup/compatibility_tests.json`, and it names each test by the name CTest knows: a doctest case by its doctest name `"<Library>/<Type>/<Behaviour>"` (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", table "Test naming"), which doctest's discovery registers as the CTest name ([Z2-N-CONTRACT-407]), and a registry entry by its `add_test` name:

```json
{
	"schema": "zenith2.compatibility-tests",
	"schemaVersion": 1,
	"kinds": {
		"source-api": ["CompileFail_Ship_DeprecatedUseIsError"],
		"luau-api": ["ZenithScript/Script/Binding_DeprecatedCallDiagnosed"],
		"modules": [],
		"saves": ["ZenithSave/SaveMigration/FixturePerVersion", "ZenithSave/SaveMigration/OtherMajorRefusedRecoverably", "ZenithSave/SaveMigration/ShippedFilesLoad", "ZenithSave/SaveMigration/StepsRunInOrder"],
		"cooked": ["ZenithAssets/CookedCompat/MismatchedFixturesRefused", "ZenithAssets/CookedFile/VersionMismatchRefused", "ZenithScript/Script/Bytecode_VersionMismatchRefused"],
		"authoring": ["ZenithCook/MetaMigration/Fixtures", "ZenithDocument/DocumentMigrator/Apply_BacksUpBeforeWriting", "ZenithDocument/DocumentMigrator/Plan_WritesNothing", "ZenithDocument/DocumentMigrator/UnknownFields_WarnOncePerPathAndRoundTrip", "ZenithDocument/JournaledWrite/InterruptedAtEachStep_Recovers", "ZenithScene/SceneDocumentMigration/Fixtures", "ZenithScene/SceneDocumentMigration/RenameRewritesOverrides"]
	}
}
```

Each doctest name is derived from the test ID by the table's one-to-one rule ([Z2-N-CONTRACT-411]): `<Type>` is the ID's first token after `Test_`, `<Behaviour>` the rest, and `<Library>` is the CMake target whose `Tests/` holds the case's file **in the ticket that registers the test**; the owner's design gives `<Library>` only where no ticket registering the test exists yet. A ticket can move a test between libraries in a same-diff design update (Z2T-SCRIPT-027 moves `Test_Script_Bytecode_VersionMismatchRefused` from the cook tests into `ZenithScript_Tests`), so the ticket, not the design, is what the built CTest name follows. The table below records the derivation with the registering Draft-ID, so the list is checked against the tickets and designs, not against memory:

| Kind | Test ID | `<Library>` from the file's directory | Registered by |
|---|---|---|---|
| source-api | `CompileFail_Ship_DeprecatedUseIsError` (SHIP's registry entry, not a doctest case) | — | SHIP's Stage 6 compatibility ticket (its own registry entry) |
| luau-api | `Test_Script_Binding_DeprecatedCallDiagnosed` (SCRIPT, cross-area row SH14) | `ZenithScript`, the library SH14 asks the test into; the Stage 6 ticket takes the name SCRIPT's ticket registers | the SCRIPT ticket SH14 asks for (no Draft-ID yet; the design is the source until it exists) |
| saves | `Test_SaveMigration_FixturePerVersion`, `_OtherMajorRefusedRecoverably`, `_ShippedFilesLoad`, `_StepsRunInOrder` | `ZenithSave` (`Zenith2/Engine/ZenithSave/Tests/Zenith_SaveMigration_Tests.cpp`, `Zenith2/Docs/Design/SAVE/Tests.md`) | no SAVE ticket yet: SAVE's design |
| cooked | `Test_CookedFile_VersionMismatchRefused`, `Test_CookedCompat_MismatchedFixturesRefused` | `ZenithAssets` (`Zenith_CookedFile_Tests.cpp`, `Zenith_AssetFailure_Tests.cpp` under `Zenith2/Engine/ZenithAssets/Tests/`, `Zenith2/Docs/Design/ASSET/Tests.md`) | Z2T-ASSET-014; Z2T-ASSET-060 |
| cooked | `Test_Script_Bytecode_VersionMismatchRefused` | `ZenithScript` (`Zenith2/Engine/ZenithScript/Tests/Zenith_ScriptModule_Tests.cpp`, the file Z2T-SCRIPT-027 extends; `Zenith2/Docs/Design/SCRIPT/Tests.md` still names `ZenithScriptCompiler/Tests/Zenith_ScriptCook_Tests.cpp` until that ticket's same-diff design update, cross-area row SH22) | Z2T-SCRIPT-027 |
| authoring | `Test_JournaledWrite_InterruptedAtEachStep_Recovers`, `Test_DocumentMigrator_UnknownFields_WarnOncePerPathAndRoundTrip`, `_Apply_BacksUpBeforeWriting`, `_Plan_WritesNothing` | `ZenithDocument` (`Zenith2/Docs/Design/SCENE/Tests.md` → "Test files") | Z2T-SCENE-018; Z2T-SCENE-014; Z2T-SCENE-017; Z2T-SCENE-016 |
| authoring | `Test_SceneDocumentMigration_Fixtures`, `_RenameRewritesOverrides` | `ZenithScene` (`Zenith_SceneDocumentMigration_Tests.cpp`) | Z2T-SCENE-040; Z2T-SCENE-084 |
| authoring | `Test_MetaMigration_Fixtures` | `ZenithCook` (`Zenith2/Tools/ZenithCook/Tests/Database/Zenith_MetaMigration_Tests.cpp`) | Z2T-ASSET-037 |

- Each kind's list is byte-ordered. `modules` is empty by policy: modules are statically linked, so no binary compatibility surface exists (Z2-D-024, Z2-D-048). Every other kind is non-empty.
- `CompileFail_Ship_DeprecatedUseIsError` is SHIP's own compile-fail test: `Zenith2/CMake/Tests/Ship/DeprecatedUse_CompileFail.cpp.in` (outside every library directory, so no library partition claims it; it includes only `ZenithCore` headers) declares a function `[[deprecated("use Replacement; removed in Z2 Stage 7")]] inline void Zenith_ShipDeprecatedFixture() {}` and calls it; registered through CORE's `zenith_core_compile_fail(CompileFail_Ship_DeprecatedUseIsError CMake/Tests/Ship/DeprecatedUse_CompileFail.cpp.in EXPECT "C4996|deprecated")` (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Compile-fail registration"), so a warnings-as-errors build of a call to a deprecated declaration fails ([Z2-N-CONTRACT-523]).
- The list carries only names some area's ticket registers. `ZenithScript/Script/Binding_DeprecatedCallDiagnosed` (the Luau-facing deprecation diagnostic of Contracts' compatibility table) is SCRIPT's to own and implement (cross-area row SH14, which supersedes SH9 (b) for the test); the SHIP Stage 6 ticket that lands `compatibility_tests.json` and `Pwsh_Ship_CompatibilityKindsCovered` is `blocked_by` the SCRIPT ticket that registers that test and writes the doctest name that ticket registers, so the check is green from the diff that adds it, and no gate is ever red by design.

### Demonstration suite

- `Pwsh_Ship_CompatibilityKindsCovered` (Test step, `win`): reads `compatibility_tests.json` and `ctest --show-only=json-v1 -C Debug` of the `win` tree, and fails naming every listed test that is not registered (a renamed or deleted test breaks the demonstration at once, not at Stage 6) and every kind other than `modules` whose list is empty. A listed name `<n>` is satisfied by exactly one of: (a) a registered CTest named `<n>`; (b) registered CTests named `<n>[<Backend>]` for a backend-parameterised case (the suffix form of `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Conformance executables", `[` followed by a backend name and `]`); (c) a registered suite entry `<Exe>[<suite>]` of [Z2-N-ARCH-257] whose command is `<exe> --test-suite=<suite>` and for which `<exe> --list-test-cases --test-suite=<suite> --no-version --no-colors=true` lists `<n>` (SAVE's `z2-save-cooked` cases `…/FixturePerVersion` and `…/ShippedFilesLoad` run inside `ZenithSave_Tests[z2-save-cooked]`, `Zenith2/Docs/Design/SAVE/Tests.md`). Every match is reported, and the check writes the resolved CTest names, one per line and byte-ordered, to `${ZENITH_OUT}/compatibility/run-list.txt`.
- The Stage 6 acceptance runs `ctest -C Release -R "^(<every name of run-list.txt, regex-escaped, |-joined>)$"` in the `win` tree and the same `-R` with `ctest -C Sanitize` in the `win-asan` tree, whose only configuration is `Sanitize` (BuildSystem's presets: `win-asan`'s `CMAKE_CONFIGURATION_TYPES` is `Sanitize`), the trees where each name is registered (`ZenithCook` tests exist in the `win` tree's tool presets alone), and records the result in the qualification record (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Security and compatibility items").
- Saves of every shipped version of the current major release load (the "saves" set) with the fixtures of the shipped release line, which SAVE's tests read from its committed `Tests/Fixtures/Migration/` (Q55: test-time generation for everything else).
