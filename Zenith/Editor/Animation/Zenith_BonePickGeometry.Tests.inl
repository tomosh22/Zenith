//------------------------------------------------------------------------------
// Zenith_BonePickGeometry unit tests (WU-4.1).
// Included at the bottom of Zenith_BonePickGeometry.cpp.
//
// ★ ALL OF IT IS CPU-ONLY AND NONE OF IT IS requiresGraphics.
// Flux_SkeletonInstance holds no device resources — its Destroy() clears an asset
// handle and a bone count and nothing else — so a test builds a skeleton in code,
// calls ComputeSkinningMatrices() and fires rays. No device, no view, no window.
// That matters more than it sounds: a requiresGraphics test is SKIPPED-AS-PASSED
// headless, so the only coverage bone picking would have had is coverage that
// rots without anything going red.
//
// ★ THE PROPERTIES PINNED HERE, one test each:
//   1. the capsule from joint i to joint child(i) belongs to bone i — asserted
//      by ROTATING a bone and checking which capsule moved, not by reading the
//      code back to itself;
//   2. a leaf bone is pickable at its joint, and its neighbour is still pickable
//      along its own length;
//   3. a DEGENERATE (zero-length) bone falls back to its joint sphere, and that
//      sphere is reachable;
//   4. the nearest hit wins between two capsules with different owners, from
//      either side;
//   5. a MISS leaves both outputs untouched;
//   6. RayIntersectsSphere — hit, miss, behind, and origin-inside;
//   7. the pick set is STALE until ComputeSkinningMatrices runs (design note
//      §3.2) — GetBoneModelTransform is a cache nothing else fills.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"

namespace
{
	//--------------------------------------------------------------------------
	// A skeleton asset plus a live instance over it.
	//
	// ★ THE ASSET IS DECLARED FIRST SO IT IS DESTROYED LAST. The instance holds an
	// owning handle on it (Flux_SkeletonInstance::CreateFromAsset calls Set, which
	// AddRefs), so the instance has to go first — the destructor does that
	// explicitly, and the declaration order is the belt to that brace.
	//
	// Nothing here touches the asset REGISTRY: the asset is a plain stack object,
	// and Zenith_Asset::Release only decrements (the registry, not the handle, is
	// what deletes), so a stack asset is safe to hand to an owning handle.
	//--------------------------------------------------------------------------
	struct BonePickRig
	{
		Zenith_SkeletonAsset m_xAsset;
		Flux_SkeletonInstance* m_pxInstance = nullptr;

		BonePickRig() = default;

		~BonePickRig()
		{
			if (m_pxInstance != nullptr)
			{
				m_pxInstance->Destroy();
				delete m_pxInstance;
				m_pxInstance = nullptr;
			}
		}

		BonePickRig(const BonePickRig&) = delete;
		BonePickRig& operator=(const BonePickRig&) = delete;

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

		void Build(Zenith_BonePickSet& xOut) const
		{
			Zenith_BuildBonePickSet(*m_pxInstance, m_xAsset, Zenith_Maths::Matrix4(1.0f), xOut);
		}
	};

	// A straight chain up +Y: Root (0,0,0) -> Spine (0,1,0) -> Head (0,2,0).
	// Root owns the capsule to Spine; Spine owns the capsule to Head; Head is a
	// leaf and owns only a joint sphere.
	void BonePickBuildChain(BonePickRig& xRig)
	{
		xRig.AddBone("Root", -1, 0.0f, 0.0f, 0.0f);
		xRig.AddBone("Spine", 0, 0.0f, 1.0f, 0.0f);
		xRig.AddBone("Head", 1, 0.0f, 1.0f, 0.0f);
		xRig.Finalise();
	}

	// Two chains hanging off one root, far apart in X, so that a horizontal ray
	// crosses TWO capsules with DIFFERENT owners:
	//   Root(0) (0,0,0)
	//   L1(1) (-2,0,0)  L2(2) (-2,1,0)   -> bone 1 owns the capsule at x = -2
	//   R1(3) ( 2,0,0)  R2(4) ( 2,2,0)   -> bone 3 owns the capsule at x = +2
	// Root owns the two horizontal capsules along y = 0.
	void BonePickBuildTwoChains(BonePickRig& xRig)
	{
		xRig.AddBone("Root", -1, 0.0f, 0.0f, 0.0f);
		xRig.AddBone("L1", 0, -2.0f, 0.0f, 0.0f);
		xRig.AddBone("L2", 1, 0.0f, 1.0f, 0.0f);
		xRig.AddBone("R1", 0, 2.0f, 0.0f, 0.0f);
		xRig.AddBone("R2", 3, 0.0f, 2.0f, 0.0f);
		xRig.Finalise();
	}

