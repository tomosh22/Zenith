#pragma once

#include "Zenithmon/Source/Data/ZM_MoveData.h"

// ============================================================================
// ZM_ItemData -- the item table (S1 data core), ~90 items across balls,
// medicine, held items, berries, evolution stones, TMs, and key items. Spec:
// Docs/GameDesignDocument.md section 7 (items, ~25 TMs) + Scope.md (items are
// battle core); decomposition rationale in DecisionLog ZM-D-024.
//
// DATA + schema only. The bag/battle logic that INTERPRETS ZM_ITEM_EFFECT
// (using an item, held-item hooks, catch math) is S2/S5 -- a row here is inert:
// it names an effect kind, a magnitude, and (for TMs) a taught ZM_MOVE_ID. Every
// effect kind is used by at least one item (a tested invariant).
//
// Data is a compiled const C array (ZM-D-009). The ZM_ITEM_ID order is
// save-stable once content ships -- APPEND before ZM_ITEM_COUNT, never reorder.
// Names are original (Scope.md: zero Nintendo IP).
// ============================================================================

// Bag pocket / behaviour class.
enum ZM_ITEM_CATEGORY : u_int
{
	ZM_ITEM_CATEGORY_BALL,       // capture devices
	ZM_ITEM_CATEGORY_MEDICINE,   // HP / status / PP heals, vitamins, rare candy
	ZM_ITEM_CATEGORY_BATTLE,     // in-battle temporary boosters
	ZM_ITEM_CATEGORY_HELD,       // passive held items
	ZM_ITEM_CATEGORY_BERRY,      // held consumables
	ZM_ITEM_CATEGORY_EVO,        // evolution stones
	ZM_ITEM_CATEGORY_TM,         // move teachers
	ZM_ITEM_CATEGORY_KEY,        // key items (gameplay gating, no bag/battle effect)
	ZM_ITEM_CATEGORY_FIELD,      // field items (repel-analog, ...)

	ZM_ITEM_CATEGORY_COUNT
};

// The executor tag: what an item DOES when used or held. Interpreted by the
// S2/S5 item logic. Save-stable: APPEND before ZM_ITEM_EFFECT_COUNT.
enum ZM_ITEM_EFFECT : u_int
{
	ZM_ITEM_EFFECT_NONE,

	// HP restore / revive (param = HP for HEAL_HP)
	ZM_ITEM_EFFECT_HEAL_HP,
	ZM_ITEM_EFFECT_HEAL_HP_FULL,
	ZM_ITEM_EFFECT_REVIVE_HALF,
	ZM_ITEM_EFFECT_REVIVE_FULL,

	// Status cures
	ZM_ITEM_EFFECT_CURE_POISON,
	ZM_ITEM_EFFECT_CURE_BURN,
	ZM_ITEM_EFFECT_CURE_FREEZE,
	ZM_ITEM_EFFECT_CURE_SLEEP,
	ZM_ITEM_EFFECT_CURE_PARALYSIS,
	ZM_ITEM_EFFECT_CURE_CONFUSE,
	ZM_ITEM_EFFECT_CURE_ALL_STATUS,

	// PP restore
	ZM_ITEM_EFFECT_RESTORE_PP,
	ZM_ITEM_EFFECT_RESTORE_PP_ALL,
	ZM_ITEM_EFFECT_PP_UP,

	// Capture / progression
	ZM_ITEM_EFFECT_CATCH,
	ZM_ITEM_EFFECT_EV_BOOST,
	ZM_ITEM_EFFECT_LEVEL_UP,
	ZM_ITEM_EFFECT_TEACH_MOVE,
	ZM_ITEM_EFFECT_EVOLVE,

	// In-battle consumable (param = stat index)
	ZM_ITEM_EFFECT_BATTLE_STAT_BOOST,

	// Held (passive)
	ZM_ITEM_EFFECT_HELD_LEFTOVERS,
	ZM_ITEM_EFFECT_HELD_TYPE_BOOST,
	ZM_ITEM_EFFECT_HELD_CHOICE,
	ZM_ITEM_EFFECT_HELD_FOCUS_SASH,
	ZM_ITEM_EFFECT_HELD_LIFE_ORB,
	ZM_ITEM_EFFECT_HELD_FLINCH,
	ZM_ITEM_EFFECT_HELD_BREED_IV,
	ZM_ITEM_EFFECT_HELD_NATURE_LOCK,

