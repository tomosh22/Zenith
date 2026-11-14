# ONLINE — online services

## Purpose and scope

### What ONLINE owns

ONLINE owns the online-services adapter of Zenith2 (Z2-D-074): the `Zenith_OnlineServices` adapter API that every backend implements, the offline implementation required in Stage 4, and the Steam and Google Play backends of Stage 6, each split into a store-independent core library that the gates build and test and a thin SDK-facing backend. Above the adapter it owns the engine-side logic every backend shares, `Zenith_OnlineSession`:

- **identity**: who is signed in, detecting sign-in, sign-out and account switches mid-session (including an identity a backend cannot vouch for after a resume), and the **rebind sequence** that moves SAVE's profile, the entitlements and the cloud state to the new account in the same frame prologue, with two generation boundaries (the fence and the bind) and no cross-account leakage;
- **entitlements**: per-account ownership answers for scripts and, from Stage 6, the implementation of PACK's `Zenith_EntitlementQuery` that gates DLC mounting;
- **achievements**: standard and incremental achievements, unlocked from Luau gameplay;
- **cloud saves**: the cloud-save conflict model on top of SAVE's slots (a three-way comparison of content hashes over content-addressed cloud objects, an index view that treats unreadable evidence as unknown, and tombstones from SAVE's deletion listener), the upload, download and deletion sequences and their recovery, garbage collection, the explicit index repair, automatic resolution policies, and the conflict resolution flows, with a reference conflict screen in the flagship slices;
- the `Online` Luau namespace, its five event types and its misuse codes ZS2900–ZS2999;
- the online catalogue authoring document (achievements, entitlements, cloud policy), its cook step and its cooked kind;
- the diagnostics, development console commands, overlay panel and editor panel of online services.

Online results never affect simulation directly: they reach a world only as recorded asynchronous publications of `ASYNC_SOURCE_ONLINE` at tick boundaries (Z2-D-053), and an account change reaches a world through PLAT's recorded `Platform.AccountChanged` (Z2-D-019).

### Explicit exclusions

| Not ONLINE's | Owner | Basis |
|---|---|---|
| local saves: the ZSAV format, slots, metadata, profiles and their directories, the journaled write, checkpoints and their leases, the restore pipeline, the resume state | SAVE | `Zenith2/Docs/Design/SAVE.md` → "Purpose and scope"; Z2-D-074 |
| OS account signals, the account-signal queue, `PLATFORM_EVENT_ACCOUNT_CHANGED` and its delivery to worlds | PLAT | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account changes" |
| DLC packaging, mounting and unmounting, Play Asset Delivery | PACK | `Zenith2/Docs/Design/PACK.md` → "Upward interfaces PACK declares" |
| UI layout, documents, focus, narration, localization of the conflict screen | UI | `Zenith2/Docs/Design/UI.md` → "Purpose and scope" |
| achievement triggers in gameplay (what earns an achievement) | game-side Luau, GAME's samples | Z2-D-077; `Zenith2/Docs/Design/GAME.md` → "Purpose and scope" |
| trust boundaries, update authentication, security evidence | SEC (not yet drafted; § "Security assumptions" lists what ONLINE relies on) | `Zenith2/Docs/Architecture.md` → "Security architecture" |
| networked multiplayer, sessions, matchmaking, leaderboards, presence, friends, voice | NET (Stage 7) or no inventory item | Z2-D-073; Z2-D-074 names identity, entitlements, achievements and cloud saves only |
| in-game purchase flows (buying a product from the game), consumables, virtual currencies | no inventory item; purchases made in a store appear as entitlements | Z2-D-074 |
| console platform services (PSN, Xbox Live, Nintendo) | SCOPE, Stage 7, gated on SDK access | Z2-D-071 |
| server-side purchase verification, session tickets, OAuth tokens | no game server is in scope | Z2-D-073 |
| dynamic loading of backend plugins | excluded | Z2-D-024 |

### Inventory items covered

| Item | Name | Where |
|---|---|---|
| Z2-I-ONLINE-01 | the `Zenith_OnlineServices` adapter API | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineServices header"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend contract" |
| Z2-I-ONLINE-02 | an offline implementation | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline backend" |
| Z2-I-ONLINE-03 | identity | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity model", "Sign-in" |
| Z2-I-ONLINE-04 | entitlements | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlements" |
| Z2-I-ONLINE-05 | achievements | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievements" |
| Z2-I-ONLINE-06 | cloud-save conflict model | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud-save model overview", "Three-way decision table" |
| Z2-I-ONLINE-07 | account change mid-session: sign-out | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind sequence" |
| Z2-I-ONLINE-08 | account change mid-session: switch user | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account change detection", "Rebind sequence" |
| Z2-I-ONLINE-09 | account change: saves rebind | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps", "Profile switch retry", "Worlds bound under the previous profile" |
| Z2-I-ONLINE-10 | account change: entitlements rebind | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind state per subsystem", "Entitlement query for packages" |
| Z2-I-ONLINE-11 | account change: no cross-account leakage | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation", "Generation fencing" |
| Z2-I-ONLINE-12 | cloud-save conflict resolution flows | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Conflict resolution flows" |
| Z2-I-ONLINE-13 | Steam backend | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam backend" |
| Z2-I-ONLINE-14 | Google Play backend | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Google Play backend" |

### Items of other areas with an ONLINE half

| Item | Owner | ONLINE's half |
|---|---|---|
| Z2-I-PLAT-30 account changes | PLAT (signals, delivery) | the online backends' signals and the account consumer that rebinds saves and entitlements before gameplay reacts (`Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery" step 3 names this document) |
| PACK's entitlement-gated packs (Z2-R-PACK-079, Z2-R-PACK-080) | PACK | the implementation of `Zenith_EntitlementQuery` (Stage 6) |
| SAVE's profiles per account (Z2-R-SAVE-097) | SAVE | the account-to-profile mapping and the calls to `SetActiveProfile` |

### Section files

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Design/ONLINE/PublicAPI.md` | Scope of this file · Rules across the ONLINE API · Header list · Zenith_OnlineTypes header · Zenith_OnlineErrors header · Zenith_OnlineCatalogue header · Zenith_OnlineServices header · Zenith_OnlineSession header · Zenith_Module_Online header · Zenith_Online_Offline header · Zenith_OnlineOfflineTestAccess header · Zenith_Online_Steam header · Zenith_Online_GooglePlay header · Zenith_OnlineSteamCore header · Zenith_OnlineGooglePlayCore header · Internal declarations · Script-facing API · Event types · Configuration keys · Misuse codes |
| `Zenith2/Docs/Design/ONLINE/Accounts.md` | Scope of this file · Identity model · Sign-in · Bind at start · Account change detection · Identity verification · Rebind sequence · Cross-account isolation · Generation fencing · Entitlements · Achievements · Script request processing · Notifications · Replay mode · OS session signals |
| `Zenith2/Docs/Design/ONLINE/CloudSave.md` | Scope of this file · Cloud-save model overview · Slot eligibility · Hashes and identities · Index view · Three-way decision table · Reconcile algorithm · Upload sequence · Download sequence · Deletion propagation · Garbage collection · Index repair · Conflict detection · Automatic resolution policies · Conflict resolution flows · Interrupted sync recovery · Transfer memory · Polling and triggers · Cloud status reporting · Shutdown and background flush |
| `Zenith2/Docs/Design/ONLINE/Formats.md` | Scope of this file · Conventions for every ONLINE format · Online catalogue document · Cooked online catalogue · Script request payloads · Script result payloads · Notification payloads · Backend completion payloads · Cloud envelope · Cloud index · Sync manifest · Device record · Offline achievement file · Offline cloud store · Format limits |
| `Zenith2/Docs/Design/ONLINE/Backends.md` | Scope of this file · Backend contract · Offline backend · Steam backend · Google Play backend · Credentials and partner access · Backend selection |
| `Zenith2/Docs/Design/ONLINE/Tests.md` | Scope of this file · Test targets · Session unit tests · Rebind and isolation tests · Entitlement and achievement tests · Cloud model tests · Format and fuzz tests · Script surface tests · Diagnostics and tooling tests · Determinism and replay tests · Offline backend tests · Conformance suite · Steam backend tests · Google Play backend tests · Integration suites · Device and needs-human runs · Stress tests · Gate steps per test group |

### Conventions

"Session" is the one `Zenith_OnlineSession` of a process; "backend" a `Zenith_OnlineServices` implementation; "bind" the moment the session adopts an account and its save profile; "generation" the session's bind generation; "slot" a SAVE slot; "envelope" a slot's cloud object; "the index" the cloud's slot list. Times on the main thread are milliseconds of ZenithCore's monotonic clock; save times are SAVE's UTC seconds and playtime seconds. Hashes of content are SHA-256 (`Zenith_Sha256Digest`); hashes of names are `Zenith_Hash64`.

### Prior art

<!-- informative -->
The old engine has no online services: no store SDK, no achievements, no cloud saves; its saves are local files. The design therefore follows platform practice rather than Zenith code: Steamworks' and Play Games Services' own recommendations (manual callback dispatch on the game thread, idempotent achievement calls, Saved Games with manual conflict resolution, Play Billing acknowledgement) and the console-certification habit of returning to a title screen when the signed-in user changes, which the account flow of the flagship slices adopts. The three-way content-hash comparison is the merge-base rule of version-control systems applied to opaque blobs: it is the smallest model that tells "changed here" from "changed elsewhere" without trusting device clocks or per-device counters.
<!-- /informative -->

## Dependencies

### Libraries ONLINE depends on

<!-- assumption: pending ARCH (row ON12) -->
`ZenithOnline` (module, layer 7) `DEPENDS`: ZenithRuntime, ZenithPlatform, ZenithJobs, ZenithCore (Architecture's current row) and, requested through row ON12: ZenithSave (profiles, slot export and import, the slot listener), ZenithReflect (the binding ABI and type registration), ZenithECS (the `Online.Events` system), ZenithAssets (the catalogue asset), ZenithProfiling (zones, counters, the panel provider), ZenithScript (`Zenith_ScriptSideEffectConsumer`, the result-decoder types), and from Stage 6 ZenithPackage (`Zenith_EntitlementQuery`). Every one is below layer 7, so the graph stays a DAG.
<!-- /assumption -->

| Library | Kind | Layer | `DEPENDS` |
|---|---|---|---|
| ZenithOnline | module (API) | 7 | as above |
| ZenithOnline_Offline | backend | 8 | ZenithOnline, ZenithPlatform, ZenithCore (Architecture's row) |
| ZenithOnline_SteamCore | engine library (`KIND ENGINE`) | 8 | ZenithOnline, ZenithPlatform, ZenithCore (no third party) |
| ZenithOnline_GooglePlayCore | engine library (`KIND ENGINE`) | 8 | ZenithOnline, ZenithPlatform, ZenithCore (no third party) |
| ZenithOnline_Steam | backend | 9 | ZenithOnline_SteamCore, ZenithOnline, ZenithPlatform, ZenithCore, `steamworks-sdk` (third party, `Internal/Windows/` only) |
| ZenithOnline_GooglePlay | backend | 9 | ZenithOnline_GooglePlayCore, ZenithOnline, ZenithPlatform, ZenithCore |

<!-- assumption: pending ARCH (core-doc items X358, X363) -->
The core libraries and the backends' layer 9 are requested through core-doc item X358 (amending X322 (2)); every edge points to a strictly lower layer ([Z2-N-ARCH-072]). Three core rules this split relies on are not yet core text and are fenced here until core-doc item X363 applies: a backend's dependence on a core library of its own family above the API's layer ([Z2-N-ARCH-075]), the `KIND` the core libraries declare (`ENGINE`, the value BuildSystem's "Signature" admits today), and the declared-but-unavailable state of a library whose partner-only third-party entry is absent (`ZenithOnline_Steam` without `steamworks-sdk`), which `CompileCoverage`, `SourceLayout` and MOD's backend resolution must understand (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Store backend libraries and what the gates prove").
<!-- /assumption -->

Third-party code: the Steamworks SDK (prebuilt, Windows) and the Play Games Services v2 and Play Billing Maven artefacts (Android, Java side only); no third-party header reaches a public header ([Z2-N-CONTRACT-043]).

### Headings ONLINE relies on

| Need | Source |
|---|---|
| module entry points, contexts, the account consumer, frame listeners | `Zenith2/Docs/Architecture.md` → "Engine modules and game projects"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"; `Zenith2/Docs/Design/MOD.md` → "Zenith_Module.h", "Registration journal and undo", "Configuration-key registration" |
| backend resolution and the generated composition | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend resolution", "Module to target mapping", "Thunk rules", "Test APK device files" |
| the asynchronous result table, the source registry, decoders, the script consumer, tick-boundary publication, replay recording | `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"; `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptAsync header", "Zenith_ScriptBinding header"; `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table" |
| the runtime's multiplexer and seams | `Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime" |
| UI's publication path (the pattern ONLINE's notifications follow) | `Zenith2/Docs/Design/UI/Interaction.md` → "UI events to simulation" |
| account signals and delivery | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account signal sources", "Account event delivery"; `Zenith2/Docs/Architecture/PlatformLayer.md` → "Account changes" |
| user-mount I/O | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API", "Atomic writes", "Other user-mount operations", "Mount kinds and roots" |
| configuration keys | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Key grammar", "Registration API" |
| profiles, slots, listing, deletion, metadata, the last-profile record, the async section | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Profile switching", "Checkpoints", "Listing slots", "Slot policy"; `Zenith2/Docs/Design/SAVE/Formats.md` → "Metadata section", "File names and directories", "Async section"; `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveTypes header", "Zenith_SaveSystem header" |
| the entitlement query | `Zenith2/Docs/Design/PACK/PublicAPI.md` → "Zenith_EntitlementQuery header"; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Asset pack transitions" |
| systems and events | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header", "Zenith_SystemContext header" |
| document types and migration | `Zenith2/Docs/Design/SCENE/DocumentFramework.md`; `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" |
| cooked asset container and kinds | `Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file", "Kind table"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader header" |
| hashing, SHA-256, containers, the clock | `Zenith2/Docs/Design/CORE/Maths.md`; `Zenith2/Docs/Design/CORE/Containers.md`; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| the memory governor | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` |
| console and overlay panels | `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"; `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers" |
| editor panels | `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface", "Extension table" |
| the tool library and its tests | `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules", "Dependency growth rule"; `Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions", "Cook diagnostics" |
| third-party pins, licences | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Integration modes in detail", "Licence fields", "Dependency cache" |
| Android Gradle wiring | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring", "Gradle project generation" |
| UI documents, localization, accessible flows | `Zenith2/Docs/Design/UI/Documents.md` → "UI document schema"; `Zenith2/Docs/Design/UI/Localization.md` → "Message formatting"; `Zenith2/Docs/Design/UI/Accessibility.md` → "End-to-end accessible flows" |

