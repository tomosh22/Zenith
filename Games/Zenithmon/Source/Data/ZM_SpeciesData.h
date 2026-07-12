#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"

// Forward declaration: ZM_RollGender takes the seeded RNG by reference only, so this
// pure data header stays free of the RNG header (ZM_SpeciesData.cpp includes it).
class ZM_BattleRNG;

// ============================================================================
// ZM_SpeciesData -- the species dex table (S1 data core).
//
// This is the STRUCTURAL roster: id / name / type(s) / archetype / evolution /
// family / rarity for all 152 species from Docs/GameDesignDocument.md section 5.
// Base stats come from ZM_GetSpeciesBaseStats (ZM-D-021, derived); level-up
// learnsets from ZM_GetSpeciesLearnset in ZM_Learnsets.h (ZM-D-023, derived).
// See DecisionLog ZM-D-020 for the roster-first decomposition of this box.
//
// Data is a compiled const C array (DecisionLog ZM-D-009): zero file I/O in
// headless tests. The ZM_SPECIES_ID order is the dex order and is save-stable
// once content ships -- APPEND before ZM_SPECIES_COUNT, never reorder.
// ============================================================================

// The 8 body plans -- each names the ZM_CreatureGen builder used for a family
// (S4). All stages of a family share one archetype (GDD 5).
enum ZM_ARCHETYPE : u_int
{
	ZM_ARCHETYPE_QUADRUPED,
	ZM_ARCHETYPE_BIPED,
	ZM_ARCHETYPE_AVIAN,
	ZM_ARCHETYPE_SERPENT,
	ZM_ARCHETYPE_AQUATIC,
	ZM_ARCHETYPE_INSECTOID,
	ZM_ARCHETYPE_BLOB,
	ZM_ARCHETYPE_FLOATER_PLANTOID,

	ZM_ARCHETYPE_COUNT
};

// Encounter-slot weight tier; LEGENDARY gates post-game (GDD 5).
enum ZM_RARITY : u_int
{
	ZM_RARITY_COMMON,
	ZM_RARITY_UNCOMMON,
	ZM_RARITY_RARE,
	ZM_RARITY_LEGENDARY,

	ZM_RARITY_COUNT
};

// Rough physical scale, consumed by ZM_CreatureGen (S4) and camera framing.
// Derived from evolution stage + archetype for now (see ZM_GetSpeciesSizeClass).
enum ZM_SIZE_CLASS : u_int
{
	ZM_SIZE_TINY,
	ZM_SIZE_SMALL,
	ZM_SIZE_MEDIUM,
	ZM_SIZE_LARGE,
	ZM_SIZE_HUGE,

	ZM_SIZE_COUNT
};

// The three sexes a monster can carry (box-6 SC-A). GENDERLESS covers legendaries
// and the inorganic BLOB body plan, which reproduce only via the universal breeder
// (box-6 SC-B). APPEND-only + save-stable; ZM_GENDER_COUNT is the sentinel, never a
// stored value.
enum ZM_GENDER : u_int
{
	ZM_GENDER_MALE,
	ZM_GENDER_FEMALE,
	ZM_GENDER_GENDERLESS,

	ZM_GENDER_COUNT
};

// Per-species sex distribution (box-6 SC-A). The three FIXED ratios (GENDERLESS /
// MALE_ONLY / FEMALE_ONLY) resolve with NO RNG draw; the five GRADED ratios roll one
// RandBelow(8) against a female threshold (see ZM_GenderRatioFemaleThresholdOutOf8).
// APPEND-only + save-stable; the SC-A derivation only emits a subset (GENDERLESS,
// MALE_7_1, EVEN, MALE_3_1, FEMALE_3_1) -- the rest are reserved for later tuning.
enum ZM_GENDER_RATIO : u_int
{
	ZM_GENDER_RATIO_GENDERLESS,
	ZM_GENDER_RATIO_MALE_ONLY,
	ZM_GENDER_RATIO_FEMALE_ONLY,
	ZM_GENDER_RATIO_MALE_7_1,
	ZM_GENDER_RATIO_MALE_3_1,
	ZM_GENDER_RATIO_EVEN,
	ZM_GENDER_RATIO_FEMALE_3_1,
	ZM_GENDER_RATIO_FEMALE_7_1,

