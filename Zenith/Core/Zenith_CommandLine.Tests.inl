#include "UnitTests/Zenith_UnitTests.h"
#include "Core/Zenith_CommandLine.h"

// ============================================================================
// ResolveUnderAssetsRoot tests (--assets-root relocatable-package override)
//
// Pure function: no engine or parse state touched, so no save/restore needed.
// Covers the two contract halves: no/empty override => the baked dir passes
// through UNCHANGED (including the deliberately-empty "" that FluxCompiler /
// hub / Android bake), and an override => "<root>/<rel>" with trailing root
// separators trimmed (run.bat passes "%~dp0", which ends in a backslash).
// Zenith_CommandLine::Parse is deliberately NOT re-parsed with fake argv here:
// that would clobber the parsed flag state (--automated-test, --assets-root, ...)
// for the rest of the batch — the override argument is injected directly instead.
// (The parser itself IS characterized below, through the pure ParseArgs entry
// point, which touches no process state.)
// ============================================================================

ZENITH_TEST(CommandLine, ResolveUnderRootNullOverride) { Zenith_UnitTests::TestCommandLineResolveUnderRootNullOverride(); }
void Zenith_UnitTests::TestCommandLineResolveUnderRootNullOverride()
{
	const std::string strResolved = Zenith_CommandLine::ResolveUnderAssetsRoot("C:/dev/Zenith/Games/Combat/Assets/", nullptr, "Games/Combat/Assets/");
	ZENITH_ASSERT_EQ(strResolved, std::string("C:/dev/Zenith/Games/Combat/Assets/"),
		"null override must pass the baked dir through unchanged");
}

ZENITH_TEST(CommandLine, ResolveUnderRootEmptyOverride) { Zenith_UnitTests::TestCommandLineResolveUnderRootEmptyOverride(); }
void Zenith_UnitTests::TestCommandLineResolveUnderRootEmptyOverride()
{
	const std::string strResolved = Zenith_CommandLine::ResolveUnderAssetsRoot("C:/baked/Assets/", "", "Games/X/Assets/");
	ZENITH_ASSERT_EQ(strResolved, std::string("C:/baked/Assets/"),
		"empty override must pass the baked dir through unchanged");
}

ZENITH_TEST(CommandLine, ResolveUnderRootJoinsUnderRoot) { Zenith_UnitTests::TestCommandLineResolveUnderRootJoinsUnderRoot(); }
void Zenith_UnitTests::TestCommandLineResolveUnderRootJoinsUnderRoot()
{
	const std::string strResolved = Zenith_CommandLine::ResolveUnderAssetsRoot("C:/baked/Assets/", "D:/pkg", "Games/Combat/Assets/");
	ZENITH_ASSERT_EQ(strResolved, std::string("D:/pkg/Games/Combat/Assets/"),
		"override must resolve the relative layout under the package root");
}

ZENITH_TEST(CommandLine, ResolveUnderRootTrimsRootSeparators) { Zenith_UnitTests::TestCommandLineResolveUnderRootTrimsRootSeparators(); }
void Zenith_UnitTests::TestCommandLineResolveUnderRootTrimsRootSeparators()
{
	const std::string strResolved = Zenith_CommandLine::ResolveUnderAssetsRoot("C:/baked/", "D:\\pkg\\", "Zenith/Assets/");
	ZENITH_ASSERT_EQ(strResolved, std::string("D:\\pkg/Zenith/Assets/"),
		"trailing root separators must be trimmed before the join (%~dp0 ends in a backslash)");
}

ZENITH_TEST(CommandLine, ResolveUnderRootEmptyBakedPassthrough) { Zenith_UnitTests::TestCommandLineResolveUnderRootEmptyBakedPassthrough(); }
void Zenith_UnitTests::TestCommandLineResolveUnderRootEmptyBakedPassthrough()
{
	const std::string strResolved = Zenith_CommandLine::ResolveUnderAssetsRoot("", nullptr, "Games/X/Assets/");
	ZENITH_ASSERT_EQ(strResolved, std::string(""),
		"baked \"\" (FluxCompiler/hub/Android) must stay \"\" when no override is given");
}

