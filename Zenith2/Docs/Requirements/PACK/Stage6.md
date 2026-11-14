# PACK requirement catalogue: Stage 6

## Purpose

The second file of the PACK catalogue: the requirements whose `Stage` is S6, in the same format and the same `Z2-R-PACK-NNN` numbering as `Zenith2/Docs/Requirements/PACK.md`, which holds the purpose of the whole catalogue, the requirements of Stages 1, 2 and 4, the coverage of inventory items (over both files) and the exclusions.

## Requirements

### Z2-R-PACK-002 Public header rules and contract blocks

- **Statement:** Every ZenithPackage public header declares only the types of `Zenith2/Docs/Design/PACK/PublicAPI.md` with one contract block per type, every fallible function returning `Zenith_Result<…, Zenith_PackageError>`, and no STL, JNI, Play or platform type.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-07, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` gate steps.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Rules across the PACK API"; Zenith2/Docs/Design/PACK.md → "ZenithPackage public root headers"

### Z2-R-PACK-004 Error mapping at library boundaries

- **Statement:** Errors crossing into PACK from PLAT, the assumed update verifier and Play are mapped explicitly by the table "Error mapping at boundaries", never propagated as another enumeration, and a PACK error in the closing actions of startup steps 4 and 11 stops startup only for a Shipping build's invalid `package.zpkm` (exit code 3) or, under `--replay`, a recorded pack that cannot be mounted (exit code 5).
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_ShippingWithoutManifest_Fails`, `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Error mapping at boundaries"

### Z2-R-PACK-005 Delivery update lock row

- **Statement:** The delivery update slot table is guarded by `uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES` (650, short-hold; 640 is `uLOCK_LEVEL_ACCESSIBILITY`), whose holder allocates nothing, calls nothing and takes no other lock, which the main thread never holds while calling Java, and whose row exists in the Contracts lock table before the constant is introduced.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackDelivery_SlotTableUnderTsan` and `Test_DeliverySlotTable_CloseRefusesLaterWrites` under `linux-tsan`; the lock-order checker in every Debug run; Sanitizers step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Lock-order entries"; Zenith2/Docs/Design/PACK/Algorithms.md → "Delivery update slots"

### Z2-R-PACK-006 No thread owned by PACK

- **Statement:** ZenithPackage starts no thread: verification runs as background jobs and `Zenith_AsyncIO` operations it owns, and Play callbacks only write the slot table.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** review; `Test_AssetPackInstaller_ShutdownCancelsVerification`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Threads and background operations"; Zenith2/Docs/Design/PACK.md → "Thread affinity per type"

### Z2-R-PACK-007 Factories unwind on failure

- **Statement:** Every PACK factory (`Zenith_UpdateApplier::Create`, `Zenith_AssetPackInstaller::Create`, `Zenith_PackageService::Create`, `Zenith_CreateHostInstallFileSystem`, `Zenith_CreateAssetPackDelivery_Play`) unwinds exactly what it built when a step fails, at each fault point of the fault point table and each allocation.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_Create_FailsAtEachStep_Unwinds`, `Test_AssetPackInstaller_Create_FailsAtEachStep_Unwinds`, `Test_PackageService_Create_FailsAtEachStep_Unwinds`, `Test_HostInstallFileSystem_Create_FailsAtRootOpen_Unwinds`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Fault point table"; Zenith2/Docs/Design/PACK.md → "Allocation failure"

### Z2-R-PACK-008 Runtime bridge in ZenithRuntime

- **Statement:** PACK's runtime bridge lives in `Zenith2/Engine/ZenithRuntime/Internal/Package/` and runs pending installs as the closing action of startup step 4 (after the platform, before the virtual filesystem), creates the service as the closing action of step 11 (after the asset registry and the save system, before the RHI device), attaches each world at world creation step 4, runs prologue step 1b, applies commands at the sync point after phase 10, and shuts down at steps 4, 6 and 10b; no `Zenith_StartupStep` enumerator is added.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-09, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_StartupStepOrder`, `Test_RuntimePackage_ShutdownOrder`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Runtime bridge declarations"; Zenith2/Docs/Design/PACK.md → "Startup and shutdown"; Zenith2/Docs/Design/PACK.md → "Frame-level flow"

### Z2-R-PACK-009 Shutdown completes PACK operations

