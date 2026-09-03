//=============================================================================
// Zenith_Tools_BushAssetExport unit tests. Included from the bottom of
// Zenith_Tools_BushAssetExport.cpp (inside its ZENITH_TOOLS branch), so the
// anonymous-namespace helpers and the ONE variant table are in scope.
//
// Same discipline as the rock and deadwood suites: rebuild the production
// data in memory and assert a PROPERTY chosen so that a defect invisible in a
// render still fails here. The VAT chain adds failure modes the static sets
// never had — an unskinned vertex freezes while its neighbours sway, a clip
// channel naming a bone the skeleton does not have silently animates nothing,
// and an all-255 alpha mask turns the MASKED material into opaque black
// squares — so each of those gets a test.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	const int iBUSH_SHAPES = static_cast<int>(BUSH_VARIANT_COUNT);

	// Build one production variant exactly as the exporter does.
	void Bush_BuildVariant(Zenith_Vector<BushBranch>& xBranches, Zenith_Vector<BushCard>& xCards,
		int iVariant)
	{
		BuildBushVariant(xBranches, xCards, BushVariantAt(iVariant));
	}
}

//-----------------------------------------------------------------------------
// The emitted mesh.
//-----------------------------------------------------------------------------

ZENITH_TEST(BushAssets, EveryCardIsEmittedDoubleSided)
{
	// The instanced pipeline backface-culls, and a card is seen from both
	// sides, so the builder emits each one twice with opposite windings in a
	// FIXED order: of a card's four triangles, [0] pairs with [2] and [1]
	// with [3]. A card that lost its second winding vanishes from half of all
	// view angles — very visible in play, invisible to any count-based check.
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);

		ZENITH_ASSERT_GT(pxMesh->GetNumVerts(), 0u, "variant emitted no geometry");
		ZENITH_ASSERT_EQ(pxMesh->GetNumIndices() % 12u, 0u,
			"a card must emit exactly four triangles (both windings)");
		ZENITH_ASSERT_EQ(pxMesh->GetNumIndices() / 12u, xCards.GetSize(),
			"triangle count disagrees with the card count");

		auto FaceNormal = [&](u_int uTri) -> Zenith_Maths::Vector3
		{
			const Zenith_Maths::Vector3 xA = pxMesh->m_xPositions.Get(pxMesh->m_xIndices.Get(uTri));
			const Zenith_Maths::Vector3 xB = pxMesh->m_xPositions.Get(pxMesh->m_xIndices.Get(uTri + 1u));
			const Zenith_Maths::Vector3 xC = pxMesh->m_xPositions.Get(pxMesh->m_xIndices.Get(uTri + 2u));
			return glm::cross(xC - xA, xB - xA);
		};

		for (u_int uCard = 0; uCard < xCards.GetSize(); uCard++)
		{
			const u_int uBase = uCard * 12u;
			for (u_int uPair = 0; uPair < 2u; uPair++)
			{
				const Zenith_Maths::Vector3 xFront = FaceNormal(uBase + uPair * 3u);
				const Zenith_Maths::Vector3 xBack = FaceNormal(uBase + (uPair + 2u) * 3u);
				ZENITH_ASSERT_GT(glm::dot(xFront, xFront), 1.0e-10f, "degenerate card triangle");
				ZENITH_ASSERT_LT(glm::dot(glm::normalize(xFront), glm::normalize(xBack)), -0.999f,
					"a card's back-face triangle does not oppose its front face -- the card "
					"is not double-sided and will be culled from one side");
			}
		}
		delete pxMesh;
	}
}

ZENITH_TEST(BushAssets, CardNormalsAreUpBiasedUnitAndTangentOrthogonal)
{
	// The shading normal is deliberately blended toward +Y (soft foliage
	// lighting, the tree-leaves recipe): a card lit by its raw plane normal
	// flickers dark whenever its plane faces away from the sun. The card's
	// right vector is horizontal and the bias vertical, so the tangent must
	// remain exactly perpendicular to the biased normal.
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);

		for (u_int u = 0; u < pxMesh->GetNumVerts(); u++)
		{
			const Zenith_Maths::Vector3 xN = pxMesh->m_xNormals.Get(u);
			const Zenith_Maths::Vector3 xT = pxMesh->m_xTangents.Get(u);
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xN), 1.0f, 1e-3f, "normal is not unit length");
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xT), 1.0f, 1e-3f, "tangent is not unit length");
			ZENITH_ASSERT_LE(std::abs(glm::dot(xN, xT)), 1e-3f,
				"tangent is not perpendicular to its shading normal");
			ZENITH_ASSERT_GT(xN.y, 0.15f,
				"a card normal lost its up-bias -- undersides will light harshly");
		}
		delete pxMesh;
	}
}

