#include "Zenith.h"

// ============================================================================
// ZM_Tests_HumanGen -- S4 SC1/SC2/SC3/SC4 unit gate for ZM_HumanGen (suite ZM_Gen).
//
// These author against the frozen public HumanGen/Data seam plus SC3's internal,
// pure ZM_HumanAppearance seam and SC4's shared in-memory clip seam. SC1 asserts
// the roster/skeleton/recipe/asset-path contract; later cases extend it without
// disk, GPU, or tools-only reach:
//   1. HumanGen_RosterTotality      -- every id yields a valid recipe + a
//                                      buildable, ZM_ValidateHuman-passing bundle.
//   2. HumanGen_SharedSkeletonWellFormed -- ZM_AppendSharedHumanBones emits the
//                                      exact 16-bone shared rig (count, single
//                                      root, parent<child, resolvable names,
//                                      identity bind-local rotation).
//   3. HumanGen_RecipePurity        -- ZM_ResolveHumanRecipe is pure; distinct
//                                      ids carry distinct synthetic seeds.
//   4. HumanGen_AssetPathScheme     -- golden per-model + shared refs + truncation.
//   5. HumanGen_ClipMetadataGolden  -- the frozen 9-clip names/durations/looping.
//   6. HumanGen_BuildDeterminism    -- reflexive lock on the byte-identity + hash
//                                      helpers (rebuild the same id twice ==; two
//                                      genuinely-different ids !=, non-degeneracy).
//   7. HumanGen_StructuralInvariants -- every SC2 mesh is fully finalised and
//                                      satisfies the loft/skin contract.
//   8. HumanGen_PerModelBonesMatchShared -- every model carries the exact shared
//                                      bone data in the exact index order.
//   9. HumanGen_SameSeedDeterminism -- every id rebuilds byte-identically.
//  10. HumanGen_Sensitivity        -- same-build humans with different mesh seeds
//                                      do not collapse to one canned geometry.
//  11. HumanGen_AppearanceAlbedoStructural -- appearance-axis/atlas coverage and
//                                      all-roster albedo material invariants.
//  12. HumanGen_AppearanceDomainIsolation -- MESH/ALBEDO sensitivity and every
//                                      other generation domain's isolation.
//  13. HumanGen_HairStyleSilhouettes -- all six hair styles are deterministic,
//                                      Head-rigid, structurally valid silhouettes.
//  14. HumanGen_AttachmentSilhouettes -- every attachment is deterministic,
//                                      structurally valid, and visibly distinct.
//  15. HumanGen_ClipChannelsMatchSharedSkeleton -- all nine clips use only the
//                                      exact shared rig names and rotation keys.
//  16. HumanGen_ClipTimingAndPlaybackPolicy -- key timing, loop seams, neutral
//                                      one-shot recovery, and Faint hold policy.
//  17. HumanGen_ClipDeterminismAndSensitivity -- byte/hash determinism plus
//                                      pairwise motion (not metadata) distinction.
//  18. HumanGen_ClipSetSharedAcrossRoster -- one model-independent clip set
//                                      validates and rebuilds identically for all.
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS reach. Runs at boot before
// the scene loads.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"
#include "Zenithmon/Source/Data/ZM_HumanData.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // strlen, strcmp
#include <cmath>     // fabsf
#include <cstdio>    // snprintf
#include <string>
#include <utility>   // pair

namespace
{
	constexpr float fHUMAN_WEIGHT_TOL = 1.0e-4f;
	constexpr float fHUMAN_INFLUENCE_EPS = 1.0e-6f;
	constexpr float fHUMAN_UNIT_TOL = 1.0e-3f;
	constexpr float fHUMAN_WORLD_BOX_LIMIT = 10.0f;
	constexpr float fHUMAN_CHANNEL_TOL = 1.0e-5f;
	constexpr float fHUMAN_ANIM_TICK_TOL = 1.0e-4f;
	constexpr float fHUMAN_ANIM_QUAT_UNIT_TOL = 1.0e-3f;
	constexpr float fHUMAN_ANIM_ROT_CLOSE_DOT = 0.9999f;
	constexpr float fHUMAN_ANIM_ROT_MOTION_DOT = 0.99999f;
	constexpr u_int uHUMAN_MIN_MATERIAL_COLOURS = 3u;
	constexpr u_int64 ulHUMAN_DOMAIN_SEED_PERTURBATION = 0xA24BAED4963EE407ULL;

	// The 16 shared-skeleton bone names, in the frozen ZM_AppendSharedHumanBones
	// emit order (index i must resolve to bone i).
	const char* g_aszSharedBones[uZM_HUMAN_BONE_COUNT] =
	{
		"Root",          "Spine",         "Neck",     "Head",
		"LeftUpperArm",  "LeftLowerArm",  "LeftHand",
		"RightUpperArm", "RightLowerArm", "RightHand",
		"LeftUpperLeg",  "LeftLowerLeg",  "LeftFoot",
		"RightUpperLeg", "RightLowerLeg", "RightFoot"
	};

	struct HumanClipGold
	{
		ZM_HUMAN_ANIM_CLIP m_eClip;
		const char* m_szName;
		float m_fDurationSeconds;
		bool m_bLooping;
	};

	const HumanClipGold g_axHumanClipGold[ZM_HUMAN_CLIP_COUNT] =
	{
		{ ZM_HUMAN_CLIP_IDLE,  "Idle",  2.0f, true  },
		{ ZM_HUMAN_CLIP_WALK,  "Walk",  1.0f, true  },
		{ ZM_HUMAN_CLIP_RUN,   "Run",   0.7f, true  },
		{ ZM_HUMAN_CLIP_TALK,  "Talk",  1.6f, true  },
		{ ZM_HUMAN_CLIP_WAVE,  "Wave",  1.0f, false },
		{ ZM_HUMAN_CLIP_POINT, "Point", 0.8f, false },
		{ ZM_HUMAN_CLIP_CHEER, "Cheer", 1.2f, false },
		{ ZM_HUMAN_CLIP_HURT,  "Hurt",  0.4f, false },
		{ ZM_HUMAN_CLIP_FAINT, "Faint", 1.2f, false },
	};

	bool HumanQuatFiniteNormalized(const Zenith_Maths::Quat& xQ)
	{
		if (!std::isfinite(xQ.w) || !std::isfinite(xQ.x)
			|| !std::isfinite(xQ.y) || !std::isfinite(xQ.z))
		{
			return false;
		}
		const float fLengthSquared = xQ.w * xQ.w + xQ.x * xQ.x
			+ xQ.y * xQ.y + xQ.z * xQ.z;
		return fabsf(fLengthSquared - 1.0f) <= fHUMAN_ANIM_QUAT_UNIT_TOL;
	}

