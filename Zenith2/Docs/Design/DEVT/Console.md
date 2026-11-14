# DEVT design: console, logging tools and remote diagnostics

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies the development console `Zenith_DevConsole` (the registration API GRAPH, JOBS, ECS, SCENE, RPROD, SCRIPT and the modules use), the development file writer, the runtime development commands and the DEVT frame dump, the JSON Lines file log sink and its record schema, the remote diagnostics server and its protocol, the host client, and the Shipping absence of every development service.

## Scope of this file

| Concern | Owner | Specified here |
|---|---|---|
| the log router, `Zenith_LogSink`, the ring and console sinks | CORE (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router") | the file sink and the on-screen console's use of the ring |
| the loopback listener, its token and Shipping exclusion | PLAT (`Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket", "Shipping exclusion") | the "profiler" service on it and its protocol |
| each area's console commands and dump formats | the area (for example `jobs.dump`, `r.graph.dump`) | the registry, grammar, execution and file output they use |
| the configuration store | CORE (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store") | the `set` and `get` commands |

## Zenith_DevConsole header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_DevConsole.h
#pragma once
#include "ZenithCore/Zenith_ConfigStore.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_DevFileWriter.h"

class Zenith_LogRingSink;

inline constexpr uint32 uDEV_CONSOLE_MAX_COMMANDS = 1024;
inline constexpr uint32 uDEV_CONSOLE_MAX_LINE_BYTES = 1024;
inline constexpr uint32 uDEV_CONSOLE_MAX_TOKENS = 32;
inline constexpr uint32 uDEV_CONSOLE_QUEUE_LINES = 64;
inline constexpr uint32 uDEV_CONSOLE_HISTORY_LINES = 256;
inline constexpr uint32 uDEV_CONSOLE_OUTPUT_BYTES = 65536;   // per command execution

enum Zenith_DevConsoleError : uint8 { /* the enumerators of Zenith2/Docs/Design/DEVT.md "Error enumerations" */ };
constexpr const char* Zenith_ToString(Zenith_DevConsoleError eError);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_DevConsoleError eError);

/// @contract value Zenith_DevCommandArgs
/// Ownership: A view built by the console for one handler call; every token views the console's line buffer.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Invalid after the handler returns; handlers copy what they keep.
/// Units: None.
/// Errors: The parse helpers return false for a missing or malformed argument and leave the out value unchanged.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_DevCommandArgs
{
public:
	uint32 GetCount() const;                         // arguments after the command name
	Zenith_StringView Get(uint32 uIndex) const;      // empty view beyond GetCount
	Zenith_StringView GetLine() const;                // the whole statement as typed
	bool GetInt(uint32 uIndex, sint64& iOut) const;
	bool GetUnsigned(uint32 uIndex, uint64& ulOut) const;
	bool GetFloat(uint32 uIndex, double& dOut) const;
	bool GetBool(uint32 uIndex, bool& bOut) const;   // 0/1, true/false, on/off
	bool HasFlag(Zenith_StringView strFlag) const;    // an argument equal to "--<flag>"
};

/// @contract type Zenith_DevCommandOutput
/// Ownership: Owned by the console; passed by reference to a handler for one call. Text goes into the console history,
///   the log (category devtools.console, LOG_LEVEL_INFO for Print, LOG_LEVEL_WARNING for Error) and, for a remote
///   command, the COMMAND_REPLY message.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Valid for the handler call alone.
/// Units: Bytes.
/// Errors: Text beyond uDEV_CONSOLE_OUTPUT_BYTES per execution is cut and "<output cut>" appended.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_DevCommandOutput
{
public:
	void Print(Zenith_StringView strLine);
	void Error(Zenith_StringView strLine);                          // marks the execution failed
	Zenith_DevFileWriter* GetFileWriter() const;      // null when no writer is set (tests, the editor without a user mount)
	bool IsRemote() const;                            // the line came from the remote server
};

using Zenith_PfnDevCommand = void (*)(const Zenith_DevCommandArgs& xArgs, Zenith_DevCommandOutput& xOutput, void* pUserData);

/// @contract value Zenith_DevCommandDesc
/// Ownership: Plain value; RegisterCommand copies the strings into the registry's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: RegisterCommand returns DEV_CONSOLE_ERROR_INVALID_NAME, DEV_CONSOLE_ERROR_DUPLICATE_NAME or
///   DEV_CONSOLE_ERROR_REGISTRY_FULL.
/// Determinism (the registrant's obligation, Z2-D-019): m_pfnHandler never writes simulation-visible state directly
///   (a component value, a script-visible value, a replayed quantity, a schedule). A command whose effect must reach a
///   simulation submits it through that world's recorded input-command stream, so a replay reproduces it, or refuses
///   itself with Zenith_DevCommandOutput::Error while the world records or replays. Review enforces it
///   (Zenith2/Docs/Design/DEVT.md "Determinism").
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_DevCommandDesc
{
	const char* const m_szName = nullptr; // "<area>.<name>[.<name>…]", Console.md "Command registry"
	const char* const m_szUsage = "";     // e.g. "<frames> [--locks]"
	const char* const m_szHelp = "";      // one line, at most 120 bytes
	const Zenith_PfnDevCommand m_pfnHandler = nullptr;
	void* const m_pUserData = nullptr;
	const bool m_bRemoteAllowed = true;   // false: refused from the remote server (commands that read host paths)
};

/// @contract type Zenith_DevCommandToken
/// Ownership: Move-only; owned by the registrant. Its destructor (or Reset) unregisters the command.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Inert after Reset or after being moved from. A token must be destroyed or Reset before its console is
///   destroyed: ~Zenith_DevConsole asserts (ZENITH_ASSERT, every configuration with asserts enabled) that no command is
///   still registered, because a token that outlived its console would unregister through a dangling pointer.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_DevCommandToken
{
public:
	Zenith_DevCommandToken() = default;
	Zenith_DevCommandToken(Zenith_DevCommandToken&& xOther);
	Zenith_DevCommandToken& operator=(Zenith_DevCommandToken&& xOther);
	Zenith_DevCommandToken(const Zenith_DevCommandToken&) = delete;
	Zenith_DevCommandToken& operator=(const Zenith_DevCommandToken&) = delete;
	~Zenith_DevCommandToken();
	void Reset();
	bool IsRegistered() const;
private:
	class Zenith_DevConsole* m_pxConsole = nullptr;
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
};

/// @contract value Zenith_DevConsoleDesc
/// Ownership: Plain value copied by Create; the referenced objects outlive the console.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns DEV_CONSOLE_ERROR_NOT_IN_SHIPPING in Shipping and DEV_CONSOLE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DevConsoleDesc
{
	Zenith_ConfigStore* const m_pxConfig = nullptr;  // null: set, get and cvars are unavailable (tests)
	const Zenith_ConfigSchema* const m_pxSchema = nullptr;
	Zenith_DevFileWriter* const m_pxFileWriter = nullptr;
	Zenith_LogRingSink* const m_pxLogRing = nullptr; // the on-screen console's scrollback source
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_DevConsole
/// Ownership: Created by Create as a Zenith_Owned<Zenith_DevConsole>, owned by ZenithRuntime's Zenith_DevTools (game),
///   the editor's development-tools object, or a test. Owns the registry, the line queue and the history.
/// Thread affinity: MAIN_THREAD except QueueLine (ANY_THREAD).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: As each member; handlers' failures are reported through their output, never returned.
/// Reentrancy: ALLOWED(Execute) from inside a handler: a command may run other lines (alias-style), to a depth of 4;
///   deeper calls fail with DEV_CONSOLE_ERROR_PARSE "nesting too deep". RegisterCommand and token destruction are
///   allowed from inside a handler and take effect after the outermost Execute returns.
/// Locks: none; QueueLine uses a bounded lock-free queue.
/// Version: since Z2 Stage 2. Absent from Shipping (Create refuses there, so no instance exists). Its state is private
///   members of this class (registry slots, sorted index, name arena, line queue, history, nesting depth), declared in the
///   header's private section with no implementation class; the private member ExecuteStatement, defined in the
///   development-only translation unit Internal/Console/Zenith_DevConsoleExecute.cpp and called only under
///   if constexpr (!Zenith_IsShippingBuild()), is the forbidden Shipping symbol (see the
///   section "Shipping absence of development services" of this file).
/// Testing: concrete (no base, no virtual member) and never doubled; tests of a registrant (GRAPH's executor, ECS, SCRIPT, modules) construct the real
///   console with Create(Zenith_DevConsoleDesc{}) (null config, writer and ring are legal), run lines with Execute and
///   inspect the registry with HasCommand and GetCommandCount.
/// Example: Zenith2/Engine/ZenithProfiling/Tests/Examples/Zenith_DevConsole_Example.cpp
/// @endcontract
class Zenith_DevConsole
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DevConsole>, Zenith_DevConsoleError> Create(const Zenith_DevConsoleDesc& xDesc);
	~Zenith_DevConsole();   // ZENITH_ASSERT that GetCommandCount() equals the built-ins' count (every token is gone)

	Zenith_Result<Zenith_DevCommandToken, Zenith_DevConsoleError> RegisterCommand(const Zenith_DevCommandDesc& xDesc);
	bool HasCommand(Zenith_StringView strName) const;    // registered now (pending registrations excluded)
	uint32 GetCommandCount() const;                     // the built-ins included

	// Runs one line (one or more statements) now. Returns the first failing statement's error; every statement before it ran.
	Zenith_Result<void, Zenith_DevConsoleError> Execute(Zenith_StringView strLine);
	// Queues a line from any thread (the remote server, the editor, tests); run by ExecuteQueued.
	Zenith_Result<void, Zenith_DevConsoleError> QueueLine(Zenith_StringView strLine);
	void ExecuteQueued();                 // frame-loop step 3 (Zenith_DevTools::EndFrame)

	struct HistoryLine { Zenith_StringView m_strText; uint8 m_uKind; };   // 1 input, 2 output, 3 error
	Zenith_Span<const HistoryLine> GetHistory() const;
	uint32 Complete(Zenith_StringView strPrefix, Zenith_Span<Zenith_StringView> axOut) const;  // names in byte order
	void SetFileWriter(Zenith_DevFileWriter* pxWriter);
	uint64 GetResidentBytes() const;   // every buffer Create allocated (part of Z2-B-DEVT-001)
private:
	// One statement already tokenised: lookup, handler call, output and history (Command execution steps 2 to 5).
	// Defined in Internal/Console/Zenith_DevConsoleExecute.cpp; the Shipping-forbidden symbol of the console.
	Zenith_Result<void, Zenith_DevConsoleError> ExecuteStatement(uint32 uStatement, bool bRemote);
	// … the registry, queue and history members, declared by the landing ticket
};
```

Consumers receive the console as `Zenith_DevConsole*` (null in Shipping and in tests that omit it): GRAPH through `Zenith_FrameExecutorDesc::m_pxConsole` (its tests construct the real console, row DV25), JOBS and the executor through the same field, RPROD through `Flux_RendererDesc::m_pxDevConsole` (row DV5), ECS, SCENE, ASSET and SCRIPT through their create descs as their designs name them, and modules through `Zenith_ModuleStartContext::GetDevConsole()` (row DV12). A null console registers nothing; a registrant keeps its tokens as members and destroys them before the console (the console is destroyed in shutdown step 10b, after every module and library that registered).

## Zenith_DevFileWriter header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_DevFileWriter.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"

inline constexpr uint64 uDEV_FILE_MAX_BYTES = 64ull << 20;
inline constexpr uint32 uDEV_FILE_MAX_PENDING = 8;

enum Zenith_DevFileError : uint8 { /* the enumerators of Zenith2/Docs/Design/DEVT.md "Error enumerations" */ };
constexpr const char* Zenith_ToString(Zenith_DevFileError eError);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_DevFileError eError);

/// @contract interface Zenith_DevFileWriter
/// Ownership: Implemented by ZenithRuntime (Zenith_DevFileWriterUserMount, over Zenith_AsyncIO::WriteAtomic on the user
///   mount) and by tests (Zenith_TestDevFileWriter); owned by Zenith_DevTools; passed to consumers as a pointer.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: xBytes is copied before Write returns.
/// Units: Bytes.
/// Errors: DEV_FILE_ERROR_BAD_NAME, DEV_FILE_ERROR_TOO_LARGE, DEV_FILE_ERROR_BUSY, DEV_FILE_ERROR_IO,
///   DEV_FILE_ERROR_NOT_IN_SHIPPING. Completion failures are logged by the implementer
///   ("devtools.console: write '<name>' failed: <error>"), never reported to the caller.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_DevFileWriter
{
public:
	virtual ~Zenith_DevFileWriter() = default;
	// strRelativeName: [A-Za-z0-9_.-]+ segments separated by '/', at most 4 segments and 128 bytes, no "." or ".." segment;
	// the file lands at user:/devtools/<strRelativeName>, replacing an existing one atomically.
	virtual Zenith_Result<void, Zenith_DevFileError> Write(Zenith_StringView strRelativeName, Zenith_ByteSpan xBytes) = 0;
	// The host-visible location of the file, for the reply text ("written to <path>").
	virtual Zenith_StringView DescribeLocation() const = 0;
};
```

- Users: `script.profiler.dump` (SCRIPT, SB13), DEVT's own `devtools.dump` frame dump and `devtools.telemetry.flush`, and any further command that writes a development file and is given the writer. Two existing commands keep their own, already-specified paths and do not use this interface: GRAPH's `jobs.dump <path>` writes through `Zenith_FrameExecutorDesc::m_pfnDumpWrite`, which ZenithRuntime implements over the user mount (Z2T-MOD-061, `Zenith2/Docs/Design/GRAPH.md` → "Console commands"), and ECS's `ecs.dump <worldTag> <path>` (§ "Runtime development commands"). Both still write the user mount only, so no development command reaches a host path.
- The runtime implementation, `Zenith_DevFileWriterUserMount` (`Zenith2/Engine/ZenithRuntime/Internal/DevTools/Zenith_DevFileWriterUserMount.h`, a `final` class: `Zenith_DevFileWriterUserMount(Zenith_AsyncIO& xIO, Zenith_Heap& xHeap)`, the two overrides, `Zenith_Result<void, Zenith_DevFileError> WriteUserPath(Zenith_StringView strUserPath, Zenith_ByteSpan xBytes)` (the same slots, size limit and completion handling for a whole user-mount virtual path that `Zenith_ParseVirtualPath` accepts with the user root, used by `ecs.dump`, § "Runtime development commands"; any other root, a host path or a traversal is `DEV_FILE_ERROR_BAD_NAME`), `void Reap()` called at frame-loop step 3, `void CancelPending()` called by `Zenith_DevTools::BeginShutdown`, `void WaitIdle()` called by `Zenith_DevTools::DetachOutputs`, and eight `{ Zenith_Owned<uint8[]> bytes; Zenith_OperationHandle op; }` slots), begins `Write` and `WriteUserPath` with `if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(DEV_FILE_ERROR_NOT_IN_SHIPPING); }` (the composition root constructs none in Shipping; a `ShippingTest` case that constructs one sees the refusal), validates the name, refuses more than `uDEV_FILE_MAX_BYTES` (64 MiB) with `DEV_FILE_ERROR_TOO_LARGE`, takes a free slot (`DEV_FILE_ERROR_BUSY` when all eight are pending), allocates a buffer of exactly the write's size with `Zenith_TryNew` (failure: `DEV_FILE_ERROR_IO`, nothing submitted), copies the bytes, submits `Zenith_AsyncIO::WriteAtomic` to `user:/devtools/<name>` and frees the buffer when `Reap` finds the operation complete, so a slot's buffer is owned by the writer from submission until the reap that sees its completion; `CancelPending` requests cancellation of every pending operation (a cancelled operation completes and is reaped like any other), and `WaitIdle` reaps until no slot is pending or 2 s pass; `devtools.files.pending` counts pending slots. No buffer exists while nothing is pending, so the writer is outside Z2-B-DEVT-001's resident figure.
- `Test_DevFileWriter_BusyAndTooLarge`: the ninth write while eight are pending returns `DEV_FILE_ERROR_BUSY` and submits nothing; a 64 MiB + 1 byte write returns `DEV_FILE_ERROR_TOO_LARGE`; after one completion is reaped a write succeeds again; run against the runtime writer over a scratch user mount in `ZenithRuntime_Tests` and against `Zenith_TestDevFileWriter` (which applies the same two limits).
- On Android `user:/devtools/` is inside the app's internal storage and is fetched with the remote client's `FILE_GET` (§ "Remote messages") or `adb exec-out run-as <package> cat files/devtools/<name>` on a debuggable build.

## Console grammar

```
line       := statement { ";" statement }
statement  := [ token { blank token } ]
token      := bare | quoted
bare       := tokenbyte { tokenbyte }          ; any byte other than blank, ";", "\"" and control bytes
quoted     := "\"" { qbyte | escape } "\""
escape     := "\\" ( "\"" | "\\" | "n" | "t" )
blank      := " " | "\t"
```

- Limits: a line at most 1,024 bytes (`DEV_CONSOLE_ERROR_LINE_TOO_LONG`), at most 32 tokens per statement, at most 16 statements per line; bytes must be valid UTF-8 and contain no control byte other than tab (`DEV_CONSOLE_ERROR_PARSE` with the column).
- An unterminated quote or an unknown escape is `DEV_CONSOLE_ERROR_PARSE`; nothing of the line runs when any statement fails to parse (the whole line is tokenised first).
- Statement form: `<command> <argument>…`; the first token is the command name; the remaining tokens are the arguments in order. A `#` token begins a comment that runs to the end of the statement.
- The tokeniser writes into a 1,024-byte buffer owned by the console and records each token as an offset and length, so a statement's tokens are views into it; no allocation per line.