ZENITH_TEST(BushAssets, EveryVertexIsFullySkinnedToALiveBranchBone)
{
	// ★ THE VAT DEFORMS ONLY WHAT IS SKINNED. An unskinned vertex (weight sum
	// 0, or a bone index off the end of the skeleton) bakes as a frozen point
	// while its card's neighbours sway -- very visible, very easy to ship,
	// and nothing else in the pipeline errors on it. Also pins that every
	// branch actually carries cards: a card-less branch is an invisible bone
	// the clip animates for nothing.
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_SkeletonAsset* pxSkel = CreateBushSkeleton(xBranches);
		Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);

		const u_int uNumBones = pxSkel->GetNumBones();
		ZENITH_ASSERT_EQ(uNumBones, xBranches.GetSize() + 1u, "skeleton is not root + one bone per branch");

		Zenith_Vector<u_int> xVertsPerBone;
		xVertsPerBone.Resize(uNumBones, 0u);
		for (u_int u = 0; u < pxMesh->GetNumVerts(); u++)
		{
			const glm::vec4 xWeights = pxMesh->m_xBoneWeights.Get(u);
			const glm::uvec4 xIndices = pxMesh->m_xBoneIndices.Get(u);
			ZENITH_ASSERT_EQ_FLOAT(xWeights.x + xWeights.y + xWeights.z + xWeights.w, 1.0f, 1e-4f,
				"skinning weights do not sum to 1 -- this vertex will not follow the sway");
			ZENITH_ASSERT_LT(xIndices.x, uNumBones, "bone index off the end of the skeleton");
			ZENITH_ASSERT_GE(xIndices.x, 1u,
				"a card vertex is bound to the root anchor instead of a branch bone");
			xVertsPerBone.Get(xIndices.x)++;
		}
		for (u_int uBone = 1; uBone < uNumBones; uBone++)
		{
			ZENITH_ASSERT_GE(xVertsPerBone.Get(uBone), 4u,
				"a branch bone carries no cards -- an invisible bone the clip animates for nothing");
		}
		delete pxMesh;
		delete pxSkel;
	}
}

//-----------------------------------------------------------------------------
// The sway clip and the VAT bake.
//-----------------------------------------------------------------------------

ZENITH_TEST(BushAssets, SwayClipDrivesEveryBranchAndClosesItsLoop)
{
	// A clip channel resolves to a skeleton bone BY NAME, and a miss animates
	// nothing without erroring. And the clip loops: an open loop pops once
	// every four seconds on every instance at once, which reads as a glitch
	// wave across the whole scatter.
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		const BushVariantSpec xSpec = BushVariantAt(iVariant);
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_SkeletonAsset* pxSkel = CreateBushSkeleton(xBranches);
		Flux_AnimationClip* pxClip = CreateBushSwayClip(xBranches, xSpec);

		ZENITH_ASSERT_EQ_FLOAT(pxClip->GetDuration(), 4.0f, 1e-5f, "sway clip duration moved");
		ZENITH_ASSERT_EQ(pxClip->GetTicksPerSecond(), 30u, "sway clip tick rate moved");
		ZENITH_ASSERT_TRUE(pxClip->IsLooping(), "the sway clip must loop");
		ZENITH_ASSERT_FALSE(pxClip->HasBoneChannel("BushRoot"),
			"the root anchor must not sway -- the whole bush would slide on its terrain seat");

		for (u_int uBranch = 0; uBranch < xBranches.GetSize(); uBranch++)
		{
			const BushBranch& xBranch = xBranches.Get(uBranch);
			ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel(xBranch.m_strBoneName),
				"a branch bone has no sway channel");
			const Flux_BoneChannel* pxChannel = pxClip->GetBoneChannel(xBranch.m_strBoneName);
			const auto& xKeys = pxChannel->GetRotationKeyframes();
			ZENITH_ASSERT_GE(xKeys.GetSize(), 2u, "a sway channel has no keyframes");
			ZENITH_ASSERT_EQ_FLOAT(xKeys.Get(0).second, 0.0f, 1e-4f,
				"the first keyframe is not at tick 0");
			// Keyframe times are TICKS: 4 s at 30 ticks/s spans 0..120. A clip
			// authored in seconds would end at tick 4 -- 3% of its duration.
			ZENITH_ASSERT_EQ_FLOAT(xKeys.Get(xKeys.GetSize() - 1).second, 120.0f, 1e-3f,
				"the last keyframe is not at tick 120 -- were the key times authored in seconds?");
			const Zenith_Maths::Quat xFirst = xKeys.Get(0).first;
			const Zenith_Maths::Quat xLast = xKeys.Get(xKeys.GetSize() - 1).first;
			ZENITH_ASSERT_GT(std::abs(glm::dot(xFirst, xLast)), 0.999999f,
				"the sway loop does not close -- every instance will pop in unison each cycle");
		}
		delete pxClip;
		delete pxSkel;
	}
}