	// Berries (held consumable)
	ZM_ITEM_EFFECT_BERRY_HEAL,
	ZM_ITEM_EFFECT_BERRY_CURE_STATUS,
	ZM_ITEM_EFFECT_BERRY_PINCH_BOOST,
	ZM_ITEM_EFFECT_BERRY_TYPE_RESIST,

	// Field
	ZM_ITEM_EFFECT_REPEL,

	ZM_ITEM_EFFECT_COUNT
};

// Every item, in bag/data order. IDs are contiguous 0..ZM_ITEM_COUNT-1 and the
// row index equals the id (asserted by ZM_Tests_Items).
enum ZM_ITEM_ID : u_int
{
	// Balls -- capture devices (effect CATCH, param = catch multiplier x10)
	ZM_ITEM_CATCHORB,
	ZM_ITEM_GREATORB,
	ZM_ITEM_ULTRAORB,
	ZM_ITEM_HEALORB,
	ZM_ITEM_NETORB,
	ZM_ITEM_DUSKORB,
	ZM_ITEM_QUICKORB,
	ZM_ITEM_PRIMEORB,
	// Medicine -- HP restore (param = HP healed; FULL variants ignore it)
	ZM_ITEM_SALVE,
	ZM_ITEM_TONIC,
	ZM_ITEM_DRAUGHT,
	ZM_ITEM_GRANDRESTORE,
	ZM_ITEM_RENEWAL,
	// Medicine -- revive
	ZM_ITEM_REVIVE,
	ZM_ITEM_MAXREVIVE,
	// Medicine -- status cures
	ZM_ITEM_ANTITOXIN,
	ZM_ITEM_BURNBALM,
	ZM_ITEM_THAWSPRAY,
	ZM_ITEM_ROUSEBELL,
	ZM_ITEM_NUMBCURE,
	ZM_ITEM_CLEARMIND,
	ZM_ITEM_PANACEA,
	// Medicine -- PP
	ZM_ITEM_ETHER,
	ZM_ITEM_ELIXIR,
	ZM_ITEM_PPBOOST,
	// Vitamins -- permanent EV boost (param = stat index) + rare candy
	ZM_ITEM_VIGORROOT,
	ZM_ITEM_POWERDRAUGHT,
	ZM_ITEM_GUARDDRAUGHT,
	ZM_ITEM_FOCUSDRAUGHT,
	ZM_ITEM_WARDDRAUGHT,
	ZM_ITEM_SWIFTDRAUGHT,
	ZM_ITEM_RARESWEET,
	// Battle items -- temporary in-battle stat boost (param = stat index)
	ZM_ITEM_XPOWER,
	ZM_ITEM_XGUARD,
	ZM_ITEM_XSWIFT,
	// Held items -- passive battle effects (not consumed)
	ZM_ITEM_AFTERGROWTH,
	ZM_ITEM_FLAMECHARM,
	ZM_ITEM_AQUACHARM,
	ZM_ITEM_LEAFCHARM,
	ZM_ITEM_CHOICEFANG,
	ZM_ITEM_CHOICELENS,
	ZM_ITEM_CHOICEBOOTS,
	ZM_ITEM_ENDURECHARM,
	ZM_ITEM_POWERORB,
	ZM_ITEM_JOLTBELL,
	ZM_ITEM_HEIRLOOMKNOT,
	ZM_ITEM_STASISSTONE,
	// Berries -- held consumables
	ZM_ITEM_ROSEWELLBERRY,
	ZM_ITEM_CLEANSEBERRY,
	ZM_ITEM_EMBERPIPBERRY,
	ZM_ITEM_FROSTPIPBERRY,
	ZM_ITEM_BUFFERFLAME,
	ZM_ITEM_BUFFERTIDE,
	// Evolution stones (param = stone variant)
	ZM_ITEM_FLARESTONE,
	ZM_ITEM_TIDESTONE,
	ZM_ITEM_VOLTSTONE,
	ZM_ITEM_MOONSTONE,
	ZM_ITEM_DUSKSTONE,
	// Field items
	ZM_ITEM_WARDSCENT,
	// TMs -- teach a move (effect TEACH_MOVE, m_eTaughtMove set); reusable
	ZM_ITEM_TM_TITANBEAM,
	ZM_ITEM_TM_CLOSEBOUT,
	ZM_ITEM_TM_TOXICLANCE,
	ZM_ITEM_TM_FAULTLINE,
	ZM_ITEM_TM_DELUGEBEAM,
	ZM_ITEM_TM_SUNLANCE,
	ZM_ITEM_TM_THUNDERPEAL,
	ZM_ITEM_TM_HAILSPEAR,
	ZM_ITEM_TM_MAGMAFALL,
	ZM_ITEM_TM_CYCLONEPEAL,
	ZM_ITEM_TM_PSYSPIRAL,
	ZM_ITEM_TM_BUZZDRONE,
	ZM_ITEM_TM_MONOLITHFALL,
	ZM_ITEM_TM_WRAITHLASH,
	ZM_ITEM_TM_SKYSUNDER,
	ZM_ITEM_TM_FOULPLOY,
	ZM_ITEM_TM_HEAVYPRESS,
	ZM_ITEM_TM_PRISMBEAM,
	ZM_ITEM_TM_STATICSNARE,
	ZM_ITEM_TM_BLIGHTDOSE,
	ZM_ITEM_TM_HEXFLAME,
	ZM_ITEM_TM_RAINCALL,
	ZM_ITEM_TM_SUNFLARE,
	ZM_ITEM_TM_SANDSTIR,
	ZM_ITEM_TM_BULWARK,
	// Key items -- gameplay gating, no bag/battle effect
	ZM_ITEM_ANGLEROD,
	ZM_ITEM_TRAILBIKE,
	ZM_ITEM_REGIONMAP,
	ZM_ITEM_SEEKSCOPE,
	ZM_ITEM_BADGECASE,
	ZM_ITEM_EGGVOUCHER,

