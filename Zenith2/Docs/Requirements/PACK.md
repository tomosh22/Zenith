# PACK requirement catalogue

## Purpose

This catalogue breaks the PACK inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "PACK Packaging & content updates") into testable requirements `Z2-R-PACK-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves (its fix version; a ticket may be scheduled earlier by the pull-forward rule), its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of `Zenith2/Docs/Design/PACK.md` and its companion files that specify it; the design's "Requirement index" is the reverse map. PACK owns every requirement here. Device runs are `needs-human` evidence by their nature; the requirements they prove are PACK's. Budget rows PACK requests and that do not exist yet (the Play pack rows, the patch size, the service memory, the verification throughput floor and the cold-boot update verification time of `Zenith2/Docs/Design/PACK.md` → "Pending new rows") are named in the statements and evidence by their row names and carry no row ID until the Budgets core-doc batch assigns one. The catalogue is two files with one numbering, on the SAVE and UI precedent: this file holds the purpose, the requirements whose `Stage` is S1, S2 or S4, the coverage of inventory items over both files and the exclusions; `Zenith2/Docs/Requirements/PACK/Stage6.md` holds the requirements whose `Stage` is S6, in the same format.

## Requirements

### Z2-R-PACK-001 ZenithPackage library target

- **Statement:** The engine library `ZenithPackage` exists at `Zenith2/Engine/ZenithPackage/` from Stage 2 (its first slice is `Zenith_PackageTypes.h`) with `ZENITH_SUPPORTED ZENITH_ALL`, declares exactly ZenithCore, ZenithProfiling, ZenithPlatform, ZenithAssets and ZenithJobs as direct dependencies (layer 4), and is linked by ZenithRuntime into every Player.
- **Covers:** Z2-I-PACK-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step over `declared_deps.json`; `CompileCoverage` on every preset; Build step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Target summary"; Zenith2/Docs/Design/PACK.md → "Libraries PACK depends on"; Zenith2/Docs/Design/PACK.md → "ZENITH_SUPPORTED matrix"

### Z2-R-PACK-003 Package error enumeration

- **Statement:** `Zenith_PackageError` is a `uint8` enumeration numbered from 1 with exactly the enumerators of the entry document's table, a `constexpr Zenith_ToString` overload, and every enumerator returned only in the cases the table names.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageTypes_ToStringCoversEveryEnumerator` and every failure case of the PACK tests asserting its enumerator; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Error enumeration"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_PackageTypes header"

### Z2-R-PACK-011 Binary format registry file

- **Statement:** `Zenith2/Tools/Package/format-registry.json` lists every versioned binary format of Zenith2 with magic, extension, owner, version, compatibility class, specification path and refusal test, in the schema of "Format registry file", and an owner adds its row in the diff that introduces a format.
- **Covers:** Z2-I-PACK-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_FormatRegistry_Valid`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"; Zenith2/Docs/Design/PACK/Formats.md → "Format registry file"

### Z2-R-PACK-012 Format registry validation

- **Statement:** The registry check rejects a duplicate magic or extension, a bad magic charset, an unknown owner or class, a missing specification path and a refusal test whose doctest name appears in no `TEST_CASE` under `Zenith2/`.
- **Covers:** Z2-I-PACK-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_FormatRegistry_Valid`, `CMake_Pack_FormatRegistry_Fixtures`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Format registry file"; Zenith2/Docs/Design/PACK/Tests.md → "Format registry tests"

### Z2-R-PACK-013 Compatibility classes

- **Statement:** Every registered format has exactly one of the classes `recook`, `signed-update`, `migrate`, `user-state`, `transient`, and packaging and updates treat each class as the table states: only `recook` and `signed-update` files are ever shipped, `migrate` and `user-state` never.
- **Covers:** Z2-I-PACK-01, Z2-I-PACK-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_FormatRegistry_Valid`, `Test_PatchDiff_ForbiddenAndEmpty`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"

### Z2-R-PACK-014 Common rules of PACK binary formats

- **Statement:** Every PACK binary format is little-endian, written and read field by field, begins with a four-byte magic and a `uint16` version, checks every size, count and offset with overflow-checked arithmetic, refuses non-zero reserved fields, and carries a low-32-bit `Zenith_Hash64` check or a signature.
- **Covers:** Z2-I-PACK-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageFormats_NoLayoutCopy` and each format's EachCheckRejects test; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Common rules for PACK formats"; Zenith2/Docs/Design/PACK/Formats.md → "Name rules"

