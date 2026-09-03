#pragma once

#include "Zenithmon/Source/Data/ZM_Types.h"   // ZM_TYPE (+ u_int etc.; mirrors ZM_BuildingData's data-core include)

// ============================================================================
// ZM_PropData -- the prop roster table (S4 ZM_PropGen data core).
//
// Props are STATIC models (NO skeleton, NO animation), like buildings and unlike
// the skinned + animated creatures/humans. This is the STRUCTURAL roster of the
// ~25 dressing/set-piece models the town + battle-dome need: fences, signs,
// lamps, bridges, ledges, rocks, furniture, and the 6 per-biome battle-dome
// dressing sets. Per-model variation lives in the box composition (SC-local) +
// the placeholder albedo driven by each row's fields.
//
// This file MIRRORS ZM_BuildingData exactly: a save-stable APPEND-ONLY enum
// (ZM_PROP_ID), a compiled `const ZM_PropData` C array (zero file I/O in headless
// tests, DecisionLog ZM-D-009 precedent), and the ZM_GetPropData/ZM_GetPropCount/
// ZM_GetPropName accessor idiom. The ZM_PROP_ID order is save-stable once content
// ships -- APPEND before ZM_PROP_COUNT, never reorder.
// ============================================================================

// Prop silhouette family -- drives the (SC4) box composition. APPEND-only +
// save-stable; ZM_PROP_KIND_COUNT is the sentinel, never a stored value.
//
// ★ THE LAST TWO ARE THE ONLY **CENTRE-ANCHORED** KINDS, and that is a property of
// where they STAND, not a style choice. Every kind above them is grounded at y = 0
// because it is authored at the surface. The two ITEM kinds present a GROUND-ITEM
// PROP, whose authored transform is "measured surface + half the cube edge" with
// that same edge as its uniform scale (ZM_Route1PropCentreY /
// fZM_ROUTE1_PROP_CUBE_EDGE), so their local origin IS the centre of a unit volume
// and local y = -0.5 IS the ground. See fZM_PROP_ITEM_BASE_Y in ZM_PropGen.h --
// the one place that offset is spelled.
enum ZM_PROP_KIND : u_int
{
	ZM_PROP_KIND_FENCE,
	ZM_PROP_KIND_SIGN,
	ZM_PROP_KIND_LAMP,
	ZM_PROP_KIND_BRIDGE,
	ZM_PROP_KIND_LEDGE,
	ZM_PROP_KIND_ROCK,
	ZM_PROP_KIND_FURNITURE,
	ZM_PROP_KIND_DRESSING,
	ZM_PROP_KIND_ITEM_PICKUP,   // a takeable item standing on a small plinth
	ZM_PROP_KIND_ITEM_SPENT,    // the same plinth with the item GONE -- an empty tray

	// ★ A LIGHT FIXTURE: the THING a Zenith_LightComponent is supposed to be
	// coming out of. Every interior light used to be a bare point in space, so a
	// room had lamplight and no lamp. A fixture is a body (metal, timber, ceramic)
	// plus a SHADE or DIFFUSER that carries an EMISSIVE material matched to the
	// light it houses -- the glow is on the shade, never on the body, which is why
	// the fixture kinds are the one family that PAINTS an emissive map (see
	// ZM_PropUVIsland: the shade faces map into the GLOW island, and only that
	// island is bright in ZM_BuildPropEmissive).
	//
	// Grounded at y = 0 like every scenery kind. A pendant is authored at the
	// CEILING by its placement row's own Y (ZM_InteriorDressing.h's fixture table
	// carries one), not by a special anchor here: the mesh is still "base at 0,
	// top at roster height", and the room decides where that base sits.
	ZM_PROP_KIND_LIGHT_FIXTURE,

	ZM_PROP_KIND_COUNT
};

// Material/colour family -- drives the (SC4 solid) albedo base colour. APPEND-only
// + save-stable.
enum ZM_PROP_PALETTE : u_int
{
	ZM_PROP_PALETTE_WOOD,
	ZM_PROP_PALETTE_STONE,
	ZM_PROP_PALETTE_METAL,
	ZM_PROP_PALETTE_PAINTED,
	ZM_PROP_PALETTE_FOLIAGE,

	ZM_PROP_PALETTE_COUNT
};

// Battle-dome biome tag -- ZM_PROP_BIOME_NONE for the generic town props; the 6
// dressing sets each carry a real biome. APPEND-only + save-stable.
enum ZM_PROP_BIOME : u_int
{
	ZM_PROP_BIOME_NONE,
	ZM_PROP_BIOME_MEADOW,
	ZM_PROP_BIOME_VOLCANIC,
	ZM_PROP_BIOME_COAST,
	ZM_PROP_BIOME_WETLAND,
	ZM_PROP_BIOME_SNOW,
	ZM_PROP_BIOME_CANYON,

