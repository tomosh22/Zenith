# PACK design: packaging tools, App Bundles and update builders

This file is part of the PACK design family; the entry document is `Zenith2/Docs/Design/PACK.md`. It specifies the tool side of packaging: ZenithCook's package modes (PACK's `Zenith2/Tools/ZenithCook/Internal/Package/` subdirectory), the shipping cook profile, the archive and manifest writers, the CMake targets of `Zenith2/CMake/ZenithPackage.cmake`, the App Bundle build and its asset-pack modules, the Java bridge, bundletool, the patch and DLC builders with their Ed25519 signing hand-off, and the Windows install conventions. Formats are `Zenith2/Docs/Design/PACK/Formats.md`; algorithms are `Zenith2/Docs/Design/PACK/Algorithms.md`.

## Scope of this file

| Component | Lives in | Heading |
|---|---|---|
| package, patch and verify modes of ZenithCook | `Zenith2/Tools/ZenithCook/Internal/Package/` | "ZenithCook package modes" |
| the shipping cook profile and `pack.dictionaries` | the same | "Shipping cook profile" |
| archive writer and read-back | the same | "Archive writer", "Archive read-back" |
| `package.zpkm` and `mount-manifest.json` writers | the same | "Manifest writers" |
| `update.zman` and `dlc.zman` bodies | the same | "Update manifest writer" |
| signing | `Zenith2/Tools/Package/Ed25519Sign.java`, `Zenith2Package.psm1` | "Ed25519 signer" |
| the patch and DLC builders | `Zenith2/Tools/Package/New-Zenith2Patch.ps1`, `New-Zenith2Dlc.ps1` | "Patch builder", "DLC builder" |
| CMake targets | `Zenith2/CMake/ZenithPackage.cmake` | "CMake targets" |
| the App Bundle | the generated bundle Gradle project | "App Bundle build" |
| the Play Asset Delivery Java bridge | `Zenith2/Engine/ZenithPackage/Internal/Android/Java/org/zenith2/pack/Zenith2AssetPacks.java` | "Java bridge" |
| bundletool | `Zenith2Package.psm1` | "bundletool usage" |
| Windows install conventions | — | "Windows install conventions" |

## Tool-side file layout

| Path | Content |
|---|---|
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageMode.h`, `.cpp` | the three modes' drivers, their option parsing and exit codes |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageRecipeDocument.h`, `.cpp` | the `Zenith.Package.RecipeDocument` type, its canonical schema, `Zenith_RegisterPackageDocumentTypes` |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageRecipe.h`, `.cpp` | the validated in-memory recipe and the pattern matcher |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackagePartition.h`, `.cpp` | "Partition algorithm" of Algorithms.md |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageArchiveWriter.h`, `.cpp` | "Archive writer" |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageManifestWriter.h`, `.cpp` | `package.zpkm` and the shipped `mount-manifest.json` |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_UpdateManifestWriter.h`, `.cpp` | unsigned `update.zman` and `dlc.zman` bodies |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PatchDiff.h`, `.cpp` | "Patch diff" of Algorithms.md |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageSizeReport.h`, `.cpp` | `size-report.json`, `partition.json`, the linker-map parser |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageCompressionPolicy.h`, `.cpp` | the recipe's compression policy lookup the shipping profile uses |
| `Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageDictionaryStep.cpp` | the `pack.dictionaries` cook step |
| `Zenith2/Tools/ZenithCook/Tests/Zenith_Package*_Tests.cpp` | the packer's tests (`ZenithCook_Tests`) |
| `Zenith2/Tools/Package/Zenith2Package.psm1` | the PowerShell functions of "PowerShell functions" |
| `Zenith2/Tools/Package/Invoke-Zenith2Package.ps1` | the `<Game>_Package` command |
| `Zenith2/Tools/Package/New-Zenith2Patch.ps1`, `New-Zenith2Dlc.ps1` | the builders |
| `Zenith2/Tools/Package/Ed25519Sign.java` | the signer, run by the pinned JDK as a single-file source program |
| `Zenith2/Tools/Package/DeviceSpecs/*.json`, `bundle-config.json`, `format-registry.json` | committed text inputs (Formats.md) |
| `Zenith2/Tools/Package/Tests/*.Tests.ps1`, `Zenith2/Tools/Package/Tests/*.cmake` | the PowerShell and script-mode tests |
| `Zenith2/CMake/ZenithPackage.cmake` | `zenith_add_package` (PACK's CMake file inside SHIP's directory, cross-area row PK5) |
| `Zenith2/Player/GradleTemplate/assetpack/build.gradle.kts.in` | the asset-pack module template (MOD's directory, PACK's file content, cross-area row PK4) |

`Zenith2/Tools/Package/` is a scripts directory with no compiled source, owned by PACK (`Zenith2/Docs/Architecture.md → "Repository layout"`).

## ZenithCook package modes

ZenithCook's modes are a closed set (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook command line"`); PACK's three rows join ASSET's table through cross-area row PK6.

| Mode | Command | Writes |
|---|---|---|
| package | `ZenithCook --game <Name> --platform <windows\|android> --out <cooked dir> --package <package dir> --set package.buildId=<32 hex> --set package.configuration=<Debug\|Release\|Shipping>` | the layouts of "Package build steps" under `<package dir>`, `size-report.json`, `partition.json`; never the cooked tree |
| patch | `ZenithCook --package-patch --base <base package dir> --new <new package dir> --out <bundle dir> --set package.patchVersion=<MAJOR.MINOR.PATCH> [--set package.chunkTargetBytes=<n>]` | `patch-NN.zpak`, `update.zman.unsigned`, `patch-report.json` in `<bundle dir>` |
| verify | `ZenithCook --package-verify --bundle <dir> --public-key <64 hex> --build-id <32 hex> --release-line <MAJOR.MINOR> --cook-version <n>` | stdout: one line per check result |

Exit codes of the three modes (the cook's codes 0, 2, 3, 4, 5 and 6 keep their meanings, `Zenith2/Docs/Design/ASSET/Cook.md → "Exit codes"`; 7 is PACK's):

