#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleArena -- S5 T0 unit gate for the ZM_BattleArena component's
// PURE surface (category ZM_BattleArena). The battle arena lives at world
// Y = -2000 as an always-visible dome + 2 platforms + 6 per-biome dressing prop
// sets, exactly one shown at a time (spec ZM-D-088 / GDD S5). These cases pin
// the STATIC helpers + the compiled data-table contracts they lean on, so a
// drift in the biome roster, the visibility-mask scheme, the arena constants, or
// the WorldSpec Battle row is a boot-gate failure long before any scene loads.
//
// PURE / headless: no disk, no GPU, no entity creation. The ctor needs a
// Zenith_Entity& and OnStart touches graphics/scene, so NO ZM_BattleArena
// instance is constructed here -- only the `static` helpers
// (DressingPropForBiome / VisibilityMaskForBiome), the compile-time constants,
// and the ZM_GetPropData / ZM_GetWorldSpec data accessors are exercised. The
// windowed built-arena behaviour is the P1 ZM_BattleArena_Test.
//   1. BiomeEnumCoverage        -- the 6-biome roster agrees across the battle
//                                  enum, the component constant, and the prop
//                                  biome/dressing tables.
//   2. DressingMappingContract  -- every biome maps to a distinct real DRESSING
//                                  prop whose biome tag matches the roster order;
//                                  out-of-range -> ZM_PROP_NONE.
//   3. VisibilityExactlyOne     -- the per-biome mask is exactly 1u<<e (one bit);
//                                  out-of-range -> 0.
//   4. ArenaConstants           -- the arena world Y and serialization version
//                                  are golden-pinned.
//   5. WorldSpecBattleRowContract -- the Battle scene row is build index 1, kind
//                                  BATTLE, no terrain set.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_BattleArena.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

// ############################################################################
// 1. Biome enum coverage -- one roster, three tables must agree
// ############################################################################

// The battle-dome biome count is 6, the component mirrors it in uBIOME_COUNT,
// and that 6 is exactly the count of REAL (non-NONE) prop biomes AND the count
// of contiguous DRESSING roster ids -- so a biome added in one place but not the
// others fails here rather than silently mis-dressing the arena.
ZENITH_TEST(ZM_BattleArena, BiomeEnumCoverage)
{
	ZENITH_ASSERT_EQ((u_int)ZM_BATTLE_BIOME_COUNT, 6u,
		"the battle-dome biome enum must carry exactly 6 biomes");
	ZENITH_ASSERT_EQ(ZM_BattleArena::uBIOME_COUNT, 6u,
		"ZM_BattleArena::uBIOME_COUNT must mirror ZM_BATTLE_BIOME_COUNT");
	ZENITH_ASSERT_EQ(ZM_BattleArena::uBIOME_COUNT, (u_int)ZM_BATTLE_BIOME_COUNT,
		"the component constant and the enum count must be identical");

	// Real (non-NONE) prop biomes: [uZM_PROP_BIOME_FIRST_REAL, ZM_PROP_BIOME_COUNT).
	const u_int uRealPropBiomes =
		(u_int)ZM_PROP_BIOME_COUNT - uZM_PROP_BIOME_FIRST_REAL;
	ZENITH_ASSERT_EQ((u_int)ZM_BATTLE_BIOME_COUNT, uRealPropBiomes,
		"battle biome count must equal the number of real prop biomes");

	// The contiguous DRESSING roster block MEADOW..CANYON is one-per-biome.
	const u_int uDressingSpan =
		(u_int)ZM_PROP_DRESSING_CANYON - (u_int)ZM_PROP_DRESSING_MEADOW + 1u;
	ZENITH_ASSERT_EQ(uDressingSpan, 6u,
		"the DRESSING roster block must carry exactly 6 per-biome sets");
	ZENITH_ASSERT_EQ((u_int)ZM_BATTLE_BIOME_COUNT, uDressingSpan,
		"battle biome count must equal the DRESSING roster span");
}

// ############################################################################
// 2. Dressing mapping contract -- biome -> distinct real DRESSING prop
// ############################################################################

