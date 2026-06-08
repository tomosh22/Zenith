#pragma once

#ifdef ZENITH_TESTING

struct Zenith_TestCase
{
	const char* m_strCategory;
	const char* m_strName;
	void (*m_pfnTest)();
	Zenith_TestCase* m_pxNext;
};

// One captured assertion failure. Collected silently during the run and
// printed by the final summary.
struct Zenith_TestFailure
{
	const char* m_strCategory;   // pointer into static Zenith_TestCase data
	const char* m_strName;
	const char* m_strFile;
	int         m_iLine;
	char        m_acMsg[768];    // fully-formatted diagnostic
	Zenith_TestFailure* m_pxNext;
};

class Zenith_TestRunner
{
public:
	static Zenith_TestRunner& Instance();

	void RegisterTest(Zenith_TestCase* pxCase);
	void RunAllTests();

	// Per-test global-state reset seam. The runner lives in Core (L0) and must
	// not name the ECS (L1) directly; a higher layer installs the concrete reset
	// (Zenith_SceneSystem::ResetForNextTest) via a captureless function pointer
	// (NO std::function), mirroring the Zenith_ECSRuntimeHooks / registrar
	// inversion. Null => no reset is run.
	static void SetResetHook(void (*pfnReset)()) { m_pfnResetHook = pfnReset; }
	static void (*GetResetHook())()              { return m_pfnResetHook; }

	// Non-template assertions - each takes printf-style (const char* strFormat, ...) at the end.
	void AssertTrue    (bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...);
	void AssertFalse   (bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...);
	void AssertNull    (const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...);
	void AssertNotNull (const void* pPtr, const char* strExpr, const char* strFile, int iLine, const char* strFormat, ...);
	void AssertStrEq   (const char* strA, const char* strB, const char* strFile, int iLine, const char* strFormat, ...);
	void AssertEqFloat (float fA, float fB, float fEpsilon, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strFormat, ...);
	void AssertNearVec3(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fEpsilon, const char* strExprA, const char* strExprB, const char* strFile, int iLine, const char* strFormat, ...);

	void Fail(const char* strMsg, const char* strFile, int iLine);
	void Skip(const char* strReason);

	// HandleFailure is public so the template assertions (below) can call it.
	void HandleFailure(const char* strType, const char* strMsg, const char* strFile, int iLine);

	bool CurrentTestFailed()  const { return m_bCurrentTestFailed;  }
	bool CurrentTestSkipped() const { return m_bCurrentTestSkipped; }

	u_int GetPassedCount()  const { return m_uPassedCount;  }
	u_int GetFailedCount()  const { return m_uFailedCount;  }
	u_int GetSkippedCount() const { return m_uSkippedCount; }
	u_int GetTestCount()    const { return m_uTestCount;    }

private:
	Zenith_TestRunner() = default;
	Zenith_TestRunner(const Zenith_TestRunner&) = delete;
	Zenith_TestRunner& operator=(const Zenith_TestRunner&) = delete;

	Zenith_TestCase*    m_pxFirstTest      = nullptr;
	Zenith_TestCase*    m_pxCurrentCase    = nullptr;  // set while a test is executing
	Zenith_TestFailure* m_pxFirstFailure   = nullptr;  // linked list of captured failures
	u_int               m_uTestCount       = 0;
	u_int               m_uPassedCount     = 0;
	u_int               m_uFailedCount     = 0;
	u_int               m_uSkippedCount    = 0;
	bool                m_bCurrentTestFailed  = false;
	bool                m_bCurrentTestSkipped = false;

	// Installed by a higher layer (EngineComposition TU) via SetResetHook.
	static inline void (*m_pfnResetHook)() = nullptr;
};

struct Zenith_TestRegistrar
{
	Zenith_TestRegistrar(Zenith_TestCase* pxCase)
	{
		Zenith_TestRunner::Instance().RegisterTest(pxCase);
	}
};

