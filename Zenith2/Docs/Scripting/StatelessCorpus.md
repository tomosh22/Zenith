# Zenith2 Scripting — the D-LUAU-STATELESS corpus

Companion to `Zenith2/Docs/Scripting.md` (its "Stateless system enforcement" section is the top-level contract; this file carries the corpus and the evaluation procedure of D-LUAU-STATELESS, open, Stage 1). Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Corpus layout

```
Zenith2/Engine/ZenithScript/Tests/Corpus/Stateless/
  manifest.json                     one entry per module: file, kind, expected verdict, expected diagnostic, pattern number
  Adversarial/
    A01_UpvalueCounter.luau
    A02_UpvalueTableWrite.luau
    …                               one file per row of "Adversarial corpus"
    Lib/                            library modules the adversarial systems require
  Accepted/
    P01_ConstantsAndPureHelpers.luau
    …                               one file per row of "Accepted corpus"
    Lib/
```

`manifest.json` entry shape:

```json
{
	"file": "Adversarial/A05_ImportedTableMutation.luau",
	"kind": "system",
	"expect": "reject",
	"diagnostic": "SCRIPT_STATELESS_IMPORTED_STATE",
	"pattern": 5,
	"line": 9
}
```

- [Z2-N-SCRIPT-721] Every corpus module carries a manifest entry with its kind, its expected verdict, and for a rejection its expected diagnostic and line; `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` compares the verdict, diagnostic and line of each rejection exactly.
- [Z2-N-SCRIPT-722] Each adversarial module is minimal: it exercises one pattern and is otherwise acceptable, so a rejection can be attributed to that pattern.
- [Z2-N-SCRIPT-723] Every adversarial pattern except 13, 14 and 19 (weak tables, environment access and module-scope coroutines, which have no legitimate equivalent in simulation code) has an accepted counterpart that achieves a similar effect legitimately (through a component, an event, a reduction or a local), so the mechanism is shown to reject the state and not the feature.

## Adversarial corpus

