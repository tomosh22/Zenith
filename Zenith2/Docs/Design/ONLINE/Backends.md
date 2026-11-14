# ONLINE — backends

## Scope of this file

This file is part of the ONLINE design family; the entry document is `Zenith2/Docs/Design/ONLINE.md`. It specifies the contract every `Zenith_OnlineServices` implementation obeys, the capability matrix, the offline backend (Stage 4), the Steam and Google Play backends (Stage 6, Z2-D-074) with the store-independent logic libraries their gate tests run against, the seams those tests drive, what needs a human with partner access, and how the module chooses a backend. The interface is `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineServices header"; payloads are `Zenith2/Docs/Design/ONLINE/Formats.md` → "Backend completion payloads".

## Backend contract

### Request completion rules

1. Every member is called on the main thread; a backend whose SDK calls back on another thread (Play's Java callbacks on the Android UI thread) copies each callback's data into a bounded internal queue and delivers it only from `Pump`.
2. `Submit` either refuses (returning an error; nothing is delivered) or accepts; an accepted request is completed exactly once through `OnRequestCompleted` from a subsequent `Pump` (never from inside `Submit`, so the session's tables are consistent when completions arrive).
3. `Cancel` of an accepted, uncompleted request makes it complete `ONLINE_STATUS_CANCELLED` unless it has already finished, in which case it completes with its real outcome ([Z2-N-CONTRACT-337]); a cancelled request releases what it holds before its completion is delivered ([Z2-N-CONTRACT-335]), and in particular never writes the request's `m_xReadBuffer` after its completion has been delivered: the session frees that buffer only after the completion (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Transfer memory").
4. Completions are delivered in the order the backend finished them; the session never relies on submission order.
5. `Shutdown` completes every outstanding request `ONLINE_STATUS_SHUTTING_DOWN`, flushes for at most its deadline, and stops the SDK.
6. Revisions: offline, SHA-256 of the object file's header (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store"); Steam, SHA-256 of (the remote file name, its size as `uint64`, its `GetFileTimestamp` as `int64`, little-endian); Play, SHA-256 of (the snapshot's unique name, its last-modified timestamp in milliseconds as `int64`, its played time in milliseconds as `int64`, its content length as `uint64`). Every derivation changes whenever the stored object changes. An absent object's revision is none (all zero) on every backend.
7. A conditional write or delete (`m_bHasExpectedRevision`) completes `ONLINE_STATUS_REVISION_MISMATCH` when the object's current revision differs, and writes nothing; an expected revision of none is a create-only write, which mismatches whenever the object exists.
8. Untrusted data: everything a backend receives from its SDK or service (identifiers, names, ownership answers, achievement states, object bytes) is validated against the limits of `Zenith2/Docs/Design/ONLINE/Formats.md` before it is delivered; an invalid value is a `BACKEND_ERROR` completion or a dropped event, counted `online.backend.bad_data`, never an assert.
9. No backend logs an account identifier, a display name, a token or a purchase token; the backends log native error codes, request kinds and counts (`Zenith2/Docs/Design/ONLINE.md` → "Security assumptions").
10. Identity: a backend that cannot vouch for its current player (the Play backend between a resume and its re-read of the player) reports `ONLINE_IDENTITY_STATE_SIGNING_IN` through `OnIdentityChanged` and `GetIdentity()` until it can (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification").

### Capability matrix

| Capability bit | Offline | Steam | Google Play |
|---|---|---|---|
| `uONLINE_CAPABILITY_IDENTITY` | yes | yes | yes |
| `uONLINE_CAPABILITY_ENTITLEMENTS` | yes | yes (`BIsSubscribedApp`) | yes (Play Billing) |
| `uONLINE_CAPABILITY_ENTITLEMENTS_WITHOUT_SIGN_IN` | no | no (the process always has a Steam user) | yes (ownership is the device's Play Store account) |
| `uONLINE_CAPABILITY_ACHIEVEMENTS` | yes | yes | yes |
| `uONLINE_CAPABILITY_INCREMENTAL_ACHIEVEMENTS` | yes | yes (an INT stat per achievement) | yes (native incremental achievements) |
| `uONLINE_CAPABILITY_CLOUD` | when `online.offline.cloudRoot` is set | when `IsCloudEnabledForApp` | yes (Saved Games) |
| `uONLINE_CAPABILITY_CLOUD_CONDITIONAL_WRITE` | yes | yes (checked against the session's view; § "Steam cloud storage") | yes (§ "Play saved games") |
| `uONLINE_CAPABILITY_INTERACTIVE_SIGN_IN` | no | no | yes |
| `uONLINE_CAPABILITY_SIGN_OUT` | yes (development and tests) | no | no (PGS v2 has no sign-out) |
| `uONLINE_CAPABILITY_SWITCH_ACCOUNT` | yes (development and tests) | no | no |
| `GetMaxCloudObjectBytes()` | `ulONLINE_MAX_CLOUD_OBJECT_BYTES` | 104,857,600 (Steam's per-file write limit, `k_unMaxCloudFileChunkSize`), lowered to the quota's available bytes | the value `getMaxDataSize()` reported at start (3 MiB on current Play services) |

### Store backend libraries and what the gates prove

Each store backend is two libraries, so that everything except the SDK calls themselves is built and tested on the host presets the gates run, without the partner SDK and without a device:

| Library | Kind | Layer | `ZENITH_SUPPORTED` | Holds | Gate evidence |
|---|---|---|---|---|---|
| `ZenithOnline_SteamCore` | `KIND ENGINE` (assumption pending ARCH, X363 (b)) | 8 | `ZENITH_WINDOWS` | the Steam backend's logic over the `Zenith_SteamApi` seam, and the scripted (recording) seam implementation; no SDK header | `ZenithOnline_SteamCore_Tests` and the `[Steam]` conformance cases in the `Test` step of the Windows presets |
| `ZenithOnline_Steam` | backend (`BACKEND_OF ZenithOnline`, `BACKEND_NAME Steam`) | 9 | `ZENITH_WINDOWS`, declared with `THIRD_PARTY steamworks-sdk`; a declared-but-unavailable library while the archive is absent (assumption pending ARCH, X363 (c)) | the factory and the flat-API seam implementation `Internal/Windows/Zenith_SteamApiFlat_Windows.cpp` | **none**: without the partner SDK the target is absent, so no gate builds, links or tests it and no requirement claims a gate step for it; its evidence is the `needs-human` run `Device_OnlineSteam_Windows` (`Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs") |
| `ZenithOnline_GooglePlayCore` | `KIND ENGINE` (assumption pending ARCH, X363 (b)) | 8 | `ZENITH_GPU` (Windows and Android) | the Play backend's logic over the `Zenith_PlayBridge` seam, and the scripted (recording) bridge; no JNI or Java | `ZenithOnline_GooglePlayCore_Tests` and the `[GooglePlay]` conformance cases in the `Test` step of the Windows presets (on Android the executables are built but never registered with CTest, [Z2-N-ARCH-180]) |
| `ZenithOnline_GooglePlay` | backend (`BACKEND_OF ZenithOnline`, `BACKEND_NAME GooglePlay`) | 9 | `ZENITH_ANDROID` | the factory, the JNI bridge `Internal/Android/Zenith_PlayBridgeJni_Android.cpp` and the Java class | `CompileCoverage` on the Android presets, and `Pwsh_OnlinePlay_JavaBridgeMatchesJniTable` in the `Test` step of `win`, registered by the test registry itself (below), not by this Android-only library; its run-time behaviour is proven only by the `needs-human` run `Device_OnlinePlay_S24` |