	float HumanQuatAbsDot(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
	{
		return fabsf(xA.w * xB.w + xA.x * xB.x
			+ xA.y * xB.y + xA.z * xB.z);
	}

	bool HumanRotationKeysWellFormed(const Flux_BoneChannel& xChannel,
		float fDurationTicks, u_int& uFirstBadKey)
	{
		uFirstBadKey = 0xFFFFFFFFu;
		const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys =
			xChannel.GetRotationKeyframes();
		if (!std::isfinite(fDurationTicks) || fDurationTicks <= 0.0f || xKeys.GetSize() < 2u)
		{
			uFirstBadKey = 0u;
			return false;
		}

		float fPreviousTick = -1.0f;
		for (u_int k = 0u; k < xKeys.GetSize(); ++k)
		{
			const float fTick = xKeys.Get(k).second;
			const bool bTickValid = std::isfinite(fTick)
				&& fTick >= 0.0f
				&& fTick <= fDurationTicks
				&& (k == 0u || fTick > fPreviousTick);
			if (!bTickValid || !HumanQuatFiniteNormalized(xKeys.Get(k).first))
			{
				uFirstBadKey = k;
				return false;
			}
			fPreviousTick = fTick;
		}
		return true;
	}

	bool HumanClipHasTemporalMotion(const Flux_AnimationClip& xClip)
	{
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels =
			xClip.GetBoneChannels();
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys =
				xIt.GetValue().GetRotationKeyframes();
			if (xKeys.GetSize() < 2u || !HumanQuatFiniteNormalized(xKeys.GetFront().first))
			{
				continue;
			}
			const Zenith_Maths::Quat& xFirst = xKeys.GetFront().first;
			for (u_int k = 1u; k < xKeys.GetSize(); ++k)
			{
				if (HumanQuatFiniteNormalized(xKeys.Get(k).first)
					&& HumanQuatAbsDot(xFirst, xKeys.Get(k).first)
						< fHUMAN_ANIM_ROT_MOTION_DOT)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool HumanClipMotionEquivalentNormalized(const Flux_AnimationClip& xA,
		const Flux_AnimationClip& xB)
	{
		const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
		const float fDurationA = xA.GetDurationInTicks();
		const float fDurationB = xB.GetDurationInTicks();
		if (!std::isfinite(fDurationA) || fDurationA <= 0.0f
			|| !std::isfinite(fDurationB) || fDurationB <= 0.0f)
		{
			return true;
		}

		for (u_int b = 0u; b < uZM_HUMAN_BONE_COUNT; ++b)
		{
			const Flux_BoneChannel* pxA = xA.GetBoneChannel(g_aszSharedBones[b]);
			const Flux_BoneChannel* pxB = xB.GetBoneChannel(g_aszSharedBones[b]);
			if (pxA == nullptr && pxB == nullptr)
			{
				continue;
			}
			if (pxA == nullptr || pxB == nullptr)
			{
				const Flux_BoneChannel* pxPresent = pxA != nullptr ? pxA : pxB;
				const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xPresentKeys =
					pxPresent->GetRotationKeyframes();
				for (u_int k = 0u; k < xPresentKeys.GetSize(); ++k)
				{
					if (!HumanQuatFiniteNormalized(xPresentKeys.Get(k).first))
					{
						return true;
					}
					if (HumanQuatAbsDot(xPresentKeys.Get(k).first, xIdentity)
						< fHUMAN_ANIM_ROT_MOTION_DOT)
					{
						return false;
					}
				}
				continue;
			}

			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeysA =
				pxA->GetRotationKeyframes();
			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeysB =
				pxB->GetRotationKeyframes();
			if ((xKeysA.GetSize() == 0u) != (xKeysB.GetSize() == 0u))
			{
				const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xPresentKeys =
					xKeysA.GetSize() != 0u ? xKeysA : xKeysB;
				for (u_int k = 0u; k < xPresentKeys.GetSize(); ++k)
				{
					if (!HumanQuatFiniteNormalized(xPresentKeys.Get(k).first))
					{
						return true;
					}
					if (HumanQuatAbsDot(xPresentKeys.Get(k).first, xIdentity)
						< fHUMAN_ANIM_ROT_MOTION_DOT)
					{
						return false;
					}
				}
				continue;
			}
			if (xKeysA.GetSize() == 0u)
			{
				continue;
			}

			for (u_int k = 0u; k < xKeysA.GetSize(); ++k)
			{
				const float fTick = xKeysA.Get(k).second;
				if (!std::isfinite(fTick) || fTick < 0.0f || fTick > fDurationA)
				{
					return true;
				}
				const Zenith_Maths::Quat xOther = pxB->SampleRotation(
					(fTick / fDurationA) * fDurationB);
				if (HumanQuatFiniteNormalized(xKeysA.Get(k).first)
					&& HumanQuatFiniteNormalized(xOther)
					&& HumanQuatAbsDot(xKeysA.Get(k).first, xOther)
						< fHUMAN_ANIM_ROT_MOTION_DOT)
				{
					return false;
				}
			}
			for (u_int k = 0u; k < xKeysB.GetSize(); ++k)
			{
				const float fTick = xKeysB.Get(k).second;
				if (!std::isfinite(fTick) || fTick < 0.0f || fTick > fDurationB)
				{
					return true;
				}
				const Zenith_Maths::Quat xOther = pxA->SampleRotation(
					(fTick / fDurationB) * fDurationA);
				if (HumanQuatFiniteNormalized(xKeysB.Get(k).first)
					&& HumanQuatFiniteNormalized(xOther)
					&& HumanQuatAbsDot(xKeysB.Get(k).first, xOther)
						< fHUMAN_ANIM_ROT_MOTION_DOT)
				{
					return false;
				}
			}
		}
		return true;
	}

	template <u_int uCount>
	bool HumanClipHasAllChannels(const Flux_AnimationClip& xClip,
		const char* const (&aszRequired)[uCount], const char*& szMissing)
	{
		szMissing = nullptr;
		for (u_int i = 0u; i < uCount; ++i)
		{
			if (!xClip.HasBoneChannel(aszRequired[i]))
			{
				szMissing = aszRequired[i];
				return false;
			}
		}
		return true;
	}

	bool HumanClipHasCompleteArmChain(const Flux_AnimationClip& xClip, bool bLeft)
	{
		const char* szUpper = bLeft ? "LeftUpperArm" : "RightUpperArm";
		const char* szLower = bLeft ? "LeftLowerArm" : "RightLowerArm";
		const char* szHand = bLeft ? "LeftHand" : "RightHand";
		return xClip.HasBoneChannel(szUpper)
			&& xClip.HasBoneChannel(szLower)
			&& xClip.HasBoneChannel(szHand);
	}

	bool HumanClipHasSemanticChannels(ZM_HUMAN_ANIM_CLIP eClip,
		const Flux_AnimationClip& xClip, const char*& szMissing)
	{
		switch (eClip)
		{
		case ZM_HUMAN_CLIP_IDLE:
		{
			const char* const aszRequired[] = { "Spine", "Neck", "Head" };
			return HumanClipHasAllChannels(xClip, aszRequired, szMissing);
		}
		case ZM_HUMAN_CLIP_WALK:
		case ZM_HUMAN_CLIP_RUN:
		{
			const char* const aszRequired[] =
			{
				"Spine", "Head", "LeftUpperArm", "RightUpperArm",
				"LeftUpperLeg", "LeftLowerLeg", "LeftFoot",
				"RightUpperLeg", "RightLowerLeg", "RightFoot"
			};
			return HumanClipHasAllChannels(xClip, aszRequired, szMissing);
		}
		case ZM_HUMAN_CLIP_TALK:
		{
			const char* const aszRequired[] = { "Spine", "Neck", "Head" };
			if (!HumanClipHasAllChannels(xClip, aszRequired, szMissing))
			{
				return false;
			}
			if (!HumanClipHasCompleteArmChain(xClip, true)
				&& !HumanClipHasCompleteArmChain(xClip, false))
			{
				szMissing = "either complete arm chain";
				return false;
			}
			return true;
		}
		case ZM_HUMAN_CLIP_WAVE:
		case ZM_HUMAN_CLIP_POINT:
			if (!HumanClipHasCompleteArmChain(xClip, true)
				&& !HumanClipHasCompleteArmChain(xClip, false))
			{
				szMissing = "either complete arm chain";
				return false;
			}
			return true;
		case ZM_HUMAN_CLIP_CHEER:
		{
			const char* const aszRequired[] =
				{ "Spine", "LeftUpperArm", "LeftLowerArm", "RightUpperArm", "RightLowerArm" };
			return HumanClipHasAllChannels(xClip, aszRequired, szMissing);
		}
		case ZM_HUMAN_CLIP_HURT:
		{
			const char* const aszRequired[] =
				{ "Spine", "Neck", "Head", "LeftUpperArm", "RightUpperArm" };
			return HumanClipHasAllChannels(xClip, aszRequired, szMissing);
		}
		case ZM_HUMAN_CLIP_FAINT:
		{
			const char* const aszRequired[] =
			{
				"Spine", "Neck", "Head", "LeftUpperLeg", "LeftLowerLeg",
				"RightUpperLeg", "RightLowerLeg"
			};
			return HumanClipHasAllChannels(xClip, aszRequired, szMissing);
		}
		default:
			szMissing = "known clip semantic contract";
			return false;
		}
	}

	// A quaternion is the identity bind-local rotation (w=1, x=y=z=0) within tol.
	bool QuatIsIdentity(const Zenith_Maths::Quat& xQ)
	{
		constexpr float fEps = 1.0e-5f;
		return fabsf(xQ.w - 1.0f) < fEps
			&& fabsf(xQ.x) < fEps
			&& fabsf(xQ.y) < fEps
			&& fabsf(xQ.z) < fEps;
	}

	bool IsFiniteVec2(const Zenith_Maths::Vector2& xV)
	{
		return std::isfinite(xV.x) && std::isfinite(xV.y);
	}

	bool IsFiniteVec3(const Zenith_Maths::Vector3& xV)
	{
		return std::isfinite(xV.x) && std::isfinite(xV.y) && std::isfinite(xV.z);
	}

	bool IsFiniteVec4(const Zenith_Maths::Vector4& xV)
	{
		return std::isfinite(xV.x) && std::isfinite(xV.y)
			&& std::isfinite(xV.z) && std::isfinite(xV.w);
	}

	bool IsUnitRange(float fValue)
	{
		return std::isfinite(fValue)
			&& fValue >= -fHUMAN_CHANNEL_TOL
			&& fValue <= 1.0f + fHUMAN_CHANNEL_TOL;
	}

	// Full field-wise equality for one shared bone. Avoid memcmp: ZM_GenBone is
	// POD-like, but this test should not depend on compiler padding bytes.
	bool BoneFieldsEqual(const ZM_GenBone& xA, const ZM_GenBone& xB)
	{
		return strcmp(xA.m_szName, xB.m_szName) == 0
			&& xA.m_iParent == xB.m_iParent
			&& xA.m_xLocalPos.x == xB.m_xLocalPos.x
			&& xA.m_xLocalPos.y == xB.m_xLocalPos.y
			&& xA.m_xLocalPos.z == xB.m_xLocalPos.z
			&& xA.m_xLocalRot.w == xB.m_xLocalRot.w
			&& xA.m_xLocalRot.x == xB.m_xLocalRot.x
			&& xA.m_xLocalRot.y == xB.m_xLocalRot.y
			&& xA.m_xLocalRot.z == xB.m_xLocalRot.z
			&& xA.m_xLocalScale.x == xB.m_xLocalScale.x
			&& xA.m_xLocalScale.y == xB.m_xLocalScale.y
			&& xA.m_xLocalScale.z == xB.m_xLocalScale.z;
	}

	bool MeshBonesEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB)
	{
		if (xA.GetNumBones() != xB.GetNumBones()) { return false; }
		for (u_int b = 0u; b < xA.GetNumBones(); ++b)
		{
			if (!BoneFieldsEqual(xA.m_xBones.Get(b), xB.m_xBones.Get(b))) { return false; }
		}
		return true;
	}

	bool BoundsFiniteAndSane(const ZM_GenMesh& xMesh)
	{
		if (xMesh.GetNumVerts() == 0u) { return false; }
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
		if (!IsFiniteVec3(xMin) || !IsFiniteVec3(xMax)) { return false; }
		if (!(xMin.x < xMax.x) || !(xMin.y < xMax.y) || !(xMin.z < xMax.z)) { return false; }
		return fabsf(xMin.x) <= fHUMAN_WORLD_BOX_LIMIT
			&& fabsf(xMin.y) <= fHUMAN_WORLD_BOX_LIMIT
			&& fabsf(xMin.z) <= fHUMAN_WORLD_BOX_LIMIT
			&& fabsf(xMax.x) <= fHUMAN_WORLD_BOX_LIMIT
			&& fabsf(xMax.y) <= fHUMAN_WORLD_BOX_LIMIT
			&& fabsf(xMax.z) <= fHUMAN_WORLD_BOX_LIMIT;
	}

	bool AtlasIslandValid(const ZM_GenUVIsland& xIsland)
	{
		return std::isfinite(xIsland.m_fU0) && std::isfinite(xIsland.m_fV0)
			&& std::isfinite(xIsland.m_fU1) && std::isfinite(xIsland.m_fV1)
			&& xIsland.m_fU0 >= 0.0f && xIsland.m_fV0 >= 0.0f
			&& xIsland.m_fU1 <= 1.0f && xIsland.m_fV1 <= 1.0f
			&& xIsland.m_fU0 < xIsland.m_fU1 && xIsland.m_fV0 < xIsland.m_fV1;
	}

	bool UVInsideIsland(const Zenith_Maths::Vector2& xUV, const ZM_GenUVIsland& xIsland)
	{
		return IsFiniteVec2(xUV)
			&& xUV.x >= xIsland.m_fU0 - fHUMAN_CHANNEL_TOL
			&& xUV.x <= xIsland.m_fU1 + fHUMAN_CHANNEL_TOL
			&& xUV.y >= xIsland.m_fV0 - fHUMAN_CHANNEL_TOL
			&& xUV.y <= xIsland.m_fV1 + fHUMAN_CHANNEL_TOL;
	}

	struct HumanImageScan
	{
		bool  m_bPackedSizeMatches = false;
		bool  m_bFinite = true;
		bool  m_bUnitRange = true;
		bool  m_bOpaque = true;
		u_int m_uDistinctRGB = 0u;   // capped at uHUMAN_MIN_MATERIAL_COLOURS
		u_int m_uFirstBadX = 0xFFFFFFFFu;
		u_int m_uFirstBadY = 0xFFFFFFFFu;
	};

	HumanImageScan ScanHumanImage(const ZM_GenImage& xImage)
	{
		HumanImageScan xScan;
		Zenith_Vector<u_int8> xPacked;
		xImage.PackRGBA8(xPacked, false);
		const u_int uExpectedBytes = xImage.GetWidth() * xImage.GetHeight() * 4u;
		xScan.m_bPackedSizeMatches = xPacked.GetSize() == uExpectedBytes;

		u_int auDistinct[uHUMAN_MIN_MATERIAL_COLOURS] = {};
		for (u_int y = 0u; y < xImage.GetHeight(); ++y)
		{
			for (u_int x = 0u; x < xImage.GetWidth(); ++x)
			{
				const Zenith_Maths::Vector4 xRGBA = xImage.Get(y, x);
				const bool bFinite = IsFiniteVec4(xRGBA);
				const bool bUnit = IsUnitRange(xRGBA.x) && IsUnitRange(xRGBA.y)
					&& IsUnitRange(xRGBA.z) && IsUnitRange(xRGBA.w);
				const bool bOpaque = std::isfinite(xRGBA.w)
					&& fabsf(xRGBA.w - 1.0f) <= fHUMAN_CHANNEL_TOL;
				xScan.m_bFinite &= bFinite;
				xScan.m_bUnitRange &= bUnit;
				xScan.m_bOpaque &= bOpaque;
				if ((!bFinite || !bUnit || !bOpaque) && xScan.m_uFirstBadX == 0xFFFFFFFFu)
				{
					xScan.m_uFirstBadX = x;
					xScan.m_uFirstBadY = y;
				}

				if (xScan.m_bPackedSizeMatches && xScan.m_uDistinctRGB < uHUMAN_MIN_MATERIAL_COLOURS)
				{
					const u_int uByte = (y * xImage.GetWidth() + x) * 4u;
					const u_int uRGB = (u_int)xPacked.Get(uByte)
						| ((u_int)xPacked.Get(uByte + 1u) << 8u)
						| ((u_int)xPacked.Get(uByte + 2u) << 16u);
					bool bSeen = false;
					for (u_int c = 0u; c < xScan.m_uDistinctRGB; ++c)
					{
						if (auDistinct[c] == uRGB) { bSeen = true; break; }
					}
					if (!bSeen)
					{
						auDistinct[xScan.m_uDistinctRGB] = uRGB;
						++xScan.m_uDistinctRGB;
					}
				}
			}
		}
		return xScan;
	}

	bool MeshSafeForFinalization(const ZM_GenMesh& xMesh, u_int& uFirstBadIndex)
	{
		uFirstBadIndex = 0xFFFFFFFFu;
		const u_int uNumVerts = xMesh.GetNumVerts();
		const u_int uNumIndices = xMesh.m_xIndices.GetSize();
		if (uNumVerts == 0u || uNumIndices < 3u || (uNumIndices % 3u) != 0u) { return false; }
		if (xMesh.m_xNormals.GetSize() != uNumVerts
			|| xMesh.m_xUVs.GetSize() != uNumVerts
			|| xMesh.m_xColors.GetSize() != uNumVerts
			|| xMesh.m_xBoneIndices.GetSize() != uNumVerts
			|| xMesh.m_xBoneWeights.GetSize() != uNumVerts)
		{
			return false;
		}
		for (u_int i = 0u; i < uNumIndices; ++i)
		{
			if (xMesh.m_xIndices.Get(i) >= uNumVerts)
			{
				uFirstBadIndex = i;
				return false;
			}
		}
		return true;
	}

	bool MeshSafeForValidation(const ZM_GenMesh& xMesh, u_int& uFirstBadIndex)
	{
		uFirstBadIndex = 0xFFFFFFFFu;
		const u_int uNumVerts = xMesh.GetNumVerts();
		const u_int uNumIndices = xMesh.m_xIndices.GetSize();
		if (uNumVerts == 0u || uNumIndices < 3u || (uNumIndices % 3u) != 0u) { return false; }
		if (xMesh.m_xNormals.GetSize() != uNumVerts
			|| xMesh.m_xUVs.GetSize() != uNumVerts
			|| xMesh.m_xTangents.GetSize() != uNumVerts
			|| xMesh.m_xColors.GetSize() != uNumVerts
			|| xMesh.m_xBoneIndices.GetSize() != uNumVerts
			|| xMesh.m_xBoneWeights.GetSize() != uNumVerts)
		{
			return false;
		}
		for (u_int i = 0u; i < uNumIndices; ++i)
		{
			if (xMesh.m_xIndices.Get(i) >= uNumVerts)
			{
				uFirstBadIndex = i;
				return false;
			}
		}
		return true;
	}

	bool VertexRangeValid(const ZM_GenMesh& xMesh, u_int uFirstVert,
		u_int uEndVert, int iRequiredRigidBone, const ZM_GenUVIsland* pxRequiredIsland,
		u_int& uFirstBadVertex)
	{
		uFirstBadVertex = 0xFFFFFFFFu;
		const u_int uNumVerts = xMesh.GetNumVerts();
		if (uFirstVert >= uEndVert || uEndVert > uNumVerts
			|| xMesh.m_xNormals.GetSize() != uNumVerts
			|| xMesh.m_xUVs.GetSize() != uNumVerts
			|| xMesh.m_xTangents.GetSize() != uNumVerts
			|| xMesh.m_xColors.GetSize() != uNumVerts
			|| xMesh.m_xBoneIndices.GetSize() != uNumVerts
			|| xMesh.m_xBoneWeights.GetSize() != uNumVerts)
		{
			return false;
		}

		for (u_int v = uFirstVert; v < uEndVert; ++v)
		{
			const Zenith_Maths::Vector3& xPos = xMesh.m_xPositions.Get(v);
			const Zenith_Maths::Vector3& xNormal = xMesh.m_xNormals.Get(v);
			const Zenith_Maths::Vector2& xUV = xMesh.m_xUVs.Get(v);
			const Zenith_Maths::Vector3& xTangent = xMesh.m_xTangents.Get(v);
			const Zenith_Maths::Vector4& xColour = xMesh.m_xColors.Get(v);
			const glm::uvec4& xIndices = xMesh.m_xBoneIndices.Get(v);
			const glm::vec4& xWeights = xMesh.m_xBoneWeights.Get(v);

			bool bValid = IsFiniteVec3(xPos) && IsFiniteVec3(xNormal)
				&& IsFiniteVec2(xUV) && IsFiniteVec3(xTangent)
				&& IsFiniteVec4(xColour) && IsFiniteVec4(xWeights)
				&& IsUnitRange(xUV.x) && IsUnitRange(xUV.y)
				&& IsUnitRange(xColour.x) && IsUnitRange(xColour.y)
				&& IsUnitRange(xColour.z) && IsUnitRange(xColour.w)
				&& fabsf(glm::length(xNormal) - 1.0f) <= fHUMAN_UNIT_TOL
				&& fabsf(glm::length(xTangent) - 1.0f) <= fHUMAN_UNIT_TOL
				&& fabsf(glm::dot(xNormal, xTangent)) <= fHUMAN_UNIT_TOL;
			if (pxRequiredIsland != nullptr) { bValid &= UVInsideIsland(xUV, *pxRequiredIsland); }

			u_int uActive = 0u;
			float fWeightSum = 0.0f;
			bool bRigidBoneMatched = iRequiredRigidBone < 0;
			for (u_int c = 0u; c < 4u; ++c)
			{
				bValid &= xIndices[c] < uZM_HUMAN_BONE_COUNT;
				bValid &= xWeights[c] >= 0.0f && xWeights[c] <= 1.0f;
				fWeightSum += xWeights[c];
				if (xWeights[c] > fHUMAN_INFLUENCE_EPS)
				{
					++uActive;
					if (iRequiredRigidBone >= 0)
					{
						bRigidBoneMatched = xIndices[c] == (u_int)iRequiredRigidBone;
					}
				}
			}
			bValid &= fabsf(fWeightSum - 1.0f) <= fHUMAN_WEIGHT_TOL;
			bValid &= uActive >= 1u && uActive <= 2u;
			if (iRequiredRigidBone >= 0) { bValid &= uActive == 1u && bRigidBoneMatched; }

			if (!bValid)
			{
				uFirstBadVertex = v;
				return false;
			}
		}
		return true;
	}

	bool MeshHasCompleteVertexBuffers(const ZM_GenMesh& xMesh)
	{
		const u_int uNumVerts = xMesh.GetNumVerts();
		return xMesh.m_xNormals.GetSize() == uNumVerts
			&& xMesh.m_xUVs.GetSize() == uNumVerts
			&& xMesh.m_xTangents.GetSize() == uNumVerts
			&& xMesh.m_xColors.GetSize() == uNumVerts
			&& xMesh.m_xBoneIndices.GetSize() == uNumVerts
			&& xMesh.m_xBoneWeights.GetSize() == uNumVerts;
	}

	bool VertexFieldsEqual(const ZM_GenMesh& xA, u_int uA, const ZM_GenMesh& xB, u_int uB)
	{
		const Zenith_Maths::Vector3& xPosA = xA.m_xPositions.Get(uA);
		const Zenith_Maths::Vector3& xPosB = xB.m_xPositions.Get(uB);
		const Zenith_Maths::Vector3& xNormalA = xA.m_xNormals.Get(uA);
		const Zenith_Maths::Vector3& xNormalB = xB.m_xNormals.Get(uB);
		const Zenith_Maths::Vector2& xUVA = xA.m_xUVs.Get(uA);
		const Zenith_Maths::Vector2& xUVB = xB.m_xUVs.Get(uB);
		const Zenith_Maths::Vector3& xTangentA = xA.m_xTangents.Get(uA);
		const Zenith_Maths::Vector3& xTangentB = xB.m_xTangents.Get(uB);
		const Zenith_Maths::Vector4& xColourA = xA.m_xColors.Get(uA);
		const Zenith_Maths::Vector4& xColourB = xB.m_xColors.Get(uB);
		const glm::uvec4& xIndicesA = xA.m_xBoneIndices.Get(uA);
		const glm::uvec4& xIndicesB = xB.m_xBoneIndices.Get(uB);
		const glm::vec4& xWeightsA = xA.m_xBoneWeights.Get(uA);
		const glm::vec4& xWeightsB = xB.m_xBoneWeights.Get(uB);
		return xPosA.x == xPosB.x && xPosA.y == xPosB.y && xPosA.z == xPosB.z
			&& xNormalA.x == xNormalB.x && xNormalA.y == xNormalB.y && xNormalA.z == xNormalB.z
			&& xUVA.x == xUVB.x && xUVA.y == xUVB.y
			&& xTangentA.x == xTangentB.x && xTangentA.y == xTangentB.y && xTangentA.z == xTangentB.z
			&& xColourA.x == xColourB.x && xColourA.y == xColourB.y
			&& xColourA.z == xColourB.z && xColourA.w == xColourB.w
			&& xIndicesA.x == xIndicesB.x && xIndicesA.y == xIndicesB.y
			&& xIndicesA.z == xIndicesB.z && xIndicesA.w == xIndicesB.w
			&& xWeightsA.x == xWeightsB.x && xWeightsA.y == xWeightsB.y
			&& xWeightsA.z == xWeightsB.z && xWeightsA.w == xWeightsB.w;
	}

	bool VertexRangesExact(const ZM_GenMesh& xA, u_int uFirstA,
		const ZM_GenMesh& xB, u_int uFirstB, u_int uCount, u_int& uFirstBadOffset)
	{
		uFirstBadOffset = 0xFFFFFFFFu;
		if (!MeshHasCompleteVertexBuffers(xA) || !MeshHasCompleteVertexBuffers(xB)
			|| uFirstA > xA.GetNumVerts() || uCount > xA.GetNumVerts() - uFirstA
			|| uFirstB > xB.GetNumVerts() || uCount > xB.GetNumVerts() - uFirstB)
		{
			return false;
		}
		for (u_int v = 0u; v < uCount; ++v)
		{
			if (!VertexFieldsEqual(xA, uFirstA + v, xB, uFirstB + v))
			{
				uFirstBadOffset = v;
				return false;
			}
		}
		return true;
	}

	bool IndexRangesExactShifted(const ZM_GenMesh& xA, u_int uFirstA,
		const ZM_GenMesh& xB, u_int uFirstB, u_int uCount, u_int uVertexOffset,
		u_int& uFirstBadOffset)
	{
		uFirstBadOffset = 0xFFFFFFFFu;
		const u_int uSizeA = xA.m_xIndices.GetSize();
		const u_int uSizeB = xB.m_xIndices.GetSize();
		if (uFirstA > uSizeA || uCount > uSizeA - uFirstA
			|| uFirstB > uSizeB || uCount > uSizeB - uFirstB)
		{
			return false;
		}
		for (u_int i = 0u; i < uCount; ++i)
		{
			const u_int64 ulExpected = (u_int64)xB.m_xIndices.Get(uFirstB + i)
				+ (u_int64)uVertexOffset;
			if (ulExpected > 0xFFFFFFFFULL
				|| (u_int64)xA.m_xIndices.Get(uFirstA + i) != ulExpected)
			{
				uFirstBadOffset = i;
				return false;
			}
		}
		return true;
	}

	bool ImagesEqualInIslandCore(const ZM_GenImage& xA, const ZM_GenImage& xB,
		const ZM_GenUVIsland& xIsland, u_int& uPixelsCompared,
		u_int& uFirstDiffX, u_int& uFirstDiffY)
	{
		uPixelsCompared = 0u;
		uFirstDiffX = uFirstDiffY = 0xFFFFFFFFu;
		if (xA.IsEmpty() || xA.GetWidth() != xB.GetWidth() || xA.GetHeight() != xB.GetHeight()
			|| !AtlasIslandValid(xIsland))
		{
			return false;
		}

		Zenith_Vector<u_int8> xPackedA;
		Zenith_Vector<u_int8> xPackedB;
		xA.PackRGBA8(xPackedA, false);
		xB.PackRGBA8(xPackedB, false);
		const u_int uWidth = xA.GetWidth();
		const u_int uHeight = xA.GetHeight();
		const u_int uExpectedBytes = uWidth * uHeight * 4u;
		if (xPackedA.GetSize() != uExpectedBytes || xPackedB.GetSize() != uExpectedBytes)
		{
			return false;
		}

		// Exclude the one-texel clamp-to-edge dilation declared by the internal
		// appearance seam; compare only the semantic painted core.
		const float fInsetU = 1.5f / (float)uWidth;
		const float fInsetV = 1.5f / (float)uHeight;
		const float fCoreU0 = xIsland.m_fU0 + fInsetU;
		const float fCoreU1 = xIsland.m_fU1 - fInsetU;
		const float fCoreV0 = xIsland.m_fV0 + fInsetV;
		const float fCoreV1 = xIsland.m_fV1 - fInsetV;
		if (!(fCoreU0 < fCoreU1) || !(fCoreV0 < fCoreV1)) { return false; }

		bool bEqual = true;
		for (u_int y = 0u; y < uHeight; ++y)
		{
			const float fV = ((float)y + 0.5f) / (float)uHeight;
			if (fV <= fCoreV0 || fV >= fCoreV1) { continue; }
			for (u_int x = 0u; x < uWidth; ++x)
			{
				const float fU = ((float)x + 0.5f) / (float)uWidth;
				if (fU <= fCoreU0 || fU >= fCoreU1) { continue; }
				++uPixelsCompared;
				const u_int uByte = (y * uWidth + x) * 4u;
				bool bTexelEqual = true;
				for (u_int c = 0u; c < 4u; ++c)
				{
					if (xPackedA.Get(uByte + c) != xPackedB.Get(uByte + c))
					{
						bTexelEqual = false;
						break;
					}
				}
				if (!bTexelEqual)
				{
					bEqual = false;
					if (uFirstDiffX == 0xFFFFFFFFu)
					{
						uFirstDiffX = x;
						uFirstDiffY = y;
					}
				}
			}
		}
		return bEqual;
	}

	void BuildHumanFromRecipe(const ZM_HumanRecipe& xRecipe, ZM_Human& xHuman)
	{
		xHuman.m_eId = xRecipe.m_eId;
		ZM_BuildHumanMesh(xRecipe, xHuman.m_xMesh);
		xHuman.m_xAlbedo = ZM_BuildHumanAlbedo(xRecipe);
	}

	bool ImagesPairwiseDistinct(const ZM_GenImage* pxImages, u_int uCount,
		u_int& uCollisionA, u_int& uCollisionB)
	{
		uCollisionA = uCollisionB = 0xFFFFFFFFu;
		for (u_int a = 0u; a < uCount; ++a)
		{
			for (u_int b = a + 1u; b < uCount; ++b)
			{
				if (pxImages[a].Equals(pxImages[b])
					|| pxImages[a].ContentHash() == pxImages[b].ContentHash())
				{
					uCollisionA = a;
					uCollisionB = b;
					return false;
				}
			}
		}
		return true;
	}

	bool MeshesPairwiseDistinct(const ZM_GenMesh* pxMeshes, u_int uCount,
		u_int& uCollisionA, u_int& uCollisionB)
	{
		uCollisionA = uCollisionB = 0xFFFFFFFFu;
		for (u_int a = 0u; a < uCount; ++a)
		{
			for (u_int b = a + 1u; b < uCount; ++b)
			{
				if (ZM_HumanMeshEqual(pxMeshes[a], pxMeshes[b]))
				{
					uCollisionA = a;
					uCollisionB = b;
					return false;
				}
			}
		}
		return true;
	}

	// Field-wise recipe equality (no memcmp -- avoids padding sensitivity). Compares
	// the id, the synthetic seed, every derived domain seed, and every variety axis.
	bool RecipeEqual(const ZM_HumanRecipe& xA, const ZM_HumanRecipe& xB)
	{
		if (xA.m_eId != xB.m_eId) { return false; }
		if (xA.m_uSyntheticSeed != xB.m_uSyntheticSeed) { return false; }
		for (u_int d = 0; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
		{
			if (xA.m_aulDomainSeed[d] != xB.m_aulDomainSeed[d]) { return false; }
		}
		if (xA.m_eBuild != xB.m_eBuild) { return false; }
		if (xA.m_fHeightScale != xB.m_fHeightScale) { return false; }
		if (xA.m_eSkinTone != xB.m_eSkinTone) { return false; }
		if (xA.m_uHairStyle != xB.m_uHairStyle) { return false; }
		if (xA.m_eHairColour != xB.m_eHairColour) { return false; }
		if (xA.m_eOutfit != xB.m_eOutfit) { return false; }
		if (xA.m_eAttachment != xB.m_eAttachment) { return false; }
		return true;
	}
}

// ############################################################################
// 1. Roster totality -- every human is resolvable + buildable + valid
// ############################################################################

// For EVERY ZM_HUMAN_ID: the roster row self-references (m_eId == index), the
// recipe resolves, and ZM_BuildHuman produces a bundle that passes the whole
// ZM_ValidateHuman contract -- a non-empty single-shared-skeleton mesh (exactly
// 16 bones) with outward winding, in-cap <=2-bone skin summing to 1, and a
// non-empty placeholder albedo.
ZENITH_TEST(ZM_Gen, HumanGen_RosterTotality)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HUMAN_ID eId = (ZM_HUMAN_ID)id;

		// Roster-table integrity: the row indexes itself.
		ZENITH_ASSERT_EQ((u_int)ZM_GetHumanData(eId).m_eId, id,
			"human row %u does not self-reference (m_eId mismatch)", id);

		// Recipe resolves and carries this id.
		const ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe(eId);
		ZENITH_ASSERT_EQ((u_int)xRecipe.m_eId, id, "recipe %u carries the wrong id", id);

		// Full bundle build + validation.
		ZM_Human xHuman;
		ZM_BuildHuman(eId, xHuman);

		ZENITH_ASSERT_GT(xHuman.m_xMesh.GetNumVerts(), 0u, "human %u mesh empty", id);
		ZENITH_ASSERT_GT(xHuman.m_xMesh.GetNumTris(), 0u, "human %u has no triangles", id);
		ZENITH_ASSERT_EQ(xHuman.m_xMesh.GetNumBones(), uZM_HUMAN_BONE_COUNT,
			"human %u does not carry the shared 16-bone skeleton", id);

		const ZM_HumanValidation xVal = ZM_ValidateHuman(xHuman);
		ZENITH_ASSERT_TRUE(xVal.m_bAllValid, "human %u failed the ZM_ValidateHuman rollup", id);
		ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward,
			"human %u winding not outward (bad tri %u)", id, xVal.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne,
			"human %u weights do not sum to 1 (bad vert %u)", id, xVal.m_uFirstBadVertex);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo, "human %u has >2 influences", id);
		ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap, "human %u exceeds the shared bone cap", id);
		ZENITH_ASSERT_TRUE(xVal.m_bBoneCountMatchesShared, "human %u bone count != 16", id);
		ZENITH_ASSERT_TRUE(xVal.m_bHasSingleRoot, "human %u skeleton is not single-rooted", id);
		ZENITH_ASSERT_TRUE(xVal.m_bParentsBeforeChildren, "human %u violates parent-before-child", id);
		ZENITH_ASSERT_TRUE(xVal.m_bAlbedoNonEmpty, "human %u placeholder albedo is empty", id);

		++uTested;
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no humans exercised the roster-totality gate");
}

