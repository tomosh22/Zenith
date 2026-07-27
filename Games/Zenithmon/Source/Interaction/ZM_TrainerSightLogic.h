#pragma once

#include "Maths/Zenith_Maths.h"                                // Zenith_Maths::Vector3 / Quat
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // ZM_FlattenXZ / ZM_IsFacingXZ / ZM_ForwardFromRotation

// ============================================================================
// ZM_TrainerSightLogic (S7 item 3 SC3) -- the PURE "can this trainer see the
// player from where they are standing and which way they are facing?" predicate.
// Free functions over plain maths types: NO ECS, NO scene, NO physics, NO UI, NO
// statics, NO g_xEngine, NO RNG, NO allocation, NO I/O.
//
// It is deliberately the SAME SHAPE as ZM_PickInteractTarget's per-probe test --
// XZ range, absolute vertical band, XZ cone -- and it calls the SAME primitives
// (ZM_FlattenXZ / ZM_IsFacingXZ) rather than restating them. A reader who knows
// the interact picker knows this file on sight, and the two can never drift into
// disagreeing about what "facing" means.
//
// OCCLUSION IS DELIBERATELY ABSENT. A trainer whose line of sight is blocked by a
// wall still satisfies this predicate; the raycast enters at SC6 as a FILTER in
// the impure glue that owns the scene, precisely so this picker stays pure and
// unit-testable. Do not add a raycast, a physics include or a scene lookup here.
//
// EVERY function below is TOTAL: no argument value, however degenerate, is UB,
// and none of them calls Zenith_Assert. Zenith_Assert breaks the process in EVERY
// configuration and the whole unit suite runs at boot, so an assert on an
// argument a unit deliberately feeds does not fail one test -- it ends the boot
// unit run and takes the whole gate down.
// ============================================================================

// ---- Shipped sight tuning (spelled once, HERE) ------------------------------
// `inline` so each has ONE definition across every TU (the ZENITH_ASSERT_* macros
// bind their operands by const reference, so the units odr-use them).

// How far a trainer notices the player, in world units, measured in XZ. Wider
// than fZM_INTERACT_MAX_DISTANCE (2.5) on purpose: being spotted has to be able
// to happen before the player is close enough to start a conversation.
inline constexpr float fZM_SIGHT_MAX_DISTANCE = 8.0f;
// Minimum dot(trainerFacing, toTarget): cos(30 degrees), i.e. a 60-degree cone.
// NARROWER than fZM_INTERACT_MIN_FACING_DOT (0.35, ~140 degrees) on purpose -- a
// trainer stares down a lane; the player talks to whatever is roughly in front.
inline constexpr float fZM_SIGHT_MIN_FACING_DOT = 0.8660254f;
// Maximum absolute height difference, so a trainer does not spot the player
// through a floor or from a roof. The same one-storey band the picker uses.
inline constexpr float fZM_SIGHT_MAX_VERTICAL = 2.0f;

// The three thresholds, passed in rather than read from the constants above so a
// unit can move a boundary without touching shipped tuning. Default-constructed
// it IS the shipped tuning.
struct ZM_TrainerSightTuning
{
	float m_fMaxDistance  = fZM_SIGHT_MAX_DISTANCE;
	float m_fMinFacingDot = fZM_SIGHT_MIN_FACING_DOT;
	float m_fMaxVertical  = fZM_SIGHT_MAX_VERTICAL;
};

// True iff xTargetPosition is inside the trainer's forward sight cone.
//
// ACCEPTANCE, in this order (the order IS the specification):
//   1. Every input finite. See totality below.
//   2. xTrainerForward flattens (ZM_FlattenXZ) to a non-zero XZ direction.
//   3. m_fMaxDistance >= 0.
//   4. XZ distance (Y IGNORED) <= m_fMaxDistance. INCLUSIVE.
//   5. fabs(dY) <= m_fMaxVertical. INCLUSIVE.
//   6. ZM_IsFacingXZ(...) -- the shared angular test. INCLUSIVE at the cone edge.
//
// PLANAR, NOT 3D, and that is a consistency decision rather than a geometric one:
// ZM_PickInteractTarget measures range in XZ, cones in XZ, and polices height as a
// separate absolute band. Two cone shapes in one game would mean an NPC the player
// can talk to but who cannot see them (or the reverse) at exactly the height
// difference where the two conventions part company.
//
// TOTALITY -- the complete table. It NEVER calls Zenith_Assert:
//   * ANY non-finite (NaN / +-inf) component of any position, of the forward, or
//     of any tuning field                        -> FALSE. Fail closed: SC6 will
//     feed this live physics positions, and one body that goes non-finite must
//     not make every trainer in the scene see the player.
//   * zero-length forward, or a straight-up / straight-down forward whose XZ
//     projection collapses                       -> FALSE (no facing, no cone).
//   * m_fMaxDistance < 0                         -> FALSE. A negative range must
//     never square its way back into "everything is in range".
//   * m_fMaxDistance == 0                        -> only a COINCIDENT target
//     passes (the inclusive range test admits distance 0 and nothing else).
//   * target COINCIDENT with the trainer (XZ separation^2 <=
//     fZM_INTERACT_DEGENERATE_LEN_SQ)            -> TRUE, provided range >= 0 and
//     the band holds. There is no direction to test and the player is standing on
//     the trainer; this matches the picker's coincident carve-out exactly.
//   * m_fMinFacingDot > 1 (half-angle <= 0, an impossible cone) -> FALSE for every
//     target except a coincident one. No clamp: the parameter is a cosine and has
//     no out-of-domain value.
//   * m_fMinFacingDot <= -1 (half-angle >= 180 degrees)         -> the angular
//     test admits everything; range and band still gate.
//   * m_fMaxVertical < 0                         -> FALSE for every target,
//     including one at the trainer's exact height (fabs(0) > negative).
bool ZM_IsTargetInTrainerSight(const Zenith_Maths::Vector3& xTrainerPosition,
	const Zenith_Maths::Vector3& xTrainerForward,
	const Zenith_Maths::Vector3& xTargetPosition,
	const ZM_TrainerSightTuning& xTuning);

// The same predicate for a caller holding a rotation rather than a facing vector
// (SC6's trainer transform). It DERIVES the facing with ZM_ForwardFromRotation
// and forwards -- it never re-derives a facing of its own, and in particular it
// must never be rewritten in terms of glm::eulerAngles(quat).y, which collapses
// past 90 degrees off +Z and has already cost this repo a full debugging cycle.
// TOTAL for the same reasons: a non-finite rotation yields a non-finite forward,
// which the finite guard above rejects.
bool ZM_IsTargetInTrainerSightFromRotation(const Zenith_Maths::Vector3& xTrainerPosition,
	const Zenith_Maths::Quat& xTrainerRotation,
	const Zenith_Maths::Vector3& xTargetPosition,
	const ZM_TrainerSightTuning& xTuning);
