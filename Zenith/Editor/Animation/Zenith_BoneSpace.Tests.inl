//------------------------------------------------------------------------------
// Zenith_BoneSpace unit tests (WU-4.2).
// Included at the bottom of Zenith_BoneSpace.cpp.
//
// ★ WHY THESE ARE WORTH WRITING. A space-conversion defect does not crash and
// does not draw wrong: it drags the bone somewhere plausible. Every one of the
// four ways to get §3.4 wrong — conjugating the wrong way round, multiplying the
// delta on the right, folding in the bone's own rotation instead of its parent's,
// or reaching for the inverse bind pose — produces a manipulator that behaves
// perfectly at an identity parent pose and drifts progressively as the parent
// rotates. There is no screenshot that shows that and no gate that fails on it.
// So the parent chain in the fixture is THREE levels deep with a rotated,
// translated, non-unit-scale root, and every rotation property is asserted as an
// equation rather than eyeballed.
//
// All of it is CPU-only: a skeleton built in code, a Flux_SkeletonInstance
// (which owns no device resources at all — its Destroy clears an asset handle and
// a bone count), and arithmetic. No device, no view, no window, no files, no
// registry. NONE of these is requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"

#include <cmath>

namespace
{
	//--------------------------------------------------------------------------
	// Comparison helpers. Two different quaternion comparisons on purpose:
	//
	//  - ComponentsNear is for an EQUATION this file claims verbatim (the
	//    conjugation formula, the entity-gizmo equivalence). Both sides are built
	//    the same way, so a sign flip there would be a real disagreement.
	//  - RotationsNear is for a rotation recovered through glm::quat_cast, which
	//    is free to return either member of the double cover. Comparing those
	//    componentwise is a coin flip, so the sign is aligned first and only then
	//    compared componentwise — which keeps the tolerance meaning "this many
	//    units of quaternion", instead of the |dot| ~ 1 test whose sensitivity
	//    falls off as the square of the angle error.
	//--------------------------------------------------------------------------
	constexpr float fBONE_SPACE_TEST_EPSILON = 1.0e-4f;

	bool BoneSpaceQuatComponentsNear(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB, float fEpsilon)
	{
		return std::fabs(xA.w - xB.w) <= fEpsilon
			&& std::fabs(xA.x - xB.x) <= fEpsilon
			&& std::fabs(xA.y - xB.y) <= fEpsilon
			&& std::fabs(xA.z - xB.z) <= fEpsilon;
	}

	bool BoneSpaceRotationsNear(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB, float fEpsilon)
	{
		const Zenith_Maths::Quat xAligned = (glm::dot(xA, xB) < 0.0f) ? -xB : xB;
		return BoneSpaceQuatComponentsNear(xA, xAligned, fEpsilon);
	}

	bool BoneSpaceMatricesNear(const Zenith_Maths::Matrix4& xA, const Zenith_Maths::Matrix4& xB, float fEpsilon)
	{
		for (int iColumn = 0; iColumn < 4; ++iColumn)
		{
			for (int iRow = 0; iRow < 4; ++iRow)
			{
				if (std::fabs(xA[iColumn][iRow] - xB[iColumn][iRow]) > fEpsilon)
				{
					return false;
				}
			}
		}
		return true;
	}

	//--------------------------------------------------------------------------
	// ★ AN INDEPENDENT re-composition of L_i, not a call back into the engine's
	// ComposeTransformMatrix. If the test reused the same function the cache was
	// filled with, "M_i == M_parent * L_i" would be comparing an expression with
	// itself and would stay green through a change to the composition ORDER —
	// which is the one thing about L_i that a pose author can feel.
	//--------------------------------------------------------------------------
	Zenith_Maths::Matrix4 BoneSpaceReferenceTRS(const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation, const Zenith_Maths::Vector3& xScale)
	{
		const Zenith_Maths::Matrix4 xTranslation = glm::translate(glm::identity<Zenith_Maths::Matrix4>(), xPosition);
		const Zenith_Maths::Matrix4 xRotationMatrix = glm::mat4_cast(xRotation);
		const Zenith_Maths::Matrix4 xScaleMatrix = glm::scale(glm::identity<Zenith_Maths::Matrix4>(), xScale);
		return xTranslation * xRotationMatrix * xScaleMatrix;
	}

