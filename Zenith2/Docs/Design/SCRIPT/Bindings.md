# SCRIPT design — bindings, views, environments and the module loader

Companion to `Zenith2/Docs/Design/SCRIPT.md`: the implementation of the binding table, the per-VM thunk tables, the dispatcher, the userdata view objects, marshalling, environment construction, the module loader, the `@zenith` and `@types` modules, the definition emitter and the low-library binding files. It elaborates `Zenith2/Docs/Scripting/Bindings.md` (every `##` there) and `Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types", and restates none of their bullets.

## Scope of this file

The binding ABI types are declared in `Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h` (Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"); their non-template members are in `ZenithReflect/Internal/Zenith_ScriptBindingTable.cpp`. Everything that touches the Luau stack is in `ZenithScript/Internal/` (exception-enabled).

## Binding table implementation

`Zenith_ScriptBindingTable` (one per process, owned by the composition root):

| Member | Contents |
|---|---|
| `m_axFunctions` | `Zenith_Array<Zenith_ScriptFunctionDesc>` in registration order; parameter and result descriptor arrays copied into `m_xDescArena` |
| `m_axEnums`, `m_axValueTypes`, `m_axConstants` | the registrations of `AddEnum`, `AddValueType`, `AddConstant` |
| `m_apxConsumers[SIDE_EFFECT_KIND_COUNT]` | the side-effect consumers (null for the ECS-carried kinds) |
| `m_auAsyncResultBytes[uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY]` | the declared maximum result bytes per `Zenith_AsyncSourceKind` value (32 entries, PublicAPI.md → "Zenith_ScriptBinding header"; `DeclareAsyncSource`, `SetAsyncResultDecoder` and `GetAsyncResultDecoder` of a kind at or above the capacity are `ZENITH_ASSERT`, engine code) |
| `m_apfnDecoders[uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY]` | the ticket-result decoders per source kind (null: none) |
| `m_szRegistrantLibrary` | the library name `SetRegistrantLibrary` last set (null before the first call); `AddFunction` copies it into `m_szLibrary` |
| `m_axNameTables` | `Zenith_Array` of `{ Zenith_StringView kind, Zenith_Span<const Zenith_StringView> names }` from `AddNameTable`, in declaration order; the views have static storage ([Z2-N-SCRIPT-922]); `FindNameTable` searches it by kind |
| `m_xByID` | after `Seal`: `Zenith_HashMap<uint64, uint32>` function ID → index |
| `m_bSealed` | set by `Seal` |

`AddFunction<pfnFunction>` is Scripting/Bindings.md → "What AddFunction generates"; its one compile-time check rejects a parameter or result type with no `Zenith_ScriptValueTraits` mapping, with a message containing `"has no Zenith_ScriptValueTraits specialisation"`. The thunk it instantiates is a function template specialisation `Zenith_ScriptThunkFor<pfnFunction>` in the caller's translation unit (exceptions off).

`Seal()` (main thread, startup, after every registration of steps 14 and 15):

1. Sort `m_axFunctions` by `(namespace, Luau name)` byte order; this order is the Luau table order and the `.d.luau` order of functions.
2. Validate each function: namespace and Luau name valid by rule 7 of Scripting/Bindings.md → "Rules"; context flags non-zero; `READ_WORLD` has at least one resource; `BUFFERED` has a non-zero side-effect mask whose SCRIPT-held kinds all have a consumer; its version members by the rules of § "Binding stability and deprecation" (a violation is `SCRIPT_BINDING_ERROR_INVALID_DESCRIPTOR`; the replacement check runs after step 3 builds `m_xByID`).
3. Build `m_xByID`; two functions with one ID, or one namespace with two equal Luau names, fail. Validate the name tables of [Z2-N-SCRIPT-922]: a parameter whose `m_szResolveKind` names no table fails with `SCRIPT_BINDING_ERROR_UNKNOWN_NAME_KIND`; two tables of one kind, or one table holding a name twice, fail with `SCRIPT_BINDING_ERROR_DUPLICATE_NAME_TABLE`.
4. On any failure: log each (category `script.bindings`, naming both registrants' namespaces and names) and return the first failure in sealed order as a `Zenith_ScriptBindingError` (`SCRIPT_BINDING_ERROR_DUPLICATE_ID`, `_DUPLICATE_NAME`, `_INVALID_NAME`, `_INVALID_DESCRIPTOR`, `_MISSING_CONSUMER`, `_UNKNOWN_NAME_KIND`, `_DUPLICATE_NAME_TABLE`; Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"); the table stays unsealed and the composition root stops with exit code 3.

## Binding stability and deprecation

Every function carries the Luau API classification of `Zenith2/Docs/Design/MAINT/Versioning.md` → "Luau API classification" (cross-area row MT2 (a)) in five descriptor members (PublicAPI.md → "Zenith_ScriptBinding header"): `m_uSinceStage`, `m_eStability`, `m_uExperimentalUntilStage` and `m_xDeprecation`, copied from `AddFunction`'s trailing `Zenith_ScriptFunctionVersion`, and `m_szLibrary`, copied from the table's current registrant library. The default version is stable since Z2 Stage 0. MAINT reads the effective since stage `max(since, 2)`, so a binding registered without a version is treated as shipped no later than its own stage's roll: of the two errors a missing version can make, this is the stricter, because it holds the binding to the deprecation window early, never late. A binding of Stage 3 or later passes its own since stage.

| Rule | Checked at `Seal` (each a `SCRIPT_BINDING_ERROR_INVALID_DESCRIPTOR`, logged naming the function) |
|---|---|
| B1 | `m_uSinceStage` ≤ 7 |
| B2 | experimental: `m_uSinceStage` ≤ `m_uExperimentalUntilStage` ≤ 7; any other stability: `m_uExperimentalUntilStage == uSCRIPT_API_STAGE_NONE` |
| B3 | not deprecated (`m_uDeprecatedStage == uSCRIPT_API_STAGE_NONE`): `m_szReplacement` null and `m_uRemovedStage == uSCRIPT_API_STAGE_NONE` |
| B4 | deprecated: stability `STABLE` (an experimental or internal binding changes without deprecation, MAINT's CLS-EXCLUSIVE); `m_uSinceStage` ≤ `m_uDeprecatedStage` ≤ 7; `m_uRemovedStage` is `m_uDeprecatedStage + 1`; `m_szReplacement` reads `Zenith.<Namespace>.<Name>` and names a sealed function that is not itself deprecated |

`SetRegistrantLibrary(szLibrary)` (main thread, before `Seal`) sets the CMake target name stamped on every later `AddFunction`. The composition root calls it before its own low-library bindings (`ZenithRuntime`) and before each module's `Start` with the module's library target name (cross-area row SC27 to MOD). `m_szLibrary` is reporting data: it appears in the binding listing and is not hashed, so moving a binding between libraries re-cooks nothing. A function registered while no library is set lists `"library": null`.

The four classification members are inputs of `Zenith_ScriptBindingSetHash`, appended to each function's hashed fields in this order: `m_uSinceStage`, `m_eStability`, `m_uExperimentalUntilStage`, `m_uDeprecatedStage`, `m_uRemovedStage`, then the replacement's bytes after a `uint32` length (0 for none). Deprecating a binding therefore re-cooks every module, and a cooked module's deprecated-call records always match the running table.

### Deprecated call sites

The deprecation diagnostic of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" (row "Luau-facing API": a load-time deprecation diagnostic per script and call site; cross-area rows SH14 and MT2 (c)) is produced in three steps that follow § "Load-time name references":

1. **Extraction.** `Zenith_ScriptExtractNameRefs`'s single AST walk also records, for each call whose callee `<Namespace>.<Function>` names a function of the sealed table with `m_uDeprecatedStage != uSCRIPT_API_STAGE_NONE`, one `{ function ID, line }`. It keeps the first such call on each line in AST pre-order: a line that calls two deprecated bindings reports the first, and the second is reported once the first is migrated. A call through a local alias records nothing, because the walk sees only direct `<Namespace>.<Function>` callees, as for name references.
2. **Manifest.** The records travel in `scripts.zlmf`'s deprecated-call block (Compiler.md → "Module manifest format"), sorted by (module ID, line); `Zenith_ScriptModuleManifest::GetDeprecatedCalls(xModule)` returns a module's records.
3. **Load.** After a module loaded successfully, `LoadModules` records one diagnostic per record in the world's diagnostic store and logs it at `LOG_LEVEL_WARNING` in category `script.bindings`. The diagnostic is `SCRIPT_BINDING_DEPRECATED_CALL`, code ZS1808, with the error fields `m_eCode = SCRIPT_ERROR_LOAD`, `m_eKind = SCRIPT_ERROR_KIND_LOAD`, `m_uCode = 1808`, `m_xModule` and `m_uLine`, and the message `<Module>:<Line>: ZS1808: Zenith.<Namespace>.<Function> is deprecated since Z2 Stage <D>; use <Replacement>; it is removed in Z2 Stage <Removed>`. `<Module>` is the manifest path; `<D>`, `<Replacement>` and `<Removed>` come from the descriptor `FindFunction` returns. It is a warning: the module loads and runs, no Luau error is raised and no invocation sees it. Each (module, line) is reported once per world load; a hot-reloaded generation is not re-reported, and the next world load reports it. The cumulative counter `script.bindings.deprecated_calls` counts the diagnostics. The catalogue row is cross-doc row X451 (`Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Diagnostics that are not Luau errors").