	// The capsule bone uOwner owns that runs to uChild, or null.
	const Zenith_BonePickShape* BonePickFindCapsule(const Zenith_BonePickSet& xSet, u_int uOwner, u_int uChild)
	{
		for (u_int u = 0; u < xSet.m_xShapes.GetSize(); ++u)
		{
			const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);
			if (!xShape.m_bIsJointOnly && xShape.m_uBoneIndex == uOwner && xShape.m_uChildBoneIndex == uChild)
			{
				return &xShape;
			}
		}
		return nullptr;
	}

	const Zenith_BonePickShape* BonePickFindJointSphere(const Zenith_BonePickSet& xSet, u_int uOwner)
	{
		for (u_int u = 0; u < xSet.m_xShapes.GetSize(); ++u)
		{
			const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);
			if (xShape.m_bIsJointOnly && xShape.m_uBoneIndex == uOwner)
			{
				return &xShape;
			}
		}
		return nullptr;
	}

	u_int BonePickCountCapsulesFor(const Zenith_BonePickSet& xSet, u_int uOwner)
	{
		u_int uCount = 0u;
		for (u_int u = 0; u < xSet.m_xShapes.GetSize(); ++u)
		{
			const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);
			if (!xShape.m_bIsJointOnly && xShape.m_uBoneIndex == uOwner) { ++uCount; }
		}
		return uCount;
	}

	// Cast a horizontal ray along -X (from far +X) at the given height.
	bool BonePickCastFromPlusX(const Zenith_BonePickSet& xSet, float fY, u_int& uOutBone)
	{
		float fT = 0.0f;
		return Zenith_RaycastBonePickSet(xSet,
			Zenith_Maths::Vector3(5.0f, fY, 0.0f), Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), uOutBone, fT);
	}

	bool BonePickCastFromMinusX(const Zenith_BonePickSet& xSet, float fY, u_int& uOutBone)
	{
		float fT = 0.0f;
		return Zenith_RaycastBonePickSet(xSet,
			Zenith_Maths::Vector3(-5.0f, fY, 0.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), uOutBone, fT);
	}
}

