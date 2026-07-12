#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"   // ZM_BattleRNG (ZM_RollGender draws)

// ============================================================================
// ZM_SpeciesData -- the 152-species dex table (structural roster), transcribed
// from Docs/GameDesignDocument.md section 5. Rows are in ZM_SPECIES_ID order;
// s_axSpecies[i].m_eId == i is asserted by the tests. Base stats + learnsets
// are added by later increments on this Roadmap box (DecisionLog ZM-D-020).
//
// Type columns follow the GDD: single-type species carry ZM_TYPE_NONE in the
// second slot; "X, final +Y" families gain the second type only at the final
// stage; a bare "X/Y" family is dual from stage 1.
// ============================================================================

namespace
{
	const ZM_SpeciesData s_axSpecies[ZM_SPECIES_COUNT] =
	{
		// -- F01 QUADRUPED, RARE (starter: grove-antlered deer) --
		{ ZM_SPECIES_FERNFAWN,     "Fernfawn",     { ZM_TYPE_GRASS,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_THICKETBUCK, 1,  ZM_RARITY_RARE     },
		{ ZM_SPECIES_THICKETBUCK,  "Thicketbuck",  { ZM_TYPE_GRASS,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_SYLVASTAG,   1,  ZM_RARITY_RARE     },
		{ ZM_SPECIES_SYLVASTAG,    "Sylvastag",    { ZM_TYPE_GRASS,    ZM_TYPE_EARTH }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        1,  ZM_RARITY_RARE     },
		// -- F02 BIPED, RARE (starter: self-firing kiln salamander) --
		{ ZM_SPECIES_KINDLET,      "Kindlet",      { ZM_TYPE_FIRE,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            1, ZM_SPECIES_SCORCHEL,    2,  ZM_RARITY_RARE     },
		{ ZM_SPECIES_SCORCHEL,     "Scorchel",     { ZM_TYPE_FIRE,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            2, ZM_SPECIES_PYROCLAST,   2,  ZM_RARITY_RARE     },
		{ ZM_SPECIES_PYROCLAST,    "Pyroclast",    { ZM_TYPE_FIRE,     ZM_TYPE_STONE }, ZM_ARCHETYPE_BIPED,            3, ZM_SPECIES_NONE,        2,  ZM_RARITY_RARE     },
		// -- F03 AQUATIC, RARE (starter: blade-billed duelist fish) --
		{ ZM_SPECIES_FINLET,       "Finlet",       { ZM_TYPE_WATER,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_AQUATIC,          1, ZM_SPECIES_MARLANCE,    3,  ZM_RARITY_RARE     },
		{ ZM_SPECIES_MARLANCE,     "Marlance",     { ZM_TYPE_WATER,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_AQUATIC,          2, ZM_SPECIES_TIDESABRE,   3,  ZM_RARITY_RARE     },
		{ ZM_SPECIES_TIDESABRE,    "Tidesabre",    { ZM_TYPE_WATER,    ZM_TYPE_IRON  }, ZM_ARCHETYPE_AQUATIC,          3, ZM_SPECIES_NONE,        3,  ZM_RARITY_RARE     },
		// -- F04 AVIAN, COMMON (everywhere-songbird) --
		{ ZM_SPECIES_PIPWIT,       "Pipwit",       { ZM_TYPE_NORMAL,   ZM_TYPE_SKY   }, ZM_ARCHETYPE_AVIAN,            1, ZM_SPECIES_TRILLARK,    4,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_TRILLARK,     "Trillark",     { ZM_TYPE_NORMAL,   ZM_TYPE_SKY   }, ZM_ARCHETYPE_AVIAN,            2, ZM_SPECIES_STRATAVIS,   4,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_STRATAVIS,    "Stratavis",    { ZM_TYPE_NORMAL,   ZM_TYPE_SKY   }, ZM_ARCHETYPE_AVIAN,            3, ZM_SPECIES_NONE,        4,  ZM_RARITY_COMMON   },
		// -- F05 QUADRUPED, COMMON (seed-hoarding vole) --
		{ ZM_SPECIES_NIBBIN,       "Nibbin",       { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_HOARDEL,     5,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_HOARDEL,      "Hoardel",      { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_GRAINMAW,    5,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_GRAINMAW,     "Grainmaw",     { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        5,  ZM_RARITY_COMMON   },
		// -- F06 INSECTOID, COMMON (early butterfly) --
		{ ZM_SPECIES_WRIGGLET,     "Wrigglet",     { ZM_TYPE_SWARM,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_CRADLEWRAP,  6,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_CRADLEWRAP,   "Cradlewrap",   { ZM_TYPE_SWARM,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_AURELWING,   6,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_AURELWING,    "Aurelwing",    { ZM_TYPE_SWARM,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        6,  ZM_RARITY_COMMON   },
		// -- F07 QUADRUPED, COMMON (gate-guarding hound) --
		{ ZM_SPECIES_STRAYLING,    "Strayling",    { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_WARDHUND,    7,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_WARDHUND,     "Wardhund",     { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_NONE,        7,  ZM_RARITY_COMMON   },
		// -- F08 FLOATER_PLANTOID, COMMON (dandelion drifter) --
		{ ZM_SPECIES_PUFFSEED,     "Puffseed",     { ZM_TYPE_GRASS,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_FLOATER_PLANTOID, 1, ZM_SPECIES_DANDELIFT,   8,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_DANDELIFT,    "Dandelift",    { ZM_TYPE_GRASS,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_FLOATER_PLANTOID, 2, ZM_SPECIES_ZEPHYRBLOOM, 8,  ZM_RARITY_COMMON   },
		{ ZM_SPECIES_ZEPHYRBLOOM,  "Zephyrbloom",  { ZM_TYPE_GRASS,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_FLOATER_PLANTOID, 3, ZM_SPECIES_NONE,        8,  ZM_RARITY_COMMON   },
		// -- F09 QUADRUPED, UNCOMMON (storm-chasing fox) --
		{ ZM_SPECIES_SPARKIT,      "Sparkit",      { ZM_TYPE_ELECTRIC, ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_AMPTAIL,     9,  ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_AMPTAIL,      "Amptail",      { ZM_TYPE_ELECTRIC, ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_FULGURUN,    9,  ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_FULGURUN,     "Fulgurun",     { ZM_TYPE_ELECTRIC, ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        9,  ZM_RARITY_UNCOMMON },
		// -- F10 BLOB, COMMON (walking cairn) --
		{ ZM_SPECIES_RUBBLET,      "Rubblet",      { ZM_TYPE_STONE,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             1, ZM_SPECIES_CAIRNGO,     10, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_CAIRNGO,      "Cairngo",      { ZM_TYPE_STONE,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             2, ZM_SPECIES_MONOLODE,    10, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_MONOLODE,     "Monolode",     { ZM_TYPE_STONE,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             3, ZM_SPECIES_NONE,        10, ZM_RARITY_COMMON   },
		// -- F11 AQUATIC, COMMON (universal river fish) --
		{ ZM_SPECIES_MINNET,       "Minnet",       { ZM_TYPE_WATER,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_AQUATIC,          1, ZM_SPECIES_SHOALFIN,    11, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_SHOALFIN,     "Shoalfin",     { ZM_TYPE_WATER,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_AQUATIC,          2, ZM_SPECIES_TORRENTFIN,  11, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_TORRENTFIN,   "Torrentfin",   { ZM_TYPE_WATER,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_AQUATIC,          3, ZM_SPECIES_NONE,        11, ZM_RARITY_COMMON   },
		// -- F12 QUADRUPED, UNCOMMON (toxin-sweating bog newt) --
		{ ZM_SPECIES_MIRELET,      "Mirelet",      { ZM_TYPE_VENOM,    ZM_TYPE_WATER }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_BOGBANE,     12, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_BOGBANE,      "Bogbane",      { ZM_TYPE_VENOM,    ZM_TYPE_WATER }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_VENOMIRE,    12, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_VENOMIRE,     "Venomire",     { ZM_TYPE_VENOM,    ZM_TYPE_WATER }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        12, ZM_RARITY_UNCOMMON },
		// -- F13 BIPED, UNCOMMON (boxing hare) --
		{ ZM_SPECIES_SCRAPLING,    "Scrapling",    { ZM_TYPE_BRAWL,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            1, ZM_SPECIES_SPARHARE,    13, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_SPARHARE,     "Sparhare",     { ZM_TYPE_BRAWL,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            2, ZM_SPECIES_GRANDGUARD,  13, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_GRANDGUARD,   "Grandguard",   { ZM_TYPE_BRAWL,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            3, ZM_SPECIES_NONE,        13, ZM_RARITY_UNCOMMON },
		// -- F14 FLOATER_PLANTOID, UNCOMMON (will-o'-the-wisp) --
		{ ZM_SPECIES_WISPET,       "Wispet",       { ZM_TYPE_PHANTOM,  ZM_TYPE_NONE  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 1, ZM_SPECIES_GLIMMOURN,   14, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_GLIMMOURN,    "Glimmourn",    { ZM_TYPE_PHANTOM,  ZM_TYPE_NONE  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 2, ZM_SPECIES_MOURNLIGHT,  14, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_MOURNLIGHT,   "Mournlight",   { ZM_TYPE_PHANTOM,  ZM_TYPE_NONE  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 3, ZM_SPECIES_NONE,        14, ZM_RARITY_UNCOMMON },
		// -- F15 BIPED, RARE (blindfolded seer) --
		{ ZM_SPECIES_TRANCET,      "Trancet",      { ZM_TYPE_MIND,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            1, ZM_SPECIES_MESMEREL,    15, ZM_RARITY_RARE     },
		{ ZM_SPECIES_MESMEREL,     "Mesmerel",     { ZM_TYPE_MIND,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            2, ZM_SPECIES_ORACLYNE,    15, ZM_RARITY_RARE     },
		{ ZM_SPECIES_ORACLYNE,     "Oraclyne",     { ZM_TYPE_MIND,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BIPED,            3, ZM_SPECIES_NONE,        15, ZM_RARITY_RARE     },
		// -- F16 QUADRUPED, UNCOMMON (alpine ice elk) --
		{ ZM_SPECIES_FRISKET,      "Frisket",      { ZM_TYPE_ICE,      ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_SNOWLOPE,    16, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_SNOWLOPE,     "Snowlope",     { ZM_TYPE_ICE,      ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_GLACIELK,    16, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_GLACIELK,     "Glacielk",     { ZM_TYPE_ICE,      ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        16, ZM_RARITY_UNCOMMON },
		// -- F17 SERPENT, RARE (pseudo-legendary climbing wyrm) --
		{ ZM_SPECIES_WYRMLING,     "Wyrmling",     { ZM_TYPE_DRAKE,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_SERPENT,          1, ZM_SPECIES_CRAGWYRM,    17, ZM_RARITY_RARE     },
		{ ZM_SPECIES_CRAGWYRM,     "Cragwyrm",     { ZM_TYPE_DRAKE,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_SERPENT,          2, ZM_SPECIES_ZENITHRAX,   17, ZM_RARITY_RARE     },
		{ ZM_SPECIES_ZENITHRAX,    "Zenithrax",    { ZM_TYPE_DRAKE,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_SERPENT,          3, ZM_SPECIES_NONE,        17, ZM_RARITY_RARE     },
		// -- F18 BLOB, UNCOMMON (living foundry slag) --
		{ ZM_SPECIES_SLAGLET,      "Slaglet",      { ZM_TYPE_IRON,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             1, ZM_SPECIES_FERRALUMP,   18, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_FERRALUMP,    "Ferralump",    { ZM_TYPE_IRON,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             2, ZM_SPECIES_SMELTITAN,   18, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_SMELTITAN,    "Smeltitan",    { ZM_TYPE_IRON,     ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             3, ZM_SPECIES_NONE,        18, ZM_RARITY_UNCOMMON },
		// -- F19 FLOATER_PLANTOID, RARE (flower-ring dancer) --
		{ ZM_SPECIES_FAYLING,      "Fayling",      { ZM_TYPE_FEY,      ZM_TYPE_NONE  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 1, ZM_SPECIES_CHIMESPRITE, 19, ZM_RARITY_RARE     },
		{ ZM_SPECIES_CHIMESPRITE,  "Chimesprite",  { ZM_TYPE_FEY,      ZM_TYPE_NONE  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 2, ZM_SPECIES_SYLPHARA,    19, ZM_RARITY_RARE     },
		{ ZM_SPECIES_SYLPHARA,     "Sylphara",     { ZM_TYPE_FEY,      ZM_TYPE_NONE  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 3, ZM_SPECIES_NONE,        19, ZM_RARITY_RARE     },
		// -- F20 QUADRUPED, UNCOMMON (dusk-woven moor cat) --
		{ ZM_SPECIES_SHADELET,     "Shadelet",     { ZM_TYPE_UMBRAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_DUSKSTALK,   20, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_DUSKSTALK,    "Duskstalk",    { ZM_TYPE_UMBRAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_NIGHTREAVE,  20, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_NIGHTREAVE,   "Nightreave",   { ZM_TYPE_UMBRAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        20, ZM_RARITY_UNCOMMON },
		// -- F21 INSECTOID, COMMON (hedge spider) --
		{ ZM_SPECIES_LOOMITE,      "Loomite",      { ZM_TYPE_SWARM,    ZM_TYPE_VENOM }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_SILKLURK,    21, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_SILKLURK,     "Silklurk",     { ZM_TYPE_SWARM,    ZM_TYPE_VENOM }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_WEAVENOM,    21, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_WEAVENOM,     "Weavenom",     { ZM_TYPE_SWARM,    ZM_TYPE_VENOM }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        21, ZM_RARITY_COMMON   },
		// -- F22 QUADRUPED, COMMON (route-digging mole) --
		{ ZM_SPECIES_BURRIT,       "Burrit",       { ZM_TYPE_EARTH,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_GRAVELOW,    22, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_GRAVELOW,     "Gravelow",     { ZM_TYPE_EARTH,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_TERRADRILL,  22, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_TERRADRILL,   "Terradrill",   { ZM_TYPE_EARTH,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        22, ZM_RARITY_COMMON   },
		// -- F23 AVIAN, RARE (thunderhead storm-petrel) --
		{ ZM_SPECIES_SQUALLET,     "Squallet",     { ZM_TYPE_SKY,      ZM_TYPE_ELECTRIC }, ZM_ARCHETYPE_AVIAN,         1, ZM_SPECIES_GALECREST,   23, ZM_RARITY_RARE     },
		{ ZM_SPECIES_GALECREST,    "Galecrest",    { ZM_TYPE_SKY,      ZM_TYPE_ELECTRIC }, ZM_ARCHETYPE_AVIAN,         2, ZM_SPECIES_THUNDEROC,   23, ZM_RARITY_RARE     },
		{ ZM_SPECIES_THUNDEROC,    "Thunderoc",    { ZM_TYPE_SKY,      ZM_TYPE_ELECTRIC }, ZM_ARCHETYPE_AVIAN,         3, ZM_SPECIES_NONE,        23, ZM_RARITY_RARE     },
		// -- F24 AQUATIC, UNCOMMON (ice-floe seal to aurora orca) --
		{ ZM_SPECIES_FLOELET,      "Floelet",      { ZM_TYPE_WATER,    ZM_TYPE_ICE   }, ZM_ARCHETYPE_AQUATIC,          1, ZM_SPECIES_PINNIFLOE,   24, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_PINNIFLOE,    "Pinnifloe",    { ZM_TYPE_WATER,    ZM_TYPE_ICE   }, ZM_ARCHETYPE_AQUATIC,          2, ZM_SPECIES_AURORCA,     24, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_AURORCA,      "Aurorca",      { ZM_TYPE_WATER,    ZM_TYPE_ICE   }, ZM_ARCHETYPE_AQUATIC,          3, ZM_SPECIES_NONE,        24, ZM_RARITY_UNCOMMON },
		// -- F25 QUADRUPED, UNCOMMON (volcanic scavenger jackal) --
		{ ZM_SPECIES_CINDERJACK,   "Cinderjack",   { ZM_TYPE_FIRE,     ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_QUADRUPED,       1, ZM_SPECIES_ASHENHOWL,   25, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_ASHENHOWL,    "Ashenhowl",    { ZM_TYPE_FIRE,     ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_QUADRUPED,       2, ZM_SPECIES_NONE,        25, ZM_RARITY_UNCOMMON },
		// -- F26 INSECTOID, UNCOMMON (pruning-blade mantis) --
		{ ZM_SPECIES_BLADEBUD,     "Bladebud",     { ZM_TYPE_GRASS,    ZM_TYPE_SWARM }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_MANTISPRIG,  26, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_MANTISPRIG,   "Mantisprig",   { ZM_TYPE_GRASS,    ZM_TYPE_SWARM }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_VERDANTIS,   26, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_VERDANTIS,    "Verdantis",    { ZM_TYPE_GRASS,    ZM_TYPE_SWARM }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        26, ZM_RARITY_UNCOMMON },
		// -- F27 QUADRUPED, RARE (boulder-mimic drake) --
		{ ZM_SPECIES_SHARDSCALE,   "Shardscale",   { ZM_TYPE_STONE,    ZM_TYPE_DRAKE }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_BOULDRAKE,   27, ZM_RARITY_RARE     },
		{ ZM_SPECIES_BOULDRAKE,    "Bouldrake",    { ZM_TYPE_STONE,    ZM_TYPE_DRAKE }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_NONE,        27, ZM_RARITY_RARE     },
		// -- F28 QUADRUPED, UNCOMMON (dream-storing sheep) --
		{ ZM_SPECIES_FLEECEL,      "Fleecel",      { ZM_TYPE_NORMAL,   ZM_TYPE_FEY   }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_RAMBELLOW,   28, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_RAMBELLOW,    "Rambellow",    { ZM_TYPE_NORMAL,   ZM_TYPE_FEY   }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_NONE,        28, ZM_RARITY_UNCOMMON },
		// -- F29 BIPED, RARE (haunted armor) --
		{ ZM_SPECIES_RUSTSHADE,    "Rustshade",    { ZM_TYPE_PHANTOM,  ZM_TYPE_IRON  }, ZM_ARCHETYPE_BIPED,            1, ZM_SPECIES_HAUNTPLATE,  29, ZM_RARITY_RARE     },
		{ ZM_SPECIES_HAUNTPLATE,   "Hauntplate",   { ZM_TYPE_PHANTOM,  ZM_TYPE_IRON  }, ZM_ARCHETYPE_BIPED,            2, ZM_SPECIES_DREADARMET,  29, ZM_RARITY_RARE     },
		{ ZM_SPECIES_DREADARMET,   "Dreadarmet",   { ZM_TYPE_PHANTOM,  ZM_TYPE_IRON  }, ZM_ARCHETYPE_BIPED,            3, ZM_SPECIES_NONE,        29, ZM_RARITY_RARE     },
		// -- F30 FLOATER_PLANTOID, COMMON (updraft jellyfish) --
		{ ZM_SPECIES_PUFFJEL,      "Puffjel",      { ZM_TYPE_SKY,      ZM_TYPE_MIND  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 1, ZM_SPECIES_NIMBJEL,     30, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_NIMBJEL,      "Nimbjel",      { ZM_TYPE_SKY,      ZM_TYPE_MIND  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 2, ZM_SPECIES_STRATOJEL,   30, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_STRATOJEL,    "Stratojel",    { ZM_TYPE_SKY,      ZM_TYPE_MIND  }, ZM_ARCHETYPE_FLOATER_PLANTOID, 3, ZM_SPECIES_NONE,        30, ZM_RARITY_COMMON   },
		// -- F31 AVIAN, COMMON (cave bat) --
		{ ZM_SPECIES_ECHOLET,      "Echolet",      { ZM_TYPE_VENOM,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_AVIAN,           1, ZM_SPECIES_GLOOMWING,   31, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_GLOOMWING,    "Gloomwing",    { ZM_TYPE_VENOM,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_AVIAN,           2, ZM_SPECIES_NOCTURSONG,  31, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_NOCTURSONG,   "Noctursong",   { ZM_TYPE_VENOM,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_AVIAN,           3, ZM_SPECIES_NONE,        31, ZM_RARITY_COMMON   },
		// -- F32 AQUATIC, UNCOMMON (lotus-throne pond turtle) --
		{ ZM_SPECIES_PADLET,       "Padlet",       { ZM_TYPE_WATER,    ZM_TYPE_GRASS }, ZM_ARCHETYPE_AQUATIC,          1, ZM_SPECIES_LILYTURT,    32, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_LILYTURT,     "Lilyturt",     { ZM_TYPE_WATER,    ZM_TYPE_GRASS }, ZM_ARCHETYPE_AQUATIC,          2, ZM_SPECIES_LOTUSTLE,    32, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_LOTUSTLE,     "Lotustle",     { ZM_TYPE_WATER,    ZM_TYPE_GRASS }, ZM_ARCHETYPE_AQUATIC,          3, ZM_SPECIES_NONE,        32, ZM_RARITY_UNCOMMON },
		// -- F33 INSECTOID, UNCOMMON (firefly streetlamp) --
		{ ZM_SPECIES_GLOWMITE,     "Glowmite",     { ZM_TYPE_FIRE,     ZM_TYPE_SWARM }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_EMBERFLY,    33, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_EMBERFLY,     "Emberfly",     { ZM_TYPE_FIRE,     ZM_TYPE_SWARM }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_PYRELUME,    33, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_PYRELUME,     "Pyrelume",     { ZM_TYPE_FIRE,     ZM_TYPE_SWARM }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        33, ZM_RARITY_UNCOMMON },
		// -- F34 QUADRUPED, UNCOMMON (fortress pangolin) --
		{ ZM_SPECIES_SCUTLET,      "Scutlet",      { ZM_TYPE_EARTH,    ZM_TYPE_STONE }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_PANGROL,     34, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_PANGROL,      "Pangrol",      { ZM_TYPE_EARTH,    ZM_TYPE_STONE }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_FORTRESCALE, 34, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_FORTRESCALE,  "Fortrescale",  { ZM_TYPE_EARTH,    ZM_TYPE_STONE }, ZM_ARCHETYPE_QUADRUPED,        3, ZM_SPECIES_NONE,        34, ZM_RARITY_UNCOMMON },
		// -- F35 BLOB, UNCOMMON (living dynamo) --
		{ ZM_SPECIES_VOLTNUT,      "Voltnut",      { ZM_TYPE_ELECTRIC, ZM_TYPE_IRON  }, ZM_ARCHETYPE_BLOB,             1, ZM_SPECIES_COILCORE,    35, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_COILCORE,     "Coilcore",     { ZM_TYPE_ELECTRIC, ZM_TYPE_IRON  }, ZM_ARCHETYPE_BLOB,             2, ZM_SPECIES_NONE,        35, ZM_RARITY_UNCOMMON },
		// -- F36 FLOATER_PLANTOID, RARE (blizzard wraith) --
		{ ZM_SPECIES_CHILLSHADE,   "Chillshade",   { ZM_TYPE_ICE,      ZM_TYPE_PHANTOM }, ZM_ARCHETYPE_FLOATER_PLANTOID, 1, ZM_SPECIES_FROSTWRAITH, 36, ZM_RARITY_RARE   },
		{ ZM_SPECIES_FROSTWRAITH,  "Frostwraith",  { ZM_TYPE_ICE,      ZM_TYPE_PHANTOM }, ZM_ARCHETYPE_FLOATER_PLANTOID, 2, ZM_SPECIES_PERMAFRIGHT, 36, ZM_RARITY_RARE   },
		{ ZM_SPECIES_PERMAFRIGHT,  "Permafright",  { ZM_TYPE_ICE,      ZM_TYPE_PHANTOM }, ZM_ARCHETYPE_FLOATER_PLANTOID, 3, ZM_SPECIES_NONE,        36, ZM_RARITY_RARE   },
		// -- F37 BIPED, UNCOMMON (quarry golem boxer) --
		{ ZM_SPECIES_PEBBLEFIST,   "Pebblefist",   { ZM_TYPE_BRAWL,    ZM_TYPE_STONE }, ZM_ARCHETYPE_BIPED,            1, ZM_SPECIES_COBBLEBRAWN, 37, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_COBBLEBRAWN,  "Cobblebrawn",  { ZM_TYPE_BRAWL,    ZM_TYPE_STONE }, ZM_ARCHETYPE_BIPED,            2, ZM_SPECIES_MONUMENTOR,  37, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_MONUMENTOR,   "Monumentor",   { ZM_TYPE_BRAWL,    ZM_TYPE_STONE }, ZM_ARCHETYPE_BIPED,            3, ZM_SPECIES_NONE,        37, ZM_RARITY_UNCOMMON },
		// -- F38 FLOATER_PLANTOID, COMMON (walking night mushroom) --
		{ ZM_SPECIES_SPORELING,    "Sporeling",    { ZM_TYPE_GRASS,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_FLOATER_PLANTOID, 1, ZM_SPECIES_DUSKCAP,     38, ZM_RARITY_COMMON  },
		{ ZM_SPECIES_DUSKCAP,      "Duskcap",      { ZM_TYPE_GRASS,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_FLOATER_PLANTOID, 2, ZM_SPECIES_FUNGROVE,    38, ZM_RARITY_COMMON  },
		{ ZM_SPECIES_FUNGROVE,     "Fungrove",     { ZM_TYPE_GRASS,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_FLOATER_PLANTOID, 3, ZM_SPECIES_NONE,        38, ZM_RARITY_COMMON  },
		// -- F39 AVIAN, COMMON (coastal gull) --
		{ ZM_SPECIES_SKIMMET,      "Skimmet",      { ZM_TYPE_WATER,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_AVIAN,            1, ZM_SPECIES_PELAGAIR,    39, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_PELAGAIR,     "Pelagair",     { ZM_TYPE_WATER,    ZM_TYPE_SKY   }, ZM_ARCHETYPE_AVIAN,            2, ZM_SPECIES_NONE,        39, ZM_RARITY_COMMON   },
		// -- F40 BLOB, COMMON (amiable ooze) --
		{ ZM_SPECIES_GLOOPET,      "Gloopet",      { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             1, ZM_SPECIES_GLUTTONUB,   40, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_GLUTTONUB,    "Gluttonub",    { ZM_TYPE_NORMAL,   ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             2, ZM_SPECIES_NONE,        40, ZM_RARITY_COMMON   },
		// -- F41 BLOB, COMMON (city-drain sludge) --
		{ ZM_SPECIES_OOZEL,        "Oozel",        { ZM_TYPE_VENOM,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             1, ZM_SPECIES_MIREMASS,    41, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_MIREMASS,     "Miremass",     { ZM_TYPE_VENOM,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_BLOB,             2, ZM_SPECIES_NONE,        41, ZM_RARITY_COMMON   },
		// -- F42 QUADRUPED, RARE (starlight foal) --
		{ ZM_SPECIES_MOONFOAL,     "Moonfoal",     { ZM_TYPE_FEY,      ZM_TYPE_MIND  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_ASTRAMARE,   42, ZM_RARITY_RARE     },
		{ ZM_SPECIES_ASTRAMARE,    "Astramare",    { ZM_TYPE_FEY,      ZM_TYPE_MIND  }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_NONE,        42, ZM_RARITY_RARE     },
		// -- F43 SERPENT, RARE (volcano serpent) --
		{ ZM_SPECIES_CINDERASP,    "Cinderasp",    { ZM_TYPE_FIRE,     ZM_TYPE_DRAKE }, ZM_ARCHETYPE_SERPENT,          1, ZM_SPECIES_MAGMABOA,    43, ZM_RARITY_RARE     },
		{ ZM_SPECIES_MAGMABOA,     "Magmaboa",     { ZM_TYPE_FIRE,     ZM_TYPE_DRAKE }, ZM_ARCHETYPE_SERPENT,          2, ZM_SPECIES_PYROTHON,    43, ZM_RARITY_RARE     },
		{ ZM_SPECIES_PYROTHON,     "Pyrothon",     { ZM_TYPE_FIRE,     ZM_TYPE_DRAKE }, ZM_ARCHETYPE_SERPENT,          3, ZM_SPECIES_NONE,        43, ZM_RARITY_RARE     },
		// -- F44 INSECTOID, RARE (geode-boring beetle) --
		{ ZM_SPECIES_PRISMITE,     "Prismite",     { ZM_TYPE_SWARM,    ZM_TYPE_STONE }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_GEOBORER,    44, ZM_RARITY_RARE     },
		{ ZM_SPECIES_GEOBORER,     "Geoborer",     { ZM_TYPE_SWARM,    ZM_TYPE_STONE }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_CRYSTALLISK, 44, ZM_RARITY_RARE     },
		{ ZM_SPECIES_CRYSTALLISK,  "Crystallisk",  { ZM_TYPE_SWARM,    ZM_TYPE_STONE }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        44, ZM_RARITY_RARE     },
		// -- F45 AVIAN, UNCOMMON (martial crane) --
		{ ZM_SPECIES_PECKIT,       "Peckit",       { ZM_TYPE_SKY,      ZM_TYPE_BRAWL }, ZM_ARCHETYPE_AVIAN,            1, ZM_SPECIES_LONGSTRIDE,  45, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_LONGSTRIDE,   "Longstride",   { ZM_TYPE_SKY,      ZM_TYPE_BRAWL }, ZM_ARCHETYPE_AVIAN,            2, ZM_SPECIES_CRANECLASH,  45, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_CRANECLASH,   "Craneclash",   { ZM_TYPE_SKY,      ZM_TYPE_BRAWL }, ZM_ARCHETYPE_AVIAN,            3, ZM_SPECIES_NONE,        45, ZM_RARITY_UNCOMMON },
		// -- F46 INSECTOID, UNCOMMON (badlands scorpion) --
		{ ZM_SPECIES_STINGLET,     "Stinglet",     { ZM_TYPE_EARTH,    ZM_TYPE_VENOM }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_BARBTAIL,    46, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_BARBTAIL,     "Barbtail",     { ZM_TYPE_EARTH,    ZM_TYPE_VENOM }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_SCORPICRAG,  46, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_SCORPICRAG,   "Scorpicrag",   { ZM_TYPE_EARTH,    ZM_TYPE_VENOM }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        46, ZM_RARITY_UNCOMMON },
		// -- F47 QUADRUPED, COMMON (flying squirrel) --
		{ ZM_SPECIES_GLIDEKIN,     "Glidekin",     { ZM_TYPE_NORMAL,   ZM_TYPE_SKY   }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_SOAREL,      47, ZM_RARITY_COMMON   },
		{ ZM_SPECIES_SOAREL,       "Soarel",       { ZM_TYPE_NORMAL,   ZM_TYPE_SKY   }, ZM_ARCHETYPE_QUADRUPED,        2, ZM_SPECIES_NONE,        47, ZM_RARITY_COMMON   },
		// -- F48 SERPENT, RARE (feeble fish that isn't) --
		{ ZM_SPECIES_FLOPFIN,      "Flopfin",      { ZM_TYPE_WATER,    ZM_TYPE_NONE  }, ZM_ARCHETYPE_SERPENT,          1, ZM_SPECIES_LEVIANTH,    48, ZM_RARITY_RARE     },
		{ ZM_SPECIES_LEVIANTH,     "Levianth",     { ZM_TYPE_WATER,    ZM_TYPE_DRAKE }, ZM_ARCHETYPE_SERPENT,          2, ZM_SPECIES_NONE,        48, ZM_RARITY_RARE     },
		// -- F49 QUADRUPED, UNCOMMON (graveyard hound) --
		{ ZM_SPECIES_GRAVEPUP,     "Gravepup",     { ZM_TYPE_PHANTOM,  ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_QUADRUPED,       1, ZM_SPECIES_DIRGEHOUND,  49, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_DIRGEHOUND,   "Dirgehound",   { ZM_TYPE_PHANTOM,  ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_QUADRUPED,       2, ZM_SPECIES_NONE,        49, ZM_RARITY_UNCOMMON },
		// -- F50 BIPED, RARE (clockwork oracle) --
		{ ZM_SPECIES_COGLING,      "Cogling",      { ZM_TYPE_MIND,     ZM_TYPE_IRON  }, ZM_ARCHETYPE_BIPED,            1, ZM_SPECIES_CEREBRASS,   50, ZM_RARITY_RARE     },
		{ ZM_SPECIES_CEREBRASS,    "Cerebrass",    { ZM_TYPE_MIND,     ZM_TYPE_IRON  }, ZM_ARCHETYPE_BIPED,            2, ZM_SPECIES_NONE,        50, ZM_RARITY_RARE     },
		// -- F51 AVIAN, UNCOMMON (augur raven) --
		{ ZM_SPECIES_CORVIT,       "Corvit",       { ZM_TYPE_UMBRAL,   ZM_TYPE_MIND  }, ZM_ARCHETYPE_AVIAN,            1, ZM_SPECIES_OMENROOK,    51, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_OMENROOK,     "Omenrook",     { ZM_TYPE_UMBRAL,   ZM_TYPE_MIND  }, ZM_ARCHETYPE_AVIAN,            2, ZM_SPECIES_AUGURAVAN,   51, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_AUGURAVAN,    "Auguravan",    { ZM_TYPE_UMBRAL,   ZM_TYPE_MIND  }, ZM_ARCHETYPE_AVIAN,            3, ZM_SPECIES_NONE,        51, ZM_RARITY_UNCOMMON },
		// -- F52 INSECTOID, UNCOMMON (hive-mind) --
		{ ZM_SPECIES_POLLENET,     "Pollenet",     { ZM_TYPE_SWARM,    ZM_TYPE_MIND  }, ZM_ARCHETYPE_INSECTOID,        1, ZM_SPECIES_COMBWING,    52, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_COMBWING,     "Combwing",     { ZM_TYPE_SWARM,    ZM_TYPE_MIND  }, ZM_ARCHETYPE_INSECTOID,        2, ZM_SPECIES_HIVEMIND,    52, ZM_RARITY_UNCOMMON },
		{ ZM_SPECIES_HIVEMIND,     "Hivemind",     { ZM_TYPE_SWARM,    ZM_TYPE_MIND  }, ZM_ARCHETYPE_INSECTOID,        3, ZM_SPECIES_NONE,        52, ZM_RARITY_UNCOMMON },
		// -- F53 AQUATIC, RARE (deep-lake angler) --
		{ ZM_SPECIES_LURELET,      "Lurelet",      { ZM_TYPE_WATER,    ZM_TYPE_PHANTOM }, ZM_ARCHETYPE_AQUATIC,        1, ZM_SPECIES_DEEPGLEAM,   53, ZM_RARITY_RARE     },
		{ ZM_SPECIES_DEEPGLEAM,    "Deepgleam",    { ZM_TYPE_WATER,    ZM_TYPE_PHANTOM }, ZM_ARCHETYPE_AQUATIC,        2, ZM_SPECIES_ABYSSVEIL,   53, ZM_RARITY_RARE     },
		{ ZM_SPECIES_ABYSSVEIL,    "Abyssveil",    { ZM_TYPE_WATER,    ZM_TYPE_PHANTOM }, ZM_ARCHETYPE_AQUATIC,        3, ZM_SPECIES_NONE,        53, ZM_RARITY_RARE     },
		// -- F54 QUADRUPED, RARE (single: gold-horned antelope) --
		{ ZM_SPECIES_AURICORN,     "Auricorn",     { ZM_TYPE_IRON,     ZM_TYPE_FEY   }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_NONE,        54, ZM_RARITY_RARE     },
		// -- F55 QUADRUPED, RARE (single: wall-mimic chameleon) --
		{ ZM_SPECIES_DUNELEON,     "Duneleon",     { ZM_TYPE_EARTH,    ZM_TYPE_UMBRAL }, ZM_ARCHETYPE_QUADRUPED,       1, ZM_SPECIES_NONE,        55, ZM_RARITY_RARE     },
		// -- F56 AQUATIC, RARE (single: hot-spring koi) --
		{ ZM_SPECIES_EMBERKOI,     "Emberkoi",     { ZM_TYPE_FIRE,     ZM_TYPE_WATER }, ZM_ARCHETYPE_AQUATIC,          1, ZM_SPECIES_NONE,        56, ZM_RARITY_RARE     },
		// -- Legendaries (single stage) --
		{ ZM_SPECIES_ZENARIS,      "Zenaris",      { ZM_TYPE_SKY,      ZM_TYPE_FIRE  }, ZM_ARCHETYPE_AVIAN,            1, ZM_SPECIES_NONE,        57, ZM_RARITY_LEGENDARY },
		{ ZM_SPECIES_NADIRATH,     "Nadirath",     { ZM_TYPE_UMBRAL,   ZM_TYPE_EARTH }, ZM_ARCHETYPE_SERPENT,          1, ZM_SPECIES_NONE,        58, ZM_RARITY_LEGENDARY },
		{ ZM_SPECIES_EQUINARA,     "Equinara",     { ZM_TYPE_FEY,      ZM_TYPE_MIND  }, ZM_ARCHETYPE_QUADRUPED,        1, ZM_SPECIES_NONE,        59, ZM_RARITY_LEGENDARY },
	};
}

const ZM_SpeciesData& ZM_GetSpeciesData(ZM_SPECIES_ID eId)
{
	Zenith_Assert(eId < ZM_SPECIES_COUNT, "ZM_GetSpeciesData: species id out of range (%u)", (u_int)eId);
	return s_axSpecies[eId];
}

u_int ZM_GetSpeciesCount()
{
	return ZM_SPECIES_COUNT;
}

const char* ZM_GetSpeciesName(ZM_SPECIES_ID eId)
{
	if (eId >= ZM_SPECIES_COUNT)
	{
		return "NONE";
	}
	return s_axSpecies[eId].m_szName;
}

ZM_SIZE_CLASS ZM_GetSpeciesSizeClass(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);

	// Base scale from evolution stage: stage 1 SMALL, 2 MEDIUM, 3 LARGE.
	int iSize = (int)ZM_SIZE_SMALL + ((int)xData.m_uEvoStage - 1);

	// Body-plan nudges: small fliers/bugs/drifters read a step smaller, bulky
	// serpents/blobs a step larger. Constant within a family, so size stays
	// non-decreasing along an evolution chain.
	switch (xData.m_eArchetype)
	{
	case ZM_ARCHETYPE_AVIAN:
	case ZM_ARCHETYPE_INSECTOID:
	case ZM_ARCHETYPE_FLOATER_PLANTOID:
		iSize -= 1;
		break;
	case ZM_ARCHETYPE_SERPENT:
	case ZM_ARCHETYPE_BLOB:
		iSize += 1;
		break;
	default:
		break;
	}

	// Legendaries loom regardless of stage.
	if (xData.m_eRarity == ZM_RARITY_LEGENDARY)
	{
		iSize = (int)ZM_SIZE_HUGE;
	}

	if (iSize < (int)ZM_SIZE_TINY) { iSize = (int)ZM_SIZE_TINY; }
	if (iSize > (int)ZM_SIZE_HUGE) { iSize = (int)ZM_SIZE_HUGE; }
	return (ZM_SIZE_CLASS)iSize;
}

u_int ZM_GetSpeciesFamilySeed(ZM_SPECIES_ID eId)
{
	// Shared by all stages of a family; Knuth multiplicative hash of the family
	// id gives a well-spread, deterministic, per-family-unique generator seed.
	const u_int uFamilyId = ZM_GetSpeciesData(eId).m_uFamilyId;
	return uFamilyId * 2654435761u;
}

namespace
{
	// Gender rolls draw one unbiased value in [0,8): the mainline /8 sex threshold.
	static const u_int uZM_GENDER_ROLL_DENOM = 8u;
}

u_int ZM_GenderRatioFemaleThresholdOutOf8(ZM_GENDER_RATIO eRatio)
{
	// /8 female-outcome counts: MALE_7_1 = 1 female of 8, ..., FEMALE_7_1 = 7 of 8.
	switch (eRatio)
	{
	case ZM_GENDER_RATIO_MALE_7_1:   return 1u;
	case ZM_GENDER_RATIO_MALE_3_1:   return 2u;
	case ZM_GENDER_RATIO_EVEN:       return 4u;
	case ZM_GENDER_RATIO_FEMALE_3_1: return 6u;
	case ZM_GENDER_RATIO_FEMALE_7_1: return 7u;
	default:
		// GENDERLESS / MALE_ONLY / FEMALE_ONLY: resolved with no /8 draw.
		return uZM_GENDER_RATIO_NO_ROLL;
	}
}

ZM_GENDER_RATIO ZM_GetSpeciesGenderRatio(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);

	// Legendaries and the inorganic BLOB body plan have no sex (they breed only via
	// the universal breeder, box-6 SC-B). Order matters: the body-plan test precedes
	// the RARE test, so a hypothetical rare blob still reads GENDERLESS.
	if (xData.m_eRarity == ZM_RARITY_LEGENDARY)  { return ZM_GENDER_RATIO_GENDERLESS; }
	if (xData.m_eArchetype == ZM_ARCHETYPE_BLOB) { return ZM_GENDER_RATIO_GENDERLESS; }

	// Starters + pseudo-legendary lines (all RARE) follow the mainline 7:1-male
	// starter convention.
	if (xData.m_eRarity == ZM_RARITY_RARE) { return ZM_GENDER_RATIO_MALE_7_1; }

	// Every other organic COMMON/UNCOMMON species: a deterministic family-seed spread.
	// A different seed slice than the base-stat emphasis (>> 3) decorrelates the two;
	// EVEN is the plurality (two of the four buckets).
	const u_int uSeed = ZM_GetSpeciesFamilySeed(eId);
	switch ((uSeed >> 3) % 4u)
	{
	case 2u:  return ZM_GENDER_RATIO_MALE_3_1;
	case 3u:  return ZM_GENDER_RATIO_FEMALE_3_1;
	default:  return ZM_GENDER_RATIO_EVEN;   // buckets 0 and 1
	}
}

ZM_GENDER ZM_RollGender(ZM_SPECIES_ID eId, ZM_BattleRNG& xRng)
{
	const ZM_GENDER_RATIO eRatio = ZM_GetSpeciesGenderRatio(eId);

	// Fixed ratios resolve with NO draw, so a fixed-gender species never perturbs the
	// pinned RNG stream.
	switch (eRatio)
	{
	case ZM_GENDER_RATIO_GENDERLESS:  return ZM_GENDER_GENDERLESS;
	case ZM_GENDER_RATIO_MALE_ONLY:   return ZM_GENDER_MALE;
	case ZM_GENDER_RATIO_FEMALE_ONLY: return ZM_GENDER_FEMALE;
	default: break;
	}

	// Graded ratio: one RandBelow(8) vs the female threshold.
	const u_int uFemaleThreshold = ZM_GenderRatioFemaleThresholdOutOf8(eRatio);
	const u_int uDraw            = xRng.RandBelow(uZM_GENDER_ROLL_DENOM);
	return (uDraw < uFemaleThreshold) ? ZM_GENDER_FEMALE : ZM_GENDER_MALE;
}

namespace
{
	// Per-archetype base-stat profile (a stage-1 baseline, each row sums ~300):
	// HP, ATK, DEF, SPATK, SPDEF, SPEED. Encodes each body plan's identity --
	// AVIAN/INSECTOID fast+frail, BLOB bulky+slow, BIPED bruiser, FLOATER special.
	const u_int s_aauArchetypeProfile[ZM_ARCHETYPE_COUNT][ZM_STAT_COUNT] =
	{
		/* QUADRUPED        */ { 55, 55, 50, 45, 45, 50 },
		/* BIPED            */ { 55, 65, 60, 40, 45, 35 },
		/* AVIAN            */ { 45, 50, 40, 45, 40, 80 },
		/* SERPENT          */ { 60, 65, 50, 50, 45, 30 },
		/* AQUATIC          */ { 60, 45, 50, 55, 55, 35 },
		/* INSECTOID        */ { 45, 55, 40, 45, 40, 75 },
		/* BLOB             */ { 70, 45, 70, 40, 55, 20 },
		/* FLOATER_PLANTOID */ { 50, 35, 45, 65, 60, 45 },
	};

	// Growth from evolution stage. A single-stage species (base that is also its
	// own final) reads as fully-evolved, so standalone rares/legendaries are
	// strong rather than stage-1 weak.
	float StageFactor(const ZM_SpeciesData& x)
	{
		const bool bSingleStage = (x.m_uEvoStage == 1 && x.m_eEvolvesTo == ZM_SPECIES_NONE);
		if (bSingleStage)
		{
			return (x.m_eRarity == ZM_RARITY_LEGENDARY) ? 1.85f : 1.70f;
		}
		switch (x.m_uEvoStage)
		{
		case 1:  return 1.00f;
		case 2:  return 1.35f;
		default: return 1.70f;
		}
	}

	float RarityFactor(ZM_RARITY eRarity)
	{
		switch (eRarity)
		{
		case ZM_RARITY_COMMON:    return 0.92f;
		case ZM_RARITY_RARE:      return 1.10f;
		case ZM_RARITY_LEGENDARY: return 1.10f;   // legendaries also get the single-stage StageFactor
		default:                  return 1.00f;   // UNCOMMON
		}
	}
}

ZM_BaseStats ZM_GetSpeciesBaseStats(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);
	const float fScale = StageFactor(xData) * RarityFactor(xData.m_eRarity);

	// Per-family emphasis: bump one stat, dock another, chosen deterministically
	// from the family seed and shared by all stages of the family. Because these
	// multipliers are constant within a family, every stat still scales purely
	// with the stage factor -- so no stat ever shrinks on evolution.
	const u_int uSeed = ZM_GetSpeciesFamilySeed(eId);
	const u_int uEmphasis = uSeed % ZM_STAT_COUNT;
	u_int uDock = (uSeed / ZM_STAT_COUNT) % ZM_STAT_COUNT;
	if (uDock == uEmphasis) { uDock = (uDock + 1) % ZM_STAT_COUNT; }

	ZM_BaseStats xOut = {};
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		float fFamily = 1.0f;
		if (i == uEmphasis)  { fFamily = 1.15f; }
		else if (i == uDock) { fFamily = 0.85f; }

		int iVal = (int)((float)s_aauArchetypeProfile[xData.m_eArchetype][i] * fScale * fFamily + 0.5f);
		if (iVal < 1)   { iVal = 1; }
		if (iVal > 255) { iVal = 255; }
		xOut.m_au[i] = (u_int)iVal;
	}
	return xOut;
}

u_int ZM_GetSpeciesBaseStatTotal(ZM_SPECIES_ID eId)
{
	const ZM_BaseStats xStats = ZM_GetSpeciesBaseStats(eId);
	u_int uSum = 0;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { uSum += xStats.m_au[i]; }
	return uSum;
}
