#include <cstdio>
#include <cstdint>
#include <set>

#define ZENITH_LOG
#ifdef ZENITH_LOG
#define Zenith_Log(...)printf(__VA_ARGS__);printf("\n")
#define Zenith_Error(...)printf(__VA_ARGS__);printf("\n")
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

//#define ZENITH_RAYTRACING