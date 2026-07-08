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

	printf("%s\n", prefixedBuffer);
	fflush(stdout);
#ifdef ZENITH_TOOLS
	Zenith_EditorAddLogMessage(prefixedBuffer, eLevel, eCategory);
#else
	(void)eLevel;
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