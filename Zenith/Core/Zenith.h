#include <cstdio>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>

#include "Maths/Zenith_Maths.h"
#include "Zenith_Core.h"

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
#define Zenith_Assert(x,...)if(!(x)){Zenith_Error("Assertion failed: ",__VA_ARGS__);__debugbreak();}
#else
#define Zenith_Assert(x, ...)
#endif

#define COUNT_OF(x) sizeof(x) / sizeof(x[0])

#define STUBBED __debugbreak();
//#define ZENITH_RAYTRACING

using GUIDType = uint64_t;
static struct Zenith_GUID {
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
	operator uint32_t() { Zenith_Assert(false, "Attempting to compress GUID into 32 bits"); }
};