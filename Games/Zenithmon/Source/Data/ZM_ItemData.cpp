#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"

// ============================================================================
// ZM_ItemData -- the ~90-item table (DecisionLog ZM-D-024). Rows are in
// ZM_ITEM_ID order; s_axItems[i].m_eId == i is asserted by the tests. See the
// header for the per-field contract. Column legend:
//   id, "name", category, buy, sell, effect, effectParam, consumable, taughtMove
// (Balls carry CATCH; TMs carry TEACH_MOVE + a real m_eTaughtMove; key items are
//  priceless + effectless. All invariants are locked by ZM_Tests_Items.)
// ============================================================================

namespace
{
	const ZM_ItemData s_axItems[ZM_ITEM_COUNT] =
	{
		// Balls -- capture devices (effect CATCH, param = catch multiplier x10)
		{ ZM_ITEM_CATCHORB,        "Catch Orb",       ZM_ITEM_CATEGORY_BALL,      200,  100, ZM_ITEM_EFFECT_CATCH,              10, true , ZM_MOVE_NONE },
		{ ZM_ITEM_GREATORB,        "Great Orb",       ZM_ITEM_CATEGORY_BALL,      600,  300, ZM_ITEM_EFFECT_CATCH,              15, true , ZM_MOVE_NONE },
		{ ZM_ITEM_ULTRAORB,        "Ultra Orb",       ZM_ITEM_CATEGORY_BALL,     1200,  600, ZM_ITEM_EFFECT_CATCH,              20, true , ZM_MOVE_NONE },
		{ ZM_ITEM_HEALORB,         "Heal Orb",        ZM_ITEM_CATEGORY_BALL,      300,  150, ZM_ITEM_EFFECT_CATCH,              10, true , ZM_MOVE_NONE },
		{ ZM_ITEM_NETORB,          "Net Orb",         ZM_ITEM_CATEGORY_BALL,     1000,  500, ZM_ITEM_EFFECT_CATCH,              10, true , ZM_MOVE_NONE },
		{ ZM_ITEM_DUSKORB,         "Dusk Orb",        ZM_ITEM_CATEGORY_BALL,     1000,  500, ZM_ITEM_EFFECT_CATCH,              10, true , ZM_MOVE_NONE },
		{ ZM_ITEM_QUICKORB,        "Quick Orb",       ZM_ITEM_CATEGORY_BALL,     1000,  500, ZM_ITEM_EFFECT_CATCH,              10, true , ZM_MOVE_NONE },
		{ ZM_ITEM_PRIMEORB,        "Prime Orb",       ZM_ITEM_CATEGORY_BALL,        0,    0, ZM_ITEM_EFFECT_CATCH,             255, true , ZM_MOVE_NONE },
		// Medicine -- HP restore (param = HP healed; FULL variants ignore it)
		{ ZM_ITEM_SALVE,           "Salve",           ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_HEAL_HP,            20, true , ZM_MOVE_NONE },
		{ ZM_ITEM_TONIC,           "Tonic",           ZM_ITEM_CATEGORY_MEDICINE,  300,  150, ZM_ITEM_EFFECT_HEAL_HP,            60, true , ZM_MOVE_NONE },
		{ ZM_ITEM_DRAUGHT,         "Elixir Draught",  ZM_ITEM_CATEGORY_MEDICINE,  700,  350, ZM_ITEM_EFFECT_HEAL_HP,           120, true , ZM_MOVE_NONE },
		{ ZM_ITEM_GRANDRESTORE,    "Grand Restore",   ZM_ITEM_CATEGORY_MEDICINE, 1200,  600, ZM_ITEM_EFFECT_HEAL_HP,           200, true , ZM_MOVE_NONE },
		{ ZM_ITEM_RENEWAL,         "Renewal",         ZM_ITEM_CATEGORY_MEDICINE, 2000, 1000, ZM_ITEM_EFFECT_HEAL_HP_FULL,        0, true , ZM_MOVE_NONE },
		// Medicine -- revive
		{ ZM_ITEM_REVIVE,          "Revive",          ZM_ITEM_CATEGORY_MEDICINE, 1500,  750, ZM_ITEM_EFFECT_REVIVE_HALF,         0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_MAXREVIVE,       "Max Revive",      ZM_ITEM_CATEGORY_MEDICINE, 4000, 2000, ZM_ITEM_EFFECT_REVIVE_FULL,         0, true , ZM_MOVE_NONE },
		// Medicine -- status cures
		{ ZM_ITEM_ANTITOXIN,       "Antitoxin",       ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_CURE_POISON,         0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_BURNBALM,        "Burn Balm",       ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_CURE_BURN,           0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_THAWSPRAY,       "Thaw Spray",      ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_CURE_FREEZE,         0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_ROUSEBELL,       "Rouse Bell",      ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_CURE_SLEEP,          0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_NUMBCURE,        "Numbcure",        ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_CURE_PARALYSIS,      0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_CLEARMIND,       "Clearmind",       ZM_ITEM_CATEGORY_MEDICINE,  100,   50, ZM_ITEM_EFFECT_CURE_CONFUSE,        0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_PANACEA,         "Panacea",         ZM_ITEM_CATEGORY_MEDICINE,  500,  250, ZM_ITEM_EFFECT_CURE_ALL_STATUS,     0, true , ZM_MOVE_NONE },
		// Medicine -- PP
		{ ZM_ITEM_ETHER,           "Ether",           ZM_ITEM_CATEGORY_MEDICINE, 1200,  600, ZM_ITEM_EFFECT_RESTORE_PP,         10, true , ZM_MOVE_NONE },
		{ ZM_ITEM_ELIXIR,          "Elixir",          ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_RESTORE_PP_ALL,      0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_PPBOOST,         "PP Boost",        ZM_ITEM_CATEGORY_MEDICINE,    0,    0, ZM_ITEM_EFFECT_PP_UP,               0, true , ZM_MOVE_NONE },
		// Vitamins -- permanent EV boost (param = stat index) + rare candy
		{ ZM_ITEM_VIGORROOT,       "Vigor Root",      ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_EV_BOOST,            0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_POWERDRAUGHT,    "Power Draught",   ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_EV_BOOST,            1, true , ZM_MOVE_NONE },
		{ ZM_ITEM_GUARDDRAUGHT,    "Guard Draught",   ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_EV_BOOST,            2, true , ZM_MOVE_NONE },
		{ ZM_ITEM_FOCUSDRAUGHT,    "Focus Draught",   ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_EV_BOOST,            3, true , ZM_MOVE_NONE },
		{ ZM_ITEM_WARDDRAUGHT,     "Ward Draught",    ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_EV_BOOST,            4, true , ZM_MOVE_NONE },
		{ ZM_ITEM_SWIFTDRAUGHT,    "Swift Draught",   ZM_ITEM_CATEGORY_MEDICINE, 3000, 1500, ZM_ITEM_EFFECT_EV_BOOST,            5, true , ZM_MOVE_NONE },
		{ ZM_ITEM_RARESWEET,       "Rare Sweet",      ZM_ITEM_CATEGORY_MEDICINE,    0,    0, ZM_ITEM_EFFECT_LEVEL_UP,            0, true , ZM_MOVE_NONE },
		// Battle items -- temporary in-battle stat boost (param = stat index)
		{ ZM_ITEM_XPOWER,          "X Power",         ZM_ITEM_CATEGORY_BATTLE,    500,  250, ZM_ITEM_EFFECT_BATTLE_STAT_BOOST,   1, true , ZM_MOVE_NONE },
		{ ZM_ITEM_XGUARD,          "X Guard",         ZM_ITEM_CATEGORY_BATTLE,    500,  250, ZM_ITEM_EFFECT_BATTLE_STAT_BOOST,   2, true , ZM_MOVE_NONE },
		{ ZM_ITEM_XSWIFT,          "X Swift",         ZM_ITEM_CATEGORY_BATTLE,    500,  250, ZM_ITEM_EFFECT_BATTLE_STAT_BOOST,   5, true , ZM_MOVE_NONE },
		// Held items -- passive battle effects (not consumed)
		{ ZM_ITEM_AFTERGROWTH,     "Aftergrowth",     ZM_ITEM_CATEGORY_HELD,     2000, 1000, ZM_ITEM_EFFECT_HELD_LEFTOVERS,      0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_FLAMECHARM,      "Flame Charm",     ZM_ITEM_CATEGORY_HELD,     1500,  750, ZM_ITEM_EFFECT_HELD_TYPE_BOOST,     1, false, ZM_MOVE_NONE },
		{ ZM_ITEM_AQUACHARM,       "Aqua Charm",      ZM_ITEM_CATEGORY_HELD,     1500,  750, ZM_ITEM_EFFECT_HELD_TYPE_BOOST,     2, false, ZM_MOVE_NONE },
		{ ZM_ITEM_LEAFCHARM,       "Leaf Charm",      ZM_ITEM_CATEGORY_HELD,     1500,  750, ZM_ITEM_EFFECT_HELD_TYPE_BOOST,     3, false, ZM_MOVE_NONE },
		{ ZM_ITEM_CHOICEFANG,      "Choice Fang",     ZM_ITEM_CATEGORY_HELD,     3000, 1500, ZM_ITEM_EFFECT_HELD_CHOICE,         1, false, ZM_MOVE_NONE },
		{ ZM_ITEM_CHOICELENS,      "Choice Lens",     ZM_ITEM_CATEGORY_HELD,     3000, 1500, ZM_ITEM_EFFECT_HELD_CHOICE,         3, false, ZM_MOVE_NONE },
		{ ZM_ITEM_CHOICEBOOTS,     "Choice Boots",    ZM_ITEM_CATEGORY_HELD,     3000, 1500, ZM_ITEM_EFFECT_HELD_CHOICE,         5, false, ZM_MOVE_NONE },
		{ ZM_ITEM_ENDURECHARM,     "Endure Charm",    ZM_ITEM_CATEGORY_HELD,     2000, 1000, ZM_ITEM_EFFECT_HELD_FOCUS_SASH,     0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_POWERORB,        "Power Orb",       ZM_ITEM_CATEGORY_HELD,     2000, 1000, ZM_ITEM_EFFECT_HELD_LIFE_ORB,       0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_JOLTBELL,        "Jolt Bell",       ZM_ITEM_CATEGORY_HELD,     2000, 1000, ZM_ITEM_EFFECT_HELD_FLINCH,         0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_HEIRLOOMKNOT,    "Heirloom Knot",   ZM_ITEM_CATEGORY_HELD,        0,    0, ZM_ITEM_EFFECT_HELD_BREED_IV,       0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_STASISSTONE,     "Stasis Stone",    ZM_ITEM_CATEGORY_HELD,        0,    0, ZM_ITEM_EFFECT_HELD_NATURE_LOCK,    0, false, ZM_MOVE_NONE },
		// Berries -- held consumables
		{ ZM_ITEM_ROSEWELLBERRY,   "Rosewell Berry",  ZM_ITEM_CATEGORY_BERRY,      20,   10, ZM_ITEM_EFFECT_BERRY_HEAL,         30, true , ZM_MOVE_NONE },
		{ ZM_ITEM_CLEANSEBERRY,    "Cleanse Berry",   ZM_ITEM_CATEGORY_BERRY,      40,   20, ZM_ITEM_EFFECT_BERRY_CURE_STATUS,   0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_EMBERPIPBERRY,   "Emberpip Berry",  ZM_ITEM_CATEGORY_BERRY,      80,   40, ZM_ITEM_EFFECT_BERRY_PINCH_BOOST,   1, true , ZM_MOVE_NONE },
		{ ZM_ITEM_FROSTPIPBERRY,   "Frostpip Berry",  ZM_ITEM_CATEGORY_BERRY,      80,   40, ZM_ITEM_EFFECT_BERRY_PINCH_BOOST,   3, true , ZM_MOVE_NONE },
		{ ZM_ITEM_BUFFERFLAME,     "Flamebuff Berry", ZM_ITEM_CATEGORY_BERRY,      80,   40, ZM_ITEM_EFFECT_BERRY_TYPE_RESIST,   1, true , ZM_MOVE_NONE },
		{ ZM_ITEM_BUFFERTIDE,      "Tidebuff Berry",  ZM_ITEM_CATEGORY_BERRY,      80,   40, ZM_ITEM_EFFECT_BERRY_TYPE_RESIST,   2, true , ZM_MOVE_NONE },
		// Evolution stones (param = stone variant)
		{ ZM_ITEM_FLARESTONE,      "Flarestone",      ZM_ITEM_CATEGORY_EVO,      3000, 1500, ZM_ITEM_EFFECT_EVOLVE,              0, true , ZM_MOVE_NONE },
		{ ZM_ITEM_TIDESTONE,       "Tidestone",       ZM_ITEM_CATEGORY_EVO,      3000, 1500, ZM_ITEM_EFFECT_EVOLVE,              1, true , ZM_MOVE_NONE },
		{ ZM_ITEM_VOLTSTONE,       "Voltstone",       ZM_ITEM_CATEGORY_EVO,      3000, 1500, ZM_ITEM_EFFECT_EVOLVE,              2, true , ZM_MOVE_NONE },
		{ ZM_ITEM_MOONSTONE,       "Moonstone",       ZM_ITEM_CATEGORY_EVO,      3000, 1500, ZM_ITEM_EFFECT_EVOLVE,              3, true , ZM_MOVE_NONE },
		{ ZM_ITEM_DUSKSTONE,       "Duskstone",       ZM_ITEM_CATEGORY_EVO,      3000, 1500, ZM_ITEM_EFFECT_EVOLVE,              4, true , ZM_MOVE_NONE },
		// Field items
		{ ZM_ITEM_WARDSCENT,       "Ward Scent",      ZM_ITEM_CATEGORY_FIELD,     350,  175, ZM_ITEM_EFFECT_REPEL,               0, true , ZM_MOVE_NONE },
		// TMs -- teach a move (effect TEACH_MOVE, m_eTaughtMove set); reusable
		{ ZM_ITEM_TM_TITANBEAM,    "TM Titan Beam",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_TITANBEAM },
		{ ZM_ITEM_TM_CLOSEBOUT,    "TM Close Bout",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_CLOSEBOUT },
		{ ZM_ITEM_TM_TOXICLANCE,   "TM Toxic Lance",  ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_TOXICLANCE },
		{ ZM_ITEM_TM_FAULTLINE,    "TM Faultline",    ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_FAULTLINE },
		{ ZM_ITEM_TM_DELUGEBEAM,   "TM Deluge Beam",  ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_DELUGEBEAM },
		{ ZM_ITEM_TM_SUNLANCE,     "TM Sunlance",     ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_SUNLANCE },
		{ ZM_ITEM_TM_THUNDERPEAL,  "TM Thunder Peal", ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_THUNDERPEAL },
		{ ZM_ITEM_TM_HAILSPEAR,    "TM Hail Spear",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_HAILSPEAR },
		{ ZM_ITEM_TM_MAGMAFALL,    "TM Magmafall",    ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_MAGMAFALL },
		{ ZM_ITEM_TM_CYCLONEPEAL,  "TM Cyclone Peal", ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_CYCLONEPEAL },
		{ ZM_ITEM_TM_PSYSPIRAL,    "TM Psy Spiral",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_PSYSPIRAL },
		{ ZM_ITEM_TM_BUZZDRONE,    "TM Buzz Drone",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_BUZZDRONE },
		{ ZM_ITEM_TM_MONOLITHFALL, "TM Monolith Fall",ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_MONOLITHFALL },
		{ ZM_ITEM_TM_WRAITHLASH,   "TM Wraithlash",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_WRAITHLASH },
		{ ZM_ITEM_TM_SKYSUNDER,    "TM Sky Sunder",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_SKYSUNDER },
		{ ZM_ITEM_TM_FOULPLOY,     "TM Foul Ploy",    ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_FOULPLOY },
		{ ZM_ITEM_TM_HEAVYPRESS,   "TM Heavy Press",  ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_HEAVYPRESS },
		{ ZM_ITEM_TM_PRISMBEAM,    "TM Prism Beam",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_PRISMBEAM },
		{ ZM_ITEM_TM_STATICSNARE,  "TM Static Snare", ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_STATICSNARE },
		{ ZM_ITEM_TM_BLIGHTDOSE,   "TM Blightdose",   ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_BLIGHTDOSE },
		{ ZM_ITEM_TM_HEXFLAME,     "TM Hexflame",     ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_HEXFLAME },
		{ ZM_ITEM_TM_RAINCALL,     "TM Raincall",     ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_RAINCALL },
		{ ZM_ITEM_TM_SUNFLARE,     "TM Sunflare",     ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_SUNFLARE },
		{ ZM_ITEM_TM_SANDSTIR,     "TM Sandstir",     ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_SANDSTIR },
		{ ZM_ITEM_TM_BULWARK,      "TM Bulwark",      ZM_ITEM_CATEGORY_TM,       1000,  500, ZM_ITEM_EFFECT_TEACH_MOVE,          0, false, ZM_MOVE_BULWARK },
		// Key items -- gameplay gating, no bag/battle effect
		{ ZM_ITEM_ANGLEROD,        "Angle Rod",       ZM_ITEM_CATEGORY_KEY,         0,    0, ZM_ITEM_EFFECT_NONE,                0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_TRAILBIKE,       "Trail Bike",      ZM_ITEM_CATEGORY_KEY,         0,    0, ZM_ITEM_EFFECT_NONE,                0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_REGIONMAP,       "Region Map",      ZM_ITEM_CATEGORY_KEY,         0,    0, ZM_ITEM_EFFECT_NONE,                0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_SEEKSCOPE,       "Seek Scope",      ZM_ITEM_CATEGORY_KEY,         0,    0, ZM_ITEM_EFFECT_NONE,                0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_BADGECASE,       "Badge Case",      ZM_ITEM_CATEGORY_KEY,         0,    0, ZM_ITEM_EFFECT_NONE,                0, false, ZM_MOVE_NONE },
		{ ZM_ITEM_EGGVOUCHER,      "Egg Voucher",     ZM_ITEM_CATEGORY_KEY,         0,    0, ZM_ITEM_EFFECT_NONE,                0, false, ZM_MOVE_NONE },
	};
}

