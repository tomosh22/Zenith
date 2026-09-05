#include "UnitTests/Zenith_UnitTests.h"

#include "Zenith_Tools_GlbImport.h"

// ============================================================================
// Publication and declaration tests, plus the real-asset checks -- which SKIP
// when the art is absent, because the asset tree is gitignored and a fresh clone
// legitimately has none.
// ============================================================================

namespace
{
	std::string HumanExportTempDir(const char* szWho)
	{
		const std::filesystem::path xDir =
			std::filesystem::temp_directory_path() / (std::string("zenith_humanbind_") + szWho);
		std::error_code xEc;
		std::filesystem::remove_all(xDir, xEc);
		std::filesystem::create_directories(xDir, xEc);
		return xDir.string();
	}

	void HumanExportWriteText(const std::string& strPath, const std::string& strText)
	{
		std::ofstream xFile(strPath, std::ios::binary | std::ios::trunc);
		xFile << strText;
	}

	std::string HumanExportMalePath()
	{
		return std::string(ENGINE_ASSETS_DIR) + "Meshes/Humans/Male/Male";
	}
}

//------------------------------------------------------------------------------
// Routing and failure handling
//------------------------------------------------------------------------------

ZENITH_TEST(HumanModelExport, PublishFailureLeavesNoZmodel)
{
	// ★ THE .zmodel IS THE COMMIT MARKER, so a failed bind must leave NONE -- not
	// even a stale one from a previous good run. An existence-only check
	// downstream cannot tell a stale bundle from a fresh one, which is exactly
	// how the LampPost incident (ZM_Tests_PropBake.cpp) rendered 72 verts where
	// 6623 were expected with the suite green throughout.
	const std::string strDir = HumanExportTempDir("publish");
	const std::string strGlb = strDir + "/Broken.glb";
	HumanExportWriteText(strGlb, "this is not a glb");

	// A stale bundle from an imaginary earlier run.
	const std::string strStaleModel = strDir + "/Broken" ZENITH_MODEL_EXT;
	HumanExportWriteText(strStaleModel, "stale");

	const Zenith_Tools_HumanModelExport::ExportResult xResult =
		Zenith_Tools_HumanModelExport::ExportBoundHumanModel(strGlb);
	ZENITH_ASSERT_TRUE(xResult.m_bAttempted, "a present source must be attempted");
	ZENITH_ASSERT_TRUE(!xResult.m_bSuccess, "an unreadable .glb cannot succeed");
	ZENITH_ASSERT_TRUE(!xResult.m_strFailureStage.empty(), "and the failure must name its stage");

	std::error_code xEc;
	ZENITH_ASSERT_TRUE(!std::filesystem::exists(strStaleModel, xEc),
		"a failed bind must leave NO .zmodel behind, stale or otherwise");
	ZENITH_ASSERT_TRUE(!std::filesystem::exists(strDir + "/Broken" ZENITH_MODEL_EXT ".tmp", xEc),
		"...and no .tmp either");
	std::filesystem::remove_all(strDir, xEc);
}

ZENITH_TEST(HumanModelExport, MissingSourceLeavesBundleIntact)
{
	// ★ A MISSING SOURCE IS NOT A FAILURE, and the difference matters: the art is
	// gitignored, so a fresh clone has no .glb and must keep whatever bundle a
	// previous run left rather than have it deleted out from under it.
	const std::string strDir = HumanExportTempDir("missing");
	const std::string strGlb = strDir + "/Absent.glb";
	const std::string strModel = strDir + "/Absent" ZENITH_MODEL_EXT;
	HumanExportWriteText(strModel, "a previous run's bundle");

	const Zenith_Tools_HumanModelExport::ExportResult xResult =
		Zenith_Tools_HumanModelExport::ExportBoundHumanModel(strGlb);
	ZENITH_ASSERT_TRUE(!xResult.m_bAttempted, "no source means nothing was attempted");
	ZENITH_ASSERT_TRUE(xResult.m_strFailureStage.empty(), "...and nothing failed");

	std::error_code xEc;
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strModel, xEc),
		"an absent source must delete NOTHING");
	std::filesystem::remove_all(strDir, xEc);
}