// ############################################################################
// 2. Shared skeleton is well-formed (the frozen 16-bone rig)
// ############################################################################

// ZM_AppendSharedHumanBones emits EXACTLY the 16-bone shared rig: right count,
// exactly one root (parent -1), every parent strictly precedes its child, each
// expected name resolves to its emit index, and EVERY bone carries the identity
// bind-local rotation (mandatory -- the rotation-only shared clips are absolute-
// local, so a non-identity bind rotation would pose every model wrong).
ZENITH_TEST(ZM_Gen, HumanGen_SharedSkeletonWellFormed)
{
	ZM_GenMesh xMesh;
	ZM_AppendSharedHumanBones(xMesh);

	ZENITH_ASSERT_EQ(xMesh.GetNumBones(), uZM_HUMAN_BONE_COUNT,
		"shared skeleton must emit exactly 16 bones");

	u_int uRootCount = 0u;
	for (u_int u = 0; u < xMesh.GetNumBones(); ++u)
	{
		const ZM_GenBone& xBone = xMesh.m_xBones.Get(u);

		// Single root + parent-before-child.
		if (xBone.m_iParent == -1)
		{
			++uRootCount;
		}
		else
		{
			ZENITH_ASSERT_LT(xBone.m_iParent, (int)u,
				"bone %u parent %d does not precede it (parent-before-child)", u, xBone.m_iParent);
		}

		// Identity bind-local rotation on EVERY bone.
		ZENITH_ASSERT_TRUE(QuatIsIdentity(xBone.m_xLocalRot),
			"bone %u (%s) bind-local rotation is not identity", u, xBone.m_szName);
	}
	ZENITH_ASSERT_EQ(uRootCount, 1u, "shared skeleton must have exactly one root");

	// Every expected name resolves to its emit index (the index-keyed skin + name-
	// keyed clip-transfer contract).
	for (u_int b = 0; b < uZM_HUMAN_BONE_COUNT; ++b)
	{
		const int iFound = ZM_GenMeshFindBone(xMesh, g_aszSharedBones[b]);
		ZENITH_ASSERT_EQ(iFound, (int)b,
			"shared bone name '%s' must resolve to index %u", g_aszSharedBones[b], b);
	}
}

// ############################################################################
// 3. Recipe purity + distinct synthetic seeds
// ############################################################################

// ZM_ResolveHumanRecipe is a pure function of the id (resolving twice yields a
// field-identical recipe), and distinct ids carry distinct synthetic seeds (so
// the name-hashed family seed never collides across the roster).
ZENITH_TEST(ZM_Gen, HumanGen_RecipePurity)
{
	Zenith_Vector<u_int> xSeeds;
	for (u_int id = 0; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HUMAN_ID eId = (ZM_HUMAN_ID)id;
		const ZM_HumanRecipe xA = ZM_ResolveHumanRecipe(eId);
		const ZM_HumanRecipe xB = ZM_ResolveHumanRecipe(eId);
		ZENITH_ASSERT_TRUE(RecipeEqual(xA, xB), "recipe not pure for human %u", id);
		xSeeds.PushBack(xA.m_uSyntheticSeed);
	}

	// Pairwise-distinct synthetic seeds across the whole roster.
	for (u_int i = 0; i < xSeeds.GetSize(); ++i)
	{
		for (u_int j = i + 1u; j < xSeeds.GetSize(); ++j)
		{
			ZENITH_ASSERT_NE(xSeeds.Get(i), xSeeds.Get(j),
				"humans %u/%u share a synthetic seed (name-hash collision)", i, j);
		}
	}
}

// ############################################################################
// 4. Asset-path scheme (golden per-model + shared refs + truncation)
// ############################################################################

// Golden-locks the EXACT shared-rig refs, the four per-model refs for a known id
// (PlayerM), and the too-small-buffer -> false (truncation) contract. Pure;
// compiled in ALL configs.
ZENITH_TEST(ZM_Gen, HumanGen_AssetPathScheme)
{
	char acRef[256];

	// --- Shared rig + clip refs ---
	ZENITH_ASSERT_TRUE(
		ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_SKELETON, acRef, sizeof(acRef)),
		"shared skeleton ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/Shared/Human.zskel", "shared skeleton ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE, acRef, sizeof(acRef)),
		"shared idle-clip ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/Shared/Human_Idle.zanim", "shared idle-clip ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_ANIM_FAINT, acRef, sizeof(acRef)),
		"shared faint-clip ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/Shared/Human_Faint.zanim", "shared faint-clip ref scheme drifted");

	// --- Per-model refs for a known id (PlayerM) ---
	ZENITH_ASSERT_TRUE(
		ZM_HumanAssetPath(ZM_HUMAN_PLAYER_M, ZM_HUMAN_ASSET_MESH, acRef, sizeof(acRef)),
		"mesh ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/PlayerM/PlayerM.zmesh", "mesh ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_HumanAssetPath(ZM_HUMAN_PLAYER_M, ZM_HUMAN_ASSET_ALBEDO, acRef, sizeof(acRef)),
		"albedo ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/PlayerM/PlayerM_albedo.ztxtr", "albedo ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_HumanAssetPath(ZM_HUMAN_PLAYER_M, ZM_HUMAN_ASSET_MATERIAL, acRef, sizeof(acRef)),
		"material ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/PlayerM/PlayerM.zmtrl", "material ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_HumanAssetPath(ZM_HUMAN_PLAYER_M, ZM_HUMAN_ASSET_MODEL, acRef, sizeof(acRef)),
		"model ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Humans/PlayerM/PlayerM.zmodel", "model ref scheme drifted");

	// --- Truncation: a cap far too small returns false + stays NUL-terminated ---
	char acTiny[8];
	const bool bFits = ZM_HumanAssetPath(ZM_HUMAN_PLAYER_M, ZM_HUMAN_ASSET_MESH, acTiny, sizeof(acTiny));
	ZENITH_ASSERT_FALSE(bFits, "an 8-byte cap cannot hold the mesh ref -- must report truncation");
	ZENITH_ASSERT_LT((u_int)strlen(acTiny), (u_int)sizeof(acTiny),
		"a truncated ref must stay NUL-terminated within the cap");

	const bool bSharedFits = ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_SKELETON, acTiny, sizeof(acTiny));
	ZENITH_ASSERT_FALSE(bSharedFits, "an 8-byte cap cannot hold the shared skeleton ref");
	ZENITH_ASSERT_LT((u_int)strlen(acTiny), (u_int)sizeof(acTiny),
		"a truncated shared ref must stay NUL-terminated within the cap");
}

// ############################################################################
// 5. The frozen 9-clip metadata (golden literals)
// ############################################################################

