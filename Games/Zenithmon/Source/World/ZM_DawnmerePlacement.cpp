#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"

#include <cmath>

float ZM_DawnmereVesperYaw()
{
	const float fDeltaX = fZM_DAWNMERE_TOWN_CENTER_X - fZM_DAWNMERE_VESPER_X;
	const float fDeltaZ = fZM_DAWNMERE_TOWN_CENTER_Z - fZM_DAWNMERE_VESPER_Z;
	// X FIRST. See the header: this is the +Z-forward convention, not the usual
	// maths-library atan2(y, x).
	return std::atan2(fDeltaX, fDeltaZ);
}

Zenith_Maths::Quat ZM_DawnmereVesperFacing()
{
	return Zenith_Maths::AngleAxis(
		ZM_DawnmereVesperYaw(), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
}

// ============================================================================
// Known-limit W5 -- the anchor table. See the header for WHY these are compiled
// constants and what re-measures them.
//
// NOTHING BELOW MAY Zenith_Assert ON ITS ARGUMENTS: the boot units feed these
// functions the out-of-range ids and NaN half-extents on purpose, and
// Zenith_Assert breaks the process in EVERY configuration.
// ============================================================================

namespace
{
	// Rows in ZM_DAWNMERE_NPC_ID order. Each XZ keeps its DERIVATION rather than a
	// bare literal, so moving the town centre moves the plaza roster with it -- the
	// same relationship the authoring block in Zenithmon.cpp used to spell inline.
	//
	// The bound is DEDUCED, never spelled. With an explicit [ZM_DAWNMERE_NPC_COUNT]
	// the static_assert below would be a tautology and a forgotten row would merely
	// zero-initialise the tail into a NULL-named anchor at the world origin.
	// Deduced, a missing or extra row is a COMPILE error at the table itself.
	const ZM_DawnmereNpcAnchor s_axDawnmereNpcAnchors[] =
	{
		// entity name,          X,                                          Z,                                          feet Y
		// townCentre.x, townCentre.z + 10 -- straight +Z of the spawn, the walk-up target.
		{ "Npc_Villager",       fZM_DAWNMERE_TOWN_CENTER_X,          fZM_DAWNMERE_TOWN_CENTER_Z + 10.0f, fZM_DAWNMERE_FEET_Y_VILLAGER     },
		// townCentre.x + 14, townCentre.z + 18 -- 18 m off the z=480 Home corridor.
		{ "Npc_TradePostClerk", fZM_DAWNMERE_TOWN_CENTER_X + 14.0f,  fZM_DAWNMERE_TOWN_CENTER_Z + 18.0f, fZM_DAWNMERE_FEET_Y_CLERK        },
		// townCentre.x - 14, townCentre.z + 18 -- the mirror of the clerk.
		{ "Npc_Caretaker",      fZM_DAWNMERE_TOWN_CENTER_X - 14.0f,  fZM_DAWNMERE_TOWN_CENTER_Z + 18.0f, fZM_DAWNMERE_FEET_Y_CARETAKER    },
		// townCentre.x - 34, townCentre.z + 18 -- on the Home walkway, off the road.
		{ "Npc_Warden",         fZM_DAWNMERE_TOWN_CENTER_X - 34.0f,  fZM_DAWNMERE_TOWN_CENTER_Z + 18.0f, fZM_DAWNMERE_FEET_Y_WARDEN       },
		// townCentre.x + 28, townCentre.z - 4 -- patrol endpoint 0, 28 m east of spawn.
		{ "Npc_Wanderer",       fZM_DAWNMERE_TOWN_CENTER_X + 28.0f,  fZM_DAWNMERE_TOWN_CENTER_Z - 4.0f,  fZM_DAWNMERE_FEET_Y_WANDERER     },
		// The rival's XZ is DERIVED in this file's header; do not restate it here.
		{ "Npc_RivalVesper",    fZM_DAWNMERE_VESPER_X,               fZM_DAWNMERE_VESPER_Z,              fZM_DAWNMERE_FEET_Y_RIVAL_VESPER },
	};

	static_assert(
		sizeof(s_axDawnmereNpcAnchors) / sizeof(s_axDawnmereNpcAnchors[0])
			== ZM_DAWNMERE_NPC_COUNT,
		"the Dawnmere anchor table must have exactly one row per ZM_DAWNMERE_NPC_ID");

	// The wanderer's two patrol endpoints: a north/south loop at x = townCentre + 28.
	// Endpoint 0 IS the wanderer's spawn anchor, so it reuses that row's measured
	// feet height rather than carrying a second editable copy of the same surface.
	const ZM_DawnmereNpcAnchor s_axDawnmereWanderWaypoints[] =
	{
		// townCentre.x + 28, townCentre.z - 4 -- identical to the wanderer anchor.
		{ "WanderWaypoint0", fZM_DAWNMERE_TOWN_CENTER_X + 28.0f, fZM_DAWNMERE_TOWN_CENTER_Z - 4.0f, fZM_DAWNMERE_FEET_Y_WANDERER   },
		// townCentre.x + 28, townCentre.z + 4 -- 8 m north along the same line.
		{ "WanderWaypoint1", fZM_DAWNMERE_TOWN_CENTER_X + 28.0f, fZM_DAWNMERE_TOWN_CENTER_Z + 4.0f, fZM_DAWNMERE_FEET_Y_WANDER_WP1 },
	};

	// Handed back for an unregistered id or waypoint index. The alternative --
	// indexing anyway after an assert -- reads off the end of the table the moment
	// the break is stepped past. Every field is DEFINED and INERT: the town-centre
	// anchor (a real, safe piece of ground that no roster row occupies) under a name
	// no roster row uses, so a caller that skipped the id check gets a legal
	// coordinate rather than a NaN, and a test can still tell it apart from content.
	const ZM_DawnmereNpcAnchor s_xInvalidDawnmereAnchor =
	{
		"UNKNOWN",
		fZM_DAWNMERE_TOWN_CENTER_X,
		fZM_DAWNMERE_TOWN_CENTER_Z,
		fZM_DAWNMERE_TOWN_CENTER_FEET_Y
	};

	// A half-extent that cannot poison a transform. NaN, +/-inf and negatives all
	// collapse to zero, which makes every "centre" function degrade to the feet
	// height rather than to a body at an undefined altitude.
	float ZM_SanitiseCapsuleHalfExtent(float fCapsuleHalfExtent)
	{
		return (std::isfinite(fCapsuleHalfExtent) && fCapsuleHalfExtent > 0.0f)
			? fCapsuleHalfExtent
			: 0.0f;
	}
}

bool ZM_IsDawnmereNpcId(u_int uNpc)
{
	return uNpc < (u_int)ZM_DAWNMERE_NPC_COUNT;
}

const ZM_DawnmereNpcAnchor& ZM_GetDawnmereNpcAnchor(u_int uNpc)
{
	if (!ZM_IsDawnmereNpcId(uNpc))
	{
		// Mis-authored data or a mis-typed caller, so it is SAID OUT LOUD -- but
		// non-fatally, because the function is total and returning the sentinel row
		// is the contract.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_DawnmerePlacement] ZM_GetDawnmereNpcAnchor: id %u is not a Dawnmere "
			"NPC -- returning the UNKNOWN town-centre anchor", uNpc);
		return s_xInvalidDawnmereAnchor;
	}
	return s_axDawnmereNpcAnchors[uNpc];
}