// ============================================================================
// ParseArgs tests (the pure parser)
//
// ParseArgs touches NO process state — it returns a Flags value — so unlike
// Parse it is safe to call with fake argv mid-batch. That is what these
// characterization tests exist for: pinning every flag's behaviour, including
// the three harness selection aliases whose loss would (per the parser's own
// warning) silently run an automated batch against PRODUCTION save data.
//
// argv[0] is the exe name and is never inspected, so every fixture below
// starts with a placeholder. ParseArgs takes char** (argv is not const in
// main), so the fixtures use mutable buffers rather than string literals.
// ============================================================================

namespace
{
	template<int N>
	Zenith_CommandLine::Flags ParseArgvFixture(char* (&apszArgv)[N])
	{
		return Zenith_CommandLine::ParseArgs(N, apszArgv);
	}
}

ZENITH_TEST(CommandLine, ParseDefaults) { Zenith_UnitTests::TestCommandLineParseDefaults(); }
void Zenith_UnitTests::TestCommandLineParseDefaults()
{
	char szExe[] = "zenith.exe";
	char* apszArgv[] = { szExe };
	const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);

	ZENITH_ASSERT_FALSE(xFlags.m_bAutomatedTestRun, "no flags must leave automated-test off");
	ZENITH_ASSERT_FALSE(xFlags.m_bNoImGuiIni, "no flags must leave the imgui ini enabled");
	ZENITH_ASSERT_FALSE(xFlags.m_bShaderDebugO0, "no flags must leave shader O0 off");
	ZENITH_ASSERT_NULL(xFlags.m_szScreenshotPath, "no flags must leave the screenshot path null");
	ZENITH_ASSERT_EQ(xFlags.m_uScreenshotFrame, 120u, "the screenshot frame must default to 120");
	ZENITH_ASSERT_NULL(xFlags.m_szAssetsRoot, "no flags must leave the assets root null");
	ZENITH_ASSERT_NULL(xFlags.m_szTestSaveRoot, "no flags must leave the test save root null");
	ZENITH_ASSERT_NULL(xFlags.m_szTestSaveRunId, "no flags must leave the test save run id null");
	ZENITH_ASSERT_NULL(xFlags.m_szBootProfileDump, "no flags must leave the boot profile dump null");
	ZENITH_ASSERT_FALSE(xFlags.m_bSkipBootCapture, "no flags must leave the boot capture on");
	ZENITH_ASSERT_NULL(xFlags.m_szUnitTestTimings, "no flags must leave the unit-test timings path null");
	ZENITH_ASSERT_FALSE(xFlags.m_bExitAfterUnitTests, "no flags must leave exit-after-unit-tests off");
}

