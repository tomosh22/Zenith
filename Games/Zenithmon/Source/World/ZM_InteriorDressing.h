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
	{ "HomeBarrel",  ZM_PROP_BARREL,   6.60f, -4.60f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
};

// ProfLab: a working laboratory. Benches down both long walls, a shelf of
// reference material, a specimen barrel.
inline constexpr ZM_InteriorProp axZM_PROFLAB_PROPS[] =
{
	{ "LabCounterWest",  ZM_PROP_COUNTER, -8.20f, -3.00f, fZM_INTERIOR_YAW90_W, fZM_INTERIOR_YAW90_Y },
	{ "LabCounterEast",  ZM_PROP_COUNTER,  8.20f, -3.00f, fZM_INTERIOR_YAW90_W, fZM_INTERIOR_YAW90_Y },
	// Same -X wall, same reasoning as HomeShelf above -- YAW0 puts the measured
	// back panel against it.
	{ "LabShelf",        ZM_PROP_SHELF,   -8.60f,  2.60f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
	{ "LabTable",        ZM_PROP_TABLE,    3.80f, -6.20f, fZM_INTERIOR_YAW0_W,  fZM_INTERIOR_YAW0_Y  },
	// ★ THE LAB CHAIR STANDS AT NO TABLE -- LabTable is 9 m away -- so there is
	// nothing for it to face and a facing still has to be chosen. It faces the
	// room (-X, a half turn from the model's +X front) rather than the near wall,
	// which is the least-wrong answer for a chair on open floor.
	{ "LabChair",        ZM_PROP_CHAIR,    5.60f,  2.80f, fZM_INTERIOR_YAW180_W, fZM_INTERIOR_YAW180_Y },
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
inline constexpr ZM_InteriorLight axZM_PLAYERHOME_LIGHTS[] =
{
	// A warm ceiling pendant on the room's axis, and two lower fills so the long
	// walls are not lit by one point (which reads as a spotlight in a cave).
	{ "HomeLampCeiling", 0.00f, 2.70f,  0.00f,  950.0f, 14.0f, 1.00f, 0.78f, 0.52f },
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
	{ "HomeLampBedside", -4.20f, 1.05f, -3.90f, 360.0f,  6.0f, 1.00f, 0.72f, 0.44f },
	{ "HomeLampTable",    5.20f, 1.35f, -1.60f, 330.0f,  6.0f, 1.00f, 0.74f, 0.46f },
};

inline constexpr ZM_InteriorLight axZM_PROFLAB_LIGHTS[] =
{
	// Four cool tubes on a grid -- a lab is lit evenly and brightly, which is a
	// lighting decision as much as a colour one.
	{ "LabTubeNW", -5.00f, 3.20f, -4.00f, 1150.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeNE",  5.00f, 3.20f, -4.00f, 1150.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeSW", -5.00f, 3.20f,  4.00f, 1150.0f, 13.0f, 0.72f, 0.84f, 1.00f },
	{ "LabTubeSE",  5.00f, 3.20f,  4.00f, 1150.0f, 13.0f, 0.72f, 0.84f, 1.00f },
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
