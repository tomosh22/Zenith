#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"   // u_int etc. (mirrors ZM_SpeciesData's data-core include)

// ============================================================================
// ZM_HumanData -- the human-cast roster table (S4 ZM_HumanGen data core).
//
// This is the STRUCTURAL roster of the ~35 human models the game ships: the
// named story cast (player m/f, professor, mom, rival, the 8 gym leaders, the
// Elite Four + Champion -- all from Docs/GameDesignDocument.md section 3) plus
// generic trainer classes and townsfolk types. Every human binds the ONE shared
// 16-bone skeleton and reuses the ONE shared 9-clip .zanim set (AssetManifest
// section 2); per-model variation lives ONLY in the mesh loft + texture, driven
// by the VARIETY AXES on each row (build / skin / hair / outfit / attachment) --
// NEVER in the skeleton.
//
// This file MIRRORS ZM_SpeciesData exactly: a save-stable APPEND-ONLY enum
// (ZM_HUMAN_ID), a compiled `const ZM_HumanData` C array (zero file I/O in
// headless tests, DecisionLog ZM-D-009 precedent), and the
// ZM_GetHumanData/ZM_GetHumanName accessor idiom. The ZM_HUMAN_ID order is
// save-stable once content ships -- APPEND before ZM_HUMAN_COUNT, never reorder.
// ============================================================================

// Body build preset -- drives the mesh loft's ring extents + a modest height
// factor (NOT the skeleton, which is shared/fixed). APPEND-only + save-stable;
// ZM_HUMAN_BUILD_COUNT is the sentinel, never a stored value.
enum ZM_HUMAN_BUILD : u_int
{
	ZM_HUMAN_BUILD_SLIGHT,    // narrow + slightly shorter
	ZM_HUMAN_BUILD_AVERAGE,   // the 1.0 reference build
	ZM_HUMAN_BUILD_STOCKY,    // broad + slightly shorter
	ZM_HUMAN_BUILD_TALL,      // reference width + taller

	ZM_HUMAN_BUILD_COUNT
};

// Skin-tone palette slot -- drives the albedo base colour (SC3 texture; SC1 uses
// it for the trivial solid placeholder). APPEND-only + save-stable.
enum ZM_HUMAN_SKIN_TONE : u_int
{
	ZM_HUMAN_SKIN_PALE,
	ZM_HUMAN_SKIN_FAIR,
	ZM_HUMAN_SKIN_TAN,
	ZM_HUMAN_SKIN_BROWN,
	ZM_HUMAN_SKIN_DARK,

	ZM_HUMAN_SKIN_COUNT
};

// Hair colour slot -- pairs with m_uHairStyle to drive the (SC3) hair texture +
// attachment tint. DYED is the vivid, type-themed colour some gym leaders sport.
// APPEND-only + save-stable.
enum ZM_HUMAN_HAIR_COLOUR : u_int
{
	ZM_HUMAN_HAIR_BLACK,
	ZM_HUMAN_HAIR_BROWN,
	ZM_HUMAN_HAIR_BLONDE,
	ZM_HUMAN_HAIR_AUBURN,
	ZM_HUMAN_HAIR_GREY,
	ZM_HUMAN_HAIR_WHITE,
	ZM_HUMAN_HAIR_DYED,       // vivid dyed (type-themed leaders)

	ZM_HUMAN_HAIR_COUNT
};

// Outfit / role -- selects an outfit recolour and (later) which attachment mesh
// the model wears. APPEND-only + save-stable.
enum ZM_HUMAN_OUTFIT : u_int
{
	ZM_HUMAN_OUTFIT_TRAVELER,   // player + most trainer classes
	ZM_HUMAN_OUTFIT_LABCOAT,    // professor
	ZM_HUMAN_OUTFIT_CASUAL,     // mom, townsfolk
	ZM_HUMAN_OUTFIT_LEADER,     // gym leaders (type-themed recolour)
	ZM_HUMAN_OUTFIT_FORMAL,     // Elite Four + Champion
	ZM_HUMAN_OUTFIT_WORKER,     // dockworker, fieldhand, ranger
	ZM_HUMAN_OUTFIT_UNIFORM,    // caretaker, shopkeep

	ZM_HUMAN_OUTFIT_COUNT
};

// Optional head/back attachment slot -- the small extra mesh a model wears (SC2
// authors the geometry; SC1 only records the choice). APPEND-only + save-stable.
enum ZM_HUMAN_ATTACHMENT : u_int
{
	ZM_HUMAN_ATTACHMENT_NONE,
	ZM_HUMAN_ATTACHMENT_CAP,       // ball cap (player)
	ZM_HUMAN_ATTACHMENT_HAT,       // wide-brim hat
	ZM_HUMAN_ATTACHMENT_BACKPACK,
	ZM_HUMAN_ATTACHMENT_GLASSES,
	ZM_HUMAN_ATTACHMENT_SATCHEL,

	ZM_HUMAN_ATTACHMENT_COUNT
};