// Golden-locks the shared 9-clip set frozen at SC1: exact names, durations, loop
// flags, shared paths, and the metadata SC4 stamps on each in-memory clip.
ZENITH_TEST(ZM_Gen, HumanGen_ClipMetadataGolden)
{
	static_assert((u_int)ZM_HUMAN_CLIP_COUNT == 9u,
		"the shared human clip enum must stay complete at nine entries");
	static_assert((u_int)ZM_HUMAN_ASSET_KIND_COUNT == 4u,
		"human animation files are shared, never appended to the per-model asset kinds");
	static_assert((u_int)ZM_HUMAN_SHARED_ASSET_ANIM_IDLE
		== (u_int)ZM_HUMAN_SHARED_ASSET_SKELETON + 1u,
		"the contiguous shared animation range must immediately follow the skeleton");
	static_assert((u_int)ZM_HUMAN_SHARED_ASSET_KIND_COUNT
		== (u_int)ZM_HUMAN_SHARED_ASSET_ANIM_IDLE + (u_int)ZM_HUMAN_CLIP_COUNT,
		"the shared asset enum must contain one path for every human clip");
	static_assert(uZM_CREATURE_ANIM_TICKS_PER_SECOND == 24u,
		"human clips reuse the pinned 24 ticks-per-second animation kit");

	for (u_int c = 0; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
	{
		const HumanClipGold& xG = g_axHumanClipGold[c];
		ZENITH_ASSERT_EQ((u_int)xG.m_eClip, c,
			"clip-golden row %u no longer matches the contiguous clip enum", c);
		ZENITH_ASSERT_STREQ(ZM_HumanClipName(xG.m_eClip), xG.m_szName,
			"clip %u name drifted", c);
		ZENITH_ASSERT_EQ(ZM_HumanClipDurationSeconds(xG.m_eClip), xG.m_fDurationSeconds,
			"clip %u duration drifted", c);
		ZENITH_ASSERT_TRUE(ZM_HumanClipLooping(xG.m_eClip) == xG.m_bLooping,
			"clip %u loop flag drifted", c);

		char acPath[256];
		const ZM_HUMAN_SHARED_ASSET_KIND eKind = (ZM_HUMAN_SHARED_ASSET_KIND)
			(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE + c);
		const bool bPathFits = ZM_HumanSharedAssetPath(eKind, acPath, sizeof(acPath));
		ZENITH_ASSERT_TRUE(bPathFits, "shared path for clip %u must fit", c);
		char acExpected[256];
		const int iExpectedLength = snprintf(acExpected, sizeof(acExpected),
			"game:Humans/Shared/Human_%s.zanim", xG.m_szName);
		const bool bExpectedFits = iExpectedLength >= 0
			&& (u_int)iExpectedLength < (u_int)sizeof(acExpected);
		ZENITH_ASSERT_TRUE(bExpectedFits, "golden shared path for clip %u must fit", c);
		if (bPathFits && bExpectedFits)
		{
			ZENITH_ASSERT_STREQ(acPath, acExpected,
				"clip %u no longer maps to its one shared path", c);
		}

		Flux_AnimationClip xBuilt;
		ZM_BuildHumanClip(xG.m_eClip, xBuilt);
		ZENITH_ASSERT_STREQ(xBuilt.GetName().c_str(), xG.m_szName,
			"built clip %u name was not stamped from the golden metadata", c);
		ZENITH_ASSERT_EQ(xBuilt.GetDuration(), xG.m_fDurationSeconds,
			"built clip %u duration was not stamped from the golden metadata", c);
		ZENITH_ASSERT_EQ(xBuilt.GetTicksPerSecond(), uZM_CREATURE_ANIM_TICKS_PER_SECOND,
			"built clip %u ticks-per-second must be the shared 24-tps value", c);
		ZENITH_ASSERT_TRUE(xBuilt.IsLooping() == xG.m_bLooping,
			"built clip %u looping flag was not stamped from the golden metadata", c);

		for (u_int other = c + 1u; other < (u_int)ZM_HUMAN_CLIP_COUNT; ++other)
		{
			ZENITH_ASSERT_TRUE(strcmp(xG.m_szName, g_axHumanClipGold[other].m_szName) != 0,
				"clip-golden rows %u/%u reuse one name", c, other);
		}
	}
}

// ############################################################################
// 6. Build determinism (reflexive) -- byte-identity + hash machinery
// ############################################################################

// Reflexive lock on the SC1 determinism helpers: building the SAME id twice into
// two separate bundles yields byte-identical meshes (ZM_HumanMeshEqual), an equal
// build (ZM_HumanBuildEqual), and an equal content hash (ZM_HumanContentHash).
// Plus a cheap non-degeneracy guard -- two DISTINCT ids whose recipes genuinely
// differ (PlayerM: AVERAGE/FAIR vs Bram: STOCKY/BROWN -> different mesh loft AND
// different placeholder albedo) must NOT collapse to the same hash + mesh, so the
// hash is not trivially constant. (The fuller SameSeedDeterminism over every id
// lands in SC2 once the real humanoid loft replaces the SC1 placeholder body.)
ZENITH_TEST(ZM_Gen, HumanGen_BuildDeterminism)
{
	ZM_Human xA;
	ZM_Human xB;
	ZM_BuildHuman(ZM_HUMAN_PLAYER_M, xA);
	ZM_BuildHuman(ZM_HUMAN_PLAYER_M, xB);

	ZENITH_ASSERT_TRUE(ZM_HumanBuildEqual(xA, xB),
		"rebuilding PlayerM must yield an equal bundle (ZM_HumanBuildEqual)");
	ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xA.m_xMesh, xB.m_xMesh),
		"rebuilding PlayerM must yield a byte-identical mesh (ZM_HumanMeshEqual)");
	ZENITH_ASSERT_EQ(ZM_HumanContentHash(xA), ZM_HumanContentHash(xB),
		"rebuilding PlayerM must yield an equal content hash");

	// Non-degeneracy: a distinct id with a genuinely different recipe must differ
	// in its hash OR its mesh (a trivially-constant hash would pass everything above
	// yet fail this).
	ZM_Human xOther;
	ZM_BuildHuman(ZM_HUMAN_LEADER_BRAM, xOther);
	const bool bHashDiffers = (ZM_HumanContentHash(xA) != ZM_HumanContentHash(xOther));
	const bool bMeshDiffers = !ZM_HumanMeshEqual(xA.m_xMesh, xOther.m_xMesh);
	ZENITH_ASSERT_TRUE(bHashDiffers || bMeshDiffers,
		"PlayerM and Bram differ in build+skin -- their content hash or mesh must differ");
}

// ############################################################################
// 7. SC2 mesh structural invariants over the complete human roster
// ############################################################################

// Every per-model loft carries one complete attribute/skin tuple per vertex,
// valid outward triangles, finite sane bounds, unit orthogonal normal/tangent
// frames, and a normalised <=2-bone bind inside the frozen 16-bone cap. Re-running
// the mandated finalisers in their fixed order must be a byte-identical no-op.
ZENITH_TEST(ZM_Gen, HumanGen_StructuralInvariants)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe((ZM_HUMAN_ID)id);
		ZM_GenMesh xMesh;
		ZM_BuildHumanMesh(xRecipe, xMesh);

		const u_int uNumVerts = xMesh.GetNumVerts();
		const u_int uNumIndices = xMesh.m_xIndices.GetSize();
		const bool bHasVerts = uNumVerts > 0u;
		const bool bIndexCountAligned = (uNumIndices % 3u) == 0u;
		const bool bHasTriangles = uNumIndices >= 3u;
		const bool bNormalsComplete = xMesh.m_xNormals.GetSize() == uNumVerts;
		const bool bUVsComplete = xMesh.m_xUVs.GetSize() == uNumVerts;
		const bool bTangentsComplete = xMesh.m_xTangents.GetSize() == uNumVerts;
		const bool bColorsComplete = xMesh.m_xColors.GetSize() == uNumVerts;
		const bool bBoneIndicesComplete = xMesh.m_xBoneIndices.GetSize() == uNumVerts;
		const bool bBoneWeightsComplete = xMesh.m_xBoneWeights.GetSize() == uNumVerts;
		const bool bCompleteVertexBuffers = bNormalsComplete
			&& bUVsComplete
			&& bTangentsComplete
			&& bColorsComplete
			&& bBoneIndicesComplete
			&& bBoneWeightsComplete;

		bool bIndicesInRange = true;
		u_int uFirstBadIndex = 0xFFFFFFFFu;
		for (u_int i = 0u; i < uNumIndices; ++i)
		{
			if (xMesh.m_xIndices.Get(i) >= uNumVerts)
			{
				bIndicesInRange = false;
				if (uFirstBadIndex == 0xFFFFFFFFu) { uFirstBadIndex = i; }
			}
		}

		// ZM_ValidateGenMesh and ZM_GenGenerateTangents dereference triangle
		// indices. Gate both behind every buffer/range prerequisite so a broken
		// builder reports assertions instead of turning the test into an OOB read.
		const bool bSafeForMeshAlgorithms = bHasVerts
			&& bHasTriangles
			&& bIndexCountAligned
			&& bIndicesInRange
			&& bCompleteVertexBuffers;

		ZENITH_ASSERT_TRUE(bHasVerts, "human %u mesh has no vertices", id);
		ZENITH_ASSERT_TRUE(bHasTriangles, "human %u mesh has no complete triangles", id);
		ZENITH_ASSERT_TRUE(bIndexCountAligned,
			"human %u index count is not triangle-aligned", id);
		ZENITH_ASSERT_TRUE(bIndicesInRange,
			"human %u index %u references a missing vertex", id, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(bNormalsComplete,
			"human %u must carry one normal per vertex", id);
		ZENITH_ASSERT_TRUE(bUVsComplete,
			"human %u must carry one UV per vertex", id);
		ZENITH_ASSERT_TRUE(bTangentsComplete,
			"human %u must carry one generated tangent per vertex", id);
		ZENITH_ASSERT_TRUE(bColorsComplete,
			"human %u must carry one colour per vertex", id);
		ZENITH_ASSERT_TRUE(bBoneIndicesComplete,
			"human %u must carry one bone-index tuple per vertex", id);
		ZENITH_ASSERT_TRUE(bBoneWeightsComplete,
			"human %u must carry one bone-weight tuple per vertex", id);
		ZENITH_ASSERT_EQ(xMesh.GetNumBones(), uZM_HUMAN_BONE_COUNT,
			"human %u must carry exactly the shared 16 bones", id);

		if (bHasVerts)
		{
			ZENITH_ASSERT_TRUE(BoundsFiniteAndSane(xMesh),
				"human %u bounds are non-finite, degenerate, or outside the sane world box", id);
		}

		if (bSafeForMeshAlgorithms)
		{
			const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_HUMAN_BONE_COUNT);
			ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward,
				"human %u has inward winding (bad tri %u)", id, xVal.m_uFirstBadTriangle);
			ZENITH_ASSERT_TRUE(xVal.m_bBoundsNonDegen, "human %u has degenerate bounds", id);
			ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne,
				"human %u has unnormalised skin weights (bad vert %u)", id, xVal.m_uFirstBadVertex);
			ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo,
				"human %u has a vertex with more than two influences", id);
			ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap,
				"human %u exceeds the shared 16-bone cap", id);
		}
		if (bHasVerts && bCompleteVertexBuffers)
		{
			bool bHasBlendedVertex = false;
			for (u_int v = 0u; v < uNumVerts; ++v)
			{
				const Zenith_Maths::Vector3& xPos = xMesh.m_xPositions.Get(v);
				const Zenith_Maths::Vector3& xNormal = xMesh.m_xNormals.Get(v);
				const Zenith_Maths::Vector2& xUV = xMesh.m_xUVs.Get(v);
				const Zenith_Maths::Vector3& xTangent = xMesh.m_xTangents.Get(v);
				const Zenith_Maths::Vector4& xColour = xMesh.m_xColors.Get(v);
				const glm::uvec4& xIndices = xMesh.m_xBoneIndices.Get(v);
				const glm::vec4& xWeights = xMesh.m_xBoneWeights.Get(v);

				ZENITH_ASSERT_TRUE(IsFiniteVec3(xPos), "human %u vertex %u position is non-finite", id, v);
				ZENITH_ASSERT_TRUE(IsFiniteVec3(xNormal), "human %u vertex %u normal is non-finite", id, v);
				ZENITH_ASSERT_TRUE(IsFiniteVec2(xUV), "human %u vertex %u UV is non-finite", id, v);
				ZENITH_ASSERT_TRUE(IsFiniteVec3(xTangent), "human %u vertex %u tangent is non-finite", id, v);
				ZENITH_ASSERT_TRUE(IsFiniteVec4(xColour), "human %u vertex %u colour is non-finite", id, v);
				ZENITH_ASSERT_TRUE(IsFiniteVec4(xWeights), "human %u vertex %u weights are non-finite", id, v);
				ZENITH_ASSERT_TRUE(IsUnitRange(xUV.x) && IsUnitRange(xUV.y),
					"human %u vertex %u UV lies outside the normalized atlas", id, v);
				ZENITH_ASSERT_TRUE(IsUnitRange(xColour.x) && IsUnitRange(xColour.y)
					&& IsUnitRange(xColour.z) && IsUnitRange(xColour.w),
					"human %u vertex %u colour lies outside [0,1]", id, v);

				ZENITH_ASSERT_LE(fabsf(glm::length(xNormal) - 1.0f), fHUMAN_UNIT_TOL,
					"human %u vertex %u normal is not unit length", id, v);
				ZENITH_ASSERT_LE(fabsf(glm::length(xTangent) - 1.0f), fHUMAN_UNIT_TOL,
					"human %u vertex %u tangent is not unit length", id, v);
				ZENITH_ASSERT_LE(fabsf(glm::dot(xNormal, xTangent)), fHUMAN_UNIT_TOL,
					"human %u vertex %u tangent is not orthogonal to its normal", id, v);

				u_int uActiveInfluences = 0u;
				float fWeightSum = 0.0f;
				for (u_int c = 0u; c < 4u; ++c)
				{
					ZENITH_ASSERT_LT(xIndices[c], uZM_HUMAN_BONE_COUNT,
						"human %u vertex %u influence %u references bone %u", id, v, c, xIndices[c]);
					ZENITH_ASSERT_TRUE(xWeights[c] >= 0.0f && xWeights[c] <= 1.0f,
						"human %u vertex %u influence %u weight is outside [0,1]", id, v, c);
					fWeightSum += xWeights[c];
					if (fabsf(xWeights[c]) > fHUMAN_INFLUENCE_EPS) { ++uActiveInfluences; }
				}
				ZENITH_ASSERT_LE(fabsf(fWeightSum - 1.0f), fHUMAN_WEIGHT_TOL,
					"human %u vertex %u weights do not sum to one", id, v);
				ZENITH_ASSERT_TRUE(uActiveInfluences >= 1u && uActiveInfluences <= 2u,
					"human %u vertex %u must have one or two active influences", id, v);
				if (uActiveInfluences == 2u) { bHasBlendedVertex = true; }
			}
			ZENITH_ASSERT_TRUE(bHasBlendedVertex,
				"human %u mesh has no two-bone blend at any articulation", id);
		}

		if (bSafeForMeshAlgorithms)
		{
			ZM_GenMesh xRefinalised = xMesh;
			ZM_GenGenerateTangents(xRefinalised);
			ZM_GenNormalizeSkinWeights(xRefinalised);
			ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xMesh, xRefinalised),
				"human %u mesh was not already finalised tangent-then-skin", id);
		}

		++uTested;
	}
	ZENITH_ASSERT_EQ(uTested, (u_int)ZM_HUMAN_COUNT,
		"structural gate did not exercise the complete human roster");
}

// ############################################################################
// 8. Every per-model skeleton is the canonical shared skeleton
// ############################################################################

ZENITH_TEST(ZM_Gen, HumanGen_PerModelBonesMatchShared)
{
	ZM_GenMesh xShared;
	ZM_AppendSharedHumanBones(xShared);
	ZENITH_ASSERT_EQ(xShared.GetNumBones(), uZM_HUMAN_BONE_COUNT,
		"canonical shared skeleton must contain exactly 16 bones");

	u_int uTested = 0u;
	for (u_int id = 0u; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		ZM_GenMesh xMesh;
		ZM_BuildHumanMesh(ZM_ResolveHumanRecipe((ZM_HUMAN_ID)id), xMesh);
		ZENITH_ASSERT_EQ(xMesh.GetNumBones(), uZM_HUMAN_BONE_COUNT,
			"human %u bone count differs from the shared skeleton", id);

		if (xMesh.GetNumBones() == uZM_HUMAN_BONE_COUNT
			&& xShared.GetNumBones() == uZM_HUMAN_BONE_COUNT)
		{
			for (u_int b = 0u; b < uZM_HUMAN_BONE_COUNT; ++b)
			{
				const ZM_GenBone& xBone = xMesh.m_xBones.Get(b);
				const ZM_GenBone& xReference = xShared.m_xBones.Get(b);
				ZENITH_ASSERT_STREQ(xBone.m_szName, g_aszSharedBones[b],
					"human %u bone %u name/order differs from the frozen rig", id, b);
				ZENITH_ASSERT_STREQ(xBone.m_szName, xReference.m_szName,
					"human %u bone %u name differs from the canonical emit", id, b);
				ZENITH_ASSERT_EQ(ZM_GenMeshFindBone(xMesh, g_aszSharedBones[b]), (int)b,
					"human %u shared bone '%s' does not resolve to index %u", id, g_aszSharedBones[b], b);
				ZENITH_ASSERT_TRUE(BoneFieldsEqual(xBone, xReference),
					"human %u bone %u bind-local data differs from the shared skeleton", id, b);
			}
		}

		ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.GetSize(), xMesh.GetNumVerts(),
			"human %u must carry one bone-index tuple per vertex", id);
		if (xMesh.m_xBoneIndices.GetSize() == xMesh.GetNumVerts())
		{
			for (u_int v = 0u; v < xMesh.m_xBoneIndices.GetSize(); ++v)
			{
				const glm::uvec4& xIndices = xMesh.m_xBoneIndices.Get(v);
				for (u_int c = 0u; c < 4u; ++c)
				{
					ZENITH_ASSERT_LT(xIndices[c], uZM_HUMAN_BONE_COUNT,
						"human %u vertex %u influence %u references bone %u", id, v, c, xIndices[c]);
				}
			}
		}

		++uTested;
	}
	ZENITH_ASSERT_EQ(uTested, (u_int)ZM_HUMAN_COUNT,
		"shared-skeleton gate did not exercise the complete human roster");
}

