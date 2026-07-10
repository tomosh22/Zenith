#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"

// ============================================================================
// ZM_Learnsets -- the systematically-derived level-up learnset (ZM-D-023). Pure
// function of the compiled species + move tables; see the header for the design
// rationale. Every invariant is locked by Tests/ZM_Tests_Learnsets.cpp.
//
// Derivation, per species:
//   1. Partition the move table into four buckets by the species' types:
//        STAB   = damaging moves of the primary type
//        SEC    = damaging moves of the secondary type (dual-types only)
//        NORM   = damaging NORMAL moves (only when the species is not NORMAL-typed)
//        STATUS = status moves of any of {primary, secondary, NORMAL}
//   2. Sort the three damaging buckets by EFFECTIVE power ascending -- so weak
//      moves are taught first, strong moves last. Fixed-damage moves (power 0:
//      OHKO / fixed-level / halve-HP) read as high power so they land late.
//   3. Teach a level-1 STAB move, then round-robin STAB / (SEC|NORM) / an
//      occasional STATUS move (capped at a few), damaging-dominant.
//   4. Cap the count by evolution role; spread the levels 1..~50.
// ============================================================================

namespace
{
	const u_int uZM_LEARNSET_MAX_STATUS = 4u;   // keep a learnset damaging-dominant
	const u_int uZM_FIXED_DAMAGE_EFFPOW = 130u;  // power-0 fixed-damage moves sort late

	u_int EffectivePower(const ZM_MoveData& xMove)
	{
		return xMove.m_uPower > 0u ? xMove.m_uPower : uZM_FIXED_DAMAGE_EFFPOW;
	}

	// Stable insertion sort of move-index buffer by effective power ascending.
	// The input is index-ascending, so equal-power ties keep table order.
	void SortByEffectivePower(u_int* auIndices, u_int uCount)
	{
		for (u_int i = 1u; i < uCount; ++i)
		{
			const u_int uCur = auIndices[i];
			const u_int uKey = EffectivePower(ZM_GetMoveData((ZM_MOVE_ID)uCur));
			int j = (int)i - 1;
			while (j >= 0 && EffectivePower(ZM_GetMoveData((ZM_MOVE_ID)auIndices[j])) > uKey)
			{
				auIndices[j + 1] = auIndices[j];
				--j;
			}
			auIndices[j + 1] = uCur;
		}
	}
}

ZM_Learnset ZM_GetSpeciesLearnset(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xSpecies = ZM_GetSpeciesData(eId);
	const ZM_TYPE eT1 = xSpecies.m_aeTypes[0];
	const ZM_TYPE eT2 = xSpecies.m_aeTypes[1];
	const bool bDual = (eT2 != ZM_TYPE_NONE);

	// Buffers are index lists into the move table; 218 moves fit comfortably.
	u_int auStab[256];   u_int uStab = 0u;
	u_int auSec[256];    u_int uSec = 0u;
	u_int auNorm[256];   u_int uNorm = 0u;
	u_int auStatus[256]; u_int uStatus = 0u;

	const u_int uMoveCount = ZM_GetMoveCount();
	for (u_int i = 0u; i < uMoveCount; ++i)
	{
		const ZM_MoveData& xMove = ZM_GetMoveData((ZM_MOVE_ID)i);
		if (xMove.m_eCategory == ZM_MOVE_CATEGORY_STATUS)
		{
			if (xMove.m_eType == eT1 || (bDual && xMove.m_eType == eT2) || xMove.m_eType == ZM_TYPE_NORMAL)
			{
				auStatus[uStatus++] = i;
			}
		}
		else if (xMove.m_eType == eT1)
		{
			auStab[uStab++] = i;
		}
		else if (bDual && xMove.m_eType == eT2)
		{
			auSec[uSec++] = i;
		}
		else if (xMove.m_eType == ZM_TYPE_NORMAL && eT1 != ZM_TYPE_NORMAL && eT2 != ZM_TYPE_NORMAL)
		{
			auNorm[uNorm++] = i;
		}
	}
	SortByEffectivePower(auStab, uStab);
	SortByEffectivePower(auSec, uSec);
	SortByEffectivePower(auNorm, uNorm);
	// auStatus stays in table order.

	// Movepool size grows with evolution stage; a single-stage final (standalone
	// rare / legendary) reads as fully evolved.
	const bool bSingleStageFinal = (xSpecies.m_uEvoStage == 1u && xSpecies.m_eEvolvesTo == ZM_SPECIES_NONE);
	u_int uCap = bSingleStageFinal ? uZM_MAX_LEARNSET_SIZE : (10u + xSpecies.m_uEvoStage * 2u);
	if (uCap > uZM_MAX_LEARNSET_SIZE)
	{
		uCap = uZM_MAX_LEARNSET_SIZE;
	}

	u_int auPicks[uZM_MAX_LEARNSET_SIZE];
	u_int uPicks = 0u;
	u_int uSi = 0u, uSei = 0u, uNi = 0u, uTi = 0u, uStatusUsed = 0u, uRound = 0u;

	// Guarantee a damaging same-type move at level 1.
	if (uStab > 0u)
	{
		auPicks[uPicks++] = auStab[uSi++];
	}

	while (uPicks < uCap)
	{
		const bool bDamagingLeft = (uSi < uStab) || (uSei < uSec) || (uNi < uNorm);
		const bool bStatusLeft = (uStatusUsed < uZM_LEARNSET_MAX_STATUS) && (uTi < uStatus);
		if (!bDamagingLeft && !bStatusLeft)
		{
			break;
		}

		if (uSi < uStab)
		{
			auPicks[uPicks++] = auStab[uSi++];
		}
		if (uPicks >= uCap) { break; }

		if (uSei < uSec)
		{
			auPicks[uPicks++] = auSec[uSei++];
		}
		else if (uNi < uNorm)
		{
			auPicks[uPicks++] = auNorm[uNi++];
		}
		if (uPicks >= uCap) { break; }

		if ((uRound % 2u) == 1u && uStatusUsed < uZM_LEARNSET_MAX_STATUS && uTi < uStatus)
		{
			auPicks[uPicks++] = auStatus[uTi++];
			++uStatusUsed;
		}
		++uRound;
	}

	// Fill any remaining slots with leftover damaging moves (strongest last).
	while (uPicks < uCap && ((uSi < uStab) || (uSei < uSec) || (uNi < uNorm)))
	{
		if (uSi < uStab)		{ auPicks[uPicks++] = auStab[uSi++]; }
		else if (uSei < uSec)	{ auPicks[uPicks++] = auSec[uSei++]; }
		else if (uNi < uNorm)	{ auPicks[uPicks++] = auNorm[uNi++]; }
	}

	ZM_Learnset xOut;
	xOut.m_uCount = uPicks;
	for (u_int k = 0u; k < uPicks; ++k)
	{
		const u_int uLevel = (uPicks > 1u) ? (1u + (k * 49u) / (uPicks - 1u)) : 1u;
		xOut.m_axMoves[k].m_uLevel = uLevel;
		xOut.m_axMoves[k].m_eMove = (ZM_MOVE_ID)auPicks[k];
	}
	return xOut;
}
