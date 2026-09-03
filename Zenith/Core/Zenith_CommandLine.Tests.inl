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

ZENITH_TEST(CommandLine, ParseWindowSize) { Zenith_UnitTests::TestCommandLineParseWindowSize(); }
void Zenith_UnitTests::TestCommandLineParseWindowSize()
{
	u_int uWidth = 0u;
	u_int uHeight = 0u;
	ZENITH_ASSERT_TRUE(Zenith_CommandLine::ParseWindowSizeArg("3840x2160", uWidth, uHeight), "WxH must parse");
	ZENITH_ASSERT_EQ(uWidth, 3840u, "width is the number before the separator");
	ZENITH_ASSERT_EQ(uHeight, 2160u, "height is the number after the separator");
	ZENITH_ASSERT_TRUE(Zenith_CommandLine::ParseWindowSizeArg("1280X720", uWidth, uHeight), "an upper-case X separator must parse");
	ZENITH_ASSERT_EQ(uWidth, 1280u, "upper-case X: width");
	ZENITH_ASSERT_EQ(uHeight, 720u, "upper-case X: height");

	// Malformed values must NOT set the override: a typo must fall back to the
	// game's own window size rather than create a 0-wide swapchain.
	uWidth = 7u; uHeight = 7u;
	ZENITH_ASSERT_TRUE(!Zenith_CommandLine::ParseWindowSizeArg("1920", uWidth, uHeight), "a missing separator is rejected");
	ZENITH_ASSERT_TRUE(!Zenith_CommandLine::ParseWindowSizeArg("0x720", uWidth, uHeight), "a zero width is rejected");
	ZENITH_ASSERT_TRUE(!Zenith_CommandLine::ParseWindowSizeArg("1920x", uWidth, uHeight), "a missing height is rejected");
	ZENITH_ASSERT_TRUE(!Zenith_CommandLine::ParseWindowSizeArg("1920x1080p", uWidth, uHeight), "trailing junk is rejected");
	ZENITH_ASSERT_TRUE(!Zenith_CommandLine::ParseWindowSizeArg("99999x10", uWidth, uHeight), "an absurd dimension is rejected");
	ZENITH_ASSERT_TRUE(!Zenith_CommandLine::ParseWindowSizeArg(nullptr, uWidth, uHeight), "null is rejected");
	ZENITH_ASSERT_EQ(uWidth, 7u, "a rejected value leaves the outputs untouched");
	ZENITH_ASSERT_EQ(uHeight, 7u, "a rejected value leaves the outputs untouched");

	// Through the real table: the value flag consumes the next argv entry, and a
	// malformed one leaves Flags at its no-override default.
	char szExe[]      = "zenith.exe";
	char szFlag[]     = "--window-size";
	char szGood[]     = "2560x1440";
	char* apszGood[]  = { szExe, szFlag, szGood };
	const Zenith_CommandLine::Flags xGood = ParseArgvFixture(apszGood);
	ZENITH_ASSERT_EQ(xGood.m_uWindowWidth, 2560u, "--window-size sets the width");
	ZENITH_ASSERT_EQ(xGood.m_uWindowHeight, 1440u, "--window-size sets the height");
	char szBad[]      = "wide";
	char* apszBad[]   = { szExe, szFlag, szBad };
	const Zenith_CommandLine::Flags xBad = ParseArgvFixture(apszBad);
	ZENITH_ASSERT_EQ(xBad.m_uWindowWidth, 0u, "a malformed --window-size leaves no override");
	ZENITH_ASSERT_EQ(xBad.m_uWindowHeight, 0u, "a malformed --window-size leaves no override");
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

// ============================================================================
// --indirect-count-mode=auto|native|padded|single (Phase 1 of the terrain
// indirect-count compatibility plan) — pure CLI parser/enum coverage.
//
// ParseArgs touches NO process state — only the pure Flags value — so these
// fixtures can drive the parser with arbitrary argv exactly like the existing
// CommandLine* tests above them. The default is Auto (the shipping mode); an
// unknown spelling and a bare `--indirect-count-mode` (no '=') both fall
// through to Auto via ResolveIndirectCountModeArg so a malformed CLI never
// silently flips the shipping mode. The CLI converter in Vulkan translates
// the enum into Flux_IndirectDrawOverride at device init; this test pins the
// parser vocabulary the engine reads from Zenith_CommandLine::GetIndirectCount-
// Mode, which is what every backend's init ultimately reads.
// ============================================================================
ZENITH_TEST(CommandLine, ParseIndirectCountModeDefaultsToAuto) { Zenith_UnitTests::TestCommandLineParseIndirectCountModeDefaultsToAuto(); }
void Zenith_UnitTests::TestCommandLineParseIndirectCountModeDefaultsToAuto()
{
	char szExe[] = "zenith.exe";
	char* apszArgv[] = { szExe };
	const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xFlags.m_eIndirectCountMode),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Auto),
		"no --indirect-count-mode must leave Auto (the shipping default) on the Flags value");
}