// ############################################################################
// 9. Same-seed determinism over the complete human roster
// ############################################################################

ZENITH_TEST(ZM_Gen, HumanGen_SameSeedDeterminism)
{
	u_int uTested = 0u;
	for (u_int id = 0u; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HUMAN_ID eId = (ZM_HUMAN_ID)id;
		ZM_Human xA;
		ZM_Human xB;
		ZM_BuildHuman(eId, xA);
		ZM_BuildHuman(eId, xB);

		ZENITH_ASSERT_EQ((u_int)xA.m_eId, id, "first human %u build carries the wrong id", id);
		ZENITH_ASSERT_EQ((u_int)xB.m_eId, id, "second human %u build carries the wrong id", id);
		ZENITH_ASSERT_GT(xA.m_xMesh.GetNumVerts(), 0u, "human %u deterministic mesh is empty", id);
		ZENITH_ASSERT_GT(xA.m_xMesh.m_xTangents.GetSize(), 0u,
			"human %u deterministic mesh has no finalised tangents", id);
		ZENITH_ASSERT_FALSE(xA.m_xAlbedo.IsEmpty(), "human %u deterministic albedo is empty", id);
		ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xA.m_xMesh, xB.m_xMesh),
			"human %u mesh differs across same-seed rebuilds", id);
		ZENITH_ASSERT_TRUE(ZM_HumanBuildEqual(xA, xB),
			"human %u bundle differs across same-seed rebuilds", id);
		ZENITH_ASSERT_EQ(ZM_HumanContentHash(xA), ZM_HumanContentHash(xB),
			"human %u content hash differs across same-seed rebuilds", id);

		++uTested;
	}
	ZENITH_ASSERT_EQ(uTested, (u_int)ZM_HUMAN_COUNT,
		"determinism gate did not exercise the complete human roster");
}

// ############################################################################
// 10. Mesh-domain sensitivity
// ############################################################################

// Clone one resolved recipe and perturb exactly one domain seed at a time. Every
// non-MESH perturbation (including SKELETON) must be invisible to the mesh, while
// a MESH-only perturbation must change it. PlayerM and Villager additionally share
// build/height, skin, hair style, and hair colour: their independently-derived
// MESH streams must yield distinct geometry without freezing an artistic golden.
ZENITH_TEST(ZM_Gen, HumanGen_Sensitivity)
{
	const ZM_HumanRecipe xPlayerRecipe = ZM_ResolveHumanRecipe(ZM_HUMAN_PLAYER_M);
	const ZM_HumanRecipe xVillagerRecipe = ZM_ResolveHumanRecipe(ZM_HUMAN_TOWN_VILLAGER);
	constexpr u_int64 ulDOMAIN_SEED_PERTURBATION = 0xD1B54A32D192ED03ULL;

	ZM_GenMesh xBaselineMesh;
	ZM_BuildHumanMesh(xPlayerRecipe, xBaselineMesh);

	// Domain isolation: changing any one non-MESH stream must not affect mesh
	// bytes. This explicitly covers SKELETON -- the human rig is shared/fixed.
	u_int uNonMeshDomainsTested = 0u;
	bool bSkeletonDomainTested = false;
	for (u_int d = 0u; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
	{
		const ZM_GEN_DOMAIN eDomain = (ZM_GEN_DOMAIN)d;
		if (eDomain == ZM_GEN_DOMAIN_MESH) { continue; }

		ZM_HumanRecipe xDomainMutated = xPlayerRecipe;
		xDomainMutated.m_aulDomainSeed[eDomain] ^= ulDOMAIN_SEED_PERTURBATION;
		ZENITH_ASSERT_NE(xDomainMutated.m_aulDomainSeed[eDomain],
			xPlayerRecipe.m_aulDomainSeed[eDomain],
			"domain %u seed perturbation must change that seed", d);

		ZM_GenMesh xDomainMutatedMesh;
		ZM_BuildHumanMesh(xDomainMutated, xDomainMutatedMesh);
		ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xBaselineMesh, xDomainMutatedMesh),
			"human mesh consumed forbidden non-MESH domain %u", d);
		if (eDomain == ZM_GEN_DOMAIN_SKELETON) { bSkeletonDomainTested = true; }
		++uNonMeshDomainsTested;
	}
	ZENITH_ASSERT_EQ(uNonMeshDomainsTested, (u_int)ZM_GEN_DOMAIN_COUNT - 1u,
		"domain-isolation gate did not exercise every non-MESH stream");
	ZENITH_ASSERT_TRUE(bSkeletonDomainTested,
		"domain-isolation gate must explicitly exercise the forbidden SKELETON stream");

	// MESH sensitivity: changing only the MESH seed must alter the authored mesh.
	ZM_HumanRecipe xMeshMutatedRecipe = xPlayerRecipe;
	xMeshMutatedRecipe.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= ulDOMAIN_SEED_PERTURBATION;
	ZENITH_ASSERT_NE(xMeshMutatedRecipe.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH],
		xPlayerRecipe.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH],
		"MESH-domain perturbation must change that seed");
	ZM_GenMesh xMeshMutated;
	ZM_BuildHumanMesh(xMeshMutatedRecipe, xMeshMutated);
	ZENITH_ASSERT_FALSE(ZM_HumanMeshEqual(xBaselineMesh, xMeshMutated),
		"changing only the MESH-domain seed must change human geometry");

	ZENITH_ASSERT_EQ((u_int)xPlayerRecipe.m_eBuild, (u_int)xVillagerRecipe.m_eBuild,
		"sensitivity control pair must share a build");
	ZENITH_ASSERT_EQ(xPlayerRecipe.m_fHeightScale, xVillagerRecipe.m_fHeightScale,
		"sensitivity control pair must share a height scale");
	ZENITH_ASSERT_EQ((u_int)xPlayerRecipe.m_eSkinTone, (u_int)xVillagerRecipe.m_eSkinTone,
		"sensitivity control pair must share a skin tone");
	ZENITH_ASSERT_EQ(xPlayerRecipe.m_uHairStyle, xVillagerRecipe.m_uHairStyle,
		"sensitivity control pair must share a hair style");
	ZENITH_ASSERT_EQ((u_int)xPlayerRecipe.m_eHairColour, (u_int)xVillagerRecipe.m_eHairColour,
		"sensitivity control pair must share a hair colour");
	ZENITH_ASSERT_NE(xPlayerRecipe.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH],
		xVillagerRecipe.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH],
		"distinct humans must carry distinct mesh-domain seeds");

	ZM_Human xPlayer;
	ZM_Human xVillager;
	ZM_BuildHuman(ZM_HUMAN_PLAYER_M, xPlayer);
	ZM_BuildHuman(ZM_HUMAN_TOWN_VILLAGER, xVillager);

	ZENITH_ASSERT_FALSE(ZM_HumanMeshEqual(xPlayer.m_xMesh, xVillager.m_xMesh),
		"same-build humans with distinct mesh seeds collapsed to one canned geometry");
	ZENITH_ASSERT_FALSE(ZM_HumanBuildEqual(xPlayer, xVillager),
		"distinct mesh-domain seeds collapsed to one identical human bundle");
	ZENITH_ASSERT_NE(ZM_HumanContentHash(xPlayer), ZM_HumanContentHash(xVillager),
		"distinct mesh-domain seeds collapsed to one identical content hash");
}

// ############################################################################
// 11. SC3 appearance coverage + all-roster albedo material invariants
// ############################################################################

// Every roster row reaches the recipe unchanged, every declared appearance axis
// is represented, and every shared atlas island is normalized/non-degenerate.
// The real albedo must be deterministic, finite, opaque, non-flat, and agree with
// the complete-bundle driver for every id. No exact pixel, colour, or hash is
// frozen; only structural material properties and cross-id differentiation are.
ZENITH_TEST(ZM_Gen, HumanGen_AppearanceAlbedoStructural)
{
	bool abBuildSeen[ZM_HUMAN_BUILD_COUNT] = {};
	bool abSkinSeen[ZM_HUMAN_SKIN_COUNT] = {};
	bool abHairStyleSeen[uZM_HUMAN_HAIR_STYLE_COUNT] = {};
	bool abHairColourSeen[ZM_HUMAN_HAIR_COUNT] = {};
	bool abOutfitSeen[ZM_HUMAN_OUTFIT_COUNT] = {};
	bool abAttachmentSeen[ZM_HUMAN_ATTACHMENT_COUNT] = {};
	u_int auBundleHashes[ZM_HUMAN_COUNT] = {};

	const ZM_GenUVIsland* apxIslands[] =
	{
		&xZM_HUMAN_UV_HEAD,
		&xZM_HUMAN_UV_HAIR,
		&xZM_HUMAN_UV_TORSO,
		&xZM_HUMAN_UV_ARM_L,
		&xZM_HUMAN_UV_ARM_R,
		&xZM_HUMAN_UV_LEG_L,
		&xZM_HUMAN_UV_LEG_R,
		&xZM_HUMAN_UV_ATTACHMENT,
	};
	const char* aszIslandNames[] =
	{
		"head", "hair", "torso", "left arm", "right arm", "left leg", "right leg", "attachment"
	};
	static_assert(sizeof(apxIslands) / sizeof(apxIslands[0])
		== sizeof(aszIslandNames) / sizeof(aszIslandNames[0]));
	for (u_int i = 0u; i < sizeof(apxIslands) / sizeof(apxIslands[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(AtlasIslandValid(*apxIslands[i]),
			"human %s atlas island is non-finite, degenerate, or outside [0,1]", aszIslandNames[i]);
	}

	for (u_int id = 0u; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HUMAN_ID eId = (ZM_HUMAN_ID)id;
		const ZM_HumanData& xData = ZM_GetHumanData(eId);
		const ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe(eId);

		ZENITH_ASSERT_EQ((u_int)xRecipe.m_eBuild, (u_int)xData.m_eBuild,
			"human %u recipe build differs from its roster row", id);
		ZENITH_ASSERT_EQ((u_int)xRecipe.m_eSkinTone, (u_int)xData.m_eSkinTone,
			"human %u recipe skin tone differs from its roster row", id);
		ZENITH_ASSERT_EQ(xRecipe.m_uHairStyle, xData.m_uHairStyle,
			"human %u recipe hair style differs from its roster row", id);
		ZENITH_ASSERT_EQ((u_int)xRecipe.m_eHairColour, (u_int)xData.m_eHairColour,
			"human %u recipe hair colour differs from its roster row", id);
		ZENITH_ASSERT_EQ((u_int)xRecipe.m_eOutfit, (u_int)xData.m_eOutfit,
			"human %u recipe outfit differs from its roster row", id);
		ZENITH_ASSERT_EQ((u_int)xRecipe.m_eAttachment, (u_int)xData.m_eAttachment,
			"human %u recipe attachment differs from its roster row", id);
		ZENITH_ASSERT_EQ(xRecipe.m_uSyntheticSeed, ZM_GenHashName(xData.m_szName),
			"human %u synthetic seed is not its name hash", id);

		const bool bBuildInRange = (u_int)xRecipe.m_eBuild < (u_int)ZM_HUMAN_BUILD_COUNT;
		const bool bSkinInRange = (u_int)xRecipe.m_eSkinTone < (u_int)ZM_HUMAN_SKIN_COUNT;
		const bool bHairStyleInRange = xRecipe.m_uHairStyle < uZM_HUMAN_HAIR_STYLE_COUNT;
		const bool bHairColourInRange = (u_int)xRecipe.m_eHairColour < (u_int)ZM_HUMAN_HAIR_COUNT;
		const bool bOutfitInRange = (u_int)xRecipe.m_eOutfit < (u_int)ZM_HUMAN_OUTFIT_COUNT;
		const bool bAttachmentInRange = (u_int)xRecipe.m_eAttachment < (u_int)ZM_HUMAN_ATTACHMENT_COUNT;
		ZENITH_ASSERT_TRUE(bBuildInRange, "human %u build is outside its enum", id);
		ZENITH_ASSERT_TRUE(bSkinInRange, "human %u skin tone is outside its enum", id);
		ZENITH_ASSERT_TRUE(bHairStyleInRange, "human %u hair style %u is unsupported", id,
			xRecipe.m_uHairStyle);
		ZENITH_ASSERT_TRUE(bHairColourInRange, "human %u hair colour is outside its enum", id);
		ZENITH_ASSERT_TRUE(bOutfitInRange, "human %u outfit is outside its enum", id);
		ZENITH_ASSERT_TRUE(bAttachmentInRange, "human %u attachment is outside its enum", id);
		if (bBuildInRange) { abBuildSeen[xRecipe.m_eBuild] = true; }
		if (bSkinInRange) { abSkinSeen[xRecipe.m_eSkinTone] = true; }
		if (bHairStyleInRange) { abHairStyleSeen[xRecipe.m_uHairStyle] = true; }
		if (bHairColourInRange) { abHairColourSeen[xRecipe.m_eHairColour] = true; }
		if (bOutfitInRange) { abOutfitSeen[xRecipe.m_eOutfit] = true; }
		if (bAttachmentInRange) { abAttachmentSeen[xRecipe.m_eAttachment] = true; }

		for (u_int d = 0u; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
		{
			const u_int64 ulExpected = ZM_GenDeriveSeed(xRecipe.m_uSyntheticSeed, id,
				uZM_HUMAN_SYNTHETIC_EVO_STAGE, (ZM_GEN_DOMAIN)d);
			ZENITH_ASSERT_EQ(xRecipe.m_aulDomainSeed[d], ulExpected,
				"human %u domain %u seed is not the canonical derivation", id, d);
		}

		const ZM_GenImage xAlbedoA = ZM_BuildHumanAlbedo(xRecipe);
		const ZM_GenImage xAlbedoB = ZM_BuildHumanAlbedo(xRecipe);
		ZENITH_ASSERT_FALSE(xAlbedoA.IsEmpty(), "human %u direct albedo is empty", id);
		ZENITH_ASSERT_EQ(xAlbedoA.GetWidth(), uZM_HUMAN_ALBEDO_RESOLUTION,
			"human %u albedo width differs from the shared resolution", id);
		ZENITH_ASSERT_EQ(xAlbedoA.GetHeight(), uZM_HUMAN_ALBEDO_RESOLUTION,
			"human %u albedo height differs from the shared resolution", id);
		ZENITH_ASSERT_TRUE(xAlbedoA.Equals(xAlbedoB),
			"human %u direct albedo is not deterministic", id);
		ZENITH_ASSERT_EQ(xAlbedoA.ContentHash(), xAlbedoB.ContentHash(),
			"human %u direct albedo hash is not deterministic", id);

		const HumanImageScan xScan = ScanHumanImage(xAlbedoA);
		ZENITH_ASSERT_TRUE(xScan.m_bPackedSizeMatches,
			"human %u albedo does not pack to exactly width*height*4 bytes", id);
		ZENITH_ASSERT_TRUE(xScan.m_bFinite,
			"human %u albedo has a non-finite texel at (%u,%u)", id,
			xScan.m_uFirstBadX, xScan.m_uFirstBadY);
		ZENITH_ASSERT_TRUE(xScan.m_bUnitRange,
			"human %u albedo has a channel outside [0,1] at (%u,%u)", id,
			xScan.m_uFirstBadX, xScan.m_uFirstBadY);
		ZENITH_ASSERT_TRUE(xScan.m_bOpaque,
			"human %u albedo has non-opaque alpha at (%u,%u)", id,
			xScan.m_uFirstBadX, xScan.m_uFirstBadY);
		ZENITH_ASSERT_GE(xScan.m_uDistinctRGB, uHUMAN_MIN_MATERIAL_COLOURS,
			"human %u albedo must contain structural skin/hair/outfit colour variation", id);

		ZM_Human xFullA;
		ZM_Human xFullB;
		ZM_BuildHuman(eId, xFullA);
		ZM_BuildHuman(eId, xFullB);
		ZENITH_ASSERT_TRUE(xFullA.m_xAlbedo.Equals(xAlbedoA),
			"human %u complete driver did not use the shared albedo builder", id);
		ZENITH_ASSERT_EQ(xFullA.m_xAlbedo.ContentHash(), xAlbedoA.ContentHash(),
			"human %u complete-driver albedo hash differs from the direct builder", id);
		ZENITH_ASSERT_TRUE(ZM_HumanBuildEqual(xFullA, xFullB),
			"human %u complete appearance bundle is not deterministic", id);
		ZENITH_ASSERT_EQ(ZM_HumanContentHash(xFullA), ZM_HumanContentHash(xFullB),
			"human %u complete appearance hash is not deterministic", id);
		auBundleHashes[id] = ZM_HumanContentHash(xFullA);
	}

	for (u_int v = 0u; v < (u_int)ZM_HUMAN_BUILD_COUNT; ++v)
	{
		ZENITH_ASSERT_TRUE(abBuildSeen[v], "human roster never exercises build %u", v);
	}
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_SKIN_COUNT; ++v)
	{
		ZENITH_ASSERT_TRUE(abSkinSeen[v], "human roster never exercises skin tone %u", v);
	}
	for (u_int v = 0u; v < uZM_HUMAN_HAIR_STYLE_COUNT; ++v)
	{
		ZENITH_ASSERT_TRUE(abHairStyleSeen[v], "human roster never exercises hair style %u", v);
	}
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_HAIR_COUNT; ++v)
	{
		ZENITH_ASSERT_TRUE(abHairColourSeen[v], "human roster never exercises hair colour %u", v);
	}
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_OUTFIT_COUNT; ++v)
	{
		ZENITH_ASSERT_TRUE(abOutfitSeen[v], "human roster never exercises outfit %u", v);
	}
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++v)
	{
		ZENITH_ASSERT_TRUE(abAttachmentSeen[v], "human roster never exercises attachment %u", v);
	}

	for (u_int a = 0u; a < (u_int)ZM_HUMAN_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_HUMAN_COUNT; ++b)
		{
			ZENITH_ASSERT_NE(auBundleHashes[a], auBundleHashes[b],
				"humans %u/%u collapsed to one complete appearance hash", a, b);
		}
	}
}

