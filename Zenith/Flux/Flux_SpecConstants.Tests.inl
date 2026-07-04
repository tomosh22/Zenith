#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_SpecConstants.h"

// ============================================================================
// Flux_SpecConstantTable + Flux_ResolveSpecConstants — Flux Shader System
// Overhaul, Stage 3a. Pure + headless (no device): the name-keyed value table
// (add / find / bool packing / handle overload / capacity) and the backend-free
// resolver (name → reflected id, skip-unknown, empty). The over-capacity and
// duplicate paths are guarded by Zenith_Assert, which halts under the test
// harness — those failure paths are validated by construction, not triggered.
// ============================================================================

ZENITH_TEST(FluxSpecConstants, TableAddFindBoolPackingAndHandle)
{
	Flux_SpecConstantTable xTable;
	ZENITH_ASSERT_TRUE(xTable.IsEmpty(), "fresh table is empty");
	ZENITH_ASSERT_EQ(xTable.m_uCount, 0u, "fresh count is 0");

	xTable.AddBool("SC_SHADOWS", true);
	xTable.AddBool("SC_CLUSTER", false);
	xTable.AddUInt("SC_QUALITY", 3u);

	ZENITH_ASSERT_FALSE(xTable.IsEmpty(), "table is non-empty after adds");
	ZENITH_ASSERT_EQ(xTable.m_uCount, 3u, "three entries");

	const Flux_SpecConstantEntry* pxShadows = xTable.Find("SC_SHADOWS");
	const Flux_SpecConstantEntry* pxCluster = xTable.Find("SC_CLUSTER");
	const Flux_SpecConstantEntry* pxQuality = xTable.Find("SC_QUALITY");
	ZENITH_ASSERT_NOT_NULL(pxShadows, "SC_SHADOWS present");
	ZENITH_ASSERT_NOT_NULL(pxCluster, "SC_CLUSTER present");
	ZENITH_ASSERT_NOT_NULL(pxQuality, "SC_QUALITY present");
	if (pxShadows) ZENITH_ASSERT_EQ(pxShadows->m_uValue, 1u, "bool true packs to 1");
	if (pxCluster) ZENITH_ASSERT_EQ(pxCluster->m_uValue, 0u, "bool false packs to 0");
	if (pxQuality) ZENITH_ASSERT_EQ(pxQuality->m_uValue, 3u, "uint value preserved");
	// Name-keyed adds carry no baked id (drift assert skipped for them).
	if (pxShadows) ZENITH_ASSERT_EQ(pxShadows->m_uBakedConstantId, UINT32_MAX, "name-keyed add has no baked id");

	// Handle-keyed add carries the codegen-baked id for the drift tripwire.
	const Flux_SpecConstantHandle xHandle{ "SC_HANDLE", 5u, 4u, 1u };
	xTable.AddBool(xHandle, true);
	const Flux_SpecConstantEntry* pxHandle = xTable.Find("SC_HANDLE");
	ZENITH_ASSERT_NOT_NULL(pxHandle, "handle-added entry present");
	if (pxHandle)
	{
		ZENITH_ASSERT_EQ(pxHandle->m_uValue, 1u, "handle bool packs to 1");
		ZENITH_ASSERT_EQ(pxHandle->m_uBakedConstantId, 5u, "handle baked id stored");
	}
	ZENITH_ASSERT_EQ(xTable.m_uCount, 4u, "four entries after handle add");

	ZENITH_ASSERT_NULL(xTable.Find("SC_MISSING"), "absent name -> null");
	ZENITH_ASSERT_NULL(xTable.Find(nullptr), "null name -> null");
}

ZENITH_TEST(FluxSpecConstants, TableFillsToCapacity)
{
	static_assert(FLUX_MAX_SPEC_CONSTANTS == 8, "test's fixed name array assumes an 8-slot table");
	static const char* aszNames[FLUX_MAX_SPEC_CONSTANTS] = {
		"SC_0", "SC_1", "SC_2", "SC_3", "SC_4", "SC_5", "SC_6", "SC_7" };

	Flux_SpecConstantTable xTable;
	for (u_int u = 0; u < FLUX_MAX_SPEC_CONSTANTS; u++) xTable.AddUInt(aszNames[u], u);
	ZENITH_ASSERT_EQ(xTable.m_uCount, FLUX_MAX_SPEC_CONSTANTS, "filled exactly to capacity");
	for (u_int u = 0; u < FLUX_MAX_SPEC_CONSTANTS; u++)
	{
		const Flux_SpecConstantEntry* pxE = xTable.Find(aszNames[u]);
		ZENITH_ASSERT_NOT_NULL(pxE, "each name findable at capacity");
		if (pxE) ZENITH_ASSERT_EQ(pxE->m_uValue, u, "each value preserved at capacity");
	}
}