| File | Kind | Pattern | Excerpt | What it exploits | Expected diagnostic |
|---|---|---|---|---|---|
| `A01_UpvalueCounter.luau` | system | 1 | `local n = 0` … `Each = function(e, ctx) n += 1 end` | an upvalue survives invocations and differs per worker VM | `SCRIPT_STATELESS_UPVALUE_WRITE` |
| `A02_UpvalueTableWrite.luau` | system | 2 | `local cache = {}` … `cache[e.Entity:Id()] = e.Stats.Stamina` | a table held in an upvalue is not frozen at load (it is created at load but written afterwards) | `SCRIPT_STATELESS_MODULE_SCOPE_MUTATION` |
| `A03_NestedModuleTable.luau` | system | 3 | `local M = { Tuning = { Values = {} } }` … `M.Tuning.Values[1] = x` | shallow `table.freeze` | `SCRIPT_STATELESS_NESTED_TABLE_MUTATION` |
| `A04_GlobalWrite.luau` | system | 4 | `Counter = (Counter or 0) + 1` | the sandbox thread proxy accepts a global write at runtime | `SCRIPT_STATELESS_GLOBAL_WRITE` |
| `A05_ImportedTableMutation.luau` + `Lib/Util05.luau` | system | 5 | `local Util = require("./Lib/Util05")` … `Util.Cache[k] = v` | state held in an imported module's table | `SCRIPT_STATELESS_IMPORTED_STATE` |
| `A06_ImportedStatefulFunction.luau` + `Lib/Util06.luau` | system | 6 | `Util.Next()` where `Util06` does `local i = 0; function M.Next() i += 1 return i end` | the callee mutates its own upvalue | `SCRIPT_STATELESS_IMPORTED_STATE` (the library itself is rejected with `SCRIPT_STATELESS_UPVALUE_WRITE`) |
| `A07_ClosureFactory.luau` | system | 7 | `local function makeCounter() local c = 0 return function() c += 1 return c end end; local inc = makeCounter()` | state in a closure's upvalue reached indirectly | `SCRIPT_STATELESS_UPVALUE_WRITE` |
| `A08_ModuleScopeBuffer.luau` | system | 8 | `local scratch = buffer.create(64)` … `buffer.writeu32(scratch, 0, x)` | a mutable buffer held at module scope | `SCRIPT_STATELESS_MUTABLE_USERDATA` |
| `A09_ModuleScopeBindingUserdata.luau` | system | 9 | `local tmp = Zenith.Scratch.New()` at module scope, mutated in `Each` | engine-provided mutable userdata at module scope | `SCRIPT_STATELESS_MUTABLE_USERDATA` |
| `A10_MathRandom.luau` | system | 10 | `math.randomseed(ctx.Tick); local r = math.random()` | the hidden library generator | `SCRIPT_STATELESS_STATEFUL_LIBRARY` |
| `A11_OsClock.luau` | system | 11 | `if os.clock() % 2 < 1 then … end` | wall-clock input | `SCRIPT_STATELESS_STATEFUL_LIBRARY` |
| `A12_MetamethodMemo.luau` | system | 12 | `local memo = {}; local P = setmetatable({}, { __index = function(_, k) memo[k] = k * 2 return memo[k] end })` | mutation hidden in a metamethod | `SCRIPT_STATELESS_METATABLE_STATE` |
| `A13_WeakTable.luau` | system | 13 | `local seen = setmetatable({}, { __mode = "k" })` | GC-dependent contents | `SCRIPT_STATELESS_METATABLE_STATE` |
| `A14_Getfenv.luau` | system | 14 | `getfenv(1).x = 1` | environment smuggling | `SCRIPT_STATELESS_ENVIRONMENT_ACCESS` |
| `A15_TableInsertUpvalue.luau` | system | 15 | `local log = {}` … `table.insert(log, e.Entity:Id())` | mutation through a library call | `SCRIPT_STATELESS_MODULE_SCOPE_MUTATION` |
| `A16_LocalAlias.luau` | system | 16 | `local cache = {}` … `local t = cache; t[k] = v` | an upvalue aliased through a local | `SCRIPT_STATELESS_ALIAS_ESCAPE` |
| `A17_HelperMutatesParam.luau` | system | 17 | `local function put(t, k, v) t[k] = v end; local cache = {}` … `put(cache, k, v)` | inter-procedural mutation of a module-scope table | `SCRIPT_STATELESS_ALIAS_ESCAPE` |
| `A18_DynamicKeys.luau` | system | 18 | `local M = {}` … `rawget(M, "Ca" .. "che")` then mutating the result; `cache[string.format("%d", i)] = v` | dynamic keys defeat name-based checks | `SCRIPT_STATELESS_DYNAMIC_ACCESS` |
| `A19_ModuleCoroutine.luau` | system | 19 | `local gen = coroutine.wrap(function() local i = 0 while true do i += 1 coroutine.yield(i) end end)` resumed in `Each` | state in a suspended coroutine frame | `SCRIPT_STATELESS_COROUTINE_CAPTURE` |
| `A20_EscapeIntoEvent.luau` | system | 20 | `local shared = {}` … `ctx:Emit(T.Events.Probe, { Data = shared })`, `shared.x = 1` | a module-scope table escaping into engine-held data | `SCRIPT_STATELESS_ALIAS_ESCAPE` |
| `A21_SharedInstanceTable.luau` | behaviour | 21 | `local SHARED = {}` … `OnCreate = function(self, state) state.Ref = SHARED end` | one module-scope table referenced by every instance | `SCRIPT_STATELESS_SHARED_INSTANCE_TABLE` |
| `A22_UpvalueInNestedClosure.luau` | system | 1 | a module-scope `local n` written inside a closure created inside `Each` | upvalue writes one closure level down | `SCRIPT_STATELESS_UPVALUE_WRITE` |
| `A23_SelectAlias.luau` | system | 16 | `local t = select(1, cache)` then `t.k = v` | aliasing through a built-in that returns its argument | `SCRIPT_STATELESS_ALIAS_ESCAPE` |
| `A24_BehaviourModuleCounter.luau` | behaviour | 1 | `local spawned = 0` incremented in `OnCreate` | per-module state shared by all instances | `SCRIPT_STATELESS_UPVALUE_WRITE` |