	// The rotation actually baked into a world matrix, recovered the same way a
	// reader outside this module would recover it. Used to cross-check that
	// ParentWorldRotation(i) * q_i really is the bone's world orientation.
	Zenith_Maths::Quat BoneSpaceRotationOf(const Zenith_Maths::Matrix4& xMatrix)
	{
		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Quat xRotation = glm::identity<Zenith_Maths::Quat>();
		Zenith_Maths::Vector3 xScale(1.0f);
		Zenith_Maths::DecomposeTRS(xMatrix, xPosition, xRotation, xScale);
		return xRotation;
	}

	Zenith_Maths::Quat BoneSpaceAngleAxis(float fRadians, const Zenith_Maths::Vector3& xAxis)
	{
		return glm::angleAxis(fRadians, glm::normalize(xAxis));
	}

	//--------------------------------------------------------------------------
	// Fixture — Root -> Child -> GrandChild, every level non-identity.
	//
	// The root carries a translation, a rotation AND a uniform scale of 1.25:
	// scale propagates into every descendant's model translation, so a conversion
	// that quietly assumed a unit chain shows up here and nowhere else. The scale
	// stays UNIFORM because that is the class of matrix DecomposeTRS is documented
	// for and the class ParentWorldRotation asserts (design note §9.5) — a
	// non-uniform rig is a limitation this module declares, not one it tests.
	//
	// No registry, no files: Flux_SkeletonInstance::CreateFromAsset AddRefs the
	// stack asset and Release just decrements (the registry, not the handle, owns
	// deletion), so a locally-built asset is safe. The instance is deleted in the
	// destructor BODY, which runs before the asset member is destroyed.
	//--------------------------------------------------------------------------
	struct BoneSpaceRig
	{
		Zenith_SkeletonAsset m_xAsset;
		Flux_SkeletonInstance* m_pxInstance = nullptr;

		BoneSpaceRig()
		{
			m_xAsset.AddBone("Root", Zenith_SkeletonAsset::INVALID_BONE_INDEX,
				Zenith_Maths::Vector3(0.25f, 1.10f, -0.40f),
				BoneSpaceAngleAxis(0.60f, Zenith_Maths::Vector3(0.2f, 1.0f, 0.3f)),
				Zenith_Maths::Vector3(1.25f));
			m_xAsset.AddBone("Child", 0,
				Zenith_Maths::Vector3(0.00f, 0.50f, 0.05f),
				BoneSpaceAngleAxis(-0.35f, Zenith_Maths::Vector3(1.0f, 0.2f, -0.4f)),
				Zenith_Maths::Vector3(1.0f));
			m_xAsset.AddBone("GrandChild", 1,
				Zenith_Maths::Vector3(0.00f, 0.40f, 0.10f),
				BoneSpaceAngleAxis(0.90f, Zenith_Maths::Vector3(-0.3f, 0.5f, 1.0f)),
				Zenith_Maths::Vector3(1.0f));
			m_xAsset.ComputeBindPoseMatrices();

			m_pxInstance = Flux_SkeletonInstance::CreateFromAsset(&m_xAsset);
		}

		~BoneSpaceRig()
		{
			delete m_pxInstance;
			m_pxInstance = nullptr;
		}

		BoneSpaceRig(const BoneSpaceRig&) = delete;
		BoneSpaceRig& operator=(const BoneSpaceRig&) = delete;

