#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Animation/Zenith_BoneSpace.h"

#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

bool Zenith_BoneSpace_ForceLink()
{
	// See the declaration: this is the only thing naming this .obj until WU-4.3's
	// manipulator lands, and without it /OPT:REF drops the TU and every
	// ZENITH_TEST registrar below with it.
	return true;
}

namespace
{
	//--------------------------------------------------------------------------
	// The scale spread a parent frame may carry and still be treated as a pure
	// rotation. Relative, not absolute, so a rig authored in centimetres is held
	// to the same standard as one authored in metres.
	//
	// 1e-3 rather than something tight: the number being checked is three column
	// lengths of a matrix accumulated down a parent chain, each one a square root
	// of a sum of products, and a deep chain of unit-scale bones already spreads
	// by a few ULPs per level. The assert exists to catch an AUTHORED non-uniform
	// scale (0.5 on one axis, a mirrored limb), not float noise.
	//--------------------------------------------------------------------------
	constexpr float fBONE_SPACE_UNIFORM_SCALE_TOLERANCE = 1.0e-3f;

	//--------------------------------------------------------------------------
	// ★ ASSERT, DO NOT ASSUME (design note §9.5). Zenith_Maths::DecomposeTRS is
	// documented for affine, shear-free, POSITIVE-scale matrices and says nothing
	// about what it returns for anything else — it divides each column by its own
	// length and quat_casts the result, so a non-uniform scale yields a
	// non-orthonormal basis and a mirrored one yields a quaternion for a rotation
	// that does not exist. Either way the recovered frame is wrong by an amount
	// that depends on the pose, which is the shape of bug that survives every
	// gate and shows up as "the gizmo drags slightly off-axis on that one limb".
	//
	// Two separate conditions, because they fail differently:
	//   - the column LENGTHS must agree (uniformity);
	//   - the 3x3 determinant must be positive (handedness). A mirrored basis has
	//     three perfectly equal column lengths, so uniformity alone cannot see it.
	//--------------------------------------------------------------------------
	void BoneSpaceAssertRecoverableRotation(const Zenith_Maths::Matrix4& xMatrix,
		const Zenith_Maths::Vector3& xScale)
	{
		float fMin = xScale.x;
		float fMax = xScale.x;
		if (xScale.y < fMin) { fMin = xScale.y; }
		if (xScale.z < fMin) { fMin = xScale.z; }
		if (xScale.y > fMax) { fMax = xScale.y; }
		if (xScale.z > fMax) { fMax = xScale.z; }

		Zenith_Assert(fMin > 0.0f,
			"Zenith_BoneSpace: degenerate parent frame (scale %.6f, %.6f, %.6f) - the recovered rotation is meaningless",
			static_cast<double>(xScale.x), static_cast<double>(xScale.y), static_cast<double>(xScale.z));

		Zenith_Assert(fMax - fMin <= fBONE_SPACE_UNIFORM_SCALE_TOLERANCE * fMax,
			"Zenith_BoneSpace: non-uniform parent scale (%.6f, %.6f, %.6f) - DecomposeTRS cannot recover a rotation from a sheared frame",
			static_cast<double>(xScale.x), static_cast<double>(xScale.y), static_cast<double>(xScale.z));

		const float fDeterminant = glm::determinant(Zenith_Maths::Matrix3(xMatrix));
		Zenith_Assert(fDeterminant > 0.0f,
			"Zenith_BoneSpace: mirrored parent frame (determinant %.6f) - a negative-scale basis has no rotation to recover",
			static_cast<double>(fDeterminant));
	}
}

Zenith_Maths::Matrix4 Zenith_BoneSpace::BoneModelMatrix(const Flux_SkeletonInstance& xSkeleton, u_int uBone)
{
	// Bounds-checked HERE rather than leaning on GetBoneModelTransform's own
	// guard: that one only rejects indices past MAX_BONES, so an index between
	// the live bone count and MAX_BONES would come back as whatever the cache
	// happens to hold for a slot ComputeSkinningMatrices never writes.
	if (uBone >= xSkeleton.GetNumBones())
	{
		return glm::identity<Zenith_Maths::Matrix4>();
	}
	return xSkeleton.GetBoneModelTransform(uBone);
}