	ZM_ITEM_COUNT,
	ZM_ITEM_NONE = ZM_ITEM_COUNT   // "no item" sentinel (empty bag / held slots)
};

// One item row. m_uEffectParam is effect-kind-specific: HEAL_HP/RESTORE_PP read
// it as an amount; CATCH as a catch multiplier x10; EV_BOOST/BATTLE_STAT_BOOST/
// HELD_CHOICE/BERRY_PINCH_BOOST as a ZM_STAT index; HELD_TYPE_BOOST/
// BERRY_TYPE_RESIST as a ZM_TYPE index; EVOLVE as a stone variant; else 0.
// m_eTaughtMove is the move a TM teaches (ZM_MOVE_NONE for every non-TM).
struct ZM_ItemData
{
	ZM_ITEM_ID		m_eId;
	const char*		m_szName;
	ZM_ITEM_CATEGORY	m_eCategory;
	u_int			m_uBuyPrice;      // 0 == not purchasable (key / story / free items)
	u_int			m_uSellPrice;     // <= buy price; 0 for key items
	ZM_ITEM_EFFECT	m_eEffect;
	u_int			m_uEffectParam;   // kind-specific (amount / stat idx / type idx / mult)
	bool			m_bConsumable;    // consumed on use (balls/medicine/berries/battle/evo/field)
	ZM_MOVE_ID		m_eTaughtMove;    // TM: taught move; ZM_MOVE_NONE otherwise
};

// Table accessors (bounds-asserted). GetItemData indexes by ZM_ITEM_ID.
const ZM_ItemData&	ZM_GetItemData(ZM_ITEM_ID eId);
u_int				ZM_GetItemCount();                 // == ZM_ITEM_COUNT
const char*			ZM_GetItemName(ZM_ITEM_ID eId);   // "NONE" out of range
const char*			ZM_ItemCategoryToString(ZM_ITEM_CATEGORY eCategory);