// ############################################################################
// 12. SC3 output-domain isolation + appearance-axis sensitivity
// ############################################################################

// A one-domain seed perturbation may affect exactly its owned output: MESH owns
// geometry (including silhouettes), ALBEDO owns texels, and every other frozen
// domain is irrelevant to the human bundle. With seeds held fixed, every declared
// skin/hair/outfit axis must remain materially observable without pinning colours.
ZENITH_TEST(ZM_Gen, HumanGen_AppearanceDomainIsolation)
{
	ZM_HumanRecipe xBaselineRecipe = ZM_ResolveHumanRecipe(ZM_HUMAN_PLAYER_M);
	xBaselineRecipe.m_eAttachment = ZM_HUMAN_ATTACHMENT_NONE;

	ZM_Human xBaseline;
	BuildHumanFromRecipe(xBaselineRecipe, xBaseline);
	for (u_int d = 0u; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
	{
		const ZM_GEN_DOMAIN eDomain = (ZM_GEN_DOMAIN)d;
		ZM_HumanRecipe xMutatedRecipe = xBaselineRecipe;
		xMutatedRecipe.m_aulDomainSeed[eDomain] ^= ulHUMAN_DOMAIN_SEED_PERTURBATION;
		ZENITH_ASSERT_NE(xMutatedRecipe.m_aulDomainSeed[eDomain],
			xBaselineRecipe.m_aulDomainSeed[eDomain],
			"appearance domain %u perturbation must change that seed", d);

		ZM_Human xMutated;
		BuildHumanFromRecipe(xMutatedRecipe, xMutated);
		const bool bMeshEqual = ZM_HumanMeshEqual(xBaseline.m_xMesh, xMutated.m_xMesh);
		const bool bAlbedoEqual = xBaseline.m_xAlbedo.Equals(xMutated.m_xAlbedo);
		const bool bBuildEqual = ZM_HumanBuildEqual(xBaseline, xMutated);
		const bool bHashEqual = ZM_HumanContentHash(xBaseline) == ZM_HumanContentHash(xMutated);

		if (eDomain == ZM_GEN_DOMAIN_MESH)
		{
			ZENITH_ASSERT_FALSE(bMeshEqual, "MESH-domain perturbation must change human geometry");
			ZENITH_ASSERT_TRUE(bAlbedoEqual, "MESH-domain perturbation leaked into human albedo");
			ZENITH_ASSERT_FALSE(bBuildEqual, "MESH-domain perturbation left the whole bundle equal");
			ZENITH_ASSERT_FALSE(bHashEqual, "MESH-domain perturbation left the content hash equal");
		}
		else if (eDomain == ZM_GEN_DOMAIN_ALBEDO)
		{
			ZENITH_ASSERT_TRUE(bMeshEqual, "ALBEDO-domain perturbation leaked into human geometry");
			ZENITH_ASSERT_FALSE(bAlbedoEqual, "ALBEDO-domain perturbation must change human texels");
			ZENITH_ASSERT_FALSE(bBuildEqual, "ALBEDO-domain perturbation left the whole bundle equal");
			ZENITH_ASSERT_FALSE(bHashEqual, "ALBEDO-domain perturbation left the content hash equal");
		}
		else
		{
			ZENITH_ASSERT_TRUE(bMeshEqual,
				"human mesh consumed forbidden domain %u (including shared SKELETON)", d);
			ZENITH_ASSERT_TRUE(bAlbedoEqual, "human albedo consumed forbidden non-ALBEDO domain %u", d);
			ZENITH_ASSERT_TRUE(bBuildEqual, "forbidden domain %u changed the human bundle", d);
			ZENITH_ASSERT_TRUE(bHashEqual, "forbidden domain %u changed the human content hash", d);
		}
	}

	ZM_GenImage axSkin[ZM_HUMAN_SKIN_COUNT];
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_SKIN_COUNT; ++v)
	{
		ZM_HumanRecipe xRecipe = xBaselineRecipe;
		xRecipe.m_eSkinTone = (ZM_HUMAN_SKIN_TONE)v;
		axSkin[v] = ZM_BuildHumanAlbedo(xRecipe);
	}
	ZM_GenImage axHairStyle[uZM_HUMAN_HAIR_STYLE_COUNT];
	for (u_int v = 0u; v < uZM_HUMAN_HAIR_STYLE_COUNT; ++v)
	{
		ZM_HumanRecipe xRecipe = xBaselineRecipe;
		xRecipe.m_uHairStyle = v;
		axHairStyle[v] = ZM_BuildHumanAlbedo(xRecipe);
	}
	ZM_GenImage axHairColour[ZM_HUMAN_HAIR_COUNT];
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_HAIR_COUNT; ++v)
	{
		ZM_HumanRecipe xRecipe = xBaselineRecipe;
		xRecipe.m_eHairColour = (ZM_HUMAN_HAIR_COLOUR)v;
		axHairColour[v] = ZM_BuildHumanAlbedo(xRecipe);
	}
	ZM_GenImage axOutfit[ZM_HUMAN_OUTFIT_COUNT];
	for (u_int v = 0u; v < (u_int)ZM_HUMAN_OUTFIT_COUNT; ++v)
	{
		ZM_HumanRecipe xRecipe = xBaselineRecipe;
		xRecipe.m_eOutfit = (ZM_HUMAN_OUTFIT)v;
		axOutfit[v] = ZM_BuildHumanAlbedo(xRecipe);
	}

	u_int uCollisionA = 0xFFFFFFFFu;
	u_int uCollisionB = 0xFFFFFFFFu;
	const bool bSkinDistinct = ImagesPairwiseDistinct(axSkin,
		(u_int)ZM_HUMAN_SKIN_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bSkinDistinct,
		"skin tones %u/%u collapse to equal albedo bytes or hashes", uCollisionA, uCollisionB);
	const bool bHairStyleDistinct = ImagesPairwiseDistinct(axHairStyle,
		uZM_HUMAN_HAIR_STYLE_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bHairStyleDistinct,
		"hair styles %u/%u collapse to equal albedo bytes or hashes", uCollisionA, uCollisionB);
	const bool bHairColourDistinct = ImagesPairwiseDistinct(axHairColour,
		(u_int)ZM_HUMAN_HAIR_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bHairColourDistinct,
		"hair colours %u/%u collapse to equal albedo bytes or hashes", uCollisionA, uCollisionB);
	const bool bOutfitDistinct = ImagesPairwiseDistinct(axOutfit,
		(u_int)ZM_HUMAN_OUTFIT_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bOutfitDistinct,
		"outfits %u/%u collapse to equal albedo bytes or hashes", uCollisionA, uCollisionB);
}

// ############################################################################
// 13. SC3 hair-style geometry silhouettes
// ############################################################################

// The six locked hair styles are geometry, not painted-only aliases. Each pure
// appender produces a deterministic valid mesh rigidly bound to the shared Head
// bone, and the complete human builder preserves that pairwise silhouette split.
// Counts and coordinates deliberately remain free for later art tuning.
ZENITH_TEST(ZM_Gen, HumanGen_HairStyleSilhouettes)
{
	static_assert(uZM_HUMAN_HAIR_STYLE_COUNT == 6u,
		"SC3 freezes exactly six human hair geometry styles");
	ZM_GenMesh xShared;
	ZM_AppendSharedHumanBones(xShared);
	const int iHead = ZM_GenMeshFindBone(xShared, "Head");
	ZENITH_ASSERT_GE(iHead, 0, "shared human rig must expose the Head bone");

	ZM_GenMesh axHairOnly[uZM_HUMAN_HAIR_STYLE_COUNT];
	ZM_GenMesh axFull[uZM_HUMAN_HAIR_STYLE_COUNT];
	u_int auContentHashes[uZM_HUMAN_HAIR_STYLE_COUNT] = {};
	for (u_int style = 0u; style < uZM_HUMAN_HAIR_STYLE_COUNT; ++style)
	{
		ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe(ZM_HUMAN_PLAYER_M);
		xRecipe.m_uHairStyle = style;
		xRecipe.m_eAttachment = ZM_HUMAN_ATTACHMENT_NONE;

		ZM_GenMesh& xHairA = axHairOnly[style];
		ZM_AppendSharedHumanBones(xHairA);
		const u_int uHairFirstVert = xHairA.GetNumVerts();
		ZM_AppendHumanHair(xRecipe, xHairA);
		u_int uFirstBadIndex = 0xFFFFFFFFu;
		const bool bSafeToFinalise = MeshSafeForFinalization(xHairA, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(bSafeToFinalise,
			"hair style %u is unsafe to finalise (bad index slot %u)", style, uFirstBadIndex);
		if (bSafeToFinalise)
		{
			ZM_GenGenerateTangents(xHairA);
			ZM_GenNormalizeSkinWeights(xHairA);
		}

		ZM_GenMesh xHairB;
		ZM_AppendSharedHumanBones(xHairB);
		ZM_AppendHumanHair(xRecipe, xHairB);
		u_int uSecondBadIndex = 0xFFFFFFFFu;
		const bool bSecondSafeToFinalise = MeshSafeForFinalization(xHairB, uSecondBadIndex);
		ZENITH_ASSERT_TRUE(bSecondSafeToFinalise,
			"second hair style %u build is unsafe to finalise (bad index slot %u)",
			style, uSecondBadIndex);
		if (bSecondSafeToFinalise)
		{
			ZM_GenGenerateTangents(xHairB);
			ZM_GenNormalizeSkinWeights(xHairB);
		}

		ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xHairA, xHairB),
			"hair style %u appender is not byte-deterministic", style);
		ZENITH_ASSERT_GT(xHairA.GetNumVerts(), uHairFirstVert,
			"hair style %u appended no silhouette vertices", style);
		ZENITH_ASSERT_GT(xHairA.GetNumTris(), 0u,
			"hair style %u appended no complete triangles", style);
		ZENITH_ASSERT_EQ(xHairA.GetNumBones(), uZM_HUMAN_BONE_COUNT,
			"hair style %u changed the shared bone count", style);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(xHairA, xShared),
			"hair style %u changed shared bone data", style);
		ZENITH_ASSERT_TRUE(BoundsFiniteAndSane(xHairA),
			"hair style %u has non-finite, degenerate, or unreasonable bounds", style);

		u_int uValidationBadIndex = 0xFFFFFFFFu;
		const bool bSafeToValidate = MeshSafeForValidation(xHairA, uValidationBadIndex);
		ZENITH_ASSERT_TRUE(bSafeToValidate,
			"hair style %u is structurally incomplete (bad index slot %u)",
			style, uValidationBadIndex);
		if (bSafeToValidate)
		{
			const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xHairA, uZM_HUMAN_BONE_COUNT);
			ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward,
				"hair style %u has inward winding (bad tri %u)", style, xVal.m_uFirstBadTriangle);
			ZENITH_ASSERT_TRUE(xVal.m_bBoundsNonDegen, "hair style %u has degenerate bounds", style);
			ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne,
				"hair style %u has unnormalised weights (bad vert %u)", style, xVal.m_uFirstBadVertex);
			ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo,
				"hair style %u exceeds the two-influence loft contract", style);
			ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap,
				"hair style %u exceeds the shared 16-bone cap", style);

			u_int uFirstBadVertex = 0xFFFFFFFFu;
			ZENITH_ASSERT_TRUE(VertexRangeValid(xHairA, uHairFirstVert, xHairA.GetNumVerts(),
				iHead, &xZM_HUMAN_UV_HAIR, uFirstBadVertex),
				"hair style %u vertex %u violates finite/atlas/rigid-Head invariants",
				style, uFirstBadVertex);
		}

		ZM_BuildHumanMesh(xRecipe, axFull[style]);
		ZM_GenMesh xFullAgain;
		ZM_BuildHumanMesh(xRecipe, xFullAgain);
		ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(axFull[style], xFullAgain),
			"complete mesh for hair style %u is not deterministic", style);
		ZENITH_ASSERT_EQ(axFull[style].GetNumBones(), uZM_HUMAN_BONE_COUNT,
			"complete mesh for hair style %u changed the shared bone count", style);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(axFull[style], xShared),
			"complete mesh for hair style %u changed shared bone data", style);

		ZM_Human xHuman;
		BuildHumanFromRecipe(xRecipe, xHuman);
		auContentHashes[style] = ZM_HumanContentHash(xHuman);
	}

	// Append-order wiring: with attachment NONE, the complete mesh must be one
	// style-invariant body prefix followed by exactly the direct hair geometry.
	const ZM_GenMesh& xReferenceHair = axHairOnly[0u];
	const ZM_GenMesh& xReferenceFull = axFull[0u];
	const bool bReferenceContainsHair =
		xReferenceFull.GetNumVerts() >= xReferenceHair.GetNumVerts()
		&& xReferenceFull.m_xIndices.GetSize() >= xReferenceHair.m_xIndices.GetSize();
	ZENITH_ASSERT_TRUE(bReferenceContainsHair,
		"complete reference mesh cannot contain its direct hair suffix");
	u_int uReferenceBodyVerts = 0u;
	u_int uReferenceBodyIndices = 0u;
	if (bReferenceContainsHair)
	{
		uReferenceBodyVerts = xReferenceFull.GetNumVerts() - xReferenceHair.GetNumVerts();
		uReferenceBodyIndices = xReferenceFull.m_xIndices.GetSize()
			- xReferenceHair.m_xIndices.GetSize();
		ZENITH_ASSERT_GT(uReferenceBodyVerts, 0u, "human common body prefix has no vertices");
		ZENITH_ASSERT_GT(uReferenceBodyIndices, 0u, "human common body prefix has no indices");
	}

	for (u_int style = 0u; style < uZM_HUMAN_HAIR_STYLE_COUNT; ++style)
	{
		const ZM_GenMesh& xHair = axHairOnly[style];
		const ZM_GenMesh& xFull = axFull[style];
		const bool bContainsHair = xFull.GetNumVerts() >= xHair.GetNumVerts()
			&& xFull.m_xIndices.GetSize() >= xHair.m_xIndices.GetSize();
		ZENITH_ASSERT_TRUE(bContainsHair,
			"complete mesh for hair style %u cannot contain its direct hair suffix", style);
		if (!bReferenceContainsHair || !bContainsHair) { continue; }

		const u_int uBodyVerts = xFull.GetNumVerts() - xHair.GetNumVerts();
		const u_int uBodyIndices = xFull.m_xIndices.GetSize() - xHair.m_xIndices.GetSize();
		ZENITH_ASSERT_EQ(uBodyVerts, uReferenceBodyVerts,
			"hair style %u changed the common body-prefix vertex count", style);
		ZENITH_ASSERT_EQ(uBodyIndices, uReferenceBodyIndices,
			"hair style %u changed the common body-prefix index count", style);
		if (uBodyVerts != uReferenceBodyVerts || uBodyIndices != uReferenceBodyIndices) { continue; }

		u_int uFirstBadVertex = 0xFFFFFFFFu;
		const bool bBodyVerticesExact = VertexRangesExact(xFull, 0u,
			xReferenceFull, 0u, uReferenceBodyVerts, uFirstBadVertex);
		ZENITH_ASSERT_TRUE(bBodyVerticesExact,
			"hair style %u changed common body vertex-field offset %u", style, uFirstBadVertex);
		u_int uFirstBadIndex = 0xFFFFFFFFu;
		const bool bBodyIndicesExact = IndexRangesExactShifted(xFull, 0u,
			xReferenceFull, 0u, uReferenceBodyIndices, 0u, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(bBodyIndicesExact,
			"hair style %u changed common body index offset %u", style, uFirstBadIndex);

		const bool bHairVerticesExact = VertexRangesExact(xFull, uBodyVerts,
			xHair, 0u, xHair.GetNumVerts(), uFirstBadVertex);
		ZENITH_ASSERT_TRUE(bHairVerticesExact,
			"hair style %u full/direct suffix vertex-field offset %u differs",
			style, uFirstBadVertex);
		const bool bHairIndicesExact = IndexRangesExactShifted(xFull, uBodyIndices,
			xHair, 0u, xHair.m_xIndices.GetSize(), uBodyVerts, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(bHairIndicesExact,
			"hair style %u full/direct shifted index offset %u differs",
			style, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(xFull, xHair),
			"hair style %u full/direct shared bones differ", style);
	}

	u_int uCollisionA = 0xFFFFFFFFu;
	u_int uCollisionB = 0xFFFFFFFFu;
	const bool bHairOnlyDistinct = MeshesPairwiseDistinct(axHairOnly,
		uZM_HUMAN_HAIR_STYLE_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bHairOnlyDistinct,
		"hair-only styles %u/%u collapse to one geometry", uCollisionA, uCollisionB);
	const bool bFullDistinct = MeshesPairwiseDistinct(axFull,
		uZM_HUMAN_HAIR_STYLE_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bFullDistinct,
		"complete meshes for hair styles %u/%u collapse to one silhouette", uCollisionA, uCollisionB);
	for (u_int a = 0u; a < uZM_HUMAN_HAIR_STYLE_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < uZM_HUMAN_HAIR_STYLE_COUNT; ++b)
		{
			ZENITH_ASSERT_NE(auContentHashes[a], auContentHashes[b],
				"hair styles %u/%u collapse to one complete content hash", a, b);
		}
	}
}

