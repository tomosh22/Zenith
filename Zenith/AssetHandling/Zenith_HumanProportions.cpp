#include "Zenith.h"

#include "AssetHandling/Zenith_HumanProportions.h"

namespace
{
	// The literals CreateStickFigureSkeleton() used to carry, in rig units, kept
	// here so the Legacy table is visibly a RE-EXPRESSION of the shipped rig
	// rather than a re-typing of it. Dividing by the authored height and
	// multiplying back is exact to float rounding, which is what lets the
	// extraction be proved rather than asserted.
	constexpr float fLEGACY_ANKLE_Y    = -1.0f;
	constexpr float fLEGACY_KNEE_Y     = -0.5f;
	constexpr float fLEGACY_HIP_Y      =  0.0f;
	constexpr float fLEGACY_SPINE_Y    =  0.5f;
	constexpr float fLEGACY_SHOULDER_Y =  1.1f;
	constexpr float fLEGACY_NECK_Y     =  1.2f;
	constexpr float fLEGACY_HEAD_Y     =  1.4f;
	constexpr float fLEGACY_SHOULDER_HALF_X = 0.3f;
	constexpr float fLEGACY_HIP_HALF_X      = 0.15f;
	constexpr float fLEGACY_ELBOW_Y    =  0.7f;
	constexpr float fLEGACY_WRIST_Y    =  0.4f;
	// The shortest authored digit tip (GetHumanHandDigits' afTipY), i.e. how far
	// down the hand geometry actually reaches.
	constexpr float fLEGACY_FINGERTIP_Y = 0.205f;

	constexpr float HeightFrac(float fY)
	{
		return (fY - fZENITH_HUMAN_RIG_SOLE_Y) / fZENITH_HUMAN_RIG_HEIGHT;
	}
	constexpr float LengthFrac(float fLength)
	{
		return fLength / fZENITH_HUMAN_RIG_HEIGHT;
	}

	const Zenith_HumanProportions xLEGACY = []
	{
		Zenith_HumanProportions x;
		x.m_fAnkleFrac    = HeightFrac(fLEGACY_ANKLE_Y);
		x.m_fKneeFrac     = HeightFrac(fLEGACY_KNEE_Y);
		x.m_fHipFrac      = HeightFrac(fLEGACY_HIP_Y);
		x.m_fSpineFrac    = HeightFrac(fLEGACY_SPINE_Y);
		x.m_fShoulderFrac = HeightFrac(fLEGACY_SHOULDER_Y);
		x.m_fNeckFrac     = HeightFrac(fLEGACY_NECK_Y);
		x.m_fHeadFrac     = HeightFrac(fLEGACY_HEAD_Y);
		x.m_fShoulderHalfXFrac = LengthFrac(fLEGACY_SHOULDER_HALF_X);
		x.m_fHipHalfXFrac      = LengthFrac(fLEGACY_HIP_HALF_X);
		x.m_fShoulderToElbowFrac     = LengthFrac(fLEGACY_SHOULDER_Y - fLEGACY_ELBOW_Y);
		x.m_fShoulderToWristFrac     = LengthFrac(fLEGACY_SHOULDER_Y - fLEGACY_WRIST_Y);
		x.m_fShoulderToFingertipFrac = LengthFrac(fLEGACY_SHOULDER_Y - fLEGACY_FINGERTIP_Y);
		return x;
	}();