- **Statement:** Shutdown step 4 cancels every verification and unmounts every delivered pack and Windows DLC with every intent kept, then calls `PumpShutdown` (which drains the completion queue and the release blocks, since no prologue runs) with 1 ms sleeps until every verification stopped and every unmounted source was released or `package.shutdownDrainMs` passed for the releases, writes the delivery state file only when it is dirty (neither writing nor waiting for a write on RHI's device-loss clean exit) and clears the entitlement forwarder's provider before any module is destroyed; step 6 destroys the service before the job system after the Java bridge's `shutdown()` zeroed its native handle under its monitor, so no native callback runs afterwards; step 10b releases the instance guard after `Zenith_VirtualFileSystem` and before `Zenith_Platform` is destroyed.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_ShutdownCancelsVerification`, `Test_AssetPackInstaller_ShutdownWithMountedPack_ReleasesBeforeDestroy`, `Test_AssetPackInstaller_ShutdownDrain_BoundedWhenSourceHeld`, `Test_RuntimePackage_ShutdownOrder`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Startup and shutdown"; Zenith2/Docs/Design/PACK.md → "Cancellation"; Zenith2/Docs/Design/PACK/Algorithms.md → "Shutdown drain"

### Z2-R-PACK-010 Configuration keys

- **Statement:** `Zenith_RegisterPackageConfigKeys` registers exactly the keys of "Configuration keys" with their types, defaults and ranges at startup step 6, `package.simulatedDelivery` is refused in Shipping builds, and `package.reverifyOnStartup` is registered as a bool defaulting to false.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageConfigKeys_RegisteredWithRanges`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Configuration keys"

### Z2-R-PACK-017 PACK fuzz targets

- **Statement:** Each PACK parser (`package.zpkm`, the install journal, the install state, the install record, the delivery state file, the verified stamp file `verified.zinv`, the package event payload) has a libFuzzer target run as a smoke in `win-asan` and for its full duration in stage acceptance, each asserting that its decoder returns a result without a crash, a sanitizer report or an allocation beyond its documented copy (`Zenith_Fuzz_VerifiedStamps` also calling `FindStamp` on a decoded file).
- **Covers:** Z2-I-PACK-01, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_PackageManifest.Smoke`, `Zenith_Fuzz_InstallJournal.Smoke`, `Zenith_Fuzz_InstallState.Smoke`, `Zenith_Fuzz_InstallRecord.Smoke`, `Zenith_Fuzz_DeliveryState.Smoke`, `Zenith_Fuzz_VerifiedStamps.Smoke`, `Zenith_Fuzz_PackageEvent.Smoke`; Sanitizers step; Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Test targets and gate steps"; Zenith2/Docs/Design/PACK.md → "Missing and corrupt input"

### Z2-R-PACK-042 Package manifest identity

- **Statement:** `CheckIdentity` refuses a `package.zpkm` whose build ID differs from the running build's, whose content hash differs from the mount manifest's pre-patch `contentHash` (`m_ulPackageContentHash`), or whose layout does not match the platform, and the service then disables delivery and DLC for the run while the base plays; a mounted patch never makes it fail.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifest_CheckIdentity_UsesPrePatchHash`, `Test_PackageService_IdentityMismatch_DisablesDelivery`, `Test_PackageService_PatchedInstall_IdentityAndPacksValid`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Package manifest checks"; Zenith2/Docs/Design/PACK/Algorithms.md → "Package service startup"

### Z2-R-PACK-045 Windows install tree

- **Statement:** An installed Windows game keeps committed patches in `<install>/Patches/<MAJOR.MINOR.PATCH>/`, deliveries in `.incoming/`, deletions in `.trash/<generation>/`, DLC in `DLC/<name>/`, and the journal and lock in `.zenith2-install/`; nothing else of PACK's is written.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallPatch_CommitsAndCleans`, `DeviceRun_Pack_WindowsPatchAndDlc`; Test step and device run.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Windows install layout"

### Z2-R-PACK-047 Android bundle layout

- **Statement:** The bundle layout puts the base unit's archives and the mount manifest with `assetPacks` into `bundle/base/assets/` and each install-time, fast-follow, on-demand and DLC unit into `bundle/packs/<pack>/assets/`.
- **Covers:** Z2-I-PACK-05, Z2-I-PACK-06, Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageManifestWriter_MountManifestsValidate`, `CMake_Pack_BundleProjectGeneration`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Android App Bundle layout"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"

### Z2-R-PACK-049 Package build identity

- **Statement:** The packaged executable's build ID read by `Get-Zenith2BinaryIdentity` is the `buildId` of every mount manifest and of `package.zpkm`, and a package mode run without `package.buildId` exits 2.
- **Covers:** Z2-I-PACK-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_InvokePackage_BinaryIdentity` (the identity `Get-Zenith2BinaryIdentity` reads is the `package.buildId` ZenithCook receives), `Test_PackageManifestWriter_PerLayout` (that ID is the `buildId` of every mount manifest and of `package.zpkm`), `Test_PackageMode_ExitCodes` and `Pwsh_Pack_InvokePackage_BuildId` (a run without a valid build ID exits 2); Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes"

### Z2-R-PACK-051 Windows install size measured

- **Statement:** MP-PKG measures each flagship slice's Windows install size as the sum of its Shipping package tree excluding `*.pdb`, and Stage 6 acceptance judges it against its slice's row.
- **Covers:** Z2-I-PACK-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-PACK-002, Z2-B-PACK-003, Z2-B-PACK-004
- **Acceptance evidence:** the Stage 6 acceptance item "Windows install size" with its evidence record; acceptance.
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Stage 6 acceptance items"; Zenith2/Docs/Design/PACK.md → "Rows PACK owns"

### Z2-R-PACK-052 Bundle Gradle project

- **Statement:** Each game has a second Gradle project per Android preset, `android-bundle/`, generated through MOD's `_zenith_gradle_project` with the release and shipping build types, the bundle base assets, one asset-pack module per pack in unit order, `assetPacks`, ABI splits on and density and language splits off.
- **Covers:** Z2-I-PACK-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_BundleProjectGeneration`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "App Bundle build"

### Z2-R-PACK-053 Asset pack module template

- **Statement:** `Zenith2/Player/GradleTemplate/assetpack/build.gradle.kts.in` declares the `com.android.asset-pack` plugin with the pack name and delivery type install-time, fast-follow or on-demand (on-demand for DLC units).
- **Covers:** Z2-I-PACK-06, Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_BundleProjectGeneration`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "App Bundle build"

### Z2-R-PACK-054 Unsigned bundle output

- **Statement:** `<Name>_Bundle` in a Shipping configuration writes `${ZENITH_OUT}/Packages/<Name>/android/Shipping/<Name>-unsigned.aab` for SHIP to upload-sign, and a Release configuration writes the development-signed `<Name>-release.aab`.
- **Covers:** Z2-I-PACK-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_PackageTargets`; Stage 6 acceptance bundle build.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets"

### Z2-R-PACK-055 Bundle base module limit

- **Statement:** The Shipping bundle's base module download size measured by `Measure-Zenith2BundleSize` (`get-size total --modules=base`) with the S24 device spec stays within Z2-B-PACK-001; once the row is adopted `<Game>_Bundle` fails with `Invoke-Zenith2Package.ps1 -Bundle` exit code 8 on a breach, and Stage 6 acceptance blocks on it (BR-ACCEPT).
- **Covers:** Z2-I-PACK-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-027
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** `bundle-report.json` per flagship slice in Stage 6 acceptance; `Pwsh_Pack_BundleLimit_AdoptedRowRefuses`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Bundle size measurement"; Zenith2/Docs/Design/PACK.md → "Rows PACK owns"

### Z2-R-PACK-056 Play base-module limit pinned

- **Statement:** The Stage 6 PACK ticket pinning the Play base-module limit writes Z2-B-PACK-001's value, and the values of the Play pack rows once they exist, from Google Play's current documentation with its URL and retrieval date.
- **Covers:** Z2-I-PACK-06, Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-027
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** the `needs-human` producer ticket's diff to `Zenith2/Docs/Budgets.md` and its DecisionLog entry; acceptance mode of the coverage gate step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Rows PACK owns"; Zenith2/Docs/Design/PACK.md → "Pending new rows"

### Z2-R-PACK-057 Play library in bundle builds only

- **Statement:** The Play Asset Delivery Java library is an `implementation` dependency of the bundle project alone, present only when a fast-follow, on-demand or DLC pack exists, and the sideload APK resolves no Maven runtime artefact.
- **Covers:** Z2-I-PACK-06, Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Pack_BundleProjectGeneration`; SHIP's Android package checklist on the sideload APK.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "App Bundle build"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records"

### Z2-R-PACK-058 Every bundle split 16 KB aligned

- **Statement:** Every split APK generated from a Shipping bundle for the S24 device spec passes `zipalign -c -P 16 -v 4`, and every shared object in it has every PT_LOAD alignment at or above 0x4000.
- **Covers:** Z2-I-PACK-06, Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_MiniBundle_Alignment`; `Test-Zenith2ApksAlignment` on every flagship bundle in Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"; Zenith2/Docs/Design/PACK/Tests.md → "Stage 6 acceptance items"

### Z2-R-PACK-059 Delivery interface

- **Statement:** `Zenith_AssetPackDelivery` is the only path from the installer to a delivery service, with `RequestStates`, `Fetch`, `Cancel`, `Remove`, `ShowConfirmation`, `GetLocation` and `Poll`, every member on the main thread and every asynchronous outcome a polled update.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_Transitions_EveryCell`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_AssetPackDelivery header"

### Z2-R-PACK-060 Play delivery over JNI

- **Statement:** `Zenith_CreateAssetPackDelivery_Play` obtains the JavaVM, the activity and its class loader from `Zenith_Platform::GetAndroidJavaContext`, loads `org.zenith2.pack.Zenith2AssetPacks` through the class loader, registers its four natives, constructs the bridge with the native state's address and calls `init`, returning `DELIVERY_UNAVAILABLE` when the bridge or Play is absent, with no SDL call; it returns `NOT_SUPPORTED` on Windows and Linux.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_JavaBridgeMatchesJniTable`; `DeviceRun_Pack_LocalTestingS24`; Build step android-arm64.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Java bridge"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_AssetPackDelivery header"

### Z2-R-PACK-061 Java bridge contract

- **Statement:** The Java bridge is an instance exposing exactly the members of "Java bridge"; every native takes the native state's address as its first argument and is entered only inside the bridge's monitor while that address is non-zero, `shutdown()` zeroes it inside the monitor, the natives copy their arguments into cells without calling back into Java or the installer, and the native declarations equal the C++ registration table.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_JavaBridgeMatchesJniTable`, `Pwsh_Pack_JavaBridgeNativesTakeHandle`, `Test_DeliverySlotTable_CloseRefusesLaterWrites`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Java bridge"

### Z2-R-PACK-062 Play status and error mapping

- **Statement:** Play's AssetPackStatus and AssetPackErrorCode integers map to `Zenith_DeliveryStatus`, `Zenith_DeliveryErrorCode`, `Zenith_PackageError` and the installer's events exactly as the mapping tables state (WAITING_FOR_WIFI to P4, REQUIRES_USER_CONFIRMATION to P5, FAILED with NETWORK_ERROR to P7N, NETWORK_UNRESTRICTED and CONFIRMATION_NOT_REQUIRED as confirmation answers), unknown values mapping to UNKNOWN or INTERNAL_ERROR and counted.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_Transitions_EveryCell` (errors), `Test_AssetPackInstaller_InsufficientStorage_Fails`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Play status and error mapping"

### Z2-R-PACK-063 Delivery update slots

- **Statement:** Each pack has a state cell holding its latest state update and a result cell holding its one outstanding removal or request result, and the delivery has one confirmation cell, all stamped from one strictly increasing sequence; `Poll` returns written cells in ascending sequence; no update overflows, a pack's latest state and its result are never lost, and `Close` makes subsequent writes no-ops.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackDelivery_SlotTableUnderTsan`, `Test_AssetPackDelivery_RemoveResultNotCoalesced`, `Test_DeliverySlotTable_CloseRefusesLaterWrites`; Sanitizers step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Delivery update slots"

### Z2-R-PACK-064 Simulated delivery

- **Statement:** `Zenith_SimulatedAssetPackDelivery` delivers packs deterministically in `Poll` at `m_ulBytesPerPoll`, with no thread, Play's status sequence and layout, in copying mode (tests) or in place (the development runtime, writing nothing), scripted faults of every `Zenith_SimulatedFaultKind` (a network loss reported as FAILED with NETWORK_ERROR), a scripted confirmation answer, and `SimulateRequesterRestart` keeping the download in progress.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-14, Z2-I-PACK-15
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_SimulatedAssetPackDelivery_PollAdvancesDeterministically`, `Test_SimulatedAssetPackDelivery_EachFaultKind`, `Test_SimulatedAssetPackDelivery_RequesterRestartKeepsDownload`, `Test_SimulatedAssetPackDelivery_CancelKeepsPartialRemoveDeletes`, `Test_SimulatedAssetPackDelivery_InPlaceMode_WritesNothing`, `Test_SimulatedAssetPackDelivery_ConfirmationAnswer`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Simulated delivery"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_AssetPackDelivery header"

### Z2-R-PACK-065 Asset pack state machine

- **Statement:** Every (state, event) case of "Asset pack transitions" (19 states, 25 per-pack events in 23 columns, 475 cells with the P1 / P2 / P3 column expanded, which the matrix test expands by target and by each conditional cell's arms into its cases) produces its next state, target, intent, action, request result and event, every BUSY cell refuses synchronously, every `—` cell changes nothing (a request returning success, any other event counted), every non-BUSY cancel clears the intent, the shutdown rule keeps every intent and starts no verification and no mount, and the confirmation request follows its global rule.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-14, Z2-I-PACK-15, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_Transitions_EveryCell`, `Test_AssetPackInstaller_ConfirmationFlow`, `Test_AssetPackInstaller_Cancel_RollsBack`, `Test_AssetPackInstaller_UnmountingRequests`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack state machine"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack events"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-066 Happy-path delivery

- **Statement:** A requested on-demand pack passes REQUESTED, PENDING, DOWNLOADING, TRANSFERRING, VERIFYING, VERIFIED and MOUNTED, after which its files resolve through the package mount, and the installer's uncoalesced `axEventsOut` holds one `PACK_STATE_CHANGED` per transition in that order.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_FetchVerifyMount_HappyPath`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"

### Z2-R-PACK-067 Install-time and base units need no delivery

- **Statement:** Units with the PRESENT flag in the running layout (base, install-time, and on Windows and in a sideload APK every non-DLC unit) are mounted with the package at startup step 5, have no installer record and report MOUNTED; requests naming them are `NOT_DELIVERABLE`.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_NotDeliverableUnits_Refused`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack state machine"; Zenith2/Docs/Design/PACK/Formats.md → "Mount manifests PACK writes"

### Z2-R-PACK-068 Verification before mount

- **Statement:** Every archive of a delivered pack must match the base `package.zpkm`'s byte count and SHA-256, at every start, before any of the pack's archives is mounted, a size mismatch failing before any read is issued.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_CorruptDelivery_RemovedAndRefetchedOnce`, `Test_AssetPackInstaller_TruncatedDelivery_FailsSizeCheck`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"

### Z2-R-PACK-069 Verification pipeline bounds

- **Statement:** Verification reads through `Zenith_AsyncIO` at background priority into a fixed arena of concurrency × buffers × chunk bytes, hashes on background jobs one per archive at a time, and verifies at most `package.maxConcurrentVerifications` packs at once.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_VerificationConcurrencyBound`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline"; Zenith2/Docs/Design/PACK.md → "In-memory layouts that matter"

### Z2-R-PACK-070 Verification throughput floor

- **Statement:** Asset pack verification on the S24 and the reference desktop meets the verification throughput floor row, measured by MP-PKGVERIFY.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the MP-PKGVERIFY evidence record per platform in Stage 6 acceptance (pending row, allocated by the Stage 1 per-system budget ticket for PACK).
- **Design:** Zenith2/Docs/Design/PACK.md → "Pending new rows"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline"

### Z2-R-PACK-071 Verified cache for delivered packs

- **Statement:** A delivered pack verified in full has its delivery-state record written with verified 1, Play's pack version, the base `package.zpkm`'s content hash, the location digest and every archive's identity stamp (size, last-write and change time, volume and file identity, taken before the first read and compared after the last, a difference being V2); at a later VFY, unless `package.reverifyOnStartup` is set, the pack goes to VERIFIED without any read when the record matches on every field and every archive's current stamp equals its record in all five fields, counting `package.verify.cache_hits`, and otherwise runs the full pipeline; the record is cleared on entering REMOVING, NOT_INSTALLED or FAILED, at V2 and at a P6 of a new pack version (Q90 ruled (a)).
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_VerifiedCache_SkipsHashing`, `Test_AssetPackInstaller_ReverifyOnStartup_AlwaysHashes`, `Test_AssetPackInstaller_ModifiedDuringVerify_Reverifies`, `Test_AssetPackInstaller_RestoredMtime_Reverifies`, `Test_AssetPackInstaller_VerifiedRecordCleared`, `Test_AssetPackInstaller_FetchVerifyMount_HappyPath` (the record written); Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state records"

### Z2-R-PACK-072 Mounting a verified pack

- **Statement:** A verified pack's archives are mounted in archive order through a container wrapper co-owning the pack's release block, ASSET's archive factory and `AddSource(MOUNT_KIND_PACKAGE, …, unit order)` in the frame prologue, a failure unmounting the archives already added, and every mount and every release triggering `OnMountsChanged`.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_FetchVerifyMount_HappyPath`, `Test_PackageIntegration_AssetPackDeliveredLoads`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"

### Z2-R-PACK-073 Removal after release

- **Statement:** A mounted pack's files are deleted only after `RemoveSource` of every archive and the destruction of every container wrapper, so in-flight reads complete with the original bytes.
- **Covers:** Z2-I-PACK-15, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_RemoveMounted_WaitsForRelease`, `Test_PackageIntegration_RemovePackWhileStreaming`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"

### Z2-R-PACK-074 Delivery state file

- **Statement:** `user:/cache/package/delivery-state.zdls` records each pack's intent and, for a pack verified in full, its verified flag, pack version, manifest content hash, location digest and one 48-byte stamp per archive (checks D1–D5, D4 including the verified-record and stamp-range rules); it is format version 1, written with `WriteAtomic` at most once per frame, and a refused file restarts from an empty state, re-verifying every pack in full.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_DeliveryStateFile_RoundTrip`, `Test_DeliveryStateFile_EachCheckRejects`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Delivery state file"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state header"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state records"

### Z2-R-PACK-075 Fetch policy

- **Statement:** Fast-follow packs not started by Play are fetched once per process when `package.autoFetchFastFollow` is true, persisted intents are resumed at startup (never under `--replay`), a cancelled pack's intent is cleared so it is never resumed, automatic re-fetches stop after attempt 3 and network retries after 3 per process, a game request clears both, and one prologue's requests go to Play in one call in unit order.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_AutoFastFollow_FetchedOnce`, `Test_AssetPackInstaller_AutoRefetchStopsAfterThreeAttempts`, `Test_AssetPackInstaller_Cancel_RollsBack`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Fetch policy"

### Z2-R-PACK-076 Initial query

- **Statement:** The service's Create waits at most `package.initialQueryTimeoutMs` for Play's initial pack states, so delivered packs start verification as early as possible; a state that arrives after the wait follows the transition tables.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_ProcessDeathMidDownload_Resumes`, `Test_AssetPackInstaller_FetchVerifyMount_HappyPath`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Initial query"

### Z2-R-PACK-077 User confirmation

- **Statement:** A pack in WAITING_FOR_WIFI or WAITING_FOR_CONFIRMATION queues one `Package.ConfirmationRequired` with reason Cellular or Consent; `RequestConfirmation` issues the delivery's `ShowConfirmation` (Play's `showConfirmationDialog`) once while any pack waits in either state, is `PACK_BUSY` otherwise, and a declined answer leaves the states unchanged.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_ConfirmationFlow`, `Test_AssetPackInstaller_WaitingStatesSwap`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-078 Insufficient storage

- **Statement:** Play's insufficient-storage failure leaves the pack FAILED with `PACKAGE_ERROR_INSUFFICIENT_STORAGE` reported to the game, and a new request starts a new attempt.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_InsufficientStorage_Fails`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Play status and error mapping"; Zenith2/Docs/Design/PACK.md → "Low disk"

