#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"

// ============================================================================
// ZM_MoveData -- the move table (S1 data core), ~220 moves as compiled rows
// over the ZM_MOVE_EFFECT executor-tag enum. Spec: Docs/GameDesignDocument.md
// section 7; decomposition rationale in DecisionLog ZM-D-022.
//
// This box is the DATA + schema only. The one executor switch that INTERPRETS
// ZM_MOVE_EFFECT (ZM_MoveExecutor) and the damage/status pipeline are S2 -- a
// move row is inert here: it names an effect kind, a magnitude, and a chance,
// and the S2 engine gives them behaviour. Every effect kind is used by at least
// one row (a tested invariant) so S2's per-effect scenarios all have a subject.
//
// Data is a compiled const C array (ZM-D-009): zero file I/O in headless tests.
// The ZM_MOVE_ID order is save-stable once content ships -- APPEND before
// ZM_MOVE_COUNT, never reorder. Names are original (Scope.md: zero Nintendo IP);
// the cut effects (Substitute / Encore / Transform / weight moves, GDD 7.2) have
// no enum value here.
// ============================================================================

// Damage class. STATUS moves have zero power and always carry an effect.
enum ZM_MOVE_CATEGORY : u_int
{
	ZM_MOVE_CATEGORY_PHYSICAL,      // uses Attack vs Defense
	ZM_MOVE_CATEGORY_SPECIAL,       // uses Sp.Attack vs Sp.Defense
	ZM_MOVE_CATEGORY_STATUS,        // no direct damage

	ZM_MOVE_CATEGORY_COUNT
};

// Who the move is aimed at -- consumed by the S2 executor and the trainer AI.
// OPPONENT: attacks, status infliction, stat drops. SELF: buffs / heal / protect.
// FIELD: weather / screens / hazards (a whole battlefield side).
enum ZM_MOVE_TARGET : u_int
{
	ZM_MOVE_TARGET_OPPONENT,
	ZM_MOVE_TARGET_SELF,
	ZM_MOVE_TARGET_FIELD,

	ZM_MOVE_TARGET_COUNT
};

// The executor tag: what a move DOES beyond (or instead of) plain typed damage.
// ~60 kinds, each interpreted by the single S2 ZM_MoveExecutor switch. Save-
// stable range: APPEND before ZM_MOVE_EFFECT_COUNT, never reorder.
enum ZM_MOVE_EFFECT : u_int
{
	ZM_MOVE_EFFECT_NONE,

	// Damage-delivery variants (how the hit is delivered / damage produced)
	ZM_MOVE_EFFECT_MULTI_HIT,
	ZM_MOVE_EFFECT_DOUBLE_HIT,
	ZM_MOVE_EFFECT_RECOIL,
	ZM_MOVE_EFFECT_DRAIN,
	ZM_MOVE_EFFECT_CHARGE_TURN,
	ZM_MOVE_EFFECT_SEMI_INVULN,
	ZM_MOVE_EFFECT_RECHARGE,
	ZM_MOVE_EFFECT_LOCK_IN,
	ZM_MOVE_EFFECT_FIXED_LEVEL,
	ZM_MOVE_EFFECT_HALVE_HP,
	ZM_MOVE_EFFECT_OHKO,

	// Major status (a secondary on a damaging move, or the point of a status move)
	ZM_MOVE_EFFECT_BURN,
	ZM_MOVE_EFFECT_FREEZE,
	ZM_MOVE_EFFECT_PARALYZE,
	ZM_MOVE_EFFECT_POISON,
	ZM_MOVE_EFFECT_TOXIC,
	ZM_MOVE_EFFECT_SLEEP,

	// Volatiles (battle-local)
	ZM_MOVE_EFFECT_CONFUSE,
	ZM_MOVE_EFFECT_FLINCH,
	ZM_MOVE_EFFECT_LEECH_SEED,
	ZM_MOVE_EFFECT_TRAP,
	ZM_MOVE_EFFECT_TAUNT,

	// Target stat drops (magnitude = stage count)
	ZM_MOVE_EFFECT_LOWER_ATTACK,
	ZM_MOVE_EFFECT_LOWER_DEFENSE,
	ZM_MOVE_EFFECT_LOWER_SPATTACK,
	ZM_MOVE_EFFECT_LOWER_SPDEFENSE,
	ZM_MOVE_EFFECT_LOWER_SPEED,
	ZM_MOVE_EFFECT_LOWER_ACCURACY,
	ZM_MOVE_EFFECT_LOWER_EVASION,

