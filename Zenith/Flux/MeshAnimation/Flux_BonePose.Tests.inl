//------------------------------------------------------------------------------
// Flux_BoneMask unit tests (WU-7.1).
// Included at the bottom of Flux_BonePose.cpp.
//
// ★ THE HEADLINE PROPERTY: a mask means the same thing on two rigs that carry
// the same bone NAMES in a different ORDER. That is the entire reason
// Zenith_BoneMaskAsset stores names and this class stores indices, and it is not
// checkable at all through the pre-existing Flux_MeshGeometry overload — the
// editor never has a Flux_MeshGeometry in hand, and the whole runtime pose path
// (Flux_SkeletonPose::SampleFromClip, the controller, the layers) resolves
// against Zenith_SkeletonAsset instead.
//
// The rest pin the edges that are silent when they are wrong: an index past the
// STORED weight count reads 0 rather than reading somebody else's bytes, a
// name/weight length mismatch is refused WHOLE rather than resolved for the
// prefix, and an all-zero mask is indistinguishable from "no mask" by weight
// alone — which is D47's whole argument for the asset's explicit flag.
//
// All of it is in-memory, headless, device-free. None of it is
// requiresGraphics.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the length-mismatch refusal asserts on purpose

#include <cmath>     // std::abs — WU-8.1's pose comparisons
#include <utility>   // std::move — the clip takes its channel by rvalue

namespace
{
	// Root -> Spine -> Arm, added in that order, so Spine is index 1.
	void BonePoseBuildRigInOrder(Zenith_SkeletonAsset& xSkeleton)
	{
		const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xUnitScale(1.0f);
		xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Arm", 1, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.ComputeBindPoseMatrices();
	}

