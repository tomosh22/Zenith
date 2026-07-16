#include "Zenith.h"

// ============================================================================
// ZM_Tests_TallGrass -- S5 T0 unit gate for the ZM_TallGrassSystem component's
// PURE surface (category ZM_Grass). The tall-grass system (component order 109)
// quantizes the player's world position onto an integer grass-tile grid, fires
// an encounter roll only when the player crosses onto a NEW tile, and gates the
// whole thing on a per-tile grass-density threshold. These cases pin the three
// PURE static helpers + the density constant, so a drift in the floor-quantize
// semantics, the "first tile is never a transition" rule, or the inclusive
// density threshold is a boot-gate failure long before any scene loads.
//
// PURE / headless: no disk, no GPU, no entity creation. The ctor needs a
// Zenith_Entity& and OnAwake/OnUpdate touch scene/Flux state, so NO
// ZM_TallGrassSystem instance is constructed here -- only the `static` helpers
// (QuantizeToTile / IsTileTransition / IsGrassDensity) and the compile-time
// fGRASS_DENSITY_THRESHOLD constant are exercised. The windowed runtime
// behaviour (density load, tile-transition rolls, event emission) is the SC4
// windowed test, not here.
//
// ZM_GrassTile carries plain int fields (m_iX/m_iZ); this file builds tile
// literals by aggregate init and compares the fields explicitly rather than
// leaning on any operator== that may or may not exist on the struct.
//   1. QuantizeToTile_FloorSemantics -- floor() per axis, negatives toward -inf.
//   2. QuantizeToTile_AxesIndependent -- X and Z quantize independently.
//   3. IsTileTransition_FirstTileNever -- no last tile => never a transition.
//   4. IsTileTransition_SameTile       -- same tile => not a transition.
//   5. IsTileTransition_ChangedTile    -- any axis differs => transition.
//   6. IsGrassDensity_Threshold        -- inclusive >= 0.5 density gate.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"

// ############################################################################
// 1. QuantizeToTile -- floor() per axis, negatives round toward -infinity
// ############################################################################

// Each axis is independently floor()'d so a continuous world position lands on
// exactly one integer tile. Positive fractions truncate toward zero (which is
// floor), and -- the load-bearing case -- negative fractions round DOWN toward
// -infinity, not toward zero, so the tile grid has no double-wide seam at the
// origin.
ZENITH_TEST(ZM_Grass, QuantizeToTile_FloorSemantics)
{
	// Origin.
	const ZM_GrassTile xOrigin = ZM_TallGrassSystem::QuantizeToTile(0.0f, 0.0f);
	ZENITH_ASSERT_EQ(xOrigin.m_iX, 0, "QuantizeToTile(0,0).X must be 0");
	ZENITH_ASSERT_EQ(xOrigin.m_iZ, 0, "QuantizeToTile(0,0).Z must be 0");

	// Positive fraction below 1 stays in tile 0.
	const ZM_GrassTile xNear = ZM_TallGrassSystem::QuantizeToTile(0.9f, 0.9f);
	ZENITH_ASSERT_EQ(xNear.m_iX, 0, "QuantizeToTile(0.9,0.9).X must floor to 0");
	ZENITH_ASSERT_EQ(xNear.m_iZ, 0, "QuantizeToTile(0.9,0.9).Z must floor to 0");

	// Exact 1.0 lands on tile 1.
	const ZM_GrassTile xOne = ZM_TallGrassSystem::QuantizeToTile(1.0f, 1.0f);
	ZENITH_ASSERT_EQ(xOne.m_iX, 1, "QuantizeToTile(1,1).X must be 1");
	ZENITH_ASSERT_EQ(xOne.m_iZ, 1, "QuantizeToTile(1,1).Z must be 1");

	// Just under 2 on X, just over 2 on Z.
	const ZM_GrassTile xTwoish = ZM_TallGrassSystem::QuantizeToTile(1.999f, 2.001f);
	ZENITH_ASSERT_EQ(xTwoish.m_iX, 1, "QuantizeToTile(1.999,..).X must floor to 1");
	ZENITH_ASSERT_EQ(xTwoish.m_iZ, 2, "QuantizeToTile(..,2.001).Z must floor to 2");

	// Negative fraction rounds toward -infinity (-0.3 -> -1), both axes.
	const ZM_GrassTile xNegFrac = ZM_TallGrassSystem::QuantizeToTile(-0.3f, -0.3f);
	ZENITH_ASSERT_EQ(xNegFrac.m_iX, -1, "QuantizeToTile(-0.3,..).X must floor to -1");
	ZENITH_ASSERT_EQ(xNegFrac.m_iZ, -1, "QuantizeToTile(..,-0.3).Z must floor to -1");

	// Exact -1.0 lands on tile -1 (not -2, not 0).
	const ZM_GrassTile xNegOne = ZM_TallGrassSystem::QuantizeToTile(-1.0f, -1.0f);
	ZENITH_ASSERT_EQ(xNegOne.m_iX, -1, "QuantizeToTile(-1,-1).X must be -1");
	ZENITH_ASSERT_EQ(xNegOne.m_iZ, -1, "QuantizeToTile(-1,-1).Z must be -1");

	// Mixed sign: -1.2 -> -2 (toward -inf), 3.7 -> 3 (toward zero).
	const ZM_GrassTile xMixed = ZM_TallGrassSystem::QuantizeToTile(-1.2f, 3.7f);
	ZENITH_ASSERT_EQ(xMixed.m_iX, -2, "QuantizeToTile(-1.2,..).X must floor to -2");
	ZENITH_ASSERT_EQ(xMixed.m_iZ, 3, "QuantizeToTile(..,3.7).Z must floor to 3");
}

// ############################################################################
// 2. QuantizeToTile -- the two axes are quantized independently
// ############################################################################

