#pragma once

#pragma warning(push)
#pragma warning(disable: 4530 4244)
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
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
// Phase 2: g_xEngine.Frame() is the universal per-frame timing
// accessor (replaces Zenith_Core::GetDt etc). Pulled into the PCH so
// every TU has it without per-file include churn. Both headers are
// light: Zenith_Engine.h is just a class decl + <type_traits>,
// FrameContext.h adds <chrono> only.
#include "Core/Zenith_Engine.h"
#include "Core/FrameContext.h"

#include "Zenith_OS_Include.h"
#include "Zenith_DebugBreak.h"

// GLFW defines APIENTRY; undef before Windows.h to avoid C4005 redefinition warning
#ifdef APIENTRY
#undef APIENTRY
#endif

#ifdef ZENITH_WINDOWS
#include <Windows.h>
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

// Memory tracking with guard bytes, leak detection, and callstack capture
// Thread-safety approach:
// - std::atomic<bool> for initialization flag (acquire/release semantics)
// - thread_local recursion guards (no cross-thread interference)
// - Initialization check BEFORE accessing TLS (avoids TLS init allocating)
// - Untracked allocations (static init) silently use plain malloc/free
//
// STATUS: currently DISABLED, but planned to be enabled in the future.
// The tracked code path (AllocateTracked, guard bytes, MemoryTracker,
// category stacking) is the target implementation, NOT dead scaffolding.
// Do not remove or restructure either path. Both paths intentionally live
// side-by-side in Zenith_MemoryManagement.cpp behind this macro until
// enablement lands.
//#define ZENITH_MEMORY_MANAGEMENT_ENABLED
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

extern const char* Project_GetGameAssetsDirectory();

// Asset limits - now defined in ZenithConfig.h for central documentation
// These macros maintain backward compatibility with existing code
#include "ZenithConfig.h"
#define ZENITH_MAX_TEXTURES ZenithConfig::MAX_TEXTURES
#define ZENITH_MAX_MESHES ZenithConfig::MAX_MESHES
#define ZENITH_MAX_MATERIALS ZenithConfig::MAX_MATERIALS

// Memory management system setup
// The macro is only enabled via Zenith_MemoryManagement_Enabled.h if:
// 1. ZENITH_MEMORY_MANAGEMENT_ENABLED is defined (line 156)
// 2. ZENITH_PLACEMENT_NEW_ZONE is NOT defined (files that create third-party objects define this)
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "Memory/Zenith_MemoryManagement.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

// Unit test framework - macros compile to no-ops when ZENITH_TESTING is undefined.
#include "Core/Zenith_TestFramework.h"