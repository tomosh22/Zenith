# SCRIPT design — compiler, cook and the stateless check

Companion to `Zenith2/Docs/Design/SCRIPT.md`: the internals of `ZenithScriptCompiler` — the compile procedure and its guard, the fast-call listing, the cook pipeline ZenithCook runs, the cooked `.zlbc` and module-manifest formats, the static declaration extraction, the D-LUAU-STATELESS (open, Stage 1) prototype with its alternatives, and the address-keyed iteration lint. It elaborates `Zenith2/Docs/Scripting.md` → "The ZenithScriptCompiler library", "Stateless system enforcement" and "Shipping bytecode compatibility", `Zenith2/Docs/Scripting/Bindings.md` → "Module loader" and `Zenith2/Docs/Scripting/StatelessCorpus.md`, and restates none of their bullets. Files are under `Zenith2/Engine/ZenithScriptCompiler/` unless the path says otherwise.

## Scope of this file

`ZenithScriptCompiler` is the compile-time half of the exception firewall: it links `Luau.Ast` and `Luau.Compiler`, its `Internal/**` is the compile group `ZenithScriptCompiler_EH`, its matrix is `ZENITH_HOST`, and no Player links it ([Z2-N-SCRIPT-052], [Z2-N-SCRIPT-054]). Its consumers are ZenithCook, ZenithEditor and test targets.

## Compiler library internals

| File | Contents |
|---|---|
| `Internal/Zenith_ScriptCompilerGuard.{h,cpp}` | `Zenith_ScriptCompilerGuarded` — the compiler's one `catch` site ([Z2-N-SCRIPT-030]) |
| `Internal/Zenith_ScriptCompile.cpp` | `Zenith_ScriptCompileModule` (the one `luau_compile`/`Luau::compile` site), parser limits |
| `Internal/Zenith_ScriptFastcall.cpp`, `Internal/Zenith_ScriptBuiltinNames.inl` | `Zenith_ScriptListFastcallSites`, the one reader of `Luau/Bytecode.h`; the hand-maintained builtin-name table |
| `Internal/Zenith_ScriptAstUtil.{h,cpp}` | shared AST walking: scope resolution, local and upvalue binding, source positions |
| `Internal/Zenith_ScriptStateless.cpp` | `Zenith_ScriptCheckStateless` (the selected D-LUAU-STATELESS mechanism) |
| `Internal/Stateless/Zenith_StatelessAst.cpp`, `…Subset.cpp`, `…Isolation.cpp` | the three prototype mechanisms of § "Stateless alternatives prototypes" (Stage 1; after the ruling the unselected files are deleted in the ruling's follow-up ticket) |
| `Internal/Zenith_ScriptDeclarations.cpp` | `Zenith_ScriptExtractDeclaration` |
| `Internal/Zenith_ScriptLint.cpp` | `Zenith_ScriptLintModule` |
| `Internal/Zenith_ScriptCook.cpp`, `Internal/Zenith_ScriptCookWatch.cpp` | `Zenith_ScriptCookGame`, the file-watch loop |
| `Internal/Zenith_ScriptZlbcWriter.cpp`, `Internal/Zenith_ScriptManifestWriter.cpp` | the two cooked writers |
| `Internal/Zenith_ScriptCompileReport.cpp` | `Zenith_ScriptCompileReport` |

## Compile procedure

`Zenith_ScriptCompileModule(xSource, xOptions, xArena, pxReport)`:

1. Check the parser limits before parsing: `m_uSourceBytes ≤ uSCRIPT_MAX_SOURCE_BYTES` (1 MiB), else `SCRIPT_ERROR_COMPILE` "module '<path>' exceeds 1048576 bytes" ([Z2-N-SCRIPT-487]).
2. Inside `Zenith_ScriptCompilerGuarded`: parse with `Luau::Parser::parse` (`ParseOptions` with `allowDeclarationSyntax = false`, `captureComments = false`); a parse error becomes `SCRIPT_ERROR_COMPILE` with its line and column; walk the AST once to measure syntactic nesting (every nested block, expression and table constructor) and reject a depth above `uSCRIPT_MAX_NESTING` (200).
3. For a simulation module (kind system or behaviour, or a library module the manifest marks as required by one), require the first non-empty line to be `--!strict`, else `SCRIPT_ERROR_COMPILE` "simulation module '<path>' must start with --!strict" ([Z2-N-SCRIPT-102]).
4. Reject every `require` whose argument is not a single string literal ([Z2-N-SCRIPT-103]).
5. Compile with `Luau::compile` (the C++ API, so `CompileOptions` can carry `mutableGlobals`): `optimizationLevel = 2`, `debugLevel = 1`, `typeInfoLevel = 1`, `mutableGlobals = xOptions.m_aszMutableGlobals`, `vectorLib = "vector"`, `vectorCtor = "create"`, `vectorType = "vector"`; a `Luau::CompileError` becomes `SCRIPT_ERROR_COMPILE` with its location.
6. Copy the blob into `xArena`; fill `Zenith_ScriptBytecodeHeader` (Scripting.md → "Cooked bytecode format"): magic, container version 1, the blob's first byte, flags (bit 0 when the source's first lines carry `--!native`; bit 1 for simulation modules; bit 2 always, because debug level 1 records lines; bit 3 left clear — only the cook and hot-reload producers set it after the stateless check), the pin hash, engine version hash (`Zenith_ScriptEngineVersionHash()` in the cook, PublicAPI.md → "Zenith_ScriptCompileOptions header") and binding-set hash passed by the caller through the report's context, the source hash `Zenith_Hash64(source)`, the module ID.