## Command registry

- Names: `[a-z][a-z0-9]*` segments joined by `.`, at least two segments for area commands (`jobs.dump`, `r.graph.aliasTransients`) and one segment for the built-ins (§ "Built-in commands"), at most 64 bytes in total, and camel case allowed after the first byte of a segment (`r.pso.dumpMisses`); the first segment names the owning area or its prefix (`r` for rendering, `ecs`, `scene`, `asset`, `script`, `jobs`, `graph`, `plat`, `stream`, `audio`, `anim`, `ui`, `vfx`, `game`, `save`, `profiler`, `devtools`).
- Storage: a fixed table of 1,024 slots of 64 bytes (`{ name, usage, help, handler, user data, remote flag, generation }`, strings copied into a 64 KiB arena) plus a sorted index rebuilt on each registration; lookups are binary searches by name bytes. With the 64-line queue (a CORE `Zenith_BoundedQueue` of 1,088-byte cells holding the 1,028-byte `Zenith_DevConsoleQueuedLine`: 69,632 bytes from the root heap, counted through its `GetAllocatedBytes()`), the 256 × 512-byte history (131,072), the 1,024 × 64-byte registry slots (65,536), the 1,024 × 2-byte sorted index (2,048), the 64 KiB name arena, the 64 KiB per-execution output buffer and the 1,024-byte tokeniser buffer, the console's resident bytes are 400,384, allocated by `Create` (`Zenith2/Docs/Design/DEVT/Profiler.md` → "Memory layout and accounting").
- Registration and unregistration are main-thread calls (`ZENITH_ASSERT`); a token's destruction marks its slot free and bumps its generation, so a stale token from a destroyed console is detected by generation in Debug.
- Configuration keys are not registered as commands: `set` and `get` reach every key of the schema (§ "Configuration variables").

