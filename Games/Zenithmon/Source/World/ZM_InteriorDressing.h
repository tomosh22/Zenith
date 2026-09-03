#pragma once

#include "Zenithmon/Source/Data/ZM_PropData.h"          // ZM_PROP_ID
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"        // ZM_INTERIOR_ROOM + the room spec
#include "Maths/Zenith_Maths.h"

#include <cstring>   // strcmp -- the shell mapping is an EXACT name match

// ============================================================================
// ZM_InteriorDressing -- what is IN the two interior rooms: the shell entity
// each wears, the furniture standing in it, and the lights that make it a room
// rather than a lit box.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O and no
// ZENITH_TOOLS guard -- the tables have to be visible to boot units in a headless
// CI build where the authoring is compiled out entirely, AND to the runtime
// components, which compile in every configuration. HEADER-ONLY.
//
// ★★ WHY THE ROOMS NEEDED LIGHTS AT ALL, stated because it is the whole reason
// the interiors read as flat. Zenithmon authored NO Zenith_LightComponent
// anywhere, in any scene. Both interiors were lit by the global ambient term
// alone, which is a constant: it has no direction, so nothing casts, nothing
// falls off with distance, and every surface returns its albedo scaled by one
// number. The engine has SSGI, SSAO, IBL, CSM and volumetric fog, and NONE of
// them can do anything with a scene containing no light -- SSAO darkens a
// contact that has no shading to darken, and CSM has no caster. A PBR material
// under a constant ambient renders exactly as flat as a painted one, which is
// why better meshes and better textures alone would not have moved the interiors.
//
// ★ THE LIGHTS ARE POINT LIGHTS WITH REAL LUMENS AND REAL RANGES. Zenith's
// Zenith_LightComponent takes intensity in LUMENS (SetIntensity clamps to 10M),
// so these are photometric rather than arbitrary: ~900 lm is a domestic bulb and
// ~1600 lm is a bright fluorescent tube, which is the honest difference between
// a bedroom and a laboratory.
// ============================================================================

// ---- The shell entity each room wears ---------------------------------------
//
// One visual-only entity per room, carrying ZM_InteriorShell, which loads the
// generated multi-surface room model. The seven blockout blocks keep their
// colliders and carry no visual at all -- the same visual/collider split the
// Dawnmere facades use, and for the same reason: Zenith_ColliderComponent sizes
// an AABB from mesh bounds when a model is present, and explicit half-extents are
// never serialized, so a block wearing a model would rebuild its collider from
// whatever had streamed in by then.
inline constexpr const char* szZM_PLAYERHOME_SHELL_ENTITY_NAME = "PlayerHomeShell";
inline constexpr const char* szZM_PROFLAB_SHELL_ENTITY_NAME    = "ProfLabShell";

// TOTAL: any other name answers ZM_INTERIOR_ROOM_COUNT, which every caller reads
// as "this entity is not a room shell". It does NOT answer with a plausible room,
// because a shell wearing the wrong room's model would render a complete and
// correct-looking interior belonging to the other building.
inline ZM_INTERIOR_ROOM ZM_RoomForShellEntity(const char* szEntityName)
{
	if (szEntityName == nullptr)
	{
		return ZM_INTERIOR_ROOM_COUNT;
	}
	if (strcmp(szEntityName, szZM_PLAYERHOME_SHELL_ENTITY_NAME) == 0)
	{
		return ZM_INTERIOR_ROOM_PLAYER_HOME;
	}
	if (strcmp(szEntityName, szZM_PROFLAB_SHELL_ENTITY_NAME) == 0)
	{
		return ZM_INTERIOR_ROOM_PROF_LAB;
	}
	return ZM_INTERIOR_ROOM_COUNT;
}

inline bool ZM_IsInteriorShellEntity(const char* szEntityName)
{
	return ZM_RoomForShellEntity(szEntityName) < ZM_INTERIOR_ROOM_COUNT;
}

// ---- Furniture ---------------------------------------------------------------
//
// ★★ THE KEEP-CLEAR CORRIDOR IS A HARD CONSTRAINT, NOT A STYLE NOTE. Both rooms
// are traversed by automated tests that drive the player with the shared walk
// driver, which has NO OBSTACLE AVOIDANCE (map playbook 3.4): anything with a
// collider on the line wedges the walk, and the failure names a DISTANCE rather
// than the blocker. The player enters at the +Z aperture on the room's axis and
// leaves the same way, so the band |x| <= fZM_INTERIOR_CORRIDOR_HALF_WIDTH is
// reserved along the whole depth of both rooms and every prop below clears it.
// ZM_Interaction/InteriorPropsClearTheEntranceCorridor is the enforcement.
inline constexpr float fZM_INTERIOR_CORRIDOR_HALF_WIDTH = 2.60f;

