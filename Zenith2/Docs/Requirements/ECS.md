# ECS requirement catalogue

The requirement catalogue of the ECS area (Reflection & ECS). Every requirement is one testable obligation with the inventory items it covers, its stage, classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design is `Zenith2/Docs/Design/ECS.md` and its split files under `Zenith2/Docs/Design/ECS/`.

## Purpose

ECS owns `ZenithReflect` (the one reflection system: explicit and runtime type registration, stable type and property IDs, schema versions, data-defined component schemas, migration, field sets and the reflection walk) and `ZenithECS` with its three backends `ZenithECS_Custom`, `ZenithECS_Flecs` and `ZenithECS_EnTT` (worlds, handles, command buffers, sync-point playback, structural epochs, invocation-scoped views, change tracking, per-field authority checks, arbitration, the frame-graph translation, world isolation, the conformance and determinism suites and the ECS benchmarks), as `Zenith2/Docs/Design/ECS.md` → "What ECS owns" states. This catalogue is authoritative for coverage (Z2-D-066, Z2-D-067): every inventory item `Z2-I-ECS-01` … `Z2-I-ECS-17` is covered by at least one requirement below.

Evidence phrases: "`ZenithECS_Conformance` on the Custom, flecs and EnTT targets" means the conformance executable registered once per backend (`<test>[Custom]`, `[Flecs]`, `[EnTT]`); the `Test` step runs it, and the `Zenith2ECS` category's `EcsConformance` line runs it on every diff touching `ZenithECS*`. Every named test is specified in `Zenith2/Docs/Design/ECS/Tests.md`.

The catalogue is two files with one numbering: this file holds the purpose, Z2-R-ECS-001 to Z2-R-ECS-175, the coverage of inventory items (over both files) and the exclusions; `Zenith2/Docs/Requirements/ECS/ConsumerSeams.md` holds Z2-R-ECS-176 onward, the seams other areas consume (the edit-world flag, container writes without a merge reservation, per-ordinal event writers, play-last command kinds with the playback version, and entity handles as reflected C++ members).

## Requirements

### Z2-R-ECS-001 ZenithReflect library target

- **Statement:** ZenithReflect is one static library at `Zenith2/Engine/ZenithReflect/` whose CMakeLists.txt is exactly `zenith_add_library(ZenithReflect DEPENDS ZenithCore)` with the default `ZENITH_ALL` matrix, no third-party dependency and no platform file.
- **Covers:** Z2-I-ECS-01, Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` (no undeclared edge, no cycle) and `CompileCoverage` steps on every preset including linux-tsan and android-arm64
- **Design:** Zenith2/Docs/Design/ECS.md → "ZenithReflect target"; Zenith2/Docs/Design/ECS.md → "Library dependencies"

### Z2-R-ECS-002 ZenithECS library target

- **Statement:** ZenithECS is one static library at `Zenith2/Engine/ZenithECS/` whose CMakeLists.txt is exactly `zenith_add_library(ZenithECS DEPENDS ZenithCore ZenithReflect ZenithGraph ZenithJobs ZenithProfiling)`, with the default `ZENITH_ALL` matrix, and it exports no world factory.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` and `CompileCoverage` steps on every preset; `Libgraph` reports no factory symbol exported by ZenithECS
- **Design:** Zenith2/Docs/Design/ECS.md → "ZenithECS target"; Zenith2/Docs/Design/ECS.md → "Library dependencies"

### Z2-R-ECS-003 Three backend library targets

- **Statement:** ZenithECS_Custom, ZenithECS_Flecs and ZenithECS_EnTT are each one `KIND BACKEND` static library with `BACKEND_OF ZenithECS`, `BACKEND_NAME` Custom, Flecs or EnTT and `FACTORY` Zenith_CreateWorld_Custom, _Flecs or _EnTT, declaring `DEPENDS ZenithECS ZenithReflect ZenithCore`, and each public header declares its factory and fault points and nothing else.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` (swap-contract report for the ECS family written, consumers use the API library alone) and `ApiContractCheck` steps
- **Design:** Zenith2/Docs/Design/ECS.md → "ZenithECS_Custom target"; Zenith2/Docs/Design/ECS.md → "ZenithECS_Flecs target"; Zenith2/Docs/Design/ECS.md → "ZenithECS_EnTT target"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Backend factory headers"

### Z2-R-ECS-004 Third-party headers confined to backend internals

- **Statement:** flecs headers are included only by the `ZenithECS_Flecs/Internal/` files the design lists and EnTT headers only by the `ZenithECS_EnTT/Internal/` files it lists; no public header of any ECS library names a flecs or EnTT type.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (leakage and third-party-outside-owner rules) and the `ForbiddenCheck` step
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Backend internal files"; Zenith2/Docs/Design/ECS.md → "ZenithECS_Flecs target"; Zenith2/Docs/Design/ECS.md → "ZenithECS_EnTT target"

### Z2-R-ECS-005 Type-erased ECS API

- **Statement:** Every public ZenithECS entry point takes reflection IDs; every template in an ECS public header is an inline wrapper over a type-erased function and adds no storage behaviour.
- **Covers:** Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_DataComponent_IdenticalToCppTwin` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line; confirmed in review of every ECS public header diff
- **Design:** Zenith2/Docs/Design/ECS.md → "API rules"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Typed convenience wrappers"

### Z2-R-ECS-006 Contract blocks on every Reflect and ECS header

- **Statement:** Every public type of ZenithReflect, ZenithECS and the three backends carries one contract block of the Contracts grammar, and every public header compiles on its own.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `ApiContractCheck` and `HeaderSelfContained` steps
- **Design:** Zenith2/Docs/Design/ECS.md → "API rules"; Zenith2/Docs/Design/ECS/Reflect.md → "Header list"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Header list"

### Z2-R-ECS-007 Upward notifications through ECS-declared interfaces

- **Statement:** ZenithReflect and ZenithECS call higher libraries only through the interfaces they declare (`Zenith_StructuralObserver`, `Zenith_TickBoundaryHandler`, `Zenith_PfnCommandPlayback`, `Zenith_PfnSystemRun`, `Zenith_PfnSystemMerge`, `Zenith_ValueKindHooks`, `Zenith_ReferenceResolver`, `Zenith_MigrationScriptHost`) and store no other pointer into higher code.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (no upward edge); confirmed in review
- **Design:** Zenith2/Docs/Design/ECS.md → "Upward-callback interfaces ECS declares"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ECSObservers header"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_CommandKinds header"

### Z2-R-ECS-008 Type ID from the stable name

- **Statement:** A `Zenith_TypeID` is `Zenith_Hash64` of the registered stable name with seed 0, never of a C++ type name, for C++-registered, schema and behaviour-state types alike.
- **Covers:** Z2-I-ECS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_TypeIDFromStableName` and `Test_Schema_TypeIDFromStableName` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_ReflectIDs header"; Zenith2/Docs/Design/ECS/Reflect.md → "Registration algorithm"

### Z2-R-ECS-009 Type ID collisions and duplicate names rejected

- **Statement:** A registration whose type ID equals a registered type with a different stable name fails with `REFLECT_ERROR_TYPE_ID_COLLISION` (`SCHEMA_ERROR_TYPE_ID_COLLISION` for schemas) naming both stable names, and a duplicate stable or script name fails with its code.
- **Covers:** Z2-I-ECS-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_EachErrorCodeReported` and `Test_Schema_CollisionWithCppType_Rejected` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"; Zenith2/Docs/Design/ECS/Schemas.md → "Validation rules catalogue"

### Z2-R-ECS-010 Explicit property IDs never reused

- **Statement:** Every property carries an explicit non-zero `Zenith_PropertyID` below 2^31, unique within its type and absent from the type's retired IDs; a reused or duplicate ID fails registration.
- **Covers:** Z2-I-ECS-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_EachErrorCodeReported`, `Test_Schema_ReusedFieldID_Rejected` and `Test_Schema_RetiredListShrink_Rejected` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"; Zenith2/Docs/Design/ECS/Schemas.md → "The schema lock file"

### Z2-R-ECS-011 Schema version and one migration step per increment

- **Statement:** Every type carries a `uint32` schema version, and `Freeze` fails with `REFLECT_ERROR_MIGRATION_CHAIN` unless the type has exactly one migration step for each version 1 to version − 1.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_EachErrorCodeReported` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_Migration header"

### Z2-R-ECS-012 Registration is explicit and never static

- **Statement:** Types register only through calls the composition root makes (engine, then modules in canonical order, then schemas), never through static initialisation, and the registration order never influences an ID, a layout or an iteration order.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` writable-data scan (F16) and `Test_TypeRegistry_GetTypesInStableNameOrder` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Registration algorithm"; Zenith2/Docs/Design/ECS/Reflect.md → "Lookup structures"

### Z2-R-ECS-013 Declarations accepted before the string table is bound

- **Statement:** The type registry accepts C++ and runtime declarations from `Create` onward before `BindStringTable`, keeping each string default as text, and `BindStringTable` interns every recorded default in registration order; `Freeze` or a schema registration before binding is `ZENITH_CHECK`.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_DeclarationBeforeStringTable` in the `Test` step (win-debug, win-release) and `Test_TypeRegistry_FreezeBeforeStringTable_Checks` in the `z2-death-check` suite
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Registration algorithm"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeRegistry"; Zenith2/Docs/Design/ECS.md → "Construction and shutdown"

### Z2-R-ECS-014 Configuration schema exposed during declarations

- **Statement:** The type registry holds the configuration schema the composition root attaches with `AttachConfigSchema` and returns it from `GetConfigSchema()` while it is not frozen, so a module's `RegisterTypes` registers its configuration keys beside its types; `GetConfigSchema` with no schema or a frozen one is `ZENITH_CHECK`.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_ConfigSchemaExposedDuringDeclaration` in the `Test` step (win-debug, win-release) and `Test_TypeRegistry_GetConfigSchemaAfterFreeze_Checks` in the `z2-death-check` suite
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Registration algorithm"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeRegistry"

### Z2-R-ECS-015 Freeze validates the whole registry in one pass

- **Statement:** `Freeze` checks every rule of the freeze-validation table over every pending type, logs every error, publishes nothing when any error exists and returns the first error in stable-name order.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_EachErrorCodeReported` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"

### Z2-R-ECS-016 Frozen registry is immutable and safe to read concurrently

- **Statement:** After `Freeze`, a C++ registration outside a hot-reload window is `ZENITH_CHECK`, a runtime registration returns `REFLECT_ERROR_FROZEN`, a schema registration returns `SCHEMA_ERROR_REGISTRY_FROZEN`, and lookups from any number of threads are race-free; the one write to a frozen registry outside a window is `ResolveReferenceDefaults`, admitted once per value kind with no window open while no world can read the defaults, any other call of it (before `Freeze`, in a window, a second time) being `ZENITH_CHECK`.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_FrozenRejectsRegistration` and `Test_TypeRegistry_ConcurrentReadsAfterFreeze` (under `Sanitizers -Preset linux-tsan`) in the `Test` step (win-debug, win-release); `Test_TypeRegistry_CppRegistrationAfterFreeze_Checks` and `Test_TypeRegistry_ResolveReferenceDefaultsOutOfState_Checks` in `z2-death-check`
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Revisions and the hot-reload window"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeRegistry"

### Z2-R-ECS-017 Hot-reload window admits only its kinds

