//------------------------------------------------------------------------------
// Zenith_AnimationPoseIK unit tests (WU-4.4).
// Included at the bottom of Zenith_AnimationPoseIK.cpp.
//
// ★ NONE OF THIS IS requiresGraphics, AND THAT IS THE POINT. A requiresGraphics
// test is SKIPPED-AS-PASSED headless, so the only coverage IK posing would have
// had is coverage that rots without anything going red. Everything below is a
// skeleton built in code, a Flux_SkeletonInstance (which owns no device
// resources — its Destroy clears an asset handle and a bone count), a stack
// Flux_SkeletonPose and arithmetic. The two document tests add a .zanim in a
// private temp directory. No device, no view, no window, no registry asset that
// outlives its test.
//
// ★ THE PROPERTIES PINNED HERE, one test each:
//   1. an UNREACHABLE target clamps to the chain's total length instead of
//      diverging — the effector lands on the line toward the target at full
//      extension, and every number is finite;
//   2. the solved LOCAL rotations reproduce the solved MODEL-SPACE joints when
//      recomposed — written into a fresh instance and composed by
//      Flux_SkeletonInstance, which is a different code path from the scratch
//      Flux_SkeletonPose the solve used, so this is a real assertion and not a
//      value compared against a re-computation of itself;
//   3. a controller's GetOutputPose() is BYTE-unchanged across a solve, and so
//      is the source instance — the whole reason the solve runs on a scratch
//      pose and a transient chain (design note §6.1);
//   4. an unresolvable chain returns false and writes NOTHING — seven separate
//      refusals, each checked against a pre-filled sentinel output;
//   5. the chain walk from the selected bone goes UP at most three bones, stops
//      at a root, and refuses a root effector;
//   6. an IK-posed limb baked to keys reproduces the same pose when the clip is
//      sampled with no solver anywhere near it;
//   7. the bake is ONE undo step.
//
// ★ WHY 1 AND 2 ARE WORTH WRITING SEPARATELY. They fail differently. A
// divergent unreachable solve produces infinities or a limb shot off to
// nowhere — loud. A rotation extracted in the wrong FRAME (model instead of
// parent-local) produces a pose that is exactly right whenever the parent is
// unrotated and progressively wrong as it turns, which is the shape of defect
// that survives every screenshot.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the duplicate-bone-name rig asserts ON PURPOSE
#include "Flux/MeshAnimation/Flux_AnimationController.h"

#include <cstring>
#include <filesystem>

namespace
{
	constexpr float fPOSE_IK_EPSILON = 1.0e-3f;

	//--------------------------------------------------------------------------
	// A quaternion recovered through a composition is free to come back as
	// either member of the double cover, so the sign is aligned before the
	// componentwise compare — which keeps the tolerance meaning "this many units
	// of quaternion" rather than the |dot| ~ 1 test whose sensitivity falls off
	// as the square of the angle error.
	//--------------------------------------------------------------------------
	bool PoseIKRotationsNear(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB, float fEpsilon)
	{
		const Zenith_Maths::Quat xAligned = (glm::dot(xA, xB) < 0.0f) ? -xB : xB;
		return std::fabs(xA.w - xAligned.w) <= fEpsilon
			&& std::fabs(xA.x - xAligned.x) <= fEpsilon
			&& std::fabs(xA.y - xAligned.y) <= fEpsilon
			&& std::fabs(xA.z - xAligned.z) <= fEpsilon;
	}

	bool PoseIKVectorsNear(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fEpsilon)
	{
		return std::fabs(xA.x - xB.x) <= fEpsilon
			&& std::fabs(xA.y - xB.y) <= fEpsilon
			&& std::fabs(xA.z - xB.z) <= fEpsilon;
	}

