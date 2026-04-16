# Hybrid Zenith Unit Testing Framework - Implementation Plan

## Overview

Combine the simplicity of Plan 1 with the type-safe assertions and `ZENITH_TESTING` guard of Plan 2.

### Features Retained from Each Plan

| Feature | Plan 1 | Plan 2 | Hybrid |
|---------|--------|--------|--------|
| 2-file structure | ✓ | - | ✓ |
| ZENITH_TESTING guard | - | ✓ | ✓ |
| Static auto-registration | ✓ | ✓ | ✓ |
| Template ASSERT_EQ/ASSERT_NE | - | ✓ | ✓ |
| Vector3 assertion | - | ✓ | ✓ |
| GT/LT/GE/LE assertions | ✓ | - | ✓ |
| StrEq assertion | ✓ | ✓ | ✓ |
| Timing per test | ✓ | - | ✓ |
| Skip/Fail methods | ✓ | - | ✓ |
| Hard break on failure | ✓ | - | ✓ |
| Selective execution | ✓ | ✓ | ✗ |

---

## Conventions Applied

| Element | Convention | Example |
|---------|-----------|---------|
| Class/struct names | `Zenith_PascalCase` | `Zenith_TestRunner`, `Zenith_TestCase` |
| Member variables | `m_` prefix + type hint | `m_uTestCount`, `m_strCategory` |
| Functions | `PascalCase` | `RegisterTest`, `RunAllTests` |
| Macros | `ZENITH_SCREAMING_SNAKE_CASE` | `ZENITH_ASSERT_EQ`, `ZENITH_TEST` |
| Indentation | Tabs | (not spaces) |
| Braces | Opening on new line | `if (cond)\n{\n` |
| Headers | `#pragma once` | All headers |
| .cpp start | `#include "Zenith.h"` | All .cpp files |

---

## Phase 1: Core Framework

### Step 1.1: Create `Zenith_TestFramework.h`

**File:** `C:\dev\Zenith\Zenith\Core\Zenith_TestFramework.h`

