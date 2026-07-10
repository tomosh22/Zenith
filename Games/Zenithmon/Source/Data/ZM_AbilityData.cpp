#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"

// ============================================================================
// ZM_AbilityData -- the ~50-ability roster (DecisionLog ZM-D-026). Rows are in
// ZM_ABILITY_ID order; s_axAbilities[i].m_eId == i is asserted by the tests. The
// m_uHookMask column names the battle hooks each ability will implement in S2;
// the fn-pointer bodies live there. Column legend: id, "name", "description", hooks.
// ============================================================================

namespace
{
	const ZM_AbilityData s_axAbilities[ZM_ABILITY_COUNT] =
	{
		{ ZM_ABILITY_VERDANTSURGE, "Verdant Surge","Powers up Grass moves in a pinch.",           ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT },
		{ ZM_ABILITY_EMBERSURGE,   "Ember Surge",  "Powers up Fire moves in a pinch.",            ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT },
		{ ZM_ABILITY_TIDALSURGE,   "Tidal Surge",  "Powers up Water moves in a pinch.",           ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT },
		{ ZM_ABILITY_HIVESURGE,    "Hive Surge",   "Powers up Swarm moves in a pinch.",           ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT },
		{ ZM_ABILITY_DAUNTINGROAR, "Daunting Roar","Lowers the foe's Attack on entry.",           ZM_ABILITY_HOOK_SWITCH_IN | ZM_ABILITY_HOOK_MODIFY_STAT },
		{ ZM_ABILITY_SKYWARDGRACE, "Skyward Grace","Immune to Earth-type moves.",                 ZM_ABILITY_HOOK_TYPE_IMMUNITY },
		{ ZM_ABILITY_BEDROCK,      "Bedrock",      "Survives a one-hit KO from full HP.",         ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN },
		{ ZM_ABILITY_STATICVEIL,   "Staticveil",   "Contact may paralyze the attacker.",          ZM_ABILITY_HOOK_CONTACT },
		{ ZM_ABILITY_CINDERSKIN,   "Cinderskin",   "Contact may burn the attacker.",              ZM_ABILITY_HOOK_CONTACT },
		{ ZM_ABILITY_BARBSKIN,     "Barbskin",     "Contact may poison the attacker.",            ZM_ABILITY_HOOK_CONTACT },
		{ ZM_ABILITY_THORNMAIL,    "Thornmail",    "Contact chips the attacker's HP.",            ZM_ABILITY_HOOK_CONTACT },
		{ ZM_ABILITY_SUNCHASER,    "Sunchaser",    "Doubles Speed under harsh sun.",              ZM_ABILITY_HOOK_MODIFY_STAT | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_STREAMLINE,   "Streamline",   "Doubles Speed in rain.",                      ZM_ABILITY_HOOK_MODIFY_STAT | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_GRITSTRIDE,   "Gritstride",   "Doubles Speed in a sandstorm.",               ZM_ABILITY_HOOK_MODIFY_STAT | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_RIMESTRIDE,   "Rimestride",   "Doubles Speed in snow.",                      ZM_ABILITY_HOOK_MODIFY_STAT | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_RAINCALLER,   "Raincaller",   "Summons rain on entry.",                      ZM_ABILITY_HOOK_SWITCH_IN | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_SUNCALLER,    "Suncaller",    "Summons harsh sun on entry.",                 ZM_ABILITY_HOOK_SWITCH_IN | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_SANDCALLER,   "Sandcaller",   "Summons a sandstorm on entry.",               ZM_ABILITY_HOOK_SWITCH_IN | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_SNOWCALLER,   "Snowcaller",   "Summons snow on entry.",                      ZM_ABILITY_HOOK_SWITCH_IN | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_FERVOR,       "Fervor",       "Boosts Attack when afflicted by status.",     ZM_ABILITY_HOOK_MODIFY_STAT },
		{ ZM_ABILITY_BLUBBER,      "Blubber",      "Halves Fire and Ice damage taken.",           ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN },
		{ ZM_ABILITY_AQUIFER,      "Aquifer",      "Heals instead of taking Water damage.",       ZM_ABILITY_HOOK_TYPE_IMMUNITY },
		{ ZM_ABILITY_DYNAMO,       "Dynamo",       "Heals instead of taking Electric damage.",    ZM_ABILITY_HOOK_TYPE_IMMUNITY },
		{ ZM_ABILITY_CINDERDRINK,  "Cinderdrink",  "Absorbs Fire, powering its own.",             ZM_ABILITY_HOOK_TYPE_IMMUNITY | ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT },
		{ ZM_ABILITY_GRAZER,       "Grazer",       "Heals instead of taking Grass damage.",       ZM_ABILITY_HOOK_TYPE_IMMUNITY },
		{ ZM_ABILITY_IRONWILL,     "Ironwill",     "Its stats can't be lowered by foes.",         ZM_ABILITY_HOOK_MODIFY_STAT },
		{ ZM_ABILITY_KEENEYE,      "Keeneye",      "Its accuracy can't be lowered.",              ZM_ABILITY_HOOK_ACCURACY },
		{ ZM_ABILITY_DEADAIM,      "Deadaim",      "Its moves never miss.",                       ZM_ABILITY_HOOK_ACCURACY },
		{ ZM_ABILITY_WAKEFUL,      "Wakeful",      "Cannot fall asleep.",                         ZM_ABILITY_HOOK_STATUS_TRY },
		{ ZM_ABILITY_PUREBLOOD,    "Pureblood",    "Cannot be poisoned.",                         ZM_ABILITY_HOOK_STATUS_TRY },
		{ ZM_ABILITY_THAWHEART,    "Thawheart",    "Cannot be frozen.",                           ZM_ABILITY_HOOK_STATUS_TRY },
		{ ZM_ABILITY_LIMBERLITHE,  "Limberlithe",  "Cannot be paralyzed.",                        ZM_ABILITY_HOOK_STATUS_TRY },
		{ ZM_ABILITY_OWNPACE,      "Ownpace",      "Cannot be confused.",                         ZM_ABILITY_HOOK_STATUS_TRY },
		{ ZM_ABILITY_COLDBLOOD,    "Coldblood",    "Cannot be burned.",                           ZM_ABILITY_HOOK_STATUS_TRY },
		{ ZM_ABILITY_BLOODRUSH,    "Bloodrush",    "Attack rises after it downs a foe.",          ZM_ABILITY_HOOK_FAINT | ZM_ABILITY_HOOK_MODIFY_STAT },
		{ ZM_ABILITY_LASTSPITE,    "Lastspite",    "Saps the attacker's PP if downed by contact.",ZM_ABILITY_HOOK_FAINT | ZM_ABILITY_HOOK_CONTACT },
		{ ZM_ABILITY_AFTERSHOCK,   "Aftershock",   "Hurts the attacker if downed by contact.",    ZM_ABILITY_HOOK_FAINT | ZM_ABILITY_HOOK_CONTACT },
		{ ZM_ABILITY_SOLIDCORE,    "Solidcore",    "Takes less from super-effective hits.",       ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN },
		{ ZM_ABILITY_HEAVYPLATE,   "Heavyplate",   "Takes less physical damage.",                 ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN },
		{ ZM_ABILITY_GOSSAMER,     "Gossamer",     "Takes less special damage.",                  ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN },
		{ ZM_ABILITY_DOWNDRAFT,    "Downdraft",    "Weakens incoming Sky moves.",                 ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN },
		{ ZM_ABILITY_RAINBASK,     "Rainbask",     "Recovers HP each turn in rain.",              ZM_ABILITY_HOOK_TURN_END | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_SUNBASK,      "Sunbask",      "Recovers HP each turn in sun.",               ZM_ABILITY_HOOK_TURN_END | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_ICEBOUND,     "Icebound",     "Recovers HP each turn in snow.",              ZM_ABILITY_HOOK_TURN_END | ZM_ABILITY_HOOK_WEATHER },
		{ ZM_ABILITY_TOXICTHRIVE,  "Toxicthrive",  "Recovers HP each turn when poisoned.",        ZM_ABILITY_HOOK_TURN_END },
		{ ZM_ABILITY_ROOTFEED,     "Rootfeed",     "Recovers a little HP each turn.",             ZM_ABILITY_HOOK_TURN_END },
		{ ZM_ABILITY_QUICKDRAW,    "Quickdraw",    "Sometimes strikes first.",                    ZM_ABILITY_HOOK_MODIFY_STAT },
		{ ZM_ABILITY_PRESSUREAURA, "Pressure Aura","The foe's moves cost extra PP.",              ZM_ABILITY_HOOK_SWITCH_IN },
		{ ZM_ABILITY_GUARDIAN,     "Guardian",     "Blocks the foe's stat-lowering moves.",       ZM_ABILITY_HOOK_MODIFY_STAT },
		{ ZM_ABILITY_TRUESHOT,     "Trueshot",     "Its moves bypass accuracy in weather.",       ZM_ABILITY_HOOK_ACCURACY | ZM_ABILITY_HOOK_WEATHER },
	};
}

const ZM_AbilityData& ZM_GetAbilityData(ZM_ABILITY_ID eId)
{
	Zenith_Assert(eId < ZM_ABILITY_COUNT, "ZM_GetAbilityData: ability id out of range (%u)", (u_int)eId);
	return s_axAbilities[eId];
}

u_int ZM_GetAbilityCount()
{
	return ZM_ABILITY_COUNT;
}

const char* ZM_GetAbilityName(ZM_ABILITY_ID eId)
{
	if (eId >= ZM_ABILITY_COUNT)
	{
		return "NONE";
	}
	return s_axAbilities[eId].m_szName;
}

bool ZM_AbilityHasHook(ZM_ABILITY_ID eId, ZM_ABILITY_HOOK eHook)
{
	return (ZM_GetAbilityData(eId).m_uHookMask & (u_int)eHook) != 0u;
}