	// Same expression as the module's own PoseIKIsFinite and as
	// Zenith_BonePickGeometry's — see the comment there for why it is not
	// std::isfinite. Spelled out again rather than reaching into the anonymous
	// namespace above it, so the assertion does not check the implementation
	// against a helper the implementation could change underneath it.
	bool PoseIKComponentIsFinite(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	bool PoseIKVectorIsFinite(const Zenith_Maths::Vector3& xValue)
	{
		return PoseIKComponentIsFinite(xValue.x)
			&& PoseIKComponentIsFinite(xValue.y)
			&& PoseIKComponentIsFinite(xValue.z);
	}

	void PoseIKSnapshotBytes(const void* pData, size_t ulSize, Zenith_Vector<uint8_t>& axOut)
	{
		axOut.Clear();
		axOut.Resize(static_cast<u_int>(ulSize));
		memcpy(axOut.GetDataPointer(), pData, ulSize);
	}

	bool PoseIKBytesEqual(const Zenith_Vector<uint8_t>& axA, const Zenith_Vector<uint8_t>& axB)
	{
		if (axA.GetSize() != axB.GetSize())
		{
			return false;
		}
		return axA.GetSize() == 0u
			|| memcmp(axA.GetDataPointer(), axB.GetDataPointer(), axA.GetSize()) == 0;
	}

	//--------------------------------------------------------------------------
	// A skeleton asset plus a live instance over it — the shape
	// Zenith_BonePickGeometry.Tests.inl and Zenith_BoneSpace.Tests.inl both use.
	//
	// ★ THE ASSET IS DECLARED FIRST SO IT IS DESTROYED LAST: the instance holds
	// an owning handle on it, so the instance has to go first. The destructor
	// does that explicitly and the declaration order is the belt to that brace.
	// Nothing here touches the asset REGISTRY — Zenith_Asset::Release only
	// decrements, so a stack asset is safe to hand to an owning handle.
	//--------------------------------------------------------------------------
	struct PoseIKRig
	{
		Zenith_SkeletonAsset m_xAsset;
		Flux_SkeletonInstance* m_pxInstance = nullptr;

		PoseIKRig() = default;

		~PoseIKRig()
		{
			if (m_pxInstance != nullptr)
			{
				m_pxInstance->Destroy();
				delete m_pxInstance;
				m_pxInstance = nullptr;
			}
		}

		PoseIKRig(const PoseIKRig&) = delete;
		PoseIKRig& operator=(const PoseIKRig&) = delete;

		void AddBone(const char* szName, int32_t iParent, float fX, float fY, float fZ)
		{
			m_xAsset.AddBone(szName, iParent, Zenith_Maths::Vector3(fX, fY, fZ),
				glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));
		}

		void Finalise()
		{
			m_xAsset.ComputeBindPoseMatrices();
			m_pxInstance = Flux_SkeletonInstance::CreateFromAsset(&m_xAsset);
		}
	};

	// The workhorse rig: a straight chain up +Y with unit bones. AddBone takes a
	// LOCAL offset, so (0,1,0) twice puts the joints at MODEL (0,0,0), (0,1,0)
	// and (0,2,0) — a total chain length of exactly 2.0, which is what makes
	// "clamped to the chain's length" a number the test can name.
	void PoseIKBuildThreeBoneChain(PoseIKRig& xRig)
	{
		xRig.AddBone("Root", Zenith_SkeletonAsset::INVALID_BONE_INDEX, 0.0f, 0.0f, 0.0f);
		xRig.AddBone("Mid", 0, 0.0f, 1.0f, 0.0f);
		xRig.AddBone("Tip", 1, 0.0f, 1.0f, 0.0f);
		xRig.Finalise();
	}

	Zenith_Vector<u_int> PoseIKChain(u_int uA, u_int uB, u_int uC)
	{
		Zenith_Vector<u_int> auChain;
		auChain.PushBack(uA);
		auChain.PushBack(uB);
		auChain.PushBack(uC);
		return auChain;
	}

	//--------------------------------------------------------------------------
	// ★ THE INDEPENDENT ORACLE. The solved local rotations are written into a
	// SECOND instance over the same asset and composed by
	// Flux_SkeletonInstance::ComputeSkinningMatrices — a different composition
	// path from the Flux_SkeletonPose the solve ran on. If the test re-used the
	// solve's own scratch pose, "the rotations reproduce the joints" would be an
	// expression compared with itself and would stay green through a change to
	// the frame the rotations are expressed in, which is exactly the defect
	// worth catching.
	//
	// Writes one model-space joint POSITION per chain entry, in chain order.
	//--------------------------------------------------------------------------
	bool PoseIKRecomposeJoints(Zenith_SkeletonAsset& xAsset,
		const Flux_SkeletonInstance& xSource,
		const Zenith_Vector<u_int>& auChain,
		const Zenith_Vector<Zenith_Maths::Quat>& axRotations,
		Zenith_Vector<Zenith_Maths::Vector3>& axOutJoints)
	{
		axOutJoints.Clear();

		Flux_SkeletonInstance* pxFresh = Flux_SkeletonInstance::CreateFromAsset(&xAsset);
		if (pxFresh == nullptr)
		{
			return false;
		}

		for (u_int u = 0; u < xSource.GetNumBones(); ++u)
		{
			pxFresh->SetBoneLocalTransform(u, xSource.GetBoneLocalPosition(u),
				xSource.GetBoneLocalRotation(u), xSource.GetBoneLocalScale(u));
		}
		for (u_int u = 0; u < auChain.GetSize(); ++u)
		{
			const u_int uBone = auChain.Get(u);
			// Copied out first: the getters hand back references INTO the arrays
			// the setter writes, and aliasing an argument with its destination is
			// a hazard nobody would think to look for later.
			const Zenith_Maths::Vector3 xPosition = pxFresh->GetBoneLocalPosition(uBone);
			const Zenith_Maths::Vector3 xScale = pxFresh->GetBoneLocalScale(uBone);
			pxFresh->SetBoneLocalTransform(uBone, xPosition, axRotations.Get(u), xScale);
		}
		pxFresh->ComputeSkinningMatrices();

		for (u_int u = 0; u < auChain.GetSize(); ++u)
		{
			axOutJoints.PushBack(Zenith_Maths::Vector3(pxFresh->GetBoneModelTransform(auChain.Get(u))[3]));
		}

		pxFresh->Destroy();
		delete pxFresh;
		return true;
	}

