# ASSET requirement catalogue

## Purpose

This catalogue breaks the ASSET inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "ASSET Asset pipeline") into testable requirements `Z2-R-ASSET-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of `Zenith2/Docs/Design/ASSET.md` and its split files under `Zenith2/Docs/Design/ASSET/` that specify it. The design document's "Requirement index" is the reverse map. ASSET owns every requirement here; where a requirement is proved partly by another area's work (a kind loader, a requester's replay path, a device run), that area is named in the evidence and in the design's "Dependencies". Test names resolve to `Zenith2/Docs/Design/ASSET/Tests.md`, which gives each test's file and asserted case. A requirement whose proof depends on a core-document change not yet applied names the pending core-doc item (`X<n>`, the queue `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`) in its acceptance evidence.

## Requirements

### Z2-R-ASSET-001 ZenithAssets library target

- **Statement:** `ZenithAssets` is a layer-3 static library declared with `zenith_add_library` depending on exactly ZenithCore, ZenithReflect, ZenithPlatform, ZenithCompression, ZenithJobs and ZenithProfiling, supported on every preset including `linux-tsan`, with its internal files under `Internal/` as the design lists them.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (declared edges and layer) and `SourceLayout` gate steps on the ticket that creates the target; `CompileCoverage` over its sources.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Target summary"; Zenith2/Docs/Design/ASSET.md → "ZenithAssets internal files"; Zenith2/Docs/Design/ASSET.md → "ZENITH_SUPPORTED matrix"

### Z2-R-ASSET-002 Public header rules and contract blocks

- **Statement:** Every public ZenithAssets header is self-contained, contains no STL or third-party header, and gives every public type one contract block with the Ownership, Thread affinity, Invalidation, Units, Errors and Version fields (plus Reentrancy for interfaces and callback-invoking types).
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-041, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained` and `ForbiddenCheck` gate steps over the headers of RuntimeAPI.md "Header list".
- **Design:** Zenith2/Docs/Design/ASSET.md → "Rules across the ASSET API"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Header list"

### Z2-R-ASSET-003 Closed asset kind table

- **Statement:** `Zenith_AssetKind` is a closed enumeration whose codes, stable names, script names, runtime object types, extensions, magics, owning subsystems, fallback policies, cacheable flags and GPU-resident flags equal the entry document's kind table exactly; names, runtime types, extensions and magics are unique and no code is ever reused.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetTypes_KindTableMatchesDesign` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Asset kinds"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Asset kinds and the kind table"

### Z2-R-ASSET-004 Asset kinds registered as reflected asset types

- **Statement:** Each asset kind is registered with the reflection registry as a `TYPE_KIND_ASSET` type under its stable name by `Zenith_RegisterAssetTypes` at startup step 6 among the engine types, before any module type, with `Zenith_RegisterAssetsConfigKeys` in the same step, so handle fields of every kind serialize by GUID and are editable through reflection.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetHandle_ValueHooksBalance` and `Test_AssetTypes_KindTableMatchesDesign` in the `Test` step; the reflection registry's duplicate-ID check — the composition-root calls are core-doc item X110 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Registration functions"; Zenith2/Docs/Design/ASSET.md → "Construction and shutdown"

### Z2-R-ASSET-005 Asset GUIDs are persistent IDs

- **Statement:** An asset GUID is a `Zenith_PersistentID` that names one asset for the life of the project, is serialized in `.meta` sidecars, catalogues and handle fields, and is never derived from a path or a hash of content.
- **Covers:** Z2-I-ASSET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_AssignGuidsWritesMeta`, `Test_AssetDatabase_MoveKeepsGuid` and `Test_AssetDatabase_GuidsNeverReused` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Asset kinds and the kind table"; Zenith2/Docs/Design/ASSET/Database.md → "GUIDs"

### Z2-R-ASSET-006 Asset handle layout and reference counting

- **Statement:** `Zenith_AssetHandle<T>` is 16 bytes `{registry pointer, slot, generation}`; copying adds a reference, moving transfers one, destruction releases one, and the counts balance after any sequence of copies, moves, assignments and destructions, including concurrent copy and destroy from several threads.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetHandle_LayoutIs16Bytes`, `Test_AssetHandle_CopyMoveDestroy_BalancesReferences` in the `Test` step; `Test_AssetHandle_ConcurrentCopyDestroy` under `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetHandle"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Reference counting and retirement"

### Z2-R-ASSET-007 Untyped handles and reflection value hooks

- **Statement:** `Zenith_AnyAssetHandle` carries the kind with the same reference semantics; `FromAny` of another kind returns `KIND_MISMATCH`; `Zenith_GetAssetHandleValueKindHooks` returns ECS's `Zenith_ValueKindHooks` (16-byte element, null construction, release, add-reference copy, no move hook because stores relocate handles bytewise), which the composition root installs for `PROPERTY_TYPE_ASSET` at startup step 10 before the first schema, and reference counts stay balanced through every reflected store.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-038, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetHandle_FromAnyKindMismatch` and `Test_AssetHandle_ValueHooksBalance` in the `Test` step — the composition-root calls are core-doc item X110 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AnyAssetHandle"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Reflection value hooks"

### Z2-R-ASSET-008 Handle resolution never asserts

- **Statement:** Resolving a null, stale or foreign handle returns `NULL_HANDLE`, `STALE_HANDLE` or `FOREIGN_HANDLE` respectively in every build configuration and never asserts, and a slot whose generation would wrap is retired permanently instead of reissued.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetHandle_NullDoesNotResolve`, `Test_AssetHandle_StaleAfterDestroy_DoesNotResolve`, `Test_AssetHandle_ForeignOwner_DoesNotResolve` and `Test_AssetHandle_GenerationExhaustion_RetiresSlot` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET/Tests.md → "Handle and registry tests"

### Z2-R-ASSET-009 Registry creation unwinds

- **Statement:** `Zenith_AssetRegistry::Create` performs its steps in the documented order and, when any step fails, releases everything earlier steps acquired and returns the step's error, leaving no allocation, job or registration behind.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_Create_FailsAt<Step>_Unwinds` (one case per step, `MissingSet` included, and the desc-validation cases) in the `Test` step and under `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistryDesc"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"

### Z2-R-ASSET-010 Registry slot capacity

- **Statement:** The registry holds at most `asset.registry.maxAssets` slots allocated once at `Create`; an acquire of a new GUID when every slot is in use returns `REGISTRY_FULL` and changes no state.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045, Z2-D-046
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetRegistry_CapacityExhaustionRefuses` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Slot header layout"; Zenith2/Docs/Design/ASSET.md → "Configuration keys"

### Z2-R-ASSET-011 Zero-reference retirement

- **Statement:** When an asset's reference count reaches zero it is retired through the retire queue by the service (cached, unloaded or freed per the state machine), and an acquire that races retirement revives the slot without unloading the object.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_RevivalFromZeroReferences` and `Test_AssetResidency_KeepAliveCacheLRU` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Reference counting and retirement"; Zenith2/Docs/Design/ASSET/Loading.md → "Unload"

### Z2-R-ASSET-012 Registry outlives handles and reports leaks

- **Statement:** The registry is owned by the composition root and destroyed after every handle holder; destroying it with a referenced slot logs one leak line per slot (GUID, logical path, count), and destroying it with a load in flight is a programmer error caught by `ZENITH_CHECK`.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_DestroyReportsReferencedSlots` in the `Test` step; `Test_AssetRegistry_DestroyWithLoadInFlightChecks` as a death check in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Construction and shutdown"; Zenith2/Docs/Design/ASSET/Loading.md → "Shutdown sequence"

### Z2-R-ASSET-013 Catalogue load and validation

- **Statement:** `Zenith_AssetCatalogue` loads `content:/assets.zcat`, validates it against every catalogue rule (sorted entries, unique GUIDs and logical paths, valid object paths, in-range and acyclic hard dependencies, the kind table, the fallback table, header check, the limits of Formats.md "Catalogue limits") and fails with `CATALOGUE_INVALID` instead of asserting.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-004, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCatalogue_LoadAndFind`, `Test_AssetCatalogue_EachRuleRejects` and `Test_AssetCatalogue_LimitsAtAndAboveLimit` in the `Test` step; `Zenith_Fuzz_Catalogue` (`win-asan` smoke in `Test`, full duration in stage acceptance); the Parser limits rows are core-doc item X111 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetCatalogue header"; Zenith2/Docs/Design/ASSET/Formats.md → "Asset catalogue file"; Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue limits"

### Z2-R-ASSET-014 Catalogue fragments and mount changes

- **Statement:** Catalogue fragments under `content:/Catalogues/` merge by mount precedence (a higher-precedence entry replaces a lower one; two same-precedence fragments disagreeing are `CATALOGUE_INVALID`), and `OnMountsChanged` publishes a new snapshot that subsequent requests use while in-flight loads keep theirs.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCatalogue_MergePrecedence` and `Test_AssetLoader_NotInstalledRetriesAfterMount` in the `Test` step; the rows of events E13 to E15 of Loading.md "Asset state machine", asserted by `Test_AssetStateMachine_EveryTransition` from Stage 4 (the S2 run of Z2-R-ASSET-153 asserts the other events).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "GUID index and catalogue snapshots"; Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue fragments"; Zenith2/Docs/Design/ASSET/Loading.md → "Retry after a mount change"

### Z2-R-ASSET-015 Acquisition by GUID and logical path

- **Statement:** `Acquire<T>` by GUID and `AcquireByLogicalPath<T>` by `content:/<path>` or `content:/<path>#<subkey>` return a counted handle of the requested kind, `NOT_FOUND` for an absent entry and `KIND_MISMATCH` for another kind, without starting a load.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_AcquireByLogicalPath` and `Test_AssetRegistry_AcquireKindMismatch` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET.md → "Content root mapping"

### Z2-R-ASSET-016 Unresolvable references give null handles

- **Statement:** `AcquireForDeserialization` of a GUID absent from the merged catalogue returns a null handle, counts `asset.missing` every time and logs one `asset.missing` record per GUID while its bounded set of 1,024 GUIDs has room (then at most one record per frame), so a document referencing a missing asset loads with a null reference instead of failing and the diagnostic never allocates.
- **Covers:** Z2-I-ASSET-12, Z2-I-ASSET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_UnknownGuidGivesNullAndDiagnostic` and `Test_AssetRegistry_MissingLogBounded` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET/Loading.md → "Failure classification"

### Z2-R-ASSET-017 Kind loader registration

- **Statement:** Kind loaders register through `RegisterKindLoader` before `CloseRegistration`; a duplicate kind, a test kind without the test flag or not declared in the registry desc, a registration after closing, a loader format version differing from a non-zero element of the catalogue's kind table, or a SUBSTITUTE kind with no role-0 fallback row is refused with its error, and a kind's owner calls `UnregisterKindLoader` explicitly, before destroying its loader, which unloads every asset of the kind first.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_RegisterKindLoaderRules` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET.md → "Construction and shutdown"

### Z2-R-ASSET-018 Common cooked file header

- **Statement:** Every cooked asset file begins with the 64-byte common header followed by a table of 40-byte section entries each embedding COMPRESS's 24-byte section descriptor, with section data 16-byte aligned and zero-padded, exactly as Formats.md lays out byte for byte.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CookedFile_WriterRoundTrip` and `Test_CookedFile_SectionAlignment` in the `Test` step; the golden files of Tests.md "Fixtures".
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"; Zenith2/Docs/Design/ASSET/Formats.md → "Section table"; Zenith2/Docs/Design/ASSET/Formats.md → "Section placement and alignment"

### Z2-R-ASSET-019 Cooked header and table validation

- **Statement:** `Zenith_ParseCookedHeader` applies checks C1 to C12 in order against the catalogue's expectation and the limits, returning the first failing check's code and detail and never asserting on any input bytes.
- **Covers:** Z2-I-ASSET-13, Z2-I-ASSET-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-004, Z2-D-045, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CookedFile_EachCheckRejects` and `Test_CookedFile_LimitsAtAndAboveLimit` in the `Test` step; `Zenith_Fuzz_CookedHeader` under `win-asan`.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Checks"; Zenith2/Docs/Design/ASSET/Formats.md → "Limits"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Parsing and validation functions"

### Z2-R-ASSET-020 Codec selected from the data

- **Statement:** The codec of every section is read from its descriptor in the file and never from configuration, so a file decodes identically whatever codec the current build defaults to.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_CookedFile_WriterRoundTrip` (every codec path) and `Test_AssetFailure_DecodeCorruptionFails` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Section decode"; Zenith2/Docs/Design/ASSET/Formats.md → "Section table"

### Z2-R-ASSET-021 Deterministic cooked file writer

- **Statement:** `Zenith_CookedFileWriter` produces byte-identical output for the same content regardless of the order sections are added and across runs, with the header check, section checks and content hash computed from the bytes written.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-004, D-SEL-HASH (open, Stage 1), Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CookedFile_WriterDeterministic` and `Test_CookedFile_Example_Runs` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_CookedFileWriter"

### Z2-R-ASSET-022 Cooked version refusal

- **Statement:** A cooked file whose format version or engine cook version differs from the running build's is refused with `VERSION_MISMATCH`, and a base catalogue whose engine cook version or kind count differs fails `Create` with `COOK_IDENTITY_MISMATCH` (exit code 3); nothing is migrated at run time and the remedy is a re-cook.
- **Covers:** Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookedFile_VersionMismatchRefused` and `Test_AssetRegistry_CatalogueCookIdentityMismatchFailsCreate` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Checks"; Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue header"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Parsing and validation functions"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetCatalogue header"; Zenith2/Docs/Design/ASSET/Loading.md → "Failure classification"

### Z2-R-ASSET-023 Load requests return operation handles

- **Statement:** `Zenith_AssetLoader::RequestLoad` returns a `Zenith_OperationHandle` per request (or a refusal when the operation table or the named completion queue is full), and each request holds a reference to its asset until its operation completes.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_LoadActivates` and `Test_AssetLoader_CompletionQueueRecords` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoader"

### Z2-R-ASSET-024 One pipeline per asset

- **Statement:** All requests for one asset join one load pipeline; the pipeline stops only when its last live request is cancelled, and each duplicate request is counted in `asset.loads.deduplicated`.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_RequestsDeduplicate` and `Test_AssetCancel_OneOfSeveralRequests` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Request deduplication and priorities"

### Z2-R-ASSET-025 Pipeline stage order

