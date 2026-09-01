#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"

// ============================================================================
// ZM_PropData -- the 28-model prop roster table (structural roster). Rows are in
// ZM_PROP_ID order; s_axProps[i].m_eId == i is asserted by the tests. Dimensions
// are metres of the box composition (small fixtures modest, bridges + dressing
// sets larger); the fields drive the SC4 static box mesh + the placeholder albedo.
//
// The 6 DRESSING rows each carry a real ZM_PROP_BIOME (one per battle-dome biome);
// every non-DRESSING row is ZM_PROP_BIOME_NONE.
//
// ★ THE 3 ITEM ROWS AT THE BOTTOM ARE IN TRANSFORM UNITS, NOT METRES -- see the
// dimensions note in ZM_PropData.h. They are worn by a ground-item prop entity
// whose scale is already fZM_ROUTE1_PROP_CUBE_EDGE (0.6 m), so 1.0 here is 0.6 m on
// screen and every one of them is comfortably under 1.0 across -- with enough
// headroom that the mesh jitter cannot push one past 1.0 either. Nothing grows past
// the blockout cube these replace, which is what keeps a prop out of the walked
// lane; the headroom star on the ITEM block below is where that is argued and says
// what checks it.
// ============================================================================

namespace
{
	const ZM_PropData s_axProps[ZM_PROP_COUNT] =
	{
		{ ZM_PROP_FENCE_WOOD,   "FenceWood",   ZM_PROP_KIND_FENCE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    2.0f, 0.12f, 1.0f },
		{ ZM_PROP_FENCE_STONE,  "FenceStone",  ZM_PROP_KIND_FENCE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.0f, 0.22f, 0.9f },
		{ ZM_PROP_SIGN_POST,    "SignPost",    ZM_PROP_KIND_SIGN,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    0.9f, 0.12f, 2.0f },
		{ ZM_PROP_TOWN_BOARD,   "TownBoard",   ZM_PROP_KIND_SIGN,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_PAINTED, 1.6f, 0.15f, 2.2f },
		{ ZM_PROP_LAMP_POST,    "LampPost",    ZM_PROP_KIND_LAMP,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_METAL,   0.4f, 0.4f,  3.0f },
		{ ZM_PROP_LANTERN_POST, "LanternPost", ZM_PROP_KIND_LAMP,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_METAL,   0.4f, 0.4f,  2.4f },
		{ ZM_PROP_BRIDGE_PLANK, "BridgePlank", ZM_PROP_KIND_BRIDGE,    ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    3.0f, 4.0f,  1.0f },
		{ ZM_PROP_BRIDGE_STONE, "BridgeStone", ZM_PROP_KIND_BRIDGE,    ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   3.4f, 5.0f,  1.2f },
		{ ZM_PROP_LEDGE_LOW,    "LedgeLow",    ZM_PROP_KIND_LEDGE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.0f, 1.0f,  0.6f },
		{ ZM_PROP_LEDGE_HIGH,   "LedgeHigh",   ZM_PROP_KIND_LEDGE,     ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.4f, 1.2f,  1.0f },
		{ ZM_PROP_ROCK_SMALL,   "RockSmall",   ZM_PROP_KIND_ROCK,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   0.8f, 0.8f,  0.7f },
		{ ZM_PROP_ROCK_LARGE,   "RockLarge",   ZM_PROP_KIND_ROCK,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   1.6f, 1.5f,  1.4f },
		{ ZM_PROP_BOULDER,      "Boulder",     ZM_PROP_KIND_ROCK,      ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_STONE,   2.4f, 2.2f,  2.0f },
		{ ZM_PROP_TABLE,        "Table",       ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    1.4f, 0.9f,  0.9f },
		{ ZM_PROP_CHAIR,        "Chair",       ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    0.6f, 0.6f,  1.0f },
		{ ZM_PROP_BED,          "Bed",         ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_PAINTED, 2.0f, 1.2f,  0.7f },
		{ ZM_PROP_SHELF,        "Shelf",       ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    1.2f, 0.4f,  2.0f },
		{ ZM_PROP_COUNTER,      "Counter",     ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    2.2f, 0.7f,  1.0f },
		{ ZM_PROP_BARREL,       "Barrel",      ZM_PROP_KIND_FURNITURE, ZM_PROP_BIOME_NONE,     ZM_PROP_PALETTE_WOOD,    0.7f, 0.7f,  1.0f },

		// --- 6 battle-dome biome dressing sets ---
		{ ZM_PROP_DRESSING_MEADOW,   "DressingMeadow",   ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_MEADOW,   ZM_PROP_PALETTE_FOLIAGE, 3.0f, 3.0f, 1.6f },
		{ ZM_PROP_DRESSING_VOLCANIC, "DressingVolcanic", ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_VOLCANIC, ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 1.8f },
		{ ZM_PROP_DRESSING_COAST,    "DressingCoast",    ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_COAST,    ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 1.4f },
		{ ZM_PROP_DRESSING_WETLAND,  "DressingWetland",  ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_WETLAND,  ZM_PROP_PALETTE_FOLIAGE, 3.0f, 3.0f, 1.5f },
		{ ZM_PROP_DRESSING_SNOW,     "DressingSnow",     ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_SNOW,     ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 1.6f },
		{ ZM_PROP_DRESSING_CANYON,   "DressingCanyon",   ZM_PROP_KIND_DRESSING, ZM_PROP_BIOME_CANYON,   ZM_PROP_PALETTE_STONE,   3.0f, 3.0f, 2.0f },

		// --- 3 ground-item pickup presentations (ZM-67) --- TRANSFORM UNITS, NOT METRES ---
		//
		// ★ THE TWO LIVE ROWS ARE PULLED APART ON **BOTH** AXES A PLAYER CAN READ AT
		// SEVEN METRES, and neither separation is a number invented here:
		//   SILHOUETTE -- 0.42 wide x 0.94 tall against 0.88 wide x 0.64 tall. The
		//     aspect ratios are 2.2 and 0.73, so one is a tall thin thing on a pad and
		//     the other a squat wide one; that survives being 7% of frame height, which
		//     a colour-only difference under a re-tuned sky might not.
		//   HUE -- FOLIAGE (green) against PAINTED (red), two of the five palettes
		//     ZM_PropPaletteColour has carried since S4. The CONSTANTS predate this
		//     ticket, so no test below is asserting a distance chosen to make it pass.
		//     ★ AND THE REASON IS NARROWER THAN "THEY ARE THE FURTHEST APART", WHICH IS
		//     FALSE. Read off ZM_PropGen.cpp:113-117, the five HSV hues are WOOD 27
		//     degrees, PAINTED 3, STONE 48, FOLIAGE 109, METAL 223 -- so the widest hue
		//     gaps in the set are STONE-METAL (175) and WOOD-METAL (164), and
		//     PAINTED-FOLIAGE is 106. What IS true is the claim that decides this pick:
		//     of the three SATURATED palettes (HSV saturation WOOD 0.64, PAINTED 0.63,
		//     FOLIAGE 0.47, against STONE 0.09 and METAL 0.13, which are the greys these
		//     props have to stand out FROM rather than candidates to be), PAINTED and
		//     FOLIAGE are the furthest apart -- 106 degrees, against 82 for
		//     WOOD-FOLIAGE and 24 for WOOD-PAINTED. WOOD is the palette that loses here,
		//     and losing by 24 degrees from PAINTED is the point: the two would read as
		//     one orange-red family at seven metres.
		//   ★ BE HONEST ABOUT THE SECOND HALF TOO. "The two furthest from the
		//     terrain/blockout greys" was ALSO wrong. Measuring chroma as the distance
		//     from the achromatic axis, the five rank PAINTED 0.376, WOOD 0.248,
		//     FOLIAGE 0.158, METAL 0.051, STONE 0.037: PAINTED is the most chromatic of
		//     the five and FOLIAGE is only THIRD, behind WOOD. The load-bearing part
		//     survives -- both live rows are 3x to 10x further off the grey axis than
		//     either near-grey palette, so neither can be mistaken for terrain or for
		//     the SPENT row -- but FOLIAGE is here for its HUE distance from PAINTED,
		//     not for being especially unlike a grey.
		//
		// The SPENT row is deliberately the least eye-catching thing in the set: STONE,
		// the same desaturated grey as the rocks, and 0.22 tall against 0.94 and 0.64 --
		// roughly a quarter the height of the phial and a third that of the orb.
		//
		// ★ NO DIMENSION IS 1.0, AND THAT HEADROOM IS LOAD-BEARING. ZM_BuildPropMesh
		// jitters every axis before it composes, so a 1.0 row would build a mesh past
		// the volume the authored transform scales -- i.e. past the blockout cube these
		// replace. The lane-reach budget ZM-D-207 froze has no slack to lend, so every
		// row is sized to stay inside [-0.5, +0.5] on every axis across the WHOLE
		// jitter band and not merely at the one draw its own name hash lands on.
		//
		// ★ WHAT CHECKS IT, AND WHAT THAT CHECK CAN ACTUALLY SEE. Each row draws ONE
		// jitter triple, from its own MESH-domain PCG, so building a row and measuring
		// it samples one point of that band and no more.
		// PropGen_GroundItemPickupRowsAreCentreAnchoredAndDistinct therefore STEERS the
		// generator: it sweeps the MESH-domain seed, asserts the unit-volume bound on
		// the widest draw it finds, and then applies that widest observed inflation to
		// EVERY row -- so a row that survives only because its own seed drew small reds
		// there. It does not re-multiply these numbers by the generator's jitter
		// constant, which would put a second copy of that constant in a test file and
		// leave it asserting the old band the day the band moves.
		{ ZM_PROP_ITEM_PHIAL,   "ItemPhial",   ZM_PROP_KIND_ITEM_PICKUP, ZM_PROP_BIOME_NONE, ZM_PROP_PALETTE_FOLIAGE, 0.42f, 0.42f, 0.94f },
		{ ZM_PROP_ITEM_ORB,     "ItemOrb",     ZM_PROP_KIND_ITEM_PICKUP, ZM_PROP_BIOME_NONE, ZM_PROP_PALETTE_PAINTED, 0.88f, 0.88f, 0.64f },
		{ ZM_PROP_ITEM_TAKEN,   "ItemTaken",   ZM_PROP_KIND_ITEM_SPENT,  ZM_PROP_BIOME_NONE, ZM_PROP_PALETTE_STONE,   0.92f, 0.92f, 0.22f },
	};
}