	// ★ THE SAME THREE BONES, PERMUTED — and still a legal skeleton, which is the
	// only reason this is a fair second rig. Zenith_SkeletonAsset requires a
	// parent to PRECEDE its child (Flux_SkeletonPose::ComputeModelSpaceMatricesFromSkeleton
	// asserts exactly that), so the permutation has to keep the chain ordered
	// while moving the INDICES: two roots, with the Root/Spine/Arm chain hanging
	// off the second slot instead of the first.
	//
	// Result: Spine is index 2 here and index 1 above, Arm is 3 here and 2 above.
	// A mask that resolves to the same INDICES on both rigs would prove nothing.
	void BonePoseBuildRigPermuted(Zenith_SkeletonAsset& xSkeleton)
	{
		const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xUnitScale(1.0f);
		xSkeleton.AddBone("Prop", -1, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Spine", 1, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Arm", 2, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.ComputeBindPoseMatrices();
	}

	Zenith_Vector<std::string> BonePoseNames(const char* szA, const char* szB)
	{
		Zenith_Vector<std::string> xNames;
		xNames.PushBack(std::string(szA));
		xNames.PushBack(std::string(szB));
		return xNames;
	}

	Zenith_Vector<float> BonePoseWeights(float fA, float fB)
	{
		Zenith_Vector<float> xWeights;
		xWeights.PushBack(fA);
		xWeights.PushBack(fB);
		return xWeights;
	}
}

//==============================================================================
// (1) THE ACCEPTANCE PROPERTY — one authored mask, two rigs, matched BY NAME.
//==============================================================================
ZENITH_TEST(BoneMask, OneMaskResolvesByNameOnTwoRigsWithDifferentBoneOrder)
{
	Zenith_SkeletonAsset xRigA;
	BonePoseBuildRigInOrder(xRigA);
	Zenith_SkeletonAsset xRigB;
	BonePoseBuildRigPermuted(xRigB);

	// The premise this test rests on: the two rigs really do number the shared
	// bones differently. Without this the assertions below would pass on two
	// identical skeletons and prove nothing at all.
	ZENITH_ASSERT_EQ(xRigA.GetBoneIndex("Spine"), 1, "rig A numbers Spine 1");
	ZENITH_ASSERT_EQ(xRigB.GetBoneIndex("Spine"), 2, "rig B numbers the SAME bone 2");
	ZENITH_ASSERT_EQ(xRigA.GetBoneIndex("Arm"), 2, "rig A numbers Arm 2");
	ZENITH_ASSERT_EQ(xRigB.GetBoneIndex("Arm"), 3, "rig B numbers the SAME bone 3");

	const Zenith_Vector<std::string> xNames = BonePoseNames("Spine", "Arm");
	const Zenith_Vector<float> xWeights = BonePoseWeights(1.0f, 0.25f);

	Flux_BoneMask xOnA;
	ZENITH_ASSERT_TRUE(xOnA.SetFromBoneNames(xRigA, xNames, xWeights), "every name is on rig A");
	Flux_BoneMask xOnB;
	ZENITH_ASSERT_TRUE(xOnB.SetFromBoneNames(xRigB, xNames, xWeights), "every name is on rig B");

	// ★ THE WEIGHTS FOLLOW THE NAMES, NOT THE SLOTS. Compared per NAME through
	// each rig's own lookup — comparing index i against index i would be
	// comparing two different bones and would fail even when the resolve is
	// perfect.
	for (u_int u = 0; u < xNames.GetSize(); ++u)
	{
		const std::string& strName = xNames.Get(u);
		const uint32_t uIndexA = static_cast<uint32_t>(xRigA.GetBoneIndex(strName));
		const uint32_t uIndexB = static_cast<uint32_t>(xRigB.GetBoneIndex(strName));
		ZENITH_ASSERT_EQ_FLOAT(xOnA.GetBoneWeight(uIndexA), xWeights.Get(u), 1e-5f,
			"the named bone carries its authored weight on rig A");
		ZENITH_ASSERT_EQ_FLOAT(xOnB.GetBoneWeight(uIndexB), xWeights.Get(u), 1e-5f,
			"and the same weight on rig B, at a different index");
	}

	// And nothing the mask did not name was written on either rig.
	ZENITH_ASSERT_EQ_FLOAT(xOnA.GetBoneWeight(0u), 0.0f, 1e-5f, "rig A's Root is unmasked");
	ZENITH_ASSERT_EQ_FLOAT(xOnB.GetBoneWeight(0u), 0.0f, 1e-5f, "rig B's Prop is unmasked");
	ZENITH_ASSERT_EQ_FLOAT(xOnB.GetBoneWeight(1u), 0.0f, 1e-5f, "rig B's Root is unmasked");

	// ★ AND THE TWO MASKS DIFFER BY INDEX, which is the other half of the same
	// fact and the reason a slot-wise resolve would be wrong. Index 2 is Arm on
	// rig A and Spine on rig B; had the weights followed the slots, these two
	// would read the same number.
	ZENITH_ASSERT_EQ_FLOAT(xOnA.GetBoneWeight(2u), 0.25f, 1e-5f, "index 2 is Arm on rig A");
	ZENITH_ASSERT_EQ_FLOAT(xOnB.GetBoneWeight(2u), 1.0f, 1e-5f,
		"the SAME index is Spine on rig B, and carries Spine's weight");
}

//==============================================================================
// (2) An EMPTY weight list means 1.0 per named bone — the mesh-geometry
//     overload's contract, kept so the two do not disagree.
//==============================================================================
ZENITH_TEST(BoneMask, AnEmptyWeightListMeansFullWeightForEveryNamedBone)
{
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);

	const Zenith_Vector<std::string> xNames = BonePoseNames("Spine", "Arm");
	const Zenith_Vector<float> xNoWeights;

	Flux_BoneMask xMask;
	ZENITH_ASSERT_TRUE(xMask.SetFromBoneNames(xRig, xNames, xNoWeights), "both names resolve");
	ZENITH_ASSERT_EQ_FLOAT(xMask.GetBoneWeight(1u), 1.0f, 1e-5f, "Spine gets the implicit full weight");
	ZENITH_ASSERT_EQ_FLOAT(xMask.GetBoneWeight(2u), 1.0f, 1e-5f, "and so does Arm");
	ZENITH_ASSERT_EQ_FLOAT(xMask.GetBoneWeight(0u), 0.0f, 1e-5f, "and Root, unnamed, is still 0");
}