- **Statement:** A hot-reload window replaces `TYPE_KIND_BEHAVIOUR_STATE` types during play and schema types between editor play sessions, and any other change in a window is `REFLECT_ERROR_HOT_RELOAD_KIND`.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_HotReloadKindRejected` and `Test_TypeRegistry_HotReloadPublishesRevision` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Revisions and the hot-reload window"

### Z2-R-ECS-018 Revision publication is atomic and failure keeps the old revision

- **Statement:** `EndHotReload` validates the staged types, publishes a new revision by one index swap and keeps replaced `Zenith_TypeInfo` pointers readable for two `RetireFrame` calls; a validation failure (including a staged asset default the bound resolver cannot map, `REFLECT_ERROR_BAD_DEFAULT`) or `AbandonHotReload` publishes nothing.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_HotReloadPublishesRevision`, `Test_TypeRegistry_HotReloadFailureKeepsRevision` and `Test_TypeRegistry_AbandonDiscardsWindow` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Revisions and the hot-reload window"

### Z2-R-ECS-019 Runtime type builder is equivalent to C++ registration

- **Statement:** `Zenith_RuntimeTypeBuilder` produces `Zenith_TypeInfo` records identical in every field but origin, layout provenance and source path to a C++ registration of the same fields, and is the path of schema and behaviour-state types.
- **Covers:** Z2-I-ECS-12, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_ReflectionWalk_SchemaAndCppTwinIdentical` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_RuntimeTypeBuilder"; Zenith2/Docs/Design/ECS/Reflect.md → "Registration algorithm"

### Z2-R-ECS-020 No consumer branches on type origin

- **Statement:** Serialization, the inspector walk, migration, Luau type generation, chunk-system column access, authority checks and replication read `Zenith_TypeInfo` and `Zenith_PropertyInfo` alone and never branch on `m_eOrigin`.
- **Covers:** Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_DataComponent_IdenticalToCppTwin` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line and `Test_LuauTypes_SchemaAndCppTwinIdentical` in the `Test` step (win-debug, win-release); confirmed in review
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeInfo header"; Zenith2/Docs/Design/ECS.md → "Inspector data"

### Z2-R-ECS-021 Computed layout of runtime types

- **Statement:** The registry places the fields of every schema, struct and behaviour-state type by alignment descending then property ID, rounds the size to the largest alignment, rejects components above 1,024 bytes or 16-byte alignment, and never writes a layout into a file.
- **Covers:** Z2-I-ECS-13, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeLayout_OrderByAlignmentThenID` and `Test_TypeLayout_LimitsRejected` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Layout algorithm"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeLayout header"

### Z2-R-ECS-022 C++ lifecycle hooks

- **Statement:** A C++ type that is not trivially copyable registers construct, destroy, copy and move hooks instantiated from its own constructors, and a component type is trivially relocatable (trivially copyable or declaring `bZENITH_TRIVIALLY_RELOCATABLE`), which `BeginComponent` asserts at compile time.
- **Covers:** Z2-I-ECS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_HooksBalanceReferences` in the `Test` step (win-debug, win-release) and the `Build` step (the static assertion)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "C++ hooks"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_ReflectValueTraits header"

### Z2-R-ECS-023 Schema lifecycle hooks and value-kind hooks

- **Statement:** A schema type's hooks construct defaults, and destroy and copy every asset field (inside structs and arrays too) through the value-kind hooks ZenithAssets installs, constructing a null-default asset field null and a resolved asset default through the copy hook; a schema with an asset field registered before the hooks are installed fails with `REFLECT_ERROR_MISSING_HOOKS`.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_HooksBalanceReferences` in the `Test` step (win-debug, win-release) and `Test_DataComponent_AssetFieldRefCounted` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Schema hooks"; Zenith2/Docs/Design/ECS/Reflect.md → "Value-kind hooks"

### Z2-R-ECS-024 Value encodings independent of storage

- **Statement:** Property values in commands, defaults and field sets use the value encodings of the design table, little-endian and independent of every storage layout, with strings as text and entity and asset references as persistent IDs in field sets.
- **Covers:** Z2-I-ECS-13, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_FieldSet_BinaryRoundTrip` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeInfo header"; Zenith2/Docs/Design/ECS/Reflect.md → "Binary field-set record"

### Z2-R-ECS-025 Field-set decoder treats input as untrusted

- **Statement:** `Zenith_DecodeFieldSet` validates every length, count and depth against its input before use and returns `Zenith_FieldSetError` on any malformed input, never an assert.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_FieldSet_DecodeRejectsTruncation` in the `Test` step (win-debug, win-release) and `Zenith_Fuzz_FieldSetDecode` as a smoke in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Binary field-set record"; Zenith2/Docs/Design/ECS/Tests.md → "Fuzz targets"

### Z2-R-ECS-026 JSON field-set form keyed by property ID

- **Statement:** The JSON field-set form keys members by decimal property ID in ascending order, writes values in the schema default JSON forms, and preserves unknown members in position.
- **Covers:** Z2-I-ECS-03, Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_FieldSet_JsonRoundTrip` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "JSON field-set form"

### Z2-R-ECS-027 Serialization never copies a layout

- **Statement:** Capture and apply read and write each property through its registered offset and encoding and never copy an object's memory image, so padding bytes and layout changes never reach a serialized form.
- **Covers:** Z2-I-ECS-13, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_SerializationByPropertyID` and `Test_Schema_LayoutIndependentSerialization` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_FieldSet header"

### Z2-R-ECS-028 Reflection walk for every consumer

- **Statement:** `Zenith_WalkProperties` visits properties depth first in ascending property ID with their property paths and editor metadata, allocates nothing, and yields identical visits for a C++ component and its schema twin.
- **Covers:** Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_ReflectionWalk_VisitsInIDOrder` and `Test_ReflectionWalk_SchemaAndCppTwinIdentical` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Reflection walk algorithm"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_ReflectionWalk header"

### Z2-R-ECS-029 Path writes clamp declared ranges

- **Statement:** `Zenith_WritePropertyAt` clamps a numeric value to the property's declared range and rejects a value of the wrong size.
- **Covers:** Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_ReflectionWalk_WriteClampsRange` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Reflection walk algorithm"

### Z2-R-ECS-030 Schema format confirmed in Stage 1

- **Statement:** The Stage 1 schema-format ticket confirms the `*.schema.json` format, the validation catalogue, the lock file and the `schemas.zreg` layout of this design and appends its ruling to `Zenith2/Docs/DecisionLog.md` with its own explicit decision ID.
- **Covers:** Z2-I-ECS-13
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** the ruling ticket's diff (the DecisionLog entry) and the `Coverage` step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/ECS.md → "Stage 1"

### Z2-R-ECS-031 One schema front end

- **Statement:** Game component and event types are declared only in `Games/<Name>/Schemas/*.schema.json`; no Luau declaration, editor-only format or second schema language defines a component.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_ValidExample_Registers` in the `Test` step (win-debug, win-release); confirmed in review
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Scope of this file"; Zenith2/Docs/Design/ECS.md → "Schema authoring"

### Z2-R-ECS-032 Validation catalogue with exact messages

- **Statement:** Every schema rule of the validation catalogue reports its code and its message template in the form `<file>: <keyPath>: <message>`, and the schema error result carries the first code and the diagnostic count and no text.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_EachErrorCode_Reported` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Validation rules catalogue"; Zenith2/Docs/Design/ECS/Schemas.md → "Zenith_Schema header"

### Z2-R-ECS-033 Every schema error reported in one run

- **Statement:** Schema validation collects every diagnostic of a file, the cook reports every diagnostic of every schema of a game in one run, and a file with any diagnostic registers nothing.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_AllErrorsInOneRun` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Schema validation pipeline"; Zenith2/Docs/Design/ECS/Schemas.md → "Cook integration"

### Z2-R-ECS-034 Schema limits enforced

- **Statement:** Schemas are limited to 128 fields per component or struct, 1,024 inline bytes, struct depth 4, array count 64, `maxCount` 65,535, 64 structs per file and 1 MiB per file, each limit reported with its name.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_Limits_Enforced` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Validation rules catalogue"

### Z2-R-ECS-035 Unknown keys rejected and extension keys preserved

- **Statement:** A schema key that is not in the format and not `x-`-prefixed fails with `SCHEMA_ERROR_UNKNOWN_KEY`, and every tool rewrite preserves `x-` keys byte for byte in position.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_EachErrorCode_Reported` and `Test_Schema_UnknownExtensionKeysPreserved` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Validation rules catalogue"; Zenith2/Docs/Design/ECS/Schemas.md → "Editor re-registration"

### Z2-R-ECS-036 Event schema restrictions

- **Statement:** An event schema carries no authority, arbitration, network authority, replicated or restore-after-cinematic key and no container field, and its fields' serialized flags decide what replays and tick hashes record of each event.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_EventForbiddenKeys` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Event schemas"

### Z2-R-ECS-037 Entity defaults are null and asset defaults resolve

- **Statement:** A non-null schema default of an `entity` field fails with `SCHEMA_ERROR_BAD_DEFAULT`; an `asset<K>` default is `null` or a 36-character persistent ID (anything else fails with `SCHEMA_ERROR_BAD_DEFAULT`), is resolved once per process by `ResolveReferenceDefaults` through `Zenith_ValueKindHooks::m_pfnConstructFromPersistentID` (an ID naming no asset of kind K fails with `SCHEMA_ERROR_BAD_DEFAULT` at the cook and at startup step 11), and no world is created over unresolved reference defaults; string defaults are interned at registration.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_NonNullEntityDefault_Rejected`, `Test_Schema_MalformedAssetDefault_Rejected`, `Test_Schema_AssetDefaultResolved` and `Test_Schema_UnresolvableAssetDefault_Rejected` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Default values"; Zenith2/Docs/Design/ECS/Reflect.md → "Value-kind hooks"; Zenith2/Docs/Design/ECS/Reflect.md → "Schema hooks"

### Z2-R-ECS-038 Schema lock file enforced

- **Statement:** Validation compares each schema with `Schemas/schemas.lock.json`: the stable name never changes, the retired list never shrinks, a vanished field is retired, and a changed field set raises the version with a complete chain; the lock is rewritten only by the explicit `schema-lock` step.
- **Covers:** Z2-I-ECS-01, Z2-I-ECS-02, Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_RetiredListShrink_Rejected` and `Test_Schema_EachErrorCode_Reported` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "The schema lock file"

### Z2-R-ECS-039 Schema blob writer is deterministic

- **Statement:** `Zenith_WriteSchemaBlob` writes the header and entries of the `schemas.zreg` layout in stable-name order, and two writes from one registry are byte-identical; a blob registered gives types identical to the JSON path.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_BlobRoundTrip` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "The schema registry blob format"

### Z2-R-ECS-040 Schema blob reader treats input as untrusted

- **Statement:** `RegisterSchemaBlob` validates magic, version, lengths, offsets, counts and enumerators before use, fails with `SCHEMA_ERROR_BLOB_CORRUPT` on any malformed blob without crashing, and runs every source-independent schema rule again.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_CorruptBlob_Rejected` in the `Test` step (win-debug, win-release) and `Zenith_Fuzz_SchemaBlob` as a smoke in `Sanitizers -Preset win-asan` and for its full duration at stage acceptance
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Reading and validating a blob"; Zenith2/Docs/Design/ECS/Tests.md → "Fuzz targets"

### Z2-R-ECS-041 Schema JSON reader is fuzzed

- **Statement:** `RegisterSchemaJson` never crashes or asserts on arbitrary input and returns a schema error code for every invalid file.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_SchemaJson` as a smoke in `Sanitizers -Preset win-asan` and for its full duration at stage acceptance
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Fuzz targets"; Zenith2/Docs/Design/ECS/Schemas.md → "Schema validation pipeline"

### Z2-R-ECS-042 Invalid schemas stop the cook and the game

- **Statement:** An invalid schema fails the cook, and an invalid `schemas.zreg` at run time stops the game before its first tick with exit code 3 and every diagnostic logged; no data-defined type registers partially.
- **Covers:** Z2-I-ECS-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_CorruptBlob_Rejected` in the `Test` step (win-debug, win-release) and the startup step-10 failure case of MOD's startup tests
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Reading and validating a blob"; Zenith2/Docs/Design/ECS.md → "Failure handling"