ZENITH_TEST(BushAssets, TheVATBakesRealFramesForEveryVariant)
{
	// End-to-end through the production chain: skinned geometry -> bake. A
	// bake that silently produced zero frames (or dropped the clip) leaves a
	// bush that renders at bind pose forever; nothing downstream errors.
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		const BushVariantSpec xSpec = BushVariantAt(iVariant);
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_SkeletonAsset* pxSkel = CreateBushSkeleton(xBranches);
		Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);
		Flux_AnimationClip* pxClip = CreateBushSwayClip(xBranches, xSpec);

		Flux_MeshGeometry* pxGeometry = Zenith_Tools_CreateFluxMeshGeometry(pxMesh, pxSkel);
		Flux_AnimationTexture* pxVAT = new Flux_AnimationTexture();
		Zenith_Vector<Flux_AnimationClip*> axAnimations;
		axAnimations.PushBack(pxClip);

		ZENITH_ASSERT_TRUE(pxVAT->BakeFromAnimations(pxGeometry, pxSkel, axAnimations, 30),
			"the VAT bake failed outright");
		ZENITH_ASSERT_EQ(pxVAT->GetVertexCount(), pxMesh->GetNumVerts(),
			"the VAT does not cover every vertex");
		ZENITH_ASSERT_GT(pxVAT->GetTextureWidth(), 0u, "VAT width is zero");
		ZENITH_ASSERT_GE(pxVAT->GetTextureWidth(), pxVAT->GetVertexCount(),
			"VAT width cannot hold one texel per vertex");
		ZENITH_ASSERT_EQ(pxVAT->GetNumAnimations(), 1u, "the bake did not keep exactly the sway clip");
		ZENITH_ASSERT_GE(pxVAT->GetFramesPerAnimation(), 60u,
			"a 4 s clip at 30 fps baked implausibly few frames");
		ZENITH_ASSERT_EQ(pxVAT->GetTextureHeight(),
			pxVAT->GetNumAnimations() * pxVAT->GetFramesPerAnimation(),
			"VAT height disagrees with anims x frames");
		const Flux_AnimationTexture::AnimationInfo* pxInfo = pxVAT->GetAnimationInfo(0);
		ZENITH_ASSERT_TRUE(pxInfo != nullptr, "the baked clip has no info record");
		ZENITH_ASSERT_EQ_FLOAT(pxInfo->m_fDuration, 4.0f, 1e-3f,
			"the baked duration disagrees with the clip -- the component's "
			"SetAnimationDuration(4.0f) would scrub at the wrong rate");

		delete pxVAT;
		delete pxGeometry;
		delete pxClip;
		delete pxMesh;
		delete pxSkel;
	}
}

//-----------------------------------------------------------------------------
// Texture, tint, envelope, determinism.
//-----------------------------------------------------------------------------