### Z2-R-PACK-079 Entitlement-gated packs

- **Statement:** Outside `--replay` (where Z2-R-PACK-183 applies), a DLC pack is fetched and mounted only while `Zenith_EntitlementQuery` answers GRANTED: DENIED refuses a game request, UNKNOWN holds the pack in AWAITING_ENTITLEMENT until the answer arrives (the withheld fetch sets and persists intent `WANT_INSTALLED` without issuing a fetch or counting an attempt, and the grant's fetch counts as a game request exactly when the held one was), a revocation unmounts or stops the pack into AWAITING_ENTITLEMENT with its files kept, and a subsequent grant resumes it.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_Entitlement_Gates`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header"

### Z2-R-PACK-080 Entitlement query interface

- **Statement:** `Zenith_EntitlementQuery` is a lower-declared interface implemented by ONLINE, by `Zenith_ConfigEntitlementQuery`, which grants exactly the IDs of `package.grantedEntitlements`, and by `Zenith_ForwardingEntitlementQuery`, which the service is given and which answers UNKNOWN until the runtime bridge sets a provider.
- **Covers:** Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigEntitlementQuery_GrantsListedOnly`, `Test_ForwardingEntitlementQuery_RevisionPerProviderChange`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header"; Zenith2/Docs/Design/PACK.md → "Upward interfaces PACK declares"

### Z2-R-PACK-081 Stale updates dropped

- **Statement:** A delivery update whose sequence is not newer than the last one applied to the same cell of its pack is dropped and counted in `package.delivery.stale_updates`.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_StaleSequenceDropped`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK.md → "Counters"

### Z2-R-PACK-082 Sideload builds have no delivery

- **Statement:** A build whose `package.zpkm` has no deliverable unit (the WINDOWS and ANDROID_SIDELOAD layouts) creates no delivery and no installer and reports every unit MOUNTED, and a bundle build without the Java bridge leaves its deliverable packs FAILED with `DELIVERY_UNAVAILABLE`, logged once.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_FoldedUnitsReportMounted`, `Test_AssetPackInstaller_Transitions_EveryCell`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Sideload builds"

### Z2-R-PACK-083 Pack events reach simulation at tick boundaries

- **Statement:** Pack, update and DLC changes reach worlds only as `ASYNC_SOURCE_PACKAGE` results of `PUBLICATION_KIND_GENERIC_RECORDED`, allocated and completed in the frame prologue, published at the world's tick boundary in completion-ID order with every other source, decoded by the world record's handler and delivered in phase 9; at most one pending error-free state change per unit is queued, an event carrying an error is never coalesced, and a paused world's excess events wait in its deferral queue.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_EventsPublishedAtTickBoundary`, `Test_RuntimePackage_PublicationOrderAcrossSources`, `Test_RuntimePackage_PausedWorldDefersEvents`, `Test_RuntimePackage_ApplyTimeRefusalReported`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Event publication and replay"; Zenith2/Docs/Design/PACK/Formats.md → "Package event payload"

### Z2-R-PACK-084 Replay reproduces pack events

- **Statement:** Under `--replay` the bridge allocates no completion ID for live events, takes no snapshot of its own and applies no `Package.*` command, the recorded `ASYNC_SOURCE_PACKAGE` publications (every world's snapshot included) are published at their recorded ticks with no delivery present, and `Package.GetPackState` returns the recorded values.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_ReplayPublishesRecorded`, `Test_RuntimePackage_EventPayloadRoundTrip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Event publication and replay"

### Z2-R-PACK-085 Script-facing package API

- **Statement:** Scripts read pack states through `Package.GetPackState`, `GetPackProgress`, `GetPackSizeBytes`, `IsMounted`, `ListPacks` and `GetUpdateKind`, and request through the buffered commands `Package.RequestPack`, `CancelPack`, `RemovePack` and `Confirm`, which a rolled-back invocation never issues; a refusal found when a command is applied arrives as `Package.PackStateChanged` with the error.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_CommandsAppliedAfterCommit`, `Test_RuntimePackage_ApplyTimeRefusalReported`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Script-facing API"

### Z2-R-PACK-086 Package misuse codes

- **Statement:** Package misuse raises controlled Luau errors ZS2601–ZS2603 with the catalogued messages, never an assert, in PACK's reserved block ZS2600–ZS2699, and a call from a context its row does not list raises the binding ABI's ZS1702.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_MisuseCodes`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Script-facing API"; Zenith2/Docs/Design/PACK.md → "Luau errors versus asserts"

### Z2-R-PACK-087 Event queue bound

- **Statement:** The package event queue holds 512 events with `QUEUE_FULL_POLICY_DROP_OLDEST` and the counters `package.events.full`, `.dropped` and `.high_water`, and each world holds at most 511 outstanding package publications, further events waiting in its bounded deferral queue (`package.events.deferred`).
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_EventsPublishedAtTickBoundary`, `Test_RuntimePackage_PausedWorldDefersEvents`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Event publication and replay"; Zenith2/Docs/Design/PACK.md → "Counters"

### Z2-R-PACK-088 bundletool pinned

- **Statement:** bundletool is a pinned `host-tool` jar of the third-party manifest run only by the pinned JDK through `Zenith2Package.psm1`, never linked.
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** SHIP's manifest validation; `Pwsh_Pack_MiniBundle_ValidateAndSplits`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"; Zenith2/Docs/Design/PACK.md → "Assumed interfaces pending other areas"

### Z2-R-PACK-089 Test-time mini bundle

- **Statement:** `New-Zenith2MiniBundle` builds an App Bundle with install-time, fast-follow and on-demand packs from generated text inputs through `aapt2 link --proto-format` and `bundletool build-bundle`, with no Gradle and no committed binary.
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_MiniBundle_ValidateAndSplits`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "PowerShell functions"; Zenith2/Docs/Design/PACK/Tests.md → "Fixtures"

### Z2-R-PACK-090 Split generation for the S24

- **Statement:** `build-apks` with `s24-ultra.json` yields `base-master`, `base-arm64_v8a` and one master slice per asset pack and no density or language split.
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_MiniBundle_ValidateAndSplits`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"; Zenith2/Docs/Design/PACK/Formats.md → "Bundle configuration and device specs"

### Z2-R-PACK-091 Local-testing APK sets

- **Statement:** `New-Zenith2DeviceApks -LocalTesting` produces an APK set in bundletool's local-testing mode for the S24 and the emulator specs.
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_MiniBundle_LocalTesting`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Local testing and the emulator smoke"

### Z2-R-PACK-092 Bundle size measurement

- **Statement:** `Measure-Zenith2BundleSize` writes `bundle-report.json` with the base module's (`--modules=base`), the install-time total's and each pack's maximum download size from `bundletool get-size total`, the procedure Z2-B-PACK-001 and the pack rows use.
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** `Pwsh_Pack_MiniBundle_GetSize`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Bundle size measurement"

### Z2-R-PACK-093 Emulator smoke

- **Statement:** The `android-x86_64` Release bundle installed with local testing on the emulator requests an on-demand pack and reaches MOUNTED, as a smoke from which no acceptance result is taken.
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `EmulatorSmoke_Pack_LocalTesting` run record (`needs-human`).
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Local testing and the emulator smoke"; Zenith2/Docs/Design/PACK/Tests.md → "Device runs"

### Z2-R-PACK-094 S24 local-testing run

- **Statement:** The Release bundle installed on the S24 with local testing runs the automated pack script to completion; the Shipping bundle's pack delivery is proven by the S24 record of Z2-R-PACK-154 and Z2-R-PACK-155 (`DeviceRun_Pack_Play*`).
- **Covers:** Z2-I-PACK-08
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `DeviceRun_Pack_LocalTestingS24` run record (`needs-human`).
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Device runs"

### Z2-R-PACK-095 Install record

- **Statement:** `user:/cache/package/install-record.zinr` stores the running identity, the previous identity, the last update kind, the migration phase, the highest patch seen and the run count in 192 bytes of format version 1 (checks R1–R5; release line 0.0 valid), written with `WriteAtomic`.
- **Covers:** Z2-I-PACK-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallRecord_RoundTrip`, `Test_InstallRecord_EachCheckRejects`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Install record"; Zenith2/Docs/Design/PACK/Formats.md → "Identity encoding"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallRecord header"

### Z2-R-PACK-096 Update classification

- **Statement:** `Zenith_ClassifyUpdate` returns FIRST_RUN, NONE, DOWNGRADE, NEW_LINE, WITHIN_LINE or CONTENT by the first matching row of "Update classification".
- **Covers:** Z2-I-PACK-09, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallRecord_ClassifyUpdate_EveryRow`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update classification"

### Z2-R-PACK-097 Migration journaled in the record

- **Statement:** A changed identity writes the record with phase PENDING before any step runs and phase DONE after, so a kill during migration re-runs the steps at the next start, and a first run writes DONE and runs no step.
- **Covers:** Z2-I-PACK-09, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_Update_RunsMigrationOnce`, `Test_PackageService_KillDuringMigration_ReRunsSteps`, `Test_PackageService_FirstRun_WritesRecordNoMigration`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Installed-update migration"

### Z2-R-PACK-098 Update migration registry

- **Statement:** `Zenith_UpdateMigrationRegistry` accepts at most 64 uniquely named steps before Freeze, runs them in (order, name) order for the kinds in their mask, and on a step failure runs its fallback (deleting its `user:/cache/<dir>/`) and continues.
- **Covers:** Z2-I-PACK-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateMigrationRegistry_RegisterRules`, `Test_UpdateMigrationRegistry_RunOrderAndKinds`, `Test_UpdateMigrationRegistry_FailingStepRunsFallback`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_UpdateMigration header"; Zenith2/Docs/Design/PACK/Algorithms.md → "Installed-update migration"

### Z2-R-PACK-099 Built-in migration steps

- **Statement:** PACK registers exactly three built-in migration steps, `Package.ClearVerifiedCache`, `Package.ForgetRemovedUnits` and `Package.ReportUpdate`, with the orders and kinds of "Built-in migration steps", each idempotent.
- **Covers:** Z2-I-PACK-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateMigration_BuiltInSteps`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Built-in migration steps"

### Z2-R-PACK-100 Package.Updated event

- **Statement:** An update other than NONE and FIRST_RUN reaches every world as exactly one `Package.Updated` with the kind and the from and to versions, published in the world's snapshot at its first tick boundary, and the next run with the same identity publishes none.
- **Covers:** Z2-I-PACK-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageIntegration_UpdateMigrationAcrossRuns`, `Test_RuntimePackage_LateWorldReceivesSnapshot`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Installed-update migration"; Zenith2/Docs/Design/PACK/Algorithms.md → "World snapshot"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Script-facing API"

### Z2-R-PACK-101 Packs re-verified after an update

- **Statement:** After a WITHIN_LINE, NEW_LINE, DOWNGRADE or CONTENT update every delivered pack is re-verified in full against the new `package.zpkm` before it is mounted, because `Package.ClearVerifiedCache` clears every pack's verified record (and a record's manifest content hash would differ anyway), and records of packs no longer in the manifest are dropped by `Package.ForgetRemovedUnits`.
- **Covers:** Z2-I-PACK-09, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateMigration_BuiltInSteps`; `DeviceRun_Pack_PlayInstalledUpdate`.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Built-in migration steps"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"

### Z2-R-PACK-102 Play installed-update device run

- **Statement:** An S24 updated from release N to N + 1 through a Play internal test track publishes `Package.Updated` (WithinLine), re-delivers and verifies its fast-follow packs, and loads release N's saves through SAVE's migration.
- **Covers:** Z2-I-PACK-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `DeviceRun_Pack_PlayInstalledUpdate` run record with SAVE's `Test_SaveDevice_InstalledUpdateMigration` (`needs-human`).
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Device runs"

### Z2-R-PACK-103 Update manifest format

- **Statement:** `update.zman` and `dlc.zman` share format version 1: a 96-byte header, 64-byte file entries sorted by name, a string table and a trailing pure Ed25519 signature over every preceding byte, with `total bytes = 96 + 64F + S + 64`.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateManifestWriter_BodyLayout`, `Pwsh_Pack_SignVerifyRoundTrip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Update manifest"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest header"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest tables and signature"

### Z2-R-PACK-104 Update manifest checks

- **Statement:** A bundle is accepted only when checks U0–U11 pass, with the signature checked before any field beyond the sizes is interpreted and the bundle's contents (U0) checked before any listed file is hashed; an unlisted entry, a subdirectory or a link refuses the bundle, a missing listed file leaves it in place (`UPDATE_INCOMPLETE`), and each failure is mapped to its applier error.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`, `Test_UpdateApplier_UnlistedEntry_Refused`, `Test_UpdateApplier_MissingListedFile_LeftInPlace`, `Pwsh_Pack_SignVerifyRoundTrip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"

### Z2-R-PACK-105 Signing hand-off

- **Statement:** The update-manifest writer obtains the Ed25519 seed only through SHIP's `Get-Zenith2UpdateSigningKey`, passes it to the signer on standard input, never on a command line or in a file, and zeroes it in a `finally` block.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_SignerSeedNeverOnCommandLine`, `Pwsh_Pack_SignVerifyRoundTrip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Ed25519 signer"

### Z2-R-PACK-106 Ed25519 signer

- **Statement:** `Ed25519Sign.java`, run by the pinned JDK, reads exactly one line of 64 lower-case hex digits and an LF from standard input (SHIP's convention), signs the body with JDK 17's Ed25519, prints 128 lower-case hex characters and an LF, zeroes the seed, and exits 3 on any other input line and 4 on any exception; it has no key-generation mode.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_Signer_Rfc8032Test1`, `Pwsh_Pack_SignVerifyRoundTrip`, `Pwsh_Pack_SignerSeedNeverOnCommandLine`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Ed25519 signer"

### Z2-R-PACK-107 Patch builder

- **Statement:** `New-Zenith2Patch.ps1` refuses release pairs differing in game, release line, version order, Windows build ID, engine cook version or composition hash (exit 2) and otherwise writes, signs, verifies and zips a patch bundle with the exit codes of "Patch builder".
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_PatchBuilder_Refusals`, `Pwsh_Pack_PatchBuilder_EndToEndUnsigned`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "Patch builder"

### Z2-R-PACK-108 Entry-granular patch diff

- **Statement:** A patch contains exactly the entries of the new package whose path is new or whose data hash differs from the base package's, never `Package/package.zpkm` (skipped and reported), chunked into `patch-NN.zpak`, with the base package's content hash as its base hash and the new package's catalogue content hash as its result.
- **Covers:** Z2-I-PACK-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PatchDiff_ChangedEntriesOnly`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"

### Z2-R-PACK-109 Cumulative patches against the base

- **Statement:** Every patch of a release line is built against the line's base package, never against another patch, because the patch mount mounts one version.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallPatch_CommitsAndCleans`, `Test_PackageIntegration_PatchInterruptedThenMounted`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"

### Z2-R-PACK-110 Patch refusals at build time

- **Statement:** The patch diff refuses a changed build ID, release line, cook version, composition, a version not newer, a file of class `migrate` or `user-state` or a `.zman`, and an empty patch, writing nothing.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PatchDiff_EachRefusal`, `Test_PatchDiff_ForbiddenAndEmpty`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"; Zenith2/Docs/Design/PACK/Tooling.md → "ZenithCook package modes"

### Z2-R-PACK-111 Patch directory name equals its version

- **Statement:** A patch is installed and mounted only from a directory whose name equals the manifest's `MAJOR.MINOR.PATCH`, so a renamed older patch is refused.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`; PLAT's `Test_PatchMount_TamperCasesRejected`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"

### Z2-R-PACK-112 Effective content hash after a patch

- **Statement:** With a patch mounted, the running identity's effective content hash (`m_ulContentHash`) is the patch's result content hash, used by the install record and update classification, while `m_ulPackageContentHash` keeps the base package's hash for `CheckIdentity`, pack verification and U11.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageIntegration_PatchInterruptedThenMounted`, `Test_PackageService_PatchedInstall_IdentityAndPacksValid`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Assumed interfaces pending other areas"; Zenith2/Docs/Design/PACK/Algorithms.md → "Package service startup"

### Z2-R-PACK-113 Patch size reported

- **Statement:** Each patch build reports its included and orphaned paths and its byte totals in `patch-report.json`, the measurement of the patch download size row.
- **Covers:** Z2-I-PACK-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_PatchBuilder_EndToEndUnsigned`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"; Zenith2/Docs/Design/PACK.md → "Pending new rows"

### Z2-R-PACK-114 DLC bundles

- **Statement:** A Windows DLC bundle is a directory `<name>` holding `dlc.zman` (kind DLC, target build all zero, the release line, the cook version, the revision) and its unit's archives, the first holding its catalogue fragment.
- **Covers:** Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageIntegration_DlcMountedWhenEntitled`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Patch and DLC bundles"; Zenith2/Docs/Design/PACK/Tooling.md → "DLC builder"

### Z2-R-PACK-115 DLC builder

- **Statement:** `New-Zenith2Dlc.ps1` requires the release identity to equal the unsigned body's, writes the revision field, signs, verifies with an all-zero build ID and zips the bundle.
- **Covers:** Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_SignVerifyRoundTrip` (DLC kind), `DeviceRun_Pack_WindowsPatchAndDlc`; Test step and device run.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "DLC builder"

### Z2-R-PACK-116 Windows DLC mounting

- **Statement:** At the close of startup step 11 and at later prologues each `DLC/<name>/` is checked (signature, targets, U6, U0 through the install file system, then U9 with the bridge's stamp lookup, re-hashing only a file whose stamp, record or manifest digest differs) and mounted at its unit's order only when every check passes, its unit exists with kind DLC and the same order, and its entitlement is GRANTED (under `--replay`, exactly the recording's mounted DLC at its recorded revision, Z2-R-PACK-183); otherwise it stays unmounted, nothing under `DLC/` is deleted, and the DLC's state reports why; a revocation unmounts it at the next prologue.
- **Covers:** Z2-I-PACK-11, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_DlcMount_Cases`, `Test_PackageService_DlcMount_StampSkip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Windows DLC mounting"

### Z2-R-PACK-117 DLC fragment must resolve

- **Statement:** A DLC whose catalogue fragment has an unresolvable external reference is unmounted when `Zenith_AssetRegistry::GetCatalogueRefreshResult` names its fragment, and reported `DLC_INCOMPATIBLE`.
- **Covers:** Z2-I-PACK-11, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_DlcFragmentUnresolved_Unmounted`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Windows DLC mounting"

### Z2-R-PACK-118 DLC adds no schemas or modules

- **Statement:** A DLC unit that would need a data-defined schema or a script module absent from the base stops the package build with `DLC_SCHEMA_CHANGE`.
- **Covers:** Z2-I-PACK-11, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackagePartition_DlcIntegrityErrors`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records"

### Z2-R-PACK-119 DLC revisions and removal

- **Statement:** Installing a DLC revision replaces the previous one only through the journaled move-out, move-in sequence, a lower revision is refused, and `BeginRemoveDlc` deletes `DLC/<name>` through the REMOVING journal for tests and distributor tools; the runtime never removes a Windows DLC, which is the distributor's job.
- **Covers:** Z2-I-PACK-11, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_DlcReplace_PowerLossAtEveryStep_Converges`, `Test_UpdateApplier_DlcRemove_Converges`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation"

### Z2-R-PACK-120 Compatibility matrix

- **Statement:** Full releases, content patches, DLC revisions and asset packs may change exactly what the table "Update compatibility rules" allows, and every disallowed change is refused at build time or at install or mount time with its error.
- **Covers:** Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PatchDiff_EachRefusal`, `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`, `Test_PackageService_DlcMount_Cases`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"

### Z2-R-PACK-121 Engine cook version constant within a line

- **Statement:** A release whose engine cook version differs from its release line's earlier releases is refused by the patch builder with `PATCH_COOK_VERSION_CHANGED_IN_LINE`, and a bundle cooked for another cook version is refused at install (U6).
- **Covers:** Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PatchDiff_EachRefusal`, `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"

### Z2-R-PACK-122 Content patches keep the executable

- **Statement:** A content patch targets exactly the build ID of the executable it applies to; the patch builder refuses a different build ID and code changes ship as full releases.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_PatchBuilder_Refusals`, `Test_PatchDiff_EachRefusal`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records"

### Z2-R-PACK-123 No downgrade through patches

- **Statement:** An incoming patch whose version is not greater than every committed version of its line and than the install state's highest patch of the line is refused (U10), even after newer version directories were removed, while a full-release downgrade is accepted and classified DOWNGRADE.
- **Covers:** Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`, `Test_UpdateApplier_ReplayedOlderPatch_RefusedAfterNewerRemoved`, `Test_InstallRecord_ClassifyUpdate_EveryRow`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update classification"

### Z2-R-PACK-124 Release line change obsoletes patches

- **Statement:** After a full release of a new line, every patch of another line is obsolete (O1) and deleted by the next start's sweep or install, and the patch mount never mounts it.
- **Covers:** Z2-I-PACK-12, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallPatch_CommitsAndCleans`, `Test_UpdateApplier_ObsoleteOtherBuild_Swept`, `Test_UpdateApplier_InstallIncomingOff_RecoveryOnly`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Obsolete directories"

### Z2-R-PACK-125 Install file system interface

- **Statement:** `Zenith_InstallFileSystem` provides the stat, list, open, read, write, truncate, flush, rename, remove (file, link, empty directory), directory, free-space, volume-kind and two-lock operations of its header with install-relative path validation, reports links without following them, and is implemented for the host and in memory.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_HostInstallFileSystem_RenameFlushLock`, `Test_MemoryInstallFileSystem_PathRules`, `Test_MemoryInstallFileSystem_Links_ReportedNotFollowed`, `Test_MemoryInstallFileSystem_ProcessViews_LockModes`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallFileSystem header"

### Z2-R-PACK-126 Host install file system durability

- **Statement:** The host implementations make data durable with `FlushFileBuffers` or `fsync`, names durable by flushing every affected parent directory (on Windows through a handle opened `GENERIC_READ | GENERIC_WRITE` with `FILE_FLAG_BACKUP_SEMANTICS`, never relying on `MOVEFILE_WRITE_THROUGH`), rename with `MoveFileExW` or `renameat`, never follow a link, retry Windows sharing and access refusals three times 100 ms apart, take shared and exclusive locks on `.zenith2-install/lock` and `running`, and refuse paths escaping the root.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_HostInstallFileSystem_RenameFlushLock`, `Test_HostInstallFileSystem_PathEscapeRefused`, `Test_HostInstallFileSystem_VolumeKind`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "ZenithPackage platform files"; Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"

### Z2-R-PACK-127 Memory install file system durability model

- **Statement:** `Zenith_MemoryInstallFileSystem` models an ordered metadata log: every metadata operation is atomic, a `FlushDirectory` makes every earlier one durable, `SimulatePowerLossAfterMetadata(k)` keeps exactly a prefix of at least the durable ones and every file's last flushed bytes, `SimulateProcessKill` keeps every change and releases handles and every process view's locks, and `FailAtOperation` fails one numbered operation changing nothing; no state lists one node under two names.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryInstallFileSystem_PowerLoss_DropsUnflushed`, `Test_MemoryInstallFileSystem_ProcessKill_KeepsWrites`, `Test_MemoryInstallFileSystem_RenameAtomicAcrossPowerLoss`, `Test_MemoryInstallFileSystem_PowerLossAfterMetadata_Prefix`, `Test_MemoryInstallFileSystem_FailAtOperation_ChangesNothing`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallFileSystem header"

### Z2-R-PACK-128 Install journal format

- **Statement:** `install-journal.zinj` records the phase, target kind, generation, version, name, flags, rejection error and 0 … 1,024 move records of the one operation in progress, in format version 1 with checks J1–J6.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallJournal_RoundTrip`, `Test_InstallJournal_EachCheckRejects`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Install journal"; Zenith2/Docs/Design/PACK/Formats.md → "Install journal header"; Zenith2/Docs/Design/PACK/Formats.md → "Install journal records"

### Z2-R-PACK-129 Durable step rules

- **Statement:** Every applier mutation is one row of "Durable step rules" (bundle seal, journal rewrite, install state rewrite, directory move, directory ensure, trash deletion, journal deletion), and `Step` performs exactly one.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_PowerLossAtEveryStep_Converges`, `Test_UpdateApplier_FlushesEveryAffectedParent`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Durable step rules"

### Z2-R-PACK-130 Installation renames and never copies

- **Statement:** An incoming bundle is sealed and verified read-only in place and committed by one atomic directory rename, so the applier writes no bundle byte.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallPatch_CommitsAndCleans` (operation log has no file write outside the journal); Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Install state machine"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records"

### Z2-R-PACK-131 Install phases

- **Statement:** The applier moves only along the phases COMMITTING, COMMITTED, CLEANING, REJECTING and REMOVING of "Install phases and events" in the step sequences of "Install steps per operation".
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallPatch_CommitsAndCleans`, `Test_UpdateApplier_DlcReplace_PowerLossAtEveryStep_Converges`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Install phases and events"; Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation"

### Z2-R-PACK-132 Recovery table

- **Statement:** `Recover` handles every row of "Recovery table", including a move made before its journal rewrite, a vanished bundle, an inconsistent journal and a corrupt journal rebuilt from the directories present, and leaves no journal and no trash tree.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_CorruptJournal_RebuildsFromDirectories`, `Test_UpdateApplier_InconsistentJournal_Rebuilds`, `Test_UpdateApplier_MoveBeforeJournalRewrite_Recovered`, `Test_UpdateApplier_PowerLossAtEveryStep_Converges`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"

### Z2-R-PACK-133 Power loss at every step converges

- **Statement:** A power loss after any operation of a patch install, with metadata surviving to any point at or beyond the last flush, then the next pass, reaches the uninterrupted run's final tree, and at every point the version the patch mount would mount is the version before the install or the one it installs.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_PowerLossAtEveryStep_Converges`, `Test_UpdateApplier_UnsealedBundle_PowerLossKeepsOldPatch`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"

### Z2-R-PACK-134 Process kill at every step converges

- **Statement:** A process kill after any operation of a patch install, then recovery, reaches the uninterrupted run's final tree.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_ProcessKillAtEveryStep_Converges`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"

### Z2-R-PACK-135 Recovery is idempotent

- **Statement:** A power loss at any operation of a recovery run followed by recovery again converges on the same final state.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_RecoveryInterruptedAtEveryStep_Converges`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"; Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"

### Z2-R-PACK-136 Pending installs at startup

- **Statement:** `Zenith_RunPendingInstalls` costs 12 file-system calls and writes nothing when nothing is pending, otherwise takes both locks, recovers, sweeps obsolete patches when the build changed, installs incoming patches in descending version order (none while the base package's content hash is unknown) and incoming DLC in name order, writes the install state, hands the instance guard over to a shared lock, and returns its report.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_NothingPending_NoWrites`, `Test_UpdateApplier_HighestIncomingWins`, `Test_UpdateApplier_UnknownBaseHash_LeavesPatches`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"

### Z2-R-PACK-137 Partial deliveries ignored

- **Statement:** Names under `.incoming` that start with `.` or are not valid version or DLC names are never touched, and a bundle with a listed file absent is left in place, so a bundle is never installed half-delivered.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_IgnoresPartialDeliveryNames`, `Test_UpdateApplier_MissingListedFile_LeftInPlace`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK/Tooling.md → "Windows install conventions"

### Z2-R-PACK-138 Refused bundles removed and reported

- **Statement:** A bundle failing any refusing check is moved to `.trash` and deleted through the REJECTING journal and reported to every world once, as `Package.UpdateFailed` in its snapshot (at most 16 per start), and nothing else changes; a bundle missing a listed file or met while the base identity is unknown is left in place instead.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation"

### Z2-R-PACK-139 Obsolete patches removed

- **Statement:** A committed patch makes every lower patch of its line, every patch of another line and every patch of another build obsolete, and a start whose build differs from the install state's swept build sweeps them without an install, both through journaled moves of at most 1,024 directories per journal, any excess being swept at later starts.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallPatch_CommitsAndCleans`, `Test_UpdateApplier_ObsoleteOtherBuild_Swept`, `Test_UpdateApplier_ObsoleteBeyondJournalLimit_Deferred`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Obsolete directories"

### Z2-R-PACK-140 Install lock and instance guard

- **Statement:** Only one process installs, recovers or sweeps an install at a time (`INSTALL_LOCK_APPLY`), and none does while another instance of the install runs (`INSTALL_LOCK_RUNNING`, held shared by every instance from startup step 4 to shutdown step 10b); a process that cannot take `INSTALL_LOCK_APPLY` (a timeout or a read-only root) touches nothing and waits, without an upper bound and logging every 5 s, until it holds `INSTALL_LOCK_RUNNING` shared, so it never reaches startup step 5 while another process installs; only an unsupported volume and a read-only root without a `running` file run without the guard, each logged; the applier refuses to exist without both locks.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_LockHeld_ReturnsLocked`, `Test_UpdateApplier_TimedOutWaiterNeverRunsGuardless`, `Test_UpdateApplier_RunningInstanceBlocksInstall`, `Test_UpdateApplier_GuardHandOverIsExclusiveSafe`, `Test_UpdateApplier_Create_RequiresBothLocks`, `Test_UpdateApplier_ReadOnlyRoot_LeavesIncoming`, `Test_HostInstallFileSystem_RenameFlushLock`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Install lock and instance guard"; Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"

### Z2-R-PACK-141 Low disk and read-only root during install

- **Statement:** Below the applier's free-space margin no install and no sweep starts while recovery still runs, and a read-only root installs nothing and holds the instance guard through a read-only shared lock of an existing `running` file; both are logged and the current content plays.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_LowDisk_StartsNothing`, `Test_UpdateApplier_ReadOnlyRoot_LeavesIncoming`, `Test_MemoryInstallFileSystem_ReadOnlyRoot_SharedLockOnly`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK.md → "Low disk"

### Z2-R-PACK-142 Installs before the virtual filesystem

- **Statement:** Pending installs run as the closing action of startup step 4, writing only under `<install>/Patches/`, `<install>/DLC/` and `<install>/.zenith2-install/`, before the patch mount verifies and mounts at step 5.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_StartupStepOrder`, `Test_PackageIntegration_PatchInterruptedThenMounted`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK.md → "Design rulings PACK records"

### Z2-R-PACK-143 Android takes updates through Play

- **Statement:** Every Android build (Debug, Release and Shipping) ignores incoming patch and DLC bundles and only recovers and sweeps; Windows builds install incoming bundles; no tool of the programme produces an Android patch bundle.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_InstallIncomingOff_RecoveryOnly`, `Test_RuntimePackage_AndroidShippingInstallsNoIncoming`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Android content updates"; Zenith2/Docs/Design/PACK/Tooling.md → "Patch builder"

### Z2-R-PACK-144 Interrupted patch never mounted partly

- **Statement:** After an install interrupted at any durable step and recovered, the virtual filesystem resolves every patched file consistently to the old package entry or to the new patch entry.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageIntegration_PatchInterruptedThenMounted`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"

### Z2-R-PACK-145 Windows patch and DLC device run

- **Statement:** On the reference desktop a signed patch and DLC dropped into `.incoming` are installed at launch, a second instance launched while the first runs installs nothing, and a process killed during a further install is recovered at the next launch.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `DeviceRun_Pack_WindowsPatchAndDlc` run record (`needs-human`).
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Device runs"

### Z2-R-PACK-146 Download interrupted by network loss resumes

- **Statement:** A pack download whose Play status becomes FAILED with NETWORK_ERROR waits in WAITING_FOR_NETWORK without mounting and is re-fetched after `package.networkRetrySeconds` (or on PLAT's connectivity signal) while the app is in the foreground, at most 3 times per process, resuming from Play's offset, verifying and mounting; after the third failed retry it is FAILED with NETWORK_UNAVAILABLE.
- **Covers:** Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_NetworkLoss_ThenResume_Completes`, `Test_AssetPackInstaller_NetworkLoss_ThreeRetriesThenFailed`, `Test_AssetPackInstaller_NetworkRetry_WaitsForForeground`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Network retry"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-147 Download interrupted by process death resumes

- **Statement:** A pack download in progress when the process dies is reported in progress after relaunch and completes, verifies and mounts, and a persisted intent re-fetches a pack that failed.
- **Covers:** Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_ProcessDeathMidDownload_Resumes`, `Test_AssetPackInstaller_ProcessDeathAfterFailure_RefetchesIntent`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Fetch policy"; Zenith2/Docs/Design/PACK/Algorithms.md → "Simulated delivery"

### Z2-R-PACK-148 Cancelled download rolled back

- **Statement:** A cancelled download reaches NOT_INSTALLED only after Play's `removePack` succeeded, with no file of the pack left and no source ever mounted.
- **Covers:** Z2-I-PACK-15
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_Cancel_RollsBack`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-149 Corrupt delivery rolled back

- **Statement:** A delivered pack failing verification is removed and re-fetched once per game request, and a further failure leaves it FAILED with `VERIFY_FAILED`, its files kept and nothing mounted.
- **Covers:** Z2-I-PACK-15, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_CorruptDelivery_RemovedAndRefetchedOnce`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verification pipeline"

### Z2-R-PACK-150 Completion wins over a racing cancel

- **Statement:** A pack completing before its cancel is observed is verified and mounted, while a racing removal request removes it.
- **Covers:** Z2-I-PACK-15
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_CompletionRacesCancel_CompletionWins`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-151 Game consistent during an interrupted download

- **Statement:** While a pack download is interrupted, resumed or rolled back, the world's per-tick state hash equals a run that never requested the pack until the pack is published MOUNTED, no entity is created or lost and nothing asserts.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageIntegration_InterruptedDownloadGameConsistent`, `Test_PackageIntegration_RolledBackDownloadGameConsistent`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"; Zenith2/Docs/Design/PACK/Tests.md → "Integration suite"

### Z2-R-PACK-152 Undelivered assets fail NOT_INSTALLED

- **Statement:** An asset of an unmounted pack resolves `ASSET_ERROR_NOT_INSTALLED` and follows ASSET's fallback policy, and loads after the pack is mounted and the catalogue swapped.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageIntegration_AssetPackDeliveredLoads`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"; Zenith2/Docs/Design/PACK.md → "Missing and corrupt input"

### Z2-R-PACK-153 Fetch and cancel stress

- **Statement:** 1,000 fetch-then-cancel or fetch-then-remove cycles at randomised bytes leave no mounted source, no pack directory, no verification buffer, no live operation and no allocation behind.
- **Covers:** Z2-I-PACK-14, Z2-I-PACK-15, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_FetchCancelStress1000Cycles` (`z2-stress`); Test and Sanitizers steps.
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Asset pack installer tests"; Zenith2/Docs/Design/PACK.md → "Cancellation"

### Z2-R-PACK-154 S24 interrupted download resumed

- **Statement:** On the S24 with a Play internal test track, an on-demand download interrupted by aeroplane mode resumes, verifies and mounts, and one interrupted by killing the app resumes after relaunch.
- **Covers:** Z2-I-PACK-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `DeviceRun_Pack_PlayInterruptedResumed`, `DeviceRun_Pack_PlayProcessDeathMidDownload` run records (`needs-human`).
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Device runs"

### Z2-R-PACK-155 S24 interrupted download rolled back

- **Statement:** On the S24 with a Play internal test track, a download cancelled from the game at half reaches NOT_INSTALLED and its storage is freed.
- **Covers:** Z2-I-PACK-15
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `DeviceRun_Pack_PlayInterruptedRolledBack` run record (`needs-human`).
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Device runs"

### Z2-R-PACK-156 Package counters

- **Statement:** ZenithPackage publishes exactly the counters of "Counters" with their kinds, delta names ending `_delta`.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_Describe_JsonShape` and the installer tests reading counters; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Counters"

### Z2-R-PACK-157 Package log messages

- **Statement:** PACK logs in category `package` with the fixed message formats of "Log category and messages".
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_IgnoresPartialDeliveryNames`, `Test_PackageService_IdentityMismatch_DisablesDelivery`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Log category and messages"

### Z2-R-PACK-158 Profiler rows and frame dump

- **Statement:** The on-screen profiler shows a Package group with one row per pack in transit and the verification throughput, and the frame dump carries the service's `Describe` entry `package`.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_Describe_JsonShape`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "On-screen profiler rows"; Zenith2/Docs/Design/PACK.md → "Frame-dump entry"

### Z2-R-PACK-159 Package telemetry

- **Statement:** Each run reports the update kind, pack states at exit, fetch-to-mount times and verification failures, with no account or device identifier.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-09
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** review; `Test_PackageService_Describe_JsonShape`; Test step.
- **Design:** Zenith2/Docs/Design/PACK.md → "Telemetry"

### Z2-R-PACK-161 Play pack rows enforced

- **Statement:** Once the Play pack rows (asset pack download size, total download size, pack count) are adopted, `<Game>_Bundle` fails with exit code 8 when the built bundle exceeds them and Stage 6 acceptance blocks on them (BR-ACCEPT); the package mode only warns when archive bytes exceed a pack row, and pending rows are reported as pending.
- **Covers:** Z2-I-PACK-06, Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-PACK-001
- **Acceptance evidence:** `Pwsh_Pack_BundleLimit_AdoptedRowRefuses`, `Test_PackageMode_PlayLimitWarnsNotRefuses`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tooling.md → "CMake targets"; Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"; Zenith2/Docs/Design/PACK.md → "Pending new rows"

### Z2-R-PACK-162 Package service memory

- **Statement:** The package service's resident memory, fixed at Create (records, slots, event queue, verification arena, the parsed manifest, the delivery-state stamp table of up to 1,228,800 bytes and the `Zenith_VerifiedStampFile` copy of up to 8,437,792 bytes), stays within the package service resident memory row.
- **Covers:** Z2-I-PACK-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** MP-SUBMEM evidence record in Stage 6 acceptance (pending row, allocated by the Stage 1 per-system budget ticket for PACK).
- **Design:** Zenith2/Docs/Design/PACK.md → "In-memory layouts that matter"; Zenith2/Docs/Design/PACK.md → "Pending new rows"

### Z2-R-PACK-164 Pending installs within the cold-boot budget

- **Statement:** With nothing pending, the closing action of startup step 4 costs 12 file-system calls and writes nothing, and the initial query waits at most `package.initialQueryTimeoutMs`, so PACK's share of W-Load's cold-boot rows (measured with no update installed) is fixed; what an installed update adds is the proposed cold-boot update verification time row.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_NothingPending_NoWrites`; W-Load measurement in Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/PACK.md → "Rows PACK is measured against"; Zenith2/Docs/Design/PACK/Algorithms.md → "Complexity"

### Z2-R-PACK-165 Bundles sealed before commit

- **Statement:** Before writing a COMMITTING journal, or a REJECTING journal after a B6 (U9) failure, the applier flushes every regular file of the bundle and the bundle's and the incoming directory's entries, so bytes a third party delivered without a flush are durable before the commit and before any older version is trashed; a B3 or B4 refusal writes REJECTING without a seal, its record being made durable by the move to trash.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_UnsealedBundle_PowerLossKeepsOldPatch`, `Test_UpdateApplier_FlushesEveryAffectedParent`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Durable step rules"; Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation"

### Z2-R-PACK-166 Links never followed

- **Statement:** No install, recovery or sweep follows a symbolic link, junction or other reparse point: the install file system reports links, refuses to open through them, and a trash tree's links are removed as themselves, so nothing outside the install root is ever deleted.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_BundleWithJunction_DeletesLinkOnly`, `Test_MemoryInstallFileSystem_Links_ReportedNotFollowed`, `Test_HostInstallFileSystem_PathEscapeRefused`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_InstallFileSystem header"

### Z2-R-PACK-167 Bundle holds exactly its listed files

- **Statement:** A patch or DLC bundle whose directory holds anything besides its manifest and the regular files the manifest lists (another file, a subdirectory, a link) is refused by check U0 before any listed file is hashed.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_UnlistedEntry_Refused`, `Test_UpdateApplier_BundleWithJunction_DeletesLinkOnly`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"; Zenith2/Docs/Design/PACK/Formats.md → "Patch and DLC bundles"

### Z2-R-PACK-168 Install state file

- **Statement:** `<root>/.zenith2-install/install-state.zins` (48 bytes, format version 1, checks S1–S4, release line 0.0 valid) records the release line, the highest patch committed in it across every build of the line and the last swept build (all zero when obsolete directories were deferred), is written durably by the applier, is read at startup step 4 for U10 and the sweep, and is rebuilt when refused.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallState_RoundTrip`, `Test_InstallState_EachCheckRejects`, `Test_InstallState_OtherVersion_IsRefused`, `Test_UpdateApplier_ReplayedOlderPatch_RefusedAfterNewerRemoved`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Install state file"; Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"

### Z2-R-PACK-169 Patch bound to its base package

- **Statement:** A patch whose signed base content hash differs from the base package's (the mount manifest's `contentHash`) is refused by the applier and always by the patch mount (U11); while the applier does not know that hash at startup step 4 it examines no patch bundle and leaves every one in place.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_EachRefusal_RemovesBundleAndReports` (base hash case), `Test_UpdateApplier_UnknownBaseHash_LeavesPatches`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Update manifest checks"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"

### Z2-R-PACK-171 Package manifest never patched

- **Statement:** A content patch never ships `Package/package.zpkm`, so on a patched install it still describes the base package, `CheckIdentity` passes against the pre-patch identity and every delivered pack verifies against the base digests; a patch changing a pack's assets overrides them at patch precedence.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PatchDiff_ChangedEntriesOnly`, `Test_PackageService_PatchedInstall_IdentityAndPacksValid`, `Pwsh_Pack_PatchBuilder_EndToEndUnsigned`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Patch diff"; Zenith2/Docs/Design/PACK/Algorithms.md → "Update compatibility rules"