//==============================================================================
// (3) A name/weight LENGTH MISMATCH is refused WHOLE.
//
// ★ NOT RESOLVED FOR THE AGREEING PREFIX. A weight attached to the wrong bone
// is worse than no mask: an override layer with a mask on the wrong half of a
// skeleton animates the wrong half, and nothing anywhere reports it.
//==============================================================================
ZENITH_TEST(BoneMask, AMismatchedNameAndWeightCountIsRefusedWhole)
{
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);

	Zenith_Vector<std::string> xNames;
	xNames.PushBack(std::string("Root"));
	xNames.PushBack(std::string("Spine"));
	xNames.PushBack(std::string("Arm"));

	Zenith_Vector<float> xTooFewWeights;
	xTooFewWeights.PushBack(1.0f);
	xTooFewWeights.PushBack(1.0f);

	Flux_BoneMask xMask;
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(xMask.SetFromBoneNames(xRig, xNames, xTooFewWeights),
			"three names against two weights is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and it asserts exactly once");
	}

	// The mask is left FULLY ZEROED — not half-applied. A caller that ignored the
	// return value gets "this layer masks nothing", which is at least a state it
	// can see, rather than two bones weighted from a third's slot.
	ZENITH_ASSERT_FALSE(xMask.HasAnyNonZeroWeight(), "nothing was written by the refused call");
	ZENITH_ASSERT_EQ(xMask.GetWeightCount(), (u_int)FLUX_MAX_BONES, "and the mask is still a complete one");
}

//==============================================================================
// (4) An unresolvable name is LISTED and its weight dropped; everything else
//     still resolves.
//==============================================================================
ZENITH_TEST(BoneMask, AnUnresolvableNameIsListedAndTheRestStillResolve)
{
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);

	Zenith_Vector<std::string> xNames;
	xNames.PushBack(std::string("Spine"));
	xNames.PushBack(std::string("Elbow"));   // not on this rig — a typo, or the wrong rig
	Zenith_Vector<float> xWeights;
	xWeights.PushBack(1.0f);
	xWeights.PushBack(0.75f);

	Flux_BoneMask xMask;
	Zenith_Vector<std::string> xUnresolved;
	ZENITH_ASSERT_FALSE(xMask.SetFromBoneNames(xRig, xNames, xWeights, &xUnresolved),
		"a name the rig does not carry fails the resolve");
	ZENITH_ASSERT_EQ(xUnresolved.GetSize(), 1u, "and is REPORTED rather than counted");
	if (xUnresolved.GetSize() == 1u)
	{
		ZENITH_ASSERT_STREQ(xUnresolved.Get(0).c_str(), "Elbow", "by name — which is what makes it actionable");
	}

	// Everything resolvable is still applied, so a caller may carry on with a
	// partial mask; it just cannot do so unknowingly.
	ZENITH_ASSERT_EQ_FLOAT(xMask.GetBoneWeight(1u), 1.0f, 1e-5f, "Spine still resolved");

	// The out-list is CLEARED by each call, so a second, clean resolve does not
	// hand back the previous one's misses.
	Zenith_Vector<std::string> xNamesOk;
	xNamesOk.PushBack(std::string("Spine"));
	Zenith_Vector<float> xWeightsOk;
	xWeightsOk.PushBack(1.0f);
	ZENITH_ASSERT_TRUE(xMask.SetFromBoneNames(xRig, xNamesOk, xWeightsOk, &xUnresolved),
		"a clean resolve reports success");
	ZENITH_ASSERT_EQ(xUnresolved.GetSize(), 0u, "and empties the report");
}

//==============================================================================
// (5) A weight past the STORED count reads 0 — never out of range.
//
// ★ THE CONSTRUCTOR AND THE STREAM READER DISAGREE ABOUT THE LENGTH, WHICH IS
// WHY THIS MATTERS. Flux_BoneMask() fills FLUX_MAX_BONES zeroes;
// ReadFromDataStream sizes to whatever count the stream carried, which may be
// far fewer. Every accessor is bounds-checked against the STORED count.
//==============================================================================
ZENITH_TEST(BoneMask, AWeightPastTheStoredCountReadsZeroRatherThanOutOfRange)
{
	Flux_BoneMask xFresh;
	ZENITH_ASSERT_EQ(xFresh.GetWeightCount(), (u_int)FLUX_MAX_BONES,
		"a constructed mask is FLUX_MAX_BONES long");

	// A stream carrying THREE weights, which is a legal thing for a scene to
	// hold: the wire format writes a count and that many floats, and nothing
	// obliges a writer to have used a full-length mask.
	Zenith_DataStream xStream;
	const uint32_t uCount = 3u;
	xStream << uCount;
	const float afWeights[3] = { 0.0f, 1.0f, 0.5f };
	for (uint32_t u = 0; u < uCount; ++u)
	{
		xStream << afWeights[u];
	}
	xStream.SetCursor(0);

	Flux_BoneMask xShort;
	xShort.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ(xShort.GetWeightCount(), 3u, "the mask is exactly as long as the stream said");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(1u), 1.0f, 1e-5f, "an in-range weight reads back");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(2u), 0.5f, 1e-5f, "including the last one");

	// The three that matter: one past the end, well past the end, and past
	// FLUX_MAX_BONES itself.
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(3u), 0.0f, 1e-5f, "one past the stored count is 0");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(64u), 0.0f, 1e-5f, "and so is anything further");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(FLUX_MAX_BONES + 17u), 0.0f, 1e-5f,
		"and an index past FLUX_MAX_BONES entirely");

	// A write past the end is dropped rather than growing the array, so the two
	// accessors agree about what this mask describes.
	xShort.SetBoneWeight(50u, 1.0f);
	ZENITH_ASSERT_EQ(xShort.GetWeightCount(), 3u, "SetBoneWeight past the end does not extend the mask");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(50u), 0.0f, 1e-5f, "and reads back as 0");

	// ★ A RESOLVE RE-FILLS IT. A short mask resolved against a rig has to come
	// back a COMPLETE one, or every bone past the old end would silently keep
	// answering 0 whatever the mask named.
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);
	Zenith_Vector<std::string> xNames;
	xNames.PushBack(std::string("Arm"));
	Zenith_Vector<float> xWeights;
	xWeights.PushBack(0.5f);
	ZENITH_ASSERT_TRUE(xShort.SetFromBoneNames(xRig, xNames, xWeights), "the resolve succeeds");
	ZENITH_ASSERT_EQ(xShort.GetWeightCount(), (u_int)FLUX_MAX_BONES, "and the mask is full length again");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(1u), 0.0f, 1e-5f, "with the previous contents gone");
	ZENITH_ASSERT_EQ_FLOAT(xShort.GetBoneWeight(2u), 0.5f, 1e-5f, "and only what was named written");
}

//==============================================================================
// (6) D47 — an all-zero mask is INDISTINGUISHABLE from no mask by weight alone.
//
// This is the whole argument for Zenith_BoneMaskAsset::m_bHasAvatarMask being
// serialized rather than derived, stated where the derivation lives:
// Flux_AnimationLayer::ReadFromDataStream decides "this layer has a mask" with
// exactly this predicate, so a scene-inline all-zero mask reads back as NO MASK
// — and an OVERRIDE layer with no mask replaces the WHOLE skeleton. The byte
// layout is frozen (committed .zscen files carry it), so that stays true for a
// scene; the asset is where the flag lives.
//==============================================================================
ZENITH_TEST(BoneMask, AnAllZeroMaskCannotBeToldFromNoMaskByItsWeights)
{
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);

	const Zenith_Vector<std::string> xNames = BonePoseNames("Spine", "Arm");
	const Zenith_Vector<float> xZeroWeights = BonePoseWeights(0.0f, 0.0f);

	Flux_BoneMask xExplicitlyEmpty;
	ZENITH_ASSERT_TRUE(xExplicitlyEmpty.SetFromBoneNames(xRig, xNames, xZeroWeights),
		"an all-zero mask is a legal, fully resolvable mask");
	ZENITH_ASSERT_FALSE(xExplicitlyEmpty.HasAnyNonZeroWeight(),
		"and carries no non-zero weight — the derivation says 'no mask'");

	Flux_BoneMask xNeverTouched;
	ZENITH_ASSERT_FALSE(xNeverTouched.HasAnyNonZeroWeight(),
		"which is exactly what a mask nobody authored says too");

	// One non-zero weight anywhere flips it, so the predicate is not simply
	// stuck at false.
	xExplicitlyEmpty.SetBoneWeight(1u, 0.001f);
	ZENITH_ASSERT_TRUE(xExplicitlyEmpty.HasAnyNonZeroWeight(), "any non-zero weight is enough");
}

//==============================================================================
// WU-8.1 — TANGENT SAMPLING, SEEN FROM THE POSE PATH.
//
// ★ Flux_SkeletonPose::SampleFromClip NEEDED NO CHANGE, AND THAT IS A CLAIM WORTH
// A TEST RATHER THAN A COMMENT. It calls Flux_BoneChannel::Sample*() per bone
// behind the Has*Keyframes() guards and does nothing else with the values, so a
// tangent authored on a channel reaches a bone's local pose for free — or it does
// not, if some layer between the two flattened it. The two units below are the
// difference: one fixes what the pose path produces for the zero-tangent clips the
// whole tree is made of, the other proves an authored tangent actually arrives.
//
// In-memory skeleton, in-memory clip, no device and no file: not requiresGraphics.
//==============================================================================