```cpp
#pragma once

#include "Zenith.h"
#include "Collections/Zenith_Vector.h"

#ifdef ZENITH_TESTING

// ============================================================================
// Zenith_TestCase - Represents a single registered test
// ============================================================================
struct Zenith_TestCase
{
    const char* m_strCategory;
    const char* m_strName;
    void (*m_pfnTest)();
};

// ============================================================================
// Zenith_TestRunner - Singleton test runner with auto-registration
// ============================================================================
class Zenith_TestRunner
{
public:
    static Zenith_TestRunner& Instance();

    void RegisterTest(const char* strCategory, const char* strName, void (*pfnTest)());
    void RunAllTests();

    // Core assertion methods
    void AssertTrue(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertFalse(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertNull(const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertNotNull(const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertStrEq(const char* strA, const char* strB, const char* strFile, int iLine, const char* strMsg, ...);
    void Fail(const char* strMsg, const char* strFile, int iLine);
    void Skip(const char* strReason);

    // Comparison assertions (u_int64)
    void AssertEq(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertNe(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertGt(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertLt(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertGe(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);
    void AssertLe(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);

    // Float assertion with epsilon
    void AssertEq(float fA, float fB, float fEpsilon, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);

    // Vector3 assertion with epsilon
    void AssertNearVec3(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fEpsilon, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...);

    u_int GetTestCount() const { return m_uTestCount; }
    u_int GetPassedCount() const { return m_uPassedCount; }
    u_int GetFailedCount() const { return m_uFailedCount; }
    u_int GetSkippedCount() const { return m_uSkippedCount; }

private:
    void HandleFailure(const char* strAssertionType, const char* strMsg, const char* strFile, int iLine);

    Zenith_TestRunner() = default;
    Zenith_TestRunner(const Zenith_TestRunner&) = delete;
    Zenith_TestRunner& operator=(const Zenith_TestRunner&) = delete;

    u_int m_uTestCount = 0;
    u_int m_uPassedCount = 0;
    u_int m_uFailedCount = 0;
    u_int m_uSkippedCount = 0;
    bool m_bCurrentTestFailed = false;
    Zenith_Vector<Zenith_TestCase> m_axTests;
};

// ============================================================================
// ZENITH_TEST - Registers a test via static initialization
// ============================================================================
#define ZENITH_TEST(category, name) \
    static void category##_##name##_test(); \
    struct category##_##name##_Registrar { \
        category##_##name##_Registrar() { \
            Zenith_TestRunner::Instance().RegisterTest(#category, #name, category##_##name##_test); \
        } \
    } category##_##name##_Instance; \
    static void category##_##name##_test()

// ============================================================================
// Assertion Macros - Delegate to Zenith_TestRunner instance methods
// ============================================================================
#define ZENITH_ASSERT_TRUE(expr, ...) \
    Zenith_TestRunner::Instance().AssertTrue(expr, #expr, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_FALSE(expr, ...) \
    Zenith_TestRunner::Instance().AssertFalse(expr, #expr, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_NULL(ptr, ...) \
    Zenith_TestRunner::Instance().AssertNull(static_cast<const void*>(ptr), #ptr, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_NOT_NULL(ptr, ...) \
    Zenith_TestRunner::Instance().AssertNotNull(static_cast<const void*>(ptr), #ptr, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_STREQ(a, b, ...) \
    Zenith_TestRunner::Instance().AssertStrEq(a, b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_EQ(a, b, ...) \
    Zenith_TestRunner::Instance().AssertEq(static_cast<u_int64>(a), static_cast<u_int64>(b), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_NE(a, b, ...) \
    Zenith_TestRunner::Instance().AssertNe(static_cast<u_int64>(a), static_cast<u_int64>(b), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_GT(a, b, ...) \
    Zenith_TestRunner::Instance().AssertGt(static_cast<u_int64>(a), static_cast<u_int64>(b), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_LT(a, b, ...) \
    Zenith_TestRunner::Instance().AssertLt(static_cast<u_int64>(a), static_cast<u_int64>(b), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_GE(a, b, ...) \
    Zenith_TestRunner::Instance().AssertGe(static_cast<u_int64>(a), static_cast<u_int64>(b), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_LE(a, b, ...) \
    Zenith_TestRunner::Instance().AssertLe(static_cast<u_int64>(a), static_cast<u_int64>(b), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_EQ_FLOAT(a, b, epsilon, ...) \
    Zenith_TestRunner::Instance().AssertEq(static_cast<float>(a), static_cast<float>(b), static_cast<float>(epsilon), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

#define ZENITH_ASSERT_NEAR_VEC3(a, b, epsilon, ...) \
    Zenith_TestRunner::Instance().AssertNearVec3(a, b, static_cast<float>(epsilon), #a, #b, __FILE__, __LINE__, __VA_ARGS__)

// ============================================================================
// Test Control Macros
// ============================================================================
#define ZENITH_SKIP(reason) \
    Zenith_TestRunner::Instance().Skip(reason)

#define ZENITH_FAIL(message) \
    Zenith_TestRunner::Instance().Fail(message, __FILE__, __LINE__)

#else // ZENITH_TESTING

// Stub definitions when testing is disabled
#define ZENITH_TEST(category, name) static_assert(false, "ZENITH_TESTING not defined")
#define ZENITH_ASSERT_TRUE(...) ((void)0)
#define ZENITH_ASSERT_FALSE(...) ((void)0)
#define ZENITH_ASSERT_NULL(...) ((void)0)
#define ZENITH_ASSERT_NOT_NULL(...) ((void)0)
#define ZENITH_ASSERT_STREQ(...) ((void)0)
#define ZENITH_ASSERT_EQ(...) ((void)0)
#define ZENITH_ASSERT_NE(...) ((void)0)
#define ZENITH_ASSERT_GT(...) ((void)0)
#define ZENITH_ASSERT_LT(...) ((void)0)
#define ZENITH_ASSERT_GE(...) ((void)0)
#define ZENITH_ASSERT_LE(...) ((void)0)
#define ZENITH_ASSERT_EQ_FLOAT(...) ((void)0)
#define ZENITH_ASSERT_NEAR_VEC3(...) ((void)0)
#define ZENITH_SKIP(reason) ((void)0)
#define ZENITH_FAIL(message) ((void)0)

#endif // ZENITH_TESTING
```