ZENITH_TEST(CommandLine, ParseEveryBareFlag) { Zenith_UnitTests::TestCommandLineParseEveryBareFlag(); }
void Zenith_UnitTests::TestCommandLineParseEveryBareFlag()
{
	// All three harness selection aliases must set the SAME bit. A missing
	// alias here is exactly the failure the parser comment calls out.
	char szSelectionA[] = "--automated-test";
	char szSelectionB[] = "--automated-tests";
	char szSelectionC[] = "--all-automated-tests";
	char* apszSelectionAliases[] = { szSelectionA, szSelectionB, szSelectionC };
	for (char* szAlias : apszSelectionAliases)
	{
		char szExe[] = "zenith.exe";
		char* apszArgv[] = { szExe, szAlias };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_TRUE(xFlags.m_bAutomatedTestRun,
			"'%s' must select an automated-test run", szAlias);
	}

	char szExe[]      = "zenith.exe";
	char szNoIni[]    = "--no-imgui-ini";
	char szO0[]       = "--shader-debug-o0";
	char szSkipBoot[] = "--skip-boot-capture";
	char szExitUnit[] = "--exit-after-unit-tests";
	char* apszArgv[] = { szExe, szNoIni, szO0, szSkipBoot, szExitUnit };
	const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);

	ZENITH_ASSERT_TRUE(xFlags.m_bNoImGuiIni, "--no-imgui-ini must disable the imgui ini");
	ZENITH_ASSERT_TRUE(xFlags.m_bShaderDebugO0, "--shader-debug-o0 must enable shader O0");
	ZENITH_ASSERT_TRUE(xFlags.m_bSkipBootCapture, "--skip-boot-capture must skip the boot capture");
	ZENITH_ASSERT_TRUE(xFlags.m_bExitAfterUnitTests, "--exit-after-unit-tests must request the early exit");
	ZENITH_ASSERT_FALSE(xFlags.m_bAutomatedTestRun, "bare flags must not imply an automated-test run");
}

ZENITH_TEST(CommandLine, ParseEveryValueFlag) { Zenith_UnitTests::TestCommandLineParseEveryValueFlag(); }
void Zenith_UnitTests::TestCommandLineParseEveryValueFlag()
{
	char szExe[]         = "zenith.exe";
	char szShot[]        = "--screenshot";
	char szShotVal[]     = "C:/tmp/shot.tga";
	char szFrame[]       = "--screenshot-frame";
	char szFrameVal[]    = "37";
	char szAssets[]      = "--assets-root";
	char szAssetsVal[]   = "D:/pkg";
	char szSaveRoot[]    = "--test-save-root";
	char szSaveRootVal[] = "D:/artifacts/run";
	char szRunId[]       = "--test-save-run-id";
	char szRunIdVal[]    = "run-1234";
	char* apszArgv[] = { szExe, szShot, szShotVal, szFrame, szFrameVal,
		szAssets, szAssetsVal, szSaveRoot, szSaveRootVal, szRunId, szRunIdVal };
	const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);

	// Each captures the argv pointer itself (process-lifetime), not a copy.
	ZENITH_ASSERT_TRUE(xFlags.m_szScreenshotPath == szShotVal, "--screenshot must capture the following argv entry");
	ZENITH_ASSERT_EQ(xFlags.m_uScreenshotFrame, 37u, "--screenshot-frame must atoi the following argv entry");
	ZENITH_ASSERT_TRUE(xFlags.m_szAssetsRoot == szAssetsVal, "--assets-root must capture the following argv entry");
	ZENITH_ASSERT_TRUE(xFlags.m_szTestSaveRoot == szSaveRootVal, "--test-save-root must capture the following argv entry");
	ZENITH_ASSERT_TRUE(xFlags.m_szTestSaveRunId == szRunIdVal, "--test-save-run-id must capture the following argv entry");
}

