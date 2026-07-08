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
// that would clobber --headless state for the rest of the batch (documented
// hazard) — the override argument is injected directly instead.
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