const ZM_ItemData& ZM_GetItemData(ZM_ITEM_ID eId)
{
	Zenith_Assert(eId < ZM_ITEM_COUNT, "ZM_GetItemData: item id out of range (%u)", (u_int)eId);
	return s_axItems[eId];
}

u_int ZM_GetItemCount()
{
	return ZM_ITEM_COUNT;
}

const char* ZM_GetItemName(ZM_ITEM_ID eId)
{
	if (eId >= ZM_ITEM_COUNT)
	{
		return "NONE";
	}
	return s_axItems[eId].m_szName;
}

const char* ZM_ItemCategoryToString(ZM_ITEM_CATEGORY eCategory)
{
	switch (eCategory)
	{
	case ZM_ITEM_CATEGORY_BALL:     return "BALL";
	case ZM_ITEM_CATEGORY_MEDICINE: return "MEDICINE";
	case ZM_ITEM_CATEGORY_BATTLE:   return "BATTLE";
	case ZM_ITEM_CATEGORY_HELD:     return "HELD";
	case ZM_ITEM_CATEGORY_BERRY:    return "BERRY";
	case ZM_ITEM_CATEGORY_EVO:      return "EVO";
	case ZM_ITEM_CATEGORY_TM:       return "TM";
	case ZM_ITEM_CATEGORY_KEY:      return "KEY";
	case ZM_ITEM_CATEGORY_FIELD:    return "FIELD";
	default:                        return "INVALID";
	}
}