### Step 1.2: Create `Zenith_TestFramework.cpp`

**File:** `C:\dev\Zenith\Zenith\Core\Zenith_TestFramework.cpp`

```cpp
#include "Zenith.h"
#include "Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include <cstdarg>
#include <cstring>
#include <cmath>

Zenith_TestRunner& Zenith_TestRunner::Instance()
{
    static Zenith_TestRunner s_xInstance;
    return s_xInstance;
}

void Zenith_TestRunner::RegisterTest(const char* strCategory, const char* strName, void (*pfnTest)())
{
    Zenith_TestCase xCase;
    xCase.m_strCategory = strCategory;
    xCase.m_strName = strName;
    xCase.m_pfnTest = pfnTest;
    m_axTests.PushBack(xCase);
    m_uTestCount++;
}

void Zenith_TestRunner::RunAllTests()
{
    Zenith_Log(LOG_CATEGORY_UNITTEST, "[==========] Running %u tests...", m_uTestCount);

    for (const Zenith_TestCase& xCase : m_axTests)
    {
        Zenith_Log(LOG_CATEGORY_UNITTEST, "[ RUN      ] %s::%s", xCase.m_strCategory, xCase.m_strName);

        m_bCurrentTestFailed = false;
        auto xStart = std::chrono::high_resolution_clock::now();

        try
        {
            xCase.m_pfnTest();
        }
        catch (const Zenith_TestSkippedException&)
        {
            // Skip counted in Skip()
        }
        catch (...)
        {
            m_bCurrentTestFailed = true;
        }

        auto xEnd = std::chrono::high_resolution_clock::now();
        auto fDuration = std::chrono::duration<float, std::milli>(xEnd - xStart).count();

        if (m_bCurrentTestFailed)
        {
            m_uFailedCount++;
            Zenith_Log(LOG_CATEGORY_UNITTEST, "[  FAILED  ] %s::%s (%.3fms)", xCase.m_strCategory, xCase.m_strName, fDuration);
        }
        else
        {
            m_uPassedCount++;
            Zenith_Log(LOG_CATEGORY_UNITTEST, "[  PASSED  ] %s::%s (%.3fms)", xCase.m_strCategory, xCase.m_strName, fDuration);
        }
    }

    Zenith_Log(LOG_CATEGORY_UNITTEST, "[==========] %u passed, %u failed, %u skipped",
        m_uPassedCount, m_uFailedCount, m_uSkippedCount);
}

void Zenith_TestRunner::HandleFailure(const char* strAssertionType, const char* strMsg, const char* strFile, int iLine)
{
    Zenith_Error(LOG_CATEGORY_UNITTEST, "%s at %s:%d: %s", strAssertionType, strFile, iLine, strMsg);
    Zenith_DebugBreak();
    m_bCurrentTestFailed = true;
}

void Zenith_TestRunner::AssertTrue(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...)
{
    if (!bExpr)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);
        HandleFailure("ZENITH_ASSERT_TRUE", acBuffer, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertFalse(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...)
{
    if (bExpr)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);
        HandleFailure("ZENITH_ASSERT_FALSE", acBuffer, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertEq(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (ulA != ulB)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%llu) != %s (%llu) - %s",
            strExprA, (unsigned long long)ulA, strExprB, (unsigned long long)ulB, acBuffer);
        HandleFailure("ZENITH_ASSERT_EQ", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertNe(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (ulA == ulB)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%llu) == %s (%llu) - %s",
            strExprA, (unsigned long long)ulA, strExprB, (unsigned long long)ulB, acBuffer);
        HandleFailure("ZENITH_ASSERT_NE", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertGt(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (ulA <= ulB)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%llu) <= %s (%llu) - %s",
            strExprA, (unsigned long long)ulA, strExprB, (unsigned long long)ulB, acBuffer);
        HandleFailure("ZENITH_ASSERT_GT", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertLt(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (ulA >= ulB)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%llu) >= %s (%llu) - %s",
            strExprA, (unsigned long long)ulA, strExprB, (unsigned long long)ulB, acBuffer);
        HandleFailure("ZENITH_ASSERT_LT", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertGe(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (ulA < ulB)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%llu) < %s (%llu) - %s",
            strExprA, (unsigned long long)ulA, strExprB, (unsigned long long)ulB, acBuffer);
        HandleFailure("ZENITH_ASSERT_GE", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertLe(u_int64 ulA, u_int64 ulB, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (ulA > ulB)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%llu) > %s (%llu) - %s",
            strExprA, (unsigned long long)ulA, strExprB, (unsigned long long)ulB, acBuffer);
        HandleFailure("ZENITH_ASSERT_LE", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertEq(float fA, float fB, float fEpsilon, const char* strExprA, const char* strExprB,
    const char* strFile, int iLine, const char* strMsg, ...)
{
    if (std::fabs(fA - fB) >= fEpsilon)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%f) != %s (%f) within %f - %s",
            strExprA, fA, strExprB, fB, fEpsilon, acBuffer);
        HandleFailure("ZENITH_ASSERT_EQ_FLOAT", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertNearVec3(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fEpsilon,
    const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strMsg, ...)
{
    if (Zenith_Maths::Abs(xA.x - xB.x) >= fEpsilon ||
        Zenith_Maths::Abs(xA.y - xB.y) >= fEpsilon ||
        Zenith_Maths::Abs(xA.z - xB.z) >= fEpsilon)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[1024];
        snprintf(acFullMsg, sizeof(acFullMsg), "%s (%.3f, %.3f, %.3f) != %s (%.3f, %.3f, %.3f) within %f - %s",
            strExprA, xA.x, xA.y, xA.z, strExprB, xB.x, xB.y, xB.z, fEpsilon, acBuffer);
        HandleFailure("ZENITH_ASSERT_NEAR_VEC3", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertNull(const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...)
{
    if (pPtr != nullptr)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);
        HandleFailure("ZENITH_ASSERT_NULL", acBuffer, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertNotNull(const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strMsg, ...)
{
    if (pPtr == nullptr)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);
        HandleFailure("ZENITH_ASSERT_NOT_NULL", acBuffer, strFile, iLine);
    }
}

void Zenith_TestRunner::AssertStrEq(const char* strA, const char* strB, const char* strFile, int iLine, const char* strMsg, ...)
{
    if (strcmp(strA, strB) != 0)
    {
        char acBuffer[512];
        va_list args;
        va_start(args, strMsg);
        vsnprintf(acBuffer, sizeof(acBuffer), strMsg, args);
        va_end(args);

        char acFullMsg[768];
        snprintf(acFullMsg, sizeof(acFullMsg), "\"%s\" != \"%s\" - %s", strA, strB, acBuffer);
        HandleFailure("ZENITH_ASSERT_STREQ", acFullMsg, strFile, iLine);
    }
}

void Zenith_TestRunner::Fail(const char* strMsg, const char* strFile, int iLine)
{
    HandleFailure("ZENITH_FAIL", strMsg, strFile, iLine);
}

void Zenith_TestRunner::Skip(const char* strReason)
{
    Zenith_Warning(LOG_CATEGORY_UNITTEST, "[  SKIPPED ] %s", strReason);
    m_uSkippedCount++;
    throw Zenith_TestSkippedException();
}

struct Zenith_TestSkippedException {};

#endif // ZENITH_TESTING
```