// A prop's footprint radius, used by the corridor and wall clauses. Generously
// over the roster's largest half-extent so a clause is never argued down.
inline constexpr float fZM_INTERIOR_PROP_RADIUS = 1.20f;

struct ZM_InteriorProp
{
	const char* m_szEntityName;
	ZM_PROP_ID  m_eProp;
	float       m_fX;
	float       m_fZ;
	// Yaw as a FROZEN quaternion pair (w, y); x and z are always 0 for a Y-axis
	// rotation. ★ AUTHORED VERBATIM through AddStep_SetTransformRotationQuat and
	// never computed: glm::angleAxis runs sin/cos, and MSVC Debug and Release
	// disagree by 1-2 ULP, which is exactly how a COMMITTED scene starts
	// ping-ponging in git (ZM-D-183). These are the four values a Y rotation can
	// take here, all of them exact in binary.
	float       m_fQuatW;
	float       m_fQuatY;
};

// The four frozen yaw values a placement may take now live in ZM_PropData.h,
// beside the roster, because the OUTDOOR table needs the same four and a second
// copy of a frozen constant is how two tables start disagreeing. The argument for
// why they are frozen at all, and the naming defect they were born with, moved
// with them.

// ★ WHICH WAY A YAW TURNS A MODEL, stated once so a placement row does not have
// to re-derive it. A rotation of a about +Y maps +X to (cos a, 0, -sin a), so
// from a model whose front is +X: YAW0 faces +X, YAW90 faces -Z, YAW180 faces -X.
// Every imported prop so far has its front on +X -- the chair's backrest occupies
// the model's -X face and the shelf's back panel occupies its whole -X face --
// which is what makes that the useful reference direction. It is a property of
// the deliveries, not a rule the pipeline enforces, so MEASURE each new one.

