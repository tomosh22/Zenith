#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"

#include <bit>     // bit_cast -- the frozen facing (ZM-D-183)
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
	// ZM-D-183: FROZEN BITS, NOT AngleAxis(ZM_DawnmereVesperYaw()). The header
	// carries the full argument; the one-line version is that atan2 and sin/cos
	// disagree by 1-2 ULP between Debug and Release codegen, so the computed form
	// authored two different scene files depending on which tools build ran.
	//
	// std::bit_cast rather than a decimal literal: a float literal has to be
	// written to 9 significant digits to round-trip, and a reader cannot verify by
	// eye that it did. These ARE the bytes, and the static_asserts below prove the
	// round trip in both directions at compile time.
	static_assert(std::bit_cast<u_int>(
		std::bit_cast<float>(uZM_DAWNMERE_VESPER_FACING_Y_BITS)) ==
		uZM_DAWNMERE_VESPER_FACING_Y_BITS,
		"the frozen y component does not round-trip through float");
	static_assert(std::bit_cast<u_int>(
		std::bit_cast<float>(uZM_DAWNMERE_VESPER_FACING_W_BITS)) ==
		uZM_DAWNMERE_VESPER_FACING_W_BITS,
		"the frozen w component does not round-trip through float");

	// glm::quat's constructor is (w, x, y, z).
	return Zenith_Maths::Quat(
		std::bit_cast<float>(uZM_DAWNMERE_VESPER_FACING_W_BITS),
		std::bit_cast<float>(uZM_DAWNMERE_VESPER_FACING_X_BITS),
		std::bit_cast<float>(uZM_DAWNMERE_VESPER_FACING_Y_BITS),
		std::bit_cast<float>(uZM_DAWNMERE_VESPER_FACING_Z_BITS));
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

