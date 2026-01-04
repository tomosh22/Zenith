#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <set>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <list>
#include <concepts>
#include <atomic>

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

#include "Zenith_OS_Include.h"
#include "Zenith_DebugBreak.h"

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

	LOG_CATEGORY_COUNT
};

inline constexpr const char* Zenith_LogCategoryNames[LOG_CATEGORY_COUNT] = {
	"General", "Core", "Scene", "ECS", "Asset", "Vulkan", "Renderer",
	"Mesh", "Animation", "Terrain", "Shadows", "Gizmos", "Particles",
	"Text", "Material", "Physics", "TaskSystem", "Editor", "Prefab",
	"UI", "Input", "Window", "Tools", "UnitTest", "Gameplay"
};

inline const char* Zenith_GetLogCategoryName(Zenith_LogCategory eCategory)
{
	return (eCategory < LOG_CATEGORY_COUNT) ? Zenith_LogCategoryNames[eCategory] : "Unknown";
}

#define ZENITH_LOG
#ifdef ZENITH_LOG

#ifdef ZENITH_TOOLS
// Forward declare editor console function
void Zenith_EditorAddLogMessage(const char* szMessage, int eLevel, Zenith_LogCategory eCategory);

// Helper to format and send to both printf and editor console
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
	Zenith_EditorAddLogMessage(prefixedBuffer, eLevel, eCategory);
}

#define Zenith_Log(eCategory, ...) Zenith_LogImpl(eCategory, 0, __VA_ARGS__)
#define Zenith_Error(eCategory, ...) Zenith_LogImpl(eCategory, 2, __VA_ARGS__)
#define Zenith_Warning(eCategory, ...) Zenith_LogImpl(eCategory, 1, __VA_ARGS__)
#else
#define Zenith_Log(eCategory, ...) { printf("[%s] ", Zenith_GetLogCategoryName(eCategory)); printf(__VA_ARGS__); printf("\n"); }
#define Zenith_Error(eCategory, ...) { printf("[%s] ", Zenith_GetLogCategoryName(eCategory)); printf(__VA_ARGS__); printf("\n"); }
#define Zenith_Warning(eCategory, ...) { printf("[%s] ", Zenith_GetLogCategoryName(eCategory)); printf(__VA_ARGS__); printf("\n"); }
#endif

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

#ifdef ZENITH_DEBUG_VARIABLES
#define DEBUGVAR static
#else
#define DEBUGVAR static const
#endif

#define COUNT_OF(x) sizeof(x) / sizeof(x[0])

#define STUBBED Zenith_DebugBreak();
//#define ZENITH_RAYTRACING

using GUIDType = uint64_t;
struct Zenith_GUID
{
	static Zenith_GUID Invalid;
	Zenith_GUID()
	{
		for (uint64_t i = 0; i < sizeof(GUIDType) * 8; i++)
			if (rand() > RAND_MAX / 2)
				m_uGUID |= static_cast<GUIDType>(1u) << i;
	}
	Zenith_GUID(GUIDType uGuid) : m_uGUID(uGuid) {}
	GUIDType m_uGUID = 0;

	bool operator == (const Zenith_GUID& xOther) const
	{
		return m_uGUID == xOther.m_uGUID;
	}

	operator uint64_t() { return m_uGUID; }
	operator uint32_t() = delete;
};

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

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "Memory/Zenith_MemoryManagement.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"