### Z2-R-PACK-015 Package manifest reader refuses other versions

- **Statement:** The `package.zpkm` reader refuses any format version other than 1 with `PACKAGE_ERROR_MANIFEST_VERSION`, never an assert, and the registry's `ZPKM` row, added in the same Stage 4 diff, names that refusal test.
- **Covers:** Z2-I-PACK-01, Z2-I-PACK-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifest_Parse_OtherVersion_IsRefused`, `CMake_Pack_FormatRegistry_Valid`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Common rules for PACK formats"; Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"

### Z2-R-PACK-016 Package manifest parser limits

- **Statement:** The limits of `package.zpkm` (units 4,096, archives 65,536, string table 1,048,576 bytes, the file at most 5,374,016 bytes, at most 256 units without the PRESENT flag by check M12) are constants tested at the limit and at limit + 1, and the core parser-limit table carries them.
- **Covers:** Z2-I-PACK-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifest_Parse_LimitsAtAndAbove`, `Test_PackageManifest_Parse_EachCheckRejects` (M12); Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_PackageTypes header"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest checks"

### Z2-R-PACK-018 Packer writes ASSET's archive layout

- **Statement:** PACK's archive writer produces exactly the version-1 `.zpak` layout of `Zenith2/Docs/Design/ASSET/Formats.md` → "Archive file", and every archive it writes passes ASSET's checks A1–A7 and `Zenith_VerifyArchiveData` before it is published.
- **Covers:** Z2-I-PACK-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageArchiveWriter_ReadBackPasses`; the read-back of every package build; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Archive writer"; Zenith2/Docs/Design/PACK/Tooling.md → "Archive read-back"; Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"

### Z2-R-PACK-019 Archive entry policy

- **Statement:** Archive entries are every file of the unit except `.`-prefixed cooked directories, the development mount manifest and stamps, sorted by path bytes, stored (flags 0), entries of at least 65,536 bytes aligned to 16,384 bytes and smaller ones to 16, with zero padding.
- **Covers:** Z2-I-PACK-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageArchiveWriter_Alignment`, `Test_PackageArchiveWriter_ReadBackPasses`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"

### Z2-R-PACK-020 Deterministic archives

- **Statement:** Two package builds from the same inputs produce byte-identical archives, `package.zpkm`, mount manifests and reports, with no timestamp, host path or environment value in any output.
- **Covers:** Z2-I-PACK-02, Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageMode_TwiceIdentical`, `Test_PackageArchiveWriter_TwiceIdentical`, `Test_PackagePartition_Deterministic`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "Archive writer"

### Z2-R-PACK-021 Archive limits enforced at pack time

- **Statement:** An entry above `ulASSET_MAX_COOKED_FILE_BYTES` or an archive above `ulASSET_MAX_ARCHIVE_BYTES` stops the build with `PACKAGE_BUILD_ERROR_ARCHIVE_LIMIT` (exit 7) before any archive is published.
- **Covers:** Z2-I-PACK-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageArchiveWriter_LimitsRefused`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes"

### Z2-R-PACK-022 Package modes in ZenithCook

- **Statement:** ZenithCook gains exactly the package, patch and verify modes of "ZenithCook package modes" with their options and exit codes 0, 2, 3, 4, 5, 6 and 7, implemented in `Zenith2/Tools/ZenithCook/Internal/Package/`.
- **Covers:** Z2-I-PACK-02, Z2-I-PACK-05, Z2-I-PACK-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageMode_ExitCodes`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes"; Zenith2/Docs/Design/PACK/Tooling.md → "Tool-side file layout"

### Z2-R-PACK-023 Atomic package publication

- **Statement:** A package build publishes each layout by renaming a complete `<layout>.new` over the previous one, so a failure at any step leaves the previous layout intact and removes the scratch directory.
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageMode_PublishKeepsPreviousLayout`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"

