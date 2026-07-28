#pragma once

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"   // Zenith_EntityID

// ============================================================================
// ZM_TrainerSightProbe (S7 item 3 SC6) -- THE OCCLUSION FILTER. One free
// function, deliberately NOT a member of anything: it takes two world positions
// and two entity ids, so a boot unit can drive the REAL raycast against a
// hermetically created static box with no scene, no player and no component.
//
// It is consulted ONLY AFTER ZM_IsTargetInTrainerSightFromRotation has already
// passed. That ordering is the entire cost control: there is NO raycast budget or
// throttle anywhere in this engine (Zenith/AI/CLAUDE.md's "raycast budget" claim
// is documentation-only and not implemented).
//
// THE SINGLE-BODY-IGNORE CONSTRAINT, and how it is handled. The engine offers
// exactly one filter -- Zenith_PhysicsQuery::RaycastIgnoring's one entity id --
// and no layer/mask. So the TRAINER is filtered by id, and the PLAYER's own
// capsule (which necessarily terminates the ray at the far end, because Jolt
// treats convex shapes as solid) is recognised EXACTLY by comparing
// RaycastResult::m_xHitEntity against the target id. That is exact and needs no
// magic distance tolerance, unlike Zenith_PerceptionSystem::CheckLineOfSight's
// hard-coded 0.5 m.
//
// TERRAIN CAVEAT, binding on every test: Dawnmere's greybox shell, door jambs,
// lintel, NPC AABBs and the warp trigger are real static bodies EVERYWHERE
// (they live in the committed Dawnmere.zscen). The TERRAIN collider is built
// from gitignored, uncommitted Assets/Terrain/Dawnmere/Physics_*.zmesh, so on a
// fresh CI checkout terrain occludes NOTHING. Never assert occlusion against
// terrain -- assert it against an explicitly created box.
//
// TOTAL. Never calls Zenith_Assert.
// ============================================================================

struct ZM_TrainerSightProbeResult
{
	// The answer the FSM consumes.
	bool            m_bClear            = false;
	// False when there is no live simulation. Exposed so a test can PROVE a
	// "clear" answer did not come from the fail-open branch below -- without it
	// every occlusion assertion would be vacuous in a physics-less context.
	bool            m_bPhysicsAvailable = false;
	// A body OTHER than the target stopped the ray.
	bool            m_bBlockerHit       = false;
	Zenith_EntityID m_xBlockerEntityID  = INVALID_ENTITY_ID;
	float           m_fBlockerDistance  = 0.0f;
};

// ANSWER TABLE, in evaluation order (the order IS the specification):
//   1. Any non-finite position component      -> m_bClear = FALSE. FAIL CLOSED,
//      matching ZM_IsTargetInTrainerSight's own totality rule: one body that goes
//      non-finite must not hand every trainer a free line of sight.
//   2. Coincident (separation^2 <= fZM_INTERACT_DEGENERATE_LEN_SQ, the ONE
//      degenerate epsilon this game has) -> m_bClear = TRUE, no ray cast. There
//      is nothing between two points at the same place, and this matches the
//      cone predicate's own coincident carve-out.
//   3. !Zenith_Physics::HasActiveSimulation() -> m_bPhysicsAvailable = FALSE and
//      m_bClear = TRUE. FAIL OPEN, deliberately: a world with no physics has no
//      occluders, and failing closed would blind every trainer in every
//      physics-less context. This is the ONE place the two polarities differ, and
//      it is why m_bPhysicsAvailable is reported.
//   4. No hit                                  -> m_bClear = TRUE.
//   5. Hit whose m_xHitEntity == xTargetEntityID (and the id is valid)
//                                              -> m_bClear = TRUE (that is the
//      target's own body, not an occluder).
//   6. Otherwise -> m_bClear = FALSE, m_bBlockerHit = TRUE, blocker id + distance
//      recorded for the failure message.
ZM_TrainerSightProbeResult ZM_ProbeTrainerSightLine(
	const Zenith_Maths::Vector3& xTrainerPosition,
	Zenith_EntityID xTrainerEntityID,
	const Zenith_Maths::Vector3& xTargetPosition,
	Zenith_EntityID xTargetEntityID);
