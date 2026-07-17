#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"     // ZM_GetSpeciesLearnset, ZM_Learnset
#include "Zenithmon/Source/Data/ZM_MoveData.h"       // ZM_MOVE_NONE, ZM_MOVE_ID

// ============================================================================
// ZM_BattleDirectorCore implementation (S5 item 4, ZM-D-101). Pure/headless:
// engine + state machine + event cursor + the total event->op table. No ECS, no
// graphics, no scene.
// ============================================================================

namespace
{
	// Per-op base durations (wall-clock seconds). zm_instant_battles collapses each
	// to 0 in Tick, so a whole turn/intro drains in one call.
	static const float fZM_OP_DUR_TEXT   = 0.8f;
	static const float fZM_OP_DUR_ANIM   = 0.5f;
	static const float fZM_OP_DUR_HP     = 0.6f;
	static const float fZM_OP_DUR_REVEAL = 0.4f;
	static const float fZM_OP_DUR_FAINT  = 0.7f;
	static const float fZM_OP_DUR_BALL   = 0.9f;
	static const float fZM_OP_DUR_EXP    = 0.7f;

	// A fixed odd salt (the golden-ratio 64-bit constant) mixed into the battle seed
	// so the AI's RNG stream is a DISTINCT sequence from the battle RNG's -- choosing
	// an action can never coincide with a battle draw (non-perturbation contract).
	static const u_int64 ulZM_AI_RNG_SALT = 0x9E3779B97F4A7C15ull;

	// Process-lifetime flag (ALL configs). A tools DebugVariable binds its ref; tests
	// flip it via ZM_SetInstantBattlesForTests. Default OFF -> normal timed presentation.
	static bool s_bInstantBattles = false;
}

bool  ZM_InstantBattlesEnabled()                    { return s_bInstantBattles; }
void  ZM_SetInstantBattlesForTests(bool bEnabled)   { s_bInstantBattles = bEnabled; }
bool& ZM_InstantBattlesRef()                        { return s_bInstantBattles; }

u_int64 ZM_DeriveAiRngSeed(u_int64 ulBattleSeed)
{
	// Distinct-stream derivation: XOR the battle seed with a fixed odd salt. Pure and
	// deterministic, so a test can reproduce the enemy picks by seeding an AI rng the
	// same way. The salt guarantees the AI seed != the battle seed for every input.
	return ulBattleSeed ^ ulZM_AI_RNG_SALT;
}