### Z2-R-PACK-024 Recipe compression policy

- **Statement:** The recipe's top-level and per-class `compression` fragments are validated with COMPRESS's `Zenith_ValidateCompressionSetting`, each asset kind belongs to at most one class, and `dictionary: true` requires codec zstd.
- **Covers:** Z2-I-PACK-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageRecipe_ValidateEachRule`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Packaging recipe"; Zenith2/Docs/Design/PACK/Tooling.md → "Shipping cook profile"

### Z2-R-PACK-025 Shipping cook profile

- **Statement:** With `cook.profile=shipping` the cook writes `${ZENITH_OUT}/Cooked/<Name>/<platform>-shipping/`, and every section whose `.meta` has no compression member uses `Zenith_PackageCompressionPolicy_Resolve` (class fragment, else the recipe default, else COMPRESS's shipping recommendation).
- **Covers:** Z2-I-PACK-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageCompressionPolicy_ResolveOrder`, `Test_PackageDictionaryStep_ReencodedSectionsDecode`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Shipping cook profile"

### Z2-R-PACK-026 Dictionary training step

- **Statement:** The `pack.dictionaries` step trains one zstd dictionary per recipe class with `dictionary: true` from at most 134,217,728 sample bytes in GUID order, skips classes under 1,048,576 sample bytes, and runs in `COOK_PHASE_GLOBAL_POST` before `asset.catalogue`.
- **Covers:** Z2-I-PACK-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageDictionaryStep_SmallClassSkipped`, `Test_PackageDictionaryStep_DeterministicGuidAndBytes`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Dictionary policy"

### Z2-R-PACK-027 Deterministic dictionary identity

- **Statement:** A trained dictionary's GUID is derived from the recipe ID and class name by SHA-256 with version-4 and variant bits set, it is written as a kind-17 `.zdict` into the base unit, and a clean and an incremental cook produce identical dictionaries and re-encoded files.
- **Covers:** Z2-I-PACK-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageDictionaryStep_DeterministicGuidAndBytes`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Dictionary policy"

### Z2-R-PACK-028 Re-encoded sections decode

- **Statement:** Every section re-encoded with a class dictionary decodes through `Zenith_DecompressSection` with that dictionary to its original bytes, and the catalogue records the re-encoded files' output hashes.
- **Covers:** Z2-I-PACK-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageDictionaryStep_ReencodedSectionsDecode`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Dictionary policy"

### Z2-R-PACK-029 Archive entries stay byte-addressable

- **Statement:** No archive entry is compressed by the packer (every entry's flags are 0, stored), so cooked sections remain readable at their raw offsets within an archive; entries of 65,536 bytes and more start at multiples of 16,384.
- **Covers:** Z2-I-PACK-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageArchiveWriter_ReadBackPasses` (entry flags 0, offsets), `Test_PackageArchiveWriter_Alignment`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "Archive writer"

### Z2-R-PACK-030 Compression in the size report

- **Statement:** The size report gives stored and decoded bytes per codec and every trained dictionary's class, GUID and size.
- **Covers:** Z2-I-PACK-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-PACK-001, Z2-B-PACK-002
- **Acceptance evidence:** `Test_PackageSizeReport_CompressionTotals`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report"

### Z2-R-PACK-031 Recipe units

- **Statement:** A recipe declares at most 255 units of kind installTime, fastFollow, onDemand or dlc with unique names, unique orders in their ranges, 1–64 include patterns, 0–64 exclude patterns, an optional kinds filter and an entitlement exactly for dlc; the base unit is implicit.
- **Covers:** Z2-I-PACK-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageRecipe_ValidateEachRule`, `Test_PackageRecipe_PatternMatching`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Packaging recipe"

### Z2-R-PACK-032 Rule assignment

- **Statement:** Each asset goes to the first recipe unit, in array order, whose include matches its logical path, whose exclude does not, and whose kinds filter contains its kind; otherwise to base; and the reason is recorded.
- **Covers:** Z2-I-PACK-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_RuleAssignmentFirstMatch`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm"

