#pragma once

#pragma warning(push)
#pragma warning(disable: 4530 4244)
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <chrono>
#include <concepts>
#include <atomic>
#include <random>
#include <algorithm>
#pragma warning(pop)

#ifdef ZENITH_ANDROID
// Zenith_LogImpl routes to logcat on Android -- an app's stdout is discarded.
#include <android/log.h>
#endif

using u_int = unsigned int;

using u_int8 = unsigned char;
static_assert(sizeof(u_int8) == 1);

using u_int16 = unsigned short;
static_assert(sizeof(u_int16) == 2);

using u_int32 = unsigned int;
static_assert(sizeof(u_int32) == 4);

using u_int64 = unsigned long long;
static_assert(sizeof(u_int64) == 8);


#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_String.h"
#include "Zenith_Core.h"
// W5.3 (PCH slimming): Zenith_Engine.h (the g_xEngine accessor surface) was
// DEMOTED out of the PCH so a subsystem add no longer invalidates the precompiled
// header for TUs that don't use g_xEngine. The actual g_xEngine code-users now
// include it themselves (it's just a class decl + <type_traits> — cheap + cycle-free);
// the ZenithECS leaf is deliberately excluded (it uses g_xEngine only in doc comments,
// staying engine-include-free). FrameContext.h stays (g_xEngine.Frame() timing prelude).
#include "Core/FrameContext.h"

#include "Zenith_OS_Include.h"
#include "Zenith_DebugBreak.h"

// W5.2 (PCH slimming): <Windows.h> is no longer dragged into the precompiled header
// (it was here purely for caller convenience — Zenith.h itself names no Win32 type).
// The handful of TUs that genuinely use Win32 directly now #include <Windows.h>
// themselves; the platform mutex/semaphore wrappers keep it confined to their .cpp
// via the W5.1 opaque-storage change. This removes a large, volatile system header
// from every translation unit's PCH.

// ----------------------------------------------------------------------------
// Render-backend query (compile-time).
//
// HEADLESS IS A BUILD CONFIG, NOT A FLAG. A `Null_*` config defines
// ZENITH_NULL_RENDERER, compiles the GPU-less Zenith/Null backend, and creates
// its window hidden — that IS "headless". There is no runtime --headless.
//
// Prefer this constexpr helper over a raw `#ifdef` wherever BOTH sides should
// stay compiled (so a Vulkan build still type-checks the Null branch and vice
// versa, and the dead branch is folded away with no runtime cost). Reach for
// `#ifdef ZENITH_NULL_RENDERER` only when one side genuinely cannot compile —
// e.g. it names a backend-specific type.
// ----------------------------------------------------------------------------
constexpr bool Zenith_IsNullRenderer()
{
#ifdef ZENITH_NULL_RENDERER
	return true;
#else
	return false;
#endif
}

// ----------------------------------------------------------------------------
// Authoring-math determinism.
//
// The whole project compiles /fp:fast, which lets the optimizer reassociate,
// contract into FMA, and swap scalar libm for its vectorized variants — and what
// it actually does differs by OPTIMIZATION LEVEL. So the same source computes
// different floats in a Debug tools build and a Release one. Measured on this
// toolchain: `1.0f + 0.8f*r`, `s * (0.95f + 0.1f*r)` and `cosf(a)*95.0f` ALL
// diverge between /Od and /O2 under /fp:fast, and none of them diverge under
// /fp:precise.
//
// That is invisible for gameplay but fatal for anything whose floats land in a
// COMMITTED asset: a Debug tools boot and a Release one write different bytes,
// so the file ping-pongs in git forever while every tolerance-based guard stays
// green. RenderTest's scene did exactly this — 19266 bytes across its 2520 tree
// instances plus six authored rotations (see Games/RenderTest/CLAUDE.md); the
// same class of defect is Zenithmon's ZM-D-183.
//
// Wrap authoring math — anything computing a value that gets SERIALIZED into a
// tracked asset — in this pair. It pins /fp:precise for those functions only, so
// runtime code keeps /fp:fast. Place it around whole function definitions, never
// inside a function body (an MSVC requirement for float_control).
//
// The frozen-constant route (ZM's AddStep_SetTransformRotationQuat + bit_cast)
// remains the right answer for a HANDFUL of authored values. This is for the
// case where there are thousands and they are computed, not typed.
// ----------------------------------------------------------------------------
#if defined(_MSC_VER)
#define ZENITH_AUTHORING_DETERMINISM_BEGIN __pragma(float_control(precise, on, push))
#define ZENITH_AUTHORING_DETERMINISM_END   __pragma(float_control(pop))
#else
#define ZENITH_AUTHORING_DETERMINISM_BEGIN
#define ZENITH_AUTHORING_DETERMINISM_END
#endif

