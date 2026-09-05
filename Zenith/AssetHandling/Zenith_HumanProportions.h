#pragma once
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Zenith_HumanProportions -- WHERE A HUMAN'S JOINTS GO, as data.
//
// These twelve numbers used to be twenty literals inside CreateStickFigureSkeleton()
// in Tools/Zenith_Tools_TestAssetExport.cpp. They are here, in an ALL-CONFIG header,
// for the same reason GetHumanHandDigits() is shared between the skeleton and the
// hand mesh: the rig and every mesh skinned to it have to agree about where a knee
// is, and the only way to guarantee that is for both to read the same table.
//
// ★ EVERY VALUE IS A FRACTION OF TOTAL HEIGHT, measured from the SOLE (0.0) to the
// CROWN (1.0). That is what makes the table a statement about PROPORTION rather
// than about one particular mesh: a 2.6-unit StickFigure and a 0.98-unit imported
// artist mesh have the same shoulder FRACTION and completely different shoulder Y.
//
// ★ THE ARM IS THREE LENGTHS, NOT THREE HEIGHTS. An arm's segments are the same
// lengths whether it is held out sideways (a T-pose) or hanging down (this rig's
// bind pose), so the table stores shoulder-RELATIVE distances along the limb. Only
// the shoulder itself is a height.
//
// ★ ZenithBase, not Tools. Zenithmon's ZM_GenCommon.h:11-16 records why: the pure
// generation library compiles in EVERY config so its unit gate exercises it
// headless, and StickFigure's own helpers were un-linkable from it because they sit
// in an anonymous namespace in a Tools TU. The proportions table is exactly the
// thing both generators must share, so it may not repeat that mistake.
// ============================================================================

// The authored StickFigure bind space, in rig units. Every skeleton this engine
// ships is built at this scale: the sole plane is where BuildHumanShoe's sole
// bottoms out, and the height is that mesh's crown minus that sole.
//
// These are the SKELETON's constants. A MESH is measured in its own space and may
// legitimately differ (Zenithmon's loft has no shoes, so its lowest vertex sits
// above this plane) -- which is precisely why the warp pins each mesh's own sole
// and crown and only moves the INTERIOR anchors onto these rig planes.
inline constexpr float fZENITH_HUMAN_RIG_SOLE_Y = -1.0404f;
inline constexpr float fZENITH_HUMAN_RIG_HEIGHT = 2.6012f;

struct Zenith_HumanProportions
{
	// Heights above the sole, as fractions of total height.
	float m_fAnkleFrac    = 0.0f;
	float m_fKneeFrac     = 0.0f;
	float m_fHipFrac      = 0.0f;   // the Root bone -- the clips write ABSOLUTE Root positions
	float m_fSpineFrac    = 0.0f;   // Idle writes an ABSOLUTE Spine position
	float m_fShoulderFrac = 0.0f;
	float m_fNeckFrac     = 0.0f;
	float m_fHeadFrac     = 0.0f;

	// Half-widths, as fractions of total height.
	float m_fShoulderHalfXFrac = 0.0f;
	float m_fHipHalfXFrac      = 0.0f;

	// Arm segment lengths measured DOWN the limb from the shoulder joint, as
	// fractions of total height. Cumulative, so each is >= the one before it.
	float m_fShoulderToElbowFrac     = 0.0f;
	float m_fShoulderToWristFrac     = 0.0f;
	float m_fShoulderToFingertipFrac = 0.0f;

	//--- Rig-space accessors. A rig is always built at the authored scale above.
	float SoleY()      const { return fZENITH_HUMAN_RIG_SOLE_Y; }
	float CrownY()     const { return fZENITH_HUMAN_RIG_SOLE_Y + fZENITH_HUMAN_RIG_HEIGHT; }
	float AnkleY()     const { return AtFrac(m_fAnkleFrac); }
	float KneeY()      const { return AtFrac(m_fKneeFrac); }
	float HipY()       const { return AtFrac(m_fHipFrac); }
	float SpineY()     const { return AtFrac(m_fSpineFrac); }
	float ShoulderY()  const { return AtFrac(m_fShoulderFrac); }
	float NeckY()      const { return AtFrac(m_fNeckFrac); }
	float HeadY()      const { return AtFrac(m_fHeadFrac); }
	float ShoulderHalfX() const { return m_fShoulderHalfXFrac * fZENITH_HUMAN_RIG_HEIGHT; }
	float HipHalfX()      const { return m_fHipHalfXFrac * fZENITH_HUMAN_RIG_HEIGHT; }
	float ElbowY()     const { return ShoulderY() - m_fShoulderToElbowFrac * fZENITH_HUMAN_RIG_HEIGHT; }