ZENITH_TEST(FluxSpecConstants, ResolveAgainstReflection)
{
	// A reflection carrying two spec constants at non-trivial ids (3 and 7).
	Flux_ShaderReflection xRefl;
	{
		Flux_ReflectedSpecConstant xA;
		xA.m_strName = "SC_SHADOWS"; xA.m_uConstantId = 3u; xA.m_uSize = 4u; xA.m_uDefaultValue = 1u; xA.m_strTypeName = "bool";
		Flux_ReflectedSpecConstant xB;
		xB.m_strName = "SC_CLUSTER"; xB.m_uConstantId = 7u; xB.m_uSize = 4u; xB.m_uDefaultValue = 1u; xB.m_strTypeName = "bool";
		xRefl.AddSpecConstant(xA);
		xRefl.AddSpecConstant(xB);
	}

	// A table staging values by name (+ one name absent from the reflection). The
	// SC_SHADOWS entry is added via a matching handle to exercise the no-drift path.
	Flux_SpecConstantTable xTable;
	const Flux_SpecConstantHandle xShadowsHandle{ "SC_SHADOWS", 3u, 4u, 1u };
	xTable.AddBool(xShadowsHandle, false);   // value 0, baked id 3 == reflected 3
	xTable.AddBool("SC_CLUSTER", true);      // value 1
	xTable.AddBool("SC_ABSENT",  true);      // not in reflection -> skipped by the resolver

	Flux_ResolvedSpecConstant axOut[FLUX_MAX_SPEC_CONSTANTS];
	const u_int uCount = Flux_ResolveSpecConstants(xTable, xRefl, axOut, FLUX_MAX_SPEC_CONSTANTS);
	ZENITH_ASSERT_EQ(uCount, 2u, "two of three resolve (SC_ABSENT skipped)");

	// Table order preserved: SC_SHADOWS (id 3, value 0), SC_CLUSTER (id 7, value 1).
	ZENITH_ASSERT_EQ(axOut[0].m_uConstantId, 3u, "first resolved id from reflection");
	ZENITH_ASSERT_EQ(axOut[0].m_uValue, 0u, "first resolved value from table");
	ZENITH_ASSERT_EQ(axOut[0].m_uSize, 4u, "first resolved size from reflection");
	ZENITH_ASSERT_EQ(axOut[1].m_uConstantId, 7u, "second resolved id from reflection");
	ZENITH_ASSERT_EQ(axOut[1].m_uValue, 1u, "second resolved value from table");
}

ZENITH_TEST(FluxSpecConstants, ResolveEmptyTableAndTinyBufferAreSafe)
{
	Flux_ShaderReflection xRefl;
	Flux_ReflectedSpecConstant xA; xA.m_strName = "SC_X"; xA.m_uConstantId = 0u;
	xRefl.AddSpecConstant(xA);

	Flux_ResolvedSpecConstant axOut[FLUX_MAX_SPEC_CONSTANTS];

	// Empty table -> nothing resolved.
	Flux_SpecConstantTable xEmpty;
	ZENITH_ASSERT_TRUE(xEmpty.IsEmpty(), "empty table reports empty");
	ZENITH_ASSERT_EQ(Flux_ResolveSpecConstants(xEmpty, xRefl, axOut, FLUX_MAX_SPEC_CONSTANTS), 0u, "empty table resolves nothing");

	// Zero-capacity output -> nothing written, no overflow.
	Flux_SpecConstantTable xOne;
	xOne.AddBool("SC_X", true);
	ZENITH_ASSERT_EQ(Flux_ResolveSpecConstants(xOne, xRefl, axOut, 0u), 0u, "zero out-capacity writes nothing");
}