float ZM_DawnmereNpcFeetY(u_int uNpc)
{
	return ZM_GetDawnmereNpcAnchor(uNpc).m_fFeetY;
}

float ZM_DawnmereNpcCentreY(u_int uNpc, float fCapsuleHalfExtent)
{
	return ZM_DawnmereNpcFeetY(uNpc)
		+ ZM_SanitiseCapsuleHalfExtent(fCapsuleHalfExtent);
}

float ZM_DawnmereWandererSpawnY(float fCapsuleHalfExtent)
{
	// ONE EXTRA half-extent above the resting centre. Spelled as centre + extra
	// rather than feet + 2 * half so the "extra air" is visible as its own term.
	const float fSanitised = ZM_SanitiseCapsuleHalfExtent(fCapsuleHalfExtent);
	return ZM_DawnmereNpcCentreY(ZM_DAWNMERE_NPC_WANDERER, fSanitised) + fSanitised;
}

u_int ZM_GetDawnmereWanderWaypointCount()
{
	return (u_int)(sizeof(s_axDawnmereWanderWaypoints)
		/ sizeof(s_axDawnmereWanderWaypoints[0]));
}

const ZM_DawnmereNpcAnchor& ZM_GetDawnmereWanderWaypoint(u_int uIndex)
{
	if (uIndex >= ZM_GetDawnmereWanderWaypointCount())
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_DawnmerePlacement] ZM_GetDawnmereWanderWaypoint: index %u is outside "
			"the authored patrol -- returning the UNKNOWN town-centre anchor", uIndex);
		return s_xInvalidDawnmereAnchor;
	}
	return s_axDawnmereWanderWaypoints[uIndex];
}