## Command execution

1. `Execute(line)` tokenises the line (§ "Console grammar"); a parse error appends the input and an error line to the history and returns.
2. For each statement in order: an empty statement is skipped; the name is looked up; an unknown name replies `unknown command '<name>'; try 'help <prefix>'` and fails with `DEV_CONSOLE_ERROR_UNKNOWN_COMMAND`, and the remaining statements of the line are not run.
3. The handler is called with the arguments, an output object and its user data; the zone `DevTools.Command` carries the command name as its dynamic name.
4. A handler that wrote `Error` marks the statement failed (counted in `devtools.console.errors_delta`); the line continues with the next statement only when the failed statement ended with `;` and the next statement begins with `?` (a "continue on error" marker: `?r.capture 1`); otherwise the remaining statements are skipped.
5. Output and input lines go into the history ring (256 lines of at most 512 bytes; older lines drop) and the log.

`QueueLine` copies the line into CORE's `Zenith_BoundedQueue` of 64 entries (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"; each entry a 1,024-byte line plus its length and remote flag, `QUEUE_FULL_POLICY_REJECT`, the triple `devtools.console.queue.full`, `.dropped`, `.high_water`); `ExecuteQueued` drains every queued line at frame-loop step 3, where no job runs, so handlers may read any main-thread state the composition root owns and change development state (configuration keys through `set`, overlay and profiler state, debug views); they never write simulation-visible state, which `Zenith_DevCommandDesc`'s determinism rule forbids. A command never blocks: a handler that needs a long operation starts it (a dump write, a capture) and returns; its completion is logged.

