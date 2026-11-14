# ASSET design: test catalogue

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It lists every named ASSET test with the case it asserts, its file, the fixtures, and the gate step that runs it. Test IDs follow `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"` ("Test naming"): `Test_<Type>_<Behaviour>` for the doctest name `"<Library>/<Type>/<Behaviour>"`.

## Scope of this file

| Group | Heading |
|---|---|
| targets | "Test targets" |
| runtime | "Handle and registry tests" … "Fuzz targets" |
| archive, built-in kinds | "Archive and built-in kind tests" |
| tools | "Database tests" … "Meta document tests" |
| end to end | "Integration suite" |
| data | "Fixtures" |
| where each runs | "Gate steps per test group" |

## Test targets

| Target | Directory | Links |
|---|---|---|
| `ZenithAssets_Tests` | `Zenith2/Engine/ZenithAssets/Tests/` | ZenithAssets, its dependencies, ZenithTestSupport, the both compression backends through `TEST_DEPENDS` |
| `Zenith_Fuzz_CookedHeader`, `_Catalogue`, `_SectionDecode`, `_LoadResult`, `_Archive` | `Zenith2/Engine/ZenithAssets/Tests/Fuzz/` | ZenithAssets, libFuzzer (`win-asan`) |
| `ZenithCook_Tests` | `Zenith2/Tools/ZenithCook/Tests/` | ZenithCook's object library ("Assumed interfaces pending other areas" in the entry document; core-doc item X114 (b), pending), SCENE's `ZenithDocument` and `ZenithDocumentHost` |
| `Zenith_Fuzz_MetaDocument`, `_DdcEntry`, `_AssetDatabase` | `Zenith2/Tools/ZenithCook/Tests/Fuzz/` | the same |
| `Zenith_Integration_AssetPipeline` | `Zenith2/Tests/AssetPipeline/` | ZenithAssets, ZenithRuntime; the CTest fixture `AssetPipeline_Cook` runs ZenithCook first |