### Step 1.3: Update `Zenith.h`

**File:** `C:\dev\Zenith\Zenith\Core\Zenith.h`

**Add after line ~144 (after `ZENITH_ASSERT` block):**
```cpp
#define ZENITH_TESTING
```

**Add include at line ~41 (after other core includes):**
```cpp
#include "Core/Zenith_TestFramework.h"
```

---

## Phase 2: Migration

### Step 2.1: Migrate `Zenith_UnitTests.h`

**File:** `C:\dev\Zenith\Zenith\UnitTests\Zenith_UnitTests.h`

| Action | Description |
|--------|-------------|
| Delete | Entire `class Zenith_UnitTests { ... };` wrapper |
| Delete | `static void RunAllTests();` |
| Add | `#pragma once` |
| Add | `#include "Core/Zenith_TestFramework.h"` |
| Convert | Each `static void Test*()` → `ZENITH_TEST("Category", "TestName")` |
| Keep | `#ifdef ZENITH_WINDOWS` guards for Slang tests |
| Keep | `#ifdef ZENITH_TOOLS` include guards for Editor/Automation tests |

**Category Mapping:**

| Test Method Prefix | Category |
|--------------------|----------|
| `TestDataStream`, `TestVector`, `TestMemoryPool`, `TestCircularQueue` | `"Core"` |
| `TestScene`, `TestSceneHandle` | `"Scene"` |
| `TestComponent`, `TestEntity`, `TestQuery`, `TestLifecycle`, `TestEvent` | `"ECS"` |
| `TestBone`, `TestIK`, `TestAnimation`, `TestStateMachine`, `TestBlendTree`, `TestLayer`, `TestCrossFade` | `"Animation"` |
| `TestBlackboard`, `TestBT`, `TestNavMesh`, `TestNavAgent`, `TestSquad`, `TestPerception`, `TestTactical` | `"AI"` |
| `TestMeshAsset`, `TestSkeletonAsset`, `TestAssetHandle`, `TestDataAsset`, `TestModelInstance` | `"Asset"` |
| `TestTween`, `TestEasing` | `"Tween"` |
| `TestGizmos` | `"Gizmos"` (ZENITH_TOOLS) |
| `TestSlang` | `"Slang"` (ZENITH_WINDOWS) |
| `TestUIStyle` | `"UI"` |
| `TestImageView`, `TestDestroy` | `"Vulkan"` |
| `TestPrefab` | `"Prefab"` |
| `TestAsyncLoad` | `"Asset"` |
| `TestChunkDistance` | `"Terrain"` |