	//--------------------------------------------------------------------------
	// Document fixture — the shape Zenith_AnimationDocument.Tests.inl
	// established: a private temp directory removed on the way out, plus a
	// ForceUnload so a throwaway asset never lingers in the live registry the
	// suite runs inside.
	//
	// ★ DECLARE THE FIXTURE BEFORE THE DOCUMENT IN EVERY TEST. The document
	// holds an OWNING asset handle and ForceUnload deletes regardless of
	// refcount, so the document has to be destroyed first.
	//--------------------------------------------------------------------------
	struct PoseIKDocFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;

		explicit PoseIKDocFixture(const char* szLeafDirectory)
		{
			std::error_code xError;
			std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xRoot = ".";
			}
			m_xDirectory = xRoot / szLeafDirectory;
			std::filesystem::remove_all(m_xDirectory, xError);
			std::filesystem::create_directories(m_xDirectory, xError);
			m_strPath = (m_xDirectory / "poseik.zanim").generic_string();
		}

		~PoseIKDocFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		PoseIKDocFixture(const PoseIKDocFixture&) = delete;
		PoseIKDocFixture& operator=(const PoseIKDocFixture&) = delete;
	};

	// A probe clip carrying ONE key, on a track the bake does not touch, whose
	// value equals the rig's bind value. So sampling the clip before the bake
	// reproduces the bind pose exactly, and every difference afterwards is
	// something the bake wrote.
	void PoseIKWriteProbeClip(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("PoseIKProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;
		xClip.GetMetadata().m_bGenerated = false;

		Flux_BoneChannel xRoot;
		xRoot.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		// Sorted before it is handed over, matching Zenith_AnimationDocument's own
		// fixture. Flux_AssertTrackSorted compares adjacent PAIRS, so a one-key
		// track cannot trip it today — but "a hand-appended channel that was never
		// sorted asserts at the first mutation" is the contract, and a fixture that
		// happens to be under the threshold is not the same as one that keeps it.
		xRoot.SortKeyframes();
		xClip.AddBoneChannel("Root", std::move(xRoot));

		xClip.Export(strPath);
	}

	u_int PoseIKRotationKeyCount(const Zenith_AnimationDocument& xDocument, const char* szBone)
	{
		return xDocument.GetKeyCount(Zenith_AnimTrackId::Bone(szBone, FLUX_ANIM_TRACK_ROTATION));
	}
}