// PlayerHome: somebody's bedroom. Bed and shelf on the -X wall, table and chair
// on the +X wall, a barrel in the far corner -- all outside the corridor.
inline constexpr ZM_InteriorProp axZM_PLAYERHOME_PROPS[] =
{
	{ "HomeBed",     ZM_PROP_BED,     -5.60f, -3.60f, fZM_INTERIOR_YAW90_W, fZM_INTERIOR_YAW90_Y },
	// ★ THE SHELF'S BACK GOES AGAINST THE -X WALL, and this row said YAW90
	// until the model was MEASURED. A shelf has a back, and the old value was
	// chosen while this prop was a symmetric greybox box whose yaw no picture
	// could contradict. The imported mesh is 0.264 x 0.998 x 0.486 m with a
	// CONTINUOUS panel across its whole -X face -- 90% of the (Y, Z) footprint
	// has material within 15 mm of the -X extreme, against 38% on +X, which is
	// instead broken into open bays with the books and jars modelled into them.
	// So the model's front is +X like every import before it, and YAW0 is what
	// leaves it there: the back to the wall, the width along Z, the open front
	// into the room. YAW90 would have stood it side-on, its open face down the
	// room and 0.97 m of it protruding from the wall.
	{ "HomeShelf",   ZM_PROP_SHELF,   -6.90f,  0.90f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
	{ "HomeTable",   ZM_PROP_TABLE,    5.40f, -2.40f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
	// ★ THE CHAIR FACES THE TABLE, and it is the first prop for which that
	// sentence means anything. Every earlier one was a symmetric greybox box, so
	// its yaw was unobservable and the two defects above went unnoticed. The chair
	// model's front is +X, the table is at -Z from it, and YAW90 turns +X to -Z.
	{ "HomeChair",   ZM_PROP_CHAIR,    5.40f, -0.90f, fZM_INTERIOR_YAW90_W, fZM_INTERIOR_YAW90_Y },
	// ★★ THE BARREL HAS NO FACING, AND THAT IS A MEASUREMENT RATHER THAN AN
	// ASSUMPTION FROM THE WORD "BARREL". Every other imported prop's yaw was
	// decided by finding a back; this one was decided by establishing that there
	// is no back to find, which is the same question with the opposite answer and
	// needs the same evidence. Its body is a solid of revolution to within
	// **2.3%** peak deviation from a circle in every one of eight height bands,
	// and its base colour carries only stave grain and iron hoops -- 15% spread
	// across 36 azimuth bins with the dark ones SCATTERED, no brand, stencil or
	// painted mark anywhere in the 2048^2 map. The only asymmetry in the whole
	// asset is a small lid plug sitting 0.040 m off the axis, 9% of the barrel's
	// own radius, on a horizontal surface.
	//
	// So YAW0 here is not the unexamined default it was for the shelf and the
	// counters -- it is free, and any value would be as correct. Left at YAW0 in
	// both rooms because a constant nothing can distinguish should be the one that
	// costs a reader the least. **Do not "fix" it**, and do not read the +X-front
	// convention above as applying: this model has no +X face to speak of.
	{ "HomeBarrel",  ZM_PROP_BARREL,   6.60f, -4.60f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
};

// ProfLab: a working laboratory. Benches down both long walls, a shelf of
// reference material, a specimen barrel.
inline constexpr ZM_InteriorProp axZM_PROFLAB_PROPS[] =
{
	// ★★ THE FIRST PAIR OF COPIES THAT DISAGREE, and they have to. Every prop
	// above appears once, or twice with one shared yaw, because a symmetric
	// greybox has no observable facing -- so "both copies, same constant" was
	// never a decision anybody made. These two stand against OPPOSITE walls, and
	// a bench's back belongs to ITS OWN wall, so one value cannot serve both.
	//
	// The model's facing was MEASURED off the decoded mesh, and the shelf's
	// instrument -- "which extreme has a continuous panel against it" -- answers
	// NOTHING here: a lab bench has no flat back panel, and the best face scores
	// 11% where the shelf's back scored 90%. What the mesh does carry is the two
	// features the brief asks for. 232 of its 5661 vertices sit ABOVE the worktop
	// plane, confined to x in [-0.159, -0.125] of a model spanning +/-0.216 --
	// the shallow upstand, 64% of the half-width toward -X. And the worktop
	// overhangs the cabinet body by 20 mm on +X and by 0 mm on -X -- a nosing,
	// which a worktop has at the front and never where it dies into a wall.
	// Both say the same thing: the BACK is -X, the FRONT is +X, like every
	// import before it.
	//
	// So the west bench, on the -X wall, keeps its back there with YAW0, and the
	// east bench needs the half turn. YAW90 -- what both rows said while this
	// prop was a box -- would have turned the 2.2 m length onto the X axis and
	// stood each bench END-ON to its wall, protruding into the room.
	{ "LabCounterWest",  ZM_PROP_COUNTER, -8.20f, -3.00f, fZM_INTERIOR_YAW0_W,   fZM_INTERIOR_YAW0_Y   },
	{ "LabCounterEast",  ZM_PROP_COUNTER,  8.20f, -3.00f, fZM_INTERIOR_YAW180_W, fZM_INTERIOR_YAW180_Y },
	// Same -X wall, same reasoning as HomeShelf above -- YAW0 puts the measured
	// back panel against it.
	{ "LabShelf",        ZM_PROP_SHELF,   -8.60f,  2.60f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
	{ "LabTable",        ZM_PROP_TABLE,    3.80f, -6.20f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
	// ★ THE LAB CHAIR STANDS AT NO TABLE -- LabTable is 9 m away -- so there is
	// nothing for it to face and a facing still has to be chosen. It faces the
	// room (-X, a half turn from the model's +X front) rather than the near wall,
	// which is the least-wrong answer for a chair on open floor.
	{ "LabChair",        ZM_PROP_CHAIR,    5.60f,  2.80f, fZM_INTERIOR_YAW180_W, fZM_INTERIOR_YAW180_Y },
	// Free, like HomeBarrel -- the measurement is on that row.
	{ "LabBarrel",       ZM_PROP_BARREL,   8.40f,  5.40f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
};

inline u_int ZM_GetInteriorPropCount(ZM_INTERIOR_ROOM eRoom)
{
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PLAYER_HOME:
		return (u_int)(sizeof(axZM_PLAYERHOME_PROPS) / sizeof(axZM_PLAYERHOME_PROPS[0]));
	case ZM_INTERIOR_ROOM_PROF_LAB:
		return (u_int)(sizeof(axZM_PROFLAB_PROPS) / sizeof(axZM_PROFLAB_PROPS[0]));
	default:
		return 0u;
	}
}

// TOTAL: an out-of-range room or index answers the first PlayerHome row.
inline const ZM_InteriorProp& ZM_GetInteriorProp(ZM_INTERIOR_ROOM eRoom, u_int uIndex)
{
	const u_int uCount = ZM_GetInteriorPropCount(eRoom);
	if (uCount == 0u || uIndex >= uCount)
	{
		return axZM_PLAYERHOME_PROPS[0];
	}
	return (eRoom == ZM_INTERIOR_ROOM_PROF_LAB)
		? axZM_PROFLAB_PROPS[uIndex]
		: axZM_PLAYERHOME_PROPS[uIndex];
}

// Which prop an authored furniture entity wears, by name, across BOTH rooms.
//
// TOTAL: any other name answers ZM_PROP_NONE, which the runtime component reads
// as "this entity is not interior furniture". Resolving from the name rather than
// serializing the id keeps the committed scene to a component name and a version
// u_int -- the same reason ZM_BuildingFacade and ZM_InteriorShell do it.
inline ZM_PROP_ID ZM_PropForInteriorPropEntity(const char* szEntityName)
{
	if (szEntityName == nullptr)
	{
		return ZM_PROP_NONE;
	}
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const u_int uCount = ZM_GetInteriorPropCount(eRoom);
		for (u_int i = 0u; i < uCount; ++i)
		{
			const ZM_InteriorProp& xProp = ZM_GetInteriorProp(eRoom, i);
			if (strcmp(szEntityName, xProp.m_szEntityName) == 0)
			{
				return xProp.m_eProp;
			}
		}
	}
	return ZM_PROP_NONE;
}

// ---- Lights ------------------------------------------------------------------
struct ZM_InteriorLight
{
	const char* m_szEntityName;
	float m_fX, m_fY, m_fZ;
	float m_fLumens;
	float m_fRange;          // metres
	float m_fR, m_fG, m_fB;  // linear colour
};

// ★ THE COLOURS ARE THE SECOND HALF OF THE ZM-D-176 ANSWER. The materials make
// the two rooms different at unit reflectance; the lamps then push them further
// apart in the SAME direction -- warm tungsten over the bedroom, cool fluorescent
// over the lab -- so the red/blue ratio gap ZM_InteriorTintPixels_Test measures
// comes from two reinforcing sources rather than one hue nudge doing all the work.
// ★★★ THE HONEST PHOTOMETRIC VALUES ARE BACK, AND THIS IS THE PARAGRAPH THAT
// SAID THEY WOULD BE. Every intensity here was cut to ~14% on 2026-09-01 --
// 130 lm for a ceiling pendant, 160 lm tubes -- and the note that did it was
// explicit that the cost was real and accepted "the day" the bloom calibration
// was re-derived: *"The honest values belong here the day it is, and they should
// go back up together so the two rooms keep the relationship below."* The
// renderer stream is re-deriving that threshold now, so they are back:
//
//   ~900 lm  domestic ceiling pendant   (a 60 W-equivalent lamp)
//   ~250 lm  bedside and standard lamp  (a 25 W-equivalent lamp)
//   ~1600 lm per lab batten             (a 4 ft fluorescent tube)
//
// ★★ AND NOTHING HERE COMPENSATES FOR THE OLD BLOOM THRESHOLD. That is the
// whole point: the cut existed because Flux extracted bloom above a PRE-EXPOSURE
// HDR luminance of 3.0 (an ABSOLUTE scene-referred figure the auto-exposure
// cannot rescue), and pre-compensating for a threshold that is being re-derived
// would bake this stream's guess about another stream's number into a table that
// outlives both. If a fixture still blooms too hard once that lands, the fix is
// the threshold or the fixture's emissive, not a second retreat from photometry.
//
// ★★ THE HOUSE-TO-LAB RATIO MOVED FROM ~1.7x TO ~2.7x, AND THE FIRST DRAFT OF
// THIS BLOCK CLAIMED IT DID NOT. It said both rooms were "scaled by the same
// factor" back up and the relationship was therefore preserved. That is FALSE
// and the arithmetic says so: the restore is 6.9x on the pendant, 5.0x and 5.6x
// on the two lamps and 10.0x on the battens, because the honest figure for each
// lamp is a property of THAT LAMP and not a multiple of what the bloom cut left.
// Per unit floor area the two rooms went 1.17 -> 7.29 lm/m2 and 2.00 -> 20.0
// lm/m2, i.e. 1.71x -> 2.74x.
//
// ★ AND THE NEW NUMBER IS THE MORE FAITHFUL ONE, which is why the values stay
// and this comment changed instead. 1.707x was never a designed figure: it was
// the arithmetic of a UNIFORM cut applied on 2026-09-01, and a uniform cut
// preserves whatever ratio it starts from whether or not that ratio means
// anything. What ZM-D-176 actually ruled is qualitative -- "a lab is lit evenly
// and brightly, which is a lighting decision as much as a colour one" -- and
// real practice puts a bedroom at 50-100 lux against a laboratory's 300-500,
// a ratio of 3x to 10x. 2.74x sits just inside that; 1.71x was under it.
//
// So the invariant worth holding is the ORDERING and its rough size -- the lab
// is clearly brighter per square metre, and not so much brighter that the
// bedroom reads as unlit beside it -- which is what
// ZM_Gen/InteriorLightsCarryPhotometricIntensities asserts. A test that pinned
// 1.7x would be pinning the residue of an emergency cut.
//
// ★ IT ALSO LEAVES ZM_InteriorTintPixels_Test's MEASUREMENT ALONE, for the same
// reason it always did: that test reads a red/blue RATIO per room and compares
// the two, and a per-room scale cancels out of a chromaticity ratio.
//
// ★★ THE OTHER HALF OF THE FIX IS THAT THESE LIGHTS NOW HAVE FIXTURES. Every
// one of them was a bare Zenith_LightComponent at a point in space -- a room
// with lamplight and no lamp, which is most of why the interiors read as CG.
// axZM_*_FIXTURES below gives each light a generated body and a glowing shade
// (ZM_PROP_KIND_LIGHT_FIXTURE), and the shade's emissive colour is matched to
// the light's own colour in ZM_GetPropEmissive -- so the thing that glows and
// the light it throws agree. A fixture's geometry BRACKETS its light: the
// pendant's shade drum spans the 2.70 m lamp, the batten's diffuser spans the
// 3.20 m tube.
inline constexpr ZM_InteriorLight axZM_PLAYERHOME_LIGHTS[] =
{
	// A warm ceiling pendant on the room's axis, and two lower fills so the long
	// walls are not lit by one point (which reads as a spotlight in a cave).
	{ "HomeLampCeiling", 0.00f, 2.70f,  0.00f,  900.0f, 14.0f, 1.00f, 0.78f, 0.52f },
	// ★ BESIDE THE BED, NOT INSIDE IT. This lamp shipped at (-5.20, 1.15, -3.40),
	// which is horizontally WITHIN the bed's footprint and 0.39 m above its
	// mattress -- a 360 lm point source at that range saturates whatever it is
	// over. On the greybox bed that read as a bright patch on a dull box and
	// nobody looked twice; against a real bed with a 2048^2 albedo it is a white
	// blob where the bedding should be, and no amount of material work survives
	// it. Moved out along +X to clear the bed's side (its +X face sits at
	// x = -4.88 once ZM_PropFit has sized it) and down to nightstand height, which
	// roughly quadruples the distance to the mattress and is also what a bedside
	// lamp physically IS. Intensity, range and colour are UNTOUCHED -- those are
	// ZM-D-176's warm-tungsten decision and this is a placement fix.
	{ "HomeLampBedside", -4.20f, 1.05f, -3.90f, 250.0f,  6.0f, 1.00f, 0.72f, 0.44f },
	{ "HomeLampTable",    5.20f, 1.35f, -1.60f, 250.0f,  6.0f, 1.00f, 0.74f, 0.46f },
};

inline constexpr ZM_InteriorLight axZM_PROFLAB_LIGHTS[] =
{
	// Four cool tubes on a grid -- a lab is lit evenly and brightly, which is a
	// lighting decision as much as a colour one.
	{ "LabTubeNW", -5.00f, 3.20f, -4.00f, 1600.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeNE",  5.00f, 3.20f, -4.00f, 1600.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeSW", -5.00f, 3.20f,  4.00f, 1600.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeSE",  5.00f, 3.20f,  4.00f, 1600.0f, 13.0f, 0.72f, 0.84f, 1.00f },
};

// ---- The FIXTURES those lights live in ---------------------------------------
//
// ★★ ONE ROW PER LIGHT, AND THE PAIRING IS ASSERTED. A fixture whose shade does
// not contain its lamp is the failure this table exists to avoid, and it renders
// perfectly happily: a glowing shade beside a glow. ZM_Tests_InteriorGen walks
// both tables together and checks that each light's Y falls inside its fixture's
// GLOWING part, so moving a lamp without moving its shade reds.
//
// ★ THE Y IS THE FIXTURE'S BASE, because ZM_PropFit grounds a model at its own
// min.y and these four are built at EXACTLY their roster size (no mesh jitter --
// see ZM_BuildPropMesh), so scale is 1 and the authored Y is the base to the
// millimetre. That is what lets a pendant end flush against the ceiling:
//   PlayerHome ceiling 3.00 - pendant 0.34 = base 2.66, rose top exactly 3.00.
//   ProfLab    ceiling 3.50 - batten  0.32 = base 3.18, rod top exactly 3.50.
//
// ★★ THE PENDANT SITS ON THE ROOM AXIS AT x = 0, INSIDE THE ENTRANCE CORRIDOR,
// AND THAT IS SAFE FOR A REASON THE FURNITURE ROWS CANNOT USE. The corridor rule
// (fZM_INTERIOR_CORRIDOR_HALF_WIDTH) exists because the walk driver has NO
// obstacle avoidance and a COLLIDER on the line wedges it. Fixtures are authored
// VISUAL-ONLY -- no ZM_ColliderComponent, ever -- and this one hangs at 2.66 m,
// clear over a 1.8 m body. It also clears the indoor camera: ZM_FollowCamera
// caps the lens at ceiling - 0.35 = 2.65 in PlayerHome, and the shade's lowest
// geometry is at 2.66. Both clauses are asserted, because 10 mm is a margin
// somebody will otherwise spend without noticing.
struct ZM_InteriorFixture
{
	const char* m_szEntityName;
	ZM_PROP_ID  m_eProp;
	const char* m_szLightEntityName;   // the light this fixture houses
	float       m_fX, m_fY, m_fZ;      // Y is the model's BASE
	float       m_fQuatW, m_fQuatY;    // frozen, like every other authored rotation
};

inline constexpr ZM_InteriorFixture axZM_PLAYERHOME_FIXTURES[] =
{
	{ "HomeFixtureCeiling", ZM_PROP_PENDANT_LAMP, "HomeLampCeiling",
		0.00f, 2.66f,  0.00f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
	// The nightstand lamp stands on the floor beside the bed, and its own
	// nightstand is part of the model -- so this is the one fixture that is also
	// furniture. Its shade spans 0.90..1.20 and the lamp is at 1.05.
	{ "HomeFixtureBedside", ZM_PROP_BEDSIDE_LAMP, "HomeLampBedside",
		-4.20f, 0.00f, -3.90f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
	// ★ A STANDARD LAMP, NOT A TABLE LAMP, and the light's own position is what
	// decided that. "HomeLampTable" sits at y = 1.35, which is 0.45 m above the
	// table's 0.90 m top and 0.8 m away from it in Z -- it never was a lamp ON the
	// table. A floor-standing lamp beside it brackets 1.35 with a shade at
	// 1.20..1.50 and leaves the light exactly where ZM-D-176 put it.
	{ "HomeFixtureTable",   ZM_PROP_FLOOR_LAMP,   "HomeLampTable",
		5.20f, 0.00f, -1.60f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
};

inline constexpr ZM_InteriorFixture axZM_PROFLAB_FIXTURES[] =
{
	{ "LabFixtureNW", ZM_PROP_LAB_BATTEN, "LabTubeNW", -5.00f, 3.18f, -4.00f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
	{ "LabFixtureNE", ZM_PROP_LAB_BATTEN, "LabTubeNE",  5.00f, 3.18f, -4.00f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
	{ "LabFixtureSW", ZM_PROP_LAB_BATTEN, "LabTubeSW", -5.00f, 3.18f,  4.00f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
	{ "LabFixtureSE", ZM_PROP_LAB_BATTEN, "LabTubeSE",  5.00f, 3.18f,  4.00f, fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y },
};

inline u_int ZM_GetInteriorFixtureCount(ZM_INTERIOR_ROOM eRoom)
{
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PLAYER_HOME:
		return (u_int)(sizeof(axZM_PLAYERHOME_FIXTURES) / sizeof(axZM_PLAYERHOME_FIXTURES[0]));
	case ZM_INTERIOR_ROOM_PROF_LAB:
		return (u_int)(sizeof(axZM_PROFLAB_FIXTURES) / sizeof(axZM_PROFLAB_FIXTURES[0]));
	default:
		return 0u;
	}
}

// TOTAL: an out-of-range room or index answers the first PlayerHome row.
inline const ZM_InteriorFixture& ZM_GetInteriorFixture(ZM_INTERIOR_ROOM eRoom, u_int uIndex)
{
	const u_int uCount = ZM_GetInteriorFixtureCount(eRoom);
	if (uCount == 0u || uIndex >= uCount)
	{
		return axZM_PLAYERHOME_FIXTURES[0];
	}
	return (eRoom == ZM_INTERIOR_ROOM_PROF_LAB)
		? axZM_PROFLAB_FIXTURES[uIndex]
		: axZM_PLAYERHOME_FIXTURES[uIndex];
}

// The GLOWING part of a fixture, as a height band above the model's base. This
// is what "the shade brackets its lamp" is checked against, and it is stated
// once here rather than re-derived from the mesh: the mesh is built from these
// same numbers in ZM_BuildPropMesh's fixture arm.
inline bool ZM_GetInteriorFixtureGlowBand(ZM_PROP_ID eProp, float& fLowOut, float& fHighOut)
{
	switch (eProp)
	{
	case ZM_PROP_PENDANT_LAMP: fLowOut = 0.00f; fHighOut = 0.22f; return true;   // the three drums
	case ZM_PROP_BEDSIDE_LAMP: fLowOut = 0.90f; fHighOut = 1.20f; return true;   // the shade
	case ZM_PROP_FLOOR_LAMP:   fLowOut = 1.20f; fHighOut = 1.50f; return true;   // the shade
	case ZM_PROP_LAB_BATTEN:   fLowOut = 0.00f; fHighOut = 0.04f; return true;   // the diffuser
	default: fLowOut = 0.0f; fHighOut = 0.0f; return false;
	}
}

// Which prop a FIXTURE entity wears, by name.
//
// ★ ITS OWN RESOLVER RATHER THAN WIDENING ZM_PropForInteriorPropEntity, which is
// the pattern this header already argues for on the Dawnmere side: each table
// owns the resolver for ITS rows and the consumer that sees several composes
// them (ZM_InteriorFurniture::OnStart). It also keeps the two tables' failure
// modes apart -- a furniture row that stops resolving and a fixture row that
// does are different defects with different fixes.
//
// TOTAL: any other name answers ZM_PROP_NONE.
inline ZM_PROP_ID ZM_PropForInteriorFixtureEntity(const char* szEntityName)
{
	if (szEntityName == nullptr)
	{
		return ZM_PROP_NONE;
	}
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const u_int uCount = ZM_GetInteriorFixtureCount(eRoom);
		for (u_int i = 0u; i < uCount; ++i)
		{
			const ZM_InteriorFixture& xFixture = ZM_GetInteriorFixture(eRoom, i);
			if (strcmp(szEntityName, xFixture.m_szEntityName) == 0)
			{
				return xFixture.m_eProp;
			}
		}
	}
	return ZM_PROP_NONE;
}

inline u_int ZM_GetInteriorLightCount(ZM_INTERIOR_ROOM eRoom)
{
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PLAYER_HOME:
		return (u_int)(sizeof(axZM_PLAYERHOME_LIGHTS) / sizeof(axZM_PLAYERHOME_LIGHTS[0]));
	case ZM_INTERIOR_ROOM_PROF_LAB:
		return (u_int)(sizeof(axZM_PROFLAB_LIGHTS) / sizeof(axZM_PROFLAB_LIGHTS[0]));
	default:
		return 0u;
	}
}

inline const ZM_InteriorLight& ZM_GetInteriorLight(ZM_INTERIOR_ROOM eRoom, u_int uIndex)
{
	const u_int uCount = ZM_GetInteriorLightCount(eRoom);
	if (uCount == 0u || uIndex >= uCount)
	{
		return axZM_PLAYERHOME_LIGHTS[0];
	}
	return (eRoom == ZM_INTERIOR_ROOM_PROF_LAB)
		? axZM_PROFLAB_LIGHTS[uIndex]
		: axZM_PLAYERHOME_LIGHTS[uIndex];
}