Runtime tests use the fake kind loader `Tests/Support/Zenith_FakeKindLoader` (test kinds, scripted per-stage delays and failures, a pending-upload list that a test thread drains as the owner's "upload" stage and can fail on demand, and an option to report an upload before the loader calls `Activate`, counters of live objects and outstanding resources) and fixtures written by `Zenith_AssetFixtureWriter` into a temporary user-mount directory mounted as the loose mount. Every runtime test's registry is created with `m_bAllowTestKinds` and `m_axTestKinds` = `Zenith_FakeKindLoader::GetStandardTestKinds()` ("Fixtures") unless the test says otherwise, and its fixture catalogues carry the test-kind table.

## Handle and registry tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetTypes_KindTableMatchesDesign` | `Zenith_AssetHandle_Tests.cpp` | every column of every kind row (runtime type and GPU resident included) equals `Tests/Fixtures/KindTable.json`, the committed copy of the entry document's table; names, runtime types, extensions and magics unique; codes dense from 1 to `uASSET_KIND_DEFINED_COUNT` − 1 |
| `Test_AssetTypes_KindSetAndLookups` | same | `Zenith_AssetKindSet` add, remove, count and contains for codes 0 … 255 and for every test code 0x7F00 … 0x7F0F (`m_uTestBits`), and ignores 256, 0x7EFF and 0x7F10 (Add and Remove change nothing, Contains false); `sizeof` is 40; `Zenith_FindAssetKindByTypeID` inverts `Zenith_Hash64(stableName)` for every kind; `Zenith_GetAssetKindInfo` of a test code is null; `Zenith_GetGpuResidentAssetKinds` equals the table's column; a registry's `GetKindInfo` returns the declared row for a declared test code and null for an undeclared one, and its `GetGpuResidentKinds` adds exactly the declared GPU-resident test kinds (0x7F02, 0x7F03 of the standard rows) |
| `Test_AssetHandle_NullDoesNotResolve` | same | resolving a null handle returns `NULL_HANDLE` in every configuration ([Z2-N-CONTRACT-096]) |
| `Test_AssetHandle_StaleAfterDestroy_DoesNotResolve` | same | an uncounted byte copy (`Zenith_AssetRegistryTestAccess::MakeUncountedCopy`) of a handle whose asset retired returns `STALE_HANDLE` |
| `Test_AssetHandle_ForeignOwner_DoesNotResolve` | same | a handle of registry A resolved by registry B returns `FOREIGN_HANDLE` |
| `Test_AssetHandle_GenerationExhaustion_RetiresSlot` | same | a slot set to generation `0xFFFFFFFE` by test access is retired after one more free and never reissued |
| `Test_AssetHandle_CopyMoveDestroy_BalancesReferences` | same | the count after every copy, move, assignment and destruction; retirement at zero ([Z2-N-CONTRACT-097]) |
| `Test_AssetHandle_LayoutIs16Bytes` | same | `sizeof` of both handle types is 16 and the member order is Contracts' |
| `Test_AssetHandle_FromAnyKindMismatch` | same | `FromAny` of a handle of another kind returns `KIND_MISMATCH` |
| `Test_AssetHandle_ValueHooksBalance` | same | the struct from `Zenith_GetAssetHandleValueKindHooks` (16-byte element, null resolver) installed into a test `Zenith_TypeRegistry`; a schema type with 1, 7 and 64 `asset<K>` fields constructed, copied, bytewise-relocated and destroyed through the schema hooks leaves every count balanced |
| `Test_AssetHandle_ResolveReferenceDefault` | same | `ResolveReferenceDefaults` with `Zenith_ResolveAssetReferenceDefault`: a default naming a catalogue asset of kind K gives a counted handle in the construction default object (one reference); another kind or an absent GUID returns false and leaves the element null, logging `asset.missing` once |
| `Test_AssetHandle_ConcurrentCopyDestroy` | same (`z2-stress`, `linux-tsan`) | 8 threads copying and destroying one handle 1,000,000 times end with the original count; TSan clean |
| `Test_AssetRegistry_UnknownGuidGivesNullAndDiagnostic` | `Zenith_AssetRegistry_Tests.cpp` | `Acquire` returns `NOT_FOUND`; `AcquireForDeserialization` returns null and logs one `asset.missing` record per GUID |
| `Test_AssetRegistry_AcquireByLogicalPath` | same | logical path lookup finds sources and `#key` sub-assets; an unknown path is `NOT_FOUND` |
| `Test_AssetRegistry_AcquireKindMismatch` | same | `Acquire<T>` of a GUID of another kind is `KIND_MISMATCH` |
| `Test_AssetRegistry_CapacityExhaustionRefuses` | same | the (max + 1)th distinct acquire returns `REGISTRY_FULL` and changes nothing |
| `Test_AssetRegistry_RevivalFromZeroReferences` | same | an acquire racing the retire path revives the slot (the service skips it); the object is not unloaded |
| `Test_AssetRegistry_Create_FailsAt<Step>_Unwinds` | same | one test per `Create` step (Object, Slots, Operations, Queues, Staging, Catalogue, Consumer, MissingSet): the error, zero leaks, no job left ([Z2-N-CONTRACT-341]); a desc with test kinds but no `m_bAllowTestKinds`, a duplicated test code and a test row with another magic are each `INVALID_REQUEST` before step 1; `m_bAllowTestKinds` or non-empty `m_axTestKinds` with `m_bDevelopmentFeatures` false is `NOT_ALLOWED` before step 1 (the path a Shipping build's `Zenith_IsShippingBuild()` takes); a catalogue listing an entry of an undeclared test code fails step 6 with `NOT_ALLOWED` naming the code and the GUID |
| `Test_AssetRegistry_RegisterKindLoaderRules` | same | duplicate kind, test kind without the flag, a test kind the desc does not declare (`NOT_ALLOWED`), after `CloseRegistration`, a format version differing from a non-zero kind-table element (`VERSION_MISMATCH`), a SUBSTITUTE kind without a role-0 fallback row (`NO_FALLBACK`) — each its error; a kind-table element of 0 (no content) accepts any format version |
| `Test_AssetRegistry_PinnedFallbacksFromCatalogue` | same | registration pins every fallback-table row of the kind (all roles) and requests each at CRITICAL; `GetPinnedAsset(kind, role)` returns counted handles; `GetPinnedLoads` completes when all are ACTIVE and fails with the first pinned failure; a swap that changes a pinned row is refused |
| `Test_AssetRegistry_MissingLogBounded` | same | 2,000 distinct unresolvable GUIDs: `asset.missing` counts 2,000; one record per GUID for the first 1,024; afterwards at most one record per frame; nothing allocates after `Create` |
| `Test_AssetStateMachine_EveryTransition` | `Zenith_AssetStateMachine_Tests.cpp` | one case per row of every state table of Loading.md "Asset state machine" whose event the public API can produce: the fake loader and fixture catalogues drive the state and the event, and the case asserts the next state and the row's action (counts, completions, loader calls); the rows marked "cannot occur" are asserted unreachable by the service's consistency check never firing across every case and the 1,000-cycle stresses |
| `Test_AssetStateMachine_ReloadAndRecoveryPipelines` | same | every row of "Reload and recovery pipelines": the peak reservation and its deferral, the reload waiting in QUEUED for its dependencies and for admission (and cancelled there), swap and retirement, failure keeping the old object, cancel, a second reload queued, recovery queued behind the in-flight limit, recovery restarting a running reload; a second E12 with a recovery queued (it stays queued) and running (it restarts with a new attempt); E12 during RETIRING (the new object's recovery runs while the old object retires, and both acknowledgements arrive); E8, E16 and E17 during RETIRING (the old object's `Unload` at once, the slot freed only after RA, never more than two unload acknowledgements outstanding, the consistency check silent); a further E11 during RETIRING starting only after RA; E13 and E14 with a reload queued (back to CATALOGUE: `NOT_FOUND`, or the new entry) and running (its result stands), and with a recovery queued or running (cancelled, LOST, no error); a reload owing a recovery that fails (the recovery then runs) and that activates (the count ends); E18 and E19 changing nothing |
| `Test_AssetRegistry_ReviveDuringUnloadWaitsForAck` | same | an acquire and a load request during UNLOADING: the unload completes, the slot is not freed, and the load starts after `AcknowledgeUnload` |
| `Test_AssetRegistry_DestroyReportsReferencedSlots` | same | destroying with one referenced slot logs one leak line with GUID, logical path and count |
| `Test_AssetRegistry_DestroyWithLoadInFlightChecks` | `Zenith_AssetLoader_DefectTests.cpp` (`z2-death-check`) | the `ZENITH_CHECK` fires |
| `Test_AssetRegistry_DumpSchema` | `Zenith_AssetRegistry_Tests.cpp` | `WriteDump` produces schema `zenith2.asset-dump` v1 with every live slot and in-flight load |
| `Test_AssetRegistry_ResolvePointerValidUntilFrameBoundary` | same | a development reload does not free an object resolved in the same frame before `AdvanceFrame` + `asset.reload.retireFrames` |
| `Test_AssetRegistry_AvailabilityEpochAdvancesOnActivation` | same (the reader case also `linux-tsan`) | `GetAvailabilityEpoch` is 0 after `Create` and moves by exactly 1 at: a CPU-kind activation, a GPU-kind activation (PENDING), its `ReportGpuResident`, a `ReportGpuFailed`, an activation consuming an early outcome (once, not twice), a reload swap, a recovery activation and a load failure; it does not move at a revive from CACHED, an E12, an unload, a stale or second report, an early report before its activation, or a headless GPU-kind activation; reader case: a thread that observes a new value and then calls `Resolve` and `GetGpuResidency` sees the published object and residency |
| `Test_AssetStageHold_ParksAndCancels` | `Zenith_AssetStageHold_Tests.cpp` | for each of DECODING, DELIVER and ACTIVATING: a load parks at the stage (`IsParked`), a `Cancel` while parked completes `CANCELLED` with every unit, staging byte and record back at baseline, and `Release` without a cancel lets it reach ACTIVE; failure cases: a second `Arm` on the registry returns false, and a hold still armed at registry destruction is caught by `ZENITH_CHECK` in a death test |

## Catalogue and cooked file tests

| Test | File | Asserts |
|---|---|---|
| `Test_CookedFile_WriterRoundTrip` | `Zenith_CookedFile_Tests.cpp` | 0, 1 and 4,096 sections of each codec write and parse back with identical bytes and descriptors |
| `Test_CookedFile_WriterDeterministic` | same | sections added in different orders produce identical bytes; two runs produce identical bytes |
| `Test_CookedFile_SectionAlignment` | same | every section offset is 16-aligned with zero padding |
| `Test_CookedFile_EachCheckRejects` | same | one mutated file per check C1–C12 fails with its code and detail, in the table's order |
| `Test_CookedFile_VersionMismatchRefused` | same | a format version or engine cook version ± 1 is `VERSION_MISMATCH` ([Z2-N-CONTRACT-528]) |
| `Test_CookedFile_LimitsAtAndAboveLimit` | same | 4,096 sections parse; 4,097 is `LIMIT_EXCEEDED` ([Z2-N-ARCH-953]) |
| `Test_CookedFile_Example_Runs` | `Examples/Zenith_CookedFile_Example.cpp` | the example |
| `Test_AssetCatalogue_LoadAndFind` | `Zenith_AssetCatalogue_Tests.cpp` | entries, logical paths, file entries and dependencies found; binary-search bounds |
| `Test_AssetCatalogue_EachRuleRejects` | same | unsorted, duplicate GUID, duplicate logical path, bad object path, dependency out of range, self-dependency, hard cycle, bad check, over limits — each `CATALOGUE_INVALID` |
| `Test_AssetRegistry_CatalogueCookIdentityMismatchFailsCreate` | `Zenith_AssetRegistry_Tests.cpp` | a base catalogue whose engine cook version is ± 1 from the build's, and one whose kind count K is ± 1 from `uASSET_KIND_DEFINED_COUNT`, each fail `Create` with `COOK_IDENTITY_MISMATCH` |
| `Test_AssetCatalogue_KindTableRules` | `Zenith_AssetCatalogue_Tests.cpp` | an entry whose format version differs from its kind's element, an entry of a kind whose element is 0, and two fragments disagreeing on a non-zero element are each `CATALOGUE_INVALID`; an element of 0 beside a non-zero one merges to the non-zero value; the test-kind table: a catalogue with flag bit 1 loads with `bAllowTestKinds` and its test entries are checked against their elements, while the same bytes without `bAllowTestKinds` are `CATALOGUE_INVALID`, as are a test-code entry with flag bit 1 clear and an entry of code 0x7F10 or 300 |
| `Test_AssetCatalogue_ExternalReferences` | same | a fragment's EXTERNAL dependency resolves to the merged entry index; an unresolvable external GUID and EXTERNAL in a base catalogue are `CATALOGUE_INVALID`; the merged catalogue has no EXTERNAL flag |
| `Test_AssetCatalogue_FallbackTableRules` | same | rows sorted and unique; an entry index out of range, a GUID or kind differing from the entry, and a fallback table in a fragment are `CATALOGUE_INVALID` |
| `Test_AssetCatalogue_LimitsAtAndAboveLimit` | same | every row of Formats.md "Catalogue limits" at the limit loads and at limit + 1 fails ([Z2-N-ARCH-953]) |
| `Test_AssetCatalogue_MergePrecedence` | `Zenith_AssetCatalogue_Tests.cpp` | a patch fragment's entry replaces the package's; two same-precedence fragments disagreeing are `CATALOGUE_INVALID` |
| `Test_AssetCatalogue_ContentHashCoversOutputs` | same | changing any entry's output hash changes the content hash |

## Loader tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetLoader_LoadActivates` | `Zenith_AssetLoader_Tests.cpp` | a load passes every stage in order (the fake loader records them) and ends ACTIVE with the operation `SUCCEEDED` after the object is published |
| `Test_AssetLoader_RequestsDeduplicate` | same | 16 requests for one asset run one pipeline; all complete; `asset.loads.deduplicated` = 15 |
| `Test_AssetLoader_HardDependenciesFirst` | same | a dependency's activation precedes the dependent's `BeginLoad`; the dependent receives the handles |
| `Test_AssetLoader_DependencyFallbackAllowsParent` | same | a failed SUBSTITUTE-kind dependency lets the parent load against the fallback; a FAIL-kind one fails it with `DEPENDENCY_FAILED`; a SUBSTITUTE dependency that fails while its pinned fallback is still loading (the fake loader delays the fallback) keeps the parent in DEPENDENCIES until the fallback is ACTIVE, and the parent then loads, in every one of 100 seeded orderings; a fallback that itself fails fails the parent |
| `Test_AssetLoader_PriorityOrder` | same | with the queue saturated, CRITICAL requests are issued before earlier NORMAL ones; `SetPriority` raises a waiting load |
| `Test_AssetLoader_SectionRounds` | same | a loader requesting three rounds receives each; a ninth round is `LIMIT_EXCEEDED` |
| `Test_AssetLoader_LoaderDestinationUsed` | same | a loader-supplied destination receives the decoded bytes and no ZenithAssets buffer is used |
| `Test_AssetLoader_LoaderFailureReported` | same | `Fail(7)` gives `LOADER_FAILED` with detail 7 on every request |
| `Test_AssetLoader_CompletionQueueRecords` | same | one record per request, the user tag echoed; a full queue refuses at `RequestLoad` |
| `Test_AssetLoader_ServiceUntilCompleteOffWorker` | same | works on the main thread; returns `INVALID_REQUEST` on a job worker; returns `TIMEOUT` with the operation still running when the fake loader never activates within the timeout |
| `Test_AssetLoader_GpuKindActivatesBeforeUpload` | same (the race case also `linux-tsan`) | a GPU-resident fake kind (0x7F02) activates with its upload requested into the fake owner's pending list and never drained: the asset is ACTIVE with residency PENDING, its dependent's `BeginLoad` runs, and `ReportGpuResident` with the ticket makes it RESIDENT; a stale ticket (after a reload swap) changes nothing; the owner failing the drained upload makes the loader call `ReportGpuFailed(ticket, 9)`: residency FAILED, the requests still `SUCCEEDED`, `ResolveOrFallback` returns the kind's role-0 fallback, `asset.gpu.failed` 1 and one `asset.load` record with code 9, and a following `RequestReload` activation returns it to PENDING; race case: 4 threads report stale and current tickets while the main thread runs 10,000 reload swaps of one asset — no report ever marks a swapped-in object RESIDENT before its own ticket's report, and the attempt values read back are distinct for every attempt; early cases, with the service paused (`Zenith_AssetRegistryTestAccess::PauseService`): an upload completed and reported RESIDENT before the service applies `Activate` (and, in a second case, before the loader calls it) ends RESIDENT at the activation with `asset.gpu.pending` back to 0, and one reported FAILED ends FAILED with exactly one `asset.load` record and `asset.gpu.failed` 1; stale-attempt cases: a load cancelled after its infallible push, whose ticket the owner then reports RESIDENT, and a record restarted by E12, whose pre-restart ticket is reported after the restart, never change the residency of the slot's next activation, which stays PENDING until its own report |
| `Test_AssetLoader_BootWithoutRendering` | same | the startup order of Loading.md "Tool and boot servicing" with fake GPU and CPU kinds: pinned fallbacks and a scene-like asset whose hard dependencies include GPU kinds all reach ACTIVE through `ServiceUntilComplete` with no frame executed; a dependent kind registered before its dependency's kind (0x7F04's pinned fallback depends on a 0x7F02 asset, 0x7F04 registered first) leaves the dependency waiting in WAIT_LOADER (`asset.loads.waiting_loader` 1) until 0x7F02 registers, after which both reach ACTIVE; a hard dependency of a kind that never registers fails `NO_LOADER` at `CloseRegistration`, which fails `GetPinnedLoads`; with the service paused, `RegisterKindLoader` of a waiting pipeline's kind immediately followed by `CloseRegistration` leaves that pipeline admitted and ACTIVE once the service resumes, never `NO_LOADER` |
| `Test_AssetLoader_HeadlessGpuKindsValidateOnly` | same | in headless mode a GPU-kind load activates with a null object, `Resolve` gives `NO_RUNTIME_OBJECT`, and a corrupt header still fails |
| `Test_AssetLoader_NotInstalledRetriesAfterMount` | same | an entry whose file is absent fails `NOT_INSTALLED`; after adding the file's source and `OnMountsChanged`, a new request succeeds |
| `Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` | same | `ReloadGpuResidencySet` re-enters every ACTIVE GPU-kind asset with `m_bDeviceRecovery` (pinned first), keeps it ACTIVE with residency LOST then PENDING, evicts CACHED ones, restarts a LOADING record that was in DELIVER, and completes after all; a failing recovery leaves LOST and fails the operation with the first error; with 64 ACTIVE GPU assets (4 pinned) and `asset.load.maxInFlight` = 4, loads in flight never exceed 4, the 4 pinned recoveries are admitted first and the rest in (kind code, GUID) order, the load-record pool and the acknowledgement queue never overflow, and every asset ends PENDING; a recovery cancelled by E8 counts as ended and E16 completes the operation `CANCELLED`; a second `ReloadGpuResidencySet` while 60 recoveries are still queued completes the first operation `CANCELLED`, keeps those 60 queued, restarts the running ones with new attempts, and the second operation completes once every slot is PENDING; a recovery queued behind the limit whose last reference then drops unloads (UNLOADING, never CACHED), and a reacquire reloads it from disk and ends PENDING |
| `Test_AssetLoader_ServiceSlicesBounded` | same | with 10,000 queued requests no service slice exceeds 2 ms on `win-release` (measured by the slice zone; overruns counted, the test fails on any) |
| `Test_AssetLoader_StressUnderTsan` | same (`z2-stress`, `linux-tsan`) | 8 requester threads, random priorities, cancellations and handle drops over 20,000 loads: every operation completes once, counts balance, TSan clean |
| `Test_AssetReader_RangeAndSection` | `Zenith_AssetReader_Tests.cpp` | `ReadRange` returns raw bytes; `ReadSection` returns decoded bytes through the decode queue; bounds errors |
| `Test_AssetReader_VerifiedFileHashMismatch` | same | a mutated file entry is `CORRUPT` / `FILE_HASH_MISMATCH`; a destination of any size other than `GetVerifiedFileSize` is `INVALID_REQUEST` |
| `Test_AssetLoadResult_EncodeDecode` | `Zenith_AssetLoader_Tests.cpp` | a FAILED record round-trips in 24 bytes; encoding SUCCEEDED or CANCELLED or a zero error code is refused; decoding any other status, length or reserved value is `CORRUPT` |
| `Test_AssetLoader_Example_Runs` | `Examples/Zenith_AssetLoader_Example.cpp` | the example |
| `Test_AssetReader_EngineContentReadBeforeFrames` | `Zenith_AssetReader_Tests.cpp` | the sequence of RuntimeAPI.md "Engine content reads during Create" on the main thread with no frame loop, over a registry whose VFS mounts a generated cooked fixture directory as its `MOUNT_KIND_LOOSE` source: `GetVerifiedFileSize`, `ReadVerifiedFile` at CRITICAL and `ServiceUntilComplete` return the file entry `content:/Engine/Test/lut.bin` byte for byte; failure cases: an asset's logical path and an absent path are `ASSET_ERROR_NOT_FOUND` from `GetVerifiedFileSize`, and a fixture whose file bytes differ from the catalogue's hash completes `CORRUPT` with `ASSET_CORRUPTION_FILE_HASH_MISMATCH` |
| `Test_AssetLoader_CancelledCountersByStage` | `Zenith_AssetLoader_Tests.cpp` | one cancellation in each of QUEUED, HEADER_READ, READING, DECODING, DELIVER and ACTIVATING (reached with the gated mount source and the stage hold) raises exactly that `asset.loads.cancelled.<stage>` by 1, and the six sum to `asset.loads.cancelled`; failure case: an operation completed before the cancel raises none |
| `Test_AssetLoader_DependencyFailedRootCause` | same | a dependent of a chain A → B → C whose C fails `OUT_OF_MEMORY` fails `DEPENDENCY_FAILED` with detail `ASSET_ERROR_OUT_OF_MEMORY` at both A and B, `m_xAsset` naming each failing asset itself; failure case: a C failing `CORRUPT` gives detail `ASSET_ERROR_CORRUPT`, never `DEPENDENCY_FAILED` |

## Back-pressure tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetBackPressure_BurstNeverExceedsDepth` | `Zenith_AssetBackPressure_Tests.cpp` (also `linux-tsan`) | 5,000 requests at once: sampled every I/O and decode event, reserved units ≤ D, staging ≤ S, reads in flight ≤ D; all complete |
| `Test_AssetBackPressure_IOWaitsForDecodeCapacity` | same | with decode slices paused (fault point), no read beyond D is issued; resuming decodes issues the waiting reads |
| `Test_AssetBackPressure_PriorityReserves` | same | BACKGROUND loads never hold more than ⌊D/2⌋ units; NORMAL never more than D − ⌈D/4⌉; a CRITICAL request issues while prefetch saturates |
| `Test_AssetBackPressure_StreamedLargeSection` | same | a section of 8 × S stored bytes decodes streamed with staging never above 2 × chunk and the result equals a one-shot decode |
| `Test_AssetBackPressure_DecodeConcurrencyCap` | same | running decode slices never exceed C |
| `Test_AssetBackPressure_IOQueueFullRetries` | same | `IO_ERROR_QUEUE_FULL` from a fault-injecting AsyncIO returns records to waiting, counts, and all complete |
| `Test_AssetBackPressure_CountersPublished` | same | the back-pressure counters match the test's own accounting; every queue of Loading.md "Internal queues" publishes `.full`, `.dropped` (0) and `.high_water`, and a full intake queue refuses `RequestLoad` with `QUEUE_FULL` and counts `.full` |

## Residency tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetResidency_AccountingPerSubsystem` | `Zenith_AssetResidency_Tests.cpp` | resident bytes reported by the fake loader sum per subsystem; the consumer reports only ASSETS bytes plus ZenithAssets' own |
| `Test_AssetResidency_KeepAliveCacheLRU` | same | zero-reference cacheable assets enter the cache; over budget the least recently released unloads; acquire revives without I/O; a cacheable GPU-kind asset whose residency is FAILED or LOST unloads at zero references instead of caching |
| `Test_AssetResidency_WarningFlushesCache` | same | `ForceLevel(WARNING)`: within one frame every CACHED asset unloads and caching stops; back to NORMAL it resumes the frame after |
| `Test_AssetResidency_CriticalDropsCpuCopies` | same | `ForceLevel(CRITICAL)`: `DropCpuCopies` called once per loader of a kind in `GetGpuResidentKinds()` (the standard GPU test kinds 0x7F02, 0x7F03); the fake loader drops the copies of RESIDENT and FAILED objects only and keeps a PENDING object's; bytes counted |
| `Test_AssetResidency_PressureChangesNoOutcome` | same | the same request sequence under NORMAL and forced CRITICAL publishes identical completion records in completion-ID order |
| `Test_AssetResidency_InFlightBudget` | same | loads in flight and their bytes never exceed the limits; a single oversized file is admitted alone; with `asset.load.maxInFlight` = 1, 16 requests for the heads of dependency chains three deep all complete (pipelines waiting for dependencies hold no record, so no wait cycle forms), and the live load records never exceed 1; a kind whose loader desc sets `m_uMaxConcurrentLoads` = 2 never has more than 2 records while 10 of its pipelines and 10 of another kind are queued at one priority, the other kind's pipelines are admitted past the held-back ones, and each kind's pipelines are admitted in request order |
| `Test_AssetResidency_ReloadReservesPeak` | same | a development reload reserves the double-residency peak; a refused reservation defers it |
| `Test_AssetResidency_UnloadReloadStress` | same (`z2-stress`) | 1,000 cycles of acquire, load, release, reacquire over 200 assets with random cache pressure leave no slot, byte, record or allocation behind |

## Cancellation and stress tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetCancel_WhileQueued` | `Zenith_AssetCancel_Tests.cpp` | nothing read; `CANCELLED` |
| `Test_AssetCancel_DuringDependencies` | same | dependency requests cancelled; unrelated dependents unaffected |
| `Test_AssetCancel_DuringIO` | same | reads cancelled; staging freed only after the read reports (a guard-page destination proves no write after release) |
| `Test_AssetCancel_DuringDecode` | same | the decode slice stops at its next boundary; its context and staging freed |
| `Test_AssetCancel_DuringUpload` | same | the fake loader's `Cancel` releases its partial resources and acknowledges; the operation completes `CANCELLED` after that |
| `Test_AssetCancel_AfterActivationSucceeds` | same | a cancel racing `Activate` completes `SUCCEEDED` |
| `Test_AssetCancel_OneOfSeveralRequests` | same | the pipeline continues for the other requests |
| `Test_AssetCancel_HandleDestructionCancels` | same | destroying an incomplete operation handle cancels and detaches |
| `Test_AssetCancel_Stress1000Cycles` | same (`z2-stress`, also `linux-tsan`) | 1,000 issue-then-cancel cycles at randomised stages (seed printed): afterwards no slot above baseline, no staging, no unit, no record, no operation, no fake-loader object, zero heap delta ([Z2-N-CONTRACT-339]) |
| `Test_AssetCancel_StreamingStress1000Cycles` | same (`z2-stress`) | as above over assets with hard dependencies, dictionaries, streamed sections and an asynchronous upload stage; 1,000 cancel/reload cycles leave no reference, allocation or object behind |

## Missing and corrupt content tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetFailure_EachCorruptionClassFails` | `Zenith_AssetFailure_Tests.cpp` | one fixture per `Zenith_AssetCorruption` value fails with it, never asserting |
| `Test_AssetFailure_DecodeCorruptionFails` | same | flipped compressed bytes (zlib, zstd) fail `DECODE_FAILED` or `BAD_SECTION_CHECK` |
| `Test_AssetFailure_IOErrorFails` | same | an injected read error is `IO` with the I/O code |
| `Test_AssetFailure_FallbackPerPolicy` | same | SUBSTITUTE resolves to the fallback; NULL_OBJECT to null; FAIL to the error; an ACTIVE object whose residency is FAILED resolves by the same rule through `ResolveOrFallback` while `Resolve` still returns it |
| `Test_AssetFailure_FallbackPinnedAtRegistration` | same | a SUBSTITUTE kind's fallback is loaded at CRITICAL at registration; a failing fallback fails startup's check |
| `Test_AssetFailure_LoggedOncePerClass` | same | one error record per asset per class |
| `Test_CookedCompat_MismatchedFixturesRefused` | same | the committed fixtures cooked at engine cook version 0 and format version 0 are refused with `VERSION_MISMATCH` (the Stage 6 compatibility demonstration uses the shipped release line's fixtures) |

## Fuzz targets

| Target | Input | Asserts |
|---|---|---|
| `Zenith_Fuzz_CookedHeader` | arbitrary bytes as a header and table | no crash, no sanitizer finding; accepted inputs re-encode identically |
| `Zenith_Fuzz_Catalogue` | arbitrary bytes as a catalogue | as above |
| `Zenith_Fuzz_SectionDecode` | a valid header with fuzzed section bytes and descriptors | decode returns an error or the declared bytes |
| `Zenith_Fuzz_LoadResult` | arbitrary 0–64 bytes | decode never crashes |
| `Zenith_Fuzz_Archive` | arbitrary bytes as a `.zpak` in an in-memory container source | `Zenith_CreateArchiveMountSource` never crashes; an accepted archive's `Lookup`, `List` and `Read` of every entry stay within its bounds |
| `Zenith_Fuzz_MetaDocument` | arbitrary JSON bytes as a `.meta` | reading never crashes; accepted documents write canonically and re-read equal |
| `Zenith_Fuzz_DdcEntry` | arbitrary bytes as an entry | verification never crashes and never accepts a corrupted valid entry |
| `Zenith_Fuzz_AssetDatabase` | arbitrary bytes as `assetdb.zdb` | never crashes; failures discard the file |

Seeds are built in code by each target at start-up (no committed corpus): one valid input per format and one input per check.

## Archive and built-in kind tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetArchive_EachCheckRejects` | `Zenith_AssetArchive_Tests.cpp` | one mutated archive per check A1–A7 of Formats.md "Archive checks" fails with `FILE_ERROR_ARCHIVE_CORRUPT` and its check number logged; nothing is leaked (`Create`-style unwinding over both fault points) |
| `Test_AssetArchive_LookupListRead` | same | file and directory lookups, a name that is neither, `List` of the root and of nested directories in byte order, reads at offsets 0, mid and end, a read past the end (`SHORT_READ`), case-sensitive lookup |
| `Test_AssetArchive_MountedThroughVFS` | same | a `Zenith_VirtualFileSystem` created with `&Zenith_CreateArchiveMountSource` as its archive factory over a fixture install directory resolves, lists and reads `content:/` paths from two archives by `order`, and `Zenith_AssetReader` loads a cooked asset from one (pending PLAT's factory member, AS22) |
| `Test_AssetArchive_DataHashesMatch` | same | `Zenith_VerifyArchiveData` accepts a valid archive and names the first entry whose bytes were mutated |
| `Test_AssetArchive_LimitsAtAndAboveLimit` | same | entries, string bytes and path length at their limits accepted and at limit + 1 refused |
| `Test_AssetBuiltIn_DataAndDictionaryLoad` | `Zenith_AssetBuiltIn_Tests.cpp` | a `.zdata` loads into `Zenith_DataAsset` with identical bytes; a `.zdict` loads into `Zenith_CompressionDictionaryAsset` whose dictionary decodes a zstd section that names its ID; a dictionary of the wrong ID fails the dependent's decode with `DECODE_FAILED` |

## Database tests

| Test | File | Asserts |
|---|---|---|
| `Test_AssetDatabase_ScanClassifiesFiles` | `Tests/Database/Zenith_AssetDatabase_Tests.cpp` | the classification table of Database.md "What is an asset source" over a fixture tree |
| `Test_AssetDatabase_MissingMetaFailsBuildCook` | same | a source without `.meta` is `COOK_ERROR_META_MISSING` and nothing is written into the source tree (a tree hash before and after) |
| `Test_AssetDatabase_AssignGuidsWritesMeta` | same | `--assign-guids` writes canonical v1 `.meta` files through the journal with fresh version-4 GUIDs |
| `Test_AssetDatabase_GuidsNeverReused` | same | a GUID present in any content root's tombstone list or in the index is never assigned |
| `Test_AssetDatabase_TombstoneLists` | same | deleting a source and its `.meta` adds the GUID to `.zenith-tombstones` of its content root (never a top-level entry of `Games/<Name>/`) on the next sidecar-writing run and never on a build cook; a build cook whose database knew the deleted GUID and whose tracked list lacks it reports `COOK_WARNING_TOMBSTONE_NOT_TRACKED` with the exact line to add, and a fresh database with the line in the list never assigns the GUID; a list with duplicated and unsorted lines reads and is rewritten sorted and deduplicated; a malformed line is `COOK_ERROR_GAME_INVALID` naming the line; the scan never treats the file as content |
| `Test_AssetDatabase_CookResolverForDefaults` | same | `Zenith_ResolveCookAssetReferenceDefault` returns true for a live record of the kind, false for an absent, tombstoned or other-kind GUID, and always leaves a null element |
| `Test_AssetDatabase_DuplicateGuidRejected` | same | two `.meta` with one GUID fail the scan; `--fix-duplicate-guids` keeps the recorded path's GUID |
| `Test_AssetDatabase_MoveKeepsGuid` | same | moving source and `.meta` keeps the GUID, changes the logical path, marks nothing dirty |
| `Test_AssetDatabase_ProbableMoveWarned` | same | a source moved without its `.meta` produces `COOK_WARNING_PROBABLE_MOVE` |
| `Test_AssetDatabase_NonNfcNameRejected` | same | a decomposed file name fails `NAME_NOT_NFC` |
| `Test_AssetDatabase_LogicalPathCollision` | same | two sources mapping to one logical path fail naming both |
| `Test_AssetDatabase_CorruptFileRebuilds` | same | a damaged database file is discarded and rebuilt with identical records |
| `Test_AssetDatabase_StatCookieOnlyHint` | same | a changed modification time with identical bytes re-hashes and marks nothing dirty |
| `Test_AssetDependencyGraph_TopologicalOrderStable` | `Tests/Database/Zenith_AssetDependencyGraph_Tests.cpp` | ties broken by GUID; cycles reported |
| `Test_AssetMeta_CanonicalRoundTripIsIdentity` | `Tests/Database/Zenith_AssetMeta_Tests.cpp` | a canonical document round-trips byte for byte |
| `Test_AssetMeta_UnknownFieldsPreserved` | same | unknown and `x-` members survive a settings change in position, with one warning each |
| `Test_AssetMeta_EachValidationMessage` | same | one fixture per message of Formats.md "Validation messages" |
| `Test_AssetMeta_NewerVersionReadOnly` | same | a `schemaVersion` 2 document is read, never written |
| `Test_Provenance_RecordComplete` | `Tests/Database/Zenith_Provenance_Tests.cpp` | every field of Database.md "Provenance record content"; no time or host in it; two cooks give identical lines |
| `Test_Provenance_QueryByEachForm` | same | GUID, logical path and cooked path find the same record |
| `Test_AssetDatabase_StoresImportDiagnostics` | `Tests/Database/Zenith_AssetDatabase_Tests.cpp` | `UpdateAfterImport` keeps a result's diagnostics and dropped count on the source record; `Save` then `Open` round-trips them through the version-3 "import diagnostics" part byte for byte; a record without diagnostics has no row; a failed import clears them; failure cases: a version-2 file, a row naming no record and a blob with a bad check each discard the file and rebuild it with no stored diagnostics |
| `Test_ImportDiagnosticsBlob_RoundTripAndChecks` | `Tests/Database/Zenith_ImportDiagnosticsBlob_Tests.cpp` | the encoder writes the hand-computed 32-byte empty blob and the hand-written golden of Formats.md "Import diagnostics blob" for two entries; decode inverts encode; failure cases, one per check (magic, version, reserved byte, 257 entries, length off by one, check, level 4, code `cook_x`, code `COOK_X`, invalid UTF-8, a message of 4,097 bytes, an offset gap), each refused |
| `Test_AssetDatabase_InputOnlyDocumentsClassified` | `Tests/Database/Zenith_AssetDatabase_Tests.cpp` | `*.generator.json`, `*.lightbake.json`, `*.lightpage.json`, `*.terraintile.json`, a region `*.scene.json` under `Worlds/W/Regions/` with `Worlds/W/W.world.json` present, and a `.bin` a previous import recorded as an input are each visited, hashed, not imported and not warned; failure cases: the same region scene with no `W.world.json` is an ordinary scene source needing a `.meta` (`COOK_ERROR_META_MISSING`), and a `.bin` no import recorded is `COOK_WARNING_UNRECOGNISED_FILE` |
| `Test_AssetDatabase_PresetChosenAtCreation` | same | `AssignMissingGuidsWithPresets` over two sources and two engine presets of different priority writes the higher preset's GUID and labels into each new `.meta` and lists `{source, preset, glob}` in source order; a source no preset matches gets `"preset": null` and glob `""`; an existing `.meta` keeps its preset; failure case: a preset document whose `match` block is invalid makes `Zenith_ImportPresetSet::Build` fail and the run exit 3 naming it, writing no `.meta` |

## Import tests

| Test | File | Asserts |
|---|---|---|
| `Test_Import_SettingsResolutionOrder` | `Tests/Import/Zenith_ImportSettings_Tests.cpp` | defaults < preset < settings < platform override, key by key; the settings hash changes with each layer |
| `Test_Import_SettingValidation` | same | wrong type, out of range, unknown enumeration — each `SETTING_INVALID` naming the key |
| `Test_Import_CompressionSettingValidated` | same | COMPRESS's table rows through `.meta` |
| `Test_Import_SameInputsSameIntermediateBytes` | `Tests/Import/Zenith_ImportDriver_Tests.cpp` | the fake importer run twice, with 1 and 8 workers, writes identical intermediates |
| `Test_Import_ReadInputRecorded` | same | a declared input's change triggers reimport; an input outside the roots is `INPUT_OUTSIDE_ROOT` |
| `Test_Import_ReimportTriggers` | same | each trigger of Import.md "Reimport triggers" reimports; nothing else does |
| `Test_Import_ReimportKeepsSubAssetGuids` | same | surviving keys keep GUIDs; removed keys orphaned; new keys new; `RenameProduct` moves a GUID |
| `Test_Import_WorkerCrashContained` | same | an out-of-process fake importer that crashes fails its item with `WORKER_CRASHED`; other items succeed |
| `Test_Import_DiagnosticCodesValidated` | `Tests/Import/Zenith_ImportDriver_Tests.cpp` | both `Report` overloads share the 256 cap and the closing record; a valid code reaches the result; an empty code and the one-argument overload give no code; failure cases: codes `lower`, `A-B`, a 65-byte code, `COOK_X` and `IMPORT_X` are dropped with the message prefixed `invalid diagnostic code: `; level 9 is recorded as ERROR; invalid UTF-8 becomes U+FFFD; a 5,000-byte message is cut to 4,096 bytes on a code-point boundary |
| `Test_Import_OpenInputDocumentRelativeTo` | same | a fake importer anchored on a terrain-like source opens `Tiles/0_0.tile.json` relative to it: the migrated document is returned, the Zenith2-relative path and hash are recorded once for two opens, editing the tile re-imports the importing source, an absent tile returns found = false with the zero hash recorded and creating it re-imports; failure cases: `../x.json`, `C:/x.json`, a path into `Tests/` are `INPUT_OUTSIDE_ROOT`, an unknown anchor GUID is `INPUT_NOT_FOUND`, a document that fails to open is `SOURCE_INVALID` |
| `Test_Import_PublishedPropertyInputs` | same | the waves of Import.md "Published-property inputs" over the `Chain` fixture: a NEW importing source runs twice (the first run deferred, its intermediates and diagnostics discarded) and records the property with its hash; a no-change cook imports nothing; a producer change that alters the property re-imports with `REIMPORT_TRIGGER_PROPERTY_INPUT`; a producer re-cook that leaves the property bytes alone (a test step version bump) re-imports nothing; an Android cook reading the Windows property cooks the Windows item into the DDC and leaves the Android tree without it; failure cases: a property the step does not publish and a GUID with no record are `INPUT_NOT_FOUND`, two importers each reading the other's product fail with the import-cycle message, an invalid name is `INVALID_DESC` |
| `Test_Import_WorkerDiagnosticsThroughDdc` | same | an out-of-process fake importer that makes 300 `Report` calls stores 256 diagnostics and a dropped count of 44 in its import record entry, the `workResult` line carries none, and the cook report lists the same 257 entries as an `--in-process` cook; failure case: with the import's record entry deleted from the local store after the cook, the next cook finds no hit, runs the import again and lists the entries live |
| `Test_Import_InputDocuments` | `Tests/Import/Zenith_ImportDriver_Tests.cpp` | `OpenSourceDocument`, `OpenInputDocument` and `OpenInputDocumentByGUID` return version-2 migrated documents of version-1 fixtures, record the relative and by-GUID paths once each, and an edit of either input re-imports; failure cases: a worker-process call is `INVALID_DESC`, an unknown GUID `INPUT_NOT_FOUND`, a 64 MiB + 1 file `LIMIT_EXCEEDED` |
| `Test_Import_ReadSourceProductsInputs` | same | an importer reading another NEW source's products is deferred once, then receives them in key order; a no-change cook imports neither; adding a product to the other source re-imports the reader with `REIMPORT_TRIGGER_PRODUCT_LIST_INPUT`, a byte change that keeps the product list does not; failure cases: two sources reading each other's products fail with the import-cycle message, an unknown GUID is `INPUT_NOT_FOUND` |
| `Test_Import_ProductMembersAndFingerprints` | same | `GetPreviousProducts` returns the pre-import entries with members and fingerprints; `SetProductFingerprint` writes `fingerprint` in sidecar mode only; a `RenameProduct` and a server-side remap move every owner member with the GUID; an orphaned entry keeps them; changing `exclude` or `materialRemap` re-imports while changing `fingerprint` or `provenance` does not; a tombstoned GUID on a live entry is accepted; failure case: `SetProductFingerprint` of an undeclared GUID is `INVALID_DESC` |
| `Test_Import_DerivedProducts` | same | a derived product gets its importer's GUID, a sub-asset record and no `subAssets` entry in any mode, never `META_SUBASSET_MISSING`, and is deleted, not orphaned, when no longer declared; failure cases: a derived key or GUID equal to a `.meta` entry, an earlier declaration or a tombstoned GUID is `DUPLICATE_PRODUCT_KEY` naming the other holder |
| `Test_Import_PresetImporterMismatch` | `Tests/Import/Zenith_ImportSettings_Tests.cpp` | a `.meta` naming a preset of its own importer resolves layer 2 from it; failure cases: a preset of another importer is `SETTING_INVALID` naming both importers and paths, and a preset failing `Zenith_ValidateImportPresetJson` is `SETTING_INVALID` with CONTENT's first message |
| `Test_Import_ThirdPartyInputs` | `Tests/Import/Zenith_ImportDriver_Tests.cpp` | `ReadInput("thirdparty:fixture-data/a.txt")` over a test roots table returns the file and records the path text, not the host path, so two roots tables with different directories give equal keys; failure cases: an entry not in the table is `INPUT_NOT_FOUND`, `thirdparty:fixture-data/../x` is `INPUT_OUTSIDE_ROOT` |

## Cook tests

| Test | File | Asserts |
|---|---|---|
| `Test_Cook_ExitCodes` | `Tests/Cook/Zenith_Cook_Tests.cpp` | each exit code of Cook.md "Exit codes" from a fixture |
| `Test_Cook_PhasesOrdered` | same | global-pre before assets before global-post; `asset.catalogue` last |
| `Test_Cook_ContentSelectionFromComposition` | same | only the selected modules' engine content and fragments are scanned |
| `Test_Cook_ConfigFragmentMerge` | same | fragments merged, a duplicate key across fragments is `CONFIG_KEY_CONFLICT`, the game file overrides, output in key order |
| `Test_Cook_CatalogueAndManifest` | same | the catalogue lists every cooked asset and file; the manifest validates with `Zenith_ParseMountManifest` |
| `Test_Cook_OutputTreeLayout` | same | object paths follow `Objects/<xx>/<guid>.<ext>`; `.cook/` holds the report |
| `Test_Cook_OutputIndependentOfWorkerCount` | same | 1, 2 and 8 workers give byte-identical trees |
| `Test_Cook_DepFileListsInputs` | same | every read file appears once, sorted; a change to any listed input reruns |
| `Test_Cook_IncrementalNoOpCooksNothing` | same | a second cook with no change runs no step |
| `Test_Cook_CleanEqualsIncremental` | same | the mutation sequence of Cook.md "Output reuse" |
| `Test_Cook_DependencyChangePropagates` | same | a texture resolution change re-cooks its material's meshes; an output-identical change stops propagation |
| `Test_Cook_StructuralValidationRejects` | same | a step writing a bad file fails `OUTPUT_INVALID` and publishes nothing |
| `Test_Cook_MissingReferenceFails` | same | a hard reference to an absent GUID fails the referrer; a soft one warns |
| `Test_Cook_ContentRuleEnforced` | same | a fixture rule over the limit is `COOK_ERROR_RULE` naming both values |
| `Test_Cook_StepFailurePublishesNothing` | same | a failing step leaves no temporary or partial output; dependents fail; independents publish |
| `Test_Cook_InterruptedResumesToCleanBytes` | same | kill after N publications (N sampled), resume, compare with clean |
| `Test_Cook_LowDiskStopsCleanly` | same | a fake volume below the margin stops with exit 4 and no temporary left |
| `Test_Cook_ConcurrentServerAndBuildExclusive` | same (runs real `ZenithCook` processes) | the six cases of Cook.md "Output tree and database locks" (the storage-root cases are the last three paragraph cases there): a build cook against a serving tree exits 6 with `COOK_ERROR_OUTPUT_LOCKED` naming the server's PID, leaving the tree, the journal and the server's `*.<pid>.z2cook` temporaries byte-identical, and succeeds after the server's `shutdown`; a server started while a build cook holds the tree answers `hello` with `COOK_ERROR_OUTPUT_LOCKED` and exits 6; a cook of the other platform while the database lock is held succeeds with `COOK_WARNING_DATABASE_LOCKED` and leaves `assetdb.zdb` unchanged; with a helper holding a storage root's writer lock and a journal, `--assign-guids` exits 6 with `COOK_ERROR_SOURCE_LOCKED` after `cook.sourceLockWaitMs` leaving the journal and every `.meta` byte-identical, a build cook exits 6 (not 3) with no command in its message and succeeds once the helper finishes, and a server's `setSettings` is answered `COOK_ERROR_SOURCE_LOCKED` with the session continuing; two concurrent `--assign-guids` over one new source give one `.meta` and one GUID |
| `Test_Cook_StaleOutputsRemoved` | same | removing a source removes its cooked file and catalogue entry |
| `Test_Cook_EngineFallbacksInEveryCatalogue` | same | a fixture game referencing no engine content still cooks every declared engine fallback and lists one fallback-table row per (kind, role) |
| `Test_Cook_FallbackMissingFails` | same | a declared fallback path absent from the database stops the cook with exit code 3 and `COOK_ERROR_FALLBACK_MISSING` |
| `Test_ContentRules_TableAgreesWithContentSpecs` | `Tests/Cook/Zenith_ContentRules_Tests.cpp` | every rule table of `Zenith2/Docs/ContentSpecs.md` (read as Markdown in the test) has the same classes and limits in `Zenith2/Docs/ContentSpecs.rules.json` |
| `Test_Cook_ContentRuleTableInvalid` | same | a table failing its schema, and a row naming an unregistered quantity, are `COOK_ERROR_RULE_TABLE_INVALID` (exit 3); the cook opens no `.md` file (a file-access trace of the run) |
| `Test_CookContext_OpenSourceDocument` | `Tests/Cook/Zenith_CookContext_Tests.cpp` | the cases of Cook.md "Source documents in cook steps", through ZenithCook's driver over a fixture game with a test document type (versions 1 and 2) and a test step: with 8 in-process workers, 256 items each opening its own document and 4 shared ones return the SHA-256 of each file's bytes, no services migrator member runs after the scan, and the pool holds all 9 sets after `Run`; an unknown and a tombstoned GUID are `COOK_ERROR_MISSING_REFERENCE`; a document rewritten by a test hook between plan step 6 and its item is `COOK_ERROR_SOURCE_CHANGED` (a deleted one with detail `DOCUMENT_ERROR_NOT_FOUND`), that item publishes nothing and the others publish; a version-1 document opens `DOCUMENT_OPEN_MIGRATED`, a version-3 one `DOCUMENT_OPEN_READ_ONLY_NEWER` and succeeds, a malformed one is `COOK_ERROR_STEP_FAILED` with `DOCUMENT_ERROR_PARSE`; a `Cook` opening a document its `AddKeyInputs` did not, and the step forced out of process, are `COOK_ERROR_INVALID_STEP`; changing one byte of a document an item's `AddKeyInputs` opened changes that item's key and re-cooks it, and a second cook re-cooks nothing |
| `Test_Cook_RefusesPendingJournal` | `Tests/Cook/Zenith_Cook_Tests.cpp` | a `*.z2journal` at a storage root whose writer lock no process holds (a writer that exited) stops the build's cook with exit code 3 naming it; a sidecar-writing mode recovers it first |
| `Test_Cook_ReportDeterministic` | same | two cooks write identical reports |
| `Test_CookServer_Protocol` | `Tests/Server/Zenith_CookServer_Tests.cpp` | every request of Formats.md "Requests" through an in-memory `Zenith_LineStream` pair: each `args` key's presence and type rule, each `result` shape, errors, progress (with `id` null during a watch-driven run); `provenance` pages its records breadth-first with `count` and `pages`, and a record over the line limit ends the request with `COOK_ERROR_RESULT_TOO_LARGE` while the session continues; `shutdown` sent while a `cook` runs is answered on arrival: the queued requests get `COOK_ERROR_CANCELLED` at once, the running one at its next item, then the `shutdown` `result`, exit code 0 and the output lock released |
| `Test_CookServer_HelloHandshake` | same | a request before `hello` is `COOK_ERROR_PROTOCOL`; a `protocolVersion` or `engineCookVersion` mismatch is `COOK_ERROR_PROTOCOL_VERSION` and exit code 2 |
| `Test_CookServer_PagedResults` | same | a `list` of 1,000 records arrives as 4 pages of at most 256 items, each line under 4 MiB, then a `result` with `count` 1,000 and `pages` 4; a line over 4 MiB from the client is refused and skipped |
| `Test_CookServer_MalformedAndUnknownRequests` | same | invalid JSON, a missing `id`, an unknown request name, an unknown `args` key and a wrong type each give `COOK_ERROR_PROTOCOL` naming it and the session continues; a second `hello` gives `COOK_ERROR_PROTOCOL` under its own `id`; a request reusing the `id` of an unfinished one gives `COOK_ERROR_PROTOCOL` with `id` null and the unfinished request still ends with its own final line; 16 in a row end it with exit code 2 |
| `Test_CookServer_CancelQueuedAndRunning` | same | cancelling a queued request answers it `COOK_ERROR_CANCELLED` at once; cancelling the running `cook` stops it at the next item with its sent pages standing; cancelling an unknown `id` returns `cancelled: false`; `cancel`'s own result precedes the target's final line |
| `Test_CookServer_WatchNotifies` | same | with watching enabled, touching a fixture source produces one `changed` after the debounce, then a `published` naming its GUID with `catalogueChanged` |
| `Test_CookServer_WorkerHelloMismatch` | same | a worker whose `workerHello` differs stops the cook with exit code 5 and `COOK_ERROR_PROTOCOL_VERSION` |
| `Test_CookServer_AbsentFromShipping` | same (link check through the `Libgraph` step's Shipping closure) | no cook code in any Player closure |
| `Test_CookTestContexts_CodedImportDiagnostics` | `Tests/Cook/Zenith_CookTestContexts_Tests.cpp` | `Zenith_MakeImportTestContext` captures each `Report` with its code as `Zenith_CapturedImportDiagnostic::m_strCode` (empty for the one-argument overload) under the import context's own validation; failure case: an invalid code is captured empty with the prefixed message, exactly as the driver stores it |
| `Test_CookTestContexts_RelativeDocumentInputs` | same | an import test context whose desc lists an anchor record and its relative documents answers `OpenInputDocumentRelativeTo` with the migrated document and lists the joined path in `GetInputsRead`; failure cases: an absent document returns found = false and is still listed, an unlisted anchor is `INPUT_NOT_FOUND` |
| `Test_CookTestContexts_PublishedPropertyInputs` | same | an import test context whose desc lists published properties answers `ReadPublishedProperty` with their bytes and captures each read once with its hash in `GetPropertyInputsRead`; failure case: an unlisted triple is `INPUT_NOT_FOUND`, never `PROPERTY_PENDING` (a test context has no waves) |
| `Test_CookContext_FindAssetByLogicalPath` | `Tests/Cook/Zenith_CookContext_Tests.cpp` | a test step's `FindAssetByLogicalPath` finds a source and a `#`-suffixed sub-asset of the plan's snapshot, through a cook and through `Zenith_MakeCookTestContext`; failure cases: an absent path, a tombstoned record's path, ``, `Assets/x` (no `content:/`) and a 1,025-byte path return null, and a worker-process context returns null |
| `Test_CookReport_ReplaysStoredDiagnostics` | `Tests/Cook/Zenith_Cook_Tests.cpp` | a fake importer reporting a coded warning and an uncoded info: the first cook lists both live (codes `FAKE_WARNING_STALE` and `COOK_IMPORT_DIAGNOSTIC`), a second cook with no change lists the same two with `"replayed": true`, a third after deleting `assetdb.zdb` imports again and lists them live; failure case: a source whose import now fails lists its live diagnostics and `COOK_ERROR_IMPORT_FAILED`, and the next cook replays nothing for it |
| `Test_CookReport_ReplayedEqualsLive` | same | a clean cook, an incremental cook after an unrelated source change, and a cook over a fresh output tree and database with a pre-populated DDC (from Stage 4) give reports equal once every `replayed` member is removed, with 1 and 8 workers; a 300-call importer contributes 257 entries in each |
| `Test_CookReport_ReplaysOnSharedDdcHit` | same | a second machine's cook (a fresh output tree, database and local store, `cook.ddc.sharedMode=read` over the first cook's store as a shared store) hits every import, runs no `Import`, and its report lists the world-like fake importer's `FAKE_WARNING_STALE` entries with `"replayed": true`; failure case: a shared entry whose `asset.importDiagnostics` payload is corrupted is a miss, the import runs and the entries are live |
| `Test_CookServer_PropertyRequest` | `Tests/Server/Zenith_CookServer_Tests.cpp` | over an in-memory line stream: `property` for three GUIDs (one twice) answers one item per distinct GUID in GUID order with the base64 bytes the producer published, cooking the misses into the DDC and writing nothing to the tree; failure cases: a GUID with no record and a step that publishes no such property answer `COOK_ERROR_MISSING_REFERENCE` items, a failing producer its error code, and 100,001 GUIDs or a name `Bad` are `COOK_ERROR_PROTOCOL` |
| `Test_CookContext_RepositoryAndThirdPartyFiles` | `Tests/Cook/Zenith_CookContext_Tests.cpp` | a GLOBAL_POST test step reads a repository file and a `thirdparty:` file in `AddKeyInputs` and again in `Cook`; both join its key and the depfile; editing either re-runs the step; `GetPlanOrigins` lists every live record in GUID order with canonical provenance text; failure cases: `ReadRepositoryFile` from an ASSET-phase step, a read in `Cook` not keyed in `AddKeyInputs`, bytes changed between the two calls (`SOURCE_CHANGED`) and a file above `ulMaxBytes` (`COOK_ERROR_RULE`) |
| `Test_CookReport_ImportReportMembers` | `Tests/Cook/Zenith_Cook_Tests.cpp` | two fake-importer sources each calling `AddReportMember("fakeImports", …)` give one `members.fakeImports` array of two values in source GUID order, identical in a no-change cook (replayed from storage); failure cases: a value that does not parse is dropped with an ERROR diagnostic, and a step contributing the same name fails `COOK_ERROR_INVALID_STEP` |
| `Test_Cook_GameRootAndSchemaLock` | same | `--game-root <scratch>` cooks and serves a scratch copy of `Tiny` leaving `Zenith2/Games` untouched; `--schema-lock` writes `schemas.lock.json` while a `--serve` holds the output lock; failure cases: a `--game-root` outside the allowed directories is exit 2, and `--schema-lock` with the game root's writer lock held by a helper exits 6 `COOK_ERROR_SOURCE_LOCKED` writing nothing |
| `Test_Cook_OnlyListRestrictsRun` | same | `--only @list.txt` naming one source cooks its products and their dependencies, runs no global step, writes no catalogue, populates the local DDC, and a following full cook hits those entries; failure cases: a malformed line, an unknown GUID and a 1,048,577-line file each exit 2 naming the line |
| `Test_CookServer_SetSettingsBatchArguments` | `Tests/Server/Zenith_CookServer_Tests.cpp` | `setSettings` with only `subAssetMembers`, only `preset`, only `provenance`, a `remapSubAssets` pair, a `removeOrphanedSubAssets` key and a `restoreSubAssets` entry each write one journaled batch, report the tombstoned GUIDs, and `record` returns `preset`, `provenance` and every member; failure cases: a remap whose `from` is live, a removal of a live entry, a restore whose GUID belongs to another `.meta`, and a request with no optional key are `COOK_ERROR_PROTOCOL` with the `.meta` byte-identical |

## DDC tests

| Test | File | Asserts |
|---|---|---|
| `Test_DerivedDataKey_EachInputChangesKey` | `Tests/DDC/Zenith_DerivedDataKey_Tests.cpp` | each key input row changes the key; path, time and worker count do not |
| `Test_DerivedDataKey_EncodingGolden` | same | a fixed input list gives the committed key bytes (after D-SEL-HASH (open, Stage 1)) |
| `Test_DerivedDataCache_PopulateThenHit` | `Tests/DDC/Zenith_DerivedDataCache_Tests.cpp` | a miss populates; the next lookup hits and verifies |
| `Test_DerivedDataCache_UnreachableFallsBackLocal` | same | an UNAVAILABLE shared store: local cook, identical tree, shared store untouched |
| `Test_DerivedDataCache_CorruptEntryIsMiss` | same | a truncated and a bit-flipped entry are misses; the local one is removed; a shared one untouched |
| `Test_DerivedDataCache_StaleEntryIsMiss` | same | producer fields differing is a miss |
| `Test_DerivedDataCache_VerifyFailureQuarantines` | same | a non-deterministic fake step detected by sampling quarantines the entry |
| `Test_DerivedDataCache_SharedHitCopiedLocal` | same | a shared hit populates the local store |
| `Test_DerivedDataCache_EvictionLRU` | same | over the cap, least recently used entries go first to 90 %; an entry being read survives |
| `Test_DerivedDataCache_ConcurrentPopulateSameKey` | same (`z2-stress`) | 4 processes populating one key leave one valid entry and no temporary |
| `Test_DerivedDataCache_InvalidationPerFingerprintInput` | same | a cook re-cooks exactly the dependent items after each of: a source byte, a setting, an importer version, a step version, a tool version, a codec level, an engine cook version |
| `Test_DerivedDataCache_ImportEntriesRoundTrip` | `Tests/DDC/Zenith_DerivedDataCache_Tests.cpp` | an import populates one intermediate entry per product and then its record entry (manifest and `asset.importDiagnostics`); a fresh database over the same store hits, materialises a byte-identical intermediate area and `Inputs` file and runs no `Import`; `--clean` populates without looking up; failure cases: an evicted intermediate entry, a manifest naming another importer version and a corrupted payload are each a miss that imports again |
| `Test_AuthoringAsset_DerivedDataEntryVerifier` | `Zenith2/Tools/Authoring/Tests/Asset/Zenith_AuthoringAsset_Tests.cpp` | `Zenith_VerifyDerivedDataEntry` accepts an entry the local store populated and returns its producer fields, and accepts a `Zenith_WriteTestDerivedDataEntry` entry of any producer; failure cases: a flipped payload byte, a wrong key, a truncated entry and a bad header check are each `DDC_ERROR_IO` |

## Meta document tests

The framework's own tests (journal interruption at every step, preview writes nothing, backup before write, chain gaps at `Freeze`) are SCENE's (`Zenith2/Docs/Design/SCENE/Tests.md → "Document framework tests"`); ASSET's tests prove that the `.meta` type uses the framework correctly:

| Test | File | Asserts |
|---|---|---|
| `Test_MetaMigration_Fixtures` | `Zenith2/Tools/ZenithCook/Tests/Database/Zenith_MetaMigration_Tests.cpp` | every `v<N>.json` of `AssetMeta`, `ImporterSettings/raw`, `ImporterSettings/zenith-doc` (and CONTENT's importers' directories) migrates through a `Zenith_MemoryDocumentStorage` to `v<N>.expected.json`; a missing pair for a version below current fails |
| `Test_AssetMeta_DocumentTypeRegistered` | same | `Zenith_RegisterAssetMetaMigrations` registers `Zenith.AssetMeta` with the desc of Migration.md "The meta document type"; `FindTypeByPath("x.png.meta")` and `FindTypeBySchemaTag("zenith2.asset-meta")` find it; a second registration is `DOCUMENT_ERROR_DUPLICATE_REGISTRATION` |
| `Test_AssetMeta_SettingsVisitorMigrates` | same | a `.meta` at `settingsVersion` 1 of a fake importer at version 3 opens migrated (`settings` and every `platforms.<p>` object through steps 1 and 2, `settingsVersion` 3); `PlanMigration` marks it changed with the diff; a `settingsVersion` newer than the importer blocks the entry |
| `Test_AssetMeta_RemovedSettingKeptAsXRemoved` | same | a step that removes a setting leaves `x-removed.<key>` with the value in `settings` and in each platform object, with a `DOCUMENT_WARNING_DANGLING_OVERRIDE`, having declared the old path through `m_pfnDeclareRemoved` (the framework's unknown-member guard passes); the same step without the declaration fails; settings resolution ignores the `x-removed.` member |
| `Test_AssetMeta_WritesAreJournaled` | same | a pending journal over a read-only storage makes `RecoverPendingJournals` report `DOCUMENT_ERROR_JOURNAL_PENDING`, which the build's cook maps to exit code 3; `--assign-guids` over a `Zenith_MemoryDocumentStorage` writes new `.meta` files through `SaveDocuments` (a journal at the root during the batch, backups under `.migration/backups/save-*` for changed files); a crash injected after each journal state leaves every `.meta` fully old or fully new after recovery |
| `Test_ImporterRegistry_SettingsChainGapRejected` | `Tests/Import/Zenith_ImporterRegistry_Tests.cpp` | an importer whose `m_apfnSettingsSteps` is shorter than `m_uSettingsVersion` − 1 or holds a null step is refused at `Register` |
| `Test_AssetMeta_SubAssetOwnerMembers` | `Tests/Database/Zenith_AssetMeta_Tests.cpp` | a `.meta` whose entries carry `exclude`, `materialRemap`, `annotation`, `rig`, `fingerprint` and an `x-` member, and a top-level `physics` object, reads and writes canonically in rule-6 order; failure cases: an unknown entry member, a `fingerprint` of 15 digits and an `exclude` string are refused naming the key path |
| `Test_AssetMeta_MemberMigrationTable` | same | the member-migration table is gap-free; a `provenance` with `schema` 1 opens unchanged (fixture `provenance/v1.json` equals `v1.expected.json`); failure case: `schema` 2 opens read-only and a `schema` string is `MIGRATION_ERROR_BAD_INPUT` |
| `Test_AuthoringAsset_MetaRegistrationWithoutCook` | `Zenith2/Tools/Authoring/Tests/Asset/Zenith_AuthoringAsset_Tests.cpp` | `Zenith_RegisterAssetMetaMigrations(registry, nullptr)` from ZenithAuthoring alone registers `Zenith.AssetMeta`, and opening a `.meta` with an older `importer.settingsVersion` leaves its settings unchanged; failure case: a second registration is `DOCUMENT_ERROR_DUPLICATE_REGISTRATION` |

## Integration suite

| Test | Asserts |
|---|---|
| `Test_Integration_CookThenLoad` | the fixture game cooks (CTest fixture `AssetPipeline_Cook`) and a runtime with test loaders loads its startup scene's asset set from the loose mount |
| `Test_Integration_ReimportThenLoad` | after mutating a source, the cook server reimports, the catalogue refreshes, and `RequestReload` activates the new object |
| `Test_Integration_MissingAndCorruptContent` | the corrupt-content fixture set loaded through the whole runtime: every failure classified, every fallback applied, no assert, exit code 0 |
| `Test_Integration_MinimalGameBothPlatforms` | `Games/Minimal` cooks for `windows` and `android` with the same catalogue content hash for platform-independent kinds; the Windows build boots it headless (`Game_Minimal_Boot`); the Android run is the Stage 2 device ticket's (`needs-human`) |

## Fixtures

| Fixture | Location | Content |
|---|---|---|
| kind table | `Zenith2/Engine/ZenithAssets/Tests/Fixtures/KindTable.json` | the entry document's kind table, every column, one object per row |
| standard test kinds | `Zenith_FakeKindLoader::GetStandardTestKinds()` in `Tests/Support/Zenith_FakeKindLoader.h` (header-only, an inline member over an `inline constexpr` array; [Z2-N-ARCH-255]) | the `Zenith_AssetKindInfo` rows below (stable name `Test.Kind<n>`, script name `TestKind<n>`, extension `.ztest`, magic `ZTST`, memory subsystem ASSETS); a test that needs another combination declares its own rows for codes 0x7F05 … 0x7F0F |

The standard test kinds and the tests that rely on each:

| Code | Fallback policy | Cacheable | GPU resident | Used by |
|---|---|---|---|---|
| 0x7F00 | FAIL | yes | no | the general loader, state-machine, cancellation, back-pressure and failure tests; `Test_AssetResidency_KeepAliveCacheLRU` (cacheable CPU kind) |
| 0x7F01 | NULL_OBJECT | no | no | `Test_AssetResidency_KeepAliveCacheLRU` (a non-cacheable kind unloads at zero references); `Test_AssetFailure_FallbackPerPolicy` (NULL_OBJECT) |
| 0x7F02 | SUBSTITUTE | yes | yes | `Test_AssetLoader_GpuKindActivatesBeforeUpload`, `Test_AssetLoader_BootWithoutRendering`, `Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` (a GPU kind whose CACHED objects recovery evicts), `Test_AssetLoader_HeadlessGpuKindsValidateOnly`, `Test_AssetResidency_CriticalDropsCpuCopies`; `Test_AssetFailure_FallbackPerPolicy` (SUBSTITUTE, with a role-0 row in the fixture's fallback table) |
| 0x7F03 | NULL_OBJECT | no | yes | `Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` and `Test_AssetResidency_CriticalDropsCpuCopies` (a GPU kind that is never cached) |
| 0x7F04 | SUBSTITUTE | yes | no | `Test_AssetLoader_BootWithoutRendering` (a CPU kind whose pinned role-0 fallback depends on a 0x7F02 asset); `Test_AssetRegistry_PinnedFallbacksFromCatalogue` |
| archives | `Zenith2/Engine/ZenithAssets/Tests/Fixtures/Archive/` | a valid two-archive install directory with its mount manifest, one mutated archive per check A1–A7 |
| content rules | `Zenith2/Tools/ZenithCook/Tests/Fixtures/ContentRules/` | a valid table, one invalid table per key rule |
| corrupt-content set | generated at test time, never committed: `${ZENITH_OUT}/test-fixtures/ASSET/Corrupt/` (`Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-257]; the entry `ASSET_CorruptFixtures`, fixture `AssetCorruptFixtures`, variable `ZENITH2_ASSET_FIXTURES`, consumers in suite `z2-asset-cooked`) | one file per `Zenith_AssetCorruption` value and per check C1–C12, plus version-mismatch files; each with a `.expected` line naming the error |
| fixture games | `Zenith2/Tools/ZenithCook/Tests/Fixtures/Games/Tiny`, `…/Dependencies`, `…/Corrupt`, `…/Duplicates` | minimal content roots with `.meta` files for the database, import and cook tests |
| pipeline game | `Zenith2/Tests/AssetPipeline/Fixtures/PipelineGame/` | the integration suite's game |
| migration fixtures | `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/AssetMeta/`, `…/ImporterSettings/raw/`, `…/ImporterSettings/zenith-doc/` | `v<N>.json`, `v<N>.expected.json` |
| fuzz seeds | built in code by each fuzz target at start-up (no committed corpus) | seeds |

## Gate steps per test group

| Group | Gate step |
|---|---|
| every `ZenithAssets_Tests`, `ZenithCook_Tests` and integration case | `Test` (`win-debug`, `win-release`) |
| `z2-stress` cases | `Test` (600-second CTest timeout) |
| `linux-tsan` cases (`ZenithAssets_Tests`) | `Sanitizers -Preset linux-tsan`, change-triggered once the PROC path edit lands ("Test targets and gate steps" in the entry document: until then a Stage 2 loader diff is not TSan-checked before merge, an accepted risk); always in stage acceptance |
| `win-asan` runs and fuzz smokes | `Sanitizers -Preset win-asan`, `Test` in `win-asan` |
| fuzz full duration | stage acceptance ([Z2-N-ARCH-952]) |
| `Test_CookServer_AbsentFromShipping` | `Libgraph` (Shipping link closure, [Z2-N-ARCH-241]) |
| contract blocks, headers, layering | `ApiContractCheck`, `HeaderSelfContained`, `Libgraph` |
| `Test_Integration_MinimalGameBothPlatforms` Android half, the S24 load benchmark | `needs-human` device tickets |
