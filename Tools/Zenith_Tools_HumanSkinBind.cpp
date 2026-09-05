#include "Zenith.h"

#include "Zenith_Tools_HumanSkinBind.h"

#include "AssetHandling/Zenith_HumanProportions.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Zenith_Tools_HumanSkinBind
{
namespace
{
	//--------------------------------------------------------------------------
	// The solve set: the sixteen core bones plus the two toes. EIGHTEEN, and the
	// three exclusions are each a decision rather than an oversight.
	//
	// ★ THE THIRTY FINGER BONES ARE COLLAPSED ONTO THE HAND. Not one of the 17
	// authored clips carries a finger channel, a 27k-vertex whole body gives a
	// phalanx one or two rings, and StickFigure's finger offsets describe ITS
	// stylised hand rather than this artist's. The bones stay in the rig for
	// parity -- they still ride the arm mask in the warp, and
	// AddStep_AttachToBone("RightHand") is unaffected -- they simply carry no
	// weight, which leaves the door open for a denser hand later.
	//
	// ★ JAW AND EYES ARE SKIPPED for the same reason and one more: an eye bone
	// sits INSIDE the skull, so a distance-based solver would hand it a patch of
	// forehead. The head owns the whole head.
	//--------------------------------------------------------------------------
	struct SolveBone
	{
		const char* m_szName;
		const char* m_szChild;      // the bone its segment runs to; nullptr = terminal
		float m_fRadiusFrac;        // influence radius, as a fraction of body height
	};

	// ★ EVERY BONE'S SEGMENT IS DEFINED, EXPLICITLY, INCLUDING THE AWKWARD ONES.
	// Root branches three ways (Spine and two legs), so "the segment to its first
	// child" would be whichever the rig happened to list first; it is declared as
	// Root->Spine and the region gates own the pelvis. A terminal bone (Head,
	// Toe) has no child at all and extends its parent's axis instead.
	constexpr SolveBone axSOLVE_BONES[] =
	{
		{ "Root",          "Spine",         0.100f },
		{ "Spine",         "Neck",          0.105f },
		{ "Neck",          "Head",          0.055f },
		{ "Head",          nullptr,         0.070f },
		{ "LeftUpperArm",  "LeftLowerArm",  0.050f },
		{ "LeftLowerArm",  "LeftHand",      0.040f },
		{ "LeftHand",      nullptr,         0.045f },
		{ "RightUpperArm", "RightLowerArm", 0.050f },
		{ "RightLowerArm", "RightHand",     0.040f },
		{ "RightHand",     nullptr,         0.045f },
		{ "LeftUpperLeg",  "LeftLowerLeg",  0.075f },
		{ "LeftLowerLeg",  "LeftFoot",      0.055f },
		{ "LeftFoot",      "LeftToe",       0.050f },
		{ "LeftToe",       nullptr,         0.040f },
		{ "RightUpperLeg", "RightLowerLeg", 0.075f },
		{ "RightLowerLeg", "RightFoot",     0.055f },
		{ "RightFoot",     "RightToe",      0.050f },
		{ "RightToe",      nullptr,         0.040f },
	};
	constexpr u_int uNUM_SOLVE_BONES = sizeof(axSOLVE_BONES) / sizeof(axSOLVE_BONES[0]);

	// One solve bone, resolved to a segment in model space.
	struct ResolvedBone
	{
		u_int m_uIndex = 0u;
		Zenith_Maths::Vector3 m_xA{ 0.0f };
		Zenith_Maths::Vector3 m_xB{ 0.0f };
		float m_fRadius = 0.0f;
		int   m_iSide = 0;          // -1 left, +1 right, 0 centre
		bool  m_bArm = false;
		bool  m_bLeg = false;
		bool  m_bSpineOrNeck = false;
		bool  m_bToe = false;
	};

	// Hermite ramp: 0 at fEdge0, 1 at fEdge1, smooth at both ends. Works either
	// way round, so a gate can open or close with increasing coordinate.
	float SmoothGate(float fValue, float fEdge0, float fEdge1)
	{
		if (std::fabs(fEdge1 - fEdge0) < 1.0e-8f) { return (fValue >= fEdge1) ? 1.0f : 0.0f; }
		const float fT = std::clamp((fValue - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
		return fT * fT * (3.0f - 2.0f * fT);
	}

		// ★★ A REGION GATE MUST BE A RAMP, NOT AN IF. This returns a multiplier in
	// [0,1] rather than a yes/no, and that difference is the whole reason the
	// shoulders came out mangled.
	//
	// The gates started as hard `continue`s: "an arm bone may not claim anything
	// inboard of 0.75 of the shoulder half-width". At that boundary a bone's
	// weight does not fade in -- it JUMPS from zero to whatever the distance
	// falloff happens to give there, which near the deltoid is about 0.4. So two
	// vertices 8 mm apart ended up with arm weights of 0.00 and 0.38, and under
	// the rebind's 90-degree shoulder rotation that weight cliff tore the surface:
	// measured edge stretches up to 8.6x, with 4.7% of ALL edges deformed by more
	// than 10%. It reads as a lumpy, padded shoulder.
	//
	// ★ THE OVERLAP OF TWO GATES IS NOT A BLEND if each gate is a step. That was
	// the mistaken premise -- the overlap decides which bones COMPETE, while what
	// makes the result continuous is each gate's own edge being smooth.
	//
	// Every band below is centred on the boundary the hard version used, so the
	// intent is unchanged: no bone reaches across the midline, no arm claims
	// chest, no spine claims deltoid, no leg claims belly, no toe claims heel.
	float RegionGate(const ResolvedBone& xR, const Zenith_Maths::Vector3& xP,
		float fShoulderHalfX, float fHipPlane, float fHeight, float fToeCut)
	{
		float fGate = 1.0f;

		// left/right bleed: a left bone reaching across the midline drags the
		// right hip when the left leg swings.
		if (xR.m_iSide < 0) { fGate *= SmoothGate(xP.x, 0.30f * fShoulderHalfX, 0.0f); }
		if (xR.m_iSide > 0) { fGate *= SmoothGate(xP.x, -0.30f * fShoulderHalfX, 0.0f); }

		// armpit bleed: an arm bone claiming chest, or a spine claiming deltoid.
		if (xR.m_bArm) { fGate *= SmoothGate(std::fabs(xP.x), 0.55f * fShoulderHalfX, 0.95f * fShoulderHalfX); }
		if (xR.m_bSpineOrNeck) { fGate *= SmoothGate(std::fabs(xP.x), 1.45f * fShoulderHalfX, 1.05f * fShoulderHalfX); }

		// crotch bleed: the same idea on Y about the hip plane.
		if (xR.m_bLeg) { fGate *= SmoothGate(xP.y, fHipPlane + 0.12f * fHeight, fHipPlane + 0.00f * fHeight); }
		if (xR.m_bSpineOrNeck) { fGate *= SmoothGate(xP.y, fHipPlane - 0.12f * fHeight, fHipPlane - 0.00f * fHeight); }

		// toes: only the forefoot.
		if (xR.m_bToe) { fGate *= SmoothGate(xP.z, fToeCut - 0.25f * std::fabs(fToeCut), fToeCut + 0.25f * std::fabs(fToeCut) + 1.0e-4f); }

		return fGate;
	}

	// Distance to the segment, plus how far the point lies BEYOND either end of it
	// (zero when it projects onto the segment itself).
	float DistanceToSegment(const Zenith_Maths::Vector3& xP,
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float& fOvershootOut)
	{
		fOvershootOut = 0.0f;
		const Zenith_Maths::Vector3 xAB = xB - xA;
		const float fLenSq = glm::dot(xAB, xAB);
		if (fLenSq <= 1.0e-12f) { return glm::length(xP - xA); }
		const float fRaw = glm::dot(xP - xA, xAB) / fLenSq;
		const float fT = std::clamp(fRaw, 0.0f, 1.0f);
		const float fLen = std::sqrt(fLenSq);
		if (fRaw < 0.0f) { fOvershootOut = -fRaw * fLen; }
		else if (fRaw > 1.0f) { fOvershootOut = (fRaw - 1.0f) * fLen; }
		return glm::length(xP - (xA + xAB * fT));
	}
}

//==============================================================================
// Normalisation
//==============================================================================

bool NormaliseMeshIntoRigSpace(Zenith_MeshAsset& xMesh, float fYawDegrees)
{
	const u_int uNumVerts = xMesh.GetNumVerts();
	if (uNumVerts == 0u) { return false; }

	const float fRad = fYawDegrees * 3.14159265358979f / 180.0f;
	const Zenith_Maths::Quat xYaw = glm::angleAxis(fRad, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

	for (u_int v = 0u; v < uNumVerts; ++v)
	{
		xMesh.m_xPositions.Get(v) = xYaw * xMesh.m_xPositions.Get(v);
		if (v < xMesh.m_xNormals.GetSize()) { xMesh.m_xNormals.Get(v) = xYaw * xMesh.m_xNormals.Get(v); }
	}

	Zenith_Maths::Vector3 xMin = xMesh.m_xPositions.Get(0u);
	Zenith_Maths::Vector3 xMax = xMin;
	for (u_int v = 1u; v < uNumVerts; ++v)
	{
		xMin = glm::min(xMin, xMesh.m_xPositions.Get(v));
		xMax = glm::max(xMax, xMesh.m_xPositions.Get(v));
	}
	const float fHeight = xMax.y - xMin.y;
	if (fHeight <= 1.0e-5f)
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: source model has no height (%.6f)", fHeight);
		return false;
	}

	// UNIFORM, always: a per-axis fit would squash a body to the rig's aspect
	// ratio, which is precisely the deformation this whole exercise exists to
	// avoid.
	const float fScale = fZENITH_HUMAN_RIG_HEIGHT / fHeight;
	const float fCentreX = 0.5f * (xMin.x + xMax.x);
	const float fCentreZ = 0.5f * (xMin.z + xMax.z);

	for (u_int v = 0u; v < uNumVerts; ++v)
	{
		Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);
		xP.x = (xP.x - fCentreX) * fScale;
		xP.y = fZENITH_HUMAN_RIG_SOLE_Y + (xP.y - xMin.y) * fScale;
		xP.z = (xP.z - fCentreZ) * fScale;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS,
		"HUMAN_BIND: normalised into rig space - yaw %.1f deg, uniform scale %.6f "
		"(source height %.6f -> %.6f), sole at %.6f",
		fYawDegrees, fScale, fHeight, fZENITH_HUMAN_RIG_HEIGHT, fZENITH_HUMAN_RIG_SOLE_Y);
	return true;
}

bool DetectAndNormaliseIntoRigSpace(Zenith_MeshAsset& xMesh, float& fYawDegreesOut)
{
	fYawDegreesOut = 0.0f;
	const u_int uNumVerts = xMesh.GetNumVerts();
	if (uNumVerts == 0u) { return false; }

	//--- Which horizontal axis carries the arm span. A T-posed humanoid is several
	//    times wider than it is deep, so this is not a close call for any body this
	//    rig could accept -- and CheckLandmarksAgainstRig refuses the ones it could.
	Zenith_Maths::Vector3 xMin = xMesh.m_xPositions.Get(0u);
	Zenith_Maths::Vector3 xMax = xMin;
	for (u_int v = 1u; v < uNumVerts; ++v)
	{
		xMin = glm::min(xMin, xMesh.m_xPositions.Get(v));
		xMax = glm::max(xMax, xMesh.m_xPositions.Get(v));
	}
	const float fSpanX = xMax.x - xMin.x;
	const float fSpanZ = xMax.z - xMin.z;
	float fYaw = (fSpanZ > fSpanX) ? 90.0f : 0.0f;

	if (!NormaliseMeshIntoRigSpace(xMesh, fYaw)) { return false; }

	//--- Which way along the other axis is forward. The mesh is centred on X and Z
	//    by the normalise above, so the 180-degree correction is a negation of both
	//    -- a proper rotation, so it preserves winding and needs no index flip, and
	//    it leaves the sole plane and the uniform scale exactly where they were.
	Zenith_SkinDeformView xView = Zenith_MakeSkinDeformView(xMesh);
	Zenith_HumanLandmarks xLandmarks;
	if (!Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_T_POSE, xLandmarks))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: cannot measure the source to orient it");
		return false;
	}
	if (!xLandmarks.m_bFootFacingMeasured)
	{
		// ★ REFUSE RATHER THAN GUESS. A coin flip here is a character that ships
		// backwards, and that is the one error a screenshot pass demonstrably does
		// not catch: at head-thumbnail size the back of a head reads as a face.
		Zenith_Error(LOG_CATEGORY_TOOLS,
			"HUMAN_BIND: cannot tell which way this model faces - no foot/shin lever was measurable. "
			"The source needs recognisable feet for the importer to orient it.");
		return false;
	}
	if (xLandmarks.m_fFacingSign < 0.0f)
	{
		for (u_int v = 0u; v < uNumVerts; ++v)
		{
			Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);
			xP.x = -xP.x;
			xP.z = -xP.z;
			if (v < xMesh.m_xNormals.GetSize())
			{
				Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(v);
				xN.x = -xN.x;
				xN.z = -xN.z;
			}
		}
		fYaw += 180.0f;
	}

	fYawDegreesOut = fYaw;
	Zenith_Log(LOG_CATEGORY_TOOLS,
		"HUMAN_BIND: orientation MEASURED - span x %.4f z %.4f, facing sign %.3f, yaw %.1f deg applied",
		fSpanX, fSpanZ, xLandmarks.m_fFacingSign, fYaw);
	return true;
}