Zenith_Maths::Matrix4 Zenith_BoneSpace::ParentModelMatrix(const Flux_SkeletonInstance& xSkeleton, u_int uBone)
{
	// The parent chain is on the ASSET; the instance only carries the pose.
	const Zenith_SkeletonAsset* pxAsset = xSkeleton.GetSourceSkeleton();
	if (pxAsset == nullptr || uBone >= xSkeleton.GetNumBones() || uBone >= pxAsset->GetNumBones())
	{
		return glm::identity<Zenith_Maths::Matrix4>();
	}

	const int32_t iParentIndex = pxAsset->GetBone(uBone).m_iParentIndex;
	if (iParentIndex == Zenith_SkeletonAsset::INVALID_BONE_INDEX || iParentIndex < 0)
	{
		return glm::identity<Zenith_Maths::Matrix4>();
	}

	// Routed back through BoneModelMatrix rather than indexing the cache directly,
	// so a parent index that is out of range for the live pose degrades to
	// identity by the same rule every other read here does.
	return BoneModelMatrix(xSkeleton, static_cast<u_int>(iParentIndex));
}

Zenith_Maths::Matrix4 Zenith_BoneSpace::BoneWorldMatrix(const Zenith_Maths::Matrix4& xSessionModel,
	const Flux_SkeletonInstance& xSkeleton, u_int uBone)
{
	return xSessionModel * BoneModelMatrix(xSkeleton, uBone);
}

Zenith_Maths::Vector3 Zenith_BoneSpace::BoneWorldPosition(const Zenith_Maths::Matrix4& xSessionModel,
	const Flux_SkeletonInstance& xSkeleton, u_int uBone)
{
	return Zenith_Maths::Vector3(BoneWorldMatrix(xSessionModel, xSkeleton, uBone)[3]);
}

Zenith_Maths::Quat Zenith_BoneSpace::ParentWorldRotation(const Zenith_Maths::Matrix4& xSessionModel,
	const Flux_SkeletonInstance& xSkeleton, u_int uBone)
{
	const Zenith_Maths::Matrix4 xParentWorld = xSessionModel * ParentModelMatrix(xSkeleton, uBone);

	Zenith_Maths::Vector3 xPosition(0.0f);
	Zenith_Maths::Quat xRotation = glm::identity<Zenith_Maths::Quat>();
	Zenith_Maths::Vector3 xScale(1.0f);
	Zenith_Maths::DecomposeTRS(xParentWorld, xPosition, xRotation, xScale);

	BoneSpaceAssertRecoverableRotation(xParentWorld, xScale);

	// Already normalised by DecomposeTRS (which does it because Jolt asserts
	// IsNormalized); named here so the conjugation downstream can rely on it.
	return xRotation;
}

Zenith_Maths::Quat Zenith_BoneSpace::WorldDeltaToBoneLocalDelta(const Zenith_Maths::Quat& xWorldDelta,
	const Zenith_Maths::Quat& xParentWorldRotation)
{
	// conj == inverse only for a unit quaternion, so normalise first. Costs one
	// square root per drag frame and removes a whole class of "the pose slowly
	// shrank" report. Note that the sign of qP is irrelevant: conj(-q) * d * (-q)
	// is conj(q) * d * q, which is why a quat_cast that came back double-covered
	// upstream cannot change the answer.
	const Zenith_Maths::Quat xParent = glm::normalize(xParentWorldRotation);
	return glm::conjugate(xParent) * xWorldDelta * xParent;
}

Zenith_Maths::Quat Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(const Zenith_Maths::Quat& xWorldDelta,
	const Zenith_Maths::Quat& xParentWorldRotation,
	const Zenith_Maths::Quat& xBoneLocalRotation)
{
	return WorldDeltaToBoneLocalDelta(xWorldDelta, xParentWorldRotation) * xBoneLocalRotation;
}

#ifdef ZENITH_TESTING
#include "Editor/Animation/Zenith_BoneSpace.Tests.inl"
#endif

#endif // ZENITH_TOOLS