//==============================================================================
// (1) An UNREACHABLE target clamps to the chain's total length rather than
//     diverging.
//
// The chain is 2.0 long and the target sits 5.0 away, so the only correct
// answer is "fully extended, pointing at it". The two things that go wrong here
// go wrong differently: a divergent solve produces infinities (caught by the
// finiteness assertions), and a solve that keeps iterating against an
// unreachable target oscillates and lands somewhere short and off-axis (caught
// by the extension and direction assertions).
//==============================================================================
ZENITH_TEST(PoseIK, UnreachableTargetClampsToChainLength)
{
	PoseIKRig xRig;
	PoseIKBuildThreeBoneChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig built (else every assertion below is vacuous)");

	// 3-4-5: exactly 5.0 from the root, against a 2.0 chain.
	const Zenith_Maths::Vector3 xTarget(3.0f, 4.0f, 0.0f);

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
	xRequest.m_xTargetModelSpace = xTarget;

	Zenith_Vector<Zenith_Maths::Quat> axRotations;
	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
		*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axRotations), "an unreachable target still SOLVES");
	ZENITH_ASSERT_EQ(axRotations.GetSize(), 3u, "one local rotation per chain bone, in chain order");

	Zenith_Vector<Zenith_Maths::Vector3> axJoints;
	ZENITH_ASSERT_TRUE(PoseIKRecomposeJoints(xRig.m_xAsset, *xRig.m_pxInstance,
		xRequest.m_auChainBoneIndices, axRotations, axJoints), "the rotations recompose");
	ZENITH_ASSERT_EQ(axJoints.GetSize(), 3u, "three joints came back");

	for (u_int u = 0; u < axJoints.GetSize(); ++u)
	{
		ZENITH_ASSERT_TRUE(PoseIKVectorIsFinite(axJoints.Get(u)),
			"every joint is FINITE - a diverged FABRIK is infinities, not a wrong number");
	}

	const Zenith_Maths::Vector3 xRoot = axJoints.Get(0);
	const Zenith_Maths::Vector3 xEffector = axJoints.Get(2);
	ZENITH_ASSERT_TRUE(PoseIKVectorsNear(xRoot, Zenith_Maths::Vector3(0.0f), fPOSE_IK_EPSILON),
		"the ROOT joint does not move - FABRIK pins it and so must the recompose");

	// Full extension: exactly the chain's total length, not more and not less.
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xEffector - xRoot), 2.0f, fPOSE_IK_EPSILON,
		"the effector sits at the chain's TOTAL LENGTH from the root");

	// ...and on the line toward the target, which is the half a bare distance
	// check cannot see: a chain that reached full extension in the wrong
	// direction passes the length assertion perfectly.
	const Zenith_Maths::Vector3 xToTarget = glm::normalize(xTarget - xRoot);
	const Zenith_Maths::Vector3 xToEffector = glm::normalize(xEffector - xRoot);
	ZENITH_ASSERT_TRUE(PoseIKVectorsNear(xToEffector, xToTarget, fPOSE_IK_EPSILON),
		"pointing straight AT the target (0.6, 0.8, 0)");

	// The bone lengths survive: a stretch is not a licence to lengthen the limb.
	ZENITH_ASSERT_EQ_FLOAT(glm::length(axJoints.Get(1) - axJoints.Get(0)), 1.0f, fPOSE_IK_EPSILON,
		"the upper bone kept its length");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(axJoints.Get(2) - axJoints.Get(1)), 1.0f, fPOSE_IK_EPSILON,
		"and so did the lower one");
}

//==============================================================================
// (2) A solved chain's LOCAL rotations reproduce the solved MODEL-SPACE joints.
//
// The target is reachable (sqrt(2) against a 2.0 chain) and off the chain's own
// axis, so the answer needs a real bend: the correct solution puts the elbow
// where it already is and turns the forearm 90 degrees, and NOTHING about that
// is reachable by leaving the rotations alone.
//==============================================================================
ZENITH_TEST(PoseIK, SolvedLocalRotationsReproduceTheSolvedJointPositions)
{
	PoseIKRig xRig;
	PoseIKBuildThreeBoneChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig built");

	const Zenith_Maths::Vector3 xTarget(1.0f, 1.0f, 0.0f);

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
	xRequest.m_xTargetModelSpace = xTarget;

	Zenith_Vector<Zenith_Maths::Quat> axRotations;
	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
		*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axRotations), "a reachable target solves");

	// The solve is not vacuous: at least one bone actually turned.
	bool bAnyRotationMoved = false;
	for (u_int u = 0; u < axRotations.GetSize(); ++u)
	{
		if (!PoseIKRotationsNear(axRotations.Get(u),
			xRig.m_pxInstance->GetBoneLocalRotation(xRequest.m_auChainBoneIndices.Get(u)), fPOSE_IK_EPSILON))
		{
			bAnyRotationMoved = true;
		}
	}
	ZENITH_ASSERT_TRUE(bAnyRotationMoved,
		"the chain MOVED - an all-identity result would pass every geometric check below by accident");

	Zenith_Vector<Zenith_Maths::Vector3> axJoints;
	ZENITH_ASSERT_TRUE(PoseIKRecomposeJoints(xRig.m_xAsset, *xRig.m_pxInstance,
		xRequest.m_auChainBoneIndices, axRotations, axJoints), "the rotations recompose");

	ZENITH_ASSERT_TRUE(PoseIKVectorsNear(axJoints.Get(0), Zenith_Maths::Vector3(0.0f), fPOSE_IK_EPSILON),
		"the root stayed put");
	ZENITH_ASSERT_TRUE(PoseIKVectorsNear(axJoints.Get(2), xTarget, fPOSE_IK_EPSILON),
		"and the EFFECTOR reached the target - composed by Flux_SkeletonInstance, not by the pose the solve used");

	ZENITH_ASSERT_EQ_FLOAT(glm::length(axJoints.Get(1) - axJoints.Get(0)), 1.0f, fPOSE_IK_EPSILON,
		"with the upper bone's length intact");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(axJoints.Get(2) - axJoints.Get(1)), 1.0f, fPOSE_IK_EPSILON,
		"and the lower one's");

	// Every rotation is normalised on the way out, so a caller can hand one
	// straight to Flux_BoneChannel (which refuses a short quaternion, D15).
	for (u_int u = 0; u < axRotations.GetSize(); ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(glm::length(axRotations.Get(u)), 1.0f, 1.0e-4f,
			"every published rotation is unit length");
	}
}

