# PACK design — Packaging & content updates

The design of the PACK area: how cooked content becomes shipped packages (archives, chunking, compression policy, install layouts on Windows and Android, App Bundles with Play Asset Delivery asset packs), how content changes after release (signed patch and DLC manifests, installed-update migration, update compatibility), and how an interrupted update or an interrupted pack download leaves the game consistent. Workers implement against this file and its companions; the requirement catalogue is `Zenith2/Docs/Requirements/PACK.md` (the purpose, the requirements of Stages 1, 2 and 4, the coverage of inventory items over both files, the exclusions) with `Zenith2/Docs/Requirements/PACK/Stage6.md` (the Stage 6 requirements), one numbering.

## Purpose and scope

### What PACK owns

| Capability | Inventory items | Specified in |
|---|---|---|
| the registry of every versioned binary format, its compatibility classes, and PACK's own formats with versioned headers | Z2-I-PACK-01 | `Zenith2/Docs/Design/PACK/Formats.md` → "Binary format registry", "Common rules for PACK formats" |
| the `.zpak` packer and its packing policy (the layout and reader are ASSET's) | Z2-I-PACK-02 | `Zenith2/Docs/Design/PACK/Formats.md` → "Archives PACK writes"; `Zenith2/Docs/Design/PACK/Tooling.md` → "Archive writer" |
| compression in packages: the per-class policy, the shipping cook profile, dictionary training | Z2-I-PACK-03 | `Zenith2/Docs/Design/PACK/Tooling.md` → "Shipping cook profile"; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Dictionary policy" |
| chunking: units, partition, dependency promotion, archive cutting | Z2-I-PACK-04 | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Partition algorithm", "Chunking" |
| installation layouts: the Windows tree, the Android sideload APK, the App Bundle modules, the shipped mount manifest, `package.zpkm` | Z2-I-PACK-05 | `Zenith2/Docs/Design/PACK/Formats.md` → "Installation layouts", "Package manifest", "Mount manifests PACK writes" |
| Android App Bundles | Z2-I-PACK-06 | `Zenith2/Docs/Design/PACK/Tooling.md` → "App Bundle build" |
| Play Asset Delivery: the delivery interface, the Play bridge, the installer and its state machine | Z2-I-PACK-07 | "Public API"; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Asset pack state machine" |
| bundletool split-APK testing | Z2-I-PACK-08 | `Zenith2/Docs/Design/PACK/Tooling.md` → "bundletool usage" |
| installed-update migration | Z2-I-PACK-09 | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Installed-update migration" |
| patch manifests | Z2-I-PACK-10 | `Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest"; `Zenith2/Docs/Design/PACK/Tooling.md` → "Patch builder" |
| DLC manifests | Z2-I-PACK-11 | the same, kind DLC; `Zenith2/Docs/Design/PACK/Tooling.md` → "DLC builder" |
| update compatibility | Z2-I-PACK-12 | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Update compatibility rules" |
| interrupted-update recovery | Z2-I-PACK-13 | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Install state machine", "Recovery table" |
| interrupted content installation: resumed, rolled back, the game consistent | Z2-I-PACK-14, Z2-I-PACK-15, Z2-I-PACK-16 | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Asset pack transitions", "Consistency invariants" |

### Explicit exclusions

| Not PACK | Owner | Rationale |
|---|---|---|
| the `.zpak` layout, its checks and its reader `Zenith_CreateArchiveMountSource`; the cooked file and catalogue formats; the cook driver and its modes table | ASSET | ruled: ASSET's reader, PACK's writer (`Zenith2/Docs/Design/ASSET.md` → "Purpose and scope"; core-doc item X114 (d)); PACK must not re-specify them and cites `Zenith2/Docs/Design/ASSET/Formats.md` → "Archive file" |
| codecs, the section descriptor, dictionary training as an algorithm | COMPRESS | PACK chooses codec, level and dictionary per class and calls COMPRESS (`Zenith2/Docs/Design/COMPRESS.md` → "Cook integration") |
| mounts, precedence, the patch mount's verification at startup, the mount-manifest schema, `Zenith_VerifyUpdateManifest`, async I/O, atomic user-mount writes | PLAT (verification rules SEC's) | PACK writes what PLAT reads (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema", "Patch mount verification") |
| the release pipeline, Shipping configuration, signing keys and their custody, `release.json`, symbols, licence inventory, the package checklists | SHIP | "Boundary with SHIP" |
| the Gradle template, the game's APK target, `zenith_add_game` | MOD | PACK's additions reach them through cross-area row PK4 |
| save files and their migration | SAVE | PACK reports updates; SAVE migrates a save when it loads (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Migration pipeline") |
| trust boundaries, update verification rules, fuzz durations | SEC | PACK's formats follow SEC's rules through PLAT's verifier |
| entitlements and their backends | ONLINE | PACK declares `Zenith_EntitlementQuery`; ONLINE implements it |
| downloading content from a server of our own | none | Z2-D-027 rejects a custom content downloader; Windows bundles arrive by whatever channel distributes the game (a launcher, Steam's own update, a zip) |
| third-party content and mod packages | none | modding is excluded (Z2-D-075); no mount accepts content outside the package, patch, loose and user mounts |
| binary delta patching inside files | none | patches are entry-granular ("Design rulings PACK records", R4) |
| console packaging | SCOPE (S7) | consoles are deferred (Z2-D-071); PACK's interfaces (`Zenith_InstallFileSystem`, `Zenith_AssetPackDelivery`) take a console implementation without restructuring |

### Boundary with ASSET

- ASSET owns the cooked tree (`Zenith2/Docs/Design/ASSET.md` → "Cooked output tree"), every cooked format, the archive format and reader, catalogue fragments as a format (`Zenith2/Docs/Design/ASSET/Formats.md` → "Catalogue fragments"), and the cook's mode table.
- PACK owns what is decided after cooking: which file goes into which unit and archive, the order, alignment beyond ASSET's 16-byte minimum, the archive names, the partition of the catalogue into a base catalogue and DLC fragments, the shipped mount manifest and `package.zpkm`. Its code lives in `Zenith2/Tools/ZenithCook/Internal/Package/`, the subdirectory ASSET assigned it (`Zenith2/Docs/Design/ASSET.md` → "Library and file layout").
- The seam is three ZenithCook modes, one cook key (`cook.profile`) and one cook step (`pack.dictionaries`) in ASSET's tables, the catalogue writer and the cooked-file rewrite reached from PACK's subdirectory (cross-area row PK6), and the read-back: every archive PACK writes is reopened through ASSET's reader before it is published (`Zenith2/Docs/Design/PACK/Tooling.md` → "Archive read-back").
- A change of the archive layout is a format-version bump edited in ASSET's and PACK's designs in the same diff.

### Boundary with SHIP

| Concern | SHIP | PACK |
|---|---|---|
| a Windows package | builds the Shipping executable, adds runtime redistributables and `Licences/`, signs, writes `release.json`, verifies | writes `Content/` (archives, `mount-manifest.json`) through `<Game>_Package` into `${ZENITH_OUT}/Packages/<Game>/windows/<Config>/` (SHIP's cross-area row SH5 (a)) |
| an Android package | signs the sideload APK and the App Bundle with its keys, verifies | the sideload layout, the bundle project and `<Game>_Bundle`, whose unsigned output is `${ZENITH_OUT}/Packages/<Game>/android/<Config>/<Game>-unsigned.aab` (SH5 (b)) |
| a patch or DLC | key custody, `Get-Zenith2UpdateSigningKey`, `release.json` of each release | the patch and DLC builders, `update.zman` and `dlc.zman`, the signer that receives the seed (SH5 (c), (d)) |
| measurement | the Release measurement package | the size report, the bundle report, MP-PKG's bundletool procedure for Z2-B-PACK-001 |

SHIP's revision 2 is in progress; its assumption blocks that name PACK are answered in "Assumed interfaces pending other areas" and cross-area row PK5.

### Inventory items covered

Z2-I-PACK-01 to Z2-I-PACK-16, all required; the coverage table, over both catalogue files, is `Zenith2/Docs/Requirements/PACK.md` → "Coverage of inventory items".

### Document family

| File | Holds | Its headings |
|---|---|---|
| `Zenith2/Docs/Design/PACK.md` (this file) | the skeleton, ownership, boundaries, threading and errors, diagnostics, failure handling, budgets, stage plan, decisions, the requirement index | the headings of this file |
| `Zenith2/Docs/Design/PACK/PublicAPI.md` | every public header of ZenithPackage, the runtime bridge, the Luau surface | Scope of this file · Rules across the PACK API · Zenith_PackageTypes header · Zenith_PackageManifest header · Zenith_AssetPackDelivery header · Zenith_EntitlementQuery header · Zenith_AssetPackInstaller header · Zenith_InstallFileSystem header · Zenith_UpdateApplier header · Zenith_InstallRecord header · Zenith_UpdateMigration header · Zenith_PackageService header · Runtime bridge declarations · Script-facing API |
| `Zenith2/Docs/Design/PACK/Formats.md` | the format registry, every PACK format, layouts, JSON documents | Scope of this file · Binary format registry · Common rules for PACK formats · Name rules · Package manifest · Package manifest header · Package manifest tables · Package manifest checks · Update manifest · Update manifest header · Update manifest tables and signature · Update manifest checks · Install journal · Install journal header · Install journal records · Install state file · Install record · Identity encoding · Delivery state file · Delivery state header · Delivery state records · Package event payload · Archives PACK writes · Mount manifests PACK writes · Installation layouts · Windows install layout · Android install layout · Android App Bundle layout · Packaging recipe · Size report and partition report · Patch and DLC bundles · Verified stamp file · Bundle configuration and device specs · Format registry file |
| `Zenith2/Docs/Design/PACK/Algorithms.md` | durability, the install and pack state machines, startup, migration, compatibility, the packer's algorithms, invariants | Scope of this file · Durability model · Durable step rules · Install state machine · Install phases and events · Install steps per operation · Obsolete directories · Recovery table · Pending installs at startup · Install lock and instance guard · Android content updates · Asset pack state machine · Asset pack events · Asset pack transitions · Play status and error mapping · Delivery update slots · Verification pipeline · Verified cache · Mounting and unmounting · Shutdown drain · Fetch policy · Network retry · Initial query · Simulated delivery · Sideload builds · Package service startup · Entitlement provider · Windows DLC mounting · Update classification · Installed-update migration · Built-in migration steps · Update compatibility rules · Partition algorithm · Chunking · Dictionary policy · Patch diff · Event publication and replay · World snapshot · Replay pack set · Consistency invariants · Complexity |
| `Zenith2/Docs/Design/PACK/Tooling.md` | ZenithCook's package modes, CMake, Gradle, the Java bridge, bundletool, the builders | Scope of this file · Tool-side file layout · ZenithCook package modes · Package build steps · Shipping cook profile · Archive writer · Archive read-back · Manifest writers · Update manifest writer · Ed25519 signer · Patch builder · DLC builder · CMake targets · App Bundle build · Java bridge · bundletool usage · Bundle size measurement · Local testing and the emulator smoke · Windows install conventions · PowerShell functions · Mini bundle manifests |
| `Zenith2/Docs/Design/PACK/Tests.md` | every test, fixture, device run and acceptance item | Scope of this file · Test targets and gate steps · Fixtures · Format tests · Format registry tests · Install file system tests · Update applier tests · Simulated delivery tests · Asset pack installer tests · Install record and migration tests · Package service tests · Runtime bridge tests · Packer tests · PowerShell and script-mode tests · Integration suite · Device runs · Stage 6 acceptance items · Fault point table |

### Design choices at a glance

| # | Choice | Where |
|---|---|---|
| C1 | One runtime library, `ZenithPackage` (layer 4), holds every runtime part of PACK; the tool side lives in ZenithCook's `Internal/Package/` and `Zenith2/Tools/Package/` | "Library and file layout" |
| C2 | Installing never copies: a bundle is sealed (every file and directory entry flushed), verified read-only where it was delivered and committed by one atomic directory rename, so an interruption leaves the old or the new content and recovery only finishes moves and deletes | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Install state machine" |
| C3 | Updates are applied before the virtual filesystem exists (the closing action of startup step 4), never while another instance of the install runs, and no instance starts while another process installs (it waits for the installer's guard hand-over), so no running game ever sees a half-installed patch or loses a mounted one | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Pending installs at startup", "Install lock and instance guard" |
| C4 | A content patch never changes the executable; code changes ship as full releases | "Design rulings PACK records", R1 |
| C5 | The base catalogue lists every non-DLC asset, so an undelivered pack's asset is known and fails `NOT_INSTALLED` with ASSET's fallback; DLC brings its own catalogue fragment | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Partition algorithm" |
| C6 | A delivered pack is verified against the base `package.zpkm`'s SHA-256 before mounting, once: a later start skips the hash while every archive's identity stamp still matches its record (Q90 ruled (a)), and its files are removed only after the virtual filesystem released them | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verification pipeline", "Mounting and unmounting" |
| C7 | Play Asset Delivery sits behind `Zenith_AssetPackDelivery`, and a simulated delivery with scripted faults drives every installer test deterministically without Play | `Zenith2/Docs/Design/PACK/PublicAPI.md` → "Zenith_AssetPackDelivery header" |
| C8 | Pack state reaches simulation only as `ASYNC_SOURCE_PACKAGE` results of a recorded publication kind, published at tick boundaries in completion-ID order, with a snapshot for every new world, so a replay reproduces it; a replay mounts exactly the packs its recording mounted | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Event publication and replay", "World snapshot", "Replay pack set" |
| C9 | bundletool runs in the gate over a bundle generated at test time without Gradle; real bundles are measured in Stage 6 acceptance and run on the S24 | `Zenith2/Docs/Design/PACK/Tests.md` → "PowerShell and script-mode tests" |
| C10 | `package.zpkm` is written per installation layout and never patched, so deliverability is a property of the manifest the running build carries and identity has one rule | `Zenith2/Docs/Design/PACK/Formats.md` → "Package manifest" |

## Dependencies

### Libraries PACK depends on

| Library | What PACK uses |
|---|---|
| ZenithCore | `Zenith_Result`, `Zenith_Owned`, arenas, `Zenith_Sha256`, `Zenith_Hash64`, `Zenith_JsonWriter`, logging, `Zenith_OperationHandle`, the fault injector |
| ZenithProfiling | counters and zones through the profiler hooks |
| ZenithPlatform | `Zenith_VirtualFileSystem` (`AddSource`, `RemoveSource`, `ReadWhole`), `Zenith_AsyncIO`, `Zenith_BuildIdentity`, the directory mount source, `Zenith_VerifyUpdateManifest` and its three halves, `Zenith_ParseMountManifest`, `Zenith_Platform::GetAndroidJavaContext` (JNI without SDL) |
| ZenithAssets | `Zenith_CreateArchiveMountSource`, `Zenith_ArchiveReaderDesc` |
| ZenithJobs | background jobs for hashing |

`ZenithPackage`'s declared direct dependencies are exactly these five; its layer is 4 (ZenithAssets is layer 3). `ZenithRuntime` adds `ZenithPackage` to its dependencies; `ZenithCook` adds it for the shared format code it links in tests and verify mode (`Zenith2/Docs/Architecture.md` → "Library dependency graph", row ZenithPackage).

### Headings PACK relies on

| Topic | Heading |
|---|---|
| ownership, results, handles, threading, compatibility | `Zenith2/Docs/Contracts.md` → "Ownership and allocation", "Errors and results", "Handle kinds", "Threading and concurrency contract", "Compatibility policy per kind" |
| security, parser limits, update manifests | `Zenith2/Docs/Architecture.md` → "Security architecture" |
| the library graph and repository layout | `Zenith2/Docs/Architecture.md` → "Library dependency graph", "Repository layout" |
| startup and exit codes | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", "Exit codes" |
| Android packaging | `Zenith2/Docs/Architecture/BuildSystem.md` → "Android packaging"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Android packaging" |
| package rows | `Zenith2/Docs/Budgets.md` → "Package and storage budgets" |
| mounts and patch verification | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount sources", "Zenith_VirtualFileSystem API", "Mount manifest schema", "Patch mount verification", "Zenith_AsyncIO API", "Atomic writes" |
| archives, catalogues, the cook | `Zenith2/Docs/Design/ASSET/Formats.md` → "Archive file", "Catalogue fragments"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetArchive header"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Cook command line", "Global outputs" |
| retry after a mount change | `Zenith2/Docs/Design/ASSET/Loading.md` → "Retry after a mount change" |
| compression | `Zenith2/Docs/Design/COMPRESS.md` → "Compression setting fragment", "Dictionary creation and training", "Cook integration" |
| SHA-256 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256" |
| operation handles, fault injection | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation"; `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support" |
| the recipe document type | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_MigrationRegistry header" |
| SHIP's hand-offs | `Zenith2/Docs/Design/SHIP/Release.md` → "Windows package", "Android package", "Update manifest signing hand-off", "release.json schema" |
| MOD's Gradle wiring | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Gradle project generation", "Placeholders", "The Apk target" |
| SAVE's update test | `Zenith2/Docs/Design/SAVE/Tests.md` → "Platform storage tests" |
| asynchronous results | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry", "Tick-boundary publication"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation" |
| the Android bridge pattern | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android bridge lifetime", "Java bridge contract" |

### Areas PACK depends on

| Area | Dependency |
|---|---|
| CORE, JOBS, PLAT, ASSET, COMPRESS | the libraries above |
| SCENE | the migration framework the recipe document uses |
| MOD | `zenith_add_game` calls `zenith_add_package`; the Gradle template and `_zenith_gradle_project`; the closing actions of startup steps 4 and 11, the entitlement provider at step 15, world creation step 4, step 4's unwind and shutdown steps 4 and 10b in ZenithRuntime; the test hook's base-path override (cross-area rows PK3, PK4, PK15, PK21, PK22) |
| SHIP | the binary identity reader, the signing-key hand-off, `release.json`, the manifest pins of bundletool and the Play library, the deterministic zip (rows SH5, PK5, PK7) |
| SEC | the update-verification rules PLAT's verifier implements, including U9's stamp skip (row PK27), and the three confirmation items of `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache" item 6 for the Stage 1 security-architecture ticket: the stamp set, the records being unauthenticated and protected only by the install root's write permission, and the keyed-record escalation (pending) |
| ONLINE | an implementation of `Zenith_EntitlementQuery` from Stage 6, provided at step 15 through MOD's start context (pending; cross-area row PK21) |
| SCRIPT | the binding ABI and its context check, module command kinds, the misuse-code block ZS2600–ZS2699 (`Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges"), the source kind `ASYNC_SOURCE_PACKAGE` (cross-area row PK13) |
| SAVE | the replay recorder, which records PACK's `ASYNC_SOURCE_PACKAGE` publications as every recorded publication (row PK19); the replay reader's enumeration of one source kind's publications for the replay pack set (row PK24) |
| DEVT | consumes the size report for D-SEL-CRASH; the frame dump hosts `package` |
| WORLD | the warning names `WORLD_WARNING_BAKE_STALE` and `WORLD_WARNING_BAKE_MISSING`, read as the `code` of the cook report's diagnostics by package build step 1b (rows WD14 (b), PK26); ASSET's code-taking import `Report` and the replay of persisted import diagnostics in every cook report (row WD35, pending ASSET) |

### Areas that depend on PACK

| Area | What it uses |
|---|---|
| SHIP | `<Game>_Package`, `<Game>_Bundle`, the update-manifest writer, the size and bundle reports |
| PLAT | the formats it reads (`mount-manifest.json` written by PACK, `update.zman`), the delivery service's mount requests, the `Zenith_UpdateStampLookup` PACK implements over `verified.zinv` for the patch mount's stamp skip (row PK27) |
| ASSET | archives PACK writes; mount changes PACK makes (`OnMountsChanged`) |
| SAVE | the `Package.Updated` event and the installed-update device run |
| DEVT | the size report's per-library bytes (D-SEL-CRASH input 4) |
| WORLD | the Shipping package's refusal of stale or missing bakes (rows WD14 (b), PK26; `Zenith2/Docs/Design/PACK/Tooling.md` → "Package build steps" step 1b) |
| AUDIO | stored archive entries for streamed sounds (row AU21) |
| STREAM | `ASSET_ERROR_NOT_INSTALLED` for cells whose content is in an undelivered pack |
| GAME, UI | the Luau surface and events, to show download progress and prompts |

### Upward interfaces PACK declares

- `Zenith_EntitlementQuery` (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Zenith_EntitlementQuery header"): implemented by ONLINE above PACK and handed to the runtime bridge's `Zenith_ForwardingEntitlementQuery` at step 15 (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Entitlement provider").
- `Zenith_InstallFileSystem` and `Zenith_AssetPackDelivery`: implemented inside PACK (host, memory, Play, simulated) and open to a further implementation without changing their callers.
- `Zenith_PfnUpdateMigrationStep`: owners' cache-migration functions, registered by the composition root.

### Assumed interfaces pending other areas

<!-- assumption: pending PLAT, SEC -->
PACK assumes from PLAT (cross-area row PK2): (1) a public declaration of `Zenith_VerifyUpdateManifest` returning the parsed header and file entries, accepting both manifest kinds and checking the patch directory name against the manifest version (Formats.md "Update manifest checks", U5 and U8); (2) a public factory `Zenith_CreateDirectoryMountSource(Zenith_StringView strHostRoot, const Zenith_DirectorySourceDesc&)` for a host directory outside the four mount roots (a delivered pack's assets path, `<install>/DLC/<name>`); (3) an asynchronous read from a source object that is not mounted, `Zenith_AsyncIO::ReadFromSource(const Zenith_MountSource&, Zenith_StringView strRelative, uint64 ulOffset, Zenith_MutableByteSpan xDestination, Zenith_IOPriority, Zenith_IOCompletionQueue*, uint64 ulUserTag)`, for pack verification; (4) `Zenith_BuildIdentity::m_ulContentHash` set to the mounted patch's result content hash when a patch is mounted. From cross-area row PK14: (5) the three public halves of the verifier, `Zenith_DecodeUpdateManifest` (U1–U3, U7), `Zenith_CheckUpdateManifestTarget` (U4, U5, U8) and `Zenith_VerifyUpdateManifestFiles` (U9); (6) U11 in the patch mount; (7) `Zenith_BuildIdentity::m_ulPackageContentHash`, the mount manifest's `contentHash`, unchanged by a patch; (8) `AddSource` and `RemoveSource` admitted in the closing action of startup step 11 and in shutdown step 4; (9) `Zenith_Platform::GetAndroidJavaContext()` returning the `JavaVM*` and global references to SDL's activity and its class loader; (10) a connectivity signal, without which PACK retries on a 30 s back-off; (11) the deletion of the runtime patch-install sentence (step 5) of "Patch mount verification"; (12) `GetPrefPath()` as the internal storage root on Android. From cross-area row PK23: (13) `Zenith_PlatformDesc::m_strBasePathOverride` (empty by default: `SDL_GetBasePath`), so the integration suite runs the packaged game over a per-test copy of its install tree. From cross-area row PK27 (Q90 ruled (a)): (14) `Zenith_FileIdentityStamp` and `Zenith_MountSource::GetIdentityStamp`, (15) the lookup interface `Zenith_UpdateStampLookup`, (16) `Zenith_VerifyUpdateManifestFiles` taking an optional lookup and skipping the hash of a file whose stamp matches, (17) `Zenith_VirtualFileSystemDesc::m_pxUpdateStamps` for the patch mount at step 5 (the assumption block of `Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest checks"). SEC's rules are assumed to be exactly U0–U5 and U7–U11, with U9's stamp skip confirmed by the Stage 1 security-architecture ticket.
<!-- /assumption -->

<!-- assumption: pending MOD -->
PACK assumes from MOD (cross-area rows PK3, PK4): `zenith_add_game` includes `CMake/ZenithPackage.cmake` and calls `zenith_add_package(<Name>)` after its Gradle step; `_zenith_gradle_project` accepts an assets directory per build type, a list of asset-pack modules, a runtime dependency list and extra Java source directories; the Release and Shipping build types of the game's own project draw assets from PACK's `sideload/assets/` and `<Name>_Apk` depends on `<Name>_Package` for them; the template ships `assetpack/build.gradle.kts.in`; `androidResources.noCompress += "zpak"`; the game's link writes a linker map (`-Wl,-Map=${ZENITH_OUT}/Games/<Name>/<preset>/lib<Name>.map` on Android, `/MAP:<…>/<Name>.map` on Windows) in Release and Shipping; MOD's files carry PACK's calls at the places of `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Call sites PACK's tickets add" (world creation step 4 gains `Zenith_RuntimePackage_AttachWorld`, with `Internal/Package/` a PACK-owned subdirectory; rows PK3, PK15; the startup, prologue and shutdown steps themselves are core text: `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", `Zenith2/Docs/Contracts.md` → "Shutdown ownership order"); `Zenith_ModuleStartContext::ProvideEntitlementQuery` calls `Zenith_RuntimePackage_SetEntitlementProvider` and step 15 ends with `Zenith_RuntimePackage_AfterModulesStarted` (row PK21); step 4's unwind entry first calls `Zenith_RuntimePackage_ReleaseInstanceGuard`, and `Zenith_RuntimeTestHooks` gains `m_strBasePathOverride`, passed to the platform desc at step 4 (row PK22).
<!-- /assumption -->

<!-- assumption: pending SHIP -->
PACK assumes from SHIP (rows SH5, PK5, PK7): `Get-Zenith2BinaryIdentity` and `Get-Zenith2UpdateSigningKey` as SHIP's release document states them; manifest pins for `bundletool` (a `host-tool` jar) and `play-asset-delivery` (the Maven artefact `com.google.android.play:asset-delivery` with its transitive `core-common`, in the offline Gradle home, licence the Play Core Software Development Kit Terms of Service, included in bundle builds only), and the licence inventory listing it for bundle builds; SHIP's Windows package tree takes `Content/` from `${ZENITH_OUT}/Packages/<Game>/windows/<Config>/Content/`; `CMake/ZenithPackage.cmake` is a PACK file in SHIP's directory; the release notes carry PACK's Windows install paragraph.
<!-- /assumption -->

<!-- assumption: pending ONLINE -->
ONLINE's adapter implements `Zenith_EntitlementQuery` from Stage 6 (Steam DLC ownership, Google Play purchases) and hands it over at step 15 through `Zenith_ModuleStartContext::ProvideEntitlementQuery`, which reaches the bridge's forwarder (row PK21, which answers ONLINE's row ON3 (b): the forwarder replaces the `Zenith_PackageService::SetEntitlementQuery` member ON3 assumed). Until then, in builds without the Online module, and when the module provides nothing, the forwarder answers from `Zenith_ConfigEntitlementQuery` over `package.grantedEntitlements` (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Entitlement provider"). ONLINE's query must stay valid until shutdown step 4, where the bridge drops it.
<!-- /assumption -->

<!-- assumption: pending SAVE -->
PACK assumes from SAVE (row PK24, extending PK19): `Zenith_ReplayLogReader::ForEachPublication(uint8 uSourceKind, Zenith_PfnReplayPublicationVisitor pfnVisitor, void* pUserData) const`, visiting every publication of the kind in log order with its world tag, tick, completion ID, publication kind and full payload, so `Zenith_RuntimePackage_Create` forms the replay pack set under `--replay` (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Replay pack set").
<!-- /assumption -->

## Library and file layout

### Target summary

| Target | Kind | Directory | Owner | `ZENITH_SUPPORTED` |
|---|---|---|---|---|
| `ZenithPackage` | engine library, layer 4, linked by `ZenithRuntime` into every Player | `Zenith2/Engine/ZenithPackage/` | PACK | `ZENITH_ALL` (Windows, Android arm64 and x86_64, `linux-tsan`) |
| `ZenithPackage_Tests` | test executable | `Zenith2/Engine/ZenithPackage/Tests/` | PACK | `ZENITH_HOST` |
| `Zenith_Integration_Package` | integration suite | `Zenith2/Tests/Package/` | PACK | `ZENITH_HOST` |
| PACK's files in `ZenithRuntime` | engine library subdirectory | `Zenith2/Engine/ZenithRuntime/Internal/Package/`, `Internal/ScriptBindings/Zenith_PackageScriptBindings.cpp` | PACK (library MOD's) | the library's |
| PACK's files in `ZenithCook` | tool subdirectory | `Zenith2/Tools/ZenithCook/Internal/Package/` | PACK (tool ASSET's) | `ZENITH_HOST` |
| PACK scripts | scripts | `Zenith2/Tools/Package/` | PACK | the `win` preset (PowerShell), every host (CMake scripts) |
| `ZenithPackage.cmake` | CMake helper | `Zenith2/CMake/ZenithPackage.cmake` | PACK (directory SHIP's) | every preset |

### ZenithPackage public root headers

`Zenith_PackageTypes.h`, `Zenith_PackageManifest.h`, `Zenith_AssetPackDelivery.h`, `Zenith_EntitlementQuery.h`, `Zenith_AssetPackInstaller.h`, `Zenith_InstallFileSystem.h`, `Zenith_UpdateApplier.h`, `Zenith_InstallRecord.h`, `Zenith_UpdateMigration.h`, `Zenith_PackageService.h`; declarations in `Zenith2/Docs/Design/PACK/PublicAPI.md`.

### ZenithPackage internal files

| File | Content |
|---|---|
| `Internal/Zenith_PackageTypes.cpp` | the `Zenith_ToString` tables |
| `Internal/Formats/Zenith_PackageManifest.cpp` | the `package.zpkm` decoder and checks M1–M11, `CheckIdentity` |
| `Internal/Formats/Zenith_InstallJournal.h`, `.cpp` | journal encode, decode, J1–J6 |
| `Internal/Formats/Zenith_InstallRecord.cpp` | record encode, decode, R1–R5, `Zenith_ClassifyUpdate` |
| `Internal/Formats/Zenith_DeliveryStateFile.h`, `.cpp` | the state file encode, decode, D1–D5 |
| `Internal/Formats/Zenith_VerifiedStampFile.h`, `.cpp` | the `verified.zinv` field codec and VS1–VS6 shared by `Zenith_VerifiedStampFile` (decode, `FindStamp`) and `Zenith_VerifiedStampSet` (decode, merge, `Encode` with the overflow rule); both types are declared in the public `Zenith_UpdateApplier.h` |
| `Internal/Delivery/Zenith_PackVerifiedCache.h`, `.cpp` | the pack cache check of A3 and the record writes of "Verified cache" item 3 |
| `Internal/Install/Zenith_InstallTreeMountSource.h`, `.cpp` | a `Zenith_MountSource` over a subtree of a `Zenith_InstallFileSystem` (B6's source, its `GetIdentityStamp` forwarded) |
| `Internal/Install/Zenith_UpdateApplier.cpp`, `Zenith_InstallRecovery.cpp`, `Zenith_PendingInstalls.cpp` | the applier (B1–B7, the steps, the sweep), recovery, `Zenith_RunPendingInstalls` with the instance guard |
| `Internal/Formats/Zenith_InstallState.h`, `.cpp` | `install-state.zins` encode, decode, S1–S4 |
| `Internal/Install/Zenith_MemoryInstallFileSystem.cpp` | the memory implementation and its durability model |
| `Internal/Install/Zenith_InstallPath.h`, `.cpp` | path validation for install-relative paths |
| `Internal/Delivery/Zenith_AssetPackInstaller.cpp`, `Zenith_PackStateMachine.cpp`, `Zenith_PackVerification.cpp`, `Zenith_PackContainerSource.h`, `.cpp` | the installer (its `PumpShutdown` and replay mode), its two transition tables, the verification pipeline, the container wrapper and the per-mount `Zenith_PackReleaseBlock` with the draining list. The pack half of the verified cache (Q90 ruled (a)) has its own row, `Internal/Delivery/Zenith_PackVerifiedCache.h`, `.cpp`. |
| `Internal/Delivery/Zenith_DeliverySlotTable.h`, `.cpp` | the update slot table: a state and a result cell per pack, one confirmation cell, `Close` (lock 650) |
| `Internal/Delivery/Zenith_SimulatedAssetPackDelivery.cpp` | the simulated delivery |
| `Internal/Zenith_EntitlementQuery.cpp` | `Zenith_ConfigEntitlementQuery`, `Zenith_ForwardingEntitlementQuery` |
| `Internal/Migration/Zenith_UpdateMigration.cpp`, `Zenith_PackageMigrationSteps.cpp` | the registry and the built-in steps |
| `Internal/Zenith_PackageService.cpp`, `Zenith_DlcMount.cpp`, `Zenith_PackageConfigKeys.cpp` | the service, Windows DLC mounting, the configuration keys |

### ZenithPackage platform files

| File | Content |
|---|---|
| `Internal/Windows/Zenith_HostInstallFileSystem_Windows.cpp` | `CreateFileW` with `FILE_FLAG_OPEN_REPARSE_POINT`, `MoveFileExW` (durability from the directory flushes alone), `FlushFileBuffers` on files and on directory handles opened `GENERIC_READ \| GENERIC_WRITE` with `FILE_FLAG_BACKUP_SEMANTICS`, `RemoveDirectoryW` and `DeleteFileW` for links, `LockFileEx` on the two lock files, `GetDiskFreeSpaceExW`, `GetVolumeInformationByHandleW`; three retries 100 ms apart on `ERROR_ACCESS_DENIED` and `ERROR_SHARING_VIOLATION`; the escape check of PLAT's design repeated on every open |
| `Internal/Android/Zenith_HostInstallFileSystem_Android.cpp`, `Internal/Linux/Zenith_HostInstallFileSystem_Linux.cpp` | `openat` with `O_NOFOLLOW`, `renameat`/`renameat2(RENAME_NOREPLACE)`, `unlinkat`, `fsync` of files and directories, `fstatat(AT_SYMLINK_NOFOLLOW)`, `flock` shared and exclusive, `statvfs`, `fstatfs` for the volume kind, `/proc/self/fd` escape check |
| `Internal/Android/Zenith_PlayAssetPackDelivery_Android.cpp`, `Zenith_PlayAssetPacksJniTable.h` | the JNI side of the Play delivery over `Zenith_Platform::GetAndroidJavaContext` (no SDL call), the native-method table |
| `Internal/Android/Java/org/zenith2/pack/Zenith2AssetPacks.java` | the Java bridge, copied into bundle projects only |
| `Internal/Windows/Zenith_PlayAssetPackDelivery_Windows.cpp`, `Internal/Linux/Zenith_PlayAssetPackDelivery_Linux.cpp` | the factory returning `PACKAGE_ERROR_NOT_SUPPORTED` |

### ZenithPackage test files

`Tests/Zenith_PackageManifest_Tests.cpp`, `Zenith_InstallJournal_Tests.cpp`, `Zenith_InstallRecord_Tests.cpp`, `Zenith_DeliveryStateFile_Tests.cpp`, `Zenith_VerifiedStampFile_Tests.cpp`, `Zenith_PackageFormats_Tests.cpp`, `Zenith_MemoryInstallFileSystem_Tests.cpp`, `Zenith_HostInstallFileSystem_Tests.cpp`, `Zenith_UpdateApplier_Tests.cpp`, `Zenith_SimulatedAssetPackDelivery_Tests.cpp`, `Zenith_AssetPackDelivery_Tests.cpp`, `Zenith_AssetPackInstaller_Tests.cpp`, `Zenith_UpdateMigration_Tests.cpp`, `Zenith_PackageService_Tests.cpp`, `Zenith_EntitlementQuery_Tests.cpp`; `Zenith_DeliverySlotTable_Tests.cpp`, `Zenith_PackageTestSigner_Tests.cpp`; `Tests/Support/Zenith_PackageTestFormats.h` and `Tests/Support/Zenith_PackageTestSigner.h` (header-only, [Z2-N-ARCH-255]); `Tests/Fuzz/PackageManifest.cpp`, `InstallJournal.cpp`, `InstallRecord.cpp`, `DeliveryState.cpp`, `VerifiedStamps.cpp`; `Tests/Examples/Zenith_PackageService_Example.cpp`. The tests and their cases are `Zenith2/Docs/Design/PACK/Tests.md`.

### Files PACK owns in other targets

| File | Target | Its owner's permission |
|---|---|---|
| `Zenith2/Engine/ZenithRuntime/Internal/Package/*`, `Internal/ScriptBindings/Zenith_PackageScriptBindings.cpp`, `Tests/Zenith_RuntimePackage_Tests.cpp` | ZenithRuntime | cross-area row PK3 to MOD (the SAVE precedent) |
| `Zenith2/Tools/ZenithCook/Internal/Package/*`, `Tests/Zenith_Package*_Tests.cpp` | ZenithCook | assigned by ASSET (`Zenith2/Docs/Design/ASSET.md` → "Library and file layout") |
| `Zenith2/CMake/ZenithPackage.cmake` | CMake helpers | cross-area row PK5 to SHIP |
| `Zenith2/Player/GradleTemplate/assetpack/build.gradle.kts.in` | the Gradle template | cross-area row PK4 to MOD |
| `Zenith2/Tests/Package/**` | `Zenith_Integration_Package` | PACK's own suite |

### ZENITH_SUPPORTED matrix

`ZenithPackage` is `ZENITH_ALL`: the applier, the memory and host install file systems, the slot table and the simulated delivery run on the headless `linux-tsan` host with manifests signed in process (`Zenith_PackageTestSigner.h`), so every state machine is tested under TSan; the Play delivery compiles on both Android ABIs; Windows and Linux compile its `NOT_SUPPORTED` stub. No source is excluded per configuration ([Z2-N-ARCH-171]); the Shipping difference on Android is a value (`m_bInstallIncoming`), not a build exclusion.

## Public API

### API map

| Need | Use |
|---|---|
| read the package's unit and archive table | `Zenith_PackageManifest` |
| deliver Android asset packs | `Zenith_AssetPackDelivery` (Play or simulated), driven by `Zenith_AssetPackInstaller` |
| gate DLC | `Zenith_EntitlementQuery` |
| apply patches and DLC before mounting | `Zenith_RunPendingInstalls`, `Zenith_UpdateApplier`, over a `Zenith_InstallFileSystem` |
| detect and migrate an installed update | `Zenith_InstallRecord`, `Zenith_ClassifyUpdate`, `Zenith_UpdateMigrationRegistry` |
| the one object the composition root owns | `Zenith_PackageService` |
| scripts | the `Package.*` functions and events |

Every declaration with its contract block is `Zenith2/Docs/Design/PACK/PublicAPI.md`.

### Handle kinds used

- `Zenith_OperationHandle` for every asynchronous read and background hashing job ([Z2-N-CONTRACT-091]); the installer owns them.
- `Zenith_MountSourceID` (PLAT's generational value) for each mounted archive source.
- No world, device or asset handle crosses PACK's API; packs, units, archives and DLC are named by strings, which are persistent identities of the package.

### Configuration keys

Registered by `Zenith_RegisterPackageConfigKeys` at startup step 6 (library-layer order). `package.reverifyOnStartup` turns off every stamp skip of `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache" for the run.

| Key | Type | Default | Meaning |
|---|---|---|---|
| `package.autoFetchFastFollow` | bool | true | fetch fast-follow packs Play has not started |
| `package.verifyChunkBytes` | u32 | 1,048,576 | read size of the verification pipeline (65,536 … 16,777,216, a power of two) |
| `package.verifyBuffersPerArchive` | u32 | 4 | buffers per archive in verification (1 … 16) |
| `package.maxConcurrentVerifications` | u32 | 2 | packs verified at once (1 … 8) |
| `package.removeOnVerifyFailure` | bool | true | remove and re-fetch once per game request after a failed verification |
| `package.reverifyOnStartup` | bool | false | hash every delivered pack, patch file and DLC file in full this run, skipping no file on a matching stamp (Algorithms.md "Verified cache"); records are still written |
| `package.networkRetrySeconds` | u32 | 30 | back-off before an automatic re-fetch after a network failure (5 … 600); at most `uPACKAGE_MAX_NETWORK_RETRIES` (3) per pack per process |
| `package.initialQueryTimeoutMs` | u32 | 500 | the service's wait for Play's initial states (0 … 5,000) |
| `package.grantedEntitlements` | string list | empty | entitlements `Zenith_ConfigEntitlementQuery` grants |
| `package.simulatedDelivery` | bool | false | use the simulated delivery (refused in Shipping builds) |
| `package.simulatedDelivery.root` | string | empty | the `Simulated/` directory of a Debug or Release Windows package (Algorithms.md "Simulated deliverable units"), an absolute host path or relative to the assets root; read in place, nothing is written; never read in a Shipping build |
| `package.simulatedDelivery.bytesPerFrame` | u64 | 8,388,608 | simulated bandwidth |
| `package.shutdownDrainMs` | u32 | 1,000 | shutdown step 4's bound on waiting for unmounted sources' release (0 … 5,000; Algorithms.md "Shutdown drain") |
| `package.replayMountTimeoutMs` | u32 | 60,000 | under `--replay`, the wait for the recording's packs to mount (1,000 … 600,000; Algorithms.md "Replay pack set") |

## Data structures and formats

### Format index

| File | Kind | Specified in |
|---|---|---|
| `format-registry.json` | JSON, committed | `Zenith2/Docs/Design/PACK/Formats.md` → "Format registry file" |
| `package.zpkm` | binary, cooked, one per layout | → "Package manifest" |
| `update.zman`, `dlc.zman` | binary, signed | → "Update manifest" |
| `install-journal.zinj` | binary, transient | → "Install journal" |
| `install-state.zins` | binary, user state | → "Install state file" |
| `verified.zinv` | binary, user state (Q90 ruled (a)) | → "Verified stamp file" |
| `install-record.zinr` | binary, user state | → "Install record" |
| `delivery-state.zdls` | binary, user state | → "Delivery state file" |
| `Package.*` publication payloads | binary, 96 bytes | → "Package event payload" |
| `.zpak` | ASSET's layout, PACK's policy | → "Archives PACK writes" |
| `mount-manifest.json` | PLAT's schema, PACK's values | → "Mount manifests PACK writes" |
| `package.recipe.json` | authoring JSON | → "Packaging recipe" |
| `size-report.json`, `partition.json`, `bundle-report.json`, `patch-report.json` | reports | → "Size report and partition report"; `Zenith2/Docs/Design/PACK/Tooling.md` → "Bundle size measurement" |
| device specs, bundle config | committed JSON | `Zenith2/Docs/Design/PACK/Formats.md` → "Bundle configuration and device specs" |

### In-memory layouts that matter

| Structure | Size | Note |
|---|---|---|
| pack record in the installer | 256 bytes per pack, at most 256 packs | state, intent, attempt, status snapshot, verification state, source IDs |
| delivery update cell | 112 bytes (`Zenith_DeliveryUpdate`, 104, plus a dirty flag and padding), 2 per pack plus 1 (513 cells, 57,456 bytes) | written under lock 650 by Play's threads |
| verification arena | `maxConcurrentVerifications × verifyBuffersPerArchive × verifyChunkBytes` (8 MiB by default) | allocated at Create, part of the package service resident memory row Z2-B-PACK-009 |
| event queue | 512 × 104 bytes (53,248) | `Zenith_PackageEvent` |
| world record | per world: 512 × 96 bytes of the table's result arena (declared by `DeclareSource`), a deferral queue of 256 + 64 events × 104 bytes, and the 6,168-byte `Zenith_PackageStateResource` | `Zenith_RuntimePackageWorldRecord` |
| release block | 24 bytes per mount operation, plus one container wrapper per archive, allocated at A4 | `Zenith_PackReleaseBlock` (Algorithms.md "Mounting and unmounting") |
| parsed `package.zpkm` | at most 5,374,016 bytes plus a name index | one per process |
| delivery-state stamp table | at most 25,600 × 48 bytes (1,228,800), beside the pack records | the verified packs' archive stamps (Algorithms.md "Verified cache" item 3); part of Z2-B-PACK-009 |
| `Zenith_VerifiedStampFile` | a copy of `verified.zinv`, at most 8,437,792 bytes, from the close of startup step 4 to shutdown step 6 | the stamp lookup of the patch mount and DLC mounting; part of Z2-B-PACK-009 |
| B6's stamp set and the step-3b/4b merge | at most 65,536 × (48 + name) bytes for B6, plus a decoded `Zenith_VerifiedStampSet` and the encoded file (each at most 8,437,792 bytes) during one commit | startup step 4 only, freed before step 5; part of the startup peak, not of P5's steady state |

### Installation layout summary

Windows: `<install>/Content/` (package), `Patches/` (committed patches, `.incoming`, `.trash`), `DLC/`, `.zenith2-install/` (journal, install state, the `lock` and `running` lock files). Android: the base APK's `assets/`, install-time pack splits, Play's pack directories, the user mount's `cache/package/`. The trees are `Zenith2/Docs/Design/PACK/Formats.md` → "Installation layouts".

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| durable steps and the two interruption models | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Durability model" |
| install, reject, clean, remove, sweep; recovery; pending installs; the two locks | → "Install state machine", "Recovery table", "Pending installs at startup", "Install lock and instance guard" |
| pack verification, the verified cache, mount, unmount, fetch policy, network retry | → "Verification pipeline", "Verified cache", "Mounting and unmounting", "Fetch policy", "Network retry" |
| startup of the service, Windows DLC | → "Package service startup", "Windows DLC mounting" |
| classification and migration | → "Update classification", "Installed-update migration" |
| partition, chunking, dictionaries, patch diff | → "Partition algorithm", "Chunking", "Dictionary policy", "Patch diff" |
| event publication | → "Event publication and replay" |

### State machine index

| Machine | States | Table |
|---|---|---|
| the applier's operation | none, COMMITTING, COMMITTED, CLEANING, REJECTING, REMOVING | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Install phases and events" |
| an asset pack | 19 `Zenith_AssetPackState` values, 26 events (25 per pack in 23 columns, plus the global confirmation request) | → "Asset pack transitions" |
| the install record's migration | DONE, PENDING | → "Installed-update migration" |
| the simulated delivery | Play's statuses | → "Simulated delivery" |

### Frame-level flow

| When | What PACK does |
|---|---|
| startup step 4, closing action | `Zenith_RuntimePackage_RunPendingInstalls`: the volume check, the two locks, recover, sweep obsolete patches, install incoming bundles (main thread, no VFS); the instance guard stays held |
| startup step 5 | (PLAT) mounts the package, the install-time packs and the highest verified patch |
| startup step 11, closing action | `Zenith_RuntimePackage_Create`: the service, the installed-update migration, Windows DLC, the installer with its initial query |
| startup step 15, during the modules' `Start` and at its end | the entitlement provider: ONLINE's query through `ProvideEntitlementQuery`, else the configuration query (`Zenith_RuntimePackage_SetEntitlementProvider`, `Zenith_RuntimePackage_AfterModulesStarted`) |
| world creation step 4 | `Zenith_RuntimePackage_AttachWorld`: the world's record, the resource `Zenith.Package.State` at its initial value, `ASYNC_SOURCE_PACKAGE` declared, its handler set, the snapshot due |
| frame prologue step 1b | `Zenith_RuntimePackage_RunPrologue`: poll the delivery, advance pack states, pump verification, apply mounts and unmounts, `OnMountsChanged` when needed, then, per world, its snapshot when it is new and one `ASYNC_SOURCE_PACKAGE` result per queued event (none under `--replay`) |
| each world's next tick boundary, step 1 | the world record's `OnPublished`, in completion-ID order with every other source: `Zenith.Package.State` refreshed, `Package.*` events for phase 9 |
| after `RunFrame`, the first call of MOD's `Zenith_FrameLoop_AfterRunFrame` | `Zenith_RuntimePackage_ApplyCommands`: the `Package.*` module commands the frame's sync-point playbacks buffered reach the installer (not under `--replay`); the installer reads them at the next prologue step 1b |
| shutdown step 4 | `Zenith_RuntimePackage_BeginShutdown`: stop verification, unmount delivered packs and Windows DLC, `PumpShutdown` until they are released or `package.shutdownDrainMs` passes (nothing verified or mounted after it begins), persist the state file when dirty (never on the device-loss exit), clear the entitlement provider |
| shutdown step 6 | destroy the service (the installer, the delivery, the Java bridge) after the modules, before the job system, then the forwarding entitlement query |
| shutdown step 10b | `Zenith_RuntimePackage_ReleaseInstanceGuard`: release `INSTALL_LOCK_RUNNING` and destroy the install file system, after `Zenith_VirtualFileSystem` and before `Zenith_Platform` |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_PackageService`, `Zenith_AssetPackInstaller`, `Zenith_AssetPackDelivery` implementations, `Zenith_UpdateMigrationRegistry`, `Zenith_EntitlementQuery` | `MAIN_THREAD` |
| the Play delivery's update slot table | written by Play's threads (the Android UI thread, task threads), read by the main thread, under lock 650 |
| `Zenith_UpdateApplier`, `Zenith_InstallFileSystem` implementations | `OWNER_THREAD` (the main thread, from the closing action of step 4 to shutdown step 10b) |
| `Zenith_ForwardingEntitlementQuery` | `MAIN_THREAD` (the bridge's, from the closing action of step 11 to shutdown step 6) |
| `Zenith_PackageManifest`, the formats' value types | `IMMUTABLE` |
| hashing jobs | background jobs, one per archive at a time, handed a `Zenith_Sha256Stream` at job boundaries |

### Frame-graph placement

PACK adds no frame-graph node. Its per-frame work is prologue step 1b on the main thread (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue"); its hashing runs as background jobs (`bg/Package.Verify`) the scheduler fits around nodes; its results are completed into each world's asynchronous result table in the prologue and published by the world's tick-boundary node with every other source's results, as PLAT's platform events are ([Z2-N-ARCH-791]).

### Lock-order entries

| Level | Constant | Owner | Guards | Short-hold | Acquired on |
|---|---|---|---|---|---|
| 650 | `uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES` | ZenithPackage | the delivery update slot table (state, result and confirmation cells, the closed flag) | yes | Play's callback threads; the main thread in `Poll` and `Close` |

A holder of 650 allocates nothing, calls nothing and takes no other lock; the main thread never holds it while calling Java, and a native callback holds it only inside the call, so the Java bridge's monitor and the lock cannot deadlock. Level 640 is `uLOCK_LEVEL_ACCESSIBILITY` (ZenithPlatform) and 650 is the next free level; the row is `Zenith2/Docs/Contracts.md` → "Lock ordering table" row 650 `uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES` ([Z2-N-CONTRACT-320]).

### Threads and background operations

PACK starts no thread. Play's service downloads in its own process; the bridge's callbacks run on threads Play owns and enter native code only through the Java bridge's handle gate (`Zenith2/Docs/Design/PACK/Tooling.md` → "Java bridge"). Hashing jobs and `Zenith_AsyncIO` reads are operations the installer owns and cancels.

### Startup and shutdown

| Point | Action | Core-doc row |
|---|---|---|
| startup step 4, closing action (after `Zenith_Platform`, before the virtual filesystem; no new `Zenith_StartupStep`, its time is step 4's) | pending installs; the instance guard taken, after waiting without bound for another process's install to hand its guard over; step 4's unwind releases it first (row PK22) | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" step 4 |
| startup step 11, closing action (after the asset registry and `Zenith_RuntimeSave_CreateSystem`, before the RHI device; its time is step 11's) | the forwarding entitlement query and the service; an absent or invalid `package.zpkm` in Shipping is exit code 3; under `--replay`, a recorded pack that cannot be mounted is exit code 5 | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" step 11 |
| startup step 15 | the entitlement provider ("Frame-level flow") | row PK21; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" step 15 |
| world creation step 4 | `Zenith_RuntimePackage_AttachWorld` | row PK15 |
| shutdown step 4 | `Zenith_RuntimePackage_BeginShutdown`: the installer's `BeginShutdown` (the shutdown rule: A7 on every verification, A5 on every mounted pack, intents kept) and the Windows DLC unmounts, then `PumpShutdown` and a 1 ms sleep in a loop until every V3 and every X (or `package.shutdownDrainMs`, 1,000 ms, for the releases), no verification starting and nothing mounting after `BeginShutdown`; the state file written with `WriteAtomic` and waited for only when it is dirty, and neither written nor waited for on RHI's device-loss clean exit; then the forwarder's provider cleared, before step 6 destroys ONLINE's query with its module | `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" step 4 |
| shutdown step 6 | destroy the service after the modules and before the job system; the Play delivery's destructor calls the Java bridge's `shutdown()`, which zeroes the bridge's native handle under its monitor (waiting for any native call in progress) before unregistering the listener, so no native callback can run after it returns, then closes the slot table; then the bridge destroys the forwarding entitlement query, which has had no provider since step 4 | `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" step 6 |
| shutdown step 10b | release the instance guard and destroy the install file system, after `Zenith_VirtualFileSystem` (whose patch-mount sources hold handles on `Patches/<v>/`) and before `Zenith_Platform` | `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" step 10b |

### Cancellation

| Operation | Cancellation points | Completion |
|---|---|---|
| a pack's verification | between chunk reads; between hashing jobs | reported after every read and job completed; buffers released by the installer ([Z2-N-CONTRACT-335]) |
| a pack download | Play's `cancel`; the state machine's CANCELLING | the pack reaches NOT_INSTALLED after `removePack` succeeded |
| an install | none mid-step: each durable step completes; between steps an interruption is recovered at the next start that holds both locks | the journal's absence |
| the service at shutdown | `BeginShutdown` | `PumpShutdown` returns true: every verification stopped and every unmounted source released; the releases are waited for at most `package.shutdownDrainMs` ("Shutdown drain" in Algorithms.md) |

### Error enumeration

`Zenith_PackageError` (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Zenith_PackageTypes header"):

| Enumerator | Returned when |
|---|---|
| `PACKAGE_ERROR_OUT_OF_MEMORY` | an allocation of a factory or a table failed |
| `PACKAGE_ERROR_INVALID_DESC` | a desc or registration violates its rules |
| `PACKAGE_ERROR_MANIFEST_MISSING` | `package.zpkm` is absent |
| `PACKAGE_ERROR_MANIFEST_CORRUPT` | it fails a check M1–M11 |
| `PACKAGE_ERROR_MANIFEST_VERSION` | another format version of a PACK cooked format |
| `PACKAGE_ERROR_MANIFEST_IDENTITY` | `package.zpkm` does not match the mount manifest's build ID and pre-patch content hash, or its layout does not match the platform |
| `PACKAGE_ERROR_UNKNOWN_PACK` | a name no unit has |
| `PACKAGE_ERROR_NOT_DELIVERABLE` | a request names a unit with the PRESENT flag (base, folded or install-time) |
| `PACKAGE_ERROR_DELIVERY_UNAVAILABLE` | the Java bridge, Play or the Play Store is absent or unusable |
| `PACKAGE_ERROR_DELIVERY_FAILED` | Play reported a failure without a more specific mapping |
| `PACKAGE_ERROR_NETWORK_UNAVAILABLE` | Play reported a network error |
| `PACKAGE_ERROR_INSUFFICIENT_STORAGE` | Play reported insufficient storage |
| `PACKAGE_ERROR_ENTITLEMENT_DENIED` | a DLC request without the entitlement |
| `PACKAGE_ERROR_VERIFY_FAILED` | a delivered archive's size or SHA-256 differs from the base `package.zpkm` |
| `PACKAGE_ERROR_PACK_BUSY` | a request the pack's state refuses (the BUSY cells of `Zenith2/Docs/Design/PACK/Algorithms.md` → "Asset pack transitions"; a confirmation while no pack waits for Wi-Fi or consent) |
| `PACKAGE_ERROR_CANCELLED` | an operation completed as cancelled |
| `PACKAGE_ERROR_SHUTTING_DOWN` | a request after `BeginShutdown` |
| `PACKAGE_ERROR_MOUNT_FAILED` | ASSET's archive checks or `AddSource` refused a pack's archive, or A4's release block or a container wrapper could not be allocated |
| `PACKAGE_ERROR_JOURNAL_CORRUPT`, `PACKAGE_ERROR_JOURNAL_VERSION` | the install journal fails J1–J6, or has another version |
| `PACKAGE_ERROR_STATE_FILE_CORRUPT` | the delivery state file fails D1–D5, or the install state file S1–S4 |
| `PACKAGE_ERROR_INSTALL_ROOT_READ_ONLY` | the install root denies writes |
| `PACKAGE_ERROR_INSTALL_IO` | any other OS failure of the install file system (the OS code is logged), and an install root on an unsupported volume |
| `PACKAGE_ERROR_INSTALL_LOCKED` | another process holds `INSTALL_LOCK_APPLY` past the wait (this start then waits for `INSTALL_LOCK_RUNNING` shared and installs nothing), another instance holds `INSTALL_LOCK_RUNNING`, or an applier is created without both locks |
| `PACKAGE_ERROR_LOW_DISK` | free space below the applier's margin |
| `PACKAGE_ERROR_PATH_INVALID` | an install-relative path breaks the path rules |
| `PACKAGE_ERROR_UPDATE_MANIFEST_INVALID` | U0 (a), U1–U3, U7 or U8 failed (an unlisted entry, a subdirectory or a link; format, signature, names) |
| `PACKAGE_ERROR_UPDATE_WRONG_TARGET` | U4–U6 or U11 failed (release line, build, cook version, base package) |
| `PACKAGE_ERROR_UPDATE_NOT_NEWER` | U10 failed |
| `PACKAGE_ERROR_UPDATE_FILE_MISMATCH` | U9 failed |
| `PACKAGE_ERROR_UPDATE_INCOMPLETE` | U0 (b): a listed file of an incoming bundle or of a Windows DLC is absent (the bundle is left in place for a later start), the bundle's directory or manifest is absent (B1, B2), or a listed file's stamp changed while B6 hashed it (left in place) |
| `PACKAGE_ERROR_DLC_INCOMPATIBLE` | a DLC whose unit, order or catalogue fragment does not fit the base |
| `PACKAGE_ERROR_MIGRATION_STEP_FAILED` | a migration step failed (its fallback ran) |
| `PACKAGE_ERROR_NOT_SUPPORTED` | the Play delivery on a non-Android platform; the simulated delivery requested in Shipping |
| `PACKAGE_ERROR_RECORD_CORRUPT` | the install record fails R1–R5 |
| `PACKAGE_ERROR_PLATFORM` | a JNI call failed |
| `PACKAGE_ERROR_REPLAY_CONTENT_MISSING` | under `--replay`, `Zenith_RuntimePackage_Create` found a unit of the recorded pack set R not delivered, failing verification or not mounted within `package.replayMountTimeoutMs`, or a Windows DLC whose installed revision is not the recorded one (Algorithms.md "Replay pack set"); MOD's step-11 handler maps it to startup exit code 5, every other error of `Create` to exit code 3 |

### Error mapping at boundaries

| From | To | Rule |
|---|---|---|
| `Zenith_FileError`, `Zenith_IOError` (PLAT) | `Zenith_PackageError` | `NOT_FOUND` → `MANIFEST_MISSING` (for `package.zpkm`) or `UPDATE_INCOMPLETE` (for a bundle file); `LOW_DISK` → `LOW_DISK`; `ARCHIVE_CORRUPT` → `MOUNT_FAILED`; every other → `INSTALL_IO` or `DELIVERY_FAILED` by operation |
| `Zenith_UpdateVerifyError` (PLAT, assumed) | `Zenith_PackageError` | format and signature → `UPDATE_MANIFEST_INVALID`; target → `UPDATE_WRONG_TARGET`; files → `UPDATE_FILE_MISMATCH` |
| `Zenith_DeliveryErrorCode` | `Zenith_PackageError` | `Zenith2/Docs/Design/PACK/Algorithms.md` → "Play status and error mapping" |
| `Zenith_PackageError` | startup exit code | `MANIFEST_*` with `m_bShippingRules` → 3 (step 11); under `--replay`, a pack of the recording that cannot be mounted → 5 (step 11, the replay identity code of [Z2-N-ARCH-784]); every other error of PACK's closing actions of steps 4 and 11 is logged and startup continues |
| `Zenith_PackageBuildError` | ZenithCook exit code | `Zenith2/Docs/Design/PACK/Tooling.md` → "ZenithCook package modes" |

### Luau errors versus asserts

Every Luau misuse is a catalogued controlled error ZS2601–ZS2603 or the binding ABI's generic context error ZS1702 (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Script-facing API"), never an assert. Asserts guard PACK's own preconditions only (a null desc pointer, a call off the main thread). Nothing read from disk, from Play or from a bundle reaches an assert.

## Diagnostics and profiling

### Counters

| Counter | Kind | Meaning |
|---|---|---|
| `package.packs.mounted`, `package.packs.downloading`, `package.packs.failed` | GAUGE | packs per state |
| `package.delivery.updates_delta` | DELTA | delivery updates polled this frame |
| `package.delivery.ignored_events`, `package.delivery.unknown_status`, `package.delivery.stale_updates` | CUMULATIVE | ignored cells, unmapped statuses, dropped stale sequences |
| `package.delivery.result_overwritten`, `package.delivery.writes_after_close` | CUMULATIVE | defect counters, 0 in every test ("Delivery update slots") |
| `package.delivery.network_retries` | CUMULATIVE | automatic re-fetches after a network failure |
| `package.verify.bytes`, `package.verify.failures`, `package.verify.cache_hits` | CUMULATIVE | verification work; `cache_hits` counts the pack archives and DLC files whose hash a matching stamp skipped (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache"; the patch mount's skips are PLAT's `vfs.patch.stamp_skips`) |
| `package.mounts.added`, `package.mounts.removed` | CUMULATIVE | archive sources |
| `package.install.installed`, `package.install.rejected`, `package.install.recovered`, `package.install.obsolete_removed`, `package.install.transient_retries`, `package.install.obsolete_deferred`, `package.install.guard_waits`, `package.install.stamp_write_failures` | CUMULATIVE | the applier's report (`stamp_write_failures`: best-effort writes of `verified.zinv` that failed, Algorithms.md "Verified cache" item 4) |
| `package.migration.steps_run`, `package.migration.steps_failed` | CUMULATIVE | migration |
| `package.events.full`, `package.events.dropped`, `package.events.high_water` | CUMULATIVE, CUMULATIVE, GAUGE | the event queue ([Z2-N-CONTRACT-345]) |
| `package.events.deferred`, `package.events.decode_failed`, `package.events.snapshots` | CUMULATIVE | events held by a world's deferral queue; malformed payloads dropped by the handler; world snapshots allocated |

### Log category and messages

Category `package`. Fixed formats: `package: pack <name> <from> -> <to>`; `package: pack <name>: verification failed: <archive>: <reason>`; `package: install <patch|dlc> <name>: <phase>: <reason>`; `package: pending installs: <i> installed, <r> rejected, <c> recovered`; `package: update <from> -> <to> (<kind>)`; `package: dlc <name>: <state> (<error>)`; `package: package.zpkm identity <hex> does not match the mount manifest <hex>`.

### On-screen profiler rows

A "Package" group in DEVT's overlay (`Zenith2/Docs/Design/DEVT/Overlay.md`, through the counters): one row per pack that is not MOUNTED or NOT_INSTALLED, with its state, progress and bytes; one row with the verification throughput of the last second.

### Frame-dump entry

`package`: the service's `Describe` output (manifest identity, update info, every pack's status, every DLC's state), written into DEVT's frame dump.

### Telemetry

Per run: the update kind, the pack states at exit, the time from fetch to MOUNTED per pack, verification failures; recorded through DEVT's telemetry counters, never containing an account or device identifier.

## Authoring and cooking

### What PACK authors

`Games/<Name>/package.recipe.json` (`Zenith2/Docs/Design/PACK/Formats.md` → "Packaging recipe"), an authoring document of type `Zenith.Package.RecipeDocument` registered by `Zenith_RegisterPackageDocumentTypes(Zenith_MigrationRegistry&)` in ZenithCook and ZenithEditor (cross-area row PK6 adds the row to ASSET's "Registered document types" table), and `Zenith2/Tools/Package/format-registry.json`.

### Editor surfaces

The editor opens the recipe as a document of its asset browser through the cook server, with the framework's preview, backup and journaled write; the size report and partition report are shown read-only in the editor's packaging panel EDIT hosts (cross-area row PK9). There is no visual recipe editor: the recipe is data authored as JSON (Z2-D-079 gives visual authoring to animation, terrain, UI and cinematics only).

### Cook integration

The shipping profile (`cook.profile=shipping`), the `pack.dictionaries` step and the three package modes (`Zenith2/Docs/Design/PACK/Tooling.md` → "ZenithCook package modes", "Shipping cook profile").

### Migration hooks

The recipe document's chain starts at version 1 with no step; each version increment ships a step and fixtures under `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/PackageRecipe/` ([Z2-N-CONTRACT-548]). PACK's binary formats are cooked or user state and are never migrated: a reader refuses another version, and the user-state files are rebuilt.

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| `package.zpkm` absent or invalid | Shipping rules: exit code 3; otherwise no units | `Test_PackageService_ShippingWithoutManifest_Fails` |
| `package.zpkm` identity mismatch | deliveries and DLC disabled, base content plays | `Test_PackageService_IdentityMismatch_DisablesDelivery` |
| incoming patch refused | removed, `Package.UpdateFailed`, previous content stays | `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports` |
| power loss or kill during an install | recovered at the next start to old or new | `Test_UpdateApplier_PowerLossAtEveryStep_Converges` |
| journal corrupt | rebuilt from directories | `Test_UpdateApplier_CorruptJournal_RebuildsFromDirectories` |
| install root read-only | nothing installed, logged once; an existing `running` file is locked shared read-only | `Test_UpdateApplier_ReadOnlyRoot_LeavesIncoming` |
| a bundle missing a listed file, or the base package's identity unknown | left in `.incoming` for a later start, logged once | `Test_UpdateApplier_MissingListedFile_LeftInPlace`, `Test_UpdateApplier_UnknownBaseHash_LeavesPatches` |
| more than 1,024 obsolete directories | the first 1,024 removed, the rest at later starts | `Test_UpdateApplier_ObsoleteBeyondJournalLimit_Deferred` |
| another process installing | `INSTALL_LOCKED` after the APPLY wait; this start then waits for `INSTALL_LOCK_RUNNING` shared, without bound, and starts on the installer's result | `Test_UpdateApplier_LockHeld_ReturnsLocked`, `Test_UpdateApplier_TimedOutWaiterNeverRunsGuardless` |
| another instance of the install running | no install, recovery or sweep; the patch mount mounts what is there | `Test_UpdateApplier_RunningInstanceBlocksInstall` |
| a bundle delivered without a flush, then power loss | the bundle is sealed before the commit; the old patch or the new one verifies, never only the base | `Test_UpdateApplier_UnsealedBundle_PowerLossKeepsOldPatch` |
| a link inside a bundle or a trash tree | refused (U0) or removed as itself; nothing outside the root is deleted | `Test_UpdateApplier_BundleWithJunction_DeletesLinkOnly` |
| an install root on FAT32, exFAT or a share | installs disabled, logged once, content plays | `Test_UpdateApplier_UnsupportedVolume_InstallsNothing` |
| pack download interrupted (network) | Play's FAILED `NETWORK_ERROR`: WAITING_FOR_NETWORK, re-fetched on the back-off (at most 3 times), resumes from Play's offset | `Test_AssetPackInstaller_NetworkLoss_ThenResume_Completes` |
| process death during a download | resumes from the persisted intent | `Test_AssetPackInstaller_ProcessDeathMidDownload_Resumes` |
| pack cancelled | rolled back to NOT_INSTALLED | `Test_AssetPackInstaller_Cancel_RollsBack` |
| delivered pack corrupt | removed and re-fetched once per game request, then FAILED | `Test_AssetPackInstaller_CorruptDelivery_RemovedAndRefetchedOnce` |
| pack still mounted at shutdown | unmounted and released in shutdown step 4 through `PumpShutdown`, before the installer is destroyed; its intent persisted | `Test_AssetPackInstaller_ShutdownWithMountedPack_ReleasesBeforeDestroy` |
| pack verifying, waiting for a slot or unmounting for a new version at shutdown | the verification is stopped; no verification starts and nothing mounts after `BeginShutdown`; each ends DELIVERED with its intent persisted | `Test_AssetPackInstaller_ShutdownCancelsVerification` |
| a Shipping package over a cooked tree with a stale or missing bake | refused before partitioning, exit 7 (`PACKAGE_BUILD_ERROR_BAKE_STALE`); Debug and Release warn | `Test_PackageMode_StaleBakesRefuseShipping` |
| a pack re-mounted, or a mount rolled back by M2 | each mount has its own release block; no removal is issued before that mount's sources are released | `Test_AssetPackInstaller_RemountThenRemove_WaitsForSecondRelease`, `Test_AssetPackInstaller_PartialMountRollback_LateReleaseIgnored` |
| a world created after a pack mounted | it receives a snapshot at the first prologue step 1b after its creation (frame k + 1's for a world created in prologue step 7 of frame k) | `Test_RuntimePackage_LateWorldReceivesSnapshot` |
| ONLINE's entitlement query not yet provided at Create | the forwarder answers UNKNOWN; the DLC mounts once the provider is set | `Test_PackageService_EntitlementProviderSwitch_RemountsDlc` |
| pack removed while read | files deleted only after release | `Test_AssetPackInstaller_RemoveMounted_WaitsForRelease` |
| Play unavailable | packs FAILED (`DELIVERY_UNAVAILABLE`), base plays | `Test_AssetPackInstaller_Transitions_EveryCell` |
| DLC not entitled or incompatible | not mounted, reported | `Test_PackageService_DlcMount_Cases` |
| migration step fails | fallback deletes its cache, startup continues | `Test_UpdateMigrationRegistry_FailingStepRunsFallback` |
| state file or record corrupt | rebuilt (re-query and re-verify; FIRST_RUN) | `Test_DeliveryStateFile_EachCheckRejects`, `Test_InstallRecord_ClassifyUpdate_EveryRow` |
| a verified file changed after it was recorded (a stamp differs), or `verified.zinv` missing or corrupt | that file (or, without a valid file, every file) is re-hashed in full; a mismatch refuses the mount as before; the record is rewritten at the next verification or commit | `Test_AssetPackInstaller_RestoredMtime_Reverifies`, `Test_PackageService_DlcMount_StampSkip`, `Test_VerifiedStampFile_EachCheckRejects`, PLAT's `Test_PatchMount_StampSkipAndRehash` |

### Missing and corrupt input

Every PACK parser treats its input as untrusted ([Z2-N-ARCH-951]): checks run in the order of their tables, the first failure is reported with its check number, and the input is never partly used. Undelivered content is not an error at the asset level: ASSET reports `ASSET_ERROR_NOT_INSTALLED` and its fallback policy applies (`Zenith2/Docs/Design/ASSET/Loading.md` → "Retry after a mount change").

### Allocation failure

Every factory unwinds on allocation failure ([Z2-N-CONTRACT-340]). After Create the installer allocates only at A4, as "Mounting and unmounting" in Algorithms.md states: one 24-byte release block, one container wrapper per archive and ASSET's archive source per archive, at most once per pack per verification, whose failure is M2 with `MOUNT_FAILED`; the event queue, the slot table and the verification arena are sized at Create, and nothing else allocates per frame. A Windows DLC mount allocates the same way. The applier's journal and record buffers come from the caller's arena.

### Device loss

PACK uses no GPU resource; a device loss does not affect it. The device-loss clean exit runs shutdown steps 4 and 6 as any shutdown, except that step 4 writes no delivery state file and waits for none (`bDeviceLossExit`, `Zenith2/Docs/Design/PACK/Algorithms.md` → "Shutdown drain"), so PACK's part of the 3 s SAVE's device-loss split leaves is the drain's bound alone.

### Low disk

The applier refuses to start an install or a sweep below its margin (recovery still runs: it only renames and deletes); Play reports insufficient storage for packs, surfaced as `Package.PackStateChanged` FAILED with `InsufficientStorage`; PACK's user-state writes go through `WriteAtomic` and its low-disk refusal (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"), failing which the previous state file stays.

### Budget breach

Z2-B-PACK-001 and the Play pack rows are download sizes that exist only after Gradle and bundletool, so the package mode cannot refuse them: `<Game>_Bundle` measures the built bundle (`Measure-Zenith2BundleSize`) and fails with `Invoke-Zenith2Package.ps1 -Bundle` exit code 8 when an adopted row is exceeded, and Stage 6 acceptance blocks on them (BR-ACCEPT); the package mode only warns when a pack's archive bytes exceed an adopted pack row (archive bytes bound the download from above). Windows install size breaches are found by MP-PKG at acceptance (BR-ACCEPT); the service's memory is fixed at Create (BR-ACCEPT).

## Test strategy

### Test summary

| Group | Count of named tests | Heading in Tests.md |
|---|---|---|
| formats and the registry | 27 | "Format tests", "Format registry tests" |
| install file systems and the applier | 56 | "Install file system tests", "Update applier tests" |
| delivery and the installer | 44 | "Simulated delivery tests", "Asset pack installer tests" |
| record, migration, service | 23 | "Install record and migration tests", "Package service tests" |
| runtime bridge | 15 | "Runtime bridge tests" |
| packer | 36 | "Packer tests" |
| scripts, bundletool, Gradle | 15 | "PowerShell and script-mode tests" |
| integration | 8 | "Integration suite" |
| device and emulator runs | 7 | "Device runs" |

The counts are a reading aid; no gate pins them ([Z2-N-CONTRACT-408]).

### Which gate step runs each group

`Test` runs every doctest, script-mode and PowerShell test on the presets of `Zenith2/Docs/Design/PACK/Tests.md` → "Test targets and gate steps"; `Sanitizers` runs the `z2-stress` cases under `win-asan` and `linux-tsan`; `Build -Preset android-arm64` compiles the Play bridge; the fuzz smokes run in `win-asan`; the integration suite runs on `win-debug` only (its fixtures are cooked by ZenithCook), while every applier, installer and slot-table test runs on `linux-tsan` too; device runs are `needs-human` tickets; Stage 6 acceptance runs the items of "Stage 6 acceptance items".

## Budget rows

### Rows PACK owns

| Row | Name | Status | PACK's role |
|---|---|---|---|
| Z2-B-PACK-001 | AAB base module download size | pending (pinned at Stage 6) | `Measure-Zenith2BundleSize` is its MP-PKG procedure (`get-size total --modules=base`, [Z2-N-BUDGET-173]); `<Game>_Bundle` refuses a breach once adopted (exit 8), and acceptance blocks on it (BR-ACCEPT) |
| Z2-B-PACK-002 | Windows install size OpenWorldSlice | pending | MP-PKG over the Shipping package tree |
| Z2-B-PACK-003 | Windows install size CinematicSlice | pending | same |
| Z2-B-PACK-004 | Windows install size Game2D | pending | same |

The Stage 6 PACK ticket pinning the Play base-module limit records the documentation URL and retrieval date ([Z2-N-BUDGET-172]); the Stage 1 per-system budget ticket for PACK allocates 002–004 from published data (PACK has no Stage 1 harness, [Z2-N-BUDGET-021]).

### Answers to consumers of PACK rows

| Consumer | Their need | PACK's answer |
|---|---|---|
| DEVT D-SEL-CRASH (`Zenith2/Docs/Design/DEVT/Crash.md`, harness input 4: "the APK and Windows install size delta … the package budget (Z2-B-PACK-001, Z2-B-PACK-002)") | the size a crash handler adds to the base module and the Windows install | D-SEL-CRASH (open, Stage 2) is ruled before PACK's size report exists (Stage 4) and before Z2-B-PACK-001 has a value (Stage 6), so the Stage 2 harness measures its own APK and install deltas, as its input 4 already does; PACK's `size-report.json` (`native.byLibrary`, the static libraries' bytes from the linker map; `otherFiles`, a `crashpad_handler` executable or shared object) serves the re-check of Stage 4 onward of the selected candidate, the base module being measured by `bundle-report.json` and the Windows install by MP-PKG; a Stage 2 comparison against Z2-B-PACK-001 or -002 needs an interim allocation from the Stage 1 per-system budget ticket for PACK, or a threshold DEVT's harness states itself (cross-area row PK17) |
| AUDIO row AU21 | streamed sounds byte-addressable; the codec's size counted in Z2-B-PACK-001 | archive entries are always stored (Formats.md "Archives PACK writes"), and a section AUDIO requires stored is never recompressed or dictionary-encoded by the shipping profile (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Dictionary policy" step 7, row PK18); the codec library appears in `native.byLibrary` |
| D-SEL-CODEC, D-SEL-VIDEO (DecisionLog: consumers blocked include PACK) | package size of the candidates | the same report, per candidate build |
| COMPRESS (rows 001–004 measured against the compression ratio) | the ratio per codec | `compression.byCodec` in the size report |

### Pending new rows

In `Zenith2/Docs/Budgets.md` → "Package and storage rows" as Z2-B-PACK-005 to -011 (pending, with producers; [Z2-N-BUDGET-484]):

| Row | Name | Unit | Procedure | Platform | Breach | Producer |
|---|---|---|---|---|---|---|
| Z2-B-PACK-005 | Play asset pack download size | MB | MP-PKG | S24 | BR-ACCEPT; `<Game>_Bundle` also refuses an adopted value (exit 8) | Stage 6 PACK ticket pinning the Play base-module limit |
| Z2-B-PACK-006 | Play asset packs total download size | MB | MP-PKG | S24 | BR-ACCEPT; refused at bundle time once adopted | the same |
| Z2-B-PACK-007 | Play asset pack count | count | MP-PKG | S24 | BR-ACCEPT; refused at bundle time once adopted | the same |
| Z2-B-PACK-008 | Patch download size per release | MB | MP-PKG | Win | BR-REPORT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-009 | Package service resident memory (the installer's records and verification arena, the delivery cells, the event queue, the world records, the delivery-state stamp table and the `Zenith_VerifiedStampFile` copy of "In-memory layouts that matter") | MB | MP-SUBMEM | Win, S24 | BR-ACCEPT (no governor consumer: the memory is fixed at Create) | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-010 | Asset pack verification throughput floor | MB/s | MP-PKGVERIFY (`Zenith2/Docs/Budgets.md` → "Measurement procedure catalogue": one 1 GiB pack hashed through the installer on the device, median of 10) | S24, Win | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |
| Z2-B-PACK-011 | Cold-boot update verification time | added ms per GiB of installed update bytes present (committed patch, Windows DLC and delivered packs), reported separately for the first start after the install (full hash) and the second (stamp checks only) | W-Load's MP-BOOT with a committed 1 GiB content patch and an installed 1 GiB Windows DLC (Win) or a delivered 1 GiB fast-follow pack (S24), minus the same run without them, divided by the GiB present, median of 10, at the first and at the second start | Win, S24 | BR-ACCEPT | Stage 1 per-system budget ticket for PACK |

The tickets that cite them are blocked by their producers ([Z2-N-BUDGET-016]); Z2-B-PACK-009 is a member of MEM-WIN-RAM and MEM-S24 in "Allocation sums". Z2-B-PACK-011 exists because the first start after an install still re-hashes a committed patch, every Windows DLC and every delivered pack in full, and every later start pays the stamp checks; Q90 was ruled (a), so Z2-B-PACK-011 is measured at both the first and the second start after the install, and the row is never removed.

### Rows PACK is measured against

The W-Load cold boot rows (`Zenith2/Docs/Budgets.md` → "Workload definitions") are measured with no update installed; PACK's share of them is the closing action of step 4 with nothing pending (12 file-system calls, none of which writes: `Zenith2/Docs/Design/PACK/Algorithms.md` → "Complexity") and the initial query's wait of at most `package.initialQueryTimeoutMs`. Everything an installed update adds to a cold boot is bounded by Z2-B-PACK-011 and measured in Stage 6 acceptance with a patched install, an installed DLC and a delivered pack: the patch mount's U9 re-hash of every file of the highest patch at step 5 (patches are cumulative, R4, so this grows towards the package's size across a release line), the Windows DLC re-hash at the close of step 11, a delivered pack's pipeline from the first prologue, and an install's own hashing at step 4 followed by the patch mount's at step 5.

## Stage plan

### Stage 1

- The Stage 1 per-system budget ticket for PACK (rows 002–004 and 008–011), a `needs-human` ruling with no harness half.
- Pull-forward: none; PACK's Stage 2 work depends on D-SEL-HASH (open, Stage 1) for its checks and D-SEL-JSON (open, Stage 1) for the recipe.

### Stage 2

- The `ZenithPackage` library target with `Zenith_PackageTypes.h` (Z2-R-PACK-001, -003: the API-only slice, so the enumerations exist for ZenithCook's Stage 2 code and for the Stage 5 slices of ONLINE and the owners).
- The format registry, its validation tests and PACK's common format rules (Z2-I-PACK-01); the registry file starts with `"formats": []`, and every row (PACK's included) joins with the ticket that introduces its format and refusal test.
- The recipe document type, its compression policy and the shipping cook profile; the `pack.dictionaries` step (Z2-I-PACK-03), blocked by ASSET's cook step API and COMPRESS's training.

### Stage 4

- The partition, chunking, archive writer with read-back, the per-layout `package.zpkm`, the shipped mount manifests, the Windows and Android sideload layouts, `<Game>_Package`, the size and partition reports, the stale-bake check of package build step 1b (row PK26) (Z2-I-PACK-02, -04, part of -05); ASSET's archive reader is Stage 4 too.

### Stage 6

- `ZenithPackage`'s runtime: the applier with sealing, recovery and the instance guard, the install file systems, the installer, the simulated and Play deliveries, the record and migration, the service, the runtime bridge with its `ASYNC_SOURCE_PACKAGE` publication and the Luau surface (Z2-I-PACK-07, -09 to -16).
- The App Bundle build, the Java bridge, bundletool tests, the patch and DLC builders and signer (Z2-I-PACK-06, -08, -10, -11).
- The device runs and the acceptance items.
- The verified cache (Q90 ruled (a), `Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache"): `verified.zinv` and its `ZINV` registry row, the applier's B6 stamps and steps 3b and 4b, the pack cache check, `package.reverifyOnStartup`, `Package.ClearVerifiedCache` and `package.verify.cache_hits`, after PLAT's PK27 slice (declare-first) and the Stage 1 security-architecture ticket that confirms the stamp set; [Z2-N-ARCH-955], [Z2-N-ARCH-956] and [Z2-N-ARCH-966] are amended in `Zenith2/Docs/Architecture.md` (core-doc patch 2026-10-16g, row X366), and row X376 restates [Z2-N-ARCH-966]'s trust premise.
- Pull-forward (`Zenith2/Docs/Roadmap.md` → "Stage 6 Shipping qualification"): tickets depending on no unresolved decision (formats, applier, installer with the simulated delivery, builders) are scheduled in `Stage 5 — Rendering and simulation completion` with fix version `Z2 Stage 6`.

### Stage 7

Nothing: console packaging is SCOPE's deferred work (Z2-D-071); PACK's interfaces take it.

### API-only slices

`Zenith_PackageTypes.h` with the library target (Stage 2), then in Stage 5 (pulled forward) `Zenith_AssetPackDelivery.h`, `Zenith_InstallFileSystem.h`, `Zenith_EntitlementQuery.h` (with `Zenith_ForwardingEntitlementQuery`) and `Zenith_UpdateMigration.h`, so ONLINE's entitlement implementation and owners' migration steps can be written against them before the service lands; the runtime bridge's `Zenith_RuntimePackage_SetEntitlementProvider` and `Zenith_RuntimePackage_AfterModulesStarted` land with the bridge, before MOD's `ProvideEntitlementQuery` body calls them (row PK21).

## Decisions

### Settled decisions governing PACK

| ID | Use in PACK |
|---|---|
| Z2-D-027 | Play distribution in Stage 6: AAB, bundletool, PAD, installed-update migration, interrupted content installation; `Zenith_AssetPackInstaller` is its named noun |
| Z2-D-038 | JSON for authoring (the recipe), versioned binary for cooked data (every PACK binary format) |
| Z2-D-048 | cooked re-cook, patches within one release line, saves migrated by SAVE |
| Z2-D-004 | compression through `Zenith_Compression`, codec IDs in the data |
| Z2-D-010 | Windows and the S24; 16 KB pages on every split |
| Z2-D-012 | the package rows |
| Z2-D-016, Z2-D-053, Z2-D-019 | script requests as commands; pack state published at tick boundaries in completion-ID order through the async-result table, and replayed |
| Z2-D-024, Z2-D-075 | no dynamic code and no third-party content in any package |
| Z2-D-045, Z2-D-046 | results, no exceptions, no mutable globals: the Java bridge is an instance carrying the native state's address as a `long`, so no C++ global exists |
| Z2-D-082 | bundletool and the Play library pinned in the manifest |
| Z2-D-084, Z2-D-097 | ruling tickets and rulings appended in the ticket's diff |

Orchestrator rulings PACK applies: ASYNC-SOURCE-REGISTRY (the source kind requested from SCRIPT, row PK13), Q89 (a) (prologue allocations of a recorded kind only, none under `--replay`; the world snapshots are recorded allocations), PACK-R2 (the unbounded guard wait, the bridge-owned forwarding entitlement query, no Q90 code before its ruling, patch numbering monotonic per release line), Q90 (a) (HUMAN RULED 2026-10-16: the verified cache for packs, patches and DLC, specified in full and no longer fenced), MISUSE-BLOCKS-2 (ZS2600–ZS2699), FINAL-DOUBLES (the memory file system, the simulated delivery and the scripted entitlement query are implementations of declared seams), GOLDEN-FILES (RFC 8032's vectors copied by hand), Q55 (every binary fixture generated at test time).

### Open decisions PACK depends on

| Decision | Effect on PACK |
|---|---|
| D-SEL-HASH (open, Stage 1) | the check function of every PACK binary format |
| D-SEL-JSON (open, Stage 1) | the recipe's and the reports' JSON |
| D-SEL-CRASH (open, Stage 2) | measured by DEVT's harness itself; PACK's size report re-checks the selection from Stage 4 (row PK17); a selected Crashpad adds `crashpad_handler` to `otherFiles` |
| D-SEL-CODEC (open, Stage 2), D-SEL-VIDEO (open, Stage 2) | package size inputs; PACK is a listed consumer |
| Q90 (ruled (a) by the human, 2026-10-16: `C:/dev/Zenith2-drafts/findings/OPEN_QUESTIONS_FOR_HUMAN.md`) | a stamp record replaces the per-start SHA-256 re-hash of delivered packs, committed patches and Windows DLC: the stamps of every verified file are recorded (the delivery state file, `verified.zinv`), signatures are checked at every start, and a file is re-hashed when a stamp changes (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache"); [Z2-N-ARCH-955] and [Z2-N-ARCH-956] carry "verified once, re-hashed when its stamps change" and [Z2-N-ARCH-966] places the record in the trust boundary (amended in `Zenith2/Docs/Architecture.md`, patch 2026-10-16g, row X366; premise restated by row X376); the Stage 1 security-architecture ticket confirms the three items of "Verified cache" item 6 |

### Selection decisions PACK owns

None. bundletool and Google's Play Asset Delivery library are not selections: Z2-D-027 names bundletool and Play Asset Delivery, and Play's library is the only client of Play's asset-pack service.

### Design rulings PACK records

These rulings are appended to `Zenith2/Docs/DecisionLog.md` with consecutive free `Z2-D-NNN` IDs in the diff of PACK's first Stage 6 implementing ticket (Z2-D-097), before any implementation relies on them:

| # | Ruling | Why |
|---|---|---|
| R1 | A content patch never changes the executable; the patch builder refuses a different build ID, and code changes ship as full releases | the patch mount binds a patch to one build ([Z2-N-ARCH-955]); a patch replacing the running executable would need a second process |
| R2 | Android builds take content updates only through Play, in every configuration | Z2-D-027 alternative 2; Play's policy on code from other sources (bytecode); no Android patch producer and no base identity readable at step 4 |
| R3 | Pending installs run as the closing action of startup step 4, before the virtual filesystem, writing under `<install>/Patches/`, `<install>/DLC/` and `<install>/.zenith2-install/` alone: the one exception to [Z2-N-ARCH-921], named in [Z2-N-ARCH-921] and in [Z2-N-ARCH-923] | no running game can observe a half-installed update |
| R4 | Patches are cumulative against the line's base package and entry-granular, with no intra-file delta | the patch mount mounts one version; cooked files are content-addressed |
| R5 | One base catalogue lists every non-DLC asset; DLC units carry catalogue fragments | undelivered assets are known and fail `NOT_INSTALLED`; DLC unknown until entitled |
| R6 | A hard dependency of a deliverable unit's asset from outside that unit is promoted to base | a dependency must be present whenever its dependent is; base and install-time units are always present |
| R7 | App Bundles are built by a second generated Gradle project per game | the sideload APK and the bundle need different asset sets from one build type |
| R8 | The Play Asset Delivery Java library is linked into bundle builds only | sideload APKs keep SHIP's no-runtime-dependency rule |
| R9 | A delivered pack, a committed patch and a Windows DLC are hashed in full once; later starts check every update manifest's signature and targets and every file's identity stamp (size, last-write and change time, file identity), and re-hash a file only when its stamp, its record or its manifest digest differs (Q90 ruled (a)); `package.reverifyOnStartup` forces the full hash | Q90 (a): the records are unauthenticated user state trusted as much as the files they cover; whoever can write the install root or the app-private storage can forge a record, and can already replace the executable ([Z2-N-ARCH-966], restated by row X376); the signature is checked at every start but does not bind a file's bytes once its record is trusted; the cold-boot cost is Z2-B-PACK-011, measured at the first and the second start |
| R10 | Installation seals and renames verified bundles in place and never copies | an interruption can only leave whole, durable directories |
| R11 | A DLC never adds schemas or script modules | the type registry freezes at step 10, before DLC mounts at the close of step 11 |
| R12 | Release and Shipping packages use a separate shipping-profile cooked tree | shipping compression and dictionaries without slowing development cooks |
| R13 | `package.zpkm` is written per installation layout, records no digest for the base unit's archives, and is never shipped in a patch | one file cannot describe its own archive, three layouts, and a patched install at once |
| R14 | Installs, recovery and sweeps need two locks: one installer at a time (`lock`) and no other running instance (`running`, shared by every instance for its lifetime); an instance that cannot take `lock` waits without bound for `running` shared and never starts without it (PACK-R2) | an install under a running instance would trash the content it has mounted, and an instance started mid-install would mount a tree being renamed |
| R15 | Dictionaries are frozen per release line: a patch build re-encodes with the base release's `.zdict` | retraining would change every re-encoded file and put the whole class in every patch |
| R16 | Release and patch versions rise monotonically within a release line, independent of the build they target; SHIP's release pipeline refuses a version not greater than every earlier release and patch of the line (row PK25) | the install state's and the record's patch floors count across builds; a renumbered build would be refused as a replay and classified a downgrade (PACK-R2) |
| R17 | A world learns the package state only from recorded publications: a snapshot at the first prologue step 1b after its creation, then the events | a world created at run time would otherwise read stale state, and a replay would diverge (PACK_round2 finding 4) |
| R18 | A Shipping package is refused while the cook report of its shipping-profile tree lists a stale or missing bake; Debug and Release packages warn and continue | stale lighting or generator output must not ship, while development packages stay buildable mid-bake (WORLD's row WD14 (b), answered by PK26) |

## Requirement index

Every `Z2-R-PACK-NNN` of `Zenith2/Docs/Requirements/PACK.md` and `Zenith2/Docs/Requirements/PACK/Stage6.md` with the design headings that specify it.

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-PACK-001 | ZenithPackage library target | S2 | Zenith2/Docs/Design/PACK.md → "Target summary"; Zenith2/Docs/Design/PACK.md → "Libraries PACK depends on"; Zenith2/Docs/Design/PACK.md → "ZENITH_SUPPORTED matrix" |
| Z2-R-PACK-002 | Public header rules and contract blocks | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Rules across the PACK API"; Zenith2/Docs/Design/PACK.md → "ZenithPackage public root headers" |
| Z2-R-PACK-003 | Package error enumeration | S2 | Zenith2/Docs/Design/PACK.md → "Error enumeration"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_PackageTypes header" |
| Z2-R-PACK-004 | Error mapping at library boundaries | S6 | Zenith2/Docs/Design/PACK.md → "Error mapping at boundaries" |
| Z2-R-PACK-005 | Delivery update lock row | S6 | Zenith2/Docs/Design/PACK.md → "Lock-order entries"; Zenith2/Docs/Design/PACK/Algorithms.md → "Delivery update slots" |
| Z2-R-PACK-006 | No thread owned by PACK | S6 | Zenith2/Docs/Design/PACK.md → "Threads and background operations"; Zenith2/Docs/Design/PACK.md → "Thread affinity per type" |
| Z2-R-PACK-007 | Factories unwind on failure | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Fault point table"; Zenith2/Docs/Design/PACK.md → "Allocation failure" |
| Z2-R-PACK-008 | Runtime bridge in ZenithRuntime | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Runtime bridge declarations"; Zenith2/Docs/Design/PACK.md → "Startup and shutdown"; Zenith2/Docs/Design/PACK.md → "Frame-level flow" |
| Z2-R-PACK-009 | Shutdown completes PACK operations | S6 | Zenith2/Docs/Design/PACK.md → "Startup and shutdown"; Zenith2/Docs/Design/PACK.md → "Cancellation"; Zenith2/Docs/Design/PACK/Algorithms.md → "Shutdown drain" |
| Z2-R-PACK-010 | Configuration keys | S6 | Zenith2/Docs/Design/PACK.md → "Configuration keys" |
| Z2-R-PACK-011 | Binary format registry file | S2 | Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"; Zenith2/Docs/Design/PACK/Formats.md → "Format registry file" |
| Z2-R-PACK-012 | Format registry validation | S2 | Zenith2/Docs/Design/PACK/Formats.md → "Format registry file"; Zenith2/Docs/Design/PACK/Tests.md → "Format registry tests" |
| Z2-R-PACK-013 | Compatibility classes | S2 | Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules" |
| Z2-R-PACK-014 | Common rules of PACK binary formats | S2 | Zenith2/Docs/Design/PACK/Formats.md → "Common rules for PACK formats"; Zenith2/Docs/Design/PACK/Formats.md → "Name rules" |
| Z2-R-PACK-015 | Package manifest reader refuses other versions | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Common rules for PACK formats"; Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry" |
| Z2-R-PACK-016 | Package manifest parser limits | S4 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_PackageTypes header"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest checks" |
| Z2-R-PACK-017 | PACK fuzz targets | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Test targets and gate steps"; Zenith2/Docs/Design/PACK.md → "Missing and corrupt input" |
| Z2-R-PACK-018 | Packer writes ASSET's archive layout | S4 | Zenith2/Docs/Design/PACK/Tooling.md → "Archive writer"; Zenith2/Docs/Design/PACK/Tooling.md → "Archive read-back"; Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes" |
| Z2-R-PACK-019 | Archive entry policy | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes" |
| Z2-R-PACK-020 | Deterministic archives | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "Archive writer" |
| Z2-R-PACK-021 | Archive limits enforced at pack time | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes" |
| Z2-R-PACK-022 | Package modes in ZenithCook | S4 | Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes"; Zenith2/Docs/Design/PACK/Tooling.md → "Tool-side file layout" |
| Z2-R-PACK-023 | Atomic package publication | S4 | Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps" |
| Z2-R-PACK-024 | Recipe compression policy | S2 | Zenith2/Docs/Design/PACK/Formats.md → "Packaging recipe"; Zenith2/Docs/Design/PACK/Tooling.md → "Shipping cook profile" |
| Z2-R-PACK-025 | Shipping cook profile | S2 | Zenith2/Docs/Design/PACK/Tooling.md → "Shipping cook profile" |
| Z2-R-PACK-026 | Dictionary training step | S2 | Zenith2/Docs/Design/PACK/Algorithms.md → "Dictionary policy" |
| Z2-R-PACK-027 | Deterministic dictionary identity | S2 | Zenith2/Docs/Design/PACK/Algorithms.md → "Dictionary policy" |
| Z2-R-PACK-028 | Re-encoded sections decode | S2 | Zenith2/Docs/Design/PACK/Algorithms.md → "Dictionary policy" |
| Z2-R-PACK-029 | Archive entries stay byte-addressable | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Archives PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "Archive writer" |
| Z2-R-PACK-030 | Compression in the size report | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report" |
| Z2-R-PACK-031 | Recipe units | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Packaging recipe" |
| Z2-R-PACK-032 | Rule assignment | S4 | Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm" |
| Z2-R-PACK-033 | Hard dependencies promoted to base | S4 | Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm" |
| Z2-R-PACK-034 | DLC dependency integrity | S4 | Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm" |
| Z2-R-PACK-035 | Global files in base | S4 | Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm" |
| Z2-R-PACK-036 | Base catalogue and DLC fragments | S4 | Zenith2/Docs/Design/PACK/Algorithms.md → "Partition algorithm"; Zenith2/Docs/Design/PACK/Formats.md → "Patch and DLC bundles" |
| Z2-R-PACK-037 | Chunk target and archive names | S4 | Zenith2/Docs/Design/PACK/Algorithms.md → "Chunking"; Zenith2/Docs/Design/PACK/Formats.md → "Name rules" |
| Z2-R-PACK-038 | Partition report | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report" |
| Z2-R-PACK-039 | Recipe authoring document | S2 | Zenith2/Docs/Design/PACK/Formats.md → "Packaging recipe"; Zenith2/Docs/Design/PACK.md → "What PACK authors"; Zenith2/Docs/Design/PACK.md → "Migration hooks" |
| Z2-R-PACK-040 | Package manifest content | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Package manifest"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest header"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest tables" |
| Z2-R-PACK-041 | Package manifest checks | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Package manifest checks"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_PackageManifest header" |
| Z2-R-PACK-042 | Package manifest identity | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Package manifest checks"; Zenith2/Docs/Design/PACK/Algorithms.md → "Package service startup" |
| Z2-R-PACK-043 | Shipped mount manifests | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Mount manifests PACK writes"; Zenith2/Docs/Design/PACK/Tooling.md → "Manifest writers" |
| Z2-R-PACK-044 | Windows package layout | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Windows install layout"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps" |
| Z2-R-PACK-045 | Windows install tree | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Windows install layout" |
| Z2-R-PACK-046 | Android sideload layout | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Android install layout"; Zenith2/Docs/Design/PACK/Algorithms.md → "Sideload builds" |
| Z2-R-PACK-047 | Android bundle layout | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Android App Bundle layout"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps" |
| Z2-R-PACK-048 | Package CMake targets | S4 | Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets" |
| Z2-R-PACK-049 | Package build identity | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes" |
| Z2-R-PACK-050 | Size report | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report"; Zenith2/Docs/Design/PACK.md → "Answers to consumers of PACK rows" |
| Z2-R-PACK-051 | Windows install size measured | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Stage 6 acceptance items"; Zenith2/Docs/Design/PACK.md → "Rows PACK owns" |
| Z2-R-PACK-052 | Bundle Gradle project | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "App Bundle build" |
| Z2-R-PACK-053 | Asset pack module template | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "App Bundle build" |
| Z2-R-PACK-054 | Unsigned bundle output | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets" |
| Z2-R-PACK-055 | Bundle base module limit | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Bundle size measurement"; Zenith2/Docs/Design/PACK.md → "Rows PACK owns" |
| Z2-R-PACK-056 | Play base-module limit pinned | S6 | Zenith2/Docs/Design/PACK.md → "Rows PACK owns"; Zenith2/Docs/Design/PACK.md → "Pending new rows" |
| Z2-R-PACK-057 | Play library in bundle builds only | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "App Bundle build"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records" |
| Z2-R-PACK-058 | Every bundle split 16 KB aligned | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"; Zenith2/Docs/Design/PACK/Tests.md → "Stage 6 acceptance items" |
| Z2-R-PACK-059 | Delivery interface | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_AssetPackDelivery header" |
| Z2-R-PACK-060 | Play delivery over JNI | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Java bridge"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_AssetPackDelivery header" |
| Z2-R-PACK-061 | Java bridge contract | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Java bridge" |
| Z2-R-PACK-062 | Play status and error mapping | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Play status and error mapping" |
| Z2-R-PACK-063 | Delivery update slots | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Delivery update slots" |
| Z2-R-PACK-064 | Simulated delivery | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Simulated delivery"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_AssetPackDelivery header" |
| Z2-R-PACK-065 | Asset pack state machine | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack state machine"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack events"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-066 | Happy-path delivery | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting" |
| Z2-R-PACK-067 | Install-time and base units need no delivery | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack state machine"; Zenith2/Docs/Design/PACK/Formats.md → "Mount manifests PACK writes" |
| Z2-R-PACK-068 | Verification before mount | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants" |
| Z2-R-PACK-069 | Verification pipeline bounds | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline"; Zenith2/Docs/Design/PACK.md → "In-memory layouts that matter" |
| Z2-R-PACK-070 | Verification throughput floor | S6 | Zenith2/Docs/Design/PACK.md → "Pending new rows"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline" |
| Z2-R-PACK-071 | Verified cache for delivered packs | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state records" |
| Z2-R-PACK-072 | Mounting a verified pack | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting" |
| Z2-R-PACK-073 | Removal after release | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants" |
| Z2-R-PACK-074 | Delivery state file | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Delivery state file"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state header"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state records" |
| Z2-R-PACK-075 | Fetch policy | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Fetch policy" |
| Z2-R-PACK-076 | Initial query | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Initial query" |
| Z2-R-PACK-077 | User confirmation | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-078 | Insufficient storage | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Play status and error mapping"; Zenith2/Docs/Design/PACK.md → "Low disk" |
| Z2-R-PACK-079 | Entitlement-gated packs | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header" |
| Z2-R-PACK-080 | Entitlement query interface | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header"; Zenith2/Docs/Design/PACK.md → "Upward interfaces PACK declares" |
| Z2-R-PACK-081 | Stale updates dropped | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK.md → "Counters" |
| Z2-R-PACK-082 | Sideload builds have no delivery | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Sideload builds" |
| Z2-R-PACK-083 | Pack events reach simulation at tick boundaries | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Event publication and replay"; Zenith2/Docs/Design/PACK/Formats.md → "Package event payload" |
| Z2-R-PACK-084 | Replay reproduces pack events | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Event publication and replay" |
| Z2-R-PACK-085 | Script-facing package API | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Script-facing API" |
| Z2-R-PACK-086 | Package misuse codes | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/PACK.md → "Luau errors versus asserts" |
| Z2-R-PACK-087 | Event queue bound | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Event publication and replay"; Zenith2/Docs/Design/PACK.md → "Counters" |
| Z2-R-PACK-088 | bundletool pinned | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"; Zenith2/Docs/Design/PACK.md → "Assumed interfaces pending other areas" |
| Z2-R-PACK-089 | Test-time mini bundle | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "PowerShell functions"; Zenith2/Docs/Design/PACK/Tests.md → "Fixtures" |
| Z2-R-PACK-090 | Split generation for the S24 | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"; Zenith2/Docs/Design/PACK/Formats.md → "Bundle configuration and device specs" |
| Z2-R-PACK-091 | Local-testing APK sets | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Local testing and the emulator smoke" |
| Z2-R-PACK-092 | Bundle size measurement | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Bundle size measurement" |
| Z2-R-PACK-093 | Emulator smoke | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Local testing and the emulator smoke"; Zenith2/Docs/Design/PACK/Tests.md → "Device runs" |
| Z2-R-PACK-094 | S24 local-testing run | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Device runs" |
| Z2-R-PACK-095 | Install record | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Install record"; Zenith2/Docs/Design/PACK/Formats.md → "Identity encoding"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallRecord header" |
| Z2-R-PACK-096 | Update classification | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update classification" |
| Z2-R-PACK-097 | Migration journaled in the record | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Installed-update migration" |
| Z2-R-PACK-098 | Update migration registry | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_UpdateMigration header"; Zenith2/Docs/Design/PACK/Algorithms.md → "Installed-update migration" |
| Z2-R-PACK-099 | Built-in migration steps | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Built-in migration steps" |
| Z2-R-PACK-100 | Package.Updated event | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Installed-update migration"; Zenith2/Docs/Design/PACK/Algorithms.md → "World snapshot"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Script-facing API" |
| Z2-R-PACK-101 | Packs re-verified after an update | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Built-in migration steps"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache" |
| Z2-R-PACK-102 | Play installed-update device run | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Device runs" |
| Z2-R-PACK-103 | Update manifest format | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Update manifest"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest header"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest tables and signature" |
| Z2-R-PACK-104 | Update manifest checks | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks" |
| Z2-R-PACK-105 | Signing hand-off | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Ed25519 signer" |
| Z2-R-PACK-106 | Ed25519 signer | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Ed25519 signer" |
| Z2-R-PACK-107 | Patch builder | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "Patch builder" |
| Z2-R-PACK-108 | Entry-granular patch diff | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff" |
| Z2-R-PACK-109 | Cumulative patches against the base | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff" |
| Z2-R-PACK-110 | Patch refusals at build time | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes" |
| Z2-R-PACK-111 | Patch directory name equals its version | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks" |
| Z2-R-PACK-112 | Effective content hash after a patch | S6 | Zenith2/Docs/Design/PACK.md → "Assumed interfaces pending other areas"; Zenith2/Docs/Design/PACK/Algorithms.md → "Package service startup" |
| Z2-R-PACK-113 | Patch size reported | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"; Zenith2/Docs/Design/PACK.md → "Pending new rows" |
| Z2-R-PACK-114 | DLC bundles | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Patch and DLC bundles"; Zenith2/Docs/Design/PACK/Tooling.md → "DLC builder" |
| Z2-R-PACK-115 | DLC builder | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "DLC builder" |
| Z2-R-PACK-116 | Windows DLC mounting | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Windows DLC mounting" |
| Z2-R-PACK-117 | DLC fragment must resolve | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Windows DLC mounting" |
| Z2-R-PACK-118 | DLC adds no schemas or modules | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records" |
| Z2-R-PACK-119 | DLC revisions and removal | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation" |
| Z2-R-PACK-120 | Compatibility matrix | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules" |
| Z2-R-PACK-121 | Engine cook version constant within a line | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks" |
| Z2-R-PACK-122 | Content patches keep the executable | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records" |
| Z2-R-PACK-123 | No downgrade through patches | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update classification" |
| Z2-R-PACK-124 | Release line change obsoletes patches | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Obsolete directories" |
| Z2-R-PACK-125 | Install file system interface | S6 | Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallFileSystem header" |
| Z2-R-PACK-126 | Host install file system durability | S6 | Zenith2/Docs/Design/PACK.md → "ZenithPackage platform files"; Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model" |
| Z2-R-PACK-127 | Memory install file system durability model | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallFileSystem header" |
| Z2-R-PACK-128 | Install journal format | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Install journal"; Zenith2/Docs/Design/PACK/Formats.md → "Install journal header"; Zenith2/Docs/Design/PACK/Formats.md → "Install journal records" |
| Z2-R-PACK-129 | Durable step rules | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Durable step rules" |
| Z2-R-PACK-130 | Installation renames and never copies | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Install state machine"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records" |
| Z2-R-PACK-131 | Install phases | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Install phases and events"; Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation" |
| Z2-R-PACK-132 | Recovery table | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table" |
| Z2-R-PACK-133 | Power loss at every step converges | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants" |
| Z2-R-PACK-134 | Process kill at every step converges | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table" |
| Z2-R-PACK-135 | Recovery is idempotent | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants" |
| Z2-R-PACK-136 | Pending installs at startup | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup" |
| Z2-R-PACK-137 | Partial deliveries ignored | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK/Tooling.md → "Windows install conventions" |
| Z2-R-PACK-138 | Refused bundles removed and reported | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation" |
| Z2-R-PACK-139 | Obsolete patches removed | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Obsolete directories" |
| Z2-R-PACK-140 | Install lock and instance guard | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Install lock and instance guard"; Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup" |
| Z2-R-PACK-141 | Low disk and read-only root during install | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK.md → "Low disk" |
| Z2-R-PACK-142 | Installs before the virtual filesystem | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records" |
| Z2-R-PACK-143 | Android takes updates through Play | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Android content updates"; Zenith2/Docs/Design/PACK/Tooling.md → "Patch builder" |
| Z2-R-PACK-144 | Interrupted patch never mounted partly | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants" |
| Z2-R-PACK-145 | Windows patch and DLC device run | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Device runs" |
| Z2-R-PACK-146 | Download interrupted by network loss resumes | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Network retry"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-147 | Download interrupted by process death resumes | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Fetch policy"; Zenith2/Docs/Design/PACK/Algorithms.md → "Simulated delivery" |
| Z2-R-PACK-148 | Cancelled download rolled back | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-149 | Corrupt delivery rolled back | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline" |
| Z2-R-PACK-150 | Completion wins over a racing cancel | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-151 | Game consistent during an interrupted download | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"; Zenith2/Docs/Design/PACK/Tests.md → "Integration suite" |
| Z2-R-PACK-152 | Undelivered assets fail NOT_INSTALLED | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"; Zenith2/Docs/Design/PACK.md → "Missing and corrupt input" |
| Z2-R-PACK-153 | Fetch and cancel stress | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Asset pack installer tests"; Zenith2/Docs/Design/PACK.md → "Cancellation" |
| Z2-R-PACK-154 | S24 interrupted download resumed | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Device runs" |
| Z2-R-PACK-155 | S24 interrupted download rolled back | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Device runs" |
| Z2-R-PACK-156 | Package counters | S6 | Zenith2/Docs/Design/PACK.md → "Counters" |
| Z2-R-PACK-157 | Package log messages | S6 | Zenith2/Docs/Design/PACK.md → "Log category and messages" |
| Z2-R-PACK-158 | Profiler rows and frame dump | S6 | Zenith2/Docs/Design/PACK.md → "On-screen profiler rows"; Zenith2/Docs/Design/PACK.md → "Frame-dump entry" |
| Z2-R-PACK-159 | Package telemetry | S6 | Zenith2/Docs/Design/PACK.md → "Telemetry" |
| Z2-R-PACK-160 | Size report answers D-SEL-CRASH | S4 | Zenith2/Docs/Design/PACK.md → "Answers to consumers of PACK rows"; Zenith2/Docs/Design/PACK/Formats.md → "Size report and partition report" |
| Z2-R-PACK-161 | Play pack rows enforced | S6 | Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"; Zenith2/Docs/Design/PACK.md → "Pending new rows" |
| Z2-R-PACK-162 | Package service memory | S6 | Zenith2/Docs/Design/PACK.md → "In-memory layouts that matter"; Zenith2/Docs/Design/PACK.md → "Pending new rows" |
| Z2-R-PACK-163 | Stage 1 per-system budget ticket for PACK | S1 | Zenith2/Docs/Design/PACK.md → "Rows PACK owns"; Zenith2/Docs/Design/PACK.md → "Stage 1" |
| Z2-R-PACK-164 | Pending installs within the cold-boot budget | S6 | Zenith2/Docs/Design/PACK.md → "Rows PACK is measured against"; Zenith2/Docs/Design/PACK/Algorithms.md → "Complexity" |
| Z2-R-PACK-165 | Bundles sealed before commit | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Durable step rules"; Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation" |
| Z2-R-PACK-166 | Links never followed | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallFileSystem header" |
| Z2-R-PACK-167 | Bundle holds exactly its listed files | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"; Zenith2/Docs/Design/PACK/Formats.md → "Patch and DLC bundles" |
| Z2-R-PACK-168 | Install state file | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Install state file"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks" |
| Z2-R-PACK-169 | Patch bound to its base package | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules" |
| Z2-R-PACK-170 | Package manifest per layout | S4 | Zenith2/Docs/Design/PACK/Formats.md → "Package manifest"; Zenith2/Docs/Design/PACK/Formats.md → "Package manifest tables"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps" |
| Z2-R-PACK-171 | Package manifest never patched | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules" |
| Z2-R-PACK-172 | Unmount and removal results never lost | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Delivery update slots"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-173 | Mounted packs released before the installer is destroyed | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"; Zenith2/Docs/Design/PACK/Algorithms.md → "Shutdown drain"; Zenith2/Docs/Design/PACK.md → "Startup and shutdown" |
| Z2-R-PACK-174 | Cold-boot update verification time | S6 | Zenith2/Docs/Design/PACK.md → "Pending new rows"; Zenith2/Docs/Design/PACK.md → "Rows PACK is measured against" |
| Z2-R-PACK-175 | Unsupported install volumes refused | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model" |
| Z2-R-PACK-176 | Test manifests signed in process | S6 | Zenith2/Docs/Design/PACK/Tests.md → "Fixtures"; Zenith2/Docs/Design/PACK/Tooling.md → "Ed25519 signer" |
| Z2-R-PACK-177 | Install and delivery readers refuse other versions | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Common rules for PACK formats"; Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry" |
| Z2-R-PACK-178 | Install and delivery format limits | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Install journal header"; Zenith2/Docs/Design/PACK/Formats.md → "Install state file"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state header" |
| Z2-R-PACK-179 | Release block per mount operation | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"; Zenith2/Docs/Design/PACK.md → "Allocation failure" |
| Z2-R-PACK-180 | Shutdown pump with a bounded drain | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Shutdown drain"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions" |
| Z2-R-PACK-181 | Package state resource and world snapshot | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "World snapshot"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Runtime bridge declarations" |
| Z2-R-PACK-182 | Forwarding entitlement query | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Entitlement provider"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header" |
| Z2-R-PACK-183 | Replay mounts the recorded pack set | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Replay pack set" |
| Z2-R-PACK-184 | Patch numbering monotonic per release line | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Update classification"; Zenith2/Docs/Design/PACK/Formats.md → "Install state file"; Zenith2/Docs/Design/PACK/Tooling.md → "Patch builder" |
| Z2-R-PACK-185 | Archives stored in every APK split | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Bundle configuration and device specs"; Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage" |
| Z2-R-PACK-186 | Shipping packages refuse stale bakes | S4 | Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes" |
| Z2-R-PACK-187 | Verified stamp file format | S6 | Zenith2/Docs/Design/PACK/Formats.md → "Verified stamp file"; Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_UpdateApplier header" |
| Z2-R-PACK-188 | The applier records the stamps it verified | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation"; Zenith2/Docs/Design/PACK/Algorithms.md → "Durable step rules" |
| Z2-R-PACK-189 | Patch and DLC mounts skip unchanged files | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Algorithms.md → "Windows DLC mounting"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Runtime bridge declarations" |
| Z2-R-PACK-190 | No unverified file is counted as verified | S6 | Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table" |
