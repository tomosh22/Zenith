# Zenith2 Scripting — misuse catalogue

Companion to `Zenith2/Docs/Scripting.md` (its "Views epochs and misuse errors" section is the top-level contract). Every script misuse raises a controlled Luau error listed here, and no script misuse is handled by an assert. Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Message format

A catalogued error's Luau error value is the string:

```
<chunkname>:<line>: ZS<code>: <text>
```

- `<chunkname>` is the module's manifest path (`@game/Combat/RegenerateStamina`), and `<line>` the line of the script frame that made the offending call; both are added by Luau's error position rule (`luaL_error` at level 1).
- `<text>` is the catalogue text with each `<Placeholder>` substituted: component, field, system, module and function names by their Luau names; numbers in decimal; stable simulation IDs in decimal.
- `Zenith_ScriptError::m_uCode` carries the numeric code; `m_eKind` the kind in the table's Kind column.

- [Z2-N-SCRIPT-821] The text of every catalogued error is exactly the Text column of its row with placeholders substituted; a change to a text is a doc change in the same diff as the code, and the code stays.
- [Z2-N-SCRIPT-822] Kind `MISUSE` errors are catchable by script `pcall`; the abort kinds (`BUDGET`, `SNAPSHOT_CAP`, `YIELD_IN_SYSTEM`, `CHECKPOINT`) are re-raised by the Zenith `pcall`; the fatal kinds (`RESERVATION`, `VM_MEMORY`, `WATCHDOG`) halt the world.

## Code ranges

Codes are allocated in blocks of 100. SCRIPT's own catalogue below holds ZS1000–ZS1999; every module or area whose bindings raise catalogued errors holds one block of its own, assigned in this table in the diff that adds that module's bindings or reserved earlier through a cross-document row ([Z2-N-SCRIPT-926]):