### Z2-R-PACK-033 Hard dependencies promoted to base

- **Statement:** An asset of a FAST_FOLLOW or ON_DEMAND unit with a hard dependent outside its unit (in base, in an INSTALL_TIME unit or in another deliverable unit) is promoted to base with a worklist that visits each asset at most once, while assets of base and INSTALL_TIME units are never promoted, so every hard dependency is present whenever its dependent is.
- **Covers:** Z2-I-PACK-04, Z2-I-PACK-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_PromotionToBase`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm"

### Z2-R-PACK-034 DLC dependency integrity

- **Statement:** A non-DLC asset hard-depending on a DLC asset and a DLC asset hard-depending on another DLC unit stop the build with `DEPENDENCY_IN_DLC` and `DLC_CROSS_DEPENDENCY`; soft dependencies may cross units.
- **Covers:** Z2-I-PACK-04, Z2-I-PACK-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_DlcIntegrityErrors`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm"

### Z2-R-PACK-035 Global files in base

- **Statement:** Bytecode, `schemas.zreg`, shader containers, the PSO precache set, merged configuration, dictionaries, `assets.zcat` and `package.zpkm` are always in the base unit; a DLC catalogue fragment is in its DLC unit.
- **Covers:** Z2-I-PACK-04, Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_GlobalFilesInBase`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm"

### Z2-R-PACK-036 Base catalogue and DLC fragments

- **Statement:** The base catalogue lists every non-DLC asset and no DLC asset, and each DLC unit carries `Catalogues/<unit>.zcat`, a fragment whose dependencies outside it are external references in ASSET's format.
- **Covers:** Z2-I-PACK-04, Z2-I-PACK-11, Z2-I-PACK-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_BaseCatalogueAndFragments`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm"; Zenith2/Docs/Design/PACK/Formats.md → "Patch and DLC bundles"

### Z2-R-PACK-037 Chunk target and archive names

- **Statement:** A unit's sorted files are cut into archives before the entry that would exceed `chunkTargetBytes` (16 MiB to 4 GiB, default 1 GiB), an oversized entry is alone, archives are named `<unit>.zpak` or `<unit>-NN.zpak`, and more than 100 archives is `TOO_MANY_ARCHIVES`.
- **Covers:** Z2-I-PACK-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageChunking_TargetAndNaming`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Chunking"; Zenith2/Docs/Design/PACK/Formats.md → "Name rules"

### Z2-R-PACK-038 Partition report

- **Statement:** `partition.json` lists every asset with its unit and reason, every promotion with its dependent and every archive's entries, byte-identical across runs over the same inputs.
- **Covers:** Z2-I-PACK-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_Deterministic`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report"

### Z2-R-PACK-039 Recipe authoring document

- **Statement:** `package.recipe.json` is the authoring document `Zenith.Package.RecipeDocument` (schema tag `zenith2.package-recipe`, version 1) registered by `Zenith_RegisterPackageDocumentTypes` in ZenithCook and ZenithEditor, migrated through the framework with fixtures, keeping unknown members with a warning.
- **Covers:** Z2-I-PACK-04, Z2-I-PACK-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageRecipe_MigrationFixtures`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Packaging recipe"; Zenith2/Docs/Design/PACK.md → "What PACK authors"; Zenith2/Docs/Design/PACK.md → "Migration hooks"

### Z2-R-PACK-040 Package manifest content

- **Statement:** `content:/Package/package.zpkm` in the base unit's first archive is written once per installation layout (WINDOWS, ANDROID_SIDELOAD, ANDROID_BUNDLE) and records the layout, every unit the layout lists (name, kind, flags including PRESENT, entitlement, mount order, bytes) and every archive (name, bytes, SHA-256), the base unit's archives with zero bytes and an all-zero digest, the build ID, content hash, release line and engine cook version, in format version 1.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifestWriter_PerLayout`, `Test_PackageManifest_Parse_RoundTrip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Package manifest"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest header"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest tables"

### Z2-R-PACK-041 Package manifest checks

- **Statement:** `Zenith_PackageManifest::Parse` applies checks M1–M11 in order and reports the first failure with its error, never using a partly valid manifest.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifest_Parse_EachCheckRejects`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Package manifest checks"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_PackageManifest header"