		// Every pose write is followed by the model-space recompute, exactly as
		// Zenith_AnimationPreviewSession::RefreshDerivedPose does (design note
		// §3.2). A test that skipped it would be reading the previous pose.
		void SetLocalRotation(u_int uBone, const Zenith_Maths::Quat& xRotation)
		{
			const Zenith_Maths::Vector3 xPosition = m_pxInstance->GetBoneLocalPosition(uBone);
			const Zenith_Maths::Vector3 xScale = m_pxInstance->GetBoneLocalScale(uBone);
			m_pxInstance->SetBoneLocalTransform(uBone, xPosition, xRotation, xScale);
			m_pxInstance->ComputeSkinningMatrices();
		}
	};
}

//==============================================================================
// (1) M_i is M_parent(i) * L_i, all the way down, against an independently
// composed reference — and identity for anything out of range.
//==============================================================================
ZENITH_TEST(BoneSpace, ModelMatricesComposeTheParentChain)
{
	BoneSpaceRig xRig;
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the rig must build without a device");
	if (xRig.m_pxInstance == nullptr)
	{
		return;
	}
	const Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;

	Zenith_Maths::Matrix4 axReference[3];
	for (u_int uBone = 0; uBone < 3u; ++uBone)
	{
		const Zenith_Maths::Matrix4 xLocal = BoneSpaceReferenceTRS(
			xSkeleton.GetBoneLocalPosition(uBone),
			xSkeleton.GetBoneLocalRotation(uBone),
			xSkeleton.GetBoneLocalScale(uBone));
		axReference[uBone] = (uBone == 0u) ? xLocal : (axReference[uBone - 1u] * xLocal);
	}

	for (u_int uBone = 0; uBone < 3u; ++uBone)
	{
		ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::BoneModelMatrix(xSkeleton, uBone),
			axReference[uBone], fBONE_SPACE_TEST_EPSILON),
			"M_%u must equal M_parent * L_%u", uBone, uBone);
	}

	// The root's non-unit uniform scale really did reach every descendant — the
	// fixture is exercising what its comment claims, and a conversion that
	// assumed a unit chain has something to be wrong about.
	for (u_int uBone = 0; uBone < 3u; ++uBone)
	{
		const Zenith_Maths::Matrix4 xModel = Zenith_BoneSpace::BoneModelMatrix(xSkeleton, uBone);
		for (int iColumn = 0; iColumn < 3; ++iColumn)
		{
			ZENITH_ASSERT_EQ_FLOAT(glm::length(Zenith_Maths::Vector3(xModel[iColumn])), 1.25f, 1.0e-4f,
				"bone %u column %d must carry the root's uniform scale", uBone, iColumn);
		}
	}

	const Zenith_Maths::Matrix4 xIdentity = glm::identity<Zenith_Maths::Matrix4>();
	ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::BoneModelMatrix(xSkeleton, 3u), xIdentity, 0.0f),
		"one past the last bone is out of range, not a live slot");
	ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::BoneModelMatrix(xSkeleton, 4096u), xIdentity, 0.0f),
		"a wildly out-of-range index must be identity, not a read past the cache");
}

//==============================================================================
// (2) ParentModelMatrix: identity for a root, the parent's M for everyone else.
//==============================================================================
ZENITH_TEST(BoneSpace, ParentModelMatrixIsIdentityForARootBone)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	const Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;
	const Zenith_Maths::Matrix4 xIdentity = glm::identity<Zenith_Maths::Matrix4>();

	// A root's parent frame is identity — which is the whole reason a root-bone
	// drag collapses to the entity-gizmo case in test (4).
	ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::ParentModelMatrix(xSkeleton, 0u), xIdentity, 0.0f),
		"a root bone has no parent transform");

	ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::ParentModelMatrix(xSkeleton, 1u),
		Zenith_BoneSpace::BoneModelMatrix(xSkeleton, 0u), 0.0f),
		"the child's parent frame is the root's model matrix");

	ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::ParentModelMatrix(xSkeleton, 2u),
		Zenith_BoneSpace::BoneModelMatrix(xSkeleton, 1u), 0.0f),
		"the grandchild's parent frame is the child's model matrix, NOT the root's");

	// ...and it is genuinely not the root's, so the assert above is testing
	// something. A one-level-too-shallow walk is the classic parent-chain bug.
	ZENITH_ASSERT_FALSE(BoneSpaceMatricesNear(Zenith_BoneSpace::ParentModelMatrix(xSkeleton, 2u),
		Zenith_BoneSpace::BoneModelMatrix(xSkeleton, 0u), fBONE_SPACE_TEST_EPSILON),
		"the child's and root's model matrices must differ or (2) proves nothing");

	ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::ParentModelMatrix(xSkeleton, 99u), xIdentity, 0.0f),
		"an out-of-range bone has no parent frame");
}

