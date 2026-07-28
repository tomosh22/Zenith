#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_TrainerBattle.h"

#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"   // ZM_BuildWildEnemySpec

// ============================================================================
// ZM_TrainerBattle (S7 item 3 SC5) -- the pure trainer-battle input builder.
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS: Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration, the whole unit suite runs at boot,
// and the boot units feed these functions the ZM_TRAINER_NONE sentinel and
// out-of-range ids ON PURPOSE to pin their fail-closed answers.
// ============================================================================

namespace
{
	// A trainer-domain salt folded BEFORE the id. Without it,
	// ZM_DeriveTrainerBattleSeed(id) would be byte-identical to
	// ZM_BattleDirector::DeriveBattleSeed(ZM_SPECIES_ID(0), id) -- the same basis,
	// the same prime, the same two folds -- and a rival battle would replay a wild
	// encounter's RNG stream. A unit pins the disjointness over a grid.
	const u_int64 ulZM_TRAINER_SEED_DOMAIN = 0x5A4D5452414E5231ull;   // "ZMTRANR1"
	const u_int64 ulZM_FNV_OFFSET_BASIS    = 0xCBF29CE484222325ull;
	const u_int64 ulZM_FNV_PRIME           = 0x100000001B3ull;
}

u_int64 ZM_DeriveTrainerBattleSeed(ZM_TRAINER_ID eTrainer)
{
	u_int64 ulHash = ulZM_FNV_OFFSET_BASIS;
	ulHash = (ulHash ^ ulZM_TRAINER_SEED_DOMAIN) * ulZM_FNV_PRIME;
	ulHash = (ulHash ^ static_cast<u_int64>(eTrainer)) * ulZM_FNV_PRIME;
	return ulHash;
}

ZM_TrainerBattleSetup ZM_BuildTrainerBattleSetup(ZM_TRAINER_ID eTrainer)
{
	ZM_TrainerBattleSetup xSetup;   // every field already the inert answer

	// The registered check comes FIRST and deliberately does not go through
	// ZM_GetTrainerData: that accessor logs a non-fatal Zenith_Error for an
	// unregistered id, and the boot unit that pins this function's totality feeds
	// it the sentinel on purpose. A total no-op has to be SILENT.
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		return xSetup;
	}

	const ZM_TrainerData& xRow = ZM_GetTrainerData(eTrainer);
	if (xRow.m_paxParty == nullptr || xRow.m_uPartyCount == 0u)
	{
		return xSetup;   // a trainer with no team cannot battle
	}

	// CLAMP HERE OR NOWHERE: ZM_BattleEngine::Begin loops uEnemyCount unbounded and
	// ZM_BattleDirectorCore::Begin forwards the count verbatim. The bound is the
	// count the ENGINE can resolve, NOT the row cap -- the engine has no forced
	// replacement on faint, so a fielded bench ends in a Zenith_Assert the moment
	// the enemy active is KO'd (uZM_TRAINER_BATTLEABLE_PARTY's comment spells the
	// whole chain out). Members are taken from the FRONT, so the row's authored
	// lead is the monster fielded.
	u_int uCount = xRow.m_uPartyCount;
	if (uCount > uZM_TRAINER_BATTLEABLE_PARTY)
	{
		uCount = uZM_TRAINER_BATTLEABLE_PARTY;
	}

	for (u_int u = 0u; u < uCount; ++u)
	{
		xSetup.m_axEnemyParty[u] = ZM_BuildWildEnemySpec(
			xRow.m_paxParty[u].m_eSpecies, xRow.m_paxParty[u].m_uLevel);
	}

	xSetup.m_uEnemyCount  = uCount;
	xSetup.m_eEnemyTier   = xRow.m_eAITier;
	xSetup.m_ulBattleSeed = ZM_DeriveTrainerBattleSeed(eTrainer);
	xSetup.m_eLeadSpecies = xSetup.m_axEnemyParty[0].m_eSpecies;
	xSetup.m_bValid       = true;
	return xSetup;
}
