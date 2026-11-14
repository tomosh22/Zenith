# Architecture — Composition root and Player

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "Composition root and Player". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 841–879.

## Settled rules and ownership

Every game is one executable: the shared Player main statically linked with the engine and the modules the game selected (Z2-D-081, Z2-D-002, Z2-D-024). There is no mutable global state except the root heap and the profiler hooks installed in `main` and the other variables of Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]), and no runtime service lookup; every dependency is passed in a constructor (Z2-D-046, Zenith2/Docs/Contracts.md → "No mutable global state"). The composition root is `Zenith2/Player/` (the entry points) plus `ZenithRuntime` (the wiring); it is owned by the MOD area.

- [Z2-N-ARCH-841] `ZenithRuntime` constructs every engine subsystem in the order of the table "Startup sequence", passes each one its dependencies by reference or `Zenith_Owned`, and destroys them in the reverse order given by Zenith2/Docs/Contracts.md → "Threading and concurrency contract"; no subsystem constructs a peer it does not own.
- [Z2-N-ARCH-842] The composition root is the one place that knows which modules and backends a game uses; a subsystem learns about another subsystem only through a constructor argument or a registration made by the composition root.

## Player layout

| Path | Contents |
|---|---|
| `Player/CMakeLists.txt` | `zenith_add_library(ZenithPlayer KIND PLAYER DEPENDS ZenithRuntime ZenithCore ZenithProfiling ZenithPlatform)` |
| `Player/Zenith_GameComposition.h` | `#include "ZenithRuntime/Zenith_GameComposition.h"`; declares `Zenith_GetGameComposition()` (defined by each game's generated TU) |
| `Engine/ZenithRuntime/Zenith_GameComposition.h` (ZenithRuntime) | `Zenith_GameComposition`, `Zenith_ModuleDescriptor`, `Zenith_RHIBackendEntry`, `Zenith_CodecEntry`, `Zenith_GameSettings`, the entry-point pointer types; in ZenithRuntime because the runtime consumes them and no engine library includes a Player header (Architecture.md → "Library dependency graph") |
| `Engine/ZenithRuntime/Zenith_ExitCode.h` (ZenithRuntime) | `Zenith_ExitCode`, whose values are the table "Exit codes" |
| `Player/Zenith_PlayerMain.h` | `Zenith_PlayerMain(const Zenith_PlayerArgs&)`, the platform-independent body of `main` |
| `Player/Internal/Zenith_PlayerMain.cpp` | the `Zenith_OOMPressureFlag` declaration, root heap installation, log router creation, profiler hook installation, command-line parse, `Zenith_RuntimeMain` call (passing the router and the flag); afterwards the router's destruction and the uninstalls |
| `Player/Internal/Windows/Zenith_PlayerEntry_Windows.cpp` | `int main(int iArgc, char** ppszArgv)`: the arguments are re-read as UTF-8 from `GetCommandLineW`; no SDL3 header is included |
| `Player/Internal/Android/Zenith_PlayerEntry_Android.cpp` | `extern "C" __attribute__((visibility("default"))) int SDL_main(int iArgc, char** ppszArgv)`, looked up by name by the SDL activity in the game's shared object; the file includes no SDL3 header (SDL3 is confined to `ZenithPlatform/Internal/`) |
| `Player/Tests/` | Player tests: composition validation, argument parsing |
| `Player/GradleTemplate/` | the Android Gradle template (no compiled sources) |

- [Z2-N-ARCH-858] The Android entry point reaches the game's shared object because `zenith_add_game` links `ZenithPlayer` whole-archive on Android (`$<LINK_LIBRARY:WHOLE_ARCHIVE,ZenithPlayer>`) and `SDL_main` has C linkage and default visibility despite `-fvisibility=hidden`; `Test_Composition_AndroidExportsSdlMain` and `ApkCheck` verify the exported symbol (Architecture/BuildSystem.md → "Games in CMake").
- [Z2-N-ARCH-859] On Windows the CRT's reference to `main` pulls the entry object from `ZenithPlayer`, so no whole-archive link is used there.

## The generated composition table

`zenith_add_game` generates `${ZENITH_OUT}/Games/<Name>/<configure-preset>/Zenith_GameComposition.generated.cpp`, the game executable's sole source for that preset (presets resolve different RHI and physics sets, so each has its own file). It holds constant data and one-line thunks that bind module entry points to the game's selected backends; it holds no main function and no logic beyond those thunks.

```cpp
// Engine/ZenithRuntime/Zenith_GameComposition.h
using Zenith_PfnModuleRegisterTypes = Zenith_Result<void, Zenith_ModuleError> (*)(Zenith_TypeRegistry& xTypes);
using Zenith_PfnModuleStart = Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> (*)(
	Zenith_ModuleStartContext& xContext);
// Stage 7 (Zenith2/Docs/Design/SCOPE/XrPublicAPI.md → "Interfaces of lower libraries"): Zenith_DeviceInteropProvider,
// Zenith_DeviceInteropContext and Zenith_DeviceInteropError are declared in MOD's ZenithRuntime/Zenith_DeviceInteropProvider.h.
using Zenith_PfnCreateDeviceInterop = Zenith_Result<Zenith_Owned<Zenith_DeviceInteropProvider>, Zenith_DeviceInteropError> (*)(
	const Zenith_DeviceInteropContext& xContext);

// Contract: IMMUTABLE. Constant data in the generated TU.
struct Zenith_ModuleDescriptor
{
	const char* const m_szName = nullptr;                              // the MODULE_NAME
	const uint32 m_uInterfaceVersion = 0;                             // uZENITH_MODULE_INTERFACE_VERSION when the TU was compiled
	const Zenith_PfnModuleRegisterTypes m_pfnRegisterTypes = nullptr;
	const Zenith_PfnModuleStart m_pfnStart = nullptr;                  // the entry point, or its backend-binding thunk
	const char* const m_szBackendName = nullptr;                       // "Jolt"; "Steam,Offline" for a backend list; nullptr without a family
	const char* const m_szModuleDependencies = nullptr;                // the selected module dependencies, comma-joined ("Physics"); "" when none
	const Zenith_PfnCreateDeviceInterop m_pfnCreateDeviceInterop = nullptr;   // Stage 7: the XR module's device-interop factory; nullptr for
	                                                   // every other module (a composition-time hook, not a module entry point)
};

// Contract: IMMUTABLE. Constant data in the generated TU.
struct Zenith_RHIBackendEntry
{
	const char* const m_szName = nullptr;              // BACKEND_NAME: "Vulkan", "D3D12"
	const Zenith_PfnCreateDevice m_pfnCreate = nullptr;
};

// Contract: IMMUTABLE. Constant data in the generated TU.
struct Zenith_CodecEntry
{
	const char* const m_szName = nullptr;              // "zlib", "zstd"
	const Zenith_PfnCreateCompression m_pfnCreate = nullptr;
};

// Contract: IMMUTABLE. Derived from game.json by zenith_add_game; aggregate-initialised constant data.
struct Zenith_GameSettings
{
	const char* const m_szStartupScene = nullptr;               // a content:/ virtual path; the default of content.startupScene
	const bool m_bScriptRecoveryDeclared = false;               // game.json carries scriptRecovery
	const Zenith_ScriptRecoveryPolicy m_eScriptRecoveryPolicy = SCRIPT_RECOVERY_POLICY_MANUAL;
	const uint32 m_uScriptRecoveryDelayTicks = 0;
	const uint32 m_uScriptRecoveryMaxPerModule = 0;
};

// Contract: IMMUTABLE. Defined by the generated TU as aggregate-initialised constant data.
struct Zenith_GameComposition
{
	const char* const m_szGameName = nullptr;
	const char* const m_szGameVersion = nullptr;
	const Zenith_Span<const Zenith_ModuleDescriptor> m_axModules = {};          // canonical registration order
	const Zenith_PfnCreateWorld m_pfnCreateWorld = nullptr;                     // the selected ECS backend
	const char* const m_szECSBackendName = nullptr;                             // "Custom", "Flecs" or "EnTT"
	const Zenith_Span<const Zenith_RHIBackendEntry> m_axRHIBackends = {};       // Windows: Vulkan, D3D12; Android: Vulkan
	const Zenith_Span<const Zenith_CodecEntry> m_axCodecs = {};                 // zlib and zstd, always both
	const Zenith_GameSettings m_xSettings = {};
	const uint64 m_ulCompositionHash = 0;                                      // [Z2-N-ARCH-864], § "Composition hash"
	const uint64 m_ulBuildIdentity = 0;                                        // per preset, § "Build identity"
};
```

```cpp
// Player/Zenith_GameComposition.h
#include "ZenithRuntime/Zenith_GameComposition.h"

const Zenith_GameComposition& Zenith_GetGameComposition();
```

- [Z2-N-ARCH-864] `zenith_add_game` computes `m_ulCompositionHash` as the first 16 hexadecimal digits of CMake's `string(SHA256)`, read as a little-endian `uint64`, over the LF-joined lines: game name, game version, each module name in canonical order, the ECS backend name, the physics backend name for the platform (or `none`), the RHI backend names in table order, and the codec names in table order.
- [Z2-N-ARCH-873] `zenith_add_game` computes `m_ulBuildIdentity` per configure preset with the digest, reading and spelling of bullet 864 over the LF-joined lines: the composition hash's printed spelling (the first 16 lower-case hexadecimal digits of its digest), the configure preset name, the Online backend names in composition order (or `none` when `Online` is not selected), `uZENITH_MODULE_INTERFACE_VERSION` in decimal, the startup scene's virtual path, and the script-recovery settings as `<policy>,<delayTicks>,<maxRecoveriesPerModule>` (or `default` when `scriptRecovery` is absent); the composition hash keeps exactly the inputs of bullet 864.
- [Z2-N-ARCH-874] The runtime checks at the start of startup step 6 that every name in a descriptor's `m_szModuleDependencies` is a module of the composition and that the descriptors are in canonical order (bullet 844), and stops with exit code 3 otherwise.

The generated file for a game selecting Audio, Online and Physics (Jolt) on the Flecs backend (bullet 872 fixes where each factory symbol comes from):

```cpp
// Generated by zenith_add_game(OpenWorldSlice) from Games/OpenWorldSlice/game.json. Never edited.
#include "Player/Zenith_GameComposition.h"
#include "ZenithECS_Flecs/Zenith_ECS_Flecs.h"
#include "ZenithRHI_Vulkan/Zenith_RHI_Vulkan.h"
#include "ZenithRHI_D3D12/Zenith_RHI_D3D12.h"
#include "ZenithCompression_Zlib/Zenith_Compression_Zlib.h"
#include "ZenithCompression_Zstd/Zenith_Compression_Zstd.h"
#include "ZenithAudio/Zenith_Module_Audio.h"
#include "ZenithOnline/Zenith_Module_Online.h"
#include "ZenithOnline_Offline/Zenith_Online_Offline.h"
#include "ZenithPhysics/Zenith_Module_Physics.h"
#include "ZenithPhysics_Jolt/Zenith_Physics_Jolt.h"

namespace
{
	constexpr Zenith_OnlineBackendEntry axONLINE[] = { { "Offline", &Zenith_CreateOnlineServices_Offline } };
	Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> StartOnline(Zenith_ModuleStartContext& xContext)
	{
		return Zenith_Module_Online_Start(xContext, axONLINE);
	}

	Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> StartPhysics(Zenith_ModuleStartContext& xContext)
	{
		return Zenith_Module_Physics_Start(xContext, &Zenith_CreatePhysics_Jolt);
	}

	constexpr Zenith_ModuleDescriptor axMODULES[] =
	{
		{ "Audio",   uZENITH_MODULE_INTERFACE_VERSION, &Zenith_Module_Audio_RegisterTypes,   &Zenith_Module_Audio_Start, nullptr,   "" },
		{ "Online",  uZENITH_MODULE_INTERFACE_VERSION, &Zenith_Module_Online_RegisterTypes,  &StartOnline,               "Offline", "" },
		{ "Physics", uZENITH_MODULE_INTERFACE_VERSION, &Zenith_Module_Physics_RegisterTypes, &StartPhysics,              "Jolt",    "" },
	};
	constexpr Zenith_RHIBackendEntry axRHI[] = { { "Vulkan", &Zenith_CreateRHI_Vulkan }, { "D3D12", &Zenith_CreateRHI_D3D12 } };
	constexpr Zenith_CodecEntry axCODECS[] = { { "zlib", &Zenith_CreateCompression_Zlib }, { "zstd", &Zenith_CreateCompression_Zstd } };

	constexpr Zenith_GameComposition xCOMPOSITION =
	{
		"OpenWorldSlice", "0.1.0", axMODULES, &Zenith_CreateWorld_Flecs, "Flecs", axRHI, axCODECS,
		{ "content:/Scenes/Main.scene.json", false, SCRIPT_RECOVERY_POLICY_MANUAL, 0u, 0u },
		0x5d1f0a3c9e7b2248ull, 0x9c41e07b2d5a3f18ull,     // illustrative composition hash and build identity
	};
}

const Zenith_GameComposition& Zenith_GetGameComposition()
{
	return xCOMPOSITION;
}
```

- [Z2-N-ARCH-872] The generator writes each factory symbol from the backend declaration's `FACTORY` value (Architecture/BuildSystem.md → "The zenith_add_library helper") and never spells one itself, so a backend renaming its factory changes no generator code.
- [Z2-N-ARCH-843] The generated composition TU contains only `constexpr` data, includes of the selected libraries' root headers, the `Zenith_GetGameComposition` definition and one-line start thunks that pass a module its selected backend factory or its list of linked backends; it declares no mutable variable and no `main`.
- [Z2-N-ARCH-844] Modules appear in `m_axModules` in canonical registration order: a topological order of the modules' declared dependencies with ties broken by byte order of `MODULE_NAME`, computed by `zenith_add_game`, independent of the order `game.json` lists them.
- [Z2-N-ARCH-845] `ZenithRuntime` checks every descriptor's `m_uInterfaceVersion` against its own `uZENITH_MODULE_INTERFACE_VERSION` at startup and stops with exit code 3 and `module '<name>' interface version <v> does not match runtime <r>` on a mismatch (a stale build tree, since modules are statically linked).

### Runtime and entry types

```cpp
// Player/Zenith_PlayerMain.h
struct Zenith_PlayerArgs
{
	int m_iArgc = 0;
	char** m_ppszArgv = nullptr;        // UTF-8 (converted from GetCommandLineW on Windows)
};
int Zenith_PlayerMain(const Zenith_PlayerArgs& xArgs);   // steps 1–3, then Zenith_RuntimeMain; static_cast<int> of its Zenith_ExitCode

// Engine/ZenithRuntime/Zenith_ExitCode.h
enum Zenith_ExitCode : uint8
{
	EXIT_CODE_NORMAL = 0,
	EXIT_CODE_RUNTIME_ERROR = 1,
	EXIT_CODE_USAGE = 2,
	EXIT_CODE_STARTUP_VALIDATION = 3,
	EXIT_CODE_DEVICE = 4,
	EXIT_CODE_TEST_FAILED = 5,
	EXIT_CODE_DEVICE_LOST = 6,       // the graphics device was lost and could not be recovered; autosave requested
	EXIT_CODE_WATCHDOG = 124,
};

// Engine/ZenithRuntime/Zenith_CommandLine.h
enum Zenith_CommandLineErrorCode : uint8 { COMMAND_LINE_ERROR_UNKNOWN_OPTION = 1, COMMAND_LINE_ERROR_MISSING_ARGUMENT, COMMAND_LINE_ERROR_BAD_VALUE };

// The host-path options; one Zenith_CommandLineFileGrant (PLAT, ZenithPlatform/Zenith_HostFile.h) per option given.
enum Zenith_CommandLineFileOption : uint8
{
	COMMAND_LINE_FILE_CONFIG_DUMP = 1,
	COMMAND_LINE_FILE_RECORD,
	COMMAND_LINE_FILE_REPLAY,
	COMMAND_LINE_FILE_INPUT_SCRIPT,
	COMMAND_LINE_FILE_EVIDENCE_DIR,   // the writable directory grant (IsDirectory() true)
	COMMAND_LINE_FILE_SCRIPT_MIGRATION_FIXTURES,   // since Z2 Stage 3: the second directory grant, read-only
};

// elaboration: the budget-run cache state and measurement conditions of Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure".
enum Zenith_BudgetRunCacheState : uint8 { BUDGET_RUN_CACHE_NONE = 0, BUDGET_RUN_CACHE_COLD, BUDGET_RUN_CACHE_WARM };
enum Zenith_BudgetRunConditions : uint8 { BUDGET_RUN_CONDITIONS_NONE = 0, BUDGET_RUN_CONDITIONS_CONTROLLED, BUDGET_RUN_CONDITIONS_ORDINARY };

// The error record of [Z2-N-CONTRACT-480]: the offending argument by index; the caller prints ppszArgv[m_uArgIndex].
struct Zenith_CommandLineError
{
	Zenith_CommandLineErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uArgIndex = 0;
};

// Contract: IMMUTABLE after parsing; views into the argument strings, which outlive it (they are main's). Members
// non-const: Parse returns it through Zenith_Result, which default-constructs and move-assigns its value.
class Zenith_CommandLine
{
public:
	static Zenith_Result<Zenith_CommandLine, Zenith_CommandLineError> Parse(int iArgc, char** ppszArgv);
	Zenith_Span<const Zenith_StringView> GetSetOptions() const;   // each "<key>=<value>"
	Zenith_StringView GetAssetsRoot() const;                     // empty when absent
	bool IsHeadless() const;
	Zenith_StringView GetReplayPath() const;
	Zenith_StringView GetRecordPath() const;
	Zenith_StringView GetInputScriptPath() const;
	uint64 GetExitAfterTicks() const;                           // 0 when absent
	uint32 GetTimeoutSeconds() const;                           // 0 when absent
	Zenith_StringView GetConfigDumpPath() const;
	const Zenith_CommandLineFileGrant* GetFileGrant(Zenith_CommandLineFileOption eOption) const;   // nullptr when absent
	bool IsBudgetRun() const;                                     // --budget-run given
	// elaboration: the remaining budget-run switches; each is 0 or empty outside a budget run
	Zenith_StringView GetBudgetRunWorkload() const;               // the --budget-run argument, e.g. "W-Dense"
	uint32 GetWorkloadVersion() const;                           // --workload-version
	Zenith_BudgetRunCacheState GetCacheState() const;             // --cache-state
	Zenith_BudgetRunConditions GetConditions() const;             // --conditions
	uint32 GetRepetition() const;                                // --repetition
};

// Engine/ZenithRuntime/Zenith_Runtime.h
enum Zenith_RuntimeErrorCode : uint8
{
	RUNTIME_ERROR_STARTUP_STEP = 1,          // a startup step failed; m_eStep names it, the startup report the cause
	RUNTIME_ERROR_CONFIGURATION,             // a startup configuration failure of step 6 or 9 (exit code 2); m_eStep names the step
	RUNTIME_ERROR_TRANSITION_PEAK_REFUSED,   // the governor refused the world-transition peak
	RUNTIME_ERROR_TRANSITION_IN_PROGRESS,    // a transition from the same world is already loading
	RUNTIME_ERROR_UNKNOWN_WORLD,             // no live world has the given tag
	RUNTIME_ERROR_WORLD_CREATE,              // Zenith_RuntimeToolAccess refusals from here on (since Z2 Stage 3): CreateWorld failed
	RUNTIME_ERROR_WORLD_SCHEDULED,           // AddToolExtractor for a world already in a compiled schedule
	RUNTIME_ERROR_REGISTRATION,              // the world's system registry refused the tool extractor
	RUNTIME_ERROR_QUEUE_FULL,                // QueueSchemaJson over the schema queue's capacity
	RUNTIME_ERROR_FRAME_FAILED,              // a tool frame ended with an unrecoverable runtime error
};

enum Zenith_StartupStep : uint8                  // the steps of the table "Startup sequence", numbered as there
{
	STARTUP_STEP_ROOT_HEAP = 1, STARTUP_STEP_PROFILER_HOOKS, STARTUP_STEP_COMMAND_LINE, STARTUP_STEP_PLATFORM,
	STARTUP_STEP_FILE_SYSTEM, STARTUP_STEP_CONFIGURATION, STARTUP_STEP_LOGGING, STARTUP_STEP_MEMORY_GOVERNOR,
	STARTUP_STEP_JOB_SYSTEM, STARTUP_STEP_REGISTRATION, STARTUP_STEP_ASSETS, STARTUP_STEP_RHI_DEVICE,
	STARTUP_STEP_RENDERER, STARTUP_STEP_SCRIPT_RUNTIME, STARTUP_STEP_MODULE_START, STARTUP_STEP_INITIAL_WORLD,
	STARTUP_STEP_STARTUP_SCENE, STARTUP_STEP_SCHEDULE, STARTUP_STEP_FRAME_LOOP,
};
inline constexpr uint32 uZENITH_STARTUP_STEP_COUNT = 19;

// The error record of [Z2-N-CONTRACT-480]; everything textual is in Zenith_StartupReport.
struct Zenith_RuntimeError
{
	Zenith_RuntimeErrorCode m_eCode;
	Zenith_StartupStep m_eStep = static_cast<Zenith_StartupStep>(0);   // 0 unless m_eCode is RUNTIME_ERROR_STARTUP_STEP or RUNTIME_ERROR_CONFIGURATION
	uint16 m_uModuleIndex = 0xFFFFu;                                  // index into m_axModules, or 0xFFFF
};

// A fixed-size value on Zenith_RuntimeMain's stack: reporting a failure allocates nothing. Not an error type.
struct Zenith_StartupReport
{
	Zenith_StartupStep m_eLastCompletedStep = static_cast<Zenith_StartupStep>(0);   // 0: none
	Zenith_StartupStep m_eFailedStep = static_cast<Zenith_StartupStep>(0);          // 0: no failure
	Zenith_ExitCode m_eExitCode = EXIT_CODE_NORMAL;
	uint16 m_uFailedModuleIndex = 0xFFFFu;
	Zenith_ModuleErrorCode m_eModuleError = static_cast<Zenith_ModuleErrorCode>(0);
	uint16 m_uStepsUnwound = 0;
	uint64 m_aulStepMicroseconds[uZENITH_STARTUP_STEP_COUNT] = {};                 // [step − 1]
	char m_acFailedModuleName[32] = {};                                             // NUL-terminated
	char m_acCause[160] = {};                                                       // the failing step's one-line cause, truncated
};

// Contract: MAIN_THREAD for every member; one instance per process, owned by Zenith_RuntimeMain's frame.
// Ownership (Create): xLogRouter and xOOMPressureFlag are owned by the Player, outlive the runtime, and are referenced,
// never owned; step 7 configures the router the Player created at step 1b, and step 8 binds the flag the Player declared
// before the root heap (Zenith_Heap exposes no accessor for its OOM user data, so both are passed).
class Zenith_Runtime
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Runtime>, Zenith_RuntimeError> Create(const Zenith_CommandLine& xCommandLine,
		const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap, Zenith_LogRouter& xLogRouter,
		Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler, Zenith_StartupReport& xReportOut);   // steps 4–18
	~Zenith_Runtime();                                                          // the shutdown order
	Zenith_ExitCode RunFrameLoop();                               // step 19
	Zenith_Result<Zenith_OperationHandle, Zenith_RuntimeError> RequestWorldTransition(
		uint16 uFromWorldTag, const Zenith_WorldTransitionDesc& xDesc);
	void RequestShutdown();                                                    // effective when the current frame-loop iteration returns
	// ANY_THREAD: one relaxed atomic increment; a run with a recorded assertion returns EXIT_CODE_TEST_FAILED (5) at its
	// normal exit (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Test assertion failures and world shutdown requests").
	void RecordTestAssertionFailure();
	const Zenith_GameComposition& GetComposition() const;
	const Zenith_StartupReport& GetStartupReport() const;
};

// The Player's report arrives with steps 1–3 already timed; the runtime fills the rest.
Zenith_ExitCode Zenith_RuntimeMain(const Zenith_CommandLine& xCommandLine, const Zenith_GameComposition& xComposition,
	Zenith_Heap& xRootHeap, Zenith_LogRouter& xLogRouter, Zenith_OOMPressureFlag& xOOMPressureFlag,
	Zenith_Profiler& xProfiler, Zenith_StartupReport& xReportInOut);

// Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h: the failure-injection seam of Test_Runtime_StartupFailureUnwinds;
// compiled in every configuration, reachable only through this test-access type, whose header TestSeparationCheck
// condition 6 (Zenith2/Docs/Contracts.md → "Test separation") refuses outside tests. Zenith_Runtime::Create is this
// function with an all-null hook table, so the tested path is the shipped path.
using Zenith_PfnStartupStepHook = bool (*)(Zenith_StartupStep eStep, void* pUserData);     // false: inject a failure
using Zenith_PfnStartupUnwindHook = void (*)(Zenith_StartupStep eStep, void* pUserData);

struct Zenith_RuntimeTestHooks
{
	Zenith_PfnStartupStepHook m_pfnBeforeStep = nullptr;       // before each of steps 4–18
	Zenith_PfnStartupStepHook m_pfnAfterStep = nullptr;        // after each succeeds; false fails it after construction
	Zenith_PfnStartupUnwindHook m_pfnOnUnwind = nullptr;       // as each constructed step is destroyed
	void* m_pUserData = nullptr;
};

class Zenith_RuntimeTestAccess
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Runtime>, Zenith_RuntimeError> Create(const Zenith_CommandLine& xCommandLine,
		const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap, Zenith_LogRouter& xLogRouter,
		Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler, const Zenith_RuntimeTestHooks& xHooks,
		Zenith_StartupReport& xReportOut);
};

// Engine/ZenithRuntime/Zenith_RuntimeToolAccess.h (since Z2 Stage 3): the tool host's composition-root entry (the editor,
// and any later tool that runs the game's runtime), MOD's (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime tool access",
// which declares the remaining members, the hook and service types and the legal call positions). Compiled in every
// configuration like Zenith_RuntimeTestAccess and reachable only from tool sources: TestSeparationCheck condition 7
// (Zenith2/Docs/Contracts.md → "Test separation"). A tool host runs startup steps 4–15 and 18 with no initial world and no
// startup scene, then drives frames one at a time through RunOneFrame, the same frame-loop steps the Player runs. The
// services xServicesOut receives are handed to the tool's composition root once, not looked up at runtime (Z2-D-046).
class Zenith_RuntimeToolAccess
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Runtime>, Zenith_RuntimeError> Create(const Zenith_CommandLine& xCommandLine,
		const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap, Zenith_LogRouter& xLogRouter,
		Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler, const Zenith_ToolRuntimeDesc& xDesc,
		Zenith_ToolFrameHooks& xHooks, Zenith_ToolServices& xServicesOut, Zenith_StartupReport& xReportOut);
	static Zenith_Result<bool, Zenith_RuntimeError> RunOneFrame(Zenith_Runtime& xRuntime);
	// … the world, extractor and schema members of MOD's header
};
```

- [Z2-N-ARCH-876] The three `Create` forms (`Zenith_Runtime`, `Zenith_RuntimeTestAccess` and `Zenith_RuntimeToolAccess`) and `Zenith_RuntimeMain` take the Player's (or the tool host's) `Zenith_LogRouter&`, `Zenith_OOMPressureFlag&` and `Zenith_Profiler&` directly after `xRootHeap`, in that order, so no caller passes one without the others; the runtime references all three and destroys none (the profiler outlives the runtime, `Zenith2/Docs/Design/DEVT/Profiler.md` → "Creation and shutdown").

