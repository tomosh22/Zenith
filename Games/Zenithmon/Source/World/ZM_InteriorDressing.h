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
// ★★★ EVERY INTENSITY BELOW WAS CUT TO ~14% ON 2026-09-01, AND THE REASON IS
// BLOOM RATHER THAN BRIGHTNESS. Nothing was clipping -- MEASURED across nine
// captures, 0.00% of every frame sat above luminance 250 -- so the tonemapper
// was holding and these rooms were not "blown out" in the usual sense. What was
// wrong is that Flux extracts bloom above a PRE-EXPOSURE HDR luminance of 3.0
// (Flux_HDR.cpp), which is an ABSOLUTE scene-referred figure and therefore
// independent of the auto-exposure: a light either pushes its surroundings past
// it or it does not, and these did, by enough that the glow swallowed the
// fixtures. The bedside lamp's halo was 114 px across in the bed's own capture,
// 2.96% of the frame.
//
// ★ THE TARGET IS PROFLAB'S OWN OLD NUMBER, not a taste call. Its tubes measured
// a 26 px halo at 0.15% of frame in the same run and read as a lamp that glows
// slightly -- so "slight" had a value already, and the others were re-tuned onto
// it rather than onto an opinion.
//
// ★★ THE LAB IS SCALED BY THE SAME FACTOR EVEN THOUGH IT WAS ALREADY IN RANGE,
// and that is deliberate. Its four tubes are the brightest rig in the game BY
// DESIGN (ZM-D-176: "a lab is lit evenly and brightly, which is a lighting
// decision as much as a colour one"), and it only measured well because they hang
// at 3.2 m in a 20 x 16 m room, far from anything. Cutting the house alone would
// have left the lab ~4x brighter per unit floor area instead of ~1.7x, which
// changes a designed relationship as a side effect of fixing a different problem.
// Scaling both keeps the ratio and keeps ZM_InteriorTintPixels_Test's red/blue
// measurement untouched, since that is a RATIO and a uniform scale cancels.
//
// ★★ AND THE CUT IS NEARLY FREE ON SCREEN, WHICH IS WHY IT COULD GO THIS FAR.
// The auto-exposure adapts to whatever the room emits, so scaling every light in
// a room TOGETHER does not darken the picture -- MEASURED, the room-wide frame's
// mean luminance moved 103.3 -> 99.5 across the first 3x cut, i.e. not at all.
// What a uniform cut DOES change is the ratio between a lamp and the surfaces
// around it, which is precisely the bloom. So the usual "dimmer means darker"
// intuition does not apply here and was not what limited the tuning.
//
// ★ THE PHOTOMETRIC COST IS REAL AND IS ACCEPTED. 130 lm is a nightlight, not a
// ceiling pendant, and 160 lm tubes are nothing like a laboratory's. These are
// the figures the CURRENT bloom calibration permits: the threshold (3.0
// pre-exposure HDR) was raised from 1.0 for the brighter SUN, and nothing has
// re-derived it for point-source fixtures indoors. The honest values belong here
// the day it is, and they should go back up together so the two rooms keep the
// relationship below.
//
// ★ THE TWO ROOMS KEEP THEIR ~1.7x RATIO, deliberately. Cutting only the house
// would have left the lab over 4x brighter per unit floor area instead of 1.7x --
// a designed relationship (ZM-D-176) broken as a side effect of fixing bloom.
// Both were scaled by the same factor instead, which also leaves
// ZM_InteriorTintPixels_Test's red/blue measurement untouched: it is a RATIO, and
// a uniform scale cancels out of it.
inline constexpr ZM_InteriorLight axZM_PLAYERHOME_LIGHTS[] =
{
	// A warm ceiling pendant on the room's axis, and two lower fills so the long
	// walls are not lit by one point (which reads as a spotlight in a cave).
	{ "HomeLampCeiling", 0.00f, 2.70f,  0.00f,  130.0f, 14.0f, 1.00f, 0.78f, 0.52f },
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
	{ "HomeLampBedside", -4.20f, 1.05f, -3.90f,  50.0f,  6.0f, 1.00f, 0.72f, 0.44f },
	{ "HomeLampTable",    5.20f, 1.35f, -1.60f,  45.0f,  6.0f, 1.00f, 0.74f, 0.46f },
};

inline constexpr ZM_InteriorLight axZM_PROFLAB_LIGHTS[] =
{
	// Four cool tubes on a grid -- a lab is lit evenly and brightly, which is a
	// lighting decision as much as a colour one.
	{ "LabTubeNW", -5.00f, 3.20f, -4.00f,  160.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeNE",  5.00f, 3.20f, -4.00f,  160.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeSW", -5.00f, 3.20f,  4.00f,  160.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeSE",  5.00f, 3.20f,  4.00f,  160.0f, 13.0f, 0.72f, 0.84f, 1.00f },
};

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