// For every battle biome, DressingPropForBiome returns a real (non-NONE) prop
// that is DRESSING-kind and whose roster biome tag matches the battle roster
// order (MEADOW + e); the six returned ids are pairwise distinct; and the
// out-of-range sentinel maps to ZM_PROP_NONE.
ZENITH_TEST(ZM_BattleArena, DressingMappingContract)
{
	ZM_PROP_ID aeMapped[ZM_BATTLE_BIOME_COUNT];

	for (u_int e = 0; e < (u_int)ZM_BATTLE_BIOME_COUNT; ++e)
	{
		const ZM_BATTLE_BIOME eBiome = (ZM_BATTLE_BIOME)e;
		const ZM_PROP_ID eProp = ZM_BattleArena::DressingPropForBiome(eBiome);

		// A real prop, in range.
		ZENITH_ASSERT_NE((u_int)eProp, (u_int)ZM_PROP_NONE,
			"biome %u must map to a real dressing prop (not ZM_PROP_NONE)", e);
		ZENITH_ASSERT_LT((u_int)eProp, (u_int)ZM_PROP_COUNT,
			"biome %u dressing prop id %u is out of roster range", e, (u_int)eProp);

		const ZM_PropData& xRow = ZM_GetPropData(eProp);

		// DRESSING kind.
		ZENITH_ASSERT_EQ((u_int)xRow.m_eKind, (u_int)ZM_PROP_KIND_DRESSING,
			"biome %u dressing prop must be ZM_PROP_KIND_DRESSING", e);

		// Roster biome tag agrees with the battle roster order (MEADOW + e).
		const ZM_PROP_BIOME eExpected = (ZM_PROP_BIOME)((u_int)ZM_PROP_BIOME_MEADOW + e);
		ZENITH_ASSERT_EQ((u_int)xRow.m_eBiome, (u_int)eExpected,
			"biome %u dressing prop carries the wrong roster biome tag", e);

		aeMapped[e] = eProp;
	}

	// Pairwise-distinct: no two biomes share a dressing set.
	for (u_int i = 0; i < (u_int)ZM_BATTLE_BIOME_COUNT; ++i)
	{
		for (u_int j = i + 1u; j < (u_int)ZM_BATTLE_BIOME_COUNT; ++j)
		{
			ZENITH_ASSERT_NE((u_int)aeMapped[i], (u_int)aeMapped[j],
				"biomes %u and %u map to the same dressing prop", i, j);
		}
	}

	// Out-of-range sentinel.
	ZENITH_ASSERT_EQ((u_int)ZM_BattleArena::DressingPropForBiome(ZM_BATTLE_BIOME_COUNT),
		(u_int)ZM_PROP_NONE,
		"an out-of-range biome must map to ZM_PROP_NONE");
}

// ############################################################################
// 3. Visibility mask -- exactly one bit, equal to 1u<<e
// ############################################################################

// VisibilityMaskForBiome yields a one-hot mask 1u<<e for each valid biome (the
// "exactly one dressing set shown" invariant, expressed as a single set bit),
// and 0 for the out-of-range sentinel. Popcount is checked by a manual bit loop
// (no std::popcount / no container).
ZENITH_TEST(ZM_BattleArena, VisibilityExactlyOne)
{
	for (u_int e = 0; e < (u_int)ZM_BATTLE_BIOME_COUNT; ++e)
	{
		const u_int uMask = ZM_BattleArena::VisibilityMaskForBiome((ZM_BATTLE_BIOME)e);

		// Exactly 1u<<e.
		ZENITH_ASSERT_EQ(uMask, (1u << e),
			"biome %u visibility mask must equal 1u<<%u", e, e);

		// Exactly one bit set (manual popcount).
		u_int uBits = 0u;
		for (u_int uScratch = uMask; uScratch != 0u; uScratch &= (uScratch - 1u))
		{
			++uBits;
		}
		ZENITH_ASSERT_EQ(uBits, 1u,
			"biome %u visibility mask must have exactly one bit set", e);
	}

	ZENITH_ASSERT_EQ(ZM_BattleArena::VisibilityMaskForBiome(ZM_BATTLE_BIOME_COUNT), 0u,
		"an out-of-range biome must map to an all-zero visibility mask");
}

// ############################################################################
// 4. Arena constants -- world Y + serialization version golden-pinned
// ############################################################################

// The arena's world Y (-2000) and its serialization version (1) are load-bearing
// literals: the Y anchors the always-below-the-world dome and the version gates
// the save-blob round trip. A change to either is a deliberate two-place edit.
ZENITH_TEST(ZM_BattleArena, ArenaConstants)
{
	ZENITH_ASSERT_EQ(ZM_BattleArena::fARENA_WORLD_Y, -2000.0f,
		"the battle arena world Y must be exactly -2000");
	ZENITH_ASSERT_EQ(ZM_BattleArena::uSERIALIZATION_VERSION, 1u,
		"the battle arena serialization version must be 1");
}

// ############################################################################
// 5. WorldSpec Battle row -- build index 1, kind BATTLE, no terrain
// ############################################################################

// The additive battle scene is WorldSpec row ZM_SCENE_BATTLE: build index 1
// (ZM-D-012), kind BATTLE, and an empty terrain set (the dome supplies its own
// geometry, so there is no baked terrain). The P1 test registers build index 1
// and loads Battle additively, so this row is the contract that keeps that wiring
// honest.
ZENITH_TEST(ZM_BattleArena, WorldSpecBattleRowContract)
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_BATTLE);

	ZENITH_ASSERT_EQ(xRow.m_uBuildIndex, 1u,
		"the Battle scene must be build index 1");
	ZENITH_ASSERT_EQ((u_int)xRow.m_eKind, (u_int)ZM_SCENE_KIND_BATTLE,
		"the Battle scene row must be kind ZM_SCENE_KIND_BATTLE");
	ZENITH_ASSERT_NOT_NULL(xRow.m_szTerrainSet,
		"the Battle scene terrain-set pointer must be non-null");
	ZENITH_ASSERT_STREQ(xRow.m_szTerrainSet, "",
		"the Battle scene must carry an empty terrain set");
}