float ZM_DawnmereTrainerSpawnY(float fCapsuleHalfExtent)
{
	// Same shape as the wanderer's below, and deliberately so -- see the header for
	// the measurement that made the rival need it too (ZM-D-184).
	const float fSanitised = ZM_SanitiseCapsuleHalfExtent(fCapsuleHalfExtent);
	return ZM_DawnmereNpcCentreY(ZM_DAWNMERE_NPC_RIVAL_VESPER, fSanitised) + fSanitised;
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
	// bracket the doorway do not agree on even the SIGN: (384, 474) reads 26.28675
	// while the two door columns at z=475.5 read 26.22059/26.19279 (falling with +Z),
	// yet the shell corners at z=476 -- 26.30927 at x=376 and 26.22234 at x=392 --
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
	// Dawnmere terrain body, taken headless on the Null backend on 2026-08-02,
	// AFTER its collision topology changed from 8 m to 4 m quads. They are
	// OBSERVED, never derived.
	//
	// ★ WHAT RE-MEASURES THEM: any change to the Dawnmere terrain recipe OR its
	// collision topology -- a pad, path, seed, flatten radius, or density divisor.
	// Run that test, read its `MEASURED FEET Y`
	// lines, paste them here, rebuild, and re-author Dawnmere from a windowed tools
	// boot. The derived authored Y values below follow automatically.
	constexpr ZM_DawnmereNpcAnchor s_axDawnmereHomeSamples[] =
	{
		// name,                x,                              z,                              measured feet Y
		{ "HomeShell_MinXMinZ", fZM_HOME_SHELL_MIN_X,           fZM_HOME_SHELL_MIN_Z,           26.30927f },
		{ "HomeShell_MaxXMinZ", fZM_HOME_SHELL_MAX_X,           fZM_HOME_SHELL_MIN_Z,           26.22234f },
		{ "HomeShell_MinXMaxZ", fZM_HOME_SHELL_MIN_X,           fZM_HOME_SHELL_MAX_Z,           25.61314f },
		{ "HomeShell_MaxXMaxZ", fZM_HOME_SHELL_MAX_X,           fZM_HOME_SHELL_MAX_Z,           25.59459f },
		{ "HomeDoorLeft",       fZM_DAWNMERE_HOME_DOOR_LEFT_X,  fZM_HOME_DOOR_SAMPLE_Z,         26.22059f },
		{ "HomeDoorRight",      fZM_DAWNMERE_HOME_DOOR_RIGHT_X, fZM_HOME_DOOR_SAMPLE_Z,         26.19279f },
		{ "HomeDoorTrigger",    fZM_DAWNMERE_HOME_X,            fZM_DAWNMERE_HOME_TRIGGER_Z,    26.28675f },
		{ "FromHomeSpawn",      fZM_DAWNMERE_HOME_X,            fZM_DAWNMERE_FROM_HOME_SPAWN_Z, 26.54212f },
		{ "HomeDoorStaging",    fZM_DAWNMERE_HOME_X,            fZM_DAWNMERE_HOME_DOOR_STAGING_Z, 26.48395f },
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

// ============================================================================
// S8 SC-D -- the lab site. See the header for the reserved terrain site, the
// camera derivation behind the entrance plane, the fixed Y formulas, and the
// freeze procedure for the PLACEHOLDER table below.
// ============================================================================

namespace
{
	constexpr float fZM_LAB_SHELL_HALF_X =
		fZM_DAWNMERE_LAB_SHELL_SCALE_X * 0.5f;
	constexpr float fZM_LAB_SHELL_HALF_Z =
		fZM_DAWNMERE_LAB_SHELL_SCALE_Z * 0.5f;
	constexpr float fZM_LAB_SHELL_MIN_X = fZM_DAWNMERE_LAB_X - fZM_LAB_SHELL_HALF_X;
	constexpr float fZM_LAB_SHELL_MAX_X = fZM_DAWNMERE_LAB_X + fZM_LAB_SHELL_HALF_X;
	constexpr float fZM_LAB_SHELL_MIN_Z =
		fZM_DAWNMERE_LAB_SHELL_Z - fZM_LAB_SHELL_HALF_Z;
	constexpr float fZM_LAB_SHELL_MAX_Z =
		fZM_DAWNMERE_LAB_SHELL_Z + fZM_LAB_SHELL_HALF_Z;

	// Which way is OUT of the lab: -1 when the entrance is the shell's MIN-Z
	// face, +1 when it is the MAX-Z face. Derived rather than assumed so the door
	// ground samples follow the entrance if it is ever moved to the other face
	// (which the ZM-D-173 camera rule forbids, but a derivation costs nothing and
	// a silent wrong-side sample would be unreadable).
	constexpr float fZM_LAB_ENTRANCE_OUTWARD =
		fZM_DAWNMERE_LAB_ENTRANCE_Z < fZM_DAWNMERE_LAB_SHELL_Z ? -1.0f : 1.0f;

	// ★ THE TWO DOOR COLUMNS ARE SAMPLED HALF A METRE OUT INTO THE FORECOURT, AND
	// THE REASON IS ENTIRELY ABOUT THE **SECOND** RUN OF THE ORACLE, NOT THIS ONE.
	//
	// TODAY (SC-D) the offset buys nothing measurable: Dawnmere contains no lab
	// geometry at all, so a probe on a jamb's exact column would find clean ground.
	// The offset is here so that the SAME columns stay measurable AFTER SC-E
	// authors the shell and the jambs, because at that point a jamb's own column
	// has TWO solid bodies over it and neither can be stepped past:
	//   * the jamb's underside is exactly AT the surface being measured, so
	//     restarting the ray below it restarts ON the ground;
	//   * the shell is EMBEDDED 0.05 m below its lowest corner, which at this
	//     column puts its underside BELOW the ground -- restarting below THAT
	//     starts underneath the terrain and misses entirely.
	// Zenith_PhysicsQuery::RaycastIgnoring takes ONE entity, so the pair cannot be
	// filtered out either (that missing multi-body ignore is booked as Shortfalls
	// E8, alongside the absent terrain height-query API). If these rows were
	// sampled on the jamb columns now, the post-SC-E re-measure would be
	// STRUCTURALLY IMPOSSIBLE and the rows would have to MOVE -- which would
	// invalidate the very freeze this table exists to hold.
	//
	// ★ AND THE RESIDUAL IS HONESTLY UNKNOWN, WHICH IS NOT THE SAME AS SMALL. Each
	// jamb's authored height comes from ground half a metre away, and the ground at
	// its own column is unmeasurable by construction once the building exists. Do
	// not quote a figure for it: the Home block records that the samples bracketing
	// ITS doorway do not agree on even the SIGN of the local gradient, because
	// eroded terrain is not locally linear. What is bounded is the consequence -- a
	// centimetre-scale gap or embed under a greybox pier the player never reaches,
	// since the warp sensor fires 2 m short of it.
	//
	// Half a metre clears the jamb's 0.25 m protrusion and the shell face, while
	// staying deep inside the Lab pad's 48 m flatten radius.
	constexpr float fZM_LAB_DOOR_SAMPLE_OFFSET = 0.5f;
	constexpr float fZM_LAB_DOOR_SAMPLE_Z = fZM_DAWNMERE_LAB_ENTRANCE_Z
		+ fZM_LAB_DOOR_SAMPLE_OFFSET * fZM_LAB_ENTRANCE_OUTWARD;

	// ==== S8 SC-D MEASURED LAB GROUND -- FROZEN 2026-08-14 ====
	//
	// ★★ THESE TEN LITERALS ARE MEASUREMENTS, NOT DESIGN VALUES. Each is the
	// `paste=` literal `ZM_DawnmereLabGroundTruth_Test` printed for that row NAME on
	// a WINDOWED warm-terrain boot -- a real downward raycast at that column against
	// the baked Dawnmere terrain body. Every row resolved with
	// `finalHit='DawnmereTerrain'` in mode `PRE-SC-E (labShellPresent=0)`, i.e. pure
	// terrain with no lab geometry to ignore, which is the whole reason this slice
	// runs BEFORE the one that authors the building.
	//
	// ★ DO NOT HAND-EDIT A ROW TO MAKE A TEST PASS. If the oracle reds, the terrain
	// moved under the site and the correct response is to RE-RUN it and re-paste, as
	// a set. The header documents the full re-measure procedure and what invalidates
	// the table. Hand-tuning one row is how a spread guard ends up guarding nothing.
	//
	// ★ THE POST-SC-E RE-RUN MEASURES THE SAME COLUMNS WITH THE SHELL AND JAMBS
	// IGNORED. The door rows sit 0.5 m out into the forecourt for exactly that run:
	// a probe on a jamb's own column has two solid bodies over it. That offset buys
	// nothing today and is not a mistake.
	//
	// ONE VALUE PER LINE, column named in the row, exactly like the ZM-D-173 Home
	// table above -- so a re-measure is a per-row replacement rather than a rewrite.
	constexpr ZM_DawnmereNpcAnchor s_axDawnmereLabSamples[] =
	{
		// name,                x,                             z,                            measured feet Y
		{ "LabShell_MinXMinZ",  fZM_LAB_SHELL_MIN_X,           fZM_LAB_SHELL_MIN_Z,          25.88701f },
		{ "LabShell_MaxXMinZ",  fZM_LAB_SHELL_MAX_X,           fZM_LAB_SHELL_MIN_Z,          25.74946f },
		{ "LabShell_MinXMaxZ",  fZM_LAB_SHELL_MIN_X,           fZM_LAB_SHELL_MAX_Z,          24.37600f },
		{ "LabShell_MaxXMaxZ",  fZM_LAB_SHELL_MAX_X,           fZM_LAB_SHELL_MAX_Z,          25.17580f },
		{ "LabDoorLeft",        fZM_DAWNMERE_LAB_DOOR_LEFT_X,  fZM_LAB_DOOR_SAMPLE_Z,        25.78080f },
		{ "LabDoorRight",       fZM_DAWNMERE_LAB_DOOR_RIGHT_X, fZM_LAB_DOOR_SAMPLE_Z,        25.72527f },
		{ "LabDoorTrigger",     fZM_DAWNMERE_LAB_X,            fZM_DAWNMERE_LAB_TRIGGER_Z,   25.81471f },
		{ "FromLabSpawn",       fZM_DAWNMERE_LAB_X,            fZM_DAWNMERE_FROM_LAB_SPAWN_Z, 26.03916f },
		{ "LabDoorStaging",     fZM_DAWNMERE_LAB_X,            fZM_DAWNMERE_LAB_DOOR_STAGING_Z, 25.95101f },
		// The reserved pad's own centre, so the oracle states the site's reference
		// height on the same run it measures everything derived from it.
		{ "LabPadCenter",       fZM_DAWNMERE_LAB_X,            fZM_DAWNMERE_LAB_PAD_CENTER_Z, 24.32298f },
	};
	// ==== END S8 SC-D MEASURED LAB GROUND ====

	// The bound is DEDUCED, never spelled, for the reason the NPC table gives: an
	// explicit [ZM_DAWNMERE_LAB_SAMPLE_COUNT] would make this a tautology and let a
	// forgotten row zero-initialise into a NULL-named anchor at the world origin.
	static_assert(
		sizeof(s_axDawnmereLabSamples) / sizeof(s_axDawnmereLabSamples[0])
			== ZM_DAWNMERE_LAB_SAMPLE_COUNT,
		"the lab ground-sample table must have exactly one row per ZM_DAWNMERE_LAB_SAMPLE");

	// The FIXED derivation constants, in the same order and with the same meanings
	// as the Home block's. Spelled once, named, and consumed by both the accessors
	// below and the boot unit that checks them.
	constexpr float fZM_LAB_SHELL_HALF_Y =
		fZM_DAWNMERE_LAB_SHELL_SCALE_Y * 0.5f;
	constexpr float fZM_LAB_SHELL_EMBED = 0.05f;
	constexpr float fZM_LAB_DOOR_HALF_Y =
		fZM_DAWNMERE_LAB_DOOR_SCALE_Y * 0.5f;
	// The lintel's underside sits on top of a full-height jamb, so its CENTRE is a
	// jamb height plus its own half-thickness above the ground.
	constexpr float fZM_LAB_LINTEL_RISE =
		fZM_DAWNMERE_LAB_DOOR_SCALE_Y + fZM_DAWNMERE_LAB_LINTEL_SCALE_Y * 0.5f;
	constexpr float fZM_LAB_TRIGGER_HALF_Y =
		fZM_DAWNMERE_LAB_TRIGGER_SCALE_Y * 0.5f;

	constexpr float ZM_LabSampleFeetY(u_int uSample)
	{
		return s_axDawnmereLabSamples[uSample].m_fFeetY;
	}

	// The LOWEST of the four footprint corners: derive off the maximum and one
	// corner of a 21 x 17 m box hangs in the air.
	constexpr float fZM_LAB_SHELL_GROUND = ZM_MinFloat(
		ZM_MinFloat(ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_SHELL_MINX_MINZ),
			ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MINZ)),
		ZM_MinFloat(ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_SHELL_MINX_MAXZ),
			ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MAXZ)));

	constexpr float fZM_LAB_SHELL_CENTER_Y =
		fZM_LAB_SHELL_GROUND + fZM_LAB_SHELL_HALF_Y - fZM_LAB_SHELL_EMBED;
	constexpr float fZM_LAB_DOOR_LEFT_CENTER_Y =
		ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_DOOR_LEFT) + fZM_LAB_DOOR_HALF_Y;
	constexpr float fZM_LAB_DOOR_RIGHT_CENTER_Y =
		ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_DOOR_RIGHT) + fZM_LAB_DOOR_HALF_Y;
	// The HIGHER of the two door grounds, so the lintel clears both jambs.
	constexpr float fZM_LAB_LINTEL_CENTER_Y =
		ZM_MaxFloat(ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_DOOR_LEFT),
			ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_DOOR_RIGHT))
		+ fZM_LAB_LINTEL_RISE;
	constexpr float fZM_LAB_TRIGGER_CENTER_Y =
		ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_TRIGGER) + fZM_LAB_TRIGGER_HALF_Y;
}