//==============================================================================
// (3) W composes on the LEFT of M_i, and the world position is that product's
// translation column.
//==============================================================================
ZENITH_TEST(BoneSpace, WorldMatrixAndPositionApplyTheSessionModelMatrix)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	const Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;

	const Zenith_Maths::Matrix4 xIdentity = glm::identity<Zenith_Maths::Matrix4>();
	const Zenith_Maths::Matrix4 xSessionModel = BoneSpaceReferenceTRS(
		Zenith_Maths::Vector3(-3.0f, 0.75f, 12.5f),
		BoneSpaceAngleAxis(1.20f, Zenith_Maths::Vector3(0.4f, -0.6f, 0.7f)),
		Zenith_Maths::Vector3(2.0f));

	for (u_int uBone = 0; uBone < 3u; ++uBone)
	{
		const Zenith_Maths::Matrix4 xModel = Zenith_BoneSpace::BoneModelMatrix(xSkeleton, uBone);

		// Identity W is the Phase-4 case and must be a pure pass-through.
		ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(
			Zenith_BoneSpace::BoneWorldMatrix(xIdentity, xSkeleton, uBone), xModel, 0.0f),
			"an identity session matrix must not perturb M_%u", uBone);

		ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(
			Zenith_BoneSpace::BoneWorldMatrix(xSessionModel, xSkeleton, uBone),
			xSessionModel * xModel, fBONE_SPACE_TEST_EPSILON),
			"W must compose on the LEFT of M_%u", uBone);

		const Zenith_Maths::Vector4 xExpectedColumn = xSessionModel * xModel[3];
		ZENITH_ASSERT_NEAR_VEC3(Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, uBone),
			Zenith_Maths::Vector3(xExpectedColumn), fBONE_SPACE_TEST_EPSILON,
			"the world pivot is the translation column of W * M_%u", uBone);
	}
}

//==============================================================================
// (4) A ROOT-BONE DRAG IS THE ENTITY-GIZMO CASE. With W identity the parent
// frame is identity, so the whole conversion has to collapse to exactly what
// Flux_GizmosImpl::ApplyRotation does for an entity:
// newRotation = deltaRotation * initialRotation.
//==============================================================================
ZENITH_TEST(BoneSpace, RootBoneDeltaReproducesTheEntityGizmoResult)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	const Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;
	const Zenith_Maths::Matrix4 xIdentity = glm::identity<Zenith_Maths::Matrix4>();

	const Zenith_Maths::Quat xParentWorld = Zenith_BoneSpace::ParentWorldRotation(xIdentity, xSkeleton, 0u);
	ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(xParentWorld, glm::identity<Zenith_Maths::Quat>(),
		fBONE_SPACE_TEST_EPSILON),
		"with W identity a root bone's parent frame is identity");

	const Zenith_Maths::Vector3 axAxes[3] =
	{
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)
	};
	const float afAngles[3] = { 0.30f, -1.10f, 2.40f };

	const Zenith_Maths::Quat xInitial = xSkeleton.GetBoneLocalRotation(0u);
	for (int iCase = 0; iCase < 3; ++iCase)
	{
		const Zenith_Maths::Quat xDelta = BoneSpaceAngleAxis(afAngles[iCase], axAxes[iCase]);

		// Verbatim the two lines of Flux_Gizmos.cpp ApplyRotation.
		const Zenith_Maths::Quat xEntityResult = xDelta * xInitial;

		const Zenith_Maths::Quat xBoneResult =
			Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(xDelta, xParentWorld, xInitial);

		ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(xBoneResult, xEntityResult, fBONE_SPACE_TEST_EPSILON),
			"a root-bone drag must produce exactly what the entity gizmo would");

		// The local delta itself is the world delta, unconjugated.
		ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(
			Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xDelta, xParentWorld), xDelta,
			fBONE_SPACE_TEST_EPSILON),
			"conjugating by identity is the identity");
	}
}