| Code | Meaning |
|---|---|
| 0 | success |
| 2 | bad command line or package key |
| 3 | inputs unusable: the cooked tree's catalogue unreadable, `composition.json` unreadable, the recipe refused, a release mismatch in patch mode |
| 4 | low disk on the output volume (`cook.lowDiskMarginBytes`), nothing published |
| 5 | internal failure |
| 6 | the cooked tree is locked by another ZenithCook process (the cook's output lock, taken shared for reading) |
| 7 | a packaging rule failed: a partition error, an archive or Play limit, a compatibility refusal, a stale or missing bake in a Shipping package (step 1b of "Package build steps"), a verification failure in verify mode; `package-errors.json` names each |

Errors inside the modes are `Zenith_PackageBuildError` (`ZenithCook/Internal/Package/Zenith_PackageMode.h`), mapped to the exit codes:

```cpp
enum Zenith_PackageBuildError : uint8
{
	PACKAGE_BUILD_ERROR_RECIPE_INVALID = 1,          // exit 3
	PACKAGE_BUILD_ERROR_COOKED_TREE_INVALID,         // exit 3
	PACKAGE_BUILD_ERROR_IDENTITY_MISSING,            // exit 2: no package.buildId
	PACKAGE_BUILD_ERROR_DEPENDENCY_IN_DLC,           // exit 7
	PACKAGE_BUILD_ERROR_DLC_CROSS_DEPENDENCY,        // exit 7
	PACKAGE_BUILD_ERROR_DLC_SCHEMA_CHANGE,           // exit 7: a DLC unit would need a schema or module the base lacks
	PACKAGE_BUILD_ERROR_ARCHIVE_LIMIT,               // exit 7
	PACKAGE_BUILD_ERROR_TOO_MANY_ARCHIVES,           // exit 7
	PACKAGE_BUILD_ERROR_READBACK_MISMATCH,           // exit 5
	PACKAGE_BUILD_ERROR_PATCH_INCOMPATIBLE_LINE,     // exit 3
	PACKAGE_BUILD_ERROR_PATCH_EXECUTABLE_CHANGED,    // exit 3
	PACKAGE_BUILD_ERROR_PATCH_COOK_VERSION_CHANGED_IN_LINE,   // exit 3
	PACKAGE_BUILD_ERROR_PATCH_COMPOSITION_CHANGED,   // exit 3
	PACKAGE_BUILD_ERROR_PATCH_NOT_NEWER,             // exit 3
	PACKAGE_BUILD_ERROR_PATCH_EMPTY,                 // exit 7
	PACKAGE_BUILD_ERROR_PATCH_FORBIDDEN_FILE,        // exit 7
	PACKAGE_BUILD_ERROR_VERIFY_FAILED,               // exit 7 (verify mode)
	PACKAGE_BUILD_ERROR_IO,                          // exit 5
	PACKAGE_BUILD_ERROR_LOW_DISK,                    // exit 4
	PACKAGE_BUILD_ERROR_BAKE_STALE,                  // exit 7: a Shipping package over a cooked tree whose report lists a stale or missing bake (step 1b)
};
constexpr const char* Zenith_ToString(Zenith_PackageBuildError eError);
```

### Package build steps

Package mode runs, in order, holding the cooked tree's output lock shared and the package directory's own lock (`<package dir>/.package.lock`, the cook's lock-file format) exclusively:

1. Load and validate the recipe (`Games/<Name>/package.recipe.json` through the framework, or the implicit all-base recipe), `composition.json`, the cooked tree's `assets.zcat` (`Zenith_AssetCatalogue::Load`) and its `mount-manifest.json` (development form, for `game` and `releaseLine`).
1b. The stale-bake check (WORLD's release refusal, cross-area rows WD14 (b), WD28, WD35 and PK26). Read `<cooked dir>/.cook/cook-report.json` (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook report"`) through the framework's JSON reader; a missing or unparsable file, a `schema` other than `"zenith2.cook-report"` or a `schemaVersion` other than 1 is `PACKAGE_BUILD_ERROR_COOKED_TREE_INVALID` (exit 3). A finding is every element of `diagnostics`, whatever its `severity`, whose `code` is `"WORLD_WARNING_BAKE_STALE"` or `"WORLD_WARNING_BAKE_MISSING"` (WORLD's warnings W34, W41 and W35, W40, `Zenith2/Docs/Design/WORLD/PublicAPI.md → "Error and diagnostics types"`), under the contract of row WD35 stated in the assumption block below: the importer reports with a code, and the report lists the persisted diagnostics of the import that produced each source's current intermediates, whether `Import` ran in this cook or was skipped (a CLEAN source or a DDC hit). With `package.configuration` `Shipping`, any finding refuses the package with `PACKAGE_BUILD_ERROR_BAKE_STALE` (exit 7) before `.work/` is created, so every previously published layout stays as it was, and `package-errors.json` holds one line per finding, `package: <path>: <code>: <message>`, where `<path>` is the element's `path` (or `-` when empty). With `Debug` or `Release` each finding is logged as the same line at warning level and the mode continues.
2. Partition (Algorithms.md "Partition algorithm"); on Windows every DLC unit's files go to `DlcBundles/<name>/`; on Android every unit becomes a pack of the bundle layout and every non-DLC unit also joins the sideload layout. The layouts the platform builds are WINDOWS (Windows) and ANDROID_SIDELOAD and ANDROID_BUNDLE (Android).
3. Write the base catalogue and each DLC fragment (Algorithms.md "Partition algorithm" step 5) into a scratch directory `<package dir>/.work/`.
4. Chunk and write every archive of every non-base unit (steps 5's rules), recording each one's bytes and SHA-256; then, per layout, write that layout's `package.zpkm` ("Manifest writers") into `.work/<layout>/Package/package.zpkm` with the base unit's archives recorded as zero bytes and an all-zero digest (Formats.md "Package manifest tables": the manifest cannot record the archive that contains it).
5. Archives are chunked and written by Algorithms.md "Chunking" and "Archive writer", each to `<target>.tmp` then renamed.
6. Per layout, write the base unit's archives including that layout's `package.zpkm` (Android writes two base archive sets, the sideload one and the bundle one, whose entries differ only in `Package/package.zpkm`); their real sizes and digests go to `size-report.json`.
7. Read every archive back ("Archive read-back").
8. Write the mount manifests ("Manifest writers") and validate each with `Zenith_ParseMountManifest`.
9. Warn, never refuse, when a pack's archive bytes exceed an adopted Play pack row (`package: pack <name>: <bytes> archive bytes above the <row> limit <limit>; the bundle measurement decides`), because archive bytes only bound a download from above and the base module's download size does not exist before Gradle and bundletool; the refusal is the bundle-time check of "CMake targets" (`<Name>_Bundle`). A pending row is reported as pending.
10. Write `size-report.json` and `partition.json`; delete `.work/`; publish by renaming each layout directory from `<layout>.new` over the previous one (the previous one is renamed to `<layout>.old` and deleted afterwards), so a package directory always holds a complete layout or the previous one.

| Layout | Directory under `<package dir>` | Content |
|---|---|---|
| Windows | `Content/` | `mount-manifest.json`, every non-DLC archive (the base archives carry the WINDOWS `package.zpkm`) |
| Windows DLC | `DlcBundles/<name>/` | the unit's archives and `dlc.zman.unsigned` |
| Android sideload | `sideload/assets/` | `mount-manifest.json` (no `assetPacks`), every non-DLC archive (the base archives carry the ANDROID_SIDELOAD `package.zpkm`) |
| Android bundle base | `bundle/base/assets/` | `mount-manifest.json` (with `assetPacks`), the base unit's archives (carrying the ANDROID_BUNDLE `package.zpkm`) |
| Android bundle packs | `bundle/packs/<pack>/assets/` | each INSTALL_TIME, FAST_FOLLOW, ON_DEMAND and DLC unit's archives |

<!-- assumption: pending ASSET (row WD35, filed by WORLD) -->
Step 1b assumes ASSET's answer to WORLD's row WD35: (a) `Zenith_ImportContext::Report` gains an overload taking a code name (`Zenith_StringView strCode`, `[A-Z0-9_]{1,64}`), serialized as the report entry's `code` for an import diagnostic, and WORLD's importer reports W34, W35, W40 and W41 through it with their `Zenith_WorldWarningCode` names; (b) every import's diagnostics are stored with its intermediates' DDC entry and its database record, and every cook report's `diagnostics` lists, for every source, those of the import that produced its current intermediates, replayed when `Import` did not run. Without (b) an incremental or shared-DDC cook of a world with stale lighting lists nothing and the check would be fail-open; without (a) no element's `code` names a WORLD warning. If ASSET refuses WD35, the fallback is: for a Shipping package, PACK's `<Name>_ShippingContent` target forces a re-import of every world source before packaging (a forced-re-import cook option PACK then requests from ASSET by its own row, so `Import` runs and reports this cook), and step 1b matches the message prefix `WORLD_WARNING_BAKE_STALE: ` or `WORLD_WARNING_BAKE_MISSING: ` (`Zenith2/Docs/Design/WORLD.md → "Error enumerations"`) instead of `code`; `PACKAGE_BUILD_ERROR_BAKE_STALE`, its test and Z2-R-PACK-186 stand either way.
<!-- /assumption -->

### Shipping cook profile

The cook key `cook.profile` (values `development`, default, and `shipping`) is PACK's step-owned cook key in ASSET's table (cross-area row PK6). With `shipping`:

- every cook step resolving a section's compression for an asset whose `.meta` has no `compression` member asks `Zenith_PackageCompressionPolicy_Resolve(eKind)`, which returns the recipe class's fragment for the kind, else the recipe's top-level `compression`, else COMPRESS's shipping recommendation (zstd 19; `none` for kinds whose payload is already compressed) (`Zenith2/Docs/Design/COMPRESS.md → "Cook integration"`);
- the `pack.dictionaries` step runs (Algorithms.md "Dictionary policy");
- the output tree is `${ZENITH_OUT}/Cooked/<Name>/<platform>-shipping/`, a separate tree from the development one, sharing the DDC (the resolved codec, level and dictionary are cook-key inputs, so the two profiles never share an output).

```cpp
// Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageCompressionPolicy.h
struct Zenith_CompressionSetting;   // COMPRESS's validated fragment value
Zenith_Result<void, Zenith_PackageBuildError> Zenith_PackageCompressionPolicy_Load(const Zenith_PackageRecipe& xRecipe,
	Zenith_PackageCompressionPolicy& xOut);
Zenith_CompressionSetting Zenith_PackageCompressionPolicy_Resolve(const Zenith_PackageCompressionPolicy& xPolicy,
	Zenith_AssetKind eKind);
```

### Archive writer

```cpp
// Zenith2/Tools/ZenithCook/Internal/Package/Zenith_PackageArchiveWriter.h
struct Zenith_PackageArchiveInput
{
	Zenith_StringView m_strPath;            // the entry path inside content:/ ("Objects/0b/0b6e….ztex")
	Zenith_StringView m_strSourceHostPath;  // the cooked file; empty when m_xGeneratedBytes holds the entry
	Zenith_ByteSpan m_xGeneratedBytes;      // a file the packer generated in memory (base catalogue, DLC fragment, package.zpkm)
	uint64 m_ulBytes = 0;
	Zenith_Hash128 m_xDataHash;             // from the catalogue, or Zenith_Hash128Of(m_xGeneratedBytes) (Formats.md "Archives PACK writes")
};

struct Zenith_PackageArchiveResult
{
	uint64 m_ulBytes = 0;
	Zenith_Sha256Value m_xSha256;
	uint32 m_uEntryCount = 0;
	uint64 m_ulPaddingBytes = 0;
};

Zenith_Result<Zenith_PackageArchiveResult, Zenith_PackageBuildError> Zenith_WritePackageArchive(
	Zenith_Span<const Zenith_PackageArchiveInput> axSortedInputs, Zenith_StringView strOutHostPath, Zenith_Arena& xScratch);
```

1. Reserve the 64-byte header (zeros), then for each input in order: pad to the entry's alignment (Formats.md "Archives PACK writes"), stream the file in 4 MiB reads into the output (or copy its generated bytes), record (offset, bytes, the input's `m_xDataHash`); a file whose size differs from `m_ulBytes` is `PACKAGE_BUILD_ERROR_COOKED_TREE_INVALID`. No 128-bit hash is computed while streaming: CORE has no streaming `Zenith_Hash128` (`Zenith2/Docs/Design/CORE/Maths.md`), and the read-back's `Zenith_VerifyArchiveData` re-hashes every entry against the recorded hash, which catches a stale catalogue hash.
2. Pad to a multiple of 8; write the entry table (40 bytes per entry) and the string table.
3. Rewrite the header with the counts, table offset, archive bytes, table hash and header check, then hash the header into the SHA-256 (the digest covers the whole file: the writer hashes the header after the body by recomputing the digest over the final file in a second streaming pass, which costs one more read of the archive and keeps the digest exactly `Zenith_Sha256Digest(file bytes)`).
4. `FlushFileBuffers` / `fsync`, close, rename `.tmp` over the target.

### Archive read-back

Every archive written is opened through ASSET's reader (`Zenith_CreateArchiveMountSource` over a directory source of the output directory, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetArchive header"`), which runs checks A1–A7, then `Zenith_VerifyArchiveData` re-hashes every entry; any failure is `PACKAGE_BUILD_ERROR_READBACK_MISMATCH`. This is the test-free guarantee that PACK's writer and ASSET's reader agree on every package built, and the packer's tests run it on generated inputs.

### Manifest writers

- `package.zpkm`, once per layout: the units the layout lists (Formats.md "Package manifest") in (base, then recipe array order), the PRESENT flag by the layout's rule (M11), archive table in (unit, archive index) order with the base unit's entries zero (M10), string table in first-use order; the writer runs `Zenith_PackageManifest::Parse` on its own output before publishing.
- `mount-manifest.json`: per layout, the keys of Formats.md "Mount manifests PACK writes", written with `Zenith_JsonWriter` in the key order of PLAT's example, `assetPacks` sorted by order.

### Update manifest writer

```cpp
// Zenith2/Tools/ZenithCook/Internal/Package/Zenith_UpdateManifestWriter.h
struct Zenith_UpdateManifestFile
{
	Zenith_StringView m_strName;
	uint64 m_ulBytes = 0;
	Zenith_Sha256Value m_xSha256;
	bool m_bArchive = false;
};

struct Zenith_UpdateManifestBody
{
	uint16 m_uKind = 1;                        // 1 PATCH, 2 DLC
	uint8 m_auTargetBuildID[16] = {};
	uint16 m_uReleaseMajor = 0, m_uReleaseMinor = 0, m_uVersionPatch = 0;
	uint32 m_uEngineCookVersion = 0;
	uint64 m_ulBaseContentHash = 0, m_ulResultContentHash = 0;
	Zenith_StringView m_strName;                // DLC
	sint32 m_iMountOrder = 0;                  // DLC
	Zenith_Span<const Zenith_UpdateManifestFile> m_axFiles;   // sorted by name
};

// The bytes [0, total - 64) of Formats.md "Update manifest": the header with total bytes counting the signature to come.
Zenith_Result<Zenith_ByteSpan, Zenith_PackageBuildError> Zenith_WriteUpdateManifestBody(const Zenith_UpdateManifestBody& xBody,
	Zenith_Arena& xArena);
```

The body is written to `<bundle dir>/update.zman.unsigned` (or `dlc.zman.unsigned`); the signer appends 64 bytes and the builder renames the result to `update.zman`.

## Ed25519 signer

The writer never opens a key file (cross-area row SH5 (c), `Zenith2/Docs/Design/SHIP/Release.md → "Update manifest signing hand-off"`):

1. `New-Zenith2UpdateSignature -Body <path> -Profile <profile>` (in `Zenith2Package.psm1`) calls SHIP's `Get-Zenith2UpdateSigningKey -Profile <profile>`, which returns the 32-byte seed as `byte[]` after checking it against the profile's public key.
2. It starts the pinned JDK's `java -Xshare:off` with `Zenith2/Tools/Package/Ed25519Sign.java <body path>` (a single-file source launch), writes the seed to the child's standard input as 64 lower-case hex digits and one LF, SHIP's convention for `Ed25519PublicKeyCheck.java` (`Zenith2/Docs/Design/SHIP/Release.md` → "Update manifest signing hand-off"), closes it, and reads 128 hex characters and an LF from its standard output.
3. It zeroes the seed and its hex string's character array (`[Array]::Clear($seed, 0, 32)`, the `char[]` the hex was built in) in a `finally` block whether signing succeeded or not.
4. It appends the 64 signature bytes to the body, writes `<name>.zman` through a `.tmp` and a rename, and deletes the `.unsigned` file.

`Ed25519Sign.java` reads one line of exactly 64 lower-case hex digits terminated by LF from standard input (anything else is exit 3, `seed: expected 64 lower-case hex digits`), decodes it into a 32-byte array, builds `new EdECPrivateKeySpec(NamedParameterSpec.ED25519, seed)`, obtains `KeyFactory.getInstance("Ed25519").generatePrivate(spec)`, signs the body file's bytes with `Signature.getInstance("Ed25519")`, prints the signature as 128 lower-case hex characters and an LF, overwrites the seed array and the line's characters with zeros, and exits 0; any exception is exit 4 with one line on standard error. JDK 17's `SunEC` provider implements Ed25519 (RFC 8032), so no third-party code is added. The signer has no key-generation mode: no test needs a random key, because every test signs with RFC 8032's published seeds (Tests.md "Fixtures").

`Pwsh_Pack_Signer_Rfc8032Test1` is its spec-derived golden (GOLDEN-FILES): an empty body file signed with RFC 8032 § 7.1 TEST 1's seed `9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60` prints exactly the RFC's signature `e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b`, both copied by hand from the RFC; a 63-digit seed and an upper-case seed exit 3.

## Patch builder

```text
pwsh -NoProfile -File Zenith2/Tools/Package/New-Zenith2Patch.ps1
	-Game <Name>
	-BaseRelease <release staging dir of the line's base full release>
	-NewRelease <release staging dir of the new release>
	-Version <MAJOR.MINOR.PATCH>
	-Profile <path to release-profile.json>
	-Out <dir>
	[-Unsigned]                         internal channel only: leaves update.zman.unsigned, no zip
```

The release staging directories are SHIP's (`Zenith2/Docs/Design/SHIP/Release.md → "Release manifest"`, the `release.json` of each, cross-area row SH5 (d)). The builder produces Windows patches only, over the WINDOWS layout and the Windows identity; no Android patch bundle is produced, because Android takes content updates through Play (Algorithms.md "Android content updates"). The patch's version is the new release's own `version`, which SHIP's release pipeline keeps greater than every earlier release and patch of the line, whatever build each targets (ruling R16 of the entry document, cross-area row PK25), so the install state's and the install record's patch floors are valid across builds.

| Step | Action | Exit code |
|---|---|---|
| 1 | read both `release.json`; require the same `game`, the same `releaseLine`, the new `version` equal to `-Version` and greater than the base's, equal Windows `buildId`, equal `engineCookVersion`, equal `composition.compositionHash.windows` | 2 |
| 2 | `ZenithCook --package-patch --base <BaseRelease>/windows/<Game>/Content --new <NewRelease>/windows/<Game>/Content --out <Out>/<Version> --set package.patchVersion=<Version>`; the new release was cooked with the base release's dictionaries ("CMake targets", `ZENITH_PACK_DICTIONARY_BASE`), so its re-encoded files of unchanged assets equal the base's and the diff stays entry-granular | 3 |
| 3 | sign `update.zman.unsigned` ("Ed25519 signer"), unless `-Unsigned` | 4 |
| 4 | `ZenithCook --package-verify --bundle <Out>/<Version> --public-key <profile publicKey as hex> --build-id <buildId> --release-line <releaseLine> --cook-version <engineCookVersion>` | 5 |
| 5 | `New-Zenith2DeterministicZip -Root <Out>/<Version> -Out <Out>/<Game>-<Version>-patch.zip` (SHIP's) | 6 |

Every failure prints `Zenith2 patch: <step>: <reason>`. The builder is run by a human on the release machine (it uses the signing key), like SHIP's pipeline.

## DLC builder

```text
pwsh -NoProfile -File Zenith2/Tools/Package/New-Zenith2Dlc.ps1
	-Game <Name> -Dlc <name> -Revision <1..65535>
	-Release <release staging dir> -PackageDir <${ZENITH_OUT}/Packages/<Game>/windows/Shipping>
	-Profile <path> -Out <dir> [-Unsigned]
```

Steps: read `release.json` (release line, cook version) and require them to equal the unsigned body's; require `<PackageDir>/DlcBundles/<name>/dlc.zman.unsigned` and every archive it lists, whose body the package build wrote with revision 1; write `-Revision` into the body's version-patch field (the `uint16` at offset 40, little-endian, the one field the builder changes; the signature covers it); sign; verify with `--build-id 00000000000000000000000000000000`; copy to `<Out>/<name>/` and zip as `<Game>-dlc-<name>-<Revision>.zip`. Exit codes as the patch builder's.

## CMake targets

`zenith_add_package(<Name>)` (in `Zenith2/CMake/ZenithPackage.cmake`) is called by `zenith_add_game` after its Gradle step (cross-area row PK4) and creates, in every preset the game's platforms include:

| Target | Command | Depends on | Outputs |
|---|---|---|---|
| `<Name>_ShippingContent` | `${ZENITH_HOST_TOOLS_DIR}/ZenithCook --game <Name> --platform <p> --out ${ZENITH_OUT}/Cooked/<Name>/<p>-shipping --set cook.profile=shipping`, plus `--set pack.dictionaryBase=${ZENITH_PACK_DICTIONARY_BASE}` when that cache variable is set (empty by default; SHIP's release pipeline sets it to the line's base release package directory for every release after the line's first, cross-area row PK20) | the cook executable, `composition.json`, `Games/<Name>/package.recipe.json` when present, `<Name>_Validate` | the shipping cooked tree; `DEPFILE …/cook.d` |
| `<Name>_Package` | `${ZENITH_PWSH} -NoProfile -File ${ZENITH_ROOT}/Tools/Package/Invoke-Zenith2Package.ps1 -Game <Name> -Platform <p> -Configuration $<CONFIG> -Binary <the game binary> -Cooked <tree> -Out ${ZENITH_OUT}/Packages/<Name>/<pkgplat>/$<CONFIG> -HostTools ${ZENITH_HOST_TOOLS_DIR}` | `<Name>`, `<Name>_Content`, `<Name>_ShippingContent` | the package directory |
| `<Name>_Bundle` (Android presets) | copy each `bundle/packs/<pack>/assets/` into the bundle project's `<pack>/src/main/assets/` with `cmake -E copy_if_different`, then the bundle project's `gradlew.bat --offline --no-daemon -p <bundle project> bundle$<CONFIG>`, then copy the bundle out, then `${ZENITH_PWSH} -NoProfile -File ${ZENITH_ROOT}/Tools/Package/Invoke-Zenith2Package.ps1 -Bundle <aab> -Game <Name> -Out <dir>`: `Measure-Zenith2BundleSize` with `s24-ultra.json`, `Test-Zenith2ApksStored`, and the adopted Play rows (Z2-B-PACK-001 and Z2-B-PACK-005 to -007 once adopted) compared; exit 0, 1 (bundletool failed), 8 (an adopted row exceeded, naming it: `package: bundle: <row> <measured> MB above <limit> MB`), which fails the target | `<Name>_Package` | `${ZENITH_OUT}/Packages/<Name>/<pkgplat>/<Config>/<Name>-unsigned.aab` (Shipping), `<Name>-release.aab` (Release, development-signed) |

- `<p>` is `windows` or `android`; `<pkgplat>` is `windows`, `android` (the `android-arm64` presets) or `android-x86_64`, so the emulator bundle never overwrites the device bundle. These are the paths SHIP assumes (cross-area row SH5 (a), (b)).
- `<tree>` is the development tree `${ZENITH_OUT}/Cooked/<Name>/<p>/` for Debug and the shipping tree for Release and Shipping, so a Release measurement package measures shipping content.
- The game binary is `$<TARGET_FILE:<Name>>` on Windows and the staged `${ZENITH_OUT}/Games/<Name>/<preset>/jni/$<CONFIG>/<abi>/lib<Name>.so` on Android; `Invoke-Zenith2Package.ps1` reads its build ID with SHIP's `Get-Zenith2BinaryIdentity` and passes it as `package.buildId`, so the mount manifest's `buildId` is the executable's ([Z2-N-ARCH-955] and the Shipping identity rule of `Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount manifest schema"`).
- Every package target is `EXCLUDE_FROM_ALL`; `zenith_end_build` creates the aggregates `zenith_packages` and, on Android presets, `zenith_bundles`. The Release and Shipping `<Name>_Apk` builds depend on `<Name>_Package` and draw their assets from `sideload/assets/` (cross-area row PK4).

## App Bundle build

The bundle is built by a second Gradle project per game and Android preset, `${ZENITH_OUT}/Games/<Name>/<preset>/android-bundle/`, generated by MOD's `_zenith_gradle_project` (`Zenith2/Docs/Design/MOD/GameBuild.md → "Gradle project generation"`) with the arguments this design adds (cross-area row PK4):

| Addition | Value in the bundle project |
|---|---|
| build types | `release`, `shipping` (the bundle is never built for `debug` or `hwasan`) |
| app assets | `${ZENITH_OUT}/Packages/<Name>/<pkgplat>/<Config>/bundle/base/assets/` per build type |
| `settings.gradle.kts` | `include(":<pack>")` for each pack of the bundle layout, in unit order |
| app `build.gradle.kts` | `android.assetPacks += listOf(":<pack>", …)`; `android.androidResources.noCompress += "zpak"`; `android.bundle { abi { enableSplit = true }; density { enableSplit = false }; language { enableSplit = false } }` |
| app runtime dependency | `implementation("com.google.android.play:asset-delivery:<pin>")` when any pack is fast-follow, on-demand or DLC (cross-area row PK7 to SHIP for the pin and its licence) |
| Java sources | `Zenith2/Engine/ZenithPackage/Internal/Android/Java/` added to `java.srcDirs` |
| keep rule | `-keep class org.zenith2.** { native <methods>; public <init>(...); public *; }` (the rule MOD's template already emits for module Java code) |
| pack modules | `<pack>/build.gradle.kts` from `Zenith2/Player/GradleTemplate/assetpack/build.gradle.kts.in` |

```kotlin
// Zenith2/Player/GradleTemplate/assetpack/build.gradle.kts.in
plugins {
	id("com.android.asset-pack")
}

assetPack {
	packName.set("@ZENITH_ASSET_PACK_NAME@")
	dynamicDelivery {
		deliveryType.set("@ZENITH_ASSET_PACK_DELIVERY@")   // install-time, fast-follow or on-demand
	}
}
```

`@ZENITH_ASSET_PACK_DELIVERY@` is `install-time` for INSTALL_TIME units, `fast-follow` for FAST_FOLLOW units and `on-demand` for ON_DEMAND and DLC units. The asset-pack plugin is part of the pinned Android Gradle Plugin, so the build itself adds no Gradle plugin dependency.

## Java bridge

<!-- elaboration: the bridge follows PLAT's Android bridge pattern, `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android bridge lifetime" A1–A4 and "Java bridge contract" -->
```java
// Zenith2/Engine/ZenithPackage/Internal/Android/Java/org/zenith2/pack/Zenith2AssetPacks.java
package org.zenith2.pack;

public final class Zenith2AssetPacks
{
	private final Object m_xLock = new Object();
	private final android.app.Activity m_xActivity;
	private long m_lNative;                                         // the C++ Zenith_PlayDeliveryNativeState*, 0 after shutdown; guarded by m_xLock
	private com.google.android.play.core.assetpacks.AssetPackManager m_xManager;
	private com.google.android.play.core.assetpacks.AssetPackStateUpdateListener m_xListener;

	public Zenith2AssetPacks(android.app.Activity xActivity, long lNative);   // stores both; no Android call
	public boolean init();                                          // AssetPackManagerFactory.getInstance(activity); registers m_xListener; false when the Play library is absent
	public void shutdown();                                         // synchronized (m_xLock) { m_lNative = 0; } then unregisterListener(m_xListener)
	public void requestStates(String[] astrPacks);                  // getPackStates → nativeOnState per pack; failure → nativeOnRequestFailure
	public void fetch(String[] astrPacks);                          // fetch → states via the listener; task failure → nativeOnRequestFailure per pack
	public void cancel(String[] astrPacks);                         // cancel → each returned state through nativeOnState
	public void removePack(String strPack);                         // removePack → nativeOnRemoveResult
	public boolean showConfirmation();                              // runOnUiThread: showConfirmationDialog(m_xActivity) → nativeOnConfirmationResult
	public String getPackAssetsPath(String strPack);                // getPackLocation(pack) then assetsPath(), or null when not on disk

	private static native void nativeOnState(long lNative, String strPack, int iStatus, int iErrorCode, long lBytesDownloaded,
		long lTotalBytesToDownload, int iTransferProgressPercentage, String strInstalledVersionTag);
	private static native void nativeOnRequestFailure(long lNative, String strPack, int iErrorCode);
	private static native void nativeOnRemoveResult(long lNative, String strPack, boolean bSuccess, int iErrorCode);
	private static native void nativeOnConfirmationResult(long lNative, int iResultCode, int iErrorCode);
}
```

| Rule | Statement |
|---|---|
| J1 handle, not global | The natives are `static` but take the native state's address as their first argument; the C++ side casts it to `Zenith_PlayDeliveryNativeState*` (the delivery's slot table and pack-name index) and nothing else, so no process-wide pointer exists (Z2-D-046). `Pwsh_Pack_JavaBridgeNativesTakeHandle` fails when any `native` declaration's first parameter is not `long lNative`. |
| J2 one gate | Every listener method and every `Task` success or failure listener calls its native only inside `synchronized (m_xLock)` and only while `m_lNative != 0`, reading the field inside the monitor; otherwise it returns at once. `shutdown()` sets `m_lNative = 0` inside the same monitor, so it waits for a native call in progress and, once it returns, no native is ever entered again: a late listener invocation on the UI thread, or a `getPackStates`, `fetch`, `removePack` or `showConfirmationDialog` task completing on Play's threads after the delivery's destructor, is a no-op by construction. |
| J3 no deadlock | A native holds `uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES` (650) only while it writes one cell, and the main thread never holds that lock while calling Java (`Poll` copies the cells and releases the lock before any JNI call), so the monitor and the lock are never taken in opposite orders. |
| J4 no SDL | `Internal/Android/Zenith_PlayAssetPackDelivery_Android.cpp` obtains the `JavaVM*`, the activity and its class loader from `Zenith_Platform::GetAndroidJavaContext()` (cross-area row PK14), calls `GetEnv` (the SDL main thread is attached), loads the class with the class loader's `loadClass("org.zenith2.pack.Zenith2AssetPacks")` (because `FindClass` on a native thread sees only system classes), registers the four natives with `RegisterNatives` from the table in `Internal/Android/Zenith_PlayAssetPacksJniTable.h`, constructs the bridge with the native state's address, keeps a global reference, and calls `init()`; `init()` returning false (the Play library absent) is `PACKAGE_ERROR_DELIVERY_UNAVAILABLE`. ZenithPackage includes no SDL header and calls no SDL function. |
| J5 destruction order | The delivery's destructor, on the main thread in shutdown step 6: `shutdown()` (J2), then `Zenith_DeliverySlotTable::Close()` (a defensive no-op gate, "Delivery update slots" in Algorithms.md), then `DeleteGlobalRef` of the bridge, then the native state is freed. |
| J6 strings | The natives copy their string arguments (`GetStringUTFChars`, released before return; pack names are ASCII by the name rule, the version tag is hashed as UTF-8) into the pack's cell and return; they never call back into Java or into the installer. |

- `m_ulPackVersion` is the first 8 bytes (little-endian) of `Zenith_Sha256Digest` of the UTF-8 `installedVersionTag` of the pack's latest state, 0 for an empty tag.
- `showConfirmation()` uses `AssetPackManager.showConfirmationDialog(Activity)` with SDL's activity, which Play requires to be the current foreground activity ("the activity on top of which the confirmation dialog is displayed"); it covers both WAITING_FOR_WIFI and REQUIRES_USER_CONFIRMATION, replacing the deprecated `showCellularDataConfirmation`, which covers WAITING_FOR_WIFI only. Its `Task<Integer>` result (`RESULT_OK` 1 accepted, `RESULT_CANCELED` 2 declined) and any failure's error code arrive through `nativeOnConfirmationResult`. Play may restart an app not installed from Play to update it, cancelling its asset requests; the persisted `WANT_INSTALLED` intents resume them after the restart ("Fetch policy" in Algorithms.md).
- `Pwsh_Pack_JavaBridgeMatchesJniTable` parses the Java file's `native` declarations and the C++ table's name and signature strings and fails on any difference.

## bundletool usage

bundletool is a pinned `host-tool` jar run by the pinned JDK (`java -jar <bundletool> …`), never linked (cross-area row PK7). `Zenith2Package.psm1` wraps every call:

| Function | Command |
|---|---|
| `Test-Zenith2Bundle -Bundle <aab>` | `validate --bundle=<aab>` |
| `New-Zenith2DeviceApks -Bundle <aab> -DeviceSpec <json> -Out <apks> [-LocalTesting]` | `build-apks --bundle=<aab> --output=<apks> --device-spec=<json> [--local-testing] --ks=$env:ZENITH2_DEV_KEYSTORE --ks-key-alias=zenith2-development --ks-pass=pass:zenith2-development --key-pass=pass:zenith2-development` (SHIP's development keystore, `Zenith2/Docs/Design/SHIP/Manifest.md` → "Development signing keystore"; `ZENITH_DEV_KEYSTORE` in CMake) |
| `Get-Zenith2ApksSplits -Apks <apks>` | lists the `.apks` zip's entries (`splits/*.apk`, `asset-slices/*.apk`) and returns their module and split names |
| `Measure-Zenith2BundleSize -Bundle <aab> -DeviceSpec <json> -Out <bundle-report.json>` | "Bundle size measurement" |
| `Install-Zenith2Apks -Apks <apks> -Serial <device>` | `install-apks --apks=<apks> --device-id=<serial> --adb=<pinned adb>` (device and emulator runs, never a gate) |
| `Test-Zenith2ApksAlignment -Apks <apks>` | `extract-apks` for the device spec into a scratch directory, then `zipalign -c -P 16 -v 4` on each split and `llvm-readelf -l` on each `.so` (every PT_LOAD alignment ≥ 0x4000) |
| `Test-Zenith2ApksStored -Apks <apks>` | `extract-apks` for the device spec, then every `assets/**/*.zpak` entry of every split read with `System.IO.Compression.ZipArchive`: compression method 0 (stored), else fail naming the entry |
| `Test-Zenith2ApksLocalTesting -Apks <apks>` | `extract-apks`, then `aapt2 dump xmltree --file AndroidManifest.xml <base-master split>`: the base split's manifest carries the local-testing marker bundletool adds with `--local-testing` (the `<meta-data android:name="local_testing_dir" android:value="local_testing"/>` element of the application; the pinned bundletool's source is re-read by the ticket that pins it) |

### Bundle size measurement

`Measure-Zenith2BundleSize` is MP-PKG's procedure for Z2-B-PACK-001 ([Z2-N-BUDGET-173]) and for the pack rows Z2-B-PACK-005 to -007: `build-apks --device-spec=s24-ultra.json` over the Shipping bundle, then `get-size total --apks=<apks> --modules=base --dimensions=ALL` for the base module (without `--modules`, bundletool sizes every module installed at install time, install-time asset packs included, while the row bounds the base module, [Z2-N-BUDGET-173]), `get-size total --apks=<apks> --dimensions=ALL` for the install-time total, and `get-size total --apks=<apks> --modules=<pack>` per pack, parsing bundletool's CSV (`SDK,ABI,…,MIN,MAX`) and recording the maximum; the pinned bundletool's `help get-size` is re-read by the ticket that pins it. `bundle-report.json`:

```json
{ "schema": "zenith2.bundle-report", "schemaVersion": 1, "game": "OpenWorldSlice", "bundle": "<sha256>",
  "deviceSpec": "s24-ultra", "baseModuleMaxBytes": 123456789, "installTimeMaxBytes": 223456789,
  "packs": [ { "name": "cinematics", "delivery": "on-demand", "maxBytes": 987654321 } ],
  "packsTotalMaxBytes": 987654321, "packCount": 1 }
```

### Local testing and the emulator smoke

`build-apks --local-testing` makes the Play library serve fast-follow and on-demand packs from the device's local storage, so the delivery flow runs without Play. The emulator smoke, on the `android-x86_64` preset's Release bundle and `emulator-x86_64.json`, installs with `Install-Zenith2Apks` and runs the automated pack script: OpenWorldSlice's scene `content:/Scenes/PackCheck/PackDelivery.scene.json`, started with `--set content.startupScene=…` through MOD's arguments extra, whose behaviour requests an on-demand pack, quits with exit code 0 at `Package.PackStateChanged` MOUNTED and reports a test failure (exit code 5) on an error or after 3,000 ticks; it is a smoke only ([Z2-N-ARCH-216]: no acceptance result is taken from it). The S24 local-testing run starts the same script on the Release bundle: the `shipping` build type refuses the arguments extra, so the Shipping bundle's delivery is proven by the S24 runs through a Play internal test track. Local testing delivers instantly and cannot interrupt a download, so interruption is proved by the simulated delivery's tests and by the S24 run through a Play internal test track (`Zenith2/Docs/Design/PACK/Tests.md → "Device runs"`).

## Windows install conventions

- The release zip extracts to any user-writable directory; `%LOCALAPPDATA%\Programs\Zenith2\<Game>\` is the documented default in the release notes (`Zenith2/Docs/Design/SHIP/Release.md → "Release documentation"` is SHIP's; PACK supplies the paragraph through cross-area row PK5).
- A deliverer (a launcher, a script, a person) places a patch bundle by writing it as `Patches/.incoming/.<version>.partial/` and renaming it to `Patches/.incoming/<version>/` when complete, and a DLC bundle likewise under `DLC/.incoming/`; the applier ignores dot-prefixed names, so a bundle is never seen half-written.
- Steam distribution replaces whole files of the package; a Steam build ships with an empty `Patches/` and patches arrive as new full releases (classified `WITHIN_LINE`), which removes the need for the patch mount there without changing the engine.

## PowerShell functions

| Function | Purpose |
|---|---|
| `Get-Zenith2BundletoolPath`, `Get-Zenith2JavaPath` | the pinned tools from `Zenith2Env.psm1`'s manifest readers |
| `Test-Zenith2Bundle`, `New-Zenith2DeviceApks`, `Get-Zenith2ApksSplits`, `Measure-Zenith2BundleSize`, `Install-Zenith2Apks`, `Test-Zenith2ApksAlignment` | "bundletool usage" |
| `New-Zenith2UpdateSignature` | "Ed25519 signer" |
| `New-Zenith2MiniBundle -Out <dir> -Packs <spec> [-NativeLibraryPageSize 16384\|4096]` | builds a test-time App Bundle without Gradle: `aapt2 link --proto-format` (pinned build-tools, pinned `android.jar`) over the generated base manifest and one asset-pack manifest per pack ("Mini bundle manifests" below), module zips in bundletool's layout (`manifest/AndroidManifest.xml`, `resources.pb`, `assets/…`, and for the base `lib/arm64-v8a/libminimal.so`), then `build-bundle --modules=… --config=bundle-config.json`; `libminimal.so` is compiled by the pinned NDK's clang from a C source the function writes (`int zenith2_minimal(void) { return 7; }`) with `--target=aarch64-linux-android34 -shared -fPIC -Wl,-z,max-page-size=<page size>` (16,384 by default; 4,096 for the negative case of `Pwsh_Pack_MiniBundle_Alignment`) |
| `Invoke-Zenith2Package` | the body of `Invoke-Zenith2Package.ps1` (package mode, and `-Bundle` mode with exits 0, 1 and 8) |

### Mini bundle manifests

The base module's `AndroidManifest.xml`:

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android" xmlns:dist="http://schemas.android.com/apk/distribution"
	package="org.zenith2.minibundle" android:versionCode="1" android:versionName="1.0">
	<uses-sdk android:minSdkVersion="34" android:targetSdkVersion="34"/>
	<dist:module dist:instant="false"/>
	<application android:hasCode="false"/>
</manifest>
```

One asset-pack module per pack, `<pack>/manifest/AndroidManifest.xml`, where `@DELIVERY@` is `<dist:install-time/>`, `<dist:fast-follow/>` or `<dist:on-demand/>`:

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android" xmlns:dist="http://schemas.android.com/apk/distribution"
	package="org.zenith2.minibundle" split="<pack>">
	<dist:module dist:type="asset-pack">
		<dist:fusing dist:include="true"/>
		<dist:delivery>
			@DELIVERY@
		</dist:delivery>
	</dist:module>
</manifest>
```