//==============================================================================
// (3) A controller's output pose is BYTE-unchanged across a solve, and so is
//     the instance the solve read.
//
// This is the one assertion that pins design note §6.1. The whole reason the
// helper builds its own Flux_SkeletonPose, its own Flux_IKChain and its own
// Flux_IKSolver is so that a controller sitting on the same skeleton is not
// solved a second time — and "a second solve happened" is invisible in a
// screenshot and invisible in the returned rotations.
//==============================================================================
ZENITH_TEST(PoseIK, TheControllerPoseIsUnchangedAcrossASolve)
{
	PoseIKRig xRig;
	PoseIKBuildThreeBoneChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig built");

	// A SEPARATE, never-solved instance over the same asset: the control.
	PoseIKRig xUntouched;
	PoseIKBuildThreeBoneChain(xUntouched);
	ZENITH_ASSERT_NOT_NULL(xUntouched.m_pxInstance, "the control rig built");

	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);
	ZENITH_ASSERT_TRUE(xController.IsInitialized(), "the controller is looking at the same skeleton the solve reads");

	Zenith_Vector<uint8_t> axPoseBefore;
	PoseIKSnapshotBytes(&xController.GetOutputPose(), sizeof(Flux_SkeletonPose), axPoseBefore);

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
	xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);

	Zenith_Vector<Zenith_Maths::Quat> axRotations;
	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
		*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axRotations), "the solve runs");

	Zenith_Vector<uint8_t> axPoseAfter;
	PoseIKSnapshotBytes(&xController.GetOutputPose(), sizeof(Flux_SkeletonPose), axPoseAfter);
	ZENITH_ASSERT_TRUE(PoseIKBytesEqual(axPoseBefore, axPoseAfter),
		"the CONTROLLER'S output pose is byte-identical - the solve never touched it");

	// The source instance is const to the helper, so its live pose must not have
	// moved either: applying the result is the CALLER'S decision (and the
	// panel's, on release), not the solver's.
	for (u_int u = 0; u < xRig.m_pxInstance->GetNumBones(); ++u)
	{
		ZENITH_ASSERT_TRUE(PoseIKRotationsNear(xRig.m_pxInstance->GetBoneLocalRotation(u),
			xUntouched.m_pxInstance->GetBoneLocalRotation(u), 0.0f),
			"the solved-from instance still matches an instance nothing solved");
		ZENITH_ASSERT_TRUE(PoseIKVectorsNear(xRig.m_pxInstance->GetBoneLocalPosition(u),
			xUntouched.m_pxInstance->GetBoneLocalPosition(u), 0.0f),
			"positions too - a solve writes rotations to its OUTPUT, never to its input");
	}

	// ...and the solve was not a no-op, which is what makes the two assertions
	// above mean something.
	ZENITH_ASSERT_FALSE(PoseIKRotationsNear(axRotations.Get(1),
		xRig.m_pxInstance->GetBoneLocalRotation(1u), fPOSE_IK_EPSILON),
		"the RETURNED middle rotation differs from the live one - there was something to leak");
}