- **Statement:** A load passes the QUEUED sub-stages CATALOGUE, DEPENDENCIES, WAIT_LOADER and ADMISSION, then the admitted stages HEADER_READ, HEADER_VALIDATE, BEGIN, SECTIONS, DELIVER and ACTIVATING in that order (SECTIONS and DELIVER repeating per round, at most eight rounds), and the asset state follows the state machine table.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_LoadActivates` and `Test_AssetLoader_SectionRounds` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Load record stages"; Zenith2/Docs/Design/ASSET/Loading.md → "Pipeline stages"; Zenith2/Docs/Design/ASSET/Loading.md → "Catalogue lookup"

### Z2-R-ASSET-026 Hard dependencies load first

- **Statement:** Every hard dependency of an asset is ACTIVE (or FAILED with policy SUBSTITUTE and its kind's role-0 pinned fallback ACTIVE) before the dependent is admitted and before its `BeginLoad`; a SUBSTITUTE dependency that fails while that fallback is still loading keeps the dependent waiting until the fallback is terminal, so the outcome never depends on which finishes first; the dependent receives the dependency handles in catalogue order and keeps them for its ACTIVE life; a failed non-substitutable dependency, or one whose fallback is absent or failed, fails the dependent with `DEPENDENCY_FAILED`; a dependent failed with `DEPENDENCY_FAILED` carries the root cause, the first non-`DEPENDENCY_FAILED` code of the chain, as its detail.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_HardDependenciesFirst` and `Test_AssetLoader_DependencyFallbackAllowsParent` (including the fallback-still-loading case over 100 seeded orderings) in the `Test` step. `Test_AssetLoader_DependencyFailedRootCause` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Dependency loading"

### Z2-R-ASSET-027 Load priorities

- **Statement:** A pipeline's priority is the most urgent of its live requests and its dependents', recomputed on every request, `SetPriority` and cancellation, and applied to reads, decode picks and admission; equal priorities start in request order.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_PriorityOrder` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Request deduplication and priorities"

### Z2-R-ASSET-028 Cancellation during I/O

- **Statement:** Cancelling a load during a read cancels every in-flight read and releases the section's staging, decode unit and destination only after the read reports complete, so no buffer is freed under an I/O thread.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_DuringIO` in the `Test` step and under `Sanitizers -Preset win-asan` (guard-page destination).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Cancellation points by stage"

### Z2-R-ASSET-029 Cancellation during decompression

- **Statement:** Cancelling a load during decompression stops its decode slice at the next slice boundary or streamed-decode step, destroys the decompression context and releases staging and the unit.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_DuringDecode` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Cancellation points by stage"

### Z2-R-ASSET-030 Cancellation while the loader builds the object

- **Statement:** Cancelling a load during delivery or activation — while the kind loader builds the object and requests its uploads — calls the loader's `Cancel`, which withdraws its upload requests, releases every partial resource it created through deferred deletion and calls `AcknowledgeCancel`; the operation does not complete before the acknowledgement, and uploads running after `Activate` belong to the object and end with its `Unload`.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_DuringUpload` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Cancellation points by stage"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"

### Z2-R-ASSET-031 Cancellation during activation

- **Statement:** A cancellation that races `Activate` completes `SUCCEEDED`: activation is one atomic publication, and the requester releases its handle to unload.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_AfterActivationSucceeds` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Cancellation points by stage"

### Z2-R-ASSET-032 Completion reported after release

- **Statement:** A cancelled pipeline's requests complete `CANCELLED` only after every read has reported, every decode slice has ended and the loader has acknowledged; completion records are pushed before the operation status is set; `SUCCEEDED` is reported only after the object is published.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_WhileQueued`, `Test_AssetCancel_DuringDependencies`, `Test_AssetCancel_HandleDestructionCancels` and `Test_AssetLoader_CompletionQueueRecords` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Completion reporting"

### Z2-R-ASSET-033 Ownership of partial results

- **Statement:** Each partial result of a load has the owner the ownership table names, and after a cancelled or failed load nothing it held remains: no slot reference, staging, unit, buffer, operation, completion slot or loader object.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_Stress1000Cycles` (leak accounting per stage) in the `Test` step and under `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Ownership of partial results"

### Z2-R-ASSET-034 Load cancellation stress

- **Statement:** 1,000 issue-then-cancel cycles at randomised stages with a printed seed leave the slot count, staging, decode units, completion records, operations, fake-loader objects and heap exactly at their baseline; other areas park a load at DECODING, DELIVER or ACTIVATING through the header-only `Zenith_AssetStageHold` and see which stage each cancellation hit through `asset.loads.cancelled.<stage>`.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_Stress1000Cycles` (`z2-stress`) in the `Test` step and under `Sanitizers -Preset linux-tsan`. `Test_AssetStageHold_ParksAndCancels`; `Test_AssetLoader_CancelledCountersByStage` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Cancellation and stress tests"

### Z2-R-ASSET-035 Streaming cancellation stress with dependencies

- **Statement:** 1,000 cancel and reload cycles over assets with hard dependencies, dictionaries, streamed sections and an asynchronous upload stage leave no reference, allocation or object behind.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-16, Z2-I-ASSET-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCancel_StreamingStress1000Cycles` (`z2-stress`) in the `Test` step; the Stage 4 acceptance evidence "streaming cancellation".
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Cancellation and stress tests"

### Z2-R-ASSET-036 Completion queues for requesters

- **Statement:** `Zenith_AssetLoadCompletionQueue` is a bounded queue owned by a requester that receives one record per request naming it, carrying the request's user tag, and a request that could overflow it is refused at `RequestLoad`.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_CompletionQueueRecords` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoadCompletionQueue"

### Z2-R-ASSET-037 Simulation sees loads only through tick-boundary publication

- **Statement:** Simulation never reads live asset state: a requester whose result affects simulation drains its completion queue and publishes results at the next tick boundary in completion-ID order, replaying recorded publications at their recorded ticks.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetResidency_PressureChangesNoOutcome` in the `Test` step; the requesters' own replay tests (SCRIPT, STREAM, SCENE).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Publication to simulation"

### Z2-R-ASSET-038 Service in background slices

- **Statement:** Pipeline state is advanced only by one `bg/Asset.Service` background operation at a time, in slices of at most 2 ms kicked by events and re-submitted as continuations; no ZenithAssets code blocks a job.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_ServiceSlicesBounded` in the `Test` step on `win-release`.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Service scheduling"

### Z2-R-ASSET-039 Assets.Service frame node

- **Statement:** The `Assets.Service` epilogue node calls `ServiceFrame` once per frame within `asset.service.frameBudgetUs`: it advances the registry frame, drains I/O completions, publishes counters and kicks the service; it reads and writes no world.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_LoadActivates` driven through `ServiceFrame` in the `Test` step; the node's registration checked by the CPU frame graph's node tests; the node's row is core-doc item X110 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "The Assets.Service node"; Zenith2/Docs/Design/ASSET.md → "Frame-graph placement"

### Z2-R-ASSET-040 Tool and boot servicing

- **Statement:** `ServiceUntilComplete` runs service slices on a non-worker thread until the operation completes or the timeout passes, returning `TIMEOUT` with the operation still running when it does and `INVALID_REQUEST` on a job worker; the composition root uses it for the pinned loads after `CloseRegistration` and for the startup scene, and tools and tests use it with no frame loop.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_ServiceUntilCompleteOffWorker` and `Test_AssetLoader_BootWithoutRendering` in the `Test` step — the composition-root calls are core-doc item X110 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Tool and boot servicing"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoader"

### Z2-R-ASSET-041 Headless loading

- **Statement:** In headless runs ZenithAssets serves every GPU-resident kind with a validate-only loader that activates with a null object after the header parse, so requests, deduplication, cancellation, failures and publication behave as in a windowed run.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_HeadlessGpuKindsValidateOnly` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Headless loading"

### Z2-R-ASSET-042 Range and section reads

- **Statement:** `Zenith_AssetReader::ReadRange` returns raw stored bytes and `ReadSection` returns decoded bytes through the decode queue, both as operations, with bounds errors for out-of-range requests.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetReader_RangeAndSection` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"

### Z2-R-ASSET-043 Verified reads of cooked files

- **Statement:** `ReadVerifiedFile` reads a catalogue file entry whole into a destination of exactly `GetVerifiedFileSize` bytes and verifies its hash, failing `CORRUPT` with `FILE_HASH_MISMATCH` on any mismatch, so consumers such as SCRIPT's bytecode, the schema blob and merged configuration never use unverified bytes from Stage 2 on; with `Zenith_AssetReader::ServiceUntilComplete` it is the one call a library's `Create`-time engine-content read uses, over `content:/` paths only, a host-directory cooked fixture being served as a loose mount that PLAT refuses in Shipping.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetReader_VerifiedFileHashMismatch` in the `Test` step. `Test_AssetReader_EngineContentReadBeforeFrames` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"; Zenith2/Docs/Design/ASSET/Formats.md → "File entries"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Engine content reads during Create"

### Z2-R-ASSET-044 Bounded decode queue

- **Statement:** The decode queue is a set of at most D = `asset.decode.queueDepth` reserved units, each held by a header read or section from read issue until its decode completes, fails or is cancelled; a refused reservation behaves as `QUEUE_FULL_POLICY_REJECT` (the record waits and retries), so the producer never issues beyond D and no job waits for a unit.
- **Covers:** Z2-I-ASSET-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_BurstNeverExceedsDepth` in the `Test` step and under `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Definitions"; Zenith2/Docs/Design/ASSET/Loading.md → "Decode concurrency and slices"; Zenith2/Docs/Design/ASSET/Loading.md → "Internal queues"

### Z2-R-ASSET-045 I/O throttled by decode capacity

- **Statement:** A section or header read is issued only when a decode unit is reserved within its priority's limit, so reads in flight plus undecoded plus decoding sections never exceed D whatever the request burst.
- **Covers:** Z2-I-ASSET-17, Z2-I-ASSET-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_IOWaitsForDecodeCapacity` and `Test_AssetBackPressure_BurstNeverExceedsDepth` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Issue rule"

### Z2-R-ASSET-046 Staging byte budget

- **Statement:** Reserved staging never exceeds S = `asset.io.stagingBytes`: a read needing its stored bytes (one-shot) or two chunks (streamed) of staging is not issued until they fit, and a record waiting more than eight slices for staging blocks subsequent records of its priority and below from taking staging.
- **Covers:** Z2-I-ASSET-17, Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetBackPressure_BurstNeverExceedsDepth` (staging sampled per event) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Issue rule"

### Z2-R-ASSET-047 Decode priority reserves

- **Statement:** BACKGROUND loads hold at most ⌊D/2⌋ units and ⌊S/2⌋ staging bytes, NORMAL and LOW at most D − ⌈D/4⌉ and S − ⌈S/4⌉, CRITICAL and HIGH up to D and S, so a CRITICAL read waits at most for decodes already running.
- **Covers:** Z2-I-ASSET-17, Z2-I-ASSET-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_PriorityReserves` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Priority reserves"

### Z2-R-ASSET-048 Streamed decode of large sections

- **Statement:** A section whose stored size exceeds `asset.io.streamThresholdBytes`, or whose decoded size exceeds `asset.decode.sliceBytes`, is decoded streamed with at most two chunks of staging and one read ahead, and its result equals a one-shot decode.
- **Covers:** Z2-I-ASSET-17, Z2-I-ASSET-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetBackPressure_StreamedLargeSection` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Streamed decode of large sections"

### Z2-R-ASSET-049 Decode slices and concurrency

- **Statement:** At most C = `asset.decode.concurrency` decode slices run at once, picked by (priority, request sequence), each producing at most `asset.decode.sliceBytes` of output; a slice over 2 ms counts `asset.decode.slice_overruns`.
- **Covers:** Z2-I-ASSET-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** Z2-B-COMPRESS-001, Z2-B-COMPRESS-002
- **Acceptance evidence:** `Test_AssetBackPressure_DecodeConcurrencyCap` and `Test_AssetLoader_ServiceSlicesBounded` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Decode concurrency and slices"

### Z2-R-ASSET-050 Back-pressure counters and stall breach

- **Statement:** ZenithAssets publishes the back-pressure counters (queue depth, high water, full, staging bytes and high water, throttled, queue_full, slices, slice overruns); a refused issue is the BR-STALL breach consequence (it waits, counts and is visible), and `IO_ERROR_QUEUE_FULL` returns the record to waiting.
- **Covers:** Z2-I-ASSET-16, Z2-I-ASSET-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_CountersPublished` and `Test_AssetBackPressure_IOQueueFullRetries` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Back-pressure counters"; Zenith2/Docs/Design/ASSET.md → "Counters"

### Z2-R-ASSET-051 Back-pressure burst test

- **Statement:** A burst of 5,000 load requests completes with reserved units, staging and reads in flight, sampled at every I/O and decode event, never above D, S and D respectively.
- **Covers:** Z2-I-ASSET-16, Z2-I-ASSET-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_BurstNeverExceedsDepth` in the `Test` step and under `Sanitizers -Preset linux-tsan`.
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Back-pressure tests"

### Z2-R-ASSET-052 Resident byte accounting

- **Statement:** Every kind loader reports each object's resident host and device bytes; ZenithAssets sums them per memory subsystem, and its staging, decode buffers and scratch are allocated once at `Create` so the load path allocates only pooled records.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-012
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetResidency_AccountingPerSubsystem` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Resident byte accounting"

### Z2-R-ASSET-053 Asset memory consumer

- **Statement:** The registry's memory consumer reports `MEMORY_SUBSYSTEM_ASSETS` as ZenithAssets' own bytes plus the host bytes of ASSETS-subsystem kinds only, never bytes another subsystem's consumer reports.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetResidency_AccountingPerSubsystem` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Memory pressure responses"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"

### Z2-R-ASSET-054 Keep-alive cache

- **Statement:** Zero-reference assets of cacheable kinds enter a keep-alive cache bounded by `asset.cache.maxBytes`, evicted least recently released first; acquiring a cached asset revives it with no I/O; non-cacheable kinds unload at zero references.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetResidency_KeepAliveCacheLRU` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "The keep-alive cache"

### Z2-R-ASSET-055 Shedding at warning

- **Statement:** At memory level WARNING the keep-alive cache is flushed and admission to it is suspended, so zero-reference assets unload at once.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetResidency_WarningFlushesCache` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Memory pressure responses"

### Z2-R-ASSET-056 Shedding at critical