namespace
{
	// ★ ONE ROW PER LIGHT-EMITTING PROP; everything else answers the inert row
	// below. See ZM_PropBulb in the header for why this is keyed by PROP and why
	// the offset is in the model's own units.
	//
	// ★★ THE LAMP POST'S NUMBER IS MEASURED, NOT CHOSEN. AB-PROP-07's lantern
	// head is the flare above a shaft of radius ~0.017: the profile widens to
	// 0.0898 at y +0.399..+0.419 with a finial above. The bulb is the
	// AREA-WEIGHTED centroid of the glass between the bracket collar and the roof
	// brim (y +0.300..+0.440) -- y = +0.3771, which is 87.8% of the model's height
	// and 2.63 m up once the fit scales the post to its roster's 3.0 m.
	//
	// X and Z are ZERO rather than the centroid's own (+0.0025, +0.0037). Those
	// are mesh asymmetry of a quarter of a centimetre, and a bulb belongs on the
	// post's axis: adopting them would be reading noise as intent.
	//
	// ★★★ 60 lm, VIA 1200, 500 AND 175, AND THE MEASUREMENTS ARE THE USEFUL PART.
	// 1200 is the honest photometric figure for a street lamp; the rest is what
	// the current bloom calibration will actually carry.
	//
	// ★ NONE OF THEM WAS "BLOWN OUT" IN THE USUAL SENSE. At 1200 lm, 0.00% of the
	// frame sat above luminance 250 and the peak stayed under it -- the tonemapper
	// was holding throughout. The problem is that Flux extracts bloom above a
	// PRE-EXPOSURE HDR luminance of 3.0 (Flux_HDR.cpp), an ABSOLUTE scene-referred
	// figure the auto-exposure cannot rescue: a light either pushes its
	// surroundings past it or it does not.
	//
	// ★★ AND THE LEVER IS SUB-LINEAR, WHICH IS WHY THIS TOOK FOUR VALUES AND NOT
	// ONE. The halo is a blurred source, so its RADIUS grows far slower than the
	// intensity -- each ~3x cut bought only ~15 px of diameter. MEASURED, as the
	// largest connected near-white blob in the subject's own capture:
	//
	//     lumens   detail   three-quarter   town-wide
	//       1200   96.7 px       66.2 px      39.5 px
	//        500   ~90            ~58          ~33
	//        175   79.9 px       49.4 px      26.4 px
	//         60   63.3 px       35.0 px      17.6 px
	//
	// A first guess of "reduce it a bit" (1200 -> 500) was therefore worth almost
	// nothing, and only the table shows that.
	//
	// ★ WHERE TO STOP CAME FROM THE SCENE, NOT FROM TASTE. ProfLab's ceiling tubes
	// measured a 26 px halo before any of this and read as a lamp that glows
	// slightly, so "slight" already had a value in this game. At 60 lm the post is
	// inside it at every distance a player actually stands, and the lantern head --
	// the part AB-PROP-07's commission describes -- reads as a shape again.
	//
	// ★★ THE REMAINING HALO IS NOT REMOVABLE BY THIS NUMBER. The bulb is INSIDE
	// its own lantern: the glass sits 0.1-0.27 m from a point source, and
	// ComputePointAttenuation's 1/(4*pi*d^2) (clamped only at MIN_LIGHT_DISTANCE =
	// 1 cm) puts those surfaces ~350x the source intensity whatever it is. Driving
	// the head itself under the threshold would need single-digit MILLILUMENS. A
	// real fix is an emissive lantern material plus a dim spill light, or a light
	// with a source RADIUS so the near field stops exploding -- neither exists yet,
	// and both are engine work rather than a number in this table.
	//
	// The colour is the warm sodium cast a town lamp has, in LINEAR RGB like every
	// other light in this game.
	const ZM_PropBulb s_xLampPostBulb =
	{
		true, 0.0f, 0.3771f, 0.0f, 60.0f, 14.0f, 1.00f, 0.86f, 0.62f
	};

