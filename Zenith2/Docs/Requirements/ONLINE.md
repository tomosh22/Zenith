# ONLINE requirement catalogue

## Purpose

This catalogue lists every testable obligation of the ONLINE area (online services: the `Zenith_OnlineServices` adapter API, the offline implementation, identity, entitlements, achievements, the cloud-save conflict model and its resolution flows, account changes mid-session with no cross-account leakage, and the Steam and Google Play backends), one requirement per obligation, each with the inventory items it covers, its stage, classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design family is `Zenith2/Docs/Design/ONLINE.md` and `Zenith2/Docs/Design/ONLINE/*.md`; the inventory is `Zenith2/Docs/Requirements/Inventory.md` (items Z2-I-ONLINE-01 to Z2-I-ONLINE-14: twelve required at Stage 4, the Steam and Google Play backends required at Stage 6, Z2-D-074). A requirement's stage is the stage whose acceptance it serves; Stage 6 requirements are those of the two store backends and of the package entitlement query that PACK's Stage 6 DLC gating consumes.

## Requirements

### Z2-R-ONLINE-001 ZenithOnline module library

- **Statement:** `ZenithOnline` is a `KIND MODULE` static library (`MODULE_NAME Online`) under `Zenith2/Engine/ZenithOnline/` whose declared `DEPENDS` are exactly those of "Libraries ONLINE depends on" and whose root headers are exactly those of "Header list".
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Libgraph`, `SourceLayout` and `CompileCoverage` gate steps; `Test_OnlineModule_RegisterTypes_RegistersEveryTypeAndKey` (`Test` gate step, `Zenith2/Tests/OnlineScript/`)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Libraries ONLINE depends on"; `Zenith2/Docs/Design/ONLINE.md` → "Targets"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Header list"

### Z2-R-ONLINE-002 Offline backend library and factory

- **Statement:** `ZenithOnline_Offline` is a `KIND BACKEND` library with `BACKEND_OF ZenithOnline` and `BACKEND_NAME Offline`, supported on the `ZENITH_GPU` presets, whose public root headers declare `Zenith_CreateOnlineServices_Offline` in `Zenith_Online_Offline.h` and the test access in `Zenith_OnlineOfflineTestAccess.h`, and nothing else.
- **Covers:** Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` gate step (swap-contract report of the online family); `CMake_Composition_OnlineThunk` (MOD, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Targets"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_Online_Offline header"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineOfflineTestAccess header"

### Z2-R-ONLINE-003 Public header rules across ONLINE

- **Statement:** Every ONLINE public header carries one contract block per type and names no STL, Steamworks, JNI or Play type.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-041, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` and `Libgraph` gate steps
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Rules across the ONLINE API"

### Z2-R-ONLINE-004 Adapter interface Zenith_OnlineServices

- **Statement:** `Zenith_OnlineServices` is the one adapter interface every online backend implements: identity snapshot, capabilities, cloud object limit, `Pump`, `Submit`, `Cancel` and `Shutdown`, with the request kinds and completion payloads specified, all on the main thread.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineConformance_SubmitThenPumpCompletesOnce`, `Test_OnlineServices_Example_Runs` (`Test` gate step, `ZenithOnline_Conformance`)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineServices header"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Backend completion payloads"

### Z2-R-ONLINE-005 Backend request completion rules

- **Statement:** An accepted backend request completes exactly once from a subsequent `Pump`; a refused one delivers nothing; `Cancel` yields `CANCELLED` unless the request had finished; `Shutdown` completes every outstanding request `SHUTTING_DOWN` and refuses subsequent calls.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineConformance_SubmitThenPumpCompletesOnce`, `Test_OnlineConformance_CancelSemantics`, `Test_OnlineConformance_ShutdownSemantics`, `Test_OnlineOffline_CancelCompletesCancelled`, `Test_OnlineOffline_ShutdownCompletesShuttingDown` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules"

### Z2-R-ONLINE-006 Capability matrix honoured

- **Statement:** Each backend reports exactly the capability bits of the capability matrix, and a request kind outside its capabilities is refused with `ONLINE_ERROR_NOT_SUPPORTED` and no completion.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineConformance_UnsupportedKindRefused` per backend (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix"

### Z2-R-ONLINE-007 Module entry points

- **Statement:** `Zenith_Module_Online_RegisterTypes` registers the five `Online.*` event types and every `online.*` configuration key, and `Zenith_Module_Online_Start` takes the generated list of every linked Online backend.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineModule_RegisterTypes_RegistersEveryTypeAndKey` (`Test` gate step, `Zenith2/Tests/OnlineScript/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_Module_Online header"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys"

### Z2-R-ONLINE-008 Module start sequence unwinds on failure

- **Statement:** `Zenith_Module_Online_Start` runs the steps of "Module start" in order, and a failure of any step returns its module error with every earlier registration undone and no leak.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineModule_Start_FailsAt<Step>_Unwinds` for Catalogue, Backend, Session, Bindings, AsyncSource, Consumer, AccountConsumer, FrameListener, MemoryConsumer, Console, Panel (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Module start"

### Z2-R-ONLINE-009 Backend chosen by configuration with fallback

- **Statement:** The module tries the passed backend entry named by `online.backend` first, then the entries in the order the generated composition passes them (store backends first, `Offline` last), naming no backend itself, falling through backends that report `ONLINE_ERROR_BACKEND_UNAVAILABLE`, so a composition that links the offline backend always starts.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-02, Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineModule_BackendSelectionOrder` (`Test` gate step, `Zenith2/Tests/OnlineScript/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend selection"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys"

### Z2-R-ONLINE-010 World attach and detach

- **Statement:** `AttachWorld` creates the world record, binds its publication port, sets the `ASYNC_SOURCE_ONLINE` handler and registers `Online.Events@W<tag>`; `DetachWorld` completes every outstanding request of the world `ABANDONED` exactly once and frees the record.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_DetachWorldAbandonsRequests`, `Test_OnlineSession_AttachBeyondMaxWorlds_Refused` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "World attach and detach"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSession header"

### Z2-R-ONLINE-011 Headless operation

- **Statement:** The Online module runs unchanged in the headless configuration: every session behaviour, request and notification runs with no renderer and no window.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** the integration suites `Zenith2/Tests/OnlineAccounts/`, `Zenith2/Tests/OnlineCloud/` and `Zenith2/Tests/OnlineScript/`, all headless (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Frame-graph placement"; `Zenith2/Docs/Design/ONLINE/Tests.md` → "Integration suites"

### Z2-R-ONLINE-012 Replay mode is inert

- **Statement:** Under `--replay` the module creates no backend, reads no catalogue asset (the session gets the empty catalogue), opens no user-mount file, allocates no completion ID, drops live script requests and ignores live account events, while recorded `ASYNC_SOURCE_ONLINE` publications still reach tickets and become the same events.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineReplay_NoBackendNoFiles` (`Test` gate step, `Zenith2/Tests/OnlineScript/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Replay mode"

### Z2-R-ONLINE-013 Offline backend implements every capability

- **Statement:** The offline backend implements identity, entitlements, standard and incremental achievements and, with a cloud root, cloud storage with conditional writes, as a complete shippable backend.
- **Covers:** Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `ZenithOnline_Conformance` cases `[Offline]` and `ZenithOnline_Offline_Tests` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline backend"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix"

### Z2-R-ONLINE-014 Offline backend makes no network call

- **Statement:** The `ZenithOnline` and `ZenithOnline_Offline` archives hold no undefined symbol that, after stripping `__imp_` and stdcall decoration, equals or begins with an entry of the committed network-symbol list, and no demangled reference to ZenithPlatform's loopback development API (`Zenith_DevListener`, `Zenith_DevConnection`, `Zenith_DevSocket`); the check runs on the Windows host presets and fails when its tool is missing.
- **Covers:** Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `CMake_OnlineOffline_NoNetworkImports` with its self-test cases (`Test` gate step, `win` and `win-asan`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline network isolation"

### Z2-R-ONLINE-015 Offline accounts and the local profile

- **Statement:** The offline backend signs in the configured accounts as `offline:<name>`, and the account `offline:local` (the default) and the signed-out state both bind SAVE's local profile.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_OfflineLocalMapsToLocalProfile`, `Test_OnlineRebind_SignOut_SwitchesToLocal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline accounts"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"

### Z2-R-ONLINE-016 Offline achievement persistence

- **Statement:** The offline backend persists each account's achievements in its `ZOAC` file `user:/saves/online/offline/<store key>/achievements.zoac`, applies every change to the account the request was submitted for, keeps entries of IDs the catalogue lacks, and renames an invalid file `.corrupt` and starts empty.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineOffline_AchievementFileRoundTrip`, `Test_OnlineOffline_AchievementFileCorruptRenamed`, `Test_OnlineOffline_RequestsBoundToSubmittingAccount`, fuzz `Zenith_Fuzz_OnlineOfflineAchievements` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline entitlements and achievements"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline achievement file"

### Z2-R-ONLINE-017 Offline cloud store

- **Statement:** With `online.offline.cloudRoot` set, the offline backend stores each cloud object of an account in one atomically written file under `user:/saves/online/cloud/<cloudRoot>/<store key>/` (`cloudRoot` one path component, never a name PLAT's rule R9 reserves) whose header gives its revision, honours conditional and create-only writes and deletes, never writes a cancelled read's buffer, and reports no cloud capability without a root.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineOffline_CloudConditionalWrite`, `Test_OnlineOffline_CloudRevisionChangesEveryWrite`, `Test_OnlineOffline_NoCloudWithoutRoot`, `Test_OnlineOffline_CloudObjectRules`, `Test_OnlineOffline_CancelledReadNeverWritesBuffer` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline cloud store algorithm"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store"

### Z2-R-ONLINE-018 Offline test access

- **Statement:** `Zenith_OnlineOfflineTestAccess` drives sign-in, sign-out, connectivity, grants, other-device cloud writes, failures, latency and held completions of an offline backend only, and refuses any other backend.
- **Covers:** Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineOffline_TestAccessRefusesOtherBackends` (`Test` gate step); `TestSeparationCheck` gate step (rule 6)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineOfflineTestAccess header"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline latency and faults"

### Z2-R-ONLINE-019 Offline latency and construction failure

- **Statement:** The offline backend delivers completions only from `Pump`, after `online.offline.latencyFrames` pumps, and each construction step of its factory has a failure-injection test that unwinds.
- **Covers:** Z2-I-ONLINE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineOffline_CompletionOrderAndLatency`, `Test_OnlineOffline_Create_FailsAt<Step>_Unwinds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Offline latency and faults"

### Z2-R-ONLINE-020 Account identifier format

- **Statement:** An account is identified by `<prefix>:<native>` with the prefixes `offline`, `steam` and `gpgs` and a native identifier of 1 … 80 printable ASCII bytes; any other input is `ONLINE_ERROR_INVALID_ID`.
- **Covers:** Z2-I-ONLINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAccountID_MakeRules` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineTypes header"

### Z2-R-ONLINE-021 Account key keyed with the install secret

- **Statement:** The account key the session posts in PLAT's account signals is `Zenith_OnlineAccountKeyFromHash` (0 → 1) of the little-endian first 8 bytes of SHA-256 of the 32-byte install secret followed by the account ID's bytes, 0 for no account; the secret comes from the OS random source, lives only in the device record and is never logged, synced, published or written elsewhere.
- **Covers:** Z2-I-ONLINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAccountKey_FromHashMapsZero`, `Test_OnlineAccountKey_KeyedBySecret`, `Test_OnlineSession_Start_SignedInBindsAfterSignal`, `Test_OnlineCloudFormats_DeviceRecord` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineTypes header"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record"

### Z2-R-ONLINE-022 Display name is sanitised presentation data

- **Statement:** Display names are sanitised UTF-8 of at most 64 bytes, shown by the editor panel, overlay and console, and published to worlds only when `online.exposeDisplayName` is true and the process is not recording a replay.
- **Covers:** Z2-I-ONLINE-03, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDisplayName_Sanitises`, `Test_OnlineSession_DisplayNameOffWhileRecording`, `Test_OnlineIsolation_NoAccountIdInLogsOrReplay` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types"

### Z2-R-ONLINE-023 Session state machine

- **Statement:** The session moves between REPLAY, STARTING, SIGNED_OUT, SIGNING_IN, SIGNED_IN, VERIFYING, REBINDING, SHUTTING_DOWN and SHUT_DOWN exactly as the session state machine table states, with the action of each row and accounts compared by their full IDs.
- **Covers:** Z2-I-ONLINE-03, Z2-I-ONLINE-07, Z2-I-ONLINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_StateMachine_EveryTransition` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine"

### Z2-R-ONLINE-024 Sign-in at start and on request

- **Statement:** When the backend is already signed in when it is created (Steam, the offline backend with auto sign-in) the session binds synchronously in `Start`, before any world exists; otherwise it signs in silently at the first `Pump` (or waits while Play's automatic sign-in is in progress), and interactive sign-in happens only on request or by `online.googlePlay.interactiveSignInOnStart`.
- **Covers:** Z2-I-ONLINE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Start_BindsAtStartWhenSignedIn`, `Test_OnlineSession_Start_SignedInBindsAfterSignal`, `Test_OnlineSession_Start_NoAutoSignInStaysSignedOut`, `Test_OnlineSession_StateMachine_EveryTransition` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Sign-in"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start"

### Z2-R-ONLINE-025 Failed silent sign-in keeps the last profile

- **Statement:** A failed silent sign-in leaves the session SIGNED_OUT, publishes IDENTITY_CHANGED with reason sign-in failed, keeps SAVE's last-profile record's profile and makes the cloud unavailable.
- **Covers:** Z2-I-ONLINE-03, Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Start_SilentSignInFailureKeepsLastProfile` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Sign-in"

### Z2-R-ONLINE-026 Account change detection posts a PLAT signal

- **Statement:** When the backend's identity differs from the bound account, detection runs after the backend's `Pump` has returned and before the inbox drain, fences, and posts `ACCOUNT_SIGNAL_SIGNED_IN`, `_SIGNED_OUT` or `_SWITCHED` with the keyed target key through `Zenith_Platform::PostAccountSignal`, re-posting after 60 frames without a consumer call and running the consumer path itself when the post fails.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Start_SignedInBindsAfterSignal`, `Test_OnlineSession_SignalRepost_After60Frames`, `Test_OnlineSession_DetectionAfterBackendPump` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account change detection"

### Z2-R-ONLINE-027 Fence at detection

- **Statement:** Detection fences the session in the same `Pump`: the generation moves to the next odd value, backend requests are cancelled, accepted script requests complete `ACCOUNT_CHANGED`, entitlements become UNKNOWN for scripts and DENIED for packages, the achievement cache and display name are discarded, and cloud jobs are cancelled with their buffers detached until their requests complete.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_InFlightRequests_CompletedAccountChanged`, `Test_OnlineRebind_NotificationsDroppedAtFence` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind state per subsystem"

### Z2-R-ONLINE-028 Rebind completes in the delivering prologue

- **Statement:** The account consumer runs rebind steps R1 to R6, so with SAVE idle the save profile, the caches and the generation of the new account are bound in the frame prologue in which PLAT delivered the change, before that frame's first tick boundary.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08, Z2-I-ONLINE-09, Z2-I-ONLINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_Switch_SwitchesSaveProfileSameFrame`, `Test_OnlineAccounts_SwitchMidSessionEndToEnd` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps"

### Z2-R-ONLINE-029 Stale and duplicate signals

- **Statement:** An account event whose key differs from the keyed key of the backend's current account is counted stale and the session binds the current account; an event for the bound account (compared by account ID) while not fenced changes nothing, and an event carrying the bound key while the backend reports another account rebinds.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_StaleSignal_UsesBackendTruth`, `Test_OnlineRebind_DuplicateSignal_NoOp`, `Test_OnlineRebind_SignalKeyEqualButAccountDiffers_Rebinds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps"

### Z2-R-ONLINE-030 Save profile derivation

- **Statement:** The target profile of a rebind is `Zenith_MakeSaveProfileKey(account ID)`, except `local` when signed out or for `offline:local`, and `SetActiveProfile` is not called when the target is already active.
- **Covers:** Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_SignOut_SwitchesToLocal`, `Test_OnlineRebind_OfflineLocalMapsToLocalProfile` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Account identifiers and keys"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps"

### Z2-R-ONLINE-031 Busy profile switch retried every prologue

- **Statement:** A switch refused `SAVE_ERROR_PROFILE_BUSY` is retried at every `Pump` while the session stays fenced (requests `ACCOUNT_CHANGING`, entitlements UNKNOWN to scripts and DENIED to packages, cloud idle) and SAVE's intake freeze refuses the previous profile's new requests, so the switch completes once the work queued before it has drained.
- **Covers:** Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_ProfileBusy_RetriesEachPump`, `Test_OnlineRebind_BusySaveCannotStarveSwitch`, `Test_OnlineAccounts_SwitchWithRetryCheckpointLease` (`Test` gate step; assumption pending SAVE, row ON16 (a))
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"

### Z2-R-ONLINE-032 Failing profile switch degrades to local

- **Statement:** After `online.rebind.maxSaveAttempts` non-busy failures the session switches SAVE to `local`, binds the account with the cloud degraded and every previous-profile flag computed against `local`, retries the account's profile every 60 frames, and never leaves the new account bound to the previous account's profile.
- **Covers:** Z2-I-ONLINE-09, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_ProfileSwitchError_DegradesToLocal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"

### Z2-R-ONLINE-033 Account change during a retry

- **Statement:** An account change while a profile switch is pending re-fences the session and replaces the pending profile.
- **Covers:** Z2-I-ONLINE-08, Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_SwitchDuringRetry_ReplacesPendingProfile` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine"

### Z2-R-ONLINE-034 Previous-profile worlds cannot save into the new profile

- **Statement:** A world bound under the previous account's save profile cannot write a save into the new account's profile, no save of either account's world is accepted into the previous profile once the switch has begun, and the world's IDENTITY_CHANGED notification reports `PreviousProfileSession`.
- **Covers:** Z2-I-ONLINE-09, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_OldWorldSaveRefused`, `Test_OnlineRebind_SavesAfterSwitchRefused` (`Test` gate step; assumption pending SAVE, rows ON1 (d) and ON16 (a))
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Worlds bound under the previous profile"

### Z2-R-ONLINE-035 IdentityChanged reaches every play world

- **Statement:** Every bind, a late profile switch and the attach of a world to a bound session queue one IDENTITY_CHANGED notification to each PLAY world concerned with the reason (attached for a world that attaches), the backend, the previous-profile flag computed against the profile SAVE made active and, when exposed and the flag is clear, the display name, parked at the end of `Pump` and delivered as `Online.IdentityChanged` in phase 9.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAccounts_SwitchMidSessionEndToEnd`, `Test_OnlineScript_EventsDeliveredPhase9`, `Test_OnlineRebind_ProfileSwitchError_DegradesToLocal`, `Test_OnlineSession_Start_BindsAtStartWhenSignedIn` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types"

### Z2-R-ONLINE-036 Results carry their generation

- **Statement:** Every result and notification header carries the bind generation at completion; nothing parked after the fence carries the previous bound generation; nothing built from the previous account's data publishes into a world attached at or after the next bind; and nothing built from the new account's data except a display-name-free IDENTITY_CHANGED is queued to a world bound under the previous profile.
- **Covers:** Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_ResultsCarryGeneration`, `Test_OnlineIsolation_PausedWorldOldPublicationsReachOnlyOldWorld`, `Test_OnlineIsolation_NewAccountNotificationsSkipPreviousProfileWorlds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script result payloads"

### Z2-R-ONLINE-037 Script requests across an account change

- **Statement:** A script request accepted before a fence completes `ACCOUNT_CHANGED` exactly once; a request recorded between a fence and the following bind carries an odd generation and completes `ACCOUNT_CHANGING` whenever it is drained; a request in an inbox before the fence completes `ACCOUNT_CHANGED` when drained.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_InFlightRequests_CompletedAccountChanged`, `Test_OnlineRebind_RequestsDuringRebind_AccountChanging`, `Test_OnlineRebind_InboxBeforeFence_AccountChanged`, `Test_OnlineRebind_RequestInFencedFrame_NotRunForNewAccount` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"

### Z2-R-ONLINE-038 Saves of one account invisible to another

- **Statement:** After a switch, no slot of the previous account is listed, loaded, uploaded or downloaded under the new account.
- **Covers:** Z2-I-ONLINE-09, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_SavesNotVisibleAcrossAccounts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"

### Z2-R-ONLINE-039 Cloud copies never install across accounts

- **Statement:** A cloud object whose envelope profile key or ZSAV `zenith.profile` pair differs from the active profile is rejected and never imported, and an index of another profile is a foreign index under which nothing is imported or deleted.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_CloudCopyOfOtherAccountRejected`, `Test_OnlineCloud_InvalidIndexNeverDeletes` (foreign-profile case) (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Download sequence"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud envelope"

### Z2-R-ONLINE-040 Entitlements never cross accounts

- **Statement:** From a fence until the bound account's first refresh completes, every entitlement answers UNKNOWN to scripts and the revision has changed, so no answer computed under the previous account reaches a script; on Play the refreshed answers are the device's Play Store account's.
- **Covers:** Z2-I-ONLINE-10, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_EntitlementsUnknownUntilRefresh` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement cache"

### Z2-R-ONLINE-041 Achievements never cross accounts

- **Statement:** An achievement operation runs only under the bound generation it was recorded in: one recorded in a fenced interval answers `ACCOUNT_CHANGING`, one accepted under a previous generation is never submitted, and one from a world bound under the previous profile answers `ACCOUNT_CHANGED`.
- **Covers:** Z2-I-ONLINE-05, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_AchievementNotSubmittedAfterSwitch`, `Test_OnlineRebind_RequestInFencedFrame_NotRunForNewAccount`, `Test_OnlineIsolation_PreviousProfileWorldRequestsRefused` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"

### Z2-R-ONLINE-042 No personal data in logs, counters, crash reports or replays

- **Statement:** No account identifier, account key in invertible form, profile key or display name appears in a log line, counter, panel, crash annotation or replay log: logs and diagnostics name accounts by generation and session-local ordinal, the only account value in a replay log is the keyed account key, and a display name reaches IDENTITY_CHANGED payloads only when exposed outside recording.
- **Covers:** Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_NoAccountIdInLogsOrReplay`, `Test_OnlineDiagnostics_LogMessages`, `Test_OnlineDevTools_PanelProvider` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE.md` → "Log categories and messages"

### Z2-R-ONLINE-043 Editor and preview worlds receive no notifications

- **Statement:** Worlds attached with `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` receive no ONLINE notification.
- **Covers:** Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_EditorWorldGetsNoNotifications` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"

### Z2-R-ONLINE-044 Offline local data per account

- **Statement:** The offline backend keeps each account's achievement file and cloud store under that account's store key, binds every request to the account it was submitted for, and no file under one account's directories refers to another account.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_OfflineFilesPerAccount`, `Test_OnlineOffline_RequestsBoundToSubmittingAccount` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store"

### Z2-R-ONLINE-045 One thousand account switches leave nothing behind

- **Statement:** One thousand account changes with randomised in-flight work leave no leak, complete every accepted request exactly once, bind the last account's profile, and pass the isolation checker.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_Stress1000Switches` (`z2-stress`, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Stress tests"

### Z2-R-ONLINE-046 OS session signals do not rebind

- **Statement:** OS-sourced account signals never rebind: a session lock pauses new cloud transfers, an unlock resumes them, and a logoff starts the background flush.
- **Covers:** Z2-I-ONLINE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_SessionLockedPausesTransfers`, `Test_OnlineCloud_BackgroundFlush` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "OS session signals"

### Z2-R-ONLINE-047 Entitlement answers per catalogue entry

- **Statement:** Each catalogue entitlement has one cached state per generation; an ID absent from the catalogue answers DENIED and is logged once per session.
- **Covers:** Z2-I-ONLINE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineEntitlements_QueryStates`, `Test_OnlineEntitlements_NotInCatalogue_DeniedLoggedOnce` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement cache"

### Z2-R-ONLINE-048 Entitlement refresh

- **Statement:** Entitlements refresh at bind, on invalidation, on a query while UNKNOWN and after a failed refresh's backoff (30,000 ms doubling to 480,000 ms), with at most one refresh in flight and one queued.
- **Covers:** Z2-I-ONLINE-04, Z2-I-ONLINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineEntitlements_RefreshFailureBackoff`, `Test_OnlineEntitlements_InvalidationCoalesced` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement refresh"

### Z2-R-ONLINE-049 Entitlement revision

- **Statement:** The entitlement revision increments at every fence and at every refresh that changes an answer, and a changed refresh parks ENTITLEMENTS_CHANGED to every play world.
- **Covers:** Z2-I-ONLINE-04, Z2-I-ONLINE-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineEntitlements_RevisionIncrementsOnChange` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement cache"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement refresh"

### Z2-R-ONLINE-050 Entitlements while signed out

- **Statement:** While signed out, every entitlement is DENIED unless the backend reports ownership without sign-in (Play), whose answers keep refreshing.
- **Covers:** Z2-I-ONLINE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineEntitlements_SignedOutDenied` (`Test` gate step); `Test_OnlinePlay_EntitlementsWhileSignedOut` (`Test` gate step on the Windows presets, `ZenithOnline_GooglePlayCore_Tests`, Stage 6)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement refresh"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix"

### Z2-R-ONLINE-051 Package entitlement query

- **Statement:** `Zenith_OnlinePackEntitlements` implements PACK's `Zenith_EntitlementQuery` over the session, mapping states one to one in the normal mode, answering DENIED for every ID in the fenced-deny mode from a fence that follows a bind until the bound account's first successful refresh, and returning the session's revision.
- **Covers:** Z2-I-ONLINE-04, Z2-I-ONLINE-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePackEntitlements_MapsStatesAndRevision`, `Test_OnlinePackEntitlements_FencedDenyUntilRefresh` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement query for packages"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations"

### Z2-R-ONLINE-052 Package service uses the online query

- **Statement:** The module provides its entitlement query to the composition root at `Start`, so the package service evaluates DLC against it, and an account switch makes every package answer DENIED in the same frame, so PACK's existing revocation rule unmounts the previous account's DLC and remounts what the new account owns after its refresh.
- **Covers:** Z2-I-ONLINE-04, Z2-I-ONLINE-10, Z2-I-ONLINE-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePackEntitlements_SwitchUnmountsPreviousDlc` (`Test` gate step, `Zenith2/Tests/OnlineAccounts/`; assumption pending PACK, rows ON3 (b) and ON17)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Entitlement query for packages"; `Zenith2/Docs/Design/ONLINE.md` → "Module start"

### Z2-R-ONLINE-053 Achievement definitions

- **Statement:** Achievements are defined in the catalogue as standard or incremental (2 … 1,000,000 steps) with optional Steam names, Steam progress stats and Play IDs.
- **Covers:** Z2-I-ONLINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCatalogue_ParseRoundTrip`, `Test_OnlineCatalogue_DocumentValidation` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Online catalogue document"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineCatalogue header"

### Z2-R-ONLINE-054 Unlock is idempotent

- **Statement:** Unlocking an unlocked achievement answers OK with `alreadyUnlocked` true and makes no backend call.
- **Covers:** Z2-I-ONLINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAchievements_UnlockIdempotent` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"

### Z2-R-ONLINE-055 Achievement steps never decrease

- **Statement:** Steps are clamped to the total, never decrease, are sent as absolute values, and increments accumulate in drain order (world tag, then merge order).
- **Covers:** Z2-I-ONLINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAchievements_StepsMonotonicAndClamped`, `Test_OnlineAchievements_IncrementsAccumulateInDrainOrder`, `Test_OnlineConformance_AchievementStepsNeverDecrease` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"

### Z2-R-ONLINE-056 Achievement precondition statuses

- **Statement:** An ID absent from the catalogue answers `unknownId`, an achievement without a mapping for the store backend answers `notSupported`, and steps on a standard achievement answer `refused`.
- **Covers:** Z2-I-ONLINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAchievements_StandardRefusesSteps`, `Test_OnlineAchievements_UnknownAndUnmapped` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"

### Z2-R-ONLINE-057 Achievements are not queued while signed out

- **Statement:** An achievement request while signed out answers `notSignedIn` and nothing is submitted after a subsequent sign-in.
- **Covers:** Z2-I-ONLINE-05, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAchievements_SignedOutNotQueued` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"

### Z2-R-ONLINE-058 Achievement load and reload

- **Statement:** The achievement cache loads at bind and reloads on invalidation; with the load failed, unlock and set steps are still submitted and increment and query answer `unavailable`.
- **Covers:** Z2-I-ONLINE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAchievements_LoadFailureStillUnlocks` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement synchronisation"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"

### Z2-R-ONLINE-059 Cloud slot eligibility

- **Statement:** Slots synchronise by the catalogue's kind mask (manual, quick and autosave by default; checkpoint only when asked; resume never), excluding newer or incompatible local saves, newer cloud copies and envelopes over 320 bytes plus the limit, and keeping at most `maxSlots` slots ordered by recency then name.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_EligibilityKinds`, `Test_OnlineCloud_EligibilityCapOrder`, `Test_OnlineCloud_TooLargeSlot`, `Test_OnlineCloud_DownloadRejectsNewerFormat` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot eligibility"

### Z2-R-ONLINE-060 Content-hash identities and the fast path

- **Statement:** The model compares SHA-256 hashes of the local file, the cloud payload and the agreed base, and skips exporting a slot whose sequence and size match its in-sync base.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_FastPathSkipsExport` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"

### Z2-R-ONLINE-061 Three-way decision table

- **Statement:** `Zenith_DecideOnlineSync` applies its seven overrides in order (L UNKNOWN, C UNKNOWN, version and size states, unchanged conflicts, the backup override, stale tombstones, the repair override) and then returns exactly the action, conflict reason and row of each of the twenty-six rows of the decision table, every input matching exactly one row, comparing commit serials with the base serial and reading the index's commit log as positive evidence of lineage (`DESCENDS`, `HORIZON`, `DIVERGED`), so that only a deletion record committed after the base in an index whose log holds the base's commit (or a record of exactly the base's bytes) deletes a local slot, only an entry committed after the base in such an index replaces one, and every other case that would do either is a conflict; the rows that read no lineage decide the same under all three values.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSyncDecision_Row01` … `Test_OnlineSyncDecision_Row26`, `Test_OnlineSyncDecision_Overrides`, `Test_OnlineSyncDecision_TotalAndExclusive` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"

### Z2-R-ONLINE-062 Reconcile algorithm

- **Statement:** A reconcile lists SAVE's slots, reads the cloud index and classifies its view, decides every eligible slot, writes adoptions, forgets and cloud resets in one manifest write, runs the actions one at a time downloads first, then uploads, then deletions, and collects garbage only under a KNOWN view.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_TwoDevicesConflictFlowEndToEnd`, `Test_OnlineCloud_FirstSyncAdoptsIdenticalCopies`, `Test_OnlineCloud_CorruptIndexFlowEndToEnd` (`Test` gate step, `Zenith2/Tests/OnlineCloud/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reconcile algorithm"

### Z2-R-ONLINE-063 Upload commits at the index write

- **Statement:** An upload exports the slot, re-reads and re-decides the index, writes the envelope create-only under a fresh content-addressed name, then commits by a conditional index write (the entry's commit serial the new index serial) retried at most three times (each attempt naming a fresh object), and only then records the base and its base serial.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_UploadCommitsAtIndex`, `Test_OnlineCloud_UploadRevisionMismatchRetriesThrice`, `Test_OnlineCloud_UploadReDecidesWhenCloudChanged`, `Test_OnlineCloud_UploadNameCollisionVerified` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence"

### Z2-R-ONLINE-064 Download validates and imports through SAVE

- **Statement:** A download reads the object the index entry names, validates the envelope (hash, slot, base serial, profile, version), imports the payload through SAVE's journaled write so the previous local file becomes the backup, records the base and parks CLOUD_SLOT_REPLACED; a rejected object is never imported or deleted, and a missing referenced object is an error, never an absence.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_DownloadImportsAndNotifies`, `Test_OnlineCloud_DownloadRejectsTamperedEnvelope`, `Test_OnlineCloud_DownloadMissingObjectNeverAbsent` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Download sequence"

### Z2-R-ONLINE-065 Deletion propagation and garbage collection

- **Statement:** A local deletion reported by SAVE's listener (a tombstone) replaces the slot's index entry with a deletion record; a deletion record committed after this device's base in an index whose commit log holds the base's commit, or one of exactly the base's bytes, deletes the slot locally through SAVE; an object is deleted only by garbage collection when no KNOWN index references it, its base serial is at least two below the index serial and (a backstop on non-atomic backends) it is at least 24 hours old, at most 8 per reconcile; a deletion record is dropped once it is two serials and 30 days old (on the deleting device's clock) or evicted as the 65th; a dropped record, or a deletion commit undone with its history, can resurrect the deleted slot, never lose a copy.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_DeleteCloudNeedsTombstone`, `Test_OnlineCloud_DeleteLocalFromOtherDevice`, `Test_OnlineCloud_GarbageCollectionGraceRules`, `Test_OnlineCloud_LostIndexCommitNeverDeletes`, `Test_OnlineCloud_UndoneDeletionResurrects` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Garbage collection"

### Z2-R-ONLINE-066 Conflict detection and persistence

- **Statement:** Rows 5, 10, 13 and 14 record a conflict with its reason and both sides' metadata (the cloud display name from the index entry, without a download), persist it in the manifest, re-announce it once per session start, and block automatic actions on the slot until resolved.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_ConflictReasonsNotify`, `Test_OnlineCloud_ConflictPersistsAcrossRestart` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Conflict detection"

### Z2-R-ONLINE-067 Automatic resolution policies

- **Statement:** The catalogue's policy per kind resolves conflicts deterministically (newest UTC, longest playtime, local, cloud, with the stated tie-breaks), never deletes a copy automatically, and ASK waits for a resolution.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_PolicyNewestUtc`, `Test_OnlineCloud_PolicyLongestPlaytime`, `Test_OnlineCloud_PolicyPreferLocalAndCloud`, `Test_OnlineCloud_PolicyDeletionKeepsExisting` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Automatic resolution policies"

### Z2-R-ONLINE-068 Conflict resolution choices

- **Statement:** A conflict is resolved by keep local, keep cloud, keep both (manual slots with both copies) or defer, with the actions of the resolution table; a slot not in conflict answers `notInConflict`, an inapplicable choice `refused`, and a keep-local or keep-cloud resolution whose conflict token no longer matches the record or, at the compare before it acts, the fresh inputs answers `stale` and writes or deletes nothing.
- **Covers:** Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_ResolveKeepLocal`, `Test_OnlineCloud_ResolveKeepCloud`, `Test_OnlineCloud_ResolveKeepBoth`, `Test_OnlineCloud_ResolveDeferKeepsConflict`, `Test_OnlineCloud_ResolveRefusals`, `Test_OnlineCloud_ResolveTokenRules` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Resolution choices"

### Z2-R-ONLINE-069 Keep-both naming

- **Statement:** Keep both names the cloud copy `<slot truncated to 26 bytes>-cloud`, then `-cloud2` … `-cloud9`, and refuses when all nine exist.
- **Covers:** Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_KeepBothNaming` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Keep-both naming"

### Z2-R-ONLINE-070 Slot sync state machine

- **Statement:** Each cloud slot moves between the states of the slot sync state machine exactly as its table states (CLOUD_UNKNOWN under an UNKNOWN index view included), with transient states never persisted.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_SlotStateMachineEveryTransition` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot sync state machine"

### Z2-R-ONLINE-071 Reference conflict flow in the flagship slices

- **Statement:** Game2D and OpenWorldSlice carry the account flow and the conflict flow (behaviours, schema, UI document, strings), and a two-device conflict resolved through the conflict screen leaves the chosen copy in the slot.
- **Covers:** Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-077, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_TwoDevicesConflictFlowEndToEnd` (`Test` gate step); the Stage 4 playthrough acceptance of Game2D and OpenWorldSlice
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reference conflict screen"

### Z2-R-ONLINE-072 Accessible conflict flow

- **Statement:** The conflict screen is completed with a screen reader, with remapped controls, with reduced motion and with a colour-vision mode in the slices' accessible-flow acceptance runs.
- **Covers:** Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Device_OnlineAccessibleConflictFlow` (`needs-human` stage acceptance run)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Reference conflict screen"; `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs"

### Z2-R-ONLINE-073 Interrupted sync converges

- **Statement:** An interruption at any step of an upload, download, deletion, collection or repair leaves a state the next reconcile repairs (upload again, adopt, download again, forget or collect), and a lost manifest produces conflicts, adoptions and one-sided transfers only, never a silent overwrite or a deletion.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_InterruptedAfterObjectWrite`, `Test_OnlineCloud_InterruptedAfterIndexWrite`, `Test_OnlineCloud_InterruptedDuringImport`, `Test_OnlineCloud_InterruptedAfterIndexDelete`, `Test_OnlineCloud_ManifestLostProducesNoOverwrite` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Interrupted sync recovery"

### Z2-R-ONLINE-074 One transfer at a time within the governor

- **Statement:** The cloud engine runs one transfer at a time (a reconcile-time export and a repair read included), reserves its buffer from the memory governor before allocating it, and defers the transfer when the reservation is refused.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_TransferDeferredOnGovernorRefusal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Transfer memory"

### Z2-R-ONLINE-075 Upload triggers and debounce

- **Statement:** A slot write triggers a targeted reconcile after `online.cloud.uploadDelayMs` (restarted by a further write of the slot), and the other triggers of the trigger table start full or targeted reconciles as listed.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_DebounceAutosaveBurst` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Polling and triggers"

### Z2-R-ONLINE-076 Cloud status reporting

- **Statement:** The cloud status (available, unavailable, disabled, quota exceeded, degraded, index unreadable, read only) changes exactly as its table states and each change queues CLOUD_STATUS to every play world.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_StatusTransitions` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting"

### Z2-R-ONLINE-077 Shutdown and background flush

- **Statement:** At shutdown and on entering the background the engine uploads pending slots for at most the flush deadline (3,000 ms; 2,000 ms in the background), cancels the running transfer at the deadline without committing it, and writes the manifest.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_ShutdownFlushDeadline`, `Test_OnlineCloud_BackgroundFlush` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Shutdown and background flush"

### Z2-R-ONLINE-078 Cancelled transfers leave no partial state

- **Statement:** One thousand transfers cancelled at randomised steps, including during an in-flight read, leave no write to a freed buffer, no partial slot, no leak and no unreleased reservation, and every subsequent reconcile converges.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_Stress1000CancelledTransfers` (`z2-stress`, `Test` gate step and `Sanitizers -Preset win-asan`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Stress tests"; `Zenith2/Docs/Design/ONLINE.md` → "Cancellation"

### Z2-R-ONLINE-079 Cloud envelope format

- **Statement:** A slot's cloud object is the 320-byte `ZCLD` header (with the 128-byte save display name and the base serial) followed by the exact ZSAV bytes, named `z2.<slot>.<base serial>.<hash prefix>`, written and parsed with the rules of the envelope and object-name tables.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloudFormats_EnvelopeRoundTrip`, `Test_OnlineCloudFormats_EnvelopeEveryRuleRejects`, `Test_OnlineCloudFormats_ObjectNameGrammar`, fuzz `Zenith_Fuzz_OnlineEnvelope`, `Zenith_Fuzz_OnlineCloudObjectName` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud envelope"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names"

### Z2-R-ONLINE-080 Cloud index format

- **Statement:** The cloud index `z2.index` is the `ZCIX` object of a 2,112-byte header (with its 128-pair commit log, log floor and repair-floor flag) and at most 128 sorted 256-byte records (at most 64 live entries and 64 deletion records, each with its flags, commit serial, base serial and 128-byte display name; a deletion record names the deleting writer's backend), at most 34,880 bytes, written and parsed with the rules of its table, its profile key checked by the caller.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloudFormats_IndexRoundTripAndRules`, `Test_OnlineCloudFormats_IndexCommitLog`, fuzz `Zenith_Fuzz_OnlineCloudIndex` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index"

### Z2-R-ONLINE-081 Sync manifest format

- **Statement:** The sync manifest `user:/saves/online/<profile key>/sync.<backend>.zosm` holds one 104-byte entry per slot with its base hash, base serial, previous base token, tombstone flag, persisted state and local identity, at most 6,672 bytes; one file per backend, and a file whose backend byte differs from its name is invalid.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloudFormats_ManifestRoundTripAndRules`, `Test_OnlineCloud_PreviousBaseTokenMaintenance`, fuzz `Zenith_Fuzz_OnlineSyncManifest` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest"

### Z2-R-ONLINE-082 Device record

- **Statement:** The 64-byte device record `user:/saves/online/device.zodv` holds one non-zero device ID and the 32-byte install secret from the OS random source, is replaced when missing or invalid, is excluded from device backup, and never reaches simulation.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloudFormats_DeviceRecord`, `Test_OnlineSession_Create_FailsAtDeviceRecord_Unwinds`, fuzz `Zenith_Fuzz_OnlineDeviceRecord` (`Test`, `Sanitizers -Preset win-asan` gate steps; assumption pending MOD and PLAT, rows ON22 (b) as amended (the second literal exclusion) and ON19 (b))
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record"

### Z2-R-ONLINE-083 Online catalogue authoring document

- **Statement:** Each game's online catalogue is the `zenith2.online` JSON document with the schema and validation rules C1 … C8, validated by `Zenith_ValidateOnlineCatalogueDocument` in `ZenithAuthoring`, which the cook step `online.catalogue` (registered in ZenithCook) and the editor's document view call.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-04, Z2-I-ONLINE-05, Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCatalogue_DocumentValidation` (`Test` gate step, `ZenithAuthoring_Tests`); `Test_OnlineCook_CatalogueStepWritesSection` (`Test` gate step, `ZenithCook_Tests`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Document schema"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Validation rules"; `Zenith2/Docs/Design/ONLINE.md` → "Cook steps"

### Z2-R-ONLINE-084 Catalogue document migration

- **Statement:** The catalogue document type is registered by `Zenith_RegisterOnlineDocumentTypes` (`ZenithAuthoring`) with SCENE's document framework at schema version 1 with unknown-field preservation, its migration fixtures live under `ZenithAuthoring_Tests`' fixture root, and every schema change adds one migration step and one fixture pair.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCatalogue_MigrationFixture` (`Test` gate step, `ZenithAuthoring_Tests`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Migration chain"; `Zenith2/Docs/Design/ONLINE.md` → "Migration hooks"

### Z2-R-ONLINE-085 Cooked online catalogue

- **Statement:** `Zenith_CompileOnlineCatalogue` writes the catalogue as the `ZONC` section of a cooked asset of the online catalogue kind, and `Zenith_OnlineCatalogue::Parse` refuses every rule violation with its rule number and accepts the limits but not one more.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCatalogue_ParseRoundTrip`, `Test_OnlineCatalogue_ParseEveryRuleRejects`, `Test_OnlineCatalogue_LimitsAtAndAbove` (`ZenithOnline_Tests`), `Test_OnlineCatalogue_CompileRoundTrip` (`ZenithAuthoring_Tests`), fuzz `Zenith_Fuzz_OnlineCatalogue` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cooked online catalogue"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineCatalogue header"

### Z2-R-ONLINE-086 Catalogue loaded at start with a safe fallback

- **Statement:** The module loads the catalogue named by `online.catalogue` at start, and a missing or invalid catalogue is logged, counted and replaced by an empty one.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineModule_CatalogueMissingStartsEmpty` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Module start"

### Z2-R-ONLINE-087 Online Luau namespace

- **Statement:** The `Online` namespace offers exactly the eleven functions of the script-facing table, each a behaviour-only asynchronous request of `ASYNC_SOURCE_ONLINE` returning a ticket whose result has the listed fields.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-03, Z2-I-ONLINE-04, Z2-I-ONLINE-05, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_EveryFunctionReturnsTicket`, `Test_OnlineScript_ChunkSystemCallRefused` (`Test` gate step, `Zenith2/Tests/OnlineScript/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API"

### Z2-R-ONLINE-088 Script request payload encoding

- **Statement:** Online requests are encoded as the 4-byte header (source kind, op 1 … 11, argument length) and the op's arguments, at most 80 bytes, decoded with rules R1 … R7.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCodec_RequestRoundTripEveryOp`, `Test_OnlineCodec_RequestRulesReject`, fuzz `Zenith_Fuzz_OnlineScriptRequest` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script request payloads"

### Z2-R-ONLINE-089 Script result payloads and decoder

- **Statement:** Results are recorded as the 8-byte result header and the op's body, decoded by `Zenith_DecodeOnlineResultForScript` into the documented fields, a zero-length payload decodes to `abandoned`, and any other shape returns false.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCodec_ResultRoundTripEveryOp`, `Test_OnlineCodec_ZeroLengthIsAbandoned`, `Test_OnlineCodec_MalformedResultReturnsFalse`, fuzz `Zenith_Fuzz_OnlineResult` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script result payloads"

### Z2-R-ONLINE-090 Notification payloads

- **Statement:** Notifications are recorded as the 8-byte notification header and the kind's body (CLOUD_STATUS with statuses 1 … 7 and reasons 0 … 9), at most 256 bytes, decoded with rules N1 … N7.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCodec_NotificationRoundTripEveryKind`, `Test_OnlineCodec_NotificationRulesReject`, fuzz `Zenith_Fuzz_OnlineNotification` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Notification payloads"

### Z2-R-ONLINE-091 Online events delivered in phase 9

- **Statement:** The five `Online.*` event types are emitted by `Online.Events@W<tag>` in phase 1 from the notifications published at the tick boundary, in completion-ID order, and delivered in phase 9 of that tick with the documented fields.
- **Covers:** Z2-I-ONLINE-03, Z2-I-ONLINE-04, Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_EventsDeliveredPhase9` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Event types"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"

### Z2-R-ONLINE-092 Online misuse codes

- **Statement:** The `Online` bindings raise ZS2901 … ZS2906 with their catalogued texts for invalid IDs, steps, increments, slot names, choices and conflict tokens, before recording any side effect, and never assert.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_MisuseCodes` (`Test` gate step); `Test_Script_Misuse_Catalogue` trigger scripts
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Misuse codes"

### Z2-R-ONLINE-093 Rolled-back invocations request nothing

- **Statement:** A behaviour invocation that fails after an `Online` call records nothing, consumes no completion ID and submits nothing to the backend.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_RolledBackInvocationRequestsNothing` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request intake"

### Z2-R-ONLINE-094 Requests abandoned by a restore

- **Statement:** An online request in flight in a restored save completes with the zero-length payload, which its ticket reports as `abandoned`.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_RestoreAbandonsInFlight` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Script result payloads"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API"

### Z2-R-ONLINE-095 Request timeouts are recorded

- **Statement:** A script request's deadline is its intake time plus `online.requestTimeoutMs`; a request that passes it is completed `TIMEOUT`, the outcome recorded so a replay reproduces it.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_TimeoutRecorded`, `Test_OnlineVerify_TimeoutKeepsBinding` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Timeouts"

### Z2-R-ONLINE-096 Every accepted script request completes once

- **Statement:** Every accepted script request is completed exactly once: with its result, `ACCOUNT_CHANGED`, `TIMEOUT`, `SHUTTING_DOWN` or `ABANDONED`.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_Stress1000Switches`, `Test_OnlineScript_DetachWorldAbandonsRequests` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Result completion"

### Z2-R-ONLINE-097 Online results reach simulation only as recorded publications

- **Statement:** No simulation-visible value depends on a backend except through `ASYNC_SOURCE_ONLINE` publications of `PUBLICATION_KIND_GENERIC_RECORDED` and PLAT's recorded account events, so a recorded run replays bit-identically with no backend.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineReplay_RecordedRunReplaysBitIdentically` (`Test` gate step, `Zenith2/Tests/OnlineScript/`)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Determinism obligations"

### Z2-R-ONLINE-098 Notification IDs allocated in the prologue for a recorded kind

- **Statement:** ONLINE allocates notification completion IDs only on the main thread in the frame prologue, for a recorded kind, never under `--replay`; a capacity refusal is retried at the next `Pump`, and a site refusal by the publication sink is logged, counted and permanent, never a retry loop.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDeterminism_PrologueAllocationsRecorded`, `Test_OnlineReplay_NoBackendNoFiles`, `Test_OnlineSession_NotifySiteRefusalIsPermanent` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"; `Zenith2/Docs/Design/ONLINE.md` → "Determinism obligations"

### Z2-R-ONLINE-099 Replays agree across worker counts

- **Statement:** A recorded run with online activity replays to identical tick hashes with 1, 2, 8 and all workers.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineReplay_WorkerCountsAgree` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Determinism and replay tests"

### Z2-R-ONLINE-100 Per-world request inboxes

- **Statement:** Script requests pass from each world's merge chain to the main thread through one single-producer `Zenith_SpscRing` per world sized by the online capacity, and requests from concurrent worlds each complete once in their world's order.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_OnlineSession_ConcurrentSubmitFromTwoWorlds` (`z2-stress`, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Cross-thread hand-offs"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request intake"

### Z2-R-ONLINE-101 Notification queues and parking limits

- **Statement:** Each play world queues at most 96 notifications (dropping the oldest entitlement or status ones first) and holds at most 8 parked and unpublished at a time in its outstanding set, from which a publication removes only an ID the set holds.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_NotificationsDroppedAtFence`, `Test_OnlineScript_RestoredNotificationsDeliveredAsRecorded`, `Test_OnlineDiagnostics_CountersPublished` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"

### Z2-R-ONLINE-102 Thread affinity and frame placement

- **Statement:** ONLINE runs its account consumer and `Pump` on the main thread in frame-prologue step 1, its intake on merge nodes, its handler on tick-boundary step 1 and `Online.Events` in phase 1, and owns no thread.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineScript_EventsDeliveredPhase9`, `Test_OnlineAccounts_SwitchMidSessionEndToEnd`, `Test_OnlineSession_ConcurrentSubmitFromTwoWorlds`, `Test_OnlineSession_DetectionAfterBackendPump` (`Test` gate step); `ApiContractCheck` gate step (every public type's Thread affinity field)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Frame-graph placement"; `Zenith2/Docs/Design/ONLINE.md` → "Thread affinity per type"; `Zenith2/Docs/Design/ONLINE.md` → "Threads ONLINE owns"

### Z2-R-ONLINE-103 Online error enumeration

- **Statement:** `Zenith_OnlineErrorCode` and `Zenith_OnlineStatus` carry exactly the enumerators specified, each with its `Zenith_ToString` spelling, and `Zenith_OnlineError` is an 8-byte string-free record.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineErrors_ToStringEveryCode` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineErrors header"; `Zenith2/Docs/Design/ONLINE.md` → "Error enumerations"

### Z2-R-ONLINE-104 Online counters

- **Statement:** ONLINE publishes exactly the counters of its counter table with their kinds and the bounded-queue triples.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDiagnostics_CountersPublished` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Counters"

### Z2-R-ONLINE-105 Online log categories and messages

- **Statement:** ONLINE logs exactly the messages of its log table in the categories `online`, `online.account`, `online.cloud`, `online.steam` and `online.play`.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDiagnostics_LogMessages` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Log categories and messages"

### Z2-R-ONLINE-106 Online profiling zones

- **Statement:** ONLINE emits the zones `Online.Pump`, `Online.Rebind`, `Online.Requests`, `Online.CloudReconcile`, `Online.CloudTransfer`, `Online.Notifications`, `Online.Intake`, `Online.Publish` and `Online.Events`.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDiagnostics_ZonesEmitted` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Profiling zones"

### Z2-R-ONLINE-107 Online console commands

- **Statement:** In development builds ONLINE registers the `online.*` console commands, each acting only through presentation actions, and registers none when the console is absent.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-03, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDevTools_ConsoleCommands` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Console commands"

### Z2-R-ONLINE-108 Online overlay panel

- **Statement:** The `online` overlay panel shows the backend, state, generation, account ordinal, local-profile flag, cloud status, index view, pending requests and slot rows, never an account identifier, account key or profile key.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineDevTools_PanelProvider` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "On-screen profiler rows and panel"

### Z2-R-ONLINE-109 Editor online panel

- **Statement:** The editor's Online panel shows a play session's identity view and slot states and offers sign-in, sign-out, switch, sync, index repair and resolve through the session's presentation actions.
- **Covers:** Z2-I-ONLINE-03, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_EditorOnlinePanel_ShowsStateAndActions` (`Test` gate step, `ZenithEditor_Tests`)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Editor surfaces"

### Z2-R-ONLINE-110 Memory consumer and shedding

- **Statement:** The session reports its resident bytes (detached buffers included) under `MEMORY_SUBSYSTEM_OTHER`, starts no new transfer, collection or repair at CRITICAL pressure, and resumes at NORMAL.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_MemoryConsumerReportsAndDefers` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Memory and the governor"

### Z2-R-ONLINE-111 Session construction unwinds

- **Statement:** `Zenith_OnlineSession::Create` validates its desc and unwinds exactly what it built when any construction step fails.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Create_FailsAt<Step>_Unwinds` (Allocation, DeviceRecord, Caches, InternalPool, CloudBuffers), `Test_OnlineSession_Create_InvalidDesc_Refused` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSession header"

### Z2-R-ONLINE-112 Session shutdown drains

- **Statement:** Shutdown completes every outstanding request `SHUTTING_DOWN`, flushes uploads within the deadline, calls the backend's `Shutdown` once and reaches SHUT_DOWN before the module is destroyed.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_ShutdownDrains` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Startup and shutdown"

### Z2-R-ONLINE-113 Backend data validated as untrusted

- **Statement:** Every identifier, name, answer, list and object a backend receives from its SDK is validated against ONLINE's limits before delivery, and an invalid one is an error, never an assert.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineConformance_BadBackendDataRejected` per backend logic, fuzz `Zenith_Fuzz_OnlineBackendPayload` (`Test`, `Sanitizers -Preset win-asan` gate steps)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules"; `Zenith2/Docs/Design/ONLINE.md` → "Security assumptions"

### Z2-R-ONLINE-114 ONLINE parsers are fuzzed

- **Statement:** Every ONLINE parser of external bytes has a libFuzzer target run as a smoke in `win-asan` and for its full duration at stage acceptance.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** fuzz targets `Zenith_Fuzz_Online*` (`Sanitizers -Preset win-asan` gate step; stage acceptance)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Format and fuzz tests"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits"

### Z2-R-ONLINE-115 No credentials or tokens held

- **Statement:** ONLINE requests, stores, logs and transmits no credential, session ticket, authentication code or purchase token: no configuration key is named or typed as one, no JNI native or Java bridge method carries one, and no recorded bridge call can hold one.
- **Covers:** Z2-I-ONLINE-11, Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineModule_ConfigKeysHoldNoCredential` (`Test` gate step, `Zenith2/Tests/OnlineScript/`); `Pwsh_OnlinePlay_JavaBridgeMatchesJniTable` (registered by the test registry on `win`), `Test_OnlinePlay_PurchasesMapToEntitlements` (`Test` gate step, Windows presets)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Security assumptions"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access"

### Z2-R-ONLINE-116 Development seams refused in Shipping

- **Statement:** The scripted Steam API, the scripted Play bridge and the offline test access are unavailable in Shipping builds: the scripted factories refuse with `ONLINE_ERROR_INVALID_DESC` and the test-access headers are confined to tests.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_ScriptedApiRefusedInShipping`, `Test_OnlinePlay_ScriptedBridgeRefusedInShipping` (`Test` gate step, Windows presets); `TestSeparationCheck` gate step for the three `TestAccess` headers
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam API seam"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play bridge seam"

### Z2-R-ONLINE-117 Steam backend library and SDK provisioning

- **Statement:** `ZenithOnline_Steam` is a Windows-only backend (layer 9, over `ZenithOnline_SteamCore`) holding only the factory and the flat-API seam over the Steamworks SDK pinned as a manually downloaded prebuilt entry; the target exists only when the SDK is provisioned, so no gate builds it, and Steam headers are confined to its `Internal/Windows/`.
- **Covers:** Z2-I-ONLINE-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `needs-human` SDK pin and licence ruling; `Device_OnlineSteam_Windows` (`needs-human`: configure with the SDK, `Test` with the target present); `Libgraph` gate step for `ZenithOnline_SteamCore`, which includes no SDK header; the declared-but-unavailable library state is assumption pending ARCH, core-doc item X363 (c)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steamworks SDK provisioning"; `Zenith2/Docs/Design/ONLINE.md` → "Targets"

### Z2-R-ONLINE-118 Steam API seam

- **Statement:** Every Steamworks call of the Steam backend goes through `Zenith_SteamApi` (declared in `ZenithOnline_SteamCore`), with the flat-API implementation in `ZenithOnline_Steam` and the scripted recording implementation in `ZenithOnline_SteamCore`, selected by `online.steam.scriptedApi` or created directly by tests.
- **Covers:** Z2-I-ONLINE-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `ZenithOnline_SteamCore_Tests` and the `[Steam]` conformance cases over the scripted seam (`Test` gate step, Windows presets); the flat implementation only in `Device_OnlineSteam_Windows` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam API seam"

### Z2-R-ONLINE-119 Steam initialisation, pumping and shutdown

- **Statement:** The Steam backend initialises with `SteamAPI_InitFlat` (and restart-if-necessary when configured, after which the module requests a clean exit before the first frame), dispatches callbacks and call results manually on the main thread once per `Pump`, stores stats at most every 5 s and at shutdown, and calls `SteamAPI_Shutdown` after its asynchronous writes have completed or the deadline passed.
- **Covers:** Z2-I-ONLINE-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_InitFailureUnavailable`, `Test_OnlineSteam_RestartIfNecessaryRequestsExit`, `Test_OnlineSteam_AchievementsAndProgressStats` (`Test` gate step, Windows presets); `Test_OnlineModule_SteamRelaunchRequestsExit` (`Test` gate step, `Zenith2/Tests/OnlineScript/`, Windows presets)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam initialisation and pumping"

### Z2-R-ONLINE-120 Steam identity

- **Statement:** The Steam identity is `steam:` plus the decimal SteamID with the persona name, connected while logged on, and signed out on `SteamShutdown_t`.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_IdentityFromSteamID`, `Test_OnlineSteam_ShutdownCallbackSignsOut` (`Test` gate step, Windows presets), `ZenithOnline_SteamCore_Tests`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam identity"

### Z2-R-ONLINE-121 Steam entitlements

- **Statement:** Steam entitlements answer `BIsSubscribedApp` per DLC app ID, and `DlcInstalled_t` invalidates them.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_EntitlementsFromSubscriptions` (`Test` gate step, Windows presets), `ZenithOnline_SteamCore_Tests`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam entitlements"

### Z2-R-ONLINE-122 Steam achievements and progress stats

- **Statement:** Steam unlocks call `SetAchievement`, incremental steps raise an INT stat, show progress only at each tenth and unlock at the total, and the catalogue's Steam names and stats match the app configuration.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_AchievementsAndProgressStats`, `Test_OnlineSteam_CatalogueMatchesScript` (`Test` gate step, Windows presets), `ZenithOnline_SteamCore_Tests`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam achievements"

### Z2-R-ONLINE-123 Steam cloud storage

- **Statement:** Steam cloud objects are remote-storage files `zenith2/<name>` with revisions from name, size and timestamp (none when absent), conditional and create-only writes checked against them, transfers through `FileWriteAsync` and `FileReadAsync` (in 4 MiB chunks) completed from a subsequent `Pump`, `CLOUD_DISABLED` when the user disabled Steam Cloud and `QUOTA` when the quota is exhausted.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_CloudFileMapping`, `Test_OnlineSteam_CloudDisabledAndQuota`, `Test_OnlineSteam_CloudTransfersAreAsynchronous`, `Test_OnlineSteam_CancelledReadNotCopied`, `Test_OnlineSteam_DeleteFromPumpOnly` (`Test` gate step, Windows presets), `ZenithOnline_SteamCore_Tests`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam cloud storage"

### Z2-R-ONLINE-124 Steam packaging

- **Statement:** A Windows package of a game linking the Steam backend carries `steam_api64.dll` beside the executable and never a `steam_appid.txt`.
- **Covers:** Z2-I-ONLINE-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** SHIP's package checks (assumption pending SHIP, row ON9); `Device_OnlineSteam_Windows`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam packaging"

### Z2-R-ONLINE-125 Steam device acceptance

- **Statement:** On the reference desktop with a Steam client, identity, entitlements, achievements and cloud saves work for one test account, and a second account sees none of the first account's saves, entitlements or achievements.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-07, Z2-I-ONLINE-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Device_OnlineSteam_Windows` (`needs-human` stage acceptance run, evidence record)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access"

### Z2-R-ONLINE-126 Google Play backend library and Gradle wiring

- **Statement:** `ZenithOnline_GooglePlay` is an Android-only backend (layer 9, over `ZenithOnline_GooglePlayCore`) holding the factory, the JNI bridge and the Java class, whose Play Games Services v2 and Play Billing dependencies and `APP_ID` meta-data are added to a game's Gradle project only when the composition links it.
- **Covers:** Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** MOD's Gradle golden-file tests for the Play backend (assumption pending MOD, row ON8); `CompileCoverage` gate step on Android presets; `Device_OnlinePlay_S24` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play dependencies and Gradle wiring"; `Zenith2/Docs/Design/ONLINE.md` → "Targets"

### Z2-R-ONLINE-127 Play Java bridge contract

- **Statement:** The Java class `org.zenith2.online.Zenith2PlayGames` has exactly the methods and natives of the bridge contract, its natives only copy into the bounded event queue, and the purchase token never crosses JNI.
- **Covers:** Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_OnlinePlay_JavaBridgeMatchesJniTable`, registered by name in the test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` (label `z2-cmake`, `Test` gate step on `win`, failing when an input is missing); `Device_OnlinePlay_S24` (`needs-human`, the bridge's run-time behaviour)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Java bridge"

### Z2-R-ONLINE-128 Play identity and resume detection

- **Statement:** The Play identity is `gpgs:` plus the player ID with the display name; at a resume the backend reports an unverified identity until it re-reads the player, a different player then raises an identity change, the same player resumes without a rebind, and an unauthenticated result after sign-in reports signed out.
- **Covers:** Z2-I-ONLINE-14, Z2-I-ONLINE-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePlay_IdentityAndResumeSwitch`, `Test_OnlinePlay_ResumeSuspendsUntilPlayerConfirmed`, `Test_OnlinePlay_UnauthenticatedAfterSignedIn_SignsOut` (`Test` gate step, Windows presets), `ZenithOnline_GooglePlayCore_Tests`; `Device_OnlinePlay_S24` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play identity"

### Z2-R-ONLINE-129 Play entitlements through Play Billing

- **Statement:** Play entitlements map PURCHASED to GRANTED, PENDING to UNKNOWN and absent to DENIED, acknowledge unacknowledged purchased catalogue products, and refresh on purchase updates and resume.
- **Covers:** Z2-I-ONLINE-14, Z2-I-ONLINE-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePlay_PurchasesMapToEntitlements` (`Test` gate step, Windows presets), `ZenithOnline_GooglePlayCore_Tests`; `Device_OnlinePlay_S24` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play entitlements through Play Billing"

### Z2-R-ONLINE-130 Play achievements

- **Statement:** Play achievements unlock with `unlockImmediate`, set steps with `setStepsImmediate` and load unlocked state and current steps.
- **Covers:** Z2-I-ONLINE-14, Z2-I-ONLINE-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePlay_AchievementsMapping` (`Test` gate step, Windows presets), `ZenithOnline_GooglePlayCore_Tests`; `Device_OnlinePlay_S24` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play achievements"

### Z2-R-ONLINE-131 Play saved games

- **Statement:** Play cloud objects are snapshots opened with manual conflict resolution, where a conflict or an existing snapshot on a conditional or create-only write is a revision mismatch, a conflict on a read resolves to the server copy and invalidates the cloud, a cancelled read is deregistered so its buffer is never written, and commits carry the envelope's playtime and display name.
- **Covers:** Z2-I-ONLINE-14, Z2-I-ONLINE-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePlay_SnapshotConflictIsRevisionMismatch`, `Test_OnlinePlay_SnapshotMetadataFromEnvelope`, `Test_OnlinePlay_CancelledReadDeregistered` (`Test` gate step, Windows presets), `ZenithOnline_GooglePlayCore_Tests`; `Device_OnlinePlay_S24` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play saved games"

### Z2-R-ONLINE-132 Play bridge seam

- **Statement:** Every JNI call of the Play backend goes through `Zenith_PlayBridge` (declared in `ZenithOnline_GooglePlayCore`), with the JNI implementation in `ZenithOnline_GooglePlay` and the scripted recording implementation in `ZenithOnline_GooglePlayCore`, selected by `online.googlePlay.scriptedBridge` (a host path on Windows, a generated-input file on a device) or created directly by tests.
- **Covers:** Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `ZenithOnline_GooglePlayCore_Tests` and the `[GooglePlay]` conformance cases (`Test` gate step, Windows presets); the JNI implementation only in `Device_OnlinePlay_S24` (`needs-human`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play bridge seam"

### Z2-R-ONLINE-133 Play event queue bound

- **Statement:** The Play bridge's event queue holds 256 records under the short-hold lock at level 670, and a refused record completes its request `BACKEND_ERROR` and is counted.
- **Covers:** Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlinePlay_EventQueueOverflow` (`Test` gate step, Windows presets), `ZenithOnline_GooglePlayCore_Tests` (the scripted bridge's queue has the JNI bridge's capacity); `Device_OnlinePlay_S24` (`needs-human`, the JNI natives under the lock)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Java bridge"; `Zenith2/Docs/Design/ONLINE.md` → "Lock-order entries"

### Z2-R-ONLINE-134 Play device acceptance

- **Statement:** On the S24 with a Play test build, the Play core tests and conformance pass on the device; sign-in, purchases, achievements and snapshots work for test account A; a switch to account B while backgrounded verifies, then rebinds on resume with none of A's data visible; a resume without a switch does not rebind; and switching back restores A's state.
- **Covers:** Z2-I-ONLINE-14, Z2-I-ONLINE-08, Z2-I-ONLINE-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Device_OnlinePlay_S24` (`needs-human` stage acceptance run, evidence record)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs"

### Z2-R-ONLINE-135 Partner configuration is human work

- **Statement:** The Steamworks app configuration and the Play Console app, Play Games project, achievements, products and test accounts are created by a human and checked against the catalogue, with no secret committed.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** the `needs-human` partner-configuration tickets; `Test_OnlineSteam_CatalogueMatchesScript`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Credentials and partner access"

### Z2-R-ONLINE-136 Backend conformance suite

- **Statement:** One conformance suite runs every scenario against the offline backend and against the Steam and Play backend logic over their scripted seams, registered per backend, on the host presets.
- **Covers:** Z2-I-ONLINE-01, Z2-I-ONLINE-02, Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `ZenithOnline_Conformance` (`Test` gate step; `[Steam]` and `[GooglePlay]` on the Windows presets)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Conformance suite"

### Z2-R-ONLINE-137 Account switch end to end

- **Statement:** In the headless runtime, an account switch mid-session rebinds before gameplay reacts, delivers `Platform.AccountChanged` and `Online.IdentityChanged`, and the game's account flow returns to a world bound under the new profile, whose saves go to the new profile.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08, Z2-I-ONLINE-09, Z2-I-ONLINE-10, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAccounts_SwitchMidSessionEndToEnd`, `Test_OnlineAccounts_SignOutDuringSaveWrite` (`Test` gate step, `Zenith2/Tests/OnlineAccounts/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Integration suites"; `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration"

### Z2-R-ONLINE-138 Offline device run on the S24

- **Statement:** Game2D on the S24 with the offline backend passes the account-switch and conflict-flow scripts, and a kill during a download converges after relaunch.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-07, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Device_OnlineOffline_S24` (`needs-human` stage acceptance run, evidence record)
- **Design:** `Zenith2/Docs/Design/ONLINE/Tests.md` → "Device and needs-human runs"

### Z2-R-ONLINE-139 Online budget rows allocated

- **Statement:** The four proposed ONLINE rows (resident memory, transfer peak, `Online.Pump` CPU, rebind latency) are allocated by the Stage 1 per-system budget ticket for ONLINE and measured by their procedures.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the Stage 1 per-system budget ticket for ONLINE (`needs-human`); the pending rows of core-doc item X321
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Pending row needs"

### Z2-R-ONLINE-140 Online asynchronous capacity per world

- **Statement:** Each world's online inbox and pending pool hold the online value of Z2-B-SCRIPT-026, and the source's capacity is that value plus 8 notifications.
- **Covers:** Z2-I-ONLINE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_OnlineSession_ConcurrentSubmitFromTwoWorlds` (`Test` gate step); SCRIPT's registry row (core-doc item X324)
- **Design:** `Zenith2/Docs/Design/ONLINE.md` → "Rows ONLINE is measured against"; `Zenith2/Docs/Design/ONLINE.md` → "Asynchronous source and misuse block"

### Z2-R-ONLINE-141 The bind is a second generation boundary

- **Statement:** The generation is odd from a fence to the following bind and even otherwise: a fence moves it to the next odd value and the bind to the next even value, so a request recorded between a fence and the bind (the fenced frame's ticks, a busy switch) can never carry the generation it is drained under and is never executed for the account bound next.
- **Covers:** Z2-I-ONLINE-07, Z2-I-ONLINE-08, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_RequestInFencedFrame_NotRunForNewAccount`, `Test_OnlineRebind_SwitchDuringRetry_ReplacesPendingProfile`, `Test_OnlineSession_StateMachine_EveryTransition`, `Test_OnlineRebind_Stress1000Switches` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Generation fencing"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind state per subsystem"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution"

### Z2-R-ONLINE-142 Unreadable cloud index is unknown, never empty

- **Statement:** A failed index read, an index failing its parser, an index of another profile, an index of a newer format (at least 48 bytes with the `ZCIX` magic and a version above 1; anything else that fails is invalid), and a missing index while envelope objects remain are each an UNKNOWN index view, under which no local or cloud deletion, transfer, index write or garbage collection happens on any device; a newer-format index makes every slot read-only and is never rewritten by this build; only a verified-empty cloud is EMPTY, and it re-seeds by upload without deleting anything locally.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_InvalidIndexNeverDeletes` (one subcase per cause, a wrong magic with version bytes 2 included), `Test_OnlineCloud_NewerIndexIsReadOnly`, `Test_OnlineCloud_EmptyCloudReseeds`, `Test_OnlineCloudFormats_IndexViewClassification` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index view"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index"

### Z2-R-ONLINE-143 Committed index entries always reference existing bytes

- **Statement:** Envelope objects are content-addressed by slot, base serial and payload-hash prefix and written create-only, so no upload overwrites the object a committed index entry references; an object is deleted only by garbage collection once no KNOWN index references it and its base serial is at least two below the index serial, which guarantees that a committed entry's bytes exist on backends whose conditional write is atomic; on the two non-atomic backends a 24-hour age backstop applies, and an object another history's index still names that is found missing is healed by a device holding the identical bytes and is otherwise an error, never an absence.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_ConcurrentUploadsKeepCommittedBytes`, `Test_OnlineCloud_UploadNameCollisionVerified`, `Test_OnlineCloud_GarbageCollectionGraceRules`, `Test_OnlineCloud_DownloadMissingObjectNeverAbsent`, `Test_OnlineCloudFormats_ObjectNameGrammar`, `Test_OnlineCloud_LostIndexCommitNeverDeletes`, `Test_OnlineCloud_MissingObjectHealedFromIdenticalLocal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Garbage collection"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud object names"

### Z2-R-ONLINE-144 Local deletions propagate only from SAVE's deletion listener

- **Statement:** A synced slot is deleted from the cloud only when SAVE's `OnSlotDeleted` reported it (a tombstone in the sync manifest); a local file that vanishes without that report is restored from the cloud, and a reconcile-time export failure takes no action on the slot.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_DeleteCloudNeedsTombstone`, `Test_OnlineCloud_VanishedLocalFileRestoredNotDeleted`, `Test_OnlineCloud_ExportFailureTakesNoAction`, `Test_OnlineSyncDecision_Row11`, `Test_OnlineSyncDecision_Row16`, `Test_OnlineSyncDecision_Row17` (`Test` gate step; assumption pending SAVE, row ON1 (c))
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest"

### Z2-R-ONLINE-145 Backup-recovered slots never overwrite the cloud

- **Statement:** A slot SAVE lists RECOVERABLE_FROM_BACKUP whose backup hash differs from the base while the cloud's entry still holds the base's bytes downloads the cloud copy instead of uploading the older backup, whatever the entry's commit serial and lineage.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_BackupRecoveredSlotDownloads`, `Test_OnlineSyncDecision_Overrides` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot eligibility"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"

### Z2-R-ONLINE-146 Display names never enter a replay log

- **Statement:** In a process that records a replay, `online.exposeDisplayName` is ignored with one log line, so every recorded `Online.IdentityChanged` carries an empty display name and the replay reproduces it.
- **Covers:** Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_DisplayNameOffWhileRecording`, `Test_OnlineIsolation_NoAccountIdInLogsOrReplay` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Configuration keys"

### Z2-R-ONLINE-147 An unverified identity fences its window

- **Statement:** While a backend reports its signed-in player as unverified (Play after a resume), the session executes no script request (every drained record waits in the pending pool as a deferred record with its intake deadline, the one place a request recorded during the hold can wait), submits nothing, pauses the cloud engine, defers account events and holds every completion; confirmation of the same account applies the held completions without a rebind; another account or none rebinds and discards them; a long wait keeps the binding and the hold, and only the backend's sign-out or another player rebinds.
- **Covers:** Z2-I-ONLINE-08, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineVerify_SameAccountResumesWithoutRebind`, `Test_OnlineVerify_OtherAccountRebinds`, `Test_OnlineVerify_TimeoutKeepsBinding`, `Test_OnlineVerify_ConsumerEventDeferred`, `Test_OnlineVerify_ResubmitAfterPredecessorCompletion`, `Test_OnlineVerify_HeldCompletionOfAbandonedRecordFreesIt`, `Test_OnlineRebind_SigningInDuringBusySwitchKeepsPending` (`Test` gate step); `Test_OnlinePlay_ResumeSuspendsUntilPlayerConfirmed` (`Test` gate step, Windows presets; Stage 6)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Identity verification"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play identity"

### Z2-R-ONLINE-148 Transfer buffers outlive the requests that may write them

- **Statement:** A transfer buffer, and its governor reservation, is freed only after every backend request and SAVE operation it was given has delivered its completion, a cancelled one included; a fence detaches it from its job, and no new transfer starts while a detached buffer is held.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_FencedBufferOutlivesCancelledRead`, `Test_OnlineConformance_CancelledReadReleasesBuffer`, `Test_OnlineOffline_CancelledReadNeverWritesBuffer` (`Test` gate step); `Test_OnlineCloud_Stress1000CancelledTransfers` (`Sanitizers -Preset win-asan`)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Transfer memory"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Request completion rules"; `Zenith2/Docs/Design/ONLINE.md` → "Cancellation"

### Z2-R-ONLINE-149 Previous-profile worlds receive no online service for the new account

- **Statement:** After a bind or a late profile switch, every script request of a world whose save binding profile differs from the bound profile answers `ACCOUNT_CHANGED`, so the previous account's world can neither unlock an achievement for the new account nor read its entitlements or cloud state; a world bound under the new profile is served normally.
- **Covers:** Z2-I-ONLINE-09, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_PreviousProfileWorldRequestsRefused` (`Test` gate step; assumption pending SAVE, row ON1 (d))
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Worlds bound under the previous profile"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution"

### Z2-R-ONLINE-150 Save intake frozen while a profile switch waits

- **Statement:** From `BeginProfileSwitch(P′)` until SAVE accepts the switch, every new save request of a world not bound under P′ is refused `SAVE_ERROR_PROFILE_MISMATCH`, so nothing of the new account's play is written into the previous profile and a frequent autosaver cannot postpone the switch.
- **Covers:** Z2-I-ONLINE-09, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_SavesAfterSwitchRefused`, `Test_OnlineRebind_BusySaveCannotStarveSwitch`, `Test_OnlineAccounts_SignOutDuringSaveWrite` (`Test` gate step; assumption pending SAVE, row ON16 (a))
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind steps"

### Z2-R-ONLINE-151 Cloud index repaired only on request

- **Statement:** An index in an UNKNOWN view of cause INVALID, FOREIGN_PROFILE or MISSING is rebuilt only by an explicit repair (presentation action, console command or `Online.RepairCloudIndex`), which reads and verifies every listed envelope of the active profile, keeps per slot the one with the largest base serial, marks the index and every entry it writes repaired, writes no deletion record, commits with a conditional write, and is refused under any other view; neither the repair nor any reconcile after it deletes a local or cloud slot because of it: a slot it could not represent is re-uploaded by a device that holds it, a device whose base differs from a repaired entry records a conflict, and a device whose base is the repaired entry but whose local file changed uploads it.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_IndexRepair`, `Test_OnlineCloud_IndexRepairNeverDeletesLocal`, `Test_OnlineCloud_RepairedEntryLocalChangeUploads`, `Test_OnlineSession_PresentationActions_Refusals` (`Test` gate step); `Test_OnlineCloud_CorruptIndexFlowEndToEnd` (`Test` gate step, `Zenith2/Tests/OnlineCloud/`)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index repair"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Script-facing API"

### Z2-R-ONLINE-152 Store backend logic is gate-tested without the SDK or a device

- **Statement:** The Steam and Google Play backends' logic lives in `ZenithOnline_SteamCore` (Windows) and `ZenithOnline_GooglePlayCore` (every GPU preset), engine libraries that include no store SDK, JNI or Java and are tested over their scripted recording seams in the `Test` step of the Windows presets; the SDK-facing backends hold only factories and seam implementations, and their behaviour is evidenced only by the needs-human device runs.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `ZenithOnline_SteamCore_Tests`, `ZenithOnline_GooglePlayCore_Tests` and the `[Steam]` and `[GooglePlay]` conformance cases (`Test` gate step, Windows presets); `Libgraph` gate step (the core libraries' declared edges and layer 8); the core libraries' `KIND ENGINE` and their dependence above the API's layer are assumption pending ARCH, core-doc item X363 (a), (b)
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Store backend libraries and what the gates prove"; `Zenith2/Docs/Design/ONLINE.md` → "Targets"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineSteamCore header"; `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Zenith_OnlineGooglePlayCore header"

### Z2-R-ONLINE-153 Steam remote storage never transfers on the main thread

- **Statement:** Every Steam cloud read and write is an asynchronous remote-storage call whose call result is handled through manual dispatch in a subsequent `Pump`; reads proceed in chunks of at most 4 MiB, the only synchronous remote-storage call is the content-free `FileDelete`, issued from `Pump`, and no completion is delivered from `Submit`.
- **Covers:** Z2-I-ONLINE-13, Z2-I-ONLINE-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSteam_CloudTransfersAreAsynchronous`, `Test_OnlineSteam_DeleteFromPumpOnly`, `Test_OnlineSteam_CancelledReadNotCopied` (`Test` gate step, Windows presets), `ZenithOnline_SteamCore_Tests`
- **Design:** `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam cloud storage"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam initialisation and pumping"

### Z2-R-ONLINE-154 A late profile switch notifies every play world

- **Statement:** When a degraded bind's retry (`SetActiveProfile` alone, every 60 frames, so no intake freeze refuses the current world's saves) switches SAVE from `local` to the account's profile, every attached PLAY world is queued IDENTITY_CHANGED with its previous-profile flag recomputed against the new profile, the cloud engine is enabled, CLOUD_STATUS available follows its first reconcile, and the generation is unchanged.
- **Covers:** Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineRebind_ProfileSwitchError_DegradesToLocal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Profile switch retry"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting"

### Z2-R-ONLINE-155 A local save is deleted only by a deletion record

- **Statement:** A cloud deletion is written as a deletion record (an index entry flagged deleted with the deleting write's commit serial), never as the removal of an entry, and a device deletes its local copy of a slot only on positive evidence: a deletion record committed after its base serial in an index whose commit log holds the base's own commit, or a record that deleted exactly the base's bytes; no index lacking an entry, no index of another history, no repaired index and no index whose log no longer reaches the base deletes a local save on serials alone; any other record newer than the base, and any record of another history at or below the base serial, is a conflict.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSyncDecision_Row12`, `Test_OnlineSyncDecision_Row14`, `Test_OnlineSyncDecision_Row24`, `Test_OnlineSyncDecision_Overrides` (the lineage-aware no-deletion sweep), `Test_OnlineCloud_DeleteLocalFromOtherDevice`, `Test_OnlineCloud_DeletionRecordOfUnseenCopyConflicts`, `Test_OnlineCloud_CommitLogHorizonConflicts`, `Test_OnlineCloud_UndoneDeletionResurrects`, `Test_OnlineCloud_LostIndexCommitNeverDeletes`, `Test_OnlineCloud_IndexRepairNeverDeletesLocal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index"

### Z2-R-ONLINE-156 Lost and regressed commits are re-uploaded

- **Statement:** A synced slot whose committed base the index has lost while the index is still on the device's own history (no entry and no record, an entry of the base's bytes or of the previous base, the base this device held before its current one, at an older serial, or a record not newer than the base in an index whose log holds the base's commit) is re-uploaded by the device that holds it, and the re-upload passes the U4 re-decision; it is never deleted locally or overwritten with the older copy by the table; a different entry at the base's own serial, any other entry of a shorter history, and a record at or below the base serial that the log does not prove to be of the base's history are conflicts instead, which the player settles for an ASK slot (nothing is lost until then) and the slot kind's automatic policy settles otherwise (`PREFER_NEWEST_UTC` overwrites the side with the smaller UTC seconds; a deletion reason keeps the existing copy); on Steam (the launch-time conflict dialog) and on Play (an offline commit losing to the server) the same rules apply over the scripted seams.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSyncDecision_Row18` … `Test_OnlineSyncDecision_Row22`, `Test_OnlineCloud_LostIndexCommitNeverDeletes`, `Test_OnlineCloud_ConcurrentSameSerialCommitIsConflict`, `Test_OnlineCloud_LostCommitOverForeignEntryConflicts`, `Test_OnlineCloud_PreviousBaseTokenMaintenance` (`Test` gate step); `Test_OnlineSteam_LostIndexCommitNeverDeletes`, `Test_OnlinePlay_LostIndexCommitNeverDeletes` (`Test` gate step, Windows presets, Stage 6)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud-save model overview"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Play saved games"

### Z2-R-ONLINE-157 ONLINE's files obey PLAT's user-mount namespace

- **Statement:** Every file ONLINE writes is under `user:/saves/online/` (the device record, the per-backend sync manifests, the offline achievement files and cloud store) and passes PLAT's virtual-path rules R1 … R10; SAVE ignores that directory, and on Android the device record and its `WriteAtomic` temporary are excluded from backup and transfer as the two literal paths `saves/online/device.zodv` and `saves/online/device.zodv.z2atomic`, while the manifests travel with the saves.
- **Covers:** Z2-I-ONLINE-02, Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Create_FailsAtDeviceRecord_Unwinds` (the path case) (`Test` gate step; assumption pending SAVE, row ON22; MOD's landed backup rules, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Backup and transfer rules")
- **Design:** `Zenith2/Docs/Design/ONLINE/Formats.md` → "Conventions for every ONLINE format"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Sync manifest"

### Z2-R-ONLINE-158 New-account notifications never reach a previous-profile world

- **Statement:** From a bind on, ENTITLEMENTS_CHANGED, CLOUD_CONFLICT, CLOUD_SLOT_REPLACED and CLOUD_STATUS are queued only to PLAY worlds whose previous-profile flag is clear, and a flagged world's IDENTITY_CHANGED carries an empty display name, so no data of the new account enters the previous account's world even if the account returns before that world is replaced.
- **Covers:** Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineIsolation_NewAccountNotificationsSkipPreviousProfileWorlds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Notifications"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"

### Z2-R-ONLINE-159 No carry-over across a profile change

- **Statement:** A world transition whose old world is bound under a profile other than SAVE's active one carries no entity over, neither requested IDs nor `Zenith.Runtime.CarryOver` entities, so a world created under the new account starts with nothing of the previous account's world.
- **Covers:** Z2-I-ONLINE-09, Z2-I-ONLINE-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineAccounts_SwitchTransitionCarriesNothingOver` (`Test` gate step, `Zenith2/Tests/OnlineAccounts/`; assumption pending GAME, row ON23 (a))
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; `Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration"

### Z2-R-ONLINE-160 Bind at start

- **Statement:** When the backend reports SIGNED_IN as soon as it is created, `Start` binds the account synchronously (the save profile switched, generation 2, the account's work submitted, no account signal) before any world attaches, so no world created at start-up is flagged; a world attaching is told the identity it finds with reason attached (signed in when bound or verifying, signed out after a failed or skipped sign-in), and one attaching while the session is starting, signing in or rebinding receives that settle's or bind's notification instead; a first bind that settles only after worlds exist flags those whose profile differs, which the flagship title flow avoids by waiting for the first identity notification.
- **Covers:** Z2-I-ONLINE-03, Z2-I-ONLINE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Start_BindsAtStartWhenSignedIn`, `Test_OnlineSession_Start_WorldsCreatedBeforeFirstBind` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start"; `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Session state machine"; `Zenith2/Docs/Design/ONLINE.md` → "Module start"

### Z2-R-ONLINE-161 Divergent cloud histories conflict

- **Statement:** Every committing index write appends (its commit serial, its commit token) to a 128-pair commit log, and the manifest keeps each slot's base serial and previous base token; a download over the local save (row 8) and a local deletion (row 12, unless the record is of the base's bytes) are taken only when the read index's log holds the base's own commit pair; when it does not, because the index is another history, a repaired one (its log starts empty) or so far ahead that its log no longer reaches the base (128 or more committing writes, `HORIZON`), the case is a conflict (rows 14, 23, 24), and a re-commit over another device's entry (row 25) or a cloud deletion over it (row 26) is a conflict unless the entry is this device's base bytes or previous base, on every backend and whatever the two histories' commit counts; a conflict is settled by the player for an ASK slot, with nothing lost until then, and by the slot kind's automatic policy otherwise (`PREFER_NEWEST_UTC`, the AUTOSAVE and CHECKPOINT default, overwrites the side with the smaller UTC seconds; a deletion reason keeps the existing copy); the accepted cost is one conflict per changed slot for a device whose base the cloud moved 128 or more committing writes past, or that predates an index repair, even when its local file is unchanged.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12, Z2-I-ONLINE-13, Z2-I-ONLINE-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSyncDecision_Row23` … `Test_OnlineSyncDecision_Row26`, `Test_OnlineSyncDecision_TotalAndExclusive`, `Test_OnlineCloud_DivergentChainsUnequalLengthConflict`, `Test_OnlineCloud_DeletionRecordOfUnseenCopyConflicts`, `Test_OnlineCloud_LostCommitOverForeignEntryConflicts`, `Test_OnlineCloud_ThreeDeviceDivergenceConflicts`, `Test_OnlineCloud_CommitLogHorizonConflicts`, `Test_OnlineCloud_RepairThenCommitStillConflictsForPreRepairBase`, `Test_OnlineCloud_PreviousBaseTokenMaintenance`, `Test_OnlineCloudFormats_IndexCommitLog` (`Test` gate step); `Test_OnlineSteam_DivergentChainsConflict`, `Test_OnlinePlay_DivergentChainsConflict` (`Test` gate step, Windows presets, Stage 6)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Three-way decision table"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Cloud index"

### Z2-R-ONLINE-162 A live cloud entry is never removed for capacity

- **Statement:** An upload that would leave more than 64 live entries in the index is not made: it ends as an error with the backoff, queues the quota-exceeded cloud status, logs and counts it, and changes no entry, record or object; only a deletion of a cloud slot frees a place.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_IndexFullRefusesNewSlotNeverEvicts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Upload sequence"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Cloud status reporting"

### Z2-R-ONLINE-163 The SAVE slot listener lives exactly as long as the session needs it

- **Statement:** `Zenith_OnlineSession::Create` registers its `Zenith_OnlineSaveListener` with SAVE as its last construction step outside replay, unregisters it when any construction step fails, and `BeginShutdown` unregisters it before anything else, so SAVE never calls into a session that does not exist or is shutting down.
- **Covers:** Z2-I-ONLINE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineSession_Create_FailsAtListenerRegistration_Unwinds`, `Test_OnlineSession_ShutdownClearsSlotListener` (`Test` gate step; assumption pending SAVE, row ON1 (c))
- **Design:** `Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Internal declarations"; `Zenith2/Docs/Design/ONLINE.md` → "Module start"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Deletion propagation"

### Z2-R-ONLINE-164 An index repair never exposes a pre-repair base to a silent overwrite

- **Statement:** After an index repair (an empty commit log, its floor at the repair's serial), no device whose base the repair chose or predates has its local save downloaded over or deleted because of an entry or record another device committed after the repair, because no pre-repair base can be proven `DESCENDS`: it records a conflict (rows 23, 24), before and after the repaired entry's flag is cleared; the header's repair-floor flag (set by the repair, kept by every ordinary and record-dropping write, cleared by a log overflow) is diagnostic only: it selects the conflict's log line, the conflict counts `online.cloud.horizon_conflicts` either way, and no decision reads it.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_RepairThenCommitStillConflictsForPreRepairBase`, `Test_OnlineCloud_IndexRepairNeverDeletesLocal`, `Test_OnlineCloudFormats_IndexCommitLog` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index repair"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Hashes and identities"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Commit tokens and the commit log"

### Z2-R-ONLINE-165 A conflict resolution acts only on the inputs the player saw

- **Statement:** Every conflict record carries a 41-byte conflict token (whether L is present and its hash prefix, the cloud side's kind, commit serial and hash prefix), carried by `ONLINE_NOTIFY_CLOUD_CONFLICT` (247 bytes at most), `QueryCloudSlot` and the slot status; a reconcile that finds a conflicted slot's inputs changed refreshes the record and the token and notifies again (or re-applies the automatic policy); `ResolveCloudConflict(slot, choice, token)` answers `STALE` when the token differs from the record's, and KEEP_LOCAL and KEEP_CLOUD compare it again with the fresh L and the freshly read index in the same `Pump` that submits their action, so no resolution, by the player or by a policy, uploads over, imports over or deletes a version the token does not name; on `STALE` nothing is written or deleted, the record is refreshed and the conflict notified again; KEEP_BOTH keeps both current sides and needs no compare.
- **Covers:** Z2-I-ONLINE-06, Z2-I-ONLINE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_OnlineCloud_ResolveStaleCloudRefused`, `Test_OnlineCloud_ResolveStaleLocalRefused`, `Test_OnlineCloud_ResolveTokenRules`, `Test_OnlineCloud_ConflictRefreshRenotifies`, `Test_OnlineCodec_NotificationRoundTripEveryKind`, `Test_OnlineCodec_NotificationRulesReject`, `Test_OnlineCodec_RequestRoundTripEveryOp` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Resolution choices"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Conflict detection"; `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Slot sync state machine"; `Zenith2/Docs/Design/ONLINE/Formats.md` → "Notification payloads"

## Coverage of inventory items

| Item | Name | Requirements |
|---|---|---|
| Z2-I-ONLINE-01 | the `Zenith_OnlineServices` adapter API | Z2-R-ONLINE-001, Z2-R-ONLINE-003, Z2-R-ONLINE-004, Z2-R-ONLINE-005, Z2-R-ONLINE-006, Z2-R-ONLINE-007, Z2-R-ONLINE-008, Z2-R-ONLINE-009, Z2-R-ONLINE-010, Z2-R-ONLINE-011, Z2-R-ONLINE-012, Z2-R-ONLINE-083, Z2-R-ONLINE-084, Z2-R-ONLINE-085, Z2-R-ONLINE-086, Z2-R-ONLINE-087, Z2-R-ONLINE-088, Z2-R-ONLINE-089, Z2-R-ONLINE-090, Z2-R-ONLINE-092, Z2-R-ONLINE-093, Z2-R-ONLINE-094, Z2-R-ONLINE-095, Z2-R-ONLINE-096, Z2-R-ONLINE-097, Z2-R-ONLINE-098, Z2-R-ONLINE-099, Z2-R-ONLINE-100, Z2-R-ONLINE-101, Z2-R-ONLINE-102, Z2-R-ONLINE-103, Z2-R-ONLINE-104, Z2-R-ONLINE-105, Z2-R-ONLINE-106, Z2-R-ONLINE-108, Z2-R-ONLINE-110, Z2-R-ONLINE-111, Z2-R-ONLINE-112, Z2-R-ONLINE-113, Z2-R-ONLINE-114, Z2-R-ONLINE-136, Z2-R-ONLINE-139, Z2-R-ONLINE-140 |
| Z2-I-ONLINE-02 | an offline implementation | Z2-R-ONLINE-002, Z2-R-ONLINE-005, Z2-R-ONLINE-009, Z2-R-ONLINE-012, Z2-R-ONLINE-013, Z2-R-ONLINE-014, Z2-R-ONLINE-015, Z2-R-ONLINE-016, Z2-R-ONLINE-017, Z2-R-ONLINE-018, Z2-R-ONLINE-019, Z2-R-ONLINE-044, Z2-R-ONLINE-107, Z2-R-ONLINE-136, Z2-R-ONLINE-138, Z2-R-ONLINE-157 |
| Z2-I-ONLINE-03 | identity | Z2-R-ONLINE-015, Z2-R-ONLINE-020, Z2-R-ONLINE-021, Z2-R-ONLINE-022, Z2-R-ONLINE-023, Z2-R-ONLINE-024, Z2-R-ONLINE-025, Z2-R-ONLINE-087, Z2-R-ONLINE-091, Z2-R-ONLINE-107, Z2-R-ONLINE-109, Z2-R-ONLINE-120, Z2-R-ONLINE-160 |
| Z2-I-ONLINE-04 | entitlements | Z2-R-ONLINE-047, Z2-R-ONLINE-048, Z2-R-ONLINE-049, Z2-R-ONLINE-050, Z2-R-ONLINE-051, Z2-R-ONLINE-052, Z2-R-ONLINE-083, Z2-R-ONLINE-087, Z2-R-ONLINE-091, Z2-R-ONLINE-121, Z2-R-ONLINE-129 |
| Z2-I-ONLINE-05 | achievements | Z2-R-ONLINE-016, Z2-R-ONLINE-041, Z2-R-ONLINE-053, Z2-R-ONLINE-054, Z2-R-ONLINE-055, Z2-R-ONLINE-056, Z2-R-ONLINE-057, Z2-R-ONLINE-058, Z2-R-ONLINE-083, Z2-R-ONLINE-087, Z2-R-ONLINE-122, Z2-R-ONLINE-130 |
| Z2-I-ONLINE-06 | cloud-save conflict model | Z2-R-ONLINE-017, Z2-R-ONLINE-039, Z2-R-ONLINE-059, Z2-R-ONLINE-060, Z2-R-ONLINE-061, Z2-R-ONLINE-062, Z2-R-ONLINE-063, Z2-R-ONLINE-064, Z2-R-ONLINE-065, Z2-R-ONLINE-066, Z2-R-ONLINE-067, Z2-R-ONLINE-070, Z2-R-ONLINE-073, Z2-R-ONLINE-074, Z2-R-ONLINE-075, Z2-R-ONLINE-076, Z2-R-ONLINE-077, Z2-R-ONLINE-078, Z2-R-ONLINE-079, Z2-R-ONLINE-080, Z2-R-ONLINE-081, Z2-R-ONLINE-082, Z2-R-ONLINE-083, Z2-R-ONLINE-091, Z2-R-ONLINE-110, Z2-R-ONLINE-123, Z2-R-ONLINE-131, Z2-R-ONLINE-142, Z2-R-ONLINE-143, Z2-R-ONLINE-144, Z2-R-ONLINE-145, Z2-R-ONLINE-148, Z2-R-ONLINE-151, Z2-R-ONLINE-153, Z2-R-ONLINE-155, Z2-R-ONLINE-156, Z2-R-ONLINE-157, Z2-R-ONLINE-161, Z2-R-ONLINE-162, Z2-R-ONLINE-163, Z2-R-ONLINE-164, Z2-R-ONLINE-165 |
| Z2-I-ONLINE-07 | account change mid-session: sign-out | Z2-R-ONLINE-023, Z2-R-ONLINE-026, Z2-R-ONLINE-027, Z2-R-ONLINE-028, Z2-R-ONLINE-029, Z2-R-ONLINE-035, Z2-R-ONLINE-037, Z2-R-ONLINE-045, Z2-R-ONLINE-046, Z2-R-ONLINE-125, Z2-R-ONLINE-137, Z2-R-ONLINE-138, Z2-R-ONLINE-141 |
| Z2-I-ONLINE-08 | account change mid-session: switch user | Z2-R-ONLINE-023, Z2-R-ONLINE-026, Z2-R-ONLINE-027, Z2-R-ONLINE-028, Z2-R-ONLINE-029, Z2-R-ONLINE-033, Z2-R-ONLINE-035, Z2-R-ONLINE-037, Z2-R-ONLINE-045, Z2-R-ONLINE-128, Z2-R-ONLINE-134, Z2-R-ONLINE-137, Z2-R-ONLINE-141, Z2-R-ONLINE-147 |
| Z2-I-ONLINE-09 | account change: saves rebind | Z2-R-ONLINE-025, Z2-R-ONLINE-028, Z2-R-ONLINE-030, Z2-R-ONLINE-031, Z2-R-ONLINE-032, Z2-R-ONLINE-033, Z2-R-ONLINE-034, Z2-R-ONLINE-038, Z2-R-ONLINE-064, Z2-R-ONLINE-137, Z2-R-ONLINE-149, Z2-R-ONLINE-150, Z2-R-ONLINE-154, Z2-R-ONLINE-159, Z2-R-ONLINE-160 |
| Z2-I-ONLINE-10 | account change: entitlements rebind | Z2-R-ONLINE-028, Z2-R-ONLINE-040, Z2-R-ONLINE-048, Z2-R-ONLINE-049, Z2-R-ONLINE-051, Z2-R-ONLINE-052, Z2-R-ONLINE-137 |
| Z2-I-ONLINE-11 | account change: no cross-account leakage | Z2-R-ONLINE-022, Z2-R-ONLINE-027, Z2-R-ONLINE-032, Z2-R-ONLINE-034, Z2-R-ONLINE-036, Z2-R-ONLINE-037, Z2-R-ONLINE-038, Z2-R-ONLINE-039, Z2-R-ONLINE-040, Z2-R-ONLINE-041, Z2-R-ONLINE-042, Z2-R-ONLINE-043, Z2-R-ONLINE-044, Z2-R-ONLINE-045, Z2-R-ONLINE-052, Z2-R-ONLINE-057, Z2-R-ONLINE-105, Z2-R-ONLINE-108, Z2-R-ONLINE-115, Z2-R-ONLINE-125, Z2-R-ONLINE-134, Z2-R-ONLINE-137, Z2-R-ONLINE-141, Z2-R-ONLINE-142, Z2-R-ONLINE-146, Z2-R-ONLINE-147, Z2-R-ONLINE-148, Z2-R-ONLINE-149, Z2-R-ONLINE-150, Z2-R-ONLINE-158, Z2-R-ONLINE-159 |
| Z2-I-ONLINE-12 | cloud-save conflict resolution flows | Z2-R-ONLINE-066, Z2-R-ONLINE-067, Z2-R-ONLINE-068, Z2-R-ONLINE-069, Z2-R-ONLINE-070, Z2-R-ONLINE-071, Z2-R-ONLINE-072, Z2-R-ONLINE-087, Z2-R-ONLINE-091, Z2-R-ONLINE-107, Z2-R-ONLINE-109, Z2-R-ONLINE-138, Z2-R-ONLINE-144, Z2-R-ONLINE-151, Z2-R-ONLINE-155, Z2-R-ONLINE-161, Z2-R-ONLINE-164, Z2-R-ONLINE-165 |
| Z2-I-ONLINE-13 | Steam backend | Z2-R-ONLINE-006, Z2-R-ONLINE-009, Z2-R-ONLINE-115, Z2-R-ONLINE-116, Z2-R-ONLINE-117, Z2-R-ONLINE-118, Z2-R-ONLINE-119, Z2-R-ONLINE-120, Z2-R-ONLINE-121, Z2-R-ONLINE-122, Z2-R-ONLINE-123, Z2-R-ONLINE-124, Z2-R-ONLINE-125, Z2-R-ONLINE-135, Z2-R-ONLINE-136, Z2-R-ONLINE-152, Z2-R-ONLINE-153, Z2-R-ONLINE-156, Z2-R-ONLINE-161 |
| Z2-I-ONLINE-14 | Google Play backend | Z2-R-ONLINE-006, Z2-R-ONLINE-009, Z2-R-ONLINE-115, Z2-R-ONLINE-116, Z2-R-ONLINE-126, Z2-R-ONLINE-127, Z2-R-ONLINE-128, Z2-R-ONLINE-129, Z2-R-ONLINE-130, Z2-R-ONLINE-131, Z2-R-ONLINE-132, Z2-R-ONLINE-133, Z2-R-ONLINE-134, Z2-R-ONLINE-135, Z2-R-ONLINE-136, Z2-R-ONLINE-152, Z2-R-ONLINE-156, Z2-R-ONLINE-161 |

## Excluded and deferred

| Capability | Classification | Rationale | Decision |
|---|---|---|---|
| console platform services (PSN, Xbox network, Nintendo) | deferred to Stage 7, gated on SDK access | specified as further backends of the same interface when the SDKs are available | Z2-D-071 |
| multiplayer sessions, matchmaking, presence, friends, leaderboards, voice | excluded from ONLINE | no inventory item; networked play is NET's Stage 7 scope | Z2-D-073, Z2-D-074 |
| in-game purchase flows, consumables, virtual currencies | excluded | no inventory item; purchases made in a store reach the game as entitlements | Z2-D-074 |
| server-side purchase verification, session tickets, server authentication codes | excluded | no game server is in scope; client-side answers are accepted (security assumption S6) | Z2-D-073 |
| dynamic or downloadable backend plugins | excluded | backends are static libraries selected at link time and chosen by configuration | Z2-D-024 |
| visual authoring of the online catalogue | excluded | online services are not among the visually authored kinds; the catalogue is JSON with validation | Z2-D-079 |
| modding services (workshop content) | excluded | modding is excluded | Z2-D-075 |