### Z2-R-PACK-172 Unmount and removal results never lost

- **Statement:** An unmount waits in UNMOUNTING with its target until every source is released, recording a fetch, a removal or a new Play version that arrives meanwhile, and a removal or request result travels in its own result cell, so no result is coalesced away by a subsequent state update.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-15, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackDelivery_RemoveResultNotCoalesced`, `Test_AssetPackInstaller_UnmountingRequests`, `Test_AssetPackInstaller_Transitions_EveryCell`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Delivery update slots"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-173 Mounted packs released before the installer is destroyed

- **Statement:** Every delivered pack and Windows DLC still mounted at shutdown is unmounted in shutdown step 4 and its release observed through `PumpShutdown` before the installer is destroyed in step 6, each mount operation has its own release block that its container wrappers co-own, and a block a rolled-back mount left drains on its own list, so no wrapper destructor touches freed memory and no removal deletes files a read still uses.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_ShutdownWithMountedPack_ReleasesBeforeDestroy` (ASan), `Test_AssetPackInstaller_RemountThenRemove_WaitsForSecondRelease`, `Test_AssetPackInstaller_PartialMountRollback_LateReleaseIgnored`; Test and Sanitizers steps.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"; Zenith2/Docs/Design/PACK/Algorithms.md → "Shutdown drain"; Zenith2/Docs/Design/PACK.md → "Startup and shutdown"