- [Z2-N-ARCH-871] A startup step that fails records the step, the exit code, the failing module's index and name (module steps) and a one-line cause in the `Zenith_StartupReport`, logs the cause at the point of failure, and returns a `Zenith_RuntimeError` that carries only the code, the step and the module index.

The factory function types the composition table carries are declared beside the factories they name:

```cpp
// Engine/ZenithECS/Zenith_World.h
using Zenith_PfnCreateWorld = Zenith_Result<Zenith_Owned<Zenith_World>, Zenith_ECSError> (*)(const Zenith_WorldDesc& xDesc);
// Engine/ZenithRHI/Zenith_RHIDevice.h (the device and its descriptor are Rendering types: Zenith2/Docs/Rendering.md → "GPU baseline and profiles")
using Zenith_PfnCreateDevice = Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> (*)(const Zenith_RHIDeviceDesc& xDesc);
// Engine/ZenithCompression/Zenith_Compression.h (the compression interface: Zenith2/Docs/Design/COMPRESS.md)
using Zenith_PfnCreateCompression = Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> (*)(const Zenith_CompressionDesc& xDesc);
// Engine/ZenithPhysics/Zenith_Physics.h (the physics interface: Zenith2/Docs/Design/PHYS.md)
using Zenith_PfnCreatePhysics = Zenith_Result<Zenith_Owned<Zenith_Physics>, Zenith_PhysicsError> (*)(const Zenith_PhysicsDesc& xDesc);
```