// Every human model, in roster order. IDs are contiguous 0..ZM_HUMAN_COUNT-1 and
// save-stable -- APPEND before ZM_HUMAN_COUNT, never reorder. Names in comments
// marked (GDD) are canonical from GameDesignDocument.md section 3; the generic
// trainer/townsfolk categories are original (zero third-party IP).
enum ZM_HUMAN_ID : u_int
{
	// --- Player avatars (two distinct meshes; unnamed in the GDD) ---
	ZM_HUMAN_PLAYER_M,          // "PlayerM"
	ZM_HUMAN_PLAYER_F,          // "PlayerF"

	// --- Named story cast (GDD 3.1-3.3) ---
	ZM_HUMAN_PROF_ASTER,        // "Aster"    Professor Aster (GDD 3.1)
	ZM_HUMAN_MOM_MAREN,         // "Maren"    Mom (GDD 3.2)
	ZM_HUMAN_RIVAL_VESPER,      // "Vesper"   Rival (GDD 3.3)

	// --- 8 gym leaders (GDD 3.4) ---
	ZM_HUMAN_LEADER_FENNA,      // "Fenna"    Grass    (Thornacre)
	ZM_HUMAN_LEADER_BRAM,       // "Bram"     Fire     (Cinderfell)
	ZM_HUMAN_LEADER_MARIS,      // "Maris"    Water    (Tidegate)
	ZM_HUMAN_LEADER_TESSA,      // "Tessa"    Electric (Gearspring)
	ZM_HUMAN_LEADER_AQUILO,     // "Aquilo"   Sky      (Skyshear)
	ZM_HUMAN_LEADER_MORWENNA,   // "Morwenna" Phantom  (Umbermoor)
	ZM_HUMAN_LEADER_HALVARD,    // "Halvard"  Ice      (Frostvale)
	ZM_HUMAN_LEADER_VARDIS,     // "Vardis"   Drake    (Stonereach)

	// --- Elite Four + Champion (GDD 3.5) ---
	ZM_HUMAN_ELITE_CASSIA,      // "Cassia"   Venom
	ZM_HUMAN_ELITE_TORBEN,      // "Torben"   Brawl
	ZM_HUMAN_ELITE_LUMEN,       // "Lumen"    Mind
	ZM_HUMAN_ELITE_SABLE,       // "Sable"    Umbral
	ZM_HUMAN_CHAMPION_ELARA,    // "Elara"    Champion

	// --- Generic trainer classes (original category names) ---
	ZM_HUMAN_TRAINER_RAMBLER,       // "Rambler"     route hiker
	ZM_HUMAN_TRAINER_ANGLER,        // "Angler"      shoreline fisher
	ZM_HUMAN_TRAINER_NETTER,        // "Netter"      bug catcher
	ZM_HUMAN_TRAINER_RIDGEWALKER,   // "Ridgewalker" mountain hiker
	ZM_HUMAN_TRAINER_ACE,           // "Ace"         ace trainer
	ZM_HUMAN_TRAINER_SCOUT,         // "Scout"       young scout
	ZM_HUMAN_TRAINER_RANGER,        // "Ranger"      wilderness ranger
	ZM_HUMAN_TRAINER_DUELIST,       // "Duelist"     battle specialist
	ZM_HUMAN_TRAINER_WAYFARER,      // "Wayfarer"    long-road traveller
	ZM_HUMAN_TRAINER_CAMPER,        // "Camper"      route camper

	// --- Townsfolk types (original category names) ---
	ZM_HUMAN_TOWN_VILLAGER,     // "Villager"
	ZM_HUMAN_TOWN_CARETAKER,    // "Caretaker"  Care Center staff (GDD 1)
	ZM_HUMAN_TOWN_SHOPKEEP,     // "Shopkeep"
	ZM_HUMAN_TOWN_ELDER,        // "Elder"
	ZM_HUMAN_TOWN_FIELDHAND,    // "Fieldhand"  Thornacre farmer
	ZM_HUMAN_TOWN_DOCKWORKER,   // "Dockworker" Tidegate harbour

	ZM_HUMAN_COUNT,
	ZM_HUMAN_NONE = ZM_HUMAN_COUNT   // "no human" sentinel
};

// One roster row. m_szName is the asset STEM (the folder + file basename under
// game:Humans/<Name>/...). The five variety axes drive the mesh loft + texture,
// never the shared skeleton.
struct ZM_HumanData
{
	ZM_HUMAN_ID          m_eId;
	const char*          m_szName;        // asset stem, e.g. "PlayerM"
	ZM_HUMAN_BUILD       m_eBuild;
	ZM_HUMAN_SKIN_TONE   m_eSkinTone;
	u_int                m_uHairStyle;    // hair-style index (SC3 selects the mesh/texture)
	ZM_HUMAN_HAIR_COLOUR m_eHairColour;
	ZM_HUMAN_OUTFIT      m_eOutfit;
	ZM_HUMAN_ATTACHMENT  m_eAttachment;
};

// Table accessors (bounds-asserted). ZM_GetHumanData indexes by ZM_HUMAN_ID.
const ZM_HumanData&	ZM_GetHumanData(ZM_HUMAN_ID eId);
u_int				ZM_GetHumanCount();				// == ZM_HUMAN_COUNT
const char*			ZM_GetHumanName(ZM_HUMAN_ID eId);