u_int ZM_GetDawnmereLabSampleCount()
{
	return (u_int)ZM_DAWNMERE_LAB_SAMPLE_COUNT;
}

const ZM_DawnmereNpcAnchor& ZM_GetDawnmereLabSample(u_int uSample)
{
	if (uSample >= (u_int)ZM_DAWNMERE_LAB_SAMPLE_COUNT)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_DawnmerePlacement] ZM_GetDawnmereLabSample: id %u is not a lab "
			"ground sample -- returning the UNKNOWN town-centre anchor", uSample);
		return s_xInvalidDawnmereAnchor;
	}
	return s_axDawnmereLabSamples[uSample];
}

float ZM_DawnmereLabSampleFeetY(u_int uSample)
{
	return ZM_GetDawnmereLabSample(uSample).m_fFeetY;
}

ZM_DawnmereBlockout ZM_GetDawnmereLabShell()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_X, fZM_LAB_SHELL_CENTER_Y,
			fZM_DAWNMERE_LAB_SHELL_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_SHELL_SCALE_X,
			fZM_DAWNMERE_LAB_SHELL_SCALE_Y, fZM_DAWNMERE_LAB_SHELL_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereLabDoorLeft()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_DOOR_LEFT_X,
			fZM_LAB_DOOR_LEFT_CENTER_Y, fZM_DAWNMERE_LAB_ENTRANCE_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_DOOR_SCALE_X,
			fZM_DAWNMERE_LAB_DOOR_SCALE_Y, fZM_DAWNMERE_LAB_DOOR_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereLabDoorRight()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_DOOR_RIGHT_X,
			fZM_LAB_DOOR_RIGHT_CENTER_Y, fZM_DAWNMERE_LAB_ENTRANCE_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_DOOR_SCALE_X,
			fZM_DAWNMERE_LAB_DOOR_SCALE_Y, fZM_DAWNMERE_LAB_DOOR_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereLabDoorLintel()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_X, fZM_LAB_LINTEL_CENTER_Y,
			fZM_DAWNMERE_LAB_ENTRANCE_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_LINTEL_SCALE_X,
			fZM_DAWNMERE_LAB_LINTEL_SCALE_Y, fZM_DAWNMERE_LAB_LINTEL_SCALE_Z)
	};
}

