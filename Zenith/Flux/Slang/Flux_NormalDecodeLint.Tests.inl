#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Slang/Flux_NormalDecodeLint.h"

// ============================================================================
// Flux_NormalDecodeLint unit tests (Photorealism.md §4).
//
// Pure text-lint coverage: the genuine open-coded shape is caught outside
// Common/Material.slang, the one legitimate decode site is exempt, a shader
// that calls the shared helper is clean, the false-positive class (UV -> NDC
// remaps with no texture sample) stays clean, and comment/string mentions are
// immune — mirroring Flux_SpineLint.Tests.inl's coverage shape.
// ============================================================================

namespace
{
	u_int NormalDecodeLintScan(const char* szSrc, bool bIsMaterialFile, Zenith_Vector<Flux_NormalDecodeLint::Violation>& axOut)
	{
		Flux_NormalDecodeLint::ScanSource("probe.slang", szSrc, bIsMaterialFile, axOut);
		return axOut.GetSize();
	}
}

ZENITH_TEST(NormalDecodeLint, OpenCodedDecodeDetected)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const u_int n = NormalDecodeLintScan(
		"float2 xNormalXY = xNormalTex.Sample(xUV).xy * 2.0 - 1.0;", false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "an open-coded Sample(...).xy * 2.0 - 1.0 must be flagged");
	if (n == 1)
	{
		ZENITH_ASSERT_TRUE(ax.Get(0).m_eRule == Flux_NormalDecodeLint::RULE_NORMAL_DECODE, "must classify as a normal-decode violation");
	}
}

ZENITH_TEST(NormalDecodeLint, ThreeComponentSwizzleDetected)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const u_int n = NormalDecodeLintScan(
		"float3 n = xTex.Sample(xUV).xyz * 2.0 - 1.0;", false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "a three-component swizzle (.xyz) must also be flagged");
}

ZENITH_TEST(NormalDecodeLint, RgSwizzleAndIntegerSpellingDetected)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const u_int n = NormalDecodeLintScan(
		"float2 n = xTex.Sample(xUV).rg * 2 - 1;", false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "the .rg swizzle with bare integer literals must be flagged");
}

ZENITH_TEST(NormalDecodeLint, SampleLevelAndLoadVariantsDetected)
{
	{
		Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
		const u_int n = NormalDecodeLintScan(
			"float3 n = xTex.SampleLevel(xUV, 0).rgb * 2.0f - 1.0f;", false, ax);
		ZENITH_ASSERT_EQ(n, 1u, "SampleLevel(...) with an 'f' literal suffix must be flagged");
	}
	{
		Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
		const u_int n = NormalDecodeLintScan(
			"float2 n = g_axBindlessTextures[uIdx].Load(int3(xPos, 0)).xy * 2.0 - 1.0;", false, ax);
		ZENITH_ASSERT_EQ(n, 1u, "Load(...) must also be flagged");
	}
}

ZENITH_TEST(NormalDecodeLint, WhitespaceAndNewlineTolerant)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const char* szSrc =
		"float2 n = xTex.Sample( xUV )\n"
		"    .xy\n"
		"    *   2.0\n"
		"    -   1.0;\n";
	const u_int n = NormalDecodeLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "arbitrary whitespace/newlines between tokens must still be caught");
}

ZENITH_TEST(NormalDecodeLint, MaterialSlangFileExempt)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	// The one legitimate decode site: UnpackNormalMapTS itself, inlined here as it
	// appears in Common/Material.slang.
	const u_int n = NormalDecodeLintScan(
		"public float3 UnpackNormalMapTS(float4 xSampled) {\n"
		"  float2 xXY = xSampled.xy * 2.0 - 1.0;\n"
		"  return float3(xXY, sqrt(saturate(1.0 - dot(xXY, xXY))));\n"
		"}\n",
		/*bIsMaterialFile*/true, ax);
	ZENITH_ASSERT_EQ(n, 0u, "Common/Material.slang is exempt from the rule entirely");
}