### Z2-R-ECS-043 Migration chain validation

- **Statement:** A schema's migrations have one entry per `fromVersion` in ascending order, every op names a field present in the version it applies to, retires are listed in `retiredFieldIds`, conversions are reachable, and replaying the chain gives the declared fields; each violation is `SCHEMA_ERROR_MIGRATION_CHAIN` with its message.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_EachErrorCode_Reported` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Chain validation"

### Z2-R-ECS-044 Migration ops applied in order

- **Statement:** `Zenith_MigrateFieldSet` applies each step's add, rename, retire, retype and script ops in order from the input version to the type's version, then sorts by ID and sets the version.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_Migration_ChainAppliesInOrder` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Op application"

### Z2-R-ECS-045 Failed migration leaves input unchanged

- **Statement:** A migration whose conversion or script op fails returns `Zenith_MigrationError` naming the component, field and versions, logs the entity or document, and leaves the input unchanged.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_Migration_FailureLeavesInput` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Op application"; Zenith2/Docs/Design/ECS/Schemas.md → "Numeric retype conversion"

### Z2-R-ECS-046 Numeric retype conversions

- **Statement:** A `numeric` retype saturates integers, rounds half to even, converts NaN to 0 with a warning, and a `vectorResize`, `stringToId` or `default` conversion behaves as the design table states.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_Migration_NumericConversions` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Numeric retype conversion"

### Z2-R-ECS-047 Newer data opens read-only

- **Statement:** Data written by a newer schema version than the running type is `MIGRATION_ERROR_NEWER_VERSION` and is never written by the running tool.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_Migration_NewerVersionReadOnly` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Op application"

### Z2-R-ECS-048 Unknown fields survive migration

- **Statement:** Fields that no step mentions and the target type does not know are kept through migration with one `MIGRATION_WARNING_UNKNOWN_FIELD` per field path.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_Migration_UnknownFieldsKept` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Op application"

### Z2-R-ECS-049 Script migration ops through the host

- **Statement:** A `script` op runs through the `Zenith_MigrationScriptHost` ZenithScript implements, in a sandboxed VM with no world access within the recovery safepoint budget, and a host failure is `MIGRATION_ERROR_SCRIPT` with the input unchanged.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-016
- **Budgets:** Z2-B-SCRIPT-018
- **Acceptance evidence:** `Test_Schema_Migration_ScriptOpThroughHost` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Script ops"

### Z2-R-ECS-050 Migration fixtures per version

- **Statement:** Every schema version increment ships `Schemas/Fixtures/<Component>/v<from>.json` and its `.expected.json`, and the game's schema test run migrates each fixture and compares it byte for byte.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Schema_GameFixtures_Migrate` in the `Test` step (win-debug, win-release) for every game
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Migration fixtures"

### Z2-R-ECS-051 Component Luau types are deterministic

- **Statement:** `Zenith_EmitComponentLuauTypes` emits one view class per component, struct and event and the `ZenithRow` class from the frozen registry, types in stable-name order and fields in ID order, names verbatim for schemas, into `out/<checkout-hash>/Generated/Luau/<Game>/`, byte-identical across runs and never tracked.
- **Covers:** Z2-I-ECS-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_LuauTypes_ComponentsDeterministic` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Luau type generation from schemas"

### Z2-R-ECS-052 Component Luau types identical for C++ and schema twins

- **Statement:** The Luau view class generated for a data-defined component equals the one generated for its C++-registered twin.
- **Covers:** Z2-I-ECS-14, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_LuauTypes_SchemaAndCppTwinIdentical` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Luau type generation from schemas"

### Z2-R-ECS-053 Cook validates schemas and writes the blob and component types

- **Statement:** The cook validates every schema of a game in file-name order against the lock, writes `schemas.zreg` and runs the `luau-types` step for the game's selected modules only.
- **Covers:** Z2-I-ECS-13, Z2-I-ECS-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** the `<Name>_Content` and `<Name>_Types` targets of `Games/Minimal` built in the `Build` step and `Test_Schema_BlobRoundTrip` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Cook integration"

### Z2-R-ECS-054 Schema types change only between play sessions

- **Statement:** A schema type changes only in a hot-reload window the editor opens with no play world alive, after the documents holding it are migrated; a play world never observes a schema-type change.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_HotReloadKindRejected` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Editor re-registration"; Zenith2/Docs/Design/ECS/Reflect.md → "Revisions and the hot-reload window"

### Z2-R-ECS-055 Edit-world rows follow a schema revision

- **Statement:** When a schema revision is published, every backend migrates the edit world's stored rows of the changed type through the chain into the new layout, keeping handles and simulation IDs and bumping every affected epoch.
- **Covers:** Z2-I-ECS-03, Z2-I-ECS-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_SchemaRevisionMigratesRows` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Schema revision in the edit world"

### Z2-R-ECS-056 Every property declares an authority

- **Statement:** Every component property declares one phase from 1 to 10 or `PHASE_COMMANDS_ONLY`, directly or through its component default, and a property without one fails registration with `REFLECT_ERROR_MISSING_AUTHORITY`.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_MissingAuthority_FailsRegistration` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"; Zenith2/Docs/Design/ECS/Schemas.md → "Authority and arbitration in schemas"

### Z2-R-ECS-057 No authority in the read-only phases

- **Statement:** No property declares authority 11 or 12, and a system of phase 11 or 12 or an epilogue stage declaring a component write fails with `SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE`.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_ReadOnlyPhaseWrite_IsError` and `Test_Schedule_EpilogueStageComponentWrite_IsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"; Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"

### Z2-R-ECS-058 Direct writes only in the owning phase

- **Statement:** A system declares a direct write only of properties whose authority is its own phase, and any other direct write fails with `SCHEDULE_ERROR_AUTHORITY_VIOLATION` and its exact message.
- **Covers:** Z2-I-ECS-17, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_WriteOutsidePhase_IsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"; Zenith2/Docs/Design/ECS/Scheduling.md → "Messages"

### Z2-R-ECS-059 CommandsOnly properties have no direct writer

- **Statement:** A direct write of a `PHASE_COMMANDS_ONLY` property fails with `SCHEDULE_ERROR_COMMANDS_ONLY_WRITE`.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_CommandsOnly_DirectWriteIsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-060 Two direct writers need an arbitration rule

- **Statement:** Two systems declaring a direct write of one property in one phase fail with `SCHEDULE_ERROR_DUPLICATE_WRITER` unless the component declares an arbitration rule covering the property, which inserts one arbitration node.
- **Covers:** Z2-I-ECS-17, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_TwoWriters_IsError` and `Test_Authority_TwoWritersWithArbitration_Compiles` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"; Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration nodes"

### Z2-R-ECS-061 Access and authority checked per property

- **Statement:** A whole-component declaration expands to every property before any check, so a whole-component write in the wrong phase names every offending property.
- **Covers:** Z2-I-ECS-17, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_WholeComponentExpands` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-062 Schema authority behaves as C++ authority

- **Statement:** A data-defined component's field authorities produce exactly the schedule errors of its C++-registered twin.
- **Covers:** Z2-I-ECS-17, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Authority_DataSchemaSameAsCpp` and `Test_DataComponent_AuthorityEnforced` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Schemas.md → "Authority and arbitration in schemas"

### Z2-R-ECS-063 World resources have an owning phase

- **Statement:** A world resource type declares an owning phase from 1 to 11 (11 only for a snapshot buffer, 12 never, else `REFLECT_ERROR_BAD_AUTHORITY`), `AddWorldResource` restates it (a different phase fails with `SCHEDULE_ERROR_RESOURCE_AUTHORITY`), and a direct write from another phase fails with `SCHEDULE_ERROR_RESOURCE_AUTHORITY`, except an epilogue stage's sole-consumer drain; a phase-11 system writing a phase-11-owned resource compiles.
- **Covers:** Z2-I-ECS-17, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_ResourceAuthority_IsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"; Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"; Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"

### Z2-R-ECS-064 Writer sources of arbitrated properties

- **Statement:** Every writer of an arbitrated property declares a writer source; a missing source fails with `SCHEDULE_ERROR_WRITER_SOURCE_MISSING` and two writers with one source in one phase with `SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE`.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_WriterSourceRules_AreErrors` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-065 WeightedBlend only on blendable types

- **Statement:** A `WeightedBlend` arbitration on a property that is not f32, f64, vec2, vec3, vec4, quat or colour fails registration with `REFLECT_ERROR_ARBITRATION_TYPE`, and a custom merge is refused for schema types.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Arbitration_WeightedBlendInvalidType_Fails` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"; Zenith2/Docs/Design/ECS/Schemas.md → "Validation rules catalogue"

### Z2-R-ECS-066 Every schedule error in one sorted pass

- **Statement:** A registry freeze reports every schedule error of the world in one pass as diagnostics sorted by code then registration index, each with the specification's message format, kept until the next freeze, with the result carrying the first code, the world tag and the count.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_AllErrorsInOnePass` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"; Zenith2/Docs/Design/ECS/Scheduling.md → "Messages"

### Z2-R-ECS-067 Schedule checks run in Shipping

- **Statement:** The authority and schedule checks run in every configuration including Shipping, and a startup compile with any error stops the game before its first tick with exit code 3.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_ErrorsCheckedInShipping` in the Shipping test build of `ZenithECS_Tests`
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-068 Appended schedule errors

- **Statement:** A cross-entity read of a column written in the same phase fails with `SCHEDULE_ERROR_CROSS_ENTITY_READ`, a chunk system outside EarlyScripts and LateScripts with `SCHEDULE_ERROR_SCRIPT_PHASE`, and an empty, over-long or `@`-bearing name with `SCHEDULE_ERROR_INVALID_NAME`, each with its message.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_CrossEntityReadOfWrittenColumn_IsError`, `Test_Schedule_ScriptChunkSystemPhase_IsError` and `Test_Schedule_InvalidName_IsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Messages"; Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-069 Queries agree with declared access

- **Statement:** Every property a query term names with `m_bWrite` is a declared write of its system and every other named property a declared read, else `SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH`.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_QueryAccessMismatch_IsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-070 Behaviours run in the script phases

- **Statement:** A behaviours system declaring a phase other than EarlyScripts or LateScripts fails with `SCHEDULE_ERROR_BEHAVIOUR_PHASE`, and a system declaring no phase with `SCHEDULE_ERROR_PHASE_NONE`.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_BehaviourPhase_IsError` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-071 Ordering declarations validated

- **Statement:** A required `Before` or `After` naming no system fails with `SCHEDULE_ERROR_ORDERING_TARGET_MISSING`, an ordering against the phase order with `SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE` in its Before or After message, and an `…IfPresent` ordering on an absent name is dropped.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_OrderingErrors` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-072 Phases run in fixed order

- **Statement:** Every tick of every world runs the tick boundary, then phases 1 to 11 each followed by its sync point, then the end-of-tick playback, and phase 12 once per frame in the epilogue.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Phases_RunInFixedOrder` and `Test_Phases_ExtractionOncePerFrame` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Stages and barriers"; Zenith2/Docs/Design/ECS.md → "Tick walkthrough"

### Z2-R-ECS-073 EmitInto obeys the graph core rules