`Test_Script_Binding_DeprecatedCallDiagnosed` (Tests.md → "Module binding and sandbox tests") asserts the extraction, the record and the exact text.

## Binding listing

The `luau-types` step writes `${ZENITH_OUT}/Generated/Luau/<Game>/bindings.json` beside the definition files (cross-area row MT2 (d)), the input of MAINT's Luau surface rules (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Luau API classification"). The writer is `Zenith_ScriptEmitDefinitions` (§ "Definition emitter procedure", step 7), through CORE's `Zenith_JsonWriter` (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer") in its canonical style, so the bytes are a function of the sealed table alone. Like every file of that directory it is never tracked ([Z2-N-ARCH-208]).

```json
{
    "schema": "zenith2.script-binding-listing",
    "schemaVersion": 1,
    "bindingSetHash": "0x<16 lower-case hex digits>",
    "functions": [
        {
            "library": "ZenithPhysics",
            "namespace": "Physics",
            "name": "RayCast",
            "since": 3,
            "stability": "stable",
            "experimentalUntil": null,
            "deprecatedIn": null,
            "replacement": null,
            "removedIn": null
        }
    ]
}
```

| Member | Value |
|---|---|
| `bindingSetHash` | `Zenith_ScriptBindingSetHash` of the emission, the value of the `zenith.d.luau` header |
| `functions` | one record per function of the sealed table, every stability included, byte-ordered by (`namespace`, `name`), the sealed order |
| `library` | `m_szLibrary`, or `null` |
| `namespace`, `name` | `m_szNamespace`, `m_szLuauName` |
| `since` | `m_uSinceStage` as registered (MAINT applies the effective stage) |
| `stability` | `"stable"`, `"experimental"` or `"internal"` |
| `experimentalUntil` | `m_uExperimentalUntilStage`, or `null` |
| `deprecatedIn`, `replacement`, `removedIn` | `m_uDeprecatedStage`, `m_szReplacement`, `m_uRemovedStage`, each `null` when not deprecated |

Enums, value types and constants are not listed: MAINT's `luau[]` section lists binding functions alone.

## Per-VM thunk tables

At VM creation (Runtime.md → "VM creation steps" step 4) the VM builds, from the sealed table:

1. One Luau table per namespace (`Zenith.Physics`, `Zenith.Input`, …) under the engine's API table.
2. For each function whose contexts intersect the VM role's allowed contexts (a worker VM: `SYSTEM`; the main VM: `BEHAVIOUR | LOAD | TOOLS`), a C closure `Zenith_ScriptDispatch` with the descriptor pointer as light-userdata upvalue 1, stored under its Luau name; a function whose contexts exclude the role gets the stub closure `Zenith_ScriptContextStub` raising ZS1702 with its `<Namespace>.<Function>` and the context name, so the error is specific ([Z2-N-SCRIPT-108]).
3. Enums: a frozen table per enum mapping value name to itself (scripts use the names); constants: frozen fields of their namespace table.
4. The API table and every namespace table are deep-frozen and stored as the engine's native API table (`__zenith_native`), read by the generated `@zenith` prelude ([Z2-N-SCRIPT-623]).

Cost: O(functions) per VM creation; `Zenith_Bench_ScriptVM` measures worker-VM creation with the full engine binding set (Design/SCRIPT/Tests.md → "Benchmarks and harnesses").

## Dispatcher implementation

`Internal/Zenith_ScriptBindingDispatch.cpp` implements Scripting/Bindings.md → "Dispatcher algorithm" steps 1–9. Specifics:

| Step | Implementation |
|---|---|
| 1 | `Zenith_ScriptDispatchScope` records `xVM.m_xFrameArena.GetMarker()` and increments `m_uLiveDispatchScopes`; its destructor restores both (it runs during unwinding because the TU is exception-enabled) |
| 2 | the VM's current invocation kind is `m_eInvocationKind`, set by the runner at each invocation start; `SCRIPT_INVOCATION_KIND_LOAD` while a module scope runs |
| 3 | the running module's `Uses` set is a 64-bit mask of world-resource IDs indexed through the world's resource index table (built at load); a resource outside it raises ZS1703 |
| 4 | arity: `lua_gettop(L)` against `m_uParamCount` minus trailing optional parameters; ZS1510 |
| 5 | conversions by § "Marshalling implementation"; a failure calls `luaL_error(L, "ZS1511: %s expects %s for argument %u, got %s", …)` with the catalogue text |
| 6 | the call frame is on the C stack; `m_pxContext` is the VM's `Zenith_ScriptInvocationContextImpl` for the running invocation, null during module load; before the call the context stores the descriptor's `m_pUserData`, which `GetFunctionUserData()` returns for the duration of the thunk (core text: `Zenith2/Docs/Scripting/Bindings.md` → "What AddFunction generates" step 5, X346 applied by core-doc patch f; cross-area rows TD22 (a), AN10) |
| 7 | on `SCRIPT_THUNK_STATUS_ERROR`: push `xFrame.m_acErrorMessage`; set `m_eRaisedKind` to `Zenith_ScriptThunkErrorKind(xFrame.m_xError)`; an abort or fatal kind is raised as the Zenith abort value, otherwise `lua_error` with the string |
| 8 | results pushed in order; the count returned |

`Zenith_ScriptInvocationContextImpl` (one per VM, re-targeted per invocation) implements the ABI interface: `GetTick`, `GetDeltaTime`, `GetCurrentSimulationID`, `GetVMIndex` from the running entity transaction; `NextRandomDraw` (Runtime.md → "Random draws"); `AppendSideEffect` (Transactions.md → "Side-effect routing"); `InvokeCallback` (a nested `FUNCTION` protected call on the same VM thread: the registry reference is pushed, arguments marshalled, results converted back; an error returns `Zenith_ScriptThunkError` with `m_eCode = Zenith_ScriptThunkErrorCodeFromKind(kind)` and the sequence of the VM's error scratch and, for an abort or fatal kind, sets the outer VM's pending re-raise so the outer dispatcher re-raises after the engine function returns, [Z2-N-SCRIPT-048]); `RetainCallback` (marks the reference retained in the VM's retained-callback list owned by the invoking behaviour instance, released at `ReleaseCallback`, quarantine or teardown); `GetErrorMessage` (copies from the VM's error scratch when its sequence equals the error's, never from the store, so an in-tick read races no writer; Design/SCRIPT/Transactions.md → "Diagnostic store implementation").

A callback reference whose owner VM differs from the calling VM is refused with ZS1102-style misuse (`handle belongs to another world` for another world; ZS1001 for another VM of the same world), because a registry reference is meaningful in its own VM alone.

## Userdata tags and view objects

Every engine value userdata carries a Luau userdata tag (`lua_newuserdatatagged`), so type checks are one integer compare:

| Tag | Object | Payload | Metatable |
|---|---|---|---|
| 1 | row proxy (`Zenith.Row`) | `Zenith_ScriptViewHeader` + current row | `__index` resolves a component name to a component view (ZS1211 when undeclared) and `Entity` |
| 2 | component view | header + component binding index + row | `__index`/`__newindex` through the declaration's perfect-hash field table (Transactions.md → "Write path implementation") |
| 3 | fixed-array view | header + field binding + row | `__index`, `__newindex`, `__len` |
| 4 | container object (`Zenith.Container<T>`) | header + shadow index | method table of Scripting/Bindings.md → "Behaviour container API" |
| 5 | invocation context (`ctx`) | header | fields `Tick`, `Time`, `DeltaTime`, `Random`, methods `Emit`, `Contribute`, `Read`, `Command`, `Spawn`, `Destroy`, `View` (behaviours) and the module bindings' `ctx:` methods |
| 6 | state proxy | header + instance index | HotReload.md → "State proxy implementation" |
| 7 | `Zenith.Self` | header + instance index | `Entity`, `Get`, `TryGet`, `GetContainer`; `__newindex` raises ZS1208 |
| 8 | `Zenith.Entity` | 16-byte opaque entity handle (ECS traits) + world tag + provisional bit | `Id`, `IsValid`, `PersistentId`, `__eq` |
| 9 | `Zenith.Asset<T>` | opaque asset handle + asset type ID | `Id`, `IsLoaded` |
| 10 | `Zenith.PersistentId` | 16 bytes | `__eq`, `__tostring` (hex, no address) |
| 11 | `Zenith.Quat`, `Vec4`, `Colour` | 4 × f32 (immutable) | component accessors, `Rotate` |
| 12 | `Zenith.AsyncTicket` | `Zenith_ScriptTicket` | `IsReady`, `Result` (for a source kind with a result decoder, the decoded table of Runtime.md → "Asynchronous result table") |
| 13 | `Zenith.Random` | none (reads the VM's transaction) | Random API table |
| 14 | abort value | a light userdata, not a full userdata | recognised by the Zenith `pcall` and the protected entry |

The per-VM view objects of tags 1 and 5 are allocated once per VM and re-targeted per invocation (no allocation per entity); component views, containers and state proxies are allocated per access and die with the invocation's garbage. Every access validates the header first (Scripting.md → "Validation on every access"), in every build ([Z2-N-SCRIPT-443]).

Perfect-hash field tables: at load, for each declared component of a module, a table over the interned Luau field names of the declared fields (CHD construction over `lua_tostring` pointer identity, which is stable because Luau interns strings per VM and the table is rebuilt per VM at module load); lookup O(1) with a final pointer compare; an absent name raises ZS1202 (read) or ZS1201 (write), and an unknown name on the type raises ZS1210 ([Z2-N-SCRIPT-111]).

## Marshalling implementation

`Internal/Zenith_ScriptMarshal.cpp` implements Scripting/Bindings.md → "Value marshalling" row by row with the checks of that table and nothing else: integral check `f == floor(f)` and range by the C++ type's limits; `|v| ≤ 2⁵³` for 64-bit kinds; f32 overflow of a finite value raises ZS1503; quaternion renormalisation when `| |q| − 1 | > 10⁻⁴`, zero length ZS1505; enums through the enum's value-name table (ZS1506 listing names in declaration order); entity handles through `Zenith_ScriptValueTraits<Zenith_EntityHandle>` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSScriptTraits header") with the world-tag compare (ZS1102); asset handles through ASSET's trait with the type compare (ZS1507); structs field by field in property-ID order with missing and unknown keys reported (ZS1508). No conversion truncates, wraps or clamps ([Z2-N-SCRIPT-581]).

<!-- elaboration -->
A fixed-array field `T[N]` of a reflected value struct returned by value (PHYS's `Zenith.Physics.OverlapResult`, TWOD's `Zenith.TwoD.OverlapResult`; cross-area row TD22 (b)) reads as a frozen Luau array created with `lua_createtable(L, N, 0)`: element i (1-based) is element i − 1 converted by its own row of the table, and a null entity or asset handle element is left `nil`, so a script iterates `for i = 1, result.Count`; the `.d.luau` type is `{ T }`. On write (a struct argument), a table of at most N elements fills the field in order, absent and `nil` elements taking the field's default, and more than N elements raises ZS1509. `Zenith2/Docs/Scripting/Bindings.md` → "Value marshalling" carries the row (cross-doc item X346 (b), applied by core-doc patch f).

### Struct fields of a decoded ticket result

A ticket-result decoder (`Zenith_ScriptAsyncResultDecoder`) may fill a field with a `SCRIPT_VALUE_KIND_STRUCT` value whose `Zenith_ScriptStructValue::m_pData` points into the `xPayload` span it was given and whose type ID names a value struct registered with `AddValueType` (AI's `corners`, the reflected `Zenith.AI.PathCorners`; cross-area rows AI4 (e) and AIB7). This is how a result carries more than the eight scalar fields of `uSCRIPT_ASYNC_RESULT_MAX_FIELDS`. `Result()` converts such a field before it returns, by the struct row of the marshalling above: field by field in property-ID order into a fresh frozen Luau table, each fixed-array field a frozen 1-based array of its N elements (so a script reads `result.corners.Points[i]` for `i = 1 … N`), null handles left `nil`. The Luau value therefore holds a copy, never the pointer. The pointer need be valid only for the decoder call and the conversion that follows it on the same thread, which it is, because the payload stays in the published table until the reading invocation commits (Runtime.md → "Asynchronous result table", release of a script result). The table is read-only: a write raises Luau's `attempt to modify a readonly table`. A struct field whose type ID names no registered value type, or whose `m_pData` lies outside `xPayload` or extends past its end for the type's reflected size, makes `Result()` halt the world as an internal inconsistency (`SCRIPT_ERROR_KIND_INTERNAL`), as a decoder returning false does, because the payload came from the source's own producer or from the replay log that recorded it.

## Environment construction

Per VM role, at creation, inside the protected body of Runtime.md → "VM creation steps":

1. `luaL_openlibs(L)`.
2. Remove from the global table: `loadstring`, `load`, `dofile`, `loadfile`, `io`, `package`, and every `debug` member except `traceback` and `info`.
3. Replace, in both roles: `pcall`, `xpcall` (§ below), `tostring` (type name for tables, functions, threads and userdata without `__tostring`), `print` (development builds: the engine log at `LOG_LEVEL_INFO`, category `script.print`, prefixed with the module path, rate-limited to 64 lines per module per tick with a one-line overflow notice; Shipping: a no-op), `setmetatable` (refuses `__mode`, `__gc`, `__metatable` with ZS1601), `getmetatable` (`nil` for engine userdata and proxies), `require` (§ "Module loader implementation").
4. Stubs raising their catalogue codes: `rawset` ZS1602; `getfenv`, `setfenv` ZS1603; `collectgarbage`, `gcinfo` ZS1604; `newproxy` ZS1605; `math.random`, `math.randomseed` ZS1606; a fresh `os` table whose every member of the pinned `os` library is a stub raising ZS1607.
5. Coroutines: worker VMs replace every `coroutine` member with a stub raising ZS1301 (an abort kind, `YIELD_IN_SYSTEM`); the main VM keeps `isyieldable`, `running`, `status`, replaces `create`, `wrap`, `resume`, `close` with stubs raising ZS1303, and replaces `yield` with a function that raises ZS1302 unless the running thread is the current instance's `Run` coroutine at depth 0 of engine re-entry (a yield inside an `InvokeCallback` raises ZS1304).
6. Set the engine API table (`Zenith`) and the descriptor table (`__zenith_types_native`, built from the registry: one frozen descriptor per component, field and event, [Z2-N-SCRIPT-117]).
7. `luaL_sandbox(L)`.

The Zenith `pcall(f, …)`: calls `f` through `lua_pcall` inside the current protected region (this `lua_pcall` is in `Zenith_ScriptProtectedCall.cpp`, reached through an internal function pointer, so the one-site rule holds); on error, if the error value is the abort value or the VM's `m_eRaisedKind` is an abort or fatal kind, re-raise it unchanged ([Z2-N-SCRIPT-119]); otherwise return `false, message`. `xpcall` does the same and calls the handler only for catchable errors.

The environment tables are the ones of Scripting/Bindings.md → "Environments and library subsets"; every replaced or stubbed root is in `Zenith_ScriptSandboxMutableGlobals()` in the order of Scripting.md's table "Builtin fast path and mutable globals" ([Z2-N-SCRIPT-126], [Z2-N-SCRIPT-682]).

## Module loader implementation

`Internal/Zenith_ScriptModuleLoader.cpp` implements Scripting/Bindings.md → "Load time per VM" with these structures:

| Structure | Contents |
|---|---|
| per VM `m_axModuleCache` | module ID → (generation, registry reference of the frozen module table) |
| per VM `m_axLoadStack` | the in-progress module IDs (cycle detection, ZS1802 naming the cycle in stack order) |
| per world module state | current and staged generation, the validated declaration (Runtime.md → "Script world internals") |

Bytecode source: each manifest entry's cooked `.zlbc`, a non-asset cooked file listed in the catalogue's file entries, read once per world load into the world's bytecode arena (a `Zenith_Arena` reserved at load to the sum of `Zenith_AssetReader::GetVerifiedFileSize` over the modules, released at world destruction) with `Zenith_AssetReader::ReadVerifiedFile`, which verifies the file's recorded Hash128 (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetReader header"; ASSET's cross-area note AS8); `LoadModules` completes when every read operation succeeded, and a failed read disables that module like any load failure. Header verification is Compiler.md → "Load-time verification procedure". A worker VM's lazy load reads the same arena (read-only, shared), so a worker never performs I/O.

`require(literal)` from module scope: the literal is resolved by the cook to a module ID; the compiled bytecode carries the literal string, and the loader maps it through the manifest's `Find` (O(1)); ZS1801 for an unknown path, ZS1803 during an invocation (the VM's invocation kind is not `LOAD`). Deep freeze after load: an iterative walk (explicit stack in the frame arena, visited set keyed by table pointer, bounded by the module's table count) calling `table.freeze` on every reachable unfrozen table and on the metatables it reaches ([Z2-N-SCRIPT-106]).

Declaration validation on the returned value (Scripting.md → "Behaviour declaration", "Chunk system declaration"): keys checked against the tables of those sections; component and field descriptors resolved in the frozen registry; write sets expanded to properties (at most `uSCRIPT_MAX_DECLARED_WRITE_PROPERTIES`); `Uses` resolved to world-resource IDs; reductions to `Zenith_ReductionDesc`s; `Before`/`After` names checked against the manifest's system names; the state schema built and compared with the cooked record (HotReload.md → "State registration at startup"). Any problem raises ZS1806 `<Module> declaration: <Key> <Problem>` and the module fails to load.

## Load-time name references

[Z2-N-SCRIPT-922] resolves the string-constant arguments a module passes to a parameter marked with `m_szResolveKind` when the module loads, but a loaded module cannot be inspected for its call arguments, so the compiler records them:

1. **Extraction.** `Zenith_ScriptExtractNameRefs` (`ZenithScriptCompiler/Internal/Zenith_ScriptNameRefs.cpp`) walks the module's AST once. For each call whose callee is `<Namespace>.<Function>` naming a function of the sealed binding table, and for each parameter of that function with `m_szResolveKind` set whose argument is a string literal, it records `{ kind, name, line }`. A non-literal argument records nothing. The cook runs it at step 8 (Compiler.md → "Cook pipeline"); every in-process compile (the editor's hot reload, the console) calls the same function, so the cooked and live paths agree.
2. **Manifest.** The references travel in `scripts.zlmf`'s name-reference block (Compiler.md → "Module manifest format"), sorted by (module ID, kind, name, line).
3. **Load.** For each reference of a loading module, the loader calls `FindNameTable(kind)`. A missing table or name fails that module's load with `SCRIPT_ERROR_LOAD` and ZS1807 (`unknown input <Kind> '<Name>' in <Module>`, Scripting/MisuseCatalogue.md → "Modules"); the module is disabled and the world loads the rest. A found name's index in the table is stored in the module's internal `Zenith_ScriptLoadContext`, keyed by (kind, name). A hot-reload staging resolves the staged module's references, carried with the reload request, before go-live, and a failure rolls the staging back (HotReload.md → "Hot-reload staging area"; [Z2-N-SCRIPT-927]).
4. **Call.** The marshalling of a marked parameter (§ "Marshalling implementation") looks the argument up in the calling module's load context, then in `FindNameTable(kind)`, and passes the index to the thunk as `SCRIPT_VALUE_KIND_NUMBER`. A name in neither raises ZS1511 with `<Type>` read as `<kind> name`.

No binding owner sees a ZenithScript type: the owner declares names with `AddNameTable` and marks parameters, and the loader never calls back into it. Every name table is an input of `Zenith_ScriptBindingSetHash` (§ "Definition emitter procedure"), so a changed table re-cooks every module.

## The @zenith prelude and @types modules

| Module | Source | Contents |
|---|---|---|
| `@zenith/Core` | hand-written `Zenith2/Engine/ZenithScript/Scripts/Core.luau` | `Behaviour(decl)` and `ChunkSystem(decl)` (return a frozen declaration table tagged with its kind), `Field.*` (return frozen field-spec tables), `WaitTicks`, `WaitSeconds`, `WaitAsync` (yield the descriptors of Runtime.md → "Wait conditions") |
| `@zenith` | generated `Zenith.luau` | Scripting/Bindings.md → "Declaration shapes" (prelude); returns `__zenith_native` cast to the typed API |
| `@types` | generated `Types.luau` | returns `__zenith_types_native` cast to the typed descriptors |

`Core.luau` is a library module accepted by the stateless check (it holds only frozen constants and pure functions) and is compiled and cooked with every game like any module.

## Definition emitter procedure

`Zenith_ScriptEmitDefinitions` (`Internal/Zenith_ScriptTypesEmitter.cpp`), the body of ZenithCook's `luau-types` step and of the editor's regeneration on a registry change:

1. Compute the binding-set hash with `Zenith_ScriptBindingSetHash(xBindings, xTypes, Zenith_ScriptProductionCompileOptions())`.
2. `components.d.luau`: call ZenithReflect's `Zenith_EmitComponentLuauTypes(xTypes, hash, sink)` (`Zenith2/Docs/Design/ECS/Schemas.md` → "Luau type generation from schemas"), which orders types by stable name and fields by field ID ([Z2-N-ARCH-611]; cross-doc item X70 supersedes the Luau-name order of [Z2-N-SCRIPT-621] for reflected types).
3. `zenith.d.luau`: the header comment `-- generated; engine <szZENITH_ENGINE_VERSION>; binding-set hash 0x<16 hex digits>` (the engine version text of `ZenithCore/Zenith_EngineVersion.h`, `Zenith2/Docs/Design/MAINT/Versioning.md` → "Where the version is shown"; cross-area row MT2 (b)); the value classes of Scripting/Bindings.md → "Outputs" in the fixed order `ZenithEntity`, `ZenithQuat`, `ZenithVec4`, `ZenithColour`, `ZenithPersistentId`, `ZenithAsyncTicket`, `ZenithRandom`; then reflected enums and value structs in stable-name order; then `__zenith_native` and `__zenith_types_native`.
4. `Zenith.luau`: the prelude type exports of Bindings.md's table "Prelude type exports"; `type ZenithApi = typeof(Core) & { <namespace>: { <function>: <signature> } }` with namespaces and functions in `(namespace, Luau name)` byte order (the sealed table's order), each signature from the parameter and result descriptors (Luau parameter names from `m_szLuauName`, optional parameters with `?`), each preceded, for a deprecated function, by the line `--- deprecated Z2 Stage <D>, use <Replacement>, removed Z2 Stage <Removed>` and, for an experimental one, by `--- experimental until Z2 Stage <G>` (cross-area row MT2 (b)); enums as string-literal unions in stable-name order; constants.
5. `Types.luau`: the component and event entries from `Zenith_EmitComponentDescriptorTypes`, then the event payload `export type`s in stable-name order.
6. `.luaurc`: `{"languageMode":"strict","aliases":{…}}` with aliases in byte order: `game` → `<game>/Scripts`, `types` and `zenith` → the generated directory's modules, one per selected module `<module>` → that module's `Scripts/`; JSON through CORE's `Zenith_JsonWriter` (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"), so the bytes are canonical.
7. `bindings.json` (`SCRIPT_DEFINITION_FILE_BINDINGS_JSON`): § "Binding listing".
8. Name mapping (Scripting/Bindings.md → "Name mapping from Zenith Hungarian") runs over every C++-derived name; the first collision or invalid name aborts the whole emission with both sources in the diagnostic, and no file is written ([Z2-N-SCRIPT-116]).
9. Each file is passed to the sink once complete; the cook writes them to `${ZENITH_OUT}/Generated/Luau/<Game>/` and touches `.stamp` only when every file was written.

Byte stability: no timestamp, no path of the build machine, no hash-map iteration order anywhere in the output ([Z2-N-SCRIPT-621]); the engine version in the `zenith.d.luau` header is the build's constant, so two emissions of one build and registry are identical. `Test_Script_Bindings_DluauGolden` compares against a committed golden file from its second line on and asserts the first line against the header computed from `szZENITH_ENGINE_VERSION` and the hash, so the golden file does not change when the engine version does.

## Side-effect consumer registration

Consumers are registered on the binding table before `Seal`:

| Kind | Consumer | Registered by | Payload |
|---|---|---|---|
| `SIDE_EFFECT_KIND_AUDIO_TRIGGER` | the audio module's trigger queue | `Zenith_Module_Audio_Start` through `Zenith_ModuleStartContext` | the audio module's trigger record |
| `SIDE_EFFECT_KIND_VFX_TRIGGER` | the VFX module's spawn queue | `Zenith_Module_VFX_Start` | the VFX module's record |
| `SIDE_EFFECT_KIND_SAVE_REQUEST` | the runtime's save scheduler over ZenithSave | ZenithRuntime at step 14 | save slot and kind |
| `SIDE_EFFECT_KIND_ASYNC_REQUEST` | one consumer multiplexing by the payload's first byte (the source kind) to the path, streaming, physics-query and online submitters | each submitting module; ZenithRuntime owns the multiplexer | source kind, then the request |
| `SIDE_EFFECT_KIND_XR_RECENTRE` | ZenithXR's recentre request: the consumer records it for the world and `Zenith_XrSystem::RequestRecentre` applies it in the next frame prologue (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Script-facing API") | `Zenith_Module_XR_Start` through `Zenith_ModuleStartContext::SetScriptSideEffectConsumer` (Stage 7; cross-area rows SP7, SC28) | none (zero bytes); the consumer reads only the world tag |

`SIDE_EFFECT_KIND_XR_RECENTRE` is appended after `SIDE_EFFECT_KIND_CONTRIBUTION`, so every earlier value is unchanged and `SIDE_EFFECT_KIND_COUNT` becomes 13 (cross-doc row X450). It is a SCRIPT-held kind like the triggers: the record has no payload, the per-entity side-effect accounting charges one record, and `Seal` refuses a binding whose mask names it without a registered consumer.

A game that selects no audio module has no audio consumer; bindings that would append audio triggers are absent too (the module registers them), so a script cannot reach an unconsumed kind; `Seal` refuses a buffered binding whose SCRIPT-held kind has no consumer (`SCRIPT_BINDING_ERROR_MISSING_CONSUMER`).

<!-- assumption: pending ARCH -->
`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" gives `Zenith_ModuleStartContext::RegisterScriptBindings(const Zenith_BindingTable&)` only. This design assumes the context names the table `Zenith_ScriptBindingTable` and gains `SetScriptSideEffectConsumer(Zenith_SideEffectKind, Zenith_ScriptSideEffectConsumer&)` and `DeclareScriptAsyncSource(uint8, uint32)`, forwarding to the table's `SetSideEffectConsumer` and `DeclareAsyncSource` (cross-area row SC10; cross-doc row X117(c)).
<!-- /assumption -->

## Low-library binding files

Bindings of libraries below ZenithReflect live in `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/` (registered by the composition root, [Z2-N-SCRIPT-125]):

| File | Namespace | Functions (context, effect) |
|---|---|---|
| `Zenith_CoreScriptBindings.cpp` | `Zenith.Math` | deterministic maths helpers over CORE (`Lerp`, `Clamp`, `SmoothStep`, vector and quaternion constructors) — all contexts, `PURE` |
| `Zenith_TimeScriptBindings.cpp` | `Zenith.Time` | `TickHz` (all contexts, `PURE`) |
| `Zenith_InputScriptBindings.cpp` | `Input` | the table of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API" (PLAT specifies the functions; SCRIPT the ABI) |
| `Zenith_LogScriptBindings.cpp` | `Zenith.Log` | `Warn`, `Error` (development builds log; Shipping no-op) — all contexts, `PURE` from the simulation's point of view ([Z2-N-SCRIPT-178]) |

Unknown action or context names passed to `Input` functions are resolved when the module loads (PLAT); the error is a load-time misuse error. <!-- assumption: pending core-doc (X121) -->
The catalogue has no row for it. `ZS1807` (kind `LOAD`, trigger "an input action or context name the loaded bindings do not declare", text `unknown input <Kind> '<Name>' in <Module>`) is added to `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Modules" with cross-doc row X121, before or with the ticket landing `Zenith_InputScriptBindings.cpp` ([Z2-N-SCRIPT-843]).
<!-- /assumption -->
