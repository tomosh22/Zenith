#pragma once

#include <cstdlib>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <list>

#include "Maths/Zenith_Maths.h"
#include "Zenith_Core.h"

#include "Zenith_OS_Include.h"

#ifdef ZENITH_WINDOWS
#include <Windows.h>
#endif

#define ZENITH_LOG
#ifdef ZENITH_LOG
#define Zenith_Log(...){printf(__VA_ARGS__);printf("\n");}
#define Zenith_Error(...){printf(__VA_ARGS__);printf("\n");}
#else
#define Zenith_Log(...)
#define Zenith_Error(...)
#endif

#define ZENITH_ASSERT
#ifdef ZENITH_ASSERT
#define Zenith_Assert(x,...)if(!(x)){Zenith_Error("Assertion failed: " __VA_ARGS__);__debugbreak();}
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

#define STUBBED __debugbreak();
//#define ZENITH_RAYTRACING
//#define ZENITH_MERGE_GBUFFER_PASSES

using GUIDType = uint64_t;
struct Zenith_GUID {
	static Zenith_GUID Invalid;
	Zenith_GUID() {
		for (uint64_t i = 0; i < sizeof(GUIDType) * 8; i++)
			if (rand() > RAND_MAX / 2)
				m_uGUID |= static_cast<GUIDType>(1u) << i;
	}
	Zenith_GUID(GUIDType uGuid) : m_uGUID(uGuid) {}
	GUIDType m_uGUID = 0;

	bool operator == (const Zenith_GUID& xOther) const {
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

#define ZENITH_MAX_TEXTURES 1024
#define ZENITH_MAX_MESHES 16384
#define ZENITH_MAX_MATERIALS 1024

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "Memory/Zenith_MemoryManagement.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"