//==============================================================================
// (4) Eight refusals, and every one of them writes NOTHING.
//
// The output vector is pre-filled with a recognisable sentinel before each
// call, so "wrote nothing" is checked rather than assumed. A helper that
// cleared its output on the way in and then refused would look identical from
// the return value alone, and would silently destroy a caller's previous
// result.
//
// ★ SEVEN OF THE EIGHT ARE PURE INPUT VALIDATION and reach no engine code at
// all — that is deliberate, and it is the reason this test can hand
// SolveChainToLocalRotations a bone index the rig does not have without
// anything downstream indexing past an array. The EIGHTH builds a genuinely
// malformed rig, and building it makes the ASSET assert; see the capture scope
// at the bottom for why that one line has to say so out loud.
//==============================================================================
ZENITH_TEST(PoseIK, AnUnresolvableChainReturnsFalseAndWritesNothing)
{
	PoseIKRig xRig;
	PoseIKBuildThreeBoneChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig built");

	const Zenith_Maths::Quat xSentinel = glm::angleAxis(0.5f, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Zenith_Vector<Zenith_Maths::Quat> axOut;
	axOut.PushBack(xSentinel);

	// (a) a bone index the rig does not have.
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 9u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "an out-of-range bone index is refused");
	}

	// (b) fewer than two bones - there is no bone to bend.
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices.PushBack(1u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "a one-bone chain is refused");
	}

	// (c) an empty chain.
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "an empty chain is refused");
	}

	// (d) the same bone twice. FABRIK would measure a zero-length segment and
	//     the rotation extraction would divide by it.
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 1u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "a repeated bone is refused");
	}

	// (e) a non-finite target. A NaN here reaches every bone in the chain
	//     through the normalize, and from there a key, and from there every
	//     pose the clip can produce at every time.
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(std::nanf(""), 0.0f, 0.0f);
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "a NaN target is refused");
	}

	// (f) a weight outside [0, 1].
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);
		xRequest.m_fWeight = 1.5f;
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "a weight above 1 is refused");
	}

	// (g) a constraint list that does not cover the chain. Flux_IKSolver indexes
	//     it POSITIONALLY, so a short list constrains the wrong joints rather
	//     than fewer of them.
	{
		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);
		xRequest.m_axJointConstraints.PushBack(Flux_JointConstraint());
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axOut), "a short constraint list is refused");
	}

	ZENITH_ASSERT_EQ(axOut.GetSize(), 1u, "seven refusals later the output vector is untouched");
	ZENITH_ASSERT_TRUE(PoseIKRotationsNear(axOut.Get(0), xSentinel, 0.0f),
		"...still holding the sentinel it went in with");

	// ★ A NAME COLLISION IS A REFUSAL TOO, and it is the reason the helper
	// resolves its own names back to indices instead of trusting them. Two bones
	// called "Dup" make Zenith_SkeletonAsset::GetBoneIndex answer with one of
	// them for both, and the solve would bend a limb the caller did not name.
	{
		PoseIKRig xCollide;
		xCollide.AddBone("Root", Zenith_SkeletonAsset::INVALID_BONE_INDEX, 0.0f, 0.0f, 0.0f);
		xCollide.AddBone("Dup", 0, 0.0f, 1.0f, 0.0f);

		// ★★ CAPTURED, AND THE SCOPE IS EXACTLY ONE CALL WIDE.
		// Zenith_SkeletonAsset::AddBone asserts "Duplicate bone name", and an
		// UNCAPTURED Zenith_Assert TERMINATES THE PROCESS in the Null config every
		// gate runs — so this line aborted the whole suite mid-run, with no "Unit
		// tests complete" line, exit 3, and every test after it unreported. A
		// deliberately malformed fixture has to say so to the assert hook, not
		// just to the reader.
		//
		// The scope covers the offending call and nothing else: Finalise() and the
		// solve below run OUTSIDE it, so a surprise assert in either is still loud
		// rather than swallowed by a capture left open for convenience.
		{
			Zenith_AssertCaptureScope xCapture;
			xCollide.AddBone("Dup", 1, 0.0f, 1.0f, 0.0f);
			ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u,
				"the ASSET itself objects, exactly once - which is the hazard this rig exists to build");
		}

		xCollide.Finalise();
		ZENITH_ASSERT_NOT_NULL(xCollide.m_pxInstance, "the colliding rig built");

		Zenith_AnimationPoseIK::SolveRequest xRequest;
		xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
		xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);

		Zenith_Vector<Zenith_Maths::Quat> axCollideOut;
		ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
			*xCollide.m_pxInstance, xCollide.m_xAsset, xRequest, axCollideOut),
			"two bones with one name is refused rather than silently retargeted");
		ZENITH_ASSERT_EQ(axCollideOut.GetSize(), 0u, "and nothing was written");
	}
}