- [Z2-N-SCRIPT-741] The adversarial corpus contains at least the 24 modules of the table "Adversarial corpus", every one of which is rejected at load time with its listed diagnostic by whichever alternative D-LUAU-STATELESS selects.

## Accepted corpus

| File | Kind | Counterpart of | Shape |
|---|---|---|---|
| `P01_ConstantsAndPureHelpers.luau` | system | 1, 7 | module-scope numeric and string constants, frozen constant tables, pure local helper functions |
| `P02_LocalScratchTable.luau` | system | 2, 15 | a table created inside `Each` and mutated freely (`table.insert` on a local) |
| `P03_FrozenTuning.luau` | system | 3 | `local TUNING = table.freeze({ Values = table.freeze({ 1, 2, 3 }) })` read in `Each` |
| `P04_ContextTime.luau` | system | 4, 11 | time from `ctx.Tick`, `ctx.Time`, `ctx.DeltaTime` |
| `P05_PureLibrary.luau` + `Lib/Pure05.luau` | system | 5, 6 | a required library of pure functions and frozen constants |
| `P08_LocalBuffer.luau` | system | 8, 9 | `buffer.create` inside `Each`, used and dropped |
| `P10_CounterRandom.luau` | system | 10 | `ctx.Random:NextFloat()` and `ctx.Random:Range(1, 6)` |
| `P12_LocalMetatable.luau` | system | 12 | a metatable with `__index` over a local table, created inside `Each` |
| `P16_LocalAliasOfLocal.luau` | system | 16, 17 | aliasing and passing a local table to a helper that mutates it |
| `P18_DynamicLocalKeys.luau` | system | 18 | dynamic string keys on a local table |
| `P20_EventPayloadLocal.luau` | system | 20 | an event payload built from a fresh table inside `Each` |
| `P21_StatePerInstance.luau` | behaviour | 21, 24 | per-instance data in declared `State` fields; cross-instance data through events |
| `P30_Reduction.luau` | system | — | contributions through `ctx:Contribute` |
| `P31_CommandsAndSpawns.luau` | system | — | commands, spawns and destroys through `ctx` |
| `P32_RepresentativeCombat.luau` | system | — | a 300-line representative gameplay system using every allowed construct |

- [Z2-N-SCRIPT-761] Every module of the accepted corpus loads and runs under the selected mechanism, and the representative gameplay modules of the Stage 3 and Stage 4 games load unchanged; a false rejection of an accepted module is a defect of the mechanism, not of the module.

## Diagnostic codes