### Areas ONLINE depends on

PLAT (signals, I/O, configuration), SAVE (profiles, slots), SCRIPT (async results, bindings, misuse block), MOD (module contexts, sink, composition), ECS (systems, events), ASSET (catalogue kind), SCENE (document framework), PACK (entitlement query, Stage 6), CORE, JOBS, DEVT (console, overlay), EDIT (editor panel, `ZenithAuthoring`), UI (the conflict screen), SHIP (pins, packaging), SEC (assumptions).

### Areas that consume ONLINE

PACK (entitlement answers, Stage 6), GAME and TWOD (the flagship slices' account and conflict flows and achievement calls), UI (the conflict screen's data come from ONLINE's events), MOD (the backend list and `online.backend`), EDIT (the panel), every game's scripts (`Online` namespace).

### Interfaces ONLINE declares and implements

- ONLINE declares `Zenith_OnlineServices` and `Zenith_OnlineEventSink` for its backends (which sit above it, layer 8), and `Zenith_OnlinePublicationPort` for its own module instance and tests.
- ONLINE implements interfaces lower libraries declare: `Zenith_ModuleInstance` and `Zenith_ModuleFrameListener` (ZenithRuntime), `Zenith_ScriptSideEffectConsumer` (ZenithReflect's binding ABI), `Zenith_MemoryConsumer` (ZenithCore), PLAT's `Zenith_PfnAccountConsumer`, SAVE's slot listener through the Internal `Zenith_OnlineSaveListener` (assumption pending SAVE, row ON1 (c); `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations") and, from Stage 6, PACK's `Zenith_EntitlementQuery`. Every callback therefore points downward (`Zenith2/Docs/Contracts.md` → "Layering DAG").

### Asynchronous source and misuse block

ONLINE uses the registered source kind `ASYNC_SOURCE_ONLINE` (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; ONLINE writes no number of its own), for script results and for ONLINE's own notifications (a mixed kind: ticket-read results and ticketless, C++-allocated notifications; core-doc item X337 adds it to [Z2-N-SCRIPT-343]), every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most `uONLINE_RESULT_MAX_BYTES` (256) bytes. SCRIPT records the kind as mixed, with its in-flight capacity Z2-B-SCRIPT-026's online value plus `uONLINE_NOTIFY_MAX_OUTSTANDING` (8) per world, in `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values" (row ON4 answered; core text X324, X337, X347). The misuse block ZS2900–ZS2999 is ONLINE's by the orchestrator's MISUSE-BLOCKS-2 ruling; core-doc item X320 adds its row to `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges".

### Security assumptions

SEC has no design document yet; ONLINE relies on these assumptions and states them for SEC's draft (row ON15):

| # | Assumption | ONLINE's mechanism |
|---|---|---|
| S1 | Responses from a backend's SDK or service are untrusted bytes | every completion payload, list and object is validated (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules" rule 8; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Conventions for every ONLINE format"); every parser is fuzzed |
| S2 | Cloud objects are user files under the player's account: untrusted and possibly tampered (another device, another tool) | envelope and index checks, payload hashes, the profile-key checks, SAVE's full verification and migration at import; nothing is executable; an index that fails a check, belongs to another profile or comes from a newer release can delete nothing (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view") |
| S3 | ONLINE holds no secret | no token, ticket, receipt or password is requested, stored, logged or passed to simulation; the Play purchase token stays inside Java; configuration keys hold only public identifiers |
| S4 | Personal data stays out of logs, replays, crash reports and saves | account identifiers and display names are confined by their contracts; the one account value PLAT records in replay logs is keyed with a per-install secret that never leaves the device record, so it cannot be inverted by enumeration (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"); logs, the overlay and crash annotations carry generations and session-local account ordinals, never a profile key; a display name never enters a replay log (exposure is turned off under `--record`); the save profile key, a deterministic identity the cloud needs, is stored only in the account's own save data and cloud objects |
| S5 | The offline implementation never contacts a network | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline network isolation": a symbol check of both archives against a network-API list and ZenithPlatform's loopback API, whose limits that section states |
| S6 | Entitlement answers on the client are not tamper-proof without a server | accepted for single-player scope: DLC content is gated by PACK's mounting, which a determined user of a modified client could bypass, as on every client-only store integration; no server verification exists (Z2-D-073) |
| S7 | Development seams are absent from Shipping behaviour | the scripted Steam and Play seams and the offline test access are refused in Shipping builds; the console commands exist only where the development console does |
| S8 | No account data crosses accounts | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation" |
| S9 | The install secret is a secret of the device | 32 bytes from the OS random source in `user:/saves/online/device.zodv`, excluded from Android device backup and transfer with its `WriteAtomic` temporary, as the two literal paths `saves/online/device.zodv` and `saves/online/device.zodv.z2atomic` (Android rule paths take no wildcard; rows ON18 (d), ON22 (b)), never synced, logged, published or written elsewhere (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record") |

### Assumed interfaces pending other areas

<!-- assumption: pending SAVE, MOD, ARCH, PACK, PLAT, SCRIPT, ASSET, SHIP, UI, DEVT, EDIT, GAME, TWOD -->
| Interface | Owner | ONLINE's use | Row |
|---|---|---|---|
| `Zenith_SaveSystem::BeginExportSlot(const Zenith_SaveSlotName&) → Zenith_Result<Zenith_OperationHandle, Zenith_SaveError>`, `GetExportedSlotBytes(const Zenith_OperationHandle&) → uint64`, `TakeExportedSlot(Zenith_OperationHandle&, Zenith_MutableByteSpan) → Zenith_Result<Zenith_SaveExportedSlot, Zenith_SaveError>` (the verified bytes of the current file, or of its backup when the current one fails, with the header's sequence, UTC seconds, playtime, kind and display name) | SAVE | uploads | ON1 (a) |
| `Zenith_SaveSystem::BeginImportSlot(const Zenith_SaveSlotName& xTarget, Zenith_ByteSpan xFile) → Zenith_Result<Zenith_OperationHandle, Zenith_SaveError>`: full verification as at a load; the `zenith.profile` pair equal to the active profile (`SAVE_ERROR_IDENTITY_MISMATCH`); newer or other-major versions refused; the METADATA slot name rewritten when the target differs; the journaled write with backup; the profile sequence raised to at least the file's | SAVE | downloads, KEEP_CLOUD, KEEP_BOTH | ON1 (b) |
| `Zenith_SaveSlotListener` (declared by SAVE: `OnSlotWritten(const Zenith_SaveSlotInfo&)`, `OnSlotDeleted(const Zenith_SaveSlotName&, Zenith_SaveKind)`, called on the main thread in `PumpMainThread`; `OnSlotDeleted` once for every slot `BeginDelete` removed and for nothing else), `Zenith_SaveSystem::SetSlotListener(Zenith_SaveSlotListener*)` | SAVE | upload triggers; tombstones | ON1 (c), ON16 (b) |
| `Zenith_SaveSystem::BeginProfileSwitch(const Zenith_SaveProfileKey&)` and `AbandonProfileSwitch()`: from the call until `SetActiveProfile` of that profile succeeds, every new request of a world not bound under it is refused `SAVE_ERROR_PROFILE_MISMATCH` while queued work drains | SAVE | rebind R4, the retry and the degraded late switch | ON16 (a) |
| `Zenith_SaveSystem::GetBindingProfile(uint16)`, `SAVE_ERROR_PROFILE_MISMATCH` (38) for every request of a world bound under another profile | SAVE | isolation I2 | ON1 (d) |
| `SetActiveProfile` of the active profile is an accepted no-op | SAVE | rebind step R4 | ON1 (e) |
| SAVE's log line of a profile switch and every SAVE diagnostic name the profile as `local` or `account`, never by its key | SAVE | isolation I8 | ON16 (c) |
| `Zenith_ModuleStartContext::GetSaveSystem() → Zenith_SaveSystem&`, `IsReplaying() → bool`, `GetDevConsole() → Zenith_DevConsole*` (null in Shipping), `GetProfilerOverlay() → Zenith_ProfilerOverlay*` (null when absent), `ProvideEntitlementQuery(Zenith_EntitlementQuery&)` (Stage 6) | MOD | start | ON5 |
| `saves/online/` reserved to ONLINE (no SAVE enumeration, recovery or cleanup touches it; never a profile key) and the two literal paths `saves/online/device.zodv` and `saves/online/device.zodv.z2atomic` excluded from Android backup and transfer (Android rule paths take no wildcard) | SAVE | every ONLINE file path (orchestrator ruling ONLINE-R10-PATHS) | ON22 |
| GAME's world host collects no carry-over roots when the old world's binding profile differs from the active profile; the flagship title flow waits for the first IDENTITY_CHANGED | GAME, TWOD | isolation I11; a late first bind | ON23 |
| `Zenith_ModuleStartContext::IsRecordingReplay() → bool` and `RequestExitAfterStart()`; the Online backend list passed in platform preference order (store backends first, `Offline` last); the device record `user:/saves/online/device.zodv` excluded from the Android backup and transfer rules (ON18 (d), amended by ON22 (b) for the new location); all four landed in MOD's design (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Start context members for ONLINE and PACK", `Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend resolution", "Backup and transfer rules"), and what stays pending is the second literal exclusion `saves/online/device.zodv.z2atomic` (ON22 (b) as amended post-round) | MOD | display-name exposure, the Steam relaunch, backend selection, the install secret | ON18 |
| `Zenith_Platform::FillOSRandom(Zenith_MutableByteSpan) → Zenith_Result<void, Zenith_PlatformError>` (ANY_THREAD, the existing `Zenith_OSRandom_*` sources); `Zenith_AccountSignal::m_ulAccountKey` documented as an opaque key chosen by the poster | PLAT | the device record; the keyed account key | ON19 |
| the single module publication sink `Zenith_WorldAttachContext::GetPublicationSink()` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink"; row ON5 (b) answered, core X344): `IsReplaying`, `AllocateCompletionID`, `Park` and `SetSourceHandler` for a kind declared through `DeclareScriptAsyncSource`; the prologue allocation of that script-declared mixed kind is landed (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Allocation sites and acceptance" step 1, row ON18 (f)); core text X344 pending | MOD, SCRIPT | results and notifications | ON5, ON4, ON18 (f) (landed) |
| the account consumer is called once per `PLATFORM_EVENT_ACCOUNT_CHANGED` on the main thread in prologue step 1, after `Platform.Pump` and before the frame listeners' `OnPlatformEvents` | ARCH, MOD | the rebind runs before the session's `Pump` | ON6 |
| the package bridge's `Zenith_ForwardingEntitlementQuery` provider set through `ProvideEntitlementQuery` (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Entitlement provider", PACK's answer PK21); PACK's existing revocation rule (DENIED unmounts, DENIED → GRANTED remounts), which ONLINE's fenced-deny answers rely on; ON3 (a) withdrawn | PACK, MOD | Stage 6 entitlements | ON3 (c), ON17, PK21 |
| the online catalogue asset kind (magic `ZONC`, extension `.zonc`, loader registered by ONLINE) | ASSET | the catalogue | ON11 |
| the `steamworks-sdk` manual-download prebuilt entry, the Play Maven pins, licence terms, `steam_api64.dll` staging, the `steam_appid.txt` refusal | SHIP | Stage 6 backends | ON9 |
| Gradle: the Java source directory, the two Maven dependencies, `android.playGamesAppId` and the `APP_ID` meta-data | MOD | Stage 6 Play backend | ON8 |
| the flagship slices' account and conflict flows, the cloud-status flow and achievement calls | GAME, TWOD | § "Flagship slice integration" | ON10, ON21 |
| the conflict screen in the accessible-flow acceptance | UI | Z2-I-ONLINE-12 | ON7 |
| the ONLINE owner row of `ZenithAuthoring` (`<Dir>` `Online`: `Zenith2/Tools/Authoring/Internal/Online/`, `Zenith_AuthoringOnline.h`, tests in `Tests/Online/`, fixtures in `Tests/Fixtures/Migration/Online/CatalogueDocument/`); the cook step registered in `ZenithCook`'s `Internal/Online/`; the editor panel in the extension table | EDIT, ASSET | authoring | ON13, ON20 |
| `ZenithOnline`'s `DEPENDS` widening; the two core libraries and the backends' layer 9; the parser-limit rows; the lock row 670 | ARCH, CONTRACT | layout and limits | ON12, ON14 (X322, X358) |
<!-- /assumption -->

## Library and file layout

### Targets

| Target | Kind | Directory | `ZENITH_SUPPORTED` | Stage |
|---|---|---|---|---|
| `ZenithOnline` | `KIND MODULE`, `MODULE_NAME Online` | `Zenith2/Engine/ZenithOnline/` | `ZENITH_GPU` (the presets of ZenithRuntime) | S4 |
| `ZenithOnline_Offline` | `KIND BACKEND`, `BACKEND_OF ZenithOnline`, `BACKEND_NAME Offline` | `Zenith2/Engine/ZenithOnline_Offline/` | `ZENITH_GPU` | S4 |
| `ZenithOnline_SteamCore` | `KIND ENGINE` (pending X363) | `Zenith2/Engine/ZenithOnline_SteamCore/` | `ZENITH_WINDOWS` | S6 |
| `ZenithOnline_Steam` | `KIND BACKEND`, `BACKEND_OF ZenithOnline`, `BACKEND_NAME Steam` | `Zenith2/Engine/ZenithOnline_Steam/` | `ZENITH_WINDOWS`; declared unconditionally, a declared-but-unavailable library (no target) while the `steamworks-sdk` archive is absent (assumption pending ARCH, X363 (c)), so no gate builds it on a machine without the partner SDK | S6 |
| `ZenithOnline_GooglePlayCore` | `KIND ENGINE` (pending X363) | `Zenith2/Engine/ZenithOnline_GooglePlayCore/` | `ZENITH_GPU` | S6 |
| `ZenithOnline_GooglePlay` | `KIND BACKEND`, `BACKEND_OF ZenithOnline`, `BACKEND_NAME GooglePlay` | `Zenith2/Engine/ZenithOnline_GooglePlay/` | `ZENITH_ANDROID` | S6 |
| `ZenithOnline_Tests`, `ZenithOnline_Conformance`, `ZenithOnline_Offline_Tests`, `ZenithOnline_SteamCore_Tests`, `ZenithOnline_GooglePlayCore_Tests`, `Zenith_Fuzz_Online*` | tests | the libraries' `Tests/` | as `Zenith2/Docs/Design/ONLINE/Tests.md` → "Test targets" | S4, S6 |

No source is excluded per configuration ([Z2-N-ARCH-171]): platform code lives in `Internal/Windows/` and `Internal/Android/`, and the Steam backend target's existence depends on the provisioned SDK, not on a source filter. Everything a gate can test of the store backends lives in the two core libraries, which need neither the SDK nor a device (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Store backend libraries and what the gates prove").

### ZenithOnline files

| File | Contents |
|---|---|
| `Zenith_OnlineTypes.h`, `Zenith_OnlineErrors.h`, `Zenith_OnlineCatalogue.h`, `Zenith_OnlineServices.h`, `Zenith_OnlineSession.h`, `Zenith_Module_Online.h` | the public headers of `Zenith2/Docs/Design/ONLINE/PublicAPI.md` |
| `Internal/Zenith_OnlineModule.cpp` | the two entry points; § "Module start" |
| `Internal/Zenith_OnlineModuleInstance.h`, `.cpp` | attach, detach, the frame listener, shutdown drain |
| `Internal/Zenith_OnlineSinkPort.h`, `.cpp` | the production publication port |
| `Internal/Zenith_OnlineScriptConsumer.h`, `.cpp` | the script side-effect consumer |
| `Internal/Zenith_OnlineBindings.cpp` | the `Online` binding table, argument validation, misuse codes |
| `Internal/Zenith_OnlineEvents.cpp` | event type registration, the `Online.Events` system |
| `Internal/Zenith_OnlineConfigKeys.cpp` | the configuration keys |
| `Internal/Zenith_OnlineTypes.cpp` | account ID and display-name functions, `Zenith_ToString` tables |
| `Internal/Zenith_OnlineAccountKeys.h`, `.cpp` | the keyed account key, `Zenith_OnlineAccountKeyFromHash`, the offline store key |
| `Internal/Zenith_OnlineCatalogue.cpp`, `Internal/Zenith_OnlineCatalogueLoader.cpp` | `Zenith_OnlineCatalogue`, the kind loader |
| `Internal/Zenith_OnlineCodec.h`, `.cpp` | request, result and notification encoding and decoding |
| `Internal/Zenith_OnlineCloudFormats.h`, `.cpp` | envelope, object names, index and its view classification, manifest, device record |
| `Internal/Zenith_OnlineSession.cpp` | `Create`, `Pump`, presentation actions, shutdown |
| `Internal/Zenith_OnlineSessionAccounts.cpp` | detection, fence, rebind, retry |
| `Internal/Zenith_OnlineSessionRequests.cpp` | inboxes, execution, pending pools, timeouts, results |
| `Internal/Zenith_OnlineSessionEntitlements.cpp`, `Internal/Zenith_OnlineSessionAchievements.cpp` | caches, refresh, achievement operations |
| `Internal/Zenith_OnlineNotifications.cpp` | queues, parking, the tick-boundary handler body |
| `Internal/Zenith_OnlineCloudEngine.h`, `.cpp`, `Internal/Zenith_OnlineCloudDecision.cpp`, `Internal/Zenith_OnlineCloudMaintenance.cpp` | the cloud engine, the decision table, garbage collection and the index repair |
| `Internal/Zenith_OnlineMemoryConsumer.cpp` | the memory consumer |
| `Internal/Zenith_OnlineDevTools.cpp` | console commands, the panel provider (registered only when the context returns the console and overlay) |
| `Internal/Zenith_OnlinePackEntitlements.h`, `.cpp` | Stage 6, PACK's query |
| `Tests/…` | `Zenith2/Docs/Design/ONLINE/Tests.md`; `Tests/Support/Zenith_OnlineRecordingPort.h`, `Zenith_OnlineFixtures.h`, `Zenith_OnlineIsolationChecker.h` (header-only, X184); `Tests/Examples/Zenith_OnlineSession_Example.cpp`; `Tests/Conformance/…`; `Tests/Fuzz/…` (no committed fixture: the catalogue's migration fixture is `ZenithAuthoring`'s, § "Tool-side files") |

### Backend files

| Library | Files |
|---|---|
| ZenithOnline_Offline | `Zenith_Online_Offline.h`, `Zenith_OnlineOfflineTestAccess.h`; `Internal/Zenith_OnlineServicesOffline.h`, `.cpp`; `Internal/Zenith_OfflineAchievementStore.h`, `.cpp`; `Internal/Zenith_OfflineCloudStore.h`, `.cpp`; `Internal/Zenith_OnlineOfflineTestAccess.cpp`; `Tests/Zenith_OnlineOffline_Tests.cpp`, `Tests/network_symbols.txt`, `Tests/Checks/Zenith_OnlineNoNetwork.cmake` |
| ZenithOnline_SteamCore | `Zenith_OnlineSteamCore.h`, `Zenith_OnlineSteamCoreTestAccess.h`; `Internal/Zenith_OnlineServicesSteam.h`, `.cpp` (the backend logic over `Zenith_SteamApi`); `Internal/Zenith_SteamApiScripted.cpp`; `Internal/Zenith_OnlineSteamCoreTestAccess.cpp`; `Tests/Zenith_OnlineSteam_Tests.cpp` |
| ZenithOnline_Steam | `Zenith_Online_Steam.h`; `Internal/Zenith_OnlineSteamFactory.cpp`; `Internal/Windows/Zenith_SteamApiFlat_Windows.cpp` (the only file including Steamworks headers) |
| ZenithOnline_GooglePlayCore | `Zenith_OnlineGooglePlayCore.h`, `Zenith_OnlineGooglePlayCoreTestAccess.h`; `Internal/Zenith_OnlineServicesGooglePlay.h`, `.cpp` (the backend logic over `Zenith_PlayBridge`); `Internal/Zenith_PlayBridgeScripted.cpp`; `Internal/Zenith_OnlineGooglePlayCoreTestAccess.cpp`; `Tests/Zenith_OnlinePlay_Tests.cpp` |
| ZenithOnline_GooglePlay | `Zenith_Online_GooglePlay.h`; `Internal/Zenith_OnlineGooglePlayFactory.cpp`; `Internal/Android/Zenith_PlayBridgeJni_Android.cpp`; `Internal/Android/Java/org/zenith2/online/Zenith2PlayGames.java`; `Tests/Checks/Pwsh_OnlinePlay_JavaBridgeMatchesJniTable.ps1` (registered by name in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` on `win`, label `z2-cmake`, independently of the library's `SUPPORTED`) |

### Tool-side files

| File | Owner rule |
|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringOnline.h`, `Zenith2/Tools/Authoring/Internal/Online/Zenith_AuthoringOnlineCatalogue.cpp` | the `ZenithAuthoring` library's ONLINE owner directory `<Dir>` = `Online` (orchestrator rulings Q84 (a) and AUTHORING-CONTRACT; `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rules 1, 3, 4): the document type registration `Zenith_RegisterOnlineDocumentTypes`, the validation `Zenith_ValidateOnlineCatalogueDocument` (rules C1 … C8) and the compile `Zenith_CompileOnlineCatalogue` (document → `ZONC` section body); no ZenithCook or ZenithEditor type; its move appends `ZenithOnline` and `ZenithDocument` to `ZenithAuthoring`'s `DEPENDS` where absent (`Zenith2/Docs/Design/EDIT.md` → "Dependency growth rule") |
| `Zenith2/Tools/Authoring/Tests/Online/Zenith_AuthoringOnline_Tests.cpp`, `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Online/CatalogueDocument/v1.json`, `v1.expected.json` | `ZenithAuthoring_Tests` (rule 2): the document, validation, compile and migration tests of `Zenith2/Docs/Design/ONLINE/Tests.md` → "Format and fuzz tests" |
| `Zenith2/Tools/ZenithCook/Internal/Online/Zenith_OnlineCookSteps.h`, `.cpp` | `ZenithCook` (rule 3: cook-step registration stays in ZenithCook): `Zenith_RegisterOnlineCookSteps` registers the step `online.catalogue` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions"), whose body calls the two authoring functions and converts `Zenith_OnlineDiagnosticLevel` to `Zenith_CookDiagnosticLevel` by value |
| `Zenith2/Tools/ZenithCook/Tests/Zenith_OnlineCook_Tests.cpp` | `ZenithCook_Tests`: `Test_OnlineCook_CatalogueStepWritesSection` |
| `Zenith2/Tools/ZenithEditor/Internal/Online/Zenith_EditorOnlinePanel.cpp` | the editor panel (§ "Editor surfaces") |

## Public API

### API summary

| Header | Specified in |
|---|---|
| every ONLINE header | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Header list" and the header headings it lists |
| the Luau surface | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API", "Event types", "Misuse codes" |
| configuration keys | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys" |

### Handle kinds used

ONLINE declares no handle kind. It holds `Zenith_OperationHandle`s of SAVE's listing, export, import and delete operations (`Zenith2/Docs/Contracts.md` → "Handle kinds"), cancelling them at a fence and when it shuts down; `Zenith_OnlineRequestID` and completion IDs are plain values, never serialized except as the recorded completion IDs SAVE and the replay log own. The catalogue is reached through a `Zenith_AssetHandle` held by the module instance for the process lifetime.

### Module start

`Zenith_Module_Online_Start(xContext, axBackends)`, startup step 15, in this order; a failing step returns its module error and the runtime undoes every registration of the steps before it (`Zenith2/Docs/Design/MOD.md` → "Registration journal and undo"), while the steps' own objects are owned by the partially built instance, whose destructor unwinds them:

| Step | Action | Failure |
|---|---|---|
| Catalogue | in replay: `Zenith_OnlineCatalogue::CreateEmpty`; otherwise read `online.catalogue`; empty → `CreateEmpty`; otherwise acquire the asset and `ServiceUntilComplete` its load (≤ 5,000 ms); a missing or invalid catalogue logs `online: catalogue <guid> unavailable (<error>); starting with an empty catalogue`, counts `online.catalogue.load_failed` and uses an empty one | allocation failure: `MODULE_ERROR_OUT_OF_MEMORY` |
| Backend | outside replay: § "Backend selection" of `Zenith2/Docs/Design/ONLINE/Backends.md`; a factory answering `ONLINE_ERROR_BACKEND_UNAVAILABLE` with native code −1 (Steam relaunching the game) makes the module call `RequestExitAfterStart()` before it tries the next candidate | no backend: `MODULE_ERROR_BACKEND_CREATE` |
| Session | `Zenith_OnlineSession::Create` with the configuration's settings (`m_bExposeDisplayName` forced false when `IsRecordingReplay()`, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure"), `xContext.GetSaveSystem()`, the platform, the I/O service, the governor and `IsReplaying()`; then, outside replay, `session.BindAtStart()` (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start"), whose `false` is not a failure; `Create` registers the session's `Zenith_OnlineSaveListener` with `SetSlotListener` as its last step (outside replay), undoing it on any failure, and `BeginShutdown` clears it first | `MODULE_ERROR_OUT_OF_MEMORY`; `MODULE_ERROR_REGISTRATION` for an invalid setting or a device record that cannot be created |
| Bindings | the `Online` binding table and its result decoder (`SetAsyncResultDecoder(ASYNC_SOURCE_ONLINE, Zenith_DecodeOnlineResultForScript)`), `RegisterScriptBindings` | `MODULE_ERROR_REGISTRATION` |
| AsyncSource | `DeclareScriptAsyncSource(ASYNC_SOURCE_ONLINE, uONLINE_RESULT_MAX_BYTES)` | `MODULE_ERROR_REGISTRATION` |
| Consumer | `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, m_xScriptConsumer)` | `MODULE_ERROR_REGISTRATION` |
| AccountConsumer | `RegisterAccountConsumer(&OnAccountEventThunk, &session)` | `MODULE_ERROR_REGISTRATION` (a second consumer in the process) |
| FrameListener | `RegisterFrameListener(instance)` | `MODULE_ERROR_REGISTRATION` |
| MemoryConsumer | `RegisterMemoryConsumer(session.GetMemoryConsumer())` | `MODULE_ERROR_REGISTRATION` |
| Console | development builds: the commands of § "Console commands" through `GetDevConsole()` | `MODULE_ERROR_REGISTRATION` |
| Panel | development builds: the `online` panel provider through `GetProfilerOverlay()` | `MODULE_ERROR_REGISTRATION` |
| Entitlements | Stage 6: `ProvideEntitlementQuery(session.GetPackageEntitlements())` | `MODULE_ERROR_REGISTRATION` |

In replay mode the Backend step is skipped, the Catalogue step creates the empty catalogue the session's desc requires, the session is created inert, and the Console step registers only the read-only `online.status` command.

### World attach and detach

- `AttachWorld(xContext)`: bind the world's publication port to `GetPublicationSink()`; `session.AttachWorld(tag, purpose, port)` (allocates the world record: the inbox, the pending pool, the notification queue and the published buffer); set the sink's handler for `ASYNC_SOURCE_ONLINE` to the instance's forwarder, which calls `session.OnPublished`; register the system `Online.Events@W<tag>` (`PHASE_INPUT`, `EmitsEvent` of the five event types) through `GetSystems()`. A failure returns `MODULE_ERROR_OUT_OF_MEMORY` or `MODULE_ERROR_REGISTRATION` after undoing the earlier steps.
- `DetachWorld(tag)`: `session.DetachWorld(tag)` completes every outstanding request of the world `ABANDONED` through the port (the sink parks into the world's table, live or retired), drops its queued notifications, frees the record, and unbinds the port; the runtime then undoes the attach registrations.

## Data structures and formats

### In-memory structures

| Structure | Contents | Size |
|---|---|---|
| session | state, sub-state, generation (`std::atomic<uint64>`, odd while fenced), bound account, profile, display name, the account-ordinal table (16 × 97 bytes, memory only), pending profile and attempt count, the VERIFYING entry time, device ID and install secret, settings, internal pending pool (64 records), the entitlement cache (256 × 1 byte plus revision and the package-answer mode), the achievement cache (1,024 × 12 bytes), the cloud engine | fixed at `Create` |
| world record | tag, purpose, port pointer, previous-profile flag, inbox (`Zenith_SpscRing` of `m_uMaxRequestsPerWorld` records of 112 bytes: completion ID, generation, intake time in milliseconds, 80 payload bytes, length), pending pool (`m_uMaxRequestsPerWorld` records, deferred records of a VERIFYING hold included), notification queue (96 × 264 bytes), published buffer (16 × 264 bytes), the outstanding set (8 completion IDs) | allocated at `AttachWorld`: 60,864 bytes for 64 requests (inbox 64 × 112 = 7,168, pending pool 64 × 376 = 24,064, notification queue 96 × 264 = 25,344, published buffer 16 × 264 = 4,224, outstanding set 8 × 8 = 64) |
| pending record | completion ID, world tag, op, generation, deadline, backend request ID, waiting-on flags, result scratch (256 bytes), held completion (status and 64 payload bytes, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification") | 376 bytes |
| cloud engine | the slot table (64 entries of 352 bytes: slot, kind, state, reason, L, C, B, T, local and cloud metadata including the 128-byte cloud display name, the 41-byte conflict token, backoff), the index view, the index buffer (34,880 bytes), the manifest buffer (6,672 bytes), the action queue (64 entries), at most one transfer buffer, the detached-buffer list (buffers awaiting their cancelled requests' completions) | ≈ 63 KiB plus the transfer: the slot table (22,528 bytes) and the action queue count in proposed O1, the index and manifest buffers (41,552 bytes) with the transfer buffer in proposed O2 |

### Formats summary

| Format | Where | Specified in |
|---|---|---|
| online catalogue document (`zenith2.online`) | `Games/<Name>/Online/online.json` | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Online catalogue document" |
| cooked online catalogue (`ZONC`) | the cooked content | "Cooked online catalogue" |
| script request, result and notification payloads | `ASYNC_SOURCE_ONLINE` publications, the replay log | "Script request payloads", "Script result payloads", "Notification payloads" |
| backend completion payloads | in memory | "Backend completion payloads" |
| cloud envelope (`ZCLD`), cloud index (`ZCIX`) | the backend's cloud | "Cloud envelope", "Cloud index" |
| sync manifest (`ZOSM`), device record (`ZODV`, holding the install secret) | `user:/saves/online/` | "Sync manifest", "Device record" |
| offline achievements (`ZOAC`), offline cloud objects (`ZOCO`) | `user:/saves/online/offline/`, `user:/saves/online/cloud/<cloudRoot>/` | "Offline achievement file", "Offline cloud store" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| module start, world attach and detach | § "Module start", § "World attach and detach" |
| backend selection | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend selection" |
| sign-in, detection, verification, rebind, retry, isolation, fencing | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Sign-in", "Account change detection", "Identity verification", "Rebind sequence", "Cross-account isolation", "Generation fencing" |
| entitlement refresh, achievement operations | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlements", "Achievements" |
| script requests, results, timeouts, notifications | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Script request processing", "Notifications" |
| index view, reconcile, upload, download, deletion and tombstones, garbage collection, index repair, conflicts, policies, resolution, recovery, flush | `Zenith2/Docs/Design/ONLINE/CloudSave.md` |
| backend internals | `Zenith2/Docs/Design/ONLINE/Backends.md` |

### State machine index

| State machine | Specified in |
|---|---|
| session states | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine" |
| cloud slot states | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot sync state machine" |
| cloud status | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting" |

### Frame-graph placement

| Work | Where | Thread |
|---|---|---|
| the account consumer (rebind) | frame prologue step 1, per account event, after `Platform.Pump` | main |
| `Pump`, in this order: backend pump, identity verification and detection with its fence, inbox drain and request execution, timeouts, profile-switch retry, cloud engine, notification parking | frame prologue step 1, the frame listener's `OnPlatformEvents` | main |
| script request intake | each world's merge nodes, through the multiplexer | the merge node's worker |
| the publication handler | each world's tick-boundary node, step 1 | the boundary node's worker |
| `Online.Events@W<tag>` | phase 1 of each tick of each world | a job worker |
| shutdown drain | shutdown step 6 | main |

ONLINE adds no frame-graph stage and no epilogue node. The prologue is not a frame-graph subgraph ([Z2-N-ARCH-437]), so its main-thread work never blocks a job.

### Tick walkthrough

1. Prologue step 1, frame F: `Platform.Pump` converts a posted account signal into an event; the runtime calls `OnAccountEvent`, which rebinds (SAVE profile, caches, the generation to its next even value) and queues IDENTITY_CHANGED; `OnPlatformEvents` runs `Pump`: completions arrive, a change of the backend's identity is detected and fenced before the inboxes drain, results and notifications are parked (notification IDs allocated now).
2. The frame's ticks: at each world's tick boundary the parked publications become visible in completion-ID order; ONLINE's handler collects notifications; phase 1 emits the `Online.*` events; phase 9 delivers them (and PLAT's `Platform.AccountChanged`); behaviours read ticket results and record new requests; merge nodes route the requests into the inboxes.
3. Frame F + 1, prologue step 1: `Pump` drains the inboxes recorded in frame F's ticks.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_OnlineSession` | `MAIN_THREAD`, except `SubmitScriptRequest` (merge nodes, one producer per world), `OnPublished` and `TakePublishedNotifications` (the world's tick-boundary node and phase-1 node, ordered by the tick) and `GetStats` (`ANY_THREAD`) |
| `Zenith_OnlineServices` and every backend | `MAIN_THREAD` |
| `Zenith_OnlinePublicationPort` | as its contract block |
| `Zenith_OnlineCatalogue` | `IMMUTABLE` |
| every value type | `IMMUTABLE` |
| `Zenith_OnlineOfflineTestAccess` | `MAIN_THREAD` |

### Threads ONLINE owns

None. The offline backend's I/O runs on PLAT's I/O threads; the Steam SDK's internal threads belong to the SDK and are stopped by `SteamAPI_Shutdown`; Play's callbacks run on the Android UI thread, owned by the activity. No row of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ("Shutdown ownership order") is added.

### Cross-thread hand-offs

| Hand-off | Producer → consumer | Primitive | Full policy |
|---|---|---|---|
| script requests | a world's merge chain → main thread | one `Zenith_SpscRing` per world (CORE; ruling Q80), `m_uMaxRequestsPerWorld` records | `REJECT`, unreachable when the merge accepted the request; refused requests complete `REFUSED` |
| Play callbacks | Android UI thread → main thread | the backend's event queue, 256 records, under `uLOCK_LEVEL_ONLINE_PLAY_EVENTS` | `REJECT`: the request completes `BACKEND_ERROR` |
| the generation | main thread → merge nodes | `std::atomic<uint64>`, store-release at the fence and at the bind, load-acquire at intake | — |
| AsyncIO completions | PLAT's I/O threads → main thread | PLAT's completion queues (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API") | PLAT's |

### Lock-order entries

<!-- assumption: pending CONTRACT (row ON14) -->
One new row of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ("Lock ordering table"): level 670, `uLOCK_LEVEL_ONLINE_PLAY_EVENTS`, owner ZenithOnline_GooglePlayCore (the `Zenith_PlayEventQueue` both Play bridges own; core-doc item X460 amends X323's owner), guards that queue and its read-buffer table, short-hold yes, same level by address no, acquired on the Android UI thread (the JNI natives) and the main thread (`Pump`, `Submit`). Its holder copies bytes and pushes or pops; it takes no other lock and allocates nothing (the queue and table are reserved at creation), so 670 sits between PLAT's 640 and SCRIPT's 700 without nesting either.
<!-- /assumption -->

### Startup and shutdown

- Startup: `RegisterTypes` at step 6; `Start` at step 15 (§ "Module start"), which binds the account synchronously when the backend is already signed in (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start"); `AttachWorld` for each world as it is created (step 16 for the first); the first `Pump` runs in the first frame's prologue.
- Shutdown step 4 (the runtime's cancellation of outstanding operations) cancels ONLINE's SAVE operations through their handles like every other; the session completes the corresponding cloud jobs as cancelled.
- Shutdown step 6 destroys the module instance in reverse registration order, while `Zenith_AsyncIO` (step 8) and the save system (step 7b) are alive: `BeginShutdown`, then a drain loop that calls the backend's `Pump` and `PumpShutdown` until `IsDrained` or the flush deadline plus 1,000 ms passes, then the backend's `Shutdown(sink, 1,000 ms)`, then destruction of the session, the backend and the catalogue handle. Worlds are detached in step 6 before modules are destroyed, so every outstanding request has already been completed `ABANDONED`.

### Cancellation

| Operation | Cancellation points | Who frees partial results |
|---|---|---|
| a backend request | `Cancel` (fence, timeout, shutdown) | the backend, before delivering `CANCELLED` |
| a SAVE export, import, listing or delete | `RequestCancel` on its handle (fence, shutdown) | SAVE ([Z2-N-CONTRACT-335]); an import cancelled after SAVE's commit point completes as succeeded and is adopted by the next reconcile |
| a transfer | its current step's request or operation; the transfer buffer is detached from the job and freed, with the governor reservation released, only once every request or operation that was given it has delivered its completion | the session |
| a script request | its backend request or job; the script result is completed at the fence, timeout or detach | the session |

The 1,000-cycle obligation of [Z2-N-CONTRACT-339] is `Test_OnlineCloud_Stress1000CancelledTransfers` and `Test_OnlineRebind_Stress1000Switches`.

### Error enumerations

`Zenith_OnlineErrorCode` with the record `Zenith_OnlineError` (`Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineErrors header") for every fallible C++ call; `Zenith_OnlineStatus` for the outcome of each request (`Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineTypes header"). Errors crossing into a module error are mapped at `Start` and `AttachWorld` (`MODULE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_BACKEND_CREATE`, `MODULE_ERROR_REGISTRATION`); SAVE errors reach ONLINE as `ONLINE_ERROR_SAVE` with the SAVE code in `m_uDetail`.

### Luau errors versus asserts

- Script misuse (an invalid ID, out-of-range steps or increments, an invalid slot name or choice) raises a catalogued Luau error ZS2901 … ZS2905 at the call, never an assert ([Z2-N-CONTRACT-473]); everything else a script can cause is a result status.
- Backend data, cloud objects, catalogue bytes and ONLINE's files are untrusted: every violation is an error value, never an assert ([Z2-N-CONTRACT-472]).
- Asserts guard only engine preconditions: a null pointer passed by engine code, a call on the wrong thread (`ZENITH_ASSERT` of the affinity), `Zenith_OnlineSession`'s destructor without `BeginShutdown` (`ZENITH_CHECK`).

### Determinism obligations

- No simulation-visible value depends on an online backend except through recorded publications (`ASYNC_SOURCE_ONLINE`, always `PUBLICATION_KIND_GENERIC_RECORDED`) and PLAT's recorded account events; no world resource, component or system reads session state.
- Completion IDs for script requests are SCRIPT's, assigned at commit in merge order; ONLINE's own notification IDs are allocated only in the frame prologue and only for a recorded kind, and never under `--replay` (orchestrator ruling Q89 (a); the replaying table adopts the recorded next completion ID of each `TICK` record, cross-doc item X278).
- A replay runs with no backend and reproduces every result, event and tick hash (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Replay mode").
- No online configuration key is simulation-affecting; none is written into the replay header.

## Diagnostics and profiling

### Counters

Names follow [Z2-N-CONTRACT-156]; every bounded queue also has its `.full`, `.dropped` and `.high_water` triple.

| Counter | Kind | Meaning |
|---|---|---|
| `online.account.rebinds` | cumulative | completed binds |
| `online.account.stale_signals`, `online.account.duplicate_signals`, `online.account.signal_reposted`, `online.account.post_failed` | cumulative | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps", "Account change detection" |
| `online.account.profile_busy_retries`, `online.account.profile_switch_failures`, `online.account.late_profile_switches` | cumulative | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry" |
| `online.account.verify_timeouts` | cumulative | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification" |
| `online.account.generation` | gauge | the bind generation |
| `online.requests.accepted_delta` | delta | script requests drained this frame |
| `online.requests.account_changed`, `online.requests.account_changing`, `online.requests.previous_profile`, `online.requests.timeouts`, `online.requests.decode_failed`, `online.requests.inbox_full` | cumulative | result statuses of note; the last two are defects |
| `online.requests.pending` | gauge | pending script and internal records |
| `online.replay.requests_dropped` | cumulative | live requests dropped in replay mode |
| `online.notify.parked`, `online.notify.dropped`, `online.notify.dropped_at_fence`, `online.notify.site_refused` | cumulative | notifications (`site_refused`: the sink refused the allocation site, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" step 3) |
| `online.events.decode_failed`, `online.events.intern_failed` | cumulative | the handler and the emitter |
| `online.entitlements.revision` | gauge | the entitlement revision |
| `online.entitlements.refreshes`, `online.entitlements.refresh_failures` | cumulative | refreshes |
| `online.achievements.unlocks`, `online.achievements.step_updates` | cumulative | operations submitted |
| `online.cloud.uploads`, `online.cloud.downloads`, `online.cloud.deletes`, `online.cloud.conflicts`, `online.cloud.resolved`, `online.cloud.transfer_failures`, `online.cloud.rejected_objects`, `online.cloud.transfer_deferred`, `online.cloud.objects_collected`, `online.cloud.records_dropped`, `online.cloud.lost_commits`, `online.cloud.index_full`, `online.cloud.objects_healed`, `online.cloud.horizon_conflicts`, `online.cloud.stale_resolutions`, `online.cloud.name_collisions`, `online.cloud.missing_objects`, `online.cloud.cloud_resets`, `online.cloud.index_repairs`, `online.cloud.tombstones` | cumulative | the cloud engine |
| `online.cloud.index_view` | gauge | 1 KNOWN, 2 EMPTY, 3 UNKNOWN READ_FAILED, 4 INVALID, 5 FOREIGN_PROFILE, 6 MISSING, 7 NEWER_FORMAT |
| `online.cloud.bytes_uploaded`, `online.cloud.bytes_downloaded` | cumulative | bytes moved |
| `online.cloud.slots_in_conflict` | gauge | slots in CONFLICT |
| `online.backend.bad_payload`, `online.backend.bad_data` | cumulative | rejected backend data |
| `online.catalogue.load_failed` | cumulative | catalogue fallback |
| `online.os.session_locked`, `online.os.ignored` | cumulative | OS signals |
| `online.play.events.full`, `.dropped`, `.high_water` | cumulative | the Play event queue |
| `online.memory.resident_bytes` | gauge | the memory consumer's report |

### Profiling zones

`Online.Pump` (with child zones `Online.Rebind`, `Online.Requests`, `Online.CloudReconcile`, `Online.CloudTransfer`, `Online.Notifications`), `Online.Intake` (each consumer call), `Online.Publish` (each handler call), `Online.Events` (the phase-1 system). `Online.Pump`'s total is the measurement of the proposed main-thread row (§ "Budget rows").

### Log categories and messages

Categories `online` (startup, backend selection, catalogue), `online.account`, `online.cloud`, `online.steam`, `online.play`. No message contains an account identifier, an account key, a profile key, a display name, a token or a purchase token; generations and account ordinals (`#<n>`, or `local` for the local profile, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys") identify accounts.

| Category | Level | Message |
|---|---|---|
| `online` | info | `online: backend <name> started (capabilities 0x<bits>)` |
| `online` | warning | `online: online.backend '<name>' is not linked; using the composition's order` |
| `online` | warning | `online: online.exposeDisplayName ignored while recording a replay` |
| `online` | info | `online: online.exposeDisplayName is on; display names reach worlds` |
| `online` | warning | `online: device record replaced` |
| `online` | warning | `online: backend <name> unavailable (<native code>); trying <next>` |
| `online` | error | `online: catalogue <guid> unavailable (<error>); starting with an empty catalogue` |
| `online` | warning | `online: entitlement '<id>' is not in the online catalogue` |
| `online.account` | info | `online.account: fenced (generation <g>)` |
| `online.account` | info | `online.account: bound generation <g> to account <#n or local> (<reason>)` |
| `online.account` | warning | `online.account: profile switch for account <#n> busy; retrying` (once per rebind) |
| `online.account` | error | `online.account: profile switch for account <#n> failed <n> times (<save error>); using local` |
| `online.account` | info | `online.account: account <#n> moved to its own profile (late switch)` |
| `online.account` | info | `online.account: identity unverified; waiting (generation <g>)` |
| `online.cloud` | info | `online.cloud: conflict on '<slot>' (<reason>)` |
| `online.cloud` | info | `online.cloud: conflict on '<slot>' resolved by <policy or player>: <choice>` |
| `online.cloud` | warning | `online.cloud: cloud copy of '<slot>' rejected (rule <n>)` |
| `online.cloud` | warning | `online.cloud: cloud index invalid (rule <n>); cloud sync suspended until repaired` |
| `online.cloud` | warning | `online.cloud: cloud index <foreign, missing, newer>; cloud sync suspended` |
| `online.cloud` | info | `online.cloud: cloud is empty; re-seeding from this device` |
| `online.cloud` | info | `online.cloud: cloud index repaired (<n> slots)` |
| `online.cloud` | error | `online.cloud: '<slot>' references a missing cloud object` |
| `online.cloud` | info | `online.cloud: lost commit of '<slot>' (<absent, regressed>); re-uploading` |
| `online.cloud` | warning | `online.cloud: index holds 64 slots; '<slot>' not uploaded` |
| `online.cloud` | info | `online.cloud: '<slot>' base is older than the cloud's commit log; asking` |
| `online.cloud` | info | `online.cloud: '<slot>' base predates a cloud index repair; asking` |
| `online.cloud` | info | `online.cloud: resolution of '<slot>' is stale; asking again` |
| `online.cloud` | info | `online.cloud: '<slot>' diverged from another history; conflict recorded` |
| `online` | error | `online: notification source refused by the publication sink (site)` |
| `online.cloud` | warning | `online.cloud: transfer of '<slot>' failed (<status>, <native code>); retrying in <ms> ms` |
| `online.steam` | info | `online.steam: relaunching through Steam` |
| `online.play` | warning | `online.play: purchase acknowledgement failed (<response code>)` |

### On-screen profiler rows and panel

The `online` panel (a provider registered through the overlay, `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers") shows: the backend and its capabilities; the session state, sub-state, generation, account ordinal and whether the bound profile is the local one (never a profile key); connectivity and the cloud status and the index view; the pending request count and the oldest pending age; one row per cloud slot (name, kind, state, reason, local and cloud UTC); the last 16 account and cloud log lines. The profiler's counter view shows the counters above; `Online.Pump` appears in the main-thread timeline.

### Console commands

Registered in development builds through `GetDevConsole()` (`Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"), first segment `online`; each changes only development state or calls a presentation action, never simulation-visible state directly (an account change reaches worlds through PLAT, a resolution through ONLINE's recorded publications).

| Command | Effect |
|---|---|
| `online.status` | prints the identity view, the entitlement revision and the cloud status (also in replay) |
| `online.signIn [interactive]` | `RequestSignIn` |
| `online.signOut` | `RequestSignOut` (offline backend) |
| `online.switch <name>` | `RequestSwitchAccount` (offline backend) |
| `online.cloud.list` | prints `GetCloudSlots` |
| `online.cloud.sync` | `RequestCloudSync` |
| `online.cloud.resolve <slot> <local|cloud|both|defer>` | `ResolveCloudConflict` |
| `online.cloud.repairIndex` | `RequestCloudIndexRepair` |
| `online.entitlements` | prints every catalogue entitlement with its state |

### Telemetry

The cumulative counters above enter the frame telemetry with every other counter; ONLINE emits no telemetry of its own and sends nothing anywhere (§ "Security assumptions" S5).

### Memory and the governor

The session's consumer reports `MEMORY_SUBSYSTEM_OTHER`: the fixed tables, the world records and the running transfer's buffer. Shedding: at WARNING nothing; at CRITICAL no new transfer starts (a running one finishes), uploads wait, garbage collection and index repairs wait, and the caches stay (at most 13 KiB). Transfer buffers are reserved through `ReserveSubsystemBytes` before allocation (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Transfer memory").

## Authoring and cooking

### What ONLINE authors

One authoring document per game, the online catalogue (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Online catalogue document"), edited as JSON text: ONLINE has no visual editor for it, as Z2-D-079 names no visual authoring for online services. The Steam and Play partner configurations (achievement names, stats, products) are made in the partners' own consoles by a human and checked against the catalogue (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access").

### Cook steps

The cook step `online.catalogue` is registered by `Zenith_RegisterOnlineCookSteps` in `ZenithCook`'s `Internal/Online/` (ASSET's cook step registration, `Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions"; cook-step registration stays in ZenithCook by `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 3). Its body: open the source document migrated to its current version (a `Zenith_DocumentTree`), call `Zenith_ValidateOnlineCatalogueDocument` (C1 … C8) and `Zenith_CompileOnlineCatalogue` (sort the definitions by ID, build the `ZONC` section body), convert each `Zenith_OnlineDiagnostic` to a cook diagnostic by value (`Zenith2/Docs/Design/ASSET/Cook.md` → "Cook diagnostics"), and write the section into the cooked asset container; its DDC key covers the document bytes and the format version. A validation error fails the cook with ASSET's step failure and ONLINE's rule in the detail field (the ENV-F7 pattern of the orchestrator's rulings). The authoring functions are declared in `Zenith2/Tools/Authoring/Zenith_AuthoringOnline.h`:

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringOnline.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_DocumentTree.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"

/// @contract enum Zenith_OnlineDiagnosticLevel
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; the values equal Zenith_CookDiagnosticLevel's WARNING and ERROR.
/// @endcontract
enum Zenith_OnlineDiagnosticLevel : uint8
{
	ONLINE_DIAGNOSTIC_LEVEL_WARNING = 2,
	ONLINE_DIAGNOSTIC_LEVEL_ERROR = 3,
};

/// @contract value Zenith_OnlineDiagnostic
/// Ownership: Plain value; the strings point into the caller's scratch arena.
/// Thread affinity: IMMUTABLE once appended; members non-const: built field by field before it is appended.
/// Invalidation: With the scratch arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_OnlineDiagnostic
{
	Zenith_OnlineDiagnosticLevel m_eLevel;      // ERROR for C1 … C7, WARNING for C8
	uint16 m_uRule;                            // 1 … 8 (C1 … C8)
	Zenith_StringView m_strPointer;             // the JSON pointer ("/achievements/3/steps")
	Zenith_StringView m_strMessage;             // the text of Formats.md "Validation rules", reported as online.json: <pointer>: <message>
};

/// @contract functions Zenith_AuthoringOnline
/// Ownership: Results are written into the caller's arena or array; nothing is retained between calls.
/// Thread affinity: ANY_THREAD; pure functions of their arguments, so the cook and the editor's document view agree.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_OnlineError: ONLINE_ERROR_CORRUPT (m_uDetail the C rule) when the document has an ERROR diagnostic,
///   ONLINE_ERROR_OUT_OF_MEMORY; never an assert on document content ([Z2-N-ARCH-951]).
/// Version: since Z2 Stage 4. Tool-side; ZenithAuthoring is outside every Shipping link closure.
/// @endcontract
Zenith_Result<void, Zenith_OnlineError> Zenith_RegisterOnlineDocumentTypes(Zenith_MigrationRegistry& xRegistry);
Zenith_Result<void, Zenith_OnlineError> Zenith_ValidateOnlineCatalogueDocument(const Zenith_DocumentTree& xDocument,
	Zenith_Arena& xScratch, Zenith_Array<Zenith_OnlineDiagnostic>& axOut);
// The ZONC section body of Formats.md "Cooked online catalogue" for a document that validated without an ERROR.
Zenith_Result<Zenith_ByteSpan, Zenith_OnlineError> Zenith_CompileOnlineCatalogue(const Zenith_DocumentTree& xDocument,
	Zenith_Arena& xArena);
```

### Migration hooks

The document type `Zenith.Online.CatalogueDocument` is registered with SCENE's document framework by `Zenith_RegisterOnlineDocumentTypes` (`ZenithAuthoring`) with schema version 1, an empty migration chain, and unknown-field preservation; every schema change adds one step and one fixture pair under `ZenithAuthoring_Tests`' fixture root (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Migration chain"); core-doc item X336 (with X359's path) lists it in [Z2-N-CONTRACT-552].

### Editor surfaces

The editor's Online panel (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"; a row of EDIT's extension table) shows the play session's identity view and cloud slot states, and offers Sign in, Sign out, Switch to (the offline accounts), Sync now, Repair index (only while the index is unreadable) and Resolve (per conflicted slot), each calling the session's presentation actions. Play-in-editor sessions run the offline backend with the editor's own save profile (I10). The catalogue document opens in the editor's text document view with C1 … C8 shown as diagnostics from `Zenith_ValidateOnlineCatalogueDocument`.

### Flagship slice integration

<!-- elaboration -->
Game2D and OpenWorldSlice select `Online` (`Zenith2/Docs/Design/MOD.md` → "Repository games") and carry, as game content (Luau and data, no C++):

| File (in each game) | Purpose |
|---|---|
| `Online/online.json` | the catalogue: 8 achievements (2 incremental), 1 entitlement (a sample DLC), cloud kinds manual, quick and autosave |
| `Scripts/Online/AccountFlow.luau` | a behaviour on the game's session entity: on `Online.IdentityChanged` with `PreviousProfileSession`, transitions to the title world (the transition carries nothing over, I11); with `SignedIn` (reasons `SignedIn`, `Switched` or `Attached`), re-asserts the achievements its save state records; the title world holds its entry into gameplay until the first `Online.IdentityChanged` or 10 s (row ON23 (b)) |
| `Scripts/Online/CloudConflictFlow.luau` | on `Online.CloudConflict`, confirms the conflict with `Online.QueryCloudSlot`, opens the conflict screen with the event's fields and maps its buttons to `Online.ResolveCloudConflict`; on `Online.CloudStatus` with `IndexUnreadable`, shows the localized message `online.cloud.indexUnreadable` with a Repair button calling `Online.RepairCloudIndex` |
| `Schemas/CloudConflictPrompt.schema.json` | the component the screen binds to |
| `Assets/UI/CloudConflict.ui.json` | the conflict screen of `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reference conflict screen" |
| `Localization/en/online.strings.json` | the screen's strings (`online.conflict.*`, `online.cloud.*`) |

The slices' playthrough scripts include an account switch through the console on the offline backend (Stage 4) and the device runs with the store backends (Stage 6).

## Failure handling

| Failure | Behaviour |
|---|---|
| no backend can start | the offline backend (always linked with the module) starts; `Start` fails only if the offline factory itself fails (allocation) |
| the store backend's SDK fails mid-session (Steam client exit) | identity SIGNED_OUT, rebind to `local`; requests complete `NOT_SIGNED_IN`; the game is expected to quit as Steam requires |
| network unreachable | requests complete `UNAVAILABLE`; cloud status unavailable; retries with backoff; nothing local changes |
| catalogue missing or corrupt | an empty catalogue, logged and counted; every ID answers `unknownId` |
| SAVE busy at a switch | retried every prologue; the session stays fenced; SAVE's intake freeze refuses the previous profile's new saves, so the switch cannot be starved |
| SAVE profile switch failing | degrade to `local` after the attempt limit; cloud disabled until the switch succeeds |
| corrupt ONLINE file (manifest, device record, offline achievements) | treated as absent or renamed `.corrupt`; the model's rules make absence safe (no silent overwrite) |
| corrupt or foreign cloud object | rejected by rule, never imported, never deleted by the transfer, counted |
| corrupt, foreign, newer or missing cloud index, or a failed index read | an UNKNOWN index view: no deletion, transfer, index write or collection anywhere; repaired only on request, newer never by this build (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view") |
| a local save file deleted outside SAVE | no tombstone: restored from the cloud, never propagated as a deletion |
| an index commit undone by the store (Steam's launch dialog, Play's offline commit losing to the server) | a lost commit: re-uploaded; never a local deletion or a download over the newer local save (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table" rows 18, 19, 22) |
| the publication sink refuses the notification allocation site (a defect: MOD admits the mixed kind's prologue site) | logged and counted once; no notification is parked for the process (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" step 3) |
| the player changes the Play Games account while the game is backgrounded | the resume fence holds every request and completion until the player is confirmed, then rebinds or resumes (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification") |
| cloud quota exhausted | the slot ERROR, cloud status quota exceeded, retried on the next successful write elsewhere |
| allocation failure for a transfer | the governor refuses or the allocation fails: the transfer waits for the next `Pump` |
| allocation failure at `Create`, `Start` or `AttachWorld` | the error is returned after unwinding; tested per step |
| a world detaches with requests in flight | each completed `abandoned` |
| a save restore with requests in flight | SAVE completes each with the zero-length payload, decoded as `abandoned` |
| an interrupted transfer (kill, crash, power loss) | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Interrupted sync recovery" |
| device loss and the clean exit | shutdown as usual; the flush is bounded by its deadline inside the 10 s of [Z2-N-CONTRACT-484] (`online.cloud.shutdownFlushMs` at most 3,000 ms by default) |
| budget breach (memory) | the governor's CRITICAL level stops new transfers |

## Test strategy

### Test kinds

| Kind | Where |
|---|---|
| unit tests of the session, rebind and isolation, entitlements, achievements, the cloud model and every format | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Session unit tests" to "Format and fuzz tests" |
| the backend conformance suite, once per backend | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Conformance suite" |
| backend tests over the scripted seams | "Offline backend tests", "Steam backend tests", "Google Play backend tests" |
| integration suites over the headless runtime | "Script surface tests", "Determinism and replay tests", "Integration suites" |
| fuzz targets | "Format and fuzz tests" |
| stress | "Stress tests" |
| device and needs-human runs | "Device and needs-human runs" |

### Required named tests summary

Every test of `Zenith2/Docs/Design/ONLINE/Tests.md` is required; each requirement of the catalogue names the tests that prove it (`Zenith2/Docs/Requirements/ONLINE.md`).

## Budget rows

### Rows ONLINE is measured against

| Row | Name | ONLINE's use |
|---|---|---|
| Z2-B-SCRIPT-026 | Per-source asynchronous request capacity in flight | its online value sizes each world's inbox and pending pool (`m_uMaxRequestsPerWorld`, default 64 until the row is allocated); the source's capacity is that value plus 8 notifications (row ON4) |
| Z2-B-SAVE-001 | Save serialization buffers | SAVE's import charges its write arena; ONLINE's own transfer buffer is outside it |

### Pending row needs

Requested through core-doc item X321 for `Zenith2/Docs/Budgets.md` → "Per-system rows" (pending, with producers; the Budgets batch assigns their IDs):

| Proposed row | Name | Unit | Procedure | Platform | Breach | Producer |
|---|---|---|---|---|---|---|
| proposed O1 | Online services resident memory (session, caches, world records, slot table; excluding the transfer, index and manifest buffers) | KiB | MP-SUBMEM | Win, S24 | BR-GOVERNOR | the Stage 1 per-system budget ticket for ONLINE |
| proposed O2 | Online transfer buffer peak (the transfer buffer plus the 34,880-byte index buffer and the 6,672-byte manifest buffer) | MB | MP-SUBMEM | Win, S24 | BR-GOVERNOR | the same |
| proposed O3 | `Online.Pump` main-thread CPU per frame | ms | MP-CPUZONE | Win, S24 | BR-ACCEPT | the same |
| proposed O4 | Account rebind latency (account event to bind, SAVE idle) | frames | MP-COUNTER | Win, S24 | BR-ACCEPT | the same |

Proposed values for the producer ticket to measure against (proposals, not rows): O1 544 KiB, derived from its parts and never below them: 8 world records of 60,864 bytes (486,912), the slot table (22,528), the internal pool of 64 × 376 bytes (24,064), the achievement cache (12,288), the account-ordinal table (1,552) and the entitlement cache with its revision (264) sum to 547,608 bytes ≈ 535 KiB, plus the 64-entry action queue, rounded up to the next 32 KiB (core-doc item X459 restates the proposal X321 carried, 256 KiB, which was below its own parts); O2 64 MB (`online.cloud.maxTransferBytes`); O3 0.05 ms desktop and 0.15 ms S24 with no transfer running; O4 1 frame.

## Stage plan

### Stage 1

The SCOPE decision for online backends is settled (Z2-D-074); the Stage 1 per-system budget ticket for ONLINE allocates the proposed rows (`needs-human`, kind `budget`).

### Stage 4

1. API-only slice (pulled forward into the Stage 3 sprint so consumers compile against it): `Zenith_OnlineTypes.h`, `Zenith_OnlineErrors.h`, `Zenith_OnlineServices.h`, `Zenith_OnlineCatalogue.h`, `Zenith_OnlineSession.h`, `Zenith_Module_Online.h`, with stub definitions, so MOD's thunk and the composition table's `Zenith_OnlineBackendEntry` resolve.
2. Formats and codecs: catalogue, payloads, envelope, index, manifest, device record, with their fuzz targets.
3. The offline backend and its test access; the conformance suite (offline).
4. The session: state machine, sign-in, detection, rebind, isolation, entitlements, achievements, requests, notifications, replay mode.
5. The cloud engine: index view, decision table with tombstones and overrides, reconcile, content-addressed upload, download, deletion, garbage collection, index repair, conflicts, policies, resolution, recovery, flush.
6. The module: start, attach, bindings, events, configuration, console, panel, memory consumer.
7. Authoring: the catalogue document type, validation and compile in `ZenithAuthoring` with their tests and migration fixture in `ZenithAuthoring_Tests`; the cook step in ZenithCook; the editor panel.
8. Integration: the three suites; the flagship slices' online content and flows; the S24 offline device run; the accessible conflict flow run.

Gated by: SAVE's Stage 4 export, import, slot listener, binding-profile and intake-freeze additions and the `saves/online/` reservation (rows ON1, ON16, ON22), PLAT's OS random source (row ON19), MOD's recording and exit members, backend order, backup rules and the prologue site of the mixed source (row ON18, every part landed in MOD's design: tickets Z2T-MOD-011, Z2T-MOD-072, Z2T-MOD-080, Z2T-MOD-084; the second literal backup exclusion of ON22 (b) as amended joins Z2T-MOD-084), GAME's carry-over rule (row ON23), MOD's sink and context members (row ON5), SCRIPT's registry row (ON4) and the X278 replay mechanism, ASSET's kind (ON11).

### Stage 6

1. `ZenithOnline_SteamCore` (the logic over `Zenith_SteamApi`, the scripted seam) with `ZenithOnline_SteamCore_Tests` and the `[Steam]` conformance cases: a worker ticket whose evidence is the `Test` step on the Windows presets, needing no SDK.
2. `ZenithOnline_GooglePlayCore` (the logic over `Zenith_PlayBridge`, the scripted bridge) with `ZenithOnline_GooglePlayCore_Tests` and the `[GooglePlay]` conformance cases: a worker ticket whose evidence is the `Test` step on the Windows presets.
3. `needs-human`: the Steamworks partner account, the SDK download and its pin (with SHIP), the Steam app's achievements, stats and cloud quota.
4. `ZenithOnline_Steam` (the factory and the flat seam): a worker ticket blocked by ticket 3 and by core-doc item X363 (the declared-but-unavailable library state), written against the pinned header; no gate builds it, because the target is absent without the partner SDK, so the ticket's only gate evidence is that nothing else changes, and its behaviour is proven by the device run of step 8.
5. `needs-human`: the Play Console app, the PGS project with the signing certificate, achievements and in-app products, licence testers and two test accounts; the Maven pins (with SHIP).
6. `ZenithOnline_GooglePlay` (the factory, the JNI bridge, the Java class, `Pwsh_OnlinePlay_JavaBridgeMatchesJniTable`): a worker ticket blocked by ticket 5's Maven pins; its gate evidence is `CompileCoverage` on the Android presets and the JNI-table check; its behaviour is proven by the device run of step 8.
7. `Zenith_OnlinePackEntitlements` and its integration with PACK's service.
8. Device runs on Windows with Steam and on the S24 with Play (`needs-human`), which are the requirements' evidence for everything the scripted seams cannot show (the SDK calls, the JNI bridge, the store services), and the security hardening evidence's fuzz runs.

### Stages 5 and 7

Stage 5: no ONLINE work (PACK's entitlement header lands pulled forward; ONLINE's Stage 6 implementation is written against it). Stage 7: console platform services, gated on SDK access (Z2-D-071), add backends through the same interface; networked services belong to NET (Z2-D-073).

### API-only slices

Stage 4 slice 1 above; Stage 6's `Zenith_OnlinePackEntitlements` lands after PACK's `Zenith_EntitlementQuery.h` (declare first, implement after the callee).

## Decisions

### Settled decisions

| ID | Use in ONLINE |
|---|---|
| Z2-D-074 | the adapter API and offline implementation in Stage 4, Steam and Google Play backends in Stage 6 |
| Z2-D-024 | ONLINE is a static module; backends are static libraries, all linked, chosen by configuration |
| Z2-D-019, Z2-D-053 | online results reach simulation only as recorded publications at tick boundaries |
| Z2-D-038, Z2-D-048 | JSON authoring with migration; versioned binary formats; saves migrate forward (SAVE's import) |
| Z2-D-040, Z2-D-045, Z2-D-046, Z2-D-047 | ownership, results and asserts, no global state, contract blocks |
| Z2-D-071, Z2-D-073, Z2-D-075 | console services deferred; no game server; no modding |
| Z2-D-077 | achievement triggers are game-side Luau |
| Z2-D-079 | no visual authoring for online services |
| Z2-D-082 | third-party code pinned and confined to `Internal/` |

### Open decisions

- D-SEL-HASH (open, Stage 1): `Zenith_Hash64`, and therefore the account key's values; the key's derivation rule does not change with the selection.
- D-SEL-JSON (open, Stage 1): the JSON reader the cook step and the scripted seams use.

### Selection decisions ONLINE owns

None: the backends are settled by Z2-D-074, and ONLINE's design choices are recorded as elaborations in this family for review.

## Requirement index

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-ONLINE-001 | ZenithOnline module library | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Libraries ONLINE depends on"; `Zenith2/Docs/Design/ONLINE.md` → "Targets"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Header list" |
| Z2-R-ONLINE-002 | Offline backend library and factory | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Targets"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_Online_Offline header"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineOfflineTestAccess header" |
| Z2-R-ONLINE-003 | Public header rules across ONLINE | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Rules across the ONLINE API" |
| Z2-R-ONLINE-004 | Adapter interface Zenith_OnlineServices | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineServices header"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Backend completion payloads" |
| Z2-R-ONLINE-005 | Backend request completion rules | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules" |
| Z2-R-ONLINE-006 | Capability matrix honoured | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix" |
| Z2-R-ONLINE-007 | Module entry points | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_Module_Online header"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys" |
| Z2-R-ONLINE-008 | Module start sequence unwinds on failure | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Module start" |
| Z2-R-ONLINE-009 | Backend chosen by configuration with fallback | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend selection"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys" |
| Z2-R-ONLINE-010 | World attach and detach | S4 | `Zenith2/Docs/Design/ONLINE.md` → "World attach and detach"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSession header" |
| Z2-R-ONLINE-011 | Headless operation | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Frame-graph placement"; `Zenith2/Docs/Design/ONLINE/Tests.md` → "Integration suites" |
| Z2-R-ONLINE-012 | Replay mode is inert | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Replay mode" |
| Z2-R-ONLINE-013 | Offline backend implements every capability | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline backend"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix" |
| Z2-R-ONLINE-014 | Offline backend makes no network call | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline network isolation" |
| Z2-R-ONLINE-015 | Offline accounts and the local profile | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline accounts"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys" |
| Z2-R-ONLINE-016 | Offline achievement persistence | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline entitlements and achievements"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline achievement file" |
| Z2-R-ONLINE-017 | Offline cloud store | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline cloud store algorithm"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store" |
| Z2-R-ONLINE-018 | Offline test access | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineOfflineTestAccess header"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline latency and faults" |
| Z2-R-ONLINE-019 | Offline latency and construction failure | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline latency and faults" |
| Z2-R-ONLINE-020 | Account identifier format | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineTypes header" |
| Z2-R-ONLINE-021 | Account key keyed with the install secret | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineTypes header"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record" |
| Z2-R-ONLINE-022 | Display name is sanitised presentation data | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types" |
| Z2-R-ONLINE-023 | Session state machine | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine" |
| Z2-R-ONLINE-024 | Sign-in at start and on request | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Sign-in"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start" |
| Z2-R-ONLINE-025 | Failed silent sign-in keeps the last profile | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Sign-in" |
| Z2-R-ONLINE-026 | Account change detection posts a PLAT signal | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account change detection" |
| Z2-R-ONLINE-027 | Fence at detection | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind state per subsystem" |
| Z2-R-ONLINE-028 | Rebind completes in the delivering prologue | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps" |
| Z2-R-ONLINE-029 | Stale and duplicate signals | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps" |
| Z2-R-ONLINE-030 | Save profile derivation | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps" |
| Z2-R-ONLINE-031 | Busy profile switch retried every prologue | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry" |
| Z2-R-ONLINE-032 | Failing profile switch degrades to local | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry" |
| Z2-R-ONLINE-033 | Account change during a retry | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine" |
| Z2-R-ONLINE-034 | Previous-profile worlds cannot save into the new profile | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Worlds bound under the previous profile" |
| Z2-R-ONLINE-035 | IdentityChanged reaches every play world | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types" |
| Z2-R-ONLINE-036 | Results carry their generation | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script result payloads" |
| Z2-R-ONLINE-037 | Script requests across an account change | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing" |
| Z2-R-ONLINE-038 | Saves of one account invisible to another | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation" |
| Z2-R-ONLINE-039 | Cloud copies never install across accounts | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Download sequence"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud envelope" |
| Z2-R-ONLINE-040 | Entitlements never cross accounts | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement cache" |
| Z2-R-ONLINE-041 | Achievements never cross accounts | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations" |
| Z2-R-ONLINE-042 | No personal data in logs, counters, crash reports or replays | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE.md` → "Log categories and messages" |
| Z2-R-ONLINE-043 | Editor and preview worlds receive no notifications | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" |
| Z2-R-ONLINE-044 | Offline local data per account | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store" |
| Z2-R-ONLINE-045 | One thousand account switches leave nothing behind | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Stress tests" |
| Z2-R-ONLINE-046 | OS session signals do not rebind | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "OS session signals" |
| Z2-R-ONLINE-047 | Entitlement answers per catalogue entry | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement cache" |
| Z2-R-ONLINE-048 | Entitlement refresh | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement refresh" |
| Z2-R-ONLINE-049 | Entitlement revision | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement cache"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement refresh" |
| Z2-R-ONLINE-050 | Entitlements while signed out | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement refresh"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix" |
| Z2-R-ONLINE-051 | Package entitlement query | S6 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement query for packages"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations" |
| Z2-R-ONLINE-052 | Package service uses the online query | S6 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement query for packages"; `Zenith2/Docs/Design/ONLINE.md` → "Module start" |
| Z2-R-ONLINE-053 | Achievement definitions | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Online catalogue document"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineCatalogue header" |
| Z2-R-ONLINE-054 | Unlock is idempotent | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations" |
| Z2-R-ONLINE-055 | Achievement steps never decrease | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations" |
| Z2-R-ONLINE-056 | Achievement precondition statuses | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations" |
| Z2-R-ONLINE-057 | Achievements are not queued while signed out | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations" |
| Z2-R-ONLINE-058 | Achievement load and reload | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement synchronisation"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations" |
| Z2-R-ONLINE-059 | Cloud slot eligibility | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot eligibility" |
| Z2-R-ONLINE-060 | Content-hash identities and the fast path | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities" |
| Z2-R-ONLINE-061 | Three-way decision table | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table" |
| Z2-R-ONLINE-062 | Reconcile algorithm | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reconcile algorithm" |
| Z2-R-ONLINE-063 | Upload commits at the index write | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence" |
| Z2-R-ONLINE-064 | Download validates and imports through SAVE | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Download sequence" |
| Z2-R-ONLINE-065 | Deletion propagation and garbage collection | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Garbage collection" |
| Z2-R-ONLINE-066 | Conflict detection and persistence | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Conflict detection" |
| Z2-R-ONLINE-067 | Automatic resolution policies | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Automatic resolution policies" |
| Z2-R-ONLINE-068 | Conflict resolution choices | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Resolution choices" |
| Z2-R-ONLINE-069 | Keep-both naming | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Keep-both naming" |
| Z2-R-ONLINE-070 | Slot sync state machine | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot sync state machine" |
| Z2-R-ONLINE-071 | Reference conflict flow in the flagship slices | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reference conflict screen" |
| Z2-R-ONLINE-072 | Accessible conflict flow | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reference conflict screen"; `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs" |
| Z2-R-ONLINE-073 | Interrupted sync converges | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Interrupted sync recovery" |
| Z2-R-ONLINE-074 | One transfer at a time within the governor | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Transfer memory" |
| Z2-R-ONLINE-075 | Upload triggers and debounce | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Polling and triggers" |
| Z2-R-ONLINE-076 | Cloud status reporting | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting" |
| Z2-R-ONLINE-077 | Shutdown and background flush | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Shutdown and background flush" |
| Z2-R-ONLINE-078 | Cancelled transfers leave no partial state | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Stress tests"; `Zenith2/Docs/Design/ONLINE.md` → "Cancellation" |
| Z2-R-ONLINE-079 | Cloud envelope format | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud envelope"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names" |
| Z2-R-ONLINE-080 | Cloud index format | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index" |
| Z2-R-ONLINE-081 | Sync manifest format | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest" |
| Z2-R-ONLINE-082 | Device record | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record" |
| Z2-R-ONLINE-083 | Online catalogue authoring document | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Document schema"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Validation rules"; `Zenith2/Docs/Design/ONLINE.md` → "Cook steps" |
| Z2-R-ONLINE-084 | Catalogue document migration | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Migration chain"; `Zenith2/Docs/Design/ONLINE.md` → "Migration hooks" |
| Z2-R-ONLINE-085 | Cooked online catalogue | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cooked online catalogue"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineCatalogue header" |
| Z2-R-ONLINE-086 | Catalogue loaded at start with a safe fallback | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Module start" |
| Z2-R-ONLINE-087 | Online Luau namespace | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API" |
| Z2-R-ONLINE-088 | Script request payload encoding | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script request payloads" |
| Z2-R-ONLINE-089 | Script result payloads and decoder | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script result payloads" |
| Z2-R-ONLINE-090 | Notification payloads | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Notification payloads" |
| Z2-R-ONLINE-091 | Online events delivered in phase 9 | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" |
| Z2-R-ONLINE-092 | Online misuse codes | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Misuse codes" |
| Z2-R-ONLINE-093 | Rolled-back invocations request nothing | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request intake" |
| Z2-R-ONLINE-094 | Requests abandoned by a restore | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script result payloads"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API" |
| Z2-R-ONLINE-095 | Request timeouts are recorded | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Timeouts" |
| Z2-R-ONLINE-096 | Every accepted script request completes once | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Result completion" |
| Z2-R-ONLINE-097 | Online results reach simulation only as recorded publications | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Determinism obligations" |
| Z2-R-ONLINE-098 | Notification IDs allocated in the prologue for a recorded kind | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"; `Zenith2/Docs/Design/ONLINE.md` → "Determinism obligations" |
| Z2-R-ONLINE-099 | Replays agree across worker counts | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Determinism and replay tests" |
| Z2-R-ONLINE-100 | Per-world request inboxes | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Cross-thread hand-offs"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request intake" |
| Z2-R-ONLINE-101 | Notification queues and parking limits | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications" |
| Z2-R-ONLINE-102 | Thread affinity and frame placement | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Frame-graph placement"; `Zenith2/Docs/Design/ONLINE.md` → "Thread affinity per type"; `Zenith2/Docs/Design/ONLINE.md` → "Threads ONLINE owns" |
| Z2-R-ONLINE-103 | Online error enumeration | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineErrors header"; `Zenith2/Docs/Design/ONLINE.md` → "Error enumerations" |
| Z2-R-ONLINE-104 | Online counters | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Counters" |
| Z2-R-ONLINE-105 | Online log categories and messages | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Log categories and messages" |
| Z2-R-ONLINE-106 | Online profiling zones | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Profiling zones" |
| Z2-R-ONLINE-107 | Online console commands | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Console commands" |
| Z2-R-ONLINE-108 | Online overlay panel | S4 | `Zenith2/Docs/Design/ONLINE.md` → "On-screen profiler rows and panel" |
| Z2-R-ONLINE-109 | Editor online panel | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Editor surfaces" |
| Z2-R-ONLINE-110 | Memory consumer and shedding | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Memory and the governor" |
| Z2-R-ONLINE-111 | Session construction unwinds | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSession header" |
| Z2-R-ONLINE-112 | Session shutdown drains | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Startup and shutdown" |
| Z2-R-ONLINE-113 | Backend data validated as untrusted | S4 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules"; `Zenith2/Docs/Design/ONLINE.md` → "Security assumptions" |
| Z2-R-ONLINE-114 | ONLINE parsers are fuzzed | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Format and fuzz tests"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" |
| Z2-R-ONLINE-115 | No credentials or tokens held | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Security assumptions"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access" |
| Z2-R-ONLINE-116 | Development seams refused in Shipping | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam API seam"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play bridge seam" |
| Z2-R-ONLINE-117 | Steam backend library and SDK provisioning | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steamworks SDK provisioning"; `Zenith2/Docs/Design/ONLINE.md` → "Targets" |
| Z2-R-ONLINE-118 | Steam API seam | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam API seam" |
| Z2-R-ONLINE-119 | Steam initialisation, pumping and shutdown | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam initialisation and pumping" |
| Z2-R-ONLINE-120 | Steam identity | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam identity" |
| Z2-R-ONLINE-121 | Steam entitlements | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam entitlements" |
| Z2-R-ONLINE-122 | Steam achievements and progress stats | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam achievements" |
| Z2-R-ONLINE-123 | Steam cloud storage | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam cloud storage" |
| Z2-R-ONLINE-124 | Steam packaging | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam packaging" |
| Z2-R-ONLINE-125 | Steam device acceptance | S6 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access" |
| Z2-R-ONLINE-126 | Google Play backend library and Gradle wiring | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play dependencies and Gradle wiring"; `Zenith2/Docs/Design/ONLINE.md` → "Targets" |
| Z2-R-ONLINE-127 | Play Java bridge contract | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Java bridge" |
| Z2-R-ONLINE-128 | Play identity and resume detection | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play identity" |
| Z2-R-ONLINE-129 | Play entitlements through Play Billing | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play entitlements through Play Billing" |
| Z2-R-ONLINE-130 | Play achievements | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play achievements" |
| Z2-R-ONLINE-131 | Play saved games | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play saved games" |
| Z2-R-ONLINE-132 | Play bridge seam | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play bridge seam" |
| Z2-R-ONLINE-133 | Play event queue bound | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Java bridge"; `Zenith2/Docs/Design/ONLINE.md` → "Lock-order entries" |
| Z2-R-ONLINE-134 | Play device acceptance | S6 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs" |
| Z2-R-ONLINE-135 | Partner configuration is human work | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access" |
| Z2-R-ONLINE-136 | Backend conformance suite | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Conformance suite" |
| Z2-R-ONLINE-137 | Account switch end to end | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Integration suites"; `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration" |
| Z2-R-ONLINE-138 | Offline device run on the S24 | S4 | `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs" |
| Z2-R-ONLINE-139 | Online budget rows allocated | S1 | `Zenith2/Docs/Design/ONLINE.md` → "Pending row needs" |
| Z2-R-ONLINE-140 | Online asynchronous capacity per world | S4 | `Zenith2/Docs/Design/ONLINE.md` → "Rows ONLINE is measured against"; `Zenith2/Docs/Design/ONLINE.md` → "Asynchronous source and misuse block" |
| Z2-R-ONLINE-141 | The bind is a second generation boundary | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind state per subsystem"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution" |
| Z2-R-ONLINE-142 | Unreadable cloud index is unknown, never empty | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index" |
| Z2-R-ONLINE-143 | Committed index entries always reference existing bytes | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Garbage collection"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names" |
| Z2-R-ONLINE-144 | Local deletions propagate only from SAVE's deletion listener | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest" |
| Z2-R-ONLINE-145 | Backup-recovered slots never overwrite the cloud | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot eligibility"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table" |
| Z2-R-ONLINE-146 | Display names never enter a replay log | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys" |
| Z2-R-ONLINE-147 | An unverified identity fences its window | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play identity" |
| Z2-R-ONLINE-148 | Transfer buffers outlive the requests that may write them | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Transfer memory"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules"; `Zenith2/Docs/Design/ONLINE.md` → "Cancellation" |
| Z2-R-ONLINE-149 | Previous-profile worlds receive no online service for the new account | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Worlds bound under the previous profile"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution" |
| Z2-R-ONLINE-150 | Save intake frozen while a profile switch waits | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps" |
| Z2-R-ONLINE-151 | Cloud index repaired only on request | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index repair"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API" |
| Z2-R-ONLINE-152 | Store backend logic is gate-tested without the SDK or a device | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Store backend libraries and what the gates prove"; `Zenith2/Docs/Design/ONLINE.md` → "Targets"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSteamCore header"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineGooglePlayCore header" |
| Z2-R-ONLINE-153 | Steam remote storage never transfers on the main thread | S6 | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam cloud storage"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam initialisation and pumping" |
| Z2-R-ONLINE-154 | A late profile switch notifies every play world | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting" |
| Z2-R-ONLINE-155 | A local save is deleted only by a deletion record | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index" |
| Z2-R-ONLINE-156 | Lost and regressed commits are re-uploaded | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud-save model overview"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play saved games" |
| Z2-R-ONLINE-157 | ONLINE's files obey PLAT's user-mount namespace | S4 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Conventions for every ONLINE format"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest" |
| Z2-R-ONLINE-158 | New-account notifications never reach a previous-profile world | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation" |
| Z2-R-ONLINE-159 | No carry-over across a profile change | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration" |
| Z2-R-ONLINE-160 | Bind at start | S4 | `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine"; `Zenith2/Docs/Design/ONLINE.md` → "Module start" |
| Z2-R-ONLINE-161 | Divergent cloud histories conflict | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index" |
| Z2-R-ONLINE-162 | A live cloud entry is never removed for capacity | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting" |
| Z2-R-ONLINE-163 | The SAVE slot listener lives exactly as long as the session needs it | S4 | `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations"; `Zenith2/Docs/Design/ONLINE.md` → "Module start"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation" |
| Z2-R-ONLINE-164 | An index repair never exposes a pre-repair base to a silent overwrite | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index repair"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Commit tokens and the commit log" |
| Z2-R-ONLINE-165 | A conflict resolution acts only on the inputs the player saw | S4 | `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Resolution choices"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Conflict detection"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot sync state machine"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Notification payloads" |

## Cross-area notes

Rows ON1 … ON23 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` list what ONLINE needs from, or answers to, SAVE, PACK, SCRIPT, MOD, ARCH, UI, GAME, TWOD, SHIP, ASSET, EDIT, CONTRACT, PLAT and SEC (ON16 … ON21 from revision 1, ON22 and ON23 from revision 2: SAVE's intake freeze, deletion listener and log wording; the PACK reply withdrawing ON3 (a); the MOD members, backend order and backup rule; PLAT's OS random source and key wording; EDIT's owner row; the flagship cloud-status flow); each assumption block of this family names its row. Core-doc items X320 (the misuse block), X321 (the budget rows), X322 (Architecture: the library rows, the trust boundary and the parser limits), X323 (Contracts: the lock row), X324 (Scripting: the source-registry row), X325 (CompositionRoot: the start-context members and the account-consumer timing), X336 (Contracts: the catalogue document type), X337 (Scripting: the mixed source kind), X338 (Architecture: [Z2-N-ARCH-268]'s wording), X358 (Architecture: the core libraries, layer 9 and the revised parser limits) and X359 (Contracts: the corrected fixture path of X336) and X363 (Architecture, BuildSystem, Contracts: the core-library dependence, the `KIND` and the declared-but-unavailable state, filed by the round-2 review) queue the core-document halves.