	const ZM_PropBulb s_xNoBulb =
	{
		false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
	};
}

// TOTAL for every id, in range or not: only the rows that emit answer a bulb.
const ZM_PropBulb& ZM_GetPropBulb(ZM_PROP_ID eId)
{
	switch (eId)
	{
	case ZM_PROP_LAMP_POST:
		return s_xLampPostBulb;
	default:
		// ★ THE LANTERN POST (ZM_PROP_LANTERN_POST) IS DELIBERATELY NOT HERE. It
		// is a different asset with a different head, still generated, and its
		// bulb has never been measured -- inheriting the lamp post's offset would
		// put a light at a point on a mesh nobody has looked at. Add a row when
		// that asset is imported and measured, not before.
		return s_xNoBulb;
	}
}

const ZM_PropData& ZM_GetPropData(ZM_PROP_ID eId)
{
	Zenith_Assert(eId < ZM_PROP_COUNT, "ZM_GetPropData: prop id out of range (%u)", (u_int)eId);
	return s_axProps[eId];
}

u_int ZM_GetPropCount()
{
	return ZM_PROP_COUNT;
}

const char* ZM_GetPropName(ZM_PROP_ID eId)
{
	if (eId >= ZM_PROP_COUNT)
	{
		return "NONE";
	}
	return s_axProps[eId].m_szName;
}