## Startup sequence

`main` (Player) and `Zenith_RuntimeMain` (ZenithRuntime) perform these steps in order. A failing step logs its error, destroys everything constructed so far in reverse order, and returns the exit code of § "Exit codes".

| Step | Owner | Action | Headless configuration |
|---|---|---|---|
| 1 | Player | call `Zenith_ApplyCrtMathMode()` as the first statement of `main` (Architecture.md → "Determinism guarantee and mechanisms"); declare the `Zenith_OOMPressureFlag` on the stack of `main`, then construct the root `Zenith_Heap` there with the flag as its OOM user data (`Zenith_HeapDesc::m_pOOMUserData`), and call `Zenith_InstallRootHeap(&xHeap)` before any other engine call; the statement directly after it is `Zenith_RunCrashHelperIfRequested(argc, argv, xIdentity, iExit)`, which on Windows runs a relaunched crash helper (`--zenith-crash-helper`) to its end and returns true, so `main` uninstalls the heap and returns `iExit` without starting the engine (bullet 860; Zenith2/Docs/Design/DEVT/Crash.md → "Windows capture") | same |
| 1b | Player | `Zenith_LogRouter::Create` with the build-time ring size `uLOG_RING_BYTES_DEFAULT` (1 MiB): allocations only, no thread, no configuration read; `FillHooks` writes its log-record hooks into the hook table | same |
| 2 | Player | `Zenith_Profiler::Create(Zenith_GetPlayerProfilerDesc(…))`, the one profiler (Tracy and the on-screen capture inside it in development builds, counters only in Shipping; a creation failure returns exit code 4 after one diagnostic line), whose `FillHooks` fills the hook table beside the router's; then `Zenith_InstallProfilerHooks(&xHooks)` (Zenith2/Docs/Design/DEVT/Profiler.md → "Creation and shutdown") | same |
| 3 | Player | parse the command line into `Zenith_CommandLine` (every option of the table "Command-line options") | same |
| 4 | Runtime | create `Zenith_Platform` (SDL init: events, video, gamepad, haptic, sensor, audio when not headless); then PACK's pending installs (`Zenith_RuntimePackage_RunPendingInstalls`, `Zenith2/Docs/Design/PACK/Algorithms.md` → "Pending installs at startup"): the instance guard is taken, the stamp lookup for step 5 is read, and the action never fails the step (the previous content stays in effect); its time is part of this step's `m_aulStepMicroseconds[3]`, and the step's unwind first releases the instance guard (`Zenith_RuntimePackage_ReleaseInstanceGuard`) | SDL events only; no window; the pending installs the same |
| 5 | Runtime | mount the virtual filesystem: package, patch, loose (development builds), user (Architecture.md → "Platform layer"); the `Zenith_VirtualFileSystemDesc` carries ZenithAssets' archive factory `&Zenith_CreateArchiveMountSource` and a `Zenith_ArchiveReaderDesc` (the factory's user data), and the package, verified patch and install-time asset-pack archives are mounted through it; the patch mount's `m_pxUpdateStamps` is the stamp lookup read at step 4 (`Zenith_RuntimePackage_GetUpdateStampLookup`, null when none) | same |
| 6 | Runtime | declarations first: validate the composition (bullet 874, the interface versions of bullet 845); create the configuration schema and `Zenith_TypeRegistry` in its declaration phase (`AttachConfigSchema`); register every engine library's configuration keys in library-layer order (ZenithAssets' through `Zenith_RegisterAssetsConfigKeys`), then the runtime's own keys (`content.startupScene`, `runtime.headless`), then the engine types (Core, Reflect, ECS, Scene, Assets — `Zenith_RegisterAssetTypes`, one `TYPE_KIND_ASSET` type per kind — Save, Script, Runtime, Flux); call each module's `m_pfnRegisterTypes` in canonical order, which registers its types and, through `GetConfigSchema`, its configuration keys; freeze the schema; in a budget run, check the `render.quality.*` keys against the `--set` options (bullet 877). Then build the configuration from its six layers with provenance (`Zenith_Config::Create`) | same (Flux types still registered: cooked data references them) |
| 7 | Runtime | create log sinks, `Zenith_LogFileSink`, the crash handler `Zenith_CrashHandler` (D-SEL-CRASH, open, Stage 2; its `Create` claims the crash handler anchor) and `Zenith_DevTools` (the development console, the debug-draw recorder, the profiler overlay, the development-file writer, the telemetry and the budget-run driver; `Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown"), and call the profiler's `SetOutput` and `ApplySettings`; a missing or malformed budget-run input (`rows.json`, `fingerprint.json` in the evidence directory) fails the step with exit code 2, an input error like every budget-run switch error; configure the router the Player created at step 1b and passed to `Zenith_RuntimeMain`: apply `log.level` and `log.categoryLevels`, add the sinks, resize its ring to `log.ringBytes`, then start its drain thread (`StartDrainThread`); every record since step 1b waits in the ring and reaches the sinks in order | same |
| 8 | Runtime | create the memory governor with the budget rows for this platform (Architecture.md → "Memory governor") and bind the Player's `Zenith_OOMPressureFlag` to it (`BindOOMPressureFlag`) | same |
| 9 | Runtime | call `Zenith_ApplyFloatEnvironment()` on the main thread first (Zenith2/Docs/Contracts.md → "Threading and concurrency contract"); create `Zenith_JobSystem` (worker count from `jobs.workerCount`) and the render-submit thread | render-submit thread not started |
| 10 | Runtime | create `Zenith_WorldRegistry`; bind the type registry to the process `Zenith_StringTable` (`BindStringTable`, which interns the string defaults declared at step 6); `InstallValueKindHooks(PROPERTY_TYPE_ASSET, Zenith_GetAssetHandleValueKindHooks())`; register the game's `schemas.zreg`; load `content:/Scripts/scripts.zlmf` and register the behaviour-state types of its state-schema records through the SCRIPT entry `Zenith_ScriptRegisterBehaviourStateTypes` (`Behaviour.<Name>.State`, `TYPE_KIND_BEHAVIOUR_STATE`, `Origin(TYPE_ORIGIN_SCHEMA)`); `Freeze` | same |
| 11 | Runtime | create both compression backends through their `m_axCodecs` factories into a `Zenith_CompressionCodecSet`, then `Zenith_AssetRegistry::Create` with that codec set; then `Zenith_TypeRegistry::ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, &Zenith_ResolveAssetReferenceDefault, <the asset registry>)` before any world exists; an unresolvable asset default is exit code 3; then, after the codec set, `Zenith_RuntimeSave_CreateSystem` (`Zenith2/Docs/Design/SAVE.md` → "Startup and shutdown"): the running build ID read once, the last-profile record (not read under `--replay`, whose initial profile is `local`), the save storage, `Zenith_SaveSystem::Create` with its synchronous journal recovery, and under `--record` or `--replay` `Zenith_RuntimeReplayCore_Create`; its `Zenith_Result` failure is exit code 4, a replay identity mismatch exit code 5; then the package service (`Zenith_RuntimePackage_Create`, `Zenith2/Docs/Design/PACK/Algorithms.md` → "Package service startup"): an absent or invalid `package.zpkm` in a Shipping build is exit code 3, and under `--replay` a recorded pack that cannot be mounted is exit code 5; its time is part of this step's `m_aulStepMicroseconds[10]`; then, when a module descriptor names `m_pfnCreateDeviceInterop` (Stage 7), create the device-interop provider through it (a configuration or script error is exit code 2, an allocation failure exit code 4; an absent XR runtime never fails the step; two non-null members are a composition defect, exit code 3) | same |
| 12 | Runtime | create the RHI device from `rhi.backend` (Windows: `vulkan` or `d3d12`; Android: `vulkan`) and the swapchain on the platform window; with a device-interop provider, the provider's `FillInteropRequest` before the device factory call and `OnDeviceCreated` after it | skipped |
| 13 | Runtime | create the Flux renderer and its render-world ring | skipped |
| 14 | Runtime | create the script runtime (VM factory, bytecode loader, debugger service in development builds); then `Zenith_RuntimeSave_RegisterBindings` (the `SAVE_REQUEST` consumer, the `Save` bindings, the `Save.Request` command kind; a failure is exit code 3) and, from Stage 3, `Zenith_RuntimeReplayScript_Attach` | same |
| 15 | Runtime | start each module in canonical order through `m_pfnStart`, storing each `Zenith_ModuleInstance`; then PACK's `Zenith_RuntimePackage_AfterModulesStarted` (the configuration entitlement query when no module provided one; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Entitlement provider"); then seal the `Zenith_CommandKindTable` the module starts filled and the script binding table (`Zenith_ScriptBindingTable::Seal`, whose `Zenith_ScriptBindingError` is exit code 3); then `Zenith_AssetRegistry::CloseRegistration` and `ServiceUntilComplete(GetPinnedLoads(), runtime.startupLoadTimeoutMs)` of every pinned fallback asset, a failure or timeout being exit code 3 | modules that need a device (audio output, UI rendering) start in their headless mode |
| 16 | Runtime | create the initial world: acquire a tag, create the ECS world through `m_pfnCreateWorld`, create `Zenith_WorldSimulation`, attach every module instance to the world in canonical order, then call `Zenith_RuntimeSave_AttachWorld(state, tag, predecessorTag)` after every module attached (a failure is exit code 3); a transition's world creation calls `Zenith_RuntimeSave_AttachWorld` at the same point, a failure there following the world host's attach-failure rule (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation") | same |
| 17 | Runtime | load the startup scene (`content.startupScene`) into the world, bounded by `runtime.startupLoadTimeoutMs`; its script systems and behaviours register with the world's system registry; then the resume decision and its synchronous restore (`Zenith2/Docs/Design/SAVE.md` → "Startup and shutdown"), except under `--replay`, where no resume decision is made; under `--record`, a successful resume closes the step-16 session with `SwapWorld`, then calls `BeginWorld` and `Zenith_RuntimeReplay_LogRestoredWorld` for the restored world, so the log of a resumed run replays | same |
| 18 | Runtime | build and compile the frame schedule, including the startup scene's script nodes and the engine stage nodes of Architecture/CPUFrameGraph.md → "Engine stage nodes" (`Assets.Service` among them); a compile error stops here | render subgraph empty; `Assets.Service` and `Render.Seal` present |
| 19 | Runtime | run the frame loop until shutdown is requested | same |

- [Z2-N-ARCH-846] The root heap and the profiler hooks are installed by `main` before any engine object exists other than the `Zenith_OOMPressureFlag` of step 1 (a plain local of `main`, declared before the heap whose OOM handler raises it) and the log router of step 1b (Zenith2/Docs/Contracts.md → "No mutable global state").
- [Z2-N-ARCH-875] The log router's drain thread starts in step 7 and nowhere earlier; a failure to start it is fatal (`ZENITH_FATAL(FATAL_CODE_LOGGING_UNAVAILABLE)`, after one line through `Zenith_WriteDiagnosticLine`), not an exit code, and the router is destroyed after `Zenith_RuntimeMain` returns and before the profiler hooks are uninstalled.
- [Z2-N-ARCH-865] The root heap and the profiler hooks are uninstalled after the last engine object is destroyed.
- [Z2-N-ARCH-866] The root heap, the profiler hooks and, from startup step 7, the crash handler anchor (`g_pxCrashHandlerAnchor`; Z2-D-046 as amended 2026-10-09), together with the two per-thread variables of [Z2-N-CONTRACT-144], are the only process-wide mutable state, every one of them an entry of Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152], and no step of the startup sequence creates any other.
- [Z2-N-ARCH-847] Registration happens in exactly the order of steps 6 and 10 — engine types, then modules in canonical order (step 6), then data-defined schemas, then the cooked behaviour-state schemas (step 10) — and the type registry is frozen before any world, asset load or script runs; the configuration schema is frozen before `Zenith_Config::Create`; `Zenith_BehaviourComponent` itself stays an engine type of step 6.
- [Z2-N-ARCH-848] A module registers its reflection types and its configuration keys in `m_pfnRegisterTypes` and nothing else (a module has no third entry point); its systems, world resources, lifecycle listeners, render extractors, memory consumers, command kinds and script bindings are registered through the `Zenith_ModuleStartContext` and `Zenith_WorldAttachContext` the runtime passes it, and no module registers anything through static initialisation.
- [Z2-N-ARCH-849] The headless configuration (`--headless` or `runtime.headless=true`) is the same executable as the windowed one.
- [Z2-N-ARCH-867] A headless run skips startup steps 12 and 13 and has no window, no audio output device and no render subgraph.
- [Z2-N-ARCH-868] Every simulation system runs unchanged in a headless run, so a headless run is simulation-equivalent to a windowed one (Architecture.md → "Multiplayer foundations").

### Command-line options

| Option | Argument | Meaning | Configuration layer it writes |
|---|---|---|---|
| `--set` | `<key>=<value>` | set a configuration key (repeatable) | command line |
| `--assets-root` | `<dir>` | mount `<dir>` as the loose mount (non-Shipping builds) | — (virtual filesystem) |
| `--headless` | — | the headless configuration; equivalent to `--set runtime.headless=true` | command line |
| `--replay` | `<file.zrpl>` | replay a log: phase 1 reads its input commands, tick boundaries its publications | — |
| `--record` | `<file.zrpl>` | record the replay log of this run | — |
| `--input-script` | `<file.json>` | drive phase 1 from a scripted input timeline (tests, the loopback client, the CI smoke) | — |
| `--exit-after-ticks` | `<n>` | request shutdown after n ticks of the primary world, summed across world swaps; exit code 0 unless a test assertion failed | — |
| `--timeout-seconds` | `<n>` | wall-clock watchdog: exit with code 124 after n seconds | — |
| `--config-dump` | `<path>` | write every configuration key, value and provenance to `<path>` after step 6 and continue | — |
| `--evidence-dir` | `<dir>` | the budget run's evidence directory (Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"): the writable directory grant `COMMAND_LINE_FILE_EVIDENCE_DIR`, created by the runtime when absent; non-Shipping builds | — |
| `--script-migration-fixtures` | `<dir>` | from Stage 3, a game's behaviour-state migration fixtures ([Z2-N-SCRIPT-400]): the read-only directory grant `COMMAND_LINE_FILE_SCRIPT_MIGRATION_FIXTURES`, through which the runtime reads every `<Behaviour>/v<n>.zbst` after step 17 (`Zenith2/Docs/Design/MOD.md` → "The gametest format"); an empty or repeated argument is `COMMAND_LINE_ERROR_BAD_VALUE`; non-Shipping builds | — |
| `--budget-run` | `<workload>` | the budget-run mode for the named workload (for example `W-Dense`): `IsBudgetRun()` is true and startup step 6 checks the `render.quality.*` keys (bullet 877); non-Shipping builds | — |
| `--workload-version` | `<n>` | the workload definition's version the run measures, 1 … 2³²−1; only with `--budget-run` | — |
| `--cache-state` | `cold` or `warm` | the cache state the run measures; only with `--budget-run` | — |
| `--conditions` | `controlled` or `ordinary` | the measurement conditions of the run; only with `--budget-run` | — |
| `--repetition` | `<n>` | the repetition index of the run, 1 … 1,000; only with `--budget-run` | — |

- [Z2-N-ARCH-860] `Zenith_CommandLine` accepts exactly the options of the table "Command-line options", with the spellings shown; every test, gate step and document uses these spellings, and any other option exits with code 2 naming it. The one exception is the Windows switch `--zenith-crash-helper`, which never reaches `Zenith_CommandLine`: `Zenith_RunCrashHelperIfRequested` dispatches it as the statement directly after `Zenith_InstallRootHeap(&xHeap)` in every `main` that creates a crash handler (the Player's `Zenith_PlayerMain`, the editor's, and ZenithPlatform's crash-test main, a target-supplied main forwarding to `Zenith_RunTestMain`), before step 1b; the helper allocates its four fixed buffers from that root heap and frees them before returning, and its trust rules are Architecture.md → "Security architecture" (bullet 965).
- [Z2-N-ARCH-877] In a budget run (`--budget-run <workload>`, `Zenith_CommandLine::IsBudgetRun()`), startup step 6, between freezing the configuration schema and `Zenith_Config::Create`, requires every key the frozen schema registers under the prefix `render.quality.` to be named by exactly one `--set` and no `--set` to name an unregistered `render.quality.` key; a key named by no `--set` or by two or more, or an unregistered one, fails step 6 with `RUNTIME_ERROR_CONFIGURATION` (exit code 2) and one `runtime.startup` line, `budget run: render.quality key '<key>' set <n> times; the workload sets each exactly once` or `budget run: render.quality key '<key>' is not registered`; outside a budget run the check does not run and repeated `--set` keeps its last-wins rule (Zenith2/Docs/Design/MOD.md → "Configuration-key registration"; `Test_Runtime_BudgetRun_QualityKeysExactlyOnce`, `Test_Runtime_NotBudgetRun_SkipsQualityCheck`).
<!-- elaboration -->
- [Z2-N-ARCH-878] `--workload-version`, `--cache-state`, `--conditions` and `--repetition` without `--budget-run`, or a repeated budget-run switch, is `COMMAND_LINE_ERROR_BAD_VALUE`, a budget run without `--evidence-dir` is `COMMAND_LINE_ERROR_MISSING_ARGUMENT`, and in a Shipping build `--budget-run`, its four companion switches, `--evidence-dir` and `--script-migration-fixtures` are `COMMAND_LINE_ERROR_UNKNOWN_OPTION` like `--assets-root`; each is exit code 2.

### Module interfaces

```cpp
// Engine/ZenithRuntime/Zenith_Module.h
inline constexpr uint32 uZENITH_MODULE_INTERFACE_VERSION = 1;