// ############################################################################
// 14. SC3 attachment geometry silhouettes
// ############################################################################

// NONE is a true no-op. Every non-NONE attachment appends valid atlas-mapped,
// shared-rig-skinned geometry, remains deterministic, and is pairwise distinct in
// both its isolated mesh and the complete human silhouette. The roster pass then
// proves each selected attachment remains observable when only that axis is reset.
ZENITH_TEST(ZM_Gen, HumanGen_AttachmentSilhouettes)
{
	static_assert(ZM_HUMAN_ATTACHMENT_COUNT == 6u,
		"SC3 freezes NONE plus five human attachment silhouettes");
	ZM_GenMesh xShared;
	ZM_AppendSharedHumanBones(xShared);
	const int iHead = ZM_GenMeshFindBone(xShared, "Head");
	const int iSpine = ZM_GenMeshFindBone(xShared, "Spine");
	ZENITH_ASSERT_GE(iHead, 0, "shared human rig must expose the Head bone");
	ZENITH_ASSERT_GE(iSpine, 0, "shared human rig must expose the Spine bone");
	ZM_GenMesh axAttachmentOnly[ZM_HUMAN_ATTACHMENT_COUNT];
	ZM_GenMesh axFull[ZM_HUMAN_ATTACHMENT_COUNT];
	ZM_GenImage axAlbedo[ZM_HUMAN_ATTACHMENT_COUNT];
	u_int auContentHashes[ZM_HUMAN_ATTACHMENT_COUNT] = {};

	for (u_int attachment = 0u; attachment < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++attachment)
	{
		ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe(ZM_HUMAN_PLAYER_M);
		xRecipe.m_uHairStyle = 0u;
		xRecipe.m_eAttachment = (ZM_HUMAN_ATTACHMENT)attachment;
		axAlbedo[attachment] = ZM_BuildHumanAlbedo(xRecipe);
		const ZM_GenImage xAlbedoAgain = ZM_BuildHumanAlbedo(xRecipe);
		ZENITH_ASSERT_TRUE(axAlbedo[attachment].Equals(xAlbedoAgain),
			"attachment %u albedo is not byte-deterministic", attachment);
		ZENITH_ASSERT_EQ(axAlbedo[attachment].ContentHash(), xAlbedoAgain.ContentHash(),
			"attachment %u albedo hash is not deterministic", attachment);

		int iExpectedRigidBone = -1;
		switch (xRecipe.m_eAttachment)
		{
		case ZM_HUMAN_ATTACHMENT_NONE:                         break;
		case ZM_HUMAN_ATTACHMENT_CAP:
		case ZM_HUMAN_ATTACHMENT_HAT:
		case ZM_HUMAN_ATTACHMENT_GLASSES: iExpectedRigidBone = iHead;  break;
		case ZM_HUMAN_ATTACHMENT_BACKPACK:
		case ZM_HUMAN_ATTACHMENT_SATCHEL: iExpectedRigidBone = iSpine; break;
		default: break;
		}

		ZM_GenMesh& xAttachmentA = axAttachmentOnly[attachment];
		ZM_AppendSharedHumanBones(xAttachmentA);
		const ZM_GenMesh xBeforeAppend = xAttachmentA;
		const u_int uFirstAttachmentVert = xAttachmentA.GetNumVerts();
		ZM_AppendHumanAttachment(xRecipe, xAttachmentA);

		if (xRecipe.m_eAttachment == ZM_HUMAN_ATTACHMENT_NONE)
		{
			ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xAttachmentA, xBeforeAppend),
				"NONE attachment must be a byte-identical no-op");
		}
		else
		{
			ZENITH_ASSERT_GE(iExpectedRigidBone, 0,
				"attachment %u has no required shared rigid-bone mapping", attachment);
			u_int uFirstBadIndex = 0xFFFFFFFFu;
			const bool bSafeToFinalise = MeshSafeForFinalization(xAttachmentA, uFirstBadIndex);
			ZENITH_ASSERT_TRUE(bSafeToFinalise,
				"attachment %u is unsafe to finalise (bad index slot %u)",
				attachment, uFirstBadIndex);
			if (bSafeToFinalise)
			{
				ZM_GenGenerateTangents(xAttachmentA);
				ZM_GenNormalizeSkinWeights(xAttachmentA);
			}

			ZM_GenMesh xAttachmentB;
			ZM_AppendSharedHumanBones(xAttachmentB);
			ZM_AppendHumanAttachment(xRecipe, xAttachmentB);
			u_int uSecondBadIndex = 0xFFFFFFFFu;
			const bool bSecondSafeToFinalise = MeshSafeForFinalization(xAttachmentB, uSecondBadIndex);
			ZENITH_ASSERT_TRUE(bSecondSafeToFinalise,
				"second attachment %u build is unsafe to finalise (bad index slot %u)",
				attachment, uSecondBadIndex);
			if (bSecondSafeToFinalise)
			{
				ZM_GenGenerateTangents(xAttachmentB);
				ZM_GenNormalizeSkinWeights(xAttachmentB);
			}

			ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(xAttachmentA, xAttachmentB),
				"attachment %u appender is not byte-deterministic", attachment);
			ZENITH_ASSERT_GT(xAttachmentA.GetNumVerts(), uFirstAttachmentVert,
				"attachment %u appended no silhouette vertices", attachment);
			ZENITH_ASSERT_GT(xAttachmentA.GetNumTris(), 0u,
				"attachment %u appended no complete triangles", attachment);
			ZENITH_ASSERT_EQ(xAttachmentA.GetNumBones(), uZM_HUMAN_BONE_COUNT,
				"attachment %u changed the shared bone count", attachment);
			ZENITH_ASSERT_TRUE(MeshBonesEqual(xAttachmentA, xShared),
				"attachment %u changed shared bone data", attachment);
			ZENITH_ASSERT_TRUE(BoundsFiniteAndSane(xAttachmentA),
				"attachment %u has non-finite, degenerate, or unreasonable bounds", attachment);

			u_int uValidationBadIndex = 0xFFFFFFFFu;
			const bool bSafeToValidate = MeshSafeForValidation(xAttachmentA, uValidationBadIndex);
			ZENITH_ASSERT_TRUE(bSafeToValidate,
				"attachment %u is structurally incomplete (bad index slot %u)",
				attachment, uValidationBadIndex);
			if (bSafeToValidate)
			{
				const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(
					xAttachmentA, uZM_HUMAN_BONE_COUNT);
				ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward,
					"attachment %u has inward winding (bad tri %u)",
					attachment, xVal.m_uFirstBadTriangle);
				ZENITH_ASSERT_TRUE(xVal.m_bBoundsNonDegen,
					"attachment %u has degenerate bounds", attachment);
				ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne,
					"attachment %u has unnormalised weights (bad vert %u)",
					attachment, xVal.m_uFirstBadVertex);
				ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo,
					"attachment %u exceeds the two-influence contract", attachment);
				ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap,
					"attachment %u exceeds the shared 16-bone cap", attachment);

				if (iExpectedRigidBone >= 0)
				{
					u_int uFirstBadVertex = 0xFFFFFFFFu;
					ZENITH_ASSERT_TRUE(VertexRangeValid(xAttachmentA, uFirstAttachmentVert,
						xAttachmentA.GetNumVerts(), iExpectedRigidBone,
						&xZM_HUMAN_UV_ATTACHMENT, uFirstBadVertex),
						"attachment %u vertex %u violates finite/atlas/rigid-bone invariants",
						attachment, uFirstBadVertex);
				}
			}
		}

		ZM_BuildHumanMesh(xRecipe, axFull[attachment]);
		ZM_GenMesh xFullAgain;
		ZM_BuildHumanMesh(xRecipe, xFullAgain);
		ZENITH_ASSERT_TRUE(ZM_HumanMeshEqual(axFull[attachment], xFullAgain),
			"complete mesh for attachment %u is not deterministic", attachment);
		ZENITH_ASSERT_EQ(axFull[attachment].GetNumBones(), uZM_HUMAN_BONE_COUNT,
			"complete mesh for attachment %u changed the shared bone count", attachment);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(axFull[attachment], xShared),
			"complete mesh for attachment %u changed shared bone data", attachment);
		ZENITH_ASSERT_TRUE(BoundsFiniteAndSane(axFull[attachment]),
			"complete mesh for attachment %u has invalid bounds", attachment);
		u_int uFullBadIndex = 0xFFFFFFFFu;
		const bool bFullSafe = MeshSafeForValidation(axFull[attachment], uFullBadIndex);
		ZENITH_ASSERT_TRUE(bFullSafe,
			"complete mesh for attachment %u is structurally incomplete (bad index slot %u)",
			attachment, uFullBadIndex);
		if (bFullSafe)
		{
			const ZM_GenMeshValidation xFullVal = ZM_ValidateGenMesh(
				axFull[attachment], uZM_HUMAN_BONE_COUNT);
			ZENITH_ASSERT_TRUE(xFullVal.m_bWindingOutward,
				"complete mesh for attachment %u has inward winding", attachment);
			ZENITH_ASSERT_TRUE(xFullVal.m_bBoundsNonDegen,
				"complete mesh for attachment %u has degenerate bounds", attachment);
			ZENITH_ASSERT_TRUE(xFullVal.m_bWeightsSumToOne,
				"complete mesh for attachment %u has unnormalised weights", attachment);
			ZENITH_ASSERT_TRUE(xFullVal.m_bWeightsAtMostTwo,
				"complete mesh for attachment %u exceeds two influences", attachment);
			ZENITH_ASSERT_TRUE(xFullVal.m_bBonesWithinCap,
				"complete mesh for attachment %u exceeds the shared bone cap", attachment);
		}

		ZM_Human xHuman;
		BuildHumanFromRecipe(xRecipe, xHuman);
		auContentHashes[attachment] = ZM_HumanContentHash(xHuman);
	}

	const ZM_GenMesh& xNoneFull = axFull[ZM_HUMAN_ATTACHMENT_NONE];
	for (u_int attachment = (u_int)ZM_HUMAN_ATTACHMENT_NONE + 1u;
		attachment < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++attachment)
	{
		const ZM_GenMesh& xDirectAttachment = axAttachmentOnly[attachment];
		ZENITH_ASSERT_GT(axFull[attachment].GetNumVerts(), xNoneFull.GetNumVerts(),
			"attachment %u did not add vertices to the complete silhouette", attachment);
		ZENITH_ASSERT_GT(axFull[attachment].GetNumTris(), xNoneFull.GetNumTris(),
			"attachment %u did not add triangles to the complete silhouette", attachment);
		ZENITH_ASSERT_FALSE(ZM_HumanMeshEqual(axFull[attachment], xNoneFull),
			"attachment %u complete mesh equals NONE", attachment);

		const u_int64 ulExpectedVerts = (u_int64)xNoneFull.GetNumVerts()
			+ (u_int64)xDirectAttachment.GetNumVerts();
		const u_int64 ulExpectedIndices = (u_int64)xNoneFull.m_xIndices.GetSize()
			+ (u_int64)xDirectAttachment.m_xIndices.GetSize();
		ZENITH_ASSERT_EQ((u_int64)axFull[attachment].GetNumVerts(), ulExpectedVerts,
			"attachment %u full mesh is not NONE-prefix plus direct-suffix vertices", attachment);
		ZENITH_ASSERT_EQ((u_int64)axFull[attachment].m_xIndices.GetSize(), ulExpectedIndices,
			"attachment %u full mesh is not NONE-prefix plus direct-suffix indices", attachment);

		u_int uFirstBadVertex = 0xFFFFFFFFu;
		const bool bPrefixVerticesExact = VertexRangesExact(axFull[attachment], 0u,
			xNoneFull, 0u, xNoneFull.GetNumVerts(), uFirstBadVertex);
		ZENITH_ASSERT_TRUE(bPrefixVerticesExact,
			"attachment %u changed NONE-prefix vertex-field offset %u", attachment, uFirstBadVertex);
		u_int uFirstBadIndex = 0xFFFFFFFFu;
		const bool bPrefixIndicesExact = IndexRangesExactShifted(axFull[attachment], 0u,
			xNoneFull, 0u, xNoneFull.m_xIndices.GetSize(), 0u, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(bPrefixIndicesExact,
			"attachment %u changed NONE-prefix index offset %u", attachment, uFirstBadIndex);

		const bool bSuffixVerticesExact = VertexRangesExact(axFull[attachment],
			xNoneFull.GetNumVerts(), xDirectAttachment, 0u,
			xDirectAttachment.GetNumVerts(), uFirstBadVertex);
		ZENITH_ASSERT_TRUE(bSuffixVerticesExact,
			"attachment %u full/direct suffix vertex-field offset %u differs",
			attachment, uFirstBadVertex);
		const bool bSuffixIndicesExact = IndexRangesExactShifted(axFull[attachment],
			xNoneFull.m_xIndices.GetSize(), xDirectAttachment, 0u,
			xDirectAttachment.m_xIndices.GetSize(), xNoneFull.GetNumVerts(), uFirstBadIndex);
		ZENITH_ASSERT_TRUE(bSuffixIndicesExact,
			"attachment %u full/direct shifted index offset %u differs",
			attachment, uFirstBadIndex);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(axFull[attachment], xNoneFull)
			&& MeshBonesEqual(axFull[attachment], xDirectAttachment),
			"attachment %u prefix/suffix shared bones differ", attachment);
	}

	// Material coverage stays semantic: compare only the shared attachment-island
	// painted core. NONE is the background control; no pixel value or hash is gold.
	for (u_int attachment = (u_int)ZM_HUMAN_ATTACHMENT_NONE + 1u;
		attachment < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++attachment)
	{
		u_int uPixelsCompared = 0u;
		u_int uFirstDiffX = 0xFFFFFFFFu;
		u_int uFirstDiffY = 0xFFFFFFFFu;
		const bool bCoreEqualsBackground = ImagesEqualInIslandCore(axAlbedo[attachment],
			axAlbedo[ZM_HUMAN_ATTACHMENT_NONE], xZM_HUMAN_UV_ATTACHMENT,
			uPixelsCompared, uFirstDiffX, uFirstDiffY);
		ZENITH_ASSERT_GT(uPixelsCompared, 0u,
			"attachment %u has no valid attachment-island core pixels", attachment);
		if (uPixelsCompared > 0u)
		{
			ZENITH_ASSERT_FALSE(bCoreEqualsBackground,
				"attachment %u core stayed equal to NONE background", attachment);
		}
	}
	for (u_int a = (u_int)ZM_HUMAN_ATTACHMENT_NONE + 1u;
		a < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++b)
		{
			u_int uPixelsCompared = 0u;
			u_int uFirstDiffX = 0xFFFFFFFFu;
			u_int uFirstDiffY = 0xFFFFFFFFu;
			const bool bCoresEqual = ImagesEqualInIslandCore(axAlbedo[a], axAlbedo[b],
				xZM_HUMAN_UV_ATTACHMENT, uPixelsCompared, uFirstDiffX, uFirstDiffY);
			ZENITH_ASSERT_GT(uPixelsCompared, 0u,
				"attachments %u/%u have no valid core pixels to compare", a, b);
			if (uPixelsCompared > 0u)
			{
				ZENITH_ASSERT_FALSE(bCoresEqual,
					"attachments %u/%u paint an identical attachment-island core", a, b);
			}
		}
	}

	u_int uCollisionA = 0xFFFFFFFFu;
	u_int uCollisionB = 0xFFFFFFFFu;
	const bool bAttachmentOnlyDistinct = MeshesPairwiseDistinct(axAttachmentOnly,
		(u_int)ZM_HUMAN_ATTACHMENT_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bAttachmentOnlyDistinct,
		"isolated attachments %u/%u collapse to one geometry", uCollisionA, uCollisionB);
	const bool bFullDistinct = MeshesPairwiseDistinct(axFull,
		(u_int)ZM_HUMAN_ATTACHMENT_COUNT, uCollisionA, uCollisionB);
	ZENITH_ASSERT_TRUE(bFullDistinct,
		"complete attachment meshes %u/%u collapse to one silhouette", uCollisionA, uCollisionB);
	for (u_int a = 0u; a < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_HUMAN_ATTACHMENT_COUNT; ++b)
		{
			ZENITH_ASSERT_NE(auContentHashes[a], auContentHashes[b],
				"attachments %u/%u collapse to one complete content hash", a, b);
		}
	}

	u_int uRosterAttachments = 0u;
	for (u_int id = 0u; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HumanRecipe xSelectedRecipe = ZM_ResolveHumanRecipe((ZM_HUMAN_ID)id);
		if (xSelectedRecipe.m_eAttachment == ZM_HUMAN_ATTACHMENT_NONE) { continue; }
		ZM_HumanRecipe xNoneRecipe = xSelectedRecipe;
		xNoneRecipe.m_eAttachment = ZM_HUMAN_ATTACHMENT_NONE;
		ZM_GenMesh xSelected;
		ZM_GenMesh xNone;
		ZM_BuildHumanMesh(xSelectedRecipe, xSelected);
		ZM_BuildHumanMesh(xNoneRecipe, xNone);
		ZENITH_ASSERT_GT(xSelected.GetNumVerts(), xNone.GetNumVerts(),
			"human %u selected attachment added no vertices", id);
		ZENITH_ASSERT_GT(xSelected.GetNumTris(), xNone.GetNumTris(),
			"human %u selected attachment added no triangles", id);
		ZENITH_ASSERT_FALSE(ZM_HumanMeshEqual(xSelected, xNone),
			"human %u selected attachment is invisible to its mesh", id);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(xSelected, xNone),
			"human %u selected attachment changed the shared rig", id);
		++uRosterAttachments;
	}
	ZENITH_ASSERT_GT(uRosterAttachments, 0u,
		"human roster contains no non-NONE attachment selection to exercise");
}