	// User stat boosts (magnitude = stage count)
	ZM_MOVE_EFFECT_RAISE_ATTACK,
	ZM_MOVE_EFFECT_RAISE_DEFENSE,
	ZM_MOVE_EFFECT_RAISE_SPATTACK,
	ZM_MOVE_EFFECT_RAISE_SPDEFENSE,
	ZM_MOVE_EFFECT_RAISE_SPEED,
	ZM_MOVE_EFFECT_RAISE_EVASION,
	ZM_MOVE_EFFECT_RAISE_CRIT,
	ZM_MOVE_EFFECT_RAISE_ATTACK_SPEED,
	ZM_MOVE_EFFECT_RAISE_ATTACK_DEFENSE,
	ZM_MOVE_EFFECT_RAISE_SPATK_SPDEF,
	ZM_MOVE_EFFECT_RAISE_DEF_SPDEF,
	ZM_MOVE_EFFECT_RAISE_ALL,

	// Mixed target/self
	ZM_MOVE_EFFECT_SWAGGER,

	// Healing / self-maintenance
	ZM_MOVE_EFFECT_HEAL_HALF,
	ZM_MOVE_EFFECT_REST,
	ZM_MOVE_EFFECT_CURE_STATUS,
	ZM_MOVE_EFFECT_HEAL_BELL,
	ZM_MOVE_EFFECT_PROTECT,
	ZM_MOVE_EFFECT_ENDURE,

	// Field / side conditions
	ZM_MOVE_EFFECT_WEATHER_RAIN,
	ZM_MOVE_EFFECT_WEATHER_SUN,
	ZM_MOVE_EFFECT_WEATHER_SAND,
	ZM_MOVE_EFFECT_WEATHER_SNOW,
	ZM_MOVE_EFFECT_SCREEN_PHYSICAL,
	ZM_MOVE_EFFECT_SCREEN_SPECIAL,
	ZM_MOVE_EFFECT_HAZARD_SPIKES,
	ZM_MOVE_EFFECT_FORCE_SWITCH,

	ZM_MOVE_EFFECT_COUNT
};