	ZM_PROP_BIOME_COUNT
};

// The first REAL (non-NONE) biome -- the biome-coverage gate iterates
// [uZM_PROP_BIOME_FIRST_REAL, ZM_PROP_BIOME_COUNT).
constexpr u_int uZM_PROP_BIOME_FIRST_REAL = ZM_PROP_BIOME_MEADOW;

// Every prop model, in roster order. IDs are contiguous 0..ZM_PROP_COUNT-1 and
// save-stable -- APPEND before ZM_PROP_COUNT, never reorder.
enum ZM_PROP_ID : u_int
{
	ZM_PROP_FENCE_WOOD, ZM_PROP_FENCE_STONE,
	ZM_PROP_SIGN_POST, ZM_PROP_TOWN_BOARD,
	ZM_PROP_LAMP_POST, ZM_PROP_LANTERN_POST,
	ZM_PROP_BRIDGE_PLANK, ZM_PROP_BRIDGE_STONE,
	ZM_PROP_LEDGE_LOW, ZM_PROP_LEDGE_HIGH,
	ZM_PROP_ROCK_SMALL, ZM_PROP_ROCK_LARGE, ZM_PROP_BOULDER,
	ZM_PROP_TABLE, ZM_PROP_CHAIR, ZM_PROP_BED, ZM_PROP_SHELF, ZM_PROP_COUNTER, ZM_PROP_BARREL,

	// 6 battle-dome biome dressing sets -> 25 total
	ZM_PROP_DRESSING_MEADOW, ZM_PROP_DRESSING_VOLCANIC, ZM_PROP_DRESSING_COAST,
	ZM_PROP_DRESSING_WETLAND, ZM_PROP_DRESSING_SNOW, ZM_PROP_DRESSING_CANYON,

	// 3 ground-item pickup presentations (ZM-67) -> 28 total. NOT dressing and NOT
	// scenery: these are what a ZM_GroundItemProp entity WEARS, chosen per frame by
	// ZM_GroundItemPropModel from the item the prop yields and whether this save has
	// already taken it. One model per SILHOUETTE, never one per prop, so a fourth
	// ground item costs a registry row and no art.
	ZM_PROP_ITEM_PHIAL, ZM_PROP_ITEM_ORB, ZM_PROP_ITEM_TAKEN,

	// 4 interior light fixtures -> 32 total. One per DISTINCT light in the two
	// rooms (ZM_InteriorDressing.h): a ceiling pendant, a nightstand lamp, a
	// standard (floor) lamp, and the lab's fluorescent batten. They are the
	// visible half of a light that was, until now, a bare point in space.
	ZM_PROP_PENDANT_LAMP, ZM_PROP_BEDSIDE_LAMP, ZM_PROP_FLOOR_LAMP, ZM_PROP_LAB_BATTEN,

	ZM_PROP_COUNT,
	ZM_PROP_NONE = ZM_PROP_COUNT   // "no prop" sentinel
};

// One roster row. m_szName is the asset STEM (the folder + file basename under
// game:Props/<Name>/...). The kind/biome/palette/dims fields drive the SC4 box
// composition + the placeholder albedo. m_eBiome is ZM_PROP_BIOME_NONE except for
// the DRESSING rows, whose biome tags the battle-dome set they belong to.
//
// ★ THE DIMENSIONS ARE METRES **EXCEPT** ON THE TWO ITEM KINDS, where they are
// units of the authored entity scale. A scenery prop is dropped onto an identity
// transform, so its row is metres and reads as metres. A ground-item prop is worn
// by an entity whose scale is already fZM_ROUTE1_PROP_CUBE_EDGE and CANNOT be
// re-authored (ZM-D-207 froze the anchors; moving one would need a windowed
// re-author of a committed scene), so an ITEM row states the fraction of that
// volume it fills. Multiply by the entity's scale to get metres.
struct ZM_PropData
{
	ZM_PROP_ID      m_eId;
	const char*     m_szName;    // asset stem, PascalCase, e.g. "LampPost" / "DressingMeadow"
	ZM_PROP_KIND    m_eKind;
	ZM_PROP_BIOME   m_eBiome;
	ZM_PROP_PALETTE m_ePalette;
	float           m_fWidth;
	float           m_fDepth;
	float           m_fHeight;
};