ZM_DawnmereBlockout ZM_GetDawnmereLabDoorTrigger()
{
	return {
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_X, fZM_LAB_TRIGGER_CENTER_Y,
			fZM_DAWNMERE_LAB_TRIGGER_Z),
		Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_TRIGGER_SCALE_X,
			fZM_DAWNMERE_LAB_TRIGGER_SCALE_Y, fZM_DAWNMERE_LAB_TRIGGER_SCALE_Z)
	};
}

Zenith_Maths::Vector3 ZM_GetDawnmereFromLabSpawnFeet()
{
	return Zenith_Maths::Vector3(fZM_DAWNMERE_LAB_X,
		ZM_LabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_SPAWN),
		fZM_DAWNMERE_FROM_LAB_SPAWN_Z);
}

Zenith_Maths::Vector3 ZM_GetDawnmereLabDoorStagingXZ()
{
	return Zenith_Maths::Vector3(
		fZM_DAWNMERE_LAB_X, 0.0f, fZM_DAWNMERE_LAB_DOOR_STAGING_Z);
}

Zenith_Maths::Vector3 ZM_GetDawnmereLabDoorTargetXZ()
{
	return Zenith_Maths::Vector3(
		fZM_DAWNMERE_LAB_X, 0.0f, fZM_DAWNMERE_LAB_DOOR_TARGET_Z);
}