### Z2-R-PACK-043 Shipped mount manifests

- **Statement:** The packer writes the shipped `mount-manifest.json` of each layout in PLAT's schema with the keys of "Mount manifests PACK writes" and validates each with `Zenith_ParseMountManifest` before publishing.
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifestWriter_MountManifestsValidate`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Mount manifests PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "Manifest writers"

### Z2-R-PACK-044 Windows package layout

- **Statement:** The Windows package layout is `${ZENITH_OUT}/Packages/<Game>/windows/<Config>/Content/` holding `mount-manifest.json` and every non-DLC archive, and DLC bundles under `DlcBundles/<name>/`, the directory SHIP stages.
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageMode_WindowsLayoutListing`, `Test_PackageMode_TwiceIdentical`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Windows install layout"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"

### Z2-R-PACK-046 Android sideload layout

- **Statement:** The Android sideload layout puts `mount-manifest.json` without asset packs, every non-DLC archive and base archives carrying the ANDROID_SIDELOAD `package.zpkm` into `sideload/assets/`, which the Release and Shipping APKs package, and omits DLC units.
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifestWriter_MountManifestsValidate`, `CMake_Pack_PackageTargets`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Android install layout"; Zenith2/Docs/Design/PACK/Algorithms.md → "Sideload builds"

### Z2-R-PACK-048 Package CMake targets

- **Statement:** `zenith_add_package(<Name>)` creates `<Name>_ShippingContent`, `<Name>_Package` and, on Android presets, `<Name>_Bundle` with the commands, dependencies and output paths of "CMake targets", all `EXCLUDE_FROM_ALL`, with the aggregates `zenith_packages` and `zenith_bundles`.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_PackageTargets`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets"

### Z2-R-PACK-050 Size report

- **Statement:** Every package build writes `size-report.json` with unit, archive, kind and class bytes, the native binary's bytes per linked library from its linker map, other files, and compression totals.
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-PACK-001, Z2-B-PACK-002, Z2-B-PACK-003, Z2-B-PACK-004
- **Acceptance evidence:** `Test_PackageSizeReport_LinkerMapAttribution`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report"; Zenith2/Docs/Design/PACK.md → "Answers to consumers of PACK rows"

### Z2-R-PACK-160 Size report answers D-SEL-CRASH

- **Statement:** The size report attributes a crash-handler candidate's bytes in the native binary and its extra files, so DEVT's harness obtains each candidate's base-module and Windows-install delta from two reports.
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-PACK-001, Z2-B-PACK-002
- **Acceptance evidence:** `Test_PackageSizeReport_LinkerMapAttribution`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Answers to consumers of PACK rows"; Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report"

### Z2-R-PACK-163 Stage 1 per-system budget ticket for PACK

- **Statement:** The Stage 1 per-system budget ticket for PACK allocates Z2-B-PACK-002 to -004 from published data with its allocation file and DecisionLog entry, as a `needs-human` ruling with no harness half.
- **Covers:** Z2-I-PACK-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-PACK-002, Z2-B-PACK-003, Z2-B-PACK-004
- **Acceptance evidence:** the producer ticket's diff; `Check-Allocation.ps1`; acceptance mode of the coverage gate step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Rows PACK owns"; Zenith2/Docs/Design/PACK.md → "Stage 1"

### Z2-R-PACK-170 Package manifest per layout