// ============================================================================
// ZM-D-173 -- the Home blockout. See the header for the clearance contract, the
// fixed derivation formulas, and what re-measures the table below.
// ============================================================================

namespace
{
	constexpr float fZM_HOME_SHELL_HALF_Z =
		fZM_DAWNMERE_HOME_SHELL_SCALE_Z * 0.5f;
	constexpr float fZM_HOME_SHELL_HALF_X =
		fZM_DAWNMERE_HOME_SHELL_SCALE_X * 0.5f;
	constexpr float fZM_HOME_SHELL_MIN_X = fZM_DAWNMERE_HOME_X - fZM_HOME_SHELL_HALF_X;
	constexpr float fZM_HOME_SHELL_MAX_X = fZM_DAWNMERE_HOME_X + fZM_HOME_SHELL_HALF_X;
	constexpr float fZM_HOME_SHELL_MIN_Z =
		fZM_DAWNMERE_HOME_SHELL_Z - fZM_HOME_SHELL_HALF_Z;
	constexpr float fZM_HOME_SHELL_MAX_Z =
		fZM_DAWNMERE_HOME_SHELL_Z + fZM_HOME_SHELL_HALF_Z;

	// Which way is OUT of the house: -1 when the entrance is the shell's MIN-Z
	// face, +1 when it is the MAX-Z face. Derived so the door ground samples below
	// follow the entrance instead of assuming a side.
	constexpr float fZM_HOME_ENTRANCE_OUTWARD =
		fZM_DAWNMERE_HOME_ENTRANCE_Z < fZM_DAWNMERE_HOME_SHELL_Z ? -1.0f : 1.0f;