ZENITH_TEST(NormalDecodeLint, CallerOfSharedHelperIsClean)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const u_int n = NormalDecodeLintScan(
		"float3 xNormalTS = UnpackNormalMapTS(xNormalTex.Sample(xUV));", false, ax);
	ZENITH_ASSERT_EQ(n, 0u, "a caller routing through UnpackNormalMapTS must stay clean");
}

ZENITH_TEST(NormalDecodeLint, UVToNDCFalsePositiveClassStaysClean)
{
	// The overwhelming idiom for `* 2.0 - 1.0` in this codebase is UV -> NDC, not a
	// normal decode -- none of these involve a texture sample, so none must trip.
	const char* aszUvToNdc[] = {
		"float2 xNDC = xUV * 2.0 - 1.0;",
		"float2 p = xPxPos * GetRcpOutputDims() * 2.0 - 1.0;",
		"float v = randomFloat(seed) * 2.0 - 1.0;",
		"float2 xNDC = input.uv.x * 2.0 - 1.0;",
		"float2 xNDC = float(xBit) * 2.0 - 1.0;",
	};
	for (const char* szSrc : aszUvToNdc)
	{
		Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
		const u_int n = NormalDecodeLintScan(szSrc, false, ax);
		ZENITH_ASSERT_EQ(n, 0u, "a UV -> NDC remap with no Sample/SampleLevel/Load prefix must NOT be flagged");
	}
}

ZENITH_TEST(NormalDecodeLint, CommentImmunity)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const char* szSrc =
		"// xTex.Sample(xUV).xy * 2.0 - 1.0 is fine in a comment\n"
		"/* xTex.Sample(xUV).xyz * 2 - 1 too */\n"
		"float4 f() { return float4(0,0,0,0); }\n";
	const u_int n = NormalDecodeLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 0u, "the shape inside comments must NOT be flagged");
}

ZENITH_TEST(NormalDecodeLint, StringImmunity)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const u_int n = NormalDecodeLintScan(
		"void f() { Log(\"xTex.Sample(xUV).xy * 2.0 - 1.0\"); }", false, ax);
	ZENITH_ASSERT_EQ(n, 0u, "the shape inside a string literal must NOT be flagged");
}

ZENITH_TEST(NormalDecodeLint, LineNumberAccurate)
{
	Zenith_Vector<Flux_NormalDecodeLint::Violation> ax;
	const char* szSrc =
		"// line 1\n"
		"float4 f()\n"
		"{ return xTex.Sample(xUV).xy * 2.0 - 1.0; }\n";   // line 3
	const u_int n = NormalDecodeLintScan(szSrc, false, ax);
	ZENITH_ASSERT_EQ(n, 1u, "one violation");
	if (n == 1) ZENITH_ASSERT_EQ(ax.Get(0).m_uLine, 3u, "violation must be reported on line 3");
}

ZENITH_TEST(NormalDecodeLint, IsMaterialSlangFilePathMatching)
{
	ZENITH_ASSERT_TRUE(Flux_NormalDecodeLint::IsMaterialSlangFile("C:/dev/Zenith/Zenith/Flux/Shaders/Common/Material.slang"),
		"absolute Windows path to the decode home must match");
	ZENITH_ASSERT_TRUE(Flux_NormalDecodeLint::IsMaterialSlangFile("Common/Material.slang"), "relative path must match");
	ZENITH_ASSERT_FALSE(Flux_NormalDecodeLint::IsMaterialSlangFile("Common/MaterialSurface.slang"),
		"MaterialSurface.slang must NOT match -- its own open-coded sites are genuine violations");
	ZENITH_ASSERT_FALSE(Flux_NormalDecodeLint::IsMaterialSlangFile("Terrain/Flux_Terrain_ToGBuffer.slang"),
		"a feature shader must NOT match");
}