bool CheckLandmarksAgainstRig(const Zenith_HumanLandmarks& xLandmarks,
	const Zenith_HumanProportions& xRig, float fToleranceFrac, std::string& strWhyOut)
{
	if (!xLandmarks.m_bValid)
	{
		strWhyOut = "no landmarks were measured";
		return false;
	}
	const float fHeight = xLandmarks.Height();
	if (fHeight <= 1.0e-5f)
	{
		strWhyOut = "measured height is degenerate";
		return false;
	}

	// Every anchor the rig actually places a bone at, as a fraction of height, so
	// the comparison means the same thing at any scale.
	//
	// ★ AN ANCHOR THE MESH DOES NOT HAVE IS SKIPPED, NOT FAILED. m_abBodyFound
	// exists because a mesh can legitimately lack one -- a leg that tapers to a
	// point has no ankle seam to find -- and treating "not modelled" as "wrong
	// proportions" would refuse a body for something it never claimed.
	struct Anchor { const char* szName; ZENITH_HUMAN_BODY_ANCHOR eAnchor; float fRig; };
	const Anchor axAnchors[] = {
		{ "ankle",    ZENITH_HUMAN_BODY_ANKLE,    xRig.m_fAnkleFrac },
		{ "knee",     ZENITH_HUMAN_BODY_KNEE,     xRig.m_fKneeFrac },
		{ "hip",      ZENITH_HUMAN_BODY_HIP,      xRig.m_fHipFrac },
		{ "shoulder", ZENITH_HUMAN_BODY_SHOULDER, xRig.m_fShoulderFrac },
		{ "neck",     ZENITH_HUMAN_BODY_NECK,     xRig.m_fNeckFrac },
	};

	for (const Anchor& xA : axAnchors)
	{
		if (!xLandmarks.m_abBodyFound[xA.eAnchor]) { continue; }
		const float fMeasured = xLandmarks.Frac(xLandmarks.m_afBodyY[xA.eAnchor]);
		const float fDelta = std::fabs(fMeasured - xA.fRig);
		if (fDelta > fToleranceFrac)
		{
			strWhyOut = std::string("its ") + xA.szName + " is at " + std::to_string(fMeasured) +
				" of height and the rig's is at " + std::to_string(xA.fRig) + " (off by " +
				std::to_string(fDelta) + ", tolerance " + std::to_string(fToleranceFrac) + ")";
			return false;
		}
	}

	if (xLandmarks.m_bArmChainFound)
	{
		const float fShoulder = xLandmarks.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER];
		const float fWristFrac = std::fabs(xLandmarks.m_afArmChain[ZENITH_HUMAN_ARM_WRIST] - fShoulder) / fHeight;
		const float fDelta = std::fabs(fWristFrac - xRig.m_fShoulderToWristFrac);
		if (fDelta > fToleranceFrac)
		{
			strWhyOut = "its shoulder-to-wrist is " + std::to_string(fWristFrac) +
				" of height and the rig's is " + std::to_string(xRig.m_fShoulderToWristFrac) +
				" (off by " + std::to_string(fDelta) + ")";
			return false;
		}
	}
	return true;
}