	// ★ THE DOOR GROUND IS SAMPLED HALF A METRE OUT INTO THE FORECOURT, NOT ON THE
	// JAMB'S OWN COLUMN, AND THAT IS FORCED BY GEOMETRY RATHER THAN CHOSEN.
	// The entrance plane COINCIDES with a shell face, and each jamb straddles it
	// with its own 0.5 m depth (z 475.75..476.25). A downward probe on a jamb's
	// exact column therefore has TWO solid bodies over it, and neither can be
	// stepped past:
	//   * the jamb's underside is exactly AT the surface being measured, so
	//     restarting the ray below it restarts ON the ground;
	//   * the shell is EMBEDDED 0.05 m below its lowest corner, which at this
	//     column puts its underside ~0.10 m BELOW the ground -- restarting below
	//     THAT starts underneath the terrain and misses entirely.
	// Zenith_PhysicsQuery::RaycastIgnoring takes ONE entity, so the pair cannot be
	// filtered out either, and Zenith_TerrainComponent exposes no height-query API
	// that would bypass colliders altogether -- a raycast is the only ground probe
	// there is. That missing API is booked as Shortfalls E8; closing it is what
	// would let these rows be sampled on their own columns. Half a metre out clears the jamb's 0.25 m protrusion and the shell
	// face, while staying deep inside the Home pad's 36 m flatten radius.
	//
	// ★ AND THE RESIDUAL IS HONESTLY UNKNOWN, WHICH IS NOT THE SAME AS SMALL.
	// What this offset buys is a MEASURABLE column; what it costs is that each
	// jamb's authored height comes from ground half a metre away, and the ground at
	// its OWN column is unmeasurable by construction -- so the difference between
	// them is never observed. Do not quote a figure for it. The samples that
	// bracket the doorway do not agree on even the SIGN: (384, 474) reads 26.29139
	// while the two door columns at z=475.5 read 26.228/26.216 (falling with +Z),
	// yet the shell corners at z=476 -- 26.30190 at x=376 and 26.17619 at x=392 --
	// interpolate ABOVE those door samples (rising with +Z). Eroded terrain is not
	// locally linear and neither extrapolation is evidence.
	// What IS bounded is the consequence: the local relief over half a metre here
	// is a couple of centimetres at most, so the worst case is a ~cm-scale gap or
	// embed under a 3 m TRANSITIONAL greybox pier that the player never reaches --
	// the warp sensor fires 2 m short of it. If these blockouts are ever replaced
	// by real art that a player can walk up to and look down at, re-derive this:
	// either give the terrain a height-query API, or give the raycast a multi-body
	// ignore, and then sample each jamb on its own column.
	//
	// The ORACLE's claim is unaffected and remains exact: it verifies each compiled
	// row against the ground at the column that row NAMES, which is this one.
	constexpr float fZM_HOME_DOOR_SAMPLE_OFFSET = 0.5f;
	constexpr float fZM_HOME_DOOR_SAMPLE_Z = fZM_DAWNMERE_HOME_ENTRANCE_Z
		+ fZM_HOME_DOOR_SAMPLE_OFFSET * fZM_HOME_ENTRANCE_OUTWARD;

	// ==== ZM-D-173 MEASURED HOME GROUND ====
	// ONE VALUE PER LINE, column named in the row. Each is the `measured=` figure
	// ZM_DawnmereHomeGroundTruth_Test (Tests/ZM_AutoTests_CameraClearance.cpp)
	// logged for that column: a REAL downward raycast at that XZ against the baked
	// Dawnmere terrain body, taken headless on the Null backend on 2026-07-31,
	// AFTER the forced heightmap regeneration that moved the Home pad. They are
	// OBSERVED, never derived.
	//
	// ★ WHAT RE-MEASURES THEM: any change to the Dawnmere terrain recipe -- a pad,
	// a path, a seed, a flatten radius. Run that test, read its `MEASURED FEET Y`
	// lines, paste them here, rebuild, and re-author Dawnmere from a windowed tools
	// boot. The derived authored Y values below follow automatically.
	constexpr ZM_DawnmereNpcAnchor s_axDawnmereHomeSamples[] =
	{
		// name,                x,                              z,                              measured feet Y
		{ "HomeShell_MinXMinZ", fZM_HOME_SHELL_MIN_X,           fZM_HOME_SHELL_MIN_Z,           26.30190f },
		{ "HomeShell_MaxXMinZ", fZM_HOME_SHELL_MAX_X,           fZM_HOME_SHELL_MIN_Z,           26.17619f },
		{ "HomeShell_MinXMaxZ", fZM_HOME_SHELL_MIN_X,           fZM_HOME_SHELL_MAX_Z,           26.31600f },
		{ "HomeShell_MaxXMaxZ", fZM_HOME_SHELL_MAX_X,           fZM_HOME_SHELL_MAX_Z,           26.32870f },
		{ "HomeDoorLeft",       fZM_DAWNMERE_HOME_DOOR_LEFT_X,  fZM_HOME_DOOR_SAMPLE_Z,         26.22815f },
		{ "HomeDoorRight",      fZM_DAWNMERE_HOME_DOOR_RIGHT_X, fZM_HOME_DOOR_SAMPLE_Z,         26.21636f },
		{ "HomeDoorTrigger",    fZM_DAWNMERE_HOME_X,            fZM_DAWNMERE_HOME_TRIGGER_Z,    26.29139f },
		{ "FromHomeSpawn",      fZM_DAWNMERE_HOME_X,            fZM_DAWNMERE_FROM_HOME_SPAWN_Z, 26.07615f },
		{ "HomeDoorStaging",    fZM_DAWNMERE_HOME_X,            fZM_DAWNMERE_HOME_DOOR_STAGING_Z, 26.25094f },
		// The anchor row, so the oracle re-checks the town centre on the same run.
		{ "TownCenter",         fZM_DAWNMERE_TOWN_CENTER_X,     fZM_DAWNMERE_TOWN_CENTER_Z,     fZM_DAWNMERE_TOWN_CENTER_FEET_Y },
	};
	// ==== END ZM-D-173 MEASURED HOME GROUND ====