//==============================================================================
// (1) ★★ THE CAPSULE FROM JOINT i TO JOINT child(i) BELONGS TO BONE i.
//
// The design note (§2) states the opposite — that the capsule parent(i)->i
// belongs to the CHILD i — and the composition in the tree says otherwise:
// ComposeTransformMatrix builds L_i = T(p_i)*R(q_i)*S, so
// translation(M_i) = M_parent * p_i, with no q_i in it. Bone i's own joint does
// not move when q_i changes; the joints below it do.
//
// ★ THIS TEST DOES NOT ASSERT THE RULE, IT MEASURES IT: rotate one bone, then
// check WHICH capsule moved. Reading the ownership back out of the builder would
// be a guard comparing a value against a re-computation of itself, and the whole
// hazard here is that either convention looks entirely plausible.
//==============================================================================
ZENITH_TEST(BonePick, TheCapsuleBelongsToTheParentWhoseRotationMovesIt)
{
	BonePickRig xRig;
	BonePickBuildChain(xRig);
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the three-bone chain produced an instance");

	Zenith_BonePickSet xRest;
	xRig.Build(xRest);

	ZENITH_ASSERT_EQ_FLOAT(xRest.m_fSkeletonExtent, 2.0f, 1.0e-4f,
		"the extent is the AABB diagonal over the joints (0,0,0)..(0,2,0)");

	const Zenith_BonePickShape* pxRootCapsule = BonePickFindCapsule(xRest, /*owner*/ 0u, /*child*/ 1u);
	const Zenith_BonePickShape* pxSpineCapsule = BonePickFindCapsule(xRest, /*owner*/ 1u, /*child*/ 2u);
	ZENITH_ASSERT_NOT_NULL(pxRootCapsule, "the ROOT owns the capsule that runs to Spine");
	ZENITH_ASSERT_NOT_NULL(pxSpineCapsule, "and SPINE owns the one that runs to Head");
	ZENITH_ASSERT_EQ(BonePickCountCapsulesFor(xRest, 2u), 0u,
		"the leaf owns no capsule — nothing hangs off it to move");

	Zenith_Maths::Vector3 xRestSpineB(0.0f);
	if (pxSpineCapsule != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxSpineCapsule->m_xA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1.0e-4f,
			"Spine's capsule STARTS at Spine's own joint");
		ZENITH_ASSERT_NEAR_VEC3(pxSpineCapsule->m_xB, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f), 1.0e-4f,
			"...and ENDS at the child joint it moves");
		xRestSpineB = pxSpineCapsule->m_xB;
	}

	// ---- the measurement: rotate SPINE and see what moved -------------------
	xRig.m_pxInstance->SetBoneLocalTransform(1u,
		xRig.m_pxInstance->GetBoneLocalPosition(1u),
		glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)),
		Zenith_Maths::Vector3(1.0f));
	xRig.m_pxInstance->ComputeSkinningMatrices();

	Zenith_BonePickSet xPosed;
	xRig.Build(xPosed);

	const Zenith_BonePickShape* pxRootPosed = BonePickFindCapsule(xPosed, 0u, 1u);
	const Zenith_BonePickShape* pxSpinePosed = BonePickFindCapsule(xPosed, 1u, 2u);
	ZENITH_ASSERT_NOT_NULL(pxRootPosed, "the root capsule is still there");
	ZENITH_ASSERT_NOT_NULL(pxSpinePosed, "and so is Spine's");

	if (pxRootPosed != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxRootPosed->m_xB, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1.0e-4f,
			"★ rotating SPINE did NOT move the capsule the ROOT owns — Spine's own joint is fixed by "
			"translation(M_spine) = M_root * p_spine, which contains no q_spine");
	}
	if (pxSpinePosed != nullptr)
	{
		// Asserted component-wise rather than against a literal vector so the test
		// pins the SWING rather than a handedness convention: the tip left the +Y
		// axis and came down into the y = 1 plane, one unit out.
		ZENITH_ASSERT_EQ_FLOAT(pxSpinePosed->m_xB.y, 1.0f, 1.0e-4f,
			"★ ...and DID move the capsule SPINE owns: its far end swung down out of +Y");
		ZENITH_ASSERT_EQ_FLOAT(fabsf(pxSpinePosed->m_xB.x), 1.0f, 1.0e-4f, "one unit out along X");
		ZENITH_ASSERT_EQ_FLOAT(pxSpinePosed->m_xB.z, 0.0f, 1.0e-4f, "and not out of the XY plane");
		ZENITH_ASSERT_GT(glm::length(pxSpinePosed->m_xB - xRestSpineB), 1.0f,
			"the far end genuinely moved, so the two capsules are being told apart by geometry");
	}

	// ---- and the pick agrees ------------------------------------------------
	u_int uBone = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(BonePickCastFromPlusX(xRest, 0.5f, uBone),
		"a ray through the middle of the root->spine segment hits something");
	ZENITH_ASSERT_EQ(uBone, 0u,
		"and selects the ROOT, whose rotation is what swings that segment");
}

