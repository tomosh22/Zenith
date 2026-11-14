# PROC design — ForbiddenCheck, TestSeparationCheck, Libgraph and Complexity

Companion to `Zenith2/Docs/Design/PROC.md`. This file specifies the four structural hard gates (Z2-D-042; [Z2-N-CONTRACT-571]): their exact inputs, their rule engines, their outputs, the protected data files each reads with the format and the initial entries of every file, and the fixtures each self-test runs. The rules themselves are the contract of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", "No mutable global state" and "Layering DAG", of `Zenith2/Docs/Architecture.md` → "Library dependency graph" and of `Zenith2/Docs/Scripting.md` → "Luau integration and the exception firewall"; this file adds the detection mechanics those documents leave to the gate.

## Scope of this file

| Step | Script | Data files it reads |
|---|---|---|
| ForbiddenCheck | `Zenith2/Tools/Gates/ForbiddenCheck.ps1` with `Forbidden/Zenith2Forbidden.psm1` | `std_headers.json`, `platform_headers.json` (with its `publicPlatformHeaders` exception, rule F18), `forbidden_allowlist.json` (with its `f21Scope`, `f25Boundary` and `profilerReachable` lists), `writable_data_allowlist.json`; from Z2T-PROC-045 `confined_apis.json` (rule F24, format `Zenith2/Docs/Design/SEC/PathTraversal.md` → "confined_apis.json") |
| TestSeparationCheck | `Zenith2/Tools/Gates/TestSeparationCheck.ps1` | none of its own; the codemodel, `declared_deps.json`, the PDBs |
| Libgraph | `Zenith2/Tools/Gates/Libgraph.ps1` (driving `Tools/libgraph/libgraph.py`) | `libgraph_policy.json`, `libgraph_baseline.json`, `shipping_forbidden_symbols.json`; from Z2T-PROC-054 `shipping_forbidden_strings.json` (format `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "shipping_forbidden_strings.json schema") |
| Complexity | `Zenith2/Tools/Gates/Complexity.ps1` (driving `Tools/analyze_code_complexity.py`) | `complexity_profile.json` |

## ForbiddenCheck

### ForbiddenCheck inputs

| Input | Source |
|---|---|
| source files | `Get-Zenith2ScanFiles -Extensions .cpp,.c,.h,.hpp,.inl` plus every `*_CompileFail.cpp.in`; tracked and untracked ([Z2-N-CONTRACT-416]) |
| compile groups | `Get-Zenith2FileApiReply` for every configure tree present on the host (`win`, `win-asan`, `android-arm64` when configured) |
| objects | the `win-debug` objects of the File API compile groups (rule F16) |
| third-party ownership | `${ZENITH_OUT}/third_party_targets.json` and the dependency-cache include directories it names (rule F23), and its `entry` and `owner` members (the `f25Boundary` check of rule F25) |
| data files | the four files of "ForbiddenCheck allowlist files", and `confined_apis.json` once it exists |

### File classes

Every scanned file gets exactly one class from its path; the rules' scopes are written in these classes.

| Class | Paths |
|---|---|
| `runtime-public` | `Zenith2/Engine/<Lib>/<file>` at a library root (`.h`, `.hpp`, `.inl`), and the per-platform threading headers `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h`, which the public synchronisation headers of ZenithCore alias |
| `runtime-internal` | `Zenith2/Engine/<Lib>/Internal/**`, and every source of `Zenith2/Player/**` outside `Tests/` |
| `test` | `Zenith2/Engine/<Lib>/Tests/**`, `Zenith2/Tests/**`, every `*_CompileFail.cpp.in` |
| `tool` | `Zenith2/Tools/<Dir>/**` except `Zenith2/Tools/Gates/**` |
| `benchmark` | `Zenith2/Benchmarks/**` |
| `other` | anything else (no compiled source is expected; SourceLayout reports it) |

`runtime-public` and `runtime-internal` together are "runtime code"; a host-only engine library (`ZenithScriptCompiler`) is runtime code for rule F4 ([Z2-N-CONTRACT-577]). A rule whose scope is "all Zenith2 code" applies to every class.

### Lexing

`Zenith2Forbidden.psm1` lexes each file once into three views, preserving line numbers:

1. **Code view:** comments (`//` to end of line, `/* … */`), string literals (ordinary, wide, UTF-8 and raw `R"delim( … )delim"`), and character literals replaced by spaces, newlines kept; a line continuation (`\` before newline) joins lines for the directive view only.
2. **Directive view:** each preprocessor directive (`#` as the first non-space character, after joining continuations) parsed into `{ line, directive, argument }`; `#include` arguments keep their `<…>` or `"…"` form.
3. **Brace view:** the code view's `{` and `}` with the token before each opening brace, so rule F12 can find the extent of every `class`, `struct` and `union` body (the body opens at the first `{` after the keyword and its name, skipping a base-clause, and closes at the matching `}`).

Tokens are matched as whole identifiers (`(?<![A-Za-z0-9_])tok(?![A-Za-z0-9_])`), and qualified names (`std::make_unique`) as the identifier sequence with optional spaces around `::`, so `Free(` never matches `free` and `GetTime(` never matches `time(` ([Z2-N-CONTRACT-416]).

### Matching and allowlist application

For each rule of "ForbiddenCheck rule table", each match in a file whose class is in the rule's rejected scope and whose path is outside the rule's allowed scope is checked against `forbidden_allowlist.json`: an entry matches when its `rule` equals the rule ID, its `path` glob matches the file, and (when present) its `token` equals the matched token or header name. A matching entry whose `decision` is a `Z2-D-NNN` that exists as a heading in `Zenith2/Docs/DecisionLog.md` exempts the match; anything else leaves the match a `fail` finding with the rule ID ([Z2-N-CONTRACT-417]). Rule F25 is the exception: no entry exempts it, and its only exemptions are the ones its row names, the `// ZENITH-ALLOW: <reason>` line signpost, `Zenith_Types.h` and the `f25Boundary` list. Rule F26 is a second: no entry and no signpost exempts it, its only exemption is the allowed scope its row names, and the `profilerReachable` list widens its rejected scope without exempting anything.

### ForbiddenCheck outputs

- `${ZENITH_OUT}/gates/ForbiddenCheck.json` (the step report) and `${ZENITH_OUT}/gates/ForbiddenCheck_WritableData.json` (the F16 records of [Z2-N-CONTRACT-151]).
- Each finding is printed as `ForbiddenCheck: <rule>: <file>:<line>: <message>`, with the Scripting diagnostic in brackets for the FS rules (`ForbiddenCheck: FS1: … [FORBIDDEN_LUA_CALL]`), and the F16 forms exactly as [Z2-N-CONTRACT-151] spells them.
- Exit 1 when any `fail` finding exists.

## ForbiddenCheck rule table

The scopes are those of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (table "ForbiddenCheck rules") for F1 to F21, of this design for F22 and F23 and of `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Confined OS APIs" for F24 (all three are added to the Contracts table by the CROSS_DOC row of reconciliation R9), of this design for F25 and F26 (the lock-profiling recursion defence), the naming rules of `Zenith2/CLAUDE.md` → "Naming in Zenith Hungarian" made mechanical (reconciliation R21), and of `Zenith2/Docs/Scripting.md` → "Luau integration and the exception firewall" (table "ForbiddenCheck rules") for FS1 to FS7. The column "Detection" is the mechanical matcher this step uses.

| Rule | Detection | Rejected in | Allowed in |
|---|---|---|---|
| F1 | code view: the expression keywords `new` and `delete` (excluding `= delete` after a declarator, `operator new`/`operator delete` declarations are matched as their own tokens) and the identifiers `malloc`, `calloc`, `realloc`, `free`, `_aligned_malloc`, `_aligned_free`, `aligned_alloc`, `posix_memalign`, `VirtualAlloc`, `mmap`, `alloca`, `_alloca` followed by `(`; `operator new`, `operator delete` | all classes | `Zenith2/Engine/ZenithCore/Internal/Memory/**` (never `alloca`, `_alloca`); allowlist entries F1-01 to F1-03 |
| F2 | `new` immediately followed by `(` (placement form) | all classes | ZenithCore's memory and container root headers (`Zenith_Owned.h`, `Zenith_Owned.inl`, `Zenith_Arena.h`, `Zenith_Pool.h`, and the container headers the CORE design lists) and `Zenith2/Engine/ZenithCore/Internal/**` |
| F3 | `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`, `std::make_unique`, `std::make_shared`, `std::auto_ptr` | all classes | nowhere (no allowlist entry is accepted for F3: an entry naming F3 is itself `ALLOWLIST-FORBIDDEN-RULE`) |
| F4 | directive view: `#include <X>` where `X` is in `std_headers.json` and not in its `allowedRuntime` list | runtime code | allowlist entries; `test`, `tool`, `benchmark` classes |
| F5 | code view of `runtime-public` files: `std::` followed by any identifier other than those of the allowed headers' facilities (the `allowedRuntimeNames` list of `std_headers.json`: `int8_t` … `uint64_t`, which F25 confines to `Zenith_Types.h`, `size_t`, `ptrdiff_t`, `nullptr_t`, `numeric_limits`, the type-traits and concepts names, `move`, `forward`, `swap`, `exchange`, `declval`, `bit_cast`, `atomic`, `memory_order*`, `initializer_list`, `strong_ordering`, `weak_ordering`, `partial_ordering`, `source_location`, `launder`, `nothrow`, `align_val_t`, `max_align_t`, the `<cmath>` functions F21 admits) | `runtime-public` | nowhere |
| F6 | `try`, `catch`, `throw` | all classes | `Zenith2/Engine/ZenithScript/Internal/**`, `Zenith2/Engine/ZenithScriptCompiler/Internal/**`, within which FS4 narrows further |
| F7 | `lua_call`, `lua_callk`, `lua_pcall`, identifiers starting `luaL_`; `#include` of `lua.h`, `lualib.h`, `luacode.h`, `luacodegen.h` or `Luau/…` | all classes | calls: `ZenithScript/Internal/**`; includes: `ZenithScript/Internal/**`, `ZenithScriptCompiler/Internal/**` |
| F8 | `dynamic_cast`, `typeid` | all classes | nowhere |
| F9 | `thread_local` | all classes | `ZenithCore/Internal/Profiling/**` and `ZenithProfiling/Internal/**`, and there only on the declarations of the two allowlisted `thread_local` variables (a third `thread_local` in those directories is a finding) |
| F10 | `using namespace` | `.h`, `.hpp`, `.inl` files of every class | nowhere |
| F11 | directive view: `#if`, `#ifdef`, `#ifndef`, `#elif`, `#elifdef`, `#elifndef` whose condition names `ZENITH_PLATFORM_*`, `ZENITH_RHI_*`, `_WIN32`, `__ANDROID__`, `__linux__`, `_MSC_VER`, `__clang__` (exactly the contract's list) | all classes | `ZenithCore/Internal/Zenith_CompilerSupport.h` through allowlist entry F11-01 (landed by the Stage 1 allowlist ticket Z2T-PROC-034) |
| F12 | any directive of the conditional family between the opening and closing brace of a `class`, `struct` or `union` body (brace view) | all classes | nowhere |
| F13 | `Zenith_Delete` followed by `(` (and `Zenith_Delete<…>(`) | all classes | `Zenith2/Engine/ZenithCore/Zenith_Owned.h`, `Zenith_Owned.inl` |
| F14 | `rand`, `srand`, `std::rand`, `time`, `clock` followed by `(` | runtime code | nowhere |
| F15 | directive view: `#pragma warning(disable`, `#pragma warning( disable`, `#pragma clang diagnostic ignored`, `#pragma GCC diagnostic ignored` | all classes | `Internal/` third-party wrapper headers through an allowlist entry per file |
| F16 | the writable-data symbol scan ("Writable-data scan") | runtime objects | `writable_data_allowlist.json` entries with their declaration comment blocks |
| F17 | compile groups: `/EHs`, `/EHsc`, `/EHa`, `/EHs-c` absent, `-fexceptions`, `/GR` (without `-`), `-frtti` in a group's `compileCommandFragments`, and the absence of `/EHs-c-` and `/D_HAS_EXCEPTIONS=0` (MSVC) or `-fno-exceptions` (clang) on a non-firewall group | every compile group except `ZenithScript_EH`, `ZenithScriptCompiler_EH` and the Luau targets whose manifest `targets` element carries `"exceptions": true` (exceptions only; RTTI is refused everywhere) | nowhere |
| F18 | directive view: `#include <X>` where `X` is a `header` of `platform_headers.json` | all classes | files under `Internal/<Platform>/` whose `<Platform>` equals the entry's `platform`; the file of a `publicPlatformHeaders` element of `platform_headers.json`, for the headers that element lists (the per-platform threading header exception) |
| F19 | `#include` of a `.inl` | all classes | the `.h` of the same stem in the same directory |
| F20 | `#include` of `ZenithRHI/Zenith_RHICommandList.h` or `ZenithRHI/Zenith_RHIBarrier.h` | all classes | command list: `ZenithFlux/Internal/**`, `ZenithRHI_*/**`, `ZenithRHI/Internal/**`, `ZenithRHI/Tests/**`; barrier: `ZenithFlux/Internal/Graph/**`, `ZenithRHI_*/**`, `ZenithRHI/Internal/**`, `ZenithRHI/Tests/**` |
| F21 | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `cbrt` and their `f`-suffixed and `l`-suffixed forms, optionally `std::`-qualified, followed by `(`; `sqrt` and `std::sqrt` are not matched ([Z2-N-ARCH-888]; row X362) | `Zenith2/Engine/**` and `Zenith2/Player/**` outside `Tests/`, `Zenith2/Tools/Authoring/**`, `Zenith2/Tools/ZenithCook/**`, and the further directories of the `f21Scope` list of `forbidden_allowlist.json` (at Stage 0 CHAR's `Zenith2/Tools/ZenithEditor/Internal/Character/**` and `Zenith2/Benchmarks/CharacterAuthoring/**`, CHAR-P1) | test files through an allowlist entry per file (CORE's `Test_Transcendental_WithinBoundOfHostLibrary_*`) |
| F22 | `LoadLibrary`, `LoadLibraryA`, `LoadLibraryW`, `LoadLibraryEx`, `LoadLibraryExA`, `LoadLibraryExW`, `dlopen` followed by `(` ([Z2-N-ARCH-274]) | all classes | `ZenithPlatform/Internal/**`, `ZenithRHI_*/Internal/**` |
| F24 | SEC's confined OS APIs ([Z2-N-CONTRACT-416] once R9's row lands; SEC-P4, SEC19; `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Confined API classes"). A hit is a token of a class of `Zenith2/Tools/Gates/confined_apis.json` that is called (the identifier, optional blanks, `(`) or address-taken (`&` then the identifier). Three matching rules apply. **Member rule:** an identifier preceded by `.`, `->` or a `::` whose left side is not `std` (blanks allowed between) is not a hit, so `xFile.open(…)` and `Zenith_Path::remove(…)` never match, while `std::remove(…)`, `::open(…)` and a bare `open(…)` do. **Prefix rule:** a token ending `*` matches every identifier with that prefix (`_spawn*`, `_wspawn*`). **Token classes:** the classes are SEC's table, including SEC19's additions `_wopen`, `_open`, `fopen_s`, `_wfopen_s`, `CopyFileW`, `CopyFile2`, `MoveFileW`, `NtCreateFile`, `CreateProcessAsUserW`, `_wsystem`, `_spawn*` and `_wspawn*`. Lands with Z2T-PROC-045 at Stage 2 | every Zenith2 source (third-party roots excluded) | the directories of the file's `allow` records for the token's class |
| F23 | `#include <X>` or `#include "X"` whose `X` resolves to a header inside a third-party target's include directory (the `includeDirs` of `third_party_targets.json`), from a file that is not under the `Internal/` of a library named in that entry's `owner`, and not in a test, benchmark or tool target that names the entry in `thirdParty` or `testThirdParty` of `declared_deps.json` ([Z2-N-ARCH-165]; row SH6) | all classes | as stated |
| F25 | code view, four token groups, each finding naming its group: **(a)** the identifiers `noexcept` and `nodiscard`, so `noexcept`, `noexcept(…)`, `[[nodiscard]]`, `[[nodiscard("…")]]` and `[[nodiscard, maybe_unused]]` all match; **(b)** every identifier beginning `u_int` (`u_int8` … `u_int64`); **(c)** the identifiers `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t` and `uint64_t`, `std::`-qualified or not; **(d)** `final` as the class-virt-specifier of a `class`, `struct` or `union` definition with no base clause: in the brace view, the class head (keyword, optional attributes, name, optional template arguments) followed by `final` and then, after blanks, the body's `{` rather than `:` | `runtime-public`, `runtime-internal`, `tool` | (a) a line whose own `//` comment (as the lexer finds it, so a `//` inside a string literal is not one) reads `// ZENITH-ALLOW: ` followed by a non-blank reason, the signpost of the rare use a language or standard-library mechanism requires; a comment on another line, or one with no reason, exempts nothing. (b) nowhere. (c) `Zenith2/Engine/ZenithCore/Zenith_Types.h`, which defines `uint8` … `uint64`, `sint8` … `sint64`, `uint` and `sint` from them, and the third-party boundary files of the `f25Boundary` list of `forbidden_allowlist.json`, each under its third-party owner's `Internal/`. (d) nowhere; a `final` class with a base clause passes. No `entries` element exempts F25 |
| F26 | code view, two clauses, each finding naming its clause: **(i)** a profiled synchronisation primitive: the whole-token identifiers `Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore`, and a platform class template of those primitives instantiated with `true`, that is `Zenith_<Platform>Mutex_T`, `Zenith_<Platform>ConditionVariable_T` or `Zenith_<Platform>Semaphore_T` (`<Platform>` one of `Windows`, `Android`, `Linux`) followed by `<`, `true` and `>` (blanks allowed between); the aliases `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` and `Zenith_SemaphoreNoProfiling` and the `<false>` instantiations are other whole identifiers and never match; **(ii)** a direct use of the hook table: either the identifier `Zenith_GetProfilerHooks`, `(`, `)` and then `.` or `->` and an identifier beginning `m_pfn` other than the log-record members `m_pfnOnLogRecord` and `m_pfnIsLogLevelEnabled` (blanks and line breaks allowed between), or one of the identifiers `m_pfnOnZoneBegin`, `m_pfnOnZoneEnd`, `m_pfnOnCounter`, `m_pfnOnLockAcquire`, `m_pfnOnLockRelease` and `m_pfnOnBlockingWait` followed by optional blanks, an optional `)` and `(`, so a call through a `Zenith_ProfilerHooks` value matches while an assignment of the member, its declaration and a call of a `Zenith_Dispatch…` function do not; the log-record members `m_pfnOnLogRecord` and `m_pfnIsLogLevelEnabled` are outside both forms (named in neither), because the log-record hooks stay outside the dispatch functions of `Zenith_ProfilerHooks.h`: the failure procedure emits its fatal record from inside whatever hook asserted, and the log router's locks and drain semaphore are `_NoProfiling` variants instead | (i) `runtime-public` and `runtime-internal` files under `Zenith2/Engine/ZenithProfiling/**` or `Zenith2/Engine/ZenithCore/Internal/Profiling/**`, and every file named by the `profilerReachable` list of `forbidden_allowlist.json`; (ii) every class | (i) `test` files (a test's subject may be a profiled primitive); (ii) `Zenith2/Engine/ZenithCore/Zenith_ProfilerHooks.h` and `test` files. No `entries` element and no signpost exempts F26; a file in both the directories and the list is reported once. Clause (ii) is lexical: a hook member copied into a variable and then called, or a ZenithCore type that embeds a profiled mutex and is used from profiler code, is not seen by the scan; layer (c), the run-time hook-depth guard of [Z2-N-CONTRACT-590], and review cover those cases. |
| FS1 | `lua_call`, `lua_callk` | all classes | `Zenith2/Engine/ZenithScript/Internal/**` — diagnostic `FORBIDDEN_LUA_CALL` |
| FS2 | `lua_pcall`, `lua_resume`, `luau_load` | all classes | `Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp` — `FORBIDDEN_LUA_PROTECTION_SITE` |
| FS3 | Luau header includes (as F7) | all classes | the two firewall `Internal/` trees — `FORBIDDEN_LUAU_INCLUDE` |
| FS4 | `catch` outside `ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp` and `ZenithScriptCompiler/Internal/Zenith_ScriptCompilerGuard.cpp`; `throw` anywhere in Zenith2 code | all classes | as stated — `FORBIDDEN_EXCEPTION_TOKEN` |
| FS5 | the exception flags of F17 | as F17 | `FORBIDDEN_EXCEPTION_FLAG` |
| FS6 | `setjmp`, `longjmp`, `LUA_USE_LONGJMP` (also in compile-group defines) | all classes | nowhere — `FORBIDDEN_LONGJMP` |
| FS7 | `luau_compile`, `Luau::compile`, `Luau::Parser` | all classes | `Zenith2/Engine/ZenithScriptCompiler/Internal/**` — `FORBIDDEN_LUAU_COMPILE_SITE` |

F26 is the second layer of the lock-profiling recursion defence (`Zenith2/Docs/Design/CORE/Threading.md` → "Platform threading classes"): the types are the first (profiler code names only the `_NoProfiling` variants), this rule is the second (no profiled primitive in profiler-reachable code, and no hook call that skips the dispatch functions of `Zenith_ProfilerHooks.h`), and the dispatch functions' run-time re-entry guard is the third. A finding is printed `ForbiddenCheck: F26: <file>:<line>: (i) profiled <token> in profiler-reachable code; use the _NoProfiling variant` or `ForbiddenCheck: F26: <file>:<line>: (ii) direct hook-table use <token>; call the Zenith_Dispatch… function of Zenith_ProfilerHooks.h`.

A match that two rules report (F6 and FS4, F7 and FS1/FS3, F17 and FS5, and F22 and F24 on `LoadLibrary*` and `dlopen`) is one finding carrying both IDs (`F6+FS4`), so a breach is never reported twice and never silently once. SEC's dynamic-loading allow records are exactly F22's directories and [Z2-N-ARCH-274]'s, `ZenithPlatform/Internal/` and every `ZenithRHI_*/Internal/` (ruling SEC-R1; row SEC19 (a)). A `LoadLibrary*` or `dlopen` call is therefore allowed or refused by both rules alike, and reported once as `F22+F24`. F24 alone covers `GetProcAddress` and `dlsym`, in the same two directories.

## ForbiddenCheck allowlist files

### std_headers.json

```json
{
    "schema": "zenith2.std-headers",
    "schemaVersion": 1,
    "comment": "The C++20 standard headers (rule F4) and the thirteen allowed in runtime code (Zenith2/Docs/Contracts.md, Containers and standard library policy, [Z2-N-CONTRACT-271]).",
    "standard": [ "algorithm", "any", "array", "atomic", "barrier", "bit", "bitset", "cassert", "cctype", "cerrno", "cfenv", "cfloat", "charconv", "chrono", "cinttypes", "climits", "clocale", "cmath", "codecvt", "compare", "complex", "concepts", "condition_variable", "coroutine", "csetjmp", "csignal", "cstdarg", "cstddef", "cstdint", "cstdio", "cstdlib", "cstring", "ctime", "cuchar", "cwchar", "cwctype", "deque", "exception", "execution", "filesystem", "format", "forward_list", "fstream", "functional", "future", "initializer_list", "iomanip", "ios", "iosfwd", "iostream", "istream", "iterator", "latch", "limits", "list", "locale", "map", "memory", "memory_resource", "mutex", "new", "numbers", "numeric", "optional", "ostream", "queue", "random", "ranges", "ratio", "regex", "scoped_allocator", "semaphore", "set", "shared_mutex", "source_location", "span", "sstream", "stack", "stdexcept", "stop_token", "streambuf", "string", "string_view", "syncstream", "system_error", "thread", "tuple", "type_traits", "typeindex", "typeinfo", "unordered_map", "unordered_set", "utility", "valarray", "variant", "vector", "version", "assert.h", "ctype.h", "errno.h", "fenv.h", "float.h", "inttypes.h", "limits.h", "locale.h", "math.h", "setjmp.h", "signal.h", "stdarg.h", "stddef.h", "stdint.h", "stdio.h", "stdlib.h", "string.h", "time.h", "uchar.h", "wchar.h", "wctype.h" ],
    "allowedRuntime": [ "cstdint", "cstddef", "cstring", "cmath", "limits", "type_traits", "concepts", "utility", "new", "bit", "atomic", "initializer_list", "compare", "source_location" ]
}
```

The `allowedRuntime` list is the table of `Zenith2/Docs/Contracts.md` → "Containers and standard library policy" (subsection "Allowed standard headers"); the step fails `STD-HEADERS-DRIFT` when the list differs from that table, which it parses, so the two never drift.

### platform_headers.json

Entries are the `{ "header", "platform" }` pairs of [Z2-N-CONTRACT-432], `platform` one of `Windows`, `Android`, `Linux`. The Stage 0 file (Z2T-PROC-009) carries the union of CORE-P3, RHI-P4 and SCENE-P1, byte-ordered by header then platform, and the `publicPlatformHeaders` exception list:

```json
{
    "schema": "zenith2.platform-headers",
    "schemaVersion": 1,
    "comment": "Rule F18 deny-list: each header is included only from Internal/<platform>/ files of its platform (Zenith2/Docs/Contracts.md, Tests and code hygiene gates, [Z2-N-CONTRACT-432]).",
    "entries": [
        { "header": "android/log.h", "platform": "Android" },
        { "header": "bcrypt.h", "platform": "Windows" },
        { "header": "d3d12.h", "platform": "Windows" },
        { "header": "d3d12sdklayers.h", "platform": "Windows" },
        { "header": "dbghelp.h", "platform": "Windows" },
        { "header": "dlfcn.h", "platform": "Android" },
        { "header": "dlfcn.h", "platform": "Linux" },
        { "header": "dxgi1_6.h", "platform": "Windows" },
        { "header": "dxgidebug.h", "platform": "Windows" },
        { "header": "fcntl.h", "platform": "Linux" },
        { "header": "pix3.h", "platform": "Windows" },
        { "header": "psapi.h", "platform": "Windows" },
        { "header": "pthread.h", "platform": "Android" },
        { "header": "pthread.h", "platform": "Linux" },
        { "header": "semaphore.h", "platform": "Android" },
        { "header": "semaphore.h", "platform": "Linux" },
        { "header": "sys/file.h", "platform": "Linux" },
        { "header": "sys/random.h", "platform": "Android" },
        { "header": "sys/random.h", "platform": "Linux" },
        { "header": "sys/stat.h", "platform": "Linux" },
        { "header": "sys/statvfs.h", "platform": "Linux" },
        { "header": "unistd.h", "platform": "Android" },
        { "header": "unistd.h", "platform": "Linux" },
        { "header": "unwind.h", "platform": "Android" },
        { "header": "unwind.h", "platform": "Linux" },
        { "header": "windows.h", "platform": "Windows" }
    ],
    "publicPlatformHeaders": [
        { "path": "Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h", "platform": "Android", "headers": [ "pthread.h", "semaphore.h" ], "comment": "ZENITH-EXEMPTION: the per-platform threading header holds pthread_mutex_t, sem_t and pthread_cond_t as typed members of the Android synchronisation classes that ZenithCore's public Zenith_Mutex, Zenith_Semaphore and condition-variable headers alias, so it includes the POSIX headers although a public header reaches it; platform code in a platform file, not a layout-changing conditional." },
        { "path": "Zenith2/Engine/ZenithCore/Platform/Linux/Zenith_PlatformThreading.h", "platform": "Linux", "headers": [ "pthread.h", "semaphore.h" ], "comment": "ZENITH-EXEMPTION: as the Android element, for the Linux synchronisation classes." }
    ]
}
```

A header listed for two platforms is allowed in either platform's directory. An `android/…` header not listed individually is treated as `Android` by the prefix rule `android/` (a built-in, so the list need not enumerate the NDK).

`publicPlatformHeaders` is the one exception to the rule that a platform SDK header is included only under `Internal/<Platform>/`: ZenithCore's synchronisation types follow one class per platform, each holding its native type as a typed member, in a per-platform header the build selects, and the public header aliases that class. Each element is `{ "path", "platform", "headers", "comment" }`, all four required. The step refuses, as `PLATFORM-HEADERS-EXCEPTION` (fail), an element whose `path` is not `Zenith2/Engine/ZenithCore/Platform/<platform>/Zenith_PlatformThreading.h` with `<platform>` equal to its `platform`, whose `headers` name anything but `pthread.h` and `semaphore.h`, or whose `comment` does not begin `ZENITH-EXEMPTION: `. The Windows header holds the pointer-sized `SRWLOCK`, `CONDITION_VARIABLE` and `HANDLE` values as `void*` members, includes no platform header and has no element, so `<windows.h>` there is an F18 finding. In the file itself, an admitted `#include` that does not sit in the run of `#include` lines directly below the file's four-line `// ZENITH-EXEMPTION` comment block ([Z2-N-CONTRACT-578]) is `F18-EXEMPTION-SIGNPOST` (report; the comment's wording is review's).

### forbidden_allowlist.json

```json
{
    "schema": "zenith2.forbidden-allowlist",
    "schemaVersion": 1,
    "comment": "Every exemption cites a Z2-D- decision ([Z2-N-CONTRACT-417]); token and comment are optional (Zenith2/Docs/Design/PROC/StructuralGates.md, ForbiddenCheck allowlist files). profilerReachable is a scope list of rule F26, not an exemption: it names the sources a profiler hook dispatch can reach (Zenith2/Docs/Design/PROC/StructuralGates.md, ForbiddenCheck rule table, row F26; [Z2-N-CONTRACT-143], [Z2-N-CONTRACT-416]).",
    "f21Scope": [ "Zenith2/Benchmarks/CharacterAuthoring/**", "Zenith2/Tools/ZenithEditor/Internal/Character/**" ],
    "f25Boundary": [],
    "profilerReachable": [
        { "path": "Zenith2/Engine/ZenithCore/Internal/Android/Zenith_Clock_Android.cpp", "comment": "the Android clock behind Zenith_Clock.h" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Android/Zenith_Diagnostics_Android.cpp", "comment": "the Android diagnostics a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Android/Zenith_Terminate_Android.cpp", "comment": "the Android termination a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Android/Zenith_Thread_Android.cpp", "comment": "the Android thread identity a hook reads" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Diagnostics/Zenith_Fail.cpp", "comment": "the failure path a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Diagnostics/Zenith_Log.cpp", "comment": "logging called from inside a hook" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Diagnostics/Zenith_LogRouter.cpp", "comment": "the log router called from inside a hook" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Diagnostics/Zenith_LogSinks.cpp", "comment": "the log sinks called from inside a hook" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Linux/Zenith_Clock_Linux.cpp", "comment": "the Linux clock behind Zenith_Clock.h" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Linux/Zenith_Diagnostics_Linux.cpp", "comment": "the Linux diagnostics a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Linux/Zenith_Terminate_Linux.cpp", "comment": "the Linux termination a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Linux/Zenith_Thread_Linux.cpp", "comment": "the Linux thread identity a hook reads" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Threading/Zenith_Thread.cpp", "comment": "the thread identity a hook reads" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Windows/Zenith_Clock_Windows.cpp", "comment": "the Windows clock behind Zenith_Clock.h" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Windows/Zenith_Diagnostics_Windows.cpp", "comment": "the Windows diagnostics a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Windows/Zenith_Terminate_Windows.cpp", "comment": "the Windows termination a failing hook reaches" },
        { "path": "Zenith2/Engine/ZenithCore/Internal/Windows/Zenith_Thread_Windows.cpp", "comment": "the Windows thread identity a hook reads" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_Clock.h", "comment": "the clock a profiled lock and every hook read" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_Log.h", "comment": "logging called from inside a hook" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_LogRouter.h", "comment": "the log router called from inside a hook" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_LogSinks.h", "comment": "the log sinks called from inside a hook" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_ProfilerHooks.h", "comment": "the hook table and the Zenith_Dispatch… functions every hook call goes through" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_SpscRing.h", "comment": "the per-thread event ring every hook writes" },
        { "path": "Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashLogMirror.cpp", "comment": "a log sink whose Write runs on the logging thread, which can be inside a hook" },
        { "path": "Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashLogMirror.h", "comment": "the log sink walk shared with the crash handler, called from the same Write" }
    ],
    "entries": [
        { "id": "F1-01", "rule": "F1", "path": "Zenith2/Engine/ZenithCore/Internal/Windows/Zenith_HeapBacking_Windows.cpp", "decision": "Z2-D-040", "comment": "the only Windows heap-backing calls (CORE-P1)" },
        { "id": "F1-02", "rule": "F1", "path": "Zenith2/Engine/ZenithCore/Internal/Android/Zenith_HeapBacking_Android.cpp", "decision": "Z2-D-040", "comment": "the only Android heap-backing calls (CORE-P1)" },
        { "id": "F1-03", "rule": "F1", "path": "Zenith2/Engine/ZenithCore/Internal/Linux/Zenith_HeapBacking_Linux.cpp", "decision": "Z2-D-040", "comment": "the only Linux heap-backing calls (CORE-P1)" },
        { "id": "FSAN-01", "rule": "SANITIZER-HEADER", "path": "Zenith2/Engine/ZenithCore/Internal/Android/Zenith_SanitizerHwasan_Android.cpp", "token": "sanitizer/hwasan_interface.h", "decision": "Z2-D-011", "comment": "HWASan tagging declarations MSVC cannot parse (CORE-P6)" }
    ]
}
```

| Member | Required | Rule |
|---|---|---|
| `id` | yes | `^[A-Z0-9]+-[0-9]{2}$`, unique; findings name the entry that exempted them in the report's `exemptedBy` list |
| `rule` | yes | an ID of the rule table, or `SANITIZER-HEADER` (the scope extension of [Z2-N-CONTRACT-432] for `<sanitizer/*.h>`, whose default allowed scope is `ZenithCore/Internal/Memory/`) |
| `path` | yes | a glob in the dialect of `Zenith2/Docs/Design/PROC.md` → "Format conventions" |
| `decision` | yes | `Z2-D-NNN` present in `Zenith2/Docs/DecisionLog.md`; nothing else ([Z2-N-CONTRACT-417]) |
| `token` | no | narrows the entry to one token or header name |
| `comment` | no | why |

Validation findings: `ALLOWLIST-NO-DECISION` (fail), `ALLOWLIST-UNKNOWN-DECISION` (a `Z2-D-` absent from the log; fail), `ALLOWLIST-FORBIDDEN-RULE` (an F3, F8, F25 or F26 entry; fail), `ALLOWLIST-DUPLICATE-ID` (fail), `ALLOWLIST-STALE` (no file matches the glob; report), and for `f25Boundary` `ALLOWLIST-F25-BOUNDARY` (fail: an element missing a member, a `thirdParty` that is no `entry` of `third_party_targets.json`, or a `path` not under the `Internal/` directory of that record's `owner`, its `dir` in `declared_deps.json`) and `ALLOWLIST-STALE` as for entries. For `profilerReachable`: `F26-LIST-MALFORMED` (fail: the key is absent or not an array, an element lacks a non-blank string `path` or `comment`, or a `path` holds `*`, `?` or `[` or lies outside `Zenith2/Engine/`), `F26-LIST-DUPLICATE` (fail: a path listed twice), `F26-LIST-NO-FILE` (report: a path names no file of the checkout) and `F26-LIST-REDUNDANT` (report: a path already lies under `Zenith2/Engine/ZenithProfiling/` or `Zenith2/Engine/ZenithCore/Internal/Profiling/`).

- **Shape against the contract.** [Z2-N-CONTRACT-417] names entries `{ "rule", "path", "decision" }`. This file adds a required `id`, the optional `token` and `comment`, a top-level wrapper (`schema`, `schemaVersion`, `comment`), the `f21Scope`, `f25Boundary` and `profilerReachable` lists, and the `rule` value `SANITIZER-HEADER`; reconciliation R7 lists each with its reason and its CROSS_DOC row.
- **`f25Boundary`** lists the files where a third-party API signature names a fixed-width `<cstdint>` type that Zenith code must pass through unchanged (a `lua_Integer` conversion, a codec's `int64_t` size), the one place rule F25 (c) admits `int8_t` … `uint64_t` besides `Zenith_Types.h`. Each element is `{ "path", "thirdParty", "comment" }`: `path` a file glob, `thirdParty` the `entry` of a `third_party_targets.json` record, `comment` the API it bridges. It needs no decision ID, because the naming rule itself admits third-party boundaries inside `Internal/`; it is a scope list like `f21Scope`, not an exemption entry, and the Stage 0 file holds none. An area adds an element through a PROC hand-off, as for `f21Scope`.
- **`profilerReachable`** is the scope list of rule F26 (i), and it is the one list of this file that widens a rejection instead of narrowing it. F26 (i) always covers `Zenith2/Engine/ZenithProfiling/**` and `Zenith2/Engine/ZenithCore/Internal/Profiling/**` (so `Zenith_DebugThreadState.h/.cpp`, `Zenith_ProfilerHooks.cpp` and `Zenith_LockOrderChecker.h/.cpp` are covered by the directory, not by the list); the list adds the sources outside both trees that a hook dispatch can reach and that therefore may not use a profiled primitive, because a hook that locks a profiled primitive calls the hook again. Each element is `{ "path", "comment" }`, both required: `path` one exact repo-relative file under `Zenith2/Engine/` (no glob), `comment` why a hook dispatch reaches it. It needs no decision ID, for the reason `f25Boundary` needs none: it is a scope list, not an exemption entry. The Stage 0 file holds the twenty-five elements of the sample above, byte-ordered by `path`: the profiler-hook dispatch header, the clock (header and the three platform sources), `Zenith_SpscRing.h`, the thread identity (`Zenith_Thread.cpp` and the three platform sources), the failure path (`Zenith_Fail.cpp` and the three platform `Zenith_Diagnostics_<Platform>.cpp` and `Zenith_Terminate_<Platform>.cpp` sources) and logging (the three log headers and sources, which the log router's `_NoProfiling` locks and drain semaphore make safe to call from a hook), plus DEVT's two log-sink sources `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashLogMirror.cpp` and `.h`, whose `Write` runs on the logging thread (DEVT-owned, added by PROC hand-off). CORE confirmed these against `Zenith2/Docs/Design/CORE/Threading.md`; any further file a hook dispatch reaches arrives as a PROC hand-off that adds an element, never as an edit by an ordinary ticket. The list is data: a new file is added to it, and the rule's code never names a path. The list is signposted in the file's own top-level `comment`, in the F26 row, in reconciliation R22 of `Zenith2/Docs/Design/PROC.md` and in requirement Z2-R-PROC-173.
- **`f21Scope`** exists because the contract's F21 scope ends in prose ("every further tool directory whose output is tracked or baked that an area's design names under [Z2-N-ARCH-888]") that a script cannot read. The list names those directories, each added by a PROC ticket that copies an area's request (CHAR-P1 at Stage 0).
- **The CORE justification-record entries** F4-01 (`<charconv>` in `ZenithCore/Internal/Text/**`, CORE-P4, record JR-CORE-CHARCONV) and F11-01 (`ZenithCore/Internal/Zenith_CompilerSupport.h`, CORE-P5, record JR-CORE-COMPILERSUPPORT) are not in the Stage 0 file. They need the decision ID that the `needs-human` ruling ticket Z2T-CORE-035 creates. The Stage 1 PROC ticket Z2T-PROC-034 (blocked by Z2T-CORE-035; it blocks Z2T-CORE-036 and Z2T-CORE-042) adds both with that ID. No entry without an existing decision is ever committed.
- **`<immintrin.h>`** (`_mm_pause` for `Zenith_CpuRelax`, CORE-P5) is neither in `std_headers.json` nor in `platform_headers.json`, so no rule matches it and it needs no entry; the include is checked by review (its file is the one F11-01 admits).

## Writable-data scan

Rule F16 runs the six steps of `Zenith2/Docs/Contracts.md` → "No mutable global state" (subsection "Writable-data symbol scan") over the `win-debug` objects, with these realisations:

- Step 2 dumps each COFF object with `llvm-readobj --section-headers --symbols <object>` in its default text form, parsed by `Zenith2Forbidden.psm1`'s `Read-Zenith2CoffSymbols`, which keys on the `Symbol {` blocks and their `Name:`, `Value:`, `Section:` and `StorageClass:` lines; the pinned LLVM's tools are the only ones used.
- Step 4 demangles with the pinned `llvm-undname`, one invocation per object with every symbol name on standard input.
- Step 5's declaration-block check (DEVT-P10): for each allowlisted symbol, the entry's `source` file must contain, on the line directly above the definition of the symbol (the first line whose code view declares the symbol's unqualified name at namespace scope with no `extern`), the four-line block of [Z2-N-CONTRACT-578]; line (1) is checked exactly — `// ZENITH-EXEMPTION <symbol>: <decision>` followed by `, extended <date> by ruling <ruling>` once per element of the entry's `extensions`, in order — and lines (2) to (4) for their fixed prefixes `// Allowlist: Zenith2/Tools/Gates/writable_data_allowlist.json, entry "<symbol>"`, `// Contract: Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]` and `// Why: `. A missing or different line (1) is a `fail`, as [Z2-N-CONTRACT-151] step 5 requires. A missing or different line (2), (3) or (4) is a `report` finding `F16-SIGNPOST-LINE`, because [Z2-N-CONTRACT-578] assigns those lines to review; DEVT's interim `CMake_Devt_AllowlistSignposts` check keeps its own scope.
  - An `extensions` element is `<date> <ruling>` (for example `2026-10-16 Q11 (a)`): the scan splits it at the first space and renders `, extended <date> by ruling <ruling>`, so that element becomes `, extended 2026-10-16 by ruling Q11 (a)`.
  - The arrow of line (3) is U+2192. The script body is ASCII-only, so the expected string is built with `[char]0x2192` and the source file is read as UTF-8.
- The entry completeness check of [Z2-N-CONTRACT-153]: `symbol`, `source`, `decision`, an `extensions` array, and a `comment` that contains the decision ID, every extension string and the signpost text `Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]`; any omission is the F16 incomplete-entry finding of [Z2-N-CONTRACT-151].

### writable_data_allowlist.json

The Stage 0 file (Z2T-PROC-009) carries exactly the five entries of `Zenith2/Docs/Contracts.md` → "No mutable global state" (subsection "Writable-data allowlist entries"), from CORE-P2 and DEVT-P5:

```json
{
    "schema": "zenith2.writable-data-allowlist",
    "schemaVersion": 1,
    "comment": "Every entry is an exemption of Z2-D-046; see Zenith2/Docs/Contracts.md → \"Writable-data allowlist entries\" [Z2-N-CONTRACT-152].",
    "entries": [
        { "symbol": "g_pxRootHeap", "source": "Zenith2/Engine/ZenithCore/Internal/Memory/Zenith_RootHeap.cpp", "decision": "Z2-D-046", "extensions": [], "comment": "The root heap pointer main installs; Z2-D-046's own text admits it; Zenith2/Docs/Contracts.md → \"Writable-data allowlist entries\" [Z2-N-CONTRACT-152]." },
        { "symbol": "g_xProfilerHooks", "source": "Zenith2/Engine/ZenithCore/Internal/Profiling/Zenith_ProfilerHooks.cpp", "decision": "Z2-D-046", "extensions": [], "comment": "The installed profiler hook table; Z2-D-046's own text admits it, and its log-record members (2026-10-16 Q11 (a)) add no symbol; Zenith2/Docs/Contracts.md → \"Writable-data allowlist entries\" [Z2-N-CONTRACT-152]." },
        { "symbol": "g_xDebugThreadState", "source": "Zenith2/Engine/ZenithCore/Internal/Profiling/Zenith_DebugThreadState.cpp", "decision": "Z2-D-046", "extensions": [ "2026-10-16 Q11 (a)" ], "comment": "The per-thread state the lock-order checker needs, and the hook depth and re-entry count that stop a profiler hook from recursing (both present in every build); Z2-D-046 as extended 2026-10-16 Q11 (a); Zenith2/Docs/Contracts.md → \"Writable-data allowlist entries\" [Z2-N-CONTRACT-152]." },
        { "symbol": "g_xProfilerThreadBuffers", "source": "Zenith2/Engine/ZenithProfiling/Internal/Hooks/Zenith_ProfilerThreadSlot.cpp", "decision": "Z2-D-046", "extensions": [ "2026-10-16 Q79 (a)" ], "comment": "The per-thread buffers of the hook implementations and the debug-draw recorder; Z2-D-046 as extended 2026-10-16 Q79 (a); Zenith2/Docs/Contracts.md → \"Writable-data allowlist entries\" [Z2-N-CONTRACT-152]." },
        { "symbol": "g_pxCrashHandlerAnchor", "source": "Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashHandlerAnchor.cpp", "decision": "Z2-D-046", "extensions": [ "2026-10-09 Q88 (a)" ], "comment": "The only state OS crash callbacks without user data can reach; Z2-D-046 as extended 2026-10-09 Q88 (a); Zenith2/Docs/Contracts.md → \"Writable-data allowlist entries\" [Z2-N-CONTRACT-152]." }
    ]
}
```

<!-- assumption: pending CORE -->
The `source` file names of the three ZenithCore entries are the files `Zenith2/Docs/Design/CORE/Memory.md` and `Zenith2/Docs/Design/CORE/Threading.md` place the definitions in; the entry's `source` must equal the file that holds the definition, so Z2T-PROC-009 copies the exact file names from the CORE design at the time it lands and the self-test `Test_Forbidden_AllowlistSourcesMatchDesign` compares each `source` with the design's file list.
<!-- /assumption -->

The entries for `g_xProfilerThreadBuffers` (Q79 (a)) and `g_pxCrashHandlerAnchor` (Q88 (a)) rest on rulings the human may still override. Z2T-PROC-009 therefore re-reads `Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-152] when it lands and copies the entries as that text then stands; `Test_Forbidden_WritableEntriesMatchContracts` keeps them equal afterwards.

A sixth writable symbol is never added by an ordinary ticket: [Z2-N-CONTRACT-141] closes the list, and a new exemption is a flagged proposal and then a PROC ticket.

## ForbiddenCheck fixtures

`Zenith2/Tools/Gates/Tests/ForbiddenCheck/Fixtures/` holds one failing fixture per rule (`F1-Malloc`, `F1-NewExpression`, `F2-PlacementOutsideCore`, `F3-UniquePtrInTest`, `F4-VectorInInternal`, `F5-StringViewInPublicHeader`, `F6-TryOutsideFirewall`, `F7-LuaCallInTool`, `F8-DynamicCast`, `F9-ThirdThreadLocal`, `F10-UsingNamespaceInHeader`, `F11-WinMacro`, `F12-IfdefInStruct`, `F13-DirectDelete`, `F14-TimeCall`, `F15-PragmaDisable`, `F16-MutableGlobal` (a `.cpp` the self-test compiles with the pinned `cl` into `${ZENITH_OUT}/gates/selftest/`), `F16-MissingSignpost`, `F16-IncompleteEntry`, `F17-EHscFlag` (a codemodel fixture), `F18-WindowsHInCore`, `F19-ForeignInl`, `F20-BarrierOutsideGraph`, `F21-SinInCook`, `F22-DlopenInRuntime`, `F23-ThirdPartyHeaderInPublic`, `F18-PthreadInPublicHeader` (a library-root header including `<pthread.h>`), `F18-WindowsHInPlatformThreading` (`<windows.h>` in the Windows threading header, which has no exception element), `F25-NoexceptInRuntime`, `F25-NodiscardInTool`, `F25-AllowWithoutReason` (`// ZENITH-ALLOW:` with nothing after it), `F25-AllowOnPreviousLine` (the signpost on the line above the `noexcept`), `F25-UnderscoreIntType` (`u_int32` in an `Internal/` source), `F25-FixedWidthInPublicHeader` (`std::uint32_t` in a library-root header other than `Zenith_Types.h`), `F25-FixedWidthInUnlistedInternal` (`int64_t` in an `Internal/` source no `f25Boundary` element names), `F25-FinalWithoutBase` (`class Zenith_X final {`), `F25-FinalTemplateWithoutBase` (`template<typename T> class Zenith_X final {`), `F26-ProfiledMutexInProfiler` (`Zenith_Mutex m_xQueueLock;` in a `Zenith2/Engine/ZenithProfiling/Internal/Hooks/` source: clause (i)), `F26-ProfiledCondVarInCoreProfiling` (`Zenith_ConditionVariable` in a `Zenith2/Engine/ZenithCore/Internal/Profiling/` source), `F26-ProfiledTemplateTrue` (`Zenith_WindowsMutex_T<true>` in a ZenithProfiling source), `F26-ProfiledSemaphoreInListedFile` (`Zenith_Semaphore` in a file the fixture's own `profilerReachable` names), `F26-DirectHookCallInEcs` (a `m_pfnOnZoneBegin(` call in a `Zenith2/Engine/ZenithECS/Internal/` source: clause (ii)), `F26-GetHooksMemberInJobs` (`Zenith_GetProfilerHooks().m_pfnOnLockAcquire` in a `Zenith2/Engine/ZenithJobs/Internal/` source), `F26-ListMalformedElement`, `F26-ListGlobPath` and `F26-ListDuplicatePath` (the three `fail` findings of `profilerReachable`, each with its own `forbidden_allowlist.json`), `F24-FileApiOutsideAllowlist`, `F24-SocketInPlatformDevServicesPasses`, `F24-MethodNamedOpenPasses` (SEC-P4's three fixtures), `F24-StdRemoveHit`, `F24-ScopedRemovePasses`, `F24-SpawnPrefixHit`, `F24-DlopenInPlatformInternalPasses` (the member, prefix and allow-record rules of SEC19, all from Z2T-PROC-045), `FS1-LuaCallOutsideFirewall`, `FS2-SecondProtectionSite`, `FS4-ThrowInFirewall`, `FS6-Setjmp`, `FS7-CompileInRuntime`) and the passing fixtures `Pass-CoreMemory` (allocation in `ZenithCore/Internal/Memory/`), `Pass-Allowlisted` (an F1-01 file), `Pass-TokensInComments` (`malloc` in a comment, `free` in a string, `Free(` and `GetTime(` in code), `Pass-RawString` (a raw string holding `throw`), `Pass-PlatformHeaderInPlatformDir`, `Pass-PlatformThreadingHeader` (the Android and Linux threading headers including `<pthread.h>` and `<semaphore.h>` under their signposts, and the Windows one including neither), `Pass-F25-AllowedNoexcept` (a `noexcept` move constructor on a line ending `// ZENITH-ALLOW: <reason>`), `Pass-F25-FinalWithBase` (`class Zenith_X final : public Zenith_Y`), `Pass-F25-ZenithTypes` (`Zenith_Types.h` aliasing `std::uint32_t`), `Pass-F25-BoundaryFile` (`int64_t` in an `f25Boundary` file under its owner's `Internal/`), `Pass-F25-TestsAndComments` (`noexcept`, `[[nodiscard]]` and `uint32_t` in a test file, and in comments and string literals of a runtime file; the identifiers `m_bFinal` and `Finalise` and the Zenith names `uint32` and `sint64`), `Pass-F26-NoProfilingInProfiler` (`Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling`, `Zenith_SemaphoreNoProfiling` and `Zenith_WindowsMutex_T<false>` in a ZenithProfiling source, and `Zenith_Mutex` in a comment there), `Pass-F26-DispatchCall` (a `Zenith_DispatchZoneBegin(` call in a ZenithECS source), `Pass-F26-HookTableInstall` (`xHooks.m_pfnOnZoneBegin = &…;`, the member's declaration and `Zenith_InstallProfilerHooks(&xHooks)` in profiler install code), `Pass-F26-HooksHeaderAndTests` (member calls in `ZenithCore/Zenith_ProfilerHooks.h` and in a `Tests/` file, and a profiled `Zenith_Mutex` in a `ZenithProfiling/Tests/` file), `Pass-F26-ProfiledOutsideScope` (`Zenith_Mutex` in a ZenithJobs `Internal/` source) and `Pass-F26-ListMissingFileReports` (a `profilerReachable` element naming no file: exit 0 with the `report` finding `F26-LIST-NO-FILE`). The self-test asserts each fixture's rule set exactly and the named cases `Test_Forbidden_DoubleRuleReportedOnce` (F6+FS4 on one `throw`), `Test_Forbidden_AllowlistNoDecisionFails`, `Test_Forbidden_AllowlistF3Refused`, `Test_Forbidden_AllowlistF25Refused` (an `entries` element naming F25 is `ALLOWLIST-FORBIDDEN-RULE`), `Test_Forbidden_F25BoundaryOutsideInternalFails` (an `f25Boundary` path outside its owner's `Internal/` is `ALLOWLIST-F25-BOUNDARY`), `Test_Forbidden_AllowlistF26Refused` (an `entries` element naming F26 is `ALLOWLIST-FORBIDDEN-RULE`), `Test_Forbidden_PlatformExceptionShape` (a `publicPlatformHeaders` element for another path, or listing `windows.h`, is `PLATFORM-HEADERS-EXCEPTION`), `Test_Forbidden_StdHeadersMatchContracts`, `Test_Forbidden_AllowlistSourcesMatchDesign`, `Test_Forbidden_WritableEntriesMatchContracts` (each of the five entries equals, member for member, the row of `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries" for its symbol: `symbol`, `source`, `decision`, `extensions` and the decision, extension and signpost text in `comment`), `Test_Forbidden_ProfilerReachableCommitted` (the committed `profilerReachable` list holds exactly the twenty-five elements of the sample, byte-ordered by `path`, each `path` under `Zenith2/Engine/ZenithCore/` or the two `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashLogMirror` sources), `Test_Forbidden_ProfilerReachableFilesExist` (every element's `path` names a file; reported as skipped while `Zenith2/Engine/ZenithCore/Zenith_SpscRing.h` does not exist, and a failure from then on), `Test_Forbidden_NoUndecidedEntry` (every `decision` of the committed file resolves in `DecisionLog.md`).

## TestSeparationCheck

`TestSeparationCheck.ps1` proves the seven conditions of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (subsection "Test separation", [Z2-N-CONTRACT-414]) as TS1 to TS7, plus TS8, which is not one of the seven: it repeats in this step, from the codemodel alone, the closure half of `Libgraph` rule LG8, so a test target reaching a Player is reported by the step a test author reads. Rows X196 and X258 are conditions 6 and 7.

- TS4 reads the game executable's PDB publics (`llvm-pdbutil dump -publics`) where the contract says "link map": the PDB carries the same public symbol set as the MSVC map file, and the pinned LLVM reads it without a second linker flag. Reconciliation R18 records the choice.
- TS4 and Libgraph LG9 demangle the decorated names with the pinned `llvm-undname` (Windows) and `llvm-nm --demangle` (Android), as F16 does, before matching.

| Condition | Detection | Inputs |
|---|---|---|
| TS1 | a source under any `Tests/` directory in a compile group of a target whose type is not a test target; a test target is one whose name ends `_Tests`, `_Conformance`, `_DefectTests`, starts `Zenith_Fuzz_` or `Zenith_Integration_`, is `ZenithTestSupport`, is a `<Name>_CompileFail_<Stem>` target, or starts `Zenith_Bench_` (benchmarks are not shipping targets) | the codemodel of every configured tree |
| TS2 | a non-test target whose link dependencies (codemodel `dependencies` plus `link.commandFragments`) name doctest, nanobench, `ZenithTestSupport`, a `<Lib>_Tests` or a `<ApiLib>_Conformance` | the same |
| TS3 | a doctest include or a `TEST_CASE`, `SUBCASE` or `TEST_SUITE` token (code view) in a file outside `Tests/` and `Zenith2/Benchmarks/` | the scan files |
| TS4 | a `doctest::` public symbol in a `win-release` game executable's PDB (`llvm-pdbutil dump -publics <ZENITH_OUT>/win/Release/bin/<Game>.pdb`, every game of `declared_deps.json`); a game with no PDB is `TS4-NO-PDB` (fail: `/DEBUG:FULL` is set in every configuration) | the `win-release` build |
| TS5 | a compile definition present on every test target's compile groups and absent from every non-test target's, whose name also appears as an identifier in a non-test source (code view or directive view) | the codemodel and the scan files |
| TS6 | an `#include` of a library-root header whose file name ends `TestAccess.h`, or a reference to a type whose name ends `TestAccess`, in a source outside `Tests/`, `Zenith2/Benchmarks/` and the `Internal/` sources of the library declaring the header (row X196) | the scan files |
| TS7 | an `#include` of a library-root header whose name ends `ToolAccess.h`, or a reference to a type whose name ends `ToolAccess`, in a source outside the sources of `KIND TOOL` targets (the `tools` records of `declared_deps.json`), `Tests/`, `Zenith2/Benchmarks/` and the declaring library's `Internal/` (row X258, EDIT-P3) | the scan files and `declared_deps.json` |
| TS8 | a shipping Player's link closure ([Z2-N-ARCH-240]) containing a test, conformance, fuzz, benchmark or integration-suite target or `ZenithTestSupport` ([Z2-N-ARCH-181]) | the `Shipping` configuration of the `win` codemodel (and `android-arm64` once configured); no build |

Fixtures: one per condition (`TS1-TestSourceInLibrary`, `TS2-LibraryLinksDoctest`, `TS3-TestCaseInInternal`, `TS4-DoctestInPlayer` (a fixture PDB publics dump, text), `TS5-TestSwitch`, `TS6-TestAccessFromRuntime`, `TS7-ToolAccessFromRuntime`, `TS8-ClosureHasTests`) and `Pass-TestAccessFromTests`, `Pass-ToolAccessFromTool`, `Pass-Minimal`.

## Libgraph

`Libgraph.ps1` runs libgraph with its CMake File API front end (`Zenith2/Docs/Design/PROC/Harness.md` → "libgraph CMake front end") over the `win-debug` build ([Z2-N-ARCH-186]) and applies the Z2 policy. It realises Z2-R-DEVT-103 (DEVT-P1).

### Libgraph invocation

```
python Tools/libgraph/libgraph.py --front-end cmake-fileapi
	--reply <ZENITH_OUT>/win/.cmake/api/v1/reply
	--declared-deps <ZENITH_OUT>/declared_deps.json
	--third-party <ZENITH_OUT>/third_party_targets.json
	--source-root <repo>/Zenith2 --config Debug
	--gate <repo>/Zenith2/Tools/Gates/libgraph_policy.json
	--baseline <repo>/Zenith2/Tools/Gates/libgraph_baseline.json
	--shipping-symbols <repo>/Zenith2/Tools/Gates/shipping_forbidden_symbols.json
	--shipping-config Shipping --shipping-reply <ZENITH_OUT>/win/.cmake/api/v1/reply
	--surface-cache <ZENITH_OUT>/gates/libgraph/surface-cache --changed <the changed set, when ticket context exists>
	--out <ZENITH_OUT>/gates/libgraph --quiet
```

`python` is the manifest's `python` pin. libgraph's exit codes are 0 (no violation), 1 (model or input error), 2 (symbol stage refused), 3 (baseline unreadable) and 4 (gate violations, added by the front-end ticket); the step maps 4 to its own exit 1 with the violations as findings, and 1, 2, 3 to exit 1 with the message as finding `LG-INPUT`.

### Rules

| Rule | Check | Severity |
|---|---|---|
| LG1 | used-but-undeclared edge: an include resolved into library B, or an undefined symbol of library A's objects defined in B's objects, with B absent from A's `depends` (and from `testDepends` for A's test targets) | fail |
| LG2 | a cycle in the declared graph, or in the real include-and-symbol graph | fail |
| LG3 | an include of `<Lib>/Internal/**` from outside `<Lib>` and its test targets | fail |
| LG4 | a third-party or `Internal/` header reachable from a public header by the transitive include closure | fail |
| LG5 | a Slang or DXC symbol (`slang::`, `spIs`, `sp*` C API names of the pinned Slang, `DxcCreateInstance`) in any runtime target's link surface | fail |
| LG6 | a third-party header included outside its owner's `Internal/` (the same set as ForbiddenCheck F23, computed from resolved includes rather than text) | fail |
| LG7 | an `INTERFACE_LIBRARY` target's undeclared include edge ([Z2-N-ARCH-243]) | fail |
| LG8 | a Shipping link closure containing `ZenithScriptCompiler`, `Luau.Compiler`, `Luau.Ast`, a test, conformance, fuzz or benchmark target, `ZenithTestSupport` or a tool target ([Z2-N-ARCH-241]) | fail |
| LG9 | a symbol of `shipping_forbidden_symbols.json` defined in a Shipping game executable ([Z2-N-ARCH-964]) | fail |
| LG10 | inbound-surface growth against the parent commit's surface (ticket context) or `libgraph_baseline.json` (no ticket context), as the review rule below defines | review-requested, or fail when uncovered; report-only while the baseline is unset |
| LG11 | a swap-contract break: a consumer of a backend family uses a symbol or type outside the family's API library ([Z2-N-ARCH-080]) | fail |

### Shipping checks scope

LG8 needs no build (the closure comes from the `Shipping` codemodel). LG9 needs the Shipping executables: the step builds the `win-shipping` build preset (`zenith_players`, through `Invoke-Zenith2Build`, so a cold Shipping tree signals the cold cap; it has its own timeout row, `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Timeout resolution") and dumps each game's `<ZENITH_OUT>/win/Shipping/bin/<Game>.pdb` with `llvm-pdbutil dump -publics`, and from Stage 2 the Android Shipping shared objects with `llvm-nm --defined-only --demangle`, when the changed set reaches `Zenith2/Engine/**`, `Zenith2/Player/**`, `Zenith2/CMake/**`, `Zenith2/ThirdParty/**` or `Zenith2/Games/**`, or when there is no ticket context (CI and acceptance). A diff that reaches none of those paths cannot change a Shipping executable's symbols, so the check is skipped with the report line `Libgraph: LG9 skipped (no Shipping input changed)`.

### The inbound-surface review rule

The inbound surface of library L is the sorted set of `(public header, symbol)` pairs of L that at least one other library's sources include or reference. A pair's consumers are those other libraries; its consumer paths are their source files that include the header or reference the symbol; its identity is `(header, symbol, consumer libraries)`.

- **The surface key.** A surface depends only on the surface inputs: the trees `Zenith2/Engine`, `Zenith2/Player`, `Zenith2/Tools`, `Zenith2/CMake`, `Zenith2/ThirdParty`, `Zenith2/Games`, `Zenith2/Benchmarks`, `Zenith2/Tests`, `Zenith2/Shaders`, the tool `Tools/libgraph`, and `Zenith2/Tools/Gates/libgraph_policy.json`, never `Zenith2/Docs/**` (which holds the board, so every `claim`, `finish` and `postFinish` changes it). The surface key is the SHA-256 of the sorted lines `<path> <tree or blob SHA>` of those paths: for the working tree, from `git write-tree` of a scratch index holding the working tree; for the parent, from `git rev-parse HEAD:<path>`. `surface.json` records `surfaceKey` beside `treeSha`.
- **The parent cache.** Every Libgraph run stores its `surface.json` at `${ZENITH_OUT}/gates/libgraph/surface-cache/<surface key>.json`, at most 64 files (a few hundred KB), oldest first out. With ticket context the parent is `HEAD` (the ticket's diff is uncommitted against it). In direct and lane mode the previous ticket's verify run had the same surface inputs as `HEAD`, because the commits after it (`finish`, the next `claim`, `postFinish` marks) touch only `Zenith2/Docs/**`. A parent miss therefore happens only on a lane's first ticket, after a human or `integrate` commit that changed surface inputs, or after eviction.
- **Per-ticket growth** (ruling PROC-R1, F8):
  - **Hit:** growth is every pair of the current surface absent from the parent's surface.
  - **Miss:** growth is every pair absent from the baseline whose header, or one of whose consumer paths, or any header in the include closure of a changed header, is in the changed set. libgraph holds the include graph, so a pair whose symbol arrives through a transitive header (the consumer includes `H1`, `H1` includes the changed `H2`) is counted.
  - Each step report records `parentSurface: "cache" | "changed-set"`.
- **Unattributed growth.** In miss mode, a pair absent from the baseline that the changed-set rule does not count (for example growth landed by another lane's `integrate` or a human commit) is listed in `surface_delta.json` as `unattributedPairs`. It is never requested from the current ticket, and MAINT's growth report lists it at the acceptance, where its review is due.
- **Covered growth.** A growth pair is covered when any pass-verdict review block (`pass` or `fixes-applied`) in a tracked ticket file under `Zenith2/Docs/Board/tickets/` lists the pair's identity in its `pairs` lines (`Zenith2/Docs/Design/PROC/Harness.md` → "3. zagent review record"). Coverage is by pair, not by file blob, so a subsequent edit to one of the pair's consumer files never un-covers a reviewed pair. A covered pair is neither requested nor failed.
- **With ticket context:** for every uncovered growth pair, the step writes a line `pair <header> <symbol> <consumer libraries>` and the pair's consumer paths to the file `$env:ZAGENT_REVIEW_SIGNAL` names, and finding `LG10` is a `report` (`surface growth: review requested`). `tick_gates.ps1` records the signal as `reviewRequest` (paths and pairs), `TICK.md` step 6.3 dispatches the reviewer, and `zagent finish --status Done` refuses unless a review record covers every requested path. The record then lists the requested pairs. This is how growth is covered by API review without an ordinary ticket editing the protected baseline ([Z2-N-CONTRACT-178]).
- **Without ticket context** (CI, the acceptance run): growth is measured against the baseline, and an uncovered pair is the `fail` finding `LG10-UNREVIEWED`. Every pair of the stage therefore reaches the stage acceptance either covered by its ticket's review record, or failing and listed in MAINT's report.
- **While the baseline is unset** (`acceptedAt` null, from Z2T-PROC-010 until the Stage 0 acceptance re-pin), LG10 is report-only in both modes.
- **Re-pinning:** only `Run-Committed.ps1 -Acceptance -Stage <N> -WriteBaseline` rewrites `libgraph_baseline.json` (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N -WriteBaseline"), after MAINT's surface growth review. `Libgraph.ps1 [-Acceptance] [-WriteBaseline]` refuses both switches outside that run (exit 2), so no tick can rewrite the baseline.
- **A change to `Tools/libgraph/**`** (a PROC ticket) follows `Zenith2/Docs/Design/MAINT/GateData.md` → "Libgraph tool changes": it attaches `surface.json` from its base tree and from its result, never lands while a lane runs, and when the two differ it is scheduled as the last ticket before the next stage acceptance's re-pin. A mid-stage re-pin is not offered, because `-WriteBaseline` exists only inside an acceptance (cross-area note PG8). The libgraph tool is a surface input, so such a change is always a cache miss for the next ticket.

### Libgraph outputs

Under `${ZENITH_OUT}/gates/libgraph/`: `violations.json`, `surface.json`, `surface_delta.json`, `swap_<family>.json` for `ecs`, `rhi`, `physics`, `compression` and `online` ([Z2-N-ARCH-080]), `shipping_closure_<Game>_<preset>.json` ([Z2-N-ARCH-240]), the surface cache, and libgraph's `index.html` report. The two surface files are inputs of MAINT's surface delta report (`Zenith2/Docs/Design/MAINT/GateData.md` → "Surface growth report"), so their formats are fixed here and change only with a `schemaVersion` bump.

`surface.json`, the current inbound surface:

```json
{
    "schema": "zenith2.libgraph-surface",
    "schemaVersion": 1,
    "surfaceKey": "<64 hex>",
    "treeSha": "<40 hex>",
    "headSha": "<40 hex>",
    "config": "Debug",
    "libraries": [
        {
            "name": "ZenithECS",
            "pairs": [
                {
                    "header": "ZenithECS/Zenith_Query.h",
                    "symbol": "Zenith_Query::Next",
                    "consumers": [ "ZenithAI", "ZenithScript" ],
                    "consumerPaths": [ "Zenith2/Engine/ZenithAI/Internal/Zenith_AIQuery.cpp", "Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptEcs.cpp" ]
                }
            ]
        }
    ]
}
```

- `libraries` holds every engine library of `declared_deps.json` that has at least one pair, byte-ordered by `name`; `pairs` is byte-ordered by `header` then `symbol`; `consumers` and `consumerPaths` are sorted and de-duplicated.
- `header` is the include path a consumer writes (`<Lib>/<Header>`); `symbol` is the demangled qualified name without parameter list, or the type name for a type reference.
- A library's test, benchmark and conformance targets are not consumers of that library.

`surface_delta.json`, this run's growth and removals:

```json
{
    "schema": "zenith2.libgraph-surface-delta",
    "schemaVersion": 1,
    "mode": "ticket",
    "base": { "kind": "parent", "surfaceKey": "<64 hex>", "acceptedAt": null },
    "surfaceKey": "<64 hex>",
    "treeSha": "<40 hex>",
    "libraries": [
        {
            "name": "ZenithECS",
            "pairsBefore": 312,
            "pairsAfter": 313,
            "added": [ { "header": "ZenithECS/Zenith_Query.h", "symbol": "Zenith_Query::Next", "consumers": [ "ZenithScript" ], "consumerPaths": [ "Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptEcs.cpp" ], "coveredBy": null } ],
            "removed": [ { "header": "ZenithECS/Zenith_World.h", "symbol": "Zenith_World::IterateAll" } ]
        }
    ],
    "requestedPaths": [ "Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptEcs.cpp" ],
    "requestedPairs": [ { "header": "ZenithECS/Zenith_Query.h", "symbol": "Zenith_Query::Next", "consumers": [ "ZenithScript" ] } ],
    "unattributedPairs": []
}
```

- `mode` is `ticket` (ticket context) or `baseline` (CI and acceptance); `base.kind` is `parent` (cache hit), `changed-set` (cache miss) or `baseline`; `base.acceptedAt` is the baseline's `acceptedAt` when `kind` is `baseline` or `changed-set`.
- `libraries` lists only libraries with at least one added or removed pair; `pairsBefore` counts the base's pairs (in `changed-set` mode, the baseline's).
- `coveredBy` is the key of a ticket whose review block lists the pair, or null; `requestedPaths` and `requestedPairs` are the consumer paths and the identities of every uncovered added pair (empty without ticket context, where they are `LG10-UNREVIEWED` findings instead); `unattributedPairs` holds the miss-mode pairs of "The inbound-surface review rule" that are absent from the baseline but not counted for this ticket (always empty on a hit and without ticket context).
- MAINT's report reads `surface.json` and the baseline and takes its "before" consumers from the previous stage's committed report (`Zenith2/Docs/Design/MAINT/GateData.md`), because the baseline stores pairs without consumers.

## Libgraph policy files

### libgraph_policy.json

```json
{
    "schema": "zenith2.libgraph-policy",
    "schemaVersion": 1,
    "comment": "Zenith2 policy for libgraph's gate mode (Zenith2/Docs/Design/PROC/StructuralGates.md, Libgraph policy files).",
    "publicHeaderRule": "root",
    "internalDirectory": "Internal",
    "testTargetPatterns": [ "*_Tests", "*_Conformance", "Zenith_Fuzz_*", "Zenith_Integration_*", "Zenith_Bench_*", "ZenithTestSupport", "*_CompileFail_*" ],
    "runtimeTargetKinds": [ "ENGINE", "MODULE", "BACKEND", "PLAYER", "GENERATED" ],
    "shaderCompilerSymbolPatterns": [ "slang::*", "sp[A-Z]*", "DxcCreateInstance*" ],
    "shippingForbiddenTargets": [ "ZenithScriptCompiler", "Luau.Compiler", "Luau.Ast", "ZenithTestSupport", "@test", "@tool" ],
    "swapFamilies": [
        { "family": "ecs", "api": "ZenithECS" },
        { "family": "rhi", "api": "ZenithRHI" },
        { "family": "physics", "api": "ZenithPhysics" },
        { "family": "compression", "api": "ZenithCompression" },
        { "family": "online", "api": "ZenithOnline" }
    ],
    "exceptions": []
}
```

`exceptions` entries are `{ "rule", "from", "to", "decision", "comment" }` (a rule exception exists only as an allowlist entry in a protected gate file with a decision ID, [Z2-N-ARCH-079]); the list is empty at Stage 0. `@test` and `@tool` stand for every target matching `testTargetPatterns` and every `tools` record.

### libgraph_baseline.json

```json
{
    "schema": "zenith2.libgraph-baseline",
    "schemaVersion": 1,
    "comment": "Inbound surface per library, rewritten only by Run-Committed.ps1 -Acceptance -Stage N -WriteBaseline in a stage acceptance ([Z2-N-CONTRACT-178]).",
    "acceptedAt": "Z2 S0 stage acceptance",
    "headSha": "<the accepted commit>",
    "libraries": [
        { "name": "ZenithCore", "surface": [ { "header": "ZenithCore/Zenith_Heap.h", "symbol": "Zenith_GetRootHeap" } ] }
    ]
}
```

At Stage 0 the file lands with `"acceptedAt": null`, `"headSha": null` and `"libraries": []`. While `acceptedAt` is null, LG10 is report-only in both modes (every pair would otherwise be growth on the scaffold's own first tick); the Stage 0 acceptance's `-WriteBaseline` writes the first real baseline, and LG10 applies from then on.

### shipping_forbidden_symbols.json

```json
{
    "schema": "zenith2.shipping-forbidden-symbols",
    "schemaVersion": 1,
    "comment": "Development-service entry symbols that must never appear in a Shipping game executable ([Z2-N-ARCH-963], [Z2-N-ARCH-964]); each entry names the area design that declares it.",
    "entries": [
        { "symbol": "Zenith_DevListenerImpl::Open", "match": "exact", "owner": "PLAT", "design": "Zenith2/Docs/Design/PLAT/Platform.md" },
        { "symbol": "Zenith_FileWatcherImpl::Start", "match": "exact", "owner": "PLAT", "design": "Zenith2/Docs/Design/PLAT/Platform.md" },
        { "symbol": "Zenith_DevClientImpl::Connect", "match": "exact", "owner": "PLAT", "design": "Zenith2/Docs/Design/PLAT/Platform.md" },
        { "symbol": "Zenith_VulkanRenderDocTool::Load", "match": "exact", "owner": "RHI", "design": "Zenith2/Docs/Design/RHI/Vulkan.md" },
        { "symbol": "Zenith_D3D12PixTool::Load", "match": "exact", "owner": "RHI", "design": "Zenith2/Docs/Design/RHI/D3D12.md" },
        { "symbol": "Zenith_D3D12RenderDocTool::Load", "match": "exact", "owner": "RHI", "design": "Zenith2/Docs/Design/RHI/D3D12.md" },
        { "symbol": "Zenith_ScriptDap", "match": "prefix", "owner": "SCRIPT", "design": "Zenith2/Docs/Design/SCRIPT/Debugger.md" },
        { "symbol": "ZenithScriptCompiler", "match": "prefix", "owner": "SCRIPT", "design": "Zenith2/Docs/Design/SCRIPT/Compiler.md" },
        { "symbol": "Luau::compile", "match": "prefix", "owner": "SCRIPT", "design": "Zenith2/Docs/Design/SCRIPT/Compiler.md" },
        { "symbol": "Luau::Parser", "match": "prefix", "owner": "SCRIPT", "design": "Zenith2/Docs/Design/SCRIPT/Compiler.md" },
        { "symbol": "Zenith_DevConsole::ExecuteStatement", "match": "exact", "owner": "DEVT", "design": "Zenith2/Docs/Design/DEVT/Console.md" },
        { "symbol": "Zenith_DevFileWriterUserMount::Write", "match": "exact", "owner": "DEVT", "design": "Zenith2/Docs/Design/DEVT/Console.md" },
        { "symbol": "Zenith_DevFrameDump::WriteFrame", "match": "exact", "owner": "DEVT", "design": "Zenith2/Docs/Design/DEVT/Console.md" },
        { "symbol": "Zenith_DevToolsServer::HandleMessage", "match": "exact", "owner": "DEVT", "design": "Zenith2/Docs/Design/DEVT/Console.md" },
        { "symbol": "___tracy_startup_profiler", "match": "exact", "owner": "DEVT", "design": "Zenith2/Docs/Design/DEVT/Profiler.md" },
        { "symbol": "___tracy_shutdown_profiler", "match": "exact", "owner": "DEVT", "design": "Zenith2/Docs/Design/DEVT/Profiler.md" },
        { "symbol": "Zenith_GraphPanelProvider::ReadDumpFile", "match": "exact", "owner": "GRAPH", "design": "Zenith2/Docs/Design/DEVT/Console.md" }
    ]
}
```

- `match` is `exact` (the demangled qualified function name, any signature) or `prefix` (any demangled name starting with the text).
- A new entry is a PROC ticket that copies the area hand-off's symbol; SEC owns the judgement of which services are development services (`Zenith2/Docs/Design/PROC.md` → "Coordination with SEC").
- Beside the entries above, Z2T-PROC-010 lands:
  - every entry of `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Consolidated entries" as that section stands when the ticket lands (SEC-P5, with `Luau::compile` as `prefix`);
  - NET's `Zenith_NetTransport::Create` (`exact`, owner NET, NT9);
  - CINE's `Zenith_CinematicCapture::Begin` and `Zenith_ExportCinematicCapture` (`prefix`, owner CINE, CINE-P4).

  An entry naming a symbol that does not exist yet is a vacuous ban, so these entries land at Stage 0 rather than with the stage that adds the symbol; NET asked for Stage 3, which this satisfies. The entries D16 and D18 (PACK, ONLINE) are added by a PROC ticket each when those areas declare them (rows SEC8, SEC13).

### Shipping exposure layers

From Z2T-PROC-054 (Stage 3), rule LG9 delegates to SEC's audit script, so the gate and SHIP's release verification share one implementation (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Gate and release placement").

1. For every Shipping game executable the LG9 build produced, the step runs `pwsh -NoProfile -File Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1 -Image <ZENITH_OUT>/win/Shipping/bin/<Game>.exe -Symbols <ZENITH_OUT>/win/Shipping/bin/<Game>.pdb -Platform windows -Out <ZENITH_OUT>/gates/libgraph/exposure_<Game>.json`, and from Stage 2 the same with `-Platform android` on each unstripped `lib<Game>.so`. The script runs layers E2 (the forbidden symbols, SEC-P5), E3 (the forbidden strings of `Zenith2/Tools/Gates/shipping_forbidden_strings.json`, SEC-P6) and E4 (the listener-import scan, SEC-P7).
2. Once per run, the step calls it with `-PositiveControl -Image <ZENITH_OUT>/win/Release/bin/Minimal.exe -Symbols <its PDB> -Platform windows`, which must find every planted control (SEC-P7).
3. Script exit 1 is the finding `LG9-EXPOSURE`, carrying the report's first finding; exit 2 is `LG-INPUT`.

Until Z2T-PROC-054 lands, LG9 is libgraph's own symbol matcher over the `--shipping-symbols` list. Z2T-PROC-054 creates `Zenith2/Tools/Gates/shipping_forbidden_strings.json` (protected) with SEC's "Initial string entries", in SEC's schema. The layers run under the same changed-path condition as LG9.
- The SHIP release checks W5 and A6 read this file through a parameter (`Zenith2/Docs/Design/SHIP/Qualification.md`), so its format is fixed by this table and changes only with a `schemaVersion` bump.

### Libgraph fixtures

`Zenith2/Tools/Gates/Tests/Libgraph/Fixtures/` holds a hand-written codemodel reply, `declared_deps.json` and source tree per rule (`LG1-UndeclaredInclude`, `LG2-Cycle`, `LG3-InternalFromOutside`, `LG4-ThirdPartyInPublic`, `LG5-SlangInRuntime` (a canned `dumpbin /symbols` text through libgraph's `--dumpbin-dir`), `LG6-ThirdPartyOutsideOwner`, `LG7-InterfaceUndeclared`, `LG8-CompilerInShipping`, `LG9-ForbiddenSymbol` (a canned `llvm-pdbutil` publics dump), `LG10-GrowthNoContext`, `LG10-GrowthWithContext` (passes, and asserts the signal file's lines), `LG10-GrowthCoveredByRecord` (a growth pair whose consumer paths a review block lists with current blob SHAs requests nothing), `LG10-ParentCacheHit` (growth against the cached parent surface only), `LG10-ParentCacheMiss` (only pairs touching the changed set), `LG10-ParentCacheMissTransitiveHeader` (a pair reached through a changed transitive header is counted), `LG10-BoardOnlyCommitHits` (a parent differing only under `Zenith2/Docs/**` is a cache hit), `LG10-ReviewedPairNotRerequested` (a reviewed pair whose consumer file a subsequent ticket edits is not requested again), `LG10-UnattributedListed`, `LG10-BaselineNullReportOnly`, `LG11-SwapBreak`), plus `Pass-Minimal`; the self-test also asserts `Test_Libgraph_WriteBaselineRefusedOutsideAcceptance`, `Test_Libgraph_SurfaceSchema` and `Test_Libgraph_SurfaceDeltaSchema` (both outputs validate against the schemas of "Libgraph outputs").

## Complexity

`Complexity.ps1` runs the analyzer with the Zenith2 profile (Z2-D-007; [Z2-N-CONTRACT-418]); it realises Z2-R-DEVT-104 (DEVT-P1).

### Complexity invocation

```
python Tools/analyze_code_complexity.py <repo> --profile zenith2 --profile-file Zenith2/Tools/Gates/complexity_profile.json --output <ZENITH_OUT>/gates/complexity --no-viz --no-md
```

The analyzer's exit code is the gate: non-zero when a `fail_on` threshold is exceeded by an entity without an exception. The step converts each over-threshold entity of the analyzer's JSON report (written to the `--output` directory by default) into a finding (`CX-<key>` at the entity's file and line).

### complexity_profile.json

```json
{
    "_doc": "Zenith2 profile for analyze_code_complexity.py (Zenith2/Docs/Design/PROC/StructuralGates.md, Complexity). Thresholds are hard failures; an exception names the entity and a Z2-D- decision.",
    "zenith2": {
        "description": "Zenith2 engine, tools, tests and benchmarks; vendor and generated code excluded.",
        "include_roots": [ "Zenith2/Engine", "Zenith2/Tools", "Zenith2/Player", "Zenith2/Benchmarks", "Zenith2/Tests" ],
        "exclude_dirs": [ "out", "ThirdParty", "Fixtures" ],
        "exclude_globs": [ "Zenith2/Tools/Gates/Tests/**" ],
        "ignore_macros": [ "ZENITH_ASSERT", "ZENITH_CHECK" ],
        "fail_on": {
            "max-cc": 25,
            "max-cognitive": 30,
            "max-nesting": 5,
            "max-loc": 150,
            "max-file-loc": 1500,
            "max-include-cycles": 0,
            "max-missing-pragma-once": 0,
            "max-forbidden-tokens": 0
        },
        "lints": { "scope_globs": [ "Zenith2/Engine/*/*.h" ], "exclude_generated": true, "allowlist_file": "Zenith2/Tools/Gates/complexity_lints_allowlist.txt" },
        "exceptions": []
    }
}
```

<!-- elaboration -->
- The Stage 0 thresholds are starting values, because no Zenith2 code exists to calibrate against: `max-cc` 25 and `max-cognitive` 30 are the common upper bounds for reviewable functions, `max-nesting` 5 and `max-loc` 150 bound a function to one screenful at the profile's 140-column limit, and `max-file-loc` 1500 keeps every file inside a single 2,000-line read. Recalibration from the measured distribution follows `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration" (row X389):
  - At each of the Stage 4 to Stage 7 acceptances (ruling MAINT-R1 R3), a MAINT `needs-human` ruling ticket `Z2 S<N> complexity calibration` commits the measurement, `complexity_profile.proposed.json` and its decision.
  - A PROC `needs-human` apply ticket, blocked by that ruling, copies the proposed profile over this file. PROC drafts the four apply tickets now: Z2T-PROC-062, -073, -083 and -095 (`Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map").
  - An apply ticket refuses to copy when the SHA-256 of the current profile differs from the `baseSha256` the calibration recorded, so an exception admitted after the ruling is never overwritten.
- `max-forbidden-tokens` 0 over public headers duplicates part of ForbiddenCheck F5 (the analyzer's built-in token list is four STL names); `max-missing-pch` is deliberately absent, because Zenith2 has no precompiled header ([Z2-N-CONTRACT-425]).
- `exceptions` entries (the generic key Z2T-PROC-004 adds) are `{ "key": "max-cc", "entity": "<qualified function name or repo-relative file>", "limit": <number>, "decision": "Z2-D-NNN", "comment": "<why>" }`; an entry without a decision is an analyzer configuration error (exit 2), and an entry whose entity no longer exceeds the default threshold is reported stale.
- `complexity_lints_allowlist.txt` exists, empty except for its header comment.

Fixtures: `CX-max-cc` (a function with 26 branches), `CX-max-file-loc` (a 1,501-code-line file the self-test generates), `CX-max-nesting`, `CX-ExceptionNoDecision`, `Pass-Exception` (a function over `max-cc` with a valid exception), `Pass-Minimal`.
