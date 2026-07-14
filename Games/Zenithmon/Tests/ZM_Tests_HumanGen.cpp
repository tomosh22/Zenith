#include "Zenith.h"

// ============================================================================
// ZM_Tests_HumanGen -- S4 SC1 unit gate for ZM_HumanGen (suite ZM_Gen).
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
// (The fuller SameSeedDeterminism over ALL ids / structural cross-id invariants /
// bones-match-shared land in SC2 once the real humanoid loft replaces the SC1
// placeholder body.)
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
