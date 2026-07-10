#pragma once

// ============================================================================
// ZM_AbilityData -- the ability roster (S1 data core), ~50 abilities as metadata
// rows. Spec: Docs/GameDesignDocument.md section 7 (~50 abilities via per-hook
// fn-pointer structs); DecisionLog ZM-D-026.
//
// S1 SCOPE = roster + metadata + each ability's declared HOOK SURFACE (a bitmask
// of ZM_ABILITY_HOOK -- which battle hooks the ability will implement). The
// fn-pointer hook BODIES are deferred to S2: they need the battle-state types
// (ZM_BattleState / ZM_BattleEvent) that do not exist yet, and wiring speculative
// signatures now would only churn. The bitmask lets S2 build the fn-pointer hook
// struct and lets tests + tools reason about coverage today. Every hook bit is
// used by at least one ability (a tested invariant).
//
// Data is a compiled const C array (ZM-D-009). Names are original (Scope.md:
// zero Nintendo IP). ZM_ABILITY_ID order is save-stable -- APPEND before
// ZM_ABILITY_COUNT, never reorder.
// ============================================================================

// The battle hook points an ability can implement, as bit flags for the hook
// surface mask. The concrete per-hook fn-pointer struct is built in S2.
enum ZM_ABILITY_HOOK : u_int
{
	ZM_ABILITY_HOOK_SWITCH_IN           = 1u << 0,
	ZM_ABILITY_HOOK_MODIFY_STAT         = 1u << 1,
	ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT = 1u << 2,
	ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN = 1u << 3,
	ZM_ABILITY_HOOK_STATUS_TRY          = 1u << 4,
	ZM_ABILITY_HOOK_CONTACT             = 1u << 5,
	ZM_ABILITY_HOOK_TURN_END            = 1u << 6,
	ZM_ABILITY_HOOK_FAINT               = 1u << 7,
	ZM_ABILITY_HOOK_ACCURACY            = 1u << 8,
	ZM_ABILITY_HOOK_WEATHER             = 1u << 9,
	ZM_ABILITY_HOOK_TYPE_IMMUNITY       = 1u << 10,
};

// Number of defined hook bits, and a mask of all of them (for validation).
static const u_int uZM_ABILITY_HOOK_COUNT = 11u;
static const u_int uZM_ABILITY_HOOK_ALL   = (1u << 11) - 1u;

// Every ability, in data order. IDs are contiguous 0..ZM_ABILITY_COUNT-1 and the
// row index equals the id (asserted by ZM_Tests_Abilities).
enum ZM_ABILITY_ID : u_int
{
	ZM_ABILITY_VERDANTSURGE,
	ZM_ABILITY_EMBERSURGE,
	ZM_ABILITY_TIDALSURGE,
	ZM_ABILITY_HIVESURGE,
	ZM_ABILITY_DAUNTINGROAR,
	ZM_ABILITY_SKYWARDGRACE,
	ZM_ABILITY_BEDROCK,
	ZM_ABILITY_STATICVEIL,
	ZM_ABILITY_CINDERSKIN,
	ZM_ABILITY_BARBSKIN,
	ZM_ABILITY_THORNMAIL,
	ZM_ABILITY_SUNCHASER,
	ZM_ABILITY_STREAMLINE,
	ZM_ABILITY_GRITSTRIDE,
	ZM_ABILITY_RIMESTRIDE,
	ZM_ABILITY_RAINCALLER,
	ZM_ABILITY_SUNCALLER,
	ZM_ABILITY_SANDCALLER,
	ZM_ABILITY_SNOWCALLER,
	ZM_ABILITY_FERVOR,
	ZM_ABILITY_BLUBBER,
	ZM_ABILITY_AQUIFER,
	ZM_ABILITY_DYNAMO,
	ZM_ABILITY_CINDERDRINK,
	ZM_ABILITY_GRAZER,
	ZM_ABILITY_IRONWILL,
	ZM_ABILITY_KEENEYE,
	ZM_ABILITY_DEADAIM,
	ZM_ABILITY_WAKEFUL,
	ZM_ABILITY_PUREBLOOD,
	ZM_ABILITY_THAWHEART,
	ZM_ABILITY_LIMBERLITHE,
	ZM_ABILITY_OWNPACE,
	ZM_ABILITY_COLDBLOOD,
	ZM_ABILITY_BLOODRUSH,
	ZM_ABILITY_LASTSPITE,
	ZM_ABILITY_AFTERSHOCK,
	ZM_ABILITY_SOLIDCORE,
	ZM_ABILITY_HEAVYPLATE,
	ZM_ABILITY_GOSSAMER,
	ZM_ABILITY_DOWNDRAFT,
	ZM_ABILITY_RAINBASK,
	ZM_ABILITY_SUNBASK,
	ZM_ABILITY_ICEBOUND,
	ZM_ABILITY_TOXICTHRIVE,
	ZM_ABILITY_ROOTFEED,
	ZM_ABILITY_QUICKDRAW,
	ZM_ABILITY_PRESSUREAURA,
	ZM_ABILITY_GUARDIAN,
	ZM_ABILITY_TRUESHOT,

	ZM_ABILITY_COUNT,
	ZM_ABILITY_NONE = ZM_ABILITY_COUNT   // "no ability" sentinel
};

// One ability row. m_uHookMask is a bitwise-OR of ZM_ABILITY_HOOK flags naming
// the hooks this ability implements (non-zero; S2 fills the matching fn-pointers).
struct ZM_AbilityData
{
	ZM_ABILITY_ID	m_eId;
	const char*		m_szName;
	const char*		m_szDescription;
	u_int			m_uHookMask;
};

// Table accessors (bounds-asserted). GetAbilityData indexes by ZM_ABILITY_ID.
const ZM_AbilityData&	ZM_GetAbilityData(ZM_ABILITY_ID eId);
u_int					ZM_GetAbilityCount();                    // == ZM_ABILITY_COUNT
const char*				ZM_GetAbilityName(ZM_ABILITY_ID eId);   // "NONE" out of range

// True if the ability declares the given hook in its surface mask.
bool					ZM_AbilityHasHook(ZM_ABILITY_ID eId, ZM_ABILITY_HOOK eHook);