	static_assert(
		sizeof(s_axDawnmereHomeSamples) / sizeof(s_axDawnmereHomeSamples[0])
			== ZM_DAWNMERE_HOME_SAMPLE_COUNT,
		"the Home ground-sample table must have exactly one row per ZM_DAWNMERE_HOME_SAMPLE");

	// The FIXED derivation constants. Spelled once, named, and consumed by both
	// the accessors below and the boot units that check them.
	constexpr float fZM_HOME_SHELL_HALF_Y =
		fZM_DAWNMERE_HOME_SHELL_SCALE_Y * 0.5f;
	constexpr float fZM_HOME_SHELL_EMBED = 0.05f;
	constexpr float fZM_HOME_DOOR_HALF_Y =
		fZM_DAWNMERE_HOME_DOOR_SCALE_Y * 0.5f;
	// The lintel's underside sits on top of a full-height door jamb, so its CENTRE
	// is a door height plus its own half-thickness above the ground.
	constexpr float fZM_HOME_LINTEL_RISE =
		fZM_DAWNMERE_HOME_DOOR_SCALE_Y + fZM_DAWNMERE_HOME_LINTEL_SCALE_Y * 0.5f;
	constexpr float fZM_HOME_TRIGGER_HALF_Y =
		fZM_DAWNMERE_HOME_TRIGGER_SCALE_Y * 0.5f;

	constexpr float ZM_HomeSampleFeetY(u_int uSample)
	{
		return s_axDawnmereHomeSamples[uSample].m_fFeetY;
	}

	constexpr float ZM_MinFloat(float fA, float fB) { return fA < fB ? fA : fB; }
	constexpr float ZM_MaxFloat(float fA, float fB) { return fA > fB ? fA : fB; }

	// The LOWEST of the four footprint corners: derive off the maximum and one
	// corner of the box hangs in the air.
	constexpr float fZM_HOME_SHELL_GROUND = ZM_MinFloat(
		ZM_MinFloat(ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_SHELL_MINX_MINZ),
			ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_SHELL_MAXX_MINZ)),
		ZM_MinFloat(ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_SHELL_MINX_MAXZ),
			ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_SHELL_MAXX_MAXZ)));

	constexpr float fZM_HOME_SHELL_CENTER_Y =
		fZM_HOME_SHELL_GROUND + fZM_HOME_SHELL_HALF_Y - fZM_HOME_SHELL_EMBED;
	constexpr float fZM_HOME_DOOR_LEFT_CENTER_Y =
		ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_DOOR_LEFT) + fZM_HOME_DOOR_HALF_Y;
	constexpr float fZM_HOME_DOOR_RIGHT_CENTER_Y =
		ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_DOOR_RIGHT) + fZM_HOME_DOOR_HALF_Y;
	// The HIGHER of the two door grounds, so the lintel clears both jambs.
	constexpr float fZM_HOME_LINTEL_CENTER_Y =
		ZM_MaxFloat(ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_DOOR_LEFT),
			ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_DOOR_RIGHT))
		+ fZM_HOME_LINTEL_RISE;
	constexpr float fZM_HOME_TRIGGER_CENTER_Y =
		ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_TRIGGER) + fZM_HOME_TRIGGER_HALF_Y;

	// ★ THE BYTE-NEUTRALITY ASSERTS THAT USED TO LIVE HERE ARE GONE, DELIBERATELY
	// AND ON SCHEDULE. Four static_asserts pinned the derived centres to the
	// literals the pre-ZM-D-173 Zenithmon.cpp spelled, so that HOISTING those
	// literals into a table could be shown to move no scene byte. They earned
	// their place -- one of them caught a hand back-derivation that was a full ULP
	// wrong -- and the property they guarded was then confirmed on the artefact
	// itself: two authoring boots wrote a Dawnmere.zscen byte-identical to the
	// committed one. This commit MOVES the Home, so keeping asserts that demand
	// the OLD values would be asserting the change did not happen.
	//
	// What replaces them is stronger and lives outside this file:
	// ZM_DawnmereHomeGroundTruth_Test re-measures every row above against the real
	// heightfield on every run, and ZM_DawnmereCameraClearance_Test checks the
	// resulting geometry against the shipped camera in the real physics world.
}