// --- Template value formatter (header-only) -------------------------------
// Prints a value into a buffer for diagnostic output. Handles primitives
// (arithmetic, pointers, C-strings) automatically; falls back to "<value>"
// for user-defined types without a known formatter. If a user type wants
// better output, it can be formatted by the caller via the user-message
// printf args.
template<typename T>
inline void Zenith_FormatTestValue(char* acBuf, size_t uSize, const T& xVal)
{
	using Decayed = std::decay_t<T>;
	if constexpr (std::is_same_v<Decayed, bool>)
	{
		snprintf(acBuf, uSize, "%s", xVal ? "true" : "false");
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		snprintf(acBuf, uSize, "%f", static_cast<double>(xVal));
	}
	else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
	{
		snprintf(acBuf, uSize, "%lld", static_cast<long long>(xVal));
	}
	else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
	{
		snprintf(acBuf, uSize, "%llu", static_cast<unsigned long long>(xVal));
	}
	else if constexpr (std::is_same_v<Decayed, const char*> || std::is_same_v<Decayed, char*>)
	{
		snprintf(acBuf, uSize, "\"%s\"", xVal ? xVal : "(null)");
	}
	else if constexpr (std::is_same_v<Decayed, std::string> || std::is_same_v<Decayed, std::string_view>)
	{
		snprintf(acBuf, uSize, "\"%.*s\"",
			static_cast<int>(xVal.size()), xVal.data());
	}
	else if constexpr (std::is_pointer_v<Decayed>)
	{
		snprintf(acBuf, uSize, "%p", static_cast<const void*>(xVal));
	}
	else if constexpr (std::is_enum_v<T>)
	{
		snprintf(acBuf, uSize, "%lld",
			static_cast<long long>(static_cast<std::underlying_type_t<T>>(xVal)));
	}
	else
	{
		snprintf(acBuf, uSize, "<value>");
	}
}

// --- Template comparison assertion plumbing -------------------------------

namespace Zenith_TestAssert_Detail
{
	inline void FormatUserMsg(char* acOut, size_t uSize, const char* strFormat, va_list args)
	{
		if (strFormat == nullptr || strFormat[0] == '\0')  acOut[0] = '\0';
		else                                               vsnprintf(acOut, uSize, strFormat, args);
	}

	inline void ReportCompareFailure(const char* strType,
		const char* strExprA, const char* strExprB,
		const char* strOp, const char* strValA, const char* strValB,
		const char* strUserMsg, const char* strFile, int iLine)
	{
		char acFull[1280];
		snprintf(acFull, sizeof(acFull), "%s (%s) %s %s (%s)%s%s",
			strExprA, strValA, strOp, strExprB, strValB,
			strUserMsg[0] ? " - " : "", strUserMsg);
		Zenith_TestRunner::Instance().HandleFailure(strType, acFull, strFile, iLine);
	}

	// Shared failure-reporting tail for all comparison asserts. Each
	// Zenith_TestAssertXX wrapper does its own comparison and, on failure,
	// forwards operand values + assert metadata here. The va_list is
	// constructed by the caller so the variadic format args reach this helper.
	template<typename T, typename U>
	inline void EmitCompareFailure(const T& xA, const U& xB,
		const char* strAssertName, const char* strNegatedOp,
		const char* strExprA, const char* strExprB,
		const char* strFile, int iLine,
		const char* strFormat, va_list args)
	{
		char acUser[512];
		FormatUserMsg(acUser, sizeof(acUser), strFormat, args);
		char acValA[128], acValB[128];
		Zenith_FormatTestValue(acValA, sizeof(acValA), xA);
		Zenith_FormatTestValue(acValB, sizeof(acValB), xB);
		ReportCompareFailure(
			strAssertName, strExprA, strExprB, strNegatedOp, acValA, acValB,
			acUser, strFile, iLine);
	}
}

// Template comparison assertions. Work for any types (T, U) where the
// relevant operator is valid. On failure, format both operand values via
// Zenith_FormatTestValue and forward the user's printf-style message.