// Log categories for categorized logging output
enum Zenith_LogCategory : u_int8
{
	LOG_CATEGORY_GENERAL = 0,   // Uncategorized / fallback
	LOG_CATEGORY_CORE,          // Main loop, config, memory
	LOG_CATEGORY_SCENE,         // Scene management, entity lifecycle
	LOG_CATEGORY_ECS,           // Component registry, component operations
	LOG_CATEGORY_ASSET,         // Asset loading, caching, database
	LOG_CATEGORY_VULKAN,        // Vulkan backend operations
	LOG_CATEGORY_RENDERER,      // Flux renderer core
	LOG_CATEGORY_MESH,          // Mesh instances, geometry
	LOG_CATEGORY_ANIMATION,     // Animation clips, state machines, IK
	LOG_CATEGORY_TERRAIN,       // Terrain rendering, streaming
	LOG_CATEGORY_SHADOWS,       // Shadow mapping
	LOG_CATEGORY_GIZMOS,        // Editor gizmos
	LOG_CATEGORY_PARTICLES,     // Particle system
	LOG_CATEGORY_TEXT,          // Text/font rendering
	LOG_CATEGORY_MATERIAL,      // Material assets
	LOG_CATEGORY_PHYSICS,       // Jolt physics integration
	LOG_CATEGORY_TASKSYSTEM,    // Task parallelism
	LOG_CATEGORY_EDITOR,        // Editor UI, panels
	LOG_CATEGORY_PREFAB,        // Prefab system
	LOG_CATEGORY_UI,            // UI system
	LOG_CATEGORY_INPUT,         // Input handling
	LOG_CATEGORY_WINDOW,        // Window/platform
	LOG_CATEGORY_TOOLS,         // Asset export, migration
	LOG_CATEGORY_UNITTEST,      // Unit test output
	LOG_CATEGORY_GAMEPLAY,      // Game-specific logs
	LOG_CATEGORY_AI,            // AI system (behavior trees, navigation, perception)

	LOG_CATEGORY_COUNT
};

inline constexpr const char* Zenith_LogCategoryNames[LOG_CATEGORY_COUNT] = {
	"General", "Core", "Scene", "ECS", "Asset", "Vulkan", "Renderer",
	"Mesh", "Animation", "Terrain", "Shadows", "Gizmos", "Particles",
	"Text", "Material", "Physics", "TaskSystem", "Editor", "Prefab",
	"UI", "Input", "Window", "Tools", "UnitTest", "Gameplay", "AI"
};

inline const char* Zenith_GetLogCategoryName(Zenith_LogCategory eCategory)
{
	return (eCategory < LOG_CATEGORY_COUNT) ? Zenith_LogCategoryNames[eCategory] : "Unknown";
}

#define ZENITH_LOG
#ifdef ZENITH_LOG

#ifdef ZENITH_TOOLS
void Zenith_EditorAddLogMessage(const char* szMessage, int eLevel, Zenith_LogCategory eCategory);
#endif

#ifdef ZENITH_WINDOWS
// The always-on log file sink (implemented in Zenith.cpp -- see the block comment
// there for why it lives in THAT file and not a new one: this declaration is
// reached from an INLINE function in the PCH, so the symbol must resolve for the
// L0 leaf and every Sentinel link proof too, and it must NOT ALLOCATE because the
// first log line of the process comes from a static initialiser).
//
// Android is excluded because logcat already IS a persistent, timestamped,
// filterable sink there; other platforms get no sink rather than an untested path.
void Zenith_LogFileSinkWrite(const char* szMessage, int eLevel);