### Step 2.2: Migrate `Zenith_UnitTests.cpp`

**File:** `C:\dev\Zenith\Zenith\UnitTests\Zenith_UnitTests.cpp`

| Action | Description |
|--------|-------------|
| Delete | `#include "UnitTests/Zenith_UnitTests.h"` |
| Delete | `void Zenith_UnitTests::RunAllTests()` function entirely |
| Delete | `void Zenith_UnitTests::` class prefix from all methods |
| Replace | All `Zenith_Assert` calls with `ZENITH_ASSERT_*` macros |
| Remove | `Zenith_Log` calls (test runner handles output) |
| Keep | `#ifndef ZENITH_ANDROID` guards for file I/O tests |
| Keep | `#ifdef ZENITH_WINDOWS` guards for Slang tests |
| Keep | `#ifdef ZENITH_TOOLS` guards for Gizmos tests |

**Assertion Conversion Table:**

| Original | New |
|----------|-----|
| `Zenith_Assert(ptr != nullptr);` | `ZENITH_ASSERT_NOT_NULL(ptr);` |
| `Zenith_Assert(ptr == nullptr);` | `ZENITH_ASSERT_NULL(ptr);` |
| `Zenith_Assert(val == expected);` | `ZENITH_ASSERT_EQ(val, expected);` |
| `Zenith_Assert(val != expected);` | `ZENITH_ASSERT_NE(val, expected);` |
| `Zenith_Assert(a > b);` | `ZENITH_ASSERT_GT(a, b);` |
| `Zenith_Assert(a < b);` | `ZENITH_ASSERT_LT(a, b);` |
| `Zenith_Assert(a >= b);` | `ZENITH_ASSERT_GE(a, b);` |
| `Zenith_Assert(a <= b);` | `ZENITH_ASSERT_LE(a, b);` |
| `Zenith_Assert(!strcmp(s1, s2) == 0);` | `ZENITH_ASSERT_STREQ(s1, s2);` |
| `Zenith_Assert(fabs(f1 - f2) < eps);` | `ZENITH_ASSERT_EQ_FLOAT(f1, f2, eps);` |
| `Zenith_Assert(expr);` (bool) | `ZENITH_ASSERT_TRUE(expr);` |
| `Zenith_Assert(!expr);` (bool) | `ZENITH_ASSERT_FALSE(expr);` |
| `Zenith_Assert(VectorsEqual(v1, v2));` | `ZENITH_ASSERT_NEAR_VEC3(v1, v2, 0.001f);` |