//==============================================================================
// (5) A ROTATED PARENT CONJUGATES THE DELTA. The formula verbatim, that it is
// genuinely different from the unconjugated delta, and — the property that
// actually matters — that applying it leaves the bone's WORLD orientation turned
// by exactly the world delta.
//==============================================================================
ZENITH_TEST(BoneSpace, ARotatedParentConjugatesTheWorldDelta)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	const Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;
	const Zenith_Maths::Matrix4 xIdentity = glm::identity<Zenith_Maths::Matrix4>();

	const Zenith_Maths::Quat xParentWorld = Zenith_BoneSpace::ParentWorldRotation(xIdentity, xSkeleton, 1u);

	// With W identity, M_parent(1) is M_0 == L_0, so the parent frame is the
	// root's own local rotation. Recovered through quat_cast, hence RotationsNear.
	ZENITH_ASSERT_TRUE(BoneSpaceRotationsNear(xParentWorld, xSkeleton.GetBoneLocalRotation(0u),
		fBONE_SPACE_TEST_EPSILON),
		"the child's parent frame is the root's rotation");

	const Zenith_Maths::Quat xDelta = BoneSpaceAngleAxis(0.85f, Zenith_Maths::Vector3(0.3f, -0.8f, 0.5f));
	const Zenith_Maths::Quat xLocalDelta = Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xDelta, xParentWorld);

	ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(xLocalDelta,
		glm::conjugate(xParentWorld) * xDelta * xParentWorld, fBONE_SPACE_TEST_EPSILON),
		"dQ_local = conj(qP) * dQ_w * qP");

	// The conjugation is not a no-op here, so the assert above is load-bearing.
	// (Getting the conjugation backwards ALSO fails this, which is the point of
	// the world-orientation check below.)
	ZENITH_ASSERT_FALSE(BoneSpaceQuatComponentsNear(xLocalDelta, xDelta, 1.0e-3f),
		"a rotated parent must actually change the delta");
	ZENITH_ASSERT_FALSE(BoneSpaceQuatComponentsNear(xLocalDelta,
		xParentWorld * xDelta * glm::conjugate(xParentWorld), 1.0e-3f),
		"conjugating the OTHER way round must give a different answer");

	// The defining property: q_1' expressed back in world space is the world
	// delta applied to where the bone was.
	const Zenith_Maths::Quat xBoneLocal = xSkeleton.GetBoneLocalRotation(1u);
	const Zenith_Maths::Quat xWorldBefore = xParentWorld * xBoneLocal;
	const Zenith_Maths::Quat xWorldAfter = xParentWorld
		* Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(xDelta, xParentWorld, xBoneLocal);

	ZENITH_ASSERT_TRUE(BoneSpaceRotationsNear(xWorldAfter, xDelta * xWorldBefore, fBONE_SPACE_TEST_EPSILON),
		"the drag must turn the bone by the world delta, in world space");
}