- **Statement:** Each installation layout carries its own `package.zpkm`, whose PRESENT flags follow the layout's rule (M11) and whose base-unit archives are recorded with zero bytes and digest (M10), so deliverability is decided by the manifest the running build carries and the manifest never records its own archive.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifestWriter_PerLayout`, `Test_PackageManifest_Parse_EachCheckRejects`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Package manifest"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest tables"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"

### Z2-R-PACK-186 Shipping packages refuse stale bakes

- **Statement:** Package mode reads the shipping-profile cooked tree's `.cook/cook-report.json` before partitioning and, with `package.configuration` Shipping, refuses with `PACKAGE_BUILD_ERROR_BAKE_STALE` (exit 7) while any `diagnostics` element's `code` is `WORLD_WARNING_BAKE_STALE` or `WORLD_WARNING_BAKE_MISSING`, including the replayed diagnostics of a world import that did not run in that cook (row WD35, pending ASSET), naming every finding in `package-errors.json`; Debug and Release packages log the same findings as warnings and continue; a missing or invalid report is `PACKAGE_BUILD_ERROR_COOKED_TREE_INVALID` (exit 3).
- **Covers:** Z2-I-PACK-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageMode_StaleBakesRefuseShipping`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes"

## Coverage of inventory items

| Item | Name | Requirements |
|---|---|---|
| Z2-I-PACK-01 | versioned binary formats | Z2-R-PACK-011, Z2-R-PACK-012, Z2-R-PACK-013, Z2-R-PACK-014, Z2-R-PACK-015, Z2-R-PACK-016, Z2-R-PACK-017, Z2-R-PACK-177, Z2-R-PACK-178, Z2-R-PACK-187 |
| Z2-I-PACK-02 | archives | Z2-R-PACK-018, Z2-R-PACK-019, Z2-R-PACK-020, Z2-R-PACK-021, Z2-R-PACK-022 |
| Z2-I-PACK-03 | compression | Z2-R-PACK-024, Z2-R-PACK-025, Z2-R-PACK-026, Z2-R-PACK-027, Z2-R-PACK-028, Z2-R-PACK-029, Z2-R-PACK-030, Z2-R-PACK-185 |
| Z2-I-PACK-04 | chunking | Z2-R-PACK-031, Z2-R-PACK-032, Z2-R-PACK-033, Z2-R-PACK-034, Z2-R-PACK-035, Z2-R-PACK-036, Z2-R-PACK-037, Z2-R-PACK-038, Z2-R-PACK-039 |
| Z2-I-PACK-05 | installation layouts | Z2-R-PACK-001, Z2-R-PACK-002, Z2-R-PACK-008, Z2-R-PACK-020, Z2-R-PACK-022, Z2-R-PACK-023, Z2-R-PACK-035, Z2-R-PACK-039, Z2-R-PACK-040, Z2-R-PACK-041, Z2-R-PACK-042, Z2-R-PACK-043, Z2-R-PACK-044, Z2-R-PACK-045, Z2-R-PACK-046, Z2-R-PACK-047, Z2-R-PACK-048, Z2-R-PACK-049, Z2-R-PACK-050, Z2-R-PACK-051, Z2-R-PACK-160, Z2-R-PACK-163, Z2-R-PACK-170, Z2-R-PACK-186 |
| Z2-I-PACK-06 | Android App Bundles | Z2-R-PACK-047, Z2-R-PACK-048, Z2-R-PACK-052, Z2-R-PACK-053, Z2-R-PACK-054, Z2-R-PACK-055, Z2-R-PACK-056, Z2-R-PACK-057, Z2-R-PACK-058, Z2-R-PACK-161, Z2-R-PACK-185 |
| Z2-I-PACK-07 | Play Asset Delivery asset packs | Z2-R-PACK-002, Z2-R-PACK-003, Z2-R-PACK-004, Z2-R-PACK-005, Z2-R-PACK-006, Z2-R-PACK-010, Z2-R-PACK-040, Z2-R-PACK-041, Z2-R-PACK-047, Z2-R-PACK-053, Z2-R-PACK-056, Z2-R-PACK-057, Z2-R-PACK-059, Z2-R-PACK-060, Z2-R-PACK-061, Z2-R-PACK-062, Z2-R-PACK-063, Z2-R-PACK-064, Z2-R-PACK-065, Z2-R-PACK-066, Z2-R-PACK-067, Z2-R-PACK-068, Z2-R-PACK-069, Z2-R-PACK-070, Z2-R-PACK-071, Z2-R-PACK-072, Z2-R-PACK-074, Z2-R-PACK-075, Z2-R-PACK-076, Z2-R-PACK-077, Z2-R-PACK-078, Z2-R-PACK-079, Z2-R-PACK-081, Z2-R-PACK-082, Z2-R-PACK-083, Z2-R-PACK-085, Z2-R-PACK-086, Z2-R-PACK-156, Z2-R-PACK-157, Z2-R-PACK-158, Z2-R-PACK-159, Z2-R-PACK-161, Z2-R-PACK-162, Z2-R-PACK-170, Z2-R-PACK-172, Z2-R-PACK-181, Z2-R-PACK-182 |
| Z2-I-PACK-08 | bundletool split-APK testing | Z2-R-PACK-058, Z2-R-PACK-088, Z2-R-PACK-089, Z2-R-PACK-090, Z2-R-PACK-091, Z2-R-PACK-092, Z2-R-PACK-093, Z2-R-PACK-094 |
| Z2-I-PACK-09 | installed-update migration | Z2-R-PACK-008, Z2-R-PACK-095, Z2-R-PACK-096, Z2-R-PACK-097, Z2-R-PACK-098, Z2-R-PACK-099, Z2-R-PACK-100, Z2-R-PACK-101, Z2-R-PACK-102, Z2-R-PACK-159 |
| Z2-I-PACK-10 | patch manifests | Z2-R-PACK-022, Z2-R-PACK-103, Z2-R-PACK-104, Z2-R-PACK-105, Z2-R-PACK-106, Z2-R-PACK-107, Z2-R-PACK-108, Z2-R-PACK-109, Z2-R-PACK-110, Z2-R-PACK-111, Z2-R-PACK-112, Z2-R-PACK-113, Z2-R-PACK-122, Z2-R-PACK-167, Z2-R-PACK-168, Z2-R-PACK-169, Z2-R-PACK-171, Z2-R-PACK-176, Z2-R-PACK-184, Z2-R-PACK-187, Z2-R-PACK-188, Z2-R-PACK-189 |
| Z2-I-PACK-11 | DLC manifests | Z2-R-PACK-034, Z2-R-PACK-036, Z2-R-PACK-079, Z2-R-PACK-080, Z2-R-PACK-103, Z2-R-PACK-104, Z2-R-PACK-105, Z2-R-PACK-106, Z2-R-PACK-114, Z2-R-PACK-115, Z2-R-PACK-116, Z2-R-PACK-117, Z2-R-PACK-118, Z2-R-PACK-119, Z2-R-PACK-145, Z2-R-PACK-167, Z2-R-PACK-176, Z2-R-PACK-182, Z2-R-PACK-187, Z2-R-PACK-188, Z2-R-PACK-189 |
| Z2-I-PACK-12 | update compatibility | Z2-R-PACK-013, Z2-R-PACK-015, Z2-R-PACK-096, Z2-R-PACK-104, Z2-R-PACK-107, Z2-R-PACK-109, Z2-R-PACK-110, Z2-R-PACK-111, Z2-R-PACK-112, Z2-R-PACK-117, Z2-R-PACK-118, Z2-R-PACK-120, Z2-R-PACK-121, Z2-R-PACK-122, Z2-R-PACK-123, Z2-R-PACK-124, Z2-R-PACK-168, Z2-R-PACK-169, Z2-R-PACK-171, Z2-R-PACK-177, Z2-R-PACK-184 |
| Z2-I-PACK-13 | interrupted-update recovery | Z2-R-PACK-002, Z2-R-PACK-003, Z2-R-PACK-004, Z2-R-PACK-006, Z2-R-PACK-007, Z2-R-PACK-008, Z2-R-PACK-017, Z2-R-PACK-045, Z2-R-PACK-097, Z2-R-PACK-119, Z2-R-PACK-124, Z2-R-PACK-125, Z2-R-PACK-126, Z2-R-PACK-127, Z2-R-PACK-128, Z2-R-PACK-129, Z2-R-PACK-130, Z2-R-PACK-131, Z2-R-PACK-132, Z2-R-PACK-133, Z2-R-PACK-134, Z2-R-PACK-135, Z2-R-PACK-136, Z2-R-PACK-137, Z2-R-PACK-138, Z2-R-PACK-139, Z2-R-PACK-140, Z2-R-PACK-141, Z2-R-PACK-142, Z2-R-PACK-143, Z2-R-PACK-144, Z2-R-PACK-145, Z2-R-PACK-156, Z2-R-PACK-157, Z2-R-PACK-164, Z2-R-PACK-165, Z2-R-PACK-166, Z2-R-PACK-167, Z2-R-PACK-168, Z2-R-PACK-174, Z2-R-PACK-175, Z2-R-PACK-188, Z2-R-PACK-189, Z2-R-PACK-190 |
| Z2-I-PACK-14 | interrupted content installation: PAD pack download interrupted, then resumed | Z2-R-PACK-063, Z2-R-PACK-064, Z2-R-PACK-065, Z2-R-PACK-074, Z2-R-PACK-075, Z2-R-PACK-081, Z2-R-PACK-146, Z2-R-PACK-147, Z2-R-PACK-153, Z2-R-PACK-154 |
| Z2-I-PACK-15 | interrupted content installation: rolled back | Z2-R-PACK-064, Z2-R-PACK-065, Z2-R-PACK-073, Z2-R-PACK-148, Z2-R-PACK-149, Z2-R-PACK-150, Z2-R-PACK-153, Z2-R-PACK-155, Z2-R-PACK-172, Z2-R-PACK-179 |
| Z2-I-PACK-16 | interrupted content installation: the game left in a consistent state | Z2-R-PACK-007, Z2-R-PACK-009, Z2-R-PACK-033, Z2-R-PACK-036, Z2-R-PACK-042, Z2-R-PACK-065, Z2-R-PACK-068, Z2-R-PACK-072, Z2-R-PACK-073, Z2-R-PACK-078, Z2-R-PACK-083, Z2-R-PACK-084, Z2-R-PACK-085, Z2-R-PACK-087, Z2-R-PACK-101, Z2-R-PACK-116, Z2-R-PACK-133, Z2-R-PACK-134, Z2-R-PACK-138, Z2-R-PACK-141, Z2-R-PACK-142, Z2-R-PACK-144, Z2-R-PACK-149, Z2-R-PACK-151, Z2-R-PACK-152, Z2-R-PACK-153, Z2-R-PACK-165, Z2-R-PACK-166, Z2-R-PACK-171, Z2-R-PACK-172, Z2-R-PACK-173, Z2-R-PACK-179, Z2-R-PACK-180, Z2-R-PACK-181, Z2-R-PACK-183, Z2-R-PACK-190 |

## Excluded and deferred

No PACK inventory item is excluded or deferred: all sixteen are required, at Stage 2 (Z2-I-PACK-01, -03), Stage 4 (Z2-I-PACK-02, -04) and Stage 6 (the rest). Scope boundaries recorded elsewhere, with their decisions:

| Boundary | Classification | Decision |
|---|---|---|
| a custom content downloader | excluded | Z2-D-027 (alternative 2 rejected) |
| third-party content and mod packages | excluded | Z2-D-075 |
| console packaging | deferred to Stage 7, owned by SCOPE | Z2-D-071 |
| intra-file binary delta patches | not provided; patches are entry-granular | the design ruling R4 of `Zenith2/Docs/Design/PACK.md` → "Design rulings PACK records" |