ZENITH_TEST(BushAssets, FoliageAlbedoAlphaIsARealMask)
{
	// The material is MASKED with cutoff 0.45, which only means anything if
	// the albedo's alpha genuinely separates leaf from background. An all-255
	// alpha (the default for any painter that forgets the channel) renders
	// every card as an opaque square and no test of the MATERIAL would see it.
	Zenith_Vector<u_int8> xPixels;
	GenerateBushFoliagePixels(xPixels);
	ZENITH_ASSERT_EQ(xPixels.GetSize(),
		static_cast<u_int>(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE * 4), "pixel buffer size");

	u_int uTransparent = 0;
	u_int uOpaque = 0;
	u_int64 ulSumR = 0, ulSumG = 0, ulSumB = 0;
	const u_int uPixelCount = static_cast<u_int>(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE);
	for (u_int u = 0; u < uPixelCount; u++)
	{
		const u_int8 ucAlpha = xPixels.Get(u * 4 + 3);
		if (ucAlpha == 0) { uTransparent++; }
		if (ucAlpha >= 250)
		{
			uOpaque++;
			ulSumR += xPixels.Get(u * 4 + 0);
			ulSumG += xPixels.Get(u * 4 + 1);
			ulSumB += xPixels.Get(u * 4 + 2);
		}
	}
	const float fTransparentFrac = static_cast<float>(uTransparent) / uPixelCount;
	const float fOpaqueFrac = static_cast<float>(uOpaque) / uPixelCount;
	ZENITH_ASSERT_GE(fTransparentFrac, 0.10f,
		"almost nothing is fully transparent -- the alpha mask has collapsed toward opaque");
	ZENITH_ASSERT_GE(fOpaqueFrac, 0.10f, "almost nothing is solid leaf -- the cutoff would eat the card");
	ZENITH_ASSERT_LE(fOpaqueFrac, 0.85f, "the card is nearly all solid -- it will read as a square");

	// Foliage is green. A channel-order slip (BGRA vs RGBA) survives every
	// other assertion here and ships blue bushes.
	ZENITH_ASSERT_GT(uOpaque, 0u, "no opaque pixels to sample");
	ZENITH_ASSERT_GT(ulSumG, ulSumR, "opaque leaf pixels are not predominantly green (R >= G)");
	ZENITH_ASSERT_GT(ulSumG, ulSumB, "opaque leaf pixels are not predominantly green (B >= G)");
}

ZENITH_TEST(BushAssets, TintDarkensOnlyFromAnUnmodulatedBaseline)
{
	// Same contract as the rock and deadwood tints: the vertex colour
	// MULTIPLIES albedo, so alpha must be 1 (the shader ignores the tint
	// entirely otherwise) and the rim must stay at exactly 1.0 -- a tint that
	// dims everything is an albedo cut with extra steps.
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);

		float fMinTint = 2.0f;
		float fMaxTint = -1.0f;
		for (u_int u = 0; u < pxMesh->GetNumVerts(); u++)
		{
			const Zenith_Maths::Vector4 xColour = pxMesh->m_xColors.Get(u);
			ZENITH_ASSERT_GT(xColour.x, 0.0f, "vertex tint is not positive");
			ZENITH_ASSERT_LE(xColour.x, 1.0f, "vertex tint brightens past the authored albedo");
			ZENITH_ASSERT_EQ_FLOAT(xColour.x, xColour.y, 1e-5f, "tint is not neutral grey");
			ZENITH_ASSERT_EQ_FLOAT(xColour.x, xColour.z, 1e-5f, "tint is not neutral grey");
			ZENITH_ASSERT_EQ_FLOAT(xColour.w, 1.0f, 1e-5f,
				"vertex colour alpha must be 1 or the shader ignores the tint entirely");
			fMinTint = std::min(fMinTint, xColour.x);
			fMaxTint = std::max(fMaxTint, xColour.x);
		}
		ZENITH_ASSERT_GT(fMaxTint, 0.98f,
			"nothing is left unmodulated -- the tint is dimming the whole bush");
		ZENITH_ASSERT_LT(fMinTint, 0.92f, "the interior tint has no depth at all");
		delete pxMesh;
	}
}