- **Statement:** `EmitInto` declares no node name containing `@`, exactly one tick-boundary node per world alone at stage 0, arbitration staging resources as non-persistent, and property resources keyed by world tag, type ID and property ID.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_EmitInto_NodeNamesHaveNoAt`, `Test_EmitInto_OneTickBoundaryAtStageZero` and `Test_EmitInto_StagingResourcesTransient` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Node and resource naming"; Zenith2/Docs/Design/ECS.md → "EmitInto rules for the graph core"

### Z2-R-ECS-074 EmitInto is deterministic

- **Statement:** Identical registrations produce identical builder declarations in identical order and an identical declaration hash, so the compiled schedule's structure hash is stable.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EmitInto_DeclarationHashStable` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "EmitInto translation"; Zenith2/Docs/Design/ECS.md → "EmitInto rules for the graph core"

### Z2-R-ECS-075 Merge nodes form a chain in stage order

- **Statement:** Every system that records commands, emits events, reduces or declares `ConsumesMergeCapacity`, and every script system, gets a merge node ordered after it and after the merge node preceding it in its world and phase's cost-independent stage order (Kahn over the stage's hazard and ordering edges without the chain, ties by registration index), and the merge node is the declared writer of the system's columns, so a merge-node reader registered before the merge-node writer of a property it reads schedules without a cycle.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_EmitInto_MergeChainInStageOrder` in the `Test` step (win-debug, win-release) and `Test_ECS_MergeOrderIndependentOfCompletion` and `Test_ECS_ChangedQueryReaderRegisteredBeforeWriter` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Merge nodes and the merge chain"; Zenith2/Docs/Design/ECS/Queries.md → "Stage order of merge points"

### Z2-R-ECS-076 Extractors precede the render seal

- **Statement:** Every extractor node declares `Before("Render.Seal@W0")` and no other epilogue stage does.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EmitInto_ExtractorsBeforeSeal` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"

### Z2-R-ECS-077 Disabling a system never recompiles

- **Statement:** Disabling a system takes effect at the next tick boundary and its node completes without running its body, with zero schedule recompiles.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_DisableFromNextTickBoundary` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Disabled systems"

### Z2-R-ECS-078 Registration index is the system order

- **Statement:** A system's registration index is the order of its `AddSystem` or `AddEpilogueStage` call, is the merge key's system order, and never depends on the compiled execution order.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SystemRegistry_RegistrationIndexIsOrder` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "System registry internals"

### Z2-R-ECS-079 Reload scope failure keeps declarations

- **Statement:** A reload scope edits a copy of the declarations, and a commit that fails its freeze leaves the previous declarations and schedule in effect.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_ReloadFailureKeepsDeclarations` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "System registry internals"

### Z2-R-ECS-080 A failed graph compile is recorded as schedule diagnostics

- **Statement:** After a failed frame-graph compile the composition root records the graph diagnostics in the registry, which appends one `SCHEDULE_ERROR_GRAPH` diagnostic per graph diagnostic with the text prefixed `schedule: `.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Schedule_AllErrorsInOnePass` in the `Test` step (win-debug, win-release) (a case with a forced ordering cycle)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "System registry internals"

### Z2-R-ECS-081 System context grants exactly the declaration

- **Statement:** A system reaches component data, resources, queries, buffers and events only through its context, which grants what the registration declared: an undeclared column is null and an undeclared resource is null, with an assert where asserts are enabled.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_UndeclaredColumnIsNull` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line, `Test_SystemContext_DeclaredResourceReturned` in the `Test` step (win-debug, win-release) and `Test_SystemContext_UndeclaredResource_Asserts` in `z2-death-assert`
- **Design:** Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"

### Z2-R-ECS-082 Sub-jobs use their own leaf context

- **Statement:** A sub-job obtains its context through `ForLeaf` and allocates scratch, change bits, buffers and event records from its own job context's node arena, never through the node's context.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_SystemContext_LeafContextOwnArena` under `Sanitizers -Preset linux-tsan` and in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Commit and collection"

### Z2-R-ECS-083 Structural changes only at sync points

- **Statement:** Spawn, destroy, add, remove and reparent are recorded into command buffers and applied only by sync-point, end-of-tick or out-of-tick playback; a spawn recorded in phase p is visible from phase p + 1.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_SpawnVisibleNextPhase` and `Test_Commands_AppliedAtNextSyncPoint` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"; Zenith2/Docs/Design/ECS.md → "Chunk queries"

### Z2-R-ECS-084 Command record format

- **Statement:** Every command record is the 32-byte header followed by its payload, 8-byte aligned, with the payload layout of the record-format table.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CommandBuffer_RecordLayout` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Record format"

### Z2-R-ECS-085 Playback in merge order

- **Statement:** Command buffers from parallel invocations are played back in lexicographic merge-key order, never completion order.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_MergeOrderIndependentOfCompletion` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-086 Last property command in merge order wins

- **Statement:** Two property commands on one entity property resolve to the greater merge key, identical with 1, 2, 8 and all workers, and each overwrite increments `ecs.command_overwrites_delta`; the one exception is a redundant `ADD_COMPONENT`, whose values step 6 applies after every step-5 property command of the playback, so they override a property command of the same property whatever its merge key.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Commands_LastInMergeOrderWins`, `Test_ECS_CommandsOverwriteLastInMergeOrder` and `Test_ECS_RedundantAddStampsVersion` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-087 Stale and foreign commands dropped

- **Statement:** A command naming a stale or foreign handle is dropped at playback with `ecs.commands_stale_delta` incremented and a log entry naming the command and its issuer, never an assert.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Commands_StaleHandleDropped` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-088 Commands on pending-destroy entities

- **Statement:** `AddComponent` on a pending-destroy entity is dropped with `ecs.commands_on_destroyed_delta`, and `SetProperty` on it is applied.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_AddOnPendingDestroyDropped` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-089 Add then remove in one playback

- **Statement:** An add then a remove of one component in one playback leaves it absent, removing an absent component is a counted no-op, and a property command addressed by handle to a component an addition earlier in the same playback adds is dropped with `ECS_ERROR_NO_COMPONENT`, because property commands play in step 5 before structural ones.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_AddRemoveSamePlayback` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-090 Spawn identity assigned at playback in merge order

- **Statement:** Spawned entities receive slots lowest-free-first and stable simulation IDs from the world counter at playback in merge order, identical with 1, 2, 8 and all workers, and a rejected spawn consumes no ID.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_SpawnIDsInMergeOrder` and `Test_ECS_RejectedSpawnConsumesNoID` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"; Zenith2/Docs/Design/ECS/Backends.md → "Entity table"

### Z2-R-ECS-091 Ticket handles resolve at playback

- **Statement:** A command naming a spawn ticket handle of its own buffer applies to the spawned entity — a ticket-addressed property record in step 6 of the playback at its own position in merge order, its row and column stamped at once with the playback's version — and one naming a rejected spawn's ticket is dropped with `ecs.commands_stale_delta`.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_TicketHandleResolves` and `Test_ECS_TicketPropertyCommandStampsVersion` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_CommandBuffer header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"; Zenith2/Docs/Design/ECS/Scheduling.md → "Property writes during structural playback"

### Z2-R-ECS-092 Destroy deferred to the end of the tick with cascade

- **Statement:** A destroyed entity is marked pending destroy at its sync point, stays readable for the rest of its tick, is removed at the end-of-tick playback with its hierarchy depth first in simulation-ID order, and its handles are stale from the next tick.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_DestroyAtTickEnd` and `Test_ECS_DestroyCascadesChildren` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"; Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-093 Owning phase may overwrite a command

- **Statement:** A property command recorded before the property's owning phase is applied at its sync point and may then be overwritten by the owning phase, deterministically.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Commands_OwningPhaseOverwrites` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-094 Command buffer caps

- **Statement:** A command record beyond its buffer's cap, or a module command beyond its kind's payload bound or outside its phase mask, is dropped with its counter and a log entry, never an assert.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** `Test_CommandBuffer_OverCapDropped` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Recording"

### Z2-R-ECS-095 Command kind table

- **Statement:** The command kind table validates each registered kind (name, payload type, bound, phase mask, playback function), assigns kind IDs in registration order from 1, and refuses registration after `Seal`.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_CommandKindTable_RegisterValidation` in the `Test` step (win-debug, win-release) and `Test_CommandKindTable_RegisterAfterSeal_Checks` in `z2-death-check`
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_CommandKinds header"

### Z2-R-ECS-096 Module commands in merge order at step 7

- **Statement:** Module and lifecycle command records play back at step 7 through their kinds' playback functions, single-threaded per world, in merge order, with the records of play-last kinds after every other record of the step (Z2-R-ECS-180).
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ModuleCommandsInMergeOrder` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-097 Merge-time capacity is first-fit in merge order

- **Statement:** Shared ECS capacity (entity slots, command records, event records, container bytes) is computed at each phase start from committed state and accepted per logical invocation by first fit — across merges in the order the stage's merge chain runs them, within a merge in merge-key order — a rejected demand subtracting nothing.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-051
- **Budgets:** Z2-B-ECS-003, Z2-B-ECS-004, Z2-B-GAME-003, Z2-B-SCRIPT-016
- **Acceptance evidence:** `Test_MergeCapacity_FirstFit` in the `Test` step (win-debug, win-release) and `Test_ECS_ContainerHeapExhaustion` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Capacity acceptance"; Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_MergeContext header"

### Z2-R-ECS-098 Commit never allocates or fails

- **Statement:** Accepting a buffer appends it to the phase commit list and playing it back allocates only from capacity reserved at merge, so commit and playback of accepted work cannot fail for lack of capacity.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ContainerHeap_ReservationNeverFailsAllocation` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Container heap"; Zenith2/Docs/Design/ECS/Scheduling.md → "Commit and collection"

### Z2-R-ECS-099 Out-of-tick playback

- **Statement:** `ApplyOutOfTick` applies a buffer's records on the main thread while no frame runs, with the given simulation IDs, stamps its property writes and the rows it creates with `GetChangeVersion()` — out of tick V(t, M) of the current tick index t, whether or not tick t's end-of-tick playback completed — carries its events to the next tick, refreshes stale queries and returns the first error after applying every other record.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_OutOfTickPlayback`, `Test_ECS_OutOfTickStampsChangeVersion` and `Test_ECS_CancelledFrameConsistent` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Out-of-tick playback"; Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"

### Z2-R-ECS-100 Structural observers notified in merge order

- **Statement:** Registered structural observers receive spawned and destroy-pending events at step 6 in merge order and destroyed events at the end of the tick in simulation-ID order, and their follow-up commands apply at the next sync point.
- **Covers:** Z2-I-ECS-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_StructuralObserverOrder` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ECSObservers header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-101 Tick boundary step order

- **Statement:** The tick-boundary node applies a pending pause, advances the tick base of an unpaused world before any step uses a version of the tick, calls its handlers for steps 1, 2 and 3 in registration order, commits their property writes at the tick boundary's merge version, then advances the tick index and resets per-tick capacities, before any phase-1 node of its world.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_TickBoundaryStepOrder` and `Test_ECS_TickBoundaryPropertyWriteCommitted` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"

### Z2-R-ECS-102 Events delivered in phase nine in merge order

- **Statement:** Events accepted in phases 1 to 8 are delivered in phase 9 of the same tick in merge order, and events emitted in phases 9 and 10 in phase 9 of the next tick.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_Events_DeliveredInPhaseNine` and `Test_ECS_EventsDeliveredInMergeOrder` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Events and reductions"

### Z2-R-ECS-103 Reductions folded by the fixed-order tree

- **Statement:** Contributions of accepted logical invocations are folded in merge order with `Zenith_FixedOrderReduce` and written to the reduction target's singleton at the merge version, bit-identically for any worker count.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_FixedOrderReduction` and `Test_ECS_ReductionTargetWritten` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Events and reductions"

### Z2-R-ECS-104 Arbitration precedence