## Built-in commands

Registered by the console itself:

| Command | Usage | Effect |
|---|---|---|
| `help` | `[<prefix>]` | lists commands (name, usage, help) whose name starts with the prefix, in byte order |
| `set` | `<key> <value>` | § "Configuration variables" |
| `get` | `<key-or-prefix*>` | the resolved value, its layer and its source; a trailing `*` lists every key with that prefix |
| `keys` | `[<prefix>]` | lists configuration keys with type, range and description |
| `echo` | `<text>…` | prints its arguments joined by spaces |
| `history` | `[<n>]` | prints the last n input lines |

Registered by `Zenith_DevTools` (ZenithRuntime) at startup step 7:

| Command | Usage | Effect |
|---|---|---|
| `exec` | `<file>` | runs each line of `content:/Dev/<file>.cfg` (a development-content file; `<file>` one segment of the file-writer name rules; at most 64 KiB and 256 lines). The console cannot read files (ZenithProfiling has no I/O), so `exec` is ZenithRuntime's: the handler submits one `Zenith_AsyncIO` read of the whole file into the one exec slot `Zenith_DevTools` owns (`{ Zenith_Owned<uint8[]> buffer; Zenith_OperationHandle op; }`, the 64 KiB buffer allocated at submission and freed when the read is consumed or cancelled), replies `exec: reading <file>` and returns at once (a command never blocks); `Zenith_DevTools::EndFrame` polls the operation at each frame-loop step 3 and, when it completes, calls `Zenith_DevConsole::Execute` on each line in order on the main thread, logging `exec <file>: <n> lines` or the read error; a line that fails is logged and the next line still runs; at most one `exec` is pending (a second replies `exec: busy`) |
| `profiler.visible` | `hidden\|compact\|full` | `Zenith_ProfilerOverlay::SetState` |
| `profiler.panel` | `<id>` | `SelectPanel` |
| `profiler.freeze`, `profiler.unfreeze` | — | freeze |
| `profiler.filter` | `[<text>]` | the counter and config panels' filter |
| `profiler.capture` | `<frames> [--locks]` | `Zenith_Profiler::StartCapture`; replies with the capture ID and the location |
| `profiler.stop` | — | `StopCapture` |
| `profiler.mode` | `capture\|counters` | `SetMode` |
| `profiler.counters` | `[<pattern>]` | prints matching counters' frame values |
| `devtools.marker` | `<text>` | a capture `MARKER` chunk and a log line |
| `devtools.dump` | `[<frames>]` | § "DEVT frame dump" for the next frames (default 1) |
| `devtools.telemetry.flush` | — | writes the run telemetry record now |
| `crash.list` | — | lists finalized reports on the user mount |
| `crash.test` | `access\|abort\|assert\|hang` | a deliberate crash, for the device runs (refused unless `--confirm` is passed) |

## Configuration variables

- `set <key> <value>` resolves the key through `Zenith_ConfigSchema::FindKey`, refuses (with the reason as the reply) a key that is unknown, that is `m_bSimulationAffecting` (`refused: simulation-affecting key; pass --set at startup`, so a typed command never perturbs a replay, Z2-D-019), that is `m_bRequiresRestart` (`refused: restart required; pass --set at startup`), or whose `m_uAllowedLayerMask` lacks `CONFIG_LAYER_COMMAND_LINE`; otherwise it calls `Zenith_ConfigStore::ApplyValueText(CONFIG_LAYER_COMMAND_LINE, key, value, "console", 0)`, which validates the type and range and records the provenance `console`.
- Refusal detection: `ApplyValueText` reports a per-key problem (wrong type, out of range, layer not allowed) as a diagnostic with a successful result (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" step 2), so `set` reads the store's diagnostic count before the call and the new diagnostics after it: when the call added a diagnostic for the key, or the key's resolved value and winning layer are unchanged although the text differed from the current value, `set` replies `refused: <the diagnostic's message>` (or `refused: value unchanged`) and counts `devtools.console.errors_delta`; only a changed resolved value replies `<key> = <value> (commandLine, console)`.
- The value is session-only (the command-line layer is never persisted), so a debug view toggled from the console is not written into the user's settings file.
- Shadowing: the command-line layer outranks the user layer, so for the rest of the session a key set from the console hides any subsequent change of the same key in the user layer (an options-menu change, a `Zenith_Config` user write); the user value is still stored and persisted, and becomes visible at the next launch. CORE's store has no per-key clear of one layer (only `ClearLayer`, which would also drop the process's real `--set` values), so the console offers none; `set` appends ` (shadows the user layer until restart)` to its reply whenever the key also has a user-layer entry (`GetLayerEntry(key, CONFIG_LAYER_USER)` non-null). `Test_DevConsole_SetShadowsUserLayer` asserts that a subsequent user-layer commit of the key leaves the resolved value at the console's value and that the reply carries the note.
- Readers observe the change through the store's revision and change registry: <!-- assumption: pending CORE --> `ApplyValueText` called after startup increments `GetRevision()` and dispatches the change registry for each key whose resolved value changed, as `CommitStagedUserValues` does for user values; this is recorded as row DV22. <!-- /assumption --> Because `set` runs at frame-loop step 3, the change is visible to every reader from the next iteration's prologue. `Test_DevConsole_SetDispatchesChangeRegistry` registers a callback on the store's change registry and asserts it fires once for a `set` that changes the resolved value and not for a refused or unchanged `set`.
- Debug views and toggles of every area are configuration keys (`r.debug.*`, `r.graph.*`, `ecs.debug.*`, `stream.debug.*` and so on), so "debug variables" of the ENV and GEOM requests are these keys, typed with `set` and listed with `keys`; an area declares a key `CONFIG_LAYER_COMMAND_LINE`-settable to make it console-settable.
- `get` prints `key = value (layer, source:line)`, and the default when no layer sets it.

## Console output and history

- Every input line, output line and error line of an execution is logged under `devtools.console` (input at `LOG_LEVEL_INFO` as `> <line>`), so a console session is in the JSON Lines log and in a capture's `LOG` chunk.
- The history ring keeps 256 lines; `GetHistory` returns them oldest first. `Complete(prefix)` returns command names and, after `set `/`get `, key names that start with the prefix, at most `axOut.size()` of them, in byte order.
- A remote command's output is returned in its `COMMAND_REPLY` and also enters the history marked remote.

## On-screen console

The on-screen console is drawn by `Zenith_DevTools` through `Zenith_DebugDraw` screen primitives, toggled by PLAT's `Dev.ToggleConsole` (the grave key):