	// ★ HOW HIGH THE FOOT BONE SITS ABOVE THE FLOOR THE MESH STANDS ON. This is
	// RenderTest's k_fAnkleHeight and the only quantity the re-proportioning
	// actually forces a game to change: it was 0.040 when the ankle was at 1.6%
	// of a body's height, and a real shod ankle is 7.5%. Derived, never typed --
	// the log line the binder prints every boot reports the same number.
	float AnkleHeightAboveSole() const { return AnkleY() - SoleY(); }
	// The three leg lengths the IK chain is built from.
	float ThighLength() const { return HipY() - KneeY(); }
	float ShinLength()  const { return KneeY() - AnkleY(); }
	float LegLength()   const { return HipY() - AnkleY(); }
	float WristY()     const { return ShoulderY() - m_fShoulderToWristFrac * fZENITH_HUMAN_RIG_HEIGHT; }
	float FingertipY() const { return ShoulderY() - m_fShoulderToFingertipFrac * fZENITH_HUMAN_RIG_HEIGHT; }

	// Every anchor in ascending Y, the order the warp needs them in.
	bool IsOrdered() const;

private:
	float AtFrac(float fFrac) const { return fZENITH_HUMAN_RIG_SOLE_Y + fFrac * fZENITH_HUMAN_RIG_HEIGHT; }
};

// EXACTLY today's rig, expressed as fractions. Every accessor round-trips to the
// literal it replaced to within float rounding, which is what
// LegacyProportionsReproduceShippedRig pins -- so "did extracting the table change
// the skeleton" is answered by a test rather than by reading.
const Zenith_HumanProportions& Zenith_HumanProportionsLegacy();

// The proportions MEASURED off the artist-authored male humanoid (see
// Tools/Zenith_Tools_HumanSkinBind.h). MaleLandmarksMatchProportionTable re-measures
// the real .glb at import and reds this table if a re-export moves a landmark.
const Zenith_HumanProportions& Zenith_HumanProportionsRealistic();

// ============================================================================
// THE SHARED HUMANOID RIG IS T-POSED.
//
// ★★ THIS IS THE WHOLE REASON AN ARTIST'S MESH CAN SHARE THE ENGINE'S ONE
// SKELETON. Inverse bind matrices live on Zenith_SkeletonAsset, so ONE skeleton
// means ONE rest pose, and every mesh bound to it must be modelled in that pose.
// Artist humanoids are authored T-posed, universally; the alternative -- an
// arms-down rest that each import is deformed into -- BAKES a 90-degree shoulder
// rotation through the skin weights and cannot be made clean. Measured on the
// real asset: 13% of shoulder edges distorted past 25%, single edges past 8x,
// permanently, reading as lumpy padded shoulders. The arithmetic says why: a
// vertex at lever r from the joint, in a blend band of width W, shears by about
// r*(pi/2)/W, so holding stretch under 25% needs W > 6r -- a band six times wider
// than its own distance from the joint, which cannot exist.
//
// So the rig is T-posed and the PROCEDURAL humans (StickFigure, Zenithmon) are
// the side that moved: both are generated, so their arms are swung out by a
// RIGID rotation about the shoulder after their proportion warp, which distorts
// nothing at all. An imported artist mesh is then never deformed by anything.
//
// ★ THE CLIPS DID NOT CHANGE, and could not have. Flux_AnimationController fills
// the output pose from the bind pose and then SampleFromClip OVERWRITES every
// channel a clip carries, so a clip's local rotation REPLACES the bind's rather
// than composing with it. The T-pose survives only in the inverse bind matrices.
// The one thing this depends on: a bone a clip does NOT animate keeps its bind
// local transform, and the two UpperArms are the only bones whose T-pose bind
// rotation is not identity -- so every clip must animate both, which
// GenerateStickFigureAssets asserts at bake time.
//
// fSide is -1 for the character's LEFT, +1 for the RIGHT, matching the sign the
// skeleton and both mesh generators already use for their limb loops.
// ============================================================================

// The UpperArm's bind LOCAL rotation. The arm chain's child offsets all run down
// -Y in the parent frame (unchanged from the arms-down rig), so this one rotation
// at the top of the chain swings the whole arm -- elbow, wrist, hand, all fifteen
// finger bones -- out along +-X. Every offset BELOW it is a model-space delta that
// rotates with its own parent, so R_parent^-1 * (child - parent) gives back
// exactly the delta that was already there: nothing below the shoulder changed.
Zenith_Maths::Quat Zenith_HumanArmBindRotation(float fSide);

// The point that rotation turns about: the UpperArm bone's own model-space bind
// position. A mesh's arm vertices must be rotated about THIS, not about the arm
// tube's centre, or the mesh stops matching the rig it is bound to.
Zenith_Maths::Vector3 Zenith_HumanShoulderPivot(const Zenith_HumanProportions& xP, float fSide);