ZENITH_TEST(CommandLine, ParsePrefixedFlags) { Zenith_UnitTests::TestCommandLineParsePrefixedFlags(); }
void Zenith_UnitTests::TestCommandLineParsePrefixedFlags()
{
	// Bare form -> default filename.
	{
		char szExe[]  = "zenith.exe";
		char szBoot[] = "--boot-profile-dump";
		char szUnit[] = "--unit-test-timings";
		char* apszArgv[] = { szExe, szBoot, szUnit };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_STREQ(xFlags.m_szBootProfileDump, "zenith_boot_profile_dump.txt",
			"the bare --boot-profile-dump form must use the default filename");
		ZENITH_ASSERT_STREQ(xFlags.m_szUnitTestTimings, "zenith_unit_test_timings.txt",
			"the bare --unit-test-timings form must use the default filename");
	}

	// "=path" form -> the text after the '='.
	{
		char szExe[]  = "zenith.exe";
		char szBoot[] = "--boot-profile-dump=D:/artifacts/boot.txt";
		char szUnit[] = "--unit-test-timings=D:/artifacts/units.txt";
		char* apszArgv[] = { szExe, szBoot, szUnit };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_STREQ(xFlags.m_szBootProfileDump, "D:/artifacts/boot.txt",
			"--boot-profile-dump=path must use the path after the '='");
		ZENITH_ASSERT_STREQ(xFlags.m_szUnitTestTimings, "D:/artifacts/units.txt",
			"--unit-test-timings=path must use the path after the '='");
	}

	// A trailing '=' with nothing after it is the bare form, not an empty path.
	{
		char szExe[]  = "zenith.exe";
		char szBoot[] = "--boot-profile-dump=";
		char* apszArgv[] = { szExe, szBoot };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_STREQ(xFlags.m_szBootProfileDump, "zenith_boot_profile_dump.txt",
			"a trailing '=' must fall back to the default filename, not an empty path");
	}

	// --skip-boot-capture must NOT be swallowed by the --boot-profile-dump
	// prefix entry that precedes it in the table.
	{
		char szExe[]  = "zenith.exe";
		char szSkip[] = "--skip-boot-capture";
		char* apszArgv[] = { szExe, szSkip };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_TRUE(xFlags.m_bSkipBootCapture, "--skip-boot-capture must still match its own spec");
		ZENITH_ASSERT_NULL(xFlags.m_szBootProfileDump,
			"--skip-boot-capture must not be captured by the --boot-profile-dump prefix");
	}
}

ZENITH_TEST(CommandLine, ParseArgvEdgeCases) { Zenith_UnitTests::TestCommandLineParseArgvEdgeCases(); }
void Zenith_UnitTests::TestCommandLineParseArgvEdgeCases()
{
	// Null argv (Android never parses) must return the defaults, not crash.
	const Zenith_CommandLine::Flags xNullArgv = Zenith_CommandLine::ParseArgs(4, nullptr);
	ZENITH_ASSERT_FALSE(xNullArgv.m_bAutomatedTestRun, "null argv must return the defaults");
	ZENITH_ASSERT_EQ(xNullArgv.m_uScreenshotFrame, 120u, "null argv must return the defaults");

	// argv[0] is never inspected: a flag-shaped exe name must not take effect.
	{
		char szFakeExe[] = "--automated-test";
		char* apszArgv[] = { szFakeExe };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_FALSE(xFlags.m_bAutomatedTestRun, "argv[0] must never be parsed as a flag");
	}

	// A null entry is skipped and an unknown argument ignored; parsing
	// continues past both.
	{
		char szExe[]     = "zenith.exe";
		char szUnknown[] = "--not-a-real-flag";
		char szNoIni[]   = "--no-imgui-ini";
		char* apszArgv[] = { szExe, nullptr, szUnknown, szNoIni };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_TRUE(xFlags.m_bNoImGuiIni, "parsing must continue past null and unknown entries");
	}

	// A value flag with nothing after it is ignored outright (the original
	// chain folded the "i + 1 < argc" guard into the match condition).
	{
		char szExe[]  = "zenith.exe";
		char szShot[] = "--screenshot";
		char* apszArgv[] = { szExe, szShot };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_NULL(xFlags.m_szScreenshotPath, "a trailing value flag with no value must be ignored");
	}

	// The value of a value flag is consumed, never re-parsed as a flag.
	{
		char szExe[]     = "zenith.exe";
		char szShot[]    = "--screenshot";
		char szShotVal[] = "--no-imgui-ini";
		char* apszArgv[] = { szExe, szShot, szShotVal };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_TRUE(xFlags.m_szScreenshotPath == szShotVal, "the value must be taken verbatim");
		ZENITH_ASSERT_FALSE(xFlags.m_bNoImGuiIni, "a consumed value must not be re-parsed as a flag");
	}
}