ZENITH_TEST(BushAssets, HeightIsExactAndTheSilhouettesActuallyDiffer)
{
	// m_fHeightMetres has to MEAN metres EXACTLY -- the scatter's bounds
	// sphere and sink depth are hand-derived from it -- which is why the
	// builder rescales the whole cluster instead of leaving a band for a test
	// to shrug at. And the variants exist to be silhouettes: if broad and
	// spindly converge on the same aspect ratio, the table has drifted into
	// three copies of one bush.
	float afAspect[iBUSH_SHAPES] = {};
	for (int iVariant = 0; iVariant < iBUSH_SHAPES; iVariant++)
	{
		const BushVariantSpec xSpec = BushVariantAt(iVariant);
		Zenith_Vector<BushBranch> xBranches;
		Zenith_Vector<BushCard> xCards;
		Bush_BuildVariant(xBranches, xCards, iVariant);
		Zenith_MeshAsset* pxMesh = CreateBushFoliageMesh(xBranches, xCards);
		ZENITH_ASSERT_GT(pxMesh->GetNumVerts(), 0u, "variant emitted no geometry");

		float fMinY = 1.0e30f;
		float fMaxY = -1.0e30f;
		float fMaxRadial = 0.0f;
		for (u_int u = 0; u < pxMesh->GetNumVerts(); u++)
		{
			const Zenith_Maths::Vector3& xP = pxMesh->m_xPositions.Get(u);
			fMinY = std::min(fMinY, xP.y);
			fMaxY = std::max(fMaxY, xP.y);
			fMaxRadial = std::max(fMaxRadial, sqrtf(xP.x * xP.x + xP.z * xP.z));
		}
		ZENITH_ASSERT_EQ_FLOAT(fMaxY, xSpec.m_fHeightMetres, 0.02f,
			"the normalised height is not the authored height -- the scatter's bounds "
			"and sink values are derived from a lie");
		ZENITH_ASSERT_GE(fMinY, -0.12f, "foliage reaches deeper below the origin than the clamp allows");
		ZENITH_ASSERT_LE(fMinY, 0.30f, "the lowest foliage floats well clear of the ground seat");
		afAspect[iVariant] = fMaxRadial / std::max(0.05f, fMaxY);
		delete pxMesh;
	}
	ZENITH_ASSERT_GT(afAspect[BUSH_BROAD], afAspect[BUSH_SPINDLY] * 1.3f,
		"the broad bush is not meaningfully wider-per-height than the spindly one -- "
		"the variants have converged");
}

ZENITH_TEST(BushAssets, GenerationIsDeterministicForAFixedSeed)
{
	Zenith_Vector<BushBranch> xBranchesA, xBranchesB;
	Zenith_Vector<BushCard> xCardsA, xCardsB;
	Bush_BuildVariant(xBranchesA, xCardsA, BUSH_BROAD);
	Bush_BuildVariant(xBranchesB, xCardsB, BUSH_BROAD);
	Zenith_MeshAsset* pxFirst = CreateBushFoliageMesh(xBranchesA, xCardsA);
	Zenith_MeshAsset* pxSecond = CreateBushFoliageMesh(xBranchesB, xCardsB);

	ZENITH_ASSERT_EQ(pxFirst->GetNumVerts(), pxSecond->GetNumVerts(), "vertex count drifted between runs");
	ZENITH_ASSERT_EQ(pxFirst->GetNumIndices(), pxSecond->GetNumIndices(), "index count drifted between runs");
	for (u_int u = 0; u < pxFirst->GetNumVerts(); u++)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxFirst->m_xPositions.Get(u), pxSecond->m_xPositions.Get(u), 0.0f,
			"vertex position drifted between runs");
		ZENITH_ASSERT_NEAR_VEC3(pxFirst->m_xNormals.Get(u), pxSecond->m_xNormals.Get(u), 0.0f,
			"vertex normal drifted between runs");
	}
	delete pxSecond;
	delete pxFirst;
}

//-----------------------------------------------------------------------------
// The foliage card's new maps (STREAM D: normal + AO).
//
// Each of these covers a failure that renders as "fine, just flat":
//   * a normal map whose Z is not dominant lights foliage from the side,
//   * an all-white AO map is what an UNBOUND occlusion slot already does, so
//     binding one would be pure cost and look identical,
//   * a height field with no relief still produces a perfectly valid normal
//     map, and every "is it a unit vector" check passes on it.
//-----------------------------------------------------------------------------

namespace
{
	// 1024^2 x three maps: built once and shared, like the mesh helpers above.
	struct BushFoliageMaps
	{
		Zenith_Vector<u_int8> m_xPixels;
		Zenith_Vector<float>  m_xHeight;
		Zenith_Vector<float>  m_xAO;

		BushFoliageMaps() { GenerateBushFoliageMaps(m_xPixels, m_xHeight, m_xAO); }
	};