<!-- assumption: pending ARCH (core-doc item X363) -->
Three core rules do not yet admit this split, and core-doc item X363 asks for each: (a) [Z2-N-ARCH-075] lets a backend depend only on its API library and libraries at or below the API's layer, while each store backend (layer 9) depends on its core library (layer 8), above `ZenithOnline`'s layer 7; X363 (a) admits one engine library of the backend's own family that depends only on the family's API and libraries at or below it, sits strictly between the two layers, is never a backend, and reaches a game only through that backend's `DEPENDS`. (b) `Zenith2/Docs/Architecture/BuildSystem.md` → "Signature" has no `KIND LIBRARY`; the core libraries declare `KIND ENGINE` (the gloss "mandatory engine library" reworded by X363 (b)), and step 1 of "Target creation steps" accepts that value today. (c) A partner-gated library: `ZenithOnline_Steam`'s `CMakeLists.txt` is one unconditional call, `zenith_add_library(ZenithOnline_Steam KIND BACKEND BACKEND_OF ZenithOnline BACKEND_NAME Steam SUPPORTED ZENITH_WINDOWS DEPENDS ZenithOnline_SteamCore ZenithOnline ZenithPlatform ZenithCore THIRD_PARTY steamworks-sdk)`, with no `if` around it; when the `steamworks-sdk` manual-download archive is absent from the dependency cache the helper records the declaration with `unavailable: "steamworks-sdk"` in `declared_deps.json` and creates no target, `CompileCoverage` and `SourceLayout` report the library as declared-but-unavailable rather than failing (the record names it, so nothing is silently filtered out), and MOD's backend resolution lists only backends whose target exists. Until X363 lands, a configure without the archive would red `CompileCoverage` for `ZenithOnline_Steam`; the Stage 6 ticket that adds the library is therefore blocked on X363 as well as on the SDK pin.
<!-- /assumption -->

The core libraries are engine libraries, not backends, so MOD's backend resolution (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend resolution", which lists libraries with `BACKEND_OF ZenithOnline`) never links one into a game on its own; a game links a core library only through its backend's `DEPENDS`. Their public root headers are `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSteamCore header" and "Zenith_OnlineGooglePlayCore header". The scripted seams are recording implementations selected by configuration, refused in Shipping builds (FINAL-DOUBLES); they prove the backends' logic, never the store's behaviour, which only the needs-human runs observe.

## Offline backend

`ZenithOnline_Offline` (`BACKEND_OF ZenithOnline`, `BACKEND_NAME Offline`, SUPPORTED `ZENITH_GPU`, the presets of `ZenithRuntime`, as `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping" lists it) is the required offline implementation of Z2-D-074: a complete, shippable backend for games without a store, and the backend every automated test of the session uses.

### Offline accounts