//==============================================================================
// (5) The chain the panel builds: the SELECTED bone is the effector, and the
//     walk goes UP at most kuIK_DEFAULT_CHAIN_LENGTH bones, or fewer to a root.
//==============================================================================
ZENITH_TEST(PoseIK, ChainFromEffectorWalksUpFromTheSelectedBone)
{
	PoseIKRig xRig;
	xRig.AddBone("Root", Zenith_SkeletonAsset::INVALID_BONE_INDEX, 0.0f, 0.0f, 0.0f);
	xRig.AddBone("A", 0, 0.0f, 1.0f, 0.0f);
	xRig.AddBone("B", 1, 0.0f, 1.0f, 0.0f);
	xRig.AddBone("C", 2, 0.0f, 1.0f, 0.0f);
	xRig.Finalise();
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the four-bone rig built");

	Zenith_Vector<u_int> auChain;

	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::BuildChainFromEffector(xRig.m_xAsset, 3u,
		Zenith_AnimationPoseIK::kuIK_DEFAULT_CHAIN_LENGTH, auChain), "a deep bone gives a full chain");
	ZENITH_ASSERT_EQ(auChain.GetSize(), 3u, "three bones, not four");
	ZENITH_ASSERT_EQ(auChain.Get(0), 1u, "ROOT FIRST - the grandparent");
	ZENITH_ASSERT_EQ(auChain.Get(1), 2u, "then the parent");
	ZENITH_ASSERT_EQ(auChain.Get(2), 3u, "and the SELECTED bone is the EFFECTOR, last");

	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::BuildChainFromEffector(xRig.m_xAsset, 1u,
		Zenith_AnimationPoseIK::kuIK_DEFAULT_CHAIN_LENGTH, auChain), "a bone one level down still works");
	ZENITH_ASSERT_EQ(auChain.GetSize(), 2u, "with FEWER bones, because the walk hit the root");
	ZENITH_ASSERT_EQ(auChain.Get(0), 0u, "the root");
	ZENITH_ASSERT_EQ(auChain.Get(1), 1u, "then the effector");

	// Refusals leave the previous answer alone, same contract as the solve.
	ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::BuildChainFromEffector(xRig.m_xAsset, 0u,
		Zenith_AnimationPoseIK::kuIK_DEFAULT_CHAIN_LENGTH, auChain),
		"a ROOT effector is refused - there is no bone above it to bend");
	ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::BuildChainFromEffector(xRig.m_xAsset, 9u,
		Zenith_AnimationPoseIK::kuIK_DEFAULT_CHAIN_LENGTH, auChain), "so is an index the rig does not have");
	ZENITH_ASSERT_FALSE(Zenith_AnimationPoseIK::BuildChainFromEffector(xRig.m_xAsset, 3u, 1u, auChain),
		"and so is a maximum length below two");
	ZENITH_ASSERT_EQ(auChain.GetSize(), 2u, "and none of the three touched the output");
	ZENITH_ASSERT_EQ(auChain.Get(1), 1u, "...which still holds the previous chain");
}

//==============================================================================
// (6) An IK-posed limb baked to keys reproduces the same pose when the clip is
//     sampled with no solver anywhere near it.
//
// ★ THIS IS THE ACCEPTANCE CRITERION FOR "BAKED DOWN TO KEYS". The clip is
// sampled through Flux_SkeletonPose::SampleFromClip — the ordinary playback
// path, which has never heard of IK — and the resulting model-space effector is
// compared against the target the solve was given. If the bake wrote the wrong
// frame, the wrong track, or rotations in the wrong space, this is where it
// shows up.
//==============================================================================
ZENITH_TEST(PoseIK, BakedKeysReproduceTheIKPoseWithNoSolver)
{
	PoseIKDocFixture xFixture("zenith_poseik_bake");
	PoseIKWriteProbeClip(xFixture.m_strPath);

	PoseIKRig xRig;
	PoseIKBuildThreeBoneChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig built");

	Zenith_AnimationDocument xDocument;
	ZENITH_ASSERT_TRUE(xDocument.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe clip opens");
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Mid"), 0u, "and carries no rotation keys yet");

	const Zenith_Maths::Vector3 xTarget(1.0f, 1.0f, 0.0f);

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
	xRequest.m_xTargetModelSpace = xTarget;

	Zenith_Vector<Zenith_Maths::Quat> axRotations;
	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
		*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axRotations), "the solve runs");

	const u_int uFrameRate = 30u;
	const float fRequestedTime = 0.5f;
	const float fSnapped = Zenith_AnimTimelineSnapToFrame(fRequestedTime, uFrameRate);

	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::BakeChain(xDocument, xRig.m_xAsset,
		xRequest.m_auChainBoneIndices, axRotations, fRequestedTime, uFrameRate), "the bake takes");
	ZENITH_ASSERT_TRUE(xDocument.IsDirty(), "and dirties the document");

	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Root"), 1u, "one rotation key on the root of the chain");
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Mid"), 1u, "one on the middle joint");
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Tip"), 1u, "one on the effector");
	ZENITH_ASSERT_EQ(xDocument.GetKeyCount(Zenith_AnimTrackId::Bone("Mid", FLUX_ANIM_TRACK_POSITION)), 0u,
		"and NOTHING on translation - Action_SetKeyForBones' asymmetry, honoured here too");

	// The key landed on the frame grid, not at the raw request.
	float fKeyTime = -1.0f;
	const Zenith_AnimTrackId xMidRotation = Zenith_AnimTrackId::Bone("Mid", FLUX_ANIM_TRACK_ROTATION);
	ZENITH_ASSERT_TRUE(xDocument.GetKeyTime(xMidRotation, xDocument.GetKeyIdAtIndex(xMidRotation, 0u), fKeyTime),
		"the key resolves");
	ZENITH_ASSERT_EQ_FLOAT(fKeyTime, fSnapped, 1.0e-6f, "at the SNAPPED time, through the one grid definition");

	//-------------------------------------------------------------------------
	// Sample the clip with the ordinary playback path and rebuild the pose.
	//-------------------------------------------------------------------------
	Flux_SkeletonPose xSampled;
	xSampled.InitFromBindPose(xRig.m_xAsset);
	xSampled.SampleFromClip(xDocument.GetClip(), fSnapped, xRig.m_xAsset);

	for (u_int u = 0; u < xRequest.m_auChainBoneIndices.GetSize(); ++u)
	{
		const u_int uBone = xRequest.m_auChainBoneIndices.Get(u);
		ZENITH_ASSERT_TRUE(PoseIKRotationsNear(xSampled.GetLocalPose(uBone).m_xRotation,
			axRotations.Get(u), fPOSE_IK_EPSILON),
			"the SAMPLED local rotation is the SOLVED one - the bake round-tripped through the clip");
	}

	xSampled.ComputeModelSpaceMatricesFromSkeleton(xRig.m_xAsset);
	const Zenith_Maths::Vector3 xSampledEffector(xSampled.GetModelSpaceMatrix(2u)[3]);
	ZENITH_ASSERT_TRUE(PoseIKVectorsNear(xSampledEffector, xTarget, fPOSE_IK_EPSILON),
		"and the clip ALONE, with IK nowhere in the picture, puts the effector on the target");
}