// ---- The four yaws a prop placement may take --------------------------------
//
// ★ HERE RATHER THAN IN A PLACEMENT TABLE, because there are TWO placement tables
// now -- ZM_InteriorDressing.h for the rooms and ZM_DawnmereDressing.h for
// outdoors -- and both need the same four values. A frozen constant copied into a
// second table is how two tables start disagreeing.
//
// ★★ THESE NAMED HALF THE ANGLE THEY APPLIED, AND EVERY ONE OF THEM WAS WRONG.
// The block read:
//
//     cos(0)=1, sin(0)=0 | cos(45°)=sin(45°)=0.70710678 | cos(90°)=0, sin(90°)=1
//     fZM_INTERIOR_YAW90_W = 0.0f;  fZM_INTERIOR_YAW90_Y = 1.0f;
//     fZM_INTERIOR_YAW45_W = 0.70710678f;  fZM_INTERIOR_YAW45_Y = 0.70710678f;
//
// -- i.e. (w, y) = (cos a, sin a). A quaternion is (cos(a/2), axis * sin(a/2)),
// so (0, 1) is a HALF TURN and (0.70710678, 0.70710678) is a QUARTER TURN. The
// old "YAW90" was 180 degrees and the old "YAW45" was 90.
//
// ★ IT HID BEHIND A SECOND DEFECT, WHICH IS WHY IT SURVIVED. Every furniture row
// below was authored with an AABB collider, and Zenith_ColliderComponent forces
// an AABB body to identity -- the physics->transform sync then wrote that
// identity back over the authored rotation, INTO THE SAVED SCENE BYTES (ZM-D-156,
// already paid for once on rival Vesper). So NO interior prop was ever rotated at
// all, and a constant that applied twice its stated angle could not be caught by
// looking at the room. Both are fixed together because neither is visible alone:
// the furniture is OBB now, and these names are the angles they apply.
//
// The half turn is exact; the quarter turn is the correctly-rounded float32
// literal and is identical in every configuration because it is a literal, not a
// call (ZM-D-183).
inline constexpr float fZM_INTERIOR_YAW0_W    = 1.0f;
inline constexpr float fZM_INTERIOR_YAW0_Y    = 0.0f;
inline constexpr float fZM_INTERIOR_YAW90_W   = 0.70710678f;
inline constexpr float fZM_INTERIOR_YAW90_Y   = 0.70710678f;
inline constexpr float fZM_INTERIOR_YAW180_W  = 0.0f;
inline constexpr float fZM_INTERIOR_YAW180_Y  = 1.0f;

// Table accessors (bounds-asserted). ZM_GetPropData indexes by ZM_PROP_ID.
const ZM_PropData&	ZM_GetPropData(ZM_PROP_ID eId);
u_int				ZM_GetPropCount();				// == ZM_PROP_COUNT
const char*			ZM_GetPropName(ZM_PROP_ID eId);

// ============================================================================
// ★★★ WHERE A PROP'S BULB IS, for the props that emit light.
//
// A lamp post is the first prop whose entity must own a LIGHT as well as a
// model, and the light has to be at the BULB -- inside the lantern head, not at
// the entity origin down at the foot of the post. Getting that wrong is not
// subtle: the lamp glows from the pavement and lights its own casing from below.
//
// ★★ IT IS KEYED BY PROP, NOT BY PLACEMENT, because that is what it is. Every
// lamp post in the world has its bulb in the same place on the model; where the
// post STANDS is the placement's business. Putting the offset on a placement row
// would copy one measurement into every row that used it, and the second copy is
// where they start disagreeing.
//
// ★★ AND IT IS IN THE MODEL'S OWN UNITS -- the same space
// Zenith_LightComponent's local position offset consumes, which is scaled and
// rotated by the entity transform before use. So the number below survives
// ZM_ComputePropFit rescaling the asset (this post fits at 3.006) and survives
// any authored yaw, which a world-space offset would not: it would have to be
// re-typed post-scale and would break on the next re-export.
//
// ★ MEASURED OFF THE DECODED MESH, never read off the roster row. The lantern
// head is the widening above the shaft: the radius profile runs ~0.017 up the
// post and flares to 0.0898 at y +0.399..+0.419, with a finial above it. The
// bulb is the AREA-WEIGHTED centroid of the glass housing between the bracket
// collar and the roof brim (y +0.300..+0.440), which lands at y +0.3771 -- 87.8%
// of the model's height, or 2.63 m up once fitted to the roster's 3.0 m post.
// Area-weighted rather than a vertex mean so a densely tessellated rim cannot
// drag the answer.
// ============================================================================
struct ZM_PropBulb
{
	bool  m_bHasBulb;
	// In the MODEL's own units, from the model ORIGIN (these models are
	// origin-centred, so a positive Y is above the middle of the post).
	float m_fX;
	float m_fY;
	float m_fZ;
	// Photometric, like every other light in this game: lumens, metres, linear RGB.
	float m_fLumens;
	float m_fRange;
	float m_fR;
	float m_fG;
	float m_fB;
};

// TOTAL: every prop that is not a light source answers m_bHasBulb == false with
// zeroes, so a caller may read the row unconditionally and branch on the flag.
const ZM_PropBulb& ZM_GetPropBulb(ZM_PROP_ID eId);