// A single call with a positive X and a negative Z proves the axes do not share
// state or borrow each other's rounding direction: floor(2.6) = 2 on X while
// floor(-4.1) = -5 on Z in the very same call.
ZENITH_TEST(ZM_Grass, QuantizeToTile_AxesIndependent)
{
	const ZM_GrassTile xTile = ZM_TallGrassSystem::QuantizeToTile(2.6f, -4.1f);
	ZENITH_ASSERT_EQ(xTile.m_iX, 2, "QuantizeToTile(2.6,-4.1).X must floor to 2");
	ZENITH_ASSERT_EQ(xTile.m_iZ, -5, "QuantizeToTile(2.6,-4.1).Z must floor to -5");
}

// ############################################################################
// 3. IsTileTransition -- with no previous tile there is never a transition
// ############################################################################

// On the first frame (or after a teleport that clears the last tile) bHasLast is
// false, so IsTileTransition must report FALSE regardless of how the "last" and
// "current" tiles compare -- otherwise the very first step would spuriously fire
// an encounter roll.
ZENITH_TEST(ZM_Grass, IsTileTransition_FirstTileNever)
{
	const ZM_GrassTile xLast    = ZM_GrassTile{ 5, 7 };
	const ZM_GrassTile xCurrent = ZM_GrassTile{ 9, 2 };  // deliberately different

	ZENITH_ASSERT_FALSE(
		ZM_TallGrassSystem::IsTileTransition(xLast, /*bHasLast*/ false, xCurrent),
		"with no last tile there is no transition, even when tiles differ");

	// Also false when the (ignored) last tile happens to equal the current one.
	ZENITH_ASSERT_FALSE(
		ZM_TallGrassSystem::IsTileTransition(xCurrent, /*bHasLast*/ false, xCurrent),
		"with no last tile there is no transition, even when tiles match");
}

// ############################################################################
// 4. IsTileTransition -- staying on the same tile is not a transition
// ############################################################################

// bHasLast is true and the last tile equals the current tile in BOTH axes, so
// the player has not crossed a tile boundary this frame -> FALSE.
ZENITH_TEST(ZM_Grass, IsTileTransition_SameTile)
{
	const ZM_GrassTile xTile = ZM_GrassTile{ 3, -4 };

	ZENITH_ASSERT_FALSE(
		ZM_TallGrassSystem::IsTileTransition(xTile, /*bHasLast*/ true, xTile),
		"staying on the same tile (x AND z equal) is not a transition");

	// A distinct-but-equal-valued last tile must behave identically.
	const ZM_GrassTile xSameValue = ZM_GrassTile{ 3, -4 };
	ZENITH_ASSERT_FALSE(
		ZM_TallGrassSystem::IsTileTransition(xSameValue, /*bHasLast*/ true, xTile),
		"an equal-valued last tile is not a transition");
}

// ############################################################################
// 5. IsTileTransition -- crossing onto a different tile IS a transition
// ############################################################################

// With bHasLast true, a difference in EITHER axis (or both) means the player
// stepped onto a new grass tile, so IsTileTransition must report TRUE for each
// of the three "differs" shapes.
ZENITH_TEST(ZM_Grass, IsTileTransition_ChangedTile)
{
	const ZM_GrassTile xCurrent = ZM_GrassTile{ 4, 8 };

	// X differs only.
	const ZM_GrassTile xDiffX = ZM_GrassTile{ 5, 8 };
	ZENITH_ASSERT_TRUE(
		ZM_TallGrassSystem::IsTileTransition(xDiffX, /*bHasLast*/ true, xCurrent),
		"a change in X only is a transition");

	// Z differs only.
	const ZM_GrassTile xDiffZ = ZM_GrassTile{ 4, 9 };
	ZENITH_ASSERT_TRUE(
		ZM_TallGrassSystem::IsTileTransition(xDiffZ, /*bHasLast*/ true, xCurrent),
		"a change in Z only is a transition");

	// Both axes differ.
	const ZM_GrassTile xDiffBoth = ZM_GrassTile{ 5, 9 };
	ZENITH_ASSERT_TRUE(
		ZM_TallGrassSystem::IsTileTransition(xDiffBoth, /*bHasLast*/ true, xCurrent),
		"a change in both X and Z is a transition");
}

// ############################################################################
// 6. IsGrassDensity -- inclusive density threshold at 0.5
// ############################################################################

// A tile counts as tall grass when its density is at or above the threshold.
// The comparison is INCLUSIVE (>=), so exactly 0.5 qualifies; 0.49 does not and
// 0.51 does. The golden threshold constant is pinned alongside so a change to it
// is a deliberate two-place edit.
ZENITH_TEST(ZM_Grass, IsGrassDensity_Threshold)
{
	ZENITH_ASSERT_TRUE(ZM_TallGrassSystem::IsGrassDensity(0.5f),
		"density exactly at the threshold (0.5) is grass (inclusive)");
	ZENITH_ASSERT_FALSE(ZM_TallGrassSystem::IsGrassDensity(0.49f),
		"density just below the threshold (0.49) is not grass");
	ZENITH_ASSERT_TRUE(ZM_TallGrassSystem::IsGrassDensity(0.51f),
		"density just above the threshold (0.51) is grass");
	ZENITH_ASSERT_TRUE(ZM_TallGrassSystem::IsGrassDensity(1.0f),
		"full density (1.0) is grass");
	ZENITH_ASSERT_FALSE(ZM_TallGrassSystem::IsGrassDensity(0.0f),
		"zero density (0.0) is not grass");

	ZENITH_ASSERT_EQ(ZM_TallGrassSystem::fGRASS_DENSITY_THRESHOLD, 0.5f,
		"the grass-density threshold constant must be exactly 0.5");
}