### Z2-R-PACK-174 Cold-boot update verification time

- **Statement:** The cold-boot time an installed update adds, in ms per GiB of installed update bytes present (the committed patch, Windows DLC and delivered packs), reported separately for the first start after the install (full hash) and the second (stamp checks only, Q90 ruled (a)), stays within the cold-boot update verification time row, measured with W-Load's MP-BOOT.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the MP-BOOT evidence records with and without an installed update per platform in Stage 6 acceptance (pending row, allocated by the Stage 1 per-system budget ticket for PACK).
- **Design:** Zenith2/Docs/Design/PACK.md → "Pending new rows"; Zenith2/Docs/Design/PACK.md → "Rows PACK is measured against"

### Z2-R-PACK-175 Unsupported install volumes refused

- **Statement:** An install root on a volume outside the supported set (NTFS and ReFS on Windows; ext4 and F2FS on Android; ext4, XFS, Btrfs and tmpfs on the Linux host) disables installs, recovery and the sweep with one log line per start, and the current content plays.
- **Covers:** Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_UnsupportedVolume_InstallsNothing`, `Test_HostInstallFileSystem_VolumeKind`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Pending installs at startup"; Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"

### Z2-R-PACK-176 Test manifests signed in process

- **Statement:** Every C++ test that needs a signed manifest signs it in process with the header-only RFC 8032 Ed25519 signer `Zenith_PackageTestSigner.h` and RFC 8032's hand-copied test seeds, so the applier, verifier and DLC tests need no fixture set, no ZenithCook and no JDK and run on every host preset, and the signer reproduces RFC 8032's published signatures.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageTestSigner_Rfc8032Vectors`, `Pwsh_Pack_Signer_Rfc8032Test1`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Tests.md → "Fixtures"; Zenith2/Docs/Design/PACK/Tooling.md → "Ed25519 signer"