- Layout: the top 40% of the view, below the profiler's top bar when both are open; the last 30 lines of the log ring sink (`Zenith_LogRingSink::CopyRecentLines`, filtered to level `INFO` and above unless `devtools.console.verbose` is set with `set`) interleaved with console history by sequence, then the input line with a caret.
- Text input: while open, the console begins a text session through PLAT's `Zenith_TextInput` (`Engine.TextEntry` pinned, so the game and UI receive no keyboard input); `Return` executes the line, `Up`/`Down` walk the input history, `Tab` completes with `Complete`, `Escape` or the grave key closes it.
- On the S24 the console opens with a four-finger tap and uses the OS soft keyboard through the same text session; most device work uses the remote console instead (§ "Remote client").
- The console view is drawn after the overlay so it is on top; its draw cost is part of `DevTools.EndFrame`.
- Before PLAT's text input exists (Stage 3), the console has no on-screen input; lines arrive from tests, the editor and the configuration key `devtools.console.exec` (a `;`-separated line set on the command line, run once at the first `ExecuteQueued`).

## Runtime development commands

`Zenith2/Engine/ZenithRuntime/Internal/DevTools/Zenith_RuntimeDevCommands.cpp` registers, at startup step 13 (after every subsystem exists), the commands whose data sits in objects the composition root owns:

| Command | Effect | Data source |
|---|---|---|
| `ecs.dump` | ECS's own argument form, unchanged: `ecs.dump <worldTag> <path>` (`Zenith2/Docs/Design/ECS.md` → "Debug views dumps and telemetry"; Z2T-ECS-069). The handler finds the live world with that tag (an unknown tag replies `ecs.dump: no world <tag>`), writes its `zenith2.ecs-dump` document with ECS's dump writer into a `Zenith_Array<char>` local to the handler and hands it to `Zenith_DevFileWriterUserMount::WriteUserPath(<path>, …)`, which copies it into a slot buffer it owns until the write completes and is reaped (the handler's array dies when the handler returns, and the write is cancelled at shutdown and waited for like every development-file write, `Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown"); `<path>` must be a user-mount virtual path (`user:/…`, accepted by `Zenith_ParseVirtualPath` with the user root, exactly the rule Z2T-MOD-061's `jobs.dump` callback applies; any other root, a host path or a traversal is refused with `ecs.dump: only user:/ paths`); the handler never waits, and a busy writer replies `ecs.dump: busy`; `Test_RuntimeDevCommands_EcsDumpArgumentForm` asserts the form, the refusal and the written document | ECS's world inspector and dump writer |
| `asset.dump` | `zenith2.asset-dump` through `Zenith_AssetRegistry::WriteDump` to `user:/devtools/assets/<frame>.json` (ASSET's text names `user:/logs/`; every development file goes through the file writer's `user:/devtools/` root, row DV19) | ASSET's registry |
| `scene.dump` | the loaded scenes and their entity counts | SCENE's scene registry |
| `config.dump` | the configuration store's `WriteDump` | CORE |
| `memory.stats` | the governor's `GetStats` printed as text | CORE |
| `startup.report` | `Zenith_Runtime::GetStartupReport()` printed | MOD |
| `world.list` | live worlds with tags, tick counts and entity counts | MOD |

The writers belong to the data's areas; this file only registers the command and calls them (row DV19). Libraries below ZenithRuntime that receive the console in their descs register their own commands (GRAPH's `jobs.recompile`, `jobs.dump <path>` written through the executor's dump-write callback of Z2T-MOD-061; RPROD's `r.*`; SCRIPT's `script.*`), and modules register theirs through the start context (`stream.dump` is registered by ZenithStreaming's module start, row DV21).

## DEVT frame dump

`Zenith_DevFrameDump.h` declares:

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_DevFrameDump.h
#pragma once
#include "ZenithCore/Zenith_CallbackToken.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_JsonWriter.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_ProfilerTypes.h"

class Zenith_Profiler;
class Zenith_DevFileWriter;

