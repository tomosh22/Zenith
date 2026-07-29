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