//==============================================================================
// Arm-chain sanity
//==============================================================================

ArmSanity CheckArmChain(const Zenith_HumanLandmarks& xLandmarks)
{
	ArmSanity xOut;
	if (!xLandmarks.m_bValid || !xLandmarks.m_bArmChainFound)
	{
		xOut.m_strReason = "no arm chain was measured";
		return xOut;
	}

	const float fHeight = xLandmarks.Height();
	const float fShoulder = xLandmarks.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER];
	const float fElbow = xLandmarks.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW];
	const float fWrist = xLandmarks.m_afArmChain[ZENITH_HUMAN_ARM_WRIST];
	const float fTip = xLandmarks.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP];

	// In a T-pose the chain runs OUTWARD along the lateral axis, so the
	// differences are the other way round from the arms-down case. Take absolute
	// segment lengths and the pose stops mattering.
	const float fUpper = std::fabs(fElbow - fShoulder);
	const float fFore = std::fabs(fWrist - fElbow);
	const float fHand = std::fabs(fTip - fWrist);

	xOut.m_fHandLengthFrac = fHand / fHeight;
	const Zenith_HumanProportions& xL = Zenith_HumanProportionsLegacy();
	const float fLegacyUpper = xL.m_fShoulderToElbowFrac * fHeight;
	const float fLegacyFore = (xL.m_fShoulderToWristFrac - xL.m_fShoulderToElbowFrac) * fHeight;
	xOut.m_fUpperArmRatio = (fLegacyUpper > 1.0e-6f) ? (fUpper / fLegacyUpper) : 0.0f;
	xOut.m_fForearmRatio = (fLegacyFore > 1.0e-6f) ? (fFore / fLegacyFore) : 0.0f;

	// A human hand is 10-11% of a stature. Anything outside 10-15% is a landmark
	// that caught something other than a wrist -- most likely a chunky mitten,
	// which is the exact failure this bound exists for.
	if (xOut.m_fHandLengthFrac < 0.10f || xOut.m_fHandLengthFrac > 0.15f)
	{
		xOut.m_strReason = "hand length is " + std::to_string(xOut.m_fHandLengthFrac) +
			" of height, outside [0.10, 0.15]";
		return xOut;
	}
	if (xOut.m_fUpperArmRatio < 0.5f || xOut.m_fUpperArmRatio > 1.5f)
	{
		xOut.m_strReason = "upper arm is " + std::to_string(xOut.m_fUpperArmRatio) + "x the legacy rig's";
		return xOut;
	}
	if (xOut.m_fForearmRatio < 0.5f || xOut.m_fForearmRatio > 1.5f)
	{
		xOut.m_strReason = "forearm is " + std::to_string(xOut.m_fForearmRatio) + "x the legacy rig's";
		return xOut;
	}

	xOut.m_bValid = true;
	return xOut;
}