class Zenith_ModuleFrameListener;          // below
struct Zenith_EngineEpilogueStageDesc;     // below
class Zenith_SimulationClock;              // ZenithJobs (Architecture/CPUFrameGraph.md → "Simulation clock")
class Zenith_SaveParticipant;              // ZenithSave/Zenith_SaveParticipant.h
class Zenith_SceneWorldState;              // ZenithScene
class Zenith_ModulePublicationSink;        // ZenithRuntime/Zenith_ModulePublication.h
class Zenith_WorldControl;                 // GAME's Zenith_WorldControl.h (Zenith2/Docs/Design/GAME/WorldHost.md)
class Zenith_FaultInjector;                // ZenithCore ([Z2-N-CONTRACT-255])
class Zenith_DevConsole;                   // ZenithProfiling (Zenith2/Docs/Design/DEVT/Console.md)
class Zenith_DevFrameDump;                 // ZenithProfiling (Zenith2/Docs/Design/DEVT/Console.md → "DEVT frame dump")
class Zenith_DebugDraw;                    // ZenithProfiling (Zenith2/Docs/Design/DEVT/DebugDraw.md)
class Zenith_ProfilerOverlay;              // ZenithProfiling (Zenith2/Docs/Design/DEVT/Overlay.md)
class Zenith_BudgetRunRecord;              // ZenithProfiling (Zenith2/Docs/Design/DEVT/Evidence.md)
class Zenith_StringTable;                  // ZenithCore (Zenith2/Docs/Design/CORE/Text.md → "String interning")
class Zenith_TickBoundaryHandler;          // ZenithECS/Zenith_ECSObservers.h
class Zenith_CaptureTickGate;              // below
class Zenith_SaveSystem;                   // ZenithSave
class Zenith_EntitlementQuery;             // ZenithPackage (Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header")
class Zenith_TestAssertionSink;            // below
class Zenith_InputCommandBridge;           // ZenithPlatform (Zenith2/Docs/Design/PLAT/SimulationInput.md)
class Zenith_DeviceInteropProvider;        // ZenithRuntime/Zenith_DeviceInteropProvider.h (Stage 7)