ZENITH_TEST(CommandLine, ParseIndirectCountModeEverySpelling) { Zenith_UnitTests::TestCommandLineParseIndirectCountModeEverySpelling(); }
void Zenith_UnitTests::TestCommandLineParseIndirectCountModeEverySpelling()
{
	struct Spelling { const char* m_szArg; Zenith_IndirectCountMode m_eExpected; };
	const Spelling axSpellings[] = {
		{ "--indirect-count-mode=auto",   Zenith_IndirectCountMode::Auto   },
		{ "--indirect-count-mode=native", Zenith_IndirectCountMode::Native },
		{ "--indirect-count-mode=padded", Zenith_IndirectCountMode::Padded },
		{ "--indirect-count-mode=single", Zenith_IndirectCountMode::Single },
	};
	for (const Spelling& xS : axSpellings)
	{
		char szExe[] = "zenith.exe";
		// argv contains mutable char* (ParseArgs takes char**), so stage each
		// spelling in a writable buffer the parser can read past.
		char szArg[64];
		std::snprintf(szArg, sizeof(szArg), "%s", xS.m_szArg);
		char* apszArgv[] = { szExe, szArg };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_EQ(static_cast<uint32_t>(xFlags.m_eIndirectCountMode),
			static_cast<uint32_t>(xS.m_eExpected),
			"'%s' must select %u (got %u) — the four spellings are the entire vocabulary the CLI exposes",
			xS.m_szArg, static_cast<uint32_t>(xS.m_eExpected), static_cast<uint32_t>(xFlags.m_eIndirectCountMode));
	}
}

ZENITH_TEST(CommandLine, ParseIndirectCountModeUnknownFallsBackToAuto) { Zenith_UnitTests::TestCommandLineParseIndirectCountModeUnknownFallsBackToAuto(); }
void Zenith_UnitTests::TestCommandLineParseIndirectCountModeUnknownFallsBackToAuto()
{
	// Unknown spelling -> Auto, not a slanted native/padded/single value.
	{
		char szExe[]   = "zenith.exe";
		char szArg[]   = "--indirect-count-mode=illegal";
		char* apszArgv[] = { szExe, szArg };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_EQ(static_cast<uint32_t>(xFlags.m_eIndirectCountMode),
			static_cast<uint32_t>(Zenith_IndirectCountMode::Auto),
			"an unknown spelling must fall through to Auto, NOT silently take a partial value");
	}

	// A trailing '=' with no value -> Auto (matches the bare \0 default).
	{
		char szExe[]   = "zenith.exe";
		char szArg[]   = "--indirect-count-mode=";
		char* apszArgv[] = { szExe, szArg };
		const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
		ZENITH_ASSERT_EQ(static_cast<uint32_t>(xFlags.m_eIndirectCountMode),
			static_cast<uint32_t>(Zenith_IndirectCountMode::Auto),
			"a bare trailing '=' must fall through to Auto — the default filename analogy");
	}
}

ZENITH_TEST(CommandLine, ParseIndirectCountModeBareFormFallsThroughToAuto) { Zenith_UnitTests::TestCommandLineParseIndirectCountModeBareFormFallsThroughToAuto(); }
void Zenith_UnitTests::TestCommandLineParseIndirectCountModeBareFormFallsThroughToAuto()
{
	// A bare `--indirect-count-mode` with no '=' pair must NOT swallow the
	// following argv and must leave Auto. The matching logic in the parser
	// treats this as a Prefixed spec; the bare form matches the spec via the
	// prefix check, ResolveIndirectCountModeArg sees a nullptr value, and
	// the default is Auto. Without this clause a test batch could flip the
	// engine into a forced mode just by mis-typing the flag.
	char szExe[]   = "zenith.exe";
	char szBare[]  = "--indirect-count-mode";
	char szNoImg[] = "--no-imgui-ini";
	char* apszArgv[] = { szExe, szBare, szNoImg };
	const Zenith_CommandLine::Flags xFlags = ParseArgvFixture(apszArgv);
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xFlags.m_eIndirectCountMode),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Auto),
		"bare --indirect-count-mode must fall through to Auto (no value after '=')");
	ZENITH_ASSERT_TRUE(xFlags.m_bNoImGuiIni,
		"the bare form must not consume the following argv as a value");
}

ZENITH_TEST(CommandLine, ResolveIndirectCountModeArgPure) { Zenith_UnitTests::TestCommandLineResolveIndirectCountModeArgPure(); }
void Zenith_UnitTests::TestCommandLineResolveIndirectCountModeArgPure()
{
	// The pure splitter is exposed so the parsing contract is testable without
	// re-running Parse. It must name every spelling, return the default on
	// null/empty, and reject an unknown spelling back to the default.
	using Zenith_CommandLine::ResolveIndirectCountModeArg;
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg("auto",   Zenith_IndirectCountMode::Native)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Auto),   "auto must resolve regardless of default");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg("native", Zenith_IndirectCountMode::Auto)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Native), "native must resolve");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg("padded", Zenith_IndirectCountMode::Auto)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Padded), "padded must resolve");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg("single", Zenith_IndirectCountMode::Auto)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Single), "single must resolve");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg(nullptr,   Zenith_IndirectCountMode::Native)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Native), "nullptr must fall back to the supplied default");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg("",        Zenith_IndirectCountMode::Padded)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Padded), "empty must fall back to the supplied default");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(ResolveIndirectCountModeArg("garbage", Zenith_IndirectCountMode::Single)),
		static_cast<uint32_t>(Zenith_IndirectCountMode::Single), "unknown must fall back to the supplied default, NOT Auto");
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