	ZM_GENDER_RATIO_COUNT
};

// Sentinel returned by ZM_GenderRatioFemaleThresholdOutOf8 for the three fixed
// ratios: they carry no /8 female threshold because ZM_RollGender resolves them
// without a RandBelow(8) draw.
static const u_int uZM_GENDER_RATIO_NO_ROLL = ~0u;

// Every species, in dex order: family F01..F56 (each family's stages in order),
// then the three legendaries. IDs are contiguous 0..ZM_SPECIES_COUNT-1.
enum ZM_SPECIES_ID : u_int
{
	// F01 QUADRUPED (starter -- grove-antlered deer)
	ZM_SPECIES_FERNFAWN,
	ZM_SPECIES_THICKETBUCK,
	ZM_SPECIES_SYLVASTAG,
	// F02 BIPED (starter -- self-firing kiln salamander)
	ZM_SPECIES_KINDLET,
	ZM_SPECIES_SCORCHEL,
	ZM_SPECIES_PYROCLAST,
	// F03 AQUATIC (starter -- blade-billed duelist fish)
	ZM_SPECIES_FINLET,
	ZM_SPECIES_MARLANCE,
	ZM_SPECIES_TIDESABRE,
	// F04 AVIAN (everywhere-songbird)
	ZM_SPECIES_PIPWIT,
	ZM_SPECIES_TRILLARK,
	ZM_SPECIES_STRATAVIS,
	// F05 QUADRUPED (seed-hoarding vole)
	ZM_SPECIES_NIBBIN,
	ZM_SPECIES_HOARDEL,
	ZM_SPECIES_GRAINMAW,
	// F06 INSECTOID (early butterfly)
	ZM_SPECIES_WRIGGLET,
	ZM_SPECIES_CRADLEWRAP,
	ZM_SPECIES_AURELWING,
	// F07 QUADRUPED (gate-guarding hound) -- 2 stages
	ZM_SPECIES_STRAYLING,
	ZM_SPECIES_WARDHUND,
	// F08 FLOATER_PLANTOID (dandelion drifter)
	ZM_SPECIES_PUFFSEED,
	ZM_SPECIES_DANDELIFT,
	ZM_SPECIES_ZEPHYRBLOOM,
	// F09 QUADRUPED (storm-chasing fox)
	ZM_SPECIES_SPARKIT,
	ZM_SPECIES_AMPTAIL,
	ZM_SPECIES_FULGURUN,
	// F10 BLOB (walking cairn)
	ZM_SPECIES_RUBBLET,
	ZM_SPECIES_CAIRNGO,
	ZM_SPECIES_MONOLODE,
	// F11 AQUATIC (universal river fish)
	ZM_SPECIES_MINNET,
	ZM_SPECIES_SHOALFIN,
	ZM_SPECIES_TORRENTFIN,
	// F12 QUADRUPED (toxin-sweating bog newt)
	ZM_SPECIES_MIRELET,
	ZM_SPECIES_BOGBANE,
	ZM_SPECIES_VENOMIRE,
	// F13 BIPED (boxing hare)
	ZM_SPECIES_SCRAPLING,
	ZM_SPECIES_SPARHARE,
	ZM_SPECIES_GRANDGUARD,
	// F14 FLOATER_PLANTOID (will-o'-the-wisp)
	ZM_SPECIES_WISPET,
	ZM_SPECIES_GLIMMOURN,
	ZM_SPECIES_MOURNLIGHT,
	// F15 BIPED (blindfolded seer)
	ZM_SPECIES_TRANCET,
	ZM_SPECIES_MESMEREL,
	ZM_SPECIES_ORACLYNE,
	// F16 QUADRUPED (alpine ice elk)
	ZM_SPECIES_FRISKET,
	ZM_SPECIES_SNOWLOPE,
	ZM_SPECIES_GLACIELK,
	// F17 SERPENT (pseudo-legendary climbing wyrm)
	ZM_SPECIES_WYRMLING,
	ZM_SPECIES_CRAGWYRM,
	ZM_SPECIES_ZENITHRAX,
	// F18 BLOB (living foundry slag)
	ZM_SPECIES_SLAGLET,
	ZM_SPECIES_FERRALUMP,
	ZM_SPECIES_SMELTITAN,
	// F19 FLOATER_PLANTOID (flower-ring dancer)
	ZM_SPECIES_FAYLING,
	ZM_SPECIES_CHIMESPRITE,
	ZM_SPECIES_SYLPHARA,
	// F20 QUADRUPED (dusk-woven moor cat)
	ZM_SPECIES_SHADELET,
	ZM_SPECIES_DUSKSTALK,
	ZM_SPECIES_NIGHTREAVE,
	// F21 INSECTOID (hedge spider)
	ZM_SPECIES_LOOMITE,
	ZM_SPECIES_SILKLURK,
	ZM_SPECIES_WEAVENOM,
	// F22 QUADRUPED (route-digging mole)
	ZM_SPECIES_BURRIT,
	ZM_SPECIES_GRAVELOW,
	ZM_SPECIES_TERRADRILL,
	// F23 AVIAN (thunderhead storm-petrel)
	ZM_SPECIES_SQUALLET,
	ZM_SPECIES_GALECREST,
	ZM_SPECIES_THUNDEROC,
	// F24 AQUATIC (ice-floe seal to aurora orca)
	ZM_SPECIES_FLOELET,
	ZM_SPECIES_PINNIFLOE,
	ZM_SPECIES_AURORCA,
	// F25 QUADRUPED (volcanic scavenger jackal) -- 2 stages
	ZM_SPECIES_CINDERJACK,
	ZM_SPECIES_ASHENHOWL,
	// F26 INSECTOID (pruning-blade mantis)
	ZM_SPECIES_BLADEBUD,
	ZM_SPECIES_MANTISPRIG,
	ZM_SPECIES_VERDANTIS,
	// F27 QUADRUPED (boulder-mimic drake) -- 2 stages
	ZM_SPECIES_SHARDSCALE,
	ZM_SPECIES_BOULDRAKE,
	// F28 QUADRUPED (dream-storing sheep) -- 2 stages
	ZM_SPECIES_FLEECEL,
	ZM_SPECIES_RAMBELLOW,
	// F29 BIPED (haunted armor)
	ZM_SPECIES_RUSTSHADE,
	ZM_SPECIES_HAUNTPLATE,
	ZM_SPECIES_DREADARMET,
	// F30 FLOATER_PLANTOID (updraft jellyfish)
	ZM_SPECIES_PUFFJEL,
	ZM_SPECIES_NIMBJEL,
	ZM_SPECIES_STRATOJEL,
	// F31 AVIAN (cave bat)
	ZM_SPECIES_ECHOLET,
	ZM_SPECIES_GLOOMWING,
	ZM_SPECIES_NOCTURSONG,
	// F32 AQUATIC (lotus-throne pond turtle)
	ZM_SPECIES_PADLET,
	ZM_SPECIES_LILYTURT,
	ZM_SPECIES_LOTUSTLE,
	// F33 INSECTOID (firefly streetlamp)
	ZM_SPECIES_GLOWMITE,
	ZM_SPECIES_EMBERFLY,
	ZM_SPECIES_PYRELUME,
	// F34 QUADRUPED (fortress pangolin)
	ZM_SPECIES_SCUTLET,
	ZM_SPECIES_PANGROL,
	ZM_SPECIES_FORTRESCALE,
	// F35 BLOB (living dynamo) -- 2 stages
	ZM_SPECIES_VOLTNUT,
	ZM_SPECIES_COILCORE,
	// F36 FLOATER_PLANTOID (blizzard wraith)
	ZM_SPECIES_CHILLSHADE,
	ZM_SPECIES_FROSTWRAITH,
	ZM_SPECIES_PERMAFRIGHT,
	// F37 BIPED (quarry golem boxer)
	ZM_SPECIES_PEBBLEFIST,
	ZM_SPECIES_COBBLEBRAWN,
	ZM_SPECIES_MONUMENTOR,
	// F38 FLOATER_PLANTOID (walking night mushroom)
	ZM_SPECIES_SPORELING,
	ZM_SPECIES_DUSKCAP,
	ZM_SPECIES_FUNGROVE,
	// F39 AVIAN (coastal gull) -- 2 stages
	ZM_SPECIES_SKIMMET,
	ZM_SPECIES_PELAGAIR,
	// F40 BLOB (amiable ooze) -- 2 stages
	ZM_SPECIES_GLOOPET,
	ZM_SPECIES_GLUTTONUB,
	// F41 BLOB (city-drain sludge) -- 2 stages
	ZM_SPECIES_OOZEL,
	ZM_SPECIES_MIREMASS,
	// F42 QUADRUPED (starlight foal) -- 2 stages
	ZM_SPECIES_MOONFOAL,
	ZM_SPECIES_ASTRAMARE,
	// F43 SERPENT (volcano serpent)
	ZM_SPECIES_CINDERASP,
	ZM_SPECIES_MAGMABOA,
	ZM_SPECIES_PYROTHON,
	// F44 INSECTOID (geode-boring beetle)
	ZM_SPECIES_PRISMITE,
	ZM_SPECIES_GEOBORER,
	ZM_SPECIES_CRYSTALLISK,
	// F45 AVIAN (martial crane)
	ZM_SPECIES_PECKIT,
	ZM_SPECIES_LONGSTRIDE,
	ZM_SPECIES_CRANECLASH,
	// F46 INSECTOID (badlands scorpion)
	ZM_SPECIES_STINGLET,
	ZM_SPECIES_BARBTAIL,
	ZM_SPECIES_SCORPICRAG,
	// F47 QUADRUPED (flying squirrel) -- 2 stages
	ZM_SPECIES_GLIDEKIN,
	ZM_SPECIES_SOAREL,
	// F48 SERPENT (feeble fish that isn't) -- 2 stages
	ZM_SPECIES_FLOPFIN,
	ZM_SPECIES_LEVIANTH,
	// F49 QUADRUPED (graveyard hound) -- 2 stages
	ZM_SPECIES_GRAVEPUP,
	ZM_SPECIES_DIRGEHOUND,
	// F50 BIPED (clockwork oracle) -- 2 stages
	ZM_SPECIES_COGLING,
	ZM_SPECIES_CEREBRASS,
	// F51 AVIAN (augur raven)
	ZM_SPECIES_CORVIT,
	ZM_SPECIES_OMENROOK,
	ZM_SPECIES_AUGURAVAN,
	// F52 INSECTOID (hive-mind)
	ZM_SPECIES_POLLENET,
	ZM_SPECIES_COMBWING,
	ZM_SPECIES_HIVEMIND,
	// F53 AQUATIC (deep-lake angler)
	ZM_SPECIES_LURELET,
	ZM_SPECIES_DEEPGLEAM,
	ZM_SPECIES_ABYSSVEIL,
	// F54 QUADRUPED (gold-horned antelope) -- single stage
	ZM_SPECIES_AURICORN,
	// F55 QUADRUPED (wall-mimic chameleon) -- single stage
	ZM_SPECIES_DUNELEON,
	// F56 AQUATIC (hot-spring koi) -- single stage
	ZM_SPECIES_EMBERKOI,
	// Legendaries (single stage)
	ZM_SPECIES_ZENARIS,   // L01 -- the Noon Crown
	ZM_SPECIES_NADIRATH,  // L02 -- the Under-Coil
	ZM_SPECIES_EQUINARA,  // L03 -- the Horizon-Walker