	const BushFoliageMaps& BushFoliageMapsOnce()
	{
		static const BushFoliageMaps ls_xMaps;
		return ls_xMaps;
	}
}

ZENITH_TEST(BushAssets, FoliageHeightIsADomePerLeafNotAFlatCard)
{
	const BushFoliageMaps& xMaps = BushFoliageMapsOnce();
	ZENITH_ASSERT_EQ(xMaps.m_xHeight.GetSize(),
		static_cast<u_int>(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE), "foliage height field size");

	float fMin = 2.0f;
	float fMax = -1.0f;
	u_int uLeafTexels = 0u;
	for (u_int u = 0; u < xMaps.m_xHeight.GetSize(); u++)
	{
		const float fH = xMaps.m_xHeight.Get(u);
		ZENITH_ASSERT_GE(fH, 0.0f, "foliage height below 0");
		ZENITH_ASSERT_LE(fH, 1.0f, "foliage height above 1");
		if (xMaps.m_xPixels.Get(u * 4 + 3) > 0u)
		{
			uLeafTexels++;
			fMin = std::min(fMin, fH);
			fMax = std::max(fMax, fH);
		}
	}
	ZENITH_ASSERT_GT(uLeafTexels, 0u, "the card painted no leaves at all");
	ZENITH_ASSERT_GT(fMax - fMin, 0.30f,
		"the foliage height field barely varies -- the per-leaf dome/layer relief is gone "
		"and the normal map it feeds will be flat");
}

ZENITH_TEST(BushAssets, FoliageHeightHasAMidribCrease)
{
	// A creased leaf has interior local minima along a horizontal scan; a smooth
	// dome does not. This is the difference between reading as a leaf and
	// reading as a pillow, and nothing else in the suite would notice its loss.
	const BushFoliageMaps& xMaps = BushFoliageMapsOnce();
	u_int uInteriorDips = 0u;
	for (int32_t iY = 1; iY < iBUSH_FOLIAGE_SIZE - 1; iY++)
	{
		for (int32_t iX = 1; iX < iBUSH_FOLIAGE_SIZE - 1; iX++)
		{
			const int32_t iIdx = iY * iBUSH_FOLIAGE_SIZE + iX;
			if (xMaps.m_xPixels.Get(iIdx * 4 + 3) < 250u ||
				xMaps.m_xPixels.Get((iIdx - 1) * 4 + 3) < 250u ||
				xMaps.m_xPixels.Get((iIdx + 1) * 4 + 3) < 250u)
			{
				continue;
			}
			const float fH = xMaps.m_xHeight.Get(iIdx);
			if (fH < xMaps.m_xHeight.Get(iIdx - 1) && fH < xMaps.m_xHeight.Get(iIdx + 1))
			{
				uInteriorDips++;
			}
		}
	}
	ZENITH_ASSERT_GT(uInteriorDips, 200u,
		"no interior height minima -- the midrib crease is missing and each leaf is a pillow");
}

ZENITH_TEST(BushAssets, FoliageNormalMapIsUnitLengthAndFacesOut)
{
	// The ENCODED texels, through the same encoder the .ztxtr gets.
	const BushFoliageMaps& xMaps = BushFoliageMapsOnce();
	Zenith_Vector<u_int8> xNormal;
	EncodeBushNormalMap(xMaps.m_xHeight, xNormal);
	ZENITH_ASSERT_EQ(xNormal.GetSize(),
		static_cast<u_int>(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE * 4), "normal map size");

	double dSumZ = 0.0;
	u_int uNonFlat = 0u;
	const u_int uTexels = static_cast<u_int>(iBUSH_FOLIAGE_SIZE * iBUSH_FOLIAGE_SIZE);
	for (u_int u = 0; u < uTexels; u++)
	{
		const float fX = xNormal.Get(u * 4 + 0) / 255.0f * 2.0f - 1.0f;
		const float fY = xNormal.Get(u * 4 + 1) / 255.0f * 2.0f - 1.0f;
		const float fZ = xNormal.Get(u * 4 + 2) / 255.0f * 2.0f - 1.0f;
		const float fLen = sqrtf(fX * fX + fY * fY + fZ * fZ);
		ZENITH_ASSERT_GT(fLen, 0.97f, "normal texel is far from unit length");
		ZENITH_ASSERT_LT(fLen, 1.03f, "normal texel is far from unit length");
		ZENITH_ASSERT_GT(fZ, 0.0f, "normal texel points INTO the surface (Z <= 0)");
		dSumZ += fZ;
		if (fX * fX + fY * fY > 0.02f)
		{
			uNonFlat++;
		}
	}
	const float fMeanZ = static_cast<float>(dSumZ / uTexels);
	ZENITH_ASSERT_GT(fMeanZ, 0.85f, "the foliage normal map is not predominantly +Z");
	ZENITH_ASSERT_GT(uNonFlat, uTexels / 200u,
		"almost every normal texel is exactly +Z -- the map carries no leaf relief");
}