### Z2-R-PACK-177 Install and delivery readers refuse other versions

- **Statement:** The readers of `install-journal.zinj`, `install-state.zins`, `install-record.zinr`, `delivery-state.zdls` and `verified.zinv` refuse any format version other than 1 with a recoverable error (`JOURNAL_VERSION` for the journal; for the user-state files, which are rebuilt, `RECORD_CORRUPT` for the install record and `STATE_FILE_CORRUPT` for the install state, the delivery state and the verified stamp file, each test asserting its error), never an assert, and each format's registry row joins in the Stage 6 diff that adds its refusal test.
- **Covers:** Z2-I-PACK-01, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallJournal_OtherVersion_IsRefused`, `Test_InstallState_OtherVersion_IsRefused`, `Test_InstallRecord_OtherVersion_IsRefused`, `Test_DeliveryStateFile_OtherVersion_IsRefused`, `Test_VerifiedStampFile_OtherVersion_IsRefused`, `CMake_Pack_FormatRegistry_Valid`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Common rules for PACK formats"; Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"

### Z2-R-PACK-178 Install and delivery format limits

- **Statement:** The limits of the install and delivery formats (update files 65,536 and update manifests of 225 bytes and more, journal moves 0 … 1,024 with strings of 64 KiB, the install state exactly 48 bytes, the install record exactly 192 bytes, delivery packs 256 with 25,600 stamps of 48 bytes and strings of 16 KiB, the verified stamp file's 1,024 directories, 131,072 files, 1 MiB of strings and 8,437,792 bytes, event payloads exactly 96 bytes) are constants tested at the limit and at limit + 1, and the core parser-limit table carries them.
- **Covers:** Z2-I-PACK-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallJournal_RoundTrip` (1,024 records), `Test_InstallJournal_EachCheckRejects` (1,025), `Test_InstallState_EachCheckRejects`, `Test_InstallRecord_EachCheckRejects`, `Test_DeliveryStateFile_RoundTrip` (256 packs, 25,600 stamps), `Test_VerifiedStampFile_LimitsAtAndAbove`, `Test_RuntimePackage_EventPayloadRoundTrip`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Install journal header"; Zenith2/Docs/Design/PACK/Formats.md → "Install state file"; Zenith2/Docs/Design/PACK/Formats.md → "Delivery state header"