// Cross-type comparisons are expected in user test code (e.g. comparing a
// u_int counter against a literal 0). Silence signed/unsigned warnings for
// the whole block.
#pragma warning(push)
#pragma warning(disable: 4018 4389)

template<typename T, typename U>
inline void Zenith_TestAssertEq(const T& xA, const U& xB,
	const char* strExprA, const char* strExprB,
	const char* strFile, int iLine, const char* strFormat, ...)
{
	if (!(xA == xB))
	{
		va_list args; va_start(args, strFormat);
		Zenith_TestAssert_Detail::EmitCompareFailure(xA, xB,
			"ZENITH_ASSERT_EQ", "!=", strExprA, strExprB, strFile, iLine, strFormat, args);
		va_end(args);
	}
}

template<typename T, typename U>
inline void Zenith_TestAssertNe(const T& xA, const U& xB,
	const char* strExprA, const char* strExprB,
	const char* strFile, int iLine, const char* strFormat, ...)
{
	if (xA == xB)
	{
		va_list args; va_start(args, strFormat);
		Zenith_TestAssert_Detail::EmitCompareFailure(xA, xB,
			"ZENITH_ASSERT_NE", "==", strExprA, strExprB, strFile, iLine, strFormat, args);
		va_end(args);
	}
}

template<typename T, typename U>
inline void Zenith_TestAssertGt(const T& xA, const U& xB,
	const char* strExprA, const char* strExprB,
	const char* strFile, int iLine, const char* strFormat, ...)
{
	if (!(xA > xB))
	{
		va_list args; va_start(args, strFormat);
		Zenith_TestAssert_Detail::EmitCompareFailure(xA, xB,
			"ZENITH_ASSERT_GT", "<=", strExprA, strExprB, strFile, iLine, strFormat, args);
		va_end(args);
	}
}

template<typename T, typename U>
inline void Zenith_TestAssertLt(const T& xA, const U& xB,
	const char* strExprA, const char* strExprB,
	const char* strFile, int iLine, const char* strFormat, ...)
{
	if (!(xA < xB))
	{
		va_list args; va_start(args, strFormat);
		Zenith_TestAssert_Detail::EmitCompareFailure(xA, xB,
			"ZENITH_ASSERT_LT", ">=", strExprA, strExprB, strFile, iLine, strFormat, args);
		va_end(args);
	}
}

template<typename T, typename U>
inline void Zenith_TestAssertGe(const T& xA, const U& xB,
	const char* strExprA, const char* strExprB,
	const char* strFile, int iLine, const char* strFormat, ...)
{
	if (!(xA >= xB))
	{
		va_list args; va_start(args, strFormat);
		Zenith_TestAssert_Detail::EmitCompareFailure(xA, xB,
			"ZENITH_ASSERT_GE", "<", strExprA, strExprB, strFile, iLine, strFormat, args);
		va_end(args);
	}
}

template<typename T, typename U>
inline void Zenith_TestAssertLe(const T& xA, const U& xB,
	const char* strExprA, const char* strExprB,
	const char* strFile, int iLine, const char* strFormat, ...)
{
	if (!(xA <= xB))
	{
		va_list args; va_start(args, strFormat);
		Zenith_TestAssert_Detail::EmitCompareFailure(xA, xB,
			"ZENITH_ASSERT_LE", ">", strExprA, strExprB, strFile, iLine, strFormat, args);
		va_end(args);
	}
}

#pragma warning(pop)  // restore C4018 / C4389

//Tests will only be linked and run if they are included in a .cpp that is used in the
//current game project, if the game .exe does not pull in the .obj the tests will not exist
#define ZENITH_TEST(category, name)                                                   \
	static void Zenith_Test_##category##_##name();                                    \
	static Zenith_TestCase g_xZenithTestCase_##category##_##name = {                  \
		#category, #name, &Zenith_Test_##category##_##name, nullptr                   \
	};                                                                                \
	static Zenith_TestRegistrar g_xZenithTestReg_##category##_##name(                 \
		&g_xZenithTestCase_##category##_##name);                                      \
	static void Zenith_Test_##category##_##name()