ZENITH_TEST(BushAssets, FoliageAOIsInRangeAndActuallyDarkens)
{
	const BushFoliageMaps& xMaps = BushFoliageMapsOnce();
	double dSum = 0.0;
	double dLeafSum = 0.0;
	u_int uLeafTexels = 0u;
	for (u_int u = 0; u < xMaps.m_xAO.GetSize(); u++)
	{
		const float fAO = xMaps.m_xAO.Get(u);
		ZENITH_ASSERT_GE(fAO, 0.0f, "foliage AO below 0");
		ZENITH_ASSERT_LE(fAO, 1.0f, "foliage AO above 1");
		dSum += fAO;
		if (xMaps.m_xPixels.Get(u * 4 + 3) >= 250u)
		{
			dLeafSum += fAO;
			uLeafTexels++;
		}
	}
	ZENITH_ASSERT_GT(uLeafTexels, 0u, "no solid leaf texels to measure AO over");
	const float fLeafMean = static_cast<float>(dLeafSum / uLeafTexels);
	ZENITH_ASSERT_LT(fLeafMean, 0.97f, "foliage AO is effectively white -- binding it changes nothing");
	ZENITH_ASSERT_GT(fLeafMean, 0.40f, "foliage AO has collapsed toward black");
	const float fMean = static_cast<float>(dSum / xMaps.m_xAO.GetSize());
	ZENITH_ASSERT_GT(fMean, fLeafMean, "unpainted texels are not the most open ones");
}

ZENITH_TEST(BushAssets, FoliageAlbedoIsUnchangedByTheMapSplit)
{
	// GenerateBushFoliagePixels is now a wrapper over GenerateBushFoliageMaps.
	// The two must agree EXACTLY: the albedo bytes are pinned by the mask test
	// above, and a wrapper that drew from a different RNG position would move
	// every one of them while every other assertion here still passed.
	const BushFoliageMaps& xMaps = BushFoliageMapsOnce();
	Zenith_Vector<u_int8> xWrapped;
	GenerateBushFoliagePixels(xWrapped);
	ZENITH_ASSERT_EQ(xWrapped.GetSize(), xMaps.m_xPixels.GetSize(), "albedo size differs");
	for (u_int u = 0; u < xWrapped.GetSize(); u++)
	{
		if (xWrapped.Get(u) != xMaps.m_xPixels.Get(u))
		{
			ZENITH_ASSERT_EQ(xWrapped.Get(u), xMaps.m_xPixels.Get(u),
				"the albedo-only wrapper does not reproduce the full generator's pixels");
			break;
		}
	}
}

ZENITH_TEST(BushAssets, FoliageMapGenerationIsDeterministic)
{
	Zenith_Vector<u_int8> xPixelsA, xPixelsB;
	Zenith_Vector<float> xHeightA, xHeightB, xAOA, xAOB;
	GenerateBushFoliageMaps(xPixelsA, xHeightA, xAOA);
	GenerateBushFoliageMaps(xPixelsB, xHeightB, xAOB);
	for (u_int u = 0; u < xHeightA.GetSize(); u++)
	{
		if (xHeightA.Get(u) != xHeightB.Get(u) || xAOA.Get(u) != xAOB.Get(u))
		{
			ZENITH_ASSERT_EQ_FLOAT(xHeightA.Get(u), xHeightB.Get(u), 0.0f,
				"foliage height is not a pure function of its seed");
			ZENITH_ASSERT_EQ_FLOAT(xAOA.Get(u), xAOB.Get(u), 0.0f,
				"foliage AO is not a pure function of its seed");
			break;
		}
	}
}

#endif // ZENITH_TESTING