### Z2-R-PACK-179 Release block per mount operation

- **Statement:** Every mount operation (A4) of a pack or a Windows DLC allocates its own release block with its own expected count, X compares only the current block's released count with that count, a block a rolled-back mount (M2) left drains on the pack's draining list, and no removal of the pack is issued while any of its blocks is unreleased; A4's allocations are the installer's only runtime allocations and fail as `MOUNT_FAILED`.
- **Covers:** Z2-I-PACK-15, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_RemountThenRemove_WaitsForSecondRelease`, `Test_AssetPackInstaller_PartialMountRollback_LateReleaseIgnored`, `Test_AssetPackInstaller_MountAllocationFails_IsMountFailed`, `Test_AssetPackInstaller_RemoveMounted_WaitsForRelease`; Test and Sanitizers steps.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Mounting and unmounting"; Zenith2/Docs/Design/PACK.md → "Allocation failure"

### Z2-R-PACK-180 Shutdown pump with a bounded drain

- **Statement:** `PumpShutdown` raises V3 and X without a prologue; `Zenith_RuntimePackage_BeginShutdown` loops it with 1 ms sleeps for at most `package.shutdownDrainMs` (1,000 ms by default) for the releases, logs every pack still unreleased at the bound and continues without deleting any file; after `BeginShutdown` no verification starts and nothing mounts (A3, A4 and S suppressed, VFY resolving to DELIVERED, no `AddSource`), so the shutdown rule leaves every MOUNTED pack NOT_INSTALLED and every pack that was verifying, waiting for a verification slot or unmounting for a new Play version DELIVERED, each with its persisted intent unchanged.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetPackInstaller_ShutdownCancelsVerification`, `Test_AssetPackInstaller_ShutdownWithMountedPack_ReleasesBeforeDestroy`, `Test_AssetPackInstaller_ShutdownDrain_BoundedWhenSourceHeld`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Shutdown drain"; Zenith2/Docs/Design/PACK/Algorithms.md → "Asset pack transitions"