//==============================================================================
// (6) THE SAME DRAG, TWO DIFFERENT PARENT POSES. Two readings of the same
// requirement, both pinned:
//   (a) the same WORLD delta yields DIFFERENT bone-local deltas under different
//       parents — each the conjugate into its own parent frame — and both leave
//       the bone turned by the same world rotation;
//   (b) the same delta expressed in the BONE'S OWN frame (what a handle drawn on
//       the bone's local axis produces) recovers the IDENTICAL bone-local delta
//       whatever the parent is doing.
//==============================================================================
ZENITH_TEST(BoneSpace, TheSameWorldDeltaUnderTwoParentPosesYieldsTheSameWorldResult)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;
	const Zenith_Maths::Matrix4 xIdentity = glm::identity<Zenith_Maths::Matrix4>();

	const Zenith_Maths::Quat axRootPoses[2] =
	{
		BoneSpaceAngleAxis(0.20f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)),
		BoneSpaceAngleAxis(2.05f, Zenith_Maths::Vector3(0.6f, 0.3f, -0.7f))
	};

	const Zenith_Maths::Quat xWorldDelta = BoneSpaceAngleAxis(0.40f, Zenith_Maths::Vector3(0.3f, -0.8f, 0.5f));
	const Zenith_Maths::Quat xBoneFrameDelta = BoneSpaceAngleAxis(0.55f, Zenith_Maths::Vector3(-0.2f, 0.4f, 0.9f));

	Zenith_Maths::Quat axFromWorldDelta[2];
	Zenith_Maths::Quat axFromBoneFrameDelta[2];

	for (int iCase = 0; iCase < 2; ++iCase)
	{
		xRig.SetLocalRotation(0u, axRootPoses[iCase]);

		const Zenith_Maths::Quat xParentWorld = Zenith_BoneSpace::ParentWorldRotation(xIdentity, xSkeleton, 1u);
		const Zenith_Maths::Quat xBoneLocal = xSkeleton.GetBoneLocalRotation(1u);

		// (a)
		axFromWorldDelta[iCase] = Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xWorldDelta, xParentWorld);

		const Zenith_Maths::Quat xWorldBefore = xParentWorld * xBoneLocal;
		const Zenith_Maths::Quat xWorldAfter = xParentWorld
			* Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(xWorldDelta, xParentWorld, xBoneLocal);
		ZENITH_ASSERT_TRUE(BoneSpaceRotationsNear(xWorldAfter, xWorldDelta * xWorldBefore, fBONE_SPACE_TEST_EPSILON),
			"parent pose %d: the world result must be the world delta applied", iCase);

		// (b) the world delta a bone-frame handle would produce, pushed back
		// through the conversion.
		const Zenith_Maths::Quat xEquivalentWorldDelta =
			xParentWorld * xBoneFrameDelta * glm::conjugate(xParentWorld);
		axFromBoneFrameDelta[iCase] =
			Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xEquivalentWorldDelta, xParentWorld);
	}

	ZENITH_ASSERT_FALSE(BoneSpaceQuatComponentsNear(axFromWorldDelta[0], axFromWorldDelta[1], 1.0e-2f),
		"one world delta under two parent poses must give two different LOCAL deltas");

	ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(axFromBoneFrameDelta[0], xBoneFrameDelta, fBONE_SPACE_TEST_EPSILON),
		"a bone-frame delta must round-trip unchanged under parent pose 0");
	ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(axFromBoneFrameDelta[1], xBoneFrameDelta, fBONE_SPACE_TEST_EPSILON),
		"a bone-frame delta must round-trip unchanged under parent pose 1");
	ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(axFromBoneFrameDelta[0], axFromBoneFrameDelta[1],
		fBONE_SPACE_TEST_EPSILON),
		"the bone-local delta must not depend on the parent's pose");
}