//==============================================================================
// (7) The bake is ONE undo step, and undoing it restores the pre-bake state.
//
// A three-bone chain writes three keys; three Ctrl+Z presses to walk back
// through a single gesture is exactly the failure Zenith_AnimationDocument's
// compound bracket exists to prevent.
//==============================================================================
ZENITH_TEST(PoseIK, BakingAChainIsOneUndoStep)
{
	PoseIKDocFixture xFixture("zenith_poseik_undo");
	PoseIKWriteProbeClip(xFixture.m_strPath);

	PoseIKRig xRig;
	PoseIKBuildThreeBoneChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig built");

	Zenith_AnimationDocument xDocument;
	ZENITH_ASSERT_TRUE(xDocument.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe clip opens");
	ZENITH_ASSERT_EQ(xDocument.GetUndoStackSize(), 0u, "with an empty history");

	const u_int uPositionKeysBefore = xDocument.GetKeyCount(
		Zenith_AnimTrackId::Bone("Root", FLUX_ANIM_TRACK_POSITION));
	ZENITH_ASSERT_EQ(uPositionKeysBefore, 1u, "the probe's one position key is there");

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = PoseIKChain(0u, 1u, 2u);
	xRequest.m_xTargetModelSpace = Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f);

	Zenith_Vector<Zenith_Maths::Quat> axRotations;
	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::SolveChainToLocalRotations(
		*xRig.m_pxInstance, xRig.m_xAsset, xRequest, axRotations), "the solve runs");

	ZENITH_ASSERT_TRUE(Zenith_AnimationPoseIK::BakeChain(xDocument, xRig.m_xAsset,
		xRequest.m_auChainBoneIndices, axRotations, 0.5f, 30u), "the bake takes");
	ZENITH_ASSERT_EQ(xDocument.GetUndoStackSize(), 1u,
		"THREE keys, ONE undo entry - the whole IK gesture is one Ctrl+Z");

	xDocument.Undo();
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Root"), 0u, "the root's rotation key is gone");
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Mid"), 0u, "and the middle joint's");
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Tip"), 0u, "and the effector's");
	ZENITH_ASSERT_EQ(xDocument.GetKeyCount(Zenith_AnimTrackId::Bone("Root", FLUX_ANIM_TRACK_POSITION)),
		uPositionKeysBefore, "while the key the bake never touched is exactly where it was");

	// A sample of the undone clip is the BIND pose again: the undo did not merely
	// remove the ids, it removed what they addressed.
	Flux_SkeletonPose xSampled;
	xSampled.InitFromBindPose(xRig.m_xAsset);
	xSampled.SampleFromClip(xDocument.GetClip(), 0.5f, xRig.m_xAsset);
	ZENITH_ASSERT_TRUE(PoseIKRotationsNear(xSampled.GetLocalPose(1u).m_xRotation,
		glm::identity<Zenith_Maths::Quat>(), fPOSE_IK_EPSILON),
		"the undone clip samples back to the bind rotation");

	xDocument.Redo();
	ZENITH_ASSERT_EQ(PoseIKRotationKeyCount(xDocument, "Mid"), 1u, "and the redo puts the whole chain back at once");
	ZENITH_ASSERT_EQ(xDocument.GetUndoStackSize(), 1u, "as one entry again");
}
