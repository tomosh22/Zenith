#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_VertexLayoutDesc.h"

// ============================================================================
// Baked vertex-layout descriptor tests. Pure CPU, no device, no Slang — hosted
// UNGATED in the always-linked Flux_MaterialTable.cpp so they run in every
// config, Null_ included.
//
// The load-bearing property is that equality is ELEMENT-WISE, not pointer-wise:
// the T2.c tripwire compares a table baked into a generated header against one
// built at runtime from live reflection, and those are two distinct arrays that
// must still compare equal. Every "same values, different array" case below
// exists to pin that, and every one-field-apart case exists to prove the
// comparison is not accidentally vacuous.
// ============================================================================

namespace
{
	// The terrain vertex, spelled the way a generated header spells it. Used as
	// the reference table on both sides of the identity tests.
	constexpr Flux_VertexLayoutElement kaxLayoutTestTerrainA[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,           0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2,           0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_SNORM10_10_10_2,  0u, 20u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, SHADER_DATA_TYPE_SNORM10_10_10_2,  0u, 24u },
	};

	// A SEPARATE array holding identical values — the "two different tables, same
	// layout" case the tripwire actually faces.
	constexpr Flux_VertexLayoutElement kaxLayoutTestTerrainB[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,           0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2,           0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_SNORM10_10_10_2,  0u, 20u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, SHADER_DATA_TYPE_SNORM10_10_10_2,  0u, 24u },
	};

	constexpr Flux_VertexLayoutDesc kxLayoutTestTerrainA{ kaxLayoutTestTerrainA, 4u, { 28u, 0u } };
	constexpr Flux_VertexLayoutDesc kxLayoutTestTerrainB{ kaxLayoutTestTerrainB, 4u, { 28u, 0u } };
}

ZENITH_TEST(VertexLayoutDesc, EqualityIsElementWiseNotPointerWise)
{
	// The whole point of the type: distinct storage, identical content, equal.
	ZENITH_ASSERT_TRUE(kxLayoutTestTerrainA.m_paxElements != kxLayoutTestTerrainB.m_paxElements,
		"the two reference descs must point at distinct arrays or this test proves nothing");
	ZENITH_ASSERT_TRUE(kxLayoutTestTerrainA == kxLayoutTestTerrainB,
		"two descs over distinct arrays with identical elements must compare equal");

	// And it holds in a constant expression, which is what lets a static_assert
	// pin a generated layout against a hand-written contract.
	static_assert(kxLayoutTestTerrainA == kxLayoutTestTerrainB,
		"element-wise equality must be usable in a constant expression");
}

ZENITH_TEST(VertexLayoutDesc, DifferingOffsetBreaksEquality)
{
	Flux_VertexLayoutElement axShifted[4];
	for (u_int u = 0; u < 4u; u++) axShifted[u] = kaxLayoutTestTerrainA[u];
	axShifted[3].m_uOffset = 25u;   // one byte off — the exact drift the tripwire hunts

	const Flux_VertexLayoutDesc xShifted{ axShifted, 4u, { 28u, 0u } };
	ZENITH_ASSERT_FALSE(xShifted == kxLayoutTestTerrainA, "a one-byte offset drift must not compare equal");
}

ZENITH_TEST(VertexLayoutDesc, DifferingTypeBreaksEquality)
{
	Flux_VertexLayoutElement axRetyped[4];
	for (u_int u = 0; u < 4u; u++) axRetyped[u] = kaxLayoutTestTerrainA[u];
	// The packed-vs-unpacked confusion this whole vocabulary exists to prevent.
	axRetyped[2].m_eType = SHADER_DATA_TYPE_FLOAT4;

	const Flux_VertexLayoutDesc xRetyped{ axRetyped, 4u, { 28u, 0u } };
	ZENITH_ASSERT_FALSE(xRetyped == kxLayoutTestTerrainA, "a storage-format change must not compare equal");
}

ZENITH_TEST(VertexLayoutDesc, DifferingSemanticIndexBreaksEquality)
{
	Flux_VertexLayoutElement axReindexed[4];
	for (u_int u = 0; u < 4u; u++) axReindexed[u] = kaxLayoutTestTerrainA[u];
	axReindexed[1].m_uSemanticIndex = 1u;   // TEXCOORD0 -> TEXCOORD1

	const Flux_VertexLayoutDesc xReindexed{ axReindexed, 4u, { 28u, 0u } };
	ZENITH_ASSERT_FALSE(xReindexed == kxLayoutTestTerrainA, "a semantic-index change must not compare equal");
}

