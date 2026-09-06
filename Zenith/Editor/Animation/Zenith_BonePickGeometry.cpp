#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Animation/Zenith_BonePickGeometry.h"

#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Maths/Zenith_Maths_Intersections.h"

namespace
{
	// A tie this close is float noise, not two genuinely competing shapes.
	constexpr float fBONE_PICK_TIE_EPSILON = 1.0e-4f;

	bool BonePickIsFinite(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	bool BonePickIsFiniteVector(const Zenith_Maths::Vector3& xValue)
	{
		return BonePickIsFinite(xValue.x) && BonePickIsFinite(xValue.y) && BonePickIsFinite(xValue.z);
	}

	// One shape's nearest non-negative hit, or false.
	//
	// ★ THE CAPSULE IS A CYLINDER PLUS TWO CAP SPHERES, TESTED SEPARATELY. The
	// shared helper is a FINITE cylinder with flat ends, so a ray that clips the
	// rounded end of a bone misses it entirely without the caps — which reads as
	// "the elbow is not clickable" and is exactly where a user aims.
	bool RaycastOneShape(const Zenith_BonePickShape& xShape,
		const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir, float& fOutT)
	{
		if (xShape.m_fRadius <= 0.0f)
		{
			return false;
		}

		if (xShape.m_bIsJointOnly)
		{
			return Zenith_Maths::Intersections::RayIntersectsSphere(
				xRayOrigin - xShape.m_xB, xRayDir, xShape.m_fRadius, fOutT);
		}

		const Zenith_Maths::Vector3 xSegment = xShape.m_xB - xShape.m_xA;
		const float fLength = glm::length(xSegment);
		if (fLength < kfBONE_PICK_MIN_SEGMENT)
		{
			return false;
		}
		const Zenith_Maths::Vector3 xAxis = xSegment / fLength;

		bool bHit = false;
		float fBest = 0.0f;

		// Every helper in Zenith_Maths_Intersections is ORIGIN-ANCHORED: the shape
		// sits at the origin and the caller pre-translates the ray.
		float fT = 0.0f;
		if (Zenith_Maths::Intersections::RayIntersectsCylinder(
			xRayOrigin - xShape.m_xA, xRayDir, xAxis, xShape.m_fRadius, fLength, fT))
		{
			bHit = true;
			fBest = fT;
		}
		if (Zenith_Maths::Intersections::RayIntersectsSphere(
			xRayOrigin - xShape.m_xA, xRayDir, xShape.m_fRadius, fT))
		{
			if (!bHit || fT < fBest) { fBest = fT; }
			bHit = true;
		}
		if (Zenith_Maths::Intersections::RayIntersectsSphere(
			xRayOrigin - xShape.m_xB, xRayDir, xShape.m_fRadius, fT))
		{
			if (!bHit || fT < fBest) { fBest = fT; }
			bHit = true;
		}

		if (bHit)
		{
			fOutT = fBest;
		}
		return bHit;
	}
}

//=============================================================================
// Build
//=============================================================================

void Zenith_BuildBonePickSet(const Flux_SkeletonInstance& xSkeleton,
	const Zenith_SkeletonAsset& xSkeletonAsset,
	const Zenith_Maths::Matrix4& xSessionModel,
	Zenith_BonePickSet& xOut)
{
	xOut.m_xShapes.Clear();
	xOut.m_fSkeletonExtent = 0.0f;

	const u_int uInstanceBones = static_cast<u_int>(xSkeleton.GetNumBones());
	const u_int uAssetBones = static_cast<u_int>(xSkeletonAsset.GetNumBones());
	const u_int uNumBones = (uInstanceBones < uAssetBones) ? uInstanceBones : uAssetBones;
	if (uNumBones == 0u)
	{
		return;
	}

	// ---- joints, in world space --------------------------------------------
	Zenith_Vector<Zenith_Maths::Vector3> axJoints;
	axJoints.Resize(uNumBones, Zenith_Maths::Vector3(0.0f));

	Zenith_Maths::Vector3 xMin(0.0f);
	Zenith_Maths::Vector3 xMax(0.0f);
	for (u_int u = 0; u < uNumBones; ++u)
	{
		const Zenith_Maths::Vector4 xJoint4 = xSessionModel * xSkeleton.GetBoneModelTransform(u)[3];
		const Zenith_Maths::Vector3 xJoint(xJoint4.x, xJoint4.y, xJoint4.z);
		axJoints.Get(u) = xJoint;

		if (u == 0u)
		{
			xMin = xJoint;
			xMax = xJoint;
		}
		else
		{
			xMin = glm::min(xMin, xJoint);
			xMax = glm::max(xMax, xJoint);
		}
	}

	xOut.m_fSkeletonExtent = glm::length(xMax - xMin);
	if (!BonePickIsFinite(xOut.m_fSkeletonExtent))
	{
		// A NaN anywhere in the pose would otherwise produce shapes every ray
		// "hits", which is worse than nothing at all.
		xOut.m_fSkeletonExtent = 0.0f;
		return;
	}

	const float fMinRadius = kfBONE_PICK_MIN_RADIUS_EXTENT * xOut.m_fSkeletonExtent;
	const float fJointRadius = kfBONE_PICK_JOINT_EXTENT * xOut.m_fSkeletonExtent;

	// ---- one capsule per (bone, child), owned by the PARENT -----------------
	//
	// ★ THE OWNER IS THE PARENT, because translation(M_child) = M_parent * p_child
	// and the parent's rotation is inside M_parent — see the header for the
	// derivation. Iterating over CHILDREN and attributing to the parent is the
	// same single walk the design note describes; only the owner index differs,
	// and that index is the whole answer a pick returns.
	//
	// ★ ONE PARENT-INDEX WALK, NOT Zenith_SkeletonAsset::GetChildBones PER BONE.
	// That returns a fresh Zenith_Vector BY VALUE — one allocation per bone per
	// rebuild, on a path that runs whenever the pose moves. The capsule tally it
	// fills is also what decides which bones need a joint sphere, so "has no
	// usable capsule" is counted once rather than re-derived.
	Zenith_Vector<u_int> auCapsuleCounts;
	auCapsuleCounts.Resize(uNumBones, 0u);

	for (u_int uChild = 0; uChild < uNumBones; ++uChild)
	{
		const int32_t iParent = xSkeletonAsset.GetBone(uChild).m_iParentIndex;
		if (iParent == Zenith_SkeletonAsset::INVALID_BONE_INDEX ||
			static_cast<u_int>(iParent) >= uNumBones)
		{
			continue;
		}
		const u_int uParent = static_cast<u_int>(iParent);

		const Zenith_Maths::Vector3& xA = axJoints.Get(uParent);
		const Zenith_Maths::Vector3& xB = axJoints.Get(uChild);
		if (!BonePickIsFiniteVector(xA) || !BonePickIsFiniteVector(xB))
		{
			continue;
		}

		const float fSegment = glm::length(xB - xA);
		if (fSegment < kfBONE_PICK_MIN_SEGMENT)
		{
			// Degenerate: there is no axis to normalise, so no capsule. The parent
			// falls back to a joint sphere below if this was its only child.
			continue;
		}

		Zenith_BonePickShape xShape;
		xShape.m_uBoneIndex = uParent;
		xShape.m_uChildBoneIndex = uChild;
		xShape.m_xA = xA;
		xShape.m_xB = xB;
		xShape.m_fRadius = glm::max(kfBONE_PICK_RADIUS_FRACTION * fSegment, fMinRadius);
		xShape.m_bIsJointOnly = false;
		xOut.m_xShapes.PushBack(xShape);
		++auCapsuleCounts.Get(uParent);
	}

	// ---- a joint sphere only where a capsule cannot serve --------------------
	for (u_int u = 0; u < uNumBones; ++u)
	{
		const Zenith_Maths::Vector3& xJoint = axJoints.Get(u);
		if (!BonePickIsFiniteVector(xJoint))
		{
			continue;
		}
		if (auCapsuleCounts.Get(u) > 0u)
		{
			// It owns real geometry; a sphere on top of it would only compete with
			// its own capsules for the same clicks.
			continue;
		}

		// Either a LEAF (no children at all — its joint dot is the only thing
		// drawn for it) or a bone whose every child sat exactly on top of it.
		// Both cases are "this bone would otherwise be unselectable".
		Zenith_BonePickShape xShape;
		xShape.m_uBoneIndex = u;
		xShape.m_uChildBoneIndex = u;
		xShape.m_xA = xJoint;
		xShape.m_xB = xJoint;
		xShape.m_fRadius = fJointRadius;
		xShape.m_bIsJointOnly = true;
		xOut.m_xShapes.PushBack(xShape);
	}
}

//=============================================================================
// Raycast
//=============================================================================

bool Zenith_RaycastBonePickSet(const Zenith_BonePickSet& xSet,
	const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir,
	u_int& uOutBoneIndex,
	float& fOutDistance)
{
	if (!BonePickIsFiniteVector(xRayOrigin) || !BonePickIsFiniteVector(xRayDir))
	{
		return false;
	}
	if (glm::dot(xRayDir, xRayDir) < 1.0e-12f)
	{
		return false;
	}

	// ★ ACCUMULATED IN LOCALS. Both outputs must survive a miss untouched, and
	// the only way to guarantee that is to not write them until there is a hit.
	//
	// ★ THE TWO CLASSES ARE ACCUMULATED SEPARATELY, because the choice between
	// them is not "which is nearer" — see the header. Within a class it is.
	bool bCapsuleHit = false;
	u_int uCapsuleBone = 0u;
	float fCapsuleT = 0.0f;
	float fCapsuleRadius = 0.0f;

	bool bJointHit = false;
	u_int uJointBone = 0u;
	float fJointT = 0.0f;

	const u_int uCount = xSet.m_xShapes.GetSize();
	for (u_int u = 0; u < uCount; ++u)
	{
		const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);

		float fT = 0.0f;
		if (!RaycastOneShape(xShape, xRayOrigin, xRayDir, fT))
		{
			continue;
		}
		if (fT < 0.0f || !BonePickIsFinite(fT))
		{
			continue;
		}

		if (xShape.m_bIsJointOnly)
		{
			// A 1e-4 tie goes to the incumbent, so the first shape of a coincident
			// pair keeps the win and the result does not depend on float noise.
			if (!bJointHit || fT < fJointT - fBONE_PICK_TIE_EPSILON)
			{
				bJointHit = true;
				uJointBone = xShape.m_uBoneIndex;
				fJointT = fT;
			}
		}
		else
		{
			if (!bCapsuleHit || fT < fCapsuleT - fBONE_PICK_TIE_EPSILON)
			{
				bCapsuleHit = true;
				uCapsuleBone = xShape.m_uBoneIndex;
				fCapsuleT = fT;
				fCapsuleRadius = xShape.m_fRadius;
			}
		}
	}

	if (!bJointHit && !bCapsuleHit)
	{
		return false;
	}

	// A joint sphere wins while it is inside the geometry standing in front of
	// it — which is the ONLY situation it is ever in, since it sits at the end
	// cap of its parent's capsule by construction. Past that margin it is
	// genuinely occluded and the capsule in front is what the user clicked.
	const bool bTakeJoint = bJointHit &&
		(!bCapsuleHit || fJointT <= fCapsuleT + kfBONE_PICK_JOINT_PRIORITY_RADII * fCapsuleRadius);

	uOutBoneIndex = bTakeJoint ? uJointBone : uCapsuleBone;
	fOutDistance = bTakeJoint ? fJointT : fCapsuleT;
	return true;
}

#ifdef ZENITH_TESTING
#include "Editor/Animation/Zenith_BonePickGeometry.Tests.inl"
#endif

#endif // ZENITH_TOOLS
