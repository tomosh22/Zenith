#pragma once

#include <string>

#include "AssetHandling/Zenith_HumanProportions.h"
#include "AssetHandling/Zenith_SkinDeform.h"
#include "Collections/Zenith_Vector.h"

class Zenith_MeshAsset;
class Zenith_SkeletonAsset;

// ============================================================================
// Zenith_Tools_HumanSkinBind -- turning an artist-authored, T-posed, UNRIGGED
// humanoid into a mesh skinned to the shipped StickFigure rig.
//
// ★ THE ORDER IS THE DESIGN, and every step of it is load-bearing:
//
//   LoadGlbMesh                  parse + decode; NO tangents (the vertices are
//                                about to move once more)
//   DetectAndNormaliseIntoRigSpace   the artist's axes and scale -> the rig's,
//                                with the facing MEASURED, not declared
//   MeasureHumanLandmarks        what this body's proportions actually ARE
//   CheckLandmarksAgainstRig     ...and whether the shared rig describes them
//   SolveHumanSkinWeights        against the SHIPPED rig itself
//   GenerateTangents             AFTER the vertices stop moving
//
// ★★ THE MESH IS NEVER RE-POSED, AND THERE IS NO INTERMEDIATE RIG. Both used to
// be here: the mesh was solved against a rig fitted to its own measurements and
// then transferred into an arms-down bind pose so every human could share one
// .zskel. That transfer BAKES a 90-degree shoulder rotation through the skin
// weights and cannot be made clean -- 13% of shoulder edges past 25% distortion,
// single edges past eightfold, reading as lumpy padded shoulders.
//
// The shared rig is T-POSED now (see Zenith_HumanArmBindRotation), so the pose an
// artist authors in IS the rest pose and there is nothing to transfer. And with
// no transfer there is no reason for a fitted rig either: weights are bone
// INDICES that ship beside the SHIPPED rig's inverse bind matrices, so solving
// against anything else deforms the mesh at rest by the difference. Solve against
// the rig the mesh will actually be skinned by; refuse a body it cannot describe.
//
// ★ WHY IT IS NOT INSIDE Zenith_Tools_GlbImport. ExportAllMeshes runs at
// Zenith_Engine.cpp:558; GenerateTestAssets -- which WRITES StickFigure.zskel --
// runs at :571. On a cold tree the rig this binds to does not exist yet when the
// glb importer runs. Hence a separate exporter, called after it, and the
// directory rule (IsHumanoidSourcePath) that makes the generic walk stand aside.
// ============================================================================
namespace Zenith_Tools_HumanSkinBind
{
	// Rotate about Y by fYawDegrees, then uniformly scale and translate xMesh so it
	// stands in the shared rig's bind space: sole on the rig's sole plane, crown on
	// its crown plane, centred on X and Z. Normals are rotated with it.
	bool NormaliseMeshIntoRigSpace(Zenith_MeshAsset& xMesh, float fYawDegrees);

	// ★★ THE SAME THING, WITH THE YAW MEASURED OFF THE BODY. This is what replaced
	// a committed .zbind sidecar carrying a hand-written 'yaw' line, and both
	// halves of the measurement come from anatomy rather than from convention:
	//
	//   WHICH horizontal axis is left/right -- a T-posed humanoid's arm span is
	//   several times its front-to-back depth, so the wider horizontal extent IS
	//   the left/right axis. There is no plausible humanoid for which it is not.
	//
	//   WHICH WAY along the other one is forward -- a foot reaches about three
	//   times further in front of its shin than behind it, which
	//   Zenith_MeasureHumanLandmarks already reports as m_fFacingSign.
	//
	// ★ THE SECOND HALF IS THE ONE THAT MATTERS. Getting the sign wrong ships a
	// character 180 degrees round, and a screenshot pass does not reliably catch it
	// -- at head-thumbnail size the back of a head reads as a face, which is how
	// the first version of this model shipped backwards past a review.
	//
	// ★ LEFT/RIGHT IS STILL NOT A GEOMETRIC FACT, and is no longer asked. A
	// symmetric T-posed humanoid gives geometry no way to tell its own left from
	// its right, so the mirror knob was never measurable -- but it was never
	// NEEDED either: the two choices produce visually identical characters, and
	// differ only in which arm leads a one-handed clip. The engine convention
	// decides it, so there is nothing to declare.
	bool DetectAndNormaliseIntoRigSpace(Zenith_MeshAsset& xMesh, float& fYawDegreesOut);

	// ------------------------------------------------------------------------
	// Does this body fit the rig it is about to be bound to?
	//
	// ★★ ONE SHARED SKELETON MEANS FIXED JOINT POSITIONS. A mesh whose knee sits
	// far from the rig's knee bone bends at its thigh, and no weight solve can
	// repair that -- so the mismatch is caught here, by name, rather than shipped.
	// Every comparison is a FRACTION OF HEIGHT, which is what makes it meaningful
	// across a 2.6-unit rig and a source of any scale.
	// ------------------------------------------------------------------------
	bool CheckLandmarksAgainstRig(const Zenith_HumanLandmarks& xLandmarks,
		const Zenith_HumanProportions& xRig, float fToleranceFrac, std::string& strWhyOut);

	// ------------------------------------------------------------------------
	// Sanity bounds on the arm chain.
	//
	// ★ THE ARM IS THE MEASUREMENT MOST LIKELY TO BE WRONG, so it is the one that
	// gets asserted. A landmark scan that catches a chunky mitten instead of a
	// wrist reports a hand half the length of the forearm, and nothing downstream
	// would notice: the rig would simply be built with a comically long hand and
	// every gate would stay green. These bounds are what turn that into a refusal.
	// ------------------------------------------------------------------------
	struct ArmSanity
	{
		bool  m_bValid = false;
		float m_fHandLengthFrac = 0.0f;      // wrist -> fingertip, over height
		float m_fUpperArmRatio = 0.0f;       // against the legacy rig's own upper arm
		float m_fForearmRatio = 0.0f;
		std::string m_strReason;
	};
	ArmSanity CheckArmChain(const Zenith_HumanLandmarks& xLandmarks);

	// What one weight solve produced, for the log line that makes it checkable.
	struct SolveReport
	{
		bool  m_bValid = false;
		u_int m_uUniquePositions = 0u;
		u_int m_uFallbackCount = 0u;      // vertices no bone claimed; must be 0
		u_int m_uMaxBoneIndex = 0u;
		float m_fMinWeightSum = 0.0f;
		float m_fMaxWeightSum = 0.0f;
	};

	// Distance-to-bone-SEGMENT with a normalised radial falloff, region-gated,
	// solved on position-welded vertices, smoothed, top-4, normalised, sorted
	// descending. See the .cpp for what each gate is for.
	bool SolveHumanSkinWeights(Zenith_MeshAsset& xMesh, const Zenith_SkeletonAsset& xRig,
		const Zenith_HumanLandmarks& xLandmarks, SolveReport& xReportOut);
}