- **Statement:** At memory level CRITICAL ZenithAssets additionally calls `DropCpuCopies` once on every registered loader of a GPU-resident kind, which releases the CPU copies of RESIDENT objects only, and counts the bytes dropped.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetResidency_CriticalDropsCpuCopies` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Memory pressure responses"

### Z2-R-ASSET-057 Shedding timing and revert

- **Statement:** A shedding response is applied within one frame of the level change and reverted one frame after the level falls, and no admission, priority or decode decision depends on the level, so the same request sequence publishes identical results under NORMAL and CRITICAL.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetResidency_WarningFlushesCache` and `Test_AssetResidency_PressureChangesNoOutcome` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Memory pressure responses"

### Z2-R-ASSET-058 In-flight load budget

- **Statement:** A pipeline waiting for admission — a load, a development reload or a device recovery — is admitted only while loads in flight (RETIRING reload records included) are below `asset.load.maxInFlight`, in-flight file bytes stay within `asset.load.maxInFlightBytes` and, for a kind whose loader desc sets `m_uMaxConcurrentLoads`, fewer of that kind's pipelines hold a record, with the priority reserves applied; a pipeline of a kind at its limit is skipped without losing its FIFO position, so it never holds back another kind; a single larger file is admitted alone; pipelines waiting for dependencies or a loader count in neither limit.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** the in-flight count, byte-limit, single-oversized-file and per-kind-limit cases of `Test_AssetResidency_InFlightBudget` in the `Test` step from Stage 4 (its dependency-chain case is Z2-R-ASSET-172's at Stage 2).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "In-flight load budget"; Zenith2/Docs/Design/ASSET/Loading.md → "Reload and recovery pipelines"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoaderDesc"

### Z2-R-ASSET-059 Reload double-residency reservation

- **Statement:** A development reload reserves the streaming double-residency transition peak for the entry's resident estimate before it starts and releases it after the old object unloads; a refused reservation defers the reload to a following frame and counts it.
- **Covers:** Z2-I-ASSET-15, Z2-I-ASSET-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-006
- **Acceptance evidence:** `Test_AssetResidency_ReloadReservesPeak` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"

### Z2-R-ASSET-060 Unload and reload stress

- **Statement:** 1,000 cycles of acquire, load, release and reacquire over 200 assets under random cache pressure leave no slot, byte, record or allocation behind.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Test_AssetResidency_UnloadReloadStress` (`z2-stress`) in the `Test` step; the Stage 4 acceptance evidence "unload/reload stress".
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Unload and reload"; Zenith2/Docs/Design/ASSET/Tests.md → "Residency tests"

### Z2-R-ASSET-061 Development reload

- **Statement:** `RequestReload` in development builds loads a new object in a second pipeline, swaps it in at activation, keeps the old object alive for `asset.reload.retireFrames` frames and then unloads it; a failed reload keeps the old object ACTIVE; Shipping returns `NOT_ALLOWED`.
- **Covers:** Z2-I-ASSET-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_ResolvePointerValidUntilFrameBoundary` and `Test_Integration_ReimportThenLoad` in the `Test` step; the rows of event E11 of Loading.md "Asset state machine" and the reload rows of "Reload and recovery pipelines", asserted by `Test_AssetStateMachine_EveryTransition` and `Test_AssetStateMachine_ReloadAndRecoveryPipelines` from Stage 4.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"

### Z2-R-ASSET-062 Device-loss residency reload

- **Statement:** `ReloadGpuResidencySet` queues a device-recovery pipeline for every ACTIVE GPU-resident asset without a second pipeline (pinned first at CRITICAL, others at HIGH, in (kind, GUID) order) with residency LOST, admits them within the in-flight limits, recreates GPU state in place while the slot stays ACTIVE, evicts CACHED ones, restarts in-flight loads whose loader has been called with a new attempt, unloads instead of caching an object whose last reference drops while its recovery is queued, and completes when every counted slot has ended (activated, failed, cancelled, or replaced by a reload's activation), `CANCELLED` under shutdown; a second call while one is incomplete keeps queued recoveries, restarts running ones and completes the earlier operation `CANCELLED`.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` (with more GPU assets than `asset.load.maxInFlight`, a second device loss, a cancelled recovery and a recovery whose last reference drops while queued) in the `Test` step; the rows of event E12 of Loading.md "Asset state machine", asserted by `Test_AssetStateMachine_EveryTransition` from Stage 3.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Device-loss reload"; Zenith2/Docs/Design/ASSET/Loading.md → "Reload and recovery pipelines"; Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"; Zenith2/Docs/Design/ASSET/Loading.md → "In-flight load budget"

### Z2-R-ASSET-063 Missing assets

- **Statement:** An absent catalogue entry is `NOT_FOUND`, a listed entry whose file is absent is `NOT_INSTALLED`, and an unreadable file is `IO` with the I/O code; each failure logs one `asset.load` record and counts `asset.failures.<code>`.
- **Covers:** Z2-I-ASSET-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_NotInstalledRetriesAfterMount`, `Test_AssetFailure_IOErrorFails` and `Test_AssetFailure_LoggedOncePerClass` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Failure classification"; Zenith2/Docs/Design/ASSET.md → "Missing and corrupt assets"

### Z2-R-ASSET-064 Undelivered content retried after mounts change

- **Statement:** After a published catalogue swap every FAILED slot whose failure is `NOT_INSTALLED` or `NOT_FOUND`, or whose entry changed, returns to UNLOADED so the next request loads it; failures of other classes stay until `RequestReload` or restart.
- **Covers:** Z2-I-ASSET-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_NotInstalledRetriesAfterMount` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Retry after a mount change"

### Z2-R-ASSET-065 Corrupt assets fail without asserting

- **Statement:** Every corruption class of `Zenith_AssetCorruption` detected in parse, verify or decode fails the load with `CORRUPT` and its detail, and no byte of any cooked file or catalogue reaches an assert.
- **Covers:** Z2-I-ASSET-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetFailure_EachCorruptionClassFails` and `Test_AssetFailure_DecodeCorruptionFails` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Failure classification"; Zenith2/Docs/Design/ASSET/Formats.md → "Checks"

### Z2-R-ASSET-066 Fallback per kind

- **Statement:** `ResolveOrFallback<T>` returns the object when ACTIVE or CACHED with a GPU residency other than FAILED, and otherwise the kind's role-0 pinned fallback object for SUBSTITUTE kinds and null for NULL_OBJECT and FAIL kinds, per the kind's fallback policy column (no loader overrides it), counting substitutions.
- **Covers:** Z2-I-ASSET-12, Z2-I-ASSET-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetFailure_FallbackPerPolicy` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Fallback policy per kind"; Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"; Zenith2/Docs/Design/ASSET/Loading.md → "The keep-alive cache"; Zenith2/Docs/Design/ASSET.md → "Asset kinds"

### Z2-R-ASSET-067 Engine fallbacks pinned at startup

- **Statement:** Every fallback-table row of a kind is pinned and requested at CRITICAL when the kind's loader registers; after `CloseRegistration` the composition root services `GetPinnedLoads` and every pinned asset is ACTIVE before startup step 17; a pinned failure, a timeout or a SUBSTITUTE kind with no role-0 row stops startup with exit code 3 naming the kind, role and error.
- **Covers:** Z2-I-ASSET-12, Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetFailure_FallbackPinnedAtRegistration`, `Test_AssetRegistry_PinnedFallbacksFromCatalogue` and `Test_AssetLoader_BootWithoutRendering` in the `Test` step; `Test_Integration_MinimalGameBothPlatforms` (the Minimal game boots with its engine fallbacks pinned) — the composition-root calls are core-doc item X110 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Fallback policy per kind"; Zenith2/Docs/Design/ASSET/Loading.md → "Tool and boot servicing"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"

### Z2-R-ASSET-068 Parser fuzz targets

- **Statement:** Fuzz targets exist for the cooked header, the catalogue, section decode and the load result payload, and run clean of crashes and sanitizer findings from their seed corpora.
- **Covers:** Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_CookedHeader`, `Zenith_Fuzz_Catalogue`, `Zenith_Fuzz_SectionDecode`, `Zenith_Fuzz_LoadResult`: 60-second `win-asan` smoke in the `Test` step, full duration in stage acceptance.
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Fuzz targets"

### Z2-R-ASSET-069 Missing and corrupt content fixture set

- **Statement:** A committed corrupt-content fixture set holds one file per `Zenith_AssetCorruption` value, one per check C1 to C12 and version-mismatch files, each with its expected error, and the integration suite loads it through the whole runtime with every failure classified and every fallback applied.
- **Covers:** Z2-I-ASSET-12, Z2-I-ASSET-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Integration_MissingAndCorruptContent` over `Zenith2/Engine/ZenithAssets/Tests/Fixtures/Corrupt/` in the `Test` step; the Stage 3 acceptance evidence "missing or corrupt content".
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Missing and corrupt content tests"; Zenith2/Docs/Design/ASSET/Tests.md → "Fixtures"

### Z2-R-ASSET-070 Meta sidecar per asset source

- **Statement:** Every asset source (a file the scan classifies as a source) has a tracked `.meta` sidecar next to it in the `zenith2.asset-meta` v1 schema holding its GUID, importer ID, settings and sub-asset table.
- **Covers:** Z2-I-ASSET-01, Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_ScanClassifiesFiles` and `Test_AssetMeta_EachValidationMessage` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "What is an asset source"; Zenith2/Docs/Design/ASSET/Database.md → "The meta sidecar"

### Z2-R-ASSET-071 GUID assignment

- **Statement:** GUIDs are assigned only by `--assign-guids`, by the editor through the cook server, or in a ticket's diff, as fresh version-4 GUIDs written through the journaled write into canonical `.meta` files.
- **Covers:** Z2-I-ASSET-05, Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_AssignGuidsWritesMeta` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "GUIDs"

### Z2-R-ASSET-072 GUIDs never reused

- **Statement:** A GUID present in the database or in any content root's tracked `.zenith-tombstones` list is never assigned again, and deleting a source with its `.meta` records its GUID in the list of its content root.
- **Covers:** Z2-I-ASSET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_GuidsNeverReused` and `Test_AssetDatabase_TombstoneLists` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "GUIDs"; Zenith2/Docs/Design/ASSET/Database.md → "Orphaned meta files and tombstones"; Zenith2/Docs/Design/ASSET/Formats.md → "Tombstone list"

### Z2-R-ASSET-073 Duplicate GUIDs rejected

- **Statement:** Two `.meta` documents carrying one GUID fail the scan naming both paths; `--fix-duplicate-guids` keeps the GUID of the recorded path and assigns a fresh one to the other.
- **Covers:** Z2-I-ASSET-05, Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_DuplicateGuidRejected` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Duplicate GUIDs"

### Z2-R-ASSET-074 Rename and move keep GUIDs

- **Statement:** Moving or renaming a source together with its `.meta` keeps its GUID, changes only its logical path and marks nothing dirty; a source moved without its `.meta` produces a probable-move warning naming both paths.
- **Covers:** Z2-I-ASSET-05, Z2-I-ASSET-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_MoveKeepsGuid` and `Test_AssetDatabase_ProbableMoveWarned` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Rename and move detection"

### Z2-R-ASSET-075 Sub-asset identity

- **Statement:** Each sub-asset an importer produces is identified by a stable importer key and gets its own GUID recorded in the source's `.meta` sub-asset table, addressable as `content:/<path>#<key>`; every member of a sub-asset entry (CONTENT's and ANIM's owner members and ASSET's `fingerprint`) follows its GUID, the owner members are import-key inputs keyed by GUID, an importer reads the previous entries and records fingerprints, and a derived product carries an importer-computed GUID with no `.meta` entry.
- **Covers:** Z2-I-ASSET-05, Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_ReimportKeepsSubAssetGuids` and `Test_AssetRegistry_AcquireByLogicalPath` in the `Test` step. `Test_Import_ProductMembersAndFingerprints`; `Test_Import_DerivedProducts` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"; Zenith2/Docs/Design/ASSET/Import.md → "Product members and fingerprints"; Zenith2/Docs/Design/ASSET/Import.md → "Derived products"

### Z2-R-ASSET-076 Asset database file

- **Statement:** The asset database persists in `assetdb.zdb` under the output tree as a cache of the scan, including each source's stored import diagnostics in the `ZIDG` blob of Formats.md "Import diagnostics blob"; a missing, damaged or other-version file is discarded and rebuilt with identical records, and file modification times are only a hint for re-hashing.
- **Covers:** Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_CorruptFileRebuilds` and `Test_AssetDatabase_StatCookieOnlyHint` in the `Test` step; `Zenith_Fuzz_AssetDatabase` under `win-asan`. `Test_AssetDatabase_StoresImportDiagnostics` and `Test_ImportDiagnosticsBlob_RoundTripAndChecks` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Database file location and rebuild"; Zenith2/Docs/Design/ASSET/Formats.md → "Asset database file"; Zenith2/Docs/Design/ASSET/Formats.md → "Import diagnostics blob"

### Z2-R-ASSET-077 Scan and reconcile

