# ECS design — tests and benchmarks

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It lists every test ECS owns with the case it asserts, its file, its target and the gate steps that run it, plus the benchmarks and research scenarios. The specification's required test tables are reproduced by name and extended: `Zenith2/Docs/Architecture/ECSModel.md` → "Required tests" (table "ECS conformance tests"), `Zenith2/Docs/Architecture/FramePhases.md` → "Required tests" (table "Phase and authority tests"), `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Required tests", `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" ([Z2-N-ARCH-649]) and "Worlds and isolation" ([Z2-N-ARCH-767]), and `Zenith2/Docs/Contracts.md` → "Handle kinds" (subsection "Handle tests").

Every test follows `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates": doctest, one CTest test per case, pass or fail only, no pinned count, the seed of any randomness printed, 60 s unless in `z2-stress`; test IDs `Test_<Type>_<Behaviour>` map one to one to doctest names `"<Library>/<Type>/<Behaviour>"`; conformance cases register once per backend as `<name>[Custom]`, `<name>[Flecs]`, `<name>[EnTT]` ([Z2-N-ARCH-179]).

## Test files

| Target | Directory | Files |
|---|---|---|
| `ZenithReflect_Tests` | `Zenith2/Engine/ZenithReflect/Tests/` | `Zenith_TypeRegistry_Tests.cpp`, `Zenith_TypeRegistry_DefectTests.cpp`, `Zenith_TypeLayout_Tests.cpp`, `Zenith_FieldSet_Tests.cpp`, `Zenith_ReflectionWalk_Tests.cpp`, `Zenith_Schema_Tests.cpp`, `Zenith_SchemaBlob_Tests.cpp`, `Zenith_SchemaMigrator_Tests.cpp`, `Zenith_LuauTypes_Tests.cpp`, `Fixtures/Schemas/**` (one fixture per catalogue row), `Fixtures/Migration/Stats/v1.json` … `v2.expected.json`, `Examples/Zenith_TypeRegistry_Example.cpp`, `Examples/Zenith_FieldSet_Example.cpp`; fuzz: `Fuzz/Zenith_Fuzz_SchemaBlob.cpp`, `Fuzz/Zenith_Fuzz_SchemaJson.cpp`, `Fuzz/Zenith_Fuzz_FieldSetDecode.cpp` |
| `ZenithECS_Tests` | `Zenith2/Engine/ZenithECS/Tests/` | `Zenith_EntityHandle_Tests.cpp`, `Zenith_EntityTable_Tests.cpp`, `Zenith_ContainerHeap_Tests.cpp`, `Zenith_CommandBuffer_Tests.cpp`, `Zenith_CommandKindTable_Tests.cpp`, `Zenith_SystemRegistry_Tests.cpp` (authority and schedule), `Zenith_EmitInto_Tests.cpp`, `Zenith_WorldRegistry_Tests.cpp`, `Zenith_MergeCapacity_Tests.cpp`, `Zenith_ECS_DefectTests.cpp`, `Support/Zenith_ECSTestSystems.{h,cpp}` (the test systems and fixture components, C++ and schema twins), `Examples/Zenith_ChunkQuery_Example.cpp`, `Examples/Zenith_SystemRegistration_Example.cpp` |
| `ZenithECS_Conformance` | `Zenith2/Engine/ZenithECS/Tests/Conformance/` plus each backend's `Tests/ConformanceAdapter/` | `Zenith_Conformance_Main.cpp` (backend loop), `Zenith_Conformance_Handles.cpp`, `…_Commands.cpp`, `…_Queries.cpp`, `…_ChangeTracking.cpp`, `…_Views.cpp`, `…_Phases.cpp`, `…_Arbitration.cpp`, `…_DataComponents.cpp`, `…_Determinism.cpp`, `…_Factory.cpp`, `Fixtures/DeterminismFixture.{h,cpp}` (the 600-tick fixture world and its recorded input) |
| `ZenithECS_Custom_Tests`, `ZenithECS_Flecs_Tests`, `ZenithECS_EnTT_Tests` | each backend's `Tests/` | backend-internal tests (§ "Backend unit tests") |
| `Zenith_Integration_Determinism` | `Zenith2/Tests/Determinism/` | `Zenith_Worlds_Tests.cpp`, `Zenith_DeterminismReplay_Tests.cpp` (links `ZenithRuntime`, the three backends through three hand-built compositions) |
| `Zenith_Bench_ECS`, `Zenith_Bench_ECSNative`, `Zenith_Bench_ECSWorkload`, `Zenith_Bench_BudgetsECS` | `Zenith2/Benchmarks/ECS/`, `Zenith2/Benchmarks/ECSNative/`, `Zenith2/Benchmarks/ECSWorkload/`, `Zenith2/Benchmarks/Budgets/ECS/` | § "Benchmarks and research scenarios", § "Stage 1 budget harness" |

## Reflect tests

| Test | Asserts |
|---|---|
| `Test_TypeRegistry_EachErrorCodeReported` | one registration per `REFLECT_ERROR_*` code fails `Freeze` with that code, the logged text naming both parties or the missing piece ([Z2-N-ARCH-649]) |
| `Test_TypeRegistry_FrozenRejectsRegistration` | after `Freeze`, a runtime registration returns `REFLECT_ERROR_FROZEN`, a schema registration `SCHEMA_ERROR_REGISTRY_FROZEN`, a second `Freeze` `REFLECT_ERROR_FROZEN` |
| `Test_TypeRegistry_CppRegistrationAfterFreeze_Checks` (suite `z2-death-check`) | `BeginComponent` after `Freeze` outside a window fails `ZENITH_CHECK` ([Z2-N-CONTRACT-378]) |
| `Test_TypeRegistry_ConcurrentReadsAfterFreeze` (`linux-tsan`) | 8 threads × 100,000 `FindType`/`FindTypeByStableName` lookups on a frozen registry report no race and correct results |
| `Test_TypeRegistry_SerializationByPropertyID` | capturing, encoding, decoding and applying a field set never reads bytes outside declared properties; a C++ type with padding bytes set to garbage round-trips identically |
| `Test_TypeRegistry_HooksBalanceReferences` | construct, copy, move and destroy through a type with asset fields leave the counting value-kind hooks balanced |
| `Test_TypeRegistry_DeclarationBeforeStringTable` | engine and module types with `STRING_ID` defaults declared as text before `BindStringTable` register; after binding each default holds the ID `Intern` returns for its text; `Freeze` succeeds and the types equal the same declarations made after binding; a runtime type giving a `STRING_ID` field encoded `m_xDefault` bytes fails with `REFLECT_ERROR_BAD_DEFAULT`, before and after binding |
| `Test_TypeRegistry_BehaviourStateAtStepTen` | `Zenith2/Docs/Design/ECS.md` → "Behaviour-state type registration": two behaviour-state types registered between a schema blob and `Freeze` are published in revision 1; a third registered after `Freeze` outside a window returns `REFLECT_ERROR_FROZEN` |
| `Test_TypeRegistry_BehaviourStateBeforeStringTable_Checks` (suite `z2-death-check`) | `BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, …)` in the DECLARING state (before `BindStringTable`, the step-6 declaration phase) fails `ZENITH_CHECK` |
| `Test_TypeRegistry_ResolveReferenceDefaultsOutOfState_Checks` (suite `z2-death-check`) | `ResolveReferenceDefaults` before `Freeze`, inside a hot-reload window, and a second time for `PROPERTY_TYPE_ASSET` after a first call that failed each fail `ZENITH_CHECK`; the first call after `Freeze` resolves the defaults and leaves the revision unchanged |
| `Test_TypeRegistry_ConfigSchemaExposedDuringDeclaration` | a test module's `RegisterTypes` registers a key through `GetConfigSchema()` and a type through the builder in one call; the key is found in the schema after MOD's freeze of it |
| `Test_TypeRegistry_GetConfigSchemaAfterFreeze_Checks` (suite `z2-death-check`) | `GetConfigSchema` with no attached schema, or after the schema is frozen, fails `ZENITH_CHECK` |
| `Test_TypeRegistry_FreezeBeforeStringTable_Checks` (suite `z2-death-check`) | `Freeze` or a schema registration in the declaration phase fails `ZENITH_CHECK` |
| `Test_TypeRegistry_TypeIDFromStableName` | the ID equals `Zenith_Hash64(stableName)`; the C++ type name never influences it |
| `Test_TypeRegistry_GetTypesInStableNameOrder` | `GetTypes` and `GetTypesOfKind` are in byte order of stable name regardless of registration order |
| `Test_TypeRegistry_HotReloadPublishesRevision` | a window replacing a behaviour-state type publishes revision + 1; the old `Zenith_TypeInfo*` stays readable for two `RetireFrame` calls |
| `Test_TypeRegistry_HotReloadFailureKeepsRevision` | a window with an invalid replacement returns the error from `EndHotReload` and leaves the previous revision in effect ([Z2-N-ARCH-651]); a staged schema type whose asset default the bound resolver does not know returns `REFLECT_ERROR_BAD_DEFAULT` with the schema diagnostic logged |
| `Test_TypeRegistry_HotReloadKindRejected` | a component replacement in a play-time window is `REFLECT_ERROR_HOT_RELOAD_KIND` |
| `Test_TypeRegistry_AbandonDiscardsWindow` | `AbandonHotReload` publishes nothing |
| `Test_TypeRegistry_Create_FailsAtIndex_Unwinds` | fault points of `Create` and `Freeze` unwind with zero leaks ([Z2-N-CONTRACT-341]) |
| `Test_TypeLayout_OrderByAlignmentThenID` | the example schema's fields are placed by alignment descending then ID, offsets and size as computed by hand in the test |
| `Test_TypeLayout_LimitsRejected` | a 1,025-byte component and a depth-5 struct fail |
| `Test_FieldSet_BinaryRoundTrip` | every value type (including strings by text, entity and asset references by persistent ID, containers, nested structs) round-trips through the binary codec |
| `Test_FieldSet_JsonRoundTrip` | the JSON form round-trips and keys by decimal property ID; unknown members survive in position |
| `Test_FieldSet_DecodeRejectsTruncation` | every truncation point of a valid record returns `FIELD_SET_ERROR_TRUNCATED`, never a crash |
| `Test_ReflectionWalk_VisitsInIDOrder` | the walk visits properties and nested struct fields depth first in ascending ID, with correct paths |
| `Test_ReflectionWalk_SchemaAndCppTwinIdentical` | the walk of a C++ component and of its schema twin yields identical visit sequences apart from origin ([Z2-N-ARCH-643]) |
| `Test_ReflectionWalk_WriteClampsRange` | `Zenith_WritePropertyAt` clamps to a declared range |

## Schema tests

The table "Data-defined component tests" of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Required tests", every row, in `Zenith_Schema_Tests.cpp`, `Zenith_SchemaBlob_Tests.cpp` and `Zenith_SchemaMigrator_Tests.cpp` (Reflect rows) and `ZenithECS_Conformance` (conformance rows), plus:

| Test | Asserts |
|---|---|
| `Test_Schema_ValidExample_Registers` | the example registers with the expected layout, IDs and authorities |
| `Test_Schema_EachErrorCode_Reported` | one fixture per row group of `Zenith2/Docs/Design/ECS/Schemas.md` → "Validation rules catalogue" yields the code and the exact message `<file>: <keyPath>: <message>` |
| `Test_Schema_AllErrorsInOneRun` | a file with five distinct errors reports all five, in document order |
| `Test_Schema_ReusedFieldID_Rejected` | a field ID present in `retiredFieldIds` fails |
| `Test_Schema_RetiredListShrink_Rejected` | a retired list smaller than the lock's fails |
| `Test_Schema_Limits_Enforced` | each limit at its value passes and at value + 1 fails |
| `Test_Schema_TypeIDFromStableName` | renaming `component` keeps the type ID; changing `stableName` changes it |
| `Test_Schema_CollisionWithCppType_Rejected` | a forced hash collision with a C++ type fails with `SCHEMA_ERROR_TYPE_ID_COLLISION` |
| `Test_Schema_BlobRoundTrip` | JSON → blob → registry equals JSON → registry; two writes are byte-identical |
| `Test_Schema_CorruptBlob_Rejected` | truncated, oversized-count and bad-magic blobs fail with `SCHEMA_ERROR_BLOB_CORRUPT`, never crash |
| `Test_Schema_UnknownExtensionKeysPreserved` | a rewrite keeps `x-` keys in position byte for byte |
| `Test_Schema_EventForbiddenKeys` | each forbidden key on an event schema is `SCHEMA_ERROR_UNKNOWN_KEY` |
| `Test_Schema_NonNullEntityDefault_Rejected` | an `entity` default other than `null` fails with `SCHEMA_ERROR_BAD_DEFAULT` and the exact message of the catalogue row |
| `Test_Schema_AssetDefaultResolved` | an `asset<K>` default naming an asset of kind K registers with the persistent ID as its declared default; after `ResolveReferenceDefaults` with a counting test resolver, a constructed component's field holds the resolved handle, the counting hooks show one reference per constructed component plus the registry's one, and destroying the components and the registry balances them |
| `Test_Schema_UnresolvableAssetDefault_Rejected` | an `asset<K>` default whose ID the resolver does not know makes `ResolveReferenceDefaults` return `SCHEMA_ERROR_BAD_DEFAULT` with the message `asset default <id> does not name a <K> asset` at `fields[i].default`, and `AreReferenceDefaultsResolved` stays false, so `Zenith_WorldDesc` validation rejects the registry |
| `Test_Schema_MalformedAssetDefault_Rejected` | an `asset<K>` default that is not `null` or the 36-character persistent-ID form fails at registration with `SCHEMA_ERROR_BAD_DEFAULT` |
| `Test_Schema_Migration_ChainAppliesInOrder` | a v1 field set migrates to v3 through each op of the example |
| `Test_Schema_Migration_FailureLeavesInput` | a failing `retype` returns an error and leaves the input unchanged |
| `Test_Schema_Migration_NumericConversions` | saturation, round half to even, NaN → 0 with a warning |
| `Test_Schema_Migration_NewerVersionReadOnly` | data from a newer version is `MIGRATION_ERROR_NEWER_VERSION` |
| `Test_Schema_Migration_UnknownFieldsKept` | fields no step mentions survive with one warning each |
| `Test_Schema_Migration_ScriptOpThroughHost` | a `script` op calls the installed `Zenith_MigrationScriptHost` with the old and new sets; a host error is `MIGRATION_ERROR_SCRIPT` and leaves the input unchanged |
| `Test_Schema_LayoutIndependentSerialization` | adding a field changes the layout and reads old serialized data unchanged |
| `Test_Schema_GameFixtures_Migrate` | per game, every `Schemas/Fixtures/<Component>/v<N>.json` migrates to its `.expected.json` ([Z2-N-ARCH-606]) |
| `Test_LuauTypes_ComponentsDeterministic` | two emissions from one registry are byte-identical; types in stable-name order, fields in ID order |
| `Test_LuauTypes_SchemaAndCppTwinIdentical` | the view classes of a C++ component and its schema twin are identical |
| `Test_DataComponent_IdenticalToCppTwin` (conformance) | per backend, the schema twin and the C++ twin give identical query, command, serialization and change-tracking results |
| `Test_DataComponent_AssetFieldRefCounted` (conformance) | per backend, copying and destroying entities with an `asset<>` field leaves the reference count balanced |
| `Test_DataComponent_ContainerFieldReleased` (conformance) | per backend, destroying entities with a `container<>` field returns every block |
| `Test_DataComponent_AuthorityEnforced` | a schema field's authority produces the same schedule errors as a C++ field's |

## ECS unit tests

| Test | Asserts |
|---|---|
| `Test_EntityHandle_NullDoesNotResolve` | the null handle resolves to `ECS_ERROR_NULL_HANDLE` in every configuration ([Z2-N-CONTRACT-096]) |
| `Test_EntityHandle_StaleAfterDestroy_DoesNotResolve` | a destroyed entity's handle resolves until the end of its tick and is stale afterwards |
| `Test_EntityHandle_ForeignOwner_DoesNotResolve` | a handle of world A resolved in world B is `ECS_ERROR_WRONG_WORLD` |
| `Test_EntityHandle_GenerationExhaustion_RetiresSlot` | a table created with initial generation 2^28 − 2 retires the slot after one destroy ([Z2-N-CONTRACT-100]) |
| `Test_ComponentHandle_NullDoesNotResolve`, `_StaleAfterDestroy_DoesNotResolve`, `_ForeignOwner_DoesNotResolve` | as the entity forms, plus a removed component is `ECS_ERROR_NO_COMPONENT` |
| `Test_EntityTable_LowestFreeSlotFirst` | allocations after frees take the lowest free slot |
| `Test_EntityTable_ChildrenInSimulationIDOrder` | children are listed in ascending simulation ID |
| `Test_EntityTable_Create_FailsAtSlots_Unwinds` (and `…FreeBits…`, `…PersistentMap…`) | each fault point unwinds |
| `Test_ContainerHeap_MatchesOracle` | 16 seeds × 10,000 operations against a map oracle ([Z2-N-CONTRACT-288]) |
| `Test_ContainerHeap_ReservationNeverFailsAllocation` | every allocation within a reservation succeeds |
| `Test_CommandBuffer_RecordLayout` | every record kind's bytes are exactly the record format table's |
| `Test_CommandBuffer_OverCapDropped` | a record beyond the cap is dropped, counted and logged, never an assert |
| `Test_CommandKindTable_RegisterValidation` | each rejection of `Register` returns its code |
| `Test_CommandKindTable_PlayLastFlag` | `Register` with `m_uFlags = COMMAND_KIND_FLAG_PLAY_LAST` succeeds and `Find` returns the flag; a kind registered with `m_uFlags` 0 reports 0; failure case: `m_uFlags = 1u << 1` returns `ECS_ERROR_INVALID_DESC` and `GetCount()` is unchanged |
| `Test_EntityHandle_ReflectValueTraits` | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_EntityHandle header": the traits `static_assert`s (`bSUPPORTED`, `PROPERTY_TYPE_ENTITY`, no lifetime, a zero `ElementType()`); a trivially copyable C++ component with a `Zenith_EntityHandle` member registers through `Property` and freezes, its property reports `PROPERTY_TYPE_ENTITY`, size 8, alignment 8, no `PROPERTY_FLAG_CARRIES_LIFETIME`, null hooks, and its default object's 8 bytes are zero; failure case: a 4-byte `Default` on that property fails `Freeze` with `REFLECT_ERROR_BAD_DEFAULT` |
| `Test_MergeCapacity_FirstFit` | a rejected demand is not subtracted and a subsequent smaller one is accepted ([Z2-N-SCRIPT-333]) |
| `Test_WorldRegistry_RoundRobin` | tags are issued round-robin skipping live tags |
| `Test_WorldRegistry_TagsExhausted` | the 4,096th live world fails with `ECS_ERROR_WORLD_TAGS_EXHAUSTED` before any factory is called |
| `Test_WorldRegistry_InitialGenerationAboveReleased` | reissuing a tag gives an initial generation one above the released highest generation, wrapping to 1 |
| `Test_WorldRegistry_FirstIssueOffset` | a never-issued tag's initial generation is `1 + ((counter × 65,537) mod 2^27)` |
| `Test_Query_CompileValidation` | every rejection of `Zenith2/Docs/Design/ECS/Queries.md` → "Query compilation" |
| `Test_Query_OwnRowOnlyMarking` | columns written in the phase are marked own-row-only |
| `Test_SystemRegistry_UnschedulablePhase_Rejected` | `AddSystem` with `PHASE_RENDER_EXTRACTION`, `PHASE_COMMANDS_ONLY` and the value 13 each records `ECS_ERROR_PHASE_NOT_SCHEDULABLE` in `GetRejectedRegistrations`, logs the exact message, creates no registration index (the next accepted system takes the index the rejected one would have), and a chained `Write` on the returned builder records nothing; `PHASE_SAVE_SNAPSHOT` is accepted |
| `Test_SystemRegistry_MaxSystemsAndMergePointBound` | `Zenith_SystemRegistry::Create` with `m_uMaxSystems` 0 or 65,537 returns `ECS_ERROR_INVALID_DESC`, with 1 and 65,536 it succeeds; a registry of 64 systems (8 of them writer sources of 4 arbitrated properties) freezes with M equal to the hand count (merge points + arbitration nodes + 11); with the limit lowered to that M through `Zenith_WorldTestAccess::SetMergePointLimit`, the simulation's next freeze fails, logs `ECS_ERROR_CAPACITY` with the exact message of `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 1 and returns one `SCHEDULE_ERROR_GRAPH` diagnostic |
| `Test_ECSScriptTraits_RoundTrip` | § "Zenith_ECSScriptTraits header" of `Zenith2/Docs/Design/ECS/WorldAPI.md`: null, live and ticket handles round-trip bit for bit; a NUMBER, a NIL and an ASSET value return the error status with code 1511 and leave the output unchanged |
| `Test_ECS_ConfigKeysMapToWorldDesc` | each configuration key of `Zenith2/Docs/Design/ECS.md` → "Configuration keys" is registered with its type, range and default; setting `ecs.maxEntitiesPerWorld`, `ecs.containerHeapBytes` and `ecs.chunkCapacityRows` to in-range values yields a `Zenith_WorldDesc` with those values; each out-of-range value is rejected by the configuration store with the key named; `ecs.forceCompaction` compacts at every sync point; `ecs.debugDestroyEvents` makes world destruction emit one `STRUCTURAL_EVENT_DESTROYED` per live entity in ascending simulation ID and none when false |
| `Test_EmitInto_NodeNamesHaveNoAt` | no declared node contains `@`; names match the naming table |
| `Test_EmitInto_OneTickBoundaryAtStageZero` | exactly one tick-boundary node per world, alone at stage 0 |
| `Test_EmitInto_StagingResourcesTransient` | arbitration staging resources are declared non-persistent |
| `Test_EmitInto_MergeChainInStageOrder` | merge nodes of one phase are chained in the stage order of `Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points" (registration index as the tie-break, so independent systems chain in registration order) and each writes its system's columns; a merge-node reader registered before the merge-node writer of a property it reads is chained after the writer, and the graph compiles with no cycle |
| `Test_EmitInto_MergeEnumerationTopological` | `Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points": one phase with a pre-reader declared `Before` its writer, a writing reader without a merge node registered before its writer, an arbitrated property with two writer sources and a three-node merge chain whose first-registered system reads a property the last-registered one's merge commits (so the chain runs against registration order) yields the hand-computed stage order, merge ordinals and read points, identical under two different cost-hint sets |
| `Test_EmitInto_ExtractorsBeforeSeal` | every extractor and no other epilogue stage declares `Before("Render.Seal@W0")` |
| `Test_EmitInto_DeclarationHashStable` | identical registrations give identical declaration hashes and identical builder contents |

## Conformance suite

Every row runs on all three backends in `ZenithECS_Conformance` ([Z2-N-ARCH-714], [Z2-N-ARCH-178]). The first twenty-one rows are the specification's table "ECS conformance tests" verbatim; the rows after them are added by this design.

| Test | Asserts |
|---|---|
| `Test_ECS_SpawnVisibleNextPhase` | a spawn recorded in phase p is absent from phase p's queries and present in phase p+1's |
| `Test_ECS_DestroyAtTickEnd` | a destroyed entity resolves until the end of its tick and is stale in the next |
| `Test_ECS_HandleGenerationRetire` | a slot at the maximum generation is retired and never reissued |
| `Test_ECS_ForeignWorldHandle_Fails` | a handle resolved in another world fails with the wrong-world error |
| `Test_ECS_MergeOrderIndependentOfCompletion` | shuffled completion order of 64 invocations yields identical playback |
| `Test_ECS_SpawnIDsInMergeOrder` | spawn handles and simulation IDs are identical with 1, 2, 8 and all workers |
| `Test_ECS_EpochBumpedOnlyByStructure` | property commands and commits leave epochs unchanged; a spawn into a chunk bumps it |
| `Test_ECS_ChangeVersionAdvancesAtMerge` | a written column's version equals the merge's version; a failed write advances nothing |
| `Test_ECS_CommitRowChangesAdvancesVersionOnce` | a merge committing two entities' bits sets the column version to the merge's version once; a rejected entity's bits leave no trace |
| `Test_ECS_ChangedQueryFilter` | a `QUERY_TERM_CHANGED` query returns exactly the chunks whose declared properties changed |
| `Test_ECS_QueryStableWithinPhase` | a query's chunk list does not change during a phase even when commands are recorded |
| `Test_ECS_ViewInvalidAfterInvocation` | a view kept past its invocation reports invalid |
| `Test_ECS_ViewInvalidAfterStructuralChange` | a view of a chunk whose epoch changed reports invalid |
| `Test_ECS_UndeclaredColumnIsNull` | an undeclared property's column is null (assert disabled) |
| `Test_ECS_DisjointColumnWritesConcurrent` | two systems writing disjoint properties of one component run concurrently and both commit |
| `Test_ECS_PerRowRestore` | restoring one row of a snapshotted chunk leaves the other rows' writes intact |
| `Test_ECS_FixedOrderReduction` | a float sum over 100,000 contributions is bit-identical with 1, 2, 8 and all workers |
| `Test_ECS_DeterministicReplay` | a 600-tick replay of the determinism fixture gives identical tick hashes across workers, compaction and capacities |
| `Test_ECS_DeterministicReplayWithFailures` | the same replay with injected script errors, budget exhaustion, snapshot-cap overflow and capacity rejection gives identical tick hashes |
| `Test_ECS_ContainerHeapExhaustion` | a spawn needing container storage beyond the heap is rejected at merge with a diagnostic, never an assert |
| `Test_ECS_FactoryFailureUnwinds` | each injected construction failure of each backend's factory returns an error and leaks nothing |
| `Test_ECS_MergeVersionsIndependentOfWorkers` | column and row versions after 60 ticks are identical with 1, 2, 8 and all workers |
| `Test_ECS_WorldHashLayoutIndependent` | one world hashed under both capacities and forced compaction gives one hash |
| `Test_ECS_CommandsOverwriteLastInMergeOrder` | two `SetProperty` on one property resolve to the greater merge key and count one overwrite |
| `Test_ECS_AddRemoveSamePlayback` | add then remove in one playback leaves the component absent; remove of an absent component is a counted no-op; a `SetProperty` by handle of a component that an earlier `ADD_COMPONENT` of the same playback adds is dropped with `ECS_ERROR_NO_COMPONENT` logged, the component keeping the addition's values (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Steps") |
| `Test_ECS_DestroyCascadesChildren` | destroying a parent marks and removes its hierarchy depth first in simulation-ID order |
| `Test_ECS_AddOnPendingDestroyDropped` | `AddComponent` on a pending-destroy entity is dropped with `ecs.commands_on_destroyed_delta`; `SetProperty` applies |
| `Test_ECS_RejectedSpawnConsumesNoID` | a spawn rejected at merge consumes no simulation ID; the next accepted spawn takes it |
| `Test_ECS_TicketHandleResolves` | a `SetProperty` on a ticket handle applies to the spawned entity; on a rejected spawn it is dropped with `ecs.commands_stale_delta` |
| `Test_ECS_EventsDeliveredInMergeOrder` | events from three systems arrive in merge order in phase 9 |
| `Test_ECS_ReductionTargetWritten` | a reduction's result lands on its singleton at the merge version |
| `Test_ECS_OutOfTickPlayback` | `ApplyOutOfTick` applies spawns and property sets with the given simulation IDs and refreshes queries |
| `Test_ECS_ContainerFieldCopyAllocatesNewBlock` | copying an entity with a container field allocates a new block; destroying either frees only its own |
| `Test_ECS_InspectorOrder` | the inspector lists entities by simulation ID and components by stable name |
| `Test_ECS_PausedWorldRunsNothing` | a paused world's systems do not run and its tick index does not advance |
| `Test_ECS_ArbitrationStagingPerSource` | a writer's view of an arbitrated property addresses its source's staging column |
| `Test_ECS_ModuleCommandsInMergeOrder` | module command kinds recorded by three systems play back at step 7 in merge order through their playback functions |
| `Test_ECS_StructuralObserverOrder` | a registered observer receives spawned, destroy-pending and destroyed events in merge order and ascending simulation ID; its follow-up commands apply at the next sync point |
| `Test_ECS_TickBoundaryStepOrder` | handlers are called for steps 1, 2 and 3 in registration order before the tick index advances and before any phase-1 node |
| `Test_ECS_ReadOnlyColumnsInViews` | string, entity, asset and persistent-ID columns are returned non-writable even when declared written; container properties have no column |
| `Test_ECS_RowChangeVersions` | `GetRowChangeVersions` holds the merge version of each row's last committed write, and a spawned row reads as changed |
| `Test_ECS_SchemaRevisionMigratesRows` | an edit-world revision of a schema type migrates every stored row through the chain, keeps handles and simulation IDs, and bumps epochs |
| `Test_ECS_CountersPublished` | every counter of `Zenith2/Docs/Design/ECS.md` → "Counters" is published with the world-tag suffix, its kind in `eKind` and at its row's publication point: each per-tick row once per tick per world after the end-of-tick playback, `ecs.version_sweeps` once per sweep, `ecs.buffers_discarded_on_cancel` only at the tick boundary after a cancelled frame, the two `world.` rows once at world destruction |
| `Test_ECS_DumpSchema` | `ecs.dump` writes a `zenith2.ecs-dump` version 1 document with every archetype, query and system, byte-identical for identical worlds |
| `Test_ChunkView_CommitRejectsStaleScope` | `CommitRowChanges` with another scope returns `ECS_ERROR_STALE_HANDLE` and changes nothing |
| `Test_Query_ChunkOrderUnobservable` | tick hashes are identical under both capacities and forced compaction |
| `Test_ECS_PlaybackWritesCommittedOncePerChunkColumn` | in one sync point, two properties of chunk A (one of them by two commands on two rows) and one property of chunk B produce three commits, one per written chunk column; written rows and columns carry the sync point's merge version and untouched rows keep theirs |
| `Test_ECS_TickBoundaryPropertyWriteCommitted` | a property written through `Zenith_TickBoundaryAccess` at step 2 is committed at V(t, 0) and matched by a phase-1 `QUERY_TERM_CHANGED` query |
| `Test_ECS_PausedWorldPublishesNothing` | a paused world's tick boundary calls no handler; a result queued while paused publishes at the first boundary after resuming |
| `Test_ECS_ExtractorQueryFreshAfterCatchUp` | in a frame of three catch-up ticks with a spawn in the last tick's phase 10, the extractor's query sees the spawned entity in that frame's epilogue |
| `Test_ECS_WorldLevelViewCommitDisabled` | a view from `Zenith_World::GetChunk` returns every column non-writable and its `CommitRowChanges` returns `ECS_ERROR_UNKNOWN_PROPERTY` with no version change; the same chunk through `Zenith_SystemContext::GetChunk` carries the invocation's merge version |
| `Test_ECS_OutOfTickStampsChangeVersion` | an `ApplyOutOfTick` property set and spawn between ticks t and t+1 stamp `GetChangeVersion()` = V(t, M), and every `QUERY_TERM_CHANGED` system of tick t+1 sees both on its first invocation |
| `Test_ECS_VersionZeroNeverIssued` | across U = 2^32 − 3 … 2^32 + 1 the issued versions are 2^32 − 2, 2^32 − 1, 1, 2, 3 and nothing is ever stamped 0 |
| `Test_ECS_VersionWrapSweep` | `Zenith2/Docs/Design/ECS/Queries.md` → "Version wrap sweep": 16 seeds, 64 advances of 2^27 to 2^29 merge points (more than 2^33 in all) each followed by a real tick, one system disabled across more than 2^30 + 2^29 + M merge points and re-enabled; at every checkpoint no chunk the 64-bit oracle matches is missed, every slot equals the oracle except the re-enabled system's first invocation, which matches every chunk, no stored version is beyond the bound, and `ecs.version_sweeps` equals the number of 2^29 boundaries crossed, with no sweep where none was crossed |
| `Test_ECS_ColumnAddressStableUntilMerge` | a system records every column pointer of every chunk it writes; while other systems of the phase record structural commands, the pointers at its merge (read back through `Zenith_MergeContext::GetWrittenViews`) are identical, on every backend including an EnTT view query |
| `Test_ECS_ChangedQueryPreReaderSeesWriteNextTick` | `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries": a reader declared `Before` a same-stage writer, with and without a merge point of its own, matches nothing in the tick of the write and exactly the written chunk in the next tick |
| `Test_ECS_ChangedQueryNoSpuriousMatchAfterSameStageWriter` | a writing reader without a merge node, registered before a same-stage writer it runs after, matches the written chunk in the tick of the write and not in the next tick unless written again |
| `Test_ECS_ChangedQueryIgnoresOwnWrite` | a system that writes the property it filters on does not re-see its own in-place writes; a command write or another writer's write of that property is seen; an arbitrated writer sees the arbitration node's commit |
| `Test_ECS_TicketPropertyCommandStampsVersion` | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Property writes during structural playback": a spawn, an `AddComponent` on its ticket and a `SetProperty` of the added component on the ticket in one buffer leave the set value, a row and column version equal to the sync point's merge version, and a match for a next-phase `QUERY_TERM_CHANGED` query |
| `Test_ECS_RedundantAddStampsVersion` | an `AddComponent` of a component the entity already has sets its values, counts `ecs.commands_redundant_delta`, stamps the row and column with the sync point's merge version, changes no epoch and is matched by a next-phase `QUERY_TERM_CHANGED` query; a `SetProperty` of the same property with a greater merge key than the addition is overridden by the addition's value (step 6 after step 5) and counted in `ecs.command_overwrites_delta` |
| `Test_ECS_RestoreResetsChangeTracking` | `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 2: after `RestoreSimulationState` to an earlier tick and an out-of-tick load, every `QUERY_TERM_CHANGED` system matches every chunk in the first tick and only chunks written since in the second; restoring from past a sweep boundary to below it, an advance across the next boundary above the restored counter runs exactly one sweep |
| `Test_ECS_CancelledFrameConsistent` | `Zenith2/Docs/Design/ECS.md` → "Cancellation": a phase-4 system requests cancellation from its body; every subsequent ECS node of the instance completes without its body; the entity table, epochs and every query cache agree with storage (`Zenith_WorldTestAccess::RefreshAllQueries` changes no binding); the next frame's tick boundary discards the stale commit list and logs its count; and the tick hash after the next completed tick equals a reference run in which the cancelled tick's phase-4 merge accepted nothing. Second case (item 6): a phase-4 `QUERY_TERM_CHANGED(P)` system N with no merge node reads a property a phase-4 merge node commits and completes before the cancelling system runs (which declares `After('<N>')`); a `SetProperty` of P applied with `ApplyOutOfTick` between the cancelled frame and the next is stamped V(t, M) of the cancelled tick t, and N matches its chunk in the next tick |
| `Test_ECS_ChangedQuerySeesMovedRow` | `Zenith2/Docs/Design/ECS/Backends.md` → "Change tables" (relocation rule), under both chunk capacities: in phase p a `SetProperty` of P on entity e and an `AddComponent` on e, then a phase p + 1 `QUERY_TERM_CHANGED(P)` system whose last-seen version is older than sync point p's matches e's destination chunk; a phase-6 in-place write of P on a row that forced end-of-tick compaction moves into a lower chunk is matched by a phase-2 `CHANGED(P)` system in the next tick; a chunk created by a relocation holds its first row's versions, never 0; a moved row whose version is not newer than the destination column's adds no match |
| `Test_ECS_ChangedQueryReaderRegisteredBeforeWriter` | `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries": a merge-node reader registered before the merge-node writer of the property it reads compiles with no cycle, is chained after the writer, matches the written chunk exactly in the tick of the write and not in the next; capacity is accepted in chain order and playback runs in merge-key order |
| `Test_ECS_RestoreThenContinueMatchesUninterrupted` | `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 2 ("Resumed replay"): a fixture with three row-filtered `QUERY_TERM_CHANGED` systems, property writers and structural churn and no carried events, captured at tick 300 through `Zenith_WorldInspector` and field sets and restored into a fresh world with `RestoreSimulationState` and `ApplyOutOfTick`, gives per-tick hashes for ticks 301 … 600 equal to the uninterrupted run's, under both chunk capacities |
| `Test_ECS_PreviousReadVersion` | `Zenith_SystemContext::GetPreviousReadVersion()` is 0 in a system's first invocation, equals the read version of its previous completed invocation afterwards (the same value in the node and in every leaf), is unchanged across a disabled tick, and is 0 in the first invocation after `RestoreSimulationState` and after a sweep that reset the system (disabled across more than 2^30 + 2^29 + M merge points, advanced with `AdvanceMergeCounter`); a row-level filter comparing `GetRowChangeVersions` against it selects exactly the rows committed since its previous invocation, and every row when it is 0 |

The phase and authority table of `Zenith2/Docs/Architecture/FramePhases.md` → "Required tests" (every row) is implemented in `Zenith_Conformance_Phases.cpp` and `Zenith_Conformance_Arbitration.cpp` for the rows marked conformance and in `ZenithECS/Tests/Zenith_SystemRegistry_Tests.cpp` for the others ([Z2-N-ARCH-500]); `Test_Schedule_ErrorsCheckedInShipping` runs in the Shipping test build of `ZenithECS_Tests`. Added to that table:

| Test | Asserts |
|---|---|
| `Test_Schedule_CrossEntityReadOfWrittenColumn_IsError` | a `ReadAnyRow` of a property written in the same phase fails with `SCHEDULE_ERROR_CROSS_ENTITY_READ` and its exact message |
| `Test_Schedule_ScriptChunkSystemPhase_IsError` | a chunk system declaring phase 5 fails with `SCHEDULE_ERROR_SCRIPT_PHASE` |
| `Test_Schedule_InvalidName_IsError` | a name containing `@` fails with `SCHEDULE_ERROR_INVALID_NAME` |
| `Test_Schedule_EpilogueStageComponentWrite_IsError` | an epilogue stage writing a component is `SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE` |
| `Test_Schedule_QueryAccessMismatch_IsError` | a query writing a property the system reads is `SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH` |
| `Test_Schedule_ReloadFailureKeepsDeclarations` | a reload scope whose commit fails leaves the previous declarations and schedule |
| `Test_Schedule_DisableFromNextTickBoundary` | `SetEnabled(false)` in tick t leaves tick t unchanged and skips the node from t+1 with zero recompiles |
| `Test_Schedule_ResourceAuthority_IsError` | a resource written from a phase other than its owning phase fails with `SCHEDULE_ERROR_RESOURCE_AUTHORITY`; a phase-11 system writing a resource whose type owns phase 11 (a snapshot buffer) compiles; a resource type declaring owning phase 12 fails `Freeze` of the type registry with `REFLECT_ERROR_BAD_AUTHORITY`; `AddWorldResource` with an `eOwningPhase` other than the type's fails with `SCHEDULE_ERROR_RESOURCE_AUTHORITY` and the exact mismatch message; an epilogue sole-consumer drain compiles |
| `Test_Schedule_WriterSourceRules_AreErrors` | an arbitrated writer without a source fails with `SCHEDULE_ERROR_WRITER_SOURCE_MISSING`; two writers with one source with `SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE` |
| `Test_Schedule_BehaviourPhase_IsError` | a behaviours system declaring phase 5 fails with `SCHEDULE_ERROR_BEHAVIOUR_PHASE` |
| `Test_Schedule_OrderingErrors` | a required `After` naming no system fails with `SCHEDULE_ERROR_ORDERING_TARGET_MISSING`; a backward `Before` and a forward `After` fail with `SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE` and their two messages; `AfterIfPresent` on an absent name compiles |
| `Test_SystemRegistry_RegistrationIndexIsOrder` | registration indices equal `AddSystem` call order and are the merge keys' system order |
| `Test_SystemContext_LeafContextOwnArena` (`linux-tsan`) | 8 leaves of one system allocate buffers and views through `ForLeaf` concurrently with no race; the merge finds every buffer |
| `Test_SystemContext_DeclaredResourceReturned` | a declared world resource is returned; an undeclared one returns null in a build with asserts disabled |
| `Test_ECS_CapacityKeysRegisteredSimulationAffecting` | the four capacity keys are registered as simulation-affecting configuration |
| `Test_ECS_EditWorldFlagReported` | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "The edit-world flag": a world created with `m_bEditWorld` true reports `IsEditWorld()` true before its first tick, after 3 ticks and after `ForceCompaction`; a world from a default desc reports false; two worlds of one seed and script that differ only in the flag give equal `HashTickState` streams for 3 ticks; failure case: a desc with the flag set and `m_uWorldTag` 0 fails with `ECS_ERROR_INVALID_DESC` and creates no world |
| `Test_ECS_RewriteContainerInPlace` | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "In-place container rewrite": a phase-3 module command whose playback function (step 7) reads `Conformance.Refs` `items` (4 `u32` elements) with `ReadContainer` and rewrites them with `RewriteContainer` leaves the cell's block, the container heap's used bytes and `GetLargestFreeBlockBytes()` unchanged, a phase-4 reader reads the new 4 values, the row's change version is sync point 3's merge version and a phase-4 `QUERY_TERM_CHANGED(items)` system matches the chunk; the same call with `xElements` equal to the block's own span succeeds and changes no value; on a heap filled so that `GetLargestFreeBlockBytes()` is 0 the rewrite still succeeds; failure cases: 3 elements → `ECS_ERROR_VALUE_SIZE`, the `container<asset<Texture>>` field `icons` of the schema fixture `Conformance.AssetList` → `ECS_ERROR_UNKNOWN_PROPERTY` with the exact `ecs.playback` line `ecs: RewriteContainer refused Conformance.AssetList.icons entity <simID>: element type carries lifetime`, the `Conformance.Refs` `string` field `label` → `ECS_ERROR_UNKNOWN_PROPERTY` with `: not a container`, each changing no value, version or heap counter |
| `Test_ECS_TickBoundarySetContainer` | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Tick-boundary container writes": a tick-boundary handler at step 2 replaces `items` 4 → 6 elements; the phase-1 reader reads 6, the row's version is V(t, 0), a phase-1 `QUERY_TERM_CHANGED(items)` matches, and after the end-of-tick flush the used bytes equal one 6-element block; failure cases: on a heap fragmented to 16-byte free blocks a 16-element write returns `ECS_ERROR_CAPACITY` with the exact WARNING line, the row keeps its value and version and a following `Reserve` of the largest free block succeeds (no reservation outstanding); a stale handle returns `ECS_ERROR_STALE_HANDLE` and a 5-element span declared as 6 returns `ECS_ERROR_VALUE_SIZE`, each reserving nothing; no assert fires |
| `Test_ECS_EventsPerInvocationOrdinal` | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "The current logical invocation": a phase-5 system emits 2 events through `Events(0, 1)` and 3 through `Events(0, 2)` and its custom merge rejects the ordinal-2 buffer; the phase-9 reader sees exactly the 2 ordinal-1 events in merge order and `ecs.capacity_rejections_delta` reads 1; `Events(sim, 0)` and `Events(sim)` return the same writer; after `Events(sim of e1, 3)` the system's `GetChunkOfEntity` of e1 returns its own-row-only column non-null and of e2 a null column, and after `Events(0, 4)` the next `GetChunkOfEntity` entity becomes current |
| `Test_ECS_PlayLastKindsPlayAfterOtherRecords` | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Play-last command kinds": in one phase-4 sync point, ordinary kind K1 and play-last kind K2 recorded as A: K2 then K1, B: K1, C: K2 (registration order A, B, C) play K1(A), K1(B), K2(A), K2(C) into the log, identical with 1, 2, 8 and all workers; an out-of-tick buffer K2, K1 plays K1 first; the played-back command log lists the records in that played order |
| `Test_ECS_PlaybackVersionPerPlayback` | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Playback version": a test kind's playback function records `GetPlaybackVersion()`; records of phases 3 and 4 of one tick report the two sync points' merge versions, distinct and each equal to `GetChangeVersion()` read by a system of the following phase; two records of one sync point report one value; two out-of-tick playbacks between the same two ticks both report `GetChangeVersion()`; between ticks `Zenith_WorldTestAccess::Playback(World()).GetPlaybackVersion()` is 0 |

## Determinism suite

The determinism tests run the same recorded replay under every combination of `Zenith2/Docs/Scripting.md` → "Determinism test matrix" and assert bit-identical per-tick world-state hashes per backend ([Z2-N-ARCH-715], [Z2-N-ARCH-886]):

| Axis | Values |
|---|---|
| worker count | 1, 2, 8, all hardware threads |
| compaction | none; forced full compaction at every sync point |
| chunk capacity | the backend default; `uECS_ALTERNATE_TEST_CHUNK_ROWS` = 13 rows (for every backend: 13 is coprime to 256 and below every default capacity, so no chunk boundary coincides until row 3,328) |
| backend | Custom, flecs, EnTT (compared within a backend only, [Z2-N-ARCH-882]) |

16 runs per backend. The fixture (`Fixtures/DeterminismFixture`) is a world of 20,000 entities over 12 archetypes with native systems in every phase 1 … 10 that spawn, destroy, add and remove components, set properties by command, emit events, contribute to reductions and write arbitrated properties from three sources, and a row-filtered change system in phases 2 and 7 — a `QUERY_TERM_CHANGED` query over properties the other systems write in place, by command and at arbitration, whose rows it filters by `GetRowChangeVersions` against `GetPreviousReadVersion()` and counts into a per-entity counter property it writes — so that the capacity and compaction axes exercise change filtering: a change missed in one layout changes that counter and the hash (`Zenith2/Docs/Design/ECS/Backends.md` → "Change tables"; `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"); its input is a recorded 600-tick command stream. A hash difference reports the first differing tick, entity and property ([Z2-N-ARCH-887]). `Test_ECS_DeterministicReplay` and `Test_ECS_DeterministicReplayWithFailures` are the conformance form; `Zenith_Integration_Determinism` repeats the matrix with `ZenithRuntime`, the Luau fixture scripts of SCRIPT's corpus and the fault matrix (`Zenith2/Docs/Scripting.md` → "Fault-matrix tests"); SCRIPT's `Test_Script_Determinism_ReplayMatrix` is SCRIPT's own and runs on the same fixture.

| Test (`Zenith2/Tests/Determinism/`) | Asserts |
|---|---|
| `Test_Worlds_TwoWorldsTickIndependently` | two worlds of the same content with different pause and time-scale settings run concurrently for 600 ticks, and each world's tick hashes equal a solo run of that world ([Z2-N-ARCH-767]) |
| `Test_Worlds_FailureInOneWorldIsolated` | a script failure and a merge rejection in world A leave world B's hashes equal to its solo run ([Z2-N-ARCH-764]) |
| `Test_Worlds_NoHandleCrossesWorlds` | every handle, view and command buffer of world A fails in world B with the wrong-world error ([Z2-N-ARCH-761]) |
| `Test_DeterminismReplay_RuntimeMatrix` | the runtime-level replay matrix above, per backend |

## Fuzz targets

| Target | Input | Oracle |
|---|---|---|
| `Zenith_Fuzz_SchemaBlob` | arbitrary bytes into `RegisterSchemaBlob` | never crashes, never asserts; either registers or returns `SCHEMA_ERROR_BLOB_CORRUPT` or a rule's code |
| `Zenith_Fuzz_SchemaJson` | arbitrary bytes into `RegisterSchemaJson` | as above with any schema code |
| `Zenith_Fuzz_FieldSetDecode` | arbitrary bytes into `Zenith_DecodeFieldSet` | never crashes; `Zenith_EncodeFieldSet` of a decoded set decodes to the same set |

Each runs as a 60-second smoke in `win-asan` ([Z2-N-ARCH-952]) and for its full duration in stage acceptance.

## Death tests

In `ZenithECS_Tests` suite `z2-death-assert` (presets with asserts): `Test_RawColumn_MarkRowChangedNotWritable_Asserts`, `Test_RawColumn_MarkRowOutOfRange_Asserts`, `Test_SystemContext_UndeclaredResource_Asserts`, `Test_ChunkView_InvalidViewUse_Asserts`, `Test_EventWriter_UndeclaredEvent_Asserts`. In suite `z2-death-assert` also: `Test_PlaybackWrites_StructuralCallInsideSet_Asserts` (a structural playback call between `BeginPlaybackWrites` and `EndPlaybackWrites`) and `Test_PlaybackWrites_NoPlaybackVersion_Asserts` (a `CreateEntity`, or a `SetProperty` outside a write set, while the playback version is 0). In suite `z2-death-check` (every preset): `Test_SystemRegistry_AddAfterFreeze_Checks`, `Test_CommandKindTable_RegisterAfterSeal_Checks`, `Test_SystemReloadScope_UnschedulablePhase_Checks` (a reload scope's `AddSystem` or `ReplaceSystem` with phase 12); the Reflect death checks `Test_TypeRegistry_CppRegistrationAfterFreeze_Checks`, `Test_TypeRegistry_GetConfigSchemaAfterFreeze_Checks`, `Test_TypeRegistry_FreezeBeforeStringTable_Checks`, `Test_TypeRegistry_BehaviourStateBeforeStringTable_Checks` and `Test_TypeRegistry_ResolveReferenceDefaultsOutOfState_Checks` run in `ZenithReflect_Tests`' suite `z2-death-check` (§ "Reflect tests"). Each asserts the report prefix of [Z2-N-CONTRACT-470].

<!-- elaboration -->
Registration: every case above is excluded from doctest discovery by the test main and registered as its own CTest entry in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` with one `zenith_core_death_case(<target> <suite> <case>)` line (the ZenithTestSupport registry function, [Z2-N-ARCH-179]), written in the diff of the ticket that adds the case. Each death `TEST_CASE` is named exactly by its `Test_*` name, so `<case>` selects it. The pass condition is the suite's: `z2-death-check` matches `ZENITH (CHECK|FATAL|UNREACHABLE) FAILED` in every configuration, `z2-death-assert` matches `ZENITH ASSERT FAILED` in the asserts-enabled configurations only. A death case that needs a running world is a conformance case of `ZenithECS_Conformance` (the ticket that adds it moves it there in this section), registered once per backend as `<case>[Custom]`, `<case>[Flecs]` and `<case>[EnTT]`.

## Backend unit tests

| Target | Tests |
|---|---|
| `ZenithECS_Custom_Tests` | `Test_CustomChunks_CapacityFromRowBytes` (the worked sizes of `Zenith2/Docs/Design/ECS/Backends.md` → "Chunk layout"), `Test_CustomChunks_SwapRemoveMovesVersions`, `Test_CustomArchetypes_EdgeCache`, `Test_CustomCompaction_PacksAndBumpsEpochs` |
| `ZenithECS_Flecs_Tests` | `Test_FlecsTypes_HooksForwarded` (ctor, dtor, copy, move reach the Zenith hooks), `Test_FlecsSlices_SliceEpochs`, `Test_FlecsSlices_MirroredRowMoves` (change tables follow flecs's swap-remove), `Test_FlecsOsApi_AllocationsFromRootHeap` (every flecs world allocation counted by the root heap and by no other heap; a desc naming another heap rejected, the flecs exception of Z2-R-ECS-137) |
| `ZenithECS_EnTT_Tests` | `Test_EnTTAdapter_SizeClasses`, `Test_EnTTAdapter_HooksAroundEmplaceErase`, `Test_EnTTGroups_PackedPrefix` (owned pools keep group members at [0, K) in one order), `Test_EnTTViews_RowIndexTables` |

## Benchmarks and research scenarios

The research methodology is RESEARCH's (Z2-D-064; `Zenith2/Docs/Rendering.md` → "Shading path research methodology" states the shared procedure); ECS provides the scenarios, measured twice each: **natively** (each backend through its own API, idiomatically, no Zenith adapter; for EnTT both plain views and owning groups over typed components) and **through `Zenith_ECS`**. Harness targets:

| Target | Directory | Links | Measures |
|---|---|---|---|
| `Zenith_Bench_ECS` | `Zenith2/Benchmarks/ECS/` | `ZenithECS`, the three backends (their public factory headers, [Z2-N-CONTRACT-050]), `ZenithJobs` | every scenario through `Zenith_ECS` on each backend except `LuauBehavioursVsChunkSystems` and `WDense`, which need the runtime (`Zenith_Bench_ECSWorkload`) |
| `Zenith_Bench_ECSNative` | `Zenith2/Benchmarks/ECSNative/` | flecs and EnTT directly from its `Internal/` (a manifest second owner, cross-area note for SHIP), `ZenithECS_Custom` internals through `ZenithECS_Custom`'s test-only native entry (below) | every scenario natively except `WDense`, whose native half is `Zenith_Bench_BudgetsECS`'s, and the two scenarios reported as not applicable natively |
| `Zenith_Bench_ECSWorkload` | `Zenith2/Benchmarks/ECSWorkload/` | `ZenithRuntime` (and through it `ZenithScript` and the selected modules) and each backend with a hand-built composition through `Zenith_RuntimeMain` (cross-area note M13); `SUPPORTED` narrows to `win`, `win-asan`, because ZenithRuntime is a GPU library | W-Dense once per backend through `Zenith_ECS` ([Z2-N-BUDGET-204]), and the `LuauBehavioursVsChunkSystems` scenario, the one scenario that needs the Luau runtime |
| `Zenith_Bench_BudgetsECS` | `Zenith2/Benchmarks/Budgets/ECS/` | flecs and EnTT directly from its own sources (a manifest second owner, cross-area notes E8 and E16), `ZenithECS_Custom_Tests` launched with `--bench-native WDense` from Stage 2 | the W-Dense component mix natively per backend: the native half of the `WDense` scenario, `Bench_ECSNative_<Backend>_WDense`, and the Stage 1 budget evidence (§ "Stage 1 budget harness") |

The custom backend's native API is its internal chunk store: its native scenarios live in `Zenith2/Engine/ZenithECS_Custom/Tests/Native/` and build into `ZenithECS_Custom_Tests` as cases of the suite `z2-stress` (which runs them at smoke size in the `Test` step and at full size when `Zenith_Bench_ECSNative` launches that executable with `--bench-native`), because only the backend's own tests may include its `Internal/` ([Z2-N-CONTRACT-024]).

Scenarios (the list of Z2-D-064), each with its benchmark name in both harnesses (`Bench_ECS_<Scenario>` through `Zenith_ECS`, `Bench_ECSNative_<Backend>_<Scenario>` natively):

| Scenario | Content |
|---|---|
| `Churn` | spawn and destroy churn: 100,000 live entities, 10% replaced per tick, 600 ticks |
| `Iterate1`, `Iterate2`, `Iterate4` | iterating 1, 2 and 4 components (f32 × 4 each) over 1,000,000 entities, read-modify-write |
| `RandomAccess` | 100,000 lookups per tick of random live entities by handle, reading one property |
| `AddRemove` | adding and removing a tag and a 32-byte component on 10% of 100,000 entities per tick |
| `Hierarchy` | walking a 100,000-entity hierarchy of depth 8 by depth level, propagating a transform |
| `LuauBehavioursVsChunkSystems` | the same stamina update as 10,000 behaviours and as one chunk system, run by `Zenith_Bench_ECSWorkload` (the only harness that links the Luau runtime), through `Zenith_ECS` only; natively not applicable, reported as such |
| `WDense` | through `Zenith_ECS`: the W-Dense workload through `Zenith_Bench_ECSWorkload` (`Bench_ECS_WDense`, once per backend); natively: `Zenith_Bench_BudgetsECS`'s W-Dense component-mix runs, designated `Bench_ECSNative_Custom_WDense`, `Bench_ECSNative_Flecs_WDense` and `Bench_ECSNative_EnTT_WDense`. The native half omits, relative to the runtime run, the Luau behaviours and chunk systems, every module (physics, animation, AI), the frame graph and executor, sync-point playback through `Zenith_ECS` and render extraction: it measures the component mix's storage bytes per entity, iteration over the mix's system access patterns and the mix's per-tick spawn, destroy and add-remove churn through each library's own API, so the report attributes the difference between the two halves to the adapter and the runtime and the difference between native halves to the storage architectures (Z2-D-064) |
| `ContainerChurn` | 100,000 entities each carrying one container field of 8 to 64 `uint32` elements, 10% resized per tick, 600 ticks; through `Zenith_ECS` against a per-block `Zenith_Heap` baseline written in the same harness (the evidence of the `Zenith_ContainerHeap` justification record, `Zenith2/Docs/Design/ECS.md` → "Justification records ECS adds"); natively not applicable, reported as such |

Each report adds, for EnTT, the adapter cost rows of `Zenith2/Docs/Design/ECS/Backends.md` → "Adapter cost model" and, for every backend, the job-scheduling overhead of the same scenario (`jobs.overhead_ns`), and states which differences are architectural (native against native) and which are adapter artefacts (Z2-D-064). Procedure: controlled conditions, one discarded warm-up, at least 10 repetitions, medians with 95% confidence intervals, raw data under `Zenith2/Docs/Research/ECS/<date>/` ([Z2-N-BUDGET-074], [Z2-N-BUDGET-076], [Z2-N-BUDGET-079]). Every harness has a `--smoke` CTest run in the `Test` step; full runs are human-run on the reference hardware for the Stage 5 research report (RESEARCH) and for flagged proposal P1's ruling if it is approved.

## Stage 1 budget harness

`Zenith_Bench_BudgetsECS` (`Zenith2/Benchmarks/Budgets/ECS/`), the harness half of "Stage 1 per-system budget ticket for ECS" and, from Stage 2, the native half of the `WDense` research scenario (`Bench_ECSNative_<Backend>_WDense`, § "Benchmarks and research scenarios") ([Z2-N-BUDGET-020]; blocked by the Stage 1 workload scale ticket for W-Dense, [Z2-N-BUDGET-022]): native custom, flecs and EnTT micro-benchmarks of the W-Dense component mix (`Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule", table "Per-system producer shape"), reporting component storage bytes per entity (Z2-B-ECS-001), sync-point playback time for the W-Dense command mix (Z2-B-ECS-002) and the per-tick spawn and command counts of the mix (Z2-B-ECS-003, Z2-B-ECS-004), as an evidence record in the Budgets format; `--smoke` in the `Test` step. The mix (component types, sizes, per-tick churn) is read from `Zenith2/Benchmarks/Budgets/ECS/WDenseMix.json`, which the harness ticket writes from `Zenith2/Docs/Research/Workloads/W-Dense.md`, so the Stage 1 evidence and the Stage 2 native runs measure one mix. In Stage 1, before `ZenithECS_Custom` exists, the custom figure comes from a 16 KiB-chunk structure-of-arrays store inside the harness laid out as `Zenith2/Docs/Design/ECS/Backends.md` → "Chunk layout"; from Stage 2 the harness runs `Bench_ECSNative_Custom_WDense` by launching `ZenithECS_Custom_Tests --bench-native WDense`, whose native case reads the same `WDenseMix.json`, and the harness's own custom store is deleted in the slice that adds the native case. flecs and EnTT are measured through their own APIs (for EnTT both plain views and owning groups over typed components) in both stages.

## Gate steps per test group

| Group | `Test` (win-debug, win-release) | `EcsConformance` (`Zenith2ECS`) | `Sanitizers -Preset win-asan` / `linux-tsan` | Stage 2 acceptance |
|---|---|---|---|---|
| Reflect and ECS unit tests, examples | yes | — | yes when the diff touches a `Zenith2Memory` path | yes, plus the S24 device run |
| conformance suite | yes | yes, against all three backend targets | yes (`linux-tsan` covers every ECS library, none is a GPU library) | yes |
| determinism integration suite | yes | — | yes | yes, full matrix |
| death suites | yes (separate CTest tests) | — | yes | yes |
| fuzz targets | — | — | `win-asan` 60-second smoke | full duration |
| benchmarks and the budget harness | `--smoke` only | — | — | full runs by the human for the ruling tickets |

<!-- elaboration --> A diff that adds a test name to "Conformance suite", "ECS unit tests", "Death tests" or "Backend unit tests" also appends it, in byte order, to `Zenith2/Engine/ZenithECS/Tests/ConformanceNames.txt` when that file exists, so the `Ecs_ConformanceNames_*` check covers it.
