#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_GameState.h"

// ============================================================================
// ZM_GameState -- the NEW GAME seed. Pure (no RNG, no ECS, no I/O). It seeds the
// economy and NOTHING ELSE: the party and the dex belong to ZM_ApplyStarterChoice
// (ZM_StarterChoice.h), which owns the one starter table this game has. The
// species and the grant level live THERE, never here -- a second file-static copy
// of either is exactly the drift S8 item 1 SC3 exists to remove.
// ============================================================================

// Starting economy (S6 item 2 SC3). Ruled placeholder values (plan Q-C): enough to
// buy a few more balls without trivialising the first route. Real economy tuning --
// prices, drop rates, the actual starting purse -- is S11.
static const u_int      uZM_STARTER_MONEY      = 3000u;
static const ZM_ITEM_ID eZM_STARTER_BALL       = ZM_ITEM_CATCHORB;   // BALL pocket, buy 200 / sell 100
static const u_int      uZM_STARTER_BALL_COUNT = 5u;
static const ZM_ITEM_ID eZM_STARTER_HEAL       = ZM_ITEM_SALVE;      // MEDICINE pocket, buy 100 / sell 50
static const u_int      uZM_STARTER_HEAL_COUNT = 3u;

ZM_GameState ZM_MakeNewGameState()
{
	ZM_GameState xState;
	xState.m_bPendingWhiteout = false;

	// Through the helper, not a direct write: the seed can then never place the purse
	// above the cap that AddMoney is the sole enforcer of.
	xState.AddMoney(uZM_STARTER_MONEY);
	xState.m_xBag.Add(eZM_STARTER_BALL, uZM_STARTER_BALL_COUNT);
	xState.m_xBag.Add(eZM_STARTER_HEAL, uZM_STARTER_HEAL_COUNT);
	return xState;
}
