#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_MaterialTable.h"
#include <vector>

// ============================================================================
// Flux_MaterialTable slot-lifecycle unit tests (Step-H hardening).
//
// Cover the pure index-assignment decision (Flux_DecideMaterialSlot) that drives
// Flux_MaterialTable::GetOrCreateIndex: a never-registered material gets a fresh
// index and needs a build; an unchanged material reuses its index with no build;
// an edited material (edit-stamp change) or a bindless-table change (generation
// change) forces a rebuild; distinct materials get distinct dense indices and the
// upload high-water tracks the max. No GPU/engine involvement — the index
// allocator + stamp/gen mirrors are constructed on the stack (the renderer's
// GetOrCreateIndex layers the engine-side effects — material index read/write +
// BuildRecord — on top of this decision).
// ============================================================================

namespace
{
	constexpr u_int kMatInvalid = 0xFFFFFFFFu;   // mirrors uFLUX_INVALID_MATERIAL_INDEX

	// A stack-only harness mirroring the table's internal bookkeeping state.
	struct MatSlotHarness
	{
		Flux_BindlessAllocator m_xAlloc;
		std::vector<u_int64>   m_xStamp;
		std::vector<u_int64>   m_xGen;
		u_int                  m_uMaxIndex = 0;

		explicit MatSlotHarness(u_int uCapacity = 16u)
		{
			m_xAlloc.Initialise(uCapacity);
			m_xStamp.assign(uCapacity, ~0ull);
			m_xGen.assign(uCapacity, ~0ull);
		}

		Flux_MaterialSlotDecision Decide(u_int uStored, u_int64 uStamp, u_int64 uGen)
		{
			return Flux_DecideMaterialSlot(m_xAlloc, m_xStamp, m_xGen, m_uMaxIndex,
				uStored, kMatInvalid, uStamp, uGen);
		}
	};
}

ZENITH_TEST(MaterialTable, FreshMaterialAllocatesNonZeroIndexAndBuilds)
{
	MatSlotHarness xH;
	const Flux_MaterialSlotDecision xDec = xH.Decide(kMatInvalid, 100ull, 5ull);

	ZENITH_ASSERT_TRUE(xDec.m_uIndex == 1u,
		"First material must get index 1 (slot 0 is the reserved default)");
	ZENITH_ASSERT_TRUE(xDec.m_bNeedsBuild,
		"A never-registered material must need its GPU record built");
	ZENITH_ASSERT_TRUE(xH.m_uMaxIndex == 1u,
		"Upload high-water must advance to the newly allocated index");
}

ZENITH_TEST(MaterialTable, UnchangedMaterialReusesIndexWithoutRebuild)
{
	MatSlotHarness xH;
	const u_int uIdx = xH.Decide(kMatInvalid, 100ull, 5ull).m_uIndex;

	const Flux_MaterialSlotDecision xDec = xH.Decide(uIdx, 100ull, 5ull);

	ZENITH_ASSERT_TRUE(xDec.m_uIndex == uIdx,
		"An already-registered material must keep its index");
	ZENITH_ASSERT_TRUE(!xDec.m_bNeedsBuild,
		"Same edit-stamp + same bindless generation must NOT rebuild");
}

ZENITH_TEST(MaterialTable, EditedMaterialRebuildsSameIndex)
{
	MatSlotHarness xH;
	const u_int uIdx = xH.Decide(kMatInvalid, 100ull, 5ull).m_uIndex;

	const Flux_MaterialSlotDecision xDec = xH.Decide(uIdx, 101ull /* edited */, 5ull);

	ZENITH_ASSERT_TRUE(xDec.m_uIndex == uIdx,
		"Editing a material keeps its table slot");
	ZENITH_ASSERT_TRUE(xDec.m_bNeedsBuild,
		"A changed edit-stamp must rebuild the GPU record");
}

ZENITH_TEST(MaterialTable, BindlessGenerationChangeRebuilds)
{
	MatSlotHarness xH;
	const u_int uIdx = xH.Decide(kMatInvalid, 100ull, 5ull).m_uIndex;

	const Flux_MaterialSlotDecision xDec = xH.Decide(uIdx, 100ull, 6ull /* a texture slot moved */);

	ZENITH_ASSERT_TRUE(xDec.m_bNeedsBuild,
		"A bindless-generation bump (texture slot freed/reallocated) must rebuild so the indices re-resolve");
}

ZENITH_TEST(MaterialTable, DistinctMaterialsGetDistinctDenseIndices)
{
	MatSlotHarness xH;
	const Flux_MaterialSlotDecision xA = xH.Decide(kMatInvalid, 10ull, 0ull);
	const Flux_MaterialSlotDecision xB = xH.Decide(kMatInvalid, 20ull, 0ull);

	ZENITH_ASSERT_TRUE(xA.m_uIndex == 1u && xB.m_uIndex == 2u,
		"Two new materials must get distinct dense indices (1, 2)");
	ZENITH_ASSERT_TRUE(xH.m_uMaxIndex == 2u,
		"Upload high-water must track the largest assigned index");
}