u_int ZM_GetDawnmereHomeSampleCount()
{
	return (u_int)ZM_DAWNMERE_HOME_SAMPLE_COUNT;
}

const ZM_DawnmereNpcAnchor& ZM_GetDawnmereHomeSample(u_int uSample)
{
	if (uSample >= (u_int)ZM_DAWNMERE_HOME_SAMPLE_COUNT)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_DawnmerePlacement] ZM_GetDawnmereHomeSample: id %u is not a Home "
			"ground sample -- returning the UNKNOWN town-centre anchor", uSample);
		return s_xInvalidDawnmereAnchor;
	}
	return s_axDawnmereHomeSamples[uSample];
}

float ZM_DawnmereHomeSampleFeetY(u_int uSample)
{
	return ZM_GetDawnmereHomeSample(uSample).m_fFeetY;
}

ZM_DawnmereBlockout ZM_GetDawnmereHomeShell()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_X, fZM_HOME_SHELL_CENTER_Y,
			fZM_DAWNMERE_HOME_SHELL_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_SHELL_SCALE_X,
			fZM_DAWNMERE_HOME_SHELL_SCALE_Y, fZM_DAWNMERE_HOME_SHELL_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorLeft()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_DOOR_LEFT_X,
			fZM_HOME_DOOR_LEFT_CENTER_Y, fZM_DAWNMERE_HOME_ENTRANCE_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_DOOR_SCALE_X,
			fZM_DAWNMERE_HOME_DOOR_SCALE_Y, fZM_DAWNMERE_HOME_DOOR_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorRight()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_DOOR_RIGHT_X,
			fZM_HOME_DOOR_RIGHT_CENTER_Y, fZM_DAWNMERE_HOME_ENTRANCE_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_DOOR_SCALE_X,
			fZM_DAWNMERE_HOME_DOOR_SCALE_Y, fZM_DAWNMERE_HOME_DOOR_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorLintel()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_X, fZM_HOME_LINTEL_CENTER_Y,
			fZM_DAWNMERE_HOME_ENTRANCE_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_LINTEL_SCALE_X,
			fZM_DAWNMERE_HOME_LINTEL_SCALE_Y, fZM_DAWNMERE_HOME_LINTEL_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorTrigger()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_X, fZM_HOME_TRIGGER_CENTER_Y,
			fZM_DAWNMERE_HOME_TRIGGER_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_TRIGGER_SCALE_X,
			fZM_DAWNMERE_HOME_TRIGGER_SCALE_Y, fZM_DAWNMERE_HOME_TRIGGER_SCALE_Z)
	};
}

Zenith_Maths::Vector3 ZM_GetDawnmereFromHomeSpawnFeet()
{
	return Zenith_Maths::Vector3(fZM_DAWNMERE_HOME_X,
		ZM_HomeSampleFeetY(ZM_DAWNMERE_HOME_SAMPLE_SPAWN),
		fZM_DAWNMERE_FROM_HOME_SPAWN_Z);
}

Zenith_Maths::Vector3 ZM_GetDawnmereHomeDoorStagingXZ()
{
	return Zenith_Maths::Vector3(
		fZM_DAWNMERE_HOME_X, 0.0f, fZM_DAWNMERE_HOME_DOOR_STAGING_Z);
}

Zenith_Maths::Vector3 ZM_GetDawnmereHomeDoorTargetXZ()
{
	return Zenith_Maths::Vector3(
		fZM_DAWNMERE_HOME_X, 0.0f, fZM_DAWNMERE_HOME_DOOR_TARGET_Z);
}