// What a world is for; set once by the world's creator and constant for the world's lifetime.
enum Zenith_WorldPurpose : uint8
{
	WORLD_PURPOSE_PLAY = 1,      // the runtime's worlds and the world host's transition targets
	WORLD_PURPOSE_EDIT,          // the editor's edited world
	WORLD_PURPOSE_PREVIEW,       // an asset-preview host's world
};

// Contract: implemented by each module; created by its Start function; owned by ZenithRuntime.
// Thread affinity: MAIN_THREAD for every member.
class Zenith_ModuleInstance
{
public:
	virtual ~Zenith_ModuleInstance() = default;
	virtual Zenith_Result<void, Zenith_ModuleError> AttachWorld(Zenith_WorldAttachContext& xContext) = 0;
	virtual void DetachWorld(uint16 uWorldTag) = 0;
};

// Contract: MAIN_THREAD; valid for the duration of one Start call.
class Zenith_ModuleStartContext
{
public:
	const Zenith_TypeRegistry& GetTypes() const;
	Zenith_Heap& GetHeap();
	Zenith_JobSystem& GetJobs();
	Zenith_Platform& GetPlatform();
	Zenith_AssetRegistry& GetAssets();
	Zenith_Config& GetConfig();
	Zenith_MemoryGovernor& GetMemoryGovernor();
	Flux_Renderer* GetRenderer();               // null when headless
	Zenith_ScriptRuntime& GetScripts();
	// The process string table the composition root owns, valid for the process's lifetime: a module's kind loaders keep it
	// past Start and intern at every load the names scripts read as Zenith_StringID (Zenith2/Docs/Design/CINE/Formats.md).
	Zenith_StringTable& GetStringTable();
	bool IsHeadless() const;
	// Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context members for ONLINE and PACK": true during Start exactly under
	// --record; and a request that startup completes, no frame runs and the run exits 0 through the ordinary shutdown order
	// (not a registration, not undone; a startup failure keeps its exit code).
	bool IsRecordingReplay() const;
	void RequestExitAfterStart();
	// ONLINE's members (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context members for ONLINE and PACK"): the process save
	// system; true during Start exactly under --replay; and the process entitlement query (Stage 6), at most one per process,
	// journaled and undone with its module.
	Zenith_SaveSystem& GetSaveSystem();
	bool IsReplaying() const;
	void ProvideEntitlementQuery(Zenith_EntitlementQuery& xQuery);
	// A module-visible wrapper of Zenith_Runtime::RecordTestAssertionFailure (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Test
	// assertion failures and world shutdown requests"), null in Shipping; safe to call from a module instance's destructor.
	Zenith_TestAssertionSink* GetTestAssertionSink();
	// Stage 7: the device-interop provider created from the calling module's descriptor at startup step 11, null otherwise.
	Zenith_DeviceInteropProvider* GetDeviceInteropProvider();
	// DEVT services (Zenith2/Docs/Design/DEVT.md → "Upward interfaces DEVT declares"): the development console, null in
	// Shipping; the debug-draw recorder, present in every build (disabled in headless runs); the profiler overlay, null in
	// Shipping and headless builds; the budget-run record, null outside a budget run.
	Zenith_DevConsole* GetDevConsole();
	// DEVT's frame dump (Zenith2/Docs/Design/DEVT/Console.md → "DEVT frame dump"), null in Shipping and in tests that omit it;
	// a module registers its own section with Zenith_DevFrameDump::AddSection during Start and destroys the returned token
	// before its instance is destroyed; the token-owned registration is undone with the module instance ([Z2-N-ARCH-879]).
	Zenith_DevFrameDump* GetDevFrameDump();
	Zenith_DebugDraw& GetDebugDraw();
	Zenith_ProfilerOverlay* GetProfilerOverlay();
	Zenith_BudgetRunRecord* GetBudgetRunRecord();
	// The development capture tick gate, null in Shipping and when sim.ticksPerFrame ≠ 1 (Zenith2/Docs/Design/CINE/Video.md →
	// "Capture algorithm"); valid for the process's lifetime.
	Zenith_CaptureTickGate* GetCaptureTickGate();
	// A driver-called budget-row emitter keyed by the emitter member of RowSources.json, called at frame-loop step 3 on the
	// main thread in a budget run (Zenith2/Docs/Design/DEVT/Evidence.md → "Area row emitters"); journaled and undone.
	void RegisterBudgetEmitter(const char* szKey, void (*pfnEmit)(Zenith_BudgetRunRecord&));
	// The runtime's fault injector (Zenith2/Docs/Contracts.md [Z2-N-CONTRACT-255]); null outside a test-access runtime.
	Zenith_FaultInjector* GetFaultInjector();
	void RegisterMemoryConsumer(Zenith_MemoryConsumer& xConsumer);
	void RegisterCommandKind(const Zenith_CommandKindDesc& xDesc);          // into the process Zenith_CommandKindTable
	void RegisterScriptBindings(const Zenith_ScriptBindingTable& xTable);   // ZenithReflect/Zenith_ScriptBinding.h
	// Forward to the script binding table's SetSideEffectConsumer and DeclareAsyncSource (Zenith2/Docs/Scripting.md →
	// "Modules, bindings and generated types"): a module that consumes a SCRIPT-held side-effect kind or owns an
	// asynchronous source kind declares it here.
	void SetScriptSideEffectConsumer(Zenith_SideEffectKind eKind, Zenith_ScriptSideEffectConsumer& xConsumer);
	void DeclareScriptAsyncSource(uint8 uSourceKind, uint32 uMaxResultBytes);   // sizes each world's result arena
	// The in-flight capacity every script world gives the kind at the merge (Z2-B-SCRIPT-026's value for kinds 1 to 4, 0
	// otherwise); a query, not journaled.
	uint32 GetScriptAsyncCapacity(uint8 uSourceKind) const;
	// The process account consumer: sees every PLATFORM_EVENT_ACCOUNT_CHANGED before any world (Z2-D-074); at most one per
	// process, a second is a rejected registration; journaled and undone with the module. Zenith_PfnAccountConsumer is PLAT's.
	void RegisterAccountConsumer(Zenith_PfnAccountConsumer pfnConsumer, void* pUserData);
	// The module's frame listener: the runtime's Zenith_AppLifecycleListener forwards OnStopAudio and OnResumeAudio to every
	// registered listener inside Platform.Pump, and OnPlatformEvents after prologue step 1, in canonical module order.
	// Journaled and undone with the module.
	void RegisterFrameListener(Zenith_ModuleFrameListener& xListener);
	// The module's engine-wide EPILOGUE stage (FRAME_NODE_KIND_STAGE, group 0, stage 13), at most one per module, compiled
	// into every frame schedule from the next compile on, headless included and with no world attached, and ordered after
	// every world's epilogue stage named xDesc.m_szAfterWorldStage (Architecture/CPUFrameGraph.md bullet 442). The returned
	// builder accepts a write of one world-independent resource alone. Journaled and undone with the module.
	Zenith_SystemBuilder& AddEngineEpilogueStage(const Zenith_EngineEpilogueStageDesc& xDesc);
};