	ZM_SPECIES_COUNT,
	ZM_SPECIES_NONE = ZM_SPECIES_COUNT   // "no species" sentinel (e.g. final-stage evolves-to)
};

// The six battle stats, in canonical order (index into ZM_BaseStats::m_au).
enum ZM_STAT : u_int
{
	ZM_STAT_HP,
	ZM_STAT_ATTACK,
	ZM_STAT_DEFENSE,
	ZM_STAT_SPATTACK,
	ZM_STAT_SPDEFENSE,
	ZM_STAT_SPEED,

	ZM_STAT_COUNT
};

// A species' six base stats -- the per-species constants the Gen-III stat
// formula scales by level / IV / EV / nature (that formula lands with
// ZM_StatCalc). Values are systematically derived (ZM-D-021), not hand-tuned.
struct ZM_BaseStats
{
	u_int m_au[ZM_STAT_COUNT];
};

// One dex row. Base stats come from ZM_GetSpeciesBaseStats below; level-up
// learnsets from ZM_GetSpeciesLearnset (ZM_Learnsets.h).
struct ZM_SpeciesData
{
	ZM_SPECIES_ID	m_eId;
	const char*		m_szName;
	ZM_TYPE			m_aeTypes[2];		// [1] == ZM_TYPE_NONE for single-type species
	ZM_ARCHETYPE	m_eArchetype;
	u_int			m_uEvoStage;		// 1, 2, or 3 (1 for single-stage / legendary)
	ZM_SPECIES_ID	m_eEvolvesTo;		// next stage, or ZM_SPECIES_NONE if final
	u_int			m_uFamilyId;		// 1..56 for F-families; 57..59 for legendaries
	ZM_RARITY		m_eRarity;
};