ZENITH_TEST(HumanModelExport, HumanoidRoutingIsByDirectory)
{
	// ★★ THE RULE THAT CLOSES THE DOUBLE-IMPORT HAZARD. ExportAllMeshes runs the
	// generic .glb walk at Zenith_Engine.cpp:558 and the human binder runs at :571,
	// so without a skip the generic path writes a STATIC bundle first and the
	// binder overwrites it in the same boot -- and if the binder then FAILS, an
	// existence-only check downstream picks up the static bundle and animates
	// nothing. That is the LampPost incident (ZM_Tests_PropBake.cpp): 6623 verts
	// became 72 with the suite green throughout.
	//
	// ★ TESTED AS A PREDICATE, not by watching a directory walk produce no files.
	// The walk writes nothing for a humanoid AND nothing for an unparseable .glb,
	// so a file-existence test passes whether or not the skip is wired at all --
	// which is a test that cannot fail for the reason it exists.
	using Zenith_Tools_HumanModelExport::IsHumanoidSourcePath;
	ZENITH_ASSERT_TRUE(IsHumanoidSourcePath("C:/x/Zenith/Assets/Meshes/Humans/Male/Male.glb"),
		"a .glb under Meshes/Humans belongs to the human binder");
	ZENITH_ASSERT_TRUE(IsHumanoidSourcePath("C:\\x\\Zenith\\Assets\\Meshes\\Humans\\Female\\Female.glb"),
		"...including one the walk hands us with native separators");
	ZENITH_ASSERT_TRUE(IsHumanoidSourcePath("/y/Assets/Meshes/Humans/AnyoneElse/X.glb"),
		"...and one for a humanoid that does not exist yet: the rule is the directory, not a list");
	ZENITH_ASSERT_TRUE(!IsHumanoidSourcePath("C:/x/Zenith/Assets/Meshes/Props/LampPost.glb"),
		"and a prop does not");
	ZENITH_ASSERT_TRUE(!IsHumanoidSourcePath("C:/x/Zenith/Assets/Meshes/HumansOfTheWorld.glb"),
		"nor does a file whose name merely starts the same way");
}

//------------------------------------------------------------------------------
// The real asset
//------------------------------------------------------------------------------

ZENITH_TEST(HumanModelExport, MaleBundleIsCompleteAndSkinned)
{
	const std::string strBase = HumanExportMalePath();
	std::error_code xEc;
	if (!std::filesystem::exists(strBase + ZENITH_MODEL_EXT, xEc))
	{
		ZENITH_SKIP("no Male bundle on disk (the art is gitignored)");
	}

	Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::GetView<Zenith_ModelAsset>(strBase + ZENITH_MODEL_EXT);
	ZENITH_ASSERT_NOT_NULL(pxModel, "the bound male must load as a model");
	ZENITH_ASSERT_TRUE(pxModel->HasSkeleton(), "and carry a skeleton reference");
	// ★ NOT the shared rig -- its OWN, T-posed, so the mesh never had to be
	// re-posed into the arms-down bind (baking that 90-degree shoulder rotation
	// through skin weights distorted 13% of the shoulder's edges). What matters is
	// that it carries the SAME BONES, because that is what makes the 17 shared
	// clips play on it: a clip binds by name and its rotation REPLACES the bind.
	Zenith_SkeletonAsset* pxMaleSkel =
		Zenith_AssetRegistry::GetView<Zenith_SkeletonAsset>(pxModel->GetSkeletonPath());
	ZENITH_ASSERT_NOT_NULL(pxMaleSkel, "the model's own skeleton must load");
	const std::string strStickSkel =
		std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
	Zenith_SkeletonAsset* pxStickSkel =
		Zenith_AssetRegistry::GetView<Zenith_SkeletonAsset>(strStickSkel);
	ZENITH_ASSERT_NOT_NULL(pxStickSkel, "the shared rig must load");
	ZENITH_ASSERT_EQ(pxMaleSkel->GetNumBones(), pxStickSkel->GetNumBones(), "same bone count as the shared rig");
	for (u_int b = 0u; b < pxStickSkel->GetNumBones(); ++b)
	{
		ZENITH_ASSERT_EQ(pxMaleSkel->GetBone(b).m_strName, pxStickSkel->GetBone(b).m_strName,
			"same bone name at the same index -- the clips bind by name");
		ZENITH_ASSERT_EQ(pxMaleSkel->GetBone(b).m_iParentIndex, pxStickSkel->GetBone(b).m_iParentIndex,
			"same hierarchy");
	}
	ZENITH_ASSERT_TRUE(pxModel->GetNumMeshes() > 0u, "and at least one mesh binding");

	Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(
		pxModel->GetMeshBinding(0u).GetMeshPath());
	ZENITH_ASSERT_NOT_NULL(pxMesh, "the mesh must load");
	// ★ HasSkinning, NOT "the file exists". A STATIC bundle -- which the generic
	// importer would write if the humanoid routing ever broke -- loads perfectly and
	// animates nothing.
	ZENITH_ASSERT_TRUE(pxMesh->HasSkinning(), "the male must be SKINNED, not merely present");

	for (u_int u = 0u; u < 4u; ++u)
	{
		const char* aszSuffix[4] = { "_albedo", "_normal", "_rm", "_ao" };
		ZENITH_ASSERT_TRUE(std::filesystem::exists(strBase + aszSuffix[u] + ZENITH_TEXTURE_EXT, xEc),
			"the bundle must be COMPLETE: a missing map reads to a generator as 'not baked yet'");
	}
}