// Contract: implemented by a module; registered with RegisterFrameListener; outlives the module instance's registrations.
// Thread affinity: MAIN_THREAD for every member; each returns promptly (AUDIO's only posts to its control thread).
class Zenith_ModuleFrameListener
{
public:
	virtual ~Zenith_ModuleFrameListener() = default;
	virtual void OnStopAudio() {}       // inside Platform.Pump, before OnEnterBackground
	virtual void OnResumeAudio() {}     // inside Platform.Pump, after OnRecreateSurface
	// after Platform.Pump in prologue step 1: the frame's platform events (window focus, device and locale events)
	virtual void OnPlatformEvents(Zenith_Span<const Zenith_PlatformEvent> axEvents) {}
	// Stage 7: in prologue step 1, immediately before Platform.Pump (the XR module pushes its controller states here)
	virtual void OnBeforePlatformPump() {}
};

// Contract: owned by ZenithRuntime, non-Shipping builds only; ANY_THREAD. RecordFailure logs the reason and forwards to
// Zenith_Runtime::RecordTestAssertionFailure(); the exit code is read after shutdown step 6.
class Zenith_TestAssertionSink
{
public:
	void RecordFailure(Zenith_StringView strReason);
};

// Contract: owned by ZenithRuntime, development builds only; MAIN_THREAD. HoldNextTick makes the frame loop's next clock
// advance AdvanceExternal(0) for that one frame, so a capture whose readback or file write is refused renders and retries
// the same tick without pausing the world (pausing would fade time-locked voices).
class Zenith_CaptureTickGate
{
public:
	void HoldNextTick();
};

struct Zenith_EngineEpilogueStageDesc
{
	const char* m_szName = nullptr;            // "<Module>.<Stage>", unique in the process, e.g. "Audio.Mix"
	const char* m_szAfterWorldStage = nullptr; // a per-world epilogue stage name, e.g. "Audio.Update": after every Audio.Update@W<tag>
	Zenith_PfnSystemRun m_pfnRun = nullptr;
	void* m_pUserData = nullptr;               // owned by the module; outlives the registration
	uint32 m_uCostHint = 1;
};

// Contract: MAIN_THREAD; valid for the duration of one AttachWorld call.
class Zenith_WorldAttachContext
{
public:
	uint16 GetWorldTag() const;
	Zenith_SystemRegistry& GetSystems();
	// The world's simulation clock (tick duration, interpolation alpha; Architecture/CPUFrameGraph.md → "Simulation clock"),
	// valid until DetachWorld for this world.
	const Zenith_SimulationClock& GetSimulationClock() const;
	// Set by the world's creator: the runtime and the world host PLAY, the editor EDIT, an asset-preview host PREVIEW; a module
	// gates preview-only and edit-only behaviour on it without a module edge.
	Zenith_WorldPurpose GetWorldPurpose() const;
	Zenith_SceneWorldState* GetSceneState();   // the world's scene state (ZenithScene); never null in a runtime world
	// The world's single module publication sink (Engine/ZenithRuntime/Zenith_ModulePublication.h; Zenith2/Docs/Design/MOD/
	// RuntimeSeams.md → "Zenith_ModulePublication header"); a module may park through it until every ID it holds is
	// completed, even after its DetachWorld returned.
	Zenith_ModulePublicationSink& GetPublicationSink();
	uint32 FindCommandKind(const char* szName) const;   // 0 when absent; the sealed Zenith_CommandKindTable::FindByName
	Zenith_WorldControl& GetWorldControl();               // GAME's Zenith_WorldControl.h
	Zenith_FaultInjector* GetFaultInjector();             // null outside a test-access runtime
	// Forwards to the world's Zenith_SaveSystem::RegisterParticipant(GetWorldTag(), xParticipant) (ZenithSave,
	// Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"); journaled and undone with the module.
	void RegisterSaveParticipant(Zenith_SaveParticipant& xParticipant);
	// Before the world's first tick: forwards to Zenith_WorldSimulation::AddTickBoundaryHandler (ZenithECS); journaled and
	// undone with the module (CINE's tick-boundary restore handler is its user).
	void AddTickBoundaryHandler(Zenith_TickBoundaryHandler& xHandler);
	// During AttachWorld, at most one per world: the world's input-command bridge (PLAT's interface); journaled and undone with
	// the module.
	void SetInputCommandBridge(Zenith_InputCommandBridge& xBridge);
	void AddWorldResource(Zenith_TypeID xResourceType, Zenith_Phase eOwningPhase, void* pResource);
	void AddLifecycleListener(Zenith_LifecycleListener& xListener);
	Zenith_SystemBuilder& AddExtractor(const Zenith_ExtractorDesc& xDesc);   // Architecture/CPUFrameGraph.md
	// An epilogue stage node of this world (FRAME_NODE_KIND_STAGE, subgraph EPILOGUE, group = the world tag).
	// The returned builder accepts reads and a sole-consumer WriteResource (Architecture/ECSModel.md bullet 727); AddExtractor is its
	// phase-12 specialisation that also receives a Flux_RenderWorldWriter partition.
	Zenith_SystemBuilder& AddEpilogueStage(const Zenith_EpilogueStageDesc& xDesc);
};

// Declared in Engine/ZenithECS/Zenith_System.h (Zenith_SystemRegistry::AddEpilogueStage takes it; ZenithECS, layer 3,
// cannot include a ZenithRuntime header); Zenith_Module.h includes it.
struct Zenith_EpilogueStageDesc
{
	const char* m_szName = nullptr;        // "<Module>.<Stage>", unique per world, e.g. "Audio.Update"
	Zenith_PfnSystemRun m_pfnRun = nullptr;
	void* m_pUserData = nullptr;           // owned by the registrant; outlives the world
	uint32 m_uCostHint = 1;
	bool m_bRenderExtractor = false;       // set by AddExtractor alone; EmitInto then orders the node Before("Render.Seal@W0")
};

enum Zenith_ModuleErrorCode : uint8
{
	MODULE_ERROR_REGISTRATION = 1,    // a type, system or command registration was rejected (logged by the context)
	MODULE_ERROR_BACKEND_CREATE,      // the selected backend's factory failed
	MODULE_ERROR_DEVICE,              // a required device (audio output, input) could not be opened
	MODULE_ERROR_OUT_OF_MEMORY,
};

// The error record of [Z2-N-CONTRACT-480]: a module logs the detail first, then returns Zenith_ModuleError{ code }.
struct Zenith_ModuleError
{
	Zenith_ModuleErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uModuleIndex = 0xFFFFu;     // filled by the runtime: index into m_axModules
};