// Every move, in dex/data order. IDs are contiguous 0..ZM_MOVE_COUNT-1 and the
// row index equals the id (asserted by ZM_Tests_Moves).
enum ZM_MOVE_ID : u_int
{
	// NORMAL -- the utility type: plain strikes plus the shared status kit
	ZM_MOVE_RAMBASH,
	ZM_MOVE_QUICKJAB,
	ZM_MOVE_BRUTESLAM,
	ZM_MOVE_COMETDASH,
	ZM_MOVE_CLAMOR,
	ZM_MOVE_SKULLKNOCK,
	ZM_MOVE_FLURRYJAB,
	ZM_MOVE_RECKLESSRUSH,
	ZM_MOVE_TITANBEAM,
	ZM_MOVE_BERSERKSPIN,
	ZM_MOVE_GNASHDOWN,
	ZM_MOVE_WHETCLAW,
	ZM_MOVE_QUICKEN,
	ZM_MOVE_FIRMUP,
	ZM_MOVE_RALLYCRY,
	ZM_MOVE_WARCRY,
	ZM_MOVE_COWERGLARE,
	ZM_MOVE_BRAVADO,
	ZM_MOVE_REPOSE,
	ZM_MOVE_SLUMBERHEAL,
	ZM_MOVE_SHAKEOFF,
	ZM_MOVE_BULWARK,
	ZM_MOVE_BRACE,
	ZM_MOVE_BELLOW,
	// FIRE -- special-leaning; burns and sun
	ZM_MOVE_CINDERSPIT,
	ZM_MOVE_FLARELASH,
	ZM_MOVE_PYREBURST,
	ZM_MOVE_MAGMAFALL,
	ZM_MOVE_EMBERCLAW,
	ZM_MOVE_FLAREPLUNGE,
	ZM_MOVE_HEATSHIMMER,
	ZM_MOVE_HEXFLAME,
	ZM_MOVE_ASHVEIL,
	ZM_MOVE_SUNFLARE,
	ZM_MOVE_KINDLEUP,
	// WATER -- special-leaning; rain and trapping currents
	ZM_MOVE_BRINELASH,
	ZM_MOVE_BUBBLESPRAY,
	ZM_MOVE_TIDECRASH,
	ZM_MOVE_TORRENTCANNON,
	ZM_MOVE_AQUAFANG,
	ZM_MOVE_SALTSPRAY,
	ZM_MOVE_MAELSTROMCOIL,
	ZM_MOVE_UNDERTOW,
	ZM_MOVE_RAINCALL,
	ZM_MOVE_MISTVEIL,
	ZM_MOVE_AQUAJET,
	ZM_MOVE_DELUGEBEAM,
	// GRASS -- seeds, drains, and the charge beam
	ZM_MOVE_LEAFCUT,
	ZM_MOVE_SEEDSHOT,
	ZM_MOVE_BRAMBLELASH,
	ZM_MOVE_SAPDRAW,
	ZM_MOVE_BLOOMBURST,
	ZM_MOVE_SUNLANCE,
	ZM_MOVE_THORNVOLLEY,
	ZM_MOVE_DROWSESPORE,
	ZM_MOVE_SEEDLEECH,
	ZM_MOVE_SPOREPOISON,
	ZM_MOVE_PHOTOSYNTH,
	ZM_MOVE_VERDANTGUARD,
	ZM_MOVE_BARKTONIC,
	// ELECTRIC -- special-leaning; paralysis
	ZM_MOVE_SPARKBITE,
	ZM_MOVE_ZAPTHREAD,
	ZM_MOVE_VOLTLANCE,
	ZM_MOVE_STORMCELL,
	ZM_MOVE_GALVANORUSH,
	ZM_MOVE_SPARKDART,
	ZM_MOVE_STATICSNARE,
	ZM_MOVE_CHARGEAURA,
	ZM_MOVE_OVERCLOCK,
	ZM_MOVE_IONVEIL,
	ZM_MOVE_THUNDERPEAL,
	// ICE -- freezes, snow, and an OHKO risk elsewhere; here a slow-bind
	ZM_MOVE_FROSTNIP,
	ZM_MOVE_RIMEBLAST,
	ZM_MOVE_GLACIERCRASH,
	ZM_MOVE_HAILSPEAR,
	ZM_MOVE_FROSTBIND,
	ZM_MOVE_ICEFANG,
	ZM_MOVE_SNOWVEIL,
	ZM_MOVE_FROSTWARD,
	ZM_MOVE_SHIVERCRY,
	ZM_MOVE_SLEETVOLLEY,
	ZM_MOVE_PERMAFROST,
	// BRAWL -- physical; crit and setup
	ZM_MOVE_JABCROSS,
	ZM_MOVE_ONETWO,
	ZM_MOVE_HAYMAKER,
	ZM_MOVE_RISINGKNEE,
	ZM_MOVE_CLOSEBOUT,
	ZM_MOVE_PIVOTKICK,
	ZM_MOVE_MACHFLURRY,
	ZM_MOVE_BULKSTANCE,
	ZM_MOVE_KILLERFOCUS,
	ZM_MOVE_GUTCHECK,
	ZM_MOVE_STAGGERPALM,
	ZM_MOVE_COUNTERSTANCE,
	// VENOM -- poisons, toxic, and corrosion drops
	ZM_MOVE_VENOMFANG,
	ZM_MOVE_SLUDGESHOT,
	ZM_MOVE_TOXICLANCE,
	ZM_MOVE_ACIDMELT,
	ZM_MOVE_LEECHBITE,
	ZM_MOVE_VENOMCOAT,
	ZM_MOVE_BLIGHTDOSE,
	ZM_MOVE_NOXHAZE,
	ZM_MOVE_CAUSTICSPINE,
	ZM_MOVE_SEEPINGMIRE,
	ZM_MOVE_PURGEPULSE,
	ZM_MOVE_FESTERGRIP,
	// EARTH -- physical; sand, digging, hazards, the fissure
	ZM_MOVE_CLODTHROW,
	ZM_MOVE_STOMPQUAKE,
	ZM_MOVE_FAULTLINE,
	ZM_MOVE_UNDERDELVE,
	ZM_MOVE_GRITCLOUD,
	ZM_MOVE_SANDSTIR,
	ZM_MOVE_CALTROPS,
	ZM_MOVE_MUDLASH,
	ZM_MOVE_BOULDERBIND,
	ZM_MOVE_FISSURECRACK,
	ZM_MOVE_TERRASPIKE,
	ZM_MOVE_QUAKESTEP,
	// SKY -- fliers; semi-invuln dive, gusts, roost
	ZM_MOVE_GALEDART,
	ZM_MOVE_WINGBEAT,
	ZM_MOVE_SKYLANCE,
	ZM_MOVE_HIGHSTOOP,
	ZM_MOVE_BRAVEBEAK,
	ZM_MOVE_ZEPHYRBLADE,
	ZM_MOVE_GUSTAWAY,
	ZM_MOVE_NESTLE,
	ZM_MOVE_TAILWINDUP,
	ZM_MOVE_CLEARVISION,
	ZM_MOVE_DOWNDRAFT,
	ZM_MOVE_CYCLONEPEAL,
	// MIND -- special; screens, calm-mind, fixed-damage, sleep, confuse
	ZM_MOVE_MINDJAB,
	ZM_MOVE_PSILANCE,
	ZM_MOVE_CEREBRALPEAL,
	ZM_MOVE_MINDSHEAR,
	ZM_MOVE_BEWILDER,
	ZM_MOVE_LULLABY,
	ZM_MOVE_SERENEMIND,
	ZM_MOVE_AEGISWALL,
	ZM_MOVE_LUMENWALL,
	ZM_MOVE_KINETICPUSH,
	ZM_MOVE_PSYSPIRAL,
	ZM_MOVE_FORESIGHT,
	ZM_MOVE_MENTALGOAD,
	// SWARM -- bugs; multi-hit, drops, silk-bind
	ZM_MOVE_NIPSTING,
	ZM_MOVE_NETTLEFLURRY,
	ZM_MOVE_MANDIBLESLASH,
	ZM_MOVE_VEINTAP,
	ZM_MOVE_POLLENBURST,
	ZM_MOVE_SPORESHRIEK,
	ZM_MOVE_SILKBIND,
	ZM_MOVE_HIVEHASTE,
	ZM_MOVE_VENOMSTINGER,
	ZM_MOVE_BUZZDRONE,
	ZM_MOVE_CHITINUP,
	// STONE -- physical; ancient-power all-up, sand pairing, walls
	ZM_MOVE_PEBBLESLING,
	ZM_MOVE_ROCKHURL,
	ZM_MOVE_BOULDERSMASH,
	ZM_MOVE_PRIMEVALMIGHT,
	ZM_MOVE_SCREESLIDE,
	ZM_MOVE_STONEEDGE,
	ZM_MOVE_BASTION,
	ZM_MOVE_CRAGGUARD,
	ZM_MOVE_DUSTBLAST,
	ZM_MOVE_MONOLITHFALL,
	ZM_MOVE_SEISMICPRESS,
	// PHANTOM -- special-leaning; night-shade fixed, curses, drops
	ZM_MOVE_SPECTERTOUCH,
	ZM_MOVE_HEXBOLT,
	ZM_MOVE_WRAITHLASH,
	ZM_MOVE_NIGHTBRAND,
	ZM_MOVE_DREADGAZE,
	ZM_MOVE_SHADOWSNARE,
	ZM_MOVE_SPITEFLICKER,
	ZM_MOVE_GRUDGEVEIL,
	ZM_MOVE_PHANTOMFEAST,
	ZM_MOVE_HOLLOWWAIL,
	ZM_MOVE_NIGHTMARE,
	// DRAKE -- mixed; outrage-lock, dragon-dance, big beams
	ZM_MOVE_DRAKECLAW,
	ZM_MOVE_WYRMPULSE,
	ZM_MOVE_RAGEFLIGHT,
	ZM_MOVE_SCALESHRED,
	ZM_MOVE_WYRMDANCE,
	ZM_MOVE_CINDERBREATH,
	ZM_MOVE_TWINFANG,
	ZM_MOVE_DRACOVEIL,
	ZM_MOVE_SKYSUNDER,
	ZM_MOVE_TAILSWEEP,
	// UMBRAL -- dark; taunt, evasion, crunch drops, priority
	ZM_MOVE_SHADEBITE,
	ZM_MOVE_DUSKFANG,
	ZM_MOVE_NIGHTREAVE,
	ZM_MOVE_SNEAKSTRIKE,
	ZM_MOVE_GOAD,
	ZM_MOVE_SHADOWDANCE,
	ZM_MOVE_FOULPLOY,
	ZM_MOVE_MENACE,
	ZM_MOVE_VOIDGRIP,
	ZM_MOVE_DARKLURE,
	ZM_MOVE_BLACKOUTBLOW,
	// IRON -- steel; broad walls, hazards, iron-defense
	ZM_MOVE_IRONJAB,
	ZM_MOVE_METALEDGE,
	ZM_MOVE_GIRDERSLAM,
	ZM_MOVE_SHARDSCATTER,
	ZM_MOVE_IRONDEFENSE,
	ZM_MOVE_MAGNETPULSE,
	ZM_MOVE_STEELVEIL,
	ZM_MOVE_BOLTGUN,
	ZM_MOVE_RIVETSHOT,
	ZM_MOVE_HEAVYPRESS,
	ZM_MOVE_ALLOYWARD,
	// FEY -- fairy; charm drops, heal-bell, calm setup
	ZM_MOVE_GLIMMERTAP,
	ZM_MOVE_SPARKLEBURST,
	ZM_MOVE_FAIRYLANCE,
	ZM_MOVE_CHARMNOTE,
	ZM_MOVE_CHIMECURE,
	ZM_MOVE_DAZZLEPULSE,
	ZM_MOVE_MOONVEIL,
	ZM_MOVE_PIXIEDUST,
	ZM_MOVE_SERENITYHYMN,
	ZM_MOVE_PRISMBEAM,

