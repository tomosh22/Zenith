# PACK design: formats and installation layouts

This file is part of the PACK design family; the entry document is `Zenith2/Docs/Design/PACK.md`. It specifies the binary format registry, every file format PACK owns (with its versioned header, fields, endianness, checks and limits), the archive and mount-manifest rules PACK applies when it writes formats other areas own, the installation layouts on Windows and Android, and the JSON documents of the packaging tool side.

## Scope of this file

| Topic | Heading |
|---|---|
| every versioned binary format of Zenith2 in one table | "Binary format registry" |
| rules every PACK binary format follows | "Common rules for PACK formats" |
| `package.zpkm` | "Package manifest" |
| `update.zman` and `dlc.zman` | "Update manifest" |
| `install-journal.zinj` | "Install journal" |
| `install-state.zins` | "Install state file" |
| `install-record.zinr` | "Install record" |
| `delivery-state.zdls` | "Delivery state file" |
| the replay payload of `Package.*` events | "Package event payload" |
| the `.zpak` archives PACK writes (ASSET's layout) | "Archives PACK writes" |
| the shipped `mount-manifest.json` (PLAT's schema) | "Mount manifests PACK writes" |
| Windows and Android trees | "Installation layouts" |
| `package.recipe.json` | "Packaging recipe" |
| `size-report.json`, `partition.json` | "Size report and partition report" |
| patch and DLC bundles | "Patch and DLC bundles" |
| bundletool inputs | "Bundle configuration and device specs" |
| `format-registry.json` | "Format registry file" |
| `verified.zinv` | "Verified stamp file" |

## Binary format registry

<!-- elaboration -->
Z2-I-PACK-01 ("versioned binary formats") is realised as one registry of every versioned binary format in Zenith2, kept by PACK in `Zenith2/Tools/Package/format-registry.json` (schema in "Format registry file") and mirrored by the table below. Each owner specifies its format in its own design and adds its registry row in the diff that introduces the format; PACK owns the registry, its validation test and the compatibility classes, never another owner's layout. The compatibility class decides what an update may do with a file of the format (Algorithms.md "Update compatibility rules").

| Magic | Extension | Owner | Version | Compatibility class | Specified in |
|---|---|---|---|---|---|
| the kind's magic (`ZTEX`, …; ASSET's kind table) | per kind (`.ztex`, …) | ASSET (layout), each kind owner (sections) | 1 | `recook` | `Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file" |
| `ZCAT` | `.zcat` | ASSET | 1 | `recook` | `Zenith2/Docs/Design/ASSET/Formats.md` → "Asset catalogue file" |
| `ZPAK` | `.zpak` | ASSET (layout), PACK (writer) | 1 | `recook` | `Zenith2/Docs/Design/ASSET/Formats.md` → "Archive file"; "Archives PACK writes" |
| `ZDCT` | `.zdict` | PACK (training, format), ASSET (loader) | 1 | `recook` | `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_BuiltInAssets header" |
| `ZPKM` | `.zpkm` | PACK | 1 | `recook` | "Package manifest" |
| `ZMAN` | `.zman` | PACK (layout, writer), PLAT and SEC (verification) | 1 | `signed-update` | "Update manifest" |
| `ZINJ` | `.zinj` | PACK | 1 | `transient` | "Install journal" |
| `ZINS` | `.zins` | PACK | 1 | `user-state` | "Install state file" |
| `ZINR` | `.zinr` | PACK | 1 | `user-state` | "Install record" |
| `ZDLS` | `.zdls` | PACK | 1 | `user-state` | "Delivery state file" |
| `ZINV` | `.zinv` | PACK | 1 | `user-state` | "Verified stamp file" |
| `ZSAV` | `.zsave` | SAVE | SAVE's | `migrate` | `Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSAV file" |
| `ZSJN` | SAVE's | SAVE | SAVE's | `transient` | `Zenith2/Docs/Design/SAVE/Formats.md` |
| `ZRPL` | SAVE's | SAVE (writer; the record layout is Architecture's) | Architecture's | `migrate` | `Zenith2/Docs/Design/SAVE/Formats.md` |
| `ZSTW` | STREAM's | STREAM | STREAM's | `recook` | `Zenith2/Docs/Design/STREAM/Formats.md` |

The table lists the formats that exist when PACK's design is written; the registry file is the authority and grows by the owners' diffs. PACK's first Stage 2 ticket creates the file with `"formats": []`, because the registry check requires every row's `refusalTest` to exist and no format's refusal test exists at Stage 2, PACK's own included. Each row joins the file in the diff of the ticket that introduces its format and its refusal test, PACK's exactly as every other owner's: `ZPKM` with the Stage 4 manifest ticket (`Test_PackageManifest_Parse_OtherVersion_IsRefused`); `ZINJ`, `ZINS`, `ZINR`, `ZDLS` and `ZINV` with their Stage 6 format tickets (`Test_InstallJournal_OtherVersion_IsRefused`, `Test_InstallState_OtherVersion_IsRefused`, `Test_InstallRecord_OtherVersion_IsRefused`, `Test_DeliveryStateFile_OtherVersion_IsRefused`, `Test_VerifiedStampFile_OtherVersion_IsRefused`); `ZMAN` with PLAT's verifier ticket, whose decoder refuses another version; `ZDCT` with ASSET's dictionary-loader ticket, whose loader refuses another version. Every other owner's row (ASSET's cooked file, catalogue and archive, SAVE's, STREAM's, SCRIPT's bytecode container, ECS's schema blob, RPROD's shader containers and pipeline caches, AUDIO's banks, ANIM's clips, ENV's tiles, GEOM's kinds, COMPRESS's descriptor) joins the file in the diff of its owner's ticket that introduces the format and its refusal test (cross-area row PK1); a design whose magic collides with an existing row is in conflict and changes its magic.

| Compatibility class | Meaning for packaging and updates |
|---|---|
| `recook` | cooked content: re-cooked on any engine change, refused by its reader at a different version; a patch may replace a file of this class only within the release line it was cooked for (`Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`) |
| `signed-update` | authenticated update metadata; never inside an archive; verified before anything it lists is used |
| `migrate` | player data: never shipped in a package or an update, migrated forward by its owner |
| `user-state` | engine bookkeeping on the player's machine (`user:/cache/`, and the install root's `.zenith2-install/`); never shipped; a reader refusing it rebuilds it (deleting a file of this class is always safe) |
| `transient` | recovery state of an operation in progress; never shipped; deleted when the operation ends |

## Common rules for PACK formats

The rules of `Zenith2/Docs/Design/ASSET/Formats.md → "Common rules"` apply to every PACK binary format, restated here with PACK's specifics:

- Little-endian; written and read field by field by an encoder and a decoder that copy no C++ object's memory (Z2-D-038).
- A four-byte ASCII magic then a `uint16` format version at offsets 0 and 4; every reader refuses another version with a recoverable error, one per format: `package.zpkm` with `PACKAGE_ERROR_MANIFEST_VERSION` (M1b), `install-journal.zinj` with `PACKAGE_ERROR_JOURNAL_VERSION`, and the user-state files, whose stale versions are rebuilt, with their own corruption error: `install-record.zinr` with `PACKAGE_ERROR_RECORD_CORRUPT` (R2), `install-state.zins`, `delivery-state.zdls` and `verified.zinv` with `PACKAGE_ERROR_STATE_FILE_CORRUPT` (S2, D2, VS2). The update manifests' version is check U1 (`UPDATE_MANIFEST_INVALID`).
- Every size, count and offset is checked against the input length with overflow-checked arithmetic before use; every reserved field is zero, and a non-zero one is refused.
- A "check" is the low 32 bits of `Zenith_Hash64` over the whole file with the check field zeroed (D-SEL-HASH (open, Stage 1)); it detects accidents only. Integrity against an attacker is the SHA-256 of `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Sha256"`, recorded in a manifest that is either signed (`update.zman`) or itself inside a verified mount (`package.zpkm`).
- Strings live in one string table per file, referenced by (offset, length), valid UTF-8, never NUL-terminated; names follow the name rules of "Name rules".
- Every PACK parser has a libFuzzer target under `Zenith2/Engine/ZenithPackage/Tests/Fuzz/` (`Zenith2/Docs/Architecture.md → "Security architecture"`), and its limits are constants tested at the limit and at limit + 1.

