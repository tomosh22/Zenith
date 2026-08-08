#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_VertexLayoutValidation.h"

// ============================================================================
// Tests for THE stale-codegen tripwire comparator. Pure CPU, no device, no
// Slang — hosted UNGATED in the always-linked Flux_MaterialTable.cpp.
//
// At every real boot the comparator only ever sees a generated table and a live
// reflection produced from the SAME source, so production traffic exercises
// nothing but the "already matches" branch. These tests are the only place the
// four mismatch categories and the null/empty spellings are ever driven — a
// comparator defect (an off-by-one, an inverted null case) would otherwise be
// unobservable until the one day the tripwire is needed for real.
// ============================================================================

namespace
{
	Flux_ReflectedVertexAttribute MakeMatchTestAttribute(const char* szName, const char* szSemantic,
		u_int uSemanticIndex, u_int uLocation, ShaderDataType eType, u_int uBinding)
	{
		Flux_ReflectedVertexAttribute xA;
		xA.m_strName = szName;
		xA.m_strSemantic = szSemantic;
		xA.m_uSemanticIndex = uSemanticIndex;
		xA.m_uLocation = uLocation;
		xA.m_eType = eType;
		xA.m_uBinding = uBinding;
		return xA;
	}

	// The canonical live side: pos/uv/normal on binding 0 + one instanced colour
	// on binding 1 — small enough to reason about, wide enough to cover both
	// bindings and three distinct storage types. Strides: 24 / 4.
	void BuildMatchTestReflection(Flux_ShaderReflection& xRefl)
	{
		xRefl.AddVertexAttribute(MakeMatchTestAttribute("a_xPosition", "POSITION", 0u, 0u, SHADER_DATA_TYPE_FLOAT3, 0u));
		xRefl.AddVertexAttribute(MakeMatchTestAttribute("a_xUV",       "TEXCOORD", 0u, 1u, SHADER_DATA_TYPE_FLOAT2, 0u));
		xRefl.AddVertexAttribute(MakeMatchTestAttribute("a_xNormal",   "NORMAL",   0u, 2u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u));
		xRefl.AddVertexAttribute(MakeMatchTestAttribute("a_xColour",   "COLOR",    0u, 3u, SHADER_DATA_TYPE_UNORM8X4, 1u));
		xRefl.ComputeVertexPacking();
	}

	// The expected side, spelled the way a generated header spells it.
	constexpr Flux_VertexLayoutElement kaxMatchTestElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,          0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2,          0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u, 20u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_UNORM8X4,        1u,  0u },
	};
	constexpr Flux_VertexLayoutDesc kxMatchTestDesc{ kaxMatchTestElements, 4u, { 24u, 4u } };
	constexpr Flux_VertexLayoutDesc kxMatchTestEmptyDesc{ nullptr, 0u, { 0u, 0u } };
}

ZENITH_TEST(VertexLayoutValidation, MatchesOnIdenticalTables)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &kxMatchTestDesc);
	ZENITH_ASSERT_TRUE(xResult.IsMatch(), "a generated table equal to the live reflection must match");
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_OK, "the match kind must be OK on success");
}

ZENITH_TEST(VertexLayoutValidation, NullExpectedMatchesEmptyReflection)
{
	Flux_ShaderReflection xEmpty;
	ZENITH_ASSERT_TRUE(Flux_MatchVertexLayoutToReflection(xEmpty, nullptr).IsMatch(),
		"nullptr is the canonical no-vertex-input spelling and must match an empty reflection");
}

ZENITH_TEST(VertexLayoutValidation, CanonicalEmptyDescMatchesEmptyReflection)
{
	Flux_ShaderReflection xEmpty;
	ZENITH_ASSERT_TRUE(Flux_MatchVertexLayoutToReflection(xEmpty, &kxMatchTestEmptyDesc).IsMatch(),
		"the generated empty desc ({nullptr,0,{0,0}}) is the other legal no-input spelling");
}

ZENITH_TEST(VertexLayoutValidation, NullExpectedRejectsNonEmptyReflection)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, nullptr);
	ZENITH_ASSERT_FALSE(xResult.IsMatch(), "a shader that fetches attributes must not accept a null layout");
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_COUNT, "the mismatch must be reported as COUNT");
}

ZENITH_TEST(VertexLayoutValidation, CountMismatchDetected)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	// One element short of the live table, otherwise identical.
	const Flux_VertexLayoutDesc xShort{ kaxMatchTestElements, 3u, { 24u, 4u } };
	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xShort);
	ZENITH_ASSERT_FALSE(xResult.IsMatch(), "an element-count drift must be detected");
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_COUNT, "the mismatch must be reported as COUNT");
}