**Example Conversion:**

```cpp
// BEFORE
void Zenith_UnitTests::TestDataStream()
{
    Zenith_DataStream xStream(1);
    xStream.WriteData(szTestData, uTestDataLen);
    xStream.SetCursor(0);
    Zenith_Assert(!strcmp(acTestData, szTestData), "Data mismatch");
    Zenith_Assert(u5 == 5, "u5 should be 5");
}

// AFTER
ZENITH_TEST(Core, DataStream)
{
    Zenith_DataStream xStream(1);
    xStream.WriteData(szTestData, uTestDataLen);
    xStream.SetCursor(0);
    ZENITH_ASSERT_STREQ(acTestData, szTestData, "Data mismatch");
    ZENITH_ASSERT_EQ(u5, 5u, "u5 should be 5");
}
```

### Step 2.3: Migrate `Zenith_SceneTests.h` and `.cpp`

**Files:**
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_SceneTests.h`
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_SceneTests.cpp`

| Action | Header | Implementation |
|--------|--------|----------------|
| Delete | `class Zenith_SceneTests { ... };` wrapper | `#include "UnitTests/Zenith_SceneTests.h"` |
| Delete | `static void RunAllTests();` | `void Zenith_SceneTests::RunAllTests()` |
| Delete | `void Zenith_SceneTests::` prefix from methods | Prefixes removed |
| Keep | Helper functions (`CreateTestSceneFile`, `CleanupTestSceneFile`, `PumpUntilComplete`) | Helper implementations unchanged |
| Convert | `static void Test*()` → `ZENITH_TEST("Scene", "TestName")` | Replace `Zenith_Assert` → `ZENITH_ASSERT_*` |
| Keep | Any platform guards | `#ifndef ZENITH_ANDROID` guards preserved |

### Step 2.4: Migrate `Zenith_PhysicsTests.h` and `.cpp`

**Files:**
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_PhysicsTests.h`
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_PhysicsTests.cpp`

**Changes:** Same pattern as other files.
- Delete class wrapper, convert to `ZENITH_TEST("Physics", "TestName")`
- Replace assertions
- Keep `ApproxEqual()` helper → `ZENITH_ASSERT_EQ_FLOAT(a, b, 0.01f)` in tests

### Step 2.5: Migrate `Zenith_AITests.h` and `.cpp`

**Files:**
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_AITests.h`
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_AITests.cpp`

**Changes:** Same pattern.
- `ZENITH_TEST("AI", "TestName")`

### Step 2.6: Migrate `Zenith_EditorTests.h` and `.cpp`

**Files:**
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_EditorTests.h`
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_EditorTests.cpp`

**Changes:**
- Wrap entire file content in `#ifdef ZENITH_TOOLS`
- `ZENITH_TEST("Editor", "TestName")`