//==============================================================================
// (7) THE FULL ROUND TRIP, three levels deep, with W non-identity: bone-local ->
// model -> world, a world delta applied to the deepest bone, written back
// through the instance, recomposed, and read out in world space again.
//
// Also pins what the world pivot IS: rotating bone i does NOT move M_i's own
// translation (L_i = T * R * S puts the rotation AFTER the offset, so the joint
// is a fixed point of q_i) — it moves the joints below it.
//==============================================================================
ZENITH_TEST(BoneSpace, WorldDeltaRoundTripsThroughTheChainUnderNonIdentityW)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;

	// Rotation + translation only: a scale in W is uniform-legal but would blur
	// what this test is about, and ParentWorldRotation's assert covers that case.
	const Zenith_Maths::Matrix4 xSessionModel = BoneSpaceReferenceTRS(
		Zenith_Maths::Vector3(4.0f, -1.5f, 0.25f),
		BoneSpaceAngleAxis(-0.95f, Zenith_Maths::Vector3(0.1f, 0.9f, -0.42f)),
		Zenith_Maths::Vector3(1.0f));

	// qP * q_i really is the rotation baked into W * M_i, at every level. This is
	// what licenses expressing the whole derivation in quaternions.
	for (u_int uBone = 0; uBone < 3u; ++uBone)
	{
		const Zenith_Maths::Quat xComposed = Zenith_BoneSpace::ParentWorldRotation(xSessionModel, xSkeleton, uBone)
			* xSkeleton.GetBoneLocalRotation(uBone);
		const Zenith_Maths::Quat xFromMatrix = BoneSpaceRotationOf(
			Zenith_BoneSpace::BoneWorldMatrix(xSessionModel, xSkeleton, uBone));
		ZENITH_ASSERT_TRUE(BoneSpaceRotationsNear(xComposed, xFromMatrix, fBONE_SPACE_TEST_EPSILON),
			"bone %u: qP * q_i must be the rotation of W * M_i", uBone);
	}

	const Zenith_Maths::Vector3 xGrandChildPivotBefore =
		Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, 2u);
	const Zenith_Maths::Vector3 xChildPivotBefore =
		Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, 1u);

	const Zenith_Maths::Quat xParentWorld = Zenith_BoneSpace::ParentWorldRotation(xSessionModel, xSkeleton, 2u);
	const Zenith_Maths::Quat xWorldBefore = BoneSpaceRotationOf(
		Zenith_BoneSpace::BoneWorldMatrix(xSessionModel, xSkeleton, 2u));

	const Zenith_Maths::Quat xDelta = BoneSpaceAngleAxis(1.35f, Zenith_Maths::Vector3(-0.55f, 0.2f, 0.8f));
	const Zenith_Maths::Quat xNewLocal = Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(
		xDelta, xParentWorld, xSkeleton.GetBoneLocalRotation(2u));

	xRig.SetLocalRotation(2u, xNewLocal);

	const Zenith_Maths::Quat xWorldAfter = BoneSpaceRotationOf(
		Zenith_BoneSpace::BoneWorldMatrix(xSessionModel, xSkeleton, 2u));
	ZENITH_ASSERT_TRUE(BoneSpaceRotationsNear(xWorldAfter, xDelta * xWorldBefore, fBONE_SPACE_TEST_EPSILON),
		"the pose written back must read out, in world space, as the delta applied");

	// The bone's own joint is the pivot: unmoved by its own rotation.
	ZENITH_ASSERT_NEAR_VEC3(Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, 2u),
		xGrandChildPivotBefore, fBONE_SPACE_TEST_EPSILON,
		"a bone's own rotation must not move the joint the ring is drawn at");
	ZENITH_ASSERT_NEAR_VEC3(Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, 1u),
		xChildPivotBefore, fBONE_SPACE_TEST_EPSILON,
		"rotating a bone must not move its ANCESTORS");

	// ...whereas rotating the bone ABOVE it does move it. Without this the assert
	// above would pass for a conversion that had stopped propagating at all.
	xRig.SetLocalRotation(1u, BoneSpaceAngleAxis(1.9f, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)));
	const Zenith_Maths::Vector3 xGrandChildPivotMoved =
		Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, 2u);
	ZENITH_ASSERT_GT(glm::length(xGrandChildPivotMoved - xGrandChildPivotBefore), 0.01f,
		"rotating the parent must move the child's joint");
}