ZENITH_TEST(VertexLayoutValidation, StrideMismatchDetectedOnBothBindings)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	const Flux_VertexLayoutDesc xWrongStride0{ kaxMatchTestElements, 4u, { 28u, 4u } };
	Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xWrongStride0);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_STRIDE && xResult.m_uIndex == 0u,
		"a per-vertex stride drift must be reported as STRIDE at binding 0");

	const Flux_VertexLayoutDesc xWrongStride1{ kaxMatchTestElements, 4u, { 24u, 8u } };
	xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xWrongStride1);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_STRIDE && xResult.m_uIndex == 1u,
		"a per-instance stride drift must be reported as STRIDE at binding 1");
}

ZENITH_TEST(VertexLayoutValidation, ElementOffsetMismatchDetected)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	Flux_VertexLayoutElement axMutated[4] = { kaxMatchTestElements[0], kaxMatchTestElements[1], kaxMatchTestElements[2], kaxMatchTestElements[3] };
	axMutated[2].m_uOffset = 16u;   // NORMAL really packs at 20
	const Flux_VertexLayoutDesc xDesc{ axMutated, 4u, { 24u, 4u } };

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xDesc);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_ELEMENT && xResult.m_uIndex == 2u,
		"an element offset drift must be reported as ELEMENT at the offending index");
}

ZENITH_TEST(VertexLayoutValidation, ElementTypeMismatchDetected)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	Flux_VertexLayoutElement axMutated[4] = { kaxMatchTestElements[0], kaxMatchTestElements[1], kaxMatchTestElements[2], kaxMatchTestElements[3] };
	axMutated[2].m_eType = SHADER_DATA_TYPE_UNORM8X4;   // same 4-byte width as snorm10 — stride survives, only the type differs
	const Flux_VertexLayoutDesc xDesc{ axMutated, 4u, { 24u, 4u } };

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xDesc);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_ELEMENT && xResult.m_uIndex == 2u,
		"a storage-format drift that preserves the stride must still be detected");
}

ZENITH_TEST(VertexLayoutValidation, ElementSemanticMismatchDetected)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	Flux_VertexLayoutElement axMutated[4] = { kaxMatchTestElements[0], kaxMatchTestElements[1], kaxMatchTestElements[2], kaxMatchTestElements[3] };
	axMutated[2].m_eSemantic = FLUX_VERTEX_SEMANTIC_TANGENT;   // same type/offset/width — only the meaning differs
	const Flux_VertexLayoutDesc xDesc{ axMutated, 4u, { 24u, 4u } };

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xDesc);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_ELEMENT && xResult.m_uIndex == 2u,
		"a semantic swap between equal-width lanes must still be detected");
}

ZENITH_TEST(VertexLayoutValidation, ElementSemanticIndexMismatchDetected)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	Flux_VertexLayoutElement axMutated[4] = { kaxMatchTestElements[0], kaxMatchTestElements[1], kaxMatchTestElements[2], kaxMatchTestElements[3] };
	axMutated[1].m_uSemanticIndex = 1u;   // TEXCOORD0 vs TEXCOORD1
	const Flux_VertexLayoutDesc xDesc{ axMutated, 4u, { 24u, 4u } };

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xDesc);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_ELEMENT && xResult.m_uIndex == 1u,
		"a semantic-index drift must be detected");
}

ZENITH_TEST(VertexLayoutValidation, ElementBindingMismatchDetected)
{
	Flux_ShaderReflection xRefl;
	BuildMatchTestReflection(xRefl);

	Flux_VertexLayoutElement axMutated[4] = { kaxMatchTestElements[0], kaxMatchTestElements[1], kaxMatchTestElements[2], kaxMatchTestElements[3] };
	axMutated[3].m_uBinding = 0u;   // expected claims per-vertex; live says per-instance. Strides kept live-correct.
	const Flux_VertexLayoutDesc xDesc{ axMutated, 4u, { 24u, 4u } };

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xDesc);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_ELEMENT && xResult.m_uIndex == 3u,
		"an input-rate/binding drift must be detected");
}

ZENITH_TEST(VertexLayoutValidation, UnknownLiveSemanticDetected)
{
	Flux_ShaderReflection xRefl;
	xRefl.AddVertexAttribute(MakeMatchTestAttribute("a_xPosition", "POSITION",  0u, 0u, SHADER_DATA_TYPE_FLOAT3, 0u));
	xRefl.AddVertexAttribute(MakeMatchTestAttribute("a_xWeird",    "BLENDWEIGHT", 0u, 1u, SHADER_DATA_TYPE_FLOAT4, 0u));
	xRefl.ComputeVertexPacking();

	// Expected side matches counts/strides so the walk reaches the offending row.
	constexpr Flux_VertexLayoutElement kaxTwo[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_FLOAT4, 0u, 12u },
	};
	const Flux_VertexLayoutDesc xDesc{ kaxTwo, 2u, { 28u, 0u } };

	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xRefl, &xDesc);
	ZENITH_ASSERT_TRUE(xResult.m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_SEMANTIC && xResult.m_uIndex == 1u,
		"a live semantic outside the closed vocabulary must be reported as SEMANTIC — it cannot correspond to any generated row");
}
