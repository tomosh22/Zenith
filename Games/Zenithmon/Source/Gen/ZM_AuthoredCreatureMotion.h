#pragma once

#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// CPU authoring measurement, using the same bind-relative linear skin as the
// renderer. This measures the complete surface, including attached details.
inline float ZM_AuthoredPoseMinY(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, const Flux_AnimationClip& xClip, float fTime)
{
	std::vector<Zenith_Maths::Matrix4> axWorld(xRig.GetNumBones());
	std::vector<Zenith_Maths::Vector4> axSkinY(xRig.GetNumBones());
	for (u_int b = 0; b < xRig.GetNumBones(); ++b)
	{
		const auto& xBone = xRig.GetBone(b); const auto* pxChannel = xClip.GetBoneChannel(xBone.m_strName);
		const auto xPosition = pxChannel && pxChannel->HasPositionKeyframes() ? pxChannel->SamplePosition(fTime) : xBone.m_xBindPosition;
		const auto xRotation = pxChannel && pxChannel->HasRotationKeyframes() ? pxChannel->SampleRotation(fTime) : xBone.m_xBindRotation;
		const auto xScale = pxChannel && pxChannel->HasScaleKeyframes() ? pxChannel->SampleScale(fTime) : xBone.m_xBindScale;
		const auto xLocal = glm::translate(Zenith_Maths::Matrix4(1), xPosition) * glm::mat4_cast(xRotation) * glm::scale(Zenith_Maths::Matrix4(1), xScale);
		axWorld[b] = xBone.m_iParentIndex < 0 ? xLocal : axWorld[xBone.m_iParentIndex] * xLocal;
		const auto xSkin = axWorld[b] * xBone.m_xInverseBindPose;
		axSkinY[b] = {xSkin[0][1], xSkin[1][1], xSkin[2][1], xSkin[3][1]};
	}
	float fMin = std::numeric_limits<float>::max();
	for (u_int v = 0; v < xMesh.GetNumVerts(); ++v)
	{
		const auto& xIds = xMesh.m_xBoneIndices.Get(v); const auto& xWeights = xMesh.m_xBoneWeights.Get(v);
		const Zenith_Maths::Vector4 xPosition(xMesh.m_xPositions.Get(v), 1);
		float fY = 0;
		for (u_int w = 0; w < 4; ++w) fY += xWeights[w] * glm::dot(axSkinY[xIds[w]], xPosition);
		if (!std::isfinite(fY)) return std::numeric_limits<float>::quiet_NaN();
		fMin = std::min(fMin, fY);
	}
	return fMin;
}