### Name rules

| Name | Rule | Example |
|---|---|---|
| unit and pack | `^[a-z][a-z0-9_]{0,63}$` (Play asset-pack names allow letters, digits and underscores; the mount manifest's rule) | `world_textures` |
| archive | `<unit>.zpak` for a one-archive unit, `<unit>-<NN>.zpak` (`NN` 00–99) otherwise; for a patch `patch-<NN>.zpak` | `world_textures-03.zpak` |
| entitlement | `^[a-z][a-z0-9_.]{0,63}$` | `dlc.expansion_isles` |
| DLC | as unit | `expansion_isles` |
| update version | `MAJOR.MINOR.PATCH`, each 0–65,535 without a leading zero (the `game.json` rule) | `1.2.3` |

## Package manifest

`content:/Package/package.zpkm` lives in the base unit's first archive (`base.zpak` or `base-00.zpak`). It is the cooked-content hash record of [Z2-N-ARCH-956]: every deliverable archive with its byte count and SHA-256, so a delivered asset pack is checked before it is mounted. It is cooked data (class `recook`), regenerated by every package build, and written once per installation layout, because the three layouts deliver different unit sets from one cooked tree:

| Layout | Written into | Units it lists | Units with the PRESENT flag |
|---|---|---|---|
| `PACKAGE_LAYOUT_WINDOWS` (1) | `Content/base[-NN].zpak` | every unit of the recipe | every non-DLC unit (folded into `Content/`) |
| `PACKAGE_LAYOUT_ANDROID_SIDELOAD` (2) | `sideload/assets/base[-NN].zpak` | every non-DLC unit (DLC units are absent from a sideload APK) | every listed unit |
| `PACKAGE_LAYOUT_ANDROID_BUNDLE` (3) | `bundle/base/assets/base[-NN].zpak` | every unit of the recipe | the base unit and every INSTALL_TIME unit |

So an Android build has two base archives, the sideload one and the bundle one; their entries differ only in `Package/package.zpkm`. A unit without the PRESENT flag in the running layout is either deliverable (FAST_FOLLOW, ON_DEMAND or DLC in the bundle layout: the installer's records) or a Windows DLC directory (DLC in the Windows layout). Nothing at runtime reads the mount manifest to decide deliverability.

`package.zpkm` always describes the release's base package: a content patch never ships it ("Patch diff" in Algorithms.md skips it), so on a patched install it still names the base package's content hash, and every delivered pack is verified against it.

### Package manifest header

64 bytes at offset 0:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZPKM` |
| 4 | 2 | format version | 1 |
| 6 | 1 | layout | `Zenith_PackageLayout` 1 … 4 (4, SIMULATED, in development and test packages only: Algorithms.md "Simulated deliverable units") |
| 7 | 1 | reserved | 0 |
| 8 | 4 | unit count U | 1 … 4,096 |
| 12 | 4 | archive count A | 1 … 65,536 |
| 16 | 4 | string table bytes S | ≤ 1,048,576 |
| 20 | 4 | engine cook version | `uZENITH_ENGINE_COOK_VERSION` of the cook |
| 24 | 16 | build ID | the 16-byte build ID of the packaged executable (`Zenith2/Docs/Design/SHIP/Release.md → "Symbols"`) |
| 40 | 8 | content hash | the base catalogue's content hash (`Zenith2/Docs/Design/ASSET/Formats.md → "Asset catalogue file"`), equal to the mount manifest's `contentHash` |
| 48 | 2 | release major | `game.json` `version` MAJOR |
| 50 | 2 | release minor | `game.json` `version` MINOR |
| 52 | 4 | header check | over the whole file with this field zero |
| 56 | 8 | total bytes | the file size; 64 … 5,374,016 (`ulPACKAGE_MAX_MANIFEST_BYTES` = 64 + 32 × 4,096 + 64 × 65,536 + 1,048,576) |

### Package manifest tables

The unit table (U × 32 bytes) starts at offset 64, the archive table (A × 64 bytes) at `64 + 32 U`, the string table (S bytes) at `64 + 32 U + 64 A`; the file ends at the string table's end.

Unit entry:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | name offset | into the string table |
| 4 | 1 | name length | 1 … 64 |
| 5 | 1 | kind | `Zenith_PackageUnitKind` 1 … 5 |
| 6 | 1 | flags | bit 0: the unit carries its own catalogue fragment (`Catalogues/<unit>.zcat`); bit 1: entitlement-gated; bit 2: PRESENT, its archives are mounted with the package in this layout (from the mount manifest's `package[]` or an `installTime` entry of `assetPacks[]`) and it has no installer record; bits 3–7 0 |
| 7 | 1 | entitlement length | 0 … 64; non-zero exactly when flag bit 1 is set |
| 8 | 4 | entitlement offset | 0 when the length is 0 |
| 12 | 4 | first archive index | |
| 16 | 4 | archive count | 1 … 100 |
| 20 | 4 | mount order | `sint32`, −1,000,000 … 1,000,000 |
| 24 | 8 | unit bytes | the sum of its archives' bytes; 0 for the base unit |

Archive entry:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | name offset | |
| 4 | 2 | name length | 6 … 255 |
| 6 | 2 | reserved | 0 |
| 8 | 4 | unit index | the owning unit |
| 12 | 4 | reserved | 0 |
| 16 | 8 | bytes | the archive's size, ≤ 64 GiB (`ulASSET_MAX_ARCHIVE_BYTES`); 0 for every archive of the base unit |
| 24 | 32 | SHA-256 | of the archive's bytes; all zero for every archive of the base unit |
| 56 | 8 | reserved | 0 |

The base unit's archives are recorded with zero bytes and an all-zero digest because the manifest lives inside the first of them and cannot record that archive's own size or digest; they are the archives PLAT mounts from the package and PACK never verifies, and their real sizes and digests are in `size-report.json`. Every other archive is written before `package.zpkm` and recorded with its real size and digest ("Package build steps" in Tooling.md).

### Package manifest checks

`Zenith_PackageManifest::Parse` applies these in order and fails on the first, logging the check number:

| # | Check | Error |
|---|---|---|
| M1 | at least 64 bytes; magic; reserved fields zero; layout 1 … 4 | `MANIFEST_CORRUPT` |
| M1b | format version 1 | `MANIFEST_VERSION` |
| M2 | header check | `MANIFEST_CORRUPT` |
| M3 | U, A and S within their limits; `64 + 32U + 64A + S` = total bytes = the input length (overflow-checked) | `MANIFEST_CORRUPT` |
| M4 | every string within the table and valid UTF-8; unit names, entitlement IDs and archive names obey "Name rules"; an archive name is a valid single virtual-path component (rules R2–R9 of `Zenith2/Docs/Design/PLAT/Filesystem.md → "Validation rules and order"`) | `MANIFEST_CORRUPT` |
| M5 | unit 0 has kind BASE and no other unit does; unit names unique | `MANIFEST_CORRUPT` |
| M6 | the units' archive ranges are contiguous in unit order and cover `[0, A)` exactly; each archive's unit index names its range's unit | `MANIFEST_CORRUPT` |
| M7 | each non-base unit's bytes equal the sum of its archives' bytes; archive names unique across the file | `MANIFEST_CORRUPT` |
| M8 | a DLC unit has flag bits 0 and 1 set; every other unit has both clear | `MANIFEST_CORRUPT` |
| M9 | the base unit's mount order is 0; every other unit's order is greater than 0 | `MANIFEST_CORRUPT` |
| M10 | the base unit's bytes are 0, and every archive of the base unit has bytes 0 and an all-zero digest; every other archive's digest is not all zero | `MANIFEST_CORRUPT` |
| M11 | flag bit 2 by layout: WINDOWS, set exactly on every non-DLC unit; ANDROID_SIDELOAD, set on every unit and no unit is DLC; ANDROID_BUNDLE and SIMULATED, set exactly on the base unit and the INSTALL_TIME units | `MANIFEST_CORRUPT` |
| M12 | at most 256 units (`uPACKAGE_MAX_STATE_UNITS`) lack flag bit 2: the deliverable units and the Windows DLC units, the units a world's package state and snapshot describe (Algorithms.md "World snapshot"); a recipe has at most 255 units, so every manifest the packer writes passes | `MANIFEST_CORRUPT` |

`CheckIdentity(const Zenith_BuildIdentity&, Zenith_BuildPlatform, bool bCompareBuildID)` then compares the build ID with `Zenith_BuildIdentity::m_auBuildID` when `bCompareBuildID` is true (the package service passes its `m_bShippingRules`, so the build ID is compared exactly where PLAT compares a mount manifest's `buildId`, in Shipping builds), the content hash with `m_ulPackageContentHash` (the mount manifest's `contentHash`, which a mounted patch never changes, cross-area row PK14), and the layout with the platform (WINDOWS on Windows, ANDROID_SIDELOAD or ANDROID_BUNDLE on Android; SIMULATED only outside Shipping rules on a Windows or Linux host, Algorithms.md "Simulated deliverable units" item 4); a difference is `PACKAGE_ERROR_MANIFEST_IDENTITY`, which disables every delivery and DLC mount for the run and is logged as `package: package.zpkm identity <hex> does not match the mount manifest <hex>` (or `… layout <n> on <platform>`).

## Update manifest

`update.zman` (a patch, in `<patchRoot>/<MAJOR.MINOR.PATCH>/`) and `dlc.zman` (a Windows DLC, in `DLC/<name>/`) share one format with a kind field. PACK owns the layout and writes it (`Zenith2/Docs/Design/PACK/Tooling.md → "Update manifest writer"`); PLAT verifies it through `Zenith_VerifyUpdateManifest` (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Patch mount verification"`) under SEC's rules, and PACK's applier verifies a bundle with the same function before it commits it. This section is the layout `Zenith2/Docs/Architecture.md → "Security architecture"` ("Authenticated update manifests") describes.

### Update manifest header

96 bytes at offset 0:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZMAN` |
| 4 | 2 | format version | 1 |
| 6 | 2 | kind | 1 `PATCH`, 2 `DLC` |
| 8 | 4 | file count F | 1 … 65,536 |
| 12 | 4 | string table bytes S | ≤ 1,048,576 |
| 16 | 16 | target build ID | PATCH: the build the patch applies to; DLC: all zero (any build of the release line) |
| 32 | 2 | release major | the release line |
| 34 | 2 | release minor | |
| 36 | 2 | version major | equal to the release major |
| 38 | 2 | version minor | equal to the release minor |
| 40 | 2 | version patch | PATCH: 1 … 65,535, the patch version; DLC: the DLC content revision, 1 … 65,535 |
| 42 | 2 | reserved | 0 |
| 44 | 4 | engine cook version | the cook version of every file listed |
| 48 | 8 | base content hash | PATCH: the content hash of the release line's base package; DLC: 0 |
| 56 | 8 | result content hash | PATCH: the content hash of the catalogue the patch ships (the effective content hash once mounted); DLC: the content hash of the DLC's catalogue fragment |
| 64 | 4 | name offset | DLC: its name; PATCH: 0 |
| 68 | 1 | name length | DLC: 1 … 64; PATCH: 0 |
| 69 | 3 | reserved | 0 |
| 72 | 4 | mount order | `sint32`; DLC: the order of its first archive (1,000 … 1,000,000); PATCH: 0 |
| 76 | 4 | reserved | 0 |
| 80 | 8 | total bytes | the file size, signature included |
| 88 | 8 | reserved | 0 |

### Update manifest tables and signature

The file table (F × 64 bytes) starts at offset 96, the string table at `96 + 64 F`, and the 64-byte Ed25519 signature occupies the last 64 bytes; `total bytes = 96 + 64 F + S + 64`.

| Offset in entry | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | path offset | |
| 4 | 2 | path length | 1 … 255 |
| 6 | 2 | flags | bit 0: an archive the mount adds as a source (the name ends `.zpak`); others 0 |
| 8 | 8 | bytes | ≤ 64 GiB |
| 16 | 32 | SHA-256 | of the file's bytes |
| 48 | 16 | reserved | 0 |

- Paths are single file names inside the bundle directory, sorted by byte order, unique; the manifest does not list itself.
- Archive entries appear in mount order: the patch mount and the DLC mount add sources in file-table order (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Patch mount verification"`, step 3).
- The signature is pure Ed25519 (RFC 8032, no context string, no pre-hash) over bytes `[0, total − 64)` with the release private key; the public key is the 32-byte `constexpr` key compiled into the Player ([Z2-N-ARCH-955]).