//==============================================================================
// The fitted T-pose rig
//==============================================================================

bool SolveHumanSkinWeights(Zenith_MeshAsset& xMesh, const Zenith_SkeletonAsset& xRig,
	const Zenith_HumanLandmarks& xLandmarks, SolveReport& xReportOut)
{
	xReportOut = SolveReport();
	const u_int uNumVerts = xMesh.GetNumVerts();
	if (uNumVerts == 0u || !xLandmarks.m_bValid) { return false; }

	const float fHeight = xLandmarks.Height();
	const float fShoulderHalfX = (xLandmarks.m_fShoulderHalfX > 1.0e-4f)
		? xLandmarks.m_fShoulderHalfX : Zenith_HumanProportionsRealistic().ShoulderHalfX();
	const float fHipPlane = Zenith_HumanProportionsRealistic().HipY();
	const float fToeCut = xLandmarks.m_fHeelZ + 0.55f * (xLandmarks.m_fToeZ - xLandmarks.m_fHeelZ);

	//--- Resolve each solve bone to its segment, in model space.
	Zenith_Vector<ResolvedBone> xBones;

	for (u_int u = 0u; u < uNUM_SOLVE_BONES; ++u)
	{
		const SolveBone& xDef = axSOLVE_BONES[u];
		const int32_t iBone = xRig.GetBoneIndex(xDef.m_szName);
		if (iBone == Zenith_SkeletonAsset::INVALID_BONE_INDEX)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: the rig has no bone '%s'", xDef.m_szName);
			return false;
		}
		ResolvedBone xR;
		xR.m_uIndex = static_cast<u_int>(iBone);
		xR.m_fRadius = xDef.m_fRadiusFrac * fHeight;
		xR.m_xA = Zenith_Maths::Vector3(xRig.GetBone(xR.m_uIndex).m_xBindPoseModel[3]);

		if (xDef.m_szChild != nullptr)
		{
			const int32_t iChild = xRig.GetBoneIndex(xDef.m_szChild);
			if (iChild == Zenith_SkeletonAsset::INVALID_BONE_INDEX) { return false; }
			xR.m_xB = Zenith_Maths::Vector3(xRig.GetBone(static_cast<u_int>(iChild)).m_xBindPoseModel[3]);
		}
		else
		{
			// ★ A TERMINAL BONE EXTENDS ITS PARENT'S AXIS by 40% of that segment.
			// A head weighted to a POINT gets a spherical falloff centred on the
			// neck joint and loses the crown; giving it a segment gives it a skull.
			const int32_t iParent = xRig.GetBone(xR.m_uIndex).m_iParentIndex;
			const Zenith_Maths::Vector3 xParentPos = (iParent >= 0)
				? Zenith_Maths::Vector3(xRig.GetBone(static_cast<u_int>(iParent)).m_xBindPoseModel[3])
				: xR.m_xA;
			xR.m_xB = xR.m_xA + (xR.m_xA - xParentPos) * 0.4f;
		}

		const std::string strName = xDef.m_szName;
		xR.m_iSide = (strName.rfind("Left", 0) == 0) ? -1 : ((strName.rfind("Right", 0) == 0) ? 1 : 0);
		xR.m_bArm = (strName.find("Arm") != std::string::npos) || (strName.find("Hand") != std::string::npos);
		xR.m_bLeg = (strName.find("Leg") != std::string::npos) || (strName.find("Foot") != std::string::npos)
			|| (strName.find("Toe") != std::string::npos);
		xR.m_bSpineOrNeck = (strName == "Spine") || (strName == "Neck") || (strName == "Root");
		xR.m_bToe = (strName.find("Toe") != std::string::npos);
		xBones.PushBack(xR);
	}

	//--- Weld positions. ★ SOLVING PER-INDEX WOULD CRACK THE MESH OPEN. A Tripo
	//    atlas splits vertices at every UV seam, so the two sides of a seam are
	//    separate indices at the SAME point; give them different weights and they
	//    part company the first time the model animates.
	struct WeldKey { int32_t x, y, z; u_int uIndex; };
	const float fQuant = 1.0e5f / fHeight;   // ~1e-5 of a body height
	std::vector<WeldKey> xKeys(uNumVerts);
	for (u_int v = 0u; v < uNumVerts; ++v)
	{
		const Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);
		xKeys[v] = { static_cast<int32_t>(std::lround(xP.x * fQuant)),
					 static_cast<int32_t>(std::lround(xP.y * fQuant)),
					 static_cast<int32_t>(std::lround(xP.z * fQuant)), v };
	}
	std::vector<WeldKey> xSorted = xKeys;
	std::sort(xSorted.begin(), xSorted.end(), [](const WeldKey& a, const WeldKey& b)
	{
		if (a.x != b.x) { return a.x < b.x; }
		if (a.y != b.y) { return a.y < b.y; }
		if (a.z != b.z) { return a.z < b.z; }
		return a.uIndex < b.uIndex;
	});

	std::vector<u_int> xGroupOf(uNumVerts, 0u);
	u_int uNumGroups = 0u;
	for (size_t i = 0; i < xSorted.size(); ++i)
	{
		if (i > 0 && (xSorted[i].x != xSorted[i - 1].x || xSorted[i].y != xSorted[i - 1].y ||
			xSorted[i].z != xSorted[i - 1].z))
		{
			++uNumGroups;
		}
		else if (i == 0)
		{
			uNumGroups = 0u;
		}
		xGroupOf[xSorted[i].uIndex] = uNumGroups;
	}
	++uNumGroups;
	xReportOut.m_uUniquePositions = uNumGroups;

	std::vector<Zenith_Maths::Vector3> xGroupPos(uNumGroups, Zenith_Maths::Vector3(0.0f));
	for (u_int v = 0u; v < uNumVerts; ++v) { xGroupPos[xGroupOf[v]] = xMesh.m_xPositions.Get(v); }

	//--- Raw weights per unique position.
	const u_int uNumBoneSlots = xBones.GetSize();
	std::vector<float> xW(static_cast<size_t>(uNumGroups) * uNumBoneSlots, 0.0f);

	for (u_int g = 0u; g < uNumGroups; ++g)
	{
		const Zenith_Maths::Vector3& xP = xGroupPos[g];
		for (u_int b = 0u; b < uNumBoneSlots; ++b)
		{
			const ResolvedBone& xR = xBones.Get(b);

			const float fGate = RegionGate(xR, xP, fShoulderHalfX, fHipPlane, fHeight, fToeCut);
			if (fGate <= 0.0f) { continue; }

			float fOvershoot = 0.0f;
			const float fD = DistanceToSegment(xP, xR.m_xA, xR.m_xB, fOvershoot);

			// ★★ A BONE IS A CAPSULE, NOT A BALL, and getting that wrong is what
			// mangled the shoulder.
			//
			// Clamping the projection gives every bone a hemispherical cap of the
			// FULL influence radius beyond each end -- for the upper arm that is a
			// 22 cm ball centred on the shoulder joint, reaching up into the
			// trapezius. The jacket's shoulder cap, 9 cm ABOVE the joint and
			// plainly torso, came out a 50/50 tie between arm and spine. Under the
			// rebind's 90-degree rotation, a weight gradient that far from the
			// pivot shears violently: the lever multiplies it, so a 0.13 weight
			// step across a 4 mm edge stretched that edge more than EIGHTFOLD.
			//
			// Tapering past the ends keeps the deltoid attached to the arm without
			// letting the arm own the shoulder cap. The neighbouring bone's own
			// capsule covers the far end, so nothing is left unclaimed.
			const float fCap = SmoothGate(fOvershoot, 1.10f * xR.m_fRadius, 0.35f * xR.m_fRadius);
			if (fCap <= 0.0f) { continue; }

			// ★ A POWER LAW WITH A HARD CUT, never 1/d^n. An inverse power never
			// reaches zero, so a hand vertex keeps a trace of the head forever --
			// tiny, normalised away to near nothing, and still enough to make the
			// fingers twitch when the head turns.
			const float fT = 1.0f - fD / (2.5f * xR.m_fRadius);
			if (fT <= 0.0f) { continue; }
			const float fT2 = fT * fT;
			xW[static_cast<size_t>(g) * uNumBoneSlots + b] = fT2 * fT2 * fGate * fCap;
		}
	}

	//--- Neighbours over unique positions, from the index buffer.
	std::vector<std::vector<u_int>> xAdj(uNumGroups);
	for (u_int i = 0u; i + 2u < xMesh.m_xIndices.GetSize(); i += 3u)
	{
		const u_int a = xGroupOf[xMesh.m_xIndices.Get(i)];
		const u_int b = xGroupOf[xMesh.m_xIndices.Get(i + 1u)];
		const u_int c = xGroupOf[xMesh.m_xIndices.Get(i + 2u)];
		xAdj[a].push_back(b); xAdj[a].push_back(c);
		xAdj[b].push_back(a); xAdj[b].push_back(c);
		xAdj[c].push_back(a); xAdj[c].push_back(b);
	}

	//--- ★★ NORMALISE BEFORE SMOOTHING, and smooth the thing that actually gets
	//    used. Raw falloff values are peaky (a fourth power near a hard cut), so
	//    averaging them leaves the NORMALISED field -- the one the rebind reads --
	//    still wobbling by +-0.1 between neighbours. Across a 90-degree joint that
	//    wobble is a tear. Normalising first makes each smoothing pass operate on
	//    the quantity whose continuity matters.
	for (u_int g = 0u; g < uNumGroups; ++g)
	{
		float fSum = 0.0f;
		for (u_int b = 0u; b < uNumBoneSlots; ++b) { fSum += xW[static_cast<size_t>(g) * uNumBoneSlots + b]; }
		if (fSum > 1.0e-8f)
		{
			const float fInv = 1.0f / fSum;
			for (u_int b = 0u; b < uNumBoneSlots; ++b) { xW[static_cast<size_t>(g) * uNumBoneSlots + b] *= fInv; }
		}
	}

	//--- Smooth. ★ EVERY ITERATION RE-APPLIES THE GATES. Averaging with a
	//    neighbour on the far side of a gate boundary walks weight straight back
	//    across it, which would reintroduce exactly the bleed the gates removed.
	std::vector<float> xNext(xW.size(), 0.0f);
	for (int iIter = 0; iIter < 10; ++iIter)
	{
		for (u_int g = 0u; g < uNumGroups; ++g)
		{
			const Zenith_Maths::Vector3& xP = xGroupPos[g];
			for (u_int b = 0u; b < uNumBoneSlots; ++b)
			{
				float fSum = xW[static_cast<size_t>(g) * uNumBoneSlots + b];
				float fCount = 1.0f;
				for (u_int n : xAdj[g])
				{
					fSum += xW[static_cast<size_t>(n) * uNumBoneSlots + b];
					fCount += 1.0f;
				}
				float fValue = fSum / fCount;

				fValue *= RegionGate(xBones.Get(b), xP, fShoulderHalfX, fHipPlane, fHeight, fToeCut);
				xNext[static_cast<size_t>(g) * uNumBoneSlots + b] = fValue;
			}
		}
		xW.swap(xNext);
		for (u_int g = 0u; g < uNumGroups; ++g)
		{
			float fSum = 0.0f;
			for (u_int b = 0u; b < uNumBoneSlots; ++b) { fSum += xW[static_cast<size_t>(g) * uNumBoneSlots + b]; }
			if (fSum > 1.0e-8f)
			{
				const float fInv = 1.0f / fSum;
				for (u_int b = 0u; b < uNumBoneSlots; ++b) { xW[static_cast<size_t>(g) * uNumBoneSlots + b] *= fInv; }
			}
		}
	}

	//--- Top-4, normalise, sort descending. Then scatter to every duplicate.
	std::vector<glm::uvec4> xGroupIdx(uNumGroups, glm::uvec4(0u));
	std::vector<glm::vec4> xGroupW(uNumGroups, glm::vec4(0.0f));
	xReportOut.m_fMinWeightSum = 1.0e9f;
	xReportOut.m_fMaxWeightSum = -1.0e9f;

	for (u_int g = 0u; g < uNumGroups; ++g)
	{
		u_int auBest[4] = { 0u, 0u, 0u, 0u };
		float afBest[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		for (u_int b = 0u; b < uNumBoneSlots; ++b)
		{
			const float fValue = xW[static_cast<size_t>(g) * uNumBoneSlots + b];
			if (fValue <= 0.0f) { continue; }
			for (int k = 0; k < 4; ++k)
			{
				if (fValue > afBest[k])
				{
					for (int j = 3; j > k; --j) { afBest[j] = afBest[j - 1]; auBest[j] = auBest[j - 1]; }
					afBest[k] = fValue;
					auBest[k] = xBones.Get(b).m_uIndex;
					break;
				}
			}
		}

		float fSum = afBest[0] + afBest[1] + afBest[2] + afBest[3];
		if (fSum <= 1.0e-8f)
		{
			// ★ NO SILENT FALLBACK BONE. A vertex no bone claimed is a hole in the
			// gates or the radii, and pinning it to Root would hide that by making
			// a patch of skin ride the pelvis. Count it, report it, and let the
			// caller refuse; SolverWeightsAreNormalised asserts the count is zero.
			// ★ NAME THE FIRST FEW. "14 vertices were unclaimed" is a number you
			// can only act on by guessing; their POSITIONS say immediately whether
			// the hole is a gate, a radius, or a fixture that is not a body.
			if (xReportOut.m_uFallbackCount < 5u)
			{
				Zenith_Warning(LOG_CATEGORY_TOOLS,
					"HUMAN_BIND: no bone claimed the vertex at (%.4f, %.4f, %.4f)",
					xGroupPos[g].x, xGroupPos[g].y, xGroupPos[g].z);
			}
			++xReportOut.m_uFallbackCount;
			afBest[0] = 1.0f;
			auBest[0] = xBones.Get(0u).m_uIndex;
			fSum = 1.0f;
		}
		const float fInv = 1.0f / fSum;
		for (int k = 0; k < 4; ++k) { afBest[k] *= fInv; }

		xReportOut.m_fMinWeightSum = std::min(xReportOut.m_fMinWeightSum, afBest[0] + afBest[1] + afBest[2] + afBest[3]);
		xReportOut.m_fMaxWeightSum = std::max(xReportOut.m_fMaxWeightSum, afBest[0] + afBest[1] + afBest[2] + afBest[3]);
		xGroupIdx[g] = glm::uvec4(auBest[0], auBest[1], auBest[2], auBest[3]);
		xGroupW[g] = glm::vec4(afBest[0], afBest[1], afBest[2], afBest[3]);
		for (int k = 0; k < 4; ++k)
		{
			xReportOut.m_uMaxBoneIndex = std::max(xReportOut.m_uMaxBoneIndex, auBest[k]);
		}
	}

	for (u_int v = 0u; v < uNumVerts; ++v)
	{
		xMesh.SetVertexSkinning(v, xGroupIdx[xGroupOf[v]], xGroupW[xGroupOf[v]]);
	}

	xReportOut.m_bValid = (xReportOut.m_uFallbackCount == 0u);
	Zenith_Log(LOG_CATEGORY_TOOLS,
		"HUMAN_BIND: solved %u verts over %u unique positions, %u solve bones; "
		"fallbacks %u, weight sum [%.6f, %.6f], max bone index %u",
		uNumVerts, xReportOut.m_uUniquePositions, uNumBoneSlots,
		xReportOut.m_uFallbackCount, xReportOut.m_fMinWeightSum, xReportOut.m_fMaxWeightSum,
		xReportOut.m_uMaxBoneIndex);
	return true;
}

}   // namespace Zenith_Tools_HumanSkinBind

#ifdef ZENITH_TOOLS
#include "Zenith_Tools_HumanSkinBind.Tests.inl"
#endif