ZM_BattlePresentationOp ZM_MapEventToOp(const ZM_BattleEvent& xEvent)
{
	// TOTAL table: every real ZM_BATTLE_EVENT kind has an explicit case. The default
	// is only reached by an out-of-range value (e.g. ZM_BATTLE_EVENT_COUNT) and is a
	// safe no-present op.
	switch (xEvent.m_eKind)
	{
	// --- framing events: nothing to present ---
	case ZM_BATTLE_EVENT_BATTLE_BEGIN:
	case ZM_BATTLE_EVENT_TURN_BEGIN:
	case ZM_BATTLE_EVENT_TURN_END:
	case ZM_BATTLE_EVENT_EVOLUTION_QUEUED:   // evolution cutscene is post-battle, not in-battle
		return { ZM_POP_NONE, 0.0f, false };

	// --- a creature entering ---
	case ZM_BATTLE_EVENT_SWITCH_IN:
		return { ZM_POP_MODEL_REVEAL, fZM_OP_DUR_REVEAL, true };

	// --- an attack/move animation ---
	case ZM_BATTLE_EVENT_MOVE_USED:
		return { ZM_POP_ANIM, fZM_OP_DUR_ANIM, true };

	// --- text-only lines ---
	case ZM_BATTLE_EVENT_MOVE_MISSED:
	case ZM_BATTLE_EVENT_CRIT:
	case ZM_BATTLE_EVENT_SUPER_EFFECTIVE:
	case ZM_BATTLE_EVENT_NOT_EFFECTIVE:
	case ZM_BATTLE_EVENT_IMMUNE:
	case ZM_BATTLE_EVENT_NO_PP:
	case ZM_BATTLE_EVENT_MOVE_FAILED:
	case ZM_BATTLE_EVENT_STATUS_APPLIED:
	case ZM_BATTLE_EVENT_STATUS_CURED:
	case ZM_BATTLE_EVENT_STAT_STAGE_CHANGED:
	case ZM_BATTLE_EVENT_VOLATILE_APPLIED:
	case ZM_BATTLE_EVENT_VOLATILE_ENDED:
	case ZM_BATTLE_EVENT_FLINCH:
	case ZM_BATTLE_EVENT_MULTI_HIT:
	case ZM_BATTLE_EVENT_ABILITY_TRIGGER:
	case ZM_BATTLE_EVENT_WEATHER_CHANGED:
	case ZM_BATTLE_EVENT_SCREEN_SET:
	case ZM_BATTLE_EVENT_SCREEN_EXPIRED:
	case ZM_BATTLE_EVENT_LEVEL_UP:
	case ZM_BATTLE_EVENT_MOVE_LEARNED:
	case ZM_BATTLE_EVENT_FLEE:
	case ZM_BATTLE_EVENT_FLEE_FAILED:
		return { ZM_POP_TEXT, fZM_OP_DUR_TEXT, true };

	// --- HP tween, no line (the damage number IS the presentation) ---
	case ZM_BATTLE_EVENT_DAMAGE_DEALT:
		return { ZM_POP_HP_TWEEN, fZM_OP_DUR_HP, false };

	// --- HP tween + a line ---
	case ZM_BATTLE_EVENT_STATUS_DAMAGE:
	case ZM_BATTLE_EVENT_WEATHER_DAMAGE:
	case ZM_BATTLE_EVENT_HEAL:
	case ZM_BATTLE_EVENT_DRAIN:
	case ZM_BATTLE_EVENT_RECOIL:
		return { ZM_POP_HP_TWEEN, fZM_OP_DUR_HP, true };

	// --- faint animation + a line ---
	case ZM_BATTLE_EVENT_FAINT:
		return { ZM_POP_FAINT_FALL, fZM_OP_DUR_FAINT, true };

	// --- battle-end banner line ---
	case ZM_BATTLE_EVENT_BATTLE_END:
		return { ZM_POP_TEXT, fZM_OP_DUR_TEXT, true };

	// --- exp bar tween + a line ---
	case ZM_BATTLE_EVENT_EXP_GAINED:
		return { ZM_POP_EXP_TWEEN, fZM_OP_DUR_EXP, true };

	// --- ball arc / wobble, then result line ---
	case ZM_BATTLE_EVENT_CATCH_SHAKE:
		return { ZM_POP_BALL, fZM_OP_DUR_BALL, false };
	case ZM_BATTLE_EVENT_CATCH_RESULT:
		return { ZM_POP_BALL, fZM_OP_DUR_BALL, true };

	case ZM_BATTLE_EVENT_COUNT:
	default:
		return { ZM_POP_NONE, 0.0f, false };
	}
}

ZM_BattleMonsterSpec ZM_BuildWildEnemySpec(ZM_SPECIES_ID eSpecies, u_int uLevel)
{
	// Default spec keeps IVs 31 / EVs 0 / nature FERAL / ability NONE / exp UNSPECIFIED.
	ZM_BattleMonsterSpec xSpec;
	xSpec.m_eSpecies = eSpecies;
	xSpec.m_uLevel   = uLevel;

	// Fill moves by MIRRORING g_FillTowerMoves (ZM_BattleTower.cpp) but against uLevel
	// (not the L50 clamp): collect every learnset entry with level <= uLevel in learn
	// order; keep the LAST up-to-four (highest-level); remaining slots stay MOVE_NONE.
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		xSpec.m_aeMoves[i] = ZM_MOVE_NONE;
	}

	const ZM_Learnset xLearnset = ZM_GetSpeciesLearnset(eSpecies);
	ZM_MOVE_ID aeEligible[uZM_MAX_LEARNSET_SIZE];
	u_int uEligible = 0u;
	for (u_int k = 0u; k < xLearnset.m_uCount; ++k)
	{
		if (xLearnset.m_axMoves[k].m_uLevel <= uLevel)
		{
			aeEligible[uEligible++] = xLearnset.m_axMoves[k].m_eMove;
		}
	}

	const u_int uKeep  = uEligible > uZM_MAX_MOVES ? uZM_MAX_MOVES : uEligible;
	const u_int uStart = uEligible - uKeep;
	for (u_int i = 0u; i < uKeep; ++i)
	{
		xSpec.m_aeMoves[i] = aeEligible[uStart + i];
	}

	return xSpec;
}