### Update manifest checks

| # | Check | Where | Error (applier) |
|---|---|---|---|
| U0 | the bundle directory holds exactly the manifest and the files the manifest lists, each a regular file, in two halves: (a) no subdirectory, no link (`m_bLink`), no name that is neither the manifest nor listed; (b) every listed name present; checked after U3 (the listed names are trusted only once the signature verifies) and before U9 (no unlisted byte is ever hashed) | applier (B4 of "Install steps per operation" in Algorithms.md); Windows DLC mount ("Windows DLC mounting" step 1, through the install file system's `List`) | (a) `UPDATE_MANIFEST_INVALID`, a refusal; (b) `UPDATE_INCOMPLETE`: the applier leaves the bundle in place for a later start, the DLC mount leaves the DLC unmounted |
| U1 | at least 225 bytes (96 + 64 × 1 + 1 + 64: one file entry, a one-byte path, the signature); magic; version 1; kind 1 or 2; reserved fields zero | verifier (`Zenith_DecodeUpdateManifest`) | `UPDATE_MANIFEST_INVALID` |
| U2 | total bytes = the input length; `96 + 64F + S + 64` = total (overflow-checked); F and S within limits | verifier | `UPDATE_MANIFEST_INVALID` |
| U3 | the signature verifies against the compiled-in key | verifier, before any further field is interpreted | `UPDATE_MANIFEST_INVALID` |
| U4 | the release line equals the running build's | verifier (`Zenith_CheckUpdateManifestTarget`) | `UPDATE_WRONG_TARGET` |
| U5 | PATCH: the target build ID equals the running build's; DLC: the target build ID is all zero | verifier | `UPDATE_WRONG_TARGET` |
| U6 | the engine cook version equals the running build's `uZENITH_ENGINE_COOK_VERSION` | PACK (applier, DLC mount) | `UPDATE_WRONG_TARGET` |
| U7 | strings valid; paths valid single components, sorted, unique; flags consistent with the name | verifier (`Zenith_DecodeUpdateManifest`) | `UPDATE_MANIFEST_INVALID` |
| U8 | PATCH: version major.minor equals the release line and the directory name equals `MAJOR.MINOR.PATCH`; DLC: the name equals the directory name | verifier (`Zenith_CheckUpdateManifestTarget`; PLAT's patch mount, cross-area row PK2) | `UPDATE_MANIFEST_INVALID` |
| U9 | every listed file exists with its size and SHA-256; a file whose identity stamp equals its `verified.zinv` record for the same manifest digest is not hashed (Algorithms.md "Verified cache" item 4); at B6, a file whose stamp changed while it was hashed is left in place, not refused | verifier (`Zenith_VerifyUpdateManifestFiles`) | `UPDATE_FILE_MISMATCH` (hash or size); `UPDATE_INCOMPLETE` (stamp changed during B6) |
| U10 | PATCH: the version is greater than every committed `Patches/<w>` of the running line and greater than the install state's highest patch of the running line ("Install state file"); DLC: the revision is greater than or equal to the installed revision of `DLC/<name>` | applier | `UPDATE_NOT_NEWER` |
| U11 | PATCH: the base content hash equals the base package's content hash (the mount manifest's `contentHash`): checked by the applier, which examines no patch bundle while `m_ulPackageContentHash` is 0 (the bridge read no `Content/mount-manifest.json`; every patch bundle is then left in place, Algorithms.md "Pending installs at startup" step 8), and always by PLAT's patch mount (cross-area row PK14) | applier, verifier | `UPDATE_WRONG_TARGET` |

U10 runs at startup step 4, before the virtual filesystem exists, so it reads only the install tree: the committed version directories and `.zenith2-install/install-state.zins`, never `user:/cache/package/install-record.zinr` (which on Windows is under `SDL_GetPrefPath`, outside the install root). A replayed older signed patch ([Z2-N-ARCH-960]) is therefore refused even after every newer version directory has been removed, for as long as the install state survives; a refused or deleted install state (class `user-state`) resets the floor to the committed directories alone, and the install record's DOWNGRADE classification still reports such a downgrade at the close of startup step 11.

<!-- assumption: pending PLAT, SEC -->
`Zenith_VerifyUpdateManifest` is PLAT's function under SEC's rules; PACK assumes a public declaration in `Zenith2/Engine/ZenithPlatform/Zenith_UpdateManifest.h` that returns `Zenith_Result<Zenith_UpdateManifestInfo, Zenith_UpdateVerifyError>`, where `Zenith_UpdateManifestInfo` carries every header field above and a span of the file entries, accepts both kinds (U5's DLC rule), checks U8 for PATCH against the directory name it is given, and that `Zenith_UpdateVerifyError` distinguishes U1–U3 and U7 (format or signature), U4, U5 and U11 (target) and U9 (files). PACK also assumes its three public halves, `Zenith_DecodeUpdateManifest` (U1–U3, U7), `Zenith_CheckUpdateManifestTarget` (U4, U5, U8) and `Zenith_VerifyUpdateManifestFiles` (U9), so the applier can run U0 between the signature and the hashing, and the obsolete sweep (O2) can read a manifest's target build without its files (cross-area rows PK2, PK14). PACK maps those errors to the applier errors in this table. From cross-area row PK27 (Q90 ruled (a)), PACK assumes: `struct Zenith_FileIdentityStamp { uint64 m_ulBytes; uint64 m_ulLastWriteNs; uint64 m_ulChangeNs; uint64 m_ulVolume; uint8 m_auFileID[16]; }` (48 bytes, `Zenith2/Engine/ZenithPlatform/Zenith_FileIdentity.h`, equality is member-wise); `virtual bool Zenith_MountSource::GetIdentityStamp(Zenith_StringView strRelative, Zenith_FileIdentityStamp& xOut) const` (default false, "hash it"; PLAT's directory source implements it with the calls of Algorithms.md "Verified cache" item 1 and answers false on a volume outside the supported set); the interface `class Zenith_UpdateStampLookup { public: virtual ~Zenith_UpdateStampLookup() = default; virtual bool FindStamp(Zenith_StringView strDirectory, Zenith_ByteSpan xManifestSha256, Zenith_StringView strFile, Zenith_FileIdentityStamp& xOut) const = 0; }` in `Zenith_UpdateManifest.h`; `Zenith_VerifyUpdateManifestFiles` taking `const Zenith_UpdateStampLookup* pxLookup` (null: hash every file), the directory's install-relative name and the manifest's SHA-256, skipping the hash of a listed file whose current stamp equals the lookup's record in all five fields, reporting the number skipped, reporting a stamp that changed during a file's hash as `UPDATE_VERIFY_ERROR_FILE_CHANGED` (which B6 maps to `UPDATE_INCOMPLETE`), and applying the 100 ms racy-timestamp rule; and `Zenith_VirtualFileSystemDesc::m_pxUpdateStamps`, which the patch mount at startup step 5 passes to it, read only during `Create` and never stored.
<!-- /assumption -->

## Install journal

`<root>/.zenith2-install/install-journal.zinj`, where `<root>` is the install file system's root (Windows `<install>`, Android the internal storage root). One journal describes the one operation in progress; its absence means none is. It is class `transient`. Installation never copies bundle bytes: a bundle is verified read-only where it was delivered and then renamed into place, so the journal records directory moves, never file progress (Algorithms.md "Install state machine").

### Install journal header

64 bytes:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZINJ` |
| 4 | 2 | format version | 1 |
| 6 | 1 | phase | `Zenith_InstallPhase` 1 … 5 |
| 7 | 1 | target kind | `Zenith_InstallTargetKind` 1 … 2 |
| 8 | 8 | generation | g0 + M when the journal is first written, incremented at every rewrite; g0 is one more than the greatest decimal `.trash/<g>` name under both kind roots (Algorithms.md "Recovery table", the generation bullet) |
| 16 | 4 | move count M | 0 … 1,024 (0: a first patch or a first DLC install moves nothing out) |
| 20 | 4 | string table bytes S | ≤ 65,536 |
| 24 | 2 | version major | the bundle's update version (DLC: release line and revision) |
| 26 | 2 | version minor | |
| 28 | 2 | version patch | |
| 30 | 1 | flags | bit 0: the old DLC revision has been moved out (DLC COMMITTING only); others 0 |
| 31 | 1 | rejection error | the `Zenith_PackageError` that refused the bundle (REJECTING), else 0 |
| 32 | 4 | name offset | the target directory name (`1.2.3` or the DLC name) |
| 36 | 1 | name length | 1 … 64 |
| 37 | 3 | reserved | 0 |
| 40 | 8 | verified bytes | the bundle bytes hashed by verification |
| 48 | 8 | reserved | 0 |
| 56 | 4 | header check | over the whole file with this field zero |
| 60 | 4 | reserved | 0 |

### Install journal records

M move records of 24 bytes follow the header, then the string table; the file ends at the string table's end. A move record describes one directory that the operation moves to `.trash` (an obsolete patch version, the old DLC revision, a refused bundle, a removed DLC), or a trash tree an earlier operation left (rule O4 of Algorithms.md "Obsolete directories": its path is `Patches/.trash/<g>` or `DLC/.trash/<g>`, its state TRASHED from the start and its trash generation `g`):

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | path offset | the directory's path relative to the root (`Patches/1.2.1`, `DLC/expansion_isles`, `Patches/.incoming/1.2.4`) |
| 4 | 2 | path length | 1 … 255 |
| 6 | 1 | state | 1 `PENDING` (still at its path), 2 `TRASHED` (renamed to its trash name), 3 `DELETED` (its trash tree removed) |
| 7 | 1 | reserved | 0 |
| 8 | 8 | trash generation | the directory's trash name is `<root of its kind>/.trash/<generation>` (decimal, no leading zero) |
| 16 | 8 | reserved | 0 |

Checks J1–J6, failing with `PACKAGE_ERROR_JOURNAL_CORRUPT` (or `JOURNAL_VERSION` for J1's version): J1 at least 64 bytes, magic, version 1, reserved zero; J2 header check; J3 M (0 … 1,024) and S within limits and `64 + 24M + S` = the input length; J4 phase and target kind in range, flags only in DLC COMMITTING, rejection error non-zero exactly in REJECTING; J5 strings valid, every path's components valid by "Name rules" or one of `.incoming`, `.trash`, every path under `Patches/` (the mount manifest's `patchRoot`) or `DLC/`; J6 every state in 1 … 3 and the records' trash generations pairwise distinct across both kind roots (a `Patches/` record and a `DLC/` record never share one). A corrupt journal is never trusted: recovery rebuilds the operation from the directories present (Algorithms.md "Recovery table", row "journal corrupt").

Every journal rewrite is one durable step: write `install-journal.zinj.tmp`, `FlushFile`, `Rename(tmp, journal, replace)`, `FlushDirectory(.zenith2-install)` (Algorithms.md "Durable step rules").

## Install state file

`<root>/.zenith2-install/install-state.zins`, exactly 48 bytes, where `<root>` is the install file system's root. The applier writes it with the "write the install state" durable step (Algorithms.md "Durable step rules") and reads it at startup step 4, before the virtual filesystem exists, so it is the replay floor of U10 and the record of the last obsolete sweep. Class `user-state`: a refused or missing file runs the sweep and leaves U10 with the committed version directories as its only floor, both always safe.

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZINS` |
| 4 | 2 | format version | 1 |
| 6 | 2 | reserved | 0 |
| 8 | 2 | release major | the release line the next two fields describe |
| 10 | 2 | release minor | |
| 12 | 2 | highest patch | the highest patch version committed in that release line on this install, across every build of the line (the numbering rule of Algorithms.md "Update classification" makes it a valid floor for every build); 0 when none |
| 14 | 2 | reserved | 0 |
| 16 | 16 | swept build ID | the build ID whose obsolete sweep (O1–O4 of Algorithms.md "Obsolete directories") last completed with nothing deferred; all zero when none, or when the last pass deferred obsolete directories (which forces the next start's sweep) |
| 32 | 8 | reserved | 0 |
| 40 | 4 | check | over the 48 bytes with this field zero |
| 44 | 4 | reserved | 0 |

Checks S1 (exactly 48 bytes, magic), S2 (version 1), S3 (check), S4 (reserved zero); a failure is `PACKAGE_ERROR_STATE_FILE_CORRUPT`, logged as `package: install state refused (<check>)`, and the state is treated as absent. A state whose release line differs from the running build's has, for the running line, highest patch 0 and no swept build; the pass rewrites it with the running line.

## Install record

`user:/cache/package/install-record.zinr`, 192 bytes, written with `Zenith_AsyncIO::WriteAtomic` (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Atomic writes"`). Class `user-state`: a missing or refused record classifies the run as `UPDATE_KIND_FIRST_RUN`, which is always safe.

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZINR` |
| 4 | 2 | format version | 1 |
| 6 | 1 | last update kind | `Zenith_UpdateKind` 1 … 6 |
| 7 | 1 | migration phase | `Zenith_MigrationPhase` 1 … 2 |
| 8 | 64 | identity | "Identity encoding" |
| 72 | 64 | previous identity | "Identity encoding"; all zero after a first run |
| 136 | 2 | highest patch seen | the highest patch version mounted in the identity's release line |
| 138 | 6 | reserved | 0 |
| 144 | 8 | run count | |
| 152 | 32 | reserved | 0 |
| 184 | 4 | record check | over the 192 bytes with this field zero |
| 188 | 4 | reserved | 0 |

Checks R1 (exactly 192 bytes, magic), R2 (version 1), R3 (check), R4 (kinds and phases in range), R5 (reserved zero); any failure is `PACKAGE_ERROR_RECORD_CORRUPT`. Release line 0.0 is valid (the `game.json` version rule admits `0.0.x`), so neither the record nor the install state refuses a zero line; an all-zero file fails the magic check.

### Identity encoding

| Offset | Size | Field |
|---|---|---|
| 0 | 16 | build ID |
| 16 | 2 | release major |
| 18 | 2 | release minor |
| 20 | 2 | patch version |
| 22 | 2 | reserved (0) |
| 24 | 4 | engine cook version |
| 28 | 8 | content hash |
| 36 | 8 | composition hash |
| 44 | 20 | reserved (0) |

## Delivery state file

`user:/cache/package/delivery-state.zdls`, written with `Zenith_AsyncIO::WriteAtomic` after every change of a pack's intent or verification record (at most once per frame). Class `user-state`: refusing it makes the installer re-query Play and re-verify every delivered pack, which is always safe.

### Delivery state header

32 bytes:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZDLS` |
| 4 | 2 | format version | 1 |
| 6 | 2 | pack count P | 0 … 256 |
| 8 | 4 | stamp count T | 0 … 25,600 (256 packs × 100 archives); the sum of the verified records' stamp counts |
| 12 | 4 | string table bytes S | ≤ 16,384 |
| 16 | 8 | sequence | incremented at every write |
| 24 | 4 | file check | over the whole file with this field zero |
| 28 | 4 | reserved | 0 |

### Delivery state records

P pack records of 80 bytes, then T stamp records of 48 bytes, then the string table:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | name offset | |
| 4 | 1 | name length | 1 … 64 |
| 5 | 1 | intent | 1 `NONE`, 2 `WANT_INSTALLED` (a fetch was requested and not cancelled), 3 `WANT_REMOVED` (a removal was requested and has not completed) |
| 6 | 1 | verified | 1 when the fields below describe a delivery verified in full (Algorithms.md "Verified cache" item 3), else 0 |
| 7 | 1 | reserved | 0 |
| 8 | 8 | pack version | Play's pack version at verification |
| 16 | 8 | manifest content hash | `package.zpkm`'s content hash the pack was verified against |
| 24 | 32 | location digest | SHA-256 of the UTF-8 assets host path at verification |
| 56 | 4 | first stamp | index into the stamp table |
| 60 | 2 | stamp count | the unit's archive count, 1 … 100, when verified; else 0 |
| 62 | 2 | reserved | 0 |
| 64 | 8 | verified bytes | |
| 72 | 8 | reserved | 0 |

A stamp record is a `Zenith_FileIdentityStamp`, one per archive in the unit's archive order:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | bytes |
| 8 | 8 | last-write time, nanoseconds since the Unix epoch (`FileBasicInfo.LastWriteTime` converted; `st_mtim`) |
| 16 | 8 | change time, nanoseconds since the Unix epoch (`FileBasicInfo.ChangeTime` converted; `st_ctim`) |
| 24 | 8 | volume (`FileIdInfo.VolumeSerialNumber`; `st_dev`) |
| 32 | 16 | file identity (`FileIdInfo.FileId`, 128 bits; `st_ino` in the low 8 bytes, the high 8 bytes zero) |

The intent fields are used in every build; the verification fields (verified, pack version, manifest content hash, location digest, first stamp, stamp count, verified bytes and the stamp records) are the pack half of the verified cache of Algorithms.md "Verified cache" (Q90 ruled (a)). Checks D1–D5 fail with `PACKAGE_ERROR_STATE_FILE_CORRUPT`, and the installer then starts from an empty state (every pack re-verified in full): D1 size and magic; D2 version; D3 check; D4 counts and ranges: P ≤ 256, T ≤ 25,600, S ≤ 16,384, the length equals 32 + 80P + 48T + S, every intent 1 … 3, every verified byte 0 or 1, a record with verified 0 has every verification field 0, a record with verified 1 has a stamp count of 1 … 100 and a range [first stamp, first stamp + stamp count) inside T, the verified records' ranges are disjoint, in record order and cover exactly T, every reserved field 0; D5 strings and names (valid UTF-8, the name rules, names unique).

## Package event payload

Every `Package.*` event reaches a world as one `ASYNC_SOURCE_PACKAGE` result of publication kind `PUBLICATION_KIND_GENERIC_RECORDED` whose payload is these 96 little-endian bytes, written and read field by field by `Zenith_EncodePackageEvent` and `Zenith_DecodePackageEvent` (`Internal/Package/Zenith_PackageEvents.cpp`); the replay recorder records the payload as it records every recorded publication, so a replay publishes the same events at the same ticks without Play (Algorithms.md "Event publication and replay"):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `Zenith_PackageEventKind` |
| 1 | 1 | `Zenith_AssetPackState` (0 for kinds without a state) |
| 2 | 1 | error (`Zenith_PackageError` or 0) |
| 3 | 1 | detail: the `Zenith_UpdateKind` for UPDATED; the `Zenith_InstallTargetKind` for UPDATE_FAILED; the `Zenith_ConfirmationReason` for CONFIRMATION_REQUIRED; 0 otherwise |
| 4 | 6 | from version (3 × `uint16`) |
| 10 | 6 | to version (3 × `uint16`) |
| 16 | 8 | bytes done; for `DLC_STATE_CHANGED`, which has no download progress, the result content hash of the mounted revision's `dlc.zman` (offset 56, "Update manifest header") when the state is MOUNTED, else 0 (the replay check of Algorithms.md "Replay pack set" step 2) |
| 24 | 8 | bytes total |
| 32 | 64 | name, UTF-8, zero-padded |

A payload whose kind, state, error or detail byte is out of range, whose name is not valid UTF-8 or not zero-padded, or whose length is not 96 is refused by the decoder; the handler drops it and counts it (`package.events.decode_failed`).

## Archives PACK writes

The archive layout, its checks A1–A7 and its limits are ASSET's (`Zenith2/Docs/Design/ASSET/Formats.md → "Archive file"`); PACK's packer writes exactly that version-1 layout and owns only the packing policy:

| Policy | Rule |
|---|---|
| entries | every file of the unit's partition (cooked asset files, catalogue fragments, non-asset files of the base unit); never a file under a `.`-prefixed directory of the cooked tree (`.cook/`), never `mount-manifest.json` or `.stamp` |
| entry order | ascending path bytes, which the entry table requires; the data region is written in the same order, so a directory-adjacent group (one streaming cell's objects share a logical-path prefix but not an object path) is not contiguous; the order is fixed so the output is a function of the input set |
| alignment | an entry of at least 65,536 bytes starts at a multiple of 16,384 (the S24's page size, for aligned reads and a future mapped read); a smaller entry at a multiple of 16 (`uASSET_SECTION_ALIGNMENT`); padding bytes are zero |
| entry flags | 0 (stored): cooked files carry their own section compression, so archive entries stay byte-addressable for `Zenith_AssetReader::ReadRange` (AUDIO's streamed sounds, cross-area row AU21) |
| data hash | the entry's `Zenith_Hash128`, taken from the catalogue (`m_xOutputHash` for a cooked asset file, the file entry's hash for a non-asset cooked file) or, for a file the packer generates in memory (the base catalogue, a DLC fragment, `package.zpkm`, at most 64 MiB each), `Zenith_Hash128Of` of those bytes; never computed while streaming a file, because CORE has no streaming 128-bit hash; written into the table and re-checked by the read-back's `Zenith_VerifyArchiveData` ("Archive read-back" in Tooling.md), which fails `READBACK_MISMATCH` on a stale catalogue hash |
| determinism | no timestamp, no host path, no environment value enters an archive; two packs of the same inputs produce identical bytes |
| size | an archive is cut before the entry that would take it past the recipe's `chunkTargetBytes`; an entry larger than the target is alone in its archive; no archive exceeds `ulASSET_MAX_ARCHIVE_BYTES` and no entry `ulASSET_MAX_COOKED_FILE_BYTES` (`PACKAGE_BUILD_ERROR_ARCHIVE_LIMIT`) |

## Mount manifests PACK writes

PACK's packager writes the shipped `mount-manifest.json` in PLAT's schema (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount manifest schema"`) and validates it with `Zenith_ParseMountManifest` before publishing; the development manifest stays the cook's (`Zenith2/Docs/Design/ASSET/Cook.md → "Mount manifest"`).

| Key | Windows package | Android sideload APK | Android App Bundle |
|---|---|---|---|
| `game`, `releaseLine` | from `composition.json` and `game.json` | same | same |
| `buildId` | the packaged executable's build ID | `lib<Game>.so`'s | `lib<Game>.so`'s |
| `contentHash` | the base catalogue's content hash | same | same |
| `package[]` | every archive of every unit except DLC units, in (unit order, archive order), each unit's `order` its mount order | every archive of every non-DLC unit | the base unit's archives |
| `assetPacks[]` | absent | absent (everything is in the base `assets/`) | one object per INSTALL_TIME, FAST_FOLLOW, ON_DEMAND and DLC unit: `name`, `delivery` (`installTime`, `fastFollow`, `onDemand`; DLC units `onDemand`), `archives`, `order` |
| `patchRoot` | `Patches` | `Patches` | `Patches` |

## Installation layouts

### Windows install layout

```text
<install>/                                   any user-writable directory (the release zip extracted, or a Steam library)
	<Game>.exe  vcruntime140.dll …  D3D12/  Licences/          SHIP's files (Zenith2/Docs/Design/SHIP/Release.md → "Windows package")
	Content/mount-manifest.json              PACK
	Content/base.zpak | base-NN.zpak         PACK: the base unit, holding Package/package.zpkm and assets.zcat
	Content/<unit>[-NN].zpak                 PACK: every INSTALL_TIME, FAST_FOLLOW and ON_DEMAND unit, folded into the package
	Patches/<MAJOR.MINOR.PATCH>/update.zman  a committed patch; the patch mount mounts the highest verified one
	Patches/<MAJOR.MINOR.PATCH>/patch-NN.zpak
	Patches/.incoming/<MAJOR.MINOR.PATCH>/   a delivered patch bundle, installed at the next start (closing action of step 4)
	Patches/.incoming/.<anything>            a bundle still being delivered (ignored until renamed to its version)
	Patches/.trash/<generation>/             superseded versions, other-build patches and refused bundles being deleted
	DLC/<name>/dlc.zman  DLC/<name>/<name>[-NN].zpak    a committed DLC, mounted at the close of step 11 when entitled
	DLC/.incoming/<name>/  DLC/.trash/<generation>/
	.zenith2-install/install-journal.zinj    the one operation in progress
	.zenith2-install/install-state.zins      the replay floor and the last sweep ("Install state file")
	.zenith2-install/lock                    INSTALL_LOCK_APPLY: one installer at a time
	.zenith2-install/running                 INSTALL_LOCK_RUNNING: shared by every running instance
```

- The package tree SHIP verifies holds `Content/` and nothing of PACK's beyond it; `Patches/`, `DLC/` and `.zenith2-install/` appear only on an installed machine (cross-area row PK5).
- The install root must be writable by the user for patches and DLC; a read-only root (a protected system directory) leaves incoming bundles untouched, reports `PACKAGE_ERROR_INSTALL_ROOT_READ_ONLY` once per start and runs on the current content.
- The install root must be on an NTFS or ReFS volume for installs; on FAT32, exFAT or a network share (no ordered metadata log, Algorithms.md "Durability model") installs are disabled with one log line per start and the current content plays.

### Android install layout

| Location | Holds | Written by |
|---|---|---|
| base APK `assets/` | `mount-manifest.json`, the base unit's archives; in a sideload APK every non-DLC unit's archives | the package build (MOD's Gradle project consumes PACK's directory, cross-area row PK4) |
| install-time pack split APKs `assets/` | the unit's archives, read through the same asset manager as the base | Play at install |
| `AssetPackLocation.assetsPath()` of a fast-follow, on-demand or DLC pack | the unit's archives | Play |
| internal storage `cache/package/` (the user mount's `user:/cache/package/`) | `install-record.zinr`, `delivery-state.zdls` | the package service through `Zenith_AsyncIO` |
| internal storage `Patches/` | nothing: no Android build installs a patch ("Android content updates" in Algorithms.md); the applier only recovers and sweeps there | the applier |
| internal storage `.zenith2-install/` | the journal, the install state, the `lock` and `running` lock files | the applier |

### Android App Bundle layout

The bundle is built by Gradle from a second generated project (Tooling.md "App Bundle build"); bundletool sees:

```text
<Game>-unsigned.aab
	base/manifest/AndroidManifest.xml  base/dex/…  base/res/…  base/resources.pb
	base/lib/arm64-v8a/lib<Game>.so              (x86_64 for the emulator bundle)
	base/assets/mount-manifest.json  base/assets/base[-NN].zpak
	<pack>/manifest/AndroidManifest.xml          one asset-pack module per INSTALL_TIME, FAST_FOLLOW, ON_DEMAND, DLC unit
	<pack>/assets/<pack>[-NN].zpak
	BundleConfig.pb                              ABI splits on; density and language splits off; uncompressed native libraries
```

- Texture-compression-format targeting is not used: Android content has one texture format (ASTC, `Zenith2/Docs/Design/GEOM/Textures.md`), so no `#tcf_` directories exist.
- The base module holds no pack content, so its size is the base unit's plus code and resources, the quantity Z2-B-PACK-001 bounds.

## Packaging recipe

`Games/<Name>/package.recipe.json` is an authoring document (the recipe COMPRESS's fragment names, `Zenith2/Docs/Design/COMPRESS.md → "Compression setting fragment"`): stable name `Zenith.Package.RecipeDocument`, schema tag `zenith2.package-recipe`, version key `schemaVersion`, ID key `id`, exact file name `/package.recipe.json`, current version 1, migrated through SCENE's framework (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`). A game without the file packages everything into the base unit with COMPRESS's shipping defaults.

```json
{
	"$schema": "zenith2.package-recipe",
	"schemaVersion": 1,
	"id": "5d1f0a3c-9e7b-4248-9d1f-0a3c9e7b2248",
	"compression": { "codec": "zstd", "level": 19, "dictionary": null },
	"chunkTargetBytes": 1073741824,
	"classes": [
		{ "name": "small_records", "kinds": ["Material", "Prefab", "UIDocument"], "compression": { "codec": "zstd", "level": 19, "dictionary": null }, "dictionary": true },
		{ "name": "audio", "kinds": ["Sound"], "compression": { "codec": "none", "level": 0, "dictionary": null }, "dictionary": false }
	],
	"units": [
		{ "name": "world_textures", "kind": "installTime", "order": 20, "include": ["content:/World/**"], "exclude": [], "kinds": ["Texture"] },
		{ "name": "cinematics", "kind": "onDemand", "order": 30, "include": ["content:/Cinematics/**"] },
		{ "name": "expansion_isles", "kind": "dlc", "order": 1000, "include": ["content:/Dlc/ExpansionIsles/**"], "entitlement": "dlc.expansion_isles" }
	]
}
```

| Key | Type | Rule |
|---|---|---|
| `$schema`, `schemaVersion`, `id` | string, integer, persistent ID | `zenith2.package-recipe`, 1, a `Zenith_PersistentID` |
| `compression` | COMPRESS fragment | the default for every cooked section of a class-less kind in the shipping profile; validated with `Zenith_ValidateCompressionSetting` |
| `chunkTargetBytes` | integer | 16,777,216 … 4,294,967,296; default 1,073,741,824 |
| `classes[]` | objects, ≤ 64 | `name` `^[a-z][a-z0-9_]{0,31}$`, unique; `kinds` ASSET kind script names (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`), each kind in at most one class; `compression` fragment; `dictionary` boolean (true requires codec `zstd`) |
| `units[]` | objects, ≤ 255 | `name` per "Name rules", unique, not `base`; `kind` `installTime`, `fastFollow`, `onDemand` or `dlc`; `order` 1 … 1,000,000 for non-DLC, 1,000 … 1,000,000 for DLC, unique; `include` 1 … 64 patterns; `exclude` 0 … 64 patterns; `kinds` optional filter; `entitlement` required for and only for `dlc` |
| patterns | strings | `content:/` followed by path components where `*` matches within one component and `**` matches any number of components; matched against logical paths (`Zenith2/Docs/Design/ASSET.md → "Content root mapping"`); a pattern with `..`, `\` or an empty component is invalid |

Validation errors are `PACKAGE_BUILD_ERROR_RECIPE_INVALID` with the key path (`package.recipe.json: units[2].order: duplicate order 30`); unknown members are kept with a warning by the framework ([Z2-N-CONTRACT-535]).

## Size report and partition report

Every package build writes, beside its layout, `size-report.json` and `partition.json` with `Zenith_JsonWriter` (LF, one trailing LF, keys in the order shown):

```json
{
	"schema": "zenith2.package-size-report", "schemaVersion": 1,
	"game": "OpenWorldSlice", "platform": "android", "configuration": "Shipping", "buildId": "<32 hex>", "contentHash": "<16 hex>",
	"units": [
		{ "name": "base", "kind": "base", "bytes": 182345678,
		  "archives": [ { "name": "base.zpak", "bytes": 182345678, "sha256": "<64 hex>" } ],
		  "byKind": { "Texture": 120000000, "Mesh": 40000000 }, "byClass": { "small_records": 1234567 } }
	],
	"native": { "file": "lib/arm64-v8a/libOpenWorldSlice.so", "bytes": 61234567,
	            "byLibrary": { "ZenithCore": 1234567, "ZenithFlux": 7654321, "third-party:crashpad": 2345678, "unattributed": 12345 } },
	"otherFiles": [ { "path": "D3D12/D3D12Core.dll", "bytes": 4567890 } ],
	"compression": { "byCodec": { "zstd": { "storedBytes": 1, "decodedBytes": 2 }, "none": { "storedBytes": 3, "decodedBytes": 3 } },
	                 "dictionaries": [ { "class": "small_records", "guid": "<36>", "bytes": 112640 } ] },
	"totals": { "windowsInstallBytes": null, "sideloadApkBytes": null }
}
```

- `native.byLibrary` attributes the shared object's (or executable's) bytes to the static libraries and third-party archives linked into it, from the linker map the game's link writes (cross-area row PK4: `-Wl,-Map=` on Android, `/MAP` on MSVC); bytes no map line attributes are `unattributed`. This is the size delta DEVT's D-SEL-CRASH harness reads for its input 4 (`Zenith2/Docs/Design/DEVT/Crash.md`), the codec size AUDIO's row AU21 adds to Z2-B-PACK-001, and the video decoder size D-SEL-VIDEO weighs.
- `totals.windowsInstallBytes` is the MP-PKG sum of the Windows package tree excluding `*.pdb` ([Z2-N-BUDGET-175]), filled by the Windows package build when SHIP's files are present in the staging tree, `null` otherwise; the base-module and pack download sizes come from bundletool in `bundle-report.json` (Tooling.md "Bundle size measurement").

`partition.json` lists every asset by GUID with its unit and the reason (`rule <n>`, `promoted by <guid>`, `default base`), every promoted asset with its dependent, and every archive with its entries' paths, in byte order; it is the diagnostic of "Partition algorithm" in Algorithms.md.

## Patch and DLC bundles

A patch bundle is a directory named `MAJOR.MINOR.PATCH` holding `update.zman` and the `patch-NN.zpak` archives it lists, nothing else (no subdirectory, no link, no other file: check U0 refuses any other entry); the release pipeline zips it deterministically (`New-Zenith2DeterministicZip`, `Zenith2/Docs/Design/SHIP.md → "Interfaces SHIP declares for other areas"`). It is installed by placing the directory at `<install>/Patches/.incoming/MAJOR.MINOR.PATCH/` (a launcher, the user extracting the zip there, or QA tooling) and starting the game.

A DLC bundle is a directory named `<name>` holding `dlc.zman` and the DLC unit's archives and nothing else (U0), installed by placing it at `<install>/DLC/.incoming/<name>/`. Its first archive holds `Catalogues/<name>.zcat`, a catalogue fragment whose dependencies on base assets are external references (`Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue fragments"`).

| Bundle | Directory | Manifest | Archives |
|---|---|---|---|
| patch | `MAJOR.MINOR.PATCH` | `update.zman`, kind PATCH | `patch-00.zpak` … `patch-99.zpak` |
| DLC | `<name>` | `dlc.zman`, kind DLC | `<name>.zpak` or `<name>-00.zpak` … |

## Verified stamp file

`<root>/.zenith2-install/verified.zinv`, where `<root>` is the install file system's root, written by the applier with the durable step "write the verified stamp file" (Algorithms.md "Durable step rules") at each commit of a patch or a Windows DLC (patch install step 3b, DLC install step 4b), and read by the runtime bridge at the close of startup step 4 for the stamp skip of Algorithms.md "Verified cache" item 4 (Q90 ruled (a)). Little-endian, written and read field by field (Z2-D-038). Class `user-state`: a missing or refused file makes every committed directory re-hash in full at its next mount, which is always safe, and the next commit writes a valid one. Android builds never commit a patch or a DLC ("Android content updates"), so the file exists only on Windows and Linux installs.

Header, 32 bytes:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZINV` |
| 4 | 2 | format version | 1 |
| 6 | 2 | directory count D | 0 … 1,024 (`uPACKAGE_MAX_VERIFIED_DIRECTORIES`) |
| 8 | 4 | file count F | 0 … 131,072 (`uPACKAGE_MAX_VERIFIED_FILES`), the sum of the directories' file counts |
| 12 | 4 | string table bytes S | ≤ 1,048,576 |
| 16 | 8 | generation | the install journal's header generation at the durable step that wrote the file (diagnostic only; never compared) |
| 24 | 4 | file check | over the whole file with this field zero |
| 28 | 4 | reserved | 0 |

D directory records of 48 bytes, sorted by path bytes, then F file records of 56 bytes, then the string table:

| Offset | Size | Directory record field | Rule |
|---|---|---|---|
| 0 | 4 | path offset | into the string table |
| 4 | 2 | path length | 1 … 255; an install-relative path `Patches/<MAJOR.MINOR.PATCH>` or `DLC/<name>` (the name rules of "Name rules"), unique |
| 6 | 2 | reserved | 0 |
| 8 | 32 | manifest digest | SHA-256 of the directory's whole `update.zman` or `dlc.zman` as verified at B6 |
| 40 | 4 | first file | index of the directory's first file record |
| 44 | 4 | file count | 1 … 65,536, the manifest's listed files |

| Offset | Size | File record field | Rule |
|---|---|---|---|
| 0 | 4 | name offset | into the string table |
| 4 | 2 | name length | 1 … 255; a listed file name of the manifest (one path component), unique within its directory; a directory's records are sorted by name bytes |
| 6 | 2 | reserved | 0 |
| 8 | 48 | stamp | the `Zenith_FileIdentityStamp` taken immediately before B6's first read of the file (or recovery's re-stamp), equal to the one taken after its last read, in the layout of "Delivery state records" |

The file is exactly 32 + 48 × D + 56 × F + S bytes, at most `ulPACKAGE_MAX_VERIFIED_FILE_BYTES` = 32 + 48 × 1,024 + 56 × 131,072 + 1,048,576 = 8,437,792 bytes. When a commit's surviving records exceed a limit (more than 1,024 directories, more than 131,072 files, or a string table above 1,048,576 bytes), `Zenith_VerifiedStampSet::Encode` drops whole directories other than the target, highest path first, until the set fits; a target that alone does not fit (its paths and names above 1,048,576 string bytes; D and F cannot overflow alone, since one directory holds at most 65,536 files) is written without its record, every other record kept as far as it fits. Each dropped directory re-hashes at its mount, logged `package: verified stamps: <n> directories not recorded`. The string table may hold bytes no record references (VS6 checks only the referenced ranges), which the limit tests use to reach exactly 1,048,576. The reader's copy of the file (up to 8,437,792 bytes, kept from startup step 4 to shutdown step 6) is part of the package service resident memory row (entry document → "In-memory layouts that matter", Z2-B-PACK-009).

Checks, in this order, each failing with `PACKAGE_ERROR_STATE_FILE_CORRUPT` (logged `package: verified stamps refused (<check>)`), after which the reader treats the file as absent:

| # | Check |
|---|---|
| VS1 | at least 32 bytes, at most `ulPACKAGE_MAX_VERIFIED_FILE_BYTES`; magic `ZINV` |
| VS2 | format version 1 (another version is refused here, the registry's refusal case) |
| VS3 | the file check |
| VS4 | D, F and S within their limits; the length equals 32 + 48D + 56F + S (overflow-checked); every reserved field zero |
| VS5 | the directories' file ranges are contiguous, in directory order, start at 0 and cover exactly F records; every directory's file count is 1 … 65,536 |
| VS6 | every string range inside the table and valid UTF-8; every path valid and unique, the directories sorted by path bytes; every name valid, unique within its directory and sorted by name bytes |

`Zenith_VerifiedStampFile::FindStamp` answers by binary search, the directory by path bytes and then the file by name bytes within its range, O(log D + log 65,536); it answers false for a directory or a file it does not record, and for a directory whose manifest digest differs from the one the caller passes.

## Bundle configuration and device specs

PACK's committed text inputs to bundletool (Q55: no binary file is committed):

| File | Content |
|---|---|
| `Zenith2/Tools/Package/DeviceSpecs/s24-ultra.json` | `{ "supportedAbis": ["arm64-v8a"], "supportedLocales": ["en-GB"], "screenDensity": 450, "sdkVersion": 34 }` — the validated device (Z2-D-010) at its default resolution |
| `Zenith2/Tools/Package/DeviceSpecs/emulator-x86_64.json` | `{ "supportedAbis": ["x86_64"], "supportedLocales": ["en-GB"], "screenDensity": 420, "sdkVersion": 34 }` — the emulator smoke target ([Z2-N-ARCH-216]) |
| `Zenith2/Tools/Package/bundle-config.json` | the `bundletool build-bundle --config` JSON for the test-time mini bundle: `{ "optimizations": { "splitsConfig": { "splitDimension": [ { "value": "ABI" }, { "value": "SCREEN_DENSITY", "negate": true }, { "value": "LANGUAGE", "negate": true } ] }, "uncompressNativeLibraries": { "enabled": true } }, "compression": { "uncompressedGlob": ["assets/**/*.zpak"] } }` |

`uncompressedGlob` keeps archives stored in the APK zip, so the asset manager reads them at file offsets without inflation; the same rule is set for Gradle builds through `androidResources.noCompress += "zpak"` (cross-area row PK4).

## Format registry file

`Zenith2/Tools/Package/format-registry.json`:

```json
{
	"schema": "zenith2.format-registry", "schemaVersion": 1,
	"formats": [
		{ "magic": "ZPKM", "extension": ".zpkm", "owner": "PACK", "version": 1, "class": "recook",
		  "spec": "Zenith2/Docs/Design/PACK/Formats.md", "refusalTest": "ZenithPackage/PackageManifest/Parse_OtherVersion_IsRefused" }
	]
}
```

| Field | Rule |
|---|---|
| `magic` | exactly 4 characters from `A`–`Z` and `0`–`9`, first character `Z`; unique |
| `extension` | `.` then 1 … 15 lower-case letters or digits, unique; a format whose extension varies (the cooked asset file, one per kind) has one row per kind magic, each with its kind's extension |
| `owner` | one of the 41 area codes |
| `version` | the current format version, 1 … 65,535 |
| `class` | `recook`, `signed-update`, `migrate`, `user-state`, `transient` |
| `spec` | a repository-relative path that exists |
| `refusalTest` | the doctest name (`"<Library>/<Type>/<Behaviour>"`) of the owner's test that refuses another version; the name appears in a `TEST_CASE` of a source under `Zenith2/` |

The example shows the row `ZPKM` gets in the Stage 4 diff that adds it; the Stage 2 file is the same document with `"formats": []`. `CMake_Pack_FormatRegistry_Valid` checks every rule (Tests.md "Format registry tests").