- The accounts are `online.offline.accounts` (default `["local"]`), each an account ID `offline:<name>`; `offline:local` maps to SAVE's local profile (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"). Each account's files live under its store key (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline achievement file").
- At creation the backend is SIGNED_IN as `online.offline.activeAccount` (default the first) when `online.offline.autoSignIn` is true, else SIGNED_OUT; `ONLINE_REQUEST_SIGN_IN_SILENT` signs the active account in.
- `ONLINE_REQUEST_SIGN_OUT` and `ONLINE_REQUEST_SWITCH_ACCOUNT` (development console commands, `Zenith2/Docs/Design/ONLINE.md` → "Console commands") and the test access's `SignIn` and `SignOut` change the identity; the next `Pump` delivers `OnIdentityChanged` first, then the request's completion.
- The display name is the account name. `m_bConnected` is true unless the test access set it false.
- **Requests are bound to their account.** `Submit` records the account that is signed in at that moment in the request's record, and every effect of the request (an achievement change, a cloud object write or delete, the in-memory table it updates) applies to that account's state alone, whatever account is signed in when it completes. A completion delivered after `SignIn(bob)` for a request submitted as alice therefore updates alice's achievement memory and alice's cloud table, never bob's, so a stale index revision can never appear in bob's table.

### Offline entitlements and achievements

- Entitlements: GRANTED for the catalogue's `offline.granted` entitlements and for every ID of `online.offline.grantedEntitlements`, plus every grant the test access set for the account; DENIED otherwise; signed out, every entitlement is DENIED. Answers are computed at `Submit` for the request's account and delivered at the next `Pump` (after `m_uLatencyFrames` frames).
- Achievements: read from `user:/saves/online/offline/<store key>/achievements.zoac` at the account's first sign-in of the process (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline achievement file") into that account's memory, which the backend keeps for every configured account it has loaded; unlock and set steps update the request's account's memory and rewrite that account's file with `WriteAtomic` (one write in flight per account; a change while it runs marks the file dirty and writes once more after it); the completion is delivered when the write completes, and an I/O failure completes `BACKEND_ERROR` with `ONLINE_ERROR_IO` while memory keeps the change, so the next successful write persists it.

### Offline cloud store algorithm

With `online.offline.cloudRoot` set, the backend keeps a per-account object directory (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store"):

1. At the account's first sign-in of the process: `CreateDirectory` of `saves/online/cloud/<cloud root>/<store key>/` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Other user-mount operations"); list its files with PLAT's user-mount listing; read every file's 64-byte header; build that account's in-memory table { name, bytes, revision, serial, UTC from the file's modification time }; the next serial is the maximum plus 1.
2. CLOUD_LIST: the request's account's table, delivered after the latency.
3. CLOUD_READ: read the whole file of the request's account; verify rules 1 … 7 (a failure is `BACKEND_ERROR` with `ONLINE_ERROR_CORRUPT`); deliver the B bytes and the revision. A cancelled read releases its `Zenith_AsyncIO` read before the `CANCELLED` completion is delivered (the backend reads into its own buffer and copies into `m_xReadBuffer` only at delivery, so the session's buffer is never written after a cancellation).
4. CLOUD_WRITE: when `m_bHasExpectedRevision`, compare with the table's revision (none for an absent object) and complete `REVISION_MISMATCH` on a difference; build the header with the next serial and the object's SHA-256; `WriteAtomic` header and bytes as one file; on completion update the request's account's table and deliver the new revision.
5. CLOUD_DELETE: the same revision check; `Delete`; on completion remove the entry from the request's account's table.
6. Requests of one object are serialised in submission order; requests of different objects may complete in any order.
7. The test access's `WriteCloudAsOtherDevice` runs steps 4 or 5 synchronously-in-effect (its writes are queued ahead of the session's next request on that object) and reports `ONLINE_INVALIDATION_CLOUD` at the next `Pump`; `WriteRawCloudObject` writes arbitrary bytes under any name (a corrupt, newer-version or foreign index for the tests of `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view").

### Offline latency and faults

- `m_uLatencyFrames` (`online.offline.latencyFrames`, or the test access's `SetLatencyFrames`) delays every completion by that many `Pump` calls; 0 delivers at the next `Pump`.
- `FailNext(kind, status, count)` makes the next `count` accepted requests of that kind complete with that status and change nothing.
- `HoldCompletions(kind, true)` keeps every completion of that kind queued until released, so a test can place an account change between a request's acceptance and its completion.
- `SetUnverifiedIdentity(true)` makes the backend report SIGNING_IN for its signed-in account until cleared, the offline form of Play's resume (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification").
- `xDesc.m_pxFaults` (CORE's `Zenith_FaultInjector`) can fail each construction step of the factory (`Test_OnlineOffline_Create_FailsAt<Step>_Unwinds`) and each user-mount operation.

### Offline network isolation

The offline backend and `ZenithOnline` itself make no network call: no socket, no HTTP, no DNS, no platform store service, directly or through ZenithPlatform's one network-capable API (the loopback development listener, `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket"). Their sources are platform-independent (neither library has an `Internal/Windows/` or `Internal/Android/` directory), so what the check below proves on the Windows host presets holds for the same sources on Android.

The check is the CTest `CMake_OnlineOffline_NoNetworkImports`: the script `Zenith2/Engine/ZenithOnline_Offline/Tests/Checks/Zenith_OnlineNoNetwork.cmake`, run as `cmake -P` and registered by the test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` with label `z2-cmake` in the host presets that build both libraries (`win`, `win-asan`; ZenithOnline is not built in `linux-tsan`), as SCRIPT's `Zenith_ScriptLuauLinkage.cmake` is registered. It:

1. Runs `llvm-nm --undefined-only --format=just-symbols` over the `ZenithOnline` and `ZenithOnline_Offline` archives of the preset, and again with `--demangle`. A missing `llvm-nm` (the toolchain's, found through `CMAKE_NM` or beside `CMAKE_CXX_COMPILER`) prints `online.no_network: llvm-nm not found` and fails; the check never skips.
2. Normalises each undecorated symbol: strips a leading `__imp_` (an import thunk) and, on a 32-bit decoration, a leading `_`; removes a trailing `@<n>` stdcall suffix.
3. Fails on any normalised symbol equal to an exact entry of the committed list `Zenith2/Engine/ZenithOnline_Offline/Tests/network_symbols.txt`, or beginning with one of its prefix entries (lines ending `*`). The list: `socket`, `connect`, `bind`, `listen`, `accept`, `send`, `recv`, `sendto`, `recvfrom`, `select`, `poll`, `shutdown`, `closesocket`, `ioctlsocket`, `getaddrinfo`, `freeaddrinfo`, `getnameinfo`, `gethostbyname`, `gethostbyaddr`, `inet_pton`, `WSA*`, `GetAddrInfo*`, `FreeAddrInfo*`, `Internet*`, `HttpOpenRequest*`, `HttpSendRequest*`, `WinHttp*`, `URLDownloadTo*`, `URLOpen*`, `Dns*`, `curl_*`, `SDLNet_*`.
4. Fails on any demangled symbol containing `Zenith_DevListener`, `Zenith_DevConnection` or `Zenith_DevSocket`, so neither library can reach the network through ZenithPlatform.
5. Prints each offending symbol with its archive and member, then exits 1; otherwise prints `online.no_network: ok (<n> symbols checked)`.

What it does not prove, stated so no requirement claims it: a network call made by another dependency library on ONLINE's behalf (none of ZenithCore, ZenithSave, ZenithScript, ZenithReflect, ZenithECS, ZenithAssets, ZenithProfiling or ZenithJobs makes one, by their own designs) and a call made through a function pointer obtained at run time (ONLINE loads no library, Z2-D-024).

## Steam backend

The Steam backend (Stage 6) implements the adapter over the Steamworks SDK's flat C API: `ZenithOnline_SteamCore` holds its logic over `Zenith_SteamApi`, `ZenithOnline_Steam` the factory and the SDK calls (§ "Store backend libraries and what the gates prove").

### Steamworks SDK provisioning

<!-- assumption: pending SHIP (row ON9) -->
- The SDK is pinned in `Zenith2/ThirdParty/manifest.json` as the entry `steamworks-sdk`, integration mode `prebuilt` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Integration modes in detail"): the include directory `public/steam`, the import library `redistributable_bin/win64/steam_api64.lib`, and the runtime file `redistributable_bin/win64/steam_api64.dll` (`ZENITH_TP_STEAMWORKS_SDK_RUNTIME_FILES`), with the SHA-256 of the SDK zip.
- The SDK can be downloaded only by a logged-in Steamworks partner, so the entry carries a manual-download field: the setup script verifies a zip a human placed in the dependency cache's download directory and never downloads it; without it the setup reports the entry missing and the configure creates no `ZenithOnline_Steam` target (the backend list then holds Offline alone, exactly as in Stage 4). `ZenithOnline_SteamCore` and its tests are configured either way.
- The licence term is the Steamworks SDK Access Agreement; admitting it is SHIP's licence ruling (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Licence fields"), a `needs-human` ticket.
<!-- /assumption -->

- The flat API headers (`steam_api_flat.h`, `steam_api.h`) are included only from `ZenithOnline_Steam/Internal/Windows/`; no Steam type reaches any header outside it ([Z2-N-CONTRACT-043]), and `ZenithOnline_SteamCore` includes none.
- Inside `Internal/Windows/Zenith_SteamApiFlat_Windows.cpp` every flat-API call keeps Steamworks' own types (`AppId_t`, `SteamAPICall_t`, `HSteamPipe`, `EResult`, `uint64_steamid`), converted to and from the engine integer types of `Zenith_SteamApi` at the call. `steamtypes.h` declares its own `uint8` … `uint64` typedefs; they name the same fundamental types as the aliases of `Zenith_Types.h` on the MSVC x64 ABI, so both headers compile in that one file.
- `steam_api64.dll` is loaded by the OS loader through the import library at process start, not through `LoadLibrary` ([Z2-N-ARCH-262] stands); the Windows package places it beside the executable (row ON9 to SHIP).

### Steam API seam

<!-- elaboration -->
Every SDK call goes through the interface `Zenith_SteamApi`, declared in `ZenithOnline_SteamCore`'s public root header (`Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSteamCore header"), so the backend's logic is built and tested without the SDK or a Steam client. Remote-storage transfers use the SDK's asynchronous calls, whose call results arrive through `RunFrame` (§ "Steam cloud storage"); the seam has no synchronous file read or write.

| Implementation | File | Library | Selected when |
|---|---|---|---|
| `Zenith_SteamApiFlat` | `ZenithOnline_Steam/Internal/Windows/Zenith_SteamApiFlat_Windows.cpp` (calls `SteamAPI_InitFlat`, `SteamAPI_RestartAppIfNecessary`, `SteamAPI_ManualDispatch_Init`, `SteamAPI_GetHSteamPipe`, `SteamAPI_ManualDispatch_RunFrame`, `SteamAPI_ManualDispatch_GetNextCallback`, `SteamAPI_ManualDispatch_FreeLastCallback`, `SteamAPI_ManualDispatch_GetAPICallResult`, `SteamAPI_Shutdown` and the `SteamAPI_ISteamUser_*`, `SteamAPI_ISteamFriends_GetPersonaName`, `SteamAPI_ISteamApps_BIsSubscribedApp`, `SteamAPI_ISteamUserStats_*`, and the `SteamAPI_ISteamRemoteStorage_*` functions named in § "Steam cloud storage", of the pinned flat header) | `ZenithOnline_Steam` | `online.steam.scriptedApi` is empty |
| `Zenith_SteamApiScripted` | `ZenithOnline_SteamCore/Internal/Zenith_SteamApiScripted.cpp` (no SDK header), created by `Zenith_CreateSteamApiScripted` | `ZenithOnline_SteamCore` | `online.steam.scriptedApi` names a JSON script (the backend), or a test creates it directly; refused in Shipping (`ONLINE_ERROR_INVALID_DESC`) |

The scripted implementation reads its script once at creation (CORE's JSON reader, the JSON parser limits): an initial state `{ "steamId", "persona", "loggedOn", "subscribedApps": [], "achievements": { name: bool }, "stats": { name: int }, "cloudEnabledForApp", "cloudEnabledForAccount", "quota": { "total", "available" }, "files": { name: base64 }, "asyncLatencyFrames": n }` and a timeline `[{ "frame": n, "event": "loggedOn" | "loggedOff" | "dlcInstalled" | "shutdown" | "failNextFileWrite" | "failNextFileRead" | "replaceFiles", "appId"?, "files"? }]` applied at the matching `RunFrame` call (`replaceFiles` replaces the named remote-storage files with the given bytes, the scripted form of Steam keeping another machine's version at launch). `FileWriteAsync` and `FileReadAsync` return a fresh non-zero call handle and deliver their call results through the `RunFrame` that is `asyncLatencyFrames` (default 1) calls away; `FileReadAsyncComplete` copies from the scripted file into the caller's span. It records every mutating call in memory as `{ kind, name, value }` for the test to compare (`Zenith_OnlineSteamCoreTestAccess::GetRecordedCalls`; `Zenith2/Docs/Design/ONLINE/Tests.md` → "Steam backend tests"). It is a recording implementation selected by configuration (FINAL-DOUBLES).

### Steam initialisation and pumping

`Zenith_ModuleStartContext::RequestExitAfterStart()` (MAIN_THREAD, during `Start` only): the runtime completes startup, runs no frame, and exits through the normal shutdown order with exit code 0 (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Start context members for ONLINE and PACK", row ON18 (b), landed).

1. The factory calls `Init(online.steam.appId, online.steam.restartIfNecessary, bMustExit)`. With `restartIfNecessary` and an app launched outside Steam, `SteamAPI_RestartAppIfNecessary` returns true: the factory returns `ONLINE_ERROR_BACKEND_UNAVAILABLE` with native code −1 and logs `online.steam: relaunching through Steam`; `Zenith_Module_Online_Start`, seeing native code −1 from a factory, calls `RequestExitAfterStart()` and continues with the next candidate, so `Start` still succeeds and the process exits cleanly before its first frame while Steam relaunches it, as Steam's documentation requires.
2. `SteamAPI_InitFlat` not returning `k_ESteamAPIInitResult_OK` (Steam not running, no app ID, version mismatch) is `ONLINE_ERROR_BACKEND_UNAVAILABLE` with the result as native code; the module falls back to the next candidate (§ "Backend selection").
3. Manual dispatch: `SteamAPI_ManualDispatch_Init` once; every `Pump` runs one `RunFrame`, which calls `SteamAPI_ManualDispatch_RunFrame(pipe)` and drains `GetNextCallback`, handling `SteamShutdown_t` (identity SIGNED_OUT, then the process is expected to quit), `SteamServersConnected_t` / `SteamServersDisconnected_t` (connectivity invalidation), `DlcInstalled_t` (entitlement invalidation), `UserStatsReceived_t` for the local user (the achievement load's completion), `UserStatsStored_t`, and `SteamAPICallCompleted_t`, whose call result is fetched with `SteamAPI_ManualDispatch_GetAPICallResult` into a stack buffer of the expected struct's size and handed to the sink (`OnFileWriteAsyncComplete`, `OnFileReadAsyncComplete`); a callback is freed with `FreeLastCallback` before the next is read. No Steam callback runs on another thread, and every completion the backend delivers is delivered from `Pump`.
4. `StoreStats` is called at most once every 5 s when a stat or achievement changed, and once at `Shutdown`; `Shutdown` then waits, pumping `RunFrame`, for the call results of every asynchronous write in flight, bounded by its deadline, and calls `SteamAPI_Shutdown`. Every upload whose `FileWriteAsync` call result arrived OK before shutdown is in Steam's local cloud cache, which the Steam client uploads after the game exits; a write still in flight at the deadline completes `SHUTTING_DOWN` and was never committed (its U6 never ran).

### Steam identity

- Native ID: the 64-bit SteamID in decimal (`GetSteamID`); display name: `GetPersonaName`; `m_bConnected`: `IsLoggedOn`.
- The identity is SIGNED_IN from a successful `Init` until `SteamShutdown_t`; Steam does not change its user while a game runs, so a mid-session switch never comes from Steam; a sign-out is the Steam client shutting down. The session still handles both through the common path.
- Sign-in requests complete OK at the next `Pump` (already signed in); interactive sign-in, sign-out and switch are `NOT_SUPPORTED`.

### Steam entitlements

`QUERY_ENTITLEMENTS` answers each app ID with `IsSubscribedApp` (GRANTED or DENIED), computed at `Submit` and delivered at the next `Pump`; an ID that is not decimal digits of a `uint32` is `BACKEND_ERROR`. `DlcInstalled_t` raises `ONLINE_INVALIDATION_ENTITLEMENTS`. Ownership is never cached by the backend.

### Steam achievements

- Load: `QUERY_ACHIEVEMENTS` completes after the first `UserStatsReceived_t` for the local user (or at the next `Pump` when already received), reading `GetAchievement` per name and `GetStatInt32` per progress stat.
- Unlock: `SetAchievement`, then the deferred `StoreStats`; completes OK with `alreadyUnlocked` from the prior `GetAchievement`.
- Set steps (n, total): when n is greater than the stat's value, `SetStatInt32(progressStat, n)`; when n ≥ total, `SetAchievement`; otherwise `IndicateAchievementProgress(name, n, total)` only when n crosses a multiple of ⌈total ÷ 10⌉ (Steam shows a progress pop-up for each call); completes with the stored steps.
- A Steam failure return (`false`) is `BACKEND_ERROR`.
- The Steamworks app configuration (achievement API names, the INT progress stats, their maxima) is made by a human in the Steamworks partner site and must match the catalogue; `Test_OnlineSteam_CatalogueMatchesScript` checks the catalogue against the scripted state, and the device run checks it against the real app (§ "Credentials and partner access").

### Steam cloud storage

- Names: `zenith2/<object name>` (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names").
- CLOUD_LIST: at `Submit`, `GetFileCount` and `GetFileNameAndSize` filtered to the `zenith2/` prefix and `GetFileTimestamp` per file (metadata reads of the client's in-memory file table; no file content), the revision per § "Request completion rules" rule 6; delivered at the next `Pump`. Listed names are reported with the `zenith2/` prefix stripped (the session sees `z2.index` and `z2.<slot>.<b>.<h>`), and every other request adds the prefix to the name it is given; a listed file whose remainder is not a valid object name is reported as is and ignored by the session.
- CLOUD_READ: at `Submit`, the size from `GetFileSize` (an absent file completes `NOT_FOUND` at the next `Pump`; a size above `m_xReadBuffer` is `BACKEND_ERROR`), then the read in chunks of `uONLINE_STEAM_READ_CHUNK_BYTES` (4,194,304 bytes), one call in flight per request: `SteamAPI_ISteamRemoteStorage_FileReadAsync(name, offset, min(chunk, size − offset))`; each `RemoteStorageFileReadAsyncComplete_t` call result arrives through a subsequent `RunFrame`, where the backend calls `SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete(handle, m_xReadBuffer + offset, bytes)` and issues the next chunk, and the last chunk completes the request; a call result other than `k_EResultOK`, or fewer bytes than asked, is `BACKEND_ERROR`. `Cancel` of a read marks it cancelled: the call result, when it arrives, is consumed without calling `FileReadAsyncComplete`, so the session's buffer is never written after the cancellation, and the request completes `CANCELLED` from that `Pump` (or at the deadline, from `Shutdown`, which the SDK's own buffer outlives).
- CLOUD_WRITE: at `Submit`, the conditional check compares the expected revision with the revision computed from the file's current size and timestamp (an absent file: none); a mismatch completes `REVISION_MISMATCH` at the next `Pump`; otherwise the call `SteamAPI_ISteamRemoteStorage_FileWriteAsync(name, bytes, size)`, keeping the session's buffer referenced until the call result arrives; its `RemoteStorageFileWriteAsyncComplete_t` call result completes the request: `k_EResultOK` with the new revision; `k_EResultLimitExceeded` or `GetQuota` available bytes below the object is `QUOTA`; any other result `BACKEND_ERROR`. A call that returns the invalid handle is `BACKEND_ERROR` at the next `Pump`.
- CLOUD_DELETE: the check, then `SteamAPI_ISteamRemoteStorage_FileDelete`, called from the next `Pump`, never from `Submit`. It is the only synchronous remote-storage call: it transfers no content (it removes one entry of the local cloud cache and queues the deletion for the client), so its cost is inside the measured `Online.Pump` row (proposed O3, `Zenith2/Docs/Design/ONLINE.md` → "Pending row needs").
- No transfer reads or writes file content on the main thread: the SDK performs asynchronous remote-storage I/O on its own threads, and the main thread only issues the calls and, for a read, copies the bytes the SDK already holds (`FileReadAsyncComplete`, a memory copy of at most 4 MiB per `Pump`).
- `IsCloudEnabledForApp` false: no cloud capability. `IsCloudEnabledForAccount` false: every cloud request completes `CLOUD_DISABLED` (the user turned Steam Cloud off for the game).
- Cross-device conflicts: Steam synchronises remote storage only at launch and exit and may show its own conflict dialog at launch before the game runs; the session's three-way model then compares the local saves with what Steam presents as the cloud, so a conflict Steam did not resolve, or one between the game's local saves (under `saves/`, which Steam does not see) and the cloud, still reaches the game's conflict flow. Because objects are content-addressed, Steam's own conflict dialog can only ever concern `zenith2/z2.index`. Whichever version it keeps, the other machine's commits since the last sync are undone, and the kept index may be a longer or a shorter history than that machine's. The machine reads the kept index's commit log: unless the log proves its base is in that history (the base's own commit pair), every case that would download over, delete, or re-commit over the kept index's entries is a conflict (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table" rows 14 and 22 … 26), whatever the two machines' commit counts; when the kept index is still on its own history and only lost its commit, it re-uploads (rows 18, 19). No device deletes or replaces a local save automatically because of the dialog; the bytes of an undone commit stay on the machine that made it, as its local save, until the player resolves the conflict (a slot whose kind's policy is ASK) or the slot kind's automatic policy keeps one side (`PREFER_NEWEST_UTC`, the AUTOSAVE and CHECKPOINT default, overwrites the side with the smaller UTC seconds; a deletion reason keeps the existing copy; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Automatic resolution policies"). A kept index so far ahead that its 128-commit log no longer reaches the machine's base is a conflict too (`HORIZON`); that extra conflict is the stated residual, never a loss for an ASK slot.

### Steam packaging

<!-- assumption: pending SHIP (row ON9) -->
SHIP's Windows package copies `steam_api64.dll` beside the game executable when the composition links the Steam backend, never copies a `steam_appid.txt` into a Shipping package (development runs may create one beside the executable from `online.steam.appId`; the package step refuses a Shipping tree that contains one), and lists the SDK's redistributable in the licence inventory.
<!-- /assumption -->

## Google Play backend

The Google Play backend (Stage 6) implements the adapter over Play Games Services v2 (identity, achievements, Saved Games) and the Play Billing Library (ownership of one-time products): `ZenithOnline_GooglePlayCore` holds its logic over `Zenith_PlayBridge`, `ZenithOnline_GooglePlay` the factory, the JNI bridge and the Java class (§ "Store backend libraries and what the gates prove").

### Play dependencies and Gradle wiring

<!-- assumption: pending MOD and SHIP (row ON8) -->
- Maven artefacts, pinned in `Zenith2/ThirdParty/manifest.json` and present in the offline Gradle home (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency cache"): `com.google.android.gms:play-services-games-v2` and `com.android.billingclient:billing`, each at the version the Stage 6 pin ticket records, with their licence terms admitted by SHIP's licence ruling.
- MOD's Gradle project generation (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Gradle project generation") adds, when the composition links the GooglePlay backend: the two dependencies; the Java source directory `Zenith2/Engine/ZenithOnline_GooglePlay/Internal/Android/Java/`; the manifest `meta-data` `com.google.android.gms.games.APP_ID` whose value is the string resource `@string/zenith_play_games_app_id`, generated from `game.json`'s optional `android.playGamesAppId` (a numeric project ID, a public identifier, never a secret); a game linking the backend without that value fails its configure with `Games/<Name>/game.json: android.playGamesAppId is required when the GooglePlay online backend is linked`.
<!-- /assumption -->

### Java bridge

```java
// Zenith2/Engine/ZenithOnline_GooglePlay/Internal/Android/Java/org/zenith2/online/Zenith2PlayGames.java
package org.zenith2.online;

public final class Zenith2PlayGames
{
	public static boolean init(android.app.Activity activity, long nativeHandle);   // stores nativeHandle; PlayGamesSdk.initialize; BillingClient build + startConnection
	public static void shutdown();                                       // BillingClient.endConnection
	public static void refreshIdentity(long requestId, boolean interactive);   // isAuthenticated / signIn, then getCurrentPlayer
	public static void queryPurchases(long requestId, String[] productIds);    // queryPurchasesAsync(INAPP); acknowledge PURCHASED unacknowledged ones
	public static void loadAchievements(long requestId, String[] ids);         // AchievementsClient.load(true)
	public static void unlockAchievement(long requestId, String id);           // unlockImmediate
	public static void setAchievementSteps(long requestId, String id, int steps);   // setStepsImmediate
	public static void listSnapshots(long requestId);                          // SnapshotsClient.load(true)
	public static void readSnapshot(long requestId, String name);              // open(name, false, RESOLUTION_POLICY_MANUAL), readFully, discardAndClose
	public static void writeSnapshot(long requestId, String name, byte[] data, long playedTimeMs, String description,
		long expectedModifiedMs, boolean conditional);                         // open(name, true, MANUAL); compare; writeBytes; commitAndClose
	public static void deleteSnapshot(long requestId, String name, long expectedModifiedMs, boolean conditional);
	public static long getMaxSnapshotBytes();                                   // getMaxDataSize, cached at init

	private static long s_nativeHandle;                                         // set by init; passed back as every native's first argument

	private static native void nativeOnPlayer(long nativeHandle, long requestId, int status, boolean authenticated, String playerId, String displayName);
	private static native void nativeOnPurchases(long nativeHandle, long requestId, int billingResponse, String[] productIds, int[] states);
	private static native void nativeOnAchievements(long nativeHandle, long requestId, int status, String[] ids, boolean[] unlocked, int[] steps);
	private static native void nativeOnDone(long nativeHandle, long requestId, int status, int nativeCode);
	private static native void nativeOnSnapshotList(long nativeHandle, long requestId, int status, String[] names, long[] modifiedMs, long[] playedMs, long[] bytes);
	private static native void nativeOnSnapshotData(long nativeHandle, long requestId, int status, byte[] data, long modifiedMs, long playedMs);
	private static native void nativeOnPurchasesUpdated(long nativeHandle);
	private static native void nativeOnResume(long nativeHandle);               // the activity resumed: the player must be re-read
}
```

- The C++ side (`Internal/Android/Zenith_PlayBridgeJni_Android.cpp`, the production `Zenith_PlayBridge`) obtains the class through the activity's class loader (`SDL_GetAndroidJNIEnv`, `SDL_GetAndroidActivity`, `getClassLoader().loadClass`), registers the eight natives with `RegisterNatives` from a static `constexpr` table, and calls `init(activity, nativeHandle)` with the bridge's own address as the `long` handle; every native receives that handle as its first argument and casts it back to the bridge, so no C++ variable outside the bridge object is needed (the natives have no `this`, and a process-global pointer would be a mutable global, [Z2-N-CONTRACT-152]); `init` returning false (Play services or the Billing library absent) is `ONLINE_ERROR_BACKEND_UNAVAILABLE` — the pattern PACK's bridge uses (`Zenith2/Docs/Design/PACK/Tooling.md` → "Java bridge").
- The natives run on the Android UI thread, and their signatures keep JNI's own types (`JNIEnv*`, `jclass`, `jlong`, `jint`, `jstring`, `jbyteArray`): each converts its JNI arguments (`GetStringUTFChars` and `GetByteArrayRegion`, released before return) and calls the matching member of the bridge's `Zenith_PlayEventQueue` (`ZenithOnline_GooglePlayCore`, `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineGooglePlayCore header"), the one queue class the scripted bridge uses as well, so the production queue is gate-tested on Windows; the queue copies the record (capacity 256, `QUEUE_FULL_POLICY_REJECT`; a refused record completes its request `BACKEND_ERROR` at the next `Pump` through a per-request overflow flag and counts `online.play.events.full`) under the lock `uLOCK_LEVEL_ONLINE_PLAY_EVENTS` (670, short-hold, a leaf below every lock its holder could take: the holder only copies and pushes), and return; they never call into Java or into the session.
- `nativeOnSnapshotData` copies the snapshot bytes into the request's read buffer, registered in the bridge's request table at `ReadSnapshot` under the same lock and untouched by the main thread until the completion is drained. `CancelRequest` removes the registration under the lock, so a native that arrives afterwards finds none and copies nothing; the backend completes the cancelled read `CANCELLED` from the next `Pump`, after which the session may free the buffer.
- `Pwsh_OnlinePlay_JavaBridgeMatchesJniTable` parses the Java file's `native` declarations and the C++ table's names and JNI signatures and fails on any difference or on a native whose first parameter is not `long nativeHandle`, and fails when any native or public method has a parameter whose name contains `token`, `secret`, `password` or `receipt` (none crosses JNI). Its script is `Zenith2/Engine/ZenithOnline_GooglePlay/Tests/Checks/Pwsh_OnlinePlay_JavaBridgeMatchesJniTable.ps1`; it is registered by name in the test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` (label `z2-cmake`, preset `win`), independently of `ZenithOnline_GooglePlay`'s `SUPPORTED`, which creates no target on Windows; a missing Java file, a missing native table or a missing `pwsh` makes it fail, never skip.
- The purchase token is used only inside Java, to acknowledge; it never crosses JNI.

### Play identity

- Native ID: the PGS player ID (`Player.getPlayerId()`); display name: `getDisplayName()`.
- Start: PGS v2 signs the player in automatically; the factory calls `RefreshIdentity(silent)` and the identity settles at the first `nativeOnPlayer`.
- Sign-in: interactive sign-in calls `GamesSignInClient.signIn()`; the session decides when (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Sign-in").
- Account change: the player can change the Play Games account outside the game while it is in the background. At `nativeOnResume` the backend sets its identity to unverified at once: it queues `OnIdentityChanged` with `ONLINE_IDENTITY_STATE_SIGNING_IN` ahead of every event that follows the resume, answers SIGNING_IN from `GetIdentity()`, and calls `RefreshIdentity(silent)`. Its `nativeOnPlayer` makes the identity SIGNED_IN with the player it reports (the same player: the session leaves VERIFYING without a rebind; another player: the session rebinds) or SIGNED_OUT when unauthenticated (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification"). While it reports SIGNING_IN the backend repeats `RefreshIdentity(silent)` at every connectivity change it observes and otherwise every 15 s (one call in flight at a time), so a lost `nativeOnPlayer` (Play services restarted, the callback dropped) cannot leave the session verifying for the rest of the process. No sign-out exists in PGS v2; an unauthenticated result after having been authenticated is reported as SIGNED_OUT.

### Play entitlements through Play Billing

- `QUERY_ENTITLEMENTS` calls `QueryPurchases(productIds)`: a product with a purchase in state PURCHASED is GRANTED; PENDING is UNKNOWN; absent is DENIED. An unacknowledged PURCHASED purchase of a catalogue product is acknowledged (Play refunds purchases left unacknowledged for three days); the acknowledgement's failure is logged with its response code and retried at the next refresh.
- `PurchasesUpdatedListener` and `nativeOnResume` raise `ONLINE_INVALIDATION_ENTITLEMENTS`.
- Ownership belongs to the device's Play Store account, which may differ from the Play Games player: the backend reports `uONLINE_CAPABILITY_ENTITLEMENTS_WITHOUT_SIGN_IN`, refreshes ownership whenever the Play Store account may have changed (resume), and the session's fence still clears every answer at a player change, so no answer computed under the previous player outlives a rebind (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation" I5).
- Purchase flows (buying a product in game) are not part of ONLINE (`Zenith2/Docs/Design/ONLINE.md` → "Explicit exclusions"); purchases made in the Play Store appear here.

### Play achievements

- Load: `AchievementsClient.load(true)`; per ID, unlocked (`getState() == Achievement.STATE_UNLOCKED`) and `getCurrentSteps()` for incremental achievements.
- Unlock: `unlockImmediate(id)`; set steps: `setStepsImmediate(id, steps)` (Play keeps the maximum). Play's client queues these while offline; a failure status is `BACKEND_ERROR` with the Play status code.

### Play saved games

- Names: snapshot unique names equal the object names (`z2.<slot>.<b>.<h>`, `z2.index`).
- CLOUD_LIST: `SnapshotsClient.load(true)` filtered to the `z2.` prefix; revisions per § "Request completion rules" rule 6.
- CLOUD_READ: `open(name, false, RESOLUTION_POLICY_MANUAL)`; a `DataOrConflict` conflict on a read resolves to the server copy (`resolveConflict(conflictId, serverSnapshot)`) and raises `ONLINE_INVALIDATION_CLOUD`, because the index is the session's concurrency control; then `readFully`, `discardAndClose`. An envelope object is written once and never rewritten, so only `z2.index` can be in conflict. Resolving to the server copy can undo an index commit this device made while offline (below); the session's decision table re-uploads when the server copy is still this device's base or previous base, and records a conflict when the server copy is another device's history (an online device committed meanwhile), never a deletion or an overwrite of either side by the decision table (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud-save model overview", rows 14, 19 and 22 … 26); the recorded conflict is then settled by the player (ASK) or by the slot kind's automatic policy, which for `PREFER_NEWEST_UTC` overwrites the side with the smaller UTC seconds.
- CLOUD_WRITE: `open(name, true, RESOLUTION_POLICY_MANUAL)`; a conflict, or a conditional write whose expected revision differs from the opened snapshot's (an existing snapshot for a create-only write), is `REVISION_MISMATCH` (the snapshot is discarded); otherwise `writeBytes`, `commitAndClose` with `SnapshotMetadataChange` played time (the envelope's playtime in milliseconds) and description (the save display name, so Play's saved-games UI shows the game's own name for the slot). While the device is offline, Play compares against and commits to its locally cached snapshot: the completion is OK, but the commit is not yet the service's and can lose to the server copy when the device reconnects, so on Play a commit is durable only when it was made online; the backend reports nothing different, and the cloud model's lineage test, not the backend, decides what an undone commit means.
- CLOUD_DELETE: the check, then `delete(metadata)`.
- Objects above `getMaxDataSize()` make their slot TOO_LARGE (`Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot eligibility").

### Play bridge seam

<!-- elaboration -->
The JNI calls sit behind the interface `Zenith_PlayBridge`, declared in `ZenithOnline_GooglePlayCore`'s public root header (`Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineGooglePlayCore header"), one virtual per Java method above plus `CancelRequest` and `Drain(Zenith_PlayEventSink&)`, with two implementations: `Zenith_PlayBridgeJni` (`ZenithOnline_GooglePlay/Internal/Android/Zenith_PlayBridgeJni_Android.cpp`, production) and `Zenith_PlayBridgeScripted` (`ZenithOnline_GooglePlayCore/Internal/Zenith_PlayBridgeScripted.cpp`, created by `Zenith_CreatePlayBridgeScripted`). The backend selects the scripted bridge when `online.googlePlay.scriptedBridge` names a JSON script file, read through PLAT's file I/O: on Windows a host path; on a device a file under the test APK's generated-input directory, pushed there by the device run (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files"); it is refused in Shipping. The host tests create the scripted bridge directly. The script holds the initial player, purchases, achievements and snapshots, a timeline of events (`resume` with the same or a new player and a `confirmFrames` delay before its `nativeOnPlayer`, `purchasesUpdated`, `conflictOnNextOpen`, `serverSnapshotOnNextOpen` with a name and the bytes the conflict resolves to (the scripted form of an offline commit losing to the server), `eventBurst` with a count) and records every call, like the Steam seam.

## Credentials and partner access

ONLINE holds no credential in the repository, in configuration or at run time:

- Steam needs an app ID (public) and a running, logged-in Steam client; Play needs a Play Games project ID (public), a Play Console app whose signing certificate's SHA-1 is registered for PGS, and test accounts. None of these is a secret the engine stores; PGS and Play Billing authenticate inside Google Play services, and the Steam client authenticates itself.
- ONLINE never requests a Steam session ticket, a Play server authentication code or an OAuth token, because Zenith2 has no game server in scope (Z2-D-073); it therefore never stores, logs or transmits one.
- The work that needs partner access is `needs-human` (`Zenith2/Docs/Design/ONLINE.md` → "Stage plan"): obtaining the Steamworks SDK and pinning it; creating the Steam app's achievements, stats and Steam Cloud quota (at least 64 files and 1 GiB per user for the flagship slices; content-addressed objects keep up to two objects per slot until collection); creating the Play Console app, the PGS project, its achievements and in-app products, the licence testers and the two test Google accounts; and running the device acceptance on Windows with Steam and on the S24 with Play, which is the only evidence of the flat Steam seam and of the JNI bridge. Placeholders in the repository: the Steam app ID 480 (Valve's public Spacewar sample, development only) in the flagship slices' development configuration, and no Play project ID (a game without one cannot link the Play backend).

## Backend selection

The generated composition passes every Online backend the preset links in platform preference order: the store backends first (byte order of `BACKEND_NAME`: `GooglePlay` on Android, `Steam` on Windows once their targets exist) and `Offline` last (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend resolution" step 5, row ON18 (a), landed).

1. `Zenith_Module_Online_Start` reads `online.backend`: a non-empty value equal to the `m_szName` of a passed entry makes that entry the first candidate; a non-empty value equal to none is logged `online: online.backend '<name>' is not linked; using the composition's order` and ignored.
2. The remaining candidates are the passed entries in the order passed. ZenithOnline contains no backend name and never reorders the list ([Z2-N-ARCH-268]; core-doc item X338).
3. Candidates are tried in order; a factory failing with `ONLINE_ERROR_BACKEND_UNAVAILABLE` is logged (`online: backend <name> unavailable (<native code>); trying <next>`; native code −1 also requests the clean exit of § "Steam initialisation and pumping") and the next is tried; any other failure, or no candidate succeeding, fails `Start` with `MODULE_ERROR_BACKEND_CREATE`. The offline backend never reports unavailable and is passed last, so a composition that links it always starts.
4. The chosen backend's name is logged once and shown by the editor panel and the overlay (`Zenith2/Docs/Design/ONLINE.md` → "Diagnostics and profiling").