// ############################################################################
// 15. SC4 clip channels bind the one exact shared skeleton
// ############################################################################

// Every shared clip is non-empty, uses only exact names from the fixed 16-bone
// rig, and authors rotation channels only. Absence of position/scale channels is
// the load-bearing proof that the animation player preserves each model's shared
// bind translation and scale. The semantic minimums describe motion roles, not
// implementation details: exact channel counts and curve values remain free.
ZENITH_TEST(ZM_Gen, HumanGen_ClipChannelsMatchSharedSkeleton)
{
	ZM_GenMesh xSharedSkeleton;
	ZM_AppendSharedHumanBones(xSharedSkeleton);
	ZENITH_ASSERT_EQ(xSharedSkeleton.GetNumBones(), uZM_HUMAN_BONE_COUNT,
		"the SC4 channel gate requires the complete shared human skeleton");

	for (u_int c = 0u; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
	{
		const ZM_HUMAN_ANIM_CLIP eClip = (ZM_HUMAN_ANIM_CLIP)c;
		Flux_AnimationClip xClip;
		ZM_BuildHumanClip(eClip, xClip);

		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels =
			xClip.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u,
			"human clip %u has no authored bone channels", c);

		const ZM_CreatureClipValidation xValidation = ZM_ValidateCreatureClip(
			xClip, xSharedSkeleton, ZM_HumanClipLooping(eClip));
		ZENITH_ASSERT_TRUE(xValidation.m_bAllValid,
			"human clip %u failed the shared clip validator (first bad bone '%s')",
			c, xValidation.m_szFirstBadBone);

		const Flux_RootMotion& xRootMotion = xClip.GetRootMotion();
		ZENITH_ASSERT_FALSE(xRootMotion.m_bEnabled,
			"human clip %u must not enable root motion", c);
		ZENITH_ASSERT_EQ(xRootMotion.m_xPositionDeltas.GetSize(), 0u,
			"human clip %u must not author root-position deltas", c);
		ZENITH_ASSERT_EQ(xRootMotion.m_xRotationDeltas.GetSize(), 0u,
			"human clip %u must not author root-rotation deltas", c);
		ZENITH_ASSERT_EQ(xClip.GetEvents().GetSize(), 0u,
			"human clip %u must not acquire per-model animation events", c);
		ZENITH_ASSERT_TRUE(xClip.GetSourcePath().empty(),
			"fresh in-memory human clip %u must have no source path", c);

		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const std::string& strBone = xChannel.GetBoneName();
			const int iBone = ZM_GenMeshFindBone(xSharedSkeleton, strBone.c_str());
			ZENITH_ASSERT_GE(iBone, 0,
				"human clip %u channel '%s' is not an exact shared bone name",
				c, strBone.c_str());
			if (iBone >= 0 && (u_int)iBone < uZM_HUMAN_BONE_COUNT)
			{
				ZENITH_ASSERT_STREQ(strBone.c_str(), g_aszSharedBones[iBone],
					"human clip %u channel '%s' resolved outside the canonical name order",
					c, strBone.c_str());
			}

			ZENITH_ASSERT_TRUE(xChannel.HasRotationKeyframes(),
				"human clip %u channel '%s' has no rotation keys", c, strBone.c_str());
			ZENITH_ASSERT_FALSE(xChannel.HasPositionKeyframes(),
				"human clip %u channel '%s' would overwrite bind translation",
				c, strBone.c_str());
			ZENITH_ASSERT_EQ(xChannel.GetPositionKeyframes().GetSize(), 0u,
				"human clip %u channel '%s' carries hidden position keys",
				c, strBone.c_str());
			ZENITH_ASSERT_FALSE(xChannel.HasScaleKeyframes(),
				"human clip %u channel '%s' would overwrite bind scale",
				c, strBone.c_str());
			ZENITH_ASSERT_EQ(xChannel.GetScaleKeyframes().GetSize(), 0u,
				"human clip %u channel '%s' carries hidden scale keys",
				c, strBone.c_str());
		}

		const char* szMissing = nullptr;
		const bool bSemanticChannels = HumanClipHasSemanticChannels(eClip, xClip, szMissing);
		ZENITH_ASSERT_TRUE(bSemanticChannels,
			"human clip %u lacks semantic motion coverage: %s", c,
			szMissing != nullptr ? szMissing : "unknown requirement");
	}
}

// ############################################################################
// 16. SC4 key timing, loop seams, and one-shot end policy
// ############################################################################

// Times are ticks, duration is seconds, and every channel explicitly spans the
// complete clip. Loops close orientation at the seam. Wave/Point/Cheer/Hurt
// recover to identity; Faint instead authors a final held downed pose.
ZENITH_TEST(ZM_Gen, HumanGen_ClipTimingAndPlaybackPolicy)
{
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();

	for (u_int c = 0u; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
	{
		const ZM_HUMAN_ANIM_CLIP eClip = (ZM_HUMAN_ANIM_CLIP)c;
		Flux_AnimationClip xClip;
		ZM_BuildHumanClip(eClip, xClip);

		const float fDurationTicks = xClip.GetDurationInTicks();
		const bool bDurationValid = std::isfinite(xClip.GetDuration())
			&& xClip.GetDuration() > 0.0f
			&& std::isfinite(fDurationTicks)
			&& fDurationTicks > 0.0f;
		ZENITH_ASSERT_TRUE(bDurationValid,
			"human clip %u has no finite positive duration", c);
		ZENITH_ASSERT_TRUE(xClip.IsLooping() == ZM_HumanClipLooping(eClip),
			"human clip %u built looping policy differs from its metadata", c);

		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels =
			xClip.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u,
			"human clip %u has no channels to exercise timing policy", c);
		ZENITH_ASSERT_TRUE(HumanClipHasTemporalMotion(xClip),
			"human clip %u contains no temporal rotation change", c);
		if (!bDurationValid || xChannels.GetSize() == 0u)
		{
			continue;
		}

		u_int uFaintHeldDifferent = 0u;
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const char* szBone = xChannel.GetBoneName().c_str();
			u_int uFirstBadKey = 0xFFFFFFFFu;
			const bool bKeysWellFormed = HumanRotationKeysWellFormed(
				xChannel, fDurationTicks, uFirstBadKey);
			ZENITH_ASSERT_TRUE(bKeysWellFormed,
				"human clip %u channel '%s' has an invalid/duplicate/out-of-range key %u",
				c, szBone, uFirstBadKey);
			if (!bKeysWellFormed)
			{
				continue;
			}

			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys =
				xChannel.GetRotationKeyframes();
			ZENITH_ASSERT_LE(fabsf(xKeys.GetFront().second), fHUMAN_ANIM_TICK_TOL,
				"human clip %u channel '%s' has no first key at tick zero", c, szBone);
			ZENITH_ASSERT_LE(fabsf(xKeys.GetBack().second - fDurationTicks),
				fHUMAN_ANIM_TICK_TOL,
				"human clip %u channel '%s' has no final key at duration", c, szBone);

			const Zenith_Maths::Quat& xFirst = xKeys.GetFront().first;
			const Zenith_Maths::Quat& xLast = xKeys.GetBack().first;
			if (ZM_HumanClipLooping(eClip))
			{
				ZENITH_ASSERT_GE(HumanQuatAbsDot(xFirst, xLast),
					fHUMAN_ANIM_ROT_CLOSE_DOT,
					"looping human clip %u channel '%s' does not close at the seam",
					c, szBone);
				continue;
			}

			const Zenith_Maths::Quat xAtEnd = xChannel.SampleRotation(fDurationTicks);
			const Zenith_Maths::Quat xPastEnd = xChannel.SampleRotation(fDurationTicks * 2.0f);
			ZENITH_ASSERT_TRUE(HumanQuatFiniteNormalized(xAtEnd)
				&& HumanQuatFiniteNormalized(xPastEnd),
				"one-shot human clip %u channel '%s' end/past-end sample is invalid",
				c, szBone);
			ZENITH_ASSERT_GE(HumanQuatAbsDot(xAtEnd, xPastEnd),
				fHUMAN_ANIM_ROT_CLOSE_DOT,
				"one-shot human clip %u channel '%s' does not clamp past its end",
				c, szBone);

			if (eClip != ZM_HUMAN_CLIP_FAINT)
			{
				ZENITH_ASSERT_GE(HumanQuatAbsDot(xAtEnd, xIdentity),
					fHUMAN_ANIM_ROT_CLOSE_DOT,
					"one-shot human clip %u channel '%s' does not recover to identity",
					c, szBone);
				continue;
			}

			const u_int uPenultimateKey = xKeys.GetSize() - 2u;
			const Zenith_Maths::Quat& xPenultimate = xKeys.Get(uPenultimateKey).first;
			const float fPenultimateTick = xKeys.Get(uPenultimateKey).second;
			ZENITH_ASSERT_LT(fPenultimateTick, fDurationTicks,
				"Faint channel '%s' penultimate key must precede duration", szBone);
			ZENITH_ASSERT_GE(HumanQuatAbsDot(xPenultimate, xLast),
				fHUMAN_ANIM_ROT_CLOSE_DOT,
				"Faint channel '%s' penultimate/final authored keys do not hold one pose",
				szBone);

			const Zenith_Maths::Quat xAtZero = xChannel.SampleRotation(0.0f);
			ZENITH_ASSERT_TRUE(HumanQuatFiniteNormalized(xAtZero),
				"Faint channel '%s' tick-zero sample is invalid", szBone);
			if (HumanQuatAbsDot(xAtZero, xAtEnd) < fHUMAN_ANIM_ROT_MOTION_DOT)
			{
				++uFaintHeldDifferent;
			}
		}

		if (eClip == ZM_HUMAN_CLIP_FAINT)
		{
			ZENITH_ASSERT_GT(uFaintHeldDifferent, 0u,
				"Faint final held pose matches tick zero on every channel");
		}
	}
}

// ############################################################################
// 17. SC4 byte determinism and implementation-independent motion sensitivity
// ############################################################################

// Equality/hash reuse the established serialized-clip helpers. Pairwise motion
// comparison normalizes each clip's own duration and ignores metadata, so merely
// changing Name/Duration cannot disguise two collapsed motion implementations.
ZENITH_TEST(ZM_Gen, HumanGen_ClipDeterminismAndSensitivity)
{
	Flux_AnimationClip axClips[ZM_HUMAN_CLIP_COUNT];
	u_int auHashes[ZM_HUMAN_CLIP_COUNT] = {};
	for (u_int c = 0u; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
	{
		const ZM_HUMAN_ANIM_CLIP eClip = (ZM_HUMAN_ANIM_CLIP)c;
		ZM_BuildHumanClip(eClip, axClips[c]);
		Flux_AnimationClip xAgain;
		ZM_BuildHumanClip(eClip, xAgain);

		ZENITH_ASSERT_GT(axClips[c].GetBoneChannels().GetSize(), 0u,
			"human clip %u determinism would be vacuous with no channels", c);
		ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(axClips[c], xAgain),
			"human clip %u is not byte-identical on a repeat build", c);
		auHashes[c] = ZM_CreatureClipContentHash(axClips[c]);
		ZENITH_ASSERT_EQ(auHashes[c], ZM_CreatureClipContentHash(xAgain),
			"human clip %u content hash changes on a repeat build", c);
	}

	for (u_int a = 0u; a < (u_int)ZM_HUMAN_CLIP_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_HUMAN_CLIP_COUNT; ++b)
		{
			ZENITH_ASSERT_FALSE(HumanClipMotionEquivalentNormalized(axClips[a], axClips[b]),
				"human clips %u/%u collapse to the same normalized rotation motion", a, b);
			ZENITH_ASSERT_NE(auHashes[a], auHashes[b],
				"human clips %u/%u share a serialized content hash", a, b);
		}
	}
}

// ############################################################################
// 18. SC4 clips are one shared, model-independent set across the whole roster
// ############################################################################

// Build the nine clips once, then validate that exact set against every model's
// unchanged shared rig. Rebuilding after each model proves no ambient per-ID
// state can perturb the no-ID clip builder.
ZENITH_TEST(ZM_Gen, HumanGen_ClipSetSharedAcrossRoster)
{
	ZM_GenMesh xSharedSkeleton;
	ZM_AppendSharedHumanBones(xSharedSkeleton);
	Flux_AnimationClip axSharedClips[ZM_HUMAN_CLIP_COUNT];
	for (u_int c = 0u; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
	{
		ZM_BuildHumanClip((ZM_HUMAN_ANIM_CLIP)c, axSharedClips[c]);
	}

	for (u_int id = 0u; id < (u_int)ZM_HUMAN_COUNT; ++id)
	{
		const ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe((ZM_HUMAN_ID)id);
		ZM_GenMesh xModelMesh;
		ZM_BuildHumanMesh(xRecipe, xModelMesh);
		ZENITH_ASSERT_TRUE(MeshBonesEqual(xModelMesh, xSharedSkeleton),
			"human %u no longer carries the exact shared skeleton", id);

		for (u_int c = 0u; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
		{
			const ZM_HUMAN_ANIM_CLIP eClip = (ZM_HUMAN_ANIM_CLIP)c;
			const ZM_CreatureClipValidation xValidation = ZM_ValidateCreatureClip(
				axSharedClips[c], xModelMesh, ZM_HumanClipLooping(eClip));
			ZENITH_ASSERT_TRUE(xValidation.m_bAllValid,
				"shared clip %u does not transfer to human %u (first bad bone '%s')",
				c, id, xValidation.m_szFirstBadBone);

			Flux_AnimationClip xAfterModelBuild;
			ZM_BuildHumanClip(eClip, xAfterModelBuild);
			ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(
				axSharedClips[c], xAfterModelBuild),
				"shared clip %u bytes depend on preceding human %u generation", c, id);
			ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(axSharedClips[c]),
				ZM_CreatureClipContentHash(xAfterModelBuild),
				"shared clip %u hash depends on preceding human %u generation", c, id);
		}
	}
}
