#include "Zenith.h"

// =============================================================================
// SentinelCore — leaf-proof executable for the Core cluster (ZenithCore +
// ZenithMaths + ZenithDataStream + ZenithFileAccess). See
// Build/Sharpmake_SentinelCore.cs for the full rationale.
//
// This exe links ONLY the four Core-cluster leaves. Its job is to drive each
// of them with a real, out-of-line call — never an empty main() — without
// naming g_xEngine, ZenithECS, Flux, Physics or AI. If any of the four
// secretly reached outside the cluster, this link would fail with an
// unresolved external.
// =============================================================================

#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

namespace
{
	int s_iFailures = 0;

	void Expect(bool bCondition, const char* szWhat)
	{
		if (!bCondition)
		{
			++s_iFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelCore FAIL: %s", szWhat);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelCore ok:   %s", szWhat);
		}
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelCore: Core-cluster leaf-proof starting "
		"(zenithcore.lib + zenithmaths.lib + zenithdatastream.lib + zenithfileaccess.lib only)");

	// 1. ZenithMaths — a real out-of-line free function (Zenith_Maths.cpp).
	float fRadians = Zenith_Maths::AuthoringRadians(180.0f);
	Expect(fRadians > 3.14f && fRadians < 3.15f, "AuthoringRadians(180) resolves from zenithmaths.lib");

	// 2. ZenithDataStream — construct, write, read back. The ctor allocates
	//    through Zenith_MemoryManagement (ZenithCore), and the templated
	//    operator<</operator>> are header-only, but Zenith_DataStream's own
	//    out-of-line members (constructor body, FailRead diagnostics on a
	//    corrupt read) still force zenithdatastream.lib to link.
	{
		Zenith_DataStream xStream;
		const u_int uValue = 0x5E2417u;
		const float fValue = 3.5f;
		xStream << uValue;
		xStream << fValue;

		xStream.SetCursor(0);

		u_int uReadBack = 0;
		float fReadBack = 0.0f;
		xStream >> uReadBack;
		xStream >> fReadBack;

		Expect(uReadBack == uValue, "Zenith_DataStream round-trips a u_int");
		Expect(fReadBack == fValue, "Zenith_DataStream round-trips a float");
	}

	// 3. ZenithFileAccess — a real call into the declared namespace. The
	//    actual implementation is platform-specific (Zenith/Windows/FileAccess/,
	//    owned by ZenithWindows, never linked here); sentinel_platform.cpp
	//    supplies the documented L0 stub, so this call resolves from THIS
	//    exe's own TU rather than from any engine platform layer.
	bool bExists = Zenith_FileAccess::FileExists("this_file_does_not_exist.bin");
	Expect(!bExists, "Zenith_FileAccess::FileExists resolves and returns the stubbed false");

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelCore: PASS (Core cluster links + runs with nothing above it)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelCore: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