/// @contract interface Zenith_DevFrameDumpSection
/// Ownership: Implemented by ZenithRuntime (platform, ECS, asset, input sections) and by any library above ZenithProfiling;
///   registered with Zenith_DevFrameDump::AddSection; unregistered before the implementer is destroyed.
/// Thread affinity: MAIN_THREAD. Write is called at frame-loop step 3.
/// Invalidation: None.
/// Units: None.
/// Errors: A section returns its JSON writer error, which ends the dump with "truncated": true.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_DevFrameDumpSection
{
public:
	virtual ~Zenith_DevFrameDumpSection() = default;
	virtual Zenith_StringView GetName() const = 0;   // the member name, [a-z][a-zA-Z0-9]*
	virtual Zenith_Result<void, Zenith_JsonWriteError> Write(Zenith_JsonWriter& xWriter) = 0;   // one object
};
```

and, in the same header:

```cpp
/// @contract value Zenith_DevFrameDumpDesc
/// Ownership: Plain value copied by Create; the profiler and the file writer outlive the frame dump.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns PROFILER_ERROR_INVALID_DESC for a null profiler or writer.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DevFrameDumpDesc
{
	const Zenith_Profiler* const m_pxProfiler = nullptr;
	Zenith_DevFileWriter* const m_pxFileWriter = nullptr;
	const uint64 m_ulBuildIdentity = 0;
	const uint64 m_ulCompositionHash = 0;
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_DevFrameDump
/// Ownership: Created by Create as a Zenith_Owned<Zenith_DevFrameDump>, owned by ZenithRuntime's Zenith_DevTools (never
///   created in Shipping); borrows the profiler, the file writer and the registered sections, which outlive it. Owns a
///   1 MiB JSON buffer and the section table, allocated by Create. State is private members of this class.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Frames.
/// Errors: Create returns PROFILER_ERROR_NOT_IN_SHIPPING in Shipping (it begins
///   if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(PROFILER_ERROR_NOT_IN_SHIPPING); }), and otherwise
///   PROFILER_ERROR_INVALID_DESC (null profiler or writer) or PROFILER_ERROR_OUT_OF_MEMORY; AddSection
///   returns PROFILER_ERROR_INVALID_DESC for a duplicate or malformed name or a 33rd section; RequestFrames clamps to 1 … 60;
///   a write failure is the file writer's error, logged under devtools.console.
/// Reentrancy: NONE. A section's Write may not call a frame-dump member.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_DevFrameDump
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DevFrameDump>, Zenith_ProfilerError> Create(const Zenith_DevFrameDumpDesc& xDesc);
	~Zenith_DevFrameDump();
	// Registration; the token's destruction removes the section (it must happen before the section's implementer dies).
	Zenith_Result<Zenith_CallbackToken, Zenith_ProfilerError> AddSection(Zenith_DevFrameDumpSection& xSection);
	void RequestFrames(uint32 uFrames);            // the console's devtools.dump [<frames>]: dump the next uFrames frames
	uint32 GetPendingFrames() const;
	// Frame-loop step 3, after Zenith_Profiler::EndFrame: when a frame is pending, writes one document for the frame just
	// assembled and decrements the count. Defined in the development-only Internal/DevTools/Zenith_DevFrameDumpWrite.cpp of
	// ZenithProfiling; the Shipping-forbidden symbol of the dump.
	void WriteFrame(uint64 ulFrameIndex);
};
```

`Zenith_DevFrameDump` holds up to 32 sections and writes, for each requested frame at frame-loop step 3, `user:/devtools/frames/<frameIndex>.json` (through `Zenith_DevFileWriter::Write("frames/<frameIndex>.json", …)`):

```json
{
	"schema": "zenith2.devtools-dump",
	"schemaVersion": 1,
	"frameIndex": 1234,
	"compositionHash": "0x…",
	"buildIdentity": "0x…",
	"profiler": { "summary": { "frameUs": 16650, "cpuSimUs": 4100, "cpuRenderUs": 3200, "gpuUs": 9800, "flags": ["HITCH"] },
		"zones": [ { "lane": "Z2 Main", "name": "Sim.Tick", "startNs": 0, "endNs": 4100000, "depth": 0, "value": null } ],
		"counters": [ { "name": "jobs.steals", "kind": "CUMULATIVE", "frame": 12, "latest": 99012 } ] },
	"platform": { "events": [], "input": { "commands": [] }, "io": { "queueDepth": 3 } },
	"ecs": { },
	"assets": { },
	"truncated": false
}
```

Zone `startNs` values are relative to the frame's first zone. The platform section (events, input commands and I/O depths, which PLAT's design lists for the frame dump) is ZenithRuntime's `Zenith_PlatformDumpSection`; it lands with PLAT's input system in Stage 3, and before it the member is absent. A reader refuses `schemaVersion` above 1. The dump is a development command alone; it never runs in Shipping.

## Log file sink

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_LogFileSink.h
#pragma once
#include "ZenithCore/Zenith_BuildInfo.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_LogRouter.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithProfiling/Zenith_Profiler.h"

/// @contract value Zenith_LogFileSinkDesc
/// Ownership: Plain value copied by Create.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Create returns PROFILER_ERROR_INVALID_DESC for a null output or a chunk size outside 64 KiB … 4 MiB.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_LogFileSinkDesc
{
	Zenith_ProfilerOutput* const m_pxOutput = nullptr;                            // the runtime's user-mount output
	const Zenith_LogLevel m_eMinimumLevel = LOG_LEVEL_DEBUG;
	const uint32 m_uChunkBytes = 256u << 10;                                      // a chunk file is written when this fills, at Flush, and at shutdown
	const uint32 m_uMaxChunks = 64;                                               // per run; beyond it the oldest chunk file of this run is replaced
	const Zenith_StringView m_strRunStem{};                                       // "<utc yyyymmddThhmmssZ>-<pid>", from the composition root
	const Zenith_BuildConfiguration m_eConfiguration = BUILD_CONFIGURATION_DEBUG; // the composition root passes Zenith_GetBuildConfiguration()
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_LogFileSink
/// Ownership: Created at startup step 7 by ZenithRuntime and added to the log router with AddSink. Detach (MOD shutdown
///   step 6, before Zenith_AsyncIO is destroyed) removes it from the router and writes its last chunk; it is destroyed in
///   step 10b. Owns two chunk buffers.
/// Thread affinity: as Zenith_LogSink: Write on logging threads (memory only), Output and Flush on the router's drain
///   thread or the fatal path.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: A chunk write refused by the output drops the chunk and counts devtools.log_file.dropped_delta by its lines.
/// Reentrancy: NONE. It never logs from Output or Flush (its own failures are counters and one diagnostic line through
///   Zenith_WriteDiagnosticLine).
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_LogFileSink final : public Zenith_LogSink
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_LogFileSink>, Zenith_ProfilerError> Create(const Zenith_LogFileSinkDesc& xDesc);
	Zenith_LogLevel GetMinimumLevel() const override;
	void Write(const Zenith_LogRecord& xRecord) override;    // nothing: the sink works on the drain thread
	void Output(const Zenith_LogRecord& xRecord) override;   // encodes one JSON line into the current chunk
	void Flush() override;                                   // hands a non-empty chunk to the output
	// Main thread, MOD shutdown step 6: xRouter.RemoveSink(*this) (which waits until the drain thread is outside the sink),
	// then hands the current chunk to the output and clears the output pointer; records logged afterwards reach the
	// remaining sinks (console, ring, crash mirror) and never this one. Idempotent.
	void Detach(Zenith_LogRouter& xRouter);
	uint64 GetResidentBytes() const;          // the two chunk buffers
};
```