// Absolute path of this run's log file; empty until the first line is written, and
// empty forever if the sink could not open one. Never null.
const char* Zenith_GetLogFilePath();
#endif

inline void Zenith_LogImpl(Zenith_LogCategory eCategory, int eLevel, const char* szFormat, ...)
{
	char buffer[2048];
	char prefixedBuffer[2112];

	va_list args;
	va_start(args, szFormat);
	vsnprintf(buffer, sizeof(buffer), szFormat, args);
	va_end(args);

	snprintf(prefixedBuffer, sizeof(prefixedBuffer), "[%s] %s",
		Zenith_GetLogCategoryName(eCategory), buffer);

#ifdef ZENITH_ANDROID
	// An Android app's stdout goes to /dev/null, so a plain printf makes the
	// ENTIRE engine silent on device -- which is exactly when logs matter most,
	// since there is no console to attach to. Route to logcat instead, carrying
	// the severity so `adb logcat *:E` filters correctly, and tag by category so
	// `adb logcat -s Zenith.Vulkan` works. NOTE the category names are
	// mixed-case (Zenith_LogCategoryNames: "Vulkan", "Core", "Renderer", ...) and
	// `logcat -s` matches tags EXACTLY -- "Zenith.VULKAN" silently matches nothing.
	static const int aiAndroidPriority[] = { ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR };
	const int iPriority = (eLevel >= 0 && eLevel <= 2) ? aiAndroidPriority[eLevel] : ANDROID_LOG_INFO;

	char acTag[64];
	snprintf(acTag, sizeof(acTag), "Zenith.%s", Zenith_GetLogCategoryName(eCategory));

	// The category is already in the tag; log the unprefixed message so it is
	// not repeated on every line.
	__android_log_write(iPriority, acTag, buffer);
#else
	printf("%s\n", prefixedBuffer);
	fflush(stdout);
#ifdef ZENITH_WINDOWS
	// Same line, to a file that survives the process. A hand-launched run has no
	// redirected stdout, and those are precisely the runs where something
	// interesting happened.
	Zenith_LogFileSinkWrite(prefixedBuffer, eLevel);
#endif
#endif
#ifdef ZENITH_TOOLS
	Zenith_EditorAddLogMessage(prefixedBuffer, eLevel, eCategory);
#else
	(void)eLevel;
	// On Android the prefix lives in the logcat tag, so the composed buffer has
	// no consumer in a non-tools build -- and warnings are errors here.
	(void)prefixedBuffer;
#endif
}

#define Zenith_Log(eCategory, ...) Zenith_LogImpl(eCategory, 0, __VA_ARGS__)
#define Zenith_Error(eCategory, ...) Zenith_LogImpl(eCategory, 2, __VA_ARGS__)
#define Zenith_Warning(eCategory, ...) Zenith_LogImpl(eCategory, 1, __VA_ARGS__)

#else
#define Zenith_Log(eCategory, ...)
#define Zenith_Error(eCategory, ...)
#define Zenith_Warning(eCategory, ...)
#endif

#define ZENITH_ASSERT
#ifdef ZENITH_ASSERT
#define Zenith_Assert(x,...)if(!(x)){Zenith_Error(LOG_CATEGORY_CORE, "Assertion failed: " __VA_ARGS__);Zenith_DebugBreak();}
#else
#define Zenith_Assert(x, ...)
#endif