| Block | Owner | Codes in use |
|---|---|---|
| ZS1000–ZS1999 | SCRIPT (this catalogue, sections "Views and context" to "Luau-native errors") | the rows below |
| ZS2000–ZS2099 | ANIM (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Misuse codes"), reserved before its bindings diff | ZS2001–ZS2013 |
| ZS2100–ZS2199 | UI (`Zenith2/Docs/Design/UI.md` → "Luau errors and asserts"), reserved before its bindings diff | ZS2101–ZS2106 |
| ZS2200–ZS2299 | AUDIO (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes"), reserved before its bindings diff | ZS2201–ZS2208 |
| ZS2300–ZS2399 | PHYS (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Script bindings"), assigned by MISUSE-BLOCKS-2 | ZS2301–ZS2305 |
| ZS2400–ZS2499 | TWOD (`Zenith2/Docs/Design/TWOD.md` → "Luau errors and asserts"), assigned by MISUSE-BLOCKS-2 | ZS2401, ZS2405–ZS2409 (ZS2402–ZS2404 unassigned) |
| ZS2500–ZS2599 | AI (`Zenith2/Docs/Design/AI/Scripting.md` → "Misuse codes"), assigned by MISUSE-BLOCKS-2 | ZS2501–ZS2509 |
| ZS2600–ZS2699 | PACK (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Script-facing API"), assigned by MISUSE-BLOCKS-2 | ZS2601–ZS2603 |
| ZS2700–ZS2799 | CINE (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Misuse codes"), assigned by MISUSE-BLOCKS-2 and reserved before its bindings diff | ZS2701–ZS2712 |
| ZS2800–ZS2899 | CHAR (`Zenith2/Docs/Design/CHAR.md` → "Luau errors and asserts"), reserved by MISUSE-BLOCKS-2; CHAR has no Luau binding, so a future script-visible CHAR surface takes its codes here | none |
| ZS2900–ZS2999 | ONLINE (`Zenith2/Docs/Design/ONLINE/PublicAPI.md` → "Misuse codes"), assigned by MISUSE-BLOCKS-2 | ZS2901–ZS2906 (invalid online identifier, steps out of range, increment out of range, invalid slot name, invalid conflict choice, invalid conflict token) |
| ZS3400–ZS3499 | NET (`Zenith2/Docs/Design/NET/PublicAPI.md` → "Misuse codes"), reserved by MISUSE-BLOCKS-NEW-AREAS-3; ZS3405 and above held for the Stage 7 bindings `Net.Connect`, `Net.Broadcast`, `Net.RewoundRaycast` | ZS3401–ZS3404 (`Net.FindEntity` argument out of range, `Net.SetOwner` on a client, an entity without NetIdentity, `Net.SetOwner` peer out of range) |
| ZS4100–ZS4199 | GAME (`Zenith2/Docs/Design/GAME/Scripting.md`) | ZS4101–ZS4117 |
| ZS5100–ZS5199 | SAVE (`Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Script-facing API") | ZS5101–ZS5103, ZS5105, ZS5106 (ZS5104 retired) |
| ZS5600–ZS5699 | VFX (`Zenith2/Docs/Design/VFX/PublicAPI.md`) | ZS5601–ZS5602 |

- [Z2-N-SCRIPT-926] A misuse code belongs to exactly one block of the table "Code ranges", and a code is raised only by the block's owner; a module takes the next unassigned block (ZS2000–ZS9999, never inside ZS1000–ZS1999) by adding its row in the diff that adds its first catalogued binding error, or earlier through a cross-document reservation that a core-doc patch applies to this table (the first reservation applied wins a contested block; the bindings diff then confirms the row), lists its codes in that row, and keeps their rows in its own design; a design citing codes in a block another owner holds is in conflict with this table, which wins, and takes a free block of its own.

## Catalogue

### Views and context

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1001 | MISUSE | a view, container object or `ctx` used outside the invocation that created it | `view used outside its invocation; views are valid only during the callback that received them` |
| ZS1002 | MISUSE | a behaviour view used in a subsequent coroutine segment | `view used after coroutine.yield; views never survive a yield` |
| ZS1003 | MISUSE | a view whose chunk's structural epoch changed | `view refers to a chunk whose structure changed at a sync point` |
| ZS1004 | MISUSE | `self` used after its instance was quarantined or destroyed | `behaviour instance is no longer active` |

### Handles

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1101 | MISUSE | a stale entity handle (destroyed entity, generation mismatch) passed to a binding that needs a live entity | `entity handle is stale: entity <SimId> no longer exists` |
| ZS1102 | MISUSE | an entity or asset handle from another world | `handle belongs to another world` |
| ZS1103 | MISUSE | a provisional spawn handle used after its one-tick remap window | `provisional entity handle expired; store the handle returned after the spawn is played back` |
| ZS1104 | MISUSE | `nil` passed where an entity is required | `<Function> expects an Entity for argument <Index>, got nil` |
| ZS1105 | MISUSE | a `Zenith.AsyncTicket` still provisional after its one-tick remap window | `asynchronous ticket expired before its request was committed; request again` |

### Field access

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1201 | MISUSE | write to a field outside the declared write set | `write to <Component>.<Field> is not in <System>'s declared write set` |
| ZS1202 | MISUSE | read of a field outside the declared read and write sets | `read of <Component>.<Field> is not in <System>'s declared read set` |
| ZS1203 | MISUSE | a chunk system writing another entity's row | `chunk systems write the current entity's row only (<Component>.<Field>)` |
| ZS1204 | MISUSE | a cross-entity read of a field column written in this phase | `cannot read another entity's <Component>.<Field>: the column is written in phase <Phase>` |
| ZS1205 | MISUSE | a string, entity or asset field written through a chunk view | `<Component>.<Field> is read-only in chunk views; change it with ctx:Command` |
| ZS1206 | MISUSE | a container field accessed through a chunk view | `container field <Component>.<Field> is not exposed in chunk views; use the behaviour API` |
| ZS1207 | MISUSE | a direct write to a field whose owning phase is not the current phase | `<Component>.<Field> is owned by phase <OwnerPhase>; write it with ctx:Command` |
| ZS1208 | MISUSE | assigning a field on `self` | `self is not extensible; keep per-instance data in state` |
| ZS1209 | MISUSE | a behaviour writing a view of another entity | `behaviours write their own entity only; use ctx:Command for <Component>.<Field>` |
| ZS1210 | MISUSE | an unknown field name on a component view | `<Component> has no field '<Name>'` |
| ZS1211 | MISUSE | `self:Get` or `ctx:View` of a component type not in the declaration | `<Component> is not declared in <Module>'s Reads or Writes` |
| ZS1212 | MISUSE | `self:Get` of a declared component the entity does not carry | `entity <SimId> has no <Component>; use self:TryGet` |

### Coroutines

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1301 | YIELD_IN_SYSTEM | any `coroutine` function called in a chunk system | `coroutine.yield is not allowed in a chunk system` |
| ZS1302 | MISUSE | `coroutine.yield` or `Zenith.Wait*` outside the instance's `Run` coroutine | `yield is allowed only inside the behaviour's Run coroutine` |
| ZS1303 | MISUSE | `coroutine.create`, `wrap`, `resume` or `close` in a behaviour | `coroutine.<Function> is not available; the engine owns the behaviour coroutine` |
| ZS1304 | MISUSE | a yield from inside a callback invoked by a binding | `cannot yield inside a callback invoked by <Function>` |

### Budgets, capacity and fatal events

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1401 | BUDGET | the safepoint count exceeds the entity transaction's limit | `script budget exhausted: <Count> safepoints (limit <Limit>) in <Module> for entity <SimId>` |
| ZS1402 | SNAPSHOT_CAP | the per-entity transaction memory would exceed its cap | `per-entity transaction memory cap exceeded: <Bytes> bytes (limit <Limit>) in <Module> for entity <SimId>` |
| ZS1403 | RESERVATION (fatal) | a transaction arena beyond its pre-reserved size cannot grow from the general heap | `transaction arena growth failed: <Bytes> bytes for <Purpose> in <Module>` |
| ZS1404 | CHECKPOINT | the behaviour state cannot be serialized into its slot | `state checkpoint failed for <Module> entity <SimId>: <Reason>` |
| ZS1405 | VM_MEMORY | the VM allocator refuses an allocation | `script VM memory cap exceeded (<Bytes> of <Limit> bytes)` |
| ZS1406 | WATCHDOG | the wall-clock watchdog tripped | `script watchdog: invocation of <Module> ran <Milliseconds> ms (limit <Limit> ms)` |

### Value marshalling

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1501 | MISUSE | a non-boolean written to a boolean field | `<Target> expects boolean, got <LuauType>` |
| ZS1502 | MISUSE | a non-integral number written to an integer field | `<Target> expects an integer, got <Value>` |
| ZS1503 | MISUSE | a number out of the field's range | `<Value> is out of range for <Target> (<CppType>)` |
| ZS1504 | MISUSE | a 64-bit field whose value exceeds 2⁵³ read or written | `<Target> holds a value beyond 2^53 that Luau numbers cannot represent exactly` |
| ZS1505 | MISUSE | a zero-length quaternion written | `<Target> expects a non-zero quaternion` |
| ZS1506 | MISUSE | an invalid enum name | `'<Value>' is not a valid <Enum>; expected one of <Names>` |
| ZS1507 | MISUSE | an asset handle of the wrong asset type | `<Target> expects Asset<<AssetType>>, got Asset<<GivenType>>` |
| ZS1508 | MISUSE | a struct table with a missing or unknown key | `<Target> expects field '<Name>'` / `<Target> has no field '<Name>'` |
| ZS1509 | MISUSE | an index out of range on a fixed array or container | `index <Index> is out of range 1..<Count> for <Target>` |
| ZS1510 | MISUSE | a binding called with too few or too many arguments | `<Function> expects <Expected> arguments, got <Given>` |
| ZS1511 | MISUSE | a binding argument of the wrong type (raised by the dispatcher's argument conversion) | `<Function> expects <Type> for argument <Index>, got <LuauType>` |

### Sandbox

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1601 | MISUSE | `setmetatable` with `__mode`, `__gc` or `__metatable` | `setmetatable: '<Key>' is not supported in Zenith scripts` |
| ZS1602 | MISUSE | `rawset` called | `rawset is not available in Zenith scripts` |
| ZS1603 | MISUSE | `getfenv` or `setfenv` called | `<Function> is not available in Zenith scripts` |
| ZS1604 | MISUSE | `collectgarbage` or `gcinfo` called | `<Function> is not available; garbage collection is engine-managed` |
| ZS1605 | MISUSE | `newproxy` called | `newproxy is not available in Zenith scripts` |
| ZS1606 | MISUSE | `math.random` or `math.randomseed` called | `math.<Function> is not available; use ctx.Random` |
| ZS1607 | MISUSE | any `os` function called | `os.<Function> is not available; use ctx.Tick, ctx.Time or ctx.DeltaTime` |
| ZS1608 | MISUSE | a global assigned at runtime (defence in depth behind the load-time check) | `global '<Name>' cannot be assigned; module scope is immutable` |

### Bindings and contexts

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1701 | MISUSE | a callback re-entry beyond the protected-call depth limit | `script re-entry depth limit (<Limit>) reached in <Function>` |
| ZS1702 | MISUSE | a binding called from a context it does not allow | `<Namespace>.<Function> is not available in <Context>` |
| ZS1703 | MISUSE | a `READ_WORLD` binding whose resource is not in the module's `Uses` | `<Namespace>.<Function> reads <Resource>, which <Module> does not declare in Uses` |
| ZS1704 | MISUSE | an invocation-only API called at module scope | `<Function> can be called only during an invocation` |
| ZS1705 | MISUSE | a load-only API called during an invocation | `<Function> can be called only while the module loads` |

### Modules

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1801 | LOAD | `require` of a path not in the module manifest | `module '<Path>' not found` |
| ZS1802 | LOAD | a require cycle | `require cycle: <PathA> -> <PathB> -> <PathA>` |
| ZS1803 | MISUSE | `require` during an invocation | `require is allowed only at module scope` |
| ZS1804 | LOAD | a module rejected by the stateless check | `module '<Path>' was rejected by the stateless check: <Diagnostic> at line <Line>: <Detail>` |
| ZS1805 | LOAD | a cooked module whose header does not match the build | `module '<Path>' bytecode mismatch: <Field>; re-cook the game` |
| ZS1806 | LOAD | an invalid behaviour or chunk-system declaration | `<Module> declaration: <Key> <Problem>` |
| ZS1807 | LOAD | an input action or context name the loaded bindings do not declare | `unknown input <Kind> '<Name>' in <Module>` |

### Behaviour state

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1901 | MISUSE | a write to an undeclared state field | `state field '<Name>' is not declared in <Module>.State` |
| ZS1902 | MISUSE | a value of the wrong kind for a state field | `state field '<Name>' expects <Kind>, got <LuauType>` |
| ZS1903 | MISUSE | a string longer than its declared maximum | `state field '<Name>' exceeds its declared maximum of <Max> bytes` |
| ZS1904 | MISUSE | an array or map beyond its declared maximum count | `state field '<Name>' exceeds its declared maximum of <Max> elements` |
| ZS1905 | MISUSE | a map key that is not a string, enum or integer | `state map '<Name>' accepts string, enum or integer keys, got <LuauType>` |
| ZS1906 | MISUSE | a function, thread or foreign userdata assigned to state | `state field '<Name>' cannot hold a <LuauType>` |
| ZS1907 | LOAD | a `Migrate` step returning a table invalid for the next version | `Migrate[<Version>] of <Module> returned invalid state: <Problem>` |

### Luau-native errors

Errors raised by the Luau VM itself keep Luau's own text; the table records the pinned revision's text, which `Test_Script_Misuse_Catalogue` asserts and a Luau pin change re-baselines.

| Kind | Trigger | Pinned Luau text (pattern) |
|---|---|---|
| RUNTIME | arithmetic on a non-number | `attempt to perform arithmetic (<op>) on <type>` |
| RUNTIME | indexing `nil` | `attempt to index nil with '<key>'` |
| RUNTIME | calling a non-function | `attempt to call a <type> value` |
| RUNTIME | a write into a frozen table (a loaded module's tables, frozen constants, built-in libraries) | `attempt to modify a readonly table` |
| RUNTIME | `error(value)` from a script | the script's own value, converted by `tostring` |

### Diagnostics that are not Luau errors

| Diagnostic | Raised at | Content |
|---|---|---|
| `SCRIPT_MERGE_REJECTED_CAPACITY` | merge point | module, tick, stable simulation ID, capacity class, demand, remaining capacity |
| `SCRIPT_STATE_REFERENCE_UNRESOLVED` | restore | module, stable simulation ID of the instance, field, unresolved persistent or simulation ID |
| `SCRIPT_MODULE_DISABLED` | the tick boundary after a failure | module, world, tick, the first failure's code and entity |
| `SCRIPT_INSTANCE_QUARANTINED` | failure | module, stable simulation ID, error code |
| `SCRIPT_SNAPSHOT_TOTAL_EXCEEDED` | end of tick | bytes against the per-tick snapshot total row |
| `SCRIPT_EVENTS_DROPPED_QUARANTINE` | quarantine | module, stable simulation ID, dropped event count |
| `SCRIPT_SCHEDULE_WRITE_OUTSIDE_AUTHORITY` | schedule compile | system, field, owning phase |
| `SCRIPT_SCHEDULE_CROSS_ENTITY_READ_OF_WRITTEN_COLUMN` | schedule compile | system, field, the writer node |
| `SCRIPT_LINT_ADDRESS_KEYED_ITERATION` | cook | module, line, inferred key type |
| `SCRIPT_STATE_RESTORE_FAILED` | restore | module, stable simulation ID, the mismatched header field (magic, format version or source ID); the instance stays quarantined |
| `SCRIPT_DEBUGGER_UNAVAILABLE` | `Zenith_ScriptRuntime::Create` | the port and the platform listener's error; recorded in the runtime's diagnostic store and logged as a warning in `script.dap` when `m_bRequired` is clear (otherwise `Create` fails with `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE`) |
| `SCRIPT_BINDING_DEPRECATED_CALL` | world load | code ZS1808 (SCRIPT's block; a warning, not a Luau error: the module loads and runs); module, line, the deprecated function, its replacement and removal stage; text `<Module>:<Line>: ZS1808: Zenith.<Namespace>.<Function> is deprecated since Z2 Stage <D>; use <Replacement>; it is removed in Z2 Stage <Removed>`, once per (module, line) per world load |
| `SCRIPT_ASYNC_CAPACITY_HELD` | merge point | source kind, the oldest published-unconsumed holder's module and stable simulation ID, its age in ticks; raised when a request is rejected because its source kind is full and that holder was published at least `uSCRIPT_ASYNC_STALE_TICKS` (600) ticks earlier, at most once per (source kind, holder) per world session |

## Catalogue test obligations

- [Z2-N-SCRIPT-841] Every row of every table under "Catalogue" has a trigger script in `Zenith2/Engine/ZenithScript/Tests/Misuse/<Code>.luau` (or, for non-Luau diagnostics, a scenario) that `Test_Script_Misuse_Catalogue` runs, asserting the exact code, kind and substituted text.
- [Z2-N-SCRIPT-842] `Test_Script_Misuse_NeverAsserts` runs every trigger script in a Debug build with an assert handler that fails the test, so a misuse that reaches an assert instead of a controlled Luau error fails the gate.
- [Z2-N-SCRIPT-843] A new misuse check added to the runtime adds its catalogue row and its trigger script in the same diff.