- `Output` encodes the record with `Zenith_JsonWriter` in `JSON_WRITE_STYLE_COMPACT` into the current chunk (§ "Log record schema"), followed by `\n`; a record that does not fit starts a new chunk (the full one is handed over first).
- A full chunk, or a `Flush`, hands the chunk to `Zenith_ProfilerOutput::WriteFile("logs/<stem>.<NNNN>.jsonl", bytes)` (the file lands at `user:/devtools/logs/`); the second buffer takes new lines while the first is written; when both are busy, lines are dropped and counted.
- Retention: at most `m_uMaxChunks` chunk files per run and the newest 8 runs; at startup step 7 the sink's creator lists `user:/devtools/logs/`, groups the files by run stem, and deletes every file of the runs older than the newest 7 (the current run is the eighth) with `Zenith_AsyncIO` remove calls whose completion is not awaited. `Test_LogFileSink_RetentionKeepsNewestEightRuns` (a scratch user mount holding the files of 10 earlier runs) asserts the 3 oldest runs' files are removed and the rest kept.
- Detach and shutdown: `Output` and `Flush` run on the drain thread until `Detach`; `Detach` runs on the main thread in MOD's shutdown step 6 (Contracts step 8 destroys `Zenith_AsyncIO` right after), so the last chunk is submitted while the I/O service lives and `Zenith_DevTools::DetachOutputs` waits for it (`Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown"); MOD logs `Runtime: shutdown complete` in its step 7, after the detach, so that line reaches the console and ring sinks but not the file. `Test_LogFileSink_NoWriteAfterDetach` asserts that after `Detach` a logged record produces no `WriteFile` on the test output and that the chunk written by `Detach` holds every record logged before it.
- The fatal path calls `Flush` like every sink (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure"); the final chunk is submitted but its completion is not awaited, so the crash report's log copy is the crash log mirror's (`Zenith2/Docs/Design/DEVT/Crash.md` → "Crash log mirror"), not this file.
- Shipping: `devtools.log.file` defaults to false; set to true in the user layer, the sink writes records of `WARNING` and above alone (`GetMinimumLevel` returns `max(m_eMinimumLevel, LOG_LEVEL_WARNING)` when the desc's `m_eConfiguration` is `BUILD_CONFIGURATION_SHIPPING`; the composition root passes `Zenith_GetBuildConfiguration()`, so the rule is data a non-Shipping test can drive). `Test_LogFileSink_ShippingWarningOnly` creates a sink with `m_eConfiguration = BUILD_CONFIGURATION_SHIPPING` and asserts an `INFO` record is not written and a `WARNING` record is.

## Log record schema

One line per record, schema `zenith2.log-record` version 1. The first line of every chunk file is a header record:

```json
{"schema":"zenith2.log-record","schemaVersion":1,"kind":"header","run":"20261003T101500Z-4242","chunk":0,"buildIdentity":"0x…","compositionHash":"0x…","platform":"windows"}
```

Then one object per record, members in this order:

| Member | Type | Source |
|---|---|---|
| `seq` | integer | `m_ulSequence` |
| `us` | integer | `m_ulMonotonicUs` |
| `utc` | string, ISO 8601 with microseconds | `m_ulUtcUs` |
| `lvl` | string: `trace`, `debug`, `info`, `warning`, `error`, `fatal` | `m_eLevel` |
| `cat` | string | the category name |
| `msg` | string | `m_strMessage` |
| `file`, `line` | string, integer | `m_xSite` (file relative to `Zenith2/`) |
| `tid` | integer | `m_ulThreadID` |
| `frame` | integer | `m_ulFrameIndex` |
| `fields` | object | `m_axFields`, each by its key and typed value |
| `trunc` | `true`, present only when `m_bTruncated` | |

A reader skips unknown members and refuses a header whose `schemaVersion` is above 1.

## Remote diagnostics server

`Zenith_DevToolsServer` (ZenithRuntime, `Internal/DevTools/Zenith_DevToolsServer.cpp`) is created at startup step 13 when `devtools.remote.enabled` is true, in non-Shipping builds, on PLAT's listener:

```cpp
Zenith_DevListenerDesc xDesc;
xDesc.m_szServiceName = "profiler";
xDesc.m_strBindAddress = "127.0.0.1";          // or devtools.bindAddress from the command line (PLAT's rule)
xDesc.m_uPort = devtools.remote.port;          // 27852 by default (the DAP service uses 27851)
xDesc.m_uMaxConnections = 2;
xDesc.m_uReceiveBufferBytes = 262144;
```

- It logs `devtools.remote: listening on 127.0.0.1:<port>` and PLAT's listener logs the session token locally; a client must send `ZENITH2-TOKEN <token>\n` first (PLAT enforces it) and then a `HELLO` (§ "Remote messages").
- It is polled from `Zenith_DevTools::EndFrame` at frame-loop step 3 (`Poll` with zero timeout, the zone `DevTools.Remote`); it never blocks and owns no thread.
- Per connection state: `AWAITING_HELLO` → `READY` → `CLOSED`. `AWAITING_HELLO` accepts only `HELLO`; anything else, a malformed frame, a payload above 1 MiB, or 10 s without `HELLO` closes the connection with one `ERROR` and a log line. `READY` accepts every request type; any malformed message closes it.
- Outgoing data is queued per connection in a 4 MiB ring; a subscription whose data would overflow it drops its oldest messages of that kind (log lines, frame summaries) and sends one `ERROR` "stream behind" message; capture segments are never dropped (the capture stops instead, as with a refused output).
- Failure to bind (`Create` returns an error) logs `devtools.remote: listener unavailable: <error>` once and the run continues without the server.

## Remote protocol

Every message is a frame:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | payload bytes (little-endian, at most 1,048,576) |
| 4 | 2 | message type |
| 6 | 2 | request ID (echoed in the reply; 0 for unsolicited stream messages) |
| 8 | n | payload |

Payloads are UTF-8 JSON objects (compact) unless the type says binary. The decoder reads the header, refuses a length above the limit before reading the payload, and parses the JSON with CORE's reader under its default limits; a fuzz target covers the frame decoder (`Zenith2/Docs/Design/DEVT/Tests.md` → "Console tests").

## Remote messages

| Type | Name | Direction | Payload |
|---|---|---|---|
| 1 | `HELLO` | client → server | `{"client":"ZenithDevTool","protocol":1}`; the reply (`HELLO`) is `{"protocol":1,"buildIdentity":…,"compositionHash":…,"platform":…,"game":…,"frameIndex":…}`; a protocol above 1 is refused with `ERROR` |
| 2 | `COMMAND` | client → server | `{"line":"<console line>"}` |
| 3 | `COMMAND_REPLY` | server → client | `{"ok":true\|false,"output":["…"],"error":"…"}`, after the line runs at the next `ExecuteQueued` |
| 4 | `LOG_SUBSCRIBE` | client → server | `{"minLevel":"info","categories":["*"]}`; `{"stop":true}` ends it |
| 5 | `LOG_LINE` | server → client | one `zenith2.log-record` object |
| 6 | `FRAME_SUMMARY_SUBSCRIBE` | client → server | `{"everyFrames":1}` or `{"stop":true}` |
| 7 | `FRAME_SUMMARY` | server → client | binary: the 64-byte `Zenith_ProfilerFrameSummary` followed by the counters named in the subscription's `counters` list as `{uint32 nameLength; bytes; int64 value}` |
| 8 | `CAPTURE_START` | client → server | `{"frames":300,"locks":false}`; the reply carries the capture ID or the error |
| 9 | `CAPTURE_SEGMENT` | server → client | binary: one `.zprof` segment, split into consecutive messages of at most 1 MiB with the same request ID, the first prefixed by `{uint32 sequence; uint32 totalBytes}` |
| 10 | `CAPTURE_DONE` | server → client | `{"captureId":"…","segments":n,"stoppedEarly":false,"reason":""}` |
| 11 | `FILE_LIST` | client → server | `{"dir":"devtools/frames"}`, relative to the user mount, restricted to `devtools/**` and `crash/**` |
| 12 | `FILE_LIST_REPLY` | server → client | `{"files":[{"name":"…","bytes":n,"utc":"…"}]}` |
| 13 | `FILE_GET` | client → server | `{"path":"crash/20261003T101500Z-4242.report.json"}`, same restriction |
| 14 | `FILE_DATA` | server → client | binary chunks of at most 1 MiB, the first prefixed by `{uint64 totalBytes}`; an empty payload ends the file |
| 15 | `ERROR` | server → client | `{"code":"…","message":"…"}` |
| 16 | `PING` | either | empty |
| 17 | `PONG` | either | empty |

- Capture segments are sent from the profiler's output path: while a remote capture runs, `Zenith_DevToolsOutput` both writes the segment to the user mount and queues it to the requesting connection.
- `FILE_GET` reads the file through `Zenith_AsyncIO` on the user mount in 1 MiB reads, one in flight per connection; paths are validated with the same segment rules as `Zenith_DevFileWriter` names, so no path outside `user:/devtools/` and `user:/crash/` is readable.
- Commands with `m_bRemoteAllowed == false` are refused with `ERROR` "not remotely allowed".

## Remote client

`ZenithDevTool remote` (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "ZenithDevTool"):

```
ZenithDevTool remote [--host 127.0.0.1] [--port 27852] --token <hex> <subcommand>
  console                       interactive console (line editing, history), replies printed
  exec "<line>"                 one line, exit code 0 when ok
  logs [--min-level info] [--category <glob>]   stream log lines until Ctrl+C
  capture --frames 300 --out <dir>              capture and save segments into <dir>
  ls <dir> | get <path> --out <file>            file list and fetch
  frames [--counters a,b,…]                     stream frame summaries as CSV
```

- Device workflow (the S24): start the app with `--set devtools.remote.enabled=true` (through `adb shell am start … --es zenithArgs "…"`, MOD's argument passing), read the token from `adb logcat -s Zenith2` (the listener's local log line), run `adb forward tcp:27852 tcp:27852`, then `ZenithDevTool remote --token <hex> console`. `ZenithDevTool remote --adb` does the forward and reads the token from logcat itself (it runs `adb` from the PATH, refusing when more than one device is attached unless `--serial` is given).
- Desktop workflow: the same without `adb`; the token is in the run's log file.
- The client verifies the `HELLO` reply's protocol and prints the build identity; it never writes outside the `--out` path.
- Sockets: the client is PLAT's `Zenith_DevClient` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket"; `ZenithDevTool` already depends on ZenithPlatform): `Zenith_DevClient::ConnectLoopback(uPort, xHeap)` connects to `127.0.0.1:<port>` only, the tool sends `ZENITH2-TOKEN <token>\n` as its first message as every peer must, then frames § "Remote protocol" messages over `Send` and reads them with `Receive(xOut, 5000)`; a `DEV_SERVICE_ERROR_*` is printed with `Zenith_ToString` and exits 1. The tool carries no socket code of its own. Processes: PLAT has no process API, so the tool keeps its own Windows process launcher, which a host tool may (it is `ZENITH_TOOLS_HOST`, `win` alone): `Internal/Windows/Zenith_DevToolProcess_Windows.cpp` (`CreateProcessW` of `adb.exe` found on `PATH`, stdout and stderr captured through an anonymous pipe, a 30 s timeout) behind `Internal/Zenith_DevToolProcess.h` (`Zenith_DevToolRunProcess(Zenith_StringView strExecutable, Zenith_Span<const Zenith_StringView> axArgs, Zenith_Array<char>& axStdout, uint32 uTimeoutMs)` returning the exit code). `--host` accepts `127.0.0.1` or `localhost` alone, matching the server's loopback bind.
- `Test_DevTool_RemoteClientAgainstServer` (in `ZenithDevTool_Tests`) runs a ZenithRuntime remote server in-process on an ephemeral port over a headless fixture runtime and drives `remote exec "echo hi"` through `Zenith_DevClient`, asserting the reply; `Test_DevTool_AdbForwardCommandLine` asserts the argument list built for `adb forward` and `adb logcat -s Zenith2 -d` without running `adb` (the process function is given a recording stand-in, a test seam of the tool's own internal header).

## Shipping absence of development services

Every development service lives in development-only translation units whose entry symbols are listed in `shipping_forbidden_symbols.json` ([Z2-N-ARCH-963]); each symbol below is declared in this family, and each is referenced only from code that the Shipping build removes (a `Create` that refuses under `if constexpr (Zenith_IsShippingBuild())`, or an object the composition root never constructs in Shipping), so function-level linking drops the translation unit.

| Service | Mechanism in Shipping | Forbidden symbol (row DV15 (b), `shipping_forbidden_symbols.json`), and where it is declared |
|---|---|---|
| console | `Zenith_DevConsole::Create` begins `if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(DEV_CONSOLE_ERROR_NOT_IN_SHIPPING); }` and `Execute` calls `ExecuteStatement` only under `if constexpr (!Zenith_IsShippingBuild())`; the composition root passes null to every consumer | `Zenith_DevConsole::ExecuteStatement` (private member, § "Zenith_DevConsole header"; `Internal/Console/Zenith_DevConsoleExecute.cpp`) |
| development file writer | `Zenith_DevTools` constructs none in Shipping; `Write` and `WriteUserPath` begin with the `DEV_FILE_ERROR_NOT_IN_SHIPPING` refusal | `Zenith_DevFileWriterUserMount::Write` (ZenithRuntime, § "Zenith_DevFileWriter header") |
| DEVT frame dump | not created; `Create` begins with the `PROFILER_ERROR_NOT_IN_SHIPPING` refusal | `Zenith_DevFrameDump::WriteFrame` (§ "DEVT frame dump"; `Internal/DevTools/Zenith_DevFrameDumpWrite.cpp` of ZenithProfiling) |
| remote server | not created; `Zenith_DevToolsServer::Create` begins `if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(DEV_SERVICE_ERROR_NOT_IN_SHIPPING); }` (PLAT's `Zenith_DevServiceError`, the error its listener returns there too) | `Zenith_DevToolsServer::HandleMessage` (ZenithRuntime `Internal/DevTools/Zenith_DevToolsServer.h`: a class with `Create(const Zenith_DevToolsServerDesc&)`, `Poll()`, `QueueCaptureSegment(…)` and the private `HandleMessage(uint32 uConnection, uint16 uType, uint16 uRequestID, Zenith_ByteSpan xPayload)`, the one function every received message passes through) |
| Tracy | every bridge call, and the one reference to the real bridge table, is under `if constexpr (!Zenith_IsShippingBuild())`, and Tracy is not linked in `Shipping` or `ShippingTest` | `___tracy_startup_profiler` (Tracy's C API) |
| run telemetry | `devtools.telemetry.enabled` is refused in Shipping (its mask admits the engine default alone, default false), so the runtime creates none; `Zenith_RunTelemetry::Create` also begins with the `PROFILER_ERROR_NOT_IN_SHIPPING` refusal | none: it is not an entry point a Shipping Player could reach, so it has no forbidden symbol |
| development panel providers that read files (graph dump reader) | not registered | `Zenith_GraphPanelProvider::ReadDumpFile` (ZenithRuntime `Internal/DevTools/Zenith_RuntimePanelProviders.h`: the private member that opens and parses a Flux graph dump for the `graph` panel; `Zenith2/Docs/Design/DEVT/Overlay.md` → "Graph view") |

The overlay, the debug-draw recorder, the profiler in counters-only mode, the counter registry, the log file sink (off by default) and the crash handler exist in Shipping (`Zenith2/Docs/Design/DEVT.md` → "API rules"). The `Libgraph` step's Shipping symbol check fails when any listed symbol survives into a Shipping executable ([Z2-N-ARCH-958], [Z2-N-ARCH-964]); that check is the evidence for symbol absence, because it alone reads a Player's Shipping link closure. Shipping *behaviour* is evidenced by `ShippingTest` cases ([Z2-N-ARCH-231]; the `win-shipping-test` preset builds every test target with every Shipping flag and define, `Zenith_IsShippingBuild()` true and Tracy unlinked): each development service's public entry refuses there, and `Zenith2/Docs/Design/DEVT/Tests.md` → "ShippingTest cases" names the cases and the rule every DEVT test that exercises a development service follows in that configuration.