// ============================================================================
// R1-2 STEP 1 -- the Route 1 arrival seam. See the header for the reserved
// landmark, the corridor arithmetic that predicts this column is levelled
// ground, the expected band, and the freeze procedure for the sentinel row
// below.
//
// ★ THIS TABLE IS NOW AUTHORED FROM (R1-2 step 3, ZM-D-202). The FromRoute1 row
// is read by ZM_GetDawnmereFromRoute1SpawnFeet() and lands in the committed
// Dawnmere as the arrival marker's position, so a re-measurement here MOVES
// SCENE BYTES and owes a windowed re-author. The two sentences that used to sit
// here -- "NOTHING is authored from this table yet" and "R1-2 step 2 is what
// adds the marker" -- were both true when written and are both false now; it
// was step 3, and it has landed.
//
// ★ ZM-65 ADDED A SECOND ROW, DawnmereNorthGate, BELOW THE FIRST -- closing
// ZM-D-203 §5's scoped deviation (the north seam gate's centre Y used to borrow
// the FromRoute1 row's measurement instead of having its own; see the R1-3
// block in the header for the corrected cost reasoning). It was authored
// UNMEASURED and FROZEN at 24.29772 in the same commit, and Dawnmere was
// re-authored from a windowed boot -- so BOTH rows are frozen and the deviation
// is CLOSED.
// ============================================================================