// Leading "" absorbs the empty __VA_ARGS__ case on MSVC without /Zc:preprocessor.
#define ZENITH_ASSERT_TRUE(expr, ...)      Zenith_TestRunner::Instance().AssertTrue    ((expr), #expr, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_FALSE(expr, ...)     Zenith_TestRunner::Instance().AssertFalse   ((expr), #expr, __FILE__, __LINE__, "" __VA_ARGS__)
// reinterpret_cast (not static_cast): tests pass FUNCTION pointers here too (e.g.
// component-meta lifecycle hooks like m_pfnOnDestroy). static_cast<void*>(fnptr)
// is ill-formed and clang rejects it under -Werror (Android/agde); the
// function->object pointer conversion is conditionally-supported via
// reinterpret_cast and is clean under the agde flags (no -Wpedantic). Identical
// to static_cast for ordinary object pointers.
#define ZENITH_ASSERT_NULL(ptr, ...)       Zenith_TestRunner::Instance().AssertNull    (reinterpret_cast<const void*>(ptr), #ptr, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_NOT_NULL(ptr, ...)   Zenith_TestRunner::Instance().AssertNotNull (reinterpret_cast<const void*>(ptr), #ptr, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_STREQ(a, b, ...)     Zenith_TestRunner::Instance().AssertStrEq   ((a), (b), __FILE__, __LINE__, "" __VA_ARGS__)
// Template comparisons - work with any type pair supporting the relevant operator.
#define ZENITH_ASSERT_EQ(a, b, ...)        Zenith_TestAssertEq((a), (b), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_NE(a, b, ...)        Zenith_TestAssertNe((a), (b), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_GT(a, b, ...)        Zenith_TestAssertGt((a), (b), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_LT(a, b, ...)        Zenith_TestAssertLt((a), (b), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_GE(a, b, ...)        Zenith_TestAssertGe((a), (b), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_LE(a, b, ...)        Zenith_TestAssertLe((a), (b), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_EQ_FLOAT(a, b, e, ...) Zenith_TestRunner::Instance().AssertEqFloat (static_cast<float>(a), static_cast<float>(b), static_cast<float>(e), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)
#define ZENITH_ASSERT_NEAR_VEC3(a, b, e, ...) Zenith_TestRunner::Instance().AssertNearVec3((a), (b), static_cast<float>(e), #a, #b, __FILE__, __LINE__, "" __VA_ARGS__)

#define ZENITH_SKIP(reason) do { Zenith_TestRunner::Instance().Skip(reason); return; } while (0)
#define ZENITH_FAIL(msg)    Zenith_TestRunner::Instance().Fail((msg), __FILE__, __LINE__)

#else // ZENITH_TESTING

#define ZENITH_TEST(category, name)      static void Zenith_Test_Disabled_##category##_##name()
#define ZENITH_ASSERT_TRUE(...)          ((void)0)
#define ZENITH_ASSERT_FALSE(...)         ((void)0)
#define ZENITH_ASSERT_NULL(...)          ((void)0)
#define ZENITH_ASSERT_NOT_NULL(...)      ((void)0)
#define ZENITH_ASSERT_STREQ(...)         ((void)0)
#define ZENITH_ASSERT_EQ(...)            ((void)0)
#define ZENITH_ASSERT_NE(...)            ((void)0)
#define ZENITH_ASSERT_GT(...)            ((void)0)
#define ZENITH_ASSERT_LT(...)            ((void)0)
#define ZENITH_ASSERT_GE(...)            ((void)0)
#define ZENITH_ASSERT_LE(...)            ((void)0)
#define ZENITH_ASSERT_EQ_FLOAT(...)      ((void)0)
#define ZENITH_ASSERT_NEAR_VEC3(...)     ((void)0)
#define ZENITH_SKIP(reason)              ((void)0)
#define ZENITH_FAIL(msg)                 ((void)0)

#endif // ZENITH_TESTING
