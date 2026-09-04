#pragma once

// ============================================================================
// ZM_HumanBody -- THE HUMAN BODY CONTRACT.
//
// One compiled statement of how big a person is in Zenithmon, and the only
// answer the game is allowed to give. Every actor that stands on two legs -- the
// player, all six Dawnmere NPCs -- is exactly this size, in every scene.
//
// ★ WHY THIS EXISTS AT ALL: the size used to be a FUNCTION OF TRANSFORM SCALE.
// A human was authored at (0.8, 1.8, 0.8) and everything that needed to know how
// tall the body was called ZM_PlayerController::CalculateCapsuleHalfExtent on
// that scale. That worked only for as long as the authored scale WAS the body
// box. It stops working the moment a human wears a MODEL, because the model then
// dictates the scale -- and a UNIFORM scale degenerates a scale-derived capsule
// into a sphere (Zenith_EditorAutomation.h says so in as many words). So the
// dimensions moved out of the transform and into these constants, and the bodies
// are installed from them explicitly.
//
// THE CONSEQUENCE WORTH STATING: gameplay dimensions no longer depend on how a
// human is drawn. Whether the human bake exists or the cold-start fallback block
// is showing, the capsule, the ground probe, the camera pivot, the head anchors,
// the sight cone and every spawn point measure the same body.
//
// VOCABULARY. Zenithmon's authored entity position for a human is the FEET.
//
// ★ IT USED TO BE THE BODY CENTRE, AND THE MIGRATION TO THE SHARED STICKFIGURE RIG
// IS WHY IT MOVED. A spawn marker, a navmesh point, a ground probe and a placement
// constant are all naturally about where someone STANDS, and every one of them had
// to add half a body height to talk to the transform. The old comment here listed
// the seven subsystems that had to be changed together and warned that nothing
// could move without all of them; they have now moved together, and the conversions
// are gone rather than inverted -- ZM_HumanBodyCentre below is the one place the
// other convention is spelled, for the two things that genuinely want a middle (a
// physics capsule and a camera pivot).
//
// The rig did NOT dictate this. StickFigure is authored hip-at-origin, which is a
// third point again, and fZM_HUMAN_MODEL_OFFSET_Y in this file reconciles the model
// to the feet regardless. Feet-origin is a choice about what this GAME finds
// natural to author, and it is now the only convention in it.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanGen.h"   // fZM_HUMAN_CANONICAL_BODY_HEIGHT

// ---- The body box -----------------------------------------------------------
// 0.8 x 1.8 x 0.8 m. These are the numbers the game shipped with; the migration
// that introduced this header changed nothing but where they are spelled.
inline constexpr float fZM_HUMAN_BODY_HEIGHT      = 1.8f;
inline constexpr float fZM_HUMAN_BODY_HALF_HEIGHT = 0.9f;
inline constexpr float fZM_HUMAN_BODY_FOOTPRINT   = 0.8f;

// ---- The capsule the three DYNAMIC bodies wear ------------------------------
// A capsule of radius R and CYLINDER half-height H (excluding the caps, matching
// Zenith_ColliderComponent::AddCapsuleCollider and SetExplicitCapsuleDimensions)
// stands R + H tall in each direction. These two therefore have to sum to the
// body's half height, and the radius has to be half the footprint, or the capsule
// would not be the body box's inscribed capsule.
inline constexpr float fZM_HUMAN_BODY_CAPSULE_RADIUS        = 0.4f;
inline constexpr float fZM_HUMAN_BODY_CAPSULE_HALF_CYLINDER = 0.5f;

// The consistency the five numbers owe each other, checked at COMPILE time.
//
// ★ WITH A TOLERANCE, DELIBERATELY, AND EXACTLY ONE OF THEM NEEDS IT. Halving is
// exact in binary floating point, so the height and footprint relations hold bit
// for bit; 0.4f + 0.5f is NOT bit-equal to 0.9f (it lands one ulp high). The
// difference is 6e-8 m -- four orders of magnitude below the tightest epsilon any
// consumer compares with -- so this is a spelling artefact, not a disagreement.
inline constexpr float fZM_HUMAN_BODY_CONSISTENCY_EPSILON = 1.0e-6f;

static_assert(fZM_HUMAN_BODY_HALF_HEIGHT * 2.0f == fZM_HUMAN_BODY_HEIGHT,
	"the body's half height must be half its height");
static_assert(fZM_HUMAN_BODY_CAPSULE_RADIUS * 2.0f == fZM_HUMAN_BODY_FOOTPRINT,
	"the capsule radius must be half the body footprint");