- **Statement:** A scan classifies every file under the content roots, reads every `.meta`, reconciles records (new, changed, moved, orphaned, removed), rejects non-NFC names and logical-path collisions, and yields the same records for the same tree; input-only documents (generator, light bake and terrain tile documents, a world's region scenes) and files a previous import recorded as inputs are classified without a `.meta` and without a warning.
- **Covers:** Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_ScanClassifiesFiles`, `Test_AssetDatabase_NonNfcNameRejected` and `Test_AssetDatabase_LogicalPathCollision` in the `Test` step. `Test_AssetDatabase_InputOnlyDocumentsClassified` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Scan algorithm"

### Z2-R-ASSET-078 Dependency graph

- **Statement:** The database keeps a dependency graph of typed edges between assets, maintained on import and cook, answering dependency and dependent queries and giving a stable topological order with ties broken by GUID and cycles reported.
- **Covers:** Z2-I-ASSET-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDependencyGraph_TopologicalOrderStable` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Dependency graph"

### Z2-R-ASSET-079 Build cooks never write sources

- **Statement:** A build cook never writes into the source tree: a source without a `.meta` fails the cook with `META_MISSING`, and the source tree's bytes are unchanged after any cook.
- **Covers:** Z2-I-ASSET-01, Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_MissingMetaFailsBuildCook` (tree hash before and after) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Reading and writing meta documents"; Zenith2/Docs/Design/ASSET/Cook.md → "Options"

### Z2-R-ASSET-080 Content names and root mapping

- **Statement:** Source paths map to logical paths `content:/<path>` by the content root mapping rules, with engine and module content under their prefixes and no content from outside the package, patch, loose and user mounts.
- **Covers:** Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-075, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_OutputTreeLayout`, `Test_AssetDatabase_LogicalPathCollision` and `Test_Cook_ContentSelectionFromComposition` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Content root mapping"; Zenith2/Docs/Design/ASSET/Cook.md → "Content root mapping rules"

### Z2-R-ASSET-081 Meta documents migrate

- **Statement:** `.meta` documents are the authoring-document type `Zenith.AssetMeta` of SCENE's framework and migrate through its registered, gap-free chain; every `v<N>.json` fixture migrates to its `v<N>.expected.json`; top-level members with their own `schema` (CONTENT's `provenance`) migrate through a constexpr member-migration table, and the registration lives in `ZenithAuthoring` with no ZenithCook type in its signature.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_MetaMigration_Fixtures` and `Test_AssetMeta_DocumentTypeRegistered` in the `Test` step. `Test_AssetMeta_MemberMigrationTable`; `Test_AuthoringAsset_MetaRegistrationWithoutCook` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"; Zenith2/Docs/Design/ASSET/Migration.md → "Registered document types"; Zenith2/Docs/Design/ASSET/Formats.md → "Meta migration chain"; Zenith2/Docs/Design/ASSET/Migration.md → "Member migrations"

### Z2-R-ASSET-082 Meta canonical form and unknown fields

- **Statement:** ZenithCook writes `.meta` documents only in canonical form (the framework's canonical writer with ASSET's rule table, sub-assets and labels sorted), round-trips a canonical document byte for byte, and preserves unknown and `x-` members in place with one warning each; the schema admits PHYS's top-level `physics` object and the owner members of sub-asset entries, and refuses any other entry member.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_CanonicalRoundTripIsIdentity`, `Test_AssetMeta_UnknownFieldsPreserved` and `Test_AssetMeta_NewerVersionReadOnly` in the `Test` step; `Zenith_Fuzz_MetaDocument` under `win-asan`. `Test_AssetMeta_SubAssetOwnerMembers` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Canonical meta form"; Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"; Zenith2/Docs/Design/ASSET/Formats.md → "Keys"

### Z2-R-ASSET-083 Importer interface and registry

- **Statement:** Importers implement `Zenith_Importer` with a `Zenith_ImporterDesc` (ID, version, extensions, settings schema) and register in the importer registry, which refuses duplicate IDs; a source is imported by the importer its `.meta` names; the import context reports diagnostics with an optional code that becomes the cook report entry's `code`, and opens a document relative to another asset's source (`OpenInputDocumentRelativeTo`) as a recorded import input migrated in memory; it opens its own source, documents relative to its directory and other assets' source documents migrated in memory as recorded inputs, and reads `thirdparty:` input files keyed by path text.
- **Covers:** Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_ReadInputRecorded`, `Test_Import_SettingValidation`, `Test_Import_DiagnosticCodesValidated`, `Test_Import_OpenInputDocumentRelativeTo`, `Test_CookTestContexts_CodedImportDiagnostics` and `Test_CookTestContexts_RelativeDocumentInputs` in the `Test` step. `Test_Import_InputDocuments`; `Test_Import_ThirdPartyInputs` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Import.md → "Importer interface"; Zenith2/Docs/Design/ASSET/Import.md → "Importer registry"; Zenith2/Docs/Design/ASSET/Import.md → "Diagnostic codes"; Zenith2/Docs/Design/ASSET/Import.md → "Documents relative to another asset"; Zenith2/Docs/Design/ASSET/Import.md → "Input documents"

### Z2-R-ASSET-084 Import settings resolution

- **Statement:** Import settings resolve key by key as schema defaults, then preset, then `.meta` settings, then the platform override; each value is validated against the schema (type, range, enumeration, compression settings), and the resolved settings hash is a key input.
- **Covers:** Z2-I-ASSET-06, Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_SettingsResolutionOrder`, `Test_Import_SettingValidation` and `Test_Import_CompressionSettingValidated` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Import.md → "Settings schema"; Zenith2/Docs/Design/ASSET/Import.md → "Settings resolution"; Zenith2/Docs/Design/ASSET/Import.md → "Compression setting"

### Z2-R-ASSET-085 Deterministic import

- **Statement:** An import is a pure function of its declared inputs: the same source bytes, settings and importer version give byte-identical intermediates whatever the worker count, and reading an input outside the content roots fails the import.
- **Covers:** Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_SameInputsSameIntermediateBytes` and `Test_Import_ReadInputRecorded` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Import.md → "Import algorithm"; Zenith2/Docs/Design/ASSET/Import.md → "Intermediates"

### Z2-R-ASSET-086 Reimport triggers

- **Statement:** A source is reimported exactly when one of the listed triggers occurs (source bytes, a declared input, settings, a preset, the importer version) and never otherwise; a preset of another importer, or one CONTENT's validator refuses, fails settings resolution, and a new `.meta`'s preset is chosen once, at creation, by CONTENT's preset set.
- **Covers:** Z2-I-ASSET-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_ReimportTriggers` in the `Test` step. `Test_Import_PresetImporterMismatch`; `Test_AssetDatabase_PresetChosenAtCreation` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Import.md → "Reimport triggers"

### Z2-R-ASSET-087 Reimport keeps identity

- **Statement:** A reimport keeps the GUID of every sub-asset whose importer key survives, orphans removed keys, assigns new GUIDs to new keys, and moves a GUID through `RenameProduct`.
- **Covers:** Z2-I-ASSET-04, Z2-I-ASSET-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_ReimportKeepsSubAssetGuids` and `Test_Integration_ReimportThenLoad` in the `Test` step; the Stage 3 acceptance evidence "reimport".
- **Design:** Zenith2/Docs/Design/ASSET/Import.md → "Identity preservation on reimport"

### Z2-R-ASSET-088 Editor reimport through the cook server

- **Statement:** The editor reimports only through the cook server's `reimport` request or its watch notifications, which return the changed GUIDs and whether the catalogue changed, so the editor refreshes the catalogue and the runtime reloads the changed assets.
- **Covers:** Z2-I-ASSET-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** Z2-B-EDIT-004, Z2-B-EDIT-010
- **Acceptance evidence:** `Test_CookServer_Protocol`, `Test_CookServer_WatchNotifies` and `Test_Integration_ReimportThenLoad` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Import.md → "Editor reimport"; Zenith2/Docs/Design/ASSET/Cook.md → "Requests"; Zenith2/Docs/Design/ASSET/Formats.md → "Notifications"

### Z2-R-ASSET-089 Importer isolation mechanism

- **Statement:** Importers can run in cook worker processes, so an importer crash fails only its item with `WORKER_CRASHED` and other items complete.
- **Covers:** Z2-I-ASSET-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_Import_WorkerCrashContained` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Worker processes"; Zenith2/Docs/Design/ASSET/Import.md → "Boundary with CONTENT importers"

### Z2-R-ASSET-090 Cook command line and exit codes

- **Statement:** `ZenithCook` accepts the documented options and exits with code 0 (success), 1 (some items failed), 2 (bad command line), 3 (uncookable game), 4 (low disk) or 5 (internal failure), always writing its report and writing the depfile only on 0 and 1; every mode accepts `--game-root <dir>` for a scratch games root, `--schema-lock` writes the schema lock under the game root's writer lock alone, and `--only @<file>` restricts a cook to listed sources and their dependencies.
- **Covers:** Z2-I-ASSET-03, Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ExitCodes` in the `Test` step. `Test_Cook_GameRootAndSchemaLock`; `Test_Cook_OnlyListRestrictsRun` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Options"; Zenith2/Docs/Design/ASSET/Cook.md → "Exit codes"

### Z2-R-ASSET-091 Cook step registry, phases and source documents

- **Statement:** Cook steps implement `Zenith_CookStep` with a `Zenith_CookStepDesc`, are registered by each owning area's `Zenith_Register<Area>CookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)` at cook services item 6 in its fixed order (SCENE's seventh), and run in the phases GLOBAL_PRE, ASSET and GLOBAL_POST in that order, with the catalogue step last; a step reads a source file only through `Zenith_CookContext::OpenSourceDocument`, which from any item thread, without waiting for the main thread, returns the document of a live snapshot record migrated in memory and byte-equal to what the plan's scan hashed (else `COOK_ERROR_SOURCE_CHANGED`), and refuses in `Cook` a document the item's `AddKeyInputs` did not open and key by its SHA-256; a step finds a live snapshot record by logical path through `Zenith_CookContext::FindAssetByLogicalPath`; global steps read the plan's origins, GLOBAL_POST steps read tracked repository files and every step reads `thirdparty:` input files, each read keyed by path and hash and listed in the depfile.
- **Covers:** Z2-I-ASSET-03, Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PhasesOrdered`, `Test_CookContext_OpenSourceDocument` and `Test_CookContext_FindAssetByLogicalPath` in the `Test` step; SCENE's `Test_SceneCookSteps_KeysAndHardReferences` exercises the member through its two steps. `Test_CookContext_RepositoryAndThirdPartyFiles` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Cook steps"; Zenith2/Docs/Design/ASSET/Cook.md → "Step phases"; Zenith2/Docs/Design/ASSET/Cook.md → "Step registration functions"; Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"; Zenith2/Docs/Design/ASSET/Cook.md → "Source documents in cook steps"; Zenith2/Docs/Design/ASSET/Cook.md → "Repository and third-party files"

### Z2-R-ASSET-092 Content selection from the composition

- **Statement:** The cook scans and cooks only the engine content and catalogue fragments of the modules the game's composition selects.
- **Covers:** Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ContentSelectionFromComposition` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Content selection"

### Z2-R-ASSET-093 Configuration fragment merge

- **Statement:** The cook merges the selected configuration fragments, refuses a key set by two fragments with `CONFIG_KEY_CONFLICT`, applies the game's file last, and writes the merged configuration in key order.
- **Covers:** Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ConfigFragmentMerge` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Configuration merge"

### Z2-R-ASSET-094 Catalogue and mount manifest generation

- **Statement:** The cook writes `assets.zcat` listing every cooked asset and file, and a mount manifest that validates with PLAT's manifest parser.
- **Covers:** Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_CatalogueAndManifest` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Catalogue generation"; Zenith2/Docs/Design/ASSET/Cook.md → "Mount manifest"

### Z2-R-ASSET-095 Cooked output tree and atomic publication

- **Statement:** Cooked objects are written to `content:/Objects/<xx>/<guid>.<ext>` through a temporary and an atomic rename, so no reader ever sees a partial file, and a failing step publishes nothing.
- **Covers:** Z2-I-ASSET-03, Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_OutputTreeLayout` and `Test_Cook_StepFailurePublishesNothing` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Cooked output tree"; Zenith2/Docs/Design/ASSET/Cook.md → "Atomic output publication"

### Z2-R-ASSET-096 Deterministic cook output

- **Statement:** The cooked tree is byte-identical for the same inputs whatever the worker count or scheduling.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_OutputIndependentOfWorkerCount` and `Test_Cook_ReportDeterministic` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Deterministic ordering"

### Z2-R-ASSET-097 Dependency file

- **Statement:** The cook writes a Ninja depfile listing every file it read once, sorted, so a change to any listed input reruns the cook.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_DepFileListsInputs` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Dependency file"; Zenith2/Docs/Design/ASSET/Formats.md → "Dependency file"

### Z2-R-ASSET-098 Incremental cook by key

- **Statement:** Each cook item is keyed by its encoded inputs, and an item whose key matches the output already in the tree is reused without running its step, so a no-change cook runs no step.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_IncrementalNoOpCooksNothing` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Keys and invalidation"; Zenith2/Docs/Design/ASSET/Cook.md → "Output reuse"

### Z2-R-ASSET-099 Dependency-driven invalidation

- **Statement:** A change to an asset re-cooks every dependent whose key includes it, and propagation stops at a dependency whose output bytes did not change; an import that reads another asset's published property through `ReadPublishedProperty` re-imports exactly when that property's bytes change, the producing item being cooked into the DDC first, and only into the DDC for a platform other than the cook's; an import that reads another source's product list re-imports exactly when that list changes.
- **Covers:** Z2-I-ASSET-09, Z2-I-ASSET-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_DependencyChangePropagates`, `Test_Import_PublishedPropertyInputs` and `Test_CookTestContexts_PublishedPropertyInputs` in the `Test` step. `Test_Import_ReadSourceProductsInputs` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Dependency-driven invalidation"; Zenith2/Docs/Design/ASSET/Import.md → "Published-property inputs"

### Z2-R-ASSET-100 Clean and incremental cook equivalence

- **Statement:** After any sequence of source mutations an incremental cook produces a tree byte-identical to a clean cook of the same sources.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-065, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_CleanEqualsIncremental` in the `Test` step; the Stage 3 acceptance evidence "clean versus incremental cook equivalence".
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Output reuse"; Zenith2/Docs/Design/ASSET/Tests.md → "Cook tests"

### Z2-R-ASSET-101 Cook report

- **Statement:** Every cook writes its report under `.cook/` in the schema of Formats.md "Cook report", listing items, reuse, failures and warnings, and two cooks of the same inputs write identical reports; every report lists, for every CURRENT source, the stored diagnostics of the import that produced its current intermediates, live when that import ran in the cook and marked `"replayed": true` when it did not; importers contribute report members merged by name and replayed like their diagnostics.
- **Covers:** Z2-I-ASSET-09, Z2-I-ASSET-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ReportDeterministic`, `Test_CookReport_ReplaysStoredDiagnostics` and `Test_CookReport_ReplayedEqualsLive` in the `Test` step. `Test_CookReport_ImportReportMembers` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Cook report"; Zenith2/Docs/Design/ASSET/Formats.md → "Cook report"; Zenith2/Docs/Design/ASSET/Import.md → "Replay into the cook report"

### Z2-R-ASSET-102 Structural validation of outputs

- **Statement:** Every output a step writes is structurally validated (common header, checks, limits) before publication, and an invalid output fails its item with `OUTPUT_INVALID` without publishing.
- **Covers:** Z2-I-ASSET-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_StructuralValidationRejects` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Structural validation"

### Z2-R-ASSET-103 Reference validation

- **Statement:** A hard reference to a GUID absent from the cook fails the referring item; a soft reference to one produces a warning.
- **Covers:** Z2-I-ASSET-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_MissingReferenceFails` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Reference validation"

### Z2-R-ASSET-104 Content rule validation