| Diagnostic | Meaning | Detail text shape |
|---|---|---|
| `SCRIPT_STATELESS_UPVALUE_WRITE` | a function assigns an upvalue that outlives the invocation | `upvalue '<name>' (declared at line <n>) assigned in <function>` |
| `SCRIPT_STATELESS_MODULE_SCOPE_MUTATION` | a module-scope table is mutated after load | `module-scope table '<name>' mutated by <operation>` |
| `SCRIPT_STATELESS_NESTED_TABLE_MUTATION` | a table nested in module-scope data is mutated | `nested table '<path>' mutated` |
| `SCRIPT_STATELESS_GLOBAL_WRITE` | a global is assigned | `global '<name>' assigned` |
| `SCRIPT_STATELESS_IMPORTED_STATE` | a required module was rejected, or state reached through it is mutated | `module '<path>' is not stateless: <inner diagnostic>` |
| `SCRIPT_STATELESS_MUTABLE_USERDATA` | a mutable userdata or buffer is held at module scope | `module-scope <type> '<name>' is mutable` |
| `SCRIPT_STATELESS_STATEFUL_LIBRARY` | a removed stateful library function is referenced | `<function> is stateful; use <replacement>` |
| `SCRIPT_STATELESS_METATABLE_STATE` | a metatable or metamethod carries state, or `__mode`/`__gc` is set | `metatable of '<name>' carries state` |
| `SCRIPT_STATELESS_ENVIRONMENT_ACCESS` | `getfenv`/`setfenv` or `_G` is used | `environment access through <name>` |
| `SCRIPT_STATELESS_COROUTINE_CAPTURE` | a coroutine is created at module scope or survives an invocation | `coroutine '<name>' held at module scope` |
| `SCRIPT_STATELESS_ALIAS_ESCAPE` | a module-scope value is aliased, passed to a function outside the non-mutating allowlist, or stored in a side effect | `module-scope '<name>' escapes through <site>` |
| `SCRIPT_STATELESS_DYNAMIC_ACCESS` | module-scope data is reached through a key the checker cannot resolve | `dynamic access to module-scope '<name>'` |
| `SCRIPT_STATELESS_SHARED_INSTANCE_TABLE` | a behaviour module places module-scope data into instance state or `self` | `module-scope '<name>' shared across instances` |

- [Z2-N-SCRIPT-781] The diagnostic enumerators of the table "Diagnostic codes" are stable identifiers used by the corpus manifest; a mechanism may emit a more specific detail text, never a different enumerator for a corpus module.

## Alternative evaluation procedure

The D-LUAU-STATELESS (open, Stage 1) harness ticket (Z2-D-084: an ordinary harness ticket under `Zenith2/Benchmarks`, then a needs-human ruling ticket) evaluates each alternative of Scripting.md → "Decision D-LUAU-STATELESS" by the criteria below. Because `ZenithScriptCompiler/Internal` is private to its library and its tests, the measurements run in `ZenithScriptCompiler_Tests` in report mode, and `Zenith_Bench_ScriptStateless` under `Zenith2/Benchmarks/ScriptStateless/` is the statistics and report tool that turns their output into the ruling's report:

| Criterion | Measurement | Acceptance threshold |
|---|---|---|
| adversarial rejection | fraction of the adversarial corpus rejected at load with the expected diagnostic | 100% |
| false rejection | modules of the accepted corpus and of the representative corpus rejected | 0 |
| load-time cost | check time per 1,000 lines of Luau, median of 10 runs on the reference desktop | reported; the ruling weighs it against the W-Editor script hot-reload row |
| runtime cost | per-invocation overhead on W-Dense's chunk systems, interpreted, 1 and 8 workers; 0 by construction for a mechanism that acts at load time alone; reported "not measurable before Stage 3" for an isolated-environment alternative, whose overhead needs the Stage 3 world runtime | reported; any non-zero runtime cost needs the ruling's justification, and the ruling may reject an alternative whose runtime cost is not measurable on the load-time-rejection criterion alone ([Z2-N-SCRIPT-352]) |
| diagnostic quality | every rejection names the variable and line | 100% |
| bypass resistance | a reviewer-authored round of 10 new adversarial modules written after the prototype freezes | every one rejected, or added to the corpus with a mechanism fix before the ruling |

- [Z2-N-SCRIPT-801] The ruling ticket records, for each alternative, the measured values of every criterion in the table "Alternative evaluation procedure" (for runtime cost, the measured value, 0 by construction, or "not measurable before Stage 3" as that row states), and the selected alternative meets every threshold.
- [Z2-N-SCRIPT-802] After the ruling, the corpus is permanent: `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` and `Test_Script_Stateless_AcceptedCorpusLoads` run in every `Test` step, and a new bypass found afterwards is added to the adversarial corpus in the same diff that fixes the mechanism.