//==============================================================================
// (2) A leaf is pickable at its joint, and its parent stays pickable along its
// own length.
//
// ★ THIS IS ALSO THE JOINT-SPHERE PRIORITY RULE'S TEST. A leaf's sphere sits at
// the far END CAP of its parent's capsule and is smaller than it, so plain
// nearest-wins would hand every ray that could reach the tip to the parent and
// the leaf would be permanently unselectable.
//==============================================================================
ZENITH_TEST(BonePick, ALeafIsPickableAtItsJointAndItsParentAlongItsLength)
{
	BonePickRig xRig;
	BonePickBuildChain(xRig);

	Zenith_BonePickSet xSet;
	xRig.Build(xSet);

	const Zenith_BonePickShape* pxTip = BonePickFindJointSphere(xSet, 2u);
	ZENITH_ASSERT_NOT_NULL(pxTip, "the leaf's only shape is a joint sphere");
	if (pxTip != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxTip->m_xB, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f), 1.0e-4f,
			"sitting on the leaf's own joint");
		ZENITH_ASSERT_EQ_FLOAT(pxTip->m_fRadius, kfBONE_PICK_JOINT_EXTENT * 2.0f, 1.0e-5f,
			"with a radius derived from the SKELETON extent, not from a segment length");
		ZENITH_ASSERT_LT(pxTip->m_fRadius, kfBONE_PICK_RADIUS_FRACTION * 1.0f,
			"★ and SMALLER than the parent capsule it is nested inside — which is exactly why "
			"nearest-wins alone could never reach it");
	}
	ZENITH_ASSERT_NULL(BonePickFindJointSphere(xSet, 1u),
		"a bone that owns a capsule gets no sphere competing with it for the same clicks");

	u_int uBone = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(BonePickCastFromPlusX(xSet, 2.0f, uBone), "a ray at the tip hits something");
	ZENITH_ASSERT_EQ(uBone, 2u, "and it is the LEAF, not the parent capsule ending there");

	ZENITH_ASSERT_TRUE(BonePickCastFromPlusX(xSet, 1.5f, uBone), "a ray halfway up Spine's segment hits");
	ZENITH_ASSERT_EQ(uBone, 1u, "and selects SPINE — the priority rule did not swallow the whole bone");
}

//==============================================================================
// (3) A DEGENERATE bone falls back to its joint sphere.
//
// Root -> A -> B -> C, with B sitting exactly on A. A's only child is coincident
// with it, so A's capsule would be zero-length (no axis to normalise) and is
// suppressed — leaving A with nothing at all unless the fallback fires. This is
// the branch that distinguishes "degenerate" from "leaf": A is not a leaf.
//==============================================================================
ZENITH_TEST(BonePick, ADegenerateBoneFallsBackToItsJointSphere)
{
	BonePickRig xRig;
	xRig.AddBone("Root", -1, 0.0f, 0.0f, 0.0f);
	xRig.AddBone("A", 0, 0.0f, 1.0f, 0.0f);
	xRig.AddBone("B", 1, 0.0f, 0.0f, 0.0f);   // coincident with A
	xRig.AddBone("C", 2, 0.0f, 1.0f, 0.0f);
	xRig.Finalise();
	ZENITH_ASSERT_NOT_NULL(xRig.m_pxInstance, "the degenerate rig produced an instance");

	Zenith_BonePickSet xSet;
	xRig.Build(xSet);

	ZENITH_ASSERT_EQ(BonePickCountCapsulesFor(xSet, 1u), 0u,
		"A's zero-length capsule is suppressed rather than emitted with a garbage axis");
	const Zenith_BonePickShape* pxJoint = BonePickFindJointSphere(xSet, 1u);
	ZENITH_ASSERT_NOT_NULL(pxJoint, "A falls back to a joint sphere even though it is NOT a leaf");
	if (pxJoint != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxJoint->m_xB, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1.0e-4f,
			"the fallback sphere sits on A's own joint");
		ZENITH_ASSERT_GT(pxJoint->m_fRadius, 0.0f, "and has a real radius, so it is not inert");
	}

	// The non-degenerate neighbours still carry their capsules.
	ZENITH_ASSERT_NOT_NULL(BonePickFindCapsule(xSet, 0u, 1u), "the root still owns its capsule to A");
	ZENITH_ASSERT_NOT_NULL(BonePickFindCapsule(xSet, 2u, 3u), "and B still owns its capsule to C");

	// ★ AND IT IS ACTUALLY REACHABLE. A fallback shape that no ray can select is
	// the same as no fallback at all, and the two are indistinguishable from a
	// structural assertion alone.
	u_int uBone = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(BonePickCastFromPlusX(xSet, 1.0f, uBone), "a ray at the shared joint hits");
	ZENITH_ASSERT_EQ(uBone, 1u, "and selects the degenerate bone, not either capsule ending there");
}