- **Statement:** An arbitrated property's value for a row is the value of the highest-precedence source that wrote it, by the declared order or the default precedence cinematic > ragdoll > physics > animation > script.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Arbitration_PrecedenceDefaultOrder` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration merge"

### Z2-R-ECS-105 Unwritten arbitrated rows keep their value

- **Statement:** A row no source wrote in the phase keeps its previous value.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Arbitration_NoWriterKeepsValue` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration merge"

### Z2-R-ECS-106 Weighted blend arithmetic

- **Statement:** A `WeightedBlend` accumulates in double in declared order, sign-aligns quaternions to the first source and normalises, rounds once to the property type, and keeps the previous value when the weights sum to 0.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Arbitration_WeightedBlend` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration merge"

### Z2-R-ECS-107 Arbitration independent of worker order

- **Statement:** Arbitration results are identical with 1, 2, 8 and all workers and with forced compaction.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Arbitration_IndependentOfWorkerOrder` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration merge"

### Z2-R-ECS-108 Writers stage per source

- **Statement:** A writer's view of an arbitrated property addresses its own source's staging column and weights, readers in the phase read the arbitration result, and staging is reset at every sync point.
- **Covers:** Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ArbitrationStagingPerSource` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Arbitration staging"; Zenith2/Docs/Design/ECS/Queries.md → "Views and validation"

### Z2-R-ECS-109 Query compilation validates terms

- **Statement:** Query compilation rejects zero or more than 16 terms, unregistered or non-component types, unknown properties, conflicting terms and a query without a WITH or CHANGED term, and drops container properties from the column set.
- **Covers:** Z2-I-ECS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_Query_CompileValidation` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Query compilation"

### Z2-R-ECS-110 Query caches refresh only at sync points

- **Statement:** A compiled query's matched-chunk cache is refreshed only by a sync point after the world's query structure version changed (the sync point preceding its owner's next use; the end-of-tick playback refreshes every stale query, so epilogue extractors see the last tick's structure) or out of tick, never during a phase.
- **Covers:** Z2-I-ECS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_QueryStableWithinPhase` and `Test_ECS_ExtractorQueryFreshAfterCatchUp` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Matched-chunk cache and invalidation"

### Z2-R-ECS-111 Storage order is unobservable

- **Statement:** No simulation result depends on chunk order or row order: identical inputs give identical tick hashes under both chunk capacities and with forced compaction.
- **Covers:** Z2-I-ECS-06, Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Query_ChunkOrderUnobservable` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Chunk iteration contract"

### Z2-R-ECS-112 Structural epochs change only with structure

- **Statement:** A chunk's structural epoch increments only when playback adds, removes or moves its rows or changes its layout, never on a property command, a commit or a merge.
- **Covers:** Z2-I-ECS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_EpochBumpedOnlyByStructure` and `Test_SyncPoint_EpochBumpedOnStructuralChange` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"; Zenith2/Docs/Design/ECS/Backends.md → "Common backend contract"

### Z2-R-ECS-113 Views are scoped to their invocation and epoch

- **Statement:** A chunk view is valid only while its scope is the current one and its chunk's epoch equals the epoch at creation, compared in every configuration, and becomes invalid at its invocation's merge.
- **Covers:** Z2-I-ECS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ViewInvalidAfterInvocation` and `Test_ECS_ViewInvalidAfterStructuralChange` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Views and validation"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ChunkView header"

### Z2-R-ECS-114 Read-only and absent columns in views

- **Statement:** String, entity, asset and persistent-ID columns are never writable in views and container properties have no column.
- **Covers:** Z2-I-ECS-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ReadOnlyColumnsInViews` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Query compilation"; Zenith2/Docs/Design/ECS/Queries.md → "Views and validation"

### Z2-R-ECS-115 Commits never invalidate other views

- **Statement:** A commit changes column contents and change versions and never a structural epoch, so two systems writing disjoint properties of one component run concurrently and both commit.
- **Covers:** Z2-I-ECS-06, Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_DisjointColumnWritesConcurrent` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Row change bits and CommitRowChanges"

### Z2-R-ECS-116 Change versions advance at merge only

- **Statement:** A column's change version advances only at a merge point through `CommitRowChanges`, or at a playback with the playback's version (its write-set commit, the rows and columns it creates, its step-6 writes outside the write set, and the relocation rule for rows it moves), never at a system's write, and a failed or rejected write advances nothing.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ChangeVersionAdvancesAtMerge`, `Test_ECS_RedundantAddStampsVersion` and `Test_ECS_ChangedQuerySeesMovedRow` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Row change bits and CommitRowChanges"; Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"; Zenith2/Docs/Design/ECS/Scheduling.md → "Property writes during structural playback"; Zenith2/Docs/Design/ECS/Backends.md → "Change tables"

### Z2-R-ECS-117 CommitRowChanges commits once per chunk column