static_assert(
	fZM_HUMAN_BODY_CAPSULE_RADIUS + fZM_HUMAN_BODY_CAPSULE_HALF_CYLINDER
		- fZM_HUMAN_BODY_HALF_HEIGHT < fZM_HUMAN_BODY_CONSISTENCY_EPSILON
	&& fZM_HUMAN_BODY_HALF_HEIGHT - fZM_HUMAN_BODY_CAPSULE_RADIUS
		- fZM_HUMAN_BODY_CAPSULE_HALF_CYLINDER < fZM_HUMAN_BODY_CONSISTENCY_EPSILON,
	"the capsule must stand as tall as the body box it inscribes");

// ---- Locomotion readability -------------------------------------------------
// When a human reads as WALKING rather than standing, and how long the two blend.
// Spelled here rather than inside the visual so a test can state the threshold it
// is driving a "Speed" parameter across, instead of a magic 0.1.
inline constexpr float fZM_HUMAN_WALK_SPEED_THRESHOLD    = 0.1f;
inline constexpr float fZM_HUMAN_LOCOMOTION_BLEND_SECONDS = 0.15f;

// ---- The visual scale -------------------------------------------------------
// The uniform transform scale a human MODEL is authored at, so that its measured
// body (ZM_HumanGen's rig-space loft, ~2.6 loft units tall) lands on the body
// contract above. UNIFORM by decision: it is why the bodies must come from the
// explicit dimensions here rather than from scale.
inline constexpr float fZM_HUMAN_VISUAL_SCALE =
	fZM_HUMAN_BODY_HEIGHT / fZM_HUMAN_CANONICAL_BODY_HEIGHT;

// ---- Where the model sits on the entity --------------------------------------
// ★ THE ONE PLACE THE RIG'S ORIGIN AND THIS GAME'S ORIGIN ARE RECONCILED.
//
// The shared StickFigure rig is authored HIP-AT-ORIGIN: its Root bone is at y=0 and
// a standing body reaches fZM_HUMAN_MESH_FEET_Y (about one loft unit) below it.
// Zenithmon's human entity origin is the FEET. Those are different points, and the
// generator cannot reconcile them -- a mesh may only be skinned in its own rig's
// bind space, so the loft has to stay where StickFigure put it.
//
// So the reconciliation is a MODEL-SPACE OFFSET, handed to
// Zenith_ModelComponent::SetModelSpaceOffset: lift the model by exactly the depth
// its feet hang below the rig origin, and the feet land on the entity origin. It is
// expressed in LOFT UNITS, not metres, because a model-space offset is scaled by the
// entity transform exactly like the vertices it moves -- fZM_HUMAN_VISUAL_SCALE
// converts it for free, and spelling it in metres here would double-apply.
//
// It is deliberately NOT baked into the mesh. Baking it back would move the
// vertices away from the pivots the rig rotates them about, which is the exact
// desync that made this game own a skeleton for five generator versions.
inline constexpr float fZM_HUMAN_MODEL_OFFSET_Y = -fZM_HUMAN_MESH_FEET_Y;

// The offset has one job, and this is it stated as an equation: a body whose feet
// are at fZM_HUMAN_MESH_FEET_Y in rig space must sit at 0 once offset.
static_assert(fZM_HUMAN_MESH_FEET_Y + fZM_HUMAN_MODEL_OFFSET_Y == 0.0f,
	"the model offset must place the canonical body's feet on the entity origin");

// ---- The one conversion out of feet-space ------------------------------------
// A human's entity origin is its FEET; a capsule and a camera pivot want the
// middle. Spelled as a named function rather than an inline "+ half height" so the
// handful of genuine centre-consumers are greppable, and so nobody re-derives the
// old convention by accident. Callers that want the TOP of the head add
// fZM_HUMAN_BODY_HEIGHT to the feet directly -- that needs no helper and reads
// correctly on its own.
inline Zenith_Maths::Vector3 ZM_HumanBodyCentre(const Zenith_Maths::Vector3& xFeet)
{
	return Zenith_Maths::Vector3(xFeet.x, xFeet.y + fZM_HUMAN_BODY_HALF_HEIGHT, xFeet.z);
}

// The collider offset that keeps a body's inscribed volume centred while its
// transform sits at the feet -- Zenith_ColliderComponent::SetExplicitShapeOffset's
// argument, and the physics twin of fZM_HUMAN_MODEL_OFFSET_Y. In METRES, unlike the
// model offset: a shape offset is absolute, exactly like the explicit capsule
// dimensions beside it, while a model-space offset is scaled by the transform.
inline constexpr float fZM_HUMAN_BODY_SHAPE_OFFSET_Y = fZM_HUMAN_BODY_HALF_HEIGHT;