// The grove-deer anatomical sources have horizontal trunks, unlike the generated
// placeholder. Their collapse therefore needs an authored side fall, not the
// placeholder's cumulative 135-degree spine pitch. This is deliberately an
// explicit family recipe; it does not change the shared archetype templates.
inline bool ZM_BuildGroveDeerFaintForArtistRig(ZM_SPECIES_ID eSpecies, const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, Flux_AnimationClip& xOut)
{
	if (!xMesh.HasSkinning() || !xMesh.GetNumVerts() || xRig.GetBoneIndex("Spine00") != 0 ||
		xMesh.m_xPositions.GetSize() != xMesh.GetNumVerts() ||
		xMesh.m_xBoneIndices.GetSize() != xMesh.GetNumVerts() || xMesh.m_xBoneWeights.GetSize() != xMesh.GetNumVerts()) return false;
	for (u_int b = 0; b < xRig.GetNumBones(); ++b)
		if (xRig.GetBone(b).m_iParentIndex >= static_cast<int>(b) || (b && xRig.GetBone(b).m_iParentIndex < 0)) return false;
	for (u_int v = 0; v < xMesh.GetNumVerts(); ++v)
		for (u_int w = 0; w < 4; ++w)
			if (xMesh.m_xBoneIndices.Get(v)[w] >= xRig.GetNumBones()) return false;

	Flux_AnimationClip xReference; ZM_BuildCreatureClipForSpecies(eSpecies, ZM_ANIM_CLIP_FAINT, xReference);
	Flux_AnimationClip xCandidate; xCandidate.GetMetadata() = xReference.GetMetadata();
	const float fDuration = xCandidate.GetDuration();
	struct Pose { const char* m_szBone; float m_fX, m_fZ; };
	const Pose axFinal[] = {
		{"Spine00", 0, 82}, {"Spine01", 4, 0}, {"Spine02", 6, 0}, {"Spine03", 8, 0}, {"Head", 18, -8},
		{"LegFLUp", 42, -8}, {"LegFLLo", -72, 0}, {"LegFRUp", 35, 10}, {"LegFRLo", -65, 0},
		{"LegHLUp", -30, -8}, {"LegHLLo", 65, 0}, {"LegHRUp", -26, 8}, {"LegHRLo", 58, 0},
		{"Tail00", 6, -8}, {"Tail01", 8, 0}, {"Tail02", 10, 0}, {"HornL", 0, 0}, {"HornR", 0, 0}
	};
	constexpr u_int uSamples = 72;
	for (const auto& xPose : axFinal)
	{
		if (!xRig.HasBone(xPose.m_szBone)) return false;
		Flux_BoneChannel xChannel; xChannel.SetBoneName(xPose.m_szBone);
		for (u_int k = 0; k <= uSamples; ++k)
		{
			const float fT = static_cast<float>(k) / uSamples;
			// Knees yield first; body rolls after the animal loses support. The
			// final 15% is a held downed pose rather than a return to standing.
			const bool bRoot = std::string(xPose.m_szBone) == "Spine00";
			const float fPhase = std::clamp((fT - (bRoot ? .15f : 0.f)) / (bRoot ? .70f : .65f), 0.f, 1.f);
			const float fEase = fPhase * fPhase * (3 - 2 * fPhase);
			const auto xRot = glm::angleAxis(glm::radians(xPose.m_fZ * fEase), Zenith_Maths::Vector3(0,0,1)) *
				glm::angleAxis(glm::radians(xPose.m_fX * fEase), Zenith_Maths::Vector3(1,0,0));
			xChannel.AddRotationKeyframe(fDuration * fT, xRot);
		}
		xCandidate.AddBoneChannel(xPose.m_szBone, std::move(xChannel));
	}
	// The body falls to the ground as part of the clip, not as a screenshot-only
	// entity offset. Measure with the untouched bind root, then add all position
	// keys together so earlier keys cannot contaminate later measurements.
	std::vector<Zenith_Maths::Vector3> axRootPositions;
	for (u_int k = 0; k <= uSamples; ++k)
	{
		const float fMinY = ZM_AuthoredPoseMinY(xRig, xMesh, xCandidate, fDuration * k / uSamples);
		if (!std::isfinite(fMinY)) return false;
		auto xPosition = xRig.GetBone(0).m_xBindPosition; xPosition.y += .002f - fMinY;
		axRootPositions.push_back(xPosition);
	}
	auto* pxRoot = xCandidate.GetBoneChannelMutable("Spine00");
	for (u_int k = 0; k <= uSamples; ++k) pxRoot->AddPositionKeyframe(fDuration * k / uSamples, axRootPositions[k]);
	// Check interpolated poses as well as the authored keys. A passing scalar
	// floor check is not an assertion about self-intersection or visual quality.
	for (u_int k = 0; k <= uSamples * 2; ++k)
	{
		const float fMinY = ZM_AuthoredPoseMinY(xRig, xMesh, xCandidate, fDuration * k / (uSamples * 2));
		if (!std::isfinite(fMinY) || fMinY < -.001f || fMinY > .005f) return false;
	}
	xOut = std::move(xCandidate); return true;
}
// The stone line articulates separate rock layers on the four blob joints.
// Preserve the six archetype actions, but replace its soft-body deflation with
// a held sideways topple. Ground contact is measured against each artist mesh
// throughout every clip, not corrected only for a selected screenshot pose.
inline bool ZM_FitStoneCreatureClip(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xClip)
{
	if (xRig.GetNumBones() != 4 || xRig.GetBoneIndex("Spine00") != 0 || !xMesh.HasSkinning() || !xMesh.GetNumVerts()) return false;
	const char* aszBones[] = {"Spine00", "Spine01", "Spine02", "Nub"};
	for (u_int b = 0; b < 4; ++b)
		if (xRig.GetBone(b).m_strName != aszBones[b] || xRig.GetBone(b).m_iParentIndex != static_cast<int>(b) - 1) return false;
	for (u_int v = 0; v < xMesh.GetNumVerts(); ++v)
		for (u_int w = 0; w < 4; ++w) if (xMesh.m_xBoneIndices.Get(v)[w] >= 4) return false;
	const float fDuration = xClip.GetDuration();
	const u_int uSamples = static_cast<u_int>(std::ceil(fDuration * 60));
	if (!uSamples) return false;
	if (eClip == ZM_ANIM_CLIP_FAINT)
	{
		Flux_AnimationClip xCandidate; xCandidate.GetMetadata() = xClip.GetMetadata();
		const float afZ[] = {78,-12,8,-18}; const float afX[] = {0,12,-18,15};
		for (u_int b = 0; b < 4; ++b)
		{
			Flux_BoneChannel channel; channel.SetBoneName(aszBones[b]);
			for (u_int k = 0; k <= uSamples; ++k)
			{
				float fT = std::clamp((static_cast<float>(k) / uSamples - .08f) / .78f, 0.f, 1.f);
				fT = fT * fT * (3 - 2 * fT);
				channel.AddRotationKeyframe(fDuration * k / uSamples,
					glm::angleAxis(glm::radians(afZ[b] * fT), Zenith_Maths::Vector3(0,0,1)) *
					glm::angleAxis(glm::radians(afX[b] * fT), Zenith_Maths::Vector3(1,0,0)));
			}
			xCandidate.AddBoneChannel(aszBones[b], std::move(channel));
		}
		xClip = std::move(xCandidate);
	}
	if (!xClip.GetBoneChannel("Spine00"))
	{
		Flux_BoneChannel root; root.SetBoneName("Spine00");
		root.AddRotationKeyframe(0, xRig.GetBone(0).m_xBindRotation);
		xClip.AddBoneChannel("Spine00", std::move(root));
	}
	std::vector<Zenith_Maths::Vector3> axPositions;
	const float fFloor = xMesh.GetBoundsMin().y + .002f;
	for (u_int k = 0; k <= uSamples; ++k)
	{
		const float fMin = ZM_AuthoredPoseMinY(xRig, xMesh, xClip, fDuration * k / uSamples);
		if (!std::isfinite(fMin)) return false;
		auto p = xRig.GetBone(0).m_xBindPosition; p.y += fFloor - fMin; axPositions.push_back(p);
	}
	auto* root = xClip.GetBoneChannelMutable("Spine00");
	for (u_int k = 0; k <= uSamples; ++k) root->AddPositionKeyframe(fDuration * k / uSamples, axPositions[k]);
	for (u_int k = 0; k <= uSamples * 2; ++k)
	{
		const float fMin = ZM_AuthoredPoseMinY(xRig, xMesh, xClip, fDuration * k / (uSamples * 2));
		if (!std::isfinite(fMin) || std::abs(fMin - fFloor) > .003f) return false;
	}
	return true;
}

