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
// VOCABULARY. Zenithmon's authored entity position for a human is the body
// CENTRE (feet + fZM_HUMAN_BODY_HALF_HEIGHT), never the feet -- the camera pivot,
// ZM_ProbeGround, the sight-cone origin, the walk-up standoff, both head anchors,
// every spawn marker conversion and the camera-clearance table all speak that
// vocabulary. Nothing here may change without changing all of them.
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
// body (ZM_HumanGen's centre-anchored bind space, ~2.6 loft units tall) lands on
// the body contract above. UNIFORM by decision: it is why the bodies must come
// from the explicit dimensions here rather than from scale.
inline constexpr float fZM_HUMAN_VISUAL_SCALE =
	fZM_HUMAN_BODY_HEIGHT / fZM_HUMAN_CANONICAL_BODY_HEIGHT;