**Header structure:**
```cpp
#ifdef ZENITH_TOOLS
#pragma once
#include "Core/Zenith_TestFramework.h"

ZENITH_TEST(Editor, PanelCreation) { /* ... */ }
ZENITH_TEST(Editor, ViewportResize) { /* ... */ }
// ... all tests

#endif
```

### Step 2.7: Migrate `Zenith_AutomationTests.h` and `.cpp`

**Files:**
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_AutomationTests.h`
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_AutomationTests.cpp`

**Changes:**
- Wrap entire file content in `#ifdef ZENITH_TOOLS`
- `ZENITH_TEST("Automation", "TestName")`

### Step 2.8: Handle `Zenith_EditorTestFixture.h` and `.cpp`

**Files:**
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_EditorTestFixture.h`
- `C:\dev\Zenith\Zenith\UnitTests\Zenith_EditorTestFixture.cpp`

**Changes:** Minor updates if needed - these are helper utilities used by editor tests.

---

## Phase 3: Integration

### Step 3.1: Update `Zenith_Main.cpp`

**File:** `C:\dev\Zenith\Zenith\Core\Zenith_Main.cpp`

**Current (line ~133):**
```cpp
void Zenith_Core::Zenith_Init()
{
    // ... initialization code ...
    
    Zenith_UnitTests::RunAllTests();
    
    // ... rest of initialization ...
}
```

**Change to:**
```cpp
void Zenith_Core::Zenith_Init()
{
    // ... initialization code ...
    
#ifdef ZENITH_TESTING
    Zenith_TestRunner::Instance().RunAllTests();
#endif
    
    // ... rest of initialization ...
}
```

---

## File Summary

| Phase | File | Change Type | Notes |
|-------|------|-------------|-------|
| **1. Framework** | `Zenith/Core/Zenith_TestFramework.h` | New | ~380 lines |
| **1. Framework** | `Zenith/Core/Zenith_TestFramework.cpp` | New | ~400 lines |
| **1. Framework** | `Zenith/Core/Zenith.h` | Modify | Add ZENITH_TESTING define + include |
| **2. Migration** | `Zenith/UnitTests/Zenith_UnitTests.h` | Rewrite | Remove class wrapper |
| **2. Migration** | `Zenith/UnitTests/Zenith_UnitTests.cpp` | Rewrite | ~8,500 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_SceneTests.h` | Rewrite | ~500 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_SceneTests.cpp` | Rewrite | ~9,000 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_PhysicsTests.h` | Rewrite | ~80 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_PhysicsTests.cpp` | Rewrite | ~1,000 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_AITests.h` | Rewrite | ~35 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_AITests.cpp` | Rewrite | ~2,200 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_EditorTests.h` | Rewrite (ifdef) | ~150 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_EditorTests.cpp` | Rewrite (ifdef) | ~1,500 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_AutomationTests.h` | Rewrite (ifdef) | ~130 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_AutomationTests.cpp` | Rewrite (ifdef) | ~2,200 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_EditorTestFixture.h` | Minor | ~60 lines |
| **2. Migration** | `Zenith/UnitTests/Zenith_EditorTestFixture.cpp` | Minor | ~140 lines |
| **3. Integration** | `Zenith/Core/Zenith_Main.cpp` | Modify | Add ZENITH_TESTING guard |

**Total:** 2 new files, 18 modified files

---

## Test Count Summary

| Test Class | Methods | Assertions |
|------------|---------|------------|
| Zenith_UnitTests | ~200 | ~2,725 |
| Zenith_SceneTests | ~200 | ~5,000 |
| Zenith_PhysicsTests | ~15 | ~200 |
| Zenith_AITests | ~5 | ~50 |
| Zenith_EditorTests | ~50 | ~300 |
| Zenith_AutomationTests | ~30 | ~150 |
| **Total** | **~500** | **~8,425** |

---

## Migration Order (Buildable)