ZENITH_TEST(VertexLayoutDesc, DifferingStrideBreaksEquality)
{
	// Same elements, different stride: reachable when a trailing attribute is
	// dropped from one side but the shared prefix still matches element-for-element.
	const Flux_VertexLayoutDesc xPadded{ kaxLayoutTestTerrainA, 4u, { 32u, 0u } };
	ZENITH_ASSERT_FALSE(xPadded == kxLayoutTestTerrainA, "a stride change must not compare equal");

	const Flux_VertexLayoutDesc xInstanced{ kaxLayoutTestTerrainA, 4u, { 28u, 16u } };
	ZENITH_ASSERT_FALSE(xInstanced == kxLayoutTestTerrainA, "an instance-stream stride must not compare equal");
}

ZENITH_TEST(VertexLayoutDesc, DifferingCountBreaksEquality)
{
	// A prefix of the same array: every compared element matches, only the count differs.
	const Flux_VertexLayoutDesc xPrefix{ kaxLayoutTestTerrainA, 3u, { 28u, 0u } };
	ZENITH_ASSERT_FALSE(xPrefix == kxLayoutTestTerrainA, "a truncated table must not compare equal");
}

ZENITH_TEST(VertexLayoutDesc, EmptyLayoutIsTheDefaultConstructedOne)
{
	// Codegen emits exactly { nullptr, 0u, { 0u, 0u } } for a program with no vertex
	// inputs, so "takes no vertex buffer" is one equality test against a default.
	constexpr Flux_VertexLayoutDesc kxNull{ nullptr, 0u, { 0u, 0u } };
	constexpr Flux_VertexLayoutDesc kxDefault{};
	static_assert(kxNull == kxDefault, "the canonical empty layout must equal a default-constructed desc");

	ZENITH_ASSERT_TRUE(kxNull == kxDefault, "the canonical empty layout must equal a default-constructed desc");
	ZENITH_ASSERT_FALSE(kxNull == kxLayoutTestTerrainA, "an empty layout must not equal a populated one");
}

ZENITH_TEST(VertexLayoutDesc, SemanticVocabularyRoundTrips)
{
	// Every row parses to its own tag and emits its own identifier — the one table
	// serving both projections is what keeps codegen from baking UNKNOWN.
	for (const Flux_VertexSemanticEntry& xEntry : kaxFLUX_VERTEX_SEMANTICS)
	{
		FluxVertexSemantic eParsed = FLUX_VERTEX_SEMANTIC_UNKNOWN;
		ZENITH_ASSERT_TRUE(Flux_ParseVertexSemantic(xEntry.m_szSemanticName, eParsed),
			"semantic '%s' must parse", xEntry.m_szSemanticName);
		ZENITH_ASSERT_EQ(static_cast<u_int>(eParsed), static_cast<u_int>(xEntry.m_eSemantic),
			"semantic '%s' must parse to its own tag", xEntry.m_szSemanticName);
		ZENITH_ASSERT_TRUE(strcmp(Flux_VertexSemanticEnumToken(eParsed), xEntry.m_szEnumToken) == 0,
			"semantic '%s' must emit its own enumerator identifier", xEntry.m_szSemanticName);
	}

	// A name outside the vocabulary FAILS rather than resolving to anything — that
	// failure is what fails the FluxCompiler run instead of baking a wrong layout.
	FluxVertexSemantic eUnknown = FLUX_VERTEX_SEMANTIC_POSITION;
	ZENITH_ASSERT_FALSE(Flux_ParseVertexSemantic("BLENDWEIGHT", eUnknown), "an unnamed semantic must not parse");
	ZENITH_ASSERT_FALSE(Flux_ParseVertexSemantic(nullptr, eUnknown), "a null semantic must not parse");
	ZENITH_ASSERT_EQ(static_cast<u_int>(eUnknown), static_cast<u_int>(FLUX_VERTEX_SEMANTIC_POSITION),
		"a failed parse must leave the caller's tag untouched");

	// Slang reports the base name with the index split off, so an indexed spelling
	// is NOT a vocabulary member — it would mean the caller skipped the split.
	ZENITH_ASSERT_FALSE(Flux_ParseVertexSemantic("TEXCOORD0", eUnknown),
		"semantics arrive with the trailing index already split off");
}
