#include "Zenith.h"

// ============================================================================
// ZM_Tests_HumanGen -- S4 SC1/SC2 unit gate for ZM_HumanGen (suite ZM_Gen).
//
// These author against the FROZEN seam (Games/Zenithmon/Source/Gen/ZM_HumanGen.h
// + ZM_HumanData.h) only. SC1 asserts the roster/skeleton/recipe/asset-path
// contract:
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
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS reach. Runs at boot before
// the scene loads.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/Data/ZM_HumanData.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // strlen, strcmp
#include <cmath>     // fabsf

namespace
{
	constexpr float fHUMAN_WEIGHT_TOL = 1.0e-4f;
	constexpr float fHUMAN_INFLUENCE_EPS = 1.0e-6f;
	constexpr float fHUMAN_UNIT_TOL = 1.0e-3f;
	constexpr float fHUMAN_WORLD_BOX_LIMIT = 10.0f;

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

// Golden-locks the shared 9-clip set frozen at SC1: exact names, durations, and
// loop flags. The curves land in SC4, but the metadata is a fixed contract now.
ZENITH_TEST(ZM_Gen, HumanGen_ClipMetadataGolden)
{
	struct ClipGold { ZM_HUMAN_ANIM_CLIP m_eClip; const char* m_szName; float m_fDur; bool m_bLoop; };
	const ClipGold aGold[ZM_HUMAN_CLIP_COUNT] =
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

	for (u_int c = 0; c < (u_int)ZM_HUMAN_CLIP_COUNT; ++c)
	{
		const ClipGold& xG = aGold[c];
		ZENITH_ASSERT_STREQ(ZM_HumanClipName(xG.m_eClip), xG.m_szName, "clip %u name drifted", c);
		ZENITH_ASSERT_EQ(ZM_HumanClipDurationSeconds(xG.m_eClip), xG.m_fDur, "clip %u duration drifted", c);
		ZENITH_ASSERT_TRUE(ZM_HumanClipLooping(xG.m_eClip) == xG.m_bLoop, "clip %u loop flag drifted", c);
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