A guard-caught non-Luau exception returns `SCRIPT_ERROR_INTERNAL`; nothing is written to `xArena` for a failed compile beyond what the arena's marker rewinds (the caller rewinds on failure).

## Compiler guard implementation

`Zenith_ScriptCompilerGuarded(pfnBody, pContext, pxReport)` — the internal three-parameter form in `Internal/Zenith_ScriptCompilerGuard.h`, Scripting.md's two parameters plus the caller's report (null when the caller keeps none) into which the converted error is written — wraps the body in `try { … } catch (const Luau::CompileError& x) { … } catch (const Luau::ParseErrors& x) { … } catch (const std::exception& x) { … } catch (...) { … }`; the first two become `SCRIPT_ERROR_COMPILE` with the first error's location and message written to the caller's report, the others `SCRIPT_ERROR_INTERNAL` ([Z2-N-SCRIPT-053]). The body functions are internal lambdas held by pointer and may throw; every public function of the library reaches Luau only through the guard, so no exception leaves it.

## Fast-call listing

`Zenith_ScriptListFastcallSites` decodes the blob of Scripting.md → "Development compilation" (third code block's comments): read the version byte, the string table, then each function prototype's instruction array; for `LOP_FASTCALL`, `LOP_FASTCALL1`, `LOP_FASTCALL2`, `LOP_FASTCALL2K` and `LOP_FASTCALL3` read the builtin ID operand and map it through `Zenith_ScriptBuiltinNames.inl` (one row per `LuauBuiltinFunction` enumerator of the pinned revision, `{ "math.floor" }` …). Import sites of replaced library members are not fast calls and are covered by `mutableGlobals` alone. Bounds: every read is checked against the blob size; a malformed blob or `uCapacity` too small returns `SCRIPT_ERROR_LOAD`. The builtin table is re-baselined on every Luau pin change as part of the pin re-qualification (Design/SCRIPT.md → "Stage 6").

## Cook pipeline

`Zenith_ScriptCookGame(xDesc, xReport)`, run by `ZenithCook --game <Name> --platform <p>` (the scripts part of the content cook) and by `--step luau-types` for types alone ([Z2-N-ARCH-207]). Every file the steps read or write, every directory they walk and the analyzer process of step 5 go through the desc's `Zenith_ScriptCookHost` on the calling thread (PublicAPI.md → "Zenith_ScriptCook header"), so the library's `Internal/` sources include only the allowed standard headers; the compile workers of step 9 are CORE's `Zenith_Thread`s and touch no host facility:

| # | Step | Output | Failure |
|---|---|---|---|
| 1 | Walk each source root through `ListFiles` (`game` = `Games/<Name>/Scripts`, `zenith` = `Engine/ZenithScript/Scripts`, one per selected module `<module>` = its `Scripts/`) for `*.luau`, excluding `Tests/`; manifest path = `@<alias>/<relative path without .luau>`; sort by byte order; module ID = index | module list | more than 8,192 modules (`uSCRIPT_MAX_MODULES_PER_GAME`) or a duplicate path: `SCRIPT_ERROR_COMPILE` |
| 2 | Generate the definitions (`Zenith_ScriptEmitDefinitions`) into `m_strGeneratedLuauDir` and add the generated `Zenith.luau` (`@zenith`) and `Types.luau` (`@types`) to the module list | `.d.luau`, `.luaurc`, two modules | name collision |
| 3 | Parse each module (compile procedure steps 1–4, without compiling) and resolve every `require` literal: `@zenith`, `@types`, `@<alias>/…`, `./…` relative to the requiring module's directory | the dependency lists | unresolvable path; cycle (reported `a -> b -> a`) |
| 4 | Classify each module's kind from its return expression: `return Zenith.Behaviour({…})` → behaviour, `return Zenith.ChunkSystem({…})` → chunk system, anything else → library (a declaration elsewhere in a library is rejected, [Z2-N-SCRIPT-101]); mark every library reachable from a simulation module as a simulation library | kinds | a declaration not at module return |
| 5 | Type-check: one `luau-analyze --mode=strict --definitions=zenith.d.luau --definitions=components.d.luau <files…>` process over the whole module set (out of process, [Z2-N-SCRIPT-115]); every reported error in a simulation module fails the cook; then one `luau-analyze --annotate` process per simulation module for the lint | analyzer output | type error |
| 6 | Lint each simulation module (`Zenith_ScriptLintModule`) | lint diagnostics | `SCRIPT_LINT_ADDRESS_KEYED_ITERATION` |
| 7 | Stateless check in dependency order (libraries first, so `xAccepted` is complete for importers) with each module's kind ([Z2-N-SCRIPT-356], [Z2-N-SCRIPT-357]) | the accepted set | any rejection |
| 8 | Extract each behaviour's and chunk system's declaration record (§ "Declaration extraction"), and each module's name references and deprecated call sites (Design/SCRIPT/Bindings.md → "Load-time name references", "Deprecated call sites") | state-schema and system records, name references, deprecated-call records | a non-static declaration |
| 9 | Compile each module (compile procedure) with `Zenith_ScriptProductionCompileOptions()` on `m_uThreadCount` threads, each with its own arena and report; set flag bit 3 on every module that passed step 7 | blobs | compile error |
| 10 | Encode each `.zlbc` (§ "Cooked bytecode container writer") and the manifest (§ "Module manifest format") and hand them to the host's `WriteFile` under `m_strOutputDir` (the host replaces each atomically), the manifest last | the cooked scripts | I/O error |

Steps 3–10 run over every module; a failure in any step is reported per module into the report and the cook fails after step 9 without writing anything (step 10 runs only when every module passed). Incremental cook: a module whose source hash, dependency closure hashes, binding-set hash and compile options are unchanged since the previous cook keeps its `.zlbc` (the `.zlbc` header is the record of those inputs), which is how `Zenith_ScriptCookResult::m_uRecompiled` stays small.

File-watch mode (`m_bWatch`): after the full cook, the cook polls the source roots every 100 ms through the host (`ListFiles` for added and removed modules, `GetModifiedTimeNs` per module, `Zenith_SleepUs` between polls) with a 100 ms debounce, re-runs steps 3–10 for the changed modules and their importers, and writes the changed `.zlbc` files and the manifest; a running development Player picks them up through its loose-mount file watcher (Design/SCRIPT/HotReload.md → "Development reload sources"). A module whose declaration's access sets changed is written like any other; the Player's reload path recompiles the schedule ([Z2-N-SCRIPT-153]).

## Cooked bytecode container writer

The `.zlbc` layout is `Zenith2/Docs/Scripting.md` → "Cooked bytecode format" (48-byte header, then the blob); every header field is written little-endian field by field (Z2-D-038). File path: `<m_strOutputDir>/<alias>/<relative path>.zlbc`, for example `Scripts/game/Village/Door.zlbc`, which the content cook mounts as `content:/Scripts/game/Village/Door.zlbc`.

### Load-time verification procedure

The runtime's loader (Design/SCRIPT/Bindings.md → "Module loader implementation") decodes a `.zlbc` and refuses it with `SCRIPT_ERROR_KIND_LOAD`, code ZS1805, naming the first mismatched field in this order ([Z2-N-SCRIPT-504]):

| # | Check | Field named |
|---|---|---|
| 1 | file at least 48 bytes; magic `ZLBC` | `magic` |
| 2 | container version 1 | `containerVersion` |
| 3 | blob bytes equal file size − 48 | `blobBytes` |
| 4 | Luau bytecode version byte equals the blob's first byte and lies in `LBC_VERSION_MIN` … `LBC_VERSION_MAX` of the pinned VM | `luauBytecodeVersion` |
| 5 | Luau pin hash equals the running build's | `luauPinHash` |
| 6 | engine version hash equals the running build's (`Zenith_ScriptEngineVersionHash()`) | `engineVersionHash` |
| 7 | binding-set hash equals the running build's (computed once at startup) | `bindingSetHash` |
| 8 | module ID equals the manifest entry's | `moduleId` |
| 9 | for a simulation module, flag bit 3 set | `statelessVerified` |

The runtime never runs refused bytecode and never translates it ([Z2-N-SCRIPT-505]). Bytecode is accepted only from the package and patch mounts in Shipping, and additionally from the loose mount in development builds ([Z2-N-ARCH-957], [Z2-N-ARCH-962]); the asset system has verified the content hashes of those mounts before the loader reads them ([Z2-N-SCRIPT-486]).

## Module manifest format

`<m_strOutputDir>/scripts.zlmf` (`content:/Scripts/scripts.zlmf`), little-endian, field by field:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | magic `ZLMF` |
| 4 | 2 | format version (1) |
| 6 | 2 | flags (0) |
| 8 | 8 | engine version hash |
| 16 | 8 | Luau pin hash |
| 24 | 8 | binding-set hash |
| 32 | 4 | module count M |
| 36 | 4 | dependency count D |
| 40 | 4 | string block bytes S |
| 44 | 4 | state-schema record count R |
| 48 | 4 | state-schema field count F |
| 52 | 4 | system record count Y |
| 56 | 8 | Zenith_Hash64 of every byte after offset 64 |
| 64 | M × 40 | module entries: ID u32, kind u8, 3 zero bytes, path hash u64, path offset u32, path bytes u32, first dependency u32, dependency count u32, bytecode asset index u32, 4 zero bytes |
| … | D × 4 | dependency module IDs |
| … | S | string block (UTF-8 paths, names, enum value names; no terminators) |
| … | R × 48 | state-schema records: module ID u32, name offset u32, name bytes u32, state version u32, first field u32, field count u32, max serialized bytes u32, reserved u32, shape hash u64, source ID u64 |
| … | F × 32 | state fields: field ID u32, parent index u32 (0xFFFFFFFF top level), kind u8 (the "Declared state schema" kinds in table order, 0 = Bool), element kind u8, flags u8 (bit 0 has explicit ID, bit 1 renamed), reserved u8, max count u16, max bytes u16, name offset u32, name bytes u32, default offset u32, default bytes u32 (defaults in the checkpoint payload encoding, in the string block) |
| … | Y × 24 | system records: module ID u32, phase u8, kind u8 (chunk system 0, behaviour 1), reserved u16, name offset u32, name bytes u32, source ID u64 |

**Deprecated-call block** (cross-area row MT2 (c); Design/SCRIPT/Bindings.md → "Deprecated call sites"). Format version 3 appends one block after the name-reference block: `u32 N`, then N × 16 bytes (module ID u32, line u32, function ID u64), sorted by (module ID, line), at most one record per (module ID, line). `Load` refuses a version below 3 (cooked data re-cooks, never migrates), bounds-checks the block like every other, and refuses a record whose module ID is out of range or whose (module ID, line) repeats or is out of order (ZS1805 with field `manifest`); it exposes a module's records through `GetDeprecatedCalls`. The cook's step 8 fills the block from the same AST walk that extracts the name references.

`Zenith_ScriptModuleManifest::Load` validates the magic, version, every count against the remaining size, every offset and length against its block, the trailing hash, sorted unique paths, dependency IDs in range and acyclic, and field parent indices in range and acyclic; any failure is `SCRIPT_ERROR_LOAD` (ZS1805 with field `manifest`). The loader builds the path hash table (open addressing, capacity the next power of two ≥ 2M) at load; nothing of it is serialized. Engine, pin and binding-set hashes are compared with the build's at load, so a stale manifest fails before any module is read. `Zenith_Fuzz_Script_Manifest` fuzzes `Load`.

The bytecode asset index is the `.zlbc`'s index in the cooked catalogue's file-entry table (the non-asset cooked files ASSET lists, `Zenith2/Docs/Design/ASSET/Formats.md`); the loader reads the file by its virtual path (§ "Cooked bytecode container writer") with `Zenith_AssetReader::ReadVerifiedFile`, which checks its recorded hash in every mount, loose development mounts included.

## Declaration extraction

`Zenith_ScriptExtractDeclaration` reads a behaviour or chunk-system module's return expression statically (no VM runs at cook time) and emits its manifest records. The declaration table must be a table constructor literal; within it:

| Key | Static form required | Extracted |
|---|---|---|
| `Name` | string literal, `Area.Name` PascalCase segments | name, source ID `Zenith_Hash64(name)` |
| `Phase` | `"Early"` or `"Late"` | phase |
| `StateVersion` | integer literal ≥ 1 | state version |
| `State` | a table constructor whose values are calls `Zenith.Field.<Kind>(<literal args>)`, with nested `Table`, `Array`, `Map` specs as literals, each optionally followed by `:Id(n)` or `:RenamedFrom("Old")` on the spec | the state fields (IDs: explicit or `Zenith_Hash64(name) & 0xFFFFFFFF`), maxima, defaults encoded |
| every other key | any expression | nothing (validated at load) |

A `State`, `Name`, `Phase` or `StateVersion` that is not in its static form fails the cook with `SCRIPT_ERROR_COMPILE` "declaration <Key> of '<path>' must be a literal (Scripting design: cooked state schemas)". The maximum serialized size is computed from the maxima (checkpoint format of Scripting.md → "Checkpoint format"); a schema whose maximum exceeds Z2-B-SCRIPT-015 fails the cook with the load error text of [Z2-N-SCRIPT-385]. Field ID reuse across the schema's history is checked against the schema lock of the previous cook (`${ZENITH_OUT}/Cooked/<Name>/<platform>/Scripts/state_ids.json`, carried forward between cooks) and against `RenamedFrom` chains ([Z2-N-SCRIPT-383]). The shape hash is `Zenith_Hash64` over the field records in order; the runtime recomputes it from the live declaration at load and refuses a mismatch with ZS1806 `<Module> declaration: State differs from the cooked schema; re-cook`.

The static form is a restriction the Scripting examples already follow (`Village.Door`'s declaration is literal); it exists so behaviour-state types can register from the cooked manifest at startup step 10, before the registry freezes and before any VM runs (Design/SCRIPT/HotReload.md → "State registration at startup").

## Stateless check prototype

D-LUAU-STATELESS (open, Stage 1) selects the mechanism; this section is the prototype design of the harness ticket, whose code lives under `Internal/Stateless/` and whose evaluation is `Zenith2/Docs/Scripting/StatelessCorpus.md` → "Alternative evaluation procedure". The primary prototype is alternative 4 of Scripting.md → "Decision D-LUAU-STATELESS" in the shape "3 as the load-time rejection plus 1's data-flow analysis": a restricted-subset check whose admissible constructs are defined by an AST data-flow pass. It runs on the Luau AST (`Luau::AstStatBlock`) at cook and at every hot-reload compile, never at runtime.

### Pass structure

1. **Scope build.** Resolve every `AstLocal` to its declaring scope: module scope (depth 0) or a function scope. A local of module scope captured by a function is a *module upvalue*.
2. **Module-scope value classification.** For each module-scope local and each field of the returned declaration, classify the initialiser: `Constant` (number, string, boolean, nil, `vector.create` of constants); `FrozenTable` (a call `table.freeze(<table constructor of Constant/FrozenTable/Function values>)`, recursively); `Function` (a function expression or local function); `Require` (a `require` of an accepted module); `ImportedMember` (field access on a `Require` value); `Mutable` (anything else: a plain table constructor, a `buffer.create`, a binding call returning userdata, `setmetatable`, `coroutine.*`, any other call). A plain table constructor whose every reachable value is Constant/FrozenTable/Function and which is never mutated (step 4) is reclassified `FrozenAtLoad`, because the loader deep-freezes tables reachable from the module's return at load; tables reachable only through upvalues are not reachable from the return and stay `Mutable`.
3. **Reject at declaration** (diagnostics of Scripting/StatelessCorpus.md → "Diagnostic codes"): a module-scope `buffer.create` or binding userdata (`MUTABLE_USERDATA`); `setmetatable` with `__mode`/`__gc` or a metamethod closing over a module upvalue (`METATABLE_STATE`); `coroutine.create`/`wrap` at module scope (`COROUTINE_CAPTURE`); `getfenv`/`setfenv`/`_G` anywhere (`ENVIRONMENT_ACCESS`); `math.random`, `math.randomseed`, `os.*` referenced anywhere (`STATEFUL_LIBRARY`); an assignment to a global name anywhere (`GLOBAL_WRITE`).
4. **Mutation analysis inside functions** (every function body, nested closures included): an assignment to a module upvalue (`UPVALUE_WRITE`, detail naming the upvalue and its declaration line — this includes writes from nested closures, pattern 22); an index assignment `x[k] = v` or `x.f = v`, or a call of a mutating library function (`table.insert`, `table.remove`, `table.clear`, `table.sort`, `table.move` into it, `buffer.write*`, `rawset`) whose target expression *may alias* module data (`MODULE_SCOPE_MUTATION` for a direct module upvalue, `NESTED_TABLE_MUTATION` for a field path below one, `ALIAS_ESCAPE` for an aliased local).
5. **May-alias.** A flow-insensitive, intra-module points-to relation over locals: a local assigned from a module upvalue, a field of one, `select(i, …)` of one, a function parameter that receives one at any call site of a local function, or the return of a local function that returns one, *may alias* module data. Passing a module upvalue (or an alias) to any function outside the non-mutating allowlist (`type`, `typeof`, `tostring`, `ipairs`, `pairs`, `next`, `select`, `rawget`, `rawequal`, `rawlen`, `unpack`, `table.find`, `table.concat`, `string.*`, `math.*`, `bit32.*`, `vector.*`, `buffer.read*`, `buffer.len`, and the engine's `PURE`/`READ_WORLD` bindings) is `ALIAS_ESCAPE`; storing one into a side effect (`ctx:Emit` payload, a command value, a state field) is `ALIAS_ESCAPE` (`SHARED_INSTANCE_TABLE` for a behaviour's state or `self`).
6. **Dynamic access.** An index of module data by a non-constant key, or `rawget(M, <non-constant>)`, is `DYNAMIC_ACCESS` unless the value read is used only as a Constant (read of a frozen constant table with a computed key is allowed when the table is `FrozenTable`/`FrozenAtLoad`).
7. **Imports.** A `Require` of a module not in `xAccepted` is `IMPORTED_STATE` naming it; calling an `ImportedMember` function of an accepted library is safe by construction (the library passed the same check as a library).
8. **Behaviours** (kind behaviour): steps 1–7 apply to module scope and every callback; in addition, a module-scope table (even frozen) stored into `state` or `self` is `SHARED_INSTANCE_TABLE` ([Z2-N-SCRIPT-360]).

The verdict is the first rejection in source order (line, column); every rejection goes to the report. Complexity: O(n) scope build and classification, O(n × a) alias propagation to a fixed point (a = aliases per local, small), bounded by the 1 MiB source limit; measured by the evaluation's load-time criterion.

### Soundness argument and its limits

The pass is conservative by construction: data reachable from module scope is either immutable (constants, frozen or frozen-at-load tables, functions whose upvalues are themselves immutable) or rejected when any function could write it, alias it into a mutating call, or leak it. What it cannot see is flow through Luau values it does not model: metatables of engine userdata (the engine's are frozen and hidden by `getmetatable`), `debug` (absent), and string-keyed access through `rawget` on a table that is not module data (allowed, because it cannot reach module data). The adversarial corpus is the evidence; the bypass-resistance criterion adds ten reviewer-written modules after the prototype freezes ([Z2-N-SCRIPT-801]).

## Stateless alternatives prototypes

The harness ticket builds all three evaluable mechanisms behind the one interface of Scripting.md → "Load-time interface", selected by an internal switch the benchmark sets:

| Prototype | File | Mechanism | Load-time component |
|---|---|---|---|
| AST data-flow (alternative 1) | `Internal/Stateless/Zenith_StatelessAst.cpp` | § "Stateless check prototype" steps 1–7 without the subset restriction of step 2 (any initialiser allowed, only mutation reachable from module scope rejected) | the pass itself |
| restricted subset (alternative 3) | `Internal/Stateless/Zenith_StatelessSubset.cpp` | step 2's classification with `Mutable` rejected outright at declaration, plus a syntactic ban on upvalue assignment and on passing upvalues outside the allowlist; no alias analysis | the pass itself |
| isolated environments (alternative 2) | `Internal/Stateless/Zenith_StatelessIsolation.cpp` (the load-time half only) | re-run the module chunk in a fresh environment before each invocation, so no state survives | requires a load-time companion (the subset check) to reject at load, per [Z2-N-SCRIPT-352] |
| combination (alternative 4, the primary prototype) | `Internal/Zenith_ScriptStateless.cpp` | subset classification plus AST alias analysis | the pass |

No runtime mode for alternative 2 exists in `ZenithScript`: a benchmark-only execution path inside a library that ships would be a test hook in a shipping target. Its runtime cost is measured by the benchmark over the public API instead, because re-running a module chunk before each invocation costs exactly one module-scope execution per invocation: `Zenith_Bench_ScriptStateless` times `Zenith_ScriptWorld::LoadModules` of each representative module alone in a fresh world, subtracts the loader's fixed cost measured with an empty module of the same kind, and multiplies the difference by that module's per-tick invocation count on W-Dense; the load-time half is `Zenith_StatelessIsolation.cpp`'s subset check.

`Zenith2/Benchmarks/ScriptStateless/` (`Zenith_Bench_ScriptStateless`) measures every criterion of StatelessCorpus.md's table for each prototype over the corpus and the representative corpus and writes `Zenith2/Docs/Research/ScriptStateless/Report.md` with raw data, 10 repetitions, medians and 95% confidence intervals; the ruling ticket records the values and the selection ([Z2-N-SCRIPT-353], [Z2-N-SCRIPT-801]).

## Address-keyed iteration lint

`Zenith_ScriptLintModule(xSource, strAnnotated, xReport)` enforces [Z2-N-SCRIPT-285]:

1. Find every generic `for` (`AstStatForIn`) whose iterator is `pairs(e)`, `next, e` or a bare `e` (generalized iteration), recording the line and column of `e`.
2. Read `e`'s inferred type from `luau-analyze --annotate` output: the annotated source prints every local and parameter with its inferred type; for `e` a local or parameter, its annotation; for `e` a field path `a.b.c`, the annotation of `a` resolved through the printed table type's field `b`, then `c`.
3. Accept when the key type is `number`, `string`, `boolean`, a string-literal union, or the table type is an array `{T}`; reject with `SCRIPT_LINT_ADDRESS_KEYED_ITERATION` (module, line, inferred key type) when the key type is a table, function, thread, userdata or a declared engine class (`ZenithEntity`, …); reject with detail `cannot infer the key type; annotate the table` when step 2 cannot determine it, which strict mode makes rare and which the author fixes with a type annotation.

The annotated output format is the pinned `luau-analyze`'s; a pin change re-baselines `Test_Script_Determinism_AddressKeyedIterationLint`.

## Compile diagnostics report

Every compiler, check, lint and cook function writes `Zenith_ScriptDiagnostic` entries into the caller's `Zenith_ScriptCompileReport` (up to 64, the rest counted): the error with its code (`SCRIPT_ERROR_COMPILE`, `SCRIPT_ERROR_LOAD`), the module ID, line and column, and the message in the shapes of this file and StatelessCorpus.md. ZenithCook prints each as `<host path>:<line>:<column>: <message>` so IDEs link to it, then the count of dropped diagnostics; ZenithEditor shows them in its script panel (EDIT).