// Zenith_PfnCommandPlayback and Zenith_CommandKindDesc are declared in Engine/ZenithECS/Zenith_CommandKinds.h, because the
// sync-point playback that calls them is ZenithECS code; Zenith_Module.h includes that header. The composition root owns
// the one process Zenith_CommandKindTable (same header), filled by module starts at step 15 and sealed before step 16.
// The kind table and its m_pUserData are one per process, so a module whose playback changes per-world module state keys
// that state by xWorld.GetWorldTag() (Architecture/ECSModel.md, Zenith_WorldPlaybackAccess).
using Zenith_PfnCommandPlayback = void (*)(Zenith_WorldPlaybackAccess& xWorld, Zenith_ByteSpan xPayload,
	const Zenith_MergeKey& xKey, void* pUserData);

struct Zenith_CommandKindDesc
{
	const char* m_szName = nullptr;            // "<Module>.<Command>", for example "Physics.AddImpulse"
	Zenith_TypeID m_xPayloadType;              // a registered TYPE_KIND_STRUCT describing the payload
	uint32 m_uMaxPayloadBytes = 0;
	uint32 m_uAllowedPhaseMask = 0;           // bit p set = recordable in phase p (1–10)
	Zenith_PfnCommandPlayback m_pfnPlayback = nullptr;
	void* m_pUserData = nullptr;
	uint32 m_uFlags = 0;                      // Zenith_CommandKindFlags
};

enum Zenith_CommandKindFlags : uint32     // Zenith_CommandKinds.h
{
	COMMAND_KIND_FLAG_PLAY_LAST = 1u << 0,     // the kind's records play after every other step-7 record, in merge order among themselves
};

// Zenith_ScriptBindingTable, Zenith_SideEffectKind and Zenith_ScriptSideEffectConsumer, the Luau-free binding ABI, are
// declared by ZenithReflect in Engine/ZenithReflect/Zenith_ScriptBinding.h (Zenith2/Docs/Scripting.md → "Modules, bindings
// and generated types"); Zenith_PfnAccountConsumer and Zenith_PlatformAccountPayload by ZenithPlatform/Zenith_Platform.h.
```

A module's root header, for a module with a backend family (`Engine/ZenithPhysics/Zenith_Module_Physics.h`):

```cpp
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Physics_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Physics_Start(
	Zenith_ModuleStartContext& xContext, Zenith_PfnCreatePhysics pfnCreateBackend);
```

A module without a backend family omits the second parameter of `Start`; Online, whose backend is chosen by configuration among every linked backend, takes the list instead (Architecture.md → "Engine modules and game projects").

- [Z2-N-ARCH-850] Module-to-module collaboration goes through world resources and systems' declared access (for example, AI reads `Zenith_PhysicsWorldResource` through its system declaration), never through one module querying another for a service object.
- [Z2-N-ARCH-861] Script bindings reach Luau only as `Zenith_ScriptBindingTable`s declared through the Luau-free ABI of `ZenithReflect/Zenith_ScriptBinding.h`: a library or module that owns bindings depends on `ZenithReflect` for them and never on `ZenithScript`, and bindings for libraries below `ZenithReflect` (Core, Graph, Jobs, Profiling, Platform, Compression) are written in `ZenithRuntime/Internal/ScriptBindings/` and registered by the composition root, which seals the table after step 15 and maps a `Seal` error to exit code 3.
- [Z2-N-ARCH-879] `RegisterAccountConsumer`, `RegisterFrameListener` and `AddEngineEpilogueStage` are `MAIN_THREAD` and valid during `Start` alone; the process has at most one account consumer and each module at most one engine-wide stage, a second registration of either is rejected (`MODULE_ERROR_REGISTRATION` for the registering module), and every one of them is journaled and undone with its module like every other registration (bullet 851); the runtime forwards a frame listener's calls in canonical module order (`OnStopAudio` and `OnResumeAudio` from its `Zenith_AppLifecycleListener` inside `Platform.Pump`, `OnPlatformEvents` after prologue step 1); `Zenith_WorldAttachContext::GetSceneState` returns the world's scene state, never null in a runtime world; `Zenith_WorldAttachContext::GetWorldPurpose` returns the value its world's creator set (`WORLD_PURPOSE_PLAY` for the runtime's and the world host's worlds, `WORLD_PURPOSE_EDIT` for the editor's, `WORLD_PURPOSE_PREVIEW` for an asset-preview host's), and `RegisterSaveParticipant` is `MAIN_THREAD`, valid during `AttachWorld` alone, rejected like any other attach registration when ZenithSave refuses it, and journaled and undone with its module; `GetDevConsole`, `GetDevFrameDump`, `GetDebugDraw`, `GetProfilerOverlay`, `GetBudgetRunRecord` and `RegisterBudgetEmitter` are `MAIN_THREAD` and valid during `Start` alone, a budget emitter registration is journaled and undone with its module, and an overlay-panel or frame-dump-section registration made through these accessors is owned by a token the module instance holds and is undone by destroying the module instance at its undo position, which is how a token-owned registration satisfies the journaled rule; `IsRecordingReplay`, `RequestExitAfterStart`, `GetSaveSystem`, `IsReplaying`, `GetScriptAsyncCapacity` and `GetDeviceInteropProvider` are `MAIN_THREAD` and valid during `Start` alone, `ProvideEntitlementQuery` is a registration valid during `Start` alone, at most one per process, journaled and undone with its module, and `GetTestAssertionSink` returns a sink whose `RecordFailure` is `ANY_THREAD` and may be called from a module instance's destructor; `GetStringTable` and `GetCaptureTickGate` are `MAIN_THREAD` and callable during `Start`, and the table and the gate they return stay valid for the process's lifetime (`Zenith_CaptureTickGate::HoldNextTick` is `MAIN_THREAD`); `AddTickBoundaryHandler` is `MAIN_THREAD`, valid during `AttachWorld` alone, before the world's first tick, and journaled and undone with its module; `SetInputCommandBridge` is `MAIN_THREAD`, valid during `AttachWorld` alone, at most one per world, and journaled and undone; `GetPublicationSink`, `FindCommandKind` and `GetWorldControl` are `MAIN_THREAD` during `AttachWorld`, and each context's `GetFaultInjector` is `MAIN_THREAD` during its call; adding these members, `Zenith_ModuleFrameListener` (with `OnBeforePlatformPump`, which has an empty default), `Zenith_TestAssertionSink`, `Zenith_WorldPurpose`, `Zenith_WorldAttachContext::GetSimulationClock` and the descriptor's optional `m_pfnCreateDeviceInterop` changes no entry point or virtual of `Zenith_ModuleInstance`, so `uZENITH_MODULE_INTERFACE_VERSION` stays 1.
- [Z2-N-ARCH-851] Every registration a module makes through the start and attach contexts is undone by the runtime in reverse order when the module instance is destroyed or detached; a module never unregisters another module's registration.

### Exit codes

| Code | Meaning |
|---|---|
| 0 | normal shutdown |
| 1 | an unrecoverable runtime error after startup (logged) |
| 2 | command-line or configuration error |
| 3 | startup validation failure: schedule compile, schema, module interface version, cooked-content version |
| 4 | a required device or platform service could not be created (window, RHI device, audio when required) |
| 5 | an automated test or replay assertion failed (`--replay`, integration smoke): a run during which `Zenith_Runtime::RecordTestAssertionFailure` was called returns 5 at its normal exit |
| 6 | the graphics device was lost and could not be recovered, or a frame-failure exit was requested; autosave requested (`EXIT_CODE_DEVICE_LOST`, the clean exit of Zenith2/Docs/Rendering.md → "Recovery"; `FATAL_CODE_DEVICE_LOST` is only the fallback when that clean exit has not finished within 10 s) |
| 124 | the wall-clock watchdog fired (`--timeout-seconds`, used by the CI smoke) |

- [Z2-N-ARCH-852] The Player's exit code is exactly one of the table "Exit codes", and the gate steps and CI branch on it.

## The frame loop

`Zenith_Runtime::RunFrameLoop` repeats until shutdown is requested:

1. Run the frame prologue, the eight main-thread steps of Architecture/CPUFrameGraph.md → "The frame prologue": pump (and PACK's step 1b), clock advance (yielding T; in a development build a tick held through `Zenith_CaptureTickGate::HoldNextTick` makes this frame's advance `AdvanceExternal(0)`), user configuration, governor and renderer housekeeping (`Flux_Renderer::RunPrologue()`), thermal, the hot-reload window, world creation, activation and swaps (step 7 first calls `Zenith_RuntimeSave_PumpMainThread`, `Zenith2/Docs/Design/SAVE.md` → "Tick walkthrough of SAVE work"), recompile.
2. `Zenith_FrameExecutor::RunFrame({ frameIndex, T, bRunRender = !headless && frameIndex > 0 && renderer.GetDeviceLossState() == FLUX_DEVICE_LOSS_STATE_NORMAL })`, the device-loss state read after step 4's `RunPrologue`, so the render subgraph is empty from `LOST` through `RESUMING` and while `EXITING`.
3. `Zenith_DevTools::EndFrame`: the profiler frame, the overlay build, the development console's queue, the remote-tool poll, the telemetry and the budget-run sampling (`Zenith2/Docs/Design/DEVT.md` → "Frame placement"); under `--record`, call `Zenith_ReplayLogWriter::CommitFrame()`.
3b. In a headless run in real-time mode (`sim.ticksPerFrame` 0), while the simulation clock's next tick is not due, the main thread paces through `Zenith_SleepUs`, at most 1,000 µs per iteration, and not at all when the tick is due or `sim.ticksPerFrame` is not 0.
4. On a shutdown request, stop after the current iteration and run the shutdown order. When the exit is the device-loss clean exit, `Zenith_RuntimeSave_RequestAutosaveForExit` is called after the last `RunFrame` returns: an out-of-tick capture that returns an operation handle, allocates no completion ID, and first cancels every queued SAVE job and the running ordinary write, so it waits behind none; shutdown step 4 (Zenith2/Docs/Contracts.md → "Threading and concurrency contract") then waits for that operation at most 6 s from the call, cancels it with rollback, and waits at most 1 s more, SAVE's 7 s inside the 10 s that [Z2-N-CONTRACT-484] gives the autosave and the shutdown together.

- [Z2-N-ARCH-853] The frame loop runs on the main thread for the life of the process.
- [Z2-N-ARCH-869] Inside `RunFrame` the main thread executes main-affinity jobs and helps with frame work.
- [Z2-N-ARCH-870] Between frames the main thread runs frame-loop steps 1, 3, 3b and 4 and nothing else.
- [Z2-N-ARCH-863] `ZenithRuntime` is the one caller of `BeginHotReload` and `EndHotReload`: in prologue step 6 it opens one window when a change is queued, applies every queued change (`Zenith_ScriptRuntime::ApplyPendingReloads`, the editor's queued schema re-registrations) and closes it, then, in every frame and also when no change was queued, calls `Zenith_ScriptRuntime::CommitPendingReloads()`, which commits changed script-system declarations through `Zenith_SystemReloadScope` so step 8 recompiles them in the same frame; ZenithScript and the editor queue changes and never open a window.
- [Z2-N-ARCH-880] The frame loop runs a frame with `bRunRender` false whenever the renderer's polled device-loss state (`Flux_Renderer::GetDeviceLossState()`, an atomic read) is not `FLUX_DEVICE_LOSS_STATE_NORMAL`, and the device-loss clean exit requests the exit autosave (`Zenith_RuntimeSave_RequestAutosaveForExit`) once the last `RunFrame` has returned, bounds its wait in shutdown step 4 to 6 s plus 1 s after a rollback cancel (frame-loop step 4), and returns `EXIT_CODE_DEVICE_LOST` (6) through the ordinary shutdown order, never `EXIT_CODE_DEVICE` (4), which means only that a required device could not be created.

## Android packaging

### The Gradle template

```text
Player/GradleTemplate/
    settings.gradle.kts.in
    build.gradle.kts.in
    gradle.properties.in
    gradle/wrapper/gradle-wrapper.properties.in
    app/build.gradle.kts.in
    app/proguard-rules.pro
    app/src/main/AndroidManifest.xml.in
    app/src/main/java/org/zenith2/player/Zenith2Activity.java
    app/src/main/java/org/zenith2/player/Zenith2Accessibility.java
    app/src/main/java/org/zenith2/player/Zenith2Connectivity.java
    app/src/main/res/values/strings.xml.in
    app/src/main/res/xml/zenith_data_extraction_rules.xml
    app/src/main/res/xml/zenith_full_backup_content.xml
    app/src/hwasan/resources/lib/arm64-v8a/wrap.sh