// Table accessors (bounds-asserted). GetSpeciesData indexes by ZM_SPECIES_ID.
const ZM_SpeciesData&	ZM_GetSpeciesData(ZM_SPECIES_ID eId);
u_int					ZM_GetSpeciesCount();				// == ZM_SPECIES_COUNT
const char*				ZM_GetSpeciesName(ZM_SPECIES_ID eId);

// Rule-derived species fields (stored per-row later if per-species tuning is
// needed): size class from evo stage + archetype; the per-family generator seed
// shared by all stages of a family (S4 ZM_CreatureGen input).
ZM_SIZE_CLASS			ZM_GetSpeciesSizeClass(ZM_SPECIES_ID eId);
u_int					ZM_GetSpeciesFamilySeed(ZM_SPECIES_ID eId);

// The /8 female threshold for a graded ratio (box-6 SC-A): ZM_RollGender returns
// FEMALE when RandBelow(8) < threshold, else MALE. The three fixed ratios
// (GENDERLESS / MALE_ONLY / FEMALE_ONLY) return uZM_GENDER_RATIO_NO_ROLL -- they
// never roll. Pure, no RNG.
u_int					ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO eRatio);

// Systematically-derived per-species sex distribution (box-6 SC-A, mirrors the
// ZM-D-021/023 derived-accessor precedent -- the dex ROW is untouched): LEGENDARY
// and the inorganic BLOB body plan are GENDERLESS; RARE (starters + pseudo-legendary
// lines) are 7:1 male; every other organic species takes a deterministic family-seed
// spread over EVEN / 3:1-male / 3:1-female. Deterministic, no RNG.
ZM_GENDER_RATIO			ZM_GetSpeciesGenderRatio(ZM_SPECIES_ID eId);

// Roll a concrete gender for a freshly-created monster of eId (wild-gen, breeding
// egg-gen). One RandBelow(8) vs the ratio's female threshold, or NO draw for a fixed
// ratio (so a fixed-gender species never perturbs the pinned RNG stream). The sole
// gender randomness source; deterministic in xRng.
ZM_GENDER				ZM_RollGender(ZM_SPECIES_ID eId, ZM_BattleRNG& xRng);

// Systematically-derived base stats (ZM-D-021): a per-archetype stat profile
// scaled by evolution stage + rarity, with a deterministic per-family emphasis
// from the family seed. Deterministic, and every stat is non-decreasing along an
// evolution chain; intended to be superseded by hand-tuned per-species values in
// a later balance pass.
ZM_BaseStats			ZM_GetSpeciesBaseStats(ZM_SPECIES_ID eId);
u_int					ZM_GetSpeciesBaseStatTotal(ZM_SPECIES_ID eId);   // sum of the six