namespace
{
	// ==== R1-2 / ZM-65 MEASURED ROUTE SEAM GROUND -- BOTH ROWS FROZEN ====
	//
	// ★★ NEITHER ROW IS A PLACEHOLDER ANY MORE. Each was authored carrying
	// fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED -- which is NOT a height and is
	// nowhere near one -- and each was then frozen from a real raycast:
	// FromRoute1Spawn on 2026-08-15, DawnmereNorthGate on 2026-08-24.
	// ZM_Interaction/RouteSeamGround_EachRowStandsOnItsOwnAnchorAndIsMeasured is
	// GREEN, and it hard-reds if EITHER row ever returns to the sentinel.
	//
	// The freeze procedure below is kept because it is what a RE-measure follows
	// (a recipe, seed, flatten-radius or collision-density change re-measures
	// every table in this file), not because anything here is currently unfrozen.
	//
	// TO FREEZE: run ZM_DawnmereRouteSeamGroundTruth_Test
	// (Tests/ZM_AutoTests_CameraClearance.cpp) against a warm Dawnmere terrain
	// bake, take the `paste=` literal it prints for the row NAME below, put it in
	// place of the sentinel, and rebuild. It is a real downward raycast at this
	// column against the baked Dawnmere terrain body -- OBSERVED, never derived,
	// and never arithmetic off another table's row.
	//
	// ★ DO NOT HAND-EDIT THIS ROW TO MAKE A TEST PASS. If the oracle reds after a
	// freeze, the terrain moved under the seam and the correct response is to
	// RE-RUN it and re-paste. Hand-tuning is how a band guard ends up guarding
	// nothing.
	//
	// ONE VALUE PER LINE, column named in the row, in the same idiom as the two
	// tables above -- so a re-measure is a per-row replacement rather than a
	// rewrite.
	constexpr ZM_DawnmereNpcAnchor s_axDawnmereRouteSeamSamples[] =
	{
		// name,               x,                          z,                          measured feet Y
		// FROZEN 2026-08-15 from ZM_DawnmereRouteSeamGroundTruth_Test against a warm
		// Dawnmere bake: hitTerrain=1, finalHit='DawnmereTerrain', playerPresent=1
		// (the capsule was found and correctly IGNORED by the probe).
		//
		// ★★ 24.366 IS NOT THE ~25.6-26.5 THIS ROW WAS PREDICTED TO READ, AND THE
		// PREDICTION WAS WRONG FOR A REASON WORTH KEEPING. It was extrapolated from
		// Dawnmere's OTHER measured columns (town centre 25.99, the Home and Lab
		// rows 25.59-26.54), which all sit ~+2 m ABOVE the recipe's 24.0 m target.
		// This column does not, because it is the first measured Dawnmere column
		// INSIDE A FLATTEN CORRIDOR: it lies 4.56 m from the "Route" path polyline
		// against an 18 m flatten radius, and a FLATTEN dab drives ground TO the
		// target. So it reads target + 0.366, while the unflattened columns carry
		// the hydraulic-erosion deposit pass on top.
		//
		// ★ THAT ALSO CORRECTS AN EXPECTATION FOR THE REST OF R1-2. The spec warned
		// that Route1's and Thornacre's provisional constants (their recipe targets,
		// 26.0 and 28.0) would measure "metres, not ULPs" off. This measurement says
		// that gap is a property of UNFLATTENED ground -- and every Route1/Thornacre
		// arrival anchor sits on a flattened pad or lane. Expect those to come back
		// NEAR their targets too. Do not treat a near-target measurement there as a
		// broken probe.
		{ "FromRoute1Spawn",   fZM_DAWNMERE_FROM_ROUTE1_X, fZM_DAWNMERE_FROM_ROUTE1_Z, 24.36592f },
		// FROZEN 2026-08-24 (ZM-65, closing ZM-D-203 §5 -- see the header) from
		// ZM_DawnmereRouteSeamGroundTruth_Test against a warm Dawnmere bake:
		// hitTerrain=1, finalHit='DawnmereTerrain', resolved=1, playerPresent=1
		// (the capsule was found and correctly IGNORED by the probe). The same
		// run re-read row 0 at tableError=0.00000, so the probe agreed with the
		// 2026-08-15 freeze on the column that had not moved.
		//
		// The north seam gate's OWN column, 12 m north of the row above. X/Z are
		// the gate's existing compiled constants
		// (Source/World/ZM_DawnmerePlacement.h), read here rather than
		// re-spelled; both containments (the "RouteGate" pad's 30 m flatten
		// radius, the "Route" path's 18 m flatten radius) are stated where those
		// constants are declared.
		//
		// ★★ THE DEVIATION'S REAL COST WAS 0.068 m, NOT THE 0.37 m PREDICTED --
		// AND THE PREDICTION'S *SIGN* IS THE HALF THAT MATTERED. 24.29772 is
		// 0.0682 m BELOW row 0's 24.36592, i.e. the borrowed column sat slightly
		// ABOVE this one. That is the favourable sign ZM-D-206 reasoned to, so
		// the derived value never endangered the sensor -- but note it is also
		// INSIDE this oracle's own 0.15 m tolerance, which is why no test could
		// ever have caught the deviation by watching the number. It was only ever
		// visible as a MISSING ROW, which is the whole argument of ZM-D-203
		// Decision 1.
		//
		// ★ IT IS ALSO AN ORDER OF MAGNITUDE UNDER THE OTHER REGIONS' 12 m DELTAS
		// (Thornacre 0.254, Route 1 south 0.475, Route 1 north 0.962), AND THAT
		// IS NOT EXPLAINED. Do not invent a reason for it. The obvious one --
		// "those pairs straddle a flatten boundary and this one does not" -- is
		// FALSE, and the recipe data inverts it: Thornacre's pair sits inside the
		// RouteGate pad {512,96} r=30 (ZM_TerrainAuthoring.cpp; ZM_ThornacrePlacement.h
		// says so in as many words), and both Route 1 pairs sit inside their own
		// r=30 gate pads AND within the DirtLane's r=16 corridor. All three
		// comparison pairs are IDENTICALLY contained on both columns. The pair
		// here is the only one whose two columns differ in containment at all
		// (row 0 in the "Route" corridor alone, row 1 also inside "RouteGate"),
		// and it moved the LEAST.
		//
		// So the honest statement is: four measured 12 m seam pairs span 0.068 to
		// 0.962 m and nothing in this repo currently accounts for the spread.
		// ★ THEREFORE DO NOT GENERALISE 0.068 INTO "SEAM PAIRS ARE CLOSE" -- the
		// rule stands, on measurement rather than on a mechanism.
		{ "DawnmereNorthGate", fZM_DAWNMERE_NORTH_GATE_X, fZM_DAWNMERE_NORTH_GATE_Z, 24.29772f },
	};
	// ==== END R1-2 / ZM-65 MEASURED ROUTE SEAM GROUND ====