//==============================================================================
// (4) The nearest hit wins between two capsules with DIFFERENT owners — from
// BOTH sides.
//
// Firing from one side only would pass just as well if the raycast simply
// returned the first shape in the list, which is the mistake worth ruling out.
// The third cast is the OTHER half of the priority rule: a leaf sphere far
// behind an intervening capsule must NOT win.
//==============================================================================
ZENITH_TEST(BonePick, TheNearestHitWins)
{
	BonePickRig xRig;
	BonePickBuildTwoChains(xRig);   // bone 1 owns the capsule at x=-2, bone 3 the one at x=+2

	Zenith_BonePickSet xSet;
	xRig.Build(xSet);
	ZENITH_ASSERT_NOT_NULL(BonePickFindCapsule(xSet, 1u, 2u), "the left chain's capsule is owned by L1");
	ZENITH_ASSERT_NOT_NULL(BonePickFindCapsule(xSet, 3u, 4u), "the right chain's by R1");
	ZENITH_ASSERT_EQ(BonePickCountCapsulesFor(xSet, 0u), 2u,
		"and the root owns ONE CAPSULE PER CHILD — two of them");

	u_int uBone = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(BonePickCastFromPlusX(xSet, 0.5f, uBone), "a ray from +X crosses both uprights");
	ZENITH_ASSERT_EQ(uBone, 3u, "and resolves to the RIGHT one, which is nearer");

	ZENITH_ASSERT_TRUE(BonePickCastFromMinusX(xSet, 0.5f, uBone), "the same line fired from -X crosses both too");
	ZENITH_ASSERT_EQ(uBone, 1u, "and now resolves to the LEFT one — the answer follows the RAY, not list order");

	// The left chain's leaf sphere sits at (-2, 1, 0), directly behind the right
	// upright along y = 1. It is a joint sphere, but it is nowhere near "inside"
	// the capsule in front of it, so the capsule wins.
	ZENITH_ASSERT_TRUE(BonePickCastFromPlusX(xSet, 1.0f, uBone), "a ray along y=1 crosses the right upright");
	ZENITH_ASSERT_EQ(uBone, 3u,
		"★ and a joint sphere far BEHIND it does not steal the click — the priority margin is bounded "
		"by the occluding capsule's own radius");
}

//==============================================================================
// (5) A miss leaves BOTH outputs untouched.
//
// The caller passes in a live selection index; a click that hit nothing must not
// overwrite it with a sentinel the caller never chose.
//==============================================================================
ZENITH_TEST(BonePick, AMissLeavesBothOutputsUntouched)
{
	BonePickRig xRig;
	BonePickBuildChain(xRig);

	Zenith_BonePickSet xSet;
	xRig.Build(xSet);

	u_int uBone = 0xABCDEF01u;
	float fT = -12345.0f;
	ZENITH_ASSERT_FALSE(Zenith_RaycastBonePickSet(xSet,
		Zenith_Maths::Vector3(5.0f, 10.0f, 0.0f), Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), uBone, fT),
		"a ray well above the whole skeleton hits nothing");
	ZENITH_ASSERT_EQ(uBone, 0xABCDEF01u, "the bone output is untouched by a miss");
	ZENITH_ASSERT_EQ_FLOAT(fT, -12345.0f, 1.0e-6f, "and so is the distance output");

	// A zero-length direction is not a ray, and must be refused rather than
	// divided by.
	ZENITH_ASSERT_FALSE(Zenith_RaycastBonePickSet(xSet,
		Zenith_Maths::Vector3(5.0f, 0.5f, 0.0f), Zenith_Maths::Vector3(0.0f), uBone, fT),
		"a zero-length direction is refused");
	ZENITH_ASSERT_EQ(uBone, 0xABCDEF01u, "and leaves the outputs alone as well");

	// An empty set is a miss, not a crash.
	Zenith_BonePickSet xEmpty;
	ZENITH_ASSERT_FALSE(Zenith_RaycastBonePickSet(xEmpty,
		Zenith_Maths::Vector3(5.0f, 0.5f, 0.0f), Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), uBone, fT),
		"an empty pick set is a clean miss");
}