	// ------------------------------------------------------------------------
	// MEASURED off Zenith/Assets/Meshes/Humans/Male/Male.glb, whose POSITION
	// accessor gives height 0.980896 with the sole at -0.490387. Every number
	// below is a fraction of that height and was produced by the same bounded
	// scans Zenith_Tools_HumanSkinBind runs at import; the binder logs its own
	// measurement every boot and MaleLandmarksMatchProportionTable reds this
	// table if a re-export moves one.
	//
	// ★ FOUR OF THESE ARE PINNED TO THE LEGACY RIG ON PURPOSE, and the pins are
	// what keep this change from rippling through two games:
	//
	//   hip   -- the 17 clips write ABSOLUTE Root positions near the origin
	//            (CreateIdleAnimation's Root curve is +-0.009), so Root must stay
	//            at y = 0 or every clip translates the whole body.
	//   spine -- Idle writes an ABSOLUTE Spine position of (0, 0.5, 0).
	//   head  -- pinning the Head bone is what keeps the skull RIGID under the
	//            warp: the head anchor's source and target coincide, so the
	//            segment above it is the identity and the authored head size
	//            survives. The neck below it absorbs the difference.
	//   hipHalfX -- the male's thigh centre measures 0.064 of height against the
	//            legacy 0.058, a 1.2 cm difference at 1.8 m. Moving it would buy
	//            nothing visible and would need a second lateral shift channel in
	//            the warp, so it stays where it is. Recorded, not silently dropped.
	//
	// ★ THE KNEE IS DERIVED, NOT MEASURED. The male's leg cross-section has no
	// trustworthy radius minimum at the knee (0.0398 against 0.0389 and 0.0400
	// either side -- noise), so it is the midpoint of the hip and the ankle, which
	// is both deterministic and anthropometrically standard. The measured minimum
	// lands within 0.003 of it.
	// ------------------------------------------------------------------------
	const Zenith_HumanProportions xREALISTIC = []
	{
		Zenith_HumanProportions x;
		x.m_fAnkleFrac    = 0.0742f;                                  // leg radius minimum, lower 25%
		x.m_fHipFrac      = xLEGACY.m_fHipFrac;                       // PINNED (absolute Root keys)
		x.m_fKneeFrac     = 0.5f * (x.m_fHipFrac + x.m_fAnkleFrac);   // derived, see above
		x.m_fSpineFrac    = xLEGACY.m_fSpineFrac;                     // PINNED (absolute Spine keys)
		x.m_fShoulderFrac = 0.7735f;                                  // T-pose arm centreline
		x.m_fNeckFrac     = 0.8477f;                                  // radius minimum, shoulder..head band
		x.m_fHeadFrac     = xLEGACY.m_fHeadFrac;                      // PINNED (keeps the skull rigid)
		x.m_fShoulderHalfXFrac = 0.1212f;                             // where the arm tube meets the torso
		x.m_fHipHalfXFrac      = xLEGACY.m_fHipHalfXFrac;             // PINNED (see above)
		x.m_fShoulderToElbowFrac     = 0.13466f;                      // shoulder..wrist midpoint
		x.m_fShoulderToWristFrac     = 0.26933f;                      // arm radius minimum in [0.58, 0.82] of the limb
		x.m_fShoulderToFingertipFrac = 0.37129f;                      // extreme along the arm axis

		// ★ THESE ARE THE BINDER'S OWN OUTPUT, transcribed from an observed run
		// rather than computed. That matters for more than provenance: the fitted
		// T-pose rig is built from the MEASUREMENT and the shipped rig from this
		// TABLE, so any gap between them turns the rebind from a pure rotation
		// into a rotation plus a rescale of the artist's arm. Matching them makes
		// the male's own geometry survive its bind untouched, and
		// MaleLandmarksMatchProportionTable reds if a re-export moves one.
		return x;
	}();
}

bool Zenith_HumanProportions::IsOrdered() const
{
	const float afAscending[7] =
	{
		m_fAnkleFrac, m_fKneeFrac, m_fHipFrac, m_fSpineFrac,
		m_fShoulderFrac, m_fNeckFrac, m_fHeadFrac
	};
	if (afAscending[0] <= 0.0f || afAscending[6] >= 1.0f)
	{
		return false;   // a landmark outside the body is not a proportion
	}
	for (u_int u = 0; u + 1 < 7u; ++u)
	{
		if (afAscending[u + 1] <= afAscending[u])
		{
			return false;
		}
	}

	if (m_fShoulderToElbowFrac <= 0.0f ||
		m_fShoulderToWristFrac <= m_fShoulderToElbowFrac ||
		m_fShoulderToFingertipFrac <= m_fShoulderToWristFrac)
	{
		return false;
	}
	// The hand may not hang past the sole.
	if (m_fShoulderFrac - m_fShoulderToFingertipFrac <= 0.0f)
	{
		return false;
	}
	return m_fShoulderHalfXFrac > 0.0f && m_fHipHalfXFrac > 0.0f;
}

const Zenith_HumanProportions& Zenith_HumanProportionsLegacy()
{
	return xLEGACY;
}

const Zenith_HumanProportions& Zenith_HumanProportionsRealistic()
{
	return xREALISTIC;
}

Zenith_Maths::Quat Zenith_HumanArmBindRotation(float fSide)
{
	// Left (-1) points the arm down -X, right (+1) down +X. Rz(theta) maps the
	// chain's own (0,-L,0) to (L*sin(theta), -L*cos(theta), 0), so theta = -90 for
	// the left and +90 for the right. Written as an assertion of the RESULT rather
	// than a remembered sign, because getting it backwards crosses the arms through
	// the torso and every unit test that only measures LENGTHS stays green.
	const float fDegrees = (fSide < 0.0f) ? -90.0f : 90.0f;
	return glm::angleAxis(glm::radians(fDegrees), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
}

Zenith_Maths::Vector3 Zenith_HumanShoulderPivot(const Zenith_HumanProportions& xP, float fSide)
{
	return Zenith_Maths::Vector3(fSide * xP.ShoulderHalfX(), xP.ShoulderY(), 0.0f);
}