//==============================================================================
// (8) Out-of-range and unresolvable queries degrade to "the session matrix
// alone" rather than reading past the pose cache. The panel asks for the
// selected bone every frame and the selection is a bare index that survives a
// document swap, so this is a live path, not a defensive one.
//==============================================================================
ZENITH_TEST(BoneSpace, OutOfRangeQueriesFallBackToTheSessionMatrixAlone)
{
	BoneSpaceRig xRig;
	if (xRig.m_pxInstance == nullptr)
	{
		ZENITH_FAIL("the rig must build without a device");
		return;
	}
	const Flux_SkeletonInstance& xSkeleton = *xRig.m_pxInstance;

	const Zenith_Maths::Matrix4 xSessionModel = BoneSpaceReferenceTRS(
		Zenith_Maths::Vector3(-7.5f, 2.0f, 3.25f),
		BoneSpaceAngleAxis(0.65f, Zenith_Maths::Vector3(0.2f, 0.5f, -0.84f)),
		Zenith_Maths::Vector3(1.0f));

	const u_int auBadIndices[3] = { 3u, 100u, 0xFFFFFFFFu };
	for (int iCase = 0; iCase < 3; ++iCase)
	{
		const u_int uBone = auBadIndices[iCase];

		ZENITH_ASSERT_TRUE(BoneSpaceMatricesNear(Zenith_BoneSpace::BoneWorldMatrix(xSessionModel, xSkeleton, uBone),
			xSessionModel, 0.0f),
			"an out-of-range bone contributes nothing but W");

		ZENITH_ASSERT_NEAR_VEC3(Zenith_BoneSpace::BoneWorldPosition(xSessionModel, xSkeleton, uBone),
			Zenith_Maths::Vector3(xSessionModel[3]), fBONE_SPACE_TEST_EPSILON,
			"an out-of-range bone's world pivot is W's origin");

		ZENITH_ASSERT_TRUE(BoneSpaceRotationsNear(
			Zenith_BoneSpace::ParentWorldRotation(xSessionModel, xSkeleton, uBone),
			BoneSpaceRotationOf(xSessionModel), fBONE_SPACE_TEST_EPSILON),
			"an out-of-range bone's parent frame is W's own rotation");
	}
}

//==============================================================================
// (9) A NON-UNIT PARENT ROTATION IS NORMALISED BEFORE THE CONJUGATION. conj is
// the inverse only for a unit quaternion; without the normalise a parent frame
// that arrived scaled would multiply the delta's magnitude by |qP|^2 and shrink
// or grow the pose a little more every drag frame.
//==============================================================================
ZENITH_TEST(BoneSpace, ANonUnitParentRotationIsNormalisedBeforeConjugation)
{
	const Zenith_Maths::Quat xParentWorld = BoneSpaceAngleAxis(0.77f, Zenith_Maths::Vector3(0.3f, 0.9f, -0.2f));
	const Zenith_Maths::Quat xDelta = BoneSpaceAngleAxis(-0.5f, Zenith_Maths::Vector3(1.0f, 0.1f, 0.4f));

	const Zenith_Maths::Quat xReference = Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xDelta, xParentWorld);

	const float afScales[3] = { 0.25f, 3.0f, 17.0f };
	for (int iCase = 0; iCase < 3; ++iCase)
	{
		const Zenith_Maths::Quat xScaled = xParentWorld * afScales[iCase];
		ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(
			Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xDelta, xScaled), xReference,
			fBONE_SPACE_TEST_EPSILON),
			"scaling the parent frame by %.2f must not change the local delta",
			static_cast<double>(afScales[iCase]));
	}

	// A unit delta in must be a unit delta out, which is what keeps a pose from
	// drifting off the unit sphere over a long drag.
	const float fLength = std::sqrt(glm::dot(xReference, xReference));
	ZENITH_ASSERT_EQ_FLOAT(fLength, 1.0f, fBONE_SPACE_TEST_EPSILON,
		"conjugation preserves magnitude");

	// And the sign of qP is irrelevant: conj(-q) * d * (-q) == conj(q) * d * q.
	ZENITH_ASSERT_TRUE(BoneSpaceQuatComponentsNear(
		Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(xDelta, -xParentWorld), xReference,
		fBONE_SPACE_TEST_EPSILON),
		"the double cover of the parent frame must not change the answer");
}