- **Statement:** A merge calls `CommitRowChanges` once per written chunk column with the accepted rows' bits, which stamps those rows and the column with the merge version, and a playback commits its property writes once per written chunk column from the backend's playback write set, while each property write it makes outside the set (step 6: a redundant addition's values, a ticket-addressed record) stamps its row and column at once with the playback's version; a stale scope or an undeclared column is an error with no effect.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_CommitRowChangesAdvancesVersionOnce`, `Test_ChunkView_CommitRejectsStaleScope`, `Test_ECS_PlaybackWritesCommittedOncePerChunkColumn`, `Test_ECS_RedundantAddStampsVersion` and `Test_ECS_TicketPropertyCommandStampsVersion` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line; `Test_PlaybackWrites_StructuralCallInsideSet_Asserts` and `Test_PlaybackWrites_NoPlaybackVersion_Asserts` in `z2-death-assert`
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Row change bits and CommitRowChanges"; Zenith2/Docs/Design/ECS/Scheduling.md → "Playback write commit"; Zenith2/Docs/Design/ECS/Scheduling.md → "Property writes during structural playback"; Zenith2/Docs/Design/ECS/Backends.md → "Playback writes"

### Z2-R-ECS-118 Merge versions are precomputed

- **Statement:** Each merge point of a tick has a merge version fixed at `Freeze` from its stage and its position in the stage's topological order of declared property hazards and orderings (Kahn's algorithm, ties by registration index, independent of cost hints), `V(t, m) = 1 + (U(t, m) mod (2^32 − 1))` over the unwrapped counter whose tick base advances at the tick boundary before the tick's first version is used, so 0 is never issued and every column and row version is identical with 1, 2, 8 and all workers and under any cost hints.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_MergeVersionsIndependentOfWorkers` and `Test_ECS_VersionZeroNeverIssued` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line; `Test_EmitInto_MergeEnumerationTopological` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"; Zenith2/Docs/Design/ECS/Queries.md → "Stage order of merge points"

### Z2-R-ECS-119 Changed-query filtering

- **Statement:** A `QUERY_TERM_CHANGED` term matches exactly the chunks with a named property whose column version is newer than the version of the merge point preceding the system's node in its previous completed invocation (its read point) and is not the version its own merge stamped then, so a same-stage write the system pre-read is seen in its next invocation, a write it read after is not seen again, and its own in-place writes do not re-trigger it; the filter is conservative and chunk-granular — it never omits a chunk holding a changed row and may include unchanged rows, and unchanged chunks after a restore or a sweep reset — and no system's committed result depends on whether an unchanged row was handed to it.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ChangedQueryFilter`, `Test_ECS_ChangedQueryPreReaderSeesWriteNextTick`, `Test_ECS_ChangedQueryNoSpuriousMatchAfterSameStageWriter`, `Test_ECS_ChangedQueryReaderRegisteredBeforeWriter`, `Test_ECS_ChangedQueryIgnoresOwnWrite` and `Test_ECS_DeterministicReplay` (whose fixture carries a row-filtered change system) in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Change-filtered queries"; Zenith2/Docs/Design/ECS/Queries.md → "Stage order of merge points"

### Z2-R-ECS-120 Per-row change versions

- **Statement:** Every chunk column keeps one row version per row, moved with rows, stamped by commits and by the playback that creates a row, and exposed through `GetRowChangeVersions`; every relocation leaves the destination column version at the newer of its value and the moved row's version, and a chunk a relocation creates starts at its first row's version, so a changed row is matched in whichever chunk it lands under any chunk capacity and compaction.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_RowChangeVersions` and `Test_ECS_ChangedQuerySeesMovedRow` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Change tables"; Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"

### Z2-R-ECS-121 Own-row marking of written columns

- **Statement:** The schedule compiler marks every column a system binds as own-row-only when any system of the same phase and world writes it, and a C++ system's `GetChunkOfEntity` returns the marked columns non-null only for the context's current entity — the simulation ID most recently passed to `Commands`, `Events` (either form) or `Contributions`, or, while none named an entity, the entity of the context's first `GetChunkOfEntity` call — and as null columns for every other entity.
- **Covers:** Z2-I-ECS-06, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Query_OwnRowOnlyMarking` in the `Test` step (win-debug, win-release); `Test_ECS_OwnRowOnlyColumnsNullForOtherEntity` and `Test_ECS_EventsPerInvocationOrdinal` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Own-row and any-row reads"; Zenith2/Docs/Design/ECS/SystemAPI.md → "The current logical invocation"

### Z2-R-ECS-122 Per-row restore support

- **Statement:** Between a sync point (or the tick boundary) and the next, no backend moves, reorders or frees a row, column, change block, row-index table or binding a binding of the phase references, so a column stays addressable at the same address from an invocation to its merge and restoring one snapshotted row leaves every other row's writes intact.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_PerRowRestore` and `Test_ECS_ColumnAddressStableUntilMerge` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Common backend contract"; Zenith2/Docs/Design/ECS/Queries.md → "Row change bits and CommitRowChanges"

### Z2-R-ECS-123 Entity handle behaviour

- **Statement:** The null entity handle never resolves, a destroyed entity's handle is stale from the next tick, a foreign world's handle fails with the wrong-world error, and a slot reaching the maximum generation is retired and never reissued, in every configuration.
- **Covers:** Z2-I-ECS-01, Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_EntityHandle_NullDoesNotResolve`, `Test_EntityHandle_StaleAfterDestroy_DoesNotResolve`, `Test_EntityHandle_ForeignOwner_DoesNotResolve` and `Test_EntityHandle_GenerationExhaustion_RetiresSlot` in the `Test` step (win-debug, win-release) and in win-release; `Test_ECS_HandleGenerationRetire` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_EntityHandle header"; Zenith2/Docs/Design/ECS/Backends.md → "Entity table"

### Z2-R-ECS-124 Component handle behaviour

- **Statement:** A component handle resolves only when its entity resolves and carries its type, and is stale otherwise, in every configuration.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_ComponentHandle_NullDoesNotResolve`, `Test_ComponentHandle_StaleAfterDestroy_DoesNotResolve` and `Test_ComponentHandle_ForeignOwner_DoesNotResolve` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ComponentHandle header"

### Z2-R-ECS-125 Entity table allocates the lowest free slot

- **Statement:** The entity table allocates the lowest free slot and lists children in ascending simulation ID.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_EntityTable_LowestFreeSlotFirst` and `Test_EntityTable_ChildrenInSimulationIDOrder` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Entity table"

### Z2-R-ECS-126 World tags issued round-robin

- **Statement:** The world registry issues tags 1 to 4,095 round-robin skipping live tags and returns `ECS_ERROR_WORLD_TAGS_EXHAUSTED` before any backend factory runs when every tag is live.
- **Covers:** Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldRegistry_RoundRobin` and `Test_WorldRegistry_TagsExhausted` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "World registry algorithm"

### Z2-R-ECS-127 Initial generations for reissued tags

- **Statement:** A reissued tag's initial generation is one above the highest generation released with it, wrapping to 1, and a never-issued tag's is `1 + ((creation counter × 65,537) mod 2^27)`.
- **Covers:** Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldRegistry_InitialGenerationAboveReleased` and `Test_WorldRegistry_FirstIssueOffset` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "World registry algorithm"

### Z2-R-ECS-128 World handles never cross worlds

- **Statement:** No entity handle, component handle, view or command buffer of one world is valid in another, and resolving one there fails with the wrong-world error in every configuration.
- **Covers:** Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_ForeignWorldHandle_Fails` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line and `Test_Worlds_NoHandleCrossesWorlds` in `Zenith_Integration_Determinism`
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Resolve and random access"; Zenith2/Docs/Design/ECS/Tests.md → "Determinism suite"

### Z2-R-ECS-129 Worlds tick independently

- **Statement:** Two worlds of the same content with different pause and time-scale settings running concurrently for 600 ticks each produce the tick hashes of their solo runs.
- **Covers:** Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Worlds_TwoWorldsTickIndependently` in `Zenith_Integration_Determinism` in the `Test` step
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Determinism suite"; Zenith2/Docs/Design/ECS.md → "EmitInto rules for the graph core"

### Z2-R-ECS-130 Failures stay in their world

- **Statement:** A script failure or merge rejection in one world leaves every other world's tick hashes equal to its solo run.
- **Covers:** Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Worlds_FailureInOneWorldIsolated` in `Zenith_Integration_Determinism` in the `Test` step
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Determinism suite"

### Z2-R-ECS-131 Per-world pause

- **Statement:** A paused world runs no system, publishes nothing at its tick boundary until it resumes and does not advance its tick index, and pausing one world pauses no other.
- **Covers:** Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_PausedWorldRunsNothing` and `Test_ECS_PausedWorldPublishesNothing` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_WorldSimulation header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"

### Z2-R-ECS-132 Custom chunk layout

- **Statement:** The custom backend stores each archetype in 16 KiB chunks with a 64-byte header, entity-handle and simulation-ID arrays and one 16-byte-aligned array per property, with capacity from the row bytes up to 1,024 rows or the world's configured capacity.
- **Covers:** Z2-I-ECS-10, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_CustomChunks_CapacityFromRowBytes` in `ZenithECS_Custom_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Chunk layout"; Zenith2/Docs/Design/ECS/Backends.md → "Archetypes"

### Z2-R-ECS-133 Custom structural operations move versions with rows

- **Statement:** Custom inserts append to the lowest-index chunk with space, removals swap the last row into the hole with its change versions, component moves relocate carried columns bytewise, and every insert, move and compaction leaves each destination column version at least as new as the rows placed in it.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_CustomChunks_SwapRemoveMovesVersions` and `Test_CustomArchetypes_EdgeCache` in `ZenithECS_Custom_Tests` in the `Test` step (win-debug, win-release); `Test_ECS_ChangedQuerySeesMovedRow` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Custom structural operations"; Zenith2/Docs/Design/ECS/Backends.md → "Compaction"; Zenith2/Docs/Design/ECS/Backends.md → "Change tables"

### Z2-R-ECS-134 Compaction at end of tick and on request

- **Statement:** Backends compact only at sync points and the end-of-tick playback, compaction bumps every touched chunk's epoch, and forced compaction is available to tests through the world test access.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_CustomCompaction_PacksAndBumpsEpochs` in the `Test` step (win-debug, win-release) and `Test_ECS_DeterministicReplay` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Compaction"

### Z2-R-ECS-135 flecs runtime components with forwarded hooks

- **Statement:** The flecs backend registers each component as a runtime flecs component with the layout's size and alignment and type hooks forwarding to the Zenith lifecycle hooks.
- **Covers:** Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_FlecsTypes_HooksForwarded` in `ZenithECS_Flecs_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "flecs backend"

### Z2-R-ECS-136 flecs slices mirror table moves

- **Statement:** The flecs backend presents slices of up to 256 table rows as chunks with their own epochs, and mirrors every flecs row move into its change tables, the destination slice's column version following the relocation rule.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_FlecsSlices_SliceEpochs` and `Test_FlecsSlices_MirroredRowMoves` in `ZenithECS_Flecs_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "flecs backend"

### Z2-R-ECS-137 flecs allocates from the world heap

- **Statement:** The flecs backend routes every flecs allocation to the world heap and flecs's abort to `ZENITH_FATAL`, and uses no flecs system, pipeline, observer, deferred mode or thread. Named flecs exception: flecs's OS API is one process-wide table with no per-call context, so for a flecs world the world heap is the root heap — the flecs factory rejects a `Zenith_WorldDesc` whose `m_pxHeap` is not the root heap with `ECS_ERROR_INVALID_DESC`, and every flecs world allocation is attributed to the root heap; the Custom and EnTT backends accept any heap. The exception carries the provisional justification record `Custom heap wrapper: Zenith_FlecsOsApi` (Zenith2/Docs/Design/ECS.md → "Justification records ECS adds", `Status: Provisional` until its ruling settles it).
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_FlecsOsApi_AllocationsFromRootHeap` in `ZenithECS_Flecs_Tests` in the `Test` step (win-debug, win-release): every flecs world allocation is counted by the root heap and none by any other heap, and a desc naming another heap is rejected
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "flecs backend"

### Z2-R-ECS-138 EnTT dynamic-storage adapter

- **Statement:** The EnTT backend stores each component in a named storage of byte blocks of the smallest size class that fits, runs the Zenith hooks around emplace, erase and copy, and names storages by dense per-world indices.
- **Covers:** Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_EnTTAdapter_SizeClasses` and `Test_EnTTAdapter_HooksAroundEmplaceErase` in `ZenithECS_EnTT_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "EnTT backend and the dynamic-storage adapter"

### Z2-R-ECS-139 EnTT adapter owning groups

- **Statement:** The adapter keeps each group's entities packed at the front of every owned pool in one order and creates groups in query-compile order for queries whose required pools no group owns.
- **Covers:** Z2-I-ECS-10, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_EnTTGroups_PackedPrefix` in `ZenithECS_EnTT_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Owning groups and views"

### Z2-R-ECS-140 EnTT view chunks with row-index tables

- **Statement:** View queries gather runs of up to 256 entities from the smallest required pool with a row-index table per column, and a view chunk's column version is the newest of its rows' versions under serial-number comparison.
- **Covers:** Z2-I-ECS-10, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_EnTTViews_RowIndexTables` in `ZenithECS_EnTT_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Owning groups and views"

### Z2-R-ECS-141 Data-defined components stored natively and identically

- **Statement:** Every backend stores data-defined components in its own storage, and a component registered once from C++ and once from a schema gives identical query, command, serialization and change-tracking results on each backend.
- **Covers:** Z2-I-ECS-15, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_DataComponent_IdenticalToCppTwin` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Common backend contract"; Zenith2/Docs/Design/ECS/Tests.md → "Schema tests"

### Z2-R-ECS-142 Asset fields keep reference counts balanced

- **Statement:** Copying and destroying entities with an asset field leaves the asset's reference count balanced on every backend.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_DataComponent_AssetFieldRefCounted` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Reflect.md → "Schema hooks"; Zenith2/Docs/Design/ECS/Backends.md → "Common backend contract"

### Z2-R-ECS-143 Container fields own their blocks

- **Statement:** Destroying entities with a container field returns every block to the container heap, and copying an entity allocates new blocks.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_DataComponent_ContainerFieldReleased` and `Test_ECS_ContainerFieldCopyAllocatesNewBlock` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line; `Test_ContainerHeap_MatchesOracle` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Container heap"

### Z2-R-ECS-144 Factories unwind every failed step

- **Statement:** Each backend factory performs the construction steps of the design table and, when a step fails, returns its error and leaks nothing; each step has a fault point and a failure-injection case.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_FactoryFailureUnwinds` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line and `Test_EntityTable_Create_FailsAtSlots_Unwinds` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Factory construction steps"

### Z2-R-ECS-145 Conformance suite on every backend

- **Statement:** Every test of the conformance-suite table exists under its name, runs once per backend in `ZenithECS_Conformance`, and asserts the case given.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** every conformance test in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Conformance suite"

### Z2-R-ECS-146 Determinism matrix per backend

- **Statement:** The determinism fixture's 600-tick replay, which includes a row-filtered `QUERY_TERM_CHANGED` system, gives bit-identical per-tick world-state hashes with 1, 2, 8 and all workers, with and without forced compaction and with the default and the 13-row capacity, on each backend, and a difference reports the first differing tick, entity and property.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_DeterministicReplay` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line; the full matrix in the Stage 2 acceptance
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Determinism suite"

### Z2-R-ECS-147 Determinism with failures

- **Statement:** The same replay with injected script errors, budget exhaustion, snapshot-cap overflow and capacity rejection gives identical tick hashes per backend.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_DeterministicReplayWithFailures` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Determinism suite"

### Z2-R-ECS-148 World-state hash is layout-independent

- **Statement:** `HashTickState` hashes entities in simulation-ID order, components in type-ID order, strings by text and references by simulation or persistent ID, the tick's events and played commands, so one world under both capacities and compaction gives one hash.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_WorldHashLayoutIndependent` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Scheduling.md → "World state hash"

### Z2-R-ECS-149 Runtime-level determinism suite

- **Statement:** The `Zenith2/Tests/Determinism/` suite runs the replay matrix through `ZenithRuntime` on each backend with scripts and the fault matrix and asserts identical tick hashes per backend.
- **Covers:** Z2-I-ECS-10, Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_DeterminismReplay_RuntimeMatrix` in `Zenith_Integration_Determinism` in the `Test` step
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Determinism suite"

### Z2-R-ECS-150 Stage 2 acceptance of the ECS

- **Statement:** The Stage 2 acceptance runs all three conformance suites and the determinism matrix green, the ECS tests under win-asan and linux-tsan, and the ECS test executables on the S24 under HWASan.
- **Covers:** Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** the `Z2 S2 stage acceptance` run through `Run-Committed.ps1 -Acceptance -Stage 2`, including its `needs-human` S24 device step
- **Design:** Zenith2/Docs/Design/ECS.md → "Stage 2"; Zenith2/Docs/Design/ECS/Tests.md → "Gate steps per test group"

### Z2-R-ECS-151 ECS counters with their kinds

- **Statement:** Every world publishes each ECS counter of the design with its world-tag suffix, its `Zenith_CounterKind` in `eKind` (cumulative, delta — every per-tick amount, zone-duration sums included — or gauge) and the Contracts name (a `_delta` suffix on every delta counter and on no other), each at its publication point (per-tick counters once per tick), including the eleven the specification names.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_CountersPublished` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS.md → "Counters"

### Z2-R-ECS-152 ECS log categories

- **Statement:** Reflect, schema, schedule, playback, merge and world messages are logged under their categories in the formats the design gives, with dropped-command lines rate-limited to 64 per tick.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Commands_StaleHandleDropped` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line and `Test_Schema_EachErrorCode_Reported` in the `Test` step (win-debug, win-release) (captured log lines)
- **Design:** Zenith2/Docs/Design/ECS.md → "Log categories and messages"

### Z2-R-ECS-153 ECS dump

- **Statement:** The development console command `ecs.dump` writes a `zenith2.ecs-dump` version 1 JSON document of archetypes, queries and systems with the canonical writer, byte-identical for identical worlds.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_DumpSchema` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS.md → "Debug views dumps and telemetry"

### Z2-R-ECS-154 Inspector data identical for every origin

- **Statement:** The world inspector lists entities in simulation-ID order and components in stable-name order and gathers values into the reflected layout, identically for data-defined and C++-registered components.
- **Covers:** Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_InspectorOrder` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line and `Test_ReflectionWalk_SchemaAndCppTwinIdentical` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS.md → "Inspector data"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_WorldInspector header"

### Z2-R-ECS-155 Capacity keys are simulation-affecting

- **Statement:** The four ECS capacity configuration keys are registered as simulation-affecting configuration, so their values are written into the replay header.
- **Covers:** Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_CapacityKeysRegisteredSimulationAffecting` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS.md → "Configuration keys"

### Z2-R-ECS-156 Research scenarios through Zenith_ECS

- **Statement:** Every Z2-D-064 scenario is measured through `Zenith_ECS` on each backend with a `--smoke` CTest run: churn, iterating 1, 2 and 4 components, random access, add and remove and hierarchy walks by `Zenith_Bench_ECS`, and behaviours against chunk systems and W-Dense by `Zenith_Bench_ECSWorkload`, the harness that links the Luau runtime. W-Dense's native-system mix is measured from Stage 2; the behaviours-against-chunk-systems scenario joins the same harness at Stage 3, when SCRIPT's behaviours and chunk systems exist.
- **Covers:** Z2-I-ECS-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_ECS --smoke` and `Zenith_Bench_ECSWorkload --smoke` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Benchmarks and research scenarios"

### Z2-R-ECS-157 Native research scenarios

- **Statement:** `Zenith_Bench_ECSNative` and the custom backend's native cases measure the same scenarios natively (flecs idiomatically, EnTT through typed views and owning groups, custom through its internal store), and `Zenith_Bench_BudgetsECS`'s W-Dense component-mix runs are the native half of W-Dense (`Bench_ECSNative_<Backend>_WDense`), each report stating what the native half omits, with smoke runs.
- **Covers:** Z2-I-ECS-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_ECSNative --smoke`, `Zenith_Bench_BudgetsECS --smoke` and the `Tests/Native/` cases of `ZenithECS_Custom_Tests` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Benchmarks and research scenarios"; Zenith2/Docs/Design/ECS/Tests.md → "Stage 1 budget harness"; Zenith2/Docs/Design/ECS.md → "Native-path measurements of the ECS rows"

### Z2-R-ECS-158 EnTT adapter costs reported separately

- **Statement:** Every EnTT research report carries the adapter's copying, indirection, memory and chunk-synthesis costs separately from the native result, and every backend report its job-scheduling overhead.
- **Covers:** Z2-I-ECS-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-130
- **Budgets:** none
- **Acceptance evidence:** the report fields written by `Zenith_Bench_ECS` and `Zenith_Bench_ECSNative` in their `--smoke` runs in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Backends.md → "Adapter cost model"; Zenith2/Docs/Design/ECS/Tests.md → "Benchmarks and research scenarios"

### Z2-R-ECS-159 W-Dense once per backend

- **Statement:** W-Dense is measured on each ECS backend twice: `Zenith_Bench_ECSWorkload` runs the workload once per backend through `Zenith_RuntimeMain` with a hand-built composition (`Bench_ECS_WDense`), so every selectable backend is measured against the W-Dense rows, and `Zenith_Bench_BudgetsECS` runs its component mix natively (`Bench_ECSNative_<Backend>_WDense`).
- **Covers:** Z2-I-ECS-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_ECSWorkload --smoke` and `Zenith_Bench_BudgetsECS --smoke` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Benchmarks and research scenarios"; Zenith2/Docs/Design/ECS.md → "Native-path measurements of the ECS rows"

### Z2-R-ECS-160 W-Dense workload scale ruled in Stage 1

- **Statement:** The Stage 1 workload scale ticket for W-Dense commits `Zenith2/Docs/Research/Workloads/W-Dense.md` and the workload's scale fields, which the ECS harness and the WorkloadDense content follow.
- **Covers:** Z2-I-ECS-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the ruling ticket's diff and the `Coverage` step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/ECS.md → "Stage 1"

### Z2-R-ECS-161 Stage 1 ECS budget harness

- **Statement:** `Zenith_Bench_BudgetsECS` measures native micro-benchmarks of the W-Dense component mix read from `WDenseMix.json` for the four ECS rows and writes an evidence record, with a `--smoke` run.
- **Covers:** Z2-I-ECS-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-012
- **Budgets:** Z2-B-ECS-001, Z2-B-ECS-002, Z2-B-ECS-003, Z2-B-ECS-004
- **Acceptance evidence:** `Zenith_Bench_BudgetsECS --smoke` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Tests.md → "Stage 1 budget harness"; Zenith2/Docs/Design/ECS.md → "Rows ECS owns"

### Z2-R-ECS-162 ECS budget rows allocated

- **Statement:** The Stage 1 per-system budget ticket for ECS allocates Z2-B-ECS-001 to Z2-B-ECS-004 with their allocation file and DecisionLog entry, after the CORE, JOBS and RPROD rulings.
- **Covers:** Z2-I-ECS-11
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-ECS-001, Z2-B-ECS-002, Z2-B-ECS-003, Z2-B-ECS-004
- **Acceptance evidence:** the ruling ticket's diff, `Check-Allocation.ps1 -Area ECS` exit 0, and the `Coverage` step in acceptance mode `-Stage 1`
- **Design:** Zenith2/Docs/Design/ECS.md → "Rows ECS owns"

### Z2-R-ECS-163 Playback and storage within their rows

- **Statement:** Sync-point playback per tick stays within Z2-B-ECS-002 and component storage per world within Z2-B-ECS-001 in the W-Dense and frame workloads on every backend.
- **Covers:** Z2-I-ECS-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ECS-001, Z2-B-ECS-002
- **Acceptance evidence:** the workload evidence records of the Stage 2 mid-sprint integration check and of every subsequent stage acceptance (MP-CPUZONE, MP-SUBMEM)
- **Design:** Zenith2/Docs/Design/ECS.md → "Rows ECS owns"

### Z2-R-ECS-164 WorkloadDense content

- **Statement:** The focused workload-game ticket authors `Games/WorkloadDense` to the W-Dense scale ruling, and the game builds and boots headless on Windows and Android.
- **Covers:** Z2-I-ECS-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the `Build` step for `Games/WorkloadDense` on win-debug and android-arm64 and its headless boot test in the `Test` step
- **Design:** Zenith2/Docs/Design/ECS.md → "Stages 3 to 7"

### Z2-R-ECS-165 Justification records for custom infrastructure

- **Statement:** The container heap, the entity table and the flecs OS-API heap wrapper `Zenith_FlecsOsApi` each carry a justification record in `Zenith2/Docs/DecisionLog.md` with its benchmark evidence and acceptance criteria, appended provisionally (`Status: Provisional`, `Measured evidence: pending`) in the diff that adds the type and completed in place by the Stage 2 records ruling on the reference hardware.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** the DecisionLog entries in the slice diffs; `Bench_ECS_ContainerChurn` and `Bench_ECS_Churn` in `Zenith_Bench_ECS --smoke`; `Bench_ECSNative_Flecs_ChurnRootHeapOsApi` and `Bench_ECSNative_Flecs_ChurnDefaultOsApi` in `Zenith_Bench_ECSNative --smoke`
- **Design:** Zenith2/Docs/Design/ECS.md → "Justification records ECS adds"

### Z2-R-ECS-166 ECS configuration key set

- **Statement:** ZenithECS registers the configuration keys of the design with their types, ranges and defaults in the startup step-6 declaration phase; the composition root copies `ecs.maxEntitiesPerWorld`, `ecs.containerHeapBytes` and `ecs.chunkCapacityRows` into every `Zenith_WorldDesc`, `ecs.forceCompaction` compacts at every sync point, `ecs.debugDestroyEvents` makes world destruction emit one destroyed event per live entity in ascending simulation ID, and an out-of-range value is rejected with the key named.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** Z2-B-ECS-003, Z2-B-ECS-004
- **Acceptance evidence:** `Test_ECS_ConfigKeysMapToWorldDesc` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS.md → "Configuration keys"

### Z2-R-ECS-167 Cancellation leaves every world at a sync-point boundary

- **Statement:** A cancelled frame instance leaves every world with the storage, structure, epochs, entity table and query caches of its last completed sync point, plays back no buffer of an unfinished phase, advances no change version for uncommitted writes, and discards the leftovers at the next tick boundary with their count logged; an out-of-tick write before that boundary is stamped V(t, M) of the cancelled tick and seen by every `QUERY_TERM_CHANGED` system in the next tick.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_CancelledFrameConsistent` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS.md → "Cancellation"; Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"

### Z2-R-ECS-168 Version wrap sweep

- **Statement:** The end-of-tick playback of every tick that crosses a multiple of 2^29 merge points (and no other) re-stamps every column and row version more than 2^30 merge points old and resets every query slot whose last-seen version is that old, so no version comparison spans 2^31 merge points and a `QUERY_TERM_CHANGED` query never misses a change; its result differs from the result under unbounded versions only by extra matches in the next invocation of a reset slot, whose system completed no invocation for more than 2^30 merge points.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_VersionWrapSweep` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Version wrap sweep"; Zenith2/Docs/Design/ECS/Backends.md → "Change tables"

### Z2-R-ECS-169 Tick-boundary handlers change properties only

- **Statement:** Tick-boundary handlers reach the world only through `Zenith_TickBoundaryAccess`, which offers property reads and writes and no structural member, and their property writes are committed at the tick boundary's merge version before phase 1, with no epoch, structure-version or query-cache change.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_TickBoundaryPropertyWriteCommitted` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ECSObservers header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"

### Z2-R-ECS-170 Unschedulable system phases are rejected

- **Statement:** `Zenith_SystemRegistry::AddSystem` rejects a system with phase 12, `PHASE_COMMANDS_ONLY` or any value above 12 at the call with `ECS_ERROR_PHASE_NOT_SCHEDULABLE`, creating no registration index and returning an inert builder, and a reload scope's `AddSystem` or `ReplaceSystem` with such a phase fails `ZENITH_CHECK`.
- **Covers:** Z2-I-ECS-09, Z2-I-ECS-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SystemRegistry_UnschedulablePhase_Rejected` in the `Test` step (win-debug, win-release) and `Test_SystemReloadScope_UnschedulablePhase_Checks` in the `z2-death-check` suite in the `Test` step
- **Design:** Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Check order"

### Z2-R-ECS-171 Views carry only stamped merge versions

- **Statement:** A chunk view carries a merge version only from its invocation (a system context), its playback or 0: views from `Zenith_World::GetChunk` and `GetChunkOfEntity` are read-only in every column and their `CommitRowChanges` fails with `ECS_ERROR_UNKNOWN_PROPERTY` and no effect.
- **Covers:** Z2-I-ECS-06, Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_WorldLevelViewCommitDisabled` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ChunkView header"; Zenith2/Docs/Design/ECS/Queries.md → "Views and validation"

### Z2-R-ECS-172 Behaviour-state types register at startup step ten

- **Statement:** Every behaviour-state type of a game registers at startup step 10, after the schema blob and before `Freeze`, from the cooked script manifest through SCRIPT's entry, and afterwards only inside the hot-reload window; a registration before `BindStringTable` (the step-6 declaration phase) is `ZENITH_CHECK`, and one after `Freeze` outside the window returns `REFLECT_ERROR_FROZEN`.
- **Covers:** Z2-I-ECS-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_TypeRegistry_BehaviourStateAtStepTen` in the `Test` step (win-debug, win-release); `Test_TypeRegistry_BehaviourStateBeforeStringTable_Checks` in `z2-death-check`
- **Design:** Zenith2/Docs/Design/ECS.md → "Behaviour-state type registration"; Zenith2/Docs/Design/ECS/Reflect.md → "Registration algorithm"; Zenith2/Docs/Design/ECS/Reflect.md → "Revisions and the hot-reload window"

### Z2-R-ECS-173 Simulation-state restore resets change tracking

- **Statement:** `RestoreSimulationState` re-stamps every stored column and row version to the restored tick's last version, resets every query slot's last-seen and own versions to 0 and the sweep floor to the restored counter's, and every system's previous read version to 0, so every `QUERY_TERM_CHANGED` system sees every chunk once after a save load or replay checkpoint, no stored version lies ahead of the counter, and the next sweep runs at the first sweep boundary above the restored counter; a replay resumed from a save or checkpoint then reproduces the recorded tick hashes' ECS part from the first tick after the restore.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_RestoreResetsChangeTracking` and `Test_ECS_RestoreThenContinueMatchesUninterrupted` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"; Zenith2/Docs/Design/ECS/Scheduling.md → "Out-of-tick playback"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_WorldSimulation header"

### Z2-R-ECS-174 Previous read version for row-level change filters

- **Statement:** `Zenith_SystemContext::GetPreviousReadVersion()` returns the read version of the system's previous completed invocation, the same value in the node and every leaf, and 0 on its first invocation, after `RestoreSimulationState` and after a version wrap sweep resets it, so a row-level filter comparing `GetRowChangeVersions` against it never misses a committed row and needs no last-seen version of its own.
- **Covers:** Z2-I-ECS-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_PreviousReadVersion` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"; Zenith2/Docs/Design/ECS/Queries.md → "Change-filtered queries"; Zenith2/Docs/Design/ECS/Scheduling.md → "Disabled systems"

### Z2-R-ECS-175 Merge points per tick bounded

- **Statement:** `Zenith_SystemRegistryDesc::m_uMaxSystems` outside 1 … 65,536 is `ECS_ERROR_INVALID_DESC` at `Zenith_SystemRegistry::Create`, and `Freeze` fails, logging `ECS_ERROR_CAPACITY` and reporting one `SCHEDULE_ERROR_GRAPH` diagnostic, when a tick's merge-point count M (at most systems + arbitration nodes + 11) reaches 2^20, so every schedule that runs keeps the version wrap sweep's one boundary per tick and its 2^31 comparison bound.
- **Covers:** Z2-I-ECS-07, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SystemRegistry_MaxSystemsAndMergePointBound` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"; Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"

## Coverage of inventory items

| Inventory item | Item | Requirements (both catalogue files) |
|---|---|---|
| Z2-I-ECS-01 | stable type IDs | Z2-R-ECS-001, Z2-R-ECS-008, Z2-R-ECS-009, Z2-R-ECS-038, Z2-R-ECS-123 |
| Z2-I-ECS-02 | stable property IDs | Z2-R-ECS-010, Z2-R-ECS-038 |
| Z2-I-ECS-03 | schema evolution | Z2-R-ECS-011, Z2-R-ECS-026, Z2-R-ECS-038, Z2-R-ECS-043, Z2-R-ECS-044, Z2-R-ECS-045, Z2-R-ECS-046, Z2-R-ECS-047, Z2-R-ECS-048, Z2-R-ECS-049, Z2-R-ECS-050, Z2-R-ECS-055, Z2-R-ECS-176 |
| Z2-I-ECS-04 | component lifecycle | Z2-R-ECS-022, Z2-R-ECS-023, Z2-R-ECS-089, Z2-R-ECS-092, Z2-R-ECS-100, Z2-R-ECS-124, Z2-R-ECS-133, Z2-R-ECS-142, Z2-R-ECS-143, Z2-R-ECS-165, Z2-R-ECS-177 |
| Z2-I-ECS-05 | deferred structural changes | Z2-R-ECS-075, Z2-R-ECS-078, Z2-R-ECS-083, Z2-R-ECS-084, Z2-R-ECS-085, Z2-R-ECS-086, Z2-R-ECS-087, Z2-R-ECS-088, Z2-R-ECS-089, Z2-R-ECS-090, Z2-R-ECS-091, Z2-R-ECS-092, Z2-R-ECS-093, Z2-R-ECS-094, Z2-R-ECS-095, Z2-R-ECS-096, Z2-R-ECS-097, Z2-R-ECS-098, Z2-R-ECS-099, Z2-R-ECS-122, Z2-R-ECS-125, Z2-R-ECS-155, Z2-R-ECS-165, Z2-R-ECS-166, Z2-R-ECS-167, Z2-R-ECS-169, Z2-R-ECS-178, Z2-R-ECS-180 |
| Z2-I-ECS-06 | query invalidation | Z2-R-ECS-109, Z2-R-ECS-110, Z2-R-ECS-111, Z2-R-ECS-112, Z2-R-ECS-113, Z2-R-ECS-114, Z2-R-ECS-115, Z2-R-ECS-121, Z2-R-ECS-171 |
| Z2-I-ECS-07 | change tracking | Z2-R-ECS-115, Z2-R-ECS-116, Z2-R-ECS-117, Z2-R-ECS-118, Z2-R-ECS-119, Z2-R-ECS-120, Z2-R-ECS-168, Z2-R-ECS-171, Z2-R-ECS-173, Z2-R-ECS-174, Z2-R-ECS-175 |
| Z2-I-ECS-08 | world isolation | Z2-R-ECS-087, Z2-R-ECS-123, Z2-R-ECS-124, Z2-R-ECS-126, Z2-R-ECS-127, Z2-R-ECS-128, Z2-R-ECS-129, Z2-R-ECS-130, Z2-R-ECS-131, Z2-R-ECS-149, Z2-R-ECS-166, Z2-R-ECS-176 |
| Z2-I-ECS-09 | scheduling contracts | Z2-R-ECS-002, Z2-R-ECS-006, Z2-R-ECS-007, Z2-R-ECS-058, Z2-R-ECS-060, Z2-R-ECS-061, Z2-R-ECS-063, Z2-R-ECS-066, Z2-R-ECS-067, Z2-R-ECS-068, Z2-R-ECS-069, Z2-R-ECS-070, Z2-R-ECS-071, Z2-R-ECS-072, Z2-R-ECS-073, Z2-R-ECS-074, Z2-R-ECS-075, Z2-R-ECS-076, Z2-R-ECS-077, Z2-R-ECS-078, Z2-R-ECS-079, Z2-R-ECS-080, Z2-R-ECS-081, Z2-R-ECS-082, Z2-R-ECS-101, Z2-R-ECS-102, Z2-R-ECS-103, Z2-R-ECS-151, Z2-R-ECS-152, Z2-R-ECS-153, Z2-R-ECS-167, Z2-R-ECS-169, Z2-R-ECS-170, Z2-R-ECS-175, Z2-R-ECS-178, Z2-R-ECS-179, Z2-R-ECS-180 |
| Z2-I-ECS-10 | all-backend conformance | Z2-R-ECS-002, Z2-R-ECS-003, Z2-R-ECS-004, Z2-R-ECS-111, Z2-R-ECS-132, Z2-R-ECS-133, Z2-R-ECS-134, Z2-R-ECS-136, Z2-R-ECS-137, Z2-R-ECS-139, Z2-R-ECS-140, Z2-R-ECS-144, Z2-R-ECS-145, Z2-R-ECS-146, Z2-R-ECS-147, Z2-R-ECS-148, Z2-R-ECS-149, Z2-R-ECS-150, Z2-R-ECS-151 |
| Z2-I-ECS-11 | representative benchmarks | Z2-R-ECS-156, Z2-R-ECS-157, Z2-R-ECS-158, Z2-R-ECS-159, Z2-R-ECS-160, Z2-R-ECS-161, Z2-R-ECS-162, Z2-R-ECS-163, Z2-R-ECS-164 |
| Z2-I-ECS-12 | runtime type registration | Z2-R-ECS-001, Z2-R-ECS-006, Z2-R-ECS-012, Z2-R-ECS-013, Z2-R-ECS-014, Z2-R-ECS-015, Z2-R-ECS-016, Z2-R-ECS-017, Z2-R-ECS-018, Z2-R-ECS-019, Z2-R-ECS-054, Z2-R-ECS-172, Z2-R-ECS-181 |
| Z2-I-ECS-13 | data-defined component schemas (`*.schema.json` only) | Z2-R-ECS-021, Z2-R-ECS-024, Z2-R-ECS-025, Z2-R-ECS-026, Z2-R-ECS-027, Z2-R-ECS-030, Z2-R-ECS-031, Z2-R-ECS-032, Z2-R-ECS-033, Z2-R-ECS-034, Z2-R-ECS-035, Z2-R-ECS-036, Z2-R-ECS-037, Z2-R-ECS-039, Z2-R-ECS-040, Z2-R-ECS-041, Z2-R-ECS-042, Z2-R-ECS-053 |
| Z2-I-ECS-14 | `.d.luau` generated from data-defined schemas | Z2-R-ECS-051, Z2-R-ECS-052, Z2-R-ECS-053 |
| Z2-I-ECS-15 | data-defined components stored natively in all three backends | Z2-R-ECS-021, Z2-R-ECS-023, Z2-R-ECS-055, Z2-R-ECS-132, Z2-R-ECS-135, Z2-R-ECS-138, Z2-R-ECS-139, Z2-R-ECS-140, Z2-R-ECS-141, Z2-R-ECS-142, Z2-R-ECS-143, Z2-R-ECS-177 |
| Z2-I-ECS-16 | data-defined components identical to C++-registered types for every consumer | Z2-R-ECS-005, Z2-R-ECS-019, Z2-R-ECS-020, Z2-R-ECS-024, Z2-R-ECS-027, Z2-R-ECS-028, Z2-R-ECS-029, Z2-R-ECS-052, Z2-R-ECS-062, Z2-R-ECS-141, Z2-R-ECS-154, Z2-R-ECS-181 |
| Z2-I-ECS-17 | per-field phase-authority declarations | Z2-R-ECS-056, Z2-R-ECS-057, Z2-R-ECS-058, Z2-R-ECS-059, Z2-R-ECS-060, Z2-R-ECS-061, Z2-R-ECS-062, Z2-R-ECS-063, Z2-R-ECS-064, Z2-R-ECS-065, Z2-R-ECS-066, Z2-R-ECS-067, Z2-R-ECS-068, Z2-R-ECS-070, Z2-R-ECS-093, Z2-R-ECS-104, Z2-R-ECS-105, Z2-R-ECS-106, Z2-R-ECS-107, Z2-R-ECS-108, Z2-R-ECS-121, Z2-R-ECS-170 |

Requirements that also serve other areas' items remain owned by ECS: the schema part of Z2-I-SCRIPT-05 (generated `.d.luau`, whose data-defined part lands in Stage 2 in ECS) through the component Luau type requirements, and the ECS half of Z2-I-RESEARCH-03 (native against adapter measurement) through the benchmark requirements, whose methodology and report are RESEARCH's.

## Excluded and deferred

No ECS inventory item is excluded, deferred or game-side: all seventeen are required in Stage 2, the four Stage 1 requirements serve the Stage 1 format confirmation, workload scale and budget allocation, the two Stage 3 requirements author the W-Dense content and declare the edit-world flag (Z2-R-ECS-176), and the one Stage 4 requirement serves EDIT's schema re-registration flow. Out of ECS's scope, with the owner named: dynamic plugins and mod-defined components (excluded by Z2-D-075 and Z2-D-024); a second schema front end or reflection system (excluded by Z2-D-020); the phase contract itself (GAME, Z2-D-080); Luau transactions and script views (SCRIPT); the authoring-document migration framework and scene formats (SCENE and ASSET); the research methodology and its Stage 5 report (RESEARCH, Z2-D-064); whether EnTT stays a shipping backend (D-SEL-EnTT-SHIPPING, which exists only if the human approves flagged proposal P1, Z2-D-130).