ZENITH_TEST(HumanModelExport, MaleAndNpcRenderAtEqualHeight)
{
	// ★ THE CHECK THAT WOULD HAVE CAUGHT A 1.98 m MALE BESIDE 1.8 m NPCs. Both
	// meshes are placed by the same rig and scaled by the same visual scale, so
	// if their BOUNDS disagree they render at different heights -- and nothing
	// else in either game measures that.
	const std::string strMale = HumanExportMalePath() + ZENITH_MODEL_EXT;
	const std::string strStick = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_ASSET_EXT;
	std::error_code xEc;
	if (!std::filesystem::exists(strMale, xEc) || !std::filesystem::exists(strStick, xEc))
	{
		ZENITH_SKIP("no Male bundle on disk (the art is gitignored)");
	}

	Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::GetView<Zenith_ModelAsset>(strMale);
	ZENITH_ASSERT_NOT_NULL(pxModel, "male model");
	Zenith_MeshAsset* pxMale = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(
		pxModel->GetMeshBinding(0u).GetMeshPath());
	Zenith_MeshAsset* pxStick = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(strStick);
	ZENITH_ASSERT_NOT_NULL(pxMale, "male mesh");
	ZENITH_ASSERT_NOT_NULL(pxStick, "stickfigure mesh");

	ZENITH_ASSERT_EQ_FLOAT(pxMale->GetBoundsMin().y, pxStick->GetBoundsMin().y, 0.01f,
		"both humans must stand on the same sole plane");
	ZENITH_ASSERT_EQ_FLOAT(pxMale->GetBoundsMax().y - pxMale->GetBoundsMin().y,
		pxStick->GetBoundsMax().y - pxStick->GetBoundsMin().y, 0.02f,
		"...and be the same height");
}

ZENITH_TEST(HumanModelExport, MaleFootBoneDepthIsPinned)
{
	// ★ WHEN THIS REDS, THE CONSTANT TO MOVE IS RenderTest's k_fAnkleHeight
	// (Games/RenderTest/Components/RenderTest_PlayerComponent.h) -- which reads
	// Zenith_HumanProportionsRealistic().AnkleHeightAboveSole(), so in practice
	// the table is what moved. A stale value plants the foot below the ground and
	// the IK folds the leg every frame to reach it, which reads as "the legs look
	// bent" and not as a wrong number.
	const std::string strSkel = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
	std::error_code xEc;
	if (!std::filesystem::exists(strSkel, xEc))
	{
		ZENITH_SKIP("no StickFigure rig on disk");
	}
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetRegistry::GetView<Zenith_SkeletonAsset>(strSkel);
	ZENITH_ASSERT_NOT_NULL(pxSkel, "the shipped rig must load");

	const int32_t iFoot = pxSkel->GetBoneIndex("LeftFoot");
	ZENITH_ASSERT_TRUE(iFoot >= 0, "the rig must have a LeftFoot");
	// The BIND pose, never a live Flux_SkeletonInstance model transform -- that is
	// whatever pose the animator last wrote.
	const float fFootBindY =
		Zenith_Maths::Vector3(pxSkel->GetBone(static_cast<u_int>(iFoot)).m_xBindPoseModel[3]).y;

	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	ZENITH_ASSERT_EQ_FLOAT(fFootBindY, xP.AnkleY(), 1.0e-3f,
		"the shipped rig's foot bone must sit on the proportions table's ankle plane");
	ZENITH_ASSERT_EQ_FLOAT(fFootBindY - xP.SoleY(), xP.AnkleHeightAboveSole(), 1.0e-4f,
		"and k_fAnkleHeight is exactly that height above the sole");
}