//==============================================================================
// (6) RayIntersectsSphere — the new origin-anchored helper.
//==============================================================================
ZENITH_TEST(BonePick, RayIntersectsSphereHitsMissesAndStartsInside)
{
	float fT = 0.0f;

	ZENITH_ASSERT_TRUE(Zenith_Maths::Intersections::RayIntersectsSphere(
		Zenith_Maths::Vector3(0.0f, 0.0f, -5.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1.0f, fT),
		"a ray aimed at the origin-centred sphere hits it");
	ZENITH_ASSERT_EQ_FLOAT(fT, 4.0f, 1.0e-4f, "at the NEAR surface, not the far one");

	float fUntouched = -777.0f;
	ZENITH_ASSERT_FALSE(Zenith_Maths::Intersections::RayIntersectsSphere(
		Zenith_Maths::Vector3(0.0f, 5.0f, -5.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1.0f, fUntouched),
		"a ray passing wide misses");
	ZENITH_ASSERT_EQ_FLOAT(fUntouched, -777.0f, 1.0e-6f, "and writes nothing");

	ZENITH_ASSERT_FALSE(Zenith_Maths::Intersections::RayIntersectsSphere(
		Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1.0f, fUntouched),
		"a sphere entirely BEHIND the ray is a miss, not a negative distance");
	ZENITH_ASSERT_EQ_FLOAT(fUntouched, -777.0f, 1.0e-6f, "and still writes nothing");

	// ★ ORIGIN INSIDE. The near root is negative, so an implementation that took
	// it unconditionally would report a miss, and a joint would stop being
	// selectable the moment the camera dollied inside it.
	ZENITH_ASSERT_TRUE(Zenith_Maths::Intersections::RayIntersectsSphere(
		Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1.0f, fT),
		"a ray whose origin is INSIDE the sphere still hits");
	ZENITH_ASSERT_EQ_FLOAT(fT, 1.0f, 1.0e-4f, "reporting the exit point");

	ZENITH_ASSERT_FALSE(Zenith_Maths::Intersections::RayIntersectsSphere(
		Zenith_Maths::Vector3(0.0f, 0.0f, -5.0f), Zenith_Maths::Vector3(0.0f), 1.0f, fUntouched),
		"a zero-length direction is not a ray");
}

//==============================================================================
// (7) ★ THE PICK SET IS STALE UNTIL ComputeSkinningMatrices RUNS (design note
// §3.2).
//
// GetBoneModelTransform returns a CACHE that only ComputeSkinningMatrices fills.
// SetBoneLocalTransform neither updates nor invalidates it, so a pose write
// followed straight by a rebuild produces shapes one write behind the bones —
// with no assert and no symptom other than lag. This is the assertion that turns
// that into a checked precondition rather than a paragraph.
//==============================================================================
ZENITH_TEST(BonePick, TheSetIsStaleUntilComputeSkinningMatricesRuns)
{
	BonePickRig xRig;
	BonePickBuildChain(xRig);

	Zenith_BonePickSet xBefore;
	xRig.Build(xBefore);
	const Zenith_BonePickShape* pxSpineBefore = BonePickFindCapsule(xBefore, 1u, 2u);
	ZENITH_ASSERT_NOT_NULL(pxSpineBefore, "Spine's capsule exists at bind pose");
	if (pxSpineBefore != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxSpineBefore->m_xA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1.0e-4f,
			"and starts where the bind pose puts it");
	}

	// Move the Spine bone a long way sideways. Nothing invalidates the cache.
	xRig.m_pxInstance->SetBoneLocalTransform(1u, Zenith_Maths::Vector3(3.0f, 1.0f, 0.0f),
		glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));

	Zenith_BonePickSet xStale;
	xRig.Build(xStale);
	const Zenith_BonePickShape* pxSpineStale = BonePickFindCapsule(xStale, 1u, 2u);
	ZENITH_ASSERT_NOT_NULL(pxSpineStale, "the capsule is still there");
	if (pxSpineStale != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxSpineStale->m_xA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1.0e-4f,
			"★ and has NOT moved — a pose write alone leaves the model-space cache stale");
	}

	xRig.m_pxInstance->ComputeSkinningMatrices();

	Zenith_BonePickSet xFresh;
	xRig.Build(xFresh);
	const Zenith_BonePickShape* pxSpineFresh = BonePickFindCapsule(xFresh, 1u, 2u);
	ZENITH_ASSERT_NOT_NULL(pxSpineFresh, "the capsule survives the refresh");
	if (pxSpineFresh != nullptr)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxSpineFresh->m_xA, Zenith_Maths::Vector3(3.0f, 1.0f, 0.0f), 1.0e-4f,
			"and only NOW follows the bone that moved");
		ZENITH_ASSERT_NEAR_VEC3(pxSpineFresh->m_xB, Zenith_Maths::Vector3(3.0f, 2.0f, 0.0f), 1.0e-4f,
			"...taking the child joint with it");
	}
}