	ZM_MOVE_COUNT,
	ZM_MOVE_NONE = ZM_MOVE_COUNT   // "no move" sentinel (empty move slots)
};

// m_uAccuracy == this means the move bypasses the accuracy check (never misses);
// all SELF/FIELD-targeted moves use it, as do a few always-hit attacks.
static const u_int uZM_MOVE_ACCURACY_ALWAYS_HITS = 0u;

// One move row. m_iEffectMagnitude is effect-kind-specific: stat-change kinds
// read it as a stage count (1..3); RECOIL/DRAIN/HEAL_HALF read it as a percent;
// delivery kinds (MULTI_HIT, OHKO, ...) ignore it (0). m_uEffectChance is the
// secondary-effect proc chance for a damaging move, or 100 for a status move's
// primary effect; it is 0 exactly when m_eEffect is ZM_MOVE_EFFECT_NONE.
struct ZM_MoveData
{
	ZM_MOVE_ID		m_eId;
	const char*		m_szName;
	ZM_TYPE			m_eType;
	ZM_MOVE_CATEGORY	m_eCategory;
	u_int			m_uPower;             // 0 for status + fixed-damage moves
	u_int			m_uAccuracy;          // 1..100, or uZM_MOVE_ACCURACY_ALWAYS_HITS
	u_int			m_uPP;                // base PP (1..40)
	int				m_iPriority;          // -7..+5 bracket, 0 is normal
	u_int			m_uCritStage;         // 0 normal, 1 high, 2 guaranteed
	bool			m_bMakesContact;
	ZM_MOVE_EFFECT	m_eEffect;
	u_int			m_uEffectChance;      // 0..100 (0 iff effect is NONE)
	int				m_iEffectMagnitude;   // kind-specific (stages / percent / unused)
	ZM_MOVE_TARGET	m_eTarget;
};

// Table accessors (bounds-asserted). GetMoveData indexes by ZM_MOVE_ID.
const ZM_MoveData&	ZM_GetMoveData(ZM_MOVE_ID eId);
u_int				ZM_GetMoveCount();                // == ZM_MOVE_COUNT
const char*			ZM_GetMoveName(ZM_MOVE_ID eId);  // "NONE" out of range

// Small enum-to-string helpers (logs / tooltips / test diagnostics).
const char*			ZM_MoveCategoryToString(ZM_MOVE_CATEGORY eCategory);
const char*			ZM_MoveTargetToString(ZM_MOVE_TARGET eTarget);
