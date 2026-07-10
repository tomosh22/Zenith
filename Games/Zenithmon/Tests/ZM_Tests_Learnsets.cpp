#include "Zenith.h"

// ============================================================================
// ZM_Tests_Learnsets -- integrity of the derived per-species level-up learnsets
// (category ZM_Data). The learnset is a placeholder derivation (ZM-D-023), so
// this suite locks its CONTRACT rather than exact contents: every entry resolves
// to a real move, the list is level-ordered and bounded, moves are
// type-appropriate, a same-type damaging move is learnable early, status moves
// stay a minority, and the derivation is deterministic. These are the properties
// S2 relies on when it builds a monster's moveset from species + level.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"

namespace
{
	bool MoveTypeAllowed(ZM_TYPE eMoveType, const ZM_SpeciesData& xSp)
	{
		return eMoveType == xSp.m_aeTypes[0]
			|| eMoveType == xSp.m_aeTypes[1]
			|| eMoveType == ZM_TYPE_NORMAL;
	}
}

// Count is within bounds and every referenced move is a real ZM_MOVE_ID.
ZENITH_TEST(ZM_Data, Learnset_CountBoundedAndResolves)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset((ZM_SPECIES_ID)s);
		ZENITH_ASSERT_GE(xLs.m_uCount, 4u, "%s has too small a learnset", ZM_GetSpeciesName((ZM_SPECIES_ID)s));
		ZENITH_ASSERT_LE(xLs.m_uCount, uZM_MAX_LEARNSET_SIZE, "%s learnset overflows", ZM_GetSpeciesName((ZM_SPECIES_ID)s));
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			ZENITH_ASSERT_TRUE(xLs.m_axMoves[k].m_eMove < ZM_MOVE_COUNT,
				"%s learnset entry %u is not a real move", ZM_GetSpeciesName((ZM_SPECIES_ID)s), k);
		}
	}
}

// Levels are non-decreasing, in [1,60], and the first move is learned early.
ZENITH_TEST(ZM_Data, Learnset_SortedByLevel)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		u_int uPrev = 0;
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			const u_int uLevel = xLs.m_axMoves[k].m_uLevel;
			ZENITH_ASSERT_GE(uLevel, 1u, "%s entry %u level < 1", ZM_GetSpeciesName(eId), k);
			ZENITH_ASSERT_LE(uLevel, 60u, "%s entry %u level > 60", ZM_GetSpeciesName(eId), k);
			ZENITH_ASSERT_GE(uLevel, uPrev, "%s learnset is not level-ordered", ZM_GetSpeciesName(eId));
			uPrev = uLevel;
		}
		ZENITH_ASSERT_LE(xLs.m_axMoves[0].m_uLevel, 5u, "%s learns nothing by level 5", ZM_GetSpeciesName(eId));
	}
}

// Every move in a learnset is of the species' type(s) or NORMAL.
ZENITH_TEST(ZM_Data, Learnset_TypeAppropriate)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_SpeciesData& xSp = ZM_GetSpeciesData(eId);
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			const ZM_MoveData& xMove = ZM_GetMoveData(xLs.m_axMoves[k].m_eMove);
			ZENITH_ASSERT_TRUE(MoveTypeAllowed(xMove.m_eType, xSp),
				"%s learns off-type move %s (%s)", ZM_GetSpeciesName(eId), xMove.m_szName,
				ZM_TypeToString(xMove.m_eType));
		}
	}
}

// Every species knows at least one same-type (STAB) move and at least one
// damaging move by level 5 (so a low-level wild can actually battle).
ZENITH_TEST(ZM_Data, Learnset_HasStabAndEarlyDamage)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_SpeciesData& xSp = ZM_GetSpeciesData(eId);
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		bool bStab = false;
		bool bEarlyDamage = false;
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			const ZM_MoveData& xMove = ZM_GetMoveData(xLs.m_axMoves[k].m_eMove);
			if (xMove.m_eType == xSp.m_aeTypes[0]) { bStab = true; }
			if (xMove.m_eCategory != ZM_MOVE_CATEGORY_STATUS && xLs.m_axMoves[k].m_uLevel <= 5u)
			{
				bEarlyDamage = true;
			}
		}
		ZENITH_ASSERT_TRUE(bStab, "%s has no same-type move", ZM_GetSpeciesName(eId));
		ZENITH_ASSERT_TRUE(bEarlyDamage, "%s has no early damaging move", ZM_GetSpeciesName(eId));
	}
}

// No move appears twice in a species' learnset.
ZENITH_TEST(ZM_Data, Learnset_NoDuplicateMoves)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		for (u_int a = 0; a < xLs.m_uCount; ++a)
		{
			for (u_int b = a + 1; b < xLs.m_uCount; ++b)
			{
				ZENITH_ASSERT_NE((u_int)xLs.m_axMoves[a].m_eMove, (u_int)xLs.m_axMoves[b].m_eMove,
					"%s learns move %s twice", ZM_GetSpeciesName(eId),
					ZM_GetMoveName(xLs.m_axMoves[a].m_eMove));
			}
		}
	}
}

// Status moves are a minority of every learnset (movesets are damaging-dominant).
ZENITH_TEST(ZM_Data, Learnset_StatusMinority)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		u_int uStatus = 0;
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			if (ZM_GetMoveData(xLs.m_axMoves[k].m_eMove).m_eCategory == ZM_MOVE_CATEGORY_STATUS)
			{
				++uStatus;
			}
		}
		ZENITH_ASSERT_LE(uStatus, xLs.m_uCount / 2u, "%s learnset is status-heavy", ZM_GetSpeciesName(eId));
	}
}

// The derivation is a pure function: two calls return identical learnsets.
ZENITH_TEST(ZM_Data, Learnset_Deterministic)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)s;
		const ZM_Learnset xA = ZM_GetSpeciesLearnset(eId);
		const ZM_Learnset xB = ZM_GetSpeciesLearnset(eId);
		ZENITH_ASSERT_EQ(xA.m_uCount, xB.m_uCount, "%s learnset count not stable", ZM_GetSpeciesName(eId));
		for (u_int k = 0; k < xA.m_uCount; ++k)
		{
			ZENITH_ASSERT_EQ(xA.m_axMoves[k].m_uLevel, xB.m_axMoves[k].m_uLevel);
			ZENITH_ASSERT_EQ((u_int)xA.m_axMoves[k].m_eMove, (u_int)xB.m_axMoves[k].m_eMove);
		}
	}
}

// Evolved forms know at least as many moves as their pre-evolution (movepool
// grows, never shrinks, along a chain).
ZENITH_TEST(ZM_Data, Learnset_CountNonDecreasingOnEvolution)
{
	for (u_int s = 0; s < ZM_SPECIES_COUNT; ++s)
	{
		const ZM_SpeciesData& xSp = ZM_GetSpeciesData((ZM_SPECIES_ID)s);
		if (xSp.m_eEvolvesTo == ZM_SPECIES_NONE)
		{
			continue;
		}
		const u_int uThis = ZM_GetSpeciesLearnset(xSp.m_eId).m_uCount;
		const u_int uNext = ZM_GetSpeciesLearnset(xSp.m_eEvolvesTo).m_uCount;
		ZENITH_ASSERT_GE(uNext, uThis, "%s learns fewer moves than its pre-evolution", ZM_GetSpeciesName(xSp.m_eEvolvesTo));
	}
}