ZENITH_TEST(HumanModelExport, MaleLandmarksMatchProportionTable)
{
	// ★ THE TABLE IS SEEDED FROM THIS MESH, so a re-export that moves a landmark
	// must RED rather than silently re-rig every human in two games around a body
	// that is no longer the one the numbers came from. The tolerance is 1% of
	// height (2.6 cm at rig scale) -- tight enough to catch a different body,
	// loose enough to survive a re-bake of the same one.
	const std::string strGlb = HumanExportMalePath() + ".glb";
	std::error_code xEc;
	if (!std::filesystem::exists(strGlb, xEc))
	{
		ZENITH_SKIP("no Male.glb on disk (the art is gitignored)");
	}

	Zenith_MeshAsset xMesh;
	Zenith_Tools_GlbImport::GlbImportResult xLoad;
	ZENITH_ASSERT_TRUE(Zenith_Tools_GlbImport::LoadGlbMesh(strGlb, xMesh, xLoad), "the source must load");

	float fYawApplied = 0.0f;
	ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::DetectAndNormaliseIntoRigSpace(xMesh, fYawApplied), "normalise");

	Zenith_SkinDeformView xView = Zenith_MakeSkinDeformView(xMesh);
	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_T_POSE, xL), "measure");

	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	const float fH = xL.Height();
	const float fTol = 0.01f * fH;

	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE], xP.AnkleY(), fTol, "ankle");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER], xP.ShoulderY(), fTol, "shoulder");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_NECK], xP.NeckY(), fTol, "neck");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_fShoulderHalfX, xP.ShoulderHalfX(), fTol, "shoulder half-width");

	const float fShoulder = xL.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER];
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afArmChain[ZENITH_HUMAN_ARM_WRIST] - fShoulder,
		xP.m_fShoulderToWristFrac * fH, fTol, "shoulder to wrist");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP] - fShoulder,
		xP.m_fShoulderToFingertipFrac * fH, fTol, "shoulder to fingertip");

	// ★ AND THE TABLE MUST AGREE WITH THE MEASUREMENT FOR A REASON, not by
	// coincidence: the fitted T-pose rig is built from the MEASUREMENT and the
	// shipped rig from the TABLE, so a gap between them turns the rebind from a
	// pure rotation into a rotation PLUS a rescale of the artist's own arm.
	ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::CheckArmChain(xL).m_bValid,
		"the measured arm must still pass its sanity bounds");
}

//------------------------------------------------------------------------------
// Winding
//------------------------------------------------------------------------------

ZENITH_TEST(HumanModelExport, ImportedMeshWindsTheEnginesWay)
{
	// ★★ THE ONE THAT WOULD HAVE CAUGHT A BACKWARDS CHARACTER. Zenith's outward
	// normal is cross(C-A, B-A); glTF, like OpenGL, uses the other handedness. An
	// import that copies indices verbatim is INSIDE-OUT -- and the symptom is not
	// a shading error, because the normals come from the file and stay correct.
	// Backface culling keeps the FAR surface, so a character reads as FACING
	// BACKWARDS and anything strapped to his back draws on his chest.
	//
	// It is invisible to every other check: the mesh is complete, skinned,
	// normalised, correctly proportioned, its bounds are right, and its FEET
	// measure forward -- because winding does not move a single vertex. Only the
	// sign of the enclosed volume sees it.
	const std::string strGlb = HumanExportMalePath() + ".glb";
	std::error_code xEc;
	if (!std::filesystem::exists(strGlb, xEc))
	{
		ZENITH_SKIP("no Male.glb on disk (the art is gitignored)");
	}

	Zenith_MeshAsset xMesh;
	Zenith_Tools_GlbImport::GlbImportResult xLoad;
	ZENITH_ASSERT_TRUE(Zenith_Tools_GlbImport::LoadGlbMesh(strGlb, xMesh, xLoad), "the source must load");

	auto SignedVolume = [](const Zenith_MeshAsset& xM)
	{
		double dVol = 0.0;
		for (u_int i = 0u; i + 2u < xM.m_xIndices.GetSize(); i += 3u)
		{
			const Zenith_Maths::Vector3& a = xM.m_xPositions.Get(xM.m_xIndices.Get(i));
			const Zenith_Maths::Vector3& b = xM.m_xPositions.Get(xM.m_xIndices.Get(i + 1u));
			const Zenith_Maths::Vector3& c = xM.m_xPositions.Get(xM.m_xIndices.Get(i + 2u));
			dVol += glm::dot(a, glm::cross(b, c));
		}
		return dVol;
	};

	// The reference is the shipped StickFigure, which is known-good and whose sum
	// is NEGATIVE under this formula. Anchoring to it rather than to a remembered
	// sign is what keeps the test honest if the convention is ever restated.
	const std::string strStick =
		std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_ASSET_EXT;
	if (!std::filesystem::exists(strStick, xEc))
	{
		ZENITH_SKIP("no StickFigure mesh on disk");
	}
	Zenith_MeshAsset* pxStick = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(strStick);
	ZENITH_ASSERT_NOT_NULL(pxStick, "the reference human must load");

	const double dStick = SignedVolume(*pxStick);
	const double dLoaded = SignedVolume(xMesh);
	ZENITH_ASSERT_TRUE(std::fabs(dStick) > 1.0e-6 && std::fabs(dLoaded) > 1.0e-6,
		"both meshes must enclose a volume for the comparison to mean anything");
	ZENITH_ASSERT_TRUE((dStick < 0.0) == (dLoaded < 0.0),
		"a loaded .glb must wind the same way as the engine's own meshes -- otherwise it renders "
		"inside-out, which on a character reads as facing backwards");
}