### Z2-R-PACK-181 Package state resource and world snapshot

- **Statement:** Every world has the transient world resource `Zenith.Package.State` (`Zenith_PackageStateResource`, 6,168 bytes, registered at world creation step 4 with the update kind NONE and every unit without the PRESENT flag UNKNOWN) and receives, at the first prologue step 1b after its creation (frame k + 1's for a world created in prologue step 7 of frame k), one snapshot of recorded publications (the update, up to 16 rejections, one state per deliverable unit and per Windows DLC unit), so a world created at run time reads the same package state and update kind as the startup world from the first tick boundary after that step on.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_LateWorldReceivesSnapshot`, `Test_RuntimePackage_ReplayPublishesRecorded`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "World snapshot"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Runtime bridge declarations"

### Z2-R-PACK-182 Forwarding entitlement query

- **Statement:** The package service queries the runtime bridge's `Zenith_ForwardingEntitlementQuery`, which answers UNKNOWN until a provider is set: the configuration query at the close of step 11 when the composition lists no `Online` module, ONLINE's query through MOD's `ProvideEntitlementQuery` at step 15, the configuration query at the end of step 15 when none was provided; every provider change is a revision change, and the provider is cleared in shutdown step 4 before any module is destroyed.
- **Covers:** Z2-I-PACK-07, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_EntitlementProviderSwitch_RemountsDlc`, `Test_RuntimePackage_EntitlementProviderLifecycle`, `Test_ForwardingEntitlementQuery_RevisionPerProviderChange`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Entitlement provider"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header"

### Z2-R-PACK-183 Replay mounts the recorded pack set

- **Statement:** Under `--replay` the installer issues no automatic fetch, resume or removal, applies neither the entitlement hold nor the fetch guard to a recorded unit and raises no entitlement event, and the bridge mounts exactly the units some recorded publication shows MOUNTED (packs, entitlement-gated Android packs included, and Windows DLC, without asking the entitlement query, a Windows DLC only at the revision whose result content hash the recording carries) before the first tick, refusing the replay with exit code 5 when one cannot be mounted within `package.replayMountTimeoutMs` or a DLC's installed revision differs.
- **Covers:** Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimePackage_ReplayPublishesRecorded`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Replay pack set"

### Z2-R-PACK-184 Patch numbering monotonic per release line

- **Statement:** Every release and every patch of a release line carries a version greater than every earlier release and patch of that line, whichever build it targets, which SHIP's release pipeline enforces (cross-area row PK25), so the install state's highest patch and the install record's highest patch seen are valid floors across every build of the line.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-12
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_InstallRecord_HighestPatchSeen_ResetByLine` (the cross-build case), `Test_UpdateApplier_ReplayedOlderPatch_RefusedAfterNewerRemoved`; SHIP's `Pwsh_Ship_ReleasePipeline_VersionNotGreaterRefused` (requested by row PK25, or the name SHIP gives it when it answers); Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Update classification"; Zenith2/Docs/Design/PACK/Formats.md → "Install state file"; Zenith2/Docs/Design/PACK/Tooling.md → "Patch builder"

### Z2-R-PACK-185 Archives stored in every APK split

- **Statement:** Every `.zpak` is stored uncompressed inside every APK and asset-pack split (`androidResources.noCompress += "zpak"` in Gradle builds, bundletool's `uncompressedGlob` in the test-time mini bundle), so the asset manager reads archives at file offsets without inflation.
- **Covers:** Z2-I-PACK-03, Z2-I-PACK-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Pack_MiniBundle_ArchivesStored`, `CMake_Pack_BundleProjectGeneration`; `Test-Zenith2ApksStored` on every slice's Shipping bundle in Stage 6 acceptance; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Bundle configuration and device specs"; Zenith2/Docs/Design/PACK/Tooling.md → "bundletool usage"

### Z2-R-PACK-187 Verified stamp file format

- **Statement:** `<root>/.zenith2-install/verified.zinv` is format version 1 (magic `ZINV`, little-endian, a 32-byte header, 48-byte directory records sorted by path, 56-byte file records sorted by name within their directory, a string table), limited to 1,024 directories, 131,072 files, 1,048,576 string bytes and 8,437,792 bytes (a writer over a limit drops other directories highest path first, and a target that alone does not fit is written without its record), refused by checks VS1–VS6 with `PACKAGE_ERROR_STATE_FILE_CORRUPT` (after which every committed directory re-hashes in full), registered as `ZINV` (`user-state`) in the Stage 6 diff that adds its refusal test, and fuzzed by `Zenith_Fuzz_VerifiedStamps`.
- **Covers:** Z2-I-PACK-01, Z2-I-PACK-10, Z2-I-PACK-11
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_VerifiedStampFile_RoundTrip`, `Test_VerifiedStampFile_EachCheckRejects`, `Test_VerifiedStampFile_OtherVersion_IsRefused`, `Test_VerifiedStampFile_LimitsAtAndAbove`, `Test_VerifiedStampFile_FindStamp`, `CMake_Pack_FormatRegistry_Valid`, the `Zenith_Fuzz_VerifiedStamps` smoke; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Formats.md → "Verified stamp file"; Zenith2/Docs/Design/PACK/Formats.md → "Binary format registry"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_UpdateApplier header"

### Z2-R-PACK-188 The applier records the stamps it verified

- **Statement:** B6 takes every listed file's identity stamp by path immediately before and after `Zenith_VerifyUpdateManifestFiles` hashes it (the racy-timestamp rule applied), leaving the bundle in `.incoming` with `UPDATE_INCOMPLETE` and no journal when the two differ and refusing it only for a hash or size mismatch; every patch or DLC commit then writes `verified.zinv` in a best-effort step (patch step 3b, DLC step 4b) holding the target's record from those stamps and the surviving records of other committed directories, whose failure is logged, counted in `package.install.stamp_write_failures` and never holds the commit; recovery that continues a commit keeps the target's record when it carries the committed manifest's digest and otherwise rebuilds it with one stamp, hash and stamp pass.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_CommitWritesVerifiedStamps`, `Test_UpdateApplier_StampChangedDuringVerify_LeftInPlace`, `Test_UpdateApplier_VerifiedStampWriteFails_CommitCompletes`, `Test_UpdateApplier_RacyStampNotRecorded`, `Test_UpdateApplier_RecoveredCommit_KeepsExistingRecord`, `Test_UpdateApplier_RecoveredDlcCommit_KeepsExistingRecord`, `Test_MemoryInstallFileSystem_IdentityStampModel`, `Test_HostInstallFileSystem_IdentityStamp`, `Test_HostInstallFileSystem_VolumeKind`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Algorithms.md → "Install steps per operation"; Zenith2/Docs/Design/PACK/Algorithms.md → "Durable step rules"

### Z2-R-PACK-189 Patch and DLC mounts skip unchanged files

- **Statement:** At every start the patch mount and Windows DLC mounting check every update manifest's signature and targets and then re-hash a listed file only when the bridge's `Zenith_VerifiedStampFile` has no record of it, the record's manifest digest differs, or its current stamp differs in any of its five fields (size, last-write time, change time, volume, file identity); a missing or refused `verified.zinv`, or `package.reverifyOnStartup`, passes no lookup and every file is hashed; the bridge reads the file at the close of startup step 4 and keeps the lookup until shutdown step 6.
- **Covers:** Z2-I-PACK-10, Z2-I-PACK-11, Z2-I-PACK-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_PackageService_DlcMount_StampSkip`, `Test_RuntimePackage_UpdateStampLookupLifecycle`, PLAT's `Test_PatchMount_StampSkipAndRehash` (row PK27); Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Algorithms.md → "Windows DLC mounting"; Zenith2/Docs/Design/PACK/PublicAPI.md → "Runtime bridge declarations"

### Z2-R-PACK-190 No unverified file is counted as verified

- **Statement:** At every interruption point of a patch install and a DLC replacement, after recovery and a new pass, `verified.zinv` is absent, refused, or holds only records satisfying invariant I13: every directory record carries the SHA-256 of that directory's manifest as B6 or recovery verified it, and a file record equal to the file's current stamp, in a directory whose current manifest has that digest, implies the file's bytes hash to the manifest's listed SHA-256 and size.
- **Covers:** Z2-I-PACK-13, Z2-I-PACK-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UpdateApplier_PowerLossAtEveryStep_Converges`, `Test_UpdateApplier_DlcReplace_PowerLossAtEveryStep_Converges` (I13 checked by hashing at every (i, k)), `Test_UpdateApplier_RecoveredDlcCommit_KeepsExistingRecord`; Test step.
- **Design:** Zenith2/Docs/Design/PACK/Algorithms.md → "Consistency invariants"; Zenith2/Docs/Design/PACK/Algorithms.md → "Verified cache"; Zenith2/Docs/Design/PACK/Algorithms.md → "Recovery table"