1. **Phase 1** - Create `Zenith_TestFramework.h` and `.cpp`
2. **Build** - Verify framework compiles in isolation
3. **Phase 2** - Migrate one file at a time, build after each:
   - `Zenith_UnitTests.h/cpp` (largest, start here)
   - `Zenith_SceneTests.h/cpp` (second largest)
   - `Zenith_PhysicsTests.h/cpp`
   - `Zenith_AITests.h/cpp`
   - `Zenith_EditorTests.h/cpp`
   - `Zenith_AutomationTests.h/cpp`
   - `Zenith_EditorTestFixture.h/cpp`
4. **Phase 3** - Update `Zenith_Main.cpp` to use new runner
5. **Final Build** - Verify everything links and tests run

---

## Assertion Methods Summary

| Method | Purpose |
|--------|---------|
| `AssertTrue(bool, ...)` | Fails if expression is false |
| `AssertFalse(bool, ...)` | Fails if expression is true |
| `AssertEq(u_int64, u_int64, ...)` | Integer equality |
| `AssertEq(float, float, float, ...)` | Float equality with epsilon |
| `AssertNe(u_int64, u_int64, ...)` | Integer inequality |
| `AssertGt(u_int64, u_int64, ...)` | Greater than |
| `AssertLt(u_int64, u_int64, ...)` | Less than |
| `AssertGe(u_int64, u_int64, ...)` | Greater or equal |
| `AssertLe(u_int64, u_int64, ...)` | Less or equal |
| `AssertNull(const void*, ...)` | Null check |
| `AssertNotNull(const void*, ...)` | Non-null check |
| `AssertStrEq(const char*, const char*, ...)` | String equality |
| `AssertNearVec3(Vector3, Vector3, float, ...)` | Vector3 equality within epsilon |
| `Fail(const char*, ...)` | Explicit failure |
| `Skip(const char*)` | Skip test execution |

---

## Usage Example

```cpp
ZENITH_TEST(Core, VectorPushBack)
{
    Zenith_Vector<u_int> xVec(1);
    xVec.PushBack(42);
    
    ZENITH_ASSERT_EQ(xVec.GetSize(), 1u, "Size should be 1");
    ZENITH_ASSERT_EQ(xVec[0], 42u, "First element should be 42");
    ZENITH_ASSERT_NOT_NULL(&xVec, "Vector pointer should not be null");
}

ZENITH_TEST(Core, FloatComparison)
{
    float fA = 0.1f;
    float fB = 0.1f;
    ZENITH_ASSERT_EQ_FLOAT(fA, fB, 0.001f, "Floats should be equal within epsilon");
}

ZENITH_TEST(Core, Vector3Comparison)
{
    Zenith_Maths::Vector3 xA(1.0f, 2.0f, 3.0f);
    Zenith_Maths::Vector3 xB(1.0f, 2.0f, 3.0f);
    ZENITH_ASSERT_NEAR_VEC3(xA, xB, 0.001f, "Vectors should be equal within epsilon");
}

ZENITH_TEST(Core, StringComparison)
{
    const char* szA = "hello";
    const char* szB = "hello";
    ZENITH_ASSERT_STREQ(szA, szB, "Strings should match");
}

ZENITH_TEST(Core, SkipExample)
{
    if (featureNotImplemented)
    {
        ZENITH_SKIP("Feature not yet implemented");
    }
    // ... rest of test
}
```

---

## Key Differences from Original Plans

1. **No selective execution** - Only `RunAllTests()` (per your request)
2. **Simplified template assertions** - Uses `u_int64` cast approach (like Plan 1) for simplicity, but with Vec3 assertion added (from Plan 2)
3. **`ZENITH_TESTING` guard** - Enables conditional compilation of all test code
4. **Stub macros** - When `ZENITH_TESTING` is not defined, all macros become no-ops for non-test builds
5. **Hard break on failure** - `Zenith_DebugBreak()` pauses debugger on assertion failure