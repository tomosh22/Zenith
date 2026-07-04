#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Slang/Flux_SpineLint.h"

// ============================================================================
// Flux_SpineLint unit tests (Flux Shader System Overhaul — D6, 5th gate).
//
// Pure text-lint coverage: poke detection, comment/string immunity, the
// Bindings.slang allowlist, accessor-call cleanliness, accurate line numbers,
// and multi-rule detection (poke / extension / block-redeclaration).
// ============================================================================

namespace
{
	u_int SpineLintScan(const char* szSrc, bool bBindings, Zenith_Vector<Flux_SpineLint::Violation>& axOut)
	{
		Flux_SpineLint::ScanSource("probe.slang", szSrc, bBindings, axOut);
		return axOut.GetSize();
	}
}

ZENITH_TEST(SpineLint, PokeDetected)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const u_int n = SpineLintScan("float4 f() { return g_xViewSet.g_xView.g_xViewProjMat[0]; }", false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "a raw g_xViewSet poke must be flagged");
	if (n == 1)
	{
		ZENITH_ASSERT_TRUE(ax.Get(0).m_eRule == Flux_SpineLint::RULE_SPINE_POKE, "must classify as a spine poke");
		ZENITH_ASSERT_TRUE(ax.Get(0).m_strDetail == "g_xViewSet", "detail must name the offending set");
	}
}

ZENITH_TEST(SpineLint, AllThreeSetsDetected)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const u_int n = SpineLintScan(
		"void f() { g_xGlobalSet.g_xGlobal; g_xViewSet.g_xView; g_xBindlessSet.g_axTextures[0]; }", false, ax);
	ZENITH_ASSERT_EQ(n, 3u, "each of the three spine sets must be flagged");
}

ZENITH_TEST(SpineLint, CommentImmunity)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const char* szSrc =
		"// g_xViewSet.g_xView is fine in a comment\n"
		"/* g_xGlobalSet.g_xGlobal too */\n"
		"float4 f() { return float4(0,0,0,0); }\n";
	const u_int n = SpineLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 0u, "spine tokens inside comments must NOT be flagged");
}

ZENITH_TEST(SpineLint, StringImmunity)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const u_int n = SpineLintScan("void f() { Log(\"g_xViewSet.g_xView\"); }", false, ax);
	ZENITH_ASSERT_EQ(n, 0u, "spine tokens inside a string literal must NOT be flagged");
}

ZENITH_TEST(SpineLint, BindingsFileAllowlisted)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const u_int n = SpineLintScan(
		"ParameterBlock<ViewParams> g_xViewSet;\nfloat4 GetVP() { return g_xViewSet.g_xView.g_xViewProjMat[0]; }",
		/*bBindings*/true, ax);
	ZENITH_ASSERT_EQ(n, 0u, "the spine home (Bindings.slang) is exempt from all rules");
}

ZENITH_TEST(SpineLint, AccessorFacadeIsClean)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const char* szSrc =
		"float4 Shade() {\n"
		"  float4x4 vp = GetViewProjMat();\n"
		"  float3 sun = GetSunDir();\n"
		"  return mul(vp, float4(sun, 1.0));\n"
		"}\n";
	const u_int n = SpineLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 0u, "post-migration accessor-facade calls must be clean");
}

ZENITH_TEST(SpineLint, LineNumberAccurate)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const char* szSrc =
		"// line 1\n"
		"float4 f()\n"
		"{ return g_xViewSet.g_xView.g_xCamPos_Pad; }\n";   // line 3
	const u_int n = SpineLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "one poke");
	if (n == 1) ZENITH_ASSERT_EQ(ax.Get(0).m_uLine, 3u, "poke must be reported on line 3");
}

ZENITH_TEST(SpineLint, ExtensionRuleDetected)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const u_int n = SpineLintScan("extension ViewParams { float4 Pierce() { return g_xView.g_xViewProjMat[0]; } }", false, ax);
	ZENITH_ASSERT_TRUE(n >= 1u, "an `extension ViewParams` outside Bindings.slang must be flagged");
	bool bFound = false;
	for (u_int i = 0; i < ax.GetSize(); i++) if (ax.Get(i).m_eRule == Flux_SpineLint::RULE_SPINE_EXTENSION) bFound = true;
	ZENITH_ASSERT_TRUE(bFound, "must classify the extension rule");
}

ZENITH_TEST(SpineLint, BlockRedeclDetected)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const u_int n = SpineLintScan("ParameterBlock<GlobalParams> g_xRogueSet;", false, ax);
	ZENITH_ASSERT_TRUE(n >= 1u, "a rogue ParameterBlock<GlobalParams> must be flagged");
	bool bFound = false;
	for (u_int i = 0; i < ax.GetSize(); i++) if (ax.Get(i).m_eRule == Flux_SpineLint::RULE_SPINE_BLOCK_REDECL) bFound = true;
	ZENITH_ASSERT_TRUE(bFound, "must classify the block-redeclaration rule");
}

ZENITH_TEST(SpineLint, MultiViolationAccumulates)
{
	Zenith_Vector<Flux_SpineLint::Violation> ax;
	const char* szSrc =
		"ParameterBlock<ViewParams> g_xRogue;\n"       // block redecl
		"extension GlobalParams { }\n"                  // extension
		"float4 f() { return g_xBindlessSet.g_axTextures[0].Load(0); }\n";  // poke
	const u_int n = SpineLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 3u, "three distinct violations must all accumulate");
}

ZENITH_TEST(SpineLint, IsBindingsFilePathMatching)
{
	ZENITH_ASSERT_TRUE(Flux_SpineLint::IsBindingsFile("C:/dev/Zenith/Zenith/Flux/Shaders/Common/Bindings.slang"),
		"absolute Windows path to the spine home must match");
	ZENITH_ASSERT_TRUE(Flux_SpineLint::IsBindingsFile("Common/Bindings.slang"), "relative path must match");
	ZENITH_ASSERT_FALSE(Flux_SpineLint::IsBindingsFile("Common/Velocity.slang"), "a sibling module must NOT match");
	ZENITH_ASSERT_FALSE(Flux_SpineLint::IsBindingsFile("DeferredShading/Flux_DeferredShading.slang"),
		"a feature shader must NOT match");
}