// Release-survivable check tier.
//
// Unlike Zenith_Assert (which calls Zenith_DebugBreak() and is meant to halt a
// developer at the point of a logic error), the check tier LOGS the failure and
// CONTINUES execution. It is intended for conditions that can legitimately fail
// in a shipping build (resource exhaustion, a GPU upload refusing, a queue
// overflowing) where the caller has a real recovery path and a hard break would
// be a worse outcome than a logged, handled degradation.
//
//   Zenith_Check(cond, ...)  — if cond is false, Zenith_Error(...) and fall
//                              through. NEVER breaks. Use at a recoverable site
//                              and pair it with the caller's fallback path.
//   Zenith_Verify(cond)      — evaluates cond for its SIDE EFFECTS and, on
//                              false, logs. The expression always runs even when
//                              checks are compiled out (see below), so it is
//                              safe to wrap a call whose return value you check.
//
// Gated by ZENITH_RUNTIME_CHECKS, defined ON for Debug and Release here. A
// future Final configuration can leave it undefined to strip the logging:
//   - Zenith_Check then compiles to nothing (cond is NOT evaluated — like
//     Zenith_Assert in a no-assert build).
//   - Zenith_Verify STILL evaluates cond (side effects must run) but does not
//     log; the result is simply discarded.
#define ZENITH_RUNTIME_CHECKS
#ifdef ZENITH_RUNTIME_CHECKS
#define Zenith_Check(x,...)if(!(x)){Zenith_Error(LOG_CATEGORY_CORE, "Check failed: " __VA_ARGS__);}
#define Zenith_Verify(x)if(!(x)){Zenith_Error(LOG_CATEGORY_CORE, "Verify failed: " #x);}
#else
#define Zenith_Check(x, ...)
#define Zenith_Verify(x)(void)(x)
#endif

#define ZENITH_USE_FINAL
#ifdef ZENITH_USE_FINAL
#define ZENITH_FINAL final
#else
#define ZENITH_FINAL
#endif

#ifdef ZENITH_TOOLS
#define ZENITH_DEBUG_VARIABLES
#endif

#define ZENITH_TESTING

// Enables GPU debug markers (RenderDoc / Nsight / PIX pass labels) and pulls
// in the supporting machinery they require: the VK_EXT_debug_utils instance
// extension and vk::DispatchLoaderDynamic. Always defined for now; can be
// undefined in a strict shipping configuration if marker overhead is ever
// measured to matter. Decoupled from ZENITH_DEBUG so the validation messenger
// (noisy, debug-only) stays orthogonal to the marker pipeline.
#define ZENITH_FLUX_PROFILING

// ZENITH_DEBUG_VARIABLES is a strict subset of ZENITH_TOOLS — the block above
// is the ONLY site that defines it, and it only runs when ZENITH_TOOLS is
// already active. Any future define of ZENITH_DEBUG_VARIABLES from a build
// system or other header breaks the "implies" relationship and would let
// editor-only debug variables leak into shipping binaries. Fail the compile
// loudly if that ever happens.
#if defined(ZENITH_DEBUG_VARIABLES) && !defined(ZENITH_TOOLS)
#error "ZENITH_DEBUG_VARIABLES must imply ZENITH_TOOLS. If you need the debug variable tree in a non-tools configuration, either define ZENITH_TOOLS too or split the guard. Mixing the two in a shipping build will surface debug variables that were only intended for editor builds."
#endif

#ifdef ZENITH_DEBUG_VARIABLES
#define DEBUGVAR static
#else
#define DEBUGVAR static const
#endif

// -----------------------------------------------------------------------------
// Memory tracking tier (AAA memory overhaul).
//
// ZENITH_MEMORY_TRACKING_LEVEL selects the cost/feature tier of the global
// operator new/delete tracking layer:
//
//   2 = FULL  (Debug):   per-alloc hashmap records, guard bytes (0xDEADBEEF),
//                        0xCD/0xDD fill, callstack capture, leak + double-free +
//                        guard checks, per-category + frame stats.
//   1 = LITE  (Release): lock-free per-category atomic counters + totals + peak
//                        via a 16-byte header-before-user cookie. No hashmap, no
//                        guards, no callstacks. Near-zero overhead.
//   0 = OFF   (Final):   operator new/delete fall straight through to malloc/free.
//                        Everything compiles out.
//
// Attribution is callstack + thread-local category scopes (ZENITH_MEMORY_SCOPE),
// resolved INSIDE the allocator (behind the init-flag check) — there is no longer
// any `#define new` hammer, so enabling a tier is near-zero call-site churn.
//
// The build system sets this per config in Build/Sharpmake_Common.cs (Debug=2,
// Release=1), in lockstep across the base/PCH lib + engine/game/tool projects
// (same ODR rule as ZENITH_TOOLS / ZENITH_PROFILING_ENABLED).
//
// The header default keys off ZENITH_DEBUG (which Sharpmake sets per config), so
// Debug=FULL / Release=LITE holds even before the explicit Sharpmake define lands.
// A future shipping/Final config defines ZENITH_MEMORY_TRACKING_LEVEL=0 to strip it.
#ifndef ZENITH_MEMORY_TRACKING_LEVEL
	#ifdef ZENITH_DEBUG
		#define ZENITH_MEMORY_TRACKING_LEVEL 2
	#else
		#define ZENITH_MEMORY_TRACKING_LEVEL 1
	#endif
