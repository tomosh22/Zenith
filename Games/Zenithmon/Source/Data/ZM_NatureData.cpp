#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"

// ============================================================================
// ZM_NatureData -- the 25-nature table (DecisionLog ZM-D-025). Rows are in
// ZM_NATURE order; s_axNatures[i].m_eId == i is asserted by the tests. The 25
// rows are exactly the 5x5 grid of (raised, lowered) stat pairs over the five
// non-HP stats; the diagonal (raised == lowered) is the five neutral natures.
// ============================================================================

namespace
{
	const ZM_NatureData s_axNatures[ZM_NATURE_COUNT] =
	{
		// raised ATTACK
		{ ZM_NATURE_FERAL,     "Feral",     ZM_STAT_ATTACK,    ZM_STAT_ATTACK    },
		{ ZM_NATURE_RECKLESS,  "Reckless",  ZM_STAT_ATTACK,    ZM_STAT_DEFENSE   },
		{ ZM_NATURE_BRUTISH,   "Brutish",   ZM_STAT_ATTACK,    ZM_STAT_SPATTACK  },
		{ ZM_NATURE_ROWDY,     "Rowdy",     ZM_STAT_ATTACK,    ZM_STAT_SPDEFENSE },
		{ ZM_NATURE_HULKING,   "Hulking",   ZM_STAT_ATTACK,    ZM_STAT_SPEED     },
		// raised DEFENSE
		{ ZM_NATURE_GUARDED,   "Guarded",   ZM_STAT_DEFENSE,   ZM_STAT_ATTACK    },
		{ ZM_NATURE_STOLID,    "Stolid",    ZM_STAT_DEFENSE,   ZM_STAT_DEFENSE   },
		{ ZM_NATURE_IRONBOUND, "Ironbound", ZM_STAT_DEFENSE,   ZM_STAT_SPATTACK  },
		{ ZM_NATURE_SHELTERED, "Sheltered", ZM_STAT_DEFENSE,   ZM_STAT_SPDEFENSE },
		{ ZM_NATURE_PONDEROUS, "Ponderous", ZM_STAT_DEFENSE,   ZM_STAT_SPEED     },
		// raised SPATTACK
		{ ZM_NATURE_ARCANE,    "Arcane",    ZM_STAT_SPATTACK,  ZM_STAT_ATTACK    },
		{ ZM_NATURE_MYSTIC,    "Mystic",    ZM_STAT_SPATTACK,  ZM_STAT_DEFENSE   },
		{ ZM_NATURE_PENSIVE,   "Pensive",   ZM_STAT_SPATTACK,  ZM_STAT_SPATTACK  },
		{ ZM_NATURE_ZEALOUS,   "Zealous",   ZM_STAT_SPATTACK,  ZM_STAT_SPDEFENSE },
		{ ZM_NATURE_BROODING,  "Brooding",  ZM_STAT_SPATTACK,  ZM_STAT_SPEED     },
		// raised SPDEFENSE
		{ ZM_NATURE_PLACID,    "Placid",    ZM_STAT_SPDEFENSE, ZM_STAT_ATTACK    },
		{ ZM_NATURE_SERENE,    "Serene",    ZM_STAT_SPDEFENSE, ZM_STAT_DEFENSE   },
		{ ZM_NATURE_DEVOUT,    "Devout",    ZM_STAT_SPDEFENSE, ZM_STAT_SPATTACK  },
		{ ZM_NATURE_TRANQUIL,  "Tranquil",  ZM_STAT_SPDEFENSE, ZM_STAT_SPDEFENSE },
		{ ZM_NATURE_PATIENT,   "Patient",   ZM_STAT_SPDEFENSE, ZM_STAT_SPEED     },
		// raised SPEED
		{ ZM_NATURE_NIMBLE,    "Nimble",    ZM_STAT_SPEED,     ZM_STAT_ATTACK    },
		{ ZM_NATURE_FLEET,     "Fleet",     ZM_STAT_SPEED,     ZM_STAT_DEFENSE   },
		{ ZM_NATURE_DARTING,   "Darting",   ZM_STAT_SPEED,     ZM_STAT_SPATTACK  },
		{ ZM_NATURE_SKITTISH,  "Skittish",  ZM_STAT_SPEED,     ZM_STAT_SPDEFENSE },
		{ ZM_NATURE_RESTLESS,  "Restless",  ZM_STAT_SPEED,     ZM_STAT_SPEED     },
	};
}

const ZM_NatureData& ZM_GetNatureData(ZM_NATURE eId)
{
	Zenith_Assert(eId < ZM_NATURE_COUNT, "ZM_GetNatureData: nature id out of range (%u)", (u_int)eId);
	return s_axNatures[eId];
}

u_int ZM_GetNatureCount()
{
	return ZM_NATURE_COUNT;
}

const char* ZM_GetNatureName(ZM_NATURE eId)
{
	if (eId >= ZM_NATURE_COUNT)
	{
		return "NONE";
	}
	return s_axNatures[eId].m_szName;
}

u_int ZM_GetNatureStatPercent(ZM_NATURE eNature, ZM_STAT eStat)
{
	const ZM_NatureData& xData = ZM_GetNatureData(eNature);

	// A neutral nature (raised == lowered) never changes anything.
	if (xData.m_eRaised == xData.m_eLowered)
	{
		return 100u;
	}
	if (eStat == xData.m_eRaised)
	{
		return 110u;
	}
	if (eStat == xData.m_eLowered)
	{
		return 90u;
	}
	return 100u;
}
