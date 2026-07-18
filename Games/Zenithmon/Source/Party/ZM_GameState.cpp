#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_GameState.h"

// ============================================================================
// ZM_GameState -- the starter seed. Pure (no RNG, no ECS, no I/O). The starter is
// continuous with the item-4 placeholder player (Fernfawn L5, ZM_BattleDirector
// BuildPlaceholderPlayerSpec); a real lab/starter-choice arrives at S8 (D4).
// ============================================================================

static const ZM_SPECIES_ID eZM_STARTER_SPECIES = ZM_SPECIES_FERNFAWN;
static const u_int          uZM_STARTER_LEVEL   = 5u;

ZM_GameState ZM_MakeStarterGameState()
{
	ZM_GameState xState;
	const ZM_Monster xStarter = ZM_BuildMonsterRecord(eZM_STARTER_SPECIES, uZM_STARTER_LEVEL);
	xState.m_xParty.Add(xStarter);
	xState.MarkCaught(eZM_STARTER_SPECIES);
	xState.m_bPendingWhiteout = false;
	return xState;
}