#endif
#if ZENITH_MEMORY_TRACKING_LEVEL < 0 || ZENITH_MEMORY_TRACKING_LEVEL > 2
	#error "ZENITH_MEMORY_TRACKING_LEVEL must be 0 (OFF), 1 (LITE), or 2 (FULL)"
#endif
// FULL-only machinery (hashmap tracker, guard bytes, callstacks, per-alloc records,
// forensics editor tabs). ANY = LITE or FULL (category stack, stats, budgets, the
// unified aggregator, the profiler Memory tab + HUD). Both are 0 at tier OFF.
#define ZENITH_MEMORY_TRACKING_FULL (ZENITH_MEMORY_TRACKING_LEVEL >= 2)
#define ZENITH_MEMORY_TRACKING_ANY  (ZENITH_MEMORY_TRACKING_LEVEL >= 1)
#define ZENITH_INPUT_SIMULATOR

#define COUNT_OF(x) sizeof(x) / sizeof(x[0])

#define STUBBED Zenith_DebugBreak();
//#define ZENITH_RAYTRACING

using GUIDType = uint64_t;
struct Zenith_GUID
{
	static Zenith_GUID Invalid;

	// Thread-safe GUID generation using proper random number generation
	// Uses thread_local RNG to avoid data races and ensure high-quality randomness
	Zenith_GUID()
	{
		// Thread-local RNG ensures thread safety without locks
		// std::random_device provides entropy for seeding
		// std::mt19937_64 provides high-quality 64-bit random numbers
		thread_local std::mt19937_64 s_xGenerator([]() {
			std::random_device xRd;
			// Seed with multiple values for better entropy
			std::seed_seq xSeed{xRd(), xRd(), xRd(), xRd()};
			return std::mt19937_64(xSeed);
		}());

		m_uGUID = s_xGenerator();
	}

	Zenith_GUID(GUIDType uGuid) : m_uGUID(uGuid) {}
	GUIDType m_uGUID = 0;

	bool operator == (const Zenith_GUID& xOther) const
	{
		return m_uGUID == xOther.m_uGUID;
	}

	operator uint64_t() const { return m_uGUID; }
	operator uint32_t() = delete;
};

inline Zenith_GUID Zenith_GUID::Invalid = Zenith_GUID(static_cast<GUIDType>(0u));

template <>
struct std::hash<Zenith_GUID>
{
	size_t operator()(const Zenith_GUID& xGUID) const
	{
		return std::hash<GUIDType>()(xGUID.m_uGUID);
	}
};

// Game-wiring contract (Project_* hooks) — one declaration site, pulled in via
// the PCH so every .cpp sees the signatures. See the header for the lifecycle
// contract (esp. the Project_Shutdown handle-Clear rule).
#include "Core/Zenith_ProjectHooks.h"

// Asset limits - now defined in ZenithConfig.h for central documentation
// These macros maintain backward compatibility with existing code
#include "ZenithConfig.h"
#define ZENITH_MAX_TEXTURES ZenithConfig::MAX_TEXTURES
#define ZENITH_MAX_MESHES ZenithConfig::MAX_MESHES
#define ZENITH_MAX_MATERIALS ZenithConfig::MAX_MATERIALS

// Memory management: global operator new/delete overloads + the tiered tracking
// layer (ZENITH_MEMORY_TRACKING_LEVEL, defined above). There is no longer a
// `#define new` hammer, so no include sandwich is needed here — the two legacy
// _Disabled.h/_Enabled.h stubs are being swept out.
#include "Memory/Zenith_MemoryManagement.h"

// Unit test framework - macros compile to no-ops when ZENITH_TESTING is undefined.
#include "Core/Zenith_TestFramework.h"