inline bool ZM_FitAuthoredSurfaceClearance(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, Flux_AnimationClip& xClip);

// Broad fungal caps remain intact while their stipes yield. The generated
// floater faint pitches its head through the stem; use a restrained wilt for
// these three anatomical sources. Measure the whole skinned surface in every
// clip so the hanging roots retain the battle director's hover clearance.
inline bool ZM_FitFungalCreatureClip(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xClip,
	bool bAdaptiveClearance = false)
{
	const char* aszBones[] = {"Spine00", "Spine01", "Spine02", "Head",
		"Tendril0", "Tendril1", "Tendril2", "Tendril3", "Tendril4", "Tendril5"};
	if (xRig.GetNumBones() != 10 || !xMesh.HasSkinning() || !xMesh.GetNumVerts()) return false;
	for (u_int b = 0; b < 10; ++b)
		if (xRig.GetBone(b).m_strName != aszBones[b] ||
			xRig.GetBone(b).m_iParentIndex != (b < 4 ? static_cast<int>(b) - 1 : 0)) return false;
	for (u_int v = 0; v < xMesh.GetNumVerts(); ++v)
		for (u_int w = 0; w < 4; ++w) if (xMesh.m_xBoneIndices.Get(v)[w] >= 10) return false;
	const float fDuration = xClip.GetDuration();
	const u_int uSamples = static_cast<u_int>(std::ceil(fDuration * 60));
	if (!uSamples) return false;
	if (eClip == ZM_ANIM_CLIP_FAINT)
	{
		Flux_AnimationClip candidate; candidate.GetMetadata() = xClip.GetMetadata();
		for (u_int b = 0; b < 10; ++b)
		{
			Flux_BoneChannel channel; channel.SetBoneName(aszBones[b]);
			for (u_int k = 0; k <= uSamples; ++k)
			{
				float t = std::clamp((static_cast<float>(k) / uSamples - .08f) / .78f, 0.f, 1.f);
				t = t * t * (3 - 2 * t);
				Zenith_Maths::Quaternion rotation;
				if (b < 4)
				{
					const float ax[] = {0,6,8,12}, az[] = {45,-4,-3,-5};
					rotation = glm::angleAxis(glm::radians(az[b] * t), Zenith_Maths::Vector3(0,0,1)) *
						glm::angleAxis(glm::radians(ax[b] * t), Zenith_Maths::Vector3(1,0,0));
				}
				else
				{
					const float theta = (b - 4) * 6.28318530718f / 6;
					rotation = glm::angleAxis(glm::radians(-20.f * t), Zenith_Maths::Vector3(-std::sin(theta),0,std::cos(theta)));
				}
				channel.AddRotationKeyframe(fDuration * k / uSamples, rotation);
			}
			candidate.AddBoneChannel(aszBones[b], std::move(channel));
		}
		xClip = std::move(candidate);
	}
	if (!xClip.GetBoneChannel("Spine00"))
	{
		Flux_BoneChannel root; root.SetBoneName("Spine00");
		root.AddRotationKeyframe(0, xRig.GetBone(0).m_xBindRotation);
		xClip.AddBoneChannel("Spine00", std::move(root));
	}
	// Windseed leaves can exchange the lowest point between the floater's
	// rotation knots. Preserve those knots and refine the intervening motion.
	if (bAdaptiveClearance) return ZM_FitAuthoredSurfaceClearance(xRig, xMesh, xClip);
	const float floor = xMesh.GetBoundsMin().y + .002f;
	std::vector<Zenith_Maths::Vector3> positions;
	for (u_int k = 0; k <= uSamples; ++k)
	{
		const float y = ZM_AuthoredPoseMinY(xRig, xMesh, xClip, fDuration * k / uSamples);
		if (!std::isfinite(y)) return false;
		auto p = xRig.GetBone(0).m_xBindPosition; p.y += floor - y; positions.push_back(p);
	}
	auto* root = xClip.GetBoneChannelMutable("Spine00");
	for (u_int k = 0; k <= uSamples; ++k) root->AddPositionKeyframe(fDuration * k / uSamples, positions[k]);
	for (u_int k = 0; k <= uSamples * 2; ++k)
	{
		const float y = ZM_AuthoredPoseMinY(xRig, xMesh, xClip, fDuration * k / (uSamples * 2));
		if (!std::isfinite(y) || std::abs(y - floor) > .003f) return false;
	}
	return true;
}
// Adaptively fit whole-surface clearance for anatomical sources. Rotation
// knots and midpoint refinement preserve abrupt changes of the lowest point.
inline bool ZM_FitAuthoredSurfaceClearance(const Zenith_SkeletonAsset& xRig,
    const Zenith_MeshAsset& xMesh, Flux_AnimationClip& xClip)
{
    const float duration = xClip.GetDuration();
    const u_int samples = static_cast<u_int>(std::ceil(duration * 60));
    if (!samples) return false;
	if (!xClip.GetBoneChannel("Spine00"))
	{
		Flux_BoneChannel root; root.SetBoneName("Spine00");
		root.AddRotationKeyframe(0,xRig.GetBone(0).m_xBindRotation);
		xClip.AddBoneChannel("Spine00",std::move(root));
	}
	const float floor = xMesh.GetBoundsMin().y + .002f;
	// Include the original rotation knots: their changes in angular velocity
	// need matching clearance keys even when they fall between 60 Hz samples.
	std::vector<float> times;
	for (u_int k = 0; k <= samples; ++k) times.push_back(duration*k/samples);
	for (u_int bone = 0; bone < xRig.GetNumBones(); ++bone)
		if (const auto* channel = xClip.GetBoneChannel(xRig.GetBone(bone).m_strName))
			for (u_int k = 0; k < channel->GetRotationKeyframes().GetSize(); ++k)
				times.push_back(channel->GetRotationKeyframes().Get(k).second);
	std::sort(times.begin(), times.end());
	times.erase(std::unique(times.begin(), times.end()), times.end());
	std::vector<Zenith_Maths::Vector3> positions;
	for (u_int pass = 0; pass < 9; ++pass)
	{
		positions.clear();
		for (const float time : times)
		{
			const float y = ZM_AuthoredPoseMinY(xRig,xMesh,xClip,time);
			if (!std::isfinite(y)) return false;
			auto p = xRig.GetBone(0).m_xBindPosition; p.y += floor-y; positions.push_back(p);
		}
		// The lowest point can switch abruptly from fin to bill between keys.
		// Refine those intervals instead of accepting a larger floor error.
		std::vector<float> extra;
		for (size_t k = 1; k < times.size(); ++k)
		{
			const float time = (times[k-1]+times[k])*.5f;
			const float y = ZM_AuthoredPoseMinY(xRig,xMesh,xClip,time);
			const float offset = (positions[k-1].y+positions[k].y)*.5f-xRig.GetBone(0).m_xBindPosition.y;
			if (!std::isfinite(y)) return false;
			if (std::abs(y+offset-floor) > .0005f) extra.push_back(time);
		}
		if (extra.empty()) break;
		if (pass == 8) return false;
		times.insert(times.end(),extra.begin(),extra.end());
		std::sort(times.begin(),times.end());
	}
	auto* root = xClip.GetBoneChannelMutable("Spine00");
	for (size_t k = 0; k < times.size(); ++k) root->AddPositionKeyframe(times[k],positions[k]);
	for (u_int k = 0; k <= samples*2; ++k)
	{
		const float y = ZM_AuthoredPoseMinY(xRig,xMesh,xClip,duration*k/(samples*2));
		if (!std::isfinite(y) || std::abs(y-floor)>.003f)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "Authored clip clearance error at %.4f: %.6f", duration*k/(samples*2), y-floor);
			return false;
		}
	}
	for (size_t k = 1; k < times.size(); ++k)
	{
		const float y = ZM_AuthoredPoseMinY(xRig,xMesh,xClip,(times[k-1]+times[k])*.5f);
		if (!std::isfinite(y) || std::abs(y-floor)>.003f) return false;
	}
	return true;
}

// Horizontal lancefish use their anatomical spine, with the existing swim/ram
// actions. A whole-body roll replaces the placeholder's cumulative spine twist
// for Faint. Surface clearance includes every fin, rostrum and armour plate.
inline bool ZM_FitLancefishClip(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xClip)
{
	const char* names[] = {"Spine00","Spine01","Spine02","Head","FinDorsal","FinPecL","FinPecR","FinCaudal"};
	const int parents[] = {-1,0,1,2,2,1,1,0};
	if (xRig.GetNumBones() != 8 || !xMesh.HasSkinning() || !xMesh.GetNumVerts()) return false;
	for (u_int b = 0; b < 8; ++b)
		if (xRig.GetBone(b).m_strName != names[b] || xRig.GetBone(b).m_iParentIndex != parents[b]) return false;
	for (u_int v = 0; v < xMesh.GetNumVerts(); ++v)
		for (u_int w = 0; w < 4; ++w) if (xMesh.m_xBoneIndices.Get(v)[w] >= 8) return false;
	const float duration = xClip.GetDuration();
	const u_int samples = static_cast<u_int>(std::ceil(duration * 60));
	if (!samples) return false;
	// The placeholder stacks large pitch angles up a vertical spine. Preserve
	// its action timing but keep the horizontal fish's trunk and bill coherent.
	if (eClip == ZM_ANIM_CLIP_ATTACK || eClip == ZM_ANIM_CLIP_SPECIAL || eClip == ZM_ANIM_CLIP_HIT)
	{
		Flux_AnimationClip candidate; candidate.GetMetadata() = xClip.GetMetadata();
		for (u_int b = 0; b < 8; ++b)
		{
			const auto* source = xClip.GetBoneChannel(names[b]); if (!source) continue;
			Flux_BoneChannel channel; channel.SetBoneName(names[b]);
			const float strength = b < 4 ? .35f : .70f;
			const auto& keys = source->GetRotationKeyframes();
			for (u_int k = 0; k < keys.GetSize(); ++k)
				channel.AddRotationKeyframe(keys.Get(k).second,
					glm::slerp(glm::identity<Zenith_Maths::Quaternion>(), keys.Get(k).first, strength));
			candidate.AddBoneChannel(names[b], std::move(channel));
		}
		xClip = std::move(candidate);
	}
	if (eClip == ZM_ANIM_CLIP_FAINT)
	{
		Flux_AnimationClip candidate; candidate.GetMetadata() = xClip.GetMetadata();
		const float anglesX[] = {-6,2,4,6,-14,0,0,0};
		const float anglesY[] = {0,0,0,0,0,0,0,15};
		const float anglesZ[] = {88,0,0,0,0,-18,18,0};
		for (u_int b = 0; b < 8; ++b)
		{
			Flux_BoneChannel channel; channel.SetBoneName(names[b]);
			for (u_int k = 0; k <= samples; ++k)
			{
				float t = std::clamp((static_cast<float>(k) / samples - .08f) / .80f, 0.f, 1.f);
				t = t * t * (3 - 2 * t);
				channel.AddRotationKeyframe(duration * k / samples,
					glm::angleAxis(glm::radians(anglesZ[b]*t),Zenith_Maths::Vector3(0,0,1)) *
					glm::angleAxis(glm::radians(anglesY[b]*t),Zenith_Maths::Vector3(0,1,0)) *
					glm::angleAxis(glm::radians(anglesX[b]*t),Zenith_Maths::Vector3(1,0,0)));
			}
			candidate.AddBoneChannel(names[b],std::move(channel));
		}
		xClip = std::move(candidate);
	}
	return ZM_FitAuthoredSurfaceClearance(xRig,xMesh,xClip);
}

// Rigid beetle sclerites articulate at their joints. Restrict the placeholder's
// large soft-body bends and use horizontal hip sweeps for the tripod gait.
inline bool ZM_FitMineralBeetleClip(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xClip)
{
	const char* names[] = {"Spine00","Spine01","Spine02","Spine03","Head",
		"LegL0Up","LegL0Lo","LegL1Up","LegL1Lo","LegL2Up","LegL2Lo",
		"LegR0Up","LegR0Lo","LegR1Up","LegR1Lo","LegR2Up","LegR2Lo","AntennaL","AntennaR"};
	const int parents[] = {-1,0,1,2,3,3,5,2,7,1,9,3,11,2,13,1,15,4,4};
	if (xRig.GetNumBones()!=19 || !xMesh.HasSkinning() || !xMesh.GetNumVerts()) return false;
	for (u_int b=0;b<19;++b)
		if (xRig.GetBone(b).m_strName!=names[b] || xRig.GetBone(b).m_iParentIndex!=parents[b]) return false;
	for (u_int v=0;v<xMesh.GetNumVerts();++v)
		for (u_int w=0;w<4;++w) if (xMesh.m_xBoneIndices.Get(v)[w]>=19) return false;
	const float duration=xClip.GetDuration();
	const u_int samples=static_cast<u_int>(std::ceil(duration*60));
	if (!samples) return false;
	Flux_AnimationClip candidate; candidate.GetMetadata()=xClip.GetMetadata();
	for (u_int b=0;b<19;++b)
	{
		Flux_BoneChannel channel;channel.SetBoneName(names[b]);
		const auto* source=xClip.GetBoneChannel(names[b]);
		for (u_int k=0;k<=samples;++k)
		{
			const float t=static_cast<float>(k)/samples;
			const float phase=t*6.28318530718f;
			auto rotation=glm::identity<Zenith_Maths::Quaternion>();
			if (eClip==ZM_ANIM_CLIP_FAINT)
			{
				float fall=std::clamp((t-.12f)/.76f,0.f,1.f);fall=fall*fall*(3-2*fall);
				if (b==0) rotation=glm::angleAxis(glm::radians(174.f*fall),Zenith_Maths::Vector3(0,0,1));
				else if (b==4) rotation=glm::angleAxis(glm::radians(10.f*fall),Zenith_Maths::Vector3(1,0,0));
				else if (b>=5 && b<=16)
				{
					const float side=b<11?-1.f:1.f;
					const bool lower=(b-5)%2==1;
					rotation=glm::angleAxis(glm::radians(-side*(lower?58.f:25.f)*fall),Zenith_Maths::Vector3(0,0,1));
				}
				else if (b>=17) rotation=glm::angleAxis(glm::radians((b==17?22.f:-22.f)*fall),Zenith_Maths::Vector3(0,1,0));
			}
			else if (eClip==ZM_ANIM_CLIP_WALK && b>=5 && b<=16)
			{
				const u_int leg=(b-5)/2;const float side=leg<3?-1.f:1.f;
				const float wave=std::sin(phase+(leg%2?3.14159265359f:0.f));
				const float lift=std::max(0.f,wave);
				const bool lower=(b-5)%2==1;
				rotation=glm::angleAxis(glm::radians(side*(lower?12.f:7.f)*lift),Zenith_Maths::Vector3(0,0,1)) *
					glm::angleAxis(glm::radians((lower?2.f:8.f)*wave),Zenith_Maths::Vector3(0,1,0));
			}
			else if (source)
			{
				const float strength=b<4?.15f:(b==4?.30f:1.f);
				rotation=glm::slerp(rotation,source->SampleRotation(duration*t),strength);
			}
			channel.AddRotationKeyframe(duration*t,rotation);
		}
		candidate.AddBoneChannel(names[b],std::move(channel));
	}
	xClip=std::move(candidate);
	return ZM_FitAuthoredSurfaceClearance(xRig,xMesh,xClip);
}
// The fire-line sculpt keeps the biped topology but has digitigrade feet and
// a balancing tail. Fit contact to its complete surface and author a side fall
// instead of folding the placeholder's tall spine through the abdomen.
inline bool ZM_FitEmberBipedClip(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xClip)
{
	const char* names[] = {"Spine00","Spine01","Spine02","Spine03","Head",
		"ArmLUp","ArmLLo","ArmRUp","ArmRLo","LegLUp","LegLLo","LegRUp","LegRLo","Crest"};
	const int parents[] = {-1,0,1,2,3,3,5,3,7,0,9,0,11,4};
	if (xRig.GetNumBones()!=14 || !xMesh.HasSkinning() || !xMesh.GetNumVerts()) return false;
	for (u_int b=0;b<14;++b)
		if (xRig.GetBone(b).m_strName!=names[b] || xRig.GetBone(b).m_iParentIndex!=parents[b]) return false;
	for (u_int v=0;v<xMesh.GetNumVerts();++v)
		for (u_int w=0;w<4;++w) if (xMesh.m_xBoneIndices.Get(v)[w]>=14) return false;
	if (eClip==ZM_ANIM_CLIP_FAINT)
	{
		Flux_AnimationClip candidate; candidate.GetMetadata()=xClip.GetMetadata();
		const float pitch[] = {0,5,6,6,14,22,-38,18,-32,-16,35,-12,28,0};
		const float roll[] = {84,0,0,0,-6,-12,0,10,0,-5,0,8,0,0};
		for (u_int b=0;b<14;++b)
		{
			Flux_BoneChannel channel; channel.SetBoneName(names[b]);
			for (u_int k=0;k<=72;++k)
			{
				const float t=static_cast<float>(k)/72;
				float fall=std::clamp((t-(b==0?.14f:0.f))/(b==0?.70f:.65f),0.f,1.f);
				fall=fall*fall*(3-2*fall);
				channel.AddRotationKeyframe(xClip.GetDuration()*t,
					glm::angleAxis(glm::radians(roll[b]*fall),Zenith_Maths::Vector3(0,0,1)) *
					glm::angleAxis(glm::radians(pitch[b]*fall),Zenith_Maths::Vector3(1,0,0)));
			}
			candidate.AddBoneChannel(names[b],std::move(channel));
		}
		xClip=std::move(candidate);
	}
	return ZM_FitAuthoredSurfaceClearance(xRig,xMesh,xClip);
}
// The feathered lark family uses anatomical perching legs and a long folded
// tail. Ground contact includes the complete feather surface in every pose.
inline bool ZM_FitLarkCreatureClip(const Zenith_SkeletonAsset& xRig,
	const Zenith_MeshAsset& xMesh, ZM_ANIM_CLIP eClip, Flux_AnimationClip& xClip)
{
	const char* names[] = {"Spine00","Spine01","Spine02","Head","Beak","WingL","WingR",
		"LegLUp","LegLLo","LegRUp","LegRLo","Tail00","Tail01"};
	const int parents[] = {-1,0,1,2,3,1,1,0,7,0,9,0,11};
	if (xRig.GetNumBones()!=13 || !xMesh.HasSkinning() || !xMesh.GetNumVerts()) return false;
	for (u_int b=0;b<13;++b)
		if (xRig.GetBone(b).m_strName!=names[b] || xRig.GetBone(b).m_iParentIndex!=parents[b]) return false;
	for (u_int v=0;v<xMesh.GetNumVerts();++v)
		for (u_int w=0;w<4;++w) if (xMesh.m_xBoneIndices.Get(v)[w]>=13) return false;
	if (eClip==ZM_ANIM_CLIP_FAINT)
	{
		Flux_AnimationClip candidate; candidate.GetMetadata()=xClip.GetMetadata();
		const float pitch[] = {0,5,8,16,0,0,0,-24,40,-20,35,6,4};
		const float roll[] = {82,0,0,-5,0,-14,12,0,0,0,0,0,0};
		for (u_int b=0;b<13;++b)
		{
			Flux_BoneChannel channel; channel.SetBoneName(names[b]);
			for (u_int k=0;k<=72;++k)
			{
				const float t=static_cast<float>(k)/72;
				float fall=std::clamp((t-(b==0?.12f:0.f))/(b==0?.72f:.65f),0.f,1.f);
				fall=fall*fall*(3-2*fall);
				channel.AddRotationKeyframe(xClip.GetDuration()*t,
					glm::angleAxis(glm::radians(roll[b]*fall),Zenith_Maths::Vector3(0,0,1)) *
					glm::angleAxis(glm::radians(pitch[b]*fall),Zenith_Maths::Vector3(1,0,0)));
			}
			candidate.AddBoneChannel(names[b],std::move(channel));
		}
		xClip=std::move(candidate);
	}
	return ZM_FitAuthoredSurfaceClearance(xRig,xMesh,xClip);
}
#endif