void ZM_BattleDirectorCore::Begin(const ZM_BattleMonsterSpec* paxPlayer, u_int uPlayerCount,
	const ZM_BattleMonsterSpec* paxEnemy, u_int uEnemyCount,
	const ZM_BattleConfig& xConfig, u_int64 ulSeed, ZM_AI_TIER eEnemyTier)
{
	m_xEngine.Begin(xConfig, paxPlayer, uPlayerCount, paxEnemy, uEnemyCount, ulSeed);

	// Seed the AI's OWN generator from the distinct-stream derivation, so calling
	// ZM_ChooseAction never advances the battle RNG (non-perturbation).
	m_xAIRng.Seed(ZM_DeriveAiRngSeed(ulSeed));
	m_eEnemyTier = eEnemyTier;

	// Present the intro range [0, eventCount): BATTLE_BEGIN + both SWITCH_INs.
	m_uCursor        = 0u;
	m_uTurnEndCursor = m_xEngine.GetEventCount();
	m_fOpElapsed     = 0.0f;
	m_eState         = ZM_DIRECTOR_PLAYING_EVENTS;
}

void ZM_BattleDirectorCore::SubmitPlayerAction(const ZM_BattleAction& xPlayerAction)
{
	Zenith_Assert(m_eState == ZM_DIRECTOR_AWAIT_INPUT,
		"SubmitPlayerAction only valid while awaiting input (state %u)", (u_int)m_eState);

	// Singles: the one opposing active replies. Chosen from the current state with the
	// director's own AI rng -- byte-identical to a hand drive that seeds an AI rng the
	// same way and chooses at the same point.
	const ZM_BattleAction xEnemy = ZM_ChooseAction(m_xEngine.GetState(), ZM_SIDE_ENEMY, m_eEnemyTier, m_xAIRng);

	m_xEngine.SubmitAction(ZM_SIDE_PLAYER, xPlayerAction);
	m_xEngine.SubmitAction(ZM_SIDE_ENEMY, xEnemy);

	const u_int uStart = m_xEngine.GetEventCount();
	m_xEngine.ResolveTurn();

	m_uCursor        = uStart;
	m_uTurnEndCursor = m_xEngine.GetEventCount();
	m_fOpElapsed     = 0.0f;
	m_eState         = ZM_DIRECTOR_PLAYING_EVENTS;
}

void ZM_BattleDirectorCore::Tick(float fDeltaSeconds)
{
	if (m_eState != ZM_DIRECTOR_PLAYING_EVENTS)
	{
		return;
	}

	m_fOpElapsed += fDeltaSeconds;
	while (m_uCursor < m_uTurnEndCursor)
	{
		const ZM_BattlePresentationOp xOp = ZM_MapEventToOp(m_xEngine.GetEvent(m_uCursor));
		const float fDur = ZM_InstantBattlesEnabled() ? 0.0f : xOp.m_fBaseDurationSeconds;
		if (m_fOpElapsed < fDur)
		{
			break;   // still presenting this op
		}
		m_fOpElapsed -= fDur;   // carry the remainder into the next op
		++m_uCursor;
	}

	if (m_uCursor >= m_uTurnEndCursor)
	{
		m_fOpElapsed = 0.0f;
		m_eState = m_xEngine.IsOver() ? ZM_DIRECTOR_OVER : ZM_DIRECTOR_AWAIT_INPUT;
	}
}

ZM_BattlePresentationOp ZM_BattleDirectorCore::CurrentOp() const
{
	if (m_eState == ZM_DIRECTOR_PLAYING_EVENTS && m_uCursor < m_uTurnEndCursor)
	{
		return ZM_MapEventToOp(m_xEngine.GetEvent(m_uCursor));
	}
	return ZM_BattlePresentationOp{};
}

const ZM_BattleEvent* ZM_BattleDirectorCore::CurrentEvent() const
{
	if (m_eState == ZM_DIRECTOR_PLAYING_EVENTS && m_uCursor < m_uTurnEndCursor)
	{
		return &m_xEngine.GetEvent(m_uCursor);
	}
	return nullptr;
}

u_int ZM_BattleDirectorCore::SideActiveHP(ZM_SIDE eSide) const
{
	return m_xEngine.GetState().Side(eSide).Active().m_uCurHP;
}

u_int ZM_BattleDirectorCore::SideActiveMaxHP(ZM_SIDE eSide) const
{
	return m_xEngine.GetState().Side(eSide).Active().m_auMaxStat[ZM_STAT_HP];
}

u_int ZM_BattleDirectorCore::SideActiveLevel(ZM_SIDE eSide) const
{
	return m_xEngine.GetState().Side(eSide).Active().m_uLevel;
}

ZM_SPECIES_ID ZM_BattleDirectorCore::SideActiveSpecies(ZM_SIDE eSide) const
{
	return m_xEngine.GetState().Side(eSide).Active().m_eSpecies;
}