- **Statement:** Content rules registered by steps are evaluated on every item against the limits of the content-rule table, and a violation fails the item with `COOK_ERROR_RULE` naming the rule, the class, the limit and the measured value.
- **Covers:** Z2-I-ASSET-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ContentRuleEnforced` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"; Zenith2/Docs/Design/ASSET/Formats.md → "Content rule table"

### Z2-R-ASSET-105 Validation report

- **Statement:** Validation findings are written to the validation report with asset, rule, severity and values, and the cook exits 1 when any error-level finding exists.
- **Covers:** Z2-I-ASSET-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ContentRuleEnforced` and `Test_Cook_ExitCodes` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Validation report"

### Z2-R-ASSET-106 Provenance records

- **Statement:** Every cooked output has a provenance record naming its source, importer and step versions, settings hash, inputs and output hash, containing no time or host, identical across cooks.
- **Covers:** Z2-I-ASSET-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Provenance_RecordComplete` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Provenance record content"; Zenith2/Docs/Design/ASSET/Formats.md → "Provenance record lines"

### Z2-R-ASSET-107 Provenance queries

- **Statement:** Provenance queries by GUID, logical path or cooked path return the same record chain, through the API and through the cook server's `provenance` request.
- **Covers:** Z2-I-ASSET-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Provenance_QueryByEachForm` and `Test_CookServer_Protocol` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Provenance queries API"; Zenith2/Docs/Design/ASSET.md → "Provenance queries"

### Z2-R-ASSET-108 Interrupted cook resumes

- **Statement:** A cook journals every publication; a cook interrupted at any point and restarted under the output lock deletes the temporaries exited processes left, verifies journaled outputs and resumes to a tree byte-identical to a clean cook.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_InterruptedResumesToCleanBytes` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Interrupted cook and resume"; Zenith2/Docs/Design/ASSET/Cook.md → "Output tree and database locks"; Zenith2/Docs/Design/ASSET/Formats.md → "Cook journal"

### Z2-R-ASSET-109 Low disk during a cook

- **Statement:** Before each publication batch the cook checks free disk against the batch bytes plus `cook.lowDiskMarginBytes` and, when short, stops with exit 4 after deleting the batch's temporaries, leaving published outputs valid for resume.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_LowDiskStopsCleanly` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Low disk during a cook"

### Z2-R-ASSET-110 Stale output removal

- **Statement:** After a successful cook, outputs and catalogue fragments no current item produced are deleted and foreign files are warned about, so the tree holds exactly the catalogue's files plus the global outputs.
- **Covers:** Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_StaleOutputsRemoved` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Stale output removal"

### Z2-R-ASSET-111 Cook worker processes

- **Statement:** The cook can run items in a pool of worker processes over a documented protocol, with output identical to an in-process cook, an out-of-process import's diagnostics included.
- **Covers:** Z2-I-ASSET-09, Z2-I-ASSET-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_OutputIndependentOfWorkerCount` (worker processes enabled), `Test_Import_WorkerCrashContained` and `Test_Import_WorkerDiagnosticsThroughDdc` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Worker processes"

### Z2-R-ASSET-112 Cook server protocol

- **Statement:** `ZenithCook --serve` speaks protocol version 1 of line-delimited JSON with a `hello` handshake, one final `result` or `error` per request after any `partial` pages, progress, diagnostic, `changed` and `published` notifications, arrival-order processing and `cancel` at the next item boundary, supporting every request with the argument and result schemas of Formats.md "Requests"; `setSettings` carries the batch arguments (preset, provenance, sub-asset members, remaps, orphan removals and restores) in one journaled batch and `record` returns them.
- **Covers:** Z2-I-ASSET-04, Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** Z2-B-EDIT-004
- **Acceptance evidence:** `Test_CookServer_Protocol` and `Test_CookServer_PropertyRequest` in the `Test` step. `Test_CookServer_SetSettingsBatchArguments` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "The cook server"; Zenith2/Docs/Design/ASSET/Formats.md → "Cook server messages"; Zenith2/Docs/Design/ASSET/Formats.md → "Requests"

### Z2-R-ASSET-113 DDC key inputs

- **Statement:** A DDC key covers every input that can change an output (source and input bytes, resolved settings, importer or step ID and version, tool versions, codec and level, platform, engine cook version, key schema version) and no path, time or worker count.
- **Covers:** Z2-I-ASSET-07, Z2-I-ASSET-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataKey_EachInputChangesKey` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Key inputs"

### Z2-R-ASSET-114 DDC key encoding

- **Statement:** The DDC key is a Hash128 of the canonical encoding of its inputs that DDC.md specifies, and a fixed input list produces the committed golden key bytes.
- **Covers:** Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataKey_EncodingGolden` in the `Test` step (golden committed after the D-SEL-HASH ruling).
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Key encoding"

### Z2-R-ASSET-115 Compression settings in DDC keys

- **Statement:** Resolved compression settings (codec, level, dictionary) are DDC key inputs, so a changed compression setting re-cooks exactly the affected items.
- **Covers:** Z2-I-ASSET-08, Z2-I-ASSET-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataKey_EachInputChangesKey` and `Test_DerivedDataCache_InvalidationPerFingerprintInput` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Key inputs"; Zenith2/Docs/Design/ASSET/Import.md → "Compression setting"

### Z2-R-ASSET-116 Local DDC store

- **Statement:** The local DDC store lives under `${ZENITH_OUT}/DDC/v1` in the documented layout, and an entry is populated through a temporary and an atomic rename so no reader sees a partial entry.
- **Covers:** Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_PopulateThenHit` and `Test_DerivedDataCache_ConcurrentPopulateSameKey` in the `Test` step; `Zenith_Fuzz_DdcEntry` under `win-asan`.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Directory layout"; Zenith2/Docs/Design/ASSET/DDC.md → "Atomic population"; Zenith2/Docs/Design/ASSET/Formats.md → "DDC entry file"

### Z2-R-ASSET-117 DDC lookup populate verify

- **Statement:** A lookup checks the local store, then the shared store, verifying magic, version, check, key, producer fields and each payload hash; a verified shared hit is copied into the local store; a miss is produced and populated; an import is cached as an import record entry and one intermediate entry per product, and a hit of all of them skips `Import` and replays the import's stored diagnostics.
- **Covers:** Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_PopulateThenHit`, `Test_DerivedDataCache_SharedHitCopiedLocal`, `Test_DerivedDataCache_ImportEntriesRoundTrip` and `Test_CookReport_ReplaysOnSharedDdcHit` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Lookup populate and verify"; Zenith2/Docs/Design/ASSET/DDC.md → "Import entries"

### Z2-R-ASSET-118 Corrupt DDC entries

- **Statement:** An entry that fails verification is a miss: a local one is removed, a shared one is left untouched and reported, and the output is produced locally.
- **Covers:** Z2-I-ASSET-08, Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_CorruptEntryIsMiss` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Corrupt entry"

### Z2-R-ASSET-119 Stale DDC entries

- **Statement:** An entry whose key matches but whose producer fields differ is a stale miss, removed when local and listed in the report.
- **Covers:** Z2-I-ASSET-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_StaleEntryIsMiss` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Stale entry"

### Z2-R-ASSET-120 Unreachable DDC store

- **Statement:** An unreachable shared store never fails a cook: the item cooks locally, the condition is warned once per run, the shared store is disabled after three consecutive failures, and the tree equals a clean cook's.
- **Covers:** Z2-I-ASSET-08, Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_UnreachableFallsBackLocal` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Unreachable store"

### Z2-R-ASSET-121 Shared DDC store contract

- **Statement:** A shared DDC store implements `Zenith_DerivedDataStore` with UNAVAILABLE as a result value, never an exception or an assert, and the client never deletes from a shared store; the store interface, key and an entry verifier returning the producer fields are declared in `ZenithAuthoring` so a shared store and its administration tool live there.
- **Covers:** Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_UnreachableFallsBackLocal` and `Test_DerivedDataCache_CorruptEntryIsMiss` (fault-injecting store) in the `Test` step. `Test_AuthoringAsset_DerivedDataEntryVerifier` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Shared store contract"; Zenith2/Docs/Design/ASSET/DDC.md → "Zenith_DerivedDataStore"

### Z2-R-ASSET-122 DDC verification sampling

- **Statement:** A sampled fraction of hits (every hit with `--verify`) is re-produced locally and compared byte for byte; a difference quarantines (local) or reports (shared) the entry, uses the local output and warns naming the step.
- **Covers:** Z2-I-ASSET-07, Z2-I-ASSET-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_VerifyFailureQuarantines` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Verification sampling"

### Z2-R-ASSET-123 Local DDC size cap

- **Statement:** The local store is capped by `cook.ddc.localMaxBytes` and the disk margin: population evicts least recently used entries to 90 % of the cap, never an entry being read, and skips caching with a warning when the margin cannot be met.
- **Covers:** Z2-I-ASSET-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_EvictionLRU` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Size cap and eviction"

### Z2-R-ASSET-124 DDC invalidation per key input

- **Statement:** Changing any one fingerprint input (a source byte, a setting, an importer version, a step version, a tool version, a codec level, the engine cook version) re-cooks exactly the dependent items.
- **Covers:** Z2-I-ASSET-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_DerivedDataCache_InvalidationPerFingerprintInput` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/DDC.md → "Key inputs"; Zenith2/Docs/Design/ASSET/Tests.md → "DDC tests"

### Z2-R-ASSET-125 Cooked content hash