	// The bound is DEDUCED, never spelled, for the reason both tables above give:
	// an explicit [ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_COUNT] would make this a
	// tautology and let a forgotten row zero-initialise into a NULL-named anchor
	// at the world origin.
	static_assert(
		sizeof(s_axDawnmereRouteSeamSamples) / sizeof(s_axDawnmereRouteSeamSamples[0])
			== ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_COUNT,
		"the route-seam ground-sample table must have exactly one row per ZM_DAWNMERE_ROUTE_SEAM_SAMPLE");
}

u_int ZM_GetDawnmereRouteSeamSampleCount()
{
	return (u_int)ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_COUNT;
}

const ZM_DawnmereNpcAnchor& ZM_GetDawnmereRouteSeamSample(u_int uSample)
{
	if (uSample >= (u_int)ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_COUNT)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_DawnmerePlacement] ZM_GetDawnmereRouteSeamSample: id %u is not a "
			"route-seam ground sample -- returning the UNKNOWN town-centre anchor",
			uSample);
		return s_xInvalidDawnmereAnchor;
	}
	return s_axDawnmereRouteSeamSamples[uSample];
}

float ZM_DawnmereRouteSeamSampleFeetY(u_int uSample)
{
	return ZM_GetDawnmereRouteSeamSample(uSample).m_fFeetY;
}