namespace
{
	bool PoseVec3Equals(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fTol = 1e-5f)
	{
		return std::abs(xA.x - xB.x) < fTol
			&& std::abs(xA.y - xB.y) < fTol
			&& std::abs(xA.z - xB.z) < fTol;
	}

	// Root -> Spine -> Arm (Spine is index 1), with ONE animated bone: Spine, on a
	// single position segment from the origin to (4,0,0) over two seconds. Nothing
	// animates Root or Arm, so their bind poses are the control.
	void PoseBuildSpineOnlyClip(Flux_AnimationClip& xClip)
	{
		xClip.SetName("TangentProbe");
		xClip.SetDuration(2.0f);

		Flux_BoneChannel xChannel;
		xChannel.SetBoneName("Spine");
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xChannel.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		xChannel.SortKeyframes();
		xClip.AddBoneChannel("Spine", std::move(xChannel));
	}
}

ZENITH_TEST(SkeletonPose, SampleFromClipIsUnchangedForAClipWithNoTangents)
{
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);

	Flux_AnimationClip xClip;
	PoseBuildSpineOnlyClip(xClip);

	Flux_SkeletonPose xPose;
	xPose.InitFromBindPose(xRig);
	xPose.SampleFromClip(xClip, 1.0f, xRig);

	// Halfway along a two-key segment with no authored tangent: still the lerp.
	ZENITH_ASSERT_TRUE(PoseVec3Equals(xPose.GetLocalPose(1u).m_xPosition, Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f)),
		"a zero-tangent clip poses exactly where it always did");

	// The Has*Keyframes() guards are untouched: a bone the clip does not animate,
	// and a TRACK the channel does not carry, both keep the bind pose.
	ZENITH_ASSERT_TRUE(PoseVec3Equals(xPose.GetLocalPose(2u).m_xPosition, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f)),
		"Arm has no channel and keeps its bind position");
	ZENITH_ASSERT_TRUE(PoseVec3Equals(xPose.GetLocalPose(1u).m_xScale, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)),
		"Spine has no SCALE keys, so its bind scale survives");
}

ZENITH_TEST(SkeletonPose, SampleFromClipCarriesAChannelTangentIntoThePose)
{
	Zenith_SkeletonAsset xRig;
	BonePoseBuildRigInOrder(xRig);

	Flux_AnimationClip xClip;
	PoseBuildSpineOnlyClip(xClip);

	// The same hand-computed segment as Flux_AnimationClip.Tests.inl's Hermite unit:
	// dt = 2, u = 0.5, m0 = (0,6,0), m1 = (0,-6,0) => (2, 3, 0), against a lerp of
	// (2, 0, 0). Authored through GetBoneChannelMutable, which is the route the
	// editor's document layer uses.
	Flux_BoneChannel* pxChannel = xClip.GetBoneChannelMutable("Spine");
	ZENITH_ASSERT_NOT_NULL(pxChannel, "the Spine channel is reachable for mutation");
	if (pxChannel == nullptr)
	{
		return;
	}

	Flux_KeyTangents xStart;
	xStart.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 6.0f, 0.0f);
	pxChannel->SetPositionTangent(0u, xStart);

	Flux_KeyTangents xEnd;
	xEnd.m_xInTangent = Zenith_Maths::Vector3(0.0f, -6.0f, 0.0f);
	pxChannel->SetPositionTangent(1u, xEnd);

	Flux_SkeletonPose xPose;
	xPose.InitFromBindPose(xRig);
	xPose.SampleFromClip(xClip, 1.0f, xRig);

	ZENITH_ASSERT_TRUE(PoseVec3Equals(xPose.GetLocalPose(1u).m_xPosition, Zenith_Maths::Vector3(2.0f, 3.0f, 0.0f)),
		"the authored tangent reaches the bone's LOCAL POSE, not only the channel");

	// And the keys themselves are still where they were authored — a tangent bends
	// the curve between keys and must never move one.
	xPose.SampleFromClip(xClip, 0.0f, xRig);
	ZENITH_ASSERT_TRUE(PoseVec3Equals(xPose.GetLocalPose(1u).m_xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)),
		"the pose at the first key time is still the first key");
	xPose.SampleFromClip(xClip, 2.0f, xRig);
	ZENITH_ASSERT_TRUE(PoseVec3Equals(xPose.GetLocalPose(1u).m_xPosition, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f)),
		"and at the last key time it is the last key");
}