- **Statement:** The catalogue carries one cooked-content hash, defined once by the catalogue header (the low 64 bits of `Zenith_Hash128Of` over the entry, file entry, external reference, fallback and string tables), which is the cooked-content input of evidence fingerprints and changes when any output changes.
- **Covers:** Z2-I-ASSET-08, Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-065, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCatalogue_ContentHashCoversOutputs` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue header"; Zenith2/Docs/Design/ASSET/DDC.md → "Relationship to evidence fingerprints"

### Z2-R-ASSET-126 Meta document type registration

- **Statement:** ZenithCook and ZenithEditor register `Zenith.AssetMeta` into SCENE's `Zenith_MigrationRegistry` with the desc of Migration.md "The meta document type" (schema tag, version key, ID key `guid`, suffix `.meta`, canonical schema, importer-settings visitor), after the importer registry is frozen and before the migration registry freezes; ASSET declares no migration registry of its own.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_DocumentTypeRegistered` in the `Test` step; the `ZenithDocument` and `ZenithDocumentHost` library rows are core-doc item X109 (a) (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"; Zenith2/Docs/Design/ASSET/Migration.md → "Registered document types"

### Z2-R-ASSET-127 Meta migration preview

- **Statement:** An upgrade preview of `.meta` documents (`--migrate --preview`, the server's `migratePreview`) is the framework's plan over `.meta` paths: it writes nothing, and `migrateApply` with the preview's token writes exactly the previewed bytes or nothing when a document changed since.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_SettingsVisitorMigrates` and `Test_CookServer_Protocol` (`migratePreview`, `migrateApply`) in the `Test` step; the framework's own preview tests (`Zenith2/Docs/Design/SCENE/Tests.md` → "Document framework tests").
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Reading and writing meta documents through the framework"

### Z2-R-ASSET-128 Meta migration backup

- **Statement:** Every tool batch that changes an existing `.meta` backs it up first through the framework: migration batches under `.migration/backups/<UTC timestamp>/` and save batches under `.migration/backups/save-<UTC timestamp>/` of the document's storage root.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_WritesAreJournaled` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Reading and writing meta documents through the framework"

### Z2-R-ASSET-129 Journaled meta writes and recovery

- **Statement:** Every tool write of a `.meta` goes through the framework's journaled write, every sidecar-writing mode recovers pending journals before anything else, and the build's cook refuses a game with an unrecovered journal with `COOK_ERROR_GAME_INVALID` (exit code 3).
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_WritesAreJournaled` and `Test_Cook_RefusesPendingJournal` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Reading and writing meta documents through the framework"; Zenith2/Docs/Design/ASSET/Migration.md → "Recovery before a cook"

### Z2-R-ASSET-130 Unknown fields and removed settings in meta documents

- **Statement:** Unknown and `x-` members of a `.meta` survive every migration and write in position with one warning per field path per document; a setting an importer version removes is kept as `x-removed.<key>` with a dangling-override warning and never dropped; a renamed setting key is rewritten in `settings` and every platform object in the same step.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_UnknownFieldsPreserved` and `Test_AssetMeta_RemovedSettingKeptAsXRemoved` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Removed settings and unknown fields"

### Z2-R-ASSET-131 Newer meta documents open read-only

- **Statement:** A `.meta` whose `schemaVersion` or `importer.settingsVersion` is newer than the tool opens read-only: the cook uses it only when every key it needs parses, and no tool writes it.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_NewerVersionReadOnly` and `Test_AssetMeta_SettingsVisitorMigrates` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Importer settings inside meta documents"; Zenith2/Docs/Design/ASSET/Database.md → "Reading and writing meta documents"

### Z2-R-ASSET-132 Migration fixtures for meta documents

- **Statement:** Every `.meta` schema version has committed migration fixtures `v<N>.json` and `v<N>.expected.json`, and a schema version bump adds its fixture in the same diff.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Test_MetaMigration_Fixtures` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Meta migration fixtures"

### Z2-R-ASSET-133 Lock usage

- **Statement:** ZenithAssets takes only the locks listed in its lock-order entries, in table order, never holds a lock across a kind-loader call or an I/O submission, and passes the lock-order checker and TSan.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_StressUnderTsan` under `Sanitizers -Preset linux-tsan`; the lock-order checker in the `Test` step; the level-200 Guards wording is core-doc item X114 (c) (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET.md → "Lock-order entries"

### Z2-R-ASSET-134 Asset counters

- **Statement:** ZenithAssets publishes every counter the entry document lists under its exact name, the three counters of every queue of Loading.md "Internal queues" included.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15, Z2-I-ASSET-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetBackPressure_CountersPublished` and `Test_AssetResidency_AccountingPerSubsystem` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Counters"; Zenith2/Docs/Design/ASSET/Loading.md → "Internal queues"

### Z2-R-ASSET-135 On-screen profiler rows

- **Statement:** The on-screen profiler shows ASSET's rows (loads in flight, decode queue depth against Z2-B-ASSET-002, staging, resident bytes, cache, failures) in game and editor builds.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_CountersPublished` in the `Test` step; DEVT's profiler panel tests.
- **Design:** Zenith2/Docs/Design/ASSET.md → "On-screen profiler rows"

### Z2-R-ASSET-136 Log categories and messages

- **Statement:** ZenithAssets and ZenithCook log under the documented categories (`asset.archive` included) with the documented message fields, each failure class logs once per asset, and the `asset.missing` records are bounded as the registry specifies.
- **Covers:** Z2-I-ASSET-12, Z2-I-ASSET-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetFailure_LoggedOncePerClass` and `Test_AssetRegistry_MissingLogBounded` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Log categories"

### Z2-R-ASSET-137 Registry dump

- **Statement:** `WriteDump` writes a `zenith2.asset-dump` v1 document listing every live slot and in-flight load.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_DumpSchema` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Debug views and dumps"

### Z2-R-ASSET-138 Shutdown sequence

- **Statement:** At shutdown `BeginShutdown` refuses new requests with `SHUTTING_DOWN`, cancels every live request and read and flushes the cache; the job system drains service and decode slices until every operation completes; each kind loader's owner calls `UnregisterKindLoader` before destroying its loader; the registry is destroyed last.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetRegistry_DestroyReportsReferencedSlots` and `Test_AssetCancel_Stress1000Cycles` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Shutdown sequence"; Zenith2/Docs/Design/ASSET.md → "Construction and shutdown"

### Z2-R-ASSET-139 Development services absent from Shipping

- **Statement:** No Shipping build contains development reload, file watching, the cook server or any cook code, and `RequestReload` returns `NOT_ALLOWED` in Shipping.
- **Covers:** Z2-I-ASSET-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_CookServer_AbsentFromShipping` through the `Libgraph` step's Shipping link closure.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"; Zenith2/Docs/Design/ASSET/Cook.md → "The cook server"

### Z2-R-ASSET-140 Stage 1 ASSET budget ticket

- **Statement:** A Stage 1 per-system budget ticket for ASSET (a harness ticket plus a `needs-human` ruling ticket) sets the values of Z2-B-ASSET-001, Z2-B-ASSET-002 and the pending new rows with their configuration fragments, measured on `Zenith_Bench_AssetLoading`.
- **Covers:** Z2-I-ASSET-15, Z2-I-ASSET-16
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-ASSET-001, Z2-B-ASSET-002
- **Acceptance evidence:** the ruling ticket's measured report under `Zenith2/Docs/Research/` and the committed platform fragments; `Zenith_Bench_AssetLoading --smoke` in the `Test` step; the pending rows' Budgets.md entries are core-doc item X112 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET.md → "Rows ASSET owns"; Zenith2/Docs/Design/ASSET.md → "Pending new rows"; Zenith2/Docs/Design/ASSET.md → "Stage 1"

### Z2-R-ASSET-141 Asset memory within its row

- **Statement:** ZenithAssets' resident data and decode buffers stay within Z2-B-ASSET-001 on the W-Load workload on reference hardware.
- **Covers:** Z2-I-ASSET-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-001
- **Acceptance evidence:** `Zenith_Bench_AssetLoading` full run on reference hardware in Stage 4 acceptance.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Rows ASSET owns"

### Z2-R-ASSET-142 Decode queue depth within its row

- **Statement:** The decode queue depth high water stays within Z2-B-ASSET-002 on the W-Load workload.
- **Covers:** Z2-I-ASSET-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Zenith_Bench_AssetLoading` full run in Stage 4 acceptance, reading `asset.decode.queue.high_water`.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Rows ASSET owns"; Zenith2/Docs/Design/ASSET/Loading.md → "Back-pressure counters"

### Z2-R-ASSET-143 Boot loading within the cold-boot rows

- **Statement:** Loading the startup scene's asset set stays within the cold-boot rows on Windows and on the Galaxy S24 Ultra.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-010
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014
- **Acceptance evidence:** `Zenith_Bench_AssetLoading` and the cold-boot measurements in Stage 4 acceptance; the S24 run is a `needs-human` device ticket.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Rows ASSET is measured against"; Zenith2/Docs/Design/ASSET/Loading.md → "Tool and boot servicing"

### Z2-R-ASSET-144 Minimal game content cooks and loads on both platforms

- **Statement:** `Games/Minimal` cooks for Windows and Android, and its startup content loads on both platforms.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Integration_MinimalGameBothPlatforms` in the `Test` step (Windows headless boot); the Android half by the Stage 2 `needs-human` device ticket.
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Integration suite"

### Z2-R-ASSET-145 Loader stress under linux-tsan

- **Statement:** The loader stress (eight requester threads, random priorities, cancellations and handle drops over 20,000 loads) completes every operation once with balanced counts and no TSan report.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_StressUnderTsan` under `Sanitizers -Preset linux-tsan`: change-triggered once the Stage 2 PROC `needs-human` ticket adds the `linux-tsan` line and ZenithAssets' `Internal/Loading/**` and `Internal/Registry/**` paths to the `Zenith2Memory` category (a protected `zagent.project.json` edit, cross-area row AS7), and in every stage acceptance's sanitizer set until then.
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Cancellation and stress tests"; Zenith2/Docs/Design/ASSET.md → "Sanitizer, fuzz and stress runs"

### Z2-R-ASSET-146 Examples compiled as tests

- **Statement:** The examples RuntimeAPI.md shows compile and run as tests.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_Example_Runs` and `Test_CookedFile_Example_Runs` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Examples"

### Z2-R-ASSET-147 Cooked compatibility demonstrated

- **Statement:** Cooked content of another engine cook version or format version is refused with `VERSION_MISMATCH` and re-cooked, demonstrated with committed fixtures from the shipped release line, and full-duration fuzzing of every cooked format is clean.
- **Covers:** Z2-I-ASSET-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CookedCompat_MismatchedFixturesRefused` in the `Test` step and the full-duration fuzz runs, in Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/ASSET/Tests.md → "Missing and corrupt content tests"; Zenith2/Docs/Design/ASSET.md → "Stage 5 to Stage 7"

### Z2-R-ASSET-148 Kind code allocation and kind sets

- **Statement:** Defined asset kind codes are dense below `uASSET_KIND_CODE_CAP` (256): a new kind takes code `uASSET_KIND_DEFINED_COUNT` in an ASSET-reviewed API change of its owning area that adds the table row, the enumerator and the `KindTable.json` row and increments `uASSET_KIND_DEFINED_COUNT` and `uZENITH_ENGINE_COOK_VERSION`; `Zenith_AssetKindSet` holds one bit per code below the cap and one per test code, and ignores every other code.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048, Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetTypes_KindTableMatchesDesign` and `Test_AssetTypes_KindSetAndLookups` in the `Test` step; the `api-change` review of any diff adding a kind.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Asset kinds"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Asset kinds and the kind table"

### Z2-R-ASSET-149 Format versions of unused kinds

- **Statement:** A catalogue's kind table holds each kind's format version or 0 for no content; every entry matches its kind's element, a merge refuses disagreeing non-zero elements, and `RegisterKindLoader` accepts any format version for a kind whose element is 0 and refuses a mismatch with a non-zero one with `VERSION_MISMATCH`.
- **Covers:** Z2-I-ASSET-13, Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCatalogue_KindTableRules` and `Test_AssetRegistry_RegisterKindLoaderRules` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Kind table"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetCatalogue header"

### Z2-R-ASSET-150 Asset reference-default resolvers

- **Statement:** `Zenith_ResolveAssetReferenceDefault` resolves a schema's non-null `asset<K>` default to a counted handle of a catalogue asset of kind K (false and a null element otherwise) when the composition root calls `ResolveReferenceDefaults` at startup step 11, and the cook's `Zenith_ResolveCookAssetReferenceDefault` accepts only a live database record of the kind, so an unresolvable default stops startup with exit code 3 and fails the cook.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-038, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetHandle_ResolveReferenceDefault` and `Test_AssetDatabase_CookResolverForDefaults` in the `Test` step — the step-11 `Create` is core-doc item X110 and the step-11 `ResolveReferenceDefaults` with exit code 3 is core-doc item X147 (4) (both pending).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Reflection value hooks"; Zenith2/Docs/Design/ASSET/Database.md → "Reference defaults in the cook"

### Z2-R-ASSET-151 GPU kinds activate before their uploads complete

- **Statement:** A GPU-resident kind's loader activates its object once it is complete on the CPU side and its uploads are requested into its owner's pending list by a step that cannot fail, never waiting for the owner's allocation or a rendered frame; the slot is ACTIVE with residency PENDING until the loader calls `ReportGpuResident` with the attempt's ticket (a stale ticket is ignored), so pinned GPU-kind fallbacks and a scene's GPU-kind hard dependencies are ACTIVE before startup steps 15 and 17 return; the registry's availability epoch (`GetAvailabilityEpoch`, ANY_THREAD, 0 until the first increment, never reset) moves by one after every activation that publishes a non-null object (load, reload swap, recovery), every residency change to RESIDENT or FAILED and every load failure, and at no other event, so a reader that sees it move also sees the publication.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_GpuKindActivatesBeforeUpload`, `Test_AssetLoader_BootWithoutRendering` and `Test_AssetRegistry_AvailabilityEpochAdvancesOnActivation` (its reader case also under `Sanitizers -Preset linux-tsan` once the PROC path edit of Z2-R-ASSET-145 lands) in the `Test` step; GEOM's loaders adopt the rule through cross-area row AS29, and RPROD's extraction resolver reads the epoch through cross-area row RP33.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"; Zenith2/Docs/Design/ASSET/Loading.md → "Tool and boot servicing"; Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoadContext"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"

### Z2-R-ASSET-152 Engine fallbacks cooked into every catalogue

- **Statement:** Each ASSET-phase cook step declares its kind's engine fallbacks by logical path and role (exactly one role-0 row for a SUBSTITUTE kind); the cook adds them to every game's plan, fails with `COOK_ERROR_FALLBACK_MISSING` (exit code 3) when one is absent, and lists them in the base catalogue's fallback table, from which the runtime pins them, so no fallback GUID is compiled into a library.
- **Covers:** Z2-I-ASSET-12, Z2-I-ASSET-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_EngineFallbacksInEveryCatalogue`, `Test_Cook_FallbackMissingFails` and `Test_AssetCatalogue_FallbackTableRules` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"; Zenith2/Docs/Design/ASSET/Formats.md → "Fallback table"

### Z2-R-ASSET-153 Every state and event of the asset state machine

- **Statement:** For events E1 to E10 and E16 to E19, the asset slot state changes exactly as the state tables of Loading.md "Asset state machine" give for every (state, event) pair — acquisition and load requests during UNLOADING (the unload completes, then the load starts), the QUEUED sub-stages, shutdown, loader registration, registration closing and loader unregistration included — and so does every second-pipeline state of "Reload and recovery pipelines" for those events: a RETIRING old object is unloaded at once at E8, E16 and E17, its acknowledgement RA is never E10, and a slot is freed only after RA, so at most two unload acknowledgements per slot are outstanding; the rows of E11 (reload), E12 (device recovery) and E13 to E15 (catalogue swaps) are proved with the requirements of their stages.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetStateMachine_EveryTransition`, `Test_AssetRegistry_ReviveDuringUnloadWaitsForAck` and the E8, E16 and E17 cases of `Test_AssetStateMachine_ReloadAndRecoveryPipelines` in the `Test` step, asserting at Stage 2 every row of events E1 to E10 and E16 to E19; the E12 rows join the same tests at Stage 3 (Z2-R-ASSET-062, -173) and the E11 and E13 to E15 rows at Stage 4 (Z2-R-ASSET-061, -014).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Asset state machine"; Zenith2/Docs/Design/ASSET/Loading.md → "Reload and recovery pipelines"

### Z2-R-ASSET-154 Catalogue fragment external references

- **Statement:** A fragment's dependency on an asset listed in another catalogue is written with `ASSET_DEPENDENCY_FLAG_EXTERNAL` indexing the fragment's external reference table of GUIDs; the merge resolves every external reference or fails with `CATALOGUE_INVALID`, and no reserved field of any catalogue carries data.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetCatalogue_ExternalReferences` and `Test_AssetCatalogue_MergePrecedence` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue fragments"; Zenith2/Docs/Design/ASSET/Formats.md → "Dependency and string tables"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetCatalogue header"

### Z2-R-ASSET-155 Archive format validation

- **Statement:** The archive reader accepts exactly the version-1 `.zpak` layout PACK writes (PACK owns the writer and the packing policy of Z2-I-PACK-02), applying checks A1 to A7 in order and failing with `FILE_ERROR_ARCHIVE_CORRUPT` naming the check, never asserting, within the archive limits tested at the limit and at limit + 1.
- **Covers:** Z2-I-ASSET-14, Z2-I-PACK-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetArchive_EachCheckRejects` and `Test_AssetArchive_LimitsAtAndAboveLimit` in the `Test` step; `Zenith_Fuzz_Archive` (`win-asan` smoke in `Test`, full duration in stage acceptance); the Parser limits row is core-doc item X111 (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Archive file"; Zenith2/Docs/Design/ASSET/Formats.md → "Archive checks"; Zenith2/Docs/Design/ASSET/Formats.md → "Archive limits"

### Z2-R-ASSET-156 Archive lookup listing and reads

- **Statement:** An archive source implements PLAT's `Zenith_MountSource`: exact byte-order lookups of files and implied directories, listing of next path components in byte order, and bounded positional reads of stored entries through its container, and `Zenith_VerifyArchiveData` checks every entry's data hash.
- **Covers:** Z2-I-ASSET-14, Z2-I-PACK-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetArchive_LookupListRead` and `Test_AssetArchive_DataHashesMatch` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetArchive header"

### Z2-R-ASSET-157 Archive sources mounted through the PLAT factory

- **Statement:** The composition root passes `Zenith_CreateArchiveMountSource` and a `Zenith_ArchiveReaderDesc` to the virtual filesystem at startup step 5, so the package mount, the verified patch mount and installed asset packs mount `.zpak` archives through ZenithAssets' reader while ZenithPlatform declares no dependency on ZenithAssets.
- **Covers:** Z2-I-ASSET-14, Z2-I-PACK-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetArchive_MountedThroughVFS` in the `Test` step; conditional on PLAT's factory member and `Zenith_SourceFile` widening and on the startup table's step-5 row, a new ask recorded as core-doc and area item X154 (open) and cross-area row AS22; the ownership wording is core-doc item X114 (d) (pending batch 4).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetArchive header"; Zenith2/Docs/Design/ASSET.md → "Construction and shutdown"

### Z2-R-ASSET-158 Cook server handshake and protocol version

- **Statement:** A cook server session starts with `hello`: a request before it is `COOK_ERROR_PROTOCOL`, and a protocol version or engine cook version differing from the server's is `COOK_ERROR_PROTOCOL_VERSION` followed by exit code 2; worker processes make the same check through `workerHello`, a mismatch stopping the cook with exit code 5.
- **Covers:** Z2-I-ASSET-04, Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CookServer_HelloHandshake` and `Test_CookServer_WorkerHelloMismatch` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Framing"; Zenith2/Docs/Design/ASSET/Formats.md → "Session rules"; Zenith2/Docs/Design/ASSET/Formats.md → "Requests"

### Z2-R-ASSET-159 Cook server result paging and line limit

- **Statement:** Cook server lines are at most 4 MiB; a result that can hold many records — `provenance` included — is sent as `partial` pages of at most 256 items that each fit the line limit, followed by one final `result` carrying the item and page counts; a single item larger than the limit ends its request with `COOK_ERROR_RESULT_TOO_LARGE`, and an over-long client line is refused and skipped.
- **Covers:** Z2-I-ASSET-04, Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookServer_PagedResults` and `Test_CookServer_Protocol` (the `provenance` paging and too-large cases) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Framing"; Zenith2/Docs/Design/ASSET/Formats.md → "Requests"

### Z2-R-ASSET-160 Cook server change notifications

- **Statement:** With watching enabled the cook server, after the debounce, sends `changed` for sources that changed on disk, reimports and cooks them in arrival order, and sends `published` naming the changed GUIDs, whether the catalogue changed and the content hash, so the editor refreshes its registries' catalogues and reloads the assets.
- **Covers:** Z2-I-ASSET-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** Z2-B-EDIT-004
- **Acceptance evidence:** `Test_CookServer_WatchNotifies` and `Test_Integration_ReimportThenLoad` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Notifications"; Zenith2/Docs/Design/ASSET/Cook.md → "The cook server"

### Z2-R-ASSET-161 Cook server unknown requests and cancellation

- **Statement:** An unknown request, a malformed line, an unknown argument, a wrongly typed one, a second `hello` or a request reusing an unfinished request's `id` (answered with `id` null) is answered with `COOK_ERROR_PROTOCOL` naming it while the session continues (16 in a row end it with exit code 2); `cancel` and `shutdown` are control requests answered on arrival: `cancel` removes a queued request at once, stops the running one at its next item boundary and reports whether it cancelled anything before the target's final line, and `shutdown` cancels every queued and running request the same way before its own `result`.
- **Covers:** Z2-I-ASSET-04, Z2-I-ASSET-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CookServer_MalformedAndUnknownRequests`, `Test_CookServer_CancelQueuedAndRunning` and `Test_CookServer_Protocol` (the `shutdown` case) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Session rules"

### Z2-R-ASSET-162 Queue full policies and queue counters

- **Statement:** Every queue ZenithAssets owns declares its full policy in its owner's contract block — REJECT for the request intake and requester completion queues, FATAL for the retire, acknowledgement and completion queues whose capacities are derived so they never fill, REJECT-behaving admission for the decode-unit count — and publishes its `.full`, `.dropped` and `.high_water` counters.
- **Covers:** Z2-I-ASSET-16, Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-045
- **Budgets:** Z2-B-ASSET-002
- **Acceptance evidence:** `Test_AssetBackPressure_CountersPublished` in the `Test` step; the `ApiContractCheck` step over the contract blocks.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Internal queues"; Zenith2/Docs/Design/ASSET.md → "Counters"

### Z2-R-ASSET-163 Content rules read from a data table

- **Statement:** The cook reads content-rule limits from `Zenith2/Docs/ContentSpecs.rules.json` and never parses Markdown; a table failing its schema or naming a quantity no step measures is `COOK_ERROR_RULE_TABLE_INVALID` (exit code 3), and a test keeps the table equal to `Zenith2/Docs/ContentSpecs.md`.
- **Covers:** Z2-I-ASSET-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentRules_TableAgreesWithContentSpecs` and `Test_Cook_ContentRuleTableInvalid` in the `Test` step; the table file itself is the ContentSpecs owner's (cross-area row AS27).
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"; Zenith2/Docs/Design/ASSET/Formats.md → "Content rule table"

### Z2-R-ASSET-164 Tracked tombstone lists

- **Statement:** Tombstoned GUIDs are kept in tracked `.zenith-tombstones` lists at the top of each content storage root, never as a new top-level entry of `Games/<Name>/`, in the sorted line format of Formats.md "Tombstone list", written only by sidecar-writing modes and read leniently.
- **Covers:** Z2-I-ASSET-05, Z2-I-ASSET-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_TombstoneLists` in the `Test` step; the `zenith_add_game` configure of every fixture game ([Z2-N-ARCH-246]).
- **Design:** Zenith2/Docs/Design/ASSET/Formats.md → "Tombstone list"; Zenith2/Docs/Design/ASSET/Database.md → "Database file location and rebuild"

### Z2-R-ASSET-165 Built-in data and dictionary loaders

- **Statement:** ZenithAssets registers built-in loaders for `ASSET_KIND_DATA` (into `Zenith_DataAsset`) and `ASSET_KIND_COMPRESSION_DICTIONARY` (into `Zenith_CompressionDictionaryAsset` created through COMPRESS), so data files and dictionaries load in every game, and a section naming a dictionary of another ID fails to decode.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetBuiltIn_DataAndDictionaryLoad` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_BuiltInAssets header"

### Z2-R-ASSET-166 Cancelled loads are never published

- **Statement:** A raw asset-load requester publishes a SUCCEEDED load as a recomputed publication with no payload and every FAILED load as a recorded publication with the 24-byte `Zenith_AssetLoadResultRecord` (status 3 only), and never publishes a CANCELLED completion; a requester that still needs the result re-issues the request under the same completion ID.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoadResult_EncodeDecode` in the `Test` step; the requesters' replay tests (SCRIPT, STREAM).
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Publication to simulation"; Zenith2/Docs/Design/ASSET/Formats.md → "Load result payload"

### Z2-R-ASSET-167 Importer settings migrate inside meta documents

- **Statement:** A `.meta`'s importer-settings objects migrate from `importer.settingsVersion` to the importer's current version inside the framework's open (the type's embedded visitor, one step per version over `settings` and each platform object), so a settings upgrade is previewed, backed up and journaled like an envelope upgrade, and an importer whose settings chain has a gap is refused at registration.
- **Covers:** Z2-I-ASSET-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetMeta_SettingsVisitorMigrates` and `Test_ImporterRegistry_SettingsChainGapRejected` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Migration.md → "Importer settings inside meta documents"; Zenith2/Docs/Design/ASSET/Import.md → "Settings schema"

### Z2-R-ASSET-168 Test kinds declared per registry

- **Statement:** Test kinds 0x7F00 to 0x7F0F exist only in tests: a registry created with `m_bAllowTestKinds` accepts the rows its desc declares in `m_axTestKinds` and answers every kind column through `GetKindInfo` and `GetGpuResidentKinds`, a catalogue carries their format versions in a test-kind table behind header flag bit 1 that only such a registry loads, an undeclared test code is `NOT_ALLOWED` (a catalogue entry of one at `Create` step 6), and `Create` refuses `m_bAllowTestKinds` or a non-empty `m_axTestKinds` with `NOT_ALLOWED` in a Shipping build (`Zenith_IsShippingBuild()`) or with development features off.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetTypes_KindSetAndLookups`, `Test_AssetCatalogue_KindTableRules`, `Test_AssetRegistry_RegisterKindLoaderRules` and the desc-validation and step-6 cases of `Test_AssetRegistry_Create_FailsAt<Step>_Unwinds` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET.md → "Asset kinds"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Asset kinds and the kind table"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistryDesc"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET/Formats.md → "Kind table"; Zenith2/Docs/Design/ASSET/Tests.md → "Fixtures"

### Z2-R-ASSET-169 GPU residency failure after activation

- **Statement:** An allocation or upload that fails after a GPU-resident kind's activation is reported by its loader with `ReportGpuFailed`, which moves the residency from PENDING to FAILED without failing the load, substitutes the kind's fallback in `ResolveOrFallback`, counts `asset.gpu.failed` and logs once, and is cleared only by a reload or a device recovery.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15, Z2-I-ASSET-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_GpuKindActivatesBeforeUpload` (the failure case) and `Test_AssetFailure_FallbackPerPolicy` in the `Test` step; GEOM's loaders report their post-activation failures this way through cross-area row AS29.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"; Zenith2/Docs/Design/ASSET/Loading.md → "Fallback policy per kind"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"

### Z2-R-ASSET-170 Residency reports race-free across reloads

- **Statement:** Every admitted pipeline is an attempt with a ticket of its own — issued at admission and again at every E12 restart from a 28-bit per-slot counter that is never reset and never reissues a value — and the slot's residency word holds the installed and current attempt values, the open flag, the residency and the open attempt's early outcome, changed only by the seven compare-exchange writers of RuntimeAPI.md "Slot header layout": a report of the installed PENDING activation applies, a report of the open attempt made before its activation is kept and installed by that activation, and any other report is ignored, so no report is lost, none reaches another attempt (a cancelled-after-push or restarted attempt included), no activated slot stays PENDING once its report is made, and the residency gauges move by the field's old and new values.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the race, early-report and stale-attempt cases of `Test_AssetLoader_GpuKindActivatesBeforeUpload` in the `Test` step and under `Sanitizers -Preset linux-tsan` (change-triggered once the PROC path edit of Z2-R-ASSET-145 lands; in every stage acceptance's sanitizer set until then).
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Slot header layout"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Priorities and states"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoadContext"; Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"

### Z2-R-ASSET-171 Loads wait for late-registering loaders

- **Statement:** A load of a kind whose loader has not yet registered waits in sub-stage WAIT_LOADER, counted, while its dependencies load, and resumes when that kind's loader registers; at the slice that applies `CloseRegistration`, the service fails with `NO_LOADER` only the waiting pipelines whose kind is absent from the served set it reads, so the registration order of steps 11 to 15, or a registration immediately followed by `CloseRegistration`, never fails a pinned asset or a hard dependency whose kind registers.
- **Covers:** Z2-I-ASSET-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_BootWithoutRendering` (the late-registration, never-registered and register-then-close cases) and the E18 and E19 rows of `Test_AssetStateMachine_EveryTransition` in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Load record stages"; Zenith2/Docs/Design/ASSET/Loading.md → "Asset state machine"; Zenith2/Docs/Design/ASSET/Loading.md → "Tool and boot servicing"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"

### Z2-R-ASSET-172 Queued pipelines hold no load record

- **Statement:** A pipeline waiting for its catalogue lookup, its hard dependencies, its loader or admission holds no load record and counts in neither in-flight limit; load records come from a pool of exactly `asset.load.maxInFlight` taken at admission, and a kind loader never defers `Activate`, `Fail` or `AcknowledgeCancel` until a load it requested completes, so no set of dependents or loaders can hold the capacity the loads they wait for need.
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the dependency-chain case of `Test_AssetResidency_InFlightBudget` (`asset.load.maxInFlight` = 1, the one case of that test asserted at Stage 2) and `Test_AssetCancel_Stress1000Cycles` (no record left) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Slot side records"; Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Internal types and files"; Zenith2/Docs/Design/ASSET/Loading.md → "Dependency loading"; Zenith2/Docs/Design/ASSET/Loading.md → "In-flight load budget"

### Z2-R-ASSET-173 Reload and recovery pipelines pass admission

- **Statement:** Development reload and device-recovery pipelines wait for admission like loads and are admitted within `asset.load.maxInFlight` and the byte limit, device recoveries pinned first and then in (kind code, GUID) order, so a device loss with more GPU-resident assets than the in-flight limit recovers them all without growing a pool, a queue or an operation table; every pipeline state has a row for every event — a second device loss, a device loss during retirement (the new object's recovery runs while the old one retires), catalogue swaps (a queued reload returns to CATALOGUE, a queued or running recovery is cancelled with residency LOST), and loader registration events (no effect).
- **Covers:** Z2-I-ASSET-14, Z2-I-ASSET-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` (64 GPU assets, `asset.load.maxInFlight` = 4) and `Test_AssetStateMachine_ReloadAndRecoveryPipelines` (every row, the E12, E13 and E14 cells included) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ASSET/Loading.md → "Reload and recovery pipelines"; Zenith2/Docs/Design/ASSET/Loading.md → "In-flight load budget"; Zenith2/Docs/Design/ASSET/Loading.md → "Device-loss reload"; Zenith2/Docs/Design/ASSET/Loading.md → "Internal queues"

### Z2-R-ASSET-174 Cook output tree and database locks

- **Statement:** Every ZenithCook mode that writes an output tree holds its lock for the run (a cook server for its whole session), so a build cook of a tree a server holds exits with code 6 and `COOK_ERROR_OUTPUT_LOCKED` having read, written and deleted nothing; a process that cannot take the database lock, which a holder keeps from start to exit, works on an unsaved copy while still writing sidecars and tombstone lists; and every writer of a document storage root holds that root's writer lock while it recovers, decides and writes (a CLI sidecar mode for its run, the server per request), so a journal is recovered only when its writer has exited, a refused writer exits with code 6 and `COOK_ERROR_SOURCE_LOCKED` naming no conflicting command, and a build cook that finds a live writer's journal waits and then exits with code 6, never 3.
- **Covers:** Z2-I-ASSET-09, Z2-I-ASSET-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_ConcurrentServerAndBuildExclusive` (all six cases) and `Test_Cook_RefusesPendingJournal` in the `Test` step; the writers take the storage-root lock through SCENE's `Zenith_DocumentWriterLock` (`GetWriterLock()`), whose `Test_HostDocumentStorage_WriterLockSerialisesWriters` covers the editor's save batches (cross-area rows AS31, AS32).
- **Design:** Zenith2/Docs/Design/ASSET/Cook.md → "Output tree and database locks"; Zenith2/Docs/Design/ASSET/Cook.md → "Exit codes"; Zenith2/Docs/Design/ASSET/Formats.md → "Cook lock file"; Zenith2/Docs/Design/ASSET/Database.md → "Database file location and rebuild"; Zenith2/Docs/Design/ASSET/Migration.md → "Recovery before a cook"

### Z2-R-ASSET-175 Deleted asset GUIDs reach the tracked tombstone lists

- **Statement:** A diff that deletes an asset source's `.meta` appends the deleted GUID's line to the tracked `.zenith-tombstones` of its content root in the same diff, and a cook whose database knew a deleted GUID missing from the tracked list reports `COOK_WARNING_TOMBSTONE_NOT_TRACKED` with the line to add, so a GUID deleted anywhere is never reissued on any checkout.
- **Covers:** Z2-I-ASSET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AssetDatabase_TombstoneLists` (the untracked-deletion case) in the `Test` step; review of every diff that deletes a `.meta` (the worker rule of Database.md "Orphaned meta files and tombstones").
- **Design:** Zenith2/Docs/Design/ASSET/Database.md → "Orphaned meta files and tombstones"; Zenith2/Docs/Design/ASSET/Formats.md → "Tombstone list"

## Coverage of inventory items

| Item | Text | Inventory stage | Requirements |
|---|---|---|---|
| Z2-I-ASSET-01 | asset database | S2 | Z2-R-ASSET-070, Z2-R-ASSET-071, Z2-R-ASSET-073, Z2-R-ASSET-074, Z2-R-ASSET-075, Z2-R-ASSET-076, Z2-R-ASSET-077, Z2-R-ASSET-079, Z2-R-ASSET-080, Z2-R-ASSET-164, Z2-R-ASSET-174 |
| Z2-I-ASSET-02 | asset dependency graph | S2 | Z2-R-ASSET-026, Z2-R-ASSET-078, Z2-R-ASSET-099, Z2-R-ASSET-172 |
| Z2-I-ASSET-03 | import | S2 | Z2-R-ASSET-083, Z2-R-ASSET-084, Z2-R-ASSET-085, Z2-R-ASSET-089, Z2-R-ASSET-090, Z2-R-ASSET-091, Z2-R-ASSET-092, Z2-R-ASSET-093, Z2-R-ASSET-094, Z2-R-ASSET-095, Z2-R-ASSET-111, Z2-R-ASSET-144 |
| Z2-I-ASSET-04 | reimport | S4 | Z2-R-ASSET-059, Z2-R-ASSET-061, Z2-R-ASSET-086, Z2-R-ASSET-087, Z2-R-ASSET-088, Z2-R-ASSET-112, Z2-R-ASSET-139, Z2-R-ASSET-158, Z2-R-ASSET-159, Z2-R-ASSET-160, Z2-R-ASSET-161 |
| Z2-I-ASSET-05 | stable GUIDs | S2 | Z2-R-ASSET-004, Z2-R-ASSET-005, Z2-R-ASSET-015, Z2-R-ASSET-016, Z2-R-ASSET-071, Z2-R-ASSET-072, Z2-R-ASSET-073, Z2-R-ASSET-074, Z2-R-ASSET-075, Z2-R-ASSET-087, Z2-R-ASSET-164, Z2-R-ASSET-175 |
| Z2-I-ASSET-06 | importer settings | S2 | Z2-R-ASSET-070, Z2-R-ASSET-079, Z2-R-ASSET-081, Z2-R-ASSET-082, Z2-R-ASSET-084, Z2-R-ASSET-115, Z2-R-ASSET-126, Z2-R-ASSET-127, Z2-R-ASSET-128, Z2-R-ASSET-129, Z2-R-ASSET-130, Z2-R-ASSET-131, Z2-R-ASSET-132, Z2-R-ASSET-167 |
| Z2-I-ASSET-07 | derived-data cache | S4 | Z2-R-ASSET-113, Z2-R-ASSET-114, Z2-R-ASSET-116, Z2-R-ASSET-117, Z2-R-ASSET-118, Z2-R-ASSET-120, Z2-R-ASSET-121, Z2-R-ASSET-122, Z2-R-ASSET-123 |
| Z2-I-ASSET-08 | derived-data cache invalidation | S4 | Z2-R-ASSET-113, Z2-R-ASSET-115, Z2-R-ASSET-118, Z2-R-ASSET-119, Z2-R-ASSET-120, Z2-R-ASSET-122, Z2-R-ASSET-124, Z2-R-ASSET-125 |
| Z2-I-ASSET-09 | incremental cooking | S4 | Z2-R-ASSET-021, Z2-R-ASSET-090, Z2-R-ASSET-091, Z2-R-ASSET-095, Z2-R-ASSET-096, Z2-R-ASSET-097, Z2-R-ASSET-098, Z2-R-ASSET-099, Z2-R-ASSET-100, Z2-R-ASSET-101, Z2-R-ASSET-108, Z2-R-ASSET-109, Z2-R-ASSET-110, Z2-R-ASSET-111, Z2-R-ASSET-112, Z2-R-ASSET-125, Z2-R-ASSET-152, Z2-R-ASSET-158, Z2-R-ASSET-159, Z2-R-ASSET-161, Z2-R-ASSET-174 |
| Z2-I-ASSET-10 | asset validation | S4 | Z2-R-ASSET-019, Z2-R-ASSET-101, Z2-R-ASSET-102, Z2-R-ASSET-103, Z2-R-ASSET-104, Z2-R-ASSET-105, Z2-R-ASSET-163 |
| Z2-I-ASSET-11 | asset provenance | S4 | Z2-R-ASSET-106, Z2-R-ASSET-107 |
| Z2-I-ASSET-12 | missing assets | S4 | Z2-R-ASSET-014, Z2-R-ASSET-016, Z2-R-ASSET-063, Z2-R-ASSET-064, Z2-R-ASSET-066, Z2-R-ASSET-067, Z2-R-ASSET-069, Z2-R-ASSET-136, Z2-R-ASSET-150, Z2-R-ASSET-152, Z2-R-ASSET-154, Z2-R-ASSET-166, Z2-R-ASSET-169 |
| Z2-I-ASSET-13 | corrupt assets | S4 | Z2-R-ASSET-013, Z2-R-ASSET-018, Z2-R-ASSET-019, Z2-R-ASSET-022, Z2-R-ASSET-043, Z2-R-ASSET-065, Z2-R-ASSET-066, Z2-R-ASSET-067, Z2-R-ASSET-068, Z2-R-ASSET-069, Z2-R-ASSET-136, Z2-R-ASSET-147, Z2-R-ASSET-149 |
| Z2-I-ASSET-14 | async loading | S2 | Z2-R-ASSET-001, Z2-R-ASSET-002, Z2-R-ASSET-003, Z2-R-ASSET-004, Z2-R-ASSET-006, Z2-R-ASSET-007, Z2-R-ASSET-008, Z2-R-ASSET-009, Z2-R-ASSET-010, Z2-R-ASSET-011, Z2-R-ASSET-012, Z2-R-ASSET-013, Z2-R-ASSET-014, Z2-R-ASSET-015, Z2-R-ASSET-017, Z2-R-ASSET-018, Z2-R-ASSET-020, Z2-R-ASSET-021, Z2-R-ASSET-023, Z2-R-ASSET-024, Z2-R-ASSET-025, Z2-R-ASSET-026, Z2-R-ASSET-027, Z2-R-ASSET-028, Z2-R-ASSET-029, Z2-R-ASSET-030, Z2-R-ASSET-031, Z2-R-ASSET-032, Z2-R-ASSET-033, Z2-R-ASSET-034, Z2-R-ASSET-035, Z2-R-ASSET-036, Z2-R-ASSET-037, Z2-R-ASSET-038, Z2-R-ASSET-039, Z2-R-ASSET-040, Z2-R-ASSET-041, Z2-R-ASSET-042, Z2-R-ASSET-043, Z2-R-ASSET-062, Z2-R-ASSET-133, Z2-R-ASSET-134, Z2-R-ASSET-135, Z2-R-ASSET-137, Z2-R-ASSET-138, Z2-R-ASSET-143, Z2-R-ASSET-144, Z2-R-ASSET-145, Z2-R-ASSET-146, Z2-R-ASSET-148, Z2-R-ASSET-149, Z2-R-ASSET-150, Z2-R-ASSET-151, Z2-R-ASSET-153, Z2-R-ASSET-154, Z2-R-ASSET-155, Z2-R-ASSET-156, Z2-R-ASSET-157, Z2-R-ASSET-162, Z2-R-ASSET-165, Z2-R-ASSET-166, Z2-R-ASSET-168, Z2-R-ASSET-169, Z2-R-ASSET-170, Z2-R-ASSET-171, Z2-R-ASSET-172, Z2-R-ASSET-173 |
| Z2-I-ASSET-15 | residency budgets | S4 | Z2-R-ASSET-011, Z2-R-ASSET-046, Z2-R-ASSET-052, Z2-R-ASSET-053, Z2-R-ASSET-054, Z2-R-ASSET-055, Z2-R-ASSET-056, Z2-R-ASSET-057, Z2-R-ASSET-058, Z2-R-ASSET-059, Z2-R-ASSET-060, Z2-R-ASSET-062, Z2-R-ASSET-134, Z2-R-ASSET-140, Z2-R-ASSET-141, Z2-R-ASSET-151, Z2-R-ASSET-153, Z2-R-ASSET-169, Z2-R-ASSET-173 |
| Z2-I-ASSET-16 | decompression back-pressure: bounded decode queues | S4 | Z2-R-ASSET-020, Z2-R-ASSET-029, Z2-R-ASSET-035, Z2-R-ASSET-044, Z2-R-ASSET-045, Z2-R-ASSET-047, Z2-R-ASSET-048, Z2-R-ASSET-049, Z2-R-ASSET-050, Z2-R-ASSET-051, Z2-R-ASSET-134, Z2-R-ASSET-135, Z2-R-ASSET-140, Z2-R-ASSET-142, Z2-R-ASSET-162, Z2-R-ASSET-165 |
| Z2-I-ASSET-17 | decompression back-pressure: I/O throttled by decode capacity | S4 | Z2-R-ASSET-035, Z2-R-ASSET-045, Z2-R-ASSET-046, Z2-R-ASSET-047, Z2-R-ASSET-048, Z2-R-ASSET-050, Z2-R-ASSET-051 |

Items of other areas that ASSET requirements serve, because the rulings place the mechanism in ASSET (the owning area's catalogue covers the item as a whole):

| Item | Text | Owning area | ASSET requirements |
|---|---|---|---|
| Z2-I-PACK-02 | archives | PACK (the `.zpak` reader is ASSET's by ruling; the packer and packing policy stay PACK's) | Z2-R-ASSET-155, Z2-R-ASSET-156, Z2-R-ASSET-157 |

Stage notes. Every item has at least one requirement at or before its inventory stage, so each item's capability is delivered when the inventory says. A requirement's own stage is the stage whose acceptance it serves, and differs from its items' inventory stages in two directions (numbers are `Z2-R-ASSET-NNN`):

| Direction | Requirements | Why |
|---|---|---|
| before one of its items' stages | S1: 140; S2: 011, 013, 016, 018, 019, 020, 021, 022, 029, 043, 044, 067, 068, 090, 091, 095, 096, 097, 101, 102, 103, 110, 125, 134, 135, 136, 149, 150, 151, 152, 153, 162, 165, 166, 169; S3: 062, 063, 065, 066, 069, 087, 100, 139, 173 | Stage 1 sets the budget rows; Stage 2 builds the mechanisms the S4 items extend (the cooked container, checks and fuzzing, the full cook, the decode queue and its queue policies, the state machine, GPU activation before upload, the reference defaults, the built-in kinds, verified reads, counters); the Stage 3 acceptance evidence (`Zenith2/Docs/Roadmap.md → "Acceptance evidence"`) needs reimport, clean versus incremental equivalence and missing or corrupt content with their fallbacks |
| after one of its items' stages | S3: 062, 087, 173; S4: 014, 035, 074, 089, 099, 111, 115, 143, 154, 155, 156, 157, 174; S6: 147 | an earlier item's subsequent extension: device recovery admitted within the in-flight limits at Stage 3, catalogue fragments and external references, the archive reader with PACK's Stage 4 archives, streaming stress with STREAM, move detection and worker processes with the Stage 4 pipeline, the budget measurement with the W-Load workload, cooked compatibility with the first shipped release line (Z2-D-048) |

Z2-I-ASSET-04 reimport, Z2-I-ASSET-12 missing assets and Z2-I-ASSET-13 corrupt assets are inventory stage S4 while the Stage 3 acceptance evidence already needs them; core-doc item X113 (pending batch 4) moves the three inventory rows to S3, after which their S3 requirements are no longer "before" the items' stage. This catalogue needs no change when it lands.

## Excluded and deferred

Every ASSET inventory item is classified required, and no ASSET requirement is deferred, excluded, game-side or research. The boundaries that keep the area to its mechanisms are exclusions of other capabilities from ASSET, not exclusions of ASSET items; each is owned elsewhere or rejected by a settled decision:

| Not in ASSET | Rationale | Decision or owner |
|---|---|---|
| the DCC matrix and the glTF, image, audio and FBX importers, presets as documents, override-preserving reimport policy, batch operations, reference repair, importer crash-isolation acceptance, the shared DDC backend and its failure-mode runs, distributed cooking, source-origin provenance | production workflows built on ASSET's mechanisms | CONTENT (Z2-I-CONTENT-01 … -31); D-SEL-FBX (open, Stage 4) |
| the `.zpak` packer and packing policy, patch and DLC manifests, dictionary training and policy per asset class (ASSET keeps the archive reader, Z2-R-ASSET-155 … -157) | packaging | PACK (Z2-I-PACK-01 … -16) |
| mesh, material and texture formats, their cook steps and loaders, texture streaming | rendering content | GEOM |
| scene and prefab documents, their loaders and transactional loading; the authoring-document migration framework and canonical serialization (`ZenithDocument`, `ZenithDocumentHost`), into which ASSET registers the `.meta` type | scene data | SCENE (ruling Q43 (a)) |
| cell streaming, streaming priorities per cell, HLOD | world streaming | STREAM |
| the virtual filesystem, mounts, asynchronous I/O, low-disk detection | platform | PLAT |
| codecs and the section descriptor | compression | COMPRESS (Z2-D-004) |
| Luau bytecode cooking, schema registration, shader compilation | other areas' cook steps the driver runs | SCRIPT, ECS, RPROD (Z2-D-025) |
| the editor's asset browser, import dialogs and migration UI | editor | EDIT, through the cook server |
| profiler drawing and content dashboards | developer tools | DEVT (Z2-D-005) |
| mod content from outside the package, patch, loose and user mounts | modding is out of scope | Z2-D-075 |
| run-time migration of cooked data | cooked data re-cooks | Z2-D-048 |