```

`zenith_add_game` copies the template into `${ZENITH_OUT}/Games/<Name>/<configure-preset>/android/` with `configure_file` in at-sign substitution mode, copies SDL3's Java activity sources (`org/libsdl/app/*.java`) from the verified SDL3 source in the dependency cache, and substitutes:

| Placeholder | Value |
|---|---|
| `@ZENITH_APPLICATION_ID@` | `android.applicationId` from `game.json` |
| `@ZENITH_VERSION_CODE@`, `@ZENITH_VERSION_NAME@` | `android.versionCode`, `version` |
| `@ZENITH_DISPLAY_NAME@` | `displayName` |
| `@ZENITH_GAME_LIB@` | the game's shared-object name without `lib` and `.so` |
| `@ZENITH_ORIENTATION@` | `android.orientation` |
| `@ZENITH_ABI@` | `arm64-v8a` or `x86_64` |
| `@ZENITH_MIN_SDK@` | `34` |
| `@ZENITH_COMPILE_SDK@`, `@ZENITH_TARGET_SDK@` | the manifest's `androidSdk` pins |
| `@ZENITH_AGP_VERSION@`, `@ZENITH_GRADLE_SHA256@` | the manifest's `agp` and `gradle` pins |
| `@ZENITH_JNI_DIR@` | `${ZENITH_OUT}/Games/<Name>/<configure-preset>/jni`; each build type appends `/<Config>`, where the game target's post-build step stages `lib<Name>.so` under `<abi>/` |
| `@ZENITH_NDK_VERSION@` | the manifest's `ndk` pin, as AGP's `ndkVersion` |
| `@ZENITH_BUILD_TYPES@` | the Gradle build types of the table "The Gradle build types" (Architecture/BuildSystem.md → "Games in CMake"), each with its CMake configuration |
| `@ZENITH_ASSETS_DIR@` | `${ZENITH_OUT}/Cooked/<Name>/android/` |

`Zenith2Accessibility.java` is MOD's file, the class of `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract", and `app/proguard-rules.pro` keeps `org.zenith2.player.Zenith2Accessibility` whole. `Zenith2Connectivity.java` is PLAT's Android connectivity callback bridge (`Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity"), kept whole by `-keep class org.zenith2.player.Zenith2Connectivity { *; }` in `app/proguard-rules.pro`. `Zenith2Activity` extends `org.libsdl.app.SDLActivity`, adds `public android.view.View getAccessibilityHostView()` returning SDL's surface view, and overrides `getLibraries()` to return exactly `{ "@ZENITH_GAME_LIB@" }` (SDL3 is statically linked into the game's shared object). It also overrides `getArguments()`, returning the string-array intent extra `org.zenith2.args` (none when the extra is absent) in every build type except `shipping`, which returns no argument, so the Player receives the command-line options of "Command-line options" on Android (`adb shell am start -n <applicationId>/org.zenith2.player.Zenith2Activity --esa org.zenith2.args <arg>,…`); `app/build.gradle.kts.in` enables `buildFeatures.buildConfig` for the build-type check and declares `namespace = "org.zenith2.player"`, the activity's package, so the generated `BuildConfig` resolves in `Zenith2Activity`, with the game's `android.applicationId` as `defaultConfig.applicationId`. `AndroidManifest.xml.in` declares the game category, the activity's configuration changes (`fontScale` among them, so a font-scale change reaches the accessibility bridge's `ComponentCallbacks` instead of recreating the activity) and the Vulkan 1.3 feature:

```xml
<application android:appCategory="game" android:label="@string/app_name" android:hasCode="true"
	android:allowBackup="true" android:dataExtractionRules="@xml/zenith_data_extraction_rules" android:fullBackupContent="@xml/zenith_full_backup_content">
	<activity android:name="org.zenith2.player.Zenith2Activity" android:exported="true"
		android:screenOrientation="@ZENITH_ORIENTATION@"
		android:configChanges="orientation|screenSize|screenLayout|keyboardHidden|keyboard|navigation|uiMode|density|fontScale">
		<intent-filter>
			<action android:name="android.intent.action.MAIN"/>
			<category android:name="android.intent.category.LAUNCHER"/>
		</intent-filter>
	</activity>
</application>
<uses-feature android:name="android.hardware.vulkan.version" android:version="0x403000" android:required="true"/>
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
```

The two backup-rule files (from Stage 4, copied `COPYONLY`; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Backup and transfer rules") include `saves/` of the user mount alone and exclude `saves/resume.zsave`, `saves/last_profile.zslp` and ONLINE's device record `saves/online/device.zodv` (A3 row ON22), so Android's cloud backup and device-to-device transfer copy the user's saves and nothing else; Android's rule paths are literal, so a restored `*.z2journal`, `*.z2tmp` or `*.corrupt` file is present and benign: SAVE's start-up journal recovery and stale-temporary sweep handle the first two and no load or listing reads the third (`Zenith2/Docs/Design/SAVE/Formats.md` → "File names and directories").

- [Z2-N-ARCH-854] The Android APK contains exactly one native library, the game's shared object, and the Gradle build packages it from the CMake output (`jniLibs`), never through AGP's `externalNativeBuild`, so CMake presets remain the one build of native code.
- [Z2-N-ARCH-855] The `<Name>_Apk` target runs the generated project's Gradle wrapper with the pinned JDK 17 (`JAVA_HOME`) and Android SDK (`ANDROID_HOME`) from the dependency cache.
- [Z2-N-ARCH-862] The Gradle wrapper distribution is verified against `@ZENITH_GRADLE_SHA256@` before use, and the build downloads no unpinned artefact.
- [Z2-N-ARCH-856] Cooked content ships inside the APK's `assets/` for sideload and development APKs; App Bundles and Play Asset Delivery packs are the Stage 6 packaging path (Z2-D-027) and use the same cooked output.

## Required tests

The table "Composition tests" lists the tests of this section; they live in `Player/Tests/` and `Engine/ZenithRuntime/Tests/`, except the Android build check, which is the `ApkCheck` gate step.

- [Z2-N-ARCH-857] Every test in the table "Composition tests" exists under the name given and asserts the case given.

Composition tests:

| Test | Asserts |
|---|---|
| `Test_Composition_ModuleOrderCanonical` | a composition generated from shuffled `modules` lists registers modules in the same canonical order |
| `Test_Composition_InterfaceVersionMismatch_Stops` | a descriptor with a different interface version stops startup with exit code 3 and the message |
| `Test_Composition_NoMutableGlobals` | the writable-data scan of the generated TU's object finds no symbol (run by `ForbiddenCheck`) |
| `Test_Runtime_StartupOrder` | a recording fake of every subsystem observes the startup table's order and the reverse shutdown order |
| `Test_Runtime_StartupFailureUnwinds` | failure injected at each startup step through `Zenith_RuntimeTestHooks` (`Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h`, a test-access header beside `Zenith_Runtime.h`; § "Runtime and entry types") destroys exactly the subsystems constructed so far, in reverse, records the step in the startup report and returns that step's exit code |
| `Test_Runtime_BudgetRun_QualityKeysExactlyOnce` | a budget run whose `--set` options name every registered `render.quality.*` key once starts; a key named by no `--set`, a key named twice and an unregistered `render.quality.` key each fail step 6 with `RUNTIME_ERROR_CONFIGURATION`, exit code 2 and the message of bullet 877 |
| `Test_Runtime_NotBudgetRun_SkipsQualityCheck` | outside a budget run a repeated `--set` of a `render.quality.*` key starts and the last value wins |
| `Test_Runtime_RegistryFrozenBeforeWorld` | registering a type after step 10 outside a hot-reload window fails with `REFLECT_ERROR_FROZEN`, and no world exists before the freeze |
| `Test_Runtime_HotReloadWindowOnlyInPrologue` | a queued behaviour-state change is applied in prologue step 6 and nowhere else; the registry revision advances once |
| `Test_Runtime_HeadlessSkipsDevice` | a headless start creates no window, RHI device, renderer or audio device and runs 300 ticks |
| `Test_Runtime_HeadlessMatchesWindowedSimulation` | the same replay headless and windowed (WARP) yields identical tick hashes |
| `Test_Runtime_ModuleRegistrationsUndone` | destroying a module instance removes its systems, listeners, extractors and consumers |
| `Test_Player_ArgumentParsing` | every option of the table "Command-line options" parses with its argument; an unknown option exits with code 2 |
| `Test_Composition_AndroidExportsSdlMain` | realised as the `ApkCheck` check of the same name (Architecture/BuildSystem.md, bullet 236), not a doctest: `llvm-readelf --dyn-syms` on every built game shared object lists `SDL_main` as a defined, default-visibility